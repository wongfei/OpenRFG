// Original: volition/rfg/code/multi/multi_game/multi_game_weapons.cpp
//
// Networked weapons (multiplayer.md MP8): bullet hits (PT_BULLET_HIT, only the ones worth syncing), tracers
// (PT_TRACER), melee hits (PT_MELEE_HIT, a client bundles the shape keys of one swing), explosion hits
// (PT_EXPLOSION_HIT, the server's verdict), collision hits (PT_COLLISION_HIT, debris on a player), the reconstructor
// fire (PT_FIRE_REPAIR) and impaling sparks (PT_IMPALING_COLLIDE). A client sends its own hits to the server, the
// server applies what it receives (after a cheat check of the attacker's last known position) and relays it to every
// other client. Plus the custom weapons (the killers of level kills) and the multiplayer weapon iteration.
// Part of the multi unity TU (rfg_multi_unity.obj, every multi/ .cpp in path order): the file statics are shared
// by the whole tree, and a shared one is placed in its first user (tools/hdrgen/specs/mp0_stubs.py).
// The external interface is declared in multi_game_weapons.h.

#include "volition/rfg/code/multi/multi_game/multi_game_weapons.h"

#include "volition/rfg/code/code_effects/bullet_sparks.h"
#include "volition/rfg/code/code_effects/explosion_shared.h"
#include "volition/rfg/code/code_effects/tracers.h"
#include "volition/rfg/code/destruction/rfg_shapes.h"
#include "volition/rfg/code/destruction/rfg_mover.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_compression.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_io.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_objects.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_packet_types.h"
#include "volition/rfg/code/multi/multi_core/include/multi_session.h"
#include "volition/rfg/code/multi/multi_game/multi_game.h"
#include "volition/rfg/code/multi/multi_game/multi_game_object_backpack.h"
#include "volition/rfg/code/multi/multi_game/multi_game_object_player.h"
#include "volition/rfg/code/multi/multi_game/multi_game_player.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_combat.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/movers/general_mover.h"
#include "volition/rfg/code/objects/object_item.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/vlib/code/math/floating.h"

#include <math.h>
#include <string.h>

// ---- file-static globals
// bool Allow_cheat_detection (0x0167D7CB) is used here too; placed in multi_connection.cpp.
// multi_gameplay_mode Multi_gameplay_mode (0x0167DEE0) is used here too; placed in multi_gameplay_mode.cpp.
// human_voice_persona_handle Multi_general_announcer (0x0167DEE4) is used here too; placed in multi_game_mode_bagman.cpp.
// const char* Multi_custom_weapon_names[10] (0x0167F4CC) is used here too; placed in multi_game_mode_common.cpp.
// multi_pool<snapshot_packet_header,500> Snapshot_packet_headers (0x01691A80) is used here too; placed in multi.cpp.
// dynamic_uint_hash_table<remote_object*> Game_handle_to_rop_hash (0x01691B40) is used here too; placed in multi_core_delta_compression.cpp.
// dynamic_uint_hash_table<remote_object*> Network_handle_to_rop_hash (0x01691B88) is used here too; placed in multi_core_delta_compression.cpp.
// dynamic_uint_hash_table<network_game_handle*> Reserved_game_to_network_hash (0x01691C18) is used here too; placed in multi_core_objects.cpp.
// dynamic_uint_hash_table<network_game_handle*> Reserved_network_to_game_hash (0x01691C60) is used here too; placed in multi_core_delta_compression.cpp.
// unsigned int Clock_current_time (0x02FEB538) is used here too; placed in multi.cpp.
// bool Server (0x02FEB590) is used here too; placed in multi_core_session_generic.cpp.
// cs_wrapper Core_io_send_job_wrapper (0x02FEB890) is used here too; placed in multi.cpp.
// cs_wrapper Add_packet_wrapper (0x02FED534) is used here too; placed in multi.cpp.
// A client's melee hits of one frame (one bundle per attacker, victim, weapon and move along one surface).
static int Num_melee_hit_bundles; // 0x02FEB75C
static melee_hit_bundle Client_melee_hit_bundles[10]; // 0x030173C0

// The angle ranges of the spherical normals (float pi widened to double, as compiled).
static const double MGW_PI = (double)3.1415927f;
static const double MGW_HALF_PI = (double)1.5707964f;

// The position deltas of the hits: 16 bits per axis over +-512 m (hits on objects, the shooter's position), +-1024 m
// (tracers that hit nothing, from the origin) and +-1500 m (explosions, from the origin).
static const int MGW_POS_BITS = 16;

// A float written big endian (PACKET_SWAP_FLOAT).
static void mgw_write_float(unsigned char* packet, unsigned short& size, float value)
{
    unsigned int bits;
    memcpy(&bits, &value, 4);
    write_u32_be(packet, size, bits);
}

static float mgw_read_float(const unsigned char* packet_data, int offset)
{
    const unsigned int bits = read_u32_be(packet_data, offset);
    float value;
    memcpy(&value, &bits, 4);
    return value;
}

// A position delta into the packet (vec_delta_compress into 6 bytes).
static void mgw_write_vec(unsigned char* packet, unsigned short& size, const vector& from, const vector& to, float pack_amount)
{
    unsigned char packed[6] = {};
    bit_packer packer(packed, 6);
    vec_delta_compress(packer, from, to, MGW_POS_BITS, pack_amount, 0.0f, 1.0f);
    memcpy(packet + size, packed, 6);
    size = (unsigned short)(size + 6);
}

static void mgw_read_vec(const unsigned char* packet_data, vector& result, const vector& from, float pack_amount)
{
    unsigned char packed[6];
    memcpy(packed, packet_data, 6);
    bit_packer packer(packed, 6);
    vec_delta_decompress(packer, result, from, MGW_POS_BITS, pack_amount, 0.0f, 1.0f);
}

// A server sends its relays and verdicts unreliably, a client its hits reliably.
static void mgw_send(multi_connection* cp, unsigned char* packet, unsigned int size)
{
    if (Multiplayer == true && Server == true)
    {
        multi_io_send(cp, packet, size);
        return;
    }
    multi_io_send_reliable(cp, packet, size, nullptr, nullptr, nullptr);
}

// Whether a connection belongs to the attacker (a player's own hits are not echoed back to it).
static bool mgw_is_attackers_connection(const multi_connection* cp, const human* attacker)
{
    return object_is_player(attacker) && cp->game_data == ((const player*)attacker)->mp_data;
}

static human* mgw_human_from_handle(unsigned int handle)
{
    object* objp = handle != 0xFFFFFFFF ? World.object_from_handle(handle) : nullptr;
    return objp != nullptr && objp->get_obj_type() == OT_HUMAN ? (human*)objp : nullptr;
}

