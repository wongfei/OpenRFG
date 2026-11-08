// Original: volition/rfg/code/objects/humans/player_cover.cpp
//
// The player's cover (NPC.md NP2). Never run. rfg.exe keeps these calculations in float chains over double, which this
// port does not reproduce. player_cover_lean_out and player_cover_stop_leaning take pp in ESI and submode in EAX, read
// from the asm at every call site.

#include "volition/rfg/code/objects/humans/player_cover.h"

#include "volition/rfg/code/camera/camera_view.h"
#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/rfg/code/control/control_map.h"
#include "volition/rfg/code/destruction/rfg_dynamics.h"
#include "volition/rfg/code/destruction/rfg_materials.h"
#include "volition/rfg/code/destruction/rfg_mover.h"
#include "volition/rfg/code/gameplay/wrecking_crew/wrecking_crew.h"
#include "volition/rfg/code/geom/character_instance.h"
#include "volition/rfg/code/geom/vanim.h"
#include "volition/rfg/code/geom/vanim_shared.h"
#include "volition/rfg/code/havok/api/hkcharactercontroller.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/havok/havok_raycast.h"
#include "volition/rfg/code/interface/ui_document_hud_rail_driver.h"
#include "volition/rfg/code/interface/ui_document_hud_sniper.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_compression.h"
#include "volition/rfg/code/multi/multi_game/multi_game_classes.h"
#include "volition/rfg/code/multi/multi_game/multi_game_object_backpack.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_combat_turret.h"
#include "volition/rfg/code/objects/humans/human_info.h"
#include "volition/rfg/code/objects/humans/human_weapons.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/level_object_properties.h"
#include "volition/rfg/code/objects/level_object_properties_shared.h"
#include "volition/rfg/code/objects/movers/object_mover.h"
#include "volition/rfg/code/objects/turret.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/os/frametime.h"
#include "volition/rfg/code/gamewide/tweak_table.h"

#include <math.h>

// player.cpp's tweak table items.
extern float Player_max_movement_speed_override; // 0x0302F0B4
extern float Mp_player_max_movement_speed_override; // 0x0302F0B8

bool Player_cover_force_fine_aim; // 0x0302F0D0
static bool Player_cover_button_press_leans; // 0x0302F0D1
static bool Mp_player_cover_use_button; // 0x0302F0D2
static tweak_table_item Tweak_table_item_Mp_player_cover_use_button("Mp_player_cover_use_button", &Mp_player_cover_use_button); // 0x03560A94
bool Player_disable_cover; // 0x0302F0D4

// Tweaks, initial values from .data.
static bool Player_cover_use_button = true; // 0x01694497
static float Player_cover_entry_stick_wall_dot = -0.5f; // 0x016944B4
static float Player_cover_entry_camera_wall_dot = -0.573f; // 0x016944B8
static bool Player_cover_allow_stick_exit = true; // 0x016944BC
static bool Player_cover_always_allow_lean = true; // 0x016944BD
static float PLAYER_COVER_MIN_MOVE_SPEED = 4.0f; // 0x016944C0
static float PLAYER_COVER_EXIT_STICK_VALUE = -0.65f; // 0x016944C4
// Each is a function-level static of its own in rfg.exe.
static const float MOVEMENT_VEHICLE_Y_OFFSET = 0.5f; // 0x016AFD70, vehicle_y_offset of player_process_cover_movement
static const float CAMERA_LEAVE_DOT = 0.4f; // 0x016AFD74
static const float CAMERA_TEST_VALUE = -0.85f; // 0x016AFE2C, camera_test_value
static const float DO_FRAME_VEHICLE_Y_OFFSET = 0.45f; // 0x016AFE30, vehicle_y_offset of player_cover_do_frame

// Animation action aliases the asm names by number.
static const int ALIAS_THROW = ANIM_ALIAS_THROW; // 0x3DB
static const int ALIAS_RELOAD = ANIM_ALIAS_RELOAD; // 0x3DD
static const int ALIAS_COMBAT_MOVE = ANIM_ALIAS_COMBAT_MOVE; // 0x3DE
static const int ALIAS_DIVE = ANIM_ALIAS_DIVE; // 0x3E3
static const int ALIAS_EQUIP = ANIM_ALIAS_EQUIP; // 0x3E9
static const int ALIAS_FACIAL_COVER = ANIMATION_ACTION_FACIAL_COVER; // 0x3D3
// Magic number: 0x40, the flags every cover exit passes to play_action.
static const unsigned int COVER_EXIT_PLAY_FLAGS = 0x40;
// Magic number: a physical material flag; a surface with it shatters when leaned against. Name not recovered.
static const unsigned int MATERIAL_FLAG_SHATTERS = 1;
// Magic number: a physical material flag; a surface with it does not count as high cover. Name not recovered.
static const unsigned int MATERIAL_FLAG_NOT_COVER = 0x80;
// Magic numbers: where the side probes start, how far they sweep and their sphere.
static const float SIDE_PROBE_BACK = 0.06999999f; // as rfg.exe's constant reads
static const float SIDE_PROBE_FORWARD = 0.88f;
static const float SIDE_PROBE_RADIUS = 0.125f;
// Magic number: raycast flags, not recovered as names.
static const unsigned int COVER_RAY_FLAGS = 0x12;

static vector cover_cross_up(const vector& v)
{
    return vector(v.y * UP_VECTOR.z - v.z * UP_VECTOR.y, v.z * UP_VECTOR.x - v.x * UP_VECTOR.z, v.x * UP_VECTOR.y - v.y * UP_VECTOR.x);
}

static void cover_input_clear(raycast_input& in)
{
    in.filter_params = nullptr;
    in.filter_callback = nullptr;
    in.m_hit_collector_type = raycast_input::COLLECT_CLOSEST_HIT;
}

// Inlined in rfg.exe wherever the zoom is dropped. Leaves a turret that forces fine aim alone.
static void cover_zoom_off(player* pp)
{
    const turret* turret_p = human_combat_get_manned_turret(pp);
    if (turret_p != nullptr && turret_p->force_fine_aim())
    {
        return;
    }
    const player_zoom_state zoom = pp->zoom_state;
    pp->previous_zoom_state = zoom;
    pp->zoom_state = PZS_OFF;
    if (zoom != PZS_OFF)
    {
        if (pp != Player)
        {
            return;
        }
        pp->get_equipped_weapon(); // Original: the result is not used.
        if (pp->previous_zoom_state == PZS_SCOPE)
        {
            pp->set_opacity_plus_child_items(1.0f);
            pp->set_casts_transparent_shadows_plus_child_items(false);
            hud_ui_rail_driver_set_visible(false);
            hud_ui_sniper_set_visible(false);
        }
    }
    if (pp == Player)
    {
        camera_set_view_player(PZS_OFF, false);
    }
}

// 0x00A86C70 (:57). Casts against the wall from the player's position, up and to the right, starting further in the
// further right it goes. A level object that explodes on death and any item count as no wall.
bool cover_cast_ray(player* pp, float up, float right, vector* normal, float sphere_size, vector* hit_pos, float dist)
{
    // Magic numbers: 0.35 back from the player, and 0.4 of the sideways distance is taken off and added to the ray.
    const float back = 0.35f - fabsf(right) * 0.4f;
    raycast_input in;
    cover_input_clear(in);
    raycast_output out;
    in.start.x = pp->cover_vector_right.x * right + (pp->pos.x - pp->cover_vector.x * back);
    in.start.y = pp->cover_vector_right.y * right + ((pp->pos.y + up) - pp->cover_vector.y * back);
    in.start.z = pp->cover_vector_right.z * right + (pp->pos.z - pp->cover_vector.z * back);
    in.radius = sphere_size;
    const float length = dist + fabsf(right) * 0.4f;
    in.end = in.start - pp->cover_vector * length;
    in.m_filter_info = COLLIDE_RAY_PLAYER_COVER;
    in.m_flags = COVER_RAY_FLAGS;
    const bool hit = sphere_size == 0.0f ? havok_raycast(in, out) : havok_spherecast(in, out);
    if (!hit)
    {
        return false;
    }
    const unsigned int hit_handle = out.m_hits[0].hit_object_handle;
    if (hit_handle != 0xFFFFFFFF)
    {
        const object* hit_obj = World.object_from_handle(hit_handle);
        if (hit_obj != nullptr && hit_obj->get_obj_type() == OT_MOVER && hit_obj->get_sub_type() == OT_SUB_MOVER_GENERAL &&
            static_cast<const object_mover*>(hit_obj)->m_level_obj_properties->m_flags.m_death_explosion != 0)
        {
            return false;
        }
    }
    // A hit with no object is read through null (original).
    if (World.object_from_handle(hit_handle)->get_obj_type() == OT_ITEM)
    {
        return false;
    }
    if (normal != nullptr)
    {
        *normal = out.m_hits[0].hit_normal.copy_flatten_normalize();
    }
    if (hit_pos != nullptr)
    {
        *hit_pos = out.m_hits[0].hit_pos;
    }
    return true;
}

