@echo off

setlocal EnableDelayedExpansion

:: ============================================================================
:: DOM Engine Build Script
:: ============================================================================

set COMPILER_FLAGS=/nologo /EHs-c- /std:c++17 /wd4530
set LINKER_FLAGS=/link User32.lib Gdi32.lib opengl32.lib Shell32.lib
set ENTRY_FILE=dom-engine/main.cpp
set OUT_EXE=dom.exe

taskkill /f /im %OUT_EXE% >nul 2>&1
REM del %OUT_EXE% >nul 2>&1

set "MIN_BUILD=0"
set "DEBUG_BUILD=0"
:arg_loop
if "%~1"=="" goto after_args
if /I "%~1"=="min" (
    set "MIN_BUILD=1"
) else if /I "%~1"=="debug" (
    set "DEBUG_BUILD=1"
)
shift
goto arg_loop
:after_args

if "!MIN_BUILD!"=="1" (
    echo [INFO] Minimized release build
    set COMPILER_FLAGS=!COMPILER_FLAGS! /O1 /Os /GL /MD /GS- /Gw /Gy /Zc:inline /GR-
    set LINKER_FLAGS=/link /LTCG /OPT:REF /OPT:ICF /MERGE:.rdata=.text /MERGE:.pdata=.text /FILEALIGN:512 /SUBSYSTEM:WINDOWS User32.lib Gdi32.lib opengl32.lib Shell32.lib
) else if "!DEBUG_BUILD!"=="1" (
    echo [INFO] Debug build with console
    set COMPILER_FLAGS=!COMPILER_FLAGS! /Od /Zi /MDd
) else (
    echo [INFO] Standard development build
    set COMPILER_FLAGS=!COMPILER_FLAGS! /O2 /MD
)

echo.

:: Check if cl.exe is already available
where cl.exe >nul 2>&1
if !ERRORLEVEL! EQU 0 goto :build_start

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

echo Compiling: cl.exe !ENTRY_FILE! !COMPILER_FLAGS! /Fe:!OUT_EXE! !LINKER_FLAGS!
echo.
cl.exe !ENTRY_FILE! !COMPILER_FLAGS! /Fe:!OUT_EXE! !LINKER_FLAGS!

if !ERRORLEVEL! NEQ 0 (
    echo.
    echo [ERROR] Build failed.
    pause
    exit /b !ERRORLEVEL!
)

:: Cleanup
if exist main.obj del main.obj
del *.lib >nul 2>&1
del *.exp >nul 2>&1

for %%A in (!OUT_EXE!) do (
    set /a "KB=%%~zA / 1024"
    echo.
    echo [SUCCESS] !OUT_EXE! = !KB! KB
)

REM pause
endlocal
