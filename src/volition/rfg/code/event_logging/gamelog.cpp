// Original: volition/rfg/code/event_logging/gamelog.cpp
//
// The game log (stub_impl.md S5): telemetry events (definitions_events/events.h) sent to a logging server, and the
// queries the events are filled from. In rfg.exe the transport is cut out: send_data is empty, receive_server_data
// never answers and player_name_checksum is 0, and the logger library is Volition's no-op build
// (vlogger_lib/logger_stubs.cpp). So the setup state machine keeps asking for a session every 10 seconds and no event
// is ever recorded; the multiplayer host still sends its logging settings to each client (PT_LOGGING).
// Not ported: the debug commands (unused_dcf_gamelog_dump_crc_maps*).

#include "volition/rfg/code/event_logging/gamelog.h"

#include "volition/rfg/code/event_logging/definitions_events/events.h"
#include "volition/rfg/code/gameplay/activities/activity.h"
#include "volition/rfg/code/gameplay/activities/activity_internal.h"
#include "volition/rfg/code/gameplay/difficulty.h"
#include "volition/rfg/code/gameplay/gameflow/gfm_control_morale.h"
#include "volition/rfg/code/gameplay/missions.h"
#include "volition/rfg/code/gameplay/mission_info.h"
#include "volition/rfg/code/gameplay/object_district.h"
#include "volition/rfg/code/gameplay/wrecking_crew/wrecking_crew.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_core/include/multi_connection.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_io.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_packet_types.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_platform.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_session_base.h"
#include "volition/rfg/code/multi/multi_core/generic/multi_core_session_generic.h"
#include "volition/rfg/code/multi/multi_game/multi_game_data.h"
#include "volition/rfg/code/multi/multi_game/multi_game_lobby.h"
#include "volition/rfg/code/multi/multi_game/multi_game_object_backpack.h"
#include "volition/rfg/code/multi/multi_game/multi_game_settings.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/humans/player_meta_data.h"
#include "volition/rfg/code/objects/inventory_item.h"
#include "volition/rfg/code/objects/movers/object_mover.h"
#include "volition/rfg/code/objects/object_item.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/vehicle/vehicle_info.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/vlib/code/math/crc.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/memory/vmemory.h"
#include "volition/vlib/code/os/frametime.h"
#include "volition/vlib/code/time/timestamp.h"
#include "volition/vlogger_lib/include/logger.h"

#include <string.h>

// ---- file-private types (rfg.pdb)
enum logging_setup_state
{
    LSS_NONE = 0,
    LSS_ENUMERATE_SERVERS = 1,
    LSS_CHOOSE_SERVER = 2,
    LSS_WAITING_FOR_SESSION_ID = 3,
    LSS_READY = 4,
    LSS_WAITING_FOR_NEW_SESSION_ID = 5,
    LSS_RE_READY = 6,
    LSS_FAILED = 7,
    LSS_FAILED_CANNOT_RECOVER = 8,
    NUM_LOGGING_SETUP_STATES = 9,
};

enum logging_receive_status
{
    LRS_NONE = 0,
};

// ---- file-static globals
static int Logging_xmit_interval_ms = 1000; // 0x01651008
static int Logging_event_update_interval_ms = 60000; // 0x0165100C
static timestamp_realtime Logging_xmit_timer; // 0x01651010 (.data -1: invalid)
static int Logging_activity_free_start_time[17]; // 0x022210D0, per activity_modes
static logging_setup_state Logging_state; // 0x02221114
static gamelog_loadout Logging_old_loadout; // 0x02221118
static void (*Lobby_inited_cb)(); // 0x02221128
static char Logging_config_buff[1200]; // 0x02221130
static char Logging_packet_buff[1200]; // 0x02220C20
static char Logging_priorities_memory[256]; // 0x022215E0
static char* Logging_event_memory; // 0x022216E0
static int Logging_initial_init_time; // 0x022216E4
static bool Logging_initd; // 0x022216E8
static bool Logging_init_interrupted; // 0x022216E9
static bool Logging_campaign_logged; // 0x022216EA
static int Logging_init_time; // 0x022216F4
static int Logging_player_index; // 0x022216F8
static int Logging_mission_start_time; // 0x022216FC
static int Logging_vehicle_start_time; // 0x02221704
static int Logging_game_start_time; // 0x02221708
static int Logging_weapon_start_time; // 0x0222170C
static int Logging_end_condition_crc; // 0x02221710
static int Logging_activity_fail_reason; // 0x02221714
static int Logging_mayhem_activity_crc; // 0x02221718
static int Logging_killing_spree_activity_crc; // 0x0222171C
static int Logging_car_bomb_activity_crc; // 0x02221720
static int Player_checksum; // 0x02221724 (never set: player_name_checksum is 0)
static fixed_mempool<16384> Logging_event_mempool("logging events", 1, 0); // 0x02221728