// What the side and diving exits share. Magic number: the weapon may not fire for 90% of the dive animation.
static void cover_exit_delay_weapon(player* pp)
{
    weapon* wp = pp->get_equipped_weapon();
    const float length = vanim_get_anim_length(pp->char_inst->vp, ALIAS_DIVE);
    human_weapons_firearm_release_trigger(pp);
    if (wp != nullptr)
    {
        wp->one_shot_delay_timestamp.set((int)((double)length * 900.0));
    }
}

static void cover_exit_block_movement(player* pp)
{
    pp->block_movement_anim = ALIAS_DIVE;
    pp->block_movement_type = BLOCK_MOVEMENT_POS_AND_ORIENT;
    pp->block_steering_vector.zero();
    pp->block_steering_heading_offset = 0.0f;
}

// 0x00A87030 (:673)
void player_exit_cover(player* pp, bool dive)
{
    if (pp == nullptr || pp->cur_state != HS_COVER)
    {
        return;
    }
    if (dive)
    {
        bool crouch_dive = false;
        const human_move_submodes submode = pp->movement_submode;
        switch (submode)
        {
        case HMSM_COVER_RIGHT:
        case HMSM_COVER_LEFT:
            {
                // Magic number: the stick must push forward this far to dive over.
                const float forward = controls_get_analog(CAA_WALK_FORWARD_BACKWARD, Player_controller_index);
                crouch_dive = pp->hflags.crouch_cover != 0 && 0.2f <= forward;
            }
            break;
        case HMSM_COVER_RIGHT_EDGE:
        case HMSM_COVER_RIGHT_LEAN:
            pp->char_inst->play_action(pp->cur_stance == HST_CROUCH || pp->hflags.crouch_cover != 0 ? ANIMATION_ACTION_COVER_CROUCH_EXIT_RIGHT : ANIMATION_ACTION_COVER_STAND_EXIT_RIGHT, ANIM_ALIAS_DIVE, 1.0f, COVER_EXIT_PLAY_FLAGS);
            cover_exit_block_movement(pp);
            pp->rotate_dir = HUMAN_ROTATE_SHORTEST;
            cover_exit_delay_weapon(pp);
            break;
        case HMSM_COVER_LEFT_EDGE:
        case HMSM_COVER_LEFT_LEAN:
            pp->char_inst->play_action(pp->cur_stance == HST_CROUCH || pp->hflags.crouch_cover != 0 ? ANIMATION_ACTION_COVER_CROUCH_EXIT_LEFT : ANIMATION_ACTION_COVER_STAND_EXIT_LEFT, ANIM_ALIAS_DIVE, 1.0f, COVER_EXIT_PLAY_FLAGS);
            // Only this side turns first, and not from the lean (original).
            if (pp->movement_submode == HMSM_COVER_LEFT_EDGE)
            {
                const vector left = -pp->cover_vector_right;
                matrix facing;
                facing.make_quick(left);
                // Magic number: the turn takes 0.2 seconds.
                vanim_set_action_starting_orient(pp->char_inst->vp, pp->pos, pp->orient, pp->pos, facing, ALIAS_DIVE, 0.2f);
            }
            cover_exit_block_movement(pp);
            pp->rotate_dir = HUMAN_ROTATE_SHORTEST;
            cover_exit_delay_weapon(pp);
            break;
        case HMSM_COVER_RIGHT_LEANUP:
        case HMSM_COVER_LEFT_LEANUP:
            crouch_dive = true;
            break;
        default:
            break;
        }
        if (crouch_dive)
        {
            // Magic numbers: the dive needs a clear path 1.75 up, a 0.4 sphere for 0.6.
            if (!cover_cast_ray(pp, 1.75f, 0.0f, nullptr, 0.4f, nullptr, 0.6f))
            {
                pp->char_inst->play_action(submode == HMSM_COVER_LEFT_LEANUP ? ANIMATION_ACTION_COVER_CROUCH_EXIT_DIVE_LEFT : ANIMATION_ACTION_COVER_CROUCH_EXIT_DIVE, ANIM_ALIAS_DIVE, 1.0f, COVER_EXIT_PLAY_FLAGS);
                matrix facing;
                facing.make_quick(pp->cover_vector);
                vanim_set_action_starting_orient(pp->char_inst->vp, pp->pos, pp->orient, pp->pos, facing, ALIAS_DIVE, 0.2f);
                pp->char_inst->vp->flags.ignore_y = 0;
                cover_exit_delay_weapon(pp);
                // Magic number: the small capsule lasts 2 seconds.
                pp->cover_dive_capsule.set(2000);
                cover_exit_block_movement(pp);
                if (pp->rdd.is_loaded())
                {
                    pp->rdd.get_data()->controller->flags.disable_gravity = 1;
                }
                pp->rotate_dir = HUMAN_ROTATE_SHORTEST;
            }
        }
    }
    pp->hflags.check_for_cover = 0;
    pp->pflags.use_cover = 0;
    pp->pflags.no_reticule = 0;
    if (pp->combat.combat_aim_timestamp.valid() && !pp->combat.combat_aim_timestamp.elapsed())
    {
        pp->animation_flags.aim = 1;
    }
    if (HMSM_COVER_RIGHT <= pp->movement_submode && pp->movement_submode <= HMSM_COVER_LEFT_BLINDFIRE2)
    {
        pp->movement_submode = HMSM_NONE;
    }
    pp->cur_state = HS_STANDARD;
    pp->cur_movement_state = HMS_IDLE;
    pp->custom_fire_animation = ANIMATION_ACTION_NONE;
    if (pp->zoom_state != PZS_OFF)
    {
        cover_zoom_off(pp);
    }
    pp->pflags.no_fineaim = 0;
    pp->look_at_speed = 1.0f;
    // Magic number: cover cannot be entered again for 300 ms.
    pp->exiting_cover.set(300);
}

// 0x00A875C0 (:802). LTCG: pp in ESI, submode in EAX, dot on the stack. dot is how far the camera looks out on this side.
static void player_cover_lean_out(player* pp, human_move_submodes submode, float dot)
{
    if (!pp->entering_cover.elapsed() || !pp->cover_lean_delay.elapsed())
    {
        return;
    }
    if (pp->char_inst->vp->default_group_index == ANIMATION_GROUP_REMOTE_CHARGE)
    {
        return;
    }
    if (pp->movement_submode != submode)
    {
        pp->movement_submode = submode;
        pp->cover_delay_ts.set(100);
        weapon* wp = pp->get_equipped_weapon();
        if (wp != nullptr)
        {
            wp->one_shot_delay_timestamp.set(200);
        }
        pp->pflags.no_reticule = 0;
        pp->pflags.no_fineaim = 0;
        if (Player_cover_force_fine_aim)
        {
            pp->pflags.no_fineaim = 1;
            // A null weapon is read through here (original).
            if (wp->w_info->flags.can_fine_aim)
            {
                player_set_zoom_state(pp, PZS_FINE_AIM, false);
            }
            else if (wp->w_info->weapon_anim_group == ANIMATION_GROUP_SNIPER || wp->w_info->weapon_anim_group == ANIMATION_GROUP_RAILGUN)
            {
                player_set_zoom_state(pp, PZS_SCOPE, false);
            }
        }
        pp->look_at_speed = 2.0f;
    }
    // Magic number: looking back past this ends cover.
    if (dot < -0.823f)
    {
        player_exit_cover(pp, false);
        return;
    }
    if (0.0f <= dot)
    {
        vanim_set_blend_tree_control_input(pp->char_inst->vp, VANIM_STATE_DESIRED, 0.0f);
        return;
    }
    // Magic numbers: the blend input runs from 0 to 3 as the look goes back.
    float control_value = dot * -2.0f;
    if (control_value - 0.0f < 0.0f)
    {
        control_value = 0.0f;
    }
    if (0.0f <= control_value - 3.0f)
    {
        control_value = 3.0f;
    }
    vanim_set_blend_tree_control_input(pp->char_inst->vp, VANIM_STATE_DESIRED, control_value);
}

