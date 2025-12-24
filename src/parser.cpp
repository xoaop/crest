#include <stdarg.h>

#include "parser.hpp"

// Ast字符串列表定义
const char *ast_strs[] = {
    #define AST_INFO(type_name, type_str, ...) type_str,
    AST_INFOS
    #undef AST_INFO
};


Ast ast_make(AstType type) {
    Ast ast = {};
    ast.type = type;
    return ast;
}

Ast *ast_alloc(AstType type) {
    Ast *a = cast(Ast *) xp_alloc(ast_allocator(), sizeof(Ast));
    a->type = type;
    return a;
}


void parser_init(Parser *parser, Array<Token> tokens) {
    parser->curr_token_index = 0;
    parser->tokens = tokens;
    parser->f = ast_file_make();
    return;
}

Parser parser_make(Array<Token> tokens) {
    Parser p;
    parser_init(&p, tokens);
    return p;
}

AstFile ast_file_make() {
    AstFile f = {};
    f.root = make_array<Ast *>(ast_allocator());
    return f;
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
    case AstType_UnrayExpr:
        print_with_prefix("op: %s\n", token_strings[a->UnrayExpr.op]);
        print_with_prefix("operand: \n");
        print_ast(a->UnrayExpr.operand);
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
    
    default:
        break;
    }

    depth--;
    return;
}


xp_internal Token curr_token(Parser *p);
xp_internal void advance_token(Parser *p);
xp_internal Token expect(Parser *p, TokenType type);
xp_internal Ast *parse_function(Parser *p);
xp_internal Ast *parse_block(Parser *p);


xp_internal Ast *parse_var_decl_or_assign(Parser *p);
xp_internal Ast *parse_if(Parser *p);
xp_internal Ast *parse_for(Parser *p);
xp_internal Ast *parse_stmt(Parser *p);


xp_internal Ast *parse_factor(Parser *p);
xp_internal Ast *parse_expr(Parser *p, isize min_prec = 0);


AstFile parse_file(Array<Token> tokens) {
    Parser p = parser_make(tokens);

    for(;;) {
        if(p.curr_token_index == tokens.count) {
            break;
        }

        Token curr = curr_token(&p);

        switch (curr.type)
        {

        //函数
        case TokenType::Ident:
            array_push_back<Ast *>(&p.f.root, parse_function(&p));
            break;
        
        default:
            XP_ASSERT_MSG(0, "Line %lld Column %lld: Unexpected Token %s, Expected function definition\n", curr.line_index, curr.column_index, curr.token_str.c_str);
        
        }
    }

    return p.f;
}


xp_internal Ast *parse_function(Parser *p) {
    Ast *a = ast_alloc(AstType_Function);

    Token ident = expect(p, TokenType::Ident);
    a->Function.name = ident.token_str;
    expect(p, TokenType::DoubleColon);
    expect(p, TokenType::LeftBracket);

    a->Function.params = make_array<Ast *>(ast_allocator());
    for(;;) {
        if(curr_token(p).type == TokenType::RightBracket) {
            break;
        }

        Token ident = expect(p, TokenType::Ident);
        expect(p, TokenType::Colon);
        //TODO(xoaop): 类型解析, 目前只有i32
        Token type = expect(p, TokenType::KW_i32);
        
        Ast *param = ast_alloc(AstType_VariableDecl);
        param->VariableDecl.var_name = ident.token_str;
        //TODO(xoaop): 支持默认参数
        param->VariableDecl.expr = NULL;

        array_push_back(&a->Function.params, param);

        if(curr_token(p).type != TokenType::RightBracket) {
            expect(p, TokenType::Comma);
        }

    }
    

    expect(p, TokenType::RightBracket);
    
    expect(p, TokenType::Arrow);

    //TODO(xoaop): 类型解析, 目前只有i32
    expect(p, TokenType::KW_i32);

    a->Function.block = parse_block(p);
    a->Function.block->Block.is_function_body = true;

    return a;
}

xp_internal Ast *parse_block(Parser *p) {
    Ast *a = ast_alloc(AstType_Block);
    auto stmts = make_array<Ast *>(ast_allocator());

    expect(p, TokenType::LeftCurlyBracket);

    while (curr_token(p).type != TokenType::RightCurlyBracket) {
        array_push_back(&stmts, parse_stmt(p));
    }
    a->Block.statements = stmts;
    a->Block.is_function_body = false; // NOTE: 默认不是函数体, 函数部分需要单独设置

    expect(p, TokenType::RightCurlyBracket);

    return a;
}

