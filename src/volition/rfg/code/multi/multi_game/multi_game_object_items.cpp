// Original: volition/rfg/code/multi/multi_game/multi_game_object_items.cpp
//
// Decompiled whole (multiplayer.md MP8). The replication of items (in practice the weapons on the ground: dropped,
// thrown and the map's pickups): a remote object of type 3 whose create packet carries the position, the orientation,
// the item and weapon indices and a weapon's ammo (and a preplaced item's game handle, so the client finds its own
// copy); the state is the position, the orientation, the velocities, the visibility and whether the body simulates. A
// client eases a bodiless item towards the state and steers a body with Havok's soft keyframe while it is in view.
// A received visibility change starts the respawn timer of a hidden respawning item, or respawns it.
// Part of the multi unity TU (rfg_multi_unity.obj, every multi/ .cpp in path order): the file statics are shared
// by the whole tree, and a shared one is placed in its first user (tools/hdrgen/specs/mp0_stubs.py).

#include "volition/rfg/code/multi/multi_game/multi_game_object_items.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/utilities/dynamics/keyframe/hkpkeyframeutility.h"
#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/havok/havok_convert.h"
#include "volition/rfg/code/level/rfg_prop_block_shared.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/level/world.inl"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_core/include/multi_connection.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_compression.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_delta_compression.h"
#include "volition/rfg/code/multi/multi_core/include/multi_pool.inl"
#include "volition/rfg/code/multi/multi_game/multi_game.h"
#include "volition/rfg/code/multi/multi_game/multi_game_data.h"
#include "volition/rfg/code/multi/multi_game/multi_game_player.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/object_item.h"
#include "volition/rfg/code/objects/object_prop_names_shared.h"
#include "volition/rfg/code/objects/object_type.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/vlib/code/math/quaternion.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/os/frametime.h"

#include <math.h>
#include <string.h>

// ---- file-static globals
// const float PI (0x01309C38) is used here too; placed in multi_game_client_predict.cpp.
// const float SERVER_FUEL_FUDGE_FACTOR (0x01309DE0) is used here too; placed in multi_game_object_backpack.cpp.
static multi_core_state_priority State_priority_base[5] = {MULTI_CORE_STATE_PRIORITY_MEDIUM, MULTI_CORE_STATE_PRIORITY_LOW, MULTI_CORE_STATE_PRIORITY_MEDIUM, MULTI_CORE_STATE_PRIORITY_MEDIUM, MULTI_CORE_STATE_PRIORITY_EXTREME}; // 0x0167F044
static unsigned int State_mask[5] = {3, 4, 0x18, 0x60, 0x80}; // 0x0167F058
static timestamp Rest_timestamp; // 0x01692E08, initial data ffffffff (invalid)
static multi_pool<object_state_data<item_data_bundle>, 3200> Object_item_allocator; // 0x01692E10
static multi_pool<multi_sim_state<multi_item_state>, 128> Sim_state_item_allocator; // 0x01692E50
static item_multi_registration* Item_mr; // 0x02FE6D3C
// unsigned int Clock_current_time (0x02FEB538) is used here too; placed in multi.cpp.

// ---- internal functions
static void slow_mp_sim_validate(); // 0x0099B000 (:279)
static object_state_data<item_data_bundle>* object_item_alloc_item_state_data(); // 0x009F48C0 (:305)
static multi_sim_state<multi_item_state>* object_item_alloc_sim_state(); // 0x0099B0D0 (:318)
static unsigned short query_possible_mask_bits_base(const item_data_bundle& cur_data, const item_data_bundle& ref_data); // 0x009594D0 (:336)

// The item of a game handle (null for any other object).
static item* item_from_handle(unsigned int game_handle)
{
    if (game_handle == 0xFFFFFFFF)
    {
        return nullptr;
    }
    object* objp = World.object_from_handle(game_handle);
    if (objp == nullptr || objp->get_obj_type() != OT_ITEM)
    {
        return nullptr;
    }
    return (item*)objp;
}

static item_data_bundle& item_base(dc_state_list* dcsl)
{
    return ((object_state_data<item_data_bundle>*)dcsl->m_data)->base_data;
}

static item_data_bundle& item_transmitted(dc_state_list* dcsl, const multi_connection* cp)
{
    return ((object_state_data<item_data_bundle>*)dcsl->m_data)->transmitted_data[cp->index_in_session];
}

// 0x0099B000 (:279): a debug walk of the used sim states looking for their items (no effect; never called)
static void slow_mp_sim_validate()
{
    const int count = World.get_num_objects<item>();
    for (multi_sim_state<multi_item_state>* sim_state = Sim_state_item_allocator.get_first(0); sim_state != nullptr; sim_state = Sim_state_item_allocator.get_next(sim_state))
    {
        for (int i = 0; i < count; i++)
        {
            if (World.get_object<item>(i)->mp_sim_state == sim_state)
            {
                break;
            }
        }
    }
}

