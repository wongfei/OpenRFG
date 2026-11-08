// Original: volition/rfg/code/objects/humans/human_move.cpp
//
// Partial (player_minimal.md P4): setting a path, the player's movement state from the stick
// (player_move_set_anim_direct), direct travel, landing, the per-frame movement mode dispatch and
// whether the human crouches (P6). Path following and crouching stay under #if 0 at the end of the file.
// The external interface is declared in human_move.h.

#include "volition/rfg/code/objects/humans/human_move.h"

#include "volition/rfg/code/camera/camera_shake.h"
#include "volition/rfg/code/camera/camera_view.h"
#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/rfg/code/interface/ui_document_hud_rail_driver.h"
#include "volition/rfg/code/interface/ui_document_hud_sniper.h"
#include "volition/rfg/code/objects/humans/human_combat_turret.h"
#include "volition/rfg/code/objects/turret.h"
#include "volition/vlib/code/math/vector2.h"
#include "volition/rfg/code/destruction/rfg_materials.h"
#include "volition/rfg/code/effects/effects.h"
#include "volition/rfg/code/effects/object_effect.h"
#include "volition/rfg/code/gamewide/gameaudio.h"
#include "volition/rfg/code/geom/character.h"
#include "volition/rfg/code/geom/character_instance.h"
#include "volition/rfg/code/geom/vanim.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_info.h"
#include "volition/rfg/code/objects/humans/human_ragdoll.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/pathfinding/bb_pathfinding.h"
#include "volition/rfg/code/pathfinding/nav_cell_detour.h"
#include "volition/rfg/code/pathfinding/nav_cell.h"
#include "volition/vlib/code/math/vector3.h"

#include "keen/base/trace.hpp"

// ---- file-static globals
static const float COVER_NODE_Y_TOLERANCE = 1.2f; // 0x0131185C
// The stick deflection (squared against its square) above which the player runs instead of walking.
static float Player_run_threshold = 0.65f; // 0x016943D4
// player* Player (0x03023874) is used here too; placed in player.cpp.

// ---- internal functions
static void init_path_following_data(human* hp); // 0x00A0AEB0 (:594)
static void human_move_path_set(human* hp, const vector& pos, const vector& dest, uint64_t end_nav_cell_hint, bool out_of_chunk); // 0x00A30BA0 (:635)

// Puts a human into a movement mode and submode unless it is an npc in cover (alive), on a ladder (npcs; only for
// HMM_NONE), or ragdolled; modes other than pathfinding, jumping and falling drop the path. HMM_NONE also stops the
// steering and HMM_PATHFINDING starts the destination checks. human_set_movement_mode inlined (human.cpp).
static void human_move_set_mode(human* hp, human_move_modes mode, human_move_submodes submode, bool ladder_check)
{
    if ((hp->get_sub_type() == OT_SUB_HUMAN_NPC && !hp->is_killed() && hp->cur_state == HS_COVER) ||
        (ladder_check && hp->hflags.is_on_ladder && hp->get_sub_type() == OT_SUB_HUMAN_NPC) || hp->ragdoll_state == HUMAN_RAGDOLL_STATE_ACTIVE ||
        hp->ragdoll_state == HUMAN_RAGDOLL_STATE_BLEND_OUT)
    {
        return;
    }
    if (hp->movement_mode != mode)
    {
        if (mode == HMM_NONE)
        {
            hp->steering_vector.zero();
        }
        else if (mode == HMM_PATHFINDING)
        {
            hp->pf.dest_check_timer.set(0);
            hp->pf.last_dest_check_pos = hp->pos;
        }
        hp->previous_movement_mode = hp->movement_mode;
        hp->movement_mode = mode;
    }
    hp->movement_submode = submode;
    const human_move_modes now = hp->movement_mode;
    if (now != HMM_PATHFINDING && now != HMM_PATHFINDING_TO_VEHICLE && now != HMM_JUMP && now != HMM_FALL)
    {
        hp->pf.dest_check_timer.invalidate();
        if (hp->pf.path != nullptr)
        {
            bb_pf_free_path(&hp->pf.path);
        }
    }
}