static void send_data(const char* packet_data, int packet_size); // 0x0074F6F0 (:1021)
static void send_session_request(); // 0x0074F700 (:1045)
static logging_receive_status receive_server_data(int& chars_written, char* buffer, int buffer_size); // 0x0074F720 (:1056)
static int player_name_checksum(); // 0x0074F730 (:1091)
static void set_logging_setup_state(logging_setup_state new_state); // 0x0074F740 (:1099)
static void set_loadout_data(human* hp, gamelog_loadout& loadout); // 0x00750630 (:1346)

// 0x0074F6F0 (:1021)
// Empty in rfg.exe: nothing is sent.
static void send_data(const char* packet_data, int packet_size)
{
}

// 0x0074F700 (:1045)
// The request is built and never sent.
static void send_session_request()
{
    unsigned int size = sizeof(Logging_packet_buff);
    log_get_session_request(Logging_packet_buff, size);
}

// 0x0074F720 (:1056)
static logging_receive_status receive_server_data(int& chars_written, char* buffer, int buffer_size)
{
    return LRS_NONE;
}

// 0x0074F730 (:1091)
static int player_name_checksum()
{
    return 0;
}

// 0x0074F740 (:1099)
// LTCG: new_state in EAX. Enumerating and choosing a server pass straight on to waiting for a session id, which
// sends the session request (an interrupted init waits for a new id instead); a ready state starts the logger's
// session and calls the lobby's callback.
static void set_logging_setup_state(logging_setup_state new_state)
{
    Logging_state = new_state;
    for (;;)
    {
        switch (Logging_state)
        {
        case LSS_ENUMERATE_SERVERS:
            Logging_state = LSS_CHOOSE_SERVER;
            break;
        case LSS_CHOOSE_SERVER:
            Logging_state = LSS_WAITING_FOR_SESSION_ID;
            break;
        case LSS_WAITING_FOR_SESSION_ID:
        case LSS_WAITING_FOR_NEW_SESSION_ID:
            if (!Logging_init_interrupted)
            {
                send_session_request(); // inlined in the binary
                Logging_xmit_timer.set(10000); // Magic number: 10 s until the request is repeated
                return;
            }
            Logging_init_interrupted = false;
            Logging_state = LSS_WAITING_FOR_NEW_SESSION_ID;
            break;
        case LSS_READY:
        {
            Logging_initd = true;
            Logging_player_index = 0;
            Logging_init_time = Frametime_total_milliseconds_increment_always;
            Logging_initial_init_time = Frametime_total_milliseconds_increment_always;
            unsigned int size = sizeof(Logging_priorities_memory);
            log_start_session(Logging_priorities_memory, size, Logging_config_buff, 32, 0);
            Logging_xmit_timer.set(Logging_xmit_interval_ms);
            if (Lobby_inited_cb != nullptr)
            {
                Lobby_inited_cb();
            }
            return;
        }
        case LSS_RE_READY:
        {
            Logging_init_time = Frametime_total_milliseconds_increment_always;
            Logging_initial_init_time = Frametime_total_milliseconds_increment_always;
            Logging_player_index = 0;
            unsigned int size = sizeof(Logging_priorities_memory);
            log_start_session(Logging_priorities_memory, size, Logging_packet_buff, 32, 0);
            Logging_xmit_timer.set(Logging_xmit_interval_ms);
            if (Lobby_inited_cb != nullptr)
            {
                Lobby_inited_cb();
            }
            return;
        }
        default:
            return;
        }
    }
}

