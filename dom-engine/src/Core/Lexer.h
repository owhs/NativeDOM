#pragma once
#include <string>
#include <vector>
#include <cctype>

enum class TokType {
    // Literals
    Number, String, TemplateString, RegExp,
    // Identifiers & Keywords
    Identifier,
    Var, Let, Const, Function, Return, If, Else, While, For, Do,
    Break, Continue, Switch, Case, Default,
    True, False, Null, Undefined,
    New, This, Typeof, Instanceof, In, Of,
    Try, Catch, Finally, Throw,
    Class, Extends, Super,
    Delete, Void,
    Async, Await,
    // Operators
    Plus, Minus, Star, Slash, Percent,
    PlusPlus, MinusMinus,
    Assign, PlusAssign, MinusAssign, StarAssign, SlashAssign, PercentAssign,
    Equal, NotEqual, StrictEqual, StrictNotEqual,
    Less, Greater, LessEqual, GreaterEqual,
    And, Or, Not,
    BitAnd, BitOr, BitXor, BitNot, ShiftLeft, ShiftRight,
    Arrow, // =>
    Question, Colon, // ? :
    NullCoalesce, OptionalChain, // ?? ?.
    Spread, // ...
    // Delimiters
    LParen, RParen, LBrace, RBrace, LBracket, RBracket,
    Comma, Semicolon, Dot,
    // Special
    EndOfFile, Comment
};

struct Token {
    TokType type;
    std::string value;
    int line;
    int col;

    Token() : type(TokType::EndOfFile), line(0), col(0) {}
    Token(TokType t, const std::string& v, int l, int c)
        : type(t), value(v), line(l), col(c) {}
};

class Lexer {
    std::string source;
    size_t pos = 0;
    int line = 1;
    int col = 1;

    char peek(int offset = 0) {
        size_t i = pos + offset;
        return i < source.size() ? source[i] : '\0';
    }
    char advance() {
        char c = source[pos++];
        if (c == '\n') { line++; col = 1; }
        else col++;
        return c;
    }
    bool match(char c) {
        if (peek() == c) { advance(); return true; }
        return false;
    }
    void skipWhitespace() {
        while (pos < source.size() && (source[pos] == ' ' || source[pos] == '\t' || source[pos] == '\r' || source[pos] == '\n'))
            advance();
    }
    void skipLineComment() {
        while (pos < source.size() && source[pos] != '\n') advance();
    }
    void skipBlockComment() {
        while (pos < source.size()) {
            if (source[pos] == '*' && peek(1) == '/') { advance(); advance(); return; }
            advance();
        }
    }

    Token makeToken(TokType t, const std::string& v) {
        return Token(t, v, line, col);
    }

    Token readString(char quote) {
        int startLine = line, startCol = col;
        std::string s;
        while (pos < source.size() && source[pos] != quote) {
            if (source[pos] == '\\') {
                advance();
                char esc = advance();
                switch (esc) {
                    case 'n': s += '\n'; break;
                    case 't': s += '\t'; break;
                    case 'r': s += '\r'; break;
                    case '\\': s += '\\'; break;
                    case '\'': s += '\''; break;
                    case '"': s += '"'; break;
                    case '`': s += '`'; break;
                    case '0': s += '\0'; break;
                    default: s += esc;
                }
            } else {
                s += advance();
            }
        }
        if (pos < source.size()) advance(); // closing quote
        return Token(TokType::String, s, startLine, startCol);
    }

    Token readTemplateString() {
        int startLine = line, startCol = col;
        std::string s;
        // Template strings with ${} interpolation stored as raw for later processing
        while (pos < source.size() && source[pos] != '`') {
            if (source[pos] == '\\') {
                advance();
                char esc = advance();
                switch (esc) {
                    case 'n': s += '\n'; break;
                    case 't': s += '\t'; break;
                    case '`': s += '`'; break;
                    case '\\': s += '\\'; break;
                    case '$': s += '$'; break;
                    default: s += esc;
                }
            } else {
                s += advance();
            }
        }
        if (pos < source.size()) advance(); // closing backtick
        return Token(TokType::TemplateString, s, startLine, startCol);
    }

