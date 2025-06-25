#pragma once
#include <vulkan/vulkan.h>
#include <vector>

class DescriptorAllocator
{
    private:
        VkDescriptorPool current_pool;

    public:
        DescriptorAllocator();
        ~DescriptorAllocator();

        // Will dynamically create more descriptor pools if necessary
        void init_pools(VkDevice device, uint32_t pool_size, std::vector<VkDescriptorPoolSize> descriptor_types);
        void destroy_pool(VkDevice device);
        VkDescriptorSet allocate(VkDevice device, VkDescriptorSetLayout layout);
        void clear_descriptors(VkDevice device);

};