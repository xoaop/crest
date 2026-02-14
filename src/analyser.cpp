#include "analyser.hpp"

#include "common.hpp"

#include "context.hpp"

#include "type_check.hpp"
#include "const_fold.hpp"

#include "error_msg.hpp"

#include "path.hpp"



Analyser make_analyser(AstFile *curr_ast_file, Package *pkg, Array<Package> all_packages) {
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








//
// Analyser
//

void resolve_ast_file(AstFile *ast_file, Analyser analyser);
void resolve_top_level(Ast *ast, Analyser analyser);
void resolve_function_decl(Ast *ast, Analyser analyser);

void resolve_var_decl(Ast *var_decl_ast, Analyser analyser);
void resolve_stmt(Ast *stmt_ast, Analyser analyser);
void resolve_expr(Ast *expr_ast, Analyser analyser);
void resolve_block(Ast *ast, Analyser analyser, bool need_new_scope);
void resolve_constant(Ast *constant, Analyser analyser);
TypeRef resolve_type(Ast *type_ast, Analyser analyser);
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
                // 只能是package import或者struct decl, 其他情况不合法, 直接返回NULL

                return NULL;
            }

        } break;
        
        case AstType_FieldAccess: {
            // 目前不支持嵌套的field access作为package访问符号
            return NULL;
            // UNREACHABLE();
        } break;
        
        default: {
            // 只能是表达式, 如(*(ptr + 1))
            return NULL;

        } break;
        }

    } break;

    case AstType_BadExpr: {
        return NULL;
    } break;

    default:
        return NULL;
    }
}






//
//
//








void bind_symbols_for_all_packages(Array<Package> all_packages) {

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

        // 符号表
        SymbolInfo string_struct_symbol;
        string_struct_symbol.name = string_struct_name;
        string_struct_symbol.type = string_struct_typeref;
        string_struct_symbol.kind = SymbolKind::StructDecl;
        add_symbol_to_scope(&context()->global_blank_package.package_scope, string_struct_name, string_struct_symbol);
    }



    for(isize i = 0; i < all_packages.count; i++) {
        bind_top_level_symbols_in_package(&all_packages[i], all_packages);
    }
}



