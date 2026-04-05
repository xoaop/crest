#include "analyser.hpp"

#include "resolve_depend.hpp"

#include "common.hpp"

#include "context.hpp"

#include "type_check.hpp"
#include "const_fold.hpp"

#include "error_msg.hpp"

#include "path.hpp"
#include "value_ops.hpp"



Analyser make_analyser(AstFile *curr_ast_file, Package *pkg, Array<Package> *all_packages) {
    Analyser analyser = {};
    analyser.all_packages = all_packages;
    analyser.pkg = pkg;
    analyser.current_scope = &curr_ast_file->file_scope;
    analyser.curr_ast_file = curr_ast_file;
    analyser.curr_func = NULL;

    return analyser;
}

Analyser make_analyser(Scope *curr_scope, Package *pkg) {
    // without all_packages
    Analyser analyser = {};
    analyser.current_scope = curr_scope;
    analyser.pkg = pkg;

    return analyser;
}

Analyser make_analyser(Scope *curr_scope, AstFile *file, Package *pkg) {
    Analyser analyser = {};
    analyser.current_scope = curr_scope;
    analyser.curr_ast_file = file;
    analyser.pkg = pkg;

    return analyser;
}


Analyser Analyser::set_pkg(Package *pkg) {
    this->pkg = pkg;
    return *this;
}

Analyser Analyser::set_current_scope(Scope *current_scope) {
    this->current_scope = current_scope;
    return *this;
}

Analyser Analyser::set_curr_ast_file(AstFile *curr_ast_file) {
    this->curr_ast_file = curr_ast_file;
    return *this;
}

Analyser Analyser::set_curr_func(Ast *curr_func) {
    this->curr_func = curr_func;
    return *this;
}



void bind_ast_with_scope(Ast *ast, Scope *scope) {
    xp_hash_map_insert(&context()->ast_scope_map, ast, scope);
}



void eval_unsolved_in_symbol_table(SymbolInfo *unsolved_symbol, Analyser analyser);



//
// Analyser
//

void resolve_ast_file(AstFile *ast_file, Analyser analyser);
void resolve_top_stmt(Ast *ast, Analyser analyser);
void resolve_function_decl(Ast *ast, Analyser analyser);
TypeRef resolve_function_value_type(Ast *value, Analyser analyser);

void resolve_var_decl(Ast *var_decl_ast, Analyser analyser);
void resolve_local_stmt(Ast *stmt_ast, Analyser analyser);
void resolve_expr(Ast *expr_ast, Analyser analyser);
void resolve_block(Ast *ast, Analyser analyser, bool need_new_scope);
TypeRef resolve_type(Ast *type_ast, Analyser analyser);
SymbolInfo *resolve_ident(Ast *ident_ast, Analyser analyser);
SymbolInfo * resolve_field_access(Ast *field_access_ast, Analyser analyser);
bool may_fall_through(Ast *ast);



//
// Symbol Binding
//



void collect_top_level_symbols_in_package(Package *pkg, Array<Package>* all_packages);


xpString get_ident_or_fieldaccess_string(Ast *ast, xpAllocator allocator) {

    if(ast->type == AstType_Ident) {
        return ast->Ident.name;
    } else if(ast->type == AstType_FieldAccess) {
        xpString parent_str = get_ident_or_fieldaccess_string(ast->FieldAccess.parent, allocator);
        xp_string_append(&parent_str, xp_string_c("."));
        xp_string_append(&parent_str, ast->FieldAccess.field_name);
        return parent_str;
    }

    return xp_string_c("<invalid ident or field access>");
}




//
// 0. 总流程
//

void collect_symbols_in_all_packages(Array<Package> *all_packages);
void collect_top_level_symbols_in_file(AstFile *ast_file, Package *curr_pkg, Array<Package> *all_packages);
void eval_top_level_types(AstFile *ast_file, Analyser analyser);
void sema_analysis_package(Package *pkg, Array<Package> *all_packages);

