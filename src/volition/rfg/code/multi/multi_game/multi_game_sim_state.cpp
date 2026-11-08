// Original: volition/rfg/code/multi/multi_game/multi_game_sim_state.cpp
//
// Decompiled whole (multiplayer.md MP5): the received states of a remote object (multi_sim_state<T>, the last 25 in
// network time order) and their interpolation and extrapolation at a network time, and the cached network to game
// handle lookup. The templates are written once; rfg.exe has an instance per state type (object, player, item, mover,
// projectile, turret, automobile, walker; the three vehicle instances have the generic code). The player's state has
// its own interpolation and extrapolation (multi_player_state specializations at the end).
// Part of the multi unity TU (rfg_multi_unity.obj, every multi/ .cpp in path order): the file statics are shared
// by the whole tree, and a shared one is placed in its first user (tools/hdrgen/specs/mp0_stubs.py).

#include "volition/rfg/code/multi/multi_game/multi_game_sim_state.h"

#include "volition/rfg/code/multi/multi_core/include/multi_core_objects.h"
#include "volition/vlib/code/math/quaternion.h"
#include "volition/vlib/code/time/timestamp.h"

#include <stdlib.h>

// ---- file-static globals
// unsigned int Max_extrap_time_ms (0x0167F4C4) is used here too; placed in multi_game_object_rfg_movers.cpp.
// unsigned int Clock_current_time (0x02FEB538) is used here too; placed in multi.cpp.

// 0x00963B50 (:26)
network_to_game_handle::network_to_game_handle()
{
    m_network_handle = 0xFFFF;
    m_game_handle = 0xFFFFFFFF;
}

// 0x00963B70 (:30)
network_to_game_handle::network_to_game_handle(unsigned short net_handle)
{
    m_network_handle = net_handle;
    m_game_handle = 0xFFFFFFFF;
}

// 0x00963B90 (:33)
void network_to_game_handle::clear()
{
    m_game_handle = 0xFFFFFFFF;
    m_network_handle = 0xFFFF;
}

// 0x00963BA0 (:39)
void network_to_game_handle::set_network_handle(unsigned short net_handle)
{
    m_game_handle = 0xFFFFFFFF;
    m_network_handle = net_handle;
}

// 0x009D2D20 (:48): looked up once (a handle not found yet is looked up again next time)
unsigned int network_to_game_handle::get_game_handle() const
{
    if (m_network_handle == 0xFFFF)
    {
        return 0xFFFFFFFF;
    }
    if (m_game_handle == 0xFFFFFFFF)
    {
        // The cache is written from a const getter (rfg.exe's member is not mutable).
        const_cast<network_to_game_handle*>(this)->m_game_handle = multi_core_game_handle_from_network_handle(m_network_handle, true);
    }
    return m_game_handle;
}

// 0x00963BC0 (:63)
unsigned short network_to_game_handle::get_network_handle() const
{
    return m_network_handle;
}

// 0x00963BD0 (:96): empty in the shipped build
void multi_game_sim_state_system_init()
{
}

// A network time difference in milliseconds as a float (unsigned).
static float mss_ms_to_float(unsigned int ms)
{
    return (float)(double)ms;
}

// <multi_item_state> 0x0096A240 (:112)
// <multi_mover_state> 0x0096A820 (:112)
// <multi_projectile_state> 0x0096AD70 (:112)
// <multi_player_state> 0x00968BD0 (:112)
// A state older than the newest one, or more than 5 s away from the network clock (a time of 0 always passes), is
// dropped; an invalid orientation becomes the identity; when all 25 slots are used the oldest state is dropped.
template<typename T>
void multi_sim_state<T>::add_state(T& state)
{
    if (m_states_used != 0 && m_states[m_states_used - 1].network_time > state.network_time)
    {
        return;
    }
    if (state.network_time != 0 && abs((int)(state.network_time - Clock_current_time)) >= 5001) // Magic number: 5 s
    {
        return;
    }
    if (!state.orientation.is_valid())
    {
        state.orientation.make_identity();
    }
    if (m_states_used == 25)
    {
        for (int i = 0; i < m_states_used - 1; i++)
        {
            m_states[i] = m_states[i + 1];
        }
        m_states_used--;
    }
    m_states[m_states_used] = state;
    m_states_used++;
}

