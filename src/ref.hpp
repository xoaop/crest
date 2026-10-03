#pragma once



#include "xoaop.h"
#include "print.hpp"



template<typename T>
struct Ref;

template<typename T>
struct RefBase {
    static const inline Ref<T> INVALID_REF{};

    T *try_get() const {
        return derived().resolve();
    }

    bool is_valid() const {
        return this->try_get() != nullptr;
    }

    // 保证非空解包：拿不到就断言崩溃
    T &unwrap() const {
        T *p = this->try_get();
        ASSERT(p != nullptr);
        return *p;
    }

    // ref-> / *ref 为保证非空解包的语法糖（同样断言）
    T *operator->() const {
        return &unwrap();
    }

    T &operator*() const {
        return unwrap();
    }

private:

    Ref<T> &derived() {
        return static_cast<Ref<T>&>(*this);
    }

    const Ref<T> &derived() const {
        return static_cast<const Ref<T>&>(*this);
    }
};



template<typename T>
struct Ref : RefBase<T> {
    isize index = -1;

    constexpr Ref() = default;
    explicit constexpr Ref(isize idx) : index(idx) {}

    bool operator==(const Ref& other) const { return index == other.index; }

    T *resolve() const;
};

template<typename T>
struct std::hash<Ref<T>> {
    usize operator()(const Ref<T>& r) const { return (usize)(u64)r.index; }
};