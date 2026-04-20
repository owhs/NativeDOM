#pragma once
#include <cstdio>
#include <string>
#include <cctype>
#include <vector>
#include <vector>
#include "../DOM/Element.h"

class DOMParser {
    struct CssRule {
        std::string selector;
        std::vector<std::pair<std::string, std::string>> props;
    };
    static inline std::vector<CssRule> globalCssRules;
    static inline std::vector<std::pair<std::string, std::shared_ptr<Element>>> componentRegistry;

    static std::shared_ptr<Element> CloneTree(std::shared_ptr<Element> node) {
        if (!node) return nullptr;
        auto clone = std::make_shared<Element>();
        clone->tag = node->tag;
        clone->id = node->id;
        clone->classes = node->classes;
        clone->props = node->props;
        clone->propScores = node->propScores;
        clone->pseudoProps = node->pseudoProps;
        clone->innerText = node->innerText;
        clone->tabIndex = node->tabIndex;
        for (auto& c : node->shadowChildren) clone->Adopt(CloneTree(c), true);
        for (auto& c : node->children) clone->Adopt(CloneTree(c), false);
        return clone;
    }

public:
    static inline std::string globalScripts = R"(
        sys.waitForWindow = function(target, callback, timeoutMs, pollTickMs) {
            let start = sys.time();
            let timer = setInterval(function() {
                if (sys.time() - start >= (timeoutMs || 5000)) {
                    clearInterval(timer);
                    if (callback) callback(false);
                    return;
                }
                let hwnd = sys.window.find(target);
                if (hwnd) {
                    clearInterval(timer);
                    if (callback) callback(true);
                }
            }, pollTickMs || 100);
        };
    )";

    // Reset state for fresh parse
    static void Reset() {
        globalCssRules.clear();
        componentRegistry.clear();
        globalScripts = R"(
        sys.waitForWindow = function(target, callback, timeoutMs, pollTickMs) {
            let start = sys.time();
            let timer = setInterval(function() {
                if (sys.time() - start >= (timeoutMs || 5000)) {
                    clearInterval(timer);
                    if (callback) callback(false);
                    return;
                }
                let hwnd = sys.window.find(target);
                if (hwnd) {
                    clearInterval(timer);
                    if (callback) callback(true);
                }
            }, pollTickMs || 100);
        };
    )";
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

    static std::vector<std::pair<std::string, std::string>> ParseAttributes(const std::string& attrStr) {
        std::vector<std::pair<std::string, std::string>> res;
        size_t p = 0;
        while (p < attrStr.size()) {
            size_t eq = attrStr.find('=', p);
            if (eq == std::string::npos) break;
            size_t start = eq;
            while (start > p && attrStr[start - 1] != ' ' && attrStr[start - 1] != '\t') start--;
            std::string key = attrStr.substr(start, eq - start);
            
            size_t quoteStart = attrStr.find('"', eq);
            if (quoteStart == std::string::npos) break;
            size_t quoteEnd = attrStr.find('"', quoteStart + 1);
            if (quoteEnd == std::string::npos) break;
            
            std::string val = attrStr.substr(quoteStart + 1, quoteEnd - quoteStart - 1);
            res.push_back({key, UnescapeXML(val)});
            p = quoteEnd + 1;
        }
        return res;
    }

    // ---- Parse a .dom file ----
    static std::shared_ptr<Element> ParseDOM(const std::string& filepath) {
        FILE* f = fopen(filepath.c_str(), "rb");
        if (!f) return nullptr;
        fseek(f, 0, SEEK_END);
        long size = ftell(f);
        fseek(f, 0, SEEK_SET);
        if (size < 0) { fclose(f); return nullptr; }
        std::string content(size, '\0');
        if (size > 0) fread(&content[0], 1, size, f);
        fclose(f);
        return ParseContent(content, filepath);
    }

    static std::string BundleDOMRecursive(const std::string& filepath, std::vector<std::string>& alreadyIncluded) {
        FILE* f = fopen(filepath.c_str(), "rb");
        if (!f) return "<!-- failed to load " + filepath + " -->";
        fseek(f, 0, SEEK_END);
        long size = ftell(f);
        fseek(f, 0, SEEK_SET);
        std::string content(size, '\0');
        if (size > 0) fread(&content[0], 1, size, f);
        fclose(f);

        std::string dir;
        size_t slash = filepath.find_last_of("/\\");
        if (slash != std::string::npos) dir = filepath.substr(0, slash + 1);

        size_t importP = 0;
        while ((importP = content.find("<import ", importP)) != std::string::npos) {
            size_t endP = content.find('>', importP);
            if (endP == std::string::npos) break;
            std::string tagStr = content.substr(importP, endP - importP + 1);
            
            size_t srcPos = tagStr.find("src=\"");
            size_t asPos = tagStr.find("as=\"");
            if (srcPos != std::string::npos && asPos != std::string::npos) {
                size_t srcEnd = tagStr.find('"', srcPos + 5);
                size_t asEnd = tagStr.find('"', asPos + 4);
                if (srcEnd != std::string::npos && asEnd != std::string::npos) {
                    std::string src = tagStr.substr(srcPos + 5, srcEnd - srcPos - 5);
                    std::string as = tagStr.substr(asPos + 4, asEnd - asPos - 4);
                    
                    std::string fullPath = dir + src;
                    bool found = false;
                    for (auto& p : alreadyIncluded) { if (p == fullPath) { found = true; break;} }
                    
                    if (!found) {
                        alreadyIncluded.push_back(fullPath);
                        std::string childContent = BundleDOMRecursive(fullPath, alreadyIncluded);
                        
                        std::string inlineElement = "\n<component as=\"" + as + "\">\n" + childContent + "\n</component>\n";
                        content.replace(importP, endP - importP + 1, inlineElement);
                        importP += inlineElement.size();
                        continue;
                    } else {
                        content.replace(importP, endP - importP + 1, "");
                        continue;
                    }
                }
            }
            importP = endP + 1;
        }
        return content;
    }

    // Produce a single embedded application string resolving all imports recursively
    static std::string BundleDOM(const std::string& rootFilepath) {
        std::vector<std::string> included;
        included.push_back(rootFilepath);
        return BundleDOMRecursive(rootFilepath, included);
    }

    // Parse from string content (useful for embedded resources)
    static std::shared_ptr<Element> ParseContent(std::string content, const std::string& filepath = "") {
        extern void sysLog(const std::string&);

        // ---- Handle imports ----
        size_t importP = 0;
        while ((importP = content.find("<import ", importP)) != std::string::npos) {
            size_t endP = content.find('>', importP);
            if (endP == std::string::npos) break;
            std::string tagStr = content.substr(importP, endP - importP + 1);
            
            size_t srcPos = tagStr.find("src=\"");
            size_t asPos = tagStr.find("as=\"");
            if (srcPos != std::string::npos && asPos != std::string::npos) {
                size_t srcEnd = tagStr.find('"', srcPos + 5);
                size_t asEnd = tagStr.find('"', asPos + 4);
                if (srcEnd != std::string::npos && asEnd != std::string::npos) {
                    std::string src = tagStr.substr(srcPos + 5, srcEnd - srcPos - 5);
                    std::string as = tagStr.substr(asPos + 4, asEnd - asPos - 4);
                    
                    std::string dir;
                    size_t slash = filepath.find_last_of("/\\");
                    if (slash != std::string::npos) dir = filepath.substr(0, slash + 1);

                    bool foundComp = false;
                    for (auto& cr : componentRegistry) { if (cr.first == as) { foundComp = true; break; } }
                    if (!foundComp) {
                        auto comp = ParseDOM(dir + src);
                        if (comp) componentRegistry.push_back({as, comp});
                        else sysLog("[Parser] Error: Failed to resolve import '" + src + "' into component <" + as + "> inside " + filepath);
                    }
                }
            }
            importP = endP + 1;
        }

        // ---- Parse inline <component> blocks ----
        size_t compP = 0;
        while ((compP = content.find("<component ", compP)) != std::string::npos) {
            size_t endP = content.find('>', compP);
            if (endP == std::string::npos) break;
            std::string tagStr = content.substr(compP, endP - compP + 1);
            
            size_t asPos = tagStr.find("as=\"");
            if (asPos != std::string::npos) {
                size_t asEnd = tagStr.find('"', asPos + 4);
                if (asEnd != std::string::npos) {
                    std::string as = tagStr.substr(asPos + 4, asEnd - asPos - 4);
                    
                    size_t closingTag = content.find("</component>", endP);
                    if (closingTag != std::string::npos) {
                        std::string compContent = content.substr(endP + 1, closingTag - endP - 1);
                        bool foundComp = false;
                        for (auto& cr : componentRegistry) { if (cr.first == as) { foundComp = true; break; } }
                        if (!foundComp) {
                            auto comp = ParseContent(compContent, filepath);
                            if (comp) componentRegistry.push_back({as, comp});
                            else sysLog("[Parser] Error: Failed to compile inline <component as=\"" + as + "\"> sequence inside " + filepath);

                            size_t blockEnd = closingTag + 12;
                            for (size_t i = compP; i < blockEnd; i++) {
                                if (content[i] != '\n' && content[i] != '\r') {
                                    content[i] = ' ';
                                }
                            }
                        }
                    } else {
                        sysLog("[Parser] Error: Missing </component> closing block for <component as=\"" + as + "\"> inside " + filepath);
                    }
                }
            }
            compP = endP + 1;
        }

        // ---- Parse <style> blocks ----
        parseStyles(content);

        // ---- Extract <script> blocks ----
        size_t scriptPos = 0;
        while (true) {
            size_t sStart = content.find("<script>", scriptPos);
            if (sStart == std::string::npos) break;
            size_t sEnd = content.find("</script>", sStart);
            if (sEnd == std::string::npos) {
                sysLog("[Parser] Error: Missing </script> tag closing block in " + filepath);
                break;
            }
            globalScripts += "\n" + content.substr(sStart + 8, sEnd - sStart - 8);
            scriptPos = sEnd + 9;
        }

        // ---- Parse <ui> tree ----
        size_t uiStart = content.find("<ui");
        size_t uiEnd = content.find("</ui>");
        if (uiStart == std::string::npos || uiEnd == std::string::npos) {
            if (uiStart == std::string::npos) sysLog("[Parser] Critical fail: Missing opening <ui> root node block in " + filepath);
            else sysLog("[Parser] Critical fail: Missing closing </ui> root node block in " + filepath);
            return nullptr;
        }
        std::string ui = content.substr(uiStart, uiEnd - uiStart + 5);

        auto parsed = parseUI(ui);
        if (!parsed) sysLog("[Parser] Critical fail: Internal node tree expansion returned explicitly null in " + filepath + " -> verify proper HTML-like closure semantics!");
        return parsed;
    }

