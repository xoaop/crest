#include "ast_file.hpp"


AstFile ast_file_make() {
    AstFile f = {};
    f.top_levels = make_array<Ast *>(ast_allocator());

    return f;
}