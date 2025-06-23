#pragma once
#include <GLFW/glfw3.h>
#include "DescriptorAllocator.h"

namespace Twilight
{
    namespace Render
    {

        struct Buffer;
        struct Image;

        /*struct Material
        {

        };*/

        enum MaterialType
        {
            MAT_FLOAT,
            MAT_INT32,
            MAT_UINT32,
            MAT_DOUBLE,
            MAT_TEXTURE
        };

        class Renderer
        {
            private:
                // Swapchain
                // GPU
                // Device
                // FrameData
                // Other Context related stuff...
                VkDevice device;
                VkPhysicalDevice physical_device;

                DescriptorAllocator general_set_allocator;
                DescriptorAllocator material_set_allocator;
                /*std::vector<DescriptorAllocator> material_set_allocators;*/       // Have a separate allocator per material type (i.e. PBR has it's own allocator, PHONG has an allocator, and so on)

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