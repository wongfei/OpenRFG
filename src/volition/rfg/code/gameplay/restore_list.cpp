// Original: volition/rfg/code/gameplay/restore_list.cpp
//
// Restore lists: which objects world::restore_object brings back when a mission starts. The handles live in one array,
// and a list is a range of it; two associated objects with the same objects share the range.

#include "volition/rfg/code/gameplay/restore_list.h"

#include "keen/base/trace.hpp"
#include "volition/rfg/code/level/world.h"
#include "volition/vlib/code/cfile/cfile.h"

// Magic number: the size of the shared handle array, also the array size in the farray argument types.
static const int MAX_RESTORABLE_OBJECTS = 800;
static unsigned int All_restorable_objects[MAX_RESTORABLE_OBJECTS]; // 0x02225AF0
static farray<restore_list,128> All_restore_lists; // 0x01658518, capacity from its initializer 0x0106BE70

// 0x007750F0 (:26)
restore_list::restore_list()
    : associated_object_handle(0xFFFFFFFF)
    , object_count(0)
    , obj_list(nullptr)
    , restoring(false)
{
}

// 0x0078F8B0 (:76)
bool lists_are_the_same(const restore_list& current_list, const farray<unsigned int,800>& new_list)
{
    const int count = new_list.size();
    if (current_list.object_count != count)
    {
        return false;
    }
    for (int i = 0; i < count; i++)
    {
        bool found = false;
        for (int j = 0; j < count && !found; j++)
        {
            found = new_list[j] == current_list.obj_list[i];
        }
        if (!found)
        {
            return false;
        }
    }
    return true;
}

// 0x0078F910 (:95)
restore_list* find_restore_list(const farray<unsigned int,800>& new_list)
{
    for (int i = 0; i < All_restore_lists.size(); i++)
    {
        if (lists_are_the_same(All_restore_lists[i], new_list))
        {
            return &All_restore_lists[i];
        }
    }
    return nullptr;
}

// 0x00775110 (:111)
restore_list* find_restore_list(unsigned int associated_object_handle)
{
    for (int i = 0; i < All_restore_lists.size(); i++)
    {
        if (All_restore_lists[i].associated_object_handle == associated_object_handle)
        {
            return &All_restore_lists[i];
        }
    }
    return nullptr;
}

// 0x0078F9A0 (:129)
void create_new_restore_list(unsigned int associated_object_handle, restore_list* existing_list)
{
    if (All_restore_lists.size() < All_restore_lists.capacity())
    {
        restore_list shared = *existing_list;
        shared.associated_object_handle = associated_object_handle;
        shared.restoring = false;
        All_restore_lists.add(shared);
    }
}

// 0x0078FA40 (:145)
void create_new_restore_list(unsigned int associated_object_handle, const farray<unsigned int,800>& new_list)
{
    if (All_restore_lists.size() >= All_restore_lists.capacity())
    {
        return;
    }
    int first_free = 0;
    while (All_restorable_objects[first_free] != 0xFFFFFFFF)
    {
        first_free++;
        if (first_free >= MAX_RESTORABLE_OBJECTS)
        {
            return;
        }
    }
    restore_list list;
    list.associated_object_handle = associated_object_handle;
    list.obj_list = All_restorable_objects + first_free;
    All_restore_lists.add(list);
    restore_list& added = All_restore_lists[All_restore_lists.size() - 1];
    if (added.associated_object_handle == associated_object_handle)
    {
        added.object_count = 0;
        while (added.object_count < new_list.size())
        {
            const int slot = added.object_count + first_free;
            if (slot >= MAX_RESTORABLE_OBJECTS)
            {
                return;
            }
            All_restorable_objects[slot] = new_list[added.object_count];
            added.object_count++;
        }
    }
}

// 0x0075F250 (:207)
void restore_list::reset()
{
    associated_object_handle = 0xFFFFFFFF;
    object_count = 0;
    obj_list = nullptr;
    restoring = false;
}

