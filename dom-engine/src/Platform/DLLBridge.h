#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include "../Core/Value.h"

class DLLBridge {
public:
    static uint64_t SafeCall(FARPROC proc, uint64_t* args, bool& outException) {
        typedef uint64_t(__stdcall* FnPtr)(uint64_t, uint64_t, uint64_t, uint64_t,
                                           uint64_t, uint64_t, uint64_t, uint64_t);
        uint64_t result = 0;
        outException = false;
        __try {
            result = ((FnPtr)proc)(args[0], args[1], args[2], args[3], args[4], args[5], args[6], args[7]);
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            outException = true;
        }
        return result;
    }

    static bool SafeInitExtension(FARPROC proc, void** ctxArgs) {
        bool exceptionThrown = false;
        __try {
            typedef void(__stdcall* InitFn)(void**);
            ((InitFn)proc)(ctxArgs);
        } __except(EXCEPTION_EXECUTE_HANDLER) {
            exceptionThrown = true;
        }
        return !exceptionThrown;
    }

    // Generic DLL call with up to 8 parameters
    static ValuePtr Call(const std::string& dllName, const std::string& funcName,
                         const std::vector<ValuePtr>& args) {
        HMODULE hMod = LoadLibraryA(dllName.c_str());
        if (!hMod) return Value::Str("Error: DLL not found.");

        FARPROC proc = GetProcAddress(hMod, funcName.c_str());
        if (!proc) return Value::Str("Error: Function not found.");

        // Convert args to native parameters
        uint64_t nativeArgs[8] = { 0 };
        std::vector<std::string> stringKeepAlive; // prevent dangling pointers

        for (size_t i = 0; i < args.size() && i < 8; i++) {
            auto& arg = args[i];
            if (arg->type == ValueType::String) {
                stringKeepAlive.push_back(arg->str);
                nativeArgs[i] = (uint64_t)stringKeepAlive.back().c_str();
            } else if (arg->type == ValueType::Number) {
                nativeArgs[i] = (uint64_t)(int64_t)arg->number;
            } else if (arg->type == ValueType::Boolean) {
                nativeArgs[i] = arg->boolean ? 1 : 0;
            } else if (arg->type == ValueType::Null || arg->type == ValueType::Undefined) {
                nativeArgs[i] = 0;
            }
        }

        bool exceptionThrown = false;
        uint64_t result = SafeCall(proc, nativeArgs, exceptionThrown);

        if (exceptionThrown) {
            return Value::Str("Error: DLL Exception thrown during execution.");
        }

        return Value::Num((double)result);
    }

    // Load a DLL and return its handle (for repeated calls)
    static ValuePtr LoadDLL(const std::string& path) {
        HMODULE h = LoadLibraryA(path.c_str());
        return h ? Value::Num((double)(uintptr_t)h) : Value::Null();
    }
};
