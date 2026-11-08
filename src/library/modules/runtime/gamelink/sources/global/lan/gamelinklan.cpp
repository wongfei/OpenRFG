// Original: library/modules/runtime/gamelink/sources/global/lan/gamelinklan.cpp
//
// GameLinkLan: lobbies on the local network over one non-blocking UDP socket bound to the matchmaking port
// (GameLinkSessionParameters::Lan::portMatchmakingBroadcast; rfg.exe: 1005).
// - Search: a client broadcasts lobby info requests to 255.255.255.255:port; each host answers the sender with a lobby
//   info response; the senders become the search results (the lobby id is the host's address).
// - Join: the client sends join requests to the lobby address until a join response arrives; the host adds the sender
//   as a lobby member (the member's station id is its network address, RemoteUserId::providerData = NetworkAddress).
// - In the lobby: the host sends each member whose last acknowledged lobby state id is behind a lobby state update
//   (the member list), and keep-alives to all; members answer with keep-alives carrying the state id they have. A
//   member silent for timeoutLobbyMember (and in the lobby for more than 3 s) is dropped; a client that loses the host
//   leaves (or migrates the host when enabled).
// - User-defined packets (the game's) carry a 12-byte PacketHeader and are queued for getNextP2PPacket.
// The lobby packets (types 2-7) from the host's own IP are ignored, so two instances on one machine cannot see each
// other as shipped; see the localhost deviation below (multiplayer.md 2.3).
//
// The inline functions of gamelinklan.hpp (the packet header initializers) are defined here too.

#include "library/modules/runtime/gamelink/sources/global/lan/gamelinklan.hpp"
#include "keen/base/string.hpp"
#include "keen/base/systemtimer.hpp"
#include "keen/base/trace.hpp"
#include "keen/network/posixsocket.hpp"
#include "library/modules/runtime/network/sources/global/networkplatform.hpp"

#include <malloc.h>
#include <stdio.h>
#include <string.h>
#include <winsock2.h> // the own address deviation (openrfgGetLanOwnHostAddress)

namespace kaiko
{
    namespace gamelink
    {
        // OpenRFG deviation (multiplayer.md 2.3, new code): the local LAN port. 0 keeps the original behaviour (the
        // socket is bound to the matchmaking port, lobby packets from the host's own IP are ignored). Any other value
        // (the programs' --lan-port option) binds the socket to it, uses it in the host's own station address, and
        // compares IP and port in the own-address checks, so several instances on one machine see each other. The
        // search broadcasts still go to the matchmaking port, where a host listens.
        uint16 g_openrfgLanLocalPort = 0;

        // The port this link's socket is bound to (the matchmaking port unless the deviation is on).
        static uint16 getLocalPort(const GameLinkSessionParameters& params)
        {
            return g_openrfgLanLocalPort != 0 ? g_openrfgLanLocalPort : params.lan.portMatchmakingBroadcast;
        }

        // True when the address is this instance's own: the original compares the IP only.
        static bool isOwnAddress(const keen::NetworkAddress& address, const keen::NetworkAddress& hostAddress, const GameLinkSessionParameters& params)
        {
            if (g_openrfgLanLocalPort != 0)
            {
                return address.ip4Address == hostAddress.ip4Address && address.port == getLocalPort(params);
            }
            return address.ip4Address == hostAddress.ip4Address;
        }

        // The host's own address. The original takes the first address of the local host (getHostNetworkAddress("")),
        // which on a machine with several adapters (virtual ones, VPNs) need not be the address the other peers see as
        // the sender of this link's packets: a client then finds the lobby under one address and gets the lobby's
        // member list under another, and leaves the lobby again (multi_session_base::process_state, "lobby changed").
        // OpenRFG deviation (part of the localhost one above, on with it): the source address of the default route
        // (a UDP socket connected to a TEST-NET address; nothing is sent), the address of the packets that leave
        // this machine; the original choice when there is no such route.
        bool openrfgGetLanOwnHostAddress(keen::NetworkAddress* pTarget)
        {
            if (!keen::network::getHostNetworkAddress(pTarget, ""))
            {
                return false;
            }
            // The route is looked up once (handlePacket asks for every packet).
            static uint32 s_routeAddress = 0;
            if (g_openrfgLanLocalPort != 0 && s_routeAddress != 0)
            {
                pTarget->ip4Address = s_routeAddress;
            }
            else if (g_openrfgLanLocalPort != 0)
            {
                const SOCKET probe = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
                if (probe != INVALID_SOCKET)
                {
                    sockaddr_in target = {};
                    target.sin_family = AF_INET;
                    target.sin_port = htons(9);
                    // 192.0.2.1 (TEST-NET-1, RFC 5737)
                    target.sin_addr.s_addr = htonl(0xC0000201);
                    sockaddr_in local = {};
                    int length = sizeof(local);
                    if (connect(probe, (const sockaddr*)&target, sizeof(target)) == 0 && getsockname(probe, (sockaddr*)&local, &length) == 0 && local.sin_addr.s_addr != 0)
                    {
                        s_routeAddress = local.sin_addr.s_addr;
                        pTarget->ip4Address = s_routeAddress;
                    }
                    closesocket(probe);
                }
            }
            return true;
        }

        // 0x00CD0820 (:26)
        void getIpAndPortString(char* pBuffer, uint32 bufferSize, keen::NetworkAddress networkAddress)
        {
            const uint32 ip = networkAddress.ip4Address;
            sprintf_s(pBuffer, bufferSize, "%u.%u.%u.%u:%u", ip & 0xFF, (ip >> 8) & 0xFF, (ip >> 16) & 0xFF, ip >> 24, (uint32)networkAddress.port);
        }

        // 0x00CD0860 (:32): a LAN station's id is its network address
        keen::NetworkAddress getNetworkAddress(const GameLinkRemoteStation* pRemoteStation)
        {
            return *(const keen::NetworkAddress*)pRemoteStation->id.providerData;
        }

        // 0x00CD0870 (:38)
        keen::NetworkAddress getNetworkAddress(const GameLinkLobbyId& lobbyId)
        {
            keen::NetworkAddress address;
            address.ip4Address = lobbyId.asNetworkAddress.ipAddress.asUint32;
            address.port = lobbyId.asNetworkAddress.port;
            address.pad0 = 0; // uninitialized in the original (a stack word)
            return address;
        }

        // 0x00CD0260 (.h:69)
        void PacketHeaderLobbyInfoRequest::initialize(uint32 a_uid)
        {
            uid = a_uid;
            size = sizeof(PacketHeaderLobbyInfoRequest);
            type = PacketType_LobbyInfoRequest;
        }

        // 0x00CD0280 (.h:90)
        void PacketHeaderLobbyInfoResponse::initialize(uint32 a_uid, uint8 a_currentNumPlayers, uint8 a_maxNumPlayers, GameLinkLobbyType a_lobbyType, uint32* a_pLobbyFilters, const char* a_pLobbyName)
        {
            uid = a_uid;
            currentNumPlayers = a_currentNumPlayers;
            maxNumPlayers = a_maxNumPlayers;
            lobbyType = a_lobbyType;
            size = sizeof(PacketHeaderLobbyInfoResponse);
            type = PacketType_LobbyInfoResponse;
            for (uint32 i = 0; i < 8; i++)
            {
                lobbyFilters[i] = a_pLobbyFilters[i];
            }
            keen::copyString(lobbyName, sizeof(lobbyName), a_pLobbyName);
        }

