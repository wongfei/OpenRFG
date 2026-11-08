// Original: volition/rfg/code/multi/multi_game/multi_game_destruction.h
// Implementation: volition/rfg/code/multi/multi_game/multi_game_destruction.cpp
//
// Networked destruction: the server records each frame's destruction (explosions, damage, broken links, shatters, shape
// cutters, repairs) into packets the clients apply in order; join in progress clients receive the destruction so far.
// The working data setters are called by rfg_dynamics. Placement of the destruction packet types inferred.
#pragma once

#include "volition/rfg/code/destruction/rfg_dynamics.h"

class general_mover;
class multi_connection;
class rfg_mover;
struct sender;
class vector;

enum jip_destruction_sync
{
    JIP_DESTRUCTION_STATE_DATA_SYNC = 0,
    JIP_DESTRUCTION_STATE_WAIT_FOR_DESTRUCTION_APPLIED = 1,
    JIP_DESTRUCTION_STATE_POST_SYNC = 2,
    JIP_DESTRUCTION_STATE_NONE = 3,
};

// size 0x20C
struct _destruction_packet
{
    unsigned short destruction_frame; // 0x00
    unsigned char num_destruction_events; // 0x02
    unsigned char num_general_movers_created; // 0x03
    unsigned char num_rfg_movers_created; // 0x04
    unsigned char num_rfg_movers_destroyed; // 0x05
    unsigned char num_shardify_delete_counts; // 0x06
    unsigned char migration; // 0x07
    bool in_use; // 0x08
    unsigned short _packet_size; // 0x0A
    unsigned char _packet_data[512]; // 0x0C
};

// size 0x2CC
struct destruction_frame_data
{
    _destruction_packet packet_data; // 0x00
    int general_mover_create_offset; // 0x20C
    int rfg_mover_create_offset; // 0x210
    int rfg_mover_destroy_offset; // 0x214
    int shardify_delete_offset; // 0x218
    bool contains_repair_event; // 0x21C
    int num_shardify_delete_count_infos; // 0x220
    shardify_delete_record shardify_delete_count_infos[20]; // 0x224
    destruction_frame_data* prev; // 0x2C4
    destruction_frame_data* next; // 0x2C8
};

