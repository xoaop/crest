#include "analyser.hpp"

#include "common.hpp"

#include "decl_collect.hpp"

#include "type_check.hpp"
#include "const_fold.hpp"

#include "error_msg.hpp"



void resolve_top_level(Ast *ast, Analyser *analyser);
void resolve_function_decl(Ast *ast, Analyser *analyser);
void resolve_struct_decl(Ast *ast, Analyser *analyser);
void resolve_var_decl(Ast *var_decl_ast, Analyser *analyser);
void resolve_stmt(Ast *stmt_ast, Analyser *analyser);
void resolve_expr(Ast *expr_ast, Analyser *analyser);
void resolve_block(Ast *ast, Analyser *analyser);
void resolve_constant(Ast *constant, Analyser *analyser);
TypeRef resolve_type(Ast *type_ast, Analyser *analyser);
void try_constant_expr_folding(Ast *const_expr);
void tag_expr_const_by_sons(Ast *expr, Analyser *analyser);
void tag_untyped_expr(Ast *expr, Analyser *analyser);
bool may_fall_through(Ast *ast);




void push_symbol_table(Analyser *analyser) {
    SymbolTable table = make_symbol_table(xp_heap_allocator());
    array_push_back(&analyser->symbol_table_stack, table);
}

void pop_symbol_table(Analyser *analyser) {
    SymbolTable t = array_pop_back(&analyser->symbol_table_stack);
    free_symbol_table(&t);
}

// bool at_global_scope(Analyser *analyser) {
    //     return analyser->symbol_table_stack.count == 1;
// }




void semantic_analysis_ast_file(AstFile *ast_file) {
    defer(xp_free_all(stage_allocator()));


    Analyser analyser;
    analyser_init(&analyser, permanent_allocator());


    // 收集所有结构体声明、函数声明、变量声明(目前只有局部变量)
    AllDecls all_decls = declaration_collect_ast_file(ast_file, &analyser, stage_allocator());


    // 进行结构体声明field类型解析
    for(isize i = 0; i < all_decls.struct_decls.count; i++) {
        resolve_struct_decl(all_decls.struct_decls[i], &analyser);
    }


    // 进行函数声明的类型解析, 修正符号表中函数类型
    Array<TypeRef> param_types = make_array<TypeRef>(stage_allocator());
    for(isize i = 0; i < all_decls.function_decls.count; i++) {
        Ast *fn_decl_ast = all_decls.function_decls[i];
        SymbolInfo *fn_info = find_symbol(symbol_table(), fn_decl_ast->Function.name);

        // 解析参数类型
        array_clear(&param_types);
        for(isize j = 0; j < fn_decl_ast->Function.params.count; j++) {
            Ast *param_ast = fn_decl_ast->Function.params[j];
            TypeRef param_type = resolve_type(param_ast->VariableDecl.type_ast, &analyser);
            array_push_back(&param_types, param_type);
        }
        // 解析返回值类型
        TypeRef return_type = resolve_type(fn_decl_ast->Function.return_type_ast, &analyser);
        // 构造函数类型
        TypeRef fn_type = function_type(param_types, return_type);
        // 修正符号表中函数类型
        fn_info->type = fn_type;

        // TODO 考虑是否移除这个, 但目前感觉也方便
        // 修正函数声明ast的类型
        fn_decl_ast->v_type = fn_type;
    }


    // TODO TEST
    // print_ast(ast_file->root);


    resolve_ast_file(ast_file, &analyser);

    // analyser_free(&analyser);
    // *symbol_table() = analyser.symbol_table_stack[0];
    return;
}





void resolve_ast_file(AstFile *ast_file, Analyser *analyser) {

    for(isize i = 0; i < ast_file->root.count; i++) {
        resolve_top_level(ast_file->root[i], analyser);
    }

    return;
}


void resolve_top_level(Ast *ast, Analyser *analyser) {
    switch (ast->type) {
    case AstType_Function:
        resolve_function_decl(ast, analyser);
        break;

    case AstType_StructDecl:
        // resolve_struct_decl(ast, analyser);
        break;
        
    default:
        XP_ASSERT_DEFAULT(0);
        break;
    }
}


