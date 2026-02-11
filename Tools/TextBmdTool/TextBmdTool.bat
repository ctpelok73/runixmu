@echo off
setlocal EnableExtensions EnableDelayedExpansion

set "TOOL_DIR=%~dp0"
set "TOOL_EXE=%TOOL_DIR%TextBmdTool.exe"
set "TOOL_EXE_BUILD=%TOOL_DIR%TextBmdTool.build.exe"
set "NO_PAUSE="
if /I "%~1"=="--nopause" (
  set "NO_PAUSE=1"
  shift
)
if defined TEXTBMD_NO_PAUSE set "NO_PAUSE=1"

if /I "%~1"=="" goto :help
if /I "%~1"=="help" goto :help

if /I "%~1"=="build" goto :do_build

if not exist "%TOOL_EXE%" (
  echo [ERROR] "%TOOL_EXE%" not found
  echo         Build/copy TextBmdTool.exe into this folder first.
  echo.
  if not defined NO_PAUSE pause
  exit /b 1
)

set "CMD=%~1"

if /I "%CMD%"=="export" goto :do_export
if /I "%CMD%"=="import" goto :do_import

echo [ERROR] Unknown command: %CMD%
echo.
goto :help

:do_build
pushd "%TOOL_DIR%" >nul

if not exist "TextBmdTool.cpp" (
  echo [ERROR] "%TOOL_DIR%TextBmdTool.cpp" not found
  echo.
  popd >nul
  pause
  exit /b 1
)

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" (
  set "VSWHERE=%ProgramFiles%\Microsoft Visual Studio\Installer\vswhere.exe"
)

set "VSINSTALL="
if exist "%VSWHERE%" (
  for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VSINSTALL=%%i"
)

if "%VSINSTALL%"=="" (
  set "CAND1=%ProgramFiles%\Microsoft Visual Studio\2022\Community"
  set "CAND2=%ProgramFiles(x86)%\Microsoft Visual Studio\2022\Community"
  set "CAND3=Y:\Program Files\Microsoft Visual Studio\2022\Community"
  set "CAND4=Y:\Program Files (x86)\Microsoft Visual Studio\2022\Community"
  if exist "%CAND1%\Common7\Tools\VsDevCmd.bat" set "VSINSTALL=%CAND1%"
  if exist "%CAND2%\Common7\Tools\VsDevCmd.bat" set "VSINSTALL=%CAND2%"
  if exist "%CAND3%\Common7\Tools\VsDevCmd.bat" set "VSINSTALL=%CAND3%"
  if exist "%CAND4%\Common7\Tools\VsDevCmd.bat" set "VSINSTALL=%CAND4%"
)

if "%VSINSTALL%"=="" (
  echo [ERROR] Visual Studio C++ Build Tools not found.
  echo         If VS is installed on another drive, ensure VsDevCmd.bat exists under:
  echo           ^<VS^>\Common7\Tools\VsDevCmd.bat
  echo         Install "Desktop development with C++" if missing.
  echo.
  popd >nul
  pause
  exit /b 1
)

set "VSDEVCMD=%VSINSTALL%\Common7\Tools\VsDevCmd.bat"
if not exist "%VSDEVCMD%" (
  echo [ERROR] VsDevCmd.bat not found: "%VSDEVCMD%"
  echo.
  popd >nul
  pause
  exit /b 1
)

call "%VSDEVCMD%" -no_logo -arch=amd64 -host_arch=amd64 >nul
where cl.exe >nul 2>&1
if errorlevel 1 (
  echo [ERROR] cl.exe not found after VsDevCmd.
  echo.
  popd >nul
  pause
  exit /b 1
)

echo [INFO] Building TextBmdTool.exe ...
cl /nologo /EHsc /std:c++17 /O2 /MT TextBmdTool.cpp /FeTextBmdTool.build.exe /link /SUBSYSTEM:WINDOWS
if errorlevel 1 goto :build_fail

copy /y "TextBmdTool.build.exe" "TextBmdTool.exe" >nul 2>&1
echo.
if exist "%TOOL_EXE%" (
  echo [OK] Built: "%TOOL_EXE%"
) else (
  echo [OK] Built: "%TOOL_EXE_BUILD%"
  echo [WARN] TextBmdTool.exe is probably running (file is locked).
  echo        Close the program and run build again to replace it.
)
echo.
popd >nul
if not defined NO_PAUSE pause
exit /b 0

:build_fail
echo.
echo [ERROR] Build failed.
echo.
popd >nul
if not defined NO_PAUSE pause
exit /b 1

:do_export
set "DATA_DIR=%~2"
set "LANG=%~3"
set "CP=%~4"

