@echo off
setlocal

set "ROOT_DIR=%~dp0"
if "%ROOT_DIR:~-1%"=="\" set "ROOT_DIR=%ROOT_DIR:~0,-1%"

set "LLAMA_CLI=%ROOT_DIR%\vendor\llama.cpp\build-cuda\bin\Release\llama-cli.exe"
set "MODEL_PATH=%ROOT_DIR%\models\ministral-3-8b\Ministral-3-8B-Instruct-2512-Q4_K_M.gguf"
set "USER_PROMPT="

if not exist "%LLAMA_CLI%" (
    echo llama-cli was not found:
    echo "%LLAMA_CLI%"
    echo Build it first with:
    echo   cmake --build vendor\llama.cpp\build-cuda --config Release --target llama-cli --parallel
    exit /b 1
)

if not exist "%MODEL_PATH%" (
    echo Model file was not found:
    echo "%MODEL_PATH%"
    echo Download it first with:
    echo   download_ministral.bat
    exit /b 1
)

if not "%~1"=="" goto :collect_args

echo.
set /P USER_PROMPT=Question: 
goto :prompt_done

:collect_args
if "%~1"=="" goto :prompt_done
if defined USER_PROMPT (
    set "USER_PROMPT=%USER_PROMPT% %~1"
) else (
    set "USER_PROMPT=%~1"
)
shift
goto :collect_args

:prompt_done

if not defined USER_PROMPT (
    echo No prompt provided.
    exit /b 1
)

echo.
echo === Ask Ministral ===
"%LLAMA_CLI%" ^
  --model "%MODEL_PATH%" ^
  --n-gpu-layers auto ^
  --ctx-size 4096 ^
  --flash-attn on ^
  --conversation ^
  --single-turn ^
  --no-mmproj ^
  --system-prompt "Tu es un assistant concis, utile, et tu reponds en francais." ^
  --prompt "%USER_PROMPT%" ^
  --n-predict 192 ^
  --temperature 0.7 ^
  --seed 42
if errorlevel 1 goto :end

:end
set "SCRIPT_EXIT_CODE=%errorlevel%"
echo.
if not defined NO_PAUSE pause
exit /b %SCRIPT_EXIT_CODE%
