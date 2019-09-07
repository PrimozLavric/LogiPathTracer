
#define GLFW_INCLUDE_VULKAN
#include <cppglfw/CppGLFW.h>
#include <logi/logi.hpp>
#include "lodepng.h"

#define LSG_VULKAN
#include <lsg/lsg.h>
#include <thread>
#include <vulkan/vulkan.hpp>
#include "App.h"
#include "RendererPT.h"
#include "RendererRTX.h"

const bool RTX = true;
std::unique_ptr<RendererCore> renderer;
std::atomic<bool> sceneLoaded = false;
std::mutex transmissionLock;

void network() {
  uWS::App()
    .ws<void>("/*", {.compression = uWS::DEDICATED_COMPRESSOR,
                     .maxPayloadLength = 4 * 3840 * 2160,
                     .idleTimeout = 60,
                     /* Handlers */
                     .open = nullptr,
                     .message =
                       [](auto* ws, std::string_view message, uWS::OpCode opCode) {
                         ScreenshotData ssData({}, 0, 0);
                         {
                           std::lock_guard lock(transmissionLock);
                           ssData = renderer->getScreenshot();
                         }

                         ws->send(std::string_view(reinterpret_cast<char*>(ssData.data.data()),
                                                   ssData.width * ssData.height * 3),
                                  uWS::OpCode::BINARY, true);
                       },
                     .drain = nullptr,
                     .ping = nullptr,
                     .pong = nullptr,
                     .close = nullptr})
    .listen(8000,
            [](auto* token) {
              if (token) {
                std::cout << "Listening on port " << 8000 << std::endl;
              }
            })
    .run();

  std::cout << "Failed to listen on port 8000" << std::endl;
}

int main() {
  lsg::GLTFLoader loader;
  std::vector<lsg::Ref<lsg::Scene>> scenes = loader.load("./resources/CornellBox/cornell.gltf");

  lsg::Ref<lsg::Object> camera;
  scenes[0]->traverseDown([&](const lsg::Ref<lsg::Object>& object) {
    if (object->getComponent<lsg::PerspectiveCamera>()) {
      camera = object;
    }
    return !camera;
  });

  lsg::Ref<lsg::Transform> cameraTransform = camera->getComponent<lsg::Transform>();

  cppglfw::GLFWManager& glfwInstance = cppglfw::GLFWManager::instance();
  cppglfw::Window window = glfwInstance.createWindow("Test", 1920, 1080, {{GLFW_CLIENT_API, GLFW_NO_API}});

  RendererConfiguration config;
  config.renderScale = 1;
  // config.validationLayers.clear();

  if (RTX) {
    config.deviceExtensions.emplace_back("VK_NV_ray_tracing");
    config.deviceExtensions.emplace_back("VK_KHR_get_memory_requirements2");
    config.instanceExtensions.emplace_back("VK_KHR_get_physical_device_properties2");
    renderer = std::make_unique<RendererRTX>(window, config);
  } else {
    renderer = std::make_unique<RendererPT>(window, config);
  }

  auto loadThread = std::thread([&]() {
    renderer->loadScene(scenes[0]);
    sceneLoaded = true;
  });

  auto networkThread = std::thread(&network);

  auto currentTime = std::chrono::high_resolution_clock::now();
  decltype(currentTime) previousTime;

  while (!window.shouldClose()) {
    // Update timepoints and compute delta time.
    previousTime = currentTime;

    currentTime = std::chrono::high_resolution_clock::now();
    auto dt = std::chrono::duration_cast<std::chrono::microseconds>(currentTime - previousTime).count() / 1000.0f;

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
    if (sceneLoaded) {
      std::lock_guard lock(transmissionLock);
      renderer->drawFrame();
    }
  }

  loadThread.join();
  networkThread.join();
  return 0;
}