



/* 
 * 一些约定:  AstType_Ident 和 AstType_FieldAccess(只限于包名.符号名) 的ast_symbol会在resolve阶段被绑定到对应的SymbolInfo, 其他类型的ast的ast_symbol保持为NULL
*/




#include "analyser.hpp"

#include "evaluator.hpp"

#include "resolve_depend.hpp"

#include "common.hpp"

#include "context.hpp"

#include "type_check.hpp"

#include "error_msg.hpp"

#include "path.hpp"
#include "value_ops.hpp"


Analyser make_analyser(AstFile *curr_ast_file, Package *pkg) {
    Analyser analyser = {};
    analyser.pkg = pkg;
    analyser.current_scope = &curr_ast_file->file_scope;
    analyser.curr_ast_file = curr_ast_file;


    return analyser;
}

Analyser make_analyser(Scope *curr_scope, Package *pkg) {
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





//
// Analyser
//

void resolve_ast_file(AstFile *ast_file, Analyser analyser);
void resolve_top_stmt(Ast *ast, Analyser analyser);
void resolve_function_decl(Ast *ast, Analyser analyser);
void resolve_struct_decl(Ast *decl, Analyser analyser);
void resolve_enum_decl(Ast *decl, Analyser analyser);

void resolve_var_decl(Ast *var_decl_ast, Analyser analyser);
void resolve_local_stmt(Ast *stmt_ast, Analyser analyser);
void resolve_expr(Ast *expr_ast, Analyser analyser);
void resolve_expr2(Ast *expr_ast, Analyser analyser);
void resolve_block(Ast *ast, Analyser analyser, bool need_new_scope);

void resolve_const_decl_local(Ast *const_decl_ast, Analyser analyser, TypeRef target_type = nullptr);
SymbolInfo *resolve_ident(Ast *ident_ast, Analyser analyser);
SymbolInfo * resolve_field_access(Ast *field_access_ast, Analyser analyser);
bool may_fall_through(Ast *ast);




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
// 总流程
//

void init_global_symbols() {

    // basic types
    {
        auto insert_basic_type = [](TypeKind kind) {
            Value type_val = make_value(type_type());
            type_val.type_val(easy_type(kind));

            SymbolInfo symbol_info = make_symbol(
                get_type_kind_str(kind), 
                type_val,
                context()->global_blank_package, 
                nullptr, 
                nullptr
            );
            add_symbol_to_scope(&context()->global_blank_package->package_scope, symbol_info);
        };

        insert_basic_type(Type_void);
        insert_basic_type(Type_bool);
        insert_basic_type(Type_i8);
        insert_basic_type(Type_i32);
        insert_basic_type(Type_i64);
        insert_basic_type(Type_u8);
        insert_basic_type(Type_u32);
        insert_basic_type(Type_u64);
        insert_basic_type(Type_f32);
        insert_basic_type(Type_f64);
        insert_basic_type(Type_isize);
        insert_basic_type(Type_usize);
    }


    {
        xpString type_string = xp_string_c("type");
        TypeRef type_type_ref = type_type();
        Value type_type_value = make_value(type_type_ref);
        type_type_value.type_val(type_type_ref);

        SymbolInfo type_symbol = make_symbol(
            type_string, 
            type_type_value,
            context()->global_blank_package,
            nullptr,
            nullptr
        );
        add_symbol_to_scope(&context()->global_blank_package->package_scope, type_string, type_symbol);
    }
}


void resolve_ast_package(Package *pkg);

// 语义分析的入口函数
void resolve_ast_all_packages(Array<Package> *all_packages) {

    init_global_symbols();

    // 第一阶段：收集所有包的顶层符号
    // 必须先收集所有符号，再解析表达式，否则跨包引用（如 std 中引用 c.malloc）
    // 会因为被引用包的符号尚未注册而失败
    for(isize i = 0; i < all_packages->count; i++) {
        Package *pkg = &(*all_packages)[i];
        collect_top_level_symbols_in_package(pkg);
    }

    // TODO: 多线程解析package, 但需要更详细的依赖分析
    // 第二阶段：逐个包解析AST
    for(isize i = 0; i < all_packages->count; i++) {
        Package *pkg = &(*all_packages)[i];
        resolve_ast_package(pkg);
    }
}

void resolve_ast_package(Package *pkg) {
    for(isize j = 0; j < pkg->ast_files.count; j++) {
        AstFile *ast_file = &pkg->ast_files[j];
        
        resolve_ast_file(ast_file, make_analyser(ast_file, pkg));
    }
}





//
// Analyser
//





Analyser new_scope(Analyser old_state, ScopeType type, Ast *related_ast, std::optional<Scope *> parent_scope = std::nullopt) {
    Scope *new_scope = alloc_scope(
        parent_scope.has_value() ? parent_scope.value() : old_state.current_scope,
        type,
        permanent_allocator(),
        related_ast
    );    
    
    Analyser new_state = old_state;
    new_state.current_scope = new_scope;
    
    
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
            ast->ast_symbol = find_symbol_until_global_ref(analyser.current_scope, ast->ConstDecl.name);
            SymbolInfo *const_info = (ast->ast_symbol)();
            XP_ASSERT_DEFAULT(const_info != NULL);

            // @NOTE: 单独处理import声明
            if(ast->ConstDecl.value_ast->type == AstType_Import) {
                const_info->val(eval_import_decl(ast->ConstDecl.value_ast, analyser)); 
                const_info->state = SymbolState::Solved;
                break;
            }

            resolve_const_decl_local(ast, analyser);
        } break;


        default: {
            context()->reporter.report_error(
                ast->src_loc,
                "invalid top level declaration"
            );

        } break;

    } 
}




