#pragma once
#include <vector>
#include "twilight_types.h"
#include <glm/glm.hpp>

namespace Twilight
{
    // Very basic node right now that just has meshes because I will be changing up the scene graph later
    struct SceneNode
    {
        std::vector<Render::Mesh> meshes;
        std::vector<SceneNode> children;
        glm::mat4 local_transform;
        glm::mat4 world_matrix;
    };

    namespace Scene
    {
        void SetTransform(SceneNode& node, const glm::mat4& transform);
        void AppendChild(SceneNode& parent, SceneNode& new_child);
    }
}