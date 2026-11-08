// Original: volition/rfg/code/multi/multi_game/multi_game_destruction.cpp
//
// Network sync of destruction (decompiled whole, multiplayer.md MP5; header comment in multi_game_destruction.h).
//
// The server records each destruction frame's events (rfg_dynamics' pre-collide events, in their network form: handles
// as network handles, points and directions as spherical coordinates quantized to a few bits), the general and
// rfg_movers the frame created (with their new network handles), the rfg_movers it destroyed and the shardify delete
// counts into one PT_DESTRUCTION_FRAME packet, sends it reliably to every connected client and keeps a copy in its own
// frame list (100 x 5 frames, for join in progress). A client keeps the frames in order and plays back the next one
// when rfg_dynamics asks for it, feeding the events back in as pre-collide events from the server, then gives the
// movers the frame created their remote objects and states. The working data setters (called by rfg_dynamics as it
// makes an event) round a server's event through its network form, so that the server applies what the clients apply.
//
// Built twice: whole in the multi unity TU (rfg_multi), and alone with OPENRFG_MULTI_DESTRUCTION_ALONE (rfg_multi_alone,
// for the programs without the multi tree: the H3/MP0 single player subset, where a server's working data is left
// unset).

#include "volition/rfg/code/multi/multi_game/multi_game_destruction.h"

#include "volition/rfg/code/destruction/rfg_dynamics.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_game/multi_game_migration.h"

#ifndef OPENRFG_MULTI_DESTRUCTION_ALONE
#include "keen/base/trace.hpp"
#include "volition/rfg/code/code_effects/explosion_shared.h"
#include "volition/rfg/code/destruction/rfg_mover.h"
#include "volition/rfg/code/destruction/rfg_shapes.h"
#include "volition/rfg/code/destruction/rfg_stress.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/interface/ui_document_hud_multiplayer.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/level/world.inl"
#include "volition/rfg/code/multi/multi_core/include/multi_connection.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_compression.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_delta_compression.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_io.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_objects.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_packet_types.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_session_base.h"
#include "volition/rfg/code/multi/multi_core/include/multi_pool.inl"
#include "volition/rfg/code/multi/multi_core/include/multi_session.h"
#include "volition/rfg/code/multi/multi_game/modes/multi_game_mode_common.h"
#include "volition/rfg/code/multi/multi_game/multi_game_destruction_migration.h"
#include "volition/rfg/code/multi/multi_game/multi_game_object_movers.h"
#include "volition/rfg/code/multi/multi_game/multi_game_object_rfg_movers.h"
#include "volition/rfg/code/multi/multi_game/multi_game_sim_state.h"
#include "volition/rfg/code/objects/movers/general_mover.h"
#include "volition/rfg/code/objects/object_type.h"
#include "volition/rfg/code/rfg.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/os/cs_wrapper.h"
#include "volition/vlib/code/os/frametime.h"
#include "volition/vlib/code/os/vconsole.h"
#include "volition/vlib/code/util/string_comp.h"
#include "volition/vlib/code/util/varray.h"

#include <stdio.h>
#include <string.h>
#endif

#ifndef OPENRFG_MULTI_DESTRUCTION_ALONE
// ---- file-static globals
// unsigned short Cur_network_handle (0x0167DD78) is used here too; placed in multi.cpp.
// jip_destruction_sync jip_destruction_sync_state (0x0167E708) is used here too; placed in multi.cpp.
// dynamic_uint_hash_table<remote_object*> Game_handle_to_rop_hash (0x01691B40) is used here too; placed in multi_core_delta_compression.cpp.
// dynamic_uint_hash_table<remote_object*> Network_handle_to_rop_hash (0x01691B88) is used here too; placed in multi_core_delta_compression.cpp.
// dynamic_uint_hash_table<network_game_handle*> Reserved_game_to_network_hash (0x01691C18) is used here too; placed in multi_core_objects.cpp.
// dynamic_uint_hash_table<network_game_handle*> Reserved_network_to_game_hash (0x01691C60) is used here too; placed in multi_core_delta_compression.cpp.
static farray<multi_player_uid, 16> JIP_client_to_sync_list; // 0x01692B88
static multi_pool<multi_sim_state<multi_mover_state>, 128> General_mover_sim_state_allocator; // 0x01692ED0
static multi_pool<multi_sim_state<multi_mover_state>, 128> Rfg_mover_sim_state_allocator; // 0x01693010
static multi_pool<object_mover_error_info, 256> Object_mover_error_info_allocator; // 0x01693050
static int Num_used_destruction_frames; // 0x02FE5BEC
static char Destruction_spew_buf[4096]; // 0x02FE5C68
static unsigned short Playback_next_client_destruction_frame; // 0x02FE6C74
// bool jip_destruction_sync_possible (0x02FE6C77) is used here too; placed in multi.cpp.
static destruction_frame_data* MP_destruction_frame_used_list; // 0x02FE73C4
static int Num_free_destruction_frames; // 0x02FE87DC
static destruction_frame_data* MP_destruction_frame_free_list; // 0x02FE8E50
static _destruction_packet Destruction_packet; // 0x02FE8E60
// destruction_frame_data* MP_destruction_frame_pool (0x02FEB564) is used here too; placed in multi_game.cpp.
// bool Server (0x02FEB590) is used here too; placed in multi_core_session_generic.cpp.
static bool Spew_to_buffer; // 0x02FEB68A
static destruction_frame_data* Active_destruction_frame_data; // 0x02FEB69C
// migration_state Migration_state (0x02FEB6C4) is used here too; placed in multi.cpp.
// bool Multi_game_has_skipped_game_migration (0x02FEB6C9) is used here too; placed in multi_game.cpp.
static cs_wrapper Destruction_reorder_wrapper(0); // 0x02FED894

vrm_effect_info Vrm_effect_infos[20]; // 0x02FED258
int Vrm_active_effect_info_count; // 0x02FEB6A0

// ---- internal functions
static void get_rfg_mover_subpiece_info(unsigned int handle, unsigned short& num_subieces, int& anchor_str, int& link_str, int& dyn_link_str); // 0x00998740 (:557)
static void init_destruction_validate_sync(); // 0x00952AE0 (:593)
static void extract_destruction_frame_header(unsigned char* packet_data, unsigned short& destruction_frame, unsigned char& num_destruction_events, unsigned char& num_general_movers_created, unsigned char& num_rfg_movers_created, unsigned char& num_rfg_movers_destroyed, unsigned char& num_shardify_delete_counts, unsigned char& migration); // 0x00952AF0 (:770)
static void multi_destruction_compress_explosion(rfg_collision_event_base& event_base); // 0x009BDED0 (:804)
static void multi_destruction_decompress_explosion(rfg_collision_event_base& event_base); // 0x009BE2C0 (:842)
static void multi_destruction_compress_object_simple(rfg_collision_event_base& event_base); // 0x009BE510 (:875)
static void multi_destruction_decompress_object_simple(rfg_collision_event_base& event_base); // 0x009BE8F0 (:913)
static void multi_destruction_compress_object_complex(rfg_collision_event_base& event_base); // 0x009BEB10 (:945)
static void multi_destruction_decompress_object_complex(rfg_collision_event_base& event_base); // 0x009BF280 (:1013)
static void multi_destruction_compress_direct(rfg_collision_event_base& event_base); // 0x009BF6C0 (:1072)
static void multi_destruction_decompress_direct(rfg_collision_event_base& event_base); // 0x009BFA50 (:1109)
static void multi_destruction_compress_link_broken(rfg_collision_event_base& event_base); // 0x009BFC60 (:1139)
static void multi_destruction_decompress_link_broken(rfg_collision_event_base& event_base); // 0x009BFC80 (:1155)
static void multi_destruction_compress_sp_array(rfg_collision_event_base& event_base); // 0x009BFCA0 (:1169)
static void multi_destruction_decompress_sp_array(rfg_collision_event_base& event_base); // 0x009BFCC0 (:1183)
static void multi_destruction_compress_shatter(rfg_collision_event_base& event_base); // 0x009BFCF0 (:1197)
static void multi_destruction_decompress_shatter(rfg_collision_event_base& event_base); // 0x009C00D0 (:1235)
static void multi_destruction_compress_shape_cutter(rfg_collision_event_base& event_base); // 0x009C02F0 (:1268)
static void multi_destruction_decompress_shape_cutter(rfg_collision_event_base& event_base); // 0x009C0510 (:1300)
static void multi_destruction_compress_slice(rfg_collision_event_base& event_base); // 0x009C0670 (:1327)
static void multi_destruction_decompress_slice(rfg_collision_event_base& event_base); // 0x009C0920 (:1372)
static void multi_destruction_compress_repair(rfg_collision_event_base& event_base); // 0x009C0B00 (:1411)
static void multi_destruction_decompress_repair(rfg_collision_event_base& event_base); // 0x009C0D50 (:1446)
static void multi_destruction_compress_mp_destroy(rfg_collision_event_base& event_base); // 0x009C0EC0 (:1477)
static void multi_destruction_decompress_mp_destroy(rfg_collision_event_base& event_base); // 0x009C0EE0 (:1491)
static void multi_destruction_compress_mp_veh(rfg_collision_event_base& event_base); // 0x009C0F00 (:1505)
static void multi_destruction_decompress_mp_veh(rfg_collision_event_base& event_base); // 0x009C1420 (:1552)
static void get_decompressed_explosion_info(rfg_collision_event_base& event_base, unsigned char* packet_data, int& packet_offset); // 0x009C1780 (:1599)
static void get_decompressed_object_simple_info(rfg_collision_event_base& event_base, unsigned char* packet_data, int& packet_offset); // 0x009C1A00 (:1639)
static void get_decompressed_object_complex_info(rfg_collision_event_base& event_base, unsigned char* packet_data, int& packet_offset); // 0x009C1CA0 (:1684)
static void get_decompressed_direct_damage_info(rfg_collision_event_base& event_base, unsigned char* packet_data, int& packet_offset); // 0x009C2080 (:1754)
static void get_decompressed_link_broken_info(rfg_collision_event_base& event_base, unsigned char* packet_data, int& packet_offset); // 0x009C2430 (:1809)
static void get_decompressed_sp_array_info(rfg_collision_event_base& event_base, unsigned char* packet_data, int& packet_offset); // 0x009C26B0 (:1855)
static void get_decompressed_shatter_info(rfg_collision_event_base& event_base, unsigned char* packet_data, int& packet_offset); // 0x009C2900 (:1895)
static void get_decompressed_shape_cutter_info(rfg_collision_event_base& event_base, unsigned char* packet_data, int& packet_offset); // 0x009C2B80 (:1936)
static void get_decompressed_repair_info(rfg_collision_event_base& event_base, unsigned char* packet_data, int& packet_offset); // 0x009C2D90 (:1972)
static void get_decompressed_slice_info(rfg_collision_event_base& event_base, unsigned char* packet_data, int& packet_offset); // 0x009C2F90 (:2008)
static void get_decompressed_mp_destroy_info(rfg_collision_event_base& event_base, unsigned char* packet_data, int& packet_offset); // 0x009C31B0 (:2055)
static void get_decompressed_mp_veh_info(rfg_collision_event_base& event_base, unsigned char* packet_data, int& packet_offset); // 0x009C3330 (:2077)
static void get_decompressed_shardify_info(rfg_collision_event_base& event_base, unsigned char* packet_data, int& packet_offset); // 0x00952C60 (:2127)
static void get_decompressed_destruction_mode_info(rfg_collision_event_base& event_base, unsigned char* packet_data, int& packet_offset); // 0x009C3600 (:2137)
static bool is_process_destruction_allowed(); // 0x0097B5E0 (:2678)
static void add_explosion_destruction_event_to_destruction_frame(const rfg_collision_event_base& event_base); // 0x00952C90 (:2889)
static int process_destruction_event_explosion(unsigned char* packet_data); // 0x009C3B50 (:2935)
static void add_object_simple_destruction_event_to_destruction_frame(const rfg_collision_event_base& event_base); // 0x00952FF0 (:2973)
static int process_destruction_event_object_simple(unsigned char* packet_data); // 0x009C3C10 (:3020)
static void add_object_complex_destruction_event_to_destruction_frame(const rfg_collision_event_base& event_base); // 0x00953340 (:3051)
static int process_destruction_event_object_complex(unsigned char* packet_data); // 0x009C3CC0 (:3122)
static void add_direct_destruction_event_to_destruction_frame(const rfg_collision_event_base& event_base); // 0x009537C0 (:3164)
static void add_link_broken_event_to_destruction_frame(const rfg_collision_event_base& event_base); // 0x00953AB0 (:3213)
static void add_sp_array_destruction_event_to_destruction_frame(const rfg_collision_event_base& event_base); // 0x00953C90 (:3252)
static int process_destruction_event_direct(unsigned char* packet_data); // 0x009C3DD0 (:3285)
static int process_destruction_event_link_broken(unsigned char* packet_data); // 0x009C3ED0 (:3320)
static int direct_damage_event_get_size_bytes(unsigned char* packet_data); // 0x00953E40 (:3343)
static int link_broken_event_get_size_bytes(unsigned char* packet_data); // 0x00953F20 (:3360)
static int sp_array_event_get_size_bytes(unsigned char* packet_data); // 0x00953FB0 (:3377)
static int destruction_mode_event_get_size_bytes(unsigned char* packet_data); // 0x00954040 (:3394)
static int process_destruction_event_sp_array(unsigned char* packet_data); // 0x009C3F70 (:3406)
static void add_shatter_destruction_event_to_destruction_frame(const rfg_collision_event_base& event_base); // 0x00954060 (:3438)
static int process_destruction_event_shatter(unsigned char* packet_data); // 0x009C4040 (:3477)
static void add_shape_cutter_destruction_event_to_destruction_frame(const rfg_collision_event_base& event_base); // 0x00954380 (:3504)
static int process_destruction_event_shape_cutter(unsigned char* packet_data); // 0x009C40E0 (:3540)
static void add_repair_destruction_event_to_destruction_frame(const rfg_collision_event_base& event_base); // 0x00954640 (:3566)
static int process_destruction_event_repair(unsigned char* packet_data); // 0x009C4180 (:3603)
static void add_mp_destroy_destruction_event_to_destruction_frame(const rfg_collision_event_base& event_base); // 0x009548F0 (:3630)
static void add_mp_veh_destruction_event_to_destruction_frame(const rfg_collision_event_base& event_base); // 0x00954A30 (:3656)
static void add_slice_to_destruction_frame(const rfg_collision_event_base& event_base); // 0x00954D60 (:3705)
static void add_mp_shardify_to_destruction_frame(const rfg_collision_event_base& event_base); // 0x00955040 (:3751)
static void add_destruction_mode_to_destruction_frame(const rfg_collision_event_base& event_base); // 0x009C4220 (:3759)
static int process_destruction_event_mp_destroy(unsigned char* packet_data); // 0x009C4330 (:3775)
static int process_destruction_event_mp_veh(unsigned char* packet_data); // 0x009C43B0 (:3798)
static int process_destruction_event_shardify(unsigned char* packet_data); // 0x00998840 (:3831)
static int process_destruction_event_destruction_mode(unsigned char* packet_data); // 0x009C4490 (:3846)
static int process_destruction_event_slice(unsigned char* packet_data); // 0x009C4500 (:3862)
static void add_destruction_event_to_destruction_frame(const rfg_collision_event_base& event_base); // 0x009C45B0 (:3886)
static void prepare_destruction_frame(); // 0x009F0850 (:4004)
static void multi_game_destruction_release_destruction_frame(destruction_frame_data* frame_data); // 0x00955190 (:4330)
static void multi_game_destruction_add_to_free_list(destruction_frame_data* frame_data); // 0x00955260 (:4341)
static unsigned short get_destruction_frame_num_bytes(unsigned char* packet_data); // 0x00998850 (:4388)
static destruction_frame_data* multi_game_destruction_get_free_destruction_frame(); // 0x009553F0 (:4505)
static bool frame_data_contains_repair_event(unsigned char* packet_data, int num_destruction_events); // 0x009554A0 (:4540)
static void get_frame_data(unsigned char* packet_data, destruction_frame_data* frame_data); // 0x00955570 (:4623)
static bool add_destruction_events_from_packet(destruction_frame_data* frame_data, bool& multi_client_must_process); // 0x009C4920 (:4679)
static void add_destruction_frame_to_list(destruction_frame_data* frame_data); // 0x00955730 (:5145)
#endif

// The single player working data: the raw point (and normal) of one side.
static void copy_working_point(rfg_collision_physics_base& base, bool with_normal)
{
    base.collision_point_local = base.raw.rcollision_point_local;
    if (with_normal)
    {
        base.collision_normal_local = base.raw.rcollision_normal_local;
    }
}

// The server rounds its events through their network form (inlined: Multiplayer && Server).
static bool multi_destruction_server_compresses()
{
#ifndef OPENRFG_MULTI_DESTRUCTION_ALONE
    return Multiplayer && Server;
#else
    return Multiplayer && multi_is_server();
#endif
}

// 0x00952A50 (:514)
// 11 significant bits and a 5-bit shift: values over 0x7FF lose their low bits.
unsigned short int_pack16(unsigned int val)
{
    unsigned int shift = 0;
    if ((val & 0xFFFFF800) != 0)
    {
        unsigned int bit = 31;
        while ((val & (1u << bit)) == 0)
        {
            if (--bit < 11)
            {
                return (unsigned short)val; // not reached: a bit over 10 is set
            }
        }
        shift = bit - 10;
    }
    return (unsigned short)((val >> shift) | (shift << 11));
}

// 0x00952A90 (:531)
unsigned int int_unpack16(unsigned short ipacked)
{
    return (unsigned int)(ipacked & 0x7FF) << (ipacked >> 11);
}

// 0x00952AB0 (:538)
// -1 becomes 0xFFFF; other keys keep their low 16 bits.
unsigned short shapekey_pack16(unsigned int shapekey)
{
    if (shapekey == 0xFFFFFFFF)
    {
        shapekey = 0xFFFF;
    }
    return (unsigned short)shapekey;
}

// 0x00952AC0 (:548)
unsigned int shapekey_unpack16(unsigned short packed_shapekey)
{
    if (packed_shapekey == 0xFFFF)
    {
        return 0xFFFFFFFF;
    }
    return packed_shapekey;
}

