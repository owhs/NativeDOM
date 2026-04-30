#pragma once
#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <cmath>
#include <algorithm>
#include <cstdio>

// Forward declarations
class Environment;
struct ASTNode;

enum class ValueType {
    Undefined,
    Null,
    Boolean,
    Number,
    String,
    Object,
    Array,
    Function,
    NativeFunction,
    Promise,
    ControlSignal
};

enum class ControlType {
    Return,
    Break,
    Continue,
    Throw
};

// Forward declare
class Value;
using ValuePtr = std::shared_ptr<Value>;
using NativeFn = std::function<ValuePtr(std::vector<ValuePtr>, ValuePtr /*thisArg*/)>;

class Value : public std::enable_shared_from_this<Value> {
public:
    ValueType type;

    // Primitives
    double number = 0;
    std::string str;
    bool boolean = false;

    // Object / Array
    std::vector<std::pair<std::string, ValuePtr>> properties;
    std::vector<ValuePtr> elements; // Array elements

    // Function
    std::string funcName;
    std::vector<std::string> params;
    std::shared_ptr<ASTNode> body;
    std::shared_ptr<Environment> closure;
    bool isArrow = false;
    bool isAsync = false;

    // NativeFunction
    NativeFn nativeFn;
    int expectedArgs = -1;

    // Prototype chain
    ValuePtr prototype;

    // Promise
    enum class PromiseState { Pending, Fulfilled, Rejected };
    PromiseState promiseState = PromiseState::Pending;
    ValuePtr promiseResult;
    std::vector<std::pair<ValuePtr, ValuePtr>> thenCallbacks; // (onFulfilled, onRejected)
    std::vector<ValuePtr> catchCallbacks;
    std::vector<ValuePtr> finallyCallbacks;

    // Native pointer wrapper
    std::weak_ptr<void> weakNative;

    // Control Signal
    ControlType controlType;
    ValuePtr signalValue;

    // Constructor helpers
    static ValuePtr Undefined() {
        auto v = std::make_shared<Value>();
        v->type = ValueType::Undefined;
        return v;
    }
    static ValuePtr Null() {
        auto v = std::make_shared<Value>();
        v->type = ValueType::Null;
        return v;
    }
    static ValuePtr Bool(bool b) {
        auto v = std::make_shared<Value>();
        v->type = ValueType::Boolean;
        v->boolean = b;
        return v;
    }
    static ValuePtr Num(double n) {
        auto v = std::make_shared<Value>();
        v->type = ValueType::Number;
        v->number = n;
        return v;
    }
    static ValuePtr Str(const std::string& s) {
        auto v = std::make_shared<Value>();
        v->type = ValueType::String;
        v->str = s;
        return v;
    }
    static ValuePtr Object() {
        auto v = std::make_shared<Value>();
        v->type = ValueType::Object;
        return v;
    }
    static ValuePtr Array() {
        auto v = std::make_shared<Value>();
        v->type = ValueType::Array;
        return v;
    }
    static ValuePtr Func(const std::string& name, std::vector<std::string> params,
                         std::shared_ptr<ASTNode> body, std::shared_ptr<Environment> closure, bool arrow = false) {
        auto v = std::make_shared<Value>();
        v->type = ValueType::Function;
        v->funcName = name;
        v->params = std::move(params);
        v->body = body;
        v->closure = closure;
        v->isArrow = arrow;
        return v;
    }
    static ValuePtr Native(NativeFn fn, int args = -1) {
        auto v = std::make_shared<Value>();
        v->type = ValueType::NativeFunction;
        v->nativeFn = std::move(fn);
        v->expectedArgs = args;
        return v;
    }
    static ValuePtr MakePromise() {
        auto v = std::make_shared<Value>();
        v->type = ValueType::Promise;
        v->promiseState = PromiseState::Pending;
        return v;
    }
    static ValuePtr Signal(ControlType ct, ValuePtr val = nullptr) {
        auto v = std::make_shared<Value>();
        v->type = ValueType::ControlSignal;
        v->controlType = ct;
        v->signalValue = val;
        return v;
    }

    // Type coercion
    bool isTruthy() const {
        switch (type) {
            case ValueType::Undefined:
            case ValueType::Null: return false;
            case ValueType::Boolean: return boolean;
            case ValueType::Number: return number != 0 && !std::isnan(number);
            case ValueType::String: return !str.empty();
            default: return true;
        }
    }