void bind_top_level_symbols_in_package(Package *pkg, Array<Package> all_packages) {
    
    // NOTE: 别忘了创建package scope
    pkg->package_scope = make_scope(&context()->global_blank_package.package_scope, ScopeType::Package, permanent_allocator());


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

                // NOTE: ScopeType::Global
                // 因为我不想让import符号覆盖掉同一文件里定义的函数和结构体符号
                // 同时也不应和全局符号冲突, 如string
                SymbolInfo *info = find_symbol_in_scope_list_until(ScopeType::Global, &ast_file->file_scope, top_level->Import.alias);

                if(info != NULL && info->kind != SymbolKind::PackageImport) {
                    // import名字冲突

                    context()->reporter.report_error(
                        top_level->span, 
                        ast_file->source_code,
                        "imported package name '%s' conflicts with existing symbol '%s'",
                        top_level->Import.alias.c_str, info->name.c_str
                    );

                    break;
                } else if(info != NULL) {
                    // 已经import过该package
                    context()->reporter.report_error(
                        top_level->span, 
                        ast_file->source_code,
                        "package name '%s' already imported",
                        top_level->Import.alias.c_str
                    );

                    break;
                }



                // 查找被import的package
                Package *imported_package = NULL;
                for(isize j = 0; j < all_packages.count; j++) {
                    if(xp_string_equal(all_packages[j].path, concat_path(xp_string_c(context()->main_src_dir_path), top_level->Import.path, stage_allocator()))) {
                        imported_package = &all_packages[j];
                        break;
                    }
                }
                if(imported_package == NULL) {
                    context()->reporter.report_error(
                        top_level->span, 
                        ast_file->source_code,
                        "imported package '%s' not found",
                        top_level->Import.alias.c_str
                    );

                    break;
                }

                SymbolInfo import_symbol = {};
                import_symbol.kind = SymbolKind::PackageImport;
                import_symbol.name = top_level->Import.alias;
                import_symbol.type = undefined_type();

                import_symbol.imported_package = imported_package;

                // import是文件作用域的符号
                add_symbol_to_scope(&ast_file->file_scope, import_symbol.name, import_symbol);
            } break;

            case AstType_StructDecl: {

                SymbolInfo *info = find_symbol_in_scope_list_until(ScopeType::Package, &ast_file->file_scope, top_level->StructDecl.name);
                if(info != NULL && info->kind == SymbolKind::StructDecl) {

                    context()->reporter.report_error(
                        top_level->span, ast_file->source_code,
                        "struct '%s' repeated definition",
                        top_level->StructDecl.name.c_str
                    );

                    break;
                } else if(info != NULL) {

                    context()->reporter.report_error(
                        top_level->span, ast_file->source_code,
                        "symbol '%s' already defined with different kind",
                        top_level->StructDecl.name.c_str
                    );

                    break;
                }


                SymbolInfo struct_symbol = {};
                struct_symbol.kind = SymbolKind::StructDecl;
                struct_symbol.name = top_level->StructDecl.name;
                
                struct_symbol.type = unfinished_struct_type(curr_pkg, top_level->StructDecl.name);
                struct_symbol.type->struct_info.decl_ast = top_level;
                struct_symbol.type->struct_info.resolve_state = ResolveState::Unresolved;

                // 结构体是包作用域的符号
                add_symbol_to_scope(&curr_pkg->package_scope, struct_symbol.name, struct_symbol);


            } break;

            case AstType_Function: {
                // 函数符号的绑定放到后面, 因为函数类型依赖结构体类型
            } break;

            case AstType_BadDecl: {
                // BadDecl, 不绑定符号, 直接等后续阶段报错
            } break;

            default: {
                UNREACHABLE();
            } break;
        }

    }

    // NOTE: 绑定函数符号, 需要在结构体和import绑定之后, 因为函数涉及到类型, 依赖结构体声明的符号
    for(isize i = 0; i < ast_file->top_levels.count; i++) {
        Ast *top_level = ast_file->top_levels[i];


        switch(top_level->type)
        {

        case AstType_StructDecl: {

            SymbolInfo *struct_symbol_info = find_symbol_in_scope_list_until_with_kind(ScopeType::Package, &ast_file->file_scope, top_level->StructDecl.name, SymbolKind::StructDecl);
            if(struct_symbol_info == NULL) {
                break; // 说明前面符号绑定阶段已经报过错了, 这里就不继续解析了
            }
            if(struct_symbol_info->type->struct_info.resolve_state == ResolveState::Resolved) {
                break; // 已经解析过了, 可能是之前解析另一个struct的时候递归解析到的, 直接跳过
            }

            // 解析结构体声明的字段类型, 补充符号表
            resolve_struct_decl(top_level, make_analyser(ast_file, curr_pkg, all_packages));
        } break;

        case AstType_Function: {
            
            bool has_error = false;

            SymbolInfo *info = find_symbol_in_scope_list_until(ScopeType::Package, &ast_file->file_scope, top_level->Function.name);
            if(info != NULL && info->kind == SymbolKind::FunctionDecl) {
                
                context()->reporter.report_error(
                    top_level->span, ast_file->source_code,
                    "function '%s' repeated definition",
                    top_level->Function.name.c_str
                );

                has_error = true;
            } else if(info != NULL) {
                context()->reporter.report_error(
                    top_level->span, ast_file->source_code,
                    "symbol '%s' already defined with different kind",
                    top_level->Function.name.c_str
                );

                has_error = true;
            }
    
    
            SymbolInfo func_symbol = {};
            func_symbol.kind = SymbolKind::FunctionDecl;
            func_symbol.name = top_level->Function.name;
            func_symbol.is_extern_c = top_level->Function.is_extern_C;
    
            top_level->v_type = resolve_function_decl_type(top_level, make_analyser(ast_file, curr_pkg, all_packages));
            func_symbol.type = top_level->v_type;

            // 函数是包作用域的符号
            if(!has_error) {
                add_symbol_to_scope(&curr_pkg->package_scope, func_symbol.name, func_symbol);
            }

    
        } break;

        case AstType_Import: {
            // do nothing
        } break;

        
        default:
            break;
        }

    }

}



