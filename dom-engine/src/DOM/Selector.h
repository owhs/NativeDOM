#pragma once
#include "Element.h"
#include <string>
#include <vector>
#include <memory>
#include <set>

// Represents a single parsed selector segment (e.g., "div.cls#id[attr='val']:hover")
struct SelectorSegment {
    std::string tag;
    std::string id;
    std::vector<std::string> classes;
    std::vector<std::tuple<std::string, std::string, std::string>> attributes; // (attr, op, value)
    std::vector<std::string> pseudoClasses;
    std::string pseudoArg; // for :nth-child(n), :has(...), :not(...)
    char combinator = ' '; // ' ' (descendant), '>' (child)
};

class Selector {
public:
    // ---- High-level API ----
    static std::vector<std::shared_ptr<Element>> QueryAll(
        std::shared_ptr<Element> root, 
        const std::string& selectorStr, 
        bool searchShadow = false) 
    {
        std::vector<std::shared_ptr<Element>> results;
        if (!root) return results;

        // Split by comma for multiple selectors
        auto selectors = splitComma(selectorStr);
        std::set<Element*> seen;

        for (auto& sel : selectors) {
            auto segments = parseSelector(sel);
            if (segments.empty()) continue;

            auto matched = matchSegments(root, segments, 0, searchShadow, true);
            for (auto& el : matched) {
                if (seen.insert(el.get()).second) {
                    results.push_back(el);
                }
            }
        }
        return results;
    }

    static std::shared_ptr<Element> Query(
        std::shared_ptr<Element> root, 
        const std::string& selector, 
        bool searchShadow = false) 
    {
        auto results = QueryAll(root, selector, searchShadow);
        return results.empty() ? nullptr : results[0];
    }

    // Query from a specific element context (like element.querySelector)
    static std::vector<std::shared_ptr<Element>> QueryAllFrom(
        std::shared_ptr<Element> context,
        const std::string& selector,
        bool searchShadow = false)
    {
        return QueryAll(context, selector, searchShadow);
    }

    // Check if a single element matches a selector string
    static bool Matches(std::shared_ptr<Element> el, const std::string& selectorStr) {
        if (!el) return false;
        auto selectors = splitComma(selectorStr);
        for (auto& sel : selectors) {
            auto segments = parseSelector(sel);
            if (segments.size() == 1 && matchesSegment(el, segments[0])) return true;
        }
        return false;
    }

private:
    // ---- Selector Parsing ----
    static std::vector<std::string> splitComma(const std::string& s) {
        std::vector<std::string> result;
        std::string current;
        int depth = 0;
        for (char c : s) {
            if (c == '(') depth++;
            else if (c == ')') depth--;
            if (c == ',' && depth == 0) {
                auto trimmed = trim(current);
                if (!trimmed.empty()) result.push_back(trimmed);
                current.clear();
            } else {
                current += c;
            }
        }
        auto trimmed = trim(current);
        if (!trimmed.empty()) result.push_back(trimmed);
        return result;
    }

    static std::string trim(const std::string& s) {
        size_t start = s.find_first_not_of(" \t\r\n");
        size_t end = s.find_last_not_of(" \t\r\n");
        if (start == std::string::npos) return "";
        return s.substr(start, end - start + 1);
    }

    static std::vector<SelectorSegment> parseSelector(const std::string& sel) {
        std::vector<SelectorSegment> segments;
        std::string s = trim(sel);
        if (s.empty()) return segments;

        size_t i = 0;
        char pendingCombinator = ' ';

        while (i < s.size()) {
            // Skip whitespace
            while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) i++;
            if (i >= s.size()) break;

            // Check for combinator
            if (s[i] == '>' || s[i] == '+' || s[i] == '~') {
                pendingCombinator = s[i];
                i++;
                while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) i++;
                if (i >= s.size()) break;
            }

            SelectorSegment seg;
            seg.combinator = segments.empty() ? ' ' : pendingCombinator;
            pendingCombinator = ' ';

            // Parse tag
            if (i < s.size() && (std::isalpha(s[i]) || s[i] == '*')) {
                while (i < s.size() && (std::isalnum(s[i]) || s[i] == '-' || s[i] == '_' || s[i] == '*')) {
                    seg.tag += s[i++];
                }
            }

