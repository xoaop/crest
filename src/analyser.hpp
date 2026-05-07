#ifndef CREST_ANALYSER_HPP
#define CREST_ANALYSER_HPP

#include "xoaop.h"

#include "parser.hpp"

#include "symbol.hpp"

#include "ast_file.hpp"

#include "scope.hpp"
#include "package.hpp"

struct Analyser {    

    Array<Package> *all_packages;
    
    Package *pkg;
    Scope *current_scope;
    
    AstFile *curr_ast_file;
    Ast *curr_func;
    
    
    
    Analyser set_pkg(Package *pkg);
    Analyser set_current_scope(Scope *current_scope);
    Analyser set_curr_ast_file(AstFile *curr_ast_file);
    Analyser set_curr_func(Ast *curr_func);
};



Analyser make_analyser(AstFile *curr_ast_file, Package *pkg, Array<Package> *all_packages);




void sema_analysis_all_packages(Array<Package> *all_packages);
TypeRef resolve_type(Ast *type_ast, Analyser analyser);



// NOTE: for evaluator.hpp/cpp
Value resolve_comptime_expr(Ast *expr, Analyser analyser, TypeRef target_typ = nullptr);
void resolve_const_decl_local(Ast *const_decl_ast, Analyser analyser, TypeRef target_type = nullptr);



#endif // CREST_ANALYSIS_HPP