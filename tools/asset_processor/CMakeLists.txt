cmake_minimum_required(VERSION 3.20)
project(asset_processor LANGUAGES CXX C)

include(FetchContent)

set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS  ON)

set(SOURCE_FILES
    src/main.cpp
    src/AssetImporter.cpp
    ../../src/engine/utils/logging.cpp
)

set(COMMON_COMPILE_FLAGS -Wall -Wextra -Wunused-result -Wno-missing-field-initializers -Wno-unused-function -fno-exceptions)

FetchContent_Declare(
    tinygltf
    GIT_REPOSITORY https://github.com/syoyo/tinygltf.git
    GIT_TAG v2.9.5
)

FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1
)

set(CMAKE_CXX_STANDARD 17)
message(NOTICE "Fetching LibKTX from https://github.com/KhronosGroup/KTX-Software ...")
set(KTX_FEATURE_STATIC_LIBRARY ON CACHE BOOL "Build KTX as a static library" FORCE)
set(KTX_FEATURE_TOOLS OFF CACHE BOOL "")
set(KTX_FEATURE_DOC OFF CACHE BOOL "")
set(KTX_FEATURE_TESTS OFF CACHE BOOL "")

FetchContent_Declare(
    ktx
    GIT_REPOSITORY https://github.com/KhronosGroup/KTX-Software.git
    GIT_TAG v4.4.0
)

FetchContent_MakeAvailable(glm tinygltf ktx)

add_executable(asset_processor ${SOURCE_FILES})
set_target_properties(asset_processor PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
)

target_link_libraries(asset_processor PRIVATE ktx)
target_include_directories(asset_processor PRIVATE ${fetch_ktx_SOURCE_DIR}/include)

target_include_directories(asset_processor PRIVATE src)
target_include_directories(asset_processor PRIVATE ${glm_SOURCE_DIR})
target_include_directories(asset_processor PRIVATE ${tinygltf_SOURCE_DIR})

target_link_libraries(asset_processor PRIVATE ${CMAKE_CXX_IMPLICIT_LINK_LIBRARIES})

target_sources(asset_processor PRIVATE ${tinygltf_SOURCE_DIR}/tiny_gltf.cc)
target_compile_options(asset_processor PRIVATE ${COMMON_COMPILE_FLAGS})
