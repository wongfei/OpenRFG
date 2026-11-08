// Original: volition/rfg/code/weapons/arc_welder.cpp
//
// The arc welder (weapons.md WP0, WP2). Each frame process_arc_welder fills the weapon's 2 (3 with the upgrade, always
// in multiplayer) arc points: humans in a sphere in front of the owner (live ones first, then dead ones), else the best
// thing a random ray in the fire cone hits (rated by electricity_hit_rating); while the trigger is down the arcs show
// (electrocution effects and the beam). Firing (fire_electricity) damages what each arc point holds: humans take the
// continuous damage and flinch, items and movers vibrate and take a small damage, vehicles hurt their passengers.
// Compiled in rfg_weapons_unity.cpp, after aim_drift.cpp and before grinder.cpp as in the original compiland.
// The external interface is declared in weapons.h.

#include "volition/rfg/code/weapons/weapons.h"

#include "volition/audio_lib/code/audiolib_att_loop_rel.h"
#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/camera/camera_shake.h"
#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/rfg/code/destruction/rfg_materials.h"
#include "volition/rfg/code/effects/effects.h"
#include "volition/rfg/code/gameplay/upgrades.h"
#include "volition/rfg/code/gameplay/wrecking_crew/wrecking_crew.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/havok/havok_ragdoll.h"
#include "volition/rfg/code/interface/localization/localization_shared.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_game/multi_game_settings.h"
#include "volition/rfg/code/multi/multi_game/multi_game_xp.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_combat.h"
#include "volition/rfg/code/objects/humans/human_combat_turret.h"
#include "volition/rfg/code/objects/humans/human_ragdoll.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/human_weapons.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/level_object_properties.h"
#include "volition/rfg/code/objects/movers/object_mover.h"
#include "volition/rfg/code/objects/turret.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/vehicle/vehicle_interaction.h"
#include "volition/vlib/code/math/ix.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/os/frametime.h"

// ---- file-static globals
static float Arc_welder_human_damage_continuous = 23.0f; // 0x016EBC24, single player damage per fire on a human
static float Arc_welder_human_damage_continuous_mp = 21.0f; // 0x016EBC28, the same in multiplayer
static float Arc_welder_human_in_vehicle_damage = 10.0f; // 0x016EBC2C, per seated human of an arced vehicle
static float Arc_welder_explosive_damage_multiplier = 5.0f; // 0x016EBC30, movers with a death explosion or script point effect
static bool Arc_welder_show_default_hits; // 0x03882CB4, debug: draw arcs without a target

// The tags a human's arc point picks from (int_rand_range(0, 27)).
static const int ARC_WELDER_NUM_HUMAN_TAGS = 28;

// 0x00BE47C0 (:90; LTCG: best_arc_point in EAX)
// The arc to a point: sometimes an electrocution effect on a random tag of the hit human and the hit effect at the
// point, always the beam from the muzzle. Both chances draw from the global generator first.
static void create_electrocution_effect(const weapon* wp, const arc_welder_cache_info& best_arc_point)
{
    static float draw_default_electrocution_effect_random_percentage = 0.3f; // 0x016EBD60
    static float draw_human_electrocution_effect_random_percentage = 0.15f; // 0x016EBD64
    if (wp == nullptr)
    {
        return;
    }
    object* hit = World.object_from_handle(best_arc_point.hit_handle);
    if (hit == nullptr)
    {
        return;
    }
    vector muzzle_pos;
    matrix muzzle_orient;
    wp->get_muzzle_transform(muzzle_pos, &muzzle_orient);
    vector hit_pos = best_arc_point.hit_pos;
    matrix hit_orient;
    hit_orient.make(best_arc_point.hit_normal);
    const float human_chance = draw_human_electrocution_effect_random_percentage;
    // Original: the hit object is looked up again by its own handle.
    if (fl_rand() <= human_chance && hit->get_handle() != 0xFFFFFFFF)
    {
        object* again = World.object_from_handle(hit->get_handle());
        if (again != nullptr && again->get_obj_type() == OT_HUMAN)
        {
            const int tag_index = int_rand_range(0, ARC_WELDER_NUM_HUMAN_TAGS - 1);
            hit->get_tag_world_coords(tag_index, hit_pos, nullptr, nullptr, nullptr);
            effects_play(wp->w_info->secondary_special_effect, hit_pos, hit_orient, false, hit->get_handle(), tag_index, 0xFFFFFFFF, 1.0f, false, EFFECTS_CATEGORY_NONE);
        }
    }
    const float default_chance = draw_default_electrocution_effect_random_percentage;
    if (fl_rand() <= default_chance)
    {
        effects_play(wp->w_info->special_hit_effect, hit_pos, hit_orient, false, 0xFFFFFFFF, -1, 0xFFFFFFFF, 1.0f, false, EFFECTS_CATEGORY_NONE);
    }
    weapon_create_arc_effect(wp->w_info->special_effect, muzzle_pos, hit_pos, muzzle_orient.fvec);
}

