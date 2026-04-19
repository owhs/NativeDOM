#include <windows.h>
#include <wrl.h>
#include "WebView2.h"
#include <string>
#include <stdio.h>
#include <stdarg.h>

using namespace Microsoft::WRL;

HWND g_hwnd = NULL;
ICoreWebView2Controller* g_webviewController = nullptr;
ICoreWebView2* g_webview = nullptr;

void writeLog(const char* format, ...) {
    FILE* f = fopen("sys_log.txt", "a");
    if (f) {
        va_list args;
        va_start(args, format);
        fprintf(f, "[WebView2 Plugin] ");
        vfprintf(f, format, args);
        fprintf(f, "\n");
        va_end(args);
        fclose(f);
    }
}

extern "C" __declspec(dllexport) void __stdcall NativeDOM_Init(void** ctxArgs) {
    g_hwnd = (HWND)ctxArgs[0];
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
}

std::wstring g_pendingNavigate = L"";

extern "C" __declspec(dllexport) uint64_t __stdcall ExecuteCommand(
    uint64_t cmd, uint64_t p1, uint64_t p2, uint64_t p3,
    uint64_t p4, uint64_t p5, uint64_t p6, uint64_t p7) 
{
    if (cmd == 0) return 1;

    // COMMAND 1: Initialize WebView2 Controller
    if (cmd == 1) {
        if (!g_hwnd) {
            writeLog("Cmd 1 Failed: g_hwnd is NULL");
            return 0;
        }
        writeLog("Initializing WebView2 on HWND: %p", g_hwnd);
        HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(nullptr, nullptr, nullptr,
            Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
                [](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {
                    if (FAILED(result) || !env) {
                        writeLog("Env creation failed! HRESULT: 0x%08X", (unsigned int)result);
                        return S_OK;
                    }
                    writeLog("Environment created successfully.");
                    
                    env->CreateCoreWebView2Controller(g_hwnd, 
                        Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                            [](HRESULT result, ICoreWebView2Controller* controller) -> HRESULT {
                                if (FAILED(result) || !controller) {
                                    writeLog("Controller creation failed! HRESULT: 0x%08X", (unsigned int)result);
                                    return S_OK;
                                }
                                writeLog("Controller created successfully.");
                                g_webviewController = controller;
                                g_webviewController->AddRef(); // MUST hold reference so COM object stays alive
                                
                                HRESULT cv = g_webviewController->get_CoreWebView2(&g_webview);
                                if (FAILED(cv)) {
                                    writeLog("Failed to get CoreWebView2! HRESULT: 0x%08X", (unsigned int)cv);
                                } else {
                                    writeLog("get_CoreWebView2 succeeded.");
                                    if (g_pendingNavigate.length() > 0) {
                                        writeLog("Dispatching cached navigation: %ws", g_pendingNavigate.c_str());
                                        g_webview->Navigate(g_pendingNavigate.c_str());
                                        g_pendingNavigate = L"";
                                        
                                        // Force UI visible
                                        g_webviewController->put_IsVisible(TRUE);
                                    }
                                }
                                
                                // Send async ready message to JS space using NativeDOM's custom message pipe
                                // lParam 999 denotes WebViewReady
                                PostMessageA(g_hwnd, WM_APP + 2, 0, 999);
                                return S_OK;
                            }).Get());
                    return S_OK;
                }).Get());
        
        if (FAILED(hr)) {
            writeLog("CreateCoreWebView2EnvironmentWithOptions Failed sync: 0x%08X", (unsigned int)hr);
        }
        return (hr == S_OK) ? 1 : 0;
    }
    
    // COMMAND 2: Navigate to URL
    if (cmd == 2) {
        const char* urlStr = (const char*)p1;
        if (!urlStr) {
            writeLog("Cmd 2 Failed: URL pointer is NULL!");
            return 0;
        }
        
        int len = MultiByteToWideChar(CP_UTF8, 0, urlStr, -1, NULL, 0);
        std::wstring wUrl(len, 0);
        MultiByteToWideChar(CP_UTF8, 0, urlStr, -1, &wUrl[0], len);
        if (len > 0) wUrl.resize(len - 1);
        
        if (!g_webview) {
            writeLog("Cmd 2 Cached! g_webview is NULL, buffering navigation...");
            g_pendingNavigate = wUrl;
            return 1;
        }
        
        writeLog("Navigating to: %s", urlStr);
        HRESULT hr = g_webview->Navigate(wUrl.c_str());
        if (FAILED(hr)) {
            writeLog("g_webview->Navigate FAILED with HRESULT: 0x%08X", (unsigned int)hr);
            return 0;
        }
        writeLog("Navigate call dispatched!");
        return 1;
    }

    // COMMAND 3: Update Geometry/Bounds
    if (cmd == 3) {
        if (g_webviewController) {
            RECT bounds;
            bounds.left = (LONG)p1;
            bounds.top = (LONG)p2;
            bounds.right = (LONG)p1 + (LONG)p3;
            bounds.bottom = (LONG)p2 + (LONG)p4;
            g_webviewController->put_Bounds(bounds);
            return 1;
        }
        return 0;
    }

    // COMMAND 4: Execute Javascript inside the Webview
    if (cmd == 4) {
        if (g_webview) {
            const char* scriptStr = (const char*)p1;
            if (!scriptStr) return 0;
            int len = MultiByteToWideChar(CP_UTF8, 0, scriptStr, -1, NULL, 0);
            std::wstring wScript(len, 0);
            MultiByteToWideChar(CP_UTF8, 0, scriptStr, -1, &wScript[0], len);

            g_webview->ExecuteScript(wScript.c_str(), nullptr);
            return 1;
        }
        return 0;
    }

    // COMMAND 5: Show or Hide the Webview
    if (cmd == 5) {
        if (g_webviewController) {
            g_webviewController->put_IsVisible(p1 == 1);
            return 1;
        }
        return 0;
    }

    return 0;
}
