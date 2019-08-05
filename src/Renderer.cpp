#include "Renderer.hpp"
#include <cppglfw/GLFWManager.h>
#include <utility>

RendererConfiguration::RendererConfiguration(std::string windowTitle, int32_t windowWidth, int32_t windowHeight,
                                             std::vector<const char*> instanceExtensions,
                                             std::vector<const char*> deviceExtensions,
                                             std::vector<const char*> validationLayers)
  : windowTitle(std::move(windowTitle)), windowWidth(windowWidth), windowHeight(windowHeight),
    instanceExtensions(std::move(instanceExtensions)), deviceExtensions(std::move(deviceExtensions)),
    validationLayers(std::move(validationLayers)) {}

VkBool32 debugCallback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT obj_type, uint64_t obj, size_t location,
                       int32_t code, const char* layer_prefix, const char* msg, void* user_data) {
  std::cout << "Validation layer: " << msg << std::endl;
  return VK_FALSE;
}

Renderer::Renderer(cppglfw::Window window, const RendererConfiguration& configuration) : window_(std::move(window)) {
  // Create instance.
  createInstance(configuration.instanceExtensions, configuration.validationLayers);
  // Create surface and register it on to the instance.
  surface_ = instance_.registerSurfaceKHR(window_.createWindowSurface(instance_).value);
  selectPhysicalDevice();
  createLogicalDevice(configuration.deviceExtensions);
  initializeSwapChain();
}

void Renderer::createInstance(const std::vector<const char*>& extensions,
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

void Renderer::selectPhysicalDevice() {
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

void Renderer::createLogicalDevice(const std::vector<const char*>& deviceExtensions) {
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

vk::SurfaceFormatKHR Renderer::chooseSwapSurfaceFormat() {
  const std::vector<vk::SurfaceFormatKHR>& availableFormats = physicalDevice_.getSurfaceFormatsKHR(surface_);

  for (const auto& availableFormat : availableFormats) {
    if (availableFormat.format == vk::Format::eB8G8R8A8Unorm &&
        availableFormat.colorSpace == vk::ColorSpaceKHR::eSrgbNonlinear) {
      return availableFormat;
    }
  }

  return availableFormats[0];
}

vk::PresentModeKHR Renderer::chooseSwapPresentMode() {
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

vk::Extent2D Renderer::chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities) {
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

void Renderer::initializeSwapChain() {
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