// 0x00A0AEB0 (:594). LTCG: hp in EDI.
// A path of one node within 0.3 m (horizontally) is dropped; otherwise following starts, and the first node's nav cell
// (unless detoured, flag 8) becomes the last visited one when not on a mover.
static void init_path_following_data(human* hp)
{
    path_node* first = hp->pf.path;
    path_node* last = first == nullptr ? nullptr : first->prev;
    if (first == last)
    {
        const float dx = hp->pos.x - last->pos.x;
        const float dz = hp->pos.z - last->pos.z;
        // Magic number: 0.09, 0.3 m squared.
        if ((double)dx * dx + 0.0 + (double)dz * dz < 0.09)
        {
            bb_pf_free_path(&hp->pf.path);
            return;
        }
    }
    hp->pf.action = HPA_STARTUP;
    hp->pf.path_repulsors_collection_time.invalidate();
    if (!hp->hflags.on_mover)
    {
        nav_cell* nc = nav_cell_from_handle(first->nav_cell_handle);
        // Magic number: nav cell flag 8, detour cells.
        if (nc != nullptr && (nc->flags & 8) == 0)
        {
            hp->pf.last_visited_nav_cell_handle = first->nav_cell_handle;
        }
    }
}

// 0x00A0AFA0 (:736)
// A new path replaces the old one; it is smoothed unless smoothing is disabled for this request (cleared) or always.
void human_move_path_set_direct(human* hp, path_node* path)
{
    bb_pf_free_path(&hp->pf.path);
    hp->pf.path = path;
    if (path == nullptr)
    {
        return;
    }
    init_path_following_data(hp);
    hp->pf.flags.smoothing = 0;
    if (!hp->hflags.disable_path_smoothing_for_request && !hp->hflags.disable_all_path_smoothing)
    {
        if (rfg_pf_calculate_smoothing_info(hp))
        {
            hp->pf.flags.smoothing = 1;
        }
    }
    else
    {
        hp->hflags.disable_path_smoothing_for_request = 0;
    }
}

// 0x00A30BA0 (:635). LTCG: hp in ESI, dest in EDI, pos, end_nav_cell_hint and out_of_chunk on the stack.
// Pathfinds from pos to dest (repeated problem detours near the same destination, within 5 m, avoid their cells)
// and remembers the goal.
static void human_move_path_set(human* hp, const vector& pos, const vector& dest, uint64_t end_nav_cell_hint, bool out_of_chunk)
{
    nav_cell_detour_remove_convex_hull_for_request(hp->nav_cell_detour_request_handle);
    hp->pf.last_cmp_hull = 0xFFFFFFFF;
    if (hp->pf.num_problem_detours > 0)
    {
        // Magic number: 25, 5 m squared.
        if (vec_dist_squared(hp->pf.problem_detours_dest_pos, dest) <= 25.0f)
        {
            nav_cell_detour_add_impassable_nav_cells(hp, hp->pf.last_visited_nav_cell_handle, end_nav_cell_hint);
        }
        else
        {
            hp->pf.num_problem_detours = 0;
        }
    }
    path_node* path = rfg_pf_pathfind(hp, pos, dest, hp->pf.last_visited_nav_cell_handle, end_nav_cell_hint, out_of_chunk);
    rfg_pf_clear_impassable_nav_cells();
    human_move_path_set_direct(hp, path);
    hp->pf.eventual_goal_pos = dest;
}

