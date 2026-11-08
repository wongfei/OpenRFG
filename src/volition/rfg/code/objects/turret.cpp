// Original: volition/rfg/code/objects/turret.cpp
//
// turret (header comment in turret.h): the object around turret_physics. It owns the mounted weapons (children at the
// tags 2 and 3), takes hit points, runs the aiming: an operator's target position, automatic firing patterns, autoaim,
// the auto return, and limits it all to the fov. Compiled in rfg_objects_unity.cpp.

#include "volition/rfg/code/objects/turret.h"

#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/audio_lib/code/audiolib_att_loop_rel.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraint/data/limitedhinge/hkplimitedhingeconstraintdata.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/hkpconstraintinstance.h"
#include "volition/rfg/code/ai/sorted_list.h"
#include "volition/rfg/code/ai/sorted_list.inl"
#include "volition/rfg/code/camera/camera_first_person.h"
#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/rfg/code/code_effects/material_effects.h"
#include "volition/rfg/code/code_effects/material_effects_shared.h"
#include "volition/rfg/code/control/control_map.h"
#include "volition/rfg/code/destruction/rfg_materials.h"
#include "volition/rfg/code/gameplay/wrecking_crew/wrecking_crew.h"
#include "volition/rfg/code/gamewide/stats.h"
#include "volition/rfg/code/gamewide/tweak_table.h"
#include "volition/rfg/code/geom/character_instance.h"
#include "volition/rfg/code/geom/vanim_shared.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/vlib/code/input/input.h"
#include "volition/vlib/code/input/mouse.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/level/world.inl"
#include "volition/rfg/code/multi/multi_core/include/multi_core_clock.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_objects.h"
#include "volition/rfg/code/multi/multi_game/multi_game_object_turret.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_combat_turret.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/object_constraint.h"
#include "volition/rfg/code/objects/object_item.h"
#include "volition/rfg/code/objects/object_prop_names_shared.h"
#include "volition/rfg/code/objects/turret_info.h"
#include "volition/rfg/code/objects/turret_internal.h"
#include "volition/rfg/code/scripting/action_script/global_event_data.h"
#include "volition/rfg/code/scripting/action_script/global_event_manager.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/vehicle/vehicle_interaction.h"
#include "volition/rfg/code/vehicle/vehicle_system.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/vlib/code/math/bbox.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/math/vector2.h"
#include "volition/vlib/code/memory/object_pool.h"

#include "keen/base/trace.hpp"

#include <math.h>

// ---- file-static globals
// const float PI (0x01310C80) is used here too; placed in human_spinebend.cpp.
// const float SILENT_DEATH_TREAT_AS_NORMAL_DIST_SQ (0x01310FFC) is used here too; placed in player.cpp.
static const char DEFAULT_TURRET_PHYSICAL_MATERIAL_NAME[13] = "Steel-Normal"; // 0x01311BA8
static const char GUN_OF_THARSIS_SHUTDOWN_SOUND_NAME[25] = "OBJ_ARTILLERY_GUN_DTH_01"; // 0x01311BB8
static const float MIN_FIXED_TURRET_H_FOV = 0.261795f; // 0x01311BD4: below 15 degrees all around a turret is fixed
// const float MIN_UPDATE_TAG_POS_DIST_SQ (0x01311C74) is used here too; placed in player.cpp.
static int Turret_prefix_overlap = 300; // 0x01694778: the prefix cue overlaps the first shot by this many ms
static float Turret_pc_multiplier = 0.09f; // 0x0169477C
static float Turret_pc_vertical_multiplier = 0.8f; // 0x01694780
// These eight are zero until tweak_table.xtbl fills them.
static float Turret_autoaim_close_dp; // 0x0301AE40
static float Turret_vehicle_turning_sound_min_delta; // 0x03023658
static float Turret_autoaim_close_distance; // 0x03023670
static float Turret_autoaim_max_distance; // 0x03023680
// player* Player (0x03023874) is used here too; placed in player.cpp.
static float Turret_autoaim_min_dp; // 0x03024E6C
static float Turret_autoaim_angle_bias; // 0x03025A98
static float Turret_turning_sound_min_delta; // 0x03029B40
static float Turret_autoaim_extra_dp; // 0x03029B44
static bool Turret_autoaim_debug; // 0x0302F226
static bool Turret_use_old_controls; // 0x0302F227
static const float TURRET_STUCK_AIM_DOT = (float)cos((double)0.17453f); // 0x0304358C: cos(10 degrees)
unsigned char Default_turret_physical_material_index = 0xFF; // 0x01694703
unsigned char Default_turret_effect_material_index = 0xFF; // 0x01694784
static tweak_table_item Tweak_table_item_Turret_autoaim_max_distance("Turret_autoaim_max_distance", &Turret_autoaim_max_distance); // 0x03537E64
static tweak_table_item Tweak_table_item_Turret_autoaim_close_distance("Turret_autoaim_close_distance", &Turret_autoaim_close_distance); // 0x030FCE0C
static tweak_table_item Tweak_table_item_Turret_autoaim_extra_dp("Turret_autoaim_extra_dp", &Turret_autoaim_extra_dp); // 0x0342A380
static tweak_table_item Tweak_table_item_Turret_autoaim_min_dp("Turret_autoaim_min_dp", &Turret_autoaim_min_dp); // 0x032A0600
static tweak_table_item Tweak_table_item_Turret_autoaim_close_dp("Turret_autoaim_close_dp", &Turret_autoaim_close_dp); // 0x03067F34
static tweak_table_item Tweak_table_item_Turret_autoaim_angle_bias("Turret_autoaim_angle_bias", &Turret_autoaim_angle_bias); // 0x0353F060
static tweak_table_item Tweak_table_item_Turret_vehicle_turning_sound_min_delta("Turret_vehicle_turning_sound_min_delta", &Turret_vehicle_turning_sound_min_delta); // 0x03045040
static tweak_table_item Tweak_table_item_Turret_turning_sound_min_delta("Turret_turning_sound_min_delta", &Turret_turning_sound_min_delta); // 0x03067FA8
static object_pool<turret, 66, 1> turretpool; // 0x03045360
static obj_class_instantiator turret_instantiator("turret", alloc_turret, types_turret, bpo_index_turret, classname_turret, turret::pool_remaining, 0x5D60, 66); // 0x03560AAC (0x0107CDE0)

// The turret's physics when it is streamed in (rdd.get_data()->physics of the original's inlined checks).
static turret_physics* turret_loaded_physics(const turret* tp)
{
    return tp->rdd.is_loaded() ? tp->rdd.get_data()->physics : nullptr;
}

// The hinge's angle limits of a component, when it has a constraint.
static void turret_set_hinge_limits(turret_component_physics_info& tcpi, float min_ang, float max_ang)
{
    if (tcpi.hconstraint != nullptr)
    {
        hkpLimitedHingeConstraintData* hinge = (hkpLimitedHingeConstraintData*)tcpi.hconstraint->m_data;
        hinge->m_atoms.m_angLimit.m_minAngle = min_ang;
        hinge->m_atoms.m_angLimit.m_maxAngle = max_ang;
    }
}

// 0x00A95FF0 (:205): the target position of an owner-less automated turret or a vehicle gunner's turret. The local
// player in a walker (sub type 6) simply aims where the player aims.
void turret::autoaim()
{
    if (!should_autoaim())
    {
        return;
    }
    if (Multiplayer && !multi_is_server())
    {
        return;
    }
    turret_flags.has_autoaim_target = false;
    weapon* wp = get_weapon_inst();
    if (wp == nullptr)
    {
        return;
    }
    human* owner = World.object_from_handle<human>(wp->owner_handle);
    vector aim_from = pos;
    vector aim_dir = orient.fvec;
    player* local = Player;
    vehicle* owner_vp = nullptr;
    if (owner != nullptr)
    {
        owner_vp = human_in_vehicle(owner);
        if (owner_vp != nullptr)
        {
            const bool follow_player = owner == local && owner_vp->get_sub_type() == OT_SUB_VEHICLE_WALKER && can_hit_target(local->aim_pos);
            // Sideways the turret counts as on the vehicle's center line.
            vec_change_to_local_coords(aim_from, owner_vp->pos, owner_vp->orient);
            aim_from.x = 0.0f;
            vec_change_to_global_coords(aim_from, owner_vp->pos, owner_vp->orient);
            if (follow_player)
            {
                aim_dir.x = local->aim_pos.x - aim_from.x;
                aim_dir.y = local->aim_pos.y - aim_from.y;
                aim_dir.z = local->aim_pos.z - aim_from.z;
                aim_dir.normalize_safe();
                target_pos.x = aim_dir.x * Turret_autoaim_max_distance + aim_from.x;
                target_pos.y = aim_dir.y * Turret_autoaim_max_distance + aim_from.y;
                target_pos.z = aim_dir.z * Turret_autoaim_max_distance + aim_from.z;
                return;
            }
        }
    }

    float best_human_score = 3.4028235e+38f; // FLT_MAX
    float best_vehicle_score = 3.4028235e+38f;
    vector best_human_pos = ZERO_VECTOR;
    vector best_vehicle_pos = ZERO_VECTOR;
    human* best_human = nullptr;
    human* best_vehicle_human = nullptr;

    // An automated turret sees its whole fov (widened by the extra dp), a gunner's turret the tweak's cone.
    float widest = fabsf(h_fov_pos);
    if (widest - fabsf(h_fov_neg) < 0.0f)
    {
        widest = fabsf(h_fov_neg);
    }
    if (widest - fabsf(v_cur_fov_pos) < 0.0f)
    {
        widest = fabsf(v_cur_fov_pos);
    }
    if (widest - fabsf(v_cur_fov_neg) < 0.0f)
    {
        widest = fabsf(v_cur_fov_neg);
    }
    const bool automated = t_info->flags.automated;
    float min_dp = (float)cos((double)widest) - Turret_autoaim_extra_dp;
    if (!automated)
    {
        min_dp = Turret_autoaim_min_dp;
    }
    // Original: Turret_autoaim_debug builds a matrix for debug drawing here (stripped from retail).

    const bool keep_target = autoaim_keep_target_timer.valid() && !autoaim_keep_target_timer.elapsed();
    const human_teams team = owner != nullptr ? owner->current_team : HUMAN_TEAM_NONE;
    const int num = World.get_num_objects<human>();
    for (int i = 0; i < num; i++)
    {
        human* hp = World.get_object<human>(i);
        if (hp->is_killed() || !human_teams_are_unfriendly(team, hp->current_team) || hp == owner)
        {
            continue;
        }
        vector head_pos;
        human_get_head_pos(hp, head_pos);
        vector dir;
        dir.x = head_pos.x - aim_from.x;
        dir.y = head_pos.y - aim_from.y;
        dir.z = head_pos.z - aim_from.z;
        if (dir.x > Turret_autoaim_max_distance || -Turret_autoaim_max_distance > dir.x
            || dir.z > Turret_autoaim_max_distance || -Turret_autoaim_max_distance > dir.z)
        {
            continue;
        }
        const float dist = dir.mag_and_normalize_safe();
        if (dist > Turret_autoaim_max_distance)
        {
            continue;
        }
        const float dp = dir.y * aim_dir.y + aim_dir.x * dir.x + aim_dir.z * dir.z;
        if ((min_dp > dp && (automated || dist > Turret_autoaim_close_distance || Turret_autoaim_close_dp > dp)) || !can_hit_target(head_pos))
        {
            continue;
        }
        if (keep_target && hp->get_handle() == autoaim_keep_target_handle)
        {
            // The target of the last shots stays until the timer ends.
            turret_flags.has_autoaim_target = true;
            target_pos = head_pos;
            autoaim_keep_target_handle = hp->get_handle();
            return;
        }
        const float score = dist - dp * Turret_autoaim_angle_bias;
        if (human_in_protected_vehicle_pos(hp))
        {
            vehicle* hvp = World.object_from_handle<vehicle>(hp->vehicle_handle);
            if (hvp != nullptr && score < best_vehicle_score)
            {
                best_vehicle_pos = hvp->pos;
                best_vehicle_score = score;
                best_vehicle_human = hp;
            }
        }
        else if (score < best_human_score)
        {
            best_human_score = score;
            best_human_pos = head_pos;
            best_human = hp;
        }
    }
    if (best_human != nullptr)
    {
        turret_flags.has_autoaim_target = true;
        target_pos = best_human_pos;
        autoaim_keep_target_handle = best_human->get_handle();
        return;
    }
    if (best_vehicle_human != nullptr)
    {
        turret_flags.has_autoaim_target = true;
        target_pos = best_vehicle_pos;
        return;
    }
    if (owner_vp == nullptr || t_info->flags.automated || !turret_flags.vehicle_turret)
    {
        wp->get_muzzle_transform(target_pos, nullptr);
        target_pos.x = Turret_autoaim_max_distance * orient.fvec.x + target_pos.x;
        target_pos.y = orient.fvec.y * Turret_autoaim_max_distance + target_pos.y;
        target_pos.z = orient.fvec.z * Turret_autoaim_max_distance + target_pos.z;
    }
    else
    {
        target_pos.x = owner_vp->pos.x + owner_vp->orient.fvec.x * Turret_autoaim_max_distance;
        target_pos.y = owner_vp->pos.y + owner_vp->orient.fvec.y * Turret_autoaim_max_distance;
        target_pos.z = owner_vp->pos.z + owner_vp->orient.fvec.z * Turret_autoaim_max_distance;
    }
}

