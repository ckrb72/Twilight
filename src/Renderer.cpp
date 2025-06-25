#include "Renderer.h"
#include <VkBootstrap.h>
#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_vulkan.h"

#include "GraphicsPipelineCompiler.h"
#include "render_util.h"
#include "render_backend.h"
#include <fstream>

#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

struct PushConstants
{
    glm::mat4 model;
    glm::mat4 norm_mat;
};

static bool vulkan_load_shader_module(const char* path, VkDevice device, VkShaderModule* out_module)
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


namespace Twilight
{
    namespace Render
    {

        Renderer::Renderer()
        {

        }


        Renderer::~Renderer()
        {

        }

        void Renderer::init(GLFWwindow* window, uint32_t width, uint32_t height)
        {
            this->window = window;
            init_vulkan();
            create_swapchain(width, height);
            init_imgui();
            this->material_set_allocator.init_pools(this->device, 20, {{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 10}, {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 10}});
            this->general_set_allocator.init_pools(this->device, 20, {{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 20}});

            {
                VkCommandPoolCreateInfo pool_info = {
                    .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                    .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                    .queueFamilyIndex = this->transfer_queue.family
                };

                VK_CHECK(vkCreateCommandPool(this->device, &pool_info, nullptr, &this->transfer_pool));

                VkCommandBufferAllocateInfo alloc_info = {
                    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                    .commandPool = this->transfer_pool,
                    .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                    .commandBufferCount = 1
                };

                VK_CHECK(vkAllocateCommandBuffers(this->device, &alloc_info, &this->transfer_cmd));

                VkFenceCreateInfo fence_info = {
                    .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                    .flags = VK_FENCE_CREATE_SIGNALED_BIT
                };
                VK_CHECK(vkCreateFence(this->device, &fence_info, nullptr, &this->transfer_fence));
            }

            // Temporary
            {
                VkDescriptorSetLayoutBinding global_ubo = {
                    .binding = 0,
                    .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                    .descriptorCount = 1,
                    .stageFlags = VK_SHADER_STAGE_VERTEX_BIT
                };

                VkDescriptorSetLayoutCreateInfo global_info = {
                    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                    .bindingCount = 1,
                    .pBindings = &global_ubo
                };

                VK_CHECK(vkCreateDescriptorSetLayout(this->device, &global_info, nullptr, &this->global_layout));
            }

            {
                VkDescriptorSetLayoutBinding diffuse_tex = {
                    .binding = 0,
                    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .descriptorCount = 1,
                    .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
                };

                VkDescriptorSetLayoutCreateInfo phong_info = {
                    .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                    .bindingCount = 1,
                    .pBindings = &diffuse_tex
                };

                VK_CHECK(vkCreateDescriptorSetLayout(this->device, &phong_info, nullptr, &this->phong_material_layout));
            }

            {
                VkDescriptorSetLayout set_layouts[] = { this->global_layout, this->phong_material_layout };

                VkPushConstantRange push_constant = {
                    .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
                    .offset = 0,
                    .size = 128,   
                };

                VkPipelineLayoutCreateInfo layout_info = {
                    .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
                    .setLayoutCount = 2,
                    .pSetLayouts = set_layouts,
                    .pushConstantRangeCount = 1,
                    .pPushConstantRanges = &push_constant
                };

                VkPipelineLayout pipeline_layout;
                VK_CHECK(vkCreatePipelineLayout(this->device, &layout_info, nullptr, &pipeline_layout));

                VkShaderModule vertex_shader;
                vulkan_load_shader_module("../shaders/default.vert.spv", this->device, &vertex_shader);
                VkShaderModule fragment_shader;
                vulkan_load_shader_module("../shaders/default.frag.spv", this->device, &fragment_shader);

                GraphicsPipelineCompiler graphics_pipeline_compiler;
                graphics_pipeline_compiler.set_layout(pipeline_layout);
                graphics_pipeline_compiler.set_color_formats({this->swapchain.format});
                graphics_pipeline_compiler.add_binding(0, 8 * sizeof(float), VK_VERTEX_INPUT_RATE_VERTEX);
                graphics_pipeline_compiler.add_attribute(0, 0, 0, VK_FORMAT_R32G32B32_SFLOAT);
                graphics_pipeline_compiler.add_attribute(0, 1, 3 * sizeof(float), VK_FORMAT_R32G32B32_SFLOAT);
                graphics_pipeline_compiler.add_attribute(0, 2, 6 * sizeof(float), VK_FORMAT_R32G32_SFLOAT);
                graphics_pipeline_compiler.add_shader(vertex_shader, VK_SHADER_STAGE_VERTEX_BIT);
                graphics_pipeline_compiler.add_shader(fragment_shader, VK_SHADER_STAGE_FRAGMENT_BIT);
                this->phong_pipeline = graphics_pipeline_compiler.compile(this->device);
        
                vkDestroyShaderModule(this->device, vertex_shader, nullptr);
                vkDestroyShaderModule(this->device, fragment_shader, nullptr);
            }

            {
                this->global_set = this->general_set_allocator.allocate(this->device, this->global_layout);

                GlobalUbo global_ubo_cpu = {
                    .projection = glm::perspective(glm::radians(45.0f), (float)width/ (float)height, 0.1f, 100.0f),
                    .view = glm::lookAt(glm::vec3(0.0, 0.0, 5.0), glm::vec3(0.0, 0.0, 0.0), glm::vec3(0.0, 1.0, 0.0))
                };

                global_ubo_cpu.projection[1][1] *= -1.0;

                this->global_ubo = Vulkan::create_buffer(this->device, {this->transfer_cmd, this->transfer_queue.handle, this->transfer_fence}, this->allocator, &global_ubo_cpu, sizeof(GlobalUbo), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);

                VkDescriptorBufferInfo buffer_info = {
                    .buffer = this->global_ubo.handle,
                    .offset = 0,
                    .range = sizeof(GlobalUbo)
                };

                VkWriteDescriptorSet write_set = {
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstSet = this->global_set,
                    .dstBinding = 0,
                    .descriptorCount = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
                    .pBufferInfo = &buffer_info
                };

                vkUpdateDescriptorSets(this->device, 1, &write_set, 0, nullptr);
            }

            {
                this->error_material = this->material_set_allocator.allocate(this->device, this->phong_material_layout);
                std::vector<uint8_t> image_data = std::vector<uint8_t>(16);
                for(int i = 0; i < 16; i += 4)
                {
                    image_data[i + 0] = 255;
                    image_data[i + 1] = 0;
                    image_data[i + 2] = 255;
                    image_data[i + 3] = 255;
                }

                this->error_texture = Vulkan::create_image(this->device, this->allocator, {this->transfer_cmd, this->transfer_queue.handle, this->transfer_fence}, image_data.data(), {2, 2, 1}, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_USAGE_SAMPLED_BIT);

                VkSamplerCreateInfo sampler_info = {
                    .sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
                    .magFilter = VK_FILTER_NEAREST,
                    .minFilter = VK_FILTER_NEAREST
                };

                VK_CHECK(vkCreateSampler(this->device, &sampler_info, nullptr, &this->default_sampler));

                VkDescriptorImageInfo image_info = {
                    .sampler = this->default_sampler,
                    .imageView = this->error_texture.view,
                    .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
                };

                VkWriteDescriptorSet write_image_set = {
                    .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
                    .dstSet = this->error_material,
                    .dstBinding = 0,
                    .descriptorCount = 1,
                    .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                    .pImageInfo = &image_info
                };

                vkUpdateDescriptorSets(this->device, 1, &write_image_set, 0, nullptr);
            }


            this->depth_buffer = Vulkan::create_image(this->device, this->allocator, {this->swapchain.extent.width, this->swapchain.extent.height, 1}, VK_FORMAT_D32_SFLOAT, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT);
        }

