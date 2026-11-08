// Original: volition/rfg/code/ai/percept.cpp
//
// ai_perception: the beliefs of one npc, what it notices and hears, attention, and combat target choice (stub_impl.md
// S9). Ported together with ai_system.cpp: ai_system_create being real is what lets the perception handlers run.

#include "volition/rfg/code/ai/percept.h"

#include "volition/anim_lib/code/anim_lib.h"
#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/rfg/code/ai/ai_building_logic.h"
#include "volition/rfg/code/ai/alerts.h"
#include "volition/rfg/code/ai/custom_actions.h"
#include "volition/rfg/code/ai/goals.h"
#include "volition/rfg/code/ai/planning.h"
#include "volition/rfg/code/ai/self_preservation_goals.h"
#include "volition/rfg/code/ai/ai_defense_coordination.h"
#include "volition/rfg/code/ai/ai_rate_limiters.h"
#include "volition/rfg/code/ai/ai_scripting.h"
#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/ai/belief_iterator.h"
#include "volition/rfg/code/ai/beliefs.h"
#include "volition/rfg/code/ai/los_queue.h"
#include "volition/rfg/code/ai/player_attack_limiter.h"
#include "volition/rfg/code/ai/search_system.h"
#include "volition/rfg/code/ai/vehicle/ai_vehicle_logic.h"
#include "volition/rfg/code/code_effects/explosion.h"
#include "volition/rfg/code/code_effects/explosion_shared.h"
#include "volition/rfg/code/destruction/rfg_mover.h"
#include "volition/rfg/code/gameplay/activities/activity.h"
#include "volition/rfg/code/gameplay/activities/activity_raid.h"
#include "volition/rfg/code/gameplay/activities/morale_effect_manager.h"
#include "volition/rfg/code/gameplay/activities/vrail_system.h"
#include "volition/rfg/code/gameplay/gameflow/alert_level_manager.h"
#include "volition/rfg/code/gameplay/gameflow/gfm_orders_manager.h"
#include "volition/rfg/code/gameplay/gameflow/reinforcement_location.h"
#include "volition/rfg/code/gameplay/missions.h"
#include "volition/rfg/code/gameplay/upgrades.h"
#include "volition/rfg/code/gamewide/tweak_table.h"
#include "volition/rfg/code/geom/character_instance.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/havok/havok_raycast.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_combat.h"
#include "volition/rfg/code/objects/humans/human_combat_cover.h"
#include "volition/rfg/code/objects/humans/human_combat_turret.h"
#include "volition/rfg/code/objects/humans/human_info.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/human_weapons.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/humans/npc_sound_info.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/humans/player_autoaim.h"
#include "volition/rfg/code/objects/object_cover_node.h"
#include "volition/rfg/code/objects/object_navpoint.h"
#include "volition/rfg/code/objects/object_restricted_area.h"
#include "volition/rfg/code/objects/object_safehouse.h"
#include "volition/rfg/code/objects/object_squad.h"
#include "volition/rfg/code/objects/projectile.h"
#include "volition/rfg/code/objects/turret.h"
#include "volition/rfg/code/pathfinding/bb_pathfinding.h"
#include "volition/rfg/code/pathfinding/nav_cell.h"
#include "volition/rfg/code/pathfinding/nav_cell_detour.h"
#include "volition/rfg/code/pathfinding/nav_heap.h"
#include "volition/rfg/code/rfg.h"
#include "volition/rfg/code/scripting/public_address.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/vehicle/vehicle_flyer.h"
#include "volition/rfg/code/vehicle/vehicle_system.h"
#include "volition/rfg/code/vehicle_ai/vehicle_ai.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/vlib/code/math/bbox.h"
#include "volition/vlib/code/math/bbox2d.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/math/ix.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/os/frametime.h"

#include <float.h>
#include <math.h>
#include <string.h>

// ---- file-static globals

// Tweak table entries, all zero until tweak_table.xtbl loads.
float Edf_notice_player_with_obvious_weapon_dist; // 0x01D65988
float Ai_fov_length_low; // 0x01D6598C
float Ai_fov_length_low_spotted; // 0x01D67720
float Ai_fov_length_high; // 0x01D65730
float Ai_fov_length_high_spotted; // 0x01D67728
float Ai_fov_width; // 0x01D65724
float Ai_fov_width_spotted; // 0x01D65204
float Ai_fov_height; // 0x01D67DA0
float Ai_fov_height_spotted; // 0x01D67744
int Marauder_kill_taunt_chance; // 0x01D67190
float Ai_target_player_in_walker_dist; // 0x01D65208
float Player_friendly_fire_threshold; // 0x01D6597C
int Player_friendly_fire_expire_time; // 0x01D65218

static tweak_table_item Tweak_table_item_Edf_notice_player_with_obvious_weapon_dist("Edf_notice_player_with_obvious_weapon_dist", &Edf_notice_player_with_obvious_weapon_dist);
static tweak_table_item Tweak_table_item_Ai_fov_length_low("Ai_fov_length_low", &Ai_fov_length_low);
static tweak_table_item Tweak_table_item_Ai_fov_length_low_spotted("Ai_fov_length_low_spotted", &Ai_fov_length_low_spotted);
static tweak_table_item Tweak_table_item_Ai_fov_length_high("Ai_fov_length_high", &Ai_fov_length_high);
static tweak_table_item Tweak_table_item_Ai_fov_length_high_spotted("Ai_fov_length_high_spotted", &Ai_fov_length_high_spotted);
static tweak_table_item Tweak_table_item_Ai_fov_width("Ai_fov_width", &Ai_fov_width);
static tweak_table_item Tweak_table_item_Ai_fov_width_spotted("Ai_fov_width_spotted", &Ai_fov_width_spotted);
static tweak_table_item Tweak_table_item_Ai_fov_height("Ai_fov_height", &Ai_fov_height);
static tweak_table_item Tweak_table_item_Ai_fov_height_spotted("Ai_fov_height_spotted", &Ai_fov_height_spotted);
static tweak_table_item Tweak_table_item_Marauder_kill_taunt_chance("Marauder_kill_taunt_chance", &Marauder_kill_taunt_chance);
static tweak_table_item Tweak_table_item_Ai_target_player_in_walker_dist("Ai_target_player_in_walker_dist", &Ai_target_player_in_walker_dist);
static tweak_table_item Tweak_table_item_Player_friendly_fire_threshold("Player_friendly_fire_threshold", &Player_friendly_fire_threshold);
static tweak_table_item Tweak_table_item_Player_friendly_fire_expire_time("Player_friendly_fire_expire_time", &Player_friendly_fire_expire_time);

static bool Ai_ignore_player; // 0x01D67DCD
static farray<unsigned int, 16> Building_update_npc_list; // 0x01642F00

// Beliefs noticed further away than these are not recorded.
static const float ALLY_NOTICE_MAX_DIST_SQ = 2500.0f;
static const float CORPSE_NOTICE_MAX_DIST_SQ = 400.0f;
static const float BUILDING_COLLECT_MAX_DIST_SQ = 1600.0f;

// Relevance is "distance like": lower is more relevant.
static const float SIGHTED_ENEMY_RELEVANCE = 1.0f;
static const float PROJECTILE_ATTACKER_RELEVANCE = 3.0f;
static const float BULLET_SOUND_ATTACKER_RELEVANCE = 5.0f;
static const float CORPSE_ATTACKER_RELEVANCE = 5.0f;
static const float FIREARM_ATTACKER_RELEVANCE = 10.0f;
static const float EXPLOSION_ATTACKER_RELEVANCE = 15.0f;
static const float LIVE_PROJECTILE_RELEVANCE = 15.0f;
static const float BUILDING_DAMAGE_VANDAL_RELEVANCE = 15.0f;
static const float VEHICLE_CRASH_RELEVANCE = 15.0f;
static const float LOW_BULLET_SOUND_ATTACKER_RELEVANCE = 15.0f;
static const float PAIN_SOUND_ATTACKER_RELEVANCE = 20.0f;
static const float ATTACK_ORDERS_RELEVANCE = 20.0f;

// How long an undercover player's cover lasts, by distance: {max distance, seconds}.
static const float Cover_blown_time_table[6][2] = { { 1.0f, 5.0f }, { 3.0f, 4.0f }, { 5.0f, 3.0f }, { 10.0f, 2.0f }, { 15.0f, 1.0f }, { 20.0f, 0.5f } };
// Indexed by shot_results; SHOT_RESULT_NEUTRAL sets no bit.
static const unsigned int Shot_result_to_belief_flags[ai_perception::NUM_SHOT_RESULTS] = { 0x08000000, 0x10000000, 0x20000000, 0, 0x40000000 };
// Indexed by alert level: green, yellow, orange, red.
static const float Max_engagement_distances[4] = { 30.0f, 40.0f, 60.0f, 80.0f }; // 0x012C8CF4

// The cosines are computed at start-up in rfg.exe (the asm reads doubles of the angles).
static const float AIM_AT_TARGET_DOT = (float)cos(0.20943599939346313);
static const float AIM_AT_TARGET_RUNNING_DOT = (float)cos(0.3141539990901947);
static const float AIM_AT_TARGET_TURRET_DOT = (float)cos(0.3490599989891052);
static const float MARAUDER_VAULT_TARGET_DIR_COS = (float)cos(0.7853981852531433);

static const float PAIN_SOUND_MAX_DIST = 15.0f;
static const float PAIN_SOUND_NEARBY_DISTANCE = PAIN_SOUND_MAX_DIST * 0.5f;
static const float IN_LINE_OF_FIRE_DIST_SQ = 2.25f; // 1.5 m
static const float HEAVY_VEHICLE_NOTICE_MAX_DISTANCE = 120.0f;
static const double DANGEROUS_MOVER_MIN_MASS = 450.0; // compared as doubles in rfg.exe
static const double DANGEROUS_MOVER_LARGE_MASS = 900.0;
static const float HUMAN_HEIGHT = 1.9f;
static const double FOV_DEG_TO_RAD = (double)0.017453f; // float precision, widened (original)
static const int BELIEF_MIN_UPDATE_MS = 50;
static const int LINE_OF_FIRE_MS = 1500;
static const int TARGET_TRACKING_INTERVAL_MS = 250;
static const int TARGET_TRACKING_MAX_GAP_MS = 5000;

// Conditions from ai_plan_conditions passed by number in rfg.exe.
static const int UNDERCOVER_BLOWN_CONDITION_A = 79;
static const int UNDERCOVER_BLOWN_CONDITION_B = 81;
static const int EXPLOSION_CONDITION_A = 116;
static const int EXPLOSION_CONDITION_B = 117;

// Reasons passed to ai_system::react.
static const int AI_REACTION_ALLY_FIRED = 0x001;
static const int AI_REACTION_WEAPON_FIRE = 0x002;
static const int AI_REACTION_EXPLOSION = 0x004;
static const int AI_REACTION_INSIDE_COLLAPSE = 0x008;
static const int AI_REACTION_PROJECTILE = 0x010;
static const int AI_REACTION_FIRST_SIGHTING = 0x040;
static const int AI_REACTION_TAUNTED = 0x080;
static const int AI_REACTION_KILL_TAUNT = 0x100;
static const int AI_REACTION_BOMB_SET = 0x400;

extern float Proxy_mine_radius; // projectile.cpp, 0x03025A9C
extern navpoint_heap Navpoint_heap; // nav_heap.cpp
extern bool Navpoint_treat_all_as_enabled; // object_navpoint.cpp

// ---- helpers

// rfg.exe inlines these in almost every function below.
static object* typed_object_from_handle(unsigned int handle, object_type type)
{
    if (handle == 0xFFFFFFFF)
    {
        return nullptr;
    }
    object* objp = World.object_from_handle(handle);
    if (objp == nullptr || objp->get_obj_type() != type)
    {
        return nullptr;
    }
    return objp;
}

template<typename T>
static T* typed_object_from_handle(unsigned int handle, object_type type)
{
    return static_cast<T*>(typed_object_from_handle(handle, type));
}

static npc* npc_from_handle(unsigned int handle)
{
    human* hp = typed_object_from_handle<human>(handle, OT_HUMAN);
    if (hp == nullptr || hp->get_sub_type() != OT_SUB_HUMAN_NPC)
    {
        return nullptr;
    }
    return static_cast<npc*>(hp);
}

static rfg_mover* rfg_mover_from_handle(unsigned int handle)
{
    object_mover* omp = typed_object_from_handle<object_mover>(handle, OT_MOVER);
    if (omp == nullptr || omp->get_sub_type() != OT_SUB_MOVER_RFG)
    {
        return nullptr;
    }
    return static_cast<rfg_mover*>(omp);
}

// 0x005ECF70 (:87)
view_frustum_parameters::view_frustum_parameters()
{
    initialized = false;
    use_fov = false;
    length = 0.0f;
    h_angle = 0.0f;
    v_angle = 0.0f;
}

// 0x006645A0 (:122)
melee_candidate_selection_info::melee_candidate_selection_info()
{
    least_min_dist = FLT_MAX;
    least_max_dist = FLT_MAX;
    fallback_move = nullptr;
}

// 0x00666DE0
melee_candidate_selection_info::~melee_candidate_selection_info()
{
}

// 0x006804B0 (:949)
void ai_update_combat_target_building(npc* np)
{
    object* target = typed_object_from_handle(np->combat.target_handle, OT_HUMAN);
    if (target == nullptr)
    {
        return;
    }
    const rfg_mover* current = rfg_mover_from_handle(np->combat.targets_building_handle);
    if (current != nullptr && ai_building_find_bbox_dist_squared(target->pos, current) < 9.0f)
    {
        return; // still within 3 m of its building
    }

    np->combat.targets_building_handle = 0xFFFFFFFF;
    float best_dist_sq = FLT_MAX;
    belief_obj_iterator<rfg_mover> it;
    it.init(np->get_ai(), AI_BELIEF_BUILDING);
    while (it)
    {
        rfg_mover* building = it;
        if (building->flags.is_ai_building != 0)
        {
            vector local_pos = target->pos;
            vector bmin, bmax;
            building->get_local_bbox(bmin, bmax);
            vec_change_to_local_coords(local_pos, building->pos, building->orient);
            const float dist_sq = bbox_find_distance_squared(bmin, bmax, local_pos);
            if (dist_sq < 100.0f && dist_sq < best_dist_sq)
            {
                np->combat.targets_building_handle = building->get_handle();
                best_dist_sq = dist_sq;
                if (dist_sq <= 0.0f)
                {
                    return;
                }
            }
        }
        ++it;
    }

    // Tank crews' targets may be in buildings this npc has no belief of: search for those over the following frames.
    if (Building_update_npc_list.size() < Building_update_npc_list.capacity())
    {
        const vehicle* vp = (vehicle*)typed_object_from_handle(np->vehicle_handle, OT_VEHICLE);
        if (vp != nullptr && vehicle_is_tank(vp))
        {
            Building_update_npc_list.add_unique(np->get_handle());
        }
    }
}

// 0x006645D0 (:1009)
void ai_process_target_building_update()
{
    if (Building_update_npc_list.size() == 0)
    {
        return;
    }
    npc* np = npc_from_handle(Building_update_npc_list[0]);
    Building_update_npc_list.remove_unsorted(0);
    if (np == nullptr)
    {
        return;
    }
    const object* target = typed_object_from_handle(np->combat.target_handle, OT_HUMAN);
    if (target == nullptr)
    {
        return;
    }
    float best_dist_sq = FLT_MAX;
    unsigned int buildings[6];
    const int num_buildings = World.collect_objects<rfg_mover>(target->pos, 30.0f, buildings, 6, collect_buildings_callback, np);
    for (int i = 0; i < num_buildings; ++i)
    {
        const rfg_mover* building = rfg_mover_from_handle(buildings[i]);
        if (building != nullptr && building->flags.is_ai_building != 0)
        {
            const float dist_sq = ai_building_find_bbox_dist_squared(target->pos, building);
            if (dist_sq < 100.0f && dist_sq < best_dist_sq)
            {
                np->combat.targets_building_handle = building->get_handle();
                best_dist_sq = dist_sq;
                if (dist_sq <= 0.0f)
                {
                    return;
                }
            }
        }
    }
}

// ---- player_friendly_fire_info

// 0x005EEC40
ai_perception::player_friendly_fire_info::player_friendly_fire_info()
{
}

// 0x005ECF90 (:1056)
void ai_perception::player_friendly_fire_info::reset(const npc* np)
{
    damage_total = 0.0f;
    valid_time.invalidate();
    enabled = false;
    const player* pp = player_get_local_player();
    if (pp != nullptr && !np->h_info->flags.guerrilla_tech && (int)human_teams_get_disposition(np, pp->current_team) > 0
        && (np->current_team == HUMAN_TEAM_MARAUDER || np->current_team == HUMAN_TEAM_GUERILLA))
    {
        enabled = true;
    }
}

// 0x005ECFF0 (:1084)
bool ai_perception::player_friendly_fire_info::register_fire(float amount, const human* victim, const human* attacker)
{
    if (!enabled)
    {
        return false;
    }
    // A hit with damage counts for at least a third of the threshold; one with none counts for all of it.
    float counted = Player_friendly_fire_threshold;
    if (amount > 0.0f)
    {
        counted = Player_friendly_fire_threshold / 3.0f;
        if (amount - counted < 0.0f)
        {
            counted = amount;
        }
    }
    if ((int)gsm_get_alert_level() >= 1)
    {
        return false;
    }
    if (mission_is_active() && (victim->current_team != HUMAN_TEAM_MARAUDER || attacker->current_team == HUMAN_TEAM_MARAUDER))
    {
        return false;
    }
    damage_total += counted;
    valid_time.set(Player_friendly_fire_expire_time * 1000);
    return Player_friendly_fire_threshold <= damage_total;
}

// 0x005ED0D0 (:1138)
void ai_perception::player_friendly_fire_info::do_frame()
{
    if (enabled && valid_time.valid() && valid_time.elapsed())
    {
        damage_total = 0.0f;
        valid_time.invalidate();
    }
}

// 0x005ED110 (:1150)
bool ai_perception::player_friendly_fire_info::is_hostile() const
{
    return enabled && valid_time.valid() && Player_friendly_fire_threshold <= damage_total;
}

// ---- movers and thrown objects

// 0x005EF2C0
mover_path_info::mover_path_info()
{
}

// 0x005EF2D0
thrown_object_path_info::thrown_object_path_info()
{
}

// 0x005ED150 (:1173). LTCG: info in ESI, omp on the stack.
static bool mover_is_dangerous(mover_path_info& info, const object_mover* omp)
{
    info.omp = omp;
    if ((omp->get_sub_type() == OT_SUB_MOVER_RFG && ((const rfg_mover*)omp)->get_move_type() == RFGM_FIXED) || omp->havok_handle == 0xFFFFFFFF)
    {
        return false;
    }
    hkpRigidBody* body = havok_body_get_pointer(omp->havok_handle);
    if (body == nullptr)
    {
        return false;
    }
    if (!body->isActive().m_bool)
    {
        return false;
    }
    float mass;
    vector center_of_mass;
    matrix inertia_tensor;
    havok_body_get_mass_props(body, mass, center_of_mass, inertia_tensor);
    if ((double)mass < DANGEROUS_MOVER_MIN_MASS)
    {
        return false;
    }
    info.center_of_mass = center_of_mass;
    vec_change_to_global_coords(info.center_of_mass, omp->pos, omp->orient);
    havok_body_get_point_velocity(body, info.center_of_mass, info.velocity);
    if ((float)((double)info.velocity.x * info.velocity.x + (double)info.velocity.y * info.velocity.y + (double)info.velocity.z * info.velocity.z) < 4.0f)
    {
        return false; // slower than 2 m/s
    }

    // Radius: the farther bbox corner from the centre of mass.
    vector bmin, bmax;
    omp->get_bbox(bmin, bmax);
    const float dist_bmin_sq = vec_dist_squared(center_of_mass, bmin);
    const float dist_bmax_sq = vec_dist_squared(center_of_mass, bmax);
    info.radius = (float)sqrt((double)(dist_bmin_sq - dist_bmax_sq < 0.0f ? dist_bmax_sq : dist_bmin_sq));

    // Where it will be now, in 1 s and in 2 s; 4.9 and 19.6 are g t^2 / 2.
    info.trajectory_points[0] = info.center_of_mass;
    info.trajectory_points[1].x = (float)((double)info.velocity.x + info.trajectory_points[0].x);
    info.trajectory_points[1].y = (float)((double)info.trajectory_points[0].y + info.velocity.y);
    info.trajectory_points[1].z = (float)((double)info.trajectory_points[0].z + info.velocity.z);
    info.trajectory_points[1].y = (float)((double)info.trajectory_points[1].y - 4.9000000953674316);
    info.trajectory_points[2].x = (float)((double)(float)((double)info.velocity.x * 2.0) + info.trajectory_points[0].x);
    info.trajectory_points[2].y = (float)((double)info.trajectory_points[0].y + (double)(float)((double)info.velocity.y * 2.0));
    info.trajectory_points[2].z = (float)((double)info.trajectory_points[0].z + (double)(float)((double)info.velocity.z * 2.0));
    info.trajectory_points[2].y = (float)((double)info.trajectory_points[2].y - 19.600000381469727);
    info.is_large = (double)mass > DANGEROUS_MOVER_LARGE_MASS && info.radius > 3.0f;
    return true;
}

// 0x005ED460 (:1245). LTCG: info in ESI, wp in EDI.
static bool thrown_object_is_dangerous(thrown_object_path_info& info, const weapon* wp)
{
    info.wp = wp;
    if (wp->w_info == nullptr || !wp->w_info->flags.non_inventory)
    {
        return false;
    }
    // Held by someone: not flying.
    if (wp->is_owned() || wp->havok_handle == 0xFFFFFFFF)
    {
        return false;
    }
    havok_body_get_linear_velocity(wp->havok_handle, info.velocity);
    const unsigned int since_fired = (unsigned int)wp->time_since_fired();
    unsigned int owner = wp->get_owner_handle();
    // Slower objects count for 6 s after their thrower let go of them.
    const bool recently_thrown = owner != 0xFFFFFFFF && since_fired <= 5999;
    const float min_speed_sq = recently_thrown ? 4.0f : 25.0f;
    const float speed_sq = (float)((double)info.velocity.x * info.velocity.x + (double)info.velocity.y * info.velocity.y + (double)info.velocity.z * info.velocity.z);
    if ((double)min_speed_sq > (double)speed_sq)
    {
        return false;
    }
    info.culprit = recently_thrown ? owner : 0xFFFFFFFF;
    const explosion_info* explosion = wp->w_info->m_explosion_info;
    if (explosion != nullptr)
    {
        info.radius = explosion->m_knockdown_radius;
    }
    else
    {
        vector bmin, bmax;
        wp->get_local_bbox(bmin, bmax);
        info.radius = (float)((double)vec_dist(bmin, bmax) * 0.5);
    }
    info.trajectory_points[0] = wp->pos;
    info.trajectory_points[1].x = (float)((double)info.trajectory_points[0].x + info.velocity.x);
    info.trajectory_points[1].y = (float)((double)info.velocity.y + info.trajectory_points[0].y);
    info.trajectory_points[1].z = (float)((double)info.velocity.z + info.trajectory_points[0].z);
    info.trajectory_points[1].y = (float)((double)info.trajectory_points[1].y - 4.9000000953674316);
    info.trajectory_points[2].x = (float)((double)info.trajectory_points[0].x + (double)(float)((double)info.velocity.x * 2.0));
    info.trajectory_points[2].y = (float)((double)(float)((double)info.velocity.y * 2.0) + info.trajectory_points[0].y);
    info.trajectory_points[2].z = (float)((double)info.trajectory_points[0].z + (double)(float)((double)info.velocity.z * 2.0));
    info.trajectory_points[2].y = (float)((double)info.trajectory_points[2].y - 19.600000381469727);
    return true;
}

// 0x0063D770 (:1309). LTCG: src in ESI, target in EAX; the asm has the constants the decompile loses.
static float get_fov_length_bonus(const human* src, const human* target)
{
    if (human_in_vehicle(target) != nullptr)
    {
        return 2.5f;
    }
    const weapon_info* w_info = human_weapons_get_weapon_info(src);
    if (w_info != nullptr && w_info->flags.can_scope)
    {
        return 2.0f;
    }
    const object* vehicle_obj = typed_object_from_handle(src->vehicle_handle, OT_VEHICLE);
    if (vehicle_obj != nullptr)
    {
        return vehicle_obj->get_sub_type() == OT_SUB_VEHICLE_FLYER ? 2.0f : 1.5f;
    }
    return human_combat_get_manned_turret(src) != nullptr ? 1.5f : 1.0f;
}

// 0x005ED6C0 (:1337)
bool ignore_debug_projectile(const projectile* proj)
{
    return false;
}

// 0x005ED6D0 (:1350)
static bool weapon_hit_gives_fix(const weapon_info* w_info)
{
    return !w_info->is_explosive();
}

// 0x005ED6E0 (:1365)
static bool belief_type_is_special(ai_belief_types type)
{
    return Ai_belief_descriptors[type].max_per_npc < 1;
}

// 0x005ED710 (:1378). LTCG: tension_level in EAX, the others unused.
static int get_projectile_react_time(const projectile* proj, const hazard_physics_info& physics_info, ai_tension_levels tension_level)
{
    return int_rand_range(Ai_tension_level_info_table[tension_level].min_react_time, Ai_tension_level_info_table[tension_level].max_react_time);
}

// 0x005ED730 (:1389)
static unsigned int occupied_belief_flags_from_disposition(human_team_dispositions disp)
{
    if (disp == TEAM_DISPOSITION_HOSTILE)
    {
        return BF_OCCUPIED_BY_ENEMY;
    }
    if ((int)disp > 2 && (int)disp < 5)
    {
        return BF_OCCUPIED_BY_FRIEND;
    }
    return BF_OCCUPIED_BY_NEUTRAL;
}

// ---- belief storage

// Unlinks a belief from a bin's circular list.
static void unlink_belief(ai_belief*& head, ai_belief* belief)
{
    if (head == belief)
    {
        if (belief->next == belief)
        {
            belief->next = nullptr;
            belief->prev = nullptr;
            head = nullptr;
            return;
        }
        head = belief->next;
    }
    if (belief->prev != nullptr)
    {
        belief->prev->next = belief->next;
    }
    if (belief->next != nullptr)
    {
        belief->next->prev = belief->prev;
    }
    belief->next = nullptr;
    belief->prev = nullptr;
}

// Appends a belief to the end of a circular list.
static void append_belief(ai_belief*& head, ai_belief* belief)
{
    if (head == nullptr)
    {
        belief->next = belief;
        belief->prev = belief;
        head = belief;
        return;
    }
    belief->prev = head->prev;
    belief->next = head;
    head->prev->next = belief;
    head->prev = belief;
}

// 0x00664760 (:1412)
bool ai_perception::belief_should_expire(ai_belief* belief) const
{
    switch (belief->get_type())
    {
    case AI_BELIEF_ENEMY_EXISTS:
    {
        object* enemy = typed_object_from_handle(belief->handle, OT_HUMAN);
        if (enemy == nullptr || enemy->is_killed())
        {
            return true;
        }
        if (!on_alert() && ((human*)enemy)->is_undercover() && remaining_enemy_search_time(belief) < 1)
        {
            return true;
        }
        break;
    }
    case AI_BELIEF_ENEMY_POSITION:
    case AI_BELIEF_LAST_CONTACT:
        if (human_is_dead(belief->handle))
        {
            return true;
        }
        break;
    case AI_BELIEF_UNFRIENDLY:
        if (human_is_dead(belief->handle))
        {
            return true;
        }
        if (ai->planner.get_current_goal() == confront_civilian_goal::get_instance() && belief->handle == attention.target)
        {
            return belief->update_time.time_since() >= 20000;
        }
        break;
    case AI_BELIEF_HAZARD:
        if (belief->get_obj_type() != OT_UNDEFINED && World.object_from_handle(belief->handle) == nullptr)
        {
            return true;
        }
        break;
    default:
        break;
    }
    const int expire_ms = Ai_belief_descriptors[belief->get_type()].sec_to_expire * 1000;
    return expire_ms > 0 && expire_ms < belief->update_time.time_since();
}

// 0x005ED750 (:1496)
bool ai_perception::insert_belief_into_bin(ai_belief* belief)
{
    const ai_belief_types type = belief->get_type();
    ai_belief_bin& bin = belief_bins[type];
    if (bin.count == Ai_belief_descriptors[type].max_per_npc)
    {
        // Full: the bin is sorted by relevance, lowest first, so the last is the least interesting.
        ai_belief* last = bin.beliefs == nullptr ? nullptr : bin.beliefs->prev;
        if (last->relevance <= belief->relevance)
        {
            return false;
        }
        unlink_belief(bin.beliefs, last);
        append_belief(dead_belief_list, last);
        --bin.count;
    }

    ai_belief* head = bin.beliefs;
    if (head != nullptr)
    {
        ai_belief* cur = head;
        do
        {
            if (belief->relevance <= cur->relevance)
            {
                cur->prev->next = belief;
                belief->prev = cur->prev;
                cur->prev = belief;
                belief->next = cur;
                if (bin.beliefs == cur)
                {
                    bin.beliefs = belief;
                }
                ++bin.count;
                return true;
            }
            cur = cur->next;
        } while (cur != head && cur != nullptr);
    }
    append_belief(bin.beliefs, belief);
    ++bin.count;
    return true;
}

// Finds the belief of a type and handle in a bin.
static ai_belief* find_belief(ai_belief* head, unsigned int handle)
{
    ai_belief* belief = head;
    while (belief != nullptr)
    {
        if (belief->handle == handle)
        {
            return belief;
        }
        belief = belief->next;
        if (belief == head)
        {
            break;
        }
    }
    return nullptr;
}

// 0x006648C0 (:1568)
ai_belief* ai_perception::update_or_add_belief(const ai_belief* other_belief, ai_belief_update_modes mode)
{
    return update_or_add_belief(other_belief, other_belief->get_relevance(), other_belief->flags, mode);
}

// 0x00664980 (:1592)
ai_belief* ai_perception::update_or_add_belief(const ai_belief* other_belief, float relevance, unsigned int flags, ai_belief_update_modes mode)
{
    // Original bug kept: the flags argument is ignored; rfg.exe always passes other_belief->flags on.
    return update_or_add_belief(other_belief->get_type(), other_belief->handle, other_belief->get_obj_type(), other_belief->pos, relevance, other_belief->flags, mode);
}

// 0x00664A40 (:1616)
ai_belief* ai_perception::update_or_add_belief(ai_belief_types type, const object* obj, float relevance, unsigned int flags, ai_belief_update_modes mode)
{
    return update_or_add_belief(type, obj->get_handle(), obj->get_obj_type(), obj->pos, relevance, flags, mode);
}

// 0x00664B10 (:1637)
ai_belief* ai_perception::update_or_add_belief(ai_belief_types type, const object* obj, const vector& pos, float relevance, unsigned int flags, ai_belief_update_modes mode)
{
    return update_or_add_belief(type, obj->get_handle(), obj->get_obj_type(), pos, relevance, flags, mode);
}

// 0x00664BE0 (:1658)
ai_belief* ai_perception::update_or_add_belief(ai_belief_types type, const vector& pos, float relevance, unsigned int flags, ai_belief_update_modes mode)
{
    if (belief_type_is_special(type))
    {
        return update_special_belief(type, 0xFFFFFFFF, OT_UNDEFINED, pos, relevance, flags, mode);
    }
    // No handle to match, so this always adds.
    return add_new_belief_internal(type, 0xFFFFFFFF, OT_UNDEFINED, pos, relevance, flags);
}

// 0x00664CA0 (:1670)
ai_belief* ai_perception::update_or_add_belief(ai_belief_types type, unsigned int handle, object_type obj_type, const vector& pos, float relevance, unsigned int flags, ai_belief_update_modes mode)
{
    if (belief_type_is_special(type))
    {
        return update_special_belief(type, handle, obj_type, pos, relevance, flags, mode);
    }
    ai_belief* belief = handle == 0xFFFFFFFF ? nullptr : find_belief(belief_bins[type].beliefs, handle);
    if (belief != nullptr)
    {
        update_existing_belief_internal(belief, pos, relevance, flags, mode);
        return belief;
    }
    return add_new_belief_internal(type, handle, obj_type, pos, relevance, flags);
}

// 0x00664D60 (:1695)
ai_belief* ai_perception::add_new_belief(const ai_belief* other_belief)
{
    return add_new_belief(other_belief->get_type(), other_belief->handle, other_belief->get_obj_type(), other_belief->pos, other_belief->get_relevance(), other_belief->flags);
}

// 0x00664DC0 (:1717)
ai_belief* ai_perception::add_new_belief(const ai_belief* other_belief, float relevance)
{
    return add_new_belief(other_belief->get_type(), other_belief->handle, other_belief->get_obj_type(), other_belief->pos, relevance, other_belief->flags);
}

// 0x00664E20 (:1739)
ai_belief* ai_perception::add_new_belief(ai_belief_types type, const object* obj, float relevance, unsigned int flags)
{
    return add_new_belief(type, obj->get_handle(), obj->get_obj_type(), obj->pos, relevance, flags);
}

// 0x00664E80 (:1758)
ai_belief* ai_perception::add_new_belief(ai_belief_types type, const object* obj, const vector& pos, float relevance, unsigned int flags)
{
    return add_new_belief(type, obj->get_handle(), obj->get_obj_type(), pos, relevance, flags);
}

// 0x00664EE0 (:1777)
ai_belief* ai_perception::add_new_belief(ai_belief_types type, const vector& pos, float relevance, unsigned int flags)
{
    return add_new_belief(type, 0xFFFFFFFF, OT_UNDEFINED, pos, relevance, flags);
}

// 0x00664FA0 (:1789)
ai_belief* ai_perception::add_new_belief(ai_belief_types type, unsigned int handle, object_type obj_type, const vector& pos, float relevance, unsigned int flags)
{
    if (belief_type_is_special(type))
    {
        return update_special_belief(type, handle, obj_type, pos, relevance, flags, AI_BELIEF_UPDATE_IF_NEWER);
    }
    return add_new_belief_internal(type, handle, obj_type, pos, relevance, flags);
}

// 0x005ED8F0 (:1813)
bool ai_perception::update_existing_belief(ai_belief* belief, unsigned int flags)
{
    belief->flags = (belief->flags & BF_STICKY_MASK) | flags;
    return true;
}

// 0x0063D7F0 (:1820)
bool ai_perception::update_existing_belief(ai_belief* belief, float relevance, unsigned int flags, ai_belief_update_modes mode)
{
    return update_existing_belief_internal(belief, belief->pos, relevance, flags, mode);
}

// 0x0063D820 (:1825)
bool ai_perception::update_existing_belief(ai_belief* belief, const vector& pos, float relevance, unsigned int flags, ai_belief_update_modes mode)
{
    return update_existing_belief_internal(belief, pos, relevance, flags, mode);
}

// 0x0063D850 (:1877)
ai_belief* ai_perception::add_new_belief_internal(ai_belief_types type, unsigned int handle, object_type obj_type, const vector& pos, float relevance, unsigned int flags)
{
    if (np->get_handle() == handle)
    {
        return nullptr;
    }
    if (obj_type == OT_HUMAN)
    {
        if (object_is_local_player(handle) && should_ignore_player())
        {
            return nullptr;
        }
        const human* hp = (const human*)World.object_from_handle(handle);
        if (hp == nullptr || hp->hflags.ai_ignore != 0)
        {
            return nullptr;
        }
    }
    ai_belief* belief = get_free_ai_belief();
    if (belief == nullptr)
    {
        return nullptr;
    }
    belief->set_type(type);
    belief->set_obj_type(obj_type);
    belief->handle = handle;
    belief->flags = flags;
    belief->update_time.set(0);
    belief->pos = pos;
    belief->relevance = relevance;
    if (!insert_belief_into_bin(belief))
    {
        free_ai_belief(belief);
        return nullptr;
    }
    return belief;
}

// 0x005ED910 (:1967)
bool ai_perception::update_existing_belief_internal(ai_belief* belief, const vector& pos, float relevance, unsigned int flags, ai_belief_update_modes mode)
{
    const ai_belief_types type = belief->get_type();
    if (mode == AI_BELIEF_UPDATE_IF_MORE_RELEVANT)
    {
        if (belief->relevance <= relevance)
        {
            return false;
        }
    }
    else if (mode == AI_BELIEF_UPDATE_IF_NEWER && belief->update_time.time_since() < BELIEF_MIN_UPDATE_MS)
    {
        return false;
    }
    unlink_belief(belief_bins[type].beliefs, belief);
    --belief_bins[type].count;
    belief->flags = (belief->flags & BF_STICKY_MASK) | flags;
    belief->update_time.set(0);
    belief->pos = pos;
    belief->relevance = relevance;
    insert_belief_into_bin(belief);
    return true;
}

