#ifndef CREST_PARSER_H
#define CREST_PARSER_H

#include "xoaop.h"
#include "tokenizer.hpp"
#include "array.hpp"
#include "common.hpp"

#include "ast.hpp"
#include "ast_file.hpp"


xp_internal b32 is_binary_op(TokenType type) {
    switch (type)
    {
    case Add:
    case Minus:
    case Star:
    case ForwardSlash:
    case Percent:
    case GreaterThan:
    case GreaterEqual:
    case LessThan:
    case LessEqual:
    case DoubleEqual:
    case ExclamationEqual:
    case DoubleAnd:
    case DoubleOr:
        return true;
    default:
        return false;
    }

    return false;
}

xp_internal b32 is_unary_op(TokenType type) {
    switch (type)
    {
    case TokenType::Minus: // -
    case TokenType::Exclamation: // !
    
    // 指针运算
    case TokenType::And: // &
    case TokenType::Star: // *
        return true;
    default:
        return false;
    }
    return false;
}


//NOTE(xoaop): 越大 优先级越高
xp_internal isize precedence(TokenType op, bool is_unary_op = false) {

    //NOTE: 13 - 13 = 0
    isize prec = 13;

    //NOTE(xoaop): 越小, 优先级越高
    switch (op) {
    case TokenType::Star:              // *
    case TokenType::ForwardSlash:      // /
    case TokenType::Percent:           // %
        prec = 3;
        break;
    case TokenType::Add:               // +
    case TokenType::Minus:             // - 
        prec = 4;
        break;
    case TokenType::GreaterThan:       // >
    case TokenType::GreaterEqual:      // >=
    case TokenType::LessThan:          // <
    case TokenType::LessEqual:         // <=
        prec = 6;
        break;
    case TokenType::DoubleEqual:       // ==
    case TokenType::ExclamationEqual:  // !=
        prec = 7;
        break;
    case TokenType::DoubleAnd:         // &&
        prec = 11;
        break;
    case TokenType::DoubleOr:          // ||
        prec = 12;
        break;
    default:
        XP_ASSERT_MSG(0, "Unknown operator precedence for %s\n", token_strings[cast(i32) op]);
    }

    //NOTE: 13 - 13 = 0
    return 13 - prec;
}




struct Parser {
    isize curr_token_index;
    Array<Token> tokens;
    AstFile f;
};



Parser parser_make(Array<Token> tokens);
void parser_init(Parser *parser, Array<Token> tokens);

AstFile parse_file(Array<Token> tokens);





#endif