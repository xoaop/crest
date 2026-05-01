#include "evaluator.hpp"

#include "context.hpp"
#include "value_ops.hpp"
#include "resolve_depend.hpp"

// 
// 1. Symbol Collect
//
void collect_top_level_symbols_in_package(Package *pkg, Array<Package> *all_packages) {
    
    // NOTE: 别忘了创建package scope
    pkg->package_scope = make_scope(&context()->global_blank_package.package_scope, ScopeType::Package, permanent_allocator());


    for(isize i = 0; i < pkg->ast_files.count; i++) {
        AstFile *ast_file = &pkg->ast_files[i];
        collect_top_level_symbols_in_file(ast_file, pkg, all_packages);
    }    
}    



void collect_top_level_symbols_in_file(AstFile *ast_file, Package *curr_pkg, Array<Package> *all_packages) {

    ast_file->file_scope = make_scope(&curr_pkg->package_scope, ScopeType::File, permanent_allocator());
    for(isize i = 0; i < ast_file->top_levels.count; i++) {
        Ast *top_level = ast_file->top_levels[i];

        Analyser analyser = make_analyser(ast_file, curr_pkg, all_packages);
        switch(top_level->type) {
        case AstType_ConstDecl: {
            collect_const_decl_symbol(top_level, make_analyser(ast_file, curr_pkg, all_packages));
        } break;    
        
        case AstType_VariableDecl: {
            collect_var_decl_symbol(top_level, make_analyser(ast_file, curr_pkg, all_packages));
        } break;    

        default:
            continue;
        }
    }    
}    


void collect_const_decl_symbol(Ast *const_decl_ast, Analyser analyser) {
    XP_ASSERT_DEFAULT(const_decl_ast->type == AstType_ConstDecl);

    Ast *value_ast = const_decl_ast->ConstDecl.value_ast;
    
    // 先检查有没有重复符号
    SymbolInfo *info = NULL;
    if(value_ast->type == AstType_Import) {
        // Import符号在文件作用域

        info = find_symbol_curr(analyser.current_scope, const_decl_ast->ConstDecl.name);
    } else {
        // 其他符号在包作用域

        info = find_symbol_until(ScopeType::Package, analyser.current_scope, const_decl_ast->ConstDecl.name);
    }    

    if(info != NULL) {
        context()->reporter.report_error(
            const_decl_ast->span, analyser.curr_ast_file->source_code,
            "symbol '{}' repeated definition",
            const_decl_ast->ConstDecl.name
        );    
        return;
    }    

    Value new_value = make_value()
    .set_is_runtime(false)
    .set_value_state(ValueState::Unsolved);
    new_value.val_ast = const_decl_ast;

    // if(value_ast->type == AstType_StructDeclValue) {
    //     TypeRef unfinished = type_type(unfinished_struct_type(analyser.pkg, const_decl_ast->ConstDecl.name));
    //     unfinished->self_type_info->struct_info.decl_ast = const_decl_ast;
    //     new_value.set_type(unfinished);
    // }


    SymbolInfo new_symbol = make_symbol(const_decl_ast->ConstDecl.name, new_value, analyser.pkg, analyser.curr_ast_file);
    if(value_ast->type == AstType_Import) {
        add_symbol_to_scope(analyser.current_scope, const_decl_ast->ConstDecl.name, new_symbol);
    } else {
        add_symbol_to_scope(&analyser.pkg->package_scope, const_decl_ast->ConstDecl.name, new_symbol);
    }

    return;
}




void collect_var_decl_symbol(Ast *var_decl_ast, Analyser analyser) {
    XP_ASSERT_DEFAULT(var_decl_ast->type == AstType_VariableDecl);

    SymbolInfo *info = find_symbol_until(ScopeType::Package, analyser.current_scope, var_decl_ast->VariableDecl.var_name);
    if(info != NULL) {
        context()->reporter.report_error(
            var_decl_ast->span, analyser.curr_ast_file->source_code,
            "symbol '{}' repeated definition",
            var_decl_ast->VariableDecl.var_name
        );        
        return;
    }

    Value new_value = make_value().set_is_runtime(true).set_value_state(ValueState::Unsolved);
    new_value.val_ast = var_decl_ast;
    SymbolInfo new_symbol = make_symbol(var_decl_ast->VariableDecl.var_name, new_value, analyser.pkg, analyser.curr_ast_file);
    add_symbol_to_scope(&analyser.pkg->package_scope, var_decl_ast->VariableDecl.var_name, new_symbol);

    return;
}    



