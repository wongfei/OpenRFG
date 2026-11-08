// Original: volition/rfg/code/objects/movers/object_mover_lists.h
// Implementation: volition/rfg/code/objects/movers/object_mover_lists.cpp
//
// Handle wrappers kept in pooled lists of movers (Object_mover_list_set, a pool of 128 with 7 categories): one list
// per mover_flag_types value, filled by object_mover::serialize from the mover type flags (decompiled).
#pragma once

#include "volition/rfg/code/objects/obj_enums.h"
#include "volition/vlib/code/memory/object_pool.h"

class object_mover;
class vector;

// size 0x4
class obj_handle_wrapper
{
public:
    obj_handle_wrapper(); // 0x00A33CA0 (.h:30)
    void reset(); // 0x00A100E0 (.h:34)

    unsigned int handle; // 0x00
};

extern object_pool<obj_handle_wrapper,128,7> Object_mover_list_set; // 0x03043590

void oml_level_init_pre(); // 0x00AA2BA0 (:29)
void oml_add_new_object(unsigned int handle, mover_flag_types flag_type); // 0x00A5FB20 (:37)
void oml_remove_object(unsigned int handle, mover_flag_types flag_type); // 0x00A8C160 (:52)
object_mover* oml_find_nearest_mover(const vector& pos, mover_flag_types flag_type); // 0x00A5FB40 (:69)
