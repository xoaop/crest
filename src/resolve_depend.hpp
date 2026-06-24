#ifndef CREST_RESOLVE_DEPEND_HPP
#define CREST_RESOLVE_DEPEND_HPP

#include "xoaop.h"
#include "package.hpp"


Array<Package> resolve_dependencies(xpString main_dir_path);


xpOption<xpString> resolve_package_path(xpString import_path);
xpOption<Package *> get_package_by_import(xpString import_path, Array<Package> *all_packages);

#endif