// Original: volition/rfg/code/gameplay/restore_list.h
// Implementation: volition/rfg/code/gameplay/restore_list.cpp
//
// Restore lists: per associated object, the handles of the objects to bring back with world::restore_object when it is
// restored (All_restore_lists), saved with the game.
#pragma once

#include "volition/vlib/code/util/varray.h"

struct cfile;

// size 0x10
class restore_list
{
public:
    restore_list(); // 0x007750F0 (:26)
    void reset(); // 0x0075F250 (:207)
    void system_save(cfile* save_file); // 0x0075F270 (:216)
    void system_load(cfile* load_file); // 0x0075F2B0 (:229)

    unsigned int associated_object_handle; // 0x00
    int object_count; // 0x04
    unsigned int* obj_list; // 0x08
    bool restoring; // 0x0C
};

bool lists_are_the_same(const restore_list& current_list, const farray<unsigned int,800>& new_list); // 0x0078F8B0 (:76)
restore_list* find_restore_list(const farray<unsigned int,800>& new_list); // 0x0078F910 (:95)
restore_list* find_restore_list(unsigned int associated_object_handle); // 0x00775110 (:111)
void create_new_restore_list(unsigned int associated_object_handle, restore_list* existing_list); // 0x0078F9A0 (:129)
void create_new_restore_list(unsigned int associated_object_handle, const farray<unsigned int,800>& new_list); // 0x0078FA40 (:145)
void restore_list_level_init(); // 0x00775150 (:258)
void restore_list_get_list(unsigned int associated_object_handle, farray<unsigned int,800>& returned_list); // 0x0078FB20 (:271)
void restore_list_system_save(cfile* save_file); // 0x00775170 (:289)
void restore_list_system_load(cfile* load_file); // 0x0078FBB0 (:309)
void restore_list_create(unsigned int associated_object_handle, const farray<unsigned int,800>& new_list); // 0x0078FCE0 (:341)
void restore_list_do_restore(unsigned int associated_object_handle); // 0x00775210 (:366)
bool restore_list_restore_complete(unsigned int associated_object_handle); // 0x00775270 (:394)
