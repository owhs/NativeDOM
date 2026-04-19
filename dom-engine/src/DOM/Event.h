#pragma once
#include <string>
#include <vector>
#include <memory>

// Forward declaration
class Element;

class UIEvent {
public:
    std::string type;
    Element* target = nullptr;
    Element* currentTarget = nullptr;

    bool bubbles = true;
    bool cancelable = true;
    bool defaultPrevented = false;
    bool propagationStopped = false;
    bool immediatePropagationStopped = false;
    bool globalPropagationStopped = false;

    // Mouse
    int clientX = 0;
    int clientY = 0;
    int button = 0;    // 0=left, 1=middle, 2=right
    bool shiftKey = false;
    bool ctrlKey = false;
    bool altKey = false;

    // Keyboard
    int keyCode = 0;
    std::string key;

    // Custom data
    std::vector<std::pair<std::string, std::string>> detail;

    UIEvent() {}
    UIEvent(const std::string& t, bool bubble = true) : type(t), bubbles(bubble) {}

    void preventDefault() { defaultPrevented = true; }
    void stopPropagation() { propagationStopped = true; }
    void stopImmediatePropagation() { immediatePropagationStopped = true; propagationStopped = true; }
    void preventGlobalPropagation() { globalPropagationStopped = true; propagationStopped = true; }
};