void unused_dcf_rfgm_net_handle(); // 0x009BDE60 (:396)
void unused_dcf_rfgm_game_handle(); // 0x009BDE90 (:409)
void unused_dcf_rfgm_destruction_uid(); // 0x00998710 (:423)
void unused_dcf_destruction_status(); // 0x00952940 (:437)
void multi_game_destruction_init(); // 0x0097B580 (:474)
void multi_game_destruction_shutdown(); // 0x00952A30 (:499)
unsigned short int_pack16(unsigned int val); // 0x00952A50 (:514)
unsigned int int_unpack16(unsigned short ipacked); // 0x00952A90 (:531)
unsigned short shapekey_pack16(unsigned int shapekey); // 0x00952AB0 (:538)
unsigned int shapekey_unpack16(unsigned short packed_shapekey); // 0x00952AC0 (:548)
unsigned int get_rfg_mover_quasi_handle_for_recorded_destruction(unsigned short net_handle); // 0x00952C80 (:2668)
void multi_game_destruction_set_explosion_working_data(rfg_collision_event_base& event_base); // 0x009C3720 (:2683)
void multi_game_destruction_set_object_simple_working_data(rfg_collision_event_base& event_base); // 0x009C3770 (:2700)
void multi_game_destruction_set_object_complex_working_data(rfg_collision_event_base& event_base); // 0x009C37D0 (:2717)
void multi_game_destruction_set_direct_damage_working_data(rfg_collision_event_base& event_base); // 0x009C3850 (:2741)
void multi_game_destruction_set_link_broken_working_data(rfg_collision_event_base& event_base); // 0x009C38A0 (:2758)
void multi_game_destruction_set_sp_array_working_data(rfg_collision_event_base& event_base); // 0x009C38F0 (:2770)
void multi_game_destruction_set_shatter_working_data(rfg_collision_event_base& event_base); // 0x009C3960 (:2787)
void multi_game_destruction_set_shape_cutter_working_data(rfg_collision_event_base& event_base); // 0x009C39B0 (:2804)
void multi_game_destruction_set_repair_working_data(rfg_collision_event_base& event_base); // 0x009C39F0 (:2820)
void multi_game_destruction_set_mp_destroy_working_data(rfg_collision_event_base& event_base); // 0x009C3A30 (:2836)
void multi_game_destruction_set_mp_veh_working_data(rfg_collision_event_base& event_base); // 0x009C3A80 (:2848)
void multi_game_destruction_set_slice_working_data(rfg_collision_event_base& event_base); // 0x009C3AF0 (:2865)
void multi_game_destruction_frame_clear(); // 0x00955060 (:3959)
bool multi_game_destruction_frame_in_use(); // 0x00955070 (:3966)
void multi_game_destruction_frame_server_start_packet(unsigned short destruction_frame); // 0x00955080 (:3977)
void multi_game_destruction_frame_server_add_destruction_event(const rfg_collision_event_base& event_base); // 0x009F08F0 (:4017)
void multi_game_destruction_frame_server_add_create_general_movers(general_mover** gmp, int count); // 0x009F0910 (:4040)
void multi_game_destruction_frame_server_add_create_rfg_movers(rfg_mover** rmp, int count); // 0x009F0A50 (:4065)
void multi_game_destruction_frame_server_add_destroyed_rfg_movers(rfg_mover** rmp, int count); // 0x009F0B90 (:4090)
void multi_game_destruction_frame_server_add_shardify_delete_counts(shardify_delete_record* delete_count_list, int count); // 0x009F0C70 (:4111)
void multi_game_destruction_frame_server_finalize_packet(); // 0x009C46B0 (:4150)
int multi_game_destruction_get_client_balance_delete_count(const rfg_mover* rmp); // 0x00955140 (:4262)
void multi_game_destruction_level_init_pre(); // 0x0097B610 (:4282)
void multi_game_destruction_level_shutdown(); // 0x0097B7C0 (:4318)
void multi_game_destruction_reset_validation(); // 0x00955180 (:4324)
unsigned short multi_game_destruction_get_last_applied_destruction_frame(); // 0x009552B0 (:4353)
unsigned short multi_game_destruction_get_last_destruction_frame(); // 0x009552D0 (:4361)
void multi_game_destruction_cull_destruction_frames_starting(int start_cull_frame_index); // 0x009552F0 (:4375)
unsigned char* multi_game_destruction_get_destruction_frame_data(int destruction_frame, int& num_bytes); // 0x00998990 (:4487)
bool multi_game_destruction_is_pending(); // 0x009555C0 (:4654)
unsigned short get_destruction_next_frame_id(unsigned short frame_id); // 0x00955630 (:4673)
bool multi_game_destruction_frame_client_get_destruction_events(); // 0x009C4CB0 (:4827)
void multi_game_destruction_frame_client_spew(); // 0x00955650 (:4887)
void multi_game_destruction_frame_client_process_synced_general_mover_remote_objects(general_mover** gmovers, int count); // 0x009DE290 (:4913)
void multi_game_destruction_frame_client_process_new_rfg_mover_remote_objects(rfg_mover** movers, int num_rfg_movers_created); // 0x009DE490 (:4960)
void multi_game_destruction_frame_client_process_destroyed_rfg_mover_remote_objects(rfg_mover** movers, int num_rfg_movers_destroyed); // 0x009C4D80 (:5007)
bool multi_game_destruction_is_setup(); // 0x009556D0 (:5060)
void multi_game_destruction_frame_client_inc_next_destruction_frame(); // 0x009556E0 (:5067)
unsigned short multi_game_destruction_get_destruction_frame_id(unsigned char* packet_data); // 0x00955700 (:5090)
void multi_game_destruction_process_packet(unsigned char* packet_data, const sender& from); // 0x00955810 (:5213)
bool multi_game_destruction_all_frames_present_up_to(unsigned short target_last_frame); // 0x00998B10 (:5261)
// A vehicle's hit on a building, replayed as an effect on the clients (placement inferred: the vehicle rfg_mover
// effect functions below fill the table).
// size 0x24
struct vrm_effect_info
{
    vrm_effect_info(); // 0x00952930

