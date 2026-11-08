// Original: volition/rfg/code/level/world.inl
//
// Template definitions of world (declared in world.h).
//
// Partial: get_num_objects, get_object, get_num_free_objects, object_from_handle, object_cast, collect_objects,
// create_object and create_object_global. Behaviour of the instantiations (decompiled):
// - get_num_objects<T>: size of subtype_objects[T::get_subtype_static()] for classes with a subtype, otherwise of
//   type_objects[T::get_type_static()]. get_object<T>(i) indexes the same list into all_objects.
// - get_num_free_objects<T>: free entries in T's object pool.
// - object_from_handle<T>: object_from_handle(handle), nullptr unless its obj_type (and sub_type) match T.
//   object_cast<T> does the same check on a pointer; is_exact_object_type<T> requires both type and subtype.
// - collect_objects<T>: handles of the T objects whose bounding box (get_bbox) intersects [bmin, bmax] (or a sphere),
//   filtered by collect_cb. Up to 100 objects of the type are scanned linearly; above that the Havok broadphase is queried
//   (_havok_collect_objects_by_type_fast).
// - create_object<T>: create_object_internal(T::get_classname_hash(), pblock, nullptr, parent);
//   create_object_global<T> passes a fixed origin zone (global_zone_grid[15] in the decompile).
// - world_collect_descendants<T>: see world.h. object::collect_children<T> (also defined here) is the one-level
//   version and comes with object.h in Tier 6.
#pragma once

#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/objects/object.h"
#include "volition/vlib/code/math/bbox.h"
#include "volition/vlib/code/math/ix.h"

// <automobile> 0x00649800 (world.inl:16)
template<typename T>
int world::get_num_objects() const
{
    if (T::get_subtype_static() != OT_SUB_UNDEFINED)
    {
        return subtype_objects[T::get_subtype_static()].size();
    }
    return type_objects[T::get_type_static()].size();
}

// <weapon> 0x00A25570 (world.inl:39)
template<typename T>
int world::get_num_free_objects() const
{
    return T::pool_remaining();
}

// <player_start> 0x0077CD70 (world.inl:44)
template<typename T>
T* world::get_object(int i) const
{
    if (T::get_subtype_static() != OT_SUB_UNDEFINED)
    {
        return (T*)all_objects[subtype_objects[T::get_subtype_static()][i]];
    }
    return (T*)all_objects[type_objects[T::get_type_static()][i]];
}

// <player_start> 0x0093D6F0 (world.inl:54)
template<typename T>
T* world::object_from_handle(unsigned int handle)
{
    if (handle == 0xFFFFFFFF)
    {
        return nullptr;
    }
    object* objp = object_from_handle(handle);
    if (objp == nullptr || objp->get_obj_type() != T::get_type_static())
    {
        return nullptr;
    }
    if (T::get_subtype_static() != OT_SUB_UNDEFINED && objp->get_sub_type() != T::get_subtype_static())
    {
        return nullptr;
    }
    return (T*)objp;
}

// <rfg_mover> 0x005F8860 (world.inl:176)
template<typename T>
T* world::object_cast(object* objp) const
{
    if (objp == nullptr || objp->get_obj_type() != T::get_type_static())
    {
        return nullptr;
    }
    if (T::get_subtype_static() != OT_SUB_UNDEFINED && objp->get_sub_type() != T::get_subtype_static())
    {
        return nullptr;
    }
    return (T*)objp;
}

// <player_start> 0x00925F40 (world.inl:72)
template<typename T>
int world::collect_objects(const vector& bmin, const vector& bmax, unsigned int* objects, int max_objects, bool (*collect_cb)(object*, unsigned int, const void*), const void* cb_user_data) const
{
    // Up to 100 objects of the class are tested one by one; more go through the class's broadphase.
    const int num = get_num_objects<T>();
    if (num < 1)
    {
        return 0;
    }
    if (num > 100)
    {
        return _havok_collect_objects_by_type_fast(bmin, bmax, T::get_bpo_index_static(), T::get_type_static(), T::get_subtype_static(), objects, max_objects, collect_cb, cb_user_data, nullptr);
    }
    int count = 0;
    for (int i = 0; i < num; i++)
    {
        object* objp = get_object<T>(i);
        vector obj_min;
        vector obj_max;
        objp->get_bbox(obj_min, obj_max);
        if (ix_box_box_aligned(bmin, bmax, obj_min, obj_max) && (collect_cb == nullptr || collect_cb(objp, 0xFFFFFFFF, cb_user_data)))
        {
            objects[count++] = objp->get_handle();
            if (count == max_objects)
            {
                return count;
            }
        }
    }
    return count;
}

// <object_mover> 0x00722EC0
// The sphere version: bounding boxes touching the sphere.
template<typename T>
int world::collect_objects(const vector& pos, float radius, unsigned int* objects, int max_objects, bool (*collect_cb)(object*, unsigned int, const void*), const void* cb_user_data) const
{
    const int num = get_num_objects<T>();
    if (num < 1)
    {
        return 0;
    }
    if (num > 100)
    {
        return _havok_collect_objects_by_type_fast(pos, radius, T::get_bpo_index_static(), T::get_type_static(), T::get_subtype_static(), objects, max_objects, collect_cb, cb_user_data);
    }
    int count = 0;
    for (int i = 0; i < num; i++)
    {
        object* objp = get_object<T>(i);
        vector obj_min;
        vector obj_max;
        objp->get_bbox(obj_min, obj_max);
        if (ix_sphere_in_box(pos, radius, obj_min, obj_max) && (collect_cb == nullptr || collect_cb(objp, 0xFFFFFFFF, cb_user_data)))
        {
            objects[count++] = objp->get_handle();
            if (count == max_objects)
            {
                return count;
            }
        }
    }
    return count;
}

