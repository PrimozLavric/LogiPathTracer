//
// Created by primoz on 7. 08. 19.
//

#include "PTSceneConverter.hpp"
#include <utility>

GPUObjectData::GPUObjectData(const glm::mat4& worldMatrix, const glm::mat4& worldMatrixInverse,
                             const glm::vec4& baseColorFactor, const glm::vec3& emissionFactor, float metallicFactor,
                             float roughnessFactor, float transmissionFactor, float ior, uint32_t colorTexture,
                             uint32_t emissionTexture, uint32_t metallicRoughnessTexture, uint32_t transmissionTexture,
                             uint32_t bvhOffset, uint32_t verticesOffset)
  : worldMatrix(worldMatrix), worldMatrixInverse(worldMatrixInverse), baseColorFactor(baseColorFactor),
    emissionFactor(emissionFactor), metallicFactor(metallicFactor), roughnessFactor(roughnessFactor),
    transmissionFactor(transmissionFactor), colorTexture(colorTexture), emissionTexture(emissionTexture),
    metallicRoughnessTexture(metallicRoughnessTexture), transmissionTexture(transmissionTexture), ior(ior),
    bvhOffset(bvhOffset), verticesOffset(verticesOffset) {}

GPUVertex::GPUVertex(const glm::vec3& position, const glm::vec3& normal, const glm::vec2& uv)
  : position(position), normal(normal), uv(uv) {}

GPUBVHNode::GPUBVHNode(const glm::vec3& min, const glm::vec3& max, bool isLeaf, const glm::uvec2& indices)
  : min(min), max(max), isLeaf(isLeaf), indices(indices) {}

PTSceneConverter::PTSceneConverter(logi::MemoryAllocator allocator, logi::CommandPool commandPool,
                                   logi::Queue transferQueue)
  : allocator_(std::move(allocator)), commandPool_(std::move(commandPool)), transferQueue_(std::move(transferQueue)) {}

