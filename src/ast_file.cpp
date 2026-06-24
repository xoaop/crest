#include "ast_file.hpp"


AstFile make_ast_file(Array<Ast *> top_levels, SourceCode src_code) {
    AstFile f;
    f.top_levels = top_levels;
    f.source_code = src_code;

    return f;
}