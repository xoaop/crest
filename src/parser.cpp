
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
Token curr_token(Parser *p);
Token next_token(Parser *p);
void advance_token(Parser *p);
Token expect(Parser *p, TokenType type);
xpPair<Token, bool> expect2(Parser *p, TokenType type);
Token expect_assert(Parser *p, TokenType type);


void advance_to_next_top_level(Parser *p);



Ast *parse_function_value(Parser *p);
Ast *parse_block(Parser *p);


Ast *parse_if(Parser *p);
Ast *parse_for(Parser *p);


Ast *parse_expr_factor(Parser *p);
Ast *parse_expr(Parser *p, isize min_prec = 0);


Ast *parse_constant(Parser *p);
void parse_integer(const char *str, TypeKind type_kind, Ast *a, Parser *p);
void parse_float(const char *str, TypeKind type_kind, Ast *a, Parser *p);

Ast *parse_array_init_expr(Parser *p);


Ast *parse_type(Parser *p);

Ast *parse_ident(Parser *p);
Ast *parse_single_ident_or_field_access_with_pure_ident(Parser *p);

Ast *parse_stmt(Parser *p);

Ast *parse_const_decl(Parser *p);


AstFile parse_file(Array<Token> tokens, SourceCode src_code) {
    defer(xp_arena_allocator_clear(stage_allocator()));

    Parser p = parser_make(tokens);
    p.f.source_code = src_code;

    for(;;) {
        if(reach_end(&p)) {
            break;
        }
        
        // array_push_back<Ast *>(&p.f.top_levels, parse_top_level(&p));
        array_push_back(&p.f.top_levels, parse_stmt(&p));
    }

    #ifdef CREST_DEBUG
    print_ast(p.f.top_levels);
    #endif

    return p.f;
}


Ast *parse_type(Parser *p) {
    return parse_expr(p, 0);
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


    isize colon_index = xp_string_find_char(raw_path, ':');
    xpOption<xpString> search_prefix = xpOption<xpString>::none();
    xpString path = xp_make_string_zero();
    if(colon_index != -1) {
        xpString search_path_str = xp_make_string_count(ast_allocator(), raw_path.c_str, colon_index);
        search_prefix = xpOption<xpString>::some(search_path_str);
        path = xp_make_string_count(stage_allocator(), raw_path.c_str + colon_index + 1, raw_path.length - colon_index - 1);
    } else {
        path = raw_path;
    }


    Ast *a = ast_alloc(AstType_Import, import_token, merge(import_token.src_loc, path_succ.first.src_loc));
    a->Import.path = normalize_path(path, ast_allocator());
    a->Import.search_prefix = search_prefix;
    

    return a;
}


Ast *parse_enum_decl(Parser *p) {
    auto name_succ = expect2(p, TokenType::KW_enum);

    Ast *type = nullptr;
    if(curr_token(p).type != TokenType::LeftCurlyBracket) {
        type = parse_type(p);
    }


    expect(p, TokenType::LeftCurlyBracket);

    Array<Ast *> fields = make_array<Ast *>(ast_allocator());
    while(!reach_end(p)) {
        if(curr_token(p).type == TokenType::RightCurlyBracket) {
            break;
        }

        Token curr = curr_token(p);
        Token next = next_token(p);


        Ast *field = nullptr;
        if(curr.type == TokenType::Ident && next.type == TokenType::DoubleColon) {
            field = parse_const_decl(p);
        } else {
            // TODO: 错误恢复

            field = parse_ident(p);
        }
        
        array_push_back(&fields, field);
        expect(p, TokenType::Semicolon);
    }

    expect(p, TokenType::RightCurlyBracket);

    Ast *a = ast_alloc(AstType_EnumDecl, name_succ.first);
    a->EnumDecl.type_ast = type;
    a->EnumDecl.fields = fields;
    a->src_loc = merge(name_succ.first.src_loc, curr_token(p).src_loc);

    return a;
}


Ast *parse_union_decl(Parser *p) {
    auto name_succ = expect2(p, TokenType::KW_union);

    expect(p, TokenType::LeftCurlyBracket);

    Array<Ast *> field_types = make_array<Ast *>(ast_allocator());
    while(!reach_end(p)) {
        if(curr_token(p).type == TokenType::RightCurlyBracket) {
            break;
        }

        Token field_name_token = expect(p, TokenType::Ident);
        expect(p, TokenType::Colon);
        Ast *type_ast = parse_type(p);

        
        // TODO: 换掉StructField这个名字, 因为它现在不仅仅用于struct了, union的字段也复用了这个AST节点, 但是StructField这个名字又不太合适, 先暂时这样吧
        Ast *field_ast = ast_alloc(AstType_StructField, field_name_token);
        field_ast->StructField.name = field_name_token.token_str;
        field_ast->StructField.type_ast = type_ast;
        field_ast->src_loc = merge(field_name_token.src_loc, type_ast->src_loc);
        array_push_back(&field_types, field_ast);

        expect(p, TokenType::Semicolon);
    }

    Token rcb = expect(p, TokenType::RightCurlyBracket);

    Ast *a = ast_alloc(AstType_UnionDecl, name_succ.first);
    a->UnionDecl.fields = field_types;
    a->src_loc = merge(name_succ.first.src_loc, rcb.src_loc);

    return a;
}


