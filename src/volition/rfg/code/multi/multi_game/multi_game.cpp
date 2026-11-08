// Original: volition/rfg/code/multi/multi_game/multi_game.cpp
//
// The multiplayer game layer: its start-up inside multi_init (the systems and the multiplayer tables: settings,
// classes, characters, overrides, the multiplayer weapon table), the level hooks of the gameplay state, the per
// network frame process, text chat and remote console, the server's spawn rotation, the building damage history,
// the deferred broadphase repositioning, migration callbacks and join in progress.
// Part of the multi unity TU (rfg_multi_unity.obj, every multi/ .cpp in path order): the file statics are shared
// by the whole tree, and a shared one is placed in its first user (tools/hdrgen/specs/mp0_stubs.py).

#include "volition/rfg/code/multi/multi_game/multi_game.h"
#include "volition/rfg/code/multi/multi_game/modes/multi_game_mode_common.h"
#include "volition/rfg/code/multi/multi_game/multi_game_announcer.h"
#include "volition/rfg/code/multi/multi_game/multi_game_badge.h"
#include "volition/rfg/code/multi/multi_game/multi_game_callbacks.h"
#include "volition/rfg/code/multi/multi_game/multi_game_character.h"
#include "volition/rfg/code/multi/multi_game/multi_game_classes.h"
#include "volition/rfg/code/multi/multi_game/multi_game_client_predict.h"
#include "volition/rfg/code/multi/multi_game/multi_game_data.h"
#include "volition/rfg/code/multi/multi_game/multi_game_destruction.h"
#include "volition/rfg/code/multi/multi_game/multi_game_friend.h"
#include "volition/rfg/code/multi/multi_game/multi_game_lobby.h"
#include "volition/rfg/code/multi/multi_game/multi_game_migration.h"
#include "volition/rfg/code/multi/multi_game/multi_game_object_backpack.h"
#include "volition/rfg/code/multi/multi_game/multi_game_object_items.h"
#include "volition/rfg/code/multi/multi_game/multi_game_object_marker.h"
#include "volition/rfg/code/multi/multi_game/multi_game_object_flag.h"
#include "volition/rfg/code/multi/multi_game/multi_game_object_movers.h"
#include "volition/rfg/code/multi/multi_game/multi_game_object_player.h"
#include "volition/rfg/code/multi/multi_game/multi_game_object_projectile.h"
#include "volition/rfg/code/multi/multi_game/multi_game_object_rfg_movers.h"
#include "volition/rfg/code/multi/multi_game/multi_game_overrides.h"
#include "volition/rfg/code/multi/multi_game/multi_game_player.h"
#include "volition/rfg/code/multi/multi_game/multi_game_preferences.h"
#include "volition/rfg/code/multi/multi_game/multi_game_render.h"
#include "volition/rfg/code/multi/multi_game/multi_game_respawn.h"
#include "volition/rfg/code/multi/multi_game/multi_game_rubberbanding.h"
#include "volition/rfg/code/multi/multi_game/multi_game_settings.h"
#include "volition/rfg/code/multi/multi_game/multi_game_sim_event.h"
#include "volition/rfg/code/multi/multi_game/multi_game_stats.h"
#include "volition/rfg/code/multi/multi_game/multi_game_xp.h"
#include "volition/rfg/code/multi/multi_game/multi_gameplay_mode.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_core/include/multi_connection.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_delta_compression.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_game_bandwidth.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_io.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_objects.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_packet_types.h"
#include "volition/rfg/code/multi/multi_core/include/multi_pool.inl"
#include "volition/rfg/code/multi/multi_core/include/multi_session.h"
#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/rfg/code/destruction/rfg_dynamics.h"
#include "volition/rfg/code/destruction/rfg_stress.h"
#include "volition/rfg/code/effects/effects.h"
#include "volition/rfg/code/gamewide/gameaudio.h"
#include "volition/rfg/code/gamewide/gamemusic.h"
#include "volition/rfg/code/interface/multi_player_menus/multi_player_menu_system.h"
#include "volition/rfg/code/interface/multi_player_menus/ui_document_mp_stats.h"
#include "volition/rfg/code/interface/ui_document_hud_tool_tips.h"
#include "volition/rfg/code/interface/ui_document_hud_multiplayer.h"
#include "volition/rfg/code/gamewide/downloadable_content.h"
#include "volition/rfg/code/gamewide/gamewide.h"
#include "volition/rfg/code/gamewide/gameseq.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/interface/loading.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice_shared.h"
#include "volition/rfg/code/objects/inventory_item.h"
#include "volition/rfg/code/objects/ladder.h"
#include "volition/rfg/code/objects/movers/general_mover.h"
#include "volition/rfg/code/objects/object_zone.h"
#include "volition/rfg/code/objects/object_item.h"
#include "volition/rfg/code/objects/object_player_start.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/rfg/code/rfg.h"
#include "volition/stream2/code/stream2.h"
#include "volition/vlib/code/input/mouse.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/memory/split_mempool.h"
#include "volition/vlib/code/os/vconsole.h"
#include "volition/vlib/code/util/dictionary.h"
#include "volition/vlib/code/util/string_util.h"
#include "volition/vlib/code/util/string_comp.h"
#include "keen/base/stringencoding.hpp"
#include "keen/base/trace.hpp"

#include <stdlib.h>
#include <string.h>
#include <wchar.h>


// ---- file-private types (rfg.pdb)
// size 0x14
struct tracked_backpack_reference
{
    const multi_backpack_info* b_info; // 0x00
    tracked_stats_indices total_xp_stat; // 0x04
    tracked_stats_indices time_equipped_stat; // 0x08
    tracked_stats_indices kills_stat; // 0x0C
    tracked_stats_indices deaths_stat; // 0x10
};

// size 0x18
struct tracked_weapon_reference
{
    const weapon_info* w_info; // 0x00
    tracked_stats_indices total_xp_stat; // 0x04
    tracked_stats_indices time_equipped_stat; // 0x08
    tracked_stats_indices kills_stat; // 0x0C
    tracked_stats_indices deaths_stat; // 0x10
    tracked_stats_indices one_life_kills_in_a_row_stat; // 0x14
};

// ---- file-static globals
static const wchar_t UNKNOWN_PLAYER_NAME_MULTI_GAME[20] = L"Unknown player name"; // 0x01309C48
// hkClass* const Classes[1523] (0x013FF698): a Havok header static (the class list), unused here.
// session_types Game_bandwidth_test_session_type (0x0167D83C) is used here too; placed in multi_core_game_bandwidth.cpp.
// int Game_bw_test_size (0x0167D840) is used here too; placed in multi.cpp.
// multi_gameplay_mode Multi_gameplay_mode (0x0167DEE0) is used here too; placed in multi_core_platform.cpp.
// human_voice_persona_handle Multi_general_announcer (0x0167DEE4) is used here too; placed in multi_game_mode_bagman.cpp.
// External linkage: multi_gameplay_mode.cpp (later in the unity TU) reads it.
bool Current_mode_has_exited = true; // 0x0167DD7B
// multi_camera_mode Current_mode_camera (0x0167E544) is used here too; placed in multi_game_mode_common.cpp.
static unsigned int Pending_target = 0xFFFFFFFF; // 0x0167E548
// unsigned int Local_game_data_handles[4] (0x0167E5F4) is used here too; placed in multi_join.cpp.
// jip_destruction_sync jip_destruction_sync_state (0x0167E708) is used here too; placed in multi.cpp.
// timestamp Client_standby_hack_check (0x016916C0) is used here too; placed in multi_connection.cpp.
// dynamic_uint_hash_table<remote_object*> Game_handle_to_rop_hash (0x01691B40) is used here too; placed in multi_core_delta_compression.cpp.
static farray<unsigned int,30> Deferred_repostion_object_list; // 0x01691E58
static timestamp_realtime Remote_packfile_reading_timer; // 0x01691EE0
// External linkage below: the later files of the unity TU use these (multi_game_badge.cpp, multi_game_camera.cpp,
// multi_game_character.cpp, multi_game_classes.cpp, multi_game_sim_event.cpp, ...).
dynamic_object_pool<multi_badge_field,1> Multi_badge_fields; // 0x016925E8
timestamp_percent Camera_transition_timestamp; // 0x01692744
timestamp Camera_stay_dead_timestamp; // 0x0169274C
dynamic_object_pool<mp_game_character_info,1> Mp_character_info_pool; // 0x016929F8
multi_pool<mp_player_class_info,4> Class_pool; // 0x01692B48
// timestamp_realtime Mgm_allow_migration (0x01692DC4) is used here too; placed in multi_game_mode_common.cpp.
multi_pool<multi_game_sim_event,1024> Sim_event_allocator; // 0x01693A10
destruction_frame_data* MP_destruction_frame_pool; // 0x02FEB564
stream2_mempool_allocator Stream_mp_interface_preload_allocator; // 0x01691E28
stream2_slot_allocator Stream_mp_interface_slot_allocator; // 0x01691E34
mempool_base* Mp_interface_slot_allocator_mempool; // 0x02FEB660
static int Current_spawn_point; // 0x02FEB664
bool Players_killer; // 0x02FEB689
mp_player_class_info* Default_class_info; // 0x02FEB694
friend_system_data* Friend_system_data; // 0x02FEB6A4
// External linkage in the port: modes/multi_game_mode_common.cpp (earlier in the unity TU) declares it.
bool Multi_game_has_migrated; // 0x02FEB6C8
// External linkage: multi.cpp and modes/multi_game_mode_common.cpp (earlier in the unity TU) declare it.
bool Multi_game_has_skipped_game_migration; // 0x02FEB6C9
bool Multi_kicked_from_party; // 0x02FEB703
multi_weapon_stats* Mp_weapon_stats; // 0x02FEB724
tracked_weapon_reference* Stat_tracked_weapons; // 0x02FEB740
tracked_backpack_reference* Stat_tracked_backpacks; // 0x02FEB748
// The other shared statics this file uses (bandwidth, session, stats and mode state) are placed in their first users.

