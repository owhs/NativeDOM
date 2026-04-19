// ============================================================================
// DOM Engine — Custom Script UI Framework
// Zero external dependencies. Win32 + GDI+ + WinHTTP + custom JS-like engine.
// ============================================================================
#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include <string>
#include <vector>
#include <memory>
#include <cstdio>

#pragma comment(lib, "User32.lib")
#pragma comment(lib, "Gdi32.lib")
#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "Shell32.lib")

bool g_isFallback = false;

#include <stdlib.h>
#define GLAD_MALLOC malloc
#define GLAD_FREE free
#define GLAD_GL_IMPLEMENTATION
#include "src/Rendering/glad/glad.h"
#define NANOVG_GL3_IMPLEMENTATION
#include "src/Rendering/nanovg.h"
#include "src/Rendering/nanovg_gl.h"
#include "src/Rendering/nanovg.c"

NVGcontext* g_nvg = nullptr;

HWND g_hwnd = NULL;
const UINT WM_APP_ASYNC_DONE = WM_APP + 1;
const UINT WM_APP_CUSTOM_MSG = WM_APP + 2;

// Include the engine
#include "src/Platform/Hooks.h"
#include "src/Core/Interpreter.h"
#include "src/DOM/Element.h"
#include "src/DOM/Selector.h"

#ifdef COMPILED_DOM_PROJECT
#include "bundled_app.h"
#endif

#ifndef IS_AOT_COMPILED
#include "src/Parser/DOMParser.h"
#endif

#ifndef COMPILED_DOM_PROJECT
#include "src/Parser/AOTCompiler.h"
#endif

#include "src/Platform/DLLBridge.h"
#include "src/Platform/Http.h"
#include "src/Platform/Window.h"
#include "src/ScriptBridge.h"

// --- RC4 & LZNT1 Utilities ---
#ifndef IS_AOT_COMPILED
namespace BundlerUtils {
    static void RC4(std::string& data, const std::string& key) {
        if (key.empty()) return;
        unsigned char S[256];
        for (int i = 0; i < 256; i++) S[i] = i;
        int j = 0;
        for (int i = 0; i < 256; i++) {
            j = (j + S[i] + key[i % key.length()]) % 256;
            std::swap(S[i], S[j]);
        }
        int i = 0; j = 0;
        for (size_t n = 0; n < data.size(); n++) {
            i = (i + 1) % 256;
            j = (j + S[i]) % 256;
            std::swap(S[i], S[j]);
            data[n] ^= S[(S[i] + S[j]) % 256];
        }
    }

    typedef NTSTATUS(WINAPI* RtlGetCompressionWorkSpaceSize_t)(USHORT, PULONG, PULONG);
    typedef NTSTATUS(WINAPI* RtlCompressBuffer_t)(USHORT, PUCHAR, ULONG, PUCHAR, ULONG, ULONG, PULONG, PVOID);
    typedef NTSTATUS(WINAPI* RtlDecompressBuffer_t)(USHORT, PUCHAR, ULONG, PUCHAR, ULONG, PULONG);
#ifndef COMPRESSION_FORMAT_LZNT1
#define COMPRESSION_FORMAT_LZNT1 0x0002
#endif

    static std::string CompressLZNT1(const std::string& src) {
        HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
        if (!hNtdll) return src;
        auto pRtlGetCompressionWorkSpaceSize = (RtlGetCompressionWorkSpaceSize_t)GetProcAddress(hNtdll, "RtlGetCompressionWorkSpaceSize");
        auto pRtlCompressBuffer = (RtlCompressBuffer_t)GetProcAddress(hNtdll, "RtlCompressBuffer");
        if (!pRtlGetCompressionWorkSpaceSize || !pRtlCompressBuffer) return src;

        ULONG bufferSpace, fragSpace;
        pRtlGetCompressionWorkSpaceSize(COMPRESSION_FORMAT_LZNT1, &bufferSpace, &fragSpace);
        std::vector<UCHAR> workspace(bufferSpace);

        std::string dest;
        dest.resize(src.size() + 1024);
        ULONG finalSize = 0;
        NTSTATUS status = pRtlCompressBuffer(COMPRESSION_FORMAT_LZNT1, (PUCHAR)src.data(), src.size(), (PUCHAR)dest.data(), dest.size(), 4096, &finalSize, workspace.data());
        if (status >= 0) {
            dest.resize(finalSize);
            return dest;
        }
        return src;
    }

    static std::string DecompressLZNT1(const std::string& src, ULONG originalSize) {
        if (originalSize == 0) return "";
        HMODULE hNtdll = GetModuleHandleA("ntdll.dll");
        if (!hNtdll) return src;
        auto pRtlDecompressBuffer = (RtlDecompressBuffer_t)GetProcAddress(hNtdll, "RtlDecompressBuffer");
        if (!pRtlDecompressBuffer) return src;
        std::string dest;
        dest.resize(originalSize);
        ULONG finalSize = 0;
        NTSTATUS status = pRtlDecompressBuffer(COMPRESSION_FORMAT_LZNT1, (PUCHAR)dest.data(), dest.size(), (PUCHAR)src.data(), src.size(), &finalSize);
        if (status >= 0) return dest;
        return src;
    }

    static std::string ToHexArray(const std::string& data) {
        std::string out;
        char buf[8];
        for (size_t i = 0; i < data.size(); i++) {
            sprintf(buf, "0x%02X,", (unsigned char)data[i]);
            out += buf;
            if (i % 16 == 15) out += "\n";
        }
        return out;
    }

    static std::string StripComments(const std::string& input) {
        std::string out;
        out.reserve(input.size());
        bool inBlockComment = false;
        bool inString = false;
        char stringChar = 0;
        bool inHtmlComment = false;
        
        for (size_t i = 0; i < input.size(); ++i) {
            if (!inBlockComment && !inHtmlComment && !inString) {
                if (input[i] == '"' || input[i] == '\'') {
                    inString = true;
                    stringChar = input[i];
                    out += input[i];
                    continue;
                }
                if (i + 3 < input.size() && input[i] == '<' && input[i+1] == '!' && input[i+2] == '-' && input[i+3] == '-') {
                    inHtmlComment = true;
                    i += 3;
                    continue;
                }
                if (i + 1 < input.size() && input[i] == '/' && input[i+1] == '*') {
                    inBlockComment = true;
                    i++;
                    continue;
                }
                out += input[i];
            } else if (inString) {
                if (input[i] == '\\') {
                    out += input[i];
                    if (i + 1 < input.size()) out += input[++i];
                } else if (input[i] == stringChar) {
                    inString = false;
                    out += input[i];
                } else {
                    out += input[i];
                }
            } else if (inBlockComment) {
                if (i + 1 < input.size() && input[i] == '*' && input[i+1] == '/') {
                    inBlockComment = false;
                    i++;
                }
            } else if (inHtmlComment) {
                if (i + 2 < input.size() && input[i] == '-' && input[i+1] == '-' && input[i+2] == '>') {
                    inHtmlComment = false;
                    i += 2;
                }
            }
        }
        return out;
    }
}
#endif

