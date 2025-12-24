#ifndef CREST_SCOPE_HPP
#define CREST_SCOPE_HPP

#include "string_map.hpp"

struct Scope {
    StringHashMap<Ast *> symbol_map;
};



#endif // CREST_SCOPE_HPP