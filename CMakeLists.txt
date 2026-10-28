cmake_minimum_required(VERSION 3.20)

# MLX v0.32 kernels target Metal 4.0: macOS 26.2 enables the M5 neural-accelerator
# (NAX) kernels, while a newer host default would select Metal 4.1, which they reject.
if (CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin" AND NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "26.2" CACHE STRING "Minimum macOS version" FORCE)
endif()

project(liminal_raytraced_renderer LANGUAGES CXX)

option(LIMINAL_ENABLE_OPENMP "Enable OpenMP render parallelism when available" ON)
option(LIMINAL_ENABLE_SDL3_FRONTEND "Build the SDL3 interactive frontend" ON)
set(LIMINAL_VENDOR_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vendor")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(LIMINAL_SOURCES
    src/animated_view.cpp
    src/main.cpp
    src/game_state.cpp
    src/investigation.cpp
    src/investigation_distribution.cpp
    src/investigation_text.cpp
    src/known_barrier.cpp
    src/llm_runtime.cpp
    src/llm_backend_native.cpp
    src/continuity.cpp
    src/research_archive.cpp
    src/scene_design.cpp
    src/scene_hash.cpp
    src/coplanar_cleanup.cpp
    src/scene_validation.cpp
    src/scene_normalization.cpp
    src/dynamic_scene.cpp
    src/scene.cpp
    src/glass_supports.cpp
    src/scene_compiler.cpp
    src/renderer.cpp
    src/turn_contract.cpp
    src/place_proposal.cpp
    src/exploration_policy.cpp
    src/inventory.cpp
    src/object_interactions.cpp
    src/object_resolution.cpp
    src/object_render.cpp
    src/narrative_language.cpp
    src/turn_runner.cpp
    src/view_post_process.cpp)

if (LIMINAL_ENABLE_SDL3_FRONTEND)
    list(APPEND LIMINAL_SOURCES src/sdl_frontend.cpp)
endif()

find_package(Python3 REQUIRED COMPONENTS Interpreter)
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS assets/scene_generation/local_v1.json)
file(READ "${CMAKE_CURRENT_SOURCE_DIR}/assets/scene_generation/local_v1.json" LIMINAL_LOCAL_SCENE_CONTRACT)
configure_file(src/local_scene_contract.generated.h.in generated/local_scene_contract.generated.h @ONLY)
set(MODEL_HEADER "${CMAKE_CURRENT_BINARY_DIR}/generated/model_profiles.generated.h")
add_custom_command(OUTPUT "${MODEL_HEADER}"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate_model_header.py" "${MODEL_HEADER}"
    DEPENDS scripts/generate_model_header.py scripts/model_profiles.py scripts/models.json
    VERBATIM)
set(SCENE_GUIDE_HEADER "${CMAKE_CURRENT_BINARY_DIR}/generated/scene_composition.generated.h")
add_custom_command(OUTPUT "${SCENE_GUIDE_HEADER}"
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/scripts/embed_scene_guide.py"
        "${CMAKE_CURRENT_SOURCE_DIR}/assets/scene_generation/composition_v1.txt" "${SCENE_GUIDE_HEADER}"
    DEPENDS scripts/embed_scene_guide.py assets/scene_generation/composition_v1.txt
    VERBATIM)
add_executable(liminal_cornell_renderer ${LIMINAL_SOURCES} "${MODEL_HEADER}" "${SCENE_GUIDE_HEADER}")
add_library(liminal_ambience STATIC src/ambience.cpp)
target_include_directories(liminal_ambience PUBLIC src PRIVATE "${LIMINAL_VENDOR_DIR}")
if(MSVC)
    target_compile_options(liminal_ambience PRIVATE /W4 /utf-8)
endif()
target_link_libraries(liminal_cornell_renderer PRIVATE liminal_ambience)
target_include_directories(liminal_cornell_renderer PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
add_executable(liminal_llm_channels_test tests/llm_channels_test.cpp)
target_include_directories(liminal_llm_channels_test PRIVATE src)
enable_testing()
add_executable(liminal_ambience_test tests/ambience_test.cpp src/place_proposal.cpp)
target_link_libraries(liminal_ambience_test PRIVATE liminal_ambience)
target_include_directories(liminal_ambience_test PRIVATE "${LIMINAL_VENDOR_DIR}")
if(MSVC)
    target_compile_options(liminal_ambience_test PRIVATE /W4 /utf-8)
endif()
add_test(NAME ambience COMMAND liminal_ambience_test)
add_test(NAME final_channels COMMAND liminal_llm_channels_test)

add_executable(liminal_glass_supports src/glass_supports_cli.cpp src/glass_supports.cpp src/scene.cpp src/renderer.cpp)
add_executable(liminal_glass_supports_test tests/glass_supports_test.cpp src/glass_supports.cpp src/scene.cpp)
add_executable(liminal_glass_supports_geometry_test tests/glass_supports_geometry_test.cpp src/glass_supports.cpp src/scene.cpp src/renderer.cpp)
foreach(target liminal_glass_supports liminal_glass_supports_test liminal_glass_supports_geometry_test)
    target_include_directories(${target} PRIVATE src vendor/stb "${LIMINAL_VENDOR_DIR}")
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /wd4996 /utf-8)
    endif()
