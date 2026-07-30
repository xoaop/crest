#ifndef CREST_STABLE_ORDERED_ARRAY_HPP
#define CREST_STABLE_ORDERED_ARRAY_HPP

#include "array.hpp"

#ifdef CREST_DEBUG
#include <vector>
#include <chrono>
#include <print>
#endif

// StableOrderedArray —— 下标永久 + 顺序可排 + 中间插入安全
//
//   外部 API 全部使用 isize（下标 == push/insert 返回的身份标识）
//   operator[i]   i 是 push/insert 返回的永久身份标识
//   begin/end     按逻辑顺序遍历（遍历中任意增删安全）
//   insert(pos, v)  pos 是 isize 逻辑位置，返回 isize 身份标识
//   insert_after(sub, v)  在身份标识 sub 的逻辑后面插入
//   remove_at(pos) / remove(sub)
template<typename T>
struct StableOrderedArray {
private:
    struct Index {
        isize v;
        Index() : v(-1) {}
        Index(isize val) : v(val) {}
        friend bool operator==(Index, Index) = default;
    };

public:
    static StableOrderedArray make(xpAllocator allocator) {
        StableOrderedArray sa;
        sa.items = make_array<T>(allocator);
        sa.order = make_array<Index>(allocator);
        return sa;
    }

    void free() {
        array_free(&items);
        array_free(&order);
    }

    // ---- 增 ----
    isize push(T v) {
        isize s = items.count;
        items.push_back(std::move(v));
        order.push_back(Index{s});
        return s;
    }

    isize insert(isize pos, T v) {
        XP_ASSERT_DEFAULT(pos >= 0 && pos <= order.count);
        isize s = items.count;
        items.push_back(std::move(v));
        order.insert(pos, Index{s});
        return s;
    }

    isize insert_after(isize sub, T v) {
        isize pos = _pos_of(Index{sub});
        XP_ASSERT_DEFAULT(pos >= 0);
        return insert(pos + 1, std::move(v));
    }

    // ---- 删 ----
    void remove_at(isize pos) {
        XP_ASSERT_DEFAULT(pos >= 0 && pos < order.count);
        order.rm(pos);
    }

    void remove(isize sub) {
        isize pos = _pos_of(Index{sub});
        XP_ASSERT_DEFAULT(pos >= 0);
        order.rm(pos);
    }

    // ---- 查 ----
    isize count() const { return order.count; }

    bool valid(isize sub) const {
        return _pos_of(Index{sub}) >= 0;
    }

    // ---- 永久身份访问 ----
    T& operator[](isize sub) {
        XP_ASSERT_DEFAULT(sub >= 0 && sub < items.count);
        return items[sub];
    }

    const T& operator[](isize sub) const {
        XP_ASSERT_DEFAULT(sub >= 0 && sub < items.count);
        return items[sub];
    }

    // ---- 迭代器（存 pos，遍历中任意增删安全） ----
    template<bool IsConst>
    struct Iter {
        using Ptr = std::conditional_t<IsConst, const StableOrderedArray*, StableOrderedArray*>;
        using Ref = std::conditional_t<IsConst, const T&, T&>;

        Ptr   sa;
        isize pos;  // index into order

        Ref   operator*()        { return (*sa)[sa->order[pos].v]; }
        auto  operator->()       { return &(*sa)[sa->order[pos].v]; }
        isize subscript() const  { return sa->order[pos].v; }

        Iter& operator++() { pos++; return *this; }

        bool operator!=(const Iter& o) const { return pos != o.pos; }
        bool operator==(const Iter& o) const { return pos == o.pos; }
    };

    using Iterator      = Iter<false>;
    using ConstIterator = Iter<true>;

    Iterator begin()             { return { this, 0 }; }
    Iterator end()               { return { this, order.count }; }
    ConstIterator begin() const  { return { this, 0 }; }
    ConstIterator end()   const  { return { this, order.count }; }

private:
    Array<T>      items;   // 只 push_back，永不移动。下标 == items 索引。
    Array<Index>  order;   // 逻辑顺序：order[logical_pos] = subscript

    isize _pos_of(Index sub) const {
        for (isize i = 0; i < order.count; i++) {
            if (order[i] == sub) return i;
        }
        return -1;
    }
};

