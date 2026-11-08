// Original: library/modules/runtime/gamelink/include/kaiko/gamelink/gamelink.hpp
// Implementation: library/modules/runtime/gamelink/sources/global/gamelink.cpp
//
// GameLink, the multiplayer transport interface: lobbies (create, find, join, filters, members, owner), parties, packets
// to the other stations of the lobby, voice chat and errors (each error maps to a game session interaction). The LAN
// implementation (GameLinkLan) broadcasts on the local network, the internet one (GameLinkInternet) uses Steam lobbies
// and P2P networking. Creation and session parameters, remote stations and lobby ids, search results, and the lobby
// invitation the game was started with (setInvitationData/getInvitationData, gamelinkinternet_steam.cpp). Placement of
// the enums, search results, voice chat info and invitation data inferred.
#pragma once

#include "keen/base/types.hpp"
#include "kaiko/gamesessioninteractions/gamesessioninteractions.hpp"
#include "unit4/systemservices/systemservices.hpp"
#include "keen/useraccount/useraccounttypes.hpp"
#include "library/modules/thirdparty/emgiec/emgiec_steam_142/sdk/public/steam/steamclientpublic.h"

namespace keen
{
    namespace SoundSystem
    {
        struct System;
    }
}

namespace unit4
{
    struct SystemServices;
}

namespace kaiko
{
    enum GameLinkConstants
    {
        GameLinkConstants_MaxUserDisplayNameLength = 64,
        GameLinkConstants_FindAndJoinMaxSearchResults = 10,
    };

    enum GameLinkConnectionType
    {
        GameLinkConnectionType_Lan = 0,
        GameLinkConnectionType_Internet = 1,
        GameLinkConnectionType_Wireless = 2,
        GameLinkConnectionType_Count = 3,
        GameLinkConnectionType_Default = 0,
    };

    enum GameLinkState
    {
        GameLinkState_Offline = 0,
        GameLinkState_Online = 1,
        GameLinkState_InLobby = 2,
    };

    enum GameLinkError
    {
        GameLinkError_None = 0,
        GameLinkError_AskToEnableParty = 1,
        GameLinkError_WaitForPermissionCheck = 2,
        GameLinkError_NoVoiceChatUnknown = 3,
        GameLinkError_NoVoiceChatSubscriptionRequired = 4,
        GameLinkError_NoVoiceChatBanned = 5,
        GameLinkError_NoVoiceChatRestricted = 6,
        GameLinkError_CreatePartyCancelled = 7,
        GameLinkError_SESSION_ERROR_START = 8,
        GameLinkError_KickedFromLobby = 9,
        GameLinkError_JoinLobbyFailed_Unknown = 10,
        GameLinkError_JoinLobbyFailed_Timeout = 11,
        GameLinkError_JoinLobbyFailed_Locked = 12,
        GameLinkError_JoinLobbyFailed_NotAllowedToJoin = 13,
        GameLinkError_JoinLobbyFailed_WrongPassword = 14,
        GameLinkError_JoinLobbyFailed_Full = 15,
        GameLinkError_JoinLobbyFailed_NotFound = 16,
        GameLinkError_CreateLobbyFailed_Unknown = 17,
        GameLinkError_CreateLobbyFailed_Timeout = 18,
        GameLinkError_UserBanned = 19,
        GameLinkError_UserBlocked = 20,
        GameLinkError_HostMigrationFailed = 21,
        GameLinkError_General_Timeout = 22,
        GameLinkError_SessionLost = 23,
        GameLinkError_CRITICAL_ERROR_START = 24,
        GameLinkError_Unknown = 25,
        GameLinkError_LatestSystemSoftwareExists = 26,
        GameLinkError_LatestSystemSoftwareExistsForTitle = 27,
        GameLinkError_LatestPatchPackageExists = 28,
        GameLinkError_LatestPatchPackageDownloaded = 29,
        GameLinkError_PlaystationPlusRequired = 30,
        GameLinkError_NoMultiplayerUnknown = 31,
        GameLinkError_NoMultiplayerSubscriptionRequired = 32,
        GameLinkError_NoMultiplayerBanned = 33,
        GameLinkError_NoMultiplayerRestricted = 34,
        GameLinkError_SignedOut = 35,
        GameLinkError_NotSignedIn = 36,
        GameLinkError_AgeRestricted = 37,
        GameLinkError_ServerNotAvailable = 38,
        GameLinkError_ServiceUnavailable = 39,
        GameLinkError_NoNetworkAccess = 40,
        GameLinkError_NoInternetAccess = 41,
        GameLinkError_InternetConnectionLost = 42,
        GameLinkError_NetworkConnectionLost = 43,
    };

