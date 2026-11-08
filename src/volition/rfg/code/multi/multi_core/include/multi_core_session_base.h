// Original: volition/rfg/code/multi/multi_core/include/multi_core_session_base.h
// Implementation: volition/rfg/code/multi/multi_core/global/multi_core_session_base.cpp
//
// The session base class: the state machines of creating, joining, starting, modifying and migrating a session, its
// slots (public and private, reserved slots) and invites. multi_session (generic) derives from it. Placement of the
// session enums and slot types inferred.
#pragma once

#include "volition/rfg/code/multi/multi_core/include/multi_player_uid.h"
#include "volition/vlib/code/time/timestamp.h"

struct connection_address;
class multi_connection;
struct sender;

enum session_types
{
    ST_INVALID = -1,
    ST_PARTY = 0,
    ST_TRANSITION = 1,
    ST_GAME = 2,
    NUM_SESSION_TYPES = 3,
    ST_GAME_SNAPSHOT = 3,
    NUM_GAME_DATA_SETS = 4,
};

enum session_invite_type
{
    SIT_NONE = 0,
    SIT_ALL = 1,
    SIT_FRIENDS = 2,
    SIT_PRIVATE = 3,
    NUM_SESSION_INVITE_TYPES = 4,
};

enum session_op_status
{
    SOS_SUCCESS = 0,
    SOS_IN_PROGRESS = 1,
    SOS_FAILED = 2,
    NUM_SESSION_OP_STATUSES = 3,
};

enum multi_session_state
{
    MSS_NONE = 0,
    MSS_CREATING = 1,
    MSS_CREATED = 2,
    MSS_JOINING = 3,
    MSS_JOINED = 4,
    MSS_READY = 5,
    MSS_REGISTERING = 6,
    MSS_REGISTERED = 7,
    MSS_STARTING = 8,
    MSS_IN_GAME = 9,
    MSS_ENDING = 10,
    MSS_ENDED = 11,
    MSS_DELETING = 12,
    MSS_MIGRATING = 13,
    MSS_MODIFYING = 14,
    NUM_MULTI_SESSION_STATES = 15,
};

enum multi_session_creation_state
{
    MSC_NOT_CREATING = 0,
    MSC_WAITING_ON_CREATE = 1,
    MSC_WAITING_FOR_INFO = 2,
    MSC_WAITING_FOR_CLIENTS = 3,
    MSC_CREATE_SUCCESS = 4,
    MSC_CREATE_FAILED = 5,
    NUM_MULTI_SESSION_CREATION_STATES = 6,
};

enum multi_session_join_state
{
    MSJ_NOT_JOINING = 0,
    MSJ_WAITING_FOR_ACCEPT = 1,
    MSJ_WAITING_FOR_CLIENTS = 2,
    MSJ_WAITING_FOR_HOST = 3,
    MSJ_MERGE_WAITING_FOR_CLIENTS = 4,
    MSJ_JOIN_FAILED = 5,
    MSJ_JOIN_COMPLETED = 6,
    NUM_MULTI_SESSION_JOIN_STATES = 7,
};

enum multi_session_join_status
{
    MSJS_INVALID = -1,
    MSJS_JOIN_ACCEPT = 0,
    MSJS_JOIN_DENY_INTERNAL_ERROR = 1,
    MSJS_JOIN_DENY_VERSION = 2,
    MSJS_JOIN_DENY_SESSION_DNE = 3,
    MSJS_JOIN_DENY_SESSION_FULL = 4,
    MSJS_JOIN_DENY_SESSION_LOCKED = 5,
    MSJS_JOIN_DENY_PRIVATE = 6,
    MSJS_JOIN_DENY_BUSY = 7,
    MSJS_JOIN_DENY_TIMEOUT = 8,
    MSJS_JOIN_DENY_DUPLICATE = 9,
    MSJS_JOIN_DENY_DLC_MISSING = 10,
    NUM_MULTI_SESSION_JOIN_STATUSES = 11,
};

