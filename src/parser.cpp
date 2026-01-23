
#include "parser.hpp"



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





xp_internal Token curr_token(Parser *p);
xp_internal Token next_token(Parser *p);
xp_internal void advance_token(Parser *p);
xp_internal Token expect(Parser *p, TokenType type);
xp_internal Ast *parse_function(Parser *p);
xp_internal Ast *parse_block(Parser *p);


Ast *parse_top_level(Parser *p);
xp_internal Ast *parse_var_decl_or_assign_or_fncall(Parser *p);
xp_internal Ast *parse_if(Parser *p);
xp_internal Ast *parse_for(Parser *p);
xp_internal Ast *parse_stmt(Parser *p);


xp_internal Ast *parse_factor(Parser *p);
xp_internal Ast *parse_expr(Parser *p, isize min_prec = 0);


Ast *parse_constant(Parser *p);
void parse_integer(const char *str, TypeKind type_kind, Ast *a);
void parse_float(const char *str, TypeKind type_kind, Ast *a);

Ast *parse_struct_init_expr(Parser *p);
Ast *parse_array_init_expr(Parser *p);


Ast *parse_type(Parser *p);
Ast *parse_pointer_type(Parser *p);
Ast *parse_array_type(Parser *p);
Ast *parse_basic_and_ident_type(Parser *p);




AstFile parse_file(Array<Token> tokens) {
    Parser p = parser_make(tokens);

    for(;;) {
        if(p.curr_token_index == tokens.count) {
            break;
        }
        
        array_push_back<Ast *>(&p.f.root, parse_top_level(&p));
    }

    return p.f;
}


Ast *parse_pointer_type(Parser *p) {
    Ast *a = ast_alloc(AstType_PointerType);

    expect(p, TokenType::Star);

    Ast *pointed_type_ast = parse_type(p);

    a->PointerType.pointed_type_ast = pointed_type_ast;

    return a;
}

Ast *parse_array_type(Parser *p) {
    Ast *a = ast_alloc(AstType_ArrayType);

    expect(p, TokenType::LeftSquareBracket);

    Ast *count_expr = NULL;
    count_expr = parse_expr(p);
    a->ArrayType.count_expr = count_expr;

    expect(p, TokenType::RightSquareBracket);

    Ast *element_type_ast = parse_type(p);
    a->ArrayType.element_type_ast = element_type_ast;

    return a;
}

Ast *parse_basic_and_ident_type(Parser *p) {
    Ast *a = NULL;

    Token curr = curr_token(p);
    
    switch(curr.type) {
        case TokenType::KW_void:
        case TokenType::KW_bool:
        case TokenType::KW_i8:
        case TokenType::KW_i32:
        case TokenType::KW_i64:
        case TokenType::KW_u8:
        case TokenType::KW_u32:
        case TokenType::KW_u64:
        case TokenType::KW_f32:
        case TokenType::KW_f64: {
            a = ast_alloc(AstType_EasyType);
            a->EasyType.kind = string_to_type_kind(curr.token_str);
        } break;
        
        case TokenType::Ident: {
            a = ast_alloc(AstType_IdentType);
            a->IdentType.name = curr.token_str;
        } break;

        default: {
            XP_ASSERT_DEFAULT(0);
        } break;
    }

    advance_token(p);
    return a;
}

Ast *parse_type(Parser *p) {
    if(curr_token(p).type == TokenType::Star) {
        return parse_pointer_type(p);
    } else if(curr_token(p).type == TokenType::LeftSquareBracket) {
        return parse_array_type(p);
    } else {
        return parse_basic_and_ident_type(p);
    }
}