// 0x005ED9F0 (:2042)
ai_belief* ai_perception::update_special_belief(ai_belief_types type, unsigned int handle, object_type obj_type, const vector& pos, float relevance, unsigned int flags, ai_belief_update_modes mode)
{
    ai_belief_bin& bin = belief_bins[type];
    if (bin.count < 2)
    {
        if (bin.count == 0)
        {
            ai_belief* belief = get_free_ai_belief();
            if (belief == nullptr)
            {
                return nullptr;
            }
            belief->flags = flags;
            belief->set_type(type);
            belief->set_obj_type(obj_type);
            belief->handle = handle;
            belief->update_time.set(0);
            belief->pos = pos;
            belief->relevance = relevance;
            append_belief(bin.beliefs, belief);
            bin.count = 1;
            return belief;
        }
    }
    else
    {
        // A special belief is single: rfg.exe reports the corruption, keeps the first and frees the rest.
        ai_belief* first = bin.beliefs;
        unlink_belief(bin.beliefs, first);
        free_ai_belief_list(bin.beliefs);
        append_belief(bin.beliefs, first);
        bin.count = 1;
    }

    ai_belief* belief = bin.beliefs;
    if (belief == nullptr)
    {
        return nullptr;
    }
    if (mode == AI_BELIEF_UPDATE_IF_MORE_RELEVANT)
    {
        if (relevance >= belief->relevance)
        {
            return nullptr;
        }
    }
    else if (mode == AI_BELIEF_UPDATE_IF_NEWER && belief->update_time.time_since() < BELIEF_MIN_UPDATE_MS)
    {
        return nullptr;
    }
    belief->pos = pos;
    belief->handle = handle;
    belief->relevance = relevance;
    belief->set_obj_type(obj_type);
    belief->update_time.set(0);
    // Original bug kept: handle was just stored, so this only tells a real handle from the invalid one and the sticky
    // flags stay for any object, not only for the same one.
    if (handle != 0xFFFFFFFF && handle == belief->handle)
    {
        belief->flags = (belief->flags & BF_STICKY_MASK) | flags;
    }
    else
    {
        belief->flags = flags;
    }
    return belief;
}

// ---- relevance

// 0x005EDC80 (:2139). LTCG: distance in XMM0, nearby_cutoff in XMM3, outer_cutoff on the stack.
static float calcuate_simple_relevance_sq(float distance_sq, float nearby_cutoff, float outer_cutoff)
{
    // Original typo kept in the name. Inside the nearby cutoff 0..10, beyond it 10.. upward; distance_sq is really a
    // distance here, the squares are taken below.
    const float dist_sq = (float)((double)distance_sq * (double)distance_sq);
    if ((double)dist_sq >= (double)nearby_cutoff)
    {
        return (float)((double)nearby_cutoff * 10.0 / (double)dist_sq);
    }
    const float outer_sq = (float)((double)outer_cutoff * (double)outer_cutoff);
    const float range = (float)((double)outer_sq - (double)dist_sq);
    if (range == 0.0f)
    {
        return 0.0f;
    }
    return (float)(((double)nearby_cutoff - (double)dist_sq) * 10.0 / (double)range + 10.0);
}

// 0x005EDD20 (:2164). LTCG: distance in XMM1, nearby_cutoff in XMM0, outer_cutoff on the stack.
static float calculate_simple_relevance(float distance, float nearby_cutoff, float outer_cutoff)
{
    if ((double)nearby_cutoff >= (double)distance)
    {
        return (float)((double)distance * 10.0 / (double)nearby_cutoff);
    }
    return (float)(((double)distance - (double)nearby_cutoff) * 10.0 / ((double)outer_cutoff - (double)nearby_cutoff) + 10.0);
}

// 0x005EDD90 (:2178)
static float get_human_belief_relevance(float distance)
{
    return (float)((double)distance * 10.0 / (double)Ai_fov_length_low);
}

// 0x005EDDB0 (:2183)
float ai_perception::get_human_belief_relevance(const human* hp) const
{
    return (float)((double)vec_dist(np->pos, hp->pos) * 10.0 / (double)Ai_fov_length_low);
}

// 0x0063D920 (:2194)
unsigned int ai_perception::get_human_belief_flags(const human* hp) const
{
    // Original bug kept: hp is unused; any acknowledged enemy flags every human.
    if (belief_bins[AI_BELIEF_ENEMY_POSITION].count > 0)
    {
        for (const ai_belief* belief = get_first_belief(AI_BELIEF_ENEMY_EXISTS); belief != nullptr; belief = get_next_belief(belief))
        {
            if (BELIEF_REQUIREMENTS_ACKNOWLEDGED.test(belief->flags))
            {
                return BF_ACKNOWLEDGED;
            }
        }
    }
    return 0;
}

// 0x006806E0 (:2221)
ai_belief* ai_perception::add_hazard(const object* obj, const hazard_physics_info& haz_physics)
{
    return add_hazard(obj, haz_physics.distance_at_pass * haz_physics.time_to_pass);
}

// 0x00665000 (:2225)
ai_belief* ai_perception::add_hazard(const object* obj, float relevance)
{
    const ai_belief* first = belief_bins[AI_BELIEF_HAZARD].beliefs;
    if (first == nullptr || relevance < first->relevance - 1.0f)
    {
        ai->planner.request_replan();
    }
    return update_or_add_belief(AI_BELIEF_HAZARD, obj, relevance, obj->get_sub_type() == OT_SUB_ITEM_PROJECTILE ? BF_PROJECTILE_THREAT : 0, AI_BELIEF_UPDATE_OVERWRITE);
}

// ---- field of view

// 0x00665090 (:2247)
bool ai_perception::should_do_fov_check(const human* target) const
{
    if (target->get_handle() == attention.target && (attention.mode == ATTENTION_AIM_AT_OBJECT || attention.mode == ATTENTION_LOOK_AT_OBJECT))
    {
        return false;
    }
    const vehicle* vp = (vehicle*)typed_object_from_handle(target->vehicle_handle, OT_VEHICLE);
    if (vp == nullptr)
    {
        return true;
    }
    // Walkers and tanks are too big to miss.
    return vp->get_sub_type() != OT_SUB_VEHICLE_WALKER && !vehicle_is_tank(vp);
}

// 0x006650F0 (:2272)
bool ai_perception::target_in_fov(const human* target, const temporary_eye_pos_orient eye_pos_orient) const
{
    const bool target_is_local_player = object_is_local_player(target);
    bool focused = false;
    if (attention.awareness == ATTENTION_AWARENESS_FOCUSED)
    {
        focused = true;
    }
    else if (attention.awareness == ATTENTION_AWARENESS_OBLIVIOUS)
    {
        return false;
    }

    view_frustum_parameters params;
    get_view_frustum_params(params, target);
    params.length *= get_fov_length_bonus(np, target);
    if (focused)
    {
        params.length *= 0.2f;
        params.h_angle *= 0.4f;
        params.v_angle *= 0.4f;
    }
    // A crouched player out of a vehicle is harder to see while nobody is alerted.
    if (target_is_local_player && human_in_vehicle(target) == nullptr
        && ((int)human_teams_get_disposition(np, HUMAN_TEAM_EDF) < 3 || (int)gsm_get_alert_level() < 1)
        && !activity_on_alert(np)
        && (target->cur_stance == HST_CROUCH || target->cur_state == HS_COVER))
    {
        params.length *= 0.5f;
        params.h_angle *= 0.6666667f;
    }

    vector head_pos;
    human_get_head_pos(target, head_pos);
    if (get_belief(AI_BELIEF_SHOULD_ENGAGE_ENEMY, target->get_handle()) != nullptr)
    {
        params.length += 75.0f;
    }

    const float dist_sq = vec_dist_squared(eye_pos_orient.eff_eye_pos, head_pos);
    if ((double)dist_sq > (double)(float)((double)params.length * (double)params.length))
    {
        return false;
    }
    if (!should_do_fov_check(target))
    {
        return true;
    }

    // A head about 0.5 m wide widens the angles by atan(0.5 / d), taken as 0.5 / d.
    const float margin = (float)(0.5 / (double)(float)sqrt((double)dist_sq));
    vector local_pos = head_pos;
    vec_change_to_local_coords(local_pos, eye_pos_orient.eff_eye_pos, eye_pos_orient.eff_eye_orient);
    float angle = (float)fabs((double)(float)atan2((double)local_pos.x, (double)local_pos.z)) - margin;
    if (angle > (float)((double)params.h_angle * 0.5))
    {
        return false;
    }
    angle = (float)fabs((double)(float)atan2((double)local_pos.y, (double)local_pos.z)) - margin;
    return angle <= (float)((double)params.v_angle * 0.5);
}

// 0x0063D990 (:2406)
void ai_perception::get_view_frustum_params(view_frustum_parameters& params, const human* target) const
{
    bool spotted = false;
    bool high = false;
    bool alerted = (int)gsm_get_alert_level() > 1;
    if (target != nullptr)
    {
        const ai_belief* exists = get_belief(AI_BELIEF_ENEMY_EXISTS, target->get_handle());
        spotted = exists != nullptr && exists->get_relevance() <= SIGHTED_ENEMY_RELEVANCE;
        if (!alerted && object_is_player(target) && human_teams_get_disposition(np, HUMAN_TEAM_EDF) == TEAM_DISPOSITION_ALLIED)
        {
            alerted = restricted_area_in_any_region(target->pos, false);
        }
        high = alerted && (target->get_sub_type() == OT_SUB_HUMAN_PLAYER || spotted);
    }
    params.initialized = true;
    params.use_fov = true;
    if (spotted)
    {
        params.length = high ? Ai_fov_length_high_spotted : Ai_fov_length_low_spotted;
        params.h_angle = (float)((double)Ai_fov_width_spotted * FOV_DEG_TO_RAD);
        params.v_angle = (float)((double)Ai_fov_height_spotted * FOV_DEG_TO_RAD);
    }
    else
    {
        params.length = high ? Ai_fov_length_high : Ai_fov_length_low;
        params.h_angle = (float)((double)Ai_fov_width * FOV_DEG_TO_RAD);
        params.v_angle = (float)((double)Ai_fov_height * FOV_DEG_TO_RAD);
    }
}

// 0x005EDDF0 (:2445)
bool ai_perception::within_view_frustum(const vector& target_pos, const view_frustum_parameters& params) const
{
    vector eye_pos;
    matrix eye_orient;
    human_get_eye_pos_orient(np, eye_pos, &eye_orient);
    vector delta;
    delta.x = target_pos.x - eye_pos.x;
    delta.y = target_pos.y - eye_pos.y;
    delta.z = target_pos.z - eye_pos.z;
    const float dist_sq = delta.x * delta.x + delta.y * delta.y + delta.z * delta.z;
    if (dist_sq == 0.0f || params.length * params.length < dist_sq)
    {
        return false;
    }
    if (!params.use_fov || human_in_vehicle(np) != nullptr)
    {
        return true;
    }
    delta.rotate(eye_orient);
    if ((float)fabs((double)(float)atan2((double)delta.x, (double)delta.z)) > (float)((double)params.h_angle * 0.5))
    {
        return false;
    }
    return (float)fabs((double)(float)atan2((double)delta.y, (double)delta.z)) <= (float)((double)params.v_angle * 0.5);
}

// 0x006A5710 (:2486)
bool ai_perception::within_view_frustum(const object* target, bool do_raycast) const
{
    if (target == nullptr)
    {
        return false;
    }
    vector target_pos = target->pos;
    view_frustum_parameters params;
    get_view_frustum_params(params, nullptr);
    if (target->get_sub_type() == OT_SUB_ITEM_PROJECTILE)
    {
        // Having heard its thrower throw it widens the view.
        const ai_belief* heard = get_belief(AI_BELIEF_COMBAT_SOUND, ((const item*)target)->get_owner_handle());
        if (heard != nullptr && (heard->flags & BF_PROJECTILE_THREAT) != 0)
        {
            params.length = 40.0f;
            params.h_angle += 0.69812f; // 40 degrees
            params.v_angle += 0.69812f;
        }
    }
    else if (target->get_obj_type() == OT_HUMAN && !target->is_killed())
    {
        human_get_head_pos((const human*)target, target_pos);
    }
    return within_view_frustum(target_pos, params) && (!do_raycast || has_los_to_target(target));
}

// 0x005EDF80 (:2534)
bool ai_perception::check_ally_against_line_of_fire(const human* ally, float dist_sq) const
{
    if (temp_max_ally_fire_dist_sq < dist_sq)
    {
        return false;
    }
    vector delta;
    delta.x = ally->pos.x - np->pos.x;
    delta.y = ally->pos.y - np->pos.y;
    delta.z = ally->pos.z - np->pos.z;
    const float along = temp_target_dir.x * delta.x + temp_target_dir.y * delta.y + temp_target_dir.z * delta.z;
    if (along < 0.5f)
    {
        return false;
    }
    delta.x -= temp_target_dir.x * along;
    delta.y -= temp_target_dir.y * along;
    delta.z -= temp_target_dir.z * along;
    return delta.y <= 0.5f && delta.y * delta.y + delta.x * delta.x + delta.z * delta.z <= IN_LINE_OF_FIRE_DIST_SQ;
}

// 0x005EE100 (:2559)
void ai_perception::register_in_ally_line_of_fire()
{
    in_ally_line_of_fire_time.set(LINE_OF_FIRE_MS);
}

// 0x005EE110 (:2564)
void ai_perception::register_ally_in_line_of_fire()
{
    ally_in_line_of_fire_time.set(LINE_OF_FIRE_MS);
}

// ---- target tracking

// 0x0064CA20 (.h:582)
ai_perception::target_tracking_info::target_tracking_info()
{
}

// 0x0063E3B0
ai_perception::target_tracking_info::~target_tracking_info()
{
}

// 0x005EE120
ai_perception::target_tracking_point::target_tracking_point()
{
}

// 0x0063DAF0 (:2573)
void ai_perception::target_tracking_info::clear()
{
    history.remove_all();
    last_target = 0xFFFFFFFF;
}

// 0x0063DB10 (:2586)
void ai_perception::target_tracking_update_target(const human* target, bool visible)
{
    const unsigned int handle = target == nullptr ? 0xFFFFFFFF : target->get_handle();
    if (target_tracking.last_target != handle)
    {
        target_tracking.clear();
        target_tracking.last_target = handle;
    }
    if (handle == 0xFFFFFFFF || human_in_vehicle(target) != nullptr)
    {
        return;
    }

    int since_last = -1;
    if (target_tracking.history.size() > 0)
    {
        since_last = target_tracking.history[0].time.time_since();
        if (since_last < TARGET_TRACKING_INTERVAL_MS)
        {
            return;
        }
        if (since_last > TARGET_TRACKING_MAX_GAP_MS)
        {
            target_tracking.history.remove_all();
            since_last = -1;
        }
    }

    target_tracking_point point;
    point.pos = target->pos;
    point.time.invalidate();
    point.time.set(0);
    if (visible)
    {
        if (since_last < 1)
        {
            point.vel = target->actual_velocity;
        }
        else
        {
            const target_tracking_point& last = target_tracking.history[0];
            const float seconds = (float)since_last / 1000.0f;
            point.vel.x = (point.pos.x - last.pos.x) / seconds * 0.6f + target->actual_velocity.x * 0.4f;
            point.vel.y = (point.pos.y - last.pos.y) / seconds * 0.6f + target->actual_velocity.y * 0.4f;
            point.vel.z = (point.pos.z - last.pos.z) / seconds * 0.6f + target->actual_velocity.z * 0.4f;
        }
    }
    else if (since_last < 1)
    {
        point.vel = ZERO_VECTOR;
    }
    else
    {
        const target_tracking_point& last = target_tracking.history[0];
        const float seconds = (float)since_last / 1000.0f;
        point.vel.x = (point.pos.x - last.pos.x) / seconds;
        point.vel.y = (point.pos.y - last.pos.y) / seconds;
        point.vel.z = (point.pos.z - last.pos.z) / seconds;
    }
    target_tracking.history.add(point);
}

// 0x006653D0 (:2677)
bool ai_perception::target_tracking_get_current_velocity(vector& velocity, const human* target, bool cheat_if_out_of_sight) const
{
    if (target == nullptr)
    {
        return false;
    }
    const unsigned int handle = target->get_handle();
    if (enemy_in_los(handle))
    {
        velocity = target->actual_velocity;
        return true;
    }
    target_tracking_info& tracking = const_cast<target_tracking_info&>(target_tracking);
    if (handle == 0xFFFFFFFF || handle != tracking.last_target || tracking.history.size() < 1)
    {
        return false;
    }
    const target_tracking_point& point = tracking.history[0];
    const int since = point.time.time_since();
    if (since > TARGET_TRACKING_MAX_GAP_MS)
    {
        return false;
    }
    if (cheat_if_out_of_sight && since > 0)
    {
        const float seconds = (float)since / 1000.0f;
        velocity.x = point.vel.x * 0.5f + (target->pos.x - point.pos.x) / seconds * 0.5f;
        velocity.y = point.vel.y * 0.5f + (target->pos.y - point.pos.y) / seconds * 0.5f;
        velocity.z = point.vel.z * 0.5f + (target->pos.z - point.pos.z) / seconds * 0.5f;
        return true;
    }
    velocity = point.vel;
    return true;
}

// 0x00665610 (:2728)
bool ai_perception::target_tracking_get_aggregate_velocity(vector& velocity, const human* target, float seconds) const
{
    if (target == nullptr)
    {
        return false;
    }
    const unsigned int handle = target->get_handle();
    target_tracking_info& tracking = const_cast<target_tracking_info&>(target_tracking);
    if (handle == 0xFFFFFFFF || handle != tracking.last_target || tracking.history.size() < 1 || !enemy_in_los(handle))
    {
        return false;
    }
    const int window_ms = (int)(seconds * 1000.0f);
    if (window_ms < 1)
    {
        return false;
    }
    for (int i = 0; i < tracking.history.size(); ++i)
    {
        const target_tracking_point& point = tracking.history[i];
        const int since = point.time.time_since();
        if (window_ms < since)
        {
            const float elapsed = (float)since / 1000.0f;
            velocity.x = (target->pos.x - point.pos.x) / elapsed;
            velocity.y = (target->pos.y - point.pos.y) / elapsed;
            velocity.z = (target->pos.z - point.pos.z) / elapsed;
            return true;
        }
    }
    return false;
}

// 0x005EE130 (:2777)
bool target_tracking_obstruction_callback(object* hit_objp, void* cb_data)
{
    const object_type type = hit_objp->get_obj_type();
    return !(type == OT_HUMAN || type == OT_ITEM || type == OT_ZONE);
}

// 0x00680710 (:2793)
bool ai_perception::target_tracking_extrapolate(vector& pos, vector& velocity, const human* target, float lookahead, bool check_obstructions) const
{
    if (!target_tracking_get_current_velocity(velocity, target, true))
    {
        return false;
    }
    target_tracking_info& tracking = const_cast<target_tracking_info&>(target_tracking);
    const target_tracking_point& point = tracking.history[0];
    float t = (float)point.time.time_since() / 1000.0f + lookahead;
    if (0.0f <= 0.001f - t)
    {
        t = 0.001f;
    }
    pos.x = point.pos.x + velocity.x * t;
    pos.y = point.pos.y + velocity.y * t;
    pos.z = velocity.z * t + point.pos.z;
    if (!check_obstructions)
    {
        return true;
    }

    float terrain_y;
    havok_terrain_height(terrain_y, point.pos.x, point.pos.z, false, nullptr);
    nav_cell* ncp = bb_pf_get_nav_cell(point.pos, false, nullptr, false, NAV_CELL_TRAVERSAL_ENABLED, nullptr);
    if (point.pos.y - terrain_y < 1.0f && ncp != nullptr)
    {
        nav_cell_longest_los_input input;
        nav_cell_longest_los_output output;
        output.result = NAV_CELL_LONGEST_LOS_RESULT_NONE;
        output.end_pos.zero();
        input.flags.check_start_in_detour_hull = 0;
        input.flags.ignore_detour_hulls = 0;
        input.flags.use_disabled_nav_cells = 0;
        output.length = 0.0f;
        memset(&output.obstruction, 0, sizeof(output.obstruction));
        input.start_pos = point.pos;
        input.start_nav_cell = ncp;
        input.delta_2d.x = velocity.x * t;
        input.delta_2d.y = velocity.z * t;
        output.end_nav_cell = nullptr;
        output.num_visited_nav_cells = 0;
        output.visited_nav_cells = nullptr;
        nav_cell_find_longest_line_of_sight_point(output, input);
        if (output.result != NAV_CELL_LONGEST_LOS_RESULT_REACHED_DESTINATION)
        {
            pos = output.end_pos;
            velocity = ZERO_VECTOR;
        }
        float ground;
        havok_terrain_height(ground, pos.x, pos.z, false, nullptr);
        pos.y = ground;
        return true;
    }

    raycast_input input;
    input.start = point.pos;
    input.start.y += 0.8f;
    input.end = pos;
    input.end.y += 0.8f;
    input.radius = 0.0f;
    input.m_hit_collector_type = raycast_input::COLLECT_CLOSEST_HIT;
    input.m_flags = 0;
    input.m_filter_info = COLLIDE_PHYSICAL_OBJECT;
    input.filter_callback = target_tracking_obstruction_callback;
    input.filter_params = nullptr;
    raycast_output output;
    if (havok_raycast(input, output))
    {
        pos = output.m_hits[0].hit_pos;
        pos.y -= 0.8f;
        velocity = ZERO_VECTOR;
    }
    float ground;
    havok_terrain_height(ground, pos.x, pos.z, false, nullptr);
    if (0.0f <= ground - pos.y)
    {
        pos.y = ground;
    }
    return true;
}

// 0x006657B0 (:2903)
bool ai_perception::treat_as_undercover(const human* target) const
{
    if (!target->is_undercover()
        || ((int)human_teams_get_disposition(np, HUMAN_TEAM_EDF) > 2 && (int)gsm_get_alert_level() > 0)
        || activity_on_alert(np))
    {
        return false;
    }
    if (get_belief(AI_BELIEF_ENEMY_EXISTS, target->get_handle()) != nullptr)
    {
        return false;
    }
    if (target->get_obj_type() == OT_HUMAN && target->get_sub_type() == OT_SUB_HUMAN_NPC)
    {
        // An npc on the attack near this one gives the player's disguise away.
        const ai_plan_conditions conditions = ai_plan_conditions(UNDERCOVER_BLOWN_CONDITION_A) | ai_plan_conditions(UNDERCOVER_BLOWN_CONDITION_B);
        const ai_goal* goal = ((const npc*)target)->get_ai()->planner.get_current_goal();
        if (goal != nullptr && goal->pursues_condition(conditions) && vec_dist_squared(np->pos, target->pos) < 900.0f)
        {
            return false;
        }
    }
    if (np->npc_flags.roadblock_guard != 0 && human_in_vehicle(target) != nullptr)
    {
        const vehicle* vp = World.object_from_handle<vehicle>(target->vehicle_handle);
        view_frustum_parameters params;
        get_view_frustum_params(params, target);
        if (vp != nullptr)
        {
            // A roadblock guard stops a vehicle coming straight at it.
            const float reach = params.length * 2.5f;
            if ((double)vec_dist_squared(vp->pos, np->pos) < (double)reach * (double)reach)
            {
                const vector start = vp->pos;
                const vector flat_fvec = vp->orient.fvec.copy_flatten_normalize_safe();
                vector end;
                end.x = start.x + flat_fvec.x * reach;
                end.y = start.y + flat_fvec.y * reach;
                end.z = flat_fvec.z * reach + start.z;
                vector closest;
                const float along = ix_closest_point_on_line_segment(closest, np->pos, start, end);
                if (vec_xz_dist_squared(np->pos, closest) <= 400.0f)
                {
                    float speed;
                    vehicle_get_speed(vp, speed);
                    if (along <= (speed + 5.0f) * 5.0f)
                    {
                        return false;
                    }
                }
            }
        }
    }
    return true;
}

// 0x0063DEB0 (:2970). LTCG: hp in EAX.
static bool is_human_aggressive(const human* hp)
{
    if (hp == nullptr || hp->get_obj_type() != OT_HUMAN || hp->get_sub_type() != OT_SUB_HUMAN_NPC)
    {
        return false;
    }
    return ((const npc*)hp)->get_ai()->has_hostile_contact();
}

// 0x00665AF0 (:2994)
human_team_dispositions ai_perception::get_effective_disposition(const human* target, bool check_undercover) const
{
    if (object_is_local_player(target) && human_teams_get_disposition(target, np) == TEAM_DISPOSITION_ALLIED && player_friendly_fire.is_hostile())
    {
        return TEAM_DISPOSITION_HOSTILE;
    }
    const human_team_dispositions disposition = human_teams_get_disposition(np, target);
    if (disposition == TEAM_DISPOSITION_HOSTILE && target->hflags.ambient_edf != 0 && target->vehicle_handle != 0xFFFFFFFF && !is_human_aggressive(target))
    {
        return TEAM_DISPOSITION_UNFRIENDLY;
    }
    if (check_undercover)
    {
        const human_team_dispositions undercover = human_teams_get_disposition(np, target->get_undercover_team());
        if ((int)disposition < (int)undercover && treat_as_undercover(target))
        {
            return undercover;
        }
    }
    return disposition;
}

// 0x0063DEE0 (:3045)
int ai_perception::remaining_enemy_search_time(const ai_belief* enemy_exists_belief) const
{
    if (enemy_exists_belief == nullptr)
    {
        return 0;
    }
    const float relevance = enemy_exists_belief->get_relevance();
    float minutes = 0.0f;
    for (int i = 0; i < 6; ++i)
    {
        if (relevance <= Cover_blown_time_table[i][0])
        {
            if (i == 0)
            {
                minutes = Cover_blown_time_table[0][1];
            }
            else
            {
                minutes = fl_parameter(relevance, Cover_blown_time_table[i][0], Cover_blown_time_table[i - 1][0], Cover_blown_time_table[i][1], Cover_blown_time_table[i - 1][1], true);
            }
            break;
        }
    }
    const int remaining = -(int)(minutes * -60.0f * 1000.0f) - enemy_exists_belief->update_time.time_since();
    return remaining < 0 ? 0 : remaining;
}

// 0x0063DFD0 (:3079)
int ai_perception::remaining_enemy_search_time(unsigned int enemy_handle) const
{
    return remaining_enemy_search_time(get_belief(AI_BELIEF_ENEMY_EXISTS, enemy_handle));
}

// 0x005EE2F0
ai_perception::temporary_eye_pos_orient::temporary_eye_pos_orient()
{
}

// 0x005EE300
ai_perception::temporary_eye_pos_orient::temporary_eye_pos_orient(const temporary_eye_pos_orient& __that)
    : eff_eye_pos(__that.eff_eye_pos), eff_eye_orient(__that.eff_eye_orient)
{
}

// 0x005EE150 (:3091)
void ai_perception::get_temporary_eye_pos_orient(temporary_eye_pos_orient& eye_pos_orient) const
{
    human_get_eye_pos_orient(np, eye_pos_orient.eff_eye_pos, &eye_pos_orient.eff_eye_orient);
    if (attention.mode != ATTENTION_LOOK_AT_NONE)
    {
        const vector uvec = eye_pos_orient.eff_eye_orient.uvec;
        vector look_at;
        human_get_look_at_vector_from_eye_pos(np, eye_pos_orient.eff_eye_pos, look_at);
        eye_pos_orient.eff_eye_orient.make_uvec(look_at, uvec);
    }
    // Keep the eye from sitting ahead of the npc's position along its facing.
    const vector& fvec = np->orient.fvec;
    vector& eye = eye_pos_orient.eff_eye_pos;
    const float ahead = fvec.y * (eye.y - np->pos.y) + fvec.x * (eye.x - np->pos.x) + fvec.z * (eye.z - np->pos.z);
    if (ahead > 0.0f)
    {
        eye.x -= fvec.x * ahead;
        eye.y -= fvec.y * ahead;
        eye.z -= fvec.z * ahead;
    }
}


// ---- noticing

// 0x006C2940 (:3114)
void ai_perception::notice_humans()
{
    bool update_friendlies = false;
    if (notice_friendlies_time.next.elapsed())
    {
        clear_beliefs(AI_BELIEF_CLOSEST_ALLY);
        clear_beliefs(AI_BELIEF_ALLY_ON_TURRET);
        clear_beliefs(AI_BELIEF_CLOSEST_FRIENDLY);
        update_friendlies = true;
        notice_friendlies_time.last.set(0);
        notice_friendlies_time.next.set(((ai->flags.reduced_processing ? 1 : 0) + 1) * 1000);

        // The line of fire check of notice_ally is against the target this npc is ready to shoot at.
        const timestamp& combat_ready = np->combat.combat_ready_timestamp;
        if (!combat_ready.valid() || combat_ready.elapsed()
            || (attention.mode != ATTENTION_LOOK_AT_OBJECT && attention.mode != ATTENTION_AIM_AT_OBJECT && attention.mode != ATTENTION_ACQUIRE_VISUAL))
        {
            temp_target = nullptr;
        }
        else
        {
            temp_target = World.object_from_handle(attention.target);
            if (temp_target != nullptr)
            {
                temp_target_dist = vec_normalized_dir(temp_target_dir, temp_target->pos, np->pos);
                float max_dist = temp_target_dist * 0.6f;
                if (0.0f <= max_dist - (temp_target_dist - 1.5f))
                {
                    max_dist = temp_target_dist - 1.5f;
                }
                temp_max_ally_fire_dist_sq = max_dist;
                if (max_dist < 0.5f)
                {
                    temp_target = nullptr;
                }
                temp_max_ally_fire_dist_sq = max_dist * max_dist;
            }
        }
    }

    bool consider_unfriendlies;
    if (human_team_is_non_combatant(np))
    {
        consider_unfriendlies = ai_get_flee_to_safety_belief(np) == nullptr;
    }
    else
    {
        consider_unfriendlies = belief_bins[AI_BELIEF_ENEMY_POSITION].beliefs == nullptr || ai->planner.get_current_action() == terrorize_action::get_instance();
    }

    if (!np->char_inst->rdd.is_loaded())
    {
        return;
    }
    const player* local_player = player_get_local_player();
    temporary_eye_pos_orient eye_pos_orient;
    get_temporary_eye_pos_orient(eye_pos_orient);

    const int num_humans = World.get_num_objects<human>();
    for (int i = 0; i < num_humans; ++i)
    {
        human* target = World.get_object<human>(i);
        if (target == np || !target->char_inst->rdd.is_loaded() || target->is_killed())
        {
            continue;
        }
        if (target->get_sub_type() == OT_SUB_HUMAN_NPC && ((npc*)target)->npc_flags.tied_up != 0)
        {
            continue;
        }
        const human_team_dispositions disposition = get_effective_disposition(target, true);
        if (disposition == TEAM_DISPOSITION_HOSTILE || target->get_handle() == np->combat.target_handle)
        {
            notice_enemy(target, eye_pos_orient);
        }
        else if (update_friendlies || target == local_player)
        {
            if (disposition == TEAM_DISPOSITION_ALLIED)
            {
                notice_ally(target);
            }
            else if (disposition == TEAM_DISPOSITION_FRIENDLY)
            {
                notice_friendly(target);
            }
            else if (disposition != TEAM_DISPOSITION_NONE && (consider_unfriendlies || target == local_player))
            {
                notice_unfriendly(target, eye_pos_orient);
            }
        }
    }

    const gfm_orders* orders = ai->get_orders();
    if (orders != nullptr && orders->get_orders_type() == GFM_ORDERS_TYPE_ATTACK)
    {
        // Attack orders tell the npc where its target is, refreshed every 20 s.
        const human* order_target = World.object_cast<human>(orders->get_target_object());
        if (!human_is_dead(order_target))
        {
            ai_belief* belief = get_belief(AI_BELIEF_ENEMY_EXISTS, order_target);
            if (belief == nullptr || ATTACK_ORDERS_RELEVANCE <= belief->get_relevance())
            {
                update_or_add_belief(AI_BELIEF_ENEMY_EXISTS, order_target, orders->get_orders_pos(), ATTACK_ORDERS_RELEVANCE, get_human_belief_flags(order_target) | BF_FROM_ORDERS, AI_BELIEF_UPDATE_OVERWRITE);
            }
            else if (belief->update_time.time_since() >= 20000)
            {
                update_existing_belief_internal(belief, orders->get_orders_pos(), belief->get_relevance(), belief->flags, AI_BELIEF_UPDATE_OVERWRITE);
            }
        }
    }

    ai_belief* beatdown = belief_bins[AI_BELIEF_BEATDOWN_TARGET].beliefs;
    if (beatdown != nullptr)
    {
        const unsigned int handle = beatdown->handle;
        if (get_belief(AI_BELIEF_UNFRIENDLY, handle) == nullptr)
        {
            deprecate_belief(beatdown);
        }
        else
        {
            const human* hp = World.object_from_handle<human>(handle);
            if (hp == nullptr || hp->is_killed())
            {
                deprecate_belief(beatdown);
            }
        }
    }
}

// 0x006C2DC0 (:3264)
void ai_perception::notice_corpses()
{
    const int num_humans = World.get_num_objects<human>();
    for (int i = 0; i < num_humans; ++i)
    {
        human* hp = World.get_object<human>(i);
        if (hp == nullptr || hp->is_killed())
        {
            notice_corpse(hp);
        }
    }
}

// 0x0063E000 (:3279)
bool collect_movers_callback(object* hit_objp, unsigned int shape_key, const void* user_data)
{
    if (hit_objp != nullptr && hit_objp->get_obj_type() == OT_MOVER && hit_objp->get_sub_type() == OT_SUB_MOVER_RFG && ((rfg_mover*)hit_objp)->get_move_type() == RFGM_FIXED)
    {
        return false;
    }
    if (hit_objp->havok_handle == 0xFFFFFFFF || !havok_body_is_active(hit_objp->havok_handle))
    {
        return false;
    }
    if ((double)hit_objp->get_mass() < DANGEROUS_MOVER_MIN_MASS)
    {
        return false;
    }
    vector velocity;
    havok_body_get_linear_velocity(hit_objp->havok_handle, velocity);
    return 4.0f <= velocity.x * velocity.x + velocity.y * velocity.y + velocity.z * velocity.z;
}

// 0x006C2E30 (:3308)
void ai_perception::notice_projectiles()
{
    // The beliefs are rebuilt, so remember what the last pass knew.
    temporary_projectile_record old_projectiles[15];
    int num_old_projectiles = 0;
    for (const ai_belief* belief = get_first_belief(AI_BELIEF_PROJECTILE); belief != nullptr; belief = get_next_belief(belief))
    {
        object* objp = typed_object_from_handle(belief->handle, OT_ITEM);
        if (objp != nullptr && objp->get_sub_type() == OT_SUB_ITEM_PROJECTILE)
        {
            old_projectiles[num_old_projectiles].proj = (projectile*)objp;
            old_projectiles[num_old_projectiles].flags = belief->flags;
            ++num_old_projectiles;
        }
    }
    clear_beliefs(AI_BELIEF_PROJECTILE);

    const int num_projectiles = World.get_num_objects<projectile>();
    for (int i = 0; i < num_projectiles; ++i)
    {
        projectile* proj = World.get_object<projectile>(i);
        if (!notice_projectile(proj, old_projectiles, num_old_projectiles))
        {
            ai_belief* hazard = get_belief(AI_BELIEF_HAZARD, proj->get_handle());
            if (hazard != nullptr)
            {
                deprecate_belief(hazard);
            }
        }
    }
}

// 0x006B8100 (:3338)
void ai_perception::notice_objects()
{
    const int num_turrets = World.get_num_objects<turret>();
    for (int i = 0; i < num_turrets; ++i)
    {
        notice_turret(World.get_object<turret>(i));
    }
    const int num_vehicles = World.get_num_objects<vehicle>();
    for (int i = 0; i < num_vehicles; ++i)
    {
        notice_vehicle(World.get_object<vehicle>(i));
    }
}

// 0x0063E0B0 (:3375)
void ai_perception::flag_unsafe_building(const object* mover)
{
    ai_belief* belief = get_belief(AI_BELIEF_BUILDING, mover->get_handle());
    if (belief != nullptr)
    {
        belief->flags |= BF_UNSAFE;
        ai->planner.request_replan();
    }
}

// 0x005EE330 (:3397)
bool collect_buildings_callback(object* hit_objp, unsigned int shape_key, const void* data)
{
    const rfg_mover* mover = (const rfg_mover*)hit_objp;
    if (mover->flags.fully_destroyed != 0 || mover->flags.is_ai_structure == 0)
    {
        return false;
    }
    if (mover->flags.is_ai_building != 0)
    {
        return true;
    }
    // Other structures only count within 20 m of the npc.
    return vec_dist_squared(((const npc*)data)->pos, hit_objp->pos) <= 400.0f;
}

// 0x00680B60 (:3416)
bool ai_perception::notice_buildings()
{
    unsigned int buildings[20];
    int num_buildings = 0;
    if (vec_dist_squared(np->pos, notice_buildings_pos) >= 64.0f || notice_building_timeout.elapsed())
    {
        if (!Ai_collect_buildings_rate_limiter.request())
        {
            return false;
        }
        notice_building_timeout.set_rand(5000, 10000);
        notice_buildings_pos = np->pos;
        num_buildings = World.collect_objects<rfg_mover>(np->pos, 40.0f, buildings, 20, collect_buildings_callback, np);
    }
    else
    {
        // Not far from the last collection: rescore what it found.
        for (const ai_belief* belief = get_first_belief(AI_BELIEF_BUILDING); belief != nullptr; belief = get_next_belief(belief))
        {
            buildings[num_buildings] = belief->handle;
            ++num_buildings;
        }
    }
    clear_beliefs(AI_BELIEF_BUILDING);
    for (int i = 0; i < num_buildings; ++i)
    {
        const rfg_mover* building = rfg_mover_from_handle(buildings[i]);
        if (building != nullptr)
        {
            notice_building(building);
        }
    }

    was_in_closest_building = false;
    // On enabled nav mesh at ground level the npc is outside.
    const nav_cell* ncp = nav_cell_from_handle(np->pf.last_visited_nav_cell_handle);
    if (ncp != nullptr && (ncp->flags & NAV_CELL_FLAG_DISABLED) == 0 && nav_cell_is_point_in_cell_column(ncp, np->pos))
    {
        float ground;
        if (havok_terrain_height(ground, np->pos.x, np->pos.z, false, nullptr) && np->pos.y <= ground + 1.25f)
        {
            return true;
        }
    }

    ai_belief* closest = nullptr;
    int num_inside = 0;
    for (ai_belief* belief = get_first_belief(AI_BELIEF_BUILDING); belief != nullptr; belief = get_next_belief(belief))
    {
        if ((belief->flags & BF_ATTACHED_TO_ME) != 0)
        {
            if (closest == nullptr)
            {
                closest = belief;
            }
            ++num_inside;
        }
    }
    if (num_inside == 0)
    {
        return true;
    }
    if (num_inside > 1)
    {
        float best_dist = FLT_MAX;
        for (ai_belief* belief = get_first_belief(AI_BELIEF_BUILDING); belief != nullptr; belief = get_next_belief(belief))
        {
            const rfg_mover* building = rfg_mover_from_handle(belief->handle);
            float dist;
            if ((belief->flags & BF_ATTACHED_TO_ME) != 0 && ai_building_find_closest_navpoint(np->pos, building, false, &dist) != nullptr && dist < best_dist)
            {
                best_dist = dist;
                closest = belief;
            }
        }
    }
    update_existing_belief_internal(closest, closest->pos, 0.1f, closest->flags, AI_BELIEF_UPDATE_OVERWRITE);
    was_in_closest_building = true;
    return true;
}

