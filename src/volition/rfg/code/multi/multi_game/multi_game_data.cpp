// Original: volition/rfg/code/multi/multi_game/multi_game_data.cpp
//
// The player data of each session type (party, transition, game, and the game snapshot taken at match start): 16 slots
// per set in Mp_game_data. A handle is the slot index in the low 16 bits and the session type (the set) in the high
// 16 bits. The server registers players from their join data and sends each slot to the clients in a full or a light
// form (multi_join.cpp); this file reads and writes those forms.
// Part of the multi unity TU (rfg_multi_unity.obj, every multi/ .cpp in path order): the file statics are shared
// by the whole tree, and a shared one is placed in its first user (tools/hdrgen/specs/mp0_stubs.py).

#include "volition/rfg/code/multi/multi_game/multi_game_data.h"
#include "volition/rfg/code/multi/multi_game/modes/multi_game_mode_common.h"
#include "volition/rfg/code/multi/multi_game/multi_game_badge.h"
#include "volition/rfg/code/multi/multi_game/multi_game_character.h"
#include "volition/rfg/code/multi/multi_game/multi_game_classes.h"
#include "volition/rfg/code/multi/multi_game/multi_game_object_backpack.h"
#include "volition/rfg/code/multi/multi_game/multi_game_party.h"
#include "volition/rfg/code/multi/multi_game/multi_game_settings.h"
#include "volition/rfg/code/multi/multi_game/multi_game_xp.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_objects.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_platform_internal.h"
#include "volition/rfg/code/multi/multi_core/include/multi_session.h"
#include "volition/rfg/code/event_logging/gamelog.h"
#include "volition/rfg/code/gamewide/downloadable_content.h"
#include "volition/rfg/code/gamewide/stats.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/vlib/code/os/vconsole.h"
#include "volition/vlib/code/util/unicode_tools.h"
#include "keen/base/stringencoding.hpp"
#include "kaiko/gamelink/gamelink.hpp"

#include <string.h>
#include <wchar.h>

// ---- file-static globals
// unsigned int Local_game_data_handles[4] (0x0167E5F4) is used here too; placed in multi_join.cpp.
static const wchar_t* Long_name = L"WWWWWWWWWWWWWWWW"; // 0x0167E604 (the string at 0x0130931C)
static wchar_t My_player_name[64] = L"Not Parker"; // 0x0167E608
static wchar_t My_player_name_uncompressed[64] = L"Not Parker"; // 0x0167E688
// unsigned char My_level (0x0167F4F8) is used here too; placed in multi_game_badge.cpp.
// bool Spectating (0x02FEB659) is used here too; placed in multi.cpp.
// unsigned int My_badge (0x02FEB66C) is used here too; placed in multi.cpp.
// mp_player_class_info* Default_class_info (0x02FEB694) is used here too; placed in multi_game.cpp.
// mp_game_data_wrapper* Mp_game_data (0x02FEB698) is used here too; placed in multi_connection.cpp.

// Magic numbers: 16 player slots per set; display names hold 64 characters.
static const int Max_game_data_players = 16;
static const int Max_display_name = 64;

// ---- internal functions

// Port helper: rfg.exe inlines this test into every lookup and iteration (multi_game_data_get's rule).
static bool game_data_is_listed(multi_game_data& data, bool include_in_game, bool include_lingering)
{
    return data.in_use || (data.in_game && include_in_game) || (data.linger_time.valid() && include_lingering);
}

// write_u32_be/read_u32_be, write_u16_be/read_u16_be and game_data_of are the port helpers of multi_join.cpp.

// 0x009514B0 (:139). LTCG: session_type in ECX.
static int find_free_index(session_types session_type)
{
    for (int i = 0; i < Max_game_data_players; i++)
    {
        if (!Mp_game_data->data[i][session_type].in_use)
        {
            return i;
        }
    }
    return -1;
}

// 0x009514E0 (:151). LTCG: mg_data in EAX.
static void mg_clear_migration_data(multi_game_data& mg_data)
{
    mg_data.dm_data.last_applied_frame = 0xFFFF;
    mg_data.dm_data.last_sent_frame = 0xFFFF;
    mg_data.dm_data.applied_destruction = false;
    mg_data.dm_data.destroyed_general_movers_recvd = false;
    mg_data.dm_data.last_destruction_frame_recvd = false;
}

// 0x00951510 (:161)
static void multi_game_data_init_migration_data()
{
    for (int set = 0; set < NUM_GAME_DATA_SETS; set++)
    {
        for (int i = 0; i < Max_game_data_players; i++)
        {
            mg_clear_migration_data(Mp_game_data->data[i][set]);
        }
    }
}

// 0x00997640 (:88): console command (unregistered in retail)
void unused_dcf_mp_list_players()
{
    if (multi_session_get_by_type(ST_GAME) == nullptr)
    {
        return;
    }
    for (const multi_game_data* data = multi_game_data_begin(ST_GAME_SNAPSHOT, false, false); data != nullptr;
         data = multi_game_data_next(data, false, false))
    {
        console_printf("%x/%ls (team %ls)\n", data->handle, data->display_name, human_team_get_display_name(data->team));
    }
}

// 0x0097AF30 (:101): console command (unregistered in retail): the longest name in every local slot
void unused_dcf_mp_long_name_local()
{
    for (int set = 0; set < NUM_GAME_DATA_SETS; set++)
    {
        multi_game_data* data = (multi_game_data*)multi_game_data_get(Local_game_data_handles[set], false, false);
        if (data != nullptr)
        {
            _snprintf_safe(data->display_name, Max_display_name, Long_name);
        }
    }
}

// 0x00997710 (:112): console command (unregistered in retail): the longest name in every slot of the party, game and
// snapshot sets
void unused_dcf_mp_long_name_all()
{
    static const session_types sets[] = { ST_PARTY, ST_GAME, ST_GAME_SNAPSHOT };
    for (session_types set : sets)
    {
        for (multi_game_data* data = (multi_game_data*)multi_game_data_begin(set, false, false); data != nullptr;
             data = (multi_game_data*)multi_game_data_next(data, false, false))
        {
            _snprintf_safe(data->display_name, Max_display_name, Long_name);
        }
    }
}