        // 0x00CD0300 (.h:122): the size is left 0, the sender adds the member list and sets it
        void PacketHeaderLobbyStateUpdate::initialize(uint32 a_uid, uint32 a_lobbyStateId, GameLinkLobbyType a_lobbyType, bool a_lobbyLocked, uint32* a_pLobbyFilters, uint32 a_numLobbyMembers, uint32 a_maxNumLobbyMembers)
        {
            uid = a_uid;
            lobbyStateId = a_lobbyStateId;
            lobbyType = a_lobbyType;
            lobbyLocked = a_lobbyLocked;
            size = 0;
            type = PacketType_LobbyStateUpdate;
            for (uint32 i = 0; i < 8; i++)
            {
                lobbyFilters[i] = a_pLobbyFilters[i];
            }
            numLobbyMembers = a_numLobbyMembers;
            maxNumLobbyMembers = a_maxNumLobbyMembers;
        }

        // 0x00CD0370 (.h:144)
        void PacketHeaderJoinRequest::initialize(uint32 a_uid, const char* a_pUserName)
        {
            uid = a_uid;
            size = sizeof(PacketHeaderJoinRequest);
            type = PacketType_JoinRequest;
            keen::copyString(userName, sizeof(userName), a_pUserName);
        }

        // 0x00CD03A0 (.h:167)
        void PacketHeaderJoinResponse::initialize(uint32 a_uid, JoinResponseResult a_result)
        {
            uid = a_uid;
            size = sizeof(PacketHeaderJoinResponse);
            type = PacketType_JoinResponse;
            result = a_result;
        }

        // 0x00CD03C0 (.h:181)
        void PacketHeaderKeepAlive::initialize(uint32 a_uid, uint32 a_lobbyStateId)
        {
            uid = a_uid;
            size = sizeof(PacketHeaderKeepAlive);
            type = PacketType_KeepAlive;
            lobbyStateId = a_lobbyStateId;
        }

        // 0x00CD5680 (:46)
        GameLinkLan::GameLinkLan()
        {
        }

        // 0x00CD5720 (:50)
        GameLinkLan::~GameLinkLan()
        {
        }

        // 0x00CD6DA0 (:54)
        void GameLinkLan::create(keen::MemoryAllocator* pAllocator, const GameLinkLanCreationParameters& params, const GameLinkGeneralCreationParameters& paramsGeneral)
        {
            m_internalState = InternalState_Offline;
            m_state = GameLinkState_Offline;
            m_error = GameLinkError_None;
            m_lobbyMembers.create(pAllocator, paramsGeneral.maxNumLobbyMembers, 16);
            m_lobbyFilters.create(pAllocator, 8, 16); // magic number: 8 lobby filter slots (the packets carry 8)
            m_lobbySearchFilterValues.create(pAllocator, 8, 16);
            m_lobbySearchResults.create(pAllocator, paramsGeneral.maxNumMatchmakingResults, 16);
            m_userDefinedPackets.create(pAllocator, params.maxNumStoredUserDefinedPackets, 16);
            keen::copyString(m_localUserName, sizeof(m_localUserName), params.localUserName);
            m_random.initFromSeed(keen::SystemTimer::getCurrentMilliseconds());
            m_findAndJoinData.timeout = paramsGeneral.findAndJoinTimeout;
        }

        // 0x00CD6EC0 (:76)
        void GameLinkLan::destroy(keen::MemoryAllocator* pAllocator)
        {
            m_lobbyMembers.destroy(pAllocator);
            m_lobbyFilters.destroy(pAllocator);
            m_lobbySearchFilterValues.destroy(pAllocator);
            m_lobbySearchResults.destroy(pAllocator);
            m_userDefinedPackets.destroy(pAllocator);
        }

        // 0x00CD3360 (:85): flags 9 = non-blocking + broadcast; bound to any address on the matchmaking port. A second
        // instance on the same machine fails here (the port is taken) unless the localhost deviation gives it a port.
        bool GameLinkLan::beginSession(const GameLinkSessionParameters& params)
        {
            if (m_state != GameLinkState_Offline)
            {
                return false;
            }
            m_params = params;
            m_socketBroadcast = keen::network::createSocket(keen::NetworkSocketType_UDP, 9);
            if (m_socketBroadcast == 0)
            {
                return false;
            }
            keen::NetworkAddress bindAddress;
            bindAddress.ip4Address = 0;
            bindAddress.port = getLocalPort(m_params); // deviation: the local port (original: portMatchmakingBroadcast)
            bindAddress.pad0 = 0;
            if (!keen::network::bind(m_socketBroadcast, bindAddress))
            {
                // original behaviour: the socket is not destroyed
                return false;
            }
            m_state = GameLinkState_Online;
            m_internalState = InternalState_Online;
            m_host = false;
            m_timeLobbyInfoRequestSent = 0;
            m_timeLobbyStateSent = 0;
            m_timeJoinRequestSent = 0;
            m_timeKeepAliveSent = 0;
            m_searchRunning = false;
            m_lobbySearchResults.clear();
            for (uint32 i = 0; i < m_lobbySearchFilterValues.getSize(); i++)
            {
                m_lobbySearchFilterValues[i] = 0;
            }
            m_maxNumSearchResults = 0;
            m_timeLobbySearchStarted = 0;
            m_lobbyStateId = 0;
            m_lobbyMembers.clear();
            for (uint32 i = 0; i < m_lobbyFilters.getSize(); i++)
            {
                m_lobbyFilters[i] = 0;
            }
            m_maxNumLobbyMembers = 0;
            m_lobbyType = GameLinkLobbyType_None;
            m_lobbyLocked = false;
            m_targetLobby.id.asUint64 = 0;
            m_timeJoinRequestStarted = 0;
            m_timeLobbyJoined = 0;
            m_receivedFirstLobbyStateUpdate = false;
            m_userDefinedPackets.clear();
            return true;
        }

        // 0x00CD0890 (:156)
        void GameLinkLan::endSession()
        {
            if (m_state != GameLinkState_Offline)
            {
                m_state = GameLinkState_Offline;
                keen::network::destroySocket(m_socketBroadcast);
                m_internalState = InternalState_Offline;
            }
        }