// File statics of later files of the unity TU, declared here.
static void multi_game_join_remove_queued_loaded_client(const multi_connection* cp); // 0x009AEE90 (multi_game_io.cpp:368)
static void multi_game_player_mp_flags_snapshot(); // 0x0095B750 (multi_game_object_player.cpp:3352)
static void multi_game_player_mp_flags_handshake(); // 0x0095B7E0 (multi_game_object_player.cpp:3366)

// Magic numbers: text chat lines hold 90 characters (names 32), remote console commands 128; packets are built in a
// 1,200 byte buffer; at most 1,000 reserved handles per object class.
static const int Max_chat_text = 90;
static const int Max_chat_name = 32;
static const int Max_console_command = 128;
static const int Packet_buffer_size = 1200;
static const int Max_reserved_handles = 1000;

static char s_debugNameUTF8[256]; // multi_game_debug_get_char_name_from_connection_UTF8's buffer

// ---- external functions

// 0x009DBD40 (:199): console command (unregistered in retail): run a command here and in the lobby's other peers
void unused_dcf_remote_console()
{
    if (Console_command == 0 || !Multiplayer)
    {
        return;
    }
    char command[256];
    strcpy(command, Console_command_line);
    console_do_command(Console_command_line);
    multi_game_send_game_lobby_console_command(command);
}

// 0x0094E3C0 (:299)
bool multi_game_allow_chat_say()
{
    if (!Multiplayer)
    {
        return false;
    }
    return console_is_chat_window_enabled();
}

// 0x0094E3E0 (:310)
bool multi_game_allow_chat_teamsay()
{
    return Multiplayer && console_is_chat_window_enabled() && Game_session != nullptr;
}

// 0x00995470 (:324): a chat line in the console's chat window, the name in the sender's team colour
void multi_game_chat_say(const char* text, const multi_game_data* game_data, human_teams team)
{
    if (game_data == nullptr)
    {
        return;
    }
    color team_color;
    multi_game_render_get_team_color(team_color, MTCT_ICON, game_data->team);
    wchar_t name[Max_chat_name];
    wcsncpy(name, game_data->display_name, Max_chat_name);
    name[Max_chat_name - 1] = L'\0';
    rfg_reverse_escape_characters(name);
    char line[Max_chat_text + 2];
    strncpy_safe(line, text, Max_chat_text);
    char name_utf8[Max_chat_name];
    keen::convertUTF16ToUTF8(name_utf8, Max_chat_name - 1, (const uint16*)name);
    console_add_textchat_line(name_utf8, line, team != HUMAN_TEAM_NONE, false, team_color, true);
}

// 0x009BB560 (:358): the sender's game data handle, the team (none for everyone) and the text; the server sends it to
// everyone (or the team), a client to the server
void multi_game_send_textchat_message(unsigned int sender_data_handle, char* text, human_teams team)
{
    multi_session* session = Game_session != nullptr ? Game_session : Lobby_session;
    if (session == nullptr)
    {
        return;
    }
    unsigned char packet[Packet_buffer_size];
    packet[0] = PT_TEXT_CHAT_MSG;
    unsigned short size = packet_header_size(PT_TEXT_CHAT_MSG);
    write_u32_be(packet, size, sender_data_handle);
    write_u32_be(packet, size, (unsigned int)team);
    const unsigned short length = (unsigned short)strlen(text);
    memcpy(packet + size, text, length);
    size = (unsigned short)(size + length);
    packet[size++] = '\0';
    packet_set_size(packet, size);
    if (!Multiplayer || !Server)
    {
        multi_io_send_reliable(session->get_host_connection(), packet, size, nullptr, nullptr, nullptr);
        return;
    }
    if (team == HUMAN_TEAM_NONE)
    {
        multi_io_send_to_all_reliable(session, packet, size);
        return;
    }
    multi_connection* first = session->get_connection_list();
    for (multi_connection* cp = first; cp != nullptr;)
    {
        if (cp != session->get_me_connection())
        {
            const multi_game_data* data = multi_game_data_get(cp->game_data, false, false);
            if (data != nullptr && data->team == team)
            {
                multi_io_send_reliable(cp, packet, size, nullptr, nullptr, nullptr);
            }
        }
        cp = cp->next;
        if (cp == first)
        {
            break;
        }
    }
}

// 0x009BB7B0 (:407): the server passes it on; it is shown to everyone or to the team
void multi_game_process_textchat_packet(unsigned char* packet_data, const sender& from)
{
    int offset = 0;
    const unsigned int sender_data_handle = read_u32_be(packet_data, offset);
    const human_teams team = (human_teams)read_u32_be(packet_data, offset);
    const char* received = (const char*)packet_data + offset;
    char text[Max_chat_text + 2];
    if (strlen(received) < Max_chat_text)
    {
        strcpy(text, received);
    }
    else
    {
        strncpy_safe(text, received, Max_chat_text);
    }
    if (Multiplayer && Server)
    {
        multi_game_send_textchat_message(sender_data_handle, text, team);
    }
    const multi_game_data* game_data = multi_game_data_get(sender_data_handle, false, false);
    const multi_game_data* my_data = multi_game_data_get(Local_game_data_handles[ST_GAME], false, false);
    if (team == HUMAN_TEAM_NONE || (my_data != nullptr && my_data->team == team))
    {
        multi_game_chat_say(text, game_data, team);
    }
}

// 0x0094E400 (:430): the chat window's scroll buttons under the mouse
void mp_game_input_process_chatmouse()
{
    vector bmin;
    vector bmax;
    console_get_scrollup_region(bmin, bmax);
    mouse_just_released(0);
    if (Mouse_scaled_x < bmin.x || bmax.x < Mouse_scaled_x || Mouse_scaled_y < bmin.y || bmax.y < Mouse_scaled_y)
    {
        console_scrollup_mouse_over(false);
    }
    else
    {
        console_scrollup_mouse_over(true);
        if (mouse_just_released(0))
        {
            console_scrollup_pressed();
        }
    }
    console_get_scrolldown_region(bmin, bmax);
    mouse_just_released(0);
    if (Mouse_scaled_x < bmin.x || bmax.x < Mouse_scaled_x || Mouse_scaled_y < bmin.y || bmax.y < Mouse_scaled_y)
    {
        console_scrolldown_mouse_over(false);
    }
    else
    {
        console_scrolldown_mouse_over(true);
        if (mouse_just_released(0))
        {
            console_scrolldown_pressed();
        }
    }
}