// 0x00997AF0 (:176)
void multi_game_data_system_init()
{
    Mp_game_data = (mp_game_data_wrapper*)MP_allocators_mempool.alloc(sizeof(mp_game_data_wrapper), 4);
    for (int set = 0; set < NUM_GAME_DATA_SETS; set++)
    {
        multi_game_data_clear_players((session_types)set);
    }
    for (int set = 0; set < NUM_GAME_DATA_SETS; set++)
    {
        Local_game_data_handles[set] = 0xFFFFFFFF;
    }
    multi_game_data_init_migration_data();
}

// 0x009AEC70 (:190): our own slot, from our profile. In the game set the team is chosen at match start; in a team
// mode the other sets join the team with fewer players.
unsigned int multi_game_data_register_server_player(session_types session_type, const multi_player_uid& player_uid)
{
    if (session_type >= NUM_GAME_DATA_SETS)
    {
        return 0xFFFFFFFF;
    }
    const int index = find_free_index(session_type);
    if (index == -1)
    {
        return 0xFFFFFFFF;
    }
    multi_game_data& mg_data = Mp_game_data->data[index][session_type];
    mg_data.in_use = true;
    mg_data.handle = (unsigned int)session_type << 16 | (unsigned int)index;
    if (session_type == ST_GAME)
    {
        mg_data.team = HUMAN_TEAM_NONE;
    }
    else if (!multi_gameplay_mode_is_team_mode(Mp_server_options.get_mode()))
    {
        mg_data.team = HUMAN_TEAM_CIVILIAN;
    }
    else
    {
        human_teams team = multi_game_mode_get_disadvantaged_team(session_type, 0xFFFFFFFF);
        if (team == HUMAN_TEAM_NONE || team == HUMAN_TEAM_CIVILIAN)
        {
            team = HUMAN_TEAM_EDF;
        }
        mg_data.team = team;
    }
    mg_data.player_selection_preference = multi_game_character_get_profile_player_selection();
    multi_badge_generate_automatic_fields(My_badge, My_level);
    mg_data.player_badge = My_badge;
    mg_data.backpack_mask = multi_game_backpack_get_loaded_backpack_mask();
    mg_data.dlc_mask = dlc_get_mp_mask();
    wcscpy(mg_data.display_name, multi_game_data_get_my_name());
    wcscpy(mg_data.display_name_uncompressed, multi_game_data_get_my_name_uncompressed());
    if (session_type == ST_PARTY)
    {
        strcpy(mg_data.desired_class, Default_class_info->name);
    }
    else
    {
        strcpy(mg_data.desired_class, multi_game_classes_get_desired_class_name());
    }
    mg_data.player_uid = player_uid;
    if (!multi_game_party_in_party() || !multi_game_party_get_leader(&mg_data.leader_uid))
    {
        mg_data.leader_uid = player_uid;
    }
    mg_data.locale = gamelog_get_locale();
    mg_data.language = gamelog_get_language();
    mg_data.player_level = My_level;
    mg_data.player_network_handle = 0xFFFF;
    mg_data.reported_stats = false;
    mg_data.bagman_dlc_unlocked = Bagman_dlc_unlocked;
    mg_data.completed_single_player = stats_get_stat_value(STATS_GAME_COMPLETED_PROFILE) != 0;
    mg_data.in_game = false;
    mg_data.linger_time.invalidate();
    mg_data.disconnect_reason = CDR_NONE;
    mg_data.desired_weapon_inv_item_info_handle = 0xFFFFFFFF;
    mg_data.desired_hammer_unique_id = multi_game_xp_get_sledge_from_profile();
    mg_clear_migration_data(mg_data);
    return mg_data.handle;
}

// 0x0097AFA0 (:243): a copy of another slot (the level is ours, as in rfg.exe). An unlisted source leaves the new
// slot in use without data (as in rfg.exe).
unsigned int multi_game_data_register_server_player(session_types session_type, unsigned int clone_from_handle)
{
    if (session_type >= NUM_GAME_DATA_SETS)
    {
        return 0xFFFFFFFF;
    }
    const int index = find_free_index(session_type);
    if (index == -1)
    {
        return 0xFFFFFFFF;
    }
    multi_game_data& mg_data = Mp_game_data->data[index][session_type];
    mg_data.in_use = true;
    mg_data.handle = (unsigned int)session_type << 16 | (unsigned int)index;
    const multi_game_data* from = multi_game_data_get(clone_from_handle, false, false);
    if (from == nullptr)
    {
        return 0xFFFFFFFF;
    }
    mg_data.team = from->team;
    mg_data.player_selection_preference = from->player_selection_preference;
    mg_data.player_badge = from->player_badge;
    mg_data.backpack_mask = from->backpack_mask;
    mg_data.dlc_mask = from->dlc_mask;
    wcscpy(mg_data.display_name, from->display_name);
    wcscpy(mg_data.display_name_uncompressed, from->display_name_uncompressed);
    strcpy(mg_data.desired_class, from->desired_class);
    mg_data.desired_weapon_inv_item_info_handle = from->desired_weapon_inv_item_info_handle;
    mg_data.desired_hammer_unique_id = from->desired_hammer_unique_id;
    mg_data.player_uid = from->player_uid;
    mg_data.leader_uid = from->leader_uid;
    mg_data.locale = from->locale;
    mg_data.language = from->language;
    mg_data.player_level = My_level;
    mg_data.player_network_handle = 0xFFFF;
    mg_data.reported_stats = false;
    mg_data.bagman_dlc_unlocked = Bagman_dlc_unlocked;
    mg_data.completed_single_player = stats_get_stat_value(STATS_GAME_COMPLETED_PROFILE) != 0;
    mg_data.in_game = false;
    mg_data.linger_time.invalidate();
    mg_data.disconnect_reason = CDR_NONE;
    mg_clear_migration_data(mg_data);
    return mg_data.handle;
}

