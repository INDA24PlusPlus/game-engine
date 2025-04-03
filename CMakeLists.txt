cmake_minimum_required(VERSION 3.20)
project(vulkan_renderer LANGUAGES CXX C)

include(FetchContent)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS  ON)

if(WIN32)
    add_definitions(-DNOMINMAX -DWIN32_LEAN_AND_MEAN)
endif()

set(ENGINE_SOURCE_FILES
    src/engine/renderer.cpp
)

set(GAME_SOURCE_FILES
    src/game/game.cpp
)

set(WIN32_SOURCES
    src/engine/win32_main.cpp
    src/engine/win32_platform.cpp
    ${ENGINE_SOURCE_FILES}
    ${GAME_SOURCE_FILES}
)

set(COMMON_COMPILE_FLAGS -Wall -Wextra -Wunused-result -Wno-missing-field-initializers -Wno-unused-function)

option(DISABLE_BOUNDS_CHECKS "Disables bounds checking for user created types" OFF)
option(ENABLE_TRACY "Build with support for profiling with the tracy profiler" OFF)

if(DISABLE_BOUNDS_CHECKS)
    target_compile_definitions(vulkan_renderer PRIVATE DISABLE_BOUNDS_CHECKS)
endif()

FetchContent_Declare(
    vulkan_headers
    GIT_REPOSITORY https://github.com/KhronosGroup/Vulkan-Headers.git
    GIT_TAG v1.3.295
)

FetchContent_Declare(
    volk
    GIT_REPOSITORY https://github.com/zeux/volk.git
    GIT_TAG 1.3.295
)

FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1
)

FetchContent_Declare(
    vulkan_memory_allocator
    GIT_REPOSITORY https://github.com/GPUOpen-LibrariesAndSDKs/VulkanMemoryAllocator.git
    GIT_TAG v3.2.1
)

FetchContent_Declare(
    tracy
    GIT_REPOSITORY https://github.com/wolfpld/tracy.git
    GIT_TAG v0.11.1
)

FetchContent_MakeAvailable(vulkan_headers volk glm vulkan_memory_allocator tracy)

add_executable(vulkan_renderer ${WIN32_SOURCES})
target_include_directories(vulkan_renderer PRIVATE src)

target_include_directories(vulkan_renderer PRIVATE ${vulkan_headers_SOURCE_DIR}/include)

if(WIN32)
    set(VULKAN_PLATFORM_DEFINE "VK_USE_PLATFORM_WIN32_KHR")
else()
    message(FATAL_ERROR "Unsupported OS")
endif()

target_include_directories(vulkan_renderer PRIVATE ${volk_SOURCE_DIR})
target_compile_definitions(vulkan_renderer PRIVATE ${VULKAN_PLATFORM_DEFINE})
target_sources(vulkan_renderer PRIVATE ${volk_SOURCE_DIR}/volk.c)

target_include_directories(vulkan_renderer PRIVATE ${glm_SOURCE_DIR})

target_include_directories(vulkan_renderer PRIVATE 
    ${vulkan_memory_allocator_SOURCE_DIR}/include
    ${vulkan_memory_allocator_SOURCE_DIR}/src
)
target_sources(vulkan_renderer PRIVATE 
    ${vulkan_memory_allocator_SOURCE_DIR}/src/VmaUsage.cpp
)

target_include_directories(vulkan_renderer PRIVATE ${tracy_SOURCE_DIR}/public)
if(ENABLE_TRACY)
    target_compile_definitions(vulkan_renderer PRIVATE TRACY_ENABLE)
    target_sources(vulkan_renderer PRIVATE ${tracy_SOURCE_DIR}/public/TracyClient.cpp)
    
    if(WIN32)
        target_link_libraries(vulkan_renderer PRIVATE ws2_32 dbghelp)
    else()
        message(FATAL_ERROR "Unsupported OS for Tracy")
    endif()
endif()

target_link_libraries(vulkan_renderer PRIVATE ${CMAKE_CXX_IMPLICIT_LINK_LIBRARIES})
target_compile_options(vulkan_renderer PRIVATE ${COMMON_COMPILE_FLAGS})