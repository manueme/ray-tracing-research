set(SUB_PROJECT_NAME denoiser)
message(STATUS "Generating project file for ${SUB_PROJECT_NAME}")

set(SHADERS
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/closesthit.rchit.spv
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/raygen.rgen.spv
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/miss.rmiss.spv
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/shadow.rmiss.spv
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/anyhit.rahit.spv
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/shadow.rahit.spv
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/postprocess.vert.spv
        ${CMAKE_CURRENT_SOURCE_DIR}/shaders/postprocess.frag.spv
        )

set(MONTE_CARLO_APP_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../monte_carlo_ray_tracing)
set(MONTE_CARLO_APP_SOURCE ${MONTE_CARLO_APP_DIR}/monte_carlo_ray_tracing.cpp)
file(GLOB SOURCE ${FRAMEWORK_SRC} ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp ${MONTE_CARLO_APP_SOURCE})
set(MAIN_CPP main.cpp)
add_executable(${SUB_PROJECT_NAME} ${MAIN_CPP} ${SOURCE})
target_link_libraries(${SUB_PROJECT_NAME} ${TARGET_LIBRARIES})

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
            COMMAND make -f shaders/Makefile build-shaders
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