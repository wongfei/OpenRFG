// Original: volition/rfg/code/code_effects/dust.cpp
//
// Ambient wind dust (VFX.md FX8), driven by weather_update: a looping ambient dust effect kept at the camera (moved
// upwind by the wind speed), a skirting dust effect whose particles become terrain decals on the render side, and
// gusts: random effects from the gust list of the terrain's effect material near or far ahead of the player, paced by
// Dust_gust_timer and the parametric particle load. The wind's line of sight (the listener's open sky) scales the
// ambient dust (the effects' WIND_LOS variable) and fades it near rfg movers. weather.xtbl's dust settings name the
// effects and skirting decals. Arithmetic follows rfg.exe (float expressions in double, rounded at stores).
// Notes (decompiled behaviour, kept as is): Dust_skirting_enabled is never set in rfg.exe, so the skirting effect and
// its decals never run.

#include "volition/rfg/code/code_effects/dust.h"

#include "volition/render_lib/code/rl_decal.h"
#include "volition/render_lib/code/rl_decal_parameters.h"
#include "volition/render_lib/code/rl_renderable_particles_instance.h"
#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/rfg/code/destruction/rfg_materials.h"
#include "volition/rfg/code/destruction/rfg_mover.h"
#include "volition/rfg/code/effects/effects.h"
#include "volition/rfg/code/effects/effects_shared.h"
#include "volition/rfg/code/effects/object_decal.h"
#include "volition/rfg/code/effects/object_effect.h"
#include "volition/rfg/code/game_render/game_render_task.h"
#include "volition/rfg/code/gamewide/game_render.h"
#include "volition/rfg/code/gamewide/gameaudio.h"
#include "volition/rfg/code/geom/decal_info.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/level/world.inl"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/object_zone.h"
#include "volition/rfg/code/rfg.h"
#include "volition/rfg/code/weather/wind.h"
#include "volition/vfxlib/code/vfx.h"
#include "volition/vfxlib/code/vfx_effect.h"
#include "volition/vfxlib/code/vfx_particle.h"
#include "volition/vfxlib/code/vfx_particle_system_emitter_instance.h"
#include "volition/vfxlib/code/vfx_particle_system_filter_instance.h"
#include "volition/vfxlib/code/vfx_particle_system_manager.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/math/vector2.h"
#include "volition/vlib/code/memory/virtual_mempool.h"
#include "volition/vlib/code/os/frametime.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/vlib/code/time/timestamp.h"

#include <math.h>

struct xml_element;

