#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include "vulkan_backend.h"

class VulkanGraphicsPipeline
{
    private:
        VkPipeline m_pipeline;
        VkPipelineLayout m_layout;
        std::vector<VkVertexInputBindingDescription> m_bindings;
        std::vector<VkVertexInputAttributeDescription> m_attributes;
        std::vector<VkPipelineShaderStageCreateInfo> m_shader_stages;

    public:
        VulkanGraphicsPipeline();
        ~VulkanGraphicsPipeline();

        void set_layout(VkPipelineLayout layout);
        void add_binding(uint32_t binding_index, uint32_t stride, VkVertexInputRate rate);
        void add_attribute(uint32_t binding, uint32_t location, uint32_t offset, VkFormat format);
        void add_shader(VkShaderModule shader, VkShaderStageFlagBits stage); 

        bool compile(const VulkanContext& context);
        void cmd_bind(VkCommandBuffer cmd);
        void destroy(const VulkanContext& context);
};
