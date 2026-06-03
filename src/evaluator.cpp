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
    add_sub_scope(&context()->global_blank_package.package_scope, &pkg->package_scope);

    for(isize i = 0; i < pkg->ast_files.count; i++) {
        AstFile *ast_file = &pkg->ast_files[i];
        collect_top_level_symbols_in_file(ast_file, pkg, all_packages);
    }    
}



void collect_top_level_symbols_in_file(AstFile *ast_file, Package *curr_pkg, Array<Package> *all_packages) {

    ast_file->file_scope = make_scope(&curr_pkg->package_scope, ScopeType::File, permanent_allocator());
    add_sub_scope(&curr_pkg->package_scope, &ast_file->file_scope);
    for(isize i = 0; i < ast_file->top_levels.count; i++) {
        Ast *top_level = ast_file->top_levels[i];

        Analyser analyser = make_analyser(ast_file, curr_pkg, all_packages);
        switch(top_level->type) {
        case AstType_ConstDecl: {
            collect_const_decl_symbol(top_level, make_analyser(ast_file, curr_pkg, all_packages));
        } break;    
        
        // case AstType_VariableDecl: {
        //     collect_var_decl_symbol(top_level, make_analyser(ast_file, curr_pkg, all_packages));
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
            const_decl_ast->span, analyser.curr_ast_file->source_code,
            "symbol '{}' repeated definition",
            const_decl_ast->ConstDecl.name
        );    
        return;
    }    

    Value new_value = make_value();

    SymbolInfo new_symbol = make_symbol(const_decl_ast->ConstDecl.name, new_value, analyser.pkg, analyser.curr_ast_file, const_decl_ast);
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
        return make_value();
    }    

    Package *imported_package = imported_package_opt.unwrap();
    Value import_value = make_value();
    import_value.set_package_value(imported_package);

    return import_value;
}    