// ---- file-static globals
// Dust_skirting_decal_handles (0x01DEE428) ends at 0x01DEE478, which Ghidra names Glare_used_list (glare.cpp).
static bool Dust_ambient_enabled = true; // 0x01648D29
static bool Dust_gust_enabled = true; // 0x01648D2A
static const char* DUST_EFFECTS_LIST_TAG = "Dust_effects"; // 0x01648EA0
static const char* DUST_EFFECTS_ELEMENT = "Element"; // 0x01648EA4
static const char* DUST_AMBIENT_MAX_OFFSET_TAG = "Dust_max_offset"; // 0x01648EA8
static const char* DUST_SKIRTING_MAX_OFFSET_TAG = "Dust_skirting_max_offset"; // 0x01648EAC
static const char* DUST_SKIRTING_DECAL_TAG = "Dust_skirting_decal"; // 0x01648EB0
static const char* DUST_SKIRTING_DECAL_ELEMENT = "Element"; // 0x01648EB4
static const char* DUST_SKIRTING_EFFECT_TAG = "Dust_skirting_effect"; // 0x01648EB8
static unsigned int Dust_ambient_effect_info_handle = 0xFFFFFFFF; // 0x01648EBC
static float Dust_ambient_percent_offset = 0.25f; // 0x01648EC0
static float Dust_skirting_percent_offset = 0.25f; // 0x01648EC4
static unsigned int Dust_ambient_effect_handle = 0xFFFFFFFF; // 0x01648EC8
static float Dust_ambient_los_scale = 1.0f; // 0x01648ECC
static float Dust_ambient_range_multiplier = 1.0f; // 0x01648ED4
static float Dust_ambient_range = 5.0f; // 0x01648ED8
static unsigned int Dust_skirting_effect_info_handle = 0xFFFFFFFF; // 0x01648EDC
static unsigned int Dust_skirting_effect_handle = 0xFFFFFFFF; // 0x01648EE0
static float Dust_skirting_depth = 2.5f; // 0x01648EE8
static float Dust_skirting_min_fade_angle = 90.0f; // 0x01648EEC
static float Dust_skirting_min_fade_depth = 1.0f; // 0x01648EF0
static float Dust_skirting_los_scale = 7.0f; // 0x01648EF4
static float Dust_skirting_transition_rate = 30.0f; // 0x01648EF8
static float Dust_gust_rate = 0.2f; // 0x01648EFC
static float Dust_gust_particle_load = 0.5f; // 0x01648F00
static float Dust_gust_near_dist = 6.0f; // 0x01648F04
static float Dust_gust_near_range = 30.0f; // 0x01648F08
static float Dust_gust_far_dist = 70.0f; // 0x01648F0C
static float Dust_gust_far_range = 50.0f; // 0x01648F10
static int Dust_gust_slope_range = 4; // 0x01648F14
static float Dust_gust_slope_threshold = 0.5f; // 0x01648F18
static float Dust_gust_far_chance = 0.5f; // 0x01648F1C
static timestamp Dust_gust_timer; // 0x0164900C, invalid (-1) from the constructor
static unsigned int Dust_skirting_decal_handles[20]; // 0x01DEE428
static float Dust_ambient_los; // 0x01DEE47C
static bool Dust_skirting_enabled; // 0x01DEE480
static bool Dust_debug; // 0x01DEE481
static float Dust_skirting_max_fade_angle; // 0x01DEE484
static float Dust_skirting_max_fade_depth; // 0x01DEE488
static float Dust_skirting_los; // 0x01DEE48C
static rl_decal_parameters Dust_skirting_decal_params[5]; // 0x01E14088
static game_render_task Update_render_task; // 0x01E24B98
// $S5 (0x01E25740) is a function-static guard; it goes into its function with it.

// Magic numbers: rfg.exe's float literals used in double arithmetic.
static const double MPH_TO_MPS = (double)0.44704f;
static const double DEG_TO_RAD = (double)0.017453292f;
static const double RAD_TO_DEG = (double)57.29578f;

// Not in the binary: a value clamped to [0, 1] the way the inlined clamps do it (the tests in double).
static float dust_clamp01(float value)
{
    if ((float)((double)value - 0.0) < 0.0f)
    {
        value = 0.0f;
    }
    if (0.0f <= (float)((double)value - 1.0))
    {
        value = 1.0f;
    }
    return value;
}

// Not in the binary: the wind's line of sight factor, 1 - the listener's sky line of sight times the scale, clamped.
static float dust_los_factor(float los_scale)
{
    return dust_clamp01((float)(1.0 - (double)gameaudio_get_listener_los() * (double)los_scale));
}

// ---- internal functions

// 0x006E1900 (:226)
// The terrain under pos when the ground around it (Dust_gust_slope_range away on the diagonals) is not more than
// Dust_gust_slope_threshold below it: the lowest of the heights and the zone's effect material there.
// Notes (decompiled behaviour, kept as is): radius is unused (dropped by LTCG).
static bool get_terrain_effect_material(const vector& pos, float radius, unsigned char& mat, vector& actual_heightfield_pos)
{
    float height = 0.0f;
    vector normal;
    if (!havok_terrain_height(height, pos.x, pos.z, false, &normal))
    {
        return false;
    }
    static const vector2 offsets[4] = {vector2(1.0f, 1.0f), vector2(-1.0f, 1.0f), vector2(1.0f, -1.0f), vector2(-1.0f, -1.0f)}; // 0x01E25720, guard $S5
    float heights[4];
    for (int i = 0; i < 4; i++)
    {
        havok_terrain_height(heights[i], offsets[i].x * (float)Dust_gust_slope_range + pos.x, offsets[i].y * (float)Dust_gust_slope_range + pos.z, false, nullptr);
        if (Dust_gust_slope_threshold < height - heights[i])
        {
            return false;
        }
        if (0.0f <= height - heights[i])
        {
            height = heights[i];
        }
    }
    actual_heightfield_pos.set(pos.x, height, pos.z);
    const vector ground(pos.x, 0.0f, pos.z);
    obj_zone* zone = World.get_zone_by_pos(ground);
    if (zone == nullptr)
    {
        return false;
    }
    mat = zone->get_effect_material(0xFFFFFFFF, &actual_heightfield_pos, nullptr);
    return true;
}