Ast *parse_struct_decl(Parser *p) {
    auto name_succ = expect2(p, TokenType::KW_struct);
    
    expect(p, TokenType::LeftCurlyBracket);

    Array<Ast *> field_types = make_array<Ast *>(ast_allocator());
    while(!reach_end(p)) {
        if(curr_token(p).type == TokenType::RightCurlyBracket) {
            break;
        }

        Token field_name_token = expect(p, TokenType::Ident);
        expect(p, TokenType::Colon);
        Ast *type_ast = parse_type(p);

        
        Ast *field_ast = ast_alloc(AstType_StructField, field_name_token);
        field_ast->StructField.name = field_name_token.token_str;
        field_ast->StructField.type_ast = type_ast;
        field_ast->src_loc = merge(field_name_token.src_loc, type_ast->src_loc);
        array_push_back(&field_types, field_ast);

        expect(p, TokenType::Semicolon);
    }

    Token rcb = expect(p, TokenType::RightCurlyBracket);


    Ast *a = ast_alloc(AstType_StructDeclValue, name_succ.first);
    a->StructDeclValue.fields = field_types;
    a->src_loc = merge(name_succ.first.src_loc, rcb.src_loc);

    return a;
}


Ast *parse_function_value(Parser *p) {
    expect(p, TokenType::LeftBracket);

    Array<Ast *> params = make_array<Ast *>(ast_allocator());
    
    
    bool must_be_c_fn = false;
    while(!reach_end(p)) {
        if(curr_token(p).type == TokenType::RightBracket) {
            break;
        }

        // 表示必须是extern C函数, 因为目前只有extern C函数支持可变参数, 其他函数如果出现...就报错

        // @CleanUp: 代码略重复
        // 特殊处理 ... (可变参数)
        if(curr_token(p).type == TokenType::ThreeDots) {
            Token dots_token = expect(p, TokenType::ThreeDots);
            Ast *param_ast = ast_alloc(AstType_ParamDecl, dots_token);
            param_ast->ParamDecl.name = dots_token.token_str;
            param_ast->ParamDecl.type_ast = nullptr;
            param_ast->ParamDecl.is_var_arg = true;


            
            array_push_back(&params, param_ast);

            // 目前可变参数限制在extern_c函数中
            must_be_c_fn = true;

            // 可变参数必须是最后一个参数, 所以直接跳出循环
            break;
        }



        Token param_name_token = expect(p, TokenType::Ident);
        expect(p, TokenType::Colon);
        Ast *param_type_ast = parse_type(p);

        // TODO: VariableDecl换成ParameterDecl更合适
        // Ast *param_ast = ast_alloc(AstType_VariableDecl, param_name_token);
        // param_ast->VariableDecl.var_name = param_name_token.token_str;
        // param_ast->VariableDecl.type_ast = param_type_ast;
        // param_ast->span = merge(param_name_token.span, param_type_ast->span);
        Ast *param_ast = ast_alloc(AstType_ParamDecl, param_name_token);
        param_ast->ParamDecl.name = param_name_token.token_str;
        param_ast->ParamDecl.type_ast = param_type_ast;
        param_ast->ParamDecl.is_var_arg = false;
        param_ast->src_loc = merge(param_name_token.src_loc, param_type_ast->src_loc);


        array_push_back(&params, param_ast);

        if(curr_token(p).type != TokenType::RightBracket) {
            expect(p, TokenType::Comma);
        }
    }

    Token rb = expect(p, TokenType::RightBracket);

    Ast *return_type_ast = NULL;
    if(curr_token(p).type == TokenType::Arrow) {
        // 有返回值类型
        expect(p, TokenType::Arrow);
        return_type_ast = parse_type(p);
    } else {
        Ast *void_ast = ast_alloc(AstType_EasyType, rb);
        void_ast->EasyType.kind = Type_void;
        void_ast->src_loc = rb.src_loc;

        return_type_ast = void_ast;
    }

    bool is_extern_c = false;
    if(curr_token(p).type == TokenType::KW_extern_C) {
        is_extern_c = true;
        advance_token(p);
    } 
    
    Ast *block_ast = NULL;
    SourceLocation loc;
    if(is_extern_c) {
        // extern C函数没有函数体

        block_ast = NULL;
        loc = merge(rb.src_loc, return_type_ast->src_loc); // TODO: 这里的span不太好, 因为extern C函数没有函数体, 没有左大括号, 只能以返回值类型的结束位置作为函数声明的结束位置了, 这样span就不能完全覆盖整个函数声明了, 先暂时这样吧
    } else {
        block_ast = parse_block(p);
        block_ast->Block.is_function_body = true; // 标记这是一个函数体, 方便后面类型检查时区分普通块和函数体
        loc = merge(rb.src_loc, block_ast->src_loc);
    }

    if(must_be_c_fn && !is_extern_c) {
        context()->reporter.report_error(
            loc,
            "functions with variable arguments must be declared as extern C"
        );
    }
    



    Ast *a = ast_alloc(AstType_FunctionDeclValue, rb, loc);
    a->FunctionDeclValue.params = params;
    a->FunctionDeclValue.block = block_ast;
    a->FunctionDeclValue.return_type_ast = return_type_ast;
    a->FunctionDeclValue.is_extern_c = is_extern_c;

    return a;
}

