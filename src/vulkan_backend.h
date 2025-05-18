#pragma once
#include <vulkan/vulkan.h>
#include <iostream>
#include <vulkan/vk_enum_string_helper.h>
#include <vector>
#include "vma.h"

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

struct VulkanFrameData
{
    VkSemaphore swapchain_semaphore, render_semaphore;
    VkFence render_fence;
    VkCommandPool cmd_pool;
    VkCommandBuffer cmd_buffer;
};

struct VulkanContext
{
    VkInstance instance;
    VkDevice device;
    VkPhysicalDevice physical_device;
    VkSurfaceKHR surface;
    VkDebugUtilsMessengerEXT debug_messenger;
    VulkanSwapchain swapchain;
    VmaAllocator allocator;
    VulkanFrameData frames[FLIGHT_COUNT];
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
    VkBuffer buffer;
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
    VkImage image;
    VkImageView view;
    VmaAllocation allocation;
    VmaAllocationInfo info;
    VkFormat format;
    uint32_t width, height, depth;
};

struct VulkanMesh
{
    VulkanBuffer vert_buf;
    VulkanBuffer indx_buf;
    std::vector<std::string> tex_id;
};

struct VulkanModel
{
    std::vector<VulkanMesh> meshes;
    std::vector<VulkanModel> children;
};

/* Init */
VulkanContext init_vulkan(void* win, uint32_t width, uint32_t height);
bool validate_vulkan(const VulkanContext& context);
void destroy_vulkan(VulkanContext& context);
void recreate_swapchain(VulkanContext& context, uint32_t width, uint32_t height);

/* Object Creation */
VulkanBuffer vulkan_create_buffer(const VulkanContext& context, uint64_t size, VkBufferUsageFlags usage, VmaMemoryUsage alloc_usage);
void vulkan_destroy_buffer(const VulkanContext& context, VulkanBuffer& buffer);
VulkanImage vulkan_create_image(const VulkanContext& context, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped);
VulkanImage vulkan_create_image(const VulkanContext& context, void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped);
void vulkan_destroy_image(const VulkanContext& context, VulkanImage& image);

/* Rendering */
void vulkan_immediate_begin(const VulkanContext& context);
void vulkan_immediate_end(const VulkanContext& context);
void vulkan_frame_begin();
void vulkan_frame_end();

/* Misc. */
void vulkan_cmd_transition_image(VkCommandBuffer cmd, VkImage image, const VulkanImageTransitionInfo& info, VkImageSubresourceRange sub_image);

/*
class Twilight(or whatever name)
{
    VulkanContext context;
    TLWindow window;

    ~Twilight()
    {
        destroy_vulkan(context);
    }

    void init()
    {
        init_window("Name", Width, Height);

        init_vulkan(window);
    }

    void recreate_swapchain()
    {
        destroy_swapchain(context);
        create_swapchain(context, window.get_width(), window.get_height());
    }
}

*/