// 0x00A87750 (:846). LTCG: pp in ESI, submode in EAX.
static void player_cover_stop_leaning(player* pp, human_move_submodes submode)
{
    pp->hflags.aim_at_pos = 0;
    pp->movement_submode = submode;
    pp->combat.combat_aim_timestamp.invalidate();
    pp->combat.combat_ready_timestamp.invalidate();
    pp->cover_delay_ts.set(300);
    pp->pflags.no_reticule = 0;
    pp->pflags.no_fineaim = 1;
    pp->look_at_speed = 1.0f;
    const bool blindfiring = !pp->combat.combat_blindfire_timestamp.elapsed() && pp->combat.combat_blindfire_timestamp.valid();
    // rfg.exe inlines get_equipped_weapon here and then calls it as well.
    weapon* wp = pp->get_equipped_weapon();
    if (wp != nullptr && !blindfiring)
    {
        wp->one_shot_delay_timestamp.set(200);
    }
    if (pp->zoom_state != PZS_OFF && pp->zoom_state != PZS_SPRINT)
    {
        cover_zoom_off(pp);
    }
}

// 0x00A878E0 (:868): everything stops a lean up but the player's own weapon.
bool player_leanup_callback(object* hit_objp, void* vp)
{
    if (hit_objp != nullptr && (object*)static_cast<human*>(static_cast<player*>(vp))->get_equipped_weapon() == hit_objp)
    {
        return false;
    }
    return true;
}

// Where the lean up looks: how far down the camera looks, 0 to 1. Magic number: full by 30 degrees down.
static void cover_leanup_blend(player* pp, const vector& camera_fvec)
{
    if (pp->hflags.cover_crouch_high_only != 0)
    {
        vanim_set_blend_tree_control_input(pp->char_inst->vp, VANIM_STATE_DESIRED, 1.0f);
        return;
    }
    float control_value = camera_fvec * UP_VECTOR;
    if (0.0f <= control_value)
    {
        control_value = 0.0f;
    }
    else
    {
        control_value = -control_value * 2.0f;
        if (1.0f - control_value < 0.0f)
        {
            control_value = 1.0f;
        }
    }
    vanim_set_blend_tree_control_input(pp->char_inst->vp, VANIM_STATE_DESIRED, control_value);
}

// Whether a lean up has room above. Magic numbers: a 0.38 sphere from 1 up to 1.5 up.
static bool cover_leanup_blocked(player* pp)
{
    raycast_input in;
    raycast_output out;
    in.start = pp->pos;
    in.start.y += 1.0f;
    in.end = in.start;
    in.end.y += 0.5f;
    in.radius = 0.38f;
    in.filter_callback = player_leanup_callback;
    in.filter_params = pp;
    in.m_filter_info = COLLIDE_PLAYER;
    in.m_hit_collector_type = raycast_input::COLLECT_CLOSEST_HIT;
    in.m_flags = 0;
    return havok_spherecast(in, out);
}

// 0x00A87900 (:881)
void player_process_cover_crouch(player* pp, float, float v)
{
    const matrix& camera_orient = Multiplayer ? pp->mp_camera_orient : Rfg_camera.real_orient;
    const vector camera_fvec = camera_orient.fvec;
    bool wants_lean = false;
    if (Player_cover_button_press_leans)
    {
        wants_lean = controls_is_button_down(CBA_OFC_ATTACK_MELEE, Player_controller_index);
    }
    else
    {
        wants_lean = 0.5f < v;
    }
    if (pp->hflags.cover_crouch_no_lean != 0 || !wants_lean || pp->get_equipped_weapon() == nullptr || pp->get_equipped_weapon()->is_reloading())
    {
        const human_move_submodes submode = pp->movement_submode;
        pp->pflags.cover_leanup = 0;
        switch (submode)
        {
        case HMSM_COVER_RIGHT_LEANUP:
            player_cover_stop_leaning(pp, HMSM_COVER_RIGHT);
            break;
        case HMSM_COVER_LEFT_LEANUP:
            player_cover_stop_leaning(pp, HMSM_COVER_LEFT);
            break;
        case HMSM_COVER_RIGHT_BLINDFIRE2:
        case HMSM_COVER_LEFT_BLINDFIRE2:
            cover_leanup_blend(pp, camera_fvec);
            break;
        default:
            break;
        }
        return;
    }
    switch (pp->movement_submode)
    {
    case HMSM_COVER_RIGHT:
    case HMSM_COVER_RIGHT_EDGE:
    case HMSM_COVER_RIGHT_BLINDFIRE2:
        if (!cover_leanup_blocked(pp))
        {
            player_cover_lean_out(pp, HMSM_COVER_RIGHT_LEANUP, 0.0f);
            pp->pflags.cover_leanup = 1;
        }
        break;
    case HMSM_COVER_LEFT:
    case HMSM_COVER_LEFT_EDGE:
    case HMSM_COVER_LEFT_BLINDFIRE2:
        if (!cover_leanup_blocked(pp))
        {
            player_cover_lean_out(pp, HMSM_COVER_LEFT_LEANUP, 0.0f);
            pp->pflags.cover_leanup = 1;
        }
        break;
    case HMSM_COVER_RIGHT_LEANUP:
    case HMSM_COVER_LEFT_LEANUP:
        {
            const bool high_only = pp->hflags.cover_crouch_high_only != 0;
            cover_leanup_blend(pp, camera_fvec);
            if (high_only)
            {
                break;
            }
            // Magic numbers: a 0.04 sphere 1.2 up and 0.4 to the side of the lean, one metre at the wall.
            raycast_input in;
            cover_input_clear(in);
            in.m_flags = 0;
            raycast_output out;
            const float side = pp->movement_submode == HMSM_COVER_RIGHT_LEANUP ? 0.4f : -0.4f;
            const vector across = cover_cross_up(pp->cover_vector);
            in.start = pp->pos + across * side;
            in.start.y += 1.2f;
            const vector end = in.start - pp->cover_vector;
            in.end = end;
            in.m_filter_info = COLLIDE_RAY_PLAYER_COVER;
            in.radius = 0.04f;
            if (havok_spherecast(in, out))
            {
                if ((rfg_get_physical_material_by_index(out.m_hits[0].hit_physical_material)->flags & MATERIAL_FLAG_SHATTERS) != 0)
                {
                    rfg_mover* mover = World.object_from_handle<rfg_mover>(out.m_hits[0].hit_object_handle);
                    vector local_point;
                    vector local_dir;
                    convert_point_direction_from_world_to_local(local_point, local_dir, out.m_hits[0].hit_pos, out.m_hits[0].hit_normal, mover->pos, mover->orient);
                    rfg_dyn_add_pre_collide_shatter(mover, pp, 0xFFFFFFFF, 1, local_point, local_dir, false);
                }
                else
                {
                    // Magic number: 0.4 higher, the wall may end.
                    in.start.y += 0.4f;
                    in.end.y += 0.4f;
                    if (havok_spherecast(in, out))
                    {
                        pp->hflags.cover_crouch_no_lean = 1;
                    }
                    else
                    {
                        pp->hflags.cover_crouch_high_only = 1;
                    }
                }
            }
        }
        break;
    default:
        break;
    }
}

// Blind fire from a crouch: looks at the wall above, then settles on the blind fire pose. side is 1 for right, -1 for left.
static void cover_blindfire_crouch(player* pp, weapon* wp, float side, human_move_submodes submode)
{
    // Magic numbers: 0.4 to the side and 1.2 up, one metre at the wall, and 0.55 higher for the second look.
    raycast_input in;
    in.radius = 0.0f;
    in.start = pp->pos + pp->cover_vector_right * (side * 0.4f);
    in.start.y += 1.2f;
    in.end = in.start - pp->cover_vector;
    cover_input_clear(in);
    in.m_flags = 0;
    in.m_filter_info = COLLIDE_RAY_PLAYER_COVER;
    raycast_output out;
    if (havok_raycast(in, out))
    {
        if ((rfg_get_physical_material_by_index(out.m_hits[0].hit_physical_material)->flags & MATERIAL_FLAG_SHATTERS) == 0)
        {
            in.start.y += 0.55f;
            in.end.y += 0.55f;
            if (havok_spherecast(in, out))
            {
                pp->hflags.cover_crouch_no_lean = 1;
            }
            else
            {
                pp->hflags.cover_crouch_high_only = 1;
            }
        }
        else
        {
            rfg_mover* mover = World.object_from_handle<rfg_mover>(out.m_hits[0].hit_object_handle);
            vector local_point;
            vector local_dir;
            convert_point_direction_from_world_to_local(local_point, local_dir, out.m_hits[0].hit_pos, out.m_hits[0].hit_normal, mover->pos, mover->orient);
            rfg_dyn_add_pre_collide_shatter(mover, pp, 0xFFFFFFFF, 1, local_point, local_dir, false);
        }
    }
    pp->movement_submode = submode;
    if (200 < wp->time_since_fired())
    {
        wp->one_shot_delay_timestamp.set(200);
    }
}