// ---- public functions

// 0x006E18D0 (:98)
void dust_enable(bool enabled)
{
    Dust_ambient_enabled = enabled;
    Dust_gust_enabled = enabled;
}

// 0x006E18E0 (:105)
void dust_start()
{
    Dust_ambient_enabled = true;
    Dust_gust_enabled = true;
}

// 0x006E18F0 (:112)
void dust_end()
{
    Dust_ambient_enabled = false;
    Dust_gust_enabled = false;
}

// 0x006E6F80 (:119)
// The skirting decals of the last frame go. With skirting enabled the skirting line of sight eases towards the wind
// speed ratio times the wind's line of sight, and the skirting effect's particles are hidden and become decals (the
// effect's emitters take the skirting decal kinds in turn, at most 5 emitters and 20 decals): sized and colored by the
// particle, on the terrain under it, facing along the particle's direction against the wind's up vector.
void dust_update_render(void*, rl_renderer*)
{
    for (int i = 0; i < 20; i++)
    {
        if (Dust_skirting_decal_handles[i] != 0xFFFFFFFF)
        {
            rl_decal_release(Dust_skirting_decal_handles[i]);
            Dust_skirting_decal_handles[i] = 0xFFFFFFFF;
        }
    }
    if (!Dust_skirting_enabled)
    {
        return;
    }
    vfx_particle_system_filter_instance* wind = vfx_system::get_global_wind_instance();
    float speed;
    float direction;
    weather_wind_get_speed(&speed, &direction);
    const double speed_mps = (double)(float)((double)speed * MPH_TO_MPS);
    const float max_mps = (float)((double)weather_wind_get_max_speed() * MPH_TO_MPS);
    const float speed_ratio = (float)(speed_mps / (double)max_mps);
    const float los = dust_los_factor(Dust_skirting_los_scale);
    const float target = (float)((double)los * (double)speed_ratio);
    Dust_skirting_los = (float)(((double)target - (double)Dust_skirting_los) * ((double)Dust_skirting_transition_rate * (double)Frametime) + (double)Dust_skirting_los);
    if (Dust_skirting_effect_handle == 0xFFFFFFFF)
    {
        return;
    }
    object* obj = World.object_from_handle(Dust_skirting_effect_handle);
    if (obj == nullptr || obj->get_obj_type() != OT_EFFECT)
    {
        return;
    }
    object_effect* effect = (object_effect*)obj;
    vfx_effect* vfx = effect->rdd.is_loaded() ? effect->rdd.get_data()->vfx_effect_p : nullptr;
    if (vfx == nullptr)
    {
        return;
    }
    for (vfx_particle_system_emitter_instance* inst = vfx->m_emitters.first(); inst != nullptr;)
    {
        if (inst->m_renderable_particles_instance_p != nullptr)
        {
            inst->m_renderable_particles_instance_p->set_is_visible(false);
        }
        inst = inst->next;
        if (inst == vfx->m_emitters.first())
        {
            break;
        }
    }
    int num_decals = -1;
    int kind = 0;
    for (vfx_particle_system_emitter_instance* inst = vfx->m_emitters.first(); inst != nullptr && kind < 5; kind++)
    {
        rl_decal_parameters& params = Dust_skirting_decal_params[kind];
        vfx_particle_iterative* particle = inst->m_particle_iterative_list.first();
        while (particle != nullptr)
        {
            num_decals++;
            if (num_decals >= 20)
            {
                break;
            }
            params.m_planar_params.m_width = particle->current_size;
            params.m_planar_params.m_depth = Dust_skirting_depth;
            params.m_planar_params.m_length = particle->current_length;
            params.m_color = particle->current_color;
            params.m_planar_params.m_min_fade_angle = (float)cos((double)(float)((double)Dust_skirting_min_fade_angle * DEG_TO_RAD));
            params.m_planar_params.m_max_fade_angle = (float)cos((double)(float)((double)Dust_skirting_max_fade_angle * DEG_TO_RAD));
            params.m_planar_params.m_min_fade_depth = Dust_skirting_min_fade_depth;
            params.m_planar_params.m_max_fade_depth = Dust_skirting_max_fade_depth;
            obj_zone* zone = World.get_zone_by_pos(particle->position);
            if (zone != nullptr)
            {
                float height;
                havok_terrain_height(height, particle->position.x, particle->position.z, false, nullptr);
                params.m_pos.set(particle->position.x, height, particle->position.z);
                // The wind's rotation with the up and forward vectors swapped.
                params.m_transform = wind->get_world_rotation();
                const vector up = params.m_transform.fvec;
                params.m_transform.fvec = params.m_transform.uvec;
                params.m_transform.uvec = up;
                const vector a = particle->velocity.copy_normalize_safe();
                const vector b = params.m_transform.fvec.copy_normalize();
                const vector c((float)((double)a.y * b.z - (double)a.z * b.y), (float)((double)a.z * b.x - (double)a.x * b.z), (float)((double)a.x * b.y - (double)a.y * b.x));
                // rvec = -a, uvec = a x b, fvec = b.
                params.m_transform.rvec = c;
                params.m_transform.uvec = a;
                params.m_transform.fvec = b;
                const vector rvec = params.m_transform.rvec;
                params.m_transform.rvec = params.m_transform.uvec;
                params.m_transform.uvec = rvec;
                // Magic number: -1.0 (a double constant).
                params.m_transform.rvec.set((float)((double)params.m_transform.rvec.x * -1.0), (float)((double)params.m_transform.rvec.y * -1.0), (float)((double)params.m_transform.rvec.z * -1.0));
                float size = particle->current_size;
                if ((float)((double)particle->current_size - (double)particle->current_length) < 0.0f)
                {
                    size = particle->current_length;
                }
                // Magic number: half the larger extent is the decal radius (a double constant).
                Dust_skirting_decal_handles[num_decals] = object_decal_apply(zone, params, (float)((double)size * 0.5), 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF);
            }
            particle = particle->next;
            if (particle == inst->m_particle_iterative_list.first())
            {
                break;
            }
        }
        inst = inst->next;
        if (inst == vfx->m_emitters.first())
        {
            break;
        }
    }
}