// 0x009BB950 (:487): console command "say"
void dcf_say()
{
    if (Console_command == 0)
    {
        if (Console_help != 0)
        {
            console_output("Usage: say text", nullptr, false);
        }
        return;
    }
    if (!Multiplayer || (Game_session == nullptr && Lobby_session == nullptr))
    {
        console_output("Text chat is only available in a multiplayer game", nullptr, false);
        return;
    }
    console_get_arg(2, 0);
    const multi_game_data* game_data = multi_game_data_get(Local_game_data_handles[ST_GAME], false, false);
    if (game_data == nullptr)
    {
        return;
    }
    if (Multiplayer && Server)
    {
        multi_game_chat_say(Console_arg, game_data, HUMAN_TEAM_NONE);
    }
    multi_game_send_textchat_message(game_data->handle, Console_arg, HUMAN_TEAM_NONE);
}

// 0x009BBA40 (:516): console command "team_say" (to everyone in a mode without teams)
void dcf_team_say()
{
    if (Console_command == 0)
    {
        if (Console_help != 0)
        {
            console_output("Usage: team_say text", nullptr, false);
        }
        return;
    }
    console_get_arg(2, 0);
    const multi_game_data* game_data = multi_game_data_get(Local_game_data_handles[ST_GAME], false, false);
    if (game_data == nullptr)
    {
        return;
    }
    const bool teams = Multiplayer && Multi_gameplay_mode != MGPM_NONE && multi_game_mode_has_teams();
    const human_teams team = teams ? game_data->team : HUMAN_TEAM_NONE;
    if (Multiplayer && Server)
    {
        multi_game_chat_say(Console_arg, game_data, team);
    }
    multi_game_send_textchat_message(game_data->handle, Console_arg, team);
}

// The chat's console commands (orig_main.md OM5), registered by their dynamic initializers.
console_command Console_say("say", "say to all", DCF_TYPE_FUNC, dcf_say); // 0x02FEBC2C
dictionary<console_command>::entry Console_entry_say("say", &Console_say); // 0x01691EE4
console_command Console_team_say("team_say", "say to team", DCF_TYPE_FUNC, dcf_team_say); // 0x03017720
dictionary<console_command>::entry Console_entry_team_say("team_say", &Console_team_say); // 0x01691EF0

// ---- internal functions

// 0x0094E550 (:619)
static int obj_handle_compare(const void* arg1, const void* arg2)
{
    const unsigned int lhs = *(const unsigned int*)arg1;
    const unsigned int rhs = *(const unsigned int*)arg2;
    if (lhs < rhs)
    {
        return -1;
    }
    return rhs < lhs ? 1 : 0;
}

// <general_mover> 0x009D7480 (:635)
// <multi_object_marker> 0x009D7530 (:635)
// <obj_zone> 0x009D75E0 (:635)
// <ladder> 0x009D7690 (:635)
// <multi_object_flag> 0x009D7740 (:635)
// <rfg_mover> 0x009D73D0 (:635): the level's objects of a class keep network handles in handle order (the same on
// every peer)
template<typename T>
static void add_reserved_handles()
{
    unsigned int handles[Max_reserved_handles];
    int num_handles = 0;
    const int count = World.get_num_objects<T>();
    for (int i = 0; i < count; i++)
    {
        if (num_handles < Max_reserved_handles)
        {
            handles[num_handles++] = World.get_object<T>(i)->get_handle();
        }
    }
    qsort(handles, (size_t)num_handles, sizeof(unsigned int), obj_handle_compare);
    for (int i = 0; i < num_handles; i++)
    {
        multi_core_add_reserved_network_handle(handles[i]);
    }
}

// 0x0094E570 (:659)
static void multi_game_interface_preload()
{
    if (Multi_badges_bitmap_sheet_srid != 0)
    {
        stream2_preload(Multi_badges_bitmap_sheet_srid);
    }
}

// 0x0094E590 (:669)
static void multi_game_interface_unload()
{
    if (Multi_badges_bitmap_sheet_srid != 0)
    {
        stream2_release(Multi_badges_bitmap_sheet_srid);
    }
}

// 0x0094E5B0 (:676): the multiplayer items, sounds, voices, effects and the badge sheet
static void multi_game_do_mp_preload()
{
    mp_preload_object_items();
    gameaudio_mp_preload();
    human_voice_persona_do_game_preload(true, false);
    mp_preload_visual_effects();
    multi_game_interface_preload();
}

// 0x0094E5E0 (:685)
static void multi_game_unload_mp_preload()
{
    multi_game_interface_unload();
    mp_unload_object_items();
    mp_unload_visual_effects();
    gameaudio_mp_unload();
    human_voice_persona_do_game_preload(false, false);
}

// 0x0094E610 (:694)
static unsigned char* mp_interface_slot_cpu_alloc(int bytes, int align)
{
    if (Mp_interface_slot_allocator_mempool == nullptr)
    {
        return nullptr;
    }
    return (unsigned char*)Mp_interface_slot_allocator_mempool->alloc((unsigned int)bytes, (unsigned int)align);
}

// 0x0094E630 (:701)
static unsigned char* mp_interface_slot_gpu_alloc(int bytes, int align)
{
    if (Mp_interface_slot_allocator_mempool != nullptr && (Mp_interface_slot_allocator_mempool->get_flags() & 0x40) != 0)
    {
        return (unsigned char*)((split_mempool*)Mp_interface_slot_allocator_mempool)->alloc_gpu((unsigned int)bytes, (unsigned int)align);
    }
    return nullptr;
}

// ---- external functions

// 0x009DBDB0 (:719)
void multi_game_setup_reserved_remote_objects()
{
    add_reserved_handles<rfg_mover>();
    add_reserved_handles<general_mover>();
    add_reserved_handles<multi_object_marker>();
    add_reserved_handles<obj_zone>();
    add_reserved_handles<ladder>();
    add_reserved_handles<multi_object_flag>();
}

// 0x0094E660 (:740)
void multi_game_system_init()
{
}

// 0x009FCE20 (:749): the multiplayer game systems and tables (inside multi_init); rfg.exe inlines most of the
// callees. Magic numbers: the remote packfile reading timer of 15 s, 1,024 sim events, the destruction frame pool
// (0x57670 bytes) and the 360-frame destruction event history.
void multi_game_init()
{
    Remote_packfile_reading_timer.set(15000);
    const int num_dlc_bundles = (int)dlc_count_callback();
    for (int i = 0; i < num_dlc_bundles; i++)
    {
        if (dlc_bundle_contains_mp((unsigned int)i))
        {
            loading_screen_parse_images_table(dlc_bundle_index_to_id((unsigned int)i));
        }
    }
    multi_game_online_tweaks_init();
    multi_gameplay_mode_system_init(MP_allocators_mempool);
    multi_game_settings_system_init(MP_allocators_mempool, MP_multi_purpose_mempool);
    multi_game_player_system_init();
    multi_game_init_remote_game_objects(false);
    Sim_event_allocator.init(MP_allocators_mempool);
    multi_game_sim_event_reset();
    multi_game_lobby_init();
    MP_destruction_frame_pool = (destruction_frame_data*)MP_allocators_mempool.alloc(0x57670, 0);
    Destruction_event_history = (unsigned int*)MP_allocators_mempool.alloc(360 * sizeof(unsigned int), 0);
    memset(Destruction_event_history, 0, 360 * sizeof(unsigned int));
    stress_evaluation_set_mode("Multi");
    jip_destruction_sync_state = JIP_DESTRUCTION_STATE_NONE;
    jip_destruction_sync_possible = true;
    Client_state = CLIENT_STATE_ESTABLISHED;
    multi_game_do_mp_preload();
    multi_game_data_system_init();
    multi_game_classes_system_init();
    Multi_kicked_from_party = false;
    Multi_partied = false;
    multi_game_stats_init();
    multi_game_character_init(MP_allocators_mempool);
    multi_game_badge_system_init(MP_allocators_mempool);
    multi_game_xp_init(MP_allocators_mempool);
    multi_game_rubberbanding_init();
    multi_game_prefereces_system_init();
    Multi_game_has_migrated = false;
    Multi_game_has_skipped_game_migration = false;
    Migration_state = MIGRATION_STATE_NONE;
    ui_document_mp_stats_multi_init(MP_allocators_mempool);
    multi_game_mp_overrides_init(MP_allocators_mempool);
    multi_player_menu_multi_init(MP_allocators_mempool);
    multi_game_bw_system_init();
    // the multiplayer weapon table (with the multiplayer variants), then the balance overrides
    weapons_read_table(true, true, -1);
    multi_game_overrides_apply_overrides();
    Friend_system_data = (friend_system_data*)MP_allocators_mempool.alloc_cpu(sizeof(friend_system_data), 0);
    memset(Friend_system_data, 0, sizeof(friend_system_data));
}