// 0x00A880D0 (:1048)
void player_process_cover_blindfire(player* pp)
{
    // rfg.exe inlines get_equipped_weapon here.
    weapon* wp = pp->get_equipped_weapon();
    if (wp == nullptr)
    {
        return;
    }
    pp->hflags.aim_at_pos = 0;
    pp->combat.combat_ready_timestamp.set(300);
    switch (pp->movement_submode)
    {
    case HMSM_COVER_RIGHT:
        if (pp->hflags.crouch_cover != 0)
        {
            cover_blindfire_crouch(pp, wp, 1.0f, HMSM_COVER_RIGHT_BLINDFIRE2);
        }
        break;
    case HMSM_COVER_LEFT:
        if (pp->hflags.crouch_cover != 0)
        {
            cover_blindfire_crouch(pp, wp, -1.0f, HMSM_COVER_LEFT_BLINDFIRE2);
        }
        break;
    case HMSM_COVER_RIGHT_EDGE:
        pp->movement_submode = HMSM_COVER_RIGHT_BLINDFIRE;
        if (200 < wp->time_since_fired())
        {
            wp->one_shot_delay_timestamp.set(200);
        }
        break;
    case HMSM_COVER_LEFT_EDGE:
        pp->movement_submode = HMSM_COVER_LEFT_BLINDFIRE;
        if (200 < wp->time_since_fired())
        {
            wp->one_shot_delay_timestamp.set(200);
        }
        break;
    case HMSM_COVER_RIGHT_BLINDFIRE2:
    case HMSM_COVER_LEFT_BLINDFIRE2:
        if (pp->hflags.crouch_cover == 0)
        {
            pp->pflags.no_reticule = 1;
            human_weapons_firearm_release_trigger(pp);
        }
        break;
    default:
        break;
    }
}

// The lean toward side (1 right, -1 left): if the wall still blocks at the edge, stop leaning. Standing, a wall that ends
// 0.45 lower means crouching. Magic numbers: from 1.45 up standing or 1.0 crouching, 0.5 to the side, 0.2 further at the end.
static void cover_process_lean(player* pp, float side, float dot, human_move_submodes lean_submode, human_move_submodes edge_submode)
{
    const bool standing = pp->hflags.crouch_cover == 0 && pp->cur_stance != HST_CROUCH;
    const float height = standing ? 1.45f : 1.0f;
    raycast_input in;
    cover_input_clear(in);
    raycast_output out;
    in.start = pp->pos - pp->cover_vector * SIDE_PROBE_BACK + pp->cover_vector_right * (side * 0.5f);
    in.start.y += height;
    in.radius = SIDE_PROBE_RADIUS;
    in.end = in.start - pp->cover_vector * SIDE_PROBE_FORWARD + pp->cover_vector_right * (side * 0.2f);
    in.m_filter_info = COLLIDE_RAY_PLAYER_COVER;
    in.m_flags = COVER_RAY_FLAGS;
    const bool blocked = havok_spherecast(in, out);
    if (standing)
    {
        if (blocked)
        {
            in.start.y -= 0.45f;
            in.end.y -= 0.45f;
            if (havok_spherecast(in, out))
            {
                player_cover_stop_leaning(pp, edge_submode);
                return;
            }
            pp->hflags.crouch_cover = 1;
        }
    }
    else if (blocked)
    {
        player_cover_stop_leaning(pp, edge_submode);
        return;
    }
    player_cover_lean_out(pp, lean_submode, dot);
}

// Sliding along the wall toward side: the sideways distance to probe for this frame. Magic numbers: speeds up at 10 a
// second to 4.4, or down at 14 to 2 near an edge; the probe starts 0.35 out and grows with the speed.
static float cover_slide_distance(player* pp, float side, float speed_scale)
{
    const float target = pp->cover_approaching_edge ? 2.0f : 4.4f;
    float vel = pp->cover_move_vel;
    if (target < vel)
    {
        vel -= Frametime * (pp->cover_approaching_edge ? 14.0f : 10.0f);
        if (0.0f <= target - vel)
        {
            vel = target;
        }
    }
    else
    {
        vel += Frametime * 10.0f;
        if (target - vel < 0.0f)
        {
            vel = target;
        }
    }
    pp->cover_move_vel = vel;
    float dist;
    if (!pp->cover_approaching_edge)
    {
        dist = (pp->cover_move_vel - 1.0f) * speed_scale + 0.35f;
        if (dist - 0.35f < 0.0f)
        {
            dist = 0.35f;
        }
    }
    else
    {
        const vector to_hit = pp->cover_edge_hit_pos - pp->pos;
        const vector to_miss = pp->cover_edge_miss_pos - pp->pos;
        dist = (to_hit.mag() + to_miss.mag()) * 0.5f;
    }
    return dist * side;
}

// The end of a slide: probes the wall ahead and steers along it, or stops at the edge. dist is signed.
static void cover_slide(player* pp, float h, float dist, float max_speed)
{
    bool first_miss = false;
    bool found_wall = false;
    for (;;)
    {
        const float offset = pp->pflags.cover_against_vehicle != 0 ? MOVEMENT_VEHICLE_Y_OFFSET : 0.0f;
        // Magic numbers: 0.1 spheres 0.5 and, standing, 1.5 up, 0.8 long.
        if (cover_cast_ray(pp, offset + 0.5f, dist, nullptr, 0.1f, nullptr, 0.8f) &&
            (pp->hflags.crouch_cover != 0 || pp->cur_stance != HST_STAND || cover_cast_ray(pp, 1.5f, dist, nullptr, 0.1f, nullptr, 0.8f)))
        {
            found_wall = true;
            break;
        }
        const bool was_approaching = pp->cover_approaching_edge;
        pp->cover_edge_miss_pos = pp->pos + pp->cover_vector_right * dist;
        if (was_approaching && !first_miss)
        {
            break;
        }
        first_miss = true;
        pp->cover_approaching_edge = true;
        pp->cover_approaching_edge_ts.set(1000);
        if (fabsf(dist) <= 0.35f)
        {
            break;
        }
        dist += dist <= 0.0f ? 0.05f : -0.05f;
    }
    if (found_wall)
    {
        pp->cover_edge_hit_pos = pp->pos + pp->cover_vector_right * dist;
        if (!first_miss && pp->cover_approaching_edge_ts.elapsed())
        {
            pp->cover_approaching_edge = false;
        }
    }
    if (!pp->cover_approaching_edge || 0.35f < fabsf(dist))
    {
        pp->steering_vector = dist <= 0.0f ? -pp->cover_vector_right : pp->cover_vector_right;
        // Magic number: leans into the wall a little.
        pp->steering_vector -= pp->cover_vector * 0.25f;
        pp->steering_vector.normalize();
        pp->move_speed = pp->cover_move_vel;
        if (pp->cur_stance == HST_CROUCH || pp->hflags.crouch_cover != 0)
        {
            // Magic number: crouched, 70% of the speed.
            pp->move_speed = pp->cover_move_vel * 0.7f;
        }
        pp->move_speed = fabsf(h) * pp->move_speed;
        pp->cur_state = HS_COVER;
        pp->cur_movement_state = HMS_WALK;
        if (pp->cover_test_queue == PCT_DONE)
        {
            pp->cover_test_queue = PCT_CHECK_COVER_NORMAL;
        }
    }
    else if (pp->hflags.side_firing == 0)
    {
        pp->pflags.no_reticule = 0;
        pp->pflags.no_fineaim = 0;
        pp->movement_submode = h <= 0.0f ? HMSM_COVER_LEFT_EDGE : HMSM_COVER_RIGHT_EDGE;
        if (Player_cover_force_fine_aim)
        {
            pp->pflags.no_fineaim = 1;
        }
        pp->cover_delay_ts.set(100);
        pp->move_speed = 0.0f;
        pp->cur_state = HS_COVER;
        pp->cur_movement_state = HMS_IDLE;
    }
    vanim_set_blend_tree_control_input(pp->char_inst->vp, VANIM_STATE_DESIRED, pp->move_speed / max_speed);
}

