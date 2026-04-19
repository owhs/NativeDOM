#pragma once
#include <windows.h>
#include <winternl.h>
#include <string>

namespace Hooks {
    inline FARPROC _GetProcAddress(HMODULE hMod, const char* funcName) {
        if (!hMod) return nullptr;
        auto dosHeader = (PIMAGE_DOS_HEADER)hMod;
        auto ntHeaders = (PIMAGE_NT_HEADERS)((BYTE*)hMod + dosHeader->e_lfanew);
        auto exports = (PIMAGE_EXPORT_DIRECTORY)((BYTE*)hMod + ntHeaders->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress);

        auto names = (PDWORD)((BYTE*)hMod + exports->AddressOfNames);
        auto ordinals = (PWORD)((BYTE*)hMod + exports->AddressOfNameOrdinals);
        auto functions = (PDWORD)((BYTE*)hMod + exports->AddressOfFunctions);

        for (DWORD i = 0; i < exports->NumberOfNames; ++i) {
            char* name = (char*)((BYTE*)hMod + names[i]);
            if (strcmp(name, funcName) == 0) {
                return (FARPROC)((BYTE*)hMod + functions[ordinals[i]]);
            }
        }
        return nullptr;
    }

    inline HMODULE _GetModuleHandleA(const char* targetDll) {
#ifdef _WIN64
        auto peb = (PPEB)__readgsqword(0x60);
#else
        auto peb = (PPEB)__readfsdword(0x30);
#endif
        auto listEntry = peb->Ldr->InMemoryOrderModuleList.Flink;
        auto endEntry = &peb->Ldr->InMemoryOrderModuleList;

        std::string target = targetDll;
        for (auto& c : target) c = tolower(c);

        while (listEntry != endEntry) {
            auto ldrEntry = CONTAINING_RECORD(listEntry, LDR_DATA_TABLE_ENTRY, InMemoryOrderLinks);
            if (ldrEntry->FullDllName.Buffer) {
                std::wstring dllNameW = ldrEntry->FullDllName.Buffer;
                std::string dllName;
                dllName.reserve(dllNameW.size());
                for (auto wc : dllNameW) dllName += (char)wc;
                for (auto& c : dllName) c = tolower(c);

                if (dllName.find(target) != std::string::npos) {
                    return (HMODULE)ldrEntry->DllBase;
                }
            }
            listEntry = listEntry->Flink;
        }
        return nullptr;
    }

    inline HMODULE _LoadLibraryA(const char* dllName) {
        HMODULE hK32 = Hooks::_GetModuleHandleA("kernel32.dll");
        if (!hK32) return nullptr;
        auto pLoadLibraryA = (HMODULE(WINAPI*)(LPCSTR))Hooks::_GetProcAddress(hK32, "LoadLibraryA");
        if (pLoadLibraryA) return pLoadLibraryA(dllName);
        return nullptr;
    }

    inline LONG _RegOpenKeyExA(HKEY hKey, LPCSTR lpSubKey, DWORD ulOptions, REGSAM samDesired, PHKEY phkResult) {
        HMODULE hAdvapi32 = Hooks::_LoadLibraryA("advapi32.dll");
        if (!hAdvapi32) return ERROR_FILE_NOT_FOUND;
        auto pRegOpenKeyExA = (LONG(WINAPI*)(HKEY, LPCSTR, DWORD, REGSAM, PHKEY))Hooks::_GetProcAddress(hAdvapi32, "RegOpenKeyExA");
        if (pRegOpenKeyExA) return pRegOpenKeyExA(hKey, lpSubKey, ulOptions, samDesired, phkResult);
        return ERROR_FILE_NOT_FOUND;
    }

    inline LONG _RegQueryValueExA(HKEY hKey, LPCSTR lpValueName, LPDWORD lpReserved, LPDWORD lpType, LPBYTE lpData, LPDWORD lpcbData) {
        HMODULE hAdvapi32 = Hooks::_LoadLibraryA("advapi32.dll");
        if (!hAdvapi32) return ERROR_FILE_NOT_FOUND;
        auto pRegQueryValueExA = (LONG(WINAPI*)(HKEY, LPCSTR, LPDWORD, LPDWORD, LPBYTE, LPDWORD))Hooks::_GetProcAddress(hAdvapi32, "RegQueryValueExA");
        if (pRegQueryValueExA) return pRegQueryValueExA(hKey, lpValueName, lpReserved, lpType, lpData, lpcbData);
        return ERROR_FILE_NOT_FOUND;
    }