// 0x00BD9180 (:131)
// A movable body gets a random impulse (25) plus one along the arc (15), weaker with distance (none at the weapon's
// range).
static void apply_electricity_vibration_to_object(object* objp, const weapon* wp, vector direction, vector vibration_pos, float distance)
{
    if (objp->havok_handle == 0xFFFFFFFF || !havok_body_is_movable(objp->havok_handle))
    {
        return;
    }
    vector random;
    random.rand_sphere();
    // Magic numbers: 25 random, 15 along the arc.
    const vector push(direction.x * 15.0f + random.x * 25.0f, direction.y * 15.0f + random.y * 25.0f, direction.z * 15.0f + random.z * 25.0f);
    float scale = (float)(1.0 - (double)distance / (double)wp->w_info->max_range);
    if ((float)((double)scale - 0.0) < 0.0f)
    {
        scale = 0.0f;
    }
    if ((float)((double)scale - 1.0) >= 0.0f)
    {
        scale = 1.0f;
    }
    const vector impulse(push.x * scale, push.y * scale, push.z * scale);
    havok_body_apply_point_impulse(objp->havok_handle, impulse, vibration_pos);
}

// 0x00BE0320 (:152; LTCG: start_pos in EAX)
// One arc point's damage (from fire_electricity).
static void arc_welder_damage_object(const weapon* wp, arc_welder_cache_info& best_arc_point, human* owner, const vector& start_pos)
{
    if (wp == nullptr)
    {
        return;
    }
    object* objp = World.object_from_handle(best_arc_point.hit_handle);
    if (objp == nullptr)
    {
        return;
    }
    vector hit_pos = best_arc_point.hit_pos;
    vector direction(hit_pos.x - start_pos.x, hit_pos.y - start_pos.y, hit_pos.z - start_pos.z);
    const float distance = direction.mag_and_normalize_safe();
    switch (objp->get_obj_type())
    {
    case OT_HUMAN:
    {
        // The continuous damage replaces the table's (the multiplayer value in multiplayer); a random tag is hit.
        human* hp = (human*)objp;
        const int tag_index = int_rand_range(0, ARC_WELDER_NUM_HUMAN_TAGS - 1);
        hp->get_tag_world_coords(tag_index, hit_pos, nullptr, nullptr, nullptr);
        const weapon_info* w_info = wp->w_info;
        float damage = w_info->low_scale_damage.damage[weapons_get_damage_category(hp)];
        if (Arc_welder_human_damage_continuous > 0.0f)
        {
            damage = Multiplayer == true ? Arc_welder_human_damage_continuous_mp : Arc_welder_human_damage_continuous;
        }
        human_damage(hp, owner, damage, DBT_NONE, w_info, nullptr, true, false);
        if (objp->get_handle() == Rfg_camera.target_handle && wp->w_info->player_hit_camera_shake != nullptr)
        {
            camera_shake_play(wp->w_info->player_hit_camera_shake, 1.0f, false);
        }
        if ((player*)objp == player_get_local_player())
        {
            human_combat_process_hit_indicator(hp, nullptr, &direction, damage);
        }
        // Magic numbers: the flinch (or ragdoll impulse) point within 0.5 m of the human, an impulse up to 2.
        vector impulse_pos;
        impulse_pos.rand_sphere();
        impulse_pos = vector(hp->pos.x + impulse_pos.x * 0.5f, hp->pos.y + impulse_pos.y * 0.5f, hp->pos.z + impulse_pos.z * 0.5f);
        vector impulse;
        impulse.rand_sphere();
        impulse = vector(impulse.x * 2.0f, impulse.y * 2.0f, impulse.z * 2.0f);
        if (human_is_dead(hp) || human_is_ragdolled(hp))
        {
            if (Multiplayer || !localization_allow_ragdoll_interaction())
            {
                return;
            }
            human_ragdoll_start(hp, false, 0xFFFFFFFF);
            havok_ragdoll_apply_point_impulse(hp->ragdoll_idx, impulse, impulse_pos);
            return;
        }
        npc* np = World.object_cast<npc>(hp);
        bool electrocuted = false;
        if (np != nullptr)
        {
            turret* tp = human_combat_get_manned_turret(hp);
            if (tp != nullptr && !tp->turret_flags.vehicle_turret)
            {
                human_combat_abandon_turret(hp);
            }
            electrocuted = np->get_ai()->register_electrocuted(2000); // Magic number: 2 s
        }
        if (!electrocuted)
        {
            const vector flinch_dir(-best_arc_point.hit_normal.x, -best_arc_point.hit_normal.y, -best_arc_point.hit_normal.z);
            // Magic numbers: a random hit location of the 8 (0-7), a full large flinch (100).
            human_combat_play_flinch(hp, impulse_pos, flinch_dir, (object_hit_location)int_rand_range(0, 7), 100);
        }
        human_weapons_drop_shield(hp);
        return;
    }
    case OT_ITEM:
    {
        apply_electricity_vibration_to_object(objp, wp, direction, hit_pos, distance);
        item* ip = (item*)objp;
        ip->apply_damage(1.0f, DBT_BULLET, hit_pos, best_arc_point.hit_normal, owner, 0xFFFFFFFF, false);
        if (ip->get_sub_type() == OT_SUB_ITEM_WEAPON)
        {
            World.object_cast<weapon>(ip)->explode();
        }
        break;
    }
    case OT_MOVER:
    {
        apply_electricity_vibration_to_object(objp, wp, direction, hit_pos, distance);
        object_mover* mp = (object_mover*)objp;
        const bool explosive = mp->m_level_obj_properties->m_flags.m_death_explosion || mp->has_script_point_effect(best_arc_point.hit_shape_key, nullptr);
        mp->apply_damage(explosive ? Arc_welder_explosive_damage_multiplier : 1.0f, DBT_BULLET, hit_pos, best_arc_point.hit_normal, owner,
                         best_arc_point.hit_shape_key, false);
        return;
    }
    case OT_VEHICLE:
    {
        // Every seated human takes the in-vehicle damage; an npc manning an exposed turret is electrocuted.
        vehicle* vp = (vehicle*)objp;
        int remaining = vp->vii.num_humans;
        if (remaining <= 0)
        {
            return;
        }
        for (int i = 0; i < 8; i++) // Magic number: the 8 seats of farray<vi_human_manager,8>
        {
            const unsigned int handle = vp->vii.humans[i].handle;
            if (handle != 0xFFFFFFFF && remaining-- != 0)
            {
                human* passenger = World.object_from_handle<human>(handle);
                if (passenger != nullptr)
                {
                    human_damage(passenger, owner, Arc_welder_human_in_vehicle_damage, DBT_NONE, wp->w_info, owner, true, false);
                    npc* np = World.object_cast<npc>(passenger);
                    if (np != nullptr && human_combat_get_manned_turret(passenger) != nullptr && !human_in_protected_vehicle_pos(passenger))
                    {
                        np->get_ai()->register_electrocuted(2000); // Magic number: 2 s
                    }
                }
            }
            if (remaining < 1)
            {
                return;
            }
        }
        return;
    }
    default:
        break;
    }
}

