#ifndef CREST_CONTEXT_HPP
#define CREST_CONTEXT_HPP

#include "package.hpp"
#include "error_msg.hpp"

struct Context {
    const char *main_src_dir_path;

    Package global_blank_package;

    ErrorReporter reporter;
};

Context *context();


#endif // CREST_CONTEXT_HPP