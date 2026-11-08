// Original: volition/rfg/code/code_effects/explosion.cpp
//
// Explosions (decompiled; header comment in explosion.h).
//
// explosion_create queues an explosion (Delay_info_pool, 20; the position also relative to the source so it follows
// it); explosion_do_frame creates the queued ones once the previous explosion's 50-125 ms delay is up (explosions of
// the same source and owner go at once). Creating one: camera shake, its damage (or, for expanding explosions with a
// Duration, a shape cutter object that applies it, see rfg_shape_cutter.cpp), salvage, blast decals, effects and
// undergrowth destruction. The damage: every object (but humans) within the secondary radius and inside the cone
// gets damage falling off from the minimum to the maximum hitpoints and an impulse within the primary radius (away
// from the explosion, movers lifted up to 45 degrees), then the humans (explosion_process_humans: hits within the
// flinch radius, AI hearing within the AI sound radius).
//
// The server sends every explosion it creates to the clients (PT_CREATE_EXPLOSION, multiplayer.md MP8), which create
// it as a sim event (or at once with the 8 events in use). unused_dcf_explosion (a console command) is not recovered.
//
// Notes (decompiled behaviour, kept as is):
// - explosion_create_internal checks the stream radius against the explosion's position box only for explosions
//   flagged 0x10 (far explosions are skipped).

#include "volition/rfg/code/code_effects/explosion.h"

#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/ai/percept.h"
#include "volition/rfg/code/camera/camera_shake.h"
#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/rfg/code/code_effects/explosion_shared.h"
#include "volition/rfg/code/destruction/rfg_materials.h"
#include "volition/rfg/code/destruction/rfg_shape_cutter.h"
#include "volition/rfg/code/effects/effects.h"
#include "volition/rfg/code/effects/object_decal.h"
#include "volition/rfg/code/effects/object_effect.h"
#include "volition/rfg/code/gameplay/activities/activity_raid.h"
#include "volition/rfg/code/gameplay/activities/activity_register_damage.h"
#include "volition/rfg/code/gamewide/stats.h"
#include "volition/rfg/code/geom/character_instance.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/level/undergrowth.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/level/world.inl"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_clock.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_compression.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_io.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_objects.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_packet_types.h"
#include "volition/rfg/code/multi/multi_core/include/multi_session.h"
#include "volition/rfg/code/multi/multi_game/multi_game_camera.h"
#include "volition/rfg/code/multi/multi_game/multi_game_object_backpack.h"
#include "volition/rfg/code/multi/multi_game/multi_game_settings.h"
#include "volition/rfg/code/multi/multi_game/multi_game_sim_event.h"
#include "volition/rfg/code/multi/multi_game/multi_game_sim_state.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_combat.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/movers/object_mover.h"
#include "volition/rfg/code/objects/object_debris.h"
#include "volition/rfg/code/objects/object_item.h"
#include "volition/rfg/code/objects/projectile.h"
#include "volition/rfg/code/objects/salvage.h"
#include "volition/rfg/code/objects/turret.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/math/ix.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/memory/object_pool.h"
#include "volition/vlib/code/os/cs_wrapper.h"
#include "volition/vlib/code/time/timestamp.h"

#include <string.h>

static const int MAX_EXPLOSION_TARGETS = 256;
static const int MAX_PENDING_EXPLOSIONS = 20;
static const float STREAM_RADIUS = 340.0f; // explosions flagged 0x10 only happen this close to the stream position
static const float FLINCH_RADIUS_PAD = 1.9f; // humans' feet within flinch radius + 1.9 m are tested further
static const float MOVER_LIFT = 0.70710677f; // movers are pushed up to 45 degrees upwards

// ---- file-static globals
static unsigned int Last_source_handle = 0xFFFFFFFF; // 0x01648F20
static unsigned int Last_owner_handle = 0xFFFFFFFF; // 0x01648F24
static unsigned int Player_owner_of_last_explosive = 0xFFFFFFFF; // 0x01648F28
static float Salvage_explosion_impulse_multiplier = 150.0f; // 0x01648F2C
static float Super_debris_object_linear = 4.0f; // 0x01648F30, impulse scale of the super debris cheat
static float Super_debris_object_angular = 2.0f; // 0x01648F34
static timestamp Explosion_delay; // 0x01649010
static object_pool<explosion_create_info, MAX_PENDING_EXPLOSIONS, 1> Delay_info_pool; // 0x01E1BE70
static cs_wrapper Explosion_wrapper(0); // 0x01E252F4

