#pragma once
#include "Lexer.h"
#include "AST.h"

class ScriptParser {
    std::vector<Token> tokens;
    size_t pos = 0;

    Token& peek(int off = 0) { 
        size_t i = pos + off;
        return i < tokens.size() ? tokens[i] : tokens.back(); 
    }
    Token& advance() { return tokens[pos++]; }
    bool check(TokType t) { return peek().type == t; }
    bool isEnd() { return check(TokType::EndOfFile); }

    Token& expect(TokType t, const std::string& msg = "") {
        if (!check(t)) {
            printf("Parse error at line %d:%d - Expected %s, got '%s'\n", peek().line, peek().col, msg.c_str(), peek().value.c_str());
            abort(); // Hard exit on syntax error since we removed exceptions
        }
        return advance();
    }
    bool match(TokType t) {
        if (check(t)) { advance(); return true; }
        return false;
    }

    // ---- Statements ----
    
    std::shared_ptr<ASTNode> parseProgram() {
        auto prog = ASTNode::make(NodeType::Program);
        while (!isEnd()) {
            prog->children.push_back(parseStatement());
        }
        return prog;
    }

    std::shared_ptr<ASTNode> parseStatement() {
        switch (peek().type) {
            case TokType::LBrace: return parseBlock();
            case TokType::Var:
            case TokType::Let:
            case TokType::Const: return parseVarDecl();
            case TokType::Function: return parseFunctionDecl();
            case TokType::If: return parseIfStmt();
            case TokType::While: return parseWhileStmt();
            case TokType::For: return parseForStmt();
            case TokType::Do: return parseDoWhileStmt();
            case TokType::Return: return parseReturnStmt();
            case TokType::Break: { advance(); match(TokType::Semicolon); return ASTNode::make(NodeType::BreakStmt); }
            case TokType::Continue: { advance(); match(TokType::Semicolon); return ASTNode::make(NodeType::ContinueStmt); }
            case TokType::Throw: return parseThrowStmt();
            case TokType::Try: return parseTryCatch();
            case TokType::Switch: return parseSwitchStmt();
            case TokType::Semicolon: { advance(); return ASTNode::make(NodeType::Block); } // empty
            default: return parseExpressionStatement();
        }
    }

    std::shared_ptr<ASTNode> parseBlock() {
        expect(TokType::LBrace, "{");
        auto block = ASTNode::make(NodeType::Block);
        while (!check(TokType::RBrace) && !isEnd()) {
            block->children.push_back(parseStatement());
        }
        expect(TokType::RBrace, "}");
        return block;
    }

    std::shared_ptr<ASTNode> parseVarDecl() {
        Token keyword = advance(); // var/let/const
        auto decl = ASTNode::make(NodeType::VarDecl);
        decl->strValue = keyword.value;
        decl->line = keyword.line;

        do {
            auto id = expect(TokType::Identifier, "variable name");
            auto varNode = ASTNode::makeId(id.value, id.line);
            
            if (match(TokType::Assign)) {
                varNode->init = parseAssignmentExpr();
            }
            decl->children.push_back(varNode);
        } while (match(TokType::Comma));
        
        match(TokType::Semicolon);
        return decl;
    }

    std::shared_ptr<ASTNode> parseFunctionDecl() {
        bool isAsync = false;
        if (peek().type == TokType::Async && peek(1).type == TokType::Function) {
            advance(); // async
            isAsync = true;
        }
        expect(TokType::Function, "function");
        auto func = ASTNode::make(NodeType::FuncDecl);
        func->isAsync = isAsync;
        func->line = peek().line;

        if (check(TokType::Identifier)) {
            func->strValue = advance().value;
        }

        expect(TokType::LParen, "(");
        while (!check(TokType::RParen) && !isEnd()) {
            if (check(TokType::Spread)) {
                advance();
                func->params.push_back("..." + expect(TokType::Identifier, "rest param").value);
            } else {
                func->params.push_back(expect(TokType::Identifier, "parameter").value);
                // Default values (simplified - skip for now)
                if (match(TokType::Assign)) parseAssignmentExpr();
            }
            if (!check(TokType::RParen)) expect(TokType::Comma, ",");
        }
        expect(TokType::RParen, ")");

        func->body = parseBlock();
        return func;
    }

