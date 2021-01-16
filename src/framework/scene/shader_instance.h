/*
 * Manuel Machado Copyright (C) 2021 This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */

#ifndef MANUEME_VULKAN_INSTANCE_H
#define MANUEME_VULKAN_INSTANCE_H

#include <cstdint>

struct ShaderMeshInstance {
    uint32_t indexBase; // this is the position of the index, not the real offset on the buffer
    uint32_t vertexBase; // this is the position of the vertex, not the real offset on the buffer
    uint32_t materialIndex;
};

#endif // MANUEME_VULKAN_INSTANCE_H
