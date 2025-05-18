#include "vulkan_backend.h"
#include <VkBootstrap.h>
#include <GLFW/glfw3.h>
#include "vma.h"

static void create_swapchain(VulkanContext& context, uint32_t width, uint32_t height);
static void destroy_swapchain(VulkanContext& context);
static void vulkan_cmd_create_mipmaps(const VulkanContext& context, const VulkanImage& image);

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

    VK_CHECK(vkCreateFence(context.device, &fence_info, nullptr, &context.immediate_fence));
    VK_CHECK(vkCreateCommandPool(context.device, &cmd_pool_info, nullptr, &context.immediate_pool));

    VkCommandBufferAllocateInfo cmd_buf_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = context.immediate_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };
    VK_CHECK(vkAllocateCommandBuffers(context.device, &cmd_buf_info, &context.immediate_buffer));

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
    
    vkDestroyCommandPool(context.device, context.immediate_pool, nullptr);
    vkDestroyFence(context.device, context.immediate_fence, nullptr);

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


VulkanBuffer vulkan_create_buffer(const VulkanContext& context, uint64_t size, VkBufferUsageFlags usage, VmaMemoryUsage alloc_usage)
{
    VkBufferCreateInfo buf_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = usage
    };

    VmaAllocationCreateInfo alloc_info = {
        .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT,
        .usage = alloc_usage
    };

    VulkanBuffer buffer = {};
    VK_CHECK(vmaCreateBuffer(context.allocator, &buf_info, &alloc_info, &buffer.buffer, &buffer.allocation, &buffer.info));
    
    return buffer;
}

void vulkan_destroy_buffer(const VulkanContext& context, VulkanBuffer& buffer)
{
    vmaDestroyBuffer(context.allocator, buffer.buffer, buffer.allocation);

    buffer.allocation = nullptr;
    buffer.buffer = VK_NULL_HANDLE;
    buffer.info = {};
}

VulkanImage vulkan_create_image(const VulkanContext& context, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped)
{
    VkImageCreateInfo image_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .format = format,
        .extent = size,
        .mipLevels = 1,
        .arrayLayers = 1,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .usage = usage,
    };

    if(mipmapped)
    {
        image_info.mipLevels = (uint32_t)(std::floor(std::log2(std::max(size.width, size.height))) + 1);
        image_info.usage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    }

    VmaAllocationCreateInfo alloc_info = {
        .usage = VMA_MEMORY_USAGE_GPU_ONLY,
        .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
    };

    VulkanImage image = { .format = format, .width = size.width, .height = size.height, .depth = size.height };

    VK_CHECK(vmaCreateImage(context.allocator, &image_info, &alloc_info, &image.image, &image.allocation, &image.info));

    VkImageViewCreateInfo view_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = image.image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = format
    };

    VkImageAspectFlags aspect = (usage == VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;

    view_info.subresourceRange = {
        .aspectMask = aspect,
        .baseMipLevel = 0,
        .levelCount = image_info.mipLevels,
        .baseArrayLayer = 0,
        .layerCount = 1
    };

    VK_CHECK(vkCreateImageView(context.device, &view_info, nullptr, &image.view));

    return image;
}

VulkanImage vulkan_create_image(const VulkanContext& context, void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage, bool mipmapped)
{
    uint64_t data_size = size.width * size.height * size.depth * 4;
    VulkanBuffer data_buffer = vulkan_create_buffer(context, data_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
    void* buffer_ptr = data_buffer.info.pMappedData;
    memcpy(buffer_ptr, data, data_size);

    VulkanImage image = vulkan_create_image(context, size, format, usage, mipmapped);

    vulkan_immediate_begin(context);

    VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT;

    // Transition base mip level to be ready for transfer

    VulkanImageTransitionInfo transition_info = {
        .src_access = VK_ACCESS_2_NONE,
        .src_stage = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        .dst_access = VK_ACCESS_2_TRANSFER_WRITE_BIT,
        .dst_stage = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        .old_layout = VK_IMAGE_LAYOUT_UNDEFINED,
        .new_layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
    };

    vulkan_cmd_transition_image(context.immediate_buffer, image.image, transition_info, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });


    VkBufferImageCopy copy_info = {
        .imageExtent = size,
    };

    copy_info.imageSubresource = {
        .aspectMask = aspect,
        .mipLevel = 0,
        .baseArrayLayer = 0,
        .layerCount = 1
    };

    vkCmdCopyBufferToImage(context.immediate_buffer, data_buffer.buffer, image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy_info);

    if(mipmapped)
    {
        vulkan_cmd_create_mipmaps(context, image);
    }

    VulkanImageTransitionInfo shader_transition_info = {
        .src_access = VK_ACCESS_2_TRANSFER_WRITE_BIT,
        .src_stage = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        .dst_access = VK_ACCESS_2_SHADER_READ_BIT,
        .dst_stage = VK_PIPELINE_STAGE_2_FRAGMENT_SHADER_BIT,
        .old_layout = VK_IMAGE_LAYOUT_UNDEFINED,
        .new_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    };

    vulkan_cmd_transition_image(context.immediate_buffer, image.image, shader_transition_info, { VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS });

    vulkan_immediate_end(context);

    vmaDestroyBuffer(context.allocator, data_buffer.buffer, data_buffer.allocation);

    return image;
}