#ifndef OPENRFG_MULTI_DESTRUCTION_ALONE
// ---- the network form of the events
// Points and directions travel as spherical coordinates: r in [0, 50] m (biased towards 25 m), theta in [-pi, pi],
// phi in [0, pi] (biased towards pi / 2), each quantized to the event type's number of bits. rfg.exe keeps the
// ranges as doubles of float constants (pi is (double)3.14159274f, 0x400921FB60000000).
static const double Dest_pi = (double)3.14159265358979f;
static const double Dest_half_pi = (double)1.57079632679490f; // 0x3FF921FB60000000
static const double Dest_quarter_pi = (double)0.785398163397448f; // 0x3FE921FB60000000
static const double Dest_max_r = 50.0; // Magic number: the largest local point distance (m)
static const double Dest_r_bias = 25.0; // Magic number: the r the quantization favours (m)

// A vector as r, theta and phi (inlined convert_rectilinear_to_spherical_coords and compress_float)
static void dest_compress_vector(const vector& v, unsigned int& r, unsigned int& theta, unsigned int& phi, int r_bits, int theta_bits, int phi_bits)
{
    float sph_r;
    float sph_theta;
    float sph_phi;
    convert_rectilinear_to_spherical_coords(sph_r, sph_theta, sph_phi, v);
    r = compress_float(sph_r, 0.0, Dest_max_r, Dest_r_bias, 1.0, r_bits);
    theta = compress_float(sph_theta, -Dest_pi, Dest_pi, 0.0, 1.0, theta_bits);
    phi = compress_float(sph_phi, 0.0, Dest_pi, Dest_half_pi, 1.0, phi_bits);
}

// A direction as theta and phi (its length is not sent)
static void dest_compress_direction(const vector& v, unsigned int& theta, unsigned int& phi, int theta_bits, int phi_bits)
{
    float sph_r;
    float sph_theta;
    float sph_phi;
    convert_rectilinear_to_spherical_coords(sph_r, sph_theta, sph_phi, v);
    theta = compress_float(sph_theta, -Dest_pi, Dest_pi, 0.0, 1.0, theta_bits);
    phi = compress_float(sph_phi, 0.0, Dest_pi, Dest_half_pi, 1.0, phi_bits);
}

// The inverse of dest_compress_vector (inlined decompress_double and convert_spherical_to_rectilinear_coords_double)
static void dest_decompress_vector(vector& v, unsigned int r, unsigned int theta, unsigned int phi, int r_bits, int theta_bits, int phi_bits)
{
    const double pol_r = decompress_double(r, 0.0, Dest_max_r, Dest_r_bias, 1.0, r_bits);
    const double pol_theta = decompress_double(theta, -Dest_pi, Dest_pi, 0.0, 1.0, theta_bits);
    const double pol_phi = decompress_double(phi, 0.0, Dest_pi, Dest_half_pi, 1.0, phi_bits);
    convert_spherical_to_rectilinear_coords_double(v, pol_r, pol_theta, pol_phi);
}

// The inverse of dest_compress_direction: a unit vector, normalized again
static void dest_decompress_direction(vector& v, unsigned int theta, unsigned int phi, int theta_bits, int phi_bits)
{
    const double pol_theta = decompress_double(theta, -Dest_pi, Dest_pi, 0.0, 1.0, theta_bits);
    const double pol_phi = decompress_double(phi, 0.0, Dest_pi, Dest_half_pi, 1.0, phi_bits);
    convert_spherical_to_rectilinear_coords_double(v, 1.0, pol_theta, pol_phi);
    v.normalize();
}

// Side a's point and direction
static void dest_compress_point_dir(rfg_collision_physics_base& base, int r_bits, int theta_bits, int phi_bits, int dir_theta_bits, int dir_phi_bits)
{
    rfg_collision_physics_base_compressed& comp = base.comp;
    dest_compress_vector(base.raw.rcollision_point_local, comp.ccollision_point_local_r, comp.ccollision_point_local_theta, comp.ccollision_point_local_phi, r_bits, theta_bits, phi_bits);
    dest_compress_direction(base.raw.rcollision_normal_local, comp.ccollision_dir_local_theta, comp.ccollision_dir_local_phi, dir_theta_bits, dir_phi_bits);
}

static void dest_decompress_point_dir(rfg_collision_physics_base& base, int r_bits, int theta_bits, int phi_bits, int dir_theta_bits, int dir_phi_bits)
{
    const rfg_collision_physics_base_compressed& comp = base.comp;
    dest_decompress_vector(base.collision_point_local, comp.ccollision_point_local_r, comp.ccollision_point_local_theta, comp.ccollision_point_local_phi, r_bits, theta_bits, phi_bits);
    dest_decompress_direction(base.collision_normal_local, comp.ccollision_dir_local_theta, comp.ccollision_dir_local_phi, dir_theta_bits, dir_phi_bits);
}

static void dest_compress_handle(rfg_collision_physics_base& base)
{
    if (Multiplayer)
    {
        base.comp.cobj_network_handle = multi_core_network_handle_from_game_handle(base.obj_handle, false);
    }
}

static void dest_decompress_handle(rfg_collision_physics_base& base)
{
    if (Multiplayer)
    {
        base.obj_handle = multi_core_game_handle_from_network_handle(base.comp.cobj_network_handle, false);
    }
}

// 0x009BDED0 (:804)
// LTCG: event_base in ESI (the port has no register convention).
static void multi_destruction_compress_explosion(rfg_collision_event_base& event_base)
{
    dest_compress_handle(event_base.base_a);
    dest_compress_point_dir(event_base.base_a, 10, 9, 9, 8, 8);
    // Magic numbers: the radius multiplier in [0, 3] in 8 bits
    event_base.compressed_exp_radius_multiplier = compress_float(event_base.exp_radius_multiplier, 0.0, 3.0, 0.0, 1.0, 8);
}

// 0x009BE2C0 (:842)
// LTCG: event_base in ESI (the port has no register convention).
static void multi_destruction_decompress_explosion(rfg_collision_event_base& event_base)
{
    dest_decompress_handle(event_base.base_a);
    dest_decompress_point_dir(event_base.base_a, 10, 9, 9, 8, 8);
    event_base.exp_radius_multiplier = (float)decompress_double(event_base.compressed_exp_radius_multiplier, 0.0, 3.0, 0.0, 1.0, 8);
}

// 0x009BE510 (:875)
// LTCG: event_base in ESI (the port has no register convention).
static void multi_destruction_compress_object_simple(rfg_collision_event_base& event_base)
{
    dest_compress_handle(event_base.base_a);
    dest_compress_point_dir(event_base.base_a, 9, 9, 8, 9, 8);
    event_base.base_a.comp.cdamage = int_pack16(event_base.damage); // inlined
}

// 0x009BE8F0 (:913)
// LTCG: event_base in ESI (the port has no register convention).
static void multi_destruction_decompress_object_simple(rfg_collision_event_base& event_base)
{
    dest_decompress_handle(event_base.base_a);
    dest_decompress_point_dir(event_base.base_a, 9, 9, 8, 9, 8);
    event_base.damage = int_unpack16(event_base.base_a.comp.cdamage); // inlined
}

// 0x009BEB10 (:945)
// LTCG: event_base in ESI (the port has no register convention).
static void multi_destruction_compress_object_complex(rfg_collision_event_base& event_base)
{
    dest_compress_handle(event_base.base_a);
    dest_compress_point_dir(event_base.base_a, 9, 9, 8, 8, 7);
    dest_compress_handle(event_base.base_b);
    dest_compress_point_dir(event_base.base_b, 9, 9, 8, 8, 8);
    event_base.base_a.comp.cdamage = int_pack16(event_base.damage); // inlined
}

// 0x009BF280 (:1013)
// LTCG: event_base in ESI (the port has no register convention).
static void multi_destruction_decompress_object_complex(rfg_collision_event_base& event_base)
{
    dest_decompress_handle(event_base.base_a);
    dest_decompress_point_dir(event_base.base_a, 9, 9, 8, 8, 7);
    dest_decompress_handle(event_base.base_b);
    dest_decompress_point_dir(event_base.base_b, 9, 9, 8, 8, 8);
    event_base.damage = int_unpack16(event_base.base_a.comp.cdamage); // inlined
}

// 0x009BF6C0 (:1072)
// LTCG: event_base in ESI (the port has no register convention).
static void multi_destruction_compress_direct(rfg_collision_event_base& event_base)
{
    dest_compress_handle(event_base.base_a);
    dest_compress_point_dir(event_base.base_a, 10, 10, 10, 10, 9);
}

// 0x009BFA50 (:1109)
// LTCG: event_base in EAX (the port has no register convention).
static void multi_destruction_decompress_direct(rfg_collision_event_base& event_base)
{
    dest_decompress_handle(event_base.base_a);
    dest_decompress_point_dir(event_base.base_a, 10, 10, 10, 10, 9);
}

// 0x009BFC60 (:1139)
// LTCG: event_base in ESI (the port has no register convention).
static void multi_destruction_compress_link_broken(rfg_collision_event_base& event_base)
{
    dest_compress_handle(event_base.base_a);
}

// 0x009BFC80 (:1155)
// LTCG: event_base in ESI (the port has no register convention).
static void multi_destruction_decompress_link_broken(rfg_collision_event_base& event_base)
{
    dest_decompress_handle(event_base.base_a);
}

// 0x009BFCA0 (:1169)
// LTCG: event_base in ESI (the port has no register convention).
static void multi_destruction_compress_sp_array(rfg_collision_event_base& event_base)
{
    dest_compress_handle(event_base.base_a);
}

// 0x009BFCC0 (:1183)
// LTCG: event_base in ESI (the port has no register convention).
static void multi_destruction_decompress_sp_array(rfg_collision_event_base& event_base)
{
    dest_decompress_handle(event_base.base_a);
}

// 0x009BFCF0 (:1197)
// LTCG: event_base in ESI (the port has no register convention).
static void multi_destruction_compress_shatter(rfg_collision_event_base& event_base)
{
    dest_compress_handle(event_base.base_a);
    dest_compress_point_dir(event_base.base_a, 9, 9, 9, 9, 8);
    event_base.base_a.comp.cdamage = int_pack16(event_base.damage); // inlined
}

// 0x009C00D0 (:1235)
// LTCG: event_base in ESI (the port has no register convention).
static void multi_destruction_decompress_shatter(rfg_collision_event_base& event_base)
{
    dest_decompress_handle(event_base.base_a);
    dest_decompress_point_dir(event_base.base_a, 9, 9, 9, 9, 8);
    event_base.damage = int_unpack16(event_base.base_a.comp.cdamage); // inlined
}

// 0x009C02F0 (:1268)
// The radius travels in 1/16 m steps in the damage field.
// LTCG: event_base in ESI (the port has no register convention).
static void multi_destruction_compress_shape_cutter(rfg_collision_event_base& event_base)
{
    rfg_collision_physics_base& base = event_base.base_a;
    dest_compress_handle(base);
    dest_compress_vector(base.raw.rcollision_point_local, base.comp.ccollision_point_local_r, base.comp.ccollision_point_local_theta, base.comp.ccollision_point_local_phi, 8, 7, 7);
    base.comp.cdamage = (unsigned short)(int)(event_base.radius * 16.0f); // Magic number: 1/16 m steps
}

// 0x009C0510 (:1300)
// LTCG: event_base in ESI (the port has no register convention).
static void multi_destruction_decompress_shape_cutter(rfg_collision_event_base& event_base)
{
    rfg_collision_physics_base& base = event_base.base_a;
    dest_decompress_handle(base);
    dest_decompress_vector(base.collision_point_local, base.comp.ccollision_point_local_r, base.comp.ccollision_point_local_theta, base.comp.ccollision_point_local_phi, 8, 7, 7);
    event_base.radius = (float)base.comp.cdamage * 0.0625f; // Magic number: 1/16 m steps
}

// 0x009C0670 (:1327)
// Side b's point holds the two slice angles, each in [-pi/4, pi/4] in 5 bits (in side b's r and theta).
// LTCG: event_base in ESI (the port has no register convention).
static void multi_destruction_compress_slice(rfg_collision_event_base& event_base)
{
    rfg_collision_physics_base& base = event_base.base_a;
    dest_compress_handle(base);
    dest_compress_vector(base.raw.rcollision_point_local, base.comp.ccollision_point_local_r, base.comp.ccollision_point_local_theta, base.comp.ccollision_point_local_phi, 8, 8, 8);
    rfg_collision_physics_base& angles = event_base.base_b;
    angles.comp.ccollision_point_local_r = compress_float(angles.raw.rcollision_point_local.x, -Dest_quarter_pi, Dest_quarter_pi, 0.0, 1.0, 5);
    angles.comp.ccollision_point_local_theta = compress_float(angles.raw.rcollision_point_local.y, -Dest_quarter_pi, Dest_quarter_pi, 0.0, 1.0, 5);
}

// 0x009C0920 (:1372)
// Kept: only in multiplayer (unlike the other decompressors, the whole body is inside the Multiplayer test).
// LTCG: event_base in ESI (the port has no register convention).
static void multi_destruction_decompress_slice(rfg_collision_event_base& event_base)
{
    if (!Multiplayer)
    {
        return;
    }
    rfg_collision_physics_base& base = event_base.base_a;
    base.obj_handle = multi_core_game_handle_from_network_handle(base.comp.cobj_network_handle, false);
    dest_decompress_vector(base.collision_point_local, base.comp.ccollision_point_local_r, base.comp.ccollision_point_local_theta, base.comp.ccollision_point_local_phi, 8, 8, 8);
    rfg_collision_physics_base& angles = event_base.base_b;
    const double angle0 = decompress_double(angles.comp.ccollision_point_local_r, -Dest_quarter_pi, Dest_quarter_pi, 0.0, 1.0, 5);
    const double angle1 = decompress_double(angles.comp.ccollision_point_local_theta, -Dest_quarter_pi, Dest_quarter_pi, 0.0, 1.0, 5);
    angles.collision_point_local.x = (float)angle0;
    angles.collision_point_local.y = (float)angle1;
}

// 0x009C0B00 (:1411)
// The radius travels in 1/16 m steps in the damage field, the duration in 100 ms steps.
// LTCG: event_base in ESI (the port has no register convention).
static void multi_destruction_compress_repair(rfg_collision_event_base& event_base)
{
    rfg_collision_physics_base& base = event_base.base_a;
    dest_compress_handle(base);
    dest_compress_vector(base.raw.rcollision_point_local, base.comp.ccollision_point_local_r, base.comp.ccollision_point_local_theta, base.comp.ccollision_point_local_phi, 8, 7, 7);
    base.comp.cdamage = (unsigned short)(int)(event_base.radius * 16.0f); // Magic number: 1/16 m steps
    event_base.repair_duration = event_base.repair_duration / 100; // Magic number: 100 ms steps
}

// 0x009C0D50 (:1446)
// LTCG: event_base in ESI (the port has no register convention).
static void multi_destruction_decompress_repair(rfg_collision_event_base& event_base)
{
    rfg_collision_physics_base& base = event_base.base_a;
    dest_decompress_handle(base);
    dest_decompress_vector(base.collision_point_local, base.comp.ccollision_point_local_r, base.comp.ccollision_point_local_theta, base.comp.ccollision_point_local_phi, 8, 7, 7);
    event_base.radius = (float)base.comp.cdamage * 0.0625f; // Magic number: 1/16 m steps
    event_base.repair_duration = event_base.repair_duration * 100; // Magic number: 100 ms steps
}

// 0x009C0EC0 (:1477)
// LTCG: event_base in ESI (the port has no register convention).
static void multi_destruction_compress_mp_destroy(rfg_collision_event_base& event_base)
{
    dest_compress_handle(event_base.base_a);
}

// 0x009C0EE0 (:1491)
// LTCG: event_base in ESI (the port has no register convention).
static void multi_destruction_decompress_mp_destroy(rfg_collision_event_base& event_base)
{
    dest_decompress_handle(event_base.base_a);
}

// 0x009C0F00 (:1505)
// The vehicle box: side a's normal keeps its length (the half extents), side b's point holds the box angles, each in
// [-pi, pi] in 9 bits.
// LTCG: event_base in ESI (the port has no register convention).
static void multi_destruction_compress_mp_veh(rfg_collision_event_base& event_base)
{
    rfg_collision_physics_base& base = event_base.base_a;
    dest_compress_handle(base);
    base.comp.cdamage = int_pack16(event_base.damage); // inlined
    dest_compress_vector(base.raw.rcollision_point_local, base.comp.ccollision_point_local_r, base.comp.ccollision_point_local_theta, base.comp.ccollision_point_local_phi, 9, 8, 8);
    dest_compress_vector(base.raw.rcollision_normal_local, base.comp.ccollision_dir_local_r, base.comp.ccollision_dir_local_theta, base.comp.ccollision_dir_local_phi, 8, 8, 8);
    rfg_collision_physics_base& angles = event_base.base_b;
    const vector box_angles = angles.raw.rcollision_point_local;
    angles.comp.ccollision_point_local_r = compress_float(box_angles.x, -Dest_pi, Dest_pi, 0.0, 1.0, 9);
    angles.comp.ccollision_point_local_theta = compress_float(box_angles.y, -Dest_pi, Dest_pi, 0.0, 1.0, 9);
    angles.comp.ccollision_point_local_phi = compress_float(box_angles.z, -Dest_pi, Dest_pi, 0.0, 1.0, 9);
}

// 0x009C1420 (:1552)
// LTCG: event_base in ESI (the port has no register convention).
static void multi_destruction_decompress_mp_veh(rfg_collision_event_base& event_base)
{
    rfg_collision_physics_base& base = event_base.base_a;
    dest_decompress_handle(base);
    event_base.damage = int_unpack16(base.comp.cdamage); // inlined
    dest_decompress_vector(base.collision_point_local, base.comp.ccollision_point_local_r, base.comp.ccollision_point_local_theta, base.comp.ccollision_point_local_phi, 9, 8, 8);
    dest_decompress_vector(base.collision_normal_local, base.comp.ccollision_dir_local_r, base.comp.ccollision_dir_local_theta, base.comp.ccollision_dir_local_phi, 8, 8, 8);
    rfg_collision_physics_base& angles = event_base.base_b;
    const double angle_y = decompress_double(angles.comp.ccollision_point_local_theta, -Dest_pi, Dest_pi, 0.0, 1.0, 9);
    const double angle_z = decompress_double(angles.comp.ccollision_point_local_phi, -Dest_pi, Dest_pi, 0.0, 1.0, 9);
    const double angle_x = decompress_double(angles.comp.ccollision_point_local_r, -Dest_pi, Dest_pi, 0.0, 1.0, 9);
    angles.collision_point_local.x = (float)angle_x;
    angles.collision_point_local.z = (float)angle_z;
    angles.collision_point_local.y = (float)angle_y;
}

