#ifndef CREST_ANALYSER_HPP
#define CREST_ANALYSER_HPP

#include "xoaop.h"
#include "string_map.hpp"

#include "parser.hpp"

#include "symbol.hpp"




struct Analyser {
    // TODO(xoaop): Analyser state

    
    Array<SymbolTable> symbol_table_stack;

    // 循环体栈
    Array<Ast *> loop_ast_stack;
};


void push_symbol_table(Analyser *analyser);
void pop_symbol_table(Analyser *analyser);

bool at_global_scope(Analyser *analyser);

#define new_scope(analyser)                                                                  \
    push_symbol_table(analyser);                                                             \
    defer(pop_symbol_table(analyser));                                                       \
/**/


void semantic_analysis_ast_file(AstFile *ast_file);
void resolve_ast_file(AstFile *ast_file, Analyser *analyser);


//
// Ast Visitor
//
typedef void (*AstVisitorFunc)(Ast *ast, Analyser *analyser);

void ast_visitor(Array<Ast *> ast_array, AstVisitorFunc visit_func[AstType_COUNT], Analyser *analyser);
void ast_visitor(Ast *ast, AstVisitorFunc visit_func[AstType_COUNT], Analyser *analyser);



#endif // CREST_ANALYSIS_HPP