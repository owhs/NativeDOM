#pragma once
#include "Core/Interpreter.h"
#include "DOM/Element.h"
#include "DOM/Selector.h"
#include "Platform/DLLBridge.h"
#include "Platform/Http.h"

extern HWND g_hwnd;

class ScriptBridge {
    Interpreter& interp;
    std::shared_ptr<Element> appRoot;

    // Map Element* -> ValuePtr for consistent JS references
    std::vector<std::pair<Element*, ValuePtr>> elementCache;

    // Event listener registry
    struct ListenerList {
        std::string type;
        std::vector<ValuePtr> callbacks;
    };
    struct ElementListeners {
        Element* el;
        std::vector<ListenerList> lists;
    };
    std::vector<ElementListeners> listenerRegistry;

    // Global (document-level) listeners
    std::vector<ListenerList> globalListeners;

    // Custom event registry (for user-defined events)
    std::vector<ListenerList> customEventListeners;

public:
    ScriptBridge(Interpreter& interp, std::shared_ptr<Element> root) : interp(interp), appRoot(root) {}

    void reset(std::shared_ptr<Element> newRoot) {
        appRoot = newRoot;
        elementCache.clear();
        listenerRegistry.clear();
        globalListeners.clear();
        customEventListeners.clear();
    }

    void init() {
        // ---- Element wrapper ----
        auto makeElementProxy = [this](std::shared_ptr<Element> el) -> ValuePtr {
            return wrapElement(el);
        };

        // ---- document object ----
        auto doc = Value::Object();

        doc->setProperty("querySelector", Value::Native([this](std::vector<ValuePtr> args, ValuePtr) -> ValuePtr {
            if (args.empty()) return Value::Null();
            auto el = Selector::Query(appRoot, args[0]->toString(), false);
            return el ? wrapElement(el) : Value::Null();
        }));

        doc->setProperty("querySelectorAll", Value::Native([this](std::vector<ValuePtr> args, ValuePtr) -> ValuePtr {
            auto arr = Value::Array();
            if (args.empty()) return arr;
            auto els = Selector::QueryAll(appRoot, args[0]->toString(), false);
            for (auto& el : els) arr->elements.push_back(wrapElement(el));
            arr->prototype = interp.getArrayProto();
            return arr;
        }));

        doc->setProperty("shadowDomSelector", Value::Native([this](std::vector<ValuePtr> args, ValuePtr) -> ValuePtr {
            if (args.empty()) return Value::Null();
            auto el = Selector::Query(appRoot, args[0]->toString(), true);
            return el ? wrapElement(el) : Value::Null();
        }));

        doc->setProperty("shadowDomSelectorAll", Value::Native([this](std::vector<ValuePtr> args, ValuePtr) -> ValuePtr {
            auto arr = Value::Array();
            if (args.empty()) return arr;
            auto els = Selector::QueryAll(appRoot, args[0]->toString(), true);
            for (auto& el : els) arr->elements.push_back(wrapElement(el));
            arr->prototype = interp.getArrayProto();
            return arr;
        }));

        doc->setProperty("getElementById", Value::Native([this](std::vector<ValuePtr> args, ValuePtr) -> ValuePtr {
            if (args.empty()) return Value::Null();
            auto el = Selector::Query(appRoot, "#" + args[0]->toString(), false);
            return el ? wrapElement(el) : Value::Null();
        }));

        doc->setProperty("createElement", Value::Native([this](std::vector<ValuePtr> args, ValuePtr) -> ValuePtr {
            if (args.empty()) return Value::Null();
            auto el = std::make_shared<Element>();
            el->tag = args[0]->toString();
            return wrapElement(el);
        }));

        // document.addEventListener (global listeners)
        doc->setProperty("addEventListener", Value::Native([this](std::vector<ValuePtr> args, ValuePtr) -> ValuePtr {
            if (args.size() < 2) return Value::Undefined();
            std::string type = args[0]->toString();
            bool found = false;
            for (auto& gl : globalListeners) {
                if (gl.type == type) { gl.callbacks.push_back(args[1]); found = true; break; }
            }
            if (!found) {
                ListenerList ll; ll.type = type; ll.callbacks.push_back(args[1]);
                globalListeners.push_back(ll);
            }
            return Value::Undefined();
        }));

        doc->setProperty("removeEventListener", Value::Native([this](std::vector<ValuePtr> args, ValuePtr) -> ValuePtr {
            if (args.size() < 2) return Value::Undefined();
            // Simplified: remove all listeners of this type (exact fn match is complex)
            std::string type = args[0]->toString();
            for (size_t i = 0; i < globalListeners.size(); i++) {
                if (globalListeners[i].type == type) {
                    globalListeners.erase(globalListeners.begin() + i);
                    break;
                }
            }
            return Value::Undefined();
        }));

        // document.dispatchEvent (custom events)
        doc->setProperty("dispatchEvent", Value::Native([this](std::vector<ValuePtr> args, ValuePtr) -> ValuePtr {
            if (args.empty()) return Value::Undefined();
            auto eventObj = args[0];
            std::string type = eventObj->getProperty("type")->toString();
            dispatchGlobalEvent(type, eventObj);
            return Value::Undefined();
        }));

        // document.measureText(text, fontSize?) -> pixel width using NanoVG
        doc->setProperty("measureText", Value::Native([](std::vector<ValuePtr> args, ValuePtr) -> ValuePtr {
            extern NVGcontext* g_nvg;
            if (!g_nvg || args.empty()) return Value::Num(0);
            std::string text = args[0]->toString();
            if (text.empty()) return Value::Num(0);
            float fontSize = args.size() > 1 ? (float)args[1]->toNumber() : 14.0f;
            std::string fontFamily = args.size() > 2 ? args[2]->toString() : "Segoe UI";
            nvgSave(g_nvg);
            nvgFontSize(g_nvg, fontSize);
            LoadFontToVG(g_nvg, fontFamily);
            nvgFontFace(g_nvg, fontFamily.c_str());
            nvgTextAlign(g_nvg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
            float advance = nvgTextBounds(g_nvg, 0, 0, text.c_str(), NULL, NULL);
            nvgRestore(g_nvg);
            return Value::Num((double)advance);
        }));

        interp.defineGlobal("document", doc);
        interp.defineGlobal("window", doc);

        // ---- fetch() ----
        interp.defineGlobal("fetch", Value::Native([this](std::vector<ValuePtr> args, ValuePtr) -> ValuePtr {
            if (args.empty()) return Value::MakePromise();
            std::string url = args[0]->toString();
            std::string method = "GET";
            std::string body;
            std::vector<std::pair<std::string, std::string>> headers;

            if (args.size() > 1 && args[1]->type == ValueType::Object) {
                auto opts = args[1];
                auto m = opts->getProperty("method");
                if (m->type == ValueType::String) method = m->str;
                auto b = opts->getProperty("body");
                if (b->type == ValueType::String) body = b->str;
                auto h = opts->getProperty("headers");
                if (h->type == ValueType::Object) {
                    for (auto& kv : h->properties) {
                        headers.push_back({kv.first, kv.second->toString()});
                    }
                }
            }

            // Create a promise that resolves asynchronously
            auto promise = Value::MakePromise();
            promise->prototype = interp.getGlobalEnv()->get("Promise");

            HttpClient::FetchAsync(url, method, body, headers,
                [this, promise](HttpClient::Response resp) {
                    auto responseObj = Value::Object();
                    responseObj->setProperty("status", Value::Num((double)resp.status));
                    responseObj->setProperty("ok", Value::Bool(resp.ok));
                    responseObj->setProperty("body", Value::Str(resp.body));
                    responseObj->setProperty("text", Value::Native([resp](std::vector<ValuePtr>, ValuePtr) -> ValuePtr {
                        return Value::Str(resp.body);
                    }));
                    responseObj->setProperty("json", Value::Native([this, resp](std::vector<ValuePtr>, ValuePtr) -> ValuePtr {
                        return interp.exec("JSON.parse('" + resp.body + "')");
                    }));

                    promise->promiseState = Value::PromiseState::Fulfilled;
                    promise->promiseResult = responseObj;
                    for (auto& cb : promise->thenCallbacks) {
                        if (cb.first && (cb.first->type == ValueType::Function || cb.first->type == ValueType::NativeFunction)) {
                            interp.callFunction(cb.first, { responseObj });
                        }
                    }
                    // Trigger repaint
                    if (g_hwnd) PostMessage(g_hwnd, WM_APP + 1, 0, 0);
                });

            return promise;
        }));

        // ---- Custom event creation ----
        interp.defineGlobal("CustomEvent", Value::Native([](std::vector<ValuePtr> args, ValuePtr) -> ValuePtr {
            auto evt = Value::Object();
            if (args.size() > 0) evt->setProperty("type", Value::Str(args[0]->toString()));
            if (args.size() > 1 && args[1]->type == ValueType::Object) {
                auto detail = args[1]->getProperty("detail");
                if (detail) evt->setProperty("detail", detail);
            }
            evt->setProperty("preventDefault", Value::Native([evt](std::vector<ValuePtr>, ValuePtr) -> ValuePtr {
                evt->setProperty("defaultPrevented", Value::Bool(true));
                return Value::Undefined();
            }));
            evt->setProperty("stopPropagation", Value::Native([evt](std::vector<ValuePtr>, ValuePtr) -> ValuePtr {
                evt->setProperty("propagationStopped", Value::Bool(true));
                return Value::Undefined();
            }));
            return evt;
        }));
    }

    // ---- Dispatch events from C++ to script listeners ----
    
    void dispatchScriptEvent(Element* el, const std::string& type, ValuePtr eventObj = nullptr) {
        if (!eventObj) {
            eventObj = Value::Object();
            eventObj->setProperty("type", Value::Str(type));
        }
        eventObj->setProperty("type", Value::Str(type));

        // Element-level listeners
        Element* current = el;
        while (current) {
            for (auto& lr : listenerRegistry) {
                if (lr.el == current) {
                    for (auto& ll : lr.lists) {
                        if (ll.type == type) {
                            for (auto& cb : ll.callbacks) {
                                interp.callFunction(cb, { eventObj });
                                auto stopped = eventObj->getProperty("immediatePropagationStopped");
                                if (stopped && stopped->isTruthy()) return;
                            }
                        }
                    }
                    break;
                }
            }
            // Check for inline handler
            std::string handler = current->Get("on" + capitalize(type));
            if (!handler.empty()) {
                interp.exec(handler);
            }

            auto propStopped = eventObj->getProperty("propagationStopped");
            if (propStopped && propStopped->isTruthy()) return;

            // Bubble up
            current = current->parent ? current->parent : current->shadowHost;
        }

        // Global listeners
        dispatchGlobalEvent(type, eventObj);
    }

    void dispatchKeyEvent(Element* el, const std::string& type, int keyCode, const std::string& key) {
        auto eventObj = Value::Object();
        eventObj->setProperty("type", Value::Str(type));
        eventObj->setProperty("keyCode", Value::Num((double)keyCode));
        eventObj->setProperty("key", Value::Str(key));

        // Modifier key state (via Win32 GetKeyState)
        bool ctrl = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
        bool shift = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
        bool alt = (GetKeyState(VK_MENU) & 0x8000) != 0;
        eventObj->setProperty("ctrlKey", Value::Bool(ctrl));
        eventObj->setProperty("shiftKey", Value::Bool(shift));
        eventObj->setProperty("altKey", Value::Bool(alt));

        eventObj->setProperty("preventDefault", Value::Native([eventObj](std::vector<ValuePtr>, ValuePtr) -> ValuePtr {
            eventObj->setProperty("defaultPrevented", Value::Bool(true));
            return Value::Undefined();
        }));
        eventObj->setProperty("stopPropagation", Value::Native([eventObj](std::vector<ValuePtr>, ValuePtr) -> ValuePtr {
            eventObj->setProperty("propagationStopped", Value::Bool(true));
            return Value::Undefined();
        }));
        eventObj->setProperty("stopImmediatePropagation", Value::Native([eventObj](std::vector<ValuePtr>, ValuePtr) -> ValuePtr {
            eventObj->setProperty("immediatePropagationStopped", Value::Bool(true));
            eventObj->setProperty("propagationStopped", Value::Bool(true));
            return Value::Undefined();
        }));

        // Update sys.event for legacy compatibility (includes modifier state)
        char kcBuf[192]; snprintf(kcBuf, sizeof(kcBuf),
            "sys.event.keyCode = %d; sys.event.ctrlKey = %s; sys.event.shiftKey = %s; sys.event.altKey = %s",
            keyCode, ctrl ? "true" : "false", shift ? "true" : "false", alt ? "true" : "false");
        interp.exec(kcBuf);

        dispatchScriptEvent(el, type, eventObj);
    }

    void dispatchMouseEvent(Element* el, const std::string& type, int x, int y) {
        auto eventObj = Value::Object();
        eventObj->setProperty("type", Value::Str(type));
        eventObj->setProperty("clientX", Value::Num((double)x));
        eventObj->setProperty("clientY", Value::Num((double)y));
        eventObj->setProperty("preventDefault", Value::Native([eventObj](std::vector<ValuePtr>, ValuePtr) -> ValuePtr {
            eventObj->setProperty("defaultPrevented", Value::Bool(true));
            return Value::Undefined();
        }));
        eventObj->setProperty("stopPropagation", Value::Native([eventObj](std::vector<ValuePtr>, ValuePtr) -> ValuePtr {
            eventObj->setProperty("propagationStopped", Value::Bool(true));
            return Value::Undefined();
        }));

        if (el) {
            eventObj->setProperty("target", wrapElement(el->shared_from_this()));
        }

        dispatchScriptEvent(el, type, eventObj);
    }

    void dispatchGlobalEvent(const std::string& type, ValuePtr eventObj) {
        for (auto& gl : globalListeners) {
            if (gl.type == type) {
                for (auto& cb : gl.callbacks) {
                    interp.callFunction(cb, { eventObj });
                }
                break;
            }
        }
    }

    std::string capitalize(const std::string& s) {
        if (s.empty()) return s;
        std::string r = s;
        r[0] = toupper(r[0]);
        return r;
    }

    static std::string UnescapeXML(const std::string& str) {
        std::string res;
        for (size_t i = 0; i < str.size(); i++) {
            if (str[i] == '&') {
                size_t end = str.find(';', i);
                if (end != std::string::npos) {
                    std::string entity = str.substr(i, end - i + 1);
                    if (entity == "&amp;") res += '&';
                    else if (entity == "&lt;") res += '<';
                    else if (entity == "&gt;") res += '>';
                    else if (entity == "&quot;") res += '"';
                    else if (entity == "&apos;") res += '\'';
                    else if (entity.find("&#x") == 0) {
                        char* eEnd;
                        int code = (int)strtol(entity.substr(3, entity.size() - 4).c_str(), &eEnd, 16);
                        if (*eEnd != '\0') { res += entity; }
                        else if (code <= 0x7F) res += (char)code;
                        else if (code <= 0x7FF) { res += (char)(0xC0 | ((code >> 6) & 0x1F)); res += (char)(0x80 | (code & 0x3F)); }
                        else if (code <= 0xFFFF) { res += (char)(0xE0 | ((code >> 12) & 0x0F)); res += (char)(0x80 | ((code >> 6) & 0x3F)); res += (char)(0x80 | (code & 0x3F)); }
                        else { res += entity; }
                    } else if (entity.find("&#") == 0) {
                        char* eEnd;
                        int code = (int)strtol(entity.substr(2, entity.size() - 3).c_str(), &eEnd, 10);
                        if (*eEnd != '\0') { res += entity; }
                        else if (code <= 0x7F) res += (char)code;
                        else if (code <= 0x7FF) { res += (char)(0xC0 | ((code >> 6) & 0x1F)); res += (char)(0x80 | (code & 0x3F)); }
                        else if (code <= 0xFFFF) { res += (char)(0xE0 | ((code >> 12) & 0x0F)); res += (char)(0x80 | ((code >> 6) & 0x3F)); res += (char)(0x80 | (code & 0x3F)); }
                        else { res += entity; }
                    } else res += entity;
                    i = end;
                    continue;
                }
            }
            res += str[i];
        }
        return res;
    }

    ValuePtr wrapElement(std::shared_ptr<Element> el) {
        if (!el) return Value::Null();

        // Check cache
        for (auto& ec : elementCache) {
            if (ec.first == el.get()) return ec.second;
        }

        auto wrapper = Value::Object();
        Element* rawPtr = el.get();
        auto sharedEl = el; // keep alive

        // Properties
        wrapper->setProperty("tag", Value::Str(el->tag));
        wrapper->setProperty("id", Value::Str(el->id));
        wrapper->setProperty("innerText", Value::Str(el->innerText));

        // getAttribute 
        wrapper->setProperty("getAttribute", Value::Native([sharedEl](std::vector<ValuePtr> args, ValuePtr) -> ValuePtr {
            if (args.empty()) return Value::Null();
            std::string key = args[0]->toString();
            if (key == "text" || key == "innerText") return Value::Str(sharedEl->innerText);
            return Value::Str(sharedEl->Get(key));
        }));

        // setAttribute (score 50 so JS overrides CSS rules which use scores 10-40)
        wrapper->setProperty("setAttribute", Value::Native([sharedEl](std::vector<ValuePtr> args, ValuePtr) -> ValuePtr {
            if (args.size() < 2) return Value::Undefined();
            std::string key = args[0]->toString();
            std::string val = args[1]->toString();
            if (key == "text" || key == "innerText") {
                sharedEl->innerText = UnescapeXML(val);
            } else {
                sharedEl->SetProp(key, val, 50);
            }
            if (g_hwnd) InvalidateRect(g_hwnd, NULL, FALSE);
            return Value::Undefined();
        }));

        // removeAttribute
        wrapper->setProperty("removeAttribute", Value::Native([sharedEl](std::vector<ValuePtr> args, ValuePtr) -> ValuePtr {
            if (args.empty()) return Value::Undefined();
            std::string key = args[0]->toString();
            if (key == "text" || key == "innerText") {
                sharedEl->innerText = "";
            } else {
                sharedEl->RemoveProp(key);
            }
            if (g_hwnd) InvalidateRect(g_hwnd, NULL, FALSE);
            return Value::Undefined();
        }));

        // style (property access as object)
        auto styleObj = Value::Object();
        wrapper->setProperty("style", styleObj);

        // classList
        auto classList = Value::Object();
        classList->setProperty("add", Value::Native([sharedEl](std::vector<ValuePtr> args, ValuePtr) -> ValuePtr {
            for (auto& a : args) sharedEl->classes.insert(a->toString());
            if (g_hwnd) InvalidateRect(g_hwnd, NULL, FALSE);
            return Value::Undefined();
        }));
        classList->setProperty("remove", Value::Native([sharedEl](std::vector<ValuePtr> args, ValuePtr) -> ValuePtr {
            for (auto& a : args) sharedEl->classes.erase(a->toString());
            if (g_hwnd) InvalidateRect(g_hwnd, NULL, FALSE);
            return Value::Undefined();
        }));
        classList->setProperty("toggle", Value::Native([sharedEl](std::vector<ValuePtr> args, ValuePtr) -> ValuePtr {
            if (args.empty()) return Value::Undefined();
            std::string cls = args[0]->toString();
            if (sharedEl->classes.count(cls)) sharedEl->classes.erase(cls);
            else sharedEl->classes.insert(cls);
            if (g_hwnd) InvalidateRect(g_hwnd, NULL, FALSE);
            return Value::Undefined();
        }));
        classList->setProperty("contains", Value::Native([sharedEl](std::vector<ValuePtr> args, ValuePtr) -> ValuePtr {
            if (args.empty()) return Value::Bool(false);
            return Value::Bool(sharedEl->classes.count(args[0]->toString()) > 0);
        }));
        wrapper->setProperty("classList", classList);

        // querySelector on element scope
        wrapper->setProperty("querySelector", Value::Native([this, sharedEl](std::vector<ValuePtr> args, ValuePtr) -> ValuePtr {
            if (args.empty()) return Value::Null();
            auto el = Selector::Query(sharedEl, args[0]->toString(), false);
            return el ? wrapElement(el) : Value::Null();
        }));
        wrapper->setProperty("querySelectorAll", Value::Native([this, sharedEl](std::vector<ValuePtr> args, ValuePtr) -> ValuePtr {
            auto arr = Value::Array();
            if (args.empty()) return arr;
            auto els = Selector::QueryAll(sharedEl, args[0]->toString(), false);
            for (auto& e : els) arr->elements.push_back(wrapElement(e));
            arr->prototype = interp.getArrayProto();
            return arr;
        }));
        wrapper->setProperty("shadowDomSelector", Value::Native([this, sharedEl](std::vector<ValuePtr> args, ValuePtr) -> ValuePtr {
            if (args.empty()) return Value::Null();
            auto el = Selector::Query(sharedEl, args[0]->toString(), true);
            return el ? wrapElement(el) : Value::Null();
        }));

        // addEventListener on element
        wrapper->setProperty("addEventListener", Value::Native([this, rawPtr](std::vector<ValuePtr> args, ValuePtr) -> ValuePtr {
            if (args.size() < 2) return Value::Undefined();
            std::string type = args[0]->toString();
            
            bool foundEl = false;
            for (auto& lr : listenerRegistry) {
                if (lr.el == rawPtr) {
                    bool foundType = false;
                    for (auto& ll : lr.lists) {
                        if (ll.type == type) { ll.callbacks.push_back(args[1]); foundType = true; break; }
                    }
                    if (!foundType) {
                        ListenerList ll; ll.type = type; ll.callbacks.push_back(args[1]);
                        lr.lists.push_back(ll);
                    }
                    foundEl = true; break;
                }
            }
            if (!foundEl) {
                ElementListeners lr; lr.el = rawPtr;
                ListenerList ll; ll.type = type; ll.callbacks.push_back(args[1]);
                lr.lists.push_back(ll);
                listenerRegistry.push_back(lr);
            }
            
            return Value::Undefined();
        }));

        // removeEventListener
        wrapper->setProperty("removeEventListener", Value::Native([this, rawPtr](std::vector<ValuePtr> args, ValuePtr) -> ValuePtr {
            if (args.size() < 1) return Value::Undefined();
            std::string type = args[0]->toString();
            for (auto& lr : listenerRegistry) {
                if (lr.el == rawPtr) {
                    for (size_t i = 0; i < lr.lists.size(); i++) {
                        if (lr.lists[i].type == type) {
                            lr.lists.erase(lr.lists.begin() + i); break;
                        }
                    }
                }
            }
            return Value::Undefined();
        }));

        // dispatchEvent
        wrapper->setProperty("dispatchEvent", Value::Native([this, rawPtr](std::vector<ValuePtr> args, ValuePtr) -> ValuePtr {
            if (args.empty()) return Value::Undefined();
            auto eventObj = args[0];
            std::string type = eventObj->getProperty("type")->toString();
            dispatchScriptEvent(rawPtr, type, eventObj);
            return Value::Undefined();
        }));

        // DOM traversal
        wrapper->setProperty("parentElement", Value::Native([this, sharedEl](std::vector<ValuePtr>, ValuePtr) -> ValuePtr {
            if (sharedEl->parent) return wrapElement(sharedEl->parent->shared_from_this());
            if (sharedEl->shadowHost) return wrapElement(sharedEl->shadowHost->shared_from_this());
            return Value::Null();
        }));

        wrapper->setProperty("children", Value::Native([this, sharedEl](std::vector<ValuePtr>, ValuePtr) -> ValuePtr {
            auto arr = Value::Array();
            for (auto& c : sharedEl->children) arr->elements.push_back(wrapElement(c));
            arr->prototype = interp.getArrayProto();
            return arr;
        }));

        wrapper->setProperty("appendChild", Value::Native([this, sharedEl](std::vector<ValuePtr> args, ValuePtr) -> ValuePtr {
            if (args.empty()) return Value::Undefined();
            auto childWrapper = args[0];
            for (auto& ec : elementCache) {
                if (ec.second.get() == childWrapper.get()) {
                    auto childShared = ec.first->shared_from_this();
                    // Remove from previous parent if any
                    if (ec.first->parent) {
                        ec.first->parent->shared_from_this()->RemoveChild(childShared);
                    } else if (ec.first->shadowHost) {
                        ec.first->shadowHost->shared_from_this()->RemoveChild(childShared);
                    }
                    sharedEl->Adopt(childShared, false);
                    break;
                }
            }
            if (g_hwnd) InvalidateRect(g_hwnd, NULL, FALSE);
            return Value::Undefined();
        }));

        wrapper->setProperty("removeChild", Value::Native([this, sharedEl](std::vector<ValuePtr> args, ValuePtr) -> ValuePtr {
            if (args.empty()) return Value::Undefined();
            auto childWrapper = args[0];
            for (auto& ec : elementCache) {
                if (ec.second.get() == childWrapper.get()) {
                    sharedEl->RemoveChild(ec.first->shared_from_this());
                    break;
                }
            }
            if (g_hwnd) InvalidateRect(g_hwnd, NULL, FALSE);
            return Value::Undefined();
        }));

        wrapper->setProperty("clearChildren", Value::Native([sharedEl](std::vector<ValuePtr>, ValuePtr) -> ValuePtr {
            sharedEl->children.clear();
            if (g_hwnd) InvalidateRect(g_hwnd, NULL, FALSE);
            return Value::Undefined();
        }));

        wrapper->setProperty("remove", Value::Native([sharedEl](std::vector<ValuePtr>, ValuePtr) -> ValuePtr {
            if (sharedEl->parent) {
                sharedEl->parent->shared_from_this()->RemoveChild(sharedEl);
            } else if (sharedEl->shadowHost) {
                sharedEl->shadowHost->shared_from_this()->RemoveChild(sharedEl);
            }
            if (g_hwnd) InvalidateRect(g_hwnd, NULL, FALSE);
            return Value::Undefined();
        }));

        // Convenience text property
        wrapper->setProperty("textContent", Value::Str(el->innerText));

        // Layout info
        wrapper->setProperty("getBoundingRect", Value::Native([sharedEl](std::vector<ValuePtr>, ValuePtr) -> ValuePtr {
            auto rect = Value::Object();
            rect->setProperty("x", Value::Num((double)sharedEl->x));
            rect->setProperty("y", Value::Num((double)sharedEl->y));
            rect->setProperty("width", Value::Num((double)sharedEl->w));
            rect->setProperty("height", Value::Num((double)sharedEl->h));
            return rect;
        }));

        // Scroll properties
        wrapper->setProperty("getScrollTop", Value::Native([sharedEl](std::vector<ValuePtr>, ValuePtr) -> ValuePtr {
            return Value::Num((double)sharedEl->scrollY);
        }));
        wrapper->setProperty("getScrollLeft", Value::Native([sharedEl](std::vector<ValuePtr>, ValuePtr) -> ValuePtr {
            return Value::Num((double)sharedEl->scrollX);
        }));
        wrapper->setProperty("getScrollHeight", Value::Native([sharedEl](std::vector<ValuePtr>, ValuePtr) -> ValuePtr {
            return Value::Num((double)sharedEl->contentH);
        }));
        wrapper->setProperty("getScrollWidth", Value::Native([sharedEl](std::vector<ValuePtr>, ValuePtr) -> ValuePtr {
            return Value::Num((double)sharedEl->contentW);
        }));
        wrapper->setProperty("getClientHeight", Value::Native([sharedEl](std::vector<ValuePtr>, ValuePtr) -> ValuePtr {
            return Value::Num((double)sharedEl->h);
        }));
        wrapper->setProperty("getClientWidth", Value::Native([sharedEl](std::vector<ValuePtr>, ValuePtr) -> ValuePtr {
            return Value::Num((double)sharedEl->w);
        }));
        wrapper->setProperty("setScrollTop", Value::Native([sharedEl](std::vector<ValuePtr> args, ValuePtr) -> ValuePtr {
            if (!args.empty()) { sharedEl->scrollY = (float)args[0]->toNumber(); if (g_hwnd) InvalidateRect(g_hwnd, NULL, FALSE); }
            return Value::Undefined();
        }));
        wrapper->setProperty("setScrollLeft", Value::Native([sharedEl](std::vector<ValuePtr> args, ValuePtr) -> ValuePtr {
            if (!args.empty()) { sharedEl->scrollX = (float)args[0]->toNumber(); if (g_hwnd) InvalidateRect(g_hwnd, NULL, FALSE); }
            return Value::Undefined();
        }));

        // Visibility
        wrapper->setProperty("show", Value::Native([sharedEl](std::vector<ValuePtr>, ValuePtr) -> ValuePtr {
            sharedEl->visible = true;
            if (g_hwnd) InvalidateRect(g_hwnd, NULL, FALSE);
            return Value::Undefined();
        }));
        wrapper->setProperty("hide", Value::Native([sharedEl](std::vector<ValuePtr>, ValuePtr) -> ValuePtr {
            sharedEl->visible = false;
            if (g_hwnd) InvalidateRect(g_hwnd, NULL, FALSE);
            return Value::Undefined();
        }));

        // Focus
        wrapper->setProperty("focus", Value::Native([sharedEl](std::vector<ValuePtr>, ValuePtr) -> ValuePtr {
            sharedEl->isFocused = true;
            if (g_hwnd) InvalidateRect(g_hwnd, NULL, FALSE);
            return Value::Undefined();
        }));
        wrapper->setProperty("blur", Value::Native([sharedEl](std::vector<ValuePtr>, ValuePtr) -> ValuePtr {
            sharedEl->isFocused = false;
            if (g_hwnd) InvalidateRect(g_hwnd, NULL, FALSE);
            return Value::Undefined();
        }));

        wrapper->prototype = interp.getObjectProto();
        elementCache.push_back({rawPtr, wrapper});
        return wrapper;
    }
};