static void vulkan_cmd_create_mipmaps(const VulkanContext& context, const VulkanImage& image)
{
    uint32_t mip_levels = (uint32_t)(std::floor(std::log2(std::max(image.width, image.height))) + 1);

    // Transfer to VK_IMAGE_LAYOUT_TRANSFER_SRC
    VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT;
        
    // Only transition the base mip level
    VulkanImageTransitionInfo transition_info = {
        .src_access = VK_ACCESS_2_TRANSFER_WRITE_BIT,
        .src_stage = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        .dst_access = VK_ACCESS_2_TRANSFER_READ_BIT,
        .dst_stage = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
        .old_layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .new_layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
    };

    vulkan_cmd_transition_image(context.immediate_buffer, image.image, transition_info, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });

    // Blit down successive mip levels
    for(uint32_t i = 1; i < mip_levels; i++)
    {
        // Transition mip level we are bliting to transfer_dst_optimal
        VulkanImageTransitionInfo miplevel_transition_info = {
            .src_access = VK_ACCESS_2_NONE,
            .src_stage = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            .dst_access =  VK_ACCESS_2_TRANSFER_WRITE_BIT,
            .dst_stage = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            .old_layout = VK_IMAGE_LAYOUT_UNDEFINED,
            .new_layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        };

        vulkan_cmd_transition_image(context.immediate_buffer, image.image, miplevel_transition_info, { VK_IMAGE_ASPECT_COLOR_BIT, i, 1, 0, 1 });

        // Blit level n - 1 to level n
        VkImageBlit blit = {};
        blit.srcOffsets[1] = { (int32_t)(image.width >> (i - 1)), (int32_t)(image.height >> (i - 1)), 1 };
        blit.srcSubresource = {
            .aspectMask = aspect,
            .mipLevel = i - 1,
            .layerCount = 1,
        };
        blit.dstOffsets[1] = { (int32_t)(image.width >> i), (int32_t)(image.height >> i), 1 };
        blit.dstSubresource = {
            .aspectMask = aspect,
            .mipLevel = i,
            .layerCount = 1
        };

        vkCmdBlitImage(context.immediate_buffer, image.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, image.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_LINEAR);

        // Transition level n to transfer_src_optimal for next loop
        VulkanImageTransitionInfo miplevel_src_transition_info = {
            .src_access = VK_ACCESS_2_TRANSFER_WRITE_BIT,
            .src_stage = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            .dst_access =  VK_ACCESS_2_TRANSFER_READ_BIT,
            .dst_stage = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
            .old_layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            .new_layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
        };

        vulkan_cmd_transition_image(context.immediate_buffer, image.image, miplevel_src_transition_info, { VK_IMAGE_ASPECT_COLOR_BIT, i, 1, 0, 1 });
    }
}

void vulkan_destroy_image(const VulkanContext& context, VulkanImage& image)
{
    vkDestroyImageView(context.device, image.view, nullptr);
    vmaDestroyImage(context.allocator, image.image, image.allocation);

    image.image = VK_NULL_HANDLE;
    image.view = VK_NULL_HANDLE;
    image.allocation = nullptr;
    image.info = {};
}

void vulkan_immediate_begin(const VulkanContext& context)
{

    VK_CHECK(vkResetFences(context.device, 1, &context.immediate_fence));
    VK_CHECK(vkResetCommandBuffer(context.immediate_buffer, 0));

    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };

    VK_CHECK(vkBeginCommandBuffer(context.immediate_buffer, &begin_info));
}

void vulkan_immediate_end(const VulkanContext& context)
{
    VK_CHECK(vkEndCommandBuffer(context.immediate_buffer));

    VkCommandBufferSubmitInfo buffer_submit_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
        .commandBuffer = context.immediate_buffer,
        .deviceMask = 0
    };

    VkSubmitInfo2 submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
        .commandBufferInfoCount = 1,
        .pCommandBufferInfos = &buffer_submit_info
    };

    VK_CHECK(vkQueueSubmit2(context.graphics_queue.queue, 1, &submit_info, context.immediate_fence));
    VK_CHECK(vkWaitForFences(context.device, 1, &context.immediate_fence, true, UINT64_MAX));
}

void vulkan_cmd_transition_image(VkCommandBuffer cmd, VkImage image, const VulkanImageTransitionInfo& info, VkImageSubresourceRange sub_image)
{
    VkImageMemoryBarrier2 image_barrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
        .srcStageMask = info.src_stage,
        .srcAccessMask = info.src_access,
        .dstStageMask = info.dst_stage,
        .dstAccessMask = info.dst_access,
        .oldLayout = info.old_layout,
        .newLayout = info.new_layout,
        .image = image,
        .subresourceRange = sub_image
    };

    VkDependencyInfo dep_info = {
        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
        .imageMemoryBarrierCount = 1,
        .pImageMemoryBarriers = &image_barrier
    };

    vkCmdPipelineBarrier2(cmd, &dep_info);
}