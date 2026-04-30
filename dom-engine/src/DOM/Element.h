#pragma once
#include <windows.h>
#include <string>
#include <vector>
#include <set>
#include <memory>
#include <algorithm>
#include <unordered_map>
#include "Event.h"
#include "../Rendering/nanovg.h"

static int LoadFontToVG(NVGcontext* vg, const std::string& family) {
    int font = nvgFindFont(vg, family.c_str());
    if (font != -1) return font; // Already loaded

    HKEY hKey;
    std::string file = "";
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion\\Fonts", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        char val[MAX_PATH] = {0};
        DWORD valSize = sizeof(val);
        DWORD type = REG_SZ;
        if (RegQueryValueExA(hKey, (family + " (TrueType)").c_str(), 0, &type, (LPBYTE)val, &valSize) == ERROR_SUCCESS) {
            file = val;
        } else {
            valSize = sizeof(val);
            if (RegQueryValueExA(hKey, family.c_str(), 0, &type, (LPBYTE)val, &valSize) == ERROR_SUCCESS) {
                file = val;
            }
        }
        RegCloseKey(hKey);
    }
    
    char winDir[MAX_PATH];
    GetWindowsDirectoryA(winDir, MAX_PATH);
    std::string path = file;
    if (path.find('\\') == std::string::npos && !path.empty()) {
        path = std::string(winDir) + "\\Fonts\\" + path;
    }
    if (!path.empty()) {
        return nvgCreateFont(vg, family.c_str(), path.c_str());
    }
    return -1;
}

struct EventListener {
    std::string type;
    int callbackId = -1;          // Script callback reference
    bool useCapture = false;
};

class Element : public std::enable_shared_from_this<Element> {
public:
    std::string tag, id;
    std::set<std::string> classes;
    std::vector<std::pair<std::string, std::string>> props;
    std::vector<std::pair<std::string, int>> propScores;

    struct PseudoClass {
        std::string name;
        std::vector<std::pair<std::string, std::string>> props;
        std::vector<std::pair<std::string, int>> propScores;
    };
    std::vector<PseudoClass> pseudoProps;

    std::string innerText;

    // Tree structure
    std::vector<std::shared_ptr<Element>> children;       // Light DOM
    std::vector<std::shared_ptr<Element>> shadowChildren;  // Shadow DOM (component internals)
    std::weak_ptr<Element> parent;
    std::weak_ptr<Element> shadowHost;

    // Layout
    int x = 0, y = 0, w = 100, h = 30;
    bool visible = true;

    // Scroll state (native overflow scrolling)
    float scrollX = 0, scrollY = 0;
    float contentW = 0, contentH = 0; // calculated content bounds
    float measuredContentH = 0; // actual rendered text height (updated each Draw)
    bool isScrollbarHovered = false; // true when mouse is over the scrollbar track

    // Interactive states
    bool isHovered = false;
    bool isFocused = false;
    bool isActive = false;

    // Event listeners (stored callback IDs for the script engine to resolve)
    std::vector<EventListener> listeners;

    // For tabbing
    int tabIndex = -1;

    virtual ~Element() {}

    // ---- CSS Variable Resolution ----
    std::string ApplyVars(const std::string& val) {
        std::string result = val;
        // Resolve var() references
        size_t p = 0;
        int loopGuard = 0;
        while ((p = result.find("var(", p)) != std::string::npos && ++loopGuard < 20) {
            size_t endP = result.find(')', p);
            if (endP == std::string::npos) break;
            std::string inner = result.substr(p + 4, endP - p - 4);
            size_t comma = inner.find(',');
            std::string varName, defaultVal;
            if (comma != std::string::npos) {
                varName = inner.substr(0, comma);
                defaultVal = inner.substr(comma + 1);
                while(!defaultVal.empty() && (defaultVal.front() == ' ' || defaultVal.front() == '"')) defaultVal.erase(defaultVal.begin());
                while(!defaultVal.empty() && (defaultVal.back() == ' ' || defaultVal.back() == '"')) defaultVal.pop_back();
            } else {
                varName = inner;
            }
            std::string resolved;
            std::shared_ptr<Element> curr = shared_from_this();
            while (curr) {
                bool found = false;
                
                auto checkState = [&](const std::string& state) {
                    for (auto& pc : curr->pseudoProps) {
                        if (pc.name == state) {
                            for (auto& kv : pc.props) {
                                if (kv.first == varName && kv.second != "var(" + inner + ")") { resolved = kv.second; found = true; return; }
                            }
                        }
                    }
                };

                if (curr->isHovered) checkState("hover");
                if (found) break;
                if (curr->isFocused) checkState("focused");
                if (found) break;
                if (curr->isActive) checkState("active");
                if (found) break;

                for (auto& kv : curr->props) {
                    if (kv.first == varName && kv.second != "var(" + inner + ")") { resolved = kv.second; found = true; break; }
                }
                if (found) break;
                auto p = curr->parent.lock();
                auto sh = curr->shadowHost.lock();
                curr = p ? p : sh;
            }
            if (resolved.empty()) resolved = defaultVal;
            if (resolved.find("var(") == std::string::npos) {
                result.replace(p, endP - p + 1, resolved);
                p += resolved.size();
            } else {
                result.replace(p, endP - p + 1, resolved);
            }
        }
        
        // Resolve attr() references
        p = 0;
        loopGuard = 0;
        while ((p = result.find("attr(", p)) != std::string::npos && ++loopGuard < 20) {
            size_t endP = result.find(')', p);
            if (endP == std::string::npos) break;
            std::string attrName = result.substr(p + 5, endP - p - 5);
            std::string defaultVal = "";
            size_t comma = attrName.find(',');
            if (comma != std::string::npos) {
                defaultVal = attrName.substr(comma + 1);
                attrName = attrName.substr(0, comma);
                while(!defaultVal.empty() && (defaultVal.front() == ' ' || defaultVal.front() == '"' || defaultVal.front() == '\'')) defaultVal.erase(defaultVal.begin());
                while(!defaultVal.empty() && (defaultVal.back() == ' ' || defaultVal.back() == '"' || defaultVal.back() == '\'')) defaultVal.pop_back();
            }
            while(!attrName.empty() && isspace((unsigned char)attrName.back())) attrName.pop_back();
            while(!attrName.empty() && isspace((unsigned char)attrName.front())) attrName.erase(attrName.begin());

            std::string resolved = defaultVal;
            for (auto& kv : this->props) {
                if (kv.first == attrName) { resolved = kv.second; break; }
            }
            
            result.replace(p, endP - p + 1, resolved);
            // Don't advance p if we just injected a substitution that might need further evaluating
        }
        
        // Resolve calc() expressions
        p = 0;
        loopGuard = 0;
        while ((p = result.find("calc(", p)) != std::string::npos && ++loopGuard < 20) {
            size_t endP = result.find(')', p);
            if (endP == std::string::npos) break;
            std::string expr = result.substr(p + 5, endP - p - 5);
            
            int calcGuard = 0;
            while (++calcGuard < 20) {
                size_t opPos = std::string::npos;
                for(size_t i = 1; i < expr.size(); i++) {
                   if (expr[i] == '+' || expr[i] == '-' || expr[i] == '*' || expr[i] == '/') {
                       if (expr[i-1] != 'e' && expr[i-1] != 'E') { opPos = i; break; }
                   }
                }
                if (opPos == std::string::npos) break;

                int lStart = (int)opPos - 1;
                while (lStart >= 0 && (isdigit((unsigned char)expr[lStart]) || expr[lStart] == '.' || expr[lStart] == ' ' || 
                       (expr[lStart] == '-' && (lStart == 0 || expr[lStart-1] == ' ')))) lStart--;
                lStart++;
                char* end;
                double a = strtod(expr.substr(lStart, opPos - lStart).c_str(), &end);

                int rEnd = (int)opPos + 1;
                while (rEnd < (int)expr.size() && (expr[rEnd] == ' ' || expr[rEnd] == '-' || expr[rEnd] == '.' || isdigit((unsigned char)expr[rEnd]))) rEnd++;
                double b = strtod(expr.substr(opPos + 1, rEnd - opPos - 1).c_str(), &end);

                char op = expr[opPos];
                double n = op == '+' ? a + b : op == '-' ? a - b : op == '*' ? a * b : op == '/' ? a / b : 0;
                
                char res[32];
                snprintf(res, sizeof(res), "%g", n);
                std::string resStr(res);
                resStr.erase(resStr.find_last_not_of('0') + 1);
                if (!resStr.empty() && resStr.back() == '.') resStr.pop_back();
                expr.replace(lStart, rEnd - lStart, resStr);
            }
            result.replace(p, endP - p + 1, expr);
            p += expr.size();
        }
        return result;
    }