// 0x006BD710 (:3528)
ai_belief* ai_perception::acquire_ally_combat_target(const human* ally)
{
    const human* target = human_combat_get_target(ally);
    if (target == nullptr || target->is_killed() || alm_grace_is_active(np, target))
    {
        return nullptr;
    }
    return note_possible_enemy_location(target, SIGHTED_ENEMY_RELEVANCE, true);
}

// The relevance of a human by distance squared: 0..10 within 20 m, 10..20 out to 50 m, at least 0.1.
static float friendly_relevance(float dist_sq)
{
    float relevance;
    if (dist_sq > 400.0f)
    {
        relevance = (dist_sq - 400.0f) * 10.0f / 2100.0f + 10.0f;
    }
    else
    {
        relevance = dist_sq * 10.0f / 400.0f;
    }
    return relevance - 0.1f < 0.0f ? 0.1f : relevance;
}

// 0x00665BE0 (:3558)
bool ai_perception::notice_ally(human* hp)
{
    const turret* tp = human_combat_get_manned_turret(hp);
    bool near_enough = true;
    const float dist_sq = vec_dist_squared(hp->pos, np->pos);
    if (ALLY_NOTICE_MAX_DIST_SQ < dist_sq)
    {
        near_enough = false;
        if (tp == nullptr || dist_sq > 10000.0f)
        {
            return false;
        }
    }
    const float relevance = friendly_relevance(dist_sq);
    const unsigned int flags = get_human_belief_flags(hp) | (relevance > 10.0f ? 0 : BF_NEARBY);
    if (tp != nullptr)
    {
        update_or_add_belief(AI_BELIEF_ALLY_ON_TURRET, hp, relevance, flags, AI_BELIEF_UPDATE_IF_MORE_RELEVANT);
    }
    if (near_enough)
    {
        if (temp_target != nullptr && check_ally_against_line_of_fire(hp, dist_sq))
        {
            register_ally_in_line_of_fire();
            if (hp->get_obj_type() == OT_HUMAN && hp->get_sub_type() == OT_SUB_HUMAN_NPC)
            {
                ((npc*)hp)->get_ai()->perception.register_in_ally_line_of_fire();
            }
        }
        update_or_add_belief(AI_BELIEF_CLOSEST_ALLY, hp, relevance, flags, AI_BELIEF_UPDATE_IF_MORE_RELEVANT);
    }
    return true;
}

// 0x00665D70 (:3617)
bool ai_perception::notice_friendly(human* hp)
{
    const float dist_sq = vec_dist_squared(hp->pos, np->pos);
    if (ALLY_NOTICE_MAX_DIST_SQ < dist_sq)
    {
        return false;
    }
    const float relevance = friendly_relevance(dist_sq);
    update_or_add_belief(AI_BELIEF_CLOSEST_FRIENDLY, hp, relevance, (relevance > 10.0f ? 0 : BF_NEARBY) | get_human_belief_flags(hp), AI_BELIEF_UPDATE_IF_MORE_RELEVANT);
    return true;
}

// 0x006B81A0 (:3649)
bool ai_perception::notice_unfriendly(human* hp, const temporary_eye_pos_orient eye_pos_orient)
{
    if (!target_in_fov(hp, eye_pos_orient))
    {
        record_los_result_unfriendly(hp, false);
        los_queue_remove_request(np, hp);
        return false;
    }
    switch (los_queue_check_records(np->get_handle(), hp->get_handle()))
    {
    case LOS_RECORD_POSITIVE:
        return record_los_result_unfriendly(hp, true) != nullptr;
    case LOS_RECORD_NEGATIVE:
        return record_los_result_unfriendly(hp, false) != nullptr;
    default:
        los_queue_add_request(np, hp, human_team_is_non_combatant(np) ? LOS_REQUEST_PRIORITY_LOW : LOS_REQUEST_PRIORITY_HIGH);
        return false;
    }
}

// 0x00665E60 (:3687)
void ai_perception::update_corpse_belief(const human* corpse)
{
    if (!corpse->is_killed())
    {
        return;
    }
    const float dist_sq = vec_dist_squared(corpse->pos, np->pos);
    float relevance;
    unsigned int flags;
    if (dist_sq > 400.0f)
    {
        // Original bug kept: a corpse beyond 20 m gets the best relevance, and the nearby flag.
        relevance = 0.0f;
        flags = BF_NEARBY;
    }
    else
    {
        relevance = dist_sq * 10.0f / 400.0f;
        flags = relevance > 10.0f ? 0 : BF_NEARBY;
    }
    update_or_add_belief(AI_BELIEF_CORPSE, corpse, relevance, flags, AI_BELIEF_UPDATE_OVERWRITE);
}

// 0x006BD770 (:3706)
void ai_perception::alert_corpse(const human* corpse, const human* enemy)
{
    if (corpse != nullptr && enemy != nullptr)
    {
        update_corpse_belief(corpse);
        suggest_enemy_exists(enemy, corpse->pos, CORPSE_ATTACKER_RELEVANCE, true);
    }
}

// 0x00665EF0 (:3716)
void ai_perception::alert_threatened(const human* threatener)
{
    if (threatener == nullptr)
    {
        return;
    }
    const ai_belief* first_unfriendly = belief_bins[AI_BELIEF_UNFRIENDLY].beliefs;
    const float relevance = get_human_belief_relevance(threatener);
    const unsigned int flags = (relevance > 10.0f ? 0 : BF_NEARBY) | get_human_belief_flags(threatener);
    const ai_belief* belief = update_or_add_belief(AI_BELIEF_UNFRIENDLY, threatener, relevance, flags, AI_BELIEF_UPDATE_OVERWRITE);
    if (belief != nullptr && human_team_is_non_combatant(np) && (first_unfriendly == nullptr || np->acknowledged_time.elapsed()))
    {
        ai->planner.request_replan();
    }
    np->acknowledged_time.set(15000);
}

// 0x006BD7B0 (:3741)
bool ai_perception::notice_corpse(human* hp)
{
    if (CORPSE_NOTICE_MAX_DIST_SQ < vec_dist_squared(hp->pos, np->pos))
    {
        return false;
    }
    const human_team_dispositions disposition = human_teams_get_disposition(np, hp);
    if (disposition == TEAM_DISPOSITION_HOSTILE)
    {
        if (hp->hflags.corpse_spotted_by_enemy == 0 && within_view_frustum(hp, true))
        {
            hp->hflags.corpse_spotted_by_enemy = 1;
        }
    }
    else if (disposition == TEAM_DISPOSITION_FRIENDLY || disposition == TEAM_DISPOSITION_ALLIED)
    {
        update_corpse_belief(hp);
        if (hp->hflags.corpse_spotted == 0 && within_view_frustum(hp, true))
        {
            hp->hflags.corpse_spotted = 1;
            if (belief_bins[AI_BELIEF_ENEMY_POSITION].beliefs == nullptr && belief_bins[AI_BELIEF_CLOSEST_ALLY].beliefs != nullptr)
            {
                np->play_voice_line(VOICE_LINE_REACT_ALLY_CORPSE_GROUP, nullptr, nullptr, AUDIOLIB_CUE_PRIORITY_VOICE_AMBIENT, false, false);
            }
            unsigned int killer_handle = hp->combat.last_hit_by_handle;
            if (killer_handle == 0xFFFFFFFF)
            {
                killer_handle = player_get_local_player()->get_handle();
            }
            human* killer = World.object_from_handle<human>(killer_handle);
            if (killer != nullptr && human_teams_are_enemies(killer, np))
            {
                suggest_enemy_exists(killer, hp->pos, CORPSE_ATTACKER_RELEVANCE, true);
                alert_shout_corpse shout(np, hp, killer);
                npc_sound_info::queue<alert_shout_corpse>(shout);
            }
        }
    }
    return true;
}

// 0x006BD910 (:3821)
bool ai_perception::notice_enemy(human* hp, const temporary_eye_pos_orient eye_pos_orient)
{
    if (!target_in_fov(hp, eye_pos_orient))
    {
        los_queue_remove_request(np, hp);
        record_los_result_enemy(hp, false);
        return false;
    }
    switch (los_queue_check_records(np->get_handle(), hp->get_handle()))
    {
    case LOS_RECORD_POSITIVE:
        return record_los_result_enemy(hp, true) != nullptr;
    case LOS_RECORD_NEGATIVE:
        return record_los_result_enemy(hp, false) != nullptr;
    default:
        los_queue_add_request(np, hp, human_team_is_non_combatant(np) ? LOS_REQUEST_PRIORITY_LOW : LOS_REQUEST_PRIORITY_HIGH);
        return false;
    }
}

// 0x00665FD0 (:3857)
bool ai_perception::notice_turret(turret* tp)
{
    if (!tp->is_functional(false) || tp->turret_flags.vehicle_turret)
    {
        return false;
    }
    const float dist_sq = vec_dist_squared(tp->pos, np->pos);
    if (dist_sq > 2500.0f)
    {
        return false;
    }
    unsigned int flags = 0;
    const human* op = human_combat_turret_get_operator(tp);
    if (op == np)
    {
        flags = BF_ATTACHED_TO_ME;
    }
    else if (op != nullptr)
    {
        flags = occupied_belief_flags_from_disposition(human_teams_get_disposition(np, op));
    }
    const float relevance = calculate_simple_relevance(sqrtf(dist_sq), 20.0f, 50.0f);
    if (relevance <= 10.0f)
    {
        flags |= BF_NEARBY;
    }
    update_or_add_belief(AI_BELIEF_TURRET, tp, relevance, flags, AI_BELIEF_UPDATE_OVERWRITE);
    return true;
}

// 0x006A5850 (:3903)
bool ai_perception::notice_vehicle(vehicle* ve)
{
    const bool heavy = ve->get_sub_type() == OT_SUB_VEHICLE_WALKER || vehicle_is_tank(ve);
    const float outer = heavy ? HEAVY_VEHICLE_NOTICE_MAX_DISTANCE : 50.0f;
    const float nearby = heavy ? 60.0f : 20.0f;
    const float dist_sq = vec_dist_squared(np->pos, ve->pos);
    if (outer * outer < dist_sq)
    {
        return false;
    }
    const bool can_be_hazard = human_in_vehicle(np) == nullptr && !heavy;
    const bool player_inside = local_player_is_in_vehicle(ve->get_handle());

    unsigned int flags = 0;
    const int num_seats = vehicle_get_num_seats(ve);
    for (int seat = 0; seat < num_seats; ++seat)
    {
        const human* occupant = (human*)typed_object_from_handle(ve->m_seat_info[seat].m_current_occupant_handle, OT_HUMAN);
        if (occupant == nullptr)
        {
            continue;
        }
        if (occupant == np)
        {
            flags |= BF_ATTACHED_TO_ME | BF_OCCUPIED;
        }
        else
        {
            flags |= BF_OCCUPIED | occupied_belief_flags_from_disposition(human_teams_get_disposition(np, occupant));
        }
    }

    if (heavy)
    {
        vector velocity;
        vehicle_get_velocity(ve, velocity);
        const float speed = velocity.mag_and_normalize_safe();
        flags |= BF_HEAVY_VEHICLE;
        if (speed <= 0.5f)
        {
            flags |= BF_STATIONARY;
        }
        vector to_me;
        to_me.x = np->pos.x - ve->pos.x;
        to_me.y = np->pos.y - ve->pos.y;
        to_me.z = np->pos.z - ve->pos.z;
        to_me.normalize_safe();
        if (to_me.y * velocity.y + velocity.x * to_me.x + to_me.z * velocity.z > 0.5f)
        {
            flags |= BF_COMING_AT_ME;
        }
    }
    if (dist_sq <= nearby * nearby)
    {
        flags |= BF_NEARBY;
    }

    float distance;
    if (can_be_hazard)
    {
        hazard_physics_info physics;
        get_hazard_physics_info(physics, np, ve);
        distance = physics.distance;
        if (physics.will_impact && human_in_vehicle(np) == nullptr
            && ((player_inside && local_player_is_friendly(np)) || dist_sq <= 100.0f || within_view_frustum(ve, true)))
        {
            add_hazard(ve, physics.time_to_pass * physics.distance_at_pass);
        }
    }
    else
    {
        distance = vec_dist(np->pos, ve->pos);
    }

    if (ve->m_flags.m_corpse != 0)
    {
        return false;
    }
    if (np->vehicle_handle != 0xFFFFFFFF && np->vehicle_handle != ve->get_handle() && ve->m_flags.m_on_fire != 0)
    {
        return false;
    }
    float relevance;
    if (nearby < distance)
    {
        relevance = (distance - nearby) * 10.0f / (outer - nearby) + 10.0f;
    }
    else
    {
        relevance = distance * 10.0f / nearby;
    }
    return update_or_add_belief(AI_BELIEF_VEHICLE, ve, relevance, flags, AI_BELIEF_UPDATE_OVERWRITE) != nullptr;
}

// 0x006660E0 (:4010)
bool ai_perception::notice_building(const rfg_mover* building)
{
    const bool destroyed = building->flags.fully_destroyed != 0;
    if (building->flags.is_ai_subpiece != 0 && destroyed)
    {
        return false;
    }
    float dist_sq;
    ai_building_find_dist_squared(dist_sq, np->pos, building, 0.0f);
    if ((building->flags.is_ai_building != 0 ? BUILDING_COLLECT_MAX_DIST_SQ : 400.0f) < dist_sq)
    {
        return false;
    }
    const rfg_mover* root = building->flags.is_ai_subpiece != 0 ? ai_building_get_root_building(building) : building;
    if (root == nullptr || (destroyed && get_belief(AI_BELIEF_BUILDING_STRESS, root) == nullptr))
    {
        return false;
    }

    ai_belief* existing = get_belief(AI_BELIEF_BUILDING, root);
    unsigned int flags = existing == nullptr ? 0 : existing->flags;
    if (dist_sq <= 100.0f)
    {
        flags |= BF_NEARBY | (dist_sq > 0.0f ? 0 : BF_ATTACHED_TO_ME);
    }
    for (const ai_belief* enemy = belief_bins[AI_BELIEF_ENEMY_EXISTS].beliefs; enemy != nullptr; enemy = get_next_belief(enemy))
    {
        float enemy_dist_sq;
        ai_building_find_dist_squared(enemy_dist_sq, enemy->pos, building, 0.0f);
        if (enemy_dist_sq <= 1.0f)
        {
            flags |= BF_OCCUPIED_BY_ENEMY;
        }
    }
    if (human_teams_are_unfriendly(np->current_team, building->team))
    {
        flags |= BF_UNFRIENDLY_BUILDING;
    }
    if (get_belief(AI_BELIEF_BUILDING_STRESS, root) != nullptr || activity_raid_is_demolition_target(root->get_handle()))
    {
        flags |= BF_UNSAFE;
    }

    const float dist = sqrtf(dist_sq);
    float relevance = dist > 10.0f ? (dist - 10.0f) * 10.0f / 30.0f + 10.0f : dist * 10.0f / 10.0f;
    if (relevance - 0.1f < 0.0f)
    {
        relevance = 0.1f;
    }
    if (existing == nullptr)
    {
        return add_new_belief(AI_BELIEF_BUILDING, root, relevance, flags) != nullptr;
    }
    if (relevance < existing->get_relevance())
    {
        return update_existing_belief(existing, relevance, flags, AI_BELIEF_UPDATE_OVERWRITE);
    }
    existing->flags = flags;
    return true;
}

// 0x006BDA40 (:4111)
bool ai_perception::notice_projectile(projectile* proj, const temporary_projectile_record* old_projectiles, int num_old_projectiles)
{
    if (proj->pflags.investigated != 0 || proj->w_info->projectile_info.start_speed > 50.0f)
    {
        return false;
    }
    const bool attached_to_me = proj->get_parent() == np->get_handle();
    if (!attached_to_me && proj->get_owner_handle() == player_get_local_player()->get_handle()
        && human_teams_get_disposition(np, HUMAN_TEAM_GUERILLA) == TEAM_DISPOSITION_ALLIED)
    {
        // The player's charges, and smart mines, are no danger to the player's friends.
        const weapon_class_type weapon_class = proj->w_info->weapon_class;
        if (weapon_class == WEAPON_CLASS_THROWN_CHARGE || (weapon_class == WEAPON_CLASS_THROWN_MINE && upgrade_has_smart_proximity_mines()))
        {
            return false;
        }
    }

    const bool explosive = proj->w_info->is_explosive();
    const bool stuck = proj->child_constraint_ptr != nullptr;
    hazard_physics_info physics;
    if (stuck || attached_to_me)
    {
        vector center;
        np->get_center_pos(center);
        physics.obj_to_npc.x = center.x - proj->pos.x;
        physics.obj_to_npc.y = center.y - proj->pos.y;
        physics.obj_to_npc.z = center.z - proj->pos.z;
        physics.distance = sqrtf(physics.obj_to_npc.y * physics.obj_to_npc.y + physics.obj_to_npc.x * physics.obj_to_npc.x + physics.obj_to_npc.z * physics.obj_to_npc.z);
        physics.velocity = ZERO_VECTOR;
        physics.time_to_pass = -1.0f;
        physics.distance_at_pass = -1.0f;
        physics.will_impact = false;
        physics.speed = 0.0f;
    }
    else
    {
        get_hazard_physics_info(physics, np, proj);
    }
    const bool slow = physics.speed < 0.1f;
    const bool not_moving = slow || stuck;
    if (!explosive && (not_moving || attached_to_me || !physics.will_impact))
    {
        return false;
    }

    bool known_before = false;
    unsigned int old_flags = 0;
    for (int i = 0; i < num_old_projectiles; ++i)
    {
        if (old_projectiles[i].proj == proj)
        {
            old_flags = old_projectiles[i].flags;
            known_before = true;
            break;
        }
    }

    bool seen;
    if (attached_to_me || (proj->w_info->weapon_class == WEAPON_CLASS_LAUNCHER && physics.distance < 10.0f))
    {
        seen = true;
    }
    else
    {
        seen = false;
        if ((not_moving && physics.distance > 5.0f) || (seen = within_view_frustum(proj, true)) == false)
        {
            if (!known_before || physics.distance > 40.0f)
            {
                return false;
            }
        }
    }

    if (proj->pflags.spotted == 0 && (int)human_teams_get_disposition(np, HUMAN_TEAM_EDF) > 2)
    {
        proj->pflags.spotted = 1;
    }

    human* owner = (human*)typed_object_from_handle(proj->get_owner_handle(), OT_HUMAN);
    int owner_disposition = owner == nullptr ? TEAM_DISPOSITION_NONE : human_teams_get_disposition(np, owner);
    const ai_belief* old_first_hazard = belief_bins[AI_BELIEF_HAZARD].beliefs;
    if (attached_to_me && object_is_local_player(owner) && player_friendly_fire.register_fire(-1.0f, np, owner))
    {
        owner_disposition = TEAM_DISPOSITION_HOSTILE;
    }

    ai_belief* hazard = nullptr;
    if (physics.will_impact)
    {
        hazard = add_hazard(proj, physics.distance_at_pass * physics.time_to_pass);
    }
    if (owner_disposition == TEAM_DISPOSITION_HOSTILE)
    {
        if (!not_moving || attached_to_me)
        {
            if (note_possible_enemy_location(owner, SIGHTED_ENEMY_RELEVANCE, true) != nullptr
                && update_or_add_belief(AI_BELIEF_SHOULD_ENGAGE_ENEMY, owner, vec_dist_squared(np->pos, owner->pos), 0, AI_BELIEF_UPDATE_OVERWRITE) != nullptr)
            {
                consider_switching_targets(owner);
            }
        }
        else if (belief_bins[AI_BELIEF_ENEMY_EXISTS].beliefs == nullptr)
        {
            suggest_enemy_exists(owner, proj->pos, LIVE_PROJECTILE_RELEVANCE, true);
        }
    }

    if (hazard == nullptr)
    {
        if (known_before)
        {
            float keep_radius = 1.0f;
            if (proj->w_info->is_explosive())
            {
                const float knockdown_radius = proj->w_info->m_explosion_info->m_knockdown_radius;
                if (1.0f - knockdown_radius < 0.0f)
                {
                    keep_radius = knockdown_radius;
                }
            }
            if (keep_radius < physics.distance)
            {
                deprecate_belief(AI_BELIEF_HAZARD, proj);
            }
        }
    }
    else if (!known_before && owner_disposition < TEAM_DISPOSITION_ALLIED && hazard != old_first_hazard && hazard == belief_bins[AI_BELIEF_HAZARD].beliefs)
    {
        ai->increase_tension_level(explosive ? 4 : 7);
        if (!ai->should_react(AI_REACTION_PROJECTILE) && attention.belief_type != AI_BELIEF_HAZARD)
        {
            ai->planner.request_replan();
            const ai_tension_levels tension_level = ai->get_tension_level();
            const int react_time = get_projectile_react_time(proj, physics, tension_level);
            ai->projectile_reaction = ai->pick_projectile_reaction(tension_level, explosive);
            if (ai->projectile_reaction == AI_PROJECTILE_REACTION_DODGE && owner_disposition > TEAM_DISPOSITION_HOSTILE)
            {
                ai->projectile_reaction = AI_PROJECTILE_REACTION_NONE;
            }
            if (ai->projectile_reaction != AI_PROJECTILE_REACTION_NONE)
            {
                ai->request_reaction(hazard, AI_REACTION_PROJECTILE, react_time);
            }
        }
        if (proj->w_info != nullptr && proj->w_info->weapon_class == WEAPON_CLASS_THROWN_CHARGE && owner != nullptr && object_is_player(owner))
        {
            set_last_hostile_action_pos(owner->pos);
        }
    }

    // In the blast: charges and singularity bombs by their radii, mines by their trigger radius.
    const weapon_info* w_info = proj->w_info;
    float danger_radius = -1.0f;
    if (w_info->weapon_class == WEAPON_CLASS_THROWN_CHARGE)
    {
        if (w_info->m_explosion_info != nullptr)
        {
            if (human_team_is_non_combatant(np) && proj->age_timer.time_since() < 2500)
            {
                danger_radius = w_info->m_explosion_info->m_knockdown_radius;
            }
            else
            {
                danger_radius = w_info->m_explosion_info->m_radius - 1.0f;
            }
        }
    }
    else if (w_info->weapon_class == WEAPON_CLASS_THROWN_MINE)
    {
        danger_radius = Proxy_mine_radius;
    }
    else if (w_info->weapon_class == WEAPON_CLASS_SINGULARITY_BOMB && w_info->m_explosion_info != nullptr)
    {
        danger_radius = w_info->m_explosion_info->m_knockdown_radius;
    }
    bool in_danger = false;
    if (danger_radius > 0.0f)
    {
        // Once in danger the npc has to get one metre further out.
        if ((old_flags & BF_NEARBY) != 0)
        {
            danger_radius += 1.0f;
        }
        in_danger = physics.distance < danger_radius;
    }

    unsigned int flags = seen ? BF_PROJECTILE_SEEN : BF_PROJECTILE_UNSEEN;
    if (not_moving)
    {
        flags |= BF_STATIONARY;
    }
    if (explosive)
    {
        flags |= BF_EXPLOSIVE;
    }
    if (attached_to_me)
    {
        flags |= BF_ATTACHED_TO_ME;
    }
    if (hazard != nullptr || (old_flags & BF_FLEEING_FROM) != 0)
    {
        flags |= BF_FLEEING_FROM;
    }
    if (in_danger)
    {
        flags |= BF_NEARBY;
    }
    float relevance = physics.distance * 10.0f / 40.0f;
    if (hazard == nullptr && !attached_to_me)
    {
        relevance += 10.0f;
    }
    const ai_belief* belief = update_or_add_belief(AI_BELIEF_PROJECTILE, proj, relevance, flags, AI_BELIEF_UPDATE_OVERWRITE);
    if (belief != nullptr && (flags & BF_ATTACHED_TO_ME) != 0 && (old_flags & BF_ATTACHED_TO_ME) == 0)
    {
        // Something just stuck to this npc: it says so.
        ai->planner.request_replan();
        npc_sound_info_pain pain(np, owner, proj->w_info);
        npc_sound_info::queue<npc_sound_info_pain>(pain);
    }
    return hazard != nullptr;
}

// ---- shots

// 0x005EE390 (:4407)
bool ai_perception::can_hit_target() const
{
    if (human_combat_get_manned_turret(np) == nullptr && attention.mode != ATTENTION_AIM_AT_OBJECT && attention.mode != ATTENTION_AIM_AT_BELIEF)
    {
        return false;
    }
    if (!attention.flags.aiming_at_target)
    {
        return false;
    }
    const ai_belief* shot = belief_bins[AI_BELIEF_SHOT_RESULT].beliefs;
    return shot != nullptr && shot->handle == attention.target && BELIEF_REQUIREMENTS_SUCCESSFUL_HIT.test(shot->flags);
}

// 0x005EE410 (:4437)
bool ai_perception::can_hit_close_to_target() const
{
    if (!attention.flags.aiming_at_target)
    {
        return false;
    }
    const ai_belief* shot = belief_bins[AI_BELIEF_SHOT_RESULT].beliefs;
    return shot != nullptr && shot->handle == attention.target && BELIEF_REQUIREMENTS_NEAR_HIT.test(shot->flags);
}

// 0x005EE460 (:4461)
bool ai_perception::recent_shot_was_bad() const
{
    const ai_belief* shot = belief_bins[AI_BELIEF_SHOT_RESULT].beliefs;
    return shot != nullptr && shot->handle == attention.target && BELIEF_REQUIREMENTS_BAD_SHOT.test(shot->flags);
}

// 0x00680EE0 (:4477)
bool ai_perception::aiming_at(const object* target, const weapon* wp, bool check_range) const
{
    if (target == nullptr || wp == nullptr)
    {
        return false;
    }
    const weapon_ammo_type ammo = wp->w_info->ammo_type;
    if (ammo != WEAPON_AMMO_TYPE_BULLET && ammo != WEAPON_AMMO_TYPE_PROJECTILE && ammo != WEAPON_AMMO_TYPE_ELECTRICITY
        && ammo != WEAPON_AMMO_TYPE_REPAIR && ammo != WEAPON_AMMO_TYPE_BLANKS && ammo != WEAPON_AMMO_TYPE_FLAME)
    {
        return false;
    }
    if (check_range)
    {
        const float max_range = wp->w_info->max_range;
        if (max_range * max_range < vec_dist_squared(np->pos, target->pos))
        {
            return false;
        }
    }
    const bool on_foot_weapon = wp->turret_handle == 0xFFFFFFFF;
    if (on_foot_weapon && !np->is_transformed())
    {
        return true;
    }
    vector muzzle_pos;
    matrix muzzle_orient;
    wp->get_muzzle_transform(muzzle_pos, &muzzle_orient);
    float radius_sq = 0.0f;
    vector target_pos;
    if (!get_effective_target_pos(target_pos, target, nullptr, &radius_sq, false))
    {
        return false;
    }
    vector dir;
    const float dist = vec_normalized_dir(dir, target_pos, muzzle_pos);
    const float dot = dir.y * muzzle_orient.fvec.y + muzzle_orient.fvec.x * dir.x + muzzle_orient.fvec.z * dir.z;
    float required = AIM_AT_TARGET_TURRET_DOT;
    if (on_foot_weapon)
    {
        required = (int)np->cur_movement_state > 1 ? AIM_AT_TARGET_RUNNING_DOT : AIM_AT_TARGET_DOT;
    }
    if (required <= dot)
    {
        return true;
    }
    // Off the required cone, but the ray still passes within the target's radius.
    return dot >= 0.0f && (1.0f - dot * dot) * dist * dist <= radius_sq;
}

// 0x006810E0 (:4565)
bool ai_perception::firing_los_clear(const object* target, human_stances stance, const vector* start_pos) const
{
    vector start;
    if (stance == HST_CURRENT && start_pos == nullptr)
    {
        const weapon* wp = human_weapons_get_weapon(np);
        if (wp == nullptr)
        {
            return false;
        }
        matrix muzzle_orient;
        wp->get_muzzle_transform(start, &muzzle_orient);
        start.y -= muzzle_orient.fvec.y * 0.75f;
        start.z -= muzzle_orient.fvec.z * 0.75f;
        start.x -= muzzle_orient.fvec.x * 0.75f;
    }
    else
    {
        if (stance == HST_CURRENT)
        {
            stance = np->cur_stance;
        }
        if (start_pos == nullptr)
        {
            if (np->hflags.override_default_anim_state != 0)
            {
                return false;
            }
            start_pos = &np->pos;
        }
        start = *start_pos;
        switch (stance)
        {
        case HST_STAND:
            start.y += 1.6f;
            break;
        case HST_CROUCH:
            start.y += 0.8f;
            break;
        case HST_PRONE:
            start.y += 0.3f;
            break;
        default:
            return false;
        }
    }

    vector target_pos;
    if (!get_effective_target_pos(target_pos, target, nullptr, nullptr, true))
    {
        return false;
    }
    weapon_fire_info fire_info;
    fire_info.start_pos = start;
    fire_info.end_pos = target_pos;
    fire_info.hit_handle = 0xFFFFFFFF;
    fire_info.w_info = nullptr;
    if (!weapons_test_bullet(fire_info, np, true, 0.0f, ZERO_VECTOR))
    {
        return true;
    }
    object* hit = World.object_from_handle(fire_info.hit_handle);
    if (target->get_obj_type() == OT_HUMAN)
    {
        return human_combat_hit_human(hit, (const human*)target);
    }
    return hit == target;
}

// 0x00681350 (:4655)
bool ai_perception::has_los_to_target(const object* target) const
{
    raycast_input input;
    input.radius = 0.0f;
    input.m_filter_info = COLLIDE_RAY_DEFAULT;
    input.m_hit_collector_type = raycast_input::COLLECT_CLOSEST_HIT;
    input.m_flags = 0xC; // the same flags as the LOS queue's rays
    raycast_output output;

    const human* target_human = (target != nullptr && target->get_obj_type() == OT_HUMAN) ? (const human*)target : nullptr;
    los_raycast_callback_data cb_data(np, target);
    human_get_head_pos(np, input.start);
    if (target_human != nullptr)
    {
        human_get_head_pos(target_human, input.end);
    }
    else
    {
        input.end = target->pos;
    }
    input.filter_params = &cb_data;
    input.filter_callback = los_raycast_filter_callback;

    // Steeper than about 60 degrees below the eyes the npc cannot see.
    const float dy = input.end.y - input.start.y;
    if (dy < 0.0f)
    {
        const float dx = input.end.x - input.start.x;
        const float dz = input.end.z - input.start.z;
        if (sqrtf(dx * dx + dz * dz) / fabsf(dy) < 0.577f)
        {
            return false;
        }
    }
    return !havok_raycast(input, output);
}

// 0x00666370 (:4702). LTCG: vp's slot on the stack holds target_pos; target in ESI.
static void percept_vehicle_turret_adjust_target_pos(const npc* np, const turret* tp, const human* target, vector& target_pos)
{
    const weapon* wp = tp->get_weapon_inst();
    if (wp == nullptr)
    {
        return;
    }
    const bool explosive = wp->w_info->is_explosive();
    const bool in_los = np->get_ai()->perception.enemy_in_los(target->get_handle());
    if (explosive && human_in_vehicle(target) == nullptr && in_los)
    {
        // Aim explosives at the feet.
        target_pos = target->pos;
        target_pos.y += 0.5f;
    }

    if (wp->w_info->ammo_type == WEAPON_AMMO_TYPE_THROWN || explosive)
    {
        bool over_own_vehicle = false;
        if (tp->turret_flags.vehicle_turret)
        {
            const vehicle* own_vehicle = tp->get_vehicle();
            if (own_vehicle != nullptr && vec_xz_dist_squared(target->pos, own_vehicle->pos) < 100.0f)
            {
                vector bmin, bmax;
                own_vehicle->get_local_bbox(bmin, bmax);
                vector local_pos;
                local_pos.x = target->pos.x - own_vehicle->pos.x;
                local_pos.y = target->pos.y - own_vehicle->pos.y;
                local_pos.z = target->pos.z - own_vehicle->pos.z;
                local_pos.unrotate(own_vehicle->orient);
                bbox2d box;
                box.min.x = bmin.x;
                box.min.y = bmin.z;
                box.max.x = bmax.x;
                box.max.y = bmax.z;
                over_own_vehicle = ix2d_point_bbox(local_pos.x, local_pos.z, box);
            }
        }
        if (!over_own_vehicle)
        {
            // Half a second of lead.
            vector velocity = ZERO_VECTOR;
            const vehicle* target_vehicle = human_in_vehicle(target);
            if (target_vehicle != nullptr)
            {
                vehicle_get_velocity(target_vehicle, velocity);
            }
            else
            {
                velocity = target->actual_velocity;
            }
            target_pos.x += velocity.x * 0.5f;
            target_pos.y += velocity.y * 0.5f;
            target_pos.z += velocity.z * 0.5f;
        }
    }

    const weapon_info* w_info = wp->w_info;
    if (w_info->projectile_info.obj_item_info != nullptr && w_info->projectile_info.gravity != 0.0f)
    {
        // Drop compensation; the first quarter second does not count.
        const float dx = np->pos.x - target_pos.x;
        const float dy = np->pos.y - target_pos.y;
        const float dz = np->pos.z - target_pos.z;
        const float flight_time = fabsf(sqrtf(dx * dx + dy * dy + dz * dz) / w_info->projectile_info.max_speed) - 0.25f;
        if (flight_time > 0.0f)
        {
            target_pos.y += fabsf(w_info->projectile_info.gravity * flight_time);
        }
    }
}

// 0x00666790 (:4794)
bool ai_perception::get_effective_target_pos(vector& pos, const object* target, bool* use_pos_belief, float* radius_sq, bool force_head_pos) const
{
    if (target == nullptr)
    {
        return false;
    }
    if (use_pos_belief != nullptr)
    {
        *use_pos_belief = false;
    }
    const vehicle* own_vehicle = (vehicle*)typed_object_from_handle(np->vehicle_handle, OT_VEHICLE);
    const turret* vehicle_turret = nullptr;
    if (own_vehicle != nullptr)
    {
        const turret* tp = vehicle_get_seat_turret(own_vehicle, np->vehicle_seat_idx);
        if (tp != nullptr && human_combat_turret_get_operator(tp) == np)
        {
            vehicle_turret = tp;
        }
    }

    const human* target_human = nullptr;
    const object* aim_object = target;
    if (target->get_obj_type() == OT_HUMAN)
    {
        target_human = (const human*)target;
        // A protected vehicle occupant, or one under explosive fire, is aimed at through the vehicle.
        bool explosive_weapon = false;
        const turret* reserved = (turret*)typed_object_from_handle(np->combat.reserved_turret, OT_TURRET);
        const weapon* wp = reserved != nullptr ? reserved->get_weapon_inst() : np->get_equipped_weapon();
        if (wp != nullptr && wp->w_info->is_explosive())
        {
            explosive_weapon = true;
        }
        if (human_in_vehicle(target_human) == nullptr || (!human_in_protected_vehicle_pos(target_human) && !explosive_weapon))
        {
            const ai_belief* belief = get_belief(AI_BELIEF_ENEMY_POSITION, target);
            if (belief == nullptr)
            {
                belief = get_belief(AI_BELIEF_UNFRIENDLY, target);
                if (belief == nullptr)
                {
                    return false;
                }
            }
            if (force_head_pos)
            {
                human_get_head_pos(target_human, pos);
            }
            else if (enemy_in_los(target->get_handle()))
            {
                if (target_human->is_transformed())
                {
                    const int tag = human_is_in_cover(target_human) ? target_human->h_info->head_bone_index : target_human->h_info->upper_spine_bone_index;
                    animlib_get_tag_world_coords(target_human->char_inst->aim_handle, tag, target->pos, target->orient, pos, nullptr);
                }
                else if (target_human->cur_stance == HST_STAND)
                {
                    pos.x = target->pos.x + UP_VECTOR.x * HUMAN_HEIGHT * 0.75f;
                    pos.y = target->pos.y + UP_VECTOR.y * HUMAN_HEIGHT * 0.75f;
                    pos.z = target->pos.z + UP_VECTOR.z * HUMAN_HEIGHT * 0.75f;
                }
                else if (target_human->cur_stance == HST_CROUCH)
                {
                    pos.x = target->pos.x + UP_VECTOR.x * HUMAN_HEIGHT * 0.33333334f;
                    pos.y = target->pos.y + UP_VECTOR.y * HUMAN_HEIGHT * 0.33333334f;
                    pos.z = target->pos.z + UP_VECTOR.z * HUMAN_HEIGHT * 0.33333334f;
                }
                else
                {
                    pos = target->pos;
                }
            }
            else
            {
                pos = belief->pos;
                pos.y += 1.33f;
                if (use_pos_belief != nullptr)
                {
                    *use_pos_belief = true;
                }
            }
            if (radius_sq != nullptr)
            {
                *radius_sq = 0.25f;
            }
            if (vehicle_turret != nullptr)
            {
                percept_vehicle_turret_adjust_target_pos(np, vehicle_turret, target_human, pos);
            }
            return true;
        }
        aim_object = typed_object_from_handle(target_human->vehicle_handle, OT_VEHICLE);
        if (aim_object == nullptr)
        {
            return false;
        }
    }

    vector bmin, bmax;
    aim_object->get_local_bbox(bmin, bmax);
    pos.x = (bmax.x + bmin.x) * 0.5f;
    pos.y = (bmin.y + bmax.y) * 0.5f;
    pos.z = (bmin.z + bmax.z) * 0.5f;
    if (aim_object->get_sub_type() == OT_SUB_VEHICLE_WALKER)
    {
        pos.y += 1.0f;
    }
    vec_change_to_global_coords(pos, aim_object->pos, aim_object->orient);
    if (radius_sq != nullptr)
    {
        // A quarter of the bbox diagonal, at least 1 m.
        const float value = vec_dist_squared(bmin, bmax) * 0.0625f;
        *radius_sq = 0.0f <= 1.0f - value ? 1.0f : value;
    }
    if (vehicle_turret != nullptr && target_human != nullptr)
    {
        percept_vehicle_turret_adjust_target_pos(np, vehicle_turret, target_human, pos);
    }
    return true;
}

