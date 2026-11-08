// Original: volition/rfg/code/havok/havok_game_filter.cpp
//
// The collision filter of the game world (decompiled). A filter info is a collision layer (low 16 bits) and a
// system group (high 16 bits): two infos of the same non-zero system group never collide, otherwise bit
// layer_a * 128 + layer_b of the layer table decides (kept symmetric by enable/disableCollisionsBetween). For
// collidable pairs, a world object with an ignore handle (property 2) does not collide with that object; ray queries
// also honour the object's ray flags (property 8) against m_current_ray_flags and the filter_callback.
// Compiled in rfg_havok_unity.cpp.

#include "volition/rfg/code/havok/havok_game_filter.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcollidable.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshapecontainer.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/query/hkpshaperaycastinput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/query/castutil/hkpworldraycastinput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/hkpworldobject.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/objects/object.h"

#include "keen/base/trace.hpp"

#include <string.h>

// ---- globals
havok_game_filter* Havok_filter; // 0x02642F84
bool (*havok_game_filter::filter_callback)(object*, void*); // 0x02B82278
void* havok_game_filter::filter_params; // 0x02B8227C
unsigned int havok_game_filter::m_current_ray_flags; // 0x02B82280

// ---- havok_disable_property_asserts
// 0x00811440 (:59)
havok_disable_property_asserts::havok_disable_property_asserts()
{
}

// 0x00811450 (:80)
havok_disable_property_asserts::~havok_disable_property_asserts()
{
}

// ---- havok_game_filter (header inlines)
// 0x00A068D0 (.h:75)
// Hands out system groups after the predefined ones (wrapping back to them past 0xFFFF).
int havok_game_filter::getNewSystemGroup()
{
    if (m_nextFreeSystemGroup > 0xFFFF)
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        m_nextFreeSystemGroup = m_num_predefined_system_groups;
    }
    return m_nextFreeSystemGroup++;
}

// 0x00A06910 (.h:91)
unsigned int havok_game_filter::get_system_group(unsigned int filter_info)
{
    return filter_info >> 16;
}

// 0x0080D6F0 (.h:101)
unsigned int havok_game_filter::calcFilterInfo(int layer, int systemGroup)
{
    return (systemGroup << 16) | layer;
}

// 0x007115D0 (.h:111)
int havok_game_filter::get_layer(unsigned int coll_type)
{
    return coll_type & 0xFFFF;
}

// 0x0080D700 (.h:118)
unsigned int havok_game_filter::get_table_index(unsigned int layer_a, unsigned int layer_b)
{
    return layer_a * MAX_LAYERS + layer_b;
}

// 0x0080D710 (.h:124)
hkBool havok_game_filter::isCollisionEnabled(unsigned int info_a, unsigned int info_b) const
{
    hkBool result;
    if ((info_a & 0xFFFF0000) != 0 && ((info_a ^ info_b) & 0xFFFF0000) == 0)
    {
        result.m_bool = 0; // same system group
        return result;
    }
    const unsigned int index = get_table_index(info_a & 0xFFFF, info_b & 0xFFFF);
    result.m_bool = (m_layer_lookup_table[index >> 5] & (1u << (index & 31))) != 0;
    return result;
}

// 0x0081BD50 (:124)
// LTCG: a_world_obj in ECX, b_world_obj in EDI. Whether a's ignore handle (property 2) is b's object.
static bool check_for_ignore_handle(const hkpWorldObject* a_world_obj, const hkpWorldObject* b_world_obj)
{
    if (a_world_obj == nullptr || !a_world_obj->hasProperty(HAVOK_PROPERTY_IGNORE_HANDLE, hkpWorldObject::MULTI_THREADING_CHECKS_ENABLE))
    {
        return false;
    }
    const unsigned int ignore_handle = (unsigned int)a_world_obj->getProperty(HAVOK_PROPERTY_IGNORE_HANDLE, hkpWorldObject::MULTI_THREADING_CHECKS_ENABLE).m_data;
    if (ignore_handle == 0xFFFFFFFF || b_world_obj == nullptr)
    {
        return false;
    }
    const object* objp = object_from_world_obj(b_world_obj);
    return objp != nullptr && objp->get_handle() == ignore_handle;
}

// 0x0081BDB0 (:155)
// Every layer collides with every layer until havok_system_init sets up the table.
havok_game_filter::havok_game_filter(unsigned int num_predefined_system_groups, unsigned int* layer_lookup_table)
    : hkpCollisionFilter()
{
    m_num_predefined_system_groups = num_predefined_system_groups;
    m_layer_lookup_table = layer_lookup_table;
    m_type.m_storage = HK_FILTER_CUSTOM;
    m_nextFreeSystemGroup = num_predefined_system_groups;
    memset(layer_lookup_table, 0xFF, NUM_BYTES_IN_LAYER_TABLE);
}

// 0x0081BF40 (:183)
// Layers and system groups, then the ignore handles both ways, then the filter callback on b's object.
hkBool havok_game_filter::isCollisionEnabled(const hkpCollidable& coll_a, const hkpCollidable& coll_b) const
{
    hkBool result;
    if (!isCollisionEnabled(coll_a.m_broadPhaseHandle.m_collisionFilterInfo, coll_b.m_broadPhaseHandle.m_collisionFilterInfo).m_bool)
    {
        result.m_bool = 0;
        return result;
    }
    const hkpWorldObject* a = (const hkpWorldObject*)coll_a.getOwner();
    const hkpWorldObject* b = (const hkpWorldObject*)coll_b.getOwner();
    if (check_for_ignore_handle(a, b) || check_for_ignore_handle(b, a))
    {
        result.m_bool = 0;
        return result;
    }
    bool (*callback)(object*, void*) = filter_callback;
    if (callback != nullptr)
    {
        object* objp = object_from_world_obj(b);
        if (objp != nullptr)
        {
            result.m_bool = callback(objp, filter_params);
            return result;
        }
    }
    result.m_bool = 1;
    return result;
}

