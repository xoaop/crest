#include <stdio.h>
#include <ctype.h>

#include "tokenizer.hpp"
#include "common.hpp"
#include "string_map.hpp"

#define END_OF_CODE '\0'

//TODO(xoaop): 可能失效
#define TOKEN_SLICE_FIX(token_type) \
xp_slice_make(cast(void *)get_token_str(token_type), xp_strlen_c(get_token_str(token_type)))

// TokenType token_strings[] = {
//     #define TOKEN_INFO(type, str) type
//         TOKEN_INFOS
//     #undef TOKEN_INFO
// };

const char* token_strings[] = {
    #define TOKEN_INFO(type, str) str
        TOKEN_INFOS
    #undef TOKEN_INFO
};

xp_global StringHashMap<TokenType> keyword_map;

void test_keyword_map() {
    for(u8 i = __START__OF__KEYWORD__+1; i < __END__OF__KEYWORD__; i+=1) {
        StringMapEntry<TokenType> *entry = string_map_get_entry(keyword_map, xp_string_c(token_strings[i]));
        XP_ASSERT_MSG(cast(TokenType)i == entry->value, "test_keyword_map() FAILED: %s\n", token_strings[i]);
    }
}

void init_keyword_map() {
    keyword_map = string_map_make<TokenType>(permanent_allocator(), xp_array_len(token_strings));

    for(u8 i = __START__OF__KEYWORD__+1; i < __END__OF__KEYWORD__; i+=1) {
        string_map_insert(&keyword_map, xp_make_string(permanent_allocator(), token_strings[i]), cast(TokenType)i);
    }
}

xp_internal b32 is_letter(char c) {
    return isalpha(cast(u8)c) || c == '_';
}

xp_internal b32 is_letter_or_digit(char c) {
    return isalnum(cast(u8)c) || c == '_';
}


xp_internal const char *get_token_str(TokenType type) {
    return token_strings[type];
}


void tokenizer_init(Tokenizer *t, xpString code) {
    t->curr_character_index = 0;
    t->token_array = make_array<Token>(permanent_allocator());
    t->code = code;
    t->curr_line_index = 1;
    t->curr_column_index = 1;
}


xp_internal isize advance_one_character(Tokenizer *t);

void tokenize(Tokenizer *t) {
    Token temp;

    for(;;) {
        temp.token_str.capacity = 0;

        if(!tokenizer_get_token(t, &temp)) {
            break;
        }

        if(temp.type == TokenType::CommentLine) {
            continue;
        }

        array_push_back(&t->token_array, temp);
    }

    return;
}


