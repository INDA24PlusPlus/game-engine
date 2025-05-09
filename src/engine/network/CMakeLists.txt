cmake_minimum_required(VERSION 3.20)
project(game_server LANGUAGES CXX C)

include(FetchContent)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(SOURCE_FILES
    main.cpp
    ../../game/components.cpp
    ../../game/time.cpp
    ../../game/enemy.cpp
    ../../game/world_gen/map.cpp
    ../utils/logging.cpp
    ../ecs/ecs.cpp
    ../ecs/entity.cpp
    ../ecs/component.cpp
    ../ecs/system.cpp
    ../ecs/entityarray.cpp
    ../ecs/utils.cpp
    ../ecs/resource.cpp
    ../ecs/signature.cpp
    ../ecs/systemmanager.cpp
    ../AssetLoader.cpp
    ../scene/Node.cpp
    ../scene/AssetManifest.cpp
    ../network/network.cpp
    ../network/client.cpp
    ../network/server.cpp
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
    GIT_TAG v1.91.9b-docking
)

FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
)

FetchContent_MakeAvailable(glm glfw imgui)

add_executable(game_server ${SOURCE_FILES})
set_target_properties(game_server PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
)

option(ENABLE_EDITOR "Enable editor features" ON)

if(ENABLE_EDITOR)
    target_compile_definitions(game_server PRIVATE ENABLE_EDITOR=1)
    target_include_directories(game_server PRIVATE ../../../vendor/nlohmann)
endif()

target_include_directories(game_server PRIVATE ../../../../game-engine/ ../../../src/)

# GLM
target_include_directories(game_server PRIVATE ${glm_SOURCE_DIR})

# GLFW
target_include_directories(game_server PRIVATE ${glfw_SOURCE_DIR}/include)
target_link_libraries(game_server PRIVATE glfw)

# IMGUI
target_include_directories(game_server PRIVATE ${imgui_SOURCE_DIR})
target_sources(game_server PRIVATE ${imgui_SOURCE_DIR}/imgui_demo.cpp)
target_sources(game_server PRIVATE ${imgui_SOURCE_DIR}/imgui_draw.cpp)
target_sources(game_server PRIVATE ${imgui_SOURCE_DIR}/imgui_tables.cpp)
target_sources(game_server PRIVATE ${imgui_SOURCE_DIR}/imgui_widgets.cpp)
target_sources(game_server PRIVATE ${imgui_SOURCE_DIR}/imgui.cpp)
target_sources(game_server PRIVATE ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp)
target_sources(game_server PRIVATE ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp)

# GLAD
target_include_directories(game_server PRIVATE vendor/glad/include)

target_link_libraries(game_server PRIVATE ${CMAKE_CXX_IMPLICIT_LINK_LIBRARIES})
target_compile_options(game_server PRIVATE ${COMMON_COMPILE_FLAGS})