Ast *parse_assignment_or_expr(Parser *p) {
    Ast *left = parse_expr(p, 0);

    if(curr_token(p).type == TokenType::Equal) {
        Token equal_token = expect(p, TokenType::Equal);
        Ast *right = parse_expr(p, 0);

        Ast *a = ast_alloc(AstType_Assignment, left->token);
        a->Assignment.left_var_expr = left;
        a->Assignment.right_expr = right;
        a->src_loc = merge(left->src_loc, right->src_loc);

        return a;
    } else {
        return left;
    }
}


Ast *parse_const_decl(Parser *p) {
    Token name_token;
    auto name_succ = expect2(p, TokenType::Ident);
    name_token = name_succ.first;

    // 有类型注解的常量声明
    Ast *type_ast = nullptr;
    if(curr_token(p).type == TokenType::Colon) {
        expect(p, TokenType::Colon);
        type_ast = parse_type(p);
    }


    expect(p, TokenType::DoubleColon);


    Ast *value_ast = NULL;
    Token curr = curr_token(p);
    switch(curr_token(p).type) {
    case TokenType::KW_struct: 
        value_ast = parse_struct_decl(p);
        break;
    case TokenType::LeftBracket: 
        value_ast = parse_function_value(p);
        break;
    case TokenType::KW_import:
        value_ast = parse_import(p);
        break;
    case TokenType::KW_enum:
        value_ast = parse_enum_decl(p);
        break;
    case TokenType::KW_union:
        value_ast = parse_union_decl(p);
        break;
    default:
        value_ast = parse_expr(p, 0);
        break;
    }


    Ast *const_decl = ast_alloc(AstType_ConstDecl, name_token);
    const_decl->ConstDecl.name = name_token.token_str;
    const_decl->ConstDecl.type_ast = type_ast;
    const_decl->ConstDecl.value_ast = value_ast;
    const_decl->src_loc = merge(name_token.src_loc, value_ast->src_loc);

    return const_decl;
}




Ast *parse_var_decl(Parser *p) {

    Token name_token;
    auto name_succ = expect2(p, TokenType::Ident);
    name_token = name_succ.first;

    Ast *a = NULL;
    Token curr = curr_token(p);
    switch(curr.type) {
        case TokenType::ColonEqual: {
            // VariableDecl without type annotation

            expect(p, TokenType::ColonEqual);

            a = ast_alloc(AstType_VariableDecl, name_token);

            a->VariableDecl.var_name = name_token.token_str;

            Token curr = curr_token(p);
            if(curr.type == TokenType::TripleMinus) {
                // 无初始化

                Token tm = expect(p, TokenType::TripleMinus);

                a->VariableDecl.no_zero_init = true;

                a->src_loc = merge(a->token.src_loc, tm.src_loc);
            } else {
                // 有初始化表达式

                a->VariableDecl.no_zero_init = false;
                a->VariableDecl.expr = parse_expr(p, 0);

                a->src_loc = merge(a->token.src_loc, a->VariableDecl.expr->src_loc);
            }

            a->VariableDecl.type_ast = NULL;
        } break;

        case TokenType::Colon: {
            // VariableDecl with type annotation
            
            expect(p, Colon);
            
            a = ast_alloc(AstType_VariableDecl, name_token);

            a->VariableDecl.var_name = name_token.token_str;

            a->VariableDecl.type_ast = parse_type(p);
            
            Token curr = curr_token(p);
            if(curr.type == TokenType::Equal) {
                expect(p, TokenType::Equal);

                Token curr = curr_token(p);
                if(curr.type == TokenType::TripleMinus) {
                    // 无初始化

                    Token tm = expect(p, TokenType::TripleMinus);

                    a->VariableDecl.no_zero_init = true;
                    a->VariableDecl.expr = NULL;

                    a->src_loc = merge(a->token.src_loc, tm.src_loc);
                } else {
                    // 有初始化表达式

                    a->VariableDecl.no_zero_init = false;
                    a->VariableDecl.expr = parse_expr(p, 0);

                    a->src_loc = merge(a->token.src_loc, a->VariableDecl.expr->src_loc);
                }
                
            } else {
                // 零初始化

                a->VariableDecl.no_zero_init = false;
                a->VariableDecl.expr = NULL;

                a->src_loc = merge(a->token.src_loc, a->VariableDecl.type_ast->src_loc);
            }

        } break;

        default: {
            UNREACHABLE();
        } break;

    }

    return a;
}