// 0x00BE0970 (:310): each of the 2 (3 with the upgrade, always in multiplayer) best arc points damages its object.
// The end position is not used (fire passes a muzzle position that is only set when the weapon overheats).
void weapon::fire_electricity(const vector& start_pos, const vector& end_pos)
{
    human* owner = nullptr;
    if (get_owner_handle() != 0xFFFFFFFF)
    {
        owner = (human*)World.object_from_handle(get_owner_handle());
        if (owner != nullptr && owner->get_obj_type() != OT_HUMAN)
        {
            owner = nullptr;
        }
    }
    const int num_arcs = upgrade_has_arc_welder_3rd_arc() + 2;
    for (int i = 0; i < num_arcs; i++)
    {
        arc_welder_cache_info& best = arc_welder_best_point[i];
        if (best.hit_handle != 0xFFFFFFFF)
        {
            arc_welder_damage_object(this, best, owner, start_pos);
        }
    }
}

// 0x00BDB600 (:325; LTCG: hit_objp in ECX, owner in EAX)
// A cached human stays a target while it is in front of the owner's weapon (its fire cone). shape_key is not used.
static bool arc_welder_pre_collect_cb(object* hit_objp, unsigned int shape_key, const void* owner)
{
    const human* owner_hp = (const human*)owner;
    if (hit_objp == owner || hit_objp == nullptr || hit_objp->get_obj_type() != OT_HUMAN || !hit_objp->obj_flags.streamed)
    {
        return false;
    }
    // Original: a fire info is filled with a random tag of the human and never used (a removed line of sight test);
    // the tag still draws from the global generator.
    weapon_fire_info unused_info;
    owner_hp->get_equipped_weapon(); // result unused in rfg.exe
    const int tag_index = int_rand_range(0, ARC_WELDER_NUM_HUMAN_TAGS - 1);
    hit_objp->get_tag_world_coords(tag_index, unused_info.end_pos, nullptr, nullptr, nullptr);
    const weapon* wp = owner_hp->get_equipped_weapon();
    return wp != nullptr && vec_dot_to_point(wp->orient.fvec, wp->pos, hit_objp->pos) >= wp->w_info->fire_cone_dot;
}

