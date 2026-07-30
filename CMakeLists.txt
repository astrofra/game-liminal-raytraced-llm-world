cmake_minimum_required(VERSION 3.20)

project(liminal_raytraced_renderer LANGUAGES CXX)

option(LIMINAL_ENABLE_LLAMA_CPP "Build with vendorized llama.cpp runtime support" ON)
option(LIMINAL_ENABLE_LLAMA_CUDA "Enable CUDA backend in vendorized llama.cpp build" ON)

set(LIMINAL_LLAMA_CPP_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vendor/llama.cpp" CACHE PATH "Path to the vendorized llama.cpp tree")

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(liminal_cornell_renderer
    src/main.cpp
    src/llm_runtime.cpp
    src/scene.cpp
    src/renderer.cpp
)

target_include_directories(liminal_cornell_renderer PRIVATE src vendor/stb)

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