// ---- internal functions
// 0x009870F0 (:159): a bullet hit event (the queued form of multi_gw_process_bullet_hit's work; not referenced in
// rfg.exe). A human in a vehicle is not hit (LTCG: event in ESI).
static bool bullet_hit_event_process_callback(const bullet_hit_event* event)
{
    human* attacker = mgw_human_from_handle(event->attacker_game_handle);
    object* hit = World.object_from_handle(event->fire_info.hit_handle);
    if (hit != nullptr && (hit->get_obj_type() != OT_HUMAN || ((human*)hit)->vehicle_handle == 0xFFFFFFFF))
    {
        weapons_create_bullet_hit_effects(attacker, event->fire_info, event->damage);
        weapons_process_bullet_hit(attacker, event->fire_info, event->damage);
    }
    return true;
}

// 0x00987170 (:186): a received tracer, of the weapon's kind.
static bool tracer_process_callback(const tracer_event& event)
{
    human* hp = mgw_human_from_handle(event.attacker_game_handle);
    const weapon_info* w_info = event.fire_info.w_info;
    if (w_info->flags.auto_aim_curved_trail)
    {
        weapon_create_curved_tracer_base(hp, event.fire_info.start_pos, event.fire_info.hit_pos, w_info, WEAPON_TRACER_ENFORCER);
        return true;
    }
    weapon_tracer_type tracer_type = WEAPON_TRACER_DEFAULT;
    if (w_info->weapon_class == WEAPON_CLASS_NANO_RIFLE)
    {
        tracer_type = WEAPON_TRACER_NANO;
    }
    else if (w_info->flags.can_scope && w_info->weapon_anim_group == ANIMATION_GROUP_SNIPER)
    {
        tracer_type = WEAPON_TRACER_SNIPER;
    }
    else if (w_info->flags.penetrating_bullets)
    {
        tracer_type = WEAPON_TRACER_RAIL;
    }
    weapon_create_tracer_base(hp, event.fire_info.start_pos, event.fire_info.hit_pos, w_info, tracer_type);
    return true;
}

// 0x00966FA0 (:221): the same swing on the same surface (within 0.5 m, normals within 30 degrees) and room for a
// key (LTCG: cur_bundle in ESI, add_bundle in EAX).
static bool can_add_to_bundle(const melee_hit_bundle& cur_bundle, const melee_hit_bundle& add_bundle)
{
    // Magic numbers: 0.25 = 0.5 m squared, 0.8660254 = cos 30 degrees; 8 keys at most.
    return cur_bundle.attacker_handle == add_bundle.attacker_handle && cur_bundle.victim_handle == add_bundle.victim_handle && cur_bundle.weapon_unique_id == add_bundle.weapon_unique_id &&
           cur_bundle.move_id == add_bundle.move_id && cur_bundle.num_shape_keys < 8 && vec_dist_squared(cur_bundle.hit_pos, add_bundle.hit_pos) <= 0.25f &&
           0.8660254f <= cur_bundle.hit_normal * add_bundle.hit_normal;
}

// 0x00967010 (:248): a client's melee hit joins a bundle of this frame (its shape key), or opens one (10 at most;
// false when full: the hit is sent alone). LTCG: add_bundle in EAX.
static bool add_melee_hit_to_melee_hit_bundle(const melee_hit_bundle& add_bundle)
{
    for (int i = 0; i < Num_melee_hit_bundles; i++)
    {
        melee_hit_bundle& bundle = Client_melee_hit_bundles[i];
        if (can_add_to_bundle(bundle, add_bundle))
        {
            bundle.shape_keys[bundle.num_shape_keys] = add_bundle.shape_keys[0];
            bundle.num_shape_keys++;
            return true;
        }
    }
    if (Num_melee_hit_bundles > 9)
    {
        return false;
    }
    Client_melee_hit_bundles[Num_melee_hit_bundles] = add_bundle;
    Num_melee_hit_bundles++;
    return true;
}

// 0x00987220 (:280): a melee hit event (not referenced in rfg.exe). LTCG: event in EAX.
static bool melee_hit_event_process_callback(melee_hit_event* event)
{
    human* hp = mgw_human_from_handle(event->attacker_game_handle);
    object* target = World.object_from_handle(event->hit_info.hit_handle);
    if (target != nullptr)
    {
        const human_melee_combat_move_data* attacker_cmp = human_combat_move_data_from_index(hp, event->hit_info.move_id);
        if (attacker_cmp == nullptr)
        {
            return false;
        }
        human_combat_unified_melee_hit(target, hp, event->hit_info.hit_pos, event->hit_info.hit_normal, event->hit_info.hit_direction, event->hit_info.w_info, attacker_cmp,
                                       event->hit_info.hit_shape_key, true, false);
    }
    return true;
}

// 0x00967130 (:310): a unit vector in two bytes: theta (the azimuth, atan2(y, x)) and phi (the polar angle, from z).
// Original: theta is packed over [-pi/2, pi/2] although atan2 covers [-pi, pi] (the other half clamps). LTCG: v in
// ESI.
static void mgw_compress_spherical_vector(unsigned char& theta, unsigned char& phi, const vector& v)
{
    float theta_value = 0.0f;
    float phi_value;
    const float mag = (float)sqrt((double)v.x * (double)v.x + (double)v.y * (double)v.y + (double)v.z * (double)v.z);
    if (mag == 0.0f)
    {
        phi_value = 0.0f;
    }
    else
    {
        theta_value = (float)atan2((double)v.y, (double)v.x);
        float cos_phi = (float)((double)v.z / (double)mag);
        if ((float)((double)cos_phi - -1.0) < 0.0f)
        {
            cos_phi = -1.0f;
        }
        if (!((float)((double)cos_phi - 1.0) < 0.0f))
        {
            cos_phi = 1.0f;
        }
        phi_value = fl_acos(cos_phi);
    }
    theta = (unsigned char)compress_float(theta_value, -MGW_HALF_PI, MGW_HALF_PI, 0.0, 1.0, 8);
    phi = (unsigned char)compress_float(phi_value, 0.0, MGW_PI, MGW_HALF_PI, 1.0, 8);
}

// 0x009672D0 (:325) (LTCG: phi in AL)
static void mgw_decompress_spherical_vector(vector& v, unsigned char theta, unsigned char phi)
{
    const float pol_phi = (float)decompress_double(phi, 0.0, MGW_PI, MGW_HALF_PI, 1.0, 8);
    const float pol_theta = (float)decompress_double(theta, -MGW_HALF_PI, MGW_HALF_PI, 0.0, 1.0, 8);
    convert_spherical_to_rectilinear_coords_float(v, 1.0f, pol_theta, pol_phi);
}

