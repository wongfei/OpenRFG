// Original: volition/rfg/code/multi/multi_game/multi_game_object_backpack.cpp
//
// Decompiled whole (multiplayer.md MP10). The multiplayer backpacks: items (object_backpack, OT_SUB_ITEM_MULTI_BACKPACK)
// read from mp_backpacks.xtbl at boot (up to 16 infos by unique id, each with passive and active abilities), worn by a
// player (human_wear_prop). An activatable backpack ramps up (the server delays it by the client's ping), then
// activates: fuel is spent per second and regenerated while off, and the active abilities apply until it runs dry or is
// deactivated. The abilities are table driven: jump boost (thrust), shockwave (an explosion), ripple (tremor: a radial
// push, damage and knockdowns), health regen (heal), repair, charge (fleetfoot), rhino (a spherecast ahead of the
// charging player cuts buildings and knocks players down), stealth, vision, kaboom (an explosion on activation or on
// death). The jetpack is the player's own code reading the fuel. The server owns pickups, drops, deaths with a backpack,
// activation toggles and the rhino and ripple hits; it sends them as the PT_*BACKPACK* packets (0x6D-0x76).
// Effects, sounds, material effects and camera effects are played as in rfg.exe; outside the renderer they are stand-ins.
// Part of the multi unity TU (rfg_multi_unity.obj, every multi/ .cpp in path order): the file statics are shared
// by the whole tree, and a shared one is placed in its first user (tools/hdrgen/specs/mp0_stubs.py).

#include "volition/rfg/code/multi/multi_game/multi_game_object_backpack.h"

#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/audio_lib/code/audiolib_att_loop_rel.h"
#include "volition/rfg/code/camera/camera_shake.h"
#include "volition/rfg/code/camera/camera_view.h"
#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/rfg/code/code_effects/explosion.h"
#include "volition/rfg/code/code_effects/explosion_shared.h"
#include "volition/rfg/code/code_effects/material_effects.h"
#include "volition/rfg/code/code_effects/material_effects_shared.h"
#include "volition/rfg/code/code_effects/xray_effect.h"
#include "volition/rfg/code/control/control_gameplay.h"
#include "volition/rfg/code/control/control_map.h"
#include "volition/rfg/code/destruction/rfg_mover.h"
#include "volition/rfg/code/destruction/rfg_shape_cutter.h"
#include "volition/rfg/code/effects/effects_shared.h"
#include "volition/rfg/code/effects/object_effect.h"
#include "volition/rfg/code/destruction/rfg_dynamics.h"
#include "volition/rfg/code/event_logging/definitions_events/events.h"
#include "volition/rfg/code/event_logging/gamelog.h"
#include "volition/rfg/code/gameplay/upgrades.h"
#include "volition/rfg/code/gameplay/wrecking_crew/wrecking_crew.h"
#include "volition/rfg/code/gamewide/downloadable_content.h"
#include "volition/rfg/code/gamewide/game_render.h"
#include "volition/rfg/code/geom/anim.h"
#include "volition/rfg/code/geom/anim_enums.h"
#include "volition/rfg/code/geom/character_instance.h"
#include "volition/rfg/code/geom/vanim_shared.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/havok/havok_raycast.h"
#include "volition/rfg/code/havok/havok_ragdoll.h"
#include "volition/rfg/code/interface/localization/localization_shared.h"
#include "volition/rfg/code/interface/ui_document_hud_multiplayer.h"
#include "volition/rfg/code/level/rfg_prop_block_shared.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/level/world.inl"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_core/include/multi_connection.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_io.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_objects.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_qos.h"
#include "volition/rfg/code/multi/multi_core/include/multi_pool.inl"
#include "volition/rfg/code/multi/multi_core/include/multi_session.h"
#include "volition/rfg/code/multi/multi_game/modes/multi_game_mode_common.h"
#include "volition/rfg/code/multi/multi_game/multi_game.h"
#include "volition/rfg/code/multi/multi_game/multi_game_backpack_rack.h"
#include "volition/rfg/code/multi/multi_game/multi_game_character.h"
#include "volition/rfg/code/multi/multi_game/multi_game_data.h"
#include "volition/rfg/code/multi/multi_game/multi_game_object_marker.h"
#include "volition/rfg/code/multi/multi_game/multi_game_rubberbanding.h"
#include "volition/rfg/code/multi/multi_game/multi_game_settings.h"
#include "volition/rfg/code/multi/multi_game/multi_game_xp.h"
#include "volition/rfg/code/multi/multi_game/multi_gameplay_mode.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_ragdoll.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/human_weapons.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/humans/player_cover.h"
#include "volition/rfg/code/objects/inventory_item.h"
#include "volition/rfg/code/objects/movers/object_mover.h"
#include "volition/rfg/code/objects/object_item.h"
#include "volition/rfg/code/objects/object_prop_names_shared.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/render_lib/code/rl_renderable_instance.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/math/spline.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/memory/mempool_base.h"
#include "volition/vlib/code/memory/object_pool.h"
#include "volition/vlib/code/os/frametime.h"
#include "volition/vlib/code/os/vconsole.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/stream2/code/stream2.h"
#include "volition/vlib/code/util/bit_util.h"
#include "volition/vlib/code/util/string_pool.h"
#include "volition/vlogger_lib/include/logger.h"

#include <math.h>
#include <string.h>

static const int MULTI_OBJECT_BACKPACK_POOL_SIZE = 76;
static const int MULTI_OBJECT_BACKPACK_POOL_USAGE = 0x1F5B0; // memory footprint registered with the instantiator
static const int BACKPACK_STRING_POOL_SIZE = 0x400;
// The stream2 container type of the backpacks preloaded for single player (stream2_container_def::container_type).
static const unsigned char BACKPACK_PRELOAD_CONTAINER_TYPE = 0x17;
// The event logger's backpack equip event (log_record_event).
static const unsigned char LOG_EVENT_BACKPACK_EQUIPPED = 0x1D;

// ---- file-static globals
// A client's fuel is short of the server's by up to its latency's worth: the server lets a client activate with this
// much less (multi_object_backpack::has_enough_fuel_to_activate).
static const float SERVER_FUEL_FUDGE_FACTOR = 0.1f; // 0x01309DE0
// multiplayer_mode Multi_gameplay_mode (0x0167DEE0) is used here too; placed in multi_gameplay_mode.cpp.
// human_voice_persona_handle Multi_general_announcer (0x0167DEE4) is used here too; placed in multi_game_mode_bagman.cpp.
static unsigned char Rhino_shape_cutter_explosion = 0xFF; // 0x0167F014, the mp_rhino_cutter explosion's unique id
static float Vision_vfx_blur = 0.6f; // 0x0167F018
static float Vision_vfx_noise_scale = 10.0f; // 0x0167F01C
static float Vision_vfx_noise_frequency = 240.0f; // 0x0167F020
static float Jetpack_charge_rate = 0.1f; // 0x0167F024, single player
static float Backpack_arc_horizontal_offset = 0.25f; // 0x0167F028
static float Backpack_arc_vertical_offset = 0.08f; // 0x0167F02C
static float Backpack_arc_rotation = 4.0f; // 0x0167F030
static float Backpack_arc_mid_vertical_offset = 2.0f; // 0x0167F034
static float Backpack_arc_back_multiplier = 0.2f; // 0x0167F038
static float s_rippleVehicleDamageScale = 4.0f; // 0x0167F03C
static float s_rippleVehicleKnockbackScale = 0.5f; // 0x0167F040
// dynamic_uint_hash_table<remote_object*> Game_handle_to_rop_hash (0x01691B40) is used here too; placed in multi_core_delta_compression.cpp.
// dynamic_uint_hash_table<remote_object*> Network_handle_to_rop_hash (0x01691B88) is used here too; placed in multi_core_delta_compression.cpp.
// dynamic_uint_hash_table<network_game_handle*> Reserved_game_to_network_hash (0x01691C18) is used here too; placed in multi_core_objects.cpp.
// dynamic_uint_hash_table<network_game_handle*> Reserved_network_to_game_hash (0x01691C60) is used here too; placed in multi_core_delta_compression.cpp.
static multi_pool<multi_backpack_info,16> Backpack_info_pool; // 0x01692DC8
// object_pool<district,12,1> districtpool (0x02229C40) is used here too; placed in object_district.cpp.
// multi_backpack_info* Backpack_info[16] (0x02FE8D18) is used here too; placed in multi_game_backpack_rack.cpp.
// bool Server (0x02FEB590) is used here too; placed in multi_core_session_generic.cpp.
// mp_game_data_wrapper* Mp_game_data (0x02FEB698) is used here too; placed in multi_connection.cpp.
static int Num_backpack_types; // 0x02FEB6CC
// Original: the three fuel effects are never set (no writer in rfg.exe), so the fuel effect plays a null effect.
static const effect_info* Backpack_fuel_effect_charging; // 0x02FEB6D0
static const effect_info* Backpack_fuel_effect_charged; // 0x02FEB6D4
static const effect_info* Backpack_fuel_effect_active; // 0x02FEB6D8
// Original: never set either (the arc's spline tension stays 0).
static float Backpack_arc_tension; // 0x02FEB6DC
// object_pool<multi_object_backpack,76,1> multi_object_backpackpool (0x02FEDC90) is used here too; placed in multi_game_mode_king_of_the_hill.cpp.
// multi_xp_event Multi_xp_events[84] (0x0300D240) is used here too; placed in multi_game_mode_bagman.cpp.
static obj_class_instantiator multi_object_backpack_instantiator("multi_object_backpack", alloc_multi_object_backpack, types_multi_object_backpack, bpo_index_multi_object_backpack, classname_multi_object_backpack,
                                                                 multi_object_backpack::pool_remaining, MULTI_OBJECT_BACKPACK_POOL_USAGE, MULTI_OBJECT_BACKPACK_POOL_SIZE); // 0x0300F850 (0x010710C0)
static string_pool Backpack_string_pool(nullptr); // 0x0300F858
// unsigned int $S6 (0x03017C3C): the guard of process's function-static repair timer.

// ---- port helpers
static void backpack_write_float(unsigned char* packet_data, unsigned short& packet_size, float value)
{
    const float swapped = PACKET_SWAP_FLOAT(value);
    memcpy(packet_data + packet_size, &swapped, 4);
    packet_size += 4;
}

static float backpack_read_float(const unsigned char* packet_data, int packet_offset)
{
    float value;
    memcpy(&value, packet_data + packet_offset, 4);
    return PACKET_SWAP_FLOAT(value);
}

// The object of a network handle (the reserved handles first, then the remote objects), null when unknown.
static object* backpack_object_from_network_handle(unsigned short net_handle)
{
    if (net_handle == 0xFFFF)
    {
        return nullptr;
    }
    const unsigned int game_handle = multi_core_game_handle_from_network_handle(net_handle, false);
    return World.object_from_handle(game_handle);
}

static player* backpack_player_from_network_handle(unsigned short net_handle)
{
    object* objp = backpack_object_from_network_handle(net_handle);
    if (objp == nullptr || objp->get_obj_type() != OT_HUMAN || objp->get_sub_type() != OT_SUB_HUMAN_PLAYER)
    {
        return nullptr;
    }
    return (player*)objp;
}

// ---- internal functions
// 0x00957150 (:287). LTCG: pp in EAX.
static bool is_player_backpack_immune_to_ragdoll(player* pp)
{
    return pp != nullptr && pp->backpack != nullptr && pp->backpack->get_backpack_abilities()->rhino;
}

// 0x00957190 (:304): the jump boost's push: the horizontal speed scaled, upwards (at least 2 m/s up first) or down
// (the downthrust). LTCG: pp in ESI.
static void apply_jump_boost(player* pp)
{
    if (pp == nullptr || pp->backpack == nullptr)
    {
        return;
    }
    vector vel;
    human_controller_get_vel(pp, vel);
    const multi_object_backpack* backpack = pp->backpack;
    vel.x = backpack->get_jump_boost_horizontal() * vel.x;
    vel.z = backpack->get_jump_boost_horizontal() * vel.z;
    if (backpack->get_jump_boost_direction_up())
    {
        // Magic number: at least 2 m/s upwards before the boost.
        if (vel.y - 2.0f < 0.0f)
        {
            vel.y = 2.0f;
        }
        vel.y = backpack->get_jump_boost_vertical() + vel.y;
    }
    else
    {
        vel.y = vel.y - backpack->get_jump_boost_vertical();
    }
    human_controller_set_vel(pp, vel);
    if (pp->cur_state == HS_STANDARD)
    {
        human_set_movement_mode(pp, HMM_JUMP, HMSM_JUMP_PRE);
        human_set_state(pp, HS_JUMP, HMS_CURRENT, pp->move_speed);
    }
}

// The rhino's spherecast filter data (on process's stack).
struct rhino_cast_info
{
    player* hitter; // 0x00
    bool hit_something; // 0x04
    bool nearly_spent; // 0x05: the fuel is about to run out: only buildings are hit
};

// 0x0097C170 (:342): what the rhino's spherecast stops at: shape cutters and buildings (not invulnerable ones), hostile
// (or with friendly fire any) living players it has not hit this run, npcs and vehicles it has not hit. Every accepted
// hit is recorded in the info.
static bool rhino_cast_filter_callback(object* objp, void* info)
{
    if (info == nullptr)
    {
        return false;
    }
    rhino_cast_info* cast = (rhino_cast_info*)info;
    const multi_object_backpack* backpack = cast->hitter->backpack;
    bool accept = true;
    if (objp->get_obj_type() == OT_SHAPE_CUTTER)
    {
        cast->hit_something = true;
        return true;
    }
    if (objp->get_obj_type() == OT_MOVER)
    {
        if (objp->get_sub_type() == OT_SUB_MOVER_RFG && ((rfg_mover*)objp)->flags.invulnerable)
        {
            return false;
        }
        cast->hit_something = true;
        return true;
    }
    if (objp->get_obj_type() == OT_HUMAN && objp->get_sub_type() == OT_SUB_HUMAN_PLAYER && objp != cast->hitter && !objp->is_killed())
    {
        player* hit_player = World.object_cast<player>(objp);
        if (hit_player == nullptr || cast->nearly_spent)
        {
            return false;
        }
        if (!human_teams_are_enemies(cast->hitter, hit_player) && !Mp_server_options.generic_options.friendly_fire)
        {
            return false;
        }
        if (hit_player->hflags.invulnerable)
        {
            return false;
        }
        if (hit_player->mp_data == 0xFFFFFFFF)
        {
            return false;
        }
        accept = !backpack->rhino_hit_players[hit_player->mp_data & 0xFFFF];
    }
    else if (objp->get_obj_type() == OT_HUMAN && objp->get_sub_type() == OT_SUB_HUMAN_NPC && !objp->is_killed())
    {
        if (cast->nearly_spent)
        {
            return false;
        }
        for (unsigned int i = 0; i < backpack->num_rhino_hit_npcs; i++)
        {
            if (backpack->rhino_hit_npcs[i] == objp->get_handle())
            {
                accept = false;
            }
        }
    }
    else
    {
        if (objp->get_obj_type() != OT_VEHICLE || objp->get_sub_type() != OT_SUB_VEHICLE_WALKER || objp->is_killed())
        {
            return false;
        }
        if (cast->nearly_spent)
        {
            return false;
        }
        // Original: the vehicles are looked up in the npcs' list (both are recorded there).
        for (unsigned int i = 0; i < backpack->num_rhino_hit_npcs; i++)
        {
            if (backpack->rhino_hit_npcs[i] == objp->get_handle())
            {
                accept = false;
            }
        }
    }
    if (!accept)
    {
        return false;
    }
    cast->hit_something = true;
    return true;
}

// 0x009C6C80 (:429): the rhino ran into a player: the server sends PT_RHINO_PLAYER_HIT (the clients play it) and does
// the damage; the hit one is ragdolled with the velocity (or, without a ragdoll slot, pushed when it gets one). LTCG:
// hitee in ECX.
static void rhino_process_player_hit(player* hitter, player* hitee, const vector& velocity)
{
    if (hitter == nullptr || hitee == nullptr || hitter->backpack == nullptr)
    {
        return;
    }
    if (human_teams_get_disposition(hitter, hitee) != TEAM_DISPOSITION_HOSTILE && !Mp_server_options.generic_options.friendly_fire)
    {
        return;
    }
    const multi_backpack_info::abilities* abilities = hitter->backpack->get_backpack_abilities();
    if (!Multiplayer || Server)
    {
        if (Multiplayer && Server)
        {
            unsigned char packet[1200];
            packet[0] = PT_RHINO_PLAYER_HIT;
            unsigned short packet_size = packet_header_size(PT_RHINO_PLAYER_HIT);
            write_u16_be(packet, packet_size, multi_core_network_handle_from_game_handle(hitter->get_handle(), false));
            write_u16_be(packet, packet_size, multi_core_network_handle_from_game_handle(hitee->get_handle(), false));
            backpack_write_float(packet, packet_size, velocity.x);
            backpack_write_float(packet, packet_size, velocity.y);
            backpack_write_float(packet, packet_size, velocity.z);
            packet_set_size(packet, packet_size);
            multi_io_send_to_all_reliable(multi_session_get_by_type(ST_GAME), packet, packet_size);
        }
        human_damage(hitee, hitter, abilities->rhino_player_hit_damage, DBT_NONE, abilities->w_info, nullptr, true, false);
    }
    if (!is_player_backpack_immune_to_ragdoll(hitee))
    {
        if (human_ragdoll_start(hitee, false, 0xFFFFFFFF))
        {
            havok_ragdoll_set_velocity(hitee->ragdoll_idx, velocity);
        }
        else
        {
            hitee->mp_ragdoll_force = velocity;
            hitee->mp_ragdoll_pos = hitee->pos;
            // Magic number: the push is applied 1 m above the feet, within 2 s.
            hitee->mp_ragdoll_pos.y = hitee->mp_ragdoll_pos.y + 1.0f;
            hitee->mp_ragdoll_force_timeout.set(2000);
        }
        if (abilities->rhino_player_hit_effect != nullptr)
        {
            effects_play(abilities->rhino_player_hit_effect, hitee->pos, hitter->orient, false, 0xFFFFFFFF, -1, 0xFFFFFFFF, 1.0f, false, EFFECTS_CATEGORY_NONE);
        }
    }
    if (object_is_local_player(hitee))
    {
        if (abilities->rhino_local_player_camera_shake != nullptr)
        {
            camera_shake_play(abilities->rhino_local_player_camera_shake, 1.0f, false);
        }
        if (abilities->rhino_local_player_hit_radial_blur != nullptr)
        {
            effects_play_radial_blur(abilities->rhino_local_player_hit_radial_blur, hitee->pos);
        }
    }
}

// 0x0097C320 (:502): single player only (not in Wrecking Crew or DLC): the rhino ran into a vehicle. LTCG: hitee in EAX.
static void rhino_process_vehicle_hit(player* hitter, vehicle* hitee, const vector& velocity, raycast_output* raycast_hit)
{
    if (Multiplayer || wrecking_crew_is_active() || dlc_get_current_id() != -1 || hitter == nullptr || hitee == nullptr || hitter->backpack == nullptr)
    {
        return;
    }
    const multi_backpack_info::abilities* abilities = hitter->backpack->get_backpack_abilities();
    const raycast_output::hit_info& hit = raycast_hit->m_hits[0];
    // Magic number: four times the player damage (the int is truncated before).
    vehicle_apply_damage(hitee, hitter, nullptr, DBT_MELEE, &hit.hit_pos, &hit.hit_normal, 1.0f, (int)abilities->rhino_player_hit_damage * 4, &velocity, false);
    vehicle_apply_point_velocity(hitee, &velocity, &raycast_hit->m_hits[0].hit_pos);
    if (object_is_local_player(hitee))
    {
        if (abilities->rhino_local_player_camera_shake != nullptr)
        {
            camera_shake_play(abilities->rhino_local_player_camera_shake, 1.0f, false);
        }
        if (abilities->rhino_local_player_hit_radial_blur != nullptr)
        {
            effects_play_radial_blur(abilities->rhino_local_player_hit_radial_blur, hitee->pos);
        }
    }
}

