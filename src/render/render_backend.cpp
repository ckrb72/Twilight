#include "render_backend.h"
#include "render_util.h"
#include <fstream>

namespace Twilight
{
    namespace Render
    {
        namespace Vulkan
        {
            Buffer create_buffer(VmaAllocator allocator, uint64_t size, VkBufferUsageFlags usage, VmaMemoryUsage alloc_usage)
            {
                VkBufferCreateInfo buf_info = {
                    .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
                    .size = size,
                    .usage = usage
                };
                VmaAllocationCreateInfo alloc_info = {
                    .flags = VMA_ALLOCATION_CREATE_MAPPED_BIT,
                    .usage = alloc_usage
                };
                Buffer buffer = {};
                VK_CHECK(vmaCreateBuffer(allocator, &buf_info, &alloc_info, &buffer.handle, &buffer.allocation, &buffer.info));
                return buffer;
            }

            Buffer create_buffer(VkDevice device, const TransferInfo& info, VmaAllocator allocator, void* data, uint64_t size, VkBufferUsageFlags usage)
            {
                Buffer staging_buffer = create_buffer(allocator, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
                void* staging_ptr = staging_buffer.info.pMappedData;
                memcpy(staging_ptr, data, size);

                Buffer gpu_buffer = create_buffer(allocator, size, usage | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY);

                transfer_begin(device, info);
                VkBufferCopy copy_info = {
                    .srcOffset = 0,
                    .dstOffset = 0,
                    .size = size,
                };
                vkCmdCopyBuffer(info.cmd, staging_buffer.handle, gpu_buffer.handle, 1, &copy_info);
                transfer_end(device, info);

                vmaDestroyBuffer(allocator, staging_buffer.handle, staging_buffer.allocation);

                return gpu_buffer;
            }

            void destroy_buffer(VmaAllocator allocator, Buffer& buffer)
            {
                if(buffer.handle == VK_NULL_HANDLE) return;
                
                vmaDestroyBuffer(allocator, buffer.handle, buffer.allocation);
                buffer.handle = VK_NULL_HANDLE;
                buffer.allocation = VK_NULL_HANDLE;
                buffer.info = {};
            }

            Image create_image(VkDevice device, VmaAllocator allocator, VkExtent3D size, VkFormat format, VkImageUsageFlags usage)
            {
                VkImageCreateInfo image_info = {
                    .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
                    .imageType = VK_IMAGE_TYPE_2D,
                    .format = format,
                    .extent = size,
                    .mipLevels = 1,
                    .arrayLayers = 1,
                    .samples = VK_SAMPLE_COUNT_1_BIT,
                    .tiling = VK_IMAGE_TILING_OPTIMAL,
                    .usage = usage,
                };

                VmaAllocationCreateInfo alloc_info = {
                    .usage = VMA_MEMORY_USAGE_GPU_ONLY,
                    .requiredFlags = VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
                };

                Image image = { .format = format, .width = size.width, .height = size.height, .depth = size.depth };

                VK_CHECK(vmaCreateImage(allocator, &image_info, &alloc_info, &image.handle, &image.allocation, &image.info));

                VkImageViewCreateInfo view_info = {
                    .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
                    .image = image.handle,
                    .viewType = VK_IMAGE_VIEW_TYPE_2D,
                    .format = format
                };

                VkImageAspectFlags aspect = (usage == VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT) ? VK_IMAGE_ASPECT_DEPTH_BIT : VK_IMAGE_ASPECT_COLOR_BIT;

                view_info.subresourceRange = {
                    .aspectMask = aspect,
                    .baseMipLevel = 0,
                    .levelCount = image_info.mipLevels,
                    .baseArrayLayer = 0,
                    .layerCount = 1
                };

                VK_CHECK(vkCreateImageView(device, &view_info, nullptr, &image.view));
                return image;
            }

            Image create_image(VkDevice device, VmaAllocator allocator, const TransferInfo& info, void* data, VkExtent3D size, VkFormat format, VkImageUsageFlags usage)
            {
                // FIXME: Potential bug if pixel channels != 4
                uint64_t data_size = size.width * size.height * size.depth * 4;
                Buffer staging_buffer = create_buffer(allocator, data_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);
                void* buffer_ptr = staging_buffer.info.pMappedData;
                memcpy(buffer_ptr, data, data_size);

                Image image = create_image(device, allocator, size, format, usage | VK_IMAGE_USAGE_TRANSFER_DST_BIT);

                transfer_begin(device, info);

                {
                    ImageTransitionInfo transition_info = {
                        .src_access = VK_ACCESS_2_NONE,
                        .src_stage = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                        .dst_access = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                        .dst_stage = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                        .old_layout = VK_IMAGE_LAYOUT_UNDEFINED,
                        .new_layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                    };

                    Cmd::transition_image(info.cmd, image.handle, transition_info, { VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1 });
                }

                {
                    VkBufferImageCopy copy_info = {
                        .imageExtent = size
                    };

                    VkImageAspectFlags aspect = VK_IMAGE_ASPECT_COLOR_BIT;

                    copy_info.imageSubresource = {
                        .aspectMask = aspect,
                        .mipLevel = 0,
                        .baseArrayLayer = 0,
                        .layerCount = 1
                    };

                    vkCmdCopyBufferToImage(info.cmd, staging_buffer.handle, image.handle, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy_info);
                }

                {
                    ImageTransitionInfo transition_info = {
                        .src_access = VK_ACCESS_2_TRANSFER_WRITE_BIT,
                        .src_stage = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                        .dst_access = VK_ACCESS_2_NONE,
                        .dst_stage = VK_PIPELINE_STAGE_2_TRANSFER_BIT,
                        .old_layout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                        .new_layout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
                    };

                    Cmd::transition_image(info.cmd, image.handle, transition_info, { VK_IMAGE_ASPECT_COLOR_BIT, 0, VK_REMAINING_MIP_LEVELS, 0, VK_REMAINING_ARRAY_LAYERS });
                }

                transfer_end(device, info);

                vmaDestroyBuffer(allocator, staging_buffer.handle, staging_buffer.allocation);

                return image;
            }

            void transfer_begin(VkDevice device, const TransferInfo& info)
            {
                VK_CHECK(vkResetFences(device, 1, &info.fence));
                VK_CHECK(vkResetCommandBuffer(info.cmd, 0));

                VkCommandBufferBeginInfo begin_info = {
                    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
                    .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
                };

                VK_CHECK(vkBeginCommandBuffer(info.cmd, &begin_info));
            }

            void transfer_end(VkDevice device, const TransferInfo& info)
            {
                VK_CHECK(vkEndCommandBuffer(info.cmd));

                VkCommandBufferSubmitInfo buffer_submit_info = {
                    .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_SUBMIT_INFO,
                    .commandBuffer = info.cmd,
                    .deviceMask = 0
                };

                VkSubmitInfo2 submit_info = {
                    .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO_2,
                    .commandBufferInfoCount = 1,
                    .pCommandBufferInfos = &buffer_submit_info
                };

                VK_CHECK(vkQueueSubmit2(info.queue, 1, &submit_info, info.fence));
                VK_CHECK(vkWaitForFences(device, 1, &info.fence, true, UINT64_MAX));
            }

            void destroy_image(VkDevice device, VmaAllocator allocator, Image& image)
            {
                if(image.handle == VK_NULL_HANDLE) return;
                
                vkDestroyImageView(device, image.view, nullptr);
                vmaDestroyImage(allocator, image.handle, image.allocation);

                image.handle = VK_NULL_HANDLE;
                image.view = VK_NULL_HANDLE;
                image.allocation = nullptr;
                image.info = {};
            }

        bool load_shader_module(const char* path, VkDevice device, VkShaderModule* out_module)
        {
            std::ifstream file(path, std::ios::ate | std::ios::binary);
            if(!file.is_open())
            {
                std::cerr << "Failed to open file: " << path << std::endl;
                return false;
            }
        
            size_t file_size = (size_t)file.tellg();
            std::vector<uint32_t> buffer(file_size / sizeof(uint32_t));
            file.seekg(0);
            file.read((char*)buffer.data(), file_size);
            file.close();
        
            VkShaderModuleCreateInfo create_info = {
                .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
                .codeSize = buffer.size() * sizeof(uint32_t),
                .pCode = buffer.data()
            };
        
            VkShaderModule shader_module;
            VK_CHECK(vkCreateShaderModule(device, &create_info, nullptr, &shader_module))
        
            *out_module = shader_module;
        
            return true;
        }

            namespace Cmd
            {
                void transition_image(VkCommandBuffer cmd, VkImage image, const ImageTransitionInfo& info, VkImageSubresourceRange sub_image)
                {
                    VkImageMemoryBarrier2 image_barrier = {
                        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER_2,
                        .srcStageMask = info.src_stage,
                        .srcAccessMask = info.src_access,
                        .dstStageMask = info.dst_stage,
                        .dstAccessMask = info.dst_access,
                        .oldLayout = info.old_layout,
                        .newLayout = info.new_layout,
                        .image = image,
                        .subresourceRange = sub_image
                    };
                
                    VkDependencyInfo dep_info = {
                        .sType = VK_STRUCTURE_TYPE_DEPENDENCY_INFO,
                        .imageMemoryBarrierCount = 1,
                        .pImageMemoryBarriers = &image_barrier
                    };
                
                    vkCmdPipelineBarrier2(cmd, &dep_info);
                }
            }
        }
    }
}