// 0x0074F880 (:1236)
void process_logging_setup_state()
{
    switch (Logging_state)
    {
    case LSS_NONE:
        set_logging_setup_state(LSS_ENUMERATE_SERVERS);
        return;
    case LSS_ENUMERATE_SERVERS:
        set_logging_setup_state(LSS_CHOOSE_SERVER);
        return;
    case LSS_WAITING_FOR_SESSION_ID:
    case LSS_WAITING_FOR_NEW_SESSION_ID:
        if (Logging_xmit_timer.valid() && !Logging_xmit_timer.elapsed())
        {
            return;
        }
        set_logging_setup_state(LSS_CHOOSE_SERVER);
        return;
    case LSS_READY:
    case LSS_RE_READY:
        if (!Logging_xmit_timer.elapsed())
        {
            return;
        }
        Logging_xmit_timer.set(Logging_xmit_interval_ms);
        log_send_events(Logging_packet_buff, sizeof(Logging_packet_buff));
        return;
    default:
        return;
    }
}

// 0x00750630 (:1346)
// LTCG: loadout in EAX. A player's handle and the weapon crcs of its first three inventory slots.
static void set_loadout_data(human* hp, gamelog_loadout& loadout)
{
    loadout.player_handle = 0xFFFFFFFF;
    if (hp == nullptr || hp->get_obj_type() != OT_HUMAN || hp->get_sub_type() != OT_SUB_HUMAN_PLAYER)
    {
        return;
    }
    loadout.player_handle = hp->get_handle();
    for (int slot = 0; slot < 3; slot++)
    {
        loadout.item_crc[slot] = 0;
        inventory_item* item = human_get_inventory_item(hp, slot);
        if (item == nullptr || item->weapon_handle == 0xFFFFFFFF)
        {
            continue;
        }
        object* objp = World.object_from_handle(item->weapon_handle);
        if (objp != nullptr && objp->get_obj_type() == OT_ITEM && objp->get_sub_type() == OT_SUB_ITEM_WEAPON)
        {
            loadout.item_crc[slot] = gamelog_get_weapon_crc((weapon*)objp); // inlined in the binary
        }
    }
}

// 0x007506B0 (:1379)
void gamelog_system_init()
{
    Logging_mayhem_activity_crc = checksum_stri("ACTIVITY_MODE_MAYHEM", 0).get_crc();
    Logging_killing_spree_activity_crc = checksum_stri("ACTIVITY_MODE_KILLING_SPREE", 0).get_crc();
    Logging_car_bomb_activity_crc = checksum_stri("ACTIVITY_MODE_CAR_BOMB", 0).get_crc();
    Logging_init_interrupted = false;
    gamelog_system_reinit();
}

// 0x0074F910 (:1428)
// The logger's event memory (16 KB) and the session state back to the start.
void gamelog_system_reinit()
{
    int size = 0x4000;
    Logging_event_mempool.clear();
    char* mem = (char*)Logging_event_mempool.alloc(size, 0);
    Logging_state = LSS_NONE;
    Logging_event_memory = mem;
    memset(mem, 0, size);
    // Magic number: 0x49B1 is the events version.
    log_system_init(mem, size, Logging_event_update_interval_ms, 0x49B1);
    Logging_initial_init_time = 0;
    Logging_init_time = 0;
    Logging_initd = false;
    Lobby_inited_cb = nullptr;
    Logging_player_index = -1;
    Logging_old_loadout.player_handle = 0xFFFFFFFF;
}

// 0x0074F9A0 (:1474)
// A lobby the local machine hosts: with the initial settings a known session restarts at once, else a new session is
// asked for (a pending server search is interrupted).
void gamelog_local_reinit(bool initial_settings, void (*initd_cb)())
{
    if (!multi_core_platform_is_connected_to_internet() || Logging_state == LSS_FAILED_CANNOT_RECOVER)
    {
        return;
    }
    log_end_session();
    if (initial_settings)
    {
        if (Logging_initd)
        {
            Logging_player_index = 0;
            Logging_init_time = Logging_initial_init_time;
            unsigned int size = sizeof(Logging_priorities_memory);
            log_start_session(Logging_priorities_memory, size, Logging_config_buff, 32, 0);
            Logging_state = LSS_READY;
        }
        else if (Logging_state != LSS_FAILED_CANNOT_RECOVER)
        {
            Logging_state = LSS_NONE;
        }
    }
    else if (Logging_state != LSS_FAILED_CANNOT_RECOVER)
    {
        if (Logging_state != LSS_ENUMERATE_SERVERS && Logging_state != LSS_CHOOSE_SERVER)
        {
            set_logging_setup_state(LSS_WAITING_FOR_NEW_SESSION_ID);
        }
        else
        {
            Logging_init_interrupted = true;
        }
    }
    Lobby_inited_cb = initd_cb;
}

