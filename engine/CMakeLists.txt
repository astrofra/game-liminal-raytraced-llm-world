# Dedicated Gemma 4 E4B inference engine (see engine/core/e4b_spec.h).
include(FetchContent)

# JSON-schema constrained decoding. Pinned; set FETCHCONTENT_SOURCE_DIR_XGRAMMAR
# to an already patched checkout to build offline.
set(XGRAMMAR_BUILD_PYTHON_BINDINGS OFF CACHE BOOL "" FORCE)
set(XGRAMMAR_BUILD_CXX_TESTS OFF CACHE BOOL "" FORCE)
set(XGRAMMAR_ENABLE_CPPTRACE OFF CACHE BOOL "" FORCE)
FetchContent_Declare(xgrammar
    GIT_REPOSITORY https://github.com/mlc-ai/xgrammar.git
    GIT_TAG 1c9556dd4cd472d3a5eaff5e7e01cd7584105246
    GIT_SUBMODULES 3rdparty/picojson 3rdparty/dlpack
    PATCH_COMMAND ${CMAKE_COMMAND} -DXGRAMMAR_SOURCE_DIR=<SOURCE_DIR>
        -P ${PROJECT_SOURCE_DIR}/cmake/patch_xgrammar.cmake)
FetchContent_MakeAvailable(xgrammar)

# macOS compute backend: MLX/Metal, pinned (v0.32.0), built without its optional formats.
option(LIMINAL_ENABLE_MLX "Build the MLX/Metal backend of the E4B engine" ${APPLE})
if (LIMINAL_ENABLE_MLX)
    set(MLX_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(MLX_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(MLX_BUILD_BENCHMARKS OFF CACHE BOOL "" FORCE)
    set(MLX_BUILD_PYTHON_BINDINGS OFF CACHE BOOL "" FORCE)
    set(MLX_BUILD_PYTHON_STUBS OFF CACHE BOOL "" FORCE)
    set(MLX_BUILD_GGUF OFF CACHE BOOL "" FORCE)
    set(MLX_BUILD_SAFETENSORS OFF CACHE BOOL "" FORCE)
    set(MLX_BUILD_METAL ON CACHE BOOL "" FORCE)
    set(MLX_BUILD_CPU ON CACHE BOOL "" FORCE)
    FetchContent_Declare(mlx
        GIT_REPOSITORY https://github.com/ml-explore/mlx.git
        GIT_TAG 7a1d4f5c12ac82f4b4d0a6e71538d89ca0605247)
    FetchContent_MakeAvailable(mlx)
    add_library(liminal_llm_engine_mlx STATIC mlx/mlx_backend.cpp)
    target_include_directories(liminal_llm_engine_mlx PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    target_compile_features(liminal_llm_engine_mlx PRIVATE cxx_std_20)
    target_link_libraries(liminal_llm_engine_mlx PUBLIC mlx)
endif()

# Windows/Linux compute backend: native Vulkan (loader opened at run time). On
# macOS it builds against MoltenVK for development only.
if (APPLE)
    option(LIMINAL_ENABLE_VULKAN_LLM "Build the native Vulkan backend of the E4B engine" OFF)
else()
    option(LIMINAL_ENABLE_VULKAN_LLM "Build the native Vulkan backend of the E4B engine" ON)
endif()
if (LIMINAL_ENABLE_VULKAN_LLM)
    find_package(Vulkan REQUIRED COMPONENTS glslc)
    set(LIMINAL_VULKAN_SHADERS gemv_q4k gemv_q6k gemv_f16 dequant_q4k dequant_q6k gemm rms_norm add_norm
        qkv_prep attn_decode attn_reduce softmax_mask geglu ple_prep)
    set(LIMINAL_SPV_DIR "${CMAKE_CURRENT_BINARY_DIR}/spv")
    file(MAKE_DIRECTORY "${LIMINAL_SPV_DIR}")
    set(LIMINAL_SPV_OUTPUTS)
    file(GLOB LIMINAL_VULKAN_SHADER_INCLUDES "${CMAKE_CURRENT_SOURCE_DIR}/vulkan/shaders/*.glsl")
    foreach(shader IN LISTS LIMINAL_VULKAN_SHADERS)
        set(source "${CMAKE_CURRENT_SOURCE_DIR}/vulkan/shaders/${shader}.comp")
        set(output "${LIMINAL_SPV_DIR}/${shader}.spv.inc")
        # -mfmt=num emits the words as a comma-separated list for an array initializer.
        add_custom_command(OUTPUT "${output}"
            COMMAND Vulkan::glslc -std=450 --target-env=vulkan1.2 -O -mfmt=num -o "${output}" "${source}"
            DEPENDS "${source}" ${LIMINAL_VULKAN_SHADER_INCLUDES}
            COMMENT "glslc ${shader}.comp"
            VERBATIM)
        list(APPEND LIMINAL_SPV_OUTPUTS "${output}")
    endforeach()
    add_custom_target(liminal_vulkan_shaders DEPENDS ${LIMINAL_SPV_OUTPUTS})
    add_library(liminal_llm_engine_vulkan STATIC vulkan/vulkan_backend.cpp)
    add_dependencies(liminal_llm_engine_vulkan liminal_vulkan_shaders)
    target_include_directories(liminal_llm_engine_vulkan PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}" "${LIMINAL_SPV_DIR}")
    target_compile_features(liminal_llm_engine_vulkan PRIVATE cxx_std_17)
    target_link_libraries(liminal_llm_engine_vulkan PRIVATE Vulkan::Headers ${CMAKE_DL_LIBS})
endif()

add_library(liminal_llm_engine STATIC
    core/gguf.cpp
    core/e4b_spec.cpp
    core/quant_ref.cpp
    core/tokenizer.cpp
    core/chat_format.cpp
    core/sampler.cpp
    core/grammar.cpp
    core/engine.cpp
    ref/forward_ref.cpp)
target_include_directories(liminal_llm_engine PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(liminal_llm_engine PUBLIC cxx_std_17)
target_include_directories(liminal_llm_engine PRIVATE "${LIMINAL_VENDOR_DIR}")
target_link_libraries(liminal_llm_engine PRIVATE xgrammar)
# grammar.cpp uses xgrammar's schema-to-EBNF converter from its source tree.
target_include_directories(liminal_llm_engine PRIVATE "${xgrammar_SOURCE_DIR}/cpp")
target_compile_definitions(liminal_llm_engine PRIVATE LIMINAL_ENGINE_HAVE_XGRAMMAR=1)
if (LIMINAL_ENABLE_MLX)
    target_link_libraries(liminal_llm_engine PUBLIC liminal_llm_engine_mlx)
    target_compile_definitions(liminal_llm_engine PRIVATE LIMINAL_ENGINE_HAVE_MLX=1)
endif()
if (LIMINAL_ENABLE_VULKAN_LLM)
    target_link_libraries(liminal_llm_engine PUBLIC liminal_llm_engine_vulkan)
    target_compile_definitions(liminal_llm_engine PRIVATE LIMINAL_ENGINE_HAVE_VULKAN=1)
endif()
if(MSVC)
    target_compile_options(liminal_llm_engine PRIVATE /W4 /utf-8)
else()
    target_compile_options(liminal_llm_engine PRIVATE -Wall -Wextra)
endif()
if (LIMINAL_ENABLE_OPENMP AND OpenMP_CXX_FOUND)
    target_link_libraries(liminal_llm_engine PRIVATE OpenMP::OpenMP_CXX)
endif()

add_executable(liminal_engine_cli tools/engine_cli.cpp)
target_include_directories(liminal_engine_cli PRIVATE "${LIMINAL_VENDOR_DIR}")
target_link_libraries(liminal_engine_cli PRIVATE liminal_llm_engine)
if (LIMINAL_ENABLE_MLX)
    target_compile_definitions(liminal_engine_cli PRIVATE LIMINAL_ENGINE_HAVE_MLX=1)
endif()
if (LIMINAL_ENABLE_VULKAN_LLM)
    target_compile_definitions(liminal_engine_cli PRIVATE LIMINAL_ENGINE_HAVE_VULKAN=1)
endif()