    Token readNumber() {
        int startLine = line, startCol = col;
        std::string n;
        // Hex
        if (peek() == '0' && (peek(1) == 'x' || peek(1) == 'X')) {
            n += advance(); n += advance();
            while (pos < source.size() && std::isxdigit(source[pos])) n += advance();
            return Token(TokType::Number, n, startLine, startCol);
        }
        while (pos < source.size() && (std::isdigit(source[pos]) || source[pos] == '.')) {
            n += advance();
        }
        // Scientific notation
        if (pos < source.size() && (source[pos] == 'e' || source[pos] == 'E')) {
            n += advance();
            if (pos < source.size() && (source[pos] == '+' || source[pos] == '-')) n += advance();
            while (pos < source.size() && std::isdigit(source[pos])) n += advance();
        }
        return Token(TokType::Number, n, startLine, startCol);
    }

    Token readIdentifier() {
        int startLine = line, startCol = col;
        std::string id;
        while (pos < source.size() && (std::isalnum(source[pos]) || source[pos] == '_' || source[pos] == '$'))
            id += advance();

        // Keywords
        if (id == "var") return Token(TokType::Var, id, startLine, startCol);
        if (id == "let") return Token(TokType::Let, id, startLine, startCol);
        if (id == "const") return Token(TokType::Const, id, startLine, startCol);
        if (id == "function") return Token(TokType::Function, id, startLine, startCol);
        if (id == "return") return Token(TokType::Return, id, startLine, startCol);
        if (id == "if") return Token(TokType::If, id, startLine, startCol);
        if (id == "else") return Token(TokType::Else, id, startLine, startCol);
        if (id == "while") return Token(TokType::While, id, startLine, startCol);
        if (id == "for") return Token(TokType::For, id, startLine, startCol);
        if (id == "do") return Token(TokType::Do, id, startLine, startCol);
        if (id == "break") return Token(TokType::Break, id, startLine, startCol);
        if (id == "continue") return Token(TokType::Continue, id, startLine, startCol);
        if (id == "switch") return Token(TokType::Switch, id, startLine, startCol);
        if (id == "case") return Token(TokType::Case, id, startLine, startCol);
        if (id == "default") return Token(TokType::Default, id, startLine, startCol);
        if (id == "true") return Token(TokType::True, id, startLine, startCol);
        if (id == "false") return Token(TokType::False, id, startLine, startCol);
        if (id == "null") return Token(TokType::Null, id, startLine, startCol);
        if (id == "undefined") return Token(TokType::Undefined, id, startLine, startCol);
        if (id == "new") return Token(TokType::New, id, startLine, startCol);
        if (id == "this") return Token(TokType::This, id, startLine, startCol);
        if (id == "typeof") return Token(TokType::Typeof, id, startLine, startCol);
        if (id == "instanceof") return Token(TokType::Instanceof, id, startLine, startCol);
        if (id == "in") return Token(TokType::In, id, startLine, startCol);
        if (id == "of") return Token(TokType::Of, id, startLine, startCol);
        if (id == "try") return Token(TokType::Try, id, startLine, startCol);
        if (id == "catch") return Token(TokType::Catch, id, startLine, startCol);
        if (id == "finally") return Token(TokType::Finally, id, startLine, startCol);
        if (id == "throw") return Token(TokType::Throw, id, startLine, startCol);
        if (id == "class") return Token(TokType::Class, id, startLine, startCol);
        if (id == "extends") return Token(TokType::Extends, id, startLine, startCol);
        if (id == "super") return Token(TokType::Super, id, startLine, startCol);
        if (id == "delete") return Token(TokType::Delete, id, startLine, startCol);
        if (id == "void") return Token(TokType::Void, id, startLine, startCol);
        if (id == "async") return Token(TokType::Async, id, startLine, startCol);
        if (id == "await") return Token(TokType::Await, id, startLine, startCol);

        return Token(TokType::Identifier, id, startLine, startCol);
    }

public:
    Lexer() {}
    Lexer(const std::string& src) : source(src) {}