    enum GameLinkFindAndJoinStatus
    {
        GameLinkFindAndJoinStatus_None = 0,
        GameLinkFindAndJoinStatus_Running = 1,
        GameLinkFindAndJoinStatus_FoundAndJoinedLobby = 2,
        GameLinkFindAndJoinStatus_Failed = 3,
    };

    enum GameLinkLobbyType
    {
        GameLinkLobbyType_None = 0,
        GameLinkLobbyType_Public = 1,
        GameLinkLobbyType_Friends = 2,
        GameLinkLobbyType_Private = 3,
    };

    // size 0x10
    struct GameLinkGeneralCreationParameters
    {
        GameLinkGeneralCreationParameters(); // 0x00941810 (.h:101)

        uint8 maxNumLobbyMembers; // 0x00
        uint8 maxNumPartyMembers; // 0x01
        uint8 maxNumMatchmakingResults; // 0x02
        uint32 findAndJoinTimeout; // 0x04
        keen::SoundSystem::System* pSoundSystem; // 0x08
        unit4::SystemServices* pSystemServices; // 0x0C
    };

    // size 0x44
    struct GameLinkLanCreationParameters
    {
        GameLinkLanCreationParameters(); // 0x00941830 (.h:117)

        uint32 maxNumStoredUserDefinedPackets; // 0x00
        char localUserName[64]; // 0x04
    };

    // size 0x1
    struct GameLinkInternetCreationParameters
    {
        GameLinkInternetCreationParameters(); // 0x00941840 (.h:136)

        bool vacEnabled; // 0x00
    };

    // size 0x1
    struct GameLinkWirelessCreationParameters
    {
    };

    // size 0x30
    struct GameLinkSessionParameters
    {
        // size 0x2C
        struct Lan
        {
            uint16 portMatchmakingBroadcast; // 0x00
            uint32 gameUniqueIdentifier; // 0x04
            uint32 searchLobbyDuration; // 0x08
            uint32 sendLobbyInfoRequestDeltaTime; // 0x0C
            uint32 sendLobbyStateDeltaTime; // 0x10
            uint32 sendJoinRequestDeltaTime; // 0x14
            uint32 sendKeepAliveDeltaTime; // 0x18
            uint32 timeoutLobbyMember; // 0x1C
            uint32 timeoutJoin; // 0x20
            uint32 timeoutReceiveFirstLobbyStateUpdate; // 0x24
            bool hostMigrationEnabled; // 0x28
        };

        // size 0x9
        struct Internet
        {
            keen::UserAccountId mainUser; // 0x00
        };

        // size 0x1
        struct Wireless
        {
        };

        GameLinkSessionParameters(); // 0x00970290 (.h:189)

        GameLinkConnectionType connectionType; // 0x00
        union
        {
            Lan lan; // 0x04
            Internet internet; // 0x04
            Wireless wireless; // 0x04
        };
    };

    // size 0x4C
    struct GameLinkRemoteStation
    {
        bool is_equal(const GameLinkRemoteStation* pOther) const; // 0x00941850 (.h:217)
        bool is_equal(const GameLinkRemoteStation&) const;

        unit4::RemoteUserId id; // 0x00
        char userName[64]; // 0x0A
        bool myStation; // 0x4A
        bool lobbyOwner; // 0x4B
    };

    // size 0x8
    union GameLinkLobbyId
    {
        uint64 asUint64; // 0x00
        struct
        {
            // size 0x4
            union IpAddressUnion
            {
                // size 0x4
                struct IpBytes
                {
                    uint8 ipByte0; // 0x00
                    uint8 ipByte1; // 0x01
                    uint8 ipByte2; // 0x02
                    uint8 ipByte3; // 0x03
                };

