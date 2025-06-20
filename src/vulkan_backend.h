#pragma once
#include <vulkan/vulkan.h>
#include <iostream>
#include <vulkan/vk_enum_string_helper.h>
#include <vector>
#include "vma.h"
#include "DescriptorAllocator.h"

#define VK_CHECK(x)                                                     \
    do {                                                                \
        VkResult err = x;                                               \
        if (err) {                                                      \
            std::cout << #x << std::endl;                               \
            std::cout << "Detected Vulkan error: " << string_VkResult(err) << std::endl;\
            std::cout << __FILE__ << " " << __LINE__ << std::endl;      \
            exit(EXIT_FAILURE);                                                   \
        }                                                               \
    } while (0);

#define FLIGHT_COUNT 2

struct VulkanSwapchain
{
    VkFormat format;
    VkExtent2D extent;
    VkSwapchainKHR swapchain;
    std::vector<VkImage> images;
    std::vector<VkImageView> views;
};

struct VulkanFrameContext
{
    VkCommandBuffer cmd;
    DescriptorAllocator descriptor_allocator;
    uint32_t swapchain_index;
};

struct ContextInternalFrameData
{
    VkSemaphore swapchain_semaphore, render_semaphore;
    VkFence render_fence;
    VkCommandPool cmd_pool;
};

struct VulkanContext
{
    uint32_t current_frame;
    VkInstance instance;
    VkDevice device;
    VkPhysicalDevice physical_device;
    VkSurfaceKHR surface;
    VkDebugUtilsMessengerEXT debug_messenger;
    VulkanSwapchain swapchain;
    VmaAllocator allocator;
    ContextInternalFrameData frame_data[FLIGHT_COUNT];
    VulkanFrameContext frame_context[FLIGHT_COUNT];
    VkCommandPool immediate_pool;
    VkCommandBuffer immediate_buffer;
    VkFence immediate_fence;

    struct VulkanQueue
    {
        VkQueue queue;
        uint32_t family;
    };

    VulkanQueue graphics_queue;
};

struct VulkanBuffer
{
    VkBuffer handle;
    VmaAllocation allocation;
    VmaAllocationInfo info;
};

struct VulkanImageTransitionInfo
{
    VkAccessFlags2 src_access;
    VkPipelineStageFlags2 src_stage;
    VkAccessFlags2 dst_access;
    VkPipelineStageFlags2 dst_stage;
    VkImageLayout old_layout;
    VkImageLayout new_layout;
};

struct VulkanImage
{
    VkImage handle;
    VkImageView view;
    VmaAllocation allocation;
    VmaAllocationInfo info;
    VkFormat format;
    uint32_t width, height, depth;
};

struct VulkanGraphicsPipeline
{
    VkPipeline pipeline;
    VkPipelineLayout layout;
};

/* Init */
VulkanContext init_vulkan(void* win, uint32_t width, uint32_t height);
bool validate_vulkan(const VulkanContext& context);
void destroy_vulkan(VulkanContext& context);
void recreate_swapchain(VulkanContext& context, uint32_t width, uint32_t height);

/* Object Creation */
VulkanBuffer vulkan_create_buffer(const VulkanContext& context, uint64_t size, VkBufferUsageFlags usage, VmaMemoryUsage alloc_usage);
VulkanBuffer vulkan_create_buffer(const VulkanContext& context, uint64_t size, void* data, VkBufferUsageFlags usage);
void vulkan_destroy_buffer(const VulkanContext& context, VulkanBuffer& buffer);
VulkanImage vulkan_create_image(const VulkanContext& context, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped);
VulkanImage vulkan_create_image(const VulkanContext& context, void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped);
void vulkan_destroy_image(const VulkanContext& context, VulkanImage& image);
void vulkan_destroy_graphics_pipeline(const VulkanContext& context, VulkanGraphicsPipeline& pipeline);

/* Rendering */
void vulkan_immediate_begin(const VulkanContext& context);
void vulkan_immediate_end(const VulkanContext& context);
VulkanFrameContext vulkan_frame_begin(VulkanContext& context);
void vulkan_frame_end(VulkanContext& context);

/* Misc. */
void vulkan_cmd_transition_image(VkCommandBuffer cmd, VkImage image, const VulkanImageTransitionInfo& info, VkImageSubresourceRange sub_image);
void vulkan_cmd_copy_buffer_to_buffer(VkCommandBuffer cmd, uint64_t size, const VulkanBuffer& src, const VulkanBuffer dst);
void vulkan_cmd_copy_buffer_to_image(VkCommandBuffer cmd, const VulkanBuffer& src, const VulkanImage& dst);
bool vulkan_load_shader_module(const char* path, VkDevice device, VkShaderModule* out_module);