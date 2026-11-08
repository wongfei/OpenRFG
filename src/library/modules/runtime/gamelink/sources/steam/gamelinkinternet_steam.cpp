// Original: library/modules/runtime/gamelink/sources/steam/gamelinkinternet_steam.cpp
//
// GameLinkInternet on Steam (stub_impl.md S2):
// - Lobbies: a game lobby (m_lobbyId) and an optional party lobby (m_partyId, a friends-only lobby whose members follow
//   the party owner into his game lobby through the "lobby_id" lobby data). Every lobby request (create, join, set
//   filters/type/locked) is retried every 500 ms, 20 times at most; only one create/join request runs at a time.
// - The lobby filters are lobby data "f<index>" = "<value>"; a search asks for lobbies whose non-zero filters match and
//   that have room for the whole party.
// - P2P packets (ISteamNetworking channel 0) carry a GameLinkSteamPacketHeader: the game's packets (UserDefined), Steam
//   voice data and the VAC auth tickets the members exchange (each member begins an auth session with every other
//   member's ticket; a VAC banned member is refused, a banned local user leaves).
// - Voice chat: the local voice is recorded by Steam and sent to every other member; received voice is decompressed and
//   queued on one XAudio2 source voice per member (CVoiceContext, which frees each buffer when it has played).
//
// No-Steam deviation (stub_impl.md S2, OpenRFG with --no-steam): sw_api.dll has no interfaces before SteamAPI_Init,
// so create() skips the voice sample rate query and beginSession() fails; the link then stays offline and makes no
// other Steam call (the constructor and destructor only register and unregister callbacks, which needs no init).

#include "library/modules/runtime/gamelink/sources/steam/gamelinkinternet_steam.hpp"
#include "keen/base/string.hpp"
#include "keen/base/stringscanner.hpp"
#include "keen/base/systemtimer.hpp"
#include "keen/base/trace.hpp"
#include "library/modules/runtime/systemservices/sources/steam/onlinehelpers_steam.hpp"
#include "library/modules/thirdparty/emgiec/emgiec_steam_142/sdk/public/steam/steam_api.h"
#include "library/modules/thirdparty/emgiec/emgiec_steam_142/sdk/public/steam/steam_api_internal.h"

#include <new>
#include <string.h>

// 0x00CD1720 (:21)
CVoiceContext::CVoiceContext()
{
    m_hBufferEndEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
    m_pSourceVoice = nullptr;
}

// 0x00CD1750 (:25)
CVoiceContext::~CVoiceContext()
{
    CloseHandle(m_hBufferEndEvent);
}

// 0x00CD0410 (:30)
void CVoiceContext::create(keen::MemoryAllocator* pAllocator)
{
    m_pAllocator = pAllocator;
}

// 0x00CD1770 (:35)
void __stdcall CVoiceContext::OnVoiceProcessingPassStart(UINT32)
{
}

// 0x00CD1780 (:38)
void __stdcall CVoiceContext::OnVoiceProcessingPassEnd()
{
}

// 0x00CD1790 (:41)
void __stdcall CVoiceContext::OnStreamEnd()
{
}

// 0x00CD17A0 (:44)
void __stdcall CVoiceContext::OnBufferStart(void*)
{
}

// 0x00CD17B0 (:47): the buffer context is the decompressed voice data (getNextP2PPacket)
void __stdcall CVoiceContext::OnBufferEnd(void* pBuffer)
{
    m_pAllocator->free(pBuffer);
    SetEvent(m_hBufferEndEvent);
}

// 0x00CD17E0 (:52)
void __stdcall CVoiceContext::OnLoopEnd(void*)
{
}

// 0x00CD17F0 (:55)
void __stdcall CVoiceContext::OnVoiceError(void*, HRESULT)
{
}

namespace kaiko
{
    // 0x00CD0460
    GameLinkSteamPartyLobbyData::GameLinkSteamPartyLobbyData()
    {
    }

    // 0x00CD04B0
    GameLinkSteamVACRemoteAuthTicket::GameLinkSteamVACRemoteAuthTicket()
    {
    }

    // 0x00CD0490
    JoinLobbyRequest::JoinLobbyRequest()
    {
    }

    // 0x00CD03E0 (gamelinkinternet_steam.hpp:135)
    GameLinkSteamVoiceChatData::GameLinkSteamVoiceChatData(CSteamID a_steamID, CVoiceContext* a_voiceContext)
    {
        steamID = a_steamID;
        hasLeft = false;
        pVoiceContext = a_voiceContext;
    }

    namespace gamelink
    {
        // 0x03A256D0, 0x03A256D4
        static bool s_hasInvitationData;
        static GameLinkInvitationData s_invitationData;

        // A remote station's id is its Steam id (RemoteUserId::providerData), a lobby's id the lobby's Steam id.
        static CSteamID getSteamId(const GameLinkRemoteStation& station)
        {
            uint64_t steamId;
            memcpy(&steamId, station.id.providerData, sizeof(steamId));
            return CSteamID(steamId);
        }

        // 0x00CD1830 (:77)
        void setInvitationData(const GameLinkInvitationData* pInvitationData)
        {
            if (pInvitationData != nullptr)
            {
                s_hasInvitationData = true;
                s_invitationData.lobbyId = pInvitationData->lobbyId;
                s_invitationData.friendId = pInvitationData->friendId;
                return;
            }
            s_hasInvitationData = false;
            s_invitationData.lobbyId.Clear();
            s_invitationData.friendId.Clear();
        }

        // 0x00CD0430 (:91)
        const GameLinkInvitationData* getInvitationData()
        {
            return s_hasInvitationData ? &s_invitationData : nullptr;
        }

        // 0x00CD77F0 (:107): the call results get their Steam callback ids with their first Set
        GameLinkInternet::GameLinkInternet()
            : m_callbackLobbyEntered(this, &GameLinkInternet::handleLobbyEnteredCallback)
            , m_callbackLobbyDataUpdate(this, &GameLinkInternet::handleLobbyDataUpdate)
            , m_callbackLobbyChatUpdate(this, &GameLinkInternet::handleLobbyChatUpdate)
            , m_callbackP2PSessionRequested(this, &GameLinkInternet::handleP2PSessionRequest)
            , m_callbackGetAuthSessionTicketResponse(this, &GameLinkInternet::handleGetAuthSessionTicketResponse)
            , m_callbackValidateAuthTicketResponse(this, &GameLinkInternet::handleValidateAuthTicketResponse)
        {
            m_internetCreationParams.vacEnabled = true;
        }

        // 0x00CD4070 (:111): the callbacks and call results unregister themselves
        GameLinkInternet::~GameLinkInternet()
        {
        }

        // 0x00CD4200 (:115)
        void GameLinkInternet::create(keen::MemoryAllocator* pAllocator, const GameLinkInternetCreationParameters& params, const GameLinkGeneralCreationParameters& paramsGeneral)
        {
            m_internetCreationParams.vacEnabled = params.vacEnabled;
            m_lobbyMembers.create(pAllocator, paramsGeneral.maxNumLobbyMembers, 0x10);
            m_partyMembers.create(pAllocator, paramsGeneral.maxNumPartyMembers, 0x10);
            // 8 filters (SetLobbyFiltersRequest::lobbyFilters)
            m_lobbyFilters.create(pAllocator, 8, 0x10);
            m_searchResults.create(pAllocator, paramsGeneral.maxNumMatchmakingResults, 0x10);
            m_random.initFromSeed(keen::SystemTimer::getCurrentMilliseconds());
            m_pAllocator = pAllocator;
            m_pSoundSystem = paramsGeneral.pSoundSystem;
            if (m_pSoundSystem == nullptr)
            {
                m_pIXaudio2 = nullptr;
            }
            else
            {
                m_pIXaudio2 = keen::SoundSystem::getXAudio2(m_pSoundSystem);
                m_voiceChatDataArray.create(pAllocator, paramsGeneral.maxNumLobbyMembers, 0x10);
                ISteamUser* pSteamUser = SteamUser();
                // deviation: no Steam (no voice sample rate; the voice contexts are never created)
                m_voiceOutPutSampleRate = pSteamUser != nullptr ? pSteamUser->GetVoiceOptimalSampleRate() : 0;
            }
            resetAllVariables();
            m_findAndJoinData.timeout = paramsGeneral.findAndJoinTimeout;
            memset(m_remoteAuthTickets, 0, sizeof(m_remoteAuthTickets));
        }

        // 0x00CD43B0 (:147)
        void GameLinkInternet::destroy(keen::MemoryAllocator*)
        {
            m_lobbyMembers.destroy(m_pAllocator);
            m_partyMembers.destroy(m_pAllocator);
            m_lobbyFilters.destroy(m_pAllocator);
            m_searchResults.destroy(m_pAllocator);
            if (m_pSoundSystem != nullptr)
            {
                m_voiceChatDataArray.destroy(m_pAllocator);
            }
        }

        // 0x00CD1940 (:162)
        bool GameLinkInternet::beginSession(const GameLinkSessionParameters&)
        {
            if (m_state != GameLinkState_Offline)
            {
                return false;
            }
            // deviation: no Steam (the link stays offline)
            if (SteamUser() == nullptr)
            {
                return false;
            }
            if (m_pSoundSystem != nullptr)
            {
                keen::SoundSystem::registerSoundDeviceChangedCallback(m_pSoundSystem, this);
            }
            m_ownVoiceChatInfo.hasHeadset = false;
            m_ownVoiceChatInfo.isMuted = false;
            m_ownVoiceChatInfo.hasJustSpoken = false;
            m_voiceChatEnabled = false;
            // long ago: the local player has not spoken (hasJustSpoken is "within the last second")
            m_ownLastVoiceDataTimeStamp = -100.0f;
            m_currentRunTime = 0.0f;
            m_state = GameLinkState_Online;
            return true;
        }

        // 0x00CD5D50 (:185)
        void GameLinkInternet::endSession()
        {
            if (m_state == GameLinkState_Offline)
            {
                return;
            }
            if (m_state == GameLinkState_InLobby)
            {
                leaveLobbyInternal();
            }
            if (m_pSoundSystem != nullptr)
            {
                keen::SoundSystem::unregisterSoundDeviceChangedCallback(m_pSoundSystem, this);
                setVoiceChatEnabled(false);
            }
            setPartyEnabled(false);
            resetAllVariables();
            m_state = GameLinkState_Offline;
        }

        // 0x00CD44B0 (:212): searches up to 10 lobbies; update() joins a random result or creates a public lobby
        bool GameLinkInternet::findAndJoinLobby(uint32 maxNumLobbyMembers, const uint32* pLobbyFilters, uint32 numLobbyFilters)
        {
            if (m_state != GameLinkState_Online || m_findAndJoinData.status == GameLinkFindAndJoinStatus_Running || (m_partyEnabled && !isPartyOwner()))
            {
                return false;
            }
            m_findAndJoinData.maxNumLobbyMembers = maxNumLobbyMembers;
            memset(m_lobbyFilters.getStart(), 0, m_lobbyFilters.getSize() * sizeof(uint32));
            for (uint32 i = 0; i < numLobbyFilters; ++i)
            {
                m_lobbyFilters[i] = pLobbyFilters[i];
            }
            if (startSearchLobby(GameLinkConstants_FindAndJoinMaxSearchResults, pLobbyFilters, numLobbyFilters))
            {
                m_findAndJoinData.timeStarted = keen::SystemTimer::getCurrentMilliseconds();
                m_findAndJoinData.status = GameLinkFindAndJoinStatus_Running;
                m_state = GameLinkState_InLobby;
                return true;
            }
            m_findAndJoinData.status = GameLinkFindAndJoinStatus_Failed;
            return false;
        }

        // 0x00CD04D0 (:254)
        GameLinkFindAndJoinStatus GameLinkInternet::getFindAndJoinStatus() const
        {
            return m_findAndJoinData.status;
        }

        // 0x00CD04E0 (:259)
        void GameLinkInternet::cancelFindAndJoinLobby()
        {
            if (m_findAndJoinData.status == GameLinkFindAndJoinStatus_Running)
            {
                m_findAndJoinData.status = GameLinkFindAndJoinStatus_None;
                m_state = GameLinkState_Online;
            }
        }

        // 0x00CD8D50 (:270)
        void GameLinkInternet::setPartyEnabled(bool enabled)
        {
            if (!m_partyEnabled)
            {
                if (enabled)
                {
                    createPartyLobby();
                }
            }
            else if (!enabled)
            {
                leavePartyLobby();
            }
        }