// 0x00997B40 (:292): a joining player's slot from its join data (multi_game_data_append_my_data's form)
unsigned int multi_game_data_register_client_player(unsigned char* packet_data, int& packet_offset, session_types session_type)
{
    if (session_type >= NUM_GAME_DATA_SETS || packet_data == nullptr || packet_offset < 0)
    {
        return 0xFFFFFFFF;
    }
    const int index = find_free_index(session_type);
    if (index == -1)
    {
        return 0xFFFFFFFF;
    }
    multi_game_data& mg_data = Mp_game_data->data[index][session_type];
    mg_data.handle = (unsigned int)index | (unsigned int)session_type << 16;
    mg_data.in_use = true;
    const wchar_t* name = (const wchar_t*)(packet_data + packet_offset);
    const unsigned short name_length = (unsigned short)wcslen(name);
    if (name_length < Max_display_name)
    {
        wcscpy(mg_data.display_name_uncompressed, name);
    }
    else
    {
        strncpy_safe(mg_data.display_name_uncompressed, name, Max_display_name);
    }
    packet_offset = (unsigned short)(name_length * 2 + 2 + packet_offset);
    wcscpy(mg_data.display_name, mg_data.display_name_uncompressed);
    unicode_insert_no_break_spaces(mg_data.display_name, Max_display_name);
    strcpy(mg_data.desired_class, Default_class_info->name);
    mg_data.player_selection_preference = read_u32_be(packet_data, packet_offset);
    multi_game_badge_read_from_packet(mg_data.player_badge, packet_data, packet_offset);
    mg_data.backpack_mask = read_u16_be(packet_data, packet_offset);
    mg_data.dlc_mask = read_u32_be(packet_data, packet_offset);
    mg_data.team = (human_teams)read_u32_be(packet_data, packet_offset);
    // A spectator (marauder team) keeps its team; otherwise the server picks it as for its own slot.
    if (mg_data.team != HUMAN_TEAM_MARAUDER)
    {
        if (session_type == ST_GAME)
        {
            mg_data.team = HUMAN_TEAM_NONE;
        }
        human_teams team = mg_data.team;
        if (session_type != ST_GAME)
        {
            if (!multi_gameplay_mode_is_team_mode(Mp_server_options.get_mode()))
            {
                team = HUMAN_TEAM_CIVILIAN;
            }
            else
            {
                if (team == HUMAN_TEAM_CIVILIAN || team == HUMAN_TEAM_NONE || multi_game_mode_team_is_full(session_type, team))
                {
                    team = multi_game_mode_get_disadvantaged_team(session_type, 0xFFFFFFFF);
                }
                if (team == HUMAN_TEAM_NONE || team == HUMAN_TEAM_CIVILIAN)
                {
                    team = HUMAN_TEAM_EDF;
                }
            }
        }
        mg_data.team = team;
    }
    mg_data.desired_weapon_inv_item_info_handle = read_u32_be(packet_data, packet_offset);
    mg_data.desired_hammer_unique_id = (int)read_u32_be(packet_data, packet_offset);
    mg_data.player_uid.read_from_packet(packet_data, packet_offset);
    mg_data.leader_uid.read_from_packet(packet_data, packet_offset);
    mg_data.locale = (int)read_u32_be(packet_data, packet_offset);
    mg_data.language = (int)read_u32_be(packet_data, packet_offset);
    mg_data.player_level = packet_data[packet_offset++];
    mg_data.reported_stats = false;
    mg_data.player_network_handle = 0xFFFF;
    mg_data.bagman_dlc_unlocked = packet_data[packet_offset++] != 0;
    mg_data.completed_single_player = packet_data[packet_offset++] != 0;
    mg_data.in_game = false;
    mg_data.linger_time.invalidate();
    mg_data.disconnect_reason = CDR_NONE;
    mg_clear_migration_data(mg_data);
    return mg_data.handle;
}

// 0x00951570 (:361): the light form (append_player_data_light). LTCG: packet_offset in EAX, player_data in ECX,
// packet_data in ESI.
static void update_player_data_light(unsigned char* packet_data, int& packet_offset, multi_game_data* player_data)
{
    player_data->team = (human_teams)packet_data[packet_offset++];
    player_data->in_game = packet_data[packet_offset++] != 0;
    player_data->in_use = packet_data[packet_offset++] != 0;
    player_data->reported_stats = packet_data[packet_offset++] != 0;
    player_data->player_network_handle = read_u16_be(packet_data, packet_offset);
}

// 0x009515E0 (:376): the full form (append_player_data_full). LTCG: packet_offset in ESI, player_data in EDI.
static void update_player_data_full(unsigned char* packet_data, int& packet_offset, multi_game_data* player_data)
{
    player_data->in_use = packet_data[packet_offset++] != 0;
    const wchar_t* name = (const wchar_t*)(packet_data + packet_offset);
    const unsigned short name_length = (unsigned short)wcslen(name);
    if (name_length < Max_display_name)
    {
        wcscpy(player_data->display_name_uncompressed, name);
    }
    else
    {
        wcsncpy(player_data->display_name_uncompressed, name, Max_display_name);
        player_data->display_name_uncompressed[Max_display_name - 1] = L'\0';
    }
    packet_offset = (unsigned short)(name_length * 2 + 2 + packet_offset);
    wcscpy(player_data->display_name, player_data->display_name_uncompressed);
    unicode_insert_no_break_spaces(player_data->display_name, Max_display_name);
    strcpy(player_data->desired_class, Default_class_info->name);
    player_data->player_selection_preference = read_u32_be(packet_data, packet_offset);
    player_data->player_badge = read_u32_be(packet_data, packet_offset);
    player_data->backpack_mask = read_u16_be(packet_data, packet_offset);
    player_data->dlc_mask = read_u32_be(packet_data, packet_offset);
    player_data->team = (human_teams)read_u32_be(packet_data, packet_offset);
    player_data->player_network_handle = read_u16_be(packet_data, packet_offset);
    player_data->desired_weapon_inv_item_info_handle = read_u32_be(packet_data, packet_offset);
    player_data->desired_hammer_unique_id = (int)read_u32_be(packet_data, packet_offset);
    player_data->player_uid.read_from_packet(packet_data, packet_offset);
    player_data->leader_uid.read_from_packet(packet_data, packet_offset);
    player_data->locale = (int)read_u32_be(packet_data, packet_offset);
    player_data->language = (int)read_u32_be(packet_data, packet_offset);
    player_data->player_level = packet_data[packet_offset++];
    player_data->reported_stats = packet_data[packet_offset++] != 0;
    player_data->bagman_dlc_unlocked = packet_data[packet_offset++] != 0;
    player_data->completed_single_player = packet_data[packet_offset++] != 0;
    player_data->in_game = packet_data[packet_offset++] != 0;
}

