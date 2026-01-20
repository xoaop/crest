#include <cstdarg>

#include "error_msg.hpp"

void error_msg(Token *token, const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    if(token != NULL) {
        printf("Error at %s:%td:%td: ", token->file_path.c_str, token->line_index, token->column_index);
    }
    vprintf(fmt, args);
    printf("\n");

    va_end(args);
}