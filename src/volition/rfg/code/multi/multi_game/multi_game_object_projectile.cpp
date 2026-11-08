// Original: volition/rfg/code/multi/multi_game/multi_game_object_projectile.cpp
//
// Decompiled whole (multiplayer.md MP8). The replication of projectiles: a projectile the server creates (or a
// client's own, matched by its create id) is a remote object of type 1 on every peer. The create packet carries the
// position, a 24-bit orientation, the owner, the velocity and the weapon (and for a client-created throw the thrower's
// head offset); the state is the position, the orientation, the linear velocity and the attachment (ballistic, free,
// constrained to an object or parented to a human's bone), steered on the client towards the server's state. The server
// keeps at most 90 projectiles (64 younger than 5 s), mines and charges excepted.
// Part of the multi unity TU (rfg_multi_unity.obj, every multi/ .cpp in path order): the file statics are shared
// by the whole tree, and a shared one is placed in its first user (tools/hdrgen/specs/mp0_stubs.py).

#include "volition/rfg/code/multi/multi_game/multi_game_object_projectile.h"

#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/rfg/code/havok/constraint_template.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/havok/havok_collision_events.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/level/world.inl"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_core/include/multi_connection.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_compression.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_delta_compression.h"
#include "volition/rfg/code/multi/multi_core/include/multi_pool.inl"
#include "volition/rfg/code/multi/multi_game/multi_game.h"
#include "volition/rfg/code/multi/multi_game/multi_game_client_create.h"
#include "volition/rfg/code/multi/multi_game/multi_game_data.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/object_constraint.h"
#include "volition/rfg/code/objects/object_type.h"
#include "volition/rfg/code/objects/projectile.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/vlib/code/math/matrix_interpolate.h"
#include "volition/vlib/code/math/quaternion.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/os/frametime.h"

#include <math.h>
#include <string.h>

// ---- file-static globals
// const float PI (0x01309C38) is used here too; placed in multi_game_client_predict.cpp.
// const float SERVER_FUEL_FUDGE_FACTOR (0x01309DE0) is used here too; placed in multi_game_object_backpack.cpp.
static multi_core_state_priority State_priority_base_projectile[5] = {MULTI_CORE_STATE_PRIORITY_LOW, MULTI_CORE_STATE_PRIORITY_LOW, MULTI_CORE_STATE_PRIORITY_LOW, MULTI_CORE_STATE_PRIORITY_MEDIUM, MULTI_CORE_STATE_PRIORITY_LOW}; // 0x0167F110
static unsigned int State_mask_projectile[5] = {3, 0xC, 0x30, 0x40, 0x80}; // 0x0167F124
// The client's steering towards the server state (move): corrections within 2 m blend by 3 x the frame time, at least
// 0.1 m off; the corrective velocity is at most 0.5 m/s.
static float Mp_max_pos_diff = 2.0f; // 0x0167F138
static float Min_corrective_pos = 0.1f; // 0x0167F13C
static float Mp_factor = 3.0f; // 0x0167F140
static float Max_corrective_vel = 0.5f; // 0x0167F144
// A ballistic projectile is reset onto its ballistic arc beyond these errors (m, m, s).
static float Max_delta_pos_error_y = 0.2f; // 0x0167F148
static float Max_delta_pos_cross_track_erorr = 0.1f; // 0x0167F14C
static float Max_time_error = 0.1f; // 0x0167F150
// dynamic_uint_hash_table<remote_object*> Game_handle_to_rop_hash (0x01691B40) is used here too; placed in multi_core_delta_compression.cpp.
// dynamic_uint_hash_table<network_game_handle*> Reserved_game_to_network_hash (0x01691C18) is used here too; placed in multi_core_objects.cpp.
static multi_pool<object_state_data<projectile_data_bundle>, 2400> Object_projectile_allocator; // 0x01692F50
static multi_pool<multi_sim_state<multi_projectile_state>, 96> Object_projectile_sim_state_allocator; // 0x01692F90
// projectile_multi_registration* Projectile_mr (0x02FE5BA4) is used here too; placed in multi_game_client_create.cpp.
// unsigned int Clock_current_time (0x02FEB538) is used here too; placed in multi.cpp.
// bool Server (0x02FEB590) is used here too; placed in multi_core_session_generic.cpp.

// ---- internal functions
static void debug_spew_projectile(const char*, ...); // 0x0095BA80 (:265)
static const char* get_attach_type_text(int attach_type); // 0x0095BA90 (:275)
static short get_attach_index(const object* attached_to_object, int raw_attach_index); // 0x0095BAD0 (:286)
static bool should_reset_ballistic_pos_velocity(projectile* pjp); // 0x0097EC00 (:299)
static object_state_data<projectile_data_bundle>* projectile_state_data_alloc_state(); // 0x009F5AC0 (:415)
static multi_sim_state<multi_projectile_state>* projectile_alloc_sim_state(); // 0x0099C580 (:430)
static unsigned short query_possible_mask_bits_base(const projectile_data_bundle& cur_data, const projectile_data_bundle& ref_data); // 0x0095BB00 (:442)
static int projectile_get_ca_time_offset(projectile* pjp); // 0x0095C1C0 (:772)
static int projectile_get_attach_type(const projectile* proj); // 0x0097EF10 (:795)
static void spew_constraint_state(const projectile* proj, const projectile_data_bundle& result_data); // 0x009CBE90 (:1586)
static void spew_child_state(const projectile_data_bundle& result_data); // 0x009CBFE0 (:1601)
static void spew_no_attach_state(unsigned int attach_state); // 0x0095C8C0 (:1616)

// The projectile of a game handle (null for any other object).
static projectile* projectile_from_handle(unsigned int game_handle)
{
    if (game_handle == 0xFFFFFFFF)
    {
        return nullptr;
    }
    object* objp = World.object_from_handle(game_handle);
    if (objp == nullptr || objp->get_obj_type() != OT_ITEM || objp->get_sub_type() != OT_SUB_ITEM_PROJECTILE)
    {
        return nullptr;
    }
    return (projectile*)objp;
}

// The constraint object of a handle (null for any other object).
static constraint* projectile_constraint_from_handle(unsigned int handle)
{
    if (handle == 0xFFFFFFFF)
    {
        return nullptr;
    }
    object* objp = World.object_from_handle(handle);
    if (objp == nullptr || objp->get_obj_type() != OT_CONSTRAINT)
    {
        return nullptr;
    }
    return (constraint*)objp;
}

static projectile_data_bundle& projectile_base(dc_state_list* dcsl)
{
    return ((object_state_data<projectile_data_bundle>*)dcsl->m_data)->base_data;
}

static projectile_data_bundle& projectile_transmitted(dc_state_list* dcsl, const multi_connection* cp)
{
    return ((object_state_data<projectile_data_bundle>*)dcsl->m_data)->transmitted_data[cp->index_in_session];
}

// short_quat -> quaternion (inlined everywhere): each component times 1 / 16383 (6.103888e-05).
static quaternion projectile_short_quat_to_quat(const short_quat& sq)
{
    return quaternion((float)((double)sq.x * 6.103888e-05f), (float)((double)sq.y * 6.103888e-05f), (float)((double)sq.z * 6.103888e-05f), (float)((double)sq.w * 6.103888e-05f));
}

// 0x0057E840 short_quat::operator=(const quaternion&) (not declared in quaternion.h): each component times 16383,
// rounded half away from zero.
static void projectile_quat_to_short_quat(short_quat& sq, const quaternion& q)
{
    const float scaled[4] = {q.x * 16383.0f, q.y * 16383.0f, q.z * 16383.0f, q.w * 16383.0f};
    short out[4];
    for (int i = 0; i < 4; i++)
    {
        out[i] = (short)(scaled[i] < 0.0f ? scaled[i] - 0.5f : scaled[i] + 0.5f);
    }
    sq.x = out[0];
    sq.y = out[1];
    sq.z = out[2];
    sq.w = out[3];
}

// Big-endian floats and words of the create packet.
static void projectile_write_float(unsigned char* packet_data, unsigned short& packet_size, float value)
{
    unsigned int bits;
    memcpy(&bits, &value, 4);
    packet_data[packet_size] = (unsigned char)(bits >> 24);
    packet_data[packet_size + 1] = (unsigned char)(bits >> 16);
    packet_data[packet_size + 2] = (unsigned char)(bits >> 8);
    packet_data[packet_size + 3] = (unsigned char)bits;
    packet_size += 4;
}

static unsigned int projectile_read_u32(const unsigned char* packet_data, int& packet_offset)
{
    const unsigned char* p = packet_data + packet_offset;
    packet_offset += 4;
    return ((unsigned int)p[0] << 24) | ((unsigned int)p[1] << 16) | ((unsigned int)p[2] << 8) | p[3];
}

