#include "Renderer.hpp"
#include <cppglfw/GLFWManager.h>
#include <utility>

VkBool32 debugCallback(VkDebugReportFlagsEXT flags, VkDebugReportObjectTypeEXT obj_type, uint64_t obj, size_t location,
                       int32_t code, const char* layer_prefix, const char* msg, void* user_data) {
  std::cout << "Validation layer: " << msg << std::endl;
  return VK_FALSE;
}

Renderer::Renderer(cppglfw::Window window, const std::vector<const char*>& extensions,
                   const std::vector<const char*>& validationLayers)
  : window_(std::move(window)) {
  // Create instance.
  createInstance(extensions, validationLayers);
  // Create surface and register it on to the instance.
  surface_ = instance_.registerSurfaceKHR(window_.createWindowSurface(instance_).value);
  selectPhysicalDevice();
  createLogicalDevice();
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

void Renderer::createLogicalDevice() {
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
  extensions.insert(extensions.end(), config_.deviceExtensions.begin(), config_.deviceExtensions.end());

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
