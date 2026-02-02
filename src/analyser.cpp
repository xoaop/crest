#include "analyser.hpp"

#include "common.hpp"

#include "context.hpp"

#include "type_check.hpp"
#include "const_fold.hpp"

#include "error_msg.hpp"

#include "path.hpp"


void analyser_init(Analyser *analyser, Scope *curr_scope, Package *pkg, Array<Package> all_packages) {
    analyser->pkg = pkg;
    analyser->current_scope = curr_scope;
    analyser->all_packages = all_packages;
}

Analyser make_analyser(Scope *curr_scope, Package *pkg, Array<Package> all_packages) {
    Analyser analyser = {};
    analyser_init(&analyser, curr_scope, pkg, all_packages);
    return analyser;
}

Analyser make_analyser(Scope *curr_scope, Package *pkg) {
    // without all_packages
    Analyser analyser = {};
    analyser.current_scope = curr_scope;
    analyser.pkg = pkg;

    return analyser;
}









//
// Analyser
//

void resolve_ast_file(AstFile *ast_file, Analyser analyser);
void resolve_top_level(Ast *ast, Analyser analyser);
void resolve_function_decl(Ast *ast, Analyser analyser);

void resolve_var_decl(Ast *var_decl_ast, Analyser analyser);
void resolve_stmt(Ast *stmt_ast, Analyser analyser);
void resolve_expr(Ast *expr_ast, Analyser analyser);
void resolve_block(Ast *ast, Analyser analyser);
void resolve_constant(Ast *constant, Analyser analyser);
TypeRef resolve_type(Ast *type_ast, Analyser analyser);
void try_constant_expr_folding(Ast *const_expr);
void tag_expr_const_by_sons(Ast *expr, Analyser analyser);
void tag_untyped_expr(Ast *expr, Analyser analyser);
bool may_fall_through(Ast *ast);



//
// Symbol Binding
//



void bind_symbols_for_all_packages(Array<Package> all_packages);
void bind_top_level_symbols_in_package(Package *pkg, Array<Package> all_packages);
void bind_symbol_in_file(AstFile *ast_file, Package *curr_pkg, Array<Package> all_packages);
TypeRef resolve_function_decl_type(Ast *decl, Analyser analyser);
void resolve_struct_decl(Ast *ast, Analyser analyser);



SymbolInfo *find_symbol_by_ident_or_fieldaccess_in_other_packages(Ast *ident_ast, Analyser analyser) {

    switch(ident_ast->type)
    {
        
        
    case AstType_Ident: {
        // 如果只是个单独的标识符, 那么就在当前作用域链里找, 直到全局作用域


        SymbolInfo *symbol_info = find_symbol_in_scope_list_until(
            ScopeType::Global,
            analyser.current_scope,
            ident_ast->Ident.name
        );

        return symbol_info;

    } break;
    
    case AstType_FieldAccess: {

        Ast *parent = ident_ast->FieldAccess.parent;
        xpString field_name = ident_ast->FieldAccess.field_name;

        switch(parent->type)
        {
        case AstType_Ident: {
            SymbolInfo *parent_symbol = find_symbol_in_scope_list_until(ScopeType::Package, analyser.current_scope, parent->Ident.name);

            if(parent_symbol == NULL) {
                return NULL;
            }

            if(parent_symbol->kind == SymbolKind::PackageImport) {
                // 在被import的package里找field_name符号

                Package *imported_package = parent_symbol->imported_package;

                SymbolInfo *field_symbol = find_symbol_in_scope_list_until(
                    ScopeType::Package,
                    &imported_package->package_scope,
                    field_name
                );

                return field_symbol;
            } else if(parent_symbol->kind == SymbolKind::StructDecl) {
                // 这个函数只是用来查符号的, 不检查结构体字段访问合法性, 直接返回NULL

                return NULL;
            } else {
                // 未知情况, 按理不该触发, 若不然, 只能是bug

                XP_ASSERT_DEFAULT(0);
            }

        } break;
        
        case AstType_FieldAccess: {
            // 目前不支持嵌套的field access作为package访问符号

            XP_ASSERT_DEFAULT(0);
        } break;
        
        default: {
            // 只能是表达式, 如(*(ptr + 1))
            return NULL;

        } break;
        }

    } break;

    default:
        XP_ASSERT_DEFAULT(0);
        return NULL;
    }
}






