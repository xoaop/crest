#include "ast.hpp"

struct Analyser;

void declaration_collect_ast_visitor(Ast *ast, Analyser *analyser);
void declaration_collect_ast_file(AstFile *file, Analyser *analyser);