// ---- file-private types (rfg.pdb)
// A received explosion waiting as a sim event.
// size 0x48
struct explosion_event_data
{
    const weapon_info* w_info; // 0x00
    const explosion_info* e_info; // 0x04
    network_to_game_handle n2g_source_handle; // 0x08
    network_to_game_handle n2g_owner_handle; // 0x10
    matrix orient; // 0x18
    vector pos; // 0x3C
};

static object_pool<explosion_event_data, 8, 1> Explosion_event_allocator; // 0x01E24C30
static explosion_event_data Temp_event_data; // 0x01E25378, the received explosion when all 8 events are in use

// ---- globals
bool Explosion_ignore_orient; // 0x01DEE482, every explosion points up

// 0x006E5E10
explosion_create_info::explosion_create_info()
{
}

// 0x006E1DC0 (:247)
// Everything but humans and the source.
static bool explosion_collector_callback(object* hit_objp, unsigned int shape_key, const void* user_data)
{
    return hit_objp->get_obj_type() != OT_HUMAN && hit_objp != user_data;
}

// The area damage factor of a player's multiplayer backpack (1 otherwise).
static float explosion_area_damage_factor(const human* hp)
{
    if (hp == nullptr || hp->get_obj_type() != OT_HUMAN || hp->get_sub_type() != OT_SUB_HUMAN_PLAYER)
    {
        return 1.0f;
    }
    const multi_object_backpack* backpack = ((const player*)hp)->backpack;
    if (backpack == nullptr)
    {
        return 1.0f;
    }
    return backpack->get_backpack_abilities()->area_damage_dealt_factor; // inlined
}

// 0x006EF850 (:262)
// LTCG: p_explosion_info in EDI, source in ECX, owner in EDX. Damage and impulses for every object but humans (and
// the source, unless it is an rfg_mover) within the secondary radius (scaled by the owner's backpack) and in the cone;
// grenades (projectiles) are not pushed by explosions flagged 0x8; the debris of the exploding vehicle is not deleted. Then the
// humans and the movers whose source exploded.
static void explosion_apply_damage(const explosion_info* p_explosion_info, object* source, object* owner, const vector& position, const vector* dir, const weapon_info* w_info)
{
    float radius = p_explosion_info->m_secondary_radius;
    human* attacker = nullptr;
    if (owner != nullptr && owner->get_obj_type() == OT_HUMAN)
    {
        attacker = (human*)owner;
        radius = explosion_area_damage_factor(attacker) * radius;
    }
    const object* ignore = source != nullptr && source->get_sub_type() != OT_SUB_MOVER_RFG ? source : nullptr;
    unsigned int handles[MAX_EXPLOSION_TARGETS];
    const int num_handles = havok_collect_objects_fast(position, radius, handles, MAX_EXPLOSION_TARGETS, COLLIDE_OBJECT_COLLECTOR, explosion_collector_callback, ignore);
    for (int i = 0; i < num_handles; i++)
    {
        object* target = World.object_from_handle(handles[i]);
        if (!explosion_is_obj_in_shape(target, p_explosion_info, position, dir))
        {
            continue;
        }
        unsigned int damage = 0;
        vector linear_impulse;
        vector angular_impulse;
        if (!explosion_get_damage_and_impulse(damage, linear_impulse, angular_impulse, p_explosion_info, target, position, dir, nullptr))
        {
            continue;
        }
        if (stats_cheat_enabled(CHEATS_SUPER_DEBRIS))
        {
            linear_impulse = linear_impulse * Super_debris_object_linear;
            angular_impulse = angular_impulse * Super_debris_object_angular;
        }
        switch (target->get_obj_type())
        {
        case OT_ITEM:
            if (target->get_sub_type() == OT_SUB_ITEM_PROJECTILE)
            {
                if ((p_explosion_info->flags & 8) == 0)
                {
                    ((projectile*)target)->process_explosion_hit(&linear_impulse, &angular_impulse, attacker);
                }
            }
            else
            {
                item_process_explosion_hit((item*)target, damage, linear_impulse, angular_impulse);
            }
            break;
        case OT_MOVER:
            object_mover_process_explosion_hit((object_mover*)target, owner, (float)damage, linear_impulse, angular_impulse, p_explosion_info, position, dir, w_info);
            break;
        case OT_VEHICLE:
            vehicle_process_explosion_hit((vehicle*)target, attacker, source, position, p_explosion_info->m_radius * 0.75f, damage, &linear_impulse, &angular_impulse, w_info, p_explosion_info);
            break;
        case OT_DEBRIS:
            object_debris_process_explosion_hit((object_debris*)target, &linear_impulse, &angular_impulse, source == nullptr || ((object_debris*)target)->m_create_info.m_vehicle_handle != source->get_handle());
            break;
        case OT_TURRET:
            ((turret*)target)->process_explosion_hit(damage, linear_impulse, angular_impulse);
            break;
        default:
            break;
        }
    }
    explosion_process_humans(source, attacker, w_info, position, dir, p_explosion_info, owner);
    object_mover_handle_source_exploded(source, p_explosion_info);
}

