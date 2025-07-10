#pragma once
#include <string>
#include <assimp/Importer.hpp>      // C++ importer interface
#include <assimp/scene.h>           // Output data structure
#include <assimp/postprocess.h>     // Post processing flags

#include <glm/glm.hpp>
#include "twilight_types.h"
#include "render/Renderer.h"
#include "Scene.h"

namespace Twilight
{

    class AssetManager
    {
        private:
            Assimp::Importer importer;
            Render::Renderer* renderer = nullptr;

            std::shared_ptr<SceneNode> load_node(aiNode* node, const aiScene* scene, const std::vector<uint32_t>& material_offsets, const glm::mat4& parent_transform, std::shared_ptr<SceneNode> parent);
            void load_vertices(const aiMesh* mesh, std::vector<Render::Vertex>& vertices);
            void load_indices(const aiMesh* mesh, std::vector<unsigned int>& indices);
            std::vector<uint32_t> load_materials(const aiScene* scene);
            glm::mat4 mat4x4_assimp_to_glm(const aiMatrix4x4& mat);

        public:
            AssetManager();
            ~AssetManager();
            void init(Render::Renderer* renderer);
            std::shared_ptr<SceneNode> load_model(const std::string& path);
    };

}