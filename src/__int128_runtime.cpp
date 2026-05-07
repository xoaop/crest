#include <stdint.h>
#include <cmath>
#include <cassert>

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





// double转__int128
__int128 __fixdfti(double a) {
    // 处理NaN和无穷大 - 编译期不应该出现这种情况，运行时出现则属于UB
    // 断言失败提示开发者，而不是静默返回错误值
    assert(!(a != a) && "Cannot convert NaN to __int128");
    assert(!(a == 1.0 / 0.0) && "Cannot convert positive infinity to __int128");
    assert(!(a == -1.0 / 0.0) && "Cannot convert negative infinity to __int128");

    if (a != a || a == 1.0 / 0.0 || a == -1.0 / 0.0) {
        return 0; // Release模式下的安全fallback
    }

    // 先转换为int64_t处理大部分情况（99%的场景）
    if (a >= static_cast<double>(INT64_MIN) && a <= static_cast<double>(INT64_MAX)) {
        return static_cast<__int128>(static_cast<int64_t>(a));
    }

    // 处理超出int64范围的值
    // 注意：double只有53位有效精度，大于2^53的整数转换会丢失精度
    bool negative = false;
    if (a < 0) {
        negative = true;
        a = -a;
    }

    __int128 result = 0;
    while (a >= 1.0) {
        result = result * 2 + ((int)fmod(a, 2.0));
        a = floor(a / 2.0);
    }

    return negative ? -result : result;
}

#ifdef __cplusplus
}
#endif
