//
// Created by primoz on 6. 08. 19.
//

#include "RendererPT.h"
#include <RendererPT.h>
#include <chrono>
#include <cmath>

RendererPT::RendererPT(const cppglfw::Window& window, const RendererConfiguration& configuration)
  : RendererCore(window, configuration) {
  allocator_ = logicalDevice_.createMemoryAllocator();

  createTexViewerRenderPass();
  createFrameBuffers();

  texViewerPipelineLayoutData_ =
    loadPipelineShaders({{"shaders/tex_to_quad.vert.spv", "main"}, {"shaders/tex_to_quad.frag.spv", "main"}});

  pathTracingPipelineLayoutData_ = loadPipelineShaders({{"shaders/path_tracing.comp.spv", "main"}});

  createTexViewerPipeline();
  createPathTracingPipeline();
  initializeAccumulationTexture();
  initializeDescriptorSets();
  updateAccumulationTexDescriptorSet();
  initializeUBOBuffer();
  // recordCommandBuffers();
}

void RendererPT::loadScene(const lsg::Ref<lsg::Scene>& scene) {
  sceneLoaded_ = false;

  sceneConverter_.loadScene(scene);
  const std::vector<lsg::Ref<lsg::Object>>& cameras = sceneConverter_.getCameras();
  if (cameras.empty()) {
    std::cout << "Loaded scene without cameras." << std::endl;
    sceneConverter_.clear();
  }

  selectedCameraTransform_ = sceneConverter_.getCameras()[0]->getComponent<lsg::Transform>();

  auto cameraData = cameras[0]->getComponent<lsg::PerspectiveCamera>();
  ubo_.camera.fovY = cameraData->fov();
  ubo_.camera.worldMatrix = selectedCameraTransform_->worldMatrix();
  ubo_.sampleCount = 0;

  initializeAndBindSceneBuffer();
  sceneLoaded_ = true;
}

void RendererPT::createTexViewerRenderPass() {
  vk::AttachmentDescription colorAttachment;

  // Swapchain color attachment.
  colorAttachment.format = swapchainImageFormat_;
  colorAttachment.samples = vk::SampleCountFlagBits::e1;
  colorAttachment.loadOp = vk::AttachmentLoadOp::eClear;
  colorAttachment.storeOp = vk::AttachmentStoreOp::eStore;
  colorAttachment.stencilLoadOp = vk::AttachmentLoadOp::eDontCare;
  colorAttachment.stencilStoreOp = vk::AttachmentStoreOp::eStore;
  colorAttachment.initialLayout = vk::ImageLayout::eUndefined;
  colorAttachment.finalLayout = vk::ImageLayout::ePresentSrcKHR;

  vk::AttachmentReference colorAttachmentRef;

  colorAttachmentRef.attachment = 0;
  colorAttachmentRef.layout = vk::ImageLayout::eColorAttachmentOptimal;

  vk::SubpassDescription subpass;
  subpass.pipelineBindPoint = vk::PipelineBindPoint::eGraphics;
  subpass.colorAttachmentCount = 1;
  subpass.pColorAttachments = &colorAttachmentRef;

  vk::SubpassDependency dependency;
  dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
  dependency.dstSubpass = 0;
  dependency.srcStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
  dependency.srcAccessMask = {};
  dependency.dstStageMask = vk::PipelineStageFlagBits::eColorAttachmentOutput;
  dependency.dstAccessMask = vk::AccessFlagBits::eColorAttachmentRead | vk::AccessFlagBits::eColorAttachmentWrite;

  vk::RenderPassCreateInfo renderPassCreateInfo;
  renderPassCreateInfo.attachmentCount = 1;
  renderPassCreateInfo.pAttachments = &colorAttachment;
  renderPassCreateInfo.subpassCount = 1;
  renderPassCreateInfo.pSubpasses = &subpass;
  renderPassCreateInfo.dependencyCount = 1;
  renderPassCreateInfo.pDependencies = &dependency;

  texViewerRenderPass_ = logicalDevice_.createRenderPass(renderPassCreateInfo);
}