// 0x00A72610 (:436): automated turrets, and the gun of a vehicle other than a tank when a player drives it. A player
// gunner aims by hand. The asm is NEG/SBB/INC on the seat, i.e. seat == VSI_DRIVER.
bool turret::should_autoaim() const
{
    const weapon* wp = get_weapon_inst();
    if (wp == nullptr)
    {
        return false;
    }
    const human* owner = World.object_from_handle<human>(wp->owner_handle);
    if (turret_flags.vehicle_turret && get_parent() != 0xFFFFFFFF)
    {
        const vehicle* vp = World.object_from_handle<vehicle>(get_parent());
        if (vp != nullptr && !vehicle_is_tank(vp) && owner != nullptr && owner->get_sub_type() == OT_SUB_HUMAN_PLAYER)
        {
            return vehicle_passenger_get_seat(vp, owner) == VSI_DRIVER;
        }
    }
    return t_info->flags.automated;
}

// 0x00A41A10 (:458)
unsigned int turret::get_detour_request_handle() const
{
    const turret_physics* physics = turret_loaded_physics(this);
    return physics != nullptr ? physics->detour_request_handle : 0;
}

// 0x00A726D0 (:464): a delay of -1 means the weapon's refire delay.
void turret::automatic_firing_pattern_start(int delay_ms)
{
    if (!should_autoaim() && !Multiplayer)
    {
        afp_is_active = true;
        afp_delay_ms = delay_ms == -1 ? (int)(t_info->w_info->default_refire_delay * 1000.0f) : delay_ms;
        automatic_firing_pattern_setup_target();
    }
}

// 0x00A1F4D0 (:480): the Gun of Tharsis winds down with a sound.
void turret::automatic_firing_pattern_end()
{
    afp_is_active = false;
    afp_delay_timer.invalidate();
    if (t_info->flags.gun_of_tharsis)
    {
        int cue = -1;
        audiolib_cue_get_id(GUN_OF_THARSIS_SHUTDOWN_SOUND_NAME, &cue);
        audiolib_cue_play_3d(cue, &pos, &orient.fvec, nullptr, nullptr);
        if (turning_alr_id != -1)
        {
            audiolib_att_loop_rel_stop(turning_alr_id, 100);
            turning_alr_id = -1;
        }
    }
}

// 0x00A1F550 (:499)
void turret::automatic_firing_pattern_setup_target()
{
    afp_target_heading = fl_rand_range(h_fov_neg, h_fov_pos);
    afp_target_pitch = fl_rand_range(v_cur_fov_neg, v_cur_fov_pos);
    afp_delay_timer.set(afp_delay_ms);
    afp_played_prefix_sound = false;
}

// 0x00A72730 (:512)
void turret::disable_fov_limits()
{
    set_h_fov_constraints(3.1415927f, -3.1415927f);
    v_cur_fov_pos = 3.1415927f;
    v_cur_fov_neg = -3.1415927f;
    // Original: reads through a null physics when the turret is not streamed in.
    turret_set_hinge_limits(turret_loaded_physics(this)->components[TURRET_COMPONENT_TOP], -3.1415927f, 3.1415927f);
    has_rear_fov_limits = false;
    has_side_fov_limits = false;
}

// 0x00A41A40 (:527)
void turret::set_h_fov_constraints(float h_pos, float h_neg)
{
    h_fov_pos = h_pos;
    h_fov_neg = h_neg;
    turret_flags.full360 = h_pos == 3.1415927f && h_neg == -3.1415927f;
    // Original: reads through a null physics when the turret is not streamed in.
    turret_component_physics_info& middle = turret_loaded_physics(this)->components[TURRET_COMPONENT_MIDDLE];
    if (turret_flags.full360)
    {
        turret_set_hinge_limits(middle, -1e+14f, 1e+14f);
    }
    else
    {
        // Original bug kept: middle hinge limits are the fov itself here, but assemble_components sets them negated.
        turret_set_hinge_limits(middle, h_neg, h_pos);
    }
}

// 0x00A41B00 (:555)
void turret::set_v_fov_constraints(float v_pos, float v_neg)
{
    v_cur_fov_pos = v_pos;
    v_cur_fov_neg = v_neg;
    // Original: reads through a null physics when the turret is not streamed in.
    turret_set_hinge_limits(turret_loaded_physics(this)->components[TURRET_COMPONENT_TOP], v_neg, v_pos);
}

// 0x00A727B0 (:571): NPCs stay off a turret with an explosive within 3 m (charges and mines).
void turret::look_for_nearby_explosives()
{
    const int num = World.get_num_objects<projectile>();
    for (int i = 0; i < num; i++)
    {
        const projectile* pjp = World.get_object<projectile>(i);
        const weapon_class_type wclass = pjp->w_info->weapon_class;
        if ((wclass == WEAPON_CLASS_THROWN_CHARGE || wclass == WEAPON_CLASS_THROWN_MINE) && vec_dist_squared(pjp->pos, pos) < 9.0f)
        {
            disable_for_npcs_time.set(Ai_reuse_turret_after_nearby_explosive * 1000);
        }
    }
}

// 0x00A1F5C0 (:585)
unsigned int turret::get_linked_turret()
{
    return linked_to;
}

// 0x00A41B60 (:590)
void turret::set_linked_turret(unsigned int h)
{
    if (h != 0xFFFFFFFF && World.object_from_handle<turret>(h) == this)
    {
        linked_to = 0xFFFFFFFF;
        return;
    }
    linked_to = h;
}

// 0x00A41BB0 (:610): the middle component's position, where the turret turns.
bool turret::get_pivot_point(vector& use_pos) const
{
    const turret_physics* physics = turret_loaded_physics(this);
    if (physics == nullptr)
    {
        return false;
    }
    const turret_component_physics_info& middle = physics->components[TURRET_COMPONENT_MIDDLE];
    if (!middle.is_veh_component)
    {
        matrix unused_orient;
        havok_body_get_pos_orient(middle.havok_handle, use_pos, unused_orient);
        return true;
    }
    if (get_parent() == 0xFFFFFFFF)
    {
        return false;
    }
    vehicle* vp = World.object_from_handle<vehicle>(get_parent());
    if (vp == nullptr || !vp->rdd.is_loaded())
    {
        return false;
    }
    use_pos = vp->rdd.get_data()->m_components[middle.veh_component_idx].get_world_space_position();
    return true;
}

// 0x00A1F5D0 (:633): the turning loop plays while the turret turns faster than the tweak's degrees per second.
void turret::handle_turning_sounds(float delta_heading)
{
    if (t_info == nullptr || t_info->turning_loop_cue_id == -1)
    {
        return;
    }
    const float min_delta = (turret_flags.vehicle_turret ? Turret_vehicle_turning_sound_min_delta : Turret_turning_sound_min_delta) * 0.017453f * Frametime;
    if (turning_alr_id == -1)
    {
        if (min_delta <= fabsf(delta_heading))
        {
            audiolib_att_loop_rel_start(t_info->turning_loop_cue_id, &turning_alr_id, false, &pos, nullptr);
        }
        return;
    }
    if (fabsf(delta_heading) <= min_delta * 0.75f)
    {
        audiolib_att_loop_rel_stop(turning_alr_id, 100);
        turning_alr_id = -1;
        return;
    }
    audiolib_att_loop_rel_set_pos(turning_alr_id, &pos, nullptr);
}

// 0x00A1F6E0 (:658)
void turret::set_up_fov_blend_splines()
{
    v_fov_blend_spline.set_points(7, t_info->v_fov_blend_spline_points);
}

// 0x00A41C70 (:674)
obj_turret_props::obj_turret_props(const turret_info* t_info, const vector& pos, const matrix& orient, int attach_tag, unsigned int owner_handle)
    : obj_prop_block(pos, orient, nullptr)
{
    set(TURRET_PROP_TINFO, t_info);
    set(TURRET_PROP_ATTACH_TAG, attach_tag);
    if (owner_handle != 0xFFFFFFFF)
    {
        set(TURRET_PROP_WEAPON_OWNER, owner_handle);
    }
}

// 0x00A1CF80
obj_turret_props::~obj_turret_props()
{
}

// 0x00A1F720 (:682)
int bpo_index_turret()
{
    return 0;
}

// 0x00A969A0 (:682)
void turret::pool_free()
{
    turretpool.free(this);
}

// 0x00A41D20 (:682)
int turret::pool_remaining()
{
    return turretpool.get_free();
}

