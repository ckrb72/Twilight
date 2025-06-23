#include <iostream>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <VkBootstrap.h>

#include "vulkan_backend.h"
#include <fstream>

#include "vma.h"

#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_vulkan.h"

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Renderer.h"
#include "DescriptorAllocator.h"
#include "DescriptorLayoutCompiler.h"
#include "GraphicsPipelineCompiler.h"
#include "AssetManager.h"

/*

    Materials will have the texture, colors, etc. as usual but will also have a specific compiled pipeline (and layout)
    associted with them. So if you want to draw all the meshes with a certain material, you bind that pipeline and then do all the descriptors and drawing and stuff...
    Internally, the renderer will holds lists of objects with their associated materials for drawing. When you ask to draw a node, the respective meshes are places in the correct material
    lists and then when rendering actually occurs all those lists are renderered one by one
*/

const int WIN_WIDTH = 1920, WIN_HEIGHT = 1080;

struct GlobalDescriptors
{
    glm::mat4 projection;
    glm::mat4 view;
};

struct PushConstants
{
    glm::mat4 model;
    glm::mat4 norm_mat;
};

// This should actually be in the renderer and should be called by doing renderer.draw(node);
void draw_node(VkCommandBuffer cmd, const SceneNode& node)
{
    for(const Mesh& mesh : node.meshes)
    {
        if(mesh.index_count > 0)
        {
            //renderer.bind_material(mesh.material_index)    /* Can implement same basic batching by having a single buffer per material that data is streamed into. Calling this would switch that buffer */
            //renderer.bind_vertices(mesh.vertices);
            //renderer.bind_indices(mesh.indices, mesh.index_count);
            VkDeviceSize sizes[] = {0};
            vkCmdBindVertexBuffers(cmd, 0, 1, &mesh.vertices.handle, sizes);
            vkCmdBindIndexBuffer(cmd, mesh.indices.handle, 0, VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexed(cmd, mesh.index_count, 1, 0, 0, 0);
        }
    }

    for(const SceneNode& child : node.children)
    {
        draw_node(cmd, child);
    }
}

void free_node(const VulkanContext& context, SceneNode& node)
{
    for(Mesh& mesh : node.meshes)
    {
        vulkan_destroy_buffer(context, mesh.vertices);
        vulkan_destroy_buffer(context, mesh.indices);
        mesh.index_count = 0;
        mesh.material_index = 0;
    }

    for(SceneNode& child : node.children)
    {
        free_node(context, child);
    }
}


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


    VkDescriptorSetLayout descriptor_layout;
    {
        DescriptorLayoutCompiler layout_compiler;
        layout_compiler.add_binding(0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT);
        descriptor_layout = layout_compiler.compile(context.device);
    }

    VkDescriptorSetLayout global_layout;
    {
        DescriptorLayoutCompiler layout_compiler;
        layout_compiler.add_binding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, VK_SHADER_STAGE_VERTEX_BIT);
        global_layout = layout_compiler.compile(context.device);
    }

    DescriptorAllocator descriptor_allocator;
    descriptor_allocator.init_pools(context.device, 20, { {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 10}, {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 10}});
    VkDescriptorSet global_set = descriptor_allocator.allocate(context.device, global_layout);

    VkPushConstantRange push_constant = {
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
        .offset = 0,
        .size = sizeof(PushConstants),
    };

    VkDescriptorSetLayout set_layouts[] = { global_layout, descriptor_layout };

    VkPipelineLayoutCreateInfo pipeline_layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 2,
        .pSetLayouts = set_layouts,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &push_constant
    };
    VkPipelineLayout pipeline_layout;
    VK_CHECK(vkCreatePipelineLayout(context.device, &pipeline_layout_info, nullptr, &pipeline_layout));

    GraphicsPipeline graphics_pipeline;
    {
        VkShaderModule vertex_shader;
        vulkan_load_shader_module("../shaders/default.vert.spv", context.device, &vertex_shader);
        VkShaderModule fragment_shader;
        vulkan_load_shader_module("../shaders/default.frag.spv", context.device, &fragment_shader);

        GraphicsPipelineCompiler graphics_pipeline_compiler;
        graphics_pipeline_compiler.set_layout(pipeline_layout);
        graphics_pipeline_compiler.add_binding(0, 8 * sizeof(float), VK_VERTEX_INPUT_RATE_VERTEX);
        graphics_pipeline_compiler.add_attribute(0, 0, 0, VK_FORMAT_R32G32B32_SFLOAT);
        graphics_pipeline_compiler.add_attribute(0, 1, 3 * sizeof(float), VK_FORMAT_R32G32B32_SFLOAT);
        graphics_pipeline_compiler.add_attribute(0, 2, 6 * sizeof(float), VK_FORMAT_R32G32_SFLOAT);
        graphics_pipeline_compiler.add_shader(vertex_shader, VK_SHADER_STAGE_VERTEX_BIT);
        graphics_pipeline_compiler.add_shader(fragment_shader, VK_SHADER_STAGE_FRAGMENT_BIT);
        graphics_pipeline = graphics_pipeline_compiler.compile(context);
        
        vkDestroyShaderModule(context.device, vertex_shader, nullptr);
        vkDestroyShaderModule(context.device, fragment_shader, nullptr);
    }
    
    VkDescriptorSet descriptor_set = descriptor_allocator.allocate(context.device, descriptor_layout);

    std::vector<uint8_t> image_data = std::vector<uint8_t>(16);
    for(int i = 0; i < 16; i += 4)
    {
        image_data[i + 0] = 255;
        image_data[i + 1] = 0;
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

    VulkanImage depth_image = vulkan_create_image(context, VkExtent3D{context.swapchain.extent.width, context.swapchain.extent.height, 1}, VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT, false);

    float angle = 0.0f;

    glm::mat4 persp = glm::perspective(glm::radians(45.0f), (float)WIN_WIDTH / (float)WIN_HEIGHT, 0.1f, 100.0f);
    persp[1][1] *= -1.0;
    GlobalDescriptors global_descriptors = 
    {
        .projection = persp,
        .view = glm::lookAt(glm::vec3(0.0, 0.0, 5.0), glm::vec3(0.0, 0.0, 0.0), glm::vec3(0.0, 1.0, 0.0))
    };

    VulkanBuffer global_ubo = vulkan_create_buffer(context, sizeof(global_descriptors), &global_descriptors, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

    AssetManager asset_manager;
    asset_manager.init(&context);
    SceneNode cube_model = asset_manager.load_model("../DamagedHelmet.glb");

    double previous_time = glfwGetTime();
    double previous_x, previous_y;
    glfwGetCursorPos(window, &previous_x, &previous_y);
    double xdelta = 0.0, ydelta = 0.0;

    {
        VkDescriptorBufferInfo buffer_info = {
            .buffer = global_ubo.handle,
            .offset = 0,
            .range = sizeof(GlobalDescriptors)
        };

        VkWriteDescriptorSet write_buffer_set = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = global_set,
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .pBufferInfo = &buffer_info
        };

        VkDescriptorImageInfo image_info = {
            .sampler = sampler,
            .imageView = data_image.view,
            .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
        };

        VkWriteDescriptorSet write_image_set = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = descriptor_set,
            .dstBinding = 0,
            .descriptorCount = 1,
            .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
            .pImageInfo = &image_info
        };
        
        VkWriteDescriptorSet write_sets[] = { write_buffer_set, write_image_set };
        vkUpdateDescriptorSets(context.device, 2, write_sets, 0, nullptr);
    }

    while(!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
        double current_time = glfwGetTime();
        double delta = current_time - previous_time;
        previous_time = current_time;

        double current_x, current_y;
        glfwGetCursorPos(window, &current_x, &current_y);
        xdelta = current_x - previous_x;
        ydelta = current_y - previous_y;
        previous_x = current_x;
        previous_y = current_y;

        /*GLFWgamepadstate state;
        if(glfwGetGamepadState(GLFW_JOYSTICK_1, &state))
        {
            std::cout << state.axes[GLFW_GAMEPAD_AXIS_LEFT_X] << std::endl;
        }*/

        /*
            // This would be in the event manager class and should be a function so all you should need to do is call (event_manager.process_events());
            while(!event_manager.queue.empty())
            {
                Event::Event event = queue.dequeue();
                handle_event(event);
            }
        */

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
        ImGui::ShowDemoWindow();
        ImGui::Render();

        VulkanFrameContext frame_context = vulkan_frame_begin(context);
        uint32_t swapchain_index = frame_context.swapchain_index;
        VkCommandBuffer cmd = frame_context.cmd;

        //glm::mat4 view_mat = glm::lookAt(glm::vec3(5.0 * sin(angle * 0.25), 0.0, 5.0 * cos(angle * 0.25)), glm::vec3(0.0, 0.0, 0.0), glm::vec3(0.0, 1.0, 0.0));
        glm::mat4 view_mat = glm::lookAt(glm::vec3(0.0, 0.0, 3.0), glm::vec3(0.0, 0.0, 0.0), glm::vec3(0.0, 1.0, 0.0));
        void* staging_buffer = frame_context.staging_buffer.info.pMappedData;
        memcpy(staging_buffer, glm::value_ptr(view_mat), sizeof(glm::mat4));
        {
            // Add pipeline memory barrier to prevent race conditions
            VkMemoryBarrier2 mem_barrier = {
                .sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER_2,
                .srcStageMask = VK_PIPELINE_STAGE_2_COPY_BIT,
                .srcAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                .dstStageMask = VK_PIPELINE_STAGE_2_COPY_BIT,
                .dstAccessMask = VK_ACCESS_2_TRANSFER_WRITE_BIT
            };

            VkDependencyInfo dependency_info = {
                .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                .memoryBarrierCount = 1,
                .pMemoryBarriers = &mem_barrier
            };

            vkCmdPipelineBarrier2(cmd, &dependency_info);

            // Copy staging buffer over to GPU buffer
            VkBufferCopy copy_info = {
                .srcOffset = 0,
                .dstOffset = offsetof(GlobalDescriptors, view),
                .size = sizeof(glm::mat4),
            };
            
            vkCmdCopyBuffer(cmd, frame_context.staging_buffer.handle, global_ubo.handle, 1, &copy_info);
        }

        vulkan_cmd_transition_image(cmd, context.swapchain.images[swapchain_index], {VK_ACCESS_2_NONE, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, 
                                    VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, 
                                    VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL}, { VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS });

        vulkan_cmd_transition_image(cmd, depth_image.handle, {VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT, 
                                    VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT, 
                                    VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL}, {VK_IMAGE_ASPECT_DEPTH_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS});
        
        {
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
        }

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

        glm::mat4 model_mat = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 0.0f));
        model_mat = glm::rotate(model_mat, glm::radians(angle), glm::vec3(1.0f, 1.0f, 1.0f));
        model_mat = glm::scale(model_mat, glm::vec3(0.25f));

        angle += 10.0 * delta;
        
        VkDescriptorSet descriptor_sets[] = { global_set, descriptor_set };

        PushConstants push_constants = 
        {
            .model = model_mat,
            .norm_mat = glm::transpose(glm::inverse(model_mat))
        };

        vkCmdPushConstants(cmd, pipeline_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstants), &push_constants);
        vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout, 0, 2, descriptor_sets, 0, nullptr);

        draw_node(cmd, cube_model);

        /*VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(cmd, 0, 1, &vertex_buffer.handle, offsets);
        vkCmdBindIndexBuffer(cmd, index_buffer.handle, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(cmd, 36, 1, 0, 0, 0);*/

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
        vulkan_cmd_transition_image(cmd, context.swapchain.images[frame_context.swapchain_index], {VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, 
                                    VK_ACCESS_2_NONE, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR}, 
                                    { VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS });


        vulkan_frame_end(context);
    }

    vkDeviceWaitIdle(context.device);

    free_node(context, cube_model);

    ImGui_ImplVulkan_Shutdown();
    vkDestroyDescriptorPool(context.device, imgui_pool, nullptr);
    vkDestroyDescriptorSetLayout(context.device, global_layout, nullptr);
    vulkan_destroy_buffer(context, global_ubo);

    vulkan_destroy_image(context, data_image);
    vulkan_destroy_image(context, depth_image);
    vulkan_destroy_graphics_pipeline(context, graphics_pipeline);
    vkDestroyPipelineLayout(context.device, pipeline_layout, nullptr);
    vkDestroyDescriptorSetLayout(context.device, descriptor_layout, nullptr);
    descriptor_allocator.destroy_pool(context.device);
    vkDestroySampler(context.device, sampler, nullptr);

    destroy_vulkan(context);

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}

/*

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

        -0.5, -0.5, 0.5,    0.0, 0.0,   0.0, -1.0, 0.0,
        0.5, -0.5, 0.5,     1.0, 0.0,   0.0, -1.0, 0.0,
        0.5, -0.5, -0.5,    1.0, 1.0,   0.0, -1.0, 0.0,
        -0.5, -0.5, -0.5,   0.0, 1.0,   0.0, -1.0, 0.0,

        -0.5, 0.5, -0.5,    0.0, 0.0,   0.0, 1.0, 0.0,
        0.5, 0.5, -0.5,     1.0, 0.0,   0.0, 1.0, 0.0,
        0.5, 0.5, 0.5,      1.0, 1.0,   0.0, 1.0, 0.0,
        -0.5, 0.5, 0.5,     0.0, 1.0,   0.0, 1.0, 0.0
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
*/