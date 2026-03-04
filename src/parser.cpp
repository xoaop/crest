
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
xp_internal Ast *parse_if(Parser *p);
xp_internal Ast *parse_for(Parser *p);


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

Ast *parse_stmt(Parser *p);
Ast *parse_named_stmt(Parser *p);




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

    #ifdef DEBUG_PRINT
    print_ast(p.f.top_levels);
    #endif

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


    Ast *a = ast_alloc(AstType_Import, import_token, merge(import_token.span, path_succ.first.span));
    a->Import.path = normalize_path(path, ast_allocator());
    a->Import.search_prefix = search_prefix;
    

    return a;
}



// TODO FINISH
Ast *parse_struct_value(Parser *p) {
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
        field_ast->span = merge(field_name_token.span, type_ast->span);
        array_push_back(&field_types, field_ast);
        

        if(curr_token(p).type != TokenType::RightCurlyBracket) {
            expect(p, TokenType::Comma);
        }
    }

    Token rcb = expect(p, TokenType::RightCurlyBracket);


    Ast *a = ast_alloc(AstType_StructDeclValue, name_succ.first);
    a->StructDeclValue.fields = field_types;
    a->span = merge(name_succ.first.span, rcb.span);

    return a;
}


Ast *parse_function_value(Parser *p) {
    expect(p, TokenType::LeftBracket);

    Array<Ast *> params = make_array<Ast *>(ast_allocator());
    while(!reach_end(p)) {
        if(curr_token(p).type == TokenType::RightBracket) {
            break;
        }

        Token param_name_token = expect(p, TokenType::Ident);
        expect(p, TokenType::Colon);
        Ast *param_type_ast = parse_type(p);

        Ast *param_ast = ast_alloc(AstType_VariableDecl, param_name_token);
        param_ast->VariableDecl.var_name = param_name_token.token_str;
        param_ast->VariableDecl.type_ast = param_type_ast;
        param_ast->span = merge(param_name_token.span, param_type_ast->span);

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
        void_ast->span = rb.span; // TODO: FIX

        return_type_ast = void_ast;
    }

    bool is_extern_c = false;
    if(curr_token(p).type == TokenType::KW_extern_C) {
        is_extern_c = true;
        advance_token(p);
    } 
    
    Ast *block_ast = NULL;
    Span span;
    if(is_extern_c) {
        // extern C函数没有函数体

        block_ast = NULL;
        span = merge(rb.span, return_type_ast->span); // TODO: 这里的span不太准确
    } else {
        block_ast = parse_block(p);
        block_ast->Block.is_function_body = true; // 标记这是一个函数体, 方便后面类型检查时区分普通块和函数体
        span = merge(rb.span, block_ast->span);
    }






    Ast *a = ast_alloc(AstType_FunctionDeclValue, rb);
    a->FunctionDeclValue.params = params;
    a->FunctionDeclValue.block = block_ast;
    a->FunctionDeclValue.return_type_ast = return_type_ast;
    a->FunctionDeclValue.is_extern_c = is_extern_c;
    a->span = span;

    return a;
}

Ast *parse_assignment_or_expr(Parser *p, bool has_struct_init = true) {
    Ast *left = parse_expr(p, 0, true);

    if(curr_token(p).type == TokenType::Equal) {
        Token equal_token = expect(p, TokenType::Equal);
        Ast *right = parse_expr(p, 0, has_struct_init);

        Ast *a = ast_alloc(AstType_Assignment, left->token);
        a->Assignment.left_var_expr = left;
        a->Assignment.right_expr = right;
        a->span = merge(left->span, right->span);

        return a;
    } else {
        return left;
    }
}


