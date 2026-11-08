// Original: library/modules/runtime/gamelink/sources/global/lan/gamelinklan.hpp
// Implementation: library/modules/runtime/gamelink/sources/global/lan/gamelinklan.cpp
//
// GameLinkLan: lobbies on the local network. Hosts answer lobby info requests broadcast on the matchmaking port and send
// lobby state updates; clients send join requests and keep-alives. Packets start with a PacketHeader (type, game unique
// identifier, uid). Placement of the packet headers, remote stations and enums inferred.
#pragma once

#include "keen/base/types.hpp"
#include "keen/base/array.hpp"
#include "keen/base/memoryallocator.hpp"
#include "keen/base/queue.hpp"
#include "keen/base/randomnumbergenerator.hpp"
#include "keen/base/sizedarray.hpp"
#include "kaiko/gamelink/gamelink.hpp"
#include "library/modules/runtime/gamelink/sources/global/gamelinkinternal.hpp"
#include "keen/network/networkaddress.hpp"
#include "unit4/systemservices/systemservices.hpp"

namespace kaiko
{
    namespace gamelink
    {
        enum GameLinkLanConstants
        {
            GameLinkLanConstants_ReceiveBufferSize = 1024,
            GameLinkLanConstants_StartCheckTimeoutDelay = 3000,
        };

        enum GameLinkLanAdditionalPlatformErrorCodes
        {
            GameLinkLanAdditionalPlatformErrorCodes_None = 0,
            GameLinkLanAdditionalPlatformErrorCodes_HostMigration_NotEnabled = 0x10000001,
            GameLinkLanAdditionalPlatformErrorCodes_JoinResponseResult_Failed_LobbyFull = 0x10000002,
            GameLinkLanAdditionalPlatformErrorCodes_JoinResponseResult_Failed_LobbyLocked = 0x10000003,
            GameLinkLanAdditionalPlatformErrorCodes_JoinResponseResult_Failed_LobbyNotPublic = 0x10000004,
            GameLinkLanAdditionalPlatformErrorCodes_JoinResponseResult_Failed_Unknown = 0x10000005,
            GameLinkLanAdditionalPlatformErrorCodes_JoinRequest_Failed_Timeout = 0x10000006,
            GameLinkLanAdditionalPlatformErrorCodes_ReceiveFirstLobbyStateUpdate_Failed_Timeout = 0x10000007,
        };

        enum PacketType
        {
            PacketType_Invalid = 0,
            PacketType_UserDefined = 1,
            PacketType_LobbyInfoRequest = 2,
            PacketType_LobbyInfoResponse = 3,
            PacketType_LobbyStateUpdate = 4,
            PacketType_JoinRequest = 5,
            PacketType_JoinResponse = 6,
            PacketType_KeepAlive = 7,
        };

        enum JoinResponseResult
        {
            JoinResponseResult_Invalid = 0,
            JoinResponseResult_Success = 1,
            JoinResponseResult_Failed_LobbyFull = 2,
            JoinResponseResult_Failed_LobbyLocked = 3,
            JoinResponseResult_Failed_LobbyNotPublic = 4,
        };

        // size 0x6
        struct IpAndPort
        {
            uint8 ip[4]; // 0x00
            uint16 port; // 0x04
        };

        // size 0xC
        struct PacketHeader
        {
            uint32 uid; // 0x00
            uint32 size; // 0x04
            PacketType type; // 0x08
        };

        // size 0xC
        struct PacketHeaderLobbyInfoRequest : public PacketHeader
        {
            void initialize(uint32 a_uid); // 0x00CD0260 (.h:69)

        };

        // size 0x74
        struct PacketHeaderLobbyInfoResponse : public PacketHeader
        {
            void initialize(uint32 a_uid, uint8 a_currentNumPlayers, uint8 a_maxNumPlayers, GameLinkLobbyType a_lobbyType, uint32* a_pLobbyFilters, const char* a_pLobbyName); // 0x00CD0280 (.h:90)

            uint8 currentNumPlayers; // 0x0C
            uint8 maxNumPlayers; // 0x0D
            GameLinkLobbyType lobbyType; // 0x10
            uint32 lobbyFilters[8]; // 0x14
            char lobbyName[64]; // 0x34
        };