                union
                {
                    uint32 asUint32; // 0x00
                    IpBytes as4Bytes; // 0x00
                };
            };

            IpAddressUnion ipAddress; // 0x00
            uint16 port; // 0x04
            uint16 padding; // 0x06
        } asNetworkAddress; // 0x00
    };

    // size 0x8
    struct GameLinkLobby
    {
        bool is_equal(const GameLinkLobby*) const;
        bool is_equal(const GameLinkLobby& other) const; // 0x005D0D40 (.h:266)
        bool is_valid() const; // 0x005D0DD0 (.h:296)

        GameLinkLobbyId id; // 0x00
    };

    // size 0x50
    struct GameLinkSearchResult
    {
        GameLinkLobby lobby; // 0x00
        uint8 currentNumPlayers; // 0x08
        uint8 maxNumPlayers; // 0x09
        char lobbyName[64]; // 0x0A
    };

    // size 0x3
    struct GameLinkVoiceChatPlayerInfo
    {
        bool hasHeadset; // 0x00
        bool isMuted; // 0x01
        bool hasJustSpoken; // 0x02
    };

    // size 0x10
    struct GameLinkInvitationData
    {
        GameLinkInvitationData(); // 0x00458C30

        CSteamID lobbyId; // 0x00
        CSteamID friendId; // 0x08
    };

    // size 0x8
    struct GameLinkErrorInteractionId
    {
        GameLinkError error; // 0x00
        GameSessionInteractionId interactionId; // 0x04
    };

