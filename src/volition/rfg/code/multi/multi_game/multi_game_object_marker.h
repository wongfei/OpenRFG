// Original: volition/rfg/code/multi/multi_game/multi_game_object_marker.h
// Implementation: volition/rfg/code/multi/multi_game/multi_game_object_marker.cpp
//
// Multiplayer markers: level markers (backpack rack positions, spawn areas, ...) found by type.
#pragma once

#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/obj_enums.h"
#include "volition/rfg/code/objects/object.h"
#include "volition/rfg/code/objects/object_type.h"
#include "volition/vlib/code/math/bbox.h"
#include "volition/vlib/code/math/vector3.h"

enum multi_marker_types
{
    MMT_NONE = -1,
    MMT_SIEGE_TARGET = 0,
    MMT_BACKPACK_RACK = 1,
    MMT_SPAWN_NODE = 2,
    MMT_FLAG_CAPTURE_ZONE = 3,
    MMT_KING_OF_THE_HILL_TARGET = 4,
    MMT_SPECTATOR_CAMERA = 5,
    NUM_MULTI_MARKER_TYPES = 6,
};

// size 0xF0
class multi_object_marker : public object
{
public:
    multi_object_marker(); // 0x00959EF0 (:77)
    static const object_type get_type_static(); // 0x0094CB60 (.h:58)
    static const object_sub_type get_subtype_static(); // 0x0094CB70 (.h:58)
    static const char* get_classname_static(); // 0x0094CB80 (.h:58)
    static int get_bpo_index_static(); // 0x0094CB90 (.h:58)
    virtual const char* get_classname() const; // vtbl+0x0C 0x00959F10 (.h:58)
    static unsigned int get_classname_hash(); // 0x00959EE0 (:76)
    static int pool_remaining(); // 0x0097DB50 (:76)
    virtual void pool_free(); // vtbl+0x80 0x009B0BF0 (:76)
    virtual bool serialize(obj_prop_block& pblock, bool load); // vtbl+0x14 0x0099BA50 (:82)
    virtual bool stream(bool load); // vtbl+0x1C 0x0097DB80 (:185)
    virtual void get_bbox(vector& bmin, vector& bmax) const; // vtbl+0x04 0x00959F30 (:217)
    virtual void get_local_bbox(vector& bmin, vector& bmax) const; // vtbl+0x08 0x0095A1B0 (:230)
    virtual void hierarchy_notify(hierarchy_notify_code code); // vtbl+0x70 0x0095A1F0 (:238)
    multi_marker_types get_marker_type() const; // 0x0094CBA0 (.h:80)
    bool is_random_rack() const; // 0x0094CBB0 (.h:88)
    bool can_attach_packs() const; // 0x0094CBC0 (.h:89)
    virtual ~multi_object_marker(); // vtbl+0x00 0x00959F20

    human_teams team; // 0x9C
    bool destroyed; // 0xA0
    int priority; // 0xA4
    int backpack_rack_group; // 0xA8
    int backpack_id; // 0xAC
    int num_backpacks; // 0xB0
    unsigned int backpacks[3]; // 0xB4
    unsigned int backpack_rack_light; // 0xC0
private:
    bbox local_bbox; // 0xC4
    vector offset; // 0xDC
    multi_marker_types marker_type; // 0xE8
    bool random_rack; // 0xEC
    bool can_attach_packs_normally; // 0xED
};

// The level's markers by priority (multi_game_marker_level_init_post).
extern multi_object_marker* Multi_markers_prioritized[128]; // 0x02FE7188

void types_multi_object_marker(signed char& type, signed char& subtype); // 0x00959EB0 (:76)
int bpo_index_multi_object_marker(); // 0x00959EC0 (:76)
const char* classname_multi_object_marker(); // 0x00959ED0 (:76)
object* alloc_multi_object_marker(); // 0x0099BA40 (:76)
void multi_game_marker_level_init_post(); // 0x0099BEE0 (:260)
multi_marker_types multi_game_marker_get_type_by_name(const char* name); // 0x0095A220 (:275)
const char* multi_game_marker_get_name_by_type(multi_marker_types type); // 0x0095A280 (:289)
multi_object_marker* multi_game_marker_find_closest(multi_marker_types type, const vector& pos); // 0x0099BF50 (:298)
multi_object_marker* multi_game_marker_get_prev(multi_object_marker* marker); // 0x0099BFD0 (:317)
multi_object_marker* multi_game_marker_get_next(multi_object_marker* marker); // 0x0099C040 (:349)
int multi_game_marker_get_num(multi_marker_types type); // 0x0099C0B0 (:379)