        // size 0x40
        struct PacketHeaderLobbyStateUpdate : public PacketHeader
        {
            void initialize(uint32 a_uid, uint32 a_lobbyStateId, GameLinkLobbyType a_lobbyType, bool a_lobbyLocked, uint32* a_pLobbyFilters, uint32 a_numLobbyMembers, uint32 a_maxNumLobbyMembers); // 0x00CD0300 (.h:122)

            uint32 lobbyStateId; // 0x0C
            GameLinkLobbyType lobbyType; // 0x10
            bool lobbyLocked; // 0x14
            uint32 lobbyFilters[8]; // 0x18
            uint32 numLobbyMembers; // 0x38
            uint32 maxNumLobbyMembers; // 0x3C
        };

        // size 0x4C
        struct PacketHeaderJoinRequest : public PacketHeader
        {
            void initialize(uint32 a_uid, const char* a_pUserName); // 0x00CD0370 (.h:144)

            char userName[64]; // 0x0C
        };

        // size 0x10
        struct PacketHeaderJoinResponse : public PacketHeader
        {
            void initialize(uint32 a_uid, JoinResponseResult a_result); // 0x00CD03A0 (.h:167)

            JoinResponseResult result; // 0x0C
        };

        // size 0x10
        struct PacketHeaderKeepAlive : public PacketHeader
        {
            void initialize(uint32 a_uid, uint32 a_lobbyStateId); // 0x00CD03C0 (.h:181)

            uint32 lobbyStateId; // 0x0C
        };

        // size 0x58
        struct GameLinkRemoteStationLan : public GameLinkRemoteStation
        {
            uint32 lastReceivedLobbyStateId; // 0x4C
            uint32 timeLastKeepAliveReceived; // 0x50
            uint32 timeAdded; // 0x54
        };

        // size 0xAE8
        class GameLinkLan : public GameLink
        {
        public:
            enum InternalState
            {
                InternalState_Offline = 0,
                InternalState_Online = 1,
                InternalState_JoiningLobby = 2,
                InternalState_InLobby = 3,
            };

            // size 0x4BC
            struct UserDefinedPacket
            {
                uint8 data[1200]; // 0x00
                uint32 size; // 0x4B0
                keen::NetworkAddress senderAddress; // 0x4B4
            };