//
//
//








void bind_symbols_for_all_packages(Array<Package> all_packages) {

    // TODO 添加全局符号, 如string类型



    for(isize i = 0; i < all_packages.count; i++) {
        bind_top_level_symbols_in_package(&all_packages[i], all_packages);
    }
}



void bind_top_level_symbols_in_package(Package *pkg, Array<Package> all_packages) {
    
    // NOTE: 别忘了创建package scope
    pkg->package_scope = make_scope(&context()->global_scope, ScopeType::Package, permanent_allocator());


    for(isize i = 0; i < pkg->ast_files.count; i++) {
        AstFile *ast_file = &pkg->ast_files[i];
        bind_symbol_in_file(ast_file, pkg, all_packages);
    }
}



void bind_symbol_in_file(AstFile *ast_file, Package *curr_pkg, Array<Package> all_packages) {

    // NOTE: 创建文件作用域
    ast_file->file_scope = make_scope(&curr_pkg->package_scope, ScopeType::File, permanent_allocator());

    for(isize i = 0; i < ast_file->top_levels.count; i++) {
        Ast *top_level = ast_file->top_levels[i];

        switch(top_level->type) {
            case AstType_Import: {
                SymbolInfo *info =
                    find_symbol_in_scope_list_until(ScopeType::File, &ast_file->file_scope, top_level->Import.alias);

                if(info != NULL && info->kind != SymbolKind::PackageImport) {
                    // TODO Warning: import名字冲突
                    error_msg(&top_level->token, "imported package '%s' name conflicts with existing symbol", top_level->Import.alias.c_str);
                    XP_ASSERT_DEFAULT(0);

                    break;
                } else if(info != NULL) {
                    // 已经import过该package

                    error_msg(&top_level->token, "package name '%s' already imported", top_level->Import.alias.c_str);
                    XP_ASSERT_DEFAULT(0);
                    break;
                }



                SymbolInfo import_symbol = {};
                import_symbol.kind = SymbolKind::PackageImport;
                import_symbol.name = top_level->Import.alias;
                import_symbol.type = undefined_type();

                // 查找被import的package
                Package *imported_package = NULL;
                for(isize j = 0; j < all_packages.count; j++) {
                    if(xp_string_equal(all_packages[j].path, concat_path(xp_string_c(context()->main_src_dir_path), top_level->Import.path, stage_allocator()))) {
                        imported_package = &all_packages[j];
                        break;
                    }
                }

                // TODO 把检查import的包是否存在放到这里, 而不是resolve_depend里
                if(imported_package == NULL) {
                    // TODO ERROR: 未找到被import的package
                    error_msg(&top_level->token, "imported package '%s' not found", top_level->Import.alias.c_str);
                    XP_ASSERT_DEFAULT(0);
                }


                import_symbol.imported_package = imported_package;


                // import是文件作用域的符号
                add_symbol_to_scope(&ast_file->file_scope, import_symbol.name, import_symbol);

            } break;

            case AstType_Function: {
                SymbolInfo *info = find_symbol_in_scope_list_until(ScopeType::Package, &ast_file->file_scope, top_level->Function.name);
                if(info != NULL && info->kind == SymbolKind::FunctionDecl) {

                    // TODO ERROR: function重复定义
                    error_msg(&top_level->token, "function '%s' repeated definition", top_level->Function.name.c_str);
                    XP_ASSERT_DEFAULT(0);
                } else if(info != NULL) {

                    // TODO ERROR: 名字冲突
                    error_msg(&top_level->token, "symbol '%s' already defined with different kind", top_level->Function.name.c_str);
                    XP_ASSERT_DEFAULT(0);
                }


                SymbolInfo func_symbol = {};
                func_symbol.kind = SymbolKind::FunctionDecl;
                func_symbol.name = top_level->Function.name;

                // 函数是包作用域的符号
                add_symbol_to_scope(&curr_pkg->package_scope, func_symbol.name, func_symbol);


                top_level->v_type = resolve_function_decl_type(top_level, make_analyser(&ast_file->file_scope, curr_pkg, all_packages));
            } break;

            case AstType_StructDecl: {

                SymbolInfo *info = find_symbol_in_scope_list_until(ScopeType::Package, &ast_file->file_scope, top_level->StructDecl.name);
                if(info != NULL && info->kind == SymbolKind::StructDecl) {

                    // TODO ERROR: struct重复定义
                    error_msg(&top_level->token, "struct '%s' repeated definition", top_level->StructDecl.name.c_str);
                    XP_ASSERT_DEFAULT(0);
                } else if(info != NULL) {

                    // TODO ERROR: 名字冲突
                    error_msg(&top_level->token, "symbol '%s' already defined with different kind", top_level->StructDecl.name.c_str);
                    XP_ASSERT_DEFAULT(0);
                }


                SymbolInfo struct_symbol = {};
                struct_symbol.kind = SymbolKind::StructDecl;
                struct_symbol.name = top_level->StructDecl.name;
                struct_symbol.type = unfinished_struct_type(curr_pkg, top_level->StructDecl.name);

                // 结构体是包作用域的符号
                add_symbol_to_scope(&curr_pkg->package_scope, struct_symbol.name, struct_symbol);

                // 解析结构体声明的字段类型, 补充符号表
                resolve_struct_decl(top_level, make_analyser(&ast_file->file_scope, curr_pkg, all_packages));

            } break;

            default: {
                // TODO ERROR: 不支持的top level类型
                error_msg(&top_level->token, "unsupported top level AST type for symbol binding");
                XP_ASSERT_DEFAULT(0);
            } break;
        }

    }

}