// <multi_mover_state> 0x0098ABE0 (:152)
// <multi_turret_state> 0x00989AC0 (:152)
// <multi_automobile_state> 0x00989800 (:152)
// <multi_walker_state> 0x00989960 (:152)
// <multi_item_state> 0x0098A560 (:152)
// <multi_projectile_state> 0x0098B070 (:152)
// The state at a network time: extrapolated from the newest state when the time is past it, interpolated between the
// two states around it, else (older than every state) the newest state.
template<typename T>
bool multi_sim_state<T>::obtain_state(unsigned int network_time, T* state, bool& exceeded_extrap_time)
{
    exceeded_extrap_time = false;
    if (m_states_used == 0)
    {
        return false;
    }
    const int newest = m_states_used - 1;
    for (int prev_index = newest; prev_index >= 0; prev_index--)
    {
        if (m_states[prev_index].network_time <= network_time)
        {
            if (prev_index == newest)
            {
                extrapolate_state(network_time, state, exceeded_extrap_time); // inlined
            }
            else
            {
                interpolate_state(network_time, prev_index, state);
            }
            return true;
        }
    }
    *state = m_states[newest];
    return true;
}

// <multi_player_state> 0x00968C70 (:180)
template<typename T>
bool multi_sim_state<T>::get_latest_state(unsigned int& update_time, T* state) const
{
    if (m_states_used == 0)
    {
        return false;
    }
    update_time = m_states[m_states_used - 1].network_time;
    *state = m_states[m_states_used - 1];
    return true;
}

// <multi_mover_state> 0x0096A8F0 (:193): the newest state moved by its linear velocity, at most Max_extrap_time_ms
template<typename T>
void multi_sim_state<T>::extrapolate_state(unsigned int network_time, T* state, bool& exceeded_extrap_time)
{
    *state = m_states[m_states_used - 1];
    unsigned int dt_ms = network_time - m_states[m_states_used - 1].network_time;
    if (dt_ms > Max_extrap_time_ms)
    {
        exceeded_extrap_time = true;
        dt_ms = Max_extrap_time_ms;
    }
    const float dt = mss_ms_to_float(dt_ms) / 1000.0f;
    state->position.x = state->position.x + dt * state->linear_velocity.x;
    state->position.y = state->linear_velocity.y * dt + state->position.y;
    state->position.z = state->position.z + state->linear_velocity.z * dt;
}

// <multi_mover_state> 0x0096A9E0 (:208)
// <multi_turret_state> 0x00969DC0 (:208)
// <multi_automobile_state> 0x00969460 (:208)
// <multi_walker_state> 0x00969920 (:208)
// <multi_item_state> 0x0096A400 (:208)
// <multi_projectile_state> 0x0096AF30 (:208)
// Between prev_index and the next state: the earlier state with the position lerped and the orientation slerped (the
// velocities are the earlier state's).
template<typename T>
void multi_sim_state<T>::interpolate_state(unsigned int network_time, int prev_index, T* state)
{
    const T& prev = m_states[prev_index];
    const T& next = m_states[prev_index + 1];
    const float t = mss_ms_to_float(network_time - prev.network_time) / mss_ms_to_float(next.network_time - prev.network_time);
    *state = prev;
    state->position.x = prev.position.x + (next.position.x - prev.position.x) * t;
    state->position.y = prev.position.y + (next.position.y - prev.position.y) * t;
    state->position.z = prev.position.z + (next.position.z - prev.position.z) * t;
    state->angular_velocity = prev.angular_velocity;
    const quaternion q_prev(prev.orientation);
    const quaternion q_next(next.orientation);
    quaternion q;
    pltind_slerp(q, q_prev, q_next, t);
    q.extract_matrix(state->orientation);
}

// <multi_mover_state> 0x0096AC50 (:240): no correction timer or an elapsed one
template<typename T>
bool multi_sim_state<T>::should_apply_correction() const
{
    if (m_apply_correction_timestamp.valid() && !m_apply_correction_timestamp.elapsed())
    {
        return false;
    }
    return true;
}

// <multi_mover_state> 0x0096AC80 (:245)
template<typename T>
void multi_sim_state<T>::set_correction_timer(int delta_time_ms)
{
    m_apply_correction_timestamp.set(delta_time_ms);
}