// 0x009F48C0 (:305)
static object_state_data<item_data_bundle>* object_item_alloc_item_state_data()
{
    object_state_data<item_data_bundle>* data = Object_item_allocator.alloc(0);
    if (data == nullptr)
    {
        multi_core_dc_flush_old_object_states();
        data = Object_item_allocator.alloc(0);
    }
    return data;
}

// 0x0099B0D0 (:318)
static multi_sim_state<multi_item_state>* object_item_alloc_sim_state()
{
    multi_sim_state<multi_item_state>* sim_state = Sim_state_item_allocator.alloc(0);
    if (sim_state == nullptr)
    {
        keen::debug::nativePrintArgs("Out of item sim states!");
        return nullptr;
    }
    sim_state->reset();
    return sim_state;
}

// 0x009594D0 (:336)
// LTCG: cur_data in EDI, ref_data in ESI (the port has no register convention).
static unsigned short query_possible_mask_bits_base(const item_data_bundle& cur_data, const item_data_bundle& ref_data)
{
    const unsigned int pos_mask = Item_mr->get_vector_mask(cur_data.position, ref_data.position, 1, 2, 3, 0xFFFFFFFF);
    const unsigned int lvel_mask = Item_mr->get_vector_mask(cur_data.linear_velocity, ref_data.linear_velocity, 8, 0x10, 0x18, 0xFFFFFFFF);
    const unsigned int avel_mask = Item_mr->get_vector_mask(cur_data.angular_velocity, ref_data.angular_velocity, 0x20, 0x40, 0x60, 0xFFFFFFFF);
    const unsigned int orient_mask = Item_mr->get_orient_mask(cur_data.orientation, ref_data.orientation, 4, 0, 0);
    unsigned short mask = (unsigned short)(pos_mask | lvel_mask | orient_mask | avel_mask);
    if (cur_data.lighting_set_once != ref_data.lighting_set_once || cur_data.simulating != ref_data.simulating || cur_data.dont_render != ref_data.dont_render)
    {
        mask |= 0x80;
    }
    return mask;
}

// 0x009595B0 (:362): never sent data sets are added
unsigned short item_multi_registration::query_possible_mask_bits(dc_state_list* current_state, dc_state_list* reference_state, multi_connection* cp) const
{
    if (reference_state == nullptr)
    {
        return 0xFF;
    }
    const item_data_bundle& client = item_transmitted(reference_state, cp);
    return query_possible_mask_bits_base(item_base(current_state), client) | (unsigned short)(~client.transmitted_data_set & 0xFF);
}

// 0x00959610 (:387)
void item_multi_registration::create(unsigned int game_handle) const
{
    object* objp = World.object_from_handle(game_handle);
    if (objp != nullptr)
    {
        objp->remote_obj_flags.remote_object = 1;
    }
}

// 0x0097D6E0 (:405)
// Packed (at most 25 bytes): the position (16 bits each, within 1280 m), the orientation (pitch, bank and heading, 8
// bits each), a flags byte (1 simulate, 2 respawn, 4 at rest, 8 a weapon, 0x10 preplaced), a preplaced item's game
// handle, the item info and weapon info indices, and a weapon's magazine and reserve ammo and (moving) velocity. Only an
// item whose weapon info index is valid is sent (and marked remote).
// Original: the flags byte's bits 5-7 are uninitialized stack (unread by the receiver; 0 here), and the weapon info is
// read from weapon::w_info of any item (past the end of a non-weapon; none here).
void item_multi_registration::send_create_packet(const remote_object* rop, unsigned char* packet_data, unsigned short& packet_size, multi_connection* dest_conn) const
{
    unsigned char bits[28] = {};
    bit_packer packer(bits, 25);
    item* itemp = item_from_handle(rop->m_game_handle);
    vec_delta_compress(packer, ZERO_VECTOR, itemp->pos, 16, 1280.0f, 0.0f, 3.0f);
    mat_pbh_delta_compress(packer, IDENTITY_MATRIX, itemp->orient, 8, 3.1415927f, 0.0f, 1.0f);
    unsigned char flags = (unsigned char)((itemp->item_flags.respawn ? 2 : 0) | (itemp->item_flags.preplaced ? 0x10 : 0));
    vector linear_velocity;
    vector angular_velocity;
    if (itemp->havok_handle == 0xFFFFFFFF)
    {
        linear_velocity.zero();
        angular_velocity.zero();
    }
    else
    {
        havok_body_get_linear_velocity(itemp->havok_handle, linear_velocity);
        havok_body_get_angular_velocity(itemp->havok_handle, angular_velocity);
    }
    if (0.1f <= (float)sqrt((double)(linear_velocity.x * linear_velocity.x + linear_velocity.y * linear_velocity.y + linear_velocity.z * linear_velocity.z)) || 0.1f <= (float)sqrt((double)(angular_velocity.x * angular_velocity.x + angular_velocity.y * angular_velocity.y + angular_velocity.z * angular_velocity.z))) // Magic number: at rest below 0.1
    {
        flags = (unsigned char)((flags & 0xFB) | 1);
    }
    else
    {
        flags |= 5;
    }
    const bool is_weapon = itemp->get_sub_type() == OT_SUB_ITEM_WEAPON;
    if (is_weapon)
    {
        flags |= 8;
    }
    else
    {
        flags &= 0xF7;
    }
    packer.set_next_bits(flags, 8);
    if (itemp->item_flags.preplaced)
    {
        packer.add_uint(rop->m_game_handle);
    }
    const int info_index = object_item_get_info_index(itemp->info->name);
    if (info_index < 0)
    {
        return;
    }
    packer.add_uint8((unsigned char)info_index);
    const weapon_info* w_info = is_weapon ? ((weapon*)itemp)->w_info : nullptr;
    const int w_handle = w_info != nullptr ? (int)weapons_get_weapon_info_handle(w_info) : -1;
    if (w_handle < 0 || w_handle >= Num_weapon_infos)
    {
        return;
    }
    packer.add_uint8((unsigned char)w_handle);
    if ((flags & 8) != 0)
    {
        weapon* wp = (weapon*)itemp;
        packer.add_uint8((unsigned char)wp->get_rounds_in_magazine());
        packer.add_uint16((unsigned short)wp->get_rounds_in_reserve());
        if ((flags & 4) == 0)
        {
            vec_delta_compress(packer, ZERO_VECTOR, linear_velocity, 16, 1280.0f, 0.0f, 3.0f);
        }
    }
    const int num_bytes = (int)(float)ceil((double)((float)packer.get_start_bit() * 0.125f));
    for (int i = 0; i < num_bytes; i++)
    {
        packet_data[packet_size] = bits[i];
        packet_size++;
    }
    itemp->remote_obj_flags.remote_object = 1;
}

