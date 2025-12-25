#include "decl_collect.hpp"

#include "analyser.hpp"



void declaration_collect_ast_visitor(Ast *ast, Analyser *analyser) {
    SymbolInfo info;


    switch(ast->type)
    {
    case AstType_Function: {
        
        SymbolInfo *existing_info = find_symbol(&analyser->symbol_table_stack, ast->Function.name);
        XP_ASSERT_MSG(existing_info == NULL, "Function redefinition");


        info = make_symbol_info(SymbolType_Function);
        info.Function.name = ast->Function.name;
        info.Function.param_types = make_array<Type>(permanent_allocator());
        for(isize i = 0; i < ast->Function.params.count; i++) {
            array_push_back(&info.Function.param_types, ast->Function.params[i]->v_type);
        }
        info.Function.return_type = ast->v_type;

        add_symbol(&analyser->symbol_table_stack, ast->Function.name, info);
    } break;
    
    case AstType_VariableDecl:
        if(at_global_scope(analyser)) {
            info = make_symbol_info(SymbolType_VariableDecl);
            info.VariableDecl.name = ast->VariableDecl.var_name;
            info.VariableDecl.type = ast->v_type;

            add_symbol(&analyser->symbol_table_stack, ast->VariableDecl.var_name, info);
        }

    default:
        break;
    }


}




void declaration_collect_ast_file(AstFile *file, Analyser *analyser) {
    static AstVisitorFunc decl_collect_ast_visitor[AstType_COUNT] = {};
    decl_collect_ast_visitor[AstType_Function] = declaration_collect_ast_visitor;
    decl_collect_ast_visitor[AstType_VariableDecl] = declaration_collect_ast_visitor;


    ast_visitor(file->root, decl_collect_ast_visitor, analyser);
}