static float projectile_read_float(const unsigned char* packet_data, int& packet_offset)
{
    const unsigned int bits = projectile_read_u32(packet_data, packet_offset);
    float value;
    memcpy(&value, &bits, 4);
    return value;
}

// 0x0095BA80 (:265): compiled out
static void debug_spew_projectile(const char*, ...)
{
}

// 0x0095BA90 (:275)
// LTCG: attach_type in EAX (the port has no register convention).
static const char* get_attach_type_text(int attach_type)
{
    switch (attach_type)
    {
    case 0:
        return "ballistic";
    case 1:
        return "not attach";
    case 2:
        return "constraint";
    case 3:
        return "child";
    default:
        return "unknown";
    }
}

// 0x0095BAD0 (:286): the attach index on an rfg mover or a vehicle, none on anything else
// LTCG: attached_to_object in ECX (the port has no register convention).
static short get_attach_index(const object* attached_to_object, int raw_attach_index)
{
    if ((attached_to_object->get_obj_type() != OT_MOVER || attached_to_object->get_sub_type() != OT_SUB_MOVER_RFG) && attached_to_object->get_obj_type() != OT_VEHICLE)
    {
        return -1;
    }
    return (short)raw_attach_index;
}

// 0x0097EC00 (:299)
// With a body and on its ballistic arc (no rocket flight, or one past its drop time): whether the body strayed from the
// arc at the distance it travelled along the start direction (more than 0.2 m up or down or 0.1 m sideways) or from
// the time the arc gives for that distance (more than 0.1 s from the age, at most 7 s).
static bool should_reset_ballistic_pos_velocity(projectile* pjp)
{
    if (pjp->havok_handle == 0xFFFFFFFF)
    {
        return false;
    }
    const float time_until_drop = pjp->w_info->projectile_info.time_until_drop;
    if ((pjp->w_info->projectile_info.flags & 1) != 0 && time_until_drop > 0.0f && (float)pjp->get_age() > time_until_drop) // 0x1: rocket flight
    {
        return false;
    }
    vector pos;
    matrix orient;
    havok_body_get_pos_orient(pjp->havok_handle, pos, orient);
    const vector start_pos = pjp->get_start_pos();
    const vector start_velocity = pjp->get_start_velocity();
    const float dx = pos.x - start_pos.x;
    const float dz = pos.z - start_pos.z;
    vector flat_dir(start_velocity.x, 0.0f, start_velocity.z);
    const float flat_speed = flat_dir.mag_and_normalize_safe();
    const float time_sec = (dx * flat_dir.x + flat_dir.y * 0.0f + dz * flat_dir.z) / flat_speed;
    int age = pjp->get_age();
    if (age > 6999) // Magic number: at most 7 s
    {
        age = 7000;
    }
    vector ballistic_pos;
    vector ballistic_vel;
    pjp->get_ballistic_position_velocity_time(ballistic_pos, ballistic_vel, time_sec);
    const bool off_arc = Max_delta_pos_error_y < (float)fabs((double)(ballistic_pos.y - pos.y)) || Max_delta_pos_cross_track_erorr < (float)fabs((double)((ballistic_pos.x - pos.x) * flat_dir.z + (ballistic_pos.y - pos.y) * 0.0f + (ballistic_pos.z - pos.z) * -flat_dir.x));
    return Max_time_error < (float)fabs((double)((float)age * 0.001f - time_sec)) || off_arc;
}

// 0x009F5AC0 (:415): no parent yet
static object_state_data<projectile_data_bundle>* projectile_state_data_alloc_state()
{
    object_state_data<projectile_data_bundle>* data = Object_projectile_allocator.alloc(0);
    if (data == nullptr)
    {
        multi_core_dc_flush_old_object_states();
        data = Object_projectile_allocator.alloc(0);
        if (data == nullptr)
        {
            return nullptr;
        }
    }
    data->base_data.parent_net_handle = 0xFFFF;
    return data;
}

// 0x0099C580 (:430)
static multi_sim_state<multi_projectile_state>* projectile_alloc_sim_state()
{
    multi_sim_state<multi_projectile_state>* sim_state = Object_projectile_sim_state_allocator.alloc(0);
    if (sim_state == nullptr)
    {
        keen::debug::nativePrintArgs("Out of projectile sim states!");
        return nullptr;
    }
    sim_state->reset();
    return sim_state;
}

// 0x0095BB00 (:442)
// A ballistic projectile sends only its attachment (the arc is computed on both sides); a free one its position,
// velocity and orientation as they changed; a changed attachment (or a constrained one's parent or prop point) sends
// the attachment, alone with the flags unless the projectile is free; a changed fade sends the flags.
// LTCG: cur_data in EDI, ref_data in ESI (the port has no register convention).
static unsigned short query_possible_mask_bits_base(const projectile_data_bundle& cur_data, const projectile_data_bundle& ref_data)
{
    unsigned int mask = 0;
    if (cur_data.projectile_flags.attach_type == 0)
    {
        return 0x40;
    }
    if (cur_data.projectile_flags.attach_type == 1)
    {
        const unsigned int pos_mask = Projectile_mr->get_vector_mask(cur_data.position, ref_data.position, 1, 2, 3, 0xFFFFFFFF);
        const unsigned int vel_mask = Projectile_mr->get_vector_mask(cur_data.linear_velocity, ref_data.linear_velocity, 0x10, 0x20, 0x30, 0xFFFFFFFF);
        const unsigned int orient_mask = Projectile_mr->get_orient_mask(cur_data.orientation, ref_data.orientation, 4, 8, 0);
        mask = (pos_mask & 0xFFFF) | orient_mask | vel_mask;
    }
    const unsigned char attach_type = cur_data.projectile_flags.attach_type;
    if (attach_type != ref_data.projectile_flags.attach_type || (attach_type == 2 && (cur_data.parent_net_handle != ref_data.parent_net_handle || cur_data.parent_prop_point != ref_data.parent_prop_point)))
    {
        mask |= 0x40;
    }
    if (cur_data.projectile_flags.fading_out != ref_data.projectile_flags.fading_out)
    {
        mask |= 0x80;
    }
    if ((mask & 0x40) != 0 && attach_type != 1)
    {
        mask &= 0xC0;
    }
    return (unsigned short)mask;
}

// 0x0095BBF0 (:501): never sent data sets are added
unsigned short projectile_multi_registration::query_possible_mask_bits(dc_state_list* current_state, dc_state_list* reference_state, multi_connection* cp) const
{
    const projectile_data_bundle& cur = projectile_base(current_state);
    if (cur.projectile_flags.attach_type == 0)
    {
        return 0x40;
    }
    if (cur.projectile_flags.no_sync)
    {
        return 0x80;
    }
    if (reference_state == nullptr)
    {
        return 0xFF;
    }
    const projectile_data_bundle& client = projectile_transmitted(reference_state, cp);
    return (unsigned short)(~client.transmitted_data_set & 0xFF) | query_possible_mask_bits_base(cur, client);
}

// 0x0095BC70 (:539)
void projectile_multi_registration::create(unsigned int game_handle) const
{
    object* objp = World.object_from_handle(game_handle);
    if (objp != nullptr)
    {
        objp->remote_obj_flags.remote_object = 1;
    }
}

