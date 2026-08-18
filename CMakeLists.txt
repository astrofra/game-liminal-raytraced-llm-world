cmake_minimum_required(VERSION 3.20)

project(liminal_raytraced_renderer LANGUAGES CXX)

option(LIMINAL_ENABLE_LLAMA_CPP "Build with vendorized llama.cpp runtime support" ON)
option(LIMINAL_ENABLE_LLAMA_CUDA "Enable CUDA backend in vendorized llama.cpp build" ON)
option(LIMINAL_ENABLE_OPENMP "Enable OpenMP render parallelism when available" ON)
option(LIMINAL_ENABLE_SDL3_FRONTEND "Build the SDL3 interactive frontend" ON)

set(LIMINAL_LLAMA_CPP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vendor/llama.cpp" CACHE PATH "Path to the vendorized llama.cpp tree")

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(LIMINAL_SOURCES
    src/animated_view.cpp
    src/main.cpp
    src/game_state.cpp
    src/llm_runtime.cpp
    src/scene.cpp
    src/scene_compiler.cpp
    src/renderer.cpp
    src/turn_contract.cpp
    src/turn_runner.cpp)

if (LIMINAL_ENABLE_SDL3_FRONTEND)
    list(APPEND LIMINAL_SOURCES src/sdl_frontend.cpp)
endif()

add_executable(liminal_cornell_renderer ${LIMINAL_SOURCES})

target_include_directories(liminal_cornell_renderer PRIVATE src vendor/stb "${LIMINAL_LLAMA_CPP_DIR}/vendor")

if (LIMINAL_ENABLE_OPENMP)
    find_package(OpenMP)
    if (OpenMP_CXX_FOUND)
        target_link_libraries(liminal_cornell_renderer PRIVATE OpenMP::OpenMP_CXX)
        target_compile_definitions(liminal_cornell_renderer PRIVATE LIMINAL_ENABLE_OPENMP=1)
    else()
        message(STATUS "OpenMP requested but not found; building renderer without OpenMP")
    endif()
endif()

if (LIMINAL_ENABLE_SDL3_FRONTEND)
    find_package(SDL3 CONFIG QUIET)
    if (NOT SDL3_FOUND)
        include(FetchContent)
        set(SDL_SHARED OFF CACHE BOOL "Build a shared version of SDL3" FORCE)
        set(SDL_STATIC ON CACHE BOOL "Build a static version of SDL3" FORCE)
        set(SDL_TEST_LIBRARY OFF CACHE BOOL "Build the SDL3 test helper library" FORCE)
        set(SDL_TESTS OFF CACHE BOOL "Build the SDL3 test programs" FORCE)
        set(SDL_EXAMPLES OFF CACHE BOOL "Build the SDL3 example programs" FORCE)
        FetchContent_Declare(
            SDL3
            URL "https://github.com/libsdl-org/SDL/archive/refs/tags/release-3.4.12.zip"
            DOWNLOAD_EXTRACT_TIMESTAMP TRUE)
        FetchContent_MakeAvailable(SDL3)
    endif()

    if (TARGET SDL3::SDL3)
        target_link_libraries(liminal_cornell_renderer PRIVATE SDL3::SDL3)
        target_compile_definitions(liminal_cornell_renderer PRIVATE LIMINAL_HAVE_SDL3=1)
    else()
        message(FATAL_ERROR "SDL3 frontend requested but SDL3::SDL3 target is not available")
    endif()

    find_package(SDL3_ttf CONFIG QUIET)
    if (NOT SDL3_ttf_FOUND AND NOT TARGET SDL3_ttf::SDL3_ttf)
        include(FetchContent)
        set(SDLTTF_SAMPLES OFF CACHE BOOL "Build SDL3_ttf samples" FORCE)
        set(SDLTTF_INSTALL OFF CACHE BOOL "Install SDL3_ttf" FORCE)
        set(SDLTTF_VENDORED ON CACHE BOOL "Use vendored dependencies for SDL3_ttf" FORCE)
        set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build static libraries" FORCE)
        FetchContent_Declare(
            SDL3_ttf
            GIT_REPOSITORY "https://github.com/libsdl-org/SDL_ttf.git"
            GIT_TAG "release-3.2.2"
            GIT_SHALLOW TRUE
            GIT_SUBMODULES_RECURSE TRUE)
        FetchContent_MakeAvailable(SDL3_ttf)
    endif()

    if (TARGET SDL3_ttf::SDL3_ttf)
        target_link_libraries(liminal_cornell_renderer PRIVATE SDL3_ttf::SDL3_ttf)
        target_compile_definitions(liminal_cornell_renderer PRIVATE LIMINAL_HAVE_SDL3_TTF=1)
    else()
        message(FATAL_ERROR "SDL3 frontend requested but SDL3_ttf::SDL3_ttf target is not available")
    endif()
endif()

if (DEFINED MATH_LIBRARY AND "${MATH_LIBRARY}" MATCHES "NOTFOUND$")
    unset(MATH_LIBRARY CACHE)
    unset(MATH_LIBRARY)
endif()

set(LIMINAL_LLAMA_VENDORED_COMMIT "not-vendored")
if (EXISTS "${LIMINAL_LLAMA_CPP_DIR}/VENDORED_COMMIT.txt")
    file(READ "${LIMINAL_LLAMA_CPP_DIR}/VENDORED_COMMIT.txt" LIMINAL_LLAMA_VENDORED_COMMIT_RAW)
    string(STRIP "${LIMINAL_LLAMA_VENDORED_COMMIT_RAW}" LIMINAL_LLAMA_VENDORED_COMMIT)
endif()

if (LIMINAL_ENABLE_LLAMA_CPP)
    if (EXISTS "${LIMINAL_LLAMA_CPP_DIR}/CMakeLists.txt")
        set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared libraries" FORCE)
        set(LLAMA_BUILD_COMMON OFF CACHE BOOL "Build llama.cpp common helpers" FORCE)
        set(LLAMA_BUILD_TESTS OFF CACHE BOOL "Build llama.cpp tests" FORCE)
        set(LLAMA_BUILD_TOOLS OFF CACHE BOOL "Build llama.cpp tools" FORCE)
        set(LLAMA_BUILD_EXAMPLES OFF CACHE BOOL "Build llama.cpp examples" FORCE)
        set(LLAMA_BUILD_SERVER OFF CACHE BOOL "Build llama.cpp server" FORCE)
        set(LLAMA_BUILD_APP OFF CACHE BOOL "Build llama.cpp app" FORCE)
        set(LLAMA_BUILD_UI OFF CACHE BOOL "Build llama.cpp embedded UI" FORCE)
        set(LLAMA_OPENSSL OFF CACHE BOOL "Enable OpenSSL support in llama.cpp" FORCE)
        if (LIMINAL_ENABLE_LLAMA_CUDA)
            set(GGML_CUDA ON CACHE BOOL "Enable CUDA backend for ggml" FORCE)
        else()
            set(GGML_CUDA OFF CACHE BOOL "Enable CUDA backend for ggml" FORCE)
        endif()

        add_subdirectory("${LIMINAL_LLAMA_CPP_DIR}" "${CMAKE_BINARY_DIR}/vendor-llama.cpp" EXCLUDE_FROM_ALL)
        target_link_libraries(liminal_cornell_renderer PRIVATE llama)
        target_compile_definitions(liminal_cornell_renderer PRIVATE LIMINAL_HAVE_LLAMA_CPP=1)
    else()
        message(WARNING "LIMINAL_ENABLE_LLAMA_CPP is ON but no vendorized llama.cpp tree was found at ${LIMINAL_LLAMA_CPP_DIR}")
    endif()
endif()

target_compile_definitions(
    liminal_cornell_renderer
    PRIVATE LIMINAL_LLAMA_VENDORED_COMMIT="${LIMINAL_LLAMA_VENDORED_COMMIT}")

if(MSVC)
    target_compile_options(liminal_cornell_renderer PRIVATE /W4 /wd4996)
else()
    target_compile_options(liminal_cornell_renderer PRIVATE -Wall -Wextra -Wpedantic)
endif()
