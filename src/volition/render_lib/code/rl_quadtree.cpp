// Original: volition/render_lib/code/rl_quadtree.cpp
//
// rl_quadtree: the partition system of the scenes, a quadtree over x/z of 2 * m_size (default 8192 m, so -8192..8192)
// with up to m_max_depth levels of rl_quadtree_sector. Removing entries marks the tree dirty; clean() then flushes the
// empty sectors.

#include "volition/render_lib/code/rl_quadtree.h"

#include "volition/render_lib/code/rl_aabb.h"
#include "volition/render_lib/code/rl_factory_rtti.h"
#include "volition/render_lib/code/rl_quadtree_sector.h"
#include "volition/render_lib/code/rl_scene_entity.h"
#include "volition/vlib/code/math/ix.h"

#include <math.h>

// <rl_quadtree::release> 0x004C4870 (:12)
// <rl_quadtree::create> 0x0054CB80 (:12)
RL_DYNAMIC_CLASS_IMPLEMENTATION(rl_quadtree)

// When set (the retail value), sectors test the entities' boxes; otherwise their bounding spheres.
extern bool g_quadtreeSectorInsertAABBs; // 0x014FA302 (rl_quadtree_sector.cpp)

// 0x00543160 (:19)
// Magic numbers: 8192 m (the half size of the root), 11 levels.
// Notes (decompiled behaviour, kept as is): m_dirty is left uninitialized.
rl_quadtree::rl_quadtree()
{
    set_root_sector(rl_quadtree_sector::create());
    m_size = 8192.0f;
    m_max_depth = 11;
    init();
}

// 0x004C4890 (:30)
rl_quadtree::~rl_quadtree()
{
}

// 0x004F1100 (:35)
// The root covers -m_size..m_size in x and z, with an empty height at y 0.
void rl_quadtree::init()
{
    rl_quadtree_sector* root = (rl_quadtree_sector*)get_root_sector();
    const float max = m_size;
    const float min = -m_size;
    rl_bvolume& volume = root->m_world_bvolume;
    volume.m_half_vec.x = (max - min) * 0.5f;
    volume.m_half_vec.y = 0.0f;
    volume.m_half_vec.z = (max - min) * 0.5f;
    volume.m_sphere.m_position.x = volume.m_half_vec.x + min;
    volume.m_sphere.m_position.y = volume.m_half_vec.y + 0.0f;
    volume.m_sphere.m_position.z = volume.m_half_vec.z + min;
    volume.m_sphere.m_radius = sqrtf(volume.m_half_vec.y * volume.m_half_vec.y + volume.m_half_vec.x * volume.m_half_vec.x + volume.m_half_vec.z * volume.m_half_vec.z);
    root->m_position.x = (max + min) * 0.5f;
    root->m_position.y = 0.0f;
    root->m_position.z = (max + min) * 0.5f;
    root->set_quadtree(this);
    set_root_sector(root);
}

// 0x00514660 (:52)
// Takes every entity out of the tree, flushes the sectors, resets the root and inserts the entities again.
void rl_quadtree::reset()
{
    rl_quadtree_sector* root = (rl_quadtree_sector*)get_root_sector();
    linked_list_p<rl_partition_sector_entry> dump_list;
    root->remove_entries(&dump_list, true);
    for (int i = 0; i < 4; i++)
    {
        if (root->m_quadrants[i] != nullptr)
        {
            root->m_quadrants[i]->flush();
        }
    }
    if (root->get_child_count() == 0 && root->m_entries.get_elem_count() == 0 && root->m_parent_p != nullptr)
    {
        root->m_parent_p->delete_child(root);
    }
    init();
    rl_partition_sector_entry* head = dump_list.first();
    for (rl_partition_sector_entry* entry = head; entry != nullptr;)
    {
        rl_partition_sector_entry* next = entry->next != head ? entry->next : nullptr;
        insert(entry->m_entity_p);
        entry = next;
    }
}

// 0x004F1300 (:77)
bool rl_quadtree::validate_entity_not_inserted(rl_scene_entity* entity_p)
{
    return !get_root_sector()->temp_validate_exists(entity_p);
}