        void Renderer::deinit()
        {
            vkDeviceWaitIdle(this->device);

            Vulkan::destroy_image(this->device, this->allocator, this->depth_buffer);
            Vulkan::destroy_buffer(this->allocator, this->global_ubo);
            Vulkan::destroy_image(this->device, this->allocator, this->error_texture);
            vkDestroySampler(this->device, this->default_sampler, nullptr);

            vkDestroyFence(this->device, this->transfer_fence, nullptr);
            vkDestroyCommandPool(this->device, this->transfer_pool, nullptr);
            vkDestroyDescriptorSetLayout(this->device, this->phong_material_layout, nullptr);
            vkDestroyDescriptorSetLayout(this->device, this->global_layout, nullptr);
            vkDestroyPipeline(this->device, this->phong_pipeline.handle, nullptr);
            vkDestroyPipelineLayout(this->device, this->phong_pipeline.layout, nullptr);


            general_set_allocator.destroy_pool(this->device);
            material_set_allocator.destroy_pool(this->device);
            deinit_imgui();
            destroy_swapchain();
            deinit_vulkan();
        }

        void Renderer::init_vulkan()
        {
            vkb::InstanceBuilder instance_builder;
            vkb::Instance instance = instance_builder.set_app_name("renderer")
                                              .request_validation_layers(true)
                                              .add_validation_feature_enable(VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT)
                                              .use_default_debug_messenger()
                                              .require_api_version(1, 3, 0)
                                              .build()
                                              .value();
    
            VK_CHECK(glfwCreateWindowSurface(instance.instance, (GLFWwindow*)this->window, nullptr, &this->surface));

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
                                              .set_surface(this->surface)
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

            VK_CHECK(vmaCreateAllocator(&allocator_info, &this->allocator));

            this->instance = instance.instance;
            this->device = device.device;
            this->physical_device = gpu.physical_device;
            this->debug_messenger = instance.debug_messenger;
            this->graphics_queue = { 
                .handle = device.get_queue(vkb::QueueType::graphics).value(), 
                .family = device.get_queue_index(vkb::QueueType::graphics).value() 
            };
            this->transfer_queue = {
                .handle = device.get_queue(vkb::QueueType::transfer).value(),
                .family = device.get_queue_index(vkb::QueueType::transfer).value()
            };

            // Sync objects and command pool
            {
                VkSemaphoreCreateInfo semaphore_info = {
                    .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO
                };

                VkFenceCreateInfo fence_info = {
                    .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
                    .flags = VK_FENCE_CREATE_SIGNALED_BIT
                };
                
                VkCommandPoolCreateInfo pool_info = {
                    .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
                    .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
                    .queueFamilyIndex = this->graphics_queue.family
                };

                for(int i = 0; i < FRAME_FLIGHT_COUNT; i++)
                {
                    VK_CHECK(vkCreateCommandPool(this->device, &pool_info, nullptr, &this->frames_intl[i].pool));

                    VkCommandBufferAllocateInfo buffer_info = {
                        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
                        .commandPool = this->frames_intl[i].pool,
                        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                        .commandBufferCount = 1
                    };

                    VK_CHECK(vkAllocateCommandBuffers(this->device, &buffer_info, &this->frames[i].cmd));
                    VK_CHECK(vkCreateSemaphore(this->device, &semaphore_info, nullptr, &this->frames_intl[i].swapchain_semaphore));
                    VK_CHECK(vkCreateSemaphore(this->device, &semaphore_info, nullptr, &this->frames_intl[i].render_semaphore));
                    VK_CHECK(vkCreateFence(this->device, &fence_info, nullptr, &this->frames_intl[i].render_fence));
                }
            }
        }