        // 0x00CD0500 (:286)
        bool GameLinkInternet::isPartyEnabled() const
        {
            return m_partyEnabled;
        }

        // 0x00CD4570 (:291): the creator of a party is its owner while the party lobby is being created
        bool GameLinkInternet::isPartyOwner() const
        {
            if (m_state == GameLinkState_Offline || !m_partyEnabled)
            {
                return false;
            }
            if (m_createPartyRequest.id != 0)
            {
                return true;
            }
            if (!m_partyOwnerSteamId.IsValid())
            {
                return false;
            }
            return getMySteamId() == m_partyOwnerSteamId;
        }

        // 0x00CD45F0 (:309)
        bool GameLinkInternet::getPartyOwner(unit4::NamedRemoteUserId* pOutHost) const
        {
            if (m_state == GameLinkState_Offline || !m_partyEnabled)
            {
                return false;
            }
            if (!m_partyOwnerSteamId.IsValid())
            {
                return false;
            }
            const GameLinkRemoteStation* pOwner = findRemoteStationInParty(m_partyOwnerSteamId);
            if (pOwner == nullptr)
            {
                return false;
            }
            pOutHost->id = pOwner->id;
            keen::copyString(pOutHost->displayName, sizeof(pOutHost->displayName), pOwner->userName);
            return true;
        }

        // 0x00CD19B0 (:332)
        uint32 GameLinkInternet::getPartyMembers(unit4::NamedRemoteUserId* pPartyMembers, uint32 capacity) const
        {
            if (m_state == GameLinkState_Offline || !m_partyEnabled)
            {
                return 0;
            }
            uint32 count = m_partyMembers.getSize();
            if (capacity < count)
            {
                count = capacity;
            }
            for (uint32 i = 0; i < count; ++i)
            {
                const GameLinkRemoteStationSteam& member = m_partyMembers.getStart()[i];
                pPartyMembers[i].id = member.id;
                keen::copyString(pPartyMembers[i].displayName, sizeof(pPartyMembers[i].displayName), member.userName);
            }
            return count;
        }

        // 0x00CD1A40 (:350)
        uint32 GameLinkInternet::getNumPartyMembers() const
        {
            if (m_state == GameLinkState_Offline || !m_partyEnabled)
            {
                return 0;
            }
            return m_partyMembers.getSize();
        }

        // 0x00CD7C30 (:361)
        void GameLinkInternet::update(float timeStep)
        {
            const uint32 now = keen::SystemTimer::getCurrentMilliseconds();
            updateAuth();
            if (m_state != GameLinkState_Offline)
            {
                if (!SteamUser()->BLoggedOn())
                {
                    setError(GameLinkError_NoInternetAccess, GameLinkSteamAddtionalPlatformErrorCodes_NoInternetAccess_update);
                    endSession();
                }
                if (m_state != GameLinkState_Offline)
                {
                    // A party member follows the party owner into (and out of) his game lobby.
                    if (m_partyEnabled && !isPartyOwner())
                    {
                        if (m_currentPartyLobbyData.lobbyId.IsValid() && m_currentPartyLobbyData.lobbyId != m_currentPartyLobbyData.lastLobbyId)
                        {
                            GameLinkLobby lobby;
                            lobby.id.asUint64 = m_currentPartyLobbyData.lobbyId.ConvertToUint64();
                            joinLobbyInternal(lobby);
                            m_currentPartyLobbyData.lastLobbyId = m_currentPartyLobbyData.lobbyId;
                        }
                        else if (!m_currentPartyLobbyData.lobbyId.IsValid() && m_lobbyId.IsValid())
                        {
                            leaveLobbyInternal();
                            m_currentPartyLobbyData.lastLobbyId.Clear();
                        }
                    }
                    if (m_createPartyRequest.id != 0 && !isProcessingAnyRequest())
                    {
                        if (m_createPartyRequest.retryCount == 0)
                        {
                            if (setError(GameLinkError_CreateLobbyFailed_Timeout, GameLinkSteamAddtionalPlatformErrorCodes_CreateLobbyFailed_Timeout_update_party))
                            {
                                // original: returns without advancing the run time
                                endSession();
                                return;
                            }
                        }
                        // retried every 500 ms
                        else if (now - m_createPartyRequest.timeRequested > 500)
                        {
                            requestCreateLobby(m_createPartyRequest, m_callResultPartyCreated, &GameLinkInternet::handlePartyCreation);
                            m_createPartyRequest.retryCount--;
                            m_createPartyRequest.idProcessing = m_createPartyRequest.id;
                            m_createPartyRequest.timeRequested = now;
                        }
                    }
                    if (m_joinPartyRequest.id != 0 && !isProcessingAnyRequest())
                    {
                        if (m_joinPartyRequest.retryCount == 0)
                        {
                            if (setError(GameLinkError_JoinLobbyFailed_Timeout, GameLinkSteamAddtionalPlatformErrorCodes_JoinLobbyFailed_Timeout_update_party))
                            {
                                endSession();
                                return;
                            }
                        }
                        else if (now - m_joinPartyRequest.timeRequested > 500)
                        {
                            requestJoinLobby(m_joinPartyRequest, m_callResultPartyEntered, &GameLinkInternet::handlePartyEnteredCallResult);
                            m_joinPartyRequest.retryCount--;
                            m_joinPartyRequest.idProcessing = m_joinPartyRequest.id;
                            m_joinPartyRequest.timeRequested = now;
                        }
                    }
                }
            }
            if (m_state == GameLinkState_InLobby)
            {
                if (m_findAndJoinData.status == GameLinkFindAndJoinStatus_Running)
                {
                    if (m_lobbyId.IsValid())
                    {
                        m_findAndJoinData.status = GameLinkFindAndJoinStatus_FoundAndJoinedLobby;
                    }
                    else if (m_joinLobbyRequest.id == 0 && m_createLobbyRequest.id == 0)
                    {
                        GameLinkSearchResult searchResults[GameLinkConstants_FindAndJoinMaxSearchResults];
                        bool searchRunning;
                        // Original bug kept: the capacity passed is the size of the array in bytes (800), not 10.
                        const uint32 numResults = getSearchResults(searchResults, sizeof(searchResults), &searchRunning);
                        if (!searchRunning)
                        {
                            bool started;
                            if (numResults == 0)
                            {
                                started = createLobby(GameLinkLobbyType_Public, m_findAndJoinData.maxNumLobbyMembers, m_lobbyFilters.getStart(), m_lobbyFilters.getSize());
                            }
                            else
                            {
                                started = joinLobby(searchResults[m_random.getRandomValue(numResults)].lobby);
                            }
                            if (!started)
                            {
                                m_findAndJoinData.status = GameLinkFindAndJoinStatus_Failed;
                            }
                        }
                    }
                    if (now - m_findAndJoinData.timeStarted > m_findAndJoinData.timeout)
                    {
                        m_findAndJoinData.status = GameLinkFindAndJoinStatus_Failed;
                    }
                    // original: returns without advancing the run time while finding a lobby
                    return;
                }
                if (m_createLobbyRequest.id != 0 && !isProcessingAnyRequest())
                {
                    if (m_createLobbyRequest.retryCount == 0)
                    {
                        if (setError(GameLinkError_CreateLobbyFailed_Timeout, GameLinkSteamAddtionalPlatformErrorCodes_CreateLobbyFailed_Timeout_update_lobby))
                        {
                            m_state = GameLinkState_Online;
                        }
                    }
                    else if (now - m_createLobbyRequest.timeRequested > 500)
                    {
                        requestCreateLobby(m_createLobbyRequest, m_callResultLobbyCreated, &GameLinkInternet::handleLobbyCreation);
                        m_createLobbyRequest.retryCount--;
                        m_createLobbyRequest.idProcessing = m_createLobbyRequest.id;
                        m_createLobbyRequest.timeRequested = now;
                    }
                }
                if (m_joinLobbyRequest.id != 0 && !isProcessingAnyRequest())
                {
                    if (m_joinLobbyRequest.retryCount == 0)
                    {
                        if (setError(GameLinkError_JoinLobbyFailed_Timeout, GameLinkSteamAddtionalPlatformErrorCodes_JoinLobbyFailed_Timeout_update_lobby))
                        {
                            m_state = GameLinkState_Online;
                        }
                        if (m_findAndJoinData.status == GameLinkFindAndJoinStatus_Running)
                        {
                            m_findAndJoinData.status = GameLinkFindAndJoinStatus_Failed;
                        }
                        if (m_partyEnabled && !isPartyOwner())
                        {
                            endSession();
                        }
                    }
                    else if (now - m_joinLobbyRequest.timeRequested > 500)
                    {
                        requestJoinLobby(m_joinLobbyRequest, m_callResultLobbyEntered, &GameLinkInternet::handleLobbyEnteredCallResult);
                        m_joinLobbyRequest.retryCount--;
                        m_joinLobbyRequest.idProcessing = m_joinLobbyRequest.id;
                        m_joinLobbyRequest.timeRequested = now;
                    }
                }
                if (m_setLobbyFiltersRequest.id != 0)
                {
                    if (m_setLobbyFiltersRequest.retryCount == 0)
                    {
                        m_setLobbyFiltersRequest.id = 0;
                    }
                    else if (now - m_setLobbyFiltersRequest.timeRequested > 500)
                    {
                        if (requestSetLobbyFilters(m_setLobbyFiltersRequest))
                        {
                            m_setLobbyFiltersRequest.id = 0;
                        }
                        else
                        {
                            m_setLobbyFiltersRequest.retryCount--;
                            m_setLobbyFiltersRequest.timeRequested = now;
                        }
                    }
                }
                if (m_setLobbyTypeRequest.id != 0)
                {
                    if (m_setLobbyTypeRequest.retryCount == 0)
                    {
                        m_setLobbyTypeRequest.id = 0;
                    }
                    else if (now - m_setLobbyTypeRequest.timeRequested > 500)
                    {
                        if (requestSetLobbyType(m_setLobbyTypeRequest))
                        {
                            m_setLobbyTypeRequest.id = 0;
                        }
                        else
                        {
                            m_setLobbyTypeRequest.retryCount--;
                            m_setLobbyTypeRequest.timeRequested = now;
                        }
                    }
                }
                if (m_setLobbyLockedRequest.id != 0)
                {
                    if (m_setLobbyLockedRequest.retryCount == 0)
                    {
                        m_setLobbyLockedRequest.id = 0;
                    }
                    else if (now - m_setLobbyLockedRequest.timeRequested > 500)
                    {
                        if (requestSetLobbyLocked(m_setLobbyLockedRequest))
                        {
                            m_setLobbyLockedRequest.id = 0;
                        }
                        else
                        {
                            m_setLobbyLockedRequest.retryCount--;
                            m_setLobbyLockedRequest.timeRequested = now;
                        }
                    }
                }
                // The other members of a running match go to the Steam "played with" list, once each.
                if (m_gameplayRunning)
                {
                    for (uint32 i = 0; i < m_lobbyMembers.getSize(); ++i)
                    {
                        GameLinkRemoteStationSteam& member = m_lobbyMembers.getStart()[i];
                        if (!member.playedWithRequested && !member.myStation)
                        {
                            SteamFriends()->SetPlayedWith(getSteamId(member));
                            m_lobbyMembers.getStart()[i].playedWithRequested = true;
                        }
                    }
                }
                if (m_voiceChatEnabled && !m_ownVoiceChatInfo.isMuted)
                {
                    uint32 compressedSize = 0;
                    if (SteamUser()->GetAvailableVoice(&compressedSize, nullptr, 0) == k_EVoiceResultOK && compressedSize != 0)
                    {
                        // a voice packet: the header and up to 1192 bytes of compressed voice
                        uint8 packet[0x4B0];
                        uint32 bytesWritten = 0;
                        SteamUser()->GetVoice(true, packet + sizeof(GameLinkSteamPacketHeader), sizeof(packet) - sizeof(GameLinkSteamPacketHeader), &bytesWritten, false, nullptr, 0, nullptr, 0);
                        GameLinkSteamPacketHeader* pHeader = (GameLinkSteamPacketHeader*)packet;
                        pHeader->packetType = GameLinkSteamPacketType_VoiceData;
                        pHeader->actualSize = bytesWritten;
                        const uint32 packetSize = bytesWritten + sizeof(GameLinkSteamPacketHeader);
                        const CSteamID mySteamId = getMySteamId();
                        for (uint32 i = 0; i < m_lobbyMembers.getSize(); ++i)
                        {
                            const CSteamID memberId = getSteamId(m_lobbyMembers.getStart()[i]);
                            if (memberId != mySteamId)
                            {
                                SteamNetworking()->SendP2PPacket(memberId, packet, packetSize, k_EP2PSendUnreliableNoDelay, 0);
                            }
                        }
                        m_ownVoiceChatInfo.hasHeadset = true;
                        m_ownLastVoiceDataTimeStamp = m_currentRunTime;
                    }
                }
            }
            // x87 double rule: the sum is formed in double
            m_currentRunTime = (float)((double)m_currentRunTime + (double)timeStep);
        }

