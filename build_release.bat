@echo off
setlocal

set "ROOT_DIR=%~dp0"
if "%ROOT_DIR:~-1%"=="\" set "ROOT_DIR=%ROOT_DIR:~0,-1%"
set "BUILD_DIR=%ROOT_DIR%\build"
set "GENERATOR=Visual Studio 17 2022"
set "EXE_PATH=%BUILD_DIR%\Release\liminal_cornell_renderer.exe"

echo.
echo === Configure CMake ===
cmake -S "%ROOT_DIR%" -B "%BUILD_DIR%" -G "%GENERATOR%"
if errorlevel 1 goto :end

echo.
echo === Build Release ===
cmake --build "%BUILD_DIR%" --config Release
if errorlevel 1 goto :end

echo.
if exist "%EXE_PATH%" (
    echo Build OK
    echo EXE: "%EXE_PATH%"
) else (
    echo Build finished but EXE not found at:
    echo "%EXE_PATH%"
    exit /b 1
)

:end
set "SCRIPT_EXIT_CODE=%errorlevel%"
echo.
if not defined NO_PAUSE pause
exit /b %SCRIPT_EXIT_CODE%
