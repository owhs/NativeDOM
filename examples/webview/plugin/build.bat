@echo off
setlocal

echo [INFO] Creating output directory...
if not exist "bin" mkdir "bin"

if not exist "sdk\build\native\x64\WebView2Loader.dll" (
    echo [INFO] Downloading Microsoft.Web.WebView2 NuGet package...
    powershell -Command "Invoke-WebRequest -Uri 'https://www.nuget.org/api/v2/package/Microsoft.Web.WebView2/1.0.2420.47' -OutFile 'webview2.zip'"
    if not exist "webview2.zip" (
        echo [ERROR] Failed to download WebView2 SDK.
        exit /b 1
    )

    echo [INFO] Extracting SDK...
    powershell -Command "Expand-Archive -Path 'webview2.zip' -DestinationPath 'sdk' -Force"
    del webview2.zip
) else (
    echo [INFO] WebView2 SDK already downloaded. Skipping network fetch!
)

echo [INFO] Copying WebView2Loader.dll to output folder...
copy /Y "sdk\build\native\x64\WebView2Loader.dll" "bin\WebView2Loader.dll"

echo [INFO] Locating MSVC build environment...
set VSWHERE="%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq tokens=*" %%i in (`%VSWHERE% -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
    set VSDIR=%%i
)

if "%VSDIR%"=="" (
    echo [ERROR] MSVC not found! Please open a Developer Command Prompt.
    exit /b 1
)

echo [INFO] Loading MSVC...
call "%VSDIR%\VC\Auxiliary\Build\vcvarsall.bat" x64 >nul

echo [INFO] Compiling WebView2 Plugin...
cl.exe /nologo /O2 /LD /MD /EHsc webview_plugin.cpp /I"sdk\build\native\include" /link /OUT:bin\webview_plugin.dll "sdk\build\native\x64\WebView2Loader.dll.lib" User32.lib Advapi32.lib Ole32.lib

if "%ERRORLEVEL%"=="0" (
    echo [SUCCESS] Plugin built successfully at bin\webview_plugin.dll
    echo [INFO] Deploying DLLs to NativeDOM root for simple linking...
    copy /Y "bin\webview_plugin.dll" "..\..\..\webview_plugin.dll"
    copy /Y "bin\WebView2Loader.dll" "..\..\..\WebView2Loader.dll"
    echo [SUCCESS] Deployed!
) else (
    echo [ERROR] Build failed!
)
