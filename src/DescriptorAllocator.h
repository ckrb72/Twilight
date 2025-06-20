#pragma once
#include <vulkan/vulkan.h>

class DescriptorAllocator
{
    private:
        VkDescriptorPool current_pool;

    public:
        DescriptorAllocator();
        ~DescriptorAllocator();

        // Will dynamically create more descriptor pools if necessary
        void init_pool(VkDevice device);
        void destroy_pool(VkDevice device);
        VkDescriptorSet allocate(VkDevice device, VkDescriptorSetLayout layout);
        void clear_descriptors(VkDevice device);

};