    // ---- Property Access (with state cascade) ----
    std::string GetRaw(const std::string& key, const std::string& def = "") {
        std::string res = def;
        int maxScore = -1;

        std::shared_ptr<Element> curr = shared_from_this();
        while (curr) {
            int score = -1;
            std::string localRes = "";

            for (auto& kv : curr->props) {
                if (kv.first == key) {
                    localRes = kv.second;
                    for (auto& skv : curr->propScores) { if (skv.first == key) { score = skv.second; break; } }
                    break;
                }
            }

            for (auto& pc : curr->pseudoProps) {
                bool active = false;
                if (pc.name == "hover") active = curr->isHovered;
                else if (pc.name == "focused") active = curr->isFocused;
                else if (pc.name == "active") active = curr->isActive;
                else if (!pc.name.empty() && pc.name.front() == '[' && pc.name.back() == ']') {
                    size_t eqLoc = pc.name.find('=');
                    if (eqLoc != std::string::npos) {
                        std::string k = pc.name.substr(1, eqLoc - 1);
                        std::string v = pc.name.substr(eqLoc + 1, pc.name.size() - eqLoc - 2);
                        if (!v.empty() && ((v.front() == '\'' && v.back() == '\'') || (v.front() == '"' && v.back() == '"'))) {
                            v = v.substr(1, v.size() - 2);
                        }
                        std::shared_ptr<Element> host = curr;
                        while (host) {
                            std::string hVal = "";
                            bool found = false;
                            for (auto& pr : host->props) {
                                if (pr.first == k) { hVal = pr.second; found = true; break; }
                            }
                            if (found) {
                                if (hVal == v) active = true;
                                break;
                            }
                            auto hp = host->parent.lock();
                            auto hsh = host->shadowHost.lock();
                            host = hp ? hp : hsh;
                        }
                    }
                }

                if (active) {
                    for (auto& kv : pc.props) {
                        if (kv.first == key) {
                            int pScore = 0;
                            for (auto& skv : pc.propScores) { if (skv.first == key) { pScore = skv.second; break; } }
                            if (pScore > score) {
                                score = pScore;
                                localRes = kv.second;
                            }
                            break;
                        }
                    }
                }
            }

            if (score != -1) {
                if (curr.get() == this) {
                    maxScore = score;
                    res = localRes;
                } else {
                    bool isInheritable = (key == "color" || key == "font-family" || key == "font-size" || 
                                          key == "font-weight" || key == "text-align" || key == "cursor" ||
                                          key == "events" || key == "pointer-events" || key == "opacity");
                    if (score >= 1000000) {
                        if (score > maxScore) {
                            maxScore = score;
                            res = localRes;
                        }
                    } else if (isInheritable && maxScore == -1) {
                        maxScore = score;
                        res = localRes;
                    }
                }
            }

            auto p = curr->parent.lock();
            auto sh = curr->shadowHost.lock();
            curr = p ? p : sh;
        }

        return ApplyVars(res);
    }
    
    struct TransitionState {
        std::string startVal;
        std::string targetVal;
        std::string currentVal;
        ULONGLONG startTick = 0;
        ULONGLONG durationMs = 0;
        std::string easing;
    };
    std::unordered_map<std::string, TransitionState> transitions;

