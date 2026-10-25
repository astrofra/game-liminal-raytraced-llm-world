@echo off
setlocal

set "ROOT_DIR=%~dp0"
if "%ROOT_DIR:~-1%"=="\" set "ROOT_DIR=%ROOT_DIR:~0,-1%"
set "EXE_PATH=%ROOT_DIR%\build\Release\liminal_cornell_renderer.exe"

echo.
echo === Build Release with MSVC and Ninja ===
python "%ROOT_DIR%\scripts\build_native.py" --publish %*
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