    std::vector<Token> tokenize() {
        std::vector<Token> tokens;
        while (pos < source.size()) {
            skipWhitespace();
            if (pos >= source.size()) break;

            int startLine = line, startCol = col;
            char c = source[pos];

            // Comments
            if (c == '/' && peek(1) == '/') { advance(); advance(); skipLineComment(); continue; }
            if (c == '/' && peek(1) == '*') { advance(); advance(); skipBlockComment(); continue; }

            // Strings
            if (c == '"' || c == '\'') { advance(); tokens.push_back(readString(c)); continue; }
            if (c == '`') { advance(); tokens.push_back(readTemplateString()); continue; }

            // Numbers
            if (std::isdigit(c) || (c == '.' && std::isdigit(peek(1)))) {
                tokens.push_back(readNumber()); continue;
            }

            // Identifiers / Keywords
            if (std::isalpha(c) || c == '_' || c == '$') {
                tokens.push_back(readIdentifier()); continue;
            }

            // Multi-char operators
            advance();
            switch (c) {
                case '(': tokens.push_back(makeToken(TokType::LParen, "(")); break;
                case ')': tokens.push_back(makeToken(TokType::RParen, ")")); break;
                case '{': tokens.push_back(makeToken(TokType::LBrace, "{")); break;
                case '}': tokens.push_back(makeToken(TokType::RBrace, "}")); break;
                case '[': tokens.push_back(makeToken(TokType::LBracket, "[")); break;
                case ']': tokens.push_back(makeToken(TokType::RBracket, "]")); break;
                case ',': tokens.push_back(makeToken(TokType::Comma, ",")); break;
                case ';': tokens.push_back(makeToken(TokType::Semicolon, ";")); break;
                case '~': tokens.push_back(makeToken(TokType::BitNot, "~")); break;
                case '?':
                    if (match('?')) tokens.push_back(makeToken(TokType::NullCoalesce, "??"));
                    else if (match('.')) tokens.push_back(makeToken(TokType::OptionalChain, "?."));
                    else tokens.push_back(makeToken(TokType::Question, "?"));
                    break;
                case ':': tokens.push_back(makeToken(TokType::Colon, ":")); break;
                case '.':
                    if (match('.')) { match('.'); tokens.push_back(makeToken(TokType::Spread, "...")); }
                    else tokens.push_back(makeToken(TokType::Dot, "."));
                    break;
                case '+':
                    if (match('+')) tokens.push_back(makeToken(TokType::PlusPlus, "++"));
                    else if (match('=')) tokens.push_back(makeToken(TokType::PlusAssign, "+="));
                    else tokens.push_back(makeToken(TokType::Plus, "+"));
                    break;
                case '-':
                    if (match('-')) tokens.push_back(makeToken(TokType::MinusMinus, "--"));
                    else if (match('=')) tokens.push_back(makeToken(TokType::MinusAssign, "-="));
                    else tokens.push_back(makeToken(TokType::Minus, "-"));
                    break;
                case '*':
                    if (match('=')) tokens.push_back(makeToken(TokType::StarAssign, "*="));
                    else tokens.push_back(makeToken(TokType::Star, "*"));
                    break;
                case '/':
                    if (match('=')) tokens.push_back(makeToken(TokType::SlashAssign, "/="));
                    else tokens.push_back(makeToken(TokType::Slash, "/"));
                    break;
                case '%':
                    if (match('=')) tokens.push_back(makeToken(TokType::PercentAssign, "%="));
                    else tokens.push_back(makeToken(TokType::Percent, "%"));
                    break;
                case '=':
                    if (match('=')) {
                        if (match('=')) tokens.push_back(makeToken(TokType::StrictEqual, "==="));
                        else tokens.push_back(makeToken(TokType::Equal, "=="));
                    }
                    else if (match('>')) tokens.push_back(makeToken(TokType::Arrow, "=>"));
                    else tokens.push_back(makeToken(TokType::Assign, "="));
                    break;
                case '!':
                    if (match('=')) {
                        if (match('=')) tokens.push_back(makeToken(TokType::StrictNotEqual, "!=="));
                        else tokens.push_back(makeToken(TokType::NotEqual, "!="));
                    }
                    else tokens.push_back(makeToken(TokType::Not, "!"));
                    break;
                case '<':
                    if (match('<')) tokens.push_back(makeToken(TokType::ShiftLeft, "<<"));
                    else if (match('=')) tokens.push_back(makeToken(TokType::LessEqual, "<="));
                    else tokens.push_back(makeToken(TokType::Less, "<"));
                    break;
                case '>':
                    if (match('>')) tokens.push_back(makeToken(TokType::ShiftRight, ">>"));
                    else if (match('=')) tokens.push_back(makeToken(TokType::GreaterEqual, ">="));
                    else tokens.push_back(makeToken(TokType::Greater, ">"));
                    break;
                case '&':
                    if (match('&')) tokens.push_back(makeToken(TokType::And, "&&"));
                    else tokens.push_back(makeToken(TokType::BitAnd, "&"));
                    break;
                case '|':
                    if (match('|')) tokens.push_back(makeToken(TokType::Or, "||"));
                    else tokens.push_back(makeToken(TokType::BitOr, "|"));
                    break;
                case '^': tokens.push_back(makeToken(TokType::BitXor, "^")); break;
                default: break; // Skip unknown chars
            }
        }
        tokens.push_back(Token(TokType::EndOfFile, "", line, col));
        return tokens;
    }
};