    std::string InterpolateStr(const std::string& start, const std::string& target, float p) {
        if (start == target) return target;
        
        bool isColor = (!target.empty() && target[0] == '#') || target.find("color-mix") == 0 || target == "transparent" ||
                       (!start.empty() && start[0] == '#') || start.find("color-mix") == 0 || start == "transparent" || target.empty() || start.empty();
        
        // Some properties like 'width' may have empty start, we handle non-colors here
        if (!isColor && (start.empty() || target.empty())) return target;

        if (isColor) {
            NVGcolor a = ParseColor(!start.empty() && start != "transparent" ? start : "#00000000");
            NVGcolor b = ParseColor(!target.empty() && target != "transparent" ? target : "#00000000");
            
            // If one is transparent, borrow the RGB from the other to prevent muddy black transitions
            if (start.empty() || start == "transparent") a = nvgRGBAf(b.r, b.g, b.b, 0.0f);
            if (target.empty() || target == "transparent") b = nvgRGBAf(a.r, a.g, a.b, 0.0f);

            if (p <= 0.0f) {
                char buf[32];
                sprintf(buf, "#%02X%02X%02X%02X", (int)(a.r*255.0f), (int)(a.g*255.0f), (int)(a.b*255.0f), (int)(a.a*255.0f)); 
                return buf;
            }
            if (p >= 1.0f) {
                char buf[32];
                sprintf(buf, "#%02X%02X%02X%02X", (int)(b.r*255.0f), (int)(b.g*255.0f), (int)(b.b*255.0f), (int)(b.a*255.0f)); 
                return buf;
            }

            int r = (int)((a.r + (b.r - a.r) * p) * 255.0f);
            int g = (int)((a.g + (b.g - a.g) * p) * 255.0f);
            int blue = (int)((a.b + (b.b - a.b) * p) * 255.0f);
            int alpha = (int)((a.a + (b.a - a.a) * p) * 255.0f);
            char buf[32];
            sprintf(buf, "#%02X%02X%02X%02X", r, g, blue, alpha); 
            return buf;
        } else {
            if (p <= 0.0f) return start;
            if (p >= 1.0f) return target;

            char* endA; char* endB;
            double a = strtod(start.c_str(), &endA);
            double b = strtod(target.c_str(), &endB);
            if (endA == start.c_str() && endB == target.c_str()) return target; 
            double result = a + (b - a) * p;
            
            std::string unit = target.substr(endB - target.c_str());
            char buf[32];
            sprintf(buf, "%g", result);
            return std::string(buf) + unit;
        }
    }

    std::string Get(const std::string& key, const std::string& def = "") {
        std::string tgt = GetRaw(key, def);
        
        std::string transCfg = GetRaw("transition", "");
        if (transCfg.empty() || transCfg == "none") return tgt;
        
        std::string myEasing = "";
        float myDurMs = 0;
        bool found = false;
        
        size_t p = 0;
        while (p < transCfg.size()) {
            size_t comma = transCfg.find(',', p);
            if (comma == std::string::npos) comma = transCfg.size();
            std::string chunk = transCfg.substr(p, comma - p);
            
            while(!chunk.empty() && isspace((unsigned char)chunk.back())) chunk.pop_back();
            while(!chunk.empty() && isspace((unsigned char)chunk.front())) chunk.erase(chunk.begin());

            if (chunk.find(key + " ") == 0) {
                found = true;
                size_t dStart = key.size();
                while(dStart < chunk.size() && isspace((unsigned char)chunk[dStart])) dStart++;
                size_t dEnd = dStart;
                while(dEnd < chunk.size() && !isspace((unsigned char)chunk[dEnd])) dEnd++;
                std::string dStr = chunk.substr(dStart, dEnd - dStart);
                float dur = 0;
                char* dEndPtr;
                float parsedF = (float)strtod(dStr.c_str(), &dEndPtr);
                if (dStr.find("ms") != std::string::npos) dur = parsedF;
                else if (dStr.find("s") != std::string::npos) dur = parsedF * 1000.0f;
                else dur = parsedF;
                
                myDurMs = dur;
                size_t eStart = dEnd;
                while(eStart < chunk.size() && isspace((unsigned char)chunk[eStart])) eStart++;
                if (eStart < chunk.size()) myEasing = chunk.substr(eStart);
                break;
            }
            if (comma == transCfg.size()) break;
            p = comma + 1;
        }
        
        if (!found) return tgt;
        
        if (transitions.count(key) == 0) {
            transitions[key] = { tgt, tgt, tgt, GetTickCount64(), 0, "" };
        }
        
        auto& t = transitions[key];
        if (t.targetVal != tgt) {
            t.startVal = t.currentVal;
            t.targetVal = tgt;
            t.startTick = GetTickCount64();
            t.durationMs = (ULONGLONG)myDurMs;
            t.easing = myEasing.empty() ? "linear" : myEasing;
        }
        
        if (t.durationMs <= 0) {
            t.currentVal = tgt;
            return tgt;
        }
        
        ULONGLONG now = GetTickCount64();
        if (now >= t.startTick + t.durationMs) {
            t.currentVal = tgt;
            return tgt;
        }
        
        float progress = (float)(now - t.startTick) / (float)t.durationMs;
        if (t.easing == "ease") progress = progress * progress * (3.0f - 2.0f * progress); // smoothstep
        
        t.currentVal = InterpolateStr(t.startVal, t.targetVal, progress);
        return t.currentVal;
    }

    bool HasActiveTransitions() {
        ULONGLONG now = GetTickCount64();
        for (auto& kv : transitions) {
            auto& t = kv.second;
            if (t.durationMs > 0 && now < t.startTick + t.durationMs) return true;
        }
        for (auto& c : shadowChildren) {
            if (c->HasActiveTransitions()) return true;
        }
        for (auto& c : children) {
            if (c->HasActiveTransitions()) return true;
        }
        return false;
    }
    
    int& getPropScore(std::vector<std::pair<std::string, int>>& scores, const std::string& key) {
        for (auto& kv : scores) if (kv.first == key) return kv.second;
        scores.push_back({key, 0});
        return scores.back().second;
    }

    void SetProp(const std::string& key, const std::string& val, int score = 0) {
        std::string finalVal = val;
        int finalScore = score;
        size_t impPos = val.find("!important");
        if (impPos != std::string::npos) {
            finalScore += 1000000;
            size_t numStart = impPos + 10;
            if (numStart < val.size() && isdigit((unsigned char)val[numStart])) {
                finalScore += (int)strtol(val.substr(numStart).c_str(), nullptr, 10);
            }
            finalVal = val.substr(0, impPos);
            while (!finalVal.empty() && isspace((unsigned char)finalVal.back())) finalVal.pop_back();
        }

        int& currScore = getPropScore(propScores, key);
        if (finalScore >= currScore) {
            currScore = finalScore;
            for (auto& kv : props) { if (kv.first == key) { kv.second = finalVal; return; } }
            props.push_back({key, finalVal});
        }
    }

