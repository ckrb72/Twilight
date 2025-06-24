#include "Renderer.h"
#include <VkBootstrap.h>
#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_vulkan.h"

#include "GraphicsPipelineCompiler.h"
#include "render_util.h"
#include "render_backend.h"
#include <fstream>

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
                    .descriptorCount = 0,
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


            // FIXME: Temporary transfer queue test
            uint8_t data[4] = { 255, 255, 255, 255 };

            Image test_image = create_image(device, allocator, {this->transfer_cmd, this->transfer_queue.handle, this->transfer_fence}, data, VkExtent3D{1, 1, 1}, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_USAGE_SAMPLED_BIT);

            vkDestroyImageView(this->device, test_image.view, nullptr);
            vmaDestroyImage(this->allocator, test_image.handle, test_image.allocation);
        }

        void Renderer::deinit()
        {
            vkDestroyFence(this->device, this->transfer_fence, nullptr);
            vkDestroyCommandPool(this->device, this->transfer_pool, nullptr);
            vkDestroyDescriptorSetLayout(this->device, this->phong_material_layout, nullptr);
            vkDestroyDescriptorSetLayout(this->device, this->global_layout, nullptr);
            vkDestroyPipeline(this->device, this->phong_pipeline.handle, nullptr);
            vkDestroyPipelineLayout(this->device, this->phong_pipeline.layout, nullptr);


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


        void Renderer::load_material()
        {
            VkDescriptorSet mat_set = material_set_allocator.allocate(this->device, this->phong_material_layout);

            

        }

    }
}