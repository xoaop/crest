#include <stdio.h>

extern int add(int a, int b);
extern int mul(int a, int b);

int main(void) {
    if(add(2, 3) != 5) {
        fprintf(stderr, "add(2, 3) = %d\n", add(2, 3));
        return 1;
    }

    if(mul(4, 5) != 20) {
        fprintf(stderr, "mul(4, 5) = %d\n", mul(4, 5));
        return 1;
    }

    return 0;
}