        // 0x00CD8380 (:707)
        bool GameLinkInternet::createLobby(GameLinkLobbyType lobbyType, uint32 maxNumLobbyMembers, const uint32* pLobbyFilters, uint32 numLobbyFilters)
        {
            if (maxNumLobbyMembers == 0 || maxNumLobbyMembers > m_lobbyMembers.getCapacity() || numLobbyFilters > m_lobbyFilters.getSize() || m_state == GameLinkState_Offline || (m_partyEnabled && !isPartyOwner()))
            {
                return false;
            }
            if (m_state == GameLinkState_InLobby)
            {
                leaveLobbyInternal();
            }
            for (uint32 i = 0; i < numLobbyFilters; ++i)
            {
                m_lobbyFilters[i] = pLobbyFilters[i];
            }
            m_joinLobbyRequest.id = 0;
            m_joinLobbyRequest.lobbyId.Clear();
            m_createLobbyRequest.id = generateRequestId();
            m_createLobbyRequest.timeRequested = keen::SystemTimer::getCurrentMilliseconds();
            // 20 tries, 500 ms apart (update)
            m_createLobbyRequest.retryCount = 20;
            m_createLobbyRequest.lobbyType = lobbyType;
            m_createLobbyRequest.maxNumLobbyMembers = maxNumLobbyMembers;
            if (!isProcessingAnyRequest())
            {
                requestCreateLobby(m_createLobbyRequest, m_callResultLobbyCreated, &GameLinkInternet::handleLobbyCreation);
                m_createLobbyRequest.idProcessing = m_createLobbyRequest.id;
            }
            m_state = GameLinkState_InLobby;
            return true;
        }

        // 0x00CD4660 (:757)
        bool GameLinkInternet::startSearchLobby(uint32 maxNumResults, const uint32* pLobbyFilters, uint32 numLobbyFilters)
        {
            if (m_state == GameLinkState_Offline || m_searchRunning)
            {
                return false;
            }
            m_searchResults.clear();
            m_searchRunning = true;
            SteamMatchmaking()->AddRequestLobbyListResultCountFilter(maxNumResults);
            for (uint32 i = 0; i < numLobbyFilters; ++i)
            {
                if (pLobbyFilters[i] != 0)
                {
                    char key[32];
                    char value[32];
                    keen::formatString(key, sizeof(key), "f%u", i);
                    keen::formatString(value, sizeof(value), "%u", pLobbyFilters[i]);
                    SteamMatchmaking()->AddRequestLobbyListStringFilter(key, value, k_ELobbyComparisonEqual);
                }
            }
            const SteamAPICall_t hCall = SteamMatchmaking()->RequestLobbyList();
            m_callResultLobbyListRequested.Set(hCall, this, &GameLinkInternet::handleLobbyListRequest);
            return true;
        }

        // 0x00CD1A60 (:795)
        uint32 GameLinkInternet::getSearchResults(GameLinkSearchResult* pSearchResults, uint32 capacity, bool* pSearchRunning) const
        {
            if (m_state == GameLinkState_Offline)
            {
                return 0;
            }
            *pSearchRunning = m_searchRunning;
            if (m_searchRunning)
            {
                return 0;
            }
            uint32 count = m_searchResults.getSize();
            if (count < capacity)
            {
                capacity = count;
            }
            if (capacity != 0)
            {
                memmove(pSearchResults, m_searchResults.getStart(), capacity * sizeof(GameLinkSearchResult));
            }
            return capacity;
        }

        // 0x00CD84D0 (:823)
        bool GameLinkInternet::joinLobby(const GameLinkLobby& lobby)
        {
            if (m_partyEnabled && !isPartyOwner())
            {
                return false;
            }
            return joinLobbyInternal(lobby);
        }

        // 0x00CD8500 (:837): an invitation from a friend joins his party, one without a friend the lobby itself
        bool GameLinkInternet::joinLobby(const GameLinkInvitationData* pInvitationData)
        {
            if (pInvitationData == nullptr || !pInvitationData->lobbyId.IsValid())
            {
                return false;
            }
            if (pInvitationData->lobbyId == m_partyId || pInvitationData->lobbyId == m_joinPartyRequest.lobbyId)
            {
                return true;
            }
            if (m_state == GameLinkState_Offline)
            {
                return false;
            }
            if (!pInvitationData->friendId.IsValid())
            {
                setPartyEnabled(false);
                GameLinkLobby lobby;
                lobby.id.asUint64 = pInvitationData->lobbyId.ConvertToUint64();
                return joinLobbyInternal(lobby);
            }
            joinPartyLobby(pInvitationData->lobbyId);
            m_state = GameLinkState_InLobby;
            return true;
        }

        // 0x00CD5DB0 (:886)
        void GameLinkInternet::leaveLobby()
        {
            if (m_partyEnabled && !isPartyOwner())
            {
                return;
            }
            leaveLobbyInternal();
        }

        // 0x00CD1B00 (:900)
        void GameLinkInternet::openInvitationDialog()
        {
            if (m_state != GameLinkState_Offline && m_partyEnabled && m_partyId.IsValid())
            {
                SteamFriends()->ActivateGameOverlayInviteDialog(m_partyId);
            }
        }

        // 0x00CD1B50 (:911)
        uint32 GameLinkInternet::getLobbyMembers(const GameLinkRemoteStation** pRemoteStations, uint32 capacity) const
        {
            if (m_state != GameLinkState_InLobby || m_lobbyMembers.getSize() == 0)
            {
                return 0;
            }
            uint32 count = m_lobbyMembers.getSize();
            if (capacity < count)
            {
                count = capacity;
            }
            for (uint32 i = 0; i < count; ++i)
            {
                pRemoteStations[i] = &m_lobbyMembers.getStart()[i];
            }
            return count;
        }

        // 0x00CD1BA0 (:932)
        uint32 GameLinkInternet::getNumLobbyMembers() const
        {
            if (m_state != GameLinkState_InLobby)
            {
                return 0;
            }
            return m_lobbyMembers.getSize();
        }

        // 0x00CD47B0 (:942): the filters that cannot be set now are retried by update()
        bool GameLinkInternet::setLobbyFilters(const uint32* pIndices, const uint32* pValues, uint32 numLobbyFilters)
        {
            if (pIndices == nullptr || pValues == nullptr || numLobbyFilters == 0 || numLobbyFilters > m_lobbyFilters.getSize() || m_state != GameLinkState_InLobby || !isLobbyOwner())
            {
                return false;
            }
            m_setLobbyFiltersRequest.id = generateRequestId();
            m_setLobbyFiltersRequest.retryCount = 20;
            m_setLobbyFiltersRequest.timeRequested = keen::SystemTimer::getCurrentMilliseconds();
            for (uint32 i = 0; i < numLobbyFilters; ++i)
            {
                m_lobbyFilters[pIndices[i]] = pValues[i];
                m_setLobbyFiltersRequest.lobbyFilters[pIndices[i]] = pValues[i];
                m_setLobbyFiltersRequest.lobbyFiltersToSet[pIndices[i]] = true;
            }
            if (requestSetLobbyFilters(m_setLobbyFiltersRequest))
            {
                m_setLobbyFiltersRequest.id = 0;
            }
            return true;
        }

        // 0x00CD1BB0 (:983)
        uint32 GameLinkInternet::getLobbyFilters(uint32* pLobbyFilters, uint32 capacity) const
        {
            if (m_state == GameLinkState_Offline)
            {
                return 0;
            }
            uint32 count = m_lobbyFilters.getSize();
            if (capacity <= count)
            {
                count = capacity;
            }
            for (uint32 i = 0; i < count; ++i)
            {
                pLobbyFilters[i] = m_lobbyFilters[i];
            }
            return count;
        }

        // 0x00CD4890 (:1003): nothing is sent to a refused (VAC banned) member
        bool GameLinkInternet::sendP2PPacketUnreliable(const GameLinkRemoteStation* pStation, const void* pPacket, uint32 packetSize)
        {
            // the header and up to 1192 bytes of the game's packet
            uint8 packet[0x4B0];
            if (m_state != GameLinkState_InLobby || !m_lobbyId.IsValid() || ((const GameLinkRemoteStationSteam*)pStation)->isBanned || packetSize > sizeof(packet) - sizeof(GameLinkSteamPacketHeader))
            {
                return false;
            }
            GameLinkSteamPacketHeader* pHeader = (GameLinkSteamPacketHeader*)packet;
            pHeader->packetType = GameLinkSteamPacketType_UserDefined;
            pHeader->actualSize = packetSize;
            memcpy(packet + sizeof(GameLinkSteamPacketHeader), pPacket, packetSize);
            return SteamNetworking()->SendP2PPacket(getSteamId(*pStation), packet, packetSize + sizeof(GameLinkSteamPacketHeader), k_EP2PSendUnreliable, 0);
        }