void resolve_function_decl(Ast *ast, Analyser *analyser) {
    new_scope(analyser);

    analyser->curr_function = ast;
    defer(analyser->curr_function = NULL);

    for(isize i = 0; i < ast->Function.params.count; i++) {
        resolve_var_decl(ast->Function.params[i], analyser);
    }
    
    if(ast->Function.block != NULL) {
        

        resolve_block(ast->Function.block, analyser);


        if(may_fall_through(ast->Function.block)) {
            if(analyser->curr_function->v_type->function_info.return_type->kind != Type_void) {
                // TODO 非void函数漏写return错误处理
                error_msg(&ast->token, "non-void function may fall through without return");
                XP_ASSERT_MSG(0, "non-void function may fall through without return");
            }
        }
    }
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

    // TODO CHECK
    if(var_decl_ast->VariableDecl.type_ast != NULL) {
        var_decl_ast->v_type = resolve_type(var_decl_ast->VariableDecl.type_ast, analyser);
    } else {
        var_decl_ast->v_type = undefined_type();
    }


    if(var_decl_ast->v_type == easy_type(Type_void)) {
        // TODO 变量定义为 void 类型错误处理
        XP_ASSERT_MSG(0, "variable can not be void type");
    }
    
    if(var_decl_ast->VariableDecl.expr != NULL) {
        resolve_expr(var_decl_ast->VariableDecl.expr, analyser);
        
        
        if(var_decl_ast->v_type != undefined_type()) {
            // TODO 有显示指定类型和初始化表达式的情况, 类型检查
            
            // !DEBUG
            infer_expr_type(var_decl_ast->VariableDecl.expr, true, var_decl_ast->v_type, analyser);

        } else {
            // TODO 有初始化表达式, 无显示指定类型的情况, 类型推导
            // infer_expr_type(var_decl_ast->VariableDecl.expr, NULL, analyser);
            infer_expr_type(var_decl_ast->VariableDecl.expr, false, {}, analyser);
            var_decl_ast->v_type = var_decl_ast->VariableDecl.expr->v_type;
        }

        // TODO 检查
        try_constant_expr_folding(var_decl_ast->VariableDecl.expr);
    } else {

        // if(var_decl_ast->v_type != undefined_type()) {
        //     // TODO 有显示指定类型, 无初始化表达式的情况, 报错, 变量必须初始化
        //     // XP_ASSERT_DEFAULT(0);
        // } else {
        //     // TODO 变量没有指定类型也没有初始化表达式, 无法推导类型错误处理
        //     XP_ASSERT_MSG(0, "variable type can not be undefined without init expr");
        // }
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
        
        infer_expr_type(left_expr, false, {}, analyser);
        infer_expr_type(right_expr, true, left_expr->v_type, analyser);


        if(left_expr->is_lvalue == false) {
            // TODO 错误处理, 赋值表达式左表达式不是左值
            error_msg(&left_expr->token, "left side of assignment is not a lvalue");
            XP_ASSERT_DEFAULT(0);
        }

    } break;

    case AstType_IfStmt: {
        resolve_expr(stmt_ast->IfStmt.condition, analyser);

        TypeRef condition_expr_type = easy_type(Type_bool);
        infer_expr_type(stmt_ast->IfStmt.condition, true, condition_expr_type, analyser);
        

        resolve_block(stmt_ast->IfStmt.then_block, analyser);

        if(stmt_ast->IfStmt.else_block != NULL)
            resolve_block(stmt_ast->IfStmt.else_block, analyser);
    } break;

    case AstType_ForStmt: {
        new_scope(analyser);

        array_push_back(&analyser->loop_ast_stack, stmt_ast);
        defer(array_pop_back(&analyser->loop_ast_stack));
        
        if(stmt_ast->ForStmt.init != NULL)
            resolve_stmt(stmt_ast->ForStmt.init, analyser);

        if(stmt_ast->ForStmt.condition != NULL)
            resolve_expr(stmt_ast->ForStmt.condition, analyser);
        

        infer_expr_type(stmt_ast->ForStmt.condition, true, easy_type(Type_bool), analyser);

        if(stmt_ast->ForStmt.post != NULL)
            resolve_stmt(stmt_ast->ForStmt.post, analyser);
        
        resolve_block(stmt_ast->ForStmt.body, analyser);
    } break;

    case AstType_ReturnStmt: {
        if(stmt_ast->ReturnStmt.expr != NULL) {
            resolve_expr(stmt_ast->ReturnStmt.expr, analyser);

            if(analyser->curr_function->v_type->function_info.return_type == easy_type(Type_void)) {
                // TODO 错误处理, return 语句不应有返回值 在void函数中
                error_msg(&stmt_ast->token, "return statement should not have expression in void function");
                XP_ASSERT_DEFAULT(0);
            }

            infer_expr_type(stmt_ast->ReturnStmt.expr, true, analyser->curr_function->v_type->function_info.return_type, analyser);
        } else {
            if(analyser->curr_function->v_type->function_info.return_type != easy_type(Type_void)) {
                // TODO 错误处理, return 语句缺少返回值 在非void函数中
                error_msg(&stmt_ast->token, "return statement missing expression in non-void function");
                XP_ASSERT_DEFAULT(0);
            }
        }

        
    } break;

    case AstType_Block: {
        resolve_block(stmt_ast, analyser);
    } break;
    
    case AstType_Break: {
        // TODO 错误处理, break 必须在循环体内
        if(analyser->loop_ast_stack.count == 0) {
            error_msg(&stmt_ast->token, "break statement not within loop");
        }
        XP_ASSERT_DEFAULT(analyser->loop_ast_stack.count > 0);
    } break;
    case AstType_Continue: {
        // TODO 错误处理, continue 必须在循环体内
        if(analyser->loop_ast_stack.count == 0) {
            error_msg(&stmt_ast->token, "continue statement not within loop");
        }
        XP_ASSERT_DEFAULT(analyser->loop_ast_stack.count > 0);
    } break;

    case AstType_FunctionCallExpr: {
        resolve_expr(stmt_ast, analyser);
    } break;

    default: {
        XP_ASSERT_DEFAULT(0);
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
        SymbolInfo *info = find_symbol(symbol_table(), expr_ast->FunctionCallExpr.name);

        // TODO 是否是函数
        XP_ASSERT_MSG(info != NULL && info->type->kind == Type_function, "function not exist");

        // TODO 参数个数是否匹配
        XP_ASSERT_MSG(info->type->function_info.param_types.count == expr_ast->FunctionCallExpr.args.count, "function arg count mismatch");

        // TODO 检查参数类型是否匹配
        for(isize i = 0; i < expr_ast->FunctionCallExpr.args.count; i++) {
            infer_expr_type(expr_ast->FunctionCallExpr.args[i], true, info->type->function_info.param_types[i], analyser);
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

        // TODO CHECK
        expr_ast->v_type = resolve_type(expr_ast->CastExpr.target_type_ast, analyser);

        // TODO 移除
        expr_ast->CastExpr.target_type = expr_ast->v_type;
    } break;

    case AstType_Constant: {
        resolve_constant(expr_ast, analyser);
    } break;

    case AstType_StructInitExpr: {
        for(isize i = 0; i < expr_ast->StructInitExpr.field_inits.count; i++) {
            resolve_expr(expr_ast->StructInitExpr.field_inits[i], analyser);
        }
    } break;

    case AstType_FieldAccessExpr: {
        resolve_expr(expr_ast->FieldAccessExpr.parent_expr, analyser);
    } break;

    case AstType_ArrayInitExpr: {
        for(isize i = 0; i < expr_ast->ArrayInitExpr.elements.count; i++) {
            resolve_expr(expr_ast->ArrayInitExpr.elements[i], analyser);
        }
    } break;

    case AstType_IndexExpr: {
        resolve_expr(expr_ast->IndexExpr.array_var_expr, analyser);
        resolve_expr(expr_ast->IndexExpr.index_expr, analyser);
    } break;


    case AstType_StringLiteralExpr: {
        // TODO 字符串字面量类型处理

        
    } break;

    case AstType_Undefined: {
        XP_ASSERT_DEFAULT(0);
    } break;


    default:
        XP_ASSERT_DEFAULT(0);
    }

    tag_expr_const_by_sons(expr_ast, analyser);
    tag_untyped_expr(expr_ast, analyser);
}