// ---- reading the events of a frame
// The event's bytes are copied from the frame (packet_offset moves past them) and read bit by bit, low bit first
// (an inlined bit_packer over a stack buffer).
static void dest_read_event_bytes(unsigned char* dest, const unsigned char* packet_data, int& packet_offset, int num_bytes)
{
    memcpy(dest, packet_data + packet_offset, num_bytes);
    packet_offset += num_bytes;
}

// 0x009C1780 (:1599): 10 bytes
// LTCG: packet_data in ECX, packet_offset in EDX, event_base in EDI (the port has no register convention).
static void get_decompressed_explosion_info(rfg_collision_event_base& event_base, unsigned char* packet_data, int& packet_offset)
{
    unsigned char data[10];
    dest_read_event_bytes(data, packet_data, packet_offset, sizeof(data));
    bit_packer bits(data, sizeof(data));
    rfg_collision_physics_base_compressed& comp = event_base.base_a.comp;
    event_base.destruction_type = (destruction_event_type)bits.get_next_bits(4);
    comp.cobj_network_handle = (unsigned short)bits.get_next_bits(16);
    comp.ccollision_point_local_r = bits.get_next_bits(10);
    comp.ccollision_point_local_theta = bits.get_next_bits(9);
    comp.ccollision_point_local_phi = bits.get_next_bits(9);
    comp.ccollision_dir_local_theta = bits.get_next_bits(8);
    comp.ccollision_dir_local_phi = bits.get_next_bits(8);
    event_base.exp_unique_id = bits.get_next_bits(8);
    event_base.compressed_exp_radius_multiplier = bits.get_next_bits(8);
    multi_destruction_decompress_explosion(event_base);
}

// 0x009C1A00 (:1639): 12 bytes
// LTCG: packet_data in ECX, packet_offset in EDX, event_base in EDI (the port has no register convention).
static void get_decompressed_object_simple_info(rfg_collision_event_base& event_base, unsigned char* packet_data, int& packet_offset)
{
    unsigned char data[12];
    dest_read_event_bytes(data, packet_data, packet_offset, sizeof(data));
    bit_packer bits(data, sizeof(data));
    rfg_collision_physics_base& base = event_base.base_a;
    event_base.destruction_type = (destruction_event_type)bits.get_next_bits(4);
    base.comp.cobj_network_handle = (unsigned short)bits.get_next_bits(16);
    base.comp.ccollision_point_local_r = bits.get_next_bits(9);
    base.comp.ccollision_point_local_theta = bits.get_next_bits(9);
    base.comp.ccollision_point_local_phi = bits.get_next_bits(8);
    base.comp.ccollision_dir_local_theta = bits.get_next_bits(9);
    base.comp.ccollision_dir_local_phi = bits.get_next_bits(8);
    base.shape_key = shapekey_unpack16((unsigned short)bits.get_next_bits(16)); // inlined
    base.comp.cdamage = (unsigned short)bits.get_next_bits(16);
    event_base.flags = bits.get_next_bits(1) != 0 ? 0x8 : 0; // the apply impulse flag (0x8) only
    multi_destruction_decompress_object_simple(event_base);
}

// 0x009C1CA0 (:1684): 21 bytes
// LTCG: packet_data in ECX, event_base in EDI (the port has no register convention).
static void get_decompressed_object_complex_info(rfg_collision_event_base& event_base, unsigned char* packet_data, int& packet_offset)
{
    unsigned char data[21];
    dest_read_event_bytes(data, packet_data, packet_offset, sizeof(data));
    bit_packer bits(data, sizeof(data));
    rfg_collision_physics_base& a = event_base.base_a;
    rfg_collision_physics_base& b = event_base.base_b;
    event_base.destruction_type = (destruction_event_type)bits.get_next_bits(4);
    a.comp.cobj_network_handle = (unsigned short)bits.get_next_bits(16);
    a.comp.ccollision_point_local_r = bits.get_next_bits(9);
    a.comp.ccollision_point_local_theta = bits.get_next_bits(9);
    a.comp.ccollision_point_local_phi = bits.get_next_bits(8);
    a.comp.ccollision_dir_local_theta = bits.get_next_bits(8);
    a.comp.ccollision_dir_local_phi = bits.get_next_bits(7);
    a.shape_key = shapekey_unpack16((unsigned short)bits.get_next_bits(16)); // inlined
    b.comp.cobj_network_handle = (unsigned short)bits.get_next_bits(16);
    b.comp.ccollision_point_local_r = bits.get_next_bits(9);
    b.comp.ccollision_point_local_theta = bits.get_next_bits(9);
    b.comp.ccollision_point_local_phi = bits.get_next_bits(8);
    b.comp.ccollision_dir_local_theta = bits.get_next_bits(8);
    b.comp.ccollision_dir_local_phi = bits.get_next_bits(8);
    b.shape_key = shapekey_unpack16((unsigned short)bits.get_next_bits(16)); // inlined
    a.comp.cdamage = (unsigned short)bits.get_next_bits(16);
    event_base.flags = bits.get_next_bits(1) != 0 ? 0x8 : 0;
    multi_destruction_decompress_object_complex(event_base);
}

// 0x009C2080 (:1754): 9 bytes, then 4 per piece (its shape key and damage, 16 bits each)
// LTCG: packet_data in ECX, packet_offset in EDX (the port has no register convention).
static void get_decompressed_direct_damage_info(rfg_collision_event_base& event_base, unsigned char* packet_data, int& packet_offset)
{
    unsigned char header[9];
    dest_read_event_bytes(header, packet_data, packet_offset, sizeof(header));
    bit_packer bits(header, sizeof(header));
    rfg_collision_physics_base_compressed& comp = event_base.base_a.comp;
    event_base.destruction_type = (destruction_event_type)bits.get_next_bits(4);
    comp.cobj_network_handle = (unsigned short)bits.get_next_bits(16);
    comp.ccollision_point_local_r = bits.get_next_bits(10);
    comp.ccollision_point_local_theta = bits.get_next_bits(10);
    comp.ccollision_point_local_phi = bits.get_next_bits(10);
    comp.ccollision_dir_local_theta = bits.get_next_bits(10);
    comp.ccollision_dir_local_phi = bits.get_next_bits(9);
    event_base.data_count = (unsigned char)bits.get_next_bits(3);
    const int num_sps = event_base.data_count;
    unsigned char sp_data[7 * 4];
    dest_read_event_bytes(sp_data, packet_data, packet_offset, num_sps * 4);
    bit_packer sp_bits(sp_data, num_sps * 4);
    for (int i = 0; i < num_sps; i++)
    {
        const unsigned short compressed_shape_key = (unsigned short)sp_bits.get_next_bits(16);
        const unsigned short compressed_damage = (unsigned short)sp_bits.get_next_bits(16);
        event_base.set_combined_direct_damage(i, compressed_damage, compressed_shape_key);
    }
    multi_destruction_decompress_direct(event_base);
}

// 0x009C2430 (:1809): 3 bytes, then 2 per broken link
// LTCG: packet_offset in ECX (the port has no register convention).
static void get_decompressed_link_broken_info(rfg_collision_event_base& event_base, unsigned char* packet_data, int& packet_offset)
{
    unsigned char header[3];
    dest_read_event_bytes(header, packet_data, packet_offset, sizeof(header));
    bit_packer bits(header, sizeof(header));
    event_base.destruction_type = (destruction_event_type)bits.get_next_bits(4);
    event_base.base_a.comp.cobj_network_handle = (unsigned short)bits.get_next_bits(16);
    event_base.flags = bits.get_next_bits(1) != 0 ? 0x20 : 0; // flag 0x20 only
    event_base.data_count = (unsigned char)bits.get_next_bits(3);
    const int num_links = event_base.data_count;
    unsigned char link_data[7 * 2];
    dest_read_event_bytes(link_data, packet_data, packet_offset, num_links * 2);
    bit_packer link_bits(link_data, num_links * 2);
    for (int i = 0; i < num_links; i++)
    {
        event_base.set_link_index(i, shapekey_unpack16((unsigned short)link_bits.get_next_bits(16))); // 0xFFFF is -1 (inlined)
    }
    multi_destruction_decompress_link_broken(event_base); // inlined
}

// 0x009C26B0 (:1855): 3 bytes, then 2 per shape key
// LTCG: packet_offset in ECX (the port has no register convention).
static void get_decompressed_sp_array_info(rfg_collision_event_base& event_base, unsigned char* packet_data, int& packet_offset)
{
    unsigned char header[3];
    dest_read_event_bytes(header, packet_data, packet_offset, sizeof(header));
    bit_packer bits(header, sizeof(header));
    event_base.destruction_type = (destruction_event_type)bits.get_next_bits(4);
    event_base.base_a.comp.cobj_network_handle = (unsigned short)bits.get_next_bits(16);
    event_base.data_count = (unsigned char)bits.get_next_bits(4);
    const int num_keys = event_base.data_count;
    unsigned char key_data[15 * 2];
    dest_read_event_bytes(key_data, packet_data, packet_offset, num_keys * 2);
    bit_packer key_bits(key_data, num_keys * 2);
    for (int i = 0; i < num_keys; i++)
    {
        event_base.set_direct_sps(i, (unsigned short)key_bits.get_next_bits(16));
    }
    multi_destruction_decompress_sp_array(event_base); // inlined
}

// 0x009C2900 (:1895): 12 bytes
// LTCG: packet_data in ECX, packet_offset in EDX, event_base in EDI (the port has no register convention).
static void get_decompressed_shatter_info(rfg_collision_event_base& event_base, unsigned char* packet_data, int& packet_offset)
{
    unsigned char data[12];
    dest_read_event_bytes(data, packet_data, packet_offset, sizeof(data));
    bit_packer bits(data, sizeof(data));
    rfg_collision_physics_base& base = event_base.base_a;
    event_base.destruction_type = (destruction_event_type)bits.get_next_bits(4);
    base.comp.cobj_network_handle = (unsigned short)bits.get_next_bits(16);
    base.comp.ccollision_point_local_r = bits.get_next_bits(9);
    base.comp.ccollision_point_local_theta = bits.get_next_bits(9);
    base.comp.ccollision_point_local_phi = bits.get_next_bits(9);
    base.comp.ccollision_dir_local_theta = bits.get_next_bits(9);
    base.comp.ccollision_dir_local_phi = bits.get_next_bits(8);
    base.shape_key = shapekey_unpack16((unsigned short)bits.get_next_bits(16)); // inlined
    base.comp.cdamage = (unsigned short)bits.get_next_bits(16);
    multi_destruction_decompress_shatter(event_base);
}

// 0x009C2B80 (:1936): 8 bytes
// LTCG: packet_data in ECX, packet_offset in EDX, event_base in EDI (the port has no register convention).
static void get_decompressed_shape_cutter_info(rfg_collision_event_base& event_base, unsigned char* packet_data, int& packet_offset)
{
    unsigned char data[8];
    dest_read_event_bytes(data, packet_data, packet_offset, sizeof(data));
    bit_packer bits(data, sizeof(data));
    rfg_collision_physics_base_compressed& comp = event_base.base_a.comp;
    event_base.destruction_type = (destruction_event_type)bits.get_next_bits(4);
    comp.cobj_network_handle = (unsigned short)bits.get_next_bits(16);
    comp.ccollision_point_local_r = bits.get_next_bits(8);
    comp.ccollision_point_local_theta = bits.get_next_bits(7);
    comp.ccollision_point_local_phi = bits.get_next_bits(7);
    comp.cdamage = (unsigned short)bits.get_next_bits(14);
    event_base.exp_unique_id = bits.get_next_bits(8);
    multi_destruction_decompress_shape_cutter(event_base);
}

// 0x009C2D90 (:1972): 7 bytes
// LTCG: packet_data in ECX, packet_offset in EDX, event_base in EDI (the port has no register convention).
static void get_decompressed_repair_info(rfg_collision_event_base& event_base, unsigned char* packet_data, int& packet_offset)
{
    unsigned char data[7];
    dest_read_event_bytes(data, packet_data, packet_offset, sizeof(data));
    bit_packer bits(data, sizeof(data));
    rfg_collision_physics_base_compressed& comp = event_base.base_a.comp;
    event_base.destruction_type = (destruction_event_type)bits.get_next_bits(4);
    comp.cobj_network_handle = (unsigned short)bits.get_next_bits(16);
    comp.ccollision_point_local_r = bits.get_next_bits(8);
    comp.ccollision_point_local_theta = bits.get_next_bits(7);
    comp.ccollision_point_local_phi = bits.get_next_bits(7);
    comp.cdamage = (unsigned short)bits.get_next_bits(9);
    event_base.repair_duration = bits.get_next_bits(5);
    multi_destruction_decompress_repair(event_base);
}

// 0x009C2F90 (:2008): 7 bytes
// LTCG: packet_data in ECX, packet_offset in EDX, event_base in EDI (the port has no register convention).
static void get_decompressed_slice_info(rfg_collision_event_base& event_base, unsigned char* packet_data, int& packet_offset)
{
    unsigned char data[7];
    dest_read_event_bytes(data, packet_data, packet_offset, sizeof(data));
    bit_packer bits(data, sizeof(data));
    rfg_collision_physics_base_compressed& comp = event_base.base_a.comp;
    event_base.destruction_type = (destruction_event_type)bits.get_next_bits(4);
    comp.cobj_network_handle = (unsigned short)bits.get_next_bits(16);
    comp.ccollision_point_local_r = bits.get_next_bits(8);
    comp.ccollision_point_local_theta = bits.get_next_bits(8);
    comp.ccollision_point_local_phi = bits.get_next_bits(8);
    event_base.set_slice_index(bits.get_next_bits(2));
    event_base.base_b.comp.ccollision_point_local_r = bits.get_next_bits(5);
    event_base.base_b.comp.ccollision_point_local_theta = bits.get_next_bits(5);
    multi_destruction_decompress_slice(event_base);
}

// 0x009C31B0 (:2055): 3 bytes (4 bits unused after the type)
// LTCG: packet_offset in EAX, packet_data in ECX (the port has no register convention).
static void get_decompressed_mp_destroy_info(rfg_collision_event_base& event_base, unsigned char* packet_data, int& packet_offset)
{
    unsigned char data[3];
    dest_read_event_bytes(data, packet_data, packet_offset, sizeof(data));
    bit_packer bits(data, sizeof(data));
    event_base.destruction_type = (destruction_event_type)bits.get_next_bits(4);
    bits.get_next_bits(4);
    event_base.base_a.comp.cobj_network_handle = (unsigned short)bits.get_next_bits(16);
    multi_destruction_decompress_mp_destroy(event_base); // inlined
}

// 0x009C3330 (:2077): 14 bytes
// LTCG: event_base in ECX, packet_offset in EDX (the port has no register convention).
static void get_decompressed_mp_veh_info(rfg_collision_event_base& event_base, unsigned char* packet_data, int& packet_offset)
{
    unsigned char data[14];
    dest_read_event_bytes(data, packet_data, packet_offset, sizeof(data));
    bit_packer bits(data, sizeof(data));
    rfg_collision_physics_base_compressed& comp = event_base.base_a.comp;
    rfg_collision_physics_base_compressed& angles = event_base.base_b.comp;
    event_base.destruction_type = (destruction_event_type)bits.get_next_bits(4);
    comp.cobj_network_handle = (unsigned short)bits.get_next_bits(16);
    comp.ccollision_point_local_r = bits.get_next_bits(9);
    comp.ccollision_point_local_theta = bits.get_next_bits(8);
    comp.ccollision_point_local_phi = bits.get_next_bits(8);
    comp.ccollision_dir_local_r = bits.get_next_bits(8);
    comp.ccollision_dir_local_theta = bits.get_next_bits(8);
    comp.ccollision_dir_local_phi = bits.get_next_bits(8);
    angles.ccollision_point_local_r = bits.get_next_bits(9);
    angles.ccollision_point_local_theta = bits.get_next_bits(9);
    angles.ccollision_point_local_phi = bits.get_next_bits(9);
    comp.cdamage = (unsigned short)bits.get_next_bits(16);
    multi_destruction_decompress_mp_veh(event_base);
}

// 0x00952C60 (:2127): 1 byte (the type)
// LTCG: packet_offset in ECX, packet_data in EDX (the port has no register convention).
static void get_decompressed_shardify_info(rfg_collision_event_base& event_base, unsigned char* packet_data, int& packet_offset)
{
    event_base.destruction_type = (destruction_event_type)packet_data[packet_offset];
    packet_offset++;
}

// 0x009C3600 (:2137): the type and the handle count in one byte, then 2 bytes (big endian) per invulnerable mover
// LTCG: event_base in EDI (the port has no register convention).
static void get_decompressed_destruction_mode_info(rfg_collision_event_base& event_base, unsigned char* packet_data, int& packet_offset)
{
    const unsigned char type_count = packet_data[packet_offset];
    packet_offset++;
    unsigned int num_handles = type_count >> 4;
    event_base.destruction_type = (destruction_event_type)(type_count & 0xF);
    event_base.data_count = 0;
    for (; num_handles != 0; num_handles--)
    {
        const unsigned short net_handle = (unsigned short)(packet_data[packet_offset] << 8 | packet_data[packet_offset + 1]);
        packet_offset += 2;
        event_base.add_invulnerable_mover_handle(multi_core_game_handle_from_network_handle(net_handle, false)); // inlined
    }
}

// 0x00952C80 (:2668): the network handle itself
unsigned int get_rfg_mover_quasi_handle_for_recorded_destruction(unsigned short net_handle)
{
    return net_handle;
}

// 0x0097B5E0 (:2678): on a multiplayer client, or while migrating
static bool is_process_destruction_allowed()
{
    bool allowed = true;
    if ((!Multiplayer || Server) && Migration_state == MIGRATION_STATE_NONE)
    {
        allowed = false;
    }
    return allowed;
}
#endif

// 0x009C3720 (:2683)
void multi_game_destruction_set_explosion_working_data(rfg_collision_event_base& event_base)
{
    if (multi_destruction_server_compresses())
    {
#ifndef OPENRFG_MULTI_DESTRUCTION_ALONE
        multi_destruction_compress_explosion(event_base);
        multi_destruction_decompress_explosion(event_base);
#endif
        return;
    }
    copy_working_point(event_base.base_a, true);
}

