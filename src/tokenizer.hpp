#ifndef CREST_TOKENIZER_H
#define CREST_TOKENIZER_H

#include "xoaop.h"
#include "array.hpp"

#include "type.hpp"

// NOTE: Keyword是可以直接加在TOKEN_INFO不用修改别的地方的
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
    TOKEN_INFO(LeftCurlyBracket, "{"),                          \
    TOKEN_INFO(RightCurlyBracket, "}"),                         \
    TOKEN_INFO(Comma, ","),                                     \
    TOKEN_INFO(Arrow, "->"),                                    \
    TOKEN_INFO(Integer, "interger"),                            \
    TOKEN_INFO(Float, "float"),                                 \
    TOKEN_INFO(Ident, "ident"),                                 \
    TOKEN_INFO(__START__OF__KEYWORD__, ""),                     \
    TOKEN_INFO(KW_void, "void"),                                \
    TOKEN_INFO(KW_bool, "bool"),                                \
    TOKEN_INFO(KW_i8, "i8"),                                    \
    TOKEN_INFO(KW_i32, "i32"),                                  \
    TOKEN_INFO(KW_i64, "i64"),                                  \
    TOKEN_INFO(KW_u8, "u8"),                                    \
    TOKEN_INFO(KW_u32, "u32"),                                  \
    TOKEN_INFO(KW_u64, "u64"),                                  \
    TOKEN_INFO(KW_f32, "f32"),                                  \
    TOKEN_INFO(KW_f64, "f64"),                                  \
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
    TOKEN_INFO(__END__OF__KEYWORD__, "")                        \
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
        TypeKind type_kind_of_number; // 用于标记带类型后缀的数字(整数, 浮点数)
    };
    
    xpString file_path;
    isize line_index;
    isize column_index;
};


struct Tokenizer {
    isize curr_character_index;
    Array<Token> token_array;
    xpString code;

    isize curr_line_index;
    isize curr_column_index;
};

void tokenizer_init(Tokenizer* t, xpString code);
void tokenize(Tokenizer* t);
b32 tokenizer_get_token(Tokenizer* t, Token *token);
char tokenizer_curr_character(Tokenizer *t);
b32 tokenizer_end(Tokenizer *t);
isize tokenizer_move_until_next_space(Tokenizer *t);
isize tokenizer_skip_space(Tokenizer *t);
void tokenizer_scan_integer(Tokenizer *t);
void tokenizer_scan_number(Tokenizer *t, Token *token, isize old_index);

void init_keyword_map();
void test_keyword_map();


xp_internal xpString file_to_string(char const *path, xpAllocator allocator) {
    FILE *file = NULL;
    
    if(fopen_s(&file, path, "r")) {
        printf("Read File Failed: %s\n", path);
    }

    fseek(file, 0, SEEK_END);
    isize size = ftell(file);
    rewind(file);

    xpString str = xp_make_string_capacity(allocator, NULL, size);
    fread(str.c_str, 1, size, file);

    fclose(file);

    return str;
}

#endif