    double toNumber() const {
        switch (type) {
            case ValueType::Undefined: return std::nan("");
            case ValueType::Null: return 0;
            case ValueType::Boolean: return boolean ? 1.0 : 0.0;
            case ValueType::Number: return number;
            case ValueType::String: {
                if (str.empty()) return 0;
                char* end;
                double val = strtod(str.c_str(), &end);
                if (end == str.c_str()) return std::nan("");
                return val;
            }
            default: return std::nan("");
        }
    }

    std::string toString() const {
        switch (type) {
            case ValueType::Undefined: return "undefined";
            case ValueType::Null: return "null";
            case ValueType::Boolean: return boolean ? "true" : "false";
            case ValueType::Number: {
                if (std::isnan(number)) return "NaN";
                if (std::isinf(number)) return number > 0 ? "Infinity" : "-Infinity";
                // Clean trailing zeros
                char buf[64];
                snprintf(buf, sizeof(buf), "%g", number);
                return std::string(buf);
            }
            case ValueType::String: return str;
            case ValueType::Object: return "[object Object]";
            case ValueType::Array: {
                std::string s;
                for (size_t i = 0; i < elements.size(); i++) {
                    if (i > 0) s += ",";
                    if (elements[i]) s += elements[i]->toString();
                }
                return s;
            }
            case ValueType::Function:
            case ValueType::NativeFunction:
                return "[function " + (funcName.empty() ? "anonymous" : funcName) + "]";
            case ValueType::Promise:
                return "[object Promise]";
        }
        return "";
    }

    std::string typeStr() const {
        switch (type) {
            case ValueType::Undefined: return "undefined";
            case ValueType::Null: return "object"; // JS compat
            case ValueType::Boolean: return "boolean";
            case ValueType::Number: return "number";
            case ValueType::String: return "string";
            case ValueType::Function:
            case ValueType::NativeFunction: return "function";
            case ValueType::Object: return "object";
            case ValueType::Array: return "object";
            case ValueType::Promise: return "object";
        }
        return "undefined";
    }

    // Property access with prototype chain
    ValuePtr getProperty(const std::string& key) {
        // Array length
        if (type == ValueType::Array && key == "length") {
            return Num((double)elements.size());
        }
        // String length
        if (type == ValueType::String && key == "length") {
            return Num((double)str.size());
        }
        // Array index access
        if (type == ValueType::Array) {
            char* end;
            size_t idx = std::strtoul(key.c_str(), &end, 10);
            if (end != key.c_str() && idx < elements.size()) return elements[idx];
        }
        // String index access
        if (type == ValueType::String) {
            char* end;
            size_t idx = std::strtoul(key.c_str(), &end, 10);
            if (end != key.c_str() && idx < str.size()) return Str(std::string(1, str[idx]));
        }
        // Own properties
        for (auto& kv : properties) {
            if (kv.first == key) return kv.second;
        }
        // Prototype chain
        if (prototype) return prototype->getProperty(key);
        return Undefined();
    }

    void setProperty(const std::string& key, ValuePtr val) {
        if (type == ValueType::Array) {
            char* end;
            size_t idx = std::strtoul(key.c_str(), &end, 10);
            if (end != key.c_str()) {
                if (idx >= elements.size()) elements.resize(idx + 1, Undefined());
                elements[idx] = val;
                return;
            }
        }
        for (auto& kv : properties) {
            if (kv.first == key) { kv.second = val; return; }
        }
        properties.push_back({key, val});
    }

    bool hasProperty(const std::string& key) {
        for (auto& kv : properties) if (kv.first == key) return true;
        if (type == ValueType::Array && key == "length") return true;
        if (type == ValueType::String && key == "length") return true;
        if (prototype) return prototype->hasProperty(key);
        return false;
    }

    // Strict equality
    bool strictEquals(ValuePtr other) {
        if (!other) return type == ValueType::Undefined;
        if (type != other->type) return false;
        switch (type) {
            case ValueType::Undefined:
            case ValueType::Null: return true;
            case ValueType::Boolean: return boolean == other->boolean;
            case ValueType::Number: return number == other->number;
            case ValueType::String: return str == other->str;
            default: return this == other.get(); // Reference equality for objects
        }
    }

    // Loose equality (simplified)
    bool looseEquals(ValuePtr other) {
        if (!other) return type == ValueType::Undefined || type == ValueType::Null;
        if (type == other->type) return strictEquals(other);
        if ((type == ValueType::Null && other->type == ValueType::Undefined) ||
            (type == ValueType::Undefined && other->type == ValueType::Null)) return true;
        // Number coercion
        return toNumber() == other->toNumber();
    }
};
