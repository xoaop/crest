#ifndef CREST_ANALYSER_HPP
#define CREST_ANALYSER_HPP

#include "xoaop.h"
#include "array.hpp"

#include "ast.hpp"
#include "scope.hpp"


struct Package;
struct AstFile;

struct Analyser {
    Package *pkg;
    Scope *current_scope;
    AstFile *curr_ast_file;
    Ast *curr_func;
};

Analyser make_analyser(AstFile *curr_ast_file, Package *pkg);
Analyser make_analyser(Scope *curr_scope, Package *pkg);
Analyser make_analyser(Scope *curr_scope, AstFile *file, Package *pkg);

void collect_top_level_symbols_in_package(Package *pkg);
void resolve_ast_package(Package *pkg);
void resolve_ast_all_packages(Array<Package> *all_packages);


#endif // CREST_ANALYSER_HPP