TypeRef resolve_function_decl_type(Ast *decl, Analyser analyser) {
    SymbolInfo *func_info = find_symbol_in_scope_list_until_with_kind(ScopeType::Package, analyser.current_scope, decl->Function.name, SymbolKind::FunctionDecl);
    XP_ASSERT_DEFAULT(func_info != NULL);

    Array<TypeRef> param_types = make_array<TypeRef>(type_allocator());
    for(isize i = 0; i < decl->Function.params.count; i++) {
        Ast *param_ast = decl->Function.params[i];

        // 解析参数类型
        TypeRef param_type = resolve_type(param_ast->VariableDecl.type_ast, analyser);
        array_push_back(&param_types, param_type);
    }

    // 解析返回值类型
    TypeRef return_type = resolve_type(decl->Function.return_type_ast, analyser);

    func_info->type = function_type(param_types, return_type);

    return func_info->type;
}



void resolve_struct_decl(Ast *decl, Analyser analyser) {
    SymbolInfo *struct_type_info = find_symbol_in_scope_list_until_with_kind(ScopeType::Package, analyser.current_scope, decl->StructDecl.name, SymbolKind::StructDecl);
    XP_ASSERT_DEFAULT(struct_type_info != NULL);

    Array<StructField> field_types = make_array<StructField>(type_allocator());
    for(isize i = 0; i < decl->StructDecl.fields.count; i++) {
        Ast *field_ast = decl->StructDecl.fields[i];

        // 解析字段类型, 
        TypeRef field_type = resolve_type(field_ast->StructField.type_ast, analyser);
        array_push_back(&field_types, StructField{field_ast->StructField.name, field_type});

        if(field_type == struct_type_info->type) {
            
            // 字段类型不能和结构体本身相同 错误处理
            error_msg(&field_ast->token, "struct field type can not be the same as struct type itself");
            XP_ASSERT_DEFAULT(0);
        }

    }

    struct_type_info->type->struct_info.struct_fields = field_types;
}






