#ifndef CREST_ANALYSER_HPP
#define CREST_ANALYSER_HPP

#include "xoaop.h"

#include "parser.hpp"

#include "symbol.hpp"

#include "ast_file.hpp"

#include "scope.hpp"
#include "package.hpp"

struct Analyser {    

    Analyser set_pkg(Package *pkg);
    Analyser set_current_scope(Scope *current_scope);
    Analyser set_curr_ast_file(AstFile *curr_ast_file);
    Analyser set_curr_func(Ast *curr_func);



    Array<Package> *all_packages;

    Package *pkg;
    Scope *current_scope;

    AstFile *curr_ast_file;
    Ast *curr_func;
};





void sema_analysis_all_packages(Array<Package> *all_packages);
SymbolInfo *find_symbol_by_ident_or_fieldaccess(Ast *ident_ast, Analyser analyser);

TypeRef resolve_type(Ast *type_ast, Analyser analyser);


#endif // CREST_ANALYSIS_HPP