// Original: library/modules/runtime/gamelink/sources/steam/gamelinkinternet_steam.hpp
// Implementation: library/modules/runtime/gamelink/sources/steam/gamelinkinternet_steam.cpp
//
// GameLinkInternet on Steam: matchmaking lobbies (a game lobby and a party lobby), P2P packets between lobby members,
// Steam voice chat played through XAudio2 voices (CVoiceContext), and VAC auth tickets exchanged with the other members.
// The Steam callbacks and call results are members. Placement of the Steam packet, party and auth ticket types inferred.
#pragma once

#include <xaudio2.h>
#include "keen/base/array.hpp"
#include "keen/base/memoryallocator.hpp"
#include "keen/base/randomnumbergenerator.hpp"
#include "keen/base/sizedarray.hpp"
#include "kaiko/gamelink/gamelink.hpp"
#include "library/modules/runtime/gamelink/sources/global/gamelinkinternal.hpp"
#include "keen/sound/soundsystem.hpp"
#include "unit4/systemservices/systemservices.hpp"
#include "library/modules/thirdparty/emgiec/emgiec_steam_142/sdk/public/steam/isteammatchmaking.h"
#include "library/modules/thirdparty/emgiec/emgiec_steam_142/sdk/public/steam/isteamnetworking.h"
#include "library/modules/thirdparty/emgiec/emgiec_steam_142/sdk/public/steam/isteamuser.h"
#include "library/modules/thirdparty/emgiec/emgiec_steam_142/sdk/public/steam/steam_api_internal.h"
#include "library/modules/thirdparty/emgiec/emgiec_steam_142/sdk/public/steam/steamclientpublic.h"

struct IXAudio2;
struct IXAudio2SourceVoice;

namespace keen
{
    namespace SoundSystem
    {
        struct System;
    }
}

// size 0x10
class CVoiceContext : public IXAudio2VoiceCallback
{
public:
    CVoiceContext(); // 0x00CD1720 (gamelinkinternet_steam.cpp:21)
    void create(keen::MemoryAllocator* pAllocator); // 0x00CD0410 (gamelinkinternet_steam.cpp:30)
    virtual void __stdcall OnVoiceProcessingPassStart(UINT32); // vtbl+0x00 0x00CD1770 (gamelinkinternet_steam.cpp:35)
    virtual void __stdcall OnVoiceProcessingPassEnd(); // vtbl+0x04 0x00CD1780 (gamelinkinternet_steam.cpp:38)
    virtual void __stdcall OnStreamEnd(); // vtbl+0x08 0x00CD1790 (gamelinkinternet_steam.cpp:41)
    virtual void __stdcall OnBufferStart(void*); // vtbl+0x0C 0x00CD17A0 (gamelinkinternet_steam.cpp:44)
    virtual void __stdcall OnBufferEnd(void* pBuffer); // vtbl+0x10 0x00CD17B0 (gamelinkinternet_steam.cpp:47)
    virtual void __stdcall OnLoopEnd(void*); // vtbl+0x14 0x00CD17E0 (gamelinkinternet_steam.cpp:52)
    virtual void __stdcall OnVoiceError(void*, HRESULT); // vtbl+0x18 0x00CD17F0 (gamelinkinternet_steam.cpp:55)
    virtual ~CVoiceContext(); // vtbl+0x1C 0x00CD1750 (gamelinkinternet_steam.cpp:25)

    // 0x00 vfptr
    void* m_hBufferEndEvent; // 0x04
    IXAudio2SourceVoice* m_pSourceVoice; // 0x08
private:
    keen::MemoryAllocator* m_pAllocator; // 0x0C
};

namespace kaiko
{
    enum GameLinkSteamPacketType
    {
        GameLinkSteamPacketType_VoiceData = 0,
        GameLinkSteamPacketType_UserDefined = 1,
        GameLinkSteamPacketType_VACAuthTicketRequest = 2,
        GameLinkSteamPacketType_VACAuthTicketResponse = 3,
    };

    enum GameLinkSteamErrorCodeOrigin
    {
        GameLinkSteamErrorCodeOrigin_EResult = 0x20000000,
        GameLinkSteamErrorCodeOrigin_EResult_handleLobbyCreationShared = 0x10000,
        GameLinkSteamErrorCodeOrigin_EChatRoomEnterResponse = 0x30000000,
    };