// 0x00BE9020 (:357)
// A human near the owner is a target when a random tag of it is in the fire cone (the camera's for a player) and in
// sight of the weapon (the bullet test hits nothing or the human).
static bool arc_welder_collect_cb(object* hit_objp, unsigned int shape_key, const void* owner)
{
    const human* owner_hp = (const human*)owner;
    if (hit_objp == owner || hit_objp == nullptr || hit_objp->get_obj_type() != OT_HUMAN || !hit_objp->obj_flags.streamed)
    {
        return false;
    }
    weapon_fire_info info;
    // Original: the owner's weapon is not checked for null here.
    info.start_pos = owner_hp->get_equipped_weapon()->pos;
    const int tag_index = int_rand_range(0, ARC_WELDER_NUM_HUMAN_TAGS - 1);
    hit_objp->get_tag_world_coords(tag_index, info.end_pos, nullptr, nullptr, nullptr);
    const weapon* wp = owner_hp->get_equipped_weapon();
    if (wp == nullptr)
    {
        return false;
    }
    info.w_info = wp->w_info;
    const matrix* orient = nullptr;
    if (object_is_player(owner_hp))
    {
        const player* pp = owner_hp != nullptr && owner_hp->get_obj_type() == OT_HUMAN && owner_hp->get_sub_type() == OT_SUB_HUMAN_PLAYER ? (const player*)owner_hp : nullptr;
        player_get_camera_pos_and_orient(pp, nullptr, &orient);
    }
    else
    {
        orient = &wp->orient;
    }
    if (vec_dot_to_point(orient->fvec, info.start_pos, info.end_pos) >= info.w_info->fire_cone_dot)
    {
        if (!weapons_test_bullet(info, owner_hp, false, 0.0f, vector(0.0f, 0.0f, 0.0f)) || info.hit_handle == hit_objp->get_handle())
        {
            return true;
        }
    }
    return false;
}

// 0x00BD9330 (:414)
// How good a human target is by team disposition. Friendly and allied humans count only with friendly fire (the
// multiplayer option; always in Wrecking Crew; in single player without the smart arc welder).
static electricity_hit_rating arc_welder_get_hit_rating_based_on_team_disposition(const human* hp1, const human* hp2)
{
    bool friendly_fire;
    if (Multiplayer || wrecking_crew_is_active())
    {
        friendly_fire = true;
        if (Multiplayer == true)
        {
            friendly_fire = Mp_server_options.generic_options.friendly_fire;
        }
    }
    else
    {
        friendly_fire = !upgrade_has_arc_welder_smart();
    }
    switch (human_teams_get_disposition(hp1, hp2))
    {
    case TEAM_DISPOSITION_HOSTILE:
        return ELECTRICITY_HIT_RATING_HOSTILE_HUMAN;
    case TEAM_DISPOSITION_UNFRIENDLY:
        return ELECTRICITY_HIT_RATING_UNFRIENDLY_HUMAN;
    case TEAM_DISPOSITION_FRIENDLY:
        return friendly_fire ? ELECTRICITY_HIT_RATING_FRIENDLY_HUMAN : ELECTRICITY_HIT_RATING_NONE;
    case TEAM_DISPOSITION_ALLIED:
        return friendly_fire ? ELECTRICITY_HIT_RATING_ALLIED_HUMAN : ELECTRICITY_HIT_RATING_NONE;
    default:
        return ELECTRICITY_HIT_RATING_HUMAN;
    }
}

