#ifndef CREST_COMMON_H
#define CREST_COMMON_H

#include "xoaop.h"

using PackageRef = isize;    // 全局包表编号（唯一定义，各头统一从这取）


//
// Memory Allocator
//
xpAllocator permanent_allocator();
xpAllocator temp_allocator();
xpAllocator stage_allocator();

void global_allocators_init();
void global_allocators_free();



#endif