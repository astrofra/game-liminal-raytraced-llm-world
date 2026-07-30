cmake_minimum_required(VERSION 3.20)

project(liminal_raytraced_renderer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(liminal_cornell_renderer
    src/main.cpp
    src/scene.cpp
    src/renderer.cpp
)

target_include_directories(liminal_cornell_renderer PRIVATE src vendor/stb)

if(MSVC)
    target_compile_options(liminal_cornell_renderer PRIVATE /W4 /wd4996)
else()
    target_compile_options(liminal_cornell_renderer PRIVATE -Wall -Wextra -Wpedantic)
endif()
