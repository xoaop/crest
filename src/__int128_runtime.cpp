typedef __int128 ti_int;

#ifdef __cplusplus
extern "C" {
#endif

// 有符号除法
ti_int __divti3(ti_int a, ti_int b) {
    if (b == 0) {
        // 除零行为可自定义
        return 0;
    }
    // 记录符号
    bool neg = false;
    if (a < 0) { a = -a; neg = !neg; }
    if (b < 0) { b = -b; neg = !neg; }

    ti_int res = 0;
    ti_int rem = 0;
    for (int i = 127; i >= 0; --i) {
        rem = (rem << 1) | ((a >> i) & 1);
        if (rem >= b) {
            rem -= b;
            res |= (ti_int(1) << i);
        }
    }
    return neg ? -res : res;
}

// 有符号取模
ti_int __modti3(ti_int a, ti_int b) { 
    ti_int q = __divti3(a, b);
    return a - q * b;
}



float __floattisf(__int128 a) {
    return (float)(long long)a;
}
double __floattidf(__int128 a) {
    return (double)(long long)a;
}


#ifdef __cplusplus
}
#endif