// 0x00BE9180 (:438; LTCG: wp in EAX, start_pos in EDI)
// One arc point without a human: a ray of the weapon's range in a random direction of the fire cone around the camera
// direction (10% of the time straight at the cached point, else 20% of the time near it), rated by what it hits; the
// cache takes a better hit, an equal one that is nearer (with up to 0.2 m of random slack), or the straight shot. A
// cached point out of range or out of the cone is dropped first.
static void arc_welder_test(weapon* wp, arc_welder_cache_info& best_arc_point, const vector& start_pos, const vector& test_direction)
{
    static const float random_test_near_best_offset = 0.999f; // 0x0133BEC4, the dot of the retry around the cached point
    if (wp == nullptr)
    {
        return;
    }
    human* owner = nullptr;
    if (wp->get_owner_handle() != 0xFFFFFFFF)
    {
        owner = (human*)World.object_from_handle(wp->get_owner_handle());
        if (owner != nullptr && owner->get_obj_type() != OT_HUMAN)
        {
            owner = nullptr;
        }
    }
    vector to_best(best_arc_point.hit_pos.x - start_pos.x, best_arc_point.hit_pos.y - start_pos.y, best_arc_point.hit_pos.z - start_pos.z);
    float best_dist = to_best.mag_and_normalize_safe();
    vector test_dir;
    test_dir.rand_around_dir(test_direction.copy_normalize_safe(), wp->w_info->fire_cone_dot, 1.0f);
    bool straight = false;
    const float best_dot = (float)((double)test_direction.y * to_best.y + (double)to_best.x * test_direction.x + (double)test_direction.z * to_best.z);
    if (best_dist > wp->w_info->max_range || best_dot < wp->w_info->fire_cone_dot)
    {
        best_arc_point.hit_handle = 0xFFFFFFFF;
        best_arc_point.hit_rating = ELECTRICITY_HIT_RATING_NONE;
    }
    else if (fl_rand() <= 0.1f) // Magic number: 10% straight at the cached point
    {
        test_dir = to_best;
        straight = true;
    }
    else if (fl_rand() <= 0.2f && best_arc_point.hit_rating > ELECTRICITY_HIT_RATING_NONE) // Magic number: 20% near it
    {
        vector near_dir;
        near_dir.rand_around_dir(to_best, random_test_near_best_offset, 1.0f);
        if (near_dir * test_direction > wp->w_info->fire_cone_dot)
        {
            test_dir = near_dir;
        }
    }
    const float range = wp->w_info->max_range;
    weapon_fire_info info;
    info.w_info = wp->w_info;
    info.start_pos = start_pos;
    info.end_pos = vector(start_pos.x + test_dir.x * range, start_pos.y + test_dir.y * range, start_pos.z + test_dir.z * range);
    info.hit_handle = 0xFFFFFFFF;
    if (!weapons_test_bullet(info, owner, false, 0.0f, vector(0.0f, 0.0f, 0.0f)))
    {
        return;
    }
    object* hit = World.object_from_handle(info.hit_handle);
    if (hit == nullptr)
    {
        return;
    }
    electricity_hit_rating rating;
    switch (hit->get_obj_type())
    {
    case OT_HUMAN:
    {
        // Original: the tag position is computed and not used.
        vector tag_pos;
        hit->get_tag_world_coords(int_rand_range(0, ARC_WELDER_NUM_HUMAN_TAGS - 1), tag_pos, nullptr, nullptr, nullptr);
        if (hit->is_killed())
        {
            rating = ELECTRICITY_HIT_RATING_DEAD_HUMAN;
        }
        else
        {
            rating = arc_welder_get_hit_rating_based_on_team_disposition((human*)hit, owner);
            if (rating == ELECTRICITY_HIT_RATING_NONE)
            {
                return;
            }
        }
        break;
    }
    case OT_VEHICLE:
        rating = ELECTRICITY_HIT_RATING_VEHICLE;
        if (human_teams_are_friendly(human_teams_get_team(hit), HUMAN_TEAM_GUERILLA) && upgrade_has_arc_welder_smart())
        {
            return;
        }
        break;
    case OT_ZONE:
    {
        // Only conductive world surfaces (physical material flag 0x100) carry an arc.
        const rfg_physical_material* material = rfg_get_physical_material_by_index(info.hit_physical_material);
        if (material == nullptr || (material->flags & 0x100) == 0)
        {
            // Original: clears the cache, and a straight shot below still takes this zone hit (rating none) into it.
            rating = ELECTRICITY_HIT_RATING_NONE;
            best_arc_point.hit_handle = 0xFFFFFFFF;
            best_arc_point.hit_rating = ELECTRICITY_HIT_RATING_NONE;
        }
        else
        {
            rating = ELECTRICITY_HIT_RATING_ELECTRIFIABLE_OBJECT;
        }
        break;
    }
    default:
    {
        const rfg_physical_material* material = rfg_get_physical_material_by_index(info.hit_physical_material);
        rating = material == nullptr || (material->flags & 0x100) == 0 ? ELECTRICITY_HIT_RATING_OBJECT : ELECTRICITY_HIT_RATING_ELECTRIFIABLE_OBJECT;
        break;
    }
    }
    const vector to_hit(info.hit_pos.x - start_pos.x, info.hit_pos.y - start_pos.y, info.hit_pos.z - start_pos.z);
    // Magic number: up to 0.2 m of random slack in favour of the new hit.
    best_dist = (float)(fl_rand() * (double)0.2f + best_dist);
    const float hit_dist_squared = (float)((double)to_hit.x * to_hit.x + (double)to_hit.y * to_hit.y + (double)to_hit.z * to_hit.z);
    if (rating > best_arc_point.hit_rating || straight ||
        (rating == best_arc_point.hit_rating && (float)((double)best_dist * best_dist) > hit_dist_squared))
    {
        best_arc_point.hit_pos = info.hit_pos;
        best_arc_point.hit_normal = info.hit_normal;
        best_arc_point.hit_handle = info.hit_handle;
        best_arc_point.hit_shape_key = info.hit_shape_key;
        best_arc_point.hit_rating = rating;
    }
}

