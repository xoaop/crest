#ifndef CREST_AST_FILE_HPP
#define CREST_AST_FILE_HPP

#include "ast.hpp"

#include "scope.hpp"

#include "source_code.hpp"

//Ast文件定义 - 同时也是单文件级编译单元
struct AstFile {
    Array<Ast *> top_levels;
    Scope file_scope;

    SourceCode source_code;

};


AstFile ast_file_make();

#endif