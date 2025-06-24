#pragma once
#include <vulkan/vulkan.h>
#include <vector>
#include "twilight_types.h"

class GraphicsPipelineCompiler
{
    private:
        VkPipelineLayout m_layout;
        std::vector<VkVertexInputBindingDescription> m_bindings;
        std::vector<VkVertexInputAttributeDescription> m_attributes;
        std::vector<VkPipelineShaderStageCreateInfo> m_shader_stages;
        std::vector<VkFormat> color_formats;

    public:
        GraphicsPipelineCompiler();
        ~GraphicsPipelineCompiler();

        void set_layout(VkPipelineLayout layout);
        void set_color_formats(std::vector<VkFormat> formats);
        void add_binding(uint32_t binding_index, uint32_t stride, VkVertexInputRate rate);
        void add_attribute(uint32_t binding, uint32_t location, uint32_t offset, VkFormat format);
        void add_shader(VkShaderModule shader, VkShaderStageFlagBits stage); 

        Twilight::Render::GraphicsPipeline compile(VkDevice device);
};
