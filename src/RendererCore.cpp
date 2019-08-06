#include "RendererCore.hpp"
#include <cppglfw/GLFWManager.h>
#include <utility>

RendererConfiguration::RendererConfiguration(std::string windowTitle, int32_t windowWidth, int32_t windowHeight,
                                             std::vector<const char*> instanceExtensions,
                                             std::vector<const char*> deviceExtensions,
                                             std::vector<const char*> validationLayers)
  : windowTitle(std::move(windowTitle)), windowWidth(windowWidth), windowHeight(windowHeight),
    instanceExtensions(std::move(instanceExtensions)), deviceExtensions(std::move(deviceExtensions)),
    validationLayers(std::move(validationLayers)) {}

ShaderInfo::ShaderInfo(std::string path, std::string entryPoint)
  : path(std::move(path)), entryPoint(std::move(entryPoint)) {}

PipelineLayoutData::PipelineLayoutData(std::map<vk::ShaderStageFlagBits, logi::ShaderModule> shaders,
                                       logi::PipelineLayout layout,
                                       std::vector<logi::DescriptorSetLayout> descriptorSetLayouts)
  : shaders(std::move(shaders)), layout(std::move(layout)), descriptorSetLayouts(std::move(descriptorSetLayouts)) {}

VkBool32 debugCallback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT obj_type, uint64_t obj, size_t location,
                       int32_t code, const char* layer_prefix, const char* msg, void* user_data) {
  std::cout << "Validation layer: " << msg << std::endl;
  return VK_FALSE;
}

RendererCore::RendererCore(cppglfw::Window window, const RendererConfiguration& configuration)
  : window_(std::move(window)) {
  // Create instance.
  createInstance(configuration.instanceExtensions, configuration.validationLayers);
  // Create surface and register it on to the instance.
  surface_ = instance_.registerSurfaceKHR(window_.createWindowSurface(instance_).value);
  selectPhysicalDevice();
  createLogicalDevice(configuration.deviceExtensions);
  initializeSwapChain();
  buildSyncObjects();
  initializeCommandBuffers();
}

void RendererCore::createInstance(const std::vector<const char*>& extensions,
                                  const std::vector<const char*>& validationLayers) {
  // Add required extensions.
  std::vector<const char*> allExtensions;
  allExtensions = cppglfw::GLFWManager::instance().getRequiredInstanceExtensions();
  allExtensions.insert(allExtensions.end(), extensions.begin(), extensions.end());
  allExtensions.emplace_back(VK_EXT_DEBUG_REPORT_EXTENSION_NAME);

  // Remove duplicate extension.
  std::sort(allExtensions.begin(), allExtensions.end());
  allExtensions.erase(std::unique(allExtensions.begin(), allExtensions.end()), allExtensions.end());

  // Create instance.
  vk::InstanceCreateInfo instanceCI;
  instanceCI.ppEnabledLayerNames = validationLayers.data();
  instanceCI.enabledLayerCount = validationLayers.size();
  instanceCI.ppEnabledExtensionNames = allExtensions.data();
  instanceCI.enabledExtensionCount = allExtensions.size();

  instance_ = logi::createInstance(
    instanceCI, reinterpret_cast<PFN_vkCreateInstance>(glfwGetInstanceProcAddress(nullptr, "vkCreateInstance")),
    reinterpret_cast<PFN_vkGetInstanceProcAddr>(glfwGetInstanceProcAddress(nullptr, "vkGetInstanceProcAddr")));

  // Setup debug report callback.
  vk::DebugReportCallbackCreateInfoEXT debugReportCI;
  debugReportCI.flags =
    vk::DebugReportFlagBitsEXT::eError | vk::DebugReportFlagBitsEXT::eDebug | vk::DebugReportFlagBitsEXT::eWarning;
  debugReportCI.pfnCallback = debugCallback;

  instance_.createDebugReportCallbackEXT(debugReportCI);
}

void RendererCore::selectPhysicalDevice() {
  // Select GPU
  const std::vector<logi::PhysicalDevice>& devices = instance_.enumeratePhysicalDevices();

  // TODO: Implement better GPU selection for systems with multiple dedicated GPU-s.
  for (const auto& device : devices) {
    vk::PhysicalDeviceType type = device.getProperties().deviceType;

    if (type == vk::PhysicalDeviceType::eDiscreteGpu) {
      // If discrete gpu is found select it immediately.
      physicalDevice_ = device;
      return;
    } else if (type == vk::PhysicalDeviceType::eIntegratedGpu || type == vk::PhysicalDeviceType::eVirtualGpu) {
      physicalDevice_ = device;
    }
  }

  // Assert if no device is found.
  assert(physicalDevice_);
}

