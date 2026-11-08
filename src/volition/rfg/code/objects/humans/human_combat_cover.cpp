// Original: volition/rfg/code/objects/humans/human_combat_cover.cpp
//
// Reserving nodes and the stepped in and out states; nothing else lives in this file in rfg.exe.

#include "volition/rfg/code/objects/humans/human_combat_cover.h"

#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/geom/anim_enums.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/humans/player_cover.h"
#include "volition/rfg/code/objects/object_cover_node.h"

// 0x00A2FAD0 (:29)
cover_node* human_combat_cover_get_node(const human* hp)
{
    const unsigned int handle = hp->combat.reserved_cover_node;
    if (handle == 0xFFFFFFFF)
    {
        return nullptr;
    }
    object* objp = World.object_from_handle(handle);
    if (objp == nullptr || objp->get_obj_type() != OT_COVER_NODE)
    {
        return nullptr;
    }
    return static_cast<cover_node*>(objp);
}

// 0x00A09D70 (:36)
bool human_combat_cover_is_using_node(const human* hp)
{
    return hp->cur_state == HS_COVER;
}

// 0x00A09DC0 (:62): the four submodes from HMSM_COVER_RIGHT_LEAN.
bool human_combat_cover_is_stepped_out(const human* hp)
{
    return hp->cur_state == HS_COVER && static_cast<unsigned int>(hp->movement_submode - HMSM_COVER_RIGHT_LEAN) < 4;
}

// 0x00A09D90 (:43)
bool human_combat_cover_is_stepped_in(const human* hp)
{
    return hp->cur_state == HS_COVER && (hp->movement_submode == HMSM_COVER_RIGHT || hp->movement_submode == HMSM_COVER_LEFT);
}

// 0x00A9D060 (:85)
void human_combat_cover_abandon_node(human* hp)
{
    if (hp != nullptr && hp->get_sub_type() == OT_SUB_HUMAN_PLAYER)
    {
        player_exit_cover((player*)hp, false);
    }
    if (hp->combat.reserved_cover_node == 0xFFFFFFFF)
    {
        return;
    }
    const human_states state = hp->cur_state;
    hp->combat.reserved_cover_node = 0xFFFFFFFF;
    if (state == HS_COVER)
    {
        human_set_state(hp, HS_STANDARD, HMS_IDLE, -1.0f);
        human_set_movement_mode(hp, HMM_NONE, HMSM_NONE);
    }
    if (hp->get_obj_type() == OT_HUMAN && hp->get_sub_type() == OT_SUB_HUMAN_NPC)
    {
        ai_system* ai = ((npc*)hp)->get_ai();
        ai->in_cover_timer.invalidate();
        ai->out_of_cover_timer.invalidate();
        ai->not_using_cover_timer.set(0);
    }
}

// 0x00A9D190 (:124)
void human_combat_cover_reserve_node(human* hp, cover_node* node)
{
    if (hp->combat.reserved_cover_node == node->get_handle())
    {
        return;
    }
    human_combat_cover_abandon_node(hp);
    hp->combat.reserved_cover_node = node->get_handle();
    if (hp->get_obj_type() == OT_HUMAN && hp->get_sub_type() == OT_SUB_HUMAN_NPC)
    {
        ((npc*)hp)->get_ai()->not_using_cover_timer.invalidate();
    }
}

// 0x00A09DE0 (:157)
animation_action human_combat_cover_get_transition_action(const human* hp, const cover_node* node, cover_node_stances cover_stance)
{
    const float side = cover_stance == COVER_STANCE_STAND_LEFT || cover_stance == COVER_STANCE_CROUCH_LEFT ? -1.0f : 1.0f;
    // The direction halfway between the node's front and the side it opens to. Magic number: one over the square root of two.
    const float x = (node->orient.fvec.x + node->orient.rvec.x * side) * 0.70710677f;
    const float y = (node->orient.fvec.y + node->orient.rvec.y * side) * 0.70710677f;
    const float z = (node->orient.fvec.z + node->orient.rvec.z * side) * 0.70710677f;
    // Each action has a _SIDE variant four further on, for a human not already facing that way.
    const int variant = hp->orient.fvec.y * y + hp->orient.fvec.x * x + hp->orient.fvec.z * z < 0.0f ? 4 : 0;
    switch (cover_stance)
    {
    case COVER_STANCE_STAND_RIGHT:
        return (animation_action)(ANIMATION_ACTION_COVER_INTO_STAND_RIGHT + variant);
    case COVER_STANCE_STAND_LEFT:
        return (animation_action)(ANIMATION_ACTION_COVER_INTO_STAND_LEFT + variant);
    case COVER_STANCE_CROUCH_RIGHT:
        return (animation_action)(ANIMATION_ACTION_COVER_INTO_CROUCH_RIGHT + variant);
    case COVER_STANCE_CROUCH_LEFT:
        return (animation_action)(ANIMATION_ACTION_COVER_INTO_CROUCH_LEFT + variant);
    default:
        return ANIMATION_ACTION_NONE;
    }
}

// 0x00A0A020 (:184)
void human_combat_cover_set_stepped_in_state(human* hp, cover_node_stances cover_stance)
{
    if (cover_stance == COVER_STANCE_UNDEFINED)
    {
        return;
    }
    hp->cur_stance = cover_stance == COVER_STANCE_STAND_RIGHT || cover_stance == COVER_STANCE_STAND_LEFT ? HST_STAND : HST_CROUCH;
    human_set_state(hp, HS_COVER, HMS_IDLE, -1.0f);
    human_set_movement_mode(hp, HMM_NONE, cover_stance == COVER_STANCE_STAND_RIGHT || cover_stance == COVER_STANCE_CROUCH_RIGHT ? HMSM_COVER_RIGHT : HMSM_COVER_LEFT);
}

// 0x00A0A290 (:213): a standing human always leans, whatever the position says.
void human_combat_cover_set_stepped_out_state(human* hp, cover_node_stances cover_stance, cover_node_firing_positions position)
{
    switch (cover_stance)
    {
    case COVER_STANCE_STAND_RIGHT:
        hp->cur_stance = HST_STAND;
        human_set_state(hp, HS_COVER, HMS_IDLE, -1.0f);
        human_set_movement_mode(hp, HMM_NONE, HMSM_COVER_RIGHT_LEAN);
        break;
    case COVER_STANCE_STAND_LEFT:
        hp->cur_stance = HST_STAND;
        human_set_state(hp, HS_COVER, HMS_IDLE, -1.0f);
        human_set_movement_mode(hp, HMM_NONE, HMSM_COVER_LEFT_LEAN);
        break;
    case COVER_STANCE_CROUCH_RIGHT:
        hp->cur_stance = HST_CROUCH;
        human_set_state(hp, HS_COVER, HMS_IDLE, -1.0f);
        human_set_movement_mode(hp, HMM_NONE, position != COVER_FIRE_LEANOUT ? HMSM_COVER_RIGHT_LEANUP : HMSM_COVER_RIGHT_LEAN);
        break;
    case COVER_STANCE_CROUCH_LEFT:
        hp->cur_stance = HST_CROUCH;
        human_set_state(hp, HS_COVER, HMS_IDLE, -1.0f);
        human_set_movement_mode(hp, HMM_NONE, position != COVER_FIRE_LEANOUT ? HMSM_COVER_LEFT_LEANUP : HMSM_COVER_LEFT_LEAN);
        break;
    default:
        break;
    }
}

// 0x005DDB50 (.h:28)
bool human_combat_cover_has_node(const human* hp)
{
    return human_combat_cover_get_node(hp) != nullptr;
}