// 0x009C3770 (:2700)
void multi_game_destruction_set_object_simple_working_data(rfg_collision_event_base& event_base)
{
    if (multi_destruction_server_compresses() && !multi_game_is_migrating()) // inlined (Migration_state)
    {
#ifndef OPENRFG_MULTI_DESTRUCTION_ALONE
        multi_destruction_compress_object_simple(event_base);
        multi_destruction_decompress_object_simple(event_base);
#endif
        return;
    }
    copy_working_point(event_base.base_a, true);
}

// 0x009C37D0 (:2717)
void multi_game_destruction_set_object_complex_working_data(rfg_collision_event_base& event_base)
{
    if (multi_destruction_server_compresses() && !multi_game_is_migrating()) // inlined (Migration_state)
    {
#ifndef OPENRFG_MULTI_DESTRUCTION_ALONE
        multi_destruction_compress_object_complex(event_base);
        multi_destruction_decompress_object_complex(event_base);
#endif
        return;
    }
    copy_working_point(event_base.base_a, true);
    copy_working_point(event_base.base_b, true);
}

// 0x009C3850 (:2741)
void multi_game_destruction_set_direct_damage_working_data(rfg_collision_event_base& event_base)
{
    if (multi_destruction_server_compresses())
    {
#ifndef OPENRFG_MULTI_DESTRUCTION_ALONE
        multi_destruction_compress_direct(event_base);
        multi_destruction_decompress_direct(event_base);
#endif
        return;
    }
    copy_working_point(event_base.base_a, true);
}

// 0x009C38A0 (:2758)
// Single player has nothing to do; a server maps the mover through its network handle.
void multi_game_destruction_set_link_broken_working_data(rfg_collision_event_base& event_base)
{
#ifndef OPENRFG_MULTI_DESTRUCTION_ALONE
    if (multi_destruction_server_compresses())
    {
        multi_destruction_compress_link_broken(event_base);
        multi_destruction_decompress_link_broken(event_base);
    }
#endif
}

// 0x009C38F0 (:2770)
void multi_game_destruction_set_sp_array_working_data(rfg_collision_event_base& event_base)
{
    if (multi_destruction_server_compresses())
    {
#ifndef OPENRFG_MULTI_DESTRUCTION_ALONE
        multi_destruction_compress_sp_array(event_base);
        multi_destruction_decompress_sp_array(event_base);
#endif
        return;
    }
    copy_working_point(event_base.base_a, true);
}

// 0x009C3960 (:2787)
void multi_game_destruction_set_shatter_working_data(rfg_collision_event_base& event_base)
{
    if (multi_destruction_server_compresses())
    {
#ifndef OPENRFG_MULTI_DESTRUCTION_ALONE
        multi_destruction_compress_shatter(event_base);
        multi_destruction_decompress_shatter(event_base);
#endif
        return;
    }
    copy_working_point(event_base.base_a, true);
}

// 0x009C39B0 (:2804)
void multi_game_destruction_set_shape_cutter_working_data(rfg_collision_event_base& event_base)
{
    if (multi_destruction_server_compresses())
    {
#ifndef OPENRFG_MULTI_DESTRUCTION_ALONE
        multi_destruction_compress_shape_cutter(event_base);
        multi_destruction_decompress_shape_cutter(event_base);
#endif
        return;
    }
    copy_working_point(event_base.base_a, false);
}

// 0x009C39F0 (:2820)
void multi_game_destruction_set_repair_working_data(rfg_collision_event_base& event_base)
{
    if (multi_destruction_server_compresses())
    {
#ifndef OPENRFG_MULTI_DESTRUCTION_ALONE
        multi_destruction_compress_repair(event_base);
        multi_destruction_decompress_repair(event_base);
#endif
        return;
    }
    copy_working_point(event_base.base_a, false);
}

// 0x009C3A30 (:2836)
// Single player has nothing to do; a server maps the mover through its network handle.
void multi_game_destruction_set_mp_destroy_working_data(rfg_collision_event_base& event_base)
{
#ifndef OPENRFG_MULTI_DESTRUCTION_ALONE
    if (multi_destruction_server_compresses())
    {
        multi_destruction_compress_mp_destroy(event_base);
        multi_destruction_decompress_mp_destroy(event_base);
    }
#endif
}

// 0x009C3A80 (:2848)
// The point of side b holds the box angles.
void multi_game_destruction_set_mp_veh_working_data(rfg_collision_event_base& event_base)
{
    if (multi_destruction_server_compresses())
    {
#ifndef OPENRFG_MULTI_DESTRUCTION_ALONE
        multi_destruction_compress_mp_veh(event_base);
        multi_destruction_decompress_mp_veh(event_base);
#endif
        return;
    }
    copy_working_point(event_base.base_a, true);
    copy_working_point(event_base.base_b, false);
}

// 0x009C3AF0 (:2865)
// The point of side b holds the slice angles.
void multi_game_destruction_set_slice_working_data(rfg_collision_event_base& event_base)
{
    if (multi_destruction_server_compresses())
    {
#ifndef OPENRFG_MULTI_DESTRUCTION_ALONE
        multi_destruction_compress_slice(event_base);
        multi_destruction_decompress_slice(event_base);
#endif
        return;
    }
    copy_working_point(event_base.base_a, false);
    copy_working_point(event_base.base_b, false);
}

// 0x00955A10 (:5358)
// Empty in rfg.exe.
void multi_game_destruction_record_last_frame(rfg_mover* mover)
{
}

#ifdef OPENRFG_MULTI_DESTRUCTION_ALONE
// 0x009DE840 (:5992)
// Without the multi tree there are no sessions: join in progress never starts (multiplayer.md MP5 ports the rest).
void multi_game_destruction_update_jip_status(bool force_disable)
{
}
#else
// ---- writing the events of a frame
// The server's packet under construction: the type and size header, the frame number, three header bytes that
// finalize fills (the counts), the events, then the movers created and destroyed and the shardify delete counts.
static void dest_packet_add_bytes(const unsigned char* data, int num_bytes)
{
    for (int i = 0; i < num_bytes; i++)
    {
        Destruction_packet._packet_data[Destruction_packet._packet_size] = data[i];
        Destruction_packet._packet_size++;
    }
}

// An event's bits are packed low bit first into a stack buffer (an inlined bit_packer) that is then appended.
// Port: the buffer starts zeroed (in rfg.exe its unused tail bits are stack garbage; the events fill whole bytes).

// 0x00952C90 (:2889): 10 bytes
static void add_explosion_destruction_event_to_destruction_frame(const rfg_collision_event_base& event_base)
{
    unsigned char data[10] = {};
    bit_packer bits(data, sizeof(data));
    const rfg_collision_physics_base_compressed& comp = event_base.base_a.comp;
    bits.set_next_bits(DET_EXPLOSION, 4);
    bits.set_next_bits(comp.cobj_network_handle, 16);
    bits.set_next_bits(comp.ccollision_point_local_r, 10);
    bits.set_next_bits(comp.ccollision_point_local_theta, 9);
    bits.set_next_bits(comp.ccollision_point_local_phi, 9);
    bits.set_next_bits(comp.ccollision_dir_local_theta, 8);
    bits.set_next_bits(comp.ccollision_dir_local_phi, 8);
    bits.set_next_bits(event_base.exp_unique_id & 0xFF, 8);
    bits.set_next_bits(event_base.compressed_exp_radius_multiplier, 8);
    dest_packet_add_bytes(data, sizeof(data));
}

// A mover by game handle (inlined handle check of the process functions)
static rfg_mover* dest_rfg_mover_from_handle(unsigned int handle)
{
    if (handle == 0xFFFFFFFF)
    {
        return nullptr;
    }
    object* objp = World.object_from_handle(handle);
    if (objp == nullptr || objp->get_obj_type() != OT_MOVER || objp->get_sub_type() != OT_SUB_MOVER_RFG)
    {
        return nullptr;
    }
    return (rfg_mover*)objp;
}

// 0x009C3B50 (:2935): the bytes read
static int process_destruction_event_explosion(unsigned char* packet_data)
{
    int packet_offset = 0;
    rfg_collision_event_base event_base;
    memset(&event_base, 0, sizeof(event_base)); // port: rfg.exe reads the decompressed fields of a stack event
    get_decompressed_explosion_info(event_base, packet_data, packet_offset);
    rfg_mover* mover = dest_rfg_mover_from_handle(multi_core_game_handle_from_network_handle(event_base.base_a.comp.cobj_network_handle, false));
    const explosion_info* e_info = explosion_lookup_by_unique_id((int)event_base.exp_unique_id);
    if (mover != nullptr && e_info != nullptr)
    {
        rfg_dyn_add_pre_collide_explosion(mover, nullptr, e_info, event_base.base_a.collision_point_local, event_base.base_a.collision_normal_local, event_base.exp_radius_multiplier, true);
    }
    return packet_offset;
}

// 0x00952FF0 (:2973): 12 bytes
static void add_object_simple_destruction_event_to_destruction_frame(const rfg_collision_event_base& event_base)
{
    unsigned char data[12] = {};
    bit_packer bits(data, sizeof(data));
    const rfg_collision_physics_base& base = event_base.base_a;
    bits.set_next_bits(DET_OBJECT_SIMPLE, 4);
    bits.set_next_bits(base.comp.cobj_network_handle, 16);
    bits.set_next_bits(base.comp.ccollision_point_local_r, 9);
    bits.set_next_bits(base.comp.ccollision_point_local_theta, 9);
    bits.set_next_bits(base.comp.ccollision_point_local_phi, 8);
    bits.set_next_bits(base.comp.ccollision_dir_local_theta, 9);
    bits.set_next_bits(base.comp.ccollision_dir_local_phi, 8);
    bits.set_next_bits(shapekey_pack16(base.shape_key), 16); // inlined
    bits.set_next_bits(base.comp.cdamage, 16);
    bits.set_next_bits((event_base.flags >> 3) & 1, 1);
    dest_packet_add_bytes(data, sizeof(data));
}

// 0x009C3C10 (:3020)
static int process_destruction_event_object_simple(unsigned char* packet_data)
{
    int packet_offset = 0;
    rfg_collision_event_base event_base;
    memset(&event_base, 0, sizeof(event_base));
    get_decompressed_object_simple_info(event_base, packet_data, packet_offset);
    rfg_mover* mover = dest_rfg_mover_from_handle(multi_core_game_handle_from_network_handle(event_base.base_a.comp.cobj_network_handle, false));
    if (mover != nullptr)
    {
        const rfg_collision_physics_base& a = event_base.base_a;
        rfg_dyn_multi_add_pre_collide_contact(mover, nullptr, event_base.damage, a.collision_point_local, a.collision_normal_local, ZERO_VECTOR, ZERO_VECTOR, a.shape_key, 0xFFFFFFFF, ((event_base.flags >> 3) & 1) != 0);
    }
    return packet_offset;
}

// 0x00953340 (:3051): 21 bytes
static void add_object_complex_destruction_event_to_destruction_frame(const rfg_collision_event_base& event_base)
{
    unsigned char data[21] = {};
    bit_packer bits(data, sizeof(data));
    const rfg_collision_physics_base& a = event_base.base_a;
    const rfg_collision_physics_base& b = event_base.base_b;
    bits.set_next_bits(DET_OBJECT_COMPLEX, 4);
    bits.set_next_bits(a.comp.cobj_network_handle, 16);
    bits.set_next_bits(a.comp.ccollision_point_local_r, 9);
    bits.set_next_bits(a.comp.ccollision_point_local_theta, 9);
    bits.set_next_bits(a.comp.ccollision_point_local_phi, 8);
    bits.set_next_bits(a.comp.ccollision_dir_local_theta, 8);
    bits.set_next_bits(a.comp.ccollision_dir_local_phi, 7);
    bits.set_next_bits(shapekey_pack16(a.shape_key), 16); // inlined
    bits.set_next_bits(b.comp.cobj_network_handle, 16);
    bits.set_next_bits(b.comp.ccollision_point_local_r, 9);
    bits.set_next_bits(b.comp.ccollision_point_local_theta, 9);
    bits.set_next_bits(b.comp.ccollision_point_local_phi, 8);
    bits.set_next_bits(b.comp.ccollision_dir_local_theta, 8);
    bits.set_next_bits(b.comp.ccollision_dir_local_phi, 8);
    bits.set_next_bits(shapekey_pack16(b.shape_key), 16); // inlined
    bits.set_next_bits(a.comp.cdamage, 16);
    bits.set_next_bits((event_base.flags >> 3) & 1, 1);
    dest_packet_add_bytes(data, sizeof(data));
}

// 0x009C3CC0 (:3122): both sides must be rfg_movers
static int process_destruction_event_object_complex(unsigned char* packet_data)
{
    int packet_offset = 0;
    rfg_collision_event_base event_base;
    memset(&event_base, 0, sizeof(event_base));
    get_decompressed_object_complex_info(event_base, packet_data, packet_offset);
    rfg_mover* mover_a = dest_rfg_mover_from_handle(multi_core_game_handle_from_network_handle(event_base.base_a.comp.cobj_network_handle, false));
    rfg_mover* mover_b = dest_rfg_mover_from_handle(multi_core_game_handle_from_network_handle(event_base.base_b.comp.cobj_network_handle, false));
    if (mover_a != nullptr && mover_b != nullptr)
    {
        const rfg_collision_physics_base& a = event_base.base_a;
        const rfg_collision_physics_base& b = event_base.base_b;
        rfg_dyn_multi_add_pre_collide_contact(mover_a, mover_b, event_base.damage, a.collision_point_local, a.collision_normal_local, b.collision_point_local, b.collision_normal_local, a.shape_key, b.shape_key, ((event_base.flags >> 3) & 1) != 0);
    }
    return packet_offset;
}

// 0x009537C0 (:3164): 9 bytes, then 4 per piece
static void add_direct_destruction_event_to_destruction_frame(const rfg_collision_event_base& event_base)
{
    unsigned char data[37] = {}; // Magic number: 9 + 7 pieces * 4 bytes (0x128 bits)
    bit_packer bits(data, sizeof(data));
    const rfg_collision_physics_base_compressed& comp = event_base.base_a.comp;
    const int num_bytes = event_base.data_count * 4 + 9;
    bits.set_next_bits(DET_DIRECT_DAMAGE, 4);
    bits.set_next_bits(comp.cobj_network_handle, 16);
    bits.set_next_bits(comp.ccollision_point_local_r, 10);
    bits.set_next_bits(comp.ccollision_point_local_theta, 10);
    bits.set_next_bits(comp.ccollision_point_local_phi, 10);
    bits.set_next_bits(comp.ccollision_dir_local_theta, 10);
    bits.set_next_bits(comp.ccollision_dir_local_phi, 9);
    bits.set_next_bits(event_base.data_count, 3);
    for (int i = 0; i < event_base.data_count; i++)
    {
        const comp_sp_combined_direct_damage& sp = event_base.get_combined_direct_damage(i);
        bits.set_next_bits(sp.compressed_shape_key, 16);
        bits.set_next_bits(sp.compressed_damage, 16);
    }
    dest_packet_add_bytes(data, num_bytes);
}

// 0x00953AB0 (:3213): 3 bytes, then 2 per broken link
static void add_link_broken_event_to_destruction_frame(const rfg_collision_event_base& event_base)
{
    unsigned char data[17] = {}; // Magic number: 3 + 7 links * 2 bytes (0x88 bits)
    bit_packer bits(data, sizeof(data));
    const int num_bytes = event_base.data_count * 2 + 3;
    bits.set_next_bits(DET_LINK_BROKEN, 4);
    bits.set_next_bits(event_base.base_a.comp.cobj_network_handle, 16);
    bits.set_next_bits((event_base.flags >> 5) & 1, 1);
    bits.set_next_bits(event_base.data_count, 3);
    for (int i = 0; i < event_base.data_count; i++)
    {
        bits.set_next_bits(shapekey_pack16(event_base.get_link_index(i)), 16); // -1 is 0xFFFF (inlined)
    }
    dest_packet_add_bytes(data, num_bytes);
}

// 0x00953C90 (:3252): 3 bytes, then 2 per shape key
static void add_sp_array_destruction_event_to_destruction_frame(const rfg_collision_event_base& event_base)
{
    unsigned char data[33] = {}; // Magic number: 3 + 15 keys * 2 bytes (0x108 bits)
    bit_packer bits(data, sizeof(data));
    const int num_bytes = event_base.data_count * 2 + 3;
    bits.set_next_bits(DET_SP_ARRAY, 4);
    bits.set_next_bits(event_base.base_a.comp.cobj_network_handle, 16);
    bits.set_next_bits(event_base.data_count, 4);
    for (int i = 0; i < event_base.data_count; i++)
    {
        bits.set_next_bits(event_base.get_direct_sps(i), 16);
    }
    dest_packet_add_bytes(data, num_bytes);
}

// 0x009C3DD0 (:3285)
static int process_destruction_event_direct(unsigned char* packet_data)
{
    int packet_offset = 0;
    rfg_collision_event_base event_base;
    memset(&event_base, 0, sizeof(event_base));
    get_decompressed_direct_damage_info(event_base, packet_data, packet_offset);
    rfg_mover* mover = dest_rfg_mover_from_handle(multi_core_game_handle_from_network_handle(event_base.base_a.comp.cobj_network_handle, false));
    if (mover != nullptr)
    {
        const int sp_count = event_base.data_count;
        sp_combined_direct_damage_raw combined_sps[7];
        for (int i = 0; i < sp_count; i++)
        {
            const comp_sp_combined_direct_damage& sp = event_base.get_combined_direct_damage(i);
            combined_sps[i].damage = int_unpack16(sp.compressed_damage); // inlined
            combined_sps[i].shape_key = shapekey_unpack16(sp.compressed_shape_key); // inlined
        }
        rfg_dyn_add_pre_collide_direct_damage(mover, nullptr, sp_count, combined_sps, event_base.base_a.collision_point_local, event_base.base_a.collision_normal_local, true);
    }
    return packet_offset;
}

