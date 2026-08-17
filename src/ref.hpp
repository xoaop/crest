#pragma once



#include "xoaop.h"
#include "print.hpp"



template<typename T>
struct Ref;

template<typename T>
concept RefBaseConcept = requires(const Ref<T> &d) { try_access_val(d); };

template<RefBaseConcept T>
struct RefBase {
    static const inline Ref<T> INVALID_REF{};

    // 保证非空解包：拿不到就断言崩溃
    T &unwrap() const {
        T *p = try_access_val(derived());
        ASSERT(p != nullptr);
        return *p;
    }

    // ref-> / *ref 为保证非空解包的语法糖（同样断言）
    T *operator->() const { 
        return &unwrap(); 
    }

    T &operator*()  const { 
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
};