    std::shared_ptr<ASTNode> parseIfStmt() {
        expect(TokType::If, "if");
        auto node = ASTNode::make(NodeType::IfStmt);
        node->line = peek().line;
        expect(TokType::LParen, "(");
        node->test = parseExpression();
        expect(TokType::RParen, ")");
        node->body = parseStatement();
        if (match(TokType::Else)) {
            node->alt = parseStatement();
        }
        return node;
    }

    std::shared_ptr<ASTNode> parseWhileStmt() {
        expect(TokType::While, "while");
        auto node = ASTNode::make(NodeType::WhileStmt);
        expect(TokType::LParen, "(");
        node->test = parseExpression();
        expect(TokType::RParen, ")");
        node->body = parseStatement();
        return node;
    }

    std::shared_ptr<ASTNode> parseForStmt() {
        expect(TokType::For, "for");
        auto node = ASTNode::make(NodeType::ForStmt);
        expect(TokType::LParen, "(");

        // Init
        if (check(TokType::Var) || check(TokType::Let) || check(TokType::Const)) {
            node->init = parseVarDecl();
            // Check for for-in / for-of  
            // (handled in VarDecl with semicolon already consumed)
        } else if (!check(TokType::Semicolon)) {
            node->init = parseExpression();
            match(TokType::Semicolon);
        } else {
            match(TokType::Semicolon);
        }

        // Test
        if (!check(TokType::Semicolon)) {
            node->test = parseExpression();
        }
        expect(TokType::Semicolon, ";");

        // Update
        if (!check(TokType::RParen)) {
            node->update = parseExpression();
        }
        expect(TokType::RParen, ")");
        node->body = parseStatement();
        return node;
    }

    std::shared_ptr<ASTNode> parseDoWhileStmt() {
        expect(TokType::Do, "do");
        auto node = ASTNode::make(NodeType::DoWhileStmt);
        node->body = parseStatement();
        expect(TokType::While, "while");
        expect(TokType::LParen, "(");
        node->test = parseExpression();
        expect(TokType::RParen, ")");
        match(TokType::Semicolon);
        return node;
    }

    std::shared_ptr<ASTNode> parseReturnStmt() {
        auto tok = advance(); // return
        auto node = ASTNode::make(NodeType::ReturnStmt);
        node->line = tok.line;
        // Check if there's a value on the same line or it's a simple semicolon/brace
        if (!check(TokType::Semicolon) && !check(TokType::RBrace) && !isEnd()) {
            node->left = parseExpression();
        }
        match(TokType::Semicolon);
        return node;
    }

    std::shared_ptr<ASTNode> parseThrowStmt() {
        advance(); // throw
        auto node = ASTNode::make(NodeType::ThrowStmt);
        node->left = parseExpression();
        match(TokType::Semicolon);
        return node;
    }

    std::shared_ptr<ASTNode> parseTryCatch() {
        expect(TokType::Try, "try");
        auto node = ASTNode::make(NodeType::TryCatch);
        node->body = parseBlock(); // try block

        if (match(TokType::Catch)) {
            if (match(TokType::LParen)) {
                node->strValue = expect(TokType::Identifier, "catch param").value;
                expect(TokType::RParen, ")");
            }
            node->alt = parseBlock(); // catch block
        }
        if (match(TokType::Finally)) {
            node->update = parseBlock(); // finally block
        }
        return node;
    }

    std::shared_ptr<ASTNode> parseSwitchStmt() {
        expect(TokType::Switch, "switch");
        auto node = ASTNode::make(NodeType::SwitchStmt);
        expect(TokType::LParen, "(");
        node->test = parseExpression();
        expect(TokType::RParen, ")");
        expect(TokType::LBrace, "{");

        while (!check(TokType::RBrace) && !isEnd()) {
            auto caseNode = ASTNode::make(NodeType::SwitchCase);
            if (match(TokType::Case)) {
                caseNode->test = parseExpression();
            } else {
                expect(TokType::Default, "default");
                // default case has no test
            }
            expect(TokType::Colon, ":");
            while (!check(TokType::Case) && !check(TokType::Default) && !check(TokType::RBrace) && !isEnd()) {
                caseNode->children.push_back(parseStatement());
            }
            node->children.push_back(caseNode);
        }
        expect(TokType::RBrace, "}");
        return node;
    }

    std::shared_ptr<ASTNode> parseExpressionStatement() {
        auto expr = parseExpression();
        auto node = ASTNode::make(NodeType::ExprStmt);
        node->left = expr;
        node->line = expr ? expr->line : 0;
        match(TokType::Semicolon);
        return node;
    }

    // ---- Expressions (Precedence Climbing) ----