// The stick pushes sideways by more than the dead zone. The two sides are mirrors in rfg.exe. dir is 1 for right, -1 for
// left; the "own" submodes are on the side of the push and the "other" ones across.
static void cover_process_sideways(player* pp, float h, float dir, const vector& camera_fvec, float speed_scale, float max_speed)
{
    const bool right = 0.0f < dir;
    const human_move_submodes own_cover = right ? HMSM_COVER_RIGHT : HMSM_COVER_LEFT;
    const human_move_submodes own_edge = right ? HMSM_COVER_RIGHT_EDGE : HMSM_COVER_LEFT_EDGE;
    const human_move_submodes own_lean = right ? HMSM_COVER_RIGHT_LEAN : HMSM_COVER_LEFT_LEAN;
    const human_move_submodes own_blindfire = right ? HMSM_COVER_RIGHT_BLINDFIRE : HMSM_COVER_LEFT_BLINDFIRE;
    const human_move_submodes own_blindfire2 = right ? HMSM_COVER_RIGHT_BLINDFIRE2 : HMSM_COVER_LEFT_BLINDFIRE2;
    const human_move_submodes other_cover = right ? HMSM_COVER_LEFT : HMSM_COVER_RIGHT;
    const human_move_submodes other_edge = right ? HMSM_COVER_LEFT_EDGE : HMSM_COVER_RIGHT_EDGE;
    const human_move_submodes other_lean = right ? HMSM_COVER_LEFT_LEAN : HMSM_COVER_RIGHT_LEAN;
    const human_move_submodes other_blindfire = right ? HMSM_COVER_LEFT_BLINDFIRE : HMSM_COVER_RIGHT_BLINDFIRE;
    const human_move_submodes other_blindfire2 = right ? HMSM_COVER_LEFT_BLINDFIRE2 : HMSM_COVER_RIGHT_BLINDFIRE2;

    human_move_submodes submode = pp->movement_submode;
    // Magic number: turning away from the other side takes a firmer push.
    if ((submode == other_cover || submode == other_edge) && fabsf(h) < 0.8f)
    {
        return;
    }
    if (submode == HMSM_COVER_RIGHT_LEANUP || submode == HMSM_COVER_LEFT_LEANUP)
    {
        pp->move_speed = 0.0f;
        pp->cur_state = HS_COVER;
        pp->cur_movement_state = HMS_IDLE;
        return;
    }
    pp->hflags.cover_crouch_high_only = 0;
    pp->hflags.cover_crouch_no_lean = 0;
    if (submode == other_lean || submode == other_blindfire)
    {
        player_cover_stop_leaning(pp, other_edge);
        return;
    }
    bool slide = false;
    if (submode == own_edge || submode == own_blindfire)
    {
        if (Player_cover_button_press_leans)
        {
            return;
        }
    }
    else if (submode == own_lean)
    {
        if (Player_cover_button_press_leans)
        {
            // rfg.exe jumps to the slide code, whose first test ends the lean.
            player_cover_stop_leaning(pp, own_edge);
            return;
        }
    }
    else
    {
        slide = true;
    }
    if (!slide)
    {
        const float dot = dir * (camera_fvec * pp->cover_vector_right);
        // Magic number: looking back past this ends the lean.
        if (dot < -0.822f)
        {
            player_cover_stop_leaning(pp, own_edge);
            return;
        }
        cover_process_lean(pp, dir, dot, own_lean, own_edge);
        return;
    }

    if (submode == other_blindfire2)
    {
        weapon* wp = pp->get_equipped_weapon();
        if (wp != nullptr)
        {
            human_weapons_firearm_release_trigger(pp);
            wp->one_shot_delay_timestamp.set(600);
        }
    }
    submode = pp->movement_submode;
    if (submode == other_cover || submode == other_edge || submode == other_blindfire2)
    {
        pp->cover_approaching_edge = false;
        if (pp->hflags.side_firing == 0)
        {
            pp->cover_move_vel = 0.0f;
        }
        pp->cover_lean_delay.set(300);
        pp->custom_fire_animation = ANIMATION_ACTION_NONE;
    }
    submode = pp->movement_submode;
    if (submode != own_blindfire2)
    {
        if (pp->hflags.side_firing == 0)
        {
            pp->movement_submode = own_cover;
            if (pp->hflags.crouch_cover == 0)
            {
                pp->pflags.no_reticule = 1;
            }
        }
        else
        {
            pp->hflags.side_firing_walk_back = submode == other_cover ? 1 : 0;
        }
    }
    cover_slide(pp, h, cover_slide_distance(pp, dir, speed_scale), max_speed);
}

