
#define GLFW_INCLUDE_VULKAN
#include <Renderer.hpp>
#include <cppglfw/CppGLFW.h>
#include <logi/logi.hpp>
#include <vulkan/vulkan.hpp>

int main() {
  cppglfw::GLFWManager& glfwInstance = cppglfw::GLFWManager::instance();
  cppglfw::Window window = glfwInstance.createWindow("Test", 800, 600);

  uint32_t a = 0;
  auto b = glfwInstance.getRequiredInstanceExtensions();

  vk::InstanceCreateInfo instanceCreateInfo;
  // instanceCreateInfo.logi::createInstance()

  while (!window.shouldClose()) {
    glfwInstance.pollEvents();
  }

  return 0;
}