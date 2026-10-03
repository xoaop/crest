#pragma once

#include "xoaop.h"

#include "array.hpp"

#include <functional>
#include <type_traits>
#include <utility>
#include <new>


// SlotMap —— 稳定句柄容器
//
//   SlotIndex{index, version} 是永久句柄：删除后槽位进空闲链复用，
//   复用会把该槽位的 version +1，故旧句柄 version 对不上 → 取不到值。
//
//   分块存 T：payload 按 chunk_size 分块，扩容量不改动已有元素地址。
//   槽位元数据（version / next_free）单独放索引块。
//
//   上限 2^31 个槽位、2^32 次复用（version 溢出即防不了悬垂了，可忽略）。

struct SlotIndex {
    i32 index = -1;
    u32 version = 0;

    bool is_valid() const { return index >= 0; }
    friend bool operator==(SlotIndex a, SlotIndex b) = default;
};

template<>
struct std::hash<SlotIndex> {
    usize operator()(SlotIndex k) const {
        return xp_hash_combine_u64((u64)k.version, (u64)k.index);
    }
};


template<typename T, isize chunk_size = 256>
struct SlotMap {

    // 让编译器把 chunk_of 的 div/mod 优化成移位/掩码
    static_assert(chunk_size > 0 && (chunk_size & (chunk_size - 1)) == 0,
                  "chunk_size 必须是 2 的幂");

    // 槽位从未分配过 / 已分配但已归还
    static constexpr i32 INVALID_INDEX = -1;
    // 空闲链尾（空闲链用 next_free 串，尾端是这个值）
    static constexpr i32 FREE_LIST_END = -2;


    static SlotMap init(xpAllocator allocator) {
        return SlotMap(allocator);
    }

    template<typename... Args>
    SlotIndex emplace(Args&&... args) {
        const auto idx = this->_free_head != FREE_LIST_END ? this->pop_free() : this->append_slot();

        new (&this->chunk_of(idx)[idx % chunk_size]) T(std::forward<Args>(args)...);
        this->_count += 1;

        return SlotIndex{idx, this->slot_version(idx)};
    }

    SlotIndex insert(T elem) {
        return this->emplace(std::move(elem));
    }

    // 移除：析构元素，槽位挂回空闲链，version +1
    void remove(SlotIndex key) {
        XP_ASSERT_DEFAULT(this->contains(key));

        const auto idx = key.index;
        this->chunk_of(idx)[idx % chunk_size].~T();
        this->_count -= 1;

        this->slot_version(idx) += 1;
        this->slot_next_free(idx) = this->_free_head;
        this->_free_head = idx;
    }

    bool contains(SlotIndex key) const {
        return key.index >= 0 && isize(key.index) < this->slots.count
            && this->slot_version(key.index) == key.version;
    }

    // 取元素，句柄失效返回 nullptr
    T *try_get(SlotIndex key) {
        if (!this->contains(key)) return nullptr;
        return &this->chunk_of(key.index)[key.index % chunk_size];
    }

    const T *try_get(SlotIndex key) const {
        if (!this->contains(key)) return nullptr;
        return &this->chunk_of(key.index)[key.index % chunk_size];
    }

    // 取元素，句柄失效直接断言崩溃
    T &operator[](SlotIndex key) {
        auto *p = this->try_get(key);
        XP_ASSERT_MSG(p != nullptr, "SlotMap: dangling or invalid SlotIndex (%d)\n", key.index);
        return *p;
    }

    const T &operator[](SlotIndex key) const {
        auto *p = this->try_get(key);
        XP_ASSERT_MSG(p != nullptr, "SlotMap: dangling or invalid SlotIndex (%d)\n", key.index);
        return *p;
    }

    // 存活元素数
    isize count() const { return this->_count; }

    // 已分配过的槽位数（含已删除未复用的空洞），即任意合法 SlotIndex 的 index 上界
    isize slot_count() const { return this->slots.count; }