TypeRef resolve_function_decl_type(Ast *decl, Analyser analyser) {
    Array<TypeRef> param_types = make_array<TypeRef>(type_allocator());
    for(isize i = 0; i < decl->Function.params.count; i++) {
        Ast *param_ast = decl->Function.params[i];

        // 解析参数类型
        TypeRef param_type = resolve_type(param_ast->VariableDecl.type_ast, analyser);
        array_push_back(&param_types, param_type);
    }

    // 解析返回值类型
    TypeRef return_type = resolve_type(decl->Function.return_type_ast, analyser);

    return function_type(param_types, return_type);
}



void resolve_struct_decl(Ast *decl, Analyser analyser) {
    SymbolInfo *struct_type_info = find_symbol_in_scope_list_until_with_kind(ScopeType::Package, analyser.current_scope, decl->StructDecl.name, SymbolKind::StructDecl);
    XP_ASSERT_DEFAULT(struct_type_info != NULL);

    struct_type_info->type->struct_info.resolve_state = ResolveState::Resolving; // 标记正在解析中, 以便检测循环依赖s

    Array<StructField> field_types = make_array<StructField>(type_allocator());
    for(isize i = 0; i < decl->StructDecl.fields.count; i++) {
        Ast *field_ast = decl->StructDecl.fields[i];

        // 解析字段类型, 
        TypeRef field_type = resolve_type(field_ast->StructField.type_ast, analyser);
        array_push_back(&field_types, StructField{field_ast->StructField.name, field_type});

        if(is_struct_type(field_type)) {
            if(field_type->struct_info.resolve_state == ResolveState::Unresolved) {
                // 递归解析struct字段的struct类型, 可能会有循环依赖
                resolve_struct_decl(field_type->struct_info.decl_ast, analyser);
            } else if(field_type->struct_info.resolve_state == ResolveState::Resolving) {
                // 循环依赖, 报错
                context()->reporter.report_error(
                    field_ast->span, analyser.curr_ast_file->source_code,
                    "circular dependency detected in struct declarations involving struct '%s'",
                    field_type->type_name.c_str
                );

                struct_type_info->type = error_type();
                break;
            }
        }

    }


    struct_type_info->type->struct_info.struct_fields = field_types;
    struct_type_info->type->struct_info.resolve_state = ResolveState::Resolved;
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
            SymbolInfo *type_info = find_symbol_by_ident_or_fieldaccess_in_other_packages(type_ast, analyser);
            type_ast->ast_symbol = type_info; // 记录一下符号表信息, 方便后续类型检查阶段使用

            if(type_info == NULL || type_info->kind != SymbolKind::StructDecl) {
                context()->reporter.report_error(
                    type_ast->span, analyser.curr_ast_file->source_code,
                    "undefined type '%s'",
                    type_ast->Ident.name.c_str
                );

                return error_type();
            }

            
            return type_info->type;
        } break;

        // 作为类型, parent只能是package, child只能是类型(目前只有结构体)名
        case AstType_FieldAccess: {
            Ast *parent_ident = type_ast->FieldAccess.parent;
            xpString field_ident = type_ast->FieldAccess.field_name;

            xpString parent_str = get_ident_or_fieldaccess_string(parent_ident, temp_allocator());

            SymbolInfo *package_symbol_info = find_symbol_by_ident_or_fieldaccess_in_other_packages(parent_ident, analyser);
            
            parent_ident->ast_symbol = package_symbol_info; // 记录一下符号表信息, 方便后续类型检查阶段使用
            if(package_symbol_info == NULL || package_symbol_info->kind != SymbolKind::PackageImport) {
                context()->reporter.report_error(
                    parent_ident->span, analyser.curr_ast_file->source_code,
                    "undefined package '%s'",
                    parent_str.c_str
                );
                return error_type();
            }

            Package *imported_package = package_symbol_info->imported_package;

            // 在被import的package里找 结构体类型
            SymbolInfo *type_symbol_info = find_symbol_in_scope_list_until_with_kind(
                ScopeType::Package,
                &imported_package->package_scope,
                field_ident,
                SymbolKind::StructDecl
            );

            type_ast->ast_symbol = type_symbol_info; // 记录一下符号表信息, 方便后续类型检查阶段使用
            if(type_symbol_info == NULL) {
                context()->reporter.report_error(
                    type_ast->span, analyser.curr_ast_file->source_code,
                    "undefined struct type '%s' in package '%s'",
                    field_ident.c_str, parent_str.c_str
                );

                return error_type();
            }


            return type_symbol_info->type;
        } break;
            


        case AstType_ArrayType: {
            TypeRef element_type = resolve_type(type_ast->ArrayType.element_type_ast, analyser);
            if(element_type == error_type()) {
                return error_type();
            }

            Ast *count_expr = type_ast->ArrayType.count_expr;


            resolve_expr(count_expr, analyser);
            infer_expr_type(count_expr, false, NULL, analyser);

            if(count_expr->v_type == error_type()) {
                context()->reporter.report_error(
                    count_expr->span, analyser.curr_ast_file->source_code,
                    "invalid array size expression type"
                );

                return error_type();
            }

            if(!count_expr->is_const_expr || !is_integer_type(count_expr->v_type)) {
                context()->reporter.report_error(
                    count_expr->span, analyser.curr_ast_file->source_code,
                    "array size expression must be a constant integer expression"
                );

                return error_type();
            }

            if(!try_constant_expr_folding(count_expr)) {
                context()->reporter.report_error(
                    count_expr->span, analyser.curr_ast_file->source_code,
                    "failed to fold array size expression to constant"
                );

                return error_type();
            }

            i128 count = count_expr->Constant.value;
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
            
            resolve_ast_file(ast_file, make_analyser(ast_file, pkg, all_packages));
        }
    }


}