// 0x00A1F740 (:682)
unsigned int turret::get_classname_hash()
{
    return turret_instantiator.this_hash;
}

// 0x00A72860 (:682)
object* alloc_turret()
{
    return turretpool.alloc(0);
}

// 0x00A1F710 (:682)
void types_turret(signed char& type, signed char& subtype)
{
    type = OT_TURRET;
    subtype = OT_SUB_UNDEFINED;
}

// 0x00A1F730 (:682)
const char* classname_turret()
{
    return "turret";
}

// 0x00A41D30 (:683)
turret::turret()
{
}

// 0x00A41DC0
turret::~turret()
{
}

// 0x00AAFC20 (:696): on load also creates the mounted weapons; a turret parented to a vehicle is a vehicle turret.
// Fixed: no angle above 15 degrees.
bool turret::serialize(obj_prop_block& pblock, bool load)
{
    if (!object::serialize(pblock, load))
    {
        return false;
    }
    if (!load)
    {
        pblock.set(TURRET_PROP_TINFO, t_info);
        pblock.set(TURRET_PROP_ATTACH_TAG, vehicle_attach_tag_index);
        return true;
    }
    obj_flags.no_save = true;
    t_info = nullptr;
    mp_sim_state = nullptr;
    havok_handle = 0xFFFFFFFF;
    current_operator = 0xFFFFFFFF;
    first_parent = 0xFFFFFFFF;
    linked_to = 0xFFFFFFFF;
    spawn_node_handle = 0xFFFFFFFF;
    vehicle_heading_offset = 0.0f;
    disable_for_npcs_time.set(0);
    divergence_time.invalidate();
    afp_is_active = false;
    afp_delay_timer.invalidate();
    turning_alr_id = -1;
    primary_door_timer.invalidate();
    secondary_door_timer.invalidate();
    autoaim_keep_target_handle = 0xFFFFFFFF;
    autoaim_keep_target_timer.invalidate();
    memset(&turret_flags, 0, sizeof(turret_flags));
    target_pos.x = pos.x + orient.fvec.x * 10.0f;
    target_pos.y = pos.y + orient.fvec.y * 10.0f + 1.0f;
    target_pos.z = pos.z + orient.fvec.z * 10.0f;
    const object* parent_vp = World.object_from_handle<vehicle>(get_parent());
    if (parent_vp != nullptr)
    {
        turret_flags.vehicle_turret = true;
    }
    if (!pblock.get_optional<const turret_info*>(TURRET_PROP_TINFO, t_info))
    {
        const char* name = pblock.get_str_ref_optional(TURRET_PROP_TINFO_NAME);
        if (name == nullptr)
        {
            return false;
        }
        t_info = turret_find_info(name);
    }
    if (t_info == nullptr)
    {
        return false;
    }
    if (pblock.get_optional(TURRET_PROP_ROTATION_LIMIT, h_fov_pos))
    {
        h_fov_pos = h_fov_pos * 0.017453f;
        h_fov_neg = -h_fov_pos;
    }
    else
    {
        h_fov_pos = t_info->h_fov_pos;
        h_fov_neg = t_info->h_fov_neg;
    }
    turret_flags.full360 = fl_greater_than_equal_robust(h_fov_pos - h_fov_neg, 6.2831855f, 0.0001f);
    v_cur_fov_pos = t_info->v_fov_blend_spline_points[1].x;
    v_cur_fov_neg = t_info->v_fov_blend_spline_points[1].y;
    v_fov_blend_spline.set_points(7, t_info->v_fov_blend_spline_points);
    has_rear_fov_limits = t_info->flags.has_rear_fov_limits;
    has_side_fov_limits = t_info->flags.has_side_fov_limits;
    float widest = fabsf(h_fov_pos);
    if (widest - fabsf(h_fov_neg) < 0.0f)
    {
        widest = fabsf(h_fov_neg);
    }
    if (widest - fabsf(v_cur_fov_pos) < 0.0f)
    {
        widest = fabsf(v_cur_fov_pos);
    }
    if (widest - fabsf(v_cur_fov_neg) < 0.0f)
    {
        widest = fabsf(v_cur_fov_neg);
    }
    turret_flags.fixed_turret = widest < MIN_FIXED_TURRET_H_FOV;
    hit_points = t_info->max_hitpoints;
    if (turret_flags.vehicle_turret)
    {
        pblock.get(TURRET_PROP_ATTACH_TAG, vehicle_attach_tag_index);
        if (vehicle_attach_tag_index == -1)
        {
            return false;
        }
    }
    else
    {
        vehicle_attach_tag_index = -1;
    }

    // The weapons: no physics of their own, simulated, shadow casters (flags 0x6006).
    obj_item_flags flags;
    memset(&flags, 0, sizeof(flags));
    flags.no_physics = true;
    flags.simulate = true;
    flags.cast_shadows = true;
    flags.cast_drop_shadows = true;
    {
        const weapon_info* w_info = t_info->w_info;
        obj_item_props props(w_info->weapon_obj_item_info, pos, orient, flags, COLLIDE_ITEM, -1.0f, INVALID_SALVAGE_MATERIAL);
        props.set(PROP_WEAPON_INFO, w_info);
        weapon* wp = World.create_object<weapon>(props, 0xFFFFFFFF);
        if (wp == nullptr)
        {
            return false;
        }
        set_child(wp, 2, -1, false, true);
        wp->obj_flags.no_save = true;
        wp->item_flags.item_held = true;
        wp->turret_handle = get_handle();
        primary_weapon_handle = wp->get_handle();
        secondary_weapon_handle = 0xFFFFFFFF;
        weapon_stop_pickup_effect(wp);
    }
    if (t_info->secondary_w_info != nullptr)
    {
        const weapon_info* w_info = t_info->secondary_w_info;
        obj_weapon_props props(w_info, w_info->weapon_obj_item_info, pos, orient, flags, COLLIDE_ITEM, -1.0f);
        weapon* wp = World.create_object<weapon>(props, 0xFFFFFFFF);
        if (wp == nullptr)
        {
            return false;
        }
        set_child(wp, 3, -1, false, true);
        wp->obj_flags.no_save = true;
        wp->item_flags.item_held = true;
        wp->turret_handle = get_handle();
        secondary_weapon_handle = wp->get_handle();
    }
    unsigned int weapon_owner;
    if (pblock.get_optional(TURRET_PROP_WEAPON_OWNER, weapon_owner))
    {
        weapon* wp = get_weapon_inst();
        if (wp == nullptr)
        {
            return false;
        }
        wp->set_owner(weapon_owner, true);
        wp = get_secondary_weapon_inst();
        if (wp != nullptr)
        {
            wp->set_owner(weapon_owner, true);
        }
    }
    if (Multiplayer && multi_is_server() && parent_vp == nullptr)
    {
        remote_obj_flags.remote_object = true;
        multi_core_object_create_remote_object(10, get_handle()); // 10: MCRO type of the turret
    }
    return true;
}

// 0x00AB02A0 (:844)
bool turret::stream_out_common()
{
    if (turning_alr_id != -1)
    {
        audiolib_att_loop_rel_stop(turning_alr_id, 100);
        turning_alr_id = -1;
    }
    if (rdd.is_loaded() && rdd.get_data()->physics != nullptr)
    {
        // Original: looks up the parent vehicle here and ignores the result (the unload's vp parameter is unused).
        turret_physics* physics = rdd.get_data()->physics;
        turret_physics_stream_unload(physics, this, nullptr);
        turret_physics_free(physics);
        rdd.get_data()->physics = nullptr;
    }
    rdd.set_loaded(false);
    return true;
}

// The weapon callbacks a stream toggles (inlined per weapon in the original).
static void turret_clear_weapon_callbacks(weapon* wp)
{
    wp->clear_callback(WEAPON_CB_IDX_POST_FIRE);
    wp->clear_callback(WEAPON_CB_IDX_READY_TO_FIRE);
    wp->clear_callback(WEAPON_CB_IDX_PRESS_TRIGGER);
    wp->clear_callback(WEAPON_CB_IDX_RELEASE_TRIGGER);
}

// 0x00AB4940 (:891)
bool turret::stream(bool load)
{
    if (!load)
    {
        if (obj_flags.destroy_on_stream)
        {
            World.destroy_object(this, false);
        }
        // Original bug kept: only cycling weapons lose their callbacks, though every weapon gets them on load.
        if (t_info->flags.cycle_primary_weapon)
        {
            weapon* wp = get_weapon_inst();
            if (wp != nullptr)
            {
                turret_clear_weapon_callbacks(wp);
            }
        }
        if (t_info->flags.cycle_secondary_weapon)
        {
            weapon* wp = get_secondary_weapon_inst();
            if (wp != nullptr)
            {
                turret_clear_weapon_callbacks(wp);
            }
        }
        return stream_out_common();
    }
    vehicle* vp = nullptr;
    if (turret_flags.vehicle_turret)
    {
        vp = World.object_from_handle<vehicle>(get_parent());
        if (vp != nullptr && vp->obj_flags.destroyed)
        {
            return false;
        }
    }
    rdd.set_loaded(true);
    turret_physics* physics = turret_physics_alloc();
    rdd.get_data()->physics = physics;
    if (physics == nullptr)
    {
        return false;
    }
    if (!turret_physics_stream_load(physics, this, vp))
    {
        stream_out_common();
        return false;
    }
    weapon* wp = World.object_from_handle<weapon>(primary_weapon_handle);
    if (wp != nullptr)
    {
        wp->set_callback(WEAPON_CB_IDX_POST_FIRE, weapon_cb_post_fire, get_handle());
        wp->set_callback(WEAPON_CB_IDX_READY_TO_FIRE, weapon_cb_ready_to_fire, get_handle());
        wp->set_callback(WEAPON_CB_IDX_PRESS_TRIGGER, weapon_cb_trigger_press, get_handle());
        wp->set_callback(WEAPON_CB_IDX_RELEASE_TRIGGER, weapon_cb_trigger_release, get_handle());
    }
    wp = World.object_from_handle<weapon>(secondary_weapon_handle);
    if (wp != nullptr)
    {
        wp->set_callback(WEAPON_CB_IDX_POST_FIRE, weapon_cb_post_fire, get_handle());
        wp->set_callback(WEAPON_CB_IDX_READY_TO_FIRE, weapon_cb_ready_to_fire, get_handle());
        wp->set_callback(WEAPON_CB_IDX_PRESS_TRIGGER, weapon_cb_trigger_press, get_handle());
        wp->set_callback(WEAPON_CB_IDX_RELEASE_TRIGGER, weapon_cb_trigger_release, get_handle());
    }
    return true;
}

// 0x00A969B0 (:980)
void turret::free_this()
{
    if (Multiplayer)
    {
        object* objp = World.object_from_handle(get_parent());
        if (objp == nullptr || objp->get_obj_type() != OT_VEHICLE)
        {
            multi_core_object_process_object_free(this);
        }
    }
    object::free_this();
    World.destroy_object(primary_weapon_handle, false);
    World.destroy_object(secondary_weapon_handle, false);
}

