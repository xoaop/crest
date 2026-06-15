#pragma once

#include "xoaop.h"
#include "array.hpp"


// ─── 通用 DAG（有向无环图），不耦合任何业务类型 ───

template<typename K>
struct DAG {
    xpAllocator allocator;
    xpHashMap<K, xpHashSet<K>> deps;       // node → 我依赖的
    xpHashMap<K, xpHashSet<K>> dependents;  // node → 依赖我的

    // ── 内部辅助 ──

    xpHashSet<K> *get_deps_or_create(K node) {
        xpHashSet<K> *set = xp_hash_map_get(deps, node);
        if (!set) {
            xpHashSet<K> empty = xp_hash_set_make<K>(allocator);
            xp_hash_map_insert(&deps, node, empty);
            set = xp_hash_map_get(deps, node);
        }
        return set;
    }

    xpHashSet<K> *get_dependents_or_create(K node) {
        xpHashSet<K> *set = xp_hash_map_get(dependents, node);
        if (!set) {
            xpHashSet<K> empty = xp_hash_set_make<K>(allocator);
            xp_hash_map_insert(&dependents, node, empty);
            set = xp_hash_map_get(dependents, node);
        }
        return set;
    }

    const xpHashSet<K> *get_deps(K node) const {
        return xp_hash_map_get(deps, node);
    }

    const xpHashSet<K> *get_dependents(K node) const {
        return xp_hash_map_get(dependents, node);
    }

    // ── 修改 ──

    // 添加边 parent → child。如果产生环则返回 false。
    bool add_edge(K parent, K child) {
        if (parent == child) return false;
        if (has_path(child, parent)) return false;

        xp_hash_set_insert(get_deps_or_create(parent), child);
        xp_hash_set_insert(get_dependents_or_create(child), parent);

        // 确保两端都在图中（即使没有边）
        get_deps_or_create(child);
        get_dependents_or_create(parent);

        return true;
    }

    // 删除节点及其所有关联边
    void remove_node(K node) {
        // 移除 node 的所有出边（我依赖的）
        xpHashSet<K> *node_deps = xp_hash_map_get(deps, node);
        if (node_deps) {
            for (isize i = 0; i < node_deps->capacity; i++) {
                if (node_deps->entries[i].state == XP_HASH_SLOT_USED) {
                    K child = node_deps->entries[i].key;
                    xpHashSet<K> *child_dependents = xp_hash_map_get(dependents, child);
                    if (child_dependents) {
                        xp_hash_set_remove(child_dependents, node);
                        if (child_dependents->count == 0 && !xp_hash_map_get(deps, child)) {
                            xp_hash_set_free(*child_dependents);
                            xp_hash_map_remove(&dependents, child);
                        }
                    }
                }
            }
            xp_hash_set_free(*node_deps);
            xp_hash_map_remove(&deps, node);
        }

        // 移除 node 的所有入边（依赖我的）
        xpHashSet<K> *node_dependents = xp_hash_map_get(dependents, node);
        if (node_dependents) {
            for (isize i = 0; i < node_dependents->capacity; i++) {
                if (node_dependents->entries[i].state == XP_HASH_SLOT_USED) {
                    K parent = node_dependents->entries[i].key;
                    xpHashSet<K> *parent_deps = xp_hash_map_get(deps, parent);
                    if (parent_deps) {
                        xp_hash_set_remove(parent_deps, node);
                        if (parent_deps->count == 0 && !xp_hash_map_get(dependents, parent)) {
                            xp_hash_set_free(*parent_deps);
                            xp_hash_map_remove(&deps, parent);
                        }
                    }
                }
            }
            xp_hash_set_free(*node_dependents);
            xp_hash_map_remove(&dependents, node);
        }
    }

    // ── 查询 ──

    bool has_node(K node) const {
        return xp_hash_map_get(deps, node) != nullptr ||
               xp_hash_map_get(dependents, node) != nullptr;
    }

    bool has_dependents(K node) const {
        const xpHashSet<K> *set = xp_hash_map_get(dependents, node);
        return set != nullptr && set->count > 0;
    }

    bool has_dependencies(K node) const {
        const xpHashSet<K> *set = xp_hash_map_get(deps, node);
        return set != nullptr && set->count > 0;
    }


    // ── 孤立节点 ──

    void collect_isolated(Array<K> &out, const Array<K> &candidates) const {
        for (isize i = 0; i < candidates.count; i++) {
            K node = candidates[i];
            if (!has_node(node)) {
                out.push_back(node);
            }
        }
    }

    // ── 传递闭包：所有下游节点 ──
    void transitive_dependents(K root, Array<K> &out) const {
        xpHashSet<K> visited = xp_hash_set_make<K>(allocator);
        collect_downstream(root, &visited, out);
        xp_hash_set_free(visited);
    }

    void transitive_dependencies(K root, Array<K> &out) const {
        xpHashSet<K> visited = xp_hash_set_make<K>(allocator);
        collect_upstream(root, &visited, out);
        xp_hash_set_free(visited);
    }

    // ── 拓扑排序（Kahn 算法） ──