/*
    {
        import_in_local :: import "foo"; -> ERROR, import不能出现在函数体内
        function_decl_in_local :: () -> i32 {} -> OK, 
        function_decl_in_local + 1 :: () -> i32 {} + 1 -> ERROR, 函数声明只能独立作为一个语句, 不能作为表达式的一部分
        expr :: 1 + 2 -> OK
        expr :: i32   -> OK
    }    

*/
void resolve_const_decl_local(Ast *const_decl_ast, Analyser analyser, TypeRef target_type) {
    xpString const_ident = const_decl_ast->ConstDecl.name;

    if(!(analyser.current_scope->scope_type == ScopeType::File)) {
        SymbolInfo *exist = find_symbol_curr(analyser.current_scope, const_ident);
        if(exist != NULL) {
            context()->reporter.report_error(
                const_decl_ast->src_loc,
                "symbol '{}' already declared in the same scope",
                const_decl_ast->ConstDecl.name
            );
        }
    }

    Ast *val_ast = const_decl_ast->ConstDecl.value_ast;
    switch(val_ast->type) {
        case AstType_Import: {
            // NOTE: 不允许import在局部定义
            context()->reporter.report_error(
                val_ast->src_loc,
                "import declaration is not allowed in local scope"
            );
        } break;

        default: {
            resolve_expr(val_ast, analyser);
        } break;

    }


    if(!(analyser.current_scope->scope_type == ScopeType::File)) {
        SymbolInfo new_symbol = make_symbol(const_decl_ast->ConstDecl.name, analyser.pkg, analyser.curr_ast_file, const_decl_ast);
        add_symbol_to_scope(analyser.current_scope, const_decl_ast->ConstDecl.name, new_symbol);
        const_decl_ast->ast_symbol = SymbolInfoRef { 
            &analyser.current_scope->symbols, 
            const_decl_ast->ConstDecl.name 
        };
    }
}