    // The error -> user interaction map of GameLink::getInteractionId. A static of this header: rfg.exe has 33 copies
    // (every TU that includes it), all equal (dumped from 0x016F21B8, gamelink.cpp's copy, on 2026-10-19).
    // Original data: CreateLobbyFailed_Timeout maps to the CreateLobbyFailed_Unknown interaction, not _Timeout.
    static const GameLinkErrorInteractionId s_mapErrorInteractionId[42] =
    {
        { GameLinkError_None, GameSessionInteractionId_None },
        { GameLinkError_CreatePartyCancelled, GameSessionInteractionId_CreatePartyCancelled },
        { GameLinkError_KickedFromLobby, GameSessionInteractionId_KickedFromLobby },
        { GameLinkError_JoinLobbyFailed_Unknown, GameSessionInteractionId_JoinLobbyFailed_Unknown },
        { GameLinkError_JoinLobbyFailed_Timeout, GameSessionInteractionId_JoinLobbyFailed_Timeout },
        { GameLinkError_JoinLobbyFailed_Locked, GameSessionInteractionId_JoinLobbyFailed_Locked },
        { GameLinkError_JoinLobbyFailed_NotAllowedToJoin, GameSessionInteractionId_JoinLobbyFailed_NotAllowedToJoin },
        { GameLinkError_JoinLobbyFailed_WrongPassword, GameSessionInteractionId_JoinLobbyFailed_WrongPassword },
        { GameLinkError_JoinLobbyFailed_Full, GameSessionInteractionId_JoinLobbyFailed_Full },
        { GameLinkError_JoinLobbyFailed_NotFound, GameSessionInteractionId_JoinLobbyFailed_NotFound },
        { GameLinkError_CreateLobbyFailed_Unknown, GameSessionInteractionId_CreateLobbyFailed_Unknown },
        { GameLinkError_CreateLobbyFailed_Timeout, GameSessionInteractionId_CreateLobbyFailed_Unknown },
        { GameLinkError_General_Timeout, GameSessionInteractionId_General_Timeout },
        { GameLinkError_HostMigrationFailed, GameSessionInteractionId_HostMigrationFailed },
        { GameLinkError_SessionLost, GameSessionInteractionId_SessionLost },
        { GameLinkError_ServerNotAvailable, GameSessionInteractionId_ServerNotAvailable },
        { GameLinkError_LatestSystemSoftwareExists, GameSessionInteractionId_NoMultiplayerSystemupdateRequired },
        { GameLinkError_LatestSystemSoftwareExistsForTitle, GameSessionInteractionId_NoMultiplayerSystemupdateRequired },
        { GameLinkError_LatestPatchPackageExists, GameSessionInteractionId_NoMultiplayerGameUpdateRequired },
        { GameLinkError_LatestPatchPackageDownloaded, GameSessionInteractionId_NoMultiplayerGameUpdateRequired },
        { GameLinkError_NoVoiceChatUnknown, GameSessionInteractionId_NoVoiceChatUnknown },
        { GameLinkError_NoVoiceChatSubscriptionRequired, GameSessionInteractionId_NoVoiceChatSubscriptionRequired },
        { GameLinkError_NoVoiceChatBanned, GameSessionInteractionId_NoVoiceChatBanned },
        { GameLinkError_NoVoiceChatRestricted, GameSessionInteractionId_NoVoiceChatAgeRestricted },
        { GameLinkError_NoMultiplayerUnknown, GameSessionInteractionId_NoMultiplayerUnknown },
        { GameLinkError_NoMultiplayerSubscriptionRequired, GameSessionInteractionId_NoMultiplayerSubscriptionRequired },
        { GameLinkError_NoMultiplayerBanned, GameSessionInteractionId_NoMultiplayerBanned },
        { GameLinkError_NoMultiplayerRestricted, GameSessionInteractionId_NoMultiplayerRestricted },
        { GameLinkError_PlaystationPlusRequired, GameSessionInteractionId_NoMultiplayerSubscriptionRequired },
        { GameLinkError_NotSignedIn, GameSessionInteractionId_NotSignedIn },
        { GameLinkError_SignedOut, GameSessionInteractionId_SignedOutMultiplayer },
        { GameLinkError_AgeRestricted, GameSessionInteractionId_NoMultiplayerAgeRestricted },
        { GameLinkError_NoNetworkAccess, GameSessionInteractionId_NoNetworkAccess },
        { GameLinkError_NoInternetAccess, GameSessionInteractionId_NoInternetAccess },
        { GameLinkError_NetworkConnectionLost, GameSessionInteractionId_NetworkConnectionLost },
        { GameLinkError_InternetConnectionLost, GameSessionInteractionId_InternetConnectionLost },
        { GameLinkError_Unknown, GameSessionInteractionId_Unknown },
        { GameLinkError_ServiceUnavailable, GameSessionInteractionId_ServiceUnavailable },
        { GameLinkError_UserBanned, GameSessionInteractionId_NoMultiplayerBanned },
        { GameLinkError_UserBlocked, GameSessionInteractionId_UserBlocked },
        { GameLinkError_AskToEnableParty, GameSessionInteractionId_AskToEnableParty },
        { GameLinkError_WaitForPermissionCheck, GameSessionInteractionId_WaitForPermissionCheck },
    };

