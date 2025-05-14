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

bool load_shader_module(const char* path, VkDevice device, VkShaderModule* out_module);

const int WIN_WIDTH = 1920, WIN_HEIGHT = 1080;

int main()
{
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(WIN_WIDTH, WIN_HEIGHT, "Graphic Demo", nullptr, nullptr);

    VulkanContext context = init_vulkan(window, WIN_WIDTH, WIN_HEIGHT);
    assert(validate_vulkan(context));

    VkPipelineLayoutCreateInfo pipeline_layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO
    };
    VkPipelineLayout pipeline_layout;
    VK_CHECK(vkCreatePipelineLayout(context.device, &pipeline_layout_info, nullptr, &pipeline_layout));

    VkPipelineInputAssemblyStateCreateInfo input_assembler = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = VK_FALSE
    };

    VkPipelineRasterizationStateCreateInfo rasterizer = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .cullMode = VK_CULL_MODE_NONE,
        .frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE,
        .lineWidth = 1.0f
    };

    VkPipelineMultisampleStateCreateInfo multisampling = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        .sampleShadingEnable = VK_FALSE,
        .minSampleShading = 1.0f,
        .pSampleMask = nullptr,
        .alphaToCoverageEnable = VK_FALSE,
        .alphaToOneEnable = VK_FALSE
    };

    VkPipelineDepthStencilStateCreateInfo depth_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
        .depthTestEnable = VK_TRUE,
        .depthWriteEnable = VK_TRUE,
        .depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL,
        .depthBoundsTestEnable = VK_FALSE,
        .stencilTestEnable = VK_FALSE,
        .front = {},
        .back = {},
        .minDepthBounds = 0.0f,
        .maxDepthBounds = 1.0f
    };

    VkPipelineRenderingCreateInfo render_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &context.swapchain.format,
        //.depthAttachmentFormat = VK_FORMAT_D32_SFLOAT
    };

    VkPipelineViewportStateCreateInfo viewport_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .scissorCount = 1
    };

    VkPipelineColorBlendAttachmentState blend_attachment = {
        .blendEnable = VK_FALSE,
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT
    };

    VkPipelineColorBlendStateCreateInfo color_blending = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable = VK_FALSE,
        .logicOp = VK_LOGIC_OP_COPY,
        .attachmentCount = 1,
        .pAttachments = &blend_attachment
    };

    VkPipelineVertexInputStateCreateInfo vertex_input_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
    };

    VkDynamicState state[] = { VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR };
    VkPipelineDynamicStateCreateInfo dynamic_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = 2,
        .pDynamicStates = &state[0]
    };

    VkShaderModule vertex_shader;
    load_shader_module("../shaders/default.vert.spv", context.device, &vertex_shader);
    VkShaderModule fragment_shader;
    load_shader_module("../shaders/default.frag.spv", context.device, &fragment_shader);

    VkPipelineShaderStageCreateInfo vertex_stage = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_VERTEX_BIT,
        .module = vertex_shader,
        .pName = "main"
    };

    VkPipelineShaderStageCreateInfo fragment_stage = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
        .module = fragment_shader,
        .pName = "main"
    };

    VkPipelineShaderStageCreateInfo shader_stages[] = { vertex_stage, fragment_stage };


    VkGraphicsPipelineCreateInfo pipeline_info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &render_info,
        .stageCount = 2,
        .pStages = shader_stages,
        .pVertexInputState = &vertex_input_info,
        .pInputAssemblyState = &input_assembler,
        .pViewportState = &viewport_state,
        .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisampling,
        .pDepthStencilState = &depth_info,
        .pColorBlendState = &color_blending,
        .pDynamicState = &dynamic_info,
        .layout = pipeline_layout
    };

    VkPipeline pipeline;
    VK_CHECK(vkCreateGraphicsPipelines(context.device, VK_NULL_HANDLE, 1, &pipeline_info, nullptr, &pipeline));

    vkDestroyShaderModule(context.device, vertex_shader, nullptr);
    vkDestroyShaderModule(context.device, fragment_shader, nullptr);


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

        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

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

    vkDestroyPipeline(context.device, pipeline, nullptr);
    vkDestroyPipelineLayout(context.device, pipeline_layout, nullptr);

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