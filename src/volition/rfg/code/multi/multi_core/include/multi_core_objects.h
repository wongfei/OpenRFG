// Original: volition/rfg/code/multi/multi_core/include/multi_core_objects.h
// Implementation: volition/rfg/code/multi/multi_core/global/multi_core_objects.cpp
//
// Networked objects: each object type registers its state handlers (multi_core_object_registration); remote objects map
// network handles to game objects with per connection data; object states are bundled, prioritized and sent, and object
// creation and destruction are replicated. object_state_data<T> holds the states of one bundle type (the bundles are in
// the multi_game_object_* headers). Placement of the
// bundles and remote objects inferred.
#pragma once


class bit_packer;
class dc_object;
class dc_state_list;
class matrix;
struct mp_transmit_list;
class multi_connection;
class object;
class quaternion;
struct remote_object;
struct sender;
class vector;
class vector2;

enum multi_core_state_priority
{
    MULTI_CORE_STATE_PRIORITY_NONE = -1,
    MULTI_CORE_STATE_PRIORITY_ABYSMAL = 0,
    MULTI_CORE_STATE_PRIORITY_LOW = 1,
    MULTI_CORE_STATE_PRIORITY_MEDIUM = 2,
    MULTI_CORE_STATE_PRIORITY_HIGH = 3,
    MULTI_CORE_STATE_PRIORITY_EXTREME = 4,
    MULTI_CORE_NUM_STATE_PRIORITIES = 5,
};

// size 0x20
struct mask_info
{
    const char* name; // 0x00
    const char* groupname; // 0x04
    unsigned int mask; // 0x08
    unsigned int num_bits; // 0x0C
    float epsilon; // 0x10
    float pack_threshold; // 0x14
    float pack_bias_value; // 0x18
    float pack_bias; // 0x1C
};

// size 0x4
struct query_size_results
{
    unsigned short mask; // 0x00
    unsigned short num_bytes; // 0x02
};

// size 0x8
struct state_priority
{
    multi_core_state_priority priority; // 0x00
    unsigned int time_since_update; // 0x04
};

// size 0x80
struct state_priority_bundle
{
    state_priority state_data[16]; // 0x00
};

// size 0x34
struct rop_sort_element
{
    remote_object* rop; // 0x00
    int weight; // 0x04
    multi_core_state_priority priority; // 0x08
    unsigned int last_size; // 0x0C
    unsigned short possible_mask_bits; // 0x10
    unsigned short mask_bits; // 0x12
    query_size_results qs_results[5]; // 0x14
    dc_state_list* current_state; // 0x28
    dc_state_list* reference_state; // 0x2C
    dc_object* dco; // 0x30
};

// size 0x204
struct object_create_data
{
    unsigned short net_handle; // 0x00
    unsigned char type; // 0x02
    unsigned char packet_data[512]; // 0x03
};

// size 0x204
struct object_destroy_data
{
    unsigned short net_handle; // 0x00
    unsigned char type; // 0x02
    unsigned char packet_data[512]; // 0x03
};

// size 0x8
struct remote_object_connection_data
{
    unsigned int time_last_updated; // 0x00
    unsigned short create_ack_id; // 0x04
};

// size 0x898
struct remote_object
{
    unsigned short m_network_handle; // 0x00
    unsigned char current_type; // 0x02
    unsigned char m_pending_delete; // 0x03
    unsigned int m_game_handle; // 0x04
    remote_object_connection_data conn_data[16]; // 0x08
    state_priority_bundle m_state_priority_bundle[16]; // 0x88
    remote_object* next; // 0x888
    remote_object* prev; // 0x88C
    remote_object* type_next; // 0x890
    remote_object* type_prev; // 0x894
};

