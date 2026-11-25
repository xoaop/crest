#include <stddef.h>
#include <stdint.h>

#if defined(_MSC_VER)
#define ALIGN(N) __declspec(align(N))
#else
#define ALIGN(N) __attribute__((aligned(N)))
#endif

struct ALIGN(16) Small { int8_t x; };
struct ALIGN(32) Aligned { int64_t x; };
struct Outer { int8_t head; struct Aligned value; int8_t tail; };

struct Small small_id(struct Small v) { v.x += 1; return v; }
struct Aligned aligned_id(struct Aligned v) { v.x += 1; return v; }

int32_t layout(struct Small *s, struct Aligned *a, struct Outer *o, struct Aligned *values) {
    if ((uintptr_t)s % 16 || (uintptr_t)a % 32 || (uintptr_t)o % 32) return 1;
    if ((uintptr_t)values % 32 || (uintptr_t)&values[1] % 32) return 2;
    if (sizeof(struct Small) != 16 || sizeof(struct Aligned) != 32) return 3;
    if (sizeof(struct Outer) != 96 || offsetof(struct Outer, value) != 32) return 4;
    o->value.x = 27;
    o->tail = 8;
    values[0].x = 37;
    values[1].x = 43;
    return 0;
}

struct Aligned budget(int64_t a, int64_t b, int64_t c, int64_t d, int64_t e, struct Small s, int64_t f) {
    struct Aligned result = { a + b + c + d + e + s.x + f };
    return result;
}
