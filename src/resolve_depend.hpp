#ifndef CREST_RESOLVE_DEPEND_HPP
#define CREST_RESOLVE_DEPEND_HPP

#include "xoaop.h"
#include "package.hpp"


void resolve_dependencies(xpString main_dir_path, Array<Package> &out_packages);

Package tokenize_and_parse_package(const char *path_of_package_dir);

xpOption<xpString> resolve_package_path(xpString import_path, xpAllocator allocator);
xpOption<Package *> get_package_by_import(xpString import_path, Array<Package> *all_packages);

#endif