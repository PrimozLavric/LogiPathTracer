//
// Created by primoz on 7. 08. 19.
//

#ifndef LOGIPATHTRACER_PTSCENECONVERTER_HPP
#define LOGIPATHTRACER_PTSCENECONVERTER_HPP

#include <glm/glm.hpp>
#include <logi/logi.hpp>
#define LSG_VULKAN
#include <lsg/lsg.h>
#include <vector>
#include "GPUTexture.hpp"

struct GPUObjectData {
  explicit GPUObjectData(const glm::mat4& worldMatrix = {}, const glm::mat4& worldMatrixInverse = {},
                         const glm::vec4& baseColorFactor = {}, const glm::vec3& emissionFactor = {},
                         float metallicFactor = {}, float roughnessFactor = {}, float transmissionFactor = {},
                         float ior = {}, uint32_t colorTexture = std::numeric_limits<uint32_t>::max(),
                         uint32_t emissionTexture = std::numeric_limits<uint32_t>::max(),
                         uint32_t metallicRoughnessTexture = std::numeric_limits<uint32_t>::max(),
                         uint32_t transmissionTexture = std::numeric_limits<uint32_t>::max(), uint32_t bvhOffset = {},
                         uint32_t verticesOffset = {});

  glm::mat4 worldMatrix;
  glm::mat4 worldMatrixInverse;
  glm::vec4 baseColorFactor;
  glm::vec3 emissionFactor;
  float metallicFactor;
  float roughnessFactor;
  float transmissionFactor;
  float ior;
  uint32_t colorTexture;
  uint32_t emissionTexture;
  uint32_t metallicRoughnessTexture;
  uint32_t transmissionTexture;
  uint32_t bvhOffset;
  uint32_t verticesOffset;
  std::byte padding[12];
};

struct GPUVertex {
  explicit GPUVertex(const glm::vec3& position = {}, const glm::vec3& normal = {}, const glm::vec2& uv = {});

  alignas(16) glm::vec3 position;
  alignas(16) glm::vec3 normal;
  alignas(8) glm::vec2 uv;
};

struct GPUBVHNode {
  GPUBVHNode(const glm::vec3& min, const glm::vec3& max, bool isLeaf, const glm::uvec2& indices);

  alignas(16) glm::vec3 min;
  alignas(16) glm::vec3 max;
  alignas(4) uint32_t isLeaf;
  alignas(8) glm::uvec2 indices;
};

class PTSceneConverter {
 public:
  PTSceneConverter(logi::MemoryAllocator allocator, logi::CommandPool commandPool, logi::Queue transferQueue);

  void loadScene(const lsg::Ref<lsg::Scene>& scene);

  const std::vector<lsg::Ref<lsg::Object>>& getCameras() const;

  const logi::VMABuffer& getObjectDataBuffer() const;

  const logi::VMABuffer& getObjectBvhNodesBuffer() const;

  const logi::VMABuffer& getVerticesBuffer() const;

  const logi::VMABuffer& getMeshBvhNodesBuffer() const;

  const std::vector<GPUTexture>& getTextures() const;

  void reset();

 protected:
  logi::VMABuffer copyToGPU(void* data, size_t size, const vk::BufferUsageFlags& usageFlags);

  uint32_t copyTextureToGPU(const lsg::Ref<lsg::Texture>& texture);

 private:
  logi::MemoryAllocator allocator_;
  logi::CommandPool commandPool_;
  logi::Queue transferQueue_;

  std::vector<lsg::Ref<lsg::Object>> cameras_;

  std::vector<GPUObjectData> objectData_;
  std::vector<GPUBVHNode> objectBVHNodes_;
  std::vector<GPUVertex> vertices_;
  std::vector<GPUBVHNode> meshBVHNodes_;

  logi::VMABuffer objectDataBuffer_;
  logi::VMABuffer objectBVHNodesBuffer_;
  logi::VMABuffer verticesBuffer_;
  logi::VMABuffer meshBVHNodesBuffer_;

  std::vector<GPUTexture> textures_;
};

#endif // LOGIPATHTRACER_PTSCENECONVERTER_HPP