            GameLinkLan(); // 0x00CD5680 (:46)
            virtual ~GameLinkLan(); // vtbl+0x00 0x00CD5720 (:50)
            void create(keen::MemoryAllocator* pAllocator, const GameLinkLanCreationParameters& params, const GameLinkGeneralCreationParameters& paramsGeneral); // 0x00CD6DA0 (:54)
            void destroy(keen::MemoryAllocator* pAllocator); // 0x00CD6EC0 (:76)
            virtual bool beginSession(const GameLinkSessionParameters& params); // vtbl+0x10 0x00CD3360 (:85)
            virtual void endSession(); // vtbl+0x14 0x00CD0890 (:156)
            virtual bool findAndJoinLobby(uint32 maxNumLobbyMembers, const uint32* pLobbyFilters, uint32 numLobbyFilters); // vtbl+0x04 0x00CD5730 (:170)
            virtual GameLinkFindAndJoinStatus getFindAndJoinStatus() const; // vtbl+0x08 0x00CD08C0 (:207)
            virtual void cancelFindAndJoinLobby(); // vtbl+0x0C 0x00CD08D0 (:212)
            virtual void setPartyEnabled(bool enabled); // vtbl+0x18 0x00CD08F0 (:223)
            virtual bool isPartyEnabled() const; // vtbl+0x1C 0x00CD0900 (:228)
            virtual bool isPartyOwner() const; // vtbl+0x20 0x00CD0910 (:233)
            virtual bool getPartyOwner(unit4::NamedRemoteUserId* pOutHost) const; // vtbl+0x24 0x00CD0920 (:238)
            virtual uint32 getPartyMembers(unit4::NamedRemoteUserId* pPartyMembers, uint32 capacity) const; // vtbl+0x28 0x00CD0930 (:245)
            virtual uint32 getNumPartyMembers() const; // vtbl+0x2C 0x00CD0940 (:253)
            virtual void update(float timeStep); // vtbl+0x30 0x00CD8700 (:258)
            virtual bool createLobby(GameLinkLobbyType lobbyType, uint32 maxNumLobbyMembers, const uint32* pLobbyFilters, uint32 numLobbyFilters); // vtbl+0x34 0x00CD6FA0 (:507)
            virtual bool startSearchLobby(uint32 maxNumResults, const uint32* pLobbyFilters, uint32 numLobbyFilters); // vtbl+0x38 0x00CD34D0 (:556)
            virtual uint32 getSearchResults(GameLinkSearchResult* pSearchResults, uint32 capacity, bool* pSearchRunning) const; // vtbl+0x3C 0x00CD3560 (:588)
            virtual bool joinLobby(const GameLinkInvitationData* pInvitationData); // vtbl+0x40 0x00CD0A00 (:645)
            virtual bool joinLobby(const GameLinkLobby& lobby); // vtbl+0x44 0x00CD0950 (:610)
            virtual void leaveLobby(); // vtbl+0x48 0x00CD57E0 (:654)
            virtual void openInvitationDialog(); // vtbl+0x4C 0x00CD0A20 (:675)
            virtual uint32 getLobbyMembers(const GameLinkRemoteStation** pRemoteStations, uint32 capacity) const; // vtbl+0x50 0x00CD36C0 (:725)
            virtual bool setLobbyFilters(const uint32* pIndices, const uint32* pValues, uint32 numLobbyFilters); // vtbl+0x54 0x00CD35D0 (:680)
            virtual uint32 getLobbyFilters(uint32* pLobbyFilters, uint32 capacity) const; // vtbl+0x58 0x00CD3630 (:702)
            virtual bool sendP2PPacketUnreliable(const GameLinkRemoteStation* pStation, const void* pPacket, uint32 packetSize); // vtbl+0x5C 0x00CD3710 (:756)
            virtual uint32 getNextP2PPacket(void* pBuffer, uint32 bufferSize, const GameLinkRemoteStation** pReturnSender); // vtbl+0x60 0x00CD70E0 (:783)
            virtual bool setLobbyType(GameLinkLobbyType lobbyType); // vtbl+0x64 0x00CD0A40 (:816)
            virtual bool setLobbyLocked(bool locked); // vtbl+0x68 0x00CD0A70 (:829)
            virtual bool setLobbyOwner(unit4::RemoteUserId); // vtbl+0x6C 0x00CD0AA0 (:842)
            virtual uint32 getLobbyMaxMembers() const; // vtbl+0x70 0x00CD0AB0 (:848)
            virtual uint32 getNumLobbyMembers() const; // vtbl+0x74 0x00CD3800 (:858)
            virtual GameLinkLobby getLobbyId() const; // vtbl+0x78 0x00CD3810 (:868)
            virtual const GameLinkRemoteStation* getLobbyOwner() const; // vtbl+0x7C 0x00CD3860 (:883)
            virtual bool isLobbyOwner() const; // vtbl+0x80 0x00CD0A30 (:746)
            virtual const GameLinkRemoteStation* getRemoteStationById(unit4::RemoteUserId id) const; // vtbl+0x84 0x00CD38A0 (:901)
            virtual void setVoiceChatEnabled(bool isEnabled); // vtbl+0x88 0x00CD0AC0 (:917)
            virtual void setVoiceChatLocalPlayerMuted(bool isMuted); // vtbl+0x8C 0x00CD0AD0 (:922)
            virtual void setVoiceChatRemotePlayerMuted(const GameLinkRemoteStation& remoteStation, bool isMuted); // vtbl+0x90 0x00CD0AE0 (:927)
            virtual bool getVoiceChatLocalPlayerInfo(GameLinkVoiceChatPlayerInfo* pVoiceChatInfo); // vtbl+0x94 0x00CD0AF0 (:933)
            virtual bool getVoiceChatRemotePlayerInfo(GameLinkVoiceChatPlayerInfo* pVoiceChatInfo, const GameLinkRemoteStation& remoteStation); // vtbl+0x98 0x00CD0B00 (:939)
            virtual bool hasVoiceChatPermissions() const; // vtbl+0x9C 0x00CD0B10 (:946)
            virtual void setVoiceChatVolume(float volume); // vtbl+0xA0 0x00CD0B20 (:951)
        private:
            bool sendBroadcastPacket(const PacketHeader* pPacket); // 0x00CD0B30 (:956)
            bool sendPacketTo(const PacketHeader* pPacket, const keen::NetworkAddress& targetAddress); // 0x00CD0B80 (:974)
            bool sendPacketToHost(const PacketHeader* pPacket); // 0x00CD0BC0 (:989)
            bool receivePacket(uint8* pBuffer, uint32 bufferSize, uint32* pNumBytesReceived, keen::NetworkAddress* pSenderAddress); // 0x00CD0C20 (:1010)
            void handlePacket(const uint8* pBuffer, uint32 numBytesReceived, const keen::NetworkAddress& senderAddress); // 0x00CD71B0 (:1024)
            void addLobbyMember(const keen::NetworkAddress& networkAddress, bool lobbyOwner, bool myStation, const char* pUserName); // 0x00CD5840 (:1257)
            GameLinkRemoteStationLan* findLobbyMember(const keen::NetworkAddress& networkAddress); // 0x00CD39A0 (:1275)
            bool isLobbyInfoRelevant(const PacketHeaderLobbyInfoResponse* pLobbyInfo, const keen::NetworkAddress& senderAddress); // 0x00CD39F0 (:1289)
            bool isLobbyStateUpdateRelevant(const PacketHeaderLobbyStateUpdate* pLobbyStateUpdate, const keen::NetworkAddress& senderAddress); // 0x00CD0C60 (:1317)
            void startHostMigration(); // 0x00CD58B0 (:1338)
            GameLinkRemoteStationLan* getNextHost(); // 0x00CD3A60 (:1398)
            GameLinkRemoteStationLan* getCurrentHost(); // 0x00CD3AA0 (:1417)