private:
    static void parseStyles(const std::string& content) {
        size_t pos = 0;
        while (true) {
            size_t styleStart = content.find("<style>", pos);
            if (styleStart == std::string::npos) break;
            size_t styleEnd = content.find("</style>", styleStart);
            if (styleEnd == std::string::npos) break;

            std::string styles = content.substr(styleStart + 7, styleEnd - styleStart - 7);
            
            // Parse CSS rules: selector { prop: val; }
            // Support nested braces, complex selectors
            size_t i = 0;
            while (i < styles.size()) {
                // Skip whitespace
                while (i < styles.size() && (styles[i] == ' ' || styles[i] == '\t' || styles[i] == '\r' || styles[i] == '\n')) i++;
                if (i >= styles.size()) break;
                
                // Skip comments
                if (i + 1 < styles.size() && styles[i] == '/' && styles[i + 1] == '*') {
                    i += 2;
                    while (i + 1 < styles.size() && !(styles[i] == '*' && styles[i + 1] == '/')) i++;
                    i += 2;
                    continue;
                }

                // Read selector
                std::string selector;
                while (i < styles.size() && styles[i] != '{') selector += styles[i++];
                // Trim
                while (!selector.empty() && (selector.back() == ' ' || selector.back() == '\r' || selector.back() == '\n' || selector.back() == '\t')) selector.pop_back();
                while (!selector.empty() && (selector.front() == ' ' || selector.front() == '\r' || selector.front() == '\n' || selector.front() == '\t')) selector.erase(selector.begin());

                if (i >= styles.size()) break;
                i++; // skip {

                // Read properties
                std::string propsBlock;
                int depth = 1;
                while (i < styles.size() && depth > 0) {
                    if (styles[i] == '{') depth++;
                    else if (styles[i] == '}') { depth--; if (depth == 0) break; }
                    propsBlock += styles[i++];
                }
                if (i < styles.size()) i++; // skip }

                // Parse individual properties
                size_t sp = 0;
                while (sp < propsBlock.size()) {
                    size_t semi = propsBlock.find(';', sp);
                    if (semi == std::string::npos) break;
                    std::string statement = propsBlock.substr(sp, semi - sp);
                    size_t colon = statement.find(':');
                    if (colon != std::string::npos) {
                        std::string k = statement.substr(0, colon);
                        std::string v = statement.substr(colon + 1);
                        // Trim k
                        while(!k.empty() && (k.back()==' ' || k.back()=='\r' || k.back()=='\n' || k.back()=='\t')) k.pop_back();
                        while(!k.empty() && (k.front()==' ' || k.front()=='\r' || k.front()=='\n' || k.front()=='\t')) k.erase(k.begin());
                        // Trim v
                        while (!v.empty() && (v.back()==' ' || v.back()=='\r' || v.back()=='\n' || v.back()=='\t')) v.pop_back();
                        while (!v.empty() && (v.front()==' ' || v.front()=='\r' || v.front()=='\n' || v.front()=='\t')) v.erase(v.begin());
                        if (v.size() >= 2 && v.front() == '"' && v.back() == '"') v = v.substr(1, v.size() - 2);
                        if (!k.empty()) {
                            bool foundSel = false;
                            for (auto& rule : globalCssRules) {
                                if (rule.selector == selector) {
                                    bool foundProp = false;
                                    for (auto& pr : rule.props) { if (pr.first == k) { pr.second = v; foundProp = true; break; } }
                                    if (!foundProp) rule.props.push_back({k, v});
                                    foundSel = true; break;
                                }
                            }
                            if (!foundSel) {
                                CssRule nr; nr.selector = selector; nr.props.push_back({k, v});
                                globalCssRules.push_back(nr);
                            }
                        }
                    }
                    sp = semi + 1;
                }
            }

            pos = styleEnd + 8;
        }
    }

    static std::shared_ptr<Element> parseUI(const std::string& ui) {
        std::vector<std::shared_ptr<Element>> stack;
        std::shared_ptr<Element> root = nullptr;
        size_t pos = 0;

        while (pos < ui.size()) {
            size_t openBracket = ui.find('<', pos);
            if (openBracket == std::string::npos) break;

            // Extract text content between tags
            std::string text = ui.substr(pos, openBracket - pos);
            size_t nonSpace = text.find_first_not_of(" \t\r\n");
            if (nonSpace != std::string::npos) {
                text.erase(0, nonSpace);
                text.erase(text.find_last_not_of(" \t\r\n") + 1);
                if (!text.empty() && !stack.empty()) stack.back()->innerText = UnescapeXML(text);
            }

            // Skip comments <!-- ... -->
            if (ui.substr(openBracket, 4) == "<!--") {
                size_t commentEnd = ui.find("-->", openBracket);
                pos = commentEnd != std::string::npos ? commentEnd + 3 : ui.size();
                continue;
            }

            size_t closeBracket = ui.find('>', openBracket);
            if (closeBracket == std::string::npos) break;
            std::string tagContent = ui.substr(openBracket + 1, closeBracket - openBracket - 1);
            pos = closeBracket + 1;

            if (tagContent.empty()) continue;
            // Skip import/style/script tags
            if (tagContent.find("import ") == 0 || tagContent.find("style") == 0 ||
                tagContent.find("/style") == 0 || tagContent.find("script") == 0 ||
                tagContent.find("/script") == 0) continue;

            // Closing tag
            if (tagContent[0] == '/') {
                if (!stack.empty()) stack.pop_back();
                continue;
            }

            // Opening tag
            bool selfClosing = false;
            if (!tagContent.empty() && tagContent.back() == '/') {
                selfClosing = true;
                tagContent.pop_back();
            }

            size_t spacePos = tagContent.find(' ');
            std::string tagName = tagContent.substr(0, spacePos);
            std::string attrStr = spacePos != std::string::npos ? tagContent.substr(spacePos) : "";

            auto el = std::make_shared<Element>();
            el->tag = tagName;
            el->props = ParseAttributes(attrStr);
            auto getProp = [&](const std::string& key) {
                for (auto& kv : el->props) if (kv.first == key) return kv.second;
                return std::string("");
            };

            auto idProp = getProp("id"); if (!idProp.empty()) el->id = idProp;
            auto textProp = getProp("text"); if (!textProp.empty()) el->innerText = textProp;
            auto tabProp = getProp("tabindex"); if (!tabProp.empty()) el->tabIndex = (int)strtol(tabProp.c_str(), nullptr, 10);

            // Parse classes
            auto clsProp = getProp("class");
            if (!clsProp.empty()) {
                std::string clsStr = clsProp;
                size_t start = 0;
                while (start < clsStr.size()) {
                    while (start < clsStr.size() && isspace((unsigned char)clsStr[start])) start++;
                    if (start >= clsStr.size()) break;
                    size_t end = start;
                    while (end < clsStr.size() && !isspace((unsigned char)clsStr[end])) end++;
                    el->classes.insert(clsStr.substr(start, end - start));
                    start = end;
                }
            }

            for (auto& cr : componentRegistry) {
                if (cr.first == tagName) {
                    for (auto& kv : cr.second->props) {
                        bool has = false;
                        for (auto& p : el->props) { if (p.first == kv.first) { has = true; break; } }
                        if (!has && kv.first != "id") el->props.push_back({kv.first, kv.second});
                    }
                    el->Adopt(CloneTree(cr.second), true);
                    break;
                }
            }

            // Apply CSS rules with correct cascading priority
            applyStyles(el, el->props);

            // Add to tree
            if (stack.empty()) root = el;
            else stack.back()->Adopt(el, false);

            if (!selfClosing) stack.push_back(el);
        }

        return root;
    }

    static void applyStyles(std::shared_ptr<Element> el, const std::vector<std::pair<std::string, std::string>>& inlineAttrs) {
        auto applyPseudo = [&](const std::string& selector, const std::string& state, int baseScore) {
            for (auto& rule : globalCssRules) {
                if (rule.selector == selector + ":" + state) {
                    for (auto& kv : rule.props) el->SetPseudoProp(state, kv.first, kv.second, baseScore);
                }
            }
        };

        auto applyStyle = [&](const std::string& selector, int baseScore) {
            for (auto& rule : globalCssRules) {
                if (rule.selector == selector) {
                    for (auto& kv : rule.props) el->SetProp(kv.first, kv.second, baseScore);
                } else if (rule.selector.find(":host([") == 0) {
                    size_t endHost = rule.selector.find("]) ");
                    if (endHost != std::string::npos) {
                        std::string cond = rule.selector.substr(6, endHost - 5);
                        std::string rest = rule.selector.substr(endHost + 3);
                        if (rest == selector) {
                            for (auto& kv : rule.props) el->SetPseudoProp(cond, kv.first, kv.second, baseScore + 50);
                        }
                    }
                }
            }
            applyPseudo(selector, "hover", baseScore);
            applyPseudo(selector, "focused", baseScore);
            applyPseudo(selector, "active", baseScore);
        };

        // Apply in specificity order
        applyStyle(el->tag, 10);
        for (auto& cls : el->classes) applyStyle("." + cls, 20);
        if (!el->id.empty()) applyStyle("#" + el->id, 30);

        // Finally restore inline properties so they take ultimate priority
        for (auto& kv : inlineAttrs) {
            size_t colon = kv.first.find(':');
            if (colon != std::string::npos) {
                std::string state = kv.first.substr(0, colon);
                std::string prop = kv.first.substr(colon + 1);
                el->SetPseudoProp(state, prop, kv.second, 40);
            } else {
                el->SetProp(kv.first, kv.second, 40);
            }
        }
    }
};
