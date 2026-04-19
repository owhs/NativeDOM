#pragma once
#include <windows.h>
#include <oleauto.h>
#include <comdef.h>
#include <string>
#include <vector>
#include <map>
#include "../Core/Value.h"

// ============================================================================
// COMBridge — Provides COM/OLE Automation (IDispatch) support for NativeDOM JS
//
// This bridge allows JavaScript scripts to:
//   - Create COM objects via ProgID (e.g. "Scripting.FileSystemObject")
//   - Call methods on COM objects
//   - Get/set properties on COM objects
//   - Enumerate collections
//   - Chain method calls (returns wrapped COM objects)
//
// Usage from JS:
//   let fso = sys.com.create("Scripting.FileSystemObject");
//   let exists = fso.call("FileExists", "C:\\test.txt");
//   let drives = fso.get("Drives");
//   fso.set("SomeProperty", value);
//   fso.release();
// ============================================================================

extern void sysLog(const std::string& msg);

// Wrap a COM IDispatch pointer as a NativeDOM Value object with methods
class COMBridge {
    // Track all live COM objects for cleanup
    static std::vector<IDispatch*>& getLiveObjects() {
        static std::vector<IDispatch*> objs;
        return objs;
    }

    static bool g_comInitialized;

    static void ensureCOMInit() {
        if (!g_comInitialized) {
            HRESULT hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);
            if (SUCCEEDED(hr) || hr == S_FALSE || hr == RPC_E_CHANGED_MODE) {
                g_comInitialized = true;
            }
        }
    }

    // Convert a NativeDOM Value to a VARIANT for passing to COM methods
    static VARIANT ValueToVariant(ValuePtr val) {
        VARIANT v;
        VariantInit(&v);
        if (!val || val->type == ValueType::Undefined || val->type == ValueType::Null) {
            v.vt = VT_EMPTY;
        } else if (val->type == ValueType::Boolean) {
            v.vt = VT_BOOL;
            v.boolVal = val->boolean ? VARIANT_TRUE : VARIANT_FALSE;
        } else if (val->type == ValueType::Number) {
            double d = val->number;
            // If it's an integer, use VT_I4 for better COM compat
            if (d == (int)d && d >= -2147483648.0 && d <= 2147483647.0) {
                v.vt = VT_I4;
                v.lVal = (LONG)d;
            } else {
                v.vt = VT_R8;
                v.dblVal = d;
            }
        } else if (val->type == ValueType::String) {
            v.vt = VT_BSTR;
            int len = MultiByteToWideChar(CP_UTF8, 0, val->str.c_str(), -1, NULL, 0);
            std::wstring ws(len - 1, 0);
            MultiByteToWideChar(CP_UTF8, 0, val->str.c_str(), -1, &ws[0], len);
            v.bstrVal = SysAllocString(ws.c_str());
        } else if (val->type == ValueType::Object) {
            // Check if this is a wrapped COM object (has __comPtr__)
            auto comPtrVal = val->getProperty("__comPtr__");
            if (comPtrVal && comPtrVal->type == ValueType::Number) {
                IDispatch* pDisp = (IDispatch*)(uintptr_t)(uint64_t)comPtrVal->number;
                if (pDisp) {
                    v.vt = VT_DISPATCH;
                    v.pdispVal = pDisp;
                    pDisp->AddRef();
                }
            } else {
                v.vt = VT_EMPTY;
            }
        } else {
            v.vt = VT_EMPTY;
        }
        return v;
    }

    // Convert a VARIANT returned from COM back to a NativeDOM Value
    static ValuePtr VariantToValue(const VARIANT& v) {
        switch (v.vt) {
            case VT_EMPTY:
            case VT_NULL:
                return Value::Null();
            case VT_BOOL:
                return Value::Bool(v.boolVal != VARIANT_FALSE);
            case VT_I1: return Value::Num((double)v.cVal);
            case VT_I2: return Value::Num((double)v.iVal);
            case VT_I4: return Value::Num((double)v.lVal);
            case VT_I8: return Value::Num((double)v.llVal);
            case VT_UI1: return Value::Num((double)v.bVal);
            case VT_UI2: return Value::Num((double)v.uiVal);
            case VT_UI4: return Value::Num((double)v.ulVal);
            case VT_UI8: return Value::Num((double)v.ullVal);
            case VT_INT: return Value::Num((double)v.intVal);
            case VT_UINT: return Value::Num((double)v.uintVal);
            case VT_R4: return Value::Num((double)v.fltVal);
            case VT_R8: return Value::Num(v.dblVal);
            case VT_CY: return Value::Num((double)v.cyVal.int64 / 10000.0);
            case VT_DATE: return Value::Num(v.date);
            case VT_BSTR: {
                if (!v.bstrVal) return Value::Str("");
                int len = WideCharToMultiByte(CP_UTF8, 0, v.bstrVal, -1, NULL, 0, NULL, NULL);
                std::string s(len - 1, 0);
                WideCharToMultiByte(CP_UTF8, 0, v.bstrVal, -1, &s[0], len, NULL, NULL);
                return Value::Str(s);
            }
            case VT_DISPATCH: {
                if (v.pdispVal) {
                    v.pdispVal->AddRef();
                    return WrapDispatch(v.pdispVal);
                }
                return Value::Null();
            }
            case VT_UNKNOWN: {
                if (v.punkVal) {
                    IDispatch* pDisp = nullptr;
                    HRESULT hr = v.punkVal->QueryInterface(IID_IDispatch, (void**)&pDisp);
                    if (SUCCEEDED(hr) && pDisp) {
                        return WrapDispatch(pDisp);
                    }
                }
                return Value::Null();
            }
            case VT_ERROR:
                return Value::Num((double)v.scode);
            default:
                // Try to coerce via VariantChangeType
                if (v.vt & VT_ARRAY) {
                    // Handle SafeArrays -> JS arrays
                    return SafeArrayToValue(v);
                }
                // If BYREF variant, dereference
                if (v.vt & VT_BYREF) {
                    VARIANT deref;
                    VariantInit(&deref);
                    VariantCopyInd(&deref, &v);
                    auto result = VariantToValue(deref);
                    VariantClear(&deref);
                    return result;
                }
                {
                    VARIANT converted;
                    VariantInit(&converted);
                    if (SUCCEEDED(VariantChangeType(&converted, &v, 0, VT_BSTR))) {
                        auto result = VariantToValue(converted);
                        VariantClear(&converted);
                        return result;
                    }
                    VariantClear(&converted);
                }
                return Value::Null();
        }
    }

    // Convert SafeArray to JS array
    static ValuePtr SafeArrayToValue(const VARIANT& v) {
        auto arr = Value::Array();
        if (!(v.vt & VT_ARRAY) || !v.parray) return arr;

        SAFEARRAY* psa = v.parray;
        LONG lBound, uBound;
        SafeArrayGetLBound(psa, 1, &lBound);
        SafeArrayGetUBound(psa, 1, &uBound);

        for (LONG i = lBound; i <= uBound; i++) {
            VARIANT elem;
            VariantInit(&elem);
            if (SUCCEEDED(SafeArrayGetElement(psa, &i, &elem))) {
                arr->elements.push_back(VariantToValue(elem));
                VariantClear(&elem);
            } else {
                arr->elements.push_back(Value::Null());
            }
        }
        return arr;
    }

    // Helper: get DISPID for a member name
    static HRESULT GetDispID(IDispatch* pDisp, const std::string& name, DISPID& dispid) {
        int len = MultiByteToWideChar(CP_UTF8, 0, name.c_str(), -1, NULL, 0);
        std::wstring wName(len - 1, 0);
        MultiByteToWideChar(CP_UTF8, 0, name.c_str(), -1, &wName[0], len);
        LPOLESTR oleStr = (LPOLESTR)wName.c_str();
        return pDisp->GetIDsOfNames(IID_NULL, &oleStr, 1, LOCALE_USER_DEFAULT, &dispid);
    }