enum multi_session_migration_state
{
    MSM_NOT_MIGRATING = 0,
    MSM_MIGRATING_AUTO = 1,
    MSM_BECOMING_HOST = 2,
    MSM_WAITING_FOR_HOST = 3,
    MSM_MIGRATING = 4,
    MSM_WAITING_FOR_CLIENTS = 5,
    MSM_MIGRATION_SUCCESS = 6,
    MSM_MIGRATION_DROPPING_LOST_PEOPLE = 7,
    MSM_MIGRATION_FAILED = 8,
    NUM_MULTI_SESSION_MIGRATE_STATES = 9,
};

enum session_migration_reason
{
    SMR_NONE = 0,
    SMR_HOST_LEFT = 1,
    SMR_HOST_LOST = 2,
    SMR_HOST_REQUESTED = 3,
};

enum multi_session_modification_state
{
    MSD_NOT_MODIFYING = 0,
    MSD_MODIFYING = 1,
    MSD_WAITING_FOR_CLIENTS = 2,
    MSD_MODIFY_SUCCESS = 3,
    MSD_MODIFY_DROPPING_LOST_PEOPLE = 4,
    MSD_MODIFY_FAILED = 5,
    NUM_MULTI_SESSION_MODIFICATION_STATES = 6,
};

enum multi_session_starting_state
{
    MSSS_HOST_WAITING_FOR_CLIENTS_TO_REGISTER = 0,
    MSSS_HOST_DROPPING_NON_REGISTERED_PLAYERS = 1,
    MSSS_REGISTERING = 2,
    MSSS_CLIENT_WAITING_FOR_HOST_TO_SIGNAL_START = 3,
    MSSS_STARTING = 4,
    MSSS_WAITING_FOR_CLIENTS_TO_START = 5,
    MSSS_HOST_DROPPING_NON_STARTED_PLAYERS = 6,
    MSSS_SUCCESS = 7,
    MSSS_FAILED = 8,
    NUM_MULTI_SESSION_STARTING_STATES = 9,
};

// size 0x16
struct slot_reservation
{
    multi_player_uid reserver; // 0x00
    multi_player_uid reservee; // 0x0B
};

// size 0x168
struct slot_use_data
{
    int used; // 0x00
    slot_reservation reservations[16]; // 0x04
    int reserved; // 0x164
};

// size 0x28
class multi_session_info_base
{
public:
    multi_session_info_base(const multi_session_info_base& src); // 0x009909F0 (:270)
    multi_session_info_base(); // 0x009AA310 (:265)
    virtual ~multi_session_info_base(); // vtbl+0x00 0x005D0E00 (.h:229)
    virtual void init(); // vtbl+0x04 0x00990A10 (:275)
    void copy(const multi_session_info_base* src); // 0x0094B000 (:324)
    virtual void copy(const multi_session_info_base& src); // vtbl+0x08 0x00972E00 (:299)
    bool is_valid() const; // 0x00972E80 (:329)
    virtual int compare_by_id(const multi_session_info_base&) const = 0; // vtbl+0x0C
    virtual void append_migration_info_to_packet(unsigned char*, unsigned short&) const = 0; // vtbl+0x10
    virtual void read_migration_info_from_packet(unsigned char*, int&) = 0; // vtbl+0x14
    virtual void append_modification_info_to_packet(unsigned char*, unsigned short&) const = 0; // vtbl+0x18
    virtual void read_modification_info_from_packet(unsigned char*, int&) = 0; // vtbl+0x1C
    virtual void append_join_info_to_packet(unsigned char*, unsigned short&) const = 0; // vtbl+0x20
    virtual void read_join_info_from_packet(const unsigned char*, int&) = 0; // vtbl+0x24
    virtual void append_id_to_packet(unsigned char*, unsigned short&) const = 0; // vtbl+0x28
    virtual void read_id_from_packet(unsigned char*, int&) = 0; // vtbl+0x2C
    virtual void append_arbitration_info_to_packet(unsigned char*, unsigned short&) const = 0; // vtbl+0x30
    virtual void read_arbitration_info_from_packet(unsigned char*, int&) = 0; // vtbl+0x34
    multi_session_info_base& operator=(const multi_session_info_base& __that); // 0x0088A210