// 0x00AB0350 (:1003)
void turret::hierarchy_notify(hierarchy_notify_code code)
{
    if (code == HN_CHILDREN_STREAMED)
    {
        turret_physics* physics = turret_loaded_physics(this);
        if (physics != nullptr)
        {
            turret_physics_children_streamed(physics, this);
        }
    }
    else if (code == HN_PARENT_DESTROYED)
    {
        stream_out_common();
        World.destroy_object(this, false);
    }
}

// 0x00A72870 (:1034)
void turret::update_render(rl_renderer*)
{
    turret_physics* physics = turret_loaded_physics(this);
    if (physics != nullptr)
    {
        turret_physics_update_renderables(physics);
    }
}

// 0x00A96A10 (:1042): the bodies stay as they are; they move the object, not the other way around.
void turret::update_pos_and_orient(const vector& update_pos, const matrix& update_orient, bool set_havok_data)
{
    object::update_pos_and_orient(update_pos, update_orient, false);
}

// 0x00A41DF0 (:1048)
void turret::snap_pos_and_orient(const vector& update_pos, const matrix& update_orient)
{
    update_pos_and_orient(update_pos, update_orient, false);
    const turret_physics* physics = turret_loaded_physics(this);
    if (physics != nullptr)
    {
        for (int i = 0; i < NUM_TURRET_COMPONENTS; i++)
        {
            if (!physics->components[i].is_veh_component && physics->components[i].havok_handle != 0xFFFFFFFF)
            {
                havok_body_set_pos_orient(physics->components[i].havok_handle, pos, orient);
            }
        }
    }
}

// 0x00A41E60 (:1066): 0 is the object's body, 1 to 3 the components'.
unsigned int turret::get_alt_hk_body_handle(unsigned int index) const
{
    unsigned int body;
    if (index == 0)
    {
        body = havok_handle;
    }
    else
    {
        const turret_physics* physics = index <= 3 ? turret_loaded_physics(this) : nullptr;
        if (physics == nullptr)
        {
            return 0xFFFFFFFF;
        }
        body = physics->components[index - 1].havok_handle;
    }
    if (body != 0xFFFFFFFF && havok_body_get_pointer(body) == nullptr)
    {
        return 0xFFFFFFFF;
    }
    return body;
}

// 0x00A41EC0 (:1093)
unsigned int turret::get_alt_hk_body_handle_index(unsigned int alt_handle) const
{
    if (alt_handle == 0xFFFFFFFF || havok_body_get_pointer(alt_handle) == nullptr)
    {
        return 0xFFFFFFFF;
    }
    if (alt_handle == havok_handle)
    {
        return 0;
    }
    const turret_physics* physics = turret_loaded_physics(this);
    if (physics != nullptr)
    {
        for (unsigned int i = 0; i < 3; i++)
        {
            if (alt_handle == physics->components[i].havok_handle)
            {
                return i + 1;
            }
        }
    }
    return 0xFFFFFFFF;
}

// 0x00A728A0 (:1132): the object's own transform without physics.
bool turret::get_tag_world_coords(int tag_index, vector& tag_pos, matrix* tag_orient, vector* obj_pos, matrix* obj_orient) const
{
    if (obj_pos != nullptr || obj_orient != nullptr)
    {
        return false;
    }
    tag_pos = pos;
    if (tag_orient != nullptr)
    {
        *tag_orient = orient;
    }
    if (rdd.is_loaded() && !is_killed() && !obj_flags.destroyed)
    {
        const vehicle* vp = World.object_from_handle<vehicle>(get_parent());
        turret_physics_get_tag_world_coords(rdd.get_data()->physics, this, vp, tag_index, tag_pos, tag_orient);
        return true;
    }
    return false;
}

// 0x00A1F760 (:1155)
bool turret::get_tag_local_coords(int tag_index, vector& tag_pos, matrix* tag_orient) const
{
    const bool result = get_tag_world_coords(tag_index, tag_pos, tag_orient, nullptr, nullptr);
    vec_change_to_local_coords(tag_pos, pos, orient);
    if (tag_orient != nullptr)
    {
        tag_orient->rotate(orient);
    }
    return result;
}

// 0x00A72960 (:1168)
void turret::cycle_tag(int tag_index)
{
    vehicle* vp = get_vehicle();
    if (vp != nullptr && rdd.is_loaded())
    {
        turret_physics_cycle_tag(rdd.get_data()->physics, this, vp, tag_index);
    }
}

// 0x00A729B0 (:1189)
void turret::reset_cycle(int tag_index)
{
    vehicle* vp = get_vehicle();
    if (vp != nullptr && rdd.is_loaded())
    {
        turret_physics_reset_cycle(rdd.get_data()->physics, this, vp, tag_index);
    }
}

// 0x00A72A00 (:1210)
void turret::play_alt_effect(int tag_index)
{
    vehicle* vp = get_vehicle();
    if (vp != nullptr && rdd.is_loaded())
    {
        turret_physics_play_alt_effect(rdd.get_data()->physics, this, vp, tag_index);
    }
}

// 0x00A72A50 (:1232): true for a turret not on a vehicle: it has no doors.
bool turret::process_door_weapon_ready(int tag_index)
{
    vehicle* vp = get_vehicle();
    if (vp == nullptr)
    {
        return true;
    }
    if (!rdd.is_loaded())
    {
        return false;
    }
    return turret_physics_process_door_weapon_ready(rdd.get_data()->physics, this, vp, tag_index);
}

// 0x00A1F7B0 (:1256)
void turret::get_bbox(vector& bmin, vector& bmax) const
{
    get_local_bbox(bmin, bmax);
    bbox_instance(bmin, bmax, orient, pos, bmin, bmax);
}

// 0x00A72AB0 (:1263)
void turret::get_local_bbox(vector& bmin, vector& bmax) const
{
    turret_physics* physics = turret_loaded_physics(this);
    if (physics != nullptr)
    {
        turret_physics_get_local_bbox(physics, this, bmin, bmax);
        return;
    }
    bmin.x = -0.5f;
    bmin.y = 0.0f;
    bmin.z = -0.5f;
    bmax.x = 0.5f;
    bmax.y = 1.0f;
    bmax.z = 0.5f;
}

// 0x00ACE0C0 (:1279)
void turret::take_damage(unsigned int damage, const vector* linear_impulse, const vector* angular_impulse)
{
    if (Multiplayer && !multi_is_server())
    {
        return;
    }
    if (!t_info->flags.invulnerable && !turret_flags.vehicle_turret && !is_killed())
    {
        hit_points -= damage < hit_points ? damage : hit_points;
        if (hit_points == 0)
        {
            kill(linear_impulse, angular_impulse);
        }
    }
}

// 0x00ACE130 (:1305)
void turret::process_explosion_hit(unsigned int damage, const vector& linear_impulse, const vector& angular_impulse)
{
    take_damage(damage, &linear_impulse, &angular_impulse);
}

// 0x00ACE140 (:1310)
bool turret::process_melee_hit(object* attacker, float damage, const vector& impulse, const vector& hit_pos, const vector& hit_normal, unsigned int shape_key, const weapon_info& w_info)
{
    take_damage((unsigned int)(long long)floor((double)damage + 0.5), &impulse, nullptr);
    return hit_points == 0;
}

// 0x00ACE1E0 (:1316): damage is the collision's energy; 100 points per unit, half of it the impulse.
void turret::process_collision(const object* other_obj, float damage, const vector& hit_vel)
{
    vector impulse = hit_vel.copy_normalize_safe();
    const float scale = damage * 0.5f;
    impulse.x *= scale;
    impulse.y *= scale;
    impulse.z *= scale;
    take_damage((unsigned int)(long long)floor((double)(damage * 100.0f) + 0.5), &impulse, nullptr);
}

// 0x00AD8D50 (:1326): bullets do a quarter of their damage; the nano rifle eats the operator and the mesh, on a vehicle
// turret the vehicle instead.
void turret::process_bullet_hit(human* attacker, float damage, const weapon_fire_info& fire_info)
{
    take_damage((unsigned int)(long long)floor((double)(damage * 0.25f) + 0.5), nullptr, nullptr);
    const bool death_full_fade = is_killed();
    if (fire_info.w_info->weapon_class != WEAPON_CLASS_NANO_RIFLE || t_info->flags.invulnerable)
    {
        return;
    }
    if (turret_flags.vehicle_turret)
    {
        // Original: reads through null if the parent is not a vehicle.
        object* objp = World.object_from_handle<vehicle>(get_parent());
        objp->process_bullet_hit(attacker, damage, fire_info);
        return;
    }
    material_effect_parameters effect_params = material_effects_get_parameters(1, 2000);
    effect_params.use_local_pos = true;
    weapon* wp = World.object_from_handle<weapon>(primary_weapon_handle);
    if (wp != nullptr)
    {
        if (wp->rdd.is_loaded())
        {
            effect_params.world_pos = fire_info.hit_pos;
            material_effects_apply(wp->rdd.get_data()->smesh_instance_p, material_effects_get_reconstructor_shards_id(), &effect_params);
        }
    }
    human* op = World.object_from_handle<human>(current_operator);
    if (op != nullptr)
    {
        // Original: passes the weapon's position in the operator's space as the "world" hit position.
        vector hit = wp != nullptr ? wp->pos : ZERO_VECTOR;
        vec_change_to_local_coords(hit, op->pos, op->orient);
        op->start_nano_rifle_fade_out(death_full_fade, attacker, true, hit);
    }
    wp = World.object_from_handle<weapon>(secondary_weapon_handle);
    if (wp != nullptr && wp->rdd.is_loaded())
    {
        material_effects_apply(wp->rdd.get_data()->smesh_instance_p, material_effects_get_reconstructor_shards_id(), &effect_params);
    }
}

// 0x00AC5840 (:1362)
void turret::kill(const vector* linear_impulse, const vector* angular_impulse)
{
    if (t_info->flags.invulnerable)
    {
        return;
    }
    hit_points = 0;
    human* op = World.object_from_handle<human>(current_operator);
    if (op != nullptr)
    {
        human_combat_abandon_turret(op);
    }
    handle_turning_sounds(0.0f);
    turret_physics* physics = turret_loaded_physics(this);
    if (physics != nullptr)
    {
        physics->kill_angular_impulse = angular_impulse != nullptr ? *angular_impulse : ZERO_VECTOR;
        physics->kill_linear_impulse = linear_impulse != nullptr ? *linear_impulse : ZERO_VECTOR;
        turret_physics_kill_turret(physics, this);
    }
    weapon* wp = get_weapon_inst();
    if (wp != nullptr && wp->get_parent() == get_handle())
    {
        remove_child(wp);
    }
    wp = get_secondary_weapon_inst();
    if (wp != nullptr && wp->get_parent() == get_handle())
    {
        remove_child(wp);
    }
    World.destroy_object(primary_weapon_handle, false);
    World.destroy_object(secondary_weapon_handle, false);
    primary_weapon_handle = 0xFFFFFFFF;
    secondary_weapon_handle = 0xFFFFFFFF;
    World.destroy_object(this, false);
    turret_destroyed_event destroyed;
    destroyed.turret_handle = get_handle();
    gem_post_event(destroyed);
}