// 0x009519E0 (:413): the handle, then the slot's form; the slot takes the handle
unsigned int multi_game_data_update_player_data(unsigned char* packet_data, int& packet_offset, bool light_update)
{
    if (packet_data == nullptr || packet_offset < 0)
    {
        return 0xFFFFFFFF;
    }
    const unsigned int handle = read_u32_be(packet_data, packet_offset);
    multi_game_data* player_data = game_data_of(handle);
    player_data->handle = handle;
    if (light_update)
    {
        update_player_data_light(packet_data, packet_offset, player_data);
    }
    else
    {
        update_player_data_full(packet_data, packet_offset, player_data);
    }
    return handle;
}

// 0x0097B150 (:438)
void multi_game_data_clear_players(session_types session_type)
{
    for (int i = 0; i < Max_game_data_players; i++)
    {
        multi_game_data& data = Mp_game_data->data[i][session_type];
        data.in_use = false;
        data.handle = 0xFFFFFFFF;
        wcscpy(data.display_name, L"");
        wcscpy(data.display_name_uncompressed, L"");
        data.desired_class[0] = '\0';
        data.team = HUMAN_TEAM_NONE;
        data.locale = 0;
        data.language = 0;
        data.player_badge = 0;
        data.dlc_mask = 0;
        data.player_network_handle = 0xFFFF;
        data.reported_stats = false;
        data.in_game = false;
        data.linger_time.invalidate();
        data.disconnect_reason = CDR_NONE;
        data.player_level = 0;
        data.player_uid.invalidate();
        data.leader_uid.invalidate();
        data.player_selection_preference = 0;
        data.desired_weapon_inv_item_info_handle = 0xFFFFFFFF;
        data.desired_hammer_unique_id = 0xFF;
        data.backpack_mask = 0;
        data.timed_out = false;
        mg_clear_migration_data(data);
    }
    Local_game_data_handles[session_type] = 0xFFFFFFFF;
}

// 0x00997FC0 (:475): our join data (read by multi_game_data_register_client_player). Without our slot in this set
// the data comes from the profile; with it, the slot's weapon, hammer and leader.
void multi_game_data_append_my_data(unsigned char* packet_data, unsigned short& packet_size, session_types session_type)
{
    if (packet_data == nullptr)
    {
        return;
    }
    const multi_game_data* my_data = multi_game_data_get(Local_game_data_handles[session_type], false, false);
    const wchar_t* name = multi_game_data_get_my_name_uncompressed();
    const unsigned short name_bytes = (unsigned short)(wcslen(name) * 2 + 2);
    memcpy(packet_data + packet_size, multi_game_data_get_my_name_uncompressed(), name_bytes);
    packet_size = (unsigned short)(packet_size + name_bytes);
    write_u32_be(packet_data, packet_size, multi_game_character_get_profile_player_selection());
    unsigned int badge = 0;
    multi_game_badge_get_my_badge(badge);
    multi_game_badge_write_to_packet(badge, packet_data, packet_size);
    write_u16_be(packet_data, packet_size, multi_game_backpack_get_loaded_backpack_mask());
    write_u32_be(packet_data, packet_size, dlc_get_mp_mask());
    // the team: a spectator asks for the marauders, everyone else for none
    write_u32_be(packet_data, packet_size, Spectating ? (unsigned int)HUMAN_TEAM_MARAUDER : 0xFFFFFFFF);
    const multi_player_uid* leader;
    multi_player_uid party_leader;
    if (my_data == nullptr)
    {
        write_u32_be(packet_data, packet_size, 0xFFFFFFFF);
        write_u32_be(packet_data, packet_size, (unsigned int)multi_game_xp_get_sledge_from_profile());
        My_player_uid.write_to_packet(packet_data, packet_size);
        if (multi_game_party_in_party() && multi_game_party_get_leader(&party_leader))
        {
            leader = &party_leader;
        }
        else
        {
            leader = &My_player_uid;
        }
    }
    else
    {
        write_u32_be(packet_data, packet_size, my_data->desired_weapon_inv_item_info_handle);
        write_u32_be(packet_data, packet_size, (unsigned int)my_data->desired_hammer_unique_id);
        my_data->player_uid.write_to_packet(packet_data, packet_size);
        leader = &my_data->leader_uid;
    }
    leader->write_to_packet(packet_data, packet_size);
    write_u32_be(packet_data, packet_size, (unsigned int)gamelog_get_locale());
    write_u32_be(packet_data, packet_size, (unsigned int)gamelog_get_language());
    packet_data[packet_size++] = My_level;
    packet_data[packet_size++] = Bagman_dlc_unlocked;
    packet_data[packet_size++] = stats_get_stat_value(STATS_GAME_COMPLETED_PROFILE) != 0;
}

