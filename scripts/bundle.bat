@echo off

setlocal EnableDelayedExpansion

:: ============================================================================
:: DOM Application Bundler
:: ============================================================================

set "BUNDLE_TARGET="
set "BUNDLE_TYPE=min"
set "BUNDLE_ARGS="

:arg_loop
if "%~1"=="" goto after_args
if /I "%~1"=="-full" (
    set "BUNDLE_TYPE=full"
) else if /I "%~1"=="/full" (
    set "BUNDLE_TYPE=full"
) else if "!BUNDLE_TARGET!"=="" (
    set "BUNDLE_TARGET=%~1"
) else (
    set "BUNDLE_ARGS=!BUNDLE_ARGS! %1"
)
shift
goto arg_loop
:after_args

if "!BUNDLE_TARGET!"=="" set "BUNDLE_TARGET=app.dom"

for %%F in ("!BUNDLE_TARGET!") do set "BUNDLE_NAME=%%~nF"

if "!BUNDLE_TYPE!"=="full" (
    set "OUT_EXE=dom_full_bundle-!BUNDLE_NAME!.exe"
    set "ENTRY_FILE=dom-engine/main.cpp"
    set "COMPILER_FLAGS=/nologo /EHs-c- /std:c++17 /wd4530 /O1 /Os /GL /MD /GS- /Gw /Gy /Zc:inline /GR- /DCOMPILED_DOM_PROJECT"
    set "LINKER_FLAGS=/link /LTCG /OPT:REF /OPT:ICF /MERGE:.rdata=.text /MERGE:.pdata=.text /FILEALIGN:512 /SUBSYSTEM:WINDOWS User32.lib Gdi32.lib opengl32.lib Shell32.lib ole32.lib oleaut32.lib Advapi32.lib"
) else (
    set "OUT_EXE=dom_bundle-!BUNDLE_NAME!.exe"
    set "ENTRY_FILE=dom-engine/stub.cpp"
    set "COMPILER_FLAGS=/nologo /EHs-c- /O1 /Os /GL /MD /GS- /Gw /Gy /Zc:inline /DCOMPILED_DOM_PROJECT"
    set "LINKER_FLAGS=/link /LTCG /OPT:REF /OPT:ICF /MERGE:.rdata=.text /MERGE:.pdata=.text /FILEALIGN:512 /SUBSYSTEM:WINDOWS User32.lib kernel32.lib Shell32.lib"
)

echo [INFO] Locating MSVC build environment...

:: Try vswhere
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if exist "!VSWHERE!" (
    for /f "usebackq tokens=*" %%i in (`"!VSWHERE!" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
        set "VS_PATH=%%i"
    )
)

if defined VS_PATH (
    if exist "!VS_PATH!\VC\Auxiliary\Build\vcvarsall.bat" (
        echo [INFO] Loading MSVC vcvarsall
        call "!VS_PATH!\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul 2>&1
        goto :build_start
    )
    if exist "!VS_PATH!\Common7\Tools\VsDevCmd.bat" (
        echo [INFO] Loading Developer Command Prompt
        call "!VS_PATH!\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul 2>&1
        goto :build_start
    )
)

:: Hardcoded fallback
if exist "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" (
    echo [INFO] Loading VS2022 Build Tools
    call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul 2>&1
    goto :build_start
)

echo [ERROR] MSVC not found. Run from Developer Command Prompt.
pause
exit /b 1

:build_start
echo [INFO] Generating DOM Bundle for !BUNDLE_TARGET!...
if not exist dom.exe (
    echo [ERROR] dom.exe not found. Please run compile.bat first to compile the engine.
    exit /b 1
)

dom.exe bundle "!BUNDLE_TARGET!" dom-engine/bundled_app.h !BUNDLE_ARGS!
if !ERRORLEVEL! NEQ 0 (
    echo [ERROR] DOM Bundler failed.
    exit /b !ERRORLEVEL!
)
echo.

echo Compiling: cl.exe !ENTRY_FILE! !COMPILER_FLAGS! /Fe:!OUT_EXE! !LINKER_FLAGS!
echo.
cl.exe !ENTRY_FILE! !COMPILER_FLAGS! /Fe:!OUT_EXE! !LINKER_FLAGS!

if !ERRORLEVEL! NEQ 0 (
    echo.
    echo [ERROR] Bundling compile failed.
    pause
    exit /b !ERRORLEVEL!
)

:: Cleanup
if exist main.obj del main.obj
if exist stub.obj del stub.obj
if exist "dom-engine\bundled_app.h" del "dom-engine\bundled_app.h"
del *.lib >nul 2>&1
del *.exp >nul 2>&1

for %%A in (!OUT_EXE!) do (
    set /a "KB=%%~zA / 1024"
    echo.
    echo [SUCCESS] !OUT_EXE! = !KB! KB
)

endlocal
