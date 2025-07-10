#include <iostream>
#include "render/Renderer.h"
#include "physics/PhysicsWorld.h"
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

/*
    TODO: 
        PBR Materials
        Scene graph (add lights and stuff)
        Camera movement
        Jolt integration
*/

const int WIN_WIDTH = 1920, WIN_HEIGHT = 1080;

// Temporary
void free_node(Twilight::Render::Renderer* renderer, std::shared_ptr<Twilight::SceneNode> node)
{
    for(Twilight::Render::Mesh& mesh : node->meshes)
    {
        renderer->destroy_mesh(mesh);
    }

    for(std::shared_ptr<Twilight::SceneNode> child : node->children)
    {
        free_node(renderer, child);
    }
}

void print_matrices(const std::shared_ptr<Twilight::SceneNode> node)
{
    for(int i = 0; i < 4; i++)
    {
        for(int j = 0; j < 4; j++)
        {
            std::cout << node->local_transform[i][j] << " ";
        }
        std::cout << std::endl;
    }
    std::cout << std::endl;

    for(const std::shared_ptr<Twilight::SceneNode> child : node->children)
    {
        print_matrices(child);
    }
}

glm::mat4 jph_to_glm(JPH::Mat44& mat)
{
    glm::mat4 out_mat = {};
    for(int c = 0; c < 4; c++)
    {
        JPH::Vec4 col = mat.GetColumn4(c);
        out_mat[c][0] = col[0];
        out_mat[c][1] = col[1];
        out_mat[c][2] = col[2];
        out_mat[c][3] = col[3];
    }

    return out_mat;
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

    std::shared_ptr<Twilight::SceneNode> little_guy = asset_manager.load_model("../little-guy.glb");
    std::shared_ptr<Twilight::SceneNode> helmet = asset_manager.load_model("../DamagedHelmet.glb");
    std::shared_ptr<Twilight::SceneNode> mech = asset_manager.load_model("../assets/halo_infinite_oddball.glb");

    Twilight::Physics::PhysicsWorld world;
    world.init();


    double delta = 0.0f;
    double previous_time = glfwGetTime();

    float angle = 0.0f;

    Twilight::Scene::AppendChild(little_guy, helmet);
    Twilight::Scene::SetTransform(helmet, glm::translate(glm::mat4(1.0f), glm::vec3(1.0f)) * glm::rotate(glm::mat4(1.0f), glm::radians(90.0f), glm::vec3(1.0f, 0.0f, 0.0f)));

    while(!glfwWindowShouldClose(window))
    {
        double current_time = glfwGetTime();
        delta = current_time - previous_time;
        previous_time = current_time;

        glfwPollEvents();
        
        world.update(delta);

        JPH::Mat44 box_transform_jph = world.get_transform_test();
        glm::mat4 box_transform = jph_to_glm(box_transform_jph);

        Twilight::Scene::SetTransform(little_guy, box_transform);
        
        //renderer.draw(mech);
        renderer.draw(little_guy);
        renderer.draw(helmet);

        renderer.present();
    }
    world.deinit();
    
    renderer.wait();
    free_node(&renderer, little_guy);
    free_node(&renderer, helmet);
    free_node(&renderer, mech);

    renderer.deinit();

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}