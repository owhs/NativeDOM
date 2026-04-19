// ============================================================================
// WebView2 Plugin for NativeDOM — Full Control Bridge
// Provides comprehensive WebView2 embedding with navigation, JS execution,
// web messaging, resource filtering, content settings, and lifecycle control.
// ============================================================================
#include <windows.h>
#include <wrl.h>
#include <shlwapi.h>
#include <string>
#include <vector>
#include <cstdio>
#include <cstdarg>
#include "sdk/build/native/include/WebView2.h"

// ---- Globals ----
static HWND g_hwnd = NULL;
static ICoreWebView2Controller* g_webviewController = nullptr;
static ICoreWebView2* g_webview = nullptr;
static ICoreWebView2Settings* g_settings = nullptr;
static std::wstring g_pendingNavigate;

// String return buffers (for QueryString calls)
static char g_urlBuffer[4096] = {};
static char g_titleBuffer[4096] = {};
static char g_webMessageBuffer[65536] = {};
static bool g_hasWebMessage = false;

// ---- Logging ----
static void writeLog(const char* fmt, ...) {
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    FILE* f = fopen("sys_log.txt", "a");
    if (f) { fprintf(f, "[WebView2 Plugin] %s\r\n", buf); fclose(f); }
    OutputDebugStringA(buf);
}

// ---- NativeDOM Extension Init ----
extern "C" __declspec(dllexport) void __stdcall NativeDOM_Init(void** ctxArgs) {
    g_hwnd = (HWND)ctxArgs[0];
    writeLog("NativeDOM_Init received HWND: %p", g_hwnd);
}

// ---- Helpers ----
static std::wstring ToWide(const char* utf8) {
    if (!utf8) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, NULL, 0);
    std::wstring ws(len, 0);
    MultiByteToWideChar(CP_UTF8, 0, utf8, -1, &ws[0], len);
    if (len > 0) ws.resize(len - 1);
    return ws;
}

static std::string ToUTF8(const wchar_t* wide) {
    if (!wide) return "";
    int len = WideCharToMultiByte(CP_UTF8, 0, wide, -1, NULL, 0, NULL, NULL);
    std::string s(len, 0);
    WideCharToMultiByte(CP_UTF8, 0, wide, -1, &s[0], len, NULL, NULL);
    if (len > 0) s.resize(len - 1);
    return s;
}