    // 遍历所有存活元素
    template<typename F>
    void for_each(F &&f) {
        for (isize i = 0; i < this->slots.count; i++) {
            if (this->is_occupied(i)) {
                f(this->chunk_of(i)[i % chunk_size], SlotIndex{i32(i), this->slot_version(i)});
            }
        }
    }

    template<typename F>
    void for_each(F &&f) const {
        for (isize i = 0; i < this->slots.count; i++) {
            if (this->is_occupied(i)) {
                f(this->chunk_of(i)[i % chunk_size], SlotIndex{i32(i), this->slot_version(i)});
            }
        }
    }

    template<typename U, isize N> friend void free(SlotMap<U, N> *m);

private:

    // 槽位元数据；next_free == INVALID_INDEX 表示该槽位正被占用
    struct SlotInfo {
        u32 version;
        i32 next_free;
    };

    // 块指针表：按 chunk_size 分块分配裸字节，元素在 emplace 时按需构造
    struct Chunk {
        u8 *bytes;
    };

    SlotMap(xpAllocator allocator)
        : allocator(allocator)
        , chunks(make_array<Chunk>(allocator))
        , slots(make_array<SlotInfo>(allocator))
        , _free_head(FREE_LIST_END)
        , _count(0)
    {}

    T *chunk_of(isize idx) {
        return (T *)this->chunks[idx / chunk_size].bytes;
    }

    const T *chunk_of(isize idx) const {
        return (const T *)this->chunks[idx / chunk_size].bytes;
    }

    u32 &slot_version(isize idx) {
        return this->slots[idx].version;
    }

    u32 slot_version(isize idx) const {
        return this->slots[idx].version;
    }

    i32 &slot_next_free(isize idx) {
        return this->slots[idx].next_free;
    }

    i32 slot_next_free(isize idx) const {
        return this->slots[idx].next_free;
    }

    bool is_occupied(isize idx) const {
        return this->slot_next_free(idx) == INVALID_INDEX;
    }

    i32 pop_free() {
        const auto idx = this->_free_head;
        this->_free_head = this->slot_next_free(idx);
        this->slot_next_free(idx) = INVALID_INDEX;
        return idx;
    }

    // 追加新槽位，返回槽位号
    i32 append_slot() {
        const auto idx = i32(this->slots.count);

        this->slots.push_back(SlotInfo{0, INVALID_INDEX});

        if (this->slots.count > this->chunks.count * chunk_size) {
            this->append_chunk();
        }

        return idx;
    }

    void append_chunk() {
        // 裸字节分配：元素在 emplace 时按需构造，这里只算大小
        this->chunks.push_back(Chunk{cast(u8 *)xp_alloc(this->allocator, sizeof(T) * chunk_size)});
    }

    xpAllocator allocator;
    Array<Chunk> chunks;
    Array<SlotInfo> slots;

    i32 _free_head;
    isize _count;
};


template<typename T, isize chunk_size>
void free(SlotMap<T, chunk_size> *m) {
    if constexpr (!std::is_trivially_destructible_v<T>) {
        for (isize i = 0; i < m->slots.count; i++) {
            if (m->is_occupied(i)) {
                m->chunk_of(i)[i % chunk_size].~T();
            }
        }
    }
    for (auto &chunk : m->chunks) {
        xp_free(m->allocator, chunk.bytes);
    }
    array_free(&m->chunks);
    array_free(&m->slots);

    m->_free_head = SlotMap<T, chunk_size>::FREE_LIST_END;
    m->_count = 0;
}

#ifdef CREST_DEBUG

#include <print>
#include <vector>
#include <chrono>

// 统计存活实例数，验证析构次数
namespace slot_map_test_detail {
    struct Counted {
        static inline int alive = 0;
        int v;
        Counted(int v = 0) : v(v) { alive += 1; }
        Counted(const Counted &o) : v(o.v) { alive += 1; }
        Counted(Counted &&o) : v(o.v) { alive += 1; }
        ~Counted() { alive -= 1; }
    };
}