// ============================================================
// 测试
// ============================================================
static void test_stable_ordered_array() {

    // --- 空 ---
    {
        auto sa = StableOrderedArray<int>::make(xp_default_allocator());
        XP_ASSERT_DEFAULT(sa.count() == 0);
        sa.free();
    }

    // --- push → 永久下标 ---
    {
        auto sa = StableOrderedArray<int>::make(xp_default_allocator());
        isize a = sa.push(10);
        isize b = sa.push(20);
        isize c = sa.push(30);
        XP_ASSERT_DEFAULT(sa.count() == 3);

        isize expected[] = {10, 20, 30};
        isize i = 0;
        for (auto it = sa.begin(); it != sa.end(); ++it, ++i) {
            XP_ASSERT_DEFAULT(*it == expected[i]);
        }

        XP_ASSERT_DEFAULT(sa[a] == 10);
        XP_ASSERT_DEFAULT(sa[b] == 20);
        XP_ASSERT_DEFAULT(sa[c] == 30);
        sa.free();
    }

    // --- insert 到头部 + 永久下标不变 ---
    {
        auto sa = StableOrderedArray<int>::make(xp_default_allocator());
        isize a = sa.push(10);
        isize b = sa.push(20);
        isize c = sa.push(30);

        isize x = sa.insert(0, 5);
        XP_ASSERT_DEFAULT(sa.count() == 4);

        isize expected[] = {5, 10, 20, 30};
        isize i = 0;
        for (auto it = sa.begin(); it != sa.end(); ++it, ++i) {
            XP_ASSERT_DEFAULT(*it == expected[i]);
        }

        XP_ASSERT_DEFAULT(sa[a] == 10);
        XP_ASSERT_DEFAULT(sa[b] == 20);
        XP_ASSERT_DEFAULT(sa[c] == 30);
        XP_ASSERT_DEFAULT(sa[x] == 5);
        sa.free();
    }

    // --- insert 到中间 ---
    {
        auto sa = StableOrderedArray<int>::make(xp_default_allocator());
        isize a = sa.push(10);
        isize b = sa.push(20);
        isize c = sa.push(30);

        isize x = sa.insert(1, 15);
        XP_ASSERT_DEFAULT(sa.count() == 4);

        isize expected[] = {10, 15, 20, 30};
        isize i = 0;
        for (auto it = sa.begin(); it != sa.end(); ++it, ++i) {
            XP_ASSERT_DEFAULT(*it == expected[i]);
        }

        XP_ASSERT_DEFAULT(sa[a] == 10);
        XP_ASSERT_DEFAULT(sa[b] == 20);
        XP_ASSERT_DEFAULT(sa[c] == 30);
        XP_ASSERT_DEFAULT(sa[x] == 15);
        sa.free();
    }

    // --- insert_after ---
    {
        auto sa = StableOrderedArray<int>::make(xp_default_allocator());
        isize a = sa.push(1);
        isize b = sa.push(3);
        isize c = sa.insert_after(a, 2);

        XP_ASSERT_DEFAULT(sa.count() == 3);
        isize expected[] = {1, 2, 3};
        isize i = 0;
        for (auto it = sa.begin(); it != sa.end(); ++it, ++i) {
            XP_ASSERT_DEFAULT(*it == expected[i]);
        }
        XP_ASSERT_DEFAULT(sa[a] == 1);
        XP_ASSERT_DEFAULT(sa[b] == 3);
        XP_ASSERT_DEFAULT(sa[c] == 2);
        sa.free();
    }

    // --- remove_at ---
    {
        auto sa = StableOrderedArray<int>::make(xp_default_allocator());
        isize a = sa.push(10);
        isize b = sa.push(20);
        isize c = sa.push(30);

        sa.remove_at(1);
        XP_ASSERT_DEFAULT(sa.count() == 2);

        isize expected[] = {10, 30};
        isize i = 0;
        for (auto it = sa.begin(); it != sa.end(); ++it, ++i) {
            XP_ASSERT_DEFAULT(*it == expected[i]);
        }

        XP_ASSERT_DEFAULT(sa[a] == 10);
        XP_ASSERT_DEFAULT(sa[b] == 20);
        XP_ASSERT_DEFAULT(sa[c] == 30);
        XP_ASSERT_DEFAULT(!sa.valid(b));
        XP_ASSERT_DEFAULT(sa.valid(a));
        XP_ASSERT_DEFAULT(sa.valid(c));
        sa.free();
    }

    // --- remove(subscript) ---
    {
        auto sa = StableOrderedArray<int>::make(xp_default_allocator());
        isize a = sa.push(10);
        isize b = sa.push(20);
        isize c = sa.push(30);

        sa.remove(b);
        XP_ASSERT_DEFAULT(sa.count() == 2);
        XP_ASSERT_DEFAULT(!sa.valid(b));
        XP_ASSERT_DEFAULT(sa[a] == 10);
        XP_ASSERT_DEFAULT(sa[b] == 20);
        XP_ASSERT_DEFAULT(sa[c] == 30);
        sa.free();
    }

    // --- 多次 insert_after + 扩容 ---
    {
        auto sa = StableOrderedArray<int>::make(xp_default_allocator());
        isize a = sa.push(100);
        isize b = sa.push(300);
        isize x = sa.insert_after(a, 150);
        isize y = sa.insert_after(x, 200);
        isize c = sa.push(400);

        XP_ASSERT_DEFAULT(sa.count() == 5);
        isize expected[] = {100, 150, 200, 300, 400};
        isize i = 0;
        for (auto it = sa.begin(); it != sa.end(); ++it, ++i) {
            XP_ASSERT_DEFAULT(*it == expected[i]);
        }
        XP_ASSERT_DEFAULT(sa[a] == 100);
        XP_ASSERT_DEFAULT(sa[b] == 300);
        XP_ASSERT_DEFAULT(sa[x] == 150);
        XP_ASSERT_DEFAULT(sa[y] == 200);
        XP_ASSERT_DEFAULT(sa[c] == 400);
        sa.free();
    }

    // --- 遍历中插入 ---
    {
        auto sa = StableOrderedArray<int>::make(xp_default_allocator());
        sa.push(1);
        sa.push(3);
        sa.push(5);

        for (auto it = sa.begin(); it != sa.end(); ++it) {
            if (*it == 1)      sa.insert_after(it.subscript(), 2);
            else if (*it == 3) sa.insert_after(it.subscript(), 4);
        }

        isize expected[] = {1, 2, 3, 4, 5};
        isize i = 0;
        for (auto it = sa.begin(); it != sa.end(); ++it, ++i) {
            XP_ASSERT_DEFAULT(*it == expected[i]);
        }
        XP_ASSERT_DEFAULT(sa.count() == 5);
        sa.free();
    }

    // --- 遍历中删除 ---
    {
        auto sa = StableOrderedArray<int>::make(xp_default_allocator());
        sa.push(99);
        sa.push(10);
        sa.push(99);
        sa.push(50);

        auto it = sa.begin();
        while (it != sa.end()) {
            auto next = it; ++next;
            if (*it == 99) sa.remove(it.subscript());
            it = next;
        }

        isize expected[] = {10, 50};
        isize i = 0;
        for (auto it2 = sa.begin(); it2 != sa.end(); ++it2, ++i) {
            XP_ASSERT_DEFAULT(*it2 == expected[i]);
        }
        XP_ASSERT_DEFAULT(sa.count() == 2);
        sa.free();
    }

    // --- 扩容压力 ---
    {
        auto sa = StableOrderedArray<int>::make(xp_default_allocator());
        auto subs = make_array<isize>(xp_default_allocator());

        for (isize i = 0; i < 200; i++) {
            subs.push_back(sa.push((int)(i * 10)));
        }
        for (isize i = 0; i < 100; i++) {
            sa.insert((i * 2) + 1, (int)(-i - 1));
        }

        for (isize i = 0; i < 200; i++) {
            XP_ASSERT_DEFAULT(sa[subs[i]] == (int)(i * 10));
        }

        isize iter_count = 0;
        for (auto it = sa.begin(); it != sa.end(); ++it) iter_count++;
        XP_ASSERT_DEFAULT(iter_count == 300);

        array_free(&subs);
        sa.free();
    }

    // --- 性能对比：std::vector vs StableOrderedArray ---
    {
        struct Fat {
            isize data[16];  // 128 bytes, 够重让移位有代价
            Fat(isize v = 0) { data[0] = v; }
        };
        using namespace std::chrono;
        auto t = [](auto d) { return duration_cast<nanoseconds>(d).count() / 1e6; };

        const isize BASE = 10000;
        const isize OPS  = 2000;

        auto print3 = [&](const char *label, isize count,
                          double soa, double vec) {
            double ratio = soa > 0 ? vec / soa : 0;
            const char *marker = ratio >= 2.0 ? "  <<" : "";
            std::println(stderr, "  {:20s}  x{:>6}   SOA {:>10.3f} ms   vec {:>10.3f} ms   ({:.1f}x){}",
                label, count, soa, vec, ratio, marker);
        };

        std::println(stderr, "\n--- StableOrderedArray vs std::vector (Fat = 128 bytes) ---");

        // == 1. push (append) ==
        {
            auto sa = StableOrderedArray<Fat>::make(xp_default_allocator());
            std::vector<Fat> vec;

            auto t0 = high_resolution_clock::now();
            for (isize i = 0; i < BASE; i++) sa.push(Fat{i});
            auto t1 = high_resolution_clock::now();
            for (isize i = 0; i < BASE; i++) vec.push_back(Fat{i});
            auto t2 = high_resolution_clock::now();

            std::println(stderr, "[push append x{}]", BASE);
            print3("  push", BASE, t(t1 - t0), t(t2 - t1));
            sa.free();
        }

        // == 2. insert at front ==
        {
            auto sa = StableOrderedArray<Fat>::make(xp_default_allocator());
            std::vector<Fat> vec;

            for (isize i = 0; i < BASE; i++) { sa.push(Fat{i}); vec.push_back(Fat{i}); }

            auto t0 = high_resolution_clock::now();
            for (isize i = 0; i < OPS; i++) sa.insert(0, Fat{-i});
            auto t1 = high_resolution_clock::now();
            for (isize i = 0; i < OPS; i++) vec.insert(vec.begin(), Fat{-i});
            auto t2 = high_resolution_clock::now();

            std::println(stderr, "[insert at front x{}]", OPS);
            print3("  insert_front", OPS, t(t1 - t0), t(t2 - t1));
            sa.free();
        }

        // == 3. insert in middle ==
        {
            auto sa = StableOrderedArray<Fat>::make(xp_default_allocator());
            std::vector<Fat> vec;

            for (isize i = 0; i < BASE; i++) { sa.push(Fat{i}); vec.push_back(Fat{i}); }

            auto t0 = high_resolution_clock::now();
            for (isize i = 0; i < OPS; i++) sa.insert(BASE / 2, Fat{-i});
            auto t1 = high_resolution_clock::now();
            for (isize i = 0; i < OPS; i++) vec.insert(vec.begin() + BASE / 2, Fat{-i});
            auto t2 = high_resolution_clock::now();

            std::println(stderr, "[insert middle x{}]", OPS);
            print3("  insert_mid", OPS, t(t1 - t0), t(t2 - t1));
            sa.free();
        }

        // == 4. remove from front ==
        {
            auto sa = StableOrderedArray<Fat>::make(xp_default_allocator());
            std::vector<Fat> vec;

            for (isize i = 0; i < BASE; i++) { sa.push(Fat{i}); vec.push_back(Fat{i}); }

            auto t0 = high_resolution_clock::now();
            for (isize i = 0; i < OPS; i++) sa.remove_at(0);
            auto t1 = high_resolution_clock::now();
            for (isize i = 0; i < OPS; i++) vec.erase(vec.begin());
            auto t2 = high_resolution_clock::now();

            std::println(stderr, "[remove from front x{}]", OPS);
            print3("  remove_front", OPS, t(t1 - t0), t(t2 - t1));
            sa.free();
        }

        // == 5. remove from middle ==
        {
            auto sa = StableOrderedArray<Fat>::make(xp_default_allocator());
            std::vector<Fat> vec;

            for (isize i = 0; i < BASE; i++) { sa.push(Fat{i}); vec.push_back(Fat{i}); }

            isize mid = (BASE - OPS) / 2;

            auto t0 = high_resolution_clock::now();
            for (isize i = 0; i < OPS; i++) sa.remove_at(mid);
            auto t1 = high_resolution_clock::now();
            for (isize i = 0; i < OPS; i++) vec.erase(vec.begin() + mid);
            auto t2 = high_resolution_clock::now();

            std::println(stderr, "[remove middle x{}]", OPS);
            print3("  remove_mid", OPS, t(t1 - t0), t(t2 - t1));
            sa.free();
        }

        // == 6. iteration ==
        {
            auto sa = StableOrderedArray<Fat>::make(xp_default_allocator());
            std::vector<Fat> vec;

            for (isize i = 0; i < BASE; i++) { sa.push(Fat{i}); vec.push_back(Fat{i}); }

            volatile isize sink = 0;
            const isize REPS = 500;

            auto t0 = high_resolution_clock::now();
            for (isize rep = 0; rep < REPS; rep++)
                for (auto it = sa.begin(); it != sa.end(); ++it) sink += it->data[0];
            auto t1 = high_resolution_clock::now();
            for (isize rep = 0; rep < REPS; rep++)
                for (auto &e : vec) sink += e.data[0];
            auto t2 = high_resolution_clock::now();

            std::println(stderr, "[iterate x{}]", REPS);
            print3("  iterate", REPS, t(t1 - t0), t(t2 - t1));
            (void)sink;
            sa.free();
        }
    }
}

#endif