// 0x009CA300 (:554)
// A flags byte (1: a create id follows; 2: a client's mine or singularity bomb, with the thrower's head offset), the
// position, the orientation (pitch, bank and heading, 8 bits each), the owner's network handle, the velocity and the
// weapon's unique id. A server marks the projectile remote.
// Original: the orientation is packed with mat_pbh_delta_compress(packer, orient, IDENTITY_MATRIX, ...), whose
// arguments are (from, to): the receiver unpacks the identity's angles relative to the orientation, its transpose.
void projectile_multi_registration::send_create_packet(const remote_object* rop, unsigned char* packet_data, unsigned short& packet_size, multi_connection* dest_conn) const
{
    projectile* pjp = projectile_from_handle(rop->m_game_handle);
    const weapon_class_type weapon_class = pjp->w_info->weapon_class;
    const bool head_offset = Multiplayer && !Server && (weapon_class == WEAPON_CLASS_THROWN_MINE || weapon_class == WEAPON_CLASS_SINGULARITY_BOMB);
    const unsigned char flags = (unsigned char)((pjp->mp_create_id != 0xFFFFFFFF ? 1 : 0) | (head_offset ? 2 : 0));
    packet_data[packet_size] = flags;
    packet_size++;
    if ((flags & 1) != 0)
    {
        const unsigned int create_id = pjp->mp_create_id;
        packet_data[packet_size] = (unsigned char)(create_id >> 24);
        packet_data[packet_size + 1] = (unsigned char)(create_id >> 16);
        packet_data[packet_size + 2] = (unsigned char)(create_id >> 8);
        packet_data[packet_size + 3] = (unsigned char)create_id;
        packet_size += 4;
    }
    projectile_write_float(packet_data, packet_size, pjp->pos.x);
    projectile_write_float(packet_data, packet_size, pjp->pos.y);
    projectile_write_float(packet_data, packet_size, pjp->pos.z);
    {
        unsigned char orient_bits[4] = {};
        bit_packer packer(orient_bits, 3);
        mat_pbh_delta_compress(packer, pjp->orient, IDENTITY_MATRIX, 8, 3.1415927f, 0.0f, 1.0f);
        for (int i = 0; i < 3; i++)
        {
            packet_data[packet_size] = orient_bits[i];
            packet_size++;
        }
    }
    if ((flags & 2) != 0)
    {
        // The local player's head relative to the projectile (8 bits each, within 4 m) and the head's height above
        // the player's position.
        unsigned char head_bits[4] = {};
        bit_packer packer(head_bits, 4);
        vector head_pos = pjp->pos;
        player* local_player = player_get_local_player();
        float head_drop = 0.0f;
        if (local_player != nullptr)
        {
            human_get_head_pos(local_player, head_pos);
            head_drop = local_player->pos.y - head_pos.y;
        }
        vec_delta_compress(packer, pjp->pos, head_pos, 8, 4.0f, 0.0f, 1.0f);
        delta_compress(packer, 0.0f, head_drop, 8, 4.0f, 0.0f, 1.0f);
        for (int i = 0; i < 4; i++)
        {
            packet_data[packet_size] = head_bits[i];
            packet_size++;
        }
    }
    const unsigned short owner_net_handle = pjp->owner_handle == 0xFFFFFFFF ? 0xFFFF : multi_core_network_handle_from_game_handle(pjp->owner_handle, false); // inlined
    packet_data[packet_size] = (unsigned char)(owner_net_handle >> 8);
    packet_data[packet_size + 1] = (unsigned char)owner_net_handle;
    packet_size += 2;
    vector velocity;
    if (pjp->havok_handle == 0xFFFFFFFF)
    {
        velocity.zero();
    }
    else
    {
        havok_body_get_linear_velocity(pjp->havok_handle, velocity);
    }
    projectile_write_float(packet_data, packet_size, velocity.x);
    projectile_write_float(packet_data, packet_size, velocity.y);
    projectile_write_float(packet_data, packet_size, velocity.z);
    packet_data[packet_size] = (unsigned char)pjp->w_info->unique_id;
    packet_size++;
    if (Multiplayer && Server)
    {
        pjp->remote_obj_flags.remote_object = 1;
    }
}

// 0x0095BC90 (:622): the create packet's fields (send_create_packet)
void multi_game_object_extract_projectile_create_info(remote_projectile_create_info& create_info, unsigned char* packet_data, int& packet_offset)
{
    const unsigned char flags = packet_data[packet_offset];
    packet_offset++;
    const bool head_offset = ((flags >> 1) & 1) != 0;
    create_info.try_launch_attach = head_offset;
    if ((flags & 1) == 0)
    {
        create_info.create_id = 0xFFFFFFFF;
    }
    else
    {
        create_info.create_id = projectile_read_u32(packet_data, packet_offset);
    }
    create_info.pos.x = projectile_read_float(packet_data, packet_offset);
    create_info.pos.y = projectile_read_float(packet_data, packet_offset);
    create_info.pos.z = projectile_read_float(packet_data, packet_offset);
    {
        unsigned char orient_bits[4] = {packet_data[packet_offset], packet_data[packet_offset + 1], packet_data[packet_offset + 2], 0};
        packet_offset += 3;
        bit_packer packer(orient_bits, 3);
        mat_pbh_delta_decompress(packer, create_info.orient, IDENTITY_MATRIX, 8, 3.1415927f, 0.0f, 1.0f);
    }
    if (head_offset)
    {
        unsigned char head_bits[4] = {packet_data[packet_offset], packet_data[packet_offset + 1], packet_data[packet_offset + 2], packet_data[packet_offset + 3]};
        packet_offset += 4;
        bit_packer packer(head_bits, 4);
        vec_delta_decompress(packer, create_info.raycast_start_pos, create_info.pos, 8, 4.0f, 0.0f, 1.0f);
        create_info.raycast_delta_pos_y = delta_decompress(packer, 0.0f, 8, 4.0f, 0.0f, 1.0f);
    }
    create_info.owner_net_handle = (unsigned short)((packet_data[packet_offset] << 8) | packet_data[packet_offset + 1]);
    packet_offset += 2;
    create_info.velocity.x = projectile_read_float(packet_data, packet_offset);
    create_info.velocity.y = projectile_read_float(packet_data, packet_offset);
    create_info.velocity.z = projectile_read_float(packet_data, packet_offset);
    const unsigned char unique_id = packet_data[packet_offset];
    packet_offset++;
    create_info.w_info = weapons_get_weapon_info_from_weapon_unique_id(unique_id);
}

// 0x009CAAF0 (:671)
// The client's own projectile with the create id takes the server's network handle (its clock offset to the server's
// create position decays over 300 ms, projectile_get_ca_time_offset); otherwise the projectile is created here. Either
// way it gets received states and is remote.
void projectile_multi_registration::process_create_packet(unsigned int& game_handle, unsigned short net_handle, int& packet_offset, unsigned char* packet_data) const
{
    remote_projectile_create_info create_info;
    multi_game_object_extract_projectile_create_info(create_info, packet_data, packet_offset);
    const unsigned int owner_handle = multi_core_game_handle_from_network_handle(create_info.owner_net_handle, false);
    projectile* pjp = projectile_find_by_create_id(create_info.create_id);
    if (pjp == nullptr)
    {
        // Original: the flags are the low bits of a stack slot masked with 0xFFFC6004 and or-ed with 0x6004 (simulate,
        // cast shadows, cast drop shadows); the slot's high bits are past the defined flags.
        obj_item_flags flags;
        *(unsigned int*)&flags = 0x6004;
        obj_projectile_props props(owner_handle, create_info.w_info->projectile_info.obj_item_info, flags, create_info.w_info, create_info.pos, create_info.orient, create_info.velocity, ZERO_VECTOR, create_info.create_id);
        pjp = World.create_object<projectile>(props, 0xFFFFFFFF);
    }
    else
    {
        pjp->ca_time_offset_sec = multi_game_ca_projectile_time_offset_sec(pjp, create_info.pos);
        pjp->ca_time_offset_stamp.set(0);
    }
    multi_sim_state<multi_projectile_state>* sim_state = projectile_alloc_sim_state(); // inlined
    // Original: the projectile is used unchecked.
    pjp->remote_obj_flags.remote_object = 1;
    pjp->mp_sim_state = sim_state;
    game_handle = pjp->get_handle();
}

// 0x0095C1A0 (:718): nothing is sent
void projectile_multi_registration::send_destroy_packet(unsigned int game_handle, unsigned char* packet_data, unsigned short& packet_size) const
{
}

// 0x0097EED0 (:728)
void projectile_multi_registration::process_destroy_packet(unsigned int game_handle, int& packet_offset, unsigned char* packet_data) const
{
    projectile* pjp = projectile_from_handle(game_handle);
    if (pjp != nullptr)
    {
        pjp->remote_obj_flags.client_destroy_permission = 1;
        World.destroy_object(game_handle, false);
    }
}

// 0x009F5AF0 (:737)
// Original: any other object is read as a null projectile (its sim state at 0x150).
void projectile_multi_registration::process_object_free(object* obj) const
{
    projectile* pjp = nullptr;
    if (obj != nullptr && obj->get_obj_type() == OT_ITEM && obj->get_sub_type() == OT_SUB_ITEM_PROJECTILE)
    {
        pjp = (projectile*)obj;
    }
    if (pjp != nullptr && pjp->mp_sim_state != nullptr)
    {
        Object_projectile_sim_state_allocator.free(pjp->mp_sim_state);
        pjp->mp_sim_state = nullptr;
    }
    multi_game_process_remote_object_free_common(obj);
}

// 0x009CAC10 (:750): only once the owner exists here
bool projectile_multi_registration::allow_create_packet(int& packet_offset, unsigned char* packet_data) const
{
    remote_projectile_create_info create_info;
    multi_game_object_extract_projectile_create_info(create_info, packet_data, packet_offset);
    if (create_info.owner_net_handle == 0xFFFF)
    {
        return true;
    }
    return multi_core_game_handle_from_network_handle(create_info.owner_net_handle, true) != 0xFFFFFFFF;
}