// 0x006EFBF0 (:348)
// LTCG: p_explosion_info in ECX, owner in EAX. See the file comment. A camera shake falls off over
// camera_shake_falloff (when the explosion pushes); expanding explosions (flag 0x8000) create a shape cutter; on a
// multiplayer client only the movers hear that their source exploded. The decal hit picks the material effect; the
// nano rifle's explosions make nano undergrowth destruction.
static void explosion_create_internal(const explosion_info* p_explosion_info, object* source, object* owner, const vector& position, const matrix& orientation, const weapon_info* w_info, const vector* dir)
{
    if (Multiplayer && multi_is_server())
    {
        multi_gw_send_explosion_create(p_explosion_info, source, owner, orientation, position, w_info);
    }
    activity_register_explosion(owner, p_explosion_info);
    if (p_explosion_info->m_impulse_magnitude != 0.0f)
    {
        float shake = 1.0f - sqrtf(vec_dist_squared(position, Rfg_camera.real_pos)) / p_explosion_info->camera_shake_falloff;
        if (shake - 0.0f < 0.0f)
        {
            shake = 0.0f;
        }
        if (0.0f <= shake - 1.0f)
        {
            shake = 1.0f;
        }
        shake = p_explosion_info->camera_shake_multiplier * shake;
        if (0.1f < shake)
        {
            camera_shake_play(p_explosion_info->camera_shake_type, shake, false);
        }
    }
    Explosion_delay.set_rand(50, 125);
    Last_source_handle = source != nullptr ? source->get_handle() : 0xFFFFFFFF;
    Last_owner_handle = owner != nullptr ? owner->get_handle() : 0xFFFFFFFF;
    if ((p_explosion_info->flags & 0x10) != 0)
    {
        const vector bmin(position.x - 0.5f, position.y - 0.5f, position.z - 0.5f);
        const vector bmax(position.x + 0.5f, position.y + 0.5f, position.z + 0.5f);
        if (!ix_box_cylinder_xz(bmin, bmax, World.stream_pos, STREAM_RADIUS))
        {
            return;
        }
    }
    if (!Multiplayer || multi_is_server())
    {
        if ((p_explosion_info->flags & 0x8000) == 0)
        {
            explosion_apply_damage(p_explosion_info, source, owner, position, dir, w_info);
        }
        else
        {
            shape_cutter_props pblock(position, orientation, (unsigned char)p_explosion_info->m_unique_id, w_info, source != nullptr ? source->get_handle() : 0xFFFFFFFF, owner != nullptr ? owner->get_handle() : 0xFFFFFFFF);
            World.create_object<shape_cutter>(pblock, 0xFFFFFFFF); // inlined
        }
    }
    else
    {
        object_mover_handle_source_exploded(source, p_explosion_info);
    }
    if (salvage_should_spawn() && 0 < p_explosion_info->salvage_max_pieces)
    {
        for (int i = 0; i < p_explosion_info->salvage_max_pieces; i++)
        {
            if (p_explosion_info->salvage_probability < fl_rand())
            {
                continue;
            }
            item* salvage = salvage_create_item(p_explosion_info->salvage_material, position, orientation);
            if (salvage != nullptr && salvage->havok_handle != 0xFFFFFFFF)
            {
                vector impulse;
                impulse.rand_sphere();
                impulse.set(impulse.x * Salvage_explosion_impulse_multiplier, Salvage_explosion_impulse_multiplier * fabsf(impulse.y), impulse.z * Salvage_explosion_impulse_multiplier);
                havok_body_apply_point_impulse(salvage->havok_handle, impulse, position);
            }
        }
    }
    rfg_effect_material* decal_material = object_decal_create_blast_decals(position, p_explosion_info->blast_decal_radius);
    if (decal_material != nullptr)
    {
        for (unsigned int i = 0; i < p_explosion_info->m_num_material_effects; i++)
        {
            if (decal_material != rfg_get_effect_material_by_index(p_explosion_info->m_material_referenece[i]))
            {
                continue;
            }
            object_effect* effect = World.object_from_handle<object_effect>(effects_play(p_explosion_info->m_material_effects[i], position, orientation, false, 0xFFFFFFFF, -1, 0xFFFFFFFF, 1.0f, false, EFFECTS_CATEGORY_EXPLOSION));
            if (effect != nullptr && decal_material->effect_tint_enabled)
            {
                effect->set_material_tint(decal_material->effect_tint);
            }
            break;
        }
    }
    for (unsigned int i = 0; i < p_explosion_info->m_num_effects; i++)
    {
        object_effect* effect = World.object_from_handle<object_effect>(effects_play(p_explosion_info->m_effects[i], position, orientation, false, 0xFFFFFFFF, -1, 0xFFFFFFFF, 1.0f, false, EFFECTS_CATEGORY_EXPLOSION));
        if (effect != nullptr && decal_material != nullptr && decal_material->effect_tint_enabled)
        {
            effect->set_material_tint(decal_material->effect_tint);
        }
    }
    const undergrowth_destruction_event_type ugde_type = w_info != nullptr && w_info->weapon_class == WEAPON_CLASS_NANO_RIFLE ? UDET_NANO : UDET_EXPLOSION;
    undergrowth_create_destruction_event(position, p_explosion_info->m_radius, ugde_type);
}