Ast *parse_stmt(Parser *p) {
    Token curr = curr_token(p);
    Token next = next_token(p);

    Ast *a = NULL;
    switch(curr.type) {
    case TokenType::KW_if:
        a = parse_if(p);
        break;

    case TokenType::KW_for:
        a = parse_for(p);
        break;
    
    
    case TokenType::LeftCurlyBracket:
        a = parse_block(p);
        break;

    case TokenType::KW_return:
        a = ast_alloc(AstType_ReturnStmt, curr);
        advance_token(p);

        if(curr_token(p).type != TokenType::Semicolon) {
            a->ReturnStmt.expr = parse_expr(p, 0);
            a->src_loc = merge(a->token.src_loc, a->ReturnStmt.expr->src_loc);
        } else {
            a->ReturnStmt.expr = NULL;
            a->src_loc = a->token.src_loc;
        }

        expect(p, TokenType::Semicolon);
        break;

    case TokenType::KW_break:
        a = ast_alloc(AstType_Break);
        a->token = expect(p, TokenType::KW_break);
        a->src_loc = a->token.src_loc;
        expect(p, TokenType::Semicolon);
        break;

    case TokenType::KW_continue:
        a = ast_alloc(AstType_Continue);
        a->token = expect(p, TokenType::KW_continue);
        a->src_loc = a->token.src_loc;
        expect(p, TokenType::Semicolon);
        break;
    
    case TokenType::Ident: 
        if(next.type == TokenType::ColonEqual || next.type == TokenType::Colon) {
            a = parse_var_decl(p);
            expect(p, TokenType::Semicolon);

        } else if(next.type == TokenType::DoubleColon) {
            a = parse_const_decl(p);
        } else {
            a = parse_assignment_or_expr(p);
            expect(p, TokenType::Semicolon);

        }
        break;
    
    // TODO: 放到parse_const_decl里去处理
    case TokenType::KW_import:
        a = parse_import(p);

        // 特殊处理, 包装成ConstDecl, 这样就不需要在顶层语义分析阶段单独处理import了
        {
            Ast *import_ast = a;
            a = ast_alloc(AstType_ConstDecl, import_ast->token);
            a->ConstDecl.name = get_last_component_of_path(import_ast->Import.path, ast_allocator());
            a->ConstDecl.value_ast = import_ast;
            a->src_loc = import_ast->src_loc;
        }


        break;

    default:
        a = parse_assignment_or_expr(p);
        expect(p, TokenType::Semicolon);
        break;
    }

    return a;
}




Ast *parse_block(Parser *p) {
    Ast *a = ast_alloc(AstType_Block, expect(p, TokenType::LeftCurlyBracket));
    auto stmts = make_array<Ast *>(ast_allocator());


    while (curr_token(p).type != TokenType::RightCurlyBracket && !reach_end(p)) {
        array_push_back(&stmts, parse_stmt(p));
    }
    a->Block.statements = stmts;
    a->Block.is_function_body = false; // NOTE: 默认不是函数体, 函数部分需要单独设置

    Token rcb = expect(p, TokenType::RightCurlyBracket);

    a->src_loc = merge(a->token.src_loc, rcb.src_loc);

    return a;
}



Ast *parse_if(Parser *p) {
    Ast *a = ast_alloc(AstType_IfStmt);
    a->token = expect(p, TokenType::KW_if);

    
    a->IfStmt.condition = parse_expr(p, 0);
    a->IfStmt.then_block = parse_block(p);
    
    a->IfStmt.else_block = NULL;
    if(curr_token(p).type == TokenType::KW_else) {
        expect(p, TokenType::KW_else);
        if(curr_token(p).type == TokenType::KW_if) {
            a->IfStmt.else_block = ast_alloc(AstType_Block, curr_token(p)); // 虚拟的Block节点, 只是为了else if的情况, 让else if的条件和then块都成为这个Block节点的子节点, 这样就能统一else if和else的处理了
            a->IfStmt.else_block->Block.statements = make_array<Ast *>(ast_allocator());
            array_push_back(&a->IfStmt.else_block->Block.statements, parse_if(p));

            a->IfStmt.else_block->src_loc = a->IfStmt.else_block->Block.statements[0]->src_loc;
        } else {
            a->IfStmt.else_block = parse_block(p);
        }
    }
    
    if(a->IfStmt.else_block != NULL) {
        a->src_loc = merge(a->token.src_loc, a->IfStmt.else_block->src_loc);
    } else {
        a->src_loc = merge(a->token.src_loc, a->IfStmt.then_block->src_loc);
    }

    return a;
}

