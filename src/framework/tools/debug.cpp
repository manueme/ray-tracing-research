/*
 * Manuel Machado Copyright (C) 2021 This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */

#include "debug.h"
#include <cstdio>
#include <iostream>
#include <sstream>
#include <string>

namespace debug {

PFN_vkCreateDebugUtilsMessengerEXT vkCreateDebugUtilsMessengerEXT;
PFN_vkDestroyDebugUtilsMessengerEXT vkDestroyDebugUtilsMessengerEXT;
VkDebugUtilsMessengerEXT debugUtilsMessenger;

VKAPI_ATTR VkBool32 VKAPI_CALL debugUtilsMessengerCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT t_messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT t_messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* t_callbackData, void* t_userData)
{
    // Select prefix depending on flags passed to the callback
    std::string prefix("");

    if (t_messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) {
        prefix = "VERBOSE: ";
    } else if (t_messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
        prefix = "INFO: ";
    } else if (t_messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        prefix = "WARNING: ";
    } else if (t_messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        prefix = "ERROR: ";
    }

    // Display message to default output (console/logcat)
    std::stringstream debugMessage;
    debugMessage << prefix << "[" << t_callbackData->messageIdNumber << "]["
                 << t_callbackData->pMessageIdName << "] : " << t_callbackData->pMessage;

    if (t_messageSeverity >= VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        std::cerr << debugMessage.str() << "\n";
    } else {
        std::cout << debugMessage.str() << "\n";
    }
    fflush(stdout);

    // The return value of this callback controls wether the Vulkan call that
    // caused the validation message will be aborted or not We return VK_FALSE as
    // we DON'T want Vulkan calls that cause a validation message to abort If you
    // instead want to have calls abort, pass in VK_TRUE and the function will
    // return VK_ERROR_VALIDATION_FAILED_EXT
    return VK_FALSE;
}

VkResult setupDebugging(
    VkInstance t_instance, VkDebugReportFlagsEXT t_flags, VkDebugReportCallbackEXT t_callBack)
{
    vkCreateDebugUtilsMessengerEXT = reinterpret_cast<PFN_vkCreateDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(t_instance, "vkCreateDebugUtilsMessengerEXT"));
    vkDestroyDebugUtilsMessengerEXT = reinterpret_cast<PFN_vkDestroyDebugUtilsMessengerEXT>(
        vkGetInstanceProcAddr(t_instance, "vkDestroyDebugUtilsMessengerEXT"));

    VkDebugUtilsMessengerCreateInfoEXT debugUtilsMessengerCI {};
    debugUtilsMessengerCI.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    debugUtilsMessengerCI.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
        | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    debugUtilsMessengerCI.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
        | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
    debugUtilsMessengerCI.pfnUserCallback = debugUtilsMessengerCallback;

    return vkCreateDebugUtilsMessengerEXT(t_instance,
        &debugUtilsMessengerCI,
        nullptr,
        &debugUtilsMessenger);
}

void freeDebugCallback(VkInstance t_instance)
{
    if (debugUtilsMessenger != VK_NULL_HANDLE) {
        vkDestroyDebugUtilsMessengerEXT(t_instance, debugUtilsMessenger, nullptr);
    }
}

} // namespace debug
