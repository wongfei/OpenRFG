// Original: volition/rfg/code/multi/multi_game/multi_game_object_turret.cpp
//
// Every live function: what turret::frame_process_pre calls (stub_misc.md group 2). The turret registration (its sync,
// state pools and query_possible_mask_bits_base) is dead in rfg.exe: multi_game_object_turret_init has no caller and
// nothing else references the vftable (final_push.md step 7). Its functions stay listed below, unported.
// Part of the multi unity TU (rfg_multi_unity.obj, every multi/ .cpp in path order): the file statics are shared
// by the whole tree, and a shared one is placed in its first user (tools/hdrgen/specs/mp0_stubs.py).

#include "volition/rfg/code/multi/multi_game/multi_game_object_turret.h"

#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/level/world.inl"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/turret.h"
#include "volition/rfg/code/weapons/weapons.h"

// ---- file-static globals
// const float PI (0x01309C38) is used here too; placed in multi_game_client_predict.cpp.
static multi_core_state_priority State_priority_base_turret[6]; // 0x0167F1A0, initial data 020000000100000000000000020000000300000004000000
static unsigned int State_mask_turret[6] = {1, 2, 4, 8, 0x10, 0x20}; // 0x0167F1B8
static multi_core_state_priority State_priority_base_automo[7]; // 0x0167F1D0, initial data 02000000020000000200000002000000020000000200000000000000
static multi_pool<object_state_data<turret_data_bundle>,1600> Turret_object_allocator; // 0x016930D8
static multi_pool<multi_sim_state<multi_turret_state>,64> Turret_sim_state_allocator; // 0x01693118
// unsigned int Clock_current_time (0x02FEB538) is used here too; placed in multi.cpp.
// bool Server (0x02FEB590) is used here too; placed in multi_core_session_generic.cpp.
// mp_game_data_wrapper* Mp_game_data (0x02FEB698) is used here too; placed in multi_connection.cpp.

// ---- internal functions
static object_state_data<turret_data_bundle>* turret_alloc_state_data(); // 0x009F60A0 (:205)
static multi_sim_state<multi_turret_state>* turret_alloc_sim_state(); // 0x0099D1A0 (:216)
// LTCG: arguments in EAX (not mapped yet).
static int calc_age_priority_bias(const state_priority& state); // 0x0095DB40 (:324)
static unsigned short query_possible_mask_bits_base(const turret_data_bundle& cur_data, const turret_data_bundle& ref_data); // 0x0097FDE0 (:463)

// 0x0099D2E0 (:1069): the server follows a turret manned by a remote player.
bool multi_game_object_turret_is_client_authoritative(const turret* tp)
{
    if (!Multiplayer || !Server || tp == nullptr || tp->mp_sim_state == nullptr)
    {
        return false;
    }
    const human* op = World.object_from_handle<human>(tp->current_operator);
    for (int i = 0; i < World.get_num_objects<player>(); i++)
    {
        player* pp = World.get_object<player>(i);
        if (op == pp)
        {
            return !object_is_local_player(pp);
        }
    }
    return false;
}

// 0x009802E0 (:1100): a replicated state on a turret the local player does not man. The target is a unit along the aim.
void multi_game_object_turret_apply_sim_state(const multi_turret_state* t_state, turret* tp)
{
    const player* op = World.object_from_handle<player>(tp->current_operator);
    if (!object_is_local_player(op))
    {
        tp->pos = t_state->position;
        tp->orient = t_state->orientation;
        matrix target_orient;
        target_orient.set(t_state->target_orient_ph.x, 0.0f, t_state->target_orient_ph.y);
        weapon* wp = tp->get_weapon_inst();
        vector from = tp->pos;
        if (wp != nullptr)
        {
            wp->get_muzzle_transform(from, nullptr);
        }
        vector target;
        target.x = target_orient.fvec.x + from.x;
        target.y = target_orient.fvec.y + from.y;
        target.z = target_orient.fvec.z + from.z;
        tp->mp_set_target_pos(target);
        if (t_state->firing)
        {
            // Original: reads through null without a weapon.
            tp->get_weapon_inst()->fire(ZERO_VECTOR);
        }
    }
    if (Multiplayer && !Server)
    {
        tp->set_hit_points_mp(t_state->hit_points);
        if (t_state->hit_points == 0)
        {
            tp->kill(nullptr, nullptr);
        }
    }
}