        void Renderer::create_swapchain(uint32_t width, uint32_t height)
        {
            this->swapchain.format = VK_FORMAT_R8G8B8A8_SRGB;
            vkb::SwapchainBuilder swapchain_builder(this->physical_device, this->device, this->surface);
            VkSurfaceFormatKHR surface_format = {
                .format = this->swapchain.format,
                .colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
            };

            vkb::Swapchain vkb_swapchain = swapchain_builder.set_desired_format(surface_format)
                                                    .set_desired_present_mode(VK_PRESENT_MODE_FIFO_KHR)
                                                    .set_desired_extent(width, height)
                                                    .add_image_usage_flags(VK_IMAGE_USAGE_TRANSFER_DST_BIT)
                                                    .build()
                                                    .value();

            this->swapchain.extent = vkb_swapchain.extent;
            this->swapchain.handle = vkb_swapchain.swapchain;
            this->swapchain.images = vkb_swapchain.get_images().value();
            this->swapchain.views = vkb_swapchain.get_image_views().value();
        }

        void Renderer::destroy_swapchain()
        {
            vkDestroySwapchainKHR(this->device, this->swapchain.handle, nullptr);

            for(const VkImageView& view : this->swapchain.views)
            {
                vkDestroyImageView(this->device, view, nullptr);
            }
        }

