// Original: volition/rfg/code/ai/combat/combat_cover.cpp

#include "volition/rfg/code/ai/combat/combat_cover.h"

#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/object_cover_node.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/vector3.h"

#include <math.h>

// Dot products with the node's right vector, cosines of 100, 80, 120, 60, 110 and 70 degrees in single precision
// radians. Dynamic initializers in rfg.exe, zero in .data.
static const float STANCE_CHANGE_RIGHT_TO_LEFT_CUTOFF = (float)cos((double)1.7453f); // 0x01D71FA4
static const float STANCE_CHANGE_LEFT_TO_RIGHT_CUTOFF = (float)cos((double)1.39624f); // 0x01D67FEC
static const float STANCE_CHANGE_CENTER_RIGHT_TO_LEFT_CUTOFF = (float)cos((double)2.09436f); // 0x01D75790
static const float STANCE_CHANGE_CENTER_LEFT_TO_RIGHT_CUTOFF = (float)cos((double)1.04718f); // 0x01D68194
static const float CHOOSE_STANCE_CENTER_LEFT_CUTOFF = (float)cos((double)1.91983f); // 0x01D6833C
static const float CHOOSE_STANCE_CENTER_RIGHT_CUTOFF = (float)cos((double)1.22171f); // 0x01D72044

// Magic numbers: how close an npc counts as at the node, 0.8 squared.
static const float COVER_AT_NODE_DIST_SQ = 0.8f * 0.8f;

// 0x005E3440 (:127)
bool cover_get_stepped_out_fvec(vector& fvec, cover_node_stances cover_stance, cover_node_firing_positions cover_position, const cover_node* node)
{
    if (cover_stance == COVER_STANCE_UNDEFINED || cover_position == COVER_FIRE_UNDEFINED)
    {
        return false;
    }
    if (cover_position == COVER_FIRE_POPUP)
    {
        fvec = node->orient.fvec;
        return true;
    }
    if (cover_stance != COVER_STANCE_CROUCH_LEFT && cover_stance != COVER_STANCE_STAND_LEFT)
    {
        fvec = node->orient.rvec;
        return true;
    }
    fvec.x = -node->orient.rvec.x;
    fvec.y = -node->orient.rvec.y;
    fvec.z = -node->orient.rvec.z;
    return true;
}

// 0x005E3500 (:152)
bool cover_get_stepped_in_orient(matrix& final_orient, const cover_node_stances cover_stance, const cover_node* node)
{
    vector fvec;
    switch (cover_stance)
    {
    case COVER_STANCE_STAND_RIGHT:
    case COVER_STANCE_CROUCH_RIGHT:
        fvec = node->orient.rvec;
        break;
    case COVER_STANCE_STAND_LEFT:
    case COVER_STANCE_CROUCH_LEFT:
        fvec.x = -node->orient.rvec.x;
        fvec.y = -node->orient.rvec.y;
        fvec.z = -node->orient.rvec.z;
        break;
    default:
        return false;
    }
    final_orient.make(fvec);
    return true;
}

// 0x005E35C0 (:235)
bool cover_is_at_node(const npc* np, const cover_node* node)
{
    if (np == nullptr || node == nullptr)
    {
        return false;
    }
    return vec_dist_squared(np->pos, node->pos) <= COVER_AT_NODE_DIST_SQ;
}

