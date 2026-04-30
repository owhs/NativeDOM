#pragma once
#include "AST.h"
#include "Value.h"
#include "Environment.h"
#include "Parser.h"
#include <cmath>
#include <cstdio>
#include <string>
#include <algorithm>

// No exceptions used in AST evaluation

class Interpreter {
    std::shared_ptr<Environment> globalEnv;
    ValuePtr globalThis;
    
    // Prototype objects for builtin types
    ValuePtr stringProto;
    ValuePtr arrayProto;
    ValuePtr numberProto;
    ValuePtr objectProto;
    ValuePtr promiseProto;

    // Async task queue
    struct AsyncTask {
        ValuePtr callback;
        std::vector<ValuePtr> args;
        int delayMs;
        ULONGLONG triggerTime;
        bool repeat;
        int id;
    };
    std::vector<AsyncTask> asyncQueue;
    int nextTimerId = 1;

    // Logger callback (for sys.log integration)
    void (*logCallback)(const std::string&) = nullptr;
    // Invalidate callback (to trigger repaint)
    void (*invalidateCallback)() = nullptr;

public:
    Interpreter() {
        globalEnv = std::make_shared<Environment>();
        globalThis = Value::Object();
        initPrototypes();
    }

    ~Interpreter() {
        if (globalEnv) {
            globalEnv->clear();
        }
    }

    void setLogCallback(void (*cb)(const std::string&)) { logCallback = cb; }
    void setInvalidateCallback(void (*cb)()) { invalidateCallback = cb; }

    std::shared_ptr<Environment> getGlobalEnv() { return globalEnv; }
    ValuePtr getGlobalThis() { return globalThis; }
    ValuePtr getStringProto() { return stringProto; }
    ValuePtr getArrayProto() { return arrayProto; }
    ValuePtr getObjectProto() { return objectProto; }

    void defineGlobal(const std::string& name, ValuePtr val) {
        globalEnv->define(name, val);
    }

    ValuePtr exec(const std::string& source) {
        auto ast = ScriptParser::ParseSource(source);
        if (!ast) return Value::Undefined();
        auto res = evaluate(ast, globalEnv);
        if (res && res->type == ValueType::ControlSignal && res->controlType == ControlType::Throw) {
            std::string msg = "Uncaught exception: " + (res->signalValue ? res->signalValue->toString() : "undefined");
            doLog("JS Error: " + msg);
        }
        return res;
    }

    ValuePtr callFunction(ValuePtr fn, std::vector<ValuePtr> args, ValuePtr thisArg = nullptr) {
        if (!fn) return Value::Undefined();
        if (fn->type == ValueType::NativeFunction) {
            return fn->nativeFn(args, thisArg);
        }
        if (fn->type == ValueType::Function) {
            auto scope = std::make_shared<Environment>(fn->closure);
            if (!fn->isArrow && thisArg) {
                scope->define("this", thisArg);
            }
            // Bind params
            for (size_t i = 0; i < fn->params.size(); i++) {
                auto& p = fn->params[i];
                if (p.substr(0, 3) == "...") {
                    // Rest param
                    auto rest = Value::Array();
                    for (size_t j = i; j < args.size(); j++) rest->elements.push_back(args[j]);
                    scope->define(p.substr(3), rest);
                    break;
                }
                scope->define(p, i < args.size() ? args[i] : Value::Undefined());
            }
            // Bind "arguments" object
            auto argsArray = Value::Array();
            for (auto& a : args) argsArray->elements.push_back(a);
            scope->define("arguments", argsArray);

            auto res = evaluate(fn->body, scope);
            if (res && res->type == ValueType::ControlSignal) {
                if (res->controlType == ControlType::Return) return res->signalValue ? res->signalValue : Value::Undefined();
                return res; // Bubble up
            }
            return res;
        }
        return Value::Undefined();
    }

    // Process pending async tasks (call from main message loop timer)
    void tick() {
        ULONGLONG now = GetTickCount64();
        std::vector<AsyncTask> pending;
        std::vector<AsyncTask> remaining;
        for (auto& t : asyncQueue) {
            if (now >= t.triggerTime) {
                pending.push_back(t);
                if (t.repeat) {
                    t.triggerTime = now + t.delayMs;
                    remaining.push_back(t);
                }
            } else {
                remaining.push_back(t);
            }
        }
        asyncQueue = remaining;
        for (auto& t : pending) {
            callFunction(t.callback, t.args);
        }
    }

    void clearTimer(int id) {
        asyncQueue.erase(
            std::remove_if(asyncQueue.begin(), asyncQueue.end(), [id](const AsyncTask& t) { return t.id == id; }),
            asyncQueue.end()
        );
    }

    int addTimer(ValuePtr callback, std::vector<ValuePtr> args, int delayMs, bool repeat) {
        AsyncTask t;
        t.callback = callback;
        t.args = args;
        t.delayMs = delayMs;
        t.triggerTime = GetTickCount64() + delayMs;
        t.repeat = repeat;
        t.id = nextTimerId++;
        asyncQueue.push_back(t);
        return t.id;
    }

private:
    void doLog(const std::string& msg) {
        if (logCallback) logCallback(msg);
        else printf("%s\n", msg.c_str());
    }