// NOTE(xoaop): Key Function Of Tokenizer
b32 tokenizer_get_token(Tokenizer *t, Token *token) {
    //跳过空格
    tokenizer_skip_space(t);
    
    //结束
    if(tokenizer_end(t)) {
        return false;
    }
    
    token->line_index = t->curr_line_index;
    token->column_index = t->curr_column_index;
    
    isize old_index = t->curr_character_index;
    if(is_letter(tokenizer_curr_character(t))) {
        token->type = TokenType::Ident;
        while (is_letter_or_digit(tokenizer_curr_character(t))) {
            advance_one_character(t);
        }

        //TODO:(xoaop) 如果在keyword_map找到, 就是关键字, 如 i32, u32等
        xpString str = xp_make_string_capacity(permanent_allocator(), t->code.c_str + old_index, t->curr_character_index - old_index);
        TokenType *type = NULL;
        if((type = string_map_get(keyword_map, str)) != NULL) {
            token->type = *type;
        }
        token->token_str = str;


    } else {
        switch (tokenizer_curr_character(t)) {
        case '+':
            token->type = TokenType::Add;
            advance_one_character(t);

            if(tokenizer_curr_character(t) == '=') {
                token->type = TokenType::AddEqual;
                advance_one_character(t);
            }
            break;
        case '-':
            token->type = TokenType::Minus;
            advance_one_character(t);

            if(tokenizer_curr_character(t) == '>') {
                token->type = TokenType::Arrow;
                advance_one_character(t);
            } else if(tokenizer_curr_character(t) == '=') {
                token->type = TokenType::MinusEqual;
                advance_one_character(t);
            } else if(tokenizer_curr_character(t) == '-') {
                
                advance_one_character(t);
                if(tokenizer_curr_character(t) == '-') {
                    token->type = TokenType::TripleMinus;
                    advance_one_character(t);
                }
            }

            break;
        case '*':
            token->type = TokenType::Star;
            advance_one_character(t);
            
            if(tokenizer_curr_character(t) == '=') {
                token->type = TokenType::StarEqual;
                advance_one_character(t);
            }
            break;
        case '/':
            token->type = TokenType::ForwardSlash;
            advance_one_character(t);

            if(tokenizer_curr_character(t) == '=') {
                token->type = TokenType::ForwardSlashEqual;
                advance_one_character(t);
            } else if(tokenizer_curr_character(t) == '/') {
                
                token->type = TokenType::CommentLine;
                advance_one_character(t);
                while (!tokenizer_end(t) && tokenizer_curr_character(t) != '\n') {
                    advance_one_character(t);
                }
            }
            break;
        case '%':
            token->type = TokenType::Percent;
            advance_one_character(t);

            if(tokenizer_curr_character(t) == '=') {
                token->type = TokenType::PercentEqual;
                advance_one_character(t);
            }
            break;
        case '!':
            token->type = TokenType::Exclamation;
            advance_one_character(t);

            if(tokenizer_curr_character(t) == '=') {
                token->type = TokenType::ExclamationEqual;
                advance_one_character(t);
            }
            break;
        case '=':
            token->type = TokenType::Equal;
            advance_one_character(t);

            if(tokenizer_curr_character(t) == '=') {
                token->type = TokenType::DoubleEqual;
                advance_one_character(t);
            }
            break;
        case '>':
            token->type = TokenType::GreaterThan;
            advance_one_character(t);

            if(tokenizer_curr_character(t) == '=') {
                token->type = TokenType::GreaterEqual;
                advance_one_character(t);
            }
            break;
        case '<':
            token->type = TokenType::LessThan;
            advance_one_character(t);

            if(tokenizer_curr_character(t) == '=') {
                token->type = TokenType::LessEqual;
                advance_one_character(t);
            }
            break;
        case '&':
            token->type = TokenType::And;
            advance_one_character(t);

            if(tokenizer_curr_character(t) == '&') {
                token->type = TokenType::DoubleAnd;
                advance_one_character(t);
            }
            break;
        case '|':
            token->type = TokenType::DoubleOr;
            advance_one_character(t);

            if(tokenizer_curr_character(t) == '|') {
                token->type = TokenType::DoubleOr;
                advance_one_character(t);
            } else {
                goto unknown_character;
            }
            break;
        case ':':
            token->type = TokenType::Colon;
            advance_one_character(t);

            if(tokenizer_curr_character(t) == ':') {
                token->type = TokenType::DoubleColon;
                advance_one_character(t);
            } else if(tokenizer_curr_character(t) == '=') {
                token->type = TokenType::ColonEqual;
                advance_one_character(t);
            }

            break;
        case ';':
            token->type = TokenType::Semicolon;
            advance_one_character(t);
            break;
        case '(':
            token->type = TokenType::LeftBracket;
            advance_one_character(t);
            break;

        case ')':
            token->type = TokenType::RightBracket;
            advance_one_character(t);
            break;

        case '[':
            token->type = TokenType::LeftSquareBracket;
            advance_one_character(t);
            break;
            
        case ']':
            token->type = TokenType::RightSquareBracket;
            advance_one_character(t);
            break;
        
        case '{':
            token->type = TokenType::LeftCurlyBracket;
            advance_one_character(t);
            break;

        case '}':
            token->type = TokenType::RightCurlyBracket;
            advance_one_character(t);
            break;

        case ',': 
            token->type = TokenType::Comma;
            advance_one_character(t);
            break;

        case '.':
            token->type = TokenType::Dot;
            advance_one_character(t);
            break;

        case '"': {
            token->type = TokenType::StringLiteral;
            advance_one_character(t); //跳过开头的引号

            while (!tokenizer_end(t) && tokenizer_curr_character(t) != '"') {
                advance_one_character(t);
            }

            if(tokenizer_end(t)) {
                // false表示字符串未闭合就到达文件末尾, 这个token不会被加入token数组

                return false;
            }

            advance_one_character(t); //跳过结尾的引号

        } break;
            
        
        case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9': 
            tokenizer_scan_number(t, token, old_index);
            // token->type = TokenType::Integer;
            // tokenizer_scan_integer(t);
            break;

        
        default: 
        unknown_character: // TODO(xoaop): 不太好
            XP_ASSERT_MSG(0, "Unknown Character At Line %lld, Column %lld: %c\n", t->curr_line_index, t->curr_column_index, tokenizer_curr_character(t));
        }

        if(token->token_str.capacity == 0) {
            token->token_str = xp_make_string_capacity(permanent_allocator(), t->code.c_str + old_index, t->curr_character_index - old_index);
        }
    }

    return true;
}

char tokenizer_curr_character(Tokenizer *t) {
    return t->code.c_str[t->curr_character_index];
}

b32 tokenizer_end(Tokenizer *t) {
    return t->code.c_str[t->curr_character_index] == END_OF_CODE;
}