void resolve_function_decl(Ast *decl, Analyser analyser) {
    XP_ASSERT_DEFAULT(decl->type == AstType_FunctionDeclValue);


    // 先在当前作用域解析函数的返回类型和参数类型
    if(decl->FunctionDeclValue.return_type_ast != nullptr) {
        resolve_expr(decl->FunctionDeclValue.return_type_ast, analyser);
    }
    
    // // NOTE: 这是为了保证函数内定义的函数的父作用域不是函数, 而是外部, 毕竟函数不能访问别的函数的变量等
    Scope *parent_scope = analyser.current_scope;

    Analyser new_sc = new_scope(analyser, ScopeType::Function, decl, parent_scope);
    
    void resolve_fn_param_list(Array<Ast *>, Analyser);
    
    // 再在函数作用域内解析参数列表
    resolve_fn_param_list(decl->FunctionDeclValue.params, new_sc);


    if(decl->FunctionDeclValue.block != NULL) {
        resolve_block(decl->FunctionDeclValue.block, new_sc, true);

        if(may_fall_through(decl->FunctionDeclValue.block)) {
            context()->reporter.report_error(
                decl->FunctionDeclValue.block->src_loc,
                "function body may fall through without returning"
            );
        }
    }

}

void resolve_struct_decl(Ast *decl, Analyser analyser) {
    Ast *value_ast = decl;
    XP_ASSERT_DEFAULT(value_ast->type == AstType_StructDeclValue);

    Analyser struct_analyser = new_scope(analyser, ScopeType::StructBlock, value_ast);


    for(isize i = 0; i < value_ast->StructDeclValue.fields.count; i++) {
        auto field = value_ast->StructDeclValue.fields[i];

        SymbolInfo *existing = find_symbol_curr(struct_analyser.current_scope, field->StructField.name);
        if(existing != NULL) {
            context()->reporter.report_error(
                field->src_loc,
                "duplicate struct field '{}'", field->StructField.name
            );
        }

        resolve_expr(field->StructField.type_ast, struct_analyser);

        SymbolInfo field_symbol = make_symbol(field->StructField.name, analyser.pkg, analyser.curr_ast_file, field);
        add_symbol_to_scope(struct_analyser.current_scope, field->StructField.name, field_symbol);
    }
}

void resolve_enum_decl(Ast *decl, Analyser analyser) {
    Ast *value_ast = decl;
    XP_ASSERT_DEFAULT(value_ast->type == AstType_EnumDecl);

    if(value_ast->EnumDecl.type_ast != nullptr) {
        resolve_expr(value_ast->EnumDecl.type_ast, analyser);
    }

    Analyser enum_analyser = new_scope(analyser, ScopeType::EnumBlock, value_ast);

    for(isize i = 0; i < value_ast->EnumDecl.fields.count; i++) {
        auto field = value_ast->EnumDecl.fields[i];

        if(field->type == AstType_ConstDecl) {
            resolve_const_decl_local(field, enum_analyser);
        } else {
            xpString field_name = field->Ident.name;

            SymbolInfo *existing = find_symbol_curr(enum_analyser.current_scope, field_name);
            if(existing != NULL) {
                context()->reporter.report_error(
                    field->src_loc,
                    "duplicate enum field '{}'", field_name
                );
            }

            SymbolInfo field_symbol = make_symbol(field_name, analyser.pkg, analyser.curr_ast_file, field);
            add_symbol_to_scope(enum_analyser.current_scope, field_name, field_symbol);
        }
    }
}


void resolve_fn_param_list(Array<Ast *> params, Analyser analyser) {
    for(isize i = 0; i < params.count; i++) {
        Ast *param = params[i];

        SymbolInfo *existing = find_symbol_curr(analyser.current_scope, param->ParamDecl.name);
        if(existing != nullptr) {
            context()->reporter.report_error(
                param->src_loc,
                "duplicate parameter name '{}'",
                param->ParamDecl.name
            );
        }

        if(param->ParamDecl.is_var_arg && i != params.count - 1) {
            context()->reporter.report_error(
                param->src_loc,
                "variadic parameter must be the last parameter"
            );
        }

        SymbolInfo param_symbol = make_symbol(param->ParamDecl.name, analyser.pkg, analyser.curr_ast_file, param);
        add_symbol_to_scope(analyser.current_scope, param->ParamDecl.name, param_symbol);
        param->ast_symbol = SymbolInfoRef { 
            &analyser.current_scope->symbols, 
            param->ParamDecl.name 
        };
        resolve_expr(param->ParamDecl.type_ast, analyser);
    }
}

