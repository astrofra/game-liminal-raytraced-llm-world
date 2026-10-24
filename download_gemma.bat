@echo off
setlocal

set "ROOT_DIR=%~dp0"
if "%ROOT_DIR:~-1%"=="\" set "ROOT_DIR=%ROOT_DIR:~0,-1%"
set "SCRIPT_PATH=%ROOT_DIR%\scripts\download_gemma.py"

echo.
echo === Check Python ===
python --version >nul 2>&1
if errorlevel 1 (
    echo Python was not found in PATH.
    echo Install Python 3.10+ and retry.
    exit /b 1
)

if not exist "%SCRIPT_PATH%" (
    echo Download script not found:
    echo "%SCRIPT_PATH%"
    exit /b 1
)

echo.
echo === Download Gemma 4 E4B GGUF ===
python "%SCRIPT_PATH%" %*
set "SCRIPT_EXIT_CODE=%errorlevel%"

if not "%SCRIPT_EXIT_CODE%"=="0" (
    echo.
    echo Download helper failed with exit code %SCRIPT_EXIT_CODE%.
)

echo.
if not defined NO_PAUSE pause
exit /b %SCRIPT_EXIT_CODE%
