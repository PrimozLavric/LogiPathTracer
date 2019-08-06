
#define GLFW_INCLUDE_VULKAN
#include <cppglfw/CppGLFW.h>
#include <logi/logi.hpp>
#include <vulkan/vulkan.hpp>
#include "RendererPT.h"

int main() {
  cppglfw::GLFWManager& glfwInstance = cppglfw::GLFWManager::instance();
  cppglfw::Window window = glfwInstance.createWindow("Test", 800, 600, {{GLFW_CLIENT_API, GLFW_NO_API}});

  RendererConfiguration config;
  RendererPT renderer(window, config);

  while (!window.shouldClose()) {
    glfwInstance.pollEvents();
    renderer.drawFrame();
  }

  return 0;
}