// <human> 0x00A48FF0
// The oriented box version: a local box at pos/orient against each object's local box at its own place and orient.
template<typename T>
int world::collect_objects(const vector& local_bmin, const vector& local_bmax, const vector& pos, const matrix& orient, unsigned int* objects, int max_objects, bool (*collect_cb)(object*, unsigned int, const void*), const void* cb_user_data) const
{
    const int num = get_num_objects<T>();
    if (num < 1)
    {
        return 0;
    }
    if (num > 100)
    {
        return _havok_collect_objects_by_type_fast(local_bmin, local_bmax, pos, orient, T::get_bpo_index_static(), T::get_type_static(), T::get_subtype_static(), objects, max_objects, collect_cb, cb_user_data);
    }
    vector extents;
    vector center;
    bbox_extract_obb_info(local_bmin, local_bmax, pos, orient, extents, center);
    int count = 0;
    for (int i = 0; i < num; i++)
    {
        object* objp = get_object<T>(i);
        vector obj_min;
        vector obj_max;
        objp->get_local_bbox(obj_min, obj_max);
        vector obj_extents;
        vector obj_center;
        bbox_extract_obb_info(obj_min, obj_max, objp->pos, objp->orient, obj_extents, obj_center);
        if (ix_oriented_box_box(extents, center, orient, obj_extents, obj_center, objp->orient) && (collect_cb == nullptr || collect_cb(objp, 0xFFFFFFFF, cb_user_data)))
        {
            objects[count++] = objp->get_handle();
            if (count == max_objects)
            {
                return count;
            }
        }
    }
    return count;
}

// <object_squad> 0x00767130 (world.inl:236)
template<typename T>
T* world::create_object(obj_prop_block& pblock, unsigned int parent)
{
    return (T*)create_object_internal(T::get_classname_hash(), pblock, nullptr, parent);
}

// <district> 0x00767090 (world.inl:243)
template<typename T>
T* world::create_object_global(obj_prop_block& pblock, unsigned int parent)
{
    // Global objects originate in the persistent zone (15, 0).
    return (T*)create_object_internal(T::get_classname_hash(), pblock, global_zone_grid[15], parent);
}

// 0x00A05500 (world.h:648): the const list; depth first; true when obj_list is full. Unlike the typed versions it does
// not check top_objp, and the recursion goes through the non-const 0x0092D070 with the same list (original cast).
inline bool world_collect_descendants(const object* top_objp, base_array<const object*>& obj_list, bool (*cb_func)(object*, unsigned int, const void*), const void* cb_data)
{
    if (obj_list.full())
    {
        return true;
    }
    object* child = top_objp->child_ptr;
    while (child != nullptr)
    {
        if (cb_func == nullptr || cb_func(child, 0xFFFFFFFF, cb_data))
        {
            const object* added = child;
            obj_list.add(added);
            if (obj_list.full())
            {
                return true;
            }
        }
        if (world_collect_descendants(child, reinterpret_cast<base_array<object*>&>(obj_list), cb_func, cb_data))
        {
            return true;
        }
        child = child->child_next;
        if (child == top_objp->child_ptr)
        {
            return false;
        }
    }
    return false;
}

// <cover_node> 0x0064A060, <object_dummy> 0x00A4D680 (world.inl:298): the non-const list; depth first; true when obj_list is full.
template<typename T>
bool world_collect_descendants(const object* top_objp, base_array<T*>& obj_list, bool (*cb_func)(object*, unsigned int, const void*), const void* cb_data)
{
    if (top_objp == nullptr || obj_list.full())
    {
        return obj_list.full();
    }
    object* child = top_objp->child_ptr;
    while (child != nullptr)
    {
        if (child->get_obj_type() == T::get_type_static() && (cb_func == nullptr || cb_func(child, 0xFFFFFFFF, cb_data)))
        {
            T* typed = static_cast<T*>(child);
            obj_list.add(typed);
            if (obj_list.full())
            {
                return true;
            }
        }
        if (world_collect_descendants<T>(child, obj_list, cb_func, cb_data))
        {
            return true;
        }
        child = child->child_next;
        if (child == top_objp->child_ptr)
        {
            return false;
        }
    }
    return false;
}

// <navpoint> 0x006726F0 (world.h:644): depth first; true when obj_list is full.
template<typename T>
bool world_collect_descendants(const object* top_objp, base_array<const T*>& obj_list, bool (*cb_func)(object*, unsigned int, const void*), const void* cb_data)
{
    if (top_objp == nullptr || obj_list.full())
    {
        return obj_list.full();
    }
    object* child = top_objp->child_ptr;
    while (child != nullptr)
    {
        if (child->get_obj_type() == T::get_type_static() && (cb_func == nullptr || cb_func(child, 0xFFFFFFFF, cb_data)))
        {
            const T* typed = static_cast<const T*>(child);
            obj_list.add(typed);
            if (obj_list.full())
            {
                return true;
            }
        }
        if (world_collect_descendants<T>(child, obj_list, cb_func, cb_data))
        {
            return true;
        }
        child = child->child_next;
        if (child == top_objp->child_ptr)
        {
            return false;
        }
    }
    return false;
}