    void RemoveProp(const std::string& key) {
        for (auto it = props.begin(); it != props.end(); ++it) {
            if (it->first == key) {
                props.erase(it);
                break;
            }
        }
        for (auto it = propScores.begin(); it != propScores.end(); ++it) {
            if (it->first == key) {
                propScores.erase(it);
                break;
            }
        }
    }

    void SetPseudoProp(const std::string& pseudo, const std::string& key, const std::string& val, int score = 0) {
        std::string finalVal = val;
        int finalScore = score;
        size_t impPos = val.find("!important");
        if (impPos != std::string::npos) {
            finalScore += 1000000;
            size_t numStart = impPos + 10;
            if (numStart < val.size() && isdigit((unsigned char)val[numStart])) {
                finalScore += (int)strtol(val.substr(numStart).c_str(), nullptr, 10);
            }
            finalVal = val.substr(0, impPos);
            while (!finalVal.empty() && isspace((unsigned char)finalVal.back())) finalVal.pop_back();
        }

        for (auto& pc : pseudoProps) {
            if (pc.name == pseudo) {
                int& currScore = getPropScore(pc.propScores, key);
                if (finalScore >= currScore) {
                    currScore = finalScore;
                    for (auto& kv : pc.props) { if (kv.first == key) { kv.second = finalVal; return; } }
                    pc.props.push_back({key, finalVal});
                }
                return;
            }
        }
        PseudoClass newPc; newPc.name = pseudo;
        newPc.propScores.push_back({key, finalScore});
        newPc.props.push_back({key, finalVal});
        pseudoProps.push_back(newPc);
    }

    int GetInt(const std::string& key, int def = 0) {
        char defBuf[32]; snprintf(defBuf, sizeof(defBuf), "%d", def);
        std::string v = Get(key, defBuf);
        char* end; double d = strtod(v.c_str(), &end);
        if (end == v.c_str()) return def; return (int)d;
    }
    float GetFloat(const std::string& key, float def = 0.0f) {
        char defBuf[32]; snprintf(defBuf, sizeof(defBuf), "%g", def);
        std::string v = Get(key, defBuf);
        char* end; double d = strtod(v.c_str(), &end);
        if (end == v.c_str()) return def; return (float)d;
    }

    // ---- Tree Manipulation ----
    void Adopt(std::shared_ptr<Element> child, bool shadow = false) {
        if (!child) return;
        if (shadow) {
            child->shadowHost = weak_from_this();
            child->parent.reset();
        } else {
            child->parent = weak_from_this();
            child->shadowHost.reset();
        }
        if (shadow) shadowChildren.push_back(child);
        else children.push_back(child);
    }

    void RemoveChild(std::shared_ptr<Element> child) {
        children.erase(std::remove(children.begin(), children.end(), child), children.end());
        shadowChildren.erase(std::remove(shadowChildren.begin(), shadowChildren.end(), child), shadowChildren.end());
    }

    std::shared_ptr<Element> getParentShared() {
        return parent.lock();
    }

    int getChildIndex() const {
        auto p = parent.lock();
        if (!p) return 0;
        for (size_t i = 0; i < p->children.size(); i++) {
            if (p->children[i].get() == this) return (int)i;
        }
        return 0;
    }

    int getChildCount() const {
        return (int)children.size();
    }

    // ---- Layout ----
    virtual void Layout(int px, int py) {
        if (GetRaw("display") == "none") { w = 0; h = 0; return; }

        auto p = parent.lock();
        auto sh = shadowHost.lock();
        int pPadL = sh ? sh->GetInt("padding-left", sh->GetInt("padding", 0)) : (p ? p->GetInt("padding-left", p->GetInt("padding", 0)) : 0);
        int pPadR = sh ? sh->GetInt("padding-right", sh->GetInt("padding", 0)) : (p ? p->GetInt("padding-right", p->GetInt("padding", 0)) : 0);
        int pPadT = sh ? sh->GetInt("padding-top", sh->GetInt("padding", 0)) : (p ? p->GetInt("padding-top", p->GetInt("padding", 0)) : 0);
        int pPadB = sh ? sh->GetInt("padding-bottom", sh->GetInt("padding", 0)) : (p ? p->GetInt("padding-bottom", p->GetInt("padding", 0)) : 0);

        std::string wStr = Get("width");
        if (!wStr.empty() && wStr.back() == '%') {
            float pct = strtof(wStr.substr(0, wStr.size() - 1).c_str(), nullptr) / 100.0f;
            float pWidth = sh ? sh->w : (p ? p->w : w);
            w = (int)((pWidth - pPadL - pPadR) * pct);
        } else w = GetInt("width", w);

        std::string hStr = Get("height");
        if (!hStr.empty() && hStr.back() == '%') {
            float pct = strtof(hStr.substr(0, hStr.size() - 1).c_str(), nullptr) / 100.0f;
            float pHeight = sh ? sh->h : (p ? p->h : h);
            h = (int)((pHeight - pPadT - pPadB) * pct);
        } else h = GetInt("height", h);

        int minW = GetInt("min-width", -1);
        int maxW = GetInt("max-width", -1);
        int minH = GetInt("min-height", -1);
        int maxH = GetInt("max-height", -1);
        
        if (minW != -1 && w < minW) w = minW;
        if (maxW != -1 && w > maxW) w = maxW;
        if (minH != -1 && h < minH) h = minH;
        if (maxH != -1 && h > maxH) h = maxH;

        if (!GetRaw("right").empty()) {
            float pWidth = sh ? sh->w : (p ? p->w : 0);
            x = px + pWidth - pPadL - pPadR - w - GetInt("right");
        } else {
            x = px + GetInt("x");
        }

        if (!GetRaw("bottom").empty()) {
            float pHeight = sh ? sh->h : (p ? p->h : 0);
            y = py + pHeight - pPadT - pPadB - h - GetInt("bottom");
        } else {
            y = py + GetInt("y");
        }

        auto doLayoutList = [&](std::vector<std::shared_ptr<Element>>& list) {
            int myPadL = GetInt("padding-left", GetInt("padding", 0));
            int myPadT = GetInt("padding-top", GetInt("padding", 0));
            
            std::string layout = Get("layout", "absolute");
            if (layout == "absolute") {
                for (auto& c : list) c->Layout(x + myPadL, y + myPadT);
                return;
            }

            int gap = GetInt("gap", 0);
            int totalW = 0, totalH = 0;
            std::vector<std::shared_ptr<Element>> views;

            // Pre-calculate intrinsic sizes and collect visible items
            for (auto& c : list) {
                if (c->GetRaw("display") == "none") {
                    c->Layout(0, 0); // silently layout hidden
                    continue;
                }
                c->Layout(x + myPadL, y + myPadT); // baseline calc given parent's bounds
                views.push_back(c);
                if (layout == "row") totalW += c->w + gap;
                if (layout == "col") totalH += c->h + gap;
            }

            if (!views.empty()) {
                if (layout == "row") totalW -= gap;
                if (layout == "col") totalH -= gap;
            }

            int padR = GetInt("padding-right", GetInt("padding", 0));
            int padB = GetInt("padding-bottom", GetInt("padding", 0));
            float innerW = w - myPadL - padR;
            float innerH = h - myPadT - padB;

            // Calculate start positions
            std::string justify = Get("justify", "start");
            float cx = x + myPadL, cy = y + myPadT;
            float spacing = gap;

            if (layout == "row") {
                if (justify == "center") cx += (innerW - totalW) / 2.0f;
                else if (justify == "end" || justify == "right") cx += (innerW - totalW);
                else if (justify == "space-between" && views.size() > 1) {
                    spacing = (float)(innerW - (totalW - gap * (views.size() - 1))) / (views.size() - 1);
                }
            } else if (layout == "col") {
                if (justify == "center") cy += (innerH - totalH) / 2.0f;
                else if (justify == "end" || justify == "bottom") cy += (innerH - totalH);
                else if (justify == "space-between" && views.size() > 1) {
                    spacing = (float)(innerH - (totalH - gap * (views.size() - 1))) / (views.size() - 1);
                }
            }

            // Position elements
            for (auto& c : views) {
                // If the user manually provided an explicit x/y block inside the row/col element, 
                // we should still let it offset the auto-layout position! But we use cx/cy as the new baseline.
                // In DOM, c->Layout(px, py) will add px to its native Get("x"). 
                // So we can just call c->Layout again with the offset! 
                // Wait! c->Layout will *keep* adding its manual `x` from CSS. 
                // But in a row layout, `x` should be ignored.
                // Let's manually overwrite its final computed x/y coordinates.
                c->x = (int)cx;
                c->y = (int)cy;

                // Handle cross-axis alignment natively for easy vertically centered rows
                std::string align = Get("align", "start");
                if (layout == "row") {
                    if (align == "center") c->y += (innerH - c->h) / 2;
                    else if (align == "end" || align == "bottom") c->y += (innerH - c->h);
                    cx += c->w + spacing;
                } else if (layout == "col") {
                    if (align == "center") c->x += (innerW - c->w) / 2;
                    else if (align == "end" || align == "right") c->x += (innerW - c->w);
                    cy += c->h + spacing;
                }
                
                // Cascade childs
                c->Layout(c->x - c->GetInt("x"), c->y - c->GetInt("y")); // trigger inner layouts without double-offsetting
            }
        };

        doLayoutList(shadowChildren);
        doLayoutList(children);
    }

