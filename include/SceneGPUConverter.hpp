//
// Created by primoz on 7. 08. 19.
//

#ifndef LOGIPATHTRACER_SCENEGPUCONVERTER_HPP
#define LOGIPATHTRACER_SCENEGPUCONVERTER_HPP

#include <glm/glm.hpp>
#include <lsg/lsg.h>
#include <vector>

struct GPUObjectData {
  explicit GPUObjectData(const glm::mat4& world_matrix = {}, const glm::mat4& world_matrix_inverse = {},
                         const glm::vec4& base_color_factor = {}, const glm::vec3& emission_factor = {},
                         float metallic_factor = {}, float roughness_factor = {}, uint32_t bvh_offset = {},
                         uint32_t vertices_offset = {});

  glm::mat4 worldMatrix;
  glm::mat4 worldMatrixInverse;
  glm::vec4 baseColorFactor;
  glm::vec3 emissionFactor;
  float metallicFactor;
  float roughnessFactor;
  uint32_t bvhOffset;
  uint32_t verticesOffset;
};

struct GPUVertex {
  explicit GPUVertex(const glm::vec3& position = {}, const glm::vec3& normal = {});

  glm::vec3 position;
  glm::vec3 normal;
};

class SceneGPUConverter {
 public:
  SceneGPUConverter() = default;

  void loadScene(const lsg::Ref<lsg::Scene>& scene);

  void clear();

  const lsg::Ref<lsg::Scene>& getScene() const;

  const std::vector<lsg::Ref<lsg::Object>>& getCameras() const;

  const std::vector<GPUObjectData>& getObjectData() const;

  const std::vector<lsg::BVH<float>::Node>& getObjectBvhNodes() const;

  const std::vector<GPUVertex>& getVertices() const;

  const std::vector<lsg::BVH<float>::Node>& getMeshBvhNodes() const;

 private:
  lsg::Ref<lsg::Scene> scene_;
  std::vector<lsg::Ref<lsg::Object>> cameras_;
  std::vector<GPUObjectData> objectData_;
  std::vector<lsg::BVH<float>::Node> objectBVHNodes_;
  std::vector<GPUVertex> vertices_;
  std::vector<lsg::BVH<float>::Node> meshBVHNodes_;
};

#endif // LOGIPATHTRACER_SCENEGPUCONVERTER_HPP
