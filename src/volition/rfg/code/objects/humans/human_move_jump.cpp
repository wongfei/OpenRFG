// Original: volition/rfg/code/objects/humans/human_move_jump.cpp
//
// Jumping (player_minimal.md P4): the jump submodes (start, pre, mid) and the switch to falling once the jump
// animation releases the movement or the human is airborne.
// The external interface is declared in human_move.h.

#include "volition/rfg/code/objects/humans/human_move.h"

#include "volition/rfg/code/geom/character.h"
#include "volition/rfg/code/geom/character_instance.h"
#include "volition/rfg/code/geom/vanim.h"
#include "volition/rfg/code/havok/api/hkcharactercontroller.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/pathfinding/bb_pathfinding.h"

#include "keen/base/trace.hpp"

// ---- internal functions
static void human_jump_movement(human* hp); // 0x00A31010 (:53)

// Enters a jump or fall submode unless the human is an npc in cover (alive) or ragdolled; any mode but pathfinding,
// jumping and falling drops the path. human_set_movement_mode inlined (human.cpp).
static void human_jump_set_mode(human* hp, human_move_modes mode, human_move_submodes submode)
{
    if ((hp->get_sub_type() == OT_SUB_HUMAN_NPC && !hp->is_killed() && hp->cur_state == HS_COVER) || hp->ragdoll_state == HUMAN_RAGDOLL_STATE_ACTIVE ||
        hp->ragdoll_state == HUMAN_RAGDOLL_STATE_BLEND_OUT)
    {
        return;
    }
    if (hp->movement_mode != mode)
    {
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

// 0x00A31010 (:53). LTCG: hp in ESI.
// Without a take-off or special jump, once the jump animation ends (or a code-driven jump is back on the ground) the
// human falls at its speed; the take-off flag clears when a normal jump is supported again; the special jump falls
// once it releases the movement.
static void human_jump_movement(human* hp)
{
    if (!hp->hflags.jump_take_off && !hp->char_inst->is_playing_action(ANIM_ALIAS_SPECIAL_JUMP) &&
        (!hp->char_inst->is_playing_action(ANIM_ALIAS_JUMP) || (!hp->code_driven_start_jump && human_controller_is_on_ground(hp))))
    {
        const float speed = hp->move_speed;
        human_jump_set_mode(hp, HMM_FALL, HMSM_FALL_NORMAL);
        hp->hflags.is_falling = 1;
        hp->move_speed = speed;
        hp->cur_state = HS_FALL;
        hp->falling_vel_mag = 0.0f;
    }
    if (hp->hflags.jump_take_off && !hp->code_driven_start_jump && hp->rdd.is_loaded() && hp->rdd.get_data()->controller->flags.is_supported)
    {
        hp->hflags.jump_take_off = 0;
    }
    if (hp->char_inst->is_playing_action(ANIM_ALIAS_SPECIAL_JUMP) && vanim_has_trigger_just_elapsed(hp->char_inst->vp, Trigger_release_movement, ANIM_ALIAS_SPECIAL_JUMP, -1))
    {
        human_jump_set_mode(hp, HMM_FALL, HMSM_FALL_NORMAL);
        hp->hflags.is_falling = 1;
        hp->move_speed = -1.0f;
        hp->cur_state = HS_FALL;
        hp->falling_vel_mag = 0.0f;
    }
}

// 0x00A0B420 (:88)
void human_move_jump_system_init()
{
}

// 0x00A31270 (:92)
// Start: the pre submode; pre: the jump state and the mid submode; mid: the jump movement.
void human_move_jump(human* hp)
{
    switch (hp->movement_submode)
    {
    case HMSM_JUMP_START:
        human_jump_set_mode(hp, HMM_JUMP, HMSM_JUMP_PRE);
        break;
    case HMSM_JUMP_PRE:
        hp->cur_state = HS_JUMP;
        human_jump_set_mode(hp, HMM_JUMP, HMSM_JUMP_MID);
        break;
    case HMSM_JUMP_MID:
        human_jump_movement(hp);
        break;
    case HMSM_LAND:
        keen::debug::nativePrintArgs("Player landed without falling");
        return;
    default:
        break;
    }
}
