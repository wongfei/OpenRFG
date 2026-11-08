// Original: volition/rfg/code/geom/character.h
// Implementation: volition/rfg/code/geom/character.cpp
//
// A loaded character: the pairing of a character mesh with its anim_lib rig and the bones the mesh uses, found by
// name and stream container (character_find); loaded with the character mesh, unloaded with it.
#pragma once

#include "volition/anim_lib/code/anim_lib_types.h"

struct character_mesh;

// size 0x60
class character
{
public:
    void init(); // 0x007F1EC0 (:77)

    char name[65]; // 0x00
    animlib_bones_used_info bones_used_info; // 0x44
    anim_rig* rig; // 0x50
    character_mesh* m_data; // 0x54
    unsigned int morph_name_hash; // 0x58
    unsigned int srid; // 0x5C
};

void character_system_init(int string_pool_size); // 0x00801CE0 (:114)
void character_system_shutdown(); // 0x007FB6F0 (:139)
void character_set_up_trigger_ids(); // 0x007F1ED0 (:156)
bool character_load(const char* name, unsigned int srid, character_mesh* cmesh, anim_rig* rig, const char* mesh_name); // 0x00801D10 (:286)
void character_unload(character* cp); // 0x0080A800 (:333)
character* character_find(const char* name, unsigned int srid); // 0x00801E20 (:348)
void character_reload(); // 0x00801F10 (:368)

// The anim_lib trigger ids of the game's triggers, set by character_set_up_trigger_ids (placement of the declarations
// inferred; the definitions are in character.cpp).
extern int Trigger_combo_start; // 0x025BC114
extern int Trigger_no_interrupt; // 0x025E9898
extern int Trigger_left_foot; // 0x025BC110
extern int Trigger_right_foot; // 0x025BD39C
extern int Trigger_left_foot_release; // 0x025EF8A4
extern int Trigger_right_foot_release; // 0x025BD3A0
extern int Trigger_critical_end; // 0x025A31B0
extern int Trigger_combo_window_start; // 0x025BCF1C
extern int Trigger_combo_window_end; // 0x025BD38C
extern int Trigger_impact_start; // 0x025BD390
extern int Trigger_unlock_ai; // 0x0253D4C4
extern int Trigger_secondary_impact; // 0x025BD3AC
extern int Trigger_impact_end; // 0x025E989C
extern int Trigger_release_movement; // 0x025BD3A8
extern int Trigger_warning_shot; // 0x025737F4
extern int Trigger_ragdoll; // 0x025BD3A4
extern int Trigger_ragdoll_stop; // 0x0253D4C0
extern int Trigger_tag_start; // 0x025737F0
extern int Trigger_tag_end; // 0x025BD3B8
extern int Trigger_enable_spinebend; // 0x025BD3B0
extern int Trigger_camera; // 0x025BD398
extern int Trigger_relative_space; // 0x025BCF24
extern int Trigger_reload_give_ammo; // 0x025A31B4
extern int Trigger_door_open_start; // 0x025E8094
extern int Trigger_door_open_end; // 0x025BD3BC
extern int Trigger_door_close_start; // 0x025694CC
extern int Trigger_door_close_end; // 0x025E8090
extern int Trigger_gravity_start; // 0x025BCF20
extern int Num_triggers; // 0x025BD3B4, 28 once set up