    void initPrototypes() {
        // ---- String.prototype ----
        stringProto = Value::Object();
        
        stringProto->setProperty("charAt", Value::Native([](std::vector<ValuePtr> args, ValuePtr thisArg) -> ValuePtr {
            if (!thisArg || thisArg->type != ValueType::String) return Value::Str("");
            int idx = args.size() > 0 ? (int)args[0]->toNumber() : 0;
            if (idx >= 0 && idx < (int)thisArg->str.size()) return Value::Str(std::string(1, thisArg->str[idx]));
            return Value::Str("");
        }));

        stringProto->setProperty("charCodeAt", Value::Native([](std::vector<ValuePtr> args, ValuePtr thisArg) -> ValuePtr {
            if (!thisArg || thisArg->type != ValueType::String) return Value::Num(std::nan(""));
            int idx = args.size() > 0 ? (int)args[0]->toNumber() : 0;
            if (idx >= 0 && idx < (int)thisArg->str.size()) return Value::Num((double)(unsigned char)thisArg->str[idx]);
            return Value::Num(std::nan(""));
        }));

        stringProto->setProperty("indexOf", Value::Native([](std::vector<ValuePtr> args, ValuePtr thisArg) -> ValuePtr {
            if (!thisArg || args.empty()) return Value::Num(-1);
            auto pos = thisArg->str.find(args[0]->toString());
            return Value::Num(pos == std::string::npos ? -1.0 : (double)pos);
        }));

        stringProto->setProperty("lastIndexOf", Value::Native([](std::vector<ValuePtr> args, ValuePtr thisArg) -> ValuePtr {
            if (!thisArg || args.empty()) return Value::Num(-1);
            auto pos = thisArg->str.rfind(args[0]->toString());
            return Value::Num(pos == std::string::npos ? -1.0 : (double)pos);
        }));

        stringProto->setProperty("includes", Value::Native([](std::vector<ValuePtr> args, ValuePtr thisArg) -> ValuePtr {
            if (!thisArg || args.empty()) return Value::Bool(false);
            return Value::Bool(thisArg->str.find(args[0]->toString()) != std::string::npos);
        }));

        stringProto->setProperty("startsWith", Value::Native([](std::vector<ValuePtr> args, ValuePtr thisArg) -> ValuePtr {
            if (!thisArg || args.empty()) return Value::Bool(false);
            std::string prefix = args[0]->toString();
            return Value::Bool(thisArg->str.substr(0, prefix.size()) == prefix);
        }));

        stringProto->setProperty("endsWith", Value::Native([](std::vector<ValuePtr> args, ValuePtr thisArg) -> ValuePtr {
            if (!thisArg || args.empty()) return Value::Bool(false);
            std::string suffix = args[0]->toString();
            if (suffix.size() > thisArg->str.size()) return Value::Bool(false);
            return Value::Bool(thisArg->str.substr(thisArg->str.size() - suffix.size()) == suffix);
        }));

        stringProto->setProperty("slice", Value::Native([](std::vector<ValuePtr> args, ValuePtr thisArg) -> ValuePtr {
            if (!thisArg) return Value::Str("");
            int len = (int)thisArg->str.size();
            int start = args.size() > 0 ? (int)args[0]->toNumber() : 0;
            int end = args.size() > 1 ? (int)args[1]->toNumber() : len;
            if (start < 0) start = (std::max)(0, len + start);
            if (end < 0) end = (std::max)(0, len + end);
            start = (std::min)(start, len);
            end = (std::min)(end, len);
            if (start >= end) return Value::Str("");
            return Value::Str(thisArg->str.substr(start, end - start));
        }));

        stringProto->setProperty("substring", Value::Native([](std::vector<ValuePtr> args, ValuePtr thisArg) -> ValuePtr {
            if (!thisArg) return Value::Str("");
            int len = (int)thisArg->str.size();
            int start = args.size() > 0 ? (int)args[0]->toNumber() : 0;
            int end = args.size() > 1 ? (int)args[1]->toNumber() : len;
            if (start < 0) start = 0; if (end < 0) end = 0;
            if (start > end) std::swap(start, end);
            start = (std::min)(start, len); end = (std::min)(end, len);
            return Value::Str(thisArg->str.substr(start, end - start));
        }));

        stringProto->setProperty("toUpperCase", Value::Native([](std::vector<ValuePtr> args, ValuePtr thisArg) -> ValuePtr {
            if (!thisArg) return Value::Str("");
            std::string s = thisArg->str;
            std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::toupper(c); });
            return Value::Str(s);
        }));

        stringProto->setProperty("toLowerCase", Value::Native([](std::vector<ValuePtr> args, ValuePtr thisArg) -> ValuePtr {
            if (!thisArg) return Value::Str("");
            std::string s = thisArg->str;
            std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return std::tolower(c); });
            return Value::Str(s);
        }));

        stringProto->setProperty("trim", Value::Native([](std::vector<ValuePtr> args, ValuePtr thisArg) -> ValuePtr {
            if (!thisArg) return Value::Str("");
            std::string s = thisArg->str;
            size_t start = s.find_first_not_of(" \t\r\n");
            size_t end = s.find_last_not_of(" \t\r\n");
            if (start == std::string::npos) return Value::Str("");
            return Value::Str(s.substr(start, end - start + 1));
        }));

        stringProto->setProperty("split", Value::Native([](std::vector<ValuePtr> args, ValuePtr thisArg) -> ValuePtr {
            auto arr = Value::Array();
            if (!thisArg) return arr;
            std::string sep = args.size() > 0 ? args[0]->toString() : "";
            if (sep.empty()) {
                for (char c : thisArg->str) arr->elements.push_back(Value::Str(std::string(1, c)));
                return arr;
            }
            std::string s = thisArg->str;
            size_t pos = 0;
            while ((pos = s.find(sep)) != std::string::npos) {
                arr->elements.push_back(Value::Str(s.substr(0, pos)));
                s = s.substr(pos + sep.size());
            }
            arr->elements.push_back(Value::Str(s));
            return arr;
        }));

        stringProto->setProperty("replace", Value::Native([](std::vector<ValuePtr> args, ValuePtr thisArg) -> ValuePtr {
            if (!thisArg || args.size() < 2) return thisArg ? Value::Str(thisArg->str) : Value::Str("");
            std::string s = thisArg->str;
            std::string from = args[0]->toString();
            std::string to = args[1]->toString();
            size_t pos = s.find(from);
            if (pos != std::string::npos) s.replace(pos, from.size(), to);
            return Value::Str(s);
        }));

        stringProto->setProperty("replaceAll", Value::Native([](std::vector<ValuePtr> args, ValuePtr thisArg) -> ValuePtr {
            if (!thisArg || args.size() < 2) return thisArg ? Value::Str(thisArg->str) : Value::Str("");
            std::string s = thisArg->str;
            std::string from = args[0]->toString();
            std::string to = args[1]->toString();
            size_t pos = 0;
            while ((pos = s.find(from, pos)) != std::string::npos) {
                s.replace(pos, from.size(), to);
                pos += to.size();
            }
            return Value::Str(s);
        }));

        stringProto->setProperty("repeat", Value::Native([](std::vector<ValuePtr> args, ValuePtr thisArg) -> ValuePtr {
            if (!thisArg || args.empty()) return Value::Str("");
            int count = (int)args[0]->toNumber();
            std::string result;
            for (int i = 0; i < count; i++) result += thisArg->str;
            return Value::Str(result);
        }));

        stringProto->setProperty("padStart", Value::Native([](std::vector<ValuePtr> args, ValuePtr thisArg) -> ValuePtr {
            if (!thisArg || args.empty()) return thisArg ? Value::Str(thisArg->str) : Value::Str("");
            int targetLen = (int)args[0]->toNumber();
            std::string padStr = args.size() > 1 ? args[1]->toString() : " ";
            std::string s = thisArg->str;
            while ((int)s.size() < targetLen) s = padStr + s;
            return Value::Str(s.substr(s.size() - targetLen));
        }));

        stringProto->setProperty("match", Value::Native([](std::vector<ValuePtr> args, ValuePtr thisArg) -> ValuePtr {
            if (!thisArg || args.empty()) return Value::Null();
            std::string q = args[0]->toString();
            if (thisArg->str.find(q) != std::string::npos) {
                auto arr = Value::Array();
                arr->elements.push_back(Value::Str(q));
                return arr;
            }
            return Value::Null();
        }));

        // ---- Array.prototype ----
        arrayProto = Value::Object();

        arrayProto->setProperty("push", Value::Native([](std::vector<ValuePtr> args, ValuePtr thisArg) -> ValuePtr {
            if (!thisArg) return Value::Num(0);
            for (auto& a : args) thisArg->elements.push_back(a);
            return Value::Num((double)thisArg->elements.size());
        }));

        arrayProto->setProperty("pop", Value::Native([](std::vector<ValuePtr> args, ValuePtr thisArg) -> ValuePtr {
            if (!thisArg || thisArg->elements.empty()) return Value::Undefined();
            auto val = thisArg->elements.back();
            thisArg->elements.pop_back();
            return val;
        }));

        arrayProto->setProperty("shift", Value::Native([](std::vector<ValuePtr> args, ValuePtr thisArg) -> ValuePtr {
            if (!thisArg || thisArg->elements.empty()) return Value::Undefined();
            auto val = thisArg->elements.front();
            thisArg->elements.erase(thisArg->elements.begin());
            return val;
        }));

        arrayProto->setProperty("unshift", Value::Native([](std::vector<ValuePtr> args, ValuePtr thisArg) -> ValuePtr {
            if (!thisArg) return Value::Num(0);
            for (int i = (int)args.size() - 1; i >= 0; i--) thisArg->elements.insert(thisArg->elements.begin(), args[i]);
            return Value::Num((double)thisArg->elements.size());
        }));

        arrayProto->setProperty("indexOf", Value::Native([](std::vector<ValuePtr> args, ValuePtr thisArg) -> ValuePtr {
            if (!thisArg || args.empty()) return Value::Num(-1);
            for (size_t i = 0; i < thisArg->elements.size(); i++) {
                if (thisArg->elements[i]->looseEquals(args[0])) return Value::Num((double)i);
            }
            return Value::Num(-1);
        }));

        arrayProto->setProperty("includes", Value::Native([](std::vector<ValuePtr> args, ValuePtr thisArg) -> ValuePtr {
            if (!thisArg || args.empty()) return Value::Bool(false);
            for (auto& e : thisArg->elements) {
                if (e->looseEquals(args[0])) return Value::Bool(true);
            }
            return Value::Bool(false);
        }));

        arrayProto->setProperty("join", Value::Native([](std::vector<ValuePtr> args, ValuePtr thisArg) -> ValuePtr {
            if (!thisArg) return Value::Str("");
            std::string sep = args.size() > 0 ? args[0]->toString() : ",";
            std::string result;
            for (size_t i = 0; i < thisArg->elements.size(); i++) {
                if (i > 0) result += sep;
                result += thisArg->elements[i]->toString();
            }
            return Value::Str(result);
        }));

        arrayProto->setProperty("slice", Value::Native([](std::vector<ValuePtr> args, ValuePtr thisArg) -> ValuePtr {
            auto arr = Value::Array();
            if (!thisArg) return arr;
            int len = (int)thisArg->elements.size();
            int start = args.size() > 0 ? (int)args[0]->toNumber() : 0;
            int end = args.size() > 1 ? (int)args[1]->toNumber() : len;
            if (start < 0) start = (std::max)(0, len + start);
            if (end < 0) end = (std::max)(0, len + end);
            for (int i = start; i < end && i < len; i++) arr->elements.push_back(thisArg->elements[i]);
            return arr;
        }));

        arrayProto->setProperty("splice", Value::Native([](std::vector<ValuePtr> args, ValuePtr thisArg) -> ValuePtr {
            auto removed = Value::Array();
            if (!thisArg || args.empty()) return removed;
            int len = (int)thisArg->elements.size();
            int start = (int)args[0]->toNumber();
            if (start < 0) start = (std::max)(0, len + start);
            int deleteCount = args.size() > 1 ? (int)args[1]->toNumber() : len - start;
            for (int i = 0; i < deleteCount && start < (int)thisArg->elements.size(); i++) {
                removed->elements.push_back(thisArg->elements[start]);
                thisArg->elements.erase(thisArg->elements.begin() + start);
            }
            for (size_t i = 2; i < args.size(); i++) {
                thisArg->elements.insert(thisArg->elements.begin() + start + (i - 2), args[i]);
            }
            return removed;
        }));

        arrayProto->setProperty("reverse", Value::Native([](std::vector<ValuePtr> args, ValuePtr thisArg) -> ValuePtr {
            if (thisArg) std::reverse(thisArg->elements.begin(), thisArg->elements.end());
            return thisArg;
        }));

        arrayProto->setProperty("concat", Value::Native([](std::vector<ValuePtr> args, ValuePtr thisArg) -> ValuePtr {
            auto arr = Value::Array();
            if (thisArg) for (auto& e : thisArg->elements) arr->elements.push_back(e);
            for (auto& a : args) {
                if (a->type == ValueType::Array) for (auto& e : a->elements) arr->elements.push_back(e);
                else arr->elements.push_back(a);
            }
            return arr;
        }));

        arrayProto->setProperty("forEach", Value::Native([this](std::vector<ValuePtr> args, ValuePtr thisArg) -> ValuePtr {
            if (!thisArg || args.empty()) return Value::Undefined();
            for (size_t i = 0; i < thisArg->elements.size(); i++) {
                callFunction(args[0], { thisArg->elements[i], Value::Num((double)i), thisArg });
            }
            return Value::Undefined();
        }));

        arrayProto->setProperty("map", Value::Native([this](std::vector<ValuePtr> args, ValuePtr thisArg) -> ValuePtr {
            auto arr = Value::Array();
            if (!thisArg || args.empty()) return arr;
            for (size_t i = 0; i < thisArg->elements.size(); i++) {
                arr->elements.push_back(callFunction(args[0], { thisArg->elements[i], Value::Num((double)i), thisArg }));
            }
            return arr;
        }));

        arrayProto->setProperty("filter", Value::Native([this](std::vector<ValuePtr> args, ValuePtr thisArg) -> ValuePtr {
            auto arr = Value::Array();
            if (!thisArg || args.empty()) return arr;
            for (size_t i = 0; i < thisArg->elements.size(); i++) {
                auto result = callFunction(args[0], { thisArg->elements[i], Value::Num((double)i), thisArg });
                if (result && result->isTruthy()) arr->elements.push_back(thisArg->elements[i]);
            }
            return arr;
        }));

        arrayProto->setProperty("reduce", Value::Native([this](std::vector<ValuePtr> args, ValuePtr thisArg) -> ValuePtr {
            if (!thisArg || args.empty()) return Value::Undefined();
            ValuePtr acc = args.size() > 1 ? args[1] : (thisArg->elements.empty() ? Value::Undefined() : thisArg->elements[0]);
            size_t start = args.size() > 1 ? 0 : 1;
            for (size_t i = start; i < thisArg->elements.size(); i++) {
                acc = callFunction(args[0], { acc, thisArg->elements[i], Value::Num((double)i), thisArg });
            }
            return acc;
        }));

        arrayProto->setProperty("find", Value::Native([this](std::vector<ValuePtr> args, ValuePtr thisArg) -> ValuePtr {
            if (!thisArg || args.empty()) return Value::Undefined();
            for (size_t i = 0; i < thisArg->elements.size(); i++) {
                auto r = callFunction(args[0], { thisArg->elements[i], Value::Num((double)i), thisArg });
                if (r && r->isTruthy()) return thisArg->elements[i];
            }
            return Value::Undefined();
        }));

        arrayProto->setProperty("findIndex", Value::Native([this](std::vector<ValuePtr> args, ValuePtr thisArg) -> ValuePtr {
            if (!thisArg || args.empty()) return Value::Num(-1);
            for (size_t i = 0; i < thisArg->elements.size(); i++) {
                auto r = callFunction(args[0], { thisArg->elements[i], Value::Num((double)i), thisArg });
                if (r && r->isTruthy()) return Value::Num((double)i);
            }
            return Value::Num(-1);
        }));

        arrayProto->setProperty("some", Value::Native([this](std::vector<ValuePtr> args, ValuePtr thisArg) -> ValuePtr {
            if (!thisArg || args.empty()) return Value::Bool(false);
            for (size_t i = 0; i < thisArg->elements.size(); i++) {
                auto r = callFunction(args[0], { thisArg->elements[i], Value::Num((double)i), thisArg });
                if (r && r->isTruthy()) return Value::Bool(true);
            }
            return Value::Bool(false);
        }));

        arrayProto->setProperty("every", Value::Native([this](std::vector<ValuePtr> args, ValuePtr thisArg) -> ValuePtr {
            if (!thisArg || args.empty()) return Value::Bool(true);
            for (size_t i = 0; i < thisArg->elements.size(); i++) {
                auto r = callFunction(args[0], { thisArg->elements[i], Value::Num((double)i), thisArg });
                if (!r || !r->isTruthy()) return Value::Bool(false);
            }
            return Value::Bool(true);
        }));

        arrayProto->setProperty("flat", Value::Native([](std::vector<ValuePtr> args, ValuePtr thisArg) -> ValuePtr {
            auto arr = Value::Array();
            if (!thisArg) return arr;
            for (auto& e : thisArg->elements) {
                if (e->type == ValueType::Array) { for (auto& inner : e->elements) arr->elements.push_back(inner); }
                else arr->elements.push_back(e);
            }
            return arr;
        }));

        arrayProto->setProperty("sort", Value::Native([this](std::vector<ValuePtr> args, ValuePtr thisArg) -> ValuePtr {
            if (!thisArg) return Value::Undefined();
            if (args.size() > 0 && args[0]->type == ValueType::Function) {
                auto compareFn = args[0];
                std::sort(thisArg->elements.begin(), thisArg->elements.end(),
                    [&](ValuePtr a, ValuePtr b) -> bool {
                        auto r = callFunction(compareFn, { a, b });
                        return r->toNumber() < 0;
                    });
            } else {
                std::sort(thisArg->elements.begin(), thisArg->elements.end(),
                    [](ValuePtr a, ValuePtr b) { return a->toString() < b->toString(); });
            }
            return thisArg;
        }));

        // ---- Number.prototype ----
        numberProto = Value::Object();

        numberProto->setProperty("toFixed", Value::Native([](std::vector<ValuePtr> args, ValuePtr thisArg) -> ValuePtr {
            if (!thisArg) return Value::Str("0");
            int digits = args.size() > 0 ? (int)args[0]->toNumber() : 0;
            char buf[64];
            snprintf(buf, sizeof(buf), "%.*f", digits, thisArg->number);
            return Value::Str(buf);
        }));

        numberProto->setProperty("toString", Value::Native([](std::vector<ValuePtr> args, ValuePtr thisArg) -> ValuePtr {
            if (!thisArg) return Value::Str("0");
            if (args.size() > 0) {
                int radix = (int)args[0]->toNumber();
                if (radix == 16) {
                    char buf[32];
                    snprintf(buf, sizeof(buf), "%x", (unsigned int)thisArg->number);
                    return Value::Str(buf);
                }
            }
            return Value::Str(thisArg->toString());
        }));

        // ---- Object prototype ----
        objectProto = Value::Object();
        objectProto->setProperty("hasOwnProperty", Value::Native([](std::vector<ValuePtr> args, ValuePtr thisArg) -> ValuePtr {
            if (!thisArg || args.empty()) return Value::Bool(false);
            std::string key = args[0]->toString();
            bool has = false;
            for (auto& p : thisArg->properties) { if (p.first == key) { has = true; break; } }
            return Value::Bool(has);
        }));

        // ---- Promise prototype ----
        promiseProto = Value::Object();
        promiseProto->setProperty("then", Value::Native([this](std::vector<ValuePtr> args, ValuePtr thisArg) -> ValuePtr {
            if (!thisArg || thisArg->type != ValueType::Promise) return Value::Undefined();
            auto onFulfilled = args.size() > 0 ? args[0] : Value::Undefined();
            auto onRejected = args.size() > 1 ? args[1] : Value::Undefined();
            
            if (thisArg->promiseState == Value::PromiseState::Fulfilled && onFulfilled->type == ValueType::Function) {
                return callFunction(onFulfilled, { thisArg->promiseResult });
            }
            if (thisArg->promiseState == Value::PromiseState::Rejected && onRejected->type == ValueType::Function) {
                return callFunction(onRejected, { thisArg->promiseResult });
            }
            // Store for later resolution
            thisArg->thenCallbacks.push_back({ onFulfilled, onRejected });
            return thisArg;
        }));
        promiseProto->setProperty("catch", Value::Native([this](std::vector<ValuePtr> args, ValuePtr thisArg) -> ValuePtr {
            if (!thisArg || thisArg->type != ValueType::Promise) return Value::Undefined();
            auto onRejected = args.size() > 0 ? args[0] : Value::Undefined();
            if (thisArg->promiseState == Value::PromiseState::Rejected && onRejected->type == ValueType::Function) {
                return callFunction(onRejected, { thisArg->promiseResult });
            }
            thisArg->catchCallbacks.push_back(onRejected);
            return thisArg;
        }));

        // ---- Global builtins ----

        // console.log
        auto console = Value::Object();
        console->setProperty("log", Value::Native([this](std::vector<ValuePtr> args, ValuePtr) -> ValuePtr {
            std::string msg;
            for (size_t i = 0; i < args.size(); i++) {
                if (i > 0) msg += " ";
                msg += args[i]->toString();
            }
            doLog(msg);
            return Value::Undefined();
        }));
        globalEnv->define("console", console);

        // parseInt / parseFloat
        globalEnv->define("parseInt", Value::Native([](std::vector<ValuePtr> args, ValuePtr) -> ValuePtr {
            if (args.empty()) return Value::Num(std::nan(""));
            std::string s = args[0]->toString();
            int radix = args.size() > 1 ? (int)args[1]->toNumber() : 10;
            char* end;
            long val = strtol(s.c_str(), &end, radix);
            if (end == s.c_str()) return Value::Num(std::nan(""));
            return Value::Num((double)val);
        }));
        globalEnv->define("parseFloat", Value::Native([](std::vector<ValuePtr> args, ValuePtr) -> ValuePtr {
            if (args.empty()) return Value::Num(std::nan(""));
            char* end;
            double val = strtod(args[0]->toString().c_str(), &end);
            if (end == args[0]->toString().c_str()) return Value::Num(std::nan(""));
            return Value::Num(val);
        }));
        globalEnv->define("isNaN", Value::Native([](std::vector<ValuePtr> args, ValuePtr) -> ValuePtr {
            return Value::Bool(args.empty() || std::isnan(args[0]->toNumber()));
        }));
        globalEnv->define("isFinite", Value::Native([](std::vector<ValuePtr> args, ValuePtr) -> ValuePtr {
            return Value::Bool(!args.empty() && std::isfinite(args[0]->toNumber()));
        }));

        // String.fromCharCode
        auto stringConstructor = Value::Object();
        stringConstructor->setProperty("fromCharCode", Value::Native([](std::vector<ValuePtr> args, ValuePtr) -> ValuePtr {
            std::string s;
            for (auto& a : args) s += (char)(int)a->toNumber();
            return Value::Str(s);
        }));
        globalEnv->define("String", stringConstructor);

        // Math
        auto math = Value::Object();
        math->setProperty("PI", Value::Num(3.14159265358979323846));
        math->setProperty("E", Value::Num(2.71828182845904523536));
        math->setProperty("floor", Value::Native([](std::vector<ValuePtr> a, ValuePtr) { return Value::Num(std::floor(a[0]->toNumber())); }));
        math->setProperty("ceil", Value::Native([](std::vector<ValuePtr> a, ValuePtr) { return Value::Num(std::ceil(a[0]->toNumber())); }));
        math->setProperty("round", Value::Native([](std::vector<ValuePtr> a, ValuePtr) { return Value::Num(std::round(a[0]->toNumber())); }));
        math->setProperty("abs", Value::Native([](std::vector<ValuePtr> a, ValuePtr) { return Value::Num(std::abs(a[0]->toNumber())); }));
        math->setProperty("sqrt", Value::Native([](std::vector<ValuePtr> a, ValuePtr) { return Value::Num(std::sqrt(a[0]->toNumber())); }));
        math->setProperty("pow", Value::Native([](std::vector<ValuePtr> a, ValuePtr) { return Value::Num(std::pow(a[0]->toNumber(), a[1]->toNumber())); }));
        math->setProperty("min", Value::Native([](std::vector<ValuePtr> a, ValuePtr) {
            double m = a.empty() ? INFINITY : a[0]->toNumber();
            for (size_t i = 1; i < a.size(); i++) m = (std::min)(m, a[i]->toNumber());
            return Value::Num(m);
        }));
        math->setProperty("max", Value::Native([](std::vector<ValuePtr> a, ValuePtr) {
            double m = a.empty() ? -INFINITY : a[0]->toNumber();
            for (size_t i = 1; i < a.size(); i++) m = (std::max)(m, a[i]->toNumber());
            return Value::Num(m);
        }));
        math->setProperty("random", Value::Native([](std::vector<ValuePtr>, ValuePtr) {
            return Value::Num((double)rand() / RAND_MAX);
        }));
        math->setProperty("sin", Value::Native([](std::vector<ValuePtr> a, ValuePtr) { return Value::Num(std::sin(a[0]->toNumber())); }));
        math->setProperty("cos", Value::Native([](std::vector<ValuePtr> a, ValuePtr) { return Value::Num(std::cos(a[0]->toNumber())); }));
        math->setProperty("tan", Value::Native([](std::vector<ValuePtr> a, ValuePtr) { return Value::Num(std::tan(a[0]->toNumber())); }));
        math->setProperty("log", Value::Native([](std::vector<ValuePtr> a, ValuePtr) { return Value::Num(std::log(a[0]->toNumber())); }));
        math->setProperty("trunc", Value::Native([](std::vector<ValuePtr> a, ValuePtr) { return Value::Num(std::trunc(a[0]->toNumber())); }));
        globalEnv->define("Math", math);

        // JSON.stringify / JSON.parse
        auto jsonObj = Value::Object();
        jsonObj->setProperty("stringify", Value::Native([this](std::vector<ValuePtr> args, ValuePtr) -> ValuePtr {
            if (args.empty()) return Value::Str("undefined");
            return Value::Str(jsonStringify(args[0]));
        }));
        jsonObj->setProperty("parse", Value::Native([this](std::vector<ValuePtr> args, ValuePtr) -> ValuePtr {
            if (args.empty()) return Value::Undefined();
            return jsonParse(args[0]->toString());
        }));
        globalEnv->define("JSON", jsonObj);

        // Object.keys / Object.values / Object.assign
        auto objectConstructor = Value::Object();
        objectConstructor->setProperty("keys", Value::Native([](std::vector<ValuePtr> args, ValuePtr) -> ValuePtr {
            auto arr = Value::Array();
            if (args.empty() || !args[0]) return arr;
            for (auto& kv : args[0]->properties) arr->elements.push_back(Value::Str(kv.first));
            return arr;
        }));
        objectConstructor->setProperty("values", Value::Native([](std::vector<ValuePtr> args, ValuePtr) -> ValuePtr {
            auto arr = Value::Array();
            if (args.empty() || !args[0]) return arr;
            for (auto& kv : args[0]->properties) arr->elements.push_back(kv.second);
            return arr;
        }));
        objectConstructor->setProperty("entries", Value::Native([](std::vector<ValuePtr> args, ValuePtr) -> ValuePtr {
            auto arr = Value::Array();
            if (args.empty() || !args[0]) return arr;
            for (auto& kv : args[0]->properties) {
                auto pair = Value::Array();
                pair->elements.push_back(Value::Str(kv.first));
                pair->elements.push_back(kv.second);
                arr->elements.push_back(pair);
            }
            return arr;
        }));
        objectConstructor->setProperty("assign", Value::Native([](std::vector<ValuePtr> args, ValuePtr) -> ValuePtr {
            if (args.empty()) return Value::Object();
            auto target = args[0];
            for (size_t i = 1; i < args.size(); i++) {
                if (args[i] && args[i]->type == ValueType::Object) {
                    for (auto& kv : args[i]->properties) target->setProperty(kv.first, kv.second);
                }
            }
            return target;
        }));
        globalEnv->define("Object", objectConstructor);

        // Array.isArray + Array.from
        auto arrayConstructor = Value::Object();
        arrayConstructor->setProperty("isArray", Value::Native([](std::vector<ValuePtr> args, ValuePtr) -> ValuePtr {
            if (args.empty()) return Value::Bool(false);
            return Value::Bool(args[0]->type == ValueType::Array);
        }));
        arrayConstructor->setProperty("from", Value::Native([](std::vector<ValuePtr> args, ValuePtr) -> ValuePtr {
            auto arr = Value::Array();
            if (!args.empty() && args[0]->type == ValueType::Array) {
                arr->elements = args[0]->elements;
            } else if (!args.empty() && args[0]->type == ValueType::String) {
                for (char c : args[0]->str) arr->elements.push_back(Value::Str(std::string(1, c)));
            }
            return arr;
        }));
        globalEnv->define("Array", arrayConstructor);

        // setTimeout / setInterval / clearTimeout / clearInterval
        globalEnv->define("setTimeout", Value::Native([this](std::vector<ValuePtr> args, ValuePtr) -> ValuePtr {
            if (args.size() < 1) return Value::Num(0);
            int delay = args.size() > 1 ? (int)args[1]->toNumber() : 0;
            int id = nextTimerId++;
            asyncQueue.push_back({ args[0], {}, delay, GetTickCount64() + delay, false, id });
            return Value::Num((double)id);
        }));
        globalEnv->define("setInterval", Value::Native([this](std::vector<ValuePtr> args, ValuePtr) -> ValuePtr {
            if (args.size() < 2) return Value::Num(0);
            int delay = (int)args[1]->toNumber();
            int id = nextTimerId++;
            asyncQueue.push_back({ args[0], {}, delay, GetTickCount64() + delay, true, id });
            return Value::Num((double)id);
        }));
        globalEnv->define("clearTimeout", Value::Native([this](std::vector<ValuePtr> args, ValuePtr) -> ValuePtr {
            if (!args.empty()) clearTimer((int)args[0]->toNumber());
            return Value::Undefined();
        }));
        globalEnv->define("clearInterval", Value::Native([this](std::vector<ValuePtr> args, ValuePtr) -> ValuePtr {
            if (!args.empty()) clearTimer((int)args[0]->toNumber());
            return Value::Undefined();
        }));

        // Promise constructor
        globalEnv->define("Promise", Value::Native([this](std::vector<ValuePtr> args, ValuePtr) -> ValuePtr {
            auto promise = Value::MakePromise();
            promise->prototype = promiseProto;
            if (args.size() > 0 && (args[0]->type == ValueType::Function || args[0]->type == ValueType::NativeFunction)) {
                auto resolveFn = Value::Native([this, promise](std::vector<ValuePtr> rArgs, ValuePtr) -> ValuePtr {
                    promise->promiseState = Value::PromiseState::Fulfilled;
                    promise->promiseResult = rArgs.empty() ? Value::Undefined() : rArgs[0];
                    for (auto& cb : promise->thenCallbacks) {
                        if (cb.first && cb.first->type == ValueType::Function) {
                            callFunction(cb.first, { promise->promiseResult });
                        }
                    }
                    for (auto& cb : promise->finallyCallbacks) {
                        if (cb && cb->type == ValueType::Function) callFunction(cb, {});
                    }
                    return Value::Undefined();
                });
                auto rejectFn = Value::Native([this, promise](std::vector<ValuePtr> rArgs, ValuePtr) -> ValuePtr {
                    promise->promiseState = Value::PromiseState::Rejected;
                    promise->promiseResult = rArgs.empty() ? Value::Undefined() : rArgs[0];
                    for (auto& cb : promise->thenCallbacks) {
                        if (cb.second && cb.second->type == ValueType::Function) {
                            callFunction(cb.second, { promise->promiseResult });
                        }
                    }
                    for (auto& cb : promise->catchCallbacks) {
                        if (cb && cb->type == ValueType::Function) callFunction(cb, { promise->promiseResult });
                    }
                    for (auto& cb : promise->finallyCallbacks) {
                        if (cb && cb->type == ValueType::Function) callFunction(cb, {});
                    }
                    return Value::Undefined();
                });
                callFunction(args[0], { resolveFn, rejectFn });
            }
            return promise;
        }));
    }

    // ---- JSON helpers ----
    std::string jsonStringify(ValuePtr val, int indent = 0) {
        if (!val) return "null";
        switch (val->type) {
            case ValueType::Null:
            case ValueType::Undefined: return "null";
            case ValueType::Boolean: return val->boolean ? "true" : "false";
            case ValueType::Number: return val->toString();
            case ValueType::String: return "\"" + escapeJsonStr(val->str) + "\"";
            case ValueType::Array: {
                std::string s = "[";
                for (size_t i = 0; i < val->elements.size(); i++) {
                    if (i > 0) s += ",";
                    s += jsonStringify(val->elements[i]);
                }
                return s + "]";
            }
            case ValueType::Object: {
                std::string s = "{";
                bool first = true;
                for (auto& kv : val->properties) {
                    if (!first) s += ",";
                    s += "\"" + escapeJsonStr(kv.first) + "\":" + jsonStringify(kv.second);
                    first = false;
                }
                return s + "}";
            }
            default: return "null";
        }
    }

    std::string escapeJsonStr(const std::string& s) {
        std::string r;
        for (char c : s) {
            switch (c) {
                case '"': r += "\\\""; break;
                case '\\': r += "\\\\"; break;
                case '\n': r += "\\n"; break;
                case '\r': r += "\\r"; break;
                case '\t': r += "\\t"; break;
                default: r += c;
            }
        }
        return r;
    }

    ValuePtr jsonParse(const std::string& s) {
        // Simple recursive descent JSON parser
        size_t i = 0;
        return jsonParseValue(s, i);
    }

    void jsonSkipWS(const std::string& s, size_t& i) {
        while (i < s.size() && (s[i] == ' ' || s[i] == '\n' || s[i] == '\r' || s[i] == '\t')) i++;
    }

    ValuePtr jsonParseValue(const std::string& s, size_t& i) {
        jsonSkipWS(s, i);
        if (i >= s.size()) return Value::Null();
        char c = s[i];
        if (c == '"') return jsonParseString(s, i);
        if (c == '{') return jsonParseObject(s, i);
        if (c == '[') return jsonParseArray(s, i);
        if (c == 't') { i += 4; return Value::Bool(true); }
        if (c == 'f') { i += 5; return Value::Bool(false); }
        if (c == 'n') { i += 4; return Value::Null(); }
        // Number
        std::string num;
        while (i < s.size() && (std::isdigit(s[i]) || s[i] == '-' || s[i] == '.' || s[i] == 'e' || s[i] == 'E' || s[i] == '+')) num += s[i++];
        char* end; double d = strtod(num.c_str(), &end);
        if (end == num.c_str()) return Value::Null();
        return Value::Num(d);
    }

    ValuePtr jsonParseString(const std::string& s, size_t& i) {
        i++; // skip "
        std::string result;
        while (i < s.size() && s[i] != '"') {
            if (s[i] == '\\') {
                i++;
                switch (s[i]) {
                    case 'n': result += '\n'; break;
                    case 't': result += '\t'; break;
                    case 'r': result += '\r'; break;
                    case '"': result += '"'; break;
                    case '\\': result += '\\'; break;
                    default: result += s[i];
                }
            } else result += s[i];
            i++;
        }
        if (i < s.size()) i++; // skip closing "
        return Value::Str(result);
    }

    ValuePtr jsonParseObject(const std::string& s, size_t& i) {
        i++; // {
        auto obj = Value::Object();
        jsonSkipWS(s, i);
        while (i < s.size() && s[i] != '}') {
            jsonSkipWS(s, i);
            auto key = jsonParseString(s, i);
            jsonSkipWS(s, i);
            if (i < s.size() && s[i] == ':') i++;
            auto val = jsonParseValue(s, i);
            obj->setProperty(key->str, val);
            jsonSkipWS(s, i);
            if (i < s.size() && s[i] == ',') i++;
        }
        if (i < s.size()) i++; // }
        return obj;
    }

    ValuePtr jsonParseArray(const std::string& s, size_t& i) {
        i++; // [
        auto arr = Value::Array();
        jsonSkipWS(s, i);
        while (i < s.size() && s[i] != ']') {
            arr->elements.push_back(jsonParseValue(s, i));
            jsonSkipWS(s, i);
            if (i < s.size() && s[i] == ',') i++;
        }
        if (i < s.size()) i++; // ]
        return arr;
    }

    // ---- Main evaluate ----
    ValuePtr evaluate(std::shared_ptr<ASTNode> node, std::shared_ptr<Environment> env) {
        if (!node) return Value::Undefined();

        switch (node->type) {
            case NodeType::Program:
            case NodeType::Block: {
                ValuePtr last = Value::Undefined();
                for (auto& child : node->children) {
                    last = evaluate(child, env);
                    if (last && last->type == ValueType::ControlSignal) return last;
                }
                return last;
            }

            case NodeType::NumberLit: return Value::Num(node->numValue);
            case NodeType::StringLit: return Value::Str(node->strValue);
            case NodeType::BoolLit: return Value::Bool(node->boolValue);
            case NodeType::NullLit: return Value::Null();
            case NodeType::UndefinedLit: return Value::Undefined();

            case NodeType::TemplateLit: {
                // Process ${...} interpolation
                std::string result;
                std::string raw = node->strValue;
                size_t i = 0;
                while (i < raw.size()) {
                    if (raw[i] == '$' && i + 1 < raw.size() && raw[i + 1] == '{') {
                        i += 2;
                        std::string expr;
                        int depth = 1;
                        while (i < raw.size() && depth > 0) {
                            if (raw[i] == '{') depth++;
                            else if (raw[i] == '}') { depth--; if (depth == 0) break; }
                            expr += raw[i++];
                        }
                        i++; // skip }
                        auto val = exec(expr);
                        result += val->toString();
                    } else {
                        result += raw[i++];
                    }
                }
                return Value::Str(result);
            }

            case NodeType::Identifier: {
                return env->get(node->strValue);
            }

            case NodeType::ThisExpr: {
                return env->has("this") ? env->get("this") : globalThis;
            }

            case NodeType::ArrayLit: {
                auto arr = Value::Array();
                arr->prototype = arrayProto;
                for (auto& child : node->children) {
                    if (child->type == NodeType::SpreadExpr) {
                        auto spread = evaluate(child->left, env);
                        if (spread->type == ValueType::Array) {
                            for (auto& e : spread->elements) arr->elements.push_back(e);
                        }
                    } else {
                        arr->elements.push_back(evaluate(child, env));
                    }
                }
                return arr;
            }

            case NodeType::ObjectLit: {
                auto obj = Value::Object();
                obj->prototype = objectProto;
                for (auto& prop : node->children) {
                    std::string key;
                    if (prop->computed) {
                        key = evaluate(prop->left, env)->toString();
                    } else {
                        key = prop->left->strValue;
                    }
                    obj->setProperty(key, evaluate(prop->right, env));
                }
                return obj;
            }

            case NodeType::VarDecl: {
                for (auto& child : node->children) {
                    ValuePtr val = child->init ? evaluate(child->init, env) : Value::Undefined();
                    env->define(child->strValue, val);
                }
                return Value::Undefined();
            }

            case NodeType::ExprStmt: {
                return evaluate(node->left, env);
            }

            case NodeType::AssignExpr: {
                ValuePtr val = evaluate(node->right, env);
                
                if (node->left->type == NodeType::Identifier) {
                    std::string name = node->left->strValue;
                    if (node->strValue == "=") {
                        env->set(name, val);
                    } else {
                        ValuePtr cur = env->get(name);
                        val = compoundAssign(node->strValue, cur, val);
                        env->set(name, val);
                    }
                    return val;
                }
                
                // Member assignment: obj.prop = val or obj[expr] = val
                if (node->left->type == NodeType::MemberExpr || node->left->type == NodeType::ComputedMemberExpr) {
                    ValuePtr obj = evaluate(node->left->object, env);
                    std::string key;
                    if (node->left->type == NodeType::ComputedMemberExpr) {
                        key = evaluate(node->left->property, env)->toString();
                    } else {
                        key = node->left->property->strValue;
                    }
                    
                    if (node->strValue != "=") {
                        ValuePtr cur = obj->getProperty(key);
                        val = compoundAssign(node->strValue, cur, val);
                    }
                    obj->setProperty(key, val);
                    if (invalidateCallback) invalidateCallback();
                    return val;
                }
                return val;
            }

            case NodeType::BinaryExpr: {
                auto op = node->strValue;
                
                // Null coalescing
                if (op == "??") {
                    auto left = evaluate(node->left, env);
                    if (left->type == ValueType::Null || left->type == ValueType::Undefined)
                        return evaluate(node->right, env);
                    return left;
                }

                auto left = evaluate(node->left, env);
                auto right = evaluate(node->right, env);

                // String concatenation
                if (op == "+" && (left->type == ValueType::String || right->type == ValueType::String)) {
                    return Value::Str(left->toString() + right->toString());
                }

                double l = left->toNumber(), r = right->toNumber();
                if (op == "+") return Value::Num(l + r);
                if (op == "-") return Value::Num(l - r);
                if (op == "*") return Value::Num(l * r);
                if (op == "/") return Value::Num(r != 0 ? l / r : std::nan(""));
                if (op == "%") return Value::Num(r != 0 ? std::fmod(l, r) : std::nan(""));
                if (op == "&") return Value::Num((double)((int)l & (int)r));
                if (op == "|") return Value::Num((double)((int)l | (int)r));
                if (op == "^") return Value::Num((double)((int)l ^ (int)r));
                if (op == "<<") return Value::Num((double)((int)l << (int)r));
                if (op == ">>") return Value::Num((double)((int)l >> (int)r));
                if (op == "<") return Value::Bool(l < r);
                if (op == ">") return Value::Bool(l > r);
                if (op == "<=") return Value::Bool(l <= r);
                if (op == ">=") return Value::Bool(l >= r);
                if (op == "==") return Value::Bool(left->looseEquals(right));
                if (op == "!=") return Value::Bool(!left->looseEquals(right));
                if (op == "===") return Value::Bool(left->strictEquals(right));
                if (op == "!==") return Value::Bool(!left->strictEquals(right));

                return Value::Undefined();
            }

            case NodeType::LogicalExpr: {
                auto left = evaluate(node->left, env);
                if (node->strValue == "&&") return left->isTruthy() ? evaluate(node->right, env) : left;
                if (node->strValue == "||") return left->isTruthy() ? left : evaluate(node->right, env);
                return left;
            }

            case NodeType::UnaryExpr: {
                auto operand = evaluate(node->left, env);
                if (node->strValue == "-") return Value::Num(-operand->toNumber());
                if (node->strValue == "+") return Value::Num(operand->toNumber());
                if (node->strValue == "!") return Value::Bool(!operand->isTruthy());
                if (node->strValue == "~") return Value::Num((double)(~(int)operand->toNumber()));
                return operand;
            }

            case NodeType::TypeofExpr: {
                auto val = evaluate(node->left, env);
                return Value::Str(val->typeStr());
            }

            case NodeType::VoidExpr: {
                evaluate(node->left, env);
                return Value::Undefined();
            }

            case NodeType::UpdateExpr: {
                if (node->left->type == NodeType::Identifier) {
                    auto cur = env->get(node->left->strValue);
                    double n = cur->toNumber();
                    double newVal = node->strValue == "++" ? n + 1 : n - 1;
                    env->set(node->left->strValue, Value::Num(newVal));
                    return node->isPrefix ? Value::Num(newVal) : Value::Num(n);
                }
                // Member update
                if (node->left->type == NodeType::MemberExpr || node->left->type == NodeType::ComputedMemberExpr) {
                    auto obj = evaluate(node->left->object, env);
                    std::string key = node->left->type == NodeType::ComputedMemberExpr 
                        ? evaluate(node->left->property, env)->toString() 
                        : node->left->property->strValue;
                    double n = obj->getProperty(key)->toNumber();
                    double newVal = node->strValue == "++" ? n + 1 : n - 1;
                    obj->setProperty(key, Value::Num(newVal));
                    return node->isPrefix ? Value::Num(newVal) : Value::Num(n);
                }
                return Value::Num(0);
            }

            case NodeType::ConditionalExpr: {
                auto test = evaluate(node->test, env);
                return test->isTruthy() ? evaluate(node->body, env) : evaluate(node->alt, env);
            }

            case NodeType::MemberExpr: {
                auto obj = evaluate(node->object, env);
                if (!obj || obj->type == ValueType::Undefined || obj->type == ValueType::Null) {
                    if (node->strValue == "?.") return Value::Undefined();
                    return Value::Undefined();
                }
                std::string key = node->property->strValue;
                
                // Auto-wrap primitives with their prototypes
                if (obj->type == ValueType::String) {
                    auto val = obj->getProperty(key);
                    if (val->type == ValueType::Undefined) {
                        val = stringProto->getProperty(key);
                    }
                    return val;
                }
                if (obj->type == ValueType::Number) {
                    auto val = obj->getProperty(key);
                    if (val->type == ValueType::Undefined) {
                        val = numberProto->getProperty(key);
                    }
                    return val;
                }
                if (obj->type == ValueType::Array) {
                    auto val = obj->getProperty(key);
                    if (val->type == ValueType::Undefined) {
                        val = arrayProto->getProperty(key);
                    }
                    return val;
                }
                return obj->getProperty(key);
            }

            case NodeType::ComputedMemberExpr: {
                auto obj = evaluate(node->object, env);
                auto key = evaluate(node->property, env);
                if (!obj || obj->type == ValueType::Undefined || obj->type == ValueType::Null)
                    return Value::Undefined();
                return obj->getProperty(key->toString());
            }

            case NodeType::CallExpr: {
                ValuePtr fn;
                ValuePtr thisArg;
                
                // Method call: obj.method()
                if (node->callee->type == NodeType::MemberExpr) {
                    thisArg = evaluate(node->callee->object, env);
                    std::string methodName = node->callee->property->strValue;
                    
                    // Auto-wrap primitives
                    if (thisArg->type == ValueType::String) {
                        fn = thisArg->getProperty(methodName);
                        if (!fn || fn->type == ValueType::Undefined)
                            fn = stringProto->getProperty(methodName);
                    } else if (thisArg->type == ValueType::Number) {
                        fn = thisArg->getProperty(methodName);
                        if (!fn || fn->type == ValueType::Undefined)
                            fn = numberProto->getProperty(methodName);
                    } else if (thisArg->type == ValueType::Array) {
                        fn = thisArg->getProperty(methodName);
                        if (!fn || fn->type == ValueType::Undefined)
                            fn = arrayProto->getProperty(methodName);
                    } else if (thisArg->type == ValueType::Promise) {
                        fn = thisArg->getProperty(methodName);
                        if (!fn || fn->type == ValueType::Undefined)
                            fn = promiseProto->getProperty(methodName);
                    } else {
                        fn = thisArg->getProperty(methodName);
                    }
                } else if (node->callee->type == NodeType::ComputedMemberExpr) {
                    thisArg = evaluate(node->callee->object, env);
                    auto key = evaluate(node->callee->property, env);
                    fn = thisArg->getProperty(key->toString());
                } else {
                    fn = evaluate(node->callee, env);
                }

                // Evaluate arguments
                std::vector<ValuePtr> args;
                for (auto& arg : node->children) {
                    if (arg->type == NodeType::SpreadExpr) {
                        auto spread = evaluate(arg->left, env);
                        if (spread->type == ValueType::Array) {
                            for (auto& e : spread->elements) args.push_back(e);
                        }
                    } else {
                        args.push_back(evaluate(arg, env));
                    }
                }

                if (!fn || (fn->type != ValueType::Function && fn->type != ValueType::NativeFunction)) {
                    // Check if it's a callable stored somewhere unusual
                    return Value::Undefined();
                }

                return callFunction(fn, args, thisArg);
            }

            case NodeType::NewExpr: {
                auto constructor = evaluate(node->callee, env);
                std::vector<ValuePtr> args;
                for (auto& arg : node->children) args.push_back(evaluate(arg, env));

                auto instance = Value::Object();
                instance->prototype = objectProto;

                // Check for "Promise" constructor specifically
                if (constructor && (constructor->type == ValueType::Function || constructor->type == ValueType::NativeFunction)) {
                    auto result = callFunction(constructor, args, instance);
                    if (result && result->type == ValueType::Promise) return result;
                    return instance;
                }
                return instance;
            }

            // ---- Function declarations and expressions ----
            case NodeType::FuncDecl: {
                auto fn = Value::Func(node->strValue, node->params, node->body, env, false);
                fn->isAsync = node->isAsync;
                fn->prototype = objectProto;
                env->define(node->strValue, fn);
                return fn;
            }
            case NodeType::FuncExpr: {
                auto fn = Value::Func(node->strValue, node->params, node->body, env, false);
                fn->prototype = objectProto;
                return fn;
            }
            case NodeType::ArrowFunc: {
                auto fn = Value::Func("", node->params, node->body, env, true);
                return fn;
            }

            // ---- Control flow ----
            case NodeType::IfStmt: {
                auto test = evaluate(node->test, env);
                if (test->isTruthy()) return evaluate(node->body, env);
                if (node->alt) return evaluate(node->alt, env);
                return Value::Undefined();
            }

            case NodeType::WhileStmt: {
                while (evaluate(node->test, env)->isTruthy()) {
                    auto res = evaluate(node->body, env);
                    if (res && res->type == ValueType::ControlSignal) {
                        if (res->controlType == ControlType::Break) break;
                        if (res->controlType == ControlType::Continue) continue;
                        return res;
                    }
                }
                return Value::Undefined();
            }

            case NodeType::DoWhileStmt: {
                do {
                    auto res = evaluate(node->body, env);
                    if (res && res->type == ValueType::ControlSignal) {
                        if (res->controlType == ControlType::Break) break;
                        if (res->controlType == ControlType::Continue) continue;
                        return res;
                    }
                } while (evaluate(node->test, env)->isTruthy());
                return Value::Undefined();
            }

            case NodeType::ForStmt: {
                auto forEnv = std::make_shared<Environment>(env);
                if (node->init) evaluate(node->init, forEnv);
                while (!node->test || evaluate(node->test, forEnv)->isTruthy()) {
                    auto res = evaluate(node->body, forEnv);
                    if (res && res->type == ValueType::ControlSignal) {
                        if (res->controlType == ControlType::Break) break;
                        if (res->controlType == ControlType::Continue) {
                            if (node->update) evaluate(node->update, forEnv);
                            continue;
                        }
                        return res;
                    }
                    if (node->update) evaluate(node->update, forEnv);
                }
                return Value::Undefined();
            }

            case NodeType::SwitchStmt: {
                auto discriminant = evaluate(node->test, env);
                bool matched = false;
                for (auto& caseNode : node->children) {
                    if (!matched && caseNode->test) {
                        auto caseVal = evaluate(caseNode->test, env);
                        if (!discriminant->strictEquals(caseVal)) continue;
                    }
                    if (caseNode->test || matched || !caseNode->test) {
                        matched = true;
                        auto res = Value::Undefined();
                        for (auto& stmt : caseNode->children) {
                            res = evaluate(stmt, env);
                            if (res && res->type == ValueType::ControlSignal) break;
                        }
                        if (res && res->type == ValueType::ControlSignal) {
                            if (res->controlType == ControlType::Break) return Value::Undefined();
                            return res;
                        }
                    }
                }
                return Value::Undefined();
            }

            case NodeType::ReturnStmt: {
                return Value::Signal(ControlType::Return, node->left ? evaluate(node->left, env) : nullptr);
            }

            case NodeType::BreakStmt: return Value::Signal(ControlType::Break);
            case NodeType::ContinueStmt: return Value::Signal(ControlType::Continue);

            case NodeType::ThrowStmt: {
                return Value::Signal(ControlType::Throw, evaluate(node->left, env));
            }

            case NodeType::TryCatch: {
                auto res = evaluate(node->body, env);
                if (res && res->type == ValueType::ControlSignal && res->controlType == ControlType::Throw) {
                    if (node->alt) {
                        auto catchEnv = std::make_shared<Environment>(env);
                        if (!node->strValue.empty()) {
                            catchEnv->define(node->strValue, res->signalValue);
                        }
                        res = evaluate(node->alt, catchEnv);
                    } else {
                        res = Value::Undefined();
                    }
                }
                if (node->update) {
                    auto finRes = evaluate(node->update, env); // finally
                    if (finRes && finRes->type == ValueType::ControlSignal) return finRes;
                }
                return res;
            }

            case NodeType::DeleteExpr: {
                if (node->left->type == NodeType::MemberExpr) {
                    auto obj = evaluate(node->left->object, env);
                    std::string key = node->left->property->strValue;
                    for (size_t i = 0; i < obj->properties.size(); i++) {
                        if (obj->properties[i].first == key) {
                            obj->properties.erase(obj->properties.begin() + i); break;
                        }
                    }
                    return Value::Bool(true);
                }
                return Value::Bool(false);
            }

            case NodeType::InstanceofExpr: {
                // Simplified - just check prototype chain
                return Value::Bool(false);
            }

            case NodeType::InExpr: {
                auto key = evaluate(node->left, env);
                auto obj = evaluate(node->right, env);
                return Value::Bool(obj && obj->hasProperty(key->toString()));
            }

            case NodeType::SequenceExpr: {
                ValuePtr last = Value::Undefined();
                for (auto& child : node->children) last = evaluate(child, env);
                return last;
            }

            case NodeType::AwaitExpr: {
                // Simplified: evaluate the promise synchronously
                auto val = evaluate(node->left, env);
                if (val && val->type == ValueType::Promise) {
                    if (val->promiseState == Value::PromiseState::Fulfilled) return val->promiseResult;
                    if (val->promiseState == Value::PromiseState::Rejected) {
                        return Value::Signal(ControlType::Throw, val->promiseResult);
                    }
                }
                return val;
            }

            default:
                return Value::Undefined();
        }
    }

    ValuePtr compoundAssign(const std::string& op, ValuePtr cur, ValuePtr val) {
        if (op == "+=") {
            if (cur->type == ValueType::String || val->type == ValueType::String)
                return Value::Str(cur->toString() + val->toString());
            return Value::Num(cur->toNumber() + val->toNumber());
        }
        if (op == "-=") return Value::Num(cur->toNumber() - val->toNumber());
        if (op == "*=") return Value::Num(cur->toNumber() * val->toNumber());
        if (op == "/=") return Value::Num(cur->toNumber() / val->toNumber());
        if (op == "%=") return Value::Num(std::fmod(cur->toNumber(), val->toNumber()));
        return val;
    }
};
