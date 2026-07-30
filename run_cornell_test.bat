@echo off
setlocal

set "ROOT_DIR=%~dp0"
if "%ROOT_DIR:~-1%"=="\" set "ROOT_DIR=%ROOT_DIR:~0,-1%"
set "BUILD_DIR=%ROOT_DIR%\build"
set "EXE_PATH=%BUILD_DIR%\Release\liminal_cornell_renderer.exe"
set "OUTPUT_DIR=%ROOT_DIR%\output"
set "OUTPUT_PATH=%OUTPUT_DIR%\cornell_box_test.png"
set "SCENE_PATH=%ROOT_DIR%\assets\cornell\cornell_box.obj"

echo.
echo === Build Fresh Release ===
set "NO_PAUSE=1"
call "%ROOT_DIR%\build_release.bat"
if errorlevel 1 goto :end

if not exist "%OUTPUT_DIR%" mkdir "%OUTPUT_DIR%"

echo.
echo === Run Cornell Box Test ===
"%EXE_PATH%" --scene "%SCENE_PATH%" --samples 32 --width 256 --height 256 --output "%OUTPUT_PATH%"
if errorlevel 1 goto :end

echo.
if exist "%OUTPUT_PATH%" (
    echo Cornell Box test OK
    echo Image: "%OUTPUT_PATH%"
) else (
    echo Cornell Box run finished but output not found at:
    echo "%OUTPUT_PATH%"
    exit /b 1
)

:end
set "SCRIPT_EXIT_CODE=%errorlevel%"
echo.
pause
exit /b %SCRIPT_EXIT_CODE%
