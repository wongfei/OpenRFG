// Original: library/modules/runtime/gameframework/sources/win32/gameframework_win32.hpp
//
// GameLinkInvitationHandler, the Steam callback that turns game invitations (join requests) into GameLink invites on
// Windows.
#pragma once

#include "library/modules/thirdparty/emgiec/emgiec_steam_142/sdk/public/steam/isteamfriends.h"
#include "library/modules/thirdparty/emgiec/emgiec_steam_142/sdk/public/steam/steam_api_internal.h"

// size 0x14
class GameLinkInvitationHandler
{
public:
    GameLinkInvitationHandler(); // 0x00C10D00 (:1014)
private:
    void handleLobbyJoinRequest(GameLobbyJoinRequested_t* pCallback); // 0x00C100B0 (:1020)
public:
    ~GameLinkInvitationHandler(); // 0x00C10130

private:
    CCallback<GameLinkInvitationHandler,GameLobbyJoinRequested_t,0> m_callbackLobbyJoinRequested; // 0x00
};
