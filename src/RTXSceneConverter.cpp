//
// Created by primoz on 25. 08. 19.
//
#include "RTXSceneConverter.hpp"
#include <glm/gtx/string_cast.hpp>
#include <utility>

struct RTXGeometryInstance {
  glm::mat4x3 transform;
  uint32_t instanceId : 24;
  uint32_t mask : 8;
  uint32_t instanceOffset : 24;
  uint32_t flags : 8;
  uint64_t accelerationStructureHandle;
};

RTXMaterial::RTXMaterial(const glm::vec4& baseColorFactor, const glm::vec3& emissionFactor, float metallicFactor,
                         float roughnessFactor, float transmissionFactor, float ior, uint32_t verticesOffset)
  : baseColorFactor(baseColorFactor), emissionFactor(emissionFactor), metallicFactor(metallicFactor),
    roughnessFactor(roughnessFactor), transmissionFactor(transmissionFactor), ior(ior), verticesOffset(verticesOffset) {
}

RTXVertex::RTXVertex(const glm::vec3& normal) : normal(normal) {}

RTXSceneConverter::RTXSceneConverter(logi::MemoryAllocator allocator, logi::CommandPool commandPool,
                                     logi::Queue transferQueue)
  : allocator_(std::move(allocator)), commandPool_(std::move(commandPool)), transferQueue_(std::move(transferQueue)) {}

void RTXSceneConverter::loadScene(const lsg::Ref<lsg::Scene>& scene) {
  reset();

  scene->traverseDownExcl([&](const lsg::Ref<lsg::Object>& obj) {
    // Check if the object has mesh.
    lsg::Ref<lsg::Mesh> mesh = obj->getComponent<lsg::Mesh>();

    if (!mesh) {
      return true;
    }

    // Fetch transform matrix.
    lsg::Ref<lsg::Transform> transform = obj->getComponent<lsg::Transform>();
    glm::mat4 wm = transform->worldMatrix();
    glm::mat4x3 transformMatrix =
      (transform) ? glm::mat4x3(wm[0][0], wm[1][0], wm[2][0], wm[3][0], wm[0][1], wm[1][1], wm[2][1], wm[3][1],
                                wm[0][2], wm[1][2], wm[2][2], wm[3][2])
                  : glm::mat4x3(1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f);

    for (const auto& subMesh : mesh->subMeshes()) {
      loadMesh(subMesh, transformMatrix);
    }

    return true;
  });

  // Create top level acceleration structure.
  std::vector<RTXGeometryInstance> instances(rtMeshes_.size());
  for (uint64_t i = 0; i < rtMeshes_.size(); i++) {
    RTXGeometryInstance& instance = instances[i];
    instance.transform = rtMeshes_[i].transform;
    instance.instanceId = static_cast<uint32_t>(i);
    instance.mask = 0xff;
    instance.instanceOffset = 0;
    instance.flags = VK_GEOMETRY_INSTANCE_TRIANGLE_CULL_DISABLE_BIT_NV;
    rtMeshes_[i].blas.getHandleNV<uint64_t>(instance.accelerationStructureHandle);
  }

  // Allocate staging buffer.
  VmaAllocationCreateInfo instancesBufferAllocationInfo = {};
  instancesBufferAllocationInfo.usage = VmaMemoryUsage::VMA_MEMORY_USAGE_CPU_ONLY;

  vk::BufferCreateInfo instancesBufferInfo;
  instancesBufferInfo.size = instances.size() * sizeof(RTXGeometryInstance);
  instancesBufferInfo.usage = vk::BufferUsageFlagBits::eRayTracingNV;
  instancesBufferInfo.sharingMode = vk::SharingMode::eExclusive;

  logi::VMABuffer instancesBuffer = allocator_.createBuffer(instancesBufferInfo, instancesBufferAllocationInfo);
  instancesBuffer.writeToBuffer(instances.data(), instances.size() * sizeof(RTXGeometryInstance));

  tlas_ =
    createAccelerationStructure(vk::AccelerationStructureTypeNV::eTopLevel, {}, instances.size(), instancesBuffer);

  instancesBuffer.destroy();

  // Copy materials and vertices to buffer
  verticesBuffer_ =
    copyToGPU(vertices_.data(), vertices_.size() * sizeof(RTXVertex), vk::BufferUsageFlagBits::eStorageBuffer);
  materialsBuffer_ =
    copyToGPU(materials_.data(), materials_.size() * sizeof(RTXMaterial), vk::BufferUsageFlagBits::eStorageBuffer);
}

const logi::VMAAccelerationStructureNV& RTXSceneConverter::getTopLevelAccelerationStructure() {
  return tlas_;
}