// 0x00A886F0 (:1189)
void player_process_cover_movement(player* pp, float h, float v)
{
    const matrix& camera_orient = Multiplayer ? pp->mp_camera_orient : Rfg_camera.real_orient;
    const bool use_button = Multiplayer ? Mp_player_cover_use_button : Player_cover_use_button;
    const vector camera_fvec = camera_orient.fvec;
    vector stick(h, 0.0f, v);
    const float wall_heading = fl_atan2_safe(-pp->cover_vector.x, -pp->cover_vector.z);
    const float camera_heading = camera_orient.extract_heading();
    stick.rotate_around_y_axis(camera_heading - wall_heading);
    h = stick.x;
    v = stick.z;
    // Magic number: the dead zone that drops an edge search.
    if (pp->pflags.cover_search_for_edge != 0 && (0.05f <= stick.x || 0.05f <= -stick.x) && (0.05f <= stick.z || 0.05f <= -stick.z))
    {
        pp->pflags.cover_search_for_edge = 0;
    }
    if (((CAMERA_LEAVE_DOT < camera_fvec * pp->cover_vector || stick.z < PLAYER_COVER_EXIT_STICK_VALUE) && (pp != Player || !use_button || Player_cover_allow_stick_exit)) ||
        (pp == Player && use_button && pp->pflags.use_cover != 0))
    {
        // Magic number: a wall that just turned gives the player 100 ms before cover ends.
        if (pp->last_frame_cover_collision_normal * pp->cover_collision_normal < 0.95f)
        {
            pp->cover_exit_delay_ts.set(100);
        }
        if (!pp->cover_exit_delay_ts.valid() || pp->cover_exit_delay_ts.elapsed())
        {
            player_exit_cover(pp, false);
            return;
        }
    }
    {
        const weapon* wp = pp->get_equipped_weapon();
        if (wp != nullptr && wp->w_info->flags.disable_player_cover)
        {
            player_exit_cover(pp, false);
            return;
        }
    }
    pp->last_frame_cover_collision_normal = pp->cover_collision_normal;

    bool throwing = false;
    if (pp->char_inst->is_playing_action(ALIAS_THROW))
    {
        v = 0.0f;
        h = 0.0f;
        if (human_in_vehicle_interaction(pp, nullptr))
        {
            pp->combat.combat_ready_timestamp.invalidate();
        }
        else if (!pp->combat.combat_ready_timestamp.valid() || pp->combat.combat_ready_timestamp.time_until() < 1500)
        {
            pp->combat.combat_ready_timestamp.set(1500);
        }
        pp->combat.combat_aim_timestamp.set(1000);
        throwing = true;
    }
    if (pp->char_inst->is_playing_action(ALIAS_RELOAD) || pp->char_inst->is_playing_action(ALIAS_EQUIP))
    {
        const human_move_submodes submode = pp->movement_submode;
        pp->pflags.cover_firing = 0;
        v = 0.0f;
        if ((submode == HMSM_COVER_RIGHT_EDGE || submode == HMSM_COVER_RIGHT_LEAN) && 0.0f < h)
        {
            h = 0.0f;
        }
        else if ((submode == HMSM_COVER_LEFT_EDGE || submode == HMSM_COVER_LEFT_LEAN) && h < 0.0f)
        {
            h = 0.0f;
        }
    }
    pp->steering_vector = pp->cover_vector;
    pp->steering_heading_offset = 0.0f;
    pp->move_speed = 0.0f;
    pp->direct_control_vel.zero();

    float max_speed;
    float speed_override = Player_max_movement_speed_override;
    if (!Multiplayer)
    {
        max_speed = pp->h_info->get_max_speed();
    }
    else
    {
        max_speed = multi_game_classes_get_default_class()->max_speed;
        speed_override = Mp_player_max_movement_speed_override;
    }
    if (0.0f < speed_override)
    {
        max_speed = speed_override;
    }

    if (pp->hflags.crouch_cover != 0)
    {
        player_process_cover_crouch(pp, h, v);
    }
    else if ((pp->movement_submode == HMSM_COVER_RIGHT || pp->movement_submode == HMSM_COVER_LEFT) && !Player_cover_always_allow_lean)
    {
        pp->pflags.no_fineaim = 1;
    }

    if (!pp->combat.combat_blindfire_timestamp.elapsed() && pp->combat.combat_blindfire_timestamp.valid())
    {
        player_process_cover_blindfire(pp);
    }
    else
    {
        bool leave_combat = false;
        switch (pp->movement_submode)
        {
        case HMSM_COVER_RIGHT:
        case HMSM_COVER_RIGHT_EDGE:
        case HMSM_COVER_LEFT:
        case HMSM_COVER_LEFT_EDGE:
            {
                weapon* wp = pp->get_equipped_weapon();
                // Magic number: side firing needs the camera to look along the wall at least this much.
                if (camera_fvec.copy_flatten_normalize() * pp->cover_vector <= -0.6f || wp == nullptr || wp->w_info->weapon_class == WEAPON_CLASS_THROWN_CHARGE)
                {
                    pp->pflags.no_reticule = 1;
                    leave_combat = true;
                    pp->hflags.side_firing = 0;
                    if (pp->zoom_state != PZS_OFF)
                    {
                        cover_zoom_off(pp);
                    }
                    human_weapons_firearm_release_trigger(pp);
                    if (wp != nullptr)
                    {
                        wp->flags.delay_fire = false;
                        wp->one_shot_delay_timestamp.set(0);
                        pp->pflags.quick_turn = 0;
                    }
                }
                else
                {
                    pp->hflags.side_firing = 1;
                    pp->pflags.no_reticule = 0;
                    pp->pflags.no_fineaim = 0;
                    human_set_combat_ready(pp, true, 1000);
                    pp->movement_submode = camera_fvec * pp->orient.rvec <= 0.0f ? HMSM_COVER_RIGHT : HMSM_COVER_LEFT;
                }
                if (!pp->char_inst->is_playing_action(ALIAS_FACIAL_COVER))
                {
                    pp->char_inst->play_action(ANIMATION_ACTION_FACIAL_COVER, ANIM_ALIAS_FACIAL_MORPH, 1.0f, 0);
                }
                if (pp->hflags.crouch_cover != 0)
                {
                    pp->pflags.no_fineaim = 0;
                }
            }
            break;
        case HMSM_COVER_RIGHT_LEAN:
        case HMSM_COVER_LEFT_LEAN:
        case HMSM_COVER_RIGHT_LEANUP:
        case HMSM_COVER_LEFT_LEANUP:
            {
                human_set_combat_ready(pp, true, 12000);
                pp->combat.combat_aim_timestamp.set(300);
                weapon* wp = pp->get_equipped_weapon();
                if (wp != nullptr)
                {
                    wp->one_shot_delay_timestamp.set(0);
                }
            }
            break;
        case HMSM_COVER_RIGHT_BLINDFIRE:
            pp->movement_submode = HMSM_COVER_RIGHT_EDGE;
            leave_combat = true;
            break;
        case HMSM_COVER_LEFT_BLINDFIRE:
            pp->movement_submode = HMSM_COVER_LEFT_EDGE;
            leave_combat = true;
            break;
        case HMSM_COVER_RIGHT_BLINDFIRE2:
            pp->movement_submode = HMSM_COVER_RIGHT;
            leave_combat = true;
            break;
        case HMSM_COVER_LEFT_BLINDFIRE2:
            pp->movement_submode = HMSM_COVER_LEFT;
            leave_combat = true;
            break;
        default:
            break;
        }
        if (leave_combat && !throwing)
        {
            pp->hflags.aim_at_pos = 0;
            pp->combat.combat_aim_timestamp.invalidate();
            pp->combat.combat_ready_timestamp.invalidate();
            weapon* wp = pp->get_equipped_weapon();
            if (wp != nullptr)
            {
                wp->one_shot_delay_timestamp.set(200);
            }
        }
    }

    if (!pp->cover_delay_ts.elapsed())
    {
        return;
    }
    if (Player_cover_button_press_leans)
    {
        bool button_lean = false;
        if (controls_is_button_down(CBA_OFC_ATTACK_MELEE, Player_controller_index))
        {
            const weapon* wp = pp->get_equipped_weapon();
            button_lean = wp != nullptr && !wp->is_reloading();
        }
        if (button_lean)
        {
            const float dot = camera_fvec * pp->cover_vector_right;
            switch (pp->movement_submode)
            {
            case HMSM_COVER_RIGHT:
            case HMSM_COVER_LEFT:
                if (pp->hflags.crouch_cover == 0)
                {
                    pp->pflags.cover_search_for_edge = 1;
                }
                break;
            case HMSM_COVER_RIGHT_EDGE:
            case HMSM_COVER_RIGHT_LEAN:
            case HMSM_COVER_RIGHT_BLINDFIRE:
                player_cover_lean_out(pp, HMSM_COVER_RIGHT_LEAN, dot);
                return;
            case HMSM_COVER_LEFT_EDGE:
            case HMSM_COVER_LEFT_LEAN:
            case HMSM_COVER_LEFT_BLINDFIRE:
                player_cover_lean_out(pp, HMSM_COVER_LEFT_LEAN, -dot);
                return;
            default:
                break;
            }
        }
        else if (pp->movement_submode == HMSM_COVER_RIGHT_LEAN)
        {
            player_cover_stop_leaning(pp, HMSM_COVER_RIGHT_EDGE);
            return;
        }
        else if (pp->movement_submode == HMSM_COVER_LEFT_LEAN)
        {
            player_cover_stop_leaning(pp, HMSM_COVER_LEFT_EDGE);
            return;
        }
    }
    if (pp->pflags.cover_search_for_edge != 0)
    {
        // Magic numbers: looks 1.25 along the wall, and walks that way at full stick until the wall ends.
        if (pp->movement_submode == HMSM_COVER_RIGHT)
        {
            if (cover_cast_ray(pp, 0.5f, 1.25f, nullptr, 0.1f, nullptr, 0.6f))
            {
                pp->pflags.cover_search_for_edge = 0;
            }
            else
            {
                h = 1.0f;
            }
        }
        else if (pp->movement_submode == HMSM_COVER_LEFT)
        {
            if (cover_cast_ray(pp, 0.5f, -1.25f, nullptr, 0.1f, nullptr, 0.6f))
            {
                pp->pflags.cover_search_for_edge = 0;
            }
            else
            {
                h = -1.0f;
            }
        }
        else
        {
            pp->pflags.cover_search_for_edge = 0;
        }
    }
    if (pp->char_inst->vp->default_group_index == ANIMATION_GROUP_REMOTE_CHARGE && pp->custom_fire_animation != ANIMATION_ACTION_NONE)
    {
        pp->pflags.no_reticule = 0;
    }
    // Magic number: how fast the probe distance grows with the speed, slower crouching.
    const float speed_scale = pp->cur_stance == HST_CROUCH || pp->hflags.crouch_cover != 0 ? 0.1f : 0.18f;
    // Magic number: the stick's dead zone.
    if (h < -0.3f)
    {
        cover_process_sideways(pp, h, -1.0f, camera_fvec, speed_scale, max_speed);
    }
    else if (0.3f < h)
    {
        cover_process_sideways(pp, h, 1.0f, camera_fvec, speed_scale, max_speed);
    }
    else
    {
        if (!Player_cover_button_press_leans)
        {
            if (pp->movement_submode == HMSM_COVER_RIGHT_LEAN)
            {
                player_cover_stop_leaning(pp, HMSM_COVER_RIGHT_EDGE);
                return;
            }
            if (pp->movement_submode == HMSM_COVER_LEFT_LEAN)
            {
                player_cover_stop_leaning(pp, HMSM_COVER_LEFT_EDGE);
                return;
            }
        }
        if (pp->cover_test_queue == PCT_DONE)
        {
            pp->cover_test_queue = PCT_THREE_FRAME_DELAY;
        }
        pp->move_speed = 0.0f;
        pp->cur_state = HS_COVER;
        pp->cur_movement_state = HMS_IDLE;
    }
}

// 0x00A11580 (:2012): true when no search started.
bool player_cover_search_for_edge(player* pp)
{
    if (pp->hflags.crouch_cover == 0 && pp->hflags.side_firing == 0 && (pp->movement_submode == HMSM_COVER_RIGHT || pp->movement_submode == HMSM_COVER_LEFT))
    {
        pp->pflags.cover_search_for_edge = 1;
        return false;
    }
    return true;
}

