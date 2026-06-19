///
// TODO: UTF-8: 目前还不支持utf-8中非ascii字符当作一个character, 
// 会导致中文后面的token的位置信息不正确(中文占不止一个character, 但编辑器里是当作一个位置的), 
// eg: 

// test\fail\infer_expr_type_all_error\1.cst:108:26: Error: expression cannot be indexed because it is not an array, slice struct or string struct type
//         你好世界a := 1[0]; 
//            应: ^    实际:^^^^

// 需要改成支持utf-8的character
// 不过至少现在还没有影响, 毕竟只有单行注释, 中文后面不可能有token了
// 以后如果支持多行注释或者字符串字面量里允许有中文, 就必须支持utf-8的character了
///






#include <stdio.h>
#include <ctype.h>

#include "tokenizer.hpp"
#include "common.hpp"

#include "context.hpp"

#define END_OF_CODE '\0'


const char* token_strings[] = {
    #define TOKEN_INFO(type, str) str
        TOKEN_INFOS
    #undef TOKEN_INFO
};

xp_global xpHashMap<xpString, TokenType> keyword_map;



char tokenizer_next_character(Tokenizer *t);
isize advance_characters(Tokenizer *t, isize count);



void test_keyword_map() {
    for(u8 i = __START__OF__KEYWORD__+1; i < __END__OF__KEYWORD__; i+=1) {
        xpHashMapEntry<xpString, TokenType> *entry = xp_hash_map_get_entry(keyword_map, xp_string_c(token_strings[i]));
        XP_ASSERT_MSG(cast(TokenType)i == entry->value, "test_keyword_map() FAILED: %s\n", token_strings[i]);
    }
}

void init_keyword_map() {
    keyword_map = xp_hash_map_make<xpString, TokenType>(permanent_allocator());

    for(u8 i = __START__OF__KEYWORD__+1; i < __END__OF__KEYWORD__; i+=1) {
        xp_hash_map_insert(&keyword_map, xp_make_string(permanent_allocator(), token_strings[i]), cast(TokenType)i);
    }
}

xp_internal b32 is_letter(char c) {
    return isalpha(cast(u8)c) || c == '_';
}

xp_internal b32 is_letter_or_digit(char c) {
    return isalnum(cast(u8)c) || c == '_';
}

bool is_number(char c) {
    return c == '0' || c == '1' || c == '2' || c == '3' || c == '4' || c == '5' || c == '6' || c == '7' || c == '8' || c == '9';
}



xp_internal const char *get_token_str(TokenType type) {
    return token_strings[type];
}



void tokenizer_init(Tokenizer *t, xpString file_path, xpString code) {
    t->curr_character_index = 0;
    t->token_array = make_array<Token>(permanent_allocator());
    t->source_code = make_source_code(file_path, code, permanent_allocator());
}


xp_internal isize advance_one_character(Tokenizer *t);

xpPair<SourceCode, Array<Token>> tokenize(xpString file_path, xpString code) {

    Tokenizer t;
    tokenizer_init(&t, file_path, code);


    for(;;) {
        auto result = tokenizer_get_token(&t);
        
        
        
        xpOption<Token> token_option = result.first;
        if(token_option.has_value()) {
            Token token = token_option.unwrap();
            
            if(token.type == TokenType::CommentLine) {
                // 注释行不加入token数组
            } else {
                array_push_back(&t.token_array, token);
            }
            
        }


        bool has_next = result.second;
        if(!has_next) {

            // End Of Tokens
            Token end = {};
            end.type = TokenType::EndOfTokens;
            end.src_loc = SourceLocation(t.source_code, make_span(t.source_code.code_string.length, t.source_code.code_string.length));


            array_push_back(&t.token_array, end);
            break;
        }
    }

    #ifdef DEBUG_PRINT 
    printf("===== TOKEN LIST START =====\n");
    for(isize i = 0; i < t.token_array.count; i++) {
        Token *token = &t.token_array[i];

        auto start = cal_line_column_index_of_byte_pos(token->src_loc.src_code, token->src_loc.span.start);
        auto end = cal_line_column_index_of_byte_pos(token->src_loc.src_code, token->src_loc.span.end);
        printf("Token[%3lld]: Type: %-20s Str: %-15.*s Span: [%lld:%lld - %lld:%lld] BytePos[%lld - %lld]\n",
            i,
            get_token_str(token->type),
            cast(int)token->token_str.length, token->token_str.c_str,
            start.first, start.second,
            end.first, end.second,
            token->src_loc.span.start, token->src_loc.span.end
        );
        
    }
    printf("===== TOKEN LIST END =====\n");
    #endif // DEBUG_PRINT


    return xp_make_pair(t.source_code, t.token_array);
}


