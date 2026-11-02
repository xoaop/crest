#include <stdint.h>

/* 与 1.cst 的 struct 定义一一对应，由 C 侧声明并调用 Crest 导出的函数 */

typedef struct { int64_t a, b, c; } S24;
typedef struct { signed char a, b, c; } S3;
typedef struct { signed char a0, a1, a2, a3, a4, a5, a6, a7, a8; } S9;
typedef union  { int a; float b; } U;
typedef struct { int64_t a; double b; } MIX;
typedef struct { float x, y, z, w; } F4;
typedef struct { float x; double y; } FD;
typedef struct { int a; float b; } IF;
typedef struct { int a[2]; double d; } NA;
typedef struct { double x, y; } V2;
typedef struct { int a, b; } A2;
typedef struct { int64_t a, b; } P16;
typedef struct { double x, y; } D2;
typedef struct { int a, b, c; } E12;

S24     s24add_def(S24 a, S24 b);
int64_t s3sum_def(S3 v);
int64_t s9sum_def(S9 v);
int     uadd_def(U a, U b);
double  mixsum_def(MIX v);
float   f4sum_def(F4 v);
double  fdsum_def(FD v);
int     ifsum_def(IF v);
int     nasum_def(NA v);
V2      v2add_def(V2 a, V2 b);
double  v2sum_def(V2 v);
int     a2sum_def(A2 v);
int64_t small_def(signed char a, short b, unsigned char c, unsigned short d);
int64_t many_def(int64_t a, int64_t b, int64_t c, int64_t d, int64_t e, int64_t f, P16 p, int64_t g);
double  manyd_def(double a, double b, double c, double d, double e, double f, double g, double h, D2 p, double z);
S3      s3id_def(S3 v);
E12     e12id_def(E12 v);

int run_checks(void) {
    S24 a = {1, 2, 3}, b = {10, 20, 30};
    S24 r = s24add_def(a, b);
    if(r.a != 11 || r.b != 22 || r.c != 33) return 1;

    S3 v3 = {1, 2, 3};
    if(s3sum_def(v3) != 6) return 2;

    S9 v9 = {1, 2, 3, 4, 5, 6, 7, 8, 9};
    if(s9sum_def(v9) != 45) return 3;

    U u; u.a = 42;
    if(uadd_def(u, u) != 84) return 4;

    MIX m = {5, 2.5};
    if(mixsum_def(m) != 7.5) return 5;

    F4 f4 = {1.0f, 2.0f, 3.0f, 4.0f};
    if(f4sum_def(f4) != 10.0f) return 6;

    FD fd = {1.5f, 2.5};
    if(fdsum_def(fd) != 4.0) return 7;

    IF iff = {7, 3.0f};
    if(ifsum_def(iff) != 10) return 8;

    NA na = {{2, 3}, 4.0};
    if(nasum_def(na) != 9) return 9;

    V2 va = {1.0, 2.0}, vb = {10.0, 20.0};
    V2 vr = v2add_def(va, vb);
    if(vr.x != 11.0 || vr.y != 22.0) return 10;
    if(v2sum_def(va) != 3.0) return 11;

    A2 aa = {4, 5};
    if(a2sum_def(aa) != 9) return 12;

    if(small_def(-1, -2, 3, 4) != 4) return 13;

    P16 p16 = {10, 20};
    if(many_def(1, 2, 3, 4, 5, 6, p16, 7) != 58) return 14;

    D2 d2 = {9.0, 10.0};
    if(manyd_def(1, 2, 3, 4, 5, 6, 7, 8, d2, 11) != 66.0) return 15;

    S3 r3 = s3id_def(v3);
    if(r3.a != 1 || r3.b != 2 || r3.c != 3) return 16;

    E12 e12 = {7, 8, 9};
    E12 re = e12id_def(e12);
    if(re.a != 7 || re.b != 8 || re.c != 9) return 17;

    return 0;
}