Ast *parse_for(Parser *p) {
    Ast *a = ast_alloc(AstType_ForStmt);
    a->token = expect(p, TokenType::KW_for);


    // TODO 换成现代 for 语法, 而不是现在的 C 风格 for 语法
    // init
    if(curr_token(p).type != TokenType::Semicolon) {
        a->ForStmt.init = parse_var_decl(p); // 只允许变量声明
    } else {
        a->ForStmt.init = NULL;
    }
    expect(p, TokenType::Semicolon);

    // condition
    if(curr_token(p).type != TokenType::Semicolon) {
        a->ForStmt.condition = parse_expr(p, 0);
    } else {
        a->ForStmt.condition = NULL;
    }
    expect(p, TokenType::Semicolon);

    // post
    if(curr_token(p).type != TokenType::LeftCurlyBracket) {
        a->ForStmt.post = parse_assignment_or_expr(p);

        if(a->ForStmt.post->type != AstType_Assignment) {
            context()->reporter.report(
                ErrorLevel::Error,
                curr_token(p).src_loc,
                "expected assignment expression in for loop post statement, got expression that is not an assignment"
            );
        }
        XP_ASSERT_DEFAULT(a->ForStmt.post->type == AstType_Assignment);
    } else {
        a->ForStmt.post = NULL;
    }


    a->ForStmt.body = parse_block(p);

    a->src_loc = merge(a->token.src_loc, a->ForStmt.body->src_loc);

    return a;
}



Ast *parse_string_literal(Parser *p) {
    XP_ASSERT_DEFAULT(curr_token(p).type == TokenType::StringLiteral);

    Token str_token = expect(p, TokenType::StringLiteral);

    xpString raw_str_with_quotes = str_token.token_str;
    XP_ASSERT_DEFAULT(raw_str_with_quotes.c_str[0] == '"' && raw_str_with_quotes.c_str[raw_str_with_quotes.length - 1] == '"');


    isize raw_str_count = raw_str_with_quotes.length - 2; // 去掉前后的引号
    char *iter = raw_str_with_quotes.c_str + 1; // 跳过开头的引号

    xpString parsed_str = xp_make_string(ast_allocator(), ""); // 先创建一个空字符串, 后面逐个字符追加


    isize curr_index = 0;
    while(curr_index < raw_str_count) {
        if(iter[curr_index] == '\\') {
            // 处理转义字符

            curr_index += 1; // 跳过反斜杠

            if(curr_index >= raw_str_count) {
                context()->reporter.report(
                    ErrorLevel::Error,
                    str_token.src_loc,
                    "invalid escape sequence in string literal: unexpected end of string after backslash"
                );
                break;
            }

            switch(iter[curr_index]) {
                case 'n':
                    xp_string_append_char(&parsed_str, '\n');
                    curr_index += 1;
                    break;
                case 't':
                    xp_string_append_char(&parsed_str, '\t');
                    curr_index += 1;
                    break;
                case 'r':
                    xp_string_append_char(&parsed_str, '\r');
                    curr_index += 1;
                    break;
                case '\\':
                    xp_string_append_char(&parsed_str, '\\');
                    curr_index += 1;
                    break;
                case '"':
                    xp_string_append_char(&parsed_str, '"');
                    curr_index += 1;
                    break;
                case '\'': 
                    xp_string_append_char(&parsed_str, '\'');
                    curr_index += 1;
                    break;
                case 'a':
                    xp_string_append_char(&parsed_str, '\a');
                    curr_index += 1;
                    break;
                case 'b':
                    xp_string_append_char(&parsed_str, '\b');
                    curr_index += 1;
                    break;
                case 'f':
                    xp_string_append_char(&parsed_str, '\f');
                    curr_index += 1;
                    break;
                case 'v':
                    xp_string_append_char(&parsed_str, '\v');
                    curr_index += 1;
                    break;

                case '0': case '1': case '2': case '3': 
                case '4': case '5': case '6': case '7': {
                    // 八进制转义, 最多三位八进制数

                    u32 value = 0;
                    isize count = 0;

                    while(curr_index < raw_str_count && count < 3) {
                        char curr_char = iter[curr_index];

                        if(curr_char >= '0' && curr_char <= '7') {
                            value = value * 8 + (curr_char - '0');
                            curr_index += 1;
                            count += 1;
                        } else {
                            break;
                        }
                    }

                    xp_string_append_char(&parsed_str, cast(char)value);
                } break;
                
                case 'x': {
                    // 十进制
                    u32 value = 0;
                    bool has_digits = false;

                    curr_index += 1; // 跳过 'x'

                    while(curr_index < raw_str_count) {
                        char curr_char = iter[curr_index];
                        int digit = -1;

                        if(curr_char >= '0' && curr_char <= '9') {
                            digit = curr_char - '0';
                        } else if(curr_char >= 'a' && curr_char <= 'f') {
                            digit = curr_char - 'a' + 10;
                        } else if(curr_char >= 'A' && curr_char <= 'F') {
                            digit = curr_char - 'A' + 10;
                        } else {
                            break;
                        }

                        if(digit != -1) {
                            value = (value << 4) | digit;
                            has_digits = true;
                            curr_index += 1;
                        } else {
                            break;
                        }

                    }

                    if(!has_digits) {
                        context()->reporter.report(
                            ErrorLevel::Error,
                            str_token.src_loc,
                            "invalid escape sequence in string literal: \\x must be followed by at least one hexadecimal digit"
                        );
                    } else {
                        xp_string_append_char(&parsed_str, cast(char)value);
                    }
                } break;


                
                default:
                    context()->reporter.report(
                        ErrorLevel::Error,
                        SourceLocation(p->f.source_code, Span{str_token.src_loc.span.start + 1 + curr_index, 1}),
                        "invalid escape sequence in string literal: unrecognized escape character"
                    );
            }


        } else {
            // 非转义字符, 直接追加

            xp_string_append_char(&parsed_str, iter[curr_index]);
            curr_index += 1;
        }
    }

    
    if(curr_index == raw_str_count) {
        // 代表解析成功

        Ast *a = ast_alloc(AstType_StringLiteralExpr, str_token, str_token.src_loc);
        a->StringLiteralExpr.str = parsed_str;
        return a;
    } else {
        // 代表解析失败, 已经报告了错误, 这里返回一个BadExpr占位符就行了
        UNREACHABLE();
        // Ast *a = ast_alloc(AstType_BadExpr, str_token, str_token.span);
        // return a;
    }

    return nullptr;
}

