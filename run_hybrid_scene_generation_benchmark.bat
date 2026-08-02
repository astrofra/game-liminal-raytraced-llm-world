@echo off
setlocal

set "ROOT_DIR=%~dp0"
if "%ROOT_DIR:~-1%"=="\" set "ROOT_DIR=%ROOT_DIR:~0,-1%"

set "SCRIPT_PATH=%ROOT_DIR%\scripts\run_hybrid_scene_generation_benchmark.py"
set "RENDERER_PATH=%ROOT_DIR%\build\Release\liminal_cornell_renderer.exe"
set "MODEL_PATH=%ROOT_DIR%\models\ministral-3-8b\Ministral-3-8B-Instruct-2512-Q4_K_M.gguf"
set "PYTHON_CMD="

if not exist "%SCRIPT_PATH%" (
    echo Hybrid benchmark script not found:
    echo "%SCRIPT_PATH%"
    exit /b 1
)

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

if not exist "%MODEL_PATH%" (
    echo Model file was not found:
    echo "%MODEL_PATH%"
    echo Download it first with:
    echo   download_ministral.bat
    exit /b 1
)

py -3 --version >nul 2>&1
if not errorlevel 1 (
    set "PYTHON_CMD=py -3"
) else (
    python --version >nul 2>&1
    if errorlevel 1 (
        echo Python 3 was not found in PATH.
        echo Install Python 3.10+ and retry.
        exit /b 1
    )
    set "PYTHON_CMD=python"
)

echo.
echo === Run Hybrid Scene Generation Benchmark ===
echo Renderer: "%RENDERER_PATH%"
echo Model: "%MODEL_PATH%"
echo Script: "%SCRIPT_PATH%"
echo.

call %PYTHON_CMD% "%SCRIPT_PATH%" --renderer "%RENDERER_PATH%" --model "%MODEL_PATH%" %*
if errorlevel 1 goto :end

echo.
echo Hybrid scene generation benchmark finished.

:end
set "SCRIPT_EXIT_CODE=%errorlevel%"
echo.
if not defined NO_PAUSE pause
exit /b %SCRIPT_EXIT_CODE%