// 0x00A41F30 (:1420)
bool turret::has_left_hand_grip_tag() const
{
    const turret_physics* physics = turret_loaded_physics(this);
    return physics != nullptr && physics->left_grip_tag_index != -1;
}

// 0x00A41F70 (:1425)
bool turret::has_right_hand_grip_tag() const
{
    const turret_physics* physics = turret_loaded_physics(this);
    return physics != nullptr && physics->right_grip_tag_index != -1;
}

// 0x00A41FB0 (:1432)
vehicle* turret::get_vehicle() const
{
    if (turret_flags.vehicle_turret && get_parent() != 0xFFFFFFFF)
    {
        return World.object_from_handle<vehicle>(get_parent());
    }
    return nullptr;
}

// 0x00A41FE0 (:1443)
weapon* turret::get_weapon_inst() const
{
    return primary_weapon_handle != 0xFFFFFFFF ? World.object_from_handle<weapon>(primary_weapon_handle) : nullptr;
}

// 0x00A42010 (:1448)
weapon* turret::get_secondary_weapon_inst() const
{
    return secondary_weapon_handle != 0xFFFFFFFF ? World.object_from_handle<weapon>(secondary_weapon_handle) : nullptr;
}

// 0x00A42040 (:1456): moves heading and pitch towards the wanted angles at the turret's speed. Wanted: a firing pattern's
// random target (or stay while its delay runs), the auto return to center 10 s after the operator left (2.2 rad/s on a
// vehicle_is_min_jetter vehicle, else 0.5 rad/s slowing down near center), else the target position. Original bug kept: the pitch also moves at the heading's speed, h_rot_speed.
void turret::calculate_aim_parameters()
{
    turret_physics* physics = turret_loaded_physics(this);
    if (physics == nullptr)
    {
        return;
    }
    human* hp = World.object_from_handle<human>(current_operator);
    if (hp != nullptr)
    {
        if (hp->char_inst->is_playing_action(ANIM_ALIAS_ENTER))
        {
            return;
        }
        if (hp == Player && Rfg_camera.mode == CAMERA_SLEW_MODE)
        {
            return;
        }
    }
    weapon* wp = get_weapon_inst();
    if (wp == nullptr)
    {
        return;
    }
    vector muzzle_pos;
    matrix muzzle_orient;
    wp->get_muzzle_transform(muzzle_pos, &muzzle_orient);

    float want_heading;
    float want_pitch;
    bool almost_centered = false; // no turning sound for the last steps of the auto return
    if (afp_is_active)
    {
        if (afp_delay_timer.elapsed())
        {
            want_heading = afp_target_heading;
            want_pitch = afp_target_pitch;
        }
        else
        {
            want_heading = physics->heading;
            want_pitch = physics->pitch;
        }
    }
    else if (auto_return.valid())
    {
        want_heading = physics->heading;
        float return_pitch = physics->pitch;
        if (auto_return.elapsed())
        {
            auto_return.invalidate();
            want_heading = physics->heading;
            const unsigned int base_body = physics->components[TURRET_COMPONENT_BASE].havok_handle;
            const unsigned int middle_body = physics->components[TURRET_COMPONENT_MIDDLE].havok_handle;
            if (base_body != 0xFFFFFFFF && middle_body != 0xFFFFFFFF)
            {
                vector unused_pos;
                matrix base_orient;
                matrix middle_orient;
                havok_body_get_pos_orient(base_body, unused_pos, base_orient);
                havok_body_get_pos_orient(middle_body, unused_pos, middle_orient);
                const float f_dot = turret_clamp(middle_orient.fvec.x * base_orient.fvec.x + middle_orient.fvec.y * base_orient.fvec.y + middle_orient.fvec.z * base_orient.fvec.z, -1.0f, 1.0f);
                const float angle = fl_acos(f_dot);
                want_heading = fl_sign(base_orient.rvec.x * middle_orient.fvec.x + base_orient.rvec.y * middle_orient.fvec.y + base_orient.rvec.z * middle_orient.fvec.z) * angle;
            }
            want_pitch = 0.0f;
        }
        else
        {
            float step = Frametime * 0.5f;
            bool slow_near_center = true;
            if (turret_flags.vehicle_turret && vehicle_is_min_jetter(get_vehicle()))
            {
                step = Frametime * 2.2f;
                slow_near_center = false;
            }
            bool returned = true;
            if (!t_info->flags.auto_return_dont_horizontal)
            {
                const float off = want_heading - 0.0f;
                almost_centered = step * 5.0f > off && step * 5.0f > -off;
                if (0.02f <= off || 0.02f <= -off)
                {
                    if (slow_near_center)
                    {
                        step = fl_parameter(fabsf(want_heading), 0.02f, 0.18f, 0.1f, 1.0f, true) * step;
                    }
                    want_heading -= turret_clamp(physics->heading, -step, step);
                    returned = false;
                }
                vehicle_heading_offset = 0.0f;
            }
            if (t_info->flags.auto_return_dont_vertical || (return_pitch - 0.0f < 0.02f && -(return_pitch - 0.0f) < 0.02f))
            {
                want_pitch = return_pitch;
                if (returned)
                {
                    auto_return.invalidate();
                }
            }
            else
            {
                // Original: the step already scaled for the heading scales again.
                if (slow_near_center)
                {
                    step = fl_parameter(fabsf(return_pitch), 0.02f, 0.18f, 0.1f, 1.0f, true) * step;
                }
                want_pitch = return_pitch - turret_clamp(physics->pitch, -step, step);
            }
        }
    }
    else
    {
        vector from = muzzle_pos;
        if (!object_is_player(hp))
        {
            vector base_tag;
            if (!get_tag_world_coords(0, base_tag, nullptr, nullptr, nullptr))
            {
                return;
            }
            from = base_tag;
        }
        vector dir;
        dir.x = target_pos.x - from.x;
        dir.y = target_pos.y - from.y;
        dir.z = target_pos.z - from.z;
        if (t_info->flags.aim_offset_valid)
        {
            matrix aim_orient;
            aim_orient.make(dir);
            const vector offset = t_info->aim_offset.copy_unrotate(aim_orient);
            dir.x -= offset.x;
            dir.y -= offset.y;
            dir.z -= offset.z;
        }
        matrix local_orient;
        local_orient.make(dir);
        local_orient.rotate(orient);
        want_pitch = -local_orient.extract_pitch();
        want_heading = local_orient.extract_heading();
    }

    if (hp != nullptr)
    {
        want_pitch = hp->spinebend_data.pitch_drift + want_pitch;
        want_heading = hp->spinebend_data.heading_drift + want_heading;
    }
    vector2 delta;
    delta.y = want_pitch - physics->pitch;
    delta.x = want_heading - physics->heading;
    float step = t_info->h_rot_speed * Frametime;
    const unsigned int base_body = physics->components[TURRET_COMPONENT_BASE].havok_handle;
    if (base_body != 0xFFFFFFFF)
    {
        if (delta.y * delta.y + delta.x * delta.x <= 0.0f)
        {
            if (!havok_body_is_active(base_body))
            {
                return;
            }
        }
        else
        {
            havok_body_force_activate(base_body);
        }
    }
    if (hp != nullptr && hp->cur_state != HS_TURRET && !turret_flags.vehicle_turret)
    {
        step = 0.0f; // the operator is not at the turret yet
    }
    delta.x = fl_angle_cap_radians(delta.x, -3.1415927f);
    const bool fast = turret_flags.fast_mode || (hp != nullptr && hp->get_sub_type() == OT_SUB_HUMAN_PLAYER);
    const float heading_off = fabsf(delta.x);
    if (0.5f < heading_off || (fast && 0.2f < heading_off))
    {
        step *= 1.5f;
        if (0.5f < heading_off)
        {
            step *= 1.3f;
        }
        turret_flags.fast_mode = true;
    }
    else
    {
        turret_flags.fast_mode = false;
    }
    if (step * step < delta.x * delta.x + delta.y * delta.y)
    {
        delta.normalize();
        delta.x *= step;
        delta.y *= step;
    }
    const float last_heading = physics->heading;
    physics->heading = delta.x + last_heading;
    physics->pitch += delta.y;
    physics->heading_sum += delta.x;
    const float h_limit = turret_flags.full360 ? 1.0f : 0.95f;
    physics->heading = fl_angle_cap_radians(physics->heading, -3.1415927f);
    physics->heading = turret_clamp(physics->heading, h_fov_neg * h_limit, h_fov_pos * h_limit);
    if (!turret_flags.full360)
    {
        physics->heading_sum = physics->heading;
    }
    physics->pitch = fl_angle_cap_radians(physics->pitch, -3.1415927f);
    physics->pitch = turret_clamp(physics->pitch, v_cur_fov_neg * 0.95f, v_cur_fov_pos * 0.95f);

    float sound_delta;
    if (Player == nullptr || current_operator != Player->get_handle())
    {
        sound_delta = physics->heading - (almost_centered ? physics->heading : last_heading);
    }
    else
    {
        // The local player hears the camera's turn, on a vehicle only from the gun of the player's seat.
        vehicle* vp = get_vehicle();
        bool own_seat = true;
        if (vp != nullptr)
        {
            const vehicle_seat_index seat = vehicle_passenger_get_seat(vp, World.object_from_handle<human>(current_operator));
            own_seat = seat != VSI_INVALID && vehicle_get_seat_turret(vp, seat) == this;
        }
        if (own_seat)
        {
            float last;
            float current;
            Rfg_camera.last_orient.extract_heading(last);
            Rfg_camera.real_orient.extract_heading(current);
            sound_delta = current - last;
        }
        else
        {
            sound_delta = 0.0f;
        }
    }
    handle_turning_sounds(sound_delta);
}