TypeRef resolve_type(Ast *type_ast, Analyser analyser) {
    XP_ASSERT_DEFAULT(type_ast != NULL);

    switch (type_ast->type) {
        case AstType_EasyType: {
            return easy_type(type_ast->EasyType.kind);
        } break;

        
        case AstType_PointerType: {
            TypeRef pointed_type = resolve_type(type_ast->PointerType.pointed_type_ast, analyser);
            return pointer_type(pointed_type);
        } break;

        
        case AstType_Ident: {
            SymbolInfo *type_info = find_symbol_by_ident_or_fieldaccess_in_other_packages(type_ast, analyser);
            if(type_info == NULL || type_info->kind != SymbolKind::StructDecl) {

                // TODO ERROR: 未定义的类型
                error_msg(&type_ast->token, "undefined type '%s'", type_ast->Ident.name.c_str);
                XP_ASSERT_DEFAULT(0);
            }
            
            return type_info->type;
        } break;

        // 作为类型, parent只能是package, child只能是类型(目前只有结构体)名
        case AstType_FieldAccess: {
            Ast *parent_ident = type_ast->FieldAccess.parent;
            xpString field_ident = type_ast->FieldAccess.field_name;

            SymbolInfo *package_symbol_info = find_symbol_by_ident_or_fieldaccess_in_other_packages(parent_ident, analyser);
            if(package_symbol_info == NULL || package_symbol_info->kind != SymbolKind::PackageImport) {
                // TODO ERROR: 未定义的package
                error_msg(&parent_ident->token, "undefined package '%s'", parent_ident->Ident.name.c_str);
                XP_ASSERT_DEFAULT(0);
            }

            Package *imported_package = package_symbol_info->imported_package;

            // 在被import的package里找 结构体类型
            SymbolInfo *type_symbol_info = find_symbol_in_scope_list_until_with_kind(
                ScopeType::Package,
                &imported_package->package_scope,
                field_ident,
                SymbolKind::StructDecl
            );

            if(type_symbol_info == NULL) {
                // TODO ERROR: 未定义的 结构体类型
                error_msg(&type_ast->token, "undefined symbol '%s' in package '%s'", field_ident.c_str, imported_package->path.c_str);
                XP_ASSERT_DEFAULT(0);
            }

            return type_symbol_info->type;
        } break;
            


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


        // TODO
        // Slice应该是一种内置结构体
        // case AstType_SliceType: {
        //     TypeRef elem_type = resolve_type(type_ast->SliceType.element_type_ast, analyser);

        //     TypeRef slice_type = slice_type_as_struct(elem_type);

        //     return slice_type;
        // } break;


        default: {
            XP_ASSERT_DEFAULT(0);
        }

    }
}



void resolve_field_access_expr(Ast *field_access, Analyser analyser) {
    Ast *parent = field_access->FieldAccess.parent;
    resolve_expr(parent, analyser);
}




//
// Analyser
//











Analyser new_scope(Analyser old_state, ScopeType type) {
    Scope *new_scope = alloc_scope(
        old_state.current_scope,
        type,
        stage_allocator()
    );
    
    Analyser new_state = old_state;
    new_state.current_scope = new_scope;


    return new_state;
}


void sema_analysis_all_packages(Array<Package> all_packages) {
    defer(xp_arena_allocator_clear(stage_allocator()));




    bind_symbols_for_all_packages(all_packages);




    for(isize i = 0; i < all_packages.count; i++) {
        Package *pkg = &all_packages[i];

        for(isize j = 0; j < pkg->ast_files.count; j++) {
            AstFile *ast_file = &pkg->ast_files[j];
            
            resolve_ast_file(ast_file, make_analyser(&ast_file->file_scope, pkg, all_packages));
        }
    }


}





void resolve_ast_file(AstFile *ast_file, Analyser analyser) {

    for(isize i = 0; i < ast_file->top_levels.count; i++) {
        resolve_top_level(ast_file->top_levels[i], analyser);
    }

    return;
}