// NOTE(xoaop): Key Function Of Tokenizer
xpPair<xpOption<Token>, bool> tokenizer_get_token(Tokenizer *t) {
    //跳过空格
    tokenizer_skip_space(t);
    
    //结束
    if(tokenizer_end(t)) {
        return xp_make_pair(xpOption<Token>::none(), false);
    }
    
    Token token = {};
    

    isize old_index = t->curr_character_index;
    if(is_letter(tokenizer_curr_character(t))) {
        token.type = TokenType::Ident;
        while (is_letter_or_digit(tokenizer_curr_character(t))) {
            advance_one_character(t);
        }

        //TODO:(xoaop) 如果在keyword_map找到, 就是关键字, 如 i32, u32等
        xpString str = xp_make_string_capacity(permanent_allocator(), t->source_code.code_string.c_str + old_index, t->curr_character_index - old_index);
        TokenType *type = NULL;
        if((type = xp_hash_map_get(keyword_map, str)) != NULL) {
            token.type = *type;
        }
        token.token_str = str;

    } else {
        switch (tokenizer_curr_character(t)) {
        case '+':
            token.type = TokenType::Add;
            advance_one_character(t);

            if(tokenizer_curr_character(t) == '=') {
                token.type = TokenType::AddEqual;
                advance_one_character(t);
            }
            break;
        case '-':
            token.type = TokenType::Minus;
            advance_one_character(t);

            if(is_number(tokenizer_curr_character(t))) {
                tokenizer_scan_number(t, &token, old_index);
                break;
            }


            if(tokenizer_curr_character(t) == '>') {
                token.type = TokenType::Arrow;
                advance_one_character(t);
            } else if(tokenizer_curr_character(t) == '=') {
                token.type = TokenType::MinusEqual;
                advance_one_character(t);
            } else if(tokenizer_curr_character(t) == '-') {
                
                advance_one_character(t);
                if(tokenizer_curr_character(t) == '-') {
                    token.type = TokenType::TripleMinus;
                    advance_one_character(t);
                }
            }

            break;
        case '*':
            token.type = TokenType::Star;
            advance_one_character(t);
            
            if(tokenizer_curr_character(t) == '=') {
                token.type = TokenType::StarEqual;
                advance_one_character(t);
            }
            break;
        case '/':
            token.type = TokenType::ForwardSlash;
            advance_one_character(t);

            if(tokenizer_curr_character(t) == '=') {
                token.type = TokenType::ForwardSlashEqual;
                advance_one_character(t);
            } else if(tokenizer_curr_character(t) == '/') {
                
                token.type = TokenType::CommentLine;
                advance_one_character(t);
                while (!tokenizer_end(t) && tokenizer_curr_character(t) != '\n') {
                    advance_one_character(t);
                }
            }
            break;
        case '%':
            token.type = TokenType::Percent;
            advance_one_character(t);

            if(tokenizer_curr_character(t) == '=') {
                token.type = TokenType::PercentEqual;
                advance_one_character(t);
            }
            break;
        case '!':
            token.type = TokenType::Exclamation;
            advance_one_character(t);

            if(tokenizer_curr_character(t) == '=') {
                token.type = TokenType::ExclamationEqual;
                advance_one_character(t);
            }
            break;
        case '=':
            token.type = TokenType::Equal;
            advance_one_character(t);

            if(tokenizer_curr_character(t) == '=') {
                token.type = TokenType::DoubleEqual;
                advance_one_character(t);
            }
            break;
        case '>':
            token.type = TokenType::GreaterThan;
            advance_one_character(t);

            if(tokenizer_curr_character(t) == '=') {
                token.type = TokenType::GreaterEqual;
                advance_one_character(t);
            }
            break;
        case '<':
            token.type = TokenType::LessThan;
            advance_one_character(t);

            if(tokenizer_curr_character(t) == '=') {
                token.type = TokenType::LessEqual;
                advance_one_character(t);
            }
            break;
        case '&':
            token.type = TokenType::And;
            advance_one_character(t);

            if(tokenizer_curr_character(t) == '&') {
                token.type = TokenType::DoubleAnd;
                advance_one_character(t);
            }
            break;
        case '|':
            token.type = TokenType::DoubleOr;
            advance_one_character(t);

            if(tokenizer_curr_character(t) == '|') {
                token.type = TokenType::DoubleOr;
                advance_one_character(t);
            }
            break;
        case ':':
            token.type = TokenType::Colon;
            advance_one_character(t);

            if(tokenizer_curr_character(t) == ':') {
                token.type = TokenType::DoubleColon;
                advance_one_character(t);
            } else if(tokenizer_curr_character(t) == '=') {
                token.type = TokenType::ColonEqual;
                advance_one_character(t);
            }

            break;
        case ';':
            token.type = TokenType::Semicolon;
            advance_one_character(t);
            break;
        case '(':
            token.type = TokenType::LeftBracket;
            advance_one_character(t);
            break;

        case ')':
            token.type = TokenType::RightBracket;
            advance_one_character(t);
            break;

        case '[':
            token.type = TokenType::LeftSquareBracket;
            advance_one_character(t);
            break;
            
        case ']':
            token.type = TokenType::RightSquareBracket;
            advance_one_character(t);
            break;
        
        case '{':
            token.type = TokenType::LeftCurlyBracket;
            advance_one_character(t);
            break;

        case '}':
            token.type = TokenType::RightCurlyBracket;
            advance_one_character(t);
            break;

        case ',': 
            token.type = TokenType::Comma;
            advance_one_character(t);
            break;

        case '.':
            token.type = TokenType::Dot;
            advance_one_character(t);

            if(tokenizer_curr_character(t) == '.' && tokenizer_next_character(t) == '.') {
                token.type = TokenType::ThreeDots;
                advance_characters(t, 2);
            }
            break;

        case '^':
            token.type = TokenType::Caret;
            advance_one_character(t);
            break;

        case '"': {
            token.type = TokenType::StringLiteral;
            advance_one_character(t); //跳过开头的引号

            while (!tokenizer_end(t) && tokenizer_curr_character(t) != '"') {
                advance_one_character(t);
            }

            if(tokenizer_end(t)) {
                // false表示字符串未闭合就到达文件末尾, 这个token不会被加入token数组

                context()->reporter.report(
                    ErrorLevel::Error,
                    SourceLocation(t->source_code, make_span(old_index, t->curr_character_index)),
                    "string literal not closed"
                );

                return xp_make_pair(xpOption<Token>::none(), false);
            }

            advance_one_character(t); //跳过结尾的引号

        } break;
            
        
        case '0': case '1': case '2': case '3': case '4': case '5': case '6': case '7': case '8': case '9': 
            tokenizer_scan_number(t, &token, old_index);
            // token.type = TokenType::Integer;
            // tokenizer_scan_integer(t);
            break;

        
        default: 
        unknown_character: // TODO(xoaop): 不太好
            // XP_ASSERT_MSG(0, "Unknown Character At Line %lld, Column %lld: %c\n", t->curr_line_index, t->curr_column_index, tokenizer_curr_character(t));
            // XP_ASSERT_DEFAULT(0);

            context()->reporter.report(
                ErrorLevel::Error,
                SourceLocation(t->source_code, make_span(old_index, t->curr_character_index)),
                "unknown character '{}'",
                tokenizer_curr_character(t)
            );
            advance_one_character(t);

            return xp_make_pair(xpOption<Token>::none(), true);
        }

        token.token_str = xp_make_string_capacity(permanent_allocator(), t->source_code.code_string.c_str + old_index, t->curr_character_index - old_index);
    }


    token.src_loc = SourceLocation(t->source_code, make_span(old_index, t->curr_character_index));

    return xp_make_pair(xpOption<Token>(token), true);
}

