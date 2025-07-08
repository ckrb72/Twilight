#include "Scene.h"
#include <iostream>

namespace Twilight
{
    namespace Scene
    {
        void SetTransform(SceneNode& node, const glm::mat4& transform)
        {
            node.world_matrix = transform * node.local_transform;
            for(SceneNode& child : node.children)
            {
                SetTransform(child, node.world_matrix);
            }
        }

        void AppendChild(SceneNode& parent, SceneNode& new_child)
        {
            std::cout << "Not Implemented" << std::endl;
        }
    }
}