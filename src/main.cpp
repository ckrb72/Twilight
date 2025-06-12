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
#include "VulkanGraphicsPipelineCompiler.h"

#include <fastgltf/core.hpp>
#include <fastgltf/types.hpp>
#include <fastgltf/tools.hpp>

#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_vulkan.h"

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

bool load_shader_module(const char* path, VkDevice device, VkShaderModule* out_module);

const int WIN_WIDTH = 1920, WIN_HEIGHT = 1080;


int main()
{
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(WIN_WIDTH, WIN_HEIGHT, "Graphic Demo", nullptr, nullptr);

    VulkanContext context = init_vulkan(window, WIN_WIDTH, WIN_HEIGHT);
    assert(validate_vulkan(context));

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();

    ImGui_ImplGlfw_InitForVulkan(window, true);
    
    VkDescriptorPoolSize imgui_pool_sizes[] = {
        {VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 }
    };

    VkDescriptorPoolCreateInfo imgui_pool_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT,
        .maxSets = 1000,
        .poolSizeCount = 11,
        .pPoolSizes = imgui_pool_sizes,
    };

    VkDescriptorPool imgui_pool;
    VK_CHECK(vkCreateDescriptorPool(context.device, &imgui_pool_info, nullptr, &imgui_pool));

    ImGui_ImplVulkan_InitInfo imgui_info = {
        .ApiVersion = VK_API_VERSION_1_3,
        .Instance = context.instance,
        .PhysicalDevice = context.physical_device,
        .Device = context.device,
        .QueueFamily = context.graphics_queue.family,
        .Queue = context.graphics_queue.queue,
        .DescriptorPool = imgui_pool,
        .MinImageCount = static_cast<uint32_t>(context.swapchain.images.size()),
        .ImageCount = static_cast<uint32_t>(context.swapchain.images.size()),
        .MSAASamples = VK_SAMPLE_COUNT_1_BIT,
        .PipelineCache = VK_NULL_HANDLE,
        .UseDynamicRendering = true
    };

    imgui_info.PipelineRenderingCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &context.swapchain.format
    };

    ImGui_ImplVulkan_Init(&imgui_info);
    ImGui_ImplVulkan_CreateFontsTexture();

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

    VkPushConstantRange push_constant = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .offset = 0,
        .size = sizeof(glm::mat4),
    };

    VkPipelineLayoutCreateInfo pipeline_layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &descriptor_layout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &push_constant
    };
    VkPipelineLayout pipeline_layout;
    VK_CHECK(vkCreatePipelineLayout(context.device, &pipeline_layout_info, nullptr, &pipeline_layout));

    VulkanGraphicsPipeline graphics_pipeline;
    {
        VkShaderModule vertex_shader;
        vulkan_load_shader_module("../shaders/default.vert.spv", context.device, &vertex_shader);
        VkShaderModule fragment_shader;
        vulkan_load_shader_module("../shaders/default.frag.spv", context.device, &fragment_shader);

        VulkanGraphicsPipelineCompiler graphics_pipeline_compiler;
        graphics_pipeline_compiler.set_layout(pipeline_layout);
        graphics_pipeline_compiler.add_binding(0, 8 * sizeof(float), VK_VERTEX_INPUT_RATE_VERTEX);
        graphics_pipeline_compiler.add_attribute(0, 0, 0, VK_FORMAT_R32G32B32_SFLOAT);
        graphics_pipeline_compiler.add_attribute(0, 1, 3 * sizeof(float), VK_FORMAT_R32G32_SFLOAT);
        graphics_pipeline_compiler.add_attribute(0, 2, 5 * sizeof(float), VK_FORMAT_R32G32B32_SFLOAT);
        graphics_pipeline_compiler.add_shader(vertex_shader, VK_SHADER_STAGE_VERTEX_BIT);
        graphics_pipeline_compiler.add_shader(fragment_shader, VK_SHADER_STAGE_FRAGMENT_BIT);
        graphics_pipeline = graphics_pipeline_compiler.compile(context);
        
        vkDestroyShaderModule(context.device, vertex_shader, nullptr);
        vkDestroyShaderModule(context.device, fragment_shader, nullptr);
    }
    
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

    float vertices[] =
    {
        -0.5, 0.5, 0.5,     0.0, 0.0,   0.0, 0.0, 1.0,
        0.5, 0.5, 0.5,      1.0, 0.0,   0.0, 0.0, 1.0,
        0.5, -0.5, 0.5,     1.0, 1.0,   0.0, 0.0, 1.0,
        -0.5, -0.5, 0.5,    0.0, 1.0,   0.0, 0.0, 1.0,

        0.5, 0.5, -0.5,     0.0, 0.0,   0.0, 0.0, -1.0,
        -0.5, 0.5, -0.5,    1.0, 0.0,   0.0, 0.0, -1.0,
        -0.5, -0.5, -0.5,   1.0, 1.0,   0.0, 0.0, -1.0,
        0.5, -0.5, -0.5,    0.0, 1.0,   0.0, 0.0, -1.0,

        -0.5, 0.5, -0.5,    0.0, 0.0,   -1.0, 0.0, 0.0,
        -0.5, 0.5, 0.5,     1.0, 0.0,   -1.0, 0.0, 0.0,
        -0.5, -0.5, 0.5,    1.0, 1.0,   -1.0, 0.0, 0.0,
        -0.5, -0.5, -0.5,   0.0, 1.0,   -1.0, 0.0, 0.0,

        0.5, 0.5, 0.5,      0.0, 0.0,   1.0, 0.0, 0.0,
        0.5, 0.5, -0.5,     1.0, 0.0,   1.0, 0.0, 0.0,
        0.5, -0.5, -0.5,    1.0, 1.0,   1.0, 0.0, 0.0,
        0.5, -0.5, 0.5,     0.0, 1.0,   1.0, 0.0, 0.0,

        -0.5, -0.5, 0.5,    0.0, 0.0,   0.0, 1.0, 0.0,
        0.5, -0.5, 0.5,     1.0, 0.0,   0.0, 1.0, 0.0,
        0.5, -0.5, -0.5,    1.0, 1.0,   0.0, 1.0, 0.0,
        -0.5, -0.5, -0.5,   0.0, 1.0,   0.0, 1.0, 0.0,

        -0.5, 0.5, -0.5,    0.0, 0.0,   0.0, -1.0, 0.0,
        0.5, 0.5, -0.5,     1.0, 0.0,   0.0, -1.0, 0.0,
        0.5, 0.5, 0.5,      1.0, 1.0,   0.0, -1.0, 0.0,
        -0.5, 0.5, 0.5,     0.0, 1.0,   0.0, -1.0, 0.0
    };
    
    VulkanBuffer staging_buffer = vulkan_create_buffer(context, sizeof(vertices), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
    void* vert_ptr = staging_buffer.info.pMappedData;
    memcpy(vert_ptr, vertices, sizeof(vertices));


    unsigned int indices[] = 
    {
        0, 1, 2,
        2, 3, 0,

        4, 5, 6,
        6, 7, 4,

        8, 9, 10,
        10, 11, 8,

        12, 13, 14,
        14, 15, 12,

        16, 17, 18,
        18, 19, 16,

        20, 21, 22,
        22, 23, 20
    };

    VulkanBuffer index_staging_buffer = vulkan_create_buffer(context, sizeof(indices), VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
    void* index_ptr = index_staging_buffer.info.pMappedData;
    memcpy(index_ptr, indices, sizeof(indices));

    VulkanBuffer index_buffer = vulkan_create_buffer(context, sizeof(indices), VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY);
    VulkanBuffer vertex_buffer = vulkan_create_buffer(context, sizeof(vertices), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY);
    vulkan_immediate_begin(context);

    vulkan_cmd_copy_buffer_to_buffer(context.immediate_buffer, sizeof(vertices), staging_buffer, vertex_buffer);
    vulkan_cmd_copy_buffer_to_buffer(context.immediate_buffer, sizeof(indices), index_staging_buffer, index_buffer);

    vulkan_immediate_end(context);

    vulkan_destroy_buffer(context, staging_buffer);
    vulkan_destroy_buffer(context, index_staging_buffer);

    VulkanImage depth_image = vulkan_create_image(context, VkExtent3D{context.swapchain.extent.width, context.swapchain.extent.height, 1}, VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, false);


    float angle = 0.0f;

    while(!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGui::ShowDemoWindow();
        ImGui::Render();

        VulkanFrameData* frame = &context.frames[context.current_frame];
        VkCommandBuffer cmd = frame->cmd_buffer;

        // Wait for fence to be signaled (if first loop fence is already signaled)
        VK_CHECK(vkWaitForFences(context.device, 1, &frame->render_fence, true, UINT64_MAX));
        // Here Fence is signaled

        // Reset Fence to unsignaled
        VK_CHECK(vkResetFences(context.device, 1, &frame->render_fence));

        // Acquire swapchain image. Swapchain_semaphore will be signaled once it has been acquired
        uint32_t swapchain_index;
        VK_CHECK(vkAcquireNextImageKHR(context.device, context.swapchain.swapchain, UINT64_MAX, frame->swapchain_semaphore, nullptr, &swapchain_index));

        VkImage swapchain_image = context.swapchain.images[swapchain_index];

        VK_CHECK(vkResetCommandBuffer(cmd, 0));

        VkCommandBufferBeginInfo cmd_info = {
            .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
            .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
        };

        VK_CHECK(vkBeginCommandBuffer(cmd, &cmd_info));

        VulkanImageTransitionInfo initial_transition_info = {
            .src_access = VK_ACCESS_2_NONE,
            .src_stage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dst_access = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT,
            .dst_stage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            .old_layout = VK_IMAGE_LAYOUT_UNDEFINED,
            .new_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        };

        vulkan_cmd_transition_image(cmd, swapchain_image, initial_transition_info, { VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS });

        VulkanImageTransitionInfo depth_transition_info = {
            .src_access = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            .src_stage = VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT,
            .dst_access = VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            .dst_stage = VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT,
            .old_layout = VK_IMAGE_LAYOUT_UNDEFINED,
            .new_layout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL
        };

        vulkan_cmd_transition_image(cmd, depth_image.handle, depth_transition_info, {VK_IMAGE_ASPECT_DEPTH_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS});

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

        VkRenderingAttachmentInfo depth_attachment_info = {
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .imageView = depth_image.view,
            .imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
            .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .clearValue = {
                .depthStencil = {.depth = 1.0f}
            }
        };

        VkRenderingInfo render_info = {
            .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .renderArea = VkRect2D{ {0, 0}, {context.swapchain.extent.width, context.swapchain.extent.height} },
            .layerCount = 1,
            .colorAttachmentCount = 1,
            .pColorAttachments = &color_attachment_info,
            .pDepthAttachment = &depth_attachment_info
        };

        vkCmdBeginRendering(cmd, &render_info);
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, graphics_pipeline.pipeline);

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
        vkCmdBindVertexBuffers(cmd, 0, 1, &vertex_buffer.handle, offsets);
        vkCmdBindIndexBuffer(cmd, index_buffer.handle, 0, VK_INDEX_TYPE_UINT32);

        glm::mat4 model_mat = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -2.0f));
        model_mat = glm::rotate(model_mat, glm::radians(angle), glm::vec3(1.0f, 1.0f, 1.0f));
        model_mat = glm::scale(model_mat, glm::vec3(0.25f));

        angle += 0.05f;

        glm::mat4 persp = glm::perspective(glm::radians(45.0f), (float)WIN_WIDTH / (float)WIN_HEIGHT, 0.1f, 100.0f);
        glm::mat4 push_mat = persp * model_mat;

        vkCmdPushConstants(cmd, *graphics_pipeline.layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(glm::mat4), glm::value_ptr(push_mat));
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, *graphics_pipeline.layout, 0, 1, &descriptor_set, 0, nullptr);
        vkCmdDrawIndexed(cmd, 36, 1, 0, 0, 0);

        vkCmdEndRendering(cmd);


        VkRenderingAttachmentInfo imgui_attachment = {
            .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
            .imageView = context.swapchain.views[swapchain_index],
            .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
            .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
            .clearValue = {
                .color = {0.0, 0.0, 0.0, 1.0}
            }
        };

        VkRenderingInfo imgui_render_info = {
            .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
            .renderArea = VkRect2D{ {0, 0}, {context.swapchain.extent.width, context.swapchain.extent.height} },
            .layerCount = 1,
            .colorAttachmentCount = 1,
            .pColorAttachments = &imgui_attachment
        }; 

        vkCmdBeginRendering(cmd, &imgui_render_info);
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
        vkCmdEndRendering(cmd);
        
        // Transition Image to presentable layout

        VulkanImageTransitionInfo transition_info = {
            .src_access = VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT,
            .src_stage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            .dst_access = VK_ACCESS_2_NONE,
            .dst_stage = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT,
            .old_layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            .new_layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR
        };

        vulkan_cmd_transition_image(cmd, swapchain_image, transition_info, { VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS });

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
        context.current_frame += (context.current_frame + 1) % FLIGHT_COUNT;
    }

    vkDeviceWaitIdle(context.device);

    ImGui_ImplVulkan_Shutdown();
    vkDestroyDescriptorPool(context.device, imgui_pool, nullptr);

    vulkan_destroy_buffer(context, vertex_buffer);
    vulkan_destroy_buffer(context, index_buffer);
    vulkan_destroy_image(context, data_image);
    vulkan_destroy_image(context, depth_image);
    vulkan_destroy_graphics_pipeline(context, graphics_pipeline);
    vkDestroyPipelineLayout(context.device, pipeline_layout, nullptr);
    vkDestroyDescriptorSetLayout(context.device, descriptor_layout, nullptr);
    vkDestroyDescriptorPool(context.device, descriptor_pool, nullptr);
    vkDestroySampler(context.device, sampler, nullptr);

    destroy_vulkan(context);

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

void load_model(const std::string& path)
{
    fastgltf::Parser parser;
    auto gltf_file = fastgltf::MappedGltfFile::FromPath(path);
    auto asset = parser.loadGltf(gltf_file.get(), path);

}