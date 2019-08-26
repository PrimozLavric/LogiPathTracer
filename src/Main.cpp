
#define GLFW_INCLUDE_VULKAN
#include <cppglfw/CppGLFW.h>
#include <logi/logi.hpp>
#include <lsg/lsg.h>
#include <thread>
#include <vulkan/vulkan.hpp>
#include "RendererPT.h"
#include "RendererRTX.h"

int main() {
  try {
    lsg::GLTFLoader loader;
    std::vector<lsg::Ref<lsg::Scene>> scenes = loader.load("./resources/cornell_box.gltf");

    lsg::Ref<lsg::Object> camera;
    scenes[0]->traverseDown([&](const lsg::Ref<lsg::Object>& object) {
      if (object->getComponent<lsg::PerspectiveCamera>()) {
        camera = object;
      }
      return !camera;
    });

    lsg::Ref<lsg::Transform> cameraTransform = camera->getComponent<lsg::Transform>();

    cppglfw::GLFWManager& glfwInstance = cppglfw::GLFWManager::instance();
    cppglfw::Window window = glfwInstance.createWindow("Test", 1024, 768, {{GLFW_CLIENT_API, GLFW_NO_API}});

    RendererConfiguration config;
    config.deviceExtensions.emplace_back("VK_NV_ray_tracing");
    config.deviceExtensions.emplace_back("VK_KHR_get_memory_requirements2");
    config.instanceExtensions.emplace_back("VK_KHR_get_physical_device_properties2");
    RendererRTX renderer(window, config);
    auto loadThread = std::thread([&]() { renderer.loadScene(scenes[0]); });

    auto currentTime = std::chrono::high_resolution_clock::now();
    decltype(currentTime) previousTime;

    while (!window.shouldClose()) {
      // Update timepoints and compute delta time.
      previousTime = currentTime;
      currentTime = std::chrono::high_resolution_clock::now();
      auto dt = std::chrono::duration_cast<std::chrono::milliseconds>(currentTime - previousTime).count();

      if (window.getKey(GLFW_KEY_W) == GLFW_PRESS) {
        cameraTransform->translateZ(-dt / 200.0f);
      }
      if (window.getKey(GLFW_KEY_S) == GLFW_PRESS) {
        cameraTransform->translateZ(dt / 200.0f);
      }
      if (window.getKey(GLFW_KEY_A) == GLFW_PRESS) {
        cameraTransform->translateX(-dt / 200.0f);
      }
      if (window.getKey(GLFW_KEY_D) == GLFW_PRESS) {
        cameraTransform->translateX(dt / 200.0f);
      }
      if (window.getKey(GLFW_KEY_Q) == GLFW_PRESS) {
        cameraTransform->translateY(-dt / 200.0f);
      }
      if (window.getKey(GLFW_KEY_E) == GLFW_PRESS) {
        cameraTransform->translateY(dt / 200.0f);
      }

      if (window.getKey(GLFW_KEY_I) == GLFW_PRESS) {
        cameraTransform->rotateX(dt / 1000.0f);
      }
      if (window.getKey(GLFW_KEY_K) == GLFW_PRESS) {
        cameraTransform->rotateX(-dt / 1000.0f);
      }
      if (window.getKey(GLFW_KEY_J) == GLFW_PRESS) {
        cameraTransform->rotateY(dt / 1000.0f);
      }
      if (window.getKey(GLFW_KEY_L) == GLFW_PRESS) {
        cameraTransform->rotateY(-dt / 1000.0f);
      }
      if (window.getKey(GLFW_KEY_U) == GLFW_PRESS) {
        cameraTransform->rotateZ(dt / 1000.0f);
      }
      if (window.getKey(GLFW_KEY_O) == GLFW_PRESS) {
        cameraTransform->rotateZ(-dt / 1000.0f);
      }

      glfwInstance.pollEvents();
      renderer.drawFrame();
    }

    loadThread.join();
  } catch (const std::exception& e) {
    std::cout << e.what() << std::endl;
    while(true);
  }
  return 0;
}