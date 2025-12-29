#pragma once 

#include "xoaop.h"
#include "tokenizer.hpp"
#include "array.hpp"

#include "type.hpp"




struct AstFile;
struct Ast;


#define AST_INFOS                                                           \
    AST_INFO(Undefined, "undefined", struct {})                             \
    AST_INFO(Function, "function", struct {                                 \
        xpString name;                                                      \
        Array<Ast *> params;                                                \
        Ast *block;                                                         \
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
    })                                                                      \
    AST_INFO(Assignment, "assignment", struct {                             \
        Ast *left_var_expr;                                                 \
        Ast *right_expr;                                                    \
    })                                                                      \
    AST_INFO(__START__OF__EXPR__, "__start__of__expr__", struct {})         \
    AST_INFO(Constant, "constant", struct {                                 \
        i128 value;                                                         \
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
    AST_INFO(VarExpr, "variable expr", struct {                             \
        xpString name;                                                      \
    })                                                                      \
    AST_INFO(FunctionCallExpr, "function call expr", struct {               \
        xpString name;                                                      \
        Array<Ast *> args;                                                  \
    })                                                                      \
    AST_INFO(CastExpr, "cast expr", struct {                                \
        Type target_type;                                                   \
        Ast *expr;                                                          \
    })                                                                      \
    AST_INFO(__END__OF__EXPR__, "__end__of__expr__", struct {})             \
/**/














    
//Ast文件定义
struct AstFile {
    Array<Ast *> root;
};

// Ast类型枚举
enum AstType {
    #define AST_INFO(type_name, ...) XP_JOIN_2(AstType_, type_name),
    AST_INFOS
    #undef AST_INFO

    // Ast类型数量
    AstType_COUNT
};




//Ast节点数据定义
#define AST_INFO(type_name, type_str, struct_def) typedef struct_def XP_JOIN_2(Ast, type_name);
AST_INFOS
#undef AST_INFO

//Ast节点定义
struct Ast {
    AstType type;

    // TODO(xoaop): more data
    Type v_type; // 该AST节点的类型
    Token token; // 记录该AST对应的第一个token


    bool is_const_expr = false; // 该AST是否是一个常量表达式
    
    union {
        #define AST_INFO(type_name, ...) XP_JOIN_2(Ast, type_name) type_name;
        AST_INFOS
        #undef AST_INFO
    };
    
};

extern const char *ast_strs[];


Ast ast_make(AstType type);
Ast *ast_alloc(AstType type);
AstFile ast_file_make();


bool is_equal_compare_operator(TokenType t);
bool is_compare_operator(TokenType t);
bool is_operator_for_bool(TokenType t);
bool is_logic_operator(TokenType t);
bool is_return_bool_operator(TokenType t);


void print_ast(Ast *a);
void print_ast(Array<Ast *> a_arr);

xpAllocator ast_allocator();





