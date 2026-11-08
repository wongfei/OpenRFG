// Original: volition/render_lib/code/rl_partition_sector.h
// Implementation: volition/render_lib/code/rl_partition_sector.cpp
//
// rl_partition_sector, a node of the partition tree holding its scene entities.
#pragma once

#include "volition/render_lib/code/linked_list_p.h"
#include "volition/render_lib/code/rl_base_object.h"
#include "volition/render_lib/code/rl_bvolume.h"
#include "volition/render_lib/code/rl_partition_sector_entry.h"
#include "volition/vlib/code/math/vector3.h"

class rl_aabb;
class rl_color;
class rl_scene_entity;
class rl_sphere;

// size 0x50
class rl_partition_sector : public rl_base_object
{
    // Access inferred: the quadtree writes the sector volume, position and children directly (inlined accessors).
    friend class rl_quadtree;
    friend class rl_quadtree_sector;
public:
    typedef linked_list_p<rl_partition_sector_entry> entry_list;

protected:
    rl_partition_sector(); // 0x004F06B0 (.h:80)
    virtual ~rl_partition_sector(); // vtbl+0x00 0x004F06F0 (.h:84)
public:
    rl_partition_sector* get_child(int index) const; // 0x004C4150 (.h:88)
    int get_child_count() const; // 0x004C4160 (.h:93)
    const linked_list_p<rl_partition_sector_entry>& get_entries() const; // 0x004C4180 (.h:104)
    void set_position(const vector& position); // 0x004C41C0 (.h:122)
    const vector& get_position() const; // 0x004C41E0 (.h:127)
    const rl_bvolume& get_world_bvolume() const; // 0x004C41B0 (.h:116)
    void get_world_sphere(rl_sphere& sphere) const; // 0x004C4210 (.h:137)
    const rl_sphere get_world_sphere() const; // 0x004C41F0 (.h:132)
    void set_world_bounding_sphere(const rl_sphere&);
    void get_world_aabb(rl_aabb& aabb) const; // 0x004C4230 (.h:152)
    const rl_aabb get_world_aabb() const; // 0x00513EE0 (.h:147)
    void set_world_aabb(const rl_aabb& world_aabb); // 0x004C4310 (.h:157)
    const rl_color& get_debug_color() const;
    bool temp_validate_exists(rl_scene_entity* entity_p); // 0x004F0700 (:12)
protected:
    void set_child_list(rl_partition_sector** child_list_p, int child_count); // 0x004C4190 (.h:109)
    void set_child_count(int child_count); // 0x004C4170 (.h:98)

    linked_list_p<rl_partition_sector_entry> m_entries; // 0x08
    int m_child_count; // 0x10
private:
    rl_partition_sector** m_children; // 0x14
    vector m_position; // 0x18
    rl_bvolume m_world_bvolume; // 0x30
    static rl_color ms_debug_color; // 0x01514FAC
};
