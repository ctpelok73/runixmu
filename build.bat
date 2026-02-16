@echo off
setlocal enabledelayedexpansion

set "ROOT=%~dp0"
set "SLN=%ROOT%Main5.2\Main.sln"
set "PLATFORM=x86"

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

set "CONFIG=%~1"
if "%CONFIG%"=="" set "CONFIG=All"

echo Using MSBuild: "%MSBUILD_EXE%"
echo Building: "%SLN%"
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
"%MSBUILD_EXE%" "%SLN%" /m /t:Build /p:Configuration=%CFG% /p:Platform=%PLATFORM% /v:m /clp:"WarningsOnly;Summary"
exit /b %errorlevel%
