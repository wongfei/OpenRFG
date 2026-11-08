// Original: volition/rfg/code/code_effects/xray_effect.h
// Implementation: volition/rfg/code/code_effects/xray_effect.cpp
//
// The x-ray effect (rail driver scope and vision backpack): humans seen through walls drawn with the x-ray material,
// tinted by team. Placement of the enums inferred.
#pragma once

enum xray_effect_types
{
    XET_VISION_BACKPACK = 0,
    XET_RAIL_DRIVER = 1,
    NUM_XRAY_EFFECT_TYPES = 2,
};

enum xray_teams
{
    XRAY_TEAM_FRIENDLY = 0,
    XRAY_TEAM_ENEMY = 1,
    NUM_XRAY_TEAMS = 2,
};

extern unsigned int Xray_effect_mat_srid; // 0x01DEE4C8

void unused_dcf_xray_humans(); // 0x006EE890 (:72)
void xray_effect_system_init(); // 0x006EE910 (:120)
void xray_effect_level_shutdown(); // 0x006E4E00 (:181)
void xray_effect_start(xray_effect_types type); // 0x006E4E30 (:190)
void xray_effect_stop(xray_effect_types type); // 0x006E4E50 (:198)
bool xray_effect_is_enabled(); // 0x006E4E70 (:206)
unsigned char xray_effect_get_xray_material(xray_teams team); // 0x006E4E80 (:211)