char tokenizer_curr_character(Tokenizer *t) {
    return t->source_code.code_string.c_str[t->curr_character_index];
}

b32 tokenizer_end(Tokenizer *t) {
    return t->source_code.code_string.c_str[t->curr_character_index] == END_OF_CODE;
}

xp_internal isize advance_one_character(Tokenizer *t) {
    if(!tokenizer_end(t)) {
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
    if(t->source_code.code_string.c_str[t->curr_character_index] == END_OF_CODE) {
        return END_OF_CODE;
    }

    return t->source_code.code_string.c_str[t->curr_character_index + 1];
}

isize tokenizer_move_until_next_space(Tokenizer *t) {
    isize count = 0;
    while (!tokenizer_end(t)) {

        if(xp_is_space(t->source_code.code_string.c_str[t->curr_character_index])) {
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
            token_type = TokenType::Integer;
        } else if(tokenizer_curr_character(t) == '.') {
            advance_one_character(t);
            tokenizer_scan_pure_integer_seq(t);
            token_type = TokenType::Float;
        } else {
            token_type = TokenType::Integer;
        }

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
        token->number_info.type_kind_of_number = string_to_type_kind(xp_string_c(postfix[i]));
    } else {
        token->number_info.type_kind_of_number = Type_Undefined;
    }

    token->type = token_type;

    token->number_info.pure_number_str = xp_make_string_capacity(permanent_allocator(), t->source_code.code_string.c_str + old_index, t->curr_character_index - old_index);
    
    
    advance_characters(t, len);


    return;
}