    inline LONG _RegCloseKey(HKEY hKey) {
        HMODULE hAdvapi32 = Hooks::_LoadLibraryA("advapi32.dll");
        if (!hAdvapi32) return ERROR_FILE_NOT_FOUND;
        auto pRegCloseKey = (LONG(WINAPI*)(HKEY))Hooks::_GetProcAddress(hAdvapi32, "RegCloseKey");
        if (pRegCloseKey) return pRegCloseKey(hKey);
        return ERROR_FILE_NOT_FOUND;
    }

    inline BOOL _CreateProcessA(LPCSTR lpApplicationName, LPSTR lpCommandLine, LPSECURITY_ATTRIBUTES lpProcessAttributes, LPSECURITY_ATTRIBUTES lpThreadAttributes, BOOL bInheritHandles, DWORD dwCreationFlags, LPVOID lpEnvironment, LPCSTR lpCurrentDirectory, LPSTARTUPINFOA lpStartupInfo, LPPROCESS_INFORMATION lpProcessInformation) {
        HMODULE hK32 = Hooks::_GetModuleHandleA("kernel32.dll");
        if (!hK32) return FALSE;
        auto pCreateProcessA = (BOOL(WINAPI*)(LPCSTR, LPSTR, LPSECURITY_ATTRIBUTES, LPSECURITY_ATTRIBUTES, BOOL, DWORD, LPVOID, LPCSTR, LPSTARTUPINFOA, LPPROCESS_INFORMATION))Hooks::_GetProcAddress(hK32, "CreateProcessA");
        if (pCreateProcessA) return pCreateProcessA(lpApplicationName, lpCommandLine, lpProcessAttributes, lpThreadAttributes, bInheritHandles, dwCreationFlags, lpEnvironment, lpCurrentDirectory, lpStartupInfo, lpProcessInformation);
        return FALSE;
    }
    inline BOOL _RegisterHotKey(HWND hWnd, int id, UINT fsModifiers, UINT vk) {
        HMODULE hUser32 = Hooks::_LoadLibraryA("user32.dll");
        if (!hUser32) return FALSE;
        auto pRegisterHotKey = (BOOL(WINAPI*)(HWND, int, UINT, UINT))Hooks::_GetProcAddress(hUser32, "RegisterHotKey");
        if (pRegisterHotKey) return pRegisterHotKey(hWnd, id, fsModifiers, vk);
        return FALSE;
    }

    inline LRESULT _CallNextHookEx(HHOOK hhk, int nCode, WPARAM wParam, LPARAM lParam) {
        HMODULE hUser32 = Hooks::_LoadLibraryA("user32.dll");
        if (!hUser32) return 0;
        auto pCallNextHookEx = (LRESULT(WINAPI*)(HHOOK, int, WPARAM, LPARAM))Hooks::_GetProcAddress(hUser32, "CallNextHookEx");
        if (pCallNextHookEx) return pCallNextHookEx(hhk, nCode, wParam, lParam);
        return 0;
    }

    inline HWND _GetForegroundWindow() {
        HMODULE hUser32 = Hooks::_LoadLibraryA("user32.dll");
        if (!hUser32) return nullptr;
        auto pGetForegroundWindow = (HWND(WINAPI*)())Hooks::_GetProcAddress(hUser32, "GetForegroundWindow");
        if (pGetForegroundWindow) return pGetForegroundWindow();
        return nullptr;
    }

    inline HANDLE _OpenProcess(DWORD dwDesiredAccess, BOOL bInheritHandle, DWORD dwProcessId) {
        HMODULE hK32 = Hooks::_GetModuleHandleA("kernel32.dll");
        if (!hK32) return nullptr;
        auto pOpenProcess = (HANDLE(WINAPI*)(DWORD, BOOL, DWORD))Hooks::_GetProcAddress(hK32, "OpenProcess");
        if (pOpenProcess) return pOpenProcess(dwDesiredAccess, bInheritHandle, dwProcessId);
        return nullptr;
    }

    inline HHOOK _SetWindowsHookExA(int idHook, HOOKPROC lpfn, HINSTANCE hmod, DWORD dwThreadId) {
        HMODULE hUser32 = Hooks::_LoadLibraryA("user32.dll");
        if (!hUser32) return NULL;
        auto pSetWindowsHookExA = (HHOOK(WINAPI*)(int, HOOKPROC, HINSTANCE, DWORD))Hooks::_GetProcAddress(hUser32, "SetWindowsHookExA");
        if (pSetWindowsHookExA) return pSetWindowsHookExA(idHook, lpfn, hmod, dwThreadId);
        return NULL;
    }

    inline BOOL _UnhookWindowsHookEx(HHOOK hhk) {
        HMODULE hUser32 = Hooks::_LoadLibraryA("user32.dll");
        if (!hUser32) return FALSE;
        auto pUnhookWindowsHookEx = (BOOL(WINAPI*)(HHOOK))Hooks::_GetProcAddress(hUser32, "UnhookWindowsHookEx");
        if (pUnhookWindowsHookEx) return pUnhookWindowsHookEx(hhk);
        return FALSE;
    }

