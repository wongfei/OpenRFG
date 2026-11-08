// The calls the multiplayer menus make into the original lobby (new code, not recovered; multiplayer.md 2.4,
// orig_main.md OM6). OpenRFG's command line (main_ui_command_line.cpp) makes them through these functions.
#pragma once

namespace kaiko
{
    struct GameLinkLobby;
}
class multi_session_info;

// The host's game options (the game options menu, GS_MULTIPLAYER_CHANGE_GAME_OPTIONS): deathmatch on the map (a
// level's file name or display name), the limits when >= 0 (else the mode's defaults). multi_game_lobby_create
// reinits Mp_server_options while the lobby session's info is not yet a system link one (multiplayer.md MP4), so this
// comes after the create, once the lobby session is ready. False when the map is none of the settings' levels.
bool mp_lobby_set_server_options(const char* map, int point_limit, int time_limit, int rounds);

// The server browser's entry for a LAN lobby (a system link game session with the lobby's GameLink id, as
// mpms_server_search_process 0x008E1E50 fills s_lan_server_infos), for multi_game_lobby_join.
void mp_lobby_make_join_info(multi_session_info& info, const kaiko::GameLinkLobby& lobby);

// The lobby session after a create or a join: 1 ready, or already past it (a client whose join completes in the
// frame the host launches goes on to MSS_STARTING / MSS_IN_GAME first); -1 it ended (MSS_NONE, no join pending); 0
// still on its way.
int mp_lobby_session_ready();

// The host may launch (the game lobby menu's launch, mp_game_lobby_process_input): the lobby session is ready with
// `players` slots used and the lobby waits for the user's launch.
bool mp_lobby_host_may_launch(int players);
