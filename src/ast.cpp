#include <stdarg.h>

#include "ast.hpp"


// Ast字符串列表定义
const char *ast_strs[] = {
    #define AST_INFO(type_name, type_str, ...) type_str,
    AST_INFOS
    #undef AST_INFO
};


Ast ast_make(AstType type) {
    Ast ast = {};
    ast.is_const_expr = false;
    ast.is_lvalue = false;

    ast.type = type;
    return ast;
}

Ast *ast_alloc(AstType type) {
    Ast *a = cast(Ast *) xp_alloc(ast_allocator(), sizeof(Ast));
    a->is_const_expr = false;
    a->is_lvalue = false;

    a->type = type;
    return a;
}


AstFile ast_file_make() {
    AstFile f = {};
    f.root = make_array<Ast *>(ast_allocator());
    return f;
}

bool is_add_sub_operator(TokenType t) {
    return t == TokenType::Add || t == TokenType::Minus;
}


bool is_equal_compare_operator(TokenType t) {
    return t == TokenType::DoubleEqual || 
           t == TokenType::ExclamationEqual;
}

bool is_compare_operator(TokenType t) {
    return t == TokenType::LessThan ||
           t == TokenType::LessEqual ||
           t == TokenType::GreaterThan ||
           t == TokenType::GreaterEqual ||
           is_equal_compare_operator(t);
}

bool is_operator_for_bool(TokenType t) {
    return is_equal_compare_operator(t) || is_logic_operator(t);
}

bool is_logic_operator(TokenType t) {
    return t == TokenType::DoubleAnd ||
           t == TokenType::DoubleOr  ||
           t == TokenType::Exclamation;
}

bool is_return_bool_operator(TokenType t) {
    return is_compare_operator(t) || is_logic_operator(t);
}


void print_ast(Array<Ast *> a_arr) {
    for(isize i = 0; i < a_arr.count; i++) {
        print_ast(a_arr[i]);
    }
    return;
}

void print_ast(Ast *a) {
    static isize depth = 0;
    if (!a) return;

    // 打印缩进函数
    auto print_prefix = [&]() {
        for(isize i = 0; i < depth; i++) {
            putchar(' ');
            putchar(' ');
        }
    };
    auto print_with_prefix = [&](const char* format, ...) {
        for(isize i = 0; i < depth; i++) {
            putchar(' ');
            putchar(' ');
        }
        va_list args;
        va_start(args, format);
        vprintf(format, args);
        va_end(args);
    };


    print_with_prefix("%s: \n", ast_strs[cast(i32) a->type]);

    print_prefix();
    print_type(a->v_type);
    putchar('\n');

    depth++;
    switch (a->type) {
    case AstType_Undefined:
        break;
    case AstType_VariableDecl:
        print_with_prefix("name: %s\n", a->VariableDecl.var_name.c_str);
        print_ast(a->VariableDecl.expr);
        break;
    case AstType_Constant:
        print_with_prefix("value: ");
        print_i128(a->Constant.value);
        print_with_prefix("\n");
        print_with_prefix("float_value: %f\n", a->Constant.float_value);
        putchar('\n');
        break;
    case AstType_Function:
        print_with_prefix("name: %s\n", a->Function.name.c_str);
        print_with_prefix("params: \n");
        print_ast(a->Function.params);
        print_ast(a->Function.block);
        break;
    case AstType_Block:
        print_ast(a->Block.statements);
        break;
    case AstType_BinaryExpr:
        print_with_prefix("op: %s\n", token_strings[a->BinaryExpr.op]);
        print_with_prefix("left: \n");
        print_ast(a->BinaryExpr.left);
        print_with_prefix("right: \n");
        print_ast(a->BinaryExpr.right);
        break;
    case AstType_UnaryExpr:
        print_with_prefix("op: %s\n", token_strings[a->UnaryExpr.op]);
        print_with_prefix("operand: \n");
        print_ast(a->UnaryExpr.operand);
        break;
    case AstType_VarExpr:
        print_with_prefix("name: %s\n", a->VarExpr.name.c_str);
        break;
    case AstType_Assignment:
        print_with_prefix("left_var_expr: \n");
        print_ast(a->Assignment.left_var_expr);
        print_with_prefix("right_expr: \n");
        print_ast(a->Assignment.right_expr);
        break;
    case AstType_IfStmt:
        print_with_prefix("condition: \n");
        print_ast(a->IfStmt.condition);
        print_with_prefix("then_block: \n");
        print_ast(a->IfStmt.then_block);
        if(a->IfStmt.else_block) {
            print_with_prefix("else_block: \n");
            print_ast(a->IfStmt.else_block);
        }
        break;
    case AstType_ForStmt:
        if(a->ForStmt.init) {
            print_with_prefix("init: \n");
            print_ast(a->ForStmt.init);
        }
        if(a->ForStmt.condition) {
            print_with_prefix("condition: \n");
            print_ast(a->ForStmt.condition);
        }
        if(a->ForStmt.post) {
            print_with_prefix("post: \n");
            print_ast(a->ForStmt.post);
        }
        print_with_prefix("body: \n");
        print_ast(a->ForStmt.body);
        break;
    case AstType_ReturnStmt:
        print_with_prefix("expr: \n");
        print_ast(a->ReturnStmt.expr);
        break;
    case AstType_CastExpr:
        print_with_prefix("target_type: \n");

        print_prefix();
        print_type(a->CastExpr.target_type);
        putchar('\n');
        
        print_with_prefix("expr: \n");
        print_ast(a->CastExpr.expr);
        break;
    case AstType_FunctionCallExpr:
        print_with_prefix("name: %s\n", a->FunctionCallExpr.name.c_str);
        print_with_prefix("args: \n");
        print_ast(a->FunctionCallExpr.args);
        break;

    case AstType_StructFieldExpr:
        print_with_prefix("struct_var_expr: \n");
        print_ast(a->StructFieldExpr.struct_var_expr);
        print_with_prefix("field_name: %s\n", a->StructFieldExpr.field_name.c_str);
        break;

    case AstType_StructDecl:
        print_with_prefix("name: %s\n", a->StructDecl.name.c_str);
        print_with_prefix("fields: \n");
        print_ast(a->StructDecl.fields);
        break;
    case AstType_StructField:
        print_with_prefix("name: %s\n", a->StructField.name.c_str);
        print_with_prefix("field_type: \n");
        print_prefix();
        print_type(a->StructField.field_type);
        putchar('\n');
        break;

    case AstType_StructInitExpr: 
        print_with_prefix("struct_type: %s\n", a->StructInitExpr.struct_type_name.c_str);        
        print_with_prefix("field_inits: \n");
        print_ast(a->StructInitExpr.field_inits);
        break;
    default:
        break;
    }

    depth--;
    return;
}



xpAllocator ast_allocator() {
    return permanent_allocator();
}