// 0x009572B0 (:527): single player only: the rhino ran into an npc. LTCG: hitee in EAX.
static void rhino_process_npc_hit(player* hitter, human* hitee, const vector& velocity)
{
    if (Multiplayer || wrecking_crew_is_active() || dlc_get_current_id() != -1 || hitter == nullptr || hitee == nullptr || hitter->backpack == nullptr)
    {
        return;
    }
    const multi_backpack_info::abilities* abilities = hitter->backpack->get_backpack_abilities();
    human_damage(hitee, hitter, abilities->rhino_player_hit_damage, DBT_NONE, abilities->w_info, nullptr, true, false);
    if (human_ragdoll_start(hitee, false, 0xFFFFFFFF))
    {
        havok_ragdoll_set_velocity(hitee->ragdoll_idx, velocity);
    }
    if (abilities->rhino_player_hit_effect != nullptr)
    {
        effects_play(abilities->rhino_player_hit_effect, hitee->pos, hitter->orient, false, 0xFFFFFFFF, -1, 0xFFFFFFFF, 1.0f, false, EFFECTS_CATEGORY_NONE);
    }
    if (object_is_local_player(hitee))
    {
        if (abilities->rhino_local_player_camera_shake != nullptr)
        {
            camera_shake_play(abilities->rhino_local_player_camera_shake, 1.0f, false);
        }
        if (abilities->rhino_local_player_hit_radial_blur != nullptr)
        {
            effects_play_radial_blur(abilities->rhino_local_player_hit_radial_blur, hitee->pos);
        }
    }
}

// 0x009F14C0 (:560): the ripple hit players (every peer: the server first, then PT_RIPPLE_PLAYERS_HIT): each is
// ragdolled with its push (or pushed when it gets a slot) unless a rhino; the server does the damage. Knocking down
// enough enemies is the group knockdown experience event.
static void ripple_process_players_hit(player* attacker, const ripple_hit_info* ripple_hit_players, int num_players_hit)
{
    if (attacker == nullptr || attacker->backpack == nullptr)
    {
        return;
    }
    // Magic number: the hit count is sent in one byte.
    if (num_players_hit > 254)
    {
        num_players_hit = 255;
    }
    int num_enemies_knocked_down = 0;
    for (int i = 0; i < num_players_hit; i++)
    {
        player* pp = (player*)ripple_hit_players[i].victim;
        if (attacker->backpack == nullptr || pp == nullptr)
        {
            return;
        }
        const multi_backpack_info::abilities* abilities = attacker->backpack->get_backpack_abilities();
        const vector& velocity = ripple_hit_players[i].velocity;
        if (!is_player_backpack_immune_to_ragdoll(pp))
        {
            if (human_ragdoll_start(pp, false, 0xFFFFFFFF))
            {
                havok_ragdoll_apply_point_velocity(pp->ragdoll_idx, velocity, attacker->backpack->pos);
            }
            else
            {
                pp->mp_ragdoll_force = velocity;
                pp->mp_ragdoll_pos = attacker->backpack->pos;
                // Magic number: within 2 s.
                pp->mp_ragdoll_force_timeout.set(2000);
            }
            if (human_teams_get_disposition(attacker, pp) == TEAM_DISPOSITION_HOSTILE)
            {
                num_enemies_knocked_down++;
            }
        }
        if (!Multiplayer || Server)
        {
            human_damage(pp, attacker, abilities->ripple_player_damage, DBT_NONE, abilities->w_info, nullptr, true, false);
        }
        if (object_is_local_player(pp) && abilities->ripple_local_player_camera_shake != nullptr)
        {
            camera_shake_play(abilities->ripple_local_player_camera_shake, 1.0f, false);
        }
    }
    // The experience event is inlined in rfg.exe (its repeat timer, the trigger and the server's PT_XP); the
    // experience system is outside multiplayer.md, so the public trigger stands for it.
    if ((int)multi_game_xp_get_event_parameter(MXE_GROUP_KNOCKDOWN) <= num_enemies_knocked_down && attacker->mp_data != 0xFFFFFFFF)
    {
        multi_game_xp_trigger_event(attacker->mp_data, MXE_GROUP_KNOCKDOWN, 1, 0xFF, -1, false);
    }
    if (!Multiplayer || !Server || num_players_hit < 1)
    {
        return;
    }
    unsigned char packet[1200];
    packet[0] = PT_RIPPLE_PLAYERS_HIT;
    unsigned short packet_size = packet_header_size(PT_RIPPLE_PLAYERS_HIT);
    write_u16_be(packet, packet_size, multi_core_network_handle_from_game_handle(attacker->get_handle(), false));
    packet[packet_size++] = (unsigned char)num_players_hit;
    for (int i = 0; i < num_players_hit; i++)
    {
        write_u16_be(packet, packet_size, multi_core_network_handle_from_game_handle(ripple_hit_players[i].victim->get_handle(), false));
        backpack_write_float(packet, packet_size, ripple_hit_players[i].velocity.x);
        backpack_write_float(packet, packet_size, ripple_hit_players[i].velocity.y);
        backpack_write_float(packet, packet_size, ripple_hit_players[i].velocity.z);
    }
    packet_set_size(packet, packet_size);
    multi_io_send_to_all_reliable(Game_session, packet, packet_size);
}

// 0x009573D0 (:636): single player: the ripple hit npcs (ragdolled with the push around the backpack, damaged)
static void ripple_process_npcs_hit(player* attacker, const ripple_hit_info* ripple_hit_npcs, int num_npcs_hit)
{
    if (attacker == nullptr || attacker->backpack == nullptr)
    {
        return;
    }
    for (int i = 0; i < num_npcs_hit; i++)
    {
        human* hp = (human*)ripple_hit_npcs[i].victim;
        if (attacker->backpack == nullptr || hp == nullptr)
        {
            return;
        }
        const multi_backpack_info::abilities* abilities = attacker->backpack->get_backpack_abilities();
        if (human_ragdoll_start(hp, false, 0xFFFFFFFF))
        {
            havok_ragdoll_apply_point_velocity(hp->ragdoll_idx, ripple_hit_npcs[i].velocity, attacker->backpack->pos);
        }
        human_damage(hp, attacker, abilities->ripple_player_damage, DBT_NONE, abilities->w_info, nullptr, true, false);
    }
}

// 0x009574B0 (:665): single player: the ripple hit walkers (melee damage scaled, half the push). LTCG:
// ripple_hit_vehicles in EAX.
static void ripple_process_vehicles_hit(player* attacker, const ripple_hit_info* ripple_hit_vehicles, int num_vehicles_hit)
{
    if (attacker == nullptr || attacker->backpack == nullptr)
    {
        return;
    }
    for (int i = 0; i < num_vehicles_hit; i++)
    {
        vehicle* vp = (vehicle*)ripple_hit_vehicles[i].victim;
        if (attacker->backpack == nullptr || vp == nullptr)
        {
            return;
        }
        const multi_backpack_info::abilities* abilities = attacker->backpack->get_backpack_abilities();
        const vector up = UP_VECTOR;
        const vector& velocity = ripple_hit_vehicles[i].velocity;
        vehicle_apply_damage(vp, attacker, nullptr, DBT_MELEE, &vp->pos, &up, 1.0f, (int)(abilities->ripple_player_damage * s_rippleVehicleDamageScale), &velocity, false);
        const vector knockback = velocity * s_rippleVehicleKnockbackScale;
        vehicle_apply_point_velocity(vp, &knockback, &vp->pos);
    }
}

// 0x00957610 (:692): the failed activation sound (not for ok, unactivatable, ramping up or ragdolled). LTCG: reason in
// EAX.
static void display_backpack_toggle_reason(multi_object_backpack* backpack, multi_backpack_toggle_reasons reason)
{
    if (reason == MBTR_OK || reason == MBTR_NON_ACTIVATABLE || reason == MBTR_RAMPING_UP || reason == MBTR_RAGDOLLED)
    {
        return;
    }
    const int cue_id = backpack->get_backpack_info()->failed_activation_foley;
    if (cue_id != -1)
    {
        audiolib_cue_play(cue_id, nullptr, nullptr);
    }
}

// 0x00957650 (:702). LTCG: attribute_name in EAX, root in ECX.
static const char* parse_backpack_string(xml_element* root, const char* attribute_name)
{
    const char* value = xml_table_get_opt_string_ref(root, attribute_name);
    if (value == nullptr)
    {
        return nullptr;
    }
    return Backpack_string_pool.add_unique(value);
}

static const effect_info* read_opt_effect(xml_element* root, const char* attribute_name)
{
    const char* name = xml_table_get_opt_string_ref(root, attribute_name);
    return name != nullptr ? effects_get_effect_info(name) : nullptr;
}

// 0x00957680 (:714): one ability set. LTCG: abilities in EAX, root in EDI.
static void read_backpack_abilities(xml_element* root, multi_backpack_info::abilities* abilities)
{
    abilities->w_info = nullptr;
    const char* weapon_name = xml_table_get_opt_string_ref(root, "weapon");
    if (weapon_name != nullptr)
    {
        abilities->w_info = weapons_get_weapon_info(weapon_name);
    }
    xml_table_get_req_float(abilities->movement_speed_factor, root, "movement_speed_factor");
    xml_table_get_req_float(abilities->damage_taken_factor, root, "damage_taken_factor");
    xml_table_get_req_float(abilities->direct_damage_dealt_factor, root, "direct_damage_dealt_factor");
    xml_table_get_req_float(abilities->area_damage_dealt_factor, root, "area_damage_dealt_factor");
    xml_table_get_req_float(abilities->refire_delay_factor, root, "refire_delay_factor");
    xml_table_get_req_float(abilities->reload_speed_factor, root, "reload_speed_factor");
    xml_table_get_req_bool(abilities->unlimited_ammo, root, "unlimited_ammo");
    xml_table_get_req_float(abilities->fall_speed_safety_factor, root, "fall_speed_safety_factor");
    abilities->additional_bullet_hit_effect_other = read_opt_effect(root, "additional_bullet_hit_effect_other");
    abilities->replacement_bullet_hit_effect_other_player = read_opt_effect(root, "replacement_bullet_hit_effect_other_player");
    abilities->additional_bullet_hit_effect_self = read_opt_effect(root, "additional_bullet_hit_effect_self");
    abilities->replacement_bullet_hit_effect_self = read_opt_effect(root, "replacement_bullet_hit_effect_self");
    abilities->bullet_tracer = read_opt_effect(root, "bullet_tracer");
    abilities->rail_tracer = read_opt_effect(root, "rail_tracer");

    abilities->vision = false;
    abilities->vision_vfx = false;
    abilities->vision_vfx_ramp_in_time_ms = 0;
    abilities->vision_vfx_ramp_out_time_ms = 0;
    xml_element* element = xml_table_find_opt(root, "vision");
    if (element != nullptr)
    {
        abilities->vision = true;
        element = xml_table_find_opt(element, "vision_vfx");
        if (element != nullptr)
        {
            abilities->vision_vfx = true;
            xml_table_get_opt_int(abilities->vision_vfx_ramp_in_time_ms, element, "vision_vfx_ramp_in_time_ms");
            xml_table_get_opt_int(abilities->vision_vfx_ramp_out_time_ms, element, "vision_vfx_ramp_out_time_ms");
        }
    }

    abilities->stealth = false;
    abilities->stealth_time_ms = 0;
    abilities->restealth_time_ms = 0;
    abilities->stealth_transparency = 0.0f;
    abilities->local_stealth_transparency = 0.0f;
    element = xml_table_find_opt(root, "stealth");
    if (element != nullptr)
    {
        abilities->stealth = true;
        xml_table_get_req_float(abilities->stealth_transparency, element, "stealth_transparency");
        xml_table_get_req_float(abilities->local_stealth_transparency, element, "local_stealth_transparency");
        xml_table_get_req_int(abilities->stealth_time_ms, element, "stealth_time_ms");
        xml_table_get_req_int(abilities->restealth_time_ms, element, "restealth_time_ms");
    }

    abilities->jump_boost = false;
    abilities->jump_boost_horizontal = 0.0f;
    abilities->jump_boost_vertical = 0.0f;
    abilities->jump_boost_timer_ms = 0;
    abilities->jump_boost_delay_ms = 0;
    abilities->jump_boost_downthrust_ground_hit_effect = nullptr;
    element = xml_table_find_opt(root, "jump_boost");
    if (element != nullptr)
    {
        abilities->jump_boost = true;
        xml_table_get_req_float(abilities->jump_boost_horizontal, element, "jump_boost_horizontal");
        xml_table_get_req_float(abilities->jump_boost_vertical, element, "jump_boost_vertical");
        xml_table_get_opt_int(abilities->jump_boost_timer_ms, element, "jump_boost_timer_ms");
        xml_table_get_opt_int(abilities->jump_boost_delay_ms, element, "jump_boost_delay_ms");
        abilities->jump_boost_downthrust_ground_hit_effect = read_opt_effect(element, "jump_boost_downthrust_ground_hit_effect");
    }

    abilities->shockwave = false;
    abilities->shockwave_explosion_info = nullptr;
    abilities->shockwave_timer_ms = 0;
    element = xml_table_find_opt(root, "shockwave");
    if (element != nullptr)
    {
        abilities->shockwave = true;
        abilities->shockwave_explosion_info = explosion_lookup(xml_table_get_req_string_ref(element, "shockwave_explosion_name"));
        xml_table_get_req_int(abilities->shockwave_timer_ms, element, "shockwave_timer_ms");
    }

    abilities->ripple = false;
    abilities->ripple_local_player_camera_shake = nullptr;
    abilities->ripple_effect.effect = nullptr;
    abilities->ripple_effect.attach_point_name = nullptr;
    abilities->ripple_timer_ms = 0;
    abilities->ripple_range = 0.0f;
    abilities->ripple_player_hit_speed = 0.0f;
    abilities->ripple_player_minimum_y_velocity = 0.0f;
    abilities->ripple_player_damage = 0.0f;
    // Original: ripple_mover_impulse is not cleared.
    element = xml_table_find_opt(root, "ripple");
    if (element != nullptr)
    {
        abilities->ripple = true;
        const char* shake_name = xml_table_get_opt_string_ref(element, "ripple_local_player_camera_shake");
        if (shake_name != nullptr)
        {
            abilities->ripple_local_player_camera_shake = camera_shake_get(shake_name);
        }
        const char* effect_name = xml_table_get_opt_string_ref(element, "ripple_effect_name");
        if (effect_name != nullptr)
        {
            abilities->ripple_effect.effect = effects_get_effect_info(effect_name);
            abilities->ripple_effect.attach_point_name = parse_backpack_string(element, "ripple_effect_attach_point_name");
        }
        xml_table_get_opt_int(abilities->ripple_timer_ms, element, "ripple_timer_ms");
        xml_table_get_req_float(abilities->ripple_player_hit_speed, element, "ripple_player_hit_speed");
        xml_table_get_req_float(abilities->ripple_range, element, "ripple_range");
        xml_table_get_req_float(abilities->ripple_player_minimum_y_velocity, element, "ripple_player_minimum_y_velocity");
        xml_table_get_req_float(abilities->ripple_player_damage, element, "ripple_player_damage");
        xml_table_get_req_int(abilities->ripple_mover_impulse, element, "ripple_mover_impulse");
    }

    abilities->health_regen = false;
    abilities->health_regen_per_second = 0.0f;
    abilities->health_regen_radius = 0.0f;
    abilities->health_regen_heal_enemies = false;
    element = xml_table_find_opt(root, "health_regen");
    if (element != nullptr)
    {
        abilities->health_regen = true;
        xml_table_get_req_float(abilities->health_regen_per_second, element, "health_regen_per_second");
        xml_table_get_req_float(abilities->health_regen_radius, element, "health_regen_radius");
        xml_table_get_req_bool(abilities->health_regen_heal_enemies, element, "health_regen_heal_enemies");
    }

    abilities->repair = false;
    abilities->repair_offset = 0.0f;
    abilities->repair_radius = 0.0f;
    element = xml_table_find_opt(root, "repair");
    if (element != nullptr)
    {
        abilities->repair = true;
        xml_table_get_req_float(abilities->repair_offset, element, "repair_offset");
        xml_table_get_req_float(abilities->repair_radius, element, "repair_radius");
    }

    abilities->charge = false;
    abilities->charge_top_speed = 0.0f;
    abilities->charge_ramp_up_time_seconds = 0.0f;
    abilities->charge_ramp_down_time_seconds = 0.0f;
    element = xml_table_find_opt(root, "charge");
    if (element != nullptr)
    {
        abilities->charge = true;
        xml_table_get_req_float(abilities->charge_top_speed, element, "charge_top_speed");
        xml_table_get_req_float(abilities->charge_ramp_up_time_seconds, element, "charge_ramp_up_time_seconds");
        xml_table_get_req_float(abilities->charge_ramp_down_time_seconds, element, "charge_ramp_down_time_seconds");
    }

    abilities->rhino = false;
    abilities->rhino_local_player_camera_shake = nullptr;
    abilities->rhino_mover_hit_effect = nullptr;
    abilities->rhino_player_hit_effect = nullptr;
    abilities->rhino_local_player_hit_radial_blur = nullptr;
    abilities->rhino_player_hit_damage = 0.0f;
    abilities->rhino_player_hit_speed = 0.0f;
    abilities->rhino_player_minimum_y_velocity = 0.0f;
    element = xml_table_find_opt(root, "rhino");
    if (element != nullptr)
    {
        abilities->rhino = true;
        const char* shake_name = xml_table_get_opt_string_ref(element, "rhino_local_player_camera_shake");
        if (shake_name != nullptr)
        {
            abilities->rhino_local_player_camera_shake = camera_shake_get(shake_name);
        }
        abilities->rhino_mover_hit_effect = read_opt_effect(element, "rhino_mover_hit_effect");
        abilities->rhino_player_hit_effect = read_opt_effect(element, "rhino_player_hit_effect");
        const char* blur_name = xml_table_get_opt_string_ref(element, "rhino_local_player_hit_radial_blur");
        if (blur_name != nullptr)
        {
            abilities->rhino_local_player_hit_radial_blur = effects_get_radial_blur_info(blur_name);
        }
        xml_table_get_req_float(abilities->rhino_player_hit_damage, element, "rhino_player_hit_damage");
        xml_table_get_req_float(abilities->rhino_player_hit_speed, element, "rhino_player_hit_speed");
        xml_table_get_req_float(abilities->rhino_player_minimum_y_velocity, element, "rhino_player_minimum_y_velocity");
    }

    abilities->ammo_factor = 1.0f;
    xml_table_get_opt_float(abilities->ammo_factor, root, "ammo_factor");
    xml_table_get_req_float(abilities->fov_scale, root, "fov_scale");
}

// 0x00957D50 (:968): one attack/loop/release effect set. LTCG: effects in ESI, root in EDI.
static void read_backpack_effects(xml_element* root, multi_backpack_info::effects_alr::effects* effects)
{
    effects->attack_effect = read_opt_effect(root, "attack_effect");
    effects->loop_effect = read_opt_effect(root, "loop_effect");
    effects->release_effect = read_opt_effect(root, "release_effect");
    effects->attach_point_name = parse_backpack_string(root, "attach_point_name");
}

static void read_backpack_effects_list(xml_element* root, const char* name, multi_backpack_info::effects_alr::effects* effects, int& num_effects)
{
    xml_element* list = xml_table_find_opt(root, name);
    num_effects = 0;
    if (list == nullptr)
    {
        return;
    }
    // Original: the list is not limited to the 8 entries of the arrays (the shipped table has fewer).
    for (xml_element* entry = list->elements; entry != nullptr; entry = entry->next)
    {
        read_backpack_effects(entry, &effects[num_effects]);
        num_effects++;
    }
}

// 0x00957DE0 (:996): the sound and the player, backpack, alternate backpack and weapon effects, and the arc
static void read_backpack_effects_alr(xml_element* root, multi_backpack_info::effects_alr* effects_alr)
{
    effects_alr->sfx_alr = -1;
    const char* sfx_name = xml_table_get_opt_string_ref(root, "sfx_alr");
    if (sfx_name != nullptr)
    {
        audiolib_att_loop_rel_get_cue_id(sfx_name, &effects_alr->sfx_alr);
    }
    // Original: sfx_npc_alr is not set to -1 when the table has none.
    sfx_name = xml_table_get_opt_string_ref(root, "sfx_npc_alr");
    if (sfx_name != nullptr)
    {
        audiolib_att_loop_rel_get_cue_id(sfx_name, &effects_alr->sfx_npc_alr);
    }
    read_backpack_effects_list(root, "player_effects", effects_alr->player_effects, effects_alr->num_player_effects);
    read_backpack_effects_list(root, "backpack_effects", effects_alr->backpack_effects, effects_alr->num_backpack_effects);
    read_backpack_effects_list(root, "alternate_backpack_effects", effects_alr->alternate_backpack_effects, effects_alr->num_alternate_backpack_effects);
    read_backpack_effects_list(root, "weapon_effects", effects_alr->weapon_effects, effects_alr->num_weapon_effects);
    xml_table_get_req_bool(effects_alr->loop_waits_for_attack, root, "loop_waits_for_attack");
    effects_alr->arc_effect = nullptr;
    effects_alr->arc_weapon_attach_point_name = nullptr;
    effects_alr->arc_backpack_attach_point_name = nullptr;
    effects_alr->arc_travel_speed = 0.0f;
    xml_element* arc = xml_table_find_opt(root, "arc_effect");
    if (arc != nullptr)
    {
        const char* effect_name = xml_table_get_req_string_ref(arc, "effect");
        if (effect_name != nullptr)
        {
            effects_alr->arc_effect = effects_get_effect_info(effect_name);
        }
        effects_alr->arc_backpack_attach_point_name = parse_backpack_string(arc, "backpack_attach_point");
        effects_alr->arc_weapon_attach_point_name = parse_backpack_string(arc, "weapon_attach_point");
        xml_table_get_req_float(effects_alr->arc_travel_speed, arc, "travel_speed");
    }
}

