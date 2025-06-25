#pragma once
#include <vector>
#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include "vma.h"

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
            Buffer buffer;     // This will be uploaded with all the data needed at startup / when the material is loaded from disk
            Image texture;
        };

        enum MaterialTextureType : uint8_t
        {
            DIFFUSE,
            NORMAL,
            METALLIC,
            AMBIENT_OCCLUSION,
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
    }
}