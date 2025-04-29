cmake_minimum_required(VERSION 3.28)

project(audio VERSION 1.0.0 LANGUAGES C)

include(FetchContent)

set(CMAKE_ENABLE_EXPORTS TRUE)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_BUILD_TYPE Release)
set(CMAKE_C_FLAGS_DEBUG "-pg -g -lm")
set(CMAKE_C_FLAGS_RELEASE "-lm -Ofast -Wall")

FetchContent_Declare(
    miniaudio
    GIT_REPOSITORY https://github.com/mackron/miniaudio
    GIT_TAG 0.11.22
)

FetchContent_MakeAvailable(miniaudio)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/src/**.c)

add_executable(audio ${SOURCES})

target_include_directories(audio 
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${miniaudio_SOURCE_DIR}
)

target_link_libraries(audio 
    PRIVATE miniaudio
)
