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

RTXVertex::RTXVertex(const glm::vec3& normal, const glm::vec2& uv) : normal(normal), uv(uv) {}

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
  auto& gpuMaterial = materials_.emplace_back(material->baseColorFactor(), material->emissiveFactor(),
                                              material->metallicFactor(), material->roughnessFactor(),
                                              material->transmissionFactor(), material->ior(), vertices_.size());

  gpuMaterial.colorTexture =
    (material->baseColorTex()) ? copyTextureToGPU(material->baseColorTex()) : std::numeric_limits<uint32_t>::max();
  gpuMaterial.emissionTexture =
    (material->emissiveTex()) ? copyTextureToGPU(material->emissiveTex()) : std::numeric_limits<uint32_t>::max();
  gpuMaterial.metallicRoughnessTexture = (material->metallicRoughnessTex())
                                           ? copyTextureToGPU(material->metallicRoughnessTex())
                                           : std::numeric_limits<uint32_t>::max();
  gpuMaterial.metallicRoughnessTexture = (material->transmissionTexture())
                                           ? copyTextureToGPU(material->transmissionTexture())
                                           : std::numeric_limits<uint32_t>::max();

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
  auto uvAccessor = geometry->hasUv(0u) ? geometry->getTriangleUVAccessor(0u) : nullptr;

  if (uvAccessor) {
    for (size_t i = 0; i < normalAccessor->count(); i++) {
      vertices_.emplace_back((*normalAccessor)[i].a(), (*uvAccessor)[i].a());
      vertices_.emplace_back((*normalAccessor)[i].b(), (*uvAccessor)[i].b());
      vertices_.emplace_back((*normalAccessor)[i].c(), (*uvAccessor)[i].c());
    }
  } else {
    for (size_t i = 0; i < normalAccessor->count(); i++) {
      vertices_.emplace_back((*normalAccessor)[i].a());
      vertices_.emplace_back((*normalAccessor)[i].b());
      vertices_.emplace_back((*normalAccessor)[i].c());
    }
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

uint32_t RTXSceneConverter::copyTextureToGPU(const lsg::Ref<lsg::Texture>& texture) {
  lsg::Ref<lsg::Image> image = texture->image();

  GPUTexture& gpuTexture = textures_.emplace_back();

  uint64_t imageByteSize = image->pixelSize() * image->height() * image->width();

  logi::CommandBuffer cmdBuffer = commandPool_.allocateCommandBuffer(vk::CommandBufferLevel::ePrimary);
  cmdBuffer.begin({vk::CommandBufferUsageFlagBits::eOneTimeSubmit});

  // Allocate staging buffer.
  VmaAllocationCreateInfo stagingBufferAllocationInfo = {};
  stagingBufferAllocationInfo.usage = VmaMemoryUsage::VMA_MEMORY_USAGE_CPU_TO_GPU;

  vk::BufferCreateInfo stagingBufferInfo;
  stagingBufferInfo.size = imageByteSize;
  stagingBufferInfo.usage = vk::BufferUsageFlagBits::eTransferSrc;
  stagingBufferInfo.sharingMode = vk::SharingMode::eExclusive;

  logi::VMABuffer stagingBuffer = allocator_.createBuffer(stagingBufferInfo, stagingBufferAllocationInfo);
  stagingBuffer.writeToBuffer(image->rawPixelData(), imageByteSize);

  // Allocate dedicated GPU image.
  VmaAllocationCreateInfo gpuImageAllocationInfo = {};
  gpuImageAllocationInfo.usage = VmaMemoryUsage::VMA_MEMORY_USAGE_GPU_ONLY;

  vk::ImageCreateInfo imageInfo;
  imageInfo.imageType = vk::ImageType::e2D;
  imageInfo.extent.width = image->width();
  imageInfo.extent.height = image->height();
  imageInfo.extent.depth = 1;
  imageInfo.mipLevels = 1;
  imageInfo.arrayLayers = 1;
  imageInfo.format = image->getFormat();
  imageInfo.tiling = vk::ImageTiling::eOptimal;
  imageInfo.initialLayout = vk::ImageLayout::eUndefined;
  imageInfo.usage = vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst;
  imageInfo.samples = vk::SampleCountFlagBits::e1;
  imageInfo.sharingMode = vk::SharingMode::eExclusive;

  gpuTexture.image = allocator_.createImage(imageInfo, gpuImageAllocationInfo);

  // Transition to DST optimal
  vk::ImageMemoryBarrier barrierDstOptimal;
  barrierDstOptimal.oldLayout = vk::ImageLayout::eUndefined;
  barrierDstOptimal.newLayout = vk::ImageLayout::eTransferDstOptimal;
  barrierDstOptimal.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrierDstOptimal.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrierDstOptimal.image = gpuTexture.image;
  barrierDstOptimal.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
  barrierDstOptimal.subresourceRange.baseMipLevel = 0;
  barrierDstOptimal.subresourceRange.levelCount = 1;
  barrierDstOptimal.subresourceRange.baseArrayLayer = 0;
  barrierDstOptimal.subresourceRange.layerCount = 1;
  barrierDstOptimal.srcAccessMask = {};
  barrierDstOptimal.dstAccessMask = vk::AccessFlagBits::eTransferWrite;

  cmdBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTopOfPipe, vk::PipelineStageFlagBits::eTransfer, {}, {}, {},
                            barrierDstOptimal);

  // Copy data to texture
  vk::BufferImageCopy region = {};
  region.bufferOffset = 0;
  region.bufferRowLength = 0;
  region.bufferImageHeight = 0;
  region.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
  region.imageSubresource.mipLevel = 0;
  region.imageSubresource.baseArrayLayer = 0;
  region.imageSubresource.layerCount = 1;
  region.imageOffset = vk::Offset3D();
  region.imageExtent = vk::Extent3D{static_cast<uint32_t>(image->width()), static_cast<uint32_t>(image->height()), 1};

  cmdBuffer.copyBufferToImage(stagingBuffer, gpuTexture.image, vk::ImageLayout::eTransferDstOptimal, region);

  // Transition to DST optimal
  vk::ImageMemoryBarrier barrierShaderReadOptimal;
  barrierShaderReadOptimal.oldLayout = vk::ImageLayout::eTransferDstOptimal;
  barrierShaderReadOptimal.newLayout = vk::ImageLayout::eShaderReadOnlyOptimal;
  barrierShaderReadOptimal.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrierShaderReadOptimal.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrierShaderReadOptimal.image = gpuTexture.image;
  barrierShaderReadOptimal.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
  barrierShaderReadOptimal.subresourceRange.baseMipLevel = 0;
  barrierShaderReadOptimal.subresourceRange.levelCount = 1;
  barrierShaderReadOptimal.subresourceRange.baseArrayLayer = 0;
  barrierShaderReadOptimal.subresourceRange.layerCount = 1;
  barrierShaderReadOptimal.srcAccessMask = vk::AccessFlagBits::eTransferWrite;
  barrierShaderReadOptimal.dstAccessMask = vk::AccessFlagBits::eShaderRead;

  cmdBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eTransfer, vk::PipelineStageFlagBits::eComputeShader, {}, {}, {},
                            barrierShaderReadOptimal);

  cmdBuffer.end();

  vk::SubmitInfo submit_info;
  submit_info.commandBufferCount = 1;
  submit_info.pCommandBuffers = &static_cast<const vk::CommandBuffer&>(cmdBuffer);
  transferQueue_.submit({submit_info});
  transferQueue_.waitIdle();

  stagingBuffer.destroy();
  cmdBuffer.destroy();

  // Create image view.
  gpuTexture.imageView = gpuTexture.image.createImageView(
    vk::ImageViewCreateFlags(), vk::ImageViewType::e2D, image->getFormat(), vk::ComponentMapping(),
    vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1));

  lsg::Ref<lsg::Sampler> sampler = texture->sampler();

  if (sampler) {
    // Create sampler.
    vk::SamplerCreateInfo samplerInfo;
    samplerInfo.magFilter = sampler->magFilter();
    samplerInfo.minFilter = sampler->minFilter();
    samplerInfo.addressModeU = sampler->wrappingU();
    samplerInfo.addressModeV = sampler->wrappingW();
    samplerInfo.addressModeW = sampler->wrappingW();
    samplerInfo.anisotropyEnable = sampler->enableAnisotropy();
    samplerInfo.maxAnisotropy = sampler->maxAnisotropy();
    samplerInfo.borderColor = vk::BorderColor::eFloatOpaqueBlack;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = sampler->enableCompare();
    samplerInfo.compareOp = sampler->compareOp();
    samplerInfo.mipmapMode = sampler->mipmapMode();

    gpuTexture.sampler = gpuTexture.image.getLogicalDevice().createSampler(samplerInfo);
  } else {
    vk::SamplerCreateInfo samplerInfo;
    samplerInfo.magFilter = vk::Filter::eLinear;
    samplerInfo.minFilter = vk::Filter::eLinear;
    samplerInfo.addressModeU = vk::SamplerAddressMode::eRepeat;
    samplerInfo.addressModeV = vk::SamplerAddressMode::eRepeat;
    samplerInfo.addressModeW = vk::SamplerAddressMode::eRepeat;
    samplerInfo.anisotropyEnable = true;
    samplerInfo.maxAnisotropy = 16;
    samplerInfo.borderColor = vk::BorderColor::eFloatOpaqueBlack;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.compareEnable = sampler->enableCompare();
    samplerInfo.compareOp = sampler->compareOp();
    samplerInfo.mipmapMode = sampler->mipmapMode();

    gpuTexture.sampler = gpuTexture.image.getLogicalDevice().createSampler(samplerInfo);
  }

  return textures_.size() - 1u;
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

const std::vector<GPUTexture>& RTXSceneConverter::getTextures() const {
  return textures_;
}
