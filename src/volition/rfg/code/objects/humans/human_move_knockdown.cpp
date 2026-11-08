// Original: volition/rfg/code/objects/humans/human_move_knockdown.cpp
//
// Knockdowns (player_minimal.md P4): a knocked down human follows its ragdoll until it settles (on the ground or
// riding a contacted object at the same velocity), then gets up (retrying every 0.9 s, forced after 2 s); dead ones
// stop their ragdoll once it rests.
// The external interface is declared in human_move.h.

#include "volition/rfg/code/objects/humans/human_move.h"

#include "volition/rfg/code/geom/character_instance.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/havok/havok_collision_events.h"
#include "volition/rfg/code/havok/havok_ragdoll.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_ragdoll.h"
#include "volition/rfg/code/pathfinding/bb_pathfinding.h"
#include "volition/vlib/code/os/frametime.h"

// ---- file-static globals
static bool Player_auto_get_up = true; // 0x01694367

// 0x00ADD4D0 (:91)
void human_move_knockdown(human* hp)
{
    // function static of the original (seconds)
    static float ragdoll_blend_time = 0.25f; // 0x016AFE70
    const human_move_submodes submode = hp->movement_submode;
    if (submode == HMSM_NONE)
    {
        hp->movement_submode = HMSM_KNOCKDOWN_RAGDOLL;
        return;
    }
    if (submode != HMSM_KNOCKDOWN_RAGDOLL)
    {
        if (submode != HMSM_KNOCKDOWN_GETUP)
        {
            return;
        }
        // getting up: the animation blends back in, the ragdoll stops when the get-up animation ends
        human_controller_process_pre(hp, false);
        float blend = (float)((double)Frametime / (double)ragdoll_blend_time + (double)hp->ragdoll_blend_pct);
        hp->ragdoll_blend_pct = blend;
        if (blend - 0.0f < 0.0f)
        {
            blend = 0.0f;
        }
        if (0.0f <= blend - 1.0f)
        {
            blend = 1.0f;
        }
        hp->ragdoll_blend_pct = blend;
        if (hp->char_inst->is_playing_action(ANIM_ALIAS_GET_UP))
        {
            return;
        }
        human_ragdoll_stop(hp, false);
        return;
    }
    if (hp->ragdoll_idx == -1)
    {
        // no ragdoll: standing again
        if (hp->cur_state == HS_DEAD)
        {
            return;
        }
        hp->move_speed = 0.0f;
        hp->cur_state = HS_STANDARD;
        hp->cur_movement_state = HMS_IDLE;
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
        return;
    }
    if (!havok_ragdoll_is_synced(hp->ragdoll_idx))
    {
        // the human follows the ragdoll's body
        hp->hflags.dont_deform_bones = 0;
        vector body_pos;
        havok_ragdoll_get_pos(hp->ragdoll_idx, body_pos, RAGDOLL_BODY);
        hp->update_pos_and_orient(body_pos, hp->orient, true);
        vector vel;
        havok_ragdoll_get_vel(hp->ragdoll_idx, vel);
        human_controller_set_vel(hp, vel);
    }
    if (hp->is_killed())
    {
        if (havok_ragdoll_is_active(hp->ragdoll_idx) || human_contacting_non_stationary_object(hp))
        {
            return;
        }
        human_ragdoll_stop(hp, true);
        return;
    }
    if (!Player_auto_get_up && object_is_player(hp))
    {
        return;
    }
    timestamp& timeout = hp->knockdown_timeout_ts;
    bool get_up = timeout.elapsed();
    if (!get_up)
    {
        if (!hp->knockdown_timestamp.valid() || !hp->knockdown_timestamp.elapsed())
        {
            return;
        }
        hp->ground_obj_handle = 0xFFFFFFFF;
        float terrain_y;
        havok_terrain_height(terrain_y, hp->pos.x, hp->pos.z, true, nullptr);
        float speed_sq = 0.0f;
        // Magic numbers: within 0.5 m of the terrain, at rest below 1 m/s.
        if (hp->pos.y - terrain_y <= 0.5f)
        {
            if (!havok_ragdoll_is_active(hp->ragdoll_idx))
            {
                get_up = true;
            }
            else
            {
                vector vel;
                havok_ragdoll_get_vel(hp->ragdoll_idx, vel);
                speed_sq = vel.mag_squared();
            }
        }
        else
        {
            // on top of something: the first contacted object below is the ground
            object* ground = nullptr;
            bool on_object = false;
            contact_node* cn = hp->contact_info.m_contact_list;
            if (cn != nullptr)
            {
                for (;;)
                {
                    ground = World.object_from_handle(cn->m_contacted_object);
                    if (!(hp->pos.y < ground->pos.y))
                    {
                        hp->ground_obj_handle = ground->get_handle();
                        on_object = true;
                        break;
                    }
                    cn = cn->next;
                    if (cn == hp->contact_info.m_contact_list || cn == nullptr)
                    {
                        break;
                    }
                }
            }
            if (!havok_ragdoll_is_active(hp->ragdoll_idx))
            {
                get_up = true;
            }
            else
            {
                vector vel;
                havok_ragdoll_get_vel(hp->ragdoll_idx, vel);
                speed_sq = vel.mag_squared();
                if (speed_sq <= 1.0f)
                {
                    get_up = true;
                }
                else
                {
                    if (!on_object)
                    {
                        return;
                    }
                    // relative to the object carrying it
                    vector ground_vel;
                    havok_body_get_point_velocity(ground->havok_handle, hp->pos, ground_vel);
                    vel = vel - ground_vel;
                    speed_sq = vel.mag_squared();
                }
            }
        }
        if (!get_up && 1.0f < speed_sq)
        {
            return;
        }
    }
    if (human_ragdoll_get_up(hp, true))
    {
        hp->knockdown_timestamp.invalidate();
    }
    else
    {
        // Magic numbers: retry in 0.9 s; after 2 s the get-up is forced.
        hp->knockdown_timestamp.set(900);
        if (timeout.elapsed() || !timeout.valid())
        {
            timeout.set(2000);
        }
    }
}