void resolve_constant(Ast *constant, Analyser *analyser) {
    Token token = constant->token;
    xpString val_str = token.token_str;

    // printf("Resolving constant: %s\n", val_str.c_str);


    if(token.type == TokenType::KW_true || token.type == TokenType::KW_false) {
        constant->v_type = easy_type(Type_bool);
        constant->Constant.value = (token.type == TokenType::KW_true) ? 1 : 0;
        return;
    }

    if(token.type == TokenType::KW_null) {
        constant->v_type = pointer_type(easy_type(Type_void));
        constant->Constant.value = 0;
        constant->is_null = true;
        return;
    }

    // 解析
    // bool success = xp_str_to_integer(val_str.c_str, &constant->Constant.value);
    // XP_ASSERT_MSG(success, "Line %lld Column %lld: Invalid integer literal %s\n", token.line_index, token.column_index, val_str.c_str);
    
    // printf("Parsed constant value: ");
    // print_i128(constant->Constant.value);
    // printf("\n");
    

    // 整型常量
    
    if(token.type_kind_of_number != Type_Undefined) {
        constant->v_type = easy_type(token.type_kind_of_number);
    } else {

        // 无类型后缀的数字字面量, 先标记为untyped类型, 等待类型推导
        if(token.type == TokenType::Integer) {
            constant->v_type = easy_type(Type_untyped_int);
        } else if(token.type == TokenType::Float) {
            constant->v_type = easy_type(Type_untyped_float);
        } 
    
    }

    return;
}