void resolve_ast_file(AstFile *ast_file, Analyser analyser) {

    analyser.curr_ast_file = ast_file;

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

    case AstType_StructDecl:
    case AstType_Import: {
        // DO NOTHING, 已经在符号绑定阶段解析完了
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


void resolve_function_decl(Ast *ast, Analyser analyser) {

    for(isize i = 0; i < ast->Function.params.count; i++) {
        resolve_var_decl(ast->Function.params[i], analyser);
    }
    
    if(ast->Function.block != NULL) {
        

        resolve_block(ast->Function.block, analyser, true);


        if(may_fall_through(ast->Function.block)) {
            if(analyser.curr_func->v_type->function_info.return_type->kind != Type_void) {
                context()->reporter.report_error(
                    ast->span, analyser.curr_ast_file->source_code,
                    "non-void function may fall through without return"
                );
            }
        }
    }
}




void resolve_block(Ast *ast, Analyser analyser, bool need_new_scope) {
    if(ast->Block.is_function_body) {
        for(isize i = 0; i < ast->Block.statements.count; i++) {
            resolve_stmt(ast->Block.statements[i], analyser);
        }
        return;
    }

    if(need_new_scope) {
        analyser = new_scope(analyser, ScopeType::Block);
    }
    
    for(isize i = 0; i < ast->Block.statements.count; i++) {
        resolve_stmt(ast->Block.statements[i], analyser);
    }
}

void resolve_var_decl(Ast *var_decl_ast, Analyser analyser) {

    // 目前变量可以遮蔽外层作用域的变量, 函数定义, 结构体定义
    // TODO: 是否允许遮蔽, 或者警告

    SymbolInfo *existing = find_symbol_in_curr_scope_with_kind(analyser.current_scope, var_decl_ast->VariableDecl.var_name, SymbolKind::VarDecl);
    if(existing != NULL) {
        context()->reporter.report_error(
            var_decl_ast->span, analyser.curr_ast_file->source_code,
            "variable '%s' already declared in the same scope",
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
            // TODO 有显示指定类型和初始化表达式的情况, 类型检查
            
            infer_expr_type(var_decl_ast->VariableDecl.expr, true, var_decl_ast->v_type, analyser);

        } else {
            // TODO 有初始化表达式, 无显示指定类型的情况, 类型推导
            // infer_expr_type(var_decl_ast->VariableDecl.expr, NULL, analyser);
            infer_expr_type(var_decl_ast->VariableDecl.expr, false, {}, analyser);

            XP_ASSERT_DEFAULT(var_decl_ast->VariableDecl.expr->v_type != undefined_type());

            var_decl_ast->v_type = var_decl_ast->VariableDecl.expr->v_type;
        }

        if(var_decl_ast->VariableDecl.expr->v_type == error_type()) {
            // 如果初始化表达式的类型错误了, 就不继续检查了, 避免产生过多的错误信息
            context()->reporter.report_error(
                var_decl_ast->VariableDecl.expr->span, analyser.curr_ast_file->source_code,
                "variable '%s' has invalid initializer which type is illegal",
                var_decl_ast->VariableDecl.var_name.c_str
            );
        } else {
            // TODO 检查
            try_constant_expr_folding(var_decl_ast->VariableDecl.expr);
        }

    } else {
    }

    // 注意这里如果发生重复定义, 不会覆盖掉原来的符号
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
            context()->reporter.report_error(
                stmt_ast->span, analyser.curr_ast_file->source_code,
                "left side of assignment is not a lvalue"
            );
        }

    } break;

    case AstType_IfStmt: {
        resolve_expr(stmt_ast->IfStmt.condition, analyser);

        TypeRef condition_expr_type = easy_type(Type_bool);
        infer_expr_type(stmt_ast->IfStmt.condition, true, condition_expr_type, analyser);
        

        resolve_block(stmt_ast->IfStmt.then_block, analyser, true);

        if(stmt_ast->IfStmt.else_block != NULL) {
            resolve_block(stmt_ast->IfStmt.else_block, analyser, true);
        }
    } break;

    case AstType_ForStmt: {
        Analyser for_scope = new_scope(analyser, ScopeType::LoopBlock);

        
        if(stmt_ast->ForStmt.init != NULL) {
            resolve_stmt(stmt_ast->ForStmt.init, for_scope);
        }

        if(stmt_ast->ForStmt.condition != NULL) {
            resolve_expr(stmt_ast->ForStmt.condition, for_scope);
            infer_expr_type(stmt_ast->ForStmt.condition, true, easy_type(Type_bool), for_scope);
        }
        


        if(stmt_ast->ForStmt.post != NULL) {
            resolve_stmt(stmt_ast->ForStmt.post, for_scope);
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

            infer_expr_type(stmt_ast->ReturnStmt.expr, true, return_type, analyser);
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
        if(analyser.current_scope->scope_type != ScopeType::LoopBlock) {
            context()->reporter.report_error(
                stmt_ast->span, analyser.curr_ast_file->source_code,
                "break statement not within loop"
            );
        }
    } break;
    case AstType_Continue: {
        if(analyser.current_scope->scope_type != ScopeType::LoopBlock) {
            context()->reporter.report_error(
                stmt_ast->span, analyser.curr_ast_file->source_code,
                "continue statement not within loop"
            );
        }
    } break;

    case AstType_FunctionCallExpr: {
        resolve_expr(stmt_ast, analyser);
        infer_expr_type(stmt_ast, false, NULL, analyser);
    } break;

    default: {
        UNREACHABLE();
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
        expr_ast->ast_symbol = entry; // 记录一下符号表信息, 方便后续类型检查阶段使用
        if(entry == NULL) {
            context()->reporter.report_error(
                expr_ast->span, analyser.curr_ast_file->source_code,
                "undefined symbol '%s'",
                expr_ast->Ident.name.c_str
            );

        }

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
        
        if(info->kind != SymbolKind::FunctionDecl) {
            context()->reporter.report_error(
                expr_ast->span, analyser.curr_ast_file->source_code,
                "called symbol '%s' is not a function",
                info->name.c_str
            );
            break;
        }

        if(info->type->function_info.param_types.count != expr_ast->FunctionCallExpr.args.count) {
            context()->reporter.report_error(
                expr_ast->span, analyser.curr_ast_file->source_code,
                "function '%s' expects %lld arguments but got %lld",
                info->name.c_str, info->type->function_info.param_types.count, expr_ast->FunctionCallExpr.args.count
            );
            break;
        }

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
        
        if(resolve_type(expr_ast->StructInitExpr.struct_type_ident, analyser) == error_type()) {
            break; // 说明前面已经报过错了, 这里就不继续解析了
        }

        // SymbolInfo *symbol = find_symbol_by_ident_or_fieldaccess_in_other_packages(expr_ast->StructInitExpr.struct_type_ident, analyser);
        SymbolInfo *symbol = expr_ast->StructInitExpr.struct_type_ident->ast_symbol;
        if(symbol == NULL || symbol->kind != SymbolKind::StructDecl) {
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
        // NOTE: resolve阶段, 对存在于表达式的FieldAccess只检查作为符号访问的合法性
        // 与类型信息无关, 结构体字段访问的合法性在类型检查阶段检查


        // 检查了parent(作为符号)是否存在
        resolve_expr(expr_ast->FieldAccess.parent, analyser);

        if(expr_ast->FieldAccess.parent->type == AstType_Ident) {
            // 如果是标识符, 再发现是package, 检查符号存不存在, 
            // 别的就是结构体字段访问, 不在这里检查合法性, 在类型检查阶段检查

            SymbolInfo *parent_symbol = expr_ast->FieldAccess.parent->ast_symbol;
            if(parent_symbol == NULL) {
                break;
            }
            
            if(parent_symbol->kind == SymbolKind::PackageImport) {
                // 在被import的package里找field_name符号

                Package *imported_package = parent_symbol->imported_package;

                SymbolInfo *field_symbol = find_symbol_in_scope_list_until(
                    ScopeType::Package,
                    &imported_package->package_scope,
                    expr_ast->FieldAccess.field_name
                );

                if(field_symbol == NULL) {
                    context()->reporter.report_error(
                        expr_ast->span, analyser.curr_ast_file->source_code,
                        "undefined symbol '%s' in package '%s'",
                        expr_ast->FieldAccess.field_name.c_str, parent_symbol->name.c_str
                    );
                }
                expr_ast->ast_symbol = field_symbol; // 记录一下符号表信息, 方便后续类型检查阶段使用
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

    tag_expr_const_by_sons(expr_ast, analyser);
    tag_untyped_expr(expr_ast, analyser);
}




void resolve_constant(Ast *constant, Analyser analyser) {
    Token token = constant->token;


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


    if(token.number_info.type_kind_of_number != Type_Undefined) {
        constant->v_type = easy_type(token.number_info.type_kind_of_number);
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
        if(expr->is_null) {
            // // TODO: 目前常量折叠不支持计算null指针, 先把它标记为非const, 后续如果需要支持null指针常量折叠再改回来
            // expr->is_const_expr = false;
            // break;
        }

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
                context()->reporter.report_error(
                    expr->span, analyser.curr_ast_file->source_code,
                    "type mismatch in binary expression with untyped operands"
                );

                expr->v_type = error_type();
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






