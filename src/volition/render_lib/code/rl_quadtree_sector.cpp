// Original: volition/render_lib/code/rl_quadtree_sector.cpp
//
// rl_quadtree_sector: a quadtree node (a factory class, 4096 objects by default, game_render_system_init sets 0x12EE).
// Entities go down to the deepest child quadrant (chosen by the entity's position against the sector centre) whose
// square still contains the entity's box (or sphere), up to the tree's depth limit, and are linked into that sector.
// The sector heights follow the entities: expand_height grows the y extent of the sector and its parents. The
// header-inline members of rl_quadtree_sector.h are defined here as well.

#include "volition/render_lib/code/rl_quadtree_sector.h"

#include "volition/render_lib/code/rl_factory_rtti.h"
#include "volition/render_lib/code/rl_quadtree.h"
#include "volition/render_lib/code/rl_scene_entity.h"
#include "volition/vlib/code/memory/vmemory.h"

#include <math.h>

// <rl_quadtree_sector::factory_init> 0x00514940 (:9)
// <rl_quadtree_sector::release> 0x00514A30 (:9)
// <rl_quadtree_sector::create> 0x0052C910 (:9)
RL_FACTORY_CLASS_IMPLEMENTATION(rl_quadtree_sector, 0xE47BC810, 0x1000)

const int rl_quadtree_sector::CHILDREN_MAX = 4; // 0x012BEE44

// When set (the retail value), sectors test the entities' boxes; otherwise their bounding spheres.
bool g_quadtreeSectorInsertAABBs = true; // 0x014FA302

// 0x007C1EF0 (.h:26)
void rl_quadtree_sector::set_objects_max(int objects_max)
{
    ms_class_factory_info.m_objects_max = objects_max;
}

// 0x004F1050 (.h:63)
rl_quadtree_sector::rl_quadtree_sector()
{
    m_depth = 0;
    m_count = 0;
    set_child_list((rl_partition_sector**)m_quadrants, 0);
    m_parent_p = nullptr;
    m_max_entity_p = nullptr;
    m_min_entity_p = nullptr;
    m_quadrants[0] = nullptr;
    m_quadrants[1] = nullptr;
    m_quadrants[2] = nullptr;
    m_quadrants[3] = nullptr;
}

// 0x004F10C0 (.h:77)
rl_quadtree_sector::~rl_quadtree_sector()
{
    for (int i = 0; i < m_child_count; i++)
    {
        if (m_quadrants[i] != nullptr)
        {
            m_quadrants[i]->release();
        }
    }
}

// 0x004C4840 (.h:88)
rl_quadtree* rl_quadtree_sector::get_quadtree() const
{
    return m_quadtree_p;
}

// 0x004C4850 (.h:93)
void rl_quadtree_sector::set_quadtree(rl_quadtree* quadtree_p)
{
    m_quadtree_p = quadtree_p;
}

// 0x00514490 (.h:98)
// Whether the entity's x/z extent lies inside the sector's square (the x half extent is used for z too).
bool rl_quadtree_sector::fits_in(rl_scene_entity* entity_p)
{
    const rl_bvolume& volume = entity_p->get_world_bvolume();
    const float half = g_quadtreeSectorInsertAABBs ? m_world_bvolume.m_half_vec.x : ((m_world_bvolume.m_half_vec.x + m_world_bvolume.m_sphere.m_position.x) - (m_world_bvolume.m_sphere.m_position.x - m_world_bvolume.m_half_vec.x)) * 0.5f;
    const float extent_x = g_quadtreeSectorInsertAABBs ? volume.m_half_vec.x : volume.m_sphere.m_radius;
    const float extent_z = g_quadtreeSectorInsertAABBs ? volume.m_half_vec.z : volume.m_sphere.m_radius;
    if (volume.m_sphere.m_position.x - extent_x < m_world_bvolume.m_sphere.m_position.x - half)
    {
        return false;
    }
    if (m_world_bvolume.m_sphere.m_position.x + half < extent_x + volume.m_sphere.m_position.x)
    {
        return false;
    }
    if (volume.m_sphere.m_position.z - extent_z < m_world_bvolume.m_sphere.m_position.z - half)
    {
        return false;
    }
    return extent_z + volume.m_sphere.m_position.z <= m_world_bvolume.m_sphere.m_position.z + half;
}