// 0x00AC59F0 (:1738)
void turret::frame_process_pre()
{
    if (!obj_flags.streamed || is_killed() || obj_flags.destroyed)
    {
        return;
    }
    turret_physics* physics = turret_loaded_physics(this);
    if (physics == nullptr)
    {
        return;
    }
    object* vp = World.object_from_handle<vehicle>(get_parent());
    if (turret_flags.vehicle_turret && vp == nullptr)
    {
        keen::debug::nativePrintArgs("Int3 critical function"); // Original: the formatted file and line
        kill(nullptr, nullptr);
        return;
    }
    weapon* wp = get_weapon_inst();
    if (wp == nullptr)
    {
        kill(nullptr, nullptr);
        return;
    }
    if (!wp->obj_flags.streamed)
    {
        return;
    }
    update_fov_blending();
    human* op = human_combat_turret_get_operator(this);
    if (op == nullptr)
    {
        current_operator = 0xFFFFFFFF;
    }
    bool aim = true;
    if (should_autoaim())
    {
        autoaim();
        if (t_info->flags.automated && (!Multiplayer || multi_is_server()))
        {
            weapon* fire_wp = get_weapon_inst();
            if (fire_wp != nullptr && fire_wp->ready_to_fire() && turret_flags.has_autoaim_target && can_hit_target(target_pos))
            {
                fire_wp->fire(ZERO_VECTOR);
                turret_flags.mp_automated_has_fired = true;
            }
        }
    }
    else if (!afp_is_active)
    {
        if (op == nullptr && !auto_return.valid())
        {
            if (turning_alr_id != -1)
            {
                audiolib_att_loop_rel_stop(turning_alr_id, 100);
                turning_alr_id = -1;
            }
            aim = false;
        }
    }
    else
    {
        const float h_off = physics->heading - afp_target_heading;
        const float p_off = physics->pitch - afp_target_pitch;
        if (h_off < 0.1f && -h_off < 0.1f && p_off < 0.1f && -p_off < 0.1f && afp_delay_timer.elapsed())
        {
            if (!afp_played_prefix_sound && t_info->firing_pattern_prefix != -1)
            {
                const int play_id = audiolib_cue_play_3d(t_info->firing_pattern_prefix, &pos, &orient.fvec, nullptr, nullptr);
                int play_time = 0;
                audiolib_play_get_play_time(play_id, &play_time, nullptr);
                afp_delay_timer.set(play_time - Turret_prefix_overlap);
                afp_played_prefix_sound = true;
            }
            else
            {
                weapon* fire_wp = get_weapon_inst();
                if (fire_wp != nullptr && fire_wp->ready_to_fire())
                {
                    fire_wp->fire(ZERO_VECTOR);
                    turret_flags.mp_automated_has_fired = true;
                    automatic_firing_pattern_setup_target();
                    if (t_info->flags.gun_of_tharsis)
                    {
                        fire_wp = get_weapon_inst();
                        if (fire_wp != nullptr)
                        {
                            audiolib_cue_play_3d(fire_wp->w_info->reload_sound, &pos, &orient.fvec, nullptr, nullptr);
                        }
                    }
                }
            }
        }
    }
    if (aim)
    {
        // The first turret of a kind measures how far the muzzle sits from the base tag, sideways and up. Original: the
        // table's entry is written through, though const everywhere else.
        if (!t_info->flags.aim_offset_valid && wp->char_inst != nullptr)
        {
            matrix43* matrices;
            vector base_tag;
            if (animlib_get_instanced_model_matrices(wp->char_inst->aim_handle, &matrices) && get_tag_local_coords(0, base_tag, nullptr))
            {
                vector muzzle;
                wp->get_muzzle_transform(muzzle, nullptr);
                vec_change_to_local_coords(muzzle, pos, orient);
                turret_info* info = const_cast<turret_info*>(t_info);
                info->aim_offset.x = muzzle.x - base_tag.x;
                info->aim_offset.y = muzzle.y - base_tag.y;
                info->flags.aim_offset_valid = true;
                info->aim_offset.z = 0.0f;
            }
        }
        calculate_aim_parameters();
        if (Multiplayer && multi_is_server() && multi_game_object_turret_is_client_authoritative(this))
        {
            multi_turret_state state;
            bool exceeded_extrap_time;
            if (mp_sim_state->obtain_state(multi_core_clock_get_time(), &state, exceeded_extrap_time))
            {
                multi_game_object_turret_apply_sim_state(&state, this);
            }
        }
        turret_physics_aim_pos_motors(physics);
    }

    if (stats_cheat_enabled(CHEATS_COOL_TURRETS) || wrecking_crew_is_active())
    {
        wp = get_weapon_inst();
        if (wp != nullptr)
        {
            wp->set_overheat_percent(0.0f);
        }
        wp = get_secondary_weapon_inst();
        if (wp != nullptr)
        {
            wp->set_overheat_percent(0.0f);
        }
    }
    if (t_info->flags.link_overheats)
    {
        // The hottest weapon of this and the linked turret sets the overheat of all of them.
        weapon* weapons[4] = { get_weapon_inst(), get_secondary_weapon_inst(), nullptr, nullptr };
        turret* linked = World.object_from_handle<turret>(linked_to);
        if (linked != nullptr)
        {
            weapons[2] = linked->get_weapon_inst();
            weapons[3] = linked->get_secondary_weapon_inst();
        }
        float overheat = 0.0f;
        if (weapons[0] != nullptr)
        {
            overheat = turret_clamp(weapons[0]->get_overheat_percent(), 0.0f, 1.0f);
        }
        for (int i = 1; i < 4; i++)
        {
            if (weapons[i] != nullptr)
            {
                if (overheat - weapons[i]->get_overheat_percent() < 0.0f)
                {
                    overheat = weapons[i]->get_overheat_percent();
                }
                overheat = turret_clamp(overheat, 0.0f, 1.0f);
            }
        }
        for (int i = 0; i < 4; i++)
        {
            if (weapons[i] != nullptr)
            {
                weapons[i]->set_overheat_percent(overheat);
            }
        }
    }
}

// 0x00AC6150 (:1950)
void turret::frame_process_post()
{
    if (!obj_flags.streamed || is_killed() || obj_flags.destroyed)
    {
        divergence_time.invalidate();
        return;
    }
    turret_physics* physics = turret_loaded_physics(this);
    if (physics == nullptr)
    {
        divergence_time.invalidate();
        return;
    }
    if (!t_info->flags.automated && !turret_flags.vehicle_turret)
    {
        // A free-standing turret dies with what holds it.
        if (first_parent == 0xFFFFFFFF)
        {
            turret_physics_constrain_to_floor(physics, this);
            havok_body_set_motion_type(physics->components[TURRET_COMPONENT_BASE].havok_handle, 3); // MOTION_DYNAMIC
        }
        const constraint* con = World.object_from_handle<constraint>(physics->base_constraint_handle);
        if (con == nullptr || first_parent != con->host_handle)
        {
            kill(nullptr, nullptr);
            return;
        }
    }
    vehicle* vp = get_vehicle();
    if (t_info->flags.transform_to_tag && vp != nullptr)
    {
        turret_physics_transform_to_tag(physics, vp);
    }
    turret_physics_update_objects(physics, this, vp);
    if ((t_info->flags.primary_weapon_local_door || t_info->flags.primary_weapon_all_doors) && primary_door_timer.elapsed())
    {
        if (turret_physics_process_door_weapon_unready(physics, this, vp, 2))
        {
            primary_door_timer.invalidate();
        }
    }
    if ((t_info->flags.secondary_weapon_local_door || t_info->flags.secondary_weapon_all_doors) && secondary_door_timer.elapsed())
    {
        if (turret_physics_process_door_weapon_unready(physics, this, vp, 3))
        {
            secondary_door_timer.invalidate();
        }
    }
    const weapon* wp = get_weapon_inst();
    if (wp != nullptr)
    {
        // Stuck: the weapon points more than 10 degrees away from where the motors aim.
        const float cos_pitch = (float)cos((double)physics->pitch);
        vector aim;
        aim.x = (float)sin((double)physics->heading) * cos_pitch;
        aim.y = (float)sin((double)physics->pitch);
        aim.z = (float)cos((double)physics->heading) * cos_pitch;
        vector fvec = wp->orient.fvec;
        fvec.rotate(orient);
        if (aim.x * fvec.x + fvec.y * aim.y + fvec.z * aim.z < TURRET_STUCK_AIM_DOT)
        {
            if (!divergence_time.valid())
            {
                divergence_time.set(0);
            }
        }
        else
        {
            divergence_time.invalidate();
        }
    }
    turret_physics_update_heading(physics, this);
    if (disable_for_npcs_time.valid() && disable_for_npcs_time.elapsed())
    {
        disable_for_npcs_time.invalidate();
        look_for_nearby_explosives();
    }
}

// 0x00A42D00 (:2045)
bool turret::is_functional(bool ignore_out_of_ammo) const
{
    if (!obj_flags.streamed || is_killed() || obj_flags.destroyed)
    {
        return false;
    }
    const weapon* wp = get_weapon_inst();
    return wp != nullptr && (ignore_out_of_ammo || wp->get_rounds_total() > 0);
}

// 0x00A1F7E0 (:2069)
void turret::set_hit_points_mp(unsigned int new_hp)
{
    if (Multiplayer && !multi_is_server())
    {
        hit_points = new_hp;
    }
}

// 0x00A1F810 (:2075)
bool turret::object_is_physical_part(unsigned int h) const
{
    return h != 0xFFFFFFFF && (h == get_handle() || h == primary_weapon_handle || h == secondary_weapon_handle);
}

// 0x00A1F840 (:2084): fdot is the top's forward along the base's forward, rdot along the base's right. The spline runs
// from 1 (left, 5 on the right side) over front (3) ... see turret_info.cpp.
void turret::calculate_fov_blending(float fdot, float rdot, float& max, float& min) const
{
    fdot = turret_clamp(fdot, -1.0f, 1.0f);
    if (has_side_fov_limits)
    {
        const float t = fl_parameter(fdot, 1.0f, -1.0f, 0.0f < rdot ? 5.0f : 1.0f, 3.0f, true);
        vector2 point;
        v_fov_blend_spline.get_point(point, t);
        max = point.x;
        min = point.y;
        return;
    }
    if (has_rear_fov_limits)
    {
        max = fl_parameter(fdot, 1.0f, -1.0f, t_info->v_fov_blend_spline_points[1].x, t_info->v_fov_blend_spline_points[3].x, true);
        min = fl_parameter(fdot, 1.0f, -1.0f, t_info->v_fov_blend_spline_points[1].y, t_info->v_fov_blend_spline_points[3].y, true);
        return;
    }
    max = t_info->v_fov_blend_spline_points[1].x;
    min = t_info->v_fov_blend_spline_points[1].y;
}

// 0x00A42D70 (:2125)
void turret::update_fov_blending()
{
    if (!has_rear_fov_limits)
    {
        return;
    }
    vector base_pos;
    matrix base_orient;
    vector middle_pos;
    matrix middle_orient;
    get_tag_world_coords(0, base_pos, &base_orient, nullptr, nullptr);
    get_tag_world_coords(1, middle_pos, &middle_orient, nullptr, nullptr);
    float max;
    float min;
    calculate_fov_blending(base_orient.fvec.y * middle_orient.fvec.y + middle_orient.fvec.x * base_orient.fvec.x + base_orient.fvec.z * middle_orient.fvec.z,
        middle_orient.rvec.y * base_orient.fvec.y + middle_orient.rvec.x * base_orient.fvec.x + middle_orient.rvec.z * base_orient.fvec.z, max, min);
    set_v_fov_constraints(max, min);
}

