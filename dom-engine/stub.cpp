#include <windows.h>
#include <string>
#include "bundled_app.h"
#include "src/Platform/Hooks.h"

#pragma comment(lib, "User32.lib")

extern "C" __declspec(dllexport) const unsigned char* GetDOMData(unsigned long* outSize, bool* lznt1, bool* rc4, const char** rc4key, unsigned long* ogSize) {
    *outSize = BUNDLED_APP_SIZE;
    *lznt1 = BUNDLED_APP_LZNT1;
    *rc4 = BUNDLED_APP_RC4;
    *rc4key = BUNDLED_APP_RC4_KEY;
    *ogSize = BUNDLED_APP_ORIGINAL_SIZE;
    return BUNDLED_DOM_APP;
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
    char myPath[MAX_PATH];
    GetModuleFileNameA(NULL, myPath, MAX_PATH);

    // 1. Search in the same directory as the stub
    char domPath[MAX_PATH];
    strcpy_s(domPath, myPath);
    char* lastSlash = strrchr(domPath, '\\');
    if (lastSlash) {
        strcpy_s(lastSlash + 1, MAX_PATH - (lastSlash - domPath) - 1, "dom.exe");
    }

    // 2. Fallback to searching PATH
    if (GetFileAttributesA(domPath) == INVALID_FILE_ATTRIBUTES) {
        char foundPath[MAX_PATH];
        LPSTR filePart;
        if (SearchPathA(NULL, "dom.exe", NULL, MAX_PATH, foundPath, &filePart)) {
            strcpy_s(domPath, foundPath);
        }
    }

    // 3. Fallback to registry install path
    if (GetFileAttributesA(domPath) == INVALID_FILE_ATTRIBUTES) {
        HKEY hKey;
        if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\DOMEngine", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
            char regPath[MAX_PATH];
            DWORD size = MAX_PATH;
            if (RegQueryValueExA(hKey, "InstallPath", NULL, NULL, (LPBYTE)regPath, &size) == ERROR_SUCCESS) {
                strcpy_s(domPath, regPath);
                if (domPath[strlen(domPath) - 1] != '\\') strcat_s(domPath, "\\");
                strcat_s(domPath, "dom.exe");
            }
            RegCloseKey(hKey);
        }
    }

    if (GetFileAttributesA(domPath) == INVALID_FILE_ATTRIBUTES) {
        MessageBoxA(NULL, "DOM Engine (dom.exe) not found in the current directory, PATH, or Registry.", "Error", MB_ICONERROR);
        return 1;
    }

    // Launch dom.exe and pass our own path to --load-dll
    std::string cmd = std::string("\"") + domPath + "\" --load-dll \"" + myPath + "\"";
    char cmdBuf[1024];
    strcpy_s(cmdBuf, cmd.c_str());

    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    if (CreateProcessA(NULL, cmdBuf, NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    } else {
        FILE* f = NULL;
        fopen_s(&f, "stub_log.txt", "w");
        if (f) {
            fprintf(f, "CreateProcessA error: %d\nCmd: %s\n", GetLastError(), cmdBuf);
            fclose(f);
        }
        return 1;
    }
    return 0;
}