// 0x00A9D380 (:207)
// The player's movement state from the stick (jh sideways, jv forward): in the air only the sprint delay is kept;
// sprinting stands up from a crouch and zooms for the sprint; otherwise a sprint zoom ends. A stop (or a blocking
// animation that moves the player, unless it is landing) idles; else the state is sprint, run (deflection above
// Player_run_threshold, or right after a sprint) or walk.
void player_move_set_anim_direct(player* pp, float jh, float jv, bool do_sprint, bool do_stop)
{
    bool stop = do_stop;
    if (pp->block_movement_anim != -1 && (pp->block_movement_type == BLOCK_MOVEMENT_POS || pp->block_movement_type == BLOCK_MOVEMENT_POS_AND_ORIENT))
    {
        jh = 0.0f;
        jv = 0.0f;
        stop = pp->char_inst->is_playing_action(ANIM_ALIAS_LAND) ? do_stop : true;
    }
    float deflection = jv * jv + jh * jh;
    vector2 dir(jh, jv);
    if (!dir.is_null())
    {
        dir.normalize_safe();
        pp->direct_control_dir = dir;
    }
    human_states state = pp->cur_state;
    if (state == HS_JUMP || state == HS_FALL)
    {
        if (do_sprint)
        {
            // Magic number: a sprint request keeps the sprint for 0.3 s.
            pp->sprint_delay_timestamp.set(300);
            return;
        }
        if (pp->sprint_delay_timestamp.elapsed() && pp->cur_movement_state == HMS_SPRINT)
        {
            pp->cur_movement_state = HMS_RUN;
        }
        return;
    }
    bool set_state = false;
    if (do_sprint && !stop)
    {
        set_state = true;
        if (pp->cur_movement_state != HMS_SPRINT)
        {
            bool zoom = true;
            if (pp->cur_stance == HST_CROUCH)
            {
                human_move_stop_crouching(pp);
                if (pp->cur_stance == HST_CROUCH)
                {
                    do_sprint = false;
                    zoom = false;
                }
                else
                {
                    pp->pflags.crouching_state = 0;
                }
            }
            if (zoom)
            {
                player_set_zoom_state(pp, PZS_SPRINT, false);
            }
        }
    }
    else
    {
        const turret* tp = nullptr;
        if (pp->cur_movement_state == HMS_SPRINT && pp->zoom_state != PZS_OFF && ((tp = human_combat_get_manned_turret(pp)) == nullptr || !tp->t_info->flags.force_fine_aim))
        {
            const player_zoom_state previous = pp->zoom_state;
            pp->previous_zoom_state = previous;
            pp->zoom_state = PZS_OFF;
            bool view = true;
            if (previous != PZS_OFF)
            {
                if (pp != Player)
                {
                    view = false;
                }
                else
                {
                    pp->get_equipped_weapon(); // Original: the result is not used.
                    if (pp->previous_zoom_state == PZS_SCOPE)
                    {
                        pp->set_opacity_plus_child_items(1.0f);
                        pp->set_casts_transparent_shadows_plus_child_items(false);
                        hud_ui_rail_driver_set_visible(false);
                        hud_ui_sniper_set_visible(false);
                    }
                }
            }
            if (view && pp == Player)
            {
                camera_set_view_player(PZS_OFF, false);
            }
        }
        if (stop)
        {
            if (state == HS_CURRENT)
            {
                state = pp->cur_state;
            }
            pp->cur_state = state;
            pp->cur_movement_state = HMS_IDLE;
            return;
        }
        set_state = true;
    }
    if (set_state)
    {
        human_movement_states movement_state = HMS_SPRINT;
        if (!pp->sprint_delay_timestamp.elapsed())
        {
            deflection = 1.0f;
        }
        if (!do_sprint)
        {
            movement_state = deflection <= Player_run_threshold * Player_run_threshold ? HMS_WALK : HMS_RUN;
        }
        else
        {
            pp->sprint_delay_timestamp.set(300);
            pp->combat.combat_aim_timestamp.invalidate();
            pp->combat.combat_ready_timestamp.invalidate();
        }
        if (pp->sprint_delay_timestamp.elapsed())
        {
            pp->sprint_delay_timestamp.set(0);
        }
        human_set_state(pp, state, movement_state, pp->move_speed);
    }
}