void RendererCore::createLogicalDevice(const std::vector<const char*>& deviceExtensions) {
  std::vector<vk::QueueFamilyProperties> familyProperties = physicalDevice_.getQueueFamilyProperties();

  // Search for graphical queue family.
  uint32_t graphicsFamilyIdx = std::numeric_limits<uint32_t>::max();
  uint32_t presentFamilyIdx = std::numeric_limits<uint32_t>::max();

  for (uint32_t i = 0; i < familyProperties.size(); i++) {
    if (familyProperties[i].queueFlags | vk::QueueFlagBits::eGraphics) {
      graphicsFamilyIdx = i;
    }

    // Check if queue family supports present.
    if (physicalDevice_.getSurfaceSupportKHR(graphicsFamilyIdx, surface_)) {
      presentFamilyIdx = graphicsFamilyIdx;
    }

    // Stop once both graphical and present queue families are found.
    if (graphicsFamilyIdx != std::numeric_limits<uint32_t>::max() &&
        presentFamilyIdx != std::numeric_limits<uint32_t>::max()) {
      break;
    }
  }

  if (graphicsFamilyIdx == std::numeric_limits<uint32_t>::max()) {
    throw std::runtime_error("Failed to find graphical queue.");
  }
  if (presentFamilyIdx == std::numeric_limits<uint32_t>::max()) {
    throw std::runtime_error("Failed to find queue family that supports presentation.");
  }

  std::vector<const char*> extensions{VK_KHR_SWAPCHAIN_EXTENSION_NAME};
  extensions.insert(extensions.end(), deviceExtensions.begin(), deviceExtensions.end());

  static const std::array<float, 1> kPriorities = {1.0f};

  std::vector<vk::DeviceQueueCreateInfo> queueCIs;
  queueCIs.emplace_back(vk::DeviceQueueCreateFlags(), graphicsFamilyIdx, 1u, kPriorities.data());
  if (graphicsFamilyIdx != presentFamilyIdx) {
    queueCIs.emplace_back(vk::DeviceQueueCreateFlags(), presentFamilyIdx, 1u, kPriorities.data());
  }

  vk::DeviceCreateInfo deviceCI;
  deviceCI.enabledExtensionCount = extensions.size();
  deviceCI.ppEnabledExtensionNames = extensions.data();
  deviceCI.queueCreateInfoCount = queueCIs.size();
  deviceCI.pQueueCreateInfos = queueCIs.data();

  logicalDevice_ = physicalDevice_.createLogicalDevice(deviceCI);
  std::vector<logi::QueueFamily> queueFamilies = logicalDevice_.enumerateQueueFamilies();
  for (const auto& family : queueFamilies) {
    if (static_cast<uint32_t>(family) == graphicsFamilyIdx) {
      graphicsFamily_ = family;
    }
    if (static_cast<uint32_t>(family) == presentFamilyIdx) {
      presentFamily_ = family;
    }
  }

  assert(graphicsFamily_);
  assert(presentFamily_);

  graphicsQueue_ = graphicsFamily_.getQueue(0);
  presentQueue_ = presentFamily_.getQueue(0);
}

vk::SurfaceFormatKHR RendererCore::chooseSwapSurfaceFormat() {
  const std::vector<vk::SurfaceFormatKHR>& availableFormats = physicalDevice_.getSurfaceFormatsKHR(surface_);

  for (const auto& availableFormat : availableFormats) {
    if (availableFormat.format == vk::Format::eB8G8R8A8Unorm &&
        availableFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
      return availableFormat;
    }
  }

  return availableFormats[0];
}

vk::PresentModeKHR RendererCore::chooseSwapPresentMode() {
  const std::vector<vk::PresentModeKHR>& availablePresentModes = physicalDevice_.getSurfacePresentModesKHR(surface_);

  vk::PresentModeKHR bestMode = vk::PresentModeKHR::eFifo;

  for (const auto& availablePresentMode : availablePresentModes) {
    if (availablePresentMode == vk::PresentModeKHR::eMailbox) {
      return availablePresentMode;
    } else if (availablePresentMode == vk::PresentModeKHR::eImmediate) {
      bestMode = availablePresentMode;
    }
  }

  return bestMode;
}