    // ---- Rendering ----
    virtual void Draw(NVGcontext* vg) {
        if (!visible || GetRaw("display") == "none") return;

        nvgSave(vg);
        
        int radius = GetInt("border-radius", 0);
        extern HWND g_hwnd;
        if (tag == "ui" && g_hwnd && IsZoomed(g_hwnd)) {
            radius = 0;
        }

        // Box Shadow and Glow (drawn before intersect scissor so they don't clip themselves)
        std::string bsStr = Get("box-shadow");
        if (bsStr.empty()) bsStr = Get("box-shadow-color");
        NVGcolor shadowCol = ParseColor(bsStr);
        if (shadowCol.a > 0.0f) {
            float sx = GetFloat("box-shadow-x", 0.0f);
            float sy = GetFloat("box-shadow-y", 0.0f);
            float blur = GetFloat("box-shadow-blur", 10.0f);
            float spread = GetFloat("box-shadow-spread", 0.0f);
            NVGpaint shadowPaint = nvgBoxGradient(vg, x + sx - spread, y + sy - spread, w + spread * 2, h + spread * 2, radius + spread, blur, shadowCol, nvgRGBA(0,0,0,0));
            nvgBeginPath(vg);
            nvgRect(vg, x + sx - spread - blur * 2, y + sy - spread - blur * 2, w + spread * 2 + blur * 4, h + spread * 2 + blur * 4);
            nvgFillPaint(vg, shadowPaint);
            nvgFill(vg);
        }
        
        NVGcolor glowCol = ParseColor(Get("glow"));
        if (glowCol.a > 0.0f) {
            float blur = GetFloat("glow-blur", 15.0f);
            float spread = GetFloat("glow-spread", 2.0f);
            NVGpaint glowPaint = nvgBoxGradient(vg, x - spread, y - spread, w + spread * 2, h + spread * 2, radius + spread, blur, glowCol, nvgRGBA(0,0,0,0));
            nvgBeginPath(vg);
            nvgRect(vg, x - spread - blur * 2, y - spread - blur * 2, w + spread * 2 + blur * 4, h + spread * 2 + blur * 4);
            nvgFillPaint(vg, glowPaint);
            nvgFill(vg);
        }

        std::string overflow = Get("overflow", "visible");
        if (overflow == "hidden") {
            nvgIntersectScissor(vg, x, y, w, h);
        }

        // Background
        NVGcolor bgCol = ParseColor(Get("bg"));
        if (bgCol.a > 0.0f) {
            nvgBeginPath(vg);
            if (radius > 0) nvgRoundedRect(vg, x, y, w, h, radius);
            else nvgRect(vg, x, y, w, h);
            nvgFillColor(vg, bgCol);
            nvgFill(vg);
        }

        // Image rendering
        if (tag == "Image") {
            std::string src = Get("src");
            if (!src.empty()) {
                int img = 0;
                if (src == "system:exe") {
                    static int exeImg = 0;
                    if (exeImg == 0) {
                        HICON hI = LoadIconA(NULL, IDI_APPLICATION);
                        if (hI) {
                            ICONINFO ii;
                            if (GetIconInfo(hI, &ii)) {
                                BITMAP bmp;
                                GetObject(ii.hbmColor, sizeof(bmp), &bmp);
                                int iw = bmp.bmWidth;
                                int ih = bmp.bmHeight;
                                std::vector<unsigned char> pixels(iw * ih * 4);
                                BITMAPINFO bmi = {};
                                bmi.bmiHeader.biSize = sizeof(bmi.bmiHeader);
                                bmi.bmiHeader.biWidth = iw;
                                bmi.bmiHeader.biHeight = -ih; 
                                bmi.bmiHeader.biPlanes = 1;
                                bmi.bmiHeader.biBitCount = 32;
                                bmi.bmiHeader.biCompression = BI_RGB;
                                HDC hdc = GetDC(NULL);
                                GetDIBits(hdc, ii.hbmColor, 0, ih, pixels.data(), &bmi, DIB_RGB_COLORS);
                                ReleaseDC(NULL, hdc);
                                // BGRA to RGBA
                                for(size_t i=0; i<pixels.size(); i+=4) {
                                    std::swap(pixels[i], pixels[i+2]);
                                }
                                exeImg = nvgCreateImageRGBA(vg, iw, ih, 0, pixels.data());
                                DeleteObject(ii.hbmColor);
                                DeleteObject(ii.hbmMask);
                            }
                        }
                    }
                    img = exeImg;
                } else {
                    img = nvgCreateImage(vg, src.c_str(), 0);
                }

                if (img != 0) {
                    NVGpaint imgPaint = nvgImagePattern(vg, x, y, w, h, 0, img, 1.0f);
                    nvgBeginPath(vg);
                    nvgRect(vg, x, y, w, h);
                    nvgFillPaint(vg, imgPaint);
                    nvgFill(vg);
                    if (src != "system:exe") nvgDeleteImage(vg, img);
                }
            }
        }

        // Text rendering
        std::string display = ApplyVars(innerText);
        if (!display.empty() || tag == "Text") {
            int fontSize = GetInt("font-size", 14);
            NVGcolor fg = ParseColor(Get("color", "#CDD6F4"));
            if (fg.a == 0.0f) fg = nvgRGBA(255, 0, 0, 255);
            
            nvgFontSize(vg, (float)fontSize);
            
            std::string fontNamesStr = Get("font-family", "Segoe UI");
            std::vector<std::string> fonts;
            size_t sIdx = 0;
            while(sIdx < fontNamesStr.size()) {
                size_t c = fontNamesStr.find(',', sIdx);
                if (c == std::string::npos) c = fontNamesStr.size();
                std::string fn = fontNamesStr.substr(sIdx, c - sIdx);
                while (!fn.empty() && isspace((unsigned char)fn.back())) fn.pop_back();
                while (!fn.empty() && isspace((unsigned char)fn.front())) fn.erase(fn.begin());
                if(fn.size() >= 2 && fn.front()=='"' && fn.back()=='"') fn = fn.substr(1, fn.size()-2);
                if (!fn.empty()) fonts.push_back(fn);
                sIdx = c + 1;
            }
            if (fonts.empty()) fonts.push_back("Segoe UI");

            std::string actualFont = "Segoe UI";
            for(auto& fn : fonts) {
                if (LoadFontToVG(vg, fn) != -1) {
                     actualFont = fn;
                     break;
                }
            }
            nvgFontFace(vg, actualFont.c_str());
            
            int align = 0;
            std::string textAlign = Get("text-align", "");
            if (textAlign == "center" || tag == "Button") align |= NVG_ALIGN_CENTER;
            else if (textAlign == "right") align |= NVG_ALIGN_RIGHT;
            else align |= NVG_ALIGN_LEFT;
            
            std::string vAlign = Get("vertical-align", "");
            if (vAlign == "center" || tag == "Button") align |= NVG_ALIGN_MIDDLE;
            else if (vAlign == "bottom") align |= NVG_ALIGN_BOTTOM;
            else align |= NVG_ALIGN_TOP;
            
            nvgTextAlign(vg, align);
            
            // (Caret is now handled by the TextInput component via separate View element)
            
            int padL = GetInt("padding-left", GetInt("padding", 0));
            int padR = GetInt("padding-right", GetInt("padding", 0));
            int padT = GetInt("padding-top", GetInt("padding", 0));
            int padB = GetInt("padding-bottom", GetInt("padding", 0));
            
            float tx = (float)(x + padL);
            float ty = (float)(y + padT);
            float mw = (float)(w - padL - padR);
            if (align & NVG_ALIGN_CENTER) {
                // For nvgTextBox, tx should be the left boundary, it centers WITHIN the wrap box
            } else if (align & NVG_ALIGN_RIGHT) {
                // nvgTextBox right align also occurs inside the box
            }
            
            // For nvgText, we need exact positions if no wrapping
            if (align & NVG_ALIGN_CENTER) tx = x + (float)w * 0.5f;
            else if (align & NVG_ALIGN_RIGHT) tx = x + w - padR;
            if (align & NVG_ALIGN_MIDDLE) ty = y + (float)h * 0.5f;
            else if (align & NVG_ALIGN_BOTTOM) ty = y + h - padB;
            
            nvgFillColor(vg, fg);
            
            std::string whiteSpace = Get("white-space", "normal");
            if (whiteSpace == "normal") {
                float boxX = (float)(x + padL);
                nvgTextBox(vg, boxX, ty, mw, display.c_str(), NULL);
                // Measure actual rendered height for scroll containers
                float bounds[4];
                nvgTextBoxBounds(vg, boxX, ty, mw, display.c_str(), NULL, bounds);
                measuredContentH = (bounds[3] - bounds[1]) + (float)padT + (float)padB;
            } else {
                nvgText(vg, tx, ty, display.c_str(), NULL);
                measuredContentH = (float)fontSize + (float)padT + (float)padB;
            }
        }

        auto sortChildren = [](const std::vector<std::shared_ptr<Element>>& list) {
            std::vector<std::shared_ptr<Element>> sorted = list;
            std::stable_sort(sorted.begin(), sorted.end(), [](const std::shared_ptr<Element>& a, const std::shared_ptr<Element>& b) {
                return a->GetInt("z-index") < b->GetInt("z-index");
            });
            return sorted;
        };

        // ---- Scroll-aware children rendering ----
        std::string curOverflow = Get("overflow", "visible");
        std::string ovX = Get("overflow-x", curOverflow);
        std::string ovY = Get("overflow-y", curOverflow);
        bool scrollX_enabled = (ovX == "scroll" || ovX == "auto");
        bool scrollY_enabled = (ovY == "scroll" || ovY == "auto");
        bool isScrollable = scrollX_enabled || scrollY_enabled;

        if (isScrollable) {
            // Calculate content bounds from children
            contentW = 0; contentH = 0;
            auto calcBounds = [&](const std::vector<std::shared_ptr<Element>>& list) {
                for (auto& c : list) {
                    if (!c->visible || c->GetRaw("display") == "none") continue;
                    float relR = (float)(c->x - x + c->w);
                    // Use measuredContentH if available (auto-sized text)
                    float childH = c->measuredContentH > (float)c->h ? c->measuredContentH : (float)c->h;
                    float relB = (float)(c->y - y) + childH;
                    if (relR > contentW) contentW = relR;
                    if (relB > contentH) contentH = relB;
                }
            };
            calcBounds(shadowChildren);
            calcBounds(children);
            // Compute clamped scroll for rendering (don't modify stored scroll yet)
            float maxSX = scrollX_enabled ? (contentW - (float)w) : 0; if (maxSX < 0) maxSX = 0;
            float maxSY = scrollY_enabled ? (contentH - (float)h) : 0; if (maxSY < 0) maxSY = 0;
            float renderSX = scrollX_enabled ? (scrollX < 0 ? 0 : (scrollX > maxSX ? maxSX : scrollX)) : 0;
            float renderSY = scrollY_enabled ? (scrollY < 0 ? 0 : (scrollY > maxSY ? maxSY : scrollY)) : 0;

            // Draw children with scroll offset
            nvgSave(vg);
            nvgIntersectScissor(vg, (float)x, (float)y, (float)w, (float)h);
            nvgTranslate(vg, -renderSX, -renderSY);
            for (auto& c : sortChildren(shadowChildren)) c->Draw(vg);
            for (auto& c : sortChildren(children)) c->Draw(vg);
            nvgRestore(vg);

            // NOW update stored scroll from freshly measured children
            contentW = 0; contentH = 0;
            for (auto& c : children) {
                if (!c->visible || c->GetRaw("display") == "none") continue;
                float cH = c->measuredContentH > (float)c->h ? c->measuredContentH : (float)c->h;
                float relR = (float)(c->x - x + c->w);
                float relB = (float)(c->y - y) + cH;
                if (relR > contentW) contentW = relR;
                if (relB > contentH) contentH = relB;
            }
            for (auto& c : shadowChildren) {
                if (!c->visible || c->GetRaw("display") == "none") continue;
                float cH = c->measuredContentH > (float)c->h ? c->measuredContentH : (float)c->h;
                float relR = (float)(c->x - x + c->w);
                float relB = (float)(c->y - y) + cH;
                if (relR > contentW) contentW = relR;
                if (relB > contentH) contentH = relB;
            }
            // Clamp stored scroll for the next event/frame
            maxSX = scrollX_enabled ? (contentW - (float)w) : 0; if (maxSX < 0) maxSX = 0;
            maxSY = scrollY_enabled ? (contentH - (float)h) : 0; if (maxSY < 0) maxSY = 0;
            if (!scrollX_enabled) scrollX = 0;
            else { if (scrollX < 0) scrollX = 0; if (scrollX > maxSX) scrollX = maxSX; }
            if (!scrollY_enabled) scrollY = 0;
            else { if (scrollY < 0) scrollY = 0; if (scrollY > maxSY) scrollY = maxSY; }

            // ---- Draw scrollbar indicators (CSS-configurable) ----
            float sbW = (float)GetInt("scrollbar-width", 8);
            float sbRad = (float)GetInt("scrollbar-radius", 4);
            float sbPad = 2.0f;
            NVGcolor trackCol = ParseColor(Get("scrollbar-track-color", "#FFFFFF0A"));
            NVGcolor thumbCol = ParseColor(Get("scrollbar-thumb-color", "#FFFFFF33"));
            NVGcolor thumbHoverCol = ParseColor(Get("scrollbar-thumb-hover-color", "#FFFFFF66"));
            NVGcolor activeThumb = isScrollbarHovered ? thumbHoverCol : thumbCol;

            if (scrollY_enabled && contentH > (float)h) {
                float trackX_sb = (float)(x + w) - sbW - sbPad;
                float trackY_sb = (float)y + sbPad;
                float trackH_sb = (float)h - sbPad * 2;
                float ratio = (float)h / contentH;
                float thumbH = trackH_sb * ratio;
                if (thumbH < 24.0f) thumbH = 24.0f;
                if (thumbH > trackH_sb) thumbH = trackH_sb;
                float thumbY = trackY_sb + (trackH_sb - thumbH) * (maxSY > 0 ? scrollY / maxSY : 0);

                nvgBeginPath(vg);
                nvgRoundedRect(vg, trackX_sb, trackY_sb, sbW, trackH_sb, sbRad);
                nvgFillColor(vg, trackCol);
                nvgFill(vg);
                nvgBeginPath(vg);
                nvgRoundedRect(vg, trackX_sb, thumbY, sbW, thumbH, sbRad);
                nvgFillColor(vg, activeThumb);
                nvgFill(vg);
            }
            if (scrollX_enabled && contentW > (float)w) {
                float trackX2 = (float)x + sbPad;
                float trackY2 = (float)(y + h) - sbW - sbPad;
                float trackW2 = (float)w - sbPad * 2;
                float ratioH = (float)w / contentW;
                float thumbW2 = trackW2 * ratioH;
                if (thumbW2 < 24.0f) thumbW2 = 24.0f;
                if (thumbW2 > trackW2) thumbW2 = trackW2;
                float thumbX2 = trackX2 + (trackW2 - thumbW2) * (maxSX > 0 ? scrollX / maxSX : 0);

                nvgBeginPath(vg);
                nvgRoundedRect(vg, trackX2, trackY2, trackW2, sbW, sbRad);
                nvgFillColor(vg, trackCol);
                nvgFill(vg);
                nvgBeginPath(vg);
                nvgRoundedRect(vg, thumbX2, trackY2, thumbW2, sbW, sbRad);
                nvgFillColor(vg, activeThumb);
                nvgFill(vg);
            }
        } else {
            for (auto& c : sortChildren(shadowChildren)) c->Draw(vg);
            for (auto& c : sortChildren(children)) c->Draw(vg);
        }

        NVGcolor borderCol = ParseColor(Get("border"));
        if (borderCol.a > 0.0f) {
            int bw = GetInt("border-width", 1);
            nvgBeginPath(vg);
            if (radius > 0) nvgRoundedRect(vg, (float)x, (float)y, (float)w, (float)h, (float)radius);
            else nvgRect(vg, (float)x, (float)y, (float)w, (float)h);
            nvgStrokeColor(vg, borderCol);
            nvgStrokeWidth(vg, (float)bw);
            nvgStroke(vg);
        }
        
        nvgRestore(vg);
    }

