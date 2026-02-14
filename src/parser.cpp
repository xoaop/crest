
#include "parser.hpp"

#include "path.hpp"

#include "context.hpp"


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


void report_unexpected(Parser *p, const char *expected_string);
void report_unexpected(Parser *p, Token token, const char *expected_string);
void report_unexpected(Parser *p, Token token, xpString expected_string);
void report_unexpected(Parser *p, TokenType expected);
void report_unexpected(Parser *p, Token token, TokenType expected);

bool reach_end(Parser *p);
xp_internal Token curr_token(Parser *p);
xp_internal Token next_token(Parser *p);
xp_internal void advance_token(Parser *p);
xp_internal Token expect(Parser *p, TokenType type);
xpPair<Token, bool> expect2(Parser *p, TokenType type);
Token expect_assert(Parser *p, TokenType type);


void advance_to_next_top_level(Parser *p);




xp_internal Ast *parse_function(Parser *p);
xp_internal Ast *parse_block(Parser *p);


Ast *parse_top_level(Parser *p);
xp_internal Ast *parse_var_decl_or_assign_or_fncall(Parser *p);
xp_internal Ast *parse_if(Parser *p);
xp_internal Ast *parse_for(Parser *p);
xp_internal Ast *parse_stmt(Parser *p);


xp_internal Ast *parse_factor(Parser *p, bool has_struct_init);
xp_internal Ast *parse_expr(Parser *p, isize min_prec = 0, bool has_struct_init = false);


Ast *parse_constant(Parser *p);
void parse_integer(const char *str, TypeKind type_kind, Ast *a, Parser *p);
void parse_float(const char *str, TypeKind type_kind, Ast *a, Parser *p);

Ast *parse_struct_init_expr(Parser *p);
Ast *parse_array_init_expr(Parser *p, bool has_struct_init);


Ast *parse_type(Parser *p);
Ast *parse_pointer_type(Parser *p);
Ast *parse_array_type(Parser *p);
Ast *parse_basic_and_ident_type(Parser *p);

Ast *parse_ident(Parser *p);
Ast *parse_single_ident_or_field_access_with_pure_ident(Parser *p);






AstFile parse_file(Array<Token> tokens, SourceCode src_code) {
    defer(xp_arena_allocator_clear(stage_allocator()));

    Parser p = parser_make(tokens);
    p.f.source_code = src_code;

    for(;;) {
        if(reach_end(&p)) {
            break;
        }
        
        array_push_back<Ast *>(&p.f.top_levels, parse_top_level(&p));
    }

    print_ast(p.f.top_levels);

    return p.f;
}


Ast *parse_pointer_type(Parser *p) {
    Ast *a = ast_alloc(AstType_PointerType, expect(p, TokenType::Star));


    Ast *pointed_type_ast = parse_type(p);

    a->PointerType.pointed_type_ast = pointed_type_ast;

    a->span = merge(a->token.span, pointed_type_ast->span);

    return a;
}

Ast *parse_array_type(Parser *p) {
    Ast *a = ast_alloc(AstType_ArrayType, expect(p, TokenType::LeftSquareBracket));

    Ast *count_expr = NULL;
    count_expr = parse_expr(p, 0, true);
    a->ArrayType.count_expr = count_expr;

    expect(p, TokenType::RightSquareBracket);

    Ast *element_type_ast = parse_type(p);
    a->ArrayType.element_type_ast = element_type_ast;

    a->span = merge(a->token.span, element_type_ast->span);

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
            a = ast_alloc(AstType_EasyType, curr);
            a->EasyType.kind = string_to_type_kind(curr.token_str);

            a->span = curr.span;

            advance_token(p);
        } break;
        
        case TokenType::Ident: {
            // 对于类型, 只能是要么单标识符, 如  a: my_type
            // 要么是 package.type_name 形式, 如 a: foo.my_type
            a = parse_single_ident_or_field_access_with_pure_ident(p);
        } break;

        default: {
            report_unexpected(p, "type");
            a = ast_alloc(AstType_BadType, curr);
            a->span = curr.span;

            advance_token(p); // 跳过错误token
        } break;
    }

    return a;
}

