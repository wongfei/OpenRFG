// Original: volition/rfg/code/objects/movers/object_mover_lists.cpp
//
// Pooled lists of mover handles by mover type (decompiled; header comment in object_mover_lists.h). Compiled in
// rfg_objects_unity.cpp.

#include "volition/rfg/code/objects/movers/object_mover_lists.h"

#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/level/world.inl"
#include "volition/rfg/code/objects/movers/object_mover.h"

// Built with obj_handle_wrapper::reset as the release callback (0x010760F0).
object_pool<obj_handle_wrapper, 128, 7> Object_mover_list_set(nullptr, &obj_handle_wrapper::reset); // 0x03043590

// ---- obj_handle_wrapper (header inlines)
// 0x00A33CA0 (.h:30)
obj_handle_wrapper::obj_handle_wrapper()
{
    handle = 0xFFFFFFFF;
}

// 0x00A100E0 (.h:34)
void obj_handle_wrapper::reset()
{
    handle = 0xFFFFFFFF;
}

// 0x00AA2BA0 (:29)
void oml_level_init_pre()
{
    Object_mover_list_set.clear();
}

// 0x00A5FB20 (:37)
void oml_add_new_object(unsigned int handle, mover_flag_types flag_type)
{
    if (flag_type >= NUM_MOVER_FLAG_TYPES)
    {
        return;
    }
    obj_handle_wrapper* wrapper = Object_mover_list_set.alloc(flag_type);
    if (wrapper != nullptr)
    {
        wrapper->handle = handle;
    }
}

// 0x00A8C160 (:52)
// Removes every entry of the handle from the type's list.
void oml_remove_object(unsigned int handle, mover_flag_types flag_type)
{
    if (flag_type >= NUM_MOVER_FLAG_TYPES)
    {
        return;
    }
    Object_mover_list_set.thread_access.enter();
    obj_handle_wrapper* wrapper = Object_mover_list_set.get_first(flag_type);
    while (wrapper != nullptr)
    {
        obj_handle_wrapper* next = Object_mover_list_set.get_next(wrapper);
        if (wrapper->handle == handle)
        {
            Object_mover_list_set.free(wrapper);
        }
        wrapper = next;
    }
    Object_mover_list_set.thread_access.leave();
}

// 0x00A5FB40 (:69)
// The closest existing mover of the type, or nullptr.
object_mover* oml_find_nearest_mover(const vector& pos, mover_flag_types flag_type)
{
    if (flag_type >= NUM_MOVER_FLAG_TYPES)
    {
        return nullptr;
    }
    object_mover* nearest = nullptr;
    float best_dist = 3.4028235e+38f; // FLT_MAX
    Object_mover_list_set.thread_access.enter();
    for (obj_handle_wrapper* wrapper = Object_mover_list_set.get_first(flag_type); wrapper != nullptr; wrapper = Object_mover_list_set.get_next(wrapper))
    {
        object_mover* omp = World.object_from_handle<object_mover>(wrapper->handle);
        if (omp == nullptr)
        {
            continue;
        }
        const float dist = vec_dist_squared(omp->pos, pos);
        if (dist < best_dist)
        {
            nearest = omp;
            best_dist = dist;
        }
    }
    Object_mover_list_set.thread_access.leave();
    return nearest;
}
