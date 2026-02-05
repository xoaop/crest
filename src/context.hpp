#ifndef CREST_CONTEXT_HPP
#define CREST_CONTEXT_HPP

#include "package.hpp"

struct Context {
    // Scope global_scope;

    const char *main_src_dir_path;

    Package global_blank_package;
};

Context *context();


#endif // CREST_CONTEXT_HPP