Ast *parse_expr_factor(Parser *p) {
    Ast *a = NULL;
    defer(XP_ASSERT_DEFAULT(a != NULL));

    Token curr = curr_token(p);


    if(is_unary_op(curr.type)) {
        advance_token(p);

        a = ast_alloc(AstType_UnaryExpr, curr);
        a->UnaryExpr.op = curr.type;

        
        
        // NOTE: 比 所有二元运算符 优先级都高 就行
        #define UNARY_OP_PRECEDENCE 114514
        a->UnaryExpr.operand = parse_expr(p, UNARY_OP_PRECEDENCE);

        a->src_loc = merge(curr.src_loc, a->UnaryExpr.operand->src_loc);

    } else {
        switch (curr.type)
        {

        // 字面量
        case TokenType::Integer:
        case TokenType::Float:
        case TokenType::KW_true:
        case TokenType::KW_false:
            a = parse_constant(p);
            break;

        // 关键字常量
        case TokenType::KW_null: {
            a = ast_alloc(AstType_Constant);

            Value null_val = make_value(pointer_type(easy_type(Type_void)));
            null_val.is_null = true;

            a->Constant.value = null_val;

            a->token = expect(p, curr.type);
            a->src_loc = a->token.src_loc;

        } break;
        
        // (expr) or (params) -> ret
        case TokenType::LeftBracket: {
            Token lb = expect(p, TokenType::LeftBracket);
            Array<Ast *> items = make_array<Ast *>(stage_allocator());

            if (curr_token(p).type != TokenType::RightBracket) {
                for (;;) {
                    array_push_back(&items, parse_expr(p, 0));
                    if (curr_token(p).type == TokenType::RightBracket) break;
                    expect(p, TokenType::Comma);
                }
            }
            Token rb = expect(p, TokenType::RightBracket);

            if (curr_token(p).type == TokenType::Arrow) {
                // Function type
                expect(p, TokenType::Arrow);
                Ast *ret = parse_expr(p, 0);
                a = ast_alloc(AstType_FunctionType, lb);
                a->FunctionType.param_types = array_copy(&items, ast_allocator());
                a->FunctionType.return_type_ast = ret;
                a->src_loc = merge(lb.src_loc, ret->src_loc);
            } else if (items.count == 1) {
                a = items[0];   // parenthesized expr
            } else {
                // () or (a, b) without -> — error
                report_unexpected(p, "-> (function type) or single expression");
                a = ast_alloc(AstType_BadExpr, rb);
                a->src_loc = rb.src_loc;
            }
        } break;
        
        

        // 指针类型 / 数组类型 / 切片类型
        case TokenType::Star: {
            a = ast_alloc(AstType_PointerType, expect(p, TokenType::Star));
            Ast *pointed = parse_expr(p, 0);
            a->PointerType.pointed_type_ast = pointed;
            a->src_loc = merge(a->token.src_loc, pointed->src_loc);
        } break;

        case TokenType::LeftSquareBracket: {
            if (next_token(p).type == TokenType::RightSquareBracket) {
                // Slice type
                a = ast_alloc(AstType_SliceType, curr_token(p));
                expect(p, TokenType::LeftSquareBracket);
                expect(p, TokenType::RightSquareBracket);
                Ast *elem = parse_expr(p, 0);
                a->SliceType.element_type_ast = elem;
                a->src_loc = merge(a->token.src_loc, elem->src_loc);
            } else {
                // Array type
                a = ast_alloc(AstType_ArrayType, expect(p, TokenType::LeftSquareBracket));
                Ast *count = parse_expr(p, 0);
                a->ArrayType.count_expr = count;
                expect(p, TokenType::RightSquareBracket);
                Ast *elem = parse_expr(p, 0);
                a->ArrayType.element_type_ast = elem;
                a->src_loc = merge(a->token.src_loc, elem->src_loc);
            }
        } break;

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
            a->CastExpr.expr = parse_expr(p, UNARY_OP_PRECEDENCE);

            a->src_loc = merge(a->token.src_loc, a->CastExpr.expr->src_loc);
            break;

        case TokenType::LeftCurlyBracket:
            // 数组字面量

            a = parse_array_init_expr(p);
            break;

        case TokenType::StringLiteral: {
            
            // 字符串字面量
            a = parse_string_literal(p);

        } break;

        default:
            report_unexpected(p, "factor (literal, ident, (expr), etc.)");

            a = ast_alloc(AstType_BadExpr, curr_token(p));
            a->src_loc = curr_token(p).src_loc;

            advance_token(p);

            // XP_ASSERT_DEFAULT(0);
            break;
        }
    }

    return a;
}

