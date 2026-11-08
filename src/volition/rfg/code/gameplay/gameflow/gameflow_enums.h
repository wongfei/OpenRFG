// Original: volition/rfg/code/gameplay/gameflow/gameflow_enums.h
//
// Seam only (M3): the game flow alert levels and the squad orders types, held by spawn nodes and squads. Placement
// inferred. The game flow manager comes with the gameplay tier (M5).
#pragma once

enum gfm_alert_levels
{
    ALERT_LEVEL_INVALID = -1,
    ALERT_LEVEL_GREEN = 0,
    ALERT_LEVEL_YELLOW = 1,
    ALERT_LEVEL_ORANGE = 2,
    ALERT_LEVEL_RED = 3,
    NUM_ALERT_LEVELS = 4,
};

enum gfm_orders_types
{
    GFM_ORDERS_TYPE_INVALID = -1,
    GFM_ORDERS_TYPE_NONE = 0,
    GFM_ORDERS_TYPE_DEFEND = 1,
    GFM_ORDERS_TYPE_ATTACK = 2,
    GFM_ORDERS_TYPE_PATROL = 3,
    GFM_ORDERS_TYPE_SMASH = 4,
    GFM_ORDERS_TYPE_CONVOY = 5,
    GFM_ORDERS_TYPE_COURIER = 6,
    GFM_ORDERS_TYPE_AMBIENT_ACTION = 7,
    GFM_ORDERS_TYPE_AMBIENT_PED = 8,
    GFM_ORDERS_TYPE_FODDER = 9,
    GFM_ORDERS_TYPE_TERRORIZE = 10,
    GFM_ORDERS_TYPE_WAIT = 11,
    GFM_ORDERS_TYPE_ROADBLOCK = 12,
    GFM_ORDERS_TYPE_TRAPPED = 13,
    GFM_ORDERS_TYPE_TRAPPED_JAIL = 14,
    GFM_ORDERS_TYPE_DRIVE_BY = 15,
    GFM_ORDERS_TYPE_FLY_BY = 16,
    GFM_ORDERS_VEHICLE_DROP_OFF = 17,
    GFM_ORDERS_TIED_UP = 18,
    NUM_GFM_ORDERS_TYPES = 19,
};