xp_internal isize advance_one_character(Tokenizer *t) {
    if(!tokenizer_end(t)) {
        t->curr_column_index += 1;
        if(tokenizer_curr_character(t) == '\n') {
            //NOTE(xoaop): RESET
            t->curr_line_index += 1;
            t->curr_column_index = 1;
        }

        t->curr_character_index += 1;
        return 1;
    }

    return 0;
}

isize advance_characters(Tokenizer *t, isize count) {
    isize advanced = 0;
    for(isize i = 0; i < count; i++) {
        advanced += advance_one_character(t);
    }
    return advanced;
}

char tokenizer_next_character(Tokenizer *t) {
    if(t->code.c_str[t->curr_character_index] == END_OF_CODE) {
        return END_OF_CODE;
    }

    return t->code.c_str[t->curr_character_index + 1];
}

isize tokenizer_move_until_next_space(Tokenizer *t) {
    isize count = 0;
    while (!tokenizer_end(t)) {

        if(xp_is_space(t->code.c_str[t->curr_character_index])) {
            break;
        }
        count += advance_one_character(t);
    }
    
    return count;
}

isize tokenizer_skip_space(Tokenizer *t) {
    isize count = 0;
    while (!tokenizer_end(t)) {
        if(!xp_is_space(tokenizer_curr_character(t))) {
            break;
        }
        count += advance_one_character(t);
    }
    return count;
}


void tokenizer_scan_integer(Tokenizer *t) {
    char c = tokenizer_curr_character(t);
    while (!tokenizer_end(t)) {
        if(!(xp_is_digit_base_all(c) || c == 'x')) {
            break;
        }
        advance_one_character(t);
        c = tokenizer_curr_character(t);
    }

    return;
}

isize tokenizer_try_to_fix(Tokenizer *t, const char *str) {
    isize curr_index = t->curr_character_index;
    defer(t->curr_character_index = curr_index);
    
    isize i = 0;
    for(i = 0; str[i] != '\0'; i++) {
        if(tokenizer_curr_character(t) != str[i]) {
            return 0;
        }
        advance_one_character(t);
    }
    
    return i;
}

bool is_number(char c) {
    return c == '0' || c == '1' || c == '2' || c == '3' || c == '4' || c == '5' || c == '6' || c == '7' || c == '8' || c == '9';
}


void tokenizer_scan_pure_integer_seq(Tokenizer *t) {
    char c = tokenizer_curr_character(t);
    while (!tokenizer_end(t)) {
        if(!is_number(c)) {
            break;
        }
        advance_one_character(t);
        c = tokenizer_curr_character(t);
    }
}


void tokenizer_scan_number(Tokenizer *t, Token *token, isize old_index) {
    TokenType token_type;
    TypeKind type_kind;

    switch(tokenizer_curr_character(t))
    {
    case '0': {
        advance_one_character(t);

        if(tokenizer_curr_character(t) == 'x') {
            advance_one_character(t);
            tokenizer_scan_pure_integer_seq(t);
        }

        token_type = TokenType::Integer;
    } break;

    case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9': {
        tokenizer_scan_pure_integer_seq(t);

        if(tokenizer_curr_character(t) == '.') {
            advance_one_character(t);
            tokenizer_scan_pure_integer_seq(t);
            token_type = TokenType::Float;
        } else {
            token_type = TokenType::Integer;
        }

    } break;

    default:
        XP_ASSERT_DEFAULT(0);
        break;
    }



    // 解析类型后缀
    isize len = 0;
    const char *postfix[] = {
        "i8", "i32", "i64",
        "u8", "u32", "u64",
        "f32", "f64"
    };

    isize i = 0;
    for(i = 0; i < xp_array_len(postfix); i++) {
        len = tokenizer_try_to_fix(t, postfix[i]);
        if(len > 0) {
            break;
        }
    }

    // 浮点数不能用整型的后缀
    if(token_type == TokenType::Float) {
        if(len > 0 && !(i == 6 || i == 7)) {
            XP_ASSERT_DEFAULT(0);
        }
    }
    // 整型不能用浮点数的后缀
    if(token_type == TokenType::Integer) {
        if(len > 0 && (i == 6 || i == 7)) {
            XP_ASSERT_DEFAULT(0);
        }
    }
    
    
    
    if(i < 8) {
        token->type_kind_of_number = string_to_type_kind(xp_string_c(postfix[i]));
    } else {
        token->type_kind_of_number = Type_Undefined;
    }

    token->type = token_type;
    
    token->token_str = xp_make_string_capacity(permanent_allocator(), t->code.c_str + old_index, t->curr_character_index - old_index);
    advance_characters(t, len);
    return;
}