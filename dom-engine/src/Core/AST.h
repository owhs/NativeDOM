#pragma once
#include <string>
#include <vector>
#include <memory>

// AST Node Types
enum class NodeType {
    // Program
    Program,
    
    // Statements
    Block,
    VarDecl,
    ExprStmt,
    IfStmt,
    WhileStmt,
    ForStmt,
    ForInStmt,
    ForOfStmt,
    DoWhileStmt,
    ReturnStmt,
    BreakStmt,
    ContinueStmt,
    ThrowStmt,
    TryCatch,
    SwitchStmt,
    SwitchCase,

    // Expressions
    NumberLit,
    StringLit,
    TemplateLit,
    BoolLit,
    NullLit,
    UndefinedLit,
    ArrayLit,
    ObjectLit,
    Identifier,
    ThisExpr,

    // Operations
    BinaryExpr,
    UnaryExpr,
    UpdateExpr,   // ++, --
    LogicalExpr,
    AssignExpr,
    ConditionalExpr, // ternary
    SequenceExpr,    // comma operator

    // Calls and access
    CallExpr,
    MemberExpr,
    ComputedMemberExpr,
    NewExpr,
    
    // Functions
    FuncDecl,
    FuncExpr,
    ArrowFunc,

    // Property (for object literals)
    Property,

    // Typeof / delete / void
    TypeofExpr,
    DeleteExpr,
    VoidExpr,
    InstanceofExpr,
    InExpr,

    // Spread
    SpreadExpr,

    // Await
    AwaitExpr,
};

struct ASTNode {
    NodeType type;
    
    // Value data (for literals, identifiers, operators)
    std::string strValue;
    double numValue = 0;
    bool boolValue = false;

    // Children
    std::vector<std::shared_ptr<ASTNode>> children;

    // Named children for structured nodes
    std::shared_ptr<ASTNode> left;     // Binary op left, assign target, for init
    std::shared_ptr<ASTNode> right;    // Binary op right, assign value, for update
    std::shared_ptr<ASTNode> body;     // Function body, loop body, if consequent
    std::shared_ptr<ASTNode> alt;      // If alternate (else), catch block
    std::shared_ptr<ASTNode> test;     // Loop/if condition, for test
    std::shared_ptr<ASTNode> init;     // For init, var init value
    std::shared_ptr<ASTNode> update;   // For update, finally block
    std::shared_ptr<ASTNode> object;   // Member expr object, new callee
    std::shared_ptr<ASTNode> property; // Member expr property
    std::shared_ptr<ASTNode> callee;   // Call expr callee

    // Function parameters
    std::vector<std::string> params;
    
    // Flags
    bool computed = false;  // Computed member access [expr]
    bool isPrefix = false;  // Update expr prefix
    bool isAsync = false;   // Async function
    bool isArrow = false;

    // Source location
    int line = 0;
    int col = 0;

    // Factory methods
    static std::shared_ptr<ASTNode> make(NodeType t) {
        auto n = std::make_shared<ASTNode>();
        n->type = t;
        return n;
    }
    static std::shared_ptr<ASTNode> makeNum(double v, int l = 0) {
        auto n = make(NodeType::NumberLit);
        n->numValue = v;
        n->line = l;
        return n;
    }
    static std::shared_ptr<ASTNode> makeStr(const std::string& v, int l = 0) {
        auto n = make(NodeType::StringLit);
        n->strValue = v;
        n->line = l;
        return n;
    }
    static std::shared_ptr<ASTNode> makeBool(bool v, int l = 0) {
        auto n = make(NodeType::BoolLit);
        n->boolValue = v;
        n->line = l;
        return n;
    }
    static std::shared_ptr<ASTNode> makeId(const std::string& name, int l = 0) {
        auto n = make(NodeType::Identifier);
        n->strValue = name;
        n->line = l;
        return n;
    }
};