void resolve_top_level(Ast *ast, Analyser analyser) {
    // 目前只有函数需要解析, 别的top level AST在符号绑定阶段已经解析完毕

    switch (ast->type) {


    
    case AstType_Function: {
        Analyser new_sc = new_scope(analyser, ScopeType::Function);
        new_sc.curr_func = ast;
        resolve_function_decl(ast, new_sc);
    } break;

    // case AstType_StructDecl:
    //     // resolve_struct_decl(ast, analyser);
    //     break;
        
    default:
        // XP_ASSERT_DEFAULT(0);
        break;
    }
}


void resolve_function_decl(Ast *ast, Analyser analyser) {

    for(isize i = 0; i < ast->Function.params.count; i++) {
        resolve_var_decl(ast->Function.params[i], analyser);
    }
    
    if(ast->Function.block != NULL) {
        

        resolve_block(ast->Function.block, analyser);


        if(may_fall_through(ast->Function.block)) {
            if(analyser.curr_func->v_type->function_info.return_type->kind != Type_void) {
                // TODO 非void函数漏写return错误处理
                error_msg(&ast->token, "non-void function may fall through without return");
                XP_ASSERT_MSG(0, "non-void function may fall through without return");
            }
        }
    }
}




void resolve_block(Ast *ast, Analyser analyser) {
    if(ast->Block.is_function_body) {
        for(isize i = 0; i < ast->Block.statements.count; i++) {
            resolve_stmt(ast->Block.statements[i], analyser);
        }
        return;
    }


    
    for(isize i = 0; i < ast->Block.statements.count; i++) {
        resolve_stmt(ast->Block.statements[i], new_scope(analyser, ScopeType::Block));
    }
}

void resolve_var_decl(Ast *var_decl_ast, Analyser analyser) {

    // 目前变量可以遮蔽外层作用域的变量, 函数定义, 结构体定义
    // TODO: 是否允许遮蔽, 或者警告

    SymbolInfo *existing = find_symbol_in_curr_scope_with_kind(analyser.current_scope, var_decl_ast->VariableDecl.var_name, SymbolKind::VarDecl);
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

            XP_ASSERT_DEFAULT(var_decl_ast->VariableDecl.expr->v_type != undefined_type());

            var_decl_ast->v_type = var_decl_ast->VariableDecl.expr->v_type;
        }

        // TODO 检查
        try_constant_expr_folding(var_decl_ast->VariableDecl.expr);
    } else {
        // 零初始化变量

        // if(var_decl_ast->v_type != undefined_type()) {
        //     // TODO 有显示指定类型, 无初始化表达式的情况, 报错, 变量必须初始化
        //     // XP_ASSERT_DEFAULT(0);
        // } else {
        //     // TODO 变量没有指定类型也没有初始化表达式, 无法推导类型错误处理
        //     XP_ASSERT_MSG(0, "variable type can not be undefined without init expr");
        // }
    }



    SymbolInfo info = {
        .kind = SymbolKind::VarDecl,
        .name = var_decl_ast->VariableDecl.var_name,
        .type = var_decl_ast->v_type,
    };
    add_symbol_to_scope(analyser.current_scope, var_decl_ast->VariableDecl.var_name, info);


    return;
}