    std::shared_ptr<ASTNode> parseExpression() {
        auto expr = parseAssignmentExpr();
        // Comma operator (sequence)
        if (check(TokType::Comma) && !isParsingArgs) {
            auto seq = ASTNode::make(NodeType::SequenceExpr);
            seq->children.push_back(expr);
            while (match(TokType::Comma)) {
                seq->children.push_back(parseAssignmentExpr());
            }
            return seq;
        }
        return expr;
    }

    bool isParsingArgs = false;

    std::shared_ptr<ASTNode> parseAssignmentExpr() {
        auto left = parseTernaryExpr();

        TokType t = peek().type;
        if (t == TokType::Assign || t == TokType::PlusAssign || t == TokType::MinusAssign ||
            t == TokType::StarAssign || t == TokType::SlashAssign || t == TokType::PercentAssign) {
            auto op = advance();
            auto node = ASTNode::make(NodeType::AssignExpr);
            node->strValue = op.value;
            node->left = left;
            node->right = parseAssignmentExpr();
            node->line = op.line;
            return node;
        }
        return left;
    }

    std::shared_ptr<ASTNode> parseTernaryExpr() {
        auto left = parseNullCoalesce();
        if (match(TokType::Question)) {
            auto node = ASTNode::make(NodeType::ConditionalExpr);
            node->test = left;
            node->body = parseAssignmentExpr(); // consequent
            expect(TokType::Colon, ":");
            node->alt = parseAssignmentExpr(); // alternate
            return node;
        }
        return left;
    }

    std::shared_ptr<ASTNode> parseNullCoalesce() {
        auto left = parseLogicalOr();
        while (match(TokType::NullCoalesce)) {
            auto node = ASTNode::make(NodeType::BinaryExpr);
            node->strValue = "??";
            node->left = left;
            node->right = parseLogicalOr();
            left = node;
        }
        return left;
    }

    std::shared_ptr<ASTNode> parseLogicalOr() {
        auto left = parseLogicalAnd();
        while (check(TokType::Or)) {
            auto op = advance();
            auto node = ASTNode::make(NodeType::LogicalExpr);
            node->strValue = op.value;
            node->left = left;
            node->right = parseLogicalAnd();
            left = node;
        }
        return left;
    }

    std::shared_ptr<ASTNode> parseLogicalAnd() {
        auto left = parseBitwiseOr();
        while (check(TokType::And)) {
            auto op = advance();
            auto node = ASTNode::make(NodeType::LogicalExpr);
            node->strValue = op.value;
            node->left = left;
            node->right = parseBitwiseOr();
            left = node;
        }
        return left;
    }

    std::shared_ptr<ASTNode> parseBitwiseOr() {
        auto left = parseBitwiseXor();
        while (check(TokType::BitOr)) {
            auto op = advance();
            auto node = ASTNode::make(NodeType::BinaryExpr);
            node->strValue = "|";
            node->left = left;
            node->right = parseBitwiseXor();
            left = node;
        }
        return left;
    }

    std::shared_ptr<ASTNode> parseBitwiseXor() {
        auto left = parseBitwiseAnd();
        while (check(TokType::BitXor)) {
            advance();
            auto node = ASTNode::make(NodeType::BinaryExpr);
            node->strValue = "^";
            node->left = left;
            node->right = parseBitwiseAnd();
            left = node;
        }
        return left;
    }

    std::shared_ptr<ASTNode> parseBitwiseAnd() {
        auto left = parseEquality();
        while (check(TokType::BitAnd)) {
            advance();
            auto node = ASTNode::make(NodeType::BinaryExpr);
            node->strValue = "&";
            node->left = left;
            node->right = parseEquality();
            left = node;
        }
        return left;
    }

    std::shared_ptr<ASTNode> parseEquality() {
        auto left = parseComparison();
        while (check(TokType::Equal) || check(TokType::NotEqual) ||
               check(TokType::StrictEqual) || check(TokType::StrictNotEqual)) {
            auto op = advance();
            auto node = ASTNode::make(NodeType::BinaryExpr);
            node->strValue = op.value;
            node->left = left;
            node->right = parseComparison();
            left = node;
        }
        return left;
    }