// 0x005E3600 (:252)
cover_node_stances cover_get_new_stance(const npc* np, const cover_node* node, const vector& target_pos)
{
    const cover_node_stances stance = cover_stance_from_state(np);
    const cover_node_firing_positions position = cover_firing_position_from_state(np);
    const bool left = stance == COVER_STANCE_CROUCH_LEFT || stance == COVER_STANCE_STAND_LEFT;
    // A node that fires to one side only keeps the stance.
    if ((node->flags.fire_right == 0 && node->flags.fire_left != 0) || (node->flags.fire_left == 0 && node->flags.fire_right != 0))
    {
        return stance;
    }
    const bool right = stance == COVER_STANCE_CROUCH_RIGHT || stance == COVER_STANCE_STAND_RIGHT;
    if (!left && !right)
    {
        return COVER_STANCE_UNDEFINED;
    }
    vector target_dir;
    vec_normalized_dir(target_dir, target_pos, node->pos);
    const float dot = node->orient.rvec.y * target_dir.y + target_dir.x * node->orient.rvec.x + node->orient.rvec.z * target_dir.z;
    if (left)
    {
        const float cutoff = position == COVER_FIRE_POPUP ? STANCE_CHANGE_CENTER_LEFT_TO_RIGHT_CUTOFF : STANCE_CHANGE_LEFT_TO_RIGHT_CUTOFF;
        if (cutoff < dot)
        {
            return stance == COVER_STANCE_CROUCH_LEFT ? COVER_STANCE_CROUCH_RIGHT : COVER_STANCE_STAND_RIGHT;
        }
    }
    else
    {
        const float cutoff = position == COVER_FIRE_POPUP ? STANCE_CHANGE_CENTER_RIGHT_TO_LEFT_CUTOFF : STANCE_CHANGE_RIGHT_TO_LEFT_CUTOFF;
        if (dot < cutoff)
        {
            return stance == COVER_STANCE_CROUCH_RIGHT ? COVER_STANCE_CROUCH_LEFT : COVER_STANCE_STAND_LEFT;
        }
    }
    return stance;
}

// 0x005E3770 (:321)
cover_node_firing_positions cover_pick_firing_position(cover_node_stances cover_stance, const cover_node* node, const vector& target_pos)
{
    if (cover_stance == COVER_STANCE_UNDEFINED)
    {
        return COVER_FIRE_UNDEFINED;
    }
    if (node->flags.fire_left == 0 && node->flags.fire_popup == 0 && node->flags.fire_right == 0)
    {
        return COVER_FIRE_UNDEFINED;
    }
    if (cover_stance != COVER_STANCE_STAND_LEFT && cover_stance != COVER_STANCE_STAND_RIGHT && node->flags.fire_popup != 0)
    {
        if (node->flags.fire_left == 0 && node->flags.fire_right == 0)
        {
            return COVER_FIRE_POPUP;
        }
        vector target_dir;
        vec_normalized_dir(target_dir, target_pos, node->pos);
        const float dot = target_dir * node->orient.rvec;
        if (cover_stance == COVER_STANCE_CROUCH_LEFT)
        {
            if (CHOOSE_STANCE_CENTER_LEFT_CUTOFF <= dot)
            {
                return COVER_FIRE_POPUP;
            }
        }
        else if (dot <= CHOOSE_STANCE_CENTER_RIGHT_CUTOFF)
        {
            return COVER_FIRE_POPUP;
        }
    }
    return COVER_FIRE_LEANOUT;
}

// 0x005E3870 (:367)
cover_node_stances cover_pick_stance(const cover_node* node, const vector& target_pos)
{
    const bool crouch = node->flags.crouch != 0;
    vector target_dir;
    vec_normalized_dir(target_dir, target_pos, node->pos);
    const bool target_on_left = node->orient.rvec.y * target_dir.y + target_dir.x * node->orient.rvec.x + node->orient.rvec.z * target_dir.z <= 0.0f;
    if (node->flags.fire_popup == 0 && node->flags.fire_left == 0 && node->flags.fire_right == 0)
    {
        if (!target_on_left)
        {
            return crouch ? COVER_STANCE_CROUCH_RIGHT : COVER_STANCE_STAND_RIGHT;
        }
    }
    else
    {
        if (node->flags.fire_right != 0 && (node->flags.fire_left == 0 || !target_on_left))
        {
            return crouch ? COVER_STANCE_CROUCH_RIGHT : COVER_STANCE_STAND_RIGHT;
        }
        if (node->flags.fire_left == 0)
        {
            // Only popup is left, always from a crouch, whatever the node's crouch flag says.
            if (node->flags.fire_popup != 0)
            {
                return target_on_left ? COVER_STANCE_CROUCH_LEFT : COVER_STANCE_CROUCH_RIGHT;
            }
            return COVER_STANCE_UNDEFINED;
        }
    }
    return crouch ? COVER_STANCE_CROUCH_LEFT : COVER_STANCE_STAND_LEFT;
}