        void Renderer::init_imgui()
        {
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

            VK_CHECK(vkCreateDescriptorPool(this->device, &imgui_pool_info, nullptr, &this->imgui_pool));

            ImGui_ImplVulkan_InitInfo imgui_info = {
                .ApiVersion = VK_API_VERSION_1_3,
                .Instance = this->instance,
                .PhysicalDevice = this->physical_device,
                .Device = this->device,
                .QueueFamily = this->graphics_queue.family,
                .Queue = this->graphics_queue.handle,
                .DescriptorPool = this->imgui_pool,
                .MinImageCount = static_cast<uint32_t>(this->swapchain.images.size()),
                .ImageCount = static_cast<uint32_t>(this->swapchain.images.size()),
                .MSAASamples = VK_SAMPLE_COUNT_1_BIT,
                .PipelineCache = VK_NULL_HANDLE,
                .UseDynamicRendering = true
            };

            imgui_info.PipelineRenderingCreateInfo = {
                .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
                .colorAttachmentCount = 1,
                .pColorAttachmentFormats = &this->swapchain.format
            };

            ImGui_ImplVulkan_Init(&imgui_info);
            ImGui_ImplVulkan_CreateFontsTexture();
        }

        void Renderer::deinit_vulkan()
        {

            for(int i = 0; i < FRAME_FLIGHT_COUNT; i++)
            {
                vkDestroyCommandPool(this->device, this->frames_intl[i].pool, nullptr);
                vkDestroySemaphore(this->device, this->frames_intl[i].render_semaphore, nullptr);
                vkDestroySemaphore(this->device, this->frames_intl[i].swapchain_semaphore, nullptr);
                vkDestroyFence(this->device, this->frames_intl[i].render_fence, nullptr);
            }

            vmaDestroyAllocator(this->allocator);
            vkDestroyDevice(this->device, nullptr);
            vkb::destroy_debug_utils_messenger(this->instance, this->debug_messenger);
            vkDestroySurfaceKHR(this->instance, this->surface, nullptr);
            vkDestroyInstance(this->instance, nullptr);
        }

        void Renderer::deinit_imgui()
        {
            ImGui_ImplVulkan_Shutdown();
            ImGui_ImplGlfw_Shutdown();
            ImGui::DestroyContext();
            vkDestroyDescriptorPool(this->device, this->imgui_pool, nullptr);
        }


        void Renderer::load_material(std::vector<MaterialTextureBinding> texture_bindings)
        {

            /*
                create_descriptor_layout() at startup for each type of material we want (maybe lazily make)

                create_pipeline_if_needed() at startup for each type of material we want (maybe lazily make)

                allocate_descriptor()

                allocate_resources()

                update_descriptor()

                add_material_to_vector_or_something()
            */
           
            VkDescriptorSet mat_set = material_set_allocator.allocate(this->device, this->phong_material_layout);

            if(texture_bindings.size() < 1 && texture_bindings[0].type != MaterialTextureType::DIFFUSE)
            {
                std::cout << "TEST: first texture was not a diffuse" << std::endl;
                return;
            }

            assert(texture_bindings[0].data != nullptr);

            Image diffuse_texture = Vulkan::create_image(this->device, this->allocator, VkExtent3D{texture_bindings[0].width, texture_bindings[0].height, 1}, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_USAGE_SAMPLED_BIT);



        }

        // For now this will directly call the render commands and stuff
        // But when the renderer is done this will actually just sort the nodes based on the material and then add them to a queue to draw
        // Then they will actually be draw in the present method
        void Renderer::draw(const SceneNode& node)
        {
            VkCommandBuffer cmd = this->frames[this->frame_count].cmd;
            for(const Mesh& mesh : node.meshes)
            {
                // Bind material

                // Bind vertices
                VkDeviceSize sizes[] = {0};
                vkCmdBindVertexBuffers(cmd, 0, 1, &mesh.vertices.handle, sizes);
                vkCmdBindIndexBuffer(cmd, mesh.indices.handle, 0, VK_INDEX_TYPE_UINT32);
                vkCmdDrawIndexed(cmd, mesh.index_count, 1, 0, 0, 0);
            }

            for(const SceneNode& child : node.children)
            {
                draw(child);
            }
        }

