#include "DescriptorLayoutCompiler.h"
#include "vulkan_backend.h"

DescriptorLayoutCompiler::DescriptorLayoutCompiler()
{

}

DescriptorLayoutCompiler::~DescriptorLayoutCompiler()
{

}

void DescriptorLayoutCompiler::add_binding(uint32_t binding, VkDescriptorType type, VkShaderStageFlags stage)
{
    VkDescriptorSetLayoutBinding binding_info = {
        .binding = binding,
        .descriptorType = type,
        .descriptorCount = 1,
        .stageFlags = stage,
    };

    bindings.push_back(binding_info);
}


VkDescriptorSetLayout DescriptorLayoutCompiler::compile(VkDevice device)
{
    VkDescriptorSetLayoutCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = (uint32_t)bindings.size(),
        .pBindings = bindings.data(),
    };

    VkDescriptorSetLayout layout;
    VK_CHECK(vkCreateDescriptorSetLayout(device, &create_info, nullptr, &layout));

    return layout;
}