// 0x0099B110 (:499)
// A preplaced item is found by its game handle (among all items) and moved; otherwise the weapon is created here. It
// gets received states; a resting one goes to sleep; a weapon takes its ammo (clamped) and a moving one its velocity.
// Original: packet_offset is not advanced, the item is used unchecked, and a failed sim state allocation is reported
// twice.
void item_multi_registration::process_create_packet(unsigned int& game_handle, unsigned short net_handle, int& packet_offset, unsigned char* packet_data) const
{
    bit_packer packer(packet_data + packet_offset, 25);
    vector pos;
    vec_delta_decompress(packer, pos, ZERO_VECTOR, 16, 1280.0f, 0.0f, 3.0f);
    matrix packed_orient;
    mat_pbh_delta_decompress(packer, packed_orient, IDENTITY_MATRIX, 8, 3.1415927f, 0.0f, 1.0f);
    quaternion q;
    q.set(packed_orient);
    const unsigned char flags = (unsigned char)packer.get_next_bits(8);
    weapon* wp = nullptr;
    if ((flags & 0x10) != 0)
    {
        const unsigned int preplaced_handle = packer.get_next_bits(32);
        const int count = World.get_num_objects<item>();
        for (int i = 0; i < count; i++)
        {
            item* candidate = World.get_object<item>(i);
            if (candidate->get_handle() == preplaced_handle)
            {
                wp = (weapon*)candidate;
                break;
            }
        }
    }
    const unsigned char info_index = (unsigned char)packer.get_next_bits(8);
    const unsigned char w_index = (unsigned char)packer.get_next_bits(8);
    matrix orient;
    q.extract_matrix(orient);
    if (wp == nullptr)
    {
        // Respawn, simulate and preplaced from the packet; cast shadows and drop shadows (0x6000).
        // Original: the flags' bits past 0x6000 come from an uninitialized stack slot (past the defined flags).
        obj_item_flags item_flags;
        *(unsigned int*)&item_flags = (unsigned int)(((flags & 0x11) | 0x1800) * 4) | ((flags >> 1) & 1);
        obj_item_props props(Item_infos[info_index].name, pos, orient, item_flags, COLLIDE_ITEM, -1.0f, INVALID_SALVAGE_MATERIAL);
        props.set(PROP_WEAPON_INFO_NAME, weapons_get_weapon_info((unsigned int)w_index)->name);
        wp = World.create_object<weapon>(props, 0xFFFFFFFF);
        wp->item_flags.respawn = (flags >> 1) & 1;
    }
    else
    {
        wp->item_flags.respawn = (flags >> 1) & 1;
        wp->item_flags.simulate = flags & 1;
        wp->item_flags.preplaced = (flags >> 4) & 1;
        wp->update_pos_and_orient(pos, orient, true);
    }
    multi_sim_state<multi_item_state>* sim_state = object_item_alloc_sim_state(); // inlined
    wp->mp_sim_state = sim_state;
    if (sim_state == nullptr)
    {
        keen::debug::nativePrintArgs("Out of item sim states!");
    }
    if ((flags & 1) == 0 && wp->havok_handle != 0xFFFFFFFF)
    {
        havok_body_force_deactivate(wp->havok_handle);
    }
    if ((flags & 8) != 0)
    {
        wp->set_rounds_in_magazine((int)packer.get_next_bits(8)); // inlined (clamped to the magazine)
        wp->set_rounds_in_reserve((int)(unsigned short)packer.get_next_bits(16)); // inlined (clamped to the reserve)
        if ((flags & 4) == 0 && wp->havok_handle != 0xFFFFFFFF)
        {
            vector velocity;
            vec_delta_decompress(packer, velocity, ZERO_VECTOR, 16, 1280.0f, 0.0f, 3.0f);
            havok_body_set_linear_velocity(wp->havok_handle, velocity);
        }
    }
    wp->remote_obj_flags.remote_object = 1;
    game_handle = wp->get_handle();
}