        // 0x00CD5DE0 (:1053): handles the voice and auth packets on the way to the next packet of the game
        uint32 GameLinkInternet::getNextP2PPacket(void* pBuffer, uint32 bufferSize, const GameLinkRemoteStation** pReturnSender)
        {
            if (m_state == GameLinkState_InLobby && m_lobbyId.IsValid())
            {
                uint32 packetSize;
                while (SteamNetworking()->IsP2PPacketAvailable(&packetSize, 0))
                {
                    uint8 packet[0x4B0];
                    CSteamID senderId;
                    if (!SteamNetworking()->ReadP2PPacket(packet, sizeof(packet), &packetSize, &senderId, 0) || packetSize < sizeof(GameLinkSteamPacketHeader))
                    {
                        keen::debug::nativePrintArgs("[gamelink] getNextP2PMessage: failed to read p2p packet\n");
                        break;
                    }
                    const GameLinkSteamPacketHeader* pHeader = (const GameLinkSteamPacketHeader*)packet;
                    const uint8* pData = packet + sizeof(GameLinkSteamPacketHeader);
                    switch (pHeader->packetType)
                    {
                    case GameLinkSteamPacketType_VoiceData:
                        if (m_voiceChatEnabled)
                        {
                            GameLinkRemoteStationSteam* pSender = (GameLinkRemoteStationSteam*)findRemoteStationInLobby(senderId);
                            if (pSender == nullptr || pSender->voiceChatInfo.isMuted)
                            {
                                break;
                            }
                            CVoiceContext* pVoiceContext = getVoiceContext(senderId);
                            if (pVoiceContext == nullptr)
                            {
                                break;
                            }
                            // 22 KB of decompressed voice
                            uint8 voice[0x5800];
                            uint32 voiceSize = 0;
                            EVoiceResult result = SteamUser()->DecompressVoice(pData, pHeader->actualSize, voice, sizeof(voice), &voiceSize, m_voiceOutPutSampleRate);
                            if (voiceSize == 0)
                            {
                                break;
                            }
                            void* pVoiceData = nullptr;
                            if (result == k_EVoiceResultOK)
                            {
                                pVoiceData = m_pAllocator->allocate(voiceSize, 0x10, 0, nullptr);
                                if (pVoiceData == nullptr)
                                {
                                    keen::debug::nativePrintArgs("[gamelink] getNextP2PMessage: failed to allocate %d Memory for Voice Packet \n", voiceSize);
                                    break;
                                }
                            }
                            else if (result == k_EVoiceResultBufferTooSmall)
                            {
                                keen::debug::nativePrintArgs("[gamelink] getNextP2PMessage: buffer to small, try reallocate %d \n", voiceSize);
                                pVoiceData = m_pAllocator->allocate(voiceSize, 0x10, 0, nullptr);
                                if (pVoiceData == nullptr)
                                {
                                    keen::debug::nativePrintArgs("[gamelink] getNextP2PMessage: failed to allocate %d Memory for Voice Packet \n", voiceSize);
                                    break;
                                }
                                // Original bug kept: the size of the new buffer is passed as 4 (the size of the pointer),
                                // so this always fails, and on success the copy below would overwrite the new buffer
                                // with the old one's contents.
                                if (SteamUser()->DecompressVoice(pData, pHeader->actualSize, pVoiceData, 4, &voiceSize, m_voiceOutPutSampleRate) != k_EVoiceResultOK)
                                {
                                    keen::debug::nativePrintArgs("[gamelink] getNextP2PMessage: buffer to small %d continue with next Packet \n", voiceSize);
                                    m_pAllocator->free(pVoiceData);
                                    break;
                                }
                            }
                            else
                            {
                                keen::debug::nativePrintArgs("[gamelink] getNextP2PMessage: decoding voice data failed\n");
                                break;
                            }
                            memcpy(pVoiceData, voice, voiceSize);
                            // the buffer is freed by CVoiceContext::OnBufferEnd (its context)
                            XAUDIO2_BUFFER buffer = {};
                            buffer.Flags = XAUDIO2_END_OF_STREAM;
                            buffer.AudioBytes = voiceSize;
                            buffer.pAudioData = (const BYTE*)pVoiceData;
                            buffer.pContext = pVoiceData;
                            pVoiceContext->m_pSourceVoice->SubmitSourceBuffer(&buffer, nullptr);
                            pSender->lastVoiceDataTimeStamp = m_currentRunTime;
                            pSender->voiceChatInfo.hasHeadset = true;
                        }
                        break;
                    case GameLinkSteamPacketType_UserDefined:
                    {
                        if (bufferSize < pHeader->actualSize)
                        {
                            keen::debug::nativePrintArgs("[gamelink] getNextP2PMessage: the provided buffer is too small, need at least %u bytes\n", pHeader->actualSize);
                            *pReturnSender = nullptr;
                            return 0;
                        }
                        // the packets of a refused member are dropped (and end the reading for this frame); a sender
                        // that is not a lobby member is returned as nullptr
                        const GameLinkRemoteStationSteam* pSender = (const GameLinkRemoteStationSteam*)findRemoteStationInLobby(senderId);
                        if (pSender != nullptr && pSender->isBanned)
                        {
                            *pReturnSender = nullptr;
                            return 0;
                        }
                        *pReturnSender = pSender;
                        memcpy(pBuffer, pData, pHeader->actualSize);
                        return pHeader->actualSize;
                    }
                    case GameLinkSteamPacketType_VACAuthTicketRequest:
                        handleReceivedAuthTicket(packet, packetSize, senderId);
                        break;
                    case GameLinkSteamPacketType_VACAuthTicketResponse:
                        handleAuthTicketResponse(packet, packetSize, senderId);
                        break;
                    }
                }
            }
            *pReturnSender = nullptr;
            return 0;
        }

        // 0x00CD4950 (:1207)
        const GameLinkRemoteStation* GameLinkInternet::getLobbyOwner() const
        {
            if (m_state != GameLinkState_InLobby || !m_lobbyOwnerSteamId.IsValid())
            {
                return nullptr;
            }
            return findRemoteStationInLobby(m_lobbyOwnerSteamId);
        }

        // 0x00CD4980 (:1221): the creator of a lobby is its owner while the lobby is being created
        bool GameLinkInternet::isLobbyOwner() const
        {
            if (m_state != GameLinkState_InLobby)
            {
                return false;
            }
            if (m_createLobbyRequest.id != 0)
            {
                return true;
            }
            if (!m_lobbyOwnerSteamId.IsValid())
            {
                return false;
            }
            return getMySteamId() == m_lobbyOwnerSteamId;
        }

        // 0x00CD1C00 (:1239): by the provider data (the Steam id) alone
        const GameLinkRemoteStation* GameLinkInternet::getRemoteStationById(unit4::RemoteUserId id) const
        {
            if (m_state != GameLinkState_InLobby)
            {
                return nullptr;
            }
            for (uint32 i = 0; i < m_lobbyMembers.getSize(); ++i)
            {
                if (memcmp(m_lobbyMembers.getStart()[i].id.providerData, id.providerData, sizeof(id.providerData)) == 0)
                {
                    return &m_lobbyMembers.getStart()[i];
                }
            }
            return nullptr;
        }

        // 0x00CD4A00 (:1258)
        void GameLinkInternet::setVoiceChatEnabled(bool isEnabled)
        {
            if (m_pSoundSystem == nullptr)
            {
                return;
            }
            if (isEnabled)
            {
                if (!m_voiceChatEnabled)
                {
                    m_ownVoiceChatInfo.isMuted = false;
                    SteamUser()->StartVoiceRecording();
                    SteamFriends()->SetInGameVoiceSpeaking(SteamUser()->GetSteamID(), true);
                    m_voiceChatEnabled = true;
                }
            }
            else if (m_voiceChatEnabled)
            {
                m_ownVoiceChatInfo.isMuted = true;
                SteamUser()->StopVoiceRecording();
                SteamFriends()->SetInGameVoiceSpeaking(SteamUser()->GetSteamID(), false);
                m_voiceChatEnabled = false;
            }
        }

        // 0x00CD4B10 (:1282)
        void GameLinkInternet::setVoiceChatLocalPlayerMuted(bool isMuted)
        {
            if (!m_voiceChatEnabled)
            {
                return;
            }
            if (isMuted)
            {
                if (m_ownVoiceChatInfo.isMuted)
                {
                    return;
                }
                m_ownVoiceChatInfo.isMuted = true;
                SteamUser()->StopVoiceRecording();
                SteamFriends()->SetInGameVoiceSpeaking(SteamUser()->GetSteamID(), false);
            }
            else
            {
                if (!m_ownVoiceChatInfo.isMuted)
                {
                    return;
                }
                m_ownVoiceChatInfo.isMuted = false;
                SteamUser()->StartVoiceRecording();
                SteamFriends()->SetInGameVoiceSpeaking(SteamUser()->GetSteamID(), true);
            }
        }

        // 0x00CD1D00 (:1303): until the next member list update (updateLobbyMembersShared clears it)
        void GameLinkInternet::setVoiceChatRemotePlayerMuted(const GameLinkRemoteStation& remoteStation, bool isMuted)
        {
            if (!m_voiceChatEnabled)
            {
                return;
            }
            for (uint32 i = 0; i < m_lobbyMembers.getSize(); ++i)
            {
                GameLinkRemoteStationSteam& member = m_lobbyMembers.getStart()[i];
                if (memcmp(member.id.providerData, remoteStation.id.providerData, sizeof(member.id.providerData)) == 0)
                {
                    member.voiceChatInfo.isMuted = isMuted;
                    return;
                }
            }
        }

        // 0x00CD0510 (:1321): "just spoken" is within the last second
        bool GameLinkInternet::getVoiceChatLocalPlayerInfo(GameLinkVoiceChatPlayerInfo* pVoiceChatInfo)
        {
            if (!m_voiceChatEnabled)
            {
                return false;
            }
            m_ownVoiceChatInfo.hasJustSpoken = m_currentRunTime - m_ownLastVoiceDataTimeStamp <= 1.0f;
            *pVoiceChatInfo = m_ownVoiceChatInfo;
            return true;
        }

        // 0x00CD1DC0 (:1336)
        bool GameLinkInternet::getVoiceChatRemotePlayerInfo(GameLinkVoiceChatPlayerInfo* pVoiceChatInfo, const GameLinkRemoteStation& remoteStation)
        {
            if (!m_voiceChatEnabled)
            {
                return false;
            }
            for (uint32 i = 0; i < m_lobbyMembers.getSize(); ++i)
            {
                GameLinkRemoteStationSteam& member = m_lobbyMembers.getStart()[i];
                if (memcmp(member.id.providerData, remoteStation.id.providerData, sizeof(member.id.providerData)) == 0)
                {
                    member.voiceChatInfo.hasJustSpoken = m_currentRunTime - member.lastVoiceDataTimeStamp <= 1.0f;
                    *pVoiceChatInfo = member.voiceChatInfo;
                    return true;
                }
            }
            return false;
        }

        // 0x00CD0580 (:1359)
        bool GameLinkInternet::hasVoiceChatPermissions() const
        {
            return true;
        }

        // 0x00CD0590 (:1364)
        GameLinkLobby GameLinkInternet::getLobbyId() const
        {
            GameLinkLobby lobby;
            lobby.id.asUint64 = m_lobbyId.IsValid() ? m_lobbyId.ConvertToUint64() : 0;
            return lobby;
        }

        // 0x00CD4BF0 (:1377)
        bool GameLinkInternet::setLobbyType(GameLinkLobbyType lobbyType)
        {
            if (lobbyType == GameLinkLobbyType_None)
            {
                keen::debug::nativePrintArgs("[gamelink] setLobbyType: tried to set GameLinkLobbyType_None\n");
                return false;
            }
            if (m_state != GameLinkState_InLobby || !isLobbyOwner())
            {
                return false;
            }
            m_setLobbyTypeRequest.id = generateRequestId();
            m_setLobbyTypeRequest.retryCount = 20;
            m_setLobbyTypeRequest.timeRequested = keen::SystemTimer::getCurrentMilliseconds();
            m_setLobbyTypeRequest.lobbyType = convertToSteamLobbyType(lobbyType);
            if (requestSetLobbyType(m_setLobbyTypeRequest))
            {
                m_setLobbyTypeRequest.id = 0;
            }
            return true;
        }

        // 0x00CD4C90 (:1412)
        bool GameLinkInternet::setLobbyLocked(bool locked)
        {
            if (m_state != GameLinkState_InLobby || !isLobbyOwner())
            {
                return false;
            }
            m_setLobbyLockedRequest.id = generateRequestId();
            m_setLobbyLockedRequest.retryCount = 20;
            m_setLobbyLockedRequest.timeRequested = keen::SystemTimer::getCurrentMilliseconds();
            m_setLobbyLockedRequest.locked = locked;
            if (requestSetLobbyLocked(m_setLobbyLockedRequest))
            {
                m_setLobbyLockedRequest.id = 0;
            }
            return true;
        }

        // 0x00CD85B0 (:1442)
        bool GameLinkInternet::setLobbyOwner(unit4::RemoteUserId newOwnerId)
        {
            if (!newOwnerId.isValid || m_state != GameLinkState_InLobby || !m_lobbyId.IsValid() || !isLobbyOwner())
            {
                return false;
            }
            const CSteamID newOwnerSteamId = unit4::systemservices::convertToCSteamID(newOwnerId);
            const bool result = SteamMatchmaking()->SetLobbyOwner(m_lobbyId, newOwnerSteamId);
            if (result)
            {
                m_lobbyOwnerSteamId = newOwnerSteamId;
            }
            updateLobbyMembers();
            return result;
        }

        // 0x00CD05D0 (:1478)
        uint32 GameLinkInternet::getLobbyMaxMembers() const
        {
            if (m_state != GameLinkState_InLobby)
            {
                return 0;
            }
            return m_currentMaxNumLobbyMembers;
        }

        // 0x00CD1ED0 (:1488): clamped to 0..1
        void GameLinkInternet::setVoiceChatVolume(float volume)
        {
            if (volume - 0.0f < 0.0f)
            {
                volume = 0.0f;
            }
            if (volume - 1.0f >= 0.0f)
            {
                volume = 1.0f;
            }
            m_voicechat_volume = volume;
            for (GameLinkSteamVoiceChatData* it = m_voiceChatDataArray.getStart(); it < m_voiceChatDataArray.getEnd(); ++it)
            {
                it->pVoiceContext->m_pSourceVoice->SetVolume(m_voicechat_volume, XAUDIO2_COMMIT_NOW);
            }
        }

