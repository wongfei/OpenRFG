// Original: volition/rfg/code/multi/multi_game/multi_game_object_flag.h
// Implementation: volition/rfg/code/multi/multi_game/multi_game_object_flag.cpp
//
// Flags (Capture the Flag and Bagman): the flag object, carried by a player, dropped and reset.
#pragma once

#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/object_item.h"
#include "volition/rfg/code/objects/object_type.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/time/timestamp.h"

class obj_prop_block;
class object;
class player;

enum multi_object_flag_states
{
    MOFS_NONE = -1,
    MOFS_AT_BASE = 0,
    MOFS_PICKED_UP = 1,
    MOFS_DROPPED = 2,
    NUM_MULTI_OBJECT_FLAG_STATES = 3,
};

// size 0xC
class obj_flag_props : public obj_item_props
{
public:
    obj_flag_props(item_info* infop, const vector& pos, const matrix& orient, obj_item_flags& flags, human_teams team); // 0x0097D4B0 (:38)
    ~obj_flag_props(); // 0x0097D6B0

};

// size 0x188
class multi_object_flag : public item
{
    // human_teams_get_team reads the team directly (friends are not recorded in rfg.pdb)
    friend human_teams human_teams_get_team(const object* obj);
public:
    multi_object_flag(); // 0x00959120 (:48)
    static const object_type get_type_static(); // 0x0094BD90 (.h:24)
    static const object_sub_type get_subtype_static(); // 0x0094BDA0 (.h:24)
    static const char* get_classname_static(); // 0x0094BDB0 (.h:24)
    static int get_bpo_index_static(); // 0x0094BDC0 (.h:24)
    virtual const char* get_classname() const; // vtbl+0x0C 0x00959150 (.h:24)
    static unsigned int get_classname_hash(); // 0x00959110 (:47)
    static int pool_remaining(); // 0x0097D520 (:47)
    virtual void pool_free(); // vtbl+0x80 0x009B0890 (:47)
    virtual bool serialize(obj_prop_block& pblock, bool load); // vtbl+0x14 0x00959170 (:53)
    virtual bool stream(bool load); // vtbl+0x1C 0x00959240 (:79)
    virtual void free_this(); // vtbl+0x28 0x0097D550 (:128)
    void event_picked_up(player* carrier_player); // 0x0097D5E0 (:213)
    void event_dropped(); // 0x0097D610 (:224)
    void event_returned();
    void process(); // 0x009B08A0 (:152)
    void reset(); // 0x0097D590 (:195)
    player* get_carrier() const; // 0x0094BDD0 (.h:53)
    human_teams get_team() const; // 0x0094BDE0 (.h:56)
    void get_base_position(vector&) const;
private:
    void attach_to_player(player* pp); // 0x009593D0 (:241)
    void detach_from_player(player* pp); // 0x009594A0 (:265)
public:
    virtual ~multi_object_flag(); // vtbl+0x00 0x00959160

    multi_object_flag_states flag_state; // 0x140
    timestamp dropped_time; // 0x144
private:
    player* carrier; // 0x148
    human_teams team; // 0x14C
    vector base_position; // 0x150
    matrix base_orient; // 0x15C
    unsigned int effect; // 0x180
    int effect_tag_index; // 0x184
};

void types_multi_object_flag(signed char& type, signed char& subtype); // 0x009590E0 (:47)
int bpo_index_multi_object_flag(); // 0x009590F0 (:47)
object* alloc_multi_object_flag(); // 0x0099ADA0 (:47)
const char* classname_multi_object_flag(); // 0x00959100 (:47)
multi_object_flag* multi_object_flag_create(const vector& pos, human_teams team); // 0x0099ADB0 (:279)
void multi_object_flag_process(); // 0x009B0A90 (:306)
multi_object_flag* player_get_flag(const player* pp); // 0x0099AE70 (:318)
bool player_has_flag(const player* pp); // 0x0099AEC0 (:340)
multi_object_flag* multi_object_flag_get_flag_by_team(human_teams team); // 0x0099AEE0 (:349)
void multi_object_flag_reset(); // 0x0099AF30 (:363)
