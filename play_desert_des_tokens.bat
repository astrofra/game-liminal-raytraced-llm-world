@echo off
setlocal

set "ROOT_DIR=%~dp0"
if "%ROOT_DIR:~-1%"=="\" set "ROOT_DIR=%ROOT_DIR:~0,-1%"
set "BUILD_DIR=%ROOT_DIR%\build"
set "EXE_PATH=%BUILD_DIR%\Release\liminal_cornell_renderer.exe"
set "MODEL_PATH=%ROOT_DIR%\models\ministral-3-8b\Ministral-3-8B-Instruct-2512-Q4_K_M.gguf"
set "OUTPUT_DIR=%ROOT_DIR%\output"
set "STATE_PATH=%OUTPUT_DIR%\sdl_session_state.json"

if not exist "%EXE_PATH%" (
    echo.
    echo === Build Fresh Release ===
    set "NO_PAUSE=1"
    call "%ROOT_DIR%\build_release.bat"
    if errorlevel 1 goto :end
)

if not exist "%MODEL_PATH%" (
    echo Model file was not found:
    echo "%MODEL_PATH%"
    echo Download it first with:
    echo   download_ministral.bat
    exit /b 1
)

if not exist "%OUTPUT_DIR%" mkdir "%OUTPUT_DIR%"

echo.
echo === Launch Desert des Tokens ===
echo EXE: "%EXE_PATH%"
echo State: "%STATE_PATH%"
echo.
echo Optional extra args:
echo   play_desert_des_tokens.bat --location roof_watch
echo   play_desert_des_tokens.bat --load-state output\sdl_session_state.json
echo.

"%EXE_PATH%" --sdl --location gate --save-state "%STATE_PATH%" %*
if errorlevel 1 goto :end

:end
set "SCRIPT_EXIT_CODE=%errorlevel%"
echo.
if not defined NO_PAUSE pause
exit /b %SCRIPT_EXIT_CODE%
