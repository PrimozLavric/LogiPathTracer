
#define GLFW_INCLUDE_VULKAN
#include <cppglfw/CppGLFW.h>
#include <logi/logi.hpp>
#include <lsg/lsg.h>
#include <vulkan/vulkan.hpp>
#include "RendererPT.h"
#include "SceneGPUConverter.hpp"

int main() {
  lsg::GLTFLoader loader;
  std::vector<lsg::Ref<lsg::Scene>> scenes = loader.load("./resources/cornell_box.gltf");
  // SceneGPUConverter sceneLoader;
  // sceneLoader.loadScene(scenes[0]);

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