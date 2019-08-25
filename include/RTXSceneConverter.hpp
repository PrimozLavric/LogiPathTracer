//
// Created by primoz on 25. 08. 19.
//

#ifndef LOGIPATHTRACER_RTX_SCENE_CONVERTER_HPP
#define LOGIPATHTRACER_RTX_SCENE_CONVERTER_HPP

#include <logi/logi.hpp>
#include <lsg/lsg.h>

struct RTMesh {
  RTMesh() = default;

  glm::mat3x4 transform;
  vk::IndexType indexType = vk::IndexType::eNoneNV;
  logi::VMABuffer indices;
  logi::VMABuffer vertices;
  logi::VMABuffer normals;

  vk::AccelerationStructureInfoNV accelerationStructureInfo;
  logi::VMAAccelerationStructureNV blas;
};

class RTXSceneConverter {
 public:
  RTXSceneConverter(logi::MemoryAllocator allocator, logi::CommandPool commandPool, logi::Queue transferQueue);

  void loadScene(const lsg::Ref<lsg::Scene>& scene);

  const logi::VMAAccelerationStructureNV& getTopLevelAccelerationStructure();

 protected:
  void loadMesh(const lsg::Ref<lsg::SubMesh>& subMesh, const glm::mat4x3& worldMatrix);

  logi::VMABuffer copyToGPU(void* data, size_t size, const vk::BufferUsageFlags& usageFlags);

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
  logi::VMAAccelerationStructureNV tlas_;
};

#endif // LOGIPATHTRACER_RTX_SCENE_CONVERTER_HPP