// Entering cover: the wall has to be real over most of the player's width and height.
static void cover_try_enter(player* pp, const matrix* camera_orient, float wall_dot)
{
    pp->cover_vector = pp->cover_collision_normal;
    pp->cover_vector_right = cover_cross_up(pp->cover_vector);
    const float offset = pp->pflags.cover_against_vehicle != 0 ? DO_FRAME_VEHICLE_Y_OFFSET : 0.0f;
    // Magic numbers: seven rays across the body at scattered heights, 0.7 long.
    static const float RAYS[7][2] = { { 0.35f, -0.3f }, { 1.2f, -0.2f }, { 0.45f, -0.1f }, { 0.65f, 0.0f }, { 1.0f, 0.1f }, { 0.55f, 0.2f }, { 1.1f, 0.3f } };
    vector normals[7];
    int count = 0;
    for (int i = 0; i < 7; i++)
    {
        if (cover_cast_ray(pp, offset + RAYS[i][0], RAYS[i][1], &normals[count], 0.0f, nullptr, 0.7f))
        {
            count++;
        }
    }
    // Magic number: at least four of them.
    if (count < 4)
    {
        pp->cover_enter_ts.invalidate();
        return;
    }
    float angles[7];
    for (int i = 0; i < count; i++)
    {
        angles[i] = fl_atan2_safe(normals[i].x, normals[i].z);
    }
    // Rays that disagree do not count, and a run of more than three that agree earns one back. Magic numbers: 1.8 radians
    // end to end, 0.15 neighbour to neighbour.
    int bad = 0;
    const float spread = fl_angle_cap_radians(angles[count - 1] - angles[0], -3.1415927f);
    if (1.8f <= spread || 1.8f <= -spread)
    {
        bad = 1;
    }
    int run = 1;
    int best_run = 1;
    int best_end = 0;
    if (0 < count - 1)
    {
        for (int i = 0; i < count - 1; i++)
        {
            const float step = fl_angle_cap_radians(angles[i + 1] - angles[i], -3.1415927f);
            if (0.15f <= step || 0.15f <= -step)
            {
                bad++;
                run = 1;
            }
            else
            {
                run++;
                if (best_run < run)
                {
                    best_end = i + 1;
                    best_run = run;
                }
            }
        }
        if (3 < best_run)
        {
            bad--;
        }
    }
    int middle = best_end - ((best_run + 1) >> 1);
    if (middle < 0)
    {
        middle = 0;
    }
    if (3 < count - bad)
    {
        pp->cover_vector = normals[middle].copy_flatten_normalize();
        pp->cover_collision_normal = normals[middle].copy_flatten_normalize();
        pp->cover_vector_right = cover_cross_up(pp->cover_vector);
    }
    if (!(camera_orient->fvec.copy_flatten_normalize() * pp->cover_collision_normal < wall_dot && pp->steering_vector * pp->cover_collision_normal < Player_cover_entry_stick_wall_dot))
    {
        return;
    }
    pp->move_speed = 0.0f;
    pp->cur_state = HS_COVER;
    pp->cur_movement_state = HMS_IDLE;
    pp->movement_submode = HMSM_COVER_RIGHT;
    // Magic numbers: starts on the right unless the wall is only on the left, by a sweep 0.5 to the left.
    raycast_input in;
    cover_input_clear(in);
    raycast_output out;
    in.start = pp->pos - pp->cover_vector * SIDE_PROBE_BACK + pp->cover_vector_right * -0.5f;
    in.start.y += 0.55f;
    in.radius = SIDE_PROBE_RADIUS;
    in.end = in.start - pp->cover_vector * SIDE_PROBE_FORWARD + pp->cover_vector_right * -0.2f;
    in.m_filter_info = COLLIDE_RAY_PLAYER_COVER;
    in.m_flags = COVER_RAY_FLAGS;
    if (!havok_spherecast(in, out))
    {
        pp->movement_submode = HMSM_COVER_LEFT;
    }
    pp->entering_cover.set(300);
    pp->cover_test_queue = PCT_CROUCH;
    pp->combat.combat_aim_timestamp.invalidate();
    pp->combat.combat_ready_timestamp.invalidate();
    pp->combat.combat_blindfire_timestamp.set(0);
    pp->hflags.aim_at_pos = 0;
    pp->cover_delay_ts.set(0);
    pp->cover_lean_delay.set(0);
    pp->pflags.no_reticule = 1;
    pp->cover_approaching_edge = false;
    pp->cover_move_vel = 0.0f;
}

// While moving in cover the wall may turn: follows it if most rays agree with the new normal. Magic numbers throughout are
// rfg.exe's: a 0.05 sphere 0.4 up, 1.3 long, and four rays 0.6 long.
static void cover_follow_wall(player* pp, const matrix* camera_orient)
{
    raycast_input in;
    in.start = pp->pos;
    in.start.y += 0.4f;
    in.end = in.start - pp->cover_vector * 1.3f;
    in.m_filter_info = COLLIDE_RAY_DEFAULT;
    cover_input_clear(in);
    in.m_flags = 0;
    in.radius = 0.05f;
    raycast_output sphere_out;
    if (!havok_spherecast(in, sphere_out))
    {
        return;
    }
    const float length = in.radius + 1.3f;
    in.end = in.start - pp->cover_vector * length;
    raycast_output ray_out;
    if (!havok_raycast(in, ray_out))
    {
        return;
    }
    const vector wall_normal = ray_out.m_hits[0].hit_normal.copy_flatten_normalize();
    const vector camera_flat = camera_orient->fvec.copy_flatten_normalize();
    if (!(0.7f < pp->cover_vector * wall_normal && wall_normal * camera_flat < CAMERA_TEST_VALUE && (!pp->cover_approaching_edge || 2.5f < pp->move_speed)))
    {
        return;
    }
    static const float RAYS[4][2] = { { 0.2f, -0.3f }, { 0.8f, -0.1f }, { 0.4f, 0.1f }, { 0.6f, 0.3f } };
    vector normals[4];
    vector positions[4];
    int count = 0;
    for (int i = 0; i < 4; i++)
    {
        if (cover_cast_ray(pp, RAYS[i][0], RAYS[i][1], &normals[count], 0.0f, &positions[count], 0.6f))
        {
            count++;
        }
    }
    if (count == 0)
    {
        return;
    }
    int agreeing = 0;
    for (int i = 0; i < count; i++)
    {
        const float cover_dot = pp->cover_vector * normals[i];
        const float camera_dot = camera_flat * normals[i];
        if (((0.706f < cover_dot && cover_dot < 0.95f) || camera_dot < CAMERA_TEST_VALUE) && (camera_dot < CAMERA_TEST_VALUE || 0.95f < wall_normal * normals[i]))
        {
            agreeing++;
        }
    }
    vector along;
    along.zero();
    for (int i = 1; i < count; i++)
    {
        along += positions[i];
    }
    along -= positions[0] * (float)(count - 1);
    along.flatten_normalize_safe();
    const vector along_normal = cover_cross_up(along);
    if (1 < agreeing && along_normal * wall_normal < along_normal * pp->cover_vector)
    {
        pp->cover_vector = wall_normal.copy_flatten_normalize();
        pp->cover_vector_right = cover_cross_up(pp->cover_vector).copy_normalize();
        pp->cover_approaching_edge = false;
    }
}

// Whether a wall is too low to stand behind. Magic numbers: 0.05 spheres one metre at the wall, 1.6 up and 0.15 right, then
// 0.3 left and 0.1 lower, then 0.15 right and 0.15 lower.
static bool cover_wall_is_low(player* pp)
{
    raycast_input in;
    in.start = pp->pos + pp->cover_vector_right * 0.15f;
    in.start.y += 1.6f;
    in.end = in.start - pp->cover_vector;
    cover_input_clear(in);
    in.m_filter_info = COLLIDE_RAY_PLAYER_COVER;
    in.m_flags = COVER_RAY_FLAGS;
    in.radius = 0.05f;
    raycast_output out;
    if (!havok_spherecast(in, out) || (rfg_get_physical_material_by_index(out.m_hits[0].hit_physical_material)->flags & MATERIAL_FLAG_NOT_COVER) != 0)
    {
        return true;
    }
    in.start -= pp->cover_vector_right * 0.3f;
    in.start.y -= 0.1f;
    in.end = in.start - pp->cover_vector;
    out.m_hits.clear();
    if (!havok_spherecast(in, out) || (rfg_get_physical_material_by_index(out.m_hits[0].hit_physical_material)->flags & MATERIAL_FLAG_NOT_COVER) != 0)
    {
        return true;
    }
    in.start += pp->cover_vector_right * 0.15f;
    in.start.y -= 0.15f;
    in.end = in.start - pp->cover_vector;
    out.m_hits.clear();
    if (!havok_spherecast(in, out) || (rfg_get_physical_material_by_index(out.m_hits[0].hit_physical_material)->flags & MATERIAL_FLAG_NOT_COVER) != 0)
    {
        return true;
    }
    return false;
}