// 0x00ADC910 (:358)
// Landing after a jump or fall: the camera shakes with the fall speed (relative to the fatal speed); a free fall
// ragdolls; otherwise the landing animation follows the fall speed, the movement and the in-air melee attack (an
// unfinished one keeps playing its landing part), landing damage is taken (death ragdolls), the movement blocks for
// the landing animation, and the land sound and ground effect play.
void human_move_play_land(human* hp)
{
    if (hp != nullptr && hp->get_sub_type() == OT_SUB_HUMAN_PLAYER && vanim_is_playing_action(hp->char_inst->vp, ANIM_ALIAS_DIVE))
    {
        return;
    }
    if (hp->ragdoll_idx > -1 && hp->ragdoll_state != HUMAN_RAGDOLL_STATE_ACTIVE_ON_IMPACT)
    {
        return;
    }
    const float fatal_speed = human_move_fall_get_fatal_speed(hp, true); // inlined
    const float impact = hp->falling_vel_mag / fatal_speed;
    if (hp->get_handle() == Rfg_camera.target_handle)
    {
        float intensity = impact;
        if (intensity - 0.0f < 0.0f)
        {
            intensity = 0.0f;
        }
        if (0.0f <= intensity - 1.0f)
        {
            intensity = 1.0f;
        }
        camera_shake_play("jump_land", intensity, false);
    }
    vector up;
    human_get_head_pos(hp, up);
    up.x = up.x - hp->pos.x;
    up.y = up.y - hp->pos.y;
    up.z = up.z - hp->pos.z;
    up.normalize();
    if (hp->movement_submode == HMSM_FALL_FREE)
    {
        human_ragdoll_start(hp, true, 0xFFFFFFFF);
        // Magic numbers: the knockdown lasts 0.4 s plus 4 s per fatal speed of impact.
        hp->knockdown_timestamp.set(400 - (int)((double)impact * -4000.0));
        hp->steering_vector.zero();
        return;
    }
    // Magic number: a body not upright (0.999) straightens up over 0.4 s.
    if (up.y < 0.999f)
    {
        vanim_interpolate_uvec(hp->char_inst->vp, hp, UP_VECTOR, 0.4f);
        hp->steering_vector = hp->orient.fvec.copy_flatten_normalize();
    }
    animation_action anim_index = ANIMATION_ACTION_NONE;
    bool block = true;
    if ((unsigned int)(hp->cur_state - HS_JUMP) < 2)
    {
        if (hp->hflags.jumping_from_building)
        {
            const bool jumping_down = hp->char_inst->is_playing_action((vanim_aliases)ANIMATION_ACTION_JUMP_DOWN);
            hp->char_inst->stop_action(ANIM_ALIAS_SPECIAL_JUMP, 0, false);
            hp->char_inst->play_action(jumping_down ? ANIMATION_ACTION_LAND_NORMAL : ANIMATION_ACTION_LAND_HARD, ANIM_ALIAS_LAND, 1.0f, 0);
            human_move_set_mode(hp, HMM_FALL, HMSM_LAND, false);
            hp->hflags.jumping_from_building = 0;
            human_move_fall_do_landing_damage(hp);
            block = false;
        }
        else
        {
            hp->char_inst->stop_action(ANIM_ALIAS_JUMP, 4, false);
            anim_index = ANIMATION_ACTION_LAND_LIGHT;
            // Magic numbers: falls up to 12 m/s land normally (from 2 m/s with an animation), faster ones land hard.
            if (hp->falling_vel_mag <= 12.0f)
            {
                bool moving_land = true;
                if (hp->hflags.in_air_melee)
                {
                    if (!vanim_is_playing_action(hp->char_inst->vp, ANIM_ALIAS_COMBAT_MOVE) || 6.0f <= hp->falling_vel_mag)
                    {
                        vanim_ramp_out_action(hp->char_inst->vp, ANIM_ALIAS_COMBAT_MOVE, 3);
                    }
                    else
                    {
                        const int frame = vanim_get_anim_frame(hp->char_inst->vp, ANIM_ALIAS_COMBAT_MOVE);
                        if ((int)vanim_get_trigger_time(hp->char_inst->vp, Trigger_impact_end, ANIM_ALIAS_COMBAT_MOVE, 0) < frame)
                        {
                            vanim_pause_and_ramp_out_action(hp->char_inst->vp, ANIM_ALIAS_COMBAT_MOVE, 3);
                        }
                        else
                        {
                            // Magic numbers: 0x216 and 0x217, the in-air attacks swapped for their landings.
                            if (vanim_is_playing_action(hp->char_inst->vp, (vanim_aliases)0x216))
                            {
                                vanim_swap_animation(hp->char_inst->vp, ANIM_ALIAS_COMBAT_MOVE, index_alias(0x216, ANIM_ALIAS_NONE));
                            }
                            else if (vanim_is_playing_action(hp->char_inst->vp, (vanim_aliases)0x217))
                            {
                                vanim_swap_animation(hp->char_inst->vp, ANIM_ALIAS_COMBAT_MOVE, index_alias(0x217, ANIM_ALIAS_NONE));
                            }
                            hp->block_steering_vector = hp->orient.fvec;
                            hp->block_steering_heading_offset = 0.0f;
                            hp->block_movement_type = BLOCK_MOVEMENT_POS_AND_ORIENT;
                            hp->block_movement_anim = ANIM_ALIAS_COMBAT_MOVE;
                            moving_land = false;
                        }
                    }
                }
                if (2.0f <= hp->falling_vel_mag && moving_land)
                {
                    bool forward = false;
                    // Magic number: moving faster than 1.2 m/s.
                    bool moving = 1.2f < hp->move_speed;
                    if (hp->movement_submode == HMSM_FALL_FREE)
                    {
                        forward = moving;
                        // Magic number: within 30 degrees of facing.
                        if (moving && hp->orient.fvec * hp->actual_velocity < 0.8660254f)
                        {
                            forward = false;
                            moving = false;
                        }
                        hp->steering_vector = ZERO_VECTOR;
                    }
                    else if (object_is_player(hp))
                    {
                        // the player lands forward when steering forward (within ~37 degrees) or not at all, moving when
                        // steering harder than 0.7
                        const vector& steer = ((player*)hp)->direct_control_vel;
                        if (0.8f < hp->orient.fvec * steer.copy_normalize() || steer.is_null())
                        {
                            forward = true;
                        }
                        moving = false;
                        if (0.5f < steer.mag_squared())
                        {
                            moving = true;
                        }
                    }
                    if (!hp->hflags.in_air_melee)
                    {
                        if (hp->cur_movement_state == HMS_RUN || hp->cur_movement_state == HMS_SPRINT)
                        {
                            anim_index = forward && moving ? ANIMATION_ACTION_LAND_RUN : ANIMATION_ACTION_LAND_RUN_STOP;
                        }
                        else
                        {
                            anim_index = forward && moving ? ANIMATION_ACTION_LAND_NORMAL : ANIMATION_ACTION_LAND_NORMAL_STOP;
                        }
                    }
                    else
                    {
                        anim_index = forward && moving ? ANIMATION_ACTION_LAND_MELEE_RUN : ANIMATION_ACTION_LAND_MELEE_STOP;
                    }
                }
            }
            else
            {
                anim_index = hp->hflags.in_air_melee ? ANIMATION_ACTION_LAND_MELEE_HARD : ANIMATION_ACTION_LAND_HARD;
                hp->char_inst->stop_action(ANIM_ALIAS_FACIAL_MORPH, 3, false);
                hp->char_inst->stop_action(ANIM_ALIAS_COMBAT_MOVE, 3, false);
                hp->char_inst->play_action(ANIMATION_ACTION_FACIAL_FLINCH, ANIM_ALIAS_FACIAL_MORPH, 1.0f, 0);
            }
            human_move_set_mode(hp, HMM_FALL, HMSM_LAND, false);
            if (anim_index == ANIMATION_ACTION_LAND_LIGHT)
            {
                hp->char_inst->play_action(ANIMATION_ACTION_LAND_LIGHT, ANIM_ALIAS_NONE, 1.0f, 0);
            }
            else
            {
                // Magic number: action flag 0x20 for the hard landings.
                const unsigned int flags = anim_index == ANIMATION_ACTION_LAND_HARD || anim_index == ANIMATION_ACTION_LAND_MELEE_HARD ? 0x20 : 0;
                hp->char_inst->play_action(anim_index, ANIM_ALIAS_LAND, 1.0f, flags);
                vanim_set_transition_ending_percent(hp->char_inst->vp, 0.5f);
                if (!hp->animation_flags.back && !hp->animation_flags.backpedal)
                {
                    if (hp->animation_flags.left)
                    {
                        vanim_set_variant(hp->char_inst->vp, ANIM_ALIAS_LAND, ANIM_VARIANT_LEFT, true, true);
                    }
                }
                else
                {
                    vanim_set_variant(hp->char_inst->vp, ANIM_ALIAS_LAND, ANIM_VARIANT_BACK, true, true);
                }
            }
            hp->hflags.jump_take_off = 0;
            human_move_fall_do_landing_damage(hp);
            if (hp->hit_points <= 0.0f)
            {
                human_ragdoll_start(hp, true, 0xFFFFFFFF);
                block = false;
            }
            else if (anim_index == ANIMATION_ACTION_NONE || anim_index == ANIMATION_ACTION_LAND_LIGHT)
            {
                block = false;
            }
        }
    }
    else
    {
        // landing from anything else (walking off a ledge)
        if (hp->hflags.in_air_melee && vanim_is_playing_action(hp->char_inst->vp, ANIM_ALIAS_COMBAT_MOVE))
        {
            const int frame = vanim_get_anim_frame(hp->char_inst->vp, ANIM_ALIAS_COMBAT_MOVE);
            if ((int)vanim_get_trigger_time(hp->char_inst->vp, Trigger_impact_end, ANIM_ALIAS_COMBAT_MOVE, 0) < frame)
            {
                vanim_pause_and_ramp_out_action(hp->char_inst->vp, ANIM_ALIAS_COMBAT_MOVE, 3);
            }
            else
            {
                if (vanim_is_playing_action(hp->char_inst->vp, (vanim_aliases)0x216))
                {
                    vanim_swap_animation(hp->char_inst->vp, ANIM_ALIAS_COMBAT_MOVE, index_alias(0x216, ANIM_ALIAS_NONE));
                }
                else if (vanim_is_playing_action(hp->char_inst->vp, (vanim_aliases)0x217))
                {
                    vanim_swap_animation(hp->char_inst->vp, ANIM_ALIAS_COMBAT_MOVE, index_alias(0x217, ANIM_ALIAS_NONE));
                }
                hp->block_steering_vector = hp->orient.fvec;
                hp->block_steering_heading_offset = 0.0f;
                hp->block_movement_type = BLOCK_MOVEMENT_POS_AND_ORIENT;
                hp->block_movement_anim = ANIM_ALIAS_COMBAT_MOVE;
            }
        }
        hp->char_inst->stop_action(ANIM_ALIAS_JUMP, 4, false);
        // Magic number: action flag 0x4000.
        hp->char_inst->play_action(ANIMATION_ACTION_LAND_NORMAL, ANIM_ALIAS_LAND, 1.0f, 0x4000);
        human_move_set_mode(hp, HMM_FALL, HMSM_LAND, false);
        human_move_fall_do_landing_damage(hp);
        vanim_set_transition_ending_percent(hp->char_inst->vp, 0.5f);
        if (hp->hit_points <= 0.0f)
        {
            human_ragdoll_start(hp, true, 0xFFFFFFFF);
            block = false;
        }
    }
    if (block)
    {
        hp->block_movement_anim = ANIM_ALIAS_LAND;
        hp->block_steering_vector = hp->orient.fvec.copy_flatten_normalize();
        hp->block_movement_type = BLOCK_MOVEMENT_POS_AND_ORIENT;
    }
    hp->hflags.is_falling = 0;
    const int land_foley = hp->h_info->foot_effects->land_foley;
    if (land_foley != -1)
    {
        gameaudio_play_cross_linked_sound(land_foley, hp->get_handle(), nullptr, nullptr);
    }
    unsigned char mat_index;
    material_shape_type shape_type;
    if (human_get_standing_on_material(hp, mat_index, shape_type) && mat_index != 0xFF)
    {
        const unsigned int effect = hp->h_info->foot_effects->get_jump_effect_handle(mat_index, shape_type, JUMP_LAND);
        effects_play(effect, hp->pos, hp->orient, false, 0xFFFFFFFF, -1, hp->get_handle(), 1.0f, false, EFFECTS_CATEGORY_NONE);
    }
}