        // 0x00CD1F70 (:1501): the voices of the old device go
        void GameLinkInternet::handleAboutToDestroyMasteringVoice(keen::SoundSystem::System*)
        {
            for (GameLinkSteamVoiceChatData* it = m_voiceChatDataArray.getStart(); it < m_voiceChatDataArray.getEnd(); ++it)
            {
                if (it->pVoiceContext == nullptr)
                {
                    keen::debug::nativePrintArgs("[gamelink] <handleAboutToDestroyMasteringVoice> voiceChatDataItr->pVoiceContext is a nullptr! \n");
                }
                else if (it->pVoiceContext->m_pSourceVoice == nullptr)
                {
                    keen::debug::nativePrintArgs("[gamelink] <handleAboutToDestroyMasteringVoice> voiceChatDataItr->pVoiceContext->m_pSourceVoice is a nullptr! \n");
                }
                else
                {
                    it->pVoiceContext->m_pSourceVoice->Stop(0, XAUDIO2_COMMIT_NOW);
                    it->pVoiceContext->m_pSourceVoice->DestroyVoice();
                    // Original: pVoiceContext is left dangling until handleMasteringVoiceCreated replaces it.
                    keen::deleteObject(it->pVoiceContext, m_pAllocator);
                }
            }
        }

        // 0x00CD4D10 (:1525): new voices on the new device (createVoiceContext inlined)
        void GameLinkInternet::handleMasteringVoiceCreated(keen::SoundSystem::System*)
        {
            for (GameLinkSteamVoiceChatData* it = m_voiceChatDataArray.getStart(); it < m_voiceChatDataArray.getEnd(); ++it)
            {
                it->pVoiceContext = createVoiceContext();
            }
        }

        // 0x00CD61A0 (:1535): rebuilds the member list from Steam, keeping each known member's auth and played-with state
        void GameLinkInternet::updateLobbyMembersShared(CSteamID lobbyID, CSteamID& ownerID, keen::SizedArray<GameLinkRemoteStationSteam>& membersList)
        {
            const uint32 now = keen::SystemTimer::getCurrentMilliseconds();
            GameLinkRemoteStationSteam* pOldMembers = nullptr;
            uint32 numOldMembers = 0;
            if (membersList.getCapacity() != 0)
            {
                pOldMembers = (GameLinkRemoteStationSteam*)m_pAllocator->allocate(membersList.getCapacity() * sizeof(GameLinkRemoteStationSteam), 0x10, 0, nullptr);
            }
            if (membersList.getCapacity() == 0 || pOldMembers != nullptr)
            {
                for (uint32 i = 0; i < membersList.getSize(); ++i)
                {
                    if (pOldMembers != nullptr)
                    {
                        pOldMembers[i] = membersList.getStart()[i];
                    }
                }
                numOldMembers = membersList.getSize();
            }
            const uint8 numMembers = (uint8)SteamMatchmaking()->GetNumLobbyMembers(lobbyID);
            ownerID = SteamMatchmaking()->GetLobbyOwner(lobbyID);
            const CSteamID mySteamId = SteamUser()->GetSteamID();
            membersList.clear();
            if (numMembers > membersList.getCapacity())
            {
                keen::debug::nativePrintArgs("[gamelink] membersList capacity is too small for numLobbyMembers: %u\n", (uint32)numMembers);
                setError(GameLinkError_Unknown, 0);
                endSession();
                // Original bug kept: the copy of the old members leaks.
                return;
            }
            for (uint32 i = 0; i < numMembers; ++i)
            {
                const CSteamID memberId = SteamMatchmaking()->GetLobbyMemberByIndex(lobbyID, (int)i);
                GameLinkRemoteStationSteam station;
                memset(&station, 0, sizeof(station));
                station.id = unit4::systemservices::convertToOnlineSystemUserId(memberId, 0);
                keen::copyString(station.userName, sizeof(station.userName), SteamFriends()->GetFriendPersonaName(memberId));
                station.lobbyOwner = memberId == ownerID;
                station.myStation = memberId == mySteamId;
                station.voiceChatInfo.isMuted = false;
                station.voiceChatInfo.hasJustSpoken = false;
                // long ago: the member has not spoken
                station.lastVoiceDataTimeStamp = -100.0f;
                station.playedWithRequested = false;
                station.isBanned = false;
                station.hasValidatedMyAuthTicket = false;
                bool found = false;
                for (uint32 j = 0; j < numOldMembers; ++j)
                {
                    if (memcmp(pOldMembers[j].id.providerData, station.id.providerData, sizeof(station.id.providerData)) == 0)
                    {
                        station.playedWithRequested = pOldMembers[j].playedWithRequested;
                        station.isBanned = pOldMembers[j].isBanned;
                        station.hasValidatedMyAuthTicket = pOldMembers[j].hasValidatedMyAuthTicket;
                        station.hasSentHisAuthTicket = pOldMembers[j].hasSentHisAuthTicket;
                        station.timeEnteredLobby = pOldMembers[j].timeEnteredLobby;
                        found = true;
                        break;
                    }
                }
                if (!found)
                {
                    station.timeEnteredLobby = now;
                }
                membersList.pushBack(station);
            }
            if (m_pSoundSystem != nullptr)
            {
                updateSourceVoiceArray(membersList, numMembers);
                setVoiceChatVolume(m_voicechat_volume);
            }
            if (pOldMembers != nullptr)
            {
                m_pAllocator->free(pOldMembers);
            }
        }

        // 0x00CD6530 (:1608)
        void GameLinkInternet::updateLobbyMembers()
        {
            updateLobbyMembersShared(m_lobbyId, m_lobbyOwnerSteamId, m_lobbyMembers);
        }

        // 0x00CD6560 (:1613)
        void GameLinkInternet::updatePartyMembers()
        {
            updateLobbyMembersShared(m_partyId, m_partyOwnerSteamId, m_partyMembers);
        }

        // 0x00CD2020 (:1618)
        const CSteamID GameLinkInternet::getMySteamId() const
        {
            return SteamUser()->GetSteamID();
        }

        // 0x00CD2050 (:1623)
        bool GameLinkInternet::isLoggedOn() const
        {
            return SteamUser()->BLoggedOn();
        }

        // 0x00CD2070 (:1628)
        CSteamID GameLinkInternet::convertToSteamId(const GameLinkLobby& lobby) const
        {
            return CSteamID(lobby.id.asUint64);
        }

        // 0x00CD2090 (:1633)
        CSteamID GameLinkInternet::convertToSteamId(const GameLinkRemoteStation& station) const
        {
            return getSteamId(station);
        }

        // 0x00CD05E0 (:1638)
        ELobbyType GameLinkInternet::convertToSteamLobbyType(GameLinkLobbyType lobbyType)
        {
            switch (lobbyType)
            {
            case GameLinkLobbyType_None:
                keen::debug::nativePrintArgs("[gamelink] convertToSteamLobbyType: GameLinkLobbyType_None\n");
                return k_ELobbyTypePrivate;
            case GameLinkLobbyType_Public:
                // Original: a "public" lobby is invisible (found by searches, not shown in the friends list).
                return k_ELobbyTypeInvisible;
            case GameLinkLobbyType_Friends:
                return k_ELobbyTypeFriendsOnly;
            case GameLinkLobbyType_Private:
            default:
                return k_ELobbyTypePrivate;
            }
        }

        // 0x00CD20B0 (:1662)
        GameLinkRemoteStation* GameLinkInternet::findRemoteStationInLobby(const CSteamID& steamId)
        {
            for (uint32 i = 0; i < m_lobbyMembers.getSize(); ++i)
            {
                if (getSteamId(m_lobbyMembers.getStart()[i]) == steamId)
                {
                    return &m_lobbyMembers.getStart()[i];
                }
            }
            return nullptr;
        }

        // 0x00CD2100 (:1675)
        const GameLinkRemoteStation* GameLinkInternet::findRemoteStationInLobby(const CSteamID& steamId) const
        {
            for (uint32 i = 0; i < m_lobbyMembers.getSize(); ++i)
            {
                if (getSteamId(m_lobbyMembers.getStart()[i]) == steamId)
                {
                    return &m_lobbyMembers.getStart()[i];
                }
            }
            return nullptr;
        }

        // 0x00CD2150 (:1688)
        GameLinkRemoteStation* GameLinkInternet::findRemoteStationInParty(const CSteamID& steamId)
        {
            for (uint32 i = 0; i < m_partyMembers.getSize(); ++i)
            {
                if (getSteamId(m_partyMembers.getStart()[i]) == steamId)
                {
                    return &m_partyMembers.getStart()[i];
                }
            }
            return nullptr;
        }

        // 0x00CD21A0 (:1701)
        const GameLinkRemoteStation* GameLinkInternet::findRemoteStationInParty(const CSteamID& steamId) const
        {
            for (uint32 i = 0; i < m_partyMembers.getSize(); ++i)
            {
                if (getSteamId(m_partyMembers.getStart()[i]) == steamId)
                {
                    return &m_partyMembers.getStart()[i];
                }
            }
            return nullptr;
        }

        // 0x00CD21F0 (:1714): lobby data "f<index>"
        bool GameLinkInternet::getFilterFromLobbyData(uint32 index, uint32& value)
        {
            char key[32];
            keen::formatString(key, sizeof(key), "f%u", index);
            const char* pValue = SteamMatchmaking()->GetLobbyData(m_lobbyId, key);
            if (pValue == nullptr || *pValue == '\0')
            {
                return false;
            }
            keen::readUint32FromDecimalString(&value, &pValue);
            return true;
        }

        // 0x00CD2290 (:1733)
        void GameLinkInternet::resetAllVariables()
        {
            m_lobbyId.Clear();
            m_partyId.Clear();
            m_lobbyOwnerSteamId.Clear();
            m_partyOwnerSteamId.Clear();
            m_currentMaxNumLobbyMembers = 0;
            m_lobbyMembers.clear();
            m_partyMembers.clear();
            for (uint32 i = 0; i < m_lobbyFilters.getSize(); ++i)
            {
                m_lobbyFilters[i] = 0;
            }
            m_currentPartyLobbyData.lobbyId.Clear();
            m_currentPartyLobbyData.lastLobbyId.Clear();
            m_searchResults.clear();
            m_searchRunning = false;
            memset((GameLinkRequest*)&m_createLobbyRequest, 0, sizeof(GameLinkRequest));
            m_createLobbyRequest.lobbyType = GameLinkLobbyType_None;
            m_createLobbyRequest.maxNumLobbyMembers = 0;
            memset((GameLinkRequest*)&m_createPartyRequest, 0, sizeof(GameLinkRequest));
            m_createPartyRequest.lobbyType = GameLinkLobbyType_None;
            m_createPartyRequest.maxNumLobbyMembers = 0;
            memset((GameLinkRequest*)&m_joinLobbyRequest, 0, sizeof(GameLinkRequest));
            m_joinLobbyRequest.lobbyId.Clear();
            memset((GameLinkRequest*)&m_joinPartyRequest, 0, sizeof(GameLinkRequest));
            m_joinPartyRequest.lobbyId.Clear();
            memset(&m_setLobbyFiltersRequest, 0, sizeof(m_setLobbyFiltersRequest));
            memset((GameLinkRequest*)&m_setLobbyTypeRequest, 0, sizeof(GameLinkRequest));
            m_setLobbyTypeRequest.lobbyType = k_ELobbyTypePrivate;
            memset(&m_setLobbyLockedRequest, 0, sizeof(m_setLobbyLockedRequest));
            m_findAndJoinData.status = GameLinkFindAndJoinStatus_None;
            m_findAndJoinData.maxNumLobbyMembers = 0;
            if (m_pSoundSystem != nullptr)
            {
                for (GameLinkSteamVoiceChatData* it = m_voiceChatDataArray.getStart(); it < m_voiceChatDataArray.getEnd(); ++it)
                {
                    it->pVoiceContext->m_pSourceVoice->Stop(0, XAUDIO2_COMMIT_NOW);
                    it->pVoiceContext->m_pSourceVoice->DestroyVoice();
                    keen::deleteObject(it->pVoiceContext, m_pAllocator);
                }
                m_voiceChatDataArray.clear();
            }
        }

        // 0x00CD4EE0 (:1779)
        void GameLinkInternet::resetLobbyVariables()
        {
            if (m_internetCreationParams.vacEnabled)
            {
                if (m_localAuthTicket.inUse)
                {
                    SteamUser()->CancelAuthTicket(m_localAuthTicket.handle);
                    m_localAuthTicket.inUse = false;
                }
                for (uint32 i = 0; i < 64; ++i)
                {
                    if (m_remoteAuthTickets[i].inUse)
                    {
                        SteamUser()->EndAuthSession(m_remoteAuthTickets[i].steamID);
                        m_remoteAuthTickets[i].inUse = false;
                    }
                }
            }
            m_lobbyId.Clear();
            m_lobbyOwnerSteamId.Clear();
            m_currentMaxNumLobbyMembers = 0;
            m_lobbyMembers.clear();
            for (uint32 i = 0; i < m_lobbyFilters.getSize(); ++i)
            {
                m_lobbyFilters[i] = 0;
            }
        }