// 0x006E1DF0 (:487)
// While the delay of the last explosion runs, only explosions of the same source and owner.
static bool explosion_can_happen_now(unsigned int source_handle, unsigned int owner_handle)
{
    return !Explosion_delay.valid() || (source_handle == Last_source_handle && owner_handle == Last_owner_handle);
}

// 0x006E1E20 (:494)
// LTCG: explosion_dir in EAX, e_info in ECX.
static bool explosion_cone_check(const vector& bmin, const vector& bmax, const explosion_info* e_info, const vector& explosion_pos, const vector* explosion_dir)
{
    if (explosion_dir == nullptr)
    {
        return false;
    }
    return ix_box_in_cone(bmin, bmax, explosion_pos, *explosion_dir, e_info->half_angle_dot);
}

// 0x006EF640 (:144)
// Partial: the "explosion <name>" console command (an explosion at the local player) is not recovered.
void unused_dcf_explosion()
{
}

// 0x006EC710 (:512)
void explosion_system_init()
{
    explosion_parse_table(-1);
}

// 0x006F2810 (:525)
void explosion_level_init_pre()
{
    {
        csspinlock_scope lock(Delay_info_pool.thread_access);
        explosion_create_info* info = Delay_info_pool.get_first(0);
        while (info != nullptr)
        {
            info = Delay_info_pool.free(info);
        }
    }
    Explosion_delay.invalidate();
}

// 0x006E76B0 (:537)
player* explosion_owner_of_last_explosion()
{
    return World.object_from_handle<player>(Player_owner_of_last_explosive);
}

// 0x006F0160 (:543)
// The queued explosions (back where their source is now), until one has to wait.
void explosion_do_frame()
{
    if (Explosion_delay.valid())
    {
        if (!Explosion_delay.elapsed())
        {
            return;
        }
        Explosion_delay.invalidate();
    }
    explosion_create_info* info = Delay_info_pool.get_first(0);
    while (info != nullptr)
    {
        if (!explosion_can_happen_now(info->source_handle, info->owner_handle)) // inlined
        {
            return;
        }
        Delay_info_pool.free(info); // kept: still read below
        object* source = World.object_from_handle(info->source_handle);
        object* owner = World.object_from_handle(info->owner_handle);
        if (source != nullptr)
        {
            info->pos = info->local_pos;
            vec_change_to_global_coords(info->pos, source->pos, source->orient);
        }
        explosion_create_internal(info->p_explosion_info, source, owner, info->pos, info->orient, info->w_info, &info->dir);
        info = Delay_info_pool.get_first(0);
    }
}

// 0x006F0270 (:579)
bool explosion_clear_all_pending()
{
    explosion_create_info* info = Delay_info_pool.get_first(0);
    if (info == nullptr)
    {
        return false;
    }
    while (info != nullptr)
    {
        Delay_info_pool.free(info);
        info = Delay_info_pool.get_first(0);
    }
    return true;
}

