// Original: volition/rfg/code/objects/obj_enums.h
//
// Object-wide enums: serialization mode, hierarchy notifications, hit locations, importance and spawn priority, kinds
// of movers and salvage materials (damaged_by_types is in level_object_properties_shared.h). Placement inferred (the PDB records no inline code for obj_enums.h).
#pragma once

enum obj_serialize_mode
{
    OSM_NONE = 0,
    OSM_RESTORE = 1,
    OSM_FULL = 2,
};

enum hierarchy_notify_code
{
    HN_CHILDREN_STREAMED = 0,
    HN_PARENT_DESTROYED = 1,
};

enum object_hit_location
{
    HIT_NONE = -1,
    HIT_GENERAL = 0,
    HIT_TORSO = 1,
    HIT_PELVIS = 2,
    HIT_LEFT_ARM = 3,
    HIT_RIGHT_ARM = 4,
    HIT_HEAD = 5,
    HIT_LEFT_LEG = 6,
    HIT_RIGHT_LEG = 7,
    NUM_HIT_LOCATIONS = 8,
};

enum object_importance_level
{
    OBJECT_IMPORTANCE_LEVEL_UNKNOWN = -1,
    OBJECT_IMPORTANCE_LEVEL_NONE = 0,
    OBJECT_IMPORTANCE_LEVEL_LOW = 1,
    OBJECT_IMPORTANCE_LEVEL_MEDIUM = 2,
    OBJECT_IMPORTANCE_LEVEL_HIGH = 3,
    NUM_OBJECT_IMPORTANCE_LEVELS = 4,
};

enum object_spawn_priority
{
    SPAWN_PRIORITY_INVALID = -1,
    SPAWN_PRIORITY_LOW = 0,
    SPAWN_PRIORITY_MEDIUM = 1,
    SPAWN_PRIORITY_HIGH = 2,
    SPAWN_PRIORITY_SPECIAL = 3,
    NUM_SPAWN_PRIORITIES = 4,
};

enum mover_flag_types
{
    MOVER_TYPE_FLAG_COMMUNICATIONS = 0,
    MOVER_TYPE_FLAG_PLAYER_BASE = 1,
    MOVER_TYPE_FLAG_HOUSE = 2,
    MOVER_TYPE_RAID = 3,
    MOVER_TYPE_BRIDGE = 4,
    MOVER_TYPE_FORCE_FIELD = 5,
    MOVER_TYPE_DYNAMIC = 6,
    NUM_MOVER_FLAG_TYPES = 7,
};

enum salvage_material_type
{
    INVALID_SALVAGE_MATERIAL = -1,
    SALVAGE_MATERIAL_METAL = 0,
    SALVAGE_MATERIAL_ORE = 1,
    SALVAGE_MATERIAL_CHEMICAL = 2,
    NUM_SALVAGE_MATERIAL_TYPES = 3,
};