// 0x00959630 (:607): nothing is sent
void item_multi_registration::send_destroy_packet(unsigned int game_handle, unsigned char* packet_data, unsigned short& packet_size) const
{
}

// 0x0097DAA0 (:617)
void item_multi_registration::process_destroy_packet(unsigned int game_handle, int& packet_offset, unsigned char* packet_data) const
{
    item* itemp = item_from_handle(game_handle);
    if (itemp != nullptr)
    {
        World.destroy_object(itemp->get_handle(), false);
    }
}

// 0x009F48F0 (:625)
// Original: any other object is read as a null item (its sim state at 0x124).
void item_multi_registration::process_object_free(object* obj) const
{
    item* itemp = nullptr;
    if (obj != nullptr && obj->get_obj_type() == OT_ITEM)
    {
        itemp = (item*)obj;
    }
    if (itemp != nullptr && itemp->mp_sim_state != nullptr)
    {
        Sim_state_item_allocator.free(itemp->mp_sim_state);
        itemp->mp_sim_state = nullptr;
    }
    multi_game_process_remote_object_free_common(obj);
}

// 0x00959640 (:673): restarts the 1 s rest timer once it ran out (nothing reads it)
void item_multi_registration::post_update_cleanup() const
{
    if (Rest_timestamp.valid() && !Rest_timestamp.elapsed())
    {
        return;
    }
    Rest_timestamp.set(1000); // Magic number: 1 s
}

// 0x0099B590 (:682)
// A bodiless item eases towards the state (99.5 % a second). A body more than 0.1 m off, awake or simulating on the
// server, takes the state's velocities when simulating and while some player sees it is steered there by the soft
// keyframe (over two frames, factors half the inverse time, 5 m at most); otherwise it is placed at the state. A
// body that is not simulating gets Havok property 7 and the item takes the body's position.
void item_multi_registration::move(unsigned int game_handle) const
{
    item* itemp = item_from_handle(game_handle);
    if (itemp == nullptr || itemp->mp_sim_state == nullptr)
    {
        return;
    }
    multi_item_state state;
    bool exceeded_extrap_time;
    if (!itemp->mp_sim_state->obtain_state(Clock_current_time, &state, exceeded_extrap_time))
    {
        return;
    }
    if (itemp->havok_handle == 0xFFFFFFFF)
    {
        const float t = exp_lerp_factor(0.995f, Frametime); // Magic number: 99.5 % a second
        itemp->pos.x = (state.position.x - itemp->pos.x) * t + itemp->pos.x;
        itemp->pos.y = itemp->pos.y + (state.position.y - itemp->pos.y) * t;
        itemp->pos.z = itemp->pos.z + (state.position.z - itemp->pos.z) * t;
        const quaternion q_from(itemp->orient);
        const quaternion q_to(state.orientation);
        quaternion q;
        pltind_slerp(q, q_from, q_to, t);
        q.extract_matrix(itemp->orient);
        return;
    }
    hkpRigidBody* rb = havok_body_get_pointer(itemp->havok_handle);
    const hkVector4f cur_pos = rb->getPosition();
    const hkQuaternionf cur_rot = rb->getRotation();
    vector body_pos;
    havok_to_game_vector(body_pos, cur_pos);
    bool awake = true;
    if (!havok_body_is_active(itemp->havok_handle) || havok_body_get_motion_type(itemp->havok_handle) == 4) // Magic number: hkpMotion::MOTION_KEYFRAMED
    {
        awake = false;
    }
    bool steered = false;
    if (0.010000001f < vec_dist_squared(body_pos, state.position) && (awake || state.simulating)) // Magic number: 0.1 m
    {
        if (state.simulating)
        {
            havok_body_set_linear_velocity(itemp->havok_handle, state.linear_velocity);
            havok_body_set_angular_velocity(itemp->havok_handle, state.angular_velocity);
        }
        if (multi_game_player_can_view_object(nullptr, itemp))
        {
            hkVector4f target_pos;
            game_to_havok_vector(target_pos, state.position);
            hkQuaternionf target_rot;
            game_to_havok_matrix(target_rot, state.orientation);
            const float delta_time = Frametime * 2.0f;
            const float inv_delta_time = 1.0f / delta_time;
            hkpKeyFrameUtility::KeyFrameInfo key_frame_info;
            key_frame_info.fastSetUsingPositionOrientationPair(cur_pos, cur_rot, target_pos, target_rot, inv_delta_time);
            const float factor = inv_delta_time * 0.5f;
            hkpKeyFrameUtility::AccelerationInfo accel_info;
            const __m128 factors = _mm_setr_ps(factor, factor, factor, 0.0f);
            accel_info.m_linearPositionFactor.m_quad = factors;
            accel_info.m_angularPositionFactor.m_quad = factors;
            accel_info.m_linearVelocityFactor.m_quad = factors;
            accel_info.m_angularVelocityFactor.m_quad = factors;
            accel_info.m_maxAllowedDistance = 5.0f; // Magic number: 5 m
            hkpKeyFrameUtility::applySoftKeyFrame(key_frame_info, accel_info, delta_time, inv_delta_time, rb);
            steered = true;
        }
    }
    if (!steered)
    {
        havok_body_set_pos_orient(itemp->havok_handle, state.position, state.orientation);
    }
    const bool has_property = rb->hasProperty(7, hkpWorldObject::MULTI_THREADING_CHECKS_ENABLE); // Magic number: Havok property 7
    if (!state.simulating)
    {
        if (!has_property)
        {
            havok_set_property(rb, 7, nullptr);
        }
        havok_to_game_vector(itemp->pos, rb->getPosition());
        return;
    }
    if (has_property)
    {
        havok_remove_property(rb, 7);
    }
}