    std::shared_ptr<ASTNode> parseComparison() {
        auto left = parseShift();
        while (check(TokType::Less) || check(TokType::Greater) ||
               check(TokType::LessEqual) || check(TokType::GreaterEqual) ||
               check(TokType::Instanceof) || check(TokType::In)) {
            auto op = advance();
            std::shared_ptr<ASTNode> node;
            if (op.type == TokType::Instanceof) {
                node = ASTNode::make(NodeType::InstanceofExpr);
            } else if (op.type == TokType::In) {
                node = ASTNode::make(NodeType::InExpr);
            } else {
                node = ASTNode::make(NodeType::BinaryExpr);
            }
            node->strValue = op.value;
            node->left = left;
            node->right = parseShift();
            left = node;
        }
        return left;
    }

    std::shared_ptr<ASTNode> parseShift() {
        auto left = parseAddSub();
        while (check(TokType::ShiftLeft) || check(TokType::ShiftRight)) {
            auto op = advance();
            auto node = ASTNode::make(NodeType::BinaryExpr);
            node->strValue = op.value;
            node->left = left;
            node->right = parseAddSub();
            left = node;
        }
        return left;
    }

    std::shared_ptr<ASTNode> parseAddSub() {
        auto left = parseMulDiv();
        while (check(TokType::Plus) || check(TokType::Minus)) {
            auto op = advance();
            auto node = ASTNode::make(NodeType::BinaryExpr);
            node->strValue = op.value;
            node->left = left;
            node->right = parseMulDiv();
            left = node;
        }
        return left;
    }

    std::shared_ptr<ASTNode> parseMulDiv() {
        auto left = parseUnary();
        while (check(TokType::Star) || check(TokType::Slash) || check(TokType::Percent)) {
            auto op = advance();
            auto node = ASTNode::make(NodeType::BinaryExpr);
            node->strValue = op.value;
            node->left = left;
            node->right = parseUnary();
            left = node;
        }
        return left;
    }

    std::shared_ptr<ASTNode> parseUnary() {
        if (check(TokType::Not) || check(TokType::Minus) || check(TokType::Plus) || 
            check(TokType::BitNot) || check(TokType::Typeof) || check(TokType::Void) || 
            check(TokType::Delete)) {
            auto op = advance();
            auto node = ASTNode::make(
                op.type == TokType::Typeof ? NodeType::TypeofExpr :
                op.type == TokType::Delete ? NodeType::DeleteExpr :
                op.type == TokType::Void ? NodeType::VoidExpr :
                NodeType::UnaryExpr
            );
            node->strValue = op.value;
            node->left = parseUnary();
            node->isPrefix = true;
            return node;
        }
        // Prefix ++/--
        if (check(TokType::PlusPlus) || check(TokType::MinusMinus)) {
            auto op = advance();
            auto node = ASTNode::make(NodeType::UpdateExpr);
            node->strValue = op.value;
            node->left = parseUnary();
            node->isPrefix = true;
            return node;
        }
        if (check(TokType::Await)) {
            advance();
            auto node = ASTNode::make(NodeType::AwaitExpr);
            node->left = parseUnary();
            return node;
        }
        return parsePostfix();
    }

    std::shared_ptr<ASTNode> parsePostfix() {
        auto left = parseCallMember();
        if (check(TokType::PlusPlus) || check(TokType::MinusMinus)) {
            auto op = advance();
            auto node = ASTNode::make(NodeType::UpdateExpr);
            node->strValue = op.value;
            node->left = left;
            node->isPrefix = false;
            return node;
        }
        return left;
    }

    std::shared_ptr<ASTNode> parseCallMember() {
        auto left = parsePrimary();
        while (true) {
            if (check(TokType::LParen)) {
                advance(); // (
                auto call = ASTNode::make(NodeType::CallExpr);
                call->callee = left;
                call->line = peek().line;

                bool prevParsingArgs = isParsingArgs;
                isParsingArgs = true;
                while (!check(TokType::RParen) && !isEnd()) {
                    if (check(TokType::Spread)) {
                        advance();
                        auto spread = ASTNode::make(NodeType::SpreadExpr);
                        spread->left = parseAssignmentExpr();
                        call->children.push_back(spread);
                    } else {
                        call->children.push_back(parseAssignmentExpr());
                    }
                    if (!check(TokType::RParen)) expect(TokType::Comma, ",");
                }
                isParsingArgs = prevParsingArgs;

                expect(TokType::RParen, ")");
                left = call;
            }
            else if (check(TokType::Dot) || check(TokType::OptionalChain)) {
                bool optional = peek().type == TokType::OptionalChain;
                advance();
                auto member = ASTNode::make(NodeType::MemberExpr);
                member->object = left;
                member->property = ASTNode::makeId(expect(TokType::Identifier, "property name").value);
                member->computed = false;
                if (optional) member->strValue = "?.";
                left = member;
            }
            else if (check(TokType::LBracket)) {
                advance();
                auto member = ASTNode::make(NodeType::ComputedMemberExpr);
                member->object = left;
                member->property = parseExpression();
                member->computed = true;
                expect(TokType::RBracket, "]");
                left = member;
            }
            else break;
        }
        return left;
    }

