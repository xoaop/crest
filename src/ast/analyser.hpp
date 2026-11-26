#pragma once

#include "xoaop.h"
#include "array.hpp"

#include "ast.hpp"
#include "scope.hpp"


struct Package;
struct AstFile;



void resolve_package(PackageRef pkg);


// 注册基础类型符号
void init_global_symbols();
