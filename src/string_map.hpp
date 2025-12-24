#ifndef CREST_STRING_MAP_H
#define CREST_STRING_MAP_H

#include "xoaop.h"


xp_internal u32 string_hash(xpString string) {
    u32 hash = xp_murmur_hash3_32(string.c_str, string.length, 0);
    //NOTE(xoaop): 0留下来当作空entry, TRICKY
    if(hash != 1) {
        hash -= 1;
    }
    return hash;
}

template<typename T>
struct StringMapEntry {
    usize hash;
    xpString key;
    T value;
};


template<typename T>
struct StringHashMap {
    xpAllocator allocator;
    StringMapEntry<T> *entries;
    
    isize count;
    isize capacity;
};

template<typename T>
StringHashMap<T> string_map_make(xpAllocator allocator, isize capacity) {
    XP_ASSERT(capacity > 0);

    StringHashMap<T> map = {};
    map.allocator = allocator;
    map.entries = xp_alloc_array<StringMapEntry<T>>(map.allocator, capacity);
    xp_zero_array<StringMapEntry<T>>(map.entries, capacity);

    map.count = 0;
    map.capacity = capacity;

    return map;
}

template<typename T>
void string_map_free(StringHashMap<T> map) {
    for(isize i = 0; i < map.capacity; i++) {
        if(map.entries[i].hash != 0) {
            xp_string_free(map.entries[i].key);
        }
    }

    xp_free(map.allocator, map.entries);
}

template<typename T>
void string_map_extend(StringHashMap<T> *map, isize new_capacity) {
    XP_ASSERT(new_capacity > map->capacity);
    map->entries = cast(StringMapEntry<T> *) xp_realloc(map->allocator, map->entries, sizeof(StringMapEntry<T>) * map->capacity, sizeof(StringMapEntry<T>) * new_capacity);
    map->capacity = new_capacity;
    return;
}

template<typename T>
StringMapEntry<T> *string_map_get_entry(StringHashMap<T> map, xpString key) {
    if(map.count == 0) {
        return NULL;
    }

    usize hash_value = string_hash(key);
    usize index = hash_value % map.capacity;
    usize original_index = index;
    do {
        StringMapEntry<T> *entry = &map.entries[index];
        if(entry->hash == 0) {
            return NULL;
        } else if(entry->hash == hash_value && !xp_string_cmp(entry->key, key)) {
            return entry;
        }

        index = (index + 1) % map.capacity;
    } while(index != original_index);

    return NULL;
}


template<typename T>
T *string_map_get(StringHashMap<T> map, xpString key) {
    StringMapEntry<T> *entry;
    if((entry = string_map_get_entry(map, key)) != NULL) {
        return &entry->value;
    }
    return NULL;
}

template<typename T>
T *string_map_set(StringHashMap<T> *map, xpString key, T value) {
    StringMapEntry<T> *entry;
    if((entry = string_map_get_entry(map, key)) != NULL) {
        entry->value = value;
        return entry->value;
    }
    return NULL;
}


template<typename T>
void string_map_insert(StringHashMap<T> *map, xpString key, T value) {
    
    //TODO: 用上下面的查找逻辑, 而不是找两遍
    StringMapEntry<T> *entry = string_map_get_entry(*map, key);
    if(entry != NULL) {
        entry->value = value;
        return;
    }

    if(map->count >= map->capacity) {
        string_map_extend(map, map->capacity + map->capacity / 2 + 1);
    }
    usize hash_value = string_hash(key);

    usize index = hash_value % map->capacity;
    usize original_index = index;
    do {
        StringMapEntry<T> *entry = &map->entries[index];
        if(entry->hash == 0) {
            entry->key = xp_string_copy(map->allocator, key);
            entry->hash = hash_value;
            entry->value = value;

            map->count += 1;

            return;
        }
        index = (index + 1) % map->capacity;
    } while(index != original_index);
    
    //NOTE: FULL MAP
    XP_ASSERT(0);
    return;
}


#endif