    // ---- Hit Testing ----
    bool HitTestRay(int mx, int my, std::vector<Element*>& hits) {
        if (!visible || GetRaw("display") == "none") return false;

        bool inBounds = (mx >= x && mx <= x + w && my >= y && my <= y + h);

        // For scrollable containers, adjust coords for children and restrict to bounds
        std::string ov = Get("overflow", "visible");
        std::string ovXht = Get("overflow-x", ov);
        std::string ovYht = Get("overflow-y", ov);
        bool htScrollX = (ovXht == "scroll" || ovXht == "auto");
        bool htScrollY = (ovYht == "scroll" || ovYht == "auto");
        bool isScrollable = htScrollX || htScrollY;
        int cmx = mx, cmy = my;
        if (isScrollable) {
            if (!inBounds) {
                // Can't click children outside a scrollable container
                // But still check if we hit the container itself below
            } else {
                cmx = mx + (int)scrollX;
                cmy = my + (int)scrollY;
            }
        }

        auto sortChildren = [](const std::vector<std::shared_ptr<Element>>& list) {
            std::vector<std::shared_ptr<Element>> sorted = list;
            std::stable_sort(sorted.begin(), sorted.end(), [](const std::shared_ptr<Element>& a, const std::shared_ptr<Element>& b) {
                return a->GetInt("z-index") < b->GetInt("z-index");
            });
            return sorted;
        };

        if (!isScrollable || inBounds) {
            auto sortedChildren = sortChildren(children);
            for (auto it = sortedChildren.rbegin(); it != sortedChildren.rend(); ++it) {
                if ((*it)->HitTestRay(cmx, cmy, hits)) return true;
            }

            auto sortedShadow = sortChildren(shadowChildren);
            for (auto it = sortedShadow.rbegin(); it != sortedShadow.rend(); ++it) {
                if ((*it)->HitTestRay(cmx, cmy, hits)) return true;
            }
        }

        if (inBounds) {
            std::string ev = Get("events", Get("pointer-events", "catch"));
            if (ev == "pass" || ev == "none") {
                return false;
            } else if (ev == "catchAndPass") {
                hits.push_back(this);
                return false;
            } else if (ev == "block") {
                return true;
            } else {
                hits.push_back(this);
                return true;
            }
        }
        return false;
    }