// 0x0075F270 (:216)
void restore_list::system_save(cfile* save_file)
{
    cf_write_uint32(associated_object_handle, save_file);
    cf_write_uint32((unsigned int)object_count, save_file);
    cf_write_uint32((unsigned int)(obj_list - All_restorable_objects), save_file);
}

// 0x0075F2B0 (:229)
void restore_list::system_load(cfile* load_file)
{
    reset();
    associated_object_handle = cf_read_uint32(load_file);
    object_count = (int)cf_read_uint32(load_file);
    const unsigned int first = cf_read_uint32(load_file);
    if (first < (unsigned int)MAX_RESTORABLE_OBJECTS)
    {
        obj_list = All_restorable_objects + first;
        return;
    }
    // Sic: rfg.exe's spelling.
    keen::debug::nativePrintArgs("Invalid index into the array of resotrable objects. This Save game is invalid.");
}

// 0x00775150 (:258)
void restore_list_level_init()
{
    for (int i = 0; i < MAX_RESTORABLE_OBJECTS; i++)
    {
        All_restorable_objects[i] = 0xFFFFFFFF;
    }
    All_restore_lists.clear();
}

// 0x0078FB20 (:271)
void restore_list_get_list(unsigned int associated_object_handle, farray<unsigned int,800>& returned_list)
{
    returned_list.clear();
    restore_list* list = find_restore_list(associated_object_handle);
    if (list != nullptr)
    {
        for (int i = 0; i < list->object_count; i++)
        {
            returned_list.add(list->obj_list[i]);
        }
    }
}

// 0x00775170 (:289)
void restore_list_system_save(cfile* save_file)
{
    cf_write_uint32(MAX_RESTORABLE_OBJECTS, save_file);
    for (int i = 0; i < MAX_RESTORABLE_OBJECTS; i++)
    {
        cf_write_uint32(All_restorable_objects[i], save_file);
    }
    cf_write_uint32((unsigned int)All_restore_lists.size(), save_file);
    for (int i = 0; i < All_restore_lists.size(); i++)
    {
        All_restore_lists[i].system_save(save_file);
    }
}

// 0x0078FBB0 (:309)
void restore_list_system_load(cfile* load_file)
{
    if (cf_read_uint32(load_file) != (unsigned int)MAX_RESTORABLE_OBJECTS)
    {
        keen::debug::nativePrintArgs("The number of objects in the file is inconsistent with the number expected. This save file can't be loaded.");
        return;
    }
    for (int i = 0; i < MAX_RESTORABLE_OBJECTS; i++)
    {
        All_restorable_objects[i] = cf_read_uint32(load_file);
    }
    All_restore_lists.clear();
    const int count = (int)cf_read_uint32(load_file);
    for (int i = 0; i < count; i++)
    {
        restore_list list;
        list.system_load(load_file);
        All_restore_lists.add(list);
    }
}

// 0x0078FCE0 (:341)
void restore_list_create(unsigned int associated_object_handle, const farray<unsigned int,800>& new_list)
{
    if (new_list.size() > 0 && associated_object_handle != 0xFFFFFFFF && find_restore_list(associated_object_handle) == nullptr)
    {
        restore_list* existing = find_restore_list(new_list);
        if (existing != nullptr)
        {
            create_new_restore_list(associated_object_handle, existing);
            return;
        }
        create_new_restore_list(associated_object_handle, new_list);
    }
}

// 0x00775210 (:366)
void restore_list_do_restore(unsigned int associated_object_handle)
{
    restore_list* list = find_restore_list(associated_object_handle);
    if (list != nullptr)
    {
        list->restoring = true;
        for (int i = 0; i < list->object_count; i++)
        {
            World.restore_object(list->obj_list[i]);
        }
    }
}

// 0x00775270 (:394)
bool restore_list_restore_complete(unsigned int associated_object_handle)
{
    restore_list* list = find_restore_list(associated_object_handle);
    if (list != nullptr && list->restoring)
    {
        for (int i = 0; i < list->object_count; i++)
        {
            if (World.restore_object_is_pending(list->obj_list[i]) && World.restore_object_is_immediate(list->obj_list[i]))
            {
                return false;
            }
        }
        list->restoring = false;
    }
    return true;
}