// 0x0095C1B0 (:768)
void projectile_multi_registration::post_update_cleanup() const
{
}

// 0x0095C1C0 (:772)
// The client-created projectile's time offset to the server's create position (ms, negative), fading out over 300 ms.
static int projectile_get_ca_time_offset(projectile* pjp)
{
    if (pjp->ca_time_offset_sec != 0.0f)
    {
        const int elapsed = pjp->ca_time_offset_stamp.time_since();
        if (elapsed < 301) // Magic number: 300 ms
        {
            float fraction = 1.0f - (float)elapsed / 300.0f;
            if (fraction - 0.0f < 0.0f)
            {
                fraction = 0.0f;
            }
            if (0.0f <= fraction - 1.0f)
            {
                fraction = 1.0f;
            }
            return (int)(fraction * pjp->ca_time_offset_sec * -1000.0f);
        }
        pjp->ca_time_offset_sec = 0.0f;
    }
    return 0;
}

// 0x0097EF10 (:795): 2 constrained to an existing host, 3 parented to a player, else 0 ballistic or 1 free
static int projectile_get_attach_type(const projectile* proj)
{
    constraint* cp = projectile_constraint_from_handle(proj->constraint_handle);
    if (cp != nullptr && World.object_from_handle(cp->host_handle) != nullptr)
    {
        return 2;
    }
    if (proj->get_parent() != 0xFFFFFFFF && object_is_player(World.object_from_handle(proj->get_parent())))
    {
        return 3;
    }
    return proj->pflags.ballistic ? 0 : 1;
}

// 0x009CAC60 (:829)
// The state at the network time (less a client-created projectile's offset). A changed attachment first undoes the
// local one. A constrained state creates or moves the constraint onto the state's host (not a player); a parented one
// re-parents the projectile to the state's human and bone (a first attach runs its collision); a ballistic state puts
// the projectile on its arc; a free one is pulled towards the state: within 2 m by a blend and a small corrective
// velocity, farther snapped to the state's position.
void projectile_multi_registration::move(unsigned int game_handle) const
{
    projectile* pjp = projectile_from_handle(game_handle);
    const unsigned int now = Clock_current_time;
    if (pjp == nullptr || pjp->mp_sim_state == nullptr)
    {
        return;
    }
    multi_projectile_state state;
    bool exceeded_extrap_time;
    if (!pjp->mp_sim_state->obtain_state(now + projectile_get_ca_time_offset(pjp), &state, exceeded_extrap_time))
    {
        return;
    }
    if (state.projectile_flags.attach_type != 2 && state.projectile_flags.attach_type != 3 && state.network_time < pjp->client_prediction_net_time)
    {
        return;
    }
    if (state.projectile_flags.fading_out && !pjp->pflags.fading)
    {
        pjp->fade_stamp.set(1000); // Magic number: 1 s fade
        pjp->pflags.fading = 1;
    }
    if (state.projectile_flags.no_sync)
    {
        return;
    }
    const unsigned int local_attach_type = (unsigned int)projectile_get_attach_type(pjp);
    if (local_attach_type != state.projectile_flags.attach_type)
    {
        if (local_attach_type == 2)
        {
            World.destroy_object(pjp->constraint_handle, false);
            pjp->constraint_handle = 0xFFFFFFFF;
            return;
        }
        if (local_attach_type == 3)
        {
            human* parent = World.object_from_handle<human>(pjp->get_parent());
            if (parent != nullptr)
            {
                parent->remove_child(pjp);
            }
            pjp->pflags.release_havok_object = 0;
            return;
        }
    }
    const unsigned char attach_type = state.projectile_flags.attach_type;
    if (attach_type == 2)
    {
        const unsigned int host_handle = multi_core_game_handle_from_network_handle(state.parent_net_handle, true);
        object* host = World.object_from_handle(host_handle);
        if (object_is_player(host) || host == nullptr)
        {
            return;
        }
        constraint_template ct; // (the constructor's reset, inlined)
        // No travel, no friction (as projectile::attach).
        ct.prismatic_data.limit_min = 0.0f;
        ct.prismatic_data.limit_max = 0.0f;
        ct.prismatic_data.limit_friction = -1.0f;
        ct.type = HAVOK_CONSTRAINT_PRISMATIC;
        ct.body_b_transform.m_translation = state.constraint_pos;
        vector attach_offset = state.constraint_pos;
        attach_offset.unrotate(host->orient);
        const vector attach_pos = host->pos + attach_offset;
        matrix relative_orient;
        projectile_short_quat_to_quat(state.constraint_orient).extract_matrix(relative_orient);
        matrix attach_orient = host->orient * relative_orient;
        attach_orient.orthogonalize();
        if (pjp->constraint_handle == 0xFFFFFFFF)
        {
            if (pjp->havok_handle == 0xFFFFFFFF)
            {
                return;
            }
            debug_spew_projectile("constraint create", multi_core_network_handle_from_game_handle(pjp->get_handle(), false));
            pjp->collide(nullptr, nullptr);
            pjp->update_pos_and_orient(attach_pos, attach_orient, true);
            obj_constraint_props props(&ct, host_handle, pjp->get_handle(), attach_pos, attach_orient, state.parent_prop_point, -1, -1, -1);
            // Original: the constraint is used unchecked.
            constraint* cp = World.create_object<constraint>(props, 0xFFFFFFFF);
            pjp->constraint_handle = cp->get_handle();
            pjp->mp_handle_collision_attach_effects();
            return;
        }
        constraint* cp = World.object_from_handle<constraint>(pjp->constraint_handle);
        if (cp == nullptr)
        {
            return;
        }
        if (state.parent_prop_point == cp->host_attach_index && host_handle == cp->host_handle && delta_mat43_pbh_is_equal_eps(ct.body_b_transform, cp->constraint_info.body_b_transform, 0.1f, 0.17453f)) // Magic numbers: 0.1 m, 10 degrees
        {
            return;
        }
        if (host->havok_handle == 0xFFFFFFFF)
        {
            return;
        }
        hkpRigidBody* host_rb = havok_body_get_pointer(host->havok_handle);
        debug_spew_projectile("constraint transfer", multi_core_network_handle_from_game_handle(pjp->get_handle(), false));
        pjp->update_pos_and_orient(attach_pos, attach_orient, true);
        object_constraint_transfer(true, cp, pjp, World.object_from_handle(cp->host_handle), host, host_rb);
        cp->host_attach_index = state.parent_prop_point;
        return;
    }
    if (attach_type == 3)
    {
        const unsigned int parent_handle = multi_core_game_handle_from_network_handle(state.parent_net_handle, true);
        human* hp = World.object_from_handle<human>(parent_handle);
        if (hp == nullptr)
        {
            return;
        }
        matrix relative_orient;
        projectile_short_quat_to_quat(state.constraint_orient).extract_matrix(relative_orient);
        relative_orient.orthogonalize();
        if (pjp->attach_info != nullptr)
        {
            bool changed = pjp->attach_info->child_prop_point != state.child_prop_point || pjp->get_parent() != parent_handle;
            if (pjp->attach_info->parent_prop_point != state.parent_prop_point)
            {
                changed = true;
            }
            if (!delta_mat_pbh_is_equal_eps(relative_orient, pjp->attach_info->relative_transform.m_rotation, 0.1f)) // Magic number: 0.1 rad
            {
                changed = true;
            }
            const vector& relative_pos = pjp->attach_info->relative_transform.m_translation;
            if ((float)fabs((double)(relative_pos.x - state.constraint_pos.x)) <= 0.01f && (float)fabs((double)(relative_pos.y - state.constraint_pos.y)) <= 0.01f && (float)fabs((double)(relative_pos.z - state.constraint_pos.z)) <= 0.01f && !changed) // Magic number: 1 cm
            {
                return;
            }
            matrix43 tag;
            hp->get_tag_world_coords(state.parent_prop_point, tag.m_translation, &tag.m_rotation, nullptr, nullptr);
            matrix43 world;
            world.m_translation = state.constraint_pos;
            world.m_translation.unrotate(tag.m_rotation);
            world.m_translation = tag.m_translation + world.m_translation;
            world.m_rotation = tag.m_rotation * relative_orient;
            world.m_rotation.orthogonalize();
            object* old_parent = hp;
            if (pjp->get_parent() != 0xFFFFFFFF)
            {
                old_parent = World.object_from_handle(pjp->get_parent());
            }
            // Original: the old parent is used unchecked.
            old_parent->remove_child(pjp);
            pjp->update_pos_and_orient(world.m_translation, world.m_rotation, true);
            hp->set_child(pjp, state.parent_prop_point, state.child_prop_point, true, true);
            return;
        }
        // A first attach: placed on the bone and collided with the human (point at the projectile, normal against its
        // flight, the bone as shape key b), then parented.
        matrix43 tag;
        hp->get_tag_world_coords(state.parent_prop_point, tag.m_translation, &tag.m_rotation, nullptr, nullptr);
        // Original: the tag position (not the state's offset) is unrotated by the tag orientation, then the offset is
        // added.
        tag.m_translation.unrotate(tag.m_rotation);
        matrix43 world;
        world.m_translation = tag.m_translation + state.constraint_pos;
        world.m_rotation = tag.m_rotation * relative_orient;
        world.m_rotation.orthogonalize();
        pjp->update_pos_and_orient(world.m_translation, world.m_rotation, true);
        collision_event c_event;
        c_event.m_num_collisions = 0;
        c_event.m_object_a = pjp;
        c_event.m_object_b = hp;
        collision_event::collision_info info;
        info.m_collision_point = pjp->pos;
        info.m_collision_normal.set(-pjp->orient.fvec.x, -pjp->orient.fvec.y, -pjp->orient.fvec.z);
        info.m_shapekey_b = (unsigned int)(int)state.parent_prop_point;
        c_event.add_collision_info(info);
        pjp->collide(&c_event, hp);
        hp->set_child(pjp, state.parent_prop_point, state.child_prop_point, true, true);
        return;
    }
    // (no_sync was checked above; the original checks it again here)
    if (attach_type == 0 && !pjp->pflags.ballistic)
    {
        // Onto the arc.
        vector ballistic_pos;
        vector ballistic_vel;
        pjp->get_ballistic_position_velocity(ballistic_pos, ballistic_vel);
        if (pjp->havok_handle == 0xFFFFFFFF)
        {
            pjp->pos = ballistic_pos;
        }
        else
        {
            havok_body_set_pos_orient(pjp->havok_handle, ballistic_pos, pjp->orient);
            havok_body_set_linear_velocity(pjp->havok_handle, ballistic_vel);
        }
        pjp->pflags.ballistic = 1;
    }
    if (attach_type != 0 && !pjp->pflags.ballistic)
    {
        vector pos;
        matrix orient;
        vector linear_velocity;
        if (pjp->havok_handle == 0xFFFFFFFF)
        {
            pos = pjp->pos;
            orient = pjp->orient;
            linear_velocity = ZERO_VECTOR;
        }
        else
        {
            havok_body_get_pos_orient(pjp->havok_handle, pos, orient);
            // Original: the linear velocity is read twice (into two locals; the second is unused).
            havok_body_get_linear_velocity(pjp->havok_handle, linear_velocity);
        }
        const vector error = pos - state.position;
        const float pos_error = error.mag();
        if (pos_error > 1.0f && pjp->havok_handle != 0xFFFFFFFF) // Magic number: 1 m
        {
            havok_body_force_activate(pjp->havok_handle);
        }
        float blend = Frametime * Mp_factor;
        if (blend - 0.0f < 0.0f)
        {
            blend = 0.0f;
        }
        if (0.0f <= blend - 1.0f)
        {
            blend = 1.0f;
        }
        const matrix43 from(pos, orient);
        const matrix43 to(state.position, state.orientation);
        if (pos_error <= Mp_max_pos_diff && Min_corrective_pos <= pos_error)
        {
            const matrix43 blended = m43_slerp_matrix(from, to, blend);
            // The position stays; the orientation blends and a corrective velocity pulls towards the blended position.
            pjp->pos = pos;
            pjp->orient = blended.m_rotation;
            if (pjp->havok_handle == 0xFFFFFFFF)
            {
                return;
            }
            havok_body_set_pos_orient(pjp->havok_handle, pos, blended.m_rotation);
            vector corrective((blended.m_translation.x - pos.x) / Frametime, (blended.m_translation.y - pos.y) / Frametime, (blended.m_translation.z - pos.z) / Frametime);
            if (corrective.mag() > Max_corrective_vel)
            {
                corrective.normalize();
                corrective.x = corrective.x * Max_corrective_vel;
                corrective.y = corrective.y * Max_corrective_vel;
                corrective.z = corrective.z * Max_corrective_vel;
            }
            vector velocity;
            velocity.x = (state.linear_velocity.x - linear_velocity.x) * blend + linear_velocity.x + corrective.x;
            velocity.y = (state.linear_velocity.y - linear_velocity.y) * blend + linear_velocity.y + corrective.y;
            velocity.z = (state.linear_velocity.z - linear_velocity.z) * blend + linear_velocity.z + corrective.z;
            havok_body_set_linear_velocity(pjp->havok_handle, velocity);
            return;
        }
        // Snapped to the state's position and velocity, the orientation blended.
        matrix43 blended = m43_slerp_matrix(from, to, blend);
        blended.m_rotation.orthogonalize();
        pjp->pos = state.position;
        pjp->orient = blended.m_rotation;
        if (pjp->havok_handle == 0xFFFFFFFF)
        {
            return;
        }
        havok_body_set_pos_orient(pjp->havok_handle, state.position, blended.m_rotation);
        havok_body_set_linear_velocity(pjp->havok_handle, state.linear_velocity);
        return;
    }
    // Ballistic here: kept awake, and put back on the arc when it strayed.
    if (pjp->havok_handle != 0xFFFFFFFF)
    {
        havok_body_force_activate(pjp->havok_handle);
    }
    if (!should_reset_ballistic_pos_velocity(pjp))
    {
        return;
    }
    vector ballistic_pos;
    vector ballistic_vel;
    pjp->get_ballistic_position_velocity(ballistic_pos, ballistic_vel);
    if (pjp->havok_handle == 0xFFFFFFFF)
    {
        return;
    }
    havok_body_set_pos_orient(pjp->havok_handle, ballistic_pos, pjp->orient);
    havok_body_set_linear_velocity(pjp->havok_handle, ballistic_vel);
}

