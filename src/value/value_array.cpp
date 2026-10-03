#include "value_array.hpp"


static ValueRef alloc_value(xpAllocator allocator);

ValueArray ValueArray::init(xpAllocator allocator) {
    ValueArray va;
    va.allocator = allocator;
    va._count = 0;
    return va;
}

ValueRef ValueArray::alloc_value() {
    auto vr = ::alloc_value(this->allocator);
    *vr = make_value();

    this->_count += 1;

    return vr;
}

ValueRef ValueArray::alloc_value(TypeRef type) {
    auto vr = ::alloc_value(this->allocator);
    *vr = make_value(type);

    this->_count += 1;

    return vr;
}

ValueRef ValueArray::alloc_value(const Value& v) {
    auto vr = this->alloc_value();
    *vr = clone_value(v, this->allocator);

    return vr;
}



isize ValueArray::count() const {
    return this->_count;
}



static ValueRef alloc_value(xpAllocator allocator) {
    ValueRef vr = xp_alloc<Value>(allocator);
    new (vr) Value();
    return vr;
}