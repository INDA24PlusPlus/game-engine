cmake_minimum_required(VERSION 3.20)
project(game_server LANGUAGES CXX C)

include(FetchContent)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(SOURCE_FILES
    main.cpp
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
    ../network/network.cpp
    ../network/server.cpp
)

set(COMMON_COMPILE_FLAGS -Wall -Wextra -Wunused-result -Wno-missing-field-initializers -Wno-unused-function -fno-exceptions)

add_executable(game_server ${SOURCE_FILES})
set_target_properties(game_server PROPERTIES
    CXX_STANDARD 23
    CXX_STANDARD_REQUIRED ON
)

option(ENABLE_EDITOR "Enable editor features" ON)

if(ENABLE_EDITOR)
    target_compile_definitions(game_server PRIVATE ENABLE_EDITOR=1)
    target_include_directories(game_server PRIVATE vendor/nlohmann)
endif()


target_include_directories(game_server PRIVATE ../../../src)

target_link_libraries(game_server PRIVATE ${CMAKE_CXX_IMPLICIT_LINK_LIBRARIES})
target_compile_options(game_server PRIVATE ${COMMON_COMPILE_FLAGS})
