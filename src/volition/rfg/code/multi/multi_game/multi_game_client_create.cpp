// Original: volition/rfg/code/multi/multi_game/multi_game_client_create.cpp
//
// Decompiled whole (multiplayer.md MP8). Client-created projectiles: a client fires its own projectile at once and
// tells its host with PT_CLIENT_OBJECT_CREATE (the projectile's create packet, with a create id: the connection index
// and a counter); the host creates the projectile led by twice the client's ping (at most 0.4 s, under gravity), with
// the collision along the lead, and its create packet carries the id back so the client's projectile becomes the
// remote one. A dead player's create is refused with PT_CLIENT_CREATE_NO and the client's projectile is destroyed.
// Part of the multi unity TU (rfg_multi_unity.obj, every multi/ .cpp in path order): the file statics are shared
// by the whole tree, and a shared one is placed in its first user (tools/hdrgen/specs/mp0_stubs.py).

#include "volition/rfg/code/multi/multi_game/multi_game_client_create.h"

#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/havok/havok_collision_events.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/level/world.inl"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_core/include/multi_connection.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_io.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_packet_types.h"
#include "volition/rfg/code/multi/multi_core/include/multi_session.h"
#include "volition/rfg/code/multi/multi_game/multi_game_data.h"
#include "volition/rfg/code/multi/multi_game/multi_game_object_projectile.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/projectile.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/vector3.h"

#include <string.h>

// ---- file-static globals
static projectile_multi_registration* Projectile_mr; // 0x02FE5BA4

// ---- internal functions
static void multi_client_create_projectile(projectile* proj); // 0x009BD090 (:85)
static void multi_game_send_client_create_no(multi_connection* cp, unsigned int create_id); // 0x009BD1A0 (:123)
static void multi_server_create_client_projectile(unsigned char* packet_data, int packet_offset, const sender& from); // 0x009DD4B0 (:136)

// 0x009BD090 (:85)
// PT_CLIENT_OBJECT_CREATE to the host: the projectile's create packet, flushed at once (the send job's cpu given up;
// multi_connection_flush_current_packet inlined).
static void multi_client_create_projectile(projectile* proj)
{
    unsigned char packet[1200];
    packet[0] = PT_CLIENT_OBJECT_CREATE;
    unsigned short size = Multi_packets[PT_CLIENT_OBJECT_CREATE].group_id != 0 ? 5 : 3;
    remote_object rop;
    rop.m_game_handle = proj->get_handle();
    ((const multi_core_object_registration*)Projectile_mr)->send_create_packet(&rop, packet, size, nullptr);
    const unsigned short payload = (unsigned short)(size - 3);
    packet[1] = (unsigned char)(payload >> 8);
    packet[2] = (unsigned char)payload;
    multi_session* session = multi_session_get_by_type(ST_GAME);
    multi_connection* cp = session != nullptr ? session->get_host_connection() : nullptr;
    multi_io_send_reliable(cp, packet, size, nullptr, nullptr, nullptr);
    multi_connection_flush_current_packet(cp); // inlined
}

// 0x009BD1A0 (:123): PT_CLIENT_CREATE_NO: the create id (big endian)
// LTCG: cp in ECX (the port has no register convention).
static void multi_game_send_client_create_no(multi_connection* cp, unsigned int create_id)
{
    unsigned char packet[1200];
    packet[0] = PT_CLIENT_CREATE_NO;
    const int header = Multi_packets[PT_CLIENT_CREATE_NO].group_id != 0 ? 5 : 3;
    packet[header] = (unsigned char)(create_id >> 24);
    packet[header + 1] = (unsigned char)(create_id >> 16);
    packet[header + 2] = (unsigned char)(create_id >> 8);
    packet[header + 3] = (unsigned char)create_id;
    const unsigned short payload = (unsigned short)(header + 1);
    packet[1] = (unsigned char)(payload >> 8);
    packet[2] = (unsigned char)payload;
    multi_io_send_reliable(cp, packet, header + 4, nullptr, nullptr, nullptr);
}