//
// 2. Evaluate unsolved symbol
//


void eval_unsolved_in_symbol_table(SymbolInfo *unsolved_symbol, Analyser analyser) {
    XP_ASSERT_DEFAULT(unsolved_symbol->value.state == ValueState::Unsolved);
    XP_ASSERT_DEFAULT(unsolved_symbol->value.val_ast != NULL);

    Ast *ast = unsolved_symbol->value.val_ast;
    ast->ast_symbol = unsolved_symbol;


    if(ast->type == AstType_VariableDecl) {
        eval_unsolved_var_decl(unsolved_symbol, analyser);
    } else if(ast->type == AstType_ConstDecl) {
        eval_unsolved_const_decl(unsolved_symbol, analyser);
    } else {
        UNREACHABLE();
    }
    
}


void eval_unsolved_const_decl(SymbolInfo *unsolved_symbol, Analyser analyser) {
    XP_ASSERT_DEFAULT(unsolved_symbol->value.state == ValueState::Unsolved);
    XP_ASSERT_DEFAULT(unsolved_symbol->value.val_ast != NULL);
    XP_ASSERT_DEFAULT(unsolved_symbol->value.val_ast->type == AstType_ConstDecl);

    Ast *const_decl_ast = unsolved_symbol->value.val_ast;
    Ast *val_ast = const_decl_ast->ConstDecl.value_ast;
    Value *const_val = &unsolved_symbol->value; 

    const_val->set_value_state(ValueState::Solving);

    // TODO: 避免const_val->val_ast被新值覆盖, llvm ir生成要用
    switch(val_ast->type) {
    case AstType_StructDeclValue: 
        eval_struct_decl_value_in_symbol_table(unsolved_symbol, analyser);
        break;

    case AstType_FunctionDeclValue: 
        *const_val = eval_function_decl_value_only_type(val_ast, analyser);
        const_val->val_ast = const_decl_ast;
        break;

    case AstType_Import: 
        *const_val = eval_import_value(val_ast, analyser);
        break;
    
    case AstType_EnumDecl:
        *const_val = eval_enum_decl(val_ast, const_decl_ast->ConstDecl.name, analyser);
        const_val->val_ast = const_decl_ast;
        break;

    default:
        // clone_value是为了避免有在stage_allocator分配的值被释放, 比如array, struct等复杂类型的值
        Value resolved_val = resolve_comptime_expr(val_ast, analyser);
        *const_val = clone_value(resolved_val, permanent_allocator());
    }

}

void eval_unsolved_var_decl(SymbolInfo *unsolved_symbol, Analyser analyser) {
    XP_ASSERT_DEFAULT(unsolved_symbol->value.state == ValueState::Unsolved);
    XP_ASSERT_DEFAULT(unsolved_symbol->value.val_ast != NULL);
    XP_ASSERT_DEFAULT(unsolved_symbol->value.val_ast->type == AstType_VariableDecl);

    Ast *var_decl_ast = unsolved_symbol->value.val_ast;

    XP_TODO();
}



// 
// 3. evaluate values
// 


Value eval_import_value(Ast *import_ast, Analyser analyser) {

    // 查找被import的package
    xpOption<Package *> imported_package_opt = get_package_by_import(
        import_ast->Import.search_prefix,
        import_ast->Import.path,
        analyser.all_packages
    );

    if(imported_package_opt.is_none()) {
        context()->reporter.report_error(
            import_ast->span,
            analyser.curr_ast_file->source_code,
            "imported package '{}' not found",
            import_ast->Import.path
        );    
        return make_error_value();
    }    

    Package *imported_package = imported_package_opt.unwrap();
    Value import_value = make_comptime_sovled_val(package_type(imported_package));

    return import_value;
}    


TypeRef resolve_function_value_type(Ast *value, Analyser analyser) {
    Array<TypeRef> param_types = make_array<TypeRef>(stage_allocator());
    for(isize i = 0; i < value->FunctionDeclValue.params.count; i++) {
        Ast *param_ast = value->FunctionDeclValue.params[i];

        TypeRef param_type = nullptr;
        if(param_ast->ParamDecl.is_var_arg) {
            param_type = easy_type(Type_var_arg_c);
        } else {
            param_type = resolve_type(param_ast->ParamDecl.type_ast, analyser);
        }


        if(param_type == error_type()) {
            return error_type();
        }

        array_push_back(&param_types, param_type);
    }    
    // 解析返回值类型
    TypeRef return_type = resolve_type(value->FunctionDeclValue.return_type_ast, analyser);
    if(return_type == error_type()) {
        return error_type();
    }

    return function_type(param_types, return_type);
}    


