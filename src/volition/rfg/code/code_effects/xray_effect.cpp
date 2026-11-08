// Original: volition/rfg/code/code_effects/xray_effect.cpp
//
// Everything but the console command unused_dcf_xray_humans (stub_impl.md S3). Two x-ray effects can be on at once;
// the rail driver's (index 1) wins over the vision backpack's (index 0).

#include "volition/rfg/code/code_effects/xray_effect.h"

#include "volition/render_lib/code/rl_material_map.h"
#include "volition/render_lib/code/rl_xray_material_manager.h"
#include "volition/rfg/code/geom/material.h"
#include "volition/rfg/code/gamewide/game_render.h"

// size 0x14
struct xray_effect_status
{
    bool enabled; // 0x00
    bool use_terrain_to_occlude; // 0x01
    unsigned int material_handle[NUM_XRAY_TEAMS]; // 0x04
    float saturation; // 0x0C, the scene saturation while the effect is on
    unsigned char xray_material[NUM_XRAY_TEAMS]; // 0x10, rl_xray_material_manager index, 0xFF none
};

unsigned int Xray_effect_mat_srid; // 0x01DEE4C8
static xray_effect_status Xray_effect_statuses[NUM_XRAY_EFFECT_TYPES]; // 0x01DEE3F8
static const xray_effect_status* Current_xray_status; // 0x01DEE4D0

// 0x006E4D90 (:96)
static void process_xray_effect_changes()
{
    Current_xray_status = nullptr;
    for (int i = NUM_XRAY_EFFECT_TYPES - 1; i >= 0; i--)
    {
        if (Xray_effect_statuses[i].enabled)
        {
            Current_xray_status = &Xray_effect_statuses[i];
            game_render_set_xray_effect_enabled(true, Xray_effect_statuses[i].saturation, Xray_effect_statuses[i].use_terrain_to_occlude);
            return;
        }
    }
    game_render_set_xray_effect_enabled(false, 1.0f, true);
}

// 0x006EE910 (:120)
// The materials of the "xray_effect" material file: the rail driver's friendly and enemy materials, the vision
// backpack's one material for both teams.
void xray_effect_system_init()
{
    for (int i = 0; i < NUM_XRAY_EFFECT_TYPES; i++)
    {
        Xray_effect_statuses[i].enabled = false;
        Xray_effect_statuses[i].use_terrain_to_occlude = false;
        Xray_effect_statuses[i].material_handle[XRAY_TEAM_ENEMY] = 0xFFFFFFFF;
        Xray_effect_statuses[i].material_handle[XRAY_TEAM_FRIENDLY] = 0xFFFFFFFF;
        Xray_effect_statuses[i].saturation = 0.0f;
        Xray_effect_statuses[i].xray_material[XRAY_TEAM_FRIENDLY] = 0xFF;
        Xray_effect_statuses[i].xray_material[XRAY_TEAM_ENEMY] = 0xFF;
    }
    material* mat = material_find("xray_effect", Xray_effect_mat_srid);
    xray_effect_status& rail = Xray_effect_statuses[XET_RAIL_DRIVER];
    if (mat != nullptr)
    {
        rl_material_map* map = (rl_material_map*)mat->m_material_map;
        int index = map->find_material_index("xray_mpfriendly");
        if (index != -1)
        {
            rail.material_handle[XRAY_TEAM_FRIENDLY] = map->get_material(index);
            rail.xray_material[XRAY_TEAM_FRIENDLY] = rl_xray_material_manager_register_material(rail.material_handle[XRAY_TEAM_FRIENDLY]);
        }
        index = map->find_material_index("xray_enemy");
        if (index != -1)
        {
            rail.material_handle[XRAY_TEAM_ENEMY] = map->get_material(index);
            rail.xray_material[XRAY_TEAM_ENEMY] = rl_xray_material_manager_register_material(rail.material_handle[XRAY_TEAM_ENEMY]);
        }
    }
    rail.saturation = 0.3f;
    rail.use_terrain_to_occlude = true;
    xray_effect_status& vision = Xray_effect_statuses[XET_VISION_BACKPACK];
    if (mat != nullptr)
    {
        rl_material_map* map = (rl_material_map*)mat->m_material_map;
        const int index = map->find_material_index("xray_vision");
        if (index != -1)
        {
            // registered once per team
            vision.material_handle[XRAY_TEAM_FRIENDLY] = map->get_material(index);
            vision.xray_material[XRAY_TEAM_FRIENDLY] = rl_xray_material_manager_register_material(vision.material_handle[XRAY_TEAM_FRIENDLY]);
            vision.material_handle[XRAY_TEAM_ENEMY] = map->get_material(index);
            vision.xray_material[XRAY_TEAM_ENEMY] = rl_xray_material_manager_register_material(vision.material_handle[XRAY_TEAM_ENEMY]);
        }
    }
    vision.use_terrain_to_occlude = false;
    vision.saturation = 1.5f;
}

// 0x006E4E00 (:181)
void xray_effect_level_shutdown()
{
    Xray_effect_statuses[XET_VISION_BACKPACK].enabled = false;
    Xray_effect_statuses[XET_RAIL_DRIVER].enabled = false;
    Current_xray_status = nullptr;
    game_render_set_xray_effect_enabled(false, 1.0f, true);
}

// 0x006E4E30 (:190)
void xray_effect_start(xray_effect_types type)
{
    if (type < NUM_XRAY_EFFECT_TYPES)
    {
        Xray_effect_statuses[type].enabled = true;
        process_xray_effect_changes();
    }
}

// 0x006E4E50 (:198)
void xray_effect_stop(xray_effect_types type)
{
    if (type < NUM_XRAY_EFFECT_TYPES)
    {
        Xray_effect_statuses[type].enabled = false;
        process_xray_effect_changes();
    }
}

// 0x006E4E70 (:206)
bool xray_effect_is_enabled()
{
    return Current_xray_status != nullptr;
}

// 0x006E4E80 (:211)
// The x-ray material of the active effect for the team (0xFF when no effect is on).
unsigned char xray_effect_get_xray_material(xray_teams team)
{
    for (int i = NUM_XRAY_EFFECT_TYPES - 1; i >= 0; i--)
    {
        if (Xray_effect_statuses[i].enabled)
        {
            return Xray_effect_statuses[i].xray_material[team];
        }
    }
    return 0xFF;
}
