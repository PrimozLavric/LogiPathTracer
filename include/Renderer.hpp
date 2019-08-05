
#ifndef LOGIPATHTRACER_RENDERER_HPP
#define LOGIPATHTRACER_RENDERER_HPP

#define GLFW_INCLUDE_VULKAN
#include <cppglfw/CppGLFW.h>
#include <logi/logi.hpp>
#include <vector>

struct RendererConfiguration {
  explicit RendererConfiguration(std::string windowTitle = "Renderer", int32_t windowWidth = 1280,
                                 int32_t windowHeight = 720, std::vector<const char*> instanceExtensions = {},
                                 std::vector<const char*> deviceExtensions = {},
                                 std::vector<const char*> validationLayers = {"VK_LAYER_LUNARG_standard_validation"});

  std::string windowTitle;
  int32_t windowWidth;
  int32_t windowHeight;
  std::vector<const char*> instanceExtensions;
  std::vector<const char*> deviceExtensions;
  std::vector<const char*> validationLayers;
};

class Renderer {
 public:
  explicit Renderer(cppglfw::Window window, const RendererConfiguration& configuration);

 protected:
  void createInstance(const std::vector<const char*>& extensions, const std::vector<const char*>& validationLayers);

  void selectPhysicalDevice();

  void createLogicalDevice(const std::vector<const char*>& deviceExtensions);

  vk::SurfaceFormatKHR chooseSwapSurfaceFormat();

  vk::PresentModeKHR chooseSwapPresentMode();

  vk::Extent2D chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities);

  void initializeSwapChain();

 private:
  cppglfw::Window window_;
  logi::VulkanInstance instance_;
  logi::SurfaceKHR surface_;
  logi::PhysicalDevice physicalDevice_;
  logi::LogicalDevice logicalDevice_;
  logi::QueueFamily graphicsFamily_;
  logi::QueueFamily presentFamily_;
  logi::Queue graphicsQueue_;
  logi::Queue presentQueue_;

  logi::SwapchainKHR swapchain_;
  std::vector<logi::SwapchainImage> swapchainImages_;
  std::vector<logi::ImageView> swapchainImageViews_;
  vk::Extent2D swapchainImageExtent_;
  vk::Format swapchainImageFormat_;
};

#endif // LOGIPATHTRACER_RENDERER_HPP
