@echo off
setlocal

set "ROOT_DIR=%~dp0"
if "%ROOT_DIR:~-1%"=="\" set "ROOT_DIR=%ROOT_DIR:~0,-1%"

set "SCRIPT_PATH=%ROOT_DIR%\scripts\run_scene_generation_benchmark.py"
set "RENDERER_PATH=%ROOT_DIR%\build\Release\liminal_cornell_renderer.exe"
set "LLAMA_CLI=%ROOT_DIR%\build\bin\Release\llama-cli.exe"
set "PYTHON_CMD="

if not exist "%SCRIPT_PATH%" (
    echo Scene generation benchmark script not found:
    echo "%SCRIPT_PATH%"
    exit /b 1
)

if "%~1"=="--help" goto :select_python
if "%~1"=="-h" goto :select_python

if not exist "%RENDERER_PATH%" (
    echo.
    echo === Build Fresh Release ===
    set "NO_PAUSE=1"
    call "%ROOT_DIR%\build_release.bat"
    if errorlevel 1 goto :end
)

if not exist "%RENDERER_PATH%" (
    echo Renderer was not found after build:
    echo "%RENDERER_PATH%"
    exit /b 1
)

if not exist "%LLAMA_CLI%" (
    set "NO_PAUSE=1"
    call "%ROOT_DIR%\build_release.bat" --with-cli
    if errorlevel 1 goto :end
)

:select_python
py -3 --version >nul 2>&1
if not errorlevel 1 (
    set "PYTHON_CMD=py -3"
) else (
    python --version >nul 2>&1
    if errorlevel 1 (
        echo Python 3 was not found in PATH.
        echo Install Python 3.11+ and retry.
        exit /b 1
    )
    set "PYTHON_CMD=python"
)

echo.
echo === Run Scene Generation Benchmark ===
echo Renderer: "%RENDERER_PATH%"
echo LLM: "%LLAMA_CLI%"
echo Profile: gemma-4-e4b-q4.
echo Script: "%SCRIPT_PATH%"
echo.

call %PYTHON_CMD% "%SCRIPT_PATH%" --renderer "%RENDERER_PATH%" --llama-cli "%LLAMA_CLI%" %*
set "SCRIPT_EXIT_CODE=%errorlevel%"
if not "%SCRIPT_EXIT_CODE%"=="0" goto :finish

echo.
echo Scene generation benchmark finished.
goto :finish

:end
set "SCRIPT_EXIT_CODE=%errorlevel%"
:finish
echo.
if not defined NO_PAUSE pause
exit /b %SCRIPT_EXIT_CODE%
