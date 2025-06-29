#pragma once
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vector>
#include "DescriptorAllocator.h"
#include "vma.h"
#include "../twilight_types.h"

#define FRAME_FLIGHT_COUNT 2

namespace Twilight
{
    namespace Render
    {

        void set_transform(Model& model, const glm::mat4& transform);

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

                // TODO: These two structs should really be one. No need to have them separate anymore
                struct InternalFrameData
                {
                    VkCommandPool pool;
                    VkFence render_fence;
                    VkSemaphore render_semaphore, swapchain_semaphore;
                };
                struct FrameData
                {
                    VkCommandBuffer cmd;
                    VkPipelineLayout layout;
                    uint32_t swapchain_index;
                };

                FrameData frames[FRAME_FLIGHT_COUNT];
                InternalFrameData frames_intl[FRAME_FLIGHT_COUNT];
                uint32_t frame_count = 0;

                DescriptorAllocator general_set_allocator;
                DescriptorAllocator material_set_allocator;
                /*std::vector<DescriptorAllocator> material_set_allocators;*/       // Have a separate allocator per material type (i.e. PBR has it's own allocator, PHONG has an allocator, and so on)

                VkDescriptorPool imgui_pool;


                // Material stuff
                VkDescriptorSetLayout global_layout;
                VkDescriptorSetLayout phong_layout;
                //VkDescriptorSetLayout pbr_layout;
                
                GraphicsPipeline phong_pipeline;
                //GraphicsPipeline pbr_pipeline;
                //GraphicsPipeline transparent_pipeline;

                GraphicsPipeline* bound_pipeline;
                
                Image depth_buffer;

                VkDescriptorSet global_set;
                Buffer global_ubo;

                VkSampler default_sampler;

                VkCommandPool transfer_pool;
                VkCommandBuffer transfer_cmd;
                VkFence transfer_fence;
                
                struct GlobalUbo
                {
                    glm::mat4 projection;
                    glm::mat4 view;
                };

                void init_vulkan();
                void deinit_vulkan();
                void init_imgui();
                void deinit_imgui();
                
                void init_material_layouts();
                void deinit_material_layouts();
                void init_material_pipelines();
                void deinit_material_pipelines();

                void bind_material(const Material& material);

                void create_swapchain(uint32_t width, uint32_t height);
                void destroy_swapchain();
                void destroy_material(Material& material);

                void frame_begin(FrameData* frame, InternalFrameData* internal_data);
                void frame_end(FrameData* frame, InternalFrameData* internal_data);

                void draw_gui();
                void draw(const Model& node, const glm::mat4& parent_transform);


                struct DrawData
                {
                    Mesh mesh;
                    glm::mat4 transform;
                };

                std::vector<DrawData> draw_list;
                std::vector<Material> materials;
                std::vector<Light> lights;

            public:

                Renderer();
                ~Renderer();

                void init(GLFWwindow* window, uint32_t width, uint32_t height);

                Buffer create_buffer(void* data, uint64_t size, VkBufferUsageFlags usage);
                void destroy_buffer(Buffer& buffer);
                
                Image create_image(void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage);
                void destroy_image(Image& image);

                Mesh create_mesh(const std::vector<Vertex>& vertices, const std::vector<unsigned int>& indices, uint32_t mat_id);
                void destroy_mesh(Mesh& mesh);
                
                uint32_t load_material(std::vector<MaterialConstantBinding> constant_bindings, std::vector<MaterialTextureBinding> texture_bindings);
                uint32_t add_light(const Light& light);
                //void remove_light(uint32_t id);
                void draw(const Model& node);
                void draw(const Mesh& mesh);
                void present();
                void wait();
                void deinit();
        };
    }
}