void RendererPT::createFrameBuffers() {
  // Destroy previous framebuffers.
  for (const auto& framebuffer : framebuffers_) {
    framebuffer.destroy();
  }
  framebuffers_.clear();

  // Create new framebuffers.
  for (const auto& imageView : swapchainImageViews_) {
    vk::FramebufferCreateInfo createInfo;
    createInfo.renderPass = texViewerRenderPass_;
    createInfo.attachmentCount = 1;
    createInfo.pAttachments = &static_cast<const vk::ImageView&>(imageView);
    createInfo.width = swapchainImageExtent_.width;
    createInfo.height = swapchainImageExtent_.height;
    createInfo.layers = 1;

    framebuffers_.emplace_back(logicalDevice_.createFramebuffer(createInfo));
  }
}

void RendererPT::createTexViewerPipeline() {
  // Destroy existing pipeline.
  if (texViewerPipeline_) {
    texViewerPipeline_.destroy();
  }

  // Pipeline
  vk::PipelineShaderStageCreateInfo vertShaderStageInfo;
  vertShaderStageInfo.stage = vk::ShaderStageFlagBits::eVertex;
  vertShaderStageInfo.module = texViewerPipelineLayoutData_.shaders.at(vk::ShaderStageFlagBits::eVertex);
  vertShaderStageInfo.pName = "main";

  vk::PipelineShaderStageCreateInfo fragShaderStageInfo;
  fragShaderStageInfo.stage = vk::ShaderStageFlagBits::eFragment;
  fragShaderStageInfo.module = texViewerPipelineLayoutData_.shaders.at(vk::ShaderStageFlagBits::eFragment);
  ;
  fragShaderStageInfo.pName = "main";

  vk::PipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

  vk::PipelineVertexInputStateCreateInfo vertexInputInfo;
  vertexInputInfo.pVertexBindingDescriptions = nullptr;
  vertexInputInfo.vertexBindingDescriptionCount = 0u;
  vertexInputInfo.pVertexAttributeDescriptions = nullptr;
  vertexInputInfo.vertexAttributeDescriptionCount = 0u;

  vk::PipelineInputAssemblyStateCreateInfo inputAssembly;
  inputAssembly.topology = vk::PrimitiveTopology::eTriangleList;
  inputAssembly.primitiveRestartEnable = VK_FALSE;

  vk::Viewport viewport = {};
  viewport.x = 0.0f;
  viewport.y = 0.0f;
  viewport.width = (float) swapchainImageExtent_.width;
  viewport.height = (float) swapchainImageExtent_.height;
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;

  vk::Rect2D scissor;
  scissor.extent = swapchainImageExtent_;

  vk::PipelineViewportStateCreateInfo viewportState;
  viewportState.viewportCount = 1;
  viewportState.pViewports = &viewport;
  viewportState.scissorCount = 1;
  viewportState.pScissors = &scissor;

  vk::PipelineRasterizationStateCreateInfo rasterizer;
  rasterizer.depthClampEnable = VK_FALSE;
  rasterizer.rasterizerDiscardEnable = VK_FALSE;
  rasterizer.polygonMode = vk::PolygonMode::eFill;
  rasterizer.lineWidth = 1.0f;
  rasterizer.cullMode = vk::CullModeFlagBits::eNone;
  rasterizer.frontFace = vk::FrontFace::eCounterClockwise;
  rasterizer.depthBiasEnable = VK_FALSE;

  vk::PipelineMultisampleStateCreateInfo multisampling;
  multisampling.sampleShadingEnable = VK_FALSE;
  multisampling.rasterizationSamples = vk::SampleCountFlagBits::e1;
  vk::SampleMask sampleMask(0xFFFFFF);
  multisampling.pSampleMask = &sampleMask;

  vk::PipelineColorBlendAttachmentState colorBlendAttachment;
  colorBlendAttachment.colorWriteMask = vk::ColorComponentFlagBits::eR | vk::ColorComponentFlagBits::eG |
                                        vk::ColorComponentFlagBits::eB | vk::ColorComponentFlagBits::eA;
  colorBlendAttachment.blendEnable = VK_FALSE;

  vk::PipelineColorBlendStateCreateInfo colorBlending;
  colorBlending.logicOpEnable = VK_FALSE;
  colorBlending.logicOp = vk::LogicOp::eCopy;
  colorBlending.attachmentCount = 1;
  colorBlending.pAttachments = &colorBlendAttachment;
  colorBlending.blendConstants[0] = 0.0f;
  colorBlending.blendConstants[1] = 0.0f;
  colorBlending.blendConstants[2] = 0.0f;
  colorBlending.blendConstants[3] = 0.0f;

  vk::GraphicsPipelineCreateInfo pipelineInfo;
  pipelineInfo.stageCount = 2;
  pipelineInfo.pStages = shaderStages;
  pipelineInfo.pVertexInputState = &vertexInputInfo;
  pipelineInfo.pInputAssemblyState = &inputAssembly;
  pipelineInfo.pViewportState = &viewportState;
  pipelineInfo.pRasterizationState = &rasterizer;
  pipelineInfo.pMultisampleState = &multisampling;
  pipelineInfo.pColorBlendState = &colorBlending;
  pipelineInfo.layout = texViewerPipelineLayoutData_.layout;
  pipelineInfo.renderPass = texViewerRenderPass_;
  pipelineInfo.subpass = 0;

  texViewerPipeline_ = logicalDevice_.createGraphicsPipeline(pipelineInfo);
}