Ast *parse_type(Parser *p) {
    if(curr_token(p).type == TokenType::Star) {
        return parse_pointer_type(p);
    } else if(curr_token(p).type == TokenType::LeftSquareBracket) {

        if(next_token(p).type == TokenType::RightSquareBracket) {
            // Slice Type

            Ast *a = ast_alloc(AstType_SliceType, curr_token(p));

            expect(p, TokenType::LeftSquareBracket);
            expect(p, TokenType::RightSquareBracket);

            Ast *element_type_ast = parse_type(p);
            a->SliceType.element_type_ast = element_type_ast;

            a->span = merge(a->token.span, element_type_ast->span);

            return a;
        } else {
            // Array Type

            return parse_array_type(p);
        }
    } else {
        return parse_basic_and_ident_type(p);
    }
}


Ast *parse_import(Parser *p) {
    Token import_token = expect_assert(p, TokenType::KW_import);

    auto path_succ = expect2(p, TokenType::StringLiteral);
    if(!path_succ.second) {
        advance_to_next_top_level(p);
        return ast_alloc(AstType_BadDecl, import_token);
    }
    
    xpString raw_path = path_succ.first.token_str;
    raw_path.c_str = raw_path.c_str + 1;  // 去掉开头的引号
    raw_path.length -= 2;                 // 去掉结尾的引号


    Ast *a = ast_alloc(AstType_Import, import_token);
    a->Import.path = normalize_path(raw_path, ast_allocator());

    // TODO 支持别名
    a->Import.alias = get_last_component_of_path(a->Import.path, ast_allocator());
    
    a->span = merge(import_token.span, path_succ.first.span);

    return a;
}



Ast *parse_top_level(Parser *p) {

    // TODO: TEMP
    if(curr_token(p).type == TokenType::KW_import) {
        Ast *a = parse_import(p);
        return a;
    } 


    Token ident;
    auto ident_result = expect2(p, TokenType::Ident);
    if(!ident_result.second) {
        advance_to_next_top_level(p);
        return ast_alloc(AstType_BadDecl);
    }
    ident = ident_result.first;

    expect2(p, TokenType::DoubleColon);


    Ast *a = NULL;
    switch(curr_token(p).type)
    {

    // 函数声明
    case TokenType::LeftBracket: {
        a = ast_alloc(AstType_Function, ident);

        a->Function.name = ident.token_str;

        expect2(p, TokenType::LeftBracket);

        
        a->Function.params = make_array<Ast *>(ast_allocator());
        for(;;) {
            if(reach_end(p) || curr_token(p).type == TokenType::RightBracket) {
                break;
            }

            Token ident = expect(p, TokenType::Ident);
            expect2(p, TokenType::Colon);

            Ast *arg_type = parse_type(p);
            
            Ast *param = ast_alloc(AstType_VariableDecl, ident);
            param->VariableDecl.type_ast = arg_type;
            param->VariableDecl.var_name = ident.token_str;
            //TODO(xoaop): 支持默认参数
            param->VariableDecl.expr = NULL;
            param->span = merge(ident.span, arg_type->span);

            array_push_back(&a->Function.params, param);

            if(curr_token(p).type != TokenType::RightBracket) {
                expect2(p, TokenType::Comma);
            }

        }
        
        expect(p, TokenType::RightBracket);

        Ast *return_type = NULL;
        if(curr_token(p).type == TokenType::Arrow) {
            // 有返回值
            advance_token(p); // 跳过箭头
            return_type = parse_type(p);
        } else {
            // 无返回值
            return_type = ast_alloc(AstType_EasyType, curr_token(p)); // 这个token不会被使用到, 只是为了记录span, 也不存在
            return_type->EasyType.kind = Type_void;
            return_type->span = return_type->token.span;
        }
        a->Function.return_type_ast = return_type;

        // TODO: TEST 临时支持 extern_C 函数
        a->Function.is_extern_C = false;
        if(curr_token(p).type == TokenType::KW_extern_C) {
            Token extern_c_token = expect(p, TokenType::KW_extern_C);
            a->Function.block = NULL;
            a->Function.is_extern_C = true;
            a->span = merge(a->token.span, extern_c_token.span);

            expect(p, TokenType::Semicolon);

        } else {
            a->span = merge(a->token.span, return_type->span); // 配合return_type的span, 注意无返回值的情况

            a->Function.block = parse_block(p);
            a->Function.block->Block.is_function_body = true;
        }


    } break;
    

    // 结构体声明
    case TokenType::KW_struct: {
        a = ast_alloc(AstType_StructDecl, ident);

        a->StructDecl.name = ident.token_str;

        expect(p, TokenType::KW_struct);
        expect(p, TokenType::LeftCurlyBracket);

        a->StructDecl.fields = make_array<Ast *>(ast_allocator());

        Array<StructField> field_types = make_array<StructField>(stage_allocator());
        for(;;) {
            if(reach_end(p) || curr_token(p).type == TokenType::RightCurlyBracket) {
                break;
            }

            
            Token field_ident = expect(p, TokenType::Ident);
            expect(p, TokenType::Colon);
            Ast *field_type_ast = parse_type(p);
            
            Ast *field_ast = ast_alloc(AstType_StructField, field_ident);
            field_ast->StructField.name = field_ident.token_str;
            field_ast->StructField.type_ast = field_type_ast;
            field_ast->span = merge(field_ident.span, field_type_ast->span);


            expect(p, TokenType::Semicolon);
            
            array_push_back(&a->StructDecl.fields, field_ast);
        }

        Token rcb = expect(p, TokenType::RightCurlyBracket);
        a->span = merge(a->token.span, rcb.span);

    } break;

    default:
        report_unexpected(p, "struct (for struct decl) or ( (for function decl)");

        a = ast_alloc(AstType_BadDecl, ident);
        a->span = merge(ident.span, curr_token(p).span);

        advance_to_next_top_level(p);

        // XP_ASSERT_DEFAULT(0);
        break;
    }

    return a;
}






