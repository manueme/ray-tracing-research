cmake_minimum_required(VERSION 3.17)
project(ray_tracing_research)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_EXPORT_COMPILE_COMMANDS=1)
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

# Determine platform architecture
IF (CMAKE_SIZEOF_VOID_P EQUAL 8)
    set(ARCHITECTURE "x64")
    set(ARQ_64 1)
ELSE()
    set(ARCHITECTURE "x32")
    set(ARQ_32 1)
ENDIF()

IF (CMAKE_BUILD_TYPE STREQUAL "Debug")
    set(DEBUG 1)
    set(BUILD_TYPE "Debug")
ELSE() # Could also be RelWithDebInfo
    set(BUILD_TYPE "Release")
ENDIF()
# ###

set(EXTERNAL_SOURCES ${CMAKE_CURRENT_LIST_DIR}/external_sources)

# Configure Vulkan
find_package(Vulkan REQUIRED)
# ###

# Configure glfw3
IF (WIN32)
    set(GLFW_INCLUDE_DIRS "${EXTERNAL_SOURCES}/glfw/include")
    set(GLFW_LIBRARY_PATH "${EXTERNAL_SOURCES}/glfw/build/${ARCHITECTURE}/src/${BUILD_TYPE}")
    set(GLFW_LIBRARY_NAMES glfw3.lib)
ELSEIF (UNIX AND NOT APPLE)
    find_package(glfw3 3.3 REQUIRED)
    set(GLFW_LIBRARY_NAMES glfw)
ENDIF ()
find_library(GLFW_LIBRARIES NAMES ${GLFW_LIBRARY_NAMES} HINTS ${GLFW_LIBRARY_PATH})
include_directories(${GLFW_INCLUDE_DIRS})
# ############

# Configure Assimp
IF (WIN32)
    set(ASSIMP_INCLUDE_DIRS "${EXTERNAL_SOURCES}/assimp/include" "${EXTERNAL_SOURCES}/assimp/build/${ARCHITECTURE}/include")
    set(ASSIMP_LIBRARY_PATH "${EXTERNAL_SOURCES}/assimp/build/${ARCHITECTURE}/lib/${BUILD_TYPE}")
    set(ASSIMP_DLL_PATH "${EXTERNAL_SOURCES}/assimp/build/${ARCHITECTURE}/bin/${BUILD_TYPE}")
    IF(DEBUG)
        set(ASSIMP_DLLS "${ASSIMP_DLL_PATH}/assimp-vc142-mtd.dll")
        set(ASSIMP_LIBRARY_NAMES "assimp-vc142-mtd.lib")
    ELSE()
        set(ASSIMP_DLLS "${ASSIMP_DLL_PATH}/assimp-vc142-mt.dll")
        set(ASSIMP_LIBRARY_NAMES "assimp-vc142-mt.lib")
    ENDIF()
    message(ASSIMP_LIBRARY_PATH-${ASSIMP_LIBRARY_PATH})
    message(ASSIMP_LIBRARY_NAMES-${ASSIMP_LIBRARY_NAMES})
    find_library(ASSIMP_LIBRARIES NAMES "${ASSIMP_LIBRARY_NAMES}" HINTS "${ASSIMP_LIBRARY_PATH}")
ELSEIF (UNIX AND NOT APPLE)
    set(ASSIMP_INCLUDE_DIRS "${EXTERNAL_SOURCES}/assimp/include" "${EXTERNAL_SOURCES}/assimp/build/include")
    set(ASSIMP_LIBRARY_PATH "${EXTERNAL_SOURCES}/assimp/build/bin")
    set(ASSIMP_LIBRARY_NAMES "libassimp.so")
    message(ASSIMP_LIBRARY_PATH-${ASSIMP_LIBRARY_PATH})
    message(ASSIMP_LIBRARY_NAMES-${ASSIMP_LIBRARY_NAMES})
    find_library(ASSIMP_LIBRARIES NAMES "${ASSIMP_LIBRARY_NAMES}" HINTS "${ASSIMP_LIBRARY_PATH}")
ENDIF ()
message(${ASSIMP_LIBRARIES})
include_directories(${ASSIMP_INCLUDE_DIRS})
# ############

# Configure Freeimage
IF (WIN32)
    set(FREEIMAGE_LIBRARY_DIR "${EXTERNAL_SOURCES}/freeImage/${ARCHITECTURE}")
    set(FREEIMAGE_DLLS ${FREEIMAGE_LIBRARY_DIR}/FreeImage.dll)
    set(FREEIMAGE_INCLUDE_DIR ${FREEIMAGE_LIBRARY_DIR})
    set(FREEIMAGE_LIBRARY_NAMES FreeImage)
ELSEIF (UNIX AND NOT APPLE)
    set(FREEIMAGE_LIBRARY_NAMES freeimage)
    find_path(FREEIMAGE_INCLUDE_DIR FreeImage.h)
ENDIF ()
find_library(FREEIMAGE_LIBRARIES NAMES ${FREEIMAGE_LIBRARY_NAMES} HINTS ${FREEIMAGE_LIBRARY_DIR})
include_directories(${FREEIMAGE_INCLUDE_DIR})
# ############

# Configure GLM
include_directories(${EXTERNAL_SOURCES}/glm)
# ############

set(ASSETS_FOLDER assets)
set(TARGET_LIBRARIES ${FREEIMAGE_LIBRARIES} ${GLFW_LIBRARIES} ${ASSIMP_LIBRARIES} Vulkan::Vulkan Threads::Threads)
set(FRAMEWORK_SRC ${CMAKE_CURRENT_SOURCE_DIR}/src/framework/*.*pp ${CMAKE_CURRENT_SOURCE_DIR}/src/framework/*/*.*pp)
set(SHARED_SRC ${CMAKE_CURRENT_SOURCE_DIR}/src/shared_pipelines/*.*pp)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/framework)
include_directories(${CMAKE_CURRENT_SOURCE_DIR}/src/shared_pipelines)

add_subdirectory(src/app_hybrid_ray_tracing)
add_subdirectory(src/app_monte_carlo_ray_tracing)
add_subdirectory(src/app_ray_tracing_optix_denoiser)