// 0x009A34C0 (:334): a bullet on an rfg mover matters when it hits glass (physical material flag 8) or a piece
// flagged 0x1000 (or the shape is not loaded). LTCG: rmp in ECX, fire_info in EAX.
static bool is_important_rfg_mover_bullet_hit(const rfg_mover* rmp, const weapon_fire_info& fire_info)
{
    if (fire_info.hit_shape_key == 0xFFFFFFFF)
    {
        return false;
    }
    if (!rmp->rdd.is_loaded() || rmp->rdd.get_data()->shape == nullptr)
    {
        return true;
    }
    const rfg_destroyable_shape* shape = rmp->rdd.get_data()->shape;
    const unsigned int key = fire_info.hit_shape_key;
    const rfg_physical_material* material = rfg_get_physical_material_by_index(shape->get_subpiece_base(key)->physical_material_index); // inlined
    if (material == nullptr)
    {
        return false;
    }
    return (material->flags & 8) != 0 || (shape->get_subpiece(key)->flags & 0x1000) != 0; // inlined
}

// 0x009673A0 (:354): a general mover that can still be damaged (not flagged 4, hitpoints left). LTCG: gmp in EAX.
static bool is_important_general_mover_bullet_hit(const general_mover* gmp)
{
    if ((gmp->mover_flags & 4) != 0)
    {
        return false;
    }
    return gmp->m_level_obj_properties->m_hitpoints > 0;
}

// 0x009D3830 (:375): the bullet hits worth sending: on living humans, projectiles and bullet-damaged items,
// breakable movers; never on corpses, effects or zones. A nano rifle hit always counts. The victim must be
// networked.
bool multi_game_weapon_bullet_strike_is_important_to_sync(const weapon_fire_info& fire_info, const object* victim)
{
    if (victim == nullptr)
    {
        return false;
    }
    const weapon_info* w_info = fire_info.w_info;
    const int type = victim->get_obj_type();
    if (type == OT_HUMAN && victim->is_killed())
    {
        return false;
    }
    if (w_info == nullptr || w_info->weapon_class != WEAPON_CLASS_NANO_RIFLE)
    {
        switch (type)
        {
        case OT_HUMAN:
            if (human_is_dead((const human*)victim))
            {
                return false;
            }
            break;
        case OT_ITEM:
            if (victim->get_sub_type() != OT_SUB_ITEM_PROJECTILE && !((const item*)victim)->damage_info.flags.damaged_by_bullets)
            {
                return false;
            }
            break;
        case OT_MOVER:
            if (victim->get_sub_type() == OT_SUB_MOVER_RFG)
            {
                return is_important_rfg_mover_bullet_hit((const rfg_mover*)victim, fire_info);
            }
            if ((((const object_mover*)victim)->mover_flags & 4) == 0)
            {
                return ((const object_mover*)victim)->m_level_obj_properties->m_hitpoints > 0;
            }
            return false;
        case OT_EFFECT:
        case OT_ZONE:
            return false;
        default:
            break;
        }
    }
    return multi_core_network_handle_from_game_handle(victim->get_handle(), true) != 0xFFFF;
}

// 0x009A3530 (:440)
bool multi_gw_can_hit_target(const human* attacker, const object* objp)
{
    if (Multiplayer && attacker != nullptr && objp != nullptr && objp->get_obj_type() == OT_HUMAN)
    {
        return multi_gw_can_hit_target(attacker, ((const human*)objp)->current_team);
    }
    return true;
}

// 0x009872A0 (:454): in team modes without friendly fire, only hostile and unfriendly teams.
bool multi_gw_can_hit_target(const human* attacker, human_teams target_team)
{
    // Magic number: 8 gameplay modes.
    if (Multiplayer && (int)Multi_gameplay_mode > -1 && (int)Multi_gameplay_mode < 8 && Multi_gameplay_mode_funcs[Multi_gameplay_mode].is_team_mode &&
        !Mp_server_options.generic_options.friendly_fire)
    {
        return (int)human_teams_get_disposition(attacker, target_team) < TEAM_DISPOSITION_FRIENDLY;
    }
    return true;
}

// 0x009D3920 (:521): PT_BULLET_HIT: attacker, victim, weapon, the hit relative to the victim, the shot's start
// relative to the attacker, hit location, shape key, materials, damage. Not for corpses.
void multi_gw_send_bullet_hit_to_connection(multi_connection* cp, const human* attacker, const weapon_fire_info& fire_info, float damage)
{
    const object* hit = fire_info.hit_handle != 0xFFFFFFFF ? World.object_from_handle(fire_info.hit_handle) : nullptr;
    if (hit != nullptr && hit->get_obj_type() == OT_HUMAN && hit->is_killed())
    {
        return;
    }
    unsigned char packet[Packet_buffer_size];
    packet[0] = PT_BULLET_HIT;
    unsigned short size = packet_header_size(PT_BULLET_HIT);
    write_u16_be(packet, size, multi_core_network_handle_from_game_handle(attacker->get_handle(), false));
    write_u16_be(packet, size, multi_core_network_handle_from_game_handle(fire_info.hit_handle, false));
    packet[size++] = (unsigned char)fire_info.w_info->unique_id;
    // Original: the hit object is used unchecked (it is null for a hit on nothing).
    hit = World.object_from_handle(fire_info.hit_handle);
    mgw_write_vec(packet, size, hit->pos, fire_info.hit_pos, 512.0f);
    mgw_write_vec(packet, size, attacker->pos, fire_info.start_pos, 512.0f);
    packet[size++] = (unsigned char)fire_info.hit_location;
    write_u16_be(packet, size, (unsigned short)fire_info.hit_shape_key);
    packet[size++] = fire_info.hit_physical_material;
    packet[size++] = fire_info.hit_effect_material;
    mgw_write_float(packet, size, damage);
    packet_set_size(packet, size);
    mgw_send(cp, packet, size);
}

