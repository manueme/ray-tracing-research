set(SUB_PROJECT_NAME ray_tracing_optix_denoiser)
message(STATUS "Generating project file for ${SUB_PROJECT_NAME}")

set(SHADERS
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/closesthit.rchit.spv
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/raygen.rgen.spv
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/miss.rmiss.spv
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/shadow.rmiss.spv
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/anyhit.rahit.spv
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/shadow.rahit.spv
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/post_process.comp.spv
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/auto_exposure.comp.spv
        )

# CUDA
# see https://cmake.org/cmake/help/v3.3/module/FindCUDA.html
find_package(CUDA 10.0 REQUIRED)
if(CUDA_FOUND)
    add_definitions("-DCUDA_PATH=R\"(${CUDA_TOOLKIT_ROOT_DIR})\"")
    Message(STATUS "--> using package CUDA (${CUDA_VERSION})")
    add_definitions(-DUSECUDA)
    include_directories(${CUDA_INCLUDE_DIRS})
    LIST(APPEND CUDA_LIBRARIES_OPTIMIZED ${CUDA_LIBRARIES} )
    LIST(APPEND CUDA_LIBRARIES_DEBUG ${CUDA_LIBRARIES} )
    # STRANGE: default CUDA package finder from cmake doesn't give anything to find cuda.lib
    if(WIN32)
        if((ARCH STREQUAL "x86"))
            LIST(APPEND CUDA_LIBRARIES_DEBUG "${CUDA_TOOLKIT_ROOT_DIR}/lib/Win32/cuda.lib" )
            LIST(APPEND CUDA_LIBRARIES_DEBUG "${CUDA_TOOLKIT_ROOT_DIR}/lib/Win32/cudart.lib" )
            LIST(APPEND CUDA_LIBRARIES_OPTIMIZED "${CUDA_TOOLKIT_ROOT_DIR}/lib/Win32/cuda.lib" )
            LIST(APPEND CUDA_LIBRARIES_OPTIMIZED "${CUDA_TOOLKIT_ROOT_DIR}/lib/Win32/cudart.lib" )
        else()
            LIST(APPEND CUDA_LIBRARIES_DEBUG "${CUDA_TOOLKIT_ROOT_DIR}/lib/x64/cuda.lib" )
            LIST(APPEND CUDA_LIBRARIES_DEBUG "${CUDA_TOOLKIT_ROOT_DIR}/lib/x64/cudart.lib" )
            LIST(APPEND CUDA_LIBRARIES_DEBUG "${CUDA_TOOLKIT_ROOT_DIR}/lib/x64/nvrtc.lib" )
            LIST(APPEND CUDA_LIBRARIES_OPTIMIZED "${CUDA_TOOLKIT_ROOT_DIR}/lib/x64/cuda.lib" )
            LIST(APPEND CUDA_LIBRARIES_OPTIMIZED "${CUDA_TOOLKIT_ROOT_DIR}/lib/x64/cudart.lib" )
            LIST(APPEND CUDA_LIBRARIES_OPTIMIZED "${CUDA_TOOLKIT_ROOT_DIR}/lib/x64/nvrtc.lib" )
        endif()
    else()
        LIST(APPEND CUDA_LIBRARIES_DEBUG "libcuda.so" )
        LIST(APPEND CUDA_LIBRARIES_DEBUG "${CUDA_TOOLKIT_ROOT_DIR}/lib64/libcudart.so" )
        LIST(APPEND CUDA_LIBRARIES_DEBUG "${CUDA_TOOLKIT_ROOT_DIR}/lib64/libnvrtc.so" )
        LIST(APPEND CUDA_LIBRARIES_OPTIMIZED "libcuda.so" )
        LIST(APPEND CUDA_LIBRARIES_OPTIMIZED "${CUDA_TOOLKIT_ROOT_DIR}/lib64/libcudart.so" )
        LIST(APPEND CUDA_LIBRARIES_OPTIMIZED "${CUDA_TOOLKIT_ROOT_DIR}/lib64/libnvrtc.so" )
    endif()
    #LIST(APPEND PACKAGE_SOURCE_FILES ${CUDA_HEADERS} ) Not available anymore with cmake 3.3... we might have to list them by hand
    # source_group(CUDA FILES ${CUDA_HEADERS} )  Not available anymore with cmake 3.3
else()
    Message(STATUS "--> NOT using package CUDA")
endif()
# ----
# OptiX 7.1 from external_sources folder
include_directories(${EXTERNAL_SOURCES}/OptiX7SDK/include)
# ----

file(GLOB SOURCE ${FRAMEWORK_SRC} ${SHARED_SRC} ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
set(MAIN_CPP main.cpp)
add_executable(${SUB_PROJECT_NAME} ${MAIN_CPP} ${SOURCE})
target_link_libraries(${SUB_PROJECT_NAME} ${TARGET_LIBRARIES} ${CUDA_LIBRARIES_OPTIMIZED})

# Compile shaders if a compile script exists and mode them to working dir
IF (WIN32)
    add_custom_command(
            TARGET ${SUB_PROJECT_NAME}
            POST_BUILD
            COMMAND ${CMAKE_CURRENT_SOURCE_DIR}/shaders/compile.bat
            COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/shaders
            COMMAND ${CMAKE_COMMAND} -E copy ${SHADERS}
            ${CMAKE_CURRENT_BINARY_DIR}/shaders
    )
    # Copy dll files if Windows
    set(DLL_LIST ${FREEIMAGE_DLLS} ${ASSIMP_DLLS})
    add_custom_command(
            TARGET ${SUB_PROJECT_NAME}
            POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy ${DLL_LIST}
            ${CMAKE_CURRENT_BINARY_DIR}
    )
ELSEIF (UNIX)
    add_custom_command(
            TARGET ${SUB_PROJECT_NAME}
            POST_BUILD
            COMMAND make -f ${CMAKE_CURRENT_SOURCE_DIR}/shaders/Makefile build-shaders
            COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_CURRENT_BINARY_DIR}/shaders
            COMMAND ${CMAKE_COMMAND} -E copy ${SHADERS}
            ${CMAKE_CURRENT_BINARY_DIR}/shaders
    )
ENDIF ()

IF (EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/../../${ASSETS_FOLDER})
    add_custom_command(
            TARGET ${SUB_PROJECT_NAME}
            POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/../../${ASSETS_FOLDER}
            ${CMAKE_CURRENT_BINARY_DIR}/${ASSETS_FOLDER}
    )
ENDIF ()