// 0x00A9EC30 (:118)
void player_cover_do_frame(player* pp)
{
    if (Multiplayer || wrecking_crew_is_active() || Player_disable_cover)
    {
        return;
    }
    pp->hflags.dive_capsule = pp->cover_dive_capsule.elapsed() ? 0 : 1;
    // Magic number: gravity comes back for the dive's last second.
    if (!pp->cover_dive_capsule.elapsed() && pp->cover_dive_capsule.time_until() < 1000 && pp->rdd.is_loaded())
    {
        pp->rdd.get_data()->controller->flags.disable_gravity = 0;
    }
    if (!pp->exiting_cover.elapsed() || pp->pflags.do_sprint != 0)
    {
        return;
    }
    if (controls_is_button_down(CBA_OFC_JUMP, Player_controller_index))
    {
        return;
    }
    // Magic number: as in rfg.exe, past 350 ms out of cover the rest does not run.
    if (0x15E < pp->sprint_start_timestamp.time_since() && pp->cur_state != HS_COVER)
    {
        pp->pflags.use_cover = 0;
        return;
    }
    if (pp->backpack != nullptr && pp->backpack->is_activated() && pp->backpack->get_backpack_info()->no_cover)
    {
        return;
    }
    {
        const weapon* wp = pp->get_equipped_weapon();
        if (wp != nullptr && wp->w_info->flags.disable_player_cover)
        {
            return;
        }
    }
    // The multiplayer tests here and below can never hold, as the first line returned (original).
    const bool use_button = Multiplayer ? Mp_player_cover_use_button : Player_cover_use_button;
    const bool local = object_is_local_player(pp);
    // Magic numbers: how directly the camera must face the wall, by default and for a player who is not local.
    float wall_dot = -0.4f;
    if (!local)
    {
        wall_dot = -0.1f;
    }
    else if (!use_button)
    {
        pp->pflags.use_cover = 1;
        wall_dot = Player_cover_entry_camera_wall_dot;
    }
    const matrix* camera_orient = (!Multiplayer || object_is_local_player(pp)) ? &Rfg_camera.real_orient : &pp->mp_camera_orient;

    bool entering = false;
    if ((pp->hflags.check_for_cover != 0 || (local && use_button)) && pp->pflags.use_cover != 0 &&
        pp->cur_state == HS_STANDARD && pp->get_equipped_weapon() != nullptr && !player_has_non_inventory(pp))
    {
        if (pp->char_inst->is_playing_action(ALIAS_EQUIP) || pp->char_inst->is_playing_action(ALIAS_COMBAT_MOVE) || player_is_zoomed(pp))
        {
            return;
        }
        const weapon* wp = pp->get_equipped_weapon();
        if (wp != nullptr && wp->flags.trigger_down)
        {
            return;
        }
        pp->pflags.use_cover = 0;
        if (pp->hflags.check_for_cover == 0)
        {
            // The button was pressed: looks for a wall where the camera faces. Magic numbers: 0.5 up, from 0.15 back for 1.35.
            pp->cover_collision_normal = -camera_orient->fvec.copy_flatten_normalize();
            raycast_input in;
            in.radius = 0.0f;
            in.start = pp->pos - pp->cover_collision_normal * 0.15f;
            in.start.y += 0.5f;
            in.end = in.start - pp->cover_collision_normal * 1.35f;
            cover_input_clear(in);
            in.m_filter_info = COLLIDE_RAY_PLAYER_COVER;
            in.m_flags = COVER_RAY_FLAGS;
            raycast_output out;
            if (!havok_raycast(in, out))
            {
                return;
            }
            pp->cover_collision_normal = out.m_hits[0].hit_normal.copy_flatten_normalize();
            pp->steering_vector = -pp->cover_collision_normal;
        }
        const float camera_dot = camera_orient->fvec.copy_flatten_normalize() * pp->cover_collision_normal;
        // Magic number: the slower walk that still enters cover crouching.
        const float min_speed = pp->cur_stance == HST_CROUCH ? 2.2f : PLAYER_COVER_MIN_MOVE_SPEED;
        if (camera_dot < wall_dot &&
            ((min_speed < pp->move_speed && !pp->direct_control_dir.is_null()) || (local && use_button)) &&
            (pp->steering_vector * pp->cover_collision_normal < Player_cover_entry_stick_wall_dot || (local && use_button)))
        {
            entering = true;
            if (!pp->cover_enter_ts.valid())
            {
                // Magic number: pushing against a wall enters cover after 75 ms.
                pp->cover_enter_ts.set(75);
            }
            if ((local && use_button) || pp->cover_enter_ts.elapsed())
            {
                cover_try_enter(pp, camera_orient, wall_dot);
            }
        }
    }
    pp->pflags.use_cover = 0;
    pp->hflags.check_for_cover = 0;
    if (!entering)
    {
        pp->cover_enter_ts.invalidate();
    }
    if (pp->cur_state != HS_COVER)
    {
        if (local && pp->pflags.no_reticule != 0)
        {
            pp->pflags.no_reticule = 0;
        }
        return;
    }
    if (!pp->entering_cover.elapsed() && pp->entering_cover.valid() && pp->cover_test_queue == PCT_DONE)
    {
        pp->cover_test_queue = PCT_THREE_FRAME_DELAY;
    }
    if (0.0f < pp->move_speed)
    {
        cover_follow_wall(pp, camera_orient);
    }
    if (pp->cover_test_queue == PCT_VERIFY_COVER)
    {
        if (pp->pflags.cover_against_vehicle == 0)
        {
            // Magic numbers: one random ray first, in this draw order, then four fixed ones of which two must hit.
            const float right = fl_rand_range(-0.2f, 0.2f);
            const float up = fl_rand_range(0.2f, 0.6f);
            if (!cover_cast_ray(pp, up, right, nullptr, 0.0f, nullptr, 0.6f))
            {
                int hits = cover_cast_ray(pp, 0.2f, -0.3f, nullptr, 0.0f, nullptr, 0.8f) ? 1 : 0;
                if (cover_cast_ray(pp, 0.4f, 0.1f, nullptr, 0.0f, nullptr, 0.8f))
                {
                    hits++;
                }
                if (hits < 2)
                {
                    if (cover_cast_ray(pp, 0.8f, -0.1f, nullptr, 0.15f, nullptr, 0.8f))
                    {
                        hits++;
                    }
                    if (cover_cast_ray(pp, 0.6f, 0.3f, nullptr, 0.15f, nullptr, 0.8f))
                    {
                        hits++;
                    }
                    if (hits < 2)
                    {
                        player_exit_cover(pp, false);
                        return;
                    }
                }
            }
        }
        else if (!cover_cast_ray(pp, 0.8f, 0.0f, nullptr, 0.3f, nullptr, 0.6f))
        {
            // No return here, so the tests below still run (original).
            player_exit_cover(pp, false);
        }
    }
    if (pp->cover_test_queue == PCT_CROUCH)
    {
        pp->hflags.crouch_cover = 0;
        if (cover_wall_is_low(pp))
        {
            pp->hflags.crouch_cover = 1;
            if (pp->cur_stance != HST_CROUCH)
            {
                pp->cur_stance = HST_CROUCH;
                const human_move_submodes submode = pp->movement_submode;
                pp->cover_approaching_edge = false;
                if (submode == HMSM_COVER_LEFT_EDGE)
                {
                    pp->movement_submode = HMSM_COVER_LEFT;
                }
                else if (submode == HMSM_COVER_RIGHT_EDGE)
                {
                    pp->movement_submode = HMSM_COVER_RIGHT;
                }
                pp->pflags.no_reticule = 0;
            }
        }
    }
    if (pp->cover_test_queue != PCT_DONE)
    {
        pp->cover_test_queue = (player_cover_test)(pp->cover_test_queue + 1);
    }
    if (pp->cur_state != HS_COVER || pp->get_equipped_weapon() == nullptr)
    {
        return;
    }
    const human_move_submodes submode = pp->movement_submode;
    if (submode != HMSM_COVER_RIGHT_BLINDFIRE && submode != HMSM_COVER_LEFT_BLINDFIRE)
    {
        return;
    }
    // Blind fire aims along the look, limited around the wall. Magic numbers: 200 degrees to the side, 20 up and 60 down,
    // and a far point 10 km out.
    vector look_dir = pp->look_at_pos - pp->pos;
    look_dir.normalize();
    const float look_heading = fl_atan2_safe(look_dir.x, look_dir.z);
    float pitch = -asinf(look_dir.y);
    const float wall_heading = fl_atan2_safe(pp->cover_vector.x, pp->cover_vector.z);
    float heading;
    if (submode == HMSM_COVER_RIGHT_BLINDFIRE)
    {
        heading = fl_angle_cap_radians(look_heading - wall_heading, -6.2831855f);
        if (heading < -3.4906f)
        {
            heading = -3.4906f;
        }
    }
    else
    {
        heading = fl_angle_cap_radians(look_heading - wall_heading, 0.0f);
        if (3.4906f < heading)
        {
            heading = 3.4906f;
        }
    }
    if (0.34906f < pitch)
    {
        pitch = 0.34906f;
    }
    else if (pitch < -1.0471799f)
    {
        pitch = -1.0471799f;
    }
    const matrix aim(pitch, 0.0f, heading + wall_heading);
    const vector target = pp->pos + aim.fvec * 10000.0f;
    human_set_look_at_target(pp, target, true, -1.0f);
}