        // 0x00CD24B0 (:1807)
        void GameLinkInternet::requestCreateLobby(const CreateLobbyRequest& request, CCallResult<GameLinkInternet, LobbyCreated_t>& callResult, void (GameLinkInternet::*pCallback)(LobbyCreated_t*, bool))
        {
            const ELobbyType lobbyType = convertToSteamLobbyType(request.lobbyType);
            const SteamAPICall_t hCall = SteamMatchmaking()->CreateLobby(lobbyType, (int)request.maxNumLobbyMembers);
            callResult.Set(hCall, this, pCallback);
        }

        // 0x00CD2570 (:1818)
        void GameLinkInternet::requestLeaveLobby(CSteamID lobbyId)
        {
            if (lobbyId.IsValid())
            {
                SteamMatchmaking()->LeaveLobby(lobbyId);
            }
        }

        // 0x00CD25B0 (:1827)
        void GameLinkInternet::requestJoinLobby(const JoinLobbyRequest& request, CCallResult<GameLinkInternet, LobbyEnter_t>& callResult, void (GameLinkInternet::*pCallback)(LobbyEnter_t*, bool))
        {
            const SteamAPICall_t hCall = SteamMatchmaking()->JoinLobby(request.lobbyId);
            callResult.Set(hCall, this, pCallback);
        }

        // 0x00CD2630 (:1835): stops at the first filter Steam refuses
        bool GameLinkInternet::requestSetLobbyFilters(const SetLobbyFiltersRequest& request)
        {
            if (!m_lobbyId.IsValid())
            {
                return false;
            }
            for (uint32 i = 0; i < 8; ++i)
            {
                if (request.lobbyFiltersToSet[i])
                {
                    char key[32];
                    char value[32];
                    keen::formatString(key, sizeof(key), "f%u", i);
                    keen::formatString(value, sizeof(value), "%u", request.lobbyFilters[i]);
                    if (!SteamMatchmaking()->SetLobbyData(m_lobbyId, key, value))
                    {
                        return false;
                    }
                }
            }
            return true;
        }

        // 0x00CD2720 (:1865)
        bool GameLinkInternet::requestSetLobbyType(const SetLobbyTypeRequest& request)
        {
            if (!m_lobbyId.IsValid())
            {
                return false;
            }
            return SteamMatchmaking()->SetLobbyType(m_lobbyId, request.lobbyType);
        }

        // 0x00CD2770 (:1883)
        bool GameLinkInternet::requestSetLobbyLocked(const SetLobbyLockedRequest& request)
        {
            if (!m_lobbyId.IsValid())
            {
                return false;
            }
            return SteamMatchmaking()->SetLobbyJoinable(m_lobbyId, !request.locked);
        }

        // 0x00CD27C0 (:1901): the party owner publishes his game lobby as the party's "lobby_id"
        void GameLinkInternet::updateLobbyInformationInParty()
        {
            m_currentPartyLobbyData.lobbyId = m_lobbyId;
            char value[32];
            keen::formatString(value, sizeof(value), "%llu", m_lobbyId.ConvertToUint64());
            if (!SteamMatchmaking()->SetLobbyData(m_partyId, "lobby_id", value))
            {
                keen::debug::nativePrintArgs("[gamelink] updateLobbyInformationInParty: could not set lobby data on party\n");
            }
        }

        // 0x00CD2850 (:1916)
        void GameLinkInternet::updateLobbyInformationFromParty()
        {
            const char* pValue = SteamMatchmaking()->GetLobbyData(m_partyId, "lobby_id");
            if (pValue != nullptr && *pValue != '\0')
            {
                uint64_t lobbyId = 0;
                keen::readSignLessUint64FromDecimalString(&lobbyId, &pValue);
                m_currentPartyLobbyData.lobbyId.SetFromUint64(lobbyId);
            }
        }

        // 0x00CD0630 (:1932)
        bool GameLinkInternet::isProcessingAnyRequest()
        {
            return m_createLobbyRequest.idProcessing != 0 || m_joinLobbyRequest.idProcessing != 0 || m_createPartyRequest.idProcessing != 0 || m_joinPartyRequest.idProcessing != 0;
        }

        // 0x00CD8650 (:1945): a friends-only lobby as big as the party
        void GameLinkInternet::createPartyLobby()
        {
            m_createPartyRequest.id = generateRequestId();
            m_createPartyRequest.timeRequested = keen::SystemTimer::getCurrentMilliseconds();
            m_createPartyRequest.retryCount = 20;
            m_createPartyRequest.lobbyType = GameLinkLobbyType_Friends;
            m_createPartyRequest.maxNumLobbyMembers = m_partyMembers.getCapacity();
            if (!isProcessingAnyRequest())
            {
                requestCreateLobby(m_createPartyRequest, m_callResultPartyCreated, &GameLinkInternet::handlePartyCreation);
                m_createPartyRequest.idProcessing = m_createPartyRequest.id;
            }
            m_currentPartyLobbyData.lobbyId.Clear();
            m_currentPartyLobbyData.lastLobbyId.Clear();
            m_partyEnabled = true;
        }

        // 0x00CD6590 (:1965)
        void GameLinkInternet::joinPartyLobby(CSteamID lobbyId)
        {
            requestLeaveLobby(m_partyId);
            m_createPartyRequest.id = 0;
            m_joinPartyRequest.id = generateRequestId();
            m_joinPartyRequest.timeRequested = keen::SystemTimer::getCurrentMilliseconds();
            m_joinPartyRequest.retryCount = 20;
            m_joinPartyRequest.lobbyId = lobbyId;
            if (!isProcessingAnyRequest())
            {
                requestJoinLobby(m_joinPartyRequest, m_callResultPartyEntered, &GameLinkInternet::handlePartyEnteredCallResult);
                m_joinPartyRequest.idProcessing = m_joinPartyRequest.id;
            }
            m_currentPartyLobbyData.lobbyId.Clear();
            m_currentPartyLobbyData.lastLobbyId.Clear();
            m_partyEnabled = true;
        }

        // 0x00CD28D0 (:1987)
        void GameLinkInternet::leavePartyLobby()
        {
            requestLeaveLobby(m_partyId);
            m_partyMembers.clear();
            m_partyId.Clear();
            m_partyOwnerSteamId.Clear();
            m_partyEnabled = false;
            m_currentPartyLobbyData.lobbyId.Clear();
            m_currentPartyLobbyData.lastLobbyId.Clear();
        }

        // 0x00CD4FE0 (:1997): the party owner's party follows him out of the lobby
        void GameLinkInternet::leaveLobbyInternal()
        {
            if (m_state != GameLinkState_InLobby)
            {
                return;
            }
            if (m_lobbyId.IsValid())
            {
                requestLeaveLobby(m_lobbyId);
            }
            resetLobbyVariables();
            if (m_partyEnabled && isPartyOwner())
            {
                m_currentPartyLobbyData.lobbyId.Clear();
                m_currentPartyLobbyData.lastLobbyId.Clear();
                updateLobbyInformationInParty();
            }
            m_state = GameLinkState_Online;
        }

        // 0x00CD6690 (:2024): joining the current lobby (or the one being joined) again is a success that does nothing
        bool GameLinkInternet::joinLobbyInternal(const GameLinkLobby& lobby)
        {
            const CSteamID lobbyId = convertToSteamId(lobby);
            if (!lobbyId.IsValid())
            {
                return false;
            }
            if (lobbyId == m_lobbyId || lobbyId == m_joinLobbyRequest.lobbyId)
            {
                return true;
            }
            if (m_state == GameLinkState_Offline)
            {
                return false;
            }
            if (m_state == GameLinkState_InLobby && m_findAndJoinData.status != GameLinkFindAndJoinStatus_Running)
            {
                leaveLobbyInternal();
            }
            m_createLobbyRequest.id = 0;
            m_joinLobbyRequest.id = generateRequestId();
            m_joinLobbyRequest.timeRequested = keen::SystemTimer::getCurrentMilliseconds();
            m_joinLobbyRequest.retryCount = 20;
            m_joinLobbyRequest.lobbyId = lobbyId;
            if (!isProcessingAnyRequest())
            {
                requestJoinLobby(m_joinLobbyRequest, m_callResultLobbyEntered, &GameLinkInternet::handleLobbyEnteredCallResult);
                m_joinLobbyRequest.idProcessing = m_joinLobbyRequest.id;
            }
            m_state = GameLinkState_InLobby;
            return true;
        }

        // 0x00CD2980 (:2096)
        bool GameLinkInternet::createAuthTicket(GameLinkSteamVACLocalAuthTicket* pOutTicket)
        {
            pOutTicket->handle = SteamUser()->GetAuthSessionTicket(pOutTicket->data, sizeof(pOutTicket->data), &pOutTicket->size);
            if (pOutTicket->handle == k_HAuthTicketInvalid)
            {
                return false;
            }
            pOutTicket->inUse = true;
            return true;
        }

        // 0x00CD29D0 (:2112)
        void GameLinkInternet::destroyAuthTicket(GameLinkSteamVACLocalAuthTicket* pTicket)
        {
            SteamUser()->CancelAuthTicket(pTicket->handle);
            pTicket->inUse = false;
        }

        // 0x00CD0660 (:2121): the buffer size is not checked
        void GameLinkInternet::createAuthTicketRequestPacket(const GameLinkSteamVACLocalAuthTicket& ticket, uint8* pOutPacketBuffer, uint32, uint32* pOutPacketSize)
        {
            GameLinkSteamPacketHeader* pHeader = (GameLinkSteamPacketHeader*)pOutPacketBuffer;
            pHeader->packetType = GameLinkSteamPacketType_VACAuthTicketRequest;
            pHeader->actualSize = ticket.size;
            memmove(pOutPacketBuffer + sizeof(GameLinkSteamPacketHeader), ticket.data, ticket.size);
            *pOutPacketSize = pHeader->actualSize + sizeof(GameLinkSteamPacketHeader);
        }

        // 0x00CD2A00 (:2138): a member validated the local ticket; a banned local user leaves
        void GameLinkInternet::handleAuthTicketResponse(const void* pPacket, uint32, CSteamID senderId)
        {
            if (!m_internetCreationParams.vacEnabled)
            {
                return;
            }
            GameLinkRemoteStationSteam* pSender = (GameLinkRemoteStationSteam*)findRemoteStationInLobby(senderId);
            if (pSender != nullptr)
            {
                pSender->hasValidatedMyAuthTicket = true;
            }
            const GameLinkSteamVACAuthTicketResponse* pResponse = (const GameLinkSteamVACAuthTicketResponse*)((const uint8*)pPacket + sizeof(GameLinkSteamPacketHeader));
            if (doesItMeanVACBanned(pResponse->beginAuthSessionResult, pResponse->authSessionResponse))
            {
                setError(GameLinkError_NoMultiplayerBanned, 0);
                endSession();
            }
        }

        // 0x00CD5090 (:2161): a member's ticket; once per member, answered with the result of beginning the session
        void GameLinkInternet::handleReceivedAuthTicket(const void* pPacket, uint32, CSteamID senderId)
        {
            if (!m_internetCreationParams.vacEnabled)
            {
                return;
            }
            GameLinkRemoteStationSteam* pSender = (GameLinkRemoteStationSteam*)findRemoteStationInLobby(senderId);
            if (pSender == nullptr || findRemoteAuthTicket(senderId) != nullptr)
            {
                return;
            }
            GameLinkSteamVACRemoteAuthTicket* pTicket = getFreeRemoteAuthTicket();
            if (pTicket == nullptr)
            {
                return;
            }
            const GameLinkSteamPacketHeader* pHeader = (const GameLinkSteamPacketHeader*)pPacket;
            pTicket->inUse = true;
            pTicket->steamID = senderId;
            pTicket->size = pHeader->actualSize;
            // Original bug kept: the size comes from the packet unchecked (up to 1192 bytes into the 1024-byte data).
            memmove(pTicket->data, (const uint8*)pPacket + sizeof(GameLinkSteamPacketHeader), pHeader->actualSize);
            pSender->hasSentHisAuthTicket = true;
            const EBeginAuthSessionResult result = beginAuthSession(*pTicket);
            uint8 response[sizeof(GameLinkSteamPacketHeader) + sizeof(GameLinkSteamVACAuthTicketResponse)];
            uint32 responseSize;
            createAuthTicketResponsePacket(result, k_EAuthSessionResponseOK, response, sizeof(response), &responseSize);
            if (result != k_EBeginAuthSessionResultOK)
            {
                pTicket->inUse = false;
                refuseConnection(pTicket->steamID);
            }
            sendP2PPacketToSteamID(senderId, response, sizeof(response));
        }

