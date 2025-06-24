#pragma once
#include <iostream>
#include <vulkan/vk_enum_string_helper.h>

#define VK_CHECK(x)                                                     \
    do {                                                                \
        VkResult err = x;                                               \
        if (err) {                                                      \
            std::cout << #x << std::endl;                               \
            std::cout << "Detected Vulkan error: " << string_VkResult(err) << std::endl;\
            std::cout << __FILE__ << " " << __LINE__ << std::endl;      \
            exit(EXIT_FAILURE);                                                   \
        }                                                               \
    } while (0);