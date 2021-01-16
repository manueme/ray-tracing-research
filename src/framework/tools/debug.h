/*
 * Manuel Machado Copyright (C) 2021 This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */

#pragma once

#include "vulkan/vulkan.h"

namespace debug {

// Load debug function pointers and set debug callback
VkResult setupDebugging(
    VkInstance t_instance, VkDebugReportFlagsEXT t_flags, VkDebugReportCallbackEXT t_callBack);
// Clear debug callback
void freeDebugCallback(VkInstance t_instance);

} // namespace debug
