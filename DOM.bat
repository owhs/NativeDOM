@echo off
setlocal EnableDelayedExpansion

:: ============================================================================
:: DOM Engine - Master Build GUI
:: ============================================================================

:: Setup ANSI Escapes
for /F "tokens=1,2 delims=#" %%a in ('"prompt #$H#$E# & echo on & for %%b in (1) do rem"') do set "ESC=%%b"
set "RED=%ESC%[91m"
set "GREEN=%ESC%[92m"
set "YELLOW=%ESC%[93m"
set "BLUE=%ESC%[94m"
set "MAGENTA=%ESC%[95m"
set "CYAN=%ESC%[96m"
set "WHITE=%ESC%[97m"
set "RESET=%ESC%[0m"
set "BOLD=%ESC%[1m"
set "DIM=%ESC%[2m"

title DOM Dashboard

:MainMenu
cls
echo %CYAN%%BOLD%==========================================================================%RESET%
echo %CYAN%%BOLD%                    DOM ENGINE TOOLCHAIN MASTER                           %RESET%
echo %CYAN%%BOLD%==========================================================================%RESET%
echo.
echo  %WHITE%%BOLD%What would you like to do?%RESET%
echo.
echo  %MAGENTA%[1]%RESET% %WHITE%Build Base Engine%RESET%      %DIM%Recompile 'dom.exe' core renderer%RESET%
echo  %MAGENTA%[2]%RESET% %WHITE%Bundle Application%RESET%     %DIM%Compile a .dom interface into a .exe%RESET%
echo  %MAGENTA%[3]%RESET% %WHITE%Clean Workspace%RESET%        %DIM%Remove intermediate build artifacts%RESET%
echo  %MAGENTA%[4]%RESET% %WHITE%Exit%RESET%                   
echo.
set "CHOICE="
set /p "CHOICE=%CYAN%Select option:%RESET% "

if "%CHOICE%"=="1" goto EngineBuild
if "%CHOICE%"=="2" goto AppBundler
if "%CHOICE%"=="3" goto Clean
if "%CHOICE%"=="4" exit /b 0
goto MainMenu

:EngineBuild
cls
echo %CYAN%%BOLD%==========================================================================%RESET%
echo %CYAN%%BOLD%                           ENGINE BUILDER                                 %RESET%
echo %CYAN%%BOLD%==========================================================================%RESET%
echo.
echo %MAGENTA%[1]%RESET% %WHITE%Optimized Release%RESET%  %DIM%(Fast, ~600KB, Minimized)%RESET%
echo %MAGENTA%[2]%RESET% %WHITE%Standard Dev%RESET%       %DIM%(Defaults)%RESET%
echo %MAGENTA%[3]%RESET% %WHITE%Debug%RESET%              %DIM%(Verbose logging + debug DB)%RESET%
echo %MAGENTA%[4]%RESET% %WHITE%Back%RESET%
echo.
set "OPT="
set /p "OPT=%CYAN%Select build type:%RESET% "
if "%OPT%"=="4" goto :MainMenu

set "BUILD_ARGS="
if "%OPT%"=="1" set "BUILD_ARGS=min"
if "%OPT%"=="3" set "BUILD_ARGS=debug"

echo.
echo %GREEN%Launching compile.bat !BUILD_ARGS!%RESET%
call compile.bat !BUILD_ARGS!
pause
goto :MainMenu

:AppBundler
cls
echo %CYAN%%BOLD%==========================================================================%RESET%
echo %CYAN%%BOLD%                       APPLICATION BUNDLER                                %RESET%
echo %CYAN%%BOLD%==========================================================================%RESET%
echo.
set "TARGET=examples\main\app.dom"
set /p "TARGET=%YELLOW%Enter target .dom path (drag/drop) [%TARGET%]: %RESET%"
:: Trim quotes if dragged
if "!TARGET:~0,1!"=="^"" set "TARGET=!TARGET:~1,-1!"
echo.

echo %CYAN%Select Architecture:%RESET%
echo  %MAGENTA%[1]%RESET% %WHITE%Full Independence%RESET%   %DIM%(Standalone EXE, embeds engine + C++)%RESET%
echo  %MAGENTA%[2]%RESET% %WHITE%Lightweight Stub%RESET%    %DIM%(Tiny EXE, dynamically loads dom.exe)%RESET%
set "ARCH="
set /p "ARCH=%YELLOW%Architecture:%RESET% "

set "USE_AOT=0"
if "%ARCH%"=="1" (
    echo.
    echo %CYAN%Ahead-of-Time ^(AOT^) Transpilation%RESET%
    echo %DIM%Converts .dom XML directly into native C++ rendering macros!%RESET%
    echo %DIM%Dramatically speeds up boot time and cuts runtime bloat.%RESET%
    set /p "AOT_OPT=%YELLOW%Use AOT transpilation? [Y/N]: %RESET%"
    if /I "!AOT_OPT!"=="Y" set "USE_AOT=1"
)

set "USE_LZNT1=0"
set "USE_RC4="
if "%USE_AOT%"=="0" (
    echo.
    echo %CYAN%Apply LZNT1 Payload Compression?%RESET%
    echo %DIM%Makes executable smaller, but unzips text at runtime.%RESET%
    set /p "Z_OPT=%YELLOW%Compress? [Y/N]: %RESET%"
    if /I "!Z_OPT!"=="Y" set "USE_LZNT1=1"
    
    echo.
    echo %CYAN%Apply RC4 Payload Encryption?%RESET%
    echo %DIM%Secures the embedded UI from static analysis.%RESET%
    set /p "USE_RC4=%YELLOW%Encryption Key (leave empty to skip): %RESET%"
)

:: Compile arguments
set "B_ARGS=!TARGET!"
if "%ARCH%"=="1" set "B_ARGS=!B_ARGS! -full"
if "%USE_AOT%"=="1" set "B_ARGS=!B_ARGS! -aot"
if "%USE_LZNT1%"=="1" set "B_ARGS=!B_ARGS! -lznt1"
if not "!USE_RC4!"=="" set B_ARGS=!B_ARGS! -rc4="!USE_RC4!"

echo.
echo %GREEN%Executing configuration:%RESET% %WHITE%bundle.bat !B_ARGS!%RESET%
echo.
call bundle.bat !B_ARGS!
pause
goto :MainMenu

:Clean
cls
echo %CYAN%%BOLD%Cleaning Workspace...%RESET%
del /q *.obj *.lib *.exp >nul 2>&1
if exist "dom-engine\bundled_app.h" del "dom-engine\bundled_app.h"
echo %GREEN%Done.%RESET%
timeout /t 2 >nul
goto :MainMenu