void RTXSceneConverter::loadMesh(const lsg::Ref<lsg::SubMesh>& subMesh, const glm::mat4x3& worldMatrix) {
  lsg::Ref<lsg::Geometry> geometry = subMesh->geometry();
  lsg::Ref<lsg::MetallicRoughnessMaterial> material =
    lsg::dynamicRefCast<lsg::MetallicRoughnessMaterial>(subMesh->material());

  // Nothing to do if mesh has no geometry
  if (!geometry || !material || !geometry->hasVertices() || !geometry->hasNormals()) {
    std::wcerr << "Skipping mesh." << std::endl;
    return;
  }

  // Store material info.
  materials_.emplace_back(material->baseColorFactor(), material->emissiveFactor(), material->metallicFactor(),
                          material->roughnessFactor(), material->transmissionFactor(), material->ior(),
                          vertices_.size());

  // BLAS Geometry info
  vk::GeometryNV geometryAS;
  geometryAS.geometryType = vk::GeometryTypeNV::eTriangles;

  // Vertices
  RTMesh& rtMesh = rtMeshes_.emplace_back();
  rtMesh.transform = worldMatrix;
  lsg::TBufferAccessor<glm::vec3> vertices = geometry->getVertices();
  rtMesh.vertices = copyToGPU(&vertices[0], vertices.count() * vertices.elementSize(),
                              vk::BufferUsageFlagBits::eVertexBuffer | vk::BufferUsageFlagBits::eRayTracingNV);

  geometryAS.geometry.triangles.vertexData = rtMesh.vertices;
  geometryAS.geometry.triangles.vertexStride = sizeof(glm::vec3);
  geometryAS.geometry.triangles.vertexCount = vertices.count();
  geometryAS.geometry.triangles.vertexOffset = 0;
  geometryAS.geometry.triangles.vertexFormat = vk::Format::eR32G32B32Sfloat;

  // Store normals to vertex array.
  auto normalAccessor = geometry->getTriangleNormalAccessor();
  for (size_t i = 0; i < normalAccessor->count(); i++) {
    vertices_.emplace_back((*normalAccessor)[i].a());
    vertices_.emplace_back((*normalAccessor)[i].b());
    vertices_.emplace_back((*normalAccessor)[i].c());
  }

  // Indices
  if (geometry->hasIndices()) {
    lsg::BufferAccessor indices = geometry->getIndices();
    rtMesh.indices =
      copyToGPU(indices.bufferView().data() + indices.byteOffset(), indices.count() * indices.elementSize(),
                vk::BufferUsageFlagBits::eIndexBuffer | vk::BufferUsageFlagBits::eRayTracingNV);

    geometryAS.geometry.triangles.indexData = rtMesh.indices;
    geometryAS.geometry.triangles.indexOffset = 0;
    geometryAS.geometry.triangles.indexCount = indices.count();
    geometryAS.geometry.triangles.indexType =
      (indices.elementSize() == 2u) ? vk::IndexType::eUint16 : vk::IndexType::eUint32;
  } else {
    geometryAS.geometry.triangles.indexType = vk::IndexType::eNoneNV;
  }

  geometryAS.geometry.triangles.transformData = nullptr;
  geometryAS.geometry.triangles.transformOffset = 0;
  geometryAS.flags = vk::GeometryFlagBitsNV::eOpaque;
  rtMesh.blas = createAccelerationStructure(vk::AccelerationStructureTypeNV::eBottomLevel, {geometryAS});
}

logi::VMABuffer RTXSceneConverter::copyToGPU(void* data, size_t size, const vk::BufferUsageFlags& usageFlags) {
  logi::CommandBuffer cmdBuffer = commandPool_.allocateCommandBuffer(vk::CommandBufferLevel::ePrimary);
  cmdBuffer.begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

  // Allocate staging buffer.
  VmaAllocationCreateInfo stagingBufferAllocationInfo = {};
  stagingBufferAllocationInfo.usage = VmaMemoryUsage::VMA_MEMORY_USAGE_CPU_TO_GPU;

  vk::BufferCreateInfo stagingBufferInfo;
  stagingBufferInfo.size = size;
  stagingBufferInfo.usage = usageFlags | vk::BufferUsageFlagBits::eTransferSrc;
  stagingBufferInfo.sharingMode = vk::SharingMode::eExclusive;

  logi::VMABuffer stagingBuffer = allocator_.createBuffer(stagingBufferInfo, stagingBufferAllocationInfo);
  stagingBuffer.writeToBuffer(data, size);

  // Allocate dedicated GPU buffer.
  VmaAllocationCreateInfo gpuBufferAllocationInfo = {};
  gpuBufferAllocationInfo.usage = VmaMemoryUsage::VMA_MEMORY_USAGE_GPU_ONLY;

  vk::BufferCreateInfo gpuBufferInfo;
  gpuBufferInfo.size = size;
  gpuBufferInfo.usage = usageFlags | vk::BufferUsageFlagBits::eTransferDst;
  gpuBufferInfo.sharingMode = vk::SharingMode::eExclusive;

  logi::VMABuffer gpuBuffer = allocator_.createBuffer(gpuBufferInfo, gpuBufferAllocationInfo);

  // Add copy command to cmd buffer.
  vk::BufferCopy copyRegion;
  copyRegion.size = size;
  copyRegion.srcOffset = 0u;
  copyRegion.dstOffset = 0u;

  cmdBuffer.copyBuffer(stagingBuffer, gpuBuffer, copyRegion);
  cmdBuffer.end();

  vk::SubmitInfo submit_info;
  submit_info.commandBufferCount = 1;
  submit_info.pCommandBuffers = &static_cast<const vk::CommandBuffer&>(cmdBuffer);
  transferQueue_.submit({submit_info});
  transferQueue_.waitIdle();

  stagingBuffer.destroy();
  cmdBuffer.destroy();

  return gpuBuffer;
}