xp_internal Ast *parse_block(Parser *p) {
    Ast *a = ast_alloc(AstType_Block, expect(p, TokenType::LeftCurlyBracket));
    auto stmts = make_array<Ast *>(ast_allocator());


    while (curr_token(p).type != TokenType::RightCurlyBracket && !reach_end(p)) {
        array_push_back(&stmts, parse_stmt(p));
    }
    a->Block.statements = stmts;
    a->Block.is_function_body = false; // NOTE: 默认不是函数体, 函数部分需要单独设置

    Token rcb = expect(p, TokenType::RightCurlyBracket);

    a->span = merge(a->token.span, rcb.span);

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
            a->ReturnStmt.expr = parse_expr(p, 0, true);
            a->span = merge(a->token.span, a->ReturnStmt.expr->span);
        } else {
            a->ReturnStmt.expr = NULL;
            a->span = a->token.span;
        }

        expect(p, TokenType::Semicolon);
        break;
    case TokenType::KW_break:
        a = ast_alloc(AstType_Break);
        a->token = expect(p, TokenType::KW_break);
        a->span = a->token.span;
        expect(p, TokenType::Semicolon);
        break;
    case TokenType::KW_continue:
        a = ast_alloc(AstType_Continue);
        a->token = expect(p, TokenType::KW_continue);
        a->span = a->token.span;
        expect(p, TokenType::Semicolon);
        break;

    default:
        // VariableDecl Or Assignment
        a = parse_var_decl_or_assign_or_fncall(p);
        expect(p, TokenType::Semicolon);
        break;
    }

    XP_ASSERT_DEFAULT(a != NULL);
    return a;
}