// 0x006EC720 (:605)
// Queues an explosion (multiplayer clients only from the server). With more than 5 pending, gauss rifle hits only
// play their effect. When the queue is full, the movers hear that their source exploded.
void explosion_create(const explosion_info* p_explosion_info, object* source, object* owner, const vector& position, const matrix& orientation, const vector* dir, const weapon_info* w_info, bool from_server)
{
    static unsigned int gauss_rifle_info_handle; // $S6 guards its lookup
    static bool gauss_rifle_info_handle_set;
    if (Multiplayer && !multi_is_server() && !from_server)
    {
        return;
    }
    cs_scope lock(Explosion_wrapper);
    if (5 < MAX_PENDING_EXPLOSIONS - Delay_info_pool.get_free())
    {
        if (!gauss_rifle_info_handle_set)
        {
            gauss_rifle_info_handle_set = true;
            gauss_rifle_info_handle = weapons_get_weapon_info_handle("gauss_rifle");
        }
        if (w_info != nullptr && w_info->weapon_class == WEAPON_CLASS_BULLET && weapons_get_weapon_info_handle(w_info) == gauss_rifle_info_handle)
        {
            effects_play(p_explosion_info->m_effects[0], position, orientation, false, 0xFFFFFFFF, -1, 0xFFFFFFFF, 1.0f, false, EFFECTS_CATEGORY_EXPLOSION);
            return;
        }
    }
    explosion_create_info* info = Delay_info_pool.alloc(0);
    if (info == nullptr)
    {
        object_mover_handle_source_exploded(source, p_explosion_info);
        return;
    }
    info->p_explosion_info = p_explosion_info;
    info->source_handle = source != nullptr ? source->get_handle() : 0xFFFFFFFF;
    info->owner_handle = owner != nullptr ? owner->get_handle() : 0xFFFFFFFF;
    info->orient = orientation;
    info->pos = position;
    if (source != nullptr)
    {
        info->local_pos = position;
        vec_change_to_local_coords(info->local_pos, source->pos, source->orient);
    }
    info->w_info = w_info;
    info->dir = dir != nullptr ? *dir : ZERO_VECTOR;
    if (p_explosion_info->ignore_orientation || Explosion_ignore_orient)
    {
        info->dir = UP_VECTOR;
        info->orient.make_identity();
    }
    if (Multiplayer)
    {
        multi_game_camera_explosion_occured(info->pos, p_explosion_info->m_knockdown_radius);
    }
}

// 0x006E1E50 (:686)
// Impulses limited to a velocity: the linear one as velocity (impulse x scalar / mass, at most max_linear_velocity)
// times the mass, the angular one scaled and limited directly.
void explosion_modify_impulses(vector* modified_linear_impulse, vector* modified_angular_impulse, const vector* linear_impulse, const vector* angular_impulse, float mass, float linear_impulse_scalar, float angular_impulse_scalar, float max_linear_velocity, float max_angular_velocity)
{
    if (modified_linear_impulse != nullptr)
    {
        *modified_linear_impulse = *linear_impulse;
        float velocity = (modified_linear_impulse->mag_and_normalize_safe() * linear_impulse_scalar) / mass;
        if (0.0f <= velocity - max_linear_velocity)
        {
            velocity = max_linear_velocity;
        }
        *modified_linear_impulse = *modified_linear_impulse * (velocity * mass);
    }
    if (modified_angular_impulse != nullptr)
    {
        *modified_angular_impulse = *angular_impulse;
        float magnitude = modified_angular_impulse->mag_and_normalize_safe() * angular_impulse_scalar;
        if (0.0f <= magnitude - max_angular_velocity)
        {
            magnitude = max_angular_velocity;
        }
        *modified_angular_impulse = *modified_angular_impulse * magnitude;
    }
}

