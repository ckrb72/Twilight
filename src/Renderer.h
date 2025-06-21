#pragma once
#include <GLFW/glfw3.h>

namespace Twilight
{
    namespace Renderer
    {

        struct Buffer
        {
            // Stuff for buffers
        };

        struct Image
        {
            // Stuff for images
        };

        class Renderer
        {
            private:
                // Swapchain
                // GPU
                // Device
                // FrameData
                // Other Context related stuff...

            public:

                Renderer();
                ~Renderer();

                void init(GLFWwindow* window, uint32_t width, uint32_t height);
                //void draw_node(const SceneNode& node);
                void present();
                void deinit();
        };
    }
}