// NOTE: 只解析了类型, 没有解析函数体, 为了避免解析到未解析的顶层符号
Value eval_function_decl_value_only_type(Ast *fn_val_ast, Analyser analyser) {
    Value func_value = make_comptime_sovled_val(
        resolve_function_value_type(fn_val_ast, analyser)
    );
    func_value.set_function_value(fn_val_ast, fn_val_ast->FunctionDeclValue.is_extern_c);

    return func_value;
}



void eval_struct_decl_value_in_symbol_table(SymbolInfo *struct_symbol_info, Analyser analyser) {
    XP_ASSERT_DEFAULT(struct_symbol_info->value.val_ast != NULL);
    XP_ASSERT_DEFAULT(struct_symbol_info->value.val_ast->type == AstType_ConstDecl);
    XP_ASSERT_DEFAULT(struct_symbol_info->value.val_ast->ConstDecl.value_ast->type == AstType_StructDeclValue);


    xpString name = struct_symbol_info->name;
    Ast *decl = struct_symbol_info->value.val_ast->ConstDecl.value_ast;

    
    
    struct_symbol_info->value.set_value_state(ValueState::Solving);

    TypeRef unfinished_type_type = type_type(unfinished_struct_type(struct_symbol_info->package, decl, name));
    struct_symbol_info->value.set_type(unfinished_type_type);

    Array<StructField> fields = make_array<StructField>(type_allocator());
    for(isize i = 0; i < decl->StructDeclValue.fields.count; i++) {
        Ast *field_ast = decl->StructDeclValue.fields[i];

        // 解析字段类型, 
        TypeRef field_type = resolve_type(field_ast->StructField.type_ast, analyser);

        if(field_type == error_type()) {
            struct_symbol_info->value.set_type(error_type());
            struct_symbol_info->value.set_value_state(ValueState::Error);
            break;
        }


        array_push_back(&fields, StructField{field_ast->StructField.name, field_type});
    }

    if(struct_symbol_info->value.has_error()) {
        return;
    }
    
    
    for(StructField field: fields) {
        TypeRef field_type = field.type;

        if(is_struct_type(field_type)) {
            // 结构体都应该在符号表有对应符号

            SymbolInfo *field_struct_sym = find_symbol_curr(
                &struct_symbol_info->package->package_scope, 
                field_type->type_name
            );
            
            if(field_struct_sym->value.state == ValueState::Solving) {
                context()->reporter.report_error(
                    decl->span, analyser.curr_ast_file->source_code,
                    "circular dependency detected in struct '{}'",
                    name
                );
                struct_symbol_info->value.set_type(error_type());
                struct_symbol_info->value.set_value_state(ValueState::Error);
                return;
            }
        }
        
    }
    
    finish_unfinish_struct_type(unfinished_type_type->self_type_info, fields);
    struct_symbol_info->value.set_value_state(ValueState::Solved);
    
}