TypeRef resolve_type(Ast *type_ast, Analyser *analyser) {
    switch(type_ast->type) {
        case AstType_EasyType: {
            TypeKind kind = type_ast->EasyType.kind;
            TypeRef type_ref = easy_type(kind);
            return type_ref;
        } break;

        // 目前就是结构体类型
        case AstType_IdentType: {
            xpString type_name = type_ast->IdentType.name;
            SymbolInfo *type_info = find_symbol(symbol_table(), type_name);
            if(type_info == NULL) {
                error_msg(&type_ast->token, "unknown type '%s'", type_name.c_str);
                XP_ASSERT_DEFAULT(0);
            }
            return type_info->type;
        } break;

        case AstType_PointerType: {
            TypeRef pointed_type = resolve_type(type_ast->PointerType.pointed_type_ast, analyser);
            TypeRef type_ref = pointer_type(pointed_type);
            return type_ref;
        } break;

        // TODO 数组
        case AstType_ArrayType: {
            TypeRef element_type = resolve_type(type_ast->ArrayType.element_type_ast, analyser);
            Ast *count_expr = type_ast->ArrayType.count_expr;


            resolve_expr(count_expr, analyser);
            infer_expr_type(count_expr, false, NULL, analyser);


            if(!count_expr->is_const_expr || !is_integer_type(count_expr->v_type)) {
                error_msg(&count_expr->token, "array size expression must be a constant integer expression");
                XP_ASSERT_DEFAULT(0);
            }
            try_constant_expr_folding(count_expr);
            if(count_expr->type != AstType_Constant) {
                error_msg(&count_expr->token, "array size expression must be a constant integer expression");
                XP_ASSERT_DEFAULT(0);
            }

            i128 count = count_expr->Constant.value;
            if(count <= 0 || count > INTPTR_MAX) { // TODO 换掉这个最大值宏
                error_msg(&count_expr->token, "array size must be a positive integer and less than max of isize");
                XP_ASSERT_DEFAULT(0);
            }

            TypeRef type_ref = array_type(element_type, cast(usize)count);
            return type_ref;
        } break;

        
        // Slice应该是一种内置结构体
        case AstType_SliceType: {
            TypeRef elem_type = resolve_type(type_ast->SliceType.element_type_ast, analyser);

            TypeRef slice_type = slice_type_as_struct(elem_type);

            return slice_type;
        } break;

        default: {
            XP_ASSERT_DEFAULT(0);
        }
    }
}


