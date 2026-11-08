// Original: volition/rfg/code/spawn/ambient_prop_info.h
// Implementation: volition/rfg/code/spawn/ambient_prop_info.cpp
//
// Ambient props: the hand-held objects ambient NPCs carry (bags, tools, drinks), read from the ambient prop table with
// their attach points and effects. amb_prop_create / amb_prop_mount spawn a prop on a human, amb_prop_do_frame tracks
// the effects of the props in use.
#pragma once

#include "volition/rfg/code/geom/anim_enums.h"
#include "volition/vlib/code/util/varray.h"

class human;
class item;
struct item_info;
class matrix;
class vector;
struct xml_element;

// size 0x1C
class ambient_prop_effect_info
{
public:
    ambient_prop_effect_info(); // 0x00B46C60 (:19)
    bool load(xml_element* xml_root); // 0x00B40AA0 (:369)
    void reset(); // 0x00B40A80 (:358)

    animation_state anim_state; // 0x00
    animation_action anim_action; // 0x04
    unsigned int effect_handle; // 0x08
    const char* attach_point_name; // 0x0C
    int start_tag_id; // 0x10
    int end_tag_id; // 0x14
    struct
    {
        unsigned int loop_effect : 1; // 0x00:0
        unsigned int attach_effct : 1; // 0x00:1
    } effect_flags; // 0x18
};

// size 0x10
class prop_effect_tracking_info
{
public:
    prop_effect_tracking_info(); // 0x00B46C80 (:51)
    void reset(); // 0x00B40C40 (:432)

    unsigned int human_handle; // 0x00
    unsigned int item_handle; // 0x04
    unsigned int effect_handle; // 0x08
    const ambient_prop_effect_info* effect_info; // 0x0C
};

// size 0x44
class ambient_prop_info
{
public:
    ambient_prop_info(); // 0x00B4B9B0 (.h:38)
    bool is_streamed() const; // 0x00B40C60 (:460)
    void reset(); // 0x00B46D70 (:444)
    bool load(xml_element* xml_root); // 0x00B547E0 (:474)

    item_info* prop_item_info; // 0x00
    const char* human_attach_point_name; // 0x04
    const char* child_attach_point_name; // 0x08
    int prop_walk_state; // 0x0C
    int prop_stand_state; // 0x10
    animation_action prop_exit_anim; // 0x14
    int exit_anim_trigger_id; // 0x18
    unsigned int perm_effect_handle; // 0x1C
    const char* perm_effect_attach_name; // 0x20
    farray<ambient_prop_effect_info const*,3> effect_info_list; // 0x24
    struct
    {
        unsigned int delete_on_exit : 1; // 0x00:0
    } prop_flags; // 0x40
};

void create_prop_effect(item* ip, const ambient_prop_info* amb_prop_info); // 0x00B40C90 (:586)
void amb_prop_system_init(); // 0x00B59470 (:604)
void amb_prop_level_init_post(); // 0x00B46DA0 (:632)
item* amb_prop_create(const ambient_prop_info* amb_prop_info, const vector& pos, const matrix& orient, unsigned int parent_handle); // 0x00B46DB0 (:640)
bool amb_prop_create(const ambient_prop_info* amb_prop_info, human* hp); // 0x00B4BB90 (:668)
bool amb_prop_mount(human* hp, const ambient_prop_info* amb_prop_info, item* ip); // 0x00B40CE0 (:742)
const ambient_prop_info* amb_prop_info_find(const char* prop_name); // 0x00B4BCF0 (:782)
item* amb_prop_human_find(const human* hp); // 0x00B46E60 (:797)
void amb_prop_detach_props(human* hp, bool delete_prop); // 0x00B46EA0 (:828)
void amb_prop_do_frame(); // 0x00B4BD70 (:860)
