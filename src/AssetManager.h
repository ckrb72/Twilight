#pragma once
#include <string>
#include <assimp/Importer.hpp>      // C++ importer interface
#include <assimp/scene.h>           // Output data structure
#include <assimp/postprocess.h>     // Post processing flags

#include <glm/glm.hpp>
#include "twilight_types.h"
#include "render/Renderer.h"

namespace Twilight
{

    class AssetManager
    {
        private:
            Assimp::Importer importer;
            Twilight::Render::Renderer* renderer = nullptr;

            Twilight::Render::Model load_node(aiNode* node, const aiScene* scene, const std::vector<uint32_t>& material_offsets);
            void load_vertices(const aiMesh* mesh, std::vector<Render::Vertex>& vertices);
            void load_indices(const aiMesh* mesh, std::vector<unsigned int>& indices);
            std::vector<uint32_t> load_materials(const aiScene* scene);
            glm::mat4 mat4x4_assimp_to_glm(const aiMatrix4x4& mat);

        public:
            AssetManager();
            ~AssetManager();
            void init(Twilight::Render::Renderer* renderer);
            Twilight::Render::Model load_model(const std::string& path);
    };

}