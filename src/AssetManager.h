#pragma once
#include <string>
#include <assimp/Importer.hpp>      // C++ importer interface
#include <assimp/scene.h>           // Output data structure
#include <assimp/postprocess.h>     // Post processing flags

#include <glm/glm.hpp>
#include "vulkan_backend.h"
#include "twilight_types.h"
#include "Renderer.h"

struct Vertex
{
    glm::vec3 pos;
    glm::vec3 norm;
    glm::vec2 tex;
};

struct Mesh
{
    VulkanBuffer vertices;
    VulkanBuffer indices;
    uint32_t index_count;
    uint32_t material_index;
};

// Basic node for the scene graph
// Meshes, lights, cameras, etc. will all be SceneNodes
struct SceneNode
{
    std::vector<Mesh> meshes;
    std::vector<SceneNode> children;
    glm::mat4 local_transform;
};

class AssetManager
{
    private:
        Assimp::Importer importer;
        VulkanContext* temp_renderer = nullptr;      /* later on when we have a renderer class change this to a pointer to the actual renderer (i.e. const Renderer::Renderer)
                                                   Then in the load_model function instead of calling vulkan_create_buffer directly just call renderer.create_buffer(data, ...); */
        Twilight::Render::Renderer* renderer = nullptr;

        SceneNode load_node(aiNode* node, const aiScene* scene);
        void load_vertices(const aiMesh* mesh, std::vector<Vertex>& vertices);
        void load_indices(const aiMesh* mesh, std::vector<unsigned int>& indices);
        uint32_t load_materials(const aiScene* scene);
        glm::mat4 mat4x4_assimp_to_glm(const aiMatrix4x4& mat);

    public:
        AssetManager();
        ~AssetManager();
        void init(Twilight::Render::Renderer* renderer, VulkanContext* context);
        SceneNode load_model(const std::string& path);
};