void RTXSceneConverter::reset() {
  materials_.clear();
  materialsBuffer_.destroy();
  vertices_.clear();
  verticesBuffer_.destroy();
  tlas_.destroy();

  for (const auto& mesh : rtMeshes_) {
    mesh.blas.destroy();
    mesh.indices.destroy();
    mesh.normals.destroy();
    mesh.vertices.destroy();
  }

  rtMeshes_.clear();
}

logi::VMAAccelerationStructureNV
  RTXSceneConverter::createAccelerationStructure(vk::AccelerationStructureTypeNV type,
                                                 const std::vector<vk::GeometryNV>& geometries, uint32_t instanceCount,
                                                 const logi::Buffer& instance_buffer) {
  vk::AccelerationStructureInfoNV accelerationStructureInfo;
  accelerationStructureInfo.type = type;
  accelerationStructureInfo.flags = vk::BuildAccelerationStructureFlagBitsNV::ePreferFastTrace;
  accelerationStructureInfo.geometryCount = geometries.size();
  accelerationStructureInfo.pGeometries = geometries.data();
  accelerationStructureInfo.instanceCount = instanceCount;

  vk::AccelerationStructureCreateInfoNV accelerationStructureCreateInfo;
  accelerationStructureCreateInfo.info = accelerationStructureInfo;
  accelerationStructureCreateInfo.compactedSize = 0;

  VmaAllocationCreateInfo accelerationStructureAllocationInfo = {};
  accelerationStructureAllocationInfo.usage = VmaMemoryUsage::VMA_MEMORY_USAGE_GPU_ONLY;

  logi::VMAAccelerationStructureNV accelerationStructure =
    allocator_.createAccelerationStructureNV(accelerationStructureCreateInfo, accelerationStructureAllocationInfo);

  // Build acceleration structure
  logi::CommandBuffer cmdBuffer = commandPool_.allocateCommandBuffer(vk::CommandBufferLevel::ePrimary);
  cmdBuffer.begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

  // Allocate scratch buffer.
  auto memoryRequirements =
    accelerationStructure.getMemoryRequirementsNV(vk::AccelerationStructureMemoryRequirementsTypeNV::eBuildScratch)
      .memoryRequirements;

  VmaAllocationCreateInfo scratchBufferAllocationInfo = {};
  scratchBufferAllocationInfo.usage = VmaMemoryUsage::VMA_MEMORY_USAGE_GPU_ONLY;
  scratchBufferAllocationInfo.memoryTypeBits = memoryRequirements.memoryTypeBits;

  vk::BufferCreateInfo scratchBufferInfo;
  scratchBufferInfo.size = memoryRequirements.size;
  scratchBufferInfo.usage = vk::BufferUsageFlagBits::eRayTracingNV;
  scratchBufferInfo.sharingMode = vk::SharingMode::eExclusive;

  logi::VMABuffer scratchBuffer = allocator_.createBuffer(scratchBufferInfo, scratchBufferAllocationInfo);

  cmdBuffer.buildAccelerationStructureNV(accelerationStructureInfo, instance_buffer, 0, false, accelerationStructure,
                                         nullptr, scratchBuffer, 0);

  vk::MemoryBarrier memoryBarrier;
  memoryBarrier.srcAccessMask =
    vk::AccessFlagBits::eAccelerationStructureReadNV | vk::AccessFlagBits::eAccelerationStructureWriteNV;
  memoryBarrier.dstAccessMask =
    vk::AccessFlagBits::eAccelerationStructureReadNV | vk::AccessFlagBits::eAccelerationStructureWriteNV;

  cmdBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eAccelerationStructureBuildNV,
                            vk::PipelineStageFlagBits::eAccelerationStructureBuildNV, {}, memoryBarrier, {}, {});

  cmdBuffer.end();

  vk::SubmitInfo submit_info;
  submit_info.commandBufferCount = 1;
  submit_info.pCommandBuffers = &static_cast<const vk::CommandBuffer&>(cmdBuffer);
  transferQueue_.submit({submit_info});
  transferQueue_.waitIdle();

  scratchBuffer.destroy();
  cmdBuffer.destroy();

  return accelerationStructure;
}

const logi::VMABuffer& RTXSceneConverter::getMaterialsBuffer() const {
  return materialsBuffer_;
}

const logi::VMABuffer& RTXSceneConverter::getVertexBuffer() const {
  return verticesBuffer_;
}
