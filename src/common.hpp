#ifndef CREST_COMMON_H
#define CREST_COMMON_H



//
// Memory Allocator 
//
xpAllocator permanent_allocator();
xpAllocator temp_allocator();
xpAllocator stage_allocator();

void global_allocators_init();
void global_allocators_free();



//
// Utilities
//

xpString rename_ident(xpHashMap<xpString, isize> *identifier_map, xpString ident);

#endif