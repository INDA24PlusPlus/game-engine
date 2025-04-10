cmake_minimum_required(VERSION 3.20)
project(game_engine LANGUAGES CXX C)

include(FetchContent)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS  ON)
set(CMAKE_ENABLE_EXPORTS TRUE)

set(BUILD_TYPE "optional_value" CACHE STRING "Debug")

set(COMMON_COMPILE_FLAGS -Wall -Wextra -Wunused-result -Wno-missing-field-initializers -Wno-unused-function -fno-exceptions)
set(CMAKE_CXX_FLAGS_DEBUG "-pg -g")
set(CMAKE_CXX_FLAGS_RELEASE "-Ofast")

set(CMAKE_CXX_CLANG_TIDY "") #"clang-tidy;--config-file=${CMAKE_CURRENT_SOURCE_DIR}/.clang-tidy")

# GLM
FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1
)

# ImGui
FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.9b
)

# GLFW
FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
)

FetchContent_MakeAvailable(glm glfw imgui)

# ImGui
set(IMGUI_SOURCES 
    "${imgui_SOURCE_DIR}/imgui.cpp"
    "${imgui_SOURCE_DIR}/imgui_demo.cpp"
    "${imgui_SOURCE_DIR}/imgui_draw.cpp"
    "${imgui_SOURCE_DIR}/imgui_tables.cpp"
    "${imgui_SOURCE_DIR}/imgui_widgets.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp"
    "${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp"
)

add_library(imgui "${IMGUI_SOURCES}")
target_include_directories(imgui
    PUBLIC "${imgui_SOURCE_DIR}"
    PUBLIC "${imgui_SOURCE_DIR}/backends"
)
set_target_properties(imgui PROPERTIES CXX_CLANG_TIDY "")

# GLAD
add_library(glad "${CMAKE_CURRENT_SOURCE_DIR}/vendor/glad/src/glad.c")
target_include_directories(glad PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/vendor/glad/include")
set_target_properties(glad PROPERTIES CXX_CLANG_TIDY "")

# Project 
file(GLOB_RECURSE SOURCE_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/**.cpp")
add_executable(game_engine ${SOURCE_FILES})

target_include_directories(game_engine
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src"
    PRIVATE "${glm_SOURCE_DIR}"
    PRIVATE "${glfw_SOURCE_DIR}/include"
)

target_compile_options(game_engine PRIVATE ${COMMON_COMPILE_FLAGS})
target_link_libraries(game_engine PRIVATE glm::glm-header-only imgui glfw glad ${CMAKE_CXX_IMPLICIT_LINK_LIBRARIES})