xp_internal Ast *parse_var_decl_or_assign_or_fncall(Parser *p) {
    Ast *a = NULL;

    Ast *left_expr = parse_expr(p, 0, true);
    if(left_expr->type == AstType_FunctionCallExpr) {
        // 函数调用表达式
        return left_expr;
    }

    if(curr_token(p).type == TokenType::ColonEqual) {
        // VariableDecl without type annotation

        // TODO error_msg
        if(left_expr->type != AstType_Ident) {
            context()->reporter.report(
                ErrorLevel::Error,
                curr_token(p).span,
                p->f.source_code,
                "expected identifier before ':=' in variable declaration, got right value expression that can not be variable name"
            );
        }
        // XP_ASSERT_DEFAULT(left_expr->type == AstType_Ident);

        a = ast_alloc(AstType_VariableDecl, left_expr->token);
        a->VariableDecl.var_name = left_expr->Ident.name; // 有可能乱码, 若left_expr不是Ident

        expect(p, ColonEqual);

        // TODO 实现 --- 初始化(即不管) 和 默认0初始化
        if(curr_token(p).type == TokenType::TripleMinus) {
            // 无初始化

            Token tm = expect(p, TokenType::TripleMinus);

            a->VariableDecl.no_zero_init = true;

            a->span = merge(a->token.span, tm.span);
        } else {
            // 有初始化表达式

            a->VariableDecl.no_zero_init = false;
            a->VariableDecl.expr = parse_expr(p, 0, true);

            a->span = merge(a->token.span, a->VariableDecl.expr->span);
        }

        a->VariableDecl.type_ast = NULL;

    } else if(curr_token(p).type == TokenType::Colon) {
        // VariableDecl with type annotation
        
        a = ast_alloc(AstType_VariableDecl, left_expr->token);

        // TODO error_msg
        if(left_expr->type != AstType_Ident) {
            context()->reporter.report(
                ErrorLevel::Error,
                curr_token(p).span,
                p->f.source_code,
                "expected identifier before ':' in variable declaration, got right value expression that can not be variable name"
            );

            a->VariableDecl.var_name = xp_string_c(""); // 有可能乱码, 若left_expr不是Ident
        } else {
            a->VariableDecl.var_name = left_expr->Ident.name;
        }


        
        expect(p, Colon);
        
        a->VariableDecl.type_ast = parse_type(p);
        
        // TODO 实现 --- 初始化(即不管) 和 默认0初始化
        if(curr_token(p).type == TokenType::Equal) {
            expect(p, TokenType::Equal);

            if(curr_token(p).type == TokenType::TripleMinus) {
                // 无初始化

                Token tm = expect(p, TokenType::TripleMinus);

                a->VariableDecl.no_zero_init = true;
                a->VariableDecl.expr = NULL;

                a->span = merge(a->token.span, tm.span);
            } else {
                // 有初始化表达式

                a->VariableDecl.no_zero_init = false;
                a->VariableDecl.expr = parse_expr(p, 0, true);

                a->span = merge(a->token.span, a->VariableDecl.expr->span);
            }
            
        } else {
            // 零初始化

            a->VariableDecl.no_zero_init = false;
            a->VariableDecl.expr = NULL;

            a->span = merge(a->token.span, a->VariableDecl.type_ast->span);
        }


    } else if (curr_token(p).type == TokenType::LeftBracket) {
        // Function Call Expr
        
        // TODO error_msg
        if(left_expr->type != AstType_Ident && !(left_expr->type == AstType_FieldAccess && left_expr->FieldAccess.parent->type == AstType_Ident)) {
            context()->reporter.report(
                ErrorLevel::Error,
                curr_token(p).span,
                p->f.source_code,
                "expected identifier or field access with pure ident as parent before '(' in function call expression, got right value expression that can not be function name"
            );
        }
        // XP_ASSERT_DEFAULT(left_expr->type == AstType_Ident || (left_expr->type == AstType_FieldAccess && left_expr->FieldAccess.parent->type == AstType_Ident));



        a = ast_alloc(AstType_FunctionCallExpr, left_expr->token);
        a->FunctionCallExpr.func_ident = left_expr;
        expect(p, TokenType::LeftBracket);

        a->FunctionCallExpr.args = make_array<Ast *>(ast_allocator());
        for(;;) {
            if(reach_end(p) || curr_token(p).type == TokenType::RightBracket) {
                break;
            }

            Ast *arg = parse_expr(p, 0, true);
            array_push_back(&a->FunctionCallExpr.args, arg);

            if(curr_token(p).type != TokenType::RightBracket) {
                expect(p, TokenType::Comma);
            }
        }

        Token rb = expect(p, TokenType::RightBracket);
        a->span = merge(a->token.span, rb.span);
    } else {
        // 赋值表达式
        a = ast_alloc(AstType_Assignment, left_expr->token);

        Ast *lvalue_expr = left_expr;
        expect(p, TokenType::Equal);

        Ast *right = parse_expr(p, 0, true);

        a->Assignment.left_var_expr = lvalue_expr;
        a->Assignment.right_expr = right;

        a->span = merge(lvalue_expr->span, right->span);
    }

    return a;
}