// size 0x814
class multi_core_object_registration
{
    const mask_info* get_mask_info(unsigned int mask) const; // 0x009483B0 (:2292)
public:
    multi_core_object_registration(); // 0x00942EA0 (.h:212)
    virtual ~multi_core_object_registration(); // vtbl+0x00 0x00942EB0 (.h:213)
    virtual void create(unsigned int) const = 0; // vtbl+0x04
    virtual void send_create_packet(const remote_object*, unsigned char*, unsigned short&, multi_connection*) const = 0; // vtbl+0x08
    virtual void process_create_packet(unsigned int&, unsigned short, int&, unsigned char*) const = 0; // vtbl+0x0C
    virtual void send_destroy_packet(unsigned int, unsigned char*, unsigned short&) const = 0; // vtbl+0x10
    virtual void process_destroy_packet(unsigned int, int&, unsigned char*) const = 0; // vtbl+0x14
    virtual void process_object_free(object*) const = 0; // vtbl+0x18
    virtual bool allow_create_packet(int& packet_offset, unsigned char* packet_data) const; // vtbl+0x1C 0x00947E30 (:627)
    virtual bool allow_destroy_packet(unsigned int game_handle, int& packet_offset, unsigned char* packet_data) const; // vtbl+0x20 0x00947E40 (:632)
    virtual void post_update_cleanup() const = 0; // vtbl+0x24
    virtual void move(unsigned int) const = 0; // vtbl+0x28
    virtual void get_state(unsigned int, dc_state_list*) const = 0; // vtbl+0x2C
    virtual void free_state(dc_state_list*) const = 0; // vtbl+0x30
    virtual void get_compressed_state(dc_state_list*, dc_state_list*, dc_state_list*, multi_connection*, multi_core_state_priority, unsigned short) const = 0; // vtbl+0x34
    virtual bool apply_compressed_state(unsigned int, dc_state_list*, dc_state_list*, dc_state_list**, unsigned int) const = 0; // vtbl+0x38
    virtual unsigned short query_possible_mask_bits(dc_state_list*, dc_state_list*, multi_connection*) const = 0; // vtbl+0x3C
    virtual void query_space_for_priority(const remote_object*, dc_state_list*, dc_state_list*, unsigned short, multi_connection*, query_size_results*) const = 0; // vtbl+0x40
    virtual void query_space_for_mask(query_size_results*, void*) const = 0; // vtbl+0x44
    virtual void notify_transmitted_updated(const remote_object*, dc_state_list*, multi_connection*, unsigned short, unsigned short) const = 0; // vtbl+0x48
    virtual void reset_state_data() const = 0; // vtbl+0x4C
    virtual void free_mp_state_data() const = 0; // vtbl+0x50
    virtual bool check_object_is_dirty(dc_state_list*, dc_state_list*, const remote_object*) const = 0; // vtbl+0x54
    virtual bool check_full_update(dc_state_list*, multi_connection*) const = 0; // vtbl+0x58
    void reset_state_masks(); // 0x009483F0 (:2306)
    bool add_state_mask(const char* namep, unsigned int mask, unsigned int num_bits, float epsilon, float pack_threshold, float pack_bias_value, float pack_bias); // 0x00948470 (:2355)
    bool add_state_mask(const char* namep, const char* groupname, unsigned int mask, unsigned int num_bits, float epsilon, float pack_threshold, float pack_bias_value, float pack_bias); // 0x00948400 (:2326)
    unsigned int get_state_mask_num_bits(unsigned int mask) const; // 0x009484E0 (:2362)
    float get_state_mask_epsilon(unsigned int mask) const; // 0x00948520 (:2370)
    float get_state_mask_pack_threshold(unsigned int mask) const; // 0x00948560 (:2378)
    float get_state_mask_pack_bias_value(unsigned int mask) const; // 0x009485A0 (:2386)
    float get_state_mask_pack_bias(unsigned int mask) const; // 0x009485E0 (:2394)
    unsigned int get_vector_mask(const vector&, const vector, ...) const; // 0x00948620 (:2400)
    unsigned int get_orient_mask(const quaternion& target_quat, const quaternion& sync_quat, unsigned int mask, unsigned int compressed_mask, unsigned int compressed2_mask) const; // 0x00948A50 (:2586)
    unsigned int get_orient_mask(const matrix& target_matrix, const matrix& sync_matrix, unsigned int mask, unsigned int compressed_mask, unsigned int compressed2_mask) const; // 0x00948810 (:2521)
    void init_flag_bandwidth(); // 0x009483A0 (:2280)
    bool buffer_compressed_pbh(bit_packer& packer, unsigned int mask, matrix& client_matrix, const matrix& transmitted_matrix) const; // 0x00949D30 (:3080)
    bool buffer_compressed_pbh(bit_packer& packer, unsigned int mask, quaternion& client_quat, const quaternion& transmitted_quat) const; // 0x00949F10 (:3118)
    bool buffer_compressed_ph(bit_packer& packer, unsigned int mask, matrix& client_matrix, const matrix& transmitted_matrix) const; // 0x0094A110 (:3156)
    bool buffer_compressed_ph(bit_packer& packer, unsigned int mask, quaternion& client_quat, const quaternion& transmitted_quat) const; // 0x0094A2B0 (:3199)
    bool buffer_delta_compressed_pbh(bit_packer& packer, unsigned int mask, matrix& client_matrix, const matrix& ref_matrix, const matrix& transmitted_matrix) const; // 0x00949790 (:2925)
    bool buffer_delta_compressed_pbh(bit_packer& packer, unsigned int mask, quaternion& client_quat, const quaternion& ref_quat, const quaternion& transmitted_quat) const; // 0x00949900 (:2964)
    bool buffer_delta_compressed_ph(bit_packer& packer, unsigned int mask, matrix& client_matrix, const matrix& ref_matrix, const matrix& transmitted_matrix) const; // 0x00949A70 (:3003)
    bool buffer_delta_compressed_ph(bit_packer& packer, unsigned int mask, quaternion& client_quat, const quaternion& ref_quat, const quaternion& transmitted_quat) const; // 0x00949BD0 (:3042)
    bool buffer_delta_compressed_vector(bit_packer& packer, unsigned int mask, vector& client_vec, const vector& ref_vec, const vector& transmitted_vec) const; // 0x00949530 (:2873)
    bool buffer_delta_compressed_vector2(bit_packer& packer, unsigned int mask, vector2& client_vec2, const vector2& ref_vec2, const vector2& transmitted_vec2) const; // 0x009496A0 (:2899)
    bool apply_compressed_pbh(bit_packer& packer, unsigned int mask, matrix& result_matrix) const; // 0x00948C90 (:2661)
    bool apply_compressed_pbh(bit_packer& packer, unsigned int mask, quaternion& result_quat) const; // 0x00948DC0 (:2686)
    bool apply_compressed_ph(bit_packer& packer, unsigned int mask, float b, matrix& result_matrix) const; // 0x00948F00 (:2711)
    bool apply_compressed_ph(bit_packer& packer, unsigned int mask, float b, quaternion& result_quat) const; // 0x00949020 (:2739)
    bool apply_delta_compressed_pbh(bit_packer& packer, unsigned int mask, matrix& result_matrix, const matrix& ref_matrix) const; // 0x00949150 (:2769)
    bool apply_delta_compressed_pbh(bit_packer& packer, unsigned int mask, quaternion& result_quat, const quaternion& ref_quat) const; // 0x00949250 (:2795)
    bool apply_delta_compressed_ph(bit_packer& packer, unsigned int mask, matrix& result_matrix, const matrix& ref_matrix) const; // 0x00949350 (:2821)
    bool apply_delta_compressed_ph(bit_packer& packer, unsigned int mask, quaternion& result_quat, const quaternion& ref_quat) const; // 0x00949440 (:2847)
    bool apply_delta_compressed_vector(bit_packer& packer, unsigned int mask, vector& result_vec, const vector& ref_vec) const; // 0x00948AA0 (:2608)
    bool apply_delta_compressed_vector2(bit_packer& packer, unsigned int mask, vector2& result_vec2, const vector2& ref_vec2) const; // 0x00948BA0 (:2635)