// 0x00A42EC0 (:2148)
void turret::get_kill_impulse(vector& linear, vector& angular)
{
    const turret_physics* physics = turret_loaded_physics(this);
    if (physics != nullptr)
    {
        linear = physics->kill_linear_impulse;
        angular = physics->kill_angular_impulse;
        return;
    }
    linear.zero();
    angular.zero();
}

// 0x00A42F30 (:2165): within the fov, with 10 percent to spare (a full 360 has 10 percent extra).
bool turret::can_hit_target(const vector& target) const
{
    vector pivot;
    if (!get_pivot_point(pivot))
    {
        return false;
    }
    vector dir;
    dir.x = target.x - pivot.x;
    dir.y = target.y - pivot.y;
    dir.z = target.z - pivot.z;
    dir.normalize_safe();
    const float heading = fl_angle_cap_radians(fl_atan2_safe(dir.x, dir.z) - orient.extract_heading(), -3.1415927f);
    const float limit = turret_flags.full360 ? 1.1f : 0.9f;
    if (h_fov_neg * limit > heading || heading > h_fov_pos * limit)
    {
        return false;
    }
    const float pitch = fl_angle_cap_radians(-(float)asin((double)dir.y) - orient.extract_pitch(), -3.1415927f);
    return v_cur_fov_neg * 0.9f <= pitch && pitch <= v_cur_fov_pos * 0.9f;
}

// 0x00A72B30 (:2229): an autoaiming turret keeps its own target unless an NPC operates it.
bool turret::aim_turret(const vector& new_target_pos)
{
    const human* op = World.object_from_handle<human>(current_operator);
    if ((op != nullptr && op->get_sub_type() != OT_SUB_HUMAN_PLAYER) || !should_autoaim())
    {
        target_pos = new_target_pos;
    }
    return inside_firing_arc(target_pos);
}

// 0x00A1FA00 (:2246)
void turret::turret_process_gyro_values(float& h_vel, float& v_vel)
{
    h_current_mul = t_info->h_rot_max_mul - 1.25f < 0.0f ? 1.25f : t_info->h_rot_max_mul;
    v_current_mul = t_info->v_rot_max_mul - 1.25f < 0.0f ? 1.25f : t_info->v_rot_max_mul;
    h_vel += input_gyro_get_camera_angular_velocity_value(GYRO_MODE_TURRET, GYRO_CAMERA_AXIS_X, Player_controller_index);
    v_vel += input_gyro_get_camera_angular_velocity_value(GYRO_MODE_TURRET, GYRO_CAMERA_AXIS_Y, Player_controller_index);
    h_vel = turret_clamp(h_vel, -2.2f, 2.2f);
    v_vel = turret_clamp(v_vel, -2.5f, 2.5f);
}

// 0x00A43150 (:2260): a player's aiming: the stick ramps up to the turret's speed; the mouse scales with the frame time
// (0.033 s = 30 fps as the base) and the sensitivity.
void turret::set_turret_velocity(float h_vel, float v_vel, control_axis_action h_action, control_axis_action v_action)
{
    turret_physics* physics = turret_loaded_physics(this);
    if (!control_mouse_camera_control(h_action, v_action) && !control_steam_camera_control())
    {
        if (input_get_gyro_enabled(GYRO_MODE_TURRET))
        {
            turret_process_gyro_values(h_vel, v_vel);
        }
        else
        {
            if (fabsf(h_vel) <= 0.97f)
            {
                h_current_mul = 1.0f;
            }
            else
            {
                h_current_mul = turret_clamp(t_info->h_rot_max_ramp_time * Frametime * t_info->h_rot_max_mul + h_current_mul, -t_info->h_rot_max_mul, t_info->h_rot_max_mul);
            }
            if (fabsf(v_vel) <= 0.97f)
            {
                v_current_mul = 1.0f;
            }
            else
            {
                v_current_mul = turret_clamp(t_info->v_rot_max_ramp_time * Frametime * t_info->v_rot_max_mul + v_current_mul, -t_info->v_rot_max_mul, t_info->v_rot_max_mul);
            }
        }
        turret_physics_aim_vel_motors(physics, t_info->h_rot_speed * h_current_mul * h_vel, t_info->v_rot_speed * v_current_mul * v_vel);
        return;
    }
    float final_h;
    float final_v;
    if (!Turret_use_old_controls)
    {
        int dx;
        int dy;
        int wheel;
        mouse_get_deltas(dx, dy, wheel);
        float mouse_h = (float)dx;
        float mouse_v = (float)dy;
        if (control_steam_camera_control())
        {
            mouse_h = h_vel;
            mouse_v = v_vel;
        }
        const float frame_time = Frametime - 0.001f < 0.0f ? 0.001f : Frametime;
        mouse_h = (0.033f / frame_time) * mouse_h;
        mouse_v = (0.033f / frame_time) * mouse_v;
        if (controls_get_axis_action_inverted_state(CAA_CAMERA_ROTATE, Player_controller_index, true))
        {
            mouse_h = -mouse_h;
        }
        if (controls_get_axis_action_inverted_state(CAA_CAMERA_ELEVATE, Player_controller_index, true))
        {
            mouse_v = -mouse_v;
        }
        final_h = Turret_pc_multiplier * mouse_h * Mouse_sensitivity * Camera_first_person_look_sensitivity_heading;
        final_v = Turret_pc_multiplier * mouse_v * Turret_pc_vertical_multiplier * Mouse_sensitivity * Camera_first_person_look_sensitivity_pitch;
        if (input_get_gyro_enabled(GYRO_MODE_TURRET))
        {
            float gyro_h = 0.0f;
            float gyro_v = 0.0f;
            turret_process_gyro_values(gyro_h, gyro_v);
            final_v = t_info->v_rot_speed * v_current_mul * gyro_v + final_v;
            final_h = t_info->h_rot_speed * h_current_mul * gyro_h + final_h;
        }
    }
    else
    {
        h_current_mul = t_info->h_rot_max_mul - 1.25f < 0.0f ? 1.25f : t_info->h_rot_max_mul;
        v_current_mul = 0.0f <= t_info->v_rot_max_mul - 1.25f ? t_info->v_rot_max_mul : 1.25f;
        const float old_h = turret_clamp(Turret_pc_multiplier * h_vel, -2.2f, 2.2f);
        const float old_v = turret_clamp(Turret_pc_multiplier * 1.2f * v_vel, -2.5f, 2.5f);
        final_v = t_info->v_rot_speed * v_current_mul * old_v;
        final_h = t_info->h_rot_speed * h_current_mul * old_h;
    }
    turret_physics_aim_vel_motors(physics, final_h, final_v);
}

// 0x00A437F0 (:2372): within 15 degrees of the muzzle (cos 15 = 0.9659258).
bool turret::inside_firing_arc(const vector& target_posp) const
{
    const weapon* wp = get_weapon_inst();
    if (wp == nullptr)
    {
        return false;
    }
    vector muzzle_pos;
    matrix muzzle_orient;
    wp->get_muzzle_transform(muzzle_pos, &muzzle_orient);
    vector dir;
    vec_normalized_dir(dir, target_posp, muzzle_pos);
    return 0.9659258f <= muzzle_orient.fvec.y * dir.y + dir.x * muzzle_orient.fvec.x + dir.z * muzzle_orient.fvec.z;
}

// 0x00A438D0 (:2388): where an operator stands, 1.2 m behind (a stationary operator also 0.86 m below the weapon).
vector turret::get_operation_position() const
{
    const weapon* wp = get_weapon_inst();
    vector result;
    if (t_info->flags.stationary_operator && wp != nullptr)
    {
        result.x = wp->pos.x - orient.fvec.x * 1.2f - orient.uvec.x * 0.86f;
        result.y = wp->pos.y - orient.fvec.y * 1.2f - orient.uvec.y * 0.86f;
        result.z = wp->pos.z - orient.fvec.z * 1.2f - orient.uvec.z * 0.86f;
        return result;
    }
    const vector dir = !t_info->flags.stationary_operator && wp != nullptr ? wp->orient.fvec.copy_flatten_normalize_safe() : FORWARD_VECTOR;
    result.x = pos.x - dir.x * 1.2f;
    result.y = pos.y - dir.y * 1.2f;
    result.z = pos.z - dir.z * 1.2f;
    return result;
}

// 0x00A43AF0 (:2408)
matrix turret::get_operation_orient() const
{
    if (t_info->flags.stationary_operator)
    {
        return orient;
    }
    const weapon* wp = get_weapon_inst();
    return wp != nullptr ? wp->orient : IDENTITY_MATRIX;
}

// 0x00A43B70 (:2423): x is the pitch, y the heading.
void turret::get_target_direction(vector2& target_dir) const
{
    const turret_physics* physics = turret_loaded_physics(this);
    if (physics != nullptr)
    {
        target_dir.x = physics->pitch;
        target_dir.y = physics->heading;
        return;
    }
    target_dir.zero();
}

// 0x00A43BC0 (:2435)
void turret::set_target_direction(const vector2& target_dir)
{
    turret_physics* physics = turret_loaded_physics(this);
    if (physics != nullptr)
    {
        physics->pitch = target_dir.x;
        physics->heading = target_dir.y;
    }
}

// 0x00A96A20 (:2447): the operator becomes the weapons' owner; a player's velocity motors take over unless the player
// is a vehicle's driver of a non-tank (that seat's turret autoaims).
void turret::attach_new_operator(human* hp)
{
    if (hp == nullptr)
    {
        return;
    }
    current_operator = hp->get_handle();
    auto_return.invalidate();
    weapon* wp = get_weapon_inst();
    // Original: reads through null if the turret has no weapon.
    wp->owner_handle = hp->get_handle();
    weapon_stop_pickup_effect(wp);
    wp = get_secondary_weapon_inst();
    if (wp != nullptr)
    {
        wp->owner_handle = hp->get_handle();
        weapon_stop_pickup_effect(wp);
    }
    if (hp->get_sub_type() == OT_SUB_HUMAN_PLAYER)
    {
        hp->char_inst->set_state(index_alias(ANIMATION_INDEX_INVALID, (vanim_aliases)-1), index_alias(-1, (vanim_aliases)-1), 0.3f, false);
    }
    turret_physics* physics = turret_loaded_physics(this);
    if (physics != nullptr)
    {
        const bool is_player = hp->get_sub_type() == OT_SUB_HUMAN_PLAYER;
        bool is_player_aimed = is_player;
        if (is_player && turret_flags.vehicle_turret && get_parent() != 0xFFFFFFFF)
        {
            vehicle* vp = World.object_from_handle<vehicle>(get_parent());
            if (vp != nullptr)
            {
                const vehicle_seat_index seat = vehicle_passenger_get_seat(vp, hp);
                if (seat != VSI_INVALID)
                {
                    is_player_aimed = vehicle_get_seat_turret(vp, seat) == this;
                    if (seat == VSI_DRIVER && !vehicle_is_tank(vp))
                    {
                        is_player_aimed = false;
                    }
                }
                else
                {
                    is_player_aimed = false;
                }
            }
        }
        turret_physics_set_control_type(physics, is_player, is_player_aimed);
        turret_physics_set_no_collide(physics, this, hp->get_handle());
    }
    for (unsigned int link = linked_to; link != 0xFFFFFFFF;)
    {
        turret* linked = World.object_from_handle<turret>(link);
        if (linked == nullptr)
        {
            break;
        }
        linked->current_operator = current_operator;
        weapon* linked_wp = linked->get_weapon_inst();
        if (linked_wp != nullptr)
        {
            linked_wp->set_linked(hp);
        }
        link = linked->linked_to;
    }
}

