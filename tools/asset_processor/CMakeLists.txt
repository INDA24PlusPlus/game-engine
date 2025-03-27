cmake_minimum_required(VERSION 3.14)
project(AssetProcessor)

# Enable fetching dependencies
include(FetchContent)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 23)

add_compile_definitions(_CRT_SECURE_NO_WARNINGS)

FetchContent_Declare(
    assimp
    GIT_REPOSITORY https://github.com/assimp/assimp.git
    GIT_TAG v5.4.3
)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(assimp)

FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1
)
FetchContent_MakeAvailable(glm)

add_executable(asset_processor src/main.cpp)
target_include_directories(asset_processor PRIVATE ${glm_SOURCE_DIR})
target_link_libraries(asset_processor PRIVATE assimp::assimp)
