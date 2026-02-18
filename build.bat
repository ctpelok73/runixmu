@echo off
setlocal enabledelayedexpansion

set "ROOT=%~dp0"
set "SLN=%ROOT%Main5.2\Main.sln"
set "DEFAULT_PLATFORM=x86"

if not exist "%SLN%" (
  echo Solution not found: "%SLN%"
  exit /b 1
)

set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
set "MSBUILD_EXE="

if exist "%VSWHERE%" (
  for /f "usebackq delims=" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe`) do (
    set "MSBUILD_EXE=%%i"
    goto :msbuild_found
  )
)

for %%i in (
  "%ProgramFiles%\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe"
  "%ProgramFiles%\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"
  "%ProgramFiles(x86)%\Microsoft Visual Studio\2019\BuildTools\MSBuild\Current\Bin\MSBuild.exe"
  "%ProgramFiles(x86)%\Microsoft Visual Studio\2019\Community\MSBuild\Current\Bin\MSBuild.exe"
) do (
  if exist %%~i (
    set "MSBUILD_EXE=%%~i"
    goto :msbuild_found
  )
)

:msbuild_found
if "%MSBUILD_EXE%"=="" (
  echo MSBuild.exe not found. Install Visual Studio Build Tools or Visual Studio.
  exit /b 1
)

set "ARG1=%~1"
set "ARG2=%~2"
set "ARG3=%~3"

if /i "%ARG1%"=="/?" goto :usage
if /i "%ARG1%"=="-h" goto :usage
if /i "%ARG1%"=="--help" goto :usage

if "%ARG1%"=="" goto :menu

set "CONFIG=%ARG1%"
set "ACTION=%ARG2%"
set "PLATFORM=%ARG3%"

if /i "%CONFIG%"=="Build" set "ACTION=Build" & set "CONFIG=All"
if /i "%CONFIG%"=="Rebuild" set "ACTION=Rebuild" & set "CONFIG=All"
if /i "%CONFIG%"=="Clean" set "ACTION=Clean" & set "CONFIG=All"

if "%CONFIG%"=="" set "CONFIG=All"
if "%ACTION%"=="" set "ACTION=Build"
if "%PLATFORM%"=="" set "PLATFORM=%DEFAULT_PLATFORM%"
goto :after_menu

:menu
echo.
echo Выберите конфигурацию:
set "SEL="
set /p SEL=1) Release 2) Debug 3) All: 
if "%SEL%"=="1" set "CONFIG=Release"
if "%SEL%"=="2" set "CONFIG=Debug"
if "%SEL%"=="3" set "CONFIG=All"
echo.
echo Выберите действие:
set "SEL="
set /p SEL=1) Build 2) Rebuild 3) Clean: 
if "%SEL%"=="1" set "ACTION=Build"
if "%SEL%"=="2" set "ACTION=Rebuild"
if "%SEL%"=="3" set "ACTION=Clean"
echo.
echo Выберите платформу:
set "SEL="
set /p SEL=1) x86 2) Win32: 
if "%SEL%"=="1" set "PLATFORM=x86"
if "%SEL%"=="2" set "PLATFORM=Win32"
if "%CONFIG%"=="" set "CONFIG=All"
if "%ACTION%"=="" set "ACTION=Build"
if "%PLATFORM%"=="" set "PLATFORM=%DEFAULT_PLATFORM%"

:after_menu

echo Using MSBuild: "%MSBUILD_EXE%"
echo Building: "%SLN%"
echo Action: %ACTION%
echo Platform: %PLATFORM%

if /i "%CONFIG%"=="All" (
  call :build_config Release || exit /b !errorlevel!
  call :build_config Debug || exit /b !errorlevel!
  exit /b 0
)

call :build_config %CONFIG%
exit /b %errorlevel%

:build_config
set "CFG=%~1"
echo.
echo ===== %CFG%-%PLATFORM% =====
"%MSBUILD_EXE%" "%SLN%" /m /t:%ACTION% /p:Configuration=%CFG% /p:Platform=%PLATFORM% /v:m /clp:"WarningsOnly;Summary"
exit /b %errorlevel%

:usage
echo Usage:
echo   build.bat [Config] [Action] [Platform]
echo.
echo   Config:   Release^|Debug^|All
echo   Action:   Build^|Rebuild^|Clean
echo   Platform: x86^|Win32
echo.
echo Examples:
echo   build.bat
echo   build.bat Release
echo   build.bat Rebuild
echo   build.bat Release Rebuild
echo   build.bat Debug Clean Win32
exit /b 0
