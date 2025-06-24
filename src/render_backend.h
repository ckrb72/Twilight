#pragma once
#include "twilight_types.h"

namespace Twilight
{
    namespace Render
    {
        struct TransferInfo
        {
            VkCommandBuffer cmd;
            VkQueue queue;
            VkFence fence;
        };

        struct ImageTransitionInfo
        {
            VkAccessFlags2 src_access;
            VkPipelineStageFlags2 src_stage;
            VkAccessFlags2 dst_access;
            VkPipelineStageFlags2 dst_stage;
            VkImageLayout old_layout;
            VkImageLayout new_layout;
        };

        Buffer create_buffer(VmaAllocator allocator, uint64_t size, VkBufferUsageFlags usage, VmaMemoryUsage alloc_usage);

        /* TODO: Make this more efficient with the staging buffer */
        Buffer create_buffer(VkDevice device, const TransferInfo& info, VmaAllocator allocator, void* data, uint64_t size, VkBufferUsageFlags usage);

        Image create_image(VkDevice device, VmaAllocator allocator, VkExtent3D size, VkFormat format, VkImageUsageFlags usage);
        Image create_image(VkDevice device, VmaAllocator allocator, const TransferInfo& info, void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage);
        void transfer_begin(VkDevice device, const TransferInfo& info);
        void transfer_end(VkDevice device, const TransferInfo& info);

        void cmd_transition_image(VkCommandBuffer cmd, VkImage image, const ImageTransitionInfo& info, VkImageSubresourceRange sub_image);
    }
}