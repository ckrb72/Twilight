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

    struct VulkanQueue
    {
        VkQueue queue;
        uint32_t family;
    };

    VulkanQueue graphics_queue;
};

/* Init */
VulkanContext init_vulkan(void* win, uint32_t width, uint32_t height);
bool validate_vulkan(const VulkanContext& context);
void destroy_vulkan(VulkanContext& context);
void recreate_swapchain(VulkanContext& context, uint32_t width, uint32_t height);

/* Object Creation */


/* Rendering */
void vulkan_begin();
void vulkan_end();

/* Object Creation */
//VulkanPipeline create_pipeline();


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