#pragma once
#include "Value.h"

class Environment : public std::enable_shared_from_this<Environment> {
    std::vector<std::pair<std::string, ValuePtr>> vars;
    std::shared_ptr<Environment> parent;
public:
    Environment() : parent(nullptr) {}
    Environment(std::shared_ptr<Environment> parent) : parent(parent) {}

    std::shared_ptr<Environment> getParent() { return parent; }

    void define(const std::string& name, ValuePtr val) {
        for (auto& kv : vars) {
            if (kv.first == name) { kv.second = val ? val : Value::Undefined(); return; }
        }
        vars.push_back({name, val ? val : Value::Undefined()});
    }

    ValuePtr get(const std::string& name) {
        for (auto& kv : vars) if (kv.first == name) return kv.second;
        if (parent) return parent->get(name);
        return Value::Undefined();
    }

    bool has(const std::string& name) {
        for (auto& kv : vars) if (kv.first == name) return true;
        if (parent) return parent->has(name);
        return false;
    }

    void set(const std::string& name, ValuePtr val) {
        for (auto& kv : vars) {
            if (kv.first == name) { kv.second = val; return; }
        }
        if (parent && parent->has(name)) {
            parent->set(name, val);
            return;
        }
        vars.push_back({name, val});
    }

    // Get all variable names (for debugging)
    std::vector<std::string> keys() const {
        std::vector<std::string> result;
        for (auto& kv : vars) result.push_back(kv.first);
        return result;
    }

    void clear() {
        vars.clear();
    }
};
