#ifndef CREST_ANALYSER_HPP
#define CREST_ANALYSER_HPP

#include "xoaop.h"
#include "array.hpp"

#include "ast.hpp"
#include "scope.hpp"


struct Package;
struct AstFile;

struct Analyser {
    PackageRef pkg = -1;
    Scope *current_scope;
    AstFile *curr_ast_file;
};

Analyser make_analyser(AstFile *curr_ast_file, PackageRef pkg);
Analyser make_analyser(Scope *curr_scope, PackageRef pkg);
Analyser make_analyser(Scope *curr_scope, AstFile *file, PackageRef pkg);


void resolve_package(PackageRef pkg);


// 注册基础类型符号
void init_global_symbols();


#endif // CREST_ANALYSER_HPP