// 0x004F13B0 (:16)
// Whether the entity's x/z extent lies inside the square of half size child_size around child_position.
bool rl_quadtree_sector::fits_in_child(rl_scene_entity* entity_p, const vector& child_position, float child_size) const
{
    const rl_bvolume& volume = entity_p->get_world_bvolume();
    const float extent_x = g_quadtreeSectorInsertAABBs ? volume.m_half_vec.x : volume.m_sphere.m_radius;
    const float extent_z = g_quadtreeSectorInsertAABBs ? volume.m_half_vec.z : volume.m_sphere.m_radius;
    if (volume.m_sphere.m_position.x - extent_x < child_position.x - child_size)
    {
        return false;
    }
    if (child_position.x + child_size < extent_x + volume.m_sphere.m_position.x)
    {
        return false;
    }
    if (volume.m_sphere.m_position.z - extent_z < child_position.z - child_size)
    {
        return false;
    }
    return extent_z + volume.m_sphere.m_position.z <= child_position.z + child_size;
}

// 0x0052C970 (:54)
// A child sector covering the square of half size child_size around child_position. Magic number: 16777216 (2^24).
// Notes (decompiled behaviour, kept as is): the y range is built with min and max swapped (y + 2^24 .. y - 2^24), so
// the new sector has a negative height that the first expand_height replaces with the entity's.
rl_quadtree_sector* rl_quadtree_sector::new_child(const vector& child_position, float child_size, unsigned short depth)
{
    rl_quadtree_sector* child = create();
    if (child != nullptr)
    {
        child->m_position = child_position;
        const float min_x = child_position.x + -child_size;
        const float min_y = child_position.y + 16777216.0f;
        const float min_z = child_position.z + -child_size;
        rl_bvolume& volume = child->m_world_bvolume;
        volume.m_half_vec.x = ((child_position.x + child_size) - min_x) * 0.5f;
        volume.m_half_vec.y = ((child_position.y - 16777216.0f) - min_y) * 0.5f;
        volume.m_half_vec.z = ((child_position.z + child_size) - min_z) * 0.5f;
        volume.m_sphere.m_position.x = volume.m_half_vec.x + min_x;
        volume.m_sphere.m_position.y = volume.m_half_vec.y + min_y;
        volume.m_sphere.m_position.z = volume.m_half_vec.z + min_z;
        volume.m_sphere.m_radius = sqrtf(volume.m_half_vec.x * volume.m_half_vec.x + volume.m_half_vec.y * volume.m_half_vec.y + volume.m_half_vec.z * volume.m_half_vec.z);
        child->m_quadtree_p = m_quadtree_p;
        child->m_depth = depth;
        child->m_parent_p = this;
    }
    return child;
}

