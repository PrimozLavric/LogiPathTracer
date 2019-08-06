//
// Created by primoz on 6. 08. 19.
//

#ifndef LOGIPATHTRACER_RENDERERPT_H
#define LOGIPATHTRACER_RENDERERPT_H

#include "RendererCore.hpp"

struct Texture {
  logi::VMAImage image;
  logi::ImageView imageView;
  logi::Sampler sampler;
};

class RendererPT : public RendererCore {
 public:
  RendererPT(const cppglfw::Window& window, const RendererConfiguration& configuration);

  void createTexViewerRenderPass();

  void createFrameBuffers();

  void createTexViewerPipeline();

  void createPathTracingPipeline();

  void recordCommandBuffers();

  void initializeAccumulationTexture();

  void initializeDescriptorSets();

  void updateAccumulationTexDescriptorSet();

 protected:
  void onSwapChainRecreate() override;

 private:
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
};

#endif // LOGIPATHTRACER_RENDERERPT_H
