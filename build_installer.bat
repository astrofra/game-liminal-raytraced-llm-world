@echo off
setlocal

set "ROOT_DIR=%~dp0"
if "%ROOT_DIR:~-1%"=="\" set "ROOT_DIR=%ROOT_DIR:~0,-1%"
set "SCRIPT_PATH=%ROOT_DIR%\scripts\build_installer.py"

echo.
echo === Build Within the Latent Walls Installer ===

python --version >nul 2>&1
if errorlevel 1 (
    echo Python 3.10 or newer was not found in PATH.
    exit /b 1
)

if not exist "%SCRIPT_PATH%" (
    echo Installer build script not found:
    echo "%SCRIPT_PATH%"
    exit /b 1
)

python "%SCRIPT_PATH%" %*

:end
set "SCRIPT_EXIT_CODE=%errorlevel%"
echo.
if not defined NO_PAUSE pause
exit /b %SCRIPT_EXIT_CODE%