// 0x006EC930 (:724)
// Humans (visible, not the source; in multiplayer without friendly fire not the attacker's team) whose feet are
// within the flinch radius + 1.9 m and whose body (feet to head) passes within the flinch radius, inside the cone,
// are hit; living npcs hear it within the AI sound radius (unless deaf) or see it, but not raid demolition charges.
// The flinch radius grows with the attacker's backpack. In multiplayer the player who caused it is remembered while
// the hits are processed.
void explosion_process_humans(const object* source, human* attacker, const weapon_info* w_info, const vector& pos, const vector* dir, const explosion_info* e_info, const object* owner)
{
    if (e_info == nullptr)
    {
        return;
    }
    const float backpack_factor = explosion_area_damage_factor(attacker);
    const float test_radius = (e_info->m_flinch_radius + FLINCH_RADIUS_PAD) * backpack_factor;
    float flinch_radius = e_info->m_flinch_radius * backpack_factor;
    const float sound_radius_sqr = e_info->m_ai_sound_radius * e_info->m_ai_sound_radius;
    flinch_radius = flinch_radius * flinch_radius;
    if (sound_radius_sqr <= 0.0f && flinch_radius <= 0.0f)
    {
        return;
    }
    Player_owner_of_last_explosive = 0xFFFFFFFF;
    if (Multiplayer)
    {
        const object* source_player = source != nullptr && source->get_obj_type() == OT_HUMAN && source->get_sub_type() == OT_SUB_HUMAN_PLAYER ? source : nullptr;
        const object* owner_player = owner != nullptr && owner->get_obj_type() == OT_HUMAN && owner->get_sub_type() == OT_SUB_HUMAN_PLAYER ? owner : nullptr;
        if (source_player == nullptr || source_player == owner_player)
        {
            if (owner_player != nullptr)
            {
                Player_owner_of_last_explosive = owner_player->get_handle();
            }
        }
        else
        {
            Player_owner_of_last_explosive = source_player->get_handle();
            source = nullptr;
        }
    }
    const bool demo_charge = activity_raid_is_building_demo_charge(pos, e_info);
    const int num_humans = World.get_num_objects<human>();
    for (int i = 0; i < num_humans; i++)
    {
        human* hp = World.get_object<human>(i);
        if (hp->hflags.hidden || !hp->char_inst->rdd.is_loaded() || (const object*)hp == source)
        {
            continue;
        }
        if (Multiplayer == true && attacker != hp && !Mp_server_options.generic_options.friendly_fire && attacker != nullptr && human_teams_are_friendly(attacker, hp))
        {
            continue;
        }
        float dist_sqr = vec_dist_squared(hp->pos, pos);
        if (dist_sqr <= test_radius * test_radius)
        {
            vector head_pos;
            human_get_head_pos(hp, head_pos);
            vector closest;
            ix_closest_point_on_line_segment(closest, pos, hp->pos, head_pos);
            dist_sqr = vec_dist_squared(closest, pos);
            if (dist_sqr <= flinch_radius && explosion_is_obj_in_shape(hp, e_info, pos, dir))
            {
                human_combat_process_explosion_hit(hp, attacker, w_info, pos, e_info, owner, false);
            }
        }
        if (attacker != nullptr && hp != attacker && !human_is_dead(hp) && hp->get_sub_type() == OT_SUB_HUMAN_NPC)
        {
            npc* np = World.object_cast<npc>(hp);
            ai_system* ai = np->get_ai(); // inlined
            if (((!np->npc_flags.deaf && dist_sqr <= sound_radius_sqr) || ai->perception.within_view_frustum(pos)) && !demo_charge)
            {
                ai->perception.explosion_observed(attacker, pos, e_info);
            }
        }
    }
    Player_owner_of_last_explosive = 0xFFFFFFFF;
}

