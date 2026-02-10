@echo off
setlocal enabledelayedexpansion

set "ROOT=%~dp0"
set "SLN=%ROOT%Main5.2\Main.sln"
set "CONFIG=%~1"
if "%CONFIG%"=="" set "CONFIG=Release"

set "PLATFORM=%~2"
if "%PLATFORM%"=="" set "PLATFORM=x86"

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
  "C:\Program Files\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe"
  "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe"
  "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\MSBuild\Current\Bin\MSBuild.exe"
  "C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\MSBuild\Current\Bin\MSBuild.exe"
) do (
  if exist %%i (
    set "MSBUILD_EXE=%%~i"
    goto :msbuild_found
  )
)

:msbuild_found
if "%MSBUILD_EXE%"=="" (
  echo MSBuild.exe not found. Install Visual Studio Build Tools or Visual Studio.
  exit /b 1
)

echo Using MSBuild: "%MSBUILD_EXE%"
echo Building: "%SLN%"
echo Configuration: %CONFIG%
echo Platform: %PLATFORM%

"%MSBUILD_EXE%" "%SLN%" /m /t:Build /p:Configuration=%CONFIG%;Platform=%PLATFORM% /v:minimal
exit /b %errorlevel%
