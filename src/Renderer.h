#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include "DescriptorAllocator.h"
#include "vma.h"
#include "twilight_types.h"

#define FRAME_FLIGHT_COUNT 2

namespace Twilight
{
    namespace Render
    {

        struct FrameData
        {
            VkCommandBuffer cmd;
            uint32_t swapchain_index;
        };

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

                struct InternalFrameData
                {
                    VkCommandPool pool;
                    VkFence render_fence;
                    VkSemaphore render_semaphore, swapchain_semaphore;
                };

                FrameData frames[FRAME_FLIGHT_COUNT];
                InternalFrameData frames_intl[FRAME_FLIGHT_COUNT];
                uint32_t frame_count = 0;

                DescriptorAllocator general_set_allocator;
                DescriptorAllocator material_set_allocator;
                /*std::vector<DescriptorAllocator> material_set_allocators;*/       // Have a separate allocator per material type (i.e. PBR has it's own allocator, PHONG has an allocator, and so on)

                VkDescriptorPool imgui_pool;


                // Temporary
                VkDescriptorSetLayout phong_material_layout;
                VkDescriptorSetLayout global_layout;
                GraphicsPipeline phong_pipeline;
                Image depth_buffer;
                // End Temporary

                VkCommandPool transfer_pool;
                VkCommandBuffer transfer_cmd;
                VkFence transfer_fence;

                std::vector<Buffer> buffers;
                std::vector<Image> images;

                

                void init_vulkan();
                void deinit_vulkan();
                void init_imgui();
                void deinit_imgui();
                void create_swapchain(uint32_t width, uint32_t height);
                void destroy_swapchain();

                void draw_gui();

            public:

                Renderer();
                ~Renderer();

                void init(GLFWwindow* window, uint32_t width, uint32_t height);

                Buffer create_buffer(void* data, uint64_t size, VkBufferUsageFlags usage);
                void destroy_buffer(Buffer& buffer);
                
                Image create_image(void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage);
                void destroy_image(Image& image);
                
                void load_material(std::vector<MaterialTextureBinding> texture_bindings);
                void draw(const SceneNode& node);
                void draw(const Buffer& vertex, const Buffer& index, uint32_t index_count);
                void present();
                void deinit();

                /* Temporary */
                FrameData start_render();
        };
    }
}