// 0x00BE09E0 (:604; LTCG: dead_count in EDI)
// The humans whose box touches the sphere and that pass the callback, split into live and dead ones (no duplicates, up
// to each maximum). In multiplayer allies are left out unless friendly fire is on; in single player only hostile and
// unfriendly humans count.
static void collect_and_separate_humans(const human* owner, const vector& pos, float radius, int& live_count, int& dead_count, unsigned int* live_humans,
                                        int max_live_humans, unsigned int* dead_humans, int max_dead_humans,
                                        bool (*collect_cb)(object*, unsigned int, const void*), const void* cb_user_data)
{
    const int num_humans = World.get_num_objects<human>();
    if (num_humans == 0)
    {
        return;
    }
    const human* owner_player = owner != nullptr && owner->get_obj_type() == OT_HUMAN && owner->get_sub_type() == OT_SUB_HUMAN_PLAYER ? owner : nullptr;
    for (int i = 0; i < num_humans; i++)
    {
        human* hp = World.get_object<human>(i);
        vector bmin;
        vector bmax;
        hp->get_bbox(bmin, bmax);
        if (!ix_sphere_in_box(pos, radius, bmin, bmax) || (collect_cb != nullptr && !collect_cb(hp, 0xFFFFFFFF, cb_user_data)))
        {
            continue;
        }
        if (Multiplayer == true)
        {
            if (owner_player != nullptr && human_teams_get_disposition(owner_player, hp) == TEAM_DISPOSITION_ALLIED && !Mp_server_options.generic_options.friendly_fire)
            {
                continue;
            }
        }
        else if (human_teams_get_disposition(owner, hp) >= TEAM_DISPOSITION_NONE)
        {
            continue;
        }
        unsigned int* list = live_humans;
        int* count = &live_count;
        int max_count = max_live_humans;
        if (hp->is_killed())
        {
            list = dead_humans;
            count = &dead_count;
            max_count = max_dead_humans;
        }
        if (*count < max_count)
        {
            bool listed = false;
            for (int j = 0; j < *count; j++)
            {
                if (list[j] == hp->get_handle())
                {
                    listed = true;
                    break;
                }
            }
            if (!listed)
            {
                list[*count] = hp->get_handle();
                (*count)++;
            }
        }
        if (dead_count == max_dead_humans && live_count == max_live_humans)
        {
            return;
        }
    }
}

