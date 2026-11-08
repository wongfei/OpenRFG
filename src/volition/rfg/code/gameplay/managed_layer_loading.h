// Original: volition/rfg/code/gameplay/managed_layer_loading.h
// Implementation: volition/rfg/code/gameplay/managed_layer_loading.cpp
//
// Managed layers: groups of zone layer files (.layer_pc) that missions load and unload as one streamed container
// (GSC_LAYER_SMALL or GSC_LAYER_LARGE, slot allocators GSA_SMALL_LAYER / GSA_LARGE_LAYER). managed_layer_set::load
// reads the layer file names from an xml element (<layer_file_0> ... <layer_file_15>); start_layer_loading requests
// the container when a slot is free (3 small slots, 1 large slot; decompiled from start_layer_loading/get_slot_count);
// serialize_layer_objects records the created objects in All_manager_layer_objects. Sets come from
// Managed_layer_set_pool (512).
#pragma once

#include "volition/vlib/code/memory/object_pool.h"
#include "volition/vlib/code/util/string_pool.h"
#include "volition/vlib/code/util/varray.h"

struct xml_element;

// size 0x8
class managed_layer_info
{
public:
    managed_layer_info(); // 0x007692C0 (.h:43)
    void reset(); // 0x0075E650 (:64)
    bool member_of_layer(unsigned int handle) const; // 0x0075E660 (:69)

    const char* file_name; // 0x00
    int layer_grid_id; // 0x04
};

// size 0x98
class managed_layer_set
{
public:
    managed_layer_set(); // 0x0077EDC0 (.h:65)
    bool load(const char* layer_set_name, xml_element* xml_root, bool large_slot_value); // 0x0078D730 (:82)
    bool start_layer_loading(); // 0x0079D550 (:140)
    void start_layer_unloading(); // 0x0078D8D0 (:159)
    bool layer_loading_started() const; // 0x00774800 (:212)
    bool layer_resources_loaded() const; // 0x0075E680 (:267)
    bool layer_resources_serialized() const; // 0x00774880 (:252)
    bool serialize_layer_objects(); // 0x0078D990 (:302)
    bool member_of_layer(unsigned int handle) const; // 0x00774910 (:395)
    void level_init(); // 0x0078DB00 (:344)
    bool using_slot() const; // 0x0075E6E0 (:351)
    bool resource_slot_free() const; // 0x0078DB10 (:358)
    bool is_dlc(); // 0x007748E0 (:373)
    bool large_slot_type() const; // 0x00754DB0 (.h:110)
private:
    void reset(); // 0x007692D0 (.h:134)
    int get_slot_cap_count() const; // 0x0075E700 (:384)

    farray<managed_layer_info,16> managed_layers; // 0x00
    unsigned int srid; // 0x90
    struct
    {
        bool large_slot : 1; // 0x00:0
        bool layer_serialized : 1; // 0x00:1
    } layer_flags; // 0x94
};

extern farray<unsigned int,512> All_manager_layer_objects; // 0x01657838
extern string_pool Managed_layer_name_string_pool; // 0x02231C00
extern object_pool<managed_layer_set,512,1> Managed_layer_set_pool; // 0x022522F0

void managed_layer_system_init(); // 0x007A57A0 (:432)
void managed_layer_level_init(); // 0x00774960 (:446)
void managed_layer_level_shutdown(); // 0x0078DB60 (:460)
managed_layer_set* managed_layer_load(const char* layer_set_name, xml_element* xml_root, bool large_slot); // 0x0079D5D0 (:473)
const farray<unsigned int,512>& manager_layer_all_objects(); // 0x0075E720 (:493)