// 0x009F5B40 (:1183)
// The projectile's pose and velocity (the body's when it has one), its parent and constraint and attachment: a
// parented one sends its relative transform and prop points, a constrained one its host, the host's attach index, the
// constraint's offset and the host's orientation. A grinder blade off its arc for more than 0.5 s stops syncing.
// Original: the state is allocated before the object is checked, and a missing projectile is dereferenced.
void projectile_multi_registration::get_state(unsigned int game_handle, dc_state_list* dcsl) const
{
    object_state_data<projectile_data_bundle>* data = projectile_state_data_alloc_state(); // inlined
    if (data == nullptr)
    {
        dcsl->m_data = nullptr;
        dcsl->m_size = 0;
        return;
    }
    data->initialize();
    dcsl->m_data = (unsigned char*)data;
    dcsl->m_size = sizeof(object_state_data<projectile_data_bundle>);
    projectile* proj = projectile_from_handle(game_handle);
    projectile_data_bundle& base = data->base_data;
    if (proj->havok_handle == 0xFFFFFFFF)
    {
        base.position = proj->pos;
        base.orientation = proj->orient;
        base.linear_velocity.zero();
    }
    else
    {
        havok_body_get_pos_orient(proj->havok_handle, base.position, base.orientation);
        havok_body_get_linear_velocity(proj->havok_handle, base.linear_velocity);
    }
    base.parent_net_handle = multi_core_network_handle_from_game_handle(proj->get_parent(), false);
    base.constraint_handle = proj->constraint_handle;
    base.projectile_flags.attach_type = (unsigned char)projectile_get_attach_type(proj);
    if (base.projectile_flags.attach_type == 3)
    {
        matrix relative_orient;
        proj->attach_info->relative_transform.get_orientation(relative_orient);
        projectile_quat_to_short_quat(base.constraint_orient, quaternion(relative_orient));
        proj->attach_info->relative_transform.get_translation(base.constraint_pos);
        base.child_prop_point = proj->attach_info->child_prop_point < 0 ? -1 : (short)proj->attach_info->child_prop_point;
        base.parent_prop_point = proj->attach_info->parent_prop_point < 0 ? -1 : (short)proj->attach_info->parent_prop_point;
    }
    if (base.projectile_flags.attach_type == 2)
    {
        base.parent_net_handle = 0xFFFF;
        base.parent_prop_point = -1;
        const unsigned int parent_handle = proj->get_parent();
        object* parent = World.object_from_handle(parent_handle);
        constraint* cp = projectile_constraint_from_handle(proj->constraint_handle);
        if (parent != nullptr && cp != nullptr)
        {
            World.object_from_handle(cp->host_handle); // (unused)
            base.parent_net_handle = multi_core_network_handle_from_game_handle(parent_handle, false);
            base.parent_prop_point = get_attach_index(parent, cp->host_attach_index); // inlined
            base.constraint_pos = cp->constraint_info.body_b_transform.get_translation();
            projectile_quat_to_short_quat(base.constraint_orient, quaternion(parent->orient));
        }
    }
    base.projectile_flags.fading_out = proj->pflags.fading;
    base.projectile_flags.no_sync = 0;
    if (!proj->pflags.ballistic && proj->mp_no_sync_timer.time_since() > 500 && proj->w_info->weapon_class == WEAPON_CLASS_GRINDER) // Magic number: 0.5 s
    {
        base.projectile_flags.no_sync = 1;
    }
}

