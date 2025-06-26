#include <iostream>
#include "render/Renderer.h"
#include "AssetManager.h"

/*
    Materials will have the texture, colors, etc. as usual but will also have a specific compiled pipeline (and layout)
    associted with them. So if you want to draw all the meshes with a certain material, you bind that pipeline and then do all the descriptors and drawing and stuff...
    Internally, the renderer will holds lists of objects with their associated materials for drawing. When you ask to draw a node, the respective meshes are places in the correct material
    lists and then when rendering actually occurs all those lists are renderered one by one
*/

const int WIN_WIDTH = 1920, WIN_HEIGHT = 1080;

// Temporary
void free_node(Twilight::Render::Renderer* renderer, Twilight::Render::Model& node)
{
    for(Twilight::Render::Mesh& mesh : node.meshes)
    {
        renderer->destroy_mesh(mesh);
    }

    for(Twilight::Render::Model& child : node.children)
    {
        free_node(renderer, child);
    }
}

int main()
{
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(WIN_WIDTH, WIN_HEIGHT, "Graphic Demo", nullptr, nullptr);

    Twilight::Render::Renderer renderer;
    renderer.init(window, WIN_WIDTH, WIN_HEIGHT);

    Twilight::AssetManager asset_manager;
    asset_manager.init(&renderer);
    Twilight::Render::Model little_guy = asset_manager.load_model("../little-guy.glb");
    Twilight::Render::Model helmet = asset_manager.load_model("../DamagedHelmet.glb");

    /* Twilight::Render::Material material = renderer->create_material(material_stuff) */
    /* renderer->bind_material(material)*/

    while(!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        renderer.draw(little_guy);
        renderer.draw(helmet);

        renderer.present();
    }
    // Fix free_node up so it actually frees the buffers at the correct time
    free_node(&renderer, little_guy);
    free_node(&renderer, helmet);
    renderer.deinit();

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}