// Not in the binary: a looping dust effect kept at the camera, turned with the wind and moved upwind by the wind speed
// ratio times percent_offset of its visual radius (dust_update inlines it for the ambient and the skirting effect; the
// skirting effect sits at the player's height).
static void dust_update_effect(unsigned int& effect_handle, unsigned int effect_info_handle, float percent_offset, float speed, bool at_player_height)
{
    if (effect_handle == 0xFFFFFFFF)
    {
        effect_handle = effects_play(effect_info_handle, Rfg_camera.real_pos, IDENTITY_MATRIX, true, 0xFFFFFFFF, -1, 0xFFFFFFFF, 1.0f, false, EFFECTS_CATEGORY_NONE);
        if (effect_handle == 0xFFFFFFFF)
        {
            return;
        }
    }
    object* obj = World.object_from_handle(effect_handle);
    if (obj == nullptr || obj->get_obj_type() != OT_EFFECT)
    {
        return;
    }
    object_effect* effect = (object_effect*)obj;
    effect->pos = Rfg_camera.real_pos;
    if (at_player_height)
    {
        player* pp = player_get_local_player();
        if (pp != nullptr)
        {
            effect->pos.y = pp->pos.y;
        }
    }
    effect->orient = vfx_system::get_global_wind_instance()->get_world_rotation();
    const float offset = (float)((double)effect->get_visual_radius() * (double)percent_offset);
    const float upwind = (float)((double)speed / (double)weather_wind_get_max_speed() * ((double)offset - 0.0) + 0.0);
    effect->pos.x = (float)((double)effect->pos.x - (double)(float)((double)effect->orient.fvec.x * (double)upwind));
    effect->pos.y = (float)((double)effect->pos.y - (double)(float)((double)effect->orient.fvec.y * (double)upwind));
    effect->pos.z = (float)((double)effect->pos.z - (double)(float)((double)effect->orient.fvec.z * (double)upwind));
    effect->set_render_flags(4);
}