    // 0x00 vfptr
    multi_player_uid host_player_uid; // 0x04
    session_types type; // 0x10
    int max_slots; // 0x14
    bool private_match; // 0x18
    bool uses_presence; // 0x19
    bool uses_peer_to_peer_networking; // 0x1A
    bool uses_abitration; // 0x1B
    bool advertise_for_matchmaking; // 0x1C
    bool reports_stats; // 0x1D
    bool systemlink_or_lan; // 0x1E
    bool invited; // 0x1F
    bool dedicated; // 0x20
    bool allow_join_in_progress; // 0x21
    bool party_lobby; // 0x22
    session_invite_type invite_type; // 0x24
};

// 0x005D0E00 (.h:229)
inline multi_session_info_base::~multi_session_info_base()
{
}

// size 0x590
class multi_session_base
{
public:
    multi_session_base(multi_session_info_base* current, multi_session_info_base* desired); // 0x009AA350 (:340)
    virtual ~multi_session_base(); // vtbl+0x00 0x0094B010 (:346)
    void session_create(const multi_session_info_base* info, multi_session_base* create_source); // 0x009FDC80 (:459)
    void session_join(const multi_session_info_base* info, multi_session_base* join_source, bool reservedSlot); // 0x009FDCC0 (:478)
    void session_migrate(multi_connection* new_host, session_migration_reason reason); // 0x009FDD10 (:504)
    void session_start_arbitration(); // 0x009FBBD0 (:535)
    void session_start_gameplay(); // 0x009B6A70 (:555)
    void session_end_gameplay(); // 0x009FBC00 (:564)
    void session_delete(); // 0x009FBCB0 (:581)
    void session_modify(const multi_session_info_base* info); // 0x0094B020 (:588)
    void session_lock(); // 0x0094B030 (:602)
    void session_unlock(bool bSetlobbyTypePublic); // 0x0094B040 (:607)
    bool is_locked(); // 0x009419E0 (.h:306)
    bool operation_in_progress(); // 0x0094B050 (:613)
    void cancel_current_operation(); // 0x009FBCD0 (:632)
    multi_session_state get_state() const; // 0x0094B2D0 (:2798)
    session_migration_reason get_migration_reason() const; // 0x0094B2E0 (:2803)
    void register_migration_callbacks(bool (*migrate_allow)(multi_session_base*), void (*migration_starting)(multi_session_base*), void (*migration_ended)(multi_session_base*), void (*migration_failed)(multi_session_base*)); // 0x0094B330 (:2828)
    void register_join_callbacks(multi_session_join_status (*player_may_join)(multi_connection*, int), void (*player_joined)(multi_connection*), void (*player_about_to_leave)(multi_connection*), void (*player_left)(multi_connection*)); // 0x0094B360 (:2836)
    void register_event_callbacks(void (*send_join_new_session)(unsigned char*, unsigned short&, const multi_session_info_base*), void (*recv_join_new_session)(unsigned char*, int&, const multi_session_info_base*)); // 0x0094B390 (:2844)
    bool reserve_slots(int num_slots, const multi_player_uid& reserver, const multi_player_uid* reservees); // 0x00973190 (:2850)
    void unreserve_slots(const multi_player_uid& uid); // 0x00973350 (:2907)
    int get_slots_used() const; // 0x0094B3C0 (:2922)
    int get_slots_reserved() const; // 0x0094B3B0 (:2917)
    int get_used_slot_information(int* num_used_slots, multi_player_uid* slot_player_uids, multi_player_uid* slot_group_uids, const int max_slots) const; // 0x00973400 (:2927)
    int count_private_slots_in_use() const;
    int count_public_slots_in_use() const;
    const multi_session_info_base* get_info() const; // 0x0094B440 (:3265)
    multi_connection* get_connection_list() const; // 0x0094B450 (:3270)
    multi_connection* get_host_connection() const; // 0x0094B460 (:3275)
    multi_connection* get_me_connection() const; // 0x0094B470 (:3280)
    multi_connection* get_next_host_connection() const; // 0x0094B480 (:3285)
    multi_connection* get_player_connection(unsigned int multi_game_data_handle) const; // 0x0094B490 (:3302)
    multi_connection* get_player_connection(const multi_player_uid& uid) const; // 0x00973500 (:3291)
    multi_connection* get_connection_by_index(unsigned char index) const; // 0x0094B4C0 (:3313)
    multi_session_join_status get_last_join_status() const; // 0x0094B4F0 (:3324)
    bool is_connected() const; // 0x0094B3D0 (:3224)
    bool am_i_host() const; // 0x0094B410 (:3255)
    bool am_i_next_host() const; // 0x0094B420 (:3260)
    int compare_by_id(const multi_session_info_base* info); // 0x0094B500 (:3337)
    void force_migration_host_update(); // 0x0094B650 (:3986)
    void debug_spew_STOP_GETTING_ON_MY_NERVES(const char*, ...) const; // 0x009419F0 (.h:351)
    void init(bool clear_persistent_members); // 0x00990A40 (:351)
    multi_session_creation_state get_create_state() const; // 0x0094B2F0 (:2808)
    multi_session_join_state get_join_state() const; // 0x0094B310 (:2818)
    multi_session_migration_state get_migration_state() const; // 0x0094B300 (:2813)
    multi_session_modification_state get_modification_state() const; // 0x0094B320 (:2823)
    void remove_reservation(int reservation_idx); // 0x009732E0 (:2895)
    multi_session_join_status add_connection(multi_connection* cp, unsigned char manually_assigned_index, int reservations); // 0x009AA3A0 (:2957)
    void register_connection(multi_connection* cp); // 0x00990CB0 (:3093)
    void remove_connection(multi_connection* cp); // 0x009DAC80 (:3122)
    void unregister_connection(multi_connection* cp, bool unregistering_me_connection); // 0x00990D20 (:3209)
    void process(); // 0x009FC040 (:3702)
    void received_connection_left(multi_connection* cp); // 0x009DAEA0 (:3342)
    void received_new_connection(multi_connection* cp); // 0x009AA680 (:3358)
    void join_received_join_accept(multi_connection* cp); // 0x009FBFE0 (:3363)
    void join_received_join_denied(multi_connection* cp, multi_session_join_status reason); // 0x0094B510 (:3376)
    void join_received_join_success(multi_connection* cp); // 0x0094B530 (:3387)
    void join_received_join_failed(multi_connection* cp); // 0x0094B540 (:3405)
    void join_received_join_complete(multi_connection* cp); // 0x0094B550 (:3417)
    void join_received_merged(multi_connection* cp); // 0x0094B560 (:3430)
    void join_sending_join_new_target(unsigned char* packet_data, unsigned short& packet_size, const multi_session_info_base* target_session_info); // 0x0094B570 (:3442)
    void join_received_join_new_session(unsigned char* packet_data, int& packet_offset, multi_connection* cp, const multi_session_info_base* session_info); // 0x0094B5A0 (:3451)
    void join_received_info_response(const multi_session_info_base* responded_info); // 0x009E5280 (:3461)
    void join_lost_connection(multi_connection* join_source_cp); // 0x0094B5D0 (:3471)
    void join_added_connection(multi_connection* cp); // 0x0094B5E0 (:3480)
    void migrate_received_force_migrate(multi_connection* next_host, session_migration_reason reason); // 0x009F7C20 (:3498)
    void migrate_received_migrate(multi_connection* cp, const multi_session_info_base* migration_info, session_migration_reason reason); // 0x00973540 (:3507)
    void migrate_received_migrated(multi_connection* cp); // 0x00973590 (:3521)
    void migrate_received_next_server(multi_connection* cp); // 0x0094B5F0 (:3533)
    void modify_received_modify(const multi_session_info_base* modification_info); // 0x0094B600 (:3542)
    void modify_received_modified(multi_connection* cp); // 0x009735E0 (:3549)
    void starting_received_register(unsigned char* packet_data, multi_connection* cp); // 0x009B6BE0 (:3559)
    void starting_received_registered(multi_connection* cp); // 0x00973620 (:3566)
    void starting_received_started(multi_connection* cp); // 0x00973660 (:3576)
protected:
    void set_state(multi_session_state new_state); // 0x009F7340 (:904)
    session_op_status set_creation_state(multi_session_creation_state new_state); // 0x009E4100 (:1256)
    session_op_status set_migration_state(multi_session_migration_state new_state); // 0x009DA520 (:2263)
    session_op_status set_join_state(multi_session_join_state new_state); // 0x009DA3A0 (:2028)
    session_op_status set_modification_state(multi_session_modification_state new_state); // 0x009DA870 (:2492)
    session_op_status set_starting_state(multi_session_starting_state new_state); // 0x009DA9F0 (:2666)
    session_op_status process_creating(); // 0x009F7980 (:1187)
    session_op_status process_migrating(); // 0x009E4840 (:2171)
    session_op_status process_joining(); // 0x009E45A0 (:1556)
    session_op_status process_modification(); // 0x009E4CF0 (:2443)
    session_op_status process_starting(); // 0x009E4EC0 (:2571)
    void process_state(); // 0x009FBD30 (:648)
    virtual session_op_status platform_session_create() = 0; // vtbl+0x04
    virtual session_op_status platform_session_join() = 0; // vtbl+0x08
    virtual session_op_status platform_session_start_arbitration() = 0; // vtbl+0x0C
    virtual session_op_status platform_session_start_gameplay() = 0; // vtbl+0x10
    virtual session_op_status platform_session_end_gameplay() = 0; // vtbl+0x14
    virtual session_op_status platform_session_delete() = 0; // vtbl+0x18
    virtual session_op_status platform_session_modify() = 0; // vtbl+0x1C
    virtual session_op_status platform_session_migrate() = 0; // vtbl+0x20
    virtual bool platform_session_lock() = 0; // vtbl+0x24
    virtual bool platform_session_unlock(bool) = 0; // vtbl+0x28
    virtual session_op_status platform_process_operation() = 0; // vtbl+0x2C
    virtual session_op_status platform_get_operation_status() = 0; // vtbl+0x30
    virtual void platform_cancel_current_operation() = 0; // vtbl+0x34
    virtual session_op_status platform_get_my_connection_address(connection_address&, bool) = 0; // vtbl+0x38
    virtual session_op_status platform_get_host_connection_address(connection_address&) = 0; // vtbl+0x3C
    virtual session_op_status platform_add_connection(multi_connection*) = 0; // vtbl+0x40
    virtual session_op_status platform_remove_connection(multi_connection*) = 0; // vtbl+0x44
    virtual int platform_compare_by_id(const multi_session_info_base*) = 0; // vtbl+0x48
public:
    bool is_party_owner_and_has_other_party_members(); // 0x0094B090 (:885)
    bool are_all_party_members_in_game_lobby(); // 0x0094B130 (:1711)
    const slot_use_data& get_slot_use_data() const; // 0x00941A00 (.h:571)
    void set_session_info(const multi_session_info_base& sessionInfo); // 0x0094B270 (:1745)
    void check_modified_table_hash(); // 0x009F7AB0 (:1751)
    bool is_allowed_to_change_lock_state(); // 0x009736B0 (:3609)
    bool does_game_state_allow_joining(); // 0x009736F0 (:3629)
    void update_lock_state(bool ignoreCurrentLockState); // 0x00973770 (:3667)
    void set_party_lobby_invite_type(session_invite_type inviteType); // 0x009B6A90 (:1829)
    session_invite_type get_party_lobby_invite_type() const; // 0x0094B2A0 (:1891)
    void process_invite_type_packet(unsigned char* packet_data, const sender& from); // 0x0094B2B0 (:1896)
private:
    virtual void platform_session_update() = 0; // vtbl+0x4C
    void reset_connection_first_transmission_time(multi_connection* pConnection); // 0x00990C40 (:1908)
    void disable_party_if_solo_playlist(); // 0x00972EB0 (:1922)
    void set_extra_lobby_data_for_nerio(); // 0x00973000 (:1980)