    namespace gamelink
    {
        // size 0x18
        class GameLink
        {
        public:
            GameLink(); // 0x00CD0130 (.h:336)
            virtual ~GameLink(); // vtbl+0x00 0x00CD0150 (.h:338)
            virtual bool findAndJoinLobby(uint32, const uint32*, uint32) = 0; // vtbl+0x04
            virtual GameLinkFindAndJoinStatus getFindAndJoinStatus() const = 0; // vtbl+0x08
            virtual void cancelFindAndJoinLobby() = 0; // vtbl+0x0C
            virtual bool beginSession(const GameLinkSessionParameters&) = 0; // vtbl+0x10
            virtual void endSession() = 0; // vtbl+0x14
            virtual void setPartyEnabled(bool) = 0; // vtbl+0x18
            virtual bool isPartyEnabled() const = 0; // vtbl+0x1C
            virtual bool isPartyOwner() const = 0; // vtbl+0x20
            virtual bool getPartyOwner(unit4::NamedRemoteUserId*) const = 0; // vtbl+0x24
            virtual uint32 getPartyMembers(unit4::NamedRemoteUserId*, uint32) const = 0; // vtbl+0x28
            virtual uint32 getNumPartyMembers() const = 0; // vtbl+0x2C
            virtual void update(float) = 0; // vtbl+0x30
            virtual bool createLobby(GameLinkLobbyType, uint32, const uint32*, uint32) = 0; // vtbl+0x34
            virtual bool startSearchLobby(uint32, const uint32*, uint32) = 0; // vtbl+0x38
            virtual uint32 getSearchResults(GameLinkSearchResult*, uint32, bool*) const = 0; // vtbl+0x3C
            virtual bool joinLobby(const GameLinkInvitationData*) = 0; // vtbl+0x40
            virtual bool joinLobby(const GameLinkLobby&) = 0; // vtbl+0x44
            bool joinLobby(const GameLinkSearchResult&);
            virtual void leaveLobby() = 0; // vtbl+0x48
            virtual void openInvitationDialog() = 0; // vtbl+0x4C
            virtual uint32 getLobbyMembers(const GameLinkRemoteStation**, uint32) const = 0; // vtbl+0x50
            virtual bool setLobbyFilters(const uint32*, const uint32*, uint32) = 0; // vtbl+0x54
            virtual uint32 getLobbyFilters(uint32*, uint32) const = 0; // vtbl+0x58
            virtual bool sendP2PPacketUnreliable(const GameLinkRemoteStation*, const void*, uint32) = 0; // vtbl+0x5C
            virtual uint32 getNextP2PPacket(void*, uint32, const GameLinkRemoteStation**) = 0; // vtbl+0x60
            virtual bool setLobbyType(GameLinkLobbyType) = 0; // vtbl+0x64
            virtual bool setLobbyLocked(bool) = 0; // vtbl+0x68
            virtual bool setLobbyOwner(unit4::RemoteUserId) = 0; // vtbl+0x6C
            virtual uint32 getLobbyMaxMembers() const = 0; // vtbl+0x70
            virtual uint32 getNumLobbyMembers() const = 0; // vtbl+0x74
            virtual GameLinkLobby getLobbyId() const = 0; // vtbl+0x78
            virtual const GameLinkRemoteStation* getLobbyOwner() const = 0; // vtbl+0x7C
            virtual bool isLobbyOwner() const = 0; // vtbl+0x80
            virtual const GameLinkRemoteStation* getRemoteStationById(unit4::RemoteUserId) const = 0; // vtbl+0x84
            virtual void setVoiceChatEnabled(bool) = 0; // vtbl+0x88
            virtual void setVoiceChatLocalPlayerMuted(bool) = 0; // vtbl+0x8C
            virtual void setVoiceChatRemotePlayerMuted(const GameLinkRemoteStation&, bool) = 0; // vtbl+0x90
            virtual bool getVoiceChatLocalPlayerInfo(GameLinkVoiceChatPlayerInfo*) = 0; // vtbl+0x94
            virtual bool getVoiceChatRemotePlayerInfo(GameLinkVoiceChatPlayerInfo*, const GameLinkRemoteStation&) = 0; // vtbl+0x98
            virtual bool hasVoiceChatPermissions() const = 0; // vtbl+0x9C
            virtual void setVoiceChatVolume(float) = 0; // vtbl+0xA0
            GameLinkState getCurrentState() const; // 0x005D0DE0 (.h:661)
            GameLinkError getError(int* pOutPlatformErrorCode); // 0x00883570 (.h:670)
            virtual void clearError(); // vtbl+0xA4 0x00CD0160 (.h:681)
            static GameSessionInteractionId getInteractionId(GameLinkError error); // 0x00CD07F0 (:8)
            void setGameplayRunning(bool running); // 0x00941870 (.h:697)
        protected:
            bool setError(GameLinkError error, int platformErrorCode); // 0x00CD0170 (.h:707)
            uint32 generateRequestId(); // 0x00CD01A0 (.h:719)

            // 0x00 vfptr
            GameLinkState m_state; // 0x04
            GameLinkError m_error; // 0x08
            int m_platformErrorCode; // 0x0C
            bool m_gameplayRunning; // 0x10
            float m_voicechat_volume; // 0x14
        };

    }

    namespace gamelink
    {
        void clearInvitationData(); // 0x00941880 (.h:757)
        void setInvitationData(const GameLinkInvitationData* pInvitationData); // 0x00CD1830 (gamelinkinternet_steam.cpp:77)
        const GameLinkInvitationData* getInvitationData(); // 0x00CD0430 (gamelinkinternet_steam.cpp:91)
    }
}
