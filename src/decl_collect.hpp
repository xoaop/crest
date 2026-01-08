#include "ast.hpp"

struct Analyser;


struct AllDecls {
    Array<Ast *> struct_decls;
    Array<Ast *> variable_decls;
    Array<Ast *> function_decls;
};


void declaration_collect_ast_visitor(Ast *ast, Analyser *analyser, AllDecls *all_decls);
AllDecls declaration_collect_ast_file(AstFile *file, Analyser *analyser, xpAllocator allocator);