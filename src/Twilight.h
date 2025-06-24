#pragma once
#include "AssetManager.h"

namespace Twilight
{
    class Twilight
    {
        private:
            AssetManager asset_manager;
            /*Window window;*/  /* <= holds width, height and sends events each frame to the event manager */
            /*Renderer::VulkanRenderer renderer*/ /* <= The thing that communicates with the gpu (if something needs to talk to gpu then goes through renderer)*/
            /*Event::EventManager event_manager;*/ /* <= Recieves event messages and sends out event notifications to all listening actors*/
            /*Physics::PhysicsWorld physics_world;*/ /* <= Handles all physics in the engine */
        public:
            Twilight();
            ~Twilight();

            void init();
            void deinit();
    };
}