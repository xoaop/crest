#include "type.hpp"


Type make_type(TypeKind kind) {
    Type t = {};
    t.kind = kind;

    switch(kind) {
    case Type_pointer:
        t.pointed_type = NULL;
        break;
    case Type_struct:
        t.struct_members = make_array<Type>(permanent_allocator());
        break;
    case Type_array:
        t.array_info.element_type = NULL;
        t.array_info.count = 0;
        break;
    default: 
        break;
    }


    return t;
}


void point_to(Type *type, Type *pointed_type) {
    XP_ASSERT_DEFAULT(type->kind == Type_pointer);
    type->pointed_type = pointed_type;
}

void struct_add_member(Type *type, Type member_type) {
    XP_ASSERT_DEFAULT(type->kind == Type_struct);
    type->struct_members.push_back(member_type);
}