// 0x00BD93C0 (:673; LTCG: awp in EAX)
// The looping sounds: the non-arcing one while the trigger is down, the arcing one while it arcs (started once the
// 300 ms after the last arc have passed), both stopped on release. The local player hears the player sounds, others
// the npc fire sound.
static void arc_welder_process_alr(weapon* awp, int& arcing_alr_id, int& non_arcing_alr_id, player* player_ptr)
{
    const player* local = player_get_local_player();
    const weapon_info* w_info = awp->w_info;
    if (w_info->fire_sound == -1 || w_info->special_sound == -1)
    {
        return;
    }
    int arcing_cue;
    int non_arcing_cue;
    if (local == player_ptr)
    {
        arcing_cue = w_info->special_sound;
        non_arcing_cue = w_info->fire_sound;
    }
    else
    {
        arcing_cue = w_info->npc_fire_sounds[0];
        non_arcing_cue = arcing_cue;
    }
    if (!awp->arc_welder_post_arc_timer.valid())
    {
        awp->arc_welder_post_arc_timer.set(0);
    }
    // Original: the position of a null player is passed as an invalid pointer; null here.
    const vector* sound_pos = player_ptr != nullptr ? &player_ptr->pos : nullptr;
    if (!awp->is_trigger_pulled())
    {
        if (non_arcing_alr_id != -1)
        {
            audiolib_att_loop_rel_stop(non_arcing_alr_id, 500); // Magic number: a 500 ms fade
            non_arcing_alr_id = -1;
        }
        if (arcing_alr_id != -1)
        {
            audiolib_att_loop_rel_stop(arcing_alr_id, 500);
            arcing_alr_id = -1;
        }
        return;
    }
    if (awp->special_flags.arc_welder_is_arcing)
    {
        if (arcing_alr_id == -1 && awp->arc_welder_post_arc_timer.elapsed())
        {
            audiolib_att_loop_rel_start(arcing_cue, &arcing_alr_id, false, sound_pos, nullptr);
        }
        awp->arc_welder_post_arc_timer.set(300); // Magic number: 300 ms
        return;
    }
    if (arcing_alr_id != -1)
    {
        audiolib_att_loop_rel_stop(arcing_alr_id, 500);
        arcing_alr_id = -1;
    }
    if (non_arcing_alr_id == -1)
    {
        audiolib_att_loop_rel_start(non_arcing_cue, &non_arcing_alr_id, false, sound_pos, nullptr);
    }
}

// 0x00BD9510 (:740): both looping sounds stop.
void weapon::arc_welder_stop_sounds()
{
    int* ids[2] = {&special_sound_id, &fire_sound_alr_id};
    for (int i = 0; i < 2; i++)
    {
        if (*ids[i] != -1)
        {
            audiolib_att_loop_rel_stop(*ids[i], 500); // Magic number: a 500 ms fade
            *ids[i] = -1;
        }
    }
}