// 0x009F4940 (:775)
// The body's pose and velocities, the visibility, the lighting flag, whether the body simulates (awake and not
// keyframed) and the pickup value, at the current network time.
// Original: the state is allocated before the object is checked, and a missing item is dereferenced.
void item_multi_registration::get_state(unsigned int game_handle, dc_state_list* dcsl) const
{
    object_state_data<item_data_bundle>* data = object_item_alloc_item_state_data(); // inlined
    if (data == nullptr)
    {
        dcsl->m_data = nullptr;
        dcsl->m_size = 0;
        return;
    }
    data->initialize();
    dcsl->m_data = (unsigned char*)data;
    dcsl->m_size = sizeof(object_state_data<item_data_bundle>);
    item_data_bundle& base = data->base_data;
    base.network_time = Clock_current_time;
    item* itemp = item_from_handle(game_handle);
    base.dont_render = !itemp->get_is_visible();
    base.lighting_set_once = itemp->obj_flags.lighting_set_once;
    base.simulating = havok_body_is_active(itemp->havok_handle) && havok_body_get_motion_type(itemp->havok_handle) != 4; // Magic number: hkpMotion::MOTION_KEYFRAMED
    havok_body_get_pos_orient(itemp->havok_handle, base.position, base.orientation);
    havok_body_get_linear_velocity(itemp->havok_handle, base.linear_velocity);
    havok_body_get_angular_velocity(itemp->havok_handle, base.angular_velocity);
    base.pickup_value = itemp->pickup_value;
}

// 0x009B0B40 (:813)
void item_multi_registration::free_state(dc_state_list* dcsl) const
{
    Object_item_allocator.free((object_state_data<item_data_bundle>*)dcsl->m_data);
    dcsl->m_data = nullptr;
    dcsl->m_size = 0;
}

// 0x0097DAE0 (:821)
bool item_multi_registration::check_object_is_dirty(dc_state_list* ref_state, dc_state_list* cur_state, const remote_object* rop) const
{
    if (ref_state == nullptr || cur_state == nullptr)
    {
        return true;
    }
    if (item_from_handle(rop->m_game_handle) == nullptr)
    {
        return false;
    }
    return query_possible_mask_bits_base(item_base(cur_state), item_base(ref_state)) != 0;
}