// 0x009B0D30 (:1277)
void projectile_multi_registration::free_state(dc_state_list* dcsl) const
{
    Object_projectile_allocator.free((object_state_data<projectile_data_bundle>*)dcsl->m_data);
    dcsl->m_data = nullptr;
    dcsl->m_size = 0;
}

// 0x0097EF80 (:1285)
bool projectile_multi_registration::check_object_is_dirty(dc_state_list* ref_state, dc_state_list* cur_state, const remote_object* rop) const
{
    if (ref_state == nullptr || cur_state == nullptr)
    {
        return true;
    }
    if (projectile_from_handle(rop->m_game_handle) == nullptr)
    {
        return false;
    }
    return query_possible_mask_bits_base(projectile_base(cur_state), projectile_base(ref_state)) != 0;
}

// 0x0095C280 (:1305)
// The mask (8 bits), the position (raw or a delta from the connection's record), the orientation (pitch and heading,
// raw or a delta), the linear velocity, the attachment (2 bits; a constraint's offset, relative angles, host and attach
// index, or a parent's handle, prop points, offset and angles) and the flags (fade, no sync). The connection's record
// takes what was sent.
void projectile_multi_registration::get_compressed_state(dc_state_list* current_state, dc_state_list* reference_state, dc_state_list* compressed_state, multi_connection* cp, multi_core_state_priority priority, unsigned short mask_bits) const
{
    projectile_data_bundle& cur = projectile_base(current_state);
    const projectile_data_bundle* client_source = reference_state != nullptr ? &projectile_transmitted(reference_state, cp) : &cur;
    projectile_data_bundle& client = projectile_transmitted(current_state, cp);
    memcpy(&client, client_source, sizeof(projectile_data_bundle));
    if (mask_bits == 0)
    {
        compressed_state->m_size = 0;
        return;
    }
    bit_packer packer(compressed_state->m_data, 1200);
    const unsigned int mask = mask_bits;
    packer.set_next_bits(mask, 8);
    vector client_vec;
    const unsigned int pos_mask = mask & 3;
    if (pos_mask != 0)
    {
        if (pos_mask == 3)
        {
            packer.add_vector(cur.position);
            client.position = cur.position;
        }
        else
        {
            buffer_delta_compressed_vector(packer, pos_mask, client_vec, client.position, cur.position);
            client.position = client_vec;
        }
        client.transmitted_data_set |= 3;
    }
    matrix client_matrix;
    if ((mask & 4) != 0)
    {
        buffer_compressed_ph(packer, 4, client_matrix, cur.orientation);
        client.transmitted_data_set |= 4;
        client.orientation = client_matrix;
    }
    else if ((mask & 8) != 0)
    {
        buffer_delta_compressed_ph(packer, 8, client_matrix, client.orientation, cur.orientation);
        client.transmitted_data_set |= 0xC;
        client.orientation = client_matrix;
    }
    const unsigned int vel_mask = mask & 0x30;
    if (vel_mask != 0)
    {
        if (vel_mask == 0x30)
        {
            packer.add_vector(cur.linear_velocity);
            client_vec = cur.linear_velocity;
        }
        else
        {
            buffer_delta_compressed_vector(packer, vel_mask, client_vec, client.linear_velocity, cur.linear_velocity);
        }
        client.transmitted_data_set |= 0x30;
        client.linear_velocity = client_vec;
    }
    if ((mask & 0x40) != 0)
    {
        packer.set_next_bits(cur.projectile_flags.attach_type, 2);
        client.transmitted_data_set |= 0x40;
        client.projectile_flags.attach_type = cur.projectile_flags.attach_type;
        if (cur.projectile_flags.attach_type == 2)
        {
            // The projectile's angles relative to the host orientation (constraint_orient holds the host's).
            packer.add_vector(cur.constraint_pos);
            matrix host_inverse;
            projectile_short_quat_to_quat(cur.constraint_orient).extract_matrix(host_inverse);
            host_inverse.inverse();
            const matrix relative = host_inverse * matrix(cur.orientation);
            vector angles;
            relative.extract_angles(angles.x, angles.z, angles.y);
            packer.add_vector(angles);
            packer.add_uint16(cur.parent_net_handle);
            packer.add_uint16((unsigned short)cur.parent_prop_point);
            client.constraint_handle = cur.constraint_handle;
            client.parent_net_handle = cur.parent_net_handle;
            client.parent_prop_point = cur.parent_prop_point;
        }
        else if (cur.projectile_flags.attach_type == 3)
        {
            packer.add_uint16(cur.parent_net_handle);
            packer.add_byte((unsigned char)(cur.child_prop_point > 0xFE ? 0xFF : cur.child_prop_point));
            packer.add_byte((unsigned char)(cur.parent_prop_point > 0xFE ? 0xFF : cur.parent_prop_point));
            matrix relative;
            projectile_short_quat_to_quat(cur.constraint_orient).extract_matrix(relative);
            vector angles;
            relative.extract_angles(angles.x, angles.z, angles.y);
            packer.add_vector(cur.constraint_pos);
            packer.add_vector(angles);
            client.parent_net_handle = cur.parent_net_handle;
        }
    }
    if ((mask & 0x80) != 0)
    {
        packer.add_bool(cur.projectile_flags.fading_out);
        packer.add_bool(cur.projectile_flags.no_sync);
        client.projectile_flags.fading_out = cur.projectile_flags.fading_out;
        client.transmitted_data_set |= 0x80;
        client.projectile_flags.no_sync = cur.projectile_flags.no_sync;
    }
    compressed_state->m_size = (unsigned long)(long long)(float)ceil((double)((float)(double)packer.get_start_bit() * 0.125f));
}

// 0x009CBE90 (:1586): debug output, compiled out (the lookups stay)
// LTCG: proj in EAX (the port has no register convention).
static void spew_constraint_state(const projectile* proj, const projectile_data_bundle& result_data)
{
    multi_core_network_handle_from_game_handle(proj->get_handle(), false);
    debug_spew_projectile("constraint", result_data.constraint_pos.x);
    matrix m;
    projectile_short_quat_to_quat(result_data.constraint_orient).extract_matrix(m);
    float p;
    float b;
    float h;
    m.extract_angles(p, b, h);
    debug_spew_projectile("angles", p);
    debug_spew_projectile("parent", multi_core_game_handle_from_network_handle(result_data.parent_net_handle, true));
    debug_spew_projectile("prop point", (int)result_data.parent_prop_point);
}

// 0x009CBFE0 (:1601): debug output, compiled out (the lookups stay)
static void spew_child_state(const projectile_data_bundle& result_data)
{
    debug_spew_projectile("child", result_data.constraint_pos.x);
    matrix m;
    projectile_short_quat_to_quat(result_data.constraint_orient).extract_matrix(m);
    float p;
    float b;
    float h;
    m.extract_angles(p, b, h);
    debug_spew_projectile("angles", p);
    debug_spew_projectile("parent", multi_core_game_handle_from_network_handle(result_data.parent_net_handle, true));
    debug_spew_projectile("prop points", (int)result_data.parent_prop_point, (int)result_data.child_prop_point);
}

// 0x0095C8C0 (:1616): debug output, compiled out
// LTCG: attach_state in EAX (the port has no register convention).
static void spew_no_attach_state(unsigned int attach_state)
{
    debug_spew_projectile(get_attach_type_text((int)attach_state)); // inlined
}

