#include "Scene.h"
#include <iostream>

namespace Twilight
{
    namespace Scene
    {
        // Kind of hacky the way this is implemented but have this in a separate function
        // so we don't have to keep looking up the parent nodes which is what the other SetTransform does
        // I'll figure out a better way to do this in a little
        static void WalkGraphUpdateTransforms(std::shared_ptr<SceneNode> node, const glm::mat4& transform)
        {
            /* node.world_matrix = node.parent->world_matrix * node.local_transform*/
            node->world_matrix = transform * node->local_transform;
            for(std::shared_ptr<SceneNode> child : node->children)
            {
                WalkGraphUpdateTransforms(child, node->world_matrix);
            }
        }

        void SetTransform(std::shared_ptr<SceneNode> node, const glm::mat4& transform)
        {
            node->local_transform = transform;
            node->world_matrix = node->local_transform;
            if(node->parent != nullptr)
            {
                std::cout << "Got Here" << std::endl;
                node->world_matrix = node->parent->world_matrix * node->world_matrix;
            }

            for(std::shared_ptr<SceneNode> child : node->children)
            {
                WalkGraphUpdateTransforms(child, node->world_matrix);
            }
        }

        // Make node a child of new_parent
        void AppendChild(std::shared_ptr<SceneNode> new_parent, std::shared_ptr<SceneNode> node)
        {
            std::cout << "Got Here" << std::endl;

            // Remove child from parent's children (horrible and disgusting but will fix this when doing a more data oriented design)
            if(node->parent != nullptr)
            {
                std::shared_ptr<SceneNode> old_parent = node->parent;
                old_parent->children.erase(std::remove(old_parent->children.begin(), old_parent->children.end(), node), old_parent->children.end());
            }
            
            //Update transforms so they are correct for new_node and it's children
            node->world_matrix = new_parent->world_matrix * node->local_transform;
            for(std::shared_ptr<SceneNode> child : node->children)
            {
                WalkGraphUpdateTransforms(child, node->world_matrix);
            }
            
            new_parent->children.push_back(node);
            node->parent = new_parent;
        }

        // Make node a sibling of sibling
        void AppendSibling(std::shared_ptr<SceneNode> sibling, std::shared_ptr<SceneNode> node)
        {
            // Trying to append to a sibling with no parent (not allowed)
            if(sibling->parent == nullptr) return;

            // Make new sibling's parent the same as node's parent
            if(node->parent == nullptr)
            {
                std::shared_ptr<SceneNode> old_parent = node->parent;
                old_parent->children.erase(std::remove(old_parent->children.begin(), old_parent->children.end(), node), old_parent->children.end());
            }

            node->parent = sibling->parent;
            node->world_matrix = node->parent->world_matrix * node->local_transform;

            for(std::shared_ptr<SceneNode> child : node->children)
            {
                WalkGraphUpdateTransforms(child, node->world_matrix);
            }

            sibling->parent->children.push_back(node);
        }
    }
}