// 0x00A0B220. A one-node path straight to dest, whatever is in the way. The cover check of the inlined
// human_set_movement_mode stays dead here, cur_state having just been set: kept.
void human_move_force_direct_walk(human* hp, vector& dest, human_movement_states mstate, nav_cell* dest_nc)
{
    hp->pf.eventual_goal_pos = dest;
    bb_pf_free_path(&hp->pf.path);
    path_node* node = bb_pf_get_free_path_node(dest, dest_nc != nullptr ? nav_cell_get_handle(dest_nc) : 0xFFFFFFFFFFFFFFFFull, 0xFFFFFFFF);
    if (node == nullptr)
    {
        return;
    }
    node->next = node;
    node->prev = node;
    hp->pf.path = node;
    init_path_following_data(hp);
    hp->pf.flags.smoothing = 0;
    if (mstate == HMS_CURRENT)
    {
        mstate = hp->cur_movement_state;
    }
    hp->move_speed = -1.0f;
    hp->cur_state = HS_STANDARD;
    hp->cur_movement_state = mstate;
    if (hp->get_sub_type() == OT_SUB_HUMAN_NPC && !hp->is_killed() && hp->cur_state == HS_COVER)
    {
        return;
    }
    if (hp->ragdoll_state == HUMAN_RAGDOLL_STATE_ACTIVE || hp->ragdoll_state == HUMAN_RAGDOLL_STATE_BLEND_OUT)
    {
        return;
    }
    if (hp->movement_mode != HMM_PATHFINDING)
    {
        hp->pf.dest_check_timer.set(0);
        hp->pf.last_dest_check_pos = hp->pos;
        hp->previous_movement_mode = hp->movement_mode;
        hp->movement_mode = HMM_PATHFINDING;
    }
    hp->movement_submode = HMSM_NONE;
}