public:

    // Wrap an IDispatch* into a NativeDOM JS object with .call(), .get(), .set(), .release()
    static ValuePtr WrapDispatch(IDispatch* pDisp) {
        if (!pDisp) return Value::Null();

        getLiveObjects().push_back(pDisp);

        auto obj = Value::Object();

        // Store the raw pointer as a number (opaque handle)
        obj->setProperty("__comPtr__", Value::Num((double)(uintptr_t)pDisp));
        obj->setProperty("__isCOMObject__", Value::Bool(true));

        // ---- obj.call(methodName, ...args) -> result ----
        obj->setProperty("call", Value::Native([pDisp](std::vector<ValuePtr> args, ValuePtr) -> ValuePtr {
            if (args.empty()) return Value::Null();
            std::string methodName = args[0]->toString();

            DISPID dispid;
            HRESULT hr = GetDispID(pDisp, methodName, dispid);
            if (FAILED(hr)) {
                char buf[256];
                snprintf(buf, sizeof(buf), "COM Error: Method '%s' not found (0x%08X)", methodName.c_str(), (unsigned)hr);
                return Value::Str(buf);
            }

            // Build DISPPARAMS (COM params are in reverse order)
            std::vector<VARIANT> variants(args.size() - 1);
            for (size_t i = 1; i < args.size(); i++) {
                variants[args.size() - 1 - i] = ValueToVariant(args[i]);
            }

            DISPPARAMS dp = {};
            dp.cArgs = (UINT)variants.size();
            dp.rgvarg = variants.empty() ? nullptr : variants.data();

            VARIANT result;
            VariantInit(&result);
            EXCEPINFO excepInfo = {};
            UINT argErr = 0;

            hr = pDisp->Invoke(dispid, IID_NULL, LOCALE_USER_DEFAULT,
                               DISPATCH_METHOD, &dp, &result, &excepInfo, &argErr);

            // Cleanup input variants
            for (auto& v : variants) VariantClear(&v);

            if (FAILED(hr)) {
                std::string errMsg = "COM Error: Invoke failed";
                if (excepInfo.bstrDescription) {
                    int elen = WideCharToMultiByte(CP_UTF8, 0, excepInfo.bstrDescription, -1, NULL, 0, NULL, NULL);
                    std::string edesc(elen - 1, 0);
                    WideCharToMultiByte(CP_UTF8, 0, excepInfo.bstrDescription, -1, &edesc[0], elen, NULL, NULL);
                    errMsg += ": " + edesc;
                    SysFreeString(excepInfo.bstrDescription);
                }
                if (excepInfo.bstrSource) SysFreeString(excepInfo.bstrSource);
                if (excepInfo.bstrHelpFile) SysFreeString(excepInfo.bstrHelpFile);
                char buf[512];
                snprintf(buf, sizeof(buf), "%s (0x%08X)", errMsg.c_str(), (unsigned)hr);
                return Value::Str(buf);
            }

            auto jsResult = VariantToValue(result);
            VariantClear(&result);
            return jsResult;
        }));

        // ---- obj.get(propertyName) -> value ----
        obj->setProperty("get", Value::Native([pDisp](std::vector<ValuePtr> args, ValuePtr) -> ValuePtr {
            if (args.empty()) return Value::Null();
            std::string propName = args[0]->toString();

            DISPID dispid;
            HRESULT hr = GetDispID(pDisp, propName, dispid);
            if (FAILED(hr)) {
                char buf[256];
                snprintf(buf, sizeof(buf), "COM Error: Property '%s' not found (0x%08X)", propName.c_str(), (unsigned)hr);
                return Value::Str(buf);
            }

            // Pass index arguments if provided (for parameterized properties like Item(i))
            std::vector<VARIANT> indexArgs;
            for (size_t i = 1; i < args.size(); i++) {
                indexArgs.insert(indexArgs.begin(), ValueToVariant(args[i]));
            }

            DISPPARAMS dp = {};
            dp.cArgs = (UINT)indexArgs.size();
            dp.rgvarg = indexArgs.empty() ? nullptr : indexArgs.data();

            VARIANT result;
            VariantInit(&result);
            EXCEPINFO excepInfo = {};

            hr = pDisp->Invoke(dispid, IID_NULL, LOCALE_USER_DEFAULT,
                               DISPATCH_PROPERTYGET, &dp, &result, &excepInfo, nullptr);

            for (auto& v : indexArgs) VariantClear(&v);

            if (FAILED(hr)) {
                if (excepInfo.bstrDescription) SysFreeString(excepInfo.bstrDescription);
                if (excepInfo.bstrSource) SysFreeString(excepInfo.bstrSource);
                if (excepInfo.bstrHelpFile) SysFreeString(excepInfo.bstrHelpFile);
                char buf[256];
                snprintf(buf, sizeof(buf), "COM Error: Get '%s' failed (0x%08X)", propName.c_str(), (unsigned)hr);
                return Value::Str(buf);
            }

            auto jsResult = VariantToValue(result);
            VariantClear(&result);
            return jsResult;
        }));

        // ---- obj.set(propertyName, value) -> bool ----
        obj->setProperty("set", Value::Native([pDisp](std::vector<ValuePtr> args, ValuePtr) -> ValuePtr {
            if (args.size() < 2) return Value::Bool(false);
            std::string propName = args[0]->toString();

            DISPID dispid;
            HRESULT hr = GetDispID(pDisp, propName, dispid);
            if (FAILED(hr)) return Value::Bool(false);

            VARIANT val = ValueToVariant(args[1]);
            DISPID putId = DISPID_PROPERTYPUT;
            DISPPARAMS dp = {};
            dp.cArgs = 1;
            dp.rgvarg = &val;
            dp.cNamedArgs = 1;
            dp.rgdispidNamedArgs = &putId;

            // Try PROPERTYPUT first, then PROPERTYPUTREF for objects
            hr = pDisp->Invoke(dispid, IID_NULL, LOCALE_USER_DEFAULT,
                               DISPATCH_PROPERTYPUT, &dp, nullptr, nullptr, nullptr);
            if (FAILED(hr) && val.vt == VT_DISPATCH) {
                hr = pDisp->Invoke(dispid, IID_NULL, LOCALE_USER_DEFAULT,
                                   DISPATCH_PROPERTYPUTREF, &dp, nullptr, nullptr, nullptr);
            }

            VariantClear(&val);
            return Value::Bool(SUCCEEDED(hr));
        }));

        // ---- obj.release() -> undefined ----
        obj->setProperty("release", Value::Native([pDisp](std::vector<ValuePtr> args, ValuePtr) -> ValuePtr {
            pDisp->Release();
            // Remove from live list
            auto& live = getLiveObjects();
            live.erase(std::remove(live.begin(), live.end(), pDisp), live.end());
            return Value::Undefined();
        }));

        // ---- obj.queryInterface(iidString) -> wrapped COM object or null ----
        obj->setProperty("queryInterface", Value::Native([pDisp](std::vector<ValuePtr> args, ValuePtr) -> ValuePtr {
            if (args.empty()) return Value::Null();
            std::string iidStr = args[0]->toString();

            // Parse IID from string like "{00000000-0000-0000-0000-000000000000}"
            int len = MultiByteToWideChar(CP_UTF8, 0, iidStr.c_str(), -1, NULL, 0);
            std::wstring wIid(len - 1, 0);
            MultiByteToWideChar(CP_UTF8, 0, iidStr.c_str(), -1, &wIid[0], len);

            IID iid;
            HRESULT hr = IIDFromString(wIid.c_str(), &iid);
            if (FAILED(hr)) return Value::Null();

            IDispatch* pNew = nullptr;
            hr = pDisp->QueryInterface(iid, (void**)&pNew);
            if (SUCCEEDED(hr) && pNew) {
                return WrapDispatch(pNew);
            }
            return Value::Null();
        }));

        // ---- obj.forEach(callback) -> iterate IEnumVARIANT (for-each collections) ----
        obj->setProperty("forEach", Value::Native([pDisp](std::vector<ValuePtr> args, ValuePtr) -> ValuePtr {
            if (args.empty()) return Value::Undefined();

            // Get _NewEnum via DISPID_NEWENUM
            DISPPARAMS dpEmpty = {};
            VARIANT enumResult;
            VariantInit(&enumResult);

            HRESULT hr = pDisp->Invoke(DISPID_NEWENUM, IID_NULL, LOCALE_USER_DEFAULT,
                                        DISPATCH_PROPERTYGET | DISPATCH_METHOD,
                                        &dpEmpty, &enumResult, nullptr, nullptr);
            if (FAILED(hr)) return Value::Str("COM Error: Collection not enumerable");

            IEnumVARIANT* pEnum = nullptr;
            if (enumResult.vt == VT_UNKNOWN && enumResult.punkVal) {
                hr = enumResult.punkVal->QueryInterface(IID_IEnumVARIANT, (void**)&pEnum);
            } else if (enumResult.vt == VT_DISPATCH && enumResult.pdispVal) {
                hr = enumResult.pdispVal->QueryInterface(IID_IEnumVARIANT, (void**)&pEnum);
            }
            VariantClear(&enumResult);

            if (!pEnum) return Value::Str("COM Error: Cannot get enumerator");

            VARIANT item;
            ULONG fetched = 0;
            int index = 0;
            while (pEnum->Next(1, &item, &fetched) == S_OK && fetched > 0) {
                auto jsItem = VariantToValue(item);
                VariantClear(&item);

                // Call the callback with (item, index)
                // Note: We can't call callFunction here since we don't have interpreter ref,
                // but if the callback is a NativeFunction (closure), it works directly
                if (args[0]->type == ValueType::NativeFunction) {
                    args[0]->nativeFn({ jsItem, Value::Num((double)index) }, nullptr);
                } else if (args[0]->type == ValueType::Function) {
                    // For script functions, we need the interpreter — store result for manual iteration
                    // This will be enhanced by the binding in Window.h
                }
                index++;
            }
            pEnum->Release();
            return Value::Num((double)index);
        }));

        // ---- obj.toArray() -> convert enumerable collection to JS array ----
        obj->setProperty("toArray", Value::Native([pDisp](std::vector<ValuePtr> args, ValuePtr) -> ValuePtr {
            auto arr = Value::Array();

            DISPPARAMS dpEmpty = {};
            VARIANT enumResult;
            VariantInit(&enumResult);

            HRESULT hr = pDisp->Invoke(DISPID_NEWENUM, IID_NULL, LOCALE_USER_DEFAULT,
                                        DISPATCH_PROPERTYGET | DISPATCH_METHOD,
                                        &dpEmpty, &enumResult, nullptr, nullptr);
            if (FAILED(hr)) return arr;

            IEnumVARIANT* pEnum = nullptr;
            if (enumResult.vt == VT_UNKNOWN && enumResult.punkVal) {
                hr = enumResult.punkVal->QueryInterface(IID_IEnumVARIANT, (void**)&pEnum);
            } else if (enumResult.vt == VT_DISPATCH && enumResult.pdispVal) {
                hr = enumResult.pdispVal->QueryInterface(IID_IEnumVARIANT, (void**)&pEnum);
            }
            VariantClear(&enumResult);

            if (!pEnum) return arr;

            VARIANT item;
            ULONG fetched = 0;
            while (pEnum->Next(1, &item, &fetched) == S_OK && fetched > 0) {
                arr->elements.push_back(VariantToValue(item));
                VariantClear(&item);
            }
            pEnum->Release();
            return arr;
        }));

        return obj;
    }

    // Create a COM object from a ProgID string (e.g. "Scripting.FileSystemObject")
    static ValuePtr CreateObject(const std::string& progId) {
        ensureCOMInit();

        // Convert ProgID to CLSID
        int len = MultiByteToWideChar(CP_UTF8, 0, progId.c_str(), -1, NULL, 0);
        std::wstring wProgId(len - 1, 0);
        MultiByteToWideChar(CP_UTF8, 0, progId.c_str(), -1, &wProgId[0], len);

        CLSID clsid;
        HRESULT hr = CLSIDFromProgID(wProgId.c_str(), &clsid);
        if (FAILED(hr)) {
            // Try as raw CLSID string
            hr = CLSIDFromString(wProgId.c_str(), &clsid);
            if (FAILED(hr)) {
                char buf[256];
                snprintf(buf, sizeof(buf), "COM Error: ProgID '%s' not found (0x%08X)", progId.c_str(), (unsigned)hr);
                return Value::Str(buf);
            }
        }

        IDispatch* pDisp = nullptr;
        hr = CoCreateInstance(clsid, NULL, CLSCTX_ALL, IID_IDispatch, (void**)&pDisp);
        if (FAILED(hr) || !pDisp) {
            char buf[256];
            snprintf(buf, sizeof(buf), "COM Error: CoCreateInstance failed for '%s' (0x%08X)", progId.c_str(), (unsigned)hr);
            return Value::Str(buf);
        }

        sysLog("[COM] Created object: " + progId);
        return WrapDispatch(pDisp);
    }

    // Get an existing running COM object by ProgID
    static ValuePtr GetObject(const std::string& progId) {
        ensureCOMInit();

        int len = MultiByteToWideChar(CP_UTF8, 0, progId.c_str(), -1, NULL, 0);
        std::wstring wProgId(len - 1, 0);
        MultiByteToWideChar(CP_UTF8, 0, progId.c_str(), -1, &wProgId[0], len);

        CLSID clsid;
        HRESULT hr = CLSIDFromProgID(wProgId.c_str(), &clsid);
        if (FAILED(hr)) return Value::Null();

        IUnknown* pUnk = nullptr;
        hr = GetActiveObject(clsid, NULL, &pUnk);
        if (FAILED(hr) || !pUnk) return Value::Null();

        IDispatch* pDisp = nullptr;
        hr = pUnk->QueryInterface(IID_IDispatch, (void**)&pDisp);
        pUnk->Release();

        if (FAILED(hr) || !pDisp) return Value::Null();

        sysLog("[COM] Got active object: " + progId);
        return WrapDispatch(pDisp);
    }

    // Release all tracked COM objects (for cleanup on exit)
    static void ReleaseAll() {
        auto& live = getLiveObjects();
        for (auto* p : live) {
            if (p) p->Release();
        }
        live.clear();
    }
};

bool COMBridge::g_comInitialized = false;