// 0x00958030 (:1076). LTCG: root in ESI.
static void parse_ramp_up_effects(xml_element* root, multi_backpack_info::ramp_up_effects* ramp_up)
{
    const char* name = xml_table_get_opt_string_ref(root, "player_effects");
    if (name != nullptr)
    {
        ramp_up->ramp_up_player_effect = effects_get_effect_info(name);
    }
    name = xml_table_get_opt_string_ref(root, "backpack_effects");
    if (name != nullptr)
    {
        ramp_up->ramp_up_backpack_effect = effects_get_effect_info(name);
    }
    ramp_up->ramp_up_effect_attach_point_name = parse_backpack_string(root, "attach_point_name");
    name = xml_table_get_opt_string_ref(root, "camera_shake");
    if (name != nullptr)
    {
        ramp_up->ramp_up_camera_shake = camera_shake_get(name);
    }
}

// 0x009AFFD0 (:1104): the backpack table: each mp_backpacks entry fills the info of its unique id. LTCG: table_name in
// EAX.
static void parse_backpacks_table(const char* table_name)
{
    xml_element* table = xml_table_parse(table_name);
    if (table == nullptr)
    {
        return;
    }
    for (xml_element* root = xml_table_find_opt(table, "mp_backpacks"); root != nullptr; root = xml_table_find_next(table, root, "mp_backpacks"), Num_backpack_types++)
    {
        int unique_id;
        xml_table_get_opt_int(unique_id, root, "Unique_ID");
        // Original: an id out of range is only reported; the entry is still used.
        if (unique_id < 0 || unique_id > 15)
        {
            keen::debug::nativePrintArgs("Invalid backpack unique_id");
        }
        multi_backpack_info* buf = Backpack_info[unique_id];
        if (buf == nullptr)
        {
            buf = Backpack_info_pool.alloc(0);
        }
        Backpack_info[unique_id] = buf;
        buf->unique_id = unique_id;
        xml_table_get_req_string(buf->internal_name, sizeof(buf->internal_name), root, "Name");
        buf->internal_name_crc = checksum_stri(buf->internal_name, 0);
        xml_table_get_req_string(buf->display_name, sizeof(buf->display_name), root, "display_name");
        buf->icon_name = parse_backpack_string(root, "icon_name");
        buf->pickup_tooltip = parse_backpack_string(root, "pickup_tooltip");
        buf->item_infop = object_item_get_info(xml_table_get_req_string_ref(root, "item_name"));
        // Original: hint_text_crc keeps its old value when the entry has no hint.
        const char* hint = xml_table_get_opt_string_ref(root, "hint_text");
        if (hint != nullptr)
        {
            buf->hint_text_crc = localization_get_id_hash(hint);
        }
        read_backpack_effects_alr(xml_table_find_opt(root, "pickup_effects"), &buf->pickup_effects);
        read_backpack_abilities(xml_table_find_req(root, "passive_abilities"), &buf->passive_abilities);
        buf->fuel_use_per_second = 1.0f;
        buf->fuel_regen_per_second = 0.0f;
        buf->fuel_activation_cost = 0.0f;
        buf->activatable = false;
        buf->uses_fuel = false;
        buf->no_fine_aim = false;
        buf->no_cover = false;
        buf->failed_activation_foley = -1;
        buf->ramp_up.ramp_up_player_effect = nullptr;
        buf->ramp_up.ramp_up_backpack_effect = nullptr;
        buf->ramp_up.ramp_up_effect_attach_point_name = nullptr;
        buf->ramp_up.ramp_up_camera_shake = nullptr;
        buf->alternate_ramp_up.ramp_up_player_effect = nullptr;
        buf->alternate_ramp_up.ramp_up_backpack_effect = nullptr;
        buf->alternate_ramp_up.ramp_up_effect_attach_point_name = nullptr;
        buf->alternate_ramp_up.ramp_up_camera_shake = nullptr;
        buf->activation_camera_shake = nullptr;
        xml_element* activatable = xml_table_find_opt(root, "activatable");
        if (activatable == nullptr)
        {
            memset(&buf->active_abilities, 0, sizeof(buf->active_abilities));
        }
        else
        {
            buf->activatable = true;
            xml_element* activation_effects = xml_table_find_opt(activatable, "activation_effects");
            read_backpack_effects_alr(activation_effects, &buf->activation_effects);
            if (activation_effects != nullptr)
            {
                const char* foley = xml_table_get_opt_string_ref(activation_effects, "failed_activation_foley");
                if (foley != nullptr)
                {
                    audiolib_cue_get_id(foley, &buf->failed_activation_foley);
                }
                xml_element* ramp_up = xml_table_find_opt(activation_effects, "ramp_up_effects");
                if (ramp_up != nullptr)
                {
                    parse_ramp_up_effects(ramp_up, &buf->ramp_up);
                }
                ramp_up = xml_table_find_opt(activation_effects, "alternate_ramp_up_effects");
                if (ramp_up != nullptr)
                {
                    parse_ramp_up_effects(ramp_up, &buf->alternate_ramp_up);
                }
                const char* shake_name = xml_table_get_opt_string_ref(activation_effects, "activation_camera_shake");
                if (shake_name != nullptr)
                {
                    buf->activation_camera_shake = camera_shake_get(shake_name);
                }
            }
            xml_table_get_req_bool(buf->no_fine_aim, activatable, "no_fine_aim");
            xml_table_get_req_bool(buf->no_cover, activatable, "no_cover");
            buf->deactivatable = false;
            xml_element* deactivatable = xml_table_find_opt(activatable, "deactivatable");
            if (deactivatable != nullptr)
            {
                buf->deactivatable = true;
                xml_table_get_req_bool(buf->player_deactivatable, deactivatable, "player_deactivatable");
            }
            xml_element* uses_fuel = xml_table_find_opt(activatable, "uses_fuel");
            if (uses_fuel != nullptr)
            {
                buf->uses_fuel = true;
                xml_table_get_req_float(buf->fuel_use_per_second, uses_fuel, "fuel_use_per_second");
                xml_table_get_req_float(buf->fuel_regen_per_second, uses_fuel, "fuel_regen_per_second");
                xml_table_get_req_float(buf->fuel_activation_cost, uses_fuel, "fuel_activation_cost");
                xml_table_get_req_float(buf->minimum_fuel_to_activate, uses_fuel, "minimum_fuel_to_activate");
                buf->fuel_refill_foley = -1;
                const char* foley = xml_table_get_opt_string_ref(uses_fuel, "fuel_refill_foley");
                if (foley != nullptr)
                {
                    audiolib_cue_get_id(foley, &buf->fuel_refill_foley);
                }
                // Magic number: the fuel use is divided by.
                if (buf->fuel_use_per_second < 1e-05f)
                {
                    buf->fuel_use_per_second = 1e-05f;
                }
            }
            xml_element* active = xml_table_find_req(activatable, "active_abilities");
            read_backpack_abilities(active, &buf->active_abilities);
            buf->kaboom = false;
            buf->kaboom_explosion_info = nullptr;
            buf->kaboom_death_chance = 0.0f;
            buf->kaboom_delay_ms = 0;
            buf->kaboom_damages_user = true;
            buf->kaboom_camera_shake = nullptr;
            buf->kaboom_camera_shake_user_intensity = 1.0f;
            buf->kaboom_camera_shake_falloff = 0.0f;
            xml_element* kaboom = xml_table_find_opt(active, "kaboom");
            if (kaboom != nullptr)
            {
                buf->kaboom = true;
                buf->kaboom_explosion_info = explosion_lookup(xml_table_get_req_string_ref(kaboom, "kaboom_explosion_name"));
                xml_table_get_req_float(buf->kaboom_death_chance, kaboom, "kaboom_death_chance");
                xml_table_get_req_int(buf->kaboom_delay_ms, kaboom, "kaboom_delay_ms");
                xml_table_get_req_bool(buf->kaboom_damages_user, kaboom, "kaboom_damages_user");
                xml_element* shake = xml_table_find_opt(kaboom, "kaboom_camera_shake");
                if (shake != nullptr)
                {
                    const char* shake_name = xml_table_get_req_string_ref(shake, "kaboom_camera_shake");
                    if (shake_name != nullptr)
                    {
                        buf->kaboom_camera_shake = camera_shake_get(shake_name);
                    }
                    xml_table_get_req_float(buf->kaboom_camera_shake_user_intensity, shake, "kaboom_camera_shake_user_intensity");
                    // Magic numbers: the intensity in [0, 100], the falloff distance in [1, 10000].
                    if (buf->kaboom_camera_shake_user_intensity < 0.0f)
                    {
                        buf->kaboom_camera_shake_user_intensity = 0.0f;
                    }
                    else if (buf->kaboom_camera_shake_user_intensity > 100.0f)
                    {
                        buf->kaboom_camera_shake_user_intensity = 100.0f;
                    }
                    xml_table_get_req_float(buf->kaboom_camera_shake_falloff, shake, "kaboom_camera_shake_falloff");
                    if (buf->kaboom_camera_shake_falloff < 1.0f)
                    {
                        buf->kaboom_camera_shake_falloff = 1.0f;
                    }
                    else if (buf->kaboom_camera_shake_falloff > 10000.0f)
                    {
                        buf->kaboom_camera_shake_falloff = 10000.0f;
                    }
                    buf->kaboom_camera_shake_ignore_disabled = false;
                    xml_table_get_opt_bool(buf->kaboom_camera_shake_ignore_disabled, shake, "kaboom_camera_shake_ignore_disabled");
                }
            }
            buf->mat_fx_on_backpack = false;
            buf->mat_fx_on_human = false;
            buf->mat_fx_on_weapon = false;
            xml_element* material_effects = xml_table_find_opt(activatable, "material_effects");
            if (material_effects != nullptr)
            {
                xml_table_get_req_bool(buf->mat_fx_on_backpack, material_effects, "on_backpack");
                xml_table_get_req_bool(buf->mat_fx_on_human, material_effects, "on_human");
                xml_table_get_req_bool(buf->mat_fx_on_weapon, material_effects, "on_weapon");
            }
        }
        buf->jetpack = false;
        xml_element* jetpack = xml_table_find_opt(root, "jetpack");
        if (jetpack != nullptr)
        {
            buf->jetpack = true;
            buf->uses_fuel = true;
            xml_table_get_req_float(buf->fuel_use_per_second, jetpack, "fuel_use_per_second");
            xml_table_get_req_float(buf->fuel_regen_per_second, jetpack, "fuel_regen_per_second");
        }
    }
    xml_table_close(table);
}

// The infos returned to the pool, the string pool emptied (parse_base_backpack_tables, unused_dcf_reload_backpack_table,
// multi_game_backpack_system_shutdown).
static void free_backpack_infos()
{
    for (multi_backpack_info* objp = Backpack_info_pool.get_first(0); objp != nullptr; objp = Backpack_info_pool.free(objp))
    {
    }
}

// 0x009DF330 (:1329): the pool and string pool set up (first time) or the file refreshed, then the table read again.
// LTCG: pool in ESI, refresh in AL.
static void parse_base_backpack_tables(mempool_base* pool, bool refresh)
{
    if (!refresh)
    {
        if (pool == nullptr)
        {
            keen::debug::nativePrintArgs("Need a mempool when parsing backpacks table");
            return;
        }
        void* address = pool->alloc(BACKPACK_STRING_POOL_SIZE, 4);
        if (address == nullptr)
        {
            return;
        }
        Backpack_info_pool.init(*pool);
        Backpack_string_pool.init(address, BACKPACK_STRING_POOL_SIZE, nullptr, 0, 0);
    }
    else
    {
        cf_refresh_file_size("mp_backpacks.xtbl");
    }
    Num_backpack_types = 0;
    Backpack_string_pool.clear();
    free_backpack_infos();
    memset(Backpack_info, 0, sizeof(Backpack_info));
    parse_backpacks_table("mp_backpacks.xtbl");
}

// 0x009C6FD0 (:4299): server: a player picked up a backpack: from a rack (the rack and the backpack's index on it) or
// not (no rack, the backpack's unique id). LTCG: backpack in EDI.
static void send_backpack_picked_up(const player* pp, const multi_object_backpack* backpack)
{
    if (!Multiplayer || !Server || pp == nullptr || backpack == nullptr)
    {
        return;
    }
    unsigned char packet[1200];
    packet[0] = PT_PICKED_UP_BACKPACK;
    unsigned short packet_size = packet_header_size(PT_PICKED_UP_BACKPACK);
    write_u16_be(packet, packet_size, multi_core_network_handle_from_game_handle(pp->get_handle(), false));
    unsigned int index;
    if (backpack->get_backpack_rack() == nullptr)
    {
        write_u16_be(packet, packet_size, 0xFFFF);
        index = (unsigned int)backpack->get_backpack_info()->unique_id;
    }
    else
    {
        write_u16_be(packet, packet_size, multi_core_network_handle_from_game_handle(backpack->get_backpack_rack()->get_handle(), false));
        index = (unsigned int)backpack->get_backpack_rack_index();
    }
    write_u32_be(packet, packet_size, index);
    packet_set_size(packet, packet_size);
    multi_io_send_to_all_reliable(multi_session_get_by_type(ST_GAME), packet, packet_size);
}

// 0x009C7140 (:4375): server: the player's backpack was dropped
static void send_backpack_dropped(const player* pp, const multi_object_backpack* backpack)
{
    if (!Multiplayer || !Server || pp == nullptr || backpack == nullptr)
    {
        return;
    }
    unsigned char packet[1200];
    packet[0] = PT_DROPPED_BACKPACK;
    unsigned short packet_size = packet_header_size(PT_DROPPED_BACKPACK);
    write_u16_be(packet, packet_size, multi_core_network_handle_from_game_handle(pp->get_handle(), false));
    packet_set_size(packet, packet_size);
    multi_io_send_to_all_reliable(multi_session_get_by_type(ST_GAME), packet, packet_size);
}

// 0x009C7210 (:4414): server: the player died with a backpack
static void send_player_died_with_backpack(const player* pp)
{
    if (pp == nullptr || pp->backpack == nullptr)
    {
        return;
    }
    unsigned char packet[1200];
    packet[0] = PT_PLAYER_DIED_WITH_BACKPACK;
    unsigned short packet_size = packet_header_size(PT_PLAYER_DIED_WITH_BACKPACK);
    write_u16_be(packet, packet_size, multi_core_network_handle_from_game_handle(pp->get_handle(), false));
    packet_set_size(packet, packet_size);
    multi_io_send_to_all_reliable(multi_session_get_by_type(ST_GAME), packet, packet_size);
}

// 0x009C72C0 (:4445): an activation toggle: the reason, the player and the backpack's own data (the jump boost's
// direction). The server sends an ok toggle to everyone and a refusal to the player's client only (none for its own
// player); a client asks the server. LTCG: pp in EDI.
static void send_backpack_toggled_activation(const player* pp, multi_backpack_toggle_reasons reason)
{
    if (pp == nullptr || pp->backpack == nullptr || !Multiplayer)
    {
        return;
    }
    unsigned char packet[1200];
    packet[0] = PT_TOGGLED_BACKPACK_ACTIVATION;
    unsigned short packet_size = packet_header_size(PT_TOGGLED_BACKPACK_ACTIVATION);
    packet[packet_size++] = (unsigned char)reason;
    write_u16_be(packet, packet_size, multi_core_network_handle_from_game_handle(pp->get_handle(), false));
    pp->backpack->write_type_specific_info_to_toggle_packet(packet, packet_size);
    packet_set_size(packet, packet_size);
    multi_session* session = multi_session_get_by_type(ST_GAME);
    if (Multiplayer && Server)
    {
        if (reason == MBTR_OK)
        {
            multi_io_send_to_all_reliable(session, packet, packet_size);
            return;
        }
        if (pp == player_get_local_player())
        {
            return;
        }
        multi_io_send_reliable(session->get_player_connection(pp->mp_data), packet, packet_size, nullptr, nullptr, nullptr);
        return;
    }
    multi_io_send_reliable(session->get_host_connection(), packet, packet_size, nullptr, nullptr, nullptr);
}

// 0x009C7580 (:4631): client: the player wants to swap for the rack backpack it is looking at: the player, the rack and
// the backpack's index on it. LTCG: pp in EDI.
static void send_swap_backpack(const player* pp)
{
    if (pp == nullptr || pp->mp_swap_backpack == nullptr || pp->mp_swap_backpack->get_backpack_rack() == nullptr)
    {
        return;
    }
    unsigned char packet[1200];
    packet[0] = PT_SWAP_BACKPACK;
    unsigned short packet_size = packet_header_size(PT_SWAP_BACKPACK);
    write_u16_be(packet, packet_size, multi_core_network_handle_from_game_handle(pp->get_handle(), false));
    write_u16_be(packet, packet_size, multi_core_network_handle_from_game_handle(pp->mp_swap_backpack->get_backpack_rack()->get_handle(), false));
    write_u32_be(packet, packet_size, (unsigned int)pp->mp_swap_backpack->get_backpack_rack_index());
    packet_set_size(packet, packet_size);
    multi_io_send_reliable(multi_session_get_by_type(ST_GAME)->get_host_connection(), packet, packet_size, nullptr, nullptr, nullptr);
}

// ---- console commands (unused in rfg.exe: not registered)
// 0x009AFE50 (:168): server: a backpack 3 m in front of the local player
void unused_dcf_drop_backpack()
{
    console_get_arg(2, 0);
    if (Multiplayer && !Server)
    {
        console_printf("Server only command.\n");
        return;
    }
    if (Console_command == 0)
    {
        if (Console_help != 0)
        {
            console_printf("Usage: drop_backpack [backpack name]\n");
        }
        return;
    }
    const int backpack_type = multi_game_backpack_get_backpack_id(Console_arg);
    if (backpack_type == -1)
    {
        // Original: the name is not passed to the format.
        console_printf("Invalid backpack type: %s\n");
        return;
    }
    player* pp = player_get_local_player();
    if (pp == nullptr)
    {
        console_printf("Cannot drop your backpack: no local player.\n");
        return;
    }
    // Magic numbers: 3 m ahead, half a metre up.
    vector pos(pp->pos.x + pp->orient.fvec.x * 3.0f, pp->pos.y + pp->orient.fvec.y * 3.0f + 0.5f, pp->pos.z + pp->orient.fvec.z * 3.0f);
    multi_game_backpack_create(backpack_type, pos, IDENTITY_MATRIX);
}

// 0x009F1370 (:202): server: the local player's backpack dropped (and its equip log event restarted)
void unused_dcf_remove_backpack()
{
    if (Multiplayer && !Server)
    {
        console_printf("Server only command.\n");
        return;
    }
    if (Console_command == 0)
    {
        return;
    }
    player* pp = player_get_local_player();
    if (pp == nullptr || pp->backpack == nullptr)
    {
        console_printf("Cannot remove your backpack: no local player or local player has no backpack.\n");
        return;
    }
    pp->backpack->event_player_dropped(true);
    if (Multiplayer && Server)
    {
        log_stop_event_backpack_equipped(pp->backpack_equip_log_handle);
        // Original: pp->backpack is null after the drop; the log functions take it as no backpack.
        const char player_xp_level = gamelog_get_player_xp_level(pp);
        const int backpack_crc = gamelog_get_backpack_crc(pp->backpack);
        const char player_index = gamelog_get_player_index((const human*)pp);
        const int time = gamelog_get_logging_time();
        pp->backpack_equip_log_handle = log_start_event_backpack_equipped(gamelog_get_player_checksum(), time, player_index, backpack_crc, player_xp_level);
    }
}

// 0x009F1430 (:266): the backpack table read again
void unused_dcf_reload_backpack_table()
{
    cf_refresh_file_size("mp_backpacks.xtbl");
    Num_backpack_types = 0;
    Backpack_string_pool.clear();
    free_backpack_infos();
    memset(Backpack_info, 0, sizeof(Backpack_info));
    parse_backpacks_table("mp_backpacks.xtbl");
}

// ---- class registration (:2413)
// 0x00958780
void types_multi_object_backpack(signed char& type, signed char& subtype)
{
    type = OT_ITEM;
    subtype = OT_SUB_ITEM_MULTI_BACKPACK;
}

// 0x00958790
int bpo_index_multi_object_backpack()
{
    return 0;
}

// 0x009587A0
const char* classname_multi_object_backpack()
{
    return "multi_object_backpack";
}

