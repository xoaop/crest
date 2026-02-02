#ifndef CREST_RESOLVE_DEPEND_HPP
#define CREST_RESOLVE_DEPEND_HPP

#include "xoaop.h"
#include "package.hpp"


Array<Package> resolve_dependencies(xpString main_dir_path);

#endif