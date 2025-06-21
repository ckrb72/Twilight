#include "AssetManager.h"
#include <assert.h>

#include <vector>
#include <iostream>
#include "vulkan_backend.h"

AssetManager::AssetManager()
:importer(), renderer()
{

}

AssetManager::~AssetManager()
{

}

void AssetManager::init(VulkanContext* context)
{
    this->renderer = context;
}

SceneNode AssetManager::load_model(const std::string& path)
{
    const aiScene* scene = importer.ReadFile(path, aiProcess_CalcTangentSpace | aiProcess_Triangulate | aiProcess_JoinIdenticalVertices);
    
    if(scene == nullptr)
    {
        std::cout << "Failed to load model: " << path << std::endl;
        return {};
    }

    // Maybe just save all materials in some "global" array that is a private member of the AssetManager
    // Probably actually want to save the materials (or at least a pointer of them) in the Renderer class
    /*std::vector<Material> materials =  load_materials(scene); */

    // load_lights()
    // load_cameras()

    SceneNode root = load_node(scene->mRootNode, scene);

    return root;
}

SceneNode AssetManager::load_node(aiNode* node, const aiScene* scene)
{
    SceneNode scene_node = {};
    scene_node.meshes.reserve(node->mNumMeshes);
    scene_node.children.reserve(node->mNumChildren);

    for(int mesh_idx = 0; mesh_idx < node->mNumMeshes; mesh_idx++)
    {
        aiMesh* mesh = scene->mMeshes[node->mMeshes[mesh_idx]];

        std::vector<Vertex> vertices = {};
        if(mesh->mNumVertices > 0)
        {
            vertices.reserve(mesh->mNumVertices);
            load_vertices(mesh, vertices);
        }

        std::vector<unsigned int> indices = {};
        if(mesh->mNumFaces > 0)
        {
            load_indices(mesh, indices);
        }

        // FIXME: hacky way to do this for now
        assert(renderer != nullptr);

        Mesh node_mesh = {
            .vertices = vulkan_create_buffer(*renderer, vertices.size() * sizeof(Vertex), vertices.data(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT),
            .indices = vulkan_create_buffer(*renderer, indices.size() * sizeof(unsigned int), indices.data(), VK_BUFFER_USAGE_INDEX_BUFFER_BIT),
            .index_count = static_cast<uint32_t>(indices.size()),
            .material_index = mesh->mMaterialIndex  /* Will probably need to change this up depending on how I store materials in the AssetManager*/
        };

        scene_node.meshes.push_back(node_mesh);
    }

    for(int child_idx = 0; child_idx < node->mNumChildren; child_idx++)
    {
        scene_node.children.push_back(load_node(node->mChildren[child_idx], scene));
    }

    return scene_node;
}

void AssetManager::load_vertices(const aiMesh* mesh, std::vector<Vertex>& out_vertices)
{
    for(int vert_idx = 0; vert_idx < mesh->mNumVertices; vert_idx++)
    {
        Vertex vertex = {};

        if(mesh->HasPositions())
        {
            vertex.pos.x = mesh->mVertices[vert_idx].x;
            vertex.pos.y = mesh->mVertices[vert_idx].y;
            vertex.pos.z = mesh->mVertices[vert_idx].z;
        }

        if(mesh->HasNormals())
        {
            vertex.norm.x = mesh->mNormals[vert_idx].x;
            vertex.norm.y = mesh->mNormals[vert_idx].y;
            vertex.norm.z = mesh->mNormals[vert_idx].z;
        }

        if(mesh->HasTextureCoords(0))
        {
            vertex.tex.x = mesh->mTextureCoords[0][vert_idx].x;
            vertex.tex.y = mesh->mTextureCoords[0][vert_idx].y;
        }
        out_vertices.push_back(vertex);
    }
}

void AssetManager::load_indices(const aiMesh* mesh, std::vector<unsigned int>& out_indices)
{
    for(int face_idx = 0; face_idx < mesh->mNumFaces; face_idx++)
    {
        aiFace* face = &mesh->mFaces[face_idx];
        for(int index = 0; index < face->mNumIndices; index++)
        {
            out_indices.push_back(face->mIndices[index]);
        }
    }
}

// Probably want to do this at a scene level rather than per mesh (so only take in the scene and just load all materials when loading scene. Then just save the material index for each mesh)
void AssetManager::load_material(const aiMesh* mesh, const aiScene* scene)
{
    aiMaterial* material = scene->mMaterials[mesh->mMaterialIndex];
    //std::cout << "Material Name: " << material->GetName().C_Str() << std::endl;

    for(int i = 0; i < material->mNumProperties; i++)
    {
        aiMaterialProperty* property = material->mProperties[i];
        //std::cout << property->mKey.C_Str() << std::endl;
        // Parse Data and Stuff...
        if(property->mType == aiPTI_Float)
        {

        }
    }
}