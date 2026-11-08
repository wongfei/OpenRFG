// The calls the multiplayer menus make into the original lobby (new code, not recovered). See mp_lobby_calls.h.

#include "mp_lobby_calls.h"

#include "kaiko/gamelink/gamelink.hpp"
#include "volition/rfg/code/multi/multi_core/generic/multi_core_session_generic.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_core/include/multi_session.h"
#include "volition/rfg/code/multi/multi_game/multi_game_lobby.h"
#include "volition/rfg/code/multi/multi_game/multi_game_settings.h"

bool mp_lobby_set_server_options(const char* map, int point_limit, int time_limit, int rounds)
{
    const multi_level_info* level = multi_game_settings_find_level_by_filename(map);
    if (level == nullptr)
    {
        level = multi_game_settings_find_level_by_name(map);
    }
    if (level == nullptr)
    {
        return false;
    }
    Mp_server_options.set_mode(MGPM_DEATHMATCH);
    Mp_server_options.set_level(level);
    if (point_limit >= 0)
    {
        Mp_server_options.generic_options.point_limit = (unsigned short)point_limit;
    }
    if (time_limit >= 0)
    {
        Mp_server_options.generic_options.round_time_limit_minutes = (unsigned char)time_limit;
    }
    if (rounds >= 0)
    {
        Mp_server_options.generic_options.num_rounds = (unsigned char)rounds;
    }
    return true;
}

void mp_lobby_make_join_info(multi_session_info& info, const kaiko::GameLinkLobby& lobby)
{
    info.type = ST_GAME;
    // Magic number: rfg.exe's LAN lobbies have 16 slots (the search result's maxNumPlayers).
    info.max_slots = 16;
    info.systemlink_or_lan = true;
    info.invite_type = SIT_ALL;
    info.allow_join_in_progress = true;
    info.uses_peer_to_peer_networking = true;
    info.m_lobbyId = lobby;
}

int mp_lobby_session_ready()
{
    const multi_session_state state = Lobby_session->get_state();
    if (state >= MSS_READY && state <= MSS_IN_GAME)
    {
        return 1;
    }
    return state == MSS_NONE && !multi_game_lobby_pending_join() ? -1 : 0;
}

bool mp_lobby_host_may_launch(int players)
{
    return Lobby_session->get_state() == MSS_READY && Lobby_session->get_slots_used() >= players && multi_game_lobby_waiting_for_user_launch();
}
