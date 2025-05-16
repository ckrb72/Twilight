#include "AssetLoader.h"

#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>

AssetLoader::AssetLoader()
{

}

AssetLoader::~AssetLoader()
{

}

id_t AssetLoader::load_texture(const std::string& path, bool mipmapped)
{
    return -1;
}


id_t AssetLoader::load_model(const std::string& path)
{
    const aiScene* scene = m_importer.ReadFile(path, aiProcess_CalcTangentSpace | aiProcess_Triangulate | aiProcess_JoinIdenticalVertices);

    if(scene == nullptr)
    {
        std::cout << "Failed to load model: " << path << std::endl;
        return -1;
    }

    // Do importing stuff here

    m_importer.FreeScene();

    return -1;
}