    std::shared_ptr<ASTNode> parsePrimary() {
        Token& tok = peek();

        switch (tok.type) {
            case TokType::Number: {
                advance();
                double val = 0;
                if (tok.value.size() > 2 && tok.value[0] == '0' && (tok.value[1] == 'x' || tok.value[1] == 'X')) {
                    val = (double)std::stoul(tok.value, nullptr, 16);
                } else {
                    char* end;
                    val = strtod(tok.value.c_str(), &end);
                }
                return ASTNode::makeNum(val, tok.line);
            }
            case TokType::String: {
                advance();
                return ASTNode::makeStr(tok.value, tok.line);
            }
            case TokType::TemplateString: {
                advance();
                auto node = ASTNode::make(NodeType::TemplateLit);
                node->strValue = tok.value;
                node->line = tok.line;
                return node;
            }
            case TokType::True: { advance(); return ASTNode::makeBool(true, tok.line); }
            case TokType::False: { advance(); return ASTNode::makeBool(false, tok.line); }
            case TokType::Null: { advance(); auto n = ASTNode::make(NodeType::NullLit); n->line = tok.line; return n; }
            case TokType::Undefined: { advance(); auto n = ASTNode::make(NodeType::UndefinedLit); n->line = tok.line; return n; }
            case TokType::This: { advance(); auto n = ASTNode::make(NodeType::ThisExpr); n->line = tok.line; return n; }
            
            case TokType::Identifier: {
                // Check for arrow function: ident => ...
                if (peek(1).type == TokType::Arrow) {
                    auto paramName = advance().value;
                    advance(); // =>
                    auto arrow = ASTNode::make(NodeType::ArrowFunc);
                    arrow->params.push_back(paramName);
                    arrow->isArrow = true;
                    if (check(TokType::LBrace)) {
                        arrow->body = parseBlock();
                    } else {
                        // Expression body
                        auto ret = ASTNode::make(NodeType::ReturnStmt);
                        ret->left = parseAssignmentExpr();
                        auto block = ASTNode::make(NodeType::Block);
                        block->children.push_back(ret);
                        arrow->body = block;
                    }
                    return arrow;
                }
                advance();
                return ASTNode::makeId(tok.value, tok.line);
            }

            case TokType::Function: {
                // Function expression
                advance();
                auto func = ASTNode::make(NodeType::FuncExpr);
                func->line = tok.line;
                if (check(TokType::Identifier)) {
                    func->strValue = advance().value;
                }
                expect(TokType::LParen, "(");
                while (!check(TokType::RParen) && !isEnd()) {
                    if (check(TokType::Spread)) {
                        advance();
                        func->params.push_back("..." + expect(TokType::Identifier, "rest param").value);
                    } else {
                        func->params.push_back(expect(TokType::Identifier, "parameter").value);
                        if (match(TokType::Assign)) parseAssignmentExpr();
                    }
                    if (!check(TokType::RParen)) expect(TokType::Comma, ",");
                }
                expect(TokType::RParen, ")");
                func->body = parseBlock();
                return func;
            }

            case TokType::New: {
                advance();
                auto node = ASTNode::make(NodeType::NewExpr);
                node->callee = parseCallMember();
                // If the callee ended up being a CallExpr, merge args from it
                if (node->callee->type == NodeType::CallExpr) {
                    node->children = node->callee->children;
                    node->callee = node->callee->callee;
                }
                return node;
            }

            case TokType::LParen: {
                advance(); // (
                // Check for arrow function: () => ... or (a, b) => ...
                if (check(TokType::RParen) && peek(1).type == TokType::Arrow) {
                    advance(); // )
                    advance(); // =>
                    auto arrow = ASTNode::make(NodeType::ArrowFunc);
                    arrow->isArrow = true;
                    if (check(TokType::LBrace)) {
                        arrow->body = parseBlock();
                    } else {
                        auto ret = ASTNode::make(NodeType::ReturnStmt);
                        ret->left = parseAssignmentExpr();
                        auto block = ASTNode::make(NodeType::Block);
                        block->children.push_back(ret);
                        arrow->body = block;
                    }
                    return arrow;
                }
                // Try to detect (params) => ... vs (expr)
                // Save state and try parsing as potential arrow params
                size_t savedPos = pos;
                bool isArrow = false;
                std::vector<std::string> arrowParams;
                
                if (check(TokType::Identifier)) {
                    // Try to see if this could be arrow params
                    size_t testPos = pos;
                    bool validParams = true;
                    while (testPos < tokens.size()) {
                        if (tokens[testPos].type == TokType::RParen) {
                            if (testPos + 1 < tokens.size() && tokens[testPos + 1].type == TokType::Arrow) {
                                isArrow = true;
                            }
                            break;
                        }
                        if (tokens[testPos].type != TokType::Identifier && tokens[testPos].type != TokType::Comma &&
                            tokens[testPos].type != TokType::Spread) {
                            validParams = false;
                            break;
                        }
                        testPos++;
                    }
                    if (!validParams) isArrow = false;
                }

                if (isArrow) {
                    while (!check(TokType::RParen) && !isEnd()) {
                        if (check(TokType::Spread)) {
                            advance();
                            arrowParams.push_back("..." + expect(TokType::Identifier, "rest param").value);
                        } else {
                            arrowParams.push_back(expect(TokType::Identifier, "param").value);
                        }
                        if (!check(TokType::RParen)) expect(TokType::Comma, ",");
                    }
                    expect(TokType::RParen, ")");
                    expect(TokType::Arrow, "=>");
                    auto arrow = ASTNode::make(NodeType::ArrowFunc);
                    arrow->params = arrowParams;
                    arrow->isArrow = true;
                    if (check(TokType::LBrace)) {
                        arrow->body = parseBlock();
                    } else {
                        auto ret = ASTNode::make(NodeType::ReturnStmt);
                        ret->left = parseAssignmentExpr();
                        auto block = ASTNode::make(NodeType::Block);
                        block->children.push_back(ret);
                        arrow->body = block;
                    }
                    return arrow;
                }

                // Regular grouping expression
                auto expr = parseExpression();
                expect(TokType::RParen, ")");
                return expr;
            }

            case TokType::LBracket: {
                // Array literal
                advance();
                auto arr = ASTNode::make(NodeType::ArrayLit);
                while (!check(TokType::RBracket) && !isEnd()) {
                    if (check(TokType::Spread)) {
                        advance();
                        auto spread = ASTNode::make(NodeType::SpreadExpr);
                        spread->left = parseAssignmentExpr();
                        arr->children.push_back(spread);
                    } else {
                        arr->children.push_back(parseAssignmentExpr());
                    }
                    if (!check(TokType::RBracket)) match(TokType::Comma);
                }
                expect(TokType::RBracket, "]");
                return arr;
            }

            case TokType::LBrace: {
                // Object literal
                advance();
                auto obj = ASTNode::make(NodeType::ObjectLit);
                while (!check(TokType::RBrace) && !isEnd()) {
                    auto prop = ASTNode::make(NodeType::Property);
                    
                    // Computed key
                    if (check(TokType::LBracket)) {
                        advance();
                        prop->left = parseAssignmentExpr(); // key expression
                        expect(TokType::RBracket, "]");
                        prop->computed = true;
                    } else if (check(TokType::String)) {
                        prop->left = ASTNode::makeStr(advance().value);
                    } else {
                        prop->left = ASTNode::makeId(expect(TokType::Identifier, "property name").value);
                    }

                    if (match(TokType::Colon)) {
                        prop->right = parseAssignmentExpr();
                    } else {
                        // Shorthand { name } => { name: name }
                        prop->right = ASTNode::makeId(prop->left->strValue);
                    }
                    obj->children.push_back(prop);
                    if (!check(TokType::RBrace)) match(TokType::Comma);
                }
                expect(TokType::RBrace, "}");
                return obj;
            }

            default: {
                advance(); // Skip unknown token
                return ASTNode::make(NodeType::UndefinedLit);
            }
        }
    }

public:
    ScriptParser() {}
    ScriptParser(const std::vector<Token>& tokens) : tokens(tokens), pos(0) {}

    std::shared_ptr<ASTNode> parse() {
        return parseProgram();
    }

    // Convenience: parse from source string
    static std::shared_ptr<ASTNode> ParseSource(const std::string& source) {
        Lexer lexer(source);
        auto tokens = lexer.tokenize();
        ScriptParser parser(tokens);
        return parser.parse();
    }
};