    // 返回拓扑序：如果 A 依赖 B (edge A→B)，则 B 在 A 之前
    // isolated 节点（不在图中的 candidates）放在结果最前面
    Array<K> topological_sort(xpAllocator allocator, const Array<K> *candidates = nullptr) const {
        Array<K> result = make_array<K>(allocator);

        // 孤立节点（不在图中的 candidates）放在最前面
        if (candidates) {
            collect_isolated(result, *candidates);
        }

        xpHashMap<K, u32> out_degree = xp_hash_map_make<K, u32>(xp_heap_allocator());

        // 出度 = 我还依赖几个节点（debs[node] 的大小）
        const xpHashMapEntry<K, xpHashSet<K>> *entry;
        isize pos = xp_hash_map_first_entry(&deps, &entry);
        while (pos != END_OF_HASH_MAP_INDEX) {
            K node = entry->key;
            u32 deg = (u32)entry->value.count;
            xp_hash_map_insert(&out_degree, node, deg);
            pos = xp_hash_map_next_entry(&deps, pos, &entry);
        }

        // 确保所有节点都在 out_degree 中（包括只在 dependents 中的）
        const xpHashMapEntry<K, xpHashSet<K>> *dep_entry;
        pos = xp_hash_map_first_entry(&dependents, &dep_entry);
        while(pos != END_OF_HASH_MAP_INDEX) {
            if(!xp_hash_map_get(out_degree, dep_entry->key)) {
                xp_hash_map_insert(&out_degree, dep_entry->key, (u32)0);
            }
            pos = xp_hash_map_next_entry(&dependents, pos, &dep_entry);
        }

        // 收集出度为 0 的节点（不依赖任何人的，可以最先求值）
        Array<K> queue = make_array<K>(xp_heap_allocator());
        const xpHashMapEntry<K, u32> *deg_entry;
        pos = xp_hash_map_first_entry(&out_degree, &deg_entry);
        while(pos != END_OF_HASH_MAP_INDEX) {
            if(deg_entry->value == 0) {
                queue.push_back(deg_entry->key);
            }
            pos = xp_hash_map_next_entry(&out_degree, pos, &deg_entry);
        }

        while(queue.count > 0) {
            K node = queue.back();
            queue.pop_back();
            result.push_back(node);

            // 处理依赖 node 的节点：它们少了一个依赖项
            const xpHashSet<K> *deps_of = get_dependents(node);
            if(deps_of) {
                for(isize i = 0; i < deps_of->capacity; i++) {
                    if(deps_of->entries[i].state == XP_HASH_SLOT_USED) {
                        K depender = deps_of->entries[i].key;
                        u32 *deg = xp_hash_map_get(out_degree, depender);
                        if(deg && *deg > 0) {
                            (*deg)--;
                            if(*deg == 0) queue.push_back(depender);
                        }
                    }
                }
            }
        }

        array_free(&queue);
        xp_hash_map_free(out_degree);
        return result;
    }

private:
    // DFS: 从 from 到 to 是否已有路径
    bool has_path(K from, K to) const {
        if (from == to) return true;
        xpHashSet<K> visited = xp_hash_set_make<K>(allocator);
        Array<K> stack = make_array<K>(allocator);
        stack.push_back(from);

        while (stack.count > 0) {
            K cur = stack.back();
            stack.pop_back();
            if (xp_hash_set_find(&visited, cur)) continue;
            xp_hash_set_insert(&visited, cur);

            const xpHashSet<K> *children = get_deps(cur);
            if (!children) continue;
            for (isize i = 0; i < children->capacity; i++) {
                if (children->entries[i].state == XP_HASH_SLOT_USED) {
                    K next = children->entries[i].key;
                    if (next == to) {
                        array_free(&stack);
                        xp_hash_set_free(visited);
                        return true;
                    }
                    stack.push_back(next);
                }
            }
        }

        array_free(&stack);
        xp_hash_set_free(visited);
        return false;
    }

    void collect_downstream(K node, xpHashSet<K> *visited, Array<K> &out) const {
        const xpHashSet<K> *deps_of = get_dependents(node);
        if (!deps_of) return;
        for (isize i = 0; i < deps_of->capacity; i++) {
            if (deps_of->entries[i].state == XP_HASH_SLOT_USED) {
                K dep = deps_of->entries[i].key;
                if (xp_hash_set_insert(visited, dep)) {
                    out.push_back(dep);
                    collect_downstream(dep, visited, out);
                }
            }
        }
    }

    void collect_upstream(K node, xpHashSet<K> *visited, Array<K> &out) const {
        const xpHashSet<K> *deps_of = get_deps(node);
        if (!deps_of) return;
        for (isize i = 0; i < deps_of->capacity; i++) {
            if (deps_of->entries[i].state == XP_HASH_SLOT_USED) {
                K dep = deps_of->entries[i].key;
                if (xp_hash_set_insert(visited, dep)) {
                    out.push_back(dep);
                    collect_upstream(dep, visited, out);
                }
            }
        }
    }
};

template<typename K>
DAG<K> make_dag(xpAllocator allocator) {
    DAG<K> dag;
    dag.allocator = allocator;
    dag.deps       = xp_hash_map_make<K, xpHashSet<K>>(allocator);
    dag.dependents = xp_hash_map_make<K, xpHashSet<K>>(allocator);
    return dag;
}

template<typename K>
void dag_free(DAG<K> *dag) {
    // 释放 deps 中所有嵌套的 hash set
    for (isize i = 0; i < dag->deps.capacity; i++) {
        if (dag->deps.entries[i].state == XP_HASH_SLOT_USED) {
            xp_hash_set_free(dag->deps.entries[i].value);
        }
    }
    xp_hash_map_free(dag->deps);

    // 释放 dependents 中所有嵌套的 hash set
    for (isize i = 0; i < dag->dependents.capacity; i++) {
        if (dag->dependents.entries[i].state == XP_HASH_SLOT_USED) {
            xp_hash_set_free(dag->dependents.entries[i].value);
        }
    }
    xp_hash_map_free(dag->dependents);
}
