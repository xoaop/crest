



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
void resolve_const_decl_local(Ast *const_decl_ast, Analyser analyser);
void resolve_expr(Ast *expr_ast, Analyser analyser);
void resolve_block(Ast *ast, Analyser analyser, bool need_new_scope);
TypeRef resolve_type(Ast *type_ast, Analyser analyser);
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

void sema_analysis_package(Package *pkg, Array<Package> *all_packages);

static void init_global_symbols() {

    // basic types
    {
        auto insert_basic_type = [](TypeKind kind) {
            TypeRef type_ref = type_type(easy_type(kind));
            SymbolInfo symbol_info = make_symbol(
                get_type_kind_str(kind), 
                make_comptime_sovled_val(type_ref), 
                &context()->global_blank_package, 
                nullptr
            );
            add_symbol_to_scope(&context()->global_blank_package.package_scope, symbol_info);
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
    }


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

        TypeRef string_struct_typeref = struct_type(&context()->global_blank_package, nullptr, string_struct_name, fields);
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
}


// 语义分析的入口函数
void sema_analysis_all_packages(Array<Package> *all_packages) {
    defer(xp_arena_allocator_clear(stage_allocator()));

    init_global_symbols();

    // TODO: 多线程解析package, 但需要更详细的依赖分析
    // 逐个包进行分析
    for(isize i = 0; i < all_packages->count; i++) {
        Package *pkg = &(*all_packages)[i];

        collect_top_level_symbols_in_package(pkg, all_packages);

        // top levels
        // TODO: 改的更清晰, 目前还在复用之前的代码
        xpHashMapEntry<xpString, SymbolInfo> *symbol_entry;
        for(isize j = xp_hash_map_first_entry(&pkg->package_scope.symbols.symbols, &symbol_entry); j != END_OF_HASH_MAP_INDEX; j = xp_hash_map_next_entry(&pkg->package_scope.symbols.symbols, j, &symbol_entry)) {
            if(symbol_entry->value.value.state != ValueState::Unsolved) {
                continue;
            }

            SymbolInfo *symbol_info = &symbol_entry->value;

            eval_unsolved_in_symbol_table(symbol_info, make_analyser(symbol_info->file, pkg, all_packages));
        }


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



void resolve_top_stmt(Ast *ast, Analyser analyser) {

    switch (ast->type) {

    case AstType_ConstDecl: {
        SymbolInfo *const_info = find_symbol_until_global(analyser.current_scope, ast->ConstDecl.name);
        if(const_info == NULL) {
            break;
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


void resolve_fn_param(Ast *param_ast, Analyser analyser) {
    XP_ASSERT_DEFAULT(param_ast->type == AstType_ParamDecl);

    SymbolInfo *existing = find_symbol_curr(analyser.current_scope, param_ast->ParamDecl.name);
    if(existing != nullptr) {
        context()->reporter.report_error(
            param_ast->span, analyser.curr_ast_file->source_code,
            "duplicate parameter name '%s'",
            param_ast->ParamDecl.name.c_str
        );
    }


    if(param_ast->ParamDecl.is_var_arg) {
        param_ast->v_type = easy_type(Type_var_arg_c);
    } else {
        param_ast->v_type = resolve_type(param_ast->ParamDecl.type_ast, analyser);
    }

    if(param_ast->v_type == easy_type(Type_void)) {
        context()->reporter.report_error(
            param_ast->span, analyser.curr_ast_file->source_code,
            "parameter cannot have void type"
        );
    }

    Value param_value = make_value().set_is_runtime(true).set_type(param_ast->v_type).set_value_state(ValueState::Solved);
    SymbolInfo param_symbol = make_symbol(param_ast->ParamDecl.name, param_value, analyser.pkg, analyser.curr_ast_file);
    add_symbol_to_scope(analyser.current_scope, param_ast->ParamDecl.name, param_symbol);

    return;
}

void resolve_fn_param_list(Array<Ast *> params, Analyser analyser) {
    for(isize i = 0; i < params.count; i++) {
        Ast *param = params[i];

        resolve_fn_param(param, analyser);
        
        if(param->ParamDecl.is_var_arg && i != params.count - 1) {
            context()->reporter.report_error(
                param->span, analyser.curr_ast_file->source_code,
                "variadic parameter must be the last parameter"
            );
        }

    }
}




void resolve_function_decl(Ast *decl, Analyser analyser) {

    Ast *value_ast = decl->ConstDecl.value_ast;
    XP_ASSERT_DEFAULT(value_ast->type == AstType_FunctionDeclValue);


    Analyser new_sc = new_scope(analyser, ScopeType::Function, decl);
    new_sc.curr_func = decl;

    resolve_fn_param_list(value_ast->FunctionDeclValue.params, new_sc);

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


    xpString const_ident = const_decl_ast->ConstDecl.name;

    SymbolInfo *exist = find_symbol_curr(analyser.current_scope, const_ident);
    if(exist != NULL) {
        context()->reporter.report_error(
            const_decl_ast->span, analyser.curr_ast_file->source_code,
            "symbol '%s' already declared in the same scope",
            const_decl_ast->ConstDecl.name.c_str
        );
    }


    Ast *val_ast = const_decl_ast->ConstDecl.value_ast;
    
    // TODO: 支持函数, 结构体定义等先前不支持在函数内部定义的常量, 目前只支持常量表达式
    Value val = {};
    switch(val_ast->type) {
      case AstType_Import: {
        // NOTE: 不允许import在局部定义
        context()->reporter.report_error(
            val_ast->span, analyser.curr_ast_file->source_code,
            "import declaration is not allowed in local scope"
        );

        val = make_error_value();
      } break;

      case AstType_FunctionDeclValue: {
        XP_TODO();

        // val = eval_function_decl_value(val_ast, analyser);
      } break;

      case AstType_StructDeclValue: {
        val = eval_struct_decl_value(val_ast, const_ident, analyser);
      } break;



      default: {
        val = resolve_comptime_expr(val_ast, analyser);
      } break;  
    }


    if(val.has_error()) {
        return;
    }


    SymbolInfo new_symbol = make_symbol(const_decl_ast->ConstDecl.name, val, analyser.pkg, analyser.curr_ast_file);
    add_symbol_to_scope(analyser.current_scope, const_decl_ast->ConstDecl.name, new_symbol);
}



void resolve_var_decl(Ast *var_decl_ast, Analyser analyser) {

    // 目前变量可以遮蔽外层作用域的变量, 函数定义, 结构体定义
    SymbolInfo *existing = find_symbol_curr(analyser.current_scope, var_decl_ast->VariableDecl.var_name);
    if(existing != NULL) {
        context()->reporter.report_error(
            var_decl_ast->span, analyser.curr_ast_file->source_code,
            "symbol '%s' already declared in the same scope",
            var_decl_ast->VariableDecl.var_name.c_str
        );

        return;
    }


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
            // @Robost: 这个判断很随意, 目前只允许结构体类型和基本类型被访问作为类型, 以后可能需要更细化的判断
            if(!(is_type_type(type) && (is_named_type(type->self_type_info) || is_basic_type_kind(type->self_type_info->kind)))) {
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
            xpString filde_ident = type_ast->FieldAccess.field_name;

            SymbolInfo *symbol = resolve_field_access(type_ast, analyser); 
            if(symbol == NULL) {
                return error_type();
            }

            TypeRef type = symbol->value.type;

            // @Robost: 这个判断很随意, 目前只允许结构体类型和基本类型被访问作为类型, 以后可能需要更细化的判断
            if(!(is_type_type(type) && (is_named_type(type->self_type_info) || is_basic_type_kind(type->self_type_info->kind)))) {
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


    // 目前这个懒解析只存在于在解析top level时遇到未解析的**当前**package的符号, 不会涉及到跨package的符号, 因为:
    // 由于禁止package的循环依赖, 且解析按依赖顺序进行, 不可能出现解析一个package时遇到另一个未解析的package的符号的情况
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