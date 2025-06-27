#pragma once
#include <vector>
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include "render/vma.h"

namespace Twilight
{
    namespace Render
    {
        struct GraphicsPipeline
        {
            VkPipeline handle;
            VkPipelineLayout layout;
        };

        struct Buffer
        {
            VkBuffer handle;
            VmaAllocation allocation;
            VmaAllocationInfo info;
        };

        struct Image
        {
            VkImage handle;
            VkImageView view;
            VmaAllocation allocation;
            VmaAllocationInfo info;
            VkFormat format;
            uint32_t width, height, depth;
        };

        struct Material
        {
            GraphicsPipeline* pipeline;
            VkDescriptorSet descriptor_set;
            Buffer buffer;
            Image texture;
        };

        enum MaterialTextureType : uint8_t
        {
            BASE_COLOR,
            NORMAL,
            METALLIC,
            AMBIENT_OCCLUSION,
        };

        enum MaterialConstantType : uint8_t
        {
            ALBEDO
        };

        struct MaterialConstantBinding
        {
            void* data;
            MaterialConstantType type;
        };

        struct MaterialTextureBinding
        {
            void* data;
            uint32_t width, height;
            MaterialTextureType type;
        };

        struct Vertex
        {
            glm::vec3 pos;
            glm::vec3 norm;
            glm::vec2 tex;
        };

        struct Mesh
        {
            Buffer vertices;
            Buffer indices;
            uint32_t index_count;
            uint32_t material_index;
        };
        
        struct Model
        {
            std::vector<Mesh> meshes;
            std::vector<Model> children;
            glm::mat4 local_transform;
        };

        struct Light
        {
            glm::vec3 pos;
            glm::vec3 color;
        };
    }

    struct SceneNode
    {
        Render::Model* model;
        Render::Light* light;
        std::vector<SceneNode> children;
        glm::mat4 transform;
    };
}