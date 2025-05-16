#pragma once
#include <string>
#include "vulkan_backend.h"
#include <assimp/Importer.hpp>

typedef int32_t id_t;

class AssetLoader
{
    private:

        std::vector<VulkanImage> m_textures;
        std::vector<VulkanModel> m_models;
        Assimp::Importer m_importer;

    public:
        AssetLoader();
        ~AssetLoader();

        id_t load_texture(const std::string& path, bool mipmapped);
        id_t load_model(const std::string& path);

        void clear();
};