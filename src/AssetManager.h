#pragma once
#include <string>
#include <assimp/Importer.hpp>      // C++ importer interface
#include <assimp/scene.h>           // Output data structure
#include <assimp/postprocess.h>     // Post processing flags

#include <glm/glm.hpp>
#include "twilight_types.h"
#include "Renderer.h"

struct Vertex
{
    glm::vec3 pos;
    glm::vec3 norm;
    glm::vec2 tex;
};


class AssetManager
{
    private:
        Assimp::Importer importer;
        Twilight::Render::Renderer* renderer = nullptr;

        Twilight::Render::Model load_node(aiNode* node, const aiScene* scene);
        void load_vertices(const aiMesh* mesh, std::vector<Vertex>& vertices);
        void load_indices(const aiMesh* mesh, std::vector<unsigned int>& indices);
        uint32_t load_materials(const aiScene* scene);
        glm::mat4 mat4x4_assimp_to_glm(const aiMatrix4x4& mat);

    public:
        AssetManager();
        ~AssetManager();
        void init(Twilight::Render::Renderer* renderer);
        Twilight::Render::Model load_model(const std::string& path);
};