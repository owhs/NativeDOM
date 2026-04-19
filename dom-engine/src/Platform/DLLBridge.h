#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include "../Core/Value.h"

class DLLBridge {
public:
    // Generic DLL call with up to 8 parameters
    // sys.dllCall("User32.dll", "MessageBoxA", [0, "Hello", "Title", 0])
    static ValuePtr Call(const std::string& dllName, const std::string& funcName,
                         const std::vector<ValuePtr>& args) {
        HMODULE hMod = LoadLibraryA(dllName.c_str());
        if (!hMod) return Value::Null();

        FARPROC proc = GetProcAddress(hMod, funcName.c_str());
        if (!proc) return Value::Null();

        // Convert args to native parameters
        // We support up to 8 args (covers most Win32 APIs)
        uint64_t nativeArgs[8] = { 0 };
        std::vector<std::string> stringKeepAlive; // prevent dangling pointers

        for (size_t i = 0; i < args.size() && i < 8; i++) {
            auto& arg = args[i];
            if (arg->type == ValueType::String) {
                stringKeepAlive.push_back(arg->str);
                nativeArgs[i] = (uint64_t)stringKeepAlive.back().c_str();
            } else if (arg->type == ValueType::Number) {
                // Check if it looks like a pointer/handle (>= 0 and integer)
                nativeArgs[i] = (uint64_t)(int64_t)arg->number;
            } else if (arg->type == ValueType::Boolean) {
                nativeArgs[i] = arg->boolean ? 1 : 0;
            } else if (arg->type == ValueType::Null || arg->type == ValueType::Undefined) {
                nativeArgs[i] = 0;
            }
        }

        // Call via function pointer cast (x64 calling convention)
        typedef uint64_t(__stdcall* FnPtr)(uint64_t, uint64_t, uint64_t, uint64_t,
                                           uint64_t, uint64_t, uint64_t, uint64_t);
        uint64_t result = ((FnPtr)proc)(
            nativeArgs[0], nativeArgs[1], nativeArgs[2], nativeArgs[3],
            nativeArgs[4], nativeArgs[5], nativeArgs[6], nativeArgs[7]);

        return Value::Num((double)result);
    }

    // Load a DLL and return its handle (for repeated calls)
    static ValuePtr LoadDLL(const std::string& path) {
        HMODULE h = LoadLibraryA(path.c_str());
        return h ? Value::Num((double)(uintptr_t)h) : Value::Null();
    }
};