Ast *parse_const_decl(Parser *p) {
    Token name_token;
    auto name_succ = expect2(p, TokenType::Ident);
    name_token = name_succ.first;

    expect(p, TokenType::DoubleColon);


    Ast *value_ast = NULL;
    Token curr = curr_token(p);
    switch(curr_token(p).type) {
    case TokenType::KW_struct: 
        value_ast = parse_struct_value(p);
        break;
    case TokenType::LeftBracket: 
        value_ast = parse_function_value(p);
        break;
    case TokenType::KW_import:
        value_ast = parse_import(p);
        break;
    default:
        value_ast = parse_expr(p, 0, true);
        break;
    }


    Ast *const_decl = ast_alloc(AstType_ConstDecl, name_token);
    const_decl->ConstDecl.name = name_token.token_str;
    const_decl->ConstDecl.value_ast = value_ast;
    const_decl->span = merge(name_token.span, value_ast->span);

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

            a->span = merge(a->token.span, tm.span);
        } else {
            // 有初始化表达式

            a->VariableDecl.no_zero_init = false;
            a->VariableDecl.expr = parse_expr(p, 0, true);

            a->span = merge(a->token.span, a->VariableDecl.expr->span);
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
            a->span = import_ast->span;
        }


        break;

    default:
        a = parse_assignment_or_expr(p);
        expect(p, TokenType::Semicolon);
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
        a->ForStmt.init = parse_var_decl(p); // 只允许变量声明
    } else {
        a->ForStmt.init = NULL;
    }
    expect(p, TokenType::Semicolon);

    // condition
    if(curr_token(p).type != TokenType::Semicolon) {
        a->ForStmt.condition = parse_expr(p, 0, true);
    } else {
        a->ForStmt.condition = NULL;
    }
    expect(p, TokenType::Semicolon);

    // post
    if(curr_token(p).type != TokenType::LeftCurlyBracket) {
        a->ForStmt.post = parse_assignment_or_expr(p, false); // for循环的post部分不允许出现结构体初始化表达式, 因为语法上不好区分, 例如 a = {1, 2} 是赋值表达式还是结构体初始化表达式, 只能当作赋值表达式来解析, 这样就要求for循环的post部分不能出现结构体初始化表达式了

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
                    str_token.span,
                    p->f.source_code,
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
                            {str_token.span.start + 1 + curr_index, 1},
                            p->f.source_code,
                            "invalid escape sequence in string literal: \\x must be followed by at least one hexadecimal digit"
                        );
                    } else {
                        xp_string_append_char(&parsed_str, cast(char)value);
                    }
                } break;


                
                default:
                    context()->reporter.report(
                        ErrorLevel::Error,
                        {str_token.span.start + 1 + curr_index, 1},
                        p->f.source_code,
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

        Ast *a = ast_alloc(AstType_StringLiteralExpr, str_token, str_token.span);
        a->StringLiteralExpr.str = parsed_str;
        return a;
    } else {
        // 代表解析失败, 已经报告了错误, 这里返回一个BadExpr占位符就行了

        Ast *a = ast_alloc(AstType_BadExpr, str_token, str_token.span);
        return a;
    }
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
            /* DEPRECATED
            // a = ast_alloc(AstType_StringLiteralExpr);
            // a->token = expect(p, TokenType::StringLiteral);

            // // TODO: xpString的稳定性的试金石
            // xpString str = a->token.token_str;
            // str.c_str = str.c_str + 1; //跳过开头的引号
            // str.length -= 2; //去掉前后的引号
            // str.capacity -= 2; //去掉前后的引号

            // a->StringLiteralExpr.str = str;

            // a->span = a->token.span;
            */

            a = parse_string_literal(p);

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
            Ast *index_expr = parse_expr(p, 0, true);
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

                Ast *arg = parse_expr(p, 0, true);
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
            Ast *right = parse_expr(p, precedence(curr.type), has_struct_init);
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
            a->token.span,
            p->f.source_code,
            "integer literal '%s' is too large",
            str
        );
    }

    // 检查负数字面量
    if(has_minus) {
        if(val > cast(i128)INTMAX_MAX) { 
            context()->reporter.report(
                ErrorLevel::Error,
                a->token.span,
                p->f.source_code,
                "integer literal '-%s' is too small",
                str
            );
        }

        val = -val;
    }

    
    // 检查溢出(类型)
    if(type_kind != Type_Undefined) {
        if(check_integer_overflow(val, easy_type(type_kind))) {
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
    
    Value value = make_value().set_is_runtime(false).set_value_state(ValueState::Solved);
    value.integer_value = val;
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
        if(check_float_overflow(val, easy_type(type_kind))) {
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
    
    Value value = make_value().set_is_runtime(false).set_value_state(ValueState::Solved);
    value.float_value = val;
    a->Constant.value = value;
    
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