        void Renderer::draw(const Buffer& vertex, const Buffer& index, uint32_t index_count)
        {
            VkCommandBuffer cmd = this->frames[this->frame_count].cmd;
            vkCmdBindVertexBuffers(cmd, 0, 1, &vertex.handle, {});
            vkCmdBindIndexBuffer(cmd, index.handle, 0, VK_INDEX_TYPE_UINT32);
            vkCmdDrawIndexed(cmd, index_count, 1, 0, 0, 0);
        }

        void Renderer::present()
        {
            InternalFrameData* internal_data = &this->frames_intl[this->frame_count];
            FrameData* frame = &this->frames[this->frame_count];

            //TODO: Refactor this when ready so start_frame is actually "called" here. In other words, put the start frame code here instead of in it's own function.
            // When an object is submitted to the renderer to draw, it will be added to a queue that will then get read here and all the necessary things will all be draw in this function.
            // The draw function doesn't actually "draw" the objects. Instead, it just submits them to be drawn later in this function

            // Start command buffer

            // Render all stuff that was submitted to renderer

            vkCmdEndRendering(frame->cmd);

            draw_gui();

            // Transition image to presentable state
            Vulkan::Cmd::transition_image(frame->cmd, this->swapchain.images[frame->swapchain_index], {VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, 
                                    VK_ACCESS_2_NONE, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR}, 
                                    { VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS });

            // Then do the rest
            VK_CHECK(vkEndCommandBuffer(frame->cmd));

            // Submit the commands to the command buffer
            VkCommandBufferSubmitInfo cmd_submit_info = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
                .commandBuffer = frame->cmd,
                .deviceMask = 0
            };

            VkSemaphoreSubmitInfo wait_info = {
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                .semaphore = internal_data->swapchain_semaphore,
                .value = 1,
                .stageMask = VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR,
                .deviceIndex = 0
            };

            VkSemaphoreSubmitInfo signal_info = {
                .sType = VK_STRUCTURE_TYPE_SEMAPHORE_SUBMIT_INFO,
                .semaphore = internal_data->render_semaphore,
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
            VK_CHECK(vkQueueSubmit2(this->graphics_queue.handle, 1, &submit_info, internal_data->render_fence));

            VkPresentInfoKHR present_info = {
                .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
                .waitSemaphoreCount = 1,
                .pWaitSemaphores = &internal_data->render_semaphore,
                .swapchainCount = 1,
                .pSwapchains = &this->swapchain.handle,
                .pImageIndices = &frame->swapchain_index
            };

            VK_CHECK(vkQueuePresentKHR(this->graphics_queue.handle, &present_info));
            this->frame_count += (this->frame_count + 1) % FRAME_FLIGHT_COUNT;
        }

