// Original: volition/render_lib/code/rl_partition_sector.cpp
//
// rl_partition_sector: a node of a partition tree with its entity entries (a circular list of the entities'
// rl_partition_sector_entry links), its children, position and world bounding volume. The header-inline members of
// rl_partition_sector.h are defined here as well.

#include "volition/render_lib/code/rl_partition_sector.h"

#include "volition/render_lib/code/rl_aabb.h"
#include "volition/render_lib/code/rl_color.h"
#include "volition/render_lib/code/rl_scene_entity.h"
#include "volition/render_lib/code/rl_sphere.h"

// Magic number: the debug draw color of sectors (debug only), 0xFF00FFFF.
rl_color rl_partition_sector::ms_debug_color(0xFF, 0xFF, 0x00, 0xFF); // 0x01514FAC

// 0x004F0700 (:12)
// Whether the entity is in this sector or one below it (a debug check).
bool rl_partition_sector::temp_validate_exists(rl_scene_entity* entity_p)
{
    rl_partition_sector_entry* head = m_entries.first();
    for (rl_partition_sector_entry* entry = head; entry != nullptr;)
    {
        rl_scene_entity* entry_entity = entry->m_entity_p;
        entry = entry->next != head ? entry->next : nullptr;
        if (entity_p == entry_entity)
        {
            return true;
        }
    }
    for (int i = 0; i < m_child_count; i++)
    {
        if (m_children[i] != nullptr && m_children[i]->temp_validate_exists(entity_p))
        {
            return true;
        }
    }
    return false;
}

// 0x004F06B0 (.h:80)
// Notes (decompiled behaviour, kept as is): the child count, children and volume are left to the derived class.
rl_partition_sector::rl_partition_sector()
{
    m_entries.init();
    m_position = ZERO_VECTOR;
}

// 0x004F06F0 (.h:84)
rl_partition_sector::~rl_partition_sector()
{
}

// 0x004C4150 (.h:88)
rl_partition_sector* rl_partition_sector::get_child(int index) const
{
    return m_children[index];
}

// 0x004C4160 (.h:93)
int rl_partition_sector::get_child_count() const
{
    return m_child_count;
}

// 0x004C4170 (.h:98)
void rl_partition_sector::set_child_count(int child_count)
{
    m_child_count = child_count;
}

// 0x004C4180 (.h:104)
const linked_list_p<rl_partition_sector_entry>& rl_partition_sector::get_entries() const
{
    return m_entries;
}

// 0x004C4190 (.h:109)
void rl_partition_sector::set_child_list(rl_partition_sector** child_list_p, int child_count)
{
    m_children = child_list_p;
    m_child_count = child_count;
}

// 0x004C41B0 (.h:116)
const rl_bvolume& rl_partition_sector::get_world_bvolume() const
{
    return m_world_bvolume;
}

// 0x004C41C0 (.h:122)
void rl_partition_sector::set_position(const vector& position)
{
    m_position = position;
}

// 0x004C41E0 (.h:127)
const vector& rl_partition_sector::get_position() const
{
    return m_position;
}

// 0x004C41F0 (.h:132)
const rl_sphere rl_partition_sector::get_world_sphere() const
{
    return m_world_bvolume.m_sphere;
}

// 0x004C4210 (.h:137)
void rl_partition_sector::get_world_sphere(rl_sphere& sphere) const
{
    m_world_bvolume.get_sphere(sphere);
}

// 0x00513EE0 (.h:147)
const rl_aabb rl_partition_sector::get_world_aabb() const
{
    rl_aabb aabb;
    m_world_bvolume.get_aabb(aabb);
    return aabb;
}

// 0x004C4230 (.h:152)
void rl_partition_sector::get_world_aabb(rl_aabb& aabb) const
{
    m_world_bvolume.get_aabb(aabb);
}

// 0x004C4310 (.h:157)
void rl_partition_sector::set_world_aabb(const rl_aabb& world_aabb)
{
    m_world_bvolume.set(world_aabb);
}