// 0x0074FA70 (:1518)
// A remote host's lobby: the host's settings arrive by PT_LOGGING (gamelog_process_logging_info).
void gamelog_remote_reinit()
{
    if (multi_core_platform_is_connected_to_internet() && Logging_state != LSS_FAILED_CANNOT_RECOVER)
    {
        log_end_session();
        Logging_state = LSS_FAILED;
    }
}

// 0x0074FAA0 (:1538)
// The setup state, and the timed events advance by the frame's milliseconds.
void gamelog_process()
{
    if (Logging_state == LSS_FAILED_CANNOT_RECOVER)
    {
        return;
    }
    process_logging_setup_state();
    float ms = (float)((double)Frametime_real * 1000.0);
    if ((float)((double)ms - 0.0) < 0.0f)
    {
        ms = 0.0f;
    }
    log_update_continuous_events(fl_round(ms));
}

// 0x0074FB00 (:1555)
int gamelog_get_logging_time()
{
    return Frametime_total_milliseconds_increment_always - Logging_init_time;
}

// 0x00750710 (:1560)
// The local player's logging index, another player's index in the lobby session, else -1.
char gamelog_get_player_index(const human* hp)
{
    if (object_is_local_player(hp))
    {
        return (char)Logging_player_index;
    }
    if (hp != nullptr && hp->get_obj_type() == OT_HUMAN && hp->get_sub_type() == OT_SUB_HUMAN_PLAYER && Lobby_session != nullptr)
    {
        const multi_game_data* data = multi_game_data_get(((const player*)hp)->mp_data, false, false);
        if (data != nullptr)
        {
            multi_connection* cp = Lobby_session->get_player_connection(data->player_uid);
            if (cp != nullptr)
            {
                return (char)cp->index_in_session;
            }
        }
    }
    return -1;
}

// 0x0074FB10 (:1582)
char gamelog_get_player_index(const multi_game_data* data)
{
    if (data != nullptr)
    {
        multi_connection* cp = Lobby_session->get_player_connection(data->player_uid);
        if (cp != nullptr)
        {
            return (char)cp->index_in_session;
        }
    }
    return -1;
}

// 0x00750780 (:1597)
char gamelog_get_player_xp_level(const human* hp)
{
    if (hp != nullptr && hp->get_obj_type() == OT_HUMAN && hp->get_sub_type() == OT_SUB_HUMAN_PLAYER)
    {
        const multi_game_data* data = multi_game_data_get(((const player*)hp)->mp_data, false, false);
        if (data != nullptr)
        {
            return (char)data->player_level;
        }
    }
    return 0;
}

// 0x0074FB30 (:1607)
char gamelog_get_player_xp_level(const player* pp)
{
    if (pp != nullptr)
    {
        const multi_game_data* data = multi_game_data_get(pp->mp_data, false, false);
        if (data != nullptr)
        {
            return (char)data->player_level;
        }
    }
    return 0;
}

// The single player's meta data, none in multiplayer and wrecking crew (inlined in the time functions of the binary).
static player_meta_data* gamelog_sp_meta_data()
{
    if (Multiplayer || wrecking_crew_is_active())
    {
        return nullptr;
    }
    return player_get_meta_data();
}

// 0x0074FB60 (:1621)
int gamelog_get_time_played()
{
    player_meta_data* md = gamelog_sp_meta_data();
    return md != nullptr ? md->get_play_time() : 0;
}

// 0x0074FB90 (:1634)
// The share of the map the EDF no longer controls, in percent.
char gamelog_get_pct_complete()
{
    if (Multiplayer || wrecking_crew_is_active())
    {
        return 0;
    }
    const float edf = gfm_get_edf_control_gamewide_percent();
    return (char)fl_round((float)((1.0 - (double)edf) * 100.0));
}

// 0x0074FBD0 (:1643)
const vector& gamelog_get_human_pos(const human* hp)
{
    if (hp == nullptr)
    {
        return ZERO_VECTOR;
    }
    return hp->pos;
}

// 0x007507C0 (:1652)
int gamelog_get_weapon_crc(const weapon* w_inst)
{
    if (w_inst != nullptr && w_inst->w_info != nullptr)
    {
        return (int)w_inst->w_info->name_crc.get_crc();
    }
    return 0;
}