// 0x006E76E0 (:861)
// The damage (minimum to maximum hitpoints, falling off over the secondary radius from the target's closest point;
// target_use_pos overrides it) and, within the primary radius, the impulse: along dir when the explosion is inside the
// target, otherwise away from the explosion; movers are lifted (up to 45 degrees), vehicles with the player get
// player_vehicle_impulse_mult; the angular impulse turns about the horizontal axis across it. Cloth items only get a
// direction and 1 damage; humans are an error.
bool explosion_get_damage_and_impulse(unsigned int& p_damage, vector& p_linear_impulse, vector& p_angular_impulse, const explosion_info* e_info, const object* target, const vector& explosion_pos, const vector* dir, const vector* target_use_pos)
{
    if (e_info == nullptr || target == nullptr)
    {
        return false;
    }
    float lift = 0.0f;
    vector impulse_dir;
    impulse_dir.zero();
    float impulse_scale = 1.0f;
    float dist;
    vector target_pos;
    switch (target->get_obj_type())
    {
    case OT_HUMAN:
        return false; // Int3 critical function
    case OT_ITEM:
        if (((const item*)target)->cloth_sim != nullptr)
        {
            p_linear_impulse = target->pos - explosion_pos;
            p_damage = 1;
            p_angular_impulse.zero();
            return true;
        }
        dist = vec_dist(target->pos, explosion_pos);
        target_pos = target->pos;
        break;
    case OT_MOVER:
        lift = MOVER_LIFT;
        // fall through
    case OT_VEHICLE:
    case OT_DEBRIS:
    case OT_TURRET:
    {
        vector bmin;
        vector bmax;
        target->get_local_bbox(bmin, bmax);
        vector local_pos = explosion_pos - target->pos;
        local_pos.rotate(target->orient);
        vector closest;
        dist = sqrtf(bbox_find_closest_point(closest, bmin, bmax, local_pos)); // the squared distance
        if (dist == 0.0f)
        {
            // inside: from the centre, along dir
            const vector center((bmin.x + bmax.x) * 0.5f, (bmin.y + bmax.y) * 0.5f, (bmin.z + bmax.z) * 0.5f);
            target_pos = center.copy_unrotate(target->orient) + target->pos;
            impulse_dir = dir != nullptr ? *dir : ZERO_VECTOR;
        }
        else
        {
            target_pos = closest.copy_unrotate(target->orient) + target->pos;
        }
        if (target->get_obj_type() == OT_VEHICLE && vehicle_player_in_vehicle((const vehicle*)target))
        {
            impulse_scale = e_info->player_vehicle_impulse_mult;
        }
        break;
    }
    default:
        dist = vec_dist(target->pos, explosion_pos);
        target_pos = target->pos;
        break;
    }
    if (target_use_pos != nullptr)
    {
        target_pos = *target_use_pos;
        dist = vec_dist(*target_use_pos, explosion_pos);
        impulse_dir.zero();
    }
    const float falloff = fl_parameter(dist, e_info->m_secondary_radius, 0.0f, true);
    p_damage = fl_round((float)e_info->m_vehicle_min_damage_hitpoints + (float)(e_info->m_vehicle_max_damage_hitpoints - e_info->m_vehicle_min_damage_hitpoints) * falloff);
    if (e_info->m_radius <= dist)
    {
        p_linear_impulse.zero();
        p_angular_impulse.zero();
        return true;
    }
    impulse_scale = e_info->m_impulse_magnitude * falloff * impulse_scale;
    p_linear_impulse = impulse_dir;
    if (p_linear_impulse.x * p_linear_impulse.x + p_linear_impulse.y * p_linear_impulse.y + p_linear_impulse.z * p_linear_impulse.z == 0.0f)
    {
        p_linear_impulse = target_pos - explosion_pos;
    }
    p_linear_impulse.normalize_safe();
    if (0.0f < lift && 0.0f < p_linear_impulse.y)
    {
        p_linear_impulse.y = (1.0f - p_linear_impulse.y) * lift + p_linear_impulse.y;
    }
    p_angular_impulse.set(p_linear_impulse.z * UP_VECTOR.y - p_linear_impulse.y * UP_VECTOR.z, p_linear_impulse.x * UP_VECTOR.z - p_linear_impulse.z * UP_VECTOR.x, p_linear_impulse.y * UP_VECTOR.x - p_linear_impulse.x * UP_VECTOR.y);
    p_angular_impulse.normalize_safe();
    p_linear_impulse = p_linear_impulse * impulse_scale;
    p_angular_impulse = p_angular_impulse * impulse_scale;
    return true;
}

// 0x006E1FB0 (:1004)
// Cone explosions (0x10000) only reach objects whose box is in the cone.
bool explosion_is_obj_in_shape(object* objp, const explosion_info* e_info, const vector& explosion_pos, const vector* explosion_dir)
{
    if ((e_info->flags & 0x10000) == 0)
    {
        return true;
    }
    vector bmin;
    vector bmax;
    objp->get_local_bbox(bmin, bmax);
    vector local_pos = explosion_pos - objp->pos;
    local_pos.rotate(objp->orient);
    vector local_dir = *explosion_dir; // not checked
    local_dir.rotate(objp->orient);
    return ix_box_in_cone(bmin, bmax, local_pos, local_dir, e_info->half_angle_dot);
}

// 0x006E20C0 (:1024)
bool explosion_is_bbox_in_shape(const vector& bmin, const vector& bmax, const explosion_info* e_info, const vector& explosion_pos, const vector* explosion_dir)
{
    if ((e_info->flags & 0x10000) == 0)
    {
        return true;
    }
    return explosion_cone_check(bmin, bmax, e_info, explosion_pos, explosion_dir); // inlined
}

// 0x006ECC40 (:1038): the explosion of a received packet, created as from the server
static bool multi_gw_process_explosion_event_callback(explosion_event_data* data)
{
    object* source = World.object_from_handle(data->n2g_source_handle.get_game_handle());
    object* owner = World.object_from_handle(data->n2g_owner_handle.get_game_handle());
    if (data->e_info != nullptr)
    {
        explosion_create(data->e_info, source, owner, data->pos, data->orient, nullptr, data->w_info, true);
    }
    return true;
}

// 0x006F02C0 (:1055)
static void multi_gw_process_explosion_event_destroy_callback(void* data)
{
    Explosion_event_allocator.free((explosion_event_data*)data);
}