// 0x005EE4B0 (:4951)
bool ai_perception::is_in_melee_move_range(const object* target, const human_melee_combat_move_data* move_data, const vector* eff_pos) const
{
    if (move_data == nullptr)
    {
        return false;
    }
    if (eff_pos == nullptr)
    {
        eff_pos = &np->pos;
    }
    float y_dist;
    float xz_dist_sq;
    if (target->get_obj_type() == OT_HUMAN)
    {
        y_dist = fabsf(target->pos.y - eff_pos->y);
        xz_dist_sq = vec_xz_dist_squared(*eff_pos, target->pos);
    }
    else
    {
        vector local_pos = *eff_pos;
        vec_change_to_local_coords(local_pos, target->pos, target->orient);
        vector bmin, bmax;
        target->get_local_bbox(bmin, bmax);
        y_dist = bmin.y - local_pos.y;
        if (0.0f <= (local_pos.y - bmax.y) - y_dist)
        {
            y_dist = local_pos.y - bmax.y;
        }
        xz_dist_sq = bbox_find_xz_distance_squared(bmin, bmax, local_pos);
    }
    if (y_dist > 1.2f)
    {
        return false;
    }
    return xz_dist_sq <= move_data->ai_max_dist * move_data->ai_max_dist;
}

// 0x00666D50 (:4997). LTCG: info in ECX, xz_dist in XMM1, move_data and allow_outside on the stack.
static void evaluate_melee_move(melee_candidate_selection_info& info, const human_melee_combat_move_data* move_data, float xz_dist, bool allow_outside)
{
    if (move_data == nullptr || (!allow_outside && xz_dist > move_data->ai_max_dist))
    {
        return;
    }
    if (move_data->ai_min_dist <= xz_dist)
    {
        info.candidates.add(move_data);
        return;
    }
    if (move_data->ai_min_dist < info.least_min_dist
        || (move_data->ai_min_dist == info.least_min_dist && move_data->ai_max_dist < info.least_max_dist))
    {
        info.fallback_move = move_data;
        info.least_min_dist = move_data->ai_min_dist;
        info.least_max_dist = move_data->ai_max_dist;
    }
}


// 0x00681570 (:5013)
const human_melee_combat_move_data* ai_perception::get_melee_move_in_range(const object* target, const weapon_info* w_info, bool allow_outside) const
{
    if (w_info == nullptr)
    {
        w_info = human_weapons_get_weapon_info(np);
        if (w_info == nullptr)
        {
            return nullptr;
        }
    }

    const human* target_human = nullptr;
    float xz_dist;
    if (target == nullptr || target->get_obj_type() != OT_HUMAN)
    {
        // Original bug kept: a null target is dereferenced here.
        vector local_pos = np->pos;
        vec_change_to_local_coords(local_pos, target->pos, target->orient);
        vector bmin, bmax;
        target->get_local_bbox(bmin, bmax);
        if (!allow_outside)
        {
            float y_dist = local_pos.y - bmax.y;
            if (y_dist - (bmin.y - local_pos.y) < 0.0f)
            {
                y_dist = bmin.y - local_pos.y;
            }
            if (y_dist > 1.2f)
            {
                return nullptr;
            }
        }
        xz_dist = sqrtf(bbox_find_xz_distance_squared(bmin, bmax, local_pos));
    }
    else
    {
        target_human = (const human*)target;
        if (!allow_outside && fabsf(target->pos.y - np->pos.y) > 1.2f)
        {
            return nullptr;
        }
        xz_dist = vec_xz_dist(np->pos, target->pos);
    }

    melee_candidate_selection_info info;
    if (w_info->standing_primary_melee_attack != INVALID_MELEE_COMBAT_MOVE_ID)
    {
        evaluate_melee_move(info, human_combat_move_data_from_index(np, w_info->standing_primary_melee_attack), xz_dist, allow_outside);
    }
    if (w_info->standing_secondary_melee_attack != INVALID_MELEE_COMBAT_MOVE_ID)
    {
        evaluate_melee_move(info, human_combat_move_data_from_index(np, w_info->standing_secondary_melee_attack), xz_dist, allow_outside);
    }
    if (w_info->standing_tertiary_melee_attack != INVALID_MELEE_COMBAT_MOVE_ID)
    {
        evaluate_melee_move(info, human_combat_move_data_from_index(np, w_info->standing_tertiary_melee_attack), xz_dist, allow_outside);
    }

    if (np->current_team == HUMAN_TEAM_MARAUDER && w_info->weapon_class == WEAPON_CLASS_GUTTER)
    {
        // Marauders vault at a running target coming their way, both on open nav mesh.
        static const human_melee_combat_move_id vault_attack_move_id = human_combat_lookup_combat_move("vault_attack"); // 0x01DE4430
        if (vault_attack_move_id != INVALID_MELEE_COMBAT_MOVE_ID && target_human != nullptr)
        {
            const nav_cell* ncp = nav_cell_from_handle(np->pf.last_visited_nav_cell_handle);
            float ground;
            if (ncp != nullptr && (ncp->flags & NAV_CELL_FLAG_DISABLED) == 0 && nav_cell_is_point_in_cell_column(ncp, np->pos)
                && havok_terrain_height(ground, np->pos.x, np->pos.z, false, nullptr) && np->pos.y <= ground + 1.25f
                && is_on_navmesh(target_human, nullptr, 1.25f) && target_human->actual_velocity_mag > 2.0f)
            {
                vector target_dir = target_human->velocity;
                target_dir.flatten_normalize_safe();
                vector my_dir = np->orient.fvec;
                my_dir.flatten_normalize();
                if (MARAUDER_VAULT_TARGET_DIR_COS < my_dir.y * target_dir.y + my_dir.x * target_dir.x + my_dir.z * target_dir.z)
                {
                    evaluate_melee_move(info, human_combat_move_data_from_index(np, vault_attack_move_id), xz_dist, allow_outside);
                }
            }
        }
    }

    const int num_candidates = info.candidates.size();
    if (num_candidates == 0)
    {
        return info.fallback_move;
    }
    return num_candidates == 1 ? info.candidates[0] : info.candidates[int_rand_range(0, num_candidates - 1)];
}

// 0x0068E980 (.h:281)
bool ai_perception::is_in_melee_range(const object* target, const weapon_info* w_info) const
{
    return get_melee_move_in_range(target, w_info, false) != nullptr;
}

// 0x005EE630 (:5140)
bool ai_perception::enemy_is_aiming_at_me(const human* enemy, vector* aim_pos) const
{
    const weapon* wp = human_weapons_get_weapon(enemy);
    if (wp == nullptr)
    {
        return false;
    }
    const weapon_ammo_type ammo = wp->w_info->ammo_type;
    if (ammo != WEAPON_AMMO_TYPE_BULLET && ammo != WEAPON_AMMO_TYPE_PROJECTILE && ammo != WEAPON_AMMO_TYPE_ELECTRICITY
        && ammo != WEAPON_AMMO_TYPE_REPAIR && ammo != WEAPON_AMMO_TYPE_BLANKS && ammo != WEAPON_AMMO_TYPE_FLAME)
    {
        return false;
    }

    vector start;
    vector dir;
    if (wp->turret_handle == 0xFFFFFFFF)
    {
        if (!human_is_combat_ready(enemy))
        {
            return false;
        }
        if (enemy->get_sub_type() == OT_SUB_HUMAN_NPC)
        {
            if (enemy->is_transformed())
            {
                matrix muzzle_orient;
                wp->get_muzzle_transform(start, &muzzle_orient);
                dir = muzzle_orient.fvec;
            }
            else
            {
                const human* target = human_combat_get_target(enemy);
                if (target == nullptr)
                {
                    return false;
                }
                human_get_head_pos(enemy, start);
                vector target_head;
                human_get_head_pos(target, target_head);
                vec_normalized_dir(dir, target_head, start);
            }
        }
        else if (enemy->get_sub_type() == OT_SUB_HUMAN_PLAYER)
        {
            wp->get_muzzle_transform(start, nullptr);
            vec_normalized_dir(dir, ((const player*)enemy)->aim_pos, start);
        }
        else
        {
            return false;
        }
    }
    else
    {
        matrix muzzle_orient;
        wp->get_muzzle_transform(start, &muzzle_orient);
        dir = muzzle_orient.fvec;
    }

    vector center;
    np->get_center_pos(center);
    const float dist = vec_dist(center, start);
    if (wp->w_info->max_engagement_dist < dist)
    {
        return false;
    }
    vector aim;
    aim.x = dir.x * dist + start.x;
    aim.y = start.y + dir.y * dist;
    aim.z = start.z + dir.z * dist;
    if (aim_pos != nullptr)
    {
        *aim_pos = aim;
    }
    const float dx = aim.x - center.x;
    const float dz = aim.z - center.z;
    static const float xz_limit_sq = 4.0f; // 0x01DDBACC
    static const float y_limit = 1.45f; // 0x01DDBAC8
    return fabsf(aim.y - center.y) <= y_limit && dx * dx + dz * dz <= xz_limit_sq;
}

// 0x005EE980 (:5241)
bool ai_perception::enemy_wants_to_kill_me(const human* enemy) const
{
    if (enemy->combat.target_handle == np->get_handle())
    {
        return true;
    }
    return object_is_player(enemy) && human_weapons_is_firearm_equipped(enemy) && enemy_is_aiming_at_me(enemy, nullptr);
}

// ---- attention: idle

// 0x005EE9D0 (:5261)
vector ai_perception::get_idle_attention_direction() const
{
    vector dir = np->cur_movement_state == HMS_IDLE ? np->orient.fvec : np->steering_vector;
    dir.flatten_normalize_safe();
    return dir;
}

// 0x005EEA20 (:5283)
bool ai_perception::is_valid_idle_attention_target(const object* obj, const vector& idle_direction) const
{
    vector dir;
    vec_normalized_dir(dir, obj->pos, np->pos);
    const timestamp& combat_ready = np->combat.combat_ready_timestamp;
    const float min_dot = (combat_ready.valid() && !combat_ready.elapsed()) ? attention.idle_ready_dot : attention.idle_not_ready_dot;
    return min_dot <= idle_direction.y * dir.y + idle_direction.x * dir.x + idle_direction.z * dir.z;
}

// 0x0063E100 (:5298)
ai_belief* ai_perception::get_current_idle_attention_belief() const
{
    if (attention.idle_belief_type == AI_BELIEF_UNDEFINED)
    {
        return nullptr;
    }
    if (attention.idle_target == 0xFFFFFFFF)
    {
        return belief_bins[attention.idle_belief_type].beliefs;
    }
    return get_belief(attention.idle_belief_type, attention.idle_target);
}

// 0x006A5C40 (:5323)
bool ai_perception::update_idle_attention_target(const ai_belief*& new_belief, const object*& new_obj, const vector* idle_direction)
{
    const object* old_obj = World.object_from_handle(attention.idle_target);
    const ai_belief* old_belief = get_current_idle_attention_belief();
    const bool target_gone = old_belief == nullptr && old_obj == nullptr
        && (attention.idle_belief_type != AI_BELIEF_UNDEFINED || attention.idle_target != 0xFFFFFFFF);
    bool can_be_bored;
    get_new_idle_attention_target(can_be_bored, new_belief, new_obj, idle_direction);
    if (target_gone || new_belief != old_belief || new_obj != old_obj)
    {
        if (can_be_bored)
        {
            attention.bored_timer.set_rand(2000, 5000);
        }
        else
        {
            attention.bored_timer.invalidate();
        }
        attention.idle_target = new_obj == nullptr ? 0xFFFFFFFF : new_obj->get_handle();
        attention.idle_belief_type = new_belief != nullptr ? new_belief->get_type() : AI_BELIEF_UNDEFINED;
        return true;
    }
    if (can_be_bored && attention.bored_timer.valid() && attention.bored_timer.elapsed())
    {
        if (new_obj != nullptr)
        {
            // Bored of it: remembered, so the next pass picks another target and the timer starts again there.
            attention.bored_idle_targets.add(new_obj->get_handle());
            return false;
        }
        attention.bored_idle_targets.add(0xFFFFFFFF);
        attention.bored_timer.set_rand(2000, 5000);
    }
    return false;
}

// 0x005EEAF0 (:5376)
object* ai_perception::get_current_idle_attention_target() const
{
    return World.object_from_handle(attention.idle_target);
}

// Whether a handle is one of the idle targets the npc got bored of.
static bool is_bored_of(const circ_array<unsigned int,2>& bored_targets, unsigned int handle)
{
    circ_array<unsigned int,2>& targets = const_cast<circ_array<unsigned int,2>&>(bored_targets);
    for (int i = 0; i < targets.size(); ++i)
    {
        if (targets[i] == handle)
        {
            return true;
        }
    }
    return false;
}

// 0x00681C20 (:5389)
void ai_perception::get_new_idle_attention_target(bool& can_be_bored, const ai_belief*& belief, const object*& obj, const vector* idle_direction) const
{
    belief = nullptr;
    obj = nullptr;
    can_be_bored = false;

    // Combat sounds of the last 3 s come first.
    const ai_belief* sound = get_most_recent_belief(AI_BELIEF_COMBAT_SOUND, BELIEF_REQUIREMENTS_NONE, OT_UNDEFINED);
    if (sound != nullptr && sound->update_time.time_since() <= 3000)
    {
        belief = sound;
        obj = World.object_from_handle(sound->handle);
        return;
    }

    // Then an enemy it may attack, seen in the last 3 s.
    belief_obj_iterator<object> objects;
    for (objects.init(ai, AI_BELIEF_ENEMY_POSITION); objects; ++objects)
    {
        if (can_engage_target_reason((const human*)(object*)objects) == AI_ENGAGE_ALLOWED && objects.get_belief()->update_time.time_since() <= 3000)
        {
            belief = objects.get_belief();
            obj = objects;
            return;
        }
    }

    const ai_belief* misc_sound = belief_bins[AI_BELIEF_MISC_SOUND].beliefs;
    if (misc_sound != nullptr)
    {
        belief = misc_sound;
        obj = World.object_from_handle(misc_sound->handle);
        return;
    }

    // A vehicle moving at 5 m/s or more nearby, or approaching.
    belief_obj_iterator<vehicle> vehicles;
    for (vehicles.init(ai, AI_BELIEF_VEHICLE); vehicles; ++vehicles)
    {
        const vehicle* vp = vehicles;
        if (vp->is_killed() || !vp->rdd.is_loaded() || vehicle_passenger_in_seat(vp, VSI_DRIVER) == nullptr)
        {
            continue;
        }
        vector velocity;
        vehicle_get_velocity(vp, velocity);
        if (velocity.y * velocity.y + velocity.x * velocity.x + velocity.z * velocity.z < 25.0f)
        {
            continue;
        }
        vector local_pos = np->pos;
        vector bmin, bmax;
        vp->get_local_bbox(bmin, bmax);
        vec_change_to_local_coords(local_pos, vp->pos, vp->orient);
        const float dist_sq = bbox_find_distance_squared(bmin, bmax, local_pos);
        if (dist_sq <= 25.0f)
        {
            belief = vehicles.get_belief();
            obj = vp;
            return;
        }
        const float approach = (np->pos.x - vp->pos.x) * velocity.x + (np->pos.y - vp->pos.y) * velocity.y + (np->pos.z - vp->pos.z) * velocity.z;
        if (approach > 0.0f && dist_sq <= 225.0f)
        {
            belief = vehicles.get_belief();
            obj = vp;
            return;
        }
    }

    // A crewed flyer within 100 m.
    const int num_flyers = World.get_num_objects<flyer>();
    for (int i = 0; i < num_flyers; ++i)
    {
        const vehicle* vp = World.get_object<flyer>(i);
        if (vp->rdd.is_loaded() && !vp->is_killed() && vehicle_passenger_in_seat(vp, VSI_DRIVER) != nullptr && vec_dist_squared(vp->pos, np->pos) <= 10000.0f)
        {
            obj = vp;
            return;
        }
    }

    if (np->current_team == HUMAN_TEAM_MARAUDER)
    {
        const player* pp = player_get_local_player();
        if (pp != nullptr && vec_dist_squared(np->pos, pp->pos) <= 400.0f)
        {
            obj = pp;
            return;
        }
    }

    // Nothing urgent: look at what is around, and get bored of it.
    can_be_bored = true;
    const vector direction = idle_direction != nullptr ? *idle_direction : get_idle_attention_direction();

    if (ai->get_formation_handle() != 0xFFFFFFFF)
    {
        const object* leader = ai_formation_get_leader(ai->get_formation_handle());
        if (leader != nullptr && !is_bored_of(attention.bored_idle_targets, leader->get_handle()))
        {
            belief = nullptr;
            obj = leader;
            return;
        }
    }

    const object_squad* squad = ai->get_squad();
    if (squad != nullptr)
    {
        const object* squad_player = squad->get_player();
        if (squad_player == nullptr)
        {
            const gfm_orders* orders = squad->get_squad_orders();
            if (orders != nullptr && orders->get_orders_type() == GFM_ORDERS_TYPE_DEFEND)
            {
                const object* target = orders->get_target_object();
                if (target != nullptr && target->get_obj_type() == OT_HUMAN && target->get_sub_type() == OT_SUB_HUMAN_PLAYER)
                {
                    squad_player = target;
                }
            }
        }
        if (squad_player != nullptr && !is_bored_of(attention.bored_idle_targets, squad_player->get_handle()))
        {
            belief = nullptr;
            obj = squad_player;
            return;
        }
    }

    for (objects.init(ai, AI_BELIEF_UNFRIENDLY); objects; ++objects)
    {
        const object* candidate = objects;
        if (!is_bored_of(attention.bored_idle_targets, candidate->get_handle()) && is_valid_idle_attention_target(candidate, direction))
        {
            belief = nullptr;
            obj = candidate;
            return;
        }
    }

    const object* kiosk = kiosk_get_active_kiosk();
    if (kiosk != nullptr && !is_bored_of(attention.bored_idle_targets, kiosk->get_handle()) && is_valid_idle_attention_target(kiosk, direction))
    {
        belief = nullptr;
        obj = kiosk;
        return;
    }

    for (objects.init(ai, AI_BELIEF_CLOSEST_ALLY); objects; ++objects)
    {
        const object* candidate = objects;
        if (!is_bored_of(attention.bored_idle_targets, candidate->get_handle()) && is_valid_idle_attention_target(candidate, direction))
        {
            belief = nullptr;
            obj = candidate;
            return;
        }
    }
    for (objects.init(ai, AI_BELIEF_CLOSEST_FRIENDLY); objects; ++objects)
    {
        const object* candidate = objects;
        if (!is_bored_of(attention.bored_idle_targets, candidate->get_handle()) && is_valid_idle_attention_target(candidate, direction))
        {
            belief = nullptr;
            obj = candidate;
            return;
        }
    }
    for (vehicles.init(ai, AI_BELIEF_VEHICLE); vehicles; ++vehicles)
    {
        const vehicle* candidate = vehicles;
        if (!is_bored_of(attention.bored_idle_targets, candidate->get_handle()) && is_valid_idle_attention_target(candidate, direction))
        {
            belief = nullptr;
            obj = candidate;
            return;
        }
    }
}

// 0x006A5DC0 (:5547)
void ai_perception::do_attention_idle_frame()
{
    // Allies of the player follow where the player aims when the player just fired.
    player* pp = nullptr;
    const object_squad* squad = ai->get_squad();
    if (squad != nullptr)
    {
        pp = squad->get_player();
        if (pp == nullptr)
        {
            const gfm_orders* orders = squad->get_squad_orders();
            if (orders != nullptr && orders->get_orders_type() == GFM_ORDERS_TYPE_DEFEND)
            {
                pp = World.object_cast<player>(orders->get_target_object());
            }
        }
    }
    if (pp == nullptr)
    {
        pp = player_get_local_player();
        if (human_teams_get_disposition(pp, np) != TEAM_DISPOSITION_ALLIED || vec_dist_squared(pp->pos, np->pos) > 100.0f || pp == nullptr)
        {
            pp = nullptr;
        }
    }
    if (pp != nullptr && pp->combat.combat_aim_timestamp.valid() && !pp->combat.combat_aim_timestamp.elapsed() && human_weapons_is_firearm_equipped(pp))
    {
        const ai_belief* sound = get_belief(AI_BELIEF_COMBAT_SOUND, pp);
        if (sound == nullptr || sound->update_time.time_since() >= 300)
        {
            return;
        }
        const float dx = pp->aim_pos.x - np->pos.x;
        const float dy = pp->aim_pos.y - np->pos.y;
        const float dz = pp->aim_pos.z - np->pos.z;
        if (dx * dx + dz * dz <= 0.25f && dy <= 2.4f && dy >= -0.5f)
        {
            // The player aims at this npc: look back.
            vector eye;
            human_get_eye_pos_orient(pp, eye, nullptr);
            human_set_look_at_target(np, eye, false, 0.5f);
            return;
        }
        human_set_look_at_target(np, pp->aim_pos, false, 0.5f);
        return;
    }

    // Civilians of a beaten district look at the ground ahead near anything unfriendly.
    if (human_team_is_non_combatant(np) && ai->get_district_morale_level() == AI_MORALE_LEVEL_LOW && !ai->persona_flags.stand_up_to_edf)
    {
        const ai_belief* unfriendly = belief_bins[AI_BELIEF_UNFRIENDLY].beliefs;
        const object* threat = unfriendly == nullptr ? nullptr : typed_object_from_handle(unfriendly->handle, OT_HUMAN);
        if (threat != nullptr && vec_dist_squared(np->pos, threat->pos) <= 225.0f)
        {
            vector ahead;
            ahead.y = np->pos.y + np->orient.fvec.y * 4.0f;
            ahead.z = np->pos.z + np->orient.fvec.z * 4.0f;
            ahead.x = np->pos.x + np->orient.fvec.x * 4.0f;
            human_set_look_at_target(np, ahead, false, 0.5f);
            return;
        }
    }

    const ai_belief* current = get_current_idle_attention_belief();
    if (!attention.update_target_timer.valid() || attention.update_target_timer.elapsed())
    {
        attention.update_target_timer.set_rand(1000, 1500);
        const ai_belief* new_belief = nullptr;
        const object* new_obj = nullptr;
        if (update_idle_attention_target(new_belief, new_obj, nullptr))
        {
            if (new_belief != nullptr)
            {
                human_set_look_at_target(np, new_belief->pos, false, 0.25f);
            }
            else if (new_obj != nullptr)
            {
                set_look_at_object(new_obj, false, 0.25f, true);
            }
            else
            {
                human_spinebend_reset_target(np);
            }
            return;
        }
    }
    if (current != nullptr)
    {
        human_set_look_at_target(np, current->pos, false, 0.25f);
    }
}

// ---- attention: search

// 0x00682940 (:5631). LTCG: np in ESI, dir in EDI. Whether a 3 m ray from the head along dir is clear.
static bool search_los_raycast(const npc* np, const vector& dir)
{
    raycast_input input;
    input.radius = 0.0f;
    input.m_filter_info = COLLIDE_RAY_DEFAULT;
    input.m_hit_collector_type = raycast_input::COLLECT_CLOSEST_HIT;
    raycast_output output;
    los_raycast_callback_data cb_data(np, nullptr);
    human_get_head_pos(np, input.start);
    input.end.x = dir.x * 3.0f + input.start.x;
    input.end.y = input.start.y + dir.y * 3.0f;
    input.end.z = input.start.z + dir.z * 3.0f;
    input.filter_params = &cb_data;
    input.m_flags = 0xC;
    input.filter_callback = los_raycast_filter_callback;
    return !havok_raycast(input, output);
}

// 0x006A61D0 (:5646)
void ai_perception::do_attention_search_frame()
{
    vector direction = np->cur_movement_state == HMS_IDLE ? np->orient.fvec : np->steering_vector;
    direction.flatten_normalize_safe();

    if (!attention.update_target_timer.valid() || attention.update_target_timer.elapsed())
    {
        attention.update_target_timer.set_rand(400, 500);
        const ai_belief* new_belief = nullptr;
        const object* new_obj = nullptr;
        update_idle_attention_target(new_belief, new_obj, &direction);
        const bool has_target = new_obj != nullptr || new_belief != nullptr;

        // One side per update.
        if (attention.flags.check_right)
        {
            attention.flags.right_side_clear = search_los_raycast(np, direction.copy_rotate_around_y_axis(attention.search_spread_angle));
        }
        else
        {
            attention.flags.left_side_clear = search_los_raycast(np, direction.copy_rotate_around_y_axis(-attention.search_spread_angle));
        }
        attention.flags.check_right = !attention.flags.check_right;
        const bool center_ok = has_target || !attention.flags.right_side_clear || !attention.flags.left_side_clear;

        if (new_belief != nullptr && new_belief->get_type() == AI_BELIEF_COMBAT_SOUND)
        {
            human_set_look_at_target(np, new_belief->pos, false, 1.0f);
            attention.direction = ATTENTION_DIRECTION_TARGET;
        }
        else
        {
            bool change_direction = attention.current_target_timer.elapsed();
            switch (attention.direction)
            {
            case ATTENTION_DIRECTION_LEFT:
                change_direction = change_direction || !attention.flags.left_side_clear;
                break;
            case ATTENTION_DIRECTION_RIGHT:
                change_direction = change_direction || !attention.flags.right_side_clear;
                break;
            case ATTENTION_DIRECTION_TARGET:
                change_direction = change_direction || !has_target;
                break;
            case ATTENTION_DIRECTION_CENTER:
                if (has_target || !center_ok)
                {
                    change_direction = true;
                }
                break;
            default:
                break;
            }
            if (change_direction)
            {
                // The direction not looked in for the longest.
                int choice = -1;
                int longest = 0;
                if (center_ok)
                {
                    choice = 0;
                    longest = attention.center_timer.time_since();
                }
                if (attention.flags.right_side_clear)
                {
                    const int since = attention.right_side_timer.time_since();
                    if (choice == -1 || longest < since)
                    {
                        choice = 1;
                        longest = since;
                    }
                }
                if (attention.flags.left_side_clear)
                {
                    const int since = attention.left_side_timer.time_since();
                    if (choice == -1 || longest < since)
                    {
                        choice = 2;
                    }
                }
                if (choice == 0)
                {
                    if (has_target)
                    {
                        if (new_obj != nullptr)
                        {
                            set_look_at_object(new_obj, false, 0.25f, true);
                        }
                        else
                        {
                            human_set_look_at_target(np, new_belief->pos, false, 0.25f);
                        }
                        attention.direction = ATTENTION_DIRECTION_TARGET;
                    }
                    else
                    {
                        human_spinebend_reset_target(np);
                        attention.direction = ATTENTION_DIRECTION_CENTER;
                    }
                    attention.center_timer.set(0);
                }
                else if (choice == 1)
                {
                    attention.right_side_timer.set(0);
                    attention.direction = ATTENTION_DIRECTION_RIGHT;
                }
                else if (choice == 2)
                {
                    attention.left_side_timer.set(0);
                    attention.direction = ATTENTION_DIRECTION_LEFT;
                }
                if (np->cur_movement_state == HMS_IDLE)
                {
                    attention.current_target_timer.set_rand(2500, 3500);
                }
                else
                {
                    attention.current_target_timer.set_rand(1200, 1800);
                }
            }
        }
    }

    if (attention.direction == ATTENTION_DIRECTION_LEFT || attention.direction == ATTENTION_DIRECTION_RIGHT)
    {
        vector eye;
        human_get_eye_pos_orient(np, eye, nullptr);
        const float angle = attention.direction == ATTENTION_DIRECTION_LEFT ? -attention.search_spread_angle : attention.search_spread_angle;
        const vector rotated = direction.copy_rotate_around_y_axis(angle);
        vector look_at;
        look_at.x = rotated.x * 2.0f + eye.x;
        look_at.y = rotated.y * 2.0f + eye.y;
        look_at.z = rotated.z * 2.0f + eye.z;
        human_set_look_at_target(np, look_at, false, 0.15f);
    }
}

// Back to idle attention; the body of set_attention_mode(ATTENTION_IDLE, default, normal) that rfg.exe inlines at the
// end of do_attention_acquire_frame.
static const float IDLE_READY_DOT = 0.70710677f;

// 0x006A6730 (:5811)
void ai_perception::do_attention_acquire_frame()
{
    for (ai_belief* belief = belief_bins[AI_BELIEF_ENEMY_POSITION].beliefs; belief != nullptr; )
    {
        if (belief->handle == np->combat.target_handle)
        {
            vector to_target;
            to_target.y = belief->pos.y - np->pos.y;
            to_target.z = belief->pos.z - np->pos.z;
            to_target.x = belief->pos.x - np->pos.x;
            const float dist_sq = to_target.x * to_target.x + to_target.y * to_target.y + to_target.z * to_target.z;
            to_target.flatten_normalize_safe();

            if (!attention.update_target_timer.valid() || attention.update_target_timer.elapsed())
            {
                if ((belief->flags & BF_LAST_POSITION_VISIBLE) == 0)
                {
                    vector above;
                    above.y = belief->pos.y + UP_VECTOR.y;
                    above.x = belief->pos.x + UP_VECTOR.x;
                    above.z = belief->pos.z + UP_VECTOR.z;
                    if (has_los_to_position(above, false))
                    {
                        belief->flags |= BF_LAST_POSITION_VISIBLE;
                    }
                }
                if (attention.flags.check_right)
                {
                    attention.flags.right_side_clear = search_los_raycast(np, to_target.copy_rotate_around_y_axis(attention.search_spread_angle));
                }
                else
                {
                    attention.flags.left_side_clear = search_los_raycast(np, to_target.copy_rotate_around_y_axis(-attention.search_spread_angle));
                }
                attention.flags.check_right = !attention.flags.check_right;
                const bool center_ok = !attention.flags.right_side_clear || !attention.flags.left_side_clear;

                bool change_direction = attention.current_target_timer.elapsed();
                if (attention.direction == ATTENTION_DIRECTION_LEFT)
                {
                    change_direction = change_direction || !attention.flags.left_side_clear;
                }
                else if (attention.direction == ATTENTION_DIRECTION_RIGHT)
                {
                    change_direction = change_direction || !attention.flags.right_side_clear;
                }
                else if (attention.direction == ATTENTION_DIRECTION_CENTER)
                {
                    change_direction = change_direction || !center_ok;
                }
                if (change_direction)
                {
                    int choice = -1;
                    int longest = 0;
                    if (center_ok)
                    {
                        choice = 0;
                        longest = attention.center_timer.time_since();
                    }
                    if (attention.flags.right_side_clear)
                    {
                        const int since = attention.right_side_timer.time_since();
                        if (choice == -1 || longest < since)
                        {
                            choice = 1;
                            longest = since;
                        }
                    }
                    if (attention.flags.left_side_clear)
                    {
                        const int since = attention.left_side_timer.time_since();
                        if (choice == -1 || longest < since)
                        {
                            choice = 2;
                        }
                    }
                    if (choice == 0)
                    {
                        attention.direction = ATTENTION_DIRECTION_CENTER;
                        attention.center_timer.set(0);
                    }
                    else if (choice == 1)
                    {
                        attention.direction = ATTENTION_DIRECTION_RIGHT;
                        attention.right_side_timer.set(0);
                    }
                    else if (choice == 2)
                    {
                        attention.direction = ATTENTION_DIRECTION_LEFT;
                        attention.left_side_timer.set(0);
                    }
                    attention.current_target_timer.set_rand(700, 1000);
                }
                attention.update_target_timer.set_rand(250, 400);
            }

            // Within 7 m it brings the gun up too.
            const bool aim = dist_sq < 49.0f;
            vector eye;
            human_get_eye_pos_orient(np, eye, nullptr);
            vector look_dir = to_target;
            if (attention.direction == ATTENTION_DIRECTION_LEFT)
            {
                look_dir = to_target.copy_rotate_around_y_axis(-attention.search_spread_angle);
            }
            else if (attention.direction == ATTENTION_DIRECTION_RIGHT)
            {
                look_dir = to_target.copy_rotate_around_y_axis(attention.search_spread_angle);
            }
            else if (attention.direction != ATTENTION_DIRECTION_CENTER)
            {
                return;
            }
            vector look_at;
            look_at.x = look_dir.x * 2.0f + eye.x;
            look_at.y = look_dir.y * 2.0f + eye.y;
            look_at.z = look_dir.z * 2.0f + eye.z;
            human_set_look_at_target(np, look_at, aim, 0.15f);
            return;
        }
        belief = belief->next;
        if (belief == belief_bins[AI_BELIEF_ENEMY_POSITION].beliefs)
        {
            break;
        }
    }

    // The target's position is not known any more: back to idle.
    attention.steering = ATTENTION_STEERING_TURN_TO_TARGET;
    np->hflags.anchor_orient = 0;
    attention.awareness = ATTENTION_AWARENESS_NORMAL;
    if (attention.mode != ATTENTION_IDLE)
    {
        np->look_at_speed = 0.25f;
        if (!attention.update_target_timer.valid() || attention.update_target_timer.time_since() > 5000)
        {
            attention.bored_timer.invalidate();
            attention.bored_idle_targets.remove_all();
            attention.update_target_timer.invalidate();
            attention.idle_target = 0xFFFFFFFF;
            attention.idle_belief_type = AI_BELIEF_UNDEFINED;
            attention.idle_not_ready_dot = human_team_is_non_combatant(np) ? IDLE_READY_DOT : 0.0f;
            attention.idle_ready_dot = IDLE_READY_DOT;
            human_spinebend_reset_target(np);
        }
    }
    attention.mode = ATTENTION_IDLE;
}

// 0x00666DF0 (:5955)
void ai_perception::do_attention_object_frame()
{
    const object* obj = World.object_from_handle(attention.target);
    if (obj != nullptr)
    {
        // Original bug kept: the asm compares the mode with 4, ATTENTION_AIM_AT_BELIEF, which this frame never has, so
        // ATTENTION_AIM_AT_OBJECT never aims through here.
        set_look_at_object(obj, attention.mode == ATTENTION_AIM_AT_BELIEF, -1.0f, false);
        return;
    }
    if (!set_attention_mode(attention.mode == ATTENTION_AIM_AT_OBJECT ? ATTENTION_AIM_AT_BELIEF : ATTENTION_LOOK_AT_BELIEF, ATTENTION_STEERING_USE_CURRENT_MODE, ATTENTION_AWARENESS_USE_CURRENT_MODE))
    {
        human_spinebend_reset_target(np);
        attention.mode = ATTENTION_LOOK_AT_NONE;
    }
}

// 0x00666E70 (:5983)
void ai_perception::do_attention_belief_frame()
{
    if (attention.belief_type != AI_BELIEF_UNDEFINED)
    {
        const ai_belief* belief = find_belief(belief_bins[attention.belief_type].beliefs, attention.target);
        if (belief != nullptr)
        {
            vector target = belief->pos;
            if (belief->get_obj_type() == OT_HUMAN)
            {
                target.y += 1.33f;
            }
            human_set_look_at_target(np, target, attention.mode == ATTENTION_AIM_AT_BELIEF, 0.5f);
            return;
        }
    }
    human_spinebend_reset_target(np);
    attention.mode = ATTENTION_LOOK_AT_NONE;
}

