/*
 * Manuel Machado Copyright (C) 2021 This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */

#ifndef MANUEME_DEBUG_H
#define MANUEME_DEBUG_H

#include "vulkan/vulkan.h"
#include <iostream>

namespace debug {

// Load debug function pointers and set debug callback
VkResult setupDebugging(
    VkInstance t_instance, VkDebugReportFlagsEXT t_flags, VkDebugReportCallbackEXT t_callBack);
// Clear debug callback
void freeDebugCallback(VkInstance t_instance);

static inline void printPercentage(int step, int length) {
    std::cout << '\r' << std::round(100.0f * (step + 1.0f) / length) << "%" << std::flush;
}

} // namespace debug

#endif // MANUEME_DEBUG_H