// 0x009FE480 (:824)
void multi_game_shutdown()
{
    Spectating = false;
    multi_game_friend_cancel_all();
    Friend_system_data = nullptr;
    multi_game_overrides_restore_overrides();
    weapons_read_table(false, true, -1);
    multi_player_menu_multi_shutdown();
    Multi_badge_fields.destroy();
    multi_game_character_shutdown();
    MP_destruction_frame_pool = nullptr;
    Destruction_event_history = nullptr;
    stress_evaluation_set_mode("");
    Game_session = nullptr;
    multi_gameplay_mode_system_shutdown();
    Stat_tracked_weapons = nullptr;
    Stat_tracked_backpacks = nullptr;
    Stat_tracked_levels = nullptr;
    Mp_player_stats = nullptr;
    Mp_weapon_stats = nullptr;
    multi_game_unload_mp_preload();
    multi_game_lobby_shutdown();
    Mp_game_data = nullptr;
    Camera_transition_timestamp.invalidate();
    Camera_stay_dead_timestamp.invalidate();
    Pending_target = Rfg_camera.target_handle;
    if (!game_is_paused())
    {
        game_render_set_camera_frame_override_blur(0.0f);
        game_render_set_camera_signal_noise_frequency(0.0f);
        game_render_set_camera_signal_noise_scale(0.0f);
    }
    Players_killer = false;
    Current_mode_camera = MCM_NONE;
    loading_screen_dlc_restore();
}

// 0x009FD380 (:870)
void multi_game_level_init_pre()
{
    // Magic number: the front end music fades out over 1 s.
    gamemusic_frontend_stop(1000);
    hud_reset_damage_indicators();
    multi_gameplay_mode_level_init_pre();
    multi_game_destruction_level_init_pre();
    multi_game_init_remote_game_objects(true);
    multi_game_sim_event_reset();
    multi_game_backpack_level_init_pre();
    Current_spawn_point = 0;
}

// 0x009BBB30 (:897)
void multi_game_level_init_post()
{
    multi_gameplay_mode_level_init_post();
}

// 0x0094E670 (:902): the remote objects are not destroyed through the network when the level goes
void multi_game_object_shutdown()
{
    for (int i = 0; i < World.get_num_objects(); i++)
    {
        object* objp = World.get_object(i);
        if (objp->remote_obj_flags.remote_object)
        {
            objp->remote_obj_flags.no_destroy_packet = true;
        }
    }
}

// 0x00A01220 (:914): the connections leave gameplay; outside a reload the lobby is told the game ended and the mode
// exits; the multiplayer item streams are released
void multi_game_level_shutdown()
{
    multi_player_menu_system_close_message_box();
    multi_connection* first = Game_session->get_connection_list();
    for (multi_connection* cp = first; cp != nullptr;)
    {
        if (cp->game_data != 0xFFFFFFFF)
        {
            game_data_of(cp->game_data)->player_network_handle = 0xFFFF;
        }
        if (cp->connection_status == CS_CONNECTED_GAMEPLAY)
        {
            cp->connection_status = CS_CONNECTED;
        }
        cp = cp->next;
        if (cp == first)
        {
            break;
        }
    }
    if (gameseq_get_pending_state() != GS_LOAD && !game_booted_to_main_menu())
    {
        multi_game_lobby_signal_game_end();
    }
    multi_gameplay_mode_level_shutdown();
    Migration_state = MIGRATION_STATE_NONE;
    multi_core_dc_reset();
    multi_game_sim_event_reset();
    Client_standby_hack_check.invalidate();
    for (int i = 0; i < Num_item_infos; i++)
    {
        // The shared backpack items were streamed in by the multiplayer preload.
        if (Item_infos[i].shared_backpack)
        {
            stream2_release(Item_infos[i].srid);
        }
    }
}

// 0x009AB9A0 (:956): a server in the background (or using the standby hack) drops its controls
bool multi_game_should_clear_controls()
{
    if (!Multiplayer)
    {
        return false;
    }
    if (!rfg_is_gameframework_fully_in_foreground())
    {
        return true;
    }
    return Multiplayer && Server && multi_core_is_server_using_standby_hack();
}

// 0x0094E6B0 (:1026)
void multi_game_set_spectating(bool spectating)
{
    Spectating = spectating;
}

// 0x0094E6C0 (:1031)
bool multi_game_spectating()
{
    return Spectating;
}

// 0x009DBDD0 (:1036): per network frame: the bandwidth tests, the next queued announcer line, a client's controls
// (from countdown on), and the server's "you hit someone" notices to the clients being sent to
void multi_game_process(const mp_transmit_list& transmit_list, bool client_update)
{
    multi_game_bw_process();
    if (Mp_announce_num_pending_dialogues > 0 && dialogue_manager_get_num_dialogues(DIALOGUE_PRIORITY_UNDEFINED) == 0)
    {
        mga_play_line(Pending_dialogues[0].line);
        if (Mp_announce_num_pending_dialogues > 0)
        {
            Mp_announce_num_pending_dialogues--;
            for (int i = 0; i < Mp_announce_num_pending_dialogues; i++)
            {
                Pending_dialogues[i] = Pending_dialogues[i + 1];
            }
        }
    }
    if (client_update)
    {
        multi_session* session = multi_session_get_by_type(ST_GAME);
        if (session != nullptr && !session->am_i_host() && session->get_host_connection() != nullptr &&
            Multi_match_info.match_state > MGMS_PREGAME + 1)
        {
            multi_game_cp_send_client_controls();
        }
    }
    if (!Multiplayer || !Server)
    {
        return;
    }
    multi_session* session = multi_session_get_by_type(ST_GAME);
    const int count = World.get_num_objects<player>();
    for (int i = 0; i < count; i++)
    {
        player* pp = World.get_object<player>(i);
        if (pp->obj_flags.destroyed || pp->mp_data == 0xFFFFFFFF || !pp->pflags.mp_hit_someone_this_network_frame)
        {
            continue;
        }
        if (pp == player_get_local_player())
        {
            pp->pflags.local_player_hit_someone_this_frame = true;
            pp->pflags.mp_hit_someone_this_network_frame = false;
            continue;
        }
        multi_connection* cp = session->get_player_connection(pp->mp_data);
        for (int j = 0; j < transmit_list.num_transmit_connections; j++)
        {
            if (transmit_list.connections[j] == cp)
            {
                unsigned char packet[3];
                packet[0] = PT_HIT_SOMEONE;
                const unsigned short size = packet_header_size(PT_HIT_SOMEONE);
                packet_set_size(packet, size);
                multi_io_send(cp, packet, size);
                pp->pflags.mp_hit_someone_this_network_frame = false;
                break;
            }
        }
    }
}

// 0x009BBB50 (:1123)
void multi_game_render()
{
    if (Multi_badge_render_my_badge)
    {
        // Magic numbers: the badge drawn at 150, 150 at full scale.
        multi_game_badge_render(My_badge, 150, 150, 1.0f, 1.0f);
    }
}

// 0x0094E6D0 (:1137): magic numbers: 20 MB (plus 512 bytes) of system memory and 4 MB of video memory
void multi_game_interface_memory_required(int* cpu_required, int* gpu_required)
{
    if (cpu_required == nullptr || gpu_required == nullptr)
    {
        return;
    }
    *cpu_required = 0;
    *gpu_required = 0;
    *cpu_required += 0x1400200;
    *gpu_required += 0x400000;
}

// 0x0094E700 (:1153): the interface preloads and slots come from the pool. Magic numbers: one 1 MB (+0x80) slot
// size, 2,048 bytes of slot bookkeeping in each memory.
void multi_game_interface_memory_init(mempool_base* pool)
{
    if (pool == nullptr)
    {
        return;
    }
    Stream_mp_interface_preload_allocator.init(pool);
    Mp_interface_slot_allocator_mempool = pool;
    int slot_info[3] = {0x100080, 0, 4};
    Stream_mp_interface_slot_allocator.init(mp_interface_slot_cpu_alloc, 0x800, mp_interface_slot_gpu_alloc, 0x800, slot_info, 1, "MP interface slot");
}

// 0x0094E780 (:1169)
void multi_game_interface_memory_shutdown()
{
    if (Mp_interface_slot_allocator_mempool != nullptr)
    {
        Mp_interface_slot_allocator_mempool = nullptr;
    }
}

// 0x0094E7A0 (:1175)
bool multi_game_interface_memory_initialized()
{
    return Mp_interface_slot_allocator_mempool != nullptr;
}

