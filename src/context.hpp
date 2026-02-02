#ifndef CREST_CONTEXT_HPP
#define CREST_CONTEXT_HPP

#include "scope.hpp"

struct Context {
    Scope global_scope;

    const char *main_src_dir_path;
};

Context *context();


#endif // CREST_CONTEXT_HPP