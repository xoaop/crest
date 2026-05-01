#pragma once

#include "analyser.hpp"


void collect_top_level_symbols_in_package(Package *pkg, Array<Package> *all_packages);
void collect_top_level_symbols_in_file(AstFile *ast_file, Package *curr_pkg, Array<Package> *all_packages);
void collect_const_decl_symbol(Ast *const_decl_ast, Analyser analyser);
void collect_var_decl_symbol(Ast *var_decl_ast, Analyser analyser);




void eval_unsolved_in_symbol_table(SymbolInfo *unsolved_symbol, Analyser analyser);
void eval_unsolved_const_decl(SymbolInfo *unsolved_symbol, Analyser analyser);
void eval_unsolved_var_decl(SymbolInfo *unsolved_symbol, Analyser analyser);



Value eval_import_value(Ast *import_ast, Analyser analyser);
Value eval_function_decl_value_only_type(Ast *fn_val_ast, Analyser analyser);
void eval_struct_decl_value_in_symbol_table(SymbolInfo *struct_symbol_info, Analyser analyser);
ValueResult eval_comptime_expr(Ast *expr, Analyser analyser, bool for_var_expr = false);
Value eval_struct_decl_value(Ast *struct_decl_ast, xpString ident, Analyser analyser);
Value eval_enum_decl(Ast *enum_decl_ast, xpString ident, Analyser analyser);