#ifndef CREST_ANALYSER_HPP
#define CREST_ANALYSER_HPP

#include "xoaop.h"

#include "parser.hpp"

#include "symbol.hpp"

#include "ast_file.hpp"

#include "scope.hpp"
#include "package.hpp"

struct Analyser {    

    Array<Package> all_packages;

    Package *pkg;
    Scope *current_scope;

    Ast *curr_func;
};





void sema_analysis_all_packages(Array<Package> all_packages);
SymbolInfo *find_symbol_by_ident_or_fieldaccess_in_other_packages(Ast *ident_ast, Analyser analyser);




#endif // CREST_ANALYSIS_HPP