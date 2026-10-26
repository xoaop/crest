typedef struct { long a,b,c; } S24;
typedef struct { signed char a,b,c; } S3;
typedef struct { char a0,a1,a2,a3,a4,a5,a6,a7,a8; } S9;
typedef union  { int a; float b; } U;
typedef struct { long a; double b; } MIX;
typedef struct { float x,y,z,w; } F4;
typedef struct { float x; double y; } FD;
typedef struct { int a; float b; } IF;
typedef struct { int a[2]; double d; } NA;

S24 s24add(S24 a, S24 b){ S24 r; r.a=a.a+b.a; r.b=a.b+b.b; r.c=a.c+b.c; return r; }
long s3sum(S3 v){ return v.a+v.b+v.c; }
long s9sum(S9 v){ return v.a0+v.a1+v.a2+v.a3+v.a4+v.a5+v.a6+v.a7+v.a8; }
int usum(U v){ return v.a; }
double mixsum(MIX v){ return (double)v.a + v.b; }
float f4sum(F4 v){ return v.x+v.y+v.z+v.w; }
double fdsum(FD v){ return (double)v.x + v.y; }
int ifsum(IF v){ return v.a + (int)v.b; }
int nasum(NA v){ return v.a[0] + v.a[1] + (int)v.d; }
long small(signed char a, short b, unsigned char c, unsigned short d){ return a+b+c+d; }
typedef struct { double x,y; } V2;
typedef struct { int a,b; } A2;
V2 v2add(V2 a, V2 b){ V2 r; r.x=a.x+b.x; r.y=a.y+b.y; return r; }
double v2sum(V2 v){ return v.x+v.y; }
int a2sum(A2 v){ return v.a+v.b; }

typedef struct { long a, b; } P16;
typedef struct { double x, y; } D2;
long many(long a,long b,long c,long d,long e,long f, P16 p, long g) {
    return a+b+c+d+e+f+p.a+p.b+g;
}
double manyd(double a,double b,double c,double d,double e,double f,double g,double h, D2 p, double z) {
    return a+b+c+d+e+f+g+h+p.x+p.y+z;
}

typedef struct { int a,b,c; } E12;
S3 s3id(S3 v){ return v; }
E12 e12id(E12 v){ return v; }