// 0x009F9FB0 (:1625)
// The received state on the reference (or the current state), as get_compressed_state wrote it: the angles come back
// as short quaternions (the host's angles composed back are not: constraint_orient then holds the relative
// orientation); a ballistic state takes the arc's position and velocity here.
// Original: without a reference a delta-compressed position, orientation or velocity reads a null reference; the port
// reads zero / the identity.
bool projectile_multi_registration::apply_compressed_state(unsigned int game_handle, dc_state_list* reference_state, dc_state_list* compressed_state, dc_state_list** resultant_state, unsigned int network_time) const
{
    projectile* proj = projectile_from_handle(game_handle);
    if (proj == nullptr)
    {
        return false;
    }
    dc_state_list* dcsl = multi_core_dc_alloc_state_list(); // inlined
    if (dcsl == nullptr)
    {
        return false;
    }
    dcsl->m_reference_ackid = 0;
    dcsl->m_data = nullptr;
    dcsl->m_size = 0;
    *resultant_state = dcsl;
    object_state_data<projectile_data_bundle>* data;
    const projectile_data_bundle* reference = nullptr;
    if (reference_state == nullptr)
    {
        get_state(game_handle, dcsl);
        data = (object_state_data<projectile_data_bundle>*)dcsl->m_data;
    }
    else
    {
        reference = &projectile_base(reference_state);
        data = projectile_state_data_alloc_state(); // inlined
        if (data == nullptr)
        {
            multi_core_dc_free_state_list(dcsl);
            *resultant_state = nullptr;
            return false;
        }
        data->initialize();
        memcpy(&data->base_data, reference, sizeof(projectile_data_bundle));
        dcsl->m_data = (unsigned char*)data;
        dcsl->m_size = sizeof(object_state_data<projectile_data_bundle>);
    }
    projectile_data_bundle& state = data->base_data;
    state.network_time = network_time;
    bit_packer packer(compressed_state->m_data, compressed_state->m_size);
    const unsigned int mask = packer.get_next_bits(8);
    const unsigned int pos_mask = mask & 3;
    if (pos_mask == 3)
    {
        packer.get_vector(state.position);
    }
    else if (pos_mask != 0)
    {
        apply_delta_compressed_vector(packer, pos_mask, state.position, reference != nullptr ? reference->position : ZERO_VECTOR);
    }
    if ((mask & 0xC) != 0)
    {
        if ((mask & 4) != 0)
        {
            apply_delta_compressed_ph(packer, 4, state.orientation, IDENTITY_MATRIX);
        }
        else
        {
            apply_delta_compressed_ph(packer, 8, state.orientation, reference != nullptr ? reference->orientation : IDENTITY_MATRIX);
        }
    }
    const unsigned int vel_mask = mask & 0x30;
    if (vel_mask == 0x30)
    {
        packer.get_vector(state.linear_velocity);
    }
    else if (vel_mask != 0)
    {
        apply_delta_compressed_vector(packer, vel_mask, state.linear_velocity, reference != nullptr ? reference->linear_velocity : ZERO_VECTOR);
    }
    if ((mask & 0x40) != 0)
    {
        const unsigned int attach_type = packer.get_next_bits(2);
        debug_spew_projectile("before");
        if (state.projectile_flags.attach_type == 2)
        {
            spew_constraint_state(proj, state);
        }
        else if (state.projectile_flags.attach_type == 3)
        {
            spew_child_state(state);
        }
        else
        {
            spew_no_attach_state(state.projectile_flags.attach_type);
        }
        state.projectile_flags.attach_type = (unsigned char)attach_type;
        if (attach_type == 2)
        {
            packer.get_vector(state.constraint_pos);
            vector angles;
            packer.get_vector(angles);
            matrix m;
            m.set(angles.x, angles.z, angles.y);
            projectile_quat_to_short_quat(state.constraint_orient, quaternion(m));
            state.parent_net_handle = packer.get_uint16();
            const unsigned short prop_point = packer.get_uint16();
            state.parent_prop_point = (short)prop_point;
            if (prop_point == 0xFFFF)
            {
                state.parent_prop_point = -1;
            }
        }
        else if (attach_type == 3)
        {
            state.parent_net_handle = packer.get_uint16();
            state.child_prop_point = (short)packer.get_uint8();
            const unsigned char parent_prop_point = packer.get_uint8();
            state.parent_prop_point = (short)parent_prop_point;
            if (state.child_prop_point == 0xFF)
            {
                state.child_prop_point = -1;
            }
            if (parent_prop_point == 0xFF)
            {
                state.parent_prop_point = -1;
            }
            packer.get_vector(state.constraint_pos);
            vector angles;
            packer.get_vector(angles);
            matrix m;
            m.set(angles.x, angles.z, angles.y);
            projectile_quat_to_short_quat(state.constraint_orient, quaternion(m));
        }
        debug_spew_projectile("after");
        if (state.projectile_flags.attach_type == 2)
        {
            spew_constraint_state(proj, state);
        }
        else if (state.projectile_flags.attach_type == 3)
        {
            spew_child_state(state);
        }
        else
        {
            spew_no_attach_state(state.projectile_flags.attach_type);
        }
    }
    if ((mask & 0x80) != 0)
    {
        state.projectile_flags.fading_out = packer.get_bool() ? 1 : 0;
        state.projectile_flags.no_sync = packer.get_bool() ? 1 : 0;
    }
    if (state.projectile_flags.attach_type == 0)
    {
        proj->get_ballistic_position_velocity(state.position, state.linear_velocity);
    }
    if (proj->mp_sim_state == nullptr)
    {
        return false;
    }
    proj->mp_sim_state->add_state(state);
    proj->mp_sim_state->set_correction_timer(0);
    return true;
}

// 0x0095C920 (:1870): bits per data set (the attachment by the attach type in additional_data), plus the 8 mask bits
// less one, in bytes
void projectile_multi_registration::query_space_for_mask(query_size_results* qs_results, void* additional_data) const
{
    for (int i = 0; i < 5; i++)
    {
        const unsigned int mask = qs_results[i].mask;
        int bits = 0;
        switch (mask & 3)
        {
        case 3:
            bits = 96;
            break;
        case 2:
            bits = 24;
            break;
        case 1:
            bits = 15;
            break;
        }
        if ((mask & 4) != 0)
        {
            bits += 32;
        }
        else if ((mask & 8) != 0)
        {
            bits += 16;
        }
        if ((mask & 0x30) == 0x30)
        {
            bits += 96;
        }
        else if ((mask & 0x20) != 0)
        {
            bits += 24;
        }
        else if ((mask & 0x10) != 0)
        {
            bits += 15;
        }
        if ((mask & 0x40) != 0 && additional_data != nullptr)
        {
            const int attach_type = *(const int*)additional_data;
            // Magic number: 2 attach type bits, plus 224 for a constraint or a parent (generous for their data).
            bits += (attach_type == 2 || attach_type == 3) ? 0xE2 : 2;
        }
        if ((mask & 0x80) != 0)
        {
            bits += 2;
        }
        qs_results[i].num_bytes = bits == 0 ? 0 : (unsigned short)((bits + 15) >> 3);
    }
}

// 0x009CC120 (:1948): as the movers' (State_priority_base_projectile, up to extreme)
void projectile_multi_registration::query_space_for_priority(const remote_object* rop, dc_state_list* current_state, dc_state_list* reference_state, unsigned short possible_mask_bits, multi_connection* cp, query_size_results* qs_results) const
{
    if (possible_mask_bits == 0)
    {
        return;
    }
    player* viewer = multi_game_data_get_player_instance(cp->game_data);
    const projectile_data_bundle& cur = projectile_base(current_state);
    state_priority_bundle* bundle = const_cast<state_priority_bundle*>(&rop->m_state_priority_bundle[cp->index_in_session]);
    const projectile_data_bundle* ref = reference_state != nullptr ? &projectile_base(reference_state) : nullptr;
    float dist2 = 0.0f;
    if (viewer != nullptr)
    {
        const vector camera_pos = object_is_local_player(viewer) ? Rfg_camera.real_pos : viewer->mp_camera_pos;
        dist2 = (camera_pos.x - cur.position.x) * (camera_pos.x - cur.position.x) + (camera_pos.y - cur.position.y) * (camera_pos.y - cur.position.y) + (camera_pos.z - cur.position.z) * (camera_pos.z - cur.position.z);
        if (ref != nullptr)
        {
            const float ref_dist2 = (camera_pos.x - ref->position.x) * (camera_pos.x - ref->position.x) + (camera_pos.y - ref->position.y) * (camera_pos.y - ref->position.y) + (camera_pos.z - ref->position.z) * (camera_pos.z - ref->position.z);
            if (0.0f <= dist2 - ref_dist2)
            {
                dist2 = ref_dist2;
            }
        }
    }
    int distance_bonus;
    if (dist2 > 400.0f) // Magic numbers: squared distances 20, 10, 5 and 3 m
    {
        distance_bonus = -2;
    }
    else if (dist2 > 100.0f)
    {
        distance_bonus = -1;
    }
    else if (dist2 > 25.0f)
    {
        distance_bonus = 0;
    }
    else if (dist2 > 9.0f)
    {
        distance_bonus = 1;
    }
    else
    {
        distance_bonus = 2;
    }
    for (int i = 0; i < 5; i++)
    {
        int priority = (int)(bundle->state_data[i].time_since_update >> 1) + State_priority_base_projectile[i] + distance_bonus;
        if (priority < 0)
        {
            priority = MULTI_CORE_STATE_PRIORITY_ABYSMAL;
        }
        else if (priority > 4)
        {
            priority = MULTI_CORE_STATE_PRIORITY_EXTREME;
        }
        bundle->state_data[i].priority = (multi_core_state_priority)priority;
    }
    for (int level = 0; level < 5; level++)
    {
        qs_results[level].mask = 0;
        for (int i = 0; i < 5; i++)
        {
            if (level <= (int)bundle->state_data[i].priority)
            {
                qs_results[level].mask |= (unsigned short)State_mask_projectile[i];
            }
        }
    }
    for (int level = 0; level < 5; level++)
    {
        qs_results[level].mask &= possible_mask_bits;
    }
    int attach_type = cur.projectile_flags.attach_type;
    query_space_for_mask(qs_results, &attach_type);
}

