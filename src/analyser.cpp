#include "analyser.hpp"

#include "common.hpp"

#include "decl_collect.hpp"



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
void infer_expr_type(Ast *expr, Type *target_type, Analyser *analyser);
void infer_expr_type_without_target(Ast *expr, Analyser *analyser);
// void check_type(Ast *expr, Type target_type, Analyser *analyser);
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
            infer_expr_type(var_decl_ast->VariableDecl.expr, &var_decl_ast->v_type, analyser);
            // infer_expr_type_without_target(var_decl_ast->VariableDecl.expr, analyser);
            print_ast(var_decl_ast->VariableDecl.expr);


            if(!is_equal_type(var_decl_ast->v_type, var_decl_ast->VariableDecl.expr->v_type)) {
                // TODO 变量类型和初始化表达式类型不匹配错误处理
                XP_ASSERT_MSG(0, "variable decl type mismatch with init expr");
            }

        } else {
            // TODO 有初始化表达式, 无显示指定类型的情况, 类型推导
            // infer_expr_type(var_decl_ast->VariableDecl.expr, NULL, analyser);
            infer_expr_type_without_target(var_decl_ast->VariableDecl.expr, analyser);
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
        
        infer_expr_type(left_expr, NULL, analyser);
        infer_expr_type(right_expr, &left_expr->v_type, analyser);

    } break;

    case AstType_IfStmt: {
        resolve_expr(stmt_ast->IfStmt.condition, analyser);

        Type condition_expr_type = make_type(Type_bool);
        infer_expr_type(stmt_ast->IfStmt.condition, &condition_expr_type, analyser);
        

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
        

        Type condition_expr_type = make_type(Type_bool);
        infer_expr_type(stmt_ast->ForStmt.condition, &condition_expr_type, analyser);


        resolve_stmt(stmt_ast->ForStmt.post, analyser);
        
        resolve_block(stmt_ast->ForStmt.body, analyser);
    } break;

    case AstType_ReturnStmt: {
        resolve_expr(stmt_ast->ReturnStmt.expr, analyser);
        
        infer_expr_type(stmt_ast->ReturnStmt.expr, analyser->curr_function->v_type.function_info.return_type, analyser);
        

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
            infer_expr_type(expr_ast->FunctionCallExpr.args[i],&info->type.function_info.param_types[i], analyser);
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

    void tag_expr_certain_type_and_const_by_sons(Ast *expr_ast, Analyser *analyser);
    tag_expr_certain_type_and_const_by_sons(expr_ast, analyser);
} 