// 0x009587B0
unsigned int multi_object_backpack::get_classname_hash()
{
    return multi_object_backpack_instantiator.this_hash;
}

// 0x0097CF90
int multi_object_backpack::pool_remaining()
{
    return multi_object_backpackpool.get_free();
}

// 0x009B0850
void multi_object_backpack::pool_free()
{
    multi_object_backpackpool.free(this);
}

// 0x0099AAB0
object* alloc_multi_object_backpack()
{
    return multi_object_backpackpool.alloc(0);
}

// 0x0097CFA0 (:2414): the timers start invalid (their constructors)
multi_object_backpack::multi_object_backpack()
{
}

// 0x0097D020
multi_object_backpack::~multi_object_backpack()
{
}

// 0x00941D10 (.h:218)
const char* multi_object_backpack::get_classname_static()
{
    return "multi_object_backpack";
}

// 0x00941D20 (.h:218)
int multi_object_backpack::get_bpo_index_static()
{
    return 0;
}

// 0x0097D010 (.h:218)
const char* multi_object_backpack::get_classname() const
{
    return "multi_object_backpack";
}

// 0x0096BC20
multi_backpack_info::multi_backpack_info()
{
    internal_name_crc = checksum_stri::INVALID;
}

// 0x00958320
ripple_hit_info::ripple_hit_info()
{
}

// 0x0097C110 (:120): an item with collision, its backpack type named
obj_backpack_props::obj_backpack_props(const multi_backpack_info* backpack_info, const vector& pos, const matrix& orient, const obj_item_flags& flags)
    : obj_item_props(backpack_info->item_infop, pos, orient, flags, COLLIDE_ITEM, -1.0f, INVALID_SALVAGE_MATERIAL)
{
    set(PROP_BACKPACK_TYPE, backpack_info->internal_name);
}

// 0x0097D4A0
obj_backpack_props::~obj_backpack_props()
{
}

// ---- accessors (.h)
// 0x006F6AB0 (.h:248)
const multi_backpack_info* multi_object_backpack::get_backpack_info() const
{
    return backpack_info;
}

// 0x006C3F10 (.h:249): the active abilities while activated, the passive ones otherwise
const multi_backpack_info::abilities* multi_object_backpack::get_backpack_abilities() const
{
    return activated ? &backpack_info->active_abilities : &backpack_info->passive_abilities;
}

// 0x00941D40 (.h:251)
multi_object_marker* multi_object_backpack::get_backpack_rack() const
{
    return backpack_rack;
}

// 0x00941D50 (.h:252)
int multi_object_backpack::get_backpack_rack_index() const
{
    return backpack_rack_index;
}

// 0x00941D60 (.h:253)
int multi_object_backpack::get_backpack_rack_attach_tag_index() const
{
    return backpack_rack_attach_tag_index;
}

// 0x00A06A10 (.h:254)
float multi_object_backpack::get_fuel() const
{
    return fuel;
}

// 0x0075E5B0 (.h:255)
bool multi_object_backpack::is_activated() const
{
    return activated;
}

// 0x00941D70 (.h:256)
float multi_object_backpack::get_jump_boost_horizontal() const
{
    return jump_boost_horizontal;
}

// 0x00941D80 (.h:257)
float multi_object_backpack::get_jump_boost_vertical() const
{
    return jump_boost_vertical;
}

// 0x00941D90 (.h:258)
bool multi_object_backpack::get_jump_boost_direction_up() const
{
    return jump_boost_direction_up;
}

// 0x0075E5C0 (.h:259)
bool multi_object_backpack::is_ramping_up() const
{
    return ramp_up_timer.valid();
}

// 0x00941DA0 (.h:314)
bool multi_object_backpack::did_rhino_hit_mover_this_run() const
{
    return rhino_hit_mover_this_run;
}

// ---- object
// 0x009B0640 (:1381): the info from the backpack type property; on load everything starts reset
bool multi_object_backpack::serialize(obj_prop_block& pblock, bool load)
{
    if (load)
    {
        backpack_info = nullptr;
        char name[32];
        pblock.get(PROP_BACKPACK_TYPE, name);
        backpack_info = multi_game_backpack_get_info(multi_game_backpack_get_backpack_id(name));
    }
    if (!item::serialize(pblock, load))
    {
        return false;
    }
    if (load)
    {
        for (int i = 0; i < 16; i++)
        {
            mat_fx_handle[i] = 0xFFFFFFFF;
        }
        mat_fx_weapon_handle = 0xFFFFFFFF;
        backpack_rack = nullptr;
        backpack_rack_index = -1;
        backpack_rack_attach_tag_index = -1;
        fuel_effect_attach_tag_index = -1;
        fuel_effect_object = 0xFFFFFFFF;
        current_fuel_effect = nullptr;
        m_prop_type = PROP_TYPE_JETPACK;
        reset();
    }
    else
    {
        pblock.set(PROP_BACKPACK_TYPE, backpack_info->internal_name);
    }
    return true;
}

// 0x009580C0 (:1429): no physics; a single player backpack must be preloaded (outside Wrecking Crew). The rack tag and
// the fuel gauge's tag are looked up and the gauge effect plays.
bool multi_object_backpack::stream(bool load)
{
    item_flags.no_physics = 1;
    if (!item::stream(load))
    {
        return false;
    }
    if (load)
    {
        if (!wrecking_crew_is_active() && !Multiplayer)
        {
            stream2_container* container = stream2_get_container(srid);
            if (container == nullptr)
            {
                return false;
            }
            if (container->cdef->container_type != BACKPACK_PRELOAD_CONTAINER_TYPE)
            {
                keen::debug::nativePrintArgs("A backpack is being used in single player that's not preloaded.");
                return false;
            }
        }
        backpack_rack_attach_tag_index = get_tag_index_from_name("Attach_To_Rack");
        fuel_effect_attach_tag_index = get_tag_index_from_name("fuel_level");
        fuel_effect_object = effects_play(Backpack_fuel_effect_charged, get_handle(), fuel_effect_attach_tag_index, true, 0xFFFFFFFF, 1.0f, false);
        current_fuel_effect = Backpack_fuel_effect_charged;
    }
    return true;
}

// 0x00958190 (:1463)
void multi_object_backpack::hierarchy_notify(hierarchy_notify_code code)
{
    item::hierarchy_notify(code);
    if (code == HN_PARENT_DESTROYED)
    {
        World.destroy_object(this, false);
    }
}

// 0x009B07B0 (:1478): off the player, the effects released, the fuel effect destroyed
void multi_object_backpack::free_this()
{
    player* pp = player_attached_to;
    if (pp != nullptr && pp->backpack == this && pp->obj_flags._dont_use_me)
    {
        pp->remove_child(this);
        player_attached_to->backpack = nullptr;
        player_attached_to = nullptr;
    }
    effects_alr_release(&backpack_info->pickup_effects, &pickup_effects_alr_handles);
    if (backpack_info->activatable)
    {
        effects_alr_release(&backpack_info->activation_effects, &activation_effects_alr_handles);
    }
    if (fuel_effect_object != 0xFFFFFFFF)
    {
        World.destroy_object(fuel_effect_object, false);
    }
    item::free_this();
}

// 0x009581C0 (:1502)
bool multi_object_backpack::attach_to_player(player* pp)
{
    pp->backpack = this;
    player_attached_to = pp;
    return human_wear_prop(pp, this);
}

// 0x009581E0 (:1512)
bool multi_object_backpack::detach_from_player()
{
    const bool removed = player_attached_to->remove_child(this);
    player_attached_to->backpack = nullptr;
    player_attached_to = nullptr;
    return removed;
}

// Every firearm of the inventory (stopping at the first non-weapon item, as rfg.exe: it returns).
static weapon* inventory_weapon(const inventory_item* iip, bool& stop)
{
    stop = false;
    if (iip->weapon_handle == 0xFFFFFFFF)
    {
        return nullptr;
    }
    object* objp = World.object_from_handle(iip->weapon_handle);
    if (objp == nullptr || objp->get_obj_type() != OT_ITEM || objp->get_sub_type() != OT_SUB_ITEM_WEAPON)
    {
        stop = true;
        return nullptr;
    }
    return (weapon*)objp;
}

// 0x0097C410 (:1523): the abilities of the current set that change the player: stealth, the ammo (scaled within the
// magazine and the reserve), the vision effect for the local player
void multi_object_backpack::activate_dual_abilities()
{
    const multi_backpack_info::abilities* abilities = get_backpack_abilities();
    if (abilities->stealth)
    {
        stealth_player();
    }
    if (abilities->ammo_factor != 1.0f)
    {
        inventory_item* iip = player_attached_to->inventory;
        while (iip != nullptr)
        {
            bool stop;
            weapon* wp = inventory_weapon(iip, stop);
            if (stop)
            {
                return;
            }
            if (wp != nullptr)
            {
                wp->set_rounds_in_magazine((int)((float)wp->get_rounds_in_magazine() * abilities->ammo_factor));
                wp->set_rounds_in_reserve((int)((float)wp->get_rounds_in_reserve() * abilities->ammo_factor));
            }
            iip = iip->next;
            if (iip == player_attached_to->inventory)
            {
                break;
            }
        }
    }
    if (abilities->vision && player_attached_to == player_get_local_player())
    {
        xray_effect_start(XET_VISION_BACKPACK);
        if (abilities->vision_vfx)
        {
            // The ramp in continues from where a ramp out stopped.
            int remaining = 0;
            if (vision_vfx_timer.valid() && !vision_vfx_timer.elapsed())
            {
                remaining = vision_vfx_timer.time_until();
            }
            vision_vfx_timer.set(abilities->vision_vfx_ramp_in_time_ms);
            vision_vfx_timer.deduct_time(remaining);
        }
    }
}

// 0x0097C5E0 (:1569): the abilities of the current set undone: stealth, the ammo clamped to the normal magazine and
// reserve, the vision effect, the field of view (no fine aim)
void multi_object_backpack::deactivate_dual_abilities()
{
    const multi_backpack_info::abilities* abilities = get_backpack_abilities();
    if (abilities->stealth)
    {
        unstealth_player();
    }
    if (abilities->ammo_factor != 1.0f)
    {
        inventory_item* iip = player_attached_to->inventory;
        while (iip != nullptr)
        {
            bool stop;
            weapon* wp = inventory_weapon(iip, stop);
            if (stop)
            {
                return;
            }
            if (wp != nullptr)
            {
                if (wp->get_rounds_in_magazine() > wp->get_magazine_size())
                {
                    wp->set_rounds_in_magazine(wp->get_magazine_size());
                }
                if (wp->get_rounds_in_reserve() > wp->get_max_rounds() - wp->get_magazine_size())
                {
                    wp->set_rounds_in_reserve(wp->get_max_rounds() - wp->get_magazine_size());
                }
            }
            iip = iip->next;
            if (iip == player_attached_to->inventory)
            {
                break;
            }
        }
    }
    if (abilities->vision && player_attached_to == player_get_local_player())
    {
        xray_effect_stop(XET_VISION_BACKPACK);
        if (abilities->vision_vfx)
        {
            int remaining = 0;
            if (vision_vfx_timer.valid() && !vision_vfx_timer.elapsed())
            {
                remaining = vision_vfx_timer.time_until();
            }
            vision_vfx_timer.set(abilities->vision_vfx_ramp_out_time_ms);
            vision_vfx_timer.deduct_time(remaining);
        }
    }
    if (player_attached_to == player_get_local_player() && backpack_info->no_fine_aim)
    {
        // Magic number: the field of view returns over 0.5 s.
        camera_view_set_fov((float)camera_view_get_fov(), 500);
    }
}

// The material effect parameters of the backpack (time in w off for stealth backpacks).
static material_effect_parameters backpack_material_effect_parameters(const multi_backpack_info* info)
{
    material_effect_parameters params = material_effects_get_parameters(0, -1);
    if (info->active_abilities.stealth)
    {
        params.use_time_in_w = false;
    }
    return params;
}

// 0x00999BF0 (:1620): the item's material effect on the backpack (and its children's meshes, up to 16) and the player
void multi_object_backpack::start_material_effects(bool player_effect_just_stopped)
{
    mat_fx_succeeded = false;
    const unsigned int effect = info->material_effect_id;
    if (effect != 0xFFFFFFFF)
    {
        const bool player_stopped = player_stop_material_effect(player_attached_to) || player_effect_just_stopped;
        if (stop_material_effects())
        {
            return;
        }
        if (player_stopped && backpack_info->mat_fx_on_human)
        {
            return;
        }
        material_effect_parameters params = backpack_material_effect_parameters(backpack_info);
        if (backpack_info->mat_fx_on_backpack && char_inst != nullptr && char_inst->rdd.is_loaded() && char_inst->rdd.get_data()->renderable_instance_p != nullptr)
        {
            mat_fx_handle[0] = material_effects_apply((rl_renderable_instance*)char_inst->rdd.get_data()->renderable_instance_p, (int)effect, &params);
            int count = 1;
            object* child = child_ptr;
            while (child != nullptr)
            {
                if (child->get_obj_type() == OT_ITEM)
                {
                    item* child_item = (item*)child;
                    if (child_item->rdd.is_loaded() && child_item->rdd.get_data()->smesh_instance_p != nullptr)
                    {
                        if (count > 15)
                        {
                            return;
                        }
                        mat_fx_handle[count++] = material_effects_apply((rl_renderable_instance*)child_item->rdd.get_data()->smesh_instance_p, (int)effect, &params);
                    }
                    character_instance* child_char = child_item->char_inst;
                    if (child_char != nullptr && child_char->rdd.is_loaded() && child_char->rdd.get_data()->renderable_instance_p != nullptr)
                    {
                        if (count > 15)
                        {
                            return;
                        }
                        mat_fx_handle[count++] = material_effects_apply((rl_renderable_instance*)child_char->rdd.get_data()->renderable_instance_p, (int)effect, &params);
                    }
                }
                child = child->child_next;
                if (child == child_ptr)
                {
                    break;
                }
            }
        }
        if (player_attached_to != nullptr)
        {
            // Original: the equipped weapon is fetched and not used.
            player_attached_to->get_equipped_weapon();
            if (backpack_info->mat_fx_on_human)
            {
                player_start_material_effect(player_attached_to, effect, params);
            }
        }
    }
    mat_fx_succeeded = true;
}

// 0x00999DA0 (:1700)
void multi_object_backpack::process_material_effects()
{
    if (!mat_fx_succeeded)
    {
        start_material_effects(false);
    }
}

// 0x0097C810 (:1712): the opacity and stealth back to normal, every material effect removed. Returns whether one was.
bool multi_object_backpack::stop_material_effects()
{
    bool stopped = false;
    if (player_attached_to != nullptr)
    {
        player_attached_to->set_opacity_plus_child_items(1.0f);
        player_attached_to->set_stealth_plus_child_items(0.0f);
        if (backpack_info->mat_fx_on_human)
        {
            stopped = player_stop_material_effect(player_attached_to);
        }
    }
    for (int i = 0; i < 16; i++)
    {
        if (mat_fx_handle[i] != 0xFFFFFFFF)
        {
            material_effects_remove(mat_fx_handle[i]);
            mat_fx_handle[i] = 0xFFFFFFFF;
            stopped = true;
        }
    }
    if (mat_fx_weapon_handle != 0xFFFFFFFF)
    {
        object* objp = World.object_from_handle(mat_fx_weapon_handle);
        if (objp != nullptr && objp->get_obj_type() == OT_ITEM && objp->get_sub_type() == OT_SUB_ITEM_WEAPON && backpack_info->mat_fx_on_weapon)
        {
            weapon_stop_material_effect((weapon*)objp);
        }
    }
    mat_fx_weapon_handle = 0xFFFFFFFF;
    return stopped;
}

// 0x0097C8E0 (:1744): an activated backpack's material effect moves to the newly equipped weapon
void multi_object_backpack::process_equip_item()
{
    if (!activated || !backpack_info->mat_fx_on_weapon)
    {
        return;
    }
    const unsigned int effect = info->material_effect_id;
    if (effect == 0xFFFFFFFF)
    {
        return;
    }
    material_effect_parameters params = backpack_material_effect_parameters(backpack_info);
    if (player_attached_to == nullptr)
    {
        return;
    }
    weapon* wp = player_attached_to->get_equipped_weapon();
    if (wp == nullptr || mat_fx_weapon_handle == wp->get_handle())
    {
        return;
    }
    weapon* old_wp = World.object_from_handle<weapon>(mat_fx_weapon_handle);
    mat_fx_weapon_handle = 0xFFFFFFFF;
    if (old_wp != nullptr)
    {
        weapon_stop_material_effect(old_wp);
    }
    mat_fx_weapon_handle = wp->get_handle();
    weapon_start_material_effect(wp, player_attached_to, effect, params);
}

// 0x00958210 (:1787): an activated backpack's weapon material effect while a remote charge is held (on) or not (off)
void multi_object_backpack::remote_charge_change(bool held)
{
    if (!activated || !backpack_info->mat_fx_on_weapon)
    {
        return;
    }
    const unsigned int effect = info->material_effect_id;
    if (effect == 0xFFFFFFFF)
    {
        return;
    }
    material_effect_parameters params = backpack_material_effect_parameters(backpack_info);
    weapon* wp = nullptr;
    if (player_attached_to != nullptr)
    {
        wp = player_attached_to->get_equipped_weapon();
    }
    if (held)
    {
        weapon_start_material_effect(wp, player_attached_to, effect, params);
        return;
    }
    weapon_stop_material_effect(wp);
}

// 0x0097C9C0 (:1814): the backpack's offset on the player's character (multiplayer: by team, character and backpack)
const vector* multi_object_backpack::get_player_offset() const
{
    const player* pp = player_attached_to;
    if (pp == nullptr)
    {
        return nullptr;
    }
    if (Multiplayer)
    {
        const unsigned int data_handle = pp->mp_data;
        if (Mp_game_data == nullptr || data_handle == 0xFFFFFFFF)
        {
            return nullptr;
        }
        const multi_game_data* data = &Mp_game_data->data[data_handle & 0xFFFF][data_handle >> 16];
        if (!data->in_use)
        {
            // Original: the linger time is read and not used.
            return nullptr;
        }
        const human_teams team = Multi_gameplay_mode >= 0 && Multi_gameplay_mode <= 7 && Multi_gameplay_mode_funcs[Multi_gameplay_mode].is_team_mode ? pp->current_team : HUMAN_TEAM_CIVILIAN;
        return multi_game_player_get_backpack_offset(team, data->player_selection_preference, backpack_info->unique_id);
    }
    // Wrecking Crew: the offsets of the current player's character (wrecking_crew.xtbl).
    if (wrecking_crew_is_active() && Wrecking_crew_current_player != -1)
    {
        const wc_game_character_info* character_info = Wrecking_crew_characters[Wc_player_infos[Wrecking_crew_current_player].character_index];
        return &character_info->backpack_offsets[backpack_info->unique_id];
    }
    return nullptr;
}

// 0x009582C0 (:1835): the shockwave explosion at the position (the server makes it)
void multi_object_backpack::use_shockwave(const vector& posp)
{
    if (player_attached_to == nullptr)
    {
        return;
    }
    const multi_backpack_info::abilities* abilities = get_backpack_abilities();
    if (!Multiplayer || Server)
    {
        explosion_create(abilities->shockwave_explosion_info, player_attached_to, player_attached_to, posp, IDENTITY_MATRIX, nullptr, abilities->w_info, false);
    }
}

// The ripple's push on a target: away from the player at the hit speed, at least the minimum upwards.
static vector ripple_velocity(const vector& target_pos, const vector& player_pos, const multi_backpack_info::abilities* abilities)
{
    vector velocity = target_pos - player_pos;
    velocity.normalize_safe();
    const float speed = abilities->ripple_player_hit_speed;
    velocity.x = velocity.x * speed;
    velocity.z = velocity.z * speed;
    float vertical = speed * velocity.y;
    if (speed * velocity.y - abilities->ripple_player_minimum_y_velocity < 0.0f)
    {
        vertical = abilities->ripple_player_minimum_y_velocity;
    }
    velocity.y = vertical;
    return velocity;
}