    // 0x00 vfptr
    unsigned int registration_type; // 0x04
    int update_rate; // 0x08
    const char* name; // 0x0C
    unsigned int num_masks; // 0x10
    mask_info masks[64]; // 0x14
};

// size (<player_data_bundle>) 0xBB0
template<typename T>
class object_state_data
{
public:
    void initialize(); // <player_data_bundle> 0x00967F70 (.h:358)
    object_state_data(); // <player_data_bundle> 0x009A7130

    T base_data; // 0x00
    T transmitted_data[16]; // 0xB0
};

// 0x00942EA0 (.h:212)
inline multi_core_object_registration::multi_core_object_registration()
{
    num_masks = 0;
}

// 0x00942EB0 (.h:213)
inline multi_core_object_registration::~multi_core_object_registration()
{
}

// <player_data_bundle> 0x00967F70 (.h:358): no connection's record holds any data set yet
template<typename T>
inline void object_state_data<T>::initialize()
{
    for (int i = 0; i < 16; i++)
    {
        transmitted_data[i].transmitted_data_set = 0;
    }
}

// <player_data_bundle> 0x009A7130: the bundles construct themselves
template<typename T>
inline object_state_data<T>::object_state_data()
{
}

extern multi_core_object_registration* Object_type_registration_list[12]; // 0x02FEB34C, by object type

