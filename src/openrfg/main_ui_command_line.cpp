// OpenRFG's command line through the original menus (new code, UI.md 2.7 item 2, UI5): --host/--join make the
// menus' choices through their test seams (off by default in the recovered code): the main menu goes to multiplayer
// (the system link), the system link hosts or finds games, the server browser joins --ip/--port (or with --search the
// first lobby the original search lists), the host's game lobby sets the deathmatch options (--map, the limits) and
// launches once --players are in, the results screen continues after --results-time. --sp starts a new single player
// game and --sp-load N loads save slot N from the main menu. A bad command line value is shown in a box once the main
// menu is up. Compiled with OPENRFG_UI (rfg_ui).

#include "main_options.h"

#include "kaiko/gamelink/gamelink.hpp"
#include "library/modules/runtime/gamelink/sources/global/lan/gamelinklan.hpp"
#include "keen/base/trace.hpp"
#include "mp_lobby_calls.h"
#include "volition/rfg/code/gamewide/game_save.h"
#include "volition/rfg/code/gamewide/gameseq.h"
#include "volition/rfg/code/gamewide/verify_gamesave.h"
#include "volition/rfg/code/interface/loading.h"
#include "volition/rfg/code/interface/main_menu.h"
#include "volition/rfg/code/interface/multi_player_menus/multi_player_menu_system.h"
#include "volition/rfg/code/interface/multi_player_menus/ui_document_mp_game_lobby.h"
#include "volition/rfg/code/interface/multi_player_menus/ui_document_mp_results.h"
#include "volition/rfg/code/interface/multi_player_menus/ui_document_mp_syslink.h"
#include "volition/rfg/code/interface/multi_player_menus/ui_document_mp_syslink_find_servers.h"
#include "volition/rfg/code/interface/ui_message_box.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_core/include/multi_session.h"
#include "volition/rfg/code/multi/multi_game/multi_game_lobby.h"
#include "volition/rfg/code/rfg.h"

#include <stdio.h>
#include <windows.h>

namespace
{
    enum command_line_request
    {
        REQUEST_NONE,
        REQUEST_HOST,
        REQUEST_JOIN,
    };

    command_line_request Request = REQUEST_NONE;
    bool Main_menu_done;
    bool Syslink_done;
    bool Join_done;
    bool Options_set;
    bool Launched;
    bool Results_waiting;
    unsigned long long Results_start_ms;
    multi_session_info Join_info;

    unsigned long long now_ms()
    {
        return GetTickCount64();
    }

    // --sp-load's slot: what the Load Game screen does when its row is selected from the main menu (the main menu's
    // delayed MENU_OPTION_LOAD_GAME, then hud_ui_save_load_selection_made). The rows are the screen's list, so a slot it
    // would not show (empty, or a save of another campaign) is reported in a box instead. verify_gamesave_start keeps
    // the pointer, so the list is static.
    void command_line_load_slot(int slot)
    {
        // Magic number: the save file has 12 slots (game_save_generic.cpp's Num_save_slots).
        static game_save_info Saves[12];
        World.dlc_bundle_id = -1;
        const int count = game_save_get_list(Saves, game_save_get_list_buf_size(12), 12, false);
        for (int i = 0; i < count; i++)
        {
            if (Saves[i].used_slot && (int)Saves[i].save_slot_index == slot)
            {
                keen::debug::nativePrintArgs("[ui] main menu: load save slot %d from the command line\n", slot);
                verify_gamesave_start(&Saves[i], false);
                return;
            }
        }
        char text[96];
        snprintf(text, sizeof(text), "--sp-load: slot %d has no save to load.", slot);
        ui_add_msgbox(MBT_OK, "OpenRFG command line", text, nullptr, false, false, nullptr, nullptr, false);
        keen::debug::nativePrintArgs("[ui] main menu: --sp-load: no save in slot %d\n", slot);
    }

    // The main menu's multiplayer entry (rfg.exe's delayed system link selection), with the LAN port deviation
    // (multiplayer.md 2.3) before the multiplayer start-up binds the link's socket; a bad value is shown in a box.
    void command_line_main_menu()
    {
        if (Main_menu_done || Multiplayer || gameseq_get_state() != GS_MAINMENU || gameseq_in_transition() || ui_is_msgbox_active(-2))
        {
            return;
        }
        const main_options& options = main_options_get();
        // --sp-load waits for the save data (the local game session hands it over after the boot).
        if (options.error[0] == '\0' && options.sp_load >= 0 && game_save_get_state() != GSS_NONE)
        {
            return;
        }
        Main_menu_done = true;
        if (options.error[0] != '\0')
        {
            ui_add_msgbox(MBT_OK, "OpenRFG command line", options.error, nullptr, false, false, nullptr, nullptr, false);
            return;
        }
        if (options.sp_load >= 0)
        {
            command_line_load_slot(options.sp_load);
            return;
        }
        if (options.sp)
        {
            // What video_cutscene_exit_state does after the new game's opening movie (stub_impl.md S9).
            keen::debug::nativePrintArgs("[ui] main menu: single player from the command line (%s)\n", options.sp_zone);
            World.dlc_bundle_id = -1;
            World.set_pending_territory_load(options.sp_zone, true, false, false, World.dlc_bundle_id, false);
            return;
        }
        if (!options.host && !options.join)
        {
            return;
        }
        Request = options.host ? REQUEST_HOST : REQUEST_JOIN;
        // Deviation from rfg.exe: the LAN port is chosen at the Host or Join press, so that instances on one
        // machine differ.
        kaiko::gamelink::g_openrfgLanLocalPort = main_options_choose_lan_port(Request == REQUEST_HOST);
        keen::debug::nativePrintArgs("[ui] main menu: %s from the command line (LAN port %u)\n", Request == REQUEST_HOST ? "host" : "join", (unsigned int)kaiko::gamelink::g_openrfgLanLocalPort);
        game_push_multi_init(GS_MULTIPLAYER_SYSLINK, true, true);
    }