// NOTE: 必须在resolve_expr和infer_expr_type之后调用, 因为需要保证符号正确解析和类型推导完成
ValueResult eval_comptime_expr(Ast *expr, Analyser analyser, bool is_runtime_expr) {

    switch(expr->type) {
        case AstType_Ident: {
            SymbolInfo *info = find_symbol_until_global(analyser.current_scope, expr->Ident.name);
            if(info == NULL) {
                return ValueResult::err(ValueErrorKind::ErrorValue);
            }

            Value value = info->value;

            if(value.has_error()) {
                return ValueResult::err(ValueErrorKind::ErrorValue);
            }

            if(value.is_runtime_value) {
                context()->reporter.report_error(
                    expr->span, analyser.curr_ast_file->source_code,
                    "cannot use runtime value '{}' in constant expression",
                    expr->Ident.name
                );
                return ValueResult::err(ValueErrorKind::UsingRuntimeValue);
            }

            if(expr->v_type != value.type) {
                // 可能untyped的常量需要根据上下文推导类型
                value.set_type(expr->v_type);
            }



            return ValueResult::ok(value);
        } break;

        case AstType_Constant: {
            return ValueResult::ok(expr->Constant.value);
        } break;

        case AstType_FieldAccess: {
            ValueResult parent_result = eval_comptime_expr(expr->FieldAccess.parent, analyser);
            if(parent_result.is_err()) {
                return parent_result;
            }
            Value parent_value = parent_result.as_ok();
            TypeRef parent_type = parent_value.type;

            if(parent_value.has_error()) {
                return ValueResult::err(ValueErrorKind::ErrorValue);
            }

            if(parent_value.is_runtime_value) {
                context()->reporter.report_error(
                    expr->span, analyser.curr_ast_file->source_code,
                    "cannot use runtime value in constant expression"
                );
                return ValueResult::err(ValueErrorKind::UsingRuntimeValue);
            }

            if(is_package_type(parent_type)) {
                Package *pkg = parent_value.get_package_value();
                SymbolInfo *field_sym = find_symbol_curr(&pkg->package_scope, expr->FieldAccess.field_name);
                
                if(field_sym == NULL) {
                    return ValueResult::err(ValueErrorKind::ErrorValue);
                }

                if(field_sym->value.has_error()) {
                    return ValueResult::err(ValueErrorKind::ErrorValue);
                }

                if(field_sym->value.is_runtime_value) {
                    context()->reporter.report_error(
                        expr->span, analyser.curr_ast_file->source_code,
                        "cannot use runtime value in constant expression"
                    );
                    return ValueResult::err(ValueErrorKind::UsingRuntimeValue);
                }

                return ValueResult::ok(field_sym->value);

            } else if(is_struct_type(parent_type)) {
                for(isize i = 0; i < parent_type->struct_info.struct_fields.count; i++) {
                    StructField field = parent_type->struct_info.struct_fields[i];
                    if(xp_string_equal(field.name, expr->FieldAccess.field_name)) {
                        Value field_value = parent_value.get_struct_fields()[i];
                        return ValueResult::ok(field_value);
                    }
                }
            }

            // UNREACHABLE();
            return ValueResult::err(ValueErrorKind::ErrorValue);
        } break;


        case AstType_BinaryExpr: {
            ValueResult left_result = eval_comptime_expr(expr->BinaryExpr.left, analyser);
            ValueResult right_result = eval_comptime_expr(expr->BinaryExpr.right, analyser);
            if(left_result.is_err()) {
                return left_result;
            }
            if(right_result.is_err()) {
                return right_result;
            }

            Value left = left_result.as_ok();
            Value right = right_result.as_ok();

            ValueResult compute_result = eval_binary_expr(left, right, expr->BinaryExpr.op);

            if(compute_result.is_err()) {
                context()->reporter.report_error(
                    expr->span, analyser.curr_ast_file->source_code,
                    "invalid constant expression"
                );
            }
            
            return compute_result;

        } break;

        case AstType_UnaryExpr: {
            ValueResult operand_result = eval_comptime_expr(expr->UnaryExpr.operand, analyser);

            if(operand_result.is_err()) {
                return operand_result;
            }
            Value operand = operand_result.as_ok();

            ValueResult compute_result = eval_unary_expr(operand, expr->UnaryExpr.op);

            if(compute_result.is_err()) {
                context()->reporter.report_error(
                    expr->span, analyser.curr_ast_file->source_code,
                    "invalid constant expression"
                );    
            }

            return compute_result;

        } break;

        case AstType_CastExpr: {
            ValueResult operand_result = eval_comptime_expr(expr->CastExpr.expr, analyser);

            if(operand_result.is_err()) {
                return operand_result;
            }
            Value operand = operand_result.as_ok();

            // 直接修改为target type就行, 因为type check检查了
            TypeRef target_type = expr->CastExpr.target_type;
            operand.set_type(target_type);

            return ValueResult::ok(operand);
        } break;

        case AstType_ArrayInitExpr: {
            Array<Value> element_values = make_array<Value>(stage_allocator());
            for(Ast *elem_expr_ast: expr->ArrayInitExpr.elements) {
                ValueResult elem_expr_result = eval_comptime_expr(elem_expr_ast, analyser);
                if(elem_expr_result.is_err()) {
                    return elem_expr_result;
                }
                array_push_back(&element_values, elem_expr_result.as_ok());
            }

            Value array_value = make_comptime_sovled_val(expr->v_type);
            array_value.set_array_value(element_values);

            return ValueResult::ok(array_value);
        } break;

        case AstType_StructInitExpr: {
            Array<Value> field_values = make_array<Value>(stage_allocator());
            for(isize i = 0; i < expr->StructInitExpr.field_inits.count; i++) {
                Ast *field_init_ast = expr->StructInitExpr.field_inits[i];
                ValueResult field_init_result = eval_comptime_expr(field_init_ast, analyser);
                if(field_init_result.is_err()) {
                    return field_init_result;
                }
                array_push_back(&field_values, field_init_result.as_ok());
            }

            Value struct_value = make_comptime_sovled_val(expr->v_type);
            struct_value.set_struct_value(field_values);

            return ValueResult::ok(struct_value);
        } break;

        case AstType_IndexExpr: {
            ValueResult indexed_value_result = eval_comptime_expr(expr->IndexExpr.array_var_expr, analyser);
            if(indexed_value_result.is_err()) {
                return indexed_value_result;
            }
            ValueResult index_result = eval_comptime_expr(expr->IndexExpr.index_expr, analyser);
            if(index_result.is_err()) {
                return index_result;
            }

            Value indexed_value = indexed_value_result.as_ok();
            Value index = index_result.as_ok();
            TypeRef indexed_type = indexed_value.type;


            // 检查下标越界
            i128 index_val = index.get_integer();
            if(is_array_type(indexed_type)) {
                if(!(index_val >= 0 && index_val < indexed_type->array_info.count)) {
                    context()->reporter.report_error(
                        expr->span, analyser.curr_ast_file->source_code,
                        "array index out of bounds"
                    );
                    return ValueResult::err(ValueErrorKind::Overflow);
                }
            } else if(is_string_struct_type(indexed_type)) {
                if(!(index_val >= 0 && index_val < indexed_value.get_string().length)) {
                    context()->reporter.report_error(
                        expr->span, analyser.curr_ast_file->source_code,
                        "string index out of bounds"
                    );
                    return ValueResult::err(ValueErrorKind::Overflow);
                }
            }

            // 获取元素值
            if(is_array_type(indexed_type)) {
                Value element_value = indexed_value.get_array_elements()[index_val];
                return ValueResult::ok(element_value);
            } else if(is_string_struct_type(indexed_type)) {
                // string类型的index表达式求值结果是一个u8的integer值
                char c = indexed_value.get_string()[index_val];
                Value char_value = make_comptime_sovled_val(easy_type(Type_u8));
                char_value.set_integer_value((u8)c);
                return ValueResult::ok(char_value);
            }


            // UNREACHABLE();
            return ValueResult::err(ValueErrorKind::ErrorValue);
        } break;

        case AstType_StringLiteralExpr: {
            Value string_value = make_comptime_sovled_val(expr->v_type);
            string_value.set_string_value(expr->StringLiteralExpr.str);

            return ValueResult::ok(string_value);
        } break;

        case AstType_ArrayType:
        case AstType_SliceType:
        case AstType_PointerType: {
            // 类型表达式, 直接返回类型值
            Value type_value = make_comptime_sovled_val(type_type(expr->v_type));
            return ValueResult::ok(type_value);
        } break;

        default: {
            context()->reporter.report_error(
                expr->span, analyser.curr_ast_file->source_code,
                "unsupported expression in constant expression evaluation"
            );
            return ValueResult::err(ValueErrorKind::ErrorValue);
        } break;

    }
}




