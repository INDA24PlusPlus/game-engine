cmake_minimum_required(VERSION 3.20)
project(game_engine LANGUAGES CXX C)

include(FetchContent)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(SOURCE_FILES
    src/game/main.cpp
    src/game/gui.cpp
    src/engine/utils/logging.cpp
    src/engine/Renderer.cpp
    src/engine/Camera.cpp
    src/engine/Scene.cpp
    src/engine/Input.cpp
    vendor/glad/src/glad.c
)

set(COMMON_COMPILE_FLAGS -Wall -Wextra -Wunused-result -Wno-missing-field-initializers -Wno-unused-function -fno-exceptions)

FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1
)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.9b
)

FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
)

# KTX seems to only build on C++17
message(NOTICE "Fetching LibKTX from https://github.com/KhronosGroup/KTX-Software ...")
set(KTX_FEATURE_STATIC_LIBRARY ON CACHE BOOL "Build KTX as a static library" FORCE)
set(KTX_FEATURE_TOOLS OFF CACHE BOOL "")
set(KTX_FEATURE_DOC OFF CACHE BOOL "")
set(KTX_FEATURE_TESTS OFF CACHE BOOL "")
set(KTX_FEATURE_VK_UPLOAD OFF CACHE BOOL "")

FetchContent_Declare(
    ktx
    GIT_REPOSITORY https://github.com/KhronosGroup/KTX-Software.git
    GIT_TAG v4.4.0
)

FetchContent_MakeAvailable(glm glfw imgui ktx)

set_target_properties(ktx PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

add_executable(game_engine ${SOURCE_FILES})
set_target_properties(game_engine PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
)
target_include_directories(game_engine PRIVATE src)

# KTX
target_link_libraries(game_engine PRIVATE ktx)
target_include_directories(game_engine PRIVATE ${ktx_SOURCE_DIR}/include)

# GLM
target_include_directories(game_engine PRIVATE ${glm_SOURCE_DIR})

# GLFW
target_include_directories(game_engine PRIVATE ${glfw_SOURCE_DIR}/include)
target_link_libraries(game_engine PRIVATE glfw)

# IMGUI
target_include_directories(game_engine PRIVATE ${imgui_SOURCE_DIR})
target_sources(game_engine PRIVATE ${imgui_SOURCE_DIR}/imgui_demo.cpp)
target_sources(game_engine PRIVATE ${imgui_SOURCE_DIR}/imgui_draw.cpp)
target_sources(game_engine PRIVATE ${imgui_SOURCE_DIR}/imgui_tables.cpp)
target_sources(game_engine PRIVATE ${imgui_SOURCE_DIR}/imgui_widgets.cpp)
target_sources(game_engine PRIVATE ${imgui_SOURCE_DIR}/imgui.cpp)
target_sources(game_engine PRIVATE ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp)
target_sources(game_engine PRIVATE ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp)

# GLAD
target_include_directories(game_engine PRIVATE vendor/glad/include)

target_link_libraries(game_engine PRIVATE ${CMAKE_CXX_IMPLICIT_LINK_LIBRARIES})
target_compile_options(game_engine PRIVATE ${COMMON_COMPILE_FLAGS})
