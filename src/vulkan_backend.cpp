#include "vulkan_backend.h"
#include <VkBootstrap.h>
#include <GLFW/glfw3.h>
#include "vma.h"

static void create_swapchain(VulkanContext& context, uint32_t width, uint32_t height);
static void destroy_swapchain(VulkanContext& context);

VulkanContext init_vulkan(void* win, uint32_t width, uint32_t height)
{
    vkb::InstanceBuilder instance_builder;
    vkb::Instance instance = instance_builder.set_app_name("renderer")
                                              .request_validation_layers(true)
                                              .add_validation_feature_enable(VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT)
                                              .use_default_debug_messenger()
                                              .require_api_version(1, 3, 0)
                                              .build()
                                              .value();
    
    VkSurfaceKHR surface;
    VK_CHECK(glfwCreateWindowSurface(instance.instance, (GLFWwindow*)win, nullptr, &surface));

    VkPhysicalDeviceVulkan13Features features13 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
        .synchronization2 = true,
        .dynamicRendering = true
    };

    VkPhysicalDeviceVulkan12Features features12 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
        .descriptorIndexing = true,
        .bufferDeviceAddress = true
    };
    
    vkb::PhysicalDeviceSelector selector(instance);
    auto gpu = selector.set_minimum_version(1, 3)
                                      .set_required_features_13(features13)
                                      .set_required_features_12(features12)
                                      .set_surface(surface)
                                      .select()
                                      .value();
    
    vkb::DeviceBuilder device_builder(gpu);
    vkb::Device device = device_builder.build().value();

    VmaAllocatorCreateInfo allocator_info = {
        .flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT,
        .physicalDevice = gpu,
        .device = device,
        .instance = instance.instance
    };

    VmaAllocator allocator;
    VK_CHECK(vmaCreateAllocator(&allocator_info, &allocator));

    VulkanContext context = {
        .instance = instance.instance,
        .device = device.device,
        .physical_device = gpu.physical_device,
        .surface = surface,
        .debug_messenger = instance.debug_messenger,
        .allocator = allocator,
        .graphics_queue = {
            .queue = device.get_queue(vkb::QueueType::graphics).value(),
            .family = device.get_queue_index(vkb::QueueType::graphics).value(),
        }
    };

    create_swapchain(context, width, height);


    VkCommandPoolCreateInfo cmd_pool_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = context.graphics_queue.family
    };

    VkSemaphoreCreateInfo semaphore_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
    };

    VkFenceCreateInfo fence_info = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT
    };


    for(int i = 0; i < FLIGHT_COUNT; i++)
    {
        VK_CHECK(vkCreateCommandPool(context.device, &cmd_pool_info, nullptr, &context.frames[i].cmd_pool));

        VkCommandBufferAllocateInfo cmd_buf_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
            .commandPool = context.frames[i].cmd_pool,
            .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
            .commandBufferCount = 1
        };

        VK_CHECK(vkAllocateCommandBuffers(context.device, &cmd_buf_info, &context.frames[i].cmd_buffer));
        VK_CHECK(vkCreateSemaphore(context.device, &semaphore_info, nullptr, &context.frames[i].swapchain_semaphore));
        VK_CHECK(vkCreateSemaphore(context.device, &semaphore_info, nullptr, &context.frames[i].render_semaphore));
        VK_CHECK(vkCreateFence(context.device, &fence_info, nullptr, &context.frames[i].render_fence));
    }

    return context;
}

bool validate_vulkan(const VulkanContext& context)
{
    return context.instance != VK_NULL_HANDLE && context.device != VK_NULL_HANDLE 
           && context.surface != VK_NULL_HANDLE && context.swapchain.swapchain != VK_NULL_HANDLE;
}


void destroy_vulkan(VulkanContext& context)
{

    for(int i = 0; i < FLIGHT_COUNT; i++)
    {
        vkDestroyCommandPool(context.device, context.frames[i].cmd_pool, nullptr);
        vkDestroySemaphore(context.device, context.frames[i].render_semaphore, nullptr);
        vkDestroySemaphore(context.device, context.frames[i].swapchain_semaphore, nullptr);
        vkDestroyFence(context.device, context.frames[i].render_fence, nullptr);
    }

    vmaDestroyAllocator(context.allocator);

    destroy_swapchain(context);
    vkDestroySurfaceKHR(context.instance, context.surface, nullptr);
    vkDestroyDevice(context.device, nullptr);
    vkb::destroy_debug_utils_messenger(context.instance, context.debug_messenger, nullptr);
    vkDestroyInstance(context.instance, nullptr);
}

void recreate_swapchain(VulkanContext& context, uint32_t width, uint32_t height)
{
    destroy_swapchain(context);
    create_swapchain(context, width, height);
}

static void create_swapchain(VulkanContext& context, uint32_t width, uint32_t height)
{
    // Shader will output linear space colors and then vulkan will automatically convert the output
    // to sRGB when writing the colors to the swapchain color attachment so when it is displayed it is
    // then back in linear space because the monitor automatically applies the gamma to it.
    context.swapchain.format = VK_FORMAT_B8G8R8A8_SRGB;

    vkb::SwapchainBuilder swapchain_builder(context.physical_device, context.device, context.surface);
    VkSurfaceFormatKHR surface_format = {
        .format = context.swapchain.format,
        .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
    };

    vkb::Swapchain vkb_swapchain = swapchain_builder.set_desired_format(surface_format)
                                                    .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
                                                    .set_desired_extent(width, height)
                                                    .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
                                                    .build()
                                                    .value();
    
    context.swapchain.extent = vkb_swapchain.extent;
    context.swapchain.swapchain = vkb_swapchain.swapchain;
    context.swapchain.images = vkb_swapchain.get_images().value();
    context.swapchain.views = vkb_swapchain.get_image_views().value();
}


static void destroy_swapchain(VulkanContext& context)
{
    vkDestroySwapchainKHR(context.device, context.swapchain.swapchain, nullptr);

    for(const VkImageView& view : context.swapchain.views)
    {
        vkDestroyImageView(context.device, view, nullptr);
    }
}