void RendererPT::createPathTracingPipeline() {
  if (pathTracingPipeline_) {
    pathTracingPipeline_.destroy();
  }

  vk::PipelineShaderStageCreateInfo compShaderStageInfo;
  compShaderStageInfo.stage = vk::ShaderStageFlagBits::eCompute;
  compShaderStageInfo.module = pathTracingPipelineLayoutData_.shaders.at(vk::ShaderStageFlagBits::eCompute);
  compShaderStageInfo.pName = "main";

  vk::ComputePipelineCreateInfo pipelineInfo;
  pipelineInfo.stage = compShaderStageInfo;
  pipelineInfo.layout = pathTracingPipelineLayoutData_.layout;

  pathTracingPipeline_ = logicalDevice_.createComputePipeline(pipelineInfo);
}

void RendererPT::onSwapChainRecreate() {
  createFrameBuffers();
  createTexViewerPipeline();
  initializeAccumulationTexture();
  updateAccumulationTexDescriptorSet();
  recordCommandBuffers();
  ubo_.sampleCount = 0;
}

void RendererPT::initializeAccumulationTexture() {
  // Destroy existing image. Useful for recreation.
  if (accumulationTexture_.image) {
    accumulationTexture_.image.destroy();
  }

  VmaAllocationCreateInfo allocationInfo = {};
  allocationInfo.usage = VmaMemoryUsage::VMA_MEMORY_USAGE_GPU_ONLY;
  allocationInfo.preferredFlags = VK_MEMORY_PROPERTY_LAZILY_ALLOCATED_BIT;

  vk::ImageCreateInfo imageInfo;
  imageInfo.imageType = vk::ImageType::e2D;
  imageInfo.format = vk::Format::eR32G32B32A32Sfloat;
  imageInfo.mipLevels = 1;
  imageInfo.arrayLayers = 1;
  imageInfo.samples = vk::SampleCountFlagBits::e1;
  imageInfo.tiling = vk::ImageTiling::eOptimal;
  imageInfo.sharingMode = vk::SharingMode::eExclusive;
  // Set initial layout of the image to undefined
  imageInfo.initialLayout = vk::ImageLayout::eUndefined;
  imageInfo.extent = vk::Extent3D(swapchainImageExtent_, 1);
  imageInfo.usage = vk::ImageUsageFlagBits::eStorage | vk::ImageUsageFlagBits::eSampled;

  accumulationTexture_.image = allocator_.createImage(imageInfo, allocationInfo);

  // Update image layout
  logi::CommandBuffer cmdBuffer = graphicsFamilyCmdPool_.allocateCommandBuffer(vk::CommandBufferLevel::ePrimary);
  cmdBuffer.begin(vk::CommandBufferBeginInfo(vk::CommandBufferUsageFlagBits::eOneTimeSubmit));

  vk::ImageMemoryBarrier changeLayoutBarrier;
  changeLayoutBarrier.oldLayout = vk::ImageLayout::eUndefined;
  changeLayoutBarrier.newLayout = vk::ImageLayout::eGeneral;
  changeLayoutBarrier.image = accumulationTexture_.image;
  changeLayoutBarrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
  changeLayoutBarrier.subresourceRange.baseArrayLayer = 0u;
  changeLayoutBarrier.subresourceRange.layerCount = 1u;
  changeLayoutBarrier.subresourceRange.baseMipLevel = 0u;
  changeLayoutBarrier.subresourceRange.levelCount = 1u;
  cmdBuffer.pipelineBarrier(vk::PipelineStageFlagBits::eHost, vk::PipelineStageFlagBits::eComputeShader, {}, {}, {},
                            changeLayoutBarrier);

  cmdBuffer.end();

  vk::SubmitInfo submit_info;
  submit_info.commandBufferCount = 1;
  submit_info.pCommandBuffers = &static_cast<const vk::CommandBuffer&>(cmdBuffer);
  graphicsQueue_.submit({submit_info});
  logicalDevice_.waitIdle();
  cmdBuffer.destroy();

  // Create image view.
  accumulationTexture_.imageView =
    accumulationTexture_.image.createImageView({}, vk::ImageViewType::e2D, vk::Format::eR32G32B32A32Sfloat, {},
                                               vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1));

  // Create sampler
  if (!accumulationTexture_.sampler) {
    vk::SamplerCreateInfo samplerInfo;
    samplerInfo.magFilter = vk::Filter::eLinear;
    samplerInfo.minFilter = vk::Filter::eLinear;
    samplerInfo.mipmapMode = vk::SamplerMipmapMode::eLinear;
    samplerInfo.addressModeU = vk::SamplerAddressMode::eClampToBorder;
    samplerInfo.addressModeV = vk::SamplerAddressMode::eClampToBorder;
    samplerInfo.addressModeW = vk::SamplerAddressMode::eClampToBorder;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.compareOp = vk::CompareOp::eNever;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 0.0f;
    samplerInfo.maxAnisotropy = 1.0;
    samplerInfo.anisotropyEnable = VK_FALSE;
    samplerInfo.borderColor = vk::BorderColor::eFloatOpaqueWhite;

    accumulationTexture_.sampler = logicalDevice_.createSampler(samplerInfo);
  }
}