endforeach()
add_test(NAME glass_supports COMMAND liminal_glass_supports_test)
add_test(NAME glass_supports_geometry COMMAND liminal_glass_supports_geometry_test)

add_executable(liminal_coplanar_cleanup src/coplanar_cleanup_cli.cpp src/coplanar_cleanup.cpp src/scene_hash.cpp src/glass_supports.cpp src/scene.cpp src/renderer.cpp src/animated_view.cpp)
add_executable(liminal_coplanar_cleanup_test tests/coplanar_cleanup_test.cpp src/coplanar_cleanup.cpp src/scene_hash.cpp src/glass_supports.cpp src/scene.cpp)
add_executable(liminal_coplanar_cleanup_geometry_test tests/coplanar_cleanup_geometry_test.cpp src/coplanar_cleanup.cpp src/scene_hash.cpp src/glass_supports.cpp src/scene.cpp src/animated_view.cpp)
foreach(target liminal_coplanar_cleanup liminal_coplanar_cleanup_test liminal_coplanar_cleanup_geometry_test)
    target_include_directories(${target} PRIVATE src vendor/stb "${LIMINAL_VENDOR_DIR}" "${LIMINAL_VENDOR_DIR}/hash")
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /wd4996 /utf-8)
    endif()
endforeach()
add_test(NAME coplanar_cleanup COMMAND liminal_coplanar_cleanup_test)
add_test(NAME coplanar_cleanup_geometry COMMAND liminal_coplanar_cleanup_geometry_test)

add_executable(liminal_distant_light_test tests/distant_light_test.cpp src/scene.cpp src/renderer.cpp)
target_include_directories(liminal_distant_light_test PRIVATE src vendor/stb)
if(MSVC)
    target_compile_options(liminal_distant_light_test PRIVATE /W4 /wd4996 /utf-8)
endif()
add_test(NAME distant_light COMMAND liminal_distant_light_test)

add_executable(liminal_dynamic_scene_test tests/dynamic_scene_test.cpp tests/object_journey_test.cpp tests/object_conservation_test.cpp tests/object_narrative_test.cpp tests/object_semantic_test.cpp
    tests/audio_turn_test.cpp tests/investigation_contract_test.cpp tests/investigation_journey_test.cpp tests/investigation_distribution_test.cpp tests/known_barrier_test.cpp
    src/investigation.cpp src/investigation_distribution.cpp src/investigation_text.cpp src/known_barrier.cpp
    src/scene_design.cpp src/scene_validation.cpp src/scene_normalization.cpp src/dynamic_scene.cpp src/scene.cpp src/renderer.cpp
    src/game_state.cpp src/object_interactions.cpp src/object_resolution.cpp src/object_render.cpp src/place_proposal.cpp src/exploration_policy.cpp src/inventory.cpp src/narrative_language.cpp src/scene_compiler.cpp src/turn_runner.cpp src/turn_contract.cpp src/llm_runtime.cpp src/llm_backend_native.cpp
    src/research_archive.cpp src/continuity.cpp src/glass_supports.cpp src/coplanar_cleanup.cpp src/scene_hash.cpp src/animated_view.cpp
    "${MODEL_HEADER}" "${SCENE_GUIDE_HEADER}")
target_include_directories(liminal_dynamic_scene_test PRIVATE src vendor/stb "${LIMINAL_VENDOR_DIR}"
    "${LIMINAL_VENDOR_DIR}/hash" "${CMAKE_CURRENT_BINARY_DIR}/generated")
if(MSVC)
    target_compile_options(liminal_dynamic_scene_test PRIVATE /W4 /wd4996 /utf-8)
