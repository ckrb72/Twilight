#pragma once
#include <vulkan/vulkan.h>
#include "vulkan_backend.h"

struct Material
{
    GraphicsPipeline* pipeline;
    VkDescriptorSet descriptor_set;
    VulkanBuffer descriptor_buffer;     // This will be uploaded with all the data needed at startup / when the material is loaded from disk
};


// When allocating a material, create the descriptor set with all of the various things it needs (i.e. allocate the buffers for the textures, constants, etc. and the descriptor sets to point to them).
// When binding the material, use the pipeline and bind the descriptor set and stuff

/*
    For example, using a phong material with only a diffuse texture you would create a descriptor set that holds a single texture, allocate the texture and stuff and then just hold it.
    Then, when you are going to draw the phong material you would want to bind the descriptor set for that and the pipeline and then draw the object
*/