// 0x00951AA0 (:532): the handle, team, in game, in use, reported stats and the player's network handle. LTCG:
// packet_size in EAX, packet_data in ECX, player_data in EDI, handle on the stack.
static void append_player_data_light(unsigned char* packet_data, unsigned short& packet_size, multi_game_data* player_data, unsigned int handle)
{
    write_u32_be(packet_data, packet_size, handle);
    packet_data[packet_size++] = (unsigned char)player_data->team;
    packet_data[packet_size++] = player_data->in_game;
    packet_data[packet_size++] = player_data->in_use;
    packet_data[packet_size++] = player_data->reported_stats;
    write_u16_be(packet_data, packet_size, player_data->player_network_handle);
}

// 0x00951B60 (:548): the whole slot. LTCG: handle in ECX, packet_size in ESI, packet_data in EDI, player_data on the
// stack.
static void append_player_data_full(unsigned char* packet_data, unsigned short& packet_size, multi_game_data* player_data, unsigned int handle)
{
    write_u32_be(packet_data, packet_size, handle);
    packet_data[packet_size++] = player_data->in_use;
    const unsigned short name_bytes = (unsigned short)(wcslen(player_data->display_name_uncompressed) * 2 + 2);
    memcpy(packet_data + packet_size, player_data->display_name_uncompressed, name_bytes);
    packet_size = (unsigned short)(packet_size + name_bytes);
    write_u32_be(packet_data, packet_size, player_data->player_selection_preference);
    write_u32_be(packet_data, packet_size, player_data->player_badge);
    write_u16_be(packet_data, packet_size, player_data->backpack_mask);
    write_u32_be(packet_data, packet_size, player_data->dlc_mask);
    write_u32_be(packet_data, packet_size, (unsigned int)player_data->team);
    write_u16_be(packet_data, packet_size, player_data->player_network_handle);
    write_u32_be(packet_data, packet_size, player_data->desired_weapon_inv_item_info_handle);
    write_u32_be(packet_data, packet_size, (unsigned int)player_data->desired_hammer_unique_id);
    player_data->player_uid.write_to_packet(packet_data, packet_size);
    player_data->leader_uid.write_to_packet(packet_data, packet_size);
    write_u32_be(packet_data, packet_size, (unsigned int)player_data->locale);
    write_u32_be(packet_data, packet_size, (unsigned int)player_data->language);
    packet_data[packet_size++] = player_data->player_level;
    packet_data[packet_size++] = player_data->reported_stats;
    packet_data[packet_size++] = player_data->bagman_dlc_unlocked;
    packet_data[packet_size++] = player_data->completed_single_player;
    packet_data[packet_size++] = player_data->in_game;
}

// 0x00951F40 (:581): ST_GAME_SNAPSHOT as the target set keeps the slot's own handle
void multi_game_data_append_player_data(unsigned char* packet_data, unsigned short& packet_size, unsigned int game_data_handle, session_types as_session_type, bool light_update)
{
    if (packet_data == nullptr || !Multiplayer || game_data_handle == 0xFFFFFFFF)
    {
        return;
    }
    multi_game_data* player_data = game_data_of(game_data_handle);
    unsigned int handle;
    if (as_session_type == NUM_GAME_DATA_SETS)
    {
        handle = player_data->handle;
    }
    else
    {
        handle = (game_data_handle & 0xFFFF) | (unsigned int)as_session_type << 16;
    }
    if (light_update)
    {
        append_player_data_light(packet_data, packet_size, player_data, handle);
    }
    else
    {
        append_player_data_full(packet_data, packet_size, player_data, handle);
    }
}

// 0x00951FC0 (:601)
void multi_game_data_set_player_instance_network_handle(unsigned int game_data_handle, unsigned short net_handle)
{
    if (game_data_handle != 0xFFFFFFFF)
    {
        game_data_of(game_data_handle)->player_network_handle = net_handle;
    }
}

// 0x00951FF0 (:617)
void multi_game_data_unset_player_instance_network_handle(unsigned int game_data_handle)
{
    if (game_data_handle != 0xFFFFFFFF)
    {
        game_data_of(game_data_handle)->player_network_handle = 0xFFFF;
    }
}

// 0x00952020 (:632)
unsigned short multi_game_data_get_player_instance_network_handle(unsigned int game_data_handle)
{
    if (game_data_handle != 0xFFFFFFFF && game_data_of(game_data_handle)->in_use)
    {
        return game_data_of(game_data_handle)->player_network_handle;
    }
    return 0xFFFF;
}

// 0x0097B290 (:645): our own team decides whether we spectate
void multi_game_data_set_player_team(unsigned int game_data_handle, human_teams team)
{
    if (game_data_handle == 0xFFFFFFFF)
    {
        return;
    }
    game_data_of(game_data_handle)->team = team;
    if (Local_game_data_handles[game_data_handle >> 16] == game_data_handle)
    {
        Spectating = team == HUMAN_TEAM_MARAUDER;
    }
}

// 0x00952060 (:666)
void multi_game_data_set_player_name(unsigned int game_data_handle, const wchar_t* name)
{
    if (game_data_handle == 0xFFFFFFFF)
    {
        return;
    }
    multi_game_data* data = game_data_of(game_data_handle);
    wcscpy(data->display_name_uncompressed, name);
    wcscpy(data->display_name, name);
    // As in rfg.exe: the no-break spaces go into the uncompressed name.
    unicode_insert_no_break_spaces(data->display_name_uncompressed, Max_display_name);
}

// 0x009520D0 (:681)
void multi_game_data_set_desired_class(unsigned int game_data_handle, const char* class_name)
{
    if (game_data_handle != 0xFFFFFFFF && class_name != nullptr)
    {
        strcpy(game_data_of(game_data_handle)->desired_class, class_name);
    }
}

// 0x00952120 (:693)
void multi_game_data_set_desired_weapon(unsigned int game_data_handle, unsigned int desired_weapon_inv_item_info_handle)
{
    if (game_data_handle != 0xFFFFFFFF)
    {
        game_data_of(game_data_handle)->desired_weapon_inv_item_info_handle = desired_weapon_inv_item_info_handle;
    }
}