// 0x009D3C50 (:604): the server relays to every connected client but the attacker's; a client sends to the server.
void multi_gw_send_bullet_hit(const human* attacker, const weapon_fire_info& fire_info, float damage)
{
    const object* victim = World.object_from_handle(fire_info.hit_handle);
    if (!multi_game_weapon_bullet_strike_is_important_to_sync(fire_info, victim))
    {
        return;
    }
    multi_session* session = multi_session_get_by_type(ST_GAME);
    if (Multiplayer != true)
    {
        return;
    }
    if (Server == true)
    {
        multi_connection* first = session->get_connection_list();
        for (multi_connection* cp = first; cp != nullptr;)
        {
            if (cp != session->get_host_connection() && (cp->connection_status == CS_CONNECTED || cp->connection_status == CS_CONNECTED_GAMEPLAY) && !mgw_is_attackers_connection(cp, attacker))
            {
                multi_gw_send_bullet_hit_to_connection(cp, attacker, fire_info, damage);
            }
            cp = cp->next;
            if (cp == first)
            {
                break;
            }
        }
    }
    else if (Server == false)
    {
        multi_gw_send_bullet_hit_to_connection(session->get_host_connection(), attacker, fire_info, damage);
    }
}

// 0x009B3540 (:628): the cheat check of a player's hit on a player: the shot must start near where the attacker was
// last seen (its speed, at least 15 m/s, over the time since plus 500 ms, plus the extra distance). LTCG: attacker
// in EAX, victim in EDI.
static bool validate_attack_hit(const object* attacker, const object* victim, const vector& fire_pos, float extra_dist)
{
    const multi_session* session = Allow_cheat_detection ? multi_session_get_by_type(ST_GAME) : nullptr;
    if (session == nullptr || session->get_info()->type != ST_GAME || (session->get_state() != MSS_IN_GAME && session->get_state() != MSS_MIGRATING) || victim == attacker)
    {
        return true;
    }
    const player* attacker_player = attacker != nullptr && attacker->get_obj_type() == OT_HUMAN && attacker->get_sub_type() == OT_SUB_HUMAN_PLAYER ? (const player*)attacker : nullptr;
    const player* victim_player = victim != nullptr && victim->get_obj_type() == OT_HUMAN && victim->get_sub_type() == OT_SUB_HUMAN_PLAYER ? (const player*)victim : nullptr;
    unsigned int update_time;
    vector last_pos;
    vector last_velocity;
    if (attacker_player == nullptr || victim_player == nullptr || !multi_game_player_get_last_pos_velocity_update(attacker_player, update_time, last_pos, last_velocity))
    {
        return true;
    }
    const int since = update_time < Clock_current_time ? (int)(Clock_current_time - update_time) : 0;
    const float dist_squared = vec_dist_squared(fire_pos, last_pos);
    float speed = last_velocity.mag();
    // Magic numbers: at least 15 m/s, 500 ms of slack.
    if (0.0f <= 15.0f - speed)
    {
        speed = 15.0f;
    }
    const float allowed = (float)(unsigned int)(since + 500) * 0.001f * speed + extra_dist;
    return dist_squared <= allowed * allowed;
}

// 0x009D3D30 (:682): a bullet hit from a client (or the server's relay): the hit and the shot rebuilt around the
// victim and the attacker (the victim when the attacker is gone), the normal toward the shooter. The server checks
// it, applies it (not to humans in vehicles) and relays it.
void multi_gw_process_bullet_hit(unsigned char* packet_data, const sender& from)
{
    int offset = 0;
    const unsigned int attacker_handle = multi_core_game_handle_from_network_handle(read_u16_be(packet_data, offset), true);
    // Original bug kept: the constructor only sets hit_handle and w_info and the packet has no alt body index or bone,
    // so hit_alt_body_idx and hit_bone stay uninitialized (stack garbage) for everything that reads them later.
    weapon_fire_info fire_info;
    fire_info.hit_handle = multi_core_game_handle_from_network_handle(read_u16_be(packet_data, offset), true);
    object* hit = World.object_from_handle(fire_info.hit_handle);
    if (hit == nullptr)
    {
        return;
    }
    object* attacker = World.object_from_handle(attacker_handle);
    if (attacker == nullptr)
    {
        attacker = hit;
    }
    fire_info.w_info = weapons_get_weapon_info_from_weapon_unique_id(packet_data[4]);
    mgw_read_vec(packet_data + 5, fire_info.hit_pos, hit->pos, 512.0f);
    mgw_read_vec(packet_data + 11, fire_info.start_pos, attacker->pos, 512.0f);
    fire_info.hit_location = (object_hit_location)packet_data[17];
    offset = 18;
    fire_info.hit_shape_key = read_u16_be(packet_data, offset);
    if (fire_info.hit_shape_key == 0xFFFF)
    {
        fire_info.hit_shape_key = 0xFFFFFFFF;
    }
    fire_info.hit_physical_material = packet_data[20];
    fire_info.hit_effect_material = packet_data[21];
    const float damage = mgw_read_float(packet_data, 22);
    // The normal faces the shooter (each component rounded through double, as compiled).
    fire_info.hit_normal.x = (float)((double)fire_info.start_pos.x - (double)fire_info.hit_pos.x);
    fire_info.hit_normal.y = (float)((double)fire_info.start_pos.y - (double)fire_info.hit_pos.y);
    fire_info.hit_normal.z = (float)((double)fire_info.start_pos.z - (double)fire_info.hit_pos.z);
    // Original bug kept: the end is the hit position itself (the packet carries no end), so on a relayed hit the kill
    // blood splatter ray of human bullet hits (end_pos - hit_pos) has zero length.
    fire_info.end_pos = fire_info.hit_pos;
    fire_info.hit_normal.normalize_safe();
    if (Multiplayer == true && Server == true && !validate_attack_hit(attacker, hit, fire_info.start_pos, 0.0f))
    {
        return;
    }
    // A simulated bullet (mp flag sim_bullet).
    fire_info.mp_flags.flag_data = 0x10;
    human* attacker_human = mgw_human_from_handle(attacker_handle);
    hit = World.object_from_handle(fire_info.hit_handle);
    if (hit != nullptr && (hit->get_obj_type() != OT_HUMAN || ((human*)hit)->vehicle_handle == 0xFFFFFFFF))
    {
        weapons_create_bullet_hit_effects(attacker_human, fire_info, damage);
        weapons_process_bullet_hit(attacker_human, fire_info, damage);
    }
    if (Multiplayer == true && Server == true)
    {
        multi_gw_send_bullet_hit(mgw_human_from_handle(attacker_handle), fire_info, damage);
    }
}