// 0x005EEB10 (:6012)
void ai_perception::update_attention_steering_mode(attention_look_at_modes look_at_mode, attention_steering_modes steering_mode)
{
    switch (steering_mode)
    {
    case ATTENTION_STEERING_USE_CURRENT_MODE:
        return;
    case ATTENTION_STEERING_USE_DEFAULT:
    {
        if (look_at_mode == ATTENTION_USE_CURRENT_MODE)
        {
            look_at_mode = attention.mode;
        }
        const bool plant_feet = look_at_mode == ATTENTION_SEARCH || look_at_mode == ATTENTION_ACQUIRE_VISUAL;
        attention.steering = plant_feet ? ATTENTION_STEERING_PLANT_FEET : ATTENTION_STEERING_TURN_TO_TARGET;
        np->hflags.anchor_orient = plant_feet ? 1 : 0;
        return;
    }
    case ATTENTION_STEERING_TURN_TO_TARGET:
        np->hflags.anchor_orient = 0;
        attention.steering = steering_mode;
        return;
    case ATTENTION_STEERING_PLANT_FEET:
        np->hflags.anchor_orient = 1;
        attention.steering = steering_mode;
        return;
    default:
        return;
    }
}

// 0x0063E150 (:6069)
void ai_perception::set_look_at_object(const object* obj, bool aim_at, float aim_speed, bool reset)
{
    if (aim_speed <= 0.0f)
    {
        aim_speed = aim_at ? 1.0f : 0.5f;
    }
    if (obj == nullptr || obj->get_obj_type() != OT_HUMAN)
    {
        // Original bug kept: a null obj is dereferenced here.
        if (reset || np->look_at_handle != obj->get_handle())
        {
            human_set_look_at_target(np, obj->get_handle(), ZERO_VECTOR, aim_at, aim_speed);
        }
        return;
    }

    const human* target = (const human*)obj;
    bool explosive = false;
    const turret* reserved = (turret*)typed_object_from_handle(np->combat.reserved_turret, OT_TURRET);
    const weapon* wp = reserved != nullptr ? reserved->get_weapon_inst() : np->get_equipped_weapon();
    if (wp != nullptr && wp->w_info->is_explosive())
    {
        explosive = true;
    }
    if (human_in_vehicle(target) == nullptr || (!human_in_protected_vehicle_pos(target) && !explosive))
    {
        if (reset || np->look_at_handle != obj->get_handle())
        {
            human_set_look_at_target(np, obj->get_handle(), ZERO_VECTOR, aim_at, aim_speed);
        }
        return;
    }
    // Protected in a vehicle: look at the vehicle instead.
    if (reset || np->look_at_handle != target->vehicle_handle)
    {
        const object* vp = typed_object_from_handle(target->vehicle_handle, OT_VEHICLE);
        if (vp != nullptr)
        {
            const vector walker_offset(0.0f, 1.0f, 0.0f);
            human_set_look_at_target(np, vp->get_handle(), vp->get_sub_type() == OT_SUB_VEHICLE_WALKER ? walker_offset : ZERO_VECTOR, aim_at, aim_speed);
        }
    }
}

// ---- enemies

// 0x006B82D0 (:6164)
ai_belief* ai_perception::suggest_enemy_exists(const human* enemy, const vector& guess_pos, float relevance, bool no_investigation)
{
    const unsigned int handle = enemy->get_handle();
    ai_belief* exists = find_belief(belief_bins[AI_BELIEF_ENEMY_EXISTS].beliefs, handle);
    if (find_belief(belief_bins[AI_BELIEF_ENEMY_POSITION].beliefs, handle) != nullptr)
    {
        return exists;
    }
    ai_belief* unfriendly = find_belief(belief_bins[AI_BELIEF_UNFRIENDLY].beliefs, handle);
    if (unfriendly != nullptr && vec_dist_squared(enemy->pos, guess_pos) <= 100.0f)
    {
        go_hostile_on_unfriendly_target(unfriendly, enemy, &exists, nullptr, true);
        return exists;
    }
    unsigned int flags = get_human_belief_flags(enemy);
    if (exists == nullptr)
    {
        if (no_investigation)
        {
            flags |= BF_FROM_ORDERS;
        }
        return add_new_belief(AI_BELIEF_ENEMY_EXISTS, handle, OT_HUMAN, guess_pos, relevance, flags);
    }
    if (no_investigation)
    {
        return exists;
    }
    if ((exists->flags & BF_INVESTIGATED) == 0 && exists->get_relevance() <= relevance)
    {
        exists->flags = (exists->flags & BF_STICKY_MASK) | flags;
        return exists;
    }
    const float new_relevance = 0.0f <= exists->get_relevance() - relevance ? relevance : exists->get_relevance();
    update_existing_belief_internal(exists, guess_pos, new_relevance, flags, AI_BELIEF_UPDATE_OVERWRITE);
    if (belief_bins[AI_BELIEF_ENEMY_EXISTS].beliefs == exists)
    {
        ai->planner.request_replan();
    }
    return exists;
}

// 0x006B8520 (:6248)
ai_belief* ai_perception::note_possible_enemy_location(const human* enemy, float relevance, bool react)
{
    const unsigned int handle = enemy->get_handle();
    unsigned int flags = get_human_belief_flags(enemy);
    if (object_is_player(enemy) && human_teams_get_disposition(np->current_team, enemy->current_team) == TEAM_DISPOSITION_ALLIED && !player_friendly_fire.is_hostile())
    {
        return nullptr;
    }

    ai_belief* exists = find_belief(belief_bins[AI_BELIEF_ENEMY_EXISTS].beliefs, handle);
    ai_belief* position = find_belief(belief_bins[AI_BELIEF_ENEMY_POSITION].beliefs, handle);
    if (position != nullptr)
    {
        // Read from the asm (0x006B86B6): with a first contact belief it returns the position belief as it is.
        if (find_belief(belief_bins[AI_BELIEF_FIRST_CONTACT].beliefs, handle) != nullptr)
        {
            return position;
        }
    }
    else
    {
        ai_belief* unfriendly = find_belief(belief_bins[AI_BELIEF_UNFRIENDLY].beliefs, handle);
        if (unfriendly != nullptr)
        {
            go_hostile_on_unfriendly_target(unfriendly, enemy, &exists, &flags, react);
            return exists;
        }
    }

    const bool had_position = belief_bins[AI_BELIEF_ENEMY_POSITION].beliefs != nullptr;
    bool position_updated = false;
    bool consider_target = false;
    bool replan = false;
    if (exists == nullptr)
    {
        exists = add_new_belief(AI_BELIEF_ENEMY_EXISTS, handle, enemy->get_obj_type(), enemy->pos, relevance, flags);
        replan = !had_position && exists != nullptr && belief_bins[AI_BELIEF_ENEMY_EXISTS].beliefs == exists;
    }
    else if (position != nullptr)
    {
        const int since_position = position->update_time.time_since();
        if (exists->update_time.time_since() >= BELIEF_MIN_UPDATE_MS)
        {
            update_existing_belief_internal(exists, enemy->pos, exists->get_relevance(), flags, AI_BELIEF_UPDATE_OVERWRITE);
        }
        update_existing_belief_internal(position, enemy->pos, get_human_belief_relevance(enemy), flags | BF_UNCONFIRMED, AI_BELIEF_UPDATE_OVERWRITE);
        position_updated = true;
        consider_target = belief_bins[AI_BELIEF_FIRST_CONTACT].beliefs == nullptr;
        replan = position == belief_bins[AI_BELIEF_ENEMY_POSITION].beliefs && since_position > 1800;
    }
    else
    {
        const float new_relevance = relevance - exists->get_relevance() < 0.0f ? relevance : exists->get_relevance();
        if (exists->update_time.time_since() >= BELIEF_MIN_UPDATE_MS)
        {
            update_existing_belief_internal(exists, enemy->pos, new_relevance, flags, AI_BELIEF_UPDATE_OVERWRITE);
        }
        replan = !had_position && belief_bins[AI_BELIEF_ENEMY_EXISTS].beliefs == exists;
    }

    if (position == nullptr && relevance <= SIGHTED_ENEMY_RELEVANCE)
    {
        position = add_new_belief(AI_BELIEF_ENEMY_POSITION, handle, enemy->get_obj_type(), enemy->pos, get_human_belief_relevance(enemy), flags | BF_UNCONFIRMED);
        position_updated = position != nullptr;
        if (position != nullptr)
        {
            if (belief_bins[AI_BELIEF_FIRST_CONTACT].beliefs == nullptr)
            {
                consider_target = true;
            }
            if (position == belief_bins[AI_BELIEF_ENEMY_POSITION].beliefs)
            {
                replan = true;
            }
        }
    }

    if (consider_target || np->combat.target_handle == 0xFFFFFFFF)
    {
        consider_switching_targets(enemy);
    }
    if (replan)
    {
        ai->planner.request_replan();
    }
    if (position_updated && handle == np->combat.target_handle)
    {
        target_tracking_update_target(enemy, false);
    }
    return exists;
}

// 0x006B8AC0 (:6377)
ai_belief* ai_perception::confirm_enemy_location(const human* enemy, bool react)
{
    return note_possible_enemy_location(enemy, SIGHTED_ENEMY_RELEVANCE, react);
}


// 0x006A6E60 (:6395)
ai_belief* ai_perception::go_hostile_on_unfriendly_target(ai_belief* unfriendly_belief, const human* enemy, ai_belief** enemy_exists_belief, const unsigned int* flags, bool react)
{
    ai_belief* exists = enemy_exists_belief != nullptr ? *enemy_exists_belief : find_belief(belief_bins[AI_BELIEF_ENEMY_EXISTS].beliefs, enemy->get_handle());
    unsigned int new_flags = flags != nullptr ? *flags : get_human_belief_flags(enemy);
    const bool unconfirmed = (unfriendly_belief->flags & BF_UNCONFIRMED) != 0;
    if (unconfirmed)
    {
        new_flags |= BF_UNCONFIRMED;
    }
    if (exists == nullptr)
    {
        exists = add_new_belief(AI_BELIEF_ENEMY_EXISTS, enemy->get_handle(), enemy->get_obj_type(), enemy->pos, SIGHTED_ENEMY_RELEVANCE, new_flags);
    }
    else
    {
        update_existing_belief_internal(exists, enemy->pos, SIGHTED_ENEMY_RELEVANCE, new_flags, AI_BELIEF_UPDATE_OVERWRITE);
    }

    const float relevance = ::get_human_belief_relevance(vec_dist(np->pos, enemy->pos));
    ai_belief* position = update_or_add_belief(AI_BELIEF_ENEMY_POSITION, enemy, relevance, new_flags, AI_BELIEF_UPDATE_OVERWRITE);
    if (position != nullptr)
    {
        if (!unconfirmed)
        {
            vector contact_pos;
            if (!los_record_get_pos_from_record(enemy->get_handle(), np->get_handle(), contact_pos))
            {
                contact_pos = enemy->pos;
            }
            update_or_add_belief(AI_BELIEF_LAST_CONTACT, enemy, contact_pos, relevance, new_flags, AI_BELIEF_UPDATE_OVERWRITE);
            update_or_add_belief(AI_BELIEF_FIRST_CONTACT, enemy, relevance, new_flags, AI_BELIEF_UPDATE_OVERWRITE);
            const human* old_target = human_combat_get_target(np);
            const bool switched = consider_switching_targets(enemy);
            if (react && old_target == nullptr && switched && !human_is_combat_ready(np) && human_in_vehicle(np) == nullptr)
            {
                ai->request_reaction(position, AI_REACTION_FIRST_SIGHTING, -1);
            }
        }
        else if (belief_bins[AI_BELIEF_FIRST_CONTACT].beliefs == nullptr)
        {
            consider_switching_targets(enemy);
        }
        if (position == belief_bins[AI_BELIEF_ENEMY_POSITION].beliefs)
        {
            ai->planner.request_replan();
        }
        deprecate_belief(unfriendly_belief);
        if (enemy->get_handle() == np->combat.target_handle)
        {
            target_tracking_update_target(enemy, !unconfirmed);
        }
    }
    if (enemy_exists_belief != nullptr)
    {
        *enemy_exists_belief = exists;
    }
    return position;
}

// 0x006BB6F0 (.h:198)
void ai_perception::go_hostile(const human* enemy, bool react)
{
    confirm_enemy_location(enemy, react);
}

// 0x006A71A0 (:6492)
ai_belief* ai_perception::record_enemy_is_visible(const human* enemy)
{
    unsigned int flags = get_human_belief_flags(enemy);
    const float dist = vec_dist(np->pos, enemy->pos);
    if (dist < Ai_fov_length_low * 0.5f)
    {
        flags |= BF_NEARBY;
    }
    const float relevance = (float)((double)dist * 10.0 / (double)Ai_fov_length_low);
    float exists_relevance = relevance / 20.0f;
    if (0.0f <= exists_relevance - SIGHTED_ENEMY_RELEVANCE)
    {
        exists_relevance = SIGHTED_ENEMY_RELEVANCE;
    }
    update_or_add_belief(AI_BELIEF_ENEMY_EXISTS, enemy, exists_relevance, flags, AI_BELIEF_UPDATE_OVERWRITE);
    ai_belief* position = update_or_add_belief(AI_BELIEF_ENEMY_POSITION, enemy, relevance, flags, AI_BELIEF_UPDATE_OVERWRITE);
    if (position == nullptr)
    {
        return nullptr;
    }
    vector contact_pos;
    if (!los_record_get_pos_from_record(enemy->get_handle(), np->get_handle(), contact_pos))
    {
        contact_pos = enemy->pos;
    }
    update_or_add_belief(AI_BELIEF_LAST_CONTACT, enemy, contact_pos, relevance, flags, AI_BELIEF_UPDATE_OVERWRITE);
    if (find_belief(belief_bins[AI_BELIEF_FIRST_CONTACT].beliefs, enemy->get_handle()) == nullptr)
    {
        add_new_belief(AI_BELIEF_FIRST_CONTACT, enemy, relevance, flags);
    }
    ai_belief* unfriendly = find_belief(belief_bins[AI_BELIEF_UNFRIENDLY].beliefs, enemy->get_handle());
    if (unfriendly != nullptr)
    {
        deprecate_belief(unfriendly);
    }
    if (enemy->get_handle() == np->combat.target_handle)
    {
        target_tracking_update_target(enemy, true);
    }
    consider_switching_targets(enemy);
    return position;
}

// 0x0063E340 (:6558)
void ai_perception::record_enemy_not_visible(const human* enemy)
{
    const unsigned int handle = enemy->get_handle();
    const ai_belief* position = find_belief(belief_bins[AI_BELIEF_ENEMY_POSITION].beliefs, handle);
    ai_belief* first_contact = find_belief(belief_bins[AI_BELIEF_FIRST_CONTACT].beliefs, handle);
    if (first_contact != nullptr && (position == nullptr || position->update_time.time_since() > 1000))
    {
        deprecate_belief(first_contact);
    }
}

// The body of set_attention_mode(ATTENTION_IDLE, ...) that rfg.exe inlines in the two functions losing their target.
void ai_perception::enter_idle_attention()
{
    attention.steering = ATTENTION_STEERING_TURN_TO_TARGET;
    np->hflags.anchor_orient = 0;
    attention.awareness = ATTENTION_AWARENESS_NORMAL;
    if (attention.mode != ATTENTION_IDLE)
    {
        np->look_at_speed = 0.25f;
        if (!attention.update_target_timer.valid() || attention.update_target_timer.time_since() > 5000)
        {
            attention.bored_timer.invalidate();
            attention.bored_idle_targets.remove_all();
            attention.update_target_timer.invalidate();
            attention.idle_target = 0xFFFFFFFF;
            attention.idle_belief_type = AI_BELIEF_UNDEFINED;
            attention.idle_not_ready_dot = human_team_is_non_combatant(np) ? IDLE_READY_DOT : 0.0f;
            attention.idle_ready_dot = IDLE_READY_DOT;
            human_spinebend_reset_target(np);
        }
    }
    attention.mode = ATTENTION_IDLE;
}

// 0x00682AF0 (:6574)
void ai_perception::check_combat_target()
{
    const unsigned int handle = np->combat.target_handle;
    if (handle == 0xFFFFFFFF)
    {
        return;
    }
    const human* target = (human*)typed_object_from_handle(handle, OT_HUMAN);
    ai_belief* exists = find_belief(belief_bins[AI_BELIEF_ENEMY_EXISTS].beliefs, handle);
    if (target == nullptr || target->is_killed() || exists == nullptr || (int)get_effective_disposition(target, false) > 1)
    {
        deprecate_all_enemy_beliefs(handle, exists);
        np->combat.target_handle = 0xFFFFFFFF;
        ai_update_combat_target_building(np);
        target_tracking_update_target(nullptr, false);
        if (attention.target == handle)
        {
            enter_idle_attention();
            ai->planner.request_replan();
        }
        return;
    }
    // A target in a vehicle with someone on its turret: shoot the gunner, unless the weapon takes out the vehicle.
    const vehicle* vp = human_in_vehicle(target);
    if (vp == nullptr)
    {
        return;
    }
    const turret* manned = ai_vehicle_get_first_manned_turret(vp);
    if (manned == nullptr)
    {
        return;
    }
    const human* gunner = World.object_from_handle<human>(manned->current_operator);
    if (gunner == nullptr || gunner == target)
    {
        return;
    }
    const turret* reserved = World.object_from_handle<turret>(np->combat.reserved_turret);
    const weapon* wp = reserved != nullptr ? reserved->get_weapon_inst() : np->get_equipped_weapon();
    if (wp != nullptr && !wp->w_info->is_explosive())
    {
        np->combat.target_handle = gunner->get_handle();
        ai_update_combat_target_building(np);
    }
}

// 0x00682D60 (:6655)
void ai_perception::update_combat_target()
{
    if (np->combat.target_handle != 0xFFFFFFFF)
    {
        // Only a melee marauder retargets, while its target climbs into a vehicle.
        if (np->current_team != HUMAN_TEAM_MARAUDER)
        {
            return;
        }
        const human* current = (human*)typed_object_from_handle(np->combat.target_handle, OT_HUMAN);
        if (current == nullptr || !human_in_vehicle_interaction(current, nullptr))
        {
            return;
        }
        const weapon* wp = np->get_equipped_weapon();
        if (wp == nullptr || wp->w_info == nullptr || wp->w_info->ammo_type != WEAPON_AMMO_TYPE_MELEE)
        {
            return;
        }
    }

    ai_belief* best = nullptr;
    bool best_armed = false;
    bool best_in_los = false;
    ai_belief* next;
    for (ai_belief* belief = belief_bins[AI_BELIEF_ENEMY_POSITION].beliefs; belief != nullptr; belief = next)
    {
        next = get_next_belief(belief);
        const human* hp = (human*)typed_object_from_handle(belief->handle, OT_HUMAN);
        if (hp == nullptr || hp->is_killed() || alm_grace_is_active(np, hp))
        {
            deprecate_all_enemy_beliefs(belief->handle, nullptr);
            continue;
        }
        if (hp->h_info->flags.guerrilla_tech && !mission_is_active())
        {
            continue;
        }
        const bool in_los = enemy_in_los(belief->handle);
        const bool armed = human_weapons_get_weapon(hp) != nullptr;
        bool unreachable = false;
        if (np->current_team == HUMAN_TEAM_MARAUDER)
        {
            const weapon* wp = np->get_equipped_weapon();
            if (wp != nullptr && wp->w_info != nullptr && wp->w_info->ammo_type == WEAPON_AMMO_TYPE_MELEE)
            {
                unreachable = human_in_vehicle_interaction(hp, nullptr);
            }
        }
        if (in_los && armed && !unreachable)
        {
            best = belief;
            break;
        }
        if (best == nullptr || (armed && !best_armed) || (armed == best_armed && in_los && !best_in_los))
        {
            best_armed = armed;
            best_in_los = in_los;
            best = belief;
        }
    }

    if (best == nullptr)
    {
        for (ai_belief* belief = belief_bins[AI_BELIEF_ENEMY_EXISTS].beliefs; belief != nullptr; belief = next)
        {
            next = get_next_belief(belief);
            const human* hp = (human*)typed_object_from_handle(belief->handle, OT_HUMAN);
            if (hp == nullptr || hp->is_killed())
            {
                deprecate_all_enemy_beliefs(belief->handle, belief);
                continue;
            }
            if (hp->h_info->flags.guerrilla_tech && !mission_is_active())
            {
                continue;
            }
            if (alm_grace_is_active(np, hp))
            {
                deprecate_all_enemy_beliefs(belief->handle, nullptr);
                continue;
            }
            best = belief;
            break;
        }
    }
    if (best != nullptr)
    {
        np->combat.target_handle = best->handle;
        ai_update_combat_target_building(np);
        updated_combat_target = true;
        ai->planner.request_replan();
    }
}

// 0x005EEBD0 (:6763)
bool ai_perception::should_ignore_player() const
{
    if (Ai_ignore_player || np->npc_flags.ignore_player != 0)
    {
        return true;
    }
    const player* pp = player_get_local_player();
    if ((int)human_teams_get_disposition(np, pp->h_info->default_team) < 2 && np->npc_flags.ambush_related == 0)
    {
        return safehouse_should_ignore_player();
    }
    return false;
}

// 0x00683060 (:6795)
bool ai_perception::should_switch_targets(const human* new_target) const
{
    const unsigned int new_handle = new_target->get_handle();
    const unsigned int cur_handle = np->combat.target_handle;
    const human* cur_target = (human*)typed_object_from_handle(cur_handle, OT_HUMAN);
    if ((int)human_teams_get_disposition(np, new_target) >= 2)
    {
        return object_is_player(new_target) && player_friendly_fire.is_hostile();
    }
    if (cur_target == nullptr || cur_target->is_killed())
    {
        return true;
    }

    bool new_in_walker = false;
    bool cur_in_walker = false;
    const player* pp = player_get_local_player();
    if (pp == new_target || pp == cur_target)
    {
        const object* vehicle_obj = typed_object_from_handle(pp->vehicle_handle, OT_VEHICLE);
        if (vehicle_obj != nullptr && vehicle_obj->get_sub_type() == OT_SUB_VEHICLE_WALKER)
        {
            if (pp == new_target)
            {
                new_in_walker = true;
            }
            else
            {
                cur_in_walker = true;
            }
            const float dx = pp->pos.x - np->pos.x;
            const float dy = pp->pos.y - np->pos.y;
            const float dz = pp->pos.z - np->pos.z;
            if (dx * dx + dy * dy + dz * dz < Ai_target_player_in_walker_dist * Ai_target_player_in_walker_dist)
            {
                return new_in_walker;
            }
        }
    }

    // Each test decides unless both targets answer the same.
    const bool new_exists = get_belief(AI_BELIEF_ENEMY_EXISTS, new_handle) != nullptr;
    if (new_exists != (get_belief(AI_BELIEF_ENEMY_EXISTS, cur_handle) != nullptr))
    {
        return new_exists;
    }
    const bool new_position = get_belief(AI_BELIEF_ENEMY_POSITION, new_handle) != nullptr;
    if (new_position != (get_belief(AI_BELIEF_ENEMY_POSITION, cur_handle) != nullptr))
    {
        return new_position;
    }
    const bool new_priority = ai_scripting_is_priority_target(new_target);
    if (new_priority != ai_scripting_is_priority_target(cur_target))
    {
        return new_priority;
    }
    const bool new_armed = new_in_walker || new_target->hflags.considered_armed != 0 || object_is_player(new_target) || human_weapons_get_weapon(new_target) != nullptr;
    const bool cur_armed = cur_in_walker || cur_target->hflags.considered_armed != 0 || object_is_player(cur_target) || human_weapons_get_weapon(cur_target) != nullptr;
    if (new_armed != cur_armed)
    {
        return new_armed;
    }
    if (can_engage_target_reason(cur_target) != AI_ENGAGE_ALLOWED && can_engage_target_reason(new_target) == AI_ENGAGE_ALLOWED)
    {
        return true;
    }

    vector to_cur;
    to_cur.y = cur_target->pos.y - np->pos.y;
    to_cur.z = cur_target->pos.z - np->pos.z;
    to_cur.x = cur_target->pos.x - np->pos.x;
    vector to_new;
    to_new.y = new_target->pos.y - np->pos.y;
    to_new.z = new_target->pos.z - np->pos.z;
    to_new.x = new_target->pos.x - np->pos.x;
    float cur_score = to_cur.mag_and_normalize_safe();
    float new_score = to_new.mag_and_normalize_safe();
    if (!updated_combat_target)
    {
        new_score += 2.0f;
    }

    // In contact, recently hurt by it, or told to engage it (the last counts against its distance).
    const ai_belief_specification specs[3] =
    {
        ai_belief_specification(AI_BELIEF_FIRST_CONTACT, 0xFFFFFFFF, -1, BELIEF_REQUIREMENTS_NONE),
        ai_belief_specification(AI_BELIEF_DAMAGE_TAKEN, 0xFFFFFFFF, 1500, BELIEF_REQUIREMENTS_NONE),
        ai_belief_specification(AI_BELIEF_SHOULD_ENGAGE_ENEMY, 0xFFFFFFFF, 5000, BELIEF_REQUIREMENTS_NONE),
    };
    const ai_belief* cur_match = find_matching_belief(specs, 3, cur_handle);
    if (cur_match != nullptr && cur_match->get_type() == AI_BELIEF_SHOULD_ENGAGE_ENEMY)
    {
        cur_score = cur_score * 1.2f + 2.0f;
    }
    const ai_belief* new_match = find_matching_belief(specs, 3, new_handle);
    if (new_match != nullptr && new_match->get_type() == AI_BELIEF_SHOULD_ENGAGE_ENEMY)
    {
        new_score = new_score * 1.2f + 2.0f;
    }
    if ((cur_match != nullptr) != (new_match != nullptr))
    {
        return new_match != nullptr;
    }
    if (!new_in_walker && !enemy_wants_to_kill_me(new_target))
    {
        new_score += 1.5f;
    }
    if (!cur_in_walker && !enemy_wants_to_kill_me(cur_target))
    {
        cur_score += 1.5f;
    }
    return new_score < cur_score;
}

// 0x00666F30 (:6963)
bool ai_perception::maybe_fudge_last_contact_belief(const human* new_target)
{
    const unsigned int handle = new_target->get_handle();
    const ai_belief* position = find_belief(belief_bins[AI_BELIEF_ENEMY_POSITION].beliefs, handle);
    if (position == nullptr || enemy_in_los(handle))
    {
        return false;
    }
    const ai_belief* last_contact = find_belief(belief_bins[AI_BELIEF_LAST_CONTACT].beliefs, handle);
    const float relevance = ::get_human_belief_relevance(vec_dist(np->pos, new_target->pos));
    const unsigned int flags = last_contact != nullptr ? last_contact->flags : get_human_belief_flags(new_target) | BF_UNCONFIRMED;
    return update_or_add_belief(AI_BELIEF_LAST_CONTACT, new_target, position->pos, relevance, flags, AI_BELIEF_UPDATE_OVERWRITE) != nullptr;
}

// 0x00683630 (:6999)
bool ai_perception::consider_switching_targets(const human* new_target)
{
    if (new_target == nullptr)
    {
        return false;
    }
    if (new_target->h_info->flags.guerrilla_tech && !mission_is_active())
    {
        return false;
    }
    if (new_target->get_handle() == np->combat.target_handle || alm_grace_is_active(np, new_target))
    {
        return false;
    }
    if (!should_switch_targets(new_target))
    {
        return false;
    }
    const unsigned int old_target = np->combat.target_handle;
    if (old_target != 0xFFFFFFFF && old_target == attention.target)
    {
        maybe_fudge_last_contact_belief(new_target);
        ai->planner.request_replan();
    }
    np->combat.target_handle = new_target->get_handle();
    ai_update_combat_target_building(np);
    updated_combat_target = true;
    return true;
}

// 0x00667010 (:7076)
float ai_perception::get_distance_squared_to_nearest_building(const vector& pos, rfg_mover** building_ptr) const
{
    if (building_ptr != nullptr)
    {
        *building_ptr = nullptr;
    }
    const ai_belief* closest = belief_bins[AI_BELIEF_BUILDING].beliefs;
    if (closest != nullptr)
    {
        rfg_mover* building = rfg_mover_from_handle(closest->handle);
        if (building != nullptr)
        {
            if (building_ptr != nullptr)
            {
                *building_ptr = building;
            }
            float dist_sq;
            ai_building_find_dist_squared(dist_sq, pos, building, 0.0f);
            return dist_sq;
        }
    }
    return -1.0f;
}

// 0x00667080 (:7104)
float ai_perception::get_distance_squared_to_nearest_building(const human* hp, rfg_mover** building_ptr) const
{
    return get_distance_squared_to_nearest_building(hp->pos, building_ptr);
}

// 0x00667090 (:7108)
float ai_perception::get_distance_squared_to_nearest_building(rfg_mover** building_ptr) const
{
    return get_distance_squared_to_nearest_building(np->pos, building_ptr);
}

// ---- construction

// 0x0063E3C0
ai_perception::attention_info::attention_info()
{
}

// 0x0063E3F0
ai_perception::attention_info::~attention_info()
{
}

// 0x006670B0 (:7119)
ai_perception::ai_perception()
{
    for (int i = 0; i < NUM_AI_BELIEF_TYPES; ++i)
    {
        belief_bins[i].count = 0;
        belief_bins[i].beliefs = nullptr;
    }
    dead_belief_list = nullptr;
}

// 0x0064B190
ai_perception::~ai_perception()
{
}

// 0x006671A0 (:7130)
void ai_perception::init(npc* np_)
{
    np = np_;
    ai = np_->get_ai();
    clear();
}

// 0x0063E400 (:7137)
void ai_perception::clear()
{
    notice_humans_time.next.set(0);
    notice_corpses_time.next.set(0);
    notice_objects_time.next.set(0);
    notice_projectiles_time.next.set(0);
    notice_buildings_time.next.set(0);
    notice_friendlies_time.next.set(0);
    belief_cleanup_time.set(0);
    check_muzzle_time.set(0);
    muzzle_stuck_time.set(0);
    notice_building_timeout.set(0);
    notice_buildings_pos.x = FLT_MIN; // far from anywhere after the first distance check
    notice_buildings_pos.y = FLT_MIN;
    notice_buildings_pos.z = FLT_MIN;
    was_in_closest_building = false;
    in_ally_line_of_fire_time.set(0);
    ally_in_line_of_fire_time.set(0);
    temp_target = nullptr;
    updated_combat_target = false;
    attention.clear();
    for (int i = 0; i < NUM_AI_BELIEF_TYPES; ++i)
    {
        clear_beliefs((ai_belief_types)i);
    }
    free_ai_belief_list(dead_belief_list);
    player_friendly_fire.reset(np);
}

// 0x005EEC50 (:7185)
void ai_perception::attention_info::clear()
{
    mode = ATTENTION_LOOK_AT_NONE;
    steering = ATTENTION_STEERING_TURN_TO_TARGET;
    awareness = ATTENTION_AWARENESS_NORMAL;
    target = 0xFFFFFFFF;
    belief_type = AI_BELIEF_UNDEFINED;
    memset(&flags, 0, sizeof(flags));
    current_target_timer.set(0);
    update_target_timer.set(0);
    center_timer.set(0);
    right_side_timer.set(0);
    left_side_timer.set(0);
    idle_target = 0xFFFFFFFF;
    idle_belief_type = AI_BELIEF_UNDEFINED;
    idle_ready_dot = 0.0f;
    idle_not_ready_dot = 0.0f;
    search_spread_angle = 0.0f;
    direction = ATTENTION_DIRECTION_CENTER;
    aim_update_time = int_rand_range(350, 500); // ms
    last_decent_shot_time.invalidate();
}

// 0x005EECF0 (:7216)
void ai_perception::clear_beliefs(ai_belief_types type)
{
    if (type < NUM_AI_BELIEF_TYPES)
    {
        free_ai_belief_list(belief_bins[type].beliefs);
        belief_bins[type].count = 0;
    }
}

// 0x005EED20 (:7223)
void ai_perception::clear_beliefs(ai_belief_types type, const ai_belief_requirements& requirements)
{
    if (type >= NUM_AI_BELIEF_TYPES)
    {
        return;
    }
    ai_belief* belief = belief_bins[type].beliefs;
    while (belief != nullptr)
    {
        ai_belief* next = belief->next != belief_bins[type].beliefs ? belief->next : nullptr;
        if (requirements.test(belief->flags))
        {
            unlink_belief(belief_bins[type].beliefs, belief);
            --belief_bins[type].count;
            free_ai_belief(belief);
        }
        belief = next;
    }
}


// ---- shots and sounds

// 0x0063E630 (:7251)
ai_perception::shot_results ai_perception::analyze_shot_result(const object* target_obj, const weapon_fire_info& fire_info, analyze_shot_modes mode)
{
    const object* hit_obj = World.object_from_handle(fire_info.hit_handle);
    if (target_obj == hit_obj || (target_obj != nullptr && target_obj->get_obj_type() == OT_HUMAN && human_combat_hit_human(hit_obj, (const human*)target_obj)))
    {
        return SHOT_RESULT_HIT_TARGET;
    }
    const human* hit_human = human_combat_get_hit_human(hit_obj);
    if (hit_human != nullptr)
    {
        if (human_teams_get_disposition(np, hit_human) == TEAM_DISPOSITION_HOSTILE)
        {
            return SHOT_RESULT_GOOD_SHOT;
        }
        if (hit_obj == hit_human && mode == ANALYZE_SHOT_ACTUAL_BULLET && human_teams_are_allies(np, hit_human))
        {
            np->play_voice_line(VOICE_LINE_COMBAT_FRIENDLY_FIRE_ASSAILANT, nullptr, nullptr, AUDIOLIB_CUE_PRIORITY_VOICE_AMBIENT, false, false);
        }
        np->npc_flags.maneuver_required = 1;
        return SHOT_RESULT_BAD_SHOT;
    }

    // Original bug kept: target_obj is dereferenced with no check here.
    vector bmin, bmax;
    target_obj->get_local_bbox(bmin, bmax);
    vector center;
    center.x = target_obj->pos.x + (bmax.x + bmin.x) * 0.5f;
    center.y = target_obj->pos.y + (bmin.y + bmax.y) * 0.5f;
    center.z = target_obj->pos.z + (bmax.z + bmin.z) * 0.5f;
    float half_height = (bmax.y - bmin.y) * 0.5f;
    const float half_width = vec_xz_dist(bmin, bmax) * 0.5f;
    const float y_error = fire_info.hit_pos.y - center.y;
    const float xz_error_sq = vec_xz_dist_squared(center, fire_info.hit_pos);
    if (fabsf(y_error) < half_height + 0.5f && xz_error_sq < (half_width + 0.5f) * (half_width + 0.5f))
    {
        return SHOT_RESULT_GOOD_SHOT;
    }

    vector dir;
    const float shot_length = vec_normalized_dir(dir, fire_info.hit_pos, fire_info.start_pos);
    const float target_dist = vec_dist(fire_info.start_pos, center);
    if (mode != ANALYZE_SHOT_EXACT_LOCATION)
    {
        // Did the line of the shot pass over the target at all?
        const float dx = fire_info.start_pos.x + dir.x * target_dist - center.x;
        const float dz = fire_info.start_pos.z + dir.z * target_dist - center.z;
        float radius = half_width;
        if (mode == ANALYZE_SHOT_LAST_LOCATION)
        {
            half_height += 2.0f;
            radius = half_width + 2.0f;
        }
        if (half_height < fabsf(fire_info.start_pos.y + dir.y * target_dist - center.y) || radius * radius < dx * dx + dz * dz)
        {
            return SHOT_RESULT_NEUTRAL;
        }
    }
    if (fire_info.hit_shape_key != 0xFFFFFFFF && hit_obj != nullptr && hit_obj->get_obj_type() == OT_MOVER && hit_obj->get_sub_type() == OT_SUB_MOVER_RFG
        && ((const rfg_mover*)hit_obj)->is_shatterable(fire_info.hit_shape_key))
    {
        return mode == ANALYZE_SHOT_ACTUAL_BULLET ? SHOT_RESULT_NEUTRAL : SHOT_RESULT_GOOD_SHOT;
    }
    float min_length = target_dist * 0.2f;
    if (min_length - 2.0f < 0.0f)
    {
        min_length = 2.0f;
    }
    if (shot_length < min_length)
    {
        return SHOT_RESULT_BAD_SHOT;
    }
    return half_width + 2.0f <= target_dist - shot_length ? SHOT_RESULT_NEUTRAL : SHOT_RESULT_NEAR_MISS;
}

// 0x006B8AE0 (:7374)
void ai_perception::explosion_observed(const human* attacker, const vector& pos, const explosion_info* e_info)
{
    const float dist = vec_dist(np->pos, pos);
    if (e_info->m_ai_sound_radius < dist)
    {
        return;
    }
    const float relevance = calculate_simple_relevance(dist, e_info->m_flinch_radius, e_info->m_ai_sound_radius);
    const bool had_position = belief_bins[AI_BELIEF_ENEMY_POSITION].beliefs != nullptr;
    const bool had_explosion = belief_bins[AI_BELIEF_EXPLOSION].beliefs != nullptr;
    const unsigned int attacker_handle = attacker->get_handle();
    const ai_belief* known = find_belief(belief_bins[AI_BELIEF_ENEMY_EXISTS].beliefs, attacker_handle);
    if (known == nullptr)
    {
        known = find_belief(belief_bins[AI_BELIEF_EXPLOSION].beliefs, attacker_handle);
    }
    if (relevance > 10.0f && known == nullptr && belief_bins[AI_BELIEF_CLOSEST_ALLY].beliefs != nullptr)
    {
        np->play_voice_line(VOICE_LINE_REACT_DISTURBANCE_GROUP, nullptr, nullptr, AUDIOLIB_CUE_PRIORITY_VOICE_AMBIENT, false, false);
    }
    const ai_belief* explosion = update_or_add_belief(AI_BELIEF_EXPLOSION, attacker, pos, relevance, 0, AI_BELIEF_UPDATE_OVERWRITE);
    if (dist < 20.0f && update_or_add_belief(AI_BELIEF_SHOULD_ENGAGE_ENEMY, attacker, vec_dist_squared(attacker->pos, np->pos), 0, AI_BELIEF_UPDATE_OVERWRITE) != nullptr)
    {
        consider_switching_targets(attacker);
    }
    if (explosion != nullptr)
    {
        const bool ignore = human_team_is_non_combatant(np) ? relevance >= 15.0f : known != nullptr;
        if (!ignore)
        {
            ai->planner.request_replan();
        }
    }

    if (human_teams_get_disposition(attacker, np) == TEAM_DISPOSITION_HOSTILE)
    {
        ai->increase_tension_level(3);
        suggest_enemy_exists(attacker, pos, EXPLOSION_ATTACKER_RELEVANCE, false);
        if (!had_position)
        {
            notice_humans_time.next.set(2000);
        }
    }
    else if (!had_position && !had_explosion && explosion != nullptr)
    {
        const ai_goal* goal = ai->planner.get_current_goal();
        const ai_plan_conditions fleeing = ai_plan_conditions(EXPLOSION_CONDITION_A) | ai_plan_conditions(EXPLOSION_CONDITION_B);
        if (goal == nullptr || !goal->pursues_condition(fleeing))
        {
            ai->request_reaction(explosion, AI_REACTION_EXPLOSION, -1);
            ai->planner.request_replan();
        }
    }
}