// 0x009F1B10 (:1849): the tremor: its effect, then the humans (single player) or the enemy players (the server)
// around pushed, and the movers with bodies around pushed away from it
void multi_object_backpack::use_ripple(const vector& posp)
{
    if (player_attached_to == nullptr)
    {
        return;
    }
    const multi_backpack_info::abilities* abilities = get_backpack_abilities();
    if (abilities->ripple_effect.effect != nullptr)
    {
        effects_play(abilities->ripple_effect.effect, posp, IDENTITY_MATRIX, false, get_handle(), get_tag_index_from_name(abilities->ripple_effect.attach_point_name), 0xFFFFFFFF, 1.0f, false, EFFECTS_CATEGORY_NONE);
    }
    unsigned int handles[64];
    ripple_hit_info hits[64];
    if (!Multiplayer && !wrecking_crew_is_active() && dlc_get_current_id() == -1)
    {
        const int num_humans = World.collect_objects<human>(posp, abilities->ripple_range, handles, 64, nullptr, nullptr);
        int num_hit = 0;
        for (int i = 0; i < num_humans; i++)
        {
            human* hp = World.object_from_handle<human>(handles[i]);
            if (hp != player_attached_to && !hp->hflags.invulnerable)
            {
                hits[num_hit].velocity = ripple_velocity(hp->pos, player_attached_to->pos, abilities);
                hits[num_hit].victim = hp;
                num_hit++;
            }
        }
        ripple_process_npcs_hit(player_attached_to, hits, num_hit);
        // Original: the vehicle handles go into the players' hit array's storage (as here) and the hits into the npc
        // array.
        unsigned int vehicle_handles[64];
        const int num_vehicles = World.collect_objects<vehicle>(posp, abilities->ripple_range, vehicle_handles, 64, nullptr, nullptr);
        num_hit = 0;
        for (int i = 0; i < num_vehicles; i++)
        {
            vehicle* vp = World.object_from_handle<vehicle>(vehicle_handles[i]);
            if (vp->get_sub_type() == OT_SUB_VEHICLE_WALKER)
            {
                hits[num_hit].velocity = ripple_velocity(vp->pos, player_attached_to->pos, abilities);
                hits[num_hit].victim = vp;
                num_hit++;
            }
        }
        ripple_process_vehicles_hit(player_attached_to, hits, num_hit);
    }
    else if (Server)
    {
        unsigned int player_handles[16];
        ripple_hit_info player_hits[16];
        const int num_players = World.collect_objects<player>(posp, abilities->ripple_range, player_handles, 16, nullptr, nullptr);
        int num_hit = 0;
        for (int i = 0; i < num_players; i++)
        {
            player* pp = World.object_from_handle<player>(player_handles[i]);
            if (pp != player_attached_to && (human_teams_are_enemies(player_attached_to, pp) || Mp_server_options.generic_options.friendly_fire) && !pp->hflags.invulnerable)
            {
                player_hits[num_hit].velocity = ripple_velocity(pp->pos, player_attached_to->pos, abilities);
                player_hits[num_hit].victim = pp;
                num_hit++;
            }
        }
        ripple_process_players_hit(player_attached_to, player_hits, num_hit);
    }
    if (abilities->ripple_mover_impulse != 0)
    {
        const int num_movers = World.collect_objects<object_mover>(posp, abilities->ripple_range, handles, 64, nullptr, nullptr);
        for (int i = 0; i < num_movers; i++)
        {
            object_mover* mover = World.object_from_handle<object_mover>(handles[i]);
            if (mover->havok_handle == 0xFFFFFFFF)
            {
                continue;
            }
            vector impulse = mover->pos - posp;
            impulse.normalize_safe();
            impulse = impulse * (float)abilities->ripple_mover_impulse;
            // Magic numbers: the linear impulse as is, no angular, at most 15 m/s.
            explosion_modify_impulses(&impulse, nullptr, &impulse, nullptr, mover->get_mass(), 1.0f, 0.0f, 15.0f, 0.0f);
            havok_body_apply_linear_impulse(mover->havok_handle, impulse);
        }
    }
}

// 0x00958330 (:1982)
void multi_object_backpack::use_jump_boost()
{
    if (player_attached_to != nullptr)
    {
        apply_jump_boost(player_attached_to);
    }
}

// 0x00958350 (:1990): the kaboom explosion at the backpack (the server; it kills the user when it damages it), its camera
// shake (stronger for the user, fading with the distance), and the delay before the next
void multi_object_backpack::use_kaboom()
{
    player* pp = player_attached_to;
    if (pp == nullptr)
    {
        return;
    }
    const multi_backpack_info* binfo = backpack_info;
    const multi_backpack_info::abilities* abilities = activated ? &binfo->active_abilities : &binfo->passive_abilities;
    if (!Multiplayer || Server)
    {
        player* source = binfo->kaboom_damages_user ? nullptr : pp;
        explosion_create(binfo->kaboom_explosion_info, source, pp, pos, IDENTITY_MATRIX, nullptr, nullptr, false);
        if (backpack_info->kaboom_damages_user)
        {
            // Magic number: -1 damage is lethal.
            human_damage(player_attached_to, player_attached_to, -1.0f, DBT_EXPLOSION, abilities->w_info, nullptr, true, false);
        }
    }
    if (backpack_info->kaboom_camera_shake != nullptr)
    {
        float intensity = 1.0f;
        if (player_get_local_player() == player_attached_to)
        {
            intensity = backpack_info->kaboom_camera_shake_user_intensity;
        }
        local_player_info local_info;
        player_get_local_player_info(&local_info);
        const float falloff = backpack_info->kaboom_camera_shake_falloff;
        if (falloff != 0.0f)
        {
            const vector d = player_attached_to->pos - *local_info.pos;
            float fraction = sqrtf(d.x * d.x + d.y * d.y + d.z * d.z) / falloff;
            if (1.0f - fraction < 0.0f)
            {
                fraction = 1.0f;
            }
            intensity = (1.0f - fraction) * intensity;
        }
        camera_shake_play(backpack_info->kaboom_camera_shake, intensity, backpack_info->kaboom_camera_shake_ignore_disabled);
    }
    kaboom_explosion_delay_timer.set(backpack_info->kaboom_delay_ms);
}

// 0x00958590 (:2034): the effects' hosts and tags: the player, the backpack and the equipped weapon (none without one)
void multi_object_backpack::effects_alr_store_handle_info(const multi_backpack_info::effects_alr* effects_alr, effects_alr_handles* handles)
{
    for (int i = 0; i < effects_alr->num_player_effects; i++)
    {
        handles->player_effects_handles[i].host = player_attached_to->get_handle();
        handles->player_effects_handles[i].tag_index = player_attached_to->get_tag_index_from_name(effects_alr->player_effects[i].attach_point_name);
    }
    for (int i = 0; i < effects_alr->num_backpack_effects; i++)
    {
        handles->backpack_effects_handles[i].host = get_handle();
        handles->backpack_effects_handles[i].tag_index = get_tag_index_from_name(effects_alr->backpack_effects[i].attach_point_name);
    }
    weapon* wp = player_attached_to->get_equipped_weapon();
    for (int i = 0; i < effects_alr->num_weapon_effects; i++)
    {
        if (wp == nullptr)
        {
            handles->weapon_effects_handles[i].host = 0xFFFFFFFF;
            handles->weapon_effects_handles[i].tag_index = -1;
        }
        else
        {
            handles->weapon_effects_handles[i].host = wp->get_handle();
            handles->weapon_effects_handles[i].tag_index = wp->get_tag_index_from_name(effects_alr->weapon_effects[i].attach_point_name);
        }
    }
    handles->arc_backpack_tag_index = get_tag_index_from_name(effects_alr->arc_backpack_attach_point_name);
    handles->arc_weapon_tag_index = wp != nullptr ? wp->get_tag_index_from_name(effects_alr->arc_weapon_attach_point_name) : -1;
}

// 0x0097CAC0 (:2066): the attack: the sound (the local player's or the 3d one) and the attack effects (the alternate
// backpack effects for an activated alternate jump)
void multi_object_backpack::effects_alr_attack(const multi_backpack_info::effects_alr* effects_alr, effects_alr_handles* handles)
{
    effects_alr_store_handle_info(effects_alr, handles);
    if (!player_attached_to->obj_flags._dont_use_me)
    {
        return;
    }
    if (effects_alr->sfx_alr != -1)
    {
        if (player_attached_to == player_get_local_player())
        {
            audiolib_att_loop_rel_start(effects_alr->sfx_alr, &handles->sfx_alr_handle, false, nullptr, nullptr);
        }
        else
        {
            audiolib_att_loop_rel_start(effects_alr->sfx_npc_alr, &handles->sfx_alr_handle, false, &pos, nullptr);
        }
    }
    for (int i = 0; i < effects_alr->num_player_effects; i++)
    {
        effects_attack(&effects_alr->player_effects[i], &handles->player_effects_handles[i]);
    }
    if (activated && use_alternate_backpack_activation_effects)
    {
        for (int i = 0; i < effects_alr->num_alternate_backpack_effects; i++)
        {
            effects_attack(&effects_alr->alternate_backpack_effects[i], &handles->backpack_effects_handles[i]);
        }
    }
    else
    {
        for (int i = 0; i < effects_alr->num_backpack_effects; i++)
        {
            effects_attack(&effects_alr->backpack_effects[i], &handles->backpack_effects_handles[i]);
        }
    }
    for (int i = 0; i < effects_alr->num_weapon_effects; i++)
    {
        effects_attack(&effects_alr->weapon_effects[i], &handles->weapon_effects_handles[i]);
    }
}

// 0x00958700 (:2107)
void multi_object_backpack::effects_attack(const multi_backpack_info::effects_alr::effects* effects, effects_alr_handles::effects_handles* handles)
{
    if (effects->attack_effect != nullptr && handles->host != 0xFFFFFFFF && World.object_from_handle(handles->host) != nullptr)
    {
        handles->attack_effect_handle = effects_play(effects->attack_effect, handles->host, handles->tag_index, false, 0xFFFFFFFF, 1.0f, false);
    }
}

// 0x00999DC0 (:2119): the sound follows a remote player's backpack, the loop effects play, and the arc runs from the
// backpack over the player's head to the weapon along a spline
void multi_object_backpack::effects_alr_loop(const multi_backpack_info::effects_alr* effects_alr, effects_alr_handles* handles)
{
    if (handles->sfx_alr_handle != -1 && player_attached_to != player_get_local_player())
    {
        audiolib_att_loop_rel_set_pos(handles->sfx_alr_handle, &pos, nullptr);
    }
    for (int i = 0; i < effects_alr->num_player_effects; i++)
    {
        effects_loop(&effects_alr->player_effects[i], &handles->player_effects_handles[i], effects_alr->loop_waits_for_attack);
    }
    if (activated && use_alternate_backpack_activation_effects)
    {
        for (int i = 0; i < effects_alr->num_alternate_backpack_effects; i++)
        {
            effects_loop(&effects_alr->alternate_backpack_effects[i], &handles->backpack_effects_handles[i], effects_alr->loop_waits_for_attack);
        }
    }
    else
    {
        for (int i = 0; i < effects_alr->num_backpack_effects; i++)
        {
            effects_loop(&effects_alr->backpack_effects[i], &handles->backpack_effects_handles[i], effects_alr->loop_waits_for_attack);
        }
    }
    for (int i = 0; i < effects_alr->num_weapon_effects; i++)
    {
        effects_loop(&effects_alr->weapon_effects[i], &handles->weapon_effects_handles[i], effects_alr->loop_waits_for_attack);
    }
    weapon* wp = player_attached_to->get_equipped_weapon();
    if (effects_alr->arc_effect == nullptr || wp == nullptr)
    {
        return;
    }
    object* arc = handles->arc_handle != 0xFFFFFFFF ? World.object_from_handle(handles->arc_handle) : nullptr;
    if (arc == nullptr || arc->get_obj_type() != OT_EFFECT)
    {
        vector start = pos;
        if (handles->arc_backpack_tag_index != -1)
        {
            get_tag_world_coords(handles->arc_backpack_tag_index, start, nullptr, nullptr, nullptr);
        }
        handles->arc_handle = effects_play(effects_alr->arc_effect, start, orient, true, 0xFFFFFFFF, -1, 0xFFFFFFFF, 1.0f, false, EFFECTS_CATEGORY_NONE);
        handles->arc_position = 0.0f;
        return;
    }
    vector backpack_point = pos;
    if (handles->arc_backpack_tag_index != -1)
    {
        get_tag_world_coords(handles->arc_backpack_tag_index, backpack_point, nullptr, nullptr, nullptr);
    }
    vector weapon_point = wp->pos;
    if (handles->arc_weapon_tag_index != -1)
    {
        wp->get_tag_world_coords(handles->arc_weapon_tag_index, weapon_point, nullptr, nullptr, nullptr);
    }
    const int head_tag = player_attached_to->get_tag_index_from_name("Head");
    vector head = player_attached_to->pos;
    player_attached_to->get_tag_world_coords(head_tag, head, nullptr, nullptr, nullptr);
    const vector line_dir = weapon_point - backpack_point;
    const vector side = line_dir.copy_flatten_normalize();
    const vector side_offset = side * Backpack_arc_horizontal_offset;
    const vector up_offset = UP_VECTOR * Backpack_arc_vertical_offset;
    // The spline's seven points: the backpack twice, behind it (lifted to the head's height), the middle, over the head,
    // the weapon twice.
    vector points[7];
    points[0] = backpack_point;
    points[1] = backpack_point;
    const vector behind = up_offset - side_offset;
    points[2].set(behind.x * Backpack_arc_back_multiplier + backpack_point.x, behind.y * Backpack_arc_back_multiplier + (head.y - backpack_point.y < 0.0f ? backpack_point.y : head.y), behind.z * Backpack_arc_back_multiplier + backpack_point.z);
    points[4] = side_offset + up_offset + head;
    points[5] = weapon_point;
    points[6] = weapon_point;
    const vector middle = (points[4] - points[2]) * 0.5f + points[2];
    points[3] = middle + UP_VECTOR * Backpack_arc_mid_vertical_offset * Backpack_arc_vertical_offset;
    // Original: the spline is built before the middle point is set and the points are rotated (the spline keeps a
    // pointer to the points).
    cardinal_spline spline(7, points, Backpack_arc_tension);
    for (int i = 0; i < 7; i++)
    {
        vec_rot_point_around_line(points[i], points[i], Backpack_arc_rotation, backpack_point, line_dir);
    }
    handles->arc_position = effects_alr->arc_travel_speed * Frametime + handles->arc_position;
    if (handles->arc_position < 1.0f)
    {
        vector arc_pos;
        // Magic numbers: the arc runs between the second and the sixth point.
        spline.get_point(arc_pos, handles->arc_position * 4.0f + 1.0f);
        arc->update_pos_and_orient(arc_pos, arc->orient, true);
        return;
    }
    World.destroy_object(arc, false);
    vector start = pos;
    if (handles->arc_backpack_tag_index != -1)
    {
        get_tag_world_coords(handles->arc_backpack_tag_index, start, nullptr, nullptr, nullptr);
    }
    handles->arc_handle = effects_play(effects_alr->arc_effect, start, orient, true, 0xFFFFFFFF, -1, 0xFFFFFFFF, 1.0f, false, EFFECTS_CATEGORY_NONE);
    handles->arc_position = 0.0f;
}

// 0x0097CD10 (:2254): the loop effect starts once the attack effect is gone (or at once), and follows the player's speed
void multi_object_backpack::effects_loop(const multi_backpack_info::effects_alr::effects* effects, effects_alr_handles::effects_handles* handles, bool loop_waits_for_attack)
{
    if (handles->attack_effect_handle != 0xFFFFFFFF)
    {
        object* objp = World.object_from_handle(handles->attack_effect_handle);
        if (objp == nullptr || objp->get_obj_type() != OT_EFFECT)
        {
            handles->attack_effect_handle = 0xFFFFFFFF;
        }
    }
    if ((handles->attack_effect_handle == 0xFFFFFFFF || !loop_waits_for_attack) && handles->loop_effect_handle == 0xFFFFFFFF && effects->loop_effect != nullptr)
    {
        if (handles->host != 0xFFFFFFFF && World.object_from_handle(handles->host) != nullptr)
        {
            handles->loop_effect_handle = effects_play(effects->loop_effect, handles->host, handles->tag_index, true, 0xFFFFFFFF, 1.0f, false);
        }
        return;
    }
    if (handles->loop_effect_handle == 0xFFFFFFFF)
    {
        return;
    }
    object_effect* loop = (object_effect*)World.object_from_handle(handles->loop_effect_handle);
    if (loop != nullptr && loop->get_obj_type() == OT_EFFECT && player_attached_to != nullptr)
    {
        vector vel;
        human_controller_get_vel(player_attached_to, vel);
        loop->set_expression_variable(EFFECTS_VAR_OBJECT_SPEED, sqrtf(vel.x * vel.x + vel.y * vel.y + vel.z * vel.z));
    }
}

static void stop_effect_object(unsigned int effect_handle)
{
    if (effect_handle == 0xFFFFFFFF)
    {
        return;
    }
    object_effect* effect = (object_effect*)World.object_from_handle(effect_handle);
    if (effect != nullptr && effect->get_obj_type() == OT_EFFECT)
    {
        effect->stop(object_effect::SS_STOP_EMITTING);
    }
}

// 0x0099A850 (:2284): the sound stops (fading 0.1 s), every effect stops emitting, the release effects play
void multi_object_backpack::effects_alr_release(const multi_backpack_info::effects_alr* effects_alr, effects_alr_handles* handles)
{
    if (handles->sfx_alr_handle != -1)
    {
        if (player_attached_to != player_get_local_player())
        {
            audiolib_att_loop_rel_set_pos(handles->sfx_alr_handle, &pos, nullptr);
        }
        audiolib_att_loop_rel_stop(handles->sfx_alr_handle, 100);
    }
    for (int i = 0; i < effects_alr->num_player_effects; i++)
    {
        effects_release(&effects_alr->player_effects[i], &handles->player_effects_handles[i]);
    }
    for (int i = 0; i < effects_alr->num_backpack_effects; i++)
    {
        effects_release(&effects_alr->backpack_effects[i], &handles->backpack_effects_handles[i]);
    }
    for (int i = 0; i < effects_alr->num_weapon_effects; i++)
    {
        effects_release(&effects_alr->weapon_effects[i], &handles->weapon_effects_handles[i]);
    }
    stop_effect_object(handles->arc_handle);
    handles->arc_handle = 0xFFFFFFFF;
}

// 0x0097CE40 (:2316)
void multi_object_backpack::effects_release(const multi_backpack_info::effects_alr::effects* effects, effects_alr_handles::effects_handles* handles)
{
    stop_effect_object(handles->attack_effect_handle);
    handles->attack_effect_handle = 0xFFFFFFFF;
    stop_effect_object(handles->loop_effect_handle);
    handles->loop_effect_handle = 0xFFFFFFFF;
    if (effects->release_effect != nullptr && handles->host != 0xFFFFFFFF && World.object_from_handle(handles->host) != nullptr)
    {
        effects_play(effects->release_effect, handles->host, handles->tag_index, false, 0xFFFFFFFF, 1.0f, false);
    }
}

// 0x0097CEF0 (:2342)
void multi_object_backpack::effects_handles_alr_reset(const multi_backpack_info::effects_alr* effects_alr, effects_alr_handles* handles)
{
    handles->sfx_alr_handle = -1;
    for (int i = 0; i < effects_alr->num_player_effects; i++)
    {
        effects_handles_reset(&handles->player_effects_handles[i]);
    }
    for (int i = 0; i < effects_alr->num_backpack_effects; i++)
    {
        effects_handles_reset(&handles->backpack_effects_handles[i]);
    }
    for (int i = 0; i < effects_alr->num_weapon_effects; i++)
    {
        effects_handles_reset(&handles->weapon_effects_handles[i]);
    }
    handles->arc_handle = 0xFFFFFFFF;
    handles->arc_position = 0.0f;
}

// 0x00958760 (:2361)
void multi_object_backpack::effects_handles_reset(effects_alr_handles::effects_handles* handles)
{
    handles->attack_effect_handle = 0xFFFFFFFF;
    handles->loop_effect_handle = 0xFFFFFFFF;
}

// 0x0099A9A0 (:2367): full fuel, off, unattached, the timers and effect handles cleared
void multi_object_backpack::reset()
{
    stop_ramp_up();
    stop_material_effects();
    fuel_timer.invalidate();
    activated_time.invalidate();
    stealth_timestamp.invalidate();
    time_until_restealth.invalidate();
    jump_boost_timer.invalidate();
    jump_boost_delay_timer.invalidate();
    shockwave_timer.invalidate();
    ripple_timer.invalidate();
    rhino_shape_cutter_timer.invalidate();
    rhino_mover_hit_effect_timer.invalidate();
    vision_vfx_timer.invalidate();
    fuel = 1.0f;
    fuel_last_frame = 1.0f;
    activated = false;
    in_stealth = false;
    player_attached_to = nullptr;
    jump_boost_horizontal = 0.0f;
    jump_boost_vertical = 0.0f;
    jump_boost_direction_up = true;
    jump_boost_downthrust_hit_ground = false;
    use_alternate_ramp_up_effects = false;
    use_alternate_backpack_activation_effects = false;
    effects_handles_alr_reset(&backpack_info->pickup_effects, &pickup_effects_alr_handles);
    if (backpack_info->activatable)
    {
        effects_handles_alr_reset(&backpack_info->activation_effects, &activation_effects_alr_handles);
    }
    for (int i = 0; i < 16; i++)
    {
        rhino_hit_players[i] = false;
    }
    num_rhino_hit_npcs = 0;
}