// 0x006E7E80 (:1064)
// PT_CREATE_EXPLOSION to every client: the network time, the explosion's unique id, the network handles of the
// source and owner, the orientation (pitch, bank and heading, 8 bits each within 198 degrees) and position (16 bits
// each within 512 m) in 9 bytes, and the weapon's unique id (0xFF: none).
void multi_gw_send_explosion_create(const explosion_info* p_explosion_info, const object* source, const object* owner, const matrix& orient, const vector& pos, const weapon_info* w_info)
{
    unsigned char packet[1200];
    packet[0] = PT_CREATE_EXPLOSION;
    const int header = Multi_packets[PT_CREATE_EXPLOSION].group_id != 0 ? 5 : 3;
    const unsigned int network_time = multi_core_clock_get_time();
    packet[header] = (unsigned char)(network_time >> 24);
    packet[header + 1] = (unsigned char)(network_time >> 16);
    packet[header + 2] = (unsigned char)(network_time >> 8);
    packet[header + 3] = (unsigned char)network_time;
    packet[header + 4] = (unsigned char)p_explosion_info->m_unique_id;
    const unsigned short source_net_handle = multi_core_network_handle_from_game_handle(source != nullptr ? source->get_handle() : 0xFFFFFFFF, true);
    packet[header + 5] = (unsigned char)(source_net_handle >> 8);
    packet[header + 6] = (unsigned char)source_net_handle;
    const unsigned short owner_net_handle = multi_core_network_handle_from_game_handle(owner != nullptr ? owner->get_handle() : 0xFFFFFFFF, true);
    packet[header + 7] = (unsigned char)(owner_net_handle >> 8);
    packet[header + 8] = (unsigned char)owner_net_handle;
    unsigned char bits[9] = {};
    bit_packer packer(bits, 9);
    mat_pbh_delta_compress(packer, IDENTITY_MATRIX, orient, 8, 3.4557521f, 0.0f, 1.0f); // Magic number: 198 degrees
    vec_delta_compress(packer, ZERO_VECTOR, pos, 16, 512.0f, 0.0f, 1.0f);
    memcpy(packet + header + 9, bits, 9);
    packet[header + 18] = w_info == nullptr ? 0xFF : (unsigned char)w_info->unique_id;
    const unsigned short payload = (unsigned short)(header + 16);
    packet[1] = (unsigned char)(payload >> 8);
    packet[2] = (unsigned char)payload;
    multi_io_send_to_all_reliable(multi_session_get_by_type(ST_GAME), packet, header + 19);
}

// 0x006F02D0 (:1099)
// The explosion becomes a sim event at network time 0 (created on the next sim event pass); with the 8 events in use
// it is created at once. The network time is not read.
void multi_gw_process_explosion_create(unsigned char* packet_data, const sender& from)
{
    explosion_event_data* data = Explosion_event_allocator.alloc(0);
    const bool pooled = data != nullptr;
    if (!pooled)
    {
        data = &Temp_event_data;
    }
    data->e_info = packet_data[4] == 0xFF ? nullptr : explosion_lookup_by_unique_id(packet_data[4]); // inlined
    data->n2g_source_handle.set_network_handle((unsigned short)((packet_data[5] << 8) | packet_data[6]));
    data->n2g_owner_handle.set_network_handle((unsigned short)((packet_data[7] << 8) | packet_data[8]));
    unsigned char bits[9];
    memcpy(bits, packet_data + 9, 9);
    bit_packer packer(bits, 9);
    mat_pbh_delta_decompress(packer, data->orient, IDENTITY_MATRIX, 8, 3.4557521f, 0.0f, 1.0f);
    vec_delta_decompress(packer, data->pos, ZERO_VECTOR, 16, 512.0f, 0.0f, 1.0f);
    data->w_info = weapons_get_weapon_info_from_weapon_unique_id(packet_data[18]);
    if (!pooled)
    {
        multi_gw_process_explosion_event_callback(data); // inlined
        return;
    }
    multi_game_sim_event_add(0, MULTI_GAME_SIM_EVENT_EXPLOSION, false, data, nullptr, (bool (*)(void*))multi_gw_process_explosion_event_callback, multi_gw_process_explosion_event_destroy_callback); // inlined
}

// 0x006F2860 (:1150): the waiting explosion events are dropped
void multi_game_explosion_event_clear()
{
    for (explosion_event_data* data = Explosion_event_allocator.get_first(0); data != nullptr;)
    {
        data = Explosion_event_allocator.free(data);
    }
}