// 0x009DD4B0 (:136)
// The host's copy of a client's projectile: led by twice the client's mean ping (at most 400 ms) along its velocity
// and gravity, facing its velocity, simulated. A client's mine or singularity bomb first tries the throw collision
// from the thrower's head; otherwise a collision along the lead places the projectile at the hit and collides it.
// Original: the sender's connection is used before it is checked.
// LTCG: from in ECX (the port has no register convention).
static void multi_server_create_client_projectile(unsigned char* packet_data, int packet_offset, const sender& from)
{
    player* pp = multi_game_data_get_player_instance(from.connection->game_data);
    if (pp == nullptr)
    {
        return;
    }
    remote_projectile_create_info create_info;
    multi_game_object_extract_projectile_create_info(create_info, packet_data, packet_offset);
    if (pp->cur_state == HS_DEAD)
    {
        multi_game_send_client_create_no(from.connection, create_info.create_id);
        return;
    }
    const unsigned int owner_handle = multi_core_game_handle_from_network_handle(create_info.owner_net_handle, false);
    if (from.connection == nullptr)
    {
        return;
    }
    int lead_ms = (int)from.connection->mean_ping * 2;
    if (lead_ms > 399) // Magic number: at most 400 ms
    {
        lead_ms = 400;
    }
    const float lead = (float)lead_ms * 0.001f;
    const vector start_pos = create_info.pos;
    vector pos(create_info.velocity.x * lead + create_info.pos.x, create_info.velocity.y * lead + create_info.pos.y, create_info.velocity.z * lead + create_info.pos.z);
    vector velocity = create_info.velocity;
    const float gravity = weapons_get_projectile_gravity(create_info.w_info);
    velocity.y = gravity * lead + velocity.y;
    pos.y = lead * lead * gravity * 0.5f + pos.y;
    matrix orient;
    orient.make(velocity.copy_normalize_safe());
    obj_item_flags flags;
    memset(&flags, 0, sizeof(flags));
    flags.cast_drop_shadows = 1;
    flags.cast_shadows = 1;
    flags.simulate = 1;
    obj_projectile_props props(owner_handle, create_info.w_info->projectile_info.obj_item_info, flags, create_info.w_info, pos, orient, velocity, ZERO_VECTOR, create_info.create_id);
    projectile* proj = World.create_object<projectile>(props, 0xFFFFFFFF);
    // Original: the projectile is used unchecked.
    if (create_info.try_launch_attach && (proj->w_info->weapon_class == WEAPON_CLASS_THROWN_MINE || proj->w_info->weapon_class == WEAPON_CLASS_SINGULARITY_BOMB))
    {
        const vector throw_start = create_info.raycast_start_pos;
        const vector throw_end(create_info.raycast_start_pos.x, create_info.raycast_start_pos.y + create_info.raycast_delta_pos_y, create_info.raycast_start_pos.z);
        if (weapon_projectile_throw_collide(throw_start, throw_end, proj))
        {
            return;
        }
    }
    collision_event c_event;
    object* collide_obj;
    if (proj->get_mp_collision_info(start_pos, pos, &collide_obj, c_event) && c_event.m_num_collisions != 0)
    {
        proj->update_pos_and_orient(c_event.m_collisions[0].m_collision_point, orient, true);
        proj->collide(&c_event, collide_obj);
    }
}

// 0x009511D0 (:235): how far (in s at its horizontal speed) the server's create position is ahead of the projectile
float multi_game_ca_projectile_time_offset_sec(const projectile* proj, const vector& server_create_pos)
{
    if (proj->havok_handle == 0xFFFFFFFF)
    {
        return 0.0f;
    }
    vector velocity;
    havok_body_get_linear_velocity(proj->havok_handle, velocity);
    vector flat_dir(velocity.x, 0.0f, velocity.z);
    const float flat_speed = flat_dir.mag_and_normalize_safe();
    return ((server_create_pos.z - proj->pos.z) * flat_dir.z + (server_create_pos.x - proj->pos.x) * flat_dir.x + (server_create_pos.y - proj->pos.y) * flat_dir.y) / flat_speed;
}

// 0x00997600 (:255): the connection index in the low byte, a running counter above (0xFFFFFFFF outside a game)
void multi_game_get_client_create_unique_id(unsigned int& unique_id)
{
    // Placement inferred: rfg.pdb has no file for it (its only writer is here).
    static unsigned int Client_create_id; // 0x03017C20
    unique_id = 0xFFFFFFFF;
    multi_session* session = multi_session_get_by_type(ST_GAME);
    if (session == nullptr || session->get_me_connection() == nullptr)
    {
        return;
    }
    const unsigned int counter = Client_create_id << 8;
    Client_create_id++;
    unique_id = session->get_me_connection()->index_in_session | counter;
}

// 0x009BD260 (:272)
void multi_game_client_create(projectile* proj)
{
    if (proj != nullptr)
    {
        multi_client_create_projectile(proj);
    }
}

// 0x009DD850 (:279): PT_CLIENT_OBJECT_CREATE
void multi_game_process_client_object_create(unsigned char* packet_data, const sender& from)
{
    multi_server_create_client_projectile(packet_data, 0, from);
}

// 0x00951270 (:287): PT_CLIENT_CREATE_NO: the refused projectile goes
void multi_game_process_client_create_no(unsigned char* packet_data, const sender& from)
{
    unsigned int create_id = ((unsigned int)packet_data[0] << 24) | ((unsigned int)packet_data[1] << 16) | ((unsigned int)packet_data[2] << 8) | packet_data[3];
    projectile* proj = projectile_find_by_create_id(create_id);
    if (proj != nullptr)
    {
        proj->remote_obj_flags.client_destroy_permission = 1;
        World.destroy_object(proj, false);
    }
}

// 0x009512F0 (:304): the weapons whose projectiles (and bullets) a client creates itself
bool multi_game_client_creates_weapon(const weapon_info* info)
{
    switch (info->weapon_class)
    {
    case WEAPON_CLASS_THROWN_CHARGE:
    case WEAPON_CLASS_THROWN_MINE:
    case WEAPON_CLASS_THROWN_GRENADE:
    case WEAPON_CLASS_LAUNCHER:
    case WEAPON_CLASS_GRINDER:
    case WEAPON_CLASS_NANO_RIFLE:
    case WEAPON_CLASS_REPAIR:
    case WEAPON_CLASS_BULLET:
    case WEAPON_CLASS_SINGULARITY_BOMB:
        return true;
    default:
        return false;
    }
}
