#pragma once 

#include "xoaop.h"
#include "tokenizer.hpp"
#include "array.hpp"

#include "type.hpp"

#include "symbol.hpp"

#include "value.hpp"


struct Ast;


#define AST_INFOS                                                           \
    AST_INFO(Undefined, "undefined", struct {})                             \
    AST_INFO(BadDecl, "bad decl", struct {})                                \
    AST_INFO(BadStmt, "bad stmt", struct {})                                \
    AST_INFO(BadExpr, "bad expr", struct {})                                \
    AST_INFO(BadType, "bad type", struct {})                                \
    AST_INFO(Import, "import", struct {                                     \
        xpString path;                                                      \
        xpOption<xpString> search_prefix;                                   \
    })                                                                      \
    AST_INFO(EasyType, "easy type", struct {                                \
        TypeKind kind;                                                      \
    })                                                                      \
    AST_INFO(PointerType, "pointer type", struct {                          \
        Ast *pointed_type_ast;                                              \
    })                                                                      \
    AST_INFO(ArrayType, "array type", struct {                              \
        Ast *element_type_ast;                                              \
        Ast *count_expr;                                                    \
    })                                                                      \
    AST_INFO(SliceType, "slice type", struct {                              \
        Ast *element_type_ast;                                              \
    })                                                                      \
    AST_INFO(Ident, "ident", struct {                                       \
        xpString name;                                                      \
    })                                                                      \
    AST_INFO(StructField, "struct field", struct {                          \
        xpString name;                                                      \
        Ast *type_ast;                                                      \
    })                                                                      \
    AST_INFO(Block, "block", struct {                                       \
        Array<Ast *> statements;                                            \
        bool is_function_body;                                              \
    })                                                                      \
    AST_INFO(IfStmt, "if statement", struct {                               \
        Ast *condition;                                                     \
        Ast *then_block;                                                    \
        Ast *else_block;                                                    \
    })                                                                      \
    AST_INFO(ForStmt, "for statement", struct {                             \
        Ast *init;                                                          \
        Ast *condition;                                                     \
        Ast *post;                                                          \
        Ast *body;                                                          \
    })                                                                      \
    AST_INFO(Break, "break statement", struct {})                           \
    AST_INFO(Continue, "continue statement", struct {})                     \
    AST_INFO(ReturnStmt, "return statement", struct {                       \
        Ast *expr;                                                          \
    })                                                                      \
    AST_INFO(VariableDecl, "variable decl", struct {                        \
        xpString var_name;                                                  \
        Ast *expr;                                                          \
        Ast *type_ast;                                                      \
        bool no_zero_init;                                                  \
    })                                                                      \
    AST_INFO(Assignment, "assignment", struct {                             \
        Ast *left_var_expr;                                                 \
        Ast *right_expr;                                                    \
    })                                                                      \
    AST_INFO(FieldAccess, "field access", struct {                          \
        Ast *parent;                                                        \
        xpString field_name;                                                \
    })                                                                      \
    AST_INFO(ParamDecl, "function parameter", struct {                      \
        xpString name;                                                      \
        Ast *type_ast;                                                      \
        bool is_var_arg;                                                    \
    })                                                                      \
    AST_INFO(__START__OF__EXPR__, "__start__of__expr__", struct {})         \
    AST_INFO(Constant, "constant", struct {                                 \
        Value value;                                                        \
    })                                                                      \
    AST_INFO(BinaryExpr, "binary expr", struct {                            \
        TokenType op;                                                       \
        Ast *left;                                                          \
        Ast *right;                                                         \
    })                                                                      \
    AST_INFO(UnaryExpr, "unary expr", struct {                              \
        TokenType op;                                                       \
        Ast *operand;                                                       \
    })                                                                      \
    AST_INFO(FunctionCallExpr, "function call expr", struct {               \
        Ast *func_ident;                                                    \
        Array<Ast *> args;                                                  \
    })                                                                      \
    AST_INFO(CastExpr, "cast expr", struct {                                \
        Ast *expr;                                                          \
        Ast *target_type_ast;                                               \
    })                                                                      \
    AST_INFO(StructInitExpr, "struct init expr", struct {                   \
        Ast *struct_type_ident;                                             \
        Array<Ast *> field_inits;                                           \
    })                                                                      \
    AST_INFO(ArrayInitExpr, "array init expr", struct {                     \
        Array<Ast *> elements;                                              \
    })                                                                      \
    AST_INFO(IndexExpr, "index expr", struct {                              \
        Ast *array_var_expr;                                                \
        Ast *index_expr;                                                    \
    })                                                                      \
    AST_INFO(StringLiteralExpr, "string literal expr", struct {             \
        xpString str;                                                       \
    })                                                                      \
    AST_INFO(FunctionDeclValue, "function decl value", struct {             \
        Array<Ast *> params;                                                \
        Ast *block;                                                         \
        Ast *return_type_ast;                                               \
        bool is_extern_c;                                                   \
    })                                                                      \
    AST_INFO(StructDeclValue, "struct decl value", struct {                 \
        Array<Ast *> fields;                                                \
    })                                                                      \
    AST_INFO(EnumDecl, "enum decl", struct {                                \
        Ast *type_ast;                                                      \
        Array<Ast *> fields;                                                \
    })                                                                      \
    AST_INFO(UnionDecl, "union decl", struct {                              \
        Array<Ast *> fields;                                                \
    })                                                                      \
    AST_INFO(ConstDecl, "const decl", struct {                              \
        xpString name;                                                      \
        Ast *type_ast;                                                      \
        Ast *value_ast;                                                     \
    })                                                                      \
    AST_INFO(__END__OF__EXPR__, "__end__of__expr__", struct {})             \