// 0x009587D0 (:2419): the server lets a client's backpack (its fuel lags) activate with a little less
bool multi_object_backpack::has_enough_fuel_to_activate() const
{
    float fudge = 0.0f;
    if (Multiplayer && Server)
    {
        fudge = player_attached_to == player_get_local_player() ? 0.0f : SERVER_FUEL_FUDGE_FACTOR;
    }
    const float available = fuel + fudge;
    return backpack_info->fuel_activation_cost <= available && backpack_info->minimum_fuel_to_activate <= available;
}

// 0x00958850 (:2435)
multi_backpack_toggle_reasons multi_object_backpack::can_activate() const
{
    if (player_attached_to == nullptr)
    {
        return MBTR_UNATTACHED;
    }
    multi_backpack_toggle_reasons reason = MBTR_OK;
    if (!backpack_info->activatable)
    {
        reason = MBTR_NON_ACTIVATABLE;
    }
    else if (ramp_up_timer.valid())
    {
        reason = MBTR_RAMPING_UP;
    }
    else if (!has_enough_fuel_to_activate())
    {
        reason = MBTR_LOW_FUEL;
    }
    else if (human_is_ragdolled_or_getting_up(player_attached_to))
    {
        reason = MBTR_RAGDOLLED;
    }
    else if (wrecking_crew_is_active() && !wrecking_crew_can_player_activate_backpack(backpack_info))
    {
        reason = MBTR_WRECKING_CREW;
    }
    // The rhino does not start during a melee move, a throw or a movement blocking animation (but landing).
    if (backpack_info->active_abilities.rhino)
    {
        const int block_anim = player_attached_to->block_movement_anim;
        if (player_attached_to->combat.cur_melee_combat_move != nullptr || human_is_throwing_weapon(player_attached_to) || (block_anim != -1 && block_anim != ANIM_ALIAS_LAND))
        {
            reason = MBTR_RAGDOLLED;
        }
    }
    return reason;
}

// 0x00958930 (:2476)
multi_backpack_toggle_reasons multi_object_backpack::can_deactivate() const
{
    if (player_attached_to == nullptr)
    {
        return MBTR_UNATTACHED;
    }
    if (!backpack_info->activatable)
    {
        return MBTR_NON_ACTIVATABLE;
    }
    if (!backpack_info->player_deactivatable)
    {
        return MBTR_PLAYER_NON_DEACTIVATABLE;
    }
    return MBTR_OK;
}

// 0x009F84B0 (:2497): off when on; otherwise the ramp up starts (a client activates at once: the server delayed it)
void multi_object_backpack::toggle_activation(int activation_delay_ms)
{
    if (player_attached_to == nullptr)
    {
        return;
    }
    if (activated)
    {
        deactivate();
        return;
    }
    const char player_xp_level = gamelog_get_player_xp_level(player_attached_to);
    const vector player_pos = gamelog_get_human_pos(player_attached_to);
    const int duration = gamelog_get_game_duration();
    const char difficulty = gamelog_get_difficulty();
    const int locale_id = gamelog_get_locale();
    const char player_index = gamelog_get_player_index((const human*)player_attached_to);
    const int time = gamelog_get_logging_time();
    log_event_backpack_used(gamelog_get_player_checksum(), time, player_index, locale_id, difficulty, duration, player_pos, player_xp_level);
    if (Multiplayer && !Server)
    {
        activate();
        return;
    }
    start_ramp_up(activation_delay_ms);
}

// 0x009F2300 (:2515): on: the local player leaves cover and fine aim, the activation cost is paid, the fuel effect and
// the abilities start (kaboom, the charge animation, the rhino's run, the ripple, the jump boost's jump), the fuel timer
// runs until the fuel is spent, the spawn invulnerability ends. A backpack that cannot be deactivated is off again at
// once (a one-shot).
void multi_object_backpack::activate()
{
    player* pp = player_attached_to;
    if (pp == player_get_local_player())
    {
        if (backpack_info->no_cover)
        {
            player_exit_cover(pp, false);
        }
        if (backpack_info->no_fine_aim && player_is_zoomed(player_attached_to))
        {
            player_set_zoom_state(player_attached_to, PZS_OFF, false);
        }
    }
    const multi_backpack_info* binfo = backpack_info;
    if (binfo->uses_fuel)
    {
        fuel = fuel - binfo->fuel_activation_cost;
    }
    activated = true;
    use_alternate_backpack_activation_effects = false;
    stop_ramp_up();
    if (object_is_local_player(player_attached_to) && backpack_info->activation_camera_shake != nullptr)
    {
        camera_shake_play(backpack_info->activation_camera_shake, 1.0f, false);
    }
    World.destroy_object(fuel_effect_object, false);
    fuel_effect_object = effects_play(Backpack_fuel_effect_active, get_handle(), fuel_effect_attach_tag_index, true, 0xFFFFFFFF, 1.0f, false);
    current_fuel_effect = Backpack_fuel_effect_active;
    activate_dual_abilities();
    if (backpack_info->kaboom)
    {
        use_kaboom();
    }
    if (binfo->active_abilities.charge)
    {
        // Magic number: the charge animation blends in over 0.3 s.
        player_attached_to->char_inst->set_state(index_alias(ANIMATION_STATE_MP_RHINO, (vanim_aliases)-1), index_alias(-1, (vanim_aliases)-1), 0.3f, false);
        human_override_default_animation_state(player_attached_to, true);
    }
    if (binfo->active_abilities.rhino)
    {
        human_melee_cancel_attack(player_attached_to);
        human_weapons_cancel_reload(player_attached_to);
        human_weapons_firearm_release_trigger(player_attached_to);
        human_weapons_firearm_release_secondary_weapon_trigger(player_attached_to);
        rhino_hit_mover_this_run = false;
        for (int i = 0; i < 16; i++)
        {
            rhino_hit_players[i] = false;
        }
        num_rhino_hit_npcs = 0;
    }
    if (binfo->active_abilities.ripple)
    {
        use_ripple(player_attached_to->pos);
    }
    if (binfo->active_abilities.jump_boost)
    {
        jump_boost_horizontal = binfo->active_abilities.jump_boost_horizontal;
        jump_boost_downthrust_hit_ground = false;
        jump_boost_vertical = binfo->active_abilities.jump_boost_vertical;
        int delay = 0;
        if (!human_is_in_air(player_attached_to))
        {
            player_attached_to->ground_obj_normal = 1.0f;
            human_jump(player_attached_to, false, false, false);
            delay = binfo->active_abilities.jump_boost_delay_ms;
        }
        jump_boost_delay_timer.set(delay);
        use_alternate_backpack_activation_effects = !jump_boost_direction_up;
    }
    memset(heal_amounts, 0, sizeof(heal_amounts));
    int fuel_ms = 0;
    if (backpack_info->uses_fuel)
    {
        fuel_ms = (int)((fuel / backpack_info->fuel_use_per_second) * 1000.0f);
    }
    fuel_timer.set(fuel_ms);
    activated_time.set(fuel_timer.time_until());
    const bool player_effect_stopped = player_stop_material_effect(player_attached_to);
    player_set_player_invulnerable(player_attached_to, false);
    start_material_effects(player_effect_stopped);
    effects_alr_attack(&backpack_info->activation_effects, &activation_effects_alr_handles);
    if (backpack_info->activatable && !backpack_info->deactivatable)
    {
        activated = false;
    }
    if (wrecking_crew_is_active())
    {
        wrecking_crew_register_backpack_activation();
    }
}

// 0x0099AAC0 (:2656): off: the activation effects released; a deactivatable backpack undoes its abilities (the charge's
// speed and animation, the rhino's last cut where it stopped, the downthrust's landing effect: its fuel is spent)
void multi_object_backpack::deactivate()
{
    const multi_backpack_info* binfo = backpack_info;
    const multi_backpack_info::abilities* abilities = activated ? &binfo->active_abilities : &binfo->passive_abilities;
    effects_alr_release(&binfo->activation_effects, &activation_effects_alr_handles);
    if (backpack_info->deactivatable)
    {
        camera_shake_stop();
        deactivate_dual_abilities();
        if (abilities->charge)
        {
            player_attached_to->move_speed = 0.0f;
            human_override_default_animation_state(player_attached_to, false);
        }
        if (abilities->rhino && (!Multiplayer || Server))
        {
            player* pp = player_attached_to;
            // Magic numbers: a 0.76 m cut 0.95 m above the feet.
            const vector cut_pos(UP_VECTOR.x * 0.95f + pp->pos.x, pp->pos.y + UP_VECTOR.y * 0.95f, pp->pos.z + UP_VECTOR.z * 0.95f);
            shape_cutter_props props(cut_pos, pp->orient, 0.76f, Rhino_shape_cutter_explosion, pp->get_handle(), pp->get_handle());
            World.create_object<shape_cutter>(props, 0xFFFFFFFF);
        }
        if (abilities->jump_boost && jump_boost_downthrust_hit_ground)
        {
            if (abilities->jump_boost_downthrust_ground_hit_effect != nullptr)
            {
                effects_play(abilities->jump_boost_downthrust_ground_hit_effect, player_attached_to->pos, IDENTITY_MATRIX, false, 0xFFFFFFFF, -1, 0xFFFFFFFF, 1.0f, false, EFFECTS_CATEGORY_NONE);
            }
            fuel = 0.0f;
        }
        player_attached_to->jetpack_fuel_percent = 0.0f;
        fuel_timer.invalidate();
        activated_time.invalidate();
        if (!stealth_timestamp.valid())
        {
            stop_material_effects();
        }
    }
    activated = false;
}

// 0x0097D050 (:2737)
void multi_object_backpack::set_backpack_rack(multi_object_marker* backpack_rack_in, int backpack_rack_index_in)
{
    backpack_rack = backpack_rack_in;
    backpack_rack_index = backpack_rack_index_in;
}

// 0x009F8570 (:2747): a player touched the backpack: one of an enemy team's rack is not for it, nor one behind
// something (seen from 1.4 m above its feet); a player with a backpack asks to swap for a rack's one (and skips the same
// kind); otherwise the server gives it the backpack
bool multi_object_backpack::event_player_touched(player* pp, bool& ask_pickup)
{
    if (!obj_flags.streamed)
    {
        return false;
    }
    ask_pickup = false;
    if (pp == nullptr)
    {
        return false;
    }
    if (backpack_rack != nullptr)
    {
        const human_teams rack_team = backpack_rack->team;
        const human_teams player_team = pp->current_team;
        if (rack_team != HUMAN_TEAM_CIVILIAN && player_team != HUMAN_TEAM_CIVILIAN && human_teams_are_enemies(rack_team, player_team))
        {
            return false;
        }
        raycast_input in;
        // Magic number: from 1.4 m above the feet.
        in.start = pp->pos + UP_VECTOR * 1.4f;
        in.radius = 0.0f;
        // Original: the ray ends at the backpack's x and z and the rack's height.
        in.end.set(pos.x, backpack_rack->pos.y, pos.z);
        in.m_filter_info = COLLIDE_RAY_DEFAULT;
        in.filter_params = nullptr;
        in.filter_callback = nullptr;
        in.m_hit_collector_type = raycast_input::COLLECT_CLOSEST_HIT;
        in.m_flags = 0;
        raycast_output out;
        if (havok_raycast(in, out))
        {
            const unsigned int my_handle = get_handle();
            const bool hit_me = out.m_hits[0].hit_object_handle == my_handle;
            const bool hit_rack_then_me = out.m_hits.getSize() >= 2 && out.m_hits[0].hit_object_handle == backpack_rack->get_parent() && out.m_hits[1].hit_object_handle == my_handle;
            if (!hit_me && !hit_rack_then_me)
            {
                return false;
            }
        }
    }
    if (pp->backpack != nullptr)
    {
        if (pp->backpack->backpack_info->unique_id == backpack_info->unique_id)
        {
            return false;
        }
        if (backpack_rack != nullptr)
        {
            ask_pickup = true;
            return false;
        }
    }
    if (!Multiplayer || Server)
    {
        event_player_picked_up(pp, true, false);
    }
    return false;
}

// 0x009F26A0 (:2823): the player takes the backpack: its old one is dropped first (the server), it leaves its rack (a
// new one is hung there when it is dropped), the player wears it, the pickup effects play; the server logs the equip
// and tells everyone (server_sync). A migrating host only logs.
void multi_object_backpack::event_player_picked_up(player* pp, bool server_sync, bool migrating)
{
    if (!obj_flags.streamed || pp == nullptr)
    {
        return;
    }
    if (!migrating)
    {
        if ((!Multiplayer || Server) && pp->backpack != nullptr)
        {
            if (Multiplayer && Server)
            {
                log_stop_event_backpack_equipped(pp->backpack_equip_log_handle);
                pp->backpack_equip_log_handle = 0;
            }
            pp->backpack->event_player_dropped(true);
        }
        if (backpack_rack != nullptr)
        {
            multi_game_backpack_rack_backpack_picked_up(backpack_rack, this);
        }
        reset();
        pp->backpack = this;
        player_attached_to = pp;
        human_wear_prop(pp, this);
        activate_dual_abilities();
        if (!wrecking_crew_is_active())
        {
            effects_alr_attack(&backpack_info->pickup_effects, &pickup_effects_alr_handles);
        }
        if (pp == player_get_local_player())
        {
            hud_ui_multiplayer_local_player_picked_up_backpack();
        }
    }
    if (Multiplayer && Server)
    {
        const unsigned int event_handle = (unsigned int)pp->backpack_equip_log_handle;
        if (event_handle != 0)
        {
            if (log_record_event(LOG_EVENT_BACKPACK_EQUIPPED))
            {
                log_stop_continuous_event(event_handle);
            }
            pp->backpack_equip_log_handle = 0;
        }
        const char player_xp_level = gamelog_get_player_xp_level(pp);
        const int backpack_crc = gamelog_get_backpack_crc(this);
        const char player_index = gamelog_get_player_index((const human*)pp);
        const int time = gamelog_get_logging_time();
        pp->backpack_equip_log_handle = log_start_event_backpack_equipped(gamelog_get_player_checksum(), time, player_index, backpack_crc, player_xp_level);
        if (server_sync)
        {
            send_backpack_picked_up(pp, this);
        }
    }
}

// 0x009F2810 (:2891): the player died: the server may set off a kaboom (by its death chance); it is turned off, its
// rack gets a new one, and the server tells everyone. The backpack stays on the body until the player respawns.
void multi_object_backpack::event_player_died()
{
    if (player_attached_to == nullptr)
    {
        return;
    }
    if (player_attached_to == player_get_local_player())
    {
        hud_ui_multiplayer_local_player_dropped_backpack();
    }
    if ((!Multiplayer || Server) && backpack_info->kaboom && !activated && fl_rand() <= backpack_info->kaboom_death_chance)
    {
        activate();
    }
    if (activated)
    {
        deactivate();
    }
    stop_ramp_up();
    if (backpack_rack != nullptr)
    {
        multi_game_backpack_rack_backpack_dropped(backpack_rack, player_attached_to->backpack);
    }
    if (Multiplayer && Server)
    {
        send_player_died_with_backpack(player_attached_to);
        const unsigned int event_handle = (unsigned int)player_attached_to->backpack_equip_log_handle;
        if (log_record_event(LOG_EVENT_BACKPACK_EQUIPPED))
        {
            log_stop_continuous_event(event_handle);
        }
        const char player_xp_level = gamelog_get_player_xp_level(player_attached_to);
        const int backpack_crc = gamelog_get_backpack_crc(player_attached_to->backpack);
        const char player_index = gamelog_get_player_index((const human*)player_attached_to);
        const int time = gamelog_get_logging_time();
        player_attached_to->backpack_equip_log_handle = log_start_event_backpack_equipped(gamelog_get_player_checksum(), time, player_index, backpack_crc, player_xp_level);
    }
}

// 0x009F2960 (:2933): a respawned player loses its backpack (the server drops it for everyone)
void multi_object_backpack::event_player_respawned()
{
    if (player_attached_to != nullptr && (!Multiplayer || Server))
    {
        event_player_dropped(true);
    }
}

// 0x009DF420 (:2945): server: an active stealth backpack turns off when its player is attacked (not by an explosion)
void multi_object_backpack::event_player_attacked(damaged_by_types attack_type)
{
    if (!Multiplayer || !Server)
    {
        return;
    }
    if (get_backpack_abilities()->stealth && activated && attack_type != DBT_EXPLOSION)
    {
        deactivate();
        send_backpack_toggled_activation(player_attached_to, MBTR_OK);
    }
}

// 0x009DF480 (:2963): the backpack comes off its player and is destroyed (the server tells everyone); its rack gets a
// new one
void multi_object_backpack::event_player_dropped(bool server_sync)
{
    if (player_attached_to == nullptr)
    {
        return;
    }
    if (Multiplayer && Server && server_sync)
    {
        send_backpack_dropped(player_attached_to, this);
    }
    if (activated)
    {
        deactivate();
    }
    stop_ramp_up();
    effects_alr_release(&backpack_info->pickup_effects, &pickup_effects_alr_handles);
    if (vision_vfx_timer.valid())
    {
        game_render_set_camera_frame_override_blur(0.0f);
        game_render_set_camera_signal_noise_frequency(0.0f);
        game_render_set_camera_signal_noise_scale(0.0f);
    }
    if (stealth_timestamp.valid())
    {
        stop_material_effects();
    }
    player_attached_to->remove_child(this);
    player_attached_to->backpack = nullptr;
    player_attached_to = nullptr;
    if (backpack_rack != nullptr)
    {
        multi_game_backpack_rack_backpack_dropped(backpack_rack, this);
    }
    World.destroy_object(this, false);
}

// 0x009B0860 (:3012): the player left the match: its rack gets a new one and the backpack is destroyed
void multi_object_backpack::event_player_left()
{
    if (player_attached_to == nullptr)
    {
        return;
    }
    if (backpack_rack != nullptr)
    {
        multi_game_backpack_rack_backpack_dropped(backpack_rack, this);
    }
    World.destroy_object(this, false);
}

// 0x0097D070 (:3028): the ramp up: the timer, the local player's jump boost direction (the alternate effects for a
// downthrust), and for the local player the ramp up effect and camera shake
void multi_object_backpack::start_ramp_up(int delay_ms)
{
    if (player_attached_to == nullptr)
    {
        return;
    }
    use_alternate_ramp_up_effects = false;
    ramp_up_timer.set(delay_ms);
    stop_material_effects();
    if (player_attached_to == player_get_local_player())
    {
        prepare_type_specific_info_for_toggle_packet();
    }
    if (!jump_boost_direction_up)
    {
        use_alternate_ramp_up_effects = true;
    }
    player* pp = player_attached_to;
    if (pp != player_get_local_player())
    {
        return;
    }
    const multi_backpack_info::ramp_up_effects* ramp_up = use_alternate_ramp_up_effects ? &backpack_info->alternate_ramp_up : &backpack_info->ramp_up;
    if (ramp_up->ramp_up_player_effect != nullptr)
    {
        const int tag = pp->get_tag_index_from_name(ramp_up->ramp_up_effect_attach_point_name);
        ramp_up_effect_object = effects_play(ramp_up->ramp_up_player_effect, player_attached_to->pos, player_attached_to->orient, false, player_attached_to->get_handle(), tag, 0xFFFFFFFF, 1.0f, false, EFFECTS_CATEGORY_NONE);
    }
    else if (ramp_up->ramp_up_backpack_effect != nullptr)
    {
        ramp_up_effect_object = effects_play(ramp_up->ramp_up_backpack_effect, get_handle(), get_tag_index_from_name(ramp_up->ramp_up_effect_attach_point_name), false, 0xFFFFFFFF, 1.0f, false);
    }
    if (ramp_up->ramp_up_camera_shake != nullptr)
    {
        camera_shake_play(ramp_up->ramp_up_camera_shake, 1.0f, false);
    }
}

// 0x009F2990 (:3068): the ramp up's end: the server (or single player) activates when it still can and tells everyone
// (a refusal to the player's client)
void multi_object_backpack::process_ramp_up()
{
    if (player_attached_to == nullptr || !ramp_up_timer.valid() || !ramp_up_timer.elapsed())
    {
        return;
    }
    stop_ramp_up();
    if (Multiplayer && !Server)
    {
        return;
    }
    const multi_backpack_toggle_reasons reason = can_activate();
    send_backpack_toggled_activation(player_attached_to, reason);
    if (reason == MBTR_OK)
    {
        activate();
    }
}

