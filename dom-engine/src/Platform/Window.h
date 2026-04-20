#pragma once
#include <windows.h>
#include <dwmapi.h>
#include "../Core/Value.h"
#include "COMBridge.h"

extern HWND g_hwnd;

inline HHOOK g_keyboardHook = NULL;
inline ValuePtr g_keyboardHookCallback = nullptr;
inline Interpreter* g_keyboardHookInterp = nullptr;

extern void sysLog(const std::string& msg);

LRESULT CALLBACK LowLevelKeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    __try {
        if (nCode == HC_ACTION && g_keyboardHookCallback && g_keyboardHookInterp) {
            KBDLLHOOKSTRUCT* pKeyBoard = (KBDLLHOOKSTRUCT*)lParam;
            bool isDown = (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN);
            
            auto eventObj = Value::Object();
            eventObj->setProperty("vkCode", Value::Num(pKeyBoard->vkCode));
            eventObj->setProperty("scanCode", Value::Num(pKeyBoard->scanCode));
            eventObj->setProperty("isDown", Value::Bool(isDown));
            eventObj->setProperty("flags", Value::Num(pKeyBoard->flags));
            
            auto result = g_keyboardHookInterp->callFunction(g_keyboardHookCallback, { eventObj });
            
            if(result && result->type == ValueType::Boolean && result->boolean) {
                return 1; // Blocked
            }
        }
    } __except(EXCEPTION_EXECUTE_HANDLER) {
        sysLog("[CRITICAL HOOK ERROR] SEH Exception 0xC0000005 thrown inside LowLevelKeyboardProc!");
    }
    return CallNextHookEx(g_keyboardHook, nCode, wParam, lParam);
}

struct DynAPI {
    decltype(&BitBlt) _BitBlt = nullptr;
    decltype(&CreateCompatibleDC) _CreateCompatibleDC = nullptr;
    decltype(&GetDC) _GetDC = nullptr;
    decltype(&FindWindowA) _FindWindowA = nullptr;
    decltype(&GetWindowLongA) _GetWindowLongA = nullptr;
    decltype(&SetWindowLongA) _SetWindowLongA = nullptr;
    
    typedef HRESULT(WINAPI* DwmExtendArea_t)(HWND, const MARGINS*);
    typedef HRESULT(WINAPI* DwmSetAttr_t)(HWND, DWORD, LPCVOID, DWORD);
    DwmExtendArea_t _DwmExtendFrameIntoClientArea = nullptr;
    DwmSetAttr_t _DwmSetWindowAttribute = nullptr;

    void init();
};
extern DynAPI dynAPI;