// 0x0095C9E0 (:2047)
void projectile_multi_registration::notify_transmitted_updated(const remote_object* rop, dc_state_list* current_state, multi_connection* recipient, unsigned short possible_mask_bits, unsigned short mask_bits_transmitted) const
{
    state_priority_bundle* bundle = const_cast<state_priority_bundle*>(&rop->m_state_priority_bundle[recipient->index_in_session]);
    for (int i = 0; i < 5; i++)
    {
        if ((State_mask_projectile[i] & possible_mask_bits) == 0 || (State_mask_projectile[i] & mask_bits_transmitted) != 0)
        {
            bundle->state_data[i].time_since_update = 0;
        }
        else
        {
            bundle->state_data[i].time_since_update++;
        }
    }
    projectile_transmitted(current_state, recipient).full_update = possible_mask_bits == 0 || possible_mask_bits == mask_bits_transmitted;
}

// 0x0095CA70 (:2071)
bool projectile_multi_registration::check_full_update(dc_state_list* ref_state, multi_connection* cp) const
{
    return projectile_transmitted(ref_state, cp).full_update;
}

// 0x009CC450 (:2082)
void projectile_multi_registration::reset_state_data() const
{
    for (object_state_data<projectile_data_bundle>* data = Object_projectile_allocator.get_first(0); data != nullptr;)
    {
        data = Object_projectile_allocator.free(data);
    }
}

// 0x009B0D50 (:2087)
void projectile_multi_registration::free_mp_state_data() const
{
    remote_object* rop = Remote_objects_list;
    while (rop != nullptr)
    {
        if (rop->current_type == registration_type)
        {
            projectile* pjp = projectile_from_handle(rop->m_game_handle);
            if (pjp != nullptr && pjp->mp_sim_state != nullptr)
            {
                Object_projectile_sim_state_allocator.free(pjp->mp_sim_state);
                pjp->mp_sim_state = nullptr;
            }
        }
        rop = rop->next;
        if (rop == Remote_objects_list)
        {
            break;
        }
    }
}

// 0x0096BC90
projectile_multi_registration::projectile_multi_registration()
{
    num_masks = 0;
}

// 0x0096BD80
projectile_multi_registration::~projectile_multi_registration()
{
}

// 0x009E0B30 (:2109)
// With clear_allocators the pools are emptied (a level change). Otherwise the pools (2400 state datas, 96 received
// state sets) and the registration ("projectile", type 1, every 100 ms).
void multi_game_object_projectile_init(bool clear_allocators)
{
    if (clear_allocators)
    {
        for (object_state_data<projectile_data_bundle>* data = Object_projectile_allocator.get_first(0); data != nullptr;)
        {
            data = Object_projectile_allocator.free(data);
        }
        for (multi_sim_state<multi_projectile_state>* sim_state = Object_projectile_sim_state_allocator.get_first(0); sim_state != nullptr;)
        {
            sim_state = Object_projectile_sim_state_allocator.free(sim_state);
        }
        return;
    }
    Object_projectile_allocator.init(MP_allocators_mempool);
    Object_projectile_sim_state_allocator.init(MP_allocators_mempool);
    // Original: the allocation is not checked before the masks are written.
    projectile_multi_registration* registration = new (MP_allocators_mempool.alloc(sizeof(projectile_multi_registration), 0)) projectile_multi_registration();
    registration->name = "projectile";
    registration->update_rate = 100;
    Projectile_mr = registration;
    registration->add_state_mask("POS", "POS", 3, 96, 0.01f, 3.4028235e+38f, 0.0f, 1.0f);
    registration->add_state_mask("POS_8BITS_DELTA", "POS_8BITS_DELTA", 2, 24, 0.01f, 5.0f, 0.0f, 3.0f);
    registration->add_state_mask("POS_5BITS_DELTA", "POS_5BITS_DELTA", 1, 15, 0.01f, 0.625f, 0.0f, 3.0f);
    registration->add_state_mask("ORIENT_PH", "ORIENT_PH", 4, 32, 0.0017453f, 3.1415927f, 0.0f, 1.0f);
    registration->add_state_mask("ORIENT_PH_DELTA", "ORIENT_PH_DELTA", 8, 16, 0.0f, 0.17453f, 0.0f, 1.0f);
    registration->add_state_mask("LINEAR_VEL", "LINEAR_VEL", 0x30, 96, 0.1f, 3.4028235e+38f, 0.0f, 1.0f);
    registration->add_state_mask("LINEAR_VEL_8BITS_DELTA", "LINEAR_VEL_8BITS_DELTA", 0x20, 24, 0.1f, 5.0f, 0.0f, 3.0f);
    registration->add_state_mask("LINEAR_VEL_5BITS_DELTA", "LINEAR_VEL_5BITS_DELTA", 0x10, 15, 0.1f, 0.625f, 0.0f, 3.0f);
    registration->add_state_mask("ATTACHMENT", "ATTACHMENT", 0x40, 0xFFFFFFFF, 0.0f, 0.0f, 0.0f, 1.0f); // (variable size)
    registration->add_state_mask("FLAGS", "FLAGS", 0x80, 2, 0.0f, 0.0f, 0.0f, 1.0f);
    multi_core_object_register_object_type(MULTI_OBJECT_TYPE_PROJECTILE, registration);
}

// 0x0095CA90 (:2220)
const multi_core_object_registration* multi_game_object_get_projectile_registration()
{
    return Projectile_mr;
}

// 0x0099C5C0 (:2228): a new host destroys every projectile without destroy packets
void multi_game_migration_cull_projectiles()
{
    const int count = World.get_num_objects<projectile>();
    for (int i = 0; i < count; i++)
    {
        projectile* pjp = World.get_object<projectile>(i);
        pjp->remote_obj_flags.client_destroy_permission = 1;
        pjp->remote_obj_flags.no_destroy_packet = 1;
        World.destroy_object(pjp, false);
    }
}

// 0x0097EFF0 (:2272): the remote projectiles forget their create ids
void multi_game_migration_reset_projectile_nonce()
{
    remote_object* first = Remote_objects_by_type[MULTI_OBJECT_TYPE_PROJECTILE];
    remote_object* rop = first;
    while (rop != nullptr)
    {
        projectile* pjp = projectile_from_handle(rop->m_game_handle);
        if (pjp != nullptr)
        {
            pjp->mp_create_id = 0xFFFFFFFF;
        }
        rop = rop->type_next;
        if (rop == first)
        {
            break;
        }
    }
}

// The oldest projectile (not a mine or a charge) older than min_age ms, or null.
static projectile* multi_game_oldest_projectile(int min_age)
{
    projectile* oldest = nullptr;
    int oldest_age = min_age;
    const int count = World.get_num_objects<projectile>();
    for (int i = 0; i < count; i++)
    {
        projectile* pjp = World.get_object<projectile>(i);
        const weapon_class_type weapon_class = pjp->w_info->weapon_class;
        if (weapon_class != WEAPON_CLASS_THROWN_MINE && weapon_class != WEAPON_CLASS_THROWN_CHARGE)
        {
            const int age = pjp->get_age();
            if (oldest_age < age)
            {
                oldest = pjp;
                oldest_age = age;
            }
        }
    }
    return oldest;
}

// 0x0099C620 (:2288)
// The server destroys one projectile a frame beyond 90 (the oldest), and one older than 5 s beyond 64; mines and
// charges are never destroyed here.
void multi_game_enforce_mp_projectile_limit()
{
    if (!Multiplayer || !Server)
    {
        return;
    }
    if (World.get_num_objects<projectile>() > 90) // Magic number: 90 projectiles
    {
        projectile* oldest = multi_game_oldest_projectile(-1);
        if (oldest != nullptr)
        {
            World.destroy_object(oldest, false);
        }
    }
    if (World.get_num_objects<projectile>() > 64) // Magic numbers: 64 projectiles, 5 s
    {
        projectile* oldest = multi_game_oldest_projectile(5000);
        if (oldest != nullptr)
        {
            World.destroy_object(oldest, false);
        }
    }
}
