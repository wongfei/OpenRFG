// Original: volition/rfg/code/objects/object_player_start.h
// Implementation: volition/rfg/code/objects/object_player_start.cpp
//
// Player starts (OT_PLAYER_START): spawn positions of the player or of multiplayer teams, flagged for the initial
// spawn, respawns, checkpoints and activities, optionally tied to a mission.
#pragma once

#include "volition/rfg/code/gameplay/mission_info_shared.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/object.h"
#include "volition/rfg/code/objects/object_type.h"

class vector;

// size 0xB0
class player_start : public object
{
public:
    player_start(); // 0x00A1ADF0 (:29)
    static const object_type get_type_static(); // 0x00755000 (.h:29)
    static const object_sub_type get_subtype_static(); // 0x00755010 (.h:29)
    static const char* get_classname_static(); // 0x00A100F0 (.h:29)
    static int get_bpo_index_static(); // 0x0091C090 (.h:29)
    virtual const char* get_classname() const; // vtbl+0x0C 0x00A1AE10 (.h:29)
    static unsigned int get_classname_hash(); // 0x00A1ADE0 (:28)
    static int pool_remaining(); // 0x00A3D5C0 (:28)
    virtual void pool_free(); // vtbl+0x80 0x00A92A40 (:28)
    virtual bool serialize(obj_prop_block& pblock, bool load); // vtbl+0x14 0x00AAAA80 (:41)
    virtual bool stream(bool load); // vtbl+0x1C 0x00A1AE30 (:115)
    virtual void get_bbox(vector& bmin, vector& bmax) const; // vtbl+0x04 0x00A1AE50 (:126)
    virtual void get_local_bbox(vector& bmin, vector& bmax) const; // vtbl+0x08 0x00A1AF20 (:134)
    virtual ~player_start(); // vtbl+0x00 0x00A1AE20

    bool indoor; // 0x9C
    human_teams team; // 0xA0
    bool available_for_initial_spawn; // 0xA4
    bool available_for_respawn; // 0xA5
    bool checkpoint_respawn; // 0xA6
    bool activity_respawn; // 0xA7
    bool enabled; // 0xA8
    mission_handle m_handle; // 0xAC
};

void types_player_start(signed char& type, signed char& subtype); // 0x00A1ADB0 (:28)
int bpo_index_player_start(); // 0x00A1ADC0 (:28)
object* alloc_player_start(); // 0x00A6A010 (:28)
const char* classname_player_start(); // 0x00A1ADD0 (:28)
