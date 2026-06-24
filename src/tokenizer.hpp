#ifndef CREST_TOKENIZER_H
#define CREST_TOKENIZER_H

#include "xoaop.h"
#include "array.hpp"

#include "type.hpp"

#include "span.hpp"
#include "source_code.hpp"

// NOTE: Keyword可以直接加在TOKEN_INFO不用修改别的地方的
// 而别的Token需要在tokenizer加上处理逻辑
#define TOKEN_INFOS                                             \
    TOKEN_INFO(DoubleForwardSlash, "//"),                       \
    TOKEN_INFO(CommentLine, "comment line"),                    \
	TOKEN_INFO(Add, "+"),                                       \
	TOKEN_INFO(Minus,     "-"),                                 \
	TOKEN_INFO(Star, "*"),                                      \
	TOKEN_INFO(ForwardSlash, "/"),                              \
    TOKEN_INFO(Percent, "%"),                                   \
    TOKEN_INFO(AddEqual, "+="),                                 \
    TOKEN_INFO(MinusEqual, "-="),                               \
    TOKEN_INFO(StarEqual, "*="),                                \
    TOKEN_INFO(ForwardSlashEqual, "/="),                        \
    TOKEN_INFO(PercentEqual, "%="),                             \
    TOKEN_INFO(Exclamation, "!"),                               \
    TOKEN_INFO(Equal, "="),                                     \
    TOKEN_INFO(DoubleEqual, "=="),                              \
    TOKEN_INFO(ExclamationEqual, "!="),                         \
    TOKEN_INFO(GreaterThan, ">"),                               \
    TOKEN_INFO(LessThan, "<"),                                  \
    TOKEN_INFO(GreaterEqual, ">="),                             \
    TOKEN_INFO(LessEqual, "<="),                                \
    TOKEN_INFO(And, "&"),                                       \
    TOKEN_INFO(DoubleAnd, "&&"),                                \
    TOKEN_INFO(DoubleOr, "||"),                                 \
    TOKEN_INFO(Colon, ":"),                                     \
    TOKEN_INFO(Semicolon, ";"),                                 \
    TOKEN_INFO(DoubleColon, "::"),                              \
    TOKEN_INFO(ColonEqual, ":="),                               \
    TOKEN_INFO(LeftBracket, "("),                               \
    TOKEN_INFO(RightBracket, ")"),                              \
    TOKEN_INFO(LeftSquareBracket, "["),                         \
    TOKEN_INFO(RightSquareBracket, "]"),                        \
    TOKEN_INFO(LeftCurlyBracket, "{"),                          \
    TOKEN_INFO(RightCurlyBracket, "}"),                         \
    TOKEN_INFO(Comma, ","),                                     \
    TOKEN_INFO(Arrow, "->"),                                    \
    TOKEN_INFO(Dot, "."),                                       \
    TOKEN_INFO(Caret, "^"),                                     \
    TOKEN_INFO(TripleMinus, "---"),                             \
    TOKEN_INFO(Integer, "interger"),                            \
    TOKEN_INFO(Float, "float"),                                 \
    TOKEN_INFO(Ident, "ident"),                                 \
    TOKEN_INFO(StringLiteral, "string literal"),                \
    TOKEN_INFO(ThreeDots, "..."),                               \
    TOKEN_INFO(__START__OF__KEYWORD__, ""),                     \
    TOKEN_INFO(KW_struct, "struct"),                            \
    TOKEN_INFO(KW_enum, "enum"),                                \
    TOKEN_INFO(KW_union, "union"),                              \
    TOKEN_INFO(KW_if, "if"),                                    \
    TOKEN_INFO(KW_else, "else"),                                \
    TOKEN_INFO(KW_for, "for"),                                  \
    TOKEN_INFO(KW_switch, "switch"),                            \
    TOKEN_INFO(KW_case, "case"),                                \
    TOKEN_INFO(KW_return, "return"),                            \
    TOKEN_INFO(KW_break, "break"),                              \
    TOKEN_INFO(KW_continue, "continue"),                        \
    TOKEN_INFO(KW_cast, "cast"),                                \
    TOKEN_INFO(KW_true, "true"),                                \
    TOKEN_INFO(KW_false, "false"),                              \
    TOKEN_INFO(KW_null, "null"),                                \
    TOKEN_INFO(KW_import, "import"),                            \
    /*      */                                                  \
    /* TEMP */                                                  \
    TOKEN_INFO(KW_extern_C, "extern_C"),                        \
    TOKEN_INFO(__END__OF__KEYWORD__, ""),                       \
    TOKEN_INFO(EndOfTokens, "end of tokens")                    \
/**/


extern const char* token_strings[];

enum TokenType: u8 {
    #define TOKEN_INFO(type, str) type
    TOKEN_INFOS
    #undef TOKEN_INFO
};



struct Token {
    TokenType type;
    xpString token_str;

    union {
        struct {
            xpString pure_number_str;     // 不带类型后缀的纯数字字符串
            TypeKind type_kind_of_number; // 用于标记带类型后缀的数字(整数, 浮点数)
        } number_info;

    };

    xpString file_path;
    isize line_index;
    isize column_index;

    SourceLocation src_loc;
};


Array<Token> tokenize(SourceCode *src_code);


void init_keyword_map();
void test_keyword_map();


#endif