// 0x0052CB60 (:96)
// Descends from this sector: the quadrant on the entity position's side of the centre (x above the centre is bit 0,
// z below it bit 1), as long as the entity fits the quadrant's square and the depth limit allows, creating the
// quadrants on the way. The entity is then linked into the sector it stopped in (and out of its previous sector).
// Notes (decompiled behaviour, kept as is): the child square is centred size / 2^(depth+1) / 2 off the centre with a
// half size of size / 2^(depth+1) (relative to the tree's half size m_size), and y stays 0.
void rl_quadtree_sector::insert(rl_scene_entity* entity_p)
{
    rl_quadtree_sector* sector = this;
    if ((int)(sector->m_depth + 1) < sector->m_quadtree_p->m_max_depth)
    {
        for (;;)
        {
            const float size = sector->m_quadtree_p->m_size;
            const float divisor = (float)(2 << (sector->m_depth & 0x1F));
            const float offset = (size / divisor) * 0.5f;
            const float child_size = ((size * 2.0f) / divisor) * 0.5f;
            const vector& entity_position = entity_p->get_position();
            const unsigned int x_bit = sector->m_position.x < entity_position.x ? 1 : 0;
            const unsigned int z_bit = entity_position.z < sector->m_position.z ? 1 : 0;
            vector child_position;
            child_position.x = sector->m_position.x + (x_bit != 0 ? offset : -offset);
            child_position.y = 0.0f;
            child_position.z = sector->m_position.z + (z_bit != 0 ? -offset : offset);
            if (!sector->fits_in_child(entity_p, child_position, child_size))
            {
                break;
            }
            const int quadrant = x_bit + z_bit * 2;
            if (sector->m_quadrants[quadrant] == nullptr)
            {
                rl_quadtree_sector* child = sector->new_child(child_position, child_size, sector->m_depth + 1);
                sector->m_quadrants[quadrant] = child;
                if (child != nullptr && sector->m_child_count < quadrant + 1)
                {
                    sector->m_child_count = quadrant + 1;
                }
            }
            rl_quadtree_sector* child = sector->m_quadrants[quadrant];
            if (child == nullptr)
            {
                break;
            }
            sector = child;
            if (child->m_quadtree_p->m_max_depth <= (int)(child->m_depth + 1))
            {
                break;
            }
        }
    }
    rl_partition_sector_entry* entry = entity_p->get_partition_sector_entry();
    rl_quadtree_sector* old_sector = (rl_quadtree_sector*)entry->m_sector_p;
    if (old_sector != sector)
    {
        if (old_sector != nullptr)
        {
            old_sector->remove(entity_p);
        }
        entry->m_sector_p = sector;
        sector->m_entries.append(entry);
    }
    sector->expand_height(entity_p);
}

// Sets the sector box (rl_bvolume::set from min/max corners, inlined).
static void set_sector_box(rl_bvolume& volume, float min_x, float min_y, float min_z, float max_x, float max_y, float max_z)
{
    volume.m_half_vec.x = (max_x - min_x) * 0.5f;
    volume.m_half_vec.y = (max_y - min_y) * 0.5f;
    volume.m_half_vec.z = (max_z - min_z) * 0.5f;
    volume.m_sphere.m_position.x = volume.m_half_vec.x + min_x;
    volume.m_sphere.m_position.y = volume.m_half_vec.y + min_y;
    volume.m_sphere.m_position.z = volume.m_half_vec.z + min_z;
    volume.m_sphere.m_radius = sqrtf(volume.m_half_vec.x * volume.m_half_vec.x + volume.m_half_vec.y * volume.m_half_vec.y + volume.m_half_vec.z * volume.m_half_vec.z);
}

// 0x00514AB0 (:163)
// Grows the y extent of this sector and its parents to the entity's (recording the entity as the lowest or highest),
// stopping at the first sector that already contains it.
void rl_quadtree_sector::expand_height(rl_scene_entity* entity_p)
{
    rl_quadtree_sector* sector = this;
    do
    {
        const rl_bvolume& entity_volume = entity_p->get_world_bvolume();
        const float entity_max_y = entity_volume.m_sphere.m_position.y + entity_volume.m_half_vec.y;
        const float entity_min_y = entity_volume.m_sphere.m_position.y - entity_volume.m_half_vec.y;
        rl_bvolume& volume = sector->m_world_bvolume;
        const float min_x = volume.m_sphere.m_position.x - volume.m_half_vec.x;
        const float min_y = volume.m_sphere.m_position.y - volume.m_half_vec.y;
        const float min_z = volume.m_sphere.m_position.z - volume.m_half_vec.z;
        const float max_x = volume.m_half_vec.x + volume.m_sphere.m_position.x;
        const float max_y = volume.m_half_vec.y + volume.m_sphere.m_position.y;
        const float max_z = volume.m_half_vec.z + volume.m_sphere.m_position.z;
        float new_min_y = min_y;
        if (min_y > entity_min_y)
        {
            sector->m_min_entity_p = entity_p;
            set_sector_box(volume, min_x, entity_min_y, min_z, max_x, max_y, max_z);
            new_min_y = entity_min_y;
        }
        if (entity_max_y <= max_y)
        {
            if (min_y <= entity_min_y)
            {
                return;
            }
        }
        else
        {
            set_sector_box(volume, min_x, new_min_y, min_z, max_x, entity_max_y, max_z);
            sector->m_max_entity_p = entity_p;
        }
        sector = sector->m_parent_p;
    } while (sector != nullptr);
}