    inline BOOL _OpenClipboard(HWND hWndNewOwner) {
        HMODULE hUser32 = Hooks::_LoadLibraryA("user32.dll");
        if (!hUser32) return FALSE;
        auto pOpenClipboard = (BOOL(WINAPI*)(HWND))Hooks::_GetProcAddress(hUser32, "OpenClipboard");
        if (pOpenClipboard) return pOpenClipboard(hWndNewOwner);
        return FALSE;
    }

    inline HANDLE _SetClipboardData(UINT uFormat, HANDLE hMem) {
        HMODULE hUser32 = Hooks::_LoadLibraryA("user32.dll");
        if (!hUser32) return nullptr;
        auto pSetClipboardData = (HANDLE(WINAPI*)(UINT, HANDLE))Hooks::_GetProcAddress(hUser32, "SetClipboardData");
        if (pSetClipboardData) return pSetClipboardData(uFormat, hMem);
        return nullptr;
    }

    inline BOOL _CloseClipboard(void) {
        HMODULE hUser32 = Hooks::_LoadLibraryA("user32.dll");
        if (!hUser32) return FALSE;
        auto pCloseClipboard = (BOOL(WINAPI*)(void))Hooks::_GetProcAddress(hUser32, "CloseClipboard");
        if (pCloseClipboard) return pCloseClipboard();
        return FALSE;
    }

    inline HANDLE _CreateThread(LPSECURITY_ATTRIBUTES lpThreadAttributes, SIZE_T dwStackSize, LPTHREAD_START_ROUTINE lpStartAddress, LPVOID lpParameter, DWORD dwCreationFlags, LPDWORD lpThreadId) {
        HMODULE hK32 = Hooks::_GetModuleHandleA("kernel32.dll");
        if (!hK32) return nullptr;
        auto pCreateThread = (HANDLE(WINAPI*)(LPSECURITY_ATTRIBUTES, SIZE_T, LPTHREAD_START_ROUTINE, LPVOID, DWORD, LPDWORD))Hooks::_GetProcAddress(hK32, "CreateThread");
        if (pCreateThread) return pCreateThread(lpThreadAttributes, dwStackSize, lpStartAddress, lpParameter, dwCreationFlags, lpThreadId);
        return nullptr;
    }

    inline HANDLE _GetClipboardData(UINT uFormat) {
        HMODULE hUser32 = Hooks::_LoadLibraryA("user32.dll");
        if (!hUser32) return nullptr;
        auto pGetClipboardData = (HANDLE(WINAPI*)(UINT))Hooks::_GetProcAddress(hUser32, "GetClipboardData");
        if (pGetClipboardData) return pGetClipboardData(uFormat);
        return nullptr;
    }
}

#undef GetProcAddress
#undef LoadLibraryA
#undef GetModuleHandleA
#undef RegOpenKeyExA
#undef RegQueryValueExA
#undef RegCloseKey
#undef CreateProcessA
#undef RegisterHotKey
#undef CallNextHookEx
#undef GetForegroundWindow
#undef OpenProcess
#undef GetClipboardData
#undef SetWindowsHookExA
#undef SetWindowsHookEx
#undef UnhookWindowsHookEx
#undef OpenClipboard
#undef SetClipboardData
#undef CloseClipboard
#undef CreateThread

#define GetProcAddress Hooks::_GetProcAddress
#define LoadLibraryA Hooks::_LoadLibraryA
#define GetModuleHandleA Hooks::_GetModuleHandleA
#define RegOpenKeyExA Hooks::_RegOpenKeyExA
#define RegQueryValueExA Hooks::_RegQueryValueExA
#define RegCloseKey Hooks::_RegCloseKey
#define CreateProcessA Hooks::_CreateProcessA
#define RegisterHotKey Hooks::_RegisterHotKey
#define CallNextHookEx Hooks::_CallNextHookEx
#define GetForegroundWindow Hooks::_GetForegroundWindow
#define OpenProcess Hooks::_OpenProcess
#define GetClipboardData Hooks::_GetClipboardData
#define SetWindowsHookExA Hooks::_SetWindowsHookExA
#define SetWindowsHookEx Hooks::_SetWindowsHookExA
#define UnhookWindowsHookEx Hooks::_UnhookWindowsHookEx
#define OpenClipboard Hooks::_OpenClipboard
#define SetClipboardData Hooks::_SetClipboardData
#define CloseClipboard Hooks::_CloseClipboard
#define CreateThread Hooks::_CreateThread