void resolve_fn_param(Ast *param_ast, Analyser analyser) {
    XP_ASSERT_DEFAULT(param_ast->type == AstType_ParamDecl);

    SymbolInfo *existing = find_symbol_curr(analyser.current_scope, param_ast->ParamDecl.name);
    if(existing != nullptr) {
        context()->reporter.report_error(
            param_ast->src_loc,
            "duplicate parameter name '{}'",
            param_ast->ParamDecl.name
        );
    }

    SymbolInfo param_symbol = make_symbol(param_ast->ParamDecl.name, analyser.pkg, analyser.curr_ast_file, param_ast);
    add_symbol_to_scope(analyser.current_scope, param_ast->ParamDecl.name, param_symbol);
    param_ast->ast_symbol = SymbolInfoRef { 
        &analyser.current_scope->symbols, 
        param_ast->ParamDecl.name 
    };

    resolve_expr(param_ast->ParamDecl.type_ast, analyser);
    return;
}



void resolve_block(Ast *ast, Analyser analyser, bool need_new_scope) {
    if(ast->Block.is_function_body) {


        for(isize i = 0; i < ast->Block.statements.count; i++) {
            resolve_local_stmt(ast->Block.statements[i], analyser);
        }
        return;
    }

    if(need_new_scope) {
        analyser = new_scope(analyser, ScopeType::Block, ast);
    }
    
    for(isize i = 0; i < ast->Block.statements.count; i++) {
        resolve_local_stmt(ast->Block.statements[i], analyser);
    }
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
    } break;

    case AstType_IfStmt: {
        resolve_expr(stmt_ast->IfStmt.condition, analyser);

        resolve_block(stmt_ast->IfStmt.then_block, analyser, true);

        if(stmt_ast->IfStmt.else_block != NULL) {
            resolve_block(stmt_ast->IfStmt.else_block, analyser, true);
        }
    } break;

    case AstType_ForStmt: {
        Analyser for_scope = new_scope(analyser, ScopeType::LoopBlock, stmt_ast);

        if(stmt_ast->ForStmt.iter_var != NULL) {
            resolve_var_decl(stmt_ast->ForStmt.iter_var, for_scope);

            if(stmt_ast->ForStmt.index_var != NULL) {
                resolve_var_decl(stmt_ast->ForStmt.index_var, for_scope);
            }

            if(stmt_ast->ForStmt.iterable != NULL) {
                resolve_expr(stmt_ast->ForStmt.iterable, for_scope);
            }
            if(stmt_ast->ForStmt.iterable_end != NULL) {
                resolve_expr(stmt_ast->ForStmt.iterable_end, for_scope);
            }
        }

        if(stmt_ast->ForStmt.condition != NULL) {
            resolve_expr(stmt_ast->ForStmt.condition, for_scope);
        }

        resolve_block(stmt_ast->ForStmt.body, for_scope, false);
    } break;

    case AstType_ReturnStmt: {
        if(stmt_ast->ReturnStmt.expr != NULL) {
            resolve_expr(stmt_ast->ReturnStmt.expr, analyser);
        }
    } break;

    case AstType_Block: {
        // 作为statement的block, 只是单纯的花括号, 没有设计如if/for那样的控制流语义

        resolve_block(stmt_ast, analyser, true);
    } break;
    
    case AstType_Break: {
        if(get_upper_scope_with_type(analyser.current_scope, ScopeType::LoopBlock) == NULL) {
            context()->reporter.report_error(
                stmt_ast->src_loc,
                "break statement not within loop"
            );
        }
    } break;
    case AstType_Continue: {
        if(get_upper_scope_with_type(analyser.current_scope, ScopeType::LoopBlock) == NULL) {
            context()->reporter.report_error(
                stmt_ast->src_loc,
                "continue statement not within loop"
            );
        }
    } break;

    case AstType_FunctionCallExpr: {
        resolve_expr(stmt_ast, analyser);
    } break;


    case AstType_ConstDecl: {
        resolve_const_decl_local(stmt_ast, analyser);
    } break;

    default: {
        context()->reporter.report_error(
            stmt_ast->src_loc,
            "invalid statement"
        );
    } break;
    
    }
}

