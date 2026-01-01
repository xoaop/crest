#include "analyser.hpp"

#include "common.hpp"

#include "decl_collect.hpp"

#include "type_check.hpp"
#include "const_fold.hpp"

void push_symbol_table(Analyser *analyser) {
    SymbolTable table = make_symbol_table(xp_heap_allocator());
    array_push_back(&analyser->symbol_table_stack, table);
}

void pop_symbol_table(Analyser *analyser) {
    SymbolTable t = array_pop_back(&analyser->symbol_table_stack);
    free_symbol_table(&t);
}

bool at_global_scope(Analyser *analyser) {
    return analyser->symbol_table_stack.count == 1;
}


void semantic_analysis_ast_file(AstFile *ast_file) {

    defer(xp_free_all(temp_allocator()));

    Analyser analyser;
    analyser_init(&analyser, permanent_allocator());

    // TODO: More Stages
    declaration_collect_ast_file(ast_file, &analyser);
    resolve_ast_file(ast_file, &analyser);

    // analyser_free(&analyser);
    *symbol_table() = analyser.symbol_table_stack[0];
    return;
}


void resolve_function_decl(Ast *ast, Analyser *analyser);
void resolve_var_decl(Ast *var_decl_ast, Analyser *analyser);
void resolve_stmt(Ast *stmt_ast, Analyser *analyser);
void resolve_expr(Ast *expr_ast, Analyser *analyser);
void resolve_block(Ast *ast, Analyser *analyser);
void resolve_constant(Ast *constant, Analyser *analyser);
void try_constant_expr_folding(Ast *const_expr);



void resolve_ast_file(AstFile *ast_file, Analyser *analyser) {

    for(isize i = 0; i < ast_file->root.count; i++) {
        switch (ast_file->root[i]->type) {
        case AstType_Function:
            resolve_function_decl(ast_file->root[i], analyser);
            break;
        default:
            XP_ASSERT_DEFAULT(0);
            break;
        }
    }

    return;
}

void resolve_function_decl(Ast *ast, Analyser *analyser) {
    new_scope(analyser);

    analyser->curr_function = ast;
    defer(analyser->curr_function = NULL);

    for(isize i = 0; i < ast->Function.params.count; i++) {
        resolve_var_decl(ast->Function.params[i], analyser);
    }

    resolve_block(ast->Function.block, analyser);
}

void resolve_block(Ast *ast, Analyser *analyser) {
    if(ast->Block.is_function_body) {
        for(isize i = 0; i < ast->Block.statements.count; i++) {
            resolve_stmt(ast->Block.statements[i], analyser);
        }
        return;
    }


    new_scope(analyser);
    for(isize i = 0; i < ast->Block.statements.count; i++) {
        resolve_stmt(ast->Block.statements[i], analyser);
    }
}

void resolve_var_decl(Ast *var_decl_ast, Analyser *analyser) {
    SymbolInfo *existing = find_symbol(&analyser->symbol_table_stack, var_decl_ast->VariableDecl.var_name);
    if(existing != NULL) {
        // TODO(xoaop): 异常处理, 同一作用域变量重复声明
        printf("---------------------------------");
        print_ast(var_decl_ast);
        XP_ASSERT_MSG(0, "var decl repeat in the same scope");
    }

    
    if(var_decl_ast->v_type.kind == Type_void) {
        // TODO 变量定义为 void 类型错误处理
        XP_ASSERT_MSG(0, "variable can not be void type");
    }
    
    if(var_decl_ast->VariableDecl.expr != NULL) {
        resolve_expr(var_decl_ast->VariableDecl.expr, analyser);
        
        
        if(var_decl_ast->v_type.kind != Type_Undefined) {
            // TODO 有显示指定类型和初始化表达式的情况, 类型检查
            
            // !DEBUG
            infer_type_new(var_decl_ast->VariableDecl.expr, true, var_decl_ast->v_type, analyser);
            // infer_expr_type_without_target(var_decl_ast->VariableDecl.expr, analyser);
            // print_ast(var_decl_ast->VariableDecl.expr);


            if(!is_equal_type(var_decl_ast->v_type, var_decl_ast->VariableDecl.expr->v_type)) {
                // TODO 变量类型和初始化表达式类型不匹配错误处理
                XP_ASSERT_MSG(0, "variable decl type mismatch with init expr");
            }

        } else {
            // TODO 有初始化表达式, 无显示指定类型的情况, 类型推导
            // infer_expr_type(var_decl_ast->VariableDecl.expr, NULL, analyser);
            infer_type_new(var_decl_ast->VariableDecl.expr, false, {}, analyser);
            var_decl_ast->v_type = var_decl_ast->VariableDecl.expr->v_type;
        }

        // TODO 检查
        try_constant_expr_folding(var_decl_ast->VariableDecl.expr);
    } else {
        if(var_decl_ast->v_type.kind != Type_Undefined) {
            // TODO 有显示指定类型, 无初始化表达式的情况, 报错, 变量必须初始化
            // XP_ASSERT_DEFAULT(0);
        } else {
            // TODO 变量没有指定类型也没有初始化表达式, 无法推导类型错误处理
            XP_ASSERT_MSG(0, "variable type can not be undefined without init expr");
        }
    }



    SymbolInfo info = {
        .name = var_decl_ast->VariableDecl.var_name,
        .type = var_decl_ast->v_type,
    };
    add_symbol(&analyser->symbol_table_stack, var_decl_ast->VariableDecl.var_name, info);


    return;
}


