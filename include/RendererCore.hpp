
#ifndef LOGIPATHTRACER_RENDERERCORE_HPP
#define LOGIPATHTRACER_RENDERERCORE_HPP

#define GLFW_INCLUDE_VULKAN
#include <cppglfw/CppGLFW.h>
#include <fstream>
#include <logi/logi.hpp>
#include <map>
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

struct ShaderInfo {
  ShaderInfo(std::string path, std::string entryPoint);

  std::string path;
  std::string entryPoint;
};

struct PipelineLayoutData {
  explicit PipelineLayoutData(std::map<vk::ShaderStageFlagBits, logi::ShaderModule> shaders = {},
                              logi::PipelineLayout layout = {},
                              std::vector<logi::DescriptorSetLayout> descriptorSetLayouts = {});

  std::map<vk::ShaderStageFlagBits, logi::ShaderModule> shaders;
  logi::PipelineLayout layout;
  std::vector<logi::DescriptorSetLayout> descriptorSetLayouts;
};

class RendererCore {
 public:
  explicit RendererCore(cppglfw::Window window, const RendererConfiguration& configuration);

  void drawFrame();

 protected:
  void createInstance(const std::vector<const char*>& extensions, const std::vector<const char*>& validationLayers);

  void selectPhysicalDevice();

  void createLogicalDevice(const std::vector<const char*>& deviceExtensions);

  vk::SurfaceFormatKHR chooseSwapSurfaceFormat();

  vk::PresentModeKHR chooseSwapPresentMode();

  vk::Extent2D chooseSwapExtent(const vk::SurfaceCapabilitiesKHR& capabilities);

  void initializeSwapChain();

  void recreateSwapChain();

  virtual void onSwapChainRecreate() = 0;

  void buildSyncObjects();

  logi::ShaderModule createShaderModule(const std::string& shaderPath);

  PipelineLayoutData loadPipelineShaders(const std::vector<ShaderInfo>& shaderInfo);

  void initializeCommandBuffers();

  virtual void preDraw();

  virtual void postDraw();

 protected:
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

  // Synchronization objects
  logi::Semaphore swapchainImgAvailableSemaphore_;
  logi::Semaphore renderFinishedSemaphore_;
  logi::Fence inFlightFence_;

  logi::CommandPool graphicsFamilyCmdPool_;
  std::vector<logi::CommandBuffer> mainCmdBuffers_;

  size_t currentFrame_ = 0;
};

#endif // LOGIPATHTRACER_RENDERERCORE_HPP
