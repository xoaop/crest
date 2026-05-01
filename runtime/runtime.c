#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <windows.h>

void print(const char *data, uint64_t len) {
    fwrite(data, 1, len, stdout);
}


void fflush_stdout() {
    fflush(stdout);
}


void system_cls() {
    system("cls");
}

void sleep_ms(int ms) {
    Sleep(ms);
}