    // 0x00 vfptr
protected:
    multi_session_info_base* const m_current_info_base; // 0x04
    multi_session_info_base* const m_desired_info_base; // 0x08
    multi_session_state m_last_state; // 0x0C
    multi_session_state m_state; // 0x10
    multi_session_creation_state m_create_state; // 0x14
    multi_session_join_state m_join_state; // 0x18
    multi_session_migration_state m_migration_state; // 0x1C
    session_migration_reason m_migration_reason; // 0x20
    multi_session_modification_state m_modification_state; // 0x24
    multi_session_starting_state m_starting_state; // 0x28
    slot_use_data m_slots; // 0x2C
    bool m_host; // 0x194
    multi_connection* m_connections; // 0x198
    multi_connection* m_host_connection; // 0x19C
    multi_connection* m_me_connection; // 0x1A0
    bool m_locked; // 0x1A4
    multi_session_base* m_create_source; // 0x1A8
    multi_player_uid m_create_players_acked_uids[16]; // 0x1AC
    int m_create_players_acked_count; // 0x25C
    int m_create_info_retry_count; // 0x260
    timestamp_realtime m_create_timeout; // 0x264
    multi_session_join_status m_join_last_status; // 0x268
    multi_session_base* m_join_source; // 0x26C
    multi_player_uid m_join_players_acked_uids[16]; // 0x270
    int m_join_players_acked_count; // 0x320
    timestamp_realtime m_join_timeout; // 0x324
    bool m_join_invited; // 0x328
    bool m_join_is_merge; // 0x329
    bool m_joinReservedSlot; // 0x32A
    bool m_migrate_packet_received; // 0x32B
    multi_connection* m_migrate_target_host; // 0x32C
    timestamp_realtime m_migrate_timeout; // 0x330
    multi_player_uid m_migrate_players_acked_uids[16]; // 0x334
    int m_migrate_players_acked_count; // 0x3E4
    timestamp_realtime m_migration_choose_server_timestamp; // 0x3E8
    multi_connection* m_migration_next_host; // 0x3EC
    timestamp_realtime m_modification_timeout; // 0x3F0
    multi_player_uid m_modification_acked_uids[16]; // 0x3F4
    int m_modification_acked_count; // 0x4A4
    bool m_starting_received_start; // 0x4A8
    timestamp_realtime m_starting_timeout; // 0x4AC
    multi_player_uid m_starting_acked_uids[16]; // 0x4B0
    int m_starting_acked_count; // 0x560
    bool (*m_cb_migarate_allow_migrate)(multi_session_base*); // 0x564
    void (*m_cb_migrate_about_to_start)(multi_session_base*); // 0x568
    void (*m_cb_migration_just_occurred)(multi_session_base*); // 0x56C
    void (*m_cb_migration_failed)(multi_session_base*); // 0x570
    void (*m_cb_send_join_new_session)(unsigned char*, unsigned short&, const multi_session_info_base*); // 0x574
    void (*m_cb_recv_join_new_session)(unsigned char*, int&, const multi_session_info_base*); // 0x578
    multi_session_join_status (*m_cb_player_may_join)(multi_connection*, int); // 0x57C
    void (*m_cb_player_joined)(multi_connection*); // 0x580
    void (*m_cb_player_about_to_leave)(multi_connection*); // 0x584
    void (*m_cb_player_left)(multi_connection*); // 0x588
private:
    session_invite_type m_partyLobbyInviteType; // 0x58C
};

