cmake_minimum_required(VERSION 3.20)

# Project name
project(WizardChess)

# Set the C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Source files
set(SOURCE_FILES
    src/main.cpp
    src/Model.cpp
    src/WizardChess.cpp
    src/MemoryTracker.cpp
    src/VulkanDeviceManager.cpp
    src/VulkanSurfaceManager.cpp
)

# Header files
set(HEADER_FILES
    include/main.h
    include/Model.h
    include/Types.h
    include/Utils.h
    include/MemoryTracker.h
    include/VulkanHelper.h
    include/VulkanDeviceManager.h
    include/VulkanSurfaceManager.h
    include/WizardChess.h
)

# Shader files
set(SHADER_SOURCE_FILES
    ${CMAKE_SOURCE_DIR}/assets/shaders/render.frag
    ${CMAKE_SOURCE_DIR}/assets/shaders/render.vert
)

set(SHADER_OUTPUT_FILES
    ${CMAKE_BINARY_DIR}/compiled_shaders/fragRender.spv
    ${CMAKE_BINARY_DIR}/compiled_shaders/vertRender.spv
)

# Organize shader files in the Visual Studio solution
source_group("Shaders" FILES ${SHADER_SOURCE_FILES})

# Custom command to compile shaders
add_custom_command(
    OUTPUT ${SHADER_OUTPUT_FILES}
    COMMAND ${CMAKE_SOURCE_DIR}/assets/shaders/compile.bat ${CMAKE_SOURCE_DIR}/assets/shaders/render.frag ${CMAKE_BINARY_DIR}/compiled_shaders/fragRender.spv
    COMMAND ${CMAKE_SOURCE_DIR}/assets/shaders/compile.bat ${CMAKE_SOURCE_DIR}/assets/shaders/render.vert ${CMAKE_BINARY_DIR}/compiled_shaders/vertRender.spv
    DEPENDS ${SHADER_SOURCE_FILES}
    COMMENT "Compiling shaders into ${CMAKE_BINARY_DIR}/compiled_shaders"
)

# Add executable, including shaders to make them visible in the solution explorer
add_executable(${PROJECT_NAME}
    ${SOURCE_FILES}
    ${HEADER_FILES}
    ${SHADER_SOURCE_FILES}
)

# Include paths
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    $ENV{VULKAN_SDK}/Include
    ${CMAKE_SOURCE_DIR}/extern/glfw-3.4.bin.WIN64/include
)

# Library paths
target_link_directories(${PROJECT_NAME} PRIVATE
    $ENV{VULKAN_SDK}/Lib
    ${CMAKE_SOURCE_DIR}/extern/glfw-3.4.bin.WIN64/lib-vc2022
)

# Link libraries
target_link_libraries(${PROJECT_NAME}
    glfw3.lib
    vulkan-1.lib
)

# Ensure shaders are compiled before build
add_custom_target(Shaders
    DEPENDS ${CMAKE_SOURCE_DIR}/assets/shaders/compile.bat
    DEPENDS ${SHADER_SOURCE_FILES}
    DEPENDS ${SHADER_OUTPUT_FILES}
)
add_dependencies(${PROJECT_NAME} Shaders)

# Set Visual Studio startup project
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT ${PROJECT_NAME})

# Define asset paths for compile-time literal strings in C++
target_compile_definitions(${PROJECT_NAME} PRIVATE TEXTURE_PATH="${CMAKE_SOURCE_DIR}/assets/textures/")
target_compile_definitions(${PROJECT_NAME} PRIVATE MODEL_PATH="${CMAKE_SOURCE_DIR}/assets/models/")
target_compile_definitions(${PROJECT_NAME} PRIVATE COMPILED_SHADER_ROOT="${CMAKE_BINARY_DIR}/compiled_shaders/")