// 0x009D42C0 (:804): PT_TRACER: the shooter, the start relative to it, then the end relative to the hit object (a
// networked one) or to the origin. The server sends it unreliably. LTCG: fire_info in ECX.
static void multi_gw_send_tracer_to_connection(multi_connection* cp, const human* attacker, const weapon_fire_info& fire_info)
{
    unsigned char packet[Packet_buffer_size];
    packet[0] = PT_TRACER;
    unsigned short size = packet_header_size(PT_TRACER);
    write_u16_be(packet, size, multi_core_network_handle_from_game_handle(attacker->get_handle(), true));
    mgw_write_vec(packet, size, attacker->pos, fire_info.start_pos, 512.0f);
    const object* hit = World.object_from_handle(fire_info.hit_handle);
    const bool networked_hit = hit != nullptr && multi_core_network_handle_from_game_handle(fire_info.hit_handle, true) != 0xFFFF;
    packet[size++] = networked_hit;
    if (networked_hit)
    {
        write_u16_be(packet, size, multi_core_network_handle_from_game_handle(fire_info.hit_handle, true));
        mgw_write_vec(packet, size, hit->pos, fire_info.hit_pos, 512.0f);
    }
    else
    {
        mgw_write_vec(packet, size, ZERO_VECTOR, fire_info.hit_pos, 1024.0f);
    }
    packet[size++] = (unsigned char)fire_info.w_info->unique_id;
    packet_set_size(packet, size);
    mgw_send(cp, packet, size);
}

// 0x009D4BF0 (:862)
void multi_gw_send_tracer(const human* attacker, const weapon_fire_info& fire_info)
{
    multi_session* session = multi_session_get_by_type(ST_GAME);
    multi_connection* host = session->get_host_connection();
    if (Multiplayer != true)
    {
        return;
    }
    if (Server == true)
    {
        multi_connection* first = session->get_connection_list();
        for (multi_connection* cp = first; cp != nullptr;)
        {
            if (cp != host && !mgw_is_attackers_connection(cp, attacker))
            {
                multi_gw_send_tracer_to_connection(cp, attacker, fire_info);
            }
            cp = cp->next;
            if (cp == first)
            {
                break;
            }
        }
    }
    else if (Server == false)
    {
        multi_gw_send_tracer_to_connection(host, attacker, fire_info);
    }
}

// 0x009D4C90 (:883): a received tracer is drawn; the server relays it.
void multi_gw_process_tracer(unsigned char* packet_data, const sender& from)
{
    int offset = 0;
    tracer_event event;
    event.fire_info.hit_handle = 0xFFFFFFFF;
    event.fire_info.w_info = nullptr;
    event.attacker_game_handle = multi_core_game_handle_from_network_handle(read_u16_be(packet_data, offset), true);
    const object* shooter = World.object_from_handle(event.attacker_game_handle);
    if (shooter == nullptr)
    {
        return;
    }
    mgw_read_vec(packet_data + 2, event.fire_info.start_pos, shooter->pos, 512.0f);
    int id_offset;
    if (packet_data[8] == 0)
    {
        mgw_read_vec(packet_data + 9, event.fire_info.hit_pos, ZERO_VECTOR, 1024.0f);
        id_offset = 15;
    }
    else
    {
        offset = 9;
        event.fire_info.hit_handle = multi_core_game_handle_from_network_handle(read_u16_be(packet_data, offset), true);
        const object* hit = World.object_from_handle(event.fire_info.hit_handle);
        if (hit == nullptr)
        {
            return;
        }
        mgw_read_vec(packet_data + 11, event.fire_info.hit_pos, hit->pos, 512.0f);
        id_offset = 17;
    }
    event.fire_info.w_info = weapons_get_weapon_info_from_weapon_unique_id(packet_data[id_offset]);
    tracer_process_callback(event);
    if (Multiplayer == true && Server == true)
    {
        multi_gw_send_tracer(mgw_human_from_handle(event.attacker_game_handle), event.fire_info);
    }
}

// 0x009D5090 (:958): PT_MELEE_HIT: attacker, victim, weapon, the hit relative to the victim, the normal in two bytes,
// the shape keys, the move. Not for corpses; the server only sends hits on humans.
static void multi_gw_send_melee_hit_to_connection(multi_connection* cp, const melee_hit_bundle& hit_bundle)
{
    const object* victim = hit_bundle.victim_handle != 0xFFFFFFFF ? World.object_from_handle(hit_bundle.victim_handle) : nullptr;
    if (victim != nullptr && victim->get_obj_type() == OT_HUMAN)
    {
        if (victim->is_killed())
        {
            return;
        }
    }
    else
    {
        victim = nullptr;
    }
    if (Multiplayer == true && Server == true && victim == nullptr)
    {
        return;
    }
    unsigned char packet[Packet_buffer_size];
    packet[0] = PT_MELEE_HIT;
    unsigned short size = packet_header_size(PT_MELEE_HIT);
    write_u16_be(packet, size, multi_core_network_handle_from_game_handle(hit_bundle.attacker_handle, false));
    write_u16_be(packet, size, multi_core_network_handle_from_game_handle(hit_bundle.victim_handle, false));
    packet[size++] = (unsigned char)hit_bundle.weapon_unique_id;
    // Original: the victim object is used unchecked (a client may hit a zone).
    const object* victim_obj = World.object_from_handle(hit_bundle.victim_handle);
    mgw_write_vec(packet, size, victim_obj->pos, hit_bundle.hit_pos, 512.0f);
    unsigned char theta;
    unsigned char phi;
    mgw_compress_spherical_vector(theta, phi, hit_bundle.hit_normal);
    packet[size++] = theta;
    packet[size++] = phi;
    packet[size++] = (unsigned char)hit_bundle.num_shape_keys;
    for (int i = 0; i < hit_bundle.num_shape_keys; i++)
    {
        write_u16_be(packet, size, (unsigned short)hit_bundle.shape_keys[i]);
    }
    packet[size++] = (unsigned char)hit_bundle.move_id;
    packet_set_size(packet, size);
    mgw_send(cp, packet, size);
}