void RendererPT::initializeDescriptorSets() {
  static const size_t numPoolSets = 10;
  static const std::vector<vk::DescriptorPoolSize> poolSizes = {
    //{vk::DescriptorType::eSampler, 0},
    {vk::DescriptorType::eCombinedImageSampler, 1},
    //{vk::DescriptorType::eSampledImage, 0},
    {vk::DescriptorType::eStorageImage, 1},
    //{vk::DescriptorType::eUniformTexelBuffer, 0},
    //{vk::DescriptorType::eStorageTexelBuffer, 0},
    {vk::DescriptorType::eUniformBuffer, 1},
    {vk::DescriptorType::eStorageBuffer, 4},
    //{vk::DescriptorType::eUniformBufferDynamic, 0},
    //{vk::DescriptorType::eStorageBufferDynamic, 0},
    //{vk::DescriptorType::eInputAttachment, 0},
    //{vk::DescriptorType::eInlineUniformBlockEXT, 0},
    //{vk::DescriptorType::eAccelerationStructureNV, 0}
  };

  vk::DescriptorPoolCreateInfo poolInfo;
  poolInfo.pPoolSizes = poolSizes.data();
  poolInfo.poolSizeCount = poolSizes.size();
  poolInfo.maxSets = numPoolSets;

  descriptorPool_ = logicalDevice_.createDescriptorPool(poolInfo);

  // Create descriptor sets.
  texViewerDescSets_ = descriptorPool_.allocateDescriptorSets(
    std::vector<vk::DescriptorSetLayout>(texViewerPipelineLayoutData_.descriptorSetLayouts.begin(),
                                         texViewerPipelineLayoutData_.descriptorSetLayouts.end()));
  pathTracingDescSets_ = descriptorPool_.allocateDescriptorSets(
    std::vector<vk::DescriptorSetLayout>(pathTracingPipelineLayoutData_.descriptorSetLayouts.begin(),
                                         pathTracingPipelineLayoutData_.descriptorSetLayouts.end()));
}