        // Temporary
        FrameData Renderer::start_render()
        {
            const InternalFrameData* frame_data = &this->frames_intl[this->frame_count];
            FrameData* frame_context = &this->frames[this->frame_count];

            // Wait for fence to be signaled (if first loop fence is already signaled)
            VK_CHECK(vkWaitForFences(this->device, 1, &frame_data->render_fence, true, UINT64_MAX));
            // Here Fence is signaled

            // Reset Fence to unsignaled
            VK_CHECK(vkResetFences(this->device, 1, &frame_data->render_fence));

            // Acquire swapchain image. Swapchain_semaphore will be signaled once it has been acquired
            VK_CHECK(vkAcquireNextImageKHR(this->device, this->swapchain.handle, UINT64_MAX, frame_data->swapchain_semaphore, nullptr, &frame_context->swapchain_index));


            VK_CHECK(vkResetCommandBuffer(frame_context->cmd, 0));

            VkCommandBufferBeginInfo cmd_info = {
                .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
            };

            VK_CHECK(vkBeginCommandBuffer(frame_context->cmd, &cmd_info));

            // Get swapchain image, transition it to renderable format, start rendering...
            Vulkan::Cmd::transition_image(frame_context->cmd, this->swapchain.images[frame_context->swapchain_index], {VK_ACCESS_2_NONE, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, 
                                    VK_ACCESS_2_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_2_COLOR_ATTACHMENT_READ_BIT, VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT, 
                                    VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL}, { VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS });
            Vulkan::Cmd::transition_image(frame_context->cmd, this->depth_buffer.handle, {VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_2_LATE_FRAGMENT_TESTS_BIT, 
                                    VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_2_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT, VK_PIPELINE_STAGE_2_EARLY_FRAGMENT_TESTS_BIT, 
                                    VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL}, {VK_IMAGE_ASPECT_DEPTH_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS});
            
            {
                VkRenderingAttachmentInfo color_attachment_info = {
                    .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                    .imageView = this->swapchain.views[frame_context->swapchain_index],
                    .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                    .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                    .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                    .clearValue = {
                        .color = {0.1, 0.1, 0.1, 1.0}
                    }
                };

                VkRenderingAttachmentInfo depth_attachment_info = {
                    .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                    .imageView = this->depth_buffer.view,
                    .imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
                    .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
                    .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                    .clearValue = {
                        .depthStencil = {.depth = 1.0f}
                    }
                };

                VkRenderingInfo render_info = {
                    .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
                    .renderArea = VkRect2D{ {0, 0}, {this->swapchain.extent.width, this->swapchain.extent.height} },
                    .layerCount = 1,
                    .colorAttachmentCount = 1,
                    .pColorAttachments = &color_attachment_info,
                    .pDepthAttachment = &depth_attachment_info
                };

                vkCmdBeginRendering(frame_context->cmd, &render_info);
            }


            // Temporary
            vkCmdBindPipeline(frame_context->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, this->phong_pipeline.handle);

            VkViewport viewport = {
                .x = 0,
                .y = 0,
                .width = (float)this->swapchain.extent.width,
                .height = (float)this->swapchain.extent.height,
                .minDepth = 0.0f,
                .maxDepth = 1.0f
            };
            VkRect2D scissor = {};
            scissor.extent = this->swapchain.extent;
            scissor.offset = VkOffset2D{0, 0};
            vkCmdSetViewport(frame_context->cmd, 0, 1, &viewport);
            vkCmdSetScissor(frame_context->cmd, 0, 1, &scissor);

            VkDescriptorSet sets[] = { this->global_set, this->error_material };
            vkCmdBindDescriptorSets(frame_context->cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, this->phong_pipeline.layout, 0, 2, sets, 0, nullptr);
    
            glm::mat4 model_mat = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, 0.0f));
            //model_mat = glm::rotate(model_mat, glm::radians(angle), glm::vec3(1.0f, 1.0f, 1.0f));
            //model_mat = glm::scale(model_mat, glm::vec3(0.25f));

            PushConstants push_constants = 
            {
                .model = model_mat,
                .norm_mat = glm::transpose(glm::inverse(model_mat))
            };

            vkCmdPushConstants(frame_context->cmd, this->phong_pipeline.layout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(PushConstants), &push_constants);


            return *frame_context;
        }

        Buffer Renderer::create_buffer(void* data, uint64_t size, VkBufferUsageFlags usage)
        {
            return Vulkan::create_buffer(this->device, { this->transfer_cmd, this->transfer_queue.handle, this->transfer_fence }, this->allocator, data, size, usage);
        }

        void Renderer::destroy_buffer(Buffer& buffer)
        {
            Vulkan::destroy_buffer(this->allocator, buffer);
        }
        
        Image Renderer::create_image(void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage)
        {
            return Vulkan::create_image(this->device, this->allocator, {this->transfer_cmd, this->transfer_queue.handle, this->transfer_fence}, data, size, format, usage);
        }

        void Renderer::destroy_image(Image& image)
        {
            Vulkan::destroy_image(this->device, this->allocator, image);
        }

        void Renderer::draw_gui()
        {

            // Temporary
            ImGui_ImplVulkan_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();
            ImGui::ShowDemoWindow();
            ImGui::Render();

            FrameData* frame = &this->frames[this->frame_count];
            VkRenderingAttachmentInfo imgui_attachment = {
                .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
                .imageView = this->swapchain.views[frame->swapchain_index],
                .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                .loadOp = VK_ATTACHMENT_LOAD_OP_LOAD,
                .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
                .clearValue = {
                    .color = {0.0, 0.0, 0.0, 1.0}
                }
            };

            VkRenderingInfo imgui_render_info = {
                .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
                .renderArea = VkRect2D{ {0, 0}, {this->swapchain.extent.width, this->swapchain.extent.height} },
                .layerCount = 1,
                .colorAttachmentCount = 1,
                .pColorAttachments = &imgui_attachment
            }; 

            vkCmdBeginRendering(frame->cmd, &imgui_render_info);
            ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), frame->cmd);
            vkCmdEndRendering(frame->cmd);
        }
    }
}