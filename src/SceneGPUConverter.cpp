//
// Created by primoz on 7. 08. 19.
//

#include "SceneGPUConverter.hpp"

GPUObjectData::GPUObjectData(const glm::mat4& world_matrix, const glm::mat4& world_matrix_inverse,
                             const glm::vec4& base_color_factor, const glm::vec3& emission_factor,
                             float metallic_factor, float roughness_factor, uint32_t bvh_offset,
                             uint32_t vertices_offset)
  : worldMatrix(world_matrix), worldMatrixInverse(world_matrix_inverse), baseColorFactor(base_color_factor),
    emissionFactor(emission_factor), metallicFactor(metallic_factor), roughnessFactor(roughness_factor),
    bvhOffset(bvh_offset), verticesOffset(vertices_offset) {}

GPUVertex::GPUVertex(const glm::vec3& position, const glm::vec3& normal) : position(position), normal(normal) {}

void SceneGPUConverter::loadScene(const lsg::Ref<lsg::Scene>& scene) {
  clear();
  scene_ = scene;

  std::vector<lsg::AABB<float>> objectAABBs;
  std::vector<GPUObjectData> unorderedObjectData;

  for (const auto& rootObj : scene_->children()) {
    rootObj->traverseDown([&](const lsg::Ref<lsg::Object>& object) {
      // Get world matrix.
      glm::mat4 worldMatrix(1.0f);

      if (auto transform = object->getComponent<lsg::Transform>()) {
        worldMatrix = transform->worldMatrix();
      }

      // Handle cameras.
      if (auto camera = object->getComponent<lsg::Camera>()) {
        cameras_.emplace_back(object);
      }

      // Handle geometry.
      if (auto mesh = object->getComponent<lsg::Mesh>()) {
        std::cout << "Building BVH for object " << object->name() << "..." << std::flush;

        for (const auto& submesh : mesh->subMeshes()) {
          lsg::Ref<lsg::MetallicRoughnessMaterial> material =
            lsg::dynamicRefCast<lsg::MetallicRoughnessMaterial>(submesh->material());

          // Skip if the object does not have MetallicRoughnessMaterial.
          if (!material) {
            std::cout << "Unknown material. Skipping submesh." << std::endl;
            continue;
          }

          // Convert object data into GPU compatible format.
          unorderedObjectData.emplace_back();
          GPUObjectData& objectData = unorderedObjectData.back();
          objectData.worldMatrix = worldMatrix;
          objectData.worldMatrix = glm::inverse(objectData.worldMatrix);
          objectData.baseColorFactor = material->baseColorFactor();
          objectData.emissionFactor = material->emissiveFactor();
          objectData.metallicFactor = material->metallicFactor();
          objectData.roughnessFactor = material->roughnessFactor();
          objectData.bvhOffset = meshBVHNodes_.size();
          objectData.verticesOffset = vertices_.size();

          auto positionAccessor = submesh->geometry()->getTrianglePositionAccessor();
          auto normalAccessor = submesh->geometry()->getTriangleNormalAccessor();

          // Build triangles BVH nodes.
          lsg::bvh::SplitBVHBuilder<float> builder;
          auto bvh = builder.process(submesh->geometry()->getTrianglePositionAccessor());
          meshBVHNodes_.insert(meshBVHNodes_.end(), bvh->getNodes().begin(), bvh->getNodes().end());

          // Convert vertices into GPU compatible format (interleave).
          for (uint32_t idx : bvh->getPrimitiveIndices()) {
            lsg::Triangle<glm::vec3> posTri = (*positionAccessor)[idx];
            lsg::Triangle<glm::vec3> normalTri = (*normalAccessor)[idx];

            vertices_.emplace_back(posTri.a(), normalTri.a());
            vertices_.emplace_back(posTri.b(), normalTri.b());
            vertices_.emplace_back(posTri.c(), normalTri.c());
          }

          objectAABBs.emplace_back(bvh->getBounds());
        }
        std::cout << " Finished." << std::endl;
      }

      return true;
    });
  }

  std::cout << "Building scene BVH..." << std::flush;

  // Build objects BVH nodes.
  lsg::bvh::BVHBuilder<float> builder;
  auto bvh = builder.process(objectAABBs);
  objectBVHNodes_ = bvh->getNodes();

  for (uint32_t idx : bvh->getPrimitiveIndices()) {
    objectData_.emplace_back(unorderedObjectData[idx]);
  }

  std::cout << " Finished." << std::endl;
}

void SceneGPUConverter::clear() {
  scene_ = nullptr;
  cameras_.clear();
  objectData_.clear();
  objectBVHNodes_.clear();
  vertices_.clear();
  meshBVHNodes_.clear();
}

const lsg::Ref<lsg::Scene>& SceneGPUConverter::getScene() const {
  return scene_;
}

const std::vector<lsg::Ref<lsg::Object>>& SceneGPUConverter::getCameras() const {
  return cameras_;
}

const std::vector<GPUObjectData>& SceneGPUConverter::getObjectData() const {
  return objectData_;
}

const std::vector<lsg::BVH<float>::Node>& SceneGPUConverter::getObjectBvhNodes() const {
  return objectBVHNodes_;
}

const std::vector<GPUVertex>& SceneGPUConverter::getVertices() const {
  return vertices_;
}

const std::vector<lsg::BVH<float>::Node>& SceneGPUConverter::getMeshBvhNodes() const {
  return meshBVHNodes_;
}