// 0x0074FBF0 (:1661)
int gamelog_get_weapon_crc(const weapon_info* w_info)
{
    if (w_info == nullptr)
    {
        return 0;
    }
    return (int)w_info->name_crc.get_crc();
}

// 0x007507E0 (:1670)
int gamelog_get_backpack_crc(const multi_object_backpack* backpack)
{
    if (backpack != nullptr && backpack->get_backpack_info() != nullptr)
    {
        return (int)backpack->get_backpack_info()->internal_name_crc.get_crc();
    }
    return 0;
}

// 0x0074FC00 (:1680)
int gamelog_get_backpack_crc(const multi_backpack_info* backpack_info)
{
    if (backpack_info == nullptr)
    {
        return 0;
    }
    return (int)backpack_info->internal_name_crc.get_crc();
}

// 0x00750800 (:1689)
int gamelog_get_item_crc(const item* item_inst)
{
    if (item_inst != nullptr && item_inst->info != nullptr)
    {
        return (int)checksum_stri(item_inst->info->name, 0).get_crc();
    }
    return 0;
}

// 0x0074FC10 (:1698)
int gamelog_get_item_crc(const item_info* i_info)
{
    if (i_info == nullptr)
    {
        return 0;
    }
    return (int)checksum_stri(i_info->name, 0).get_crc();
}

// 0x0074FC30 (:1708)
// By name; the three free activities by their mode names.
int gamelog_get_activity_crc(activity_base* activity)
{
    if (activity == nullptr)
    {
        return 0;
    }
    switch (activity->mode)
    {
    case ACTIVITY_MODE_KILLING_SPREE:
        return Logging_killing_spree_activity_crc;
    case ACTIVITY_MODE_MAYHEM:
        return Logging_mayhem_activity_crc;
    case ACTIVITY_MODE_CAR_BOMB:
        return Logging_car_bomb_activity_crc;
    default:
        return (int)checksum_stri(activity->get_instance_name(), 0).get_crc();
    }
}

// 0x0074FC80 (:1736)
int gamelog_get_mission_crc(mission_info* mission)
{
    if (mission == nullptr)
    {
        return 0;
    }
    return (int)checksum_stri(mission->name, 0).get_crc();
}

// 0x0074FCA0 (:1746)
int gamelog_get_mover_crc(object_mover* mover)
{
    if (mover == nullptr)
    {
        return 0;
    }
    return (int)checksum_stri(mover->get_name(), 0).get_crc();
}

// 0x0074FCD0 (:1756)
int gamelog_get_vehicle_crc(vehicle* obj_vehicle)
{
    if (obj_vehicle == nullptr)
    {
        return 0;
    }
    return (int)checksum_stri(obj_vehicle->info->display_name, 0).get_crc();
}

// 0x0074FD00 (:1766)
int gamelog_get_district_crc(district* obj_district)
{
    if (obj_district == nullptr)
    {
        return 0;
    }
    return (int)checksum_stri(obj_district->get_name(), 0).get_crc();
}

// 0x0074FD30 (:1776)
int gamelog_get_locale()
{
    return 0;
}

// 0x0074FD40 (:1785)
int gamelog_get_language()
{
    return 0;
}

// 0x0074FD50 (:1794)
int gamelog_get_titleid()
{
    return 0;
}

// 0x0074FD60 (:1803)
// The DLC bundle in the high bits, the difficulty in the low three; -1 outside single player.
char gamelog_get_difficulty()
{
    if (Multiplayer || wrecking_crew_is_active())
    {
        return -1;
    }
    const difficulty_levels level = difficulty_get_level();
    return (char)((World.dlc_bundle_id << 3) | ((int)level & 7));
}

// 0x0074FD90 (:1815)
int gamelog_get_player_checksum()
{
    return Player_checksum;
}

// 0x0074FDA0 (:1820)
void gamelog_mark_mission_start_time()
{
    player_meta_data* md = gamelog_sp_meta_data();
    Logging_mission_start_time = md != nullptr ? md->get_play_time() : 0;
}

// 0x0074FDD0 (:1825)
int gamelog_get_mission_duration()
{
    player_meta_data* md = gamelog_sp_meta_data();
    return (md != nullptr ? md->get_play_time() : 0) - Logging_mission_start_time;
}

// 0x0074FE10 (:1830)
void gamelog_set_activity_failed_reason(gamelog_complete_reasons reason)
{
    Logging_activity_fail_reason = reason;
}