Ast *parse_top_level(Parser *p) {
    Ast *a = ast_alloc(AstType_Undefined);

    Token ident = expect(p, TokenType::Ident);
    a->token = ident;

    expect(p, TokenType::DoubleColon);


    switch(curr_token(p).type)
    {

    // 函数声明
    case TokenType::LeftBracket: {
        a->type = AstType_Function;
        a->Function.name = ident.token_str;

        expect(p, TokenType::LeftBracket);

        a->Function.params = make_array<Ast *>(ast_allocator());


        for(;;) {
            if(curr_token(p).type == TokenType::RightBracket) {
                break;
            }

            Token ident = expect(p, TokenType::Ident);
            expect(p, TokenType::Colon);

            Ast *arg_type = parse_type(p);
            
            Ast *param = ast_alloc(AstType_VariableDecl);
            param->VariableDecl.type_ast = arg_type;

            param->token = ident;

            param->VariableDecl.var_name = ident.token_str;

            //TODO(xoaop): 支持默认参数
            param->VariableDecl.expr = NULL;

            array_push_back(&a->Function.params, param);

            if(curr_token(p).type != TokenType::RightBracket) {
                expect(p, TokenType::Comma);
            }

        }
        

        expect(p, TokenType::RightBracket);
        
        Ast *return_type = NULL;
        if(curr_token(p).type == TokenType::Arrow) {
            // 有返回值
            expect(p, TokenType::Arrow);
            return_type = parse_type(p);
        } else {
            // 无返回值
            return_type = ast_alloc(AstType_EasyType);
            return_type->EasyType.kind = Type_void;
        }
        a->Function.return_type_ast = return_type;

        

        a->Function.block = parse_block(p);
        a->Function.block->Block.is_function_body = true;

    } break;
    

    // 结构体声明
    case TokenType::KW_struct: {
        a->type = AstType_StructDecl;

        a->StructDecl.name = ident.token_str;

        expect(p, TokenType::KW_struct);
        expect(p, TokenType::LeftCurlyBracket);

        a->StructDecl.fields = make_array<Ast *>(ast_allocator());

        Array<StructField> field_types = make_array<StructField>(stage_allocator());
        for(;;) {
            if(curr_token(p).type == TokenType::RightCurlyBracket) {
                break;
            }

            
            Token field_ident = expect(p, TokenType::Ident);
            expect(p, TokenType::Colon);
            Ast *field_type_ast = parse_type(p);
            
            Ast *field_ast = ast_alloc(AstType_StructField);
            field_ast->token = field_ident;
            field_ast->StructField.name = field_ident.token_str;
            field_ast->StructField.type_ast = field_type_ast;



            expect(p, TokenType::Semicolon);
            
            array_push_back(&a->StructDecl.fields, field_ast);
        }

        expect(p, TokenType::RightCurlyBracket);

    } break;

    default:
        XP_ASSERT_DEFAULT(0);
        break;
    }

    return a;
}






xp_internal Ast *parse_block(Parser *p) {
    Ast *a = ast_alloc(AstType_Block);
    auto stmts = make_array<Ast *>(ast_allocator());

    a->token = expect(p, TokenType::LeftCurlyBracket);

    while (curr_token(p).type != TokenType::RightCurlyBracket) {
        array_push_back(&stmts, parse_stmt(p));
    }
    a->Block.statements = stmts;
    a->Block.is_function_body = false; // NOTE: 默认不是函数体, 函数部分需要单独设置

    expect(p, TokenType::RightCurlyBracket);

    return a;
}

// TODO
xp_internal Ast *parse_stmt(Parser *p) {
    Ast *a = NULL;

    Token ident;
    switch (curr_token(p).type)
    {
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
        a->token = expect(p, TokenType::KW_return);

        if(curr_token(p).type != TokenType::Semicolon) {
            a->ReturnStmt.expr = parse_expr(p);
        } else {
            a->ReturnStmt.expr = NULL;
        }

        expect(p, TokenType::Semicolon);
        break;
    case TokenType::KW_break:
        a = ast_alloc(AstType_Break);
        a->token = expect(p, TokenType::KW_break);
        expect(p, TokenType::Semicolon);
        break;
    case TokenType::KW_continue:
        a = ast_alloc(AstType_Continue);
        a->token = expect(p, TokenType::KW_continue);
        expect(p, TokenType::Semicolon);
        break;

    default:
        // VariableDecl Or Assignment
        a = parse_var_decl_or_assign_or_fncall(p);
        expect(p, TokenType::Semicolon);
        break;
    }

    XP_ASSERT_DEFAULT(a->type != AstType_Undefined);
    return a;
}