void resolve_stmt(Ast *stmt_ast, Analyser *analyser) {

    switch (stmt_ast->type)
    {
    case AstType_VariableDecl: {
        resolve_var_decl(stmt_ast, analyser);
    } break;

    case AstType_Assignment: {
        resolve_expr(stmt_ast->Assignment.left_var_expr, analyser);
        resolve_expr(stmt_ast->Assignment.right_expr, analyser);

        Ast *left_expr = stmt_ast->Assignment.left_var_expr;
        Ast *right_expr = stmt_ast->Assignment.right_expr;
        
        infer_type_new(left_expr, false, {}, analyser);
        infer_type_new(right_expr, true, left_expr->v_type, analyser);

    } break;

    case AstType_IfStmt: {
        resolve_expr(stmt_ast->IfStmt.condition, analyser);

        Type condition_expr_type = make_type(Type_bool);
        infer_type_new(stmt_ast->IfStmt.condition, true, condition_expr_type, analyser);
        

        resolve_block(stmt_ast->IfStmt.then_block, analyser);

        if(stmt_ast->IfStmt.else_block != NULL)
            resolve_block(stmt_ast->IfStmt.else_block, analyser);
    } break;

    case AstType_ForStmt: {
        new_scope(analyser);

        array_push_back(&analyser->loop_ast_stack, stmt_ast);
        defer(array_pop_back(&analyser->loop_ast_stack));
        
        
        resolve_stmt(stmt_ast->ForStmt.init, analyser);
        resolve_expr(stmt_ast->ForStmt.condition, analyser);
        

        infer_type_new(stmt_ast->ForStmt.condition, true, make_type(Type_bool), analyser);

        resolve_stmt(stmt_ast->ForStmt.post, analyser);
        
        resolve_block(stmt_ast->ForStmt.body, analyser);
    } break;

    case AstType_ReturnStmt: {
        resolve_expr(stmt_ast->ReturnStmt.expr, analyser);
        
        // infer_expr_type(stmt_ast->ReturnStmt.expr, analyser->curr_function->v_type.function_info.return_type, analyser);
        infer_type_new(stmt_ast->ReturnStmt.expr, true, *analyser->curr_function->v_type.function_info.return_type, analyser);

        // // *NOTE: 这是唯一在tag_expr__type_and_const__by_children之外设置类型的地方, 因为return是stmt不是expr, 但它也需要携带类型信息
        // // *用于判断返回类型是否和函数返回类型匹配
        // stmt_ast->v_type = stmt_ast->ReturnStmt.expr->v_type;

    } break;

    case AstType_Block: {
        resolve_block(stmt_ast, analyser);
    } break;
    
    case AstType_Break: {
        // TODO 错误处理, break 必须在循环体内
        XP_ASSERT_DEFAULT(analyser->loop_ast_stack.count > 0);
    } break;
    case AstType_Continue: {
        // TODO 错误处理, continue 必须在循环体内
        XP_ASSERT_DEFAULT(analyser->loop_ast_stack.count > 0);
    } break;

    default: {

    } break;
    
    }
} 

