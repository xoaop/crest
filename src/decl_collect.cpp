#include "decl_collect.hpp"

#include "analyser.hpp"

#include "error_msg.hpp"


void declaration_collect_ast_visitor(Ast *ast, Analyser *analyser, AllDecls *all_decls) {
    SymbolInfo info;


    switch(ast->type)
    {
    case AstType_Function: {
        
        SymbolInfo *existing_info = find_symbol(symbol_table(), ast->Function.name);
        
        if(existing_info != NULL) {
            error_msg(&ast->token, "function '%s' redefinition", ast->Function.name.c_str);
            XP_ASSERT_MSG(0, "Function redefinition");
        }


        info.name = ast->Function.name;

        add_symbol(symbol_table(), ast->Function.name, info);

        // 函数参数的变量声明收集
        for(isize i = 0; i < ast->Function.params.count; i++) {
            Ast *param_ast = ast->Function.params[i];
            array_push_back(&all_decls->variable_decls, param_ast);
        }

        // 函数体内的变量声明收集
        if(ast->Function.block != NULL) {
            for(isize i = 0; i < ast->Function.block->Block.statements.count; i++) {
                Ast *stmt = ast->Function.block->Block.statements[i];
                declaration_collect_ast_visitor(stmt, analyser, all_decls);
            }
        }

        array_push_back(&all_decls->function_decls, ast);
    } break;

    
    // TODO 检查
    case AstType_StructDecl: {
        SymbolInfo *existing_info = find_symbol(symbol_table(), ast->StructDecl.name);


        if(existing_info != NULL) {
            error_msg(&ast->token, "struct '%s' redefinition", ast->StructDecl.name.c_str);
            XP_ASSERT_MSG(0, "Struct redefinition");
        }
    
        info.name = ast->StructDecl.name;
        // 创建不确定类型占位符
        info.type = add_uncertain_type(ast->StructDecl.name);
        add_symbol(symbol_table(), ast->StructDecl.name, info);





        array_push_back(&all_decls->struct_decls, ast);
    } break;

    // TODO: 还没有全局变量
    case AstType_VariableDecl:
        array_push_back(&all_decls->variable_decls, ast);

        break;
    default:
        break;
    }


}




AllDecls declaration_collect_ast_file(AstFile *file, Analyser *analyser, xpAllocator allocator) {
    AllDecls all_decls;
    all_decls.struct_decls = make_array<Ast *>(allocator);
    all_decls.variable_decls = make_array<Ast *>(allocator);
    all_decls.function_decls = make_array<Ast *>(allocator);
    
    for(isize i = 0; i < file->top_levels.count; i++) {
        declaration_collect_ast_visitor(file->top_levels[i], analyser, &all_decls);
    }

    return all_decls;
}