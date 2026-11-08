// Original: volition/rfg/code/objects/gameplay_properties.h
// Implementation: volition/rfg/code/objects/gameplay_properties.cpp
//
// Gameplay property sets (gameplay_properties table): per named entry the chunk flags, importance, team, destroyed
// percentages, salvage material and pieces, control and monetary value and the LOD properties; looked up by name by
// buildings and other destroyables.
#pragma once

#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/obj_enums.h"

struct LOD_properties;
class mempool_base;

// size 0x2C
struct gameplay_properties
{
    gameplay_properties(); // 0x00A2C6A0 (.h:33)
    void clear(); // 0x00A05DB0 (.h:39)

    char* name; // 0x00
    int chunk_flags; // 0x04
    object_importance_level importance; // 0x08
    human_teams team; // 0x0C
    float game_destroyed_pct; // 0x10
    float fully_destroyed_pct; // 0x14
    salvage_material_type salvage_material; // 0x18
    int salvage_num_pieces; // 0x1C
    float control; // 0x20
    float monetary_value; // 0x24
    const LOD_properties* lod_properties; // 0x28
};

void gpm_dlc_shutdown(); // 0x00A995C0 (:279)
void gpm_system_init(mempool_base* pool, int dlc_id); // 0x00A7AF80 (:288)
const gameplay_properties* gpm_get_properties(const char* property_entry_name); // 0x00A52A80 (:323)