xp_internal Ast *parse_var_decl_or_assign_or_fncall(Parser *p) {
    Ast *a = ast_alloc(AstType_Undefined);

    Token ident = curr_token(p);
    a->token = ident;

    Token next = next_token(p);
    if(next.type == TokenType::ColonEqual) {
        // VariableDecl
        expect(p, TokenType::Ident);

        a->type = AstType_VariableDecl;
        a->VariableDecl.var_name = ident.token_str;
        expect(p, ColonEqual);

        // TODO 实现 --- 初始化(即不管) 和 默认0初始化
        if(curr_token(p).type == TokenType::TripleMinus) {
            // 无初始化

            expect(p, TokenType::TripleMinus);

            a->VariableDecl.no_zero_init = true;
        } else {
            // 有初始化表达式

            a->VariableDecl.no_zero_init = false;
            a->VariableDecl.expr = parse_expr(p);
        }

        a->VariableDecl.type_ast = NULL;

    } else if(next.type == TokenType::Colon) {
        // VariableDecl without init expr
        expect(p, TokenType::Ident);
        
        a->type = AstType_VariableDecl;
        a->VariableDecl.var_name = ident.token_str;
        expect(p, Colon);
        
        a->VariableDecl.type_ast = parse_type(p);
        
        // TODO 实现 --- 初始化(即不管) 和 默认0初始化
        if(curr_token(p).type == TokenType::Equal) {
            expect(p, TokenType::Equal);

            if(curr_token(p).type == TokenType::TripleMinus) {
                // 无初始化

                expect(p, TokenType::TripleMinus);

                a->VariableDecl.no_zero_init = true;
                a->VariableDecl.expr = NULL;
            } else {
                // 有初始化表达式

                a->VariableDecl.no_zero_init = false;
                a->VariableDecl.expr = parse_expr(p);
            }
            
        } else {
            // 零初始化

            a->VariableDecl.no_zero_init = false;
            a->VariableDecl.expr = NULL;
        }


    } else if (next.type == TokenType::LeftBracket) {
        // Function Call Expr
        a->type = AstType_FunctionCallExpr;
        a->FunctionCallExpr.name = ident.token_str;
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
        // 赋值表达式
        a->type = AstType::AstType_Assignment;
        
        Ast *lvalue_expr = parse_expr(p);
        expect(p, TokenType::Equal);

        Ast *right = parse_expr(p, 0);

        a->Assignment.left_var_expr = lvalue_expr;
        a->Assignment.right_expr = right;
    }

    return a;
}

xp_internal Ast *parse_if(Parser *p) {
    Ast *a = ast_alloc(AstType_IfStmt);

    a->token = expect(p, TokenType::KW_if);
    
    a->IfStmt.condition = parse_expr(p);
    a->IfStmt.then_block = parse_block(p);
    
    a->IfStmt.else_block = NULL;
    if(curr_token(p).type == TokenType::KW_else) {
        expect(p, TokenType::KW_else);
        if(curr_token(p).type == TokenType::KW_if) {
            a->IfStmt.else_block = ast_alloc(AstType_Block);
            a->IfStmt.else_block->Block.statements = make_array<Ast *>(ast_allocator());
            array_push_back(&a->IfStmt.else_block->Block.statements, parse_if(p));
        } else {
            a->IfStmt.else_block = parse_block(p);
        }
    }

    return a;
}

xp_internal Ast *parse_for(Parser *p) {
    Ast *a = ast_alloc(AstType_ForStmt);

    a->token = expect(p, TokenType::KW_for);

    // TODO 换成现代 for 语法, 而不是现在的 C 风格 for 语法
    // init
    if(curr_token(p).type != TokenType::Semicolon) {
        a->ForStmt.init = parse_var_decl_or_assign_or_fncall(p);
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
        a->ForStmt.post = parse_var_decl_or_assign_or_fncall(p);
        XP_ASSERT_DEFAULT(a->ForStmt.post->type == AstType_Assignment);
    } else {
        a->ForStmt.post = NULL;
    }


    a->ForStmt.body = parse_block(p);

    return a;
}