//TODO
xp_internal Ast *parse_stmt(Parser *p) {
    Ast *a = NULL;

    Token ident;
    switch (curr_token(p).type)
    {
    // VariableDecl Or Assignment
    case TokenType::Ident: 
        a = parse_var_decl_or_assign(p);
        expect(p, TokenType::Semicolon);
        break;
    case TokenType::KW_if:
        a = parse_if(p);
        break;
    case TokenType::KW_for:
        a = parse_for(p);
        break;
    case TokenType::KW_switch:
        //TODO(xoaop): switch 语句
        break;
    case TokenType::LeftCurlyBracket:
        a = parse_block(p);
        break;
    case TokenType::KW_return:
        a = ast_alloc(AstType_ReturnStmt);
        expect(p, TokenType::KW_return);
        a->ReturnStmt.expr = parse_expr(p);
        expect(p, TokenType::Semicolon);
        break;
    case TokenType::KW_break:
        a = ast_alloc(AstType_Break);
        expect(p, TokenType::KW_break);
        expect(p, TokenType::Semicolon);
        break;
    case TokenType::KW_continue:
        a = ast_alloc(AstType_Continue);
        expect(p, TokenType::KW_continue);
        expect(p, TokenType::Semicolon);
        break;
    default:
        XP_ASSERT_DEFAULT(0);
        break;
    }

    XP_ASSERT_DEFAULT(a->type != AstType_Undefined);
    return a;
}

xp_internal Ast *parse_var_decl_or_assign(Parser *p) {
    Ast *a = ast_alloc(AstType_Undefined);

    Token ident = expect(p, TokenType::Ident);

    Token curr = curr_token(p);
    if(curr.type == TokenType::ColonEqual) {
        // VariableDecl
        a->type = AstType_VariableDecl;
        a->VariableDecl.var_name = ident.token_str;
        expect(p, ColonEqual);
        a->VariableDecl.expr = parse_expr(p);
    } else if(curr.type == TokenType::Colon) {
        // VariableDecl without init expr
        a->type = AstType_VariableDecl;
        a->VariableDecl.var_name = ident.token_str;
        expect(p, Colon);
        //TODO(xoaop): 类型解析, 目前只有i32
        expect(p, TokenType::KW_i32);
        expect(p, TokenType::Equal);
        a->VariableDecl.expr = parse_expr(p);
    } else if(curr.type == TokenType::Equal) {
        expect(p, TokenType::Equal);

        a->type = AstType::AstType_Assignment;

        Ast *right = parse_expr(p, 0);

        Ast *var_expr = ast_alloc(AstType::AstType_VarExpr);
        var_expr->VarExpr.name = ident.token_str;

        a->Assignment.left_var_expr = var_expr;
        a->Assignment.right_expr = right;
    } else {
        XP_ASSERT_DEFAULT(0);
    }

    return a;
}

xp_internal Ast *parse_if(Parser *p) {
    Ast *a = ast_alloc(AstType_IfStmt);

    expect(p, TokenType::KW_if);
    
    a->IfStmt.condition = parse_expr(p);
    a->IfStmt.then_block = parse_block(p);
    
    a->IfStmt.else_block = NULL;
    if(curr_token(p).type == TokenType::KW_else) {
        expect(p, TokenType::KW_else);
        if(curr_token(p).type == TokenType::KW_if) {
            a->IfStmt.else_block = parse_if(p);
        } else {
            a->IfStmt.else_block = parse_block(p);
        }
    }

    return a;
}

xp_internal Ast *parse_for(Parser *p) {
    Ast *a = ast_alloc(AstType_ForStmt);

    expect(p, TokenType::KW_for);

    // TODO
    // init
    if(curr_token(p).type != TokenType::Semicolon) {
        a->ForStmt.init = parse_var_decl_or_assign(p);

        // 变量声明
        // if() {

        // } else {
        //     // 赋值
        // }

    } else {
        a->ForStmt.init = NULL;
    }
    expect(p, TokenType::Semicolon);

    // condition
    if(curr_token(p).type != TokenType::Semicolon) {
        a->ForStmt.condition = parse_expr(p);
    } else {
        a->ForStmt.condition = NULL;
    }
    expect(p, TokenType::Semicolon);

    // post
    if(curr_token(p).type != TokenType::LeftCurlyBracket) {
        a->ForStmt.post = parse_var_decl_or_assign(p);
    } else {
        a->ForStmt.post = NULL;
    }


    a->ForStmt.body = parse_block(p);

    return a;
}