void tag_expr_certain_type_and_const_by_sons(Ast *expr, Analyser *analyser) {
    switch (expr->type)
    {

    // TODO: 目前只有这两种expr是确定类型的(符号表查找), 后面可能会有更多
    // case AstType_VarExpr: {
    //     SymbolInfo *info = find_symbol(&analyser->symbol_table_stack, expr->VarExpr.name);
    //     expr->v_type = info->type;

    // } break;
    // case AstType_FunctionCallExpr: {
    //     SymbolInfo *info = find_symbol(&analyser->symbol_table_stack, expr->FunctionCallExpr.name);
    //     expr->v_type = *info->type.function_info.return_type;

    // } break;


    // TODO 以下表达式的类型都要从infer_type里推导出来
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
    bool success = xp_str_to_number(val_str.c_str, &constant->Constant.value);
    XP_ASSERT_MSG(success, "Line %lld Column %lld: Invalid integer literal %s\n", token.line_index, token.column_index, val_str.c_str);
    
    // printf("Parsed constant value: ");
    // print_i128(constant->Constant.value);
    // printf("\n");
    

    // 整型常量
    constant->v_type = make_type(Type_literal);
    constant->is_const_expr = true;

    return;
}



bool fit_in_type(i128 value, Type target_type) {
    bool fit = false;

    // i64 high = cast(i64)(value >> 64);
    // i64 low = cast(i64)(value & 0xFFFFFFFFFFFFFFFF);

    // printf("Checking if value fits in type: ");
    // print_i128(value);
    // print_type(target_type);
    // printf("\n");


    switch(target_type.kind) {
        case Type_i8:
            fit = (value >= INT8_MIN && value <= INT8_MAX);
            break;
        case Type_i32:
            fit = (value >= INT32_MIN && value <= INT32_MAX);
            break;
        case Type_i64:
            fit = (value >= INT64_MIN && value <= INT64_MAX);
            break;
        case Type_u8:
            fit = (value >= 0 && value <= UINT8_MAX);
            break;
        case Type_u32:
            fit = (value >= 0 && value <= UINT32_MAX);
            break;
        case Type_u64:
            fit = (value >= 0 && value <= UINT64_MAX);
            break;
        default:
            // TODO 
            print_type(target_type);
            printf("\n");
            XP_ASSERT_DEFAULT(0);
            break;
    }

    return fit;
}


Type get_compliable_constant_type(i128 value) {

    if(fit_in_type(value, make_type(Type_i32))) {
        return make_type(Type_i32);
    } else if(fit_in_type(value, make_type(Type_i64))) {
        return make_type(Type_i64);
    } else if(fit_in_type(value, make_type(Type_u64))) {
        return make_type(Type_u64);
    }


    // TODO 超出范围错误处理
    XP_ASSERT_DEFAULT(0);
    return make_type(Type_Undefined);
}

// TODO TEST
void infer_expr_type_without_target(Ast *expr, Analyser *analyser) {
    switch(expr->type)
    {
    case AstType_Constant: {
        if(is_equal_type(expr->v_type, make_type(Type_literal))) {
            expr->v_type = get_compliable_constant_type(expr->Constant.value);
        }
    } break;
    
    case AstType_BinaryExpr: {
        Type old_left_type = expr->BinaryExpr.left->v_type;
        Type old_right_type = expr->BinaryExpr.right->v_type;

        infer_expr_type_without_target(expr->BinaryExpr.left, analyser);
        infer_expr_type_without_target(expr->BinaryExpr.right, analyser);

        if(old_left_type.kind != Type_literal && old_right_type.kind != Type_literal) {
            // NOTE: 这个应该只要判断类型是否一样即可

        } else if((old_left_type.kind == Type_literal && old_right_type.kind != Type_literal) || (old_left_type.kind != Type_literal && old_right_type.kind == Type_literal)) {
            // 确定哪个是Type_literal, 哪个不是
            Ast *literal_expr = expr->BinaryExpr.left;
            Ast *not_literal_expr = expr->BinaryExpr.right;
            if(old_right_type.kind == Type_literal) {
                Ast *t = literal_expr;
                literal_expr = not_literal_expr;
                not_literal_expr = t;
            }

            
            if(!fit_in_type(literal_expr->Constant.value, not_literal_expr->v_type)) {
                // TODO 常量溢出错误处理
                XP_ASSERT_MSG(0, "constant overflowed");
            }
            literal_expr->v_type = not_literal_expr->v_type;

        } else if(old_left_type.kind == Type_literal && old_right_type.kind == Type_literal) {
            // TODO 把两个转化为较大类型的
        }


        if(!is_equal_type(expr->BinaryExpr.left->v_type, expr->BinaryExpr.right->v_type)) {
            XP_ASSERT_DEFAULT(0);
        }

        if(is_equal_type(expr->BinaryExpr.left->v_type, make_type(Type_bool)) || 
           is_equal_type(expr->BinaryExpr.right->v_type, make_type(Type_bool))) {
            XP_ASSERT_DEFAULT(is_operator_for_bool(expr->BinaryExpr.op));
        }


        if(is_return_bool_operator(expr->BinaryExpr.op)) {
            expr->v_type = make_type(Type_bool);
        } else {
            expr->v_type = expr->BinaryExpr.left->v_type;
        }

    } break;
    
    case AstType_UnaryExpr: {
        Type old_type = expr->UnaryExpr.operand->v_type;

        infer_expr_type_without_target(expr->UnaryExpr.operand, analyser);
        expr->v_type = expr->UnaryExpr.operand->v_type;

        if(expr->UnaryExpr.op == TokenType::Minus) {
            if(is_equal_type(old_type, make_type(Type_literal))) {
                
                i128 val = expr->UnaryExpr.operand->Constant.value;
                i128 neg_val = -val;
                
                // 重新推导类型
                Type inferred_type = get_compliable_constant_type(neg_val);
                
                if(fit_in_type(neg_val, inferred_type)) {
                    expr->v_type = inferred_type;
                    expr->UnaryExpr.operand->v_type = inferred_type;
                } else {
                    // TODO 常量溢出错误处理
                    XP_ASSERT_MSG(0, "constant overflowed");
                }
    
            }
        } else if(expr->UnaryExpr.op == TokenType::Exclamation) {
            // 逻辑非运算符, 输入和输出都是bool类型

            Type bool_type = make_type(Type_bool);
            infer_expr_type_without_target(expr->UnaryExpr.operand, analyser);
            if(!is_equal_type(expr->UnaryExpr.operand->v_type, bool_type)) {
                // TODO 逻辑非运算符操作数类型错误处理
                XP_ASSERT_MSG(0, "logical not operator operand must be bool type");
            }

            expr->v_type = bool_type;
        }

    } break;


    case AstType_CastExpr: {
        expr->v_type = expr->CastExpr.target_type;

        infer_expr_type_without_target(expr->CastExpr.expr, analyser);

        if((!is_equal_type(expr->CastExpr.expr->v_type, make_type(Type_bool)) && is_equal_type(expr->CastExpr.target_type, make_type(Type_bool))) || (is_equal_type(expr->CastExpr.expr->v_type, make_type(Type_bool)) && !is_equal_type(expr->CastExpr.target_type, make_type(Type_bool)))) {
            XP_ASSERT_DEFAULT(0);
        }

    } break;

    // 别的expr类型确定了, 不用推导
    case AstType_VarExpr: {
        SymbolInfo *info = find_symbol(&analyser->symbol_table_stack, expr->VarExpr.name);
        expr->v_type = info->type;
    } break;
    case AstType_FunctionCallExpr: {
        SymbolInfo *info = find_symbol(&analyser->symbol_table_stack, expr->FunctionCallExpr.name);
        expr->v_type = *info->type.function_info.return_type;
    } break;
    

    default:
        XP_ASSERT_DEFAULT(0);
        break;
    }
}

// TODO 完成
// !TRY: 尝试不用Type *target_type
void check_type(Ast *expr, Type target_type, Analyser *analyser) {
    switch(expr->type)
    {
    case AstType_Constant: {
        if(expr->v_type.kind == Type_literal) {
            // 防止Type_literal转化成Type_bool
            if(is_equal_type(target_type, make_type(Type_bool))) {
                XP_ASSERT_MSG(0, "Type_literal can not be Type_bool");
            }

            // 尝试把Type_literal值放入target_type
            if(!fit_in_type(expr->Constant.value, target_type)) {
                // TODO 常量溢出错误处理
                XP_ASSERT_MSG(0, "constant overflowed");
            } 
        }
    } break;
    
    case AstType_BinaryExpr: {

        TokenType op = expr->BinaryExpr.op;
        Ast *left_expr = expr->BinaryExpr.left;
        Ast *right_expr = expr->BinaryExpr.right;

        Type left_type = expr->BinaryExpr.left->v_type;
        Type right_type = expr->BinaryExpr.right->v_type;


        if((left_type.kind != Type_literal && right_type.kind == Type_literal) || 
            (right_type.kind != Type_literal && left_type.kind == Type_literal)) {
            // *DONE!

            // 确定哪个是Type_literal, 哪个不是
            Ast *literal_expr = left_expr;
            Ast *not_literal_expr = right_expr;
            if(not_literal_expr->v_type.kind == Type_literal) {
                Ast *t = literal_expr;
                literal_expr = not_literal_expr;
                not_literal_expr = t;
            }


            check_type(not_literal_expr, target_type, analyser);

            if(!fit_in_type(literal_expr->Constant.value, not_literal_expr->v_type)) {
                // TODO 常量溢出错误处理
                XP_ASSERT_MSG(0, "constant overflowed");
            }

            literal_expr->v_type = not_literal_expr->v_type;
            expr->v_type = not_literal_expr->v_type;

        } else if(left_type.kind == Type_literal && right_type.kind == Type_literal) {
            // *DONE!

            if(is_equal_type(target_type, make_type(Type_bool))) {

                // TODO 换成自动转化为大的类型
                expr->BinaryExpr.left->v_type = get_compliable_constant_type(expr->BinaryExpr.left->Constant.value);
                expr->BinaryExpr.right->v_type = get_compliable_constant_type(expr->BinaryExpr.right->Constant.value);
                if(!is_equal_type(expr->BinaryExpr.left->v_type, expr->BinaryExpr.right->v_type)) {
                    XP_ASSERT_MSG(0, "binary expr type mismatch in infer_expr_type");
                }
            } else {
                check_type(expr->BinaryExpr.left, target_type, analyser);
                check_type(expr->BinaryExpr.right, target_type, analyser);

                // TODO 换成自动转化为大的类型
                if(!is_equal_type(expr->BinaryExpr.left->v_type, expr->BinaryExpr.right->v_type)) {
                    XP_ASSERT_MSG(0, "binary expr type mismatch in infer_expr_type");
                }
            }

            expr->v_type = expr->BinaryExpr.left->v_type;

        } else if(left_type.kind != Type_literal && right_type.kind != Type_literal) {



            check_type(expr->BinaryExpr.left, target_type, analyser);
            check_type(expr->BinaryExpr.right, target_type, analyser);

            if(!is_equal_type(expr->BinaryExpr.left->v_type, expr->BinaryExpr.right->v_type)) {
                XP_ASSERT_MSG(0, "binary expr type mismatch in infer_expr_type");
            }

            expr->v_type = expr->BinaryExpr.left->v_type;
        }

        
        if(expr->BinaryExpr.left->v_type.kind == Type_bool) {
            // TODO 逻辑运算符只能操作bool类型
            XP_ASSERT_MSG(is_operator_for_bool(expr->BinaryExpr.op), "bool type operands can only be used with logical operators and double/! equal operator");
        }


        // TODO TEST
        if(is_return_bool_operator(expr->BinaryExpr.op)) {
            expr->v_type = make_type(Type_bool);
        }
    } break;

    case AstType_UnaryExpr: {

    } break;

    // *无视target_type, 因为CastExpr的子表达式的类型无论如何都是要被转化为CastExpr.target_type
    case AstType_CastExpr: {
        infer_expr_type_without_target(expr->CastExpr.expr, analyser);

        // 限制cast能力
        if(!is_equal_type(expr->CastExpr.expr->v_type, make_type(Type_bool)) && is_equal_type(expr->CastExpr.target_type, make_type(Type_bool))) {
            // TODO 错误处理 非bool类型转换为bool类型
            XP_ASSERT_MSG(0, "only bool type can be casted to bool type");
        }

        expr->v_type = expr->CastExpr.target_type;
    } break;

    // *无视target_type
    case AstType_VarExpr: {
        SymbolInfo *info = find_symbol(&analyser->symbol_table_stack, expr->VarExpr.name);
        expr->v_type = info->type;
    } break;
    case AstType_FunctionCallExpr: {
        SymbolInfo *info = find_symbol(&analyser->symbol_table_stack, expr->FunctionCallExpr.name);
        expr->v_type = *info->type.function_info.return_type;
    } break;

    default:
        break;
    }

}

// 这个函数用来尝试递归地让表达式迎合target_type
// TODO 重构, 现在这个函数太乱
void infer_expr_type(Ast *expr, Type *target_type, Analyser *analyser) {
    static i32 recurisve_depth = 0;

    recurisve_depth++;

    switch(expr->type) {

        /* AstType_Constant目前有两种类型
            1. Type_literial: 未确定的整数， 可能是i8-64, u8-64 类型 它们需要推导
            2. Type_bool: true、false 不需要推导
            3. Type_i/u 8-64: 这里不应该出现
        */ 
        case AstType_Constant: {

            /*
                1. 对于Type_literal, 如果有target_type, 尝试把其放入target_type
                Type_literial可以是i8-64, u8-64 类型, 不可以是bool
            
            */
            if(expr->v_type.kind == Type_literal) {
                if(target_type != NULL) {

                    // 1. Type_literal不可以是bool
                    if(is_equal_type(*target_type, make_type(Type_bool))) {
                        XP_ASSERT_MSG(0, "Type_literal can not be Type_bool");
                    }
                    // 2. Type_literal要放的进target_type
                    if(!fit_in_type(expr->Constant.value, *target_type)) {
                        // TODO 常量溢出错误处理
                        XP_ASSERT_MSG(0, "constant overflowed");
                    } 

                    expr->v_type = *target_type;

                } else {
                    expr->v_type = get_compliable_constant_type(expr->Constant.value);
                }
            } else if(expr->v_type.kind == Type_bool) {
                // 无需处理
            } else {
                XP_ASSERT_MSG(0, "Constant must be Type_literal or Type_bool in infer_expr_type");
            }
        } break;


        // 如果两个子表达式都是Type_literal 不能无视target_type
        case AstType_BinaryExpr: {


            Type left_type = expr->BinaryExpr.left->v_type;
            Type right_type = expr->BinaryExpr.right->v_type;


            if((left_type.kind != Type_literal && right_type.kind == Type_literal) || 
               (right_type.kind != Type_literal && left_type.kind == Type_literal)) {

                // 确定哪个是Type_literal, 哪个不是
                Ast *literal_expr = expr->BinaryExpr.left;
                Ast *not_literal_expr = expr->BinaryExpr.right;
                if(not_literal_expr->v_type.kind == Type_literal) {
                    Ast *t = literal_expr;
                    literal_expr = not_literal_expr;
                    not_literal_expr = t;
                }

                infer_expr_type(not_literal_expr, target_type, analyser);

                if(!fit_in_type(literal_expr->Constant.value, not_literal_expr->v_type)) {
                    // TODO 常量溢出错误处理
                    XP_ASSERT_MSG(0, "constant overflowed");
                }
                literal_expr->v_type = not_literal_expr->v_type;
                expr->v_type = not_literal_expr->v_type;

            } else if(left_type.kind == Type_literal && right_type.kind == Type_literal) {
                if(target_type == NULL) {
                    infer_expr_type(expr->BinaryExpr.left, target_type, analyser);
                    infer_expr_type(expr->BinaryExpr.right, target_type, analyser);

                    // TODO 换成自动转化为大的类型
                    if(!is_equal_type(expr->BinaryExpr.left->v_type, expr->BinaryExpr.right->v_type)) {
                        XP_ASSERT_MSG(0, "binary expr type mismatch in infer_expr_type");
                    }
                } else if(target_type != NULL) {

                    
                    if(is_equal_type(*target_type, make_type(Type_bool))) {
                        
                        // TODO 换成自动转化为大的类型
                        expr->BinaryExpr.left->v_type = get_compliable_constant_type(expr->BinaryExpr.left->Constant.value);
                        expr->BinaryExpr.right->v_type = get_compliable_constant_type(expr->BinaryExpr.right->Constant.value);
                        if(!is_equal_type(expr->BinaryExpr.left->v_type, expr->BinaryExpr.right->v_type)) {
                            XP_ASSERT_MSG(0, "binary expr type mismatch in infer_expr_type");
                        }
                    } else {
                        infer_expr_type(expr->BinaryExpr.left, target_type, analyser);
                        infer_expr_type(expr->BinaryExpr.right, target_type, analyser);

                        // TODO 换成自动转化为大的类型
                        if(!is_equal_type(expr->BinaryExpr.left->v_type, expr->BinaryExpr.right->v_type)) {
                            XP_ASSERT_MSG(0, "binary expr type mismatch in infer_expr_type");
                        }
                    }

                }
                expr->v_type = expr->BinaryExpr.left->v_type;

            } else if(left_type.kind != Type_literal && right_type.kind != Type_literal) {
                infer_expr_type(expr->BinaryExpr.left, target_type, analyser);
                infer_expr_type(expr->BinaryExpr.right, target_type, analyser);

                if(!is_equal_type(expr->BinaryExpr.left->v_type, expr->BinaryExpr.right->v_type)) {
                    print_ast(expr);
                    XP_ASSERT_MSG(0, "binary expr type mismatch in infer_expr_type");
                }

                expr->v_type = expr->BinaryExpr.left->v_type;
            }

            
            if(expr->BinaryExpr.left->v_type.kind == Type_bool) {
                // TODO 逻辑运算符只能操作bool类型
                XP_ASSERT_MSG(is_operator_for_bool(expr->BinaryExpr.op), "bool type operands can only be used with logical operators and double/! equal operator");
            }


            // TODO TEST
            if(is_return_bool_operator(expr->BinaryExpr.op)) {
                expr->v_type = make_type(Type_bool);
            }
            
        } break;

        case AstType_UnaryExpr: {
            
            
            // NOTE: 注意单独处理如 -128(i8) 这种情况,  不能简单地把 128 作为常量处理, 不然会被推导为高一级别的类型
            // TODO 特殊处理负号
            if(expr->UnaryExpr.op == TokenType::Minus) {
                if(expr->UnaryExpr.operand->type == AstType_Constant) {
                    if(expr->UnaryExpr.operand->v_type.kind == Type_literal) {

                        i128 val = expr->UnaryExpr.operand->Constant.value;
                        i128 neg_val = -val;
                        
                        // 重新推导类型
                        Type inferred_type;
                        if(target_type != NULL) {
                            inferred_type = *target_type;
                        } else {
                            inferred_type = get_compliable_constant_type(neg_val);
                        }
                        
                        if(fit_in_type(neg_val, inferred_type)) {
                            expr->v_type = inferred_type;
                            expr->UnaryExpr.operand->Constant.value = val;
                            expr->UnaryExpr.operand->v_type = inferred_type;
                        } else {
                            // TODO 常量溢出错误处理
                            XP_ASSERT_MSG(0, "constant overflowed");
                        }
    
                        return;
                    } 

                }

                infer_expr_type(expr->UnaryExpr.operand, target_type, analyser);
                if(expr->UnaryExpr.operand->v_type.kind == Type_bool) {
                    // TODO 负号不能操作bool类型错误处理
                    XP_ASSERT_MSG(0, "minus operator can not operate on bool type");
                }
                expr->v_type = expr->UnaryExpr.operand->v_type;
                return;

            } else if(expr->UnaryExpr.op == TokenType::Exclamation) {
                // 逻辑非运算符, 输入和输出都是bool类型

                Type bool_type = make_type(Type_bool);
                // infer_expr_type(expr->UnaryExpr.operand, NULL, analyser);
                infer_expr_type_without_target(expr->UnaryExpr.operand, analyser);
                if(!is_equal_type(expr->UnaryExpr.operand->v_type, bool_type)) {
                    // TODO 逻辑非运算符操作数类型错误处理
                    XP_ASSERT_MSG(0, "logical not operator operand must be bool type");
                }

                expr->v_type = bool_type;
                return;
            }
            
            // 普遍推导操作数类型
            infer_expr_type(expr->UnaryExpr.operand, target_type, analyser);

            expr->v_type = expr->UnaryExpr.operand->v_type;
        } break;
        

        // *无视target_type, 因为CastExpr的子表达式的类型无论如何都是要被转化为CastExpr.target_type
        case AstType_CastExpr: {
            // 强制转换类型
                        
            // infer_expr_type(expr->CastExpr.expr, &expr->CastExpr.target_type, analyser);
            infer_expr_type_without_target(expr->CastExpr.expr, analyser);

            if(!is_equal_type(expr->CastExpr.expr->v_type, make_type(Type_bool)) && is_equal_type(expr->CastExpr.target_type, make_type(Type_bool))) {
                // TODO 错误处理 非bool类型转换为bool类型
                XP_ASSERT_MSG(0, "only bool type can be casted to bool type");
            }

            expr->v_type = expr->CastExpr.target_type;
        } break;

        // *无视target_type
        // 别的expr类型确定了, 不用推导
        case AstType_VarExpr: {
            SymbolInfo *info = find_symbol(&analyser->symbol_table_stack, expr->VarExpr.name);
            expr->v_type = info->type;
        } break;
        case AstType_FunctionCallExpr: {
            SymbolInfo *info = find_symbol(&analyser->symbol_table_stack, expr->FunctionCallExpr.name);
            expr->v_type = *info->type.function_info.return_type;
        } break;


        default: {
            XP_ASSERT_DEFAULT(0);
        } break;
    }



    if(recurisve_depth == 1 && target_type != NULL) {
        // 顶层调用时, 尝试让expr迎合target_type
        if(!is_equal_type(expr->v_type, *target_type)) {
            // TODO 类型不匹配错误处理
            XP_ASSERT_MSG(0, "expr type mismatch with target type in infer_expr_type");
        }
    }
    
    recurisve_depth--;
}



i128 operation_constant_ast(TokenType op_type, Ast *left_c, Ast *right_c) {
    XP_ASSERT_DEFAULT(left_c->type == AstType_Constant);
    XP_ASSERT_DEFAULT(right_c == NULL || right_c->type == AstType_Constant);

    i128 left = left_c->Constant.value;
    i128 right;
    
    i128 result = 0;


    if(right_c == NULL) {
        goto unary_operation;
    } else {
        goto binary_operation;
    }

binary_operation:
    right = right_c->Constant.value;
    switch(op_type) {
        case TokenType::Add: {
            result = left + right;
        } break;
        case TokenType::Minus: {
            result = left - right;
        } break;
        case TokenType::Star: {
            result = left * right;
        } break;
        case TokenType::ForwardSlash: {
            XP_ASSERT_DEFAULT(right != 0);
            result = left / right;
        } break;
        case TokenType::Percent: {
            XP_ASSERT_DEFAULT(right != 0);
            result = left % right;
        } break;

        case TokenType::DoubleEqual: {
            result = left == right;
        } break;
        case TokenType::ExclamationEqual: {
            result = left != right;
        } break;
        case TokenType::GreaterThan: {
            result = left > right;
        } break;
        case TokenType::GreaterEqual: {
            result = left >= right;
        } break;
        case TokenType::LessThan: {
            result = left < right;
        } break;
        case TokenType::LessEqual: {
            result = left <= right;
        } break;
        case TokenType::DoubleAnd: {
            result = left && right;
        } break;
        case TokenType::DoubleOr: {
            result = left || right;
        } break;

        default: {
            XP_ASSERT_DEFAULT(0);
        } break;
    }
    goto unsigned_cast;


unary_operation:

    switch(op_type)
    {
    case TokenType::Minus: {
        result = -left;
    } break;
    case TokenType::Exclamation: {
        result = !left;
    } break;
    
    default:
        break;
    }
    goto unsigned_cast;


unsigned_cast:

    switch (left_c->v_type.kind)
    {
    case Type_u8:
        result = cast(u8) result;
        break;
    case Type_u32:
        result = cast(u32) result;
        break;
    case Type_u64:
        result = cast(u64) result;
        break;
    default:
        break;
    }

    return result;
}

bool check_const_overflow(Type type, i128 result) {
    TypeKind type_kind = type.kind;

    bool overflowed = false;
    switch(type_kind) {
        case Type_i8: 
            overflowed = (result < INT8_MIN || result > INT8_MAX);
            break;
        case Type_i32: 
            overflowed = (result < INT32_MIN || result > INT32_MAX);
            break;
        case Type_i64:
            overflowed = (result < INT64_MIN || result > INT64_MAX);
            break;
        default:
            break;
    }

    return overflowed;
}




void try_constant_expr_folding(Ast *const_expr) {
    if(const_expr->is_const_expr == false) {
        return;
    }


    switch(const_expr->type)
    {
    case AstType_BinaryExpr: 
    case AstType_UnaryExpr:
    {
        TokenType op_type;
        Ast *left;
        Ast *right;
        if(const_expr->type == AstType_BinaryExpr) {
            try_constant_expr_folding(const_expr->BinaryExpr.left);
            try_constant_expr_folding(const_expr->BinaryExpr.right);

            op_type = const_expr->BinaryExpr.op;
            left = const_expr->BinaryExpr.left;
            right = const_expr->BinaryExpr.right;
        } else {
            try_constant_expr_folding(const_expr->UnaryExpr.operand);

            op_type = const_expr->UnaryExpr.op;
            left = const_expr->UnaryExpr.operand;
            right = NULL;
        }


        i128 result = operation_constant_ast(op_type, left, right);
        if(check_const_overflow(left->v_type, result)) {
            // TODO 常量溢出错误处理
            XP_ASSERT_MSG(0, "constant overflowed");
        }

        Ast constant = ast_make(AstType_Constant);
        constant.is_const_expr = true;
        constant.Constant.value = result;
        constant.v_type = left->v_type;

        *const_expr = constant;
    } break;

    case AstType_CastExpr: {
        try_constant_expr_folding(const_expr->CastExpr.expr);

        i128 result = const_expr->CastExpr.expr->Constant.value;
        Type target_type = const_expr->CastExpr.target_type;

        // 类型转换
        switch (target_type.kind)
        {
        case Type_i8:
            result = cast(i8) result;
            break;
        case Type_i32:
            result = cast(i32) result;
            break;
        case Type_i64:
            result = cast(i64) result;
            break;
        case Type_u8:
            result = cast(u8) result;
            break;
        case Type_u32:
            result = cast(u32) result;
            break;
        case Type_u64:
            result = cast(u64) result;
            break;
        default:
            break;
        }

        if(check_const_overflow(target_type, result)) {
            // TODO 常量溢出错误处理
            XP_ASSERT_MSG(0, "constant overflowed");
        }

        Ast constant = ast_make(AstType_Constant);
        constant.is_const_expr = true;
        constant.Constant.value = result;
        constant.v_type = target_type;

        *const_expr = constant;
    } break;

    
    default:
        break;
    }
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