void resolve_expr(Ast *expr_ast, Analyser *analyser) {
    if(expr_ast == NULL) {
        return;
    }

    switch (expr_ast->type)
    {
    case AstType_VarExpr: {
        SymbolInfo *entry;
        if(!(entry = find_symbol(&analyser->symbol_table_stack, expr_ast->VarExpr.name))) {
            // TODO: 错误处理, 使用未声明变量 
            XP_ASSERT_DEFAULT(0);
        }


    } break;
    case AstType_FunctionCallExpr: {
        
        for(isize i = 0; i < expr_ast->FunctionCallExpr.args.count; i++) {
            resolve_expr(expr_ast->FunctionCallExpr.args[i], analyser);
        }


        // TODO 函数类型检查
        SymbolInfo *info = find_symbol(&analyser->symbol_table_stack, expr_ast->FunctionCallExpr.name);

        // TODO 是否是函数
        XP_ASSERT_MSG(info != NULL && info->type.kind == Type_function, "function not exist");

        // TODO 参数个数是否匹配
        XP_ASSERT_MSG(info->type.function_info.param_types.count == expr_ast->FunctionCallExpr.args.count, "function arg count mismatch");

        // TODO 检查参数类型是否匹配
        for(isize i = 0; i < expr_ast->FunctionCallExpr.args.count; i++) {
            // infer_expr_type(expr_ast->FunctionCallExpr.args[i],&info->type.function_info.param_types[i], analyser);
            infer_type_new(expr_ast->FunctionCallExpr.args[i], true, info->type.function_info.param_types[i], analyser);
        }
        

    } break;

    case AstType_BinaryExpr: {
        resolve_expr(expr_ast->BinaryExpr.left, analyser);
        resolve_expr(expr_ast->BinaryExpr.right, analyser);

    } break;

    case AstType_UnaryExpr: {
        resolve_expr(expr_ast->UnaryExpr.operand, analyser);
    } break;

    case AstType_CastExpr: {
        resolve_expr(expr_ast->CastExpr.expr, analyser);
    } break;

    case AstType_Constant: {
        resolve_constant(expr_ast, analyser);
        break;
    }

    case AstType_Undefined: {
        XP_ASSERT_DEFAULT(0);
    } break;

    default:
        XP_ASSERT_DEFAULT(0);
    }

    void tag_expr_const_by_sons(Ast *expr_ast, Analyser *analyser);
    tag_expr_const_by_sons(expr_ast, analyser);
} 

void tag_expr_const_by_sons(Ast *expr, Analyser *analyser) {
    switch (expr->type)
    {

    case AstType_BinaryExpr: {
        if(expr->BinaryExpr.left->is_const_expr && expr->BinaryExpr.right->is_const_expr) {
            expr->is_const_expr = true;
        }
    } break;

    case AstType_UnaryExpr: {
        if(expr->UnaryExpr.operand->is_const_expr) {
            expr->is_const_expr = true;
        }
    } break;

    case AstType_CastExpr: {
        if(expr->CastExpr.expr->is_const_expr) {
            expr->is_const_expr = true;
        }
    } break;


    default: 
        break;
    }
}


// TODO 目前只有 i8-64, u8-64 常量, true,false, 
void resolve_constant(Ast *constant, Analyser *analyser) {
    Token token = constant->token;
    xpString val_str = token.token_str;

    // printf("Resolving constant: %s\n", val_str.c_str);


    if(token.type == TokenType::KW_true || token.type == TokenType::KW_false) {
        constant->v_type = make_type(Type_bool);
        constant->Constant.value = (token.type == TokenType::KW_true) ? 1 : 0;
        return;
    }

    // 解析
    // bool success = xp_str_to_integer(val_str.c_str, &constant->Constant.value);
    // XP_ASSERT_MSG(success, "Line %lld Column %lld: Invalid integer literal %s\n", token.line_index, token.column_index, val_str.c_str);
    
    // printf("Parsed constant value: ");
    // print_i128(constant->Constant.value);
    // printf("\n");
    

    // TODO 兼容 类型后缀 和 浮点数
    // 整型常量
    if(token.type_kind_of_number != Type_Undefined) {
        constant->v_type = make_type(token.type_kind_of_number);
    } else {
        if(token.type == TokenType::Integer) {
            constant->v_type = make_type(Type_literal);
        } else if(token.type == TokenType::Float) {
            constant->v_type = make_type(Type_literal_float);
        } 
    
    }
    constant->is_const_expr = true;

    return;
}