// 0x005431E0 (:82)
// An entity that is in a sector and still fits it (rl_quadtree_sector::fits_in, inlined) is re-inserted from that
// sector down; otherwise from the root.
void rl_quadtree::insert(rl_scene_entity* entity_p)
{
    rl_quadtree_sector* sector = (rl_quadtree_sector*)entity_p->get_partition_sector_entry()->m_sector_p;
    if (sector == nullptr || !sector->fits_in(entity_p))
    {
        sector = (rl_quadtree_sector*)get_root_sector();
    }
    sector->insert(entity_p);
}

// 0x005146F0 (:118)
// rl_quadtree_sector::remove (inlined), then the tree is dirty when the sector became empty.
void rl_quadtree::remove(rl_scene_entity* entity_p)
{
    rl_partition_sector_entry* entry = entity_p->get_partition_sector_entry();
    rl_quadtree_sector* sector = (rl_quadtree_sector*)entry->m_sector_p;
    sector->remove(entity_p);
    entry->m_sector_p = nullptr;
    if (sector->m_entries.get_elem_count() == 0)
    {
        m_dirty = true;
    }
}

// 0x00514740 (:131)
void rl_quadtree::clean()
{
    if (m_dirty)
    {
        ((rl_quadtree_sector*)get_root_sector())->flush();
        m_dirty = false;
    }
}

// 0x00514760 (:209)
// The entities of the sector and its children whose world box intersects aabb (and that are in the visibility list
// visibility_index, unless it is -1), at most max_entities.
int rl_quadtree::query_aabb_quadrant(rl_quadtree_sector* sector_p, rl_scene_entity** entities, int max_entities, const rl_aabb& aabb, int visibility_index)
{
    if (max_entities < 1)
    {
        return 0;
    }
    int count = 0;
    rl_partition_sector_entry* head = sector_p->m_entries.first();
    for (rl_partition_sector_entry* entry = head; entry != nullptr;)
    {
        rl_scene_entity* entity = entry->m_entity_p;
        const rl_bvolume& volume = entity->get_world_bvolume();
        vector entity_min;
        vector entity_max;
        entity_min.x = volume.m_sphere.m_position.x - volume.m_half_vec.x;
        entity_min.y = volume.m_sphere.m_position.y - volume.m_half_vec.y;
        entity_min.z = volume.m_sphere.m_position.z - volume.m_half_vec.z;
        entity_max.x = volume.m_half_vec.x + volume.m_sphere.m_position.x;
        entity_max.y = volume.m_sphere.m_position.y + volume.m_half_vec.y;
        entity_max.z = volume.m_half_vec.z + volume.m_sphere.m_position.z;
        if (ix_box_box_aligned(entity_min, entity_max, aabb.min, aabb.max) && (visibility_index == -1 || visibility_index == entry->m_entity_p->get_visibility_list_index()))
        {
            entities[count++] = entry->m_entity_p;
            if (count == max_entities)
            {
                return count;
            }
        }
        entry = entry->next;
        if (entry == head)
        {
            break;
        }
    }
    for (int i = 0; i < sector_p->get_child_count(); i++)
    {
        rl_quadtree_sector* child = (rl_quadtree_sector*)sector_p->get_child(i);
        if (child != nullptr)
        {
            count += query_aabb_quadrant(child, entities + count, max_entities - count, aabb, visibility_index);
        }
    }
    return count;
}

// 0x00514920 (:243)
int rl_quadtree::query_aabb(rl_scene_entity** entities, int max_entities, const rl_aabb& aabb, int visibility_index)
{
    return query_aabb_quadrant((rl_quadtree_sector*)get_root_sector(), entities, max_entities, aabb, visibility_index);
}

// 0x004C47E0 (.h:84)
int rl_quadtree::get_max_depth() const
{
    return m_max_depth;
}

// 0x007C1EC0 (.h:89)
void rl_quadtree::set_max_depth(int max_depth)
{
    m_max_depth = max_depth;
}

// 0x004C47F0 (.h:95)
float rl_quadtree::get_size() const
{
    return m_size;
}

// 0x007C1ED0 (.h:100)
void rl_quadtree::set_size(float size)
{
    m_size = size;
}

// 0x004C4800 (.h:106)
bool rl_quadtree::get_dirty() const
{
    return m_dirty;
}

// 0x004C4810 (.h:111)
void rl_quadtree::set_dirty(bool dirty)
{
    m_dirty = dirty;
}
