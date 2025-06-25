#pragma once
#include <vector>
#include <vulkan/vulkan.h>

class DescriptorLayoutCompiler
{
    private:
        std::vector<VkDescriptorSetLayoutBinding> bindings;

    public:
        DescriptorLayoutCompiler();
        ~DescriptorLayoutCompiler();

        void add_binding(uint32_t binding, VkDescriptorType type, VkShaderStageFlags stage);
        VkDescriptorSetLayout compile(VkDevice device);
};