if "%DATA_DIR%"=="" set "DATA_DIR=Data"
if "%LANG%"=="" set "LANG=Ru"
if "%CP%"=="" set "CP=1251"

set "OUT_DIR=%TOOL_DIR%out"
if not exist "%OUT_DIR%" mkdir "%OUT_DIR%" >nul 2>&1

set "IN_TEXT=%DATA_DIR%\Local\%LANG%\Text.bmd"
set "IN_GLOBAL=%DATA_DIR%\Local\%LANG%\GlobalText.bmd"

echo [INFO] Export:
echo        "%IN_TEXT%"  ^> "%OUT_DIR%\Text.tsv"
echo        "%IN_GLOBAL%" ^> "%OUT_DIR%\GlobalText.tsv"
echo        codepage=%CP%
echo.

"%TOOL_EXE%" export "%IN_TEXT%" "%OUT_DIR%\Text.tsv" --cp %CP%
if errorlevel 1 goto :fail

"%TOOL_EXE%" export "%IN_GLOBAL%" "%OUT_DIR%\GlobalText.tsv" --cp %CP%
if errorlevel 1 goto :fail

echo.
echo [OK] Done: "%OUT_DIR%\Text.tsv", "%OUT_DIR%\GlobalText.tsv"
exit /b 0

:do_import
set "DATA_DIR=%~2"
set "LANG=%~3"
set "CP=%~4"

if "%DATA_DIR%"=="" set "DATA_DIR=Data"
if "%LANG%"=="" set "LANG=Ru"
if "%CP%"=="" set "CP=1251"

set "OUT_DIR=%TOOL_DIR%out"
if not exist "%OUT_DIR%" mkdir "%OUT_DIR%" >nul 2>&1

set "BASE_TEXT=%DATA_DIR%\Local\%LANG%\Text.bmd"
set "BASE_GLOBAL=%DATA_DIR%\Local\%LANG%\GlobalText.bmd"
set "IN_TEXT_TSV=%OUT_DIR%\Text.tsv"
set "IN_GLOBAL_TSV=%OUT_DIR%\GlobalText.tsv"

if not exist "%IN_TEXT_TSV%" (
  echo [ERROR] "%IN_TEXT_TSV%" not found
  echo         Run export first.
  echo.
  if not defined NO_PAUSE pause
  exit /b 1
)
if not exist "%IN_GLOBAL_TSV%" (
  echo [ERROR] "%IN_GLOBAL_TSV%" not found
  echo         Run export first.
  echo.
  if not defined NO_PAUSE pause
  exit /b 1
)

echo [INFO] Import (merge into base BMD):
echo        "%IN_TEXT_TSV%" ^+ "%BASE_TEXT%" ^> "%OUT_DIR%\Text.new.bmd"
echo        "%IN_GLOBAL_TSV%" ^+ "%BASE_GLOBAL%" ^> "%OUT_DIR%\GlobalText.new.bmd"
echo        codepage=%CP%
echo.

"%TOOL_EXE%" import "%IN_TEXT_TSV%" "%OUT_DIR%\Text.new.bmd" --base "%BASE_TEXT%" --cp %CP%
if errorlevel 1 goto :fail

"%TOOL_EXE%" import "%IN_GLOBAL_TSV%" "%OUT_DIR%\GlobalText.new.bmd" --base "%BASE_GLOBAL%" --cp %CP%
if errorlevel 1 goto :fail

echo.
echo [OK] Done: "%OUT_DIR%\Text.new.bmd", "%OUT_DIR%\GlobalText.new.bmd"
exit /b 0

:fail
echo.
echo [ERROR] Command failed.
echo.
if not defined NO_PAUSE pause
exit /b 1

:help
echo TextBmdTool.bat build
echo TextBmdTool.bat export  [DataDir] [Lang] [CodePage]
echo TextBmdTool.bat import  [DataDir] [Lang] [CodePage]
echo.
echo Examples:
echo   TextBmdTool.bat --nopause build
echo   TextBmdTool.bat --nopause export "D:\MU\Client\Data" Ru 1251
echo   TextBmdTool.bat --nopause import "D:\MU\Client\Data" Ru 1251
echo   TextBmdTool.bat build
echo   TextBmdTool.bat export "D:\MU\Client\Data" Ru 1251
echo   TextBmdTool.bat import "D:\MU\Client\Data" Ru 1251
echo.
echo Defaults:
echo   DataDir = Data
echo   Lang    = Ru
echo   CP      = 1251
echo.
echo Output folder:
echo   "%~dp0out\"
echo.
if not defined NO_PAUSE pause
exit /b 0