xp_internal Ast *parse_factor(Parser *p) {
    Ast *a = NULL;
    Token curr = curr_token(p);
    Token next = next_token(p);
    b8 success = false;


    if(is_unary_op(curr.type)) {
        advance_token(p);

        a = ast_alloc(AstType_UnaryExpr);
        a->UnaryExpr.op = curr.type;

        
        // a->UnaryExpr.operand = parse_factor(p);
        
        // NOTE: 比 所有二元运算符 优先级都高 就行
        #define UNARY_OP_PRECEDENCE 114514
        a->UnaryExpr.operand = parse_expr(p, UNARY_OP_PRECEDENCE);



    } else {
        switch (curr.type)
        {

        // 字面量
        case TokenType::Integer:
        case TokenType::Float:
            a = parse_constant(p);
            break;

        // 关键字常量
        case TokenType::KW_true:
        case TokenType::KW_false:
        case TokenType::KW_null:
            a = ast_alloc(AstType_Constant);
            a->token = expect(p, curr.type);
            break;
        
        // (expr)
        case TokenType::LeftBracket:
            expect(p, TokenType::LeftBracket);
            a = parse_expr(p);
            expect(p, TokenType::RightBracket);
            
            break;


        case TokenType::Ident:

            if(next.type == TokenType::LeftBracket) {
                // 函数调用

                a = ast_alloc(AstType_FunctionCallExpr);
                a->token = expect(p, TokenType::Ident);
                a->FunctionCallExpr.name = curr.token_str;
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
                break;
            } else if(next.type == TokenType::LeftCurlyBracket) {
                // 结构体初始化表达式

                a = parse_struct_init_expr(p);
            } else {
                // 变量表达式

                a = ast_alloc(AstType_VarExpr);
                a->token = expect(p, TokenType::Ident);
                a->type = AstType_VarExpr;
                a->VarExpr.name = curr.token_str;
            }

            break;
        case TokenType::KW_cast:

            a = ast_alloc(AstType_Undefined);
            a->token = expect(p, TokenType::KW_cast);
            expect(p, TokenType::LeftBracket);

            a->type = AstType_CastExpr;

            a->CastExpr.target_type_ast = parse_type(p);

            expect(p, TokenType::RightBracket);
            a->CastExpr.expr = parse_factor(p);

            break;

        case TokenType::LeftSquareBracket:
            // 数组字面量

            a = parse_array_init_expr(p);
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
    

    // TODO 添加对后缀表达式的支持
    // TODO 检查
    for(;;) {
        curr = curr_token(p);
        if(curr.type == TokenType::LeftSquareBracket) {

            expect(p, TokenType::LeftSquareBracket);
            Ast *index_expr = parse_expr(p);
            expect(p, TokenType::RightSquareBracket);
            
            Ast *new_left = ast_alloc(AstType_IndexExpr);
            new_left->IndexExpr.array_var_expr = left;
            new_left->IndexExpr.index_expr = index_expr;

            left = new_left;

        } else if(curr.type == TokenType::Dot) {

            expect(p, TokenType::Dot);
            Token field_ident = expect(p, TokenType::Ident);

            Ast *new_left = ast_alloc(AstType_StructFieldExpr);
            new_left->StructFieldExpr.struct_var_expr = left;
            new_left->StructFieldExpr.field_name = field_ident.token_str;

            left = new_left;

        } else {
            break;
        }
    }


    for(;;) {
        curr = curr_token(p);
        
        if(is_binary_op(curr.type) && precedence(curr.type) > min_prec) {
            advance_token(p);
            Ast *right = parse_expr(p, precedence(curr.type));
            Ast *new_left = ast_alloc(AstType_BinaryExpr);

            new_left->BinaryExpr.op = curr.type;
            new_left->BinaryExpr.left = left;
            new_left->BinaryExpr.right = right;

            left = new_left;

            left->token = curr;
        } 
        // else if(curr.type == TokenType::Dot) {
        //     // 结构体字段访问

        //     expect(p, TokenType::Dot);
        //     Token field_ident = expect(p, TokenType::Ident);

        //     Ast *new_left = ast_alloc(AstType_StructFieldExpr);
        //     new_left->StructFieldExpr.struct_var_expr = left;
        //     new_left->StructFieldExpr.field_name = field_ident.token_str;
            
        //     left = new_left;
            
        //     left->token = field_ident;
        // } else if(curr.type == TokenType::LeftSquareBracket) {
        //     // 下标访问表达式

        //     expect(p, TokenType::LeftSquareBracket);
        //     Ast *index_expr = parse_expr(p);
        //     expect(p, TokenType::RightSquareBracket);

        //     Ast *new_left = ast_alloc(AstType_IndexExpr);
        //     new_left->IndexExpr.array_var_expr = left;
        //     new_left->IndexExpr.index_expr = index_expr;

        //     left = new_left;

        //     left->token = curr;
        // } 
        else {
            break;
        }

    }

    return left;
}


Ast *parse_constant(Parser *p) {
    Ast *a = ast_alloc(AstType_Constant);
    a->token = curr_token(p);

    Token curr = curr_token(p);
    
    
    if(curr.type == TokenType::Integer) {
        parse_integer(curr.token_str.c_str, curr.type_kind_of_number, a);
        expect(p, TokenType::Integer);
    }

    if(curr.type == TokenType::Float) {
        parse_float(curr.token_str.c_str, curr.type_kind_of_number, a);
        expect(p, TokenType::Float);
    }

    return a;
}


#include <errno.h>
#include <inttypes.h>

void parse_integer(const char *str, TypeKind type_kind, Ast *a) {
    XP_ASSERT_DEFAULT(str != NULL && a != NULL);


    // 解析
    char *end = NULL;
    errno = 0;
    uintmax_t val = strtoumax(str, &end, 0);

    // 检查解析完成
    if(end == str) {
        XP_ASSERT_DEFAULT(0);
    }

    // 检查溢出(字面量)
    if(errno == ERANGE) {
        XP_ASSERT_DEFAULT(0);
    }

    
    // 检查溢出(类型)
    if(type_kind != Type_Undefined) {
        check_literal_overflow(type_kind, cast(i128)val, 0.0);
    }

    a->type = AstType_Constant;
    a->Constant.value = cast(i128)val;

}

void parse_float(const char *str, TypeKind type_kind, Ast *a) {
    XP_ASSERT_DEFAULT(str != NULL && a != NULL);

    // 解析
    char *end;
    errno = 0;
    double val = strtod(str, &end);

    // 检查解析完成
    if(end == str) {
        XP_ASSERT_DEFAULT(0);
    }

    // 检查溢出(字面量)
    if(errno == ERANGE) {
        if(val == HUGE_VAL || val == -HUGE_VAL) {
            XP_ASSERT_DEFAULT(0);
        }
        // underflow (如1e-400) -> 返回0.0, 不算溢出
    }

    // 拒绝 NaN 和 Inf
    if(!isfinite(val)) {
        XP_ASSERT_DEFAULT(0);
    }

    // 检查溢出(类型)
    if(type_kind != Type_Undefined) {
        check_literal_overflow(type_kind, 0, val);
    }

    a->type = AstType_Constant;
    a->Constant.float_value = val;
    
    return;
}


Ast *parse_struct_init_expr(Parser *p) {

    Ast *a = ast_alloc(AstType_StructInitExpr);
    a->StructInitExpr.field_inits = make_array<Ast *>(ast_allocator());

    Token ident = expect(p, TokenType::Ident);
    a->StructInitExpr.struct_type_name = ident.token_str;

    // TODO 目前只支持全部字段按顺序初始化(不能缺失)
    expect(p, TokenType::LeftCurlyBracket);

    for(;;) {
        if(curr_token(p).type == TokenType::RightCurlyBracket) {
            break;
        }

        Ast *field_init_expr = parse_expr(p);

        array_push_back(&a->StructInitExpr.field_inits, field_init_expr);

        if(curr_token(p).type != TokenType::RightCurlyBracket) {
            expect(p, TokenType::Comma);
        }
    }

    expect(p, TokenType::RightCurlyBracket);

    return a;
}

Ast *parse_array_init_expr(Parser *p) {
    Ast *a = ast_alloc(AstType_ArrayInitExpr);
    a->ArrayInitExpr.elements = make_array<Ast *>(ast_allocator());

    expect(p, TokenType::LeftSquareBracket);
    for(;;) {
        if(curr_token(p).type == TokenType::RightSquareBracket) {
            break;
        }

        Ast *element_expr = parse_expr(p);

        array_push_back(&a->ArrayInitExpr.elements, element_expr);

        if(curr_token(p).type != TokenType::RightSquareBracket) {
            expect(p, TokenType::Comma);
        }
    }

    expect(p, TokenType::RightSquareBracket);

    return a;
}



//  Utils
xp_internal Token curr_token(Parser *p) {
    XP_ASSERT_MSG(p->curr_token_index < p->tokens.count, "Reached end of tokens\n");

    return p->tokens[p->curr_token_index];
}

xp_internal Token next_token(Parser *p) {
    XP_ASSERT_MSG(p->curr_token_index + 1 < p->tokens.count, "Reached end of tokens\n");

    return p->tokens[p->curr_token_index + 1];
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