// 0x00BE96E0 (:752)
// See the file comment. The search runs every frame for the owner's current arc welder; the arc points keep the
// cached humans that are still in range and in the cone, add the humans collect_and_separate_humans finds (live first,
// then dead), and fill the rest with arc_welder_test's rays.
void weapon::process_arc_welder()
{
    static float random_create_effect_percentage = 1.0f; // 0x016EBD70
    static float framerate_degradation_multiplier = 0.03f; // 0x016EBD74
    // Magic numbers: the ray origin (0.4 right, 1.2 up, 0.1 ahead) and the human sphere centre (0.4 right, 2 up, 4
    // ahead) in the camera's frame. The range is the first arc welder's (set once, in rfg.exe under a guard bit too).
    static const vector ray_cast_offset(0.4f, 1.2f, 0.1f); // 0x038B2F78
    static const vector human_sphere_test_offset(0.4f, 2.0f, 4.0f); // 0x038B2F6C
    if (!obj_flags.streamed)
    {
        return;
    }
    human* hp = nullptr;
    player* pp = nullptr;
    if (get_owner_handle() != 0xFFFFFFFF)
    {
        object* owner = World.object_from_handle(get_owner_handle());
        if (owner != nullptr && owner->get_obj_type() == OT_HUMAN)
        {
            hp = (human*)owner;
            if (owner->get_sub_type() == OT_SUB_HUMAN_PLAYER)
            {
                pp = (player*)owner;
            }
        }
    }
    arc_welder_process_alr(this, special_sound_id, fire_sound_alr_id, pp);
    if (hp == nullptr || human_weapons_get_weapon(hp) != this)
    {
        return;
    }
    const matrix* camera_orient = nullptr;
    if (Rfg_camera.mode == CAMERA_SLEW_MODE && object_is_local_player(hp))
    {
        camera_orient = &orient;
    }
    else
    {
        player_get_camera_pos_and_orient(pp, nullptr, &camera_orient);
    }
    const vector ray_offset = ray_cast_offset.copy_unrotate(*camera_orient);
    const vector ray_origin(hp->pos.x + ray_offset.x, ray_offset.y + hp->pos.y, ray_offset.z + hp->pos.z);
    const vector camera_dir = camera_orient->fvec;
    const vector sphere_offset = human_sphere_test_offset.copy_unrotate(*camera_orient);
    const vector sphere_center(hp->pos.x + sphere_offset.x, sphere_offset.y + hp->pos.y, sphere_offset.z + hp->pos.z);
    static const float hack_human_test_range = w_info->max_range; // 0x038B2F68
    int live_count = 0;
    int dead_count = 0;
    const int num_arcs = upgrade_has_arc_welder_3rd_arc() + 2;
    // Original: an npc owner calls get_equipped_weapon on a null player (a crash in rfg.exe if ever reached; npcs do
    // not carry arc welders). The port returns.
    if (pp == nullptr)
    {
        return;
    }
    const weapon* equipped = pp->get_equipped_weapon();
    if (equipped == nullptr || equipped->w_info == nullptr)
    {
        return;
    }
    // The humans of the arc points, live ones in [0, 3), dead ones in [3, 6).
    unsigned int humans[6];
    for (int i = 0; i < num_arcs; i++)
    {
        human* cached = World.object_from_handle<human>(arc_welder_best_point[i].hit_handle);
        if (cached == nullptr)
        {
            continue;
        }
        vector bmin;
        vector bmax;
        cached->get_bbox(bmin, bmax);
        if (ix_sphere_in_box(sphere_center, hack_human_test_range, bmin, bmax) && arc_welder_pre_collect_cb(cached, 0, hp))
        {
            if (human_is_dead(cached))
            {
                humans[3 + dead_count++] = cached->get_handle();
            }
            else
            {
                humans[live_count++] = cached->get_handle();
            }
        }
    }
    collect_and_separate_humans(hp, sphere_center, hack_human_test_range, live_count, dead_count, humans, num_arcs, humans + 3, num_arcs, arc_welder_collect_cb, hp);
    // Multiplayer: three enemies arced at once (the "BBQ" experience event; stand-in).
    if (Multiplayer == true && live_count > 2 && flags.trigger_down)
    {
        int enemies = 0;
        for (int i = 0; i < live_count; i++)
        {
            player* target = World.object_from_handle<player>(humans[i]);
            if (target != nullptr && human_teams_are_enemies(pp, target))
            {
                enemies++;
            }
        }
        if (enemies > 2)
        {
            // Magic numbers: one repetition, no weapon (0xFF) or backpack (-1) id.
            multi_game_xp_trigger_event(pp->mp_data, MXE_BBQ, 1, 0xFF, -1, false);
        }
    }
    int num_humans = live_count + dead_count;
    if (num_humans > num_arcs)
    {
        num_humans = num_arcs;
    }
    for (int i = 0; i < num_humans; i++)
    {
        arc_welder_cache_info& best = arc_welder_best_point[i];
        human* target;
        if (i < live_count)
        {
            target = World.object_from_handle<human>(humans[i]);
            best.hit_handle = humans[i];
            best.hit_rating = arc_welder_get_hit_rating_based_on_team_disposition(target, hp);
        }
        else
        {
            const unsigned int human_handle = humans[3 + i - live_count];
            target = World.object_from_handle<human>(human_handle);
            best.hit_handle = human_handle;
            best.hit_rating = ELECTRICITY_HIT_RATING_DEAD_HUMAN;
        }
        best.hit_normal = vector(-camera_dir.x, -camera_dir.y, -camera_dir.z);
        target->get_tag_world_coords(int_rand_range(0, ARC_WELDER_NUM_HUMAN_TAGS - 1), best.hit_pos, nullptr, nullptr, nullptr);
    }
    for (int i = num_humans; i < num_arcs; i++)
    {
        arc_welder_cache_info& best = arc_welder_best_point[i];
        arc_welder_test(this, best, ray_origin, camera_dir);
        // Humans the ray finds (the hostile and unfriendly ratings) are not kept: they come from the human search.
        if (best.hit_rating > ELECTRICITY_HIT_RATING_VEHICLE)
        {
            best.hit_handle = 0xFFFFFFFF;
            best.hit_rating = ELECTRICITY_HIT_RATING_NONE;
        }
    }
    // Fewer arc effects at low frame rates: 0.03 per frame per second, between 0.7 and 1.
    random_create_effect_percentage = framerate_degradation_multiplier * Frames_per_second;
    if ((double)random_create_effect_percentage - 0.7 < 0.0)
    {
        random_create_effect_percentage = 0.7f;
    }
    if (0.0 <= (double)random_create_effect_percentage - 1.0)
    {
        random_create_effect_percentage = 1.0f;
    }
    special_flags.arc_welder_is_arcing = 0;
    for (int i = 0; i < num_arcs; i++)
    {
        const arc_welder_cache_info& best = arc_welder_best_point[i];
        if ((Arc_welder_show_default_hits || best.hit_handle != 0xFFFFFFFF) && best.hit_rating != ELECTRICITY_HIT_RATING_NONE && flags.trigger_down)
        {
            special_flags.arc_welder_is_arcing = 1;
            if (object_is_local_player(hp))
            {
                camera_shake_play(w_info->secondary_camera_shake, 1.0f, false);
            }
            if (fl_rand_chance(random_create_effect_percentage))
            {
                create_electrocution_effect(this, best);
            }
        }
    }
}