            // Parse id, class, attribute, pseudo selectors (chained)
            while (i < s.size()) {
                if (s[i] == '#') {
                    i++;
                    std::string idVal;
                    while (i < s.size() && (std::isalnum(s[i]) || s[i] == '-' || s[i] == '_')) idVal += s[i++];
                    seg.id = idVal;
                } 
                else if (s[i] == '.') {
                    i++;
                    std::string cls;
                    while (i < s.size() && (std::isalnum(s[i]) || s[i] == '-' || s[i] == '_')) cls += s[i++];
                    seg.classes.push_back(cls);
                }
                else if (s[i] == '[') {
                    i++;
                    std::string attr, op, val;
                    while (i < s.size() && s[i] != '=' && s[i] != ']' && s[i] != '*' && s[i] != '^' && s[i] != '$') attr += s[i++];
                    attr = trim(attr);
                    if (i < s.size() && s[i] != ']') {
                        // Read operator
                        if (s[i] == '*' || s[i] == '^' || s[i] == '$') { op += s[i++]; }
                        if (i < s.size() && s[i] == '=') { op += s[i++]; }
                        // Read value (quoted)
                        while (i < s.size() && (s[i] == '\'' || s[i] == '"' || s[i] == ' ')) i++;
                        while (i < s.size() && s[i] != '\'' && s[i] != '"' && s[i] != ']') val += s[i++];
                        while (i < s.size() && (s[i] == '\'' || s[i] == '"')) i++;
                    }
                    if (i < s.size() && s[i] == ']') i++;
                    seg.attributes.push_back({ attr, op.empty() ? "" : op, val });
                }
                else if (s[i] == ':') {
                    i++;
                    std::string pseudo;
                    while (i < s.size() && (std::isalnum(s[i]) || s[i] == '-')) pseudo += s[i++];
                    // Check for argument: :nth-child(n), :has(...), :not(...)
                    if (i < s.size() && s[i] == '(') {
                        i++; // skip (
                        std::string arg;
                        int depth = 1;
                        while (i < s.size() && depth > 0) {
                            if (s[i] == '(') depth++;
                            else if (s[i] == ')') { depth--; if (depth == 0) break; }
                            arg += s[i++];
                        }
                        if (i < s.size()) i++; // skip )
                        seg.pseudoClasses.push_back(pseudo);
                        seg.pseudoArg = arg;
                    } else {
                        seg.pseudoClasses.push_back(pseudo);
                    }
                }
                else break;
            }