void resolve_struct_decl(Ast *ast, Analyser *analyser) {
    XP_ASSERT_DEFAULT(ast->type == AstType_StructDecl);

    SymbolInfo *struct_type_info = find_symbol(symbol_table(), ast->StructDecl.name);

    Array<StructField> field_types = make_array<StructField>(stage_allocator());
    for(isize i = 0; i < ast->StructDecl.fields.count; i++) {
        Ast *field_ast = ast->StructDecl.fields[i];

        // 解析字段类型, 
        TypeRef field_type = resolve_type(field_ast->StructField.type_ast, analyser);
        array_push_back(&field_types, StructField{field_ast->StructField.name, field_type});

        if(field_type == struct_type_info->type) {
            
            // 字段类型不能和结构体本身相同 错误处理
            error_msg(&field_ast->token, "struct field type can not be the same as struct type itself");
            XP_ASSERT_DEFAULT(0);
        }

    }

    update_uncertain_to_struct(struct_type_info->type, field_types);
}





void tag_expr_const_by_sons(Ast *expr, Analyser *analyser) {
    switch (expr->type)
    {

    case AstType_Constant: {
        expr->is_const_expr = true;
    } break;


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

void tag_untyped_expr(Ast *expr, Analyser *analyser) {
    switch(expr->type) {
        case AstType_BinaryExpr: {
            if(expr->BinaryExpr.left->v_type == easy_type(Type_untyped_int) &&
               expr->BinaryExpr.right->v_type == easy_type(Type_untyped_int)) {
                expr->v_type = easy_type(Type_untyped_int);
            } else if(expr->BinaryExpr.left->v_type == easy_type(Type_untyped_float) &&
               expr->BinaryExpr.right->v_type == easy_type(Type_untyped_float)) {
                expr->v_type = easy_type(Type_untyped_float);
            } else if((expr->BinaryExpr.left->v_type ==  easy_type(Type_untyped_int) && expr->BinaryExpr.right->v_type == easy_type(Type_untyped_float)) || 
                      (expr->BinaryExpr.left->v_type == easy_type(Type_untyped_float) && expr->BinaryExpr.right->v_type == easy_type(Type_untyped_int))) {
                
                // TODO 目前先报错, 后续可以考虑类型提升等规则
                error_msg(&expr->token, "type mismatch in binary expression with untyped operands");
                XP_ASSERT_DEFAULT(0);
            }

        } break;

        case AstType_UnaryExpr: {
            if(expr->UnaryExpr.operand->v_type == easy_type(Type_untyped_int)) {
                expr->v_type = easy_type(Type_untyped_int);
            }

            if(expr->UnaryExpr.operand->v_type == easy_type(Type_untyped_float)) {
                expr->v_type = easy_type(Type_untyped_float);
            }
        } break;
        

        default:
            break;
    }
}


bool may_fall_through(Ast *ast) {
    
    switch(ast->type)
    {
    case AstType_ReturnStmt:
        return false;
        break;
    case AstType_Break:
        return true;
        break;
    case AstType_Continue:
        return false;
        break;
    case AstType_Block: {
        Ast *last_stmt = NULL;
        for(isize i = 0; i < ast->Block.statements.count; i++) {
            if(!may_fall_through(ast->Block.statements[i])) {
                return false;
            }
        }

        return true;
    } break;
    case AstType_IfStmt: {
        Ast *then_block = ast->IfStmt.then_block;
        Ast *else_block = ast->IfStmt.else_block;

        if(else_block == NULL) {
            return true;
        }

        return may_fall_through(then_block) || may_fall_through(else_block);
    } break;

    case AstType_ForStmt:
        return may_fall_through(ast->ForStmt.body);    
        break;
    default:
        break;
    }

    return true;
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