    enum GameLinkSteamAddtionalPlatformErrorCodes
    {
        GameLinkSteamAddtionalPlatformErrorCodes_NoInternetAccess_update = 0x10000001,
        GameLinkSteamAddtionalPlatformErrorCodes_CreateLobbyFailed_Timeout_update_party = 0x10000002,
        GameLinkSteamAddtionalPlatformErrorCodes_JoinLobbyFailed_Timeout_update_party = 0x10000003,
        GameLinkSteamAddtionalPlatformErrorCodes_CreateLobbyFailed_Timeout_update_lobby = 0x10000004,
        GameLinkSteamAddtionalPlatformErrorCodes_JoinLobbyFailed_Timeout_update_lobby = 0x10000005,
    };

    // size 0x8
    struct GameLinkSteamPacketHeader
    {
        GameLinkSteamPacketType packetType; // 0x00
        uint32 actualSize; // 0x04
    };

    // size 0x5C
    struct GameLinkRemoteStationSteam : public GameLinkRemoteStation
    {
        GameLinkVoiceChatPlayerInfo voiceChatInfo; // 0x4C
        float lastVoiceDataTimeStamp; // 0x50
        bool playedWithRequested; // 0x54
        bool isBanned; // 0x55
        bool hasValidatedMyAuthTicket; // 0x56
        bool hasSentHisAuthTicket; // 0x57
        uint32 timeEnteredLobby; // 0x58
    };

    // size 0x10
    struct GameLinkSteamPartyLobbyData
    {
        GameLinkSteamPartyLobbyData(); // 0x00CD0460

        CSteamID lobbyId; // 0x00
        CSteamID lastLobbyId; // 0x08
    };

    // size 0x8
    struct GameLinkSteamVACAuthTicketResponse
    {
        EBeginAuthSessionResult beginAuthSessionResult; // 0x00
        EAuthSessionResponse authSessionResponse; // 0x04
    };

    // size 0x40C
    struct GameLinkSteamVACLocalAuthTicket
    {
        bool inUse; // 0x00
        uint32 handle; // 0x04
        char data[1024]; // 0x08
        uint32 size; // 0x408
    };

    // size 0x410
    struct GameLinkSteamVACRemoteAuthTicket
    {
        GameLinkSteamVACRemoteAuthTicket(); // 0x00CD04B0

        bool inUse; // 0x00
        CSteamID steamID; // 0x01
        char data[1024]; // 0x09
        uint32 size; // 0x40C
    };

    // size 0x10
    struct GameLinkSteamVoiceChatData
    {
        GameLinkSteamVoiceChatData(CSteamID a_steamID, CVoiceContext* a_voiceContext); // 0x00CD03E0 (.h:135)

        CSteamID steamID; // 0x00
        bool hasLeft; // 0x08
        CVoiceContext* pVoiceContext; // 0x0C
    };