// 0x00952150 (:704)
void multi_game_data_set_desired_hammer(unsigned int game_data_handle, int desired_hammer_unique_id)
{
    if (game_data_handle != 0xFFFFFFFF)
    {
        game_data_of(game_data_handle)->desired_hammer_unique_id = desired_hammer_unique_id;
    }
}

// 0x00952180 (:715)
void multi_game_data_set_player_select_bundle(unsigned int game_data_handle, unsigned int bundle)
{
    if (game_data_handle != 0xFFFFFFFF)
    {
        game_data_of(game_data_handle)->player_selection_preference = bundle;
    }
}

// 0x009521B0 (:726)
void multi_game_data_set_badge(unsigned int game_data_handle, const unsigned int& badge)
{
    if (game_data_handle != 0xFFFFFFFF)
    {
        game_data_of(game_data_handle)->player_badge = badge;
    }
}

// 0x009521E0 (:737)
void multi_game_data_set_isMuted(unsigned int game_data_handle, bool isMuted)
{
    if (game_data_handle != 0xFFFFFFFF)
    {
        game_data_of(game_data_handle)->isMuted = isMuted;
    }
}

// 0x00998460 (:748): a snapshot slot's level goes to the player's game slot too
void multi_game_data_set_player_level(unsigned int game_data_handle, unsigned char level)
{
    while (game_data_handle != 0xFFFFFFFF)
    {
        game_data_of(game_data_handle)->player_level = level;
        if (game_data_handle >> 16 != ST_GAME_SNAPSHOT)
        {
            return;
        }
        const multi_game_data* data = multi_game_data_begin(ST_GAME, false, false);
        while (data != nullptr && !data->player_uid.get_internal_id().is_equal(game_data_of(game_data_handle)->player_uid.get_internal_id()))
        {
            data = multi_game_data_next(data, false, false);
        }
        if (data == nullptr)
        {
            return;
        }
        game_data_handle = data->handle;
    }
}

// 0x00952210 (:770)
void multi_game_data_set_lag_timestamp(unsigned int game_data_handle)
{
    if (game_data_handle != 0xFFFFFFFF)
    {
        game_data_of(game_data_handle)->bad_lag_time.set(0);
    }
}

// 0x00952240 (:781)
int multi_game_data_lag_timestamp_time_since(unsigned int game_data_handle)
{
    if (game_data_handle == 0xFFFFFFFF)
    {
        return 0;
    }
    return game_data_of(game_data_handle)->bad_lag_time.time_since();
}

// 0x00952270 (:791)
void multi_game_data_clear_lag_timestamp(unsigned int game_data_handle)
{
    if (game_data_handle != 0xFFFFFFFF)
    {
        game_data_of(game_data_handle)->bad_lag_time.invalidate();
    }
}

// 0x009522A0 (:802)
unsigned int multi_game_data_get_player_select_preferences(unsigned int game_data_handle)
{
    if (game_data_handle == 0xFFFFFFFF)
    {
        return 0;
    }
    return game_data_of(game_data_handle)->player_selection_preference;
}

// 0x009522D0 (:813): a slot in use, or in the game, or lingering after a disconnect, as asked
const multi_game_data* multi_game_data_get(unsigned int game_data_handle, bool include_in_game, bool include_lingering)
{
    if (Mp_game_data == nullptr || game_data_handle == 0xFFFFFFFF)
    {
        return nullptr;
    }
    multi_game_data* data = game_data_of(game_data_handle);
    if (!game_data_is_listed(*data, include_in_game, include_lingering))
    {
        return nullptr;
    }
    return data;
}

// 0x0097B2E0 (:837)
const multi_game_data* multi_game_data_get_my_data(session_types session_type, bool include_in_game, bool include_lingering)
{
    if (Mp_game_data == nullptr || Local_game_data_handles[session_type] == 0xFFFFFFFF)
    {
        return nullptr;
    }
    return multi_game_data_get(Local_game_data_handles[session_type], include_in_game, include_lingering);
}

// 0x00998580 (:851)
const multi_game_data* multi_game_data_get_by_player_uid(session_types session_type, const multi_player_uid& player_uid)
{
    if (Mp_game_data == nullptr)
    {
        return nullptr;
    }
    for (const multi_game_data* data = multi_game_data_begin(session_type, false, false); data != nullptr;
         data = multi_game_data_next(data, false, false))
    {
        if (data->player_uid.get_internal_id().is_equal(player_uid.get_internal_id()))
        {
            return data;
        }
    }
    return nullptr;
}

// 0x00952340 (:868)
void multi_game_data_set_my_handle(unsigned int game_data_handle)
{
    if (game_data_handle != 0xFFFFFFFF)
    {
        Local_game_data_handles[game_data_handle >> 16] = game_data_handle;
    }
}

// 0x00952360 (:875)
unsigned int multi_game_data_get_my_handle(session_types session_type)
{
    return Local_game_data_handles[session_type];
}

// 0x00952370 (:880)
bool multi_game_data_is_my_handle(unsigned int game_data_handle)
{
    if (game_data_handle == 0xFFFFFFFF)
    {
        return false;
    }
    return Local_game_data_handles[game_data_handle >> 16] == game_data_handle;
}

// 0x00998660 (:889)
const wchar_t* multi_game_data_get_my_name()
{
    wcsncpy(My_player_name, multi_game_data_get_my_name_uncompressed(), Max_display_name);
    My_player_name[Max_display_name - 1] = L'\0';
    unicode_insert_no_break_spaces(My_player_name, Max_display_name);
    return My_player_name;
}