    virtual Element* HitTest(int mx, int my) {
        std::vector<Element*> hits;
        HitTestRay(mx, my, hits);
        return hits.empty() ? nullptr : hits.front();
    }
    
    virtual std::vector<Element*> HitTestAll(int mx, int my) {
        std::vector<Element*> hits;
        HitTestRay(mx, my, hits);
        return hits;
    }

    // ---- Event Dispatching ----
    void DispatchEvent(UIEvent& e) {
        e.currentTarget = this;
        // Listeners are dispatched by the script bridge (not stored here as C++ callbacks)
    }

    // ---- Utilities ----
    static std::wstring S2W(const std::string& s) {
        if (s.empty()) return L"";
        int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, NULL, 0);
        std::wstring ws(len, 0);
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), -1, &ws[0], len);
        return ws;
    }

    NVGcolor ParseColor(const std::string& val) {
        float globalOp = GetFloat("opacity", 1.0f);
        std::string hex = val;
        
        if (hex.find("color-mix(") == 0) {
            size_t start = 10;
            size_t end = hex.find(')', start);
            if (end != std::string::npos) {
                std::string inner = hex.substr(start, end - start);
                size_t c1 = inner.find(',');
                if (c1 != std::string::npos) {
                    size_t c2 = inner.find(',', c1 + 1);
                    if (c2 != std::string::npos) {
                        std::string p1 = inner.substr(0, c1);
                        std::string p2 = inner.substr(c1 + 1, c2 - c1 - 1);
                        std::string p3 = inner.substr(c2 + 1);
                        auto trim = [](std::string& s) {
                            while(!s.empty() && isspace((unsigned char)s.back())) s.pop_back();
                            while(!s.empty() && isspace((unsigned char)s.front())) s.erase(s.begin());
                            if(s.size()>=2 && s.front()=='"' && s.back()=='"') s = s.substr(1, s.size()-2);
                        };
                        trim(p1); trim(p2); trim(p3);
                        NVGcolor cA = ParseHex(p1);
                        NVGcolor cB = ParseHex(p2);
                        float mix = 0.5f;
                        char* pEnd;
                        float d = (float)strtod(p3.c_str(), &pEnd);
                        if (pEnd != p3.c_str()) mix = d;
                        
                        float r = (cA.r * (1 - mix) + cB.r * mix);
                        float g = (cA.g * (1 - mix) + cB.g * mix);
                        float b = (cA.b * (1 - mix) + cB.b * mix);
                        float a = (cA.a * (1 - mix) + cB.a * mix);
                        return nvgRGBAf(r, g, b, a * globalOp);
                    }
                }
            }
        }
        
        NVGcolor c = ParseHex(hex);
        return nvgRGBAf(c.r, c.g, c.b, c.a * globalOp);
    }

    static NVGcolor ParseHex(const std::string& hex) {
        if (hex.empty() || hex[0] != '#') return nvgRGBAf(0, 0, 0, 0);
        int r = 0, g = 0, b = 0, a = 255;
        if (hex.length() == 7) sscanf_s(hex.c_str(), "#%02x%02x%02x", &r, &g, &b);
        else if (hex.length() == 4) {
            sscanf_s(hex.c_str(), "#%1x%1x%1x", &r, &g, &b);
            r *= 17; g *= 17; b *= 17;
        }
        else if (hex.length() == 9) sscanf_s(hex.c_str(), "#%02x%02x%02x%02x", &r, &g, &b, &a);
        return nvgRGBA(r, g, b, a);
    }
};