// 0x009C3ED0 (:3320)
static int process_destruction_event_link_broken(unsigned char* packet_data)
{
    int packet_offset = 0;
    rfg_collision_event_base event_base;
    memset(&event_base, 0, sizeof(event_base));
    get_decompressed_link_broken_info(event_base, packet_data, packet_offset);
    rfg_mover* mover = dest_rfg_mover_from_handle(multi_core_game_handle_from_network_handle(event_base.base_a.comp.cobj_network_handle, false));
    if (mover != nullptr)
    {
        rfg_dyn_add_pre_collide_link_broken(mover, event_base.data_count, event_base.get_link_indices(), event_base.flags, true);
    }
    return packet_offset;
}

// 0x00953E40 (:3343): the piece count is 3 bits at bit 69
static int direct_damage_event_get_size_bytes(unsigned char* packet_data)
{
    unsigned char header[9];
    memcpy(header, packet_data, sizeof(header));
    bit_packer bits(header, sizeof(header));
    bits.set_start_bit(69); // Magic number: 4 + 16 + 4 * 10 + 9 header bits
    return (int)(bits.get_next_bits(3) & 0x7FFFFFF) * 4 + 9;
}

// 0x00953F20 (:3360): the link count is 3 bits at bit 21
// LTCG: packet_data in EAX (the port has no register convention).
static int link_broken_event_get_size_bytes(unsigned char* packet_data)
{
    unsigned char header[3];
    memcpy(header, packet_data, sizeof(header));
    bit_packer bits(header, sizeof(header));
    bits.set_start_bit(21); // Magic number: 4 + 16 + 1 header bits
    return (int)bits.get_next_bits(3) * 2 + 3;
}

// 0x00953FB0 (:3377): the key count is 4 bits at bit 20
// LTCG: packet_data in EAX (the port has no register convention).
static int sp_array_event_get_size_bytes(unsigned char* packet_data)
{
    unsigned char header[3];
    memcpy(header, packet_data, sizeof(header));
    bit_packer bits(header, sizeof(header));
    bits.set_start_bit(20); // Magic number: 4 + 16 header bits
    return (int)(bits.get_next_bits(4) & 0xFFFFFFF) * 2 + 3;
}

// 0x00954040 (:3394): the handle count is the high nibble
// LTCG: packet_data in EAX (the port has no register convention).
static int destruction_mode_event_get_size_bytes(unsigned char* packet_data)
{
    return ((int)(packet_data[0] & 0xFFFFFFF0) >> 3) + 1;
}

// The bytes of one event of a frame (the switch that get_destruction_frame_num_bytes, the frame data lookup and the
// repair check inline); an unknown type is 0 bytes.
static int dest_event_get_size_bytes(unsigned char* packet_data)
{
    switch (packet_data[0] & 0xF)
    {
    case DET_EXPLOSION:
        return 10;
    case DET_DIRECT_DAMAGE:
        return direct_damage_event_get_size_bytes(packet_data);
    case DET_SP_ARRAY:
        return sp_array_event_get_size_bytes(packet_data);
    case DET_OBJECT_SIMPLE:
    case DET_SHATTER:
        return 12;
    case DET_OBJECT_COMPLEX:
        return 21;
    case DET_SHAPE_CUTTER:
        return 8;
    case DET_REPAIR:
    case DET_SLICE:
        return 7;
    case DET_MP_DESTROY:
        return 3;
    case DET_MP_VEHICLE:
        return 14;
    case DET_SHARDIFY:
        return 1;
    case DET_DESTRUCTION_MODE:
        return destruction_mode_event_get_size_bytes(packet_data); // inlined
    case DET_LINK_BROKEN:
        return link_broken_event_get_size_bytes(packet_data);
    default:
        return 0;
    }
}

// 0x009C3F70 (:3406)
// Kept: the keys are widened without turning 0xFFFF back into -1.
static int process_destruction_event_sp_array(unsigned char* packet_data)
{
    int packet_offset = 0;
    rfg_collision_event_base event_base;
    memset(&event_base, 0, sizeof(event_base));
    get_decompressed_sp_array_info(event_base, packet_data, packet_offset);
    rfg_mover* mover = dest_rfg_mover_from_handle(multi_core_game_handle_from_network_handle(event_base.base_a.comp.cobj_network_handle, false));
    const int num_keys = event_base.data_count;
    unsigned int shape_keys[15];
    for (int i = 0; i < num_keys; i++)
    {
        shape_keys[i] = event_base.get_direct_sps(i);
    }
    if (mover != nullptr)
    {
        rfg_dyn_add_pre_collide_sp_array(mover, nullptr, shape_keys, num_keys, true);
    }
    return packet_offset;
}

// 0x00954060 (:3438): 12 bytes
static void add_shatter_destruction_event_to_destruction_frame(const rfg_collision_event_base& event_base)
{
    unsigned char data[12] = {};
    bit_packer bits(data, sizeof(data));
    const rfg_collision_physics_base& base = event_base.base_a;
    bits.set_next_bits(DET_SHATTER, 4);
    bits.set_next_bits(base.comp.cobj_network_handle, 16);
    bits.set_next_bits(base.comp.ccollision_point_local_r, 9);
    bits.set_next_bits(base.comp.ccollision_point_local_theta, 9);
    bits.set_next_bits(base.comp.ccollision_point_local_phi, 9);
    bits.set_next_bits(base.comp.ccollision_dir_local_theta, 9);
    bits.set_next_bits(base.comp.ccollision_dir_local_phi, 8);
    bits.set_next_bits(shapekey_pack16(base.shape_key), 16); // inlined
    bits.set_next_bits(base.comp.cdamage, 16);
    dest_packet_add_bytes(data, sizeof(data));
}

// 0x009C4040 (:3477)
static int process_destruction_event_shatter(unsigned char* packet_data)
{
    int packet_offset = 0;
    rfg_collision_event_base event_base;
    memset(&event_base, 0, sizeof(event_base));
    get_decompressed_shatter_info(event_base, packet_data, packet_offset);
    rfg_mover* mover = dest_rfg_mover_from_handle(multi_core_game_handle_from_network_handle(event_base.base_a.comp.cobj_network_handle, false));
    if (mover != nullptr)
    {
        rfg_dyn_add_pre_collide_shatter(mover, nullptr, event_base.base_a.shape_key, event_base.damage, event_base.base_a.collision_point_local, event_base.base_a.collision_normal_local, true);
    }
    return packet_offset;
}

// 0x00954380 (:3504): 8 bytes
static void add_shape_cutter_destruction_event_to_destruction_frame(const rfg_collision_event_base& event_base)
{
    unsigned char data[8] = {};
    bit_packer bits(data, sizeof(data));
    const rfg_collision_physics_base_compressed& comp = event_base.base_a.comp;
    bits.set_next_bits(DET_SHAPE_CUTTER, 4);
    bits.set_next_bits(comp.cobj_network_handle, 16);
    bits.set_next_bits(comp.ccollision_point_local_r, 8);
    bits.set_next_bits(comp.ccollision_point_local_theta, 7);
    bits.set_next_bits(comp.ccollision_point_local_phi, 7);
    bits.set_next_bits(comp.cdamage, 14);
    bits.set_next_bits(event_base.exp_unique_id & 0xFF, 8);
    dest_packet_add_bytes(data, sizeof(data));
}

// 0x009C40E0 (:3540)
static int process_destruction_event_shape_cutter(unsigned char* packet_data)
{
    int packet_offset = 0;
    rfg_collision_event_base event_base;
    memset(&event_base, 0, sizeof(event_base));
    get_decompressed_shape_cutter_info(event_base, packet_data, packet_offset);
    rfg_mover* mover = dest_rfg_mover_from_handle(multi_core_game_handle_from_network_handle(event_base.base_a.comp.cobj_network_handle, false));
    if (mover != nullptr)
    {
        rfg_dyn_add_pre_collide_shape_cutter(mover, nullptr, event_base.radius, event_base.base_a.collision_point_local, (unsigned char)event_base.exp_unique_id, true);
    }
    return packet_offset;
}

// 0x00954640 (:3566): 7 bytes
static void add_repair_destruction_event_to_destruction_frame(const rfg_collision_event_base& event_base)
{
    unsigned char data[7] = {};
    bit_packer bits(data, sizeof(data));
    const rfg_collision_physics_base_compressed& comp = event_base.base_a.comp;
    bits.set_next_bits(DET_REPAIR, 4);
    bits.set_next_bits(comp.cobj_network_handle, 16);
    bits.set_next_bits(comp.ccollision_point_local_r, 8);
    bits.set_next_bits(comp.ccollision_point_local_theta, 7);
    bits.set_next_bits(comp.ccollision_point_local_phi, 7);
    bits.set_next_bits(comp.cdamage, 9);
    // In 100 ms steps (the working data holds the duration in ms again after the round trip)
    bits.set_next_bits(event_base.repair_duration / 100 & 0xFF, 5);
    dest_packet_add_bytes(data, sizeof(data));
}

// 0x009C4180 (:3603)
static int process_destruction_event_repair(unsigned char* packet_data)
{
    int packet_offset = 0;
    rfg_collision_event_base event_base;
    memset(&event_base, 0, sizeof(event_base));
    get_decompressed_repair_info(event_base, packet_data, packet_offset);
    rfg_mover* mover = dest_rfg_mover_from_handle(multi_core_game_handle_from_network_handle(event_base.base_a.comp.cobj_network_handle, false));
    if (mover != nullptr)
    {
        rfg_dyn_add_pre_collide_repair(mover, nullptr, event_base.base_a.collision_point_local, event_base.radius, (int)event_base.repair_duration, true);
    }
    return packet_offset;
}

// 0x009548F0 (:3630): 3 bytes (4 zero bits after the type)
static void add_mp_destroy_destruction_event_to_destruction_frame(const rfg_collision_event_base& event_base)
{
    unsigned char data[3] = {};
    bit_packer bits(data, sizeof(data));
    bits.set_next_bits(DET_MP_DESTROY, 4);
    bits.set_next_bits(0, 4);
    bits.set_next_bits(event_base.base_a.comp.cobj_network_handle, 16);
    dest_packet_add_bytes(data, sizeof(data));
}

// 0x00954A30 (:3656): 14 bytes
static void add_mp_veh_destruction_event_to_destruction_frame(const rfg_collision_event_base& event_base)
{
    unsigned char data[14] = {};
    bit_packer bits(data, sizeof(data));
    const rfg_collision_physics_base_compressed& comp = event_base.base_a.comp;
    const rfg_collision_physics_base_compressed& angles = event_base.base_b.comp;
    bits.set_next_bits(DET_MP_VEHICLE, 4);
    bits.set_next_bits(comp.cobj_network_handle, 16);
    bits.set_next_bits(comp.ccollision_point_local_r, 9);
    bits.set_next_bits(comp.ccollision_point_local_theta, 8);
    bits.set_next_bits(comp.ccollision_point_local_phi, 8);
    bits.set_next_bits(comp.ccollision_dir_local_r, 8);
    bits.set_next_bits(comp.ccollision_dir_local_theta, 8);
    bits.set_next_bits(comp.ccollision_dir_local_phi, 8);
    bits.set_next_bits(angles.ccollision_point_local_r, 9);
    bits.set_next_bits(angles.ccollision_point_local_theta, 9);
    bits.set_next_bits(angles.ccollision_point_local_phi, 9);
    bits.set_next_bits(comp.cdamage, 16);
    dest_packet_add_bytes(data, sizeof(data));
}

// 0x00954D60 (:3705): 7 bytes
static void add_slice_to_destruction_frame(const rfg_collision_event_base& event_base)
{
    unsigned char data[7] = {};
    bit_packer bits(data, sizeof(data));
    const rfg_collision_physics_base_compressed& comp = event_base.base_a.comp;
    bits.set_next_bits(DET_SLICE, 4);
    bits.set_next_bits(comp.cobj_network_handle, 16);
    bits.set_next_bits(comp.ccollision_point_local_r, 8);
    bits.set_next_bits(comp.ccollision_point_local_theta, 8);
    bits.set_next_bits(comp.ccollision_point_local_phi, 8);
    bits.set_next_bits(event_base.get_slice_index(), 2);
    bits.set_next_bits(event_base.base_b.comp.ccollision_point_local_r, 5);
    bits.set_next_bits(event_base.base_b.comp.ccollision_point_local_theta, 5);
    dest_packet_add_bytes(data, sizeof(data));
}

// 0x00955040 (:3751): the type byte only
static void add_mp_shardify_to_destruction_frame(const rfg_collision_event_base& event_base)
{
    Destruction_packet._packet_data[Destruction_packet._packet_size] = DET_SHARDIFY;
    Destruction_packet._packet_size++;
}

// 0x009C4220 (:3759)
static void add_destruction_mode_to_destruction_frame(const rfg_collision_event_base& event_base)
{
    Destruction_packet._packet_data[Destruction_packet._packet_size] = (unsigned char)(event_base.data_count << 4 | DET_DESTRUCTION_MODE);
    Destruction_packet._packet_size++;
    const unsigned int* handles = event_base.get_invulnerable_mover_handles();
    for (int i = 0; i < event_base.data_count; i++)
    {
        const unsigned short net_handle = multi_core_network_handle_from_game_handle(handles[i], false); // inlined
        Destruction_packet._packet_data[Destruction_packet._packet_size] = (unsigned char)(net_handle >> 8);
        Destruction_packet._packet_data[Destruction_packet._packet_size + 1] = (unsigned char)net_handle;
        Destruction_packet._packet_size += 2;
    }
}

// 0x009C4330 (:3775)
static int process_destruction_event_mp_destroy(unsigned char* packet_data)
{
    int packet_offset = 0;
    rfg_collision_event_base event_base;
    memset(&event_base, 0, sizeof(event_base));
    get_decompressed_mp_destroy_info(event_base, packet_data, packet_offset);
    rfg_mover* mover = dest_rfg_mover_from_handle(multi_core_game_handle_from_network_handle(event_base.base_a.comp.cobj_network_handle, false));
    if (mover != nullptr)
    {
        rfg_dyn_add_pre_collide_mp_destroy(mover, nullptr, true);
    }
    return packet_offset;
}

// 0x009C43B0 (:3798): the box orientation is made from side b's angles
static int process_destruction_event_mp_veh(unsigned char* packet_data)
{
    int packet_offset = 0;
    rfg_collision_event_base event_base;
    memset(&event_base, 0, sizeof(event_base));
    get_decompressed_mp_veh_info(event_base, packet_data, packet_offset);
    rfg_mover* mover = dest_rfg_mover_from_handle(multi_core_game_handle_from_network_handle(event_base.base_a.comp.cobj_network_handle, false));
    if (mover != nullptr)
    {
        const vector& angles = event_base.base_b.collision_point_local;
        const matrix box_orient(angles.x, angles.y, angles.z);
        rfg_dyn_add_pre_collide_mp_vehicle(mover, nullptr, event_base.damage, event_base.base_a.collision_point_local, event_base.base_a.collision_normal_local, box_orient, true);
    }
    return packet_offset;
}

// 0x00998840 (:3831): the type byte
static int process_destruction_event_shardify(unsigned char* packet_data)
{
    rfg_dyn_add_pre_collide_shardify(true);
    return 1;
}

// 0x009C4490 (:3846)
static int process_destruction_event_destruction_mode(unsigned char* packet_data)
{
    int packet_offset = 0;
    rfg_collision_event_base event_base;
    memset(&event_base, 0, sizeof(event_base));
    get_decompressed_destruction_mode_info(event_base, packet_data, packet_offset);
    rfg_dyn_add_pre_collide_destruction_mode(event_base.get_invulnerable_mover_handles(), event_base.data_count, true);
    return packet_offset;
}

// 0x009C4500 (:3862)
static int process_destruction_event_slice(unsigned char* packet_data)
{
    int packet_offset = 0;
    rfg_collision_event_base event_base;
    memset(&event_base, 0, sizeof(event_base));
    get_decompressed_slice_info(event_base, packet_data, packet_offset);
    rfg_mover* mover = dest_rfg_mover_from_handle(multi_core_game_handle_from_network_handle(event_base.base_a.comp.cobj_network_handle, false));
    if (mover != nullptr)
    {
        const vector& angles = event_base.base_b.collision_point_local;
        rfg_dyn_add_pre_collide_slice(mover, event_base.base_a.collision_point_local, (int)event_base.get_slice_index(), angles.x, angles.y, true);
    }
    return packet_offset;
}

// 0x009C45B0 (:3886)
// LTCG: event_base in EAX (the port has no register convention).
static void add_destruction_event_to_destruction_frame(const rfg_collision_event_base& event_base)
{
    switch (event_base.destruction_type)
    {
    case DET_EXPLOSION:
        add_explosion_destruction_event_to_destruction_frame(event_base);
        break;
    case DET_DIRECT_DAMAGE:
        add_direct_destruction_event_to_destruction_frame(event_base);
        break;
    case DET_SP_ARRAY:
        add_sp_array_destruction_event_to_destruction_frame(event_base);
        break;
    case DET_OBJECT_SIMPLE:
        add_object_simple_destruction_event_to_destruction_frame(event_base);
        break;
    case DET_OBJECT_COMPLEX:
        add_object_complex_destruction_event_to_destruction_frame(event_base);
        break;
    case DET_SHATTER:
        add_shatter_destruction_event_to_destruction_frame(event_base);
        break;
    case DET_SHAPE_CUTTER:
        add_shape_cutter_destruction_event_to_destruction_frame(event_base);
        break;
    case DET_REPAIR:
        add_repair_destruction_event_to_destruction_frame(event_base);
        break;
    case DET_MP_DESTROY:
        add_mp_destroy_destruction_event_to_destruction_frame(event_base);
        break;
    case DET_MP_VEHICLE:
        add_mp_veh_destruction_event_to_destruction_frame(event_base);
        break;
    case DET_SHARDIFY:
        add_mp_shardify_to_destruction_frame(event_base); // inlined
        break;
    case DET_DESTRUCTION_MODE:
        add_destruction_mode_to_destruction_frame(event_base);
        break;
    case DET_LINK_BROKEN:
        add_link_broken_event_to_destruction_frame(event_base);
        break;
    case DET_SLICE:
        add_slice_to_destruction_frame(event_base);
        break;
    default:
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n");
        break;
    }
}