// Not in the binary: a dust effect stops emitting and is forgotten.
static void dust_stop_effect(unsigned int& effect_handle)
{
    if (effect_handle == 0xFFFFFFFF)
    {
        return;
    }
    object* obj = World.object_from_handle(effect_handle);
    if (obj == nullptr || obj->get_obj_type() != OT_EFFECT)
    {
        return;
    }
    ((object_effect*)obj)->stop(object_effect::SS_STOP_EMITTING);
    effect_handle = 0xFFFFFFFF;
}

// 0x006EBBF0 (:279)
// The ambient dust fades out (by up to 1.2 times the closeness) within Dust_ambient_range of the nearest rfg mover;
// the wind's line of sight (WIND_LOS) is the wind speed in m/s times the listener's sky factor and that fade. The
// ambient and skirting effects follow the camera (or stop when disabled). A gust, when the gust timer is up and the
// parametric particles are below Dust_gust_particle_load: near (Dust_gust_far_chance against a random roll) or far
// ahead of the player, randomly spread, on fairly flat terrain; one of the terrain material's gust effects of that
// distance, the first passing its random chance, else the most likely one; tinted with the material's effect tint.
// Notes (decompiled behaviour, kept as is): the local player is not checked for the gust; with no gust effect of the
// distance the effect index is -1 (reading the slot before the gust list); a gust effect that is not streamed is
// destroyed right after it is played.
void dust_update()
{
    unsigned int movers[10];
    const int num_movers = World.collect_objects<rfg_mover>(Rfg_camera.real_pos, Dust_ambient_range, movers, 10, nullptr, nullptr);
    object* closest = nullptr;
    for (int i = 0; i < num_movers; i++)
    {
        if (movers[i] == 0xFFFFFFFF)
        {
            continue;
        }
        object* obj = World.object_from_handle(movers[i]);
        if (obj == nullptr || obj->get_obj_type() != OT_MOVER || obj->get_sub_type() != OT_SUB_MOVER_RFG)
        {
            continue;
        }
        const float dx = Rfg_camera.real_pos.x - obj->pos.x;
        const float dy = Rfg_camera.real_pos.y - obj->pos.y;
        const float dz = Rfg_camera.real_pos.z - obj->pos.z;
        if (closest == nullptr)
        {
            closest = obj;
            continue;
        }
        const float cx = Rfg_camera.real_pos.x - closest->pos.x;
        const float cy = Rfg_camera.real_pos.y - closest->pos.y;
        const float cz = Rfg_camera.real_pos.z - closest->pos.z;
        const float dist_sq = (float)((double)dy * dy + (double)dx * dx + (double)dz * dz);
        const float closest_sq = (float)((double)cy * cy + (double)cx * cx + (double)cz * cz);
        if ((double)closest_sq > (double)dist_sq)
        {
            closest = obj;
        }
    }
    if (closest != nullptr)
    {
        if (Dust_debug)
        {
            // The debug box drawing is stripped in rfg.exe; only the box query is left.
            vector bmin;
            vector bmax;
            closest->get_bbox(bmin, bmax);
        }
        const float dx = Rfg_camera.real_pos.x - closest->pos.x;
        const float dy = Rfg_camera.real_pos.y - closest->pos.y;
        const float dz = Rfg_camera.real_pos.z - closest->pos.z;
        const float dist = (float)sqrt((double)(float)((double)dy * dy + (double)dx * dx + (double)dz * dz));
        // Magic number: the fade reaches full 1.2 times faster than the closeness.
        const float closeness = dust_clamp01((float)(((double)Dust_ambient_range - (double)dist) / (double)Dust_ambient_range * (double)1.2f));
        Dust_ambient_range_multiplier = (float)(1.0 - (double)closeness);
    }
    else
    {
        Dust_ambient_range_multiplier = 1.0f;
    }
    float speed;
    weather_wind_get_speed(&speed, nullptr);
    const float speed_mps = (float)((double)speed * MPH_TO_MPS);
    const float los = dust_los_factor(Dust_ambient_los_scale);
    Dust_ambient_los = (float)((double)los * (double)speed_mps * (double)Dust_ambient_range_multiplier);
    object_effect_expressions_set_global_variable(EFFECTS_VAR_WIND_LOS, Dust_ambient_los);
    if (Dust_ambient_enabled)
    {
        dust_update_effect(Dust_ambient_effect_handle, Dust_ambient_effect_info_handle, Dust_ambient_percent_offset, speed, false);
    }
    else
    {
        dust_stop_effect(Dust_ambient_effect_handle);
    }
    if (Dust_skirting_enabled)
    {
        dust_update_effect(Dust_skirting_effect_handle, Dust_skirting_effect_info_handle, Dust_skirting_percent_offset, speed, true);
    }
    else
    {
        dust_stop_effect(Dust_skirting_effect_handle);
    }
    if (!Dust_gust_enabled || !Dust_gust_timer.elapsed())
    {
        return;
    }
    Dust_gust_timer.invalidate();
    // Magic number: seconds to milliseconds (a double constant).
    Dust_gust_timer.set((int)((double)Dust_gust_rate * 1000.0));
    const int max_particles = vfx_particle_system_manager::get_particles_parametric_max();
    const int used = max_particles - vfx_system::get_particle_system_manager()->get_num_particles_parametric_free();
    float load = 0.0f;
    if (used != 0)
    {
        load = (float)((double)(float)used / (double)(float)max_particles);
    }
    if (!((double)Dust_gust_particle_load > (double)load))
    {
        return;
    }
    bool near_gust = false;
    float range;
    float dist;
    const float far_chance = Dust_gust_far_chance;
    if (far_chance < fl_rand())
    {
        range = Dust_gust_far_range;
        dist = Dust_gust_far_dist;
    }
    else
    {
        range = Dust_gust_near_range;
        near_gust = true;
        dist = Dust_gust_near_dist;
    }
    player* pp = player_get_local_player();
    const float rx = fl_rand_range(-range, range);
    const float rz = fl_rand_range(-range, range);
    vector pos;
    pos.x = pp->pos.x + pp->orient.fvec.x * dist + rx;
    pos.y = (float)((double)(pp->pos.y + pp->orient.fvec.y * dist) + 0.0);
    pos.z = pp->pos.z + pp->orient.fvec.z * dist + rz;
    unsigned char mat;
    vector ground;
    if (!get_terrain_effect_material(pos, 0.0f, mat, ground))
    {
        return;
    }
    rfg_effect_material* material = rfg_get_effect_material_by_index(mat);
    int pick = -1;
    for (int i = 0; i < material->num_gust_effects; i++)
    {
        if (near_gust != material->gust_effects[i].near_far)
        {
            continue;
        }
        const float chance = material->gust_effects[i].percent_chance;
        if (chance >= fl_rand())
        {
            pick = i;
            break;
        }
    }
    if (pick == -1)
    {
        float best = 0.0f;
        for (int i = 0; i < material->num_gust_effects; i++)
        {
            if (near_gust == material->gust_effects[i].near_far && (double)material->gust_effects[i].percent_chance >= (double)best)
            {
                pick = i;
                best = material->gust_effects[i].percent_chance;
            }
        }
    }
    // Notes (decompiled behaviour, kept as is): pick -1 reads the slot before gust_effects[0].
    const unsigned int gust = ((const gust_effect_info*)((const char*)material->gust_effects + pick * (int)sizeof(gust_effect_info)))->effect;
    if (gust == 0xFFFFFFFF)
    {
        return;
    }
    const unsigned int handle = effects_play(gust, pos, IDENTITY_MATRIX, false, 0xFFFFFFFF, -1, 0xFFFFFFFF, 1.0f, false, EFFECTS_CATEGORY_NONE);
    if (handle == 0xFFFFFFFF)
    {
        return;
    }
    object* obj = World.object_from_handle(handle);
    if (obj == nullptr || obj->get_obj_type() != OT_EFFECT)
    {
        return;
    }
    if (!obj->obj_flags.streamed)
    {
        World.destroy_object(obj, false);
    }
    if (material->effect_tint_enabled)
    {
        ((object_effect*)obj)->set_material_tint(material->effect_tint);
    }
}

