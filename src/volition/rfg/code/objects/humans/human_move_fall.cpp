// Original: volition/rfg/code/objects/humans/human_move_fall.cpp
//
// Falling (player_minimal.md P4): the fall submodes (normal, worry past the safe speed, panic past the fatal speed,
// free fall with air drag), landing damage and the safe and fatal fall speeds (tweak table items, 0 until the tweak
// table is read).
// The external interface is declared in human_move.h.

#include "volition/rfg/code/objects/humans/human_move.h"

#include "volition/rfg/code/camera/camera_free.h"
#include "volition/rfg/code/gameplay/difficulty.h"
#include "volition/rfg/code/gamewide/tweak_table.h"
#include "volition/rfg/code/geom/character_instance.h"
#include "volition/rfg/code/havok/api/hkcharactercontroller.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_game/multi_game_object_backpack.h"
#include "volition/rfg/code/multi/multi_game/multi_game_player.h"
#include "volition/rfg/code/multi/multi_game/multi_game_weapons.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_combat.h"
#include "volition/rfg/code/objects/humans/human_info.h"
#include "volition/rfg/code/objects/humans/human_ragdoll.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/humans/player_cover.h"
#include "volition/rfg/code/pathfinding/bb_pathfinding.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/os/frametime.h"

#include <math.h>

// ---- globals and file statics
// player* Player (0x03023874) is used here too; placed in player.cpp.
float Npc_fatal_jumping_fall_speed; // 0x0301A224
float Npc_fatal_fall_speed; // 0x0301A23C
float Npc_safe_fall_speed; // 0x03023688
float Npc_safe_jumping_fall_speed; // 0x030266AC
float Player_fatal_fall_speed; // 0x030266B8
float Player_safe_fall_speed; // 0x030269C4
static tweak_table_item Tweak_table_item_Player_safe_fall_speed("Player_safe_fall_speed", &Player_safe_fall_speed); // 0x0339FB30
static tweak_table_item Tweak_table_item_Player_fatal_fall_speed("Player_fatal_fall_speed", &Player_fatal_fall_speed); // 0x030FCDD0
static tweak_table_item Tweak_table_item_Npc_safe_jumping_fall_speed("Npc_safe_jumping_fall_speed", &Npc_safe_jumping_fall_speed); // 0x0353F06C
static tweak_table_item Tweak_table_item_Npc_fatal_jumping_fall_speed("Npc_fatal_jumping_fall_speed", &Npc_fatal_jumping_fall_speed); // 0x03067EC0
static tweak_table_item Tweak_table_item_Npc_safe_fall_speed("Npc_safe_fall_speed", &Npc_safe_fall_speed); // 0x033B1FC0
static tweak_table_item Tweak_table_item_Npc_fatal_fall_speed("Npc_fatal_fall_speed", &Npc_fatal_fall_speed); // 0x035B5930

// The player's backpack scales its fall speeds (1 without one).
static float human_fall_speed_factor(const human* hp)
{
    if (hp->get_obj_type() == OT_HUMAN && ((const player*)hp)->backpack != nullptr)
    {
        return ((const player*)hp)->backpack->get_backpack_abilities()->fall_speed_safety_factor;
    }
    return 1.0f;
}

// Puts a falling human into a fall submode unless it is an npc in cover (alive) or ragdolled; any mode but pathfinding,
// jumping and falling drops the path. human_set_movement_mode inlined (human.cpp). Returns false when skipped.
static bool human_fall_set_mode(human* hp, human_move_submodes submode)
{
    if ((hp->get_sub_type() == OT_SUB_HUMAN_NPC && !hp->is_killed() && hp->cur_state == HS_COVER) || hp->ragdoll_state == HUMAN_RAGDOLL_STATE_ACTIVE ||
        hp->ragdoll_state == HUMAN_RAGDOLL_STATE_BLEND_OUT)
    {
        return false;
    }
    if (hp->movement_mode != HMM_FALL)
    {
        hp->previous_movement_mode = hp->movement_mode;
        hp->movement_mode = HMM_FALL;
    }
    hp->movement_submode = submode;
    return true;
}