// ---- the frame header
// 0x00952AF0 (:770): the frame number (big endian), then 3 bytes of counts, low bit first: the events (3 bits), the
// general movers created, the rfg_movers created, the rfg_movers destroyed, the shardify delete counts (5 bits each)
// and the migration flag.
// LTCG: destruction_frame in EAX, packet_data in ECX (the port has no register convention).
static void extract_destruction_frame_header(unsigned char* packet_data, unsigned short& destruction_frame, unsigned char& num_destruction_events, unsigned char& num_general_movers_created, unsigned char& num_rfg_movers_created, unsigned char& num_rfg_movers_destroyed, unsigned char& num_shardify_delete_counts, unsigned char& migration)
{
    destruction_frame = (unsigned short)(packet_data[0] << 8 | packet_data[1]);
    unsigned char header[3];
    memcpy(header, packet_data + 2, sizeof(header));
    bit_packer bits(header, sizeof(header));
    num_destruction_events = (unsigned char)bits.get_next_bits(3);
    num_general_movers_created = (unsigned char)bits.get_next_bits(5);
    num_rfg_movers_created = (unsigned char)bits.get_next_bits(5);
    num_rfg_movers_destroyed = (unsigned char)bits.get_next_bits(5);
    num_shardify_delete_counts = (unsigned char)bits.get_next_bits(5);
    migration = (unsigned char)bits.get_next_bits(1);
}
#endif

#ifndef OPENRFG_MULTI_DESTRUCTION_ALONE
// ---- console commands (not registered in the shipped build)
// 0x009BDE60 (:396)
void unused_dcf_rfgm_net_handle()
{
    console_get_arg(0x20, 0); // Magic number: an integer argument
    const unsigned int handle = multi_core_game_handle_from_network_handle((unsigned short)Console_arg_int, true);
    if (handle != 0xFFFFFFFF)
    {
        World.object_from_handle(handle); // the result is not used
    }
}

// 0x009BDE90 (:409)
void unused_dcf_rfgm_game_handle()
{
    console_get_arg(0x20, 0); // Magic number: an integer argument
    const int game_handle = Console_arg_int;
    if (game_handle == -1)
    {
        return;
    }
    object* objp = World.object_from_handle(game_handle);
    if (objp != nullptr && objp->get_obj_type() == OT_MOVER && objp->get_sub_type() == OT_SUB_MOVER_RFG)
    {
        multi_core_network_handle_from_game_handle(game_handle, true); // the result is not used
    }
}

// 0x00998710 (:423): walks the rfg_movers and prints nothing (the shipped build stripped the output)
void unused_dcf_rfgm_destruction_uid()
{
    console_get_arg(8, 0); // Magic number: a string argument
}

// 0x00952940 (:437)
void unused_dcf_destruction_status()
{
    if (!Multiplayer)
    {
        return;
    }
    if (Server)
    {
        rfg_dyn_get_active_destruction_frame();
        console_printf("server destruction_frame: %d"); // Kept: the frame is not passed
        return;
    }
    unsigned int first_avail = 0xFFFFFFFF;
    unsigned int last_avail = 0xFFFFFFFF;
    if (MP_destruction_frame_used_list != nullptr)
    {
        first_avail = MP_destruction_frame_used_list->packet_data.destruction_frame;
        last_avail = MP_destruction_frame_used_list->prev->packet_data.destruction_frame;
    }
    char buf[1024];
    sprintf(buf, "client next_destruction_frame: %d, last_avail: %d, backlog: %d, first_avail: %d, used: %d, free: %d", Playback_next_client_destruction_frame, last_avail, Playback_next_client_destruction_frame - last_avail - 1, first_avail, Num_used_destruction_frames, Num_free_destruction_frames);
    console_printf(buf);
}

// 0x00952930: the effect starts with an invalid timestamp
vrm_effect_info::vrm_effect_info()
{
}

// 0x0097B580 (:474): the frame pool (500 frames, 0x57670 bytes) and the 360 entry event history from the multiplayer
// allocators (multi_game_init inlines the same)
void multi_game_destruction_init()
{
    MP_destruction_frame_pool = (destruction_frame_data*)MP_allocators_mempool.alloc(0x57670, 0);
    Destruction_event_history = (unsigned int*)MP_allocators_mempool.alloc(360 * sizeof(unsigned int), 0);
    memset(Destruction_event_history, 0, 360 * sizeof(unsigned int));
    stress_evaluation_set_mode("Multi");
    jip_destruction_sync_state = JIP_DESTRUCTION_STATE_NONE;
    jip_destruction_sync_possible = true;
}

// 0x00952A30 (:499)
void multi_game_destruction_shutdown()
{
    MP_destruction_frame_pool = nullptr;
    Destruction_event_history = nullptr;
    stress_evaluation_set_mode("");
}

// 0x00998740 (:557): the mover's own pieces, the anchor strength of its anchored pieces, the strength of its own
// pieces' links and of the dynamic links of all its pieces
// LTCG: handle in EAX (the port has no register convention).
static void get_rfg_mover_subpiece_info(unsigned int handle, unsigned short& num_subieces, int& anchor_str, int& link_str, int& dyn_link_str)
{
    num_subieces = 0;
    anchor_str = 0;
    link_str = 0;
    dyn_link_str = 0;
    rfg_mover* mover = dest_rfg_mover_from_handle(handle);
    if (mover == nullptr)
    {
        return; // port: rfg.exe dereferences the null mover
    }
    rfg_destroyable_shape* shape = mover->get_shape();
    const int num_objects = shape->parent->num_objects;
    for (int i = 0; i < num_objects; i++)
    {
        rfg_subpiece* sp = shape->get_subpiece(i);
        if ((sp->flags & 1) != 0 && 0 < sp->anchor_yield) // Magic number: flag 0x1, the anchored pieces
        {
            anchor_str += sp->anchor_yield;
        }
        const rfg_dynamic_link* dyn_link = rfg_mover_get_dynamic_link(sp);
        if (dyn_link != nullptr)
        {
            dyn_link_str += dyn_link->yield_limit;
        }
        if (sp->get_destroyable() == shape)
        {
            num_subieces++;
            for (int j = 0; j < sp->num_links; j++)
            {
                link_str += shape->get_link(sp->links[j])->yield_limit;
            }
        }
    }
}

// 0x00952AE0 (:593): empty in the shipped build
static void init_destruction_validate_sync()
{
}

// 0x00955060 (:3959)
void multi_game_destruction_frame_clear()
{
    Destruction_packet.in_use = false;
}

// 0x00955070 (:3966)
bool multi_game_destruction_frame_in_use()
{
    return Destruction_packet.in_use;
}

// 0x00955080 (:3977): the packet header, the frame number and three zero bytes for the counts
void multi_game_destruction_frame_server_start_packet(unsigned short destruction_frame)
{
    Destruction_packet.in_use = true;
    Destruction_packet.num_destruction_events = 0;
    Destruction_packet.num_general_movers_created = 0;
    Destruction_packet.num_rfg_movers_created = 0;
    Destruction_packet.num_rfg_movers_destroyed = 0;
    Destruction_packet.num_shardify_delete_counts = 0;
    Destruction_packet.destruction_frame = destruction_frame;
    Destruction_packet._packet_data[0] = PT_DESTRUCTION_FRAME;
    Destruction_packet._packet_size = (unsigned short)multi_core_object_packet_header_size(PT_DESTRUCTION_FRAME); // inlined
    Destruction_packet._packet_data[Destruction_packet._packet_size] = (unsigned char)(destruction_frame >> 8);
    Destruction_packet._packet_data[Destruction_packet._packet_size + 1] = (unsigned char)destruction_frame;
    Destruction_packet._packet_size += 2;
    for (int i = 0; i < 3; i++)
    {
        Destruction_packet._packet_data[Destruction_packet._packet_size] = 0;
        Destruction_packet._packet_size++;
    }
}

// 0x009F0850 (:4004): the frame's packet starts with its first content; past 400 frames join in progress ends
// (multi_game_destruction_update_jip_status inlined)
static void prepare_destruction_frame()
{
    multi_game_destruction_update_jip_status(false);
    if (!Destruction_packet.in_use)
    {
        multi_game_destruction_frame_server_start_packet(rfg_dyn_get_next_destruction_frame());
    }
}

// 0x009F08F0 (:4017)
void multi_game_destruction_frame_server_add_destruction_event(const rfg_collision_event_base& event_base)
{
    prepare_destruction_frame();
    add_destruction_event_to_destruction_frame(event_base);
    Destruction_packet.num_destruction_events++;
}

// 0x009F0910 (:4040): each new general mover gets the next network handle (2 bytes, big endian) and sends its
// reference position index (1 byte); the server makes its remote object
void multi_game_destruction_frame_server_add_create_general_movers(general_mover** gmp, int count)
{
    prepare_destruction_frame();
    for (int i = 0; i < count; i++)
    {
        const unsigned short net_handle = Cur_network_handle;
        multi_core_set_next_network_handle();
        Destruction_packet._packet_data[Destruction_packet._packet_size] = (unsigned char)(net_handle >> 8);
        Destruction_packet._packet_data[Destruction_packet._packet_size + 1] = (unsigned char)net_handle;
        Destruction_packet._packet_size += 2;
        Destruction_packet._packet_data[Destruction_packet._packet_size] = (unsigned char)gmp[i]->reference_position_index;
        Destruction_packet._packet_size++;
        multi_core_create_remote_object_core_internal(gmp[i]->get_handle(), net_handle, MULTI_OBJECT_TYPE_GEN_MOVER); // inlined
    }
    Destruction_packet.num_general_movers_created = (unsigned char)count;
}

// 0x009F0A50 (:4065): the same for the new rfg_movers
void multi_game_destruction_frame_server_add_create_rfg_movers(rfg_mover** rmp, int count)
{
    prepare_destruction_frame();
    for (int i = 0; i < count; i++)
    {
        const unsigned short net_handle = Cur_network_handle;
        multi_core_set_next_network_handle();
        Destruction_packet._packet_data[Destruction_packet._packet_size] = (unsigned char)(net_handle >> 8);
        Destruction_packet._packet_data[Destruction_packet._packet_size + 1] = (unsigned char)net_handle;
        Destruction_packet._packet_size += 2;
        Destruction_packet._packet_data[Destruction_packet._packet_size] = (unsigned char)rmp[i]->reference_position_index;
        Destruction_packet._packet_size++;
        multi_core_create_remote_object_core_internal(rmp[i]->get_handle(), net_handle, MULTI_OBJECT_TYPE_RFG_MOVER); // inlined
    }
    Destruction_packet.num_rfg_movers_created = (unsigned char)count;
}

// 0x009F0B90 (:4090): the network handles of the destroyed rfg_movers (2 bytes each, big endian)
void multi_game_destruction_frame_server_add_destroyed_rfg_movers(rfg_mover** rmp, int count)
{
    prepare_destruction_frame();
    for (int i = 0; i < count; i++)
    {
        const unsigned short net_handle = multi_core_network_handle_from_game_handle(rmp[i]->get_handle(), false); // inlined
        Destruction_packet._packet_data[Destruction_packet._packet_size] = (unsigned char)(net_handle >> 8);
        Destruction_packet._packet_data[Destruction_packet._packet_size + 1] = (unsigned char)net_handle;
        Destruction_packet._packet_size += 2;
    }
    Destruction_packet.num_rfg_movers_destroyed = (unsigned char)count;
}

// 0x009F0C70 (:4111): per record the mover's network handle (16 bits) and its delete count (12 bits), packed low bit
// first and padded to whole bytes
void multi_game_destruction_frame_server_add_shardify_delete_counts(shardify_delete_record* delete_count_list, int count)
{
    prepare_destruction_frame();
    Destruction_packet.num_shardify_delete_counts = (unsigned char)count;
    if (count < 1)
    {
        return;
    }
    unsigned char data[72] = {}; // Magic number: 20 records * 28 bits (0x230 bits) and padding
    bit_packer bits(data, 70);
    for (int i = 0; i < count; i++)
    {
        bits.set_next_bits(multi_core_network_handle_from_game_handle(delete_count_list[i].mover->get_handle(), false), 16); // inlined
        bits.set_next_bits(delete_count_list[i].delete_count, 12);
    }
    const int num_bytes = ((int)bits.get_start_bit() + 7) / 8;
    dest_packet_add_bytes(data, num_bytes);
}

// 0x009C46B0 (:4150): the counts into the header, the size into the packet header; the server applies the frame
// itself (its own frame list, for join in progress) and sends it reliably to every client that is not joining
void multi_game_destruction_frame_server_finalize_packet()
{
    Destruction_packet.migration = Migration_state != MIGRATION_STATE_NONE;
    unsigned char header[3] = {};
    bit_packer bits(header, sizeof(header));
    // Kept: the event count has 3 bits (a frame with more than 7 events sends its count modulo 8).
    bits.set_next_bits(Destruction_packet.num_destruction_events, 3);
    bits.set_next_bits(Destruction_packet.num_general_movers_created, 5);
    bits.set_next_bits(Destruction_packet.num_rfg_movers_created, 5);
    bits.set_next_bits(Destruction_packet.num_rfg_movers_destroyed, 5);
    bits.set_next_bits(Destruction_packet.num_shardify_delete_counts, 5);
    bits.set_next_bits(Destruction_packet.migration & 1, 1);
    // Kept: the counts go to bytes 5-7, after a 3 byte packet header (a grouped packet type would overwrite its frame
    // number; PT_DESTRUCTION_FRAME is not grouped).
    Destruction_packet._packet_data[5] = header[0];
    Destruction_packet._packet_data[6] = header[1];
    Destruction_packet._packet_data[7] = header[2];
    multi_core_object_packet_set_size(Destruction_packet._packet_data, Destruction_packet._packet_size); // inlined
    const sender from((multi_connection*)nullptr);
    multi_game_destruction_process_packet(Destruction_packet._packet_data + 3, from);
    if (Multi_game_has_skipped_game_migration)
    {
        return;
    }
    multi_session* sp = multi_session_get_by_type(ST_GAME);
    multi_connection* cp = sp->get_connection_list();
    do
    {
        if (cp == nullptr)
        {
            return;
        }
        if (cp != sp->get_me_connection() && !cp->joining_in_progress)
        {
            multi_io_send_reliable(cp, Destruction_packet._packet_data, Destruction_packet._packet_size, nullptr, nullptr, nullptr);
        }
        cp = cp->next;
    } while (cp != sp->get_connection_list());
}

// 0x00955140 (:4262): the delete count the server recorded for the mover in the frame being played back
int multi_game_destruction_get_client_balance_delete_count(const rfg_mover* rmp)
{
    for (int i = 0; i < Active_destruction_frame_data->num_shardify_delete_count_infos; i++)
    {
        if (Active_destruction_frame_data->shardify_delete_count_infos[i].mover == rmp)
        {
            return Active_destruction_frame_data->shardify_delete_count_infos[i].delete_count;
        }
    }
    return 0;
}

// An element at the tail of a circular list (vlib list macro): the head stays.
static void dest_list_add_tail(destruction_frame_data*& head, destruction_frame_data* item)
{
    if (head == nullptr)
    {
        item->next = item;
        item->prev = item;
        head = item;
        return;
    }
    item->prev = head->prev;
    item->next = head;
    head->prev->next = item;
    head->prev = item;
}

// 0x0097B610 (:4282): all 500 frames free (the pool in order), playback from frame 0
void multi_game_destruction_level_init_pre()
{
    MP_destruction_frame_used_list = nullptr;
    MP_destruction_frame_free_list = nullptr;
    for (int i = 0; i < 500; i++) // Magic number: the frames in the pool (0x57670 / 0x2CC)
    {
        dest_list_add_tail(MP_destruction_frame_free_list, &MP_destruction_frame_pool[i]);
    }
    Num_free_destruction_frames = 500;
    Playback_next_client_destruction_frame = 0;
    Num_used_destruction_frames = 0;
    rfg_dyn_reset_destruction_frame(0);
    JIP_client_to_sync_list.remove_all();
    jip_destruction_sync_state = JIP_DESTRUCTION_STATE_NONE;
    jip_destruction_sync_possible = true;
}

// 0x0097B7C0 (:4318): empty in rfg.exe
void multi_game_destruction_level_shutdown()
{
}

// 0x00955180 (:4324): empty in rfg.exe
void multi_game_destruction_reset_validation()
{
}

// 0x00955190 (:4330): from the used list to the tail of the free list
// LTCG: frame_data in EAX (the port has no register convention).
static void multi_game_destruction_release_destruction_frame(destruction_frame_data* frame_data)
{
    mc_list_remove(MP_destruction_frame_used_list, frame_data, &destruction_frame_data::next, &destruction_frame_data::prev, true);
    dest_list_add_tail(MP_destruction_frame_free_list, frame_data);
    Num_free_destruction_frames++;
    Num_used_destruction_frames--;
}

// 0x00955260 (:4341)
// LTCG: frame_data in EAX (the port has no register convention).
static void multi_game_destruction_add_to_free_list(destruction_frame_data* frame_data)
{
    Num_free_destruction_frames++;
    dest_list_add_tail(MP_destruction_frame_free_list, frame_data);
}

// 0x009552B0 (:4353): 0xFFFF before the first
unsigned short multi_game_destruction_get_last_applied_destruction_frame()
{
    if (Playback_next_client_destruction_frame == 0)
    {
        return 0xFFFF;
    }
    return (unsigned short)(Playback_next_client_destruction_frame - 1);
}

// 0x009552D0 (:4361): the newest frame received (the tail of the used list)
unsigned short multi_game_destruction_get_last_destruction_frame()
{
    if (MP_destruction_frame_used_list == nullptr)
    {
        return 0xFFFF;
    }
    return MP_destruction_frame_used_list->prev->packet_data.destruction_frame;
}

// 0x009552F0 (:4375): the frames from start_cull_frame_index on go back to the free list
void multi_game_destruction_cull_destruction_frames_starting(int start_cull_frame_index)
{
    Destruction_reorder_wrapper.enter();
    destruction_frame_data* frame_data = MP_destruction_frame_used_list;
    while (frame_data != nullptr)
    {
        destruction_frame_data* next = frame_data->next;
        destruction_frame_data* following = next != MP_destruction_frame_used_list ? next : nullptr;
        if (start_cull_frame_index <= (int)frame_data->packet_data.destruction_frame)
        {
            mc_list_remove(MP_destruction_frame_used_list, frame_data, &destruction_frame_data::next, &destruction_frame_data::prev, true);
            dest_list_add_tail(MP_destruction_frame_free_list, frame_data);
            Num_free_destruction_frames++;
            Num_used_destruction_frames--;
        }
        frame_data = following;
    }
    Destruction_reorder_wrapper.leave();
}