// ============================================================
// 测试（Debug 专用，依赖 <print>）
// ============================================================
static void test_slot_map() {

    // --- 空 ---
    {
        auto m = SlotMap<int>::init(xp_default_allocator());
        XP_ASSERT_DEFAULT(m.count() == 0);
        XP_ASSERT_DEFAULT(m.slot_count() == 0);
        free(&m);
    }

    // --- emplace / try_get / 地址稳定 ---
    {
        auto m = SlotMap<int>::init(xp_default_allocator());
        auto a = m.emplace(10);
        auto b = m.emplace(20);
        auto c = m.emplace(30);
        XP_ASSERT_DEFAULT(m.count() == 3);
        XP_ASSERT_DEFAULT(m[a] == 10 && m[b] == 20 && m[c] == 30);

        auto *pa = m.try_get(a);
        auto *pb = m.try_get(b);

        // 跨多块扩容后地址必须不变
        for (int i = 0; i < 2000; i++) m.emplace(i);
        XP_ASSERT_DEFAULT(m.try_get(a) == pa);
        XP_ASSERT_DEFAULT(m.try_get(b) == pb);
        XP_ASSERT_DEFAULT(*pa == 10 && *pb == 20);
        XP_ASSERT_DEFAULT(m.count() == 2003);
        free(&m);
    }

    // --- 删除 → 句柄失效 → 槽位复用后 version +1 ---
    {
        auto m = SlotMap<int>::init(xp_default_allocator());
        auto a = m.emplace(10);
        auto b = m.emplace(20);

        m.remove(a);
        XP_ASSERT_DEFAULT(m.count() == 1);
        XP_ASSERT_DEFAULT(!m.contains(a));
        XP_ASSERT_DEFAULT(m.try_get(a) == nullptr);
        XP_ASSERT_DEFAULT(m[b] == 20);
        XP_ASSERT_DEFAULT(m.slot_count() == 2);

        auto c = m.emplace(30);
        XP_ASSERT_DEFAULT(c.index == a.index);      // 归还的槽位被复用
        XP_ASSERT_DEFAULT(c.version == a.version + 1);
        XP_ASSERT_DEFAULT(!m.contains(a));          // 旧句柄永远失效
        XP_ASSERT_DEFAULT(m[c] == 30);
        XP_ASSERT_DEFAULT(m.count() == 2);
        free(&m);
    }

    // --- 反复复用同一槽位，version 持续增长 ---
    {
        auto m = SlotMap<int>::init(xp_default_allocator());
        auto k0 = m.emplace(0);
        auto k = k0;
        for (u32 i = 1; i < 100; i++) {
            m.remove(k);
            XP_ASSERT_DEFAULT(m.count() == 0);

            auto next = m.emplace(i32(i));
            XP_ASSERT_DEFAULT(next.index == k0.index);
            XP_ASSERT_DEFAULT(next.version == k.version + 1);
            XP_ASSERT_DEFAULT(!m.contains(k) && !m.contains(k0));
            XP_ASSERT_DEFAULT(m[next] == i32(i));
            k = next;
        }
        m.remove(k);
        XP_ASSERT_DEFAULT(m.count() == 0);
        XP_ASSERT_DEFAULT(m.slot_count() == 1);
        free(&m);
    }

    // --- 越界句柄 ---
    {
        auto m = SlotMap<int>::init(xp_default_allocator());
        m.emplace(1);
        XP_ASSERT_DEFAULT(m.try_get(SlotIndex{-1, 0}) == nullptr);
        XP_ASSERT_DEFAULT(m.try_get(SlotIndex{99, 0}) == nullptr);
        XP_ASSERT_DEFAULT(m.try_get(SlotIndex{0, 1}) == nullptr);
        free(&m);
    }

    // --- 元素析构次数正确（remove 过的不能重复析构） ---
    {
        using Counted = slot_map_test_detail::Counted;
        Counted::alive = 0;

        auto m = SlotMap<Counted>::init(xp_default_allocator());
        auto k0 = m.emplace(1);
        m.emplace(2);   // k1：remove 掉别的元素后仍存活，free 时才析构
        auto k2 = m.emplace(3);
        XP_ASSERT_DEFAULT(Counted::alive == 3);

        m.remove(k0);
        m.remove(k2);
        XP_ASSERT_DEFAULT(Counted::alive == 1);

        free(&m);
        XP_ASSERT_DEFAULT(Counted::alive == 0);     // k1 被 free 析构，k0/k2 不重复析构
    }

    // --- for_each ---
    {
        auto m = SlotMap<int>::init(xp_default_allocator());
        auto keys = make_array<SlotIndex>(xp_default_allocator());
        for (int i = 0; i < 10; i++) keys.push_back(m.emplace(i));
        m.remove(keys[2]);
        m.remove(keys[7]);

        int seen = 0;
        int sum = 0;
        bool keys_match = true;
        m.for_each([&](int &v, SlotIndex k) {
            seen += 1;
            sum += v;
            keys_match = keys_match && m.contains(k);
        });
        XP_ASSERT_DEFAULT(seen == 8);
        XP_ASSERT_DEFAULT(sum == 45 - 2 - 7);
        XP_ASSERT_DEFAULT(keys_match);

        array_free(&keys);
        free(&m);
    }

    // --- 随机增删压力：对照一个 bool 存活模型 ---
    {
        constexpr isize N = 500;
        auto m = SlotMap<i32>::init(xp_default_allocator());
        auto keys = make_array<SlotIndex>(xp_default_allocator());
        auto alive = make_array<b8>(xp_default_allocator());
        for (isize i = 0; i < N; i++) {
            keys.push_back(SlotIndex{});
            alive.push_back(false);
        }

        isize live = 0;
        u64 seed = 12345;
        for (isize step = 0; step < 20000; step++) {
            seed = seed * 6364136223846793005ull + 1442695040888963407ull;
            const auto pick = isize((seed >> 33) % N);

            if (alive[pick]) {
                m.remove(keys[pick]);
                XP_ASSERT_DEFAULT(!m.contains(keys[pick]));
                alive[pick] = false;
                live -= 1;
            } else {
                keys[pick] = m.emplace(i32(pick));
                XP_ASSERT_DEFAULT(m[keys[pick]] == i32(pick));
                alive[pick] = true;
                live += 1;
            }
            XP_ASSERT_DEFAULT(m.count() == live);
        }

        for (isize i = 0; i < N; i++) {
            if (alive[i]) m.remove(keys[i]);
        }
        XP_ASSERT_DEFAULT(m.count() == 0);

        array_free(&keys);
        array_free(&alive);
        free(&m);
    }

    // --- 全删再插：槽位全复用，不涨 slot_count ---
    {
        constexpr isize BIG = 20000;
        auto m = SlotMap<int>::init(xp_default_allocator());
        auto keys = make_array<SlotIndex>(xp_default_allocator());

        for (isize i = 0; i < BIG; i++) keys.push_back(m.emplace(i32(i)));
        XP_ASSERT_DEFAULT(m.count() == BIG);
        XP_ASSERT_DEFAULT(m.slot_count() == BIG);

        for (isize i = 0; i < BIG; i++) m.remove(keys[i]);
        XP_ASSERT_DEFAULT(m.count() == 0);

        for (isize i = 0; i < BIG; i++) keys[i] = m.emplace(i32(i * 2));
        XP_ASSERT_DEFAULT(m.count() == BIG);
        XP_ASSERT_DEFAULT(m.slot_count() == BIG);
        for (isize i = 0; i < BIG; i++) XP_ASSERT_DEFAULT(m[keys[i]] == i32(i * 2));

        array_free(&keys);
        free(&m);
    }

    // --- 性能对比：SlotMap vs std::vector<std::optional<T>> ---
    {
        struct Fat {
            isize data[16];
            Fat(isize v = 0) { data[0] = v; }
        };
        using namespace std::chrono;
        auto ms = [](auto d) { return duration_cast<nanoseconds>(d).count() / 1e6; };

        constexpr isize BASE = 100000;
        constexpr isize OPS  = 100000;

        std::println(stderr, "\n--- SlotMap vs std::vector (Fat = 128 bytes) ---");

        auto print2 = [&](const char *label, double sm, double vec) {
            const auto ratio = vec > 0 ? sm / vec : 0.0;
            std::println(stderr, "  {:20s}  SlotMap {:>10.3f} ms   vec {:>10.3f} ms   ({:.1f}x)",
                label, sm, vec, ratio);
        };

        // == 1. emplace 追加 ==
        {
            auto m = SlotMap<Fat>::init(xp_default_allocator());
            std::vector<Fat> vec;
            vec.reserve(BASE);

            auto t0 = high_resolution_clock::now();
            for (isize i = 0; i < BASE; i++) m.emplace(i);
            auto t1 = high_resolution_clock::now();
            for (isize i = 0; i < BASE; i++) vec.push_back(Fat{i});
            auto t2 = high_resolution_clock::now();

            print2("emplace x100000", ms(t1 - t0), ms(t2 - t1));
            free(&m);
        }

        // == 2. 取元素 ==
        {
            auto m = SlotMap<Fat>::init(xp_default_allocator());
            std::vector<Fat> vec;
            vec.reserve(BASE);
            auto keys = make_array<SlotIndex>(xp_default_allocator());
            for (isize i = 0; i < BASE; i++) keys.push_back(m.emplace(i));
            for (isize i = 0; i < BASE; i++) vec.push_back(Fat{i});

            volatile isize sink = 0;
            constexpr isize REPS = 100;

            auto t0 = high_resolution_clock::now();
            for (isize rep = 0; rep < REPS; rep++)
                for (isize i = 0; i < BASE; i++) sink += m[keys[i]].data[0];
            auto t1 = high_resolution_clock::now();
            for (isize rep = 0; rep < REPS; rep++)
                for (isize i = 0; i < BASE; i++) sink += vec[i].data[0];
            auto t2 = high_resolution_clock::now();

            print2("try_get x100x100k", ms(t1 - t0), ms(t2 - t1));
            (void)sink;
            array_free(&keys);
            free(&m);
        }

        // == 3. remove（SlotMap 进空闲链 vs vec erase 移位）==
        {
            auto m = SlotMap<Fat>::init(xp_default_allocator());
            std::vector<Fat> vec;
            vec.reserve(BASE);
            auto keys = make_array<SlotIndex>(xp_default_allocator());
            for (isize i = 0; i < BASE; i++) keys.push_back(m.emplace(i));
            for (isize i = 0; i < BASE; i++) vec.push_back(Fat{i});

            // 从中间连续删，vec 每次都要搬半个数组
            const isize mid = BASE / 2;
            const isize removes = BASE - mid;

            auto t0 = high_resolution_clock::now();
            for (isize i = 0; i < removes; i++) m.remove(keys[mid + i]);
            auto t1 = high_resolution_clock::now();
            for (isize i = 0; i < removes; i++) vec.erase(vec.begin() + mid);
            auto t2 = high_resolution_clock::now();

            print2("remove mid x50000", ms(t1 - t0), ms(t2 - t1));
            array_free(&keys);
            free(&m);
        }

        // == 4. 删+插（槽位复用）==
        {
            constexpr isize N = 1000;
            auto m = SlotMap<Fat>::init(xp_default_allocator());
            auto keys = make_array<SlotIndex>(xp_default_allocator());
            for (isize i = 0; i < N; i++) keys.push_back(m.emplace(i));

            std::vector<Fat> vec;
            vec.reserve(N);
            for (isize i = 0; i < N; i++) vec.push_back(Fat{i});

            auto t0 = high_resolution_clock::now();
            for (isize i = 0; i < OPS; i++) {
                const auto slot = i % N;
                m.remove(keys[slot]);
                keys[slot] = m.emplace(i);
            }
            auto t1 = high_resolution_clock::now();
            for (isize i = 0; i < OPS; i++) {
                const auto slot = i % N;
                vec.erase(vec.begin() + slot);
                vec.insert(vec.begin() + slot, Fat{i});
            }
            auto t2 = high_resolution_clock::now();

            print2("churn x100000", ms(t1 - t0), ms(t2 - t1));
            array_free(&keys);
            free(&m);
        }
    }
}

#endif // CREST_DEBUG