// 0x006B8E10 (:7489)
void ai_perception::pain_sound_heard(const human* target, const human* attacker, const weapon_info* w_info)
{
    if (target == nullptr)
    {
        return;
    }
    if (morale_use_sound_propagation_spontaneous_conversion() && human_teams_get_disposition(np, target) == TEAM_DISPOSITION_ALLIED)
    {
        morale_maybe_convert_civilian_spontaneously(np, attacker, 0.5f);
    }
    if (np == attacker)
    {
        if (target->is_killed() && human_teams_get_disposition(target, np) == TEAM_DISPOSITION_HOSTILE
            && np->current_team == HUMAN_TEAM_MARAUDER && int_rand_chance(Marauder_kill_taunt_chance))
        {
            ai->request_reaction(target, AI_REACTION_KILL_TAUNT, 1000);
            ai->planner.request_replan();
        }
        return;
    }

    const player* pp = player_get_local_player();
    if (pp == nullptr)
    {
        return;
    }
    // At green alert the player hurting a friend of this npc counts as friendly fire too.
    if (gsm_get_alert_level() == ALERT_LEVEL_GREEN && (int)human_teams_get_disposition(np->current_team, pp->current_team) > 2
        && human_teams_are_friendly(np->current_team, target->current_team))
    {
        const npc* target_npc = World.object_cast<npc>(const_cast<human*>(target));
        if (((target_npc != nullptr && target_npc->get_ai()->perception.player_friendly_fire.is_hostile()) || human_is_dead(target)) && attacker == player_get_local_player())
        {
            player_friendly_fire.register_fire(-1.0f, np, attacker);
        }
    }

    vector sound_pos = target->pos;
    sound_pos.y += HUMAN_HEIGHT * 0.75f + 0.0625f; // 1.4875 m, the chest
    const float dist = vec_dist(sound_pos, np->pos);
    const float relevance = calculate_simple_relevance(dist, PAIN_SOUND_NEARBY_DISTANCE, PAIN_SOUND_MAX_DIST);
    update_or_add_belief(AI_BELIEF_COMBAT_SOUND, target, sound_pos, relevance, 0, AI_BELIEF_UPDATE_OVERWRITE);

    const human_team_dispositions target_disposition = get_effective_disposition(target, true);
    const int attacker_disposition = (attacker == nullptr || attacker == np) ? TEAM_DISPOSITION_NONE : human_teams_get_disposition(np, attacker);
    if ((int)target_disposition < 1)
    {
        // An enemy was hurt.
        note_possible_enemy_location(target, PAIN_SOUND_ATTACKER_RELEVANCE, true);
        if (target->is_killed() && attacker_disposition > 3)
        {
            np->play_voice_line(VOICE_LINE_COMBAT_CONGRATULATE_ON_KILL, nullptr, nullptr, AUDIOLIB_CUE_PRIORITY_VOICE_AMBIENT, false, false);
        }
        if (w_info != nullptr && object_is_player(attacker))
        {
            maybe_react_to_player_weapon((const player*)attacker, w_info);
        }
        return;
    }
    if ((int)target_disposition <= 2)
    {
        return;
    }
    // A friend was hurt.
    if (attacker_disposition < 1)
    {
        suggest_enemy_exists(attacker, target->pos, PAIN_SOUND_ATTACKER_RELEVANCE, false);
        if (update_or_add_belief(AI_BELIEF_SHOULD_ENGAGE_ENEMY, attacker, vec_dist_squared(np->pos, attacker->pos), 0, AI_BELIEF_UPDATE_OVERWRITE) != nullptr)
        {
            consider_switching_targets(attacker);
        }
        const ai_belief* ally = belief_bins[AI_BELIEF_CLOSEST_ALLY].beliefs;
        if (ally != nullptr && ally->handle != target->get_handle())
        {
            if (!human_is_dead(target) || target->hflags.corpse_spotted != 0)
            {
                np->play_voice_line(VOICE_LINE_REACT_DISTURBANCE_GROUP, nullptr, nullptr, AUDIOLIB_CUE_PRIORITY_VOICE_AMBIENT, false, false);
                return;
            }
            vector head;
            human_get_head_pos(target, head);
            if (has_los_to_position(head, false))
            {
                const_cast<human*>(target)->hflags.corpse_spotted = 1;
                np->play_voice_line(VOICE_LINE_COMBAT_ALLY_KILLED, nullptr, nullptr, AUDIOLIB_CUE_PRIORITY_VOICE_AMBIENT, false, false);
            }
        }
    }
    else if (attacker_disposition > 3 && human_teams_are_friendly(attacker, target) && w_info != nullptr && w_info->is_firearm())
    {
        np->play_voice_line(VOICE_LINE_COMBAT_FRIENDLY_FIRE_OBSERVED, nullptr, nullptr, AUDIOLIB_CUE_PRIORITY_VOICE_AMBIENT, false, false);
    }
}

// 0x006671C0 (:7636)
void ai_perception::misc_sound_heard(const human* source, const vector& pos)
{
    if (source == np)
    {
        return;
    }
    update_or_add_belief(AI_BELIEF_MISC_SOUND, source, pos, vec_dist_squared(np->pos, pos) * 10.0f / 100.0f, 0, AI_BELIEF_UPDATE_OVERWRITE);
    attention.update_target_timer.set(0);
    attention.current_target_timer.set(0);
    attention.right_side_timer.set(100);
    attention.left_side_timer.set(100);
    attention.center_timer.set(0);
}

// Civilians with no disturbance yet replan on the first one.
static bool undisturbed(const ai_belief* explosion, const ai_belief* combat_sound, const ai_belief* bullet_impact, const ai_belief* damage_taken)
{
    return explosion == nullptr && combat_sound == nullptr && bullet_impact == nullptr && damage_taken == nullptr;
}

// 0x006BE230 (:7665)
void ai_perception::weapon_sound_heard(const human* attacker, const weapon_info* w_info)
{
    if (attacker == nullptr || w_info == nullptr)
    {
        return;
    }
    if (human_team_is_non_combatant(np)
        && undisturbed(belief_bins[AI_BELIEF_EXPLOSION].beliefs, belief_bins[AI_BELIEF_COMBAT_SOUND].beliefs, belief_bins[AI_BELIEF_BULLET_IMPACT].beliefs, belief_bins[AI_BELIEF_DAMAGE_TAKEN].beliefs))
    {
        ai->planner.request_replan();
    }
    vector sound_pos = attacker->pos;
    sound_pos.y += 1.4875001f;
    const float radius = w_info->firing_sound_radius;
    const float dist = vec_dist(sound_pos, np->pos);
    float nearby = radius * 0.5f;
    if (0.0f <= nearby - 15.0f)
    {
        nearby = 15.0f;
    }
    const float relevance = calculate_simple_relevance(dist, nearby, radius);
    unsigned int flags = relevance <= 10.0f ? BF_NEARBY : 0;
    if (w_info->ammo_type == WEAPON_AMMO_TYPE_PROJECTILE)
    {
        flags |= BF_PROJECTILE_THREAT;
    }
    update_or_add_belief(AI_BELIEF_COMBAT_SOUND, attacker, sound_pos, relevance, flags, AI_BELIEF_UPDATE_OVERWRITE);

    if (human_teams_get_disposition(np, attacker) == TEAM_DISPOSITION_ALLIED)
    {
        // Allies join in on the same target; the player's within 15 m get an order to engage it.
        const ai_belief* enemy_belief = acquire_ally_combat_target(attacker);
        if (enemy_belief != nullptr && object_is_player(attacker) && vec_dist_squared(attacker->pos, np->pos) <= 225.0f)
        {
            const human* enemy = World.object_from_handle<human>(enemy_belief->handle);
            if (enemy != nullptr)
            {
                const ai_belief* old_engage = belief_bins[AI_BELIEF_SHOULD_ENGAGE_ENEMY].beliefs;
                if (update_or_add_belief(AI_BELIEF_SHOULD_ENGAGE_ENEMY, enemy, vec_dist_squared(np->pos, enemy->pos), 0, AI_BELIEF_UPDATE_OVERWRITE) != nullptr)
                {
                    // Original bug kept: it considers switching to the attacker (the player), not to the enemy.
                    consider_switching_targets(attacker);
                }
                if (old_engage == nullptr)
                {
                    ai->planner.request_replan();
                    ai->request_reaction(enemy_belief, AI_REACTION_ALLY_FIRED, -1);
                }
            }
        }
        return;
    }
    if (human_teams_get_disposition(attacker, np) != TEAM_DISPOSITION_HOSTILE)
    {
        if (morale_use_sound_propagation_spontaneous_conversion())
        {
            morale_maybe_convert_civilian_spontaneously(np, attacker, 0.5f);
        }
        return;
    }

    const ai_belief* first_exists = belief_bins[AI_BELIEF_ENEMY_EXISTS].beliefs;
    const bool new_threat = first_exists == nullptr || FIREARM_ATTACKER_RELEVANCE < first_exists->get_relevance();
    const ai_belief* exists = note_possible_enemy_location(attacker, (flags & BF_NEARBY) != 0 ? SIGHTED_ENEMY_RELEVANCE : FIREARM_ATTACKER_RELEVANCE, true);
    bool react = true;
    if (w_info->ammo_type == WEAPON_AMMO_TYPE_PROJECTILE && w_info->projectile_info.start_speed <= 50.0f)
    {
        // A slow projectile: if it noticed it, reacting to that is enough.
        projectile* proj = World.object_from_handle<projectile>(attacker->combat.last_projectile_handle);
        if (proj != nullptr)
        {
            notice_projectile(proj, nullptr, 0);
            if (get_belief(AI_BELIEF_PROJECTILE, proj) != nullptr)
            {
                react = false;
            }
        }
    }
    if (exists != nullptr && new_threat && react)
    {
        ai->request_reaction(exists, AI_REACTION_WEAPON_FIRE, -1);
    }
    if (w_info->is_firearm())
    {
        update_shot_at_by_enemy(attacker, w_info);
    }
    if (object_is_player(attacker))
    {
        maybe_react_to_player_weapon((const player*)attacker, w_info);
        set_last_hostile_action_pos(attacker->pos);
    }
}

// 0x006BE6A0 (:7777)
void ai_perception::bullet_sound_heard(const human* attacker, const weapon_info* w_info, const vector& pos)
{
    const human_team_dispositions disposition = human_teams_get_disposition(np, attacker);
    if ((int)disposition > 3)
    {
        const human* target = human_combat_get_target(attacker);
        if (target != nullptr && !target->is_killed() && !alm_grace_is_active(np, target))
        {
            note_possible_enemy_location(target, SIGHTED_ENEMY_RELEVANCE, true);
        }
        return;
    }
    if (human_team_is_non_combatant(np))
    {
        const bool was_undisturbed = undisturbed(belief_bins[AI_BELIEF_EXPLOSION].beliefs, belief_bins[AI_BELIEF_COMBAT_SOUND].beliefs, belief_bins[AI_BELIEF_BULLET_IMPACT].beliefs, belief_bins[AI_BELIEF_DAMAGE_TAKEN].beliefs);
        update_or_add_belief(AI_BELIEF_BULLET_IMPACT, attacker, pos, calculate_simple_relevance(vec_dist(np->pos, pos), 6.0f, 12.0f), 0, AI_BELIEF_UPDATE_IF_MORE_RELEVANT);
        if (was_undisturbed)
        {
            ai->planner.request_replan();
        }
        return;
    }
    if ((int)disposition > 0)
    {
        return;
    }

    const ai_belief* engage = update_or_add_belief(AI_BELIEF_SHOULD_ENGAGE_ENEMY, attacker, vec_dist_squared(attacker->pos, np->pos), 0, AI_BELIEF_UPDATE_OVERWRITE);
    const float dist = vec_dist(np->pos, pos);
    if (dist > 6.0f)
    {
        update_or_add_belief(AI_BELIEF_BULLET_IMPACT, attacker, pos, (dist - 6.0f) * 10.0f / 6.0f + 10.0f, 0, AI_BELIEF_UPDATE_IF_MORE_RELEVANT);
        // It guesses the shooter is some way back along the shot.
        vector to_attacker;
        to_attacker.x = attacker->pos.x - pos.x;
        to_attacker.y = attacker->pos.y - pos.y;
        to_attacker.z = attacker->pos.z - pos.z;
        float along = to_attacker.mag_and_normalize_safe();
        float guess = along * 0.5f;
        if (guess - 10.0f < 0.0f)
        {
            guess = 10.0f;
        }
        if (0.0f <= along - guess)
        {
            along = guess;
        }
        vector guess_pos;
        guess_pos.x = pos.x + to_attacker.x * along;
        guess_pos.y = pos.y + to_attacker.y * along;
        guess_pos.z = pos.z + to_attacker.z * along;
        suggest_enemy_exists(attacker, guess_pos, LOW_BULLET_SOUND_ATTACKER_RELEVANCE, false);
        return;
    }
    update_or_add_belief(AI_BELIEF_BULLET_IMPACT, attacker, pos, dist * 10.0f / 6.0f, BF_NEARBY, AI_BELIEF_UPDATE_IF_NEWER);
    if (belief_bins[AI_BELIEF_DAMAGE_TAKEN].beliefs == nullptr && belief_bins[AI_BELIEF_CLOSEST_ALLY].beliefs != nullptr)
    {
        np->play_voice_line(VOICE_LINE_REACT_DISTURBANCE_GROUP, nullptr, nullptr, AUDIOLIB_CUE_PRIORITY_VOICE_AMBIENT, false, false);
    }
    const ai_belief* first_position = belief_bins[AI_BELIEF_ENEMY_POSITION].beliefs;
    ai_belief* exists = note_possible_enemy_location(attacker, SIGHTED_ENEMY_RELEVANCE, true);
    if (exists == nullptr)
    {
        return;
    }
    if (engage != nullptr)
    {
        consider_switching_targets(attacker);
    }
    if (first_position == nullptr)
    {
        exists->flags |= BF_SHOT_AT_ME;
        ai->request_reaction(exists, AI_REACTION_WEAPON_FIRE, -1);
        ai->planner.request_replan();
    }
}

// 0x006B9270 (:7864)
void ai_perception::register_hit(const human* attacker, float damage_taken, const weapon_info* w_info)
{
    if (attacker == nullptr || attacker == np || damage_taken <= 0.0f)
    {
        return;
    }
    if (object_is_local_player(attacker) && w_info != nullptr)
    {
        player_friendly_fire.register_fire(damage_taken, np, attacker);
    }
    cover_node* cover = human_combat_cover_get_node(np);
    if (cover != nullptr && human_combat_cover_is_stepped_in(np) && cover->register_hit(attacker))
    {
        ai->planner.request_replan();
    }
    if (human_team_is_non_combatant(np)
        && undisturbed(belief_bins[AI_BELIEF_EXPLOSION].beliefs, belief_bins[AI_BELIEF_COMBAT_SOUND].beliefs, belief_bins[AI_BELIEF_BULLET_IMPACT].beliefs, belief_bins[AI_BELIEF_DAMAGE_TAKEN].beliefs))
    {
        ai->planner.request_replan();
    }

    const float relevance = damage_taken / (float)np->max_hit_points * 20.0f;
    update_or_add_belief(AI_BELIEF_LAST_DAMAGE_TAKEN, attacker, np->pos, relevance, 0, AI_BELIEF_UPDATE_OVERWRITE);
    if (update_or_add_belief(AI_BELIEF_DAMAGE_TAKEN, attacker, relevance, 0, AI_BELIEF_UPDATE_OVERWRITE) == nullptr)
    {
        return;
    }
    if (human_teams_get_disposition(attacker, np) == TEAM_DISPOSITION_HOSTILE)
    {
        if (w_info == nullptr || w_info->is_explosive())
        {
            suggest_enemy_exists(attacker, np->pos, EXPLOSION_ATTACKER_RELEVANCE, false);
        }
        else
        {
            note_possible_enemy_location(attacker, SIGHTED_ENEMY_RELEVANCE, true);
        }
        if (update_or_add_belief(AI_BELIEF_SHOULD_ENGAGE_ENEMY, attacker, vec_dist_squared(np->pos, attacker->pos), 0, AI_BELIEF_UPDATE_OVERWRITE) != nullptr)
        {
            consider_switching_targets(attacker);
        }
    }
    else if (human_teams_are_allies(attacker, np))
    {
        update_or_add_belief(AI_BELIEF_FRIENDLY_FIRE, attacker, 50.0f / damage_taken, 0, AI_BELIEF_UPDATE_IF_MORE_RELEVANT);
    }
}

// 0x005EEDF0 (:7950)
void ai_perception::threw_grenade(const projectile* proj)
{
    if (belief_bins[AI_BELIEF_CLOSEST_ALLY].beliefs != nullptr)
    {
        np->play_voice_line(VOICE_LINE_COMBAT_THROW_GRENADE_GROUP, nullptr, nullptr, AUDIOLIB_CUE_PRIORITY_VOICE_AMBIENT, false, false);
    }
}

// 0x00667260 (:7961)
void ai_perception::dropped_grenade(const projectile* proj)
{
    update_or_add_belief(AI_BELIEF_HAZARD, proj, 0.1f, BF_PROJECTILE_THREAT, AI_BELIEF_UPDATE_OVERWRITE);
    ai->planner.request_replan();
}

// 0x006672A0 (:7972)
void ai_perception::register_taunt(const human* taunter)
{
    const float relevance = get_human_belief_relevance(taunter);
    update_or_add_belief(AI_BELIEF_UNFRIENDLY, taunter, relevance, (relevance > 10.0f ? 0 : BF_NEARBY) | get_human_belief_flags(taunter), AI_BELIEF_UPDATE_OVERWRITE);
    ai->request_reaction(taunter, AI_REACTION_TAUNTED, -1);
    ai->planner.request_replan();
}

// 0x00667380 (:7987)
void ai_perception::register_projectile_attached(const projectile* proj)
{
    if (proj->w_info->is_explosive())
    {
        notice_projectiles_time.next.set_rand(120, 160);
    }
    ai_belief* hazard = find_belief(belief_bins[AI_BELIEF_HAZARD].beliefs, proj->get_handle());
    if (hazard != nullptr)
    {
        deprecate_belief(hazard);
    }
}

// 0x006673E0 (:8003)
void ai_perception::register_bomb_set(const projectile* proj)
{
    update_or_add_belief(AI_BELIEF_PROJECTILE, proj, 3.0f, 0, AI_BELIEF_UPDATE_OVERWRITE);
    ai->request_reaction_bomb(proj, AI_REACTION_BOMB_SET, -1);
    ai->planner.request_replan();
}

// 0x00667460 (:8015)
void ai_perception::register_charge_placed(const projectile* proj)
{
    if (proj->w_info->m_explosion_info == nullptr || proj->get_parent() == np->get_handle())
    {
        return;
    }
    const ai_belief* first_projectile = belief_bins[AI_BELIEF_PROJECTILE].beliefs;
    vector center;
    np->get_center_pos(center);
    const float relevance = (vec_dist(center, proj->pos) / 40.0f + 1.0f) * 10.0f;
    const ai_belief* belief = update_or_add_belief(AI_BELIEF_PROJECTILE, proj, relevance, BF_PROJECTILE_SEEN | BF_STATIONARY | BF_EXPLOSIVE, AI_BELIEF_UPDATE_OVERWRITE);
    ai_belief* hazard = find_belief(belief_bins[AI_BELIEF_HAZARD].beliefs, proj->get_handle());
    if (hazard != nullptr)
    {
        deprecate_belief(hazard);
    }
    if (belief != nullptr && first_projectile == nullptr)
    {
        ai->planner.request_replan();
    }
}

// 0x005EEE10 (:8058)
void ai_perception::register_team_change(human_teams old_team, human_teams new_team)
{
    player_friendly_fire.reset(np);
}

// 0x00667540 (:8067)
void ai_perception::building_stress_sound_heard(const rfg_mover* building, float stress_amount)
{
    if (building->percent_destroyed() < 0.05f)
    {
        return;
    }
    if (!human_team_is_non_combatant(np) && building->flags.game_destroyed == 0)
    {
        return;
    }
    const float relevance = calculate_simple_relevance(vec_dist(np->pos, building->pos), 25.0f, 50.0f);
    update_or_add_belief(AI_BELIEF_BUILDING_STRESS, building, relevance, relevance < 10.0f ? BF_NEARBY : 0, AI_BELIEF_UPDATE_OVERWRITE);
    ai_belief* belief = find_belief(belief_bins[AI_BELIEF_BUILDING].beliefs, building->get_handle());
    if (belief != nullptr)
    {
        belief->flags |= BF_UNSAFE;
    }
}

// 0x006B9490 (:8108)
void ai_perception::building_damage_sound_heard(const rfg_mover* building, const human* attacker, const vector& damage_pos)
{
    update_or_add_belief(AI_BELIEF_COMBAT_SOUND, attacker, damage_pos, calculate_simple_relevance(vec_dist(damage_pos, np->pos), 10.0f, 50.0f), 0, AI_BELIEF_UPDATE_OVERWRITE);
    suggest_enemy_exists(attacker, damage_pos, BUILDING_DAMAGE_VANDAL_RELEVANCE, false);
    if (update_or_add_belief(AI_BELIEF_SHOULD_ENGAGE_ENEMY, attacker, vec_dist_squared(np->pos, attacker->pos), 0, AI_BELIEF_UPDATE_OVERWRITE) != nullptr)
    {
        note_possible_enemy_location(attacker, SIGHTED_ENEMY_RELEVANCE, true);
    }
}

// 0x0063EAD0 (:8133)
void ai_perception::building_collapse_heard(const rfg_mover* building)
{
    vector local_pos = np->pos;
    vector bmin, bmax;
    building->get_local_bbox(bmin, bmax);
    vec_change_to_local_coords(local_pos, building->pos, building->orient);
    unsigned int reason;
    if (bbox_find_distance_squared(bmin, bmax, local_pos) > 0.0f)
    {
        if (belief_bins[AI_BELIEF_ENEMY_POSITION].beliefs != nullptr)
        {
            // Busy fighting: the nearer it is the likelier it still reacts, 0.2 to 0.8.
            float chance = 0.2f;
            const ai_belief* belief = find_belief(belief_bins[AI_BELIEF_BUILDING].beliefs, building->get_handle());
            if (belief != nullptr)
            {
                chance = (20.0f - belief->get_relevance()) * 0.6f / 20.0f + 0.2f;
            }
            if (chance < fl_rand())
            {
                return;
            }
        }
        reason = AI_REACTION_EXPLOSION;
    }
    else
    {
        reason = AI_REACTION_INSIDE_COLLAPSE;
    }
    ai->request_reaction(building, reason, 5000);
    ai->planner.request_replan();
}


// 0x006BEB20 (:8185)
void ai_perception::vehicle_damage_heard(const vehicle* target, const human* attacker, const vector& damage_pos, bool vehicle_collision)
{
    if (attacker == nullptr)
    {
        return;
    }
    if (human_teams_get_disposition(np, attacker) == TEAM_DISPOSITION_HOSTILE)
    {
        update_or_add_belief(AI_BELIEF_COMBAT_SOUND, attacker, damage_pos, calculate_simple_relevance(vec_dist(damage_pos, np->pos), 10.0f, 50.0f), 0, AI_BELIEF_UPDATE_OVERWRITE);
        if (human_in_vehicle(np, target))
        {
            if (note_possible_enemy_location(attacker, SIGHTED_ENEMY_RELEVANCE, true) != nullptr)
            {
                update_or_add_belief(AI_BELIEF_SHOULD_ENGAGE_ENEMY, attacker, vec_dist_squared(np->pos, attacker->pos), 0, AI_BELIEF_UPDATE_OVERWRITE);
            }
        }
        else
        {
            suggest_enemy_exists(attacker, damage_pos, VEHICLE_CRASH_RELEVANCE, false);
        }
        if (update_or_add_belief(AI_BELIEF_SHOULD_ENGAGE_ENEMY, attacker, vec_dist_squared(np->pos, attacker->pos), 0, AI_BELIEF_UPDATE_OVERWRITE) != nullptr)
        {
            consider_switching_targets(attacker);
        }
        if (object_is_player(attacker))
        {
            set_last_hostile_action_pos(attacker->pos);
        }
        return;
    }
    if ((int)human_teams_get_disposition(np, attacker) > 1)
    {
        if (!human_in_vehicle_interaction(np, target))
        {
            return;
        }
        const bool attacker_heavy = human_is_in_heavy_vehicle(attacker);
        if (human_is_driving(np))
        {
            voice_lines line = VOICE_LINE_FLINCH;
            if (!vehicle_collision || attacker_heavy)
            {
                if (int_rand_chance(50) && np->voice != nullptr && np->voice->has_line(VOICE_LINE_VEHICLE_ATTACKED))
                {
                    line = VOICE_LINE_VEHICLE_ATTACKED;
                }
            }
            else if (gsm_get_alert_level() == ALERT_LEVEL_GREEN && int_rand_chance(50))
            {
                line = VOICE_LINE_VEHICLE_IMPACTS_VEHICLE;
            }
            np->play_voice_line(line, nullptr, nullptr, AUDIOLIB_CUE_PRIORITY_VOICE_AMBIENT, false, false);
        }
        if (vehicle_collision && !attacker_heavy && target->get_vehicle_damage() <= 0.5f)
        {
            return;
        }
        update_or_add_belief(AI_BELIEF_COMBAT_SOUND, attacker, damage_pos, 10.0f, 0, AI_BELIEF_UPDATE_OVERWRITE);
        return;
    }
    if (np->current_team == HUMAN_TEAM_EDF && object_is_player(attacker))
    {
        np->get_ai()->perception.note_possible_enemy_location(attacker, SIGHTED_ENEMY_RELEVANCE, true);
    }
}

// 0x006B9580 (:8262)
void ai_perception::footstep_sound_heard(const human* target)
{
    if (target == nullptr)
    {
        return;
    }
    const human_team_dispositions disposition = get_effective_disposition(target, true);
    if (disposition == TEAM_DISPOSITION_HOSTILE)
    {
        note_possible_enemy_location(target, SIGHTED_ENEMY_RELEVANCE, true);
    }
    else if (disposition == TEAM_DISPOSITION_UNFRIENDLY)
    {
        alert_unfriendly_target(target);
    }
}

// 0x006B95D0 (:8282)
void ai_perception::register_bumped_into(const human* target)
{
    if (target == nullptr)
    {
        return;
    }
    const human_team_dispositions disposition = get_effective_disposition(target, true);
    if (disposition == TEAM_DISPOSITION_HOSTILE)
    {
        note_possible_enemy_location(target, SIGHTED_ENEMY_RELEVANCE, true);
    }
    else if (disposition == TEAM_DISPOSITION_UNFRIENDLY)
    {
        alert_unfriendly_target(target);
        if (object_is_player(target) && attention.target == target->get_handle())
        {
            // A guard told the player to stop; a bump is the last straw.
            const ai_goal* goal = ai->planner.get_current_goal();
            if ((goal == threaten_civilian_goal::get_instance() || goal == threaten_watch_goal::get_instance())
                && ai->planner.get_time_in_current_goal() > 500 && start_beatdown(target))
            {
                ai->planner.request_replan();
            }
        }
    }
}

// 0x0063EC60 (:8323)
void ai_perception::register_vehicle_is_on_fire(const vehicle* vp)
{
    ai->planner.request_replan();
}

// 0x00667660 (:8341)
void ai_perception::alert_hazard(const npc* other_npc, const object* hazard)
{
    const ai_belief* theirs = other_npc->get_ai()->perception.get_belief(AI_BELIEF_HAZARD, hazard->get_handle());
    if (theirs != nullptr && find_belief(belief_bins[AI_BELIEF_HAZARD].beliefs, hazard->get_handle()) == nullptr)
    {
        add_new_belief(theirs);
        ai->planner.request_replan();
    }
}

// 0x00667730 (:8360)
void ai_perception::alert_collapsing_building(const npc* other_npc, const rfg_mover* building)
{
    const ai_belief* theirs = other_npc->get_ai()->perception.get_belief(AI_BELIEF_BUILDING, building->get_handle());
    if (theirs != nullptr && find_belief(belief_bins[AI_BELIEF_BUILDING].beliefs, building->get_handle()) == nullptr)
    {
        add_new_belief(theirs);
        ai->planner.request_replan();
    }
}

// 0x006B9680 (:8382)
void ai_perception::alert_enemy(const npc* other_npc, const human* enemy)
{
    if (gsm_get_alert_level() == ALERT_LEVEL_GREEN && other_npc != nullptr && other_npc->get_ai()->perception.player_friendly_fire.is_hostile()
        && (int)human_teams_get_disposition(np->current_team, HUMAN_TEAM_GUERILLA) > 2 && enemy == player_get_local_player())
    {
        player_friendly_fire.register_fire(-1.0f, np, enemy);
    }
    const ai_belief* first_position = belief_bins[AI_BELIEF_ENEMY_POSITION].beliefs;
    const ai_belief* exists = note_possible_enemy_location(enemy, SIGHTED_ENEMY_RELEVANCE, true);
    if (first_position == nullptr && exists != nullptr)
    {
        ai->request_reaction(exists, AI_REACTION_WEAPON_FIRE, -1);
    }
    np->combat.pursuit_timestamp.invalidate();
    np->combat.last_pursuit_timestamp.invalidate();
}

// 0x00667800 (:8414)
void ai_perception::alert_unfriendly_target(const human* target)
{
    const float relevance = get_human_belief_relevance(target);
    update_or_add_belief(AI_BELIEF_UNFRIENDLY, target, relevance, (relevance > 10.0f ? 0 : BF_NEARBY) | get_human_belief_flags(target), AI_BELIEF_UPDATE_OVERWRITE);
}

// 0x006B97A0 (:8429)
void ai_perception::grant_belief(const ai_belief* belief)
{
    const ai_belief_types type = belief->get_type();
    object* target = nullptr;
    const ai_belief* existing;
    if (belief->handle == 0xFFFFFFFF)
    {
        existing = belief_bins[type].beliefs;
    }
    else
    {
        target = World.object_from_handle(belief->handle);
        if (target == nullptr)
        {
            return;
        }
        existing = find_belief(belief_bins[type].beliefs, target->get_handle());
    }
    if (existing != nullptr)
    {
        return;
    }
    switch (type)
    {
    case AI_BELIEF_ENEMY_EXISTS:
        if (target != nullptr && target->get_obj_type() == OT_HUMAN)
        {
            suggest_enemy_exists((const human*)target, belief->pos, belief->get_relevance(), false);
        }
        break;
    case AI_BELIEF_ENEMY_POSITION:
    case AI_BELIEF_LAST_CONTACT:
    case AI_BELIEF_FIRST_CONTACT:
        break; // rfg.exe reports an error: these can't be granted
    default:
    {
        ai_belief* granted = add_new_belief(belief);
        if (granted != nullptr)
        {
            granted->flags &= ~(BF_NEARBY | BF_FROM_ORDERS);
        }
        break;
    }
    }
}

// 0x006B98E0 (:8502)
bool ai_perception::force_search(const human* enemy, const vector& pos)
{
    if (belief_bins[AI_BELIEF_ENEMY_EXISTS].beliefs != nullptr || human_teams_get_disposition(enemy, np) != TEAM_DISPOSITION_HOSTILE)
    {
        return false;
    }
    return suggest_enemy_exists(enemy, pos, EXPLOSION_ATTACKER_RELEVANCE, false) != nullptr;
}

// 0x006A73B0 (:8516)
bool ai_perception::force_enemy_knowledge(const human* enemy)
{
    if (human_teams_get_disposition(enemy, np) != TEAM_DISPOSITION_HOSTILE)
    {
        return false;
    }
    return record_enemy_is_visible(enemy) != nullptr;
}

// 0x00667880 (:8532)
bool ai_perception::start_beatdown(const human* target)
{
    if (target != nullptr && target->get_obj_type() == OT_HUMAN && target->get_sub_type() == OT_SUB_HUMAN_NPC)
    {
        const npc* target_npc = (const npc*)target;
        if (target_npc->get_ai()->oppression_action != 0 || target_npc->npc_flags.civilian_has_been_oppressed != 0)
        {
            return false;
        }
        if (vec_xz_dist_squared(np->pos, target->pos) >= 100.0f)
        {
            return false;
        }
    }
    return update_or_add_belief(AI_BELIEF_BEATDOWN_TARGET, target, 10.0f, 0, AI_BELIEF_UPDATE_OVERWRITE) != nullptr;
}

// 0x00667910 (:8562)
bool ai_perception::cancel_beatdown(const human* target)
{
    ai_belief* belief = find_belief(belief_bins[AI_BELIEF_BEATDOWN_TARGET].beliefs, target->get_handle());
    if (belief == nullptr)
    {
        return false;
    }
    deprecate_belief(belief);
    return true;
}

// 0x0063EC80 (:8572)
void ai_perception::pf_dest_obstructed()
{
    ai->stop_moving();
    ai->planner.request_replan();
}

// 0x006B9940 (:8581)
ai_belief* ai_perception::record_los_result_enemy(human* hp, bool result)
{
    if (!result)
    {
        // An npc that cannot hurt this one but is on its way to attack still counts as seen.
        bool seen_anyway = false;
        if (hp != nullptr && hp->get_obj_type() == OT_HUMAN && hp->get_sub_type() == OT_SUB_HUMAN_NPC && !human_combat_allowed_to_damage_target(hp, np))
        {
            const ai_goal* goal = ((npc*)hp)->get_ai()->planner.get_current_goal();
            seen_anyway = goal != nullptr && goal->pursues_condition(ai_plan_conditions(UNDERCOVER_BLOWN_CONDITION_B));
        }
        if (!seen_anyway)
        {
            record_enemy_not_visible(hp);
            return nullptr;
        }
    }
    if (object_is_local_player(hp) && (int)human_teams_get_disposition(np, HUMAN_TEAM_EDF) > 2)
    {
        reinforcement_report_player_pos(np, hp->pos);
        if (((player*)hp)->pflags.jetpack_started != 0)
        {
            np->play_voice_line(VOICE_LINE_REACT_TO_PLAYER_JETPACK, nullptr, nullptr, AUDIOLIB_CUE_PRIORITY_VOICE_AMBIENT, false, false);
        }
    }
    hp->reported_timer.set(2000);
    const ai_belief* first_position = belief_bins[AI_BELIEF_ENEMY_POSITION].beliefs;
    const bool first_sighting = first_position == nullptr || (first_position->flags & BF_SHOT_AT_ME) != 0;
    ai_belief* position = record_enemy_is_visible(hp);
    if (position != nullptr)
    {
        if (ai->desired_search_handle != 0xFFFFFFFF)
        {
            ai_search_found_target(ai->desired_search_handle, hp);
            ai->desired_search_handle = 0xFFFFFFFF;
            ai->planner.request_replan();
        }
        if (first_sighting && can_engage_target_reason(hp) == AI_ENGAGE_ALLOWED && belief_bins[AI_BELIEF_HAZARD].beliefs == nullptr)
        {
            ai->request_reaction(position, AI_REACTION_ALLY_FIRED, -1);
            ai->planner.request_replan();
        }
    }
    return position;
}

