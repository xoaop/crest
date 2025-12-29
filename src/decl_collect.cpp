#include "decl_collect.hpp"

#include "analyser.hpp"



void declaration_collect_ast_visitor(Ast *ast, Analyser *analyser) {
    SymbolInfo info;


    switch(ast->type)
    {
    case AstType_Function: {
        
        SymbolInfo *existing_info = find_symbol(&analyser->symbol_table_stack, ast->Function.name);
        XP_ASSERT_MSG(existing_info == NULL, "Function redefinition");


        info.name = ast->Function.name;
        info.type = ast->v_type;

        add_symbol(&analyser->symbol_table_stack, ast->Function.name, info);
    } break;
    
    // TODO: 还没有全局变量
    case AstType_VariableDecl:
        if(at_global_scope(analyser)) {
            info.name = ast->VariableDecl.var_name;
            info.type = ast->v_type;

            add_symbol(&analyser->symbol_table_stack, ast->VariableDecl.var_name, info);
        }

    default:
        break;
    }


}




void declaration_collect_ast_file(AstFile *file, Analyser *analyser) {
    for(isize i = 0; i < file->root.count; i++) {
        declaration_collect_ast_visitor(file->root[i], analyser);
    }
}