// ---- Main Command Dispatcher ----
// This function exposes the entire WebView2 API surface to JS via sys.dllCall.
//
// COMMANDS:
//   1  = Initialize (create environment + controller)
//   2  = Navigate to URL               (p1=url_ptr)
//   3  = Update geometry/bounds        (p1=x, p2=y, p3=w, p4=h)
//   4  = Execute JavaScript            (p1=script_ptr)
//   5  = Set visibility                (p1=1/0)
//   6  = Reload
//   7  = Stop loading
//   8  = Go back
//   9  = Go forward
//  10  = Query URL (stores in buffer, use QueryString to read)
//  11  = Query title (stores in buffer, use QueryString to read)
//  12  = Set user agent                (p1=useragent_ptr)
//  13  = Post web message to page      (p1=message_ptr)
//  14  = Enable/disable setting        (p1=setting_id, p2=enable)
//        Setting IDs: 0=JavaScript, 1=WebMessage, 2=DefaultScriptDialogs, 
//                     3=StatusBar, 4=DevTools, 5=DefaultContextMenus, 6=Zoom
//  15  = Open DevTools
//  16  = Close/Destroy
//  17  = Set zoom factor               (p1=factor_x100, e.g. 150 = 1.5x)
//  18  = Print
//  19  = Check navigation state        (returns bitfield: bit0=canGoBack, bit1=canGoForward)
//  20  = Capture preview/screenshot    (p1=filepath_ptr, p2=format: 0=PNG,1=JPEG)
//  21  = Poll web message              (returns 1 if message available)
//  22  = Set background color          (p1=r, p2=g, p3=b, p4=a)
//  23  = Suspend
//  24  = Resume
//
extern "C" __declspec(dllexport) uint64_t __stdcall ExecuteCommand(
    uint64_t cmd, uint64_t p1, uint64_t p2, uint64_t p3,
    uint64_t p4, uint64_t p5, uint64_t p6, uint64_t p7)
{
    // COMMAND 1: Initialize WebView2
    if (cmd == 1) {
        if (g_webviewController) {
            writeLog("Already initialized, skipping re-init.");
            return 1;
        }
        
        HRESULT hr = CreateCoreWebView2EnvironmentWithOptions(
            nullptr, nullptr, nullptr,
            Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2EnvironmentCompletedHandler>(
                [](HRESULT result, ICoreWebView2Environment* env) -> HRESULT {
                    if (FAILED(result) || !env) {
                        writeLog("Environment creation failed: 0x%08X", (unsigned int)result);
                        return result;
                    }
                    writeLog("Environment created successfully.");
                    env->CreateCoreWebView2Controller(g_hwnd,
                        Microsoft::WRL::Callback<ICoreWebView2CreateCoreWebView2ControllerCompletedHandler>(
                            [](HRESULT result, ICoreWebView2Controller* controller) -> HRESULT {
                                if (FAILED(result) || !controller) {
                                    writeLog("Controller creation failed: 0x%08X", (unsigned int)result);
                                    return result;
                                }
                                writeLog("Controller created successfully.");
                                g_webviewController = controller;
                                g_webviewController->AddRef();
                                
                                HRESULT hr = g_webviewController->get_CoreWebView2(&g_webview);
                                if (SUCCEEDED(hr) && g_webview) {
                                    writeLog("get_CoreWebView2 succeeded.");

                                    // Get settings interface
                                    g_webview->get_Settings(&g_settings);
                                    
                                    // Enable web messaging by default
                                    if (g_settings) {
                                        g_settings->put_IsWebMessageEnabled(TRUE);
                                        g_settings->put_IsScriptEnabled(TRUE);
                                        g_settings->put_AreDevToolsEnabled(TRUE);
                                        g_settings->put_IsStatusBarEnabled(FALSE);
                                    }
                                    
                                    // Register web message received handler
                                    EventRegistrationToken token;
                                    g_webview->add_WebMessageReceived(
                                        Microsoft::WRL::Callback<ICoreWebView2WebMessageReceivedEventHandler>(
                                            [](ICoreWebView2* sender, ICoreWebView2WebMessageReceivedEventArgs* args) -> HRESULT {
                                                LPWSTR msg = nullptr;
                                                args->TryGetWebMessageAsString(&msg);
                                                if (msg) {
                                                    std::string utf8 = ToUTF8(msg);
                                                    strncpy(g_webMessageBuffer, utf8.c_str(), sizeof(g_webMessageBuffer) - 1);
                                                    g_hasWebMessage = true;
                                                    CoTaskMemFree(msg);
                                                    
                                                    // Notify JS via custom message (lParam=1000 = web message)
                                                    PostMessageA(g_hwnd, WM_APP + 2, 0, 1000);
                                                }
                                                return S_OK;
                                            }).Get(), &token);
                                    
                                    // Process any pending navigation
                                    if (!g_pendingNavigate.empty()) {
                                        g_webview->Navigate(g_pendingNavigate.c_str());
                                        g_pendingNavigate = L"";
                                        g_webviewController->put_IsVisible(TRUE);
                                    }
                                }
                                
                                // Signal ready to JS (lParam=999)
                                PostMessageA(g_hwnd, WM_APP + 2, 0, 999);
                                return S_OK;
                            }).Get());
                    return S_OK;
                }).Get());
        
        if (FAILED(hr)) {
            writeLog("CreateCoreWebView2EnvironmentWithOptions failed: 0x%08X", (unsigned int)hr);
        }
        return (hr == S_OK) ? 1 : 0;
    }
    
    // COMMAND 2: Navigate to URL
    if (cmd == 2) {
        const char* urlStr = (const char*)p1;
        if (!urlStr) return 0;
        
        std::wstring wUrl = ToWide(urlStr);
        
        if (!g_webview) {
            writeLog("Navigate cached (controller not ready yet): %s", urlStr);
            g_pendingNavigate = wUrl;
            return 1;
        }
        
        writeLog("Navigating to: %s", urlStr);
        HRESULT hr = g_webview->Navigate(wUrl.c_str());
        if (FAILED(hr)) writeLog("Navigate FAILED: 0x%08X", (unsigned int)hr);
        return SUCCEEDED(hr) ? 1 : 0;
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

    // COMMAND 4: Execute JavaScript
    if (cmd == 4) {
        if (g_webview) {
            const char* scriptStr = (const char*)p1;
            if (!scriptStr) return 0;
            std::wstring wScript = ToWide(scriptStr);
            g_webview->ExecuteScript(wScript.c_str(), nullptr);
            return 1;
        }
        return 0;
    }

    // COMMAND 5: Set Visibility
    if (cmd == 5) {
        if (g_webviewController) {
            g_webviewController->put_IsVisible(p1 == 1);
            return 1;
        }
        return 0;
    }
    
    // COMMAND 6: Reload
    if (cmd == 6) {
        if (g_webview) {
            g_webview->Reload();
            return 1;
        }
        return 0;
    }
    
    // COMMAND 7: Stop loading
    if (cmd == 7) {
        if (g_webview) {
            g_webview->Stop();
            return 1;
        }
        return 0;
    }

    // COMMAND 8: Go back
    if (cmd == 8) {
        if (g_webview) {
            g_webview->GoBack();
            return 1;
        }
        return 0;
    }

    // COMMAND 9: Go forward
    if (cmd == 9) {
        if (g_webview) {
            g_webview->GoForward();
            return 1;
        }
        return 0;
    }

    // COMMAND 10: Query URL (store in buffer)
    if (cmd == 10) {
        if (g_webview) {
            LPWSTR uri = nullptr;
            g_webview->get_Source(&uri);
            if (uri) {
                std::string utf8 = ToUTF8(uri);
                strncpy(g_urlBuffer, utf8.c_str(), sizeof(g_urlBuffer) - 1);
                CoTaskMemFree(uri);
                return 1;
            }
        }
        return 0;
    }

    // COMMAND 11: Query title (store in buffer)
    if (cmd == 11) {
        if (g_webview) {
            LPWSTR title = nullptr;
            g_webview->get_DocumentTitle(&title);
            if (title) {
                std::string utf8 = ToUTF8(title);
                strncpy(g_titleBuffer, utf8.c_str(), sizeof(g_titleBuffer) - 1);
                CoTaskMemFree(title);
                return 1;
            }
        }
        return 0;
    }

    // COMMAND 12: Set user agent string
    if (cmd == 12) {
        if (g_settings) {
            const char* ua = (const char*)p1;
            if (!ua) return 0;
            
            // Need ICoreWebView2Settings2 for user agent
            ICoreWebView2Settings2* settings2 = nullptr;
            g_settings->QueryInterface(IID_PPV_ARGS(&settings2));
            if (settings2) {
                std::wstring wua = ToWide(ua);
                settings2->put_UserAgent(wua.c_str());
                settings2->Release();
                writeLog("User agent set: %s", ua);
                return 1;
            }
        }
        return 0;
    }

    // COMMAND 13: Post web message to page
    if (cmd == 13) {
        if (g_webview) {
            const char* msg = (const char*)p1;
            if (!msg) return 0;
            std::wstring wmsg = ToWide(msg);
            // Try as JSON first, fallback to string
            HRESULT hr = g_webview->PostWebMessageAsJson(wmsg.c_str());
            if (FAILED(hr)) hr = g_webview->PostWebMessageAsString(wmsg.c_str());
            return SUCCEEDED(hr) ? 1 : 0;
        }
        return 0;
    }

    // COMMAND 14: Enable/disable content settings
    if (cmd == 14) {
        if (g_settings) {
            int settingId = (int)p1;
            BOOL enable = p2 ? TRUE : FALSE;
            switch (settingId) {
                case 0: g_settings->put_IsScriptEnabled(enable); break;
                case 1: g_settings->put_IsWebMessageEnabled(enable); break;
                case 2: g_settings->put_AreDefaultScriptDialogsEnabled(enable); break;
                case 3: g_settings->put_IsStatusBarEnabled(enable); break;
                case 4: g_settings->put_AreDevToolsEnabled(enable); break;
                case 5: g_settings->put_AreDefaultContextMenusEnabled(enable); break;
                case 6: g_settings->put_IsZoomControlEnabled(enable); break;
                default: return 0;
            }
            return 1;
        }
        return 0;
    }

    // COMMAND 15: Open DevTools
    if (cmd == 15) {
        if (g_webview) {
            g_webview->OpenDevToolsWindow();
            return 1;
        }
        return 0;
    }

    // COMMAND 16: Close/Destroy
    if (cmd == 16) {
        if (g_webviewController) {
            g_webviewController->Close();
            g_webviewController->Release();
            g_webviewController = nullptr;
            g_webview = nullptr;
            g_settings = nullptr;
            writeLog("WebView2 destroyed.");
            return 1;
        }
        return 0;
    }

    // COMMAND 17: Set zoom factor (p1 = factor * 100, e.g. 150 = 1.5x)
    if (cmd == 17) {
        if (g_webviewController) {
            double factor = (double)p1 / 100.0;
            g_webviewController->put_ZoomFactor(factor);
            return 1;
        }
        return 0;
    }

    // COMMAND 18: Print
    if (cmd == 18) {
        if (g_webview) {
            // Execute window.print() in the webview
            g_webview->ExecuteScript(L"window.print()", nullptr);
            return 1;
        }
        return 0;
    }

    // COMMAND 19: Check navigation state -> bitfield (bit0=canGoBack, bit1=canGoForward)
    if (cmd == 19) {
        uint64_t result = 0;
        if (g_webview) {
            BOOL canGoBack = FALSE, canGoForward = FALSE;
            g_webview->get_CanGoBack(&canGoBack);
            g_webview->get_CanGoForward(&canGoForward);
            if (canGoBack) result |= 1;
            if (canGoForward) result |= 2;
        }
        return result;
    }

    // COMMAND 20: Capture preview/screenshot (p1=filepath_ptr, p2=format: 0=PNG,1=JPEG)
    if (cmd == 20) {
        if (g_webview) {
            const char* filePath = (const char*)p1;
            if (!filePath) return 0;
            std::string pathStr(filePath);
            
            COREWEBVIEW2_CAPTURE_PREVIEW_IMAGE_FORMAT format = 
                p2 == 1 ? COREWEBVIEW2_CAPTURE_PREVIEW_IMAGE_FORMAT_JPEG 
                        : COREWEBVIEW2_CAPTURE_PREVIEW_IMAGE_FORMAT_PNG;
            
            // Create file stream
            IStream* stream = nullptr;
            std::wstring wPath = ToWide(filePath);
            HRESULT hr = SHCreateStreamOnFileW(wPath.c_str(), STGM_READWRITE | STGM_CREATE, &stream);
            if (FAILED(hr) || !stream) {
                writeLog("Failed to create file stream for screenshot: 0x%08X", (unsigned int)hr);
                return 0;
            }
            
            g_webview->CapturePreview(format, stream,
                Microsoft::WRL::Callback<ICoreWebView2CapturePreviewCompletedHandler>(
                    [stream, pathStr](HRESULT errorCode) -> HRESULT {
                        stream->Release();
                        if (SUCCEEDED(errorCode)) {
                            writeLog("Screenshot saved: %s", pathStr.c_str());
                        } else {
                            writeLog("Screenshot failed: 0x%08X", (unsigned int)errorCode);
                        }
                        return S_OK;
                    }).Get());
            return 1;
        }
        return 0;
    }

    // COMMAND 21: Poll web message (returns 1 if message available)
    if (cmd == 21) {
        if (g_hasWebMessage) {
            g_hasWebMessage = false;
            return 1;
        }
        return 0;
    }

    // COMMAND 22: Set background color (p1=r, p2=g, p3=b, p4=a)
    if (cmd == 22) {
        if (g_webviewController) {
            ICoreWebView2Controller2* ctrl2 = nullptr;
            g_webviewController->QueryInterface(IID_PPV_ARGS(&ctrl2));
            if (ctrl2) {
                COREWEBVIEW2_COLOR color;
                color.R = (BYTE)p1;
                color.G = (BYTE)p2;
                color.B = (BYTE)p3;
                color.A = (BYTE)p4;
                ctrl2->put_DefaultBackgroundColor(color);
                ctrl2->Release();
                return 1;
            }
        }
        return 0;
    }

    // COMMAND 23: Suspend (minimize resources)
    if (cmd == 23) {
        if (g_webview) {
            ICoreWebView2_3* wv3 = nullptr;
            g_webview->QueryInterface(IID_PPV_ARGS(&wv3));
            if (wv3) {
                wv3->TrySuspend(nullptr);
                wv3->Release();
                writeLog("WebView suspended.");
                return 1;
            }
        }
        return 0;
    }

    // COMMAND 24: Resume
    if (cmd == 24) {
        if (g_webview) {
            ICoreWebView2_3* wv3 = nullptr;
            g_webview->QueryInterface(IID_PPV_ARGS(&wv3));
            if (wv3) {
                wv3->Resume();
                wv3->Release();
                writeLog("WebView resumed.");
                return 1;
            }
        }
        return 0;
    }

    return 0;
}

// ---- String Query Export ----
// Returns a pointer to static string buffers for string data retrieval.
// queryId: 0=URL, 1=Title, 2=WebMessage
extern "C" __declspec(dllexport) const char* __stdcall QueryString(
    uint64_t queryId, uint64_t, uint64_t, uint64_t,
    uint64_t, uint64_t, uint64_t, uint64_t)
{
    switch (queryId) {
        case 0: return g_urlBuffer;
        case 1: return g_titleBuffer;
        case 2: return g_webMessageBuffer;
        default: return "";
    }
}
