#ifndef CREST_COMMON_H
#define CREST_COMMON_H

#include "xoaop.h"
#include "ref.hpp"

struct Package;
struct Scope;

Package *try_access_val(const Ref<Package> &r);
Scope *try_access_val(const Ref<Scope> &r);


//
// Memory Allocator
//
xpAllocator permanent_allocator();
xpAllocator temp_allocator();
xpAllocator stage_allocator();

void global_allocators_init();
void global_allocators_free();



#endif