            segments.push_back(seg);
        }
        return segments;
    }

    // ---- Matching ----
    static bool matchesSegment(std::shared_ptr<Element> el, const SelectorSegment& seg) {
        if (!el) return false;

        // Tag
        if (!seg.tag.empty() && seg.tag != "*" && el->tag != seg.tag) return false;

        // ID
        if (!seg.id.empty() && el->id != seg.id) return false;

        // Classes
        for (auto& cls : seg.classes) {
            if (el->classes.count(cls) == 0) return false;
        }

        // Attributes
        for (auto& [attr, op, val] : seg.attributes) {
            if (op.empty()) {
                // Just check existence
                bool hasAttr = false;
                for (auto& pr : el->props) { if (pr.first == attr) { hasAttr = true; break; } }
                if (!hasAttr) return false;
            } else {
                std::string elVal = el->Get(attr);
                if (op == "=") { if (elVal != val) return false; }
                else if (op == "*=") { if (elVal.find(val) == std::string::npos) return false; }
                else if (op == "^=") { if (elVal.find(val) != 0) return false; }
                else if (op == "$=") {
                    if (elVal.size() < val.size() || elVal.substr(elVal.size() - val.size()) != val) return false;
                }
            }
        }

        // Pseudo classes
        for (auto& pseudo : seg.pseudoClasses) {
            if (pseudo == "hover" && !el->isHovered) return false;
            if (pseudo == "focused" && !el->isFocused) return false;
            if (pseudo == "active" && !el->isActive) return false;
            if (pseudo == "first-child") {
                if (!el->parent || el->parent->children.empty() || el->parent->children[0].get() != el.get()) return false;
            }
            if (pseudo == "last-child") {
                if (!el->parent || el->parent->children.empty() || el->parent->children.back().get() != el.get()) return false;
            }
            if (pseudo == "nth-child") {
                if (!el->parent) return false;
                char* nEnd;
                int n = (int)strtol(seg.pseudoArg.c_str(), &nEnd, 10);
                if (nEnd == seg.pseudoArg.c_str()) return false;
                if (el->getChildIndex() != n - 1) return false; // 1-indexed
            }
            if (pseudo == "not") {
                // :not(.class) or :not(#id) etc
                if (Matches(el, seg.pseudoArg)) return false;
            }
            if (pseudo == "has") {
                // :has(.child) — check if descendants match
                auto found = QueryAll(el, seg.pseudoArg, false);
                if (found.empty()) return false;
            }
        }

        return true;
    }

    static std::vector<std::shared_ptr<Element>> matchSegments(
        std::shared_ptr<Element> root,
        const std::vector<SelectorSegment>& segments,
        size_t segIdx,
        bool searchShadow,
        bool isRoot = false)
    {
        std::vector<std::shared_ptr<Element>> results;
        if (segIdx >= segments.size()) return results;

        auto& seg = segments[segIdx];
        bool isLastSegment = (segIdx == segments.size() - 1);

        // Collect candidates
        std::vector<std::shared_ptr<Element>> candidates;

        if (segIdx == 0) {
            // For the first segment, collect all descendants
            collectDescendants(root, candidates, searchShadow, true);
        } else {
            candidates = { root };
        }

        for (auto& candidate : candidates) {
            if (!matchesSegment(candidate, seg)) continue;

            if (isLastSegment) {
                results.push_back(candidate);
            } else {
                // Next segment: determine scope based on combinator
                auto& nextSeg = segments[segIdx + 1];
                if (nextSeg.combinator == '>') {
                    // Direct children only
                    for (auto& child : candidate->children) {
                        auto sub = matchSegments(child, segments, segIdx + 1, searchShadow);
                        results.insert(results.end(), sub.begin(), sub.end());
                    }
                    if (searchShadow) {
                        for (auto& child : candidate->shadowChildren) {
                            auto sub = matchSegments(child, segments, segIdx + 1, searchShadow);
                            results.insert(results.end(), sub.begin(), sub.end());
                        }
                    }
                } else if (nextSeg.combinator == '+') {
                    std::vector<std::shared_ptr<Element>> siblings;
                    if (candidate->parent) siblings = candidate->parent->children;
                    else if (candidate->shadowHost) siblings = candidate->shadowHost->shadowChildren;
                    
                    for (size_t i = 0; i < siblings.size(); i++) {
                        if (siblings[i].get() == candidate.get() && i + 1 < siblings.size()) {
                            auto sub = matchSegments(siblings[i+1], segments, segIdx + 1, searchShadow);
                            results.insert(results.end(), sub.begin(), sub.end());
                            break;
                        }
                    }
                } else if (nextSeg.combinator == '~') {
                    std::vector<std::shared_ptr<Element>> siblings;
                    if (candidate->parent) siblings = candidate->parent->children;
                    else if (candidate->shadowHost) siblings = candidate->shadowHost->shadowChildren;
                    
                    bool foundSelf = false;
                    for (size_t i = 0; i < siblings.size(); i++) {
                        if (foundSelf) {
                            auto sub = matchSegments(siblings[i], segments, segIdx + 1, searchShadow);
                            results.insert(results.end(), sub.begin(), sub.end());
                        }
                        if (siblings[i].get() == candidate.get()) foundSelf = true;
                    }
                } else {
                    // Descendant: search all descendants
                    std::vector<std::shared_ptr<Element>> desc;
                    collectDescendants(candidate, desc, searchShadow, false);
                    for (auto& d : desc) {
                        auto sub = matchSegments(d, segments, segIdx + 1, searchShadow);
                        results.insert(results.end(), sub.begin(), sub.end());
                    }
                }
            }
        }

        return results;
    }

    static void collectDescendants(
        std::shared_ptr<Element> node,
        std::vector<std::shared_ptr<Element>>& out,
        bool searchShadow,
        bool includeRoot)
    {
        if (!node) return;
        if (!includeRoot || true) { // Always collect, skip root check in matching
            if (!includeRoot || node->parent || node->shadowHost) {  
                // skip the actual root itself for first segment matching
            }
            out.push_back(node);
        }
        for (auto& c : node->children) collectDescendants(c, out, searchShadow, false);
        if (searchShadow) {
            for (auto& c : node->shadowChildren) collectDescendants(c, out, searchShadow, false);
        }
    }
};