Ast *parse_expr(Parser *p, isize min_prec) {
    Ast *left = parse_expr_factor(p);

    Token curr;
    

    // TODO 检查
    for(;;) {
        curr = curr_token(p);
        if(curr.type == TokenType::LeftSquareBracket) {
            // 下标访问表达式

            expect(p, TokenType::LeftSquareBracket);
            Ast *index_expr = parse_expr(p, 0);
            Token rsb = expect(p, TokenType::RightSquareBracket);
            
            Ast *new_left = ast_alloc(AstType_IndexExpr, curr);
            new_left->IndexExpr.array_var_expr = left;
            new_left->IndexExpr.index_expr = index_expr;
            new_left->src_loc = merge(left->src_loc, rsb.src_loc);

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

                Ast *arg = parse_expr(p, 0);
                array_push_back(&a->FunctionCallExpr.args, arg);

                if(curr_token(p).type != TokenType::RightBracket) {
                    expect(p, TokenType::Comma);
                }
            }
            Token rb = expect(p, TokenType::RightBracket);

            a->src_loc = merge(left->src_loc, rb.src_loc);

            left = a;
        } else if(curr.type == TokenType::Dot) {
            // 成员访问表达式 or  结构体初始化表达式

            Token next = next_token(p);

            if(next.type == TokenType::LeftCurlyBracket) {
                // 结构体初始化表达式, 形如 Point.{1, 2}

                Ast *a = ast_alloc(AstType_StructInitExpr, curr);
                a->StructInitExpr.field_inits = make_array<Ast *>(ast_allocator());
                a->StructInitExpr.struct_type_ident = left;
                

                expect(p, TokenType::Dot);
                expect(p, TokenType::LeftCurlyBracket);
                
                // TODO 目前只支持全部字段按顺序初始化(不能缺失)
                for(;;) {
                    if(reach_end(p) || curr_token(p).type == TokenType::RightCurlyBracket) {
                        break;
                    }

                    Ast *field_init_expr = parse_expr(p, 0);
                    
                    array_push_back(&a->StructInitExpr.field_inits, field_init_expr);

                    if(curr_token(p).type != TokenType::RightCurlyBracket) {
                        expect(p, TokenType::Comma);
                    }
                }

                Token rcb = expect(p, TokenType::RightCurlyBracket);

                a->src_loc = merge(left->src_loc, rcb.src_loc);

                left = a;
            } else {
                // 成员访问表达式, 形如 a.b

                Ast *new_left = ast_alloc(AstType_FieldAccess, expect(p, TokenType::Dot));
                new_left->FieldAccess.parent = left;
                Token field_name_token = expect(p, TokenType::Ident);
                new_left->FieldAccess.field_name = field_name_token.token_str;
                
                new_left->src_loc = merge(left->src_loc, field_name_token.src_loc);

                left = new_left;
            }

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
            new_left->src_loc = merge(left->src_loc, right->src_loc);

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

    if(curr.type == TokenType::KW_true || curr.type == TokenType::KW_false) {
        a->type = AstType_Constant;

        Value value = make_value();
        value.bool_val(curr.type == TokenType::KW_true);
        value.set_type(easy_type(Type_bool));
        a->Constant.value = value;

        expect(p, curr.type);
    }

    a->src_loc = curr.src_loc;

    return a;
}


#include <errno.h>
#include <inttypes.h>

void parse_integer(const char *str, TypeKind type_kind, Ast *a, Parser *p) {
    XP_ASSERT_DEFAULT(str != NULL && a != NULL);

    bool has_minus = false;
    if(str[0] == '-') {
        has_minus = true;
        str++; // 跳过负号, 因为strtoumax不处理负号, 需要我们自己处理
    }

    // 解析
    char *end = NULL;
    errno = 0;
    i128 val = strtoumax(str, &end, 0);

    // 检查解析完成
    if(end == str) {
        UNREACHABLE();
    }

    // 检查溢出(字面量)
    if(errno == ERANGE) {
        context()->reporter.report(
            ErrorLevel::Error,
            a->token.src_loc,
            "integer literal '{}' is too large",
            str
        );
    }

    // 检查负数字面量
    if(has_minus) {
        val = -val;
        if(!(val >= INTPTR_MIN && val <= INTPTR_MAX)) {
            context()->reporter.report(
                ErrorLevel::Error,
                a->token.src_loc,
                "integer literal '-{}' is too small",
                str
            );
        }

    }

    // TODO: 统一
    // 检查溢出(类型)
    if(type_kind != Type_Undefined) {
        if(check_integer_overflow(val, easy_type(type_kind))) {
            context()->reporter.report(
                ErrorLevel::Error,
                a->token.src_loc,
                "integer literal '{}' can't fit in type '{}'",
                str,
                get_type_kind_str(type_kind)
            );
        }
    }

    a->type = AstType_Constant;

    Value value = make_value();
    value.integer_val(val);
    if(type_kind != Type_Undefined) {
        value.set_type(easy_type(type_kind));
    } else {
        value.set_type(easy_type(Type_untyped_int));
    }
    a->Constant.value = value;
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
                a->token.src_loc,
                "float literal '{}' is too large",
                str
            );
        } else if(val == -HUGE_VAL) {
            context()->reporter.report(
                ErrorLevel::Error,
                a->token.src_loc,
                "float literal '{}' is too small",
                str
            );
        }


        // underflow (如1e-400) -> 返回0.0, 不算溢出
    }

    // 拒绝 NaN 和 Inf
    if(!isfinite(val)) {
        context()->reporter.report(
            ErrorLevel::Error,
            a->token.src_loc,
            "float literal '{}' is not a finite number",
            str
        );
    }

    // TODO: 统一
    // 检查溢出(类型)
    if(type_kind != Type_Undefined) {
        if(check_float_overflow(val, easy_type(type_kind))) {
            context()->reporter.report(
                ErrorLevel::Error,
                a->token.src_loc,
                "float literal '{}' can't fit in type '{}'",
                str,
                get_type_kind_str(type_kind)
            );
        }
    }

    a->type = AstType_Constant;
    
    Value value = make_value();
    value.set_type(type_kind != Type_Undefined ? easy_type(type_kind) : easy_type(Type_untyped_float));
    value.float_val(val);

    a->Constant.value = value;
    
    return;
}