void RendererPT::updateAccumulationTexDescriptorSet() {
  std::vector<vk::WriteDescriptorSet> descriptorWrites(2);

  vk::DescriptorImageInfo texViewerTextureDescriptor;
  texViewerTextureDescriptor.imageView = accumulationTexture_.imageView;
  texViewerTextureDescriptor.sampler = accumulationTexture_.sampler;
  texViewerTextureDescriptor.imageLayout = vk::ImageLayout::eGeneral;

  descriptorWrites[0].dstSet = texViewerDescSets_[0];
  descriptorWrites[0].dstBinding = 0;
  descriptorWrites[0].dstArrayElement = 0;
  descriptorWrites[0].descriptorType = vk::DescriptorType::eCombinedImageSampler;
  descriptorWrites[0].descriptorCount = 1;
  descriptorWrites[0].pImageInfo = &texViewerTextureDescriptor;

  vk::DescriptorImageInfo pathTracerTextureDescriptor;
  pathTracerTextureDescriptor.imageView = accumulationTexture_.imageView;
  pathTracerTextureDescriptor.sampler = accumulationTexture_.sampler;
  pathTracerTextureDescriptor.imageLayout = vk::ImageLayout::eGeneral;

  descriptorWrites[1].dstSet = pathTracingDescSets_[0];
  descriptorWrites[1].dstBinding = 0;
  descriptorWrites[1].dstArrayElement = 0;
  descriptorWrites[1].descriptorType = vk::DescriptorType::eStorageImage;
  descriptorWrites[1].descriptorCount = 1;
  descriptorWrites[1].pImageInfo = &pathTracerTextureDescriptor;

  logicalDevice_.updateDescriptorSets(descriptorWrites);
}

void RendererPT::initializeUBOBuffer() {
  VmaAllocationCreateInfo allocationInfo = {};
  allocationInfo.usage = VmaMemoryUsage::VMA_MEMORY_USAGE_CPU_TO_GPU;

  // Create and init matrices UBO buffer.
  vk::BufferCreateInfo uboBufferInfo;
  uboBufferInfo.size = sizeof(ubo_);
  uboBufferInfo.usage = vk::BufferUsageFlagBits::eUniformBuffer | vk::BufferUsageFlagBits::eTransferDst;
  uboBufferInfo.sharingMode = vk::SharingMode::eExclusive;

  uboBuffer_ = allocator_.createBuffer(uboBufferInfo, allocationInfo);

  // Update UBO descriptor.
  vk::DescriptorBufferInfo bufferInfo;
  bufferInfo.buffer = uboBuffer_;
  bufferInfo.offset = 0;
  bufferInfo.range = sizeof(ubo_);

  vk::WriteDescriptorSet descriptorWrite;
  descriptorWrite.dstSet = pathTracingDescSets_[0];
  descriptorWrite.dstBinding = 1;
  descriptorWrite.dstArrayElement = 0;
  descriptorWrite.descriptorType = vk::DescriptorType::eUniformBuffer;
  descriptorWrite.descriptorCount = 1;
  descriptorWrite.pBufferInfo = &bufferInfo;

  logicalDevice_.updateDescriptorSets(descriptorWrite);
}

void RendererPT::updateUBOBuffer() {
  uboBuffer_.writeToBuffer(&ubo_, sizeof(ubo_));
}

