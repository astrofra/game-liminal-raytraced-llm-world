@echo off
setlocal

set "ROOT_DIR=%~dp0"
if "%ROOT_DIR:~-1%"=="\" set "ROOT_DIR=%ROOT_DIR:~0,-1%"

call "%ROOT_DIR%\run_hybrid_scene_generation_benchmark.bat" ^
  --cases "%ROOT_DIR%\scripts\local_room_variation_benchmark_cases.json" ^
  --output-root "%ROOT_DIR%\documentation\generated\local_room_variation_benchmark" ^
  --markdown "%ROOT_DIR%\documentation\LOCAL_ROOM_VARIATION_BENCHMARK.md" ^
  %*

exit /b %errorlevel%