void resolve_var_decl(Ast *var_decl_ast, Analyser analyser) {

    SymbolInfo *existing = find_symbol_curr(analyser.current_scope, var_decl_ast->VariableDecl.var_name);
    if(existing != NULL) {
        context()->reporter.report_error(
            var_decl_ast->src_loc,
            "symbol '{}' already declared in the same scope",
            var_decl_ast->VariableDecl.var_name
        );

        return;
    }

    resolve_expr(var_decl_ast->VariableDecl.type_ast, analyser);

    if(var_decl_ast->VariableDecl.expr != NULL) {
        resolve_expr(var_decl_ast->VariableDecl.expr, analyser);
    }


    SymbolInfo info = make_symbol(var_decl_ast->VariableDecl.var_name, analyser.pkg, analyser.curr_ast_file, var_decl_ast);
    add_symbol_to_scope(analyser.current_scope, var_decl_ast->VariableDecl.var_name, info);
    var_decl_ast->ast_symbol = SymbolInfoRef { 
        &analyser.current_scope->symbols, 
        var_decl_ast->VariableDecl.var_name 
    };
    return;
}


void resolve_expr2(Ast *expr_ast, Analyser analyser) {
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
            resolve_expr2(expr_ast->FunctionCallExpr.func_ident, analyser);

            for(isize i = 0; i < expr_ast->FunctionCallExpr.args.count; i++) {
                resolve_expr2(expr_ast->FunctionCallExpr.args[i], analyser);
            }


            SymbolInfo *info = (expr_ast->FunctionCallExpr.func_ident->ast_symbol)();
            if(info == NULL) {
                break; // 说明前面已经报过错了, 这里就不继续解析了
            }

        } break;

        case AstType_BinaryExpr: {
            resolve_expr2(expr_ast->BinaryExpr.left, analyser);
            resolve_expr2(expr_ast->BinaryExpr.right, analyser);

        } break;

        case AstType_UnaryExpr: {
            resolve_expr2(expr_ast->UnaryExpr.operand, analyser);
        } break;

        case AstType_CastExpr: {
            resolve_expr2(expr_ast->CastExpr.expr, analyser);
            resolve_expr2(expr_ast->CastExpr.target_type_ast, analyser);
        } break;

        case AstType_Constant: {

        } break;

        case AstType_StructInitExpr: {
            resolve_expr(expr_ast->StructInitExpr.struct_type_ident, analyser);

            for(isize i = 0; i < expr_ast->StructInitExpr.field_inits.count; i++) {
                resolve_expr2(expr_ast->StructInitExpr.field_inits[i], analyser);
            }
        } break;

        case AstType_FieldAccess: {
            resolve_field_access(expr_ast, analyser);
        } break;

        case AstType_ArrayInitExpr: {
            for(isize i = 0; i < expr_ast->ArrayInitExpr.elements.count; i++) {
                resolve_expr2(expr_ast->ArrayInitExpr.elements[i], analyser);
            }
        } break;

        case AstType_IndexExpr: {
            resolve_expr2(expr_ast->IndexExpr.array_var_expr, analyser);
            resolve_expr2(expr_ast->IndexExpr.index_expr, analyser);
        } break;


        case AstType_StringLiteralExpr: {

        } break;


        case AstType_EasyType: {

        } break;

        case AstType_PointerType: {
            resolve_expr2(expr_ast->PointerType.pointed_type_ast, analyser);
        } break;

        // TODO: 统一类型和表达式的解析, 类型也是表达式
        case AstType_ArrayType: {
            resolve_expr2(expr_ast->ArrayType.element_type_ast, analyser);
            resolve_expr2(expr_ast->ArrayType.count_expr, analyser);
        } break;

        case AstType_SliceType: {
            resolve_expr2(expr_ast->SliceType.element_type_ast, analyser);
        } break;

        case AstType_FunctionType: {
            for(isize i = 0; i < expr_ast->FunctionType.param_types.count; i++) {
                resolve_expr2(expr_ast->FunctionType.param_types[i], analyser);
            }

            resolve_expr2(expr_ast->FunctionType.return_type_ast, analyser);
        } break;

    case AstType_BadExpr: {
        UNREACHABLE();
    } break;
    
    case AstType_Undefined: {
        UNREACHABLE();
    } break;

    default: {
        DEBUG_LOG("unhandled expr type: {}", ast_string(expr_ast->type));

        UNREACHABLE();
    } break;
    
    }
}