// 0x006E1D10 (:547)
void dust_level_init_pre()
{
    Dust_gust_timer.set((int)(Dust_gust_rate * 1000.0)); // seconds to ms (x87: double)
}

// 0x006E1D40 (:553)
void dust_level_shutdown()
{
    Dust_ambient_effect_handle = 0xFFFFFFFF;
    Dust_skirting_effect_handle = 0xFFFFFFFF;
}

// 0x006E1D50 (:575)
void dust_system_shutdown()
{
}

// 0x006E7560 (:560)
// Registers the update render task (the skirting decals); no decals yet.
void dust_system_init()
{
    Update_render_task.m_callback = dust_update_render;
    game_render_update_render_task_register(&Update_render_task);
    for (int i = 0; i < 20; i++)
    {
        Dust_skirting_decal_handles[i] = 0xFFFFFFFF;
    }
}

// 0x006E1B30 (:474)
// weather.xtbl's dust: the ambient effect (the last of the list), the upwind offsets (percent of the visual radius),
// the skirting decal kinds and effect; the skirting depth, fade angles (stored as cosines in the decal, kept in
// degrees) and fade depths come from the first decal kind.
bool weather_internal_parse_dust(xml_element* dust)
{
    mempool_mark_scope mark_scope(&Common_mempool);
    xml_element* list = xml_table_find_req(dust, DUST_EFFECTS_LIST_TAG);
    int count = xml_table_count(list, DUST_EFFECTS_ELEMENT);
    // Magic number: 64-byte names.
    char* names = (char*)Common_mempool.alloc(count << 6, 0);
    xml_table_get_string_list(names, 0x40, count, list);
    for (int i = 0; i < count; i++)
    {
        Dust_ambient_effect_info_handle = effects_get_handle(names + i * 0x40);
    }
    // Magic numbers: percent to fraction (a double constant).
    Dust_ambient_percent_offset = (float)((double)xml_table_get_float(xml_table_find_req(dust, DUST_AMBIENT_MAX_OFFSET_TAG)) / 100.0);
    Dust_skirting_percent_offset = (float)((double)xml_table_get_float(xml_table_find_req(dust, DUST_SKIRTING_MAX_OFFSET_TAG)) / 100.0);
    list = xml_table_find_req(dust, DUST_SKIRTING_DECAL_TAG);
    count = xml_table_count(list, DUST_SKIRTING_DECAL_ELEMENT);
    names = (char*)Common_mempool.alloc(count << 6, 0);
    xml_table_get_string_list(names, 0x40, count, list);
    for (int i = 0; i < count; i++)
    {
        decal_info_get(&Dust_skirting_decal_params[i], names + i * 0x40);
    }
    char effect_name[64];
    xml_table_get_req_string(effect_name, 0x40, dust, DUST_SKIRTING_EFFECT_TAG);
    Dust_skirting_effect_info_handle = effects_get_handle(effect_name);
    const rl_planar_decal_parameters& planar = Dust_skirting_decal_params[0].m_planar_params;
    Dust_skirting_depth = planar.m_depth;
    Dust_skirting_min_fade_angle = (float)((double)(float)acos((double)planar.m_min_fade_angle) * RAD_TO_DEG);
    Dust_skirting_max_fade_angle = (float)((double)(float)acos((double)planar.m_max_fade_angle) * RAD_TO_DEG);
    Dust_skirting_min_fade_depth = planar.m_min_fade_depth;
    Dust_skirting_max_fade_depth = planar.m_max_fade_depth;
    return true;
}

// 0x006E1D60 (:580)
bool dust_object_is_dust_effect(unsigned int obj)
{
    return obj == Dust_ambient_effect_handle || obj == Dust_skirting_effect_handle;
}