void session_connection_dropped(multi_connection* cp); // 0x00990950 (:244)
void multi_send_goodbye(multi_connection* cp); // 0x009AA6A0 (:3995)
void multi_process_goodbye(unsigned char* packet_data, const sender& from); // 0x0094B670 (:4004)
void multi_session_send_data_request(multi_connection* cp, const multi_session_info_base* session_info); // 0x009AA730 (:4012)
void multi_session_process_data_request(unsigned char* packet_data, const sender& from); // 0x009DAEE0 (:4021)
void multi_session_send_data_response(multi_connection* cp, const multi_session_info_base* request_session_info, const multi_session_info_base* response_session_info); // 0x009B6C20 (:4043)
void multi_session_process_data_response(unsigned char* packet_data, const sender& from); // 0x009E52B0 (:4061)
void multi_send_force_migrate_packet(multi_connection* new_host, multi_connection* next_host, session_migration_reason reason); // 0x009B6D00 (:4082)
void multi_process_force_migrate_packet(unsigned char* packet_data, const sender& from); // 0x009F7C50 (:4098)
void multi_send_migrate_packet(multi_connection* cp, const multi_session_info_base* migrated_info, session_migration_reason reason); // 0x009B6DC0 (:4119)
void multi_process_migrate_packet(unsigned char* packet_data, const sender& from); // 0x009B6FA0 (:4136)
void multi_send_migrated_packet(multi_connection* cp); // 0x009B7200 (:4182)
void multi_process_migrated_packet(unsigned char* packet_data, const sender& from); // 0x009737F0 (:4190)
void multi_process_next_server_packet(unsigned char* packet_data, const sender& from); // 0x0094B690 (:4197)
void multi_send_modify_session_packet(multi_connection* cp, const multi_session_info_base* modified_info); // 0x009B7260 (:4203)
void multi_process_modify_session_packet(unsigned char* packet_data, const sender& from); // 0x009AA7D0 (:4212)
void multi_send_modified_session_packet(multi_connection* cp); // 0x009B7300 (:4224)
void multi_process_modified_session_packet(unsigned char* packet_data, const sender& from); // 0x00973840 (:4232)
void multi_send_register_for_arbitration(multi_connection* cp); // 0x009B7360 (:4238)
void multi_process_register_for_arbitration(unsigned char* packet_data, const sender& from); // 0x009B7400 (:4247)
void multi_send_registered_for_arbitration(multi_connection* cp); // 0x009B7450 (:4253)
void multi_process_registered_for_arbitration(unsigned char* packet_data, const sender& from); // 0x00973890 (:4261)
void multi_send_start(multi_connection* cp); // 0x009B74B0 (:4267)
void multi_process_start(unsigned char* packet_data, const sender& from); // 0x009B7510 (:4275)
void multi_send_started(multi_connection* cp); // 0x009B7540 (:4281)
void multi_process_started(unsigned char* packet_data, const sender& from); // 0x009738E0 (:4289)
void multi_send_end(multi_connection* cp); // 0x009B75A0 (:4295)
void multi_process_end(unsigned char* packet_data, const sender& from); // 0x009FC3A0 (:4303)
void multi_nat_debug_render(); // 0x0094B6B0 (:4310)

// Port deviation (multiplayer.md MP4; no rfg.exe counterpart): rfg.exe cannot start a system link match. The LAN
// lobby is created without filters (multi_session::platform_session_create), the game start sets the match state
// filter (platform_session_start_gameplay), and from then on check_modified_table_hash finds no build tag in the
// filters and deletes the session, on the host and on every client (whose lobby copies the filters). When set, a
// system link session skips that check. false (the default) keeps the original behaviour; the lobby driver of the
// multiplayer programs sets it.
extern bool g_openrfgSkipSystemLinkTableHashCheck;
