#pragma once
#include <vulkan/vulkan.h>
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

        struct Material
        {
            GraphicsPipeline* pipeline;
            VkDescriptorSet descriptor_set;
            Buffer descriptor_buffer;     // This will be uploaded with all the data needed at startup / when the material is loaded from disk
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
    }
}