void sema_analysis_all_packages(Array<Package> *all_packages) {
    defer(xp_arena_allocator_clear(stage_allocator()));

    collect_symbols_in_all_packages(all_packages);

    for(isize i = 0; i < all_packages->count; i++) {
        Package *pkg = &(*all_packages)[i];
        sema_analysis_package(pkg, all_packages);
    }
}

void sema_analysis_package(Package *pkg, Array<Package> *all_packages) {
    for(isize j = 0; j < pkg->ast_files.count; j++) {
        AstFile *ast_file = &pkg->ast_files[j];
        
        resolve_ast_file(ast_file, make_analyser(ast_file, pkg, all_packages));
    }
}

// 
// 1. Symbol Collect
//


void collect_top_level_symbols_in_package(Package *pkg, Array<Package> *all_packages);
void collect_top_level_symbols_in_file(AstFile *ast_file, Package *curr_pkg, Array<Package> *all_packages);
void collect_const_decl_symbol(Ast *const_decl_ast, Analyser analyser);
void collect_var_decl_symbol(Ast *var_decl_ast, Analyser analyser);




void collect_symbols_in_all_packages(Array<Package> *all_packages) {

    
    // TODO 添加全局符号, 如string类型
    {
        xpString string_struct_name = xp_string_c("string");

        Array<StructField> fields = make_array<StructField>(temp_allocator());
        defer(xp_arena_allocator_clear(temp_allocator()));

        array_push_back(&fields, StructField { 
            xp_string_c("data"), 
            pointer_type(easy_type(Type_u8)) 
        });    
        array_push_back(&fields, StructField { 
            xp_string_c("count"), 
            easy_type(Type_i64) // TODO 考虑改成isize
        });    

        TypeRef string_struct_typeref = struct_type(&context()->global_blank_package, string_struct_name, fields);
        TypeRef string_type = type_type(string_struct_typeref);

        // 符号表
        SymbolInfo string_struct_symbol = make_symbol(
            string_struct_name, 
            make_comptime_sovled_val(string_type),
            &context()->global_blank_package,
            NULL
        );
        add_symbol_to_scope(&context()->global_blank_package.package_scope, string_struct_name, string_struct_symbol);
    }



    for(isize i = 0; i < all_packages->count; i++) {
        collect_top_level_symbols_in_package(&(*all_packages)[i], all_packages);
    }    
}    



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
            "symbol '%s' repeated definition",
            const_decl_ast->ConstDecl.name.c_str
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
            "symbol '%s' repeated definition",
            var_decl_ast->VariableDecl.var_name.c_str
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
// Evaluate top level types and constants
//

Value eval_import_value(Ast *import_ast, Analyser analyser);
Value eval_function_decl_value(Ast *fn_val_ast, Analyser analyser);
void eval_unsolved_var_decl(SymbolInfo *unsolved_symbol, Analyser analyser);
Value resolve_comptime_expr(Ast *expr_ast, Analyser analyser);
void eval_unsolved_const_decl(SymbolInfo *unsolved_symbol, Analyser analyser);
void eval_struct_decl_value_in_symbol_table(SymbolInfo *struct_symbol_info, Analyser analyser);



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
        *const_val = eval_function_decl_value(val_ast, analyser);
        const_val->val_ast = const_decl_ast;
        break;

    case AstType_Import: 
        *const_val = eval_import_value(val_ast, analyser);
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
            "imported package '%s' not found",
            import_ast->Import.path.c_str
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
        TypeRef param_type = resolve_type(param_ast->VariableDecl.type_ast, analyser);

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



Value eval_function_decl_value(Ast *fn_val_ast, Analyser analyser) {
    Value func_value = make_comptime_sovled_val(
        resolve_function_value_type(fn_val_ast, analyser)
    );
    func_value.function_value.is_extern_c = fn_val_ast->FunctionDeclValue.is_extern_c;
    func_value.function_value.function_ast = fn_val_ast;

    return func_value;
}