// 0x006A73F0 (:8665)
ai_belief* ai_perception::record_los_result_unfriendly(human* hp, bool result)
{
    const human_team_dispositions edf_disposition = human_teams_get_disposition(np, HUMAN_TEAM_EDF);
    ai_belief* existing = find_belief(belief_bins[AI_BELIEF_UNFRIENDLY].beliefs, hp->get_handle());
    if (!result)
    {
        if (existing != nullptr)
        {
            existing->flags |= BF_UNCONFIRMED;
            if (edf_disposition == TEAM_DISPOSITION_ALLIED && (existing->flags & BF_IN_RESTRICTED_AREA) != 0 && ai_defense_is_human_in_restricted_area(hp, true) == nullptr)
            {
                existing->flags &= ~BF_IN_RESTRICTED_AREA;
            }
        }
        return nullptr;
    }

    const float relevance = get_human_belief_relevance(hp);
    const unsigned int flags = get_human_belief_flags(hp) | (relevance > 10.0f ? 0 : BF_NEARBY);
    const bool had_any = belief_bins[AI_BELIEF_ENEMY_EXISTS].beliefs != nullptr || belief_bins[AI_BELIEF_UNFRIENDLY].beliefs != nullptr;
    ai_belief* belief = update_or_add_belief(AI_BELIEF_UNFRIENDLY, hp, relevance, flags, AI_BELIEF_UPDATE_OVERWRITE);
    if (belief == nullptr)
    {
        return nullptr;
    }
    if (edf_disposition == TEAM_DISPOSITION_ALLIED)
    {
        if (ai_defense_is_human_in_restricted_area(hp, true) == nullptr)
        {
            belief->flags &= ~BF_IN_RESTRICTED_AREA;
        }
        else
        {
            // Driving into one is an attack.
            if (human_in_vehicle(hp) != nullptr && ai_defense_is_human_in_restricted_area(hp, false) != nullptr)
            {
                return go_hostile_on_unfriendly_target(belief, hp, nullptr, nullptr, true);
            }
            belief->flags |= BF_IN_RESTRICTED_AREA;
        }
        const float notice_dist = Edf_notice_player_with_obvious_weapon_dist;
        if (object_is_player(hp) && vec_dist_squared(hp->pos, np->pos) <= notice_dist * notice_dist && human_weapons_is_obvious_weapon_equipped(hp)
            && get_belief(AI_BELIEF_BEATDOWN_TARGET, hp) == nullptr && start_beatdown(hp))
        {
            np->play_voice_line(VOICE_LINE_RESTRICTED_SPOT, nullptr, nullptr, AUDIOLIB_CUE_PRIORITY_VOICE_AMBIENT, false, false);
            ai->planner.request_replan();
        }
    }
    if (!had_any)
    {
        ai->planner.request_replan();
    }
    return belief;
}

// 0x006B9B30 (:8731)
ai_belief* ai_perception::record_los_result(human* hp, bool result)
{
    const human_team_dispositions disposition = get_effective_disposition(hp, true);
    if (disposition == TEAM_DISPOSITION_HOSTILE)
    {
        return record_los_result_enemy(hp, result);
    }
    if (disposition == TEAM_DISPOSITION_UNFRIENDLY)
    {
        return record_los_result_unfriendly(hp, result);
    }
    return nullptr;
}

// ---- where it stands

// 0x005EEE80 (:8765)
bool ai_perception::is_on_navmesh(const human* hp, nav_cell** nav_cell_ptr, float terrain_tolerance) const
{
    if (nav_cell_ptr != nullptr)
    {
        *nav_cell_ptr = nullptr;
    }
    nav_cell* ncp = nav_cell_from_handle(hp->pf.last_visited_nav_cell_handle);
    if (ncp == nullptr || (ncp->flags & NAV_CELL_FLAG_DISABLED) != 0)
    {
        return false;
    }
    if (nav_cell_ptr != nullptr)
    {
        *nav_cell_ptr = ncp;
    }
    if (!nav_cell_is_point_in_cell_column(ncp, hp->pos))
    {
        return false;
    }
    float ground;
    return havok_terrain_height(ground, hp->pos.x, hp->pos.z, false, nullptr) && hp->pos.y <= ground + terrain_tolerance;
}

// 0x005EEF40 (:8800)
bool ai_perception::is_on_navmesh(nav_cell** nav_cell_ptr, float terrain_tolerance) const
{
    return is_on_navmesh(np, nav_cell_ptr, terrain_tolerance);
}

// 0x00667950 (:8820)
bool ai_perception::get_nearest_navpoint_or_navcell(nav_cell*& ncp, navpoint*& npp, vector* navmesh_pos_out, float* xz_dist_squared) const
{
    ncp = nullptr;
    npp = nullptr;
    if (xz_dist_squared != nullptr)
    {
        *xz_dist_squared = 0.0f;
    }
    if (navmesh_pos_out != nullptr)
    {
        *navmesh_pos_out = np->pos;
    }

    const nav_cell* last_cell = nav_cell_from_handle(np->pf.last_visited_nav_cell_handle);
    const nav_cell* found_cell = last_cell;
    vector found_pos = np->pos;
    float cell_dist_sq = 0.0f;
    bool on_ground = false;
    if (last_cell != nullptr)
    {
        float ground;
        if (havok_terrain_height(ground, np->pos.x, np->pos.z, false, nullptr))
        {
            on_ground = np->pos.y - ground <= 1.25f;
        }
        const bool in_column = nav_cell_is_point_in_cell_column(last_cell, np->pos);
        if (on_ground && in_column)
        {
            ncp = const_cast<nav_cell*>(last_cell);
            return true;
        }
        if (!in_column)
        {
            vector centroid;
            nav_cell_get_centroid(&centroid, last_cell);
            const nav_cell* out_cell = nullptr;
            nav_cell_find_longest_line_of_sight_no_detour(found_pos, out_cell, centroid, last_cell, np->pos, true, nullptr);
            found_cell = out_cell;
            cell_dist_sq = vec_xz_dist_squared(np->pos, found_pos);
        }
    }

    const ai_belief* closest_building = belief_bins[AI_BELIEF_BUILDING].beliefs;
    const rfg_mover* building = closest_building == nullptr ? nullptr : rfg_mover_from_handle(closest_building->handle);
    float nav_dist_sq = 0.0f;
    float building_dist_sq;
    navpoint* nav = nullptr;
    if (building != nullptr && ai_building_find_dist_squared(building_dist_sq, np->pos, building, 5.0f))
    {
        nav = ai_building_find_closest_navpoint(np->pos, building, true, &nav_dist_sq);
    }
    if (nav == nullptr)
    {
        if (found_cell == nullptr)
        {
            return false;
        }
    }
    else if (found_cell == nullptr || !on_ground || nav_dist_sq < cell_dist_sq)
    {
        npp = nav;
        if (xz_dist_squared != nullptr)
        {
            *xz_dist_squared = nav_dist_sq;
        }
        return true;
    }
    ncp = const_cast<nav_cell*>(found_cell);
    if (navmesh_pos_out != nullptr)
    {
        *navmesh_pos_out = found_pos;
    }
    if (xz_dist_squared != nullptr)
    {
        *xz_dist_squared = cell_dist_sq;
    }
    return true;
}

// 0x00667BF0 (:8938)
bool ai_perception::is_in_closest_building(const human* hp, rfg_mover** building_ptr, float building_tolerance, float terrain_tolerance) const
{
    const float dist_sq = get_distance_squared_to_nearest_building(hp->pos, building_ptr);
    if (dist_sq < 0.0f || building_tolerance * building_tolerance < dist_sq || (building_ptr != nullptr && *building_ptr == nullptr))
    {
        return false;
    }
    // On enabled nav mesh at ground level it is outside even inside the bbox.
    const nav_cell* ncp = nav_cell_from_handle(hp->pf.last_visited_nav_cell_handle);
    if (ncp != nullptr && (ncp->flags & NAV_CELL_FLAG_DISABLED) == 0 && nav_cell_is_point_in_cell_column(ncp, hp->pos))
    {
        float ground;
        if (havok_terrain_height(ground, hp->pos.x, hp->pos.z, false, nullptr) && hp->pos.y <= ground + terrain_tolerance)
        {
            return false;
        }
    }
    return true;
}

// 0x00667CE0 (:8955)
bool ai_perception::is_in_closest_building(rfg_mover** building_ptr, float building_tolerance, float terrain_tolerance) const
{
    return is_in_closest_building(np, building_ptr, building_tolerance, terrain_tolerance);
}

// 0x005EEF60 (:8969)
bool ai_perception::has_clear_line_of_travel(const vector& end_pos, navpoint* start_npp, navpoint* end_npp) const
{
    if (start_npp == end_npp)
    {
        return true;
    }
    if (start_npp->set != end_npp->set)
    {
        return false;
    }
    // An A* over the navpoints, kept inside a range and within 75 degrees of the way there.
    const float to_start = vec_dist(np->pos, start_npp->pos);
    const float from_end = vec_dist(end_pos, end_npp->pos);
    const float between = vec_dist(start_npp->pos, end_npp->pos);
    vector direction;
    direction.y = end_pos.y - np->pos.y;
    direction.z = end_pos.z - np->pos.z;
    direction.x = end_pos.x - np->pos.x;
    const float max_range = direction.mag_and_normalize_safe() + to_start + from_end;

    const unsigned int session = Navpoint_heap.start_session();
    start_npp->heap_data.est_cost_to_goal = between;
    start_npp->heap_data.flags.is_open = 1;
    start_npp->heap_data.arrival_cost = 0.0f;
    start_npp->heap_data.session_id = session;
    Navpoint_heap.insert_cell(start_npp);
    navpoint* current = Navpoint_heap.remove_best_cell();
    while (current != nullptr)
    {
        current->heap_data.flags.is_open = 0;
        if (current == end_npp)
        {
            break;
        }
        for (int i = 0; i < current->num_links; ++i)
        {
            navpoint* link = current->links[i];
            if ((!Navpoint_treat_all_as_enabled && current->link_disabled_by[i] != 0) || link == nullptr
                || (!Navpoint_treat_all_as_enabled && link->heap_data.disabled_by != 0)
                || (link->heap_data.session_id == session && link->heap_data.flags.is_open == 0))
            {
                continue;
            }
            if (vec_dist_squared(np->pos, link->pos) > max_range * max_range)
            {
                continue;
            }
            vector link_dir;
            const float link_length = vec_normalized_dir(link_dir, link->pos, current->pos);
            if (direction.x * link_dir.x + link_dir.y * direction.y + link_dir.z * direction.z < 0.25881904f)
            {
                continue;
            }
            const float cost = current->heap_data.arrival_cost + link_length;
            if (link->heap_data.session_id == session)
            {
                if (cost < link->heap_data.arrival_cost)
                {
                    link->heap_data.arrival_cost = cost;
                    Navpoint_heap.adjust_cell(link);
                }
            }
            else
            {
                link->heap_data.arrival_cost = cost;
                link->heap_data.est_cost_to_goal = vec_dist(link->pos, end_npp->pos);
                link->heap_data.flags.is_open = 1;
                link->heap_data.session_id = session;
                Navpoint_heap.insert_cell(link);
            }
        }
        current = Navpoint_heap.remove_best_cell();
    }
    Navpoint_heap.end_session();
    return current == end_npp;
}

// The distance from pos to the closest navpoint graph edge at a navpoint, less their radii.
static float navpoint_graph_distance(const vector& pos, const navpoint* nav)
{
    float best = vec_dist(pos, nav->pos) - nav->radius;
    for (int i = 0; i < nav->num_links; ++i)
    {
        const navpoint* link = nav->links[i];
        vector closest = ZERO_VECTOR;
        ix_closest_point_on_line_segment(closest, pos, nav->pos, link->pos);
        const float radius = link->radius <= nav->radius ? link->radius : nav->radius;
        const float dist = vec_dist(closest, pos) - radius;
        if (dist < best)
        {
            best = dist;
        }
    }
    return best;
}

// 0x00667D10 (:9107)
bool ai_perception::has_clear_line_of_travel(const human* hp, float max_dist, float y_tolerance) const
{
    const vector& hp_pos = hp->pos;
    if ((double)max_dist * (double)max_dist < (double)vec_dist_squared(hp_pos, np->pos))
    {
        return false;
    }
    const unsigned int handle = hp->get_handle();
    if (!enemy_in_los(handle))
    {
        if (get_belief(AI_BELIEF_ENEMY_EXISTS, handle) != nullptr || !unfriendly_in_los(handle))
        {
            return false;
        }
    }
    const float dx = hp_pos.x - np->pos.x;
    const float dz = hp_pos.z - np->pos.z;
    if (fabsf(hp_pos.y - np->pos.y) < y_tolerance && dz * dz + dx * dx <= 1.44f)
    {
        return true; // within 1.2 m
    }

    nav_cell* my_cell = nullptr;
    nav_cell* hp_cell = nullptr;
    const bool i_am_on_mesh = is_on_navmesh(np, &my_cell, 1.25f);
    const bool hp_on_mesh = is_on_navmesh(hp, &hp_cell, 1.25f);
    if (i_am_on_mesh && hp_on_mesh)
    {
        nav_cell_detour_remove_convex_hull_for_request(np->nav_cell_detour_request_handle);
        nav_cell_detour_remove_convex_hull_for_request(hp->nav_cell_detour_request_handle);
        const compound_hull* hull = nav_cell_detour_is_point_in_compound_hull(np->pos, my_cell);
        if (hull != nullptr)
        {
            // Standing in a detour hull only counts if the way out leads towards the target.
            vector2 my_pos2;
            my_pos2.x = np->pos.x;
            my_pos2.y = np->pos.z;
            vector2 closest;
            int closest_index;
            nav_cell_detour_get_closest_point_on_compound_hull(&closest, &closest_index, my_pos2, hull);
            vector2 to_exit;
            to_exit.x = closest.x - my_pos2.x;
            to_exit.y = closest.y - my_pos2.y;
            to_exit.mag_and_normalize_safe();
            vector2 to_target;
            to_target.x = dx;
            to_target.y = dz;
            to_target.mag_and_normalize_safe();
            if (to_exit.x * to_target.x + to_target.y * to_exit.y < 0.70710677f)
            {
                return false;
            }
        }
        nav_cell_longest_los_input input;
        nav_cell_longest_los_output output;
        input.start_nav_cell = my_cell;
        input.start_pos = np->pos;
        input.flags.use_disabled_nav_cells = 0;
        input.flags.ignore_detour_hulls = 0;
        input.flags.check_start_in_detour_hull = 0;
        input.delta_2d.x = dx;
        input.delta_2d.y = dz;
        nav_cell_find_longest_line_of_sight_point(output, input);
        return output.result == NAV_CELL_LONGEST_LOS_RESULT_REACHED_DESTINATION;
    }

    // In a building: within half a metre of the navpoint graph, for both.
    navpoint* my_nav = nullptr;
    navpoint* hp_nav = nullptr;
    float my_nav_dist_sq = 0.0f;
    float hp_nav_dist_sq = 0.0f;
    bool i_am_on_graph = false;
    if (bb_pf_find_closest_navpoint(np->pos, my_nav, my_nav_dist_sq))
    {
        const float dist = navpoint_graph_distance(np->pos, my_nav);
        my_nav_dist_sq = dist * dist;
        i_am_on_graph = my_nav_dist_sq < 0.25f;
    }
    bool hp_on_graph = false;
    if (bb_pf_find_closest_navpoint(hp_pos, hp_nav, hp_nav_dist_sq))
    {
        const float dist = navpoint_graph_distance(hp_pos, hp_nav);
        hp_nav_dist_sq = dist * dist;
        hp_on_graph = hp_nav_dist_sq < 0.25f;
    }
    if (i_am_on_graph && hp_on_graph)
    {
        return has_clear_line_of_travel(hp_pos, my_nav, hp_nav);
    }

    // Between mesh and graph: from the nearest points on the mesh, if not further than the graph.
    vector my_mesh_pos = np->pos;
    vector hp_mesh_pos = hp_pos;
    if (!i_am_on_mesh && (!bb_pf_find_closest_point_on_nav_mesh(np->pos, my_mesh_pos, my_cell, false) || vec_dist_squared(np->pos, my_mesh_pos) > my_nav_dist_sq))
    {
        return false;
    }
    if (!hp_on_mesh && (!bb_pf_find_closest_point_on_nav_mesh(hp_pos, hp_mesh_pos, hp_cell, false) || vec_dist_squared(hp_pos, hp_mesh_pos) > hp_nav_dist_sq))
    {
        return false;
    }
    const float mesh_dx = hp_mesh_pos.x - my_mesh_pos.x;
    const float mesh_dz = hp_mesh_pos.z - my_mesh_pos.z;
    nav_cell_detour_remove_convex_hull_for_request(np->nav_cell_detour_request_handle);
    nav_cell_detour_remove_convex_hull_for_request(hp->nav_cell_detour_request_handle);
    const compound_hull* hull = nav_cell_detour_is_point_in_compound_hull(np->pos, my_cell);
    if (hull != nullptr)
    {
        vector2 my_pos2;
        my_pos2.x = np->pos.x;
        my_pos2.y = np->pos.z;
        vector2 closest;
        int closest_index;
        nav_cell_detour_get_closest_point_on_compound_hull(&closest, &closest_index, my_pos2, hull);
        // Original bug kept: here neither direction is normalized before the 45 degree test.
        if ((closest.y - my_pos2.y) * dz + (closest.x - my_pos2.x) * dx < 0.70710677f)
        {
            return false;
        }
    }
    nav_cell_longest_los_input input;
    nav_cell_longest_los_output output;
    input.flags.use_disabled_nav_cells = 0;
    input.flags.ignore_detour_hulls = 0;
    input.flags.check_start_in_detour_hull = 0;
    input.start_pos = my_mesh_pos;
    input.start_nav_cell = my_cell;
    input.delta_2d.x = mesh_dx;
    input.delta_2d.y = mesh_dz;
    nav_cell_find_longest_line_of_sight_point(output, input);
    return output.result == NAV_CELL_LONGEST_LOS_RESULT_REACHED_DESTINATION && hp_pos.y - output.end_pos.y <= y_tolerance;
}


// ---- buildings

// 0x0063ECB0 (:9337)
bool ai_perception::building_is_safe(const rfg_mover* building, bool friendly_only) const
{
    if (building->is_killed())
    {
        return false;
    }
    const ai_belief* belief = find_belief(belief_bins[AI_BELIEF_BUILDING].beliefs, building->get_handle());
    if (belief == nullptr)
    {
        return false;
    }
    return (friendly_only ? BELIEF_REQUIREMENTS_SAFE_FRIENDLY : BELIEF_REQUIREMENTS_SAFE).test(belief->flags);
}

// 0x0063ED60 (:9361)
rfg_mover* ai_perception::get_nearest_safe_building(bool friendly_only) const
{
    const ai_belief_requirements& requirements = friendly_only ? BELIEF_REQUIREMENTS_SAFE_FRIENDLY : BELIEF_REQUIREMENTS_SAFE;
    for (const ai_belief* belief = belief_bins[AI_BELIEF_BUILDING].beliefs; belief != nullptr; belief = get_next_belief(belief))
    {
        if (requirements.test(belief->flags))
        {
            return rfg_mover_from_handle(belief->handle);
        }
    }
    return nullptr;
}

// 0x0063EDE0 (:9376)
rfg_mover* ai_perception::get_nearest_unsafe_building() const
{
    for (const ai_belief* belief = belief_bins[AI_BELIEF_BUILDING].beliefs; belief != nullptr; belief = get_next_belief(belief))
    {
        if (BELIEF_REQUIREMENTS_UNSAFE.test(belief->flags))
        {
            return rfg_mover_from_handle(belief->handle);
        }
    }
    return nullptr;
}

// 0x005EF2A0 (:9388)
float ai_perception::get_unsafe_building_dist(const rfg_mover* building) const
{
    return building->flags.game_destroyed != 0 ? 15.0f : 0.1f;
}

// 0x00668640 (:9412)
bool ai_perception::enemy_in_visual_range(const human* target) const
{
    const unsigned int handle = target->get_handle();
    const unsigned int contact_handle = handle == 0xFFFFFFFF ? np->combat.target_handle : handle;
    if (find_belief(belief_bins[AI_BELIEF_FIRST_CONTACT].beliefs, contact_handle) != nullptr)
    {
        return true;
    }
    if (find_belief(belief_bins[AI_BELIEF_ENEMY_EXISTS].beliefs, handle) == nullptr)
    {
        return false;
    }
    view_frustum_parameters params;
    get_view_frustum_params(params, target);
    params.use_fov = false;
    vector target_pos;
    return get_effective_target_pos(target_pos, target, nullptr, nullptr, false) && within_view_frustum(target_pos, params);
}

// 0x0063EE60 (:9448)
float ai_perception::get_min_flee_dist(const human* target) const
{
    const float length = (int)gsm_get_alert_level() > 1 ? Ai_fov_length_high : Ai_fov_length_low;
    return length * get_fov_length_bonus(np, target);
}

// 0x0063EEB0 (:9454)
float ai_perception::get_max_notice_dist(const human* target) const
{
    bool alerted = (int)gsm_get_alert_level() > 1;
    if (!alerted && object_is_player(target) && human_teams_get_disposition(np, HUMAN_TEAM_EDF) == TEAM_DISPOSITION_ALLIED)
    {
        alerted = restricted_area_in_any_region(target->pos, false);
    }
    const float length = alerted ? Ai_fov_length_high : Ai_fov_length_low;
    return length * get_fov_length_bonus(np, target);
}

// 0x0063EF40 (:9470)
bool ai_perception::within_view_frustum(const vector& target_pos) const
{
    gsm_get_alert_level(); // the result is unused
    view_frustum_parameters params;
    params.length = Ai_fov_length_low;
    params.h_angle = (float)((double)Ai_fov_width * FOV_DEG_TO_RAD);
    params.v_angle = (float)((double)Ai_fov_height * FOV_DEG_TO_RAD);
    params.initialized = true;
    params.use_fov = true;
    return within_view_frustum(target_pos, params);
}

// 0x006836E0 (:9485)
bool ai_perception::has_los_to_position(const vector& pos, bool ignore_combat_target) const
{
    raycast_input input;
    input.radius = 0.0f;
    input.m_filter_info = COLLIDE_RAY_DEFAULT;
    input.m_hit_collector_type = raycast_input::COLLECT_CLOSEST_HIT;
    raycast_output output;
    const human* target = ignore_combat_target ? nullptr : human_combat_get_target(np);
    los_raycast_callback_data cb_data(np, target);
    human_get_head_pos(np, input.start);
    input.end = pos;
    input.filter_params = &cb_data;
    input.m_flags = 0xC;
    input.filter_callback = los_raycast_filter_callback;
    return !havok_raycast(input, output);
}

// 0x0063EFC0 (:9518)
bool ai_perception::enemy_in_los(unsigned int enemy_handle) const
{
    if (enemy_handle == 0xFFFFFFFF)
    {
        enemy_handle = np->combat.target_handle;
    }
    return find_belief(belief_bins[AI_BELIEF_FIRST_CONTACT].beliefs, enemy_handle) != nullptr;
}

// 0x0063F000 (:9539)
bool ai_perception::unfriendly_in_los(unsigned int target_handle) const
{
    const ai_belief* belief = find_belief(belief_bins[AI_BELIEF_UNFRIENDLY].beliefs, target_handle);
    return belief != nullptr && (belief->flags & BF_UNCONFIRMED) == 0;
}

// 0x00683820 (:9553)
void ai_perception::evaluate_corpses()
{
    if ((int)gsm_get_alert_level() >= 1)
    {
        return;
    }
    // Two unreported EDF bodies raise the alert.
    int num_bodies = 0;
    belief_obj_iterator<human> corpses;
    for (corpses.init(ai, AI_BELIEF_CORPSE); corpses; ++corpses)
    {
        const human* hp = corpses;
        if (hp->hflags.death_reported == 0 && (int)human_teams_get_disposition(hp, HUMAN_TEAM_EDF) > 2)
        {
            ++num_bodies;
        }
    }
    if (num_bodies > 1)
    {
        gsm_set_alert_level(ALERT_LEVEL_YELLOW);
    }
}

// ---- the frame

// 0x006C2FA0 (:9588)
void ai_perception::do_frame()
{
    const int interval_scale = (ai->flags.reduced_processing ? 1 : 0) + 1;
    player_friendly_fire.do_frame();

    if (belief_cleanup_time.elapsed())
    {
        belief_cleanup_time.set_rand(400, 500);
        for (int type = 0; type < NUM_AI_BELIEF_TYPES; ++type)
        {
            ai_belief_bin& bin = belief_bins[type];
            ai_belief* belief = bin.beliefs;
            while (belief != nullptr)
            {
                ai_belief* next = belief->next != bin.beliefs ? belief->next : nullptr;
                if (belief_should_expire(belief))
                {
                    unlink_belief(bin.beliefs, belief);
                    --bin.count;
                    free_ai_belief(belief);
                }
                belief = next;
            }
        }
    }
    free_ai_belief_list(dead_belief_list);

    check_combat_target();

    if (notice_humans_time.next.elapsed() && Ai_notice_humans_rate_limiter.request())
    {
        const unsigned int old_target = np->combat.target_handle;
        notice_humans();
        notice_humans_time.last.set(0);
        const human_teams team = np->current_team;
        // Civilians look around less often than soldiers.
        const bool civilian = (int)team < 0 || team == HUMAN_TEAM_CIVILIAN || (int)team > 3;
        const int interval = civilian ? int_rand_range(650, 800) : int_rand_range(350, 450);
        notice_humans_time.next.set(interval * interval_scale);
        update_combat_target();
        if (old_target != np->combat.target_handle)
        {
            target_tracking_update_target(human_combat_get_target(np), true);
        }
    }
    if (notice_corpses_time.next.elapsed())
    {
        notice_corpses();
        notice_corpses_time.last.set(0);
        notice_corpses_time.next.set(int_rand_range(1500, 2000) * interval_scale);
        if ((int)human_teams_get_disposition(np, HUMAN_TEAM_EDF) > 2)
        {
            evaluate_corpses();
        }
    }
    if (notice_objects_time.next.elapsed())
    {
        notice_objects();
        notice_objects_time.last.set(0);
        notice_objects_time.next.set(int_rand_range(400, 600) * interval_scale);
    }
    if (notice_projectiles_time.next.elapsed())
    {
        notice_projectiles();
        notice_projectiles_time.last.set(0);
        notice_projectiles_time.next.set(int_rand_range(240, 320) * interval_scale * 2 / 3);
    }

    if (was_in_closest_building)
    {
        const float dist_sq = get_distance_squared_to_nearest_building(np->pos, nullptr);
        bool left = dist_sq < 0.0f || dist_sq > 0.0f;
        if (!left)
        {
            const nav_cell* ncp = nav_cell_from_handle(np->pf.last_visited_nav_cell_handle);
            if (ncp != nullptr && (ncp->flags & NAV_CELL_FLAG_DISABLED) == 0 && nav_cell_is_point_in_cell_column(ncp, np->pos))
            {
                float ground;
                left = havok_terrain_height(ground, np->pos.x, np->pos.z, false, nullptr) && np->pos.y <= ground + 1.25f;
            }
        }
        if (left)
        {
            notice_buildings_time.next.set(0);
            was_in_closest_building = false;
        }
    }
    if (notice_buildings_time.next.elapsed() && notice_buildings())
    {
        notice_buildings_time.last.set(0);
        notice_buildings_time.next.set(int_rand_range(1500, 2000));
    }

    const object* target = World.object_from_handle(attention.target);
    weapon* wp = human_weapons_get_weapon(np);
    const weapon_fire_info* fire_info = wp == nullptr ? nullptr : wp->get_new_fire_info();
    if (!situation_allows_shooting(target, wp))
    {
        clear_beliefs(AI_BELIEF_SHOT_RESULT);
        attention.flags.aiming_at_target = false;
        updated_combat_target = false;
        return;
    }
    attention.flags.aiming_at_target = aiming_at(target, wp, true);
    update_can_shoot_at(target, wp, fire_info);
    updated_combat_target = false;
}

// A human's closest point of a path, as a segment count plus a fraction: the lower the sooner.
static bool closest_trajectory_fraction(float& best_fraction, const vector* points, const vector& pos, float radius)
{
    bool found = false;
    for (int segment = 0; segment < 2; ++segment)
    {
        vector hit;
        if (!ix_linesegment_sphere(hit, points[segment], points[segment + 1], pos, radius))
        {
            continue;
        }
        float fraction = vec_dist_squared(points[segment], hit) / vec_dist_squared(points[segment], points[segment + 1]);
        if (fraction - 0.0f < 0.0f)
        {
            fraction = 0.0f;
        }
        if (0.0f <= fraction - 1.0f)
        {
            fraction = 1.0f;
        }
        const float value = (float)segment + fraction;
        if (!found && value < best_fraction)
        {
            best_fraction = value;
            found = true;
        }
        if (found)
        {
            break;
        }
    }
    return found;
}

// 0x00668720 (:9850)
void ai_perception::process_movers()
{
    static int next_mover_idx; // 0x01DDBC08
    const int num_movers = World.get_num_objects<object_mover>();
    if (next_mover_idx >= num_movers)
    {
        next_mover_idx = 0;
    }
    // A fifteenth of them a frame, at least 50, stopping at 5 dangerous ones.
    int num_to_check = num_movers / 15 + 50;
    if (num_movers <= num_to_check)
    {
        num_to_check = num_movers;
    }
    mover_path_info infos[5];
    int num_infos = 0;
    for (int i = 0; i < num_to_check; ++i)
    {
        const object_mover* omp = World.get_object<object_mover>(next_mover_idx);
        if (omp != nullptr && mover_is_dangerous(infos[num_infos], omp))
        {
            ++num_infos;
        }
        next_mover_idx = (next_mover_idx + 1) % num_movers;
        if (num_infos >= 5)
        {
            break;
        }
    }
    if (num_infos < 1)
    {
        return;
    }

    const int num_npcs = World.get_num_objects<npc>();
    for (int n = 0; n < num_npcs; ++n)
    {
        npc* np = World.get_object<npc>(n);
        if (np->is_killed())
        {
            continue;
        }
        ai_perception& perception = np->get_ai()->perception;
        int best_index = -1;
        float best_fraction = FLT_MAX;
        for (int i = 0; i < num_infos; ++i)
        {
            const mover_path_info& info = infos[i];
            const float reach = info.radius + HUMAN_HEIGHT;
            const float dist_sq = vec_dist_squared(np->pos, info.center_of_mass);
            if (dist_sq <= reach * reach)
            {
                best_index = i;
                best_fraction = 0.0f;
                break;
            }
            if (best_index < 0)
            {
                best_fraction = FLT_MAX;
            }
            if (closest_trajectory_fraction(best_fraction, info.trajectory_points, np->pos, reach))
            {
                best_index = i;
            }
            if (info.is_large && dist_sq <= (info.radius + 20.0f) * (info.radius + 20.0f))
            {
                perception.update_or_add_belief(AI_BELIEF_LARGE_DESTRUCTION, info.omp, dist_sq, 0, AI_BELIEF_UPDATE_OVERWRITE);
            }
        }
        if (best_index >= 0)
        {
            perception.add_hazard(infos[best_index].omp, fl_parameter(best_fraction, 0.0f, 2.0f, 0.1f, 10.0f, false));
        }
    }
}

// 0x006B9B80 (:9966)
void ai_perception::process_thrown_objects()
{
    static int next_weapon_index; // 0x01DE44FC
    static timestamp next_process_thrown_objects_time; // 0x01DE4500
    const int num_weapons = World.get_num_objects<weapon>();
    if (num_weapons <= next_weapon_index)
    {
        // A full pass at most every half second.
        if (next_process_thrown_objects_time.valid() && !next_process_thrown_objects_time.elapsed())
        {
            return;
        }
        next_weapon_index = 0;
        next_process_thrown_objects_time.set(500);
    }
    int num_to_check = num_weapons / 15 + 5;
    if (num_weapons <= num_to_check)
    {
        num_to_check = num_weapons;
    }
    thrown_object_path_info infos[2];
    int num_infos = 0;
    for (int i = 0; i < num_to_check; ++i)
    {
        const weapon* wp = World.get_object<weapon>(next_weapon_index);
        if (wp != nullptr && thrown_object_is_dangerous(infos[num_infos], wp))
        {
            ++num_infos;
        }
        ++next_weapon_index;
        if (num_weapons <= next_weapon_index || num_infos > 1)
        {
            break;
        }
    }
    if (num_infos < 1)
    {
        return;
    }

    const int num_npcs = World.get_num_objects<npc>();
    for (int n = 0; n < num_npcs; ++n)
    {
        npc* np = World.get_object<npc>(n);
        if (np->is_killed() || np->hflags.hidden != 0 || !np->rdd.is_loaded() || !np->char_inst->rdd.is_loaded())
        {
            continue;
        }
        ai_perception& perception = np->get_ai()->perception;
        gsm_get_alert_level(); // the result is unused
        view_frustum_parameters params;
        params.h_angle = (float)((double)Ai_fov_width * FOV_DEG_TO_RAD);
        params.v_angle = (float)((double)Ai_fov_height * FOV_DEG_TO_RAD);
        params.length = 12.0f;
        params.initialized = true;
        params.use_fov = true;

        int best_index = -1;
        float best_fraction = FLT_MAX;
        for (int i = 0; i < num_infos; ++i)
        {
            const thrown_object_path_info& info = infos[i];
            if (!perception.within_view_frustum(info.wp->pos, params))
            {
                continue;
            }
            const float reach = info.radius + HUMAN_HEIGHT;
            if ((double)vec_dist_squared(np->pos, info.wp->pos) <= (double)reach * (double)reach)
            {
                best_index = i;
                best_fraction = 0.0f;
                break;
            }
            if (best_index < 0)
            {
                best_fraction = FLT_MAX;
            }
            if (closest_trajectory_fraction(best_fraction, info.trajectory_points, np->pos, reach))
            {
                best_index = i;
            }
        }
        if (best_index < 0)
        {
            continue;
        }
        const thrown_object_path_info& info = infos[best_index];
        if (perception.add_hazard(info.wp, fl_parameter(best_fraction, 0.0f, 2.0f, 0.1f, 10.0f, false)) == nullptr)
        {
            continue;
        }
        const human* culprit = (human*)typed_object_from_handle(info.culprit, OT_HUMAN);
        if (culprit != nullptr && human_teams_get_disposition(culprit, np) == TEAM_DISPOSITION_HOSTILE && info.wp->time_since_fired() <= 5000
            && perception.note_possible_enemy_location(culprit, SIGHTED_ENEMY_RELEVANCE, true) != nullptr
            && perception.update_or_add_belief(AI_BELIEF_SHOULD_ENGAGE_ENEMY, culprit, vec_dist_squared(np->pos, culprit->pos), 0, AI_BELIEF_UPDATE_OVERWRITE) != nullptr)
        {
            perception.consider_switching_targets(culprit);
        }
    }
}

// 0x0063F040 (:10120)
bool ai_perception::situation_allows_shooting(const object* target, const weapon* wp)
{
    if (target == nullptr || wp == nullptr)
    {
        return false;
    }
    const weapon_ammo_type ammo = wp->w_info->ammo_type;
    if (ammo != WEAPON_AMMO_TYPE_BULLET && ammo != WEAPON_AMMO_TYPE_PROJECTILE && ammo != WEAPON_AMMO_TYPE_ELECTRICITY
        && ammo != WEAPON_AMMO_TYPE_REPAIR && ammo != WEAPON_AMMO_TYPE_BLANKS && ammo != WEAPON_AMMO_TYPE_FLAME)
    {
        return false;
    }
    if (target->get_obj_type() == OT_HUMAN && get_belief(AI_BELIEF_ENEMY_EXISTS, target) == nullptr)
    {
        return false;
    }
    if (attention.mode == ATTENTION_AIM_AT_OBJECT || attention.mode == ATTENTION_AIM_AT_BELIEF)
    {
        return true;
    }
    return wp->turret_handle != 0xFFFFFFFF && (human_in_vehicle(np) != nullptr || attention.mode == ATTENTION_LOOK_AT_OBJECT);
}

// 0x006BEDE0 (:10160)
void ai_perception::update_can_shoot_at(const object* target, weapon* wp, const weapon_fire_info* wfip)
{
    if (target == nullptr || wp == nullptr)
    {
        return;
    }
    const weapon_ammo_type ammo = wp->w_info->ammo_type;
    if (ammo != WEAPON_AMMO_TYPE_BULLET && ammo != WEAPON_AMMO_TYPE_PROJECTILE && ammo != WEAPON_AMMO_TYPE_ELECTRICITY
        && ammo != WEAPON_AMMO_TYPE_REPAIR && ammo != WEAPON_AMMO_TYPE_BLANKS && ammo != WEAPON_AMMO_TYPE_FLAME)
    {
        return;
    }

    // A muzzle stuck through a wall counts as a bad shot.
    if (attention.flags.aiming_at_target && wp->turret_handle == 0xFFFFFFFF && !human_combat_cover_is_using_node(np) && check_muzzle_time.elapsed())
    {
        check_muzzle_time.set_rand(300, 500);
        vector muzzle_pos;
        wp->get_muzzle_transform(muzzle_pos, nullptr);
        if (!has_los_to_position(muzzle_pos, true))
        {
            attention.last_decent_shot_time.invalidate();
            update_or_add_belief(AI_BELIEF_SHOT_RESULT, target, target->pos, 10.0f, Shot_result_to_belief_flags[SHOT_RESULT_BAD_SHOT], AI_BELIEF_UPDATE_OVERWRITE);
            muzzle_stuck_time.set(check_muzzle_time.time_until());
            return;
        }
    }
    if (!muzzle_stuck_time.elapsed())
    {
        return;
    }

    shot_results result = SHOT_RESULT_NEUTRAL;
    if (wfip != nullptr)
    {
        result = analyze_shot_result(target, *wfip, ANALYZE_SHOT_ACTUAL_BULLET);
    }
    weapon_fire_info test_shot;
    if (wfip == nullptr || result == SHOT_RESULT_NEUTRAL || result == SHOT_RESULT_NEAR_MISS)
    {
        // No news from a real bullet: test a ray now and then.
        const ai_belief* previous = get_belief(AI_BELIEF_SHOT_RESULT, target);
        int since_previous = -1;
        if (previous != nullptr)
        {
            since_previous = previous->update_time.time_since();
            if (since_previous < attention.aim_update_time)
            {
                return;
            }
        }
        if (!attention.flags.aiming_at_target)
        {
            return;
        }
        vector target_pos;
        bool use_pos_belief;
        if (!get_effective_target_pos(target_pos, target, &use_pos_belief, nullptr, false))
        {
            update_or_add_belief(AI_BELIEF_SHOT_RESULT, target, target->pos, 10.0f, Shot_result_to_belief_flags[SHOT_RESULT_NEUTRAL], AI_BELIEF_UPDATE_OVERWRITE);
            return;
        }
        if (!Ai_update_can_shoot_at_limiter.request())
        {
            return;
        }
        attention.aim_update_time = int_rand_range(350, 500);
        vector muzzle_pos;
        matrix muzzle_orient;
        wp->get_muzzle_transform(muzzle_pos, &muzzle_orient);
        test_shot.w_info = wp->w_info;
        test_shot.start_pos = muzzle_pos;
        test_shot.end_pos = target_pos;
        test_shot.hit_handle = 0xFFFFFFFF;
        weapons_test_bullet(test_shot, np, true, 0.0f, ZERO_VECTOR);
        result = analyze_shot_result(target, test_shot, use_pos_belief ? ANALYZE_SHOT_LAST_LOCATION : ANALYZE_SHOT_EXACT_LOCATION);
        if (previous != nullptr && result != SHOT_RESULT_HIT_TARGET && result != SHOT_RESULT_BAD_SHOT
            && (previous->flags & (BF_SHOT_HIT | BF_SHOT_BAD)) != 0 && since_previous <= 1000)
        {
            return;
        }
        wfip = &test_shot;
    }
    record_shot_result(target, wfip->hit_pos, result);
}

