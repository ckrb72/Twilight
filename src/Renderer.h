#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include "DescriptorAllocator.h"
#include "vma.h"
#include "twilight_types.h"

namespace Twilight
{
    namespace Render
    {
        class Renderer
        {
            private:
                VkInstance instance;
                VkDevice device;
                VkPhysicalDevice physical_device;
                VmaAllocator allocator;
                VkSurfaceKHR surface;
                GLFWwindow* window = nullptr;
                VkDebugUtilsMessengerEXT debug_messenger;

                struct Swapchain
                {
                    VkFormat format;
                    VkExtent2D extent;
                    VkSwapchainKHR handle;
                    std::vector<VkImage> images;
                    std::vector<VkImageView> views;
                };

                Swapchain swapchain;

                struct Queue
                {
                    VkQueue handle;
                    uint32_t family;
                };

                Queue graphics_queue;
                Queue transfer_queue;

                DescriptorAllocator general_set_allocator;
                DescriptorAllocator material_set_allocator;
                /*std::vector<DescriptorAllocator> material_set_allocators;*/       // Have a separate allocator per material type (i.e. PBR has it's own allocator, PHONG has an allocator, and so on)

                VkDescriptorPool imgui_pool;


                // Temporary
                VkDescriptorSetLayout phong_material_layout;
                VkDescriptorSetLayout global_layout;

                GraphicsPipeline phong_pipeline;

                VkCommandPool transfer_pool;
                VkCommandBuffer transfer_cmd;
                VkFence transfer_fence;

                

                void init_vulkan();
                void deinit_vulkan();
                void init_imgui();
                void deinit_imgui();
                void create_swapchain(uint32_t width, uint32_t height);
                void destroy_swapchain();

            public:

                Renderer();
                ~Renderer();

                void init(GLFWwindow* window, uint32_t width, uint32_t height);
                void load_material();
                //void draw_node(const SceneNode& node);
                void present();
                void deinit();
        };
    }
}