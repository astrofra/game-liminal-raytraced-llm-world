@echo off
setlocal

set "ROOT_DIR=%~dp0"
if "%ROOT_DIR:~-1%"=="\" set "ROOT_DIR=%ROOT_DIR:~0,-1%"
set "SCRIPT_PATH=%ROOT_DIR%\scripts\download_ministral.py"

echo.
echo === Check Python ===
python --version >nul 2>&1
if errorlevel 1 (
    echo Python was not found in PATH.
    echo Install Python 3.10+ and retry.
    exit /b 1
)

echo.
echo === Check huggingface_hub ===
python -c "import huggingface_hub" >nul 2>&1
if errorlevel 1 (
    echo huggingface_hub is missing. Installing a compatible version...
    python -m pip install "huggingface_hub<1.0"
    if errorlevel 1 goto :end
) else (
    echo huggingface_hub already available.
)

if not exist "%SCRIPT_PATH%" (
    echo Download script not found:
    echo "%SCRIPT_PATH%"
    exit /b 1
)

echo.
echo === Download Ministral 3 8B GGUF ===
python "%SCRIPT_PATH%" %*
if errorlevel 1 goto :end

echo.
echo Download helper finished.

:end
set "SCRIPT_EXIT_CODE=%errorlevel%"
echo.
if not defined NO_PAUSE pause
exit /b %SCRIPT_EXIT_CODE%