    namespace gamelink
    {
        // size 0x114B0
        class GameLinkInternet : public GameLink, public keen::SoundDeviceChangedCallback
        {
        public:
            GameLinkInternet(); // 0x00CD77F0 (:107)
            virtual ~GameLinkInternet(); // vtbl+0x00 0x00CD4070 (:111)
            void create(keen::MemoryAllocator* pAllocator, const GameLinkInternetCreationParameters& params, const GameLinkGeneralCreationParameters& paramsGeneral); // 0x00CD4200 (:115)
            void destroy(keen::MemoryAllocator* pAllocator); // 0x00CD43B0 (:147)
            virtual bool beginSession(const GameLinkSessionParameters& params); // vtbl+0x10 0x00CD1940 (:162)
            virtual void endSession(); // vtbl+0x14 0x00CD5D50 (:185)
            virtual bool findAndJoinLobby(uint32 maxNumLobbyMembers, const uint32* pLobbyFilters, uint32 numLobbyFilters); // vtbl+0x04 0x00CD44B0 (:212)
            virtual GameLinkFindAndJoinStatus getFindAndJoinStatus() const; // vtbl+0x08 0x00CD04D0 (:254)
            virtual void cancelFindAndJoinLobby(); // vtbl+0x0C 0x00CD04E0 (:259)
            virtual void setPartyEnabled(bool enabled); // vtbl+0x18 0x00CD8D50 (:270)
            virtual bool isPartyEnabled() const; // vtbl+0x1C 0x00CD0500 (:286)
            virtual bool isPartyOwner() const; // vtbl+0x20 0x00CD4570 (:291)
            virtual bool getPartyOwner(unit4::NamedRemoteUserId* pOutHost) const; // vtbl+0x24 0x00CD45F0 (:309)
            virtual uint32 getPartyMembers(unit4::NamedRemoteUserId* pPartyMembers, uint32 capacity) const; // vtbl+0x28 0x00CD19B0 (:332)
            virtual uint32 getNumPartyMembers() const; // vtbl+0x2C 0x00CD1A40 (:350)
            virtual void update(float timeStep); // vtbl+0x30 0x00CD7C30 (:361)
            virtual bool createLobby(GameLinkLobbyType lobbyType, uint32 maxNumLobbyMembers, const uint32* pLobbyFilters, uint32 numLobbyFilters); // vtbl+0x34 0x00CD8380 (:707)
            virtual bool startSearchLobby(uint32 maxNumResults, const uint32* pLobbyFilters, uint32 numLobbyFilters); // vtbl+0x38 0x00CD4660 (:757)
            virtual uint32 getSearchResults(GameLinkSearchResult* pSearchResults, uint32 capacity, bool* pSearchRunning) const; // vtbl+0x3C 0x00CD1A60 (:795)
            virtual bool joinLobby(const GameLinkInvitationData* pInvitationData); // vtbl+0x40 0x00CD8500 (:837)
            virtual bool joinLobby(const GameLinkLobby& lobby); // vtbl+0x44 0x00CD84D0 (:823)
            virtual void leaveLobby(); // vtbl+0x48 0x00CD5DB0 (:886)
            virtual void openInvitationDialog(); // vtbl+0x4C 0x00CD1B00 (:900)
            virtual uint32 getLobbyMembers(const GameLinkRemoteStation** pRemoteStations, uint32 capacity) const; // vtbl+0x50 0x00CD1B50 (:911)
            virtual bool setLobbyFilters(const uint32* pIndices, const uint32* pValues, uint32 numLobbyFilters); // vtbl+0x54 0x00CD47B0 (:942)
            virtual uint32 getLobbyFilters(uint32* pLobbyFilters, uint32 capacity) const; // vtbl+0x58 0x00CD1BB0 (:983)
            virtual bool sendP2PPacketUnreliable(const GameLinkRemoteStation* pStation, const void* pPacket, uint32 packetSize); // vtbl+0x5C 0x00CD4890 (:1003)
            virtual uint32 getNextP2PPacket(void* pBuffer, uint32 bufferSize, const GameLinkRemoteStation** pReturnSender); // vtbl+0x60 0x00CD5DE0 (:1053)
            virtual bool setLobbyType(GameLinkLobbyType lobbyType); // vtbl+0x64 0x00CD4BF0 (:1377)
            virtual bool setLobbyLocked(bool locked); // vtbl+0x68 0x00CD4C90 (:1412)
            virtual bool setLobbyOwner(unit4::RemoteUserId newOwnerId); // vtbl+0x6C 0x00CD85B0 (:1442)
            virtual uint32 getLobbyMaxMembers() const; // vtbl+0x70 0x00CD05D0 (:1478)
            virtual uint32 getNumLobbyMembers() const; // vtbl+0x74 0x00CD1BA0 (:932)
            virtual GameLinkLobby getLobbyId() const; // vtbl+0x78 0x00CD0590 (:1364)
            virtual const GameLinkRemoteStation* getLobbyOwner() const; // vtbl+0x7C 0x00CD4950 (:1207)
            virtual bool isLobbyOwner() const; // vtbl+0x80 0x00CD4980 (:1221)
            virtual const GameLinkRemoteStation* getRemoteStationById(unit4::RemoteUserId id) const; // vtbl+0x84 0x00CD1C00 (:1239)
            virtual void setVoiceChatEnabled(bool isEnabled); // vtbl+0x88 0x00CD4A00 (:1258)
            virtual void setVoiceChatLocalPlayerMuted(bool isMuted); // vtbl+0x8C 0x00CD4B10 (:1282)
            virtual void setVoiceChatRemotePlayerMuted(const GameLinkRemoteStation& remoteStation, bool isMuted); // vtbl+0x90 0x00CD1D00 (:1303)
            virtual bool getVoiceChatLocalPlayerInfo(GameLinkVoiceChatPlayerInfo* pVoiceChatInfo); // vtbl+0x94 0x00CD0510 (:1321)
            virtual bool getVoiceChatRemotePlayerInfo(GameLinkVoiceChatPlayerInfo* pVoiceChatInfo, const GameLinkRemoteStation& remoteStation); // vtbl+0x98 0x00CD1DC0 (:1336)
            virtual bool hasVoiceChatPermissions() const; // vtbl+0x9C 0x00CD0580 (:1359)
            virtual void setVoiceChatVolume(float volume); // vtbl+0xA0 0x00CD1ED0 (:1488)
            CVoiceContext* createVoiceContext(); // 0x00CD30B0 (:2841)
            CVoiceContext* getVoiceContext(CSteamID steamID); // 0x00CD3290 (:2907)
            void updateSourceVoiceArray(keen::SizedArray<GameLinkRemoteStationSteam>& membersList, uint8 numLobbyMembers); // 0x00CD5460 (:2921)
            virtual void handleAboutToDestroyMasteringVoice(keen::SoundSystem::System* pSoundSystem); // vtbl+0x00 0x00CD1F70 (:1501)
            virtual void handleMasteringVoiceCreated(keen::SoundSystem::System* pSoundSystem); // vtbl+0x04 0x00CD4D10 (:1525)
        private:
            void updateLobbyMembersShared(CSteamID lobbyID, CSteamID& ownerID, keen::SizedArray<GameLinkRemoteStationSteam>& membersList); // 0x00CD61A0 (:1535)
            void updateLobbyMembers(); // 0x00CD6530 (:1608)
            void updatePartyMembers(); // 0x00CD6560 (:1613)
            const CSteamID getMySteamId() const; // 0x00CD2020 (:1618)
            bool isLoggedOn() const; // 0x00CD2050 (:1623)
            CSteamID convertToSteamId(const GameLinkRemoteStation& station) const; // 0x00CD2090 (:1633)
            CSteamID convertToSteamId(const GameLinkLobby& lobby) const; // 0x00CD2070 (:1628)
            ELobbyType convertToSteamLobbyType(GameLinkLobbyType lobbyType); // 0x00CD05E0 (:1638)
            const GameLinkRemoteStation* findRemoteStationInLobby(const CSteamID& steamId) const; // 0x00CD2100 (:1675)
            GameLinkRemoteStation* findRemoteStationInLobby(const CSteamID& steamId); // 0x00CD20B0 (:1662)
            const GameLinkRemoteStation* findRemoteStationInParty(const CSteamID& steamId) const; // 0x00CD21A0 (:1701)
            GameLinkRemoteStation* findRemoteStationInParty(const CSteamID& steamId); // 0x00CD2150 (:1688)
            bool getFilterFromLobbyData(uint32 index, uint32& value); // 0x00CD21F0 (:1714)
            void resetAllVariables(); // 0x00CD2290 (:1733)
            void resetLobbyVariables(); // 0x00CD4EE0 (:1779)
            void requestCreateLobby(const CreateLobbyRequest& request, CCallResult<GameLinkInternet,LobbyCreated_t>& callResult, void (GameLinkInternet::*pCallback)(LobbyCreated_t*, bool)); // 0x00CD24B0 (:1807)
            void requestLeaveLobby(CSteamID lobbyId); // 0x00CD2570 (:1818)
            void requestJoinLobby(const JoinLobbyRequest& request, CCallResult<GameLinkInternet,LobbyEnter_t>& callResult, void (GameLinkInternet::*pCallback)(LobbyEnter_t*, bool)); // 0x00CD25B0 (:1827)
            bool requestSetLobbyFilters(const SetLobbyFiltersRequest& request); // 0x00CD2630 (:1835)
            bool requestSetLobbyType(const SetLobbyTypeRequest& request); // 0x00CD2720 (:1865)
            bool requestSetLobbyLocked(const SetLobbyLockedRequest& request); // 0x00CD2770 (:1883)
            void updateLobbyInformationInParty(); // 0x00CD27C0 (:1901)
            void updateLobbyInformationFromParty(); // 0x00CD2850 (:1916)
            bool isProcessingAnyRequest(); // 0x00CD0630 (:1932)
            void createPartyLobby(); // 0x00CD8650 (:1945)
            void joinPartyLobby(CSteamID lobbyId); // 0x00CD6590 (:1965)
            void leavePartyLobby(); // 0x00CD28D0 (:1987)
            void leaveLobbyInternal(); // 0x00CD4FE0 (:1997)
            bool joinLobbyInternal(const GameLinkLobby& lobby); // 0x00CD6690 (:2024)
            bool createAuthTicket(GameLinkSteamVACLocalAuthTicket* pOutTicket); // 0x00CD2980 (:2096)
            void destroyAuthTicket(GameLinkSteamVACLocalAuthTicket* pTicket); // 0x00CD29D0 (:2112)
            void createAuthTicketRequestPacket(const GameLinkSteamVACLocalAuthTicket& ticket, uint8* pOutPacketBuffer, uint32 packetBufferSize, uint32* pOutPacketSize); // 0x00CD0660 (:2121)
            void handleAuthTicketResponse(const void* pPacket, uint32, CSteamID senderId); // 0x00CD2A00 (:2138)
            void handleReceivedAuthTicket(const void* pPacket, uint32, CSteamID senderId); // 0x00CD5090 (:2161)
            EBeginAuthSessionResult beginAuthSession(const GameLinkSteamVACRemoteAuthTicket& ticket); // 0x00CD2AB0 (:2212)
            void endAuthSession(CSteamID id); // 0x00CD2AF0 (:2220)
            void createAuthTicketResponsePacket(EBeginAuthSessionResult beginAuthSessionResult, EAuthSessionResponse authSessionResponse, uint8* pOutPacketBuffer, uint32 packetBufferSize, uint32* pOutPacketSize); // 0x00CD06F0 (:2228)
            void refuseConnection(CSteamID id); // 0x00CD2B20 (:2244)
            GameLinkSteamVACRemoteAuthTicket* getFreeRemoteAuthTicket(); // 0x00CD0720 (:2262)
            GameLinkSteamVACRemoteAuthTicket* findRemoteAuthTicket(CSteamID id); // 0x00CD0750 (:2276)
            void updateAuth(); // 0x00CD2B70 (:2290)
            void sendPacketToAllOthersInternal(uint8* pPacket, uint32 size) const; // 0x00CD2D20 (:2370)
            void sendP2PPacketToSteamID(CSteamID steamID, const void* pPacket, uint32 packetSize) const; // 0x00CD2DA0 (:2384)
            bool doesItMeanVACBanned(EBeginAuthSessionResult beginAuthSessionResult, EAuthSessionResponse authSessionResponse) const; // 0x00CD07A0 (:2392)
            void handleLobbyListRequest(LobbyMatchList_t* pLobbyMatchList, bool bIOFailure); // 0x00CD2DE0 (:2407)
            void handleLobbyCreation(LobbyCreated_t* pCallback, bool bIOFailure); // 0x00CD6A20 (:2567)
            void handlePartyCreation(LobbyCreated_t* pCallback, bool bIOFailure); // 0x00CD6A50 (:2572)
            void handleLobbyCreationShared(LobbyCreated_t* pCallback, bool bIOFailure, bool forPartyLobby, CSteamID& lobbyId, CreateLobbyRequest& lobbyRequest); // 0x00CD6790 (:2449)
            void handleLobbyEnteredCallResult(LobbyEnter_t* pCallback, bool bIOFailure); // 0x00CD5400 (:2664)
            void handlePartyEnteredCallResult(LobbyEnter_t* pCallback, bool bIOFailure); // 0x00CD5430 (:2669)
            void handleLobbyEnteredCallResultShared(LobbyEnter_t* pCallback, bool bIOFailure, bool forPartyLobby, CSteamID& lobbyId, JoinLobbyRequest& lobbyRequest); // 0x00CD5220 (:2577)
            void handleLobbyEnteredCallback(LobbyEnter_t* pCallback); // 0x00CD6A80 (:2675)
            void handleLobbyDataUpdate(LobbyDataUpdate_t* pCallback); // 0x00CD6B70 (:2702)
            void handleLobbyChatUpdate(LobbyChatUpdate_t* pCallback); // 0x00CD6C70 (:2746)
            void handleP2PSessionRequest(P2PSessionRequest_t* pCallback); // 0x00CD2F40 (:2780)
            void handleGetAuthSessionTicketResponse(GetAuthSessionTicketResponse_t* pCallback); // 0x00CD07D0 (:2803)
            void handleValidateAuthTicketResponse(ValidateAuthTicketResponse_t* pCallback); // 0x00CD3000 (:2809)

