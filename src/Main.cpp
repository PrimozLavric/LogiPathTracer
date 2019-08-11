
#define GLFW_INCLUDE_VULKAN
#include <cppglfw/CppGLFW.h>
#include <logi/logi.hpp>
#include <lsg/lsg.h>
#include <thread>
#include <vulkan/vulkan.hpp>
#include "RendererPT.h"

int main() {
  lsg::GLTFLoader loader;
  std::vector<lsg::Ref<lsg::Scene>> scenes = loader.load("./resources/cornell_box.gltf");
  // SceneGPUConverter sceneLoader;

  cppglfw::GLFWManager& glfwInstance = cppglfw::GLFWManager::instance();
  cppglfw::Window window = glfwInstance.createWindow("Test", 300, 300, {{GLFW_CLIENT_API, GLFW_NO_API}});

  RendererConfiguration config;
  RendererPT renderer(window, config);

  auto loadThread = std::thread([&]() { renderer.loadScene(scenes[0]); });

  while (!window.shouldClose()) {
    glfwInstance.pollEvents();
    renderer.drawFrame();
  }

  loadThread.join();

  return 0;
}