xp_internal Ast *parse_if(Parser *p) {
    Ast *a = ast_alloc(AstType_IfStmt);
    a->token = expect(p, TokenType::KW_if);

    
    a->IfStmt.condition = parse_expr(p, 0, false);
    a->IfStmt.then_block = parse_block(p);
    
    a->IfStmt.else_block = NULL;
    if(curr_token(p).type == TokenType::KW_else) {
        expect(p, TokenType::KW_else);
        if(curr_token(p).type == TokenType::KW_if) {
            a->IfStmt.else_block = ast_alloc(AstType_Block, curr_token(p)); // 虚拟的Block节点, 只是为了else if的情况, 让else if的条件和then块都成为这个Block节点的子节点, 这样就能统一else if和else的处理了
            a->IfStmt.else_block->Block.statements = make_array<Ast *>(ast_allocator());
            array_push_back(&a->IfStmt.else_block->Block.statements, parse_if(p));

            a->IfStmt.else_block->span = a->IfStmt.else_block->Block.statements[0]->span; // 只有一个子节点, span就是子节点的span
        } else {
            a->IfStmt.else_block = parse_block(p);
        }
    }
    
    if(a->IfStmt.else_block != NULL) {
        a->span = merge(a->token.span, a->IfStmt.else_block->span);
    } else {
        a->span = merge(a->token.span, a->IfStmt.then_block->span);
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
        a->ForStmt.condition = parse_expr(p, 0, false);
    } else {
        a->ForStmt.condition = NULL;
    }
    expect(p, TokenType::Semicolon);

    // post
    if(curr_token(p).type != TokenType::LeftCurlyBracket) {
        a->ForStmt.post = parse_var_decl_or_assign_or_fncall(p);

        if(a->ForStmt.post->type != AstType_Assignment) {
            context()->reporter.report(
                ErrorLevel::Error,
                curr_token(p).span,
                p->f.source_code,
                "expected assignment expression in for loop post statement, got expression that is not an assignment"
            );
        }
        XP_ASSERT_DEFAULT(a->ForStmt.post->type == AstType_Assignment);
    } else {
        a->ForStmt.post = NULL;
    }


    a->ForStmt.body = parse_block(p);

    a->span = merge(a->token.span, a->ForStmt.body->span);

    return a;
}

xp_internal Ast *parse_factor(Parser *p, bool has_struct_init) {
    Ast *a = NULL;
    defer(XP_ASSERT_DEFAULT(a != NULL));

    Token curr = curr_token(p);


    if(is_unary_op(curr.type)) {
        advance_token(p);

        a = ast_alloc(AstType_UnaryExpr, curr);
        a->UnaryExpr.op = curr.type;

        
        
        // NOTE: 比 所有二元运算符 优先级都高 就行
        #define UNARY_OP_PRECEDENCE 114514
        a->UnaryExpr.operand = parse_expr(p, UNARY_OP_PRECEDENCE, has_struct_init);

        a->span = merge(curr.span, a->UnaryExpr.operand->span);

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
            a->span = a->token.span;
            break;
        
        // (expr)
        case TokenType::LeftBracket:
            expect(p, TokenType::LeftBracket);
            a = parse_expr(p, 0, has_struct_init);
            expect(p, TokenType::RightBracket);
            
            break;


        case TokenType::Ident: {

            a = parse_ident(p);

        } break;

        case TokenType::KW_cast:

            a = ast_alloc(AstType_Undefined);
            a->token = expect(p, TokenType::KW_cast);
            expect(p, TokenType::LeftBracket);

            a->type = AstType_CastExpr;

            a->CastExpr.target_type_ast = parse_type(p);

            expect(p, TokenType::RightBracket);
            a->CastExpr.expr = parse_expr(p, UNARY_OP_PRECEDENCE, has_struct_init);

            a->span = merge(a->token.span, a->CastExpr.expr->span);
            break;

        case TokenType::LeftSquareBracket:
            // 数组字面量

            a = parse_array_init_expr(p, has_struct_init);
            break;

        case TokenType::StringLiteral: {
            
            // 字符串字面量

            a = ast_alloc(AstType_StringLiteralExpr);
            a->token = expect(p, TokenType::StringLiteral);

            // TODO: xpString的稳定性的试金石
            xpString str = a->token.token_str;
            str.c_str = str.c_str + 1; //跳过开头的引号
            str.length -= 2; //去掉前后的引号
            str.capacity -= 2; //去掉前后的引号

            a->StringLiteralExpr.str = str;

            a->span = a->token.span;
        } break;

        default:
            report_unexpected(p, "factor (literal, ident, (expr), etc.)");

            a = ast_alloc(AstType_BadExpr, curr_token(p));
            a->span = curr_token(p).span;

            advance_token(p);

            // XP_ASSERT_DEFAULT(0);
            break;
        }
    }

    return a;
}