// 0x00A96C20 (:2512)
void turret::detach_operator(human* hp)
{
    current_operator = 0xFFFFFFFF;
    weapon* wp = get_weapon_inst();
    if (wp != nullptr)
    {
        wp->owner_handle = 0xFFFFFFFF;
    }
    wp = get_secondary_weapon_inst();
    if (wp != nullptr)
    {
        wp->owner_handle = 0xFFFFFFFF;
    }
    turret_physics* physics = turret_loaded_physics(this);
    if (physics != nullptr)
    {
        turret_physics_set_control_type(physics, false, false);
        turret_physics_set_no_collide(physics, this, 0xFFFFFFFF);
    }
    for (unsigned int link = linked_to; link != 0xFFFFFFFF;)
    {
        turret* linked = World.object_from_handle<turret>(link);
        if (linked == nullptr)
        {
            break;
        }
        linked->current_operator = 0xFFFFFFFF;
        weapon* linked_wp = linked->get_weapon_inst();
        if (linked_wp != nullptr)
        {
            linked_wp->set_linked(nullptr);
            linked_wp->owner_handle = 0xFFFFFFFF;
        }
        link = linked->linked_to;
    }
    if (!t_info->flags.auto_return_dont_horizontal || !t_info->flags.auto_return_dont_vertical)
    {
        auto_return.set(10000);
    }
}

// 0x00A72BC0 (:2557): a stable turret drifting off its vehicle: pulled back from the base of the hierarchy up (sorted by
// component index); when that fails the vehicle is destroyed (100000 damage, pushed down).
void turret::vehicle_turrets_process_post()
{
    const int num = World.get_num_objects<vehicle>();
    for (int v = 0; v < num; v++)
    {
        vehicle* vp = World.get_object<vehicle>(v);
        if (!vp->rdd.is_loaded() || vp->m_num_turret_mounts <= 0 || vp->is_killed())
        {
            continue;
        }
        sorted_list_element<turret*, int> buffer[2];
        sorted_list_base<turret*, int> turrets(true, buffer, 2);
        for (int i = 0; i < vp->m_num_turret_mounts; i++)
        {
            turret* tp = World.object_from_handle<turret>(vp->m_turret_mounts[i].attached_turret);
            if (tp != nullptr && tp->obj_flags.streamed && tp->t_info->flags.maintain_constraint_stability)
            {
                turret* entry = tp;
                turrets.add(entry, vp->get_component_index_from_tag_index(tp->vehicle_attach_tag_index));
            }
        }
        for (turret* const* entry = turrets.get_first(); entry != nullptr; entry = turrets.get_next(entry))
        {
            turret_physics* physics = turret_loaded_physics(*entry);
            if (physics != nullptr && !turret_physics_fix_pos_orient(physics, vp))
            {
                const vector normal = vp->orient.uvec;
                vector impulse;
                impulse.x = normal.x * -1000.0f;
                impulse.y = normal.y * -1000.0f;
                impulse.z = normal.z * -1000.0f;
                vehicle_apply_damage(vp, nullptr, nullptr, DBT_WORLD, &vp->pos, &normal, 10.0f, 100000, &impulse, true);
                break;
            }
        }
    }
}

// 0x00A1FB20 (:3230): the turret up the item's parent chain, through items.
turret* turret_from_item(const item* ip)
{
    if (ip == nullptr)
    {
        return nullptr;
    }
    object* objp = World.object_from_handle(ip->get_parent());
    while (objp != nullptr && objp->get_obj_type() == OT_ITEM)
    {
        objp = World.object_from_handle(objp->get_parent());
    }
    return objp != nullptr && objp->get_obj_type() == OT_TURRET ? (turret*)objp : nullptr;
}

// 0x00A43C00 (:3261)
void turret_system_init()
{
    Default_turret_physical_material_index = rfg_get_physical_material_index(DEFAULT_TURRET_PHYSICAL_MATERIAL_NAME);
    Default_turret_effect_material_index = rfg_get_effect_material_index(Default_turret_physical_material_index);
    turret_info_read_table(false, -1);
}

// 0x00A72DF0 (:3273)
void turret_level_shutdown()
{
    const int num = World.get_num_objects<turret>();
    for (int i = 0; i < num; i++)
    {
        object* objp = World.get_object<turret>(i);
        objp->remote_obj_flags.client_destroy_permission = true;
        objp->remote_obj_flags.no_destroy_packet = true;
        World.destroy_object(objp, false);
    }
}

// 0x00AC6450 (:3284)
void turret_do_frame_pre()
{
    const int num = World.get_num_objects<turret>();
    for (int i = 0; i < num; i++)
    {
        turret* tp = World.get_object<turret>(i);
        human_combat_turret_do_frame_pre(tp);
        tp->frame_process_pre();
    }
}

// 0x00AC64B0 (:3293)
void turret_do_frame_post()
{
    const int num = World.get_num_objects<turret>();
    for (int i = 0; i < num; i++)
    {
        turret* tp = World.get_object<turret>(i);
        tp->frame_process_post();
        human_combat_turret_do_frame_post(tp);
    }
    turret::vehicle_turrets_process_post();
}

// 0x00A43C30 (:3304): empty in retail.
void validate_turret_destruction(const object* obj)
{
}

// 0x00A43C40 (:3319)
bool turret::weapon_cb_trigger_press(weapon_callback_idx cb_idx, weapon* wp, unsigned int user_handle)
{
    turret* tp = World.object_from_handle<turret>(user_handle);
    if (tp != nullptr)
    {
        if (wp->get_handle() == tp->primary_weapon_handle)
        {
            tp->turret_flags.primary_trigger_down = true;
        }
        if (wp->get_handle() == tp->secondary_weapon_handle)
        {
            tp->turret_flags.secondary_trigger_down = true;
        }
    }
    return true;
}

// 0x00A72E50 (:3340): doors close 5 s after the trigger is released; a resetting cycle goes back to the first tag.
bool turret::weapon_cb_trigger_release(weapon_callback_idx cb_idx, weapon* wp, unsigned int user_handle)
{
    turret* tp = World.object_from_handle<turret>(user_handle);
    if (tp == nullptr)
    {
        return true;
    }
    if (wp->get_handle() == tp->primary_weapon_handle)
    {
        tp->turret_flags.primary_trigger_down = false;
        if (tp->t_info->flags.primary_weapon_local_door || tp->t_info->flags.primary_weapon_all_doors)
        {
            tp->primary_door_timer.set(5000);
        }
        if (tp->t_info->flags.primary_reset_cycle)
        {
            tp->reset_cycle(2);
            vector tag_pos;
            tp->get_tag_world_coords(2, tag_pos, nullptr, nullptr, nullptr);
            wp->pos = tag_pos;
        }
    }
    if (wp->get_handle() == tp->secondary_weapon_handle)
    {
        tp->turret_flags.secondary_trigger_down = false;
        if (tp->t_info->flags.secondary_weapon_local_door || tp->t_info->flags.secondary_weapon_all_doors)
        {
            tp->secondary_door_timer.set(5000);
        }
        if (tp->t_info->flags.secondary_reset_cycle)
        {
            tp->reset_cycle(3);
            vector tag_pos;
            tp->get_tag_world_coords(3, tag_pos, nullptr, nullptr, nullptr);
            wp->pos = tag_pos;
        }
    }
    return true;
}

// 0x00A72F70 (:3390): after a shot: the alt effect, the next tag of a cycling weapon.
bool turret::weapon_cb_post_fire(weapon_callback_idx cb_idx, weapon* wp, unsigned int user_handle)
{
    turret* tp = World.object_from_handle<turret>(user_handle);
    if (tp == nullptr)
    {
        return true;
    }
    if (tp->should_autoaim())
    {
        tp->autoaim_keep_target_timer.set(1000);
    }
    if (wp->get_handle() == tp->primary_weapon_handle)
    {
        if (tp->t_info->primary_alt_fire_effect != 0xFFFFFFFF)
        {
            tp->play_alt_effect(2);
        }
        if (tp->t_info->flags.cycle_primary_weapon)
        {
            tp->cycle_tag(2);
            tp->get_tag_world_coords(2, wp->pos, &wp->orient, nullptr, nullptr);
        }
    }
    else if (wp->get_handle() == tp->secondary_weapon_handle)
    {
        if (tp->t_info->secondary_alt_fire_effect != 0xFFFFFFFF)
        {
            tp->play_alt_effect(3);
        }
        if (tp->t_info->flags.cycle_secondary_weapon)
        {
            tp->cycle_tag(3);
            tp->get_tag_world_coords(3, wp->pos, &wp->orient, nullptr, nullptr);
        }
    }
    return true;
}

// 0x00A73070 (:3441): a weapon may not fire while linked overheating, while the other trigger is down (unless simultaneous
// fire is allowed), or before its doors are open.
bool turret::weapon_cb_ready_to_fire(weapon_callback_idx cb_idx, weapon* wp, unsigned int user_handle)
{
    turret* tp = World.object_from_handle<turret>(user_handle);
    if (tp == nullptr)
    {
        return true;
    }
    if (tp->t_info->flags.link_overheats)
    {
        const weapon* primary = tp->get_weapon_inst();
        const weapon* secondary = tp->get_secondary_weapon_inst();
        if ((primary != nullptr && primary->is_overheated()) || (secondary != nullptr && secondary->is_overheated()))
        {
            return false;
        }
    }
    const unsigned int wp_handle = wp->get_handle();
    if (wp_handle == tp->primary_weapon_handle)
    {
        if (!tp->t_info->flags.allow_simul_fire && tp->turret_flags.secondary_trigger_down)
        {
            return false;
        }
        if (tp->t_info->flags.primary_weapon_local_door || tp->t_info->flags.primary_weapon_all_doors)
        {
            tp->primary_door_timer.invalidate();
            return tp->process_door_weapon_ready(2);
        }
    }
    if (wp_handle == tp->secondary_weapon_handle)
    {
        if (!tp->t_info->flags.allow_simul_fire && tp->turret_flags.primary_trigger_down)
        {
            return false;
        }
        if (tp->t_info->flags.secondary_weapon_local_door || tp->t_info->flags.secondary_weapon_all_doors)
        {
            tp->secondary_door_timer.invalidate();
            return tp->process_door_weapon_ready(3);
        }
    }
    return true;
}