// 0x00514EA0 (:195)
// Like expand_height for this sector, then expand_height of the parents.
void rl_quadtree_sector::minimize_height(rl_scene_entity* entity_p)
{
    const rl_bvolume& entity_volume = entity_p->get_world_bvolume();
    const float entity_max_y = entity_volume.m_sphere.m_position.y + entity_volume.m_half_vec.y;
    const float entity_min_y = entity_volume.m_sphere.m_position.y - entity_volume.m_half_vec.y;
    rl_bvolume& volume = m_world_bvolume;
    const float min_x = volume.m_sphere.m_position.x - volume.m_half_vec.x;
    float min_y = volume.m_sphere.m_position.y - volume.m_half_vec.y;
    const float max_x = volume.m_half_vec.x + volume.m_sphere.m_position.x;
    const float max_y = volume.m_half_vec.y + volume.m_sphere.m_position.y;
    const float max_z = volume.m_half_vec.z + volume.m_sphere.m_position.z;
    const float min_z = volume.m_sphere.m_position.z - volume.m_half_vec.z;
    const bool contains_min = entity_min_y <= min_y;
    if (!contains_min)
    {
        set_sector_box(volume, min_x, entity_min_y, min_z, max_x, max_y, max_z);
        min_y = entity_min_y;
    }
    if (max_y <= entity_max_y)
    {
        if (contains_min)
        {
            return;
        }
    }
    else
    {
        set_sector_box(volume, min_x, min_y, min_z, max_x, entity_max_y, max_z);
    }
    if (m_parent_p != nullptr)
    {
        m_parent_p->expand_height(entity_p);
    }
}

// 0x004F1520 (:225)
void rl_quadtree_sector::remove(rl_scene_entity* entity_p)
{
    rl_partition_sector_entry* entry = entity_p->get_partition_sector_entry();
    m_entries.remove(entry);
    entry->m_sector_p = nullptr;
    if (m_entries.get_elem_count() == 0)
    {
        m_quadtree_p->m_dirty = true;
    }
}

// 0x00515270 (:257)
// When the entity was the highest one, the highest remaining entity of the sector sets the top again (the parents
// first).
// Notes (decompiled behaviour, kept as is): only the top of the box moves (the bottom stays), and the parents are
// updated before this sector's m_max_entity_p.
void rl_quadtree_sector::recompute_max_entity(rl_scene_entity* entity_p)
{
    if (entity_p != m_max_entity_p)
    {
        return;
    }
    rl_scene_entity* best = nullptr;
    rl_partition_sector_entry* head = m_entries.first();
    if (head != nullptr)
    {
        rl_partition_sector_entry* entry = head;
        do
        {
            bool take = true;
            if (best != nullptr)
            {
                const rl_bvolume& volume = entry->m_entity_p->get_world_bvolume();
                const rl_bvolume& best_volume = best->get_world_bvolume();
                take = best_volume.m_sphere.m_position.y + best_volume.m_half_vec.y < volume.m_half_vec.y + volume.m_sphere.m_position.y;
            }
            if (take)
            {
                best = entry->m_entity_p;
            }
            entry = entry->next;
        } while (entry != m_entries.first() && entry != nullptr);
        if (best != nullptr)
        {
            const rl_bvolume& best_volume = best->get_world_bvolume();
            rl_bvolume& volume = m_world_bvolume;
            const float min_x = volume.m_sphere.m_position.x - volume.m_half_vec.x;
            const float min_y = volume.m_sphere.m_position.y - volume.m_half_vec.y;
            const float min_z = volume.m_sphere.m_position.z - volume.m_half_vec.z;
            set_sector_box(volume, min_x, min_y, min_z, volume.m_half_vec.x + volume.m_sphere.m_position.x, best_volume.m_sphere.m_position.y + best_volume.m_half_vec.y, volume.m_half_vec.z + volume.m_sphere.m_position.z);
        }
    }
    if (m_parent_p != nullptr)
    {
        m_parent_p->recompute_max_entity(entity_p);
    }
    m_max_entity_p = best;
}

