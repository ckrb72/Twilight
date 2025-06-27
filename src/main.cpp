#include <iostream>
#include "render/Renderer.h"
#include "AssetManager.h"
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
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

void print_matrices(const Twilight::Render::Model& model)
{
    for(int i = 0; i < 4; i++)
    {
        for(int j = 0; j < 4; j++)
        {
            std::cout << model.local_transform[i][j] << " ";
        }
        std::cout << std::endl;
    }
    std::cout << std::endl;

    for(const Twilight::Render::Model& child : model.children)
    {
        print_matrices(child);
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
    //Twilight::Render::Model mech = asset_manager.load_model("../rx-88.glb");
    /* Twilight::Render::Material material = renderer->create_material(material_stuff) */
    /* renderer->bind_material(material)*/

    //renderer.add_light({glm::vec3(1.0f, 1.0f, 1.0f), glm::vec3(1.0f, 1.0f, 1.0f)});
    //renderer.set_light_color(light_id, glm::vec3(1.0f, 0.0f, 0.0f));

    double delta = 0.0f;
    double previous_time = glfwGetTime();

    float angle = 0.0f;

    while(!glfwWindowShouldClose(window))
    {
        double current_time = glfwGetTime();
        delta = current_time - previous_time;
        previous_time = current_time;

        glfwPollEvents();

        //helmet.local_transform = glm::rotate(glm::mat4(1.0f), glm::radians(10.0f * (float)delta), glm::vec3(0.0f, 1.0f, 0.0f)) * helmet.local_transform;
        //little_guy.local_transform = glm::rotate(glm::mat4(1.0f), glm::radians(50.0f * (float)delta), glm::vec3(0.0f, 1.0f, 0.0f)) * little_guy.local_transform;
        //renderer.draw(mech);
        renderer.draw(little_guy);
        renderer.draw(helmet);

        renderer.present();
    }
    // Fix free_node up so it actually frees the buffers at the correct time
    free_node(&renderer, little_guy);
    free_node(&renderer, helmet);
    //free_node(&renderer, mech);
    renderer.deinit();

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}