void analyser_init(Analyser *analyser, xpAllocator allocator) {
    analyser->loop_ast_stack = make_array<Ast *>(allocator);
    analyser->symbol_table_stack = make_array<SymbolTable>(allocator);

    // NOTE: 这是全局符号表
    array_push_back(&analyser->symbol_table_stack, make_symbol_table(permanent_allocator()));

    // 记录当前正在解析的函数
    analyser->curr_function = NULL;
}

void analyser_free(Analyser *analyser) {
    array_free(&analyser->loop_ast_stack);
    array_free(&analyser->symbol_table_stack);
}







// TODO Remove
void ast_visitor(Array<Ast *> ast_array, AstVisitorFunc visit_func[AstType_COUNT], Analyser *analyser) {
    XP_ASSERT_DEFAULT(visit_func != NULL);

    for(isize i = 0; i < ast_array.count; i++) {
        ast_visitor(ast_array[i], visit_func, analyser);
    }
}


// TODO Remove
void ast_visitor(Ast *ast, AstVisitorFunc visit_func[AstType_COUNT], Analyser *analyser) {
    XP_ASSERT_DEFAULT(visit_func != NULL);

    if(ast == NULL) {
        return;
    }

    if(visit_func[ast->type] != NULL) {
        visit_func[ast->type](ast, analyser);
    }

    switch (ast->type)
    {
        case AstType_Function: {
            new_scope(analyser);
            for(isize i = 0; i < ast->Function.params.count; i++) {
                ast_visitor(ast->Function.params[i], visit_func, analyser);
            }
            ast_visitor(ast->Function.block, visit_func, analyser);
        } break;

        case AstType_Block: {
            if(ast->Block.is_function_body) {
                for(isize i = 0; i < ast->Block.statements.count; i++) {
                    ast_visitor(ast->Block.statements[i], visit_func, analyser);
                }
                break;
            }

            new_scope(analyser);
            for(isize i = 0; i < ast->Block.statements.count; i++) {
                ast_visitor(ast->Block.statements[i], visit_func, analyser);
            }
        } break;

        case AstType_IfStmt: {
            ast_visitor(ast->IfStmt.condition, visit_func, analyser);
            ast_visitor(ast->IfStmt.then_block, visit_func, analyser);
            ast_visitor(ast->IfStmt.else_block, visit_func, analyser);
        } break;

        case AstType_ForStmt: {

            new_scope(analyser);

            array_push_back(&analyser->loop_ast_stack, ast);
            
            ast_visitor(ast->ForStmt.init, visit_func, analyser);
            ast_visitor(ast->ForStmt.condition, visit_func, analyser);
            ast_visitor(ast->ForStmt.post, visit_func, analyser);
            ast_visitor(ast->ForStmt.body, visit_func, analyser);

            array_pop_back(&analyser->loop_ast_stack);
        } break;

        case AstType_ReturnStmt: {
            ast_visitor(ast->ReturnStmt.expr, visit_func, analyser);
        } break;

        case AstType_VariableDecl: {
            ast_visitor(ast->VariableDecl.expr, visit_func, analyser);
        } break;

        case AstType_Assignment: {
            ast_visitor(ast->Assignment.left_var_expr, visit_func, analyser);
            ast_visitor(ast->Assignment.right_expr, visit_func, analyser);
        } break;

        case AstType_BinaryExpr: {
            ast_visitor(ast->BinaryExpr.left, visit_func, analyser);
            ast_visitor(ast->BinaryExpr.right, visit_func, analyser);
        } break;

        case AstType_UnaryExpr: {
            ast_visitor(ast->UnaryExpr.operand, visit_func, analyser);
        } break;

        case AstType_FunctionCallExpr: {
            for(isize i = 0; i < ast->FunctionCallExpr.args.count; i++) {
                ast_visitor(ast->FunctionCallExpr.args[i], visit_func, analyser);
            }
        } break;

        default: {
            // Nope
        } break;
    }
}