// 0x0097B310 (:900): on LAN the name of our own lobby station (the local user name it was created with), else the
// platform's player name
const wchar_t* multi_game_data_get_my_name_uncompressed()
{
    My_player_name_uncompressed[0] = L'\0';
    if (!Multiplayer)
    {
        return My_player_name_uncompressed;
    }
    if (!Multi_syslink)
    {
        multi_core_platform_internal_get_player_name(My_player_name_uncompressed, Max_display_name);
        return My_player_name_uncompressed;
    }
    g_gameLinkMutex.lock(0);
    kaiko::gamelink::GameLink* link = Remote_game_session.getGameLinkSession();
    // Magic number: 16 lobby members at most.
    const kaiko::GameLinkRemoteStation* members[16];
    const unsigned int count = link->getLobbyMembers(members, 16);
    for (unsigned int i = 0; i < count; i++)
    {
        if (members[i]->myStation)
        {
            keen::convertUTF8ToUTF16((uint16*)My_player_name_uncompressed, sizeof(My_player_name_uncompressed), members[i]->userName);
            break;
        }
    }
    g_gameLinkMutex.unlock();
    return My_player_name_uncompressed;
}

// 0x00952390 (:936)
void multi_game_data_remove_player(unsigned int game_data_handle)
{
    if (game_data_handle == 0xFFFFFFFF)
    {
        return;
    }
    game_data_of(game_data_handle)->in_use = false;
    if (game_data_handle == Local_game_data_handles[game_data_handle >> 16])
    {
        Local_game_data_handles[game_data_handle >> 16] = 0xFFFFFFFF;
    }
}

// 0x009523E0 (:950): every slot of a set into another set (with the target set's handles)
void multi_game_data_clone_set(session_types from, session_types to)
{
    for (int i = 0; i < Max_game_data_players; i++)
    {
        Mp_game_data->data[i][to] = Mp_game_data->data[i][from];
        Mp_game_data->data[i][to].handle = (Mp_game_data->data[i][to].handle & 0xFFFF) | (unsigned int)to << 16;
    }
    Local_game_data_handles[to] = (Local_game_data_handles[from] & 0xFFFF) | (unsigned int)to << 16;
}

// 0x00952460 (:960)
void multi_game_data_clone_handle(unsigned int game_data_handle, session_types from, session_types to)
{
    if (game_data_handle == 0xFFFFFFFF)
    {
        return;
    }
    const unsigned int index = game_data_handle & 0xFFFF;
    Mp_game_data->data[index][to] = Mp_game_data->data[index][from];
    Mp_game_data->data[index][to].handle = (Mp_game_data->data[index][to].handle & 0xFFFF) | (unsigned int)to << 16;
}

// 0x009524C0 (:970)
int multi_game_data_count_players(session_types session_type)
{
    int count = 0;
    for (int i = 0; i < Max_game_data_players; i++)
    {
        if (Mp_game_data->data[i][session_type].in_use)
        {
            count++;
        }
    }
    return count;
}

// 0x00952510 (:983)
void multi_game_data_set_leader(unsigned int game_data_handle, const multi_player_uid& leader)
{
    if (game_data_handle != 0xFFFFFFFF)
    {
        game_data_of(game_data_handle)->leader_uid = leader;
    }
}

// 0x00952550 (:994)
const multi_player_uid* multi_game_data_get_leader(unsigned int game_data_handle)
{
    if (game_data_handle == 0xFFFFFFFF)
    {
        return nullptr;
    }
    return &game_data_of(game_data_handle)->leader_uid;
}

// 0x00952580 (:1003)
const multi_player_uid* multi_game_data_get_player_uid(unsigned int game_data_handle)
{
    if (game_data_handle == 0xFFFFFFFF)
    {
        return nullptr;
    }
    return &game_data_of(game_data_handle)->player_uid;
}

// 0x009525B0 (:1015)
int multi_game_data_get_player_index(unsigned int game_data_handle)
{
    if (game_data_handle == 0xFFFFFFFF)
    {
        return -1;
    }
    return (int)(game_data_handle & 0xFFFF);
}

// 0x009525C0 (:1024)
int multi_game_data_get_my_index(session_types session_type)
{
    if (session_type < NUM_GAME_DATA_SETS && Local_game_data_handles[session_type] != 0xFFFFFFFF)
    {
        return (int)(Local_game_data_handles[session_type] & 0xFFFF);
    }
    return -1;
}

// 0x009BDDD0 (:1031): the player object of a slot (through its network handle)
player* multi_game_data_get_player_instance(unsigned int game_data_handle)
{
    if (game_data_handle == 0xFFFFFFFF || !game_data_of(game_data_handle)->in_use)
    {
        return nullptr;
    }
    const unsigned int handle = multi_core_game_handle_from_network_handle(game_data_of(game_data_handle)->player_network_handle, true);
    if (handle == 0xFFFFFFFF)
    {
        return nullptr;
    }
    object* objp = World.object_from_handle(handle);
    if (objp == nullptr || objp->get_obj_type() != OT_HUMAN || objp->get_sub_type() != OT_SUB_HUMAN_PLAYER)
    {
        return nullptr;
    }
    return (player*)objp;
}

// 0x009BDE40 (:1049): an index of the game set
player* multi_game_data_get_player_instance(int player_index)
{
    if (player_index == -1)
    {
        return nullptr;
    }
    return multi_game_data_get_player_instance((unsigned int)(player_index & 0xFFFF) | (unsigned int)ST_GAME << 16);
}

// 0x00998690 (:1060): the player has a connection in the session of the slot's set (the game session for the
// snapshot set)
bool multi_game_data_is_connected(unsigned int game_data_handle)
{
    const multi_game_data* data = multi_game_data_get(game_data_handle, false, false);
    if (data == nullptr)
    {
        return false;
    }
    session_types type = (session_types)(game_data_handle >> 16);
    if (type == ST_GAME_SNAPSHOT)
    {
        type = ST_GAME;
    }
    multi_session* session = multi_session_get_by_type(type);
    if (session == nullptr)
    {
        return false;
    }
    return session->get_player_connection(data->player_uid) != nullptr;
}

// 0x009525E0 (:1084)
void multi_game_data_set_reported_stats(unsigned int game_data_handle)
{
    if (game_data_handle != 0xFFFFFFFF)
    {
        game_data_of(game_data_handle)->reported_stats = true;
    }
}