// 0x006BA080 (:10325)
void ai_perception::record_shot_result(const object* target_obj, const vector& hit_pos, shot_results result)
{
    bool hit_enemy = false;
    if (target_obj != nullptr && target_obj->get_obj_type() == OT_HUMAN && human_teams_get_disposition(np, (const human*)target_obj) == TEAM_DISPOSITION_HOSTILE)
    {
        const human* target = (const human*)target_obj;
        if (result == SHOT_RESULT_HIT_TARGET)
        {
            note_possible_enemy_location(target, SIGHTED_ENEMY_RELEVANCE, true);
            update_or_add_belief(AI_BELIEF_LAST_CONTACT, target, get_human_belief_relevance(target), get_human_belief_flags(target), AI_BELIEF_UPDATE_OVERWRITE);
            hit_enemy = true;
        }
        else if ((int)result < 3)
        {
            note_possible_enemy_location(target, SIGHTED_ENEMY_RELEVANCE, true);
        }
    }
    if (hit_enemy || (int)result <= 2)
    {
        attention.last_decent_shot_time.set(0);
    }
    else if (result == SHOT_RESULT_BAD_SHOT)
    {
        attention.last_decent_shot_time.invalidate();
    }
    update_or_add_belief(AI_BELIEF_SHOT_RESULT, target_obj, hit_pos, 10.0f, Shot_result_to_belief_flags[result], AI_BELIEF_UPDATE_OVERWRITE);
}

// 0x00668B50 (:10373)
void ai_perception::update_shot_at_by_enemy(const human* attacker, const weapon_info* w_info)
{
    if (attacker->get_handle() != np->combat.target_handle || attacker->is_killed() || !human_weapons_is_firearm_equipped(attacker)
        || w_info == nullptr || !w_info->is_firearm() || w_info->is_explosive())
    {
        return;
    }
    const ai_belief* shot_at = belief_bins[AI_BELIEF_SHOT_AT].beliefs;
    int since = -1;
    if (shot_at != nullptr && shot_at->handle == attacker->get_handle())
    {
        since = shot_at->update_time.time_since();
        if (since < 500)
        {
            return;
        }
    }
    vector aim_pos;
    if (!enemy_is_aiming_at_me(attacker, &aim_pos))
    {
        return;
    }
    if (since < 0 || since > 3000)
    {
        ai->planner.request_replan();
    }
    vector center;
    np->get_center_pos(center);
    float relevance = vec_dist(aim_pos, center);
    if (0.0f <= 0.1f - relevance)
    {
        relevance = 0.1f;
    }
    update_or_add_belief(AI_BELIEF_SHOT_AT, attacker, aim_pos, relevance, 0, AI_BELIEF_UPDATE_OVERWRITE);
}



// 0x005EF2E0
void ai_perception::maybe_react_to_player_weapon(const player* /*pp*/, const weapon_info* w_info)
{
    // The grinder, arc welder and the class after them.
    if (static_cast<unsigned int>(w_info->weapon_class - WEAPON_CLASS_GRINDER) < 3)
    {
        np->play_voice_line(VOICE_LINE_REACT_TO_PLAYER_WEAPON, nullptr, nullptr, AUDIOLIB_CUE_PRIORITY_VOICE_AMBIENT, false, false);
    }
}

// 0x005EF310
void ai_perception::maybe_react_to_player_jetpack(const player* pp)
{
    if (pp->pflags.jetpack_started != 0)
    {
        np->play_voice_line(VOICE_LINE_REACT_TO_PLAYER_JETPACK, nullptr, nullptr, AUDIOLIB_CUE_PRIORITY_VOICE_AMBIENT, false, false);
    }
}

// 0x0063F0D0
void ai_perception::acknowledge_enemy(const human* enemy)
{
    ai_belief* belief = get_belief(AI_BELIEF_ENEMY_EXISTS, enemy->get_handle());
    if (belief != nullptr)
    {
        belief->flags |= BF_ACKNOWLEDGED;
    }
    belief = get_belief(AI_BELIEF_COMBAT_SOUND, enemy->get_handle());
    if (belief != nullptr)
    {
        belief->flags |= BF_ACKNOWLEDGED;
    }
    belief = get_belief(AI_BELIEF_EXPLOSION, enemy->get_handle());
    if (belief != nullptr)
    {
        belief->flags |= BF_ACKNOWLEDGED;
    }
}

// 0x005EF340
bool ai_perception::on_alert() const
{
    // Disposition above 2: friendly or allied with the EDF.
    if (human_teams_get_disposition(np, HUMAN_TEAM_EDF) > 2 && gsm_get_alert_level() > 0)
    {
        return true;
    }
    return activity_on_alert(np);
}

// 0x006838E0
bool ai_perception::any_enemy_player_in_los() const
{
    belief_obj_iterator<player> it;
    it.init(ai, AI_BELIEF_FIRST_CONTACT);
    return it != nullptr;
}

// 0x00683920
bool ai_perception::has_player_enemy_belief() const
{
    belief_obj_iterator<player> it;
    it.init(ai, AI_BELIEF_ENEMY_EXISTS);
    return it != nullptr;
}

// 0x00668C90
ai_engage_target_results ai_perception::can_engage_target_reason(const human* target) const
{
    bool too_many_attackers = false;
    bool on_patrol_vehicle = false;
    if (!Multiplayer && human_teams_get_disposition(np, HUMAN_TEAM_EDF) == TEAM_DISPOSITION_ALLIED && object_is_local_player(target))
    {
        unsigned int patrol_vehicle = 0xFFFFFFFF;
        if (vrail_get_patrol_vehicle(patrol_vehicle) && patrol_vehicle == target->vehicle_handle)
        {
            on_patrol_vehicle = true;
        }
        else
        {
            too_many_attackers = !player_attack_limiter_request(np);
        }
    }

    if (get_belief(AI_BELIEF_SHOULD_ENGAGE_ENEMY, target->get_handle()) != nullptr)
    {
        return AI_ENGAGE_ALLOWED;
    }
    if (np->hflags.activity_engage != 0 && target->hflags.activity_engage != 0)
    {
        return AI_ENGAGE_ALLOWED;
    }
    if (ai_scripting_is_priority_target(target))
    {
        return AI_ENGAGE_ALLOWED;
    }
    object_squad* squad = typed_object_from_handle<object_squad>(ai->squad_handle, OT_SQUAD);
    const gfm_orders* orders = squad != nullptr ? squad->get_squad_orders() : nullptr;
    if (orders == nullptr)
    {
        return AI_ENGAGE_ALLOWED;
    }

    const gfm_orders_types orders_type = orders->get_orders_type();
    if (orders_type == GFM_ORDERS_TYPE_ATTACK || orders_type == GFM_ORDERS_TYPE_CONVOY || orders_type == GFM_ORDERS_TYPE_SMASH || orders->get_target_object_handle() == target->get_handle())
    {
        return AI_ENGAGE_ALLOWED;
    }

    // A roadblock guard stops vehicles further out than it would otherwise see them.
    if (np->npc_flags.roadblock_guard != 0 && human_in_vehicle(target) != nullptr)
    {
        view_frustum_parameters params;
        get_view_frustum_params(params, target);
        const double range = params.length * 2.5;
        if (vec_dist_squared(np->pos, target->pos) <= range * range)
        {
            return AI_ENGAGE_ALLOWED;
        }
    }

    gfm_alert_levels alert = gsm_get_alert_level();
    if (alert < ALERT_LEVEL_YELLOW && on_alert())
    {
        alert = ALERT_LEVEL_YELLOW;
    }

    player* squad_player = ai->is_in_player_squad(true);
    if (squad_player != nullptr)
    {
        if (World.object_cast<player>(const_cast<human*>(target)) != nullptr)
        {
            return AI_ENGAGE_ALLOWED;
        }
        npc* target_npc = World.object_cast<npc>(const_cast<human*>(target));
        if (target_npc != nullptr)
        {
            ai_system* target_ai = target_npc->get_ai();
            if (target_ai->is_in_player_squad(true) != nullptr)
            {
                return AI_ENGAGE_ALLOWED;
            }
            if (target_ai->perception.get_effective_disposition(squad_player, true) == TEAM_DISPOSITION_HOSTILE && target_ai->perception.can_engage_target_reason(squad_player) == AI_ENGAGE_ALLOWED)
            {
                return AI_ENGAGE_ALLOWED;
            }
            if (human_teams_are_allies(target_npc, HUMAN_TEAM_EDF) && alert <= ALERT_LEVEL_YELLOW)
            {
                return AI_ENGAGE_NOT_ALLOWED_ALERT_LEVEL;
            }
        }
    }

    const object_restricted_area* area = ai_defense_is_human_in_restricted_area(target, false);
    // The asm passes the npc's position to is_inside; the decompile drops it.
    if (orders_type != GFM_ORDERS_TYPE_PATROL && !(orders_type == GFM_ORDERS_TYPE_DEFEND && area != nullptr && area->is_inside(np->pos)))
    {
        const float dist_sq = vec_dist_squared(target->pos, np->pos);
        const vehicle* my_vehicle = World.object_from_handle<vehicle>(np->vehicle_handle);
        const vehicle* target_vehicle = World.object_from_handle<vehicle>(target->vehicle_handle);
        float scale = 1.0f;
        if (my_vehicle != nullptr)
        {
            scale = 2.0f; // 0x012C86B0
        }
        else if (human_combat_is_manning_turret(np))
        {
            scale = 2.5f; // 0x012C86BC
        }
        else if (target_vehicle != nullptr && target_vehicle->get_sub_type() == OT_SUB_VEHICLE_FLYER)
        {
            scale = 2.0f;
        }
        if (on_patrol_vehicle)
        {
            scale = static_cast<float>(scale + 0.5);
        }
        float max_dist = static_cast<float>(Max_engagement_distances[alert] * static_cast<double>(scale));
        // Per the asm the perimeter check is on the DEFEND orders; the decompile shows the alert level there
        // (it mislabels stack slots here).
        if (max_dist * max_dist < dist_sq && (orders_type != GFM_ORDERS_TYPE_DEFEND || ai->outside_defensive_perimeter(target->pos, true, &max_dist)))
        {
            return AI_ENGAGE_NOT_ALLOWED_DISTANCE;
        }
    }
    return too_many_attackers ? AI_ENGAGE_NOT_ALLOWED_TOO_MANY : AI_ENGAGE_ALLOWED;
}

// 0x00669060
void ai_perception::deprecate_all_beliefs_about_object(unsigned int obj)
{
    if (obj != 0xFFFFFFFF)
    {
        for (int type = AI_BELIEF_ENEMY_EXISTS; type < NUM_AI_BELIEF_TYPES; type++)
        {
            deprecate_belief(static_cast<ai_belief_types>(type), obj);
        }
    }
}

// 0x005EF380
void ai_perception::deprecate_all_enemy_beliefs()
{
    clear_beliefs(AI_BELIEF_ENEMY_EXISTS);
    clear_beliefs(AI_BELIEF_ENEMY_POSITION);
    clear_beliefs(AI_BELIEF_FIRST_CONTACT);
    clear_beliefs(AI_BELIEF_LAST_CONTACT);
    clear_beliefs(AI_BELIEF_SHOULD_ENGAGE_ENEMY);
    clear_beliefs(AI_BELIEF_BULLET_IMPACT);
    clear_beliefs(AI_BELIEF_DAMAGE_TAKEN);
    clear_beliefs(AI_BELIEF_LAST_DAMAGE_TAKEN);
    clear_beliefs(AI_BELIEF_EXPLOSION);
    clear_beliefs(AI_BELIEF_COMBAT_SOUND);
    clear_beliefs(AI_BELIEF_TURRET, BELIEF_REQUIREMENTS_OCCUPIED_BY_ENEMY);
    clear_beliefs(AI_BELIEF_VEHICLE, BELIEF_REQUIREMENTS_OCCUPIED_BY_ENEMY);
    np->combat.target_handle = 0xFFFFFFFF;
}

// 0x00669090
void ai_perception::deprecate_all_enemy_beliefs(unsigned int enemy_handle, ai_belief* enemy_exists_belief)
{
    if (enemy_exists_belief == nullptr)
    {
        enemy_exists_belief = get_belief(AI_BELIEF_ENEMY_EXISTS, enemy_handle);
    }
    deprecate_belief(enemy_exists_belief);
    deprecate_belief(AI_BELIEF_ENEMY_POSITION, enemy_handle);
    deprecate_belief(AI_BELIEF_FIRST_CONTACT, enemy_handle);
    deprecate_belief(AI_BELIEF_LAST_CONTACT, enemy_handle);
    deprecate_belief(AI_BELIEF_SHOULD_ENGAGE_ENEMY, enemy_handle);
    deprecate_belief(AI_BELIEF_BULLET_IMPACT, enemy_handle);
    deprecate_belief(AI_BELIEF_DAMAGE_TAKEN, enemy_handle);
}

// 0x005EF470
bool ai_perception::vehicle_is_heavy(const vehicle* vp)
{
    if (vp->get_sub_type() == OT_SUB_VEHICLE_WALKER)
    {
        return true;
    }
    return vehicle_is_tank(vp);
}

// 0x0063F160
bool ai_perception::human_is_in_heavy_vehicle(const human* hp)
{
    const vehicle* vp = typed_object_from_handle<vehicle>(hp->vehicle_handle, OT_VEHICLE);
    return vp != nullptr && vehicle_is_heavy(vp);
}

// 0x00683960
bool ai_perception::enemy_is_being_attacked_from_turret(const human* hp) const
{
    belief_obj_iterator<human> it;
    for (it.init(ai, AI_BELIEF_ALLY_ON_TURRET); it; ++it)
    {
        human* gunner = it;
        const turret* tp = human_combat_get_manned_turret(gunner);
        const weapon* wp = tp != nullptr ? tp->get_weapon_inst() : nullptr;
        if (wp == nullptr)
        {
            continue;
        }
        const weapon_info* w_info = wp->w_info;
        const player* gunner_player = World.object_cast<player>(gunner);
        const npc* gunner_npc = World.object_cast<npc>(gunner);
        vector aim_pos;
        if (gunner_player != nullptr)
        {
            aim_pos = gunner_player->aim_pos;
        }
        else if (gunner_npc != nullptr)
        {
            const human* gunner_target = human_combat_get_target(gunner_npc);
            if (gunner_target == nullptr)
            {
                continue;
            }
            aim_pos = gunner_target->pos;
        }
        float radius = 10.0f;
        if (w_info->is_explosive())
        {
            const float blast = w_info->m_explosion_info->m_knockdown_radius * 2.0f;
            if (blast - 10.0f >= 0.0f)
            {
                radius = blast;
            }
        }
        if (vec_dist_squared(hp->pos, aim_pos) <= radius * radius)
        {
            return true;
        }
    }
    return false;
}

// 0x005EF490
bool ai_perception::is_in_friendly_player_line_of_fire() const
{
    const player* pp = player_get_local_player();
    if (pp == nullptr || human_teams_get_disposition(pp, np) != TEAM_DISPOSITION_ALLIED)
    {
        return false;
    }
    const weapon* wp = pp->get_equipped_weapon();
    if (wp != nullptr && wp->w_info->ammo_type == WEAPON_AMMO_TYPE_MELEE)
    {
        return false;
    }
    const vector aim = player_autoaim_get_aim_vector();
    const vector& start = pp->pos;
    const vector end(start.x + aim.x * 100.0f, start.y + aim.y * 100.0f, start.z + aim.z * 100.0f);
    vector closest;
    ix_closest_point_on_line_segment(closest, np->pos, start, end);
    return vec_dist_squared(closest, np->pos) < 25.0f;
}

// 0x006A7610
void ai_perception::do_attention_frame()
{
    switch (attention.mode)
    {
    case ATTENTION_LOOK_AT_NONE:
    case ATTENTION_AIM_STRAIGHT_AHEAD:
        break;
    case ATTENTION_LOOK_AT_BELIEF:
    case ATTENTION_AIM_AT_BELIEF:
        do_attention_belief_frame();
        break;
    case ATTENTION_LOOK_AT_OBJECT:
    case ATTENTION_AIM_AT_OBJECT:
        do_attention_object_frame();
        break;
    case ATTENTION_IDLE:
        do_attention_idle_frame();
        break;
    case ATTENTION_SEARCH:
    case ATTENTION_SEARCH_COVER_MY_SIX:
        do_attention_search_frame();
        break;
    case ATTENTION_ACQUIRE_VISUAL:
        do_attention_acquire_frame();
        break;
    default:
        break; // Int3 in retail
    }
}

// 0x0063F1A0
ai_belief* ai_perception::get_attention_belief() const
{
    if (attention.belief_type == AI_BELIEF_UNDEFINED)
    {
        return nullptr;
    }
    return get_belief(attention.belief_type, attention.target);
}

// 0x005EF5C0
object* ai_perception::get_attention_target() const
{
    return World.object_from_handle(attention.target);
}

// 0x00683B10
void ai_perception::pay_attention(unsigned int target_handle, attention_look_at_modes look_at_mode, attention_steering_modes steering_mode, attention_awareness_levels awareness_level)
{
    pay_attention(target_handle, AI_BELIEF_UNDEFINED, look_at_mode, steering_mode, awareness_level);
}

// 0x00683B30
void ai_perception::pay_attention(const object* obj, attention_look_at_modes look_at_mode, attention_steering_modes steering_mode, attention_awareness_levels awareness_level)
{
    if (obj != nullptr)
    {
        pay_attention(obj->get_handle(), AI_BELIEF_UNDEFINED, look_at_mode, steering_mode, awareness_level);
    }
}

// 0x00683B60
void ai_perception::pay_attention(const ai_belief* belief, attention_look_at_modes look_at_mode, attention_steering_modes steering_mode, attention_awareness_levels awareness_level)
{
    pay_attention(belief->handle, belief->get_type(), look_at_mode, steering_mode, awareness_level);
}

// 0x006691D0
void ai_perception::pay_attention(unsigned int target_handle, ai_belief_types belief_type, attention_look_at_modes look_at_mode, attention_steering_modes steering_mode, attention_awareness_levels awareness_level)
{
    attention.target = target_handle;
    attention.belief_type = belief_type;
    if (!set_attention_mode(look_at_mode, steering_mode, awareness_level))
    {
        enter_idle_attention();
    }
}

// 0x005EF5E0
void ai_perception::stop_paying_attention(bool reset_spine)
{
    attention.target = 0xFFFFFFFF;
    attention.belief_type = AI_BELIEF_UNDEFINED;
    attention.mode = ATTENTION_LOOK_AT_NONE;
    attention.steering = ATTENTION_STEERING_TURN_TO_TARGET;
    if (reset_spine)
    {
        human_spinebend_reset_target(np);
    }
}

// 0x006692E0
void ai_perception::maybe_look_away_from_corpse()
{
    if (!human_team_is_non_combatant(np))
    {
        return;
    }
    float closest_dist_sq = FLT_MAX;
    const ai_belief* closest = nullptr;
    const ai_belief* head = belief_bins[AI_BELIEF_CORPSE].beliefs;
    const ai_belief* belief = head;
    while (belief != nullptr)
    {
        const float dist_sq = vec_xz_dist_squared(np->pos, belief->pos);
        if (dist_sq < closest_dist_sq)
        {
            closest = belief;
            closest_dist_sq = dist_sq;
        }
        belief = belief->next;
        if (belief == head)
        {
            break;
        }
    }
    if (closest == nullptr || closest_dist_sq > 25.0f)
    {
        return;
    }
    const vector& my_pos = np->pos;
    const matrix& orient = np->orient;
    const float side = orient.rvec.y * (closest->pos.y - my_pos.y) + orient.rvec.x * (closest->pos.x - my_pos.x) + orient.rvec.z * (closest->pos.z - my_pos.z);
    attention.target = 0xFFFFFFFF;
    attention.belief_type = AI_BELIEF_UNDEFINED;
    attention.mode = ATTENTION_LOOK_AT_NONE;
    attention.steering = ATTENTION_STEERING_TURN_TO_TARGET;
    np->hflags.anchor_orient = 0;
    attention.awareness = ATTENTION_AWARENESS_NORMAL;
    human_spinebend_reset_target(np);
    attention.mode = ATTENTION_LOOK_AT_NONE;
    // Forward, down and to the side away from the corpse.
    const float away = side <= 0.0f ? 1.0f : -1.0f;
    vector look;
    look.x = orient.fvec.x + (my_pos.x + orient.rvec.x * away) - orient.uvec.x;
    look.y = orient.fvec.y + (my_pos.y + orient.rvec.y * away) - orient.uvec.y + HUMAN_HEIGHT;
    look.z = orient.fvec.z + (my_pos.z + orient.rvec.z * away) - orient.uvec.z;
    human_set_look_at_target(np, look, false, -1.0f);
}

// Shared by the two search modes of set_attention_mode.
void ai_perception::enter_search_attention(attention_look_at_modes look_at_mode, float ready_dot, float spread_angle)
{
    np->look_at_speed = 0.15f;
    const bool switching = attention.mode == ATTENTION_ACQUIRE_VISUAL || attention.mode == (look_at_mode == ATTENTION_SEARCH ? ATTENTION_SEARCH_COVER_MY_SIX : ATTENTION_SEARCH);
    // 0x2711: at least ten seconds on the current target.
    const bool stale = !attention.current_target_timer.valid() || attention.current_target_timer.time_since() >= 10001;
    if (stale || switching)
    {
        attention.bored_timer.invalidate();
        attention.bored_idle_targets.remove_all();
        attention.flags.check_right = false;
        attention.flags.left_side_clear = false;
        attention.flags.right_side_clear = false;
        attention.update_target_timer.invalidate();
        attention.idle_target = 0xFFFFFFFF;
        attention.idle_belief_type = AI_BELIEF_UNDEFINED;
        attention.current_target_timer.set(0);
        attention.left_side_timer.set(0);
        attention.right_side_timer.set(0);
        attention.center_timer.set(0);
        attention.direction = ATTENTION_DIRECTION_CENTER;
        human_spinebend_reset_target(np);
    }
    attention.idle_not_ready_dot = 0.0f;
    attention.idle_ready_dot = ready_dot;
    attention.mode = look_at_mode;
    attention.search_spread_angle = spread_angle;
}

// 0x0063F1E0
bool ai_perception::set_attention_mode(attention_look_at_modes look_at_mode, attention_steering_modes steering_mode, attention_awareness_levels awareness_level)
{
    if (look_at_mode == ATTENTION_USE_CURRENT_MODE)
    {
        look_at_mode = attention.mode;
    }
    update_attention_steering_mode(look_at_mode, steering_mode);
    if (awareness_level != ATTENTION_AWARENESS_USE_CURRENT_MODE)
    {
        attention.awareness = awareness_level;
    }
    switch (look_at_mode)
    {
    case ATTENTION_LOOK_AT_NONE:
    case ATTENTION_AIM_STRAIGHT_AHEAD:
        human_spinebend_reset_target(np);
        break;
    case ATTENTION_LOOK_AT_BELIEF:
    case ATTENTION_AIM_AT_BELIEF:
        if (attention.belief_type == AI_BELIEF_UNDEFINED || get_belief(attention.belief_type, attention.target) == nullptr)
        {
            return false;
        }
        break;
    case ATTENTION_LOOK_AT_OBJECT:
    case ATTENTION_AIM_AT_OBJECT:
    {
        const object* obj = World.object_from_handle(attention.target);
        if (obj == nullptr)
        {
            return false;
        }
        set_look_at_object(obj, look_at_mode == ATTENTION_AIM_AT_OBJECT, -1.0f, true);
        break;
    }
    case ATTENTION_IDLE:
        if (attention.mode != ATTENTION_IDLE)
        {
            np->look_at_speed = 0.25f;
            if (!attention.update_target_timer.valid() || attention.update_target_timer.time_since() > 5000)
            {
                attention.bored_timer.invalidate();
                attention.bored_idle_targets.remove_all();
                attention.update_target_timer.invalidate();
                attention.idle_target = 0xFFFFFFFF;
                attention.idle_belief_type = AI_BELIEF_UNDEFINED;
                attention.idle_not_ready_dot = human_team_is_non_combatant(np) ? IDLE_READY_DOT : 0.0f;
                attention.idle_ready_dot = IDLE_READY_DOT;
                human_spinebend_reset_target(np);
            }
        }
        break;
    case ATTENTION_SEARCH:
        if (attention.mode != ATTENTION_SEARCH)
        {
            enter_search_attention(look_at_mode, IDLE_READY_DOT, 0.7853982f); // 45 degrees
            return true;
        }
        break;
    case ATTENTION_SEARCH_COVER_MY_SIX:
        if (attention.mode != ATTENTION_SEARCH_COVER_MY_SIX)
        {
            enter_search_attention(look_at_mode, 0.0f, 1.5707964f); // 90 degrees
            return true;
        }
        break;
    case ATTENTION_ACQUIRE_VISUAL:
        if (attention.mode != ATTENTION_ACQUIRE_VISUAL)
        {
            attention.belief_type = AI_BELIEF_ENEMY_POSITION;
            attention.target = np->combat.target_handle;
            if (get_belief(AI_BELIEF_ENEMY_POSITION, attention.target) == nullptr)
            {
                return false;
            }
            np->look_at_speed = 0.15f;
            if (attention.direction == ATTENTION_DIRECTION_CENTER)
            {
                attention.direction = ATTENTION_DIRECTION_TARGET;
            }
            attention.flags.check_right = false;
            attention.flags.left_side_clear = false;
            attention.flags.right_side_clear = false;
            attention.update_target_timer.invalidate();
            attention.current_target_timer.set(0);
            attention.left_side_timer.set(0);
            attention.right_side_timer.set(0);
            attention.center_timer.set(0);
            human_spinebend_reset_target(np);
            attention.direction = ATTENTION_DIRECTION_CENTER; // original overwrites the TARGET set just above
            attention.mode = look_at_mode;
            attention.search_spread_angle = 0.47123894f; // 27 degrees
            return true;
        }
        break;
    default:
        return false; // Int3 in retail
    }
    attention.mode = look_at_mode;
    return true;
}

// 0x005EF620
void ai_perception::deprecate_belief(ai_belief* belief)
{
    if (belief == nullptr)
    {
        return;
    }
    ai_belief_bin& bin = belief_bins[belief->type];
    if (bin.beliefs == belief && belief->next == belief)
    {
        belief->next = nullptr;
        belief->prev = nullptr;
        bin.beliefs = nullptr;
    }
    else
    {
        if (bin.beliefs == belief)
        {
            bin.beliefs = belief->next;
        }
        if (belief->prev != nullptr)
        {
            belief->prev->next = belief->next;
        }
        if (belief->next != nullptr)
        {
            belief->next->prev = belief->prev;
        }
        belief->next = nullptr;
        belief->prev = nullptr;
    }
    bin.count--;
    belief->relevance = -1.0f;
    if (dead_belief_list != nullptr)
    {
        belief->prev = dead_belief_list->prev;
        belief->next = dead_belief_list;
        dead_belief_list->prev->next = belief;
        dead_belief_list->prev = belief;
    }
    else
    {
        belief->next = belief;
        belief->prev = belief;
        dead_belief_list = belief;
    }
}

// 0x0063F6A0
bool ai_perception::deprecate_belief(ai_belief_types type, unsigned int handle)
{
    ai_belief* belief = get_belief(type, handle);
    if (belief == nullptr)
    {
        return false;
    }
    deprecate_belief(belief);
    return true;
}

// 0x0064C9D0
bool ai_perception::deprecate_belief(ai_belief_types type, const object* obj)
{
    return deprecate_belief(type, obj->get_handle());
}

// 0x005EF6E0
int ai_perception::num_beliefs(ai_belief_types type, const ai_belief_requirements& requirements) const
{
    int count = 0;
    const ai_belief* head = belief_bins[type].beliefs;
    const ai_belief* belief = head;
    while (belief != nullptr)
    {
        if (requirements.test(belief->flags))
        {
            count++;
        }
        belief = belief->next;
        if (belief == head)
        {
            break;
        }
    }
    return count;
}

// 0x005DC8A0
int ai_perception::num_beliefs(ai_belief_types type) const
{
    return belief_bins[type].count;
}

// 0x005EF730
int ai_perception::num_relevant_beliefs(ai_belief_types type, float cutoff, const ai_belief_requirements& requirements) const
{
    int count = 0;
    const ai_belief* head = belief_bins[type].beliefs;
    const ai_belief* belief = head;
    while (belief != nullptr)
    {
        if (belief->get_relevance() <= cutoff && requirements.test(belief->flags))
        {
            count++;
        }
        belief = belief->next;
        if (belief == head)
        {
            break;
        }
    }
    return count;
}

// 0x005DC8B0
ai_belief* ai_perception::get_first_belief(ai_belief_types type) const
{
    return belief_bins[type].beliefs;
}

// 0x005EF7A0
ai_belief* ai_perception::get_next_belief(const ai_belief* belief) const
{
    if (belief == nullptr || belief->next == belief_bins[belief->get_type()].beliefs)
    {
        return nullptr;
    }
    return belief->next;
}

// 0x005DC8C0
ai_belief* ai_perception::get_belief(ai_belief_types type) const
{
    return belief_bins[type].beliefs;
}

// 0x005EF7D0
ai_belief* ai_perception::get_belief(ai_belief_types type, unsigned int handle) const
{
    ai_belief* head = belief_bins[type].beliefs;
    ai_belief* belief = head;
    while (belief != nullptr)
    {
        if (belief->handle == handle)
        {
            return belief;
        }
        belief = belief->next;
        if (belief == head)
        {
            return nullptr;
        }
    }
    return nullptr;
}

// 0x005EF800
ai_belief* ai_perception::get_belief(ai_belief_types type, const object* obj) const
{
    return get_belief(type, obj->get_handle());
}

// 0x005EF830
ai_belief* ai_perception::get_belief(ai_belief_types type, const ai_belief_requirements& requirements, object_type obj_type) const
{
    ai_belief* head = belief_bins[type].beliefs;
    ai_belief* belief = head;
    while (belief != nullptr)
    {
        if (requirements.test(belief->flags) && (obj_type == OT_UNDEFINED || belief->get_obj_type() == obj_type))
        {
            return belief;
        }
        belief = belief->next;
        if (belief == head)
        {
            break;
        }
    }
    return nullptr;
}

// 0x005EF890
ai_belief* ai_perception::get_belief(ai_belief_types type, const vector& pos, const ai_belief_requirements& requirements, object_type obj_type) const
{
    ai_belief* closest = nullptr;
    float closest_dist_sq = 0.0f;
    ai_belief* head = belief_bins[type].beliefs;
    ai_belief* belief = head;
    while (belief != nullptr)
    {
        if (requirements.test(belief->flags) && (obj_type == OT_UNDEFINED || belief->get_obj_type() == obj_type))
        {
            const float dist_sq = vec_dist_squared(pos, belief->pos);
            if (closest == nullptr || dist_sq < closest_dist_sq)
            {
                closest = belief;
                closest_dist_sq = dist_sq;
            }
        }
        belief = belief->next;
        if (belief == head)
        {
            break;
        }
    }
    return closest;
}

// 0x005EF950
ai_belief* ai_perception::get_most_recent_belief(ai_belief_types type, const ai_belief_requirements& requirements, object_type obj_type) const
{
    ai_belief* newest = nullptr;
    int newest_age = 0;
    ai_belief* head = belief_bins[type].beliefs;
    ai_belief* belief = head;
    while (belief != nullptr)
    {
        if (requirements.test(belief->flags) && (obj_type == OT_UNDEFINED || belief->get_obj_type() == obj_type))
        {
            const int age = belief->update_time.time_since();
            if (newest == nullptr || age < newest_age)
            {
                newest = belief;
                newest_age = age;
            }
        }
        belief = belief->next;
        if (belief == head)
        {
            break;
        }
    }
    return newest;
}

// 0x005EF9E0
ai_belief* ai_perception::get_recent_belief(ai_belief_types type, int cutoff_msec, const ai_belief_requirements& requirements, object_type obj_type) const
{
    ai_belief* head = belief_bins[type].beliefs;
    ai_belief* belief = head;
    while (belief != nullptr)
    {
        if (requirements.test(belief->flags) && (obj_type == OT_UNDEFINED || belief->get_obj_type() == obj_type) && belief->update_time.time_since() <= cutoff_msec)
        {
            return belief;
        }
        belief = belief->next;
        if (belief == head)
        {
            break;
        }
    }
    return nullptr;
}

// 0x0063F6E0
ai_belief* ai_perception::find_matching_belief(const ai_belief_specification* specifications, int num_specifications, unsigned int obj_override) const
{
    ai_belief* belief = nullptr;
    const int count = collect_all_matching_beliefs(&belief, 1, specifications, num_specifications, obj_override);
    return count >= 1 ? belief : nullptr;
}

// 0x005EFA60
int ai_perception::collect_all_matching_beliefs(ai_belief** beliefs, int max_beliefs, const ai_belief_specification* specifications, int num_specifications, unsigned int obj_override) const
{
    int count = 0;
    for (int i = 0; i < num_specifications; i++)
    {
        const ai_belief_specification& spec = specifications[i];
        // Range check allows 0x1B, one past the bins (original).
        if (spec.type < 0 || spec.type > NUM_AI_BELIEF_TYPES)
        {
            continue; // Int3 in retail
        }
        const unsigned int handle = obj_override != 0xFFFFFFFF ? obj_override : spec.handle;
        if (handle == 0xFFFFFFFF)
        {
            ai_belief* head = belief_bins[spec.type].beliefs;
            ai_belief* belief = head;
            while (belief != nullptr)
            {
                if ((spec.time_since < 0 || belief->update_time.time_since() <= spec.time_since) && spec.requirements.test(belief->flags))
                {
                    beliefs[count++] = belief;
                    if (count >= max_beliefs)
                    {
                        return count;
                    }
                }
                belief = belief->next;
                if (belief == head)
                {
                    break;
                }
            }
        }
        else
        {
            ai_belief* belief = get_belief(spec.type, handle);
            if (belief != nullptr && (spec.time_since < 0 || belief->update_time.time_since() <= spec.time_since) && spec.requirements.test(belief->flags))
            {
                beliefs[count++] = belief;
                if (count >= max_beliefs)
                {
                    return count;
                }
            }
        }
    }
    return count;
}

// 0x00669670
void ai_ignore_player(bool enable)
{
    if (enable == Ai_ignore_player)
    {
        return;
    }
    const player* pp = player_get_local_player();
    if (pp == nullptr)
    {
        return;
    }
    const unsigned int handle = pp->get_handle();
    Ai_ignore_player = enable;
    if (!enable)
    {
        return;
    }
    const int count = World.get_num_objects<npc>();
    for (int i = 0; i < count; i++)
    {
        npc* np = World.get_object<npc>(i);
        if (np != nullptr && !np->is_killed())
        {
            np->get_ai()->perception.deprecate_all_beliefs_about_object(handle);
        }
    }
}

// 0x005DC840
int ai_perception::get_time_since_last_decent_shot() const
{
    if (attention.last_decent_shot_time.valid())
    {
        return attention.last_decent_shot_time.time_since();
    }
    return -1;
}

// 0x005FB630
bool ai_perception::has_enemy_belief() const
{
    return belief_bins[AI_BELIEF_ENEMY_EXISTS].beliefs != nullptr;
}

// 0x005FB640
bool ai_perception::has_enemy_contact() const
{
    return belief_bins[AI_BELIEF_ENEMY_POSITION].beliefs != nullptr;
}

// 0x00673A00
bool ai_perception::can_engage_target(const human* target) const
{
    return can_engage_target_reason(target) == AI_ENGAGE_ALLOWED;
}

// 0x005DC860
bool ai_perception::is_in_ally_line_of_fire() const
{
    return !in_ally_line_of_fire_time.elapsed();
}

// 0x005DC870
bool ai_perception::ally_is_in_line_of_fire() const
{
    return !ally_in_line_of_fire_time.elapsed();
}

// 0x005FB650
bool ai_perception::in_combat() const
{
    return belief_bins[AI_BELIEF_ENEMY_POSITION].beliefs != nullptr;
}

// 0x005DC880
attention_look_at_modes ai_perception::get_attention_mode() const
{
    return attention.mode;
}

// 0x005DC890
unsigned int ai_perception::get_attention_handle() const
{
    return attention.target;
}
