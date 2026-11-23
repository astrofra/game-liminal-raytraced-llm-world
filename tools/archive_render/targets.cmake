add_executable(liminal_archive_render
    "${LIMINAL_ROOT}/src/archive_render.cpp"
    "${LIMINAL_ROOT}/src/scene_hash.cpp"
    "${LIMINAL_ROOT}/src/scene.cpp"
    "${LIMINAL_ROOT}/src/renderer.cpp")
target_include_directories(liminal_archive_render PRIVATE
    "${LIMINAL_ROOT}/src" "${LIMINAL_ROOT}/vendor/stb"
    "${LIMINAL_ROOT}/vendor/llama.cpp/vendor" "${LIMINAL_ROOT}/vendor/llama.cpp/vendor/hash")
if(MSVC)
    target_compile_options(liminal_archive_render PRIVATE /W4 /wd4996 /utf-8)
endif()
if(WIN32)
    target_link_libraries(liminal_archive_render PRIVATE gdi32 user32)
endif()
find_package(OpenMP QUIET)
if(OpenMP_CXX_FOUND)
    target_link_libraries(liminal_archive_render PRIVATE OpenMP::OpenMP_CXX)
    target_compile_definitions(liminal_archive_render PRIVATE LIMINAL_ENABLE_OPENMP=1)
endif()
add_executable(liminal_archive_camera_test
    "${LIMINAL_ROOT}/tests/archive_camera_test.cpp"
    "${LIMINAL_ROOT}/src/scene.cpp"
    "${LIMINAL_ROOT}/src/renderer.cpp")
target_include_directories(liminal_archive_camera_test PRIVATE "${LIMINAL_ROOT}/src" "${LIMINAL_ROOT}/vendor/stb")
if(MSVC)
    target_compile_options(liminal_archive_camera_test PRIVATE /W4 /wd4996 /utf-8)
endif()
add_test(NAME archive_camera COMMAND liminal_archive_camera_test)
find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(NAME archive_render_cli COMMAND "${Python3_EXECUTABLE}"
    "${LIMINAL_ROOT}/tests/test_archive_render.py" "$<TARGET_FILE:liminal_archive_render>")
