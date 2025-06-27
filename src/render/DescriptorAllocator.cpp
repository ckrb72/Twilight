#include "DescriptorAllocator.h"
#include "render_util.h"

DescriptorAllocator::DescriptorAllocator()
:current_pool(VK_NULL_HANDLE)
{

}

DescriptorAllocator::~DescriptorAllocator()
{

}

void DescriptorAllocator::init_pools(VkDevice device, uint32_t pool_size, std::vector<VkDescriptorPoolSize> descriptor_types)
{

    this->current_pool = allocate_pool(device, pool_size, descriptor_types);
    this->device = device;
    this->pool_size = pool_size;
    this->descriptor_types = descriptor_types;
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
    if (vkAllocateDescriptorSets(device, &alloc_info, &set) == VK_ERROR_OUT_OF_POOL_MEMORY)
    {
        pools.push_back(current_pool);
        current_pool = allocate_pool(device, pool_size, descriptor_types);
        alloc_info.descriptorPool = current_pool;

        VK_CHECK(vkAllocateDescriptorSets(device, &alloc_info, &set));
    }

    return set;
}

void DescriptorAllocator::clear_descriptors(VkDevice device)
{
    for(const VkDescriptorPool& pool : pools)
    {
        vkResetDescriptorPool(device, pool, 0);
    }

    vkResetDescriptorPool(device, current_pool, 0);
}

void DescriptorAllocator::destroy_pool(VkDevice device)
{
    for(const VkDescriptorPool& pool : pools)
    {
        vkDestroyDescriptorPool(device, pool, nullptr);
    }

    vkDestroyDescriptorPool(device, current_pool, nullptr);
}

VkDescriptorPool DescriptorAllocator::allocate_pool(VkDevice device, uint32_t pool_size, std::vector<VkDescriptorPoolSize> descriptor_types)
{
    VkDescriptorPoolCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = pool_size,
        .poolSizeCount = static_cast<uint32_t>(descriptor_types.size()),
        .pPoolSizes = descriptor_types.data()
    };

    VkDescriptorPool pool;
    VK_CHECK(vkCreateDescriptorPool(device, &info, nullptr, &pool));

    return pool;
}