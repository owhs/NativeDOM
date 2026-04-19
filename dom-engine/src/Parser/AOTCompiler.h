#pragma once
#include <string>
#include <memory>
#include <vector>
#include "../DOM/Element.h"

class AOTCompiler {
private:
    static std::string EscapeString(const std::string& str) {
        std::string res;
        for (char c : str) {
            if (c == '"') res += "\\\"";
            else if (c == '\\') res += "\\\\";
            else if (c == '\n') res += "\\n";
            else if (c == '\r') res += "\\r";
            else if (c == '\t') res += "\\t";
            else res += c;
        }
        return res;
    }

    static void GenerateElement(std::shared_ptr<Element> el, std::string& out, int& idCounter, const std::string& parentName, bool isShadow) {
        if (!el) return;
        std::string varName = "el_" + std::to_string(idCounter++);
        
        out += "    auto " + varName + " = std::make_shared<Element>();\n";
        out += "    " + varName + "->tag = \"" + EscapeString(el->tag) + "\";\n";
        if (!el->id.empty()) out += "    " + varName + "->id = \"" + EscapeString(el->id) + "\";\n";
        if (!el->innerText.empty()) out += "    " + varName + "->innerText = \"" + EscapeString(el->innerText) + "\";\n";
        if (el->tabIndex != -1) out += "    " + varName + "->tabIndex = " + std::to_string(el->tabIndex) + ";\n";
        
        // Classes
        for (const auto& cls : el->classes) {
            out += "    " + varName + "->classes.insert(\"" + EscapeString(cls) + "\");\n";
        }
        
        // Props
        for (const auto& prop : el->props) {
            // Find score
            int score = 40;
            for (const auto& ps : el->propScores) {
                if (ps.first == prop.first) {
                    score = ps.second;
                    break;
                }
            }
            out += "    " + varName + "->SetProp(\"" + EscapeString(prop.first) + "\", \"" + EscapeString(prop.second) + "\", " + std::to_string(score) + ");\n";
        }

        // Pseudo Props
        for (const auto& pseudo : el->pseudoProps) {
            for (const auto& prop : pseudo.props) {
                int score = 40;
                for (const auto& ps : pseudo.propScores) {
                    if (ps.first == prop.first) {
                        score = ps.second;
                        break;
                    }
                }
                out += "    " + varName + "->SetPseudoProp(\"" + EscapeString(pseudo.name) + "\", \"" + EscapeString(prop.first) + "\", \"" + EscapeString(prop.second) + "\", " + std::to_string(score) + ");\n";
            }
        }

        if (parentName != "nullptr") {
            out += "    " + parentName + "->Adopt(" + varName + ", " + (isShadow ? "true" : "false") + ");\n";
        }

        // Children
        for (auto& child : el->shadowChildren) {
            GenerateElement(child, out, idCounter, varName, true);
        }
        for (auto& child : el->children) {
            GenerateElement(child, out, idCounter, varName, false);
        }
    }

public:
    static std::string Compile(std::shared_ptr<Element> root, const std::string& globalScripts) {
        std::string out = "#pragma once\n// AOT Compiled DOM App\n\n";
        out += "#include \"src/DOM/Element.h\"\n\n";
        out += "#define IS_AOT_COMPILED\n\n";
        out += "constexpr bool BUNDLED_APP_LZNT1 = false;\n";
        out += "constexpr bool BUNDLED_APP_RC4 = false;\n";
        out += "constexpr const char* BUNDLED_APP_RC4_KEY = \"\";\n";
        out += "constexpr unsigned long BUNDLED_APP_ORIGINAL_SIZE = 0;\n";
        out += "constexpr unsigned char BUNDLED_DOM_APP[] = {0x00};\n";
        out += "constexpr unsigned long BUNDLED_APP_SIZE = 0;\n\n";

        // Scripts
        out += "inline const char* AOT_GLOBAL_SCRIPTS = \"" + EscapeString(globalScripts) + "\";\n\n";

        out += "inline std::shared_ptr<Element> BuildAOTUI() {\n";
        if (!root) {
            out += "    return nullptr;\n}\n";
            return out;
        }

        int idCounter = 0;
        std::string elGen;
        GenerateElement(root, elGen, idCounter, "nullptr", false);
        
        out += elGen;
        out += "    return el_0;\n";
        out += "}\n";

        return out;
    }
};
