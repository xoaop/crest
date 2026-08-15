#include "evaluator.hpp"

#include "context.hpp"
#include "value_ops.hpp"



void collect_top_level_symbols_in_file(AstFile *ast_file, PackageRef curr_pkg);
void collect_const_decl_symbol(Ast *const_decl_ast, Analyser analyser);




//
// 1. Symbol Collect
//
void collect_top_level_symbols_in_package(PackageRef pkg) {
    Package* p = package_by_ref(pkg);

    for(isize i = 0; i < p->ast_files.count; i++) {
        AstFile *ast_file = &p->ast_files[i];
        collect_top_level_symbols_in_file(ast_file, pkg);
    }
}



void collect_top_level_symbols_in_file(AstFile *ast_file, PackageRef curr_pkg) {

    ast_file->file_scope = make_scope(&package_by_ref(curr_pkg)->package_scope, ScopeType::File, permanent_allocator());
    add_sub_scope(&package_by_ref(curr_pkg)->package_scope, &ast_file->file_scope);
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
        add_symbol_to_scope(&package_by_ref(analyser.pkg)->package_scope, const_decl_ast->ConstDecl.name, new_symbol);
    }

    return;
}