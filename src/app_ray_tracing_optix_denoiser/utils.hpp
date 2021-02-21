/*
 * Manuel Machado Copyright (C) 2021 This code is licensed under the MIT license (MIT)
 * (http://opensource.org/licenses/MIT)
 */

#ifndef RAY_TRACING_DENOISER_OPTIX_HELPERS_H
#define RAY_TRACING_DENOISER_OPTIX_HELPERS_H

#include <fstream>
#include <iomanip>
#include <iostream>

#define OPTIX_CHECK(call)                                                                          \
    {                                                                                              \
        OptixResult res = (call);                                                                  \
        if (res != OPTIX_SUCCESS) {                                                                \
            std::cout << "Optix call (" << #call << " ) failed with code " << res << " ("          \
                      << static_cast<const char*>(__FILE__) << ":" << static_cast<int>(__LINE__)   \
                      << std::endl;                                                                \
            assert(res == OPTIX_SUCCESS);                                                          \
        }                                                                                          \
    }

#define CUDA_CHECK(call)                                                                           \
    {                                                                                              \
        cudaError_t error = (call);                                                                \
        if (error != cudaSuccess) {                                                                \
            std::cout << "CUDA call (" << #call << " ) failed with code " << error << "\" in "     \
                      << static_cast<const char*>(__FILE__) << " at line "                         \
                      << static_cast<int>(__LINE__) << " in function \""                           \
                      << static_cast<const char*>(__FUNCTION__) << "\"" << std::endl;              \
            assert(error == cudaSuccess);                                                          \
        }                                                                                          \
    }

static void context_log_cb(
    unsigned int level, const char* tag, const char* message, void* /*cbdata */)
{
    std::cerr << "[" << std::setw(2) << level << "][" << std::setw(12) << tag << "]: " << message
              << "\n";
}

#endif // RAY_TRACING_DENOISER_OPTIX_HELPERS_H