    timestamp start_time; // 0x00
    vector world_pos; // 0x04
    vector world_dir; // 0x10
    float damage; // 0x1C
    bool effect_added; // 0x20
};

extern vrm_effect_info Vrm_effect_infos[20]; // 0x02FED258
extern int Vrm_active_effect_info_count; // 0x02FEB6A0

void multi_game_destruction_clear_vehicle_rfg_mover_destruction_effect_info(); // 0x009558A0 (:5291)
void multi_game_destruction_add_vehicle_rfg_mover_destruction_effect_info(const vector& world_pos, const vector& world_dir, float damage); // 0x00955930 (:5308)
void multi_game_destruction_process_vehicle_rfg_mover_destruction_effect_info(); // 0x009559E0 (:5332)
bool multi_game_destruction_should_run_stress(); // 0x00955A00 (:5352)
void multi_game_destruction_record_last_frame(rfg_mover* mover); // 0x00955A10 (:5358)
void multi_game_validate_destruction(unsigned char* packet_data, const sender& from); // 0x009C4EB0 (:5367)
void multi_game_process_validate_destruction_sync(unsigned char* packet_data, const sender& from); // 0x00955A20 (:5457)
void multi_send_validate_destruction(unsigned int handle, unsigned char light, unsigned int start_index, unsigned int stop_index); // 0x009C50F0 (:5495)
void unused_dcf_validate_client_movers(); // 0x009C54D0 (:5554)
void multi_game_destruction_validate_rfg_mover_initial_state(); // 0x00955A30 (:5612)
void multi_game_destruction_send_validate_destruction_frame(int destruction_frame_completed); // 0x0097B7D0 (:5664)
void multi_game_destruction_process_validate_destruction_frame(unsigned int destruction_frame_completed); // 0x0097B7E0 (:5751)
int multi_game_destruction_get_max_repair_duration(); // 0x00955A40 (:5787)
void send_begin_sync_packet(multi_connection* cp); // 0x009C5600 (:5801)
void send_finished_sync_packet(multi_connection* cp); // 0x009C5660 (:5809)
void send_client_ready_for_destruction_data_packet(multi_connection* cp); // 0x009C56C0 (:5818)
void multi_game_destruction_start_server_sync(multi_connection* cp); // 0x009DE680 (:5827)
void multi_game_destruction_server_sync_send_data(unsigned char* packet_data, const sender& from); // 0x00998B90 (:5842)
bool multi_game_destruction_is_jip_sync(); // 0x00955A50 (:5847)
bool multi_game_destruction_jip_can_receive_destr(); // 0x00955A60 (:5852)
void multi_game_destruction_all_data_arrived(); // 0x009DE6B0 (:5861)
void multi_game_destruction_set_jip_sync_state(jip_destruction_sync new_state); // 0x009C5720 (:5868)
void multi_game_destruction_jip_sync_process(); // 0x009DE6C0 (:5900)
void multi_game_destruction_start_jip_sync(unsigned char* packet_data, const sender& from); // 0x009C5760 (:5962)
void multi_game_destruction_finish_jip_sync(unsigned char* packet_data, const sender& from); // 0x009DE750 (:5967)
void multi_game_destruction_update_jip_status(bool force_disable); // 0x009DE840 (:5992)
bool multi_game_destruction_can_jip_client(); // 0x00955A70 (:6027)
void multi_game_destruction_sync_jip(unsigned short start_index, unsigned short end_index); // 0x009DE8C0 (:6032)