// 0x00A30D30 (:863)
// Moves straight toward dest (the turn-to target): a human without a movement mode goes direct (not an NPC alive in
// cover, nor a ragdoll), dropping its path.
void human_move_direct_travel_to(human* hp, vector& dest)
{
    hp->turnto_target = dest;
    if (hp->movement_mode != HMM_NONE)
    {
        return;
    }
    if (hp->get_sub_type() == OT_SUB_HUMAN_NPC && !hp->is_killed() && hp->cur_state == HS_COVER)
    {
        return;
    }
    if (hp->ragdoll_state == HUMAN_RAGDOLL_STATE_ACTIVE || hp->ragdoll_state == HUMAN_RAGDOLL_STATE_BLEND_OUT)
    {
        return;
    }
    if (hp->movement_mode != HMM_DIRECT)
    {
        hp->previous_movement_mode = hp->movement_mode;
        hp->movement_mode = HMM_DIRECT;
    }
    const human_move_modes mode = hp->movement_mode;
    hp->movement_submode = HMSM_NONE;
    if (mode != HMM_PATHFINDING && mode != HMM_PATHFINDING_TO_VEHICLE && mode != HMM_JUMP && mode != HMM_FALL)
    {
        hp->pf.dest_check_timer.invalidate();
        if (hp->pf.path != nullptr)
        {
            bb_pf_free_path(&hp->pf.path);
        }
    }
}

