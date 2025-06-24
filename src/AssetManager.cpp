#include "AssetManager.h"
#include <assert.h>

#include <vector>
#include <iostream>
#include "vulkan_backend.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

AssetManager::AssetManager()
:importer(), temp_renderer()
{
    stbi_set_flip_vertically_on_load(true);
}

AssetManager::~AssetManager()
{

}

void AssetManager::init(Twilight::Render::Renderer* renderer, VulkanContext* context)
{
    this->renderer = renderer;
    this->temp_renderer = context;
}

SceneNode AssetManager::load_model(const std::string& path)
{
    // FIXME: hacky way to do this for now
    assert(temp_renderer != nullptr);

    const aiScene* scene = importer.ReadFile(path, aiProcess_CalcTangentSpace | aiProcess_Triangulate | aiProcess_JoinIdenticalVertices);
    
    if(scene == nullptr)
    {
        std::cout << "Failed to load model: " << path << std::endl;
        return {};
    }

    // Maybe just save all materials in some "global" array that is a private member of the AssetManager
    // Probably actually want to save the materials (or at least a pointer of them) in the Renderer class
    
        uint32_t material_offset = load_materials(scene); 
    /*   
        for(const Material& material : materials)
        {
            renderer.register_material(material);
        }
    */

    // load_lights();
    // load_cameras();

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

        Mesh node_mesh = {
            .vertices = vulkan_create_buffer(*temp_renderer, vertices.size() * sizeof(Vertex), vertices.data(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT),
            .indices = vulkan_create_buffer(*temp_renderer, indices.size() * sizeof(unsigned int), indices.data(), VK_BUFFER_USAGE_INDEX_BUFFER_BIT),
            .index_count = static_cast<uint32_t>(indices.size()),
            .material_index = mesh->mMaterialIndex  /* Will probably need to change this up depending on how I store materials in the AssetManager*/
        };

        scene_node.meshes.push_back(node_mesh);
    }

    for(int child_idx = 0; child_idx < node->mNumChildren; child_idx++)
    {
        scene_node.children.push_back(load_node(node->mChildren[child_idx], scene));
    }
    scene_node.local_transform = mat4x4_assimp_to_glm(node->mTransformation);

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

// Loads materials from a given scene. The materials are referenced by meshes via their material index (i.e. mesh0 might have material at index 0 and mesh1 might have material at index 2)
// returns the offset in the renderer's global material list. Add offset to the loaded mesh's material index to get the global index
// i.e. If mesh0 has a material index of 2 and load_materials returns 3 then the global offset (the number that should be stored in the mesh's material_index member) should be 5
uint32_t AssetManager::load_materials(const aiScene* scene)
{
    std::cout << "Material Count: " << scene->mNumMaterials << std::endl;
    for(int mat_index = 0; mat_index < scene->mNumMaterials; mat_index++)
    {
        aiMaterial* material = scene->mMaterials[mat_index];
        
        std::cout << "Diffuse: " << material->GetTextureCount(aiTextureType_DIFFUSE) << std::endl;

        aiString tex_path;
        material->GetTexture(aiTextureType_DIFFUSE, 0, &tex_path);
        const aiTexture* texture = scene->GetEmbeddedTexture(tex_path.C_Str());
        if(texture != nullptr)
        {
            int width, height;
            unsigned char* image_data = stbi_load_from_memory((unsigned char*)texture->pcData, texture->mWidth, &width, &height, nullptr, 4);

            //renderer->load_material({{image_data, static_cast<uint32_t>(width), static_cast<uint32_t>(height), Twilight::Render::MaterialTextureType::DIFFUSE}});

            stbi_image_free(image_data);
        }
    }

    return 0;
}

glm::mat4 AssetManager::mat4x4_assimp_to_glm(const aiMatrix4x4& mat)
{
    // Transpose assimp matrix
    glm::mat4 out_mat;
    for(int r = 0; r < 4; r++)
    {
        for(int c = 0; c < 4; c++)
        {
            out_mat[r][c] = mat[c][r];
        }
    }
    return out_mat;
}