// 0x009D5320 (:1034): the melee hit on a networked target: humans that are alive, zones (the ground) when a player's
// backpack makes a shockwave or ripple. The server relays it to every client but the attacker's; a client bundles
// it (or sends it when the bundles are full).
void multi_gw_send_melee_hit(const human* attacker, unsigned int target_handle, const weapon_info* w_info, const vector& hit_pos, const vector& hit_normal, const vector& hit_direction,
                             unsigned int hit_shape_key, human_melee_combat_move_id melee_combat_move_id)
{
    if (!Multiplayer || multi_core_network_handle_from_game_handle(target_handle, true) == 0xFFFF)
    {
        return;
    }
    const object* target = World.object_from_handle(target_handle);
    const player* attacker_player = attacker != nullptr && attacker->get_obj_type() == OT_HUMAN && attacker->get_sub_type() == OT_SUB_HUMAN_PLAYER ? (const player*)attacker : nullptr;
    // Original: target is used unchecked.
    if (target->get_obj_type() == OT_HUMAN)
    {
        if (target->is_killed())
        {
            return;
        }
    }
    else if (target->get_obj_type() == OT_ZONE)
    {
        if (attacker_player == nullptr || attacker_player->backpack == nullptr)
        {
            return;
        }
        const multi_object_backpack* backpack = attacker_player->backpack;
        if (backpack->get_backpack_abilities()->shockwave != true && backpack->get_backpack_abilities()->ripple != true)
        {
            return;
        }
    }
    melee_hit_bundle bundle;
    bundle.attacker_handle = attacker == nullptr ? 0xFFFFFFFF : attacker->get_handle();
    bundle.victim_handle = target_handle;
    // Magic number: 0xFF, no weapon.
    bundle.weapon_unique_id = w_info == nullptr ? 0xFF : w_info->unique_id;
    bundle.hit_pos = hit_pos;
    // Original: the hit normal argument is not used; the bundle's normal is the reversed hit direction.
    bundle.hit_normal = vector(-hit_direction.x, -hit_direction.y, -hit_direction.z);
    bundle.move_id = melee_combat_move_id;
    bundle.shape_keys[0] = hit_shape_key;
    bundle.num_shape_keys = 1;
    multi_session* session = multi_session_get_by_type(ST_GAME);
    if (Multiplayer != true)
    {
        return;
    }
    if (Server == true)
    {
        multi_connection* first = session->get_connection_list();
        if (first == nullptr)
        {
            return;
        }
        for (multi_connection* cp = first;;)
        {
            if (cp != session->get_host_connection() && multi_connection_is_connected(cp) && !mgw_is_attackers_connection(cp, attacker))
            {
                multi_gw_send_melee_hit_to_connection(cp, bundle);
            }
            cp = cp->next;
            if (cp == first || cp == nullptr)
            {
                break;
            }
        }
        if (Multiplayer != true)
        {
            return;
        }
    }
    multi_connection* host = session->get_host_connection();
    if (Server == false && host != nullptr && !add_melee_hit_to_melee_hit_bundle(bundle))
    {
        multi_gw_send_melee_hit_to_connection(host, bundle);
    }
}

// 0x009D5560 (:1117): a received melee hit: the hit rebuilt around the victim, the server checks it (within the
// weapon's range, 1 to 3 m), then each shape key is applied and relayed.
void multi_gw_process_melee_hit(unsigned char* packet_data, const sender& from)
{
    int offset = 0;
    const unsigned int attacker_handle = multi_core_game_handle_from_network_handle(read_u16_be(packet_data, offset), true);
    const unsigned int victim_handle = multi_core_game_handle_from_network_handle(read_u16_be(packet_data, offset), true);
    object* victim = World.object_from_handle(victim_handle);
    if (victim == nullptr)
    {
        return;
    }
    object* attacker = World.object_from_handle(attacker_handle);
    if (attacker == nullptr)
    {
        attacker = victim;
    }
    const weapon_info* w_info = weapons_get_weapon_info_from_weapon_unique_id(packet_data[4]);
    vector hit_pos;
    mgw_read_vec(packet_data + 5, hit_pos, victim->pos, 512.0f);
    vector hit_normal;
    mgw_decompress_spherical_vector(hit_normal, packet_data[11], packet_data[12]);
    const vector hit_direction(-hit_normal.x, -hit_normal.y, -hit_normal.z);
    const int num_keys = packet_data[13];
    unsigned int shape_keys[9];
    offset = 14;
    for (int i = 0; i < num_keys; i++)
    {
        shape_keys[i] = read_u16_be(packet_data, offset);
    }
    const human_melee_combat_move_id move_id = (human_melee_combat_move_id)packet_data[offset];
    if (Multiplayer == true && Server == true)
    {
        // Magic numbers: the range allowed is the weapon's, 1 to 3 m.
        float range = w_info == nullptr ? 0.0f : w_info->max_range;
        if (range - 1.0f < 0.0f)
        {
            range = 1.0f;
        }
        if (0.0f <= range - 3.0f)
        {
            range = 3.0f;
        }
        if (!validate_attack_hit(attacker, victim, hit_pos, range))
        {
            return;
        }
    }
    for (int i = 0; i < num_keys; i++)
    {
        const unsigned int shape_key = shape_keys[i];
        human* attacker_human = mgw_human_from_handle(attacker_handle);
        object* target = World.object_from_handle(victim_handle);
        if (target != nullptr)
        {
            const human_melee_combat_move_data* attacker_cmp = human_combat_move_data_from_index(attacker_human, move_id);
            if (attacker_cmp != nullptr)
            {
                human_combat_unified_melee_hit(target, attacker_human, hit_pos, hit_normal, hit_direction, w_info, attacker_cmp, shape_key, true, false);
            }
        }
        if (Multiplayer == true && Server == true)
        {
            human* relay_attacker = World.object_cast<human>(attacker);
            if (relay_attacker != nullptr)
            {
                multi_gw_send_melee_hit(relay_attacker, victim_handle, w_info, hit_pos, hit_normal, hit_direction, shape_key, move_id);
            }
        }
    }
}

// 0x00967410 (:1238): the local client's bundles start empty each frame.
void multi_game_weapons_reset_melee_hit(const human* hp)
{
    if (Multiplayer && !Server && object_is_local_player(hp))
    {
        Num_melee_hit_bundles = 0;
    }
}

// 0x009D5B20 (:1245): the local client sends its bundles of the frame to the server.
void multi_game_weapons_process_melee_hit(const human* hp)
{
    if (!Multiplayer || Server || !object_is_local_player(hp) || Num_melee_hit_bundles <= 0)
    {
        return;
    }
    multi_connection* host = multi_session_get_by_type(ST_GAME)->get_host_connection();
    if (host == nullptr)
    {
        return;
    }
    for (int i = 0; i < Num_melee_hit_bundles; i++)
    {
        multi_gw_send_melee_hit_to_connection(host, Client_melee_hit_bundles[i]);
    }
}

// 0x009D5B90 (:1262): PT_EXPLOSION_HIT: attacker, target, owner, the explosion's position, weapon and explosion ids
// (0xFF for none).
void multi_gw_send_explosion_hit_to_connection(multi_connection* cp, const explosion_hit_info& eh_info)
{
    unsigned char packet[Packet_buffer_size];
    packet[0] = PT_EXPLOSION_HIT;
    unsigned short size = packet_header_size(PT_EXPLOSION_HIT);
    write_u16_be(packet, size, multi_core_network_handle_from_game_handle(eh_info.attacker, true));
    write_u16_be(packet, size, multi_core_network_handle_from_game_handle(eh_info.target, true));
    write_u16_be(packet, size, multi_core_network_handle_from_game_handle(eh_info.owner, true));
    mgw_write_vec(packet, size, ZERO_VECTOR, eh_info.explosion_pos, 1500.0f);
    packet[size++] = eh_info.w_info == nullptr ? 0xFF : (unsigned char)eh_info.w_info->unique_id;
    packet[size++] = eh_info.e_info == nullptr ? 0xFF : (unsigned char)eh_info.e_info->m_unique_id;
    packet_set_size(packet, size);
    mgw_send(cp, packet, size);
}