// Ends a landing: humans following a path (not the player) resume pathfinding (recalculated at nodes when asked);
// the others stop (HMM_NONE; not npcs in cover or on ladders, nor ragdolls). Inlined three times in human_move_fall.
static void human_fall_end_landing(human* hp)
{
    if (!object_is_player(hp) && hp->pf.path != nullptr)
    {
        if ((hp->get_sub_type() != OT_SUB_HUMAN_NPC || hp->is_killed() || hp->cur_state != HS_COVER) && hp->ragdoll_state != HUMAN_RAGDOLL_STATE_ACTIVE &&
            hp->ragdoll_state != HUMAN_RAGDOLL_STATE_BLEND_OUT)
        {
            if (hp->movement_mode != HMM_PATHFINDING)
            {
                hp->pf.dest_check_timer.set(0);
                hp->pf.last_dest_check_pos = hp->pos;
                hp->previous_movement_mode = hp->movement_mode;
                hp->movement_mode = HMM_PATHFINDING;
            }
            hp->movement_submode = HMSM_NONE;
        }
        return;
    }
    if ((hp->get_sub_type() == OT_SUB_HUMAN_NPC && !hp->is_killed() && hp->cur_state == HS_COVER) || (hp->hflags.is_on_ladder && hp->get_sub_type() == OT_SUB_HUMAN_NPC) ||
        hp->ragdoll_state == HUMAN_RAGDOLL_STATE_ACTIVE || hp->ragdoll_state == HUMAN_RAGDOLL_STATE_BLEND_OUT)
    {
        return;
    }
    if (hp->movement_mode != HMM_NONE)
    {
        hp->steering_vector.zero();
        hp->previous_movement_mode = hp->movement_mode;
        hp->movement_mode = HMM_NONE;
    }
    hp->movement_submode = HMSM_NONE;
    const human_move_modes mode = hp->movement_mode;
    if (mode != HMM_PATHFINDING && mode != HMM_PATHFINDING_TO_VEHICLE && mode != HMM_JUMP && mode != HMM_FALL)
    {
        hp->pf.dest_check_timer.invalidate();
        if (hp->pf.path != nullptr)
        {
            bb_pf_free_path(&hp->pf.path);
        }
    }
}

// 0x00A0B400 (:97)
void human_move_fall_system_init()
{
}

// 0x00A0B410 (:101)
void human_move_fall_level_init_post()
{
}

// 0x00A80BB0 (:104)
// Air drag from 8 m/s: the terminal velocity is 120 mph falling feet (or head) first and 200 mph spread out
// (by how much the body axis lines up with the velocity).
void human_move_fall_update_speed(human* hp)
{
    if (!(8.0f <= hp->actual_velocity_mag))
    {
        return;
    }
    vector head;
    human_get_head_pos(hp, head);
    vector body = head - hp->pos;
    body.normalize();
    const vector dir = hp->actual_velocity.copy_normalize_safe();
    const float along = (float)fabs((double)body.y * -dir.y + (double)body.x * -dir.x + (double)body.z * -dir.z);
    // Magic numbers: 120 and 200 mph terminal velocities, 0.44704 m/s per mph, gravity 9.8.
    const float terminal_mph = (float)((1.0 - (double)along) * 120.0 + (double)along * 200.0);
    const float scale = (float)((-9.8 / ((double)terminal_mph * terminal_mph * 0.44704)) * (double)hp->actual_velocity_mag * (double)Frametime + 1.0);
    hp->velocity.x = hp->actual_velocity.x * scale;
    hp->velocity.y = hp->actual_velocity.y * scale;
    hp->velocity.z = hp->actual_velocity.z * scale;
}

