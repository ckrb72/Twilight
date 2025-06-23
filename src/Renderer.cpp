#include "Renderer.h"
#include <VkBootstrap.h>
#include <vulkan/vk_enum_string_helper.h>
#include <iostream>
#include "imgui/imgui.h"
#include "imgui/imgui_impl_glfw.h"
#include "imgui/imgui_impl_vulkan.h"

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
        }

        void Renderer::deinit()
        {

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
            VkDescriptorSetLayoutBinding texture = {
                .binding = 0,
                .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
                .descriptorCount = 1,
                .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT
            };


            VkDescriptorSetLayoutCreateInfo layout_info = {
                .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
                .bindingCount = 1,
                .pBindings = &texture
            };

            VkDescriptorSetLayout descriptor_layout;
            VK_CHECK(vkCreateDescriptorSetLayout(this->device, &layout_info, nullptr, &descriptor_layout));
        }

    }
}