// 0x00959690 (:840)
// The mask (8 bits), the position (16 bits from zero, or a delta from the connection's record), the orientation
// (pitch, bank and heading), the velocities likewise, and the lighting, simulating and hidden flags.
void item_multi_registration::get_compressed_state(dc_state_list* current_state, dc_state_list* reference_state, dc_state_list* compressed_state, multi_connection* cp, multi_core_state_priority priority, unsigned short mask_bits) const
{
    item_data_bundle& cur = item_base(current_state);
    const item_data_bundle* client_source = reference_state != nullptr ? &item_transmitted(reference_state, cp) : &cur;
    item_data_bundle& client = item_transmitted(current_state, cp);
    memcpy(&client, client_source, sizeof(item_data_bundle));
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
        buffer_delta_compressed_vector(packer, pos_mask, client_vec, pos_mask == 3 ? ZERO_VECTOR : client.position, cur.position);
        client.transmitted_data_set |= 3;
        client.position = client_vec;
    }
    if ((mask & 4) != 0)
    {
        matrix client_matrix;
        buffer_compressed_pbh(packer, 4, client_matrix, cur.orientation);
        client.transmitted_data_set |= 4;
        client.orientation = client_matrix;
    }
    const unsigned int lvel_mask = mask & 0x18;
    if (lvel_mask != 0)
    {
        buffer_delta_compressed_vector(packer, lvel_mask, client_vec, lvel_mask == 0x18 ? ZERO_VECTOR : client.linear_velocity, cur.linear_velocity);
        client.linear_velocity = client_vec;
        client.transmitted_data_set |= 0x18;
    }
    const unsigned int avel_mask = mask & 0x60;
    if (avel_mask != 0)
    {
        buffer_delta_compressed_vector(packer, avel_mask, client_vec, avel_mask == 0x60 ? ZERO_VECTOR : client.angular_velocity, cur.angular_velocity);
        client.angular_velocity = client_vec;
        client.transmitted_data_set |= 0x60;
    }
    if ((mask & 0x80) != 0)
    {
        packer.add_bool(cur.lighting_set_once);
        packer.add_bool(cur.simulating);
        packer.add_bool(cur.dont_render);
        client.lighting_set_once = cur.lighting_set_once;
        client.simulating = cur.simulating;
        client.transmitted_data_set |= 0x80;
        client.dont_render = cur.dont_render;
    }
    compressed_state->m_size = (unsigned long)(long long)(float)ceil((double)((float)(double)packer.get_start_bit() * 0.125f));
}

// 0x009F8A70 (:1023)
// The received state on the reference (or the current state, deltas from itself); the item takes the pickup value,
// the visibility (a respawning item's respawn timer starts when it hides, else it respawns) and the lighting flag.
// Original: with a reference the state list's size is set to one bundle (0x58).
bool item_multi_registration::apply_compressed_state(unsigned int game_handle, dc_state_list* reference_state, dc_state_list* compressed_state, dc_state_list** resultant_state, unsigned int network_time) const
{
    item* itemp = item_from_handle(game_handle);
    if (itemp == nullptr)
    {
        return false;
    }
    dc_state_list* dcsl = multi_core_dc_alloc_state_list(); // inlined
    *resultant_state = dcsl;
    if (dcsl == nullptr)
    {
        return false;
    }
    dcsl->m_data = nullptr;
    dcsl->m_reference_ackid = 0;
    dcsl->m_size = 0;
    object_state_data<item_data_bundle>* data;
    if (reference_state == nullptr || reference_state->m_data == nullptr)
    {
        get_state(game_handle, dcsl);
        data = (object_state_data<item_data_bundle>*)dcsl->m_data;
        data->base_data.network_time = 0;
    }
    else
    {
        data = object_item_alloc_item_state_data(); // inlined
        if (data == nullptr)
        {
            multi_core_dc_free_state_list(dcsl);
            *resultant_state = nullptr;
            return false;
        }
        data->initialize();
        memcpy(&data->base_data, &item_base(reference_state), sizeof(item_data_bundle));
        dcsl->m_data = (unsigned char*)data;
        dcsl->m_size = sizeof(item_data_bundle);
    }
    item_data_bundle& state = data->base_data;
    const item_data_bundle reference = state; // (the deltas' reference: the reference state, or the current one)
    bit_packer packer(compressed_state->m_data, compressed_state->m_size);
    const unsigned int mask = packer.get_next_bits(8);
    const unsigned int pos_mask = mask & 3;
    if (pos_mask != 0)
    {
        apply_delta_compressed_vector(packer, pos_mask, state.position, pos_mask == 3 ? ZERO_VECTOR : reference.position);
    }
    if ((mask & 4) != 0)
    {
        apply_compressed_pbh(packer, 4, state.orientation);
    }
    const unsigned int lvel_mask = mask & 0x18;
    if (lvel_mask != 0)
    {
        apply_delta_compressed_vector(packer, lvel_mask, state.linear_velocity, lvel_mask == 0x18 ? ZERO_VECTOR : reference.linear_velocity);
    }
    const unsigned int avel_mask = mask & 0x60;
    if (avel_mask != 0)
    {
        apply_delta_compressed_vector(packer, avel_mask, state.angular_velocity, avel_mask == 0x60 ? ZERO_VECTOR : reference.angular_velocity);
    }
    if ((mask & 0x80) != 0)
    {
        state.lighting_set_once = packer.get_bool();
        state.simulating = packer.get_bool();
        state.dont_render = packer.get_bool();
    }
    itemp->pickup_value = state.pickup_value;
    state.network_time = network_time;
    object_set_visibility_plus_children(itemp, !state.dont_render);
    if (itemp->item_flags.respawn)
    {
        if (!itemp->get_is_visible())
        {
            itemp->respawn_timer.set(multi_game_get_item_respawn_delay(itemp));
        }
        else
        {
            itemp->respawn();
        }
    }
    itemp->obj_flags.lighting_set_once = state.lighting_set_once;
    if (itemp->mp_sim_state != nullptr)
    {
        itemp->mp_sim_state->add_state(state);
    }
    return true;
}

