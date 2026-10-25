@echo off
setlocal

set "ROOT_DIR=%~dp0"
if "%ROOT_DIR:~-1%"=="\" set "ROOT_DIR=%ROOT_DIR:~0,-1%"
set "BUILD_DIR=%ROOT_DIR%\build"
set "EXE_PATH=%BUILD_DIR%\Release\liminal_cornell_renderer.exe"

if not exist "%EXE_PATH%" (
    echo.
    echo === Build Fresh Release ===
    set "NO_PAUSE=1"
    call "%ROOT_DIR%\build_release.bat"
    if errorlevel 1 goto :end
)

echo.
echo === Within the Latent Walls / Entre les Murs Latents ===
echo EXE: "%EXE_PATH%"
echo Nouvelle partie. Archives : "%LOCALAPPDATA%\WithinTheLatentWalls\research"
echo Depart : seuil de la carriere. Est pour commencer, puis suivre les issues indiquees.
echo.
pushd "%ROOT_DIR%"
"%EXE_PATH%" --sdl %*
set "GAME_EXIT_CODE=%errorlevel%"
popd
if not "%GAME_EXIT_CODE%"=="0" exit /b %GAME_EXIT_CODE%

:end
set "SCRIPT_EXIT_CODE=%errorlevel%"
echo.
if not defined NO_PAUSE pause
exit /b %SCRIPT_EXIT_CODE%