// 0x00AE4C60 (:969)
// Pathfinding (a failed path stops the human, or makes a building jumper fall, and the player returns to direct
// control), jumping, falling and knockdowns.
void human_move_process(human* hp)
{
    switch (hp->movement_mode)
    {
    case HMM_NONE:
    case HMM_DIRECT:
    case HMM_PATHFINDING_TO_VEHICLE:
    case HMM_FP_MOVE:
    case HMM_SATELLITE:
        return;
    case HMM_PATHFINDING:
        if (!human_move_pathfind(hp) && hp->vehicle_handle == 0xFFFFFFFF)
        {
            if (!hp->hflags.jumping_from_building)
            {
                human_move_set_mode(hp, HMM_NONE, HMSM_NONE, true);
            }
            else
            {
                human_set_movement_mode(hp, HMM_FALL, HMSM_FALL_START);
            }
            hp->move_speed = -1.0f;
            hp->cur_state = HS_STANDARD;
            hp->cur_movement_state = HMS_IDLE;
            player* pp = World.object_cast<player>(hp);
            if (pp != nullptr)
            {
                human_move_set_mode(pp, HMM_DIRECT, HMSM_NONE, false);
            }
        }
        return;
    case HMM_JUMP:
        human_move_jump(hp);
        return;
    case HMM_FALL:
        human_move_fall(hp);
        return;
    case HMM_KNOCKDOWN:
        human_move_knockdown(hp);
        return;
    default:
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        return;
    }
}

// 0x00A0B3D0 (:1114)
bool human_move_is_crouching(const human* hp)
{
    return hp->cur_stance == HST_CROUCH;
}

// 0x00A30E10 (:1121): a supported (standing on something) human crouches; a player's free exterior camera crouches
// too and a player at a cover edge moves off it
void human_move_start_crouching(human* hp)
{
    if (!hp->rdd.is_loaded() || !hp->rdd.get_data()->controller->flags.is_supported)
    {
        return;
    }
    hp->crouch_to_stand_test_ts.set(0);
    hp->cur_stance = HST_CROUCH;
    if (hp->get_sub_type() != OT_SUB_HUMAN_PLAYER)
    {
        return;
    }
    if (Rfg_camera.mode == CAMERA_FREE_MODE && camera_free_get_submode() == FREE_CAMERA_SUBMODE_EXTERIOR_STAND)
    {
        // Magic numbers: the submode's blend (1) and time (0.3 s).
        camera_free_set_submode(FREE_CAMERA_SUBMODE_EXTERIOR_CROUCH, 1.0f, 0.3f);
    }
    if (hp->cur_state == HS_COVER)
    {
        if (hp->movement_submode == HMSM_COVER_LEFT_EDGE)
        {
            hp->movement_submode = HMSM_COVER_LEFT;
            return;
        }
        if (hp->movement_submode == HMSM_COVER_RIGHT_EDGE)
        {
            hp->movement_submode = HMSM_COVER_RIGHT;
        }
    }
}