Ast *parse_array_init_expr(Parser *p) {
    Ast *a = ast_alloc(AstType_ArrayInitExpr);
    a->ArrayInitExpr.elements = make_array<Ast *>(ast_allocator());

    a->token = expect(p, TokenType::LeftCurlyBracket);
    for(;;) {
        if(reach_end(p) || curr_token(p).type == TokenType::RightCurlyBracket) {
            break;
        }

        Ast *element_expr = parse_expr(p, 0);

        array_push_back(&a->ArrayInitExpr.elements, element_expr);

        if(curr_token(p).type != TokenType::RightCurlyBracket) {
            expect(p, TokenType::Comma);
        }
    }

    Token rsb = expect(p, TokenType::RightCurlyBracket);

    a->src_loc = merge(a->token.src_loc, rsb.src_loc);

    return a;
}


Ast *parse_ident(Parser *p) {
    Ast *a = ast_alloc(AstType_Ident, expect(p, TokenType::Ident));
    a->Ident.name = a->token.token_str;
    a->src_loc = a->token.src_loc;
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
        new_a->src_loc = merge(a->src_loc, field_token.first.src_loc);
        new_a->FieldAccess.field_name = field_token.first.token_str;
        a = new_a;
    }

    return a;
}



//  Utils
bool reach_end(Parser *p) {
    return p->tokens[p->curr_token_index].type == TokenType::EndOfTokens && p->curr_token_index >= p->tokens.count - 1;
}


Token curr_token(Parser *p) {
    return p->tokens[p->curr_token_index];
}

Token next_token(Parser *p) {
    XP_ASSERT_MSG(p->curr_token_index < p->tokens.count - 1, "No next token, reached end of tokens\n");

    return p->tokens[p->curr_token_index + 1];
}

Token peek_token(Parser *p, isize offset) {
    if(p->curr_token_index + offset >= p->tokens.count) {
        return p->tokens[p->tokens.count - 1]; // EndOfTokens
    }

    return p->tokens[p->curr_token_index + offset];
}

void advance_token(Parser *p) {
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
            token.src_loc,
            "unexpected end of tokens, expected {}",
            expected_string
        );
    } else {
        context()->reporter.report(
            ErrorLevel::Error, 
            token.src_loc,
            "unexpected token '{}', expected {}",
            token.token_str,
            expected_string
        );
    }
}

void report_unexpected(Parser *p, TokenType expected) {
    report_unexpected(p, curr_token(p), expected);
}

void report_unexpected(Parser *p, Token token, TokenType expected) {
    report_unexpected(p, token, token_strings[cast(i32) expected]);
} 


Token expect(Parser *p, TokenType type) {
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