vk::Extent2D RendererCore::chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities) {
  if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
    return capabilities.currentExtent;
  } else {
    vk::Extent2D actualExtent(window_.getSize().first, window_.getSize().second);

    actualExtent.width =
      std::max(capabilities.minImageExtent.width, std::min(capabilities.maxImageExtent.width, actualExtent.width));
    actualExtent.height =
      std::max(capabilities.minImageExtent.height, std::min(capabilities.maxImageExtent.height, actualExtent.height));

    return actualExtent;
  }
}

void RendererCore::initializeSwapChain() {
  vk::SurfaceCapabilitiesKHR capabilities = physicalDevice_.getSurfaceCapabilitiesKHR(surface_);

  vk::SurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat();
  vk::PresentModeKHR presentMode = chooseSwapPresentMode();
  vk::Extent2D extent = chooseSwapExtent(capabilities);

  uint32_t imageCount = capabilities.minImageCount + 1;
  if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
    imageCount = capabilities.maxImageCount;
  }

  logi::SwapchainKHR oldSwapchain = swapchain_;

  vk::SwapchainCreateInfoKHR createInfo = {};
  createInfo.surface = surface_;
  createInfo.minImageCount = imageCount;
  createInfo.imageFormat = surfaceFormat.format;
  createInfo.imageColorSpace = surfaceFormat.colorSpace;
  createInfo.imageExtent = extent;
  createInfo.imageArrayLayers = 1;
  createInfo.imageUsage = vk::ImageUsageFlagBits::eColorAttachment;
  if (graphicsFamily_ != presentFamily_) {
    createInfo.imageSharingMode = vk::SharingMode::eConcurrent;
    createInfo.queueFamilyIndexCount = 2;
    uint32_t indices[2]{graphicsFamily_, presentFamily_};
    createInfo.pQueueFamilyIndices = indices;
  } else {
    createInfo.imageSharingMode = vk::SharingMode::eExclusive;
  }

  createInfo.preTransform = capabilities.currentTransform;
  createInfo.compositeAlpha = vk::CompositeAlphaFlagBitsKHR::eOpaque;
  createInfo.presentMode = presentMode;
  createInfo.clipped = VK_TRUE;
  createInfo.oldSwapchain = static_cast<vk::SwapchainKHR>(oldSwapchain);

  swapchain_ = logicalDevice_.createSwapchainKHR(createInfo);
  // Destroy old swapchain and clear the images.
  if (oldSwapchain) {
    oldSwapchain.destroy();
  }

  swapchainImages_.clear();
  swapchainImageViews_.clear();

  swapchainImages_ = swapchain_.getImagesKHR();
  swapchainImageFormat_ = surfaceFormat.format;
  swapchainImageExtent_ = extent;

  // Create image views
  swapchainImageViews_.reserve(swapchainImages_.size());

  for (auto& image : swapchainImages_) {
    swapchainImageViews_.emplace_back(image.createImageView(
      vk::ImageViewCreateFlags(), vk::ImageViewType::e2D, swapchainImageFormat_, vk::ComponentMapping(),
      vk::ImageSubresourceRange(vk::ImageAspectFlagBits::eColor, 0, 1, 0, 1)));
  }
}

void RendererCore::recreateSwapChain() {
  initializeSwapChain();
  onSwapChainRecreate();
}

void RendererCore::buildSyncObjects() {
  swapchainImgAvailableSemaphore_ = logicalDevice_.createSemaphore(vk::SemaphoreCreateInfo());
  renderFinishedSemaphore_ = logicalDevice_.createSemaphore(vk::SemaphoreCreateInfo());
  inFlightFence_ = logicalDevice_.createFence(vk::FenceCreateInfo(vk::FenceCreateFlagBits::eSignaled));
}

logi::ShaderModule RendererCore::createShaderModule(const std::string& shaderPath) {
  std::ifstream file(shaderPath, std::ios::ate | std::ios::binary);

  if (!file.is_open()) {
    throw std::runtime_error("failed to open file!");
  }

  size_t fileSize = (size_t) file.tellg();
  std::vector<char> code(fileSize);

  file.seekg(0);
  file.read(code.data(), fileSize);

  file.close();

  vk::ShaderModuleCreateInfo createInfo;
  createInfo.codeSize = code.size();
  createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data());

  return logicalDevice_.createShaderModule(createInfo);
}

