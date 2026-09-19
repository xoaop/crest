#pragma once



#include "value.hpp"

#include "ref.hpp"

#include "array.hpp"


struct Value;
struct ValueArray;

using ValueRef = Ref<Value>;

// 全局 Value 池（实现在 context.cpp，经 context()->value_array 访问）
Value* try_access_val(const Ref<Value>& r);

// 无效 ValueRef（未指向任何池值）
static const ValueRef INVALID_VALUE = Ref<Value>{};

ValueRef alloc_value();
ValueRef alloc_value(TypeRef type);
ValueRef alloc_value(const Value& v);

struct ValueArray {
    xpAllocator allocator;

    void init(xpAllocator allocator);

    ValueRef alloc_value();
    ValueRef alloc_value(TypeRef type);
    ValueRef alloc_value(const Value& v);

    Value& operator[](ValueRef r);
    const Value& operator[](ValueRef r) const;

    isize count() const;

private:
    Array<Value> values;
};
