#pragma once



#include "value.hpp"

#include "ref.hpp"

#include "array.hpp"


struct Value;
struct ValueArray;

using ValueRef = Value *;

static constexpr ValueRef INVALID_VALUE = nullptr;

struct ValueArray {
    
    static ValueArray init(xpAllocator allocator);
    
    ValueRef alloc_value();
    ValueRef alloc_value(TypeRef type);
    ValueRef alloc_value(const Value& v);

    isize count() const;
private:
    xpAllocator allocator;
    isize _count;
};
