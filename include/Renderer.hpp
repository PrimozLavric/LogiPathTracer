
#ifndef LOGIPATHTRACER_RENDERER_HPP
#define LOGIPATHTRACER_RENDERER_HPP

#include <cppglfw/Window.h>
#include <logi/logi.hpp>
#include <vector>

class Renderer {
 public:
  explicit Renderer(cppglfw::Window window, const std::vector<const char*>& extensions = {},
                    const std::vector<const char*>& validationLayers = {});

 protected:
  void createInstance(const std::vector<const char*>& extensions, const std::vector<const char*>& validationLayers);

  void selectPhysicalDevice();

  void createLogicalDevice();

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
};

#endif // LOGIPATHTRACER_RENDERER_HPP
