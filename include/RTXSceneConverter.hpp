//
// Created by primoz on 25. 08. 19.
//

#ifndef LOGIPATHTRACER_RTX_SCENE_CONVERTER_HPP
#define LOGIPATHTRACER_RTX_SCENE_CONVERTER_HPP

#include <logi/logi.hpp>
#define LSG_VULKAN
#include <lsg/lsg.h>
#include "GPUTexture.hpp"

struct RTMesh {
  RTMesh() = default;

  glm::mat4x3 transform;
  vk::IndexType indexType = vk::IndexType::eNoneNV;
  logi::VMABuffer indices;
  logi::VMABuffer vertices;

  vk::AccelerationStructureInfoNV accelerationStructureInfo;
  logi::VMAAccelerationStructureNV blas;
};

struct RTXMaterial {
  RTXMaterial(const glm::vec4& baseColorFactor, const glm::vec3& emissionFactor, float metallicFactor,
              float roughnessFactor, float transmissionFactor, float ior, uint32_t verticesOffset);

  glm::vec4 baseColorFactor;
  glm::vec3 emissionFactor;
  float metallicFactor;
  float roughnessFactor;
  float transmissionFactor;
  float ior;
  uint32_t colorTexture = std::numeric_limits<uint32_t>::max();
  uint32_t emissionTexture = std::numeric_limits<uint32_t>::max();
  uint32_t metallicRoughnessTexture = std::numeric_limits<uint32_t>::max();
  uint32_t transmissionTexture = std::numeric_limits<uint32_t>::max();
  uint32_t verticesOffset;
};

struct RTXVertex {
  RTXVertex(const glm::vec3& normal, const glm::vec2& uv = {});

  alignas(16) glm::vec3 normal;
  alignas(16) glm::vec2 uv;
};

class RTXSceneConverter {
 public:
  RTXSceneConverter(logi::MemoryAllocator allocator, logi::CommandPool commandPool, logi::Queue transferQueue);

  void loadScene(const lsg::Ref<lsg::Scene>& scene);

  const logi::VMAAccelerationStructureNV& getTopLevelAccelerationStructure();

  const logi::VMABuffer& getMaterialsBuffer() const;

  const logi::VMABuffer& getVertexBuffer() const;

  const std::vector<GPUTexture>& getTextures() const;

 protected:
  void loadMesh(const lsg::Ref<lsg::SubMesh>& subMesh, const glm::mat4x3& worldMatrix);

  logi::VMABuffer copyToGPU(void* data, size_t size, const vk::BufferUsageFlags& usageFlags);

  uint32_t copyTextureToGPU(const lsg::Ref<lsg::Texture>& texture);

  void reset();

  logi::VMAAccelerationStructureNV createAccelerationStructure(vk::AccelerationStructureTypeNV type,
                                                               const std::vector<vk::GeometryNV>& geometries,
                                                               uint32_t instanceCount = 0,
                                                               const logi::Buffer& instance_buffer = {});

 private:
  logi::MemoryAllocator allocator_;
  logi::CommandPool commandPool_;
  logi::Queue transferQueue_;

  std::vector<RTMesh> rtMeshes_;
  std::vector<RTXMaterial> materials_;
  logi::VMABuffer materialsBuffer_;
  std::vector<RTXVertex> vertices_;
  logi::VMABuffer verticesBuffer_;

  logi::VMAAccelerationStructureNV tlas_;
  std::vector<GPUTexture> textures_;
};

#endif // LOGIPATHTRACER_RTX_SCENE_CONVERTER_HPP