// 0x00515540 (:290)
// The lowest counterpart of recompute_max_entity (only the bottom moves).
void rl_quadtree_sector::recompute_min_entity(rl_scene_entity* entity_p)
{
    if (entity_p != m_min_entity_p)
    {
        return;
    }
    rl_scene_entity* best = nullptr;
    rl_partition_sector_entry* head = m_entries.first();
    if (head != nullptr)
    {
        rl_partition_sector_entry* entry = head;
        do
        {
            bool take = true;
            if (best != nullptr)
            {
                const rl_bvolume& volume = entry->m_entity_p->get_world_bvolume();
                const rl_bvolume& best_volume = best->get_world_bvolume();
                take = volume.m_sphere.m_position.y - volume.m_half_vec.y < best_volume.m_sphere.m_position.y - best_volume.m_half_vec.y;
            }
            if (take)
            {
                best = entry->m_entity_p;
            }
            entry = entry->next;
        } while (entry != m_entries.first() && entry != nullptr);
        if (best != nullptr)
        {
            const rl_bvolume& best_volume = best->get_world_bvolume();
            rl_bvolume& volume = m_world_bvolume;
            const float min_x = volume.m_sphere.m_position.x - volume.m_half_vec.x;
            const float min_z = volume.m_sphere.m_position.z - volume.m_half_vec.z;
            set_sector_box(volume, min_x, best_volume.m_sphere.m_position.y - best_volume.m_half_vec.y, min_z, volume.m_sphere.m_position.x + volume.m_half_vec.x, volume.m_sphere.m_position.y + volume.m_half_vec.y, volume.m_half_vec.z + volume.m_sphere.m_position.z);
        }
    }
    if (m_parent_p != nullptr)
    {
        m_parent_p->recompute_min_entity(entity_p);
    }
    m_min_entity_p = best;
}

// 0x004C48B0 (:324)
// Releases the child and shrinks the child count to the last remaining quadrant.
void rl_quadtree_sector::delete_child(rl_quadtree_sector* child_p)
{
    int index = 0;
    while (m_quadrants[index] == nullptr || m_quadrants[index] != child_p)
    {
        index++;
        if (index > 3)
        {
            return;
        }
    }
    m_quadrants[index]->release();
    m_quadrants[index] = nullptr;
    int last = 3;
    while (last != -1 && m_quadrants[last] == nullptr)
    {
        last--;
    }
    m_child_count = last + 1;
}

// 0x004F1550 (:349)
// Deletes the empty leaf sectors (children first).
void rl_quadtree_sector::flush()
{
    for (int i = 0; i < 4; i++)
    {
        if (m_quadrants[i] != nullptr)
        {
            m_quadrants[i]->flush();
        }
    }
    if (m_child_count == 0 && m_entries.get_elem_count() == 0 && m_parent_p != nullptr)
    {
        m_parent_p->delete_child(this);
    }
}

// 0x004F1590 (:370)
// Unlinks all entries (and with recurse those of the children), appending them to dump_list when it is given.
void rl_quadtree_sector::remove_entries(linked_list_p<rl_partition_sector_entry>* dump_list, bool recurse)
{
    rl_partition_sector_entry* entry = m_entries.first();
    while (entry != nullptr)
    {
        rl_partition_sector_entry* next = entry->next != m_entries.first() ? entry->next : nullptr;
        rl_partition_sector_entry* entity_entry = entry->m_entity_p->get_partition_sector_entry();
        m_entries.remove(entity_entry);
        entity_entry->m_sector_p = nullptr;
        if (m_entries.get_elem_count() == 0)
        {
            m_quadtree_p->m_dirty = true;
        }
        if (dump_list != nullptr)
        {
            dump_list->append(entry);
        }
        entry = next;
    }
    if (recurse)
    {
        for (int i = 0; i < m_child_count; i++)
        {
            if (m_quadrants[i] != nullptr)
            {
                m_quadrants[i]->remove_entries(dump_list, true);
            }
        }
    }
}