// 0x0074FE20 (:1835)
gamelog_complete_reasons gamelog_get_activity_failed_reason()
{
    return (gamelog_complete_reasons)Logging_activity_fail_reason;
}

// 0x0074FE30 (:1840)
void gamelog_mark_activity_start_time(activity_modes mode, bool is_modal)
{
    if ((int)mode <= 16)
    {
        player_meta_data* md = gamelog_sp_meta_data();
        Logging_activity_free_start_time[mode] = md != nullptr ? md->get_play_time() : 0;
    }
    if (is_modal)
    {
        Logging_activity_fail_reason = GLCR_FAILED;
    }
}

// 0x0074FE80 (:1851)
int gamelog_get_activity_duration(activity_modes mode)
{
    if ((int)mode > 16)
    {
        return 0;
    }
    player_meta_data* md = gamelog_sp_meta_data();
    return (md != nullptr ? md->get_play_time() : 0) - Logging_activity_free_start_time[mode];
}

// 0x0074FED0 (:1860)
void gamelog_mark_vehicle_start_time()
{
    player_meta_data* md = gamelog_sp_meta_data();
    Logging_vehicle_start_time = md != nullptr ? md->get_play_time() : 0;
}

// 0x0074FF00 (:1865)
// The time since the vehicle entry, which it clears.
int gamelog_get_vehicle_duration()
{
    player_meta_data* md = gamelog_sp_meta_data();
    const int duration = (md != nullptr ? md->get_play_time() : 0) - Logging_vehicle_start_time;
    Logging_vehicle_start_time = 0;
    return duration;
}

// 0x0074FF50 (:1877)
bool gamelog_has_logged_vehicle_entry()
{
    return Logging_vehicle_start_time != 0;
}

// 0x0074FF60 (:1882)
void gamelog_mark_game_start_time()
{
    Logging_game_start_time = Frametime_total_milliseconds_increment_always - Logging_init_time;
}

// 0x0074FF80 (:1887)
int gamelog_get_game_duration()
{
    return Frametime_total_milliseconds_increment_always - Logging_init_time - Logging_game_start_time;
}

// 0x00750830 (:1893)
void gamelog_store_current_loadout(human* hp)
{
    set_loadout_data(hp, Logging_old_loadout);
}

// 0x00750840 (:1899)
// A player whose loadout differs from the stored one logs the change (the stored loadout stays as it was).
void gamelog_maybe_record_loadout_change(human* hp, gamelog_loadout_change_types change_type)
{
    gamelog_loadout loadout;
    set_loadout_data(hp, loadout);
    if (hp == nullptr || hp->get_obj_type() != OT_HUMAN || hp->get_sub_type() != OT_SUB_HUMAN_PLAYER)
    {
        return;
    }
    if (Logging_old_loadout.player_handle == 0xFFFFFFFF || loadout.player_handle != Logging_old_loadout.player_handle)
    {
        return;
    }
    if (memcmp(loadout.item_crc, Logging_old_loadout.item_crc, sizeof(loadout.item_crc)) == 0)
    {
        return;
    }
    const vector pos = hp->pos;
    const int duration = gamelog_get_game_duration(); // inlined in the binary
    const char difficulty = gamelog_get_difficulty(); // inlined in the binary
    const int time = gamelog_get_logging_time(); // inlined in the binary
    log_event_loadout_change(Player_checksum, time, gamelog_get_player_index(hp), Logging_old_loadout.item_crc[0], Logging_old_loadout.item_crc[1], Logging_old_loadout.item_crc[2], loadout.item_crc[0], loadout.item_crc[1], loadout.item_crc[2], 0, difficulty, duration, pos, (char)change_type);
}