// 0x0097D1A0 (:3089)
void multi_object_backpack::stop_ramp_up()
{
    const multi_backpack_info::ramp_up_effects* ramp_up = use_alternate_ramp_up_effects ? &backpack_info->alternate_ramp_up : &backpack_info->ramp_up;
    ramp_up_timer.invalidate();
    if (ramp_up->ramp_up_camera_shake != nullptr)
    {
        camera_shake_stop(ramp_up->ramp_up_camera_shake);
    }
    stop_effect_object(ramp_up_effect_object);
    ramp_up_effect_object = 0xFFFFFFFF;
}

// 0x00958970 (:3113): the player fades out over the stealth time (from where a fade in stopped)
void multi_object_backpack::stealth_player()
{
    if (player_attached_to == nullptr)
    {
        return;
    }
    const multi_backpack_info::abilities* abilities = get_backpack_abilities();
    if (in_stealth)
    {
        return;
    }
    in_stealth = true;
    float stealth;
    if (stealth_timestamp.valid())
    {
        stealth = 1.0f - stealth_timestamp.percent();
        stealth_timestamp.set(abilities->stealth_time_ms, 1.0f - stealth_timestamp.percent());
    }
    else
    {
        stealth = 0.0f;
        stealth_timestamp.set(abilities->stealth_time_ms);
    }
    player_attached_to->set_stealth_plus_child_items(stealth);
}

// 0x00958A40 (:3137): the player fades in (in three quarters of the stealth time) and may stealth again after the
// restealth time
void multi_object_backpack::unstealth_player()
{
    if (player_attached_to == nullptr)
    {
        return;
    }
    time_until_restealth.set(get_backpack_abilities()->restealth_time_ms);
    if (!in_stealth)
    {
        return;
    }
    in_stealth = false;
    const multi_backpack_info::abilities* abilities = get_backpack_abilities();
    // Magic number: the fade in takes three quarters of the fade out's time.
    const int fade_in_ms = (int)((double)abilities->stealth_time_ms * 0.75);
    float stealth;
    if (stealth_timestamp.valid())
    {
        stealth = stealth_timestamp.percent();
        stealth_timestamp.set(fade_in_ms, 1.0f - stealth_timestamp.percent());
    }
    else
    {
        stealth_timestamp.set(fade_in_ms);
        stealth = 1.0f;
    }
    player_attached_to->set_stealth_plus_child_items(stealth);
}

// 0x00958B50 (:3164)
bool multi_object_backpack::is_fully_stealthed() const
{
    if (player_attached_to == nullptr)
    {
        return false;
    }
    return get_backpack_abilities()->stealth && !time_until_restealth.valid() && !stealth_timestamp.valid() && in_stealth;
}

// 0x00958BB0 (:3172)
bool multi_object_backpack::is_stealth_active() const
{
    if (player_attached_to == nullptr)
    {
        return false;
    }
    const float fading = stealth_timestamp.valid() ? 1.0f - stealth_timestamp.percent() : 0.0f;
    return get_backpack_abilities()->stealth || in_stealth || fading > 0.0f;
}

// 0x00958C40 (:3210)
void multi_object_backpack::attempt_shockwave(const vector& posp)
{
    if (player_attached_to == nullptr)
    {
        return;
    }
    const multi_backpack_info::abilities* abilities = get_backpack_abilities();
    if (abilities->shockwave && (!shockwave_timer.valid() || shockwave_timer.elapsed()))
    {
        shockwave_timer.set(abilities->shockwave_timer_ms);
        use_shockwave(posp);
    }
}

// 0x009F2A00 (:3230)
void multi_object_backpack::attempt_ripple(const vector& posp)
{
    if (player_attached_to == nullptr)
    {
        return;
    }
    const multi_backpack_info::abilities* abilities = get_backpack_abilities();
    if (abilities->ripple && (!ripple_timer.valid() || ripple_timer.elapsed()))
    {
        ripple_timer.set(abilities->ripple_timer_ms);
        use_ripple(posp);
    }
}

// 0x00958CB0 (:3251): the local player's jump boost goes up unless the jump button is held (the downthrust)
void multi_object_backpack::prepare_type_specific_info_for_toggle_packet()
{
    if (player_attached_to != nullptr && backpack_info->active_abilities.jump_boost && player_attached_to == player_get_local_player())
    {
        jump_boost_direction_up = !controls_is_button_down(CBA_OFC_JUMP, Player_controller_index);
    }
}

// 0x00958D00 (:3278)
void multi_object_backpack::write_type_specific_info_to_toggle_packet(unsigned char* packet_data, unsigned short& packet_size) const
{
    if (player_attached_to == nullptr || !backpack_info->active_abilities.jump_boost)
    {
        return;
    }
    packet_data[packet_size++] = jump_boost_direction_up;
    packet_data[packet_size++] = jump_boost_downthrust_hit_ground;
}

// 0x00958D60 (:3300)
void multi_object_backpack::read_type_specific_info_from_toggle_packet(const unsigned char* packet_data, unsigned short& packet_offset)
{
    if (player_attached_to == nullptr || !backpack_info->active_abilities.jump_boost)
    {
        return;
    }
    jump_boost_direction_up = packet_data[packet_offset++] != 0;
    jump_boost_downthrust_hit_ground = packet_data[packet_offset++] != 0;
}

// 0x009F2A70 (:3321): every frame: the ramp up, the material effects, stealth, the heal and repair, the charge speed,
// the rhino's run, the field of view, the jump boost (its delayed push; the downthrust ends on the ground), the jetpack
// fuel, the kaboom delay, the vision effect, the fuel (spent while on, else regenerated), the loop effects, the
// weapon's material effect and the fuel gauge
void multi_object_backpack::process()
{
    if (player_attached_to != nullptr)
    {
        process_ramp_up();
        const multi_backpack_info::abilities* abilities = get_backpack_abilities();
        const float fuel_seconds_left = (float)fuel_timer.time_until() / 1000.0f;
        if (activated && !mat_fx_succeeded)
        {
            start_material_effects(false);
        }
        if (abilities->stealth)
        {
            if (time_until_restealth.elapsed())
            {
                time_until_restealth.invalidate();
                stealth_player();
            }
            if (!stealth_timestamp.valid() && !time_until_restealth.valid())
            {
                player_attached_to->set_stealth_plus_child_items(in_stealth ? 1.0f : 0.0f);
            }
        }
        if (stealth_timestamp.valid())
        {
            const float percent = stealth_timestamp.percent();
            player_attached_to->set_stealth_plus_child_items(in_stealth ? 1.0f - percent : percent);
            if (stealth_timestamp.elapsed())
            {
                stealth_timestamp.invalidate();
                if (!in_stealth)
                {
                    stop_material_effects();
                }
            }
        }
        if (abilities->health_regen && (!Multiplayer || Server))
        {
            const float heal = abilities->health_regen_per_second * Frametime;
            unsigned int handles[16];
            const int num = World.collect_objects<player>(player_attached_to->pos, abilities->health_regen_radius, handles, 16, nullptr, nullptr);
            for (int i = 0; i < num; i++)
            {
                object* objp = handles[i] != 0xFFFFFFFF ? World.object_from_handle(handles[i]) : nullptr;
                player* pp = objp != nullptr && objp->get_obj_type() == OT_HUMAN && objp->get_sub_type() == OT_SUB_HUMAN_PLAYER ? (player*)objp : nullptr;
                // Original: the cast result is not checked for null.
                if (pp->is_killed())
                {
                    continue;
                }
                if (human_teams_get_disposition(pp->current_team, player_attached_to->current_team) <= TEAM_DISPOSITION_NONE && !abilities->health_regen_heal_enemies && pp != player_attached_to)
                {
                    continue;
                }
                const float hit_points = pp->hit_points;
                float amount = (float)pp->max_hit_points - hit_points;
                if (heal - amount < 0.0f)
                {
                    amount = heal;
                }
                pp->hit_points = hit_points + amount;
                if (Multiplayer && Server && pp != player_attached_to && pp->mp_data != 0xFFFFFFFF)
                {
                    const unsigned int index = pp->mp_data & 0xFFFF;
                    heal_amounts[index] = heal_amounts[index] + amount;
                    // Healing a teammate by the event's share of its health is worth the heal teammate experience.
                    const float threshold = multi_game_xp_get_event_parameter(MXE_HEAL_TEAMMATE) * (float)pp->max_hit_points;
                    if (threshold <= heal_amounts[index])
                    {
                        multi_game_xp_trigger_event(player_attached_to->mp_data, MXE_HEAL_TEAMMATE, 1, 0xFF, -1, false);
                        heal_amounts[index] = heal_amounts[index] - threshold;
                    }
                }
            }
        }
        if (abilities->repair && (!Multiplayer || Server))
        {
            static timestamp repair_timestamp; // 0x03017C38
            if (!repair_timestamp.valid() || repair_timestamp.elapsed())
            {
                const float offset = abilities->repair_offset;
                const player* pp = player_attached_to;
                const vector repair_pos(pp->pos.x + offset * Rfg_camera.real_orient.fvec.x, pp->pos.y + offset * Rfg_camera.real_orient.fvec.y, pp->pos.z + offset * Rfg_camera.real_orient.fvec.z);
                // Magic numbers: a 100 ms repair every 100 ms.
                rfg_dyn_repair_sphere(repair_pos, abilities->repair_radius, 100, player_attached_to);
                repair_timestamp.set(100);
            }
        }
        if (abilities->charge)
        {
            // The speed ramps up (squared) after activating and down as the fuel runs out.
            float factor = 1.0f;
            const float since = (float)activated_time.time_since_set() / 1000.0f;
            const float ramp_up = abilities->charge_ramp_up_time_seconds;
            if (since < ramp_up && ramp_up != 0.0f)
            {
                factor = powf(since / ramp_up, 2.0f);
            }
            else
            {
                const float ramp_down = abilities->charge_ramp_down_time_seconds;
                if (fuel_seconds_left < ramp_down && ramp_down != 0.0f)
                {
                    factor = fuel_seconds_left / ramp_down;
                }
            }
            if (factor - 0.0f < 0.0f)
            {
                factor = 0.0f;
            }
            player_attached_to->move_speed = abilities->charge_top_speed * factor;
        }
        if (abilities->rhino && (!Multiplayer || Server) && (rhino_shape_cutter_timer.elapsed() || !rhino_shape_cutter_timer.valid()))
        {
            // The cast starts half a body behind the player's chest and reaches
            // as far as the player moves in the look ahead time (0.35 s for the local player, 0.45 s for a remote one, plus 0.1 s,
            // at most the fuel left but at least 0.4 s; a remote player's latency twice on top; at least 0.1 s). The first hit:
            // a building gets a 0.95 m cut ahead of the hit (and the hit effect every 0.5 s); a player, npc or walker is pushed
            // away at the hit speed (at least the minimum upwards). Without a hit the cast waits 0.1 s.
            player* pp = player_attached_to;
            vector vel;
            human_controller_get_vel(pp, vel);
            // Magic numbers: the look ahead times (s).
            float look_ahead = (pp == player_get_local_player() ? 0.35f : 0.45f) + 0.1f;
            float limit = fuel_seconds_left;
            if (fuel_seconds_left - 0.4f < 0.0f)
            {
                limit = 0.4f;
            }
            if (0.0f <= look_ahead - limit)
            {
                look_ahead = limit;
            }
            if (pp != player_get_local_player())
            {
                const unsigned int latency_ms = (unsigned int)(multi_core_qos_find_latency_by_game_data(pp->mp_data) * 2);
                look_ahead = (float)latency_ms / 1000.0f + look_ahead;
            }
            if (look_ahead - 0.1f < 0.0f)
            {
                look_ahead = 0.1f;
            }
            // Magic number: the player's body is 0.95 m (the cast's sphere radius 0.855, 90% of it).
            const float length = (sqrtf(vel.y * vel.y + vel.x * vel.x + vel.z * vel.z) * look_ahead) / 0.95f;
            const vector dir = vel.copy_normalize_safe();
            rhino_cast_info cast_info;
            cast_info.hitter = pp;
            cast_info.hit_something = false;
            cast_info.nearly_spent = fuel_seconds_left < 0.2f;
            raycast_input in;
            const float center_y = (pp->pos.y + 0.95f) - dir.y * 0.95f * 0.5f;
            in.start.x = (pp->pos.x + 0.0f) - dir.x * 0.95f * 0.5f;
            in.start.y = center_y + 0.09500002f;
            in.start.z = (pp->pos.z + 0.0f) - dir.z * 0.95f * 0.5f;
            in.end.x = dir.x * length + in.start.x;
            in.end.y = dir.y * length + center_y + 0.09500002f;
            in.end.z = dir.z * length + in.start.z;
            in.radius = 0.855f;
            in.filter_params = &cast_info;
            in.filter_callback = rhino_cast_filter_callback;
            in.m_filter_info = COLLIDE_RAY_DEFAULT;
            in.m_hit_collector_type = raycast_input::COLLECT_CLOSEST_HIT;
            in.m_flags = 0;
            raycast_output out;
            if (havok_spherecast(in, out))
            {
                object* objp = out.m_hits.getSize() >= 1 ? World.object_from_handle(out.m_hits[0].hit_object_handle) : nullptr;
                // Original: a hit without an object (or a player hit that is not a player) ends process for this
                // frame (the rest of it is skipped).
                if (objp == nullptr)
                {
                    return;
                }
                const vector& hit_center = out.m_hits[0].hit_center_pos;
                if (objp->get_obj_type() == OT_SHAPE_CUTTER)
                {
                    // Nothing to do: the cut is there already.
                }
                else if (objp->get_obj_type() == OT_MOVER)
                {
                    // Magic number: the cut half a body ahead of the hit.
                    const vector cut_pos(dir.x * 0.475f + hit_center.x, dir.y * 0.475f + hit_center.y, dir.z * 0.475f + hit_center.z);
                    shape_cutter_props props(cut_pos, pp->orient, 0.95f, Rhino_shape_cutter_explosion, pp->get_handle(), pp->get_handle());
                    World.create_object<shape_cutter>(props, 0xFFFFFFFF);
                    rhino_hit_mover_this_run = true;
                    if (!rhino_mover_hit_effect_timer.valid() || rhino_mover_hit_effect_timer.elapsed())
                    {
                        if (abilities->rhino_mover_hit_effect != nullptr)
                        {
                            effects_play(abilities->rhino_mover_hit_effect, cut_pos, pp->orient, false, 0xFFFFFFFF, -1, 0xFFFFFFFF, 1.0f, false, EFFECTS_CATEGORY_NONE);
                        }
                        // Magic number: the building hit effect at most every 0.5 s.
                        rhino_mover_hit_effect_timer.set(500);
                    }
                }
                else
                {
                    vector velocity = objp->pos - hit_center;
                    velocity.normalize_safe();
                    const float speed = abilities->rhino_player_hit_speed;
                    velocity.z = speed * velocity.z;
                    float vertical = speed * velocity.y;
                    if (speed * velocity.y - abilities->rhino_player_minimum_y_velocity < 0.0f)
                    {
                        vertical = abilities->rhino_player_minimum_y_velocity;
                    }
                    velocity.y = vertical;
                    velocity.x = velocity.x * speed;
                    if (objp->get_obj_type() == OT_HUMAN && objp->get_sub_type() == OT_SUB_HUMAN_PLAYER)
                    {
                        player* hit_player = World.object_cast<player>(objp);
                        if (hit_player == nullptr)
                        {
                            return;
                        }
                        rhino_process_player_hit(pp, hit_player, velocity);
                        if (hit_player->mp_data != 0xFFFFFFFF)
                        {
                            rhino_hit_players[hit_player->mp_data & 0xFFFF] = true;
                        }
                    }
                    else if (objp->get_obj_type() == OT_HUMAN && objp->get_sub_type() == OT_SUB_HUMAN_NPC)
                    {
                        // Magic number: the npc list holds 64.
                        if (!Multiplayer && !wrecking_crew_is_active() && dlc_get_current_id() == -1 && num_rhino_hit_npcs <= 63)
                        {
                            rhino_process_npc_hit(pp, (human*)objp, velocity);
                            rhino_hit_npcs[num_rhino_hit_npcs] = objp->get_handle();
                            num_rhino_hit_npcs++;
                        }
                    }
                    else if (objp->get_obj_type() == OT_VEHICLE && objp->get_sub_type() == OT_SUB_VEHICLE_WALKER)
                    {
                        rhino_process_vehicle_hit(pp, (vehicle*)objp, velocity, &out);
                        // Original: the walkers go into the npc list, unbounded.
                        rhino_hit_npcs[num_rhino_hit_npcs] = objp->get_handle();
                        num_rhino_hit_npcs++;
                    }
                }
            }
            if (!cast_info.hit_something)
            {
                // Magic number: the next cast in 0.1 s.
                rhino_shape_cutter_timer.set(100);
            }
        }
        if (abilities->fov_scale != 1.0f && player_attached_to == player_get_local_player())
        {
            // The field of view follows the movement stick towards its scale, at most 0.3 degrees a frame.
            const float stick = sqrtf(Stick_v * Stick_v + Stick_h * Stick_h);
            const float scale = abilities->fov_scale;
            const float target = 1.0f <= scale ? (scale - 1.0f) * stick + 1.0f : (1.0f - scale) * stick + scale;
            const float current = camera_view_get_current_fov();
            float delta = (float)camera_view_get_fov() * target - current;
            if (delta - -0.3f < 0.0f)
            {
                delta = -0.3f;
            }
            if (0.0f <= delta - 0.3f)
            {
                delta = 0.3f;
            }
            camera_view_set_fov(delta + current, -1);
        }
        if (jump_boost_delay_timer.elapsed())
        {
            if (player_attached_to != nullptr)
            {
                apply_jump_boost(player_attached_to);
            }
            jump_boost_delay_timer.invalidate();
        }
        else if (activated && abilities->jump_boost && !jump_boost_delay_timer.valid() && (!Multiplayer || Server) && !human_is_in_air(player_attached_to))
        {
            jump_boost_downthrust_hit_ground = true;
            deactivate();
            send_backpack_toggled_activation(player_attached_to, MBTR_OK);
        }
        if (player_attached_to->pflags.jetpack_started)
        {
            fuel = player_attached_to->jetpack_fuel_percent;
        }
        if (backpack_info->kaboom && activated && kaboom_explosion_delay_timer.elapsed())
        {
            use_kaboom();
        }
        if (vision_vfx_timer.valid())
        {
            const float strength = abilities->vision_vfx ? 1.0f - vision_vfx_timer.percent() : vision_vfx_timer.percent();
            const float r = fl_rand();
            // Magic number: the noise flickers by up to 60%.
            const float noise = strength + strength * 0.6f * ((r - 0.5f) + (r - 0.5f));
            game_render_set_camera_frame_override_blur(strength * Vision_vfx_blur);
            game_render_set_camera_signal_noise_frequency(strength * Vision_vfx_noise_frequency);
            game_render_set_camera_signal_noise_scale(noise * Vision_vfx_noise_scale);
            if (!abilities->vision_vfx && vision_vfx_timer.elapsed())
            {
                vision_vfx_timer.invalidate();
            }
        }
        if (backpack_info->uses_fuel && !player_attached_to->is_killed())
        {
            if (activated)
            {
                float left = fuel - backpack_info->fuel_use_per_second * Frametime_real;
                fuel = left;
                if (0.0f <= 0.0f - left)
                {
                    left = 0.0f;
                }
                fuel = left;
                if (0.0f < left)
                {
                    fuel_timer.set((int)((left / backpack_info->fuel_use_per_second) * 1000.0f));
                }
                else if (!Multiplayer || Server)
                {
                    deactivate();
                    send_backpack_toggled_activation(player_attached_to, MBTR_OK);
                }
            }
            else if (!player_attached_to->pflags.jetpack_started)
            {
                float recharge = multi_game_rubberbanding_get_stat_value(player_attached_to->mp_data, MRS_BACKPACK_RECHARGE);
                if (wrecking_crew_is_active())
                {
                    // Magic number: Wrecking Crew recharges twice as fast.
                    recharge = 2.0f;
                }
                if (Multiplayer || wrecking_crew_is_active())
                {
                    fuel = backpack_info->fuel_regen_per_second * recharge * Frametime_real + fuel;
                }
                else
                {
                    fuel = Jetpack_charge_rate * Frametime_real * upgrade_jetpack_charge_multiplier() + fuel;
                }
                if (1.0f - fuel < 0.0f)
                {
                    fuel = 1.0f;
                }
            }
            if (fuel == 1.0f && fuel_last_frame != 1.0f && backpack_info->fuel_refill_foley != -1 && player_attached_to == player_get_local_player())
            {
                audiolib_cue_play(backpack_info->fuel_refill_foley, nullptr, nullptr);
            }
            fuel_last_frame = fuel;
        }
        effects_alr_loop(&backpack_info->pickup_effects, &pickup_effects_alr_handles);
        if (activated)
        {
            effects_alr_loop(&backpack_info->activation_effects, &activation_effects_alr_handles);
        }
        weapon* wp = player_attached_to != nullptr ? player_attached_to->get_equipped_weapon() : nullptr;
        const unsigned int effect = info->material_effect_id;
        if (effect != 0xFFFFFFFF && activated && backpack_info->mat_fx_on_weapon && wp != nullptr)
        {
            material_effect_parameters params = backpack_material_effect_parameters(backpack_info);
            if (mat_fx_weapon_handle == 0xFFFFFFFF)
            {
                mat_fx_weapon_handle = wp->get_handle();
                weapon_start_material_effect(wp, player_attached_to, effect, params);
            }
        }
    }
    // The fuel gauge only while off.
    if (activated)
    {
        return;
    }
    if (fuel < 1.0f)
    {
        if (current_fuel_effect != Backpack_fuel_effect_charging)
        {
            World.destroy_object(fuel_effect_object, false);
            fuel_effect_object = effects_play(Backpack_fuel_effect_charging, get_handle(), fuel_effect_attach_tag_index, true, 0xFFFFFFFF, 1.0f, false);
            current_fuel_effect = Backpack_fuel_effect_charging;
        }
    }
    else if (current_fuel_effect != Backpack_fuel_effect_charged)
    {
        World.destroy_object(fuel_effect_object, false);
        fuel_effect_object = effects_play(Backpack_fuel_effect_charged, get_handle(), fuel_effect_attach_tag_index, true, 0xFFFFFFFF, 1.0f, false);
        current_fuel_effect = Backpack_fuel_effect_charged;
    }
    if (fuel_effect_object != 0xFFFFFFFF)
    {
        object_effect* gauge = (object_effect*)World.object_from_handle(fuel_effect_object);
        if (gauge != nullptr && gauge->get_obj_type() == OT_EFFECT)
        {
            gauge->set_expression_variable(EFFECTS_VAR_BACKPACK_FUEL_LIFE, fuel);
            gauge->set_expression_variable(EFFECTS_VAR_BACKPACK_FUEL_COLOR, fuel);
        }
    }
}