// 0x005E39A0 (:406)
cover_node_stances cover_get_current_stance(const npc* np)
{
    if (np->cur_state == HS_COVER && (np->cur_stance == HST_CROUCH || np->cur_stance == HST_STAND))
    {
        switch (np->movement_submode)
        {
        case HMSM_COVER_RIGHT:
        case HMSM_COVER_RIGHT_EDGE:
        case HMSM_COVER_RIGHT_LEAN:
        case HMSM_COVER_RIGHT_LEANUP:
        case HMSM_COVER_RIGHT_BLINDFIRE:
        case HMSM_COVER_RIGHT_BLINDFIRE2:
            return np->cur_stance == HST_CROUCH ? COVER_STANCE_CROUCH_RIGHT : COVER_STANCE_STAND_RIGHT;
        case HMSM_COVER_LEFT:
        case HMSM_COVER_LEFT_EDGE:
        case HMSM_COVER_LEFT_LEAN:
        case HMSM_COVER_LEFT_LEANUP:
        case HMSM_COVER_LEFT_BLINDFIRE:
        case HMSM_COVER_LEFT_BLINDFIRE2:
            return np->cur_stance == HST_CROUCH ? COVER_STANCE_CROUCH_LEFT : COVER_STANCE_STAND_LEFT;
        default:
            break;
        }
    }
    return COVER_STANCE_UNDEFINED;
}

// 0x005E3A30 (:454)
cover_node_firing_positions cover_get_current_firing_position(const npc* np)
{
    if (np->cur_state == HS_COVER)
    {
        switch (np->movement_submode)
        {
        case HMSM_COVER_RIGHT_LEAN:
        case HMSM_COVER_LEFT_LEAN:
            return COVER_FIRE_LEANOUT;
        case HMSM_COVER_RIGHT_LEANUP:
        case HMSM_COVER_LEFT_LEANUP:
            return COVER_FIRE_POPUP;
        default:
            break;
        }
    }
    return COVER_FIRE_UNDEFINED;
}

// 0x006946B0 (:486)
ai_results cover_generate_path_to_node(npc* np, const cover_node* node, human_movement_states m_state, human_stances h_stance)
{
    ai_system* ai = np->get_ai();
    human_set_stance(np, h_stance);
    if (ai->is_pathing_to(node->pos, 0.25f, 1.0f))
    {
        return AI_RESULT_SUCCESS;
    }
    if (node->flags.on_mover != 0)
    {
        return ai_pf_to_result(ai->pathfind_to(node->pos, m_state, h_stance, AI_PATHFIND_PRIORITY_HIGH, 0xFFFFFFFFFFFFFFFFull, false));
    }
    if (node->flags.off_navmesh == 0)
    {
        if (node->closest_nav_cell_handle == 0xFFFFFFFFFFFFFFFFull)
        {
            return AI_RESULT_FAILURE;
        }
        return ai_pf_to_result(ai->pathfind_to(node->pos, m_state, h_stance, AI_PATHFIND_PRIORITY_HIGH, node->closest_nav_cell_handle, false));
    }
    if (ai->is_pathing_to(node->closest_point, 0.25f, 1.0f))
    {
        return AI_RESULT_SUCCESS;
    }
    // An off navmesh node: walk straight at it from within this distance of the mesh point (0.3 + 0.2, squared in double),
    // otherwise path to the mesh point first.
    const double direct_dist = (double)0.3f + (double)0.2f;
    if ((float)(direct_dist * direct_dist) <= vec_xz_dist_squared(np->pos, node->closest_point))
    {
        return ai_pf_to_result(ai->pathfind_to(node->closest_point, m_state, h_stance, AI_PATHFIND_PRIORITY_HIGH, node->closest_nav_cell_handle, false));
    }
    return ai_pf_to_result(ai->set_direct_movement_to_pos(node->pos, 0xFFFFFFFFFFFFFFFFull, m_state, h_stance, false));
}