// 0x0074FFA0 (:1919)
// The host's logging time, the client's index and the logger's config profile, big endian, reliably to the client.
void gamelog_send_logging_info(multi_connection* cp)
{
    unsigned char config[1024];
    const unsigned int config_size = (unsigned int)log_get_config_profile((char*)config, sizeof(config));
    unsigned char packet[1200];
    packet[0] = PT_LOGGING;
    unsigned int size = Multi_packets[PT_LOGGING].group_id != 0 ? 5 : 3;
    const unsigned int time = (unsigned int)(Frametime_total_milliseconds_increment_always - Logging_init_time);
    packet[size] = (unsigned char)(time >> 24);
    packet[size + 1] = (unsigned char)(time >> 16);
    packet[size + 2] = (unsigned char)(time >> 8);
    packet[size + 3] = (unsigned char)time;
    packet[size + 4] = cp->index_in_session;
    packet[size + 5] = (unsigned char)(config_size >> 24);
    packet[size + 6] = (unsigned char)(config_size >> 16);
    packet[size + 7] = (unsigned char)(config_size >> 8);
    packet[size + 8] = (unsigned char)config_size;
    size += 9;
    for (int i = 0; i < (int)config_size; i++)
    {
        packet[size & 0xFFFF] = config[i];
        size++;
    }
    const unsigned short payload = (unsigned short)(size - 3);
    packet[1] = (unsigned char)(payload >> 8);
    packet[2] = (unsigned char)payload;
    multi_io_send_reliable(cp, packet, (unsigned short)size, nullptr, nullptr, nullptr);
}

// 0x00750100 (:1939)
// A client takes the host's logging time and its index, and starts its session with the host's config.
void gamelog_process_logging_info(unsigned char* packet_data, const sender& from)
{
    const unsigned int time = ((unsigned int)packet_data[0] << 24) | ((unsigned int)packet_data[1] << 16) | ((unsigned int)packet_data[2] << 8) | packet_data[3];
    const unsigned char index = packet_data[4];
    Logging_player_index = (int)(signed char)index;
    Logging_init_time = Frametime_total_milliseconds_increment_always - (int)time;
    const unsigned int config_size = ((unsigned int)packet_data[5] << 24) | ((unsigned int)packet_data[6] << 16) | ((unsigned int)packet_data[7] << 8) | packet_data[8];
    char config[1024];
    if ((int)config_size > 0)
    {
        memcpy(config, packet_data + 9, config_size);
    }
    unsigned int size = sizeof(Logging_priorities_memory);
    log_start_session(Logging_priorities_memory, size, config, 32, index);
    Logging_state = LSS_RE_READY;
}

// 0x00750230 (:1968)
void gamelog_set_end_condition(const char* end_condition)
{
    Logging_end_condition_crc = (int)checksum_stri(end_condition, 0).get_crc();
}

// 0x00750250 (:1974)
int gamelog_get_end_condition_crc()
{
    return Logging_end_condition_crc;
}

// 0x00750260 (:1980)
// What the player plays: the mission or activity by name in single player, the game mode in multiplayer.
int gamelog_get_player_playing_crc()
{
    if (Multiplayer || wrecking_crew_is_active())
    {
        return (int)Mp_server_options.get_mode();
    }
    if (mission_is_active())
    {
        if (mission_get_name() != nullptr)
        {
            return (int)checksum_stri(mission_get_name(), 0).get_crc();
        }
    }
    else if (!activity_none())
    {
        if (activity_get_current_instance_name() != nullptr)
        {
            return (int)checksum_stri(activity_get_current_instance_name(), 0).get_crc();
        }
    }
    return 0;
}

// 0x007502E0 (:2002)
void gamelog_campaign_start_logged(bool has_been_logged)
{
    Logging_campaign_logged = has_been_logged;
}

// 0x007502F0 (:2007)
bool gamelog_has_campaign_start_been_logged()
{
    return Logging_campaign_logged;
}

// 0x00750300 (:2012)
// The local player's last weapon is logged with how long it was held; the time starts again.
void gamelog_maybe_log_weapon_equipped(player* pp)
{
    if (pp == nullptr || !object_is_local_player(pp))
    {
        return;
    }
    weapon* wp = pp->get_equipped_weapon();
    if (Logging_weapon_start_time != 0 && wp != nullptr)
    {
        const vector pos = pp->pos;
        const int weapon_crc = gamelog_get_weapon_crc(wp); // inlined in the binary
        const int time = gamelog_get_logging_time(); // inlined in the binary
        const int duration = gamelog_get_time_played() - Logging_weapon_start_time;
        const char difficulty = gamelog_get_difficulty();
        const char pct = gamelog_get_pct_complete();
        log_event_weapon_equipped(Player_checksum, time, weapon_crc, gamelog_get_time_played(), pct, 0, difficulty, duration, pos);
        Logging_weapon_start_time = gamelog_get_time_played();
        return;
    }
    player_meta_data* md = gamelog_sp_meta_data();
    Logging_weapon_start_time = md != nullptr ? md->get_play_time() : 0;
}