// 0x0097D210 (:3853): the backpack's meshes get the fuel, the activation threshold and whether it is on (a started
// jetpack counts) in their instance parameters
void multi_object_backpack::update_render(rl_renderer* pRenderer)
{
    item::update_render(pRenderer);
    const multi_backpack_info* binfo = backpack_info;
    const float fuel_level = fuel;
    const float threshold = binfo->activatable ? binfo->minimum_fuel_to_activate : 0.0f;
    float on = activated ? 1.0f : 0.0f;
    if (binfo->jetpack && player_attached_to != nullptr && player_attached_to->pflags.jetpack_started)
    {
        on = 1.0f;
    }
    if (rdd.is_loaded() && rdd.get_data()->smesh_instance_p != nullptr)
    {
        rl_renderable_instance* ri = (rl_renderable_instance*)rdd.get_data()->smesh_instance_p;
        vector4 params = ri->get_instance_params();
        params.x = fuel_level;
        params.y = threshold;
        params.z = on;
        ri->set_instance_params(params);
    }
    if (char_inst != nullptr && char_inst->rdd.is_loaded() && char_inst->rdd.get_data()->renderable_instance_p != nullptr)
    {
        rl_renderable_instance* ri = (rl_renderable_instance*)char_inst->rdd.get_data()->renderable_instance_p;
        vector4 params = ri->get_instance_params();
        params.x = fuel_level;
        params.y = threshold;
        params.z = on;
        ri->set_instance_params(params);
    }
}

// 0x0097D330 (:3893): the tags shifted by the player's backpack offset (unrotated by the backpack)
bool multi_object_backpack::get_tag_world_coords(int tag_index, vector& tag_pos, matrix* tag_orient, vector* obj_pos, matrix* obj_orient) const
{
    const bool result = item::get_tag_world_coords(tag_index, tag_pos, tag_orient, obj_pos, obj_orient);
    const vector* offset = get_player_offset();
    if (offset != nullptr)
    {
        const vector world_offset = offset->copy_unrotate(orient);
        tag_pos.x = tag_pos.x + world_offset.x;
        tag_pos.y = world_offset.y + tag_pos.y;
        tag_pos.z = world_offset.z + tag_pos.z;
    }
    return result;
}

// 0x0097D3E0 (:3906)
bool multi_object_backpack::get_tag_local_coords(int tag_index, vector& tag_pos, matrix* tag_orient) const
{
    const bool result = item::get_tag_local_coords(tag_index, tag_pos, tag_orient);
    const vector* offset = get_player_offset();
    if (offset != nullptr)
    {
        tag_pos.x = tag_pos.x - offset->x;
        tag_pos.y = tag_pos.y - offset->y;
        tag_pos.z = tag_pos.z - offset->z;
    }
    return result;
}

// ---- system
// 0x009B0630 (:1372)
void multi_game_backpack_parse_backpack_table_file(const char* backpack_table_file_name)
{
    parse_backpacks_table(backpack_table_file_name);
}

// 0x009DF590 (:3919): the table, and the rhino's cut explosion. Original: the pool's free space is read before and
// after (a memory report compiled out); not done here.
void multi_game_backpack_system_init(mempool_base& pool)
{
    parse_base_backpack_tables(&pool, false);
    const explosion_info* e_info = explosion_lookup("mp_rhino_cutter");
    Rhino_shape_cutter_explosion = e_info == nullptr ? 0xFF : explosion_get_explosion_unique_id(e_info);
}

// 0x009DF5E0 (:3947)
void multi_game_backpack_system_shutdown()
{
    Num_backpack_types = 0;
    free_backpack_infos();
}

// 0x00958DC0 (:3953): the shared backpack items are preloaded
void multi_game_backpack_level_init_pre()
{
    for (int i = 0; i < Num_item_infos; i++)
    {
        if (!Item_infos[i].shared_backpack)
        {
            continue;
        }
        stream2_container* container = stream2_get_container(Item_infos[i].srid);
        if (container == nullptr)
        {
            continue;
        }
        stream2_preload(Item_infos[i].srid);
        // Original: the requirements are computed and not used.
        int cpu_bytes = 0;
        int cpu_align;
        int gpu_bytes = 0;
        int gpu_align;
        container->calc_container_requirements(cpu_bytes, cpu_align, gpu_bytes, gpu_align);
    }
}

// 0x00958E50 (:3981)
void multi_game_backpack_level_shutdown()
{
    for (int i = 0; i < Num_item_infos; i++)
    {
        if (Item_infos[i].shared_backpack)
        {
            stream2_release(Item_infos[i].srid);
        }
    }
}

// 0x009F41A0 (:3990)
void multi_game_backpack_process()
{
    const int num = World.get_num_objects<multi_object_backpack>();
    for (int i = 0; i < num; i++)
    {
        World.get_object<multi_object_backpack>(i)->process();
    }
}

// 0x00958E90 (:3998): empty in rfg.exe
void multi_game_backpack_render()
{
}

// 0x009F8790 (:4038): every active backpack turns off and the jetpacks stop
void multi_game_backpack_round_over()
{
    const int num = World.get_num_objects<player>();
    for (int i = 0; i < num; i++)
    {
        player* pp = World.get_object<player>(i);
        multi_object_backpack* backpack = pp->backpack;
        if (backpack != nullptr && backpack->is_activated())
        {
            backpack->toggle_activation(0);
        }
        player_update_jetpack(pp, false);
    }
}

// 0x00958EA0 (:4053): by internal or display name
int multi_game_backpack_get_backpack_id(const char* name)
{
    if (name == nullptr)
    {
        return -1;
    }
    for (int i = 0; i < 16; i++)
    {
        if (Backpack_info[i] != nullptr && (v_stricmp(name, Backpack_info[i]->internal_name) == 0 || v_stricmp(name, Backpack_info[i]->display_name) == 0))
        {
            return i;
        }
    }
    return -1;
}

// 0x00958F00 (:4068): a bit per loaded backpack id
unsigned short multi_game_backpack_get_loaded_backpack_mask()
{
    unsigned short mask = 0;
    for (int i = 0; i < 16; i++)
    {
        if (Backpack_info[i] != nullptr)
        {
            mask = (unsigned short)(mask | (1 << i));
        }
    }
    return mask;
}

// 0x00958F80 (:4082)
const char* multi_game_backpack_get_internal_name(int backpack_type)
{
    // Original: the info is not checked for null (its name is at offset 0, so a null info gives "UNKNOWN").
    if (backpack_type == -1 || (unsigned int)backpack_type > 15 || Backpack_info[backpack_type] == nullptr)
    {
        return "UNKNOWN";
    }
    return Backpack_info[backpack_type]->internal_name;
}

// 0x00958FA0 (:4093)
const char* multi_game_backpack_get_display_name(int backpack_type)
{
    if (backpack_type != -1 && (unsigned int)backpack_type < 16 && Backpack_info[backpack_type] != nullptr)
    {
        return Backpack_info[backpack_type]->display_name;
    }
    return "MENU_UNKNOWN";
}

// 0x0097D470 (:4104)
const multi_backpack_info* multi_game_backpack_get_info(const char* name)
{
    if (name == nullptr)
    {
        return nullptr;
    }
    const unsigned int id = (unsigned int)multi_game_backpack_get_backpack_id(name);
    if (id == 0xFFFFFFFF || id >= 16)
    {
        return nullptr;
    }
    return Backpack_info[id];
}

// 0x00958FD0 (:4117)
const multi_backpack_info* multi_game_backpack_get_info(int backpack_type)
{
    if (backpack_type != -1 && (unsigned int)backpack_type < 16)
    {
        return Backpack_info[backpack_type];
    }
    return nullptr;
}

// 0x00958FF0 (:4129): the next loaded id after this one (-1: the first)
int multi_game_backpack_get_next_backpack_id(int backpack_id)
{
    if (backpack_id < 0 || backpack_id > 15)
    {
        if (backpack_id != -1)
        {
            return -1;
        }
        for (int i = 0; i < 16; i++)
        {
            if (Backpack_info[i] != nullptr)
            {
                return i;
            }
        }
        // Original: with none loaded the search below goes on from -1.
    }
    do
    {
        backpack_id++;
        if (backpack_id > 15)
        {
            return -1;
        }
    } while (Backpack_info[backpack_id] == nullptr);
    return backpack_id;
}

// 0x00959050 (:4153): the previous loaded id before this one (-1: the last)
int multi_game_backpack_get_prev_backpack_id(int backpack_id)
{
    if (backpack_id < 0 || backpack_id > 15)
    {
        if (backpack_id != -1)
        {
            return -1;
        }
        for (int i = 15; i >= 0; i--)
        {
            if (Backpack_info[i] != nullptr)
            {
                return i;
            }
        }
    }
    do
    {
        backpack_id--;
        if (backpack_id < 0)
        {
            return -1;
        }
    } while (Backpack_info[backpack_id] == nullptr);
    return backpack_id;
}

// 0x0099ACE0 (:4178): a backpack item (simulated, casting shadows, nothing else)
multi_object_backpack* multi_game_backpack_create(int backpack_type, const vector& pos, const matrix& orient)
{
    if (backpack_type == -1 || (unsigned int)backpack_type >= 16 || Backpack_info[backpack_type] == nullptr)
    {
        return nullptr;
    }
    const multi_backpack_info* binfo = Backpack_info[backpack_type];
    obj_item_flags flags;
    obj_item_props props(binfo->item_infop, pos, orient, flags, COLLIDE_ITEM, -1.0f, INVALID_SALVAGE_MATERIAL);
    props.set(PROP_BACKPACK_TYPE, binfo->internal_name);
    return World.create_object<multi_object_backpack>(props, 0xFFFFFFFF);
}

// 0x009F8800 (:4196): the activation button (or a toggle packet): the server (or single player) checks and toggles,
// telling the player's client a refusal; a client checks, starts its ramp up and asks the server. Without
// from_server the local checks run first, then the server's (300 ms delay).
void player_toggle_backpack_activation(player* pp, bool from_server, int activation_delay_ms)
{
    if (pp == nullptr)
    {
        return;
    }
    while (pp->backpack != nullptr)
    {
        multi_object_backpack* backpack = pp->backpack;
        const bool server_side = from_server;
        from_server = true;
        if (server_side)
        {
            if (!Multiplayer || Server)
            {
                multi_backpack_toggle_reasons reason;
                if (backpack->is_activated())
                {
                    reason = backpack->can_deactivate();
                }
                else
                {
                    reason = backpack->can_activate();
                    if (reason == MBTR_OK)
                    {
                        pp->backpack->toggle_activation(activation_delay_ms);
                        return;
                    }
                }
                send_backpack_toggled_activation(pp, reason);
                if (reason != MBTR_OK)
                {
                    return;
                }
            }
            pp->backpack->toggle_activation(activation_delay_ms);
            return;
        }
        const multi_backpack_toggle_reasons reason = backpack->is_activated() ? backpack->can_deactivate() : backpack->can_activate();
        if (pp == player_get_local_player())
        {
            display_backpack_toggle_reason(pp->backpack, reason);
        }
        if (reason != MBTR_OK)
        {
            return;
        }
        if (Multiplayer && !Server)
        {
            if (!pp->backpack->is_activated())
            {
                pp->backpack->start_ramp_up(activation_delay_ms);
            }
            send_backpack_toggled_activation(pp, MBTR_OK);
            return;
        }
        // Magic number: the server's own toggles ramp up for 0.3 s.
        activation_delay_ms = 300;
    }
}

// 0x009F41F0 (:4250): the use button on a rack's backpack: the server gives it at once, a client asks
bool player_swap_backpack(player* pp, bool do_action)
{
    if (pp == nullptr || pp->action_object.handle == 0xFFFFFFFF)
    {
        return false;
    }
    object* objp = World.object_from_handle(pp->action_object.handle);
    if (objp == nullptr || objp->get_obj_type() != OT_ITEM || objp->get_sub_type() != OT_SUB_ITEM_MULTI_BACKPACK)
    {
        return false;
    }
    multi_object_backpack* backpack = (multi_object_backpack*)objp;
    if (backpack->get_backpack_rack() == nullptr)
    {
        return false;
    }
    if (do_action)
    {
        if (!Multiplayer || Server)
        {
            if (backpack->get_player_attached_to() != nullptr)
            {
                return false;
            }
            backpack->event_player_picked_up(pp, true, false);
            return true;
        }
        pp->mp_swap_backpack = backpack;
        send_swap_backpack(pp);
    }
    return true;
}

// 0x009590A0 (:4287)
bool player_has_active_backpack(const player* pp)
{
    return pp != nullptr && pp->backpack != nullptr && pp->backpack->is_activated();
}

// ---- packets
// 0x009F4270 (:4326): client: a player picked up a backpack: the rack's one, or a new one at the player (no rack)
void multi_game_backpack_process_picked_up(unsigned char* packet_data, const sender& from)
{
    if (!Multiplayer || Server)
    {
        return;
    }
    int offset = 0;
    player* pp = World.object_from_handle<player>(multi_core_game_handle_from_network_handle(read_u16_be(packet_data, offset), false));
    if (pp == nullptr)
    {
        return;
    }
    const unsigned int rack_handle = multi_core_game_handle_from_network_handle(read_u16_be(packet_data, offset), false);
    const unsigned int index = read_u32_be(packet_data, offset);
    multi_object_backpack* backpack;
    if (rack_handle == 0xFFFFFFFF)
    {
        backpack = multi_game_backpack_create((int)index, pp->pos, IDENTITY_MATRIX);
    }
    else
    {
        multi_object_marker* rack = World.object_from_handle<multi_object_marker>(rack_handle);
        if (rack == nullptr)
        {
            return;
        }
        // Original: the index is not range checked.
        backpack = World.object_from_handle<multi_object_backpack>(rack->backpacks[index]);
    }
    if (backpack != nullptr)
    {
        backpack->event_player_picked_up(pp, true, false);
    }
}

// 0x009DF630 (:4391): a player's backpack dropped
void multi_game_backpack_process_dropped(unsigned char* packet_data, const sender& from)
{
    int offset = 0;
    player* pp = backpack_player_from_network_handle(read_u16_be(packet_data, offset));
    if (pp != nullptr && pp->backpack != nullptr)
    {
        pp->backpack->event_player_dropped(true);
    }
}

// 0x009F43E0 (:4431): a player died with a backpack
void multi_game_backpack_process_player_died_with_backpack(unsigned char* packet_data, const sender& from)
{
    int offset = 0;
    player* pp = backpack_player_from_network_handle(read_u16_be(packet_data, offset));
    if (pp != nullptr && pp->backpack != nullptr)
    {
        pp->backpack->event_player_died();
    }
}

// 0x009F8900 (:4484): an activation toggle. Ok: the backpack's own data is read and the toggle runs as the server's
// (the server delays a client's activation by up to 300 ms less its ping, twice: the client ramped up already); a
// refusal is shown to its player.
void multi_game_backpack_process_toggled_activation(unsigned char* packet_data, const sender& from)
{
    const multi_backpack_toggle_reasons reason = (multi_backpack_toggle_reasons)packet_data[0];
    int offset = 1;
    player* pp = backpack_player_from_network_handle(read_u16_be(packet_data, offset));
    if (pp == nullptr || pp->backpack == nullptr)
    {
        return;
    }
    if (reason != MBTR_OK)
    {
        if (pp == player_get_local_player())
        {
            display_backpack_toggle_reason(pp->backpack, reason);
        }
        return;
    }
    int activation_delay_ms = 0;
    if (Multiplayer && Server)
    {
        // Magic numbers: 150 ms less the ping, twice, within [0, 300] ms.
        activation_delay_ms = (150 - from.connection->mean_ping) * 2;
        if (activation_delay_ms < 0)
        {
            activation_delay_ms = 0;
        }
        else if (activation_delay_ms > 300)
        {
            activation_delay_ms = 300;
        }
    }
    unsigned short packet_offset = 3;
    pp->backpack->read_type_specific_info_from_toggle_packet(packet_data, packet_offset);
    player_toggle_backpack_activation(pp, true, activation_delay_ms);
}

// 0x009C7430 (:4531): client: a rhino hit
void multi_game_backpack_process_rhino_hit(unsigned char* packet_data, const sender& from)
{
    if (!Multiplayer || Server)
    {
        return;
    }
    int offset = 0;
    player* hitter = player_from_network_handle(read_u16_be(packet_data, offset));
    if (hitter == nullptr)
    {
        return;
    }
    player* hitee = player_from_network_handle(read_u16_be(packet_data, offset));
    if (hitee == nullptr)
    {
        return;
    }
    const vector velocity(backpack_read_float(packet_data, 4), backpack_read_float(packet_data, 8), backpack_read_float(packet_data, 12));
    rhino_process_player_hit(hitter, hitee, velocity);
}

// 0x009F44B0 (:4559): client: the ripple's hits (the attacker, then each player and its push)
void multi_game_backpack_process_ripple_hits(unsigned char* packet_data, const sender& from)
{
    if (!Multiplayer || Server)
    {
        return;
    }
    int offset = 0;
    player* attacker = World.object_from_handle<player>(multi_core_game_handle_from_network_handle(read_u16_be(packet_data, offset), false));
    if (attacker == nullptr)
    {
        return;
    }
    const unsigned char num_hit = packet_data[offset++];
    ripple_hit_info hits[16];
    // Original: up to 255 hits are read into 16 (the server sends at most the 16 players).
    for (unsigned char i = 0; i < num_hit; i++)
    {
        player* pp = World.object_from_handle<player>(multi_core_game_handle_from_network_handle(read_u16_be(packet_data, offset), false));
        hits[i].victim = pp;
        if (pp == nullptr)
        {
            return;
        }
        hits[i].velocity.set(backpack_read_float(packet_data, offset), backpack_read_float(packet_data, offset + 4), backpack_read_float(packet_data, offset + 8));
        offset += 12;
    }
    ripple_process_players_hit(attacker, hits, num_hit);
}

// 0x009F4690 (:4654): server: a client swaps for a rack's backpack
void multi_game_backpack_process_swap(unsigned char* packet_data, const sender& from)
{
    int offset = 0;
    player* pp = backpack_player_from_network_handle(read_u16_be(packet_data, offset));
    if (pp == nullptr)
    {
        return;
    }
    object* objp = backpack_object_from_network_handle(read_u16_be(packet_data, offset));
    if (objp == nullptr || objp->get_obj_type() != OT_MULTI_MARKER)
    {
        return;
    }
    multi_object_marker* rack = (multi_object_marker*)objp;
    const int index = (int)read_u32_be(packet_data, offset);
    if (index < 0 || index >= rack->num_backpacks)
    {
        return;
    }
    multi_object_backpack* backpack = World.object_from_handle<multi_object_backpack>(rack->backpacks[index]);
    pp->mp_swap_backpack = backpack;
    // Original: the backpack is not checked for null.
    pp->action_object.handle = backpack->get_handle();
    player_swap_backpack(pp, true);
}