// The bytes of a frame: the frame number and header (5), the events, 3 per created mover, 2 per destroyed rfg_mover and
// the shardify delete counts (28 bits each, padded to whole bytes).
static unsigned short dest_frame_num_bytes(unsigned char* packet_data)
{
    unsigned short destruction_frame;
    unsigned char num_destruction_events;
    unsigned char num_general_movers_created;
    unsigned char num_rfg_movers_created;
    unsigned char num_rfg_movers_destroyed;
    unsigned char num_shardify_delete_counts;
    unsigned char migration;
    extract_destruction_frame_header(packet_data, destruction_frame, num_destruction_events, num_general_movers_created, num_rfg_movers_created, num_rfg_movers_destroyed, num_shardify_delete_counts, migration);
    int offset = 5;
    for (unsigned int i = num_destruction_events; i != 0; i--)
    {
        offset += dest_event_get_size_bytes(packet_data + offset);
    }
    return (unsigned short)(num_rfg_movers_destroyed * 2 + (num_general_movers_created + num_rfg_movers_created) * 3 + ((num_shardify_delete_counts * 28 + 7) >> 3) + offset);
}

// 0x00998850 (:4388)
static unsigned short get_destruction_frame_num_bytes(unsigned char* packet_data)
{
    return dest_frame_num_bytes(packet_data);
}

// 0x00998990 (:4487): a stored frame's data and size, or null
unsigned char* multi_game_destruction_get_destruction_frame_data(int destruction_frame, int& num_bytes)
{
    destruction_frame_data* frame_data = MP_destruction_frame_used_list;
    while (true)
    {
        if (frame_data == nullptr)
        {
            return nullptr;
        }
        if (destruction_frame == (int)frame_data->packet_data.destruction_frame)
        {
            break;
        }
        frame_data = frame_data->next;
        if (frame_data == MP_destruction_frame_used_list)
        {
            return nullptr;
        }
    }
    unsigned char* packet_data = frame_data->packet_data._packet_data;
    num_bytes = dest_frame_num_bytes(packet_data); // inlined
    return packet_data;
}

// 0x009553F0 (:4505): the head of the free list; with none free the oldest used frame is dropped first
static destruction_frame_data* multi_game_destruction_get_free_destruction_frame()
{
    destruction_frame_data* frame_data = MP_destruction_frame_free_list;
    if (frame_data == nullptr)
    {
        multi_game_destruction_release_destruction_frame(MP_destruction_frame_used_list);
        frame_data = MP_destruction_frame_free_list;
        if (Multiplayer && !Server && Playback_next_client_destruction_frame <= MP_destruction_frame_free_list->packet_data.destruction_frame)
        {
            keen::debug::nativePrintArgs("destruction_sync_problem_remove ");
        }
        if (frame_data == nullptr)
        {
            return nullptr;
        }
    }
    mc_list_remove(MP_destruction_frame_free_list, frame_data, &destruction_frame_data::next, &destruction_frame_data::prev, true);
    Num_free_destruction_frames--;
    return frame_data;
}

// 0x009554A0 (:4540)
// LTCG: packet_data in EDI (the port has no register convention).
static bool frame_data_contains_repair_event(unsigned char* packet_data, int num_destruction_events)
{
    int offset = 0;
    for (int i = 0; i < num_destruction_events; i++)
    {
        if ((packet_data[offset] & 0xF) == DET_REPAIR)
        {
            return true;
        }
        offset += dest_event_get_size_bytes(packet_data + offset);
    }
    return false;
}

// 0x00955570 (:4623): the header into the frame, whether it repairs, and 512 bytes of data
// Kept: 512 bytes are copied whatever the frame's size (past the end of a shorter packet).
// LTCG: packet_data in EAX, frame_data in ESI (the port has no register convention).
static void get_frame_data(unsigned char* packet_data, destruction_frame_data* frame_data)
{
    _destruction_packet& frame = frame_data->packet_data;
    extract_destruction_frame_header(packet_data, frame.destruction_frame, frame.num_destruction_events, frame.num_general_movers_created, frame.num_rfg_movers_created, frame.num_rfg_movers_destroyed, frame.num_shardify_delete_counts, frame.migration);
    frame_data->contains_repair_event = frame_data_contains_repair_event(packet_data + 5, frame.num_destruction_events);
    memcpy(frame._packet_data, packet_data, sizeof(frame._packet_data));
}

// 0x00955630 (:4673)
unsigned short get_destruction_next_frame_id(unsigned short frame_id)
{
    if (frame_id == 0xFFFF)
    {
        return 0;
    }
    return (unsigned short)(frame_id + 1);
}

// 0x009C4920 (:4679): the frame's events go to rfg_dynamics as events from the server; the offsets of the mover lists
// are noted and the shardify delete counts read. A frame that creates or destroys anything must be processed even
// without events.
// LTCG: frame_data in ECX, multi_client_must_process in EDX (the port has no register convention).
static bool add_destruction_events_from_packet(destruction_frame_data* frame_data, bool& multi_client_must_process)
{
    _destruction_packet& frame = frame_data->packet_data;
    multi_client_must_process = frame.num_destruction_events != 0 || frame.num_general_movers_created != 0 || frame.num_rfg_movers_created != 0 || frame.num_rfg_movers_destroyed != 0;
    Destruction_spew_buf[0] = '\0';
    Spew_to_buffer = false;
    int offset = 5;
    for (int i = 0; i < frame.num_destruction_events; i++)
    {
        unsigned char* event_data = &frame._packet_data[offset];
        int num_bytes;
        switch (event_data[0] & 0xF)
        {
        case DET_EXPLOSION:
            num_bytes = process_destruction_event_explosion(event_data);
            break;
        case DET_DIRECT_DAMAGE:
            num_bytes = process_destruction_event_direct(event_data);
            break;
        case DET_SP_ARRAY:
            num_bytes = process_destruction_event_sp_array(event_data);
            break;
        case DET_OBJECT_SIMPLE:
            num_bytes = process_destruction_event_object_simple(event_data);
            break;
        case DET_OBJECT_COMPLEX:
            num_bytes = process_destruction_event_object_complex(event_data);
            break;
        case DET_SHATTER:
            num_bytes = process_destruction_event_shatter(event_data);
            break;
        case DET_SHAPE_CUTTER:
            num_bytes = process_destruction_event_shape_cutter(event_data);
            break;
        case DET_REPAIR:
            num_bytes = process_destruction_event_repair(event_data);
            break;
        case DET_MP_DESTROY:
            num_bytes = process_destruction_event_mp_destroy(event_data);
            break;
        case DET_MP_VEHICLE:
            num_bytes = process_destruction_event_mp_veh(event_data);
            break;
        case DET_SHARDIFY:
            num_bytes = process_destruction_event_shardify(event_data); // inlined
            break;
        case DET_DESTRUCTION_MODE:
            num_bytes = process_destruction_event_destruction_mode(event_data); // inlined
            break;
        case DET_LINK_BROKEN:
            num_bytes = process_destruction_event_link_broken(event_data);
            break;
        case DET_SLICE:
            num_bytes = process_destruction_event_slice(event_data);
            break;
        default:
            num_bytes = 0;
            break;
        }
        offset += num_bytes;
    }
    frame_data->general_mover_create_offset = offset;
    frame_data->rfg_mover_create_offset = offset + frame.num_general_movers_created * 3;
    frame_data->rfg_mover_destroy_offset = frame_data->rfg_mover_create_offset + frame.num_rfg_movers_created * 3;
    frame_data->shardify_delete_offset = frame_data->rfg_mover_destroy_offset + frame.num_rfg_movers_destroyed * 2;
    if (Active_destruction_frame_data == nullptr)
    {
        return multi_client_must_process;
    }
    const unsigned int num_counts = frame.num_shardify_delete_counts;
    Active_destruction_frame_data->num_shardify_delete_count_infos = num_counts;
    if (num_counts == 0)
    {
        return multi_client_must_process;
    }
    // Kept: more than 20 records (the 5-bit count allows 31) would overrun the buffer and the record array; the server
    // sends at most 20.
    unsigned char data[72]; // Magic number: 20 records * 28 bits (0x230 bits) and padding
    const unsigned int num_bytes = (num_counts * 28 + 7) >> 3;
    if (num_bytes != 0)
    {
        memcpy(data, &frame._packet_data[frame_data->shardify_delete_offset], num_bytes);
    }
    bit_packer bits(data, 70);
    for (unsigned int i = 0; i < num_counts; i++)
    {
        const unsigned short net_handle = (unsigned short)bits.get_next_bits(16);
        rfg_mover* mover = nullptr;
        if (net_handle != 0xFFFF)
        {
            mover = dest_rfg_mover_from_handle(multi_core_game_handle_from_network_handle(net_handle, false)); // inlined
        }
        shardify_delete_record& record = Active_destruction_frame_data->shardify_delete_count_infos[i];
        record.mover = mover;
        record.delete_count = bits.get_next_bits(12);
    }
    return multi_client_must_process;
}

// The frame to play back: from the newest frame back to the next one; null when it has not arrived
static destruction_frame_data* dest_find_playback_frame()
{
    if (MP_destruction_frame_used_list == nullptr)
    {
        return nullptr;
    }
    destruction_frame_data* frame_data = MP_destruction_frame_used_list->prev;
    if (frame_data == nullptr)
    {
        return nullptr;
    }
    while (frame_data->packet_data.destruction_frame != Playback_next_client_destruction_frame)
    {
        if (frame_data->packet_data.destruction_frame < Playback_next_client_destruction_frame || frame_data == MP_destruction_frame_used_list)
        {
            return nullptr;
        }
        frame_data = frame_data->prev;
        if (frame_data == nullptr)
        {
            return nullptr;
        }
    }
    return frame_data;
}

// 0x009555C0 (:4654): the next frame has arrived
bool multi_game_destruction_is_pending()
{
    Destruction_reorder_wrapper.enter();
    const bool pending = dest_find_playback_frame() != nullptr; // inlined
    Destruction_reorder_wrapper.leave();
    return pending;
}

// 0x009C4CB0 (:4827): the next frame becomes the active one and its events are added; a frame with a repair waits for
// the terminal Havok step (unless migrating or syncing). True when the frame has anything to process.
bool multi_game_destruction_frame_client_get_destruction_events()
{
    Destruction_reorder_wrapper.enter();
    bool must_process = false;
    Active_destruction_frame_data = nullptr;
    destruction_frame_data* frame_data = dest_find_playback_frame(); // inlined
    if (frame_data != nullptr)
    {
        Active_destruction_frame_data = frame_data;
        if (!frame_data->contains_repair_event || havok_get_is_terminal_havok_step() || Migration_state != MIGRATION_STATE_NONE || jip_destruction_sync_state != JIP_DESTRUCTION_STATE_NONE)
        {
            if (frame_data->packet_data.destruction_frame == Playback_next_client_destruction_frame)
            {
                add_destruction_events_from_packet(frame_data, must_process);
            }
            Destruction_reorder_wrapper.leave();
            return must_process;
        }
    }
    Active_destruction_frame_data = nullptr;
    Destruction_reorder_wrapper.leave();
    return false;
}

// 0x00955650 (:4887): the spew buffer in 1000 character pieces (the output was stripped from the shipped build)
void multi_game_destruction_frame_client_spew()
{
    if (Destruction_spew_buf[0] == '\0')
    {
        return;
    }
    char piece[1024];
    const char* src = Destruction_spew_buf;
    for (size_t len = strlen(Destruction_spew_buf); len != 0 && 1000 < len; len -= 1000)
    {
        strncpy(piece, src, 1000);
        src += 1000;
    }
}

// The desync report of the client's mover lists (formatted and dropped; only the debug print remains)
static void dest_report_desync(const char* format, int server_count, int client_count, const char* message)
{
    char buf[1024];
    sprintf(buf, format, server_count, client_count, Active_destruction_frame_data->packet_data.destruction_frame);
    keen::debug::nativePrintArgs(message);
}

// 0x009DE290 (:4913): the general movers the frame created get the server's network handles, remote objects, states and
// reference positions
void multi_game_destruction_frame_client_process_synced_general_mover_remote_objects(general_mover** gmovers, int count)
{
    const unsigned char* data = Active_destruction_frame_data->packet_data._packet_data + Active_destruction_frame_data->general_mover_create_offset;
    const int server_count = Active_destruction_frame_data->packet_data.num_general_movers_created;
    if (count != server_count)
    {
        dest_report_desync("general_mover create desync.  server count: %d, client count: %d, df: %d", server_count, count, "general mover create desync");
    }
    for (int i = 0; i < count; i++)
    {
        const unsigned short net_handle = (unsigned short)(data[0] << 8 | data[1]);
        const unsigned char reference_position_index = data[2];
        data += 3;
        multi_core_create_remote_object_core_internal(gmovers[i]->get_handle(), net_handle, MULTI_OBJECT_TYPE_GEN_MOVER); // inlined
        multi_sim_state<multi_mover_state>* sim_state = General_mover_sim_state_allocator.alloc(0);
        if (sim_state == nullptr)
        {
            keen::debug::nativePrintArgs("Out of general mover sim states!");
        }
        else
        {
            sim_state->reset();
        }
        gmovers[i]->mp_sim_state = sim_state;
        object_mover_error_info* error_info = Object_mover_error_info_allocator.alloc(0);
        if (error_info != nullptr)
        {
            error_info->hold_time.invalidate();
            error_info->hold_count = 0;
        }
        gmovers[i]->m_mp_error_info = error_info;
        gmovers[i]->reference_position_index = reference_position_index;
    }
}

// 0x009DE490 (:4960): the same for the rfg_movers the frame created
void multi_game_destruction_frame_client_process_new_rfg_mover_remote_objects(rfg_mover** movers, int num_rfg_movers_created)
{
    const unsigned char* data = Active_destruction_frame_data->packet_data._packet_data + Active_destruction_frame_data->rfg_mover_create_offset;
    const int server_count = Active_destruction_frame_data->packet_data.num_rfg_movers_created;
    if (num_rfg_movers_created != server_count)
    {
        dest_report_desync("rfg_mover create desync.  server count: %d, client count: %d, df: %d", server_count, num_rfg_movers_created, "rfg mover create desync");
    }
    for (int i = 0; i < num_rfg_movers_created; i++)
    {
        const unsigned short net_handle = (unsigned short)(data[0] << 8 | data[1]);
        const unsigned char reference_position_index = data[2];
        data += 3;
        multi_core_create_remote_object_core_internal(movers[i]->get_handle(), net_handle, MULTI_OBJECT_TYPE_RFG_MOVER); // inlined
        multi_sim_state<multi_mover_state>* sim_state = Rfg_mover_sim_state_allocator.alloc(0);
        if (sim_state != nullptr)
        {
            sim_state->reset();
        }
        movers[i]->mp_info->sim_state = sim_state;
        object_mover_error_info* error_info = Object_mover_error_info_allocator.alloc(0);
        if (error_info != nullptr)
        {
            error_info->hold_time.invalidate();
            error_info->hold_count = 0;
        }
        movers[i]->m_mp_error_info = error_info;
        movers[i]->reference_position_index = reference_position_index;
    }
}

// 0x009C4D80 (:5007)
// Kept: only the count is checked; each destroyed mover's network handle is read and its remote object looked up, and
// neither is used (the remote objects go with the movers' own destruction).
void multi_game_destruction_frame_client_process_destroyed_rfg_mover_remote_objects(rfg_mover** movers, int num_rfg_movers_destroyed)
{
    const int server_count = Active_destruction_frame_data->packet_data.num_rfg_movers_destroyed;
    if (num_rfg_movers_destroyed != server_count)
    {
        dest_report_desync("rfg_mover destroyed desync.  server count: %d, client count: %d, df: %d", server_count, num_rfg_movers_destroyed, "rfg_mover destroyed desync");
    }
}

// 0x009556D0 (:5060)
bool multi_game_destruction_is_setup()
{
    return Active_destruction_frame_data != nullptr;
}

// 0x009556E0 (:5067)
void multi_game_destruction_frame_client_inc_next_destruction_frame()
{
    multi_game_destruction_frame_client_spew();
    Playback_next_client_destruction_frame++;
    Active_destruction_frame_data = nullptr;
}

// 0x00955700 (:5090): big endian
unsigned short multi_game_destruction_get_destruction_frame_id(unsigned char* packet_data)
{
    return (unsigned short)(packet_data[0] << 8 | packet_data[1]);
}

// 0x00955730 (:5145): the used list in frame order (oldest at the head); a frame already there goes back to the free
// list
// LTCG: frame_data in EAX (the port has no register convention).
static void add_destruction_frame_to_list(destruction_frame_data* frame_data)
{
    if (MP_destruction_frame_used_list == nullptr)
    {
        Num_used_destruction_frames++;
        dest_list_add_tail(MP_destruction_frame_used_list, frame_data);
        return;
    }
    destruction_frame_data* tail = MP_destruction_frame_used_list->prev;
    const unsigned short frame = frame_data->packet_data.destruction_frame;
    if (tail->packet_data.destruction_frame < frame)
    {
        Num_used_destruction_frames++;
        dest_list_add_tail(MP_destruction_frame_used_list, frame_data);
        return;
    }
    if (frame < MP_destruction_frame_used_list->packet_data.destruction_frame)
    {
        Num_used_destruction_frames++;
        tail->next = frame_data;
        frame_data->prev = MP_destruction_frame_used_list->prev;
        MP_destruction_frame_used_list->prev = frame_data;
        frame_data->next = MP_destruction_frame_used_list;
        MP_destruction_frame_used_list = frame_data;
        return;
    }
    destruction_frame_data* cur = tail;
    while (true)
    {
        const unsigned short cur_frame = cur->packet_data.destruction_frame;
        if (frame == cur_frame)
        {
            multi_game_destruction_add_to_free_list(frame_data);
            return;
        }
        if (cur_frame <= frame)
        {
            break;
        }
        if (cur == MP_destruction_frame_used_list)
        {
            return; // Kept: the frame is lost (neither list holds it)
        }
        cur = cur->prev;
        if (cur == nullptr)
        {
            return;
        }
    }
    Num_used_destruction_frames++;
    frame_data->next = cur->next;
    frame_data->prev = cur;
    cur->next->prev = frame_data;
    cur->next = frame_data;
}

