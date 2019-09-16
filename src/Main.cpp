
#define GLFW_INCLUDE_VULKAN
#include <cppglfw/CppGLFW.h>
#include <logi/logi.hpp>
#include "lodepng.h"
#include "lz4.h"

#define LSG_VULKAN
#include <lsg/lsg.h>
#include <thread>
#include <vulkan/vulkan.hpp>
#include "App.h"
#include "RendererPT.h"
#include "RendererRTX.h"

enum class Compression { NONE, LZ4, ZLIB };

const bool RTX = true;
std::unique_ptr<RendererCore> renderer;
std::atomic<bool> sceneLoaded = false;
std::atomic_flag spinLockFlag = false;

constexpr bool sendDiff = true;
constexpr Compression compression = Compression::LZ4;

glm::uvec2 resolution(1920, 1200);

size_t imageSize = resolution.x * resolution.y * 3;
size_t signBitfieldSize = glm::ceil((resolution.x * resolution.y * 3) / 8.0);

struct PerSocketData {
  PerSocketData()
    : transmissionBuffer(imageSize + signBitfieldSize, std::byte(0)),
      compressionBuffer(LZ4_compressBound(imageSize + signBitfieldSize)), imageData(imageSize, glm::u8vec3(0)) {}

  std::vector<std::byte> transmissionBuffer;
  std::vector<char> compressionBuffer;
  std::vector<glm::u8vec3> imageData;
};

void network() {
  uWS::App()
    .ws<PerSocketData>(
      "/*",
      {.compression = (compression == Compression::ZLIB) ? uWS::DEDICATED_COMPRESSOR : uWS::DISABLED,
       .maxPayloadLength = 64 * 1024,
       .idleTimeout = 120,
       /* Handlers */
       .open = [](auto* ws, auto* req) { new (ws->getUserData()) PerSocketData(); },
       .message =
         [](auto* ws, std::string_view message, uWS::OpCode opCode) {
           ScreenshotData ssData({}, 0, 0);
           {
             while (spinLockFlag.test_and_set(std::memory_order_acquire))
               ;
             ssData = renderer->getScreenshot();
             spinLockFlag.clear(std::memory_order_release);
           }
           std::vector<glm::u8vec3>& currentImage = ssData.data;

           char* dataToSend;
           size_t dataSize;

           if constexpr (sendDiff) {
             std::vector<std::byte>& transmissionBuffer =
               reinterpret_cast<PerSocketData*>(ws->getUserData())->transmissionBuffer;
             std::vector<glm::u8vec3>& previousImage = reinterpret_cast<PerSocketData*>(ws->getUserData())->imageData;

             size_t currentBit = 0;
             std::byte* currentImageByte = transmissionBuffer.data();
             std::byte* currentSignByte = transmissionBuffer.data() + imageSize;

             for (size_t i = 0; i < imageSize / 3; i++) {
               for (size_t j = 0; j < 3; j++) {
                 // Write color channel data.
                 uint8_t channelDiff = glm::abs(previousImage[i][j] - currentImage[i][j]);
                 *currentImageByte = static_cast<std::byte>(static_cast<uint8_t>(glm::abs(channelDiff)));
                 currentImageByte++;

                 if (previousImage[i][j] > currentImage[i][j]) {
                   *currentSignByte |= std::byte(1 << currentBit);
                 } else {
                   *currentSignByte &= ~std::byte(1 << currentBit);
                 }

                 if (++currentBit >= 8) {
                   currentBit = 0;
                   currentSignByte++;
                 }
               }
             }

             previousImage = currentImage;

             /*std::cout << std::chrono::duration_cast<std::chrono::microseconds>(std::chrono::steady_clock::now() -
                                                                                start)
                              .count() /
                            1000000.0
                       << std::endl;*/

             dataToSend = reinterpret_cast<char*>(transmissionBuffer.data());
             dataSize = transmissionBuffer.size();
           } else {
             dataToSend = reinterpret_cast<char*>(ssData.data.data());
             dataSize = imageSize;
           }

           if constexpr (compression == Compression::LZ4) {
             std::vector<char>& compressionBuffer =
               reinterpret_cast<PerSocketData*>(ws->getUserData())->compressionBuffer;

             dataSize = LZ4_compress_default(dataToSend, compressionBuffer.data(), dataSize, compressionBuffer.size());
             dataToSend = reinterpret_cast<char*>(compressionBuffer.data());
           }

           ws->send(std::string_view(dataToSend, dataSize), uWS::OpCode::BINARY, compression == Compression::ZLIB);
         },
       .drain = nullptr,
       .ping = nullptr,
       .pong = nullptr,
       .close = nullptr})
    .listen(10000,
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
  cppglfw::Window window =
    glfwInstance.createWindow("Test", resolution.x, resolution.y, {{GLFW_CLIENT_API, GLFW_NO_API}});

  RendererConfiguration config;
  config.renderScale = 1;
  config.validationLayers.clear();

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
    glfwInstance.pollEvents();

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

    if (sceneLoaded) {
      while (spinLockFlag.test_and_set(std::memory_order_acquire))
        ;
      renderer->drawFrame();
      spinLockFlag.clear(std::memory_order_release);
    }
  }

  loadThread.join();
  networkThread.join();
  return 0;
}