// 0x009D5F10 (:1299): the server's explosion verdict to every connection but its own (a client sends to the
// server).
void multi_gw_send_explosion_hit(const human* attacker, const human* victim, const human* owner, const vector& explosion_pos, const weapon_info* w_info, const explosion_info* e_info)
{
    explosion_hit_info eh_info;
    eh_info.attacker = attacker == nullptr ? 0xFFFFFFFF : attacker->get_handle();
    eh_info.target = victim == nullptr ? 0xFFFFFFFF : victim->get_handle();
    eh_info.owner = owner == nullptr ? 0xFFFFFFFF : owner->get_handle();
    eh_info.explosion_pos = explosion_pos;
    eh_info.w_info = w_info;
    eh_info.e_info = e_info;
    multi_session* session = multi_session_get_by_type(ST_GAME);
    if (Multiplayer == true && Server == true)
    {
        multi_connection* first = session->get_connection_list();
        for (multi_connection* cp = first; cp != nullptr;)
        {
            if (cp != session->get_me_connection())
            {
                multi_gw_send_explosion_hit_to_connection(cp, eh_info);
            }
            cp = cp->next;
            if (cp == first)
            {
                break;
            }
        }
        return;
    }
    multi_gw_send_explosion_hit_to_connection(session->get_host_connection(), eh_info);
}

// 0x009D5FF0 (:1339): a client applies the server's explosion hit (no relay).
void multi_gw_process_explosion_hit(unsigned char* packet_data, const sender& from)
{
    int offset = 0;
    const unsigned int attacker_handle = multi_core_game_handle_from_network_handle(read_u16_be(packet_data, offset), true);
    const unsigned int target_handle = multi_core_game_handle_from_network_handle(read_u16_be(packet_data, offset), true);
    const unsigned int owner_handle = multi_core_game_handle_from_network_handle(read_u16_be(packet_data, offset), true);
    vector explosion_pos;
    mgw_read_vec(packet_data + 6, explosion_pos, ZERO_VECTOR, 1500.0f);
    const weapon_info* w_info = weapons_get_weapon_info_from_weapon_unique_id(packet_data[12]);
    const explosion_info* e_info = explosion_lookup_by_unique_id(packet_data[13]);
    human* attacker = mgw_human_from_handle(attacker_handle);
    human* target = mgw_human_from_handle(target_handle);
    const object* owner = mgw_human_from_handle(owner_handle);
    if (target != nullptr)
    {
        human_combat_process_explosion_hit(target, attacker, w_info, explosion_pos, e_info, owner, false);
    }
}

// 0x009D6350 (:1384): PT_COLLISION_HIT to the server: the player, what hit it, the damage (whole).
void multi_gw_send_collision_hit(const human* hp, const object* other_obj, float damage)
{
    unsigned char packet[Packet_buffer_size];
    packet[0] = PT_COLLISION_HIT;
    unsigned short size = packet_header_size(PT_COLLISION_HIT);
    write_u16_be(packet, size, multi_core_network_handle_from_game_handle(hp->get_handle(), true));
    write_u16_be(packet, size, multi_core_network_handle_from_game_handle(other_obj->get_handle(), true));
    write_u16_be(packet, size, (unsigned short)(int)damage);
    packet_set_size(packet, size);
    multi_io_send_reliable(multi_session_get_by_type(ST_GAME)->get_host_connection(), packet, size, nullptr, nullptr, nullptr);
}

// 0x009F6D90 (:1404): the server damages the player hit by debris (credited to the destroyer of the mover).
void multi_gw_process_collision_hit(unsigned char* packet_data, const sender& from)
{
    int offset = 0;
    const unsigned short player_net = read_u16_be(packet_data, offset);
    if (player_net == 0xFFFF)
    {
        return;
    }
    const unsigned int player_handle = multi_core_game_handle_from_network_handle(player_net, true);
    human* hp = player_handle != 0xFFFFFFFF ? World.object_from_handle<player>(player_handle) : nullptr;
    if (hp == nullptr)
    {
        return;
    }
    const unsigned int other_handle = multi_core_game_handle_from_network_handle(read_u16_be(packet_data, offset), true);
    object* other_obj = World.object_from_handle(other_handle);
    human* attacker = nullptr;
    const weapon_info* w_info = nullptr;
    multi_gw_get_debris_kill_info(hp, other_obj, &attacker, &w_info);
    human_damage(hp, attacker, (float)read_u16_be(packet_data, offset), DBT_LEVEL_OBJECT, w_info, nullptr, true, false);
}

// 0x009D6530 (:1431): PT_FIRE_REPAIR to the server: the player and the position (raw floats).
void multi_gw_send_fire_repair(const player* pp, const vector& pos)
{
    unsigned char packet[Packet_buffer_size];
    packet[0] = PT_FIRE_REPAIR;
    unsigned short size = packet_header_size(PT_FIRE_REPAIR);
    write_u16_be(packet, size, multi_core_network_handle_from_game_handle(pp->get_handle(), true));
    mgw_write_float(packet, size, pos.x);
    mgw_write_float(packet, size, pos.y);
    mgw_write_float(packet, size, pos.z);
    packet_set_size(packet, size);
    multi_io_send_reliable(multi_session_get_by_type(ST_GAME)->get_host_connection(), packet, size, nullptr, nullptr, nullptr);
}

// 0x009D6750 (:1448): the server fires the player's reconstructor at the position.
void multi_gw_process_fire_repair(unsigned char* packet_data, const sender& from)
{
    int offset = 0;
    const unsigned short player_net = read_u16_be(packet_data, offset);
    if (player_net == 0xFFFF)
    {
        return;
    }
    const unsigned int handle = multi_core_game_handle_from_network_handle(player_net, true);
    human* hp = handle != 0xFFFFFFFF ? World.object_from_handle<player>(handle) : nullptr;
    if (hp == nullptr)
    {
        return;
    }
    const vector pos(mgw_read_float(packet_data, 2), mgw_read_float(packet_data, 6), mgw_read_float(packet_data, 10));
    weapon* wp = hp->get_equipped_weapon();
    if (wp != nullptr && wp->w_info->weapon_class == WEAPON_CLASS_REPAIR)
    {
        wp->fire_repair(pos);
    }
}