// 0x0081C010 (:233)
// Ray against a collidable: layers and system groups, then the object's ray flags (property 8) against the current
// ray's flags, then the filter callback.
hkBool havok_game_filter::isCollisionEnabled(const hkpWorldRayCastInput& input_a, const hkpCollidable& coll_b) const
{
    hkBool result;
    const unsigned int ray_flags = m_current_ray_flags;
    if (!isCollisionEnabled(input_a.m_filterInfo, coll_b.m_broadPhaseHandle.m_collisionFilterInfo).m_bool)
    {
        result.m_bool = 0;
        return result;
    }
    hkpWorldObject* obj = (hkpWorldObject*)coll_b.getOwner();
    if (m_current_ray_flags != 0 && (ray_flags & (unsigned int)havok_get_property(obj, 8, nullptr, true)) != 0) // property 8: rays the object ignores
    {
        result.m_bool = 0;
        return result;
    }
    bool (*callback)(object*, void*) = filter_callback;
    if (callback != nullptr)
    {
        object* objp = object_from_world_obj(obj);
        if (objp != nullptr)
        {
            result.m_bool = callback(objp, filter_params);
            return result;
        }
    }
    result.m_bool = 1;
    return result;
}

// 0x00811460 (:282)
// Ray against a child shape of a collection: the child's filter info.
hkBool havok_game_filter::isCollisionEnabled(const hkpShapeRayCastInput& input_a, const hkpShapeContainer& cont_b, unsigned int key_b) const
{
    return isCollisionEnabled(input_a.m_filterInfo, cont_b.getCollisionFilterInfo(key_b));
}

// The filter info of a body's root collidable.
static unsigned int havok_root_filter_info(const hkpCdBody* body)
{
    while (body->m_parent != nullptr)
    {
        body = body->m_parent;
    }
    return ((const hkpCollidable*)body)->m_broadPhaseHandle.m_collisionFilterInfo;
}

// 0x008114D0 (:319)
// A body against a child shape: the child's filter info, or b's root filter info when the child has none.
hkBool havok_game_filter::isCollisionEnabled(const hkpCollisionInput& input_a, const hkpCdBody& body_a, const hkpCdBody& body_b, const hkpShapeContainer& cont_b, unsigned int key_b) const
{
    const unsigned int info_a = havok_root_filter_info(&body_a);
    unsigned int info_b = cont_b.getCollisionFilterInfo(key_b);
    if (info_b == 0)
    {
        info_b = havok_root_filter_info(&body_b);
    }
    return isCollisionEnabled(info_a, info_b);
}

// 0x00811570 (:375)
// Two child shapes: their filter infos, or their root bodies' when they have none.
hkBool havok_game_filter::isCollisionEnabled(const hkpCollisionInput& input, const hkpCdBody& body_a, const hkpCdBody& body_b, const hkpShapeContainer& cont_a, const hkpShapeContainer& cont_b, unsigned int key_a, unsigned int key_b) const
{
    unsigned int info_a = cont_a.getCollisionFilterInfo(key_a);
    unsigned int info_b = cont_b.getCollisionFilterInfo(key_b);
    if (info_a == 0)
    {
        info_a = havok_root_filter_info(&body_a);
    }
    if (info_b == 0)
    {
        info_b = havok_root_filter_info(&body_b);
    }
    return isCollisionEnabled(info_a, info_b);
}

// 0x00811620 (:400)
void havok_game_filter::enableCollisionsBetween(unsigned int layer_a, unsigned int layer_b)
{
    const unsigned int ab = get_table_index(layer_a, layer_b);
    const unsigned int ba = get_table_index(layer_b, layer_a);
    m_layer_lookup_table[ab >> 5] |= 1u << (ab & 31);
    m_layer_lookup_table[ba >> 5] |= 1u << (ba & 31);
}

// 0x00811670 (:436)
void havok_game_filter::disableCollisionsBetween(unsigned int layer_a, unsigned int layer_b)
{
    const unsigned int ab = get_table_index(layer_a, layer_b);
    const unsigned int ba = get_table_index(layer_b, layer_a);
    m_layer_lookup_table[ab >> 5] &= ~(1u << (ab & 31));
    m_layer_lookup_table[ba >> 5] &= ~(1u << (ba & 31));
}

// 0x008116D0 (:477)
// Layers 1..127 (layer 0 is left alone).
void havok_game_filter::disable_collisions_for_layer(unsigned int layer)
{
    for (unsigned int other = 1; other < MAX_LAYERS; other++)
    {
        disableCollisionsBetween(layer, other);
    }
}

// 0x00811700 (:495)
// dest_layer collides with what source_layer collides with (layers 1..127).
void havok_game_filter::copy_layer(unsigned int dest_layer, unsigned int source_layer)
{
    for (unsigned int other = 1; other < MAX_LAYERS; other++)
    {
        if (isCollisionEnabled(source_layer, other).m_bool)
        {
            enableCollisionsBetween(dest_layer, other);
        }
        else
        {
            disableCollisionsBetween(dest_layer, other);
        }
    }
}

// 0x0081BE20
havok_game_filter::~havok_game_filter()
{
}