        // 0x00CD5730 (:170): searches (10 results); update joins a random result or creates a public lobby
        bool GameLinkLan::findAndJoinLobby(uint32 maxNumLobbyMembers, const uint32* pLobbyFilters, uint32 numLobbyFilters)
        {
            if (m_state != GameLinkState_Online || m_findAndJoinData.status == GameLinkFindAndJoinStatus_Running)
            {
                return false;
            }
            m_findAndJoinData.maxNumLobbyMembers = maxNumLobbyMembers;
            memset(m_lobbyFilters.getStart(), 0, m_lobbyFilters.getSize() * sizeof(uint32));
            for (uint32 i = 0; i < numLobbyFilters; i++)
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

        // 0x00CD08C0 (:207)
        GameLinkFindAndJoinStatus GameLinkLan::getFindAndJoinStatus() const
        {
            return m_findAndJoinData.status;
        }

        // 0x00CD08D0 (:212)
        void GameLinkLan::cancelFindAndJoinLobby()
        {
            if (m_findAndJoinData.status == GameLinkFindAndJoinStatus_Running)
            {
                m_findAndJoinData.status = GameLinkFindAndJoinStatus_None;
                m_state = GameLinkState_Online;
            }
        }

        // 0x00CD08F0 (:223): no parties on LAN
        void GameLinkLan::setPartyEnabled(bool)
        {
        }

        // 0x00CD0900 (:228)
        bool GameLinkLan::isPartyEnabled() const
        {
            return false;
        }

        // 0x00CD0910 (:233)
        bool GameLinkLan::isPartyOwner() const
        {
            return false;
        }

        // 0x00CD0920 (:238)
        bool GameLinkLan::getPartyOwner(unit4::NamedRemoteUserId*) const
        {
            return false;
        }

        // 0x00CD0930 (:245)
        uint32 GameLinkLan::getPartyMembers(unit4::NamedRemoteUserId*, uint32) const
        {
            return 0;
        }

        // 0x00CD0940 (:253)
        uint32 GameLinkLan::getNumPartyMembers() const
        {
            return 0;
        }

        // 0x00CD8700 (:258)
        void GameLinkLan::update(float)
        {
            if (m_internalState == InternalState_JoiningLobby)
            {
                if (keen::SystemTimer::getCurrentMilliseconds() - m_timeJoinRequestSent > m_params.lan.sendJoinRequestDeltaTime)
                {
                    PacketHeaderJoinRequest joinRequest;
                    joinRequest.initialize(m_params.lan.gameUniqueIdentifier, m_localUserName);
                    const keen::NetworkAddress target = getNetworkAddress(m_targetLobby.id);
                    uint32 sent;
                    if (keen::network::sendDataTo(m_socketBroadcast, &sent, &joinRequest, joinRequest.size, target) != 0 || sent == 0)
                    {
                        keen::debug::nativePrintArgs("[gamelink] failed to send joinRequest packet\n");
                    }
                    m_timeJoinRequestSent = keen::SystemTimer::getCurrentMilliseconds();
                }
                // original behaviour: the join times out after timeoutLobbyMember (timeoutJoin is not read)
                if (keen::SystemTimer::getCurrentMilliseconds() - m_timeJoinRequestStarted > m_params.lan.timeoutLobbyMember)
                {
                    setError(GameLinkError_JoinLobbyFailed_Timeout, GameLinkLanAdditionalPlatformErrorCodes_JoinRequest_Failed_Timeout);
                    m_state = GameLinkState_Online;
                    m_internalState = InternalState_Online;
                    m_targetLobby.id.asUint64 = 0;
                }
            }
            else if (m_internalState == InternalState_InLobby)
            {
                if (!m_host)
                {
                    if (!m_receivedFirstLobbyStateUpdate)
                    {
                        if (keen::SystemTimer::getCurrentMilliseconds() - m_timeLobbyJoined > m_params.lan.timeoutReceiveFirstLobbyStateUpdate)
                        {
                            setError(GameLinkError_SessionLost, GameLinkLanAdditionalPlatformErrorCodes_ReceiveFirstLobbyStateUpdate_Failed_Timeout);
                            leaveLobby();
                        }
                    }
                    else
                    {
                        const GameLinkRemoteStationLan* pHost = static_cast<const GameLinkRemoteStationLan*>(getLobbyOwner());
                        bool hostAlive = false;
                        if (pHost != nullptr)
                        {
                            if (keen::SystemTimer::getCurrentMilliseconds() - m_timeKeepAliveSent > m_params.lan.sendKeepAliveDeltaTime)
                            {
                                PacketHeaderKeepAlive keepAlive;
                                keepAlive.initialize(m_params.lan.gameUniqueIdentifier, m_lobbyStateId);
                                if (!sendPacketToHost(&keepAlive))
                                {
                                    keen::debug::nativePrintArgs("[gamelink] update: InLobby: failed to send keepAlive packet\n");
                                }
                                m_timeKeepAliveSent = keen::SystemTimer::getCurrentMilliseconds();
                            }
                            const uint32 now = keen::SystemTimer::getCurrentMilliseconds();
                            hostAlive = now - pHost->timeLastKeepAliveReceived <= m_params.lan.timeoutLobbyMember || now - pHost->timeAdded <= GameLinkLanConstants_StartCheckTimeoutDelay;
                        }
                        if (!hostAlive)
                        {
                            startHostMigration();
                        }
                    }
                }
                else
                {
                    if (keen::SystemTimer::getCurrentMilliseconds() - m_timeLobbyStateSent > m_params.lan.sendLobbyStateDeltaTime)
                    {
                        for (uint32 i = 0; i < m_lobbyMembers.getSize(); i++)
                        {
                            const GameLinkRemoteStationLan& member = m_lobbyMembers[i];
                            if (member.lastReceivedLobbyStateId == m_lobbyStateId || member.myStation)
                            {
                                continue;
                            }
                            // The state update and the member list (each member as a GameLinkRemoteStation). Original
                            // limitation: with more than 12 members the packet is larger than the receivers' 1024-byte
                            // buffer (GameLinkLanConstants_ReceiveBufferSize) and does not arrive.
                            const uint32 numMembers = m_lobbyMembers.getSize();
                            const uint32 packetSize = sizeof(PacketHeaderLobbyStateUpdate) + numMembers * sizeof(GameLinkRemoteStation);
                            PacketHeaderLobbyStateUpdate* pUpdate = (PacketHeaderLobbyStateUpdate*)_alloca(packetSize);
                            pUpdate->initialize(m_params.lan.gameUniqueIdentifier, m_lobbyStateId, m_lobbyType, m_lobbyLocked, m_lobbyFilters.getStart(), numMembers, m_maxNumLobbyMembers);
                            pUpdate->size = packetSize;
                            uint8* pMemberData = (uint8*)(pUpdate + 1);
                            for (uint32 j = 0; j < numMembers; j++)
                            {
                                memcpy(pMemberData, static_cast<const GameLinkRemoteStation*>(&m_lobbyMembers[j]), sizeof(GameLinkRemoteStation));
                                pMemberData += sizeof(GameLinkRemoteStation);
                            }
                            uint32 sent;
                            if (keen::network::sendDataTo(m_socketBroadcast, &sent, pUpdate, pUpdate->size, getNetworkAddress(&member)) != 0 || sent == 0)
                            {
                                keen::debug::nativePrintArgs("[gamelink] failed to send lobbyStateUpdate packet\n");
                            }
                        }
                        m_timeLobbyStateSent = keen::SystemTimer::getCurrentMilliseconds();
                    }
                    if (keen::SystemTimer::getCurrentMilliseconds() - m_timeKeepAliveSent > m_params.lan.sendKeepAliveDeltaTime)
                    {
                        for (uint32 i = 0; i < m_lobbyMembers.getSize(); i++)
                        {
                            const GameLinkRemoteStationLan& member = m_lobbyMembers[i];
                            if (member.myStation)
                            {
                                continue;
                            }
                            PacketHeaderKeepAlive keepAlive;
                            keepAlive.initialize(m_params.lan.gameUniqueIdentifier, m_lobbyStateId);
                            uint32 sent;
                            if (keen::network::sendDataTo(m_socketBroadcast, &sent, &keepAlive, sizeof(keepAlive), getNetworkAddress(&member)) != 0 || sent == 0)
                            {
                                keen::debug::nativePrintArgs("[gamelink] update: failed to send keepAlive packet\n");
                            }
                        }
                        m_timeKeepAliveSent = keen::SystemTimer::getCurrentMilliseconds();
                    }
                    // drop the members that went silent (and were added more than 3 s ago)
                    const uint32 now = keen::SystemTimer::getCurrentMilliseconds();
                    bool removed = false;
                    for (uint32 i = 0; i < m_lobbyMembers.getSize(); i++)
                    {
                        GameLinkRemoteStationLan* pMember = &m_lobbyMembers[i];
                        if (!pMember->myStation && now - pMember->timeLastKeepAliveReceived > m_params.lan.timeoutLobbyMember && now - pMember->timeAdded > GameLinkLanConstants_StartCheckTimeoutDelay)
                        {
                            GameLinkRemoteStationLan* pLast = m_lobbyMembers.getStart() + (m_lobbyMembers.getSize() - 1);
                            if (pMember < pLast)
                            {
                                memmove(pMember, pMember + 1, (uint8*)(pLast + 1) - (uint8*)(pMember + 1));
                            }
                            m_lobbyMembers.popBack();
                            i--;
                            removed = true;
                        }
                    }
                    if (removed)
                    {
                        m_lobbyStateId++;
                    }
                }
            }

            if (m_internalState > InternalState_Offline)
            {
                uint8 buffer[GameLinkLanConstants_ReceiveBufferSize];
                uint32 received;
                keen::NetworkAddress sender;
                while (keen::network::receiveDataFrom(m_socketBroadcast, &received, buffer, sizeof(buffer), &sender) == 0 && received != 0)
                {
                    handlePacket(buffer, received, sender);
                }
                if (m_searchRunning)
                {
                    // Original behaviour: m_timeLobbyInfoRequestSent is not updated here (only beginSession and
                    // createLobby reset it), so once the first interval passed a search broadcasts every update.
                    if (keen::SystemTimer::getCurrentMilliseconds() - m_timeLobbyInfoRequestSent > m_params.lan.sendLobbyInfoRequestDeltaTime)
                    {
                        PacketHeaderLobbyInfoRequest request;
                        request.initialize(m_params.lan.gameUniqueIdentifier);
                        keen::NetworkAddress broadcast;
                        broadcast.ip4Address = 0xFFFFFFFFu; // 255.255.255.255
                        broadcast.port = m_params.lan.portMatchmakingBroadcast;
                        broadcast.pad0 = 0;
                        uint32 sent;
                        if (keen::network::sendDataTo(m_socketBroadcast, &sent, &request, sizeof(request), broadcast) != 0 || sent == 0)
                        {
                            keen::debug::nativePrintArgs("[gamelink] update: failed to send LobbyInfoRequest broadcast packet\n");
                        }
                    }
                    if (keen::SystemTimer::getCurrentMilliseconds() - m_timeLobbySearchStarted > m_params.lan.searchLobbyDuration)
                    {
                        m_searchRunning = false;
                    }
                }
                if (m_findAndJoinData.status == GameLinkFindAndJoinStatus_Running)
                {
                    if (m_internalState == InternalState_InLobby)
                    {
                        m_findAndJoinData.status = GameLinkFindAndJoinStatus_FoundAndJoinedLobby;
                    }
                    else if (m_internalState == InternalState_Online)
                    {
                        GameLinkSearchResult results[GameLinkConstants_FindAndJoinMaxSearchResults];
                        bool searchRunning;
                        // Original bug: the capacity passed is the buffer's size in bytes (800), not 10 results; the
                        // search keeps at most maxNumMatchmakingResults, so it overflows only with more than 10.
                        const uint32 numResults = getSearchResults(results, sizeof(results), &searchRunning);
                        if (!searchRunning)
                        {
                            bool ok;
                            if (numResults == 0)
                            {
                                ok = createLobby(GameLinkLobbyType_Public, m_findAndJoinData.maxNumLobbyMembers, m_lobbyFilters.getStart(), m_lobbyFilters.getSize());
                            }
                            else
                            {
                                ok = joinLobby(results[m_random.getRandomValue(numResults)].lobby);
                            }
                            if (!ok)
                            {
                                m_findAndJoinData.status = GameLinkFindAndJoinStatus_Failed;
                            }
                        }
                    }
                    if (keen::SystemTimer::getCurrentMilliseconds() - m_findAndJoinData.timeStarted > m_findAndJoinData.timeout)
                    {
                        m_findAndJoinData.status = GameLinkFindAndJoinStatus_Failed;
                    }
                }
            }
        }

        // 0x00CD6FA0 (:507): the host is the first member (its own station: the host address on the local port)
        bool GameLinkLan::createLobby(GameLinkLobbyType lobbyType, uint32 maxNumLobbyMembers, const uint32* pLobbyFilters, uint32 numLobbyFilters)
        {
            if (m_state == GameLinkState_Offline)
            {
                return false;
            }
            if (m_findAndJoinData.status != GameLinkFindAndJoinStatus_Running)
            {
                leaveLobby();
            }
            m_maxNumLobbyMembers = maxNumLobbyMembers;
            m_lobbyType = lobbyType;
            m_host = true;
            uint32 i = 0;
            for (; i < numLobbyFilters; i++)
            {
                m_lobbyFilters[i] = pLobbyFilters[i];
            }
            for (; i < m_lobbyFilters.getSize(); i++)
            {
                m_lobbyFilters[i] = 0;
            }
            m_lobbyMembers.clear();
            keen::NetworkAddress hostAddress;
            hostAddress.ip4Address = 0;
            hostAddress.port = 0;
            hostAddress.pad0 = 0;
            if (!openrfgGetLanOwnHostAddress(&hostAddress)) // deviation: see openrfgGetLanOwnHostAddress
            {
                return false;
            }
            hostAddress.port = getLocalPort(m_params); // deviation: the local port (original: portMatchmakingBroadcast)
            GameLinkRemoteStationLan& station = m_lobbyMembers.pushBack();
            *(keen::NetworkAddress*)station.id.providerData = hostAddress;
            station.id.guestIndex = 0;
            station.id.isValid = true;
            station.myStation = true;
            station.lobbyOwner = true;
            keen::copyString(station.userName, sizeof(station.userName), m_localUserName);
            station.lastReceivedLobbyStateId = 0;
            station.timeLastKeepAliveReceived = 0;
            station.timeAdded = keen::SystemTimer::getCurrentMilliseconds();
            m_lobbyStateId = 1;
            m_timeLobbyInfoRequestSent = 0;
            m_timeLobbyStateSent = 0;
            m_state = GameLinkState_InLobby;
            m_internalState = InternalState_InLobby;
            return true;
        }

        // 0x00CD34D0 (:556)
        bool GameLinkLan::startSearchLobby(uint32 maxNumResults, const uint32* pLobbyFilters, uint32 numLobbyFilters)
        {
            if (m_state == GameLinkState_Offline)
            {
                return false;
            }
            m_maxNumSearchResults = maxNumResults;
            m_searchRunning = true;
            m_lobbySearchResults.clear();
            uint32 i = 0;
            for (; i < numLobbyFilters; i++)
            {
                m_lobbySearchFilterValues[i] = pLobbyFilters[i];
            }
            for (; i < 8; i++)
            {
                m_lobbySearchFilterValues[i] = 0;
            }
            m_timeLobbySearchStarted = keen::SystemTimer::getCurrentMilliseconds();
            return true;
        }

        // 0x00CD3560 (:588): the results only once the search ended. Returns the number of results found, which can
        // be more than were copied (original behaviour).
        uint32 GameLinkLan::getSearchResults(GameLinkSearchResult* pSearchResults, uint32 capacity, bool* pSearchRunning) const
        {
            *pSearchRunning = m_searchRunning;
            if (m_searchRunning)
            {
                *pSearchRunning = true;
                return 0;
            }
            const uint32 count = m_lobbySearchResults.getSize() < capacity ? m_lobbySearchResults.getSize() : capacity;
            for (uint32 i = 0; i < count; i++)
            {
                pSearchResults[i] = m_lobbySearchResults[i];
            }
            *pSearchRunning = false;
            return m_lobbySearchResults.getSize();
        }

        // 0x00CD0950 (:610): starts the join requests (update); the lobby id is the host's address
        bool GameLinkLan::joinLobby(const GameLinkLobby& lobby)
        {
            if (m_state == GameLinkState_Offline)
            {
                return false;
            }
            if (lobby.is_equal(m_targetLobby) || lobby.is_equal(getLobbyId()))
            {
                return false;
            }
            if (m_state == GameLinkState_InLobby && m_findAndJoinData.status != GameLinkFindAndJoinStatus_Running)
            {
                leaveLobby();
            }
            m_state = GameLinkState_InLobby;
            m_internalState = InternalState_JoiningLobby;
            m_targetLobby = lobby;
            m_host = false;
            m_timeJoinRequestStarted = keen::SystemTimer::getCurrentMilliseconds();
            m_receivedFirstLobbyStateUpdate = false;
            return true;
        }

        // 0x00CD0A00 (:645)
        bool GameLinkLan::joinLobby(const GameLinkInvitationData*)
        {
            keen::debug::nativePrintArgs("[gamelink] Joining a lobby from an invitation is not supported by GameLinkLan\n");
            return false;
        }

        // 0x00CD57E0 (:654): nothing is sent; the host drops this member when its keep-alives stop
        void GameLinkLan::leaveLobby()
        {
            if (m_state == GameLinkState_InLobby)
            {
                m_state = GameLinkState_Online;
                m_internalState = InternalState_Online;
                m_lobbyStateId = 0;
                m_lobbyMembers.clear();
                memset(m_lobbyFilters.getStart(), 0, m_lobbyFilters.getSize() * sizeof(uint32));
                m_lobbyType = GameLinkLobbyType_None;
                m_lobbyLocked = false;
                m_maxNumLobbyMembers = 0;
                m_host = false;
                m_receivedFirstLobbyStateUpdate = false;
            }
        }

        // 0x00CD0A20 (:675)
        void GameLinkLan::openInvitationDialog()
        {
            keen::debug::nativePrintArgs("[gamelink] Sending invitations is only supported by GameLinkInternet\n");
        }

        // 0x00CD35D0 (:680): original behaviour: always answers false, also when the filters were set
        bool GameLinkLan::setLobbyFilters(const uint32* pIndices, const uint32* pValues, uint32 numLobbyFilters)
        {
            if (isLobbyOwner() && numLobbyFilters <= m_lobbyFilters.getSize())
            {
                for (uint32 i = 0; i < numLobbyFilters; i++)
                {
                    m_lobbyFilters[pIndices[i]] = pValues[i];
                }
                m_lobbyStateId++;
            }
            return false;
        }

        // 0x00CD3630 (:702)
        uint32 GameLinkLan::getLobbyFilters(uint32* pLobbyFilters, uint32 capacity) const
        {
            if (m_state == GameLinkState_Offline || m_lobbyFilters.getSize() == 0)
            {
                return 0;
            }
            const uint32 count = capacity < m_lobbyFilters.getSize() ? capacity : m_lobbyFilters.getSize();
            if (count != 0)
            {
                memmove(pLobbyFilters, m_lobbyFilters.getStart(), count * sizeof(uint32));
            }
            return count;
        }

        // 0x00CD36C0 (:725)
        uint32 GameLinkLan::getLobbyMembers(const GameLinkRemoteStation** pRemoteStations, uint32 capacity) const
        {
            if (m_state != GameLinkState_InLobby || m_lobbyMembers.getSize() == 0)
            {
                return 0;
            }
            const uint32 count = capacity < m_lobbyMembers.getSize() ? capacity : m_lobbyMembers.getSize();
            for (uint32 i = 0; i < count; i++)
            {
                pRemoteStations[i] = &m_lobbyMembers[i];
            }
            return count;
        }

        // 0x00CD0A30 (:746)
        bool GameLinkLan::isLobbyOwner() const
        {
            if (m_state != GameLinkState_InLobby)
            {
                return false;
            }
            return m_host;
        }

        // 0x00CD3710 (:756): the payload behind a user-defined PacketHeader, sent to the station's address. A station
        // without an address (0.0.0.0:0) counts as sent (original behaviour).
        bool GameLinkLan::sendP2PPacketUnreliable(const GameLinkRemoteStation* pStation, const void* pPacket, uint32 packetSize)
        {
            if (m_state != GameLinkState_InLobby)
            {
                return false;
            }
            const keen::NetworkAddress target = getNetworkAddress(pStation);
            if (target.ip4Address == 0 && target.port == 0)
            {
                return true;
            }
            const uint32 totalSize = packetSize + sizeof(PacketHeader);
            PacketHeader* pHeader = (PacketHeader*)_alloca(totalSize);
            pHeader->uid = m_params.lan.gameUniqueIdentifier;
            pHeader->size = totalSize;
            pHeader->type = PacketType_UserDefined;
            memmove(pHeader + 1, pPacket, packetSize);
            uint32 sent;
            return keen::network::sendDataTo(m_socketBroadcast, &sent, pHeader, pHeader->size, target) == 0 && sent != 0;
        }

        // 0x00CD70E0 (:783): the oldest queued user-defined packet; the sender is the member with its address (null when
        // it is not a member). The copy is not limited to bufferSize (original behaviour).
        uint32 GameLinkLan::getNextP2PPacket(void* pBuffer, uint32, const GameLinkRemoteStation** pReturnSender)
        {
            *pReturnSender = nullptr;
            if (m_state != GameLinkState_InLobby || m_userDefinedPackets.getSize() == 0)
            {
                return 0;
            }
            const UserDefinedPacket& packet = m_userDefinedPackets.pop();
            memmove(pBuffer, packet.data, packet.size);
            *pReturnSender = findLobbyMember(packet.senderAddress);
            return packet.size;
        }

        // 0x00CD0A40 (:816)
        bool GameLinkLan::setLobbyType(GameLinkLobbyType lobbyType)
        {
            if (!isLobbyOwner())
            {
                return false;
            }
            m_lobbyType = lobbyType;
            m_lobbyStateId++;
            return true;
        }

        // 0x00CD0A70 (:829)
        bool GameLinkLan::setLobbyLocked(bool locked)
        {
            if (!isLobbyOwner())
            {
                return false;
            }
            m_lobbyLocked = locked;
            m_lobbyStateId++;
            return true;
        }

        // 0x00CD0AA0 (:842): not supported on LAN
        bool GameLinkLan::setLobbyOwner(unit4::RemoteUserId)
        {
            return false;
        }

        // 0x00CD0AB0 (:848)
        uint32 GameLinkLan::getLobbyMaxMembers() const
        {
            if (m_state != GameLinkState_InLobby)
            {
                return 0;
            }
            return m_maxNumLobbyMembers;
        }

        // 0x00CD3800 (:858)
        uint32 GameLinkLan::getNumLobbyMembers() const
        {
            if (m_state != GameLinkState_InLobby)
            {
                return 0;
            }
            return m_lobbyMembers.getSize();
        }

        // 0x00CD3810 (:868): the owner's address (the padding cleared); 0 without an owner
        GameLinkLobby GameLinkLan::getLobbyId() const
        {
            GameLinkLobby lobby;
            lobby.id.asUint64 = 0;
            const GameLinkRemoteStation* pOwner = getLobbyOwner();
            if (pOwner != nullptr)
            {
                const keen::NetworkAddress address = getNetworkAddress(pOwner);
                lobby.id.asNetworkAddress.ipAddress.asUint32 = address.ip4Address;
                lobby.id.asNetworkAddress.port = address.port;
                lobby.id.asNetworkAddress.padding = 0;
            }
            return lobby;
        }

        // 0x00CD3860 (:883)
        const GameLinkRemoteStation* GameLinkLan::getLobbyOwner() const
        {
            if (m_state != GameLinkState_InLobby)
            {
                return nullptr;
            }
            for (uint32 i = 0; i < m_lobbyMembers.getSize(); i++)
            {
                if (m_lobbyMembers[i].lobbyOwner)
                {
                    return &m_lobbyMembers[i];
                }
            }
            return nullptr;
        }

        // 0x00CD38A0 (:901): by the provider data only (the address)
        const GameLinkRemoteStation* GameLinkLan::getRemoteStationById(unit4::RemoteUserId id) const
        {
            if (m_state != GameLinkState_InLobby)
            {
                return nullptr;
            }
            for (uint32 i = 0; i < m_lobbyMembers.getSize(); i++)
            {
                if (memcmp(m_lobbyMembers[i].id.providerData, id.providerData, sizeof(id.providerData)) == 0)
                {
                    return &m_lobbyMembers[i];
                }
            }
            return nullptr;
        }

        // 0x00CD0AC0 (:917): no voice chat on LAN
        void GameLinkLan::setVoiceChatEnabled(bool)
        {
        }

        // 0x00CD0AD0 (:922)
        void GameLinkLan::setVoiceChatLocalPlayerMuted(bool)
        {
        }

        // 0x00CD0AE0 (:927)
        void GameLinkLan::setVoiceChatRemotePlayerMuted(const GameLinkRemoteStation&, bool)
        {
        }

        // 0x00CD0AF0 (:933)
        bool GameLinkLan::getVoiceChatLocalPlayerInfo(GameLinkVoiceChatPlayerInfo*)
        {
            return false;
        }

        // 0x00CD0B00 (:939)
        bool GameLinkLan::getVoiceChatRemotePlayerInfo(GameLinkVoiceChatPlayerInfo*, const GameLinkRemoteStation&)
        {
            return false;
        }

        // 0x00CD0B10 (:946)
        bool GameLinkLan::hasVoiceChatPermissions() const
        {
            return false;
        }

        // 0x00CD0B20 (:951)
        void GameLinkLan::setVoiceChatVolume(float)
        {
        }

        // 0x00CD0B30 (:956)
        bool GameLinkLan::sendBroadcastPacket(const PacketHeader* pPacket)
        {
            keen::NetworkAddress broadcast;
            broadcast.ip4Address = 0xFFFFFFFFu; // 255.255.255.255
            broadcast.port = m_params.lan.portMatchmakingBroadcast;
            broadcast.pad0 = 0;
            uint32 sent;
            return keen::network::sendDataTo(m_socketBroadcast, &sent, pPacket, pPacket->size, broadcast) == 0 && sent != 0;
        }

        // 0x00CD0B80 (:974)
        bool GameLinkLan::sendPacketTo(const PacketHeader* pPacket, const keen::NetworkAddress& targetAddress)
        {
            uint32 sent;
            return keen::network::sendDataTo(m_socketBroadcast, &sent, pPacket, pPacket->size, targetAddress) == 0 && sent != 0;
        }

        // 0x00CD0BC0 (:989): there must be an owner (not checked)
        bool GameLinkLan::sendPacketToHost(const PacketHeader* pPacket)
        {
            const keen::NetworkAddress target = getNetworkAddress(getLobbyOwner());
            uint32 sent;
            return keen::network::sendDataTo(m_socketBroadcast, &sent, pPacket, pPacket->size, target) == 0 && sent != 0;
        }

        // 0x00CD0C20 (:1010)
        bool GameLinkLan::receivePacket(uint8* pBuffer, uint32 bufferSize, uint32* pNumBytesReceived, keen::NetworkAddress* pSenderAddress)
        {
            return keen::network::receiveDataFrom(m_socketBroadcast, pNumBytesReceived, pBuffer, bufferSize, pSenderAddress) == 0 && *pNumBytesReceived != 0;
        }

        // 0x00CD71B0 (:1024)
        void GameLinkLan::handlePacket(const uint8* pBuffer, uint32 numBytesReceived, const keen::NetworkAddress& senderAddress)
        {
            const PacketHeader* pHeader = (const PacketHeader*)pBuffer;
            if (pHeader->uid != m_params.lan.gameUniqueIdentifier)
            {
                return;
            }
            if (pHeader->type == PacketType_UserDefined)
            {
                // queued from any sender, in any state; not checked against the capacity (original behaviour: a full
                // queue overwrites its oldest packet)
                UserDefinedPacket& packet = m_userDefinedPackets.pushUninitialized();
                const uint32 payloadSize = numBytesReceived - sizeof(PacketHeader);
                packet.senderAddress = senderAddress;
                packet.size = payloadSize;
                memmove(packet.data, pBuffer + sizeof(PacketHeader), payloadSize);
                return;
            }
            keen::NetworkAddress hostAddress;
            if (!openrfgGetLanOwnHostAddress(&hostAddress)) // deviation: see openrfgGetLanOwnHostAddress
            {
                return;
            }
            // the lobby packets from this machine are ignored (deviation: from this instance, see isOwnAddress)
            if (isOwnAddress(senderAddress, hostAddress, m_params))
            {
                return;
            }
            switch (pHeader->type)
            {
            case PacketType_LobbyInfoRequest:
                if (m_host)
                {
                    PacketHeaderLobbyInfoResponse response;
                    response.initialize(m_params.lan.gameUniqueIdentifier, (uint8)m_lobbyMembers.getSize(), (uint8)m_maxNumLobbyMembers, m_lobbyType, m_lobbyFilters.getStart(), m_localUserName);
                    if (!sendPacketTo(&response, senderAddress))
                    {
                        keen::debug::nativePrintArgs("[gamelink] failed to send lobbyInfo broadcast packet\n");
                    }
                }
                break;
            case PacketType_LobbyInfoResponse:
                if (m_searchRunning && m_lobbySearchResults.getSize() < m_maxNumSearchResults && isLobbyInfoRelevant((const PacketHeaderLobbyInfoResponse*)pBuffer, senderAddress))
                {
                    const PacketHeaderLobbyInfoResponse* pResponse = (const PacketHeaderLobbyInfoResponse*)pBuffer;
                    GameLinkSearchResult& result = m_lobbySearchResults.pushBack();
                    memset(&result, 0, sizeof(result));
                    result.lobby.id.asNetworkAddress.ipAddress.asUint32 = senderAddress.ip4Address;
                    result.lobby.id.asNetworkAddress.port = senderAddress.port;
                    result.currentNumPlayers = pResponse->currentNumPlayers;
                    result.maxNumPlayers = pResponse->maxNumPlayers;
                    keen::copyString(result.lobbyName, sizeof(result.lobbyName), pResponse->lobbyName);
                }
                break;
            case PacketType_LobbyStateUpdate:
                if (!m_host && m_internalState == InternalState_InLobby)
                {
                    // (the retail build formats the sender with getIpAndPortString for a stripped debug message)
                    const PacketHeaderLobbyStateUpdate* pUpdate = (const PacketHeaderLobbyStateUpdate*)pBuffer;
                    if (isLobbyStateUpdateRelevant(pUpdate, senderAddress))
                    {
                        m_lobbyStateId = pUpdate->lobbyStateId;
                        m_lobbyType = pUpdate->lobbyType;
                        for (uint32 i = 0; i < 8; i++)
                        {
                            m_lobbyFilters[i] = pUpdate->lobbyFilters[i];
                        }
                        m_lobbyLocked = pUpdate->lobbyLocked;
                        // this instance's station: the host address on the local port (deviation; original: the
                        // matchmaking port, which is the same when the deviation is off)
                        keen::NetworkAddress ownAddress;
                        openrfgGetLanOwnHostAddress(&ownAddress); // deviation: see openrfgGetLanOwnHostAddress
                        ownAddress.port = getLocalPort(m_params);
                        m_lobbyMembers.clear();
                        const uint32 numMembers = pUpdate->numLobbyMembers;
                        m_maxNumLobbyMembers = pUpdate->maxNumLobbyMembers;
                        const GameLinkRemoteStation* pMembers = (const GameLinkRemoteStation*)(pUpdate + 1);
                        for (uint32 i = 0; i < numMembers; i++)
                        {
                            const keen::NetworkAddress memberAddress = getNetworkAddress(&pMembers[i]);
                            addLobbyMember(memberAddress, pMembers[i].lobbyOwner, ownAddress == memberAddress, pMembers[i].userName);
                        }
                        m_receivedFirstLobbyStateUpdate = true;
                    }
                }
                break;
            case PacketType_JoinRequest:
                if (m_host)
                {
                    JoinResponseResult result;
                    if (m_lobbyType != GameLinkLobbyType_Public)
                    {
                        result = JoinResponseResult_Failed_LobbyNotPublic;
                    }
                    else if (m_lobbyLocked)
                    {
                        result = JoinResponseResult_Failed_LobbyLocked;
                    }
                    else
                    {
                        result = m_lobbyMembers.getSize() != m_maxNumLobbyMembers ? JoinResponseResult_Success : JoinResponseResult_Failed_LobbyFull;
                    }
                    unit4::RemoteUserId senderId;
                    *(keen::NetworkAddress*)senderId.providerData = senderAddress;
                    senderId.guestIndex = 0; // uninitialized in the original (getRemoteStationById ignores both)
                    senderId.isValid = false;
                    if (getRemoteStationById(senderId) != nullptr)
                    {
                        // a repeated request from a member: answered as a success again
                        result = JoinResponseResult_Success;
                    }
                    else if (result == JoinResponseResult_Success)
                    {
                        addLobbyMember(senderAddress, false, false, ((const PacketHeaderJoinRequest*)pBuffer)->userName);
                        m_lobbyStateId++;
                    }
                    PacketHeaderJoinResponse response;
                    response.initialize(m_params.lan.gameUniqueIdentifier, result);
                    if (!sendPacketTo(&response, senderAddress))
                    {
                        keen::debug::nativePrintArgs("[gamelink] handlePacket: failed to send joinResponse\n");
                    }
                }
                break;
            case PacketType_JoinResponse:
                if (!m_host && m_internalState == InternalState_JoiningLobby && senderAddress == getNetworkAddress(m_targetLobby.id))
                {
                    const JoinResponseResult result = ((const PacketHeaderJoinResponse*)pBuffer)->result;
                    m_targetLobby.id.asUint64 = 0;
                    if (result == JoinResponseResult_Success)
                    {
                        // in the lobby; the members arrive with the first lobby state update
                        m_internalState = InternalState_InLobby;
                        m_timeLobbyJoined = keen::SystemTimer::getCurrentMilliseconds();
                        m_receivedFirstLobbyStateUpdate = false;
                        return;
                    }
                    m_internalState = InternalState_Online;
                    m_state = GameLinkState_Online;
                    if (result == JoinResponseResult_Failed_LobbyFull)
                    {
                        setError(GameLinkError_JoinLobbyFailed_Full, GameLinkLanAdditionalPlatformErrorCodes_JoinResponseResult_Failed_LobbyFull);
                    }
                    else if (result == JoinResponseResult_Failed_LobbyLocked)
                    {
                        setError(GameLinkError_JoinLobbyFailed_Locked, GameLinkLanAdditionalPlatformErrorCodes_JoinResponseResult_Failed_LobbyLocked);
                    }
                    else if (result == JoinResponseResult_Failed_LobbyNotPublic)
                    {
                        // original: reported as locked
                        setError(GameLinkError_JoinLobbyFailed_Locked, GameLinkLanAdditionalPlatformErrorCodes_JoinResponseResult_Failed_LobbyNotPublic);
                    }
                    else
                    {
                        setError(GameLinkError_JoinLobbyFailed_Unknown, GameLinkLanAdditionalPlatformErrorCodes_JoinResponseResult_Failed_Unknown);
                    }
                }
                break;
            case PacketType_KeepAlive:
                if (m_internalState == InternalState_InLobby)
                {
                    GameLinkRemoteStationLan* pMember = findLobbyMember(senderAddress);
                    if (pMember != nullptr)
                    {
                        pMember->lastReceivedLobbyStateId = ((const PacketHeaderKeepAlive*)pBuffer)->lobbyStateId;
                        pMember->timeLastKeepAliveReceived = keen::SystemTimer::getCurrentMilliseconds();
                    }
                    // (from a non-member: the retail build formats the sender for a stripped debug message)
                }
                break;
            default:
                break;
            }
        }

        // 0x00CD5840 (:1257): not checked against the capacity
        void GameLinkLan::addLobbyMember(const keen::NetworkAddress& networkAddress, bool lobbyOwner, bool myStation, const char* pUserName)
        {
            GameLinkRemoteStationLan& member = m_lobbyMembers.pushBack();
            *(keen::NetworkAddress*)member.id.providerData = networkAddress;
            member.lobbyOwner = lobbyOwner;
            member.id.guestIndex = 0;
            member.id.isValid = true;
            member.myStation = myStation;
            keen::copyString(member.userName, sizeof(member.userName), pUserName);
            member.lastReceivedLobbyStateId = 0;
            member.timeLastKeepAliveReceived = 0;
            member.timeAdded = keen::SystemTimer::getCurrentMilliseconds();
        }

        // 0x00CD39A0 (:1275)
        GameLinkRemoteStationLan* GameLinkLan::findLobbyMember(const keen::NetworkAddress& networkAddress)
        {
            for (uint32 i = 0; i < m_lobbyMembers.getSize(); i++)
            {
                const keen::NetworkAddress memberAddress = getNetworkAddress(&m_lobbyMembers[i]);
                if (memberAddress.ip4Address == networkAddress.ip4Address && memberAddress.port == networkAddress.port)
                {
                    return &m_lobbyMembers[i];
                }
            }
            return nullptr;
        }

        // 0x00CD39F0 (:1289): a lobby with room whose filters match the search's (0 matches any), not yet listed
        bool GameLinkLan::isLobbyInfoRelevant(const PacketHeaderLobbyInfoResponse* pLobbyInfo, const keen::NetworkAddress& senderAddress)
        {
            if (pLobbyInfo->currentNumPlayers >= pLobbyInfo->maxNumPlayers)
            {
                return false;
            }
            for (uint32 i = 0; i < 8; i++)
            {
                if (pLobbyInfo->lobbyFilters[i] != m_lobbySearchFilterValues[i] && m_lobbySearchFilterValues[i] != 0)
                {
                    return false;
                }
            }
            for (uint32 i = 0; i < m_lobbySearchResults.getSize(); i++)
            {
                const GameLinkLobbyId& listed = m_lobbySearchResults[i].lobby.id;
                // original: by IP only (one host per machine); deviation: IP and port
                if (listed.asNetworkAddress.ipAddress.asUint32 == senderAddress.ip4Address && (g_openrfgLanLocalPort == 0 || listed.asNetworkAddress.port == senderAddress.port))
                {
                    return false;
                }
            }
            return true;
        }

        // 0x00CD0C60 (:1317): a new state id, from the owner (any sender before the first update)
        bool GameLinkLan::isLobbyStateUpdateRelevant(const PacketHeaderLobbyStateUpdate* pLobbyStateUpdate, const keen::NetworkAddress& senderAddress)
        {
            if (m_lobbyStateId == pLobbyStateUpdate->lobbyStateId)
            {
                return false;
            }
            const GameLinkRemoteStation* pOwner = getLobbyOwner();
            if (pOwner != nullptr)
            {
                const keen::NetworkAddress ownerAddress = getNetworkAddress(pOwner);
                if (senderAddress.ip4Address != ownerAddress.ip4Address || senderAddress.port != ownerAddress.port)
                {
                    return false;
                }
            }
            return true;
        }

        // 0x00CD58B0 (:1338): rfg.exe runs without host migration (multi_init), so a client that loses the host
        // leaves the lobby with GameLinkError_SessionLost. With migration the lowest IP becomes the host; the others
        // join it. Original behaviour: getNextHost can answer null (an empty lobby), which is not checked.
        void GameLinkLan::startHostMigration()
        {
            if (!m_params.lan.hostMigrationEnabled)
            {
                setError(GameLinkError_SessionLost, GameLinkLanAdditionalPlatformErrorCodes_HostMigration_NotEnabled);
                leaveLobby();
                return;
            }
            GameLinkRemoteStationLan* pOldHost = getCurrentHost();
            if (m_lobbyMembers.getSize() != 0)
            {
                // erase the old host (with none, the original erases at the null position: the shift copies nothing)
                GameLinkRemoteStationLan* pLast = m_lobbyMembers.getStart() + (m_lobbyMembers.getSize() - 1);
                if (pOldHost != nullptr && pOldHost < pLast)
                {
                    memmove(pOldHost, pOldHost + 1, (uint8*)(pLast + 1) - (uint8*)(pOldHost + 1));
                }
                m_lobbyMembers.popBack();
            }
            GameLinkRemoteStationLan* pNewHost = getNextHost();
            pNewHost->lobbyOwner = true;
            if (pNewHost->myStation)
            {
                m_host = true;
                const uint32 now = keen::SystemTimer::getCurrentMilliseconds();
                for (uint32 i = 0; i < m_lobbyMembers.getSize(); i++)
                {
                    GameLinkRemoteStationLan& member = m_lobbyMembers[i];
                    if (!member.myStation)
                    {
                        member.lastReceivedLobbyStateId = 0;
                        member.timeAdded = now;
                        member.timeLastKeepAliveReceived = now;
                    }
                }
                m_lobbyStateId++;
                return;
            }
            m_internalState = InternalState_JoiningLobby;
            m_timeJoinRequestStarted = keen::SystemTimer::getCurrentMilliseconds();
            const keen::NetworkAddress newHostAddress = getNetworkAddress(pNewHost);
            m_targetLobby.id.asNetworkAddress.ipAddress.asUint32 = newHostAddress.ip4Address;
            m_targetLobby.id.asNetworkAddress.port = newHostAddress.port;
            pNewHost->timeAdded = keen::SystemTimer::getCurrentMilliseconds();
            // (the retail build formats the new host's address for a stripped debug message)
        }

        // 0x00CD3A60 (:1398): the member with the lowest IP (as a little-endian word)
        GameLinkRemoteStationLan* GameLinkLan::getNextHost()
        {
            GameLinkRemoteStationLan* pNext = nullptr;
            uint32 lowest = 0xFFFFFFFFu;
            for (uint32 i = 0; i < m_lobbyMembers.getSize(); i++)
            {
                const uint32 ip = getNetworkAddress(&m_lobbyMembers[i]).ip4Address;
                if (ip < lowest)
                {
                    pNext = &m_lobbyMembers[i];
                    lowest = ip;
                }
            }
            return pNext;
        }

        // 0x00CD3AA0 (:1417)
        GameLinkRemoteStationLan* GameLinkLan::getCurrentHost()
        {
            for (uint32 i = 0; i < m_lobbyMembers.getSize(); i++)
            {
                if (m_lobbyMembers[i].lobbyOwner)
                {
                    return &m_lobbyMembers[i];
                }
            }
            return nullptr;
        }
    }
}