class WindowAPI {
public:
    static void BindToEnv(Interpreter& interp) {
        auto sysObj = Value::Object();

        sysObj->setProperty("isCompiled", Value::Native([](std::vector<ValuePtr>, ValuePtr) -> ValuePtr {
#ifdef COMPILED_DOM_PROJECT
            return Value::Bool(true);
#else
            return Value::Bool(false);
#endif
        }));

        sysObj->setProperty("setTimeout", Value::Native([&interp](std::vector<ValuePtr> args, ValuePtr) -> ValuePtr {
            if (args.size() < 2) return Value::Num(0);
            std::vector<ValuePtr> callArgs;
            for (size_t i = 2; i < args.size(); i++) callArgs.push_back(args[i]);
            int id = interp.addTimer(args[0], callArgs, (int)args[1]->toNumber(), false);
            return Value::Num(id);
        }));

        sysObj->setProperty("setInterval", Value::Native([&interp](std::vector<ValuePtr> args, ValuePtr) -> ValuePtr {
            if (args.size() < 2) return Value::Num(0);
            std::vector<ValuePtr> callArgs;
            for (size_t i = 2; i < args.size(); i++) callArgs.push_back(args[i]);
            int id = interp.addTimer(args[0], callArgs, (int)args[1]->toNumber(), true);
            return Value::Num(id);
        }));

        sysObj->setProperty("clearTimeout", Value::Native([&interp](std::vector<ValuePtr> args, ValuePtr) -> ValuePtr {
            if (args.empty()) return Value::Undefined();
            interp.clearTimer((int)args[0]->toNumber());
            return Value::Undefined();
        }));

        sysObj->setProperty("clearInterval", Value::Native([&interp](std::vector<ValuePtr> args, ValuePtr) -> ValuePtr {
            if (args.empty()) return Value::Undefined();
            interp.clearTimer((int)args[0]->toNumber());
            return Value::Undefined();
        }));

        // ---- sys.log ----
        sysObj->setProperty("log", Value::Native([&interp](std::vector<ValuePtr> args, ValuePtr) -> ValuePtr {
            extern bool g_sysLogEnabled;
            if (!g_sysLogEnabled) return Value::Undefined();
            std::string msg;
            for (size_t i = 0; i < args.size(); i++) {
                if (i > 0) msg += " ";
                msg += args[i]->toString();
            }
            // Uses the interpreter's log callback (which writes to stdout, OutputDebugString, and file)
            auto logCb = interp.getGlobalEnv()->get("__logCallback__");
            // Just use OutputDebugString directly + stdout
            printf("[DOM] %s\n", msg.c_str());
            OutputDebugStringA(("[DOM] " + msg + "\n").c_str());
            
            // Append to sys_log.txt
            FILE* log = fopen("sys_log.txt", "a");
            if (log) { fprintf(log, "[DOM] %s\n", msg.c_str()); fclose(log); }
            return Value::Undefined();
        }));

        sysObj->setProperty("setLogEnabled", Value::Native([](std::vector<ValuePtr> args, ValuePtr) -> ValuePtr {
            extern bool g_sysLogEnabled;
            if (!args.empty()) g_sysLogEnabled = args[0]->isTruthy();
            return Value::Bool(g_sysLogEnabled);
        }));

        // ---- sys.time ----
        sysObj->setProperty("time", Value::Native([](std::vector<ValuePtr>, ValuePtr) -> ValuePtr {
            return Value::Num((double)GetTickCount64());
        }));
        // ---- sys.sleep ----
        sysObj->setProperty("sleep", Value::Native([](std::vector<ValuePtr> args, ValuePtr) -> ValuePtr {
            if (args.empty()) return Value::Undefined();
            Sleep((DWORD)args[0]->toNumber());
            return Value::Undefined();
        }));

        // ---- sys.exec ----
        sysObj->setProperty("exec", Value::Native([](std::vector<ValuePtr> args, ValuePtr) -> ValuePtr {
            if (args.empty()) return Value::Num(0);
            std::string cmd = args[0]->toString();
            bool hidden = args.size() > 1 && args[1]->isTruthy();
            STARTUPINFOA si = { sizeof(si) };
            if (hidden) { si.dwFlags = STARTF_USESHOWWINDOW; si.wShowWindow = SW_HIDE; }
            PROCESS_INFORMATION pi;
            if (CreateProcessA(NULL, (LPSTR)cmd.c_str(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
                CloseHandle(pi.hThread);
                CloseHandle(pi.hProcess);
                return Value::Num(pi.dwProcessId);
            }
            return Value::Num(0);
        }));
        
        // ---- sys.readText / sys.writeText ----
        sysObj->setProperty("readText", Value::Native([](std::vector<ValuePtr> args, ValuePtr) -> ValuePtr {
            if (args.empty()) return Value::Null();
            FILE* f = fopen(args[0]->toString().c_str(), "rb");
            if (!f) return Value::Null();
            fseek(f, 0, SEEK_END); long s = ftell(f); fseek(f, 0, SEEK_SET);
            std::vector<char> buf(s + 1, 0);
            fread(buf.data(), 1, s, f); fclose(f);
            return Value::Str(buf.data());
        }));
        
        sysObj->setProperty("writeText", Value::Native([](std::vector<ValuePtr> args, ValuePtr) -> ValuePtr {
            if (args.size() < 2) return Value::Num(0);
            FILE* f = fopen(args[0]->toString().c_str(), "wb");
            if (!f) return Value::Num(0);
            std::string d = args[1]->toString();
            fwrite(d.data(), 1, d.size(), f); fclose(f);
            return Value::Num(1);
        }));

        // ---- sys.window ----
        auto windowObj = Value::Object();

        windowObj->setProperty("resize", Value::Native([](std::vector<ValuePtr> args, ValuePtr) -> ValuePtr {
            if (args.size() < 2) return Value::Undefined();
            int w = (int)args[0]->toNumber();
            int h = (int)args[1]->toNumber();
            SetWindowPos(g_hwnd, NULL, 0, 0, w, h, SWP_NOMOVE | SWP_NOZORDER);
            InvalidateRect(g_hwnd, NULL, FALSE);
            return Value::Undefined();
        }));

        windowObj->setProperty("move", Value::Native([](std::vector<ValuePtr> args, ValuePtr) -> ValuePtr {
            if (args.size() < 2) return Value::Undefined();
            int x = (int)args[0]->toNumber();
            int y = (int)args[1]->toNumber();
            SetWindowPos(g_hwnd, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
            return Value::Undefined();
        }));

        windowObj->setProperty("center", Value::Native([](std::vector<ValuePtr> args, ValuePtr) -> ValuePtr {
            RECT rc;
            GetWindowRect(g_hwnd, &rc);
            int w = rc.right - rc.left;
            int h = rc.bottom - rc.top;

            // Get the monitor this window is on
            HMONITOR hMon = MonitorFromWindow(g_hwnd, MONITOR_DEFAULTTONEAREST);
            MONITORINFO mi = {};
            mi.cbSize = sizeof(mi);
            GetMonitorInfoA(hMon, &mi);

            int scrW = mi.rcWork.right - mi.rcWork.left;
            int scrH = mi.rcWork.bottom - mi.rcWork.top;
            int x = mi.rcWork.left + (scrW - w) / 2;
            int y = mi.rcWork.top + (scrH - h) / 2;
            SetWindowPos(g_hwnd, NULL, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER);
            return Value::Undefined();
        }));

        windowObj->setProperty("setTitle", Value::Native([](std::vector<ValuePtr> args, ValuePtr) -> ValuePtr {
            if (!args.empty()) SetWindowTextA(g_hwnd, args[0]->toString().c_str());
            return Value::Undefined();
        }));

        windowObj->setProperty("getSize", Value::Native([](std::vector<ValuePtr>, ValuePtr) -> ValuePtr {
            RECT rc;
            GetClientRect(g_hwnd, &rc);
            auto obj = Value::Object();
            obj->setProperty("width", Value::Num((double)rc.right));
            obj->setProperty("height", Value::Num((double)rc.bottom));
            return obj;
        }));

        windowObj->setProperty("getPosition", Value::Native([](std::vector<ValuePtr>, ValuePtr) -> ValuePtr {
            RECT rc;
            GetWindowRect(g_hwnd, &rc);
            auto obj = Value::Object();
            obj->setProperty("x", Value::Num((double)rc.left));
            obj->setProperty("y", Value::Num((double)rc.top));
            return obj;
        }));

        windowObj->setProperty("minimize", Value::Native([](std::vector<ValuePtr>, ValuePtr) -> ValuePtr {
            ShowWindow(g_hwnd, SW_MINIMIZE);
            return Value::Undefined();
        }));

        windowObj->setProperty("maximize", Value::Native([](std::vector<ValuePtr>, ValuePtr) -> ValuePtr {
            ShowWindow(g_hwnd, SW_MAXIMIZE);
            return Value::Undefined();
        }));

        windowObj->setProperty("isMaximized", Value::Native([](std::vector<ValuePtr>, ValuePtr) -> ValuePtr {
            return Value::Bool(IsZoomed(g_hwnd));
        }));

        windowObj->setProperty("find", Value::Native([](std::vector<ValuePtr> args, ValuePtr) -> ValuePtr {
            if (args.empty()) return Value::Null();
            std::string target = args[0]->toString();
            HWND found = FindWindowA(NULL, target.c_str());
            return found ? Value::Num((double)(uintptr_t)found) : Value::Null();
        }));

        windowObj->setProperty("focus", Value::Native([](std::vector<ValuePtr> args, ValuePtr) -> ValuePtr {
            if (args.empty()) return Value::Undefined();
            HWND targetWnd = NULL;
            if (args[0]->type == ValueType::Number) {
                targetWnd = (HWND)(uintptr_t)args[0]->toNumber();
            } else {
                targetWnd = FindWindowA(NULL, args[0]->toString().c_str());
            }

            if (targetWnd) {
                ShowWindow(targetWnd, SW_RESTORE);
                SetForegroundWindow(targetWnd);
                return Value::Bool(true);
            }
            return Value::Bool(false);
        }));

        windowObj->setProperty("setIcon", Value::Native([](std::vector<ValuePtr> args, ValuePtr) -> ValuePtr {
            if (args.empty()) return Value::Undefined();
            std::string path = args[0]->toString();
            HICON hIcon = (HICON)LoadImageA(NULL, path.c_str(), IMAGE_ICON, 0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE);
            if (hIcon) {
                SendMessage(g_hwnd, WM_SETICON, ICON_SMALL, (LPARAM)hIcon);
                SendMessage(g_hwnd, WM_SETICON, ICON_BIG, (LPARAM)hIcon);
            }
            return Value::Undefined();
        }));

        windowObj->setProperty("restore", Value::Native([](std::vector<ValuePtr>, ValuePtr) -> ValuePtr {
            ShowWindow(g_hwnd, SW_RESTORE);
            return Value::Undefined();
        }));

        windowObj->setProperty("showNotification", Value::Native([](std::vector<ValuePtr> args, ValuePtr) -> ValuePtr {
            if (args.size() < 2) return Value::Undefined();
            std::string title = args[0]->toString();
            std::string msg = args[1]->toString();
            NOTIFYICONDATAA nid = {};
            nid.cbSize = sizeof(nid);
            nid.hWnd = g_hwnd;
            nid.uID = 1001;
            nid.uFlags = NIF_INFO | NIF_ICON;
            nid.hIcon = LoadIcon(NULL, IDI_INFORMATION);
            strncpy_s(nid.szInfoTitle, title.c_str(), sizeof(nid.szInfoTitle) - 1);
            strncpy_s(nid.szInfo, msg.c_str(), sizeof(nid.szInfo) - 1);
            nid.dwInfoFlags = NIIF_INFO;
            
            if (!Shell_NotifyIconA(NIM_MODIFY, &nid)) {
                Shell_NotifyIconA(NIM_ADD, &nid);
            }
            return Value::Bool(true);
        }));

        windowObj->setProperty("setFullscreen", Value::Native([](std::vector<ValuePtr> args, ValuePtr) -> ValuePtr {
            bool enter = true;
            if(!args.empty()) enter = args[0]->isTruthy();
            static DWORD savedExStyle;
            static DWORD savedStyle;
            static RECT savedRect;
            static bool isFullscreen = false;

            if (enter && !isFullscreen) {
                savedExStyle = dynAPI._GetWindowLongA(g_hwnd, GWL_EXSTYLE);
                savedStyle = dynAPI._GetWindowLongA(g_hwnd, GWL_STYLE);
                GetWindowRect(g_hwnd, &savedRect);

                dynAPI._SetWindowLongA(g_hwnd, GWL_STYLE, savedStyle & ~(WS_CAPTION | WS_THICKFRAME));
                dynAPI._SetWindowLongA(g_hwnd, GWL_EXSTYLE, savedExStyle & ~(WS_EX_DLGMODALFRAME | WS_EX_WINDOWEDGE | WS_EX_CLIENTEDGE | WS_EX_STATICEDGE));

                HMONITOR hMon = MonitorFromWindow(g_hwnd, MONITOR_DEFAULTTONEAREST);
                MONITORINFO mi = { sizeof(mi) };
                if (GetMonitorInfoA(hMon, &mi)) {
                    SetWindowPos(g_hwnd, HWND_TOP, 
                        mi.rcMonitor.left, mi.rcMonitor.top,
                        mi.rcMonitor.right - mi.rcMonitor.left,
                        mi.rcMonitor.bottom - mi.rcMonitor.top,
                        SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
                }
                isFullscreen = true;
            } else if (!enter && isFullscreen) {
                dynAPI._SetWindowLongA(g_hwnd, GWL_STYLE, savedStyle);
                dynAPI._SetWindowLongA(g_hwnd, GWL_EXSTYLE, savedExStyle);
                SetWindowPos(g_hwnd, NULL,
                    savedRect.left, savedRect.top,
                    savedRect.right - savedRect.left,
                    savedRect.bottom - savedRect.top,
                    SWP_NOZORDER | SWP_NOOWNERZORDER | SWP_FRAMECHANGED);
                isFullscreen = false;
            }
            return Value::Undefined();
        }));

        windowObj->setProperty("hide", Value::Native([](std::vector<ValuePtr>, ValuePtr) -> ValuePtr {
            ShowWindow(g_hwnd, SW_HIDE);
            return Value::Undefined();
        }));

        windowObj->setProperty("show", Value::Native([](std::vector<ValuePtr>, ValuePtr) -> ValuePtr {
            ShowWindow(g_hwnd, SW_SHOW);
            return Value::Undefined();
        }));

        windowObj->setProperty("getActiveProcessName", Value::Native([](std::vector<ValuePtr>, ValuePtr) -> ValuePtr {
            //sysLog("[SYS] getActiveProcessName called.");
            HWND fg = GetForegroundWindow();
            if(!fg) { /*sysLog("[SYS] getActiveProcessName: No fg window.");*/ return Value::Str(""); }
            
            DWORD pid = 0;
            GetWindowThreadProcessId(fg, &pid);
            if(!pid) { /*sysLog("[SYS] getActiveProcessName: Thread ID failed.");*/ return Value::Str(""); }
            
            //sysLog("[SYS] getActiveProcessName: Opening process " + std::to_string(pid));
            HANDLE hProc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
            if(!hProc) { /*sysLog("[SYS] getActiveProcessName: OpenProcess failed. Access Denied?");*/ return Value::Str(""); }
            
            char buf[MAX_PATH] = {0};
            DWORD size = MAX_PATH;
            typedef BOOL (WINAPI *QueryFullProcessImageNameA_t)(HANDLE, DWORD, LPSTR, PDWORD);
            HMODULE hKernel = GetModuleHandleA("kernel32.dll");
            auto queryFunc = (QueryFullProcessImageNameA_t)GetProcAddress(hKernel, "QueryFullProcessImageNameA");
            
            std::string ret = "";
            //sysLog("[SYS] getActiveProcessName: Querying name...");
            if(queryFunc && queryFunc(hProc, 0, buf, &size)) {
                std::string fullPath = buf;
                size_t pos = fullPath.find_last_of("\\/");
                if(pos != std::string::npos) ret = fullPath.substr(pos + 1);
                else ret = fullPath;
                //sysLog("[SYS] getActiveProcessName: Found -> " + ret);
            } else {
                //sysLog("[SYS] getActiveProcessName: Query returned FALSE.");
            }
            CloseHandle(hProc);
            return Value::Str(ret);
        }));

        windowObj->setProperty("registerHotkey", Value::Native([](std::vector<ValuePtr> args, ValuePtr) -> ValuePtr {
            if(args.size() < 2) return Value::Bool(false);
            int id = (int)args[0]->toNumber();
            int modifiers = 0;
            int vk = 0;
            
            if (args[1]->type == ValueType::String) {
                std::string k = args[1]->toString();
                std::transform(k.begin(), k.end(), k.begin(), ::toupper);
                if (k.find("CTRL") != std::string::npos) modifiers |= MOD_CONTROL;
                if (k.find("SHIFT") != std::string::npos) modifiers |= MOD_SHIFT;
                if (k.find("ALT") != std::string::npos) modifiers |= MOD_ALT;
                if (k.find("WIN") != std::string::npos) modifiers |= MOD_WIN;
                
                size_t plus = k.find_last_of('+');
                std::string keyName = (plus != std::string::npos) ? k.substr(plus + 1) : k;
                while(!keyName.empty() && isspace(keyName.front())) keyName.erase(keyName.begin());
                while(!keyName.empty() && isspace(keyName.back())) keyName.pop_back();
                
                if (keyName.length() == 1) vk = keyName[0];
                else if (keyName == "SPACE") vk = VK_SPACE;
                else if (keyName == "ENTER") vk = VK_RETURN;
                else if (keyName == "TAB") vk = VK_TAB;
                else if (keyName == "ESC" || keyName == "ESCAPE") vk = VK_ESCAPE;
                else if (keyName[0] == 'F' && keyName.length() > 1 && isdigit(keyName[1])) {
                    vk = VK_F1 + (int)strtol(keyName.substr(1).c_str(), nullptr, 10) - 1;
                }
            } else if (args.size() >= 3) {
                modifiers = (int)args[1]->toNumber();
                vk = (int)args[2]->toNumber();
            }
            return Value::Bool(RegisterHotKey(g_hwnd, id, modifiers, vk));
        }));

        windowObj->setProperty("unregisterHotkey", Value::Native([](std::vector<ValuePtr> args, ValuePtr) -> ValuePtr {
            if(args.empty()) return Value::Bool(false);
            return Value::Bool(UnregisterHotKey(g_hwnd, (int)args[0]->toNumber()));
        }));

        windowObj->setProperty("close", Value::Native([](std::vector<ValuePtr>, ValuePtr) -> ValuePtr {
            PostMessage(g_hwnd, WM_CLOSE, 0, 0);
            return Value::Undefined();
        }));

        windowObj->setProperty("setOpacity", Value::Native([](std::vector<ValuePtr> args, ValuePtr) -> ValuePtr {
            if (args.empty()) return Value::Undefined();
            int alpha = (int)(args[0]->toNumber() * 255);
            dynAPI._SetWindowLongA(g_hwnd, GWL_EXSTYLE, dynAPI._GetWindowLongA(g_hwnd, GWL_EXSTYLE) | WS_EX_LAYERED);
            SetLayeredWindowAttributes(g_hwnd, 0, (BYTE)alpha, LWA_ALPHA);
            return Value::Undefined();
        }));

        windowObj->setProperty("setAlwaysOnTop", Value::Native([](std::vector<ValuePtr> args, ValuePtr) -> ValuePtr {
            bool top = args.empty() || args[0]->isTruthy();
            SetWindowPos(g_hwnd, top ? HWND_TOPMOST : HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
            return Value::Undefined();
        }));

        sysObj->setProperty("window", windowObj);

        // ---- sys.screen ----
        auto screenObj = Value::Object();

        screenObj->setProperty("getInfo", Value::Native([](std::vector<ValuePtr>, ValuePtr) -> ValuePtr {
            auto info = Value::Object();
            info->setProperty("width", Value::Num((double)GetSystemMetrics(SM_CXSCREEN)));
            info->setProperty("height", Value::Num((double)GetSystemMetrics(SM_CYSCREEN)));
            info->setProperty("monitors", Value::Num((double)GetSystemMetrics(SM_CMONITORS)));

            // Work area (excluding taskbar)
            RECT work;
            SystemParametersInfo(SPI_GETWORKAREA, 0, &work, 0);
            auto workArea = Value::Object();
            workArea->setProperty("x", Value::Num((double)work.left));
            workArea->setProperty("y", Value::Num((double)work.top));
            workArea->setProperty("width", Value::Num((double)(work.right - work.left)));
            workArea->setProperty("height", Value::Num((double)(work.bottom - work.top)));
            info->setProperty("workArea", workArea);

            return info;
        }));

        screenObj->setProperty("getDPI", Value::Native([](std::vector<ValuePtr>, ValuePtr) -> ValuePtr {
            HDC hdc = dynAPI._GetDC(NULL);
            int dpi = GetDeviceCaps(hdc, LOGPIXELSX);
            ReleaseDC(NULL, hdc);
            return Value::Num((double)dpi);
        }));

        screenObj->setProperty("getMousePosition", Value::Native([](std::vector<ValuePtr>, ValuePtr) -> ValuePtr {
            POINT pt;
            GetCursorPos(&pt);
            auto pos = Value::Object();
            pos->setProperty("x", Value::Num((double)pt.x));
            pos->setProperty("y", Value::Num((double)pt.y));
            return pos;
        }));

        screenObj->setProperty("getMonitorAt", Value::Native([](std::vector<ValuePtr> args, ValuePtr) -> ValuePtr {
            if (args.size() < 2) return Value::Null();
            POINT pt;
            pt.x = (LONG)args[0]->toNumber();
            pt.y = (LONG)args[1]->toNumber();
            HMONITOR hMon = MonitorFromPoint(pt, MONITOR_DEFAULTTONULL);
            if (!hMon) return Value::Null();
            
            MONITORINFOEXA mi;
            mi.cbSize = sizeof(mi);
            if (GetMonitorInfoA(hMon, (MONITORINFO*)&mi)) {
                auto obj = Value::Object();
                obj->setProperty("name", Value::Str(mi.szDevice));
                obj->setProperty("x", Value::Num((double)mi.rcMonitor.left));
                obj->setProperty("y", Value::Num((double)mi.rcMonitor.top));
                obj->setProperty("width", Value::Num((double)(mi.rcMonitor.right - mi.rcMonitor.left)));
                obj->setProperty("height", Value::Num((double)(mi.rcMonitor.bottom - mi.rcMonitor.top)));
                obj->setProperty("isPrimary", Value::Bool((mi.dwFlags & MONITORINFOF_PRIMARY) != 0));
                return obj;
            }
            return Value::Null();
        }));

        sysObj->setProperty("screen", screenObj);

        // ---- sys.keyboard ----
        auto keyboardObj = Value::Object();
        Interpreter* pInterp = &interp;
        keyboardObj->setProperty("hook", Value::Native([pInterp](std::vector<ValuePtr> args, ValuePtr) -> ValuePtr {
            sysLog("[SYS] sys.keyboard.hook initiated...");
            if(args.empty()) return Value::Bool(false);
            
            sysLog("[SYS] Storing callback...");
            g_keyboardHookCallback = args[0];
            g_keyboardHookInterp = pInterp;
            
            if(!g_keyboardHook) {
                sysLog("[SYS] Installing SetWindowsHookEx...");
                __try {
                    g_keyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, LowLevelKeyboardProc, NULL, 0);
                    sysLog(std::string("[SYS] SetWindowsHookEx returned: ") + (g_keyboardHook ? "VALID" : "NULL"));
                } __except(EXCEPTION_EXECUTE_HANDLER) {
                    sysLog("[SYS] FATAL CRASH INSIDE SetWindowsHookEx NATIVE CALL!");
                }
            } else {
                sysLog("[SYS] Hook already installed.");
            }
            return Value::Bool(g_keyboardHook != NULL);
        }));
        keyboardObj->setProperty("unhook", Value::Native([](std::vector<ValuePtr>, ValuePtr) -> ValuePtr {
            if(g_keyboardHook) {
                UnhookWindowsHookEx(g_keyboardHook);
                g_keyboardHook = NULL;
            }
            g_keyboardHookCallback = nullptr;
            g_keyboardHookInterp = nullptr;
            return Value::Undefined();
        }));

        keyboardObj->setProperty("sendText", Value::Native([](std::vector<ValuePtr> args, ValuePtr) -> ValuePtr {
            if (args.empty()) return Value::Undefined();
            std::string text = args[0]->toString();
            int wlen = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, NULL, 0);
            if (wlen > 0) {
                std::wstring wtext(wlen, 0);
                MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, &wtext[0], wlen);
                
                std::vector<INPUT> inputs;
                for (size_t i = 0; i < wlen - 1; i++) {
                    INPUT in = {0};
                    in.type = INPUT_KEYBOARD;
                    in.ki.wScan = wtext[i];
                    in.ki.dwFlags = KEYEVENTF_UNICODE;
                    inputs.push_back(in);
                    
                    INPUT inUp = {0};
                    inUp.type = INPUT_KEYBOARD;
                    inUp.ki.wScan = wtext[i];
                    inUp.ki.dwFlags = KEYEVENTF_UNICODE | KEYEVENTF_KEYUP;
                    inputs.push_back(inUp);
                }
                if (!inputs.empty()) SendInput((UINT)inputs.size(), inputs.data(), sizeof(INPUT));
            }
            return Value::Bool(true);
        }));

        sysObj->setProperty("keyboard", keyboardObj);

        // ---- sys.clipboard ----
        auto clipObj = Value::Object();
        clipObj->setProperty("getText", Value::Native([](std::vector<ValuePtr>, ValuePtr) -> ValuePtr {
            if (!OpenClipboard(NULL)) return Value::Null();
            ValuePtr result = Value::Null();
            HANDLE hData = GetClipboardData(CF_TEXT);
            if (hData) {
                char* text = static_cast<char*>(GlobalLock(hData));
                if (text) {
                    result = Value::Str(text);
                    GlobalUnlock(hData);
                }
            }
            CloseClipboard();
            return result;
        }));
        clipObj->setProperty("setText", Value::Native([](std::vector<ValuePtr> args, ValuePtr) -> ValuePtr {
            if (args.empty()) return Value::Bool(false);
            std::string text = args[0]->toString();
            if (!OpenClipboard(NULL)) return Value::Bool(false);
            EmptyClipboard();
            HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, text.size() + 1);
            if (hMem) {
                memcpy(GlobalLock(hMem), text.c_str(), text.size() + 1);
                GlobalUnlock(hMem);
                SetClipboardData(CF_TEXT, hMem);
            }
            CloseClipboard();
            return Value::Bool(true);
        }));
        clipObj->setProperty("getFormat", Value::Native([](std::vector<ValuePtr> args, ValuePtr) -> ValuePtr {
            if (args.empty()) return Value::Null();
            UINT format = args[0]->type == ValueType::String ? RegisterClipboardFormatA(args[0]->toString().c_str()) : (UINT)args[0]->toNumber();
            if (!OpenClipboard(NULL)) return Value::Null();
            ValuePtr result = Value::Null();
            HANDLE hData = GetClipboardData(format);
            if (hData) {
                SIZE_T size = GlobalSize(hData);
                void* data = GlobalLock(hData);
                if (data && size > 0) {
                    auto arr = Value::Array();
                    unsigned char* bytes = (unsigned char*)data;
                    for(SIZE_T i = 0; i < size; i++) arr->elements.push_back(Value::Num(bytes[i]));
                    result = arr;
                    GlobalUnlock(hData);
                }
            }
            CloseClipboard();
            return result;
        }));
        clipObj->setProperty("setFormat", Value::Native([](std::vector<ValuePtr> args, ValuePtr) -> ValuePtr {
            if (args.size() < 2) return Value::Bool(false);
            UINT format = args[0]->type == ValueType::String ? RegisterClipboardFormatA(args[0]->toString().c_str()) : (UINT)args[0]->toNumber();
            if (!OpenClipboard(NULL)) return Value::Bool(false);
            HGLOBAL hMem = NULL;
            if (args[1]->type == ValueType::String) {
                std::string txt = args[1]->toString();
                hMem = GlobalAlloc(GMEM_MOVEABLE, txt.size() + 1);
                if (hMem) {
                    memcpy(GlobalLock(hMem), txt.c_str(), txt.size() + 1);
                    GlobalUnlock(hMem);
                }
            } else if (args[1]->type == ValueType::Array) {
                size_t sz = args[1]->elements.size();
                hMem = GlobalAlloc(GMEM_MOVEABLE, sz);
                if (hMem) {
                    unsigned char* ptr = (unsigned char*)GlobalLock(hMem);
                    for(size_t i=0; i<sz; i++) ptr[i] = (unsigned char)args[1]->elements[i]->toNumber();
                    GlobalUnlock(hMem);
                }
            }
            bool success = false;
            if (hMem) {
                if (SetClipboardData(format, hMem)) success = true;
            }
            CloseClipboard();
            return Value::Bool(success);
        }));
        clipObj->setProperty("clear", Value::Native([](std::vector<ValuePtr>, ValuePtr) -> ValuePtr {
            if (!OpenClipboard(NULL)) return Value::Bool(false);
            EmptyClipboard();
            CloseClipboard();
            return Value::Bool(true);
        }));
        sysObj->setProperty("clipboard", clipObj);

        // ---- sys.dllCall ----
        sysObj->setProperty("dllCall", Value::Native([](std::vector<ValuePtr> args, ValuePtr) -> ValuePtr {
            if (args.size() < 3) return Value::Null();
            std::string dllName = args[0]->toString();
            std::string funcName = args[1]->toString();
            std::vector<ValuePtr> callArgs;
            if (args[2]->type == ValueType::Array) callArgs = args[2]->elements;
            return DLLBridge::Call(dllName, funcName, callArgs);
        }));

        // ---- sys.dllCallString (returns string from DLL instead of number) ----
        sysObj->setProperty("dllCallString", Value::Native([](std::vector<ValuePtr> args, ValuePtr) -> ValuePtr {
            if (args.size() < 3) return Value::Null();
            std::string dllName = args[0]->toString();
            std::string funcName = args[1]->toString();
            std::vector<ValuePtr> callArgs;
            if (args[2]->type == ValueType::Array) callArgs = args[2]->elements;
            return DLLBridge::CallString(dllName, funcName, callArgs);
        }));

        // ---- sys.screenshot(filepath, x?, y?, w?, h?) ----
        sysObj->setProperty("screenshot", Value::Native([](std::vector<ValuePtr> args, ValuePtr) -> ValuePtr {
            std::string path = args.size() > 0 ? args[0]->toString() : "screenshot.bmp";
            
            RECT clientRect;
            GetClientRect(g_hwnd, &clientRect);
            int sx = args.size() > 1 ? (int)args[1]->toNumber() : 0;
            int sy = args.size() > 2 ? (int)args[2]->toNumber() : 0;
            int sw = args.size() > 3 ? (int)args[3]->toNumber() : (clientRect.right - clientRect.left);
            int sh = args.size() > 4 ? (int)args[4]->toNumber() : (clientRect.bottom - clientRect.top);

            HDC hdcWin = GetDC(g_hwnd);
            HDC hdcMem = CreateCompatibleDC(hdcWin);
            HBITMAP hbm = CreateCompatibleBitmap(hdcWin, sw, sh);
            SelectObject(hdcMem, hbm);
            BitBlt(hdcMem, 0, 0, sw, sh, hdcWin, sx, sy, SRCCOPY);

            // Save as BMP
            BITMAPINFOHEADER bi = {};
            bi.biSize = sizeof(bi);
            bi.biWidth = sw;
            bi.biHeight = -sh; // top-down
            bi.biPlanes = 1;
            bi.biBitCount = 24;
            bi.biCompression = BI_RGB;
            int rowBytes = ((sw * 3 + 3) & ~3);
            bi.biSizeImage = rowBytes * sh;

            std::vector<unsigned char> pixels(bi.biSizeImage);
            GetDIBits(hdcMem, hbm, 0, sh, pixels.data(), (BITMAPINFO*)&bi, DIB_RGB_COLORS);

            BITMAPFILEHEADER bf = {};
            bf.bfType = 0x4D42;
            bf.bfOffBits = sizeof(bf) + sizeof(bi);
            bf.bfSize = bf.bfOffBits + bi.biSizeImage;

            FILE* f = fopen(path.c_str(), "wb");
            if (f) {
                fwrite(&bf, sizeof(bf), 1, f);
                fwrite(&bi, sizeof(bi), 1, f);
                fwrite(pixels.data(), bi.biSizeImage, 1, f);
                fclose(f);
            }

            DeleteObject(hbm);
            DeleteDC(hdcMem);
            ReleaseDC(g_hwnd, hdcWin);
            return Value::Bool(f != nullptr);
        }));

        // ---- sys.getPixelColor(x?, y?) -> {r, g, b, hex} ----
        sysObj->setProperty("getPixelColor", Value::Native([](std::vector<ValuePtr> args, ValuePtr) -> ValuePtr {
            POINT pt;
            if (args.size() >= 2) {
                pt.x = (int)args[0]->toNumber();
                pt.y = (int)args[1]->toNumber();
            } else {
                GetCursorPos(&pt);
            }
            HDC hdcScreen = GetDC(NULL);
            COLORREF c = GetPixel(hdcScreen, pt.x, pt.y);
            ReleaseDC(NULL, hdcScreen);

            int r = GetRValue(c);
            int g = GetGValue(c);
            int b = GetBValue(c);
            char hex[16];
            snprintf(hex, sizeof(hex), "#%02X%02X%02X", r, g, b);

            auto result = Value::Object();
            result->setProperty("r", Value::Num(r));
            result->setProperty("g", Value::Num(g));
            result->setProperty("b", Value::Num(b));
            result->setProperty("hex", Value::Str(hex));
            result->setProperty("x", Value::Num(pt.x));
            result->setProperty("y", Value::Num(pt.y));
            return result;
        }));

        // ---- sys.com ---- (COM/OLE Automation bridge)
        auto comObj = Value::Object();

        // sys.com.create(progId) -> wrapped COM object
        comObj->setProperty("create", Value::Native([](std::vector<ValuePtr> args, ValuePtr) -> ValuePtr {
            if (args.empty()) return Value::Null();
            return COMBridge::CreateObject(args[0]->toString());
        }));

        // sys.com.getActive(progId) -> wrapped running COM object
        comObj->setProperty("getActive", Value::Native([](std::vector<ValuePtr> args, ValuePtr) -> ValuePtr {
            if (args.empty()) return Value::Null();
            return COMBridge::GetObject(args[0]->toString());
        }));

        // sys.com.releaseAll() -> cleanup all COM objects
        comObj->setProperty("releaseAll", Value::Native([](std::vector<ValuePtr> args, ValuePtr) -> ValuePtr {
            COMBridge::ReleaseAll();
            return Value::Undefined();
        }));

        sysObj->setProperty("com", comObj);

        // ---- sys.loadExtension ----
        sysObj->setProperty("loadExtension", Value::Native([&interp](std::vector<ValuePtr> args, ValuePtr) -> ValuePtr {
            if (args.empty()) return Value::Bool(false);
            std::string dllName = args[0]->toString();
            HMODULE hMod = LoadLibraryA(dllName.c_str());
            if (!hMod) return Value::Str("Error: Could not load extension DLL");

            FARPROC initProc = GetProcAddress(hMod, "NativeDOM_Init");
            if (!initProc) return Value::Str("Error: NativeDOM_Init export not found in DLL");

            extern struct NVGcontext* g_nvg;
            void* ctxArgs[3] = { g_hwnd, g_nvg, &interp };

            if (!DLLBridge::SafeInitExtension(initProc, ctxArgs)) {
                return Value::Str("Error: Exception caught during NativeDOM_Init execution");
            }

            return Value::Bool(true);
        }));

        // ---- sys.event (runtime event data placeholder) ----
        auto eventObj = Value::Object();
        eventObj->setProperty("keyCode", Value::Num(0));
        eventObj->setProperty("key", Value::Str(""));
        sysObj->setProperty("event", eventObj);

        // ---- sys.sendMessage / sys.postMessage (inter-window communication) ----
        sysObj->setProperty("sendMessage", Value::Native([](std::vector<ValuePtr> args, ValuePtr) -> ValuePtr {
            if (args.size() < 3) return Value::Num(0);
            HWND target = (HWND)(uintptr_t)(int64_t)args[0]->toNumber();
            UINT msg = (UINT)args[1]->toNumber();
            WPARAM wp = (WPARAM)(int64_t)args[2]->toNumber();
            LPARAM lp = args.size() > 3 ? (LPARAM)(int64_t)args[3]->toNumber() : 0;
            LRESULT result = SendMessage(target, msg, wp, lp);
            return Value::Num((double)result);
        }));

        sysObj->setProperty("findWindow", Value::Native([](std::vector<ValuePtr> args, ValuePtr) -> ValuePtr {
            if (args.empty()) return Value::Num(0);
            std::string title = args[0]->toString();
            HWND found = dynAPI._FindWindowA(NULL, title.c_str());
            return Value::Num((double)(uintptr_t)found);
        }));

        sysObj->setProperty("getHwnd", Value::Native([](std::vector<ValuePtr>, ValuePtr) -> ValuePtr {
            return Value::Num((double)(uintptr_t)g_hwnd);
        }));

        // ---- sys.loadScript ----
        sysObj->setProperty("loadScript", Value::Native([](std::vector<ValuePtr> args, ValuePtr) -> ValuePtr {
            if (args.size() >= 1) {
                std::string path = args[0]->toString();
                bool asNewProcess = args.size() >= 2 && args[1]->isTruthy();
                
                if (asNewProcess) {
                    char exe[MAX_PATH];
                    GetModuleFileNameA(NULL, exe, MAX_PATH);
                    std::string cmd = "\"" + std::string(exe) + "\" \"" + path + "\"";
                    
                    STARTUPINFOA si = { sizeof(si) };
                    PROCESS_INFORMATION pi;
                    if (CreateProcessA(NULL, (LPSTR)cmd.c_str(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
                        CloseHandle(pi.hProcess);
                        CloseHandle(pi.hThread);
                    }
                } else {
                    char* pathCopy = new char[path.size() + 1];
                    strcpy(pathCopy, path.c_str());
                    PostMessage(g_hwnd, WM_APP + 5, (WPARAM)pathCopy, 0);
                }
            }
            return Value::Undefined();
        }));

        interp.defineGlobal("sys", sysObj);
    }
};
