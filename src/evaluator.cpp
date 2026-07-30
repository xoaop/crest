#include "evaluator.hpp"

#include "context.hpp"
#include "value_ops.hpp"
#include "resolve_depend.hpp"

// 前向声明
void collect_top_level_symbols_in_file(AstFile *ast_file, Package *curr_pkg);

//
// 1. Symbol Collect
//
void collect_top_level_symbols_in_package(Package *pkg) {
    
    // NOTE: 别忘了创建package scope
    // global_blank_package 的 scope 已在 main.cpp 初始化，跳过以免自引用
    if (pkg != context()->global_blank_package) {
        pkg->package_scope = make_scope(&context()->global_blank_package->package_scope, ScopeType::Package, permanent_allocator());
        add_sub_scope(&context()->global_blank_package->package_scope, &pkg->package_scope);
    }

    for(isize i = 0; i < pkg->ast_files.count; i++) {
        AstFile *ast_file = &pkg->ast_files[i];
        collect_top_level_symbols_in_file(ast_file, pkg);
    }    
}



void collect_top_level_symbols_in_file(AstFile *ast_file, Package *curr_pkg) {

    ast_file->file_scope = make_scope(&curr_pkg->package_scope, ScopeType::File, permanent_allocator());
    add_sub_scope(&curr_pkg->package_scope, &ast_file->file_scope);
    for(isize i = 0; i < ast_file->top_levels.count; i++) {
        Ast *top_level = ast_file->top_levels[i];

        Analyser analyser = make_analyser(ast_file, curr_pkg);
        switch(top_level->type) {
        case AstType_ConstDecl: {
            collect_const_decl_symbol(top_level, make_analyser(ast_file, curr_pkg));
        } break;    
        
        // case AstType_VariableDecl: {
        //     collect_var_decl_symbol(top_level, make_analyser(ast_file, curr_pkg));
        // } break;    

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
            SourceLocation(analyser.curr_ast_file->source_code, const_decl_ast->src_loc.span),
            "symbol '{}' repeated definition",
            const_decl_ast->ConstDecl.name
        );    
        return;
    }    

    SymbolInfo new_symbol = make_symbol(const_decl_ast->ConstDecl.name, analyser.pkg, analyser.curr_ast_file, const_decl_ast);
    if(value_ast->type == AstType_Import) {
        add_symbol_to_scope(analyser.current_scope, const_decl_ast->ConstDecl.name, new_symbol);
    } else {
        add_symbol_to_scope(&analyser.pkg->package_scope, const_decl_ast->ConstDecl.name, new_symbol);
    }

    return;
}






Value eval_import_decl(Ast *import_ast, Analyser analyser) {

    // 查找被import的package
    xpOption<Package *> imported_package_opt = get_package_by_import(
        import_ast->Import.path,
        &context()->all_packages
    );

    if(imported_package_opt.is_none()) {
        context()->reporter.report_error(
            SourceLocation(analyser.curr_ast_file->source_code, import_ast->src_loc.span),
            "imported package '{}' not found",
            import_ast->Import.path
        );    
        return make_value();
    }    

    Package *imported_package = imported_package_opt.unwrap();
    Value import_value = make_value();
    import_value.set_type(package_type(imported_package));

    return import_value;
}    