void resolve_expr(Ast *expr_ast, Analyser analyser) {
    if(expr_ast == NULL) {
        return;
    }

    switch (expr_ast->type) {
        case AstType_FunctionDeclValue: {
            resolve_function_decl(expr_ast, analyser);
        } break;

        case AstType_StructDeclValue: {
            resolve_struct_decl(expr_ast, analyser);
        } break;

        case AstType_EnumDecl: {
            resolve_enum_decl(expr_ast, analyser);
        } break;

        case AstType_UnionDecl: {
            context()->reporter.report_error(
                expr_ast->src_loc,
                "union type is not supported yet"
            );
        } break;

        default: {
            resolve_expr2(expr_ast, analyser);
        } break;
    }
}


SymbolInfo *resolve_string_as_ident(xpString str, Analyser analyser) {
    SymbolInfo *info = find_symbol_until_global(analyser.current_scope, str);
    if(info == NULL) {
        return NULL;
    }

    return info;
}


SymbolInfo *resolve_ident(Ast *ident_ast, Analyser analyser) {
    xpString ident_str = ident_ast->Ident.name;
    SymbolInfo *info = resolve_string_as_ident(ident_str, analyser);

    if(info == NULL) {
        context()->reporter.report_error(
            ident_ast->src_loc,
            "undefined symbol '{}'",
            ident_str
        );
    }

    ident_ast->ast_symbol = find_symbol_until_global_ref(analyser.current_scope, ident_str);
    return info;
}


SymbolInfo *resolve_field_access(Ast *field_access_ast, Analyser analyser) {
    Ast *parent_ast = field_access_ast->FieldAccess.parent;
    xpString field_name = field_access_ast->FieldAccess.field_name;

    resolve_expr2(parent_ast, analyser);


    SymbolInfo *parent_symbol_info = (parent_ast->ast_symbol)();
    if(parent_symbol_info == nullptr) {
        return nullptr;
    }


    auto r = parent_symbol_info->result();
    Value parent_value = r.state == CIRResultState::WholeValue ? r.actual_val() : make_value();
    TypeRef parent_type = parent_value.type;
    if(is_package_type(parent_type)) {
        Package *pkg = parent_value.type->package_info;

        Analyser pkg_analyser = analyser;
        pkg_analyser.current_scope = &pkg->package_scope;
        pkg_analyser.curr_ast_file = parent_symbol_info->file;
        pkg_analyser.pkg = parent_symbol_info->package;

        SymbolInfo *field_sym = resolve_string_as_ident(field_name, pkg_analyser);

        if(field_sym == NULL) {
            context()->reporter.report_error(
                field_access_ast->src_loc,
                "undefined symbol '{}' in package '{}'",
                field_name, parent_symbol_info->name
            );

            return NULL;
        }


        field_access_ast->ast_symbol = find_symbol_until_global_ref(pkg_analyser.current_scope, field_name);
        return (field_access_ast->ast_symbol)();
    } else {
        // TODO: 结构体字段访问, 枚举字段访问, 放到type_check阶段检查
        // 因为目前还没有类型信息, 没法检查

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

