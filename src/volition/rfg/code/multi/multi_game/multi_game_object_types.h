// Original: volition/rfg/code/multi/multi_game/multi_game_object_types.h
//
// The networked object types and object_data_bundle<T>, the base of the per-class state bundles (which data sets of the
// object's state were sent, and whether the update is complete).
#pragma once

enum multi_game_object_types
{
    MULTI_OBJECT_TYPE_NONE = 0,
    MULTI_OBJECT_TYPE_FIRST = 1,
    MULTI_OBJECT_TYPE_PROJECTILE = 1,
    MULTI_OBJECT_TYPE_PLAYER = 2,
    MULTI_OBJECT_TYPE_ITEM = 3,
    MULTI_OBJECT_TYPE_VEHICLE = 4,
    MULTI_OBJECT_TYPE_NPC = 5,
    MULTI_OBJECT_TYPE_GEN_MOVER = 6,
    MULTI_OBJECT_TYPE_RFG_MOVER = 7,
    MULTI_OBJECT_TYPE_TRIGGER = 8,
    MULTI_OBJECT_TYPE_WEAPON_PICKUP = 9,
    MULTI_OBJECT_TYPE_TURRET = 10,
    MULTI_OBJECT_TYPE_WALKER = 11,
    MULTI_OBJECT_TYPE_MAX = 12,
};

// size (<unsigned short>) 0x4
template<typename T>
struct object_data_bundle
{
    T transmitted_data_set; // 0x00
    bool full_update; // 0x02
};