xp_internal Ast *parse_factor(Parser *p) {
    Ast *a = NULL;
    Token curr = curr_token(p);
    b8 success = false;


    if(is_unray_op(curr.type)) {
        advance_token(p);

        a = ast_alloc(AstType_Constant);
        a->type = AstType_UnrayExpr;
        a->UnrayExpr.op = curr.type;
        a->UnrayExpr.operand = parse_factor(p);
    } else {
        switch (curr.type)
        {

        // 字面量
        case TokenType::Integer:
            expect(p, TokenType::Integer);
            a = ast_alloc(AstType_Constant);

            // TODO: 移到tokenizer阶段, 且不转化为实际数值, 只保留字符串, 转化放到语义分析阶段
            success = xp_str_to_number(curr.token_str.c_str, &a->Constant.value);

            XP_ASSERT_MSG(success, "Line %lld Column %lld: Invalid integer literal %s\n", curr.line_index, curr.column_index, curr.token_str.c_str);
            break;
        
        // (expr)
        case TokenType::LeftBracket:
            expect(p, TokenType::LeftBracket);
            a = parse_expr(p);
            expect(p, TokenType::RightBracket);
            break;

        // VarExpr Or FnCall
        case TokenType::Ident:
            advance_token(p);
            a = ast_alloc(AstType_Undefined);

            if(curr_token(p).type == TokenType::LeftBracket) {
                // Function Call Expr
                a->type = AstType_FunctionCallExpr;
                a->FunctionCallExpr.func_name = curr.token_str;
                expect(p, TokenType::LeftBracket);

                a->FunctionCallExpr.args = make_array<Ast *>(ast_allocator());
                for(;;) {
                    if(curr_token(p).type == TokenType::RightBracket) {
                        break;
                    }

                    Ast *arg = parse_expr(p);
                    array_push_back(&a->FunctionCallExpr.args, arg);

                    if(curr_token(p).type != TokenType::RightBracket) {
                        expect(p, TokenType::Comma);
                    }
                }

                expect(p, TokenType::RightBracket);
            } else {
                // Variable Expr
                a->type = AstType_VarExpr;
                a->VarExpr.name = curr.token_str;
            }

            break;
        default:
            XP_ASSERT_DEFAULT(0);
            break;
        }
    }

    return a;
}

xp_internal Ast *parse_expr(Parser *p, isize min_prec) {
    Ast *left = parse_factor(p);

    Token curr;
    
    for(;;) {
        curr = curr_token(p);
        
        if(is_binary_op(curr.type) && precedence(curr.type) >= min_prec) {
            advance_token(p);
            Ast *right = parse_expr(p, precedence(curr.type));
            Ast *new_left = ast_alloc(AstType_BinaryExpr);

            new_left->BinaryExpr.op = curr.type;
            new_left->BinaryExpr.left = left;
            new_left->BinaryExpr.right = right;

            left = new_left;
        // } else if(curr.type == TokenType::Equal) { // 赋值运算
        //     advance_token(p);
        //     Ast *right = parse_expr(p, 0);

        //     Ast *assignment = ast_alloc(AstType_Assignment);
        //     assignment->Assignment.left_var_expr = left;
        //     assignment->Assignment.right_expr = right;

        //     left = assignment;

        //     break;
        } else {
            break;
        }

    }

    return left;
}


//  Utils
xp_internal Token curr_token(Parser *p) {
    XP_ASSERT_MSG(p->curr_token_index < p->tokens.count, "Reached end of tokens\n");

    return p->tokens[p->curr_token_index];
}

xp_internal void advance_token(Parser *p) {
    XP_ASSERT_DEFAULT(p->curr_token_index < p->tokens.count);
    p->curr_token_index += 1;
    return;
}


xp_internal Token expect(Parser *p, TokenType type) {
    Token curr = curr_token(p);
    if(curr.type == type) {
        advance_token(p);
        return curr;
    }

    // TODO(xoaop): 完善错误处理
    XP_ASSERT_MSG(0, "Line %lld Column %lld: Unexpected Token %s, Expected %s\n", curr.line_index, curr.column_index, curr.token_str.c_str, token_strings[type]);
}


xpAllocator ast_allocator() {
    return permanent_allocator();
}

