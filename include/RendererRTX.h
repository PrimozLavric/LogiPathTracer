#ifndef LOGIPATHTRACER_RENDERERRTX_H
#define LOGIPATHTRACER_RENDERERRTX_H

#include <lsg/lsg.h>
#include "GPUTexture.hpp"
#include "RTXSceneConverter.hpp"
#include "RendererCore.hpp"

struct VkGeometryInstance {
  float transform[12];
  uint32_t instanceId : 24;
  uint32_t mask : 8;
  uint32_t instanceOffset : 24;
  uint32_t flags : 8;
  uint64_t accelerationStructureHandle;
};

class RendererRTX : public RendererCore {
 public:
  RendererRTX(const cppglfw::Window& window, const RendererConfiguration& configuration);

  void loadScene(const lsg::Ref<lsg::Scene>& scene) override;

  void drawFrame() override;

 protected:
  void createTexViewerRenderPass();

  void createFrameBuffers();

  void createTexViewerPipeline();

  void createPathTracingPipeline();

  void createShaderBindingTable();

  void initializeAccumulationTexture();

  void initializeDescriptorSets();

  void updateAccumulationTexDescriptorSet();

  void initializeUBOBuffer();

  void updateUBOBuffer();

  void initializeAndBindSceneBuffer();

  void recordCommandBuffers();

  void onSwapChainRecreate() override;

  void preDraw() override;

  void postDraw() override;

 private:
  static constexpr uint32_t kIndexRaygen = 0u;
  static constexpr uint32_t kIndexMiss = 1u;
  static constexpr uint32_t kIndexClosestHit = 2u;

  struct CameraGPU {
    glm::mat4 worldMatrix;
    float fovY;
    std::byte padding[12];
  };

  struct PathTracerUBO {
    CameraGPU camera;
    uint32_t sampleCount = 0;
  };

  vk::PhysicalDeviceRayTracingPropertiesNV rayTracingProperties_;

  logi::DescriptorPool descriptorPool_;
  logi::MemoryAllocator allocator_;

  logi::RenderPass texViewerRenderPass_;
  std::vector<logi::Framebuffer> framebuffers_;

  // Pipelines layout data.
  PipelineLayoutData texViewerPipelineLayoutData_;
  logi::Pipeline texViewerPipeline_;
  std::vector<logi::DescriptorSet> texViewerDescSets_;

  PipelineLayoutData pathTracingPipelineLayoutData_;
  logi::Pipeline pathTracingPipeline_;
  std::vector<logi::DescriptorSet> pathTracingDescSets_;
  logi::VMABuffer shaderBindingTable_;

  GPUTexture accumulationTexture_;

  PathTracerUBO ubo_;
  logi::VMABuffer uboBuffer_;

  RTXSceneConverter sceneConverter_;
  std::atomic<bool> sceneLoaded_ = false;
  lsg::Ref<lsg::Transform> selectedCameraTransform_;
};

#endif // LOGIPATHTRACER_RENDERERRTX_H