// 0x009D6920 (:1475): PT_IMPALING_COLLIDE to everyone: the sparks of a blade on a human (raw floats).
void multi_gw_send_impaling_collide(const vector& pos, const vector& direction, const weapon_info* w_info, unsigned char material_type, material_shape_type shape_type, const human* hit_obj, float speed)
{
    if (hit_obj == nullptr)
    {
        return;
    }
    unsigned char packet[Packet_buffer_size];
    packet[0] = PT_IMPALING_COLLIDE;
    unsigned short size = packet_header_size(PT_IMPALING_COLLIDE);
    mgw_write_float(packet, size, pos.x);
    mgw_write_float(packet, size, pos.y);
    mgw_write_float(packet, size, pos.z);
    mgw_write_float(packet, size, direction.x);
    mgw_write_float(packet, size, direction.y);
    mgw_write_float(packet, size, direction.z);
    packet[size++] = w_info == nullptr ? 0xFF : (unsigned char)w_info->unique_id;
    packet[size++] = material_type;
    packet[size++] = (unsigned char)shape_type;
    write_u16_be(packet, size, multi_core_network_handle_from_game_handle(hit_obj->get_handle(), false));
    mgw_write_float(packet, size, speed);
    packet_set_size(packet, size);
    multi_io_send_to_all(Game_session, packet, size);
}

// 0x009D6BD0 (:1500): the sparks on the hit human.
void multi_gw_process_impaling_collide(unsigned char* packet_data, const sender& from)
{
    const vector pos(mgw_read_float(packet_data, 0), mgw_read_float(packet_data, 4), mgw_read_float(packet_data, 8));
    const vector direction(mgw_read_float(packet_data, 12), mgw_read_float(packet_data, 16), mgw_read_float(packet_data, 20));
    const weapon_info* w_info = weapons_get_weapon_info_from_weapon_unique_id(packet_data[24]);
    const unsigned char material_type = packet_data[25];
    const unsigned char shape_type = packet_data[26];
    int offset = 27;
    const unsigned short hit_net = read_u16_be(packet_data, offset);
    if (hit_net == 0xFFFF)
    {
        return;
    }
    const unsigned int hit_handle = multi_core_game_handle_from_network_handle(hit_net, true);
    const object* hit_obj = hit_handle != 0xFFFFFFFF ? World.object_from_handle(hit_handle) : nullptr;
    if (hit_obj != nullptr && hit_obj->get_obj_type() == OT_HUMAN)
    {
        bullet_spark_create(pos, direction, w_info, material_type, (material_shape_type)shape_type, hit_obj, 0xFFFFFFFF, mgw_read_float(packet_data, 29));
    }
}

// 0x00967450 (:1534): the weapon info that stands for a level kill (Multi_custom_weapon_names).
const weapon_info* multi_gw_get_custom_weapon_info(multi_custom_weapons wep)
{
    if (wep < NUM_MULTI_CUSTOM_WEAPONS)
    {
        return weapons_get_weapon_info(Multi_custom_weapon_names[wep]);
    }
    return nullptr;
}

// 0x009E26F0 (:1548): debris kills: the debris weapon, credited to the last player who damaged the rfg mover (not a
// teammate without friendly fire).
void multi_gw_get_debris_kill_info(const human* hp, const object* other_obj, human** attacker, const weapon_info** w_info)
{
    if (attacker == nullptr || w_info == nullptr)
    {
        return;
    }
    *w_info = weapons_get_weapon_info(Multi_custom_weapon_names[MCW_DEBRIS]);
    if (other_obj != nullptr && other_obj->get_obj_type() == OT_MOVER && other_obj->get_sub_type() == OT_SUB_MOVER_RFG)
    {
        player* destroyer = multi_game_last_player_to_damage_rfg_mover((const rfg_mover*)other_obj);
        *attacker = destroyer;
        if (destroyer == nullptr || human_teams_get_disposition(destroyer, hp) == TEAM_DISPOSITION_HOSTILE || Mp_server_options.generic_options.friendly_fire)
        {
            return;
        }
    }
    *attacker = nullptr;
}

// 0x00987300 (:1569): the multiplayer-selectable weapons in table order.
unsigned int multi_gw_get_first_weapon_info_handle()
{
    const unsigned int handle = weapons_get_first_weapon_info_handle();
    const weapon_info* w_info = weapons_get_weapon_info(handle);
    if (w_info == nullptr)
    {
        return 0xFFFFFFFF;
    }
    if (w_info->flags.mp_selectable)
    {
        return handle;
    }
    return multi_gw_get_next_weapon_info_handle(handle);
}

// 0x00987330 (:1584)
unsigned int multi_gw_get_last_weapon_info_handle()
{
    const unsigned int handle = weapons_get_last_weapon_info_handle();
    const weapon_info* w_info = weapons_get_weapon_info(handle);
    if (w_info == nullptr)
    {
        return 0xFFFFFFFF;
    }
    if (w_info->flags.mp_selectable)
    {
        return handle;
    }
    return multi_gw_get_prev_weapon_info_handle(handle);
}

// 0x00967470 (:1599)
unsigned int multi_gw_get_prev_weapon_info_handle(unsigned int handle)
{
    if (handle == 0xFFFFFFFF)
    {
        return 0xFFFFFFFF;
    }
    for (unsigned int prev = weapons_get_prev_weapon_info_handle(handle); prev != 0xFFFFFFFF;)
    {
        const weapon_info* w_info = weapons_get_weapon_info(prev);
        if (w_info == nullptr)
        {
            break;
        }
        if (w_info->flags.mp_selectable)
        {
            return prev;
        }
        prev = weapons_get_prev_weapon_info_handle(prev);
    }
    return 0xFFFFFFFF;
}

// 0x009674C0 (:1623)
unsigned int multi_gw_get_next_weapon_info_handle(unsigned int handle)
{
    if (handle == 0xFFFFFFFF)
    {
        return 0xFFFFFFFF;
    }
    for (unsigned int next = weapons_get_next_weapon_info_handle(handle); next != 0xFFFFFFFF;)
    {
        const weapon_info* w_info = weapons_get_weapon_info(next);
        if (w_info == nullptr)
        {
            break;
        }
        if (w_info->flags.mp_selectable)
        {
            return next;
        }
        next = weapons_get_next_weapon_info_handle(next);
    }
    return 0xFFFFFFFF;
}