        // 0x00CD2AB0 (:2212)
        EBeginAuthSessionResult GameLinkInternet::beginAuthSession(const GameLinkSteamVACRemoteAuthTicket& ticket)
        {
            return SteamUser()->BeginAuthSession(ticket.data, (int)ticket.size, ticket.steamID);
        }

        // 0x00CD2AF0 (:2220)
        void GameLinkInternet::endAuthSession(CSteamID id)
        {
            SteamUser()->EndAuthSession(id);
        }

        // 0x00CD06F0 (:2228)
        void GameLinkInternet::createAuthTicketResponsePacket(EBeginAuthSessionResult beginAuthSessionResult, EAuthSessionResponse authSessionResponse, uint8* pOutPacketBuffer, uint32, uint32* pOutPacketSize)
        {
            GameLinkSteamPacketHeader* pHeader = (GameLinkSteamPacketHeader*)pOutPacketBuffer;
            pHeader->packetType = GameLinkSteamPacketType_VACAuthTicketResponse;
            pHeader->actualSize = sizeof(GameLinkSteamVACAuthTicketResponse);
            GameLinkSteamVACAuthTicketResponse* pResponse = (GameLinkSteamVACAuthTicketResponse*)(pOutPacketBuffer + sizeof(GameLinkSteamPacketHeader));
            pResponse->beginAuthSessionResult = beginAuthSessionResult;
            pResponse->authSessionResponse = authSessionResponse;
            *pOutPacketSize = pHeader->actualSize + sizeof(GameLinkSteamPacketHeader);
        }

        // 0x00CD2B20 (:2244): no more packets to or from the member
        void GameLinkInternet::refuseConnection(CSteamID id)
        {
            GameLinkRemoteStationSteam* pMember = (GameLinkRemoteStationSteam*)findRemoteStationInLobby(id);
            if (pMember != nullptr)
            {
                pMember->isBanned = true;
            }
        }

        // 0x00CD0720 (:2262)
        GameLinkSteamVACRemoteAuthTicket* GameLinkInternet::getFreeRemoteAuthTicket()
        {
            for (uint32 i = 0; i < 64; ++i)
            {
                if (!m_remoteAuthTickets[i].inUse)
                {
                    return &m_remoteAuthTickets[i];
                }
            }
            return nullptr;
        }

        // 0x00CD0750 (:2276)
        GameLinkSteamVACRemoteAuthTicket* GameLinkInternet::findRemoteAuthTicket(CSteamID id)
        {
            for (uint32 i = 0; i < 64; ++i)
            {
                if (m_remoteAuthTickets[i].inUse && m_remoteAuthTickets[i].steamID == id)
                {
                    return &m_remoteAuthTickets[i];
                }
            }
            return nullptr;
        }

        // 0x00CD2B70 (:2290): sends the local ticket (every 200 ms) to the members that have not validated it, and ends
        // the auth sessions of members that left
        void GameLinkInternet::updateAuth()
        {
            if (!m_internetCreationParams.vacEnabled || m_state != GameLinkState_InLobby)
            {
                return;
            }
            const uint32 now = keen::SystemTimer::getCurrentMilliseconds();
            if (!m_localAuthTicket.inUse)
            {
                if (!createAuthTicket(&m_localAuthTicket))
                {
                    setError(GameLinkError_Unknown, 0);
                    endSession();
                }
            }
            else if (now - m_lastTimeAuthTicketRequestSent > 200)
            {
                m_lastTimeAuthTicketRequestSent = now;
                uint8 packet[0x4B0];
                uint32 packetSize;
                createAuthTicketRequestPacket(m_localAuthTicket, packet, sizeof(packet), &packetSize);
                for (uint32 i = 0; i < m_lobbyMembers.getSize(); ++i)
                {
                    const GameLinkRemoteStationSteam& member = m_lobbyMembers.getStart()[i];
                    if (!member.myStation && !member.hasValidatedMyAuthTicket)
                    {
                        SteamNetworking()->SendP2PPacket(getSteamId(member), packet, packetSize, k_EP2PSendUnreliableNoDelay, 0);
                    }
                }
            }
            for (uint32 i = 0; i < 64; ++i)
            {
                GameLinkSteamVACRemoteAuthTicket& ticket = m_remoteAuthTickets[i];
                if (ticket.inUse && findRemoteStationInLobby(ticket.steamID) == nullptr)
                {
                    SteamUser()->EndAuthSession(ticket.steamID);
                    ticket.inUse = false;
                }
            }
        }

        // 0x00CD2D20 (:2370)
        void GameLinkInternet::sendPacketToAllOthersInternal(uint8* pPacket, uint32 size) const
        {
            for (uint32 i = 0; i < m_lobbyMembers.getSize(); ++i)
            {
                const GameLinkRemoteStationSteam& member = m_lobbyMembers.getStart()[i];
                if (!member.myStation)
                {
                    SteamNetworking()->SendP2PPacket(getSteamId(member), pPacket, size, k_EP2PSendUnreliableNoDelay, 0);
                }
            }
        }

        // 0x00CD2DA0 (:2384)
        void GameLinkInternet::sendP2PPacketToSteamID(CSteamID steamID, const void* pPacket, uint32 packetSize) const
        {
            SteamNetworking()->SendP2PPacket(steamID, pPacket, packetSize, k_EP2PSendUnreliableNoDelay, 0);
        }

        // 0x00CD07A0 (:2392)
        bool GameLinkInternet::doesItMeanVACBanned(EBeginAuthSessionResult beginAuthSessionResult, EAuthSessionResponse authSessionResponse) const
        {
            return beginAuthSessionResult != k_EBeginAuthSessionResultOK || authSessionResponse == k_EAuthSessionResponseVACBanned || authSessionResponse == k_EAuthSessionResponseVACCheckTimedOut || authSessionResponse == k_EAuthSessionResponsePublisherIssuedBan;
        }

        // 0x00CD2DE0 (:2407): the lobbies with room for the whole party, except the own lobby and party
        void GameLinkInternet::handleLobbyListRequest(LobbyMatchList_t* pLobbyMatchList, bool)
        {
            for (uint32 i = 0; i < pLobbyMatchList->m_nLobbiesMatching; ++i)
            {
                if (m_searchResults.isFull())
                {
                    break;
                }
                const CSteamID lobbyId = SteamMatchmaking()->GetLobbyByIndex((int)i);
                if (lobbyId == m_lobbyId || lobbyId == m_partyId)
                {
                    continue;
                }
                const int numMembers = SteamMatchmaking()->GetNumLobbyMembers(lobbyId);
                const int memberLimit = SteamMatchmaking()->GetLobbyMemberLimit(lobbyId);
                if (m_partyMembers.getSize() <= (uint32)(memberLimit - numMembers))
                {
                    GameLinkSearchResult result;
                    memset(&result, 0, sizeof(result));
                    result.lobby.id.asUint64 = lobbyId.ConvertToUint64();
                    result.currentNumPlayers = (uint8)numMembers;
                    result.maxNumPlayers = (uint8)memberLimit;
                    m_searchResults.pushBack(result);
                }
            }
            m_searchRunning = false;
        }

        // 0x00CD6790 (:2449): a lobby created after its request was replaced is left again
        void GameLinkInternet::handleLobbyCreationShared(LobbyCreated_t* pCallback, bool, bool forPartyLobby, CSteamID& lobbyId, CreateLobbyRequest& lobbyRequest)
        {
            if (forPartyLobby ? m_state == GameLinkState_Offline : m_state != GameLinkState_InLobby)
            {
                return;
            }
            lobbyId.SetFromUint64(pCallback->m_ulSteamIDLobby);
            if (lobbyRequest.id == lobbyRequest.idProcessing)
            {
                if (pCallback->m_eResult == k_EResultOK)
                {
                    if (forPartyLobby)
                    {
                        updateLobbyMembersShared(m_partyId, m_partyOwnerSteamId, m_partyMembers);
                    }
                    else
                    {
                        updateLobbyMembersShared(m_lobbyId, m_lobbyOwnerSteamId, m_lobbyMembers);
                    }
                    // Original: the game lobby's filters are set on the party lobby too.
                    for (uint32 i = 0; i < m_lobbyFilters.getSize(); ++i)
                    {
                        char key[32];
                        char value[32];
                        keen::formatString(key, sizeof(key), "f%u", i);
                        keen::formatString(value, sizeof(value), "%u", m_lobbyFilters[i]);
                        if (!SteamMatchmaking()->SetLobbyData(lobbyId, key, value))
                        {
                            keen::debug::nativePrintArgs("[gamelink] handleLobbyCreation: could not set filter lobby data\n");
                        }
                    }
                }
                else
                {
                    keen::debug::nativePrintArgs("[gamelink] handleLobbyCreation: pCallback->m_eResult != k_EResultOK\n");
                    const int platformErrorCode = pCallback->m_eResult | GameLinkSteamErrorCodeOrigin_EResult | GameLinkSteamErrorCodeOrigin_EResult_handleLobbyCreationShared;
                    switch (pCallback->m_eResult)
                    {
                    case k_EResultFail:
                    case k_EResultNoConnection:
                    case k_EResultLoggedInElsewhere:
                        endSession();
                        setError(GameLinkError_NoNetworkAccess, platformErrorCode);
                        break;
                    case k_EResultTimeout:
                        if (forPartyLobby)
                        {
                            endSession();
                            setError(GameLinkError_CreateLobbyFailed_Timeout, platformErrorCode);
                        }
                        else
                        {
                            setError(GameLinkError_CreateLobbyFailed_Timeout, platformErrorCode);
                            leaveLobbyInternal();
                        }
                        break;
                    default:
                        if (forPartyLobby)
                        {
                            endSession();
                            setError(GameLinkError_CreateLobbyFailed_Unknown, platformErrorCode);
                        }
                        else
                        {
                            setError(GameLinkError_CreateLobbyFailed_Unknown, platformErrorCode);
                            leaveLobbyInternal();
                        }
                        break;
                    }
                }
            }
            else
            {
                requestLeaveLobby(lobbyId);
                lobbyId.Clear();
            }
            if (m_partyEnabled && isPartyOwner())
            {
                updateLobbyInformationInParty();
            }
            if (lobbyRequest.id == lobbyRequest.idProcessing)
            {
                lobbyRequest.id = 0;
            }
            lobbyRequest.idProcessing = 0;
        }

        // 0x00CD6A20 (:2567)
        void GameLinkInternet::handleLobbyCreation(LobbyCreated_t* pCallback, bool bIOFailure)
        {
            handleLobbyCreationShared(pCallback, bIOFailure, false, m_lobbyId, m_createLobbyRequest);
        }

        // 0x00CD6A50 (:2572)
        void GameLinkInternet::handlePartyCreation(LobbyCreated_t* pCallback, bool bIOFailure)
        {
            handleLobbyCreationShared(pCallback, bIOFailure, true, m_partyId, m_createPartyRequest);
        }