void DynAPI::init() {
    HMODULE hGdi = GetModuleHandleA("gdi32.dll");
    if (!hGdi) hGdi = LoadLibraryA("gdi32.dll");
    _BitBlt = (decltype(&BitBlt))GetProcAddress(hGdi, "BitBlt");
    _CreateCompatibleDC = (decltype(&CreateCompatibleDC))GetProcAddress(hGdi, "CreateCompatibleDC");

    HMODULE hUser = GetModuleHandleA("user32.dll");
    if (!hUser) hUser = LoadLibraryA("user32.dll");
    _GetDC = (decltype(&GetDC))GetProcAddress(hUser, "GetDC");
    _FindWindowA = (decltype(&FindWindowA))GetProcAddress(hUser, "FindWindowA");
    _GetWindowLongA = (decltype(&GetWindowLongA))GetProcAddress(hUser, "GetWindowLongA");
    _SetWindowLongA = (decltype(&SetWindowLongA))GetProcAddress(hUser, "SetWindowLongA");

    HMODULE hDwmapi = GetModuleHandleA("dwmapi.dll");
    if (!hDwmapi) hDwmapi = LoadLibraryA("dwmapi.dll");
    if (hDwmapi) {
        _DwmExtendFrameIntoClientArea = (DwmExtendArea_t)GetProcAddress(hDwmapi, "DwmExtendFrameIntoClientArea");
        _DwmSetWindowAttribute = (DwmSetAttr_t)GetProcAddress(hDwmapi, "DwmSetWindowAttribute");
    }
}

DynAPI dynAPI;

// ---- Globals ----
std::shared_ptr<Element> appRoot;
Element* focusedElement = nullptr;
Element* hoveredElement = nullptr;
Element* activeElement = nullptr;

Interpreter* g_interp = nullptr;
ScriptBridge* g_bridge = nullptr;

// ---- Debug logging helper ----
void sysLog(const std::string& msg) {
    printf("[DOM] %s\n", msg.c_str());
    OutputDebugStringA(("[DOM] " + msg + "\n").c_str());
    FILE* log = fopen("sys_log.txt", "a");
    if (log) { fprintf(log, "[DOM] %s\n", msg.c_str()); fclose(log); }
}

