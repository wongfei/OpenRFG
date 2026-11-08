// Original: volition/rfg/code/code_effects/distortion_effect.cpp

#include "volition/rfg/code/code_effects/distortion_effect.h"

#include "volition/render_lib/code/rl_material_map.h"
#include "volition/rfg/code/geom/material.h"
#include "volition/rfg/code/gamewide/game_render.h"

unsigned int Distortion_effect_mat_srid; // 0x01DEE4CC
static unsigned int Distortion_effect_material_handle = 0xFFFFFFFF; // 0x01648E9C, initialized in .data

// 0x006EBBC0 (:28)
// The stealth backpack's distortion material (vfx_stealth_dist_matfx, first entry of its map) for game_render.
void distortion_effect_system_init()
{
    material* mat = material_find("vfx_stealth_dist_matfx", Distortion_effect_mat_srid);
    if (mat != nullptr)
    {
        Distortion_effect_material_handle = ((rl_material_map*)mat->m_material_map)->get_material(0);
        game_render_set_distortion_material(Distortion_effect_material_handle);
    }
}