bool sort_rop_ptr_elements(const rop_sort_element* lhs, const rop_sort_element* rhs); // 0x00947C60 (:218)
unsigned short multi_core_get_next_reserved_network_handle(); // 0x00947DD0 (:397)
void multi_core_add_reserved_network_handle(unsigned int game_handle); // 0x009B5F10 (:407)
void multi_core_level_init_pre(); // 0x009E39F0 (:420)
void multi_core_object_send_job_pre(const mp_transmit_list& transmit_list); // 0x009B6160 (:599)
void multi_core_destroy_remote_object_no_packet_external(unsigned short net_handle); // 0x009E3AE0 (:783)
void multi_core_migrate_fixup_network_handles(); // 0x009D99C0 (:831)
void multi_core_objects_reinit(bool client_migrate); // 0x009D99D0 (:836)
void multi_core_sim_event_clear(); // 0x009B6240 (:861)
void multi_core_objects_init(); // 0x009B62D0 (:869)
void multi_core_objects_shutdown(); // 0x00947FE0 (:923)
unsigned int multi_core_object_remote_object_count(); // 0x00948010 (:945)
unsigned int multi_core_object_remote_object_count(unsigned int registered_type); // 0x00948030 (:962)
void multi_core_object_register_object_type(unsigned int type, const multi_core_object_registration* object_registration); // 0x00948060 (:979)
unsigned short multi_core_get_next_network_handle(); // 0x009D9A20 (:997)
void multi_core_create_level_load_sync_remote_object(unsigned int _registered_type, unsigned int game_handle); // 0x009D9A70 (:1028)
void multi_core_object_create_remote_object(unsigned int _registered_type, unsigned int game_handle); // 0x009D9AA0 (:1052)
void multi_core_objects_register_level_load_remote_object_local_create_types(int object_type); // 0x009480C0 (:1102)
void multi_core_object_connection_setup(multi_connection* cp); // 0x009D9C60 (:1114)
void multi_core_object_change_type(unsigned int registered_type, unsigned int game_handle); // 0x009D9FB0 (:1176)
void multi_core_object_buffer_destroy_remote_object(unsigned short net_handle); // 0x009DA0E0 (:1204)
void multi_core_object_destroy_buffered_remote_objects(); // 0x009F7120 (:1216)
void multi_core_object_set_allow_destroy_remote_object(bool allow); // 0x009480F0 (:1230)
void multi_core_object_destroy_remote_object(unsigned short net_handle); // 0x009E3B20 (:1241)
void multi_core_object_move_all(); // 0x00948100 (:1301)
void multi_core_object_process_create_packet(unsigned char* packet_data, const sender& from); // 0x009DA120 (:1324)
void multi_core_object_process_destroy_packet(unsigned char* packet_data, const sender& from); // 0x009DA1C0 (:1375)
void multi_core_object_process_update_packet(unsigned char* packet_data, const sender& from); // 0x009E3CD0 (:1426)
void multi_core_object_process_object_free(object* obj); // 0x009DA290 (:1569)
const char* multi_core_object_complex_type_name(unsigned int object_type); // 0x00948150 (:1592)
void multi_core_object_get_state(const remote_object* rop, dc_state_list* dcsl); // 0x00948170 (:1605)
bool multi_core_object_check_dirty(const remote_object* rop, dc_state_list* ref_state, dc_state_list* cur_state); // 0x009481A0 (:1614)
void multi_core_object_free_state(unsigned int game_handle, dc_state_list* dcsl); // 0x009DA2C0 (:1633)
bool multi_core_object_apply_compressed_state(unsigned int game_handle, dc_state_list* reference_state, dc_state_list* compressed_state, dc_state_list** resultant_state, unsigned int network_time); // 0x009DA310 (:1660)
void multi_core_store_object_states(); // 0x009A9A50 (:1673)
void multi_core_cleanup_object_states(); // 0x009481E0 (:1687)
void multi_core_query_space_for_priority(const remote_object* rop, dc_state_list* current_state, dc_state_list* reference_state, unsigned short possible_mask_bits, multi_connection* cp, query_size_results* qs_results); // 0x00948200 (:1706)
unsigned short multi_core_query_possible_mask_bits(remote_object* rop, dc_state_list* current_state, dc_state_list* reference_state, multi_connection* cp); // 0x00948240 (:1716)
bool multi_core_check_full_update(dc_state_list* ref_state, const remote_object* rop, multi_connection* cp); // 0x00948270 (:1729)
void multi_core_notify_transmitted_update_callback(remote_object* rop, dc_state_list* current_state, multi_connection* recipient, unsigned short possible_mask_bits, unsigned short mask_bits_transmitted); // 0x009482A0 (:1736)
void multi_core_object_reset_state_data(); // 0x009482D0 (:1745)
void multi_core_object_free_mp_sim_state(); // 0x009482F0 (:1759)
int multi_core_get_number_of_update_cycles_since_last_reset(); // 0x00948340 (:1785)
void multi_core_reset_number_object_update_cycles(); // 0x00948350 (:1790)
unsigned int multi_core_get_object_updates_for_connection(multi_connection* cp, unsigned long max_size); // 0x009FB490 (:1841)
remote_object* multi_core_get_remote_object_from_network_handle(unsigned short net_handle); // 0x009B65D0 (:2211)
remote_object* multi_core_get_remote_object_from_game_handle(unsigned int game_handle); // 0x009B6610 (:2216)
state_priority_bundle* multi_core_get_state_priority_bundle(const remote_object* rop); // 0x00948390 (:2221)
unsigned int multi_core_game_handle_from_network_handle(unsigned short net_handle, bool silent); // 0x009B6650 (:2227)
unsigned short multi_core_network_handle_from_game_handle(unsigned int game_handle, bool silent); // 0x009B66C0 (:2251)
void multi_core_create_remote_object_external(unsigned int game_handle, unsigned short net_handle, unsigned int remote_type); // 0x009DA380 (:2275)
