#ifndef CREST_RESOLVE_DEPEND_HPP
#define CREST_RESOLVE_DEPEND_HPP

#include "xoaop.h"
#include "package.hpp"


Array<Package> resolve_dependencies(xpString main_dir_path);


xpString import_path_to_package_path(xpOption<xpString> search_prefix, xpString import_path, xpAllocator allocator);
xpOption<Package *> get_package_by_import(xpOption<xpString> search_prefix, xpString import_path, Array<Package> *all_packages);

#endif