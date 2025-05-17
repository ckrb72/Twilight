/*
    Engine Name Ideas - Twilight, Nightfall, Spectre, Mirage
*/

#include <iostream>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <VkBootstrap.h>

#include "vulkan_backend.h"
#include <fstream>

#include "vma.h"
#include "VulkanGraphicsPipeline.h"

bool load_shader_module(const char* path, VkDevice device, VkShaderModule* out_module);

const int WIN_WIDTH = 1920, WIN_HEIGHT = 1080;

int main()
{
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(WIN_WIDTH, WIN_HEIGHT, "Graphic Demo", nullptr, nullptr);

    VulkanContext context = init_vulkan(window, WIN_WIDTH, WIN_HEIGHT);
    assert(validate_vulkan(context));

    VkDescriptorSetLayoutBinding descriptor_binding = {
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
    };

    VkDescriptorSetLayoutCreateInfo descriptor_layout_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings = &descriptor_binding
    };

    VkDescriptorSetLayout descriptor_layout;
    VK_CHECK(vkCreateDescriptorSetLayout(context.device, &descriptor_layout_info, nullptr, &descriptor_layout));

    VkPipelineLayoutCreateInfo pipeline_layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &descriptor_layout
    };
    VkPipelineLayout pipeline_layout;
    VK_CHECK(vkCreatePipelineLayout(context.device, &pipeline_layout_info, nullptr, &pipeline_layout));

    VkShaderModule vertex_shader;
    load_shader_module("../shaders/default.vert.spv", context.device, &vertex_shader);
    VkShaderModule fragment_shader;
    load_shader_module("../shaders/default.frag.spv", context.device, &fragment_shader);

    VulkanGraphicsPipeline graphics_pipeline;
    graphics_pipeline.set_layout(pipeline_layout);

    graphics_pipeline.add_binding(0, 5 * sizeof(float), VK_VERTEX_INPUT_RATE_VERTEX);
    graphics_pipeline.add_attribute(0, 0, 0, VK_FORMAT_R32G32B32_SFLOAT);
    graphics_pipeline.add_attribute(0, 1, 3 * sizeof(float), VK_FORMAT_R32G32_SFLOAT);

    graphics_pipeline.add_shader(vertex_shader, VK_SHADER_STAGE_VERTEX_BIT);
    graphics_pipeline.add_shader(fragment_shader, VK_SHADER_STAGE_FRAGMENT_BIT);
    
    graphics_pipeline.compile(context);

    vkDestroyShaderModule(context.device, vertex_shader, nullptr);
    vkDestroyShaderModule(context.device, fragment_shader, nullptr);

    VkDescriptorPoolSize pool_sizes[] = {
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1}
    };

    VkDescriptorPoolCreateInfo descriptor_pool_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = 1,
        .poolSizeCount = 1,
        .pPoolSizes = pool_sizes
    };

    VkDescriptorPool descriptor_pool;
    VK_CHECK(vkCreateDescriptorPool(context.device, &descriptor_pool_info, nullptr, &descriptor_pool));

    VkDescriptorSetAllocateInfo descriptor_set_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = descriptor_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &descriptor_layout,
    };

    VkDescriptorSet descriptor_set;
    VK_CHECK(vkAllocateDescriptorSets(context.device, &descriptor_set_info, &descriptor_set));

    std::vector<uint8_t> image_data = std::vector<uint8_t>(16);
    for(int i = 0; i < 16; i += 4)
    {
        image_data[i + 0] = 127;
        image_data[i + 1] = 63;
        image_data[i + 2] = 255;
        image_data[i + 3] = 255;
    }

    VulkanImage data_image = vulkan_create_image(context, image_data.data(), VkExtent3D{2, 2, 1}, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, true);

    VkSamplerCreateInfo sampler_info = {
        .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
        .magFilter = VK_FILTER_NEAREST,
        .minFilter = VK_FILTER_NEAREST
    };

    VkSampler sampler;
    VK_CHECK(vkCreateSampler(context.device, &sampler_info, nullptr, &sampler));

    VkDescriptorImageInfo image_info = {
        .sampler = sampler,
        .imageView = data_image.view,
        .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
    };

    VkWriteDescriptorSet write_info = {
        .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
        .dstSet = descriptor_set,
        .dstBinding = 0,
        .descriptorCount = 1,
        .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
        .pImageInfo = &image_info
    };

    vkUpdateDescriptorSets(context.device, 1, &write_info, 0, nullptr);


    VulkanBuffer staging_buffer = vulkan_create_buffer(context, 3 * 5 * sizeof(float), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

    float vertices[] =
    {
        -0.5, 0.5, 0.0,     0.0, 0.0,
        0.5, 0.5, 0.0,      1.0, 0.0,
        0.0, -0.5, 0.0,     0.5, 1.0
    };

    void* vert_ptr = staging_buffer.info.pMappedData;
    memcpy(vert_ptr, vertices, sizeof(vertices));

    VulkanBuffer vertex_buffer = vulkan_create_buffer(context, 3 * 5 * sizeof(float), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY);

    vulkan_immediate_begin(context);
    VkBufferCopy copy_info = {
        .srcOffset = 0,
        .dstOffset = 0,
        .size = 3 * 5 * sizeof(float),
    };
    vkCmdCopyBuffer(context.immediate_buffer, staging_buffer.buffer, vertex_buffer.buffer, 1, &copy_info);
    vulkan_immediate_end(context);

    vulkan_destroy_buffer(context, staging_buffer);

    uint32_t current_frame = 0;

    while(!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        VulkanFrameData* frame = &context.frames[current_frame];
        VkCommandBuffer cmd = frame->cmd_buffer;

        // Wait for fence to be signaled (if first loop fence is already signaled)
        VK_CHECK(vkWaitForFences(context.device, 1, &frame->render_fence, true, UINT64_MAX));
        // Here Fence is signaled

        // Reset Fence to unsignaled
        VK_CHECK(vkResetFences(context.device, 1, &frame->render_fence));

        // Acquire swapchain image. Swapchain_semaphore will be signaled once it has been acquired
        uint32_t swapchain_index;
        VK_CHECK(vkAcquireNextImageKHR(context.device, context.swapchain.swapchain, UINT64_MAX, frame->swapchain_semaphore, nullptr, &swapchain_index))

        VkImage swapchain_image = context.swapchain.images[swapchain_index];

        VK_CHECK(vkResetCommandBuffer(cmd, 0));

        VkCommandBufferBeginInfo cmd_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
        };

        VK_CHECK(vkBeginCommandBuffer(cmd, &cmd_info));
        VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT;
        
        VkImageSubresourceRange sub_image = {
            .aspectMask = aspect,
            .baseMipLevel = 0,
            .levelCount = VK_REMAINING_MIP_LEVELS,
            .baseArrayLayer = 0,
            .layerCount = VK_REMAINING_ARRAY_LAYERS 
        };

        VkImageMemoryBarrier2 image_barrier = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            .srcAccessMask = VK_ACCESS_2_NONE,
            .dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT,
            .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
            .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .image = swapchain_image,
            .subresourceRange = sub_image
        };

        VkDependencyInfo dep_info = {
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers = &image_barrier
        };

        vkCmdPipelineBarrier2(cmd, &dep_info);

        VkRenderingAttachmentInfo color_attachment_info = {
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .imageView = context.swapchain.views[swapchain_index],
            .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .clearValue = {
                .color = {0.1, 0.1, 0.1, 1.0}
            }
        };

        VkRenderingInfo render_info = {
            .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .renderArea = VkRect2D{ {0, 0}, {context.swapchain.extent.width, context.swapchain.extent.height} },
            .layerCount = 1,
            .colorAttachmentCount = 1,
            .pColorAttachments = &color_attachment_info
        };

        vkCmdBeginRendering(cmd, &render_info);
        //vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        graphics_pipeline.cmd_bind(cmd);

        VkViewport viewport = {
            .x = 0,
            .y = 0,
            .width = (float)context.swapchain.extent.width,
            .height = (float)context.swapchain.extent.height,
            .minDepth = 0.0f,
            .maxDepth = 1.0f
        };
        VkRect2D scissor = {};
        scissor.extent = context.swapchain.extent;
        scissor.offset = VkOffset2D{0, 0};
        vkCmdSetViewport(cmd, 0, 1, &viewport);
        vkCmdSetScissor(cmd, 0, 1, &scissor);

        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(cmd, 0, 1, &vertex_buffer.buffer, offsets);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, 0, 1, &descriptor_set, 0, nullptr);
        vkCmdDraw(cmd, 3, 1, 0, 0);

        vkCmdEndRendering(cmd);

        // Transition Image to presentable layout

        VkImageSubresourceRange sub_image2 = {
            .aspectMask = aspect,
            .baseMipLevel = 0,
            .levelCount = VK_REMAINING_MIP_LEVELS,
            .baseArrayLayer = 0,
            .layerCount = VK_REMAINING_ARRAY_LAYERS 
        };

        VkImageMemoryBarrier2 image_barrier2 = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
            .srcStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            .srcAccessMask = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
            .dstStageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dstAccessMask = VK_ACCESS_2_NONE,
            .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
            .image = swapchain_image,
            .subresourceRange = sub_image2
        };

        VkDependencyInfo dep_info2 = {
            .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
            .imageMemoryBarrierCount = 1,
            .pImageMemoryBarriers = &image_barrier2
        };

        vkCmdPipelineBarrier2(cmd, &dep_info2);

        VK_CHECK(vkEndCommandBuffer(cmd));
        
        // Submit the commands to the command buffer
        VkCommandBufferSubmitInfo cmd_submit_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
            .commandBuffer = cmd,
            .deviceMask = 0
        };

        VkSemaphoreSubmitInfo wait_info = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .semaphore = frame->swapchain_semaphore,
            .value = 1,
            .stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
            .deviceIndex = 0
        };

        VkSemaphoreSubmitInfo signal_info = {
            .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
            .semaphore = frame->render_semaphore,
            .value = 1,
            .stageMask = VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT_KHR,
            .deviceIndex = 0
        };

        VkSubmitInfo2 submit_info = {
            .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
            .waitSemaphoreInfoCount = 1,
            .pWaitSemaphoreInfos = &wait_info,
            .commandBufferInfoCount = 1,
            .pCommandBufferInfos = &cmd_submit_info,
            .signalSemaphoreInfoCount = 1,
            .pSignalSemaphoreInfos = &signal_info
        };

        // Submits commands to queue and sets fence to signal when done
        VK_CHECK(vkQueueSubmit2(context.graphics_queue.queue, 1, &submit_info, frame->render_fence));

        VkPresentInfoKHR present_info = {
            .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
            .waitSemaphoreCount = 1,
            .pWaitSemaphores = &frame->render_semaphore,
            .swapchainCount = 1,
            .pSwapchains = &context.swapchain.swapchain,
            .pImageIndices = &swapchain_index
        };
        VK_CHECK(vkQueuePresentKHR(context.graphics_queue.queue, &present_info));
        current_frame += (current_frame + 1) % FLIGHT_COUNT;
    }

    vkDeviceWaitIdle(context.device);

    vulkan_destroy_buffer(context, vertex_buffer);
    vulkan_destroy_image(context, data_image);

    graphics_pipeline.destroy(context);
    vkDestroyPipelineLayout(context.device, pipeline_layout, nullptr);
    vkDestroyDescriptorSetLayout(context.device, descriptor_layout, nullptr);
    vkDestroyDescriptorPool(context.device, descriptor_pool, nullptr);
    vkDestroySampler(context.device, sampler, nullptr);

    destroy_vulkan(context);

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}


bool load_shader_module(const char* path, VkDevice device, VkShaderModule* out_module)
{
    std::ifstream file(path, std::ios::ate | std::ios::binary);
    if(!file.is_open())
    {
        std::cerr << "Failed to open file: " << path << std::endl;
        return false;
    }

    size_t file_size = (size_t)file.tellg();
    std::vector<uint32_t> buffer(file_size / sizeof(uint32_t));
    file.seekg(0);
    file.read((char*)buffer.data(), file_size);
    file.close();

    VkShaderModuleCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = buffer.size() * sizeof(uint32_t),
        .pCode = buffer.data()
    };

    VkShaderModule shader_module;
    VK_CHECK(vkCreateShaderModule(device, &create_info, nullptr, &shader_module))

    *out_module = shader_module;

    return true;
}