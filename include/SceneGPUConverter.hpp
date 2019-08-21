//
// Created by primoz on 7. 08. 19.
//

#ifndef LOGIPATHTRACER_SCENEGPUCONVERTER_HPP
#define LOGIPATHTRACER_SCENEGPUCONVERTER_HPP

#include <glm/glm.hpp>
#include <lsg/lsg.h>
#include <vector>

struct GPUObjectData {
  explicit GPUObjectData(const glm::mat4& worldMatrix = {}, const glm::mat4& worldMatrixInverse = {},
                         const glm::vec4& baseColorFactor = {}, const glm::vec3& emissionFactor = {},
                         float metallicFactor = {}, float roughnessFactor = {}, float transmissionFactor = {},
                         float ior = {}, uint32_t bvhOffset = {}, uint32_t verticesOffset = {});

  glm::mat4 worldMatrix;
  glm::mat4 worldMatrixInverse;
  glm::vec4 baseColorFactor;
  glm::vec3 emissionFactor;
  float metallicFactor;
  float roughnessFactor;
  float transmissionFactor;
  float ior;
  uint32_t bvhOffset;
  uint32_t verticesOffset;
  std::byte padding[12];
};

struct GPUVertex {
  explicit GPUVertex(const glm::vec3& position = {}, const glm::vec3& normal = {});

  alignas(16) glm::vec3 position;
  alignas(16) glm::vec3 normal;
};

struct GPUBVHNode {
  GPUBVHNode(const glm::vec3& min, const glm::vec3& max, bool isLeaf, const glm::uvec2& indices);

  alignas(16) glm::vec3 min;
  alignas(16) glm::vec3 max;
  alignas(4) uint32_t isLeaf;
  alignas(8) glm::uvec2 indices;
};

class SceneGPUConverter {
 public:
  SceneGPUConverter() {
    std::cout << "GPUObjectData (" << sizeof(GPUObjectData) << "):" << std::endl;
    std::cout << offsetof(GPUObjectData, worldMatrix) << std::endl;
    std::cout << offsetof(GPUObjectData, worldMatrixInverse) << std::endl;
    std::cout << offsetof(GPUObjectData, baseColorFactor) << std::endl;
    std::cout << offsetof(GPUObjectData, emissionFactor) << std::endl;
    std::cout << offsetof(GPUObjectData, metallicFactor) << std::endl;
    std::cout << offsetof(GPUObjectData, roughnessFactor) << std::endl;
    std::cout << offsetof(GPUObjectData, bvhOffset) << std::endl;
    std::cout << offsetof(GPUObjectData, verticesOffset) << std::endl << std::endl;

    std::cout << "GPUVertex (" << sizeof(GPUVertex) << "):" << std::endl;
    std::cout << offsetof(GPUVertex, position) << std::endl;
    std::cout << offsetof(GPUVertex, normal) << std::endl << std::endl;

    std::cout << "GPUBVHNode (" << sizeof(GPUBVHNode) << "):" << std::endl;
    std::cout << offsetof(GPUBVHNode, min) << std::endl;
    std::cout << offsetof(GPUBVHNode, max) << std::endl;
    std::cout << offsetof(GPUBVHNode, isLeaf) << std::endl;
    std::cout << offsetof(GPUBVHNode, indices) << std::endl;
  }

  void loadScene(const lsg::Ref<lsg::Scene>& scene);

  void clear();

  const lsg::Ref<lsg::Scene>& getScene() const;

  const std::vector<lsg::Ref<lsg::Object>>& getCameras() const;

  const std::vector<GPUObjectData>& getObjectData() const;

  const std::vector<GPUBVHNode>& getObjectBvhNodes() const;

  const std::vector<GPUVertex>& getVertices() const;

  const std::vector<GPUBVHNode>& getMeshBvhNodes() const;

 private:
  lsg::Ref<lsg::Scene> scene_;
  std::vector<lsg::Ref<lsg::Object>> cameras_;
  std::vector<GPUObjectData> objectData_;
  std::vector<GPUBVHNode> objectBVHNodes_;
  std::vector<GPUVertex> vertices_;
  std::vector<GPUBVHNode> meshBVHNodes_;
};

#endif // LOGIPATHTRACER_SCENEGPUCONVERTER_HPP