            CCallResult<GameLinkInternet,LobbyMatchList_t> m_callResultLobbyListRequested; // 0x20
            CCallResult<GameLinkInternet,LobbyCreated_t> m_callResultLobbyCreated; // 0x48
            CCallResult<GameLinkInternet,LobbyCreated_t> m_callResultPartyCreated; // 0x70
            CCallResult<GameLinkInternet,LobbyEnter_t> m_callResultLobbyEntered; // 0x98
            CCallResult<GameLinkInternet,LobbyEnter_t> m_callResultPartyEntered; // 0xC0
            CCallback<GameLinkInternet,LobbyEnter_t,0> m_callbackLobbyEntered; // 0xE8
            CCallback<GameLinkInternet,LobbyDataUpdate_t,0> m_callbackLobbyDataUpdate; // 0x100
            CCallback<GameLinkInternet,LobbyChatUpdate_t,0> m_callbackLobbyChatUpdate; // 0x118
            CCallback<GameLinkInternet,P2PSessionRequest_t,0> m_callbackP2PSessionRequested; // 0x130
            CCallback<GameLinkInternet,GetAuthSessionTicketResponse_t,0> m_callbackGetAuthSessionTicketResponse; // 0x148
            CCallback<GameLinkInternet,ValidateAuthTicketResponse_t,0> m_callbackValidateAuthTicketResponse; // 0x160
            GameLinkInternetCreationParameters m_internetCreationParams; // 0x178
            float m_currentRunTime; // 0x17C
            bool m_partyEnabled; // 0x180
            CSteamID m_lobbyId; // 0x181
            CSteamID m_partyId; // 0x189
            CSteamID m_lobbyOwnerSteamId; // 0x191
            CSteamID m_partyOwnerSteamId; // 0x199
            uint32 m_currentMaxNumLobbyMembers; // 0x1A4
            keen::SizedArray<GameLinkRemoteStationSteam> m_lobbyMembers; // 0x1A8
            keen::SizedArray<GameLinkRemoteStationSteam> m_partyMembers; // 0x1B4
            keen::Array<uint32> m_lobbyFilters; // 0x1C0
            bool m_voiceChatEnabled; // 0x1C8
            GameLinkVoiceChatPlayerInfo m_ownVoiceChatInfo; // 0x1C9
            float m_ownLastVoiceDataTimeStamp; // 0x1CC
            GameLinkSteamPartyLobbyData m_currentPartyLobbyData; // 0x1D0
            keen::SizedArray<GameLinkSearchResult> m_searchResults; // 0x1E0
            bool m_searchRunning; // 0x1EC
            CreateLobbyRequest m_createLobbyRequest; // 0x1F0
            CreateLobbyRequest m_createPartyRequest; // 0x208
            JoinLobbyRequest m_joinLobbyRequest; // 0x220
            JoinLobbyRequest m_joinPartyRequest; // 0x238
            SetLobbyFiltersRequest m_setLobbyFiltersRequest; // 0x250
            SetLobbyTypeRequest m_setLobbyTypeRequest; // 0x288
            SetLobbyLockedRequest m_setLobbyLockedRequest; // 0x29C
            GameLinkFindAndJoinData m_findAndJoinData; // 0x2B0
            keen::RandomNumberGenerator m_random; // 0x2C0
            keen::MemoryAllocator* m_pAllocator; // 0xC84
            keen::SoundSystem::System* m_pSoundSystem; // 0xC88
            IXAudio2* m_pIXaudio2; // 0xC8C
            keen::SizedArray<GameLinkSteamVoiceChatData> m_voiceChatDataArray; // 0xC90
            unsigned long m_voiceOutPutSampleRate; // 0xC9C
            GameLinkSteamVACLocalAuthTicket m_localAuthTicket; // 0xCA0
            GameLinkSteamVACRemoteAuthTicket m_remoteAuthTickets[64]; // 0x10AC
            uint32 m_lastTimeAuthTicketRequestSent; // 0x114AC
        };

    }
}
