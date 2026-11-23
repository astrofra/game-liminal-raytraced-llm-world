@echo off
setlocal

set "ROOT_DIR=%~dp0"
if "%ROOT_DIR:~-1%"=="\" set "ROOT_DIR=%ROOT_DIR:~0,-1%"
set "BUILD_DIR=%ROOT_DIR%\build"
set "EXE_PATH=%BUILD_DIR%\Release\liminal_cornell_renderer.exe"

if not defined OLLAMA_API_KEY (
    echo.
    echo === Ollama Cloud: OLLAMA_API_KEY is not defined ===
    echo This launcher runs the game with Ollama Cloud; default model: DeepSeek V4.1 Flash.
    echo In that mode, game prompts and relevant context are sent online.
    echo Define OLLAMA_API_KEY in the environment of this process before
    echo starting the script, then run it again. The key value is read from
    echo the environment only; it is never displayed by this script.
    echo.
    if not defined NO_PAUSE pause
    exit /b 1
)

if not exist "%EXE_PATH%" (
    echo.
    echo === Build Fresh Release ===
    set "NO_PAUSE=1"
    call "%ROOT_DIR%\build_release.bat"
    if errorlevel 1 goto :end
)

echo.
echo === Within the Latent Walls / Entre les Murs Latents ===
echo Mode: Ollama Cloud - default DeepSeek V4.1 Flash, overridable with --cloud-model.
echo Prompts and context are sent online.
echo EXE: "%EXE_PATH%"
echo Nouvelle partie. Archives : "%LOCALAPPDATA%\WithinTheLatentWalls\research"
echo Depart : seuil de la carriere. Nord, Est ou Sud pour commencer, puis suivre les issues indiquees.
echo.
pushd "%ROOT_DIR%"
"%EXE_PATH%" --sdl %* --llm-backend ollama-cloud
set "GAME_EXIT_CODE=%errorlevel%"
popd
if not "%GAME_EXIT_CODE%"=="0" exit /b %GAME_EXIT_CODE%

:end
set "SCRIPT_EXIT_CODE=%errorlevel%"
echo.
if not defined NO_PAUSE pause
exit /b %SCRIPT_EXIT_CODE%