/**/
















// Ast类型枚举
enum AstType {
    #define AST_INFO(type_name, ...) XP_JOIN_2(AstType_, type_name),
    AST_INFOS
    #undef AST_INFO

    // Ast类型数量
    AstType_COUNT
};




//Ast节点数据定义
#define AST_INFO(type_name, type_str, ...) typedef __VA_ARGS__ XP_JOIN_2(Ast, type_name);
AST_INFOS
#undef AST_INFO

//Ast节点定义
struct Ast {
    AstType type;

    Token token; // 记录该AST对应的第一个token

    ImplicitConversionTag implicit_conversion_tag;

    SymbolInfo *ast_symbol = nullptr; // 该AST节点对应的符号表信息, 主要用于Ident, FieldAccess等需要符号表信息的AST节点

    // 表达式属性
    bool is_const_expr = false; // 该AST是否是一个常量表达式
    bool is_lvalue = false;     // 该AST是否是一个左值表达式
    bool is_null = false;       // 该AST是否是一个null值(仅用于常量表达式)

    Ast();
    Ast(const Ast& other);
    Ast& operator=(const Ast& other);
    

    union {
        #define AST_INFO(type_name, ...) XP_JOIN_2(Ast, type_name) type_name;
        AST_INFOS
        #undef AST_INFO
    };

    SourceLocation src_loc; // 该AST节点对应的源代码位置, 主要用于错误提示
};

extern const char *ast_strs[];


Ast *ast_alloc(AstType type, xpAllocator allocator);
Ast *ast_alloc(AstType type);
Ast *ast_alloc(AstType type, Token token);
Ast *ast_alloc(AstType type, Token token, SourceLocation src_loc);
Ast ast_make(AstType type);


bool is_binary_op(TokenType type);
bool is_unary_op(TokenType type);
bool is_add_sub_operator(TokenType t);
bool is_equal_compare_operator(TokenType t);
bool is_compare_operator(TokenType t);
bool is_operator_for_bool(TokenType t);
bool is_logic_operator(TokenType t);
bool is_return_bool_operator(TokenType t);


void print_ast(Array<Ast*> a_arr, i32 depth = 0, bool is_last = true);

xpAllocator ast_allocator();





