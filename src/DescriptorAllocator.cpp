#include "DescriptorAllocator.h"
#include "vulkan_backend.h"

DescriptorAllocator::DescriptorAllocator()
:current_pool(VK_NULL_HANDLE)
{

}

DescriptorAllocator::~DescriptorAllocator()
{

}

void DescriptorAllocator::init_pool(VkDevice device)
{
    VkDescriptorPoolSize pool_sizes[] = 
    {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 5},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 5}
    };

    VkDescriptorPoolCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = 10,
        .poolSizeCount = 2,
        .pPoolSizes = pool_sizes
    };

    VK_CHECK(vkCreateDescriptorPool(device, &info, nullptr, &current_pool));
}

VkDescriptorSet DescriptorAllocator::allocate(VkDevice device, VkDescriptorSetLayout layout)
{
    if(current_pool == VK_NULL_HANDLE) return VK_NULL_HANDLE;
    
    VkDescriptorSetAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = current_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &layout
    };
    VkDescriptorSet set;
    VK_CHECK(vkAllocateDescriptorSets(device, &alloc_info, &set));

    return set;
}

void DescriptorAllocator::clear_descriptors(VkDevice device)
{
    vkResetDescriptorPool(device, current_pool, 0);
}

void DescriptorAllocator::destroy_pool(VkDevice device)
{
    vkDestroyDescriptorPool(device, current_pool, nullptr);
}