void resolve_stmt(Ast *stmt_ast, Analyser analyser) {

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
        new_scope(analyser, ScopeType::LoopBlock);

        
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

            if(analyser.curr_func->v_type->function_info.return_type == easy_type(Type_void)) {
                // TODO 错误处理, return 语句不应有返回值 在void函数中
                error_msg(&stmt_ast->token, "return statement should not have expression in void function");
                XP_ASSERT_DEFAULT(0);
            }

            infer_expr_type(stmt_ast->ReturnStmt.expr, true, analyser.curr_func->v_type->function_info.return_type, analyser);
        } else {
            if(analyser.curr_func->v_type->function_info.return_type != easy_type(Type_void)) {
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
        if(analyser.current_scope->scope_type != ScopeType::LoopBlock) {
            error_msg(&stmt_ast->token, "break statement not within loop");
        }
    } break;
    case AstType_Continue: {
        // TODO 错误处理, continue 必须在循环体内
        if(analyser.current_scope->scope_type != ScopeType::LoopBlock) {
            error_msg(&stmt_ast->token, "continue statement not within loop");
        }
    } break;

    case AstType_FunctionCallExpr: {
        resolve_expr(stmt_ast, analyser);
        infer_expr_type(stmt_ast, false, NULL, analyser);
    } break;

    default: {
        XP_ASSERT_DEFAULT(0);
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
        SymbolInfo *entry = find_symbol_by_ident_or_fieldaccess_in_other_packages(expr_ast, analyser);
        if(entry == NULL) {
            // TODO: 错误处理, 使用未声明符号
            XP_ASSERT_DEFAULT(0);
        }

        // if(entry->kind != SymbolKind::VarDecl) {
        //     // TODO: 错误处理, 变量表达式引用了非变量符号
        //     XP_ASSERT_DEFAULT(0);
        // }


    } break;
    case AstType_FunctionCallExpr: {
        
        // 检查函数标识符符号是否存在
        resolve_expr(expr_ast->FunctionCallExpr.func_ident, analyser);

        for(isize i = 0; i < expr_ast->FunctionCallExpr.args.count; i++) {
            resolve_expr(expr_ast->FunctionCallExpr.args[i], analyser);
        }


        // TODO 函数类型检查
        SymbolInfo *info = find_symbol_by_ident_or_fieldaccess_in_other_packages(expr_ast->FunctionCallExpr.func_ident, analyser);
        
        // TODO 检查函数符号是不是函数
        XP_ASSERT_MSG(info->kind == SymbolKind::FunctionDecl, "called symbol is not a function");

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
        
        // 保证类型存在
        SymbolInfo *symbol = find_symbol_by_ident_or_fieldaccess_in_other_packages(expr_ast->StructInitExpr.struct_type_ident, analyser);
        if(symbol == NULL || symbol->kind != SymbolKind::StructDecl) {
            // TODO ERROR: 未定义的结构体类型
            error_msg(&expr_ast->token, "you try to initialize undefined struct type");
            XP_ASSERT_DEFAULT(0);
        }

        for(isize i = 0; i < expr_ast->StructInitExpr.field_inits.count; i++) {
            resolve_expr(expr_ast->StructInitExpr.field_inits[i], analyser);
        }
    } break;

    case AstType_FieldAccess: {
        // NOTE: resolve阶段, 对存在于表达式的FieldAccess只检查作为符号访问的合法性
        // 与类型信息无关, 结构体字段访问的合法性在类型检查阶段检查


        // 检查了parent(作为符号)是否存在
        resolve_expr(expr_ast->FieldAccess.parent, analyser);

        if(expr_ast->FieldAccess.parent->type == AstType_Ident) {
            // 如果是标识符, 再发现是package, 检查符号存不存在, 
            // 别的就是结构体字段访问, 不在这里检查合法性, 在类型检查阶段检查

            SymbolInfo *parent_symbol = find_symbol_by_ident_or_fieldaccess_in_other_packages(expr_ast->FieldAccess.parent, analyser);
            if(parent_symbol->kind == SymbolKind::PackageImport) {
                // 在被import的package里找field_name符号

                Package *imported_package = parent_symbol->imported_package;

                SymbolInfo *field_symbol = find_symbol_in_scope_list_until(
                    ScopeType::Package,
                    &imported_package->package_scope,
                    expr_ast->FieldAccess.field_name
                );

                if(field_symbol == NULL) {
                    // TODO ERROR: 未定义的符号
                    error_msg(&expr_ast->token, "undefined symbol '%s' in package '%s'", expr_ast->FieldAccess.field_name.c_str, imported_package->path.c_str);
                    XP_ASSERT_DEFAULT(0);
                }
            }
        }

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




void resolve_constant(Ast *constant, Analyser analyser) {
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




void tag_expr_const_by_sons(Ast *expr, Analyser analyser) {
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

void tag_untyped_expr(Ast *expr, Analyser analyser) {
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






