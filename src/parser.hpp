#ifndef CREST_PARSER_H
#define CREST_PARSER_H

#include "xoaop.h"
#include "tokenizer.hpp"
#include "array.hpp"

#include "ast.hpp"

#include "source_code.hpp"


Array<Ast *> parse(Array<Token> tokens, SourceCode *src_code);


#endif