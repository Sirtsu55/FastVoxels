#pragma once

#include <iostream>
#include <glm/glm.hpp>
#include <vulkan/vulkan.hpp>
#include <tiny_gltf.h>
#include "Camera.h"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_major_storage.hpp>
#include "voxelizer.h"

struct Mesh
{
    std::vector<uint32_t> GeometryReferences;

    // transform is applied to all geometries in the mesh
    // stored in row major order, similar to VkTransformMatrixKHR
    glm::mat3x4 Transform = glm::mat3x4(1.0f); 
};

struct Material
{
    glm::vec4 BaseColorFactor = glm::vec4(1.0f);
    float MetallicFactor = 1.0f;
    float RoughnessFactor = 1.0f;
    glm::vec3 EmissiveFactor = glm::vec3(0.0f);

};

struct VoxelGeometry
{
    std::vector<glm::vec3> Positions;

    Material Material;
};

struct VoxelScene
{
    std::vector<VoxelGeometry> Geometries;

    std::vector<Mesh> Meshes;

    std::vector<Camera> Cameras;

    float VoxelSize = 1.0f;

    float Resolution = 1.0f;
};

class MeshLoader
{
public:
    VoxelScene LoadVoxelizedGLBScene(const std::string& path, float size = 1.0f, float resolution = 1.0f);

private:


    void AddMeshToScene(const tinygltf::Mesh& mesh, tinygltf::Model& model, VoxelScene& outScene);

    tinygltf::TinyGLTF mLoader;
};