void eval_struct_decl_value_in_symbol_table(SymbolInfo *struct_symbol_info, Analyser analyser) {
    XP_ASSERT_DEFAULT(struct_symbol_info->value.val_ast != NULL);
    XP_ASSERT_DEFAULT(struct_symbol_info->value.val_ast->type == AstType_ConstDecl);
    XP_ASSERT_DEFAULT(struct_symbol_info->value.val_ast->ConstDecl.value_ast->type == AstType_StructDeclValue);


    xpString name = struct_symbol_info->name;
    Ast *decl = struct_symbol_info->value.val_ast->ConstDecl.value_ast;

    
    
    struct_symbol_info->value.set_value_state(ValueState::Solving);

    TypeRef unfinished_type_type = type_type(unfinished_struct_type(struct_symbol_info->package, name));
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
                    "circular dependency detected in struct '%s'",
                    name.c_str
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



//
// Evaluate constexpr values
//

ValueResult eval_comptime_expr(Ast *expr, Analyser analyser, bool for_var_expr = false);

Value resolve_comptime_expr(Ast *expr, Analyser analyser) {
    resolve_expr(expr, analyser);
    infer_expr_type(expr, false, NULL, analyser, true);

    if(expr->v_type == error_type()) {
        return make_error_value();
    }

    ValueResult result = eval_comptime_expr(expr, analyser);

    if(result.is_err()) {
        return make_error_value();
    }
    
    return result.as_ok();
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
                    "cannot use runtime value '%s' in constant expression",
                    expr->Ident.name.c_str
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
                Package *pkg = get_package_value(parent_value);
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
                        Value field_value = parent_value.struct_field_values[i];
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
            array_value.array_element_values = element_values;

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
            struct_value.struct_field_values = field_values;

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
            i128 index_val = index.integer_value;
            if(is_array_type(indexed_type)) {
                if(!(index_val >= 0 && index_val < indexed_type->array_info.count)) {
                    context()->reporter.report_error(
                        expr->span, analyser.curr_ast_file->source_code,
                        "array index out of bounds"
                    );
                    return ValueResult::err(ValueErrorKind::Overflow);
                }
            } else if(is_string_struct_type(indexed_type)) {
                if(!(index_val >= 0 && index_val < indexed_value.string_value.length)) {
                    context()->reporter.report_error(
                        expr->span, analyser.curr_ast_file->source_code,
                        "string index out of bounds"
                    );
                    return ValueResult::err(ValueErrorKind::Overflow);
                }
            }

            // 获取元素值
            if(is_array_type(indexed_type)) {
                Value element_value = indexed_value.array_element_values[index_val];
                return ValueResult::ok(element_value);
            } else if(is_string_struct_type(indexed_type)) {
                // string类型的index表达式求值结果是一个u8的integer值
                char c = indexed_value.string_value[index_val];
                Value char_value = make_comptime_sovled_val(easy_type(Type_u8));
                char_value.integer_value = c;
                return ValueResult::ok(char_value);
            }


            // UNREACHABLE();
            return ValueResult::err(ValueErrorKind::ErrorValue);
        } break;

        case AstType_StringLiteralExpr: {
            Value string_value = make_comptime_sovled_val(expr->v_type);
            string_value.string_value = expr->StringLiteralExpr.str;

            return ValueResult::ok(string_value);
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









//
// Analyser
//







Analyser new_scope(Analyser old_state, ScopeType type, Ast *related_ast) {
    Scope *new_scope = alloc_scope(
        old_state.current_scope,
        type,
        permanent_allocator()
    );    
    
    Analyser new_state = old_state;
    new_state.current_scope = new_scope;
    
    bind_ast_with_scope(related_ast, new_scope);
    
    return new_state;
}    






void resolve_ast_file(AstFile *ast_file, Analyser analyser) {

    analyser.curr_ast_file = ast_file;

    for(isize i = 0; i < ast_file->top_levels.count; i++) {
        resolve_top_stmt(ast_file->top_levels[i], analyser);
    }

    return;
}


void resolve_top_stmt(Ast *ast, Analyser analyser) {

    switch (ast->type) {

    case AstType_ConstDecl: {
        SymbolInfo *const_info = find_symbol_until_global(analyser.current_scope, ast->ConstDecl.name);
        if(const_info == NULL) {
            break;
        }

        if(const_info->value.state == ValueState::Unsolved) {
            eval_unsolved_in_symbol_table(const_info, analyser);
        }

        if(const_info->value.has_error()) {
            break;
        }

        // 函数(原始定义, 不是重命名)要单独解析block
        if(ast->ConstDecl.value_ast->type == AstType_FunctionDeclValue) {
            ast->v_type = const_info->value.type;
            resolve_function_decl(ast, analyser);
        }


    } break;

    case AstType_VariableDecl: {
        UNREACHABLE();

    } break;

    case AstType_BadDecl: {

    } break;

    default:
        context()->reporter.report_error(
            ast->span, analyser.curr_ast_file->source_code,
            "invalid top level declaration"
        );

        break;
    }
}



void resolve_function_decl(Ast *decl, Analyser analyser) {

    Ast *value_ast = decl->ConstDecl.value_ast;
    XP_ASSERT_DEFAULT(value_ast->type == AstType_FunctionDeclValue);


    Analyser new_sc = new_scope(analyser, ScopeType::Function, decl);
    new_sc.curr_func = decl;

    // 函数参数作为变量声明解析
    for(isize i = 0; i < value_ast->FunctionDeclValue.params.count; i++) {
        Ast *param_ast = value_ast->FunctionDeclValue.params[i];

        //@CleanUp
        if(param_ast->VariableDecl.is_var_arg == true) {
            if(i != value_ast->FunctionDeclValue.params.count - 1) {
                context()->reporter.report_error(
                    param_ast->span, analyser.curr_ast_file->source_code,
                    "variadic parameter must be the last parameter"
                );
            }

            break;
        }

        resolve_var_decl(param_ast, new_sc);
    }

    if(value_ast->FunctionDeclValue.block != NULL) {
        resolve_block(value_ast->FunctionDeclValue.block, new_sc, true);
        if(may_fall_through(value_ast->FunctionDeclValue.block)) {
            // TODO
            context()->reporter.report_error(
                value_ast->FunctionDeclValue.block->span, analyser.curr_ast_file->source_code,
                "function body may fall through without returning"
            );
        }
    }

}    







void resolve_block(Ast *ast, Analyser analyser, bool need_new_scope) {
    if(ast->Block.is_function_body) {

        bind_ast_with_scope(ast, analyser.current_scope);

        for(isize i = 0; i < ast->Block.statements.count; i++) {
            resolve_local_stmt(ast->Block.statements[i], analyser);
        }
        return;
    }

    if(need_new_scope) {
        analyser = new_scope(analyser, ScopeType::Block, ast);
    } else {
        bind_ast_with_scope(ast, analyser.current_scope);
    }
    
    for(isize i = 0; i < ast->Block.statements.count; i++) {
        resolve_local_stmt(ast->Block.statements[i], analyser);
    }
}


void resolve_const_decl_local(Ast *const_decl_ast, Analyser analyser) {

    SymbolInfo *exist = find_symbol_curr(analyser.current_scope, const_decl_ast->ConstDecl.name);
    if(exist != NULL) {
        context()->reporter.report_error(
            const_decl_ast->span, analyser.curr_ast_file->source_code,
            "symbol '%s' already declared in the same scope",
            const_decl_ast->ConstDecl.name.c_str
        );
    }


    Ast *val_ast = const_decl_ast->ConstDecl.value_ast;
    Value val = resolve_comptime_expr(val_ast, analyser);

    if(val.has_error()) {
        // context()->reporter.report_error(
        //     val_ast->span, analyser.curr_ast_file->source_code,
        //     "invalid constant expression"
        // );
        return;
    }


    SymbolInfo new_symbol = make_symbol(const_decl_ast->ConstDecl.name, val, analyser.pkg, analyser.curr_ast_file);
    add_symbol_to_scope(analyser.current_scope, const_decl_ast->ConstDecl.name, new_symbol);
}



void resolve_var_decl(Ast *var_decl_ast, Analyser analyser) {

    // 目前变量可以遮蔽外层作用域的变量, 函数定义, 结构体定义
    // TODO: 是否允许遮蔽, 或者警告

    SymbolInfo *existing = find_symbol_curr(analyser.current_scope, var_decl_ast->VariableDecl.var_name);
    if(existing != NULL) {
        context()->reporter.report_error(
            var_decl_ast->span, analyser.curr_ast_file->source_code,
            "symbol '%s' already declared in the same scope",
            var_decl_ast->VariableDecl.var_name.c_str
        );

        return;
    }


    // TODO CHECK
    if(var_decl_ast->VariableDecl.type_ast != NULL) {
        var_decl_ast->v_type = resolve_type(var_decl_ast->VariableDecl.type_ast, analyser);
    } else {
        var_decl_ast->v_type = undefined_type();
    }


    if(var_decl_ast->v_type == easy_type(Type_void)) {
        context()->reporter.report_error(
            var_decl_ast->span, analyser.curr_ast_file->source_code,
            "variable can not be void type"
        );

        return;
    }
    
    if(var_decl_ast->VariableDecl.expr != NULL) {
        resolve_expr(var_decl_ast->VariableDecl.expr, analyser);
        
        
        if(var_decl_ast->v_type != undefined_type()) {
            // 有显示指定类型和初始化表达式的情况, 类型检查
            
            infer_expr_type(var_decl_ast->VariableDecl.expr, true, var_decl_ast->v_type, analyser, false);

        } else {
            // 有初始化表达式, 无显示指定类型的情况, 类型推导
            infer_expr_type(var_decl_ast->VariableDecl.expr, false, NULL, analyser, false);

            XP_ASSERT_DEFAULT(var_decl_ast->VariableDecl.expr->v_type != undefined_type());

            var_decl_ast->v_type = var_decl_ast->VariableDecl.expr->v_type;
        }

        if(var_decl_ast->VariableDecl.expr->v_type == error_type()) {
            return;
        } 
        
        // TODO 用现有的eval_comptime_expr代替, 添加针对var_decl的特殊处理, 以支持更多的表达式类型
        // 目前存在报错重复, 诸如array init, struct init等不支持
        // ValueResult try_const_val = eval_comptime_expr(var_decl_ast->VariableDecl.expr, analyser);
        // TODO: 如果值无问题, 尝试记录下来, 以便后续编译优化使用
    }

    // 注意这里如果发生重复定义, 不会覆盖掉原来的符号
    Value var_val = make_value_for_var_decl(var_decl_ast->v_type);
    SymbolInfo info = make_symbol(var_decl_ast->VariableDecl.var_name, var_val, analyser.pkg, analyser.curr_ast_file);
    add_symbol_to_scope(analyser.current_scope, var_decl_ast->VariableDecl.var_name, info);

    return;
}


void resolve_local_stmt(Ast *stmt_ast, Analyser analyser) {

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
        
        infer_expr_type(left_expr, false, {}, analyser, false);
        infer_expr_type(right_expr, true, left_expr->v_type, analyser, false);


        if(left_expr->is_lvalue == false) {
            context()->reporter.report_error(
                stmt_ast->span, analyser.curr_ast_file->source_code,
                "left side of assignment is not a lvalue"
            );
        }

    } break;

    case AstType_IfStmt: {
        resolve_expr(stmt_ast->IfStmt.condition, analyser);

        TypeRef condition_expr_type = easy_type(Type_bool);
        infer_expr_type(stmt_ast->IfStmt.condition, true, condition_expr_type, analyser, false);
        

        resolve_block(stmt_ast->IfStmt.then_block, analyser, true);

        if(stmt_ast->IfStmt.else_block != NULL) {
            resolve_block(stmt_ast->IfStmt.else_block, analyser, true);
        }
    } break;

    case AstType_ForStmt: {
        Analyser for_scope = new_scope(analyser, ScopeType::LoopBlock, stmt_ast);

        
        if(stmt_ast->ForStmt.init != NULL) {
            resolve_local_stmt(stmt_ast->ForStmt.init, for_scope);
        }

        if(stmt_ast->ForStmt.condition != NULL) {
            resolve_expr(stmt_ast->ForStmt.condition, for_scope);
            infer_expr_type(stmt_ast->ForStmt.condition, true, easy_type(Type_bool), for_scope, false);
        }
        


        if(stmt_ast->ForStmt.post != NULL) {
            resolve_local_stmt(stmt_ast->ForStmt.post, for_scope);
        }
        

        // 对于for 循环体的block, 不创建新作用域, 而是和init. condition. post共享同一作用域
        // 同时ScopeType::LoopBlock也标记了这是一个循环体作用域
        resolve_block(stmt_ast->ForStmt.body, for_scope, false);
    } break;

    case AstType_ReturnStmt: {

        // 无论是否有和别符号冲突, 函数都有v_type
        TypeRef return_type = analyser.curr_func->v_type->function_info.return_type;

        if(stmt_ast->ReturnStmt.expr != NULL) {
            resolve_expr(stmt_ast->ReturnStmt.expr, analyser);


            if(return_type == easy_type(Type_void)) {
                context()->reporter.report_error(
                    stmt_ast->span, analyser.curr_ast_file->source_code,
                    "return statement should not have expression in void function"
                );

                break;
            }

            infer_expr_type(stmt_ast->ReturnStmt.expr, true, return_type, analyser, false);
        } else {
            if(return_type != easy_type(Type_void)) {
                context()->reporter.report_error(
                    stmt_ast->span, analyser.curr_ast_file->source_code,
                    "return statement missing expression in non-void function"
                );
            }
        }

        
    } break;

    case AstType_Block: {
        // 作为statement的block, 只是单纯的花括号, 没有设计如if/for那样的控制流语义

        resolve_block(stmt_ast, analyser, true);
    } break;
    
    case AstType_Break: {
        if(get_upper_scope_with_type(analyser.current_scope, ScopeType::LoopBlock) == NULL) {
            context()->reporter.report_error(
                stmt_ast->span, analyser.curr_ast_file->source_code,
                "break statement not within loop"
            );
        }
    } break;
    case AstType_Continue: {
        if(get_upper_scope_with_type(analyser.current_scope, ScopeType::LoopBlock) == NULL) {
            context()->reporter.report_error(
                stmt_ast->span, analyser.curr_ast_file->source_code,
                "continue statement not within loop"
            );
        }
    } break;

    case AstType_FunctionCallExpr: {
        resolve_expr(stmt_ast, analyser);
        infer_expr_type(stmt_ast, false, NULL, analyser, false);
    } break;


    case AstType_ConstDecl: {
        resolve_const_decl_local(stmt_ast, analyser);
    } break;

    default: {
        context()->reporter.report_error(
            stmt_ast->span, analyser.curr_ast_file->source_code,
            "invalid statement"
        );
    } break;
    
    }
} 

void resolve_expr(Ast *expr_ast, Analyser analyser) {
    if(expr_ast == NULL) {
        return;
    }

    switch (expr_ast->type)
    {
    case AstType_Ident: {
        resolve_ident(expr_ast, analyser);
    } break;
    case AstType_FunctionCallExpr: {
        
        // 检查函数标识符符号是否存在
        resolve_expr(expr_ast->FunctionCallExpr.func_ident, analyser);

        for(isize i = 0; i < expr_ast->FunctionCallExpr.args.count; i++) {
            resolve_expr(expr_ast->FunctionCallExpr.args[i], analyser);
        }


        SymbolInfo *info = expr_ast->FunctionCallExpr.func_ident->ast_symbol;
        if(info == NULL) {
            break; // 说明前面已经报过错了, 这里就不继续解析了
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

    } break;

    case AstType_StructInitExpr: {
        
        TypeRef maybe_struct_type = resolve_type(expr_ast->StructInitExpr.struct_type_ident, analyser);

        if(maybe_struct_type == error_type()) {
            break; // 说明前面已经报过错了, 这里就不继续解析了
        }

        SymbolInfo *symbol = expr_ast->StructInitExpr.struct_type_ident->ast_symbol;

        if(symbol == NULL || !is_struct_type(maybe_struct_type)) {
            context()->reporter.report_error(
                expr_ast->span, analyser.curr_ast_file->source_code,
                "try to initialize undefined struct type '%s'",
                get_ident_or_fieldaccess_string(expr_ast->StructInitExpr.struct_type_ident, temp_allocator()).c_str
            );

            break;
        }

        for(isize i = 0; i < expr_ast->StructInitExpr.field_inits.count; i++) {
            resolve_expr(expr_ast->StructInitExpr.field_inits[i], analyser);
        }
    } break;

    case AstType_FieldAccess: {
        resolve_field_access(expr_ast, analyser);
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

        
    } break;

    case AstType_Undefined: {
        UNREACHABLE();
    } break;

    case AstType_BadExpr: {
        // BadExpr, 不解析表达式, 直接等后续阶段报错
    } break;

    default: {
        UNREACHABLE();
    } break;
    
    }
}





TypeRef resolve_type(Ast *type_ast, Analyser analyser) {
    XP_ASSERT_DEFAULT(type_ast != NULL);

    switch (type_ast->type) {
        case AstType_EasyType: {
            return easy_type(type_ast->EasyType.kind);
        } break;    

        
        case AstType_PointerType: {
            TypeRef pointed_type = resolve_type(type_ast->PointerType.pointed_type_ast, analyser);
            if(pointed_type == error_type()) {
                return error_type();
            }    

            return pointer_type(pointed_type);
        } break;    

        
        case AstType_Ident: {
            SymbolInfo *maybe_type_info = resolve_ident(type_ast, analyser);

            if(maybe_type_info == NULL) {
                return error_type();
            }
            

            TypeRef type = maybe_type_info->value.type;
            if(!(is_type_type(type) && is_struct_type(type->self_type_info))) {
                context()->reporter.report_error(
                    type_ast->span, analyser.curr_ast_file->source_code,
                    "symbol '%s' is not a type",
                    type_ast->Ident.name.c_str
                );

                return error_type();
            }

            
            return type->self_type_info;
        } break;    

        // 作为类型, parent只能是package, child只能是类型(目前只有结构体)名
        case AstType_FieldAccess: {
            Ast *parent_ident = type_ast->FieldAccess.parent;
            xpString filde_ident = type_ast->FieldAccess.field_name;

            SymbolInfo *symbol = resolve_field_access(type_ast, analyser); 
            if(symbol == NULL) {
                return error_type();
            }

            TypeRef type = symbol->value.type;
            if(!(is_type_type(type) && is_struct_type(type->self_type_info))) {
                context()->reporter.report_error(
                    type_ast->span, analyser.curr_ast_file->source_code,
                    "symbol '%s' is not a type",
                    filde_ident.c_str
                );

                return error_type();
            }

            return type->self_type_info;
        } break;    
            


        case AstType_ArrayType: {
            TypeRef element_type = resolve_type(type_ast->ArrayType.element_type_ast, analyser);
            if(element_type == error_type()) {
                return error_type();
            }

            Ast *count_expr = type_ast->ArrayType.count_expr;


            resolve_expr(count_expr, analyser);
            infer_expr_type(count_expr, false, NULL, analyser, false);

            if(count_expr->v_type == error_type()) {
                context()->reporter.report_error(
                    count_expr->span, analyser.curr_ast_file->source_code,
                    "invalid array size expression type"
                );    

                return error_type();
            }

            if(!is_integer_type(count_expr->v_type)) {
                context()->reporter.report_error(
                    count_expr->span, analyser.curr_ast_file->source_code,
                    "array size expression must be a constant integer expression"
                );    
                
                return error_type();
            }
            
            ValueResult count_val_result = eval_comptime_expr(count_expr, analyser);
            if(count_val_result.is_err()) {
                context()->reporter.report_error(
                    count_expr->span, analyser.curr_ast_file->source_code,
                    "not a valid constant expression for array size"
                );    

                return error_type();
            }

            
            Value count_val = count_val_result.as_ok();
            i128 count = get_integer_value(count_val);
            if(count <= 0 || count > INTPTR_MAX) { // TODO 换掉这个最大值宏

                context()->reporter.report_error(
                    count_expr->span, analyser.curr_ast_file->source_code,
                    "array size must be a positive integer and less than max of isize"
                );    

                return error_type();
            }    

            TypeRef type_ref = array_type(element_type, cast(usize)count);
            return type_ref;
        } break;


        case AstType_SliceType: {
            TypeRef elem_type = resolve_type(type_ast->SliceType.element_type_ast, analyser);
            if(elem_type == error_type()) {
                return error_type();
            }    

            TypeRef slice_type = slice_type_as_struct(elem_type);

            return slice_type;
        } break;



        case AstType_BadType: {
            // BadType, 不解析类型, 直接等后续阶段报错
            return error_type();
        } break;

        case AstType_BadExpr: {
            return error_type();
        } break;    

        default: {
            return error_type();
        } break;

    }    
}    

SymbolInfo *resolve_string_as_ident_and_eval_unsolved(xpString str, Analyser analyser) {
    SymbolInfo *info = find_symbol_until_global(analyser.current_scope, str);
    if(info == NULL) {
        return NULL;
    }

    if(info->value.state == ValueState::Unsolved) {
        eval_unsolved_in_symbol_table(info, analyser);
    }

    return info;
}


SymbolInfo *resolve_ident(Ast *ident_ast, Analyser analyser) {
    xpString ident_str = ident_ast->Ident.name;
    SymbolInfo *info = resolve_string_as_ident_and_eval_unsolved(ident_str, analyser);

    if(info == NULL) {
        context()->reporter.report_error(
            ident_ast->span, analyser.curr_ast_file->source_code,
            "undefined symbol '%s'",
            ident_str.c_str
        );
    }

    ident_ast->ast_symbol = info; // *RECORD SYMBOL IN AST*
    return info;
}


SymbolInfo *resolve_field_access(Ast *field_access_ast, Analyser analyser) {
    Ast *parent_ast = field_access_ast->FieldAccess.parent;
    xpString field_name = field_access_ast->FieldAccess.field_name;

    resolve_expr(parent_ast, analyser);


    SymbolInfo *parent_symbol_info = parent_ast->ast_symbol;
    if(parent_symbol_info == NULL) {
        return NULL;
    }


    Value parent_value = parent_symbol_info->value;
    TypeRef parent_type = parent_value.type;
    if(is_package_type(parent_type)) {
        Package *pkg = get_package_value(parent_value);

        SymbolInfo *field_sym = resolve_string_as_ident_and_eval_unsolved(
            field_name,
            analyser
            .set_current_scope(&pkg->package_scope)
            .set_curr_ast_file(parent_symbol_info->file)
            .set_pkg(parent_symbol_info->package)
        );

        if(field_sym == NULL) {
            context()->reporter.report_error(
                field_access_ast->span, analyser.curr_ast_file->source_code,
                "undefined symbol '%s' in package '%s'",
                field_name.c_str, parent_symbol_info->name.c_str
            );

            return NULL;
        }


        field_access_ast->ast_symbol = field_sym; // *RECORD SYMBOL IN AST*
        return field_sym;
    } else {

        // 非包字段访问, 目前只有结构体字段访问, 结构体字段访问的合法性在类型检查阶段检查
        return NULL;
    }


    UNREACHABLE();
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






