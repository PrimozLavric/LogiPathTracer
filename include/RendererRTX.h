#ifndef LOGIPATHTRACER_RENDERERRTX_H
#define LOGIPATHTRACER_RENDERERRTX_H

#include <lsg/lsg.h>
#include "RendererCore.hpp"
#include "SceneGPUConverter.hpp"

struct Texture {
  logi::VMAImage image;
  logi::ImageView imageView;
  logi::Sampler sampler;
};

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

  void loadScene(const lsg::Ref<lsg::Scene>& scene);

  void drawFrame() override;

 protected:
  void createTexViewerRenderPass();

  void createFrameBuffers();

  void createTexViewerPipeline();

  void createPathTracingPipeline();

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
  struct CameraGPU {
    glm::mat4 worldMatrix;
    float fovY;
    std::byte padding[12];
  };

  struct PathTracerUBO {
    CameraGPU camera;
    uint32_t sampleCount = 0;
  };

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

  Texture accumulationTexture_;

  PathTracerUBO ubo_;
  logi::VMABuffer uboBuffer_;

  std::atomic<bool> sceneLoaded_ = false;
  lsg::Ref<lsg::Transform> selectedCameraTransform_;
  SceneGPUConverter sceneConverter_;
  logi::VMABuffer sceneBuffer_;
};

#endif // LOGIPATHTRACER_RENDERERRTX_H