void PTSceneConverter::loadScene(const lsg::Ref<lsg::Scene>& scene) {
  reset();

  std::vector<lsg::AABB<float>> objectAABBs;
  std::vector<GPUObjectData> unorderedObjectData;

  for (const auto& rootObj : scene->children()) {
    rootObj->traverseDown([&](const lsg::Ref<lsg::Object>& object) {
      // Get world matrix.
      glm::mat4 worldMatrix(1.0f);

      if (auto transform = object->getComponent<lsg::Transform>()) {
        worldMatrix = transform->worldMatrix();
      }

      // Handle cameras.
      if (auto camera = object->getComponent<lsg::PerspectiveCamera>()) {
        cameras_.emplace_back(object);
      }

      // Handle geometry.
      if (auto mesh = object->getComponent<lsg::Mesh>()) {
        std::cout << "Building BVH for object " << object->name() << "..." << std::endl;

        for (const auto& submesh : mesh->subMeshes()) {
          lsg::Ref<lsg::MetallicRoughnessMaterial> material =
            lsg::dynamicRefCast<lsg::MetallicRoughnessMaterial>(submesh->material());

          // Skip if the object does not have MetallicRoughnessMaterial.
          if (!material) {
            std::cout << "Unknown material. Skipping submesh." << std::endl;
            continue;
          }

          // Convert object data into GPU compatible format.
          unorderedObjectData.emplace_back();
          GPUObjectData& objectData = unorderedObjectData.back();
          objectData.worldMatrix = worldMatrix;
          objectData.worldMatrixInverse = glm::inverse(objectData.worldMatrix);
          objectData.baseColorFactor = material->baseColorFactor();
          objectData.colorTexture = (material->baseColorTex()) ? copyTextureToGPU(material->baseColorTex())
                                                               : std::numeric_limits<uint32_t>::max();
          objectData.emissionFactor = material->emissiveFactor();
          objectData.emissionTexture = (material->emissiveTex()) ? copyTextureToGPU(material->emissiveTex())
                                                                 : std::numeric_limits<uint32_t>::max();
          objectData.metallicFactor = material->metallicFactor();
          objectData.roughnessFactor = material->roughnessFactor();
          objectData.metallicRoughnessTexture = (material->metallicRoughnessTex())
                                                  ? copyTextureToGPU(material->metallicRoughnessTex())
                                                  : std::numeric_limits<uint32_t>::max();
          objectData.transmissionFactor = material->transmissionFactor();
          objectData.metallicRoughnessTexture = (material->transmissionTexture())
                                                  ? copyTextureToGPU(material->transmissionTexture())
                                                  : std::numeric_limits<uint32_t>::max();
          objectData.ior = material->ior();
          objectData.bvhOffset = meshBVHNodes_.size();
          objectData.verticesOffset = vertices_.size();

          std::cout << "VTX offset: " << vertices_.size() << std::endl;

          auto positionAccessor = submesh->geometry()->getTrianglePositionAccessor();
          auto normalAccessor = submesh->geometry()->getTriangleNormalAccessor();
          auto uvAccessor = submesh->geometry()->hasUv(0u) ? submesh->geometry()->getTriangleUVAccessor(0u) : nullptr;

          // Build triangles BVH nodes.
          lsg::bvh::SplitBVHBuilder<float> builder;
          auto bvh = builder.process(positionAccessor);
          for (const auto& node : bvh->getNodes()) {
            meshBVHNodes_.emplace_back(node.bounds.min(), node.bounds.max(), node.is_leaf, node.child_indices);
          }
          std::cout << "Vtx count: " << bvh->getPrimitiveIndices().size() * 3 << std::endl;

          // Convert vertices into GPU compatible format (interleave).
          for (uint32_t idx : bvh->getPrimitiveIndices()) {
            lsg::Triangle<glm::vec3> posTri = (*positionAccessor)[idx];
            lsg::Triangle<glm::vec3> normalTri = (*normalAccessor)[idx];

            if (uvAccessor) {
              lsg::Triangle<glm::vec2> uvTri = (*uvAccessor)[idx];

              vertices_.emplace_back(posTri.a(), normalTri.a(), uvTri.a());
              vertices_.emplace_back(posTri.b(), normalTri.b(), uvTri.b());
              vertices_.emplace_back(posTri.c(), normalTri.c(), uvTri.c());
            } else {
              vertices_.emplace_back(posTri.a(), normalTri.a());
              vertices_.emplace_back(posTri.b(), normalTri.b());
              vertices_.emplace_back(posTri.c(), normalTri.c());
            }
          }

          objectAABBs.emplace_back(bvh->getBounds().transform(worldMatrix));
        }
        std::cout << " Finished." << std::endl;
      }

      return true;
    });
  }

  std::cout << "Building scene BVH..." << std::flush;

  // Build objects BVH nodes.
  lsg::bvh::BVHBuilder<float> builder;
  auto bvh = builder.process(objectAABBs);
  for (const auto& node : bvh->getNodes()) {
    objectBVHNodes_.emplace_back(node.bounds.min(), node.bounds.max(), node.is_leaf, node.child_indices);
  }

  for (uint32_t idx : bvh->getPrimitiveIndices()) {
    objectData_.emplace_back(unorderedObjectData[idx]);
  }

  std::cout << " Copying to GPU" << std::endl;

  objectDataBuffer_ =
    copyToGPU(objectData_.data(), objectData_.size() * sizeof(GPUObjectData), vk::BufferUsageFlagBits::eStorageBuffer);
  objectBVHNodesBuffer_ = copyToGPU(objectBVHNodes_.data(), objectBVHNodes_.size() * sizeof(GPUBVHNode),
                                    vk::BufferUsageFlagBits::eStorageBuffer);
  verticesBuffer_ =
    copyToGPU(vertices_.data(), vertices_.size() * sizeof(GPUVertex), vk::BufferUsageFlagBits::eStorageBuffer);
  meshBVHNodesBuffer_ =
    copyToGPU(meshBVHNodes_.data(), meshBVHNodes_.size() * sizeof(GPUBVHNode), vk::BufferUsageFlagBits::eStorageBuffer);

  std::cout << " Finished." << std::endl;
}

const std::vector<lsg::Ref<lsg::Object>>& PTSceneConverter::getCameras() const {
  return cameras_;
}

const logi::VMABuffer& PTSceneConverter::getObjectDataBuffer() const {
  return objectDataBuffer_;
}

const logi::VMABuffer& PTSceneConverter::getObjectBvhNodesBuffer() const {
  return objectBVHNodesBuffer_;
}

const logi::VMABuffer& PTSceneConverter::getVerticesBuffer() const {
  return verticesBuffer_;
}

const logi::VMABuffer& PTSceneConverter::getMeshBvhNodesBuffer() const {
  return meshBVHNodesBuffer_;
}

const std::vector<GPUTexture>& PTSceneConverter::getTextures() const {
  return textures_;
}

logi::VMABuffer PTSceneConverter::copyToGPU(void* data, size_t size, const vk::BufferUsageFlags& usageFlags) {
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

void PTSceneConverter::reset() {
  cameras_.clear();
  objectData_.clear();
  objectBVHNodes_.clear();
  vertices_.clear();
  meshBVHNodes_.clear();

  objectDataBuffer_.destroy();
  objectBVHNodesBuffer_.destroy();
  verticesBuffer_.destroy();
  meshBVHNodesBuffer_.destroy();
}

uint32_t PTSceneConverter::copyTextureToGPU(const lsg::Ref<lsg::Texture>& texture) {
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