    // The system link's choice: host (row 0) or find games (row 1), once.
    int command_line_syslink_selection()
    {
        if (Request == REQUEST_NONE || Syslink_done)
        {
            return -1;
        }
        Syslink_done = true;
        keen::debug::nativePrintArgs("[ui] system link: %s\n", Request == REQUEST_HOST ? "host game" : "find games");
        return Request == REQUEST_HOST ? 0 : 1;
    }

    // The server browser's join: --ip/--port directly, or with --search the first lobby the search found; once.
    const multi_session_info* command_line_find_servers_join()
    {
        if (Request != REQUEST_JOIN || Join_done)
        {
            return nullptr;
        }
        const main_options& options = main_options_get();
        if (options.search)
        {
            if (s_lan_num_search_results == 0)
            {
                return nullptr;
            }
            Join_done = true;
            keen::debug::nativePrintArgs("[ui] server browser: joining the first lobby found\n");
            return &s_lan_server_infos[0].session_info;
        }
        unsigned int ip = 0;
        unsigned short port = 0;
        main_options_direct_lobby(&ip, &port);
        kaiko::GameLinkLobby lobby;
        lobby.id.asUint64 = 0;
        lobby.id.asNetworkAddress.ipAddress.asUint32 = ip;
        lobby.id.asNetworkAddress.port = port;
        mp_lobby_make_join_info(Join_info, lobby);
        Join_done = true;
        keen::debug::nativePrintArgs("[ui] server browser: joining %u.%u.%u.%u:%u\n", ip & 0xFF, (ip >> 8) & 0xFF, (ip >> 16) & 0xFF, ip >> 24, (unsigned int)port);
        return &Join_info;
    }

    // The host's game lobby: the deathmatch options once the lobby session is ready (the game options menu's calls),
    // then the launch once --players are in.
    bool command_line_game_lobby_launch()
    {
        if (Request != REQUEST_HOST || Launched || Lobby_session == nullptr || !Lobby_session->am_i_host())
        {
            return false;
        }
        const main_options& options = main_options_get();
        if (!Options_set)
        {
            if (mp_lobby_session_ready() != 1)
            {
                return false;
            }
            Options_set = true;
            if (!mp_lobby_set_server_options(options.map, options.point_limit, options.time_limit, options.rounds))
            {
                char text[160];
                snprintf(text, sizeof(text), "--map: \"%s\" is none of the multiplayer levels.", options.map);
                ui_add_msgbox(MBT_OK, "MENU_MP_GAME_OPTIONS", text, nullptr, false, false, nullptr, nullptr, false);
                keen::debug::nativePrintArgs("[ui] game lobby: no level %s\n", options.map);
                Launched = true;
                return false;
            }
            keen::debug::nativePrintArgs("[ui] game lobby: deathmatch on %s\n", options.map);
        }
        if (!mp_lobby_host_may_launch(options.players))
        {
            return false;
        }
        Launched = true;
        keen::debug::nativePrintArgs("[ui] game lobby: launch with %d players\n", Lobby_session->get_slots_used());
        return true;
    }

    // The results screen continues after --results-time seconds (0: it waits for the player).
    bool command_line_results_continue()
    {
        const int wait_s = main_options_results_time();
        if (wait_s <= 0)
        {
            return false;
        }
        if (!Results_waiting)
        {
            Results_waiting = true;
            Results_start_ms = now_ms();
            return false;
        }
        if (now_ms() - Results_start_ms < (unsigned long long)wait_s * 1000ull)
        {
            return false;
        }
        Results_waiting = false;
        keen::debug::nativePrintArgs("[ui] results: continue\n");
        return true;
    }

    const bool Hooked = (g_openrfgMainMenuProcess = command_line_main_menu, g_openrfgSyslinkSelection = command_line_syslink_selection, g_openrfgFindServersJoin = command_line_find_servers_join, g_openrfgGameLobbyLaunch = command_line_game_lobby_launch, g_openrfgResultsContinue = command_line_results_continue, true);
}