void RendererPT::initializeAndBindSceneBuffer() {
  // Compute data size.
  vk::DeviceSize objectDataSize = sceneConverter_.getObjectData().size() * sizeof(GPUObjectData);
  vk::DeviceSize objectBVHNodesOffset = objectDataSize + (256 - objectDataSize % 256);
  vk::DeviceSize objectBVHNodesSize = sceneConverter_.getObjectBvhNodes().size() * sizeof(GPUBVHNode);
  vk::DeviceSize verticesOffset =
    (objectBVHNodesOffset + objectBVHNodesSize) + (256 - (objectBVHNodesOffset + objectBVHNodesSize) % 256);
  vk::DeviceSize verticesSize = sceneConverter_.getVertices().size() * sizeof(GPUVertex);
  vk::DeviceSize meshBVHNodesOffset = (verticesOffset + verticesSize) + (256 - (verticesOffset + verticesSize) % 256);
  vk::DeviceSize meshBVHNodesSize = sceneConverter_.getMeshBvhNodes().size() * sizeof(GPUBVHNode);
  vk::DeviceSize bufferSize = meshBVHNodesOffset + meshBVHNodesSize;

  // Allocate staging buffer.
  VmaAllocationCreateInfo stagingBufferAllocationInfo = {};
  stagingBufferAllocationInfo.usage = VmaMemoryUsage::VMA_MEMORY_USAGE_CPU_TO_GPU;

  vk::BufferCreateInfo stagingBufferInfo;
  stagingBufferInfo.size = bufferSize;
  stagingBufferInfo.usage = vk::BufferUsageFlagBits::eTransferSrc;
  stagingBufferInfo.sharingMode = vk::SharingMode::eExclusive;

  logi::VMABuffer stagingBuffer = allocator_.createBuffer(stagingBufferInfo, stagingBufferAllocationInfo);

  // Fill staging buffer with scene data.
  stagingBuffer.writeToBuffer(sceneConverter_.getObjectData().data(), objectDataSize);
  stagingBuffer.writeToBuffer(sceneConverter_.getObjectBvhNodes().data(), objectBVHNodesSize, objectBVHNodesOffset);
  stagingBuffer.writeToBuffer(sceneConverter_.getVertices().data(), verticesSize, verticesOffset);
  stagingBuffer.writeToBuffer(sceneConverter_.getMeshBvhNodes().data(), meshBVHNodesSize, meshBVHNodesOffset);

  // Destroy existing scene buffer.
  if (sceneBuffer_) {
    sceneBuffer_.destroy();
  }

  // Allocate scene buffer on GPU
  VmaAllocationCreateInfo sceneBufferAllocationInfo = {};
  sceneBufferAllocationInfo.usage = VmaMemoryUsage::VMA_MEMORY_USAGE_GPU_ONLY;

  vk::BufferCreateInfo sceneBufferInfo;
  sceneBufferInfo.size = bufferSize;
  sceneBufferInfo.usage = vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer;
  sceneBufferInfo.sharingMode = vk::SharingMode::eExclusive;

  sceneBuffer_ = allocator_.createBuffer(sceneBufferInfo, sceneBufferAllocationInfo);

  // Copy scene data from staging buffer to scene buffer.
  blockingBufferCopy(stagingBuffer, sceneBuffer_, bufferSize);

  // Destroy staging buffer.
  stagingBuffer.destroy();

  // Update descriptor sets.
  std::array<vk::WriteDescriptorSet, 4> descriptorWrites;

  // Object data binding
  vk::DescriptorBufferInfo objectDataBufferInfo;
  objectDataBufferInfo.buffer = sceneBuffer_;
  objectDataBufferInfo.offset = 0;
  objectDataBufferInfo.range = objectDataSize;

  descriptorWrites[0].dstSet = pathTracingDescSets_[0];
  descriptorWrites[0].dstBinding = 2;
  descriptorWrites[0].dstArrayElement = 0;
  descriptorWrites[0].descriptorType = vk::DescriptorType::eStorageBuffer;
  descriptorWrites[0].descriptorCount = 1;
  descriptorWrites[0].pBufferInfo = &objectDataBufferInfo;

  // Object BVH nodes binding
  vk::DescriptorBufferInfo objectBVHNodesInfo;
  objectBVHNodesInfo.buffer = sceneBuffer_;
  objectBVHNodesInfo.offset = objectBVHNodesOffset;
  objectBVHNodesInfo.range = objectBVHNodesSize;

  descriptorWrites[1].dstSet = pathTracingDescSets_[0];
  descriptorWrites[1].dstBinding = 3;
  descriptorWrites[1].dstArrayElement = 0;
  descriptorWrites[1].descriptorType = vk::DescriptorType::eStorageBuffer;
  descriptorWrites[1].descriptorCount = 1;
  descriptorWrites[1].pBufferInfo = &objectBVHNodesInfo;

  // Vertices binding
  vk::DescriptorBufferInfo verticesInfo;
  verticesInfo.buffer = sceneBuffer_;
  verticesInfo.offset = verticesOffset;
  verticesInfo.range = verticesSize;

  descriptorWrites[2].dstSet = pathTracingDescSets_[0];
  descriptorWrites[2].dstBinding = 4;
  descriptorWrites[2].dstArrayElement = 0;
  descriptorWrites[2].descriptorType = vk::DescriptorType::eStorageBuffer;
  descriptorWrites[2].descriptorCount = 1;
  descriptorWrites[2].pBufferInfo = &verticesInfo;

  // Mesh BVH nodes binding
  vk::DescriptorBufferInfo meshBVHNodesInfo;
  meshBVHNodesInfo.buffer = sceneBuffer_;
  meshBVHNodesInfo.offset = meshBVHNodesOffset;
  meshBVHNodesInfo.range = meshBVHNodesSize;

  descriptorWrites[3].dstSet = pathTracingDescSets_[0];
  descriptorWrites[3].dstBinding = 5;
  descriptorWrites[3].dstArrayElement = 0;
  descriptorWrites[3].descriptorType = vk::DescriptorType::eStorageBuffer;
  descriptorWrites[3].descriptorCount = 1;
  descriptorWrites[3].pBufferInfo = &meshBVHNodesInfo;

  logicalDevice_.updateDescriptorSets(descriptorWrites);
  // We need to rerecord command buffers once we update descriptor sets.
  recordCommandBuffers();
}

void RendererPT::recordCommandBuffers() {
  // Destroy old command buffers.
  for (const auto& cmdBuffer : mainCmdBuffers_) {
    cmdBuffer.reset();
  }

  for (size_t i = 0; i < mainCmdBuffers_.size(); i++) {
    vk::CommandBufferBeginInfo beginInfo = {};
    beginInfo.flags = vk::CommandBufferUsageFlagBits::eSimultaneousUse;

    mainCmdBuffers_[i].begin(beginInfo);

    // Compute shader.
    mainCmdBuffers_[i].bindPipeline(vk::PipelineBindPoint::eCompute, pathTracingPipeline_);
    mainCmdBuffers_[i].bindDescriptorSets(
      vk::PipelineBindPoint::eCompute, pathTracingPipelineLayoutData_.layout, 0,
      std::vector<vk::DescriptorSet>(pathTracingDescSets_.begin(), pathTracingDescSets_.end()));
    mainCmdBuffers_[i].dispatch(static_cast<uint32_t>(std::ceil(swapchainImageExtent_.width / float(32))),
                                static_cast<uint32_t>(std::ceil(swapchainImageExtent_.height / float(32))), 1);

    vk::ImageMemoryBarrier imageMemoryBarrier;
    imageMemoryBarrier.srcAccessMask = vk::AccessFlagBits::eShaderWrite;
    imageMemoryBarrier.srcAccessMask = vk::AccessFlagBits::eShaderRead;
    imageMemoryBarrier.oldLayout = vk::ImageLayout::eGeneral;
    imageMemoryBarrier.newLayout = vk::ImageLayout::eGeneral;
    imageMemoryBarrier.image = accumulationTexture_.image;
    imageMemoryBarrier.subresourceRange.aspectMask = vk::ImageAspectFlagBits::eColor;
    imageMemoryBarrier.subresourceRange.baseArrayLayer = 0u;
    imageMemoryBarrier.subresourceRange.layerCount = 1u;
    imageMemoryBarrier.subresourceRange.baseMipLevel = 0u;
    imageMemoryBarrier.subresourceRange.levelCount = 1u;

    mainCmdBuffers_[i].pipelineBarrier(vk::PipelineStageFlagBits::eComputeShader,
                                       vk::PipelineStageFlagBits::eFragmentShader, {}, {}, {}, imageMemoryBarrier);

    vk::RenderPassBeginInfo renderPassInfo = {};
    renderPassInfo.renderPass = texViewerRenderPass_;
    renderPassInfo.framebuffer = framebuffers_[i];
    renderPassInfo.renderArea.extent = swapchainImageExtent_;

    vk::ClearValue clearValue;
    clearValue.color.setFloat32({0.0, 0.0, 0.0, 1.0});
    renderPassInfo.clearValueCount = 1;
    renderPassInfo.pClearValues = &clearValue;

    mainCmdBuffers_[i].beginRenderPass(renderPassInfo, vk::SubpassContents::eInline);
    mainCmdBuffers_[i].bindPipeline(vk::PipelineBindPoint::eGraphics, texViewerPipeline_);
    mainCmdBuffers_[i].bindDescriptorSets(
      vk::PipelineBindPoint::eGraphics, texViewerPipelineLayoutData_.layout, 0,
      std::vector<vk::DescriptorSet>(texViewerDescSets_.begin(), texViewerDescSets_.end()));

    mainCmdBuffers_[i].draw(3);
    mainCmdBuffers_[i].endRenderPass();
    mainCmdBuffers_[i].end();
  }
}

void RendererPT::preDraw() {
  if (selectedCameraTransform_->isWorldMatrixDirty()) {
    ubo_.camera.worldMatrix = selectedCameraTransform_->worldMatrix();
    ubo_.sampleCount = 0;
  }

  updateUBOBuffer();
}

void RendererPT::postDraw() {
  ubo_.sampleCount++;
  std::cout << "Sample: " << ubo_.sampleCount << std::endl;
}
void RendererPT::drawFrame() {
  if (sceneLoaded_) {
    RendererCore::drawFrame();
  }
}