Value eval_struct_decl_value(Ast *struct_decl_ast, xpString ident, Analyser analyser) {
    Array<StructField> fields = make_array<StructField>(stage_allocator());
    for(Ast *field_ast: struct_decl_ast->StructDeclValue.fields) {
        TypeRef field_type = resolve_type(field_ast->StructField.type_ast, analyser);

        if(field_type == error_type()) {
            return make_error_value();
        }

        array_push_back(&fields, StructField{field_ast->StructField.name, field_type});
    }

    TypeRef struct_typeref = struct_type(analyser.pkg, struct_decl_ast, ident, fields);

    Value struct_value = make_comptime_sovled_val(type_type(struct_typeref));
    return struct_value;
}




Value eval_enum_decl(Ast *enum_decl_ast, xpString ident, Analyser analyser) {
    XP_ASSERT_DEFAULT(enum_decl_ast->type == AstType_EnumDecl);



    TypeRef elem_type = nullptr;
    if(enum_decl_ast->EnumDecl.type_ast != nullptr) {
        elem_type = resolve_type(enum_decl_ast->EnumDecl.type_ast, analyser);
        if(elem_type == error_type()) {
            return make_error_value();
        }
    } else {
        // 如果没有指定枚举元素类型, 默认是i32
        elem_type = easy_type(Type_i32);
    }

    // elem_type must be ** Integer ** type
    if(!is_integer_type(elem_type)) {
        context()->reporter.report_error(
            enum_decl_ast->span, analyser.curr_ast_file->source_code,
            "enum element type must be an integer type"
        );
        return make_error_value();
    }





    TypeRef enum_type = unifinished_enum_type(enum_decl_ast, analyser.current_scope, ident, elem_type);



    // @Learn: 不能记录prev_enum_sym, 符号表扩容导致prev_enum_sym指针失效, 只能记录prev_enum_member_name, 每次都通过名字查找prev_enum_sym
    xpString prev_enum_member_name = xp_make_string_zero();
    SymbolInfo *curr_enum_sym = nullptr;
    for(isize i = 0; i < enum_decl_ast->EnumDecl.fields.count; i++) {
        Ast *const_decl_or_ident = enum_decl_ast->EnumDecl.fields[i];

        if(const_decl_or_ident->type == AstType_ConstDecl) {
            // 有初始化的枚举字段, 作为ConstDecl解析, 限制值只能是对应整型值
            // @TODO:  我想这个idea可以泛化成namespace, 若不加限制的话, 当然enum本身还是要限制的

            resolve_const_decl_local(const_decl_or_ident, analyser.set_current_scope(&enum_type->enum_info.enum_scope));

            curr_enum_sym = find_symbol_curr(&enum_type->enum_info.enum_scope, const_decl_or_ident->ConstDecl.name);
            
            // 检查枚举字段的值类型是否正确
            if(curr_enum_sym != nullptr) {
                // 只要是整数类型或无类型常量就行
                if(!is_integer_type(curr_enum_sym->value.type) && !is_untyped_type(curr_enum_sym->value.type)) {
                    context()->reporter.report_error(
                        const_decl_or_ident->span, analyser.curr_ast_file->source_code,
                        "enum field initializer must be an integer constant"
                    );
                    return make_error_value();
                }

                // 将枚举成员的类型从整数类型改为枚举类型本身
                curr_enum_sym->value.set_type(enum_type);
            }

        } else if(const_decl_or_ident->type == AstType_Ident) {
            if(i == 0) {
                // 第一个枚举成员如果没有初始化, 默认值是0

                Value first_enum_value = make_comptime_sovled_val(enum_type);
                first_enum_value.set_integer_value(0);
                first_enum_value.set_type(enum_type);

                SymbolInfo first_enum_field_sym = make_symbol(const_decl_or_ident->Ident.name, first_enum_value, analyser.pkg, analyser.curr_ast_file);
                add_symbol_to_scope(&enum_type->enum_info.enum_scope, const_decl_or_ident->Ident.name, first_enum_field_sym);


                curr_enum_sym = find_symbol_curr(&enum_type->enum_info.enum_scope, const_decl_or_ident->Ident.name);
            } else {
                // 没有初始化的枚举字段, 默认值是前一个枚举成员的值加1

                SymbolInfo *prev_enum_sym = find_symbol_curr(&enum_type->enum_info.enum_scope, prev_enum_member_name);
                XP_ASSERT_DEFAULT(prev_enum_sym != nullptr);



                // TODO: 溢出检查
                i128 new_enum_value_int = prev_enum_sym->value.get_enum_value() + 1;
                Value new_enum_value = make_comptime_sovled_val(enum_type);
                new_enum_value.set_integer_value(new_enum_value_int);
                new_enum_value.set_type(enum_type);
                

                SymbolInfo new_enum_field_sym = make_symbol(const_decl_or_ident->Ident.name, new_enum_value, analyser.pkg, analyser.curr_ast_file);
                add_symbol_to_scope(&enum_type->enum_info.enum_scope, const_decl_or_ident->Ident.name, new_enum_field_sym);

                curr_enum_sym = find_symbol_curr(&enum_type->enum_info.enum_scope, const_decl_or_ident->Ident.name);
            }

        } else {

            context()->reporter.report_error(
                const_decl_or_ident->span, analyser.curr_ast_file->source_code,
                "invalid enum field declaration"
            );
            return make_error_value();
        }


        prev_enum_member_name = curr_enum_sym->name;
    }

    Value enum_value = make_comptime_sovled_val(type_type(enum_type));
    return enum_value;
}
