// Original: volition/render_lib/code/rl_quadtree_sector.h
// Implementation: volition/render_lib/code/rl_quadtree_sector.cpp
//
// rl_quadtree_sector, a node of rl_quadtree.
#pragma once

#include "volition/render_lib/code/linked_list_p.h"
#include "volition/render_lib/code/pool_list.h"
#include "volition/render_lib/code/rl_partition_sector.h"

class mempool;
class rl_class_factory_info;
struct rl_partition_sector_entry;
class rl_quadtree;
class rl_scene_entity;
class vector;

// size 0x80
class rl_quadtree_sector : public rl_partition_sector
{
    // Access inferred: rl_quadtree resets and inspects the root sector directly.
    friend class rl_quadtree;
protected:
    rl_quadtree_sector(); // 0x004F1050 (.h:63)
    virtual ~rl_quadtree_sector(); // vtbl+0x00 0x004F10C0 (.h:77)
private:
    static const pool_list<rl_quadtree_sector*>& get_object_pool(); // 0x004C4820 (.h:26)
public:
    static void set_objects_max(int objects_max); // 0x007C1EF0 (.h:26)
    static int get_objects_max(); // 0x004C4830 (.h:26)
    static int get_objects_used();
    static int get_objects_free();
    static void factory_init(mempool& factory_mempool); // 0x00514940 (:9)
    static rl_quadtree_sector* create_placement(rl_quadtree_sector* _data_p); // 0x004F1320 (:9)
    static rl_quadtree_sector* create(); // 0x0052C910 (:9)
    static bool can_create(); // 0x004F13A0 (:9)
    virtual void release(); // vtbl+0x04 0x00514A30 (:9)
    void insert(rl_scene_entity* entity_p); // 0x0052CB60 (:96)
    void remove(rl_scene_entity* entity_p); // 0x004F1520 (:225)
    rl_quadtree* get_quadtree() const; // 0x004C4840 (.h:88)
    void set_quadtree(rl_quadtree* quadtree_p); // 0x004C4850 (.h:93)
    bool fits_in(rl_scene_entity* entity_p); // 0x00514490 (.h:98)
    void flush(); // 0x004F1550 (:349)
    void remove_entries(linked_list_p<rl_partition_sector_entry>* dump_list, bool recurse); // 0x004F1590 (:370)
private:
    bool fits_in_child(rl_scene_entity* entity_p, const vector& child_position, float child_size) const; // 0x004F13B0 (:16)
    rl_quadtree_sector* new_child(const vector& child_position, float child_size, unsigned short depth); // 0x0052C970 (:54)
    void delete_child(rl_quadtree_sector* child_p); // 0x004C48B0 (:324)
    void expand_height(rl_scene_entity* entity_p); // 0x00514AB0 (:163)
    void minimize_height(rl_scene_entity* entity_p); // 0x00514EA0 (:195)
    void recompute_max_entity(rl_scene_entity* entity_p); // 0x00515270 (:257)
    void recompute_min_entity(rl_scene_entity* entity_p); // 0x00515540 (:290)

public:
    static const unsigned int m___class_id; // 0x012BEE40
    static rl_class_factory_info ms_class_factory_info; // 0x01515074
protected:
    static pool_list<rl_quadtree_sector*> ms_object_pool; // 0x015150A4
private:
    static const int CHILDREN_MAX; // 0x012BEE44
    rl_quadtree_sector* m_quadrants[4]; // 0x50
    rl_quadtree_sector* m_parent_p; // 0x60
    rl_quadtree* m_quadtree_p; // 0x64
    rl_scene_entity* m_max_entity_p; // 0x68
    rl_scene_entity* m_min_entity_p; // 0x6C
    unsigned short m_depth; // 0x70
    unsigned short m_count; // 0x72
};
