cmake_minimum_required(VERSION 3.28)
include(FetchContent)

project(gameengine VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_ENABLE_EXPORTS TRUE)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(BUILD_TYPE "optional_value" CACHE STRING "Debug")

set(DEFAULT_FLAGS "-std=c++23 -Wall -Wextra -Werror")
set(CMAKE_CXX_FLAGS_DEBUG "${DEFAULT_FLAGS} -pg -g")
set(CMAKE_CXX_FLAGS_RELEASE "${DEFAULT_FLAGS} -Ofast")

set(CMAKE_CXX_CLANG_TIDY "clang-tidy;--config-file=${CMAKE_CURRENT_SOURCE_DIR}/.clang-tidy")

find_package(Vulkan REQUIRED)

FetchContent_Declare(glm URL https://github.com/g-truc/glm/releases/download/1.0.1/glm-1.0.1-light.zip)
FetchContent_Declare(imgui URL https://github.com/ocornut/imgui/archive/refs/tags/v1.91.9b.zip)

FetchContent_MakeAvailable(glm)
FetchContent_MakeAvailable(imgui)

set(IMGUI_SOURCES 
    "${imgui_SOURCE_DIR}/imgui.cpp"
    "${imgui_SOURCE_DIR}/imgui_demo.cpp"
    "${imgui_SOURCE_DIR}/imgui_draw.cpp"
    "${imgui_SOURCE_DIR}/imgui_tables.cpp"
    "${imgui_SOURCE_DIR}/imgui_widgets.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp"
)

message(STATUS "ImGui at: ${imgui_SOURCE_DIR}")

# Create project binary
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/**.cpp")
add_executable(gameengine "${SOURCES}" "${IMGUI_SOURCES}")

target_include_directories(gameengine
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src"
    PUBLIC "${VULKAN_INCLUDE_DIRS}"
    PUBLIC "${imgui_SOURCE_DIR}"
    PUBLIC "${imgui_SOURCE_DIR}/backends"
)

target_link_libraries(gameengine PRIVATE Vulkan::Vulkan glm::glm-header-only)