endif()
add_test(NAME dynamic_scene COMMAND liminal_dynamic_scene_test)
target_link_libraries(liminal_dynamic_scene_test PRIVATE liminal_ambience)
add_executable(liminal_continuity_test tests/continuity_test.cpp src/continuity.cpp)
target_include_directories(liminal_continuity_test PRIVATE src "${LIMINAL_VENDOR_DIR}")
if(MSVC)
    target_compile_options(liminal_continuity_test PRIVATE /W4 /utf-8)
endif()
add_test(NAME continuity_budget COMMAND liminal_continuity_test)
set_tests_properties(dynamic_scene PROPERTIES WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

if(WIN32)
    target_link_libraries(liminal_cornell_renderer PRIVATE shell32 bcrypt)
endif()

target_include_directories(liminal_cornell_renderer PRIVATE src vendor/stb "${LIMINAL_VENDOR_DIR}" "${LIMINAL_VENDOR_DIR}/hash")

if (LIMINAL_ENABLE_OPENMP)
    find_package(OpenMP)
    if (OpenMP_CXX_FOUND)
        target_link_libraries(liminal_cornell_renderer PRIVATE OpenMP::OpenMP_CXX)
        target_compile_definitions(liminal_cornell_renderer PRIVATE LIMINAL_ENABLE_OPENMP=1)
        target_link_libraries(liminal_glass_supports PRIVATE OpenMP::OpenMP_CXX)
        target_compile_definitions(liminal_glass_supports PRIVATE LIMINAL_ENABLE_OPENMP=1)
        target_link_libraries(liminal_coplanar_cleanup PRIVATE OpenMP::OpenMP_CXX)
        target_compile_definitions(liminal_coplanar_cleanup PRIVATE LIMINAL_ENABLE_OPENMP=1)
    else()
        message(STATUS "OpenMP requested but not found; building renderer without OpenMP")
    endif()
endif()

if (LIMINAL_ENABLE_SDL3_FRONTEND)
    find_package(SDL3 3.4.0 CONFIG QUIET)
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
        add_library(liminal_ambience_audio STATIC src/ambience_audio.cpp src/scene_hash.cpp)
        target_include_directories(liminal_ambience_audio PRIVATE vendor/stb "${LIMINAL_VENDOR_DIR}" "${LIMINAL_VENDOR_DIR}/hash")
        target_link_libraries(liminal_ambience_audio PUBLIC liminal_ambience SDL3::SDL3)
        if(MSVC)
            target_compile_options(liminal_ambience_audio PRIVATE /W4 /wd4996 /utf-8)
        endif()
        target_link_libraries(liminal_cornell_renderer PRIVATE liminal_ambience_audio)
        add_executable(liminal_audio_diagnostic src/ambience_cli.cpp)
        target_include_directories(liminal_audio_diagnostic PRIVATE "${LIMINAL_VENDOR_DIR}")
        target_link_libraries(liminal_audio_diagnostic PRIVATE liminal_ambience_audio)
        if(MSVC)
            target_compile_options(liminal_audio_diagnostic PRIVATE /W4 /utf-8)
        endif()
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

# Revision reported as the LLM runtime commit (metrics, --llm-info, installer).
set(LIMINAL_ENGINE_COMMIT "unknown")
find_package(Git QUIET)
if (GIT_FOUND)
    execute_process(COMMAND "${GIT_EXECUTABLE}" rev-parse --short=12 HEAD
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" OUTPUT_VARIABLE LIMINAL_ENGINE_COMMIT_RAW
        RESULT_VARIABLE LIMINAL_ENGINE_COMMIT_STATUS ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
    if (LIMINAL_ENGINE_COMMIT_STATUS EQUAL 0)
        set(LIMINAL_ENGINE_COMMIT "${LIMINAL_ENGINE_COMMIT_RAW}")
    endif()
endif()
target_compile_definitions(liminal_cornell_renderer PRIVATE LIMINAL_ENGINE_COMMIT="${LIMINAL_ENGINE_COMMIT}")

if(MSVC)
    target_compile_options(liminal_cornell_renderer PRIVATE /W4 /wd4996 /utf-8)
else()
    target_compile_options(liminal_cornell_renderer PRIVATE -Wall -Wextra -Wpedantic)
endif()

# The dedicated Gemma 4 E4B engine is the game's only LLM runtime.
add_subdirectory(engine)
add_executable(liminal_engine_tokenizer_test tests/engine_tokenizer_test.cpp)
target_include_directories(liminal_engine_tokenizer_test PRIVATE "${LIMINAL_VENDOR_DIR}")
target_link_libraries(liminal_engine_tokenizer_test PRIVATE liminal_llm_engine)
target_compile_definitions(liminal_engine_tokenizer_test PRIVATE LIMINAL_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
if(MSVC)
    target_compile_options(liminal_engine_tokenizer_test PRIVATE /utf-8)
endif()
add_test(NAME engine_tokenizer COMMAND liminal_engine_tokenizer_test)
target_link_libraries(liminal_cornell_renderer PRIVATE liminal_llm_engine)
target_compile_definitions(liminal_cornell_renderer PRIVATE LIMINAL_HAVE_NATIVE_LLM=1)
set_tests_properties(engine_tokenizer PROPERTIES SKIP_RETURN_CODE 77)
add_executable(liminal_engine_grammar_test tests/engine_grammar_test.cpp)
target_include_directories(liminal_engine_grammar_test PRIVATE "${LIMINAL_VENDOR_DIR}")
target_link_libraries(liminal_engine_grammar_test PRIVATE liminal_llm_engine)
target_compile_definitions(liminal_engine_grammar_test PRIVATE LIMINAL_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
add_test(NAME engine_grammar COMMAND liminal_engine_grammar_test)
set_tests_properties(engine_grammar PROPERTIES SKIP_RETURN_CODE 77)
# Greedy agreement of the platform's GPU device with the frozen llama.cpp goldens.
if (LIMINAL_ENABLE_MLX)
    set(LIMINAL_PARITY_DEVICE mlx)
elseif (LIMINAL_ENABLE_VULKAN_LLM)
    set(LIMINAL_PARITY_DEVICE vulkan)
endif()
if (LIMINAL_PARITY_DEVICE)
    foreach(golden 1793137373113-0000-player_interpretation_v2 1793137373113-0002-scene_program_v1)
        add_test(NAME engine_parity_${golden} COMMAND liminal_engine_cli parity
            --model "${CMAKE_CURRENT_SOURCE_DIR}/models/gemma-4-e4b/gemma-4-E4B-it-Q4_K_M.gguf"
            --golden "${CMAKE_CURRENT_SOURCE_DIR}/tests/data/llm/golden/${golden}.json"
            --steps 64 --device ${LIMINAL_PARITY_DEVICE})
        set_tests_properties(engine_parity_${golden} PROPERTIES SKIP_RETURN_CODE 77)
    endforeach()
endif()

add_executable(liminal_scene_normalization_test tests/scene_normalization_test.cpp src/scene_normalization.cpp)
target_include_directories(liminal_scene_normalization_test PRIVATE src)
add_test(NAME scene_normalization COMMAND liminal_scene_normalization_test)

add_executable(liminal_scene_visibility_test tests/scene_visibility_test.cpp
    src/scene_validation.cpp src/scene_design.cpp src/scene_hash.cpp src/continuity.cpp src/scene.cpp src/renderer.cpp "${SCENE_GUIDE_HEADER}")
target_include_directories(liminal_scene_visibility_test PRIVATE src vendor/stb "${LIMINAL_VENDOR_DIR}" "${LIMINAL_VENDOR_DIR}/hash" "${CMAKE_CURRENT_BINARY_DIR}/generated")
if(MSVC)
    target_compile_options(liminal_scene_visibility_test PRIVATE /utf-8)
endif()
add_test(NAME scene_visibility COMMAND liminal_scene_visibility_test)

add_executable(liminal_place_proposal_test tests/place_proposal_test.cpp src/place_proposal.cpp)
target_include_directories(liminal_place_proposal_test PRIVATE src "${LIMINAL_VENDOR_DIR}")
add_test(NAME place_proposal COMMAND liminal_place_proposal_test)

add_executable(liminal_exploration_policy_test tests/exploration_policy_test.cpp src/exploration_policy.cpp)
target_include_directories(liminal_exploration_policy_test PRIVATE src)
if(MSVC)
    target_compile_options(liminal_exploration_policy_test PRIVATE /W4 /utf-8)
endif()
add_test(NAME exploration_policy COMMAND liminal_exploration_policy_test)

add_executable(liminal_inventory_test tests/inventory_test.cpp src/inventory.cpp)
target_include_directories(liminal_inventory_test PRIVATE src)
if(MSVC)
    target_compile_options(liminal_inventory_test PRIVATE /utf-8)
endif()
add_test(NAME inventory COMMAND liminal_inventory_test)

add_executable(liminal_narrative_language_test tests/narrative_language_test.cpp src/narrative_language.cpp)
target_include_directories(liminal_narrative_language_test PRIVATE src)
if(MSVC)
    target_compile_options(liminal_narrative_language_test PRIVATE /utf-8)
endif()
add_test(NAME narrative_language COMMAND liminal_narrative_language_test)