// 0x009BBB90 (:1180)
player* player_from_network_handle(unsigned short net_handle)
{
    const unsigned int handle = multi_core_game_handle_from_network_handle(net_handle, true);
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

// 0x009BBBD0 (:1187): PT_CLIENT_LOADED_GAME to the server, sent at once; our connections are now in gameplay
void multi_game_send_client_loaded_level()
{
    multi_session* session = multi_session_get_by_type(ST_GAME);
    multi_connection* host = session->get_host_connection();
    if (host == nullptr)
    {
        return;
    }
    unsigned char packet[Packet_buffer_size];
    packet[0] = PT_CLIENT_LOADED_GAME;
    const unsigned short size = packet_header_size(PT_CLIENT_LOADED_GAME);
    packet_set_size(packet, size);
    multi_io_send_reliable(host, packet, size, nullptr, nullptr, nullptr);
    multi_connection_flush_current_packet(host);
    multi_connection* first = session->get_connection_list();
    for (multi_connection* cp = first; cp != nullptr;)
    {
        if (cp->connection_status == CS_CONNECTED)
        {
            cp->connection_status = CS_CONNECTED_GAMEPLAY;
        }
        cp = cp->next;
        if (cp == first)
        {
            break;
        }
    }
}

// 0x0094E7B0 (:1210)
bool multi_game_in_gameplay()
{
    return gameseq_state_is_active(GS_GAMEPLAY);
}

// ---- internal functions

// 0x00978AD0 (:1222): an object that left the broadphase goes to the middle of it, half a metre above the terrain
// (players excepted). LTCG: objp in EAX.
static void deferred_reposition_process_object(object* objp)
{
    vector bmin;
    vector bmax;
    havok_get_broadphase_extents(bmin, bmax);
    vector pos;
    pos.x = (bmax.x + bmin.x) * 0.5f;
    pos.y = (bmax.y + bmin.y) * 0.5f;
    pos.z = (bmin.z + bmax.z) * 0.5f;
    const float middle_y = pos.y;
    if (!havok_terrain_height(pos.y, pos.x, pos.z, true, nullptr))
    {
        pos.y = middle_y;
    }
    pos.y += 0.5f;
    if ((objp == nullptr || objp->get_obj_type() != OT_HUMAN || objp->get_sub_type() != OT_SUB_HUMAN_PLAYER) && objp->havok_handle != 0xFFFFFFFF)
    {
        havok_body_set_position(objp->havok_handle, pos);
    }
}

// 0x00978C10 (:1263): the deferred objects are moved back in; a building of the server is destroyed (outside a
// migration)
static void deferred_delete_list_post_process()
{
    if (!Multiplayer && !wrecking_crew_is_active())
    {
        Deferred_repostion_object_list.clear();
        return;
    }
    const int count = Deferred_repostion_object_list.size();
    for (int i = 0; i < count; i++)
    {
        object* objp = World.object_from_handle(Deferred_repostion_object_list[i]);
        if (objp == nullptr || objp->obj_flags.destroyed)
        {
            continue;
        }
        deferred_reposition_process_object(objp);
        if (Multiplayer && Server && objp->get_obj_type() == OT_MOVER && objp->get_sub_type() == OT_SUB_MOVER_RFG && Migration_state == MIGRATION_STATE_NONE)
        {
            rfg_dyn_add_pre_collide_mp_destroy((rfg_mover*)objp, nullptr, false);
        }
    }
    Deferred_repostion_object_list.clear();
}

// 0x00978CC0 (:1290)
static void deferred_delete_list_pre_process()
{
    Deferred_repostion_object_list.clear();
}

// ---- external functions

// 0x00995550 (:1302): objects that leave the broadphase in multiplayer are moved back after the step instead of being
// deleted (a client's remote objects, the server's players and buildings; players in Wrecking Crew)
bool multi_game_havok_broadphase_defer_delete(object* objp)
{
    if (!Multiplayer && !wrecking_crew_is_active())
    {
        return false;
    }
    vector bmin;
    vector bmax;
    objp->get_bbox(bmax, bmin);
    if (Multiplayer)
    {
        if (!Server)
        {
            if (objp->remote_obj_flags.remote_object || objp->remote_obj_flags.client_sync)
            {
                Deferred_repostion_object_list.add_unique(objp->get_handle());
                return true;
            }
        }
        else
        {
            if (objp->get_obj_type() == OT_HUMAN && objp->get_sub_type() == OT_SUB_HUMAN_PLAYER)
            {
                keen::debug::nativePrintArgs("[mp_tp_bug] multi_game_havok_broadphase_defer_delete: MULTI_SERVER: add player object to Deferred_repostion_object_list. obj: %p , pos: (%f, %f, %f)\n",
                    objp, objp->pos.x, objp->pos.y, objp->pos.z);
                Deferred_repostion_object_list.add_unique(objp->get_handle());
                return true;
            }
            if (objp->get_obj_type() == OT_MOVER && objp->get_sub_type() == OT_SUB_MOVER_RFG)
            {
                Deferred_repostion_object_list.add_unique(objp->get_handle());
                return true;
            }
        }
    }
    if (wrecking_crew_is_active() && objp->get_obj_type() == OT_HUMAN && objp->get_sub_type() == OT_SUB_HUMAN_PLAYER)
    {
        Deferred_repostion_object_list.add_unique(objp->get_handle());
        return true;
    }
    return false;
}

// 0x009F7FD0 (:1369)
void multi_game_init_remote_game_objects(bool clear_allocators)
{
    multi_game_object_player_init(clear_allocators);
    multi_game_object_projectile_init(clear_allocators);
    multi_game_object_items_init(clear_allocators);
    multi_game_object_rfg_mover_init(clear_allocators);
    multi_game_object_mover_init(clear_allocators);
}

// 0x009BBCA0 (:1382): PT_SERVER_OPTIONS: the options, the teams and the desired weapons of the session's players
void multi_game_send_server_options(multi_session* session)
{
    unsigned char packet[Packet_buffer_size];
    packet[0] = PT_SERVER_OPTIONS;
    unsigned short size = packet_header_size(PT_SERVER_OPTIONS);
    multi_game_append_server_options_to_packet(packet, size, &Mp_server_options);
    multi_game_player_append_teams_to_packet(packet, size, session->get_info()->type);
    multi_game_player_append_desired_weapons_to_packet(packet, size, session);
    packet_set_size(packet, size);
    multi_io_send_to_all_reliable(session, packet, size);
}

// 0x009BBD60 (:1395)
void multi_game_process_server_options(unsigned char* packet_data, const sender& from)
{
    int offset = 0;
    multi_game_read_server_options_from_packet(&Mp_server_options, packet_data, offset);
    multi_game_player_read_teams_from_packet(packet_data, offset);
    multi_game_player_read_desired_weapons_from_packet(packet_data, offset);
}

// 0x00995680 (:1407): when each player last damaged or repaired a building (on the network clock)
void multi_game_player_damaged_rfg_mover(player* pp, rfg_mover* mover)
{
    if (mover != nullptr && pp != nullptr && pp->mp_data != 0xFFFFFFFF && mover->mp_info != nullptr)
    {
        mover->mp_info->damage_history.damaged[pp->mp_data & 0xFFFF] = Clock_current_time;
    }
}

// 0x00978CD0 (:1418)
void multi_game_player_damaged_rfg_mover(unsigned int multi_game_data_handle, rfg_mover* mover)
{
    if (mover != nullptr && multi_game_data_handle != 0xFFFFFFFF && mover->mp_info != nullptr)
    {
        mover->mp_info->damage_history.damaged[multi_game_data_handle & 0xFFFF] = Clock_current_time;
    }
}

// 0x009956C0 (:1433)
void multi_game_player_repaired_rfg_mover(player* pp, rfg_mover* mover)
{
    if (mover != nullptr && pp != nullptr && pp->mp_data != 0xFFFFFFFF && mover->mp_info != nullptr)
    {
        mover->mp_info->damage_history.repaired[pp->mp_data & 0xFFFF] = Clock_current_time;
    }
}

// 0x00978D00 (:1444)
void multi_game_player_repaired_rfg_mover(unsigned int multi_game_data_handle, rfg_mover* mover)
{
    if (mover != nullptr && multi_game_data_handle != 0xFFFFFFFF && mover->mp_info != nullptr)
    {
        mover->mp_info->damage_history.repaired[multi_game_data_handle & 0xFFFF] = Clock_current_time;
    }
}

// 0x00995700 (:1462)
unsigned int multi_game_time_since_player_damaged_rfg_mover(player* pp, const rfg_mover* mover)
{
    if (pp != nullptr && mover != nullptr && mover->mp_info != nullptr && pp->mp_data != 0xFFFFFFFF)
    {
        return Clock_current_time - mover->mp_info->damage_history.damaged[pp->mp_data & 0xFFFF];
    }
    return 0;
}

// 0x00978D30 (:1473)
unsigned int multi_game_time_since_player_damaged_rfg_mover(unsigned int multi_game_data_handle, const rfg_mover* mover)
{
    if (mover != nullptr && mover->mp_info != nullptr && multi_game_data_handle != 0xFFFFFFFF)
    {
        return Clock_current_time - mover->mp_info->damage_history.damaged[multi_game_data_handle & 0xFFFF];
    }
    return 0;
}

// 0x00995740 (:1490)
unsigned int multi_game_time_since_player_repaired_rfg_mover(player* pp, const rfg_mover* mover)
{
    if (pp != nullptr && mover != nullptr && mover->mp_info != nullptr && pp->mp_data != 0xFFFFFFFF)
    {
        return Clock_current_time - mover->mp_info->damage_history.repaired[pp->mp_data & 0xFFFF];
    }
    return 0;
}

// 0x00978D70 (:1501)
unsigned int multi_game_time_since_player_repaired_rfg_mover(unsigned int multi_game_data_handle, const rfg_mover* mover)
{
    if (mover != nullptr && mover->mp_info != nullptr && multi_game_data_handle != 0xFFFFFFFF)
    {
        return Clock_current_time - mover->mp_info->damage_history.repaired[multi_game_data_handle & 0xFFFF];
    }
    return 0;
}

// Port helper: the player of the most recent time in a history (rfg.exe inlines this into both getters).
static player* most_recent_player(const unsigned int* times)
{
    int player_index = -1;
    unsigned int best = 0xFFFFFFFF;
    for (int i = 0; i < 16; i++)
    {
        const unsigned int since = Clock_current_time - times[i];
        if (times[i] != 0 && since < best)
        {
            player_index = i;
            best = since;
        }
    }
    return multi_game_data_get_player_instance(player_index);
}

// 0x009DBFB0 (:1515)
player* multi_game_last_player_to_damage_rfg_mover(const rfg_mover* mover)
{
    if (mover == nullptr || mover->mp_info == nullptr)
    {
        return nullptr;
    }
    return most_recent_player(mover->mp_info->damage_history.damaged);
}

// 0x009DC050 (:1537)
player* multi_game_last_player_to_repair_rfg_mover(const rfg_mover* mover)
{
    if (mover == nullptr || mover->mp_info == nullptr)
    {
        return nullptr;
    }
    return most_recent_player(mover->mp_info->damage_history.repaired);
}

// 0x0094E7C0 (:1559): weapon pickups follow the respawn speed option (others keep their delay)
int multi_game_get_item_respawn_delay(item* obj_itemp)
{
    if (Mp_server_options.generic_options.weapon_respawn_speed == MGWRS_ALWAYS)
    {
        return 0;
    }
    float scale = 1.0f;
    if (obj_itemp->get_sub_type() == OT_SUB_ITEM_WEAPON)
    {
        // Magic numbers: fast halves the delay, slow adds half.
        if (Mp_server_options.generic_options.weapon_respawn_speed == MGWRS_FAST)
        {
            scale = 0.5f;
        }
        else if (Mp_server_options.generic_options.weapon_respawn_speed == MGWRS_SLOW)
        {
            scale = 1.5f;
        }
    }
    return (int)((float)obj_itemp->info->respawn_delay * scale);
}

// 0x0094E820 (:1576)
void multi_game_process_player_hit_someone(unsigned char* packet_data, const sender& from)
{
    player* pp = player_get_local_player();
    if (pp != nullptr)
    {
        pp->pflags.local_player_hit_someone_this_frame = true;
    }
}

// 0x009DC0F0 (:1604)
void multi_game_trigger_tooltip(player* pp, const char* tooltip_name)
{
    multi_game_trigger_tooltip(pp, checksum_stri(tooltip_name, 0));
}

// 0x009BBDA0 (:1610): ours is shown; the server sends another player's to its client (PT_TOOLTIP)
void multi_game_trigger_tooltip(player* pp, const checksum_stri& tooltip_name_crc)
{
    if (pp == player_get_local_player())
    {
        ui_tool_tips_add_tip(tooltip_name_crc, 0, nullptr);
        return;
    }
    if (!Multiplayer || !Server)
    {
        return;
    }
    unsigned char packet[Packet_buffer_size];
    packet[0] = PT_TOOLTIP;
    unsigned short size = packet_header_size(PT_TOOLTIP);
    write_u32_be(packet, size, tooltip_name_crc.get_crc());
    packet_set_size(packet, size);
    multi_session* session = multi_session_get_by_type(ST_GAME);
    if (session != nullptr)
    {
        multi_connection* cp = session->get_player_connection(pp->mp_data);
        if (cp != nullptr)
        {
            multi_io_send_reliable(cp, packet, size, nullptr, nullptr, nullptr);
        }
    }
}

// 0x009BBEE0 (:1637)
void multi_game_process_tooltip_packet(unsigned char* packet_data, const sender& from)
{
    int offset = 0;
    const checksum_stri crc(read_u32_be(packet_data, offset));
    multi_game_trigger_tooltip(player_get_local_player(), crc);
}

// 0x009EFED0 (:1649): as multi_game_removed_connection
void multi_game_player_left(multi_connection* cp)
{
    multi_game_join_remove_queued_loaded_client(cp);
    if (cp->game_data == 0xFFFFFFFF)
    {
        return;
    }
    player* pp = multi_game_data_get_player_instance(cp->game_data);
    if (pp != nullptr)
    {
        multi_game_remove_player(pp);
    }
    multi_game_data_remove_player(cp->game_data);
    cp->game_data = 0xFFFFFFFF;
}

// 0x009BBF50 (:1654): the game pauses for the migration (not after the last round)
void multi_game_migration_about_to_start(multi_session_base* sp)
{
    if (Multi_match_info.match_state == MGMS_VIEWING_RESULTS && multi_game_mode_is_last_round())
    {
        return;
    }
    game_pause(true);
    multi_player_menu_system_pull_up_migrate_message_box();
}

// 0x00978DB0 (:1662): from countdown on, once the migration timer allows it
bool multi_game_migration_allow_migration(multi_session_base* sp)
{
    return Multi_match_info.match_state > MGMS_PREGAME + 1 && Mgm_allow_migration.elapsed();
}

// 0x009F80D0 (:1671)
void multi_game_migration_just_occurred(multi_session_base* sp)
{
    if (Multi_match_info.match_state == MGMS_VIEWING_RESULTS && multi_game_mode_is_last_round())
    {
        Multi_game_has_skipped_game_migration = true;
        return;
    }
    multi_game_migration_start_migration(sp);
}

// 0x009BBF80 (:1681): back to the main menu (the host left, or the migration failed)
void multi_game_migration_failed(multi_session_base* sp)
{
    if (Multi_match_info.match_state == MGMS_VIEWING_RESULTS && multi_game_mode_is_last_round())
    {
        return;
    }
    Migration_state = MIGRATION_STATE_FAILED;
    multi_player_menu_system_close_message_box();
    if (gameseq_get_state() == GS_MAINMENU)
    {
        return;
    }
    if (sp->get_migration_reason() != SMR_HOST_LEFT)
    {
        game_boot_to_main_menu(BC_GAME_MIGRATION_FAIL);
        return;
    }
    game_boot_to_main_menu(BC_HOST_LEFT);
}

// 0x009EFEE0 (:1701): a freed remote object leaves replication (the server tells the clients unless told not to)
void multi_game_process_remote_object_free_common(object* obj)
{
    if (!obj->remote_obj_flags.remote_object)
    {
        return;
    }
    remote_object* rop = Game_handle_to_rop_hash.get(obj->get_handle());
    if (rop == nullptr || !Multiplayer)
    {
        return;
    }
    if (Server)
    {
        if (!obj->remote_obj_flags.no_destroy_packet)
        {
            multi_core_object_destroy_remote_object(rop->m_network_handle);
            return;
        }
        multi_core_destroy_remote_object_no_packet_external(rop->m_network_handle);
        return;
    }
    multi_core_destroy_remote_object_no_packet_external(rop->m_network_handle);
}

// 0x0094E840 (:1723)
void multi_game_call_debug_render_frame()
{
}

// 0x0094E850 (:1741): not while loading or in gameplay
bool multi_game_in_joinable_mode()
{
    return (unsigned int)(gameseq_get_state() - GS_GAMEPLAY) > 1;
}

// 0x009DC120 (:1761): every player respawns at its best spawn point (a player without a desired weapon gets one of
// the level's at random); the players ignore each other's collisions meanwhile
void multi_game_respawn_all_players(bool start_of_round)
{
    const int count = World.get_num_objects<player>();
    for (int i = 0; i < count; i++)
    {
        World.get_object<player>(i)->pflags.mp_ignore_player_for_respawn_collisions = true;
    }
    for (int i = 0; i < count; i++)
    {
        player* pp = World.get_object<player>(i);
        const multi_game_data* data = multi_game_data_get(pp->mp_data, false, false);
        if (data == nullptr)
        {
            continue;
        }
        const multi_level_info* level = Mp_server_options.get_level();
        if (level->num_player_weapons > 0 && data->desired_weapon_inv_item_info_handle == 0xFFFFFFFF)
        {
            const unsigned int weapon = level->player_weapons[int_rand_range(0, level->num_player_weapons - 1)];
            multi_game_player_set_desired_weapon(multi_session_get_by_type(ST_GAME), data->handle, weapon);
        }
        player_respawn(pp, multi_game_respawn_get_best_spawn_point(pp, start_of_round), true);
        pp->pflags.mp_ignore_player_for_respawn_collisions = false;
    }
}

// 0x009BBFD0 (:1792): the default class's inventory (its sledgehammer replaced by the player's), then the mode's
// loadout: the starting weapon (or the desired one), the third and fourth weapons
void multi_game_set_player_default_inventory(player* pp)
{
    if (pp == nullptr || (pp->mp_data == 0xFFFFFFFF && game_booted_to_main_menu()))
    {
        return;
    }
    multi_game_equipment_loadout loadout;
    multi_gameplay_mode_get_default_equipment(pp, &loadout);
    const multi_game_data* data = multi_game_data_get(pp->mp_data, false, false);
    if (data == nullptr)
    {
        return;
    }
    const farray<human_info_inventory_info, 15> inventory(Default_class_info->default_inventory);
    for (int i = 0; i < inventory.size(); i++)
    {
        const inv_item_info* i_info = inventory[i].i_info;
        if (i_info->w_info != nullptr && i_info->w_info->weapon_class == WEAPON_CLASS_SLEDGEHAMMER && data->desired_hammer_unique_id != 0xFF)
        {
            const weapon_info* hammer = weapons_get_weapon_info_from_weapon_unique_id(data->desired_hammer_unique_id);
            if (hammer != nullptr && hammer->weapon_class == WEAPON_CLASS_SLEDGEHAMMER && hammer->weapon_inv_item_info != nullptr)
            {
                i_info = hammer->weapon_inv_item_info;
            }
        }
        human_add_item_to_inventory(pp, i_info, inventory[i].count, -1, inventory[i].slot, true, false, true);
    }
    if (!loadout.replace_starting_weapon)
    {
        if (data->desired_weapon_inv_item_info_handle != 0xFFFFFFFF)
        {
            human_add_item_to_inventory(pp, inv_item_info_from_handle(data->desired_weapon_inv_item_info_handle), 0, -1, -1, true, false, true);
        }
    }
    else
    {
        const weapon_info* w_info = weapons_get_weapon_info(loadout.starting_weapon);
        if (w_info != nullptr)
        {
            human_add_item_to_inventory(pp, w_info->weapon_inv_item_info, 0, -1, -1, true, false, true);
        }
    }
    const weapon_info* third = weapons_get_weapon_info(loadout.third_weapon);
    if (third != nullptr)
    {
        human_add_item_to_inventory(pp, third->weapon_inv_item_info, 0, -1, -1, true, false, true);
    }
    const weapon_info* fourth = weapons_get_weapon_info(loadout.fourth_weapon);
    if (fourth != nullptr)
    {
        human_add_item_to_inventory(pp, fourth->weapon_inv_item_info, 0, -1, -1, true, false, true);
    }
}

// 0x009BC200 (:1937): the replication type of an object (7 for a local building)
int multi_game_get_remote_object_type(const object* obj)
{
    if (!obj->remote_obj_flags.remote_object)
    {
        if (obj->get_obj_type() == OT_MOVER && obj->get_sub_type() == OT_SUB_MOVER_RFG)
        {
            return 7;
        }
        return 0;
    }
    remote_object* rop = Game_handle_to_rop_hash.get(obj->get_handle());
    if (rop == nullptr)
    {
        return 0;
    }
    return (int)rop->current_type;
}

// 0x0094E860 (:1967)
bool multi_game_vehicle_is_controlled_by_local_player(const vehicle* vp)
{
    player* local = player_get_local_player();
    return local != nullptr && local == (player*)vehicle_passenger_in_seat(vp, VSI_DRIVER);
}

// 0x0094E890 (:1975)
bool multi_game_vehicle_is_uncontrolled(const vehicle* vp)
{
    return vehicle_passenger_in_seat(vp, VSI_DRIVER) == nullptr;
}

// 0x0094E8B0 (:1984)
void multi_game_call_client_connection_denied_callback(multi_session_join_status reason)
{
    gameseq_set_state(GS_MAINMENU, true);
}

// 0x00978DD0 (:1992)
void multi_game_pre_sim()
{
    multi_game_destruction_clear_vehicle_rfg_mover_destruction_effect_info();
    deferred_delete_list_pre_process();
}

// 0x00978DE0 (:1998): this frame's vehicle building effects count as added; the deferred objects move back
void multi_game_post_sim()
{
    for (int i = 0; i < Vrm_active_effect_info_count; i++)
    {
        if (!Vrm_effect_infos[i].effect_added)
        {
            Vrm_effect_infos[i].effect_added = true;
        }
    }
    deferred_delete_list_post_process();
}

// 0x0094E8C0 (:2004)
bool multi_game_player_has_unlimited_ammo(const player* pp)
{
    return Mp_server_options.generic_options.unlimited_ammo;
}

// 0x0094E8D0 (:2014): remote players never run out of magazine ammo here
bool multi_game_player_has_unlimited_magazine_ammo(const player* pp)
{
    if (pp == nullptr)
    {
        return false;
    }
    return !object_is_local_player(pp);
}

// 0x00995780 (:2025): the player starts in turn (the next start each time, wrapping)
void multi_game_get_player_starting_position(int& player_start_index, vector& pos, matrix& orient)
{
    const int count = World.get_num_objects<player_start>();
    if (count <= 0)
    {
        return;
    }
    Current_spawn_point++;
    if (Current_spawn_point >= count)
    {
        Current_spawn_point = 0;
    }
    player_start_index = Current_spawn_point;
    const player_start* start = World.get_object<player_start>(Current_spawn_point);
    pos = start->pos;
    orient = start->orient;
}

// 0x00995800 (:2043): a random spawn node marker (by priority); null without one
multi_object_marker* multi_game_get_camera_starting_position()
{
    // Magic number: up to 128 candidates.
    multi_object_marker* candidates[128];
    int num_candidates = 0;
    for (int i = World.get_num_objects<multi_object_marker>() - 1; i >= 0; i--)
    {
        if (Multi_markers_prioritized[i]->get_marker_type() != MMT_SPAWN_NODE)
        {
            continue;
        }
        if (num_candidates < 128)
        {
            candidates[num_candidates++] = Multi_markers_prioritized[i];
        }
        else
        {
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        }
    }
    if (num_candidates <= 0)
    {
        return nullptr;
    }
    if (num_candidates == 1)
    {
        return candidates[0];
    }
    const int pick = int_rand_range(0, num_candidates - 1);
    if (pick >= 0 && pick < num_candidates)
    {
        return candidates[pick];
    }
    return nullptr;
}

// 0x0094E8F0 (:2074): teams go on the wire as team + 1 (none becomes 0)
unsigned int multi_game_compress_team(human_teams team)
{
    return (unsigned int)(team + 1);
}

// 0x0094E900 (:2088)
human_teams multi_game_decompress_team(unsigned int compressed_team)
{
    return (human_teams)(compressed_team - 1);
}

// 0x00978E10 (:2095)
const wchar_t* multi_game_debug_get_char_name_from_connection(const multi_connection* cp)
{
    if (cp != nullptr)
    {
        const multi_game_data* data = multi_game_data_get(cp->game_data, false, false);
        if (data != nullptr)
        {
            return data->display_name;
        }
    }
    return UNKNOWN_PLAYER_NAME_MULTI_GAME;
}

// 0x00978E70 (:2110)
const char* multi_game_debug_get_char_name_from_connection_UTF8(const multi_connection* cp)
{
    if (cp != nullptr)
    {
        const multi_game_data* data = multi_game_data_get(cp->game_data, false, false);
        if (data != nullptr && keen::convertUTF16ToUTF8(s_debugNameUTF8, sizeof(s_debugNameUTF8), (const uint16*)data->display_name) != 0)
        {
            return s_debugNameUTF8;
        }
    }
    return "UnknownPlayer";
}

// 0x0094E910 (:2131)
bool multi_game_is_booted_to_main_menu()
{
    // game_booted_to_main_menu, inlined (Booted_to_main_menu is a gamewide.cpp static)
    return game_booted_to_main_menu();
}

// 0x0094E920 (:2137): PT_REMOTE_CONSOLE: a console command from another peer
void multi_game_process_remote_console_command(unsigned char* packet_data, const sender& from)
{
    char command[Max_console_command];
    if (strlen((const char*)packet_data) < Max_console_command)
    {
        strcpy(command, (const char*)packet_data);
    }
    else
    {
        strncpy_safe(command, (const char*)packet_data, Max_console_command);
    }
    console_do_command(command);
}

// 0x009BC270 (:2147)
void multi_game_send_game_lobby_console_command(const char* console_string)
{
    unsigned char packet[Packet_buffer_size];
    packet[0] = PT_REMOTE_CONSOLE;
    unsigned short size = packet_header_size(PT_REMOTE_CONSOLE);
    const unsigned short length = (unsigned short)strlen(console_string);
    memcpy(packet + size, console_string, length);
    size = (unsigned short)(size + length);
    packet[size++] = '\0';
    packet_set_size(packet, size);
    multi_session* session = Game_session != nullptr ? Game_session : Lobby_session;
    if (session != nullptr)
    {
        multi_io_send_to_all_reliable(session, packet, size);
    }
}

// 0x009AB9E0 (:2160): the music of our team (Guerilla or EDF), else of our character, else at random
human_teams multi_game_get_my_team_music()
{
    const multi_game_data* data = multi_game_data_get(Local_game_data_handles[ST_GAME_SNAPSHOT], false, false);
    if (data != nullptr)
    {
        if (data->team == HUMAN_TEAM_GUERILLA)
        {
            return HUMAN_TEAM_GUERILLA;
        }
        if (data->team == HUMAN_TEAM_EDF)
        {
            return HUMAN_TEAM_EDF;
        }
        return multi_game_character_get_my_base_team();
    }
    return (human_teams)(int_rand_range(0, 1) == 0);
}

// 0x00978EF0 (:2180): a client holds sim events back during a migration and a join-in-progress destruction sync
bool multi_game_allow_sim_event_process()
{
    if (Multiplayer && !Server)
    {
        if (Migration_state != MIGRATION_STATE_NONE)
        {
            return Migration_state == MIGRATION_CLIENT_WAIT_FOR_CREATE_PROCESS;
        }
        if (jip_destruction_sync_state != JIP_DESTRUCTION_STATE_NONE)
        {
            return false;
        }
    }
    return true;
}

// 0x00978F30 (:2194)
void multi_game_setup_game_callbacks()
{
    Pre_io_send_job_game_callback = multi_game_player_mp_flags_snapshot;
    Handshake_game_callback = multi_game_player_mp_flags_handshake;
}

// 0x009DC280 (:2201): PT_REQUEST_JIP: a joining client gets a team (the disadvantaged one in a team mode), its game
// data joins the snapshot set, and it gets PT_ACCEPT_JIP (the server options, desired weapons and flags)
void multi_game_jip_request(unsigned char* packet_datap, const sender& from)
{
    const multi_game_data* data = multi_game_data_get(from.connection->game_data, false, false);
    if (data == nullptr)
    {
        return;
    }
    const human_teams old_team = data->team;
    human_teams new_team = old_team == HUMAN_TEAM_MARAUDER ? HUMAN_TEAM_MARAUDER : HUMAN_TEAM_NONE;
    if (multi_gameplay_mode_is_team_mode(multi_gameplay_mode_get()))
    {
        if (old_team != HUMAN_TEAM_MARAUDER)
        {
            if (!multi_gameplay_mode_is_team_mode(Mp_server_options.get_mode()))
            {
                new_team = HUMAN_TEAM_CIVILIAN;
            }
            else
            {
                new_team = multi_game_mode_get_jip_disadvantaged_team(ST_GAME_SNAPSHOT, from.connection->game_data);
                if (new_team == HUMAN_TEAM_NONE || new_team == HUMAN_TEAM_CIVILIAN)
                {
                    new_team = HUMAN_TEAM_EDF;
                }
            }
            if (new_team == HUMAN_TEAM_NONE)
            {
                new_team = (human_teams)(Mp_team_stats[1].score <= Mp_team_stats[0].score);
                if (new_team == HUMAN_TEAM_NONE)
                {
                    // Magic number: a coin toss.
                    new_team = (human_teams)(int_rand_range(0, 100) < 50);
                }
            }
        }
    }
    else if (old_team != HUMAN_TEAM_CIVILIAN && old_team != HUMAN_TEAM_MARAUDER)
    {
        new_team = HUMAN_TEAM_CIVILIAN;
    }
    multi_game_data_set_player_team(from.connection->game_data, new_team);
    multi_game_player_send_team_change(from.connection, new_team);
    multi_game_data_clone_handle(from.connection->game_data, ST_GAME, ST_GAME_SNAPSHOT);
    for (unsigned int handle = multi_game_data_handles_begin(ST_GAME_SNAPSHOT, false, false); handle != 0xFFFFFFFF;
         handle = multi_game_data_handles_next(handle, false, false))
    {
        multi_game_data_set_in_game(handle);
    }
    unsigned char packet[Packet_buffer_size];
    packet[0] = PT_ACCEPT_JIP;
    unsigned short size = packet_header_size(PT_ACCEPT_JIP);
    multi_game_append_server_options_to_packet(packet, size, &Mp_server_options);
    multi_game_player_append_desired_weapons_to_packet(packet, size, from.connection->owner);
    packet[size] = Skill_mismatch;
    packet[(unsigned short)(size + 1)] = Multi_match_info.use_default_time_of_day;
    size = (unsigned short)(size + 2);
    packet_set_size(packet, size);
    multi_io_send_reliable(from.connection, packet, size, nullptr, nullptr, nullptr);
    // As in rfg.exe: a walk over the snapshot set whose lookups are unused.
    for (unsigned int handle = multi_game_data_handles_begin(ST_GAME_SNAPSHOT, false, false); handle != 0xFFFFFFFF;
         handle = multi_game_data_handles_next(handle, false, false))
    {
        multi_game_data_get(handle, true, false);
    }
}

// 0x009958E0 (:2297): PT_CLIENT_JOINED_JIP: the joined client's connection is in gameplay, its game data joins the
// snapshot set and the HUD shows it
void multi_game_jip_client_joined(unsigned char* packet_data, const sender& from)
{
    int offset = 0;
    const unsigned int game_data_handle = read_u32_be(packet_data, offset);
    multi_session* session = multi_session_get_by_type(ST_GAME);
    if (session == nullptr)
    {
        return;
    }
    multi_connection* first = session->get_connection_list();
    for (multi_connection* cp = first; cp != nullptr;)
    {
        if (cp->game_data == game_data_handle)
        {
            if (cp->connection_status == CS_CONNECTED)
            {
                cp->connection_status = CS_CONNECTED_GAMEPLAY;
            }
            break;
        }
        cp = cp->next;
        if (cp == first)
        {
            break;
        }
    }
    multi_game_data_clone_handle(game_data_handle, ST_GAME, ST_GAME_SNAPSHOT);
    for (unsigned int handle = multi_game_data_handles_begin(ST_GAME_SNAPSHOT, false, false); handle != 0xFFFFFFFF;
         handle = multi_game_data_handles_next(handle, false, false))
    {
        multi_game_data_set_in_game(handle);
    }
    hud_ui_multiplayer_player_joined(game_data_handle);
}