// 0x00A809B0 (:1151): the stand-up test ignores the human, its weapon and the zones
bool crouch_to_stand_callback(object* hit_objp, void* vp)
{
    if (hit_objp == nullptr)
    {
        return true;
    }
    if ((object*)((human*)vp)->get_equipped_weapon() == hit_objp || hit_objp == vp || hit_objp->get_obj_type() == OT_ZONE)
    {
        return false;
    }
    return true;
}

// 0x00A809E0 (:1173)
// A crouching human stands up (tested every 600 ms) unless crouching in cover or blocked above (a sphere of 0.36 m from
// 1 m to 1.3 m above its feet); a blocked player falls into the seat of what blocks it (vehicle_interaction_ragdoll_
// fall_in_seat, with no vehicle as often as not). A player's free exterior camera stands up.
void human_move_stop_crouching(human* hp)
{
    if (!hp->crouch_to_stand_test_ts.elapsed())
    {
        return;
    }
    if (hp->cur_stance == HST_CROUCH)
    {
        if (hp->cur_state == HS_COVER && hp->hflags.crouch_cover)
        {
            return;
        }
        // Magic numbers: the test every 600 ms, 1 m to 1.3 m up, 0.36 m radius.
        hp->crouch_to_stand_test_ts.set(600);
        raycast_input in;
        in.start = vector(hp->pos.x, hp->pos.y + 1.0f, hp->pos.z);
        in.end = vector(hp->pos.x, hp->pos.y + 1.0f + 0.3f, hp->pos.z);
        in.m_flags = 0;
        in.radius = 0.36f;
        in.filter_callback = crouch_to_stand_callback;
        in.filter_params = hp;
        in.m_filter_info = COLLIDE_PLAYER;
        in.m_hit_collector_type = raycast_input::COLLECT_CLOSEST_HIT;
        raycast_output out;
        if (havok_spherecast(in, out))
        {
            if (hp->get_sub_type() == OT_SUB_HUMAN_PLAYER)
            {
                const unsigned int hit_handle = out.m_hits[0].hit_object_handle;
                object* objp = hit_handle != 0xFFFFFFFF ? World.object_from_handle(hit_handle) : nullptr;
                vehicle_interaction_ragdoll_fall_in_seat(hp, objp != nullptr && objp->get_obj_type() == OT_VEHICLE ? (vehicle*)objp : nullptr);
                return;
            }
        }
        else
        {
            // Magic numbers: the crouch actions (0x3EA, 0x3EB) ramp out in 3 frames.
            vanim_ramp_out_action(hp->char_inst->vp, 0x3EA, 3);
            vanim_ramp_out_action(hp->char_inst->vp, 0x3EB, 3);
            hp->cur_stance = HST_STAND;
        }
    }
    if (hp->get_sub_type() == OT_SUB_HUMAN_PLAYER && Rfg_camera.mode == CAMERA_FREE_MODE && camera_free_get_submode() == FREE_CAMERA_SUBMODE_EXTERIOR_CROUCH)
    {
        camera_free_set_submode(FREE_CAMERA_SUBMODE_EXTERIOR_STAND, 1.0f, 0.3f);
    }
}

#if 0
// ---- not ported yet (the rest of the recovered stub list)
#endif

// 0x00A0B030 (:830): sets the steering; true once facing it to within 0.0005 radians.
bool human_move_turn_to_target(human* hp, const vector& pnt)
{
    hp->steering_vector.x = pnt.x - hp->pos.x;
    hp->steering_vector.y = pnt.y - hp->pos.y;
    hp->steering_vector.z = pnt.z - hp->pos.z;
    hp->steering_vector.flatten_normalize_safe();
    if (hp->starting_turn)
    {
        return false;
    }
    vector dir = hp->steering_vector;
    if (hp->steering_heading_offset != 0.0f)
    {
        const vector unrotated = dir;
        vec_rot_point_around_line(dir, unrotated, hp->steering_heading_offset, ZERO_VECTOR, UP_VECTOR);
    }
    float dot = hp->orient.fvec.y * dir.y + hp->orient.fvec.x * dir.x + hp->orient.fvec.z * dir.z;
    if (dot - -1.0f < 0.0f)
    {
        dot = -1.0f;
    }
    if (0.0f <= dot - 1.0f)
    {
        dot = 1.0f;
    }
    const float angle = fl_acos(dot);
    return angle - 0.0f < 0.0005f && -(angle - 0.0f) < 0.0005f;
}