// 0x00959AB0 (:1162): bits per data set plus the 8 mask bits less one, in bytes
void item_multi_registration::query_space_for_mask(query_size_results* qs_results, void* additional_data) const
{
    for (int i = 0; i < 5; i++)
    {
        const unsigned int mask = qs_results[i].mask;
        int bits = 0;
        switch (mask & 3)
        {
        case 3:
            bits = 48;
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
            bits += 15;
        }
        switch (mask & 0x18)
        {
        case 0x18:
            bits += 48;
            break;
        case 0x10:
            bits += 24;
            break;
        case 8:
            bits += 15;
            break;
        }
        switch (mask & 0x60)
        {
        case 0x60:
            bits += 48;
            break;
        case 0x40:
            bits += 24;
            break;
        case 0x20:
            bits += 15;
            break;
        }
        if ((mask & 0x80) != 0)
        {
            bits += 3;
        }
        qs_results[i].num_bytes = bits == 0 ? 0 : (unsigned short)((bits + 15) >> 3);
    }
}

// 0x009C76C0 (:1228)
// By the distance to the viewer's camera (squared: beyond 20 m -3, 10 m -2, 5 m 0, 3 m +1, else +2) and whether the
// camera looks at it (within about 8 degrees: +2, and the flags at extreme), from State_priority_base. Without a viewer
// both bonuses apply.
void item_multi_registration::query_space_for_priority(const remote_object* rop, dc_state_list* current_state, dc_state_list* reference_state, unsigned short possible_mask_bits, multi_connection* cp, query_size_results* qs_results) const
{
    player* viewer = multi_game_data_get_player_instance(cp->game_data);
    state_priority_bundle* bundle = const_cast<state_priority_bundle*>(&rop->m_state_priority_bundle[cp->index_in_session]);
    float dist2 = 0.0f;
    float look_dot = 1.0f;
    if (viewer != nullptr)
    {
        const item_data_bundle& cur = item_base(current_state);
        const unsigned char* ref_data = reference_state != nullptr ? reference_state->m_data : nullptr;
        vector camera_pos;
        const matrix* camera_orient;
        if (object_is_local_player(viewer))
        {
            camera_pos = Rfg_camera.real_pos;
            camera_orient = &Rfg_camera.real_orient;
        }
        else
        {
            camera_pos = viewer->mp_camera_pos;
            camera_orient = &viewer->mp_camera_orient;
        }
        vector target = cur.position;
        dist2 = (camera_pos.x - target.x) * (camera_pos.x - target.x) + (camera_pos.y - target.y) * (camera_pos.y - target.y) + (camera_pos.z - target.z) * (camera_pos.z - target.z);
        if (ref_data != nullptr)
        {
            const item_data_bundle& ref = item_base(reference_state);
            const float ref_dist2 = (camera_pos.x - ref.position.x) * (camera_pos.x - ref.position.x) + (camera_pos.y - ref.position.y) * (camera_pos.y - ref.position.y) + (camera_pos.z - ref.position.z) * (camera_pos.z - ref.position.z);
            if (0.0f <= dist2 - ref_dist2)
            {
                dist2 = ref_dist2;
            }
            // The direction is to the position last sent to this connection.
            target = item_transmitted(reference_state, cp).position;
        }
        vector dir = target - camera_pos;
        const float mag = dir.mag_and_normalize_safe();
        look_dot = mag <= 0.0f ? 0.0f : camera_orient->fvec.y * dir.y + camera_orient->fvec.x * dir.x + camera_orient->fvec.z * dir.z;
    }
    int distance_bonus;
    if (dist2 > 400.0f) // Magic numbers: squared distances 20, 10, 5 and 3 m
    {
        distance_bonus = -3;
    }
    else if (dist2 > 100.0f)
    {
        distance_bonus = -2;
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
    const int look_bonus = 0.99f <= look_dot ? 2 : 0; // Magic number: cos of about 8 degrees
    for (int i = 0; i < 5; i++)
    {
        int priority = (int)(bundle->state_data[i].time_since_update >> 1) + State_priority_base[i] + look_bonus + distance_bonus;
        if (i == 4 && look_bonus != 0)
        {
            priority = MULTI_CORE_STATE_PRIORITY_EXTREME;
        }
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
                qs_results[level].mask |= (unsigned short)State_mask[i];
            }
        }
    }
    for (int level = 0; level < 5; level++)
    {
        qs_results[level].mask &= possible_mask_bits;
    }
    query_space_for_mask(qs_results, nullptr);
}