// 0x00955810 (:5213): PT_DESTRUCTION_FRAME (and the server's own frames): stored in order for playback
void multi_game_destruction_process_packet(unsigned char* packet_data, const sender& from)
{
    Destruction_reorder_wrapper.enter();
    destruction_frame_data* frame_data = multi_game_destruction_get_free_destruction_frame();
    if (frame_data == nullptr)
    {
        keen::debug::nativePrintArgs("no free destruction frames");
        Destruction_reorder_wrapper.leave();
        return;
    }
    get_frame_data(packet_data, frame_data); // inlined
    add_destruction_frame_to_list(frame_data);
    Destruction_reorder_wrapper.leave();
}

// 0x00998B10 (:5261): every frame from the next one to play back up to target_last_frame is stored
bool multi_game_destruction_all_frames_present_up_to(unsigned short target_last_frame)
{
    if (target_last_frame == 0xFFFF)
    {
        return true;
    }
    const unsigned short first_frame = Playback_next_client_destruction_frame;
    if (first_frame < (unsigned short)(target_last_frame + 1))
    {
        for (int frame = first_frame; frame <= (int)target_last_frame; frame++)
        {
            int num_bytes;
            if (multi_game_destruction_get_destruction_frame_data(frame, num_bytes) == nullptr)
            {
                return false;
            }
        }
    }
    return true;
}

// 0x009558A0 (:5291): the effects older than 200 ms (or never started) are dropped
void multi_game_destruction_clear_vehicle_rfg_mover_destruction_effect_info()
{
    for (int i = 0; i < Vrm_active_effect_info_count; i++)
    {
        vrm_effect_info& info = Vrm_effect_infos[i];
        if (!info.start_time.valid() || (info.start_time.valid() && 200 < info.start_time.time_since())) // Magic number: 200 ms
        {
            info.start_time.invalidate();
            // Kept: one element too few is moved (the last active effect is lost when an earlier one is dropped).
            memmove(&Vrm_effect_infos[i], &Vrm_effect_infos[i + 1], (Vrm_active_effect_info_count - i) * sizeof(vrm_effect_info) - sizeof(vrm_effect_info));
            i--;
            Vrm_active_effect_info_count--;
        }
    }
}

// 0x00955930 (:5308): a hit within 1.5 m of a recorded one adds its damage; otherwise a new effect (at most 20)
void multi_game_destruction_add_vehicle_rfg_mover_destruction_effect_info(const vector& world_pos, const vector& world_dir, float damage)
{
    for (int i = 0; i < Vrm_active_effect_info_count; i++)
    {
        if (vec_dist_squared(world_pos, Vrm_effect_infos[i].world_pos) < 2.25f) // Magic number: 1.5 m squared
        {
            Vrm_effect_infos[i].damage += damage;
            return;
        }
    }
    if (Vrm_active_effect_info_count < 20)
    {
        vrm_effect_info& info = Vrm_effect_infos[Vrm_active_effect_info_count];
        Vrm_active_effect_info_count++;
        info.world_pos = world_pos;
        info.world_dir = world_dir;
        info.damage = damage;
        info.effect_added = false;
        info.start_time.set(0);
    }
}

// 0x009559E0 (:5332)
void multi_game_destruction_process_vehicle_rfg_mover_destruction_effect_info()
{
    for (int i = 0; i < Vrm_active_effect_info_count; i++)
    {
        if (!Vrm_effect_infos[i].effect_added)
        {
            Vrm_effect_infos[i].effect_added = true;
        }
    }
}

// 0x00955A00 (:5352)
bool multi_game_destruction_should_run_stress()
{
    return true;
}

// 0x009C4EB0 (:5367): PT_VALIDATE_DESTRUCTION (the debug validation, only sent by unused_dcf_validate_client_movers):
// the light form checks the mover's totals (computed and not compared in the shipped build); the full form compares
// each listed piece's ownership with the server's and stops at the first difference. Nothing is reported.
void multi_game_validate_destruction(unsigned char* packet_data, const sender& from)
{
    static int last_recvd_gameframe; // function static in rfg.exe
    const unsigned char light = packet_data[0];
    const unsigned short net_handle = (unsigned short)(packet_data[1] << 8 | packet_data[2]);
    const unsigned int handle = multi_core_game_handle_from_network_handle(net_handle, false);
    rfg_mover* mover = dest_rfg_mover_from_handle(handle);
    if (mover == nullptr)
    {
        return;
    }
    if (last_recvd_gameframe != Framecount)
    {
        last_recvd_gameframe = Framecount;
    }
    if (light != 0)
    {
        unsigned short num_subpieces;
        int anchor_str;
        int link_str;
        int dyn_link_str = 0;
        get_rfg_mover_subpiece_info(handle, num_subpieces, anchor_str, link_str, dyn_link_str);
        return;
    }
    rfg_destroyable_shape* shape = mover->get_shape();
    unsigned int index = (unsigned short)(packet_data[3] << 8 | packet_data[4]);
    unsigned int stop_index = shape->parent->num_objects - 1;
    if ((int)(index + 9) < (int)stop_index) // Magic number: 10 pieces per packet
    {
        stop_index = index + 9;
    }
    int offset = 5;
    for (; index <= (stop_index & 0xFFFF); index++)
    {
        const unsigned int sp_index = (unsigned short)(packet_data[offset] << 8 | packet_data[offset + 1]);
        rfg_subpiece* sp = shape->get_subpiece(sp_index);
        if ((sp->get_destroyable() == shape) != (packet_data[offset + 2] != 0))
        {
            return;
        }
        // Kept: 8 bytes and the client piece's links are skipped even when the server sent an unowned piece (3 bytes).
        offset += 8;
        for (unsigned int link = sp->num_links; link != 0; link--)
        {
            offset += 4;
        }
    }
}

// 0x00955A20 (:5457): empty in the shipped build
void multi_game_process_validate_destruction_sync(unsigned char* packet_data, const sender& from)
{
}

// Big endian 32 bits into a packet
static void dest_write_uint_be(unsigned char* dest, unsigned int value)
{
    dest[0] = (unsigned char)(value >> 24);
    dest[1] = (unsigned char)(value >> 16);
    dest[2] = (unsigned char)(value >> 8);
    dest[3] = (unsigned char)value;
}

// 0x009C50F0 (:5495): PT_VALIDATE_DESTRUCTION to every client: the light form sends the mover's totals, the full form
// each piece from start_index to stop_index (its index, whether the mover still owns it, its anchor yield, its link
// count and link strengths)
void multi_send_validate_destruction(unsigned int handle, unsigned char light, unsigned int start_index, unsigned int stop_index)
{
    unsigned char packet[1200]; // Magic number: the largest packet
    packet[0] = PT_VALIDATE_DESTRUCTION;
    unsigned int size = multi_core_object_packet_header_size(PT_VALIDATE_DESTRUCTION); // inlined
    packet[size] = light;
    const unsigned short net_handle = multi_core_network_handle_from_game_handle(handle, false); // inlined
    packet[size + 1] = (unsigned char)(net_handle >> 8);
    packet[size + 2] = (unsigned char)net_handle;
    if (light == 0)
    {
        size += 3;
        rfg_mover* mover = dest_rfg_mover_from_handle(handle);
        rfg_destroyable_shape* shape = mover->get_shape(); // Kept: no null check
        for (unsigned int index = start_index; index <= stop_index; index++)
        {
            const rfg_subpiece* sp = shape->get_subpiece(index);
            packet[size & 0xFFFF] = (unsigned char)(index >> 8);
            packet[(size & 0xFFFF) + 1] = (unsigned char)index;
            const bool owned = sp->get_destroyable() == shape;
            packet[(size + 2) & 0xFFFF] = owned;
            size += 3;
            if (owned)
            {
                dest_write_uint_be(&packet[size & 0xFFFF], (unsigned int)sp->anchor_yield);
                packet[(size + 4) & 0xFFFF] = sp->num_links;
                size += 5;
                for (int link = 0; link < sp->num_links; link++)
                {
                    dest_write_uint_be(&packet[size & 0xFFFF], (unsigned int)shape->get_link(sp->links[link])->yield_limit);
                    size += 4;
                }
            }
        }
    }
    else
    {
        unsigned short num_subpieces;
        int anchor_str;
        int link_str;
        int dyn_link_str = 0;
        get_rfg_mover_subpiece_info(handle, num_subpieces, anchor_str, link_str, dyn_link_str);
        packet[size + 3] = (unsigned char)(num_subpieces >> 8);
        packet[size + 4] = (unsigned char)num_subpieces;
        dest_write_uint_be(&packet[size + 5], (unsigned int)anchor_str);
        dest_write_uint_be(&packet[size + 9], (unsigned int)link_str);
        dest_write_uint_be(&packet[size + 13], (unsigned int)dyn_link_str);
        size += 17;
    }
    multi_core_object_packet_set_size(packet, size & 0xFFFF); // inlined
    multi_io_send_to_all_reliable(multi_session_get_by_type(ST_GAME), packet, size & 0xFFFF);
}

// 0x009C54D0 (:5554): a server sends the full validation of one mover (10 pieces per packet), or the light one of the
// destroyed movers (or of all with "all")
void unused_dcf_validate_client_movers()
{
    if (!Multiplayer || !Server)
    {
        return;
    }
    bool is_string = false;
    const bool has_arg = console_get_arg(10, 1); // Magic number: an integer or a string argument, optional
    const int handle = Console_arg_int;
    if (has_arg)
    {
        is_string = Console_arg_type == 2; // Magic number: a string
    }
    if (has_arg && Console_arg_type == 8) // Magic number: an integer
    {
        rfg_mover* mover = dest_rfg_mover_from_handle(handle);
        rfg_destroyable_shape* shape = mover->get_shape(); // Kept: no null check
        const int last_index = shape->parent->num_objects - 1;
        unsigned int start_index = 0;
        unsigned int stop_index = 9;
        if (last_index < 10)
        {
            stop_index = last_index;
        }
        int next_start;
        do
        {
            multi_send_validate_destruction(handle, 0, start_index, stop_index);
            next_start = start_index + 10;
            stop_index = start_index + 19;
            if (last_index <= (int)stop_index)
            {
                stop_index = last_index;
            }
            start_index = next_start;
        } while (next_start < (int)stop_index);
        return;
    }
    const bool all = is_string && v_stricmp(Console_arg, "all") == 0;
    for (int i = 0; i < World.get_num_objects<rfg_mover>(); i++)
    {
        rfg_mover* mover = World.get_object<rfg_mover>(i);
        if (mover->mp_info->last_destruction_frame != 0xFFFF || all)
        {
            multi_send_validate_destruction(mover->get_handle(), 1, 0, 0);
        }
    }
}

// 0x00955A30 (:5612): empty in the shipped build
void multi_game_destruction_validate_rfg_mover_initial_state()
{
}

// 0x0097B7D0 (:5664): empty in the shipped build
void multi_game_destruction_send_validate_destruction_frame(int destruction_frame_completed)
{
}

// 0x0097B7E0 (:5751): empty in the shipped build
void multi_game_destruction_process_validate_destruction_frame(unsigned int destruction_frame_completed)
{
}

// 0x00955A40 (:5787)
int multi_game_destruction_get_max_repair_duration()
{
    return 3100; // Magic number: 0xC1C ms (31 steps of 100 ms in the 5-bit field)
}

// A packet of the type's header only (the join in progress sync signals)
static void dest_send_header_packet(multi_connection* cp, unsigned char type)
{
    unsigned char packet[1200];
    packet[0] = type;
    const unsigned int size = multi_core_object_packet_header_size(type); // inlined
    multi_core_object_packet_set_size(packet, size); // inlined
    multi_io_send_reliable(cp, packet, size, nullptr, nullptr, nullptr);
}

// 0x009C5600 (:5801)
void send_begin_sync_packet(multi_connection* cp)
{
    dest_send_header_packet(cp, PT_JIP_START_DESTRUCTION_SYNC);
}

// 0x009C5660 (:5809)
void send_finished_sync_packet(multi_connection* cp)
{
    dest_send_header_packet(cp, PT_JIP_FINISH_DESTRUCTION_SYNC);
}

// 0x009C56C0 (:5818)
void send_client_ready_for_destruction_data_packet(multi_connection* cp)
{
    dest_send_header_packet(cp, PT_JIP_CLIENT_READY_FOR_DATA);
}

// 0x009DE680 (:5827): a client joining after destruction starts the sync; before any, it joins the round at once
void multi_game_destruction_start_server_sync(multi_connection* cp)
{
    if (rfg_dyn_get_active_destruction_frame() != 0xFFFF && rfg_dyn_get_active_destruction_frame() != 0)
    {
        send_begin_sync_packet(cp);
        return;
    }
    multi_game_mode_round_begin_common_jip(cp, cp->game_data);
}

// 0x00998B90 (:5842): PT_JIP_CLIENT_READY_FOR_DATA: the client is synced at the end of the next destruction frame
void multi_game_destruction_server_sync_send_data(unsigned char* packet_data, const sender& from)
{
    if (JIP_client_to_sync_list.add(from.connection->player_uid) < 0)
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n");
    }
}

// 0x00955A50 (:5847)
bool multi_game_destruction_is_jip_sync()
{
    return jip_destruction_sync_state != JIP_DESTRUCTION_STATE_NONE;
}

// 0x00955A60 (:5852)
bool multi_game_destruction_jip_can_receive_destr()
{
    return !Multi_join_in_progress;
}

// 0x009DE6B0 (:5861)
void multi_game_destruction_all_data_arrived()
{
    jip_destruction_sync_state = JIP_DESTRUCTION_STATE_WAIT_FOR_DESTRUCTION_APPLIED;
}

// 0x009C5720 (:5868)
void multi_game_destruction_set_jip_sync_state(jip_destruction_sync new_state)
{
    jip_destruction_sync_state = new_state;
    if (Multiplayer && !Server && new_state == JIP_DESTRUCTION_STATE_DATA_SYNC)
    {
        send_client_ready_for_destruction_data_packet(multi_session_get_by_type(ST_GAME)->get_host_connection());
    }
}

// 0x009DE6C0 (:5900): a joining client applies the frames it was sent, then tells the server it is done
void multi_game_destruction_jip_sync_process()
{
    if (Multiplayer && Server)
    {
        return;
    }
    if (jip_destruction_sync_state == JIP_DESTRUCTION_STATE_WAIT_FOR_DESTRUCTION_APPLIED)
    {
        if (multi_game_destruction_is_pending())
        {
            rfg_dyn_do_frame_pre();
            rfg_dyn_do_collide_post();
        }
        if (has_all_destruction_been_applied())
        {
            multi_game_migration_destroy_general_mover_destroyed_list();
            jip_destruction_sync_state = JIP_DESTRUCTION_STATE_POST_SYNC;
        }
        return;
    }
    if (jip_destruction_sync_state != JIP_DESTRUCTION_STATE_POST_SYNC)
    {
        return;
    }
    multi_session* sp = multi_session_get_by_type(ST_GAME);
    if (sp == nullptr || sp->get_host_connection() == nullptr)
    {
        return;
    }
    send_finished_sync_packet(sp->get_host_connection());
    if (sp->get_me_connection() != nullptr)
    {
        jip_destruction_sync_state = JIP_DESTRUCTION_STATE_NONE;
    }
}

// 0x009C5760 (:5962): PT_JIP_START_DESTRUCTION_SYNC (multi_game_destruction_set_jip_sync_state inlined)
void multi_game_destruction_start_jip_sync(unsigned char* packet_data, const sender& from)
{
    multi_game_destruction_set_jip_sync_state(JIP_DESTRUCTION_STATE_DATA_SYNC);
}

// 0x009DE750 (:5967): PT_JIP_FINISH_DESTRUCTION_SYNC: everyone learns that the player joined (PT_CLIENT_JOINED_JIP with
// its game data handle, big endian), and the player joins the round
void multi_game_destruction_finish_jip_sync(unsigned char* packet_data, const sender& from)
{
    unsigned char packet[1200];
    packet[0] = PT_CLIENT_JOINED_JIP;
    unsigned int size = multi_core_object_packet_header_size(PT_CLIENT_JOINED_JIP); // inlined
    const unsigned int game_data = from.connection->game_data;
    dest_write_uint_be(&packet[size], game_data);
    size += 4;
    multi_core_object_packet_set_size(packet, size); // inlined
    multi_io_send_to_all_reliable(multi_session_get_by_type(ST_GAME), packet, size);
    hud_ui_multiplayer_player_joined(from.connection->game_data);
    multi_game_mode_round_begin_common_jip(from.connection, from.connection->game_data);
}

// 0x009DE840 (:5992): on a server, after 400 destruction frames (or when forced) join in progress ends and the clients
// still joining are disconnected
void multi_game_destruction_update_jip_status(bool force_disable)
{
    if (!Multiplayer || !Server || !jip_destruction_sync_possible)
    {
        return;
    }
    if (rfg_dyn_get_active_destruction_frame() <= 399 && !force_disable) // Magic number: 400 frames
    {
        return;
    }
    jip_destruction_sync_possible = false;
    multi_session* sp = multi_session_get_by_type(ST_GAME);
    multi_connection* cp = sp->get_connection_list();
    do
    {
        if (cp == nullptr)
        {
            return;
        }
        if (cp->joining_in_progress)
        {
            sp->remove_connection(cp);
            multi_connection_disconnect(cp, CDR_SESSION_START_FAILED);
        }
        cp = cp->next;
    } while (cp != sp->get_connection_list());
}

// 0x00955A70 (:6027)
bool multi_game_destruction_can_jip_client()
{
    return jip_destruction_sync_possible;
}

// 0x009DE8C0 (:6032): each client that asked for the data gets the frames from start_index to end_index and the
// destroyed movers list, and stops joining
void multi_game_destruction_sync_jip(unsigned short start_index, unsigned short end_index)
{
    multi_session* sp = multi_session_get_by_type(ST_GAME);
    for (int i = 0; i < JIP_client_to_sync_list.size(); i++)
    {
        multi_connection* first = sp->get_connection_list();
        if (first == nullptr)
        {
            continue;
        }
        multi_connection* cp = first;
        while (!cp->player_uid.is_equal(JIP_client_to_sync_list[i].get_internal_id()))
        {
            cp = cp->next;
            if (cp == first || cp == nullptr)
            {
                cp = nullptr;
                break;
            }
        }
        if (cp == nullptr)
        {
            continue;
        }
        multi_game_migration_send_client_destruction_update(cp, start_index, end_index);
        multi_game_object_mover_send_destroyed_movers_list(cp);
        cp->joining_in_progress = false;
    }
    JIP_client_to_sync_list.remove_all();
}
#endif