xp_internal Ast *parse_expr(Parser *p, isize min_prec, bool has_struct_init) {
    Ast *left = parse_factor(p, has_struct_init);

    Token curr;
    

    // TODO 检查
    for(;;) {
        curr = curr_token(p);
        if(curr.type == TokenType::LeftSquareBracket) {
            // 下标访问表达式

            expect(p, TokenType::LeftSquareBracket);
            Ast *index_expr = parse_expr(p);
            Token rsb = expect(p, TokenType::RightSquareBracket);
            
            Ast *new_left = ast_alloc(AstType_IndexExpr, curr);
            new_left->IndexExpr.array_var_expr = left;
            new_left->IndexExpr.index_expr = index_expr;
            new_left->span = merge(left->span, rsb.span);

            left = new_left;

        } else if(curr.type == TokenType::LeftBracket) {
            // 函数调用表达式

            Ast *a = ast_alloc(AstType_FunctionCallExpr, curr);
            a->FunctionCallExpr.func_ident = left;
            expect(p, TokenType::LeftBracket);

            a->FunctionCallExpr.args = make_array<Ast *>(ast_allocator());
            for(;;) {
                if(reach_end(p) || curr_token(p).type == TokenType::RightBracket) {
                    break;
                }

                Ast *arg = parse_expr(p);
                array_push_back(&a->FunctionCallExpr.args, arg);

                if(curr_token(p).type != TokenType::RightBracket) {
                    expect(p, TokenType::Comma);
                }
            }
            Token rb = expect(p, TokenType::RightBracket);

            a->span = merge(left->span, rb.span);

            left = a;
        } else if(curr.type == TokenType::Dot) {
            // 成员访问表达式

            Ast *new_left = ast_alloc(AstType_FieldAccess, expect(p, TokenType::Dot));
            new_left->FieldAccess.parent = left;
            Token field_name_token = expect(p, TokenType::Ident);
            new_left->FieldAccess.field_name = field_name_token.token_str;
            
            new_left->span = merge(left->span, field_name_token.span);

            left = new_left;
        } else if(curr.type == TokenType::LeftCurlyBracket && has_struct_init) {
            // 结构体初始化表达式, 
            // NOTE: 目前不能出现在如if, for 的条件表达式中, 因为无法区分代码块和结构体初始化表达式

            Ast *a = ast_alloc(AstType_StructInitExpr, curr);
            a->StructInitExpr.field_inits = make_array<Ast *>(ast_allocator());

            a->StructInitExpr.struct_type_ident = left;

            // TODO 目前只支持全部字段按顺序初始化(不能缺失)
            expect(p, TokenType::LeftCurlyBracket);

            for(;;) {
                if(reach_end(p) || curr_token(p).type == TokenType::RightCurlyBracket) {
                    break;
                }

                Ast *field_init_expr = parse_expr(p, 0, true);
                
                array_push_back(&a->StructInitExpr.field_inits, field_init_expr);

                if(curr_token(p).type != TokenType::RightCurlyBracket) {
                    expect(p, TokenType::Comma);
                }
            }

            Token rcb = expect(p, TokenType::RightCurlyBracket);

            a->span = merge(left->span, rcb.span);

            left = a;
        } else {
            break;
        }
    }


    for(;;) {
        curr = curr_token(p);
        
        if(is_binary_op(curr.type) && precedence(curr.type) > min_prec) {
            advance_token(p);
            Ast *right = parse_expr(p, precedence(curr.type));
            Ast *new_left = ast_alloc(AstType_BinaryExpr, curr);

            new_left->BinaryExpr.op = curr.type;
            new_left->BinaryExpr.left = left;
            new_left->BinaryExpr.right = right;
            new_left->span = merge(left->span, right->span);

            left = new_left;

            left->token = curr;
        } else {
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
        parse_integer(curr.number_info.pure_number_str.c_str, curr.number_info.type_kind_of_number, a, p);
        expect(p, TokenType::Integer);
    }

    if(curr.type == TokenType::Float) {
        parse_float(curr.number_info.pure_number_str.c_str, curr.number_info.type_kind_of_number, a, p);
        expect(p, TokenType::Float);
    }
    a->span = curr.span;

    return a;
}


#include <errno.h>
#include <inttypes.h>

void parse_integer(const char *str, TypeKind type_kind, Ast *a, Parser *p) {
    XP_ASSERT_DEFAULT(str != NULL && a != NULL);


    // 解析
    char *end = NULL;
    errno = 0;
    uintmax_t val = strtoumax(str, &end, 0);

    // 检查解析完成
    if(end == str) {
        UNREACHABLE();
    }

    // 检查溢出(字面量)
    if(errno == ERANGE) {
        context()->reporter.report(
            ErrorLevel::Error,
            a->token.span,
            p->f.source_code,
            "integer literal '%s' is too large",
            str
        );
    }

    
    // 检查溢出(类型)
    if(type_kind != Type_Undefined) {
        if(!check_literal_overflow(type_kind, cast(i128)val, 0.0)) {
            context()->reporter.report(
                ErrorLevel::Error,
                a->token.span,
                p->f.source_code,
                "integer literal '%s' can't fit in type '%s'",
                str,
                get_type_kind_str(type_kind)
            );
        }
    }

    a->type = AstType_Constant;
    a->Constant.value = cast(i128)val;

}

void parse_float(const char *str, TypeKind type_kind, Ast *a, Parser *p) {
    XP_ASSERT_DEFAULT(str != NULL && a != NULL);

    // 解析
    char *end;
    errno = 0;
    double val = strtod(str, &end);

    // 检查解析完成
    if(end == str) {
        UNREACHABLE();
    }

    // 检查溢出(字面量)
    if(errno == ERANGE) {
        if(val == HUGE_VAL) {
            context()->reporter.report(
                ErrorLevel::Error,
                a->token.span,
                p->f.source_code,
                "float literal '%s' is too large",
                str
            );
        } else if(val == -HUGE_VAL) {
            context()->reporter.report(
                ErrorLevel::Error,
                a->token.span,
                p->f.source_code,
                "float literal '%s' is too small",
                str
            );
        }


        // underflow (如1e-400) -> 返回0.0, 不算溢出
    }

    // 拒绝 NaN 和 Inf
    if(!isfinite(val)) {
        context()->reporter.report(
            ErrorLevel::Error,
            a->token.span,
            p->f.source_code,
            "float literal '%s' is not a finite number",
            str
        );
    }

    // 检查溢出(类型)
    if(type_kind != Type_Undefined) {
        if(!check_literal_overflow(type_kind, 0, val)) {
            context()->reporter.report(
                ErrorLevel::Error,
                a->token.span,
                p->f.source_code,
                "float literal '%s' can't fit in type '%s'",
                str,
                get_type_kind_str(type_kind)
            );
        }
    }

    a->type = AstType_Constant;
    a->Constant.float_value = val;
    
    return;
}



Ast *parse_array_init_expr(Parser *p, bool has_struct_init) {
    Ast *a = ast_alloc(AstType_ArrayInitExpr);
    a->ArrayInitExpr.elements = make_array<Ast *>(ast_allocator());

    a->token = expect(p, TokenType::LeftSquareBracket);
    for(;;) {
        if(reach_end(p) || curr_token(p).type == TokenType::RightSquareBracket) {
            break;
        }

        Ast *element_expr = parse_expr(p, 0, has_struct_init);

        array_push_back(&a->ArrayInitExpr.elements, element_expr);

        if(curr_token(p).type != TokenType::RightSquareBracket) {
            expect(p, TokenType::Comma);
        }
    }

    Token rsb = expect(p, TokenType::RightSquareBracket);

    a->span = merge(a->token.span, rsb.span);

    return a;
}


Ast *parse_ident(Parser *p) {
    Ast *a = ast_alloc(AstType_Ident, expect(p, TokenType::Ident));
    a->Ident.name = a->token.token_str;
    a->span = a->token.span;
    return a;
}


Ast *parse_single_ident_or_field_access_with_pure_ident(Parser *p) {
    Ast *a = NULL;

    a = parse_ident(p);

    while (curr_token(p).type == TokenType::Dot) {
        Token dot_t = expect(p, TokenType::Dot);

        auto field_token = expect2(p, TokenType::Ident);
        if(!field_token.second) {
            break;
        }
        
        Ast *new_a = ast_alloc(AstType_FieldAccess, dot_t);
        new_a->FieldAccess.parent = a;
        new_a->span = merge(a->span, field_token.first.span);
        new_a->FieldAccess.field_name = field_token.first.token_str;
        a = new_a;
    }

    return a;
}



//  Utils
bool reach_end(Parser *p) {
    return p->tokens[p->curr_token_index].type == TokenType::EndOfTokens && p->curr_token_index >= p->tokens.count - 1;
}


xp_internal Token curr_token(Parser *p) {
    return p->tokens[p->curr_token_index];
}

xp_internal Token next_token(Parser *p) {
    XP_ASSERT_MSG(p->curr_token_index < p->tokens.count - 1, "No next token, reached end of tokens\n");

    return p->tokens[p->curr_token_index + 1];
}

Token peek_token(Parser *p, isize offset) {
    if(p->curr_token_index + offset >= p->tokens.count) {
        return p->tokens[p->tokens.count - 1]; // EndOfTokens
    }

    return p->tokens[p->curr_token_index + offset];
}

xp_internal void advance_token(Parser *p) {
    if(!reach_end(p)) {
        p->curr_token_index += 1;
    }
    return;
}

void report_unexpected(Parser *p, const char *expected_string) {
    report_unexpected(p, curr_token(p), expected_string);
}

void report_unexpected(Parser *p, Token token, const char *expected_string) {
    report_unexpected(p, token, xp_string_c(expected_string));
}


void report_unexpected(Parser *p, Token token, xpString expected_string) {
    if(token.type == TokenType::EndOfTokens) {
        context()->reporter.report(
            ErrorLevel::Error, 
            token.span,
            p->f.source_code,
            "unexpected end of tokens, expected %s",
            expected_string.c_str
        );
    } else {
        context()->reporter.report(
            ErrorLevel::Error, 
            token.span,
            p->f.source_code,
            "unexpected token '%s', expected %s",
            token.token_str.c_str,
            expected_string.c_str
        );
    }
}

void report_unexpected(Parser *p, TokenType expected) {
    report_unexpected(p, curr_token(p), expected);
}

void report_unexpected(Parser *p, Token token, TokenType expected) {
    report_unexpected(p, token, token_strings[cast(i32) expected]);
} 


xp_internal Token expect(Parser *p, TokenType type) {
    Token curr = curr_token(p);

    if(curr.type != type) {
        report_unexpected(p, type);
    }
    // 无论如何都要前进, 否则错误恢复会陷入死循环
    advance_token(p);

    // 返回被期待的 token
    return curr;
}


xpPair<Token, bool> expect2(Parser *p, TokenType type) {
    Token curr = curr_token(p);
    
    // 无论如何都要前进, 否则错误恢复会陷入死循环
    advance_token(p);

    if(curr.type != type) {
        report_unexpected(p, curr, type);
        return xp_make_pair(curr, false);
    } else {
        return xp_make_pair(curr, true);
    }

}


Token expect_assert(Parser *p, TokenType type) {
    Token expected = expect(p, type);
    XP_ASSERT_MSG(expected.type == type, "Expected token type %s, but got %s", token_strings[cast(i32)type], token_strings[cast(i32)expected.type]);
    return expected;
}



void advance_to_next_top_level(Parser *p) {

    for(;;) {
        if(reach_end(p)) {
            break;
        }

        Token curr = curr_token(p);

        // toplevel开头肯定得是Ident
        if(curr.type == TokenType::Ident) {
            Token next_1 = peek_token(p, 1);
            Token next_2 = peek_token(p, 2);

            // Ident后面接着 "::", 才可能是函数声明或者结构体声明
            if(next_1.type == TokenType::DoubleColon) {

                // 如果接着是 "(" 或者 "struct" 就说明是函数声明或者结构体声明, 否则就继续往下找
                // if(next_2.type == TokenType::LeftBracket || next_2.type == TokenType::KW_struct) {
                break;
                // }
            }
        }

        // 或import
        if(curr.type == TokenType::KW_import) {
            break;
        }

        advance_token(p);
    }
}


void advance_to_next_stmt(Parser *p) {
    for(;;) {
        if(reach_end(p)) {
            break;
        }

        Token curr = curr_token(p);

        // 语句结尾肯定是分号或者右大括号
        if(curr.type == TokenType::Semicolon || curr.type == TokenType::RightCurlyBracket) {
            advance_token(p);
            break;
        }

        advance_token(p);
    }
}