            InternalState m_internalState; // 0x18
            GameLinkSessionParameters m_params; // 0x1C
            uint32 m_socketBroadcast; // 0x4C
            bool m_host; // 0x50
            char m_localUserName[64]; // 0x51
            uint32 m_timeLobbyInfoRequestSent; // 0x94
            uint32 m_timeLobbyStateSent; // 0x98
            uint32 m_timeJoinRequestSent; // 0x9C
            uint32 m_timeKeepAliveSent; // 0xA0
            keen::Queue<UserDefinedPacket> m_userDefinedPackets; // 0xA4
            bool m_searchRunning; // 0xB8
            keen::SizedArray<GameLinkSearchResult> m_lobbySearchResults; // 0xBC
            keen::Array<uint32> m_lobbySearchFilterValues; // 0xC8
            uint32 m_maxNumSearchResults; // 0xD0
            uint32 m_timeLobbySearchStarted; // 0xD4
            GameLinkFindAndJoinData m_findAndJoinData; // 0xD8
            keen::RandomNumberGenerator m_random; // 0xE8
            GameLinkLobby m_targetLobby; // 0xAB0
            uint32 m_timeJoinRequestStarted; // 0xAB8
            uint32 m_timeLobbyJoined; // 0xABC
            bool m_receivedFirstLobbyStateUpdate; // 0xAC0
            uint32 m_lobbyStateId; // 0xAC4
            keen::SizedArray<GameLinkRemoteStationLan> m_lobbyMembers; // 0xAC8
            keen::Array<uint32> m_lobbyFilters; // 0xAD4
            GameLinkLobbyType m_lobbyType; // 0xADC
            bool m_lobbyLocked; // 0xAE0
            uint32 m_maxNumLobbyMembers; // 0xAE4
        };

    }

    namespace gamelink
    {
        // OpenRFG deviation (multiplayer.md 2.3, new code; see gamelinklan.cpp): the local LAN port, 0 = original
        // behaviour. Set before beginSession (the multiplayer programs' --lan-port).
        extern uint16 g_openrfgLanLocalPort;

        // OpenRFG deviation (part of the one above; see gamelinklan.cpp): the host's own address, the default route's
        // source address when the deviation is on (original: the local host's first address). The port is left as it
        // is. False without a local address.
        bool openrfgGetLanOwnHostAddress(keen::NetworkAddress* pTarget);

        void getIpAndPortString(char* pBuffer, uint32 bufferSize, keen::NetworkAddress networkAddress); // 0x00CD0820 (:26)
        keen::NetworkAddress getNetworkAddress(const GameLinkRemoteStation* pRemoteStation); // 0x00CD0860 (:32)
        keen::NetworkAddress getNetworkAddress(const GameLinkLobbyId& lobbyId); // 0x00CD0870 (:38)
    }
}
