#pragma once
#include "AssetManager.h"
#include "render/Renderer.h"

namespace Twilight
{
    class Twilight
    {
        private:
            /*Window window;*/  /* <= holds width, height and sends events each frame to the event manager */
            Render::Renderer renderer; /* <= The thing that communicates with the gpu (if something needs to talk to gpu then goes through renderer)*/
            AssetManager asset_manager;
            /*Event::EventManager event_manager;*/ /* <= Recieves event messages and sends out event notifications to all listening actors*/
            /*Physics::PhysicsWorld physics_world;*/ /* <= Handles all physics in the engine */
        public:
            Twilight();
            ~Twilight();

            void init();
            void deinit();
    };
}