PipelineLayoutData RendererCore::loadPipelineShaders(const std::vector<ShaderInfo>& shaderInfo) {
  PipelineLayoutData layoutData;
  std::vector<logi::ShaderStage> stages;

  for (const auto& entry : shaderInfo) {
    logi::ShaderModule shader = createShaderModule(entry.path);
    vk::ShaderStageFlagBits stage = shader.getEntryPointReflectionInfo(entry.entryPoint).stage;

    if (layoutData.shaders.find(stage) != layoutData.shaders.end()) {
      throw std::runtime_error("Found multiple shaders for same stage in the pipeline.");
    }

    layoutData.shaders.emplace(stage, shader);
    stages.emplace_back(shader, entry.entryPoint);
  }

  // Generate descriptor set layouts.
  std::vector<logi::DescriptorSetReflectionInfo> descriptorSetInfo = logi::reflectDescriptorSets(stages);
  layoutData.descriptorSetLayouts.reserve(descriptorSetInfo.size());

  for (const auto& info : descriptorSetInfo) {
    // Generate binding infos.
    std::vector<vk::DescriptorSetLayoutBinding> bindings(info.bindings.begin(), info.bindings.end());

    vk::DescriptorSetLayoutCreateInfo descriptorSetLayoutInfo;
    descriptorSetLayoutInfo.bindingCount = bindings.size();
    descriptorSetLayoutInfo.pBindings = bindings.data();

    layoutData.descriptorSetLayouts.emplace_back(logicalDevice_.createDescriptorSetLayout(descriptorSetLayoutInfo));
  }

  // Generate push constant ranges.
  std::vector<logi::PushConstantReflectionInfo> pushConstants = logi::reflectPushConstants(stages);
  std::vector<vk::PushConstantRange> pushConstantRanges(pushConstants.begin(), pushConstants.end());

  // Pipeline layout
  std::vector<vk::DescriptorSetLayout> descriptorSetLayouts(layoutData.descriptorSetLayouts.begin(),
                                                            layoutData.descriptorSetLayouts.end());
  vk::PipelineLayoutCreateInfo pipelineLayoutInfo;
  pipelineLayoutInfo.setLayoutCount = layoutData.descriptorSetLayouts.size();
  pipelineLayoutInfo.pSetLayouts = descriptorSetLayouts.data();
  pipelineLayoutInfo.pushConstantRangeCount = pushConstantRanges.size();
  pipelineLayoutInfo.pPushConstantRanges = pushConstantRanges.data();

  layoutData.layout = logicalDevice_.createPipelineLayout(pipelineLayoutInfo);

  return layoutData;
}

void RendererCore::initializeCommandBuffers() {
  graphicsFamilyCmdPool_ = graphicsFamily_.createCommandPool(vk::CommandPoolCreateFlagBits::eResetCommandBuffer);
  mainCmdBuffers_ =
    graphicsFamilyCmdPool_.allocateCommandBuffers(vk::CommandBufferLevel::ePrimary, swapchainImages_.size());
}

void RendererCore::drawFrame() {
  try {
    // Wait if drawing is still in progress.
    inFlightFence_.wait(std::numeric_limits<uint64_t>::max());

    // Acquire next image.
    const uint32_t imageIndex =
      swapchain_.acquireNextImageKHR(std::numeric_limits<uint64_t>::max(), swapchainImgAvailableSemaphore_, nullptr)
        .value;
    inFlightFence_.reset();

    static const vk::PipelineStageFlags wait_stages{vk::PipelineStageFlagBits::eColorAttachmentOutput};

    vk::SubmitInfo submit_info;
    submit_info.pWaitDstStageMask = &wait_stages;
    submit_info.pWaitSemaphores = &static_cast<const vk::Semaphore&>(swapchainImgAvailableSemaphore_);
    submit_info.waitSemaphoreCount = 1u;

    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &static_cast<const vk::CommandBuffer&>(mainCmdBuffers_[imageIndex]);

    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = &static_cast<const vk::Semaphore&>(renderFinishedSemaphore_);
    graphicsQueue_.submit({submit_info}, inFlightFence_);

    // Present image.
    presentQueue_.presentKHR(vk::PresentInfoKHR(1, &static_cast<const vk::Semaphore&>(renderFinishedSemaphore_), 1,
                                                &static_cast<const vk::SwapchainKHR&>(swapchain_), &imageIndex));
    currentFrame_ = (currentFrame_ + 1) % swapchainImages_.size();
  } catch (const vk::OutOfDateKHRError&) {
    logicalDevice_.waitIdle();
    recreateSwapChain();
  }
}