// ---- Window Procedure ----
void RequestRedraw(HWND hwnd) {
    if (!hwnd || !appRoot || !g_nvg) return;
    HDC hdc = GetDC(hwnd);
    
    bool isLayered = (GetWindowLongA(hwnd, GWL_EXSTYLE) & WS_EX_LAYERED);
    RECT rc;
    if (isLayered) {
        GetWindowRect(hwnd, &rc);
    } else {
        GetClientRect(hwnd, &rc);
    }
    
    int w = rc.right - rc.left;
    int h = rc.bottom - rc.top;
    if (w == 0 || h == 0) w = h = 1;

    if (isLayered) {
        static GLuint fbo = 0;
        static GLuint tex = 0;
        static int fboW = 0, fboH = 0;

        if (fbo == 0) {
            glGenFramebuffers(1, &fbo);
            glGenTextures(1, &tex);
        }
        
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        if (w != fboW || h != fboH) {
            glBindTexture(GL_TEXTURE_2D, tex);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, NULL);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
            fboW = w;
            fboH = h;
        }

        glViewport(0, 0, w, h);
        glClearColor(0, 0, 0, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        nvgBeginFrame(g_nvg, (float)w, (float)h, 1.0f);
        appRoot->Layout(0, 0);
        appRoot->Draw(g_nvg);
        nvgEndFrame(g_nvg);

        HDC hdcScreen = GetDC(NULL);
        HDC memDC = dynAPI._CreateCompatibleDC(hdcScreen);
        BITMAPINFO bmi = {};
        bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        bmi.bmiHeader.biWidth = w;
        bmi.bmiHeader.biHeight = h; 
        bmi.bmiHeader.biPlanes = 1;
        bmi.bmiHeader.biBitCount = 32;
        bmi.bmiHeader.biCompression = BI_RGB;
        
        void* bits;
        HBITMAP memBmp = CreateDIBSection(hdcScreen, &bmi, DIB_RGB_COLORS, &bits, NULL, 0);
        HBITMAP oldBmp = (HBITMAP)SelectObject(memDC, memBmp);

        glReadPixels(0, 0, w, h, GL_BGRA, GL_UNSIGNED_BYTE, bits);
        
        unsigned char* p = (unsigned char*)bits;
        for(int i=0; i<w*h; i++) {
            unsigned char a = p[i*4+3];
            p[i*4+0] = (p[i*4+0] * a) / 255;
            p[i*4+1] = (p[i*4+1] * a) / 255;
            p[i*4+2] = (p[i*4+2] * a) / 255;
        }

        BLENDFUNCTION blend = {};
        blend.BlendOp = AC_SRC_OVER;
        blend.SourceConstantAlpha = 255;
        blend.AlphaFormat = AC_SRC_ALPHA;
        POINT ptPos = { rc.left, rc.top };
        SIZE size = { w, h };
        POINT ptSrc = { 0, 0 };
        UpdateLayeredWindow(hwnd, hdcScreen, &ptPos, &size, memDC, &ptSrc, 0, &blend, ULW_ALPHA);
        
        SelectObject(memDC, oldBmp);
        DeleteObject(memBmp);
        DeleteDC(memDC);
        ReleaseDC(NULL, hdcScreen);

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    } else {
        glViewport(0, 0, w, h);
        glClearColor(0, 0, 0, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

        nvgBeginFrame(g_nvg, (float)w, (float)h, 1.0f);
        appRoot->Layout(0, 0);
        appRoot->Draw(g_nvg);
        nvgEndFrame(g_nvg);

        SwapBuffers(hdc);
    }
    ReleaseDC(hwnd, hdc);
}

// ---- Position parser helper ----
static int parsePos(std::string s, int maxSz, int selfSz) {
    if (s == "center") return (maxSz - selfSz) / 2;
    size_t pct;
    while ((pct = s.find('%')) != std::string::npos) {
        int nS = (int)pct - 1;
        while(nS >= 0 && (isdigit((unsigned char)s[nS]) || s[nS] == '.' || s[nS] == '-')) nS--;
        nS++;
        char* fEnd;
        float pcf = strtof(s.substr(nS, pct - nS).c_str(), &fEnd);
        if (fEnd == s.substr(nS, pct - nS).c_str()) break;
        pcf /= 100.0f;
        char buf[32]; snprintf(buf, sizeof(buf), "%d", (int)(maxSz * pcf));
        s.replace(nS, pct - nS + 1, buf);
    }
    if (appRoot) s = appRoot->ApplyVars(s);
    char* end; double val = strtod(s.c_str(), &end);
    return end != s.c_str() ? (int)val : CW_USEDEFAULT;
}

static void onInvalidate() {
    if (g_hwnd) RequestRedraw(g_hwnd);
}

#define InvalidateRect(hwnd, lpRect, bErase) RequestRedraw(hwnd)

LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (msg == WM_APP_ASYNC_DONE) {
        RequestRedraw(hwnd);
        return 0;
    }
    if (msg == WM_APP_CUSTOM_MSG && g_bridge) {
        auto eventObj = Value::Object();
        eventObj->setProperty("type", Value::Str("customMessage"));
        eventObj->setProperty("wParam", Value::Num((double)wp));
        eventObj->setProperty("lParam", Value::Num((double)lp));
        g_bridge->dispatchScriptEvent(appRoot.get(), "customMessage", eventObj);
        return 0;
    }

    if (!appRoot) return DefWindowProc(hwnd, msg, wp, lp);

    switch (msg) {
#ifndef IS_AOT_COMPILED
        case WM_APP + 5: {
            char* path = (char*)wp;
            std::string newPath = path;
            delete[] path;
            DOMParser::Reset(); // Reset parser state and flush old scripts
            auto newRoot = DOMParser::ParseDOM(newPath);
            if (newRoot) {
                appRoot = newRoot;
                appRoot->Layout(0, 0);

                DWORD oldStyle = dynAPI._GetWindowLongA(hwnd, GWL_STYLE);
                DWORD style = WS_OVERLAPPEDWINDOW;
                if (appRoot->Get("frameless") == "true" && appRoot->Get("system-shadow") == "false") {
                    style = WS_POPUP | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU;
                }
                if (appRoot->Get("resizable") == "false") style &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);
                if (appRoot->Get("maximizable") == "false") style &= ~WS_MAXIMIZEBOX;
                style |= (oldStyle & WS_VISIBLE); // Preserve visibility
                dynAPI._SetWindowLongA(hwnd, GWL_STYLE, style);

                DWORD exStyle = dynAPI._GetWindowLongA(hwnd, GWL_EXSTYLE);
                if (appRoot->Get("transparent") == "true") exStyle |= WS_EX_LAYERED;
                else exStyle &= ~WS_EX_LAYERED;
                dynAPI._SetWindowLongA(hwnd, GWL_EXSTYLE, exStyle);

                if (dynAPI._DwmExtendFrameIntoClientArea) {
                    if (appRoot->Get("system-shadow") == "true") {
                        if (dynAPI._DwmSetWindowAttribute) {
                            DWORD policy = 2; // DWMNCRP_ENABLED
                            dynAPI._DwmSetWindowAttribute(hwnd, 2 /*DWMWA_NCRENDERING_POLICY*/, &policy, sizeof(policy));
                            DWORD cornerPref = 2; // DWMWCP_ROUND
                            dynAPI._DwmSetWindowAttribute(hwnd, 33, &cornerPref, sizeof(cornerPref));
                        }
                        MARGINS ms = { 1, 1, 1, 1 };
                        dynAPI._DwmExtendFrameIntoClientArea(hwnd, &ms);
                    } else if (appRoot->Get("system-shadow") == "false") {
                        if (dynAPI._DwmSetWindowAttribute) {
                            DWORD policy = 1; // DWMNCRP_DISABLED
                            dynAPI._DwmSetWindowAttribute(hwnd, 2 /*DWMWA_NCRENDERING_POLICY*/, &policy, sizeof(policy));
                            DWORD cornerPref = 1; // DWMWCP_DONOTROUND
                            dynAPI._DwmSetWindowAttribute(hwnd, 33, &cornerPref, sizeof(cornerPref));
                        }
                        MARGINS ms = { 0, 0, 0, 0 };
                        dynAPI._DwmExtendFrameIntoClientArea(hwnd, &ms);
                    } else {
                        MARGINS ms = { 0, 0, 0, 0 };
                        dynAPI._DwmExtendFrameIntoClientArea(hwnd, &ms);
                        if (dynAPI._DwmSetWindowAttribute) {
                            DWORD cornerPref = 0; // DWMWCP_DEFAULT
                            dynAPI._DwmSetWindowAttribute(hwnd, 33, &cornerPref, sizeof(cornerPref));
                        }
                    }
                }
                
                if (g_bridge) g_bridge->reset(newRoot);

                int screenW = GetSystemMetrics(SM_CXSCREEN);
                int screenH = GetSystemMetrics(SM_CYSCREEN);
                int newW = appRoot->GetInt("width", 800);
                int newH = appRoot->GetInt("height", 600);

                RECT rc; GetWindowRect(hwnd, &rc);
                int x = rc.left, y = rc.top;

                if (appRoot->Get("centered") == "true") { x = (screenW - newW) / 2; y = (screenH - newH) / 2; }
                std::string px = appRoot->Get("position-x"), py = appRoot->Get("position-y");
                if (!px.empty()) { int p = parsePos(px, screenW, newW); if (p != CW_USEDEFAULT) x = p; }
                if (!py.empty()) { int p = parsePos(py, screenH, newH); if (p != CW_USEDEFAULT) y = p; }

                SetWindowPos(hwnd, NULL, x, y, newW, newH, SWP_NOZORDER | SWP_FRAMECHANGED);
                RequestRedraw(hwnd);

                if (g_interp && g_bridge) {
                    if (!DOMParser::globalScripts.empty()) {
                        g_interp->exec(DOMParser::globalScripts);
                    }
                    auto onloadObj = Value::Object();
                    onloadObj->setProperty("type", Value::Str("load"));
                    g_bridge->dispatchScriptEvent(appRoot.get(), "load", onloadObj);
                }
            }
            return 0;
        }
#endif
        case WM_NCCALCSIZE: {
            if (wp && appRoot && appRoot->Get("frameless") == "true") {
                NCCALCSIZE_PARAMS* params = (NCCALCSIZE_PARAMS*)lp;
                if (IsZoomed(hwnd)) {
                    int xBorders = GetSystemMetrics(SM_CXSIZEFRAME);
                    int yBorders = GetSystemMetrics(SM_CYSIZEFRAME) + GetSystemMetrics(SM_CXPADDEDBORDER);
                    params->rgrc[0].left += xBorders;
                    params->rgrc[0].right -= xBorders;
                    params->rgrc[0].top += yBorders;
                    params->rgrc[0].bottom -= yBorders;
                }
                return 0; // The entire window is client area
            }
            break;
        }

        case WM_GETMINMAXINFO: {
            if (appRoot && appRoot->Get("frameless") == "true") {
                MINMAXINFO* mmi = (MINMAXINFO*)lp;
                mmi->ptMinTrackSize.x = 200;
                mmi->ptMinTrackSize.y = 150;
                return 0;
            }
            break;
        }

        case WM_NCPAINT: {
            if (appRoot && appRoot->Get("frameless") == "true") return 0;
            break;
        }

        case WM_NCACTIVATE: {
            if (appRoot && appRoot->Get("frameless") == "true") return TRUE;
            break;
        }

        case WM_DROPFILES: {
            HDROP hDrop = (HDROP)wp;
            char filePath[MAX_PATH];
            if (DragQueryFileA(hDrop, 0, filePath, MAX_PATH)) {
                if (g_isFallback) {
                    char exePath[MAX_PATH];
                    GetModuleFileNameA(NULL, exePath, MAX_PATH);
                    std::string cmd = std::string("\"") + exePath + "\" \"" + filePath + "\"";
                    
                    STARTUPINFOA si = { sizeof(si) };
                    PROCESS_INFORMATION pi;
                    if (CreateProcessA(NULL, (LPSTR)cmd.c_str(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
                        CloseHandle(pi.hProcess);
                        CloseHandle(pi.hThread);
                    }
                    PostQuitMessage(0);
                } else if (g_bridge) {
                    auto eventObj = Value::Object();
                    eventObj->setProperty("type", Value::Str("drop"));
                    eventObj->setProperty("file", Value::Str(filePath));
                    g_bridge->dispatchScriptEvent(appRoot.get(), "drop", eventObj);
                }
            }
            DragFinish(hDrop);
            return 0;
        }

        case WM_NCHITTEST: {
            LRESULT hit = DefWindowProc(hwnd, msg, wp, lp);
            if (hit == HTCLIENT) {
                POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
                ScreenToClient(hwnd, &pt);
                RECT rc;
                GetClientRect(hwnd, &rc);
                int borderLimit = 8;
                bool isTop = pt.y < borderLimit;
                bool isBottom = pt.y >= rc.bottom - borderLimit;
                bool isLeft = pt.x < borderLimit;
                bool isRight = pt.x >= rc.right - borderLimit;
                
                if (isTop && isLeft) return HTTOPLEFT;
                if (isTop && isRight) return HTTOPRIGHT;
                if (isBottom && isLeft) return HTBOTTOMLEFT;
                if (isBottom && isRight) return HTBOTTOMRIGHT;
                if (isTop) return HTTOP;
                if (isBottom) return HTBOTTOM;
                if (isLeft) return HTLEFT;
                if (isRight) return HTRIGHT;
            }
            return hit;
        }
        case WM_MOUSEMOVE: {
            int mx = GET_X_LPARAM(lp);
            int my = GET_Y_LPARAM(lp);

            // Track mouse for WM_MOUSELEAVE
            TRACKMOUSEEVENT tme = { sizeof(tme), TME_LEAVE, hwnd, 0 };
            TrackMouseEvent(&tme);

            auto hits = appRoot->HitTestAll(mx, my);
            Element* hit = hits.empty() ? nullptr : hits.front();

            if (hit != hoveredElement) {
                // Remove hover from old chain
                if (hoveredElement) {
                    Element* c = hoveredElement;
                    while (c) { c->isHovered = false; c = c->parent ? c->parent : c->shadowHost; }
                    if (g_bridge) g_bridge->dispatchMouseEvent(hoveredElement, "mouseleave", mx, my);
                }
                hoveredElement = hit;
                // Add hover to new chain
                if (hoveredElement) {
                    Element* c = hoveredElement;
                    while (c) { c->isHovered = true; c = c->parent ? c->parent : c->shadowHost; }
                    if (g_bridge) g_bridge->dispatchMouseEvent(hoveredElement, "mouseenter", mx, my);
                }
                RequestRedraw(hwnd);
            }

            if (g_bridge) {
                for (Element* h : hits) g_bridge->dispatchMouseEvent(h, "mousemove", mx, my);
            }

            // Cursor
            std::string cursorMode = "arrow";
            Element* curr = hit;
            while (curr) {
                std::string c = curr->Get("cursor");
                if (c == "pointer" || !curr->Get("onClick").empty()) { cursorMode = "pointer"; break; }
                if (c == "text") { cursorMode = "text"; break; }
                if (c == "move") { cursorMode = "move"; break; }
                curr = curr->parent ? curr->parent : curr->shadowHost;
            }
            if (cursorMode == "pointer") SetCursor(LoadCursor(NULL, IDC_HAND));
            else if (cursorMode == "text") SetCursor(LoadCursor(NULL, IDC_IBEAM));
            else if (cursorMode == "move") SetCursor(LoadCursor(NULL, IDC_SIZEALL));
            else SetCursor(LoadCursor(NULL, IDC_ARROW));
            return 0;
        }

        case WM_MOUSELEAVE: {
            if (hoveredElement) {
                Element* c = hoveredElement;
                while (c) { c->isHovered = false; c = c->parent ? c->parent : c->shadowHost; }
                if (g_bridge) g_bridge->dispatchMouseEvent(hoveredElement, "mouseleave", 0, 0);
                hoveredElement = nullptr;
                RequestRedraw(hwnd);
            }
            return 0;
        }

        case WM_LBUTTONDOWN: {
            int mx = GET_X_LPARAM(lp);
            int my = GET_Y_LPARAM(lp);

            // Clear old focus
            if (focusedElement) {
                Element* c = focusedElement;
                while (c) { c->isFocused = false; c = c->parent ? c->parent : c->shadowHost; }
                if (g_bridge) g_bridge->dispatchScriptEvent(focusedElement, "blur");
                focusedElement = nullptr;
            }

            auto hits = appRoot->HitTestAll(mx, my);
            Element* hit = hits.empty() ? nullptr : hits.front();
            
            if (hit) {
                // Set new focus
                focusedElement = hit;
                Element* c = focusedElement;
                while (c) { c->isFocused = true; c = c->parent ? c->parent : c->shadowHost; }
                if (g_bridge) {
                    g_bridge->dispatchScriptEvent(focusedElement, "focus");
                    for (Element* h : hits) g_bridge->dispatchMouseEvent(h, "mousedown", mx, my);
                }

                // Active state
                activeElement = hit;
                hit->isActive = true;

                // Walk up for drag
                Element* curr = hit;
                while (curr) {
                    if (curr->Get("drag") == "true") {
                        ReleaseCapture();
                        SendMessage(hwnd, WM_NCLBUTTONDOWN, HTCAPTION, 0);
                        return 0;
                    }
                    curr = curr->parent ? curr->parent : curr->shadowHost;
                }
            }
            RequestRedraw(hwnd);
            return 0;
        }

        case WM_LBUTTONDBLCLK: {
            int mx = GET_X_LPARAM(lp);
            int my = GET_Y_LPARAM(lp);
            auto hits = appRoot->HitTestAll(mx, my);
            if (g_bridge) {
                for (Element* h : hits) {
                    g_bridge->dispatchMouseEvent(h, "dblclick", mx, my);
                }
            }
            return 0;
        }

        case WM_LBUTTONUP: {
            int mx = GET_X_LPARAM(lp);
            int my = GET_Y_LPARAM(lp);
            auto hits = appRoot->HitTestAll(mx, my);
            Element* hit = hits.empty() ? nullptr : hits.front();

            // Clear old active state on whatever element received mousedown
            if (activeElement) {
                Element* c = activeElement;
                while (c) { c->isActive = false; c = c->parent ? c->parent : c->shadowHost; }
            }

            if (g_bridge) {
                for (Element* h : hits) {
                    g_bridge->dispatchMouseEvent(h, "mouseup", mx, my);
                    if (h == activeElement) {
                        g_bridge->dispatchMouseEvent(h, "click", mx, my);
                    } else if (h->Get("events") == "catchAndPass") {
                        // Allow clicking piercing elements even if not the primary active
                        g_bridge->dispatchMouseEvent(h, "click", mx, my);
                    }
                }
            }
            activeElement = nullptr;
            RequestRedraw(hwnd);
            return 0;
        }

        case WM_RBUTTONUP: {
            int mx = GET_X_LPARAM(lp);
            int my = GET_Y_LPARAM(lp);
            auto hits = appRoot->HitTestAll(mx, my);
            if (g_bridge) {
                for (Element* h : hits) g_bridge->dispatchMouseEvent(h, "contextmenu", mx, my);
            }
            return 0;
        }

        case WM_MOUSEWHEEL: {
            POINT pt = { GET_X_LPARAM(lp), GET_Y_LPARAM(lp) };
            ScreenToClient(hwnd, &pt);
            auto hits = appRoot->HitTestAll(pt.x, pt.y);
            if (g_bridge) {
                auto eventObj = Value::Object();
                eventObj->setProperty("type", Value::Str("wheel"));
                eventObj->setProperty("clientX", Value::Num((double)pt.x));
                eventObj->setProperty("clientY", Value::Num((double)pt.y));
                eventObj->setProperty("deltaY", Value::Num((double)-GET_WHEEL_DELTA_WPARAM(wp) / WHEEL_DELTA * 100));
                for (Element* h : hits) g_bridge->dispatchScriptEvent(h, "wheel", eventObj);
            }
            return 0;
        }

        case WM_CHAR: {
            if (!focusedElement) return 0;
            Element* curr = focusedElement;
            while (curr) {
                std::string handler = curr->Get("onKey");
                if (!handler.empty()) {
                    // Update sys.event.keyCode
                    if (g_bridge) g_bridge->dispatchKeyEvent(curr, "keypress", (int)wp, std::string(1, (char)wp));
                    if (g_interp) g_interp->exec(handler);
                    break;
                }
                curr = curr->parent ? curr->parent : curr->shadowHost;
            }
            RequestRedraw(hwnd);
            return 0;
        }

        case WM_KEYDOWN: {
            if (focusedElement && g_bridge) {
                g_bridge->dispatchKeyEvent(focusedElement, "keydown", (int)wp, "");
            }
            // Tab handling
            if (wp == VK_TAB) {
                // Simple tab cycling (could be expanded)
            }
            return 0;
        }

        case WM_KEYUP: {
            if (focusedElement && g_bridge) {
                g_bridge->dispatchKeyEvent(focusedElement, "keyup", (int)wp, "");
            }
            return 0;
        }

        case WM_SIZE: {
            if (appRoot && appRoot->Get("frameless") == "true") {
                bool hideShadow = (appRoot->Get("system-shadow") == "false");
                if (dynAPI._DwmSetWindowAttribute) {
                    DWORD cornerPref = (IsZoomed(hwnd) || hideShadow) ? 1 /*DWMWCP_DONOTROUND*/ : 2 /*DWMWCP_ROUND*/;
                    dynAPI._DwmSetWindowAttribute(hwnd, 33, &cornerPref, sizeof(cornerPref));
                    
                    DWORD borderCol = 0xFFFFFFFE; // DWMWA_COLOR_NONE
                    if (!hideShadow && IsZoomed(hwnd) == FALSE) {
                        NVGcolor c = Element::ParseHex(appRoot->Get("bg", "#1E1E2E"));
                        borderCol = RGB((int)(c.r*255), (int)(c.g*255), (int)(c.b*255));
                    }
                    dynAPI._DwmSetWindowAttribute(hwnd, 34, &borderCol, sizeof(borderCol));
                }
                if (dynAPI._DwmExtendFrameIntoClientArea) {
                    MARGINS ms = (IsZoomed(hwnd) || hideShadow) ? MARGINS{0,0,0,0} : MARGINS{1,1,1,1};
                    dynAPI._DwmExtendFrameIntoClientArea(hwnd, &ms);
                }
            }
            if (appRoot) {
                char wBuf[16], hBuf[16];
                snprintf(wBuf, sizeof(wBuf), "%d", (int)LOWORD(lp));
                snprintf(hBuf, sizeof(hBuf), "%d", (int)HIWORD(lp));
                appRoot->SetProp("width", wBuf, 999);
                appRoot->SetProp("height", hBuf, 999);
                appRoot->Layout(0, 0);

                int r = appRoot->GetInt("border-radius", 0);
                if (r > 0 && appRoot->Get("frameless") == "true" && appRoot->Get("transparent") != "true") {
                    HRGN rgn = CreateRoundRectRgn(0, 0, LOWORD(lp) + 1, HIWORD(lp) + 1, r * 2, r * 2);
                    SetWindowRgn(hwnd, rgn, TRUE);
                } else if (appRoot->Get("frameless") == "true") {
                    SetWindowRgn(hwnd, NULL, TRUE);
                }
            }
            RequestRedraw(hwnd);
            if (g_bridge) {
                auto eventObj = Value::Object();
                eventObj->setProperty("type", Value::Str("resize"));
                eventObj->setProperty("width", Value::Num((double)LOWORD(lp)));
                eventObj->setProperty("height", Value::Num((double)HIWORD(lp)));
                g_bridge->dispatchScriptEvent(appRoot.get(), "resize", eventObj);
            }
            return 0;
        }

        case WM_PAINT: {
            if (GetWindowLongA(hwnd, GWL_EXSTYLE) & WS_EX_LAYERED) {
                PAINTSTRUCT ps;
                BeginPaint(hwnd, &ps);
                EndPaint(hwnd, &ps);
                return 0;
            }
            PAINTSTRUCT ps;
            BeginPaint(hwnd, &ps);
            RequestRedraw(hwnd);
            EndPaint(hwnd, &ps);
            return 0;
        }

        case WM_ERASEBKGND: return 1; // Prevent flicker



        case WM_HOTKEY: {
            if (g_bridge) {
                auto eventObj = Value::Object();
                eventObj->setProperty("type", Value::Str("hotkey"));
                eventObj->setProperty("id", Value::Num((double)wp));
                eventObj->setProperty("modifiers", Value::Num((double)LOWORD(lp)));
                eventObj->setProperty("vkCode", Value::Num((double)HIWORD(lp)));
                g_bridge->dispatchScriptEvent(appRoot.get(), "hotkey", eventObj);
            }
            return 0;
        }

        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
    }
    return DefWindowProc(hwnd, msg, wp, lp);
}

// ---- Entry Point ----
void* GlobalGetProcAddress(const char* name) {
    void* p = (void*)wglGetProcAddress(name);
    if(p == 0 || (p == (void*)0x1) || (p == (void*)0x2) || (p == (void*)0x3) || (p == (void*)-1)) {
        HMODULE module = GetModuleHandleA("opengl32.dll");
        p = (void*)GetProcAddress(module, name);
    }
    return p;
}

LONG WINAPI GlobalUnhandledExceptionFilter(EXCEPTION_POINTERS* ep) {
    if (FILE* f = fopen("sys_log.txt", "a")) {
        fprintf(f, "[FATAL SEH CRASH] Exception Code: 0x%08X\n", ep->ExceptionRecord->ExceptionCode);
        fclose(f);
    }
    return EXCEPTION_EXECUTE_HANDLER;
}

int WINAPI WinMain(HINSTANCE inst, HINSTANCE, LPSTR lpCmdLine, int) {
    SetUnhandledExceptionFilter(GlobalUnhandledExceptionFilter);
    dynAPI.init();
    
    // Init GDI+ removed


    // Clear log
    { FILE* f = fopen("sys_log.txt", "w"); if(f) fclose(f); }
    sysLog("DOM Engine starting...");

    std::string argStr = lpCmdLine;
    bool bundleMode = false;
    bool loadFromDllMode = false;
    std::string dllPathToLoad = "";
    std::string targetFile = "app.dom";
    std::string outFile = "bundled_app.h";
    bool useLznt1 = false;
    bool useStrip = false;
    bool useAot = false;
    std::string useRc4 = "";

    if (argStr.find("--load-dll ") == 0) {
        loadFromDllMode = true;
        dllPathToLoad = argStr.substr(11);
        if (dllPathToLoad.size() >= 2 && dllPathToLoad.front() == '"' && dllPathToLoad.back() == '"') 
            dllPathToLoad = dllPathToLoad.substr(1, dllPathToLoad.length() - 2);
    } else if (argStr.find("bundle ") == 0) {
        bundleMode = true;
        // Parse out flags safely
        if (argStr.find(" -lznt1") != std::string::npos) { useLznt1 = true; argStr.erase(argStr.find(" -lznt1"), 7); }
        if (argStr.find(" -strip") != std::string::npos) { useStrip = true; argStr.erase(argStr.find(" -strip"), 7); }
        if (argStr.find(" -aot") != std::string::npos) { useAot = true; argStr.erase(argStr.find(" -aot"), 5); }
        size_t rc4Pos = argStr.find(" -rc4=");
        size_t rc4SpacePos = argStr.find(" -rc4 ");
        if (rc4Pos != std::string::npos) {
            size_t rc4End = argStr.find(' ', rc4Pos + 1);
            if (rc4End == std::string::npos) rc4End = argStr.length();
            useRc4 = argStr.substr(rc4Pos + 6, rc4End - rc4Pos - 6);
            if (!useRc4.empty() && useRc4.front() == '"' && useRc4.back() == '"') useRc4 = useRc4.substr(1, useRc4.length() - 2);
            argStr.erase(rc4Pos, rc4End - rc4Pos);
        } else if (rc4SpacePos != std::string::npos) {
            size_t rc4End = argStr.find(' ', rc4SpacePos + 6);
            if (rc4End == std::string::npos) rc4End = argStr.length();
            useRc4 = argStr.substr(rc4SpacePos + 6, rc4End - rc4SpacePos - 6);
            if (!useRc4.empty() && useRc4.front() == '"' && useRc4.back() == '"') useRc4 = useRc4.substr(1, useRc4.length() - 2);
            argStr.erase(rc4SpacePos, rc4End - rc4SpacePos);
        }
        
        // Trim trailing spaces
        if (!argStr.empty()) argStr.erase(argStr.find_last_not_of(" \t\r\n") + 1);
        
        size_t firstSpace = argStr.find(' ', 7);
        if (firstSpace != std::string::npos) {
            targetFile = argStr.substr(7, firstSpace - 7);
            outFile = argStr.substr(firstSpace + 1);
            if (targetFile.size() >= 2 && targetFile.front() == '"' && targetFile.back() == '"') targetFile = targetFile.substr(1, targetFile.length() - 2);
            if (outFile.size() >= 2 && outFile.front() == '"' && outFile.back() == '"') outFile = outFile.substr(1, outFile.length() - 2);
        } else {
            targetFile = argStr.substr(7);
            if (targetFile.size() >= 2 && targetFile.front() == '"' && targetFile.back() == '"') targetFile = targetFile.substr(1, targetFile.length() - 2);
        }
    } else {
        if (argStr.size() >= 2 && argStr.front() == '"' && argStr.back() == '"')
            argStr = argStr.substr(1, argStr.size() - 2);
        if (!argStr.empty()) targetFile = argStr;
    }

    if (bundleMode) {
#ifndef COMPILED_DOM_PROJECT
        printf("[DOM Bundler] Compiling project %s -> %s\n", targetFile.c_str(), outFile.c_str());
        
        if (useAot) {
            DOMParser::Reset();
            auto root = DOMParser::ParseDOM(targetFile);
            if (!root) {
                printf("[DOM Bundler] ERROR: Target file %s not found or invalid!\n", targetFile.c_str());
                return 1;
            }
            std::string code = AOTCompiler::Compile(root, BundlerUtils::StripComments(DOMParser::globalScripts));
            FILE* f = fopen(outFile.c_str(), "w");
            if (f) {
                fprintf(f, "%s", code.c_str());
                fclose(f);
                printf("[DOM Bundler] AOT Compilation Success. Compile main project with /DCOMPILED_DOM_PROJECT\n");
            } else {
                printf("[DOM Bundler] Failed to write %s\n", outFile.c_str());
            }
            return 0;
        }

        std::string bundled = DOMParser::BundleDOM(targetFile);
        
        if (bundled.find("<!-- failed to load ") == 0) {
            printf("[DOM Bundler] ERROR: Target file %s not found!\n", targetFile.c_str());
            return 1;
        }

        if (useStrip) bundled = BundlerUtils::StripComments(bundled);
        size_t originalSize = bundled.size();
        if (useLznt1) bundled = BundlerUtils::CompressLZNT1(bundled);
        if (!useRc4.empty()) BundlerUtils::RC4(bundled, useRc4);
        
        FILE* f = fopen(outFile.c_str(), "w");
        if (f) {
            fprintf(f, "#pragma once\n\n");
            fprintf(f, "constexpr bool BUNDLED_APP_LZNT1 = %s;\n", useLznt1 ? "true" : "false");
            fprintf(f, "constexpr bool BUNDLED_APP_RC4 = %s;\n", useRc4.empty() ? "false" : "true");
            fprintf(f, "constexpr const char* BUNDLED_APP_RC4_KEY = \"%s\";\n", useRc4.c_str());
            fprintf(f, "constexpr unsigned long BUNDLED_APP_ORIGINAL_SIZE = %u;\n", (unsigned int)originalSize);
            fprintf(f, "constexpr unsigned char BUNDLED_DOM_APP[] = {\n%s\n0x00\n};\n", BundlerUtils::ToHexArray(bundled).c_str());
            fprintf(f, "constexpr unsigned long BUNDLED_APP_SIZE = sizeof(BUNDLED_DOM_APP) - 1;\n");
            fclose(f);
            printf("[DOM Bundler] Success. You can now compile the main project with /DCOMPILED_DOM_PROJECT\n");
        } else {
            printf("[DOM Bundler] Failed to write %s\n", outFile.c_str());
        }
        return 0;
#else
        return 1;
#endif
    }

#ifndef IS_AOT_COMPILED
    DOMParser::Reset();
#endif
    std::string errStr;

    if (loadFromDllMode) {
        sysLog("Loading DOM application from DLL: " + dllPathToLoad);
        HMODULE hModule = LoadLibraryA(dllPathToLoad.c_str());
        if (hModule) {
            typedef const unsigned char* (*GetDOMData_t)(unsigned long*, bool*, bool*, const char**, unsigned long*);
            GetDOMData_t getDOMData = (GetDOMData_t)GetProcAddress(hModule, "GetDOMData");
            if (getDOMData) {
                unsigned long outSize = 0, ogSize = 0;
                bool lznt1 = false, rc4 = false;
                const char* rc4key = "";
                const unsigned char* appData = getDOMData(&outSize, &lznt1, &rc4, &rc4key, &ogSize);
                if (appData) {
#ifndef IS_AOT_COMPILED
                    std::string embeddedApp((const char*)appData, outSize);
                    if (rc4) BundlerUtils::RC4(embeddedApp, rc4key);
                    if (lznt1) embeddedApp = BundlerUtils::DecompressLZNT1(embeddedApp, ogSize);
                    appRoot = DOMParser::ParseContent(embeddedApp, "");
#endif
                    errStr = "DLL load: " + dllPathToLoad;
                } else {
                    errStr = "Error: GetDOMData returned NULL";
                }
            } else {
                errStr = "Error: Could not find GetDOMData in DLL";
            }
        } else {
            errStr = "Error: Could not load DLL";
        }
    } else {
#ifdef COMPILED_DOM_PROJECT
        sysLog("Loading embedded DOM application...");
#ifdef IS_AOT_COMPILED
        appRoot = BuildAOTUI();
        errStr = "Embedded AOT compilation";
#else
        std::string embeddedApp((const char*)BUNDLED_DOM_APP, BUNDLED_APP_SIZE);
        if (BUNDLED_APP_RC4) BundlerUtils::RC4(embeddedApp, BUNDLED_APP_RC4_KEY);
        if (BUNDLED_APP_LZNT1) embeddedApp = BundlerUtils::DecompressLZNT1(embeddedApp, BUNDLED_APP_ORIGINAL_SIZE);
        
        appRoot = DOMParser::ParseContent(embeddedApp, "");
        errStr = "Embedded compilation";
#endif
#else
        sysLog("Loading: " + targetFile);
#ifndef IS_AOT_COMPILED
        appRoot = DOMParser::ParseDOM(targetFile);
#endif
        errStr = targetFile;
#endif
    }

    // Fallback error UI
    if (!appRoot) {
        g_isFallback = true;
        sysLog("Error: Could not parse " + errStr);
        appRoot = std::make_shared<Element>();
        appRoot->tag = "Window";
        //appRoot->SetProp("frameless", "true");
        appRoot->SetProp("resizable", "false");
        appRoot->SetProp("centered", "true");
        appRoot->SetProp("bg", "#1E1E2E");
        appRoot->SetProp("width", "600");
        appRoot->SetProp("height", "200");
        auto title = std::make_shared<Element>();
        title->tag = "Text";
        title->innerText = errStr.empty() ? "DOM Error: No .dom file provided." : ("DOM Error: Could not load " + errStr);
        title->SetProp("color", "#F38BA8");
        title->SetProp("font-size", "18");
        title->SetProp("x", "20");
        title->SetProp("y", "20");
        title->w = 560;
        title->h = 160;
        appRoot->Adopt(title, false);

        auto hint = std::make_shared<Element>();
        hint->tag = "Text";
        hint->innerText = "Drag a .dom file onto this window, or place app.dom next to the executable.";
        hint->SetProp("color", "#6C7086");
        hint->SetProp("font-size", "13");
        hint->SetProp("x", "20");
        hint->SetProp("y", "60");
        hint->w = 560;
        hint->h = 100;
        appRoot->Adopt(hint, false);
    }

    // ---- Initialize Script Engine ----
    Interpreter interp;
    g_interp = &interp;
    interp.setLogCallback(sysLog);
    interp.setInvalidateCallback(onInvalidate);

    // Bind platform APIs
    WindowAPI::BindToEnv(interp);

    // Do layout before window creation to get size/frameless info
    appRoot->Layout(0, 0);

    // ---- Create Window ----
    WNDCLASSEX wc = {};
    wc.cbSize = sizeof(wc);
    wc.style = CS_DBLCLKS; // Essential for WM_LBUTTONDBLCLK
    wc.lpfnWndProc = WndProc;
    wc.hInstance = inst;
    wc.lpszClassName = "DOMWindow";
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);
    wc.hbrBackground = NULL;
    
    wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
    wc.hIconSm = wc.hIcon;
    
    RegisterClassEx(&wc);

    DWORD style = WS_OVERLAPPEDWINDOW;
    if (appRoot->Get("frameless") == "true" && appRoot->Get("system-shadow") == "false") {
        style = WS_POPUP | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SYSMENU;
    }
    if (appRoot->Get("resizable") == "false") {
        style &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);
    }
    if (appRoot->Get("maximizable") == "false") {
        style &= ~WS_MAXIMIZEBOX;
    }
    DWORD exStyle = 0;
    if (appRoot->Get("transparent") == "true") {
        exStyle |= WS_EX_LAYERED;
    }
    int width = appRoot->GetInt("width", 800);
    int height = appRoot->GetInt("height", 600);

    // Adjust for non-client area
    RECT adjustedRect = { 0, 0, width, height };
    AdjustWindowRectEx(&adjustedRect, style, FALSE, exStyle);
    int adjW = adjustedRect.right - adjustedRect.left;
    int adjH = adjustedRect.bottom - adjustedRect.top;

    std::string title = appRoot->Get("title", "DOM App");
    
    int screenW = GetSystemMetrics(SM_CXSCREEN);
    int screenH = GetSystemMetrics(SM_CYSCREEN);
    int x = CW_USEDEFAULT;
    int y = CW_USEDEFAULT;

    if (appRoot->Get("centered") == "true" || appRoot->Get("position") == "center") {
        x = (screenW - adjW) / 2;
        y = (screenH - adjH) / 2;
    }
    std::string posX = appRoot->Get("position-x");
    std::string posY = appRoot->Get("position-y");
    if (!posX.empty()) { int p = parsePos(posX, screenW, adjW); if (p != CW_USEDEFAULT) x = p; }
    if (!posY.empty()) { int p = parsePos(posY, screenH, adjH); if (p != CW_USEDEFAULT) y = p; }

    g_hwnd = CreateWindowEx(exStyle, "DOMWindow", title.c_str(), style,
                            x, y, adjW, adjH,
                            NULL, NULL, inst, NULL);

    if (appRoot->Get("frameless") == "true") {
        SetWindowPos(g_hwnd, NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);
    }

    HDC hdc = GetDC(g_hwnd);
    PIXELFORMATDESCRIPTOR pfd = {};
    pfd.nSize = sizeof(PIXELFORMATDESCRIPTOR);
    pfd.nVersion = 1;
    pfd.dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    if (appRoot->Get("transparent") == "true") pfd.dwFlags |= PFD_SUPPORT_COMPOSITION;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cAlphaBits = 8;
    pfd.cDepthBits = 24;
    pfd.cStencilBits = 8;
    pfd.iLayerType = PFD_MAIN_PLANE;

    int pf = ChoosePixelFormat(hdc, &pfd);
    SetPixelFormat(hdc, pf, &pfd);
    HGLRC hrc = wglCreateContext(hdc);
    wglMakeCurrent(hdc, hrc);

    gladLoadGL((GLADloadfunc)GlobalGetProcAddress);

    g_nvg = nvgCreateGL3(NVG_ANTIALIAS | NVG_STENCIL_STROKES);
    
    // Add default font
    char winDir[MAX_PATH];
    GetWindowsDirectoryA(winDir, MAX_PATH);
    std::string fontPath = std::string(winDir) + "\\Fonts\\segoeui.ttf";
    nvgCreateFont(g_nvg, "Segoe UI", fontPath.c_str());

    if (g_hwnd && dynAPI._DwmExtendFrameIntoClientArea) {
        if (appRoot->Get("system-shadow") == "true") {
            if (dynAPI._DwmSetWindowAttribute) {
                DWORD policy = 2; // DWMNCRP_ENABLED
                dynAPI._DwmSetWindowAttribute(g_hwnd, 2 /*DWMWA_NCRENDERING_POLICY*/, &policy, sizeof(policy));
                DWORD cornerPref = 2; // DWMWCP_ROUND
                dynAPI._DwmSetWindowAttribute(g_hwnd, 33, &cornerPref, sizeof(cornerPref));
                DWORD borderColor = 0xFFFFFFFE; // DWMWA_COLOR_NONE
                dynAPI._DwmSetWindowAttribute(g_hwnd, 34, &borderColor, sizeof(borderColor));
            }
            MARGINS ms = { 1, 1, 1, 1 };
            dynAPI._DwmExtendFrameIntoClientArea(g_hwnd, &ms);
        } else if (appRoot->Get("system-shadow") == "false") {
            if (dynAPI._DwmSetWindowAttribute) {
                DWORD policy = 1; // DWMNCRP_DISABLED
                dynAPI._DwmSetWindowAttribute(g_hwnd, 2 /*DWMWA_NCRENDERING_POLICY*/, &policy, sizeof(policy));
                DWORD cornerPref = 1; // DWMWCP_DONOTROUND
                dynAPI._DwmSetWindowAttribute(g_hwnd, 33, &cornerPref, sizeof(cornerPref));
            }
            MARGINS ms = { 0, 0, 0, 0 };
            dynAPI._DwmExtendFrameIntoClientArea(g_hwnd, &ms);
        }
    }

    DragAcceptFiles(g_hwnd, TRUE);

    ShowWindow(g_hwnd, SW_SHOW);
    UpdateWindow(g_hwnd);

    // ---- Initialize Script Bridge ----
    ScriptBridge bridge(interp, appRoot);
    g_bridge = &bridge;
    bridge.init();

    sysLog("Script engine initialized.");

    // Caret blink timer
    SetTimer(g_hwnd, 1, 500, [](HWND hwnd, UINT, UINT_PTR, DWORD) {
        if (focusedElement) RequestRedraw(hwnd);
    });

    // Async task timer (for setTimeout/setInterval/promises) and Animation loop
    SetTimer(g_hwnd, 2, 16, [](HWND hwnd, UINT, UINT_PTR, DWORD) {
        if (g_interp) g_interp->tick();
        if (appRoot && appRoot->HasActiveTransitions()) RequestRedraw(hwnd);
    });

    // Execute Scripts
#ifdef IS_AOT_COMPILED
    if (AOT_GLOBAL_SCRIPTS && AOT_GLOBAL_SCRIPTS[0] != '\0') {
        sysLog("Executing AOT scripts...");
        interp.exec(AOT_GLOBAL_SCRIPTS);
    }
#else
    if (!DOMParser::globalScripts.empty()) {
        sysLog("Executing scripts...");
        interp.exec(DOMParser::globalScripts);
    }
#endif

    // Dispatch 'load' event to global listeners (and inline onLoad trigger automatically)
    if (g_bridge) {
        auto loadEvent = Value::Object();
        loadEvent->setProperty("type", Value::Str("load"));
        g_bridge->dispatchScriptEvent(appRoot.get(), "load", loadEvent);
    }

    sysLog("App ready.");

    // Trim working set to immediately drop the 40-50MB OpenGL driver initialization overhead
    SetProcessWorkingSetSize(GetCurrentProcess(), (SIZE_T)-1, (SIZE_T)-1);

    // ---- Message Loop ----
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    g_interp = nullptr;
    g_bridge = nullptr;
    sysLog("DOM Environment Shutting Down.");
    return 0;
}