// 0x0097AF00
multi_player_state::multi_player_state()
{
}

// ---- multi_player_state: the position is not extrapolated; the knockdown time counts down with the network time

// 0x00968CB0 (:152)
template<>
bool multi_sim_state<multi_player_state>::obtain_state(unsigned int network_time, multi_player_state* state, bool& exceeded_extrap_time)
{
    exceeded_extrap_time = false;
    if (m_states_used == 0)
    {
        return false;
    }
    const int newest = m_states_used - 1;
    for (int prev_index = newest; prev_index >= 0; prev_index--)
    {
        if (m_states[prev_index].network_time <= network_time)
        {
            if (prev_index != newest)
            {
                interpolate_state(network_time, prev_index, state);
                return true;
            }
            extrapolate_state(network_time, state, exceeded_extrap_time); // inlined
            return true;
        }
    }
    *state = m_states[newest];
    return true;
}

// 0x00963E80 (:307)
template<>
void multi_sim_state<multi_player_state>::extrapolate_state(unsigned int network_time, multi_player_state* state, bool& exceeded_extrap_time)
{
    const multi_player_state& newest = m_states[m_states_used - 1];
    *state = newest;
    if (network_time - newest.network_time > Max_extrap_time_ms)
    {
        exceeded_extrap_time = true;
    }
    state->knockdown_timestamp += (int)(newest.network_time - network_time);
    if (state->knockdown_timestamp < 0)
    {
        state->knockdown_timestamp = 0;
    }
}

// 0x00963BE0 (:268): the position and the linear velocity are lerped; the orientation is slerped only between states
// with the same ragdoll state
template<>
void multi_sim_state<multi_player_state>::interpolate_state(unsigned int network_time, int prev_index, multi_player_state* state)
{
    const multi_player_state& prev = m_states[prev_index];
    const multi_player_state& next = m_states[prev_index + 1];
    const float t = mss_ms_to_float(network_time - prev.network_time) / mss_ms_to_float(next.network_time - prev.network_time);
    *state = prev;
    state->position.x = prev.position.x + (next.position.x - prev.position.x) * t;
    state->position.y = prev.position.y + (next.position.y - prev.position.y) * t;
    state->position.z = prev.position.z + (next.position.z - prev.position.z) * t;
    state->linear_velocity.x = prev.linear_velocity.x + (next.linear_velocity.x - prev.linear_velocity.x) * t;
    state->linear_velocity.y = prev.linear_velocity.y + (next.linear_velocity.y - prev.linear_velocity.y) * t;
    state->linear_velocity.z = prev.linear_velocity.z + (next.linear_velocity.z - prev.linear_velocity.z) * t;
    if (prev.ragdoll_state == next.ragdoll_state)
    {
        const quaternion q_prev(prev.orientation);
        const quaternion q_next(next.orientation);
        quaternion q;
        pltind_slerp(q, q_prev, q_next, t);
        q.extract_matrix(state->orientation);
    }
    state->knockdown_timestamp += (int)(prev.network_time - network_time);
    if (state->knockdown_timestamp < 0)
    {
        state->knockdown_timestamp = 0;
    }
}

// 0x0097AEE0
multi_turret_state::multi_turret_state()
{
}

// objects/turret.cpp (another translation unit) reads a client's turret state: 0x00989AC0, and 0x00969DC0 below it.
template bool multi_sim_state<multi_turret_state>::obtain_state(unsigned int network_time, multi_turret_state* state, bool& exceeded_extrap_time);

// 0x0097AEC0
multi_automobile_state::multi_automobile_state()
{
}

// 0x0097AED0
multi_walker_state::multi_walker_state()
{
}

// vehicle/automobile.cpp and vehicle/walker.cpp (other translation units) read a remote driver's state; the generic
// bodies are rfg.exe's (decompiles of the instances compared): <multi_automobile_state> 0x00989800 and 0x00969460
// below it, <multi_walker_state> 0x00989960 and 0x00969920 below it.
template bool multi_sim_state<multi_automobile_state>::obtain_state(unsigned int network_time, multi_automobile_state* state, bool& exceeded_extrap_time);
template bool multi_sim_state<multi_walker_state>::obtain_state(unsigned int network_time, multi_walker_state* state, bool& exceeded_extrap_time);