// 0x00959DC0 (:1347)
void item_multi_registration::notify_transmitted_updated(const remote_object* rop, dc_state_list* current_state, multi_connection* recipient, unsigned short possible_mask_bits, unsigned short mask_bits_transmitted) const
{
    state_priority_bundle* bundle = const_cast<state_priority_bundle*>(&rop->m_state_priority_bundle[recipient->index_in_session]);
    for (int i = 0; i < 5; i++)
    {
        if ((State_mask[i] & possible_mask_bits) == 0 || (State_mask[i] & mask_bits_transmitted) != 0)
        {
            bundle->state_data[i].time_since_update = 0;
        }
        else
        {
            bundle->state_data[i].time_since_update++;
        }
    }
    item_transmitted(current_state, recipient).full_update = possible_mask_bits == 0 || possible_mask_bits == mask_bits_transmitted;
}

// 0x00959E50 (:1370)
bool item_multi_registration::check_full_update(dc_state_list* ref_state, multi_connection* cp) const
{
    return item_transmitted(ref_state, cp).full_update;
}

// 0x009C7AF0 (:1382)
void item_multi_registration::reset_state_data() const
{
    for (object_state_data<item_data_bundle>* data = Object_item_allocator.get_first(0); data != nullptr;)
    {
        data = Object_item_allocator.free(data);
    }
}

// 0x009B0B60 (:1387)
void item_multi_registration::free_mp_state_data() const
{
    remote_object* rop = Remote_objects_list;
    while (rop != nullptr)
    {
        if (rop->current_type == registration_type)
        {
            item* itemp = item_from_handle(rop->m_game_handle);
            if (itemp != nullptr && itemp->mp_sim_state != nullptr)
            {
                Sim_state_item_allocator.free(itemp->mp_sim_state);
                itemp->mp_sim_state = nullptr;
            }
        }
        rop = rop->next;
        if (rop == Remote_objects_list)
        {
            break;
        }
    }
}

// 0x0096BC40
item_multi_registration::item_multi_registration()
{
    num_masks = 0;
}

// 0x0096BD50
item_multi_registration::~item_multi_registration()
{
}

// 0x009DF700 (:1410)
// With clear_allocators the pools are emptied (a level change). Otherwise the pools (3200 state datas, 128 received
// state sets) and the registration ("item", type 3, every 100 ms).
void multi_game_object_items_init(bool clear_allocators)
{
    if (clear_allocators)
    {
        for (object_state_data<item_data_bundle>* data = Object_item_allocator.get_first(0); data != nullptr;)
        {
            data = Object_item_allocator.free(data);
        }
        for (multi_sim_state<multi_item_state>* sim_state = Sim_state_item_allocator.get_first(0); sim_state != nullptr;)
        {
            sim_state = Sim_state_item_allocator.free(sim_state);
        }
        return;
    }
    Object_item_allocator.init(MP_allocators_mempool);
    Sim_state_item_allocator.init(MP_allocators_mempool);
    // Original: the allocation is not checked before the masks are written.
    item_multi_registration* registration = new (MP_allocators_mempool.alloc(sizeof(item_multi_registration), 0)) item_multi_registration();
    registration->name = "item";
    registration->update_rate = 100;
    Item_mr = registration;
    registration->add_state_mask("16BIT_D", "POS", 3, 48, 0.01f, 1280.0f, 0.0f, 3.0f);
    registration->add_state_mask("8BIT_D", "POS", 2, 24, 0.01f, 5.0f, 0.0f, 3.0f);
    registration->add_state_mask("5BIT_D", "POS", 1, 15, 0.01f, 0.625f, 0.0f, 3.0f);
    registration->add_state_mask("ORIENT_PBH", "ORIENT_PBH", 4, 15, 0.19634955f, 3.1415927f, 0.0f, 1.0f);
    registration->add_state_mask("16BIT_D", "LVEL", 0x18, 48, 0.1f, 1280.0f, 0.0f, 3.0f);
    registration->add_state_mask("8BIT_D", "LVEL", 0x10, 24, 0.1f, 5.0f, 0.0f, 3.0f);
    registration->add_state_mask("5BIT_D", "LVEL", 8, 15, 0.1f, 0.625f, 0.0f, 3.0f);
    registration->add_state_mask("16BIT_D", "AVEL", 0x60, 48, 0.1f, 512.0f, 0.0f, 3.0f);
    registration->add_state_mask("8BIT_D", "AVEL", 0x40, 24, 0.1f, 2.0f, 0.0f, 3.0f);
    // Original: the 5-bit angular velocity mask is named "8BIT_D".
    registration->add_state_mask("8BIT_D", "AVEL", 0x20, 15, 0.1f, 0.25f, 0.0f, 3.0f);
    registration->add_state_mask("DATA", "DATA", 0x80, 3, 0.0f, 0.0f, 0.0f, 1.0f);
    multi_core_object_register_object_type(MULTI_OBJECT_TYPE_ITEM, registration);
}

// 0x009B0BD0 (:1533)
// Original: the sim state pointer is not cleared.
void multi_game_object_item_cleanup(item* obj_item)
{
    Sim_state_item_allocator.free(obj_item->mp_sim_state);
}