// 0x00AC0F50 (:137)
// The fall submodes: starting (leaving cover), normal (npcs worry past the safe speed), worry (panic past the fatal
// speed), panic, free fall (drag, the speed capped by the top speed, ragdolling on impact), and landing (the
// equipped weapon comes back for the player once the landing animation ends; then the human stops or resumes its
// path, standing, running after a running landing).
void human_move_fall(human* hp)
{
    if (hp == nullptr || hp->is_killed())
    {
        return;
    }
    vector vel;
    if (hp->rdd.is_loaded())
    {
        hp->rdd.get_data()->controller->get_velocity(vel);
    }
    else
    {
        vel.zero();
    }
    camera_free_suppress_orientation_lag();
    switch (hp->movement_submode)
    {
    case HMSM_FALL_START:
    {
        if (hp->cur_state == HS_COVER)
        {
            player* pp = World.object_cast<player>(hp);
            if (pp != nullptr)
            {
                player_exit_cover(pp, false);
            }
        }
        hp->hflags.is_falling = 1;
        human_fall_set_mode(hp, HMSM_FALL_NORMAL);
        hp->cur_state = HS_FALL;
        return;
    }
    case HMSM_FALL_NORMAL:
        hp->hflags.is_falling = 1;
        if (hp->get_sub_type() == OT_SUB_HUMAN_PLAYER)
        {
            return;
        }
        if (-human_move_fall_get_safe_speed(hp, true) <= vel.y)
        {
            return;
        }
        human_fall_set_mode(hp, HMSM_FALL_WORRY);
        hp->cur_state = HS_FALL;
        return;
    case HMSM_FALL_WORRY:
        hp->hflags.is_falling = 1;
        if (-human_move_fall_get_fatal_speed(hp, true) <= vel.y)
        {
            return;
        }
        human_fall_set_mode(hp, HMSM_FALL_PANIC);
        hp->cur_state = HS_FALL;
        return;
    case HMSM_FALL_PANIC:
        hp->hflags.is_falling = 1;
        return;
    case HMSM_FALL_FREE:
    {
        hp->hflags.is_falling = 1;
        human_move_fall_update_speed(hp);
        hp->hflags.use_current_velocity = 1;
        hp->hflags.use_as_final_velocity = 1;
        float speed = hp->velocity.mag_xz();
        const float max_speed = hp->h_info->get_max_speed();
        if (0.0f <= speed - max_speed)
        {
            speed = max_speed;
        }
        hp->move_speed = speed;
        // Magic number: 0.13333334, two animation frames (at 15 fps) of impact before ragdolling.
        if (0.13333334f <= hp->ragdoll_on_impact_time)
        {
            return;
        }
        human_ragdoll_on_impact(hp, 1.0f, 0.1f, true);
        return;
    }
    case HMSM_LAND:
    {
        // the player's landing animation does not hold the landing
        bool landing = hp->char_inst->is_playing_action(ANIM_ALIAS_LAND);
        if (object_is_player(hp->get_handle()))
        {
            landing = false;
        }
        // Magic numbers: 0x16E and 0x16C, the running landing actions.
        if (!hp->char_inst->is_playing_action((vanim_aliases)0x16E))
        {
            if (!hp->char_inst->is_playing_action((vanim_aliases)0x16C))
            {
                if (landing)
                {
                    return;
                }
                if (World.object_cast<player>(hp) != nullptr)
                {
                    inventory_item* iip = hp->desired_equipped_inv_item;
                    if (iip == nullptr)
                    {
                        iip = hp->last_equipped_weapon;
                    }
                    human_equip_inventory_item(hp, iip);
                }
                hp->move_speed = -1.0f;
                hp->cur_state = HS_STANDARD;
                if (!object_is_player(hp) && hp->pf.path != nullptr)
                {
                    human_fall_end_landing(hp); // inlined
                    if (hp->hflags.recalculate_at_node)
                    {
                        human_pathfind_to(hp, hp->pf.eventual_goal_pos, HS_CURRENT, HMS_CURRENT, 0xFFFFFFFFFFFFFFFFull, false);
                    }
                    return;
                }
                human_fall_end_landing(hp); // inlined
                return;
            }
            if (landing)
            {
                return;
            }
            human_fall_end_landing(hp); // inlined
            hp->cur_state = HS_STANDARD;
            hp->cur_movement_state = HMS_IDLE;
            hp->move_speed = -1.0f;
            return;
        }
        if (landing)
        {
            return;
        }
        human_fall_end_landing(hp); // inlined
        hp->cur_movement_state = HMS_RUN;
        hp->cur_state = HS_STANDARD;
        hp->move_speed = -1.0f;
        return;
    }
    default:
        return;
    }
}

// 0x00AD4D90 (:268)
// A landing faster than the safe speed hurts: from nothing up to all hit points at the fatal speed (single player:
// the local player only, scaled by the difficulty; multiplayer clients ask the server).
void human_move_fall_do_landing_damage(human* hp)
{
    const float safe_speed = human_move_fall_get_safe_speed(hp, true); // inlined
    const float speed = hp->falling_vel_mag;
    if (!(safe_speed < speed))
    {
        return;
    }
    const float fatal_speed = human_move_fall_get_fatal_speed(hp, true);
    const float damage = fl_parameter(speed, safe_speed, fatal_speed, 0.0f, (float)hp->max_hit_points, false);
    const weapon_info* w_info = nullptr;
    if (Multiplayer == true)
    {
        w_info = multi_gw_get_custom_weapon_info(MCW_FALL_DAMAGE);
    }
    if (!Multiplayer)
    {
        if ((player*)hp == Player)
        {
            human_damage(hp, nullptr, difficulty_get_value(DIFFICULTY_FALLING_DAMAGE, NUM_DIFFICULTY_LEVELS) * damage, DBT_NONE, w_info, nullptr, true, false);
        }
    }
    else if ((player*)hp == Player)
    {
        if (Multiplayer == true && !multi_is_server())
        {
            multi_game_player_request_damage(damage, RDR_FALL);
            return;
        }
        human_damage(hp, nullptr, damage, DBT_NONE, w_info, nullptr, true, false);
    }
}

// 0x00A30ED0 (:301). LTCG: hp in ESI.
// The player's (scaled by its backpack), or an npc's (jumping from a building when allowed).
float human_move_fall_get_safe_speed(const human* hp, bool allow_jump)
{
    if (hp->get_sub_type() == OT_SUB_HUMAN_PLAYER)
    {
        return Player_safe_fall_speed * human_fall_speed_factor(hp);
    }
    if (allow_jump && hp->hflags.jumping_from_building)
    {
        return Npc_safe_jumping_fall_speed;
    }
    return Npc_safe_fall_speed;
}

// 0x00A30F70 (:315)
float human_move_fall_get_fatal_speed(const human* hp, bool allow_jump)
{
    if (hp->get_sub_type() == OT_SUB_HUMAN_PLAYER)
    {
        return Player_fatal_fall_speed * human_fall_speed_factor(hp);
    }
    if (allow_jump && hp->hflags.jumping_from_building)
    {
        return Npc_fatal_jumping_fall_speed;
    }
    return Npc_fatal_fall_speed;
}
