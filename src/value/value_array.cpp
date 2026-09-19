#include "value_array.hpp"


void ValueArray::init(xpAllocator allocator) {
    this->allocator = allocator;
    values = make_array<Value>(allocator);
}

ValueRef ValueArray::alloc_value() {
    values.push_back(make_value());
    return Ref<Value>{values.count - 1};
}

ValueRef ValueArray::alloc_value(TypeRef type) {
    values.push_back(make_value(type));
    return Ref<Value>{values.count - 1};
}

ValueRef ValueArray::alloc_value(const Value& v) {
    values.push_back(v);
    return Ref<Value>{values.count - 1};
}

Value& ValueArray::operator[](ValueRef r) {
    return values[r.index];
}
const Value& ValueArray::operator[](ValueRef r) const {
    return values[r.index];
}


isize ValueArray::count() const {
    return values.count;
}
