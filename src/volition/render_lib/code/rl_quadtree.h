// Original: volition/render_lib/code/rl_quadtree.h
// Implementation: volition/render_lib/code/rl_quadtree.cpp
//
// rl_quadtree, a quadtree partition.
#pragma once

#include "volition/render_lib/code/rl_partition_system.h"

class rl_aabb;
class rl_class_dynamic_info;
class rl_quadtree_sector;
class rl_scene_entity;

// size 0x18
class rl_quadtree : public rl_partition_system
{
    // Access inferred: the sectors read the size and depth limit and set the dirty flag directly.
    friend class rl_quadtree_sector;
protected:
    rl_quadtree(); // 0x00543160 (:19)
    virtual ~rl_quadtree(); // vtbl+0x00 0x004C4890 (:30)
public:
    static int get_objects_peak();
    static int get_objects_used();
    static rl_quadtree* create(); // 0x0054CB80 (:12)
    static bool can_create(); // 0x004C4860 (:12)
    virtual void release(); // vtbl+0x04 0x004C4870 (:12)
    virtual void insert(rl_scene_entity* entity_p); // vtbl+0x08 0x005431E0 (:82)
    virtual void remove(rl_scene_entity* entity_p); // vtbl+0x0C 0x005146F0 (:118)
    virtual void clean(); // vtbl+0x10 0x00514740 (:131)
    virtual int query_aabb(rl_scene_entity** entities, int max_entities, const rl_aabb& aabb, int visibility_index); // vtbl+0x14 0x00514920 (:243)
    void reset(); // 0x00514660 (:52)
    int get_max_depth() const; // 0x004C47E0 (.h:84)
    void set_max_depth(int max_depth); // 0x007C1EC0 (.h:89)
    float get_size() const; // 0x004C47F0 (.h:95)
    void set_size(float size); // 0x007C1ED0 (.h:100)
    bool get_dirty() const; // 0x004C4800 (.h:106)
    void set_dirty(bool dirty); // 0x004C4810 (.h:111)
private:
    void init(); // 0x004F1100 (:35)
    void flush_sector(rl_quadtree_sector*);
    void render_quadrant(rl_quadtree_sector*) const;
    bool validate_entity_not_inserted(rl_scene_entity* entity_p); // 0x004F1300 (:77)
    int query_aabb_quadrant(rl_quadtree_sector* sector_p, rl_scene_entity** entities, int max_entities, const rl_aabb& aabb, int visibility_index); // 0x00514760 (:209)

public:
    static rl_class_dynamic_info ms_class_dynamic_info; // 0x0151505C
private:
    int m_max_depth; // 0x0C
    float m_size; // 0x10
    bool m_dirty; // 0x14
};