// 0x00952610 (:1094)
void multi_game_data_set_disconnect_reason(unsigned int game_data_handle, multi_connection_disconnect_reason reason)
{
    if (game_data_handle != 0xFFFFFFFF)
    {
        game_data_of(game_data_handle)->disconnect_reason = reason;
    }
}

// 0x00952640 (:1104)
void multi_game_data_set_linger_time(unsigned int game_data_handle, int linger_time_ms)
{
    if (game_data_handle != 0xFFFFFFFF)
    {
        game_data_of(game_data_handle)->linger_time.set(linger_time_ms);
    }
}

// 0x00952680 (:1114)
void multi_game_data_clear_linger_time(unsigned int game_data_handle)
{
    if (game_data_handle != 0xFFFFFFFF)
    {
        game_data_of(game_data_handle)->linger_time.invalidate();
    }
}

// 0x009526B0 (:1124)
void multi_game_data_reset_in_game(session_types session_type)
{
    if (session_type >= NUM_GAME_DATA_SETS)
    {
        return;
    }
    for (int i = 0; i < Max_game_data_players; i++)
    {
        Mp_game_data->data[i][session_type].in_game = false;
    }
}

// 0x009526E0 (:1133)
void multi_game_data_set_in_game(unsigned int game_data_handle)
{
    if (game_data_handle != 0xFFFFFFFF)
    {
        game_data_of(game_data_handle)->in_game = true;
    }
}

// 0x00952710 (:1143)
void multi_game_data_clear_migration_data(unsigned int game_data_handle)
{
    if (game_data_handle != 0xFFFFFFFF)
    {
        mg_clear_migration_data(*game_data_of(game_data_handle));
    }
}

// 0x00952750 (:1159)
unsigned short multi_game_data_get_last_applied_frame(unsigned int game_data_handle)
{
    return game_data_of(game_data_handle)->dm_data.last_applied_frame;
}

// 0x00952780 (:1169)
unsigned short multi_game_data_get_last_sent_frame(unsigned int game_data_handle)
{
    return game_data_of(game_data_handle)->dm_data.last_sent_frame;
}

// 0x009527B0 (:1178)
bool multi_game_data_get_applied_destruction(unsigned int game_data_handle)
{
    return game_data_of(game_data_handle)->dm_data.applied_destruction;
}

// 0x009527E0 (:1187)
bool multi_game_data_get_general_mover_destroyed_received(unsigned int game_data_handle)
{
    return game_data_of(game_data_handle)->dm_data.destroyed_general_movers_recvd;
}

// 0x00952810 (:1196)
bool multi_game_data_get_last_destruction_frame_received(unsigned int game_data_handle)
{
    return game_data_of(game_data_handle)->dm_data.last_destruction_frame_recvd;
}

// 0x00952840 (:1205)
void multi_game_data_set_last_applied_frame(unsigned int game_data_handle, unsigned short last_applied_frame)
{
    destruction_migration_data& dm_data = game_data_of(game_data_handle)->dm_data;
    dm_data.last_destruction_frame_recvd = true;
    dm_data.last_applied_frame = last_applied_frame;
}

// 0x00952870 (:1215)
void multi_game_data_set_last_sent_frame(unsigned int game_data_handle, unsigned short last_sent_frame)
{
    game_data_of(game_data_handle)->dm_data.last_sent_frame = last_sent_frame;
}

// 0x009528A0 (:1224)
void multi_game_data_set_applied_destruction(unsigned int game_data_handle, bool applied_destruction)
{
    game_data_of(game_data_handle)->dm_data.applied_destruction = applied_destruction;
}

// 0x009528D0 (:1233)
void multi_game_data_set_general_mover_destroyed_received(unsigned int game_data_handle)
{
    game_data_of(game_data_handle)->dm_data.destroyed_general_movers_recvd = true;
}

// 0x0097B3D0 (:1243)
unsigned int multi_game_data_handles_begin(session_types session_type, bool include_in_game, bool include_lingering)
{
    for (int i = 0; i < Max_game_data_players; i++)
    {
        if (game_data_is_listed(Mp_game_data->data[i][session_type], include_in_game, include_lingering))
        {
            return Mp_game_data->data[i][session_type].handle;
        }
    }
    return 0xFFFFFFFF;
}

// 0x0097B460 (:1256)
unsigned int multi_game_data_handles_next(unsigned int player_handle, bool include_in_game, bool include_lingering)
{
    const unsigned int set = player_handle >> 16;
    for (unsigned int i = (player_handle & 0xFFFF) + 1; i < (unsigned int)Max_game_data_players; i++)
    {
        if (game_data_is_listed(Mp_game_data->data[i][set], include_in_game, include_lingering))
        {
            return Mp_game_data->data[i][set].handle;
        }
    }
    return 0xFFFFFFFF;
}

// 0x00952900 (:1272)
unsigned int multi_game_data_handles_end(session_types session_type)
{
    return 0xFFFFFFFF;
}

// 0x0097B500 (:1277)
const multi_game_data* multi_game_data_begin(session_types session_type, bool include_in_game, bool include_lingering)
{
    const unsigned int handle = multi_game_data_handles_begin(session_type, include_in_game, include_lingering);
    if (handle == 0xFFFFFFFF)
    {
        return nullptr;
    }
    return multi_game_data_get(handle, include_in_game, include_lingering);
}

// 0x0097B540 (:1287)
const multi_game_data* multi_game_data_next(const multi_game_data* data, bool include_in_game, bool include_lingering)
{
    if (data == nullptr)
    {
        return nullptr;
    }
    const unsigned int handle = multi_game_data_handles_next(data->handle, include_in_game, include_lingering);
    if (handle == 0xFFFFFFFF)
    {
        return nullptr;
    }
    return multi_game_data_get(handle, include_in_game, include_lingering);
}

// 0x00952910 (:1299)
const multi_game_data* multi_game_data_end(session_types session_type)
{
    return nullptr;
}

// 0x00952920 (:1304)
void multi_game_data_system_shutdown()
{
    Mp_game_data = nullptr;
}