        // 0x00CD5220 (:2577): the members come with the LobbyEnter_t callback (handleLobbyEnteredCallback); a failed
        // join of a party member that follows his owner only leaves the lobby
        void GameLinkInternet::handleLobbyEnteredCallResultShared(LobbyEnter_t* pCallback, bool, bool forPartyLobby, CSteamID& lobbyId, JoinLobbyRequest& lobbyRequest)
        {
            lobbyId.SetFromUint64(pCallback->m_ulSteamIDLobby);
            if (lobbyRequest.id != lobbyRequest.idProcessing)
            {
                requestLeaveLobby(lobbyId);
                lobbyId.Clear();
            }
            // 1: k_EChatRoomEnterResponseSuccess
            if (pCallback->m_EChatRoomEnterResponse != 1)
            {
                keen::debug::nativePrintArgs("[gamelink] handleLobbyEnteredCallResult() pCallback->m_EChatRoomEnterResponse %u\n", pCallback->m_EChatRoomEnterResponse);
                bool leave = true;
                if (forPartyLobby || !isPartyEnabled() || isPartyOwner() || !m_partyId.IsValid())
                {
                    const int platformErrorCode = pCallback->m_EChatRoomEnterResponse | GameLinkSteamErrorCodeOrigin_EChatRoomEnterResponse;
                    // EChatRoomEnterResponse
                    switch (pCallback->m_EChatRoomEnterResponse)
                    {
                    case 2: // DoesntExist
                        setError(GameLinkError_JoinLobbyFailed_NotFound, platformErrorCode);
                        break;
                    case 3: // NotAllowed
                    case 6: // Banned
                    case 7: // Limited
                    case 8: // ClanDisabled
                    case 9: // CommunityBan
                    case 11: // MemberBlockedYou
                        setError(GameLinkError_JoinLobbyFailed_NotAllowedToJoin, platformErrorCode);
                        break;
                    case 4: // Full
                        setError(GameLinkError_JoinLobbyFailed_Full, platformErrorCode);
                        break;
                    case 10: // YouBlockedMember
                        setError(GameLinkError_UserBlocked, platformErrorCode);
                        break;
                    case 5: // Error
                    default:
                        setError(GameLinkError_JoinLobbyFailed_Unknown, platformErrorCode);
                        break;
                    }
                    if (forPartyLobby)
                    {
                        endSession();
                        leave = false;
                    }
                }
                if (leave)
                {
                    leaveLobbyInternal();
                }
            }
            if (m_partyEnabled && isPartyOwner())
            {
                updateLobbyInformationInParty();
            }
            if (lobbyRequest.id != lobbyRequest.idProcessing)
            {
                lobbyRequest.idProcessing = 0;
                return;
            }
            lobbyRequest.id = 0;
            lobbyRequest.lobbyId.Clear();
            lobbyRequest.idProcessing = 0;
        }

        // 0x00CD5400 (:2664)
        void GameLinkInternet::handleLobbyEnteredCallResult(LobbyEnter_t* pCallback, bool bIOFailure)
        {
            handleLobbyEnteredCallResultShared(pCallback, bIOFailure, false, m_lobbyId, m_joinLobbyRequest);
        }

        // 0x00CD5430 (:2669)
        void GameLinkInternet::handlePartyEnteredCallResult(LobbyEnter_t* pCallback, bool bIOFailure)
        {
            handleLobbyEnteredCallResultShared(pCallback, bIOFailure, true, m_partyId, m_joinPartyRequest);
            updateLobbyInformationFromParty();
        }

        // 0x00CD6A80 (:2675)
        void GameLinkInternet::handleLobbyEnteredCallback(LobbyEnter_t* pCallback)
        {
            const CSteamID lobbyId(pCallback->m_ulSteamIDLobby);
            if (m_state != GameLinkState_InLobby && lobbyId != m_partyId)
            {
                return;
            }
            if (lobbyId == m_lobbyId)
            {
                m_currentMaxNumLobbyMembers = SteamMatchmaking()->GetLobbyMemberLimit(m_lobbyId);
                updateLobbyMembersShared(m_lobbyId, m_lobbyOwnerSteamId, m_lobbyMembers);
            }
            else if (lobbyId == m_partyId)
            {
                updateLobbyMembersShared(m_partyId, m_partyOwnerSteamId, m_partyMembers);
            }
        }

        // 0x00CD6B70 (:2702): the lobby's own data (the filters; the update stops at the first missing one) or the
        // party's (the owner's game lobby)
        void GameLinkInternet::handleLobbyDataUpdate(LobbyDataUpdate_t* pCallback)
        {
            const CSteamID lobbyId(pCallback->m_ulSteamIDLobby);
            const bool isOwnLobby = lobbyId == m_lobbyId || lobbyId == m_partyId;
            if (!pCallback->m_bSuccess)
            {
                keen::debug::nativePrintArgs("[gamelink] handleLobbyDataUpdate: pCallback->m_bSuccess == false\n");
                return;
            }
            if (!isOwnLobby)
            {
                return;
            }
            // the lobby itself changed (not one of its members)
            const CSteamID memberId(pCallback->m_ulSteamIDMember);
            if (memberId == m_lobbyId)
            {
                for (uint32 i = 0; i < m_lobbyFilters.getSize(); ++i)
                {
                    if (!getFilterFromLobbyData(i, m_lobbyFilters[i]))
                    {
                        break;
                    }
                }
                updateLobbyMembersShared(m_lobbyId, m_lobbyOwnerSteamId, m_lobbyMembers);
            }
            else if (memberId == m_partyId)
            {
                updateLobbyInformationFromParty();
                updateLobbyMembersShared(m_partyId, m_partyOwnerSteamId, m_partyMembers);
            }
        }

        // 0x00CD6C70 (:2746): the owner query's result is not used
        void GameLinkInternet::handleLobbyChatUpdate(LobbyChatUpdate_t* pCallback)
        {
            const CSteamID lobbyId(pCallback->m_ulSteamIDLobby);
            if (lobbyId == m_lobbyId)
            {
                SteamMatchmaking()->GetLobbyOwner(m_lobbyId);
                updateLobbyMembersShared(m_lobbyId, m_lobbyOwnerSteamId, m_lobbyMembers);
            }
            else if (lobbyId == m_partyId)
            {
                SteamMatchmaking()->GetLobbyOwner(m_partyId);
                updateLobbyMembersShared(m_partyId, m_partyOwnerSteamId, m_partyMembers);
            }
        }

        // 0x00CD2F40 (:2780): sessions from lobby and party members only
        void GameLinkInternet::handleP2PSessionRequest(P2PSessionRequest_t* pCallback)
        {
            if (findRemoteStationInLobby(pCallback->m_steamIDRemote) != nullptr || findRemoteStationInParty(pCallback->m_steamIDRemote) != nullptr)
            {
                SteamNetworking()->AcceptP2PSessionWithUser(pCallback->m_steamIDRemote);
            }
        }

        // 0x00CD07D0 (:2803)
        void GameLinkInternet::handleGetAuthSessionTicketResponse(GetAuthSessionTicketResponse_t*)
        {
        }

        // 0x00CD3000 (:2809): a banned member is told so and refused
        void GameLinkInternet::handleValidateAuthTicketResponse(ValidateAuthTicketResponse_t* pCallback)
        {
            if (!m_internetCreationParams.vacEnabled)
            {
                return;
            }
            const EAuthSessionResponse response = pCallback->m_eAuthSessionResponse;
            if (response == k_EAuthSessionResponseVACBanned || response == k_EAuthSessionResponseVACCheckTimedOut || response == k_EAuthSessionResponsePublisherIssuedBan)
            {
                uint8 packet[sizeof(GameLinkSteamPacketHeader) + sizeof(GameLinkSteamVACAuthTicketResponse)];
                uint32 packetSize;
                createAuthTicketResponsePacket(k_EBeginAuthSessionResultOK, response, packet, sizeof(packet), &packetSize);
                SteamNetworking()->SendP2PPacket(pCallback->m_SteamID, packet, sizeof(packet), k_EP2PSendUnreliableNoDelay, 0);
                refuseConnection(pCallback->m_SteamID);
            }
        }

        // 0x00CD30B0 (:2841): one source voice per member, 16-bit mono PCM at Steam's voice sample rate
        CVoiceContext* GameLinkInternet::createVoiceContext()
        {
            if (m_pIXaudio2 == nullptr)
            {
                return nullptr;
            }
            void* pMemory = m_pAllocator->allocate(sizeof(CVoiceContext), 4, 0, nullptr);
            if (pMemory == nullptr)
            {
                keen::debug::nativePrintArgs("[gamelink] getNextP2PMessage: failed to allocate %d Memory for Voice Context \n", (int)sizeof(CVoiceContext));
                return nullptr;
            }
            CVoiceContext* pContext = new (pMemory) CVoiceContext();
            // Original bug kept: the failures below leak the context (and its event).
            if (m_voiceOutPutSampleRate == 0)
            {
                keen::debug::nativePrintArgs("[gamelink] m_voiceOutPutSampleRate = 0. m_voiceOutPutSampleRate is not correctly set. \n");
                return nullptr;
            }
            UINT32 deviceCount = 0;
            const HRESULT result = m_pIXaudio2->GetDeviceCount(&deviceCount);
            if (result != S_OK)
            {
                keen::debug::nativePrintArgs("[gamelink] m_pIXaudio2->GetDeviceCount failed! Result:%d \n", result);
                return nullptr;
            }
            if (deviceCount == 0)
            {
                keen::debug::nativePrintArgs("[gamelink] Device count == 0! \n");
                return nullptr;
            }
            pContext->create(m_pAllocator);
            WAVEFORMATEX format;
            format.wFormatTag = WAVE_FORMAT_PCM;
            format.nChannels = 1;
            format.nSamplesPerSec = m_voiceOutPutSampleRate;
            format.nAvgBytesPerSec = m_voiceOutPutSampleRate * 2;
            format.nBlockAlign = 2;
            format.wBitsPerSample = 16;
            format.cbSize = 0;
            // max frequency ratio 1: the voice is never pitched
            if (FAILED(m_pIXaudio2->CreateSourceVoice(&pContext->m_pSourceVoice, &format, 0, 1.0f, pContext, nullptr, nullptr)))
            {
                // Original bug kept: freed without the destructor (the event leaks).
                m_pAllocator->free(pContext);
                return nullptr;
            }
            pContext->m_pSourceVoice->SetVolume(m_voicechat_volume, XAUDIO2_COMMIT_NOW);
            pContext->m_pSourceVoice->Start(0, XAUDIO2_COMMIT_NOW);
            return pContext;
        }

        // 0x00CD3290 (:2907)
        CVoiceContext* GameLinkInternet::getVoiceContext(CSteamID steamID)
        {
            for (uint32 i = 0; i < m_voiceChatDataArray.getSize(); ++i)
            {
                if (m_voiceChatDataArray.getStart()[i].steamID == steamID)
                {
                    return m_voiceChatDataArray.getStart()[i].pVoiceContext;
                }
            }
            return nullptr;
        }

        // 0x00CD5460 (:2921): a voice whose member is gone is destroyed once its queued buffers have played (every
        // voice is marked as gone, then the members still there are unmarked); new members get a voice
        void GameLinkInternet::updateSourceVoiceArray(keen::SizedArray<GameLinkRemoteStationSteam>& membersList, uint8 numLobbyMembers)
        {
            for (GameLinkSteamVoiceChatData* it = m_voiceChatDataArray.getStart(); it < m_voiceChatDataArray.getEnd(); ++it)
            {
                XAUDIO2_VOICE_STATE state;
                it->pVoiceContext->m_pSourceVoice->GetState(&state);
                if (it->hasLeft && state.BuffersQueued == 0)
                {
                    it->pVoiceContext->m_pSourceVoice->Stop(0, XAUDIO2_COMMIT_NOW);
                    it->pVoiceContext->m_pSourceVoice->DestroyVoice();
                    keen::deleteObject(it->pVoiceContext, m_pAllocator);
                    // Original: the element moved into this slot is skipped until the next update.
                    if (m_voiceChatDataArray.getSize() != 0)
                    {
                        m_voiceChatDataArray.erase(it);
                    }
                }
                else
                {
                    it->hasLeft = true;
                }
            }
            for (uint32 i = 0; i < numLobbyMembers; ++i)
            {
                const CSteamID memberId = getSteamId(membersList.getStart()[i]);
                if (memberId == SteamUser()->GetSteamID())
                {
                    continue;
                }
                bool found = false;
                for (uint32 j = 0; j < m_voiceChatDataArray.getSize(); ++j)
                {
                    if (m_voiceChatDataArray.getStart()[j].steamID == memberId)
                    {
                        m_voiceChatDataArray.getStart()[j].hasLeft = false;
                        found = true;
                    }
                }
                if (found || m_voiceChatDataArray.isFull())
                {
                    continue;
                }
                CVoiceContext* pVoiceContext = createVoiceContext();
                if (pVoiceContext == nullptr)
                {
                    keen::debug::nativePrintArgs("[gamelink] failed to create Voice Context \n");
                    continue;
                }
                m_voiceChatDataArray.pushBack(GameLinkSteamVoiceChatData(memberId, pVoiceContext));
            }
        }
    }
}
