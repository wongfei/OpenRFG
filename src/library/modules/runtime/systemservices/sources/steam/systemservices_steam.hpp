// Original: library/modules/runtime/systemservices/sources/steam/systemservices_steam.hpp
// Implementation: library/modules/runtime/systemservices/sources/steam/systemservices_steam.cpp
//
// The Steam system services (SystemServices: Steam achievements, stats, leaderboards, rich presence). Placement of
// SystemServices inferred.
#pragma once

#include "keen/base/types.hpp"
#include "keen/base/memoryallocator.hpp"
#include "library/modules/runtime/systemservices/sources/global/systemservices_internal.hpp"
#include "library/modules/runtime/systemservices/sources/steam/onlineachievements_steam.hpp"
#include "library/modules/runtime/systemservices/sources/steam/onlinestats_steam.hpp"
#include "keen/useraccount/useraccounttypes.hpp"
#include "library/modules/runtime/useraccount/sources/global/useraccountsysteminternal.hpp"

struct INetworkListManager;

namespace keen
{
    struct LocalPlayerIdStructureType;
}

namespace unit4
{
    struct NamedRemoteUserId;
    struct RemoteUserId;
    struct SystemServicesParameters;

    // size 0x5B8
    struct SystemServices : public SystemServicesBase
    {
        enum SendRankingStep
        {
            SendRankingStep_Invalid = 0,
            SendRankingStep_FindLeaderboard = 1,
            SendRankingStep_UploadScore = 2,
        };

        enum ReceiveRankingStep
        {
            ReceiveRankingStep_Invalid = 0,
            ReceiveRankingStep_FindLeaderboard = 1,
            ReceiveRankingStep_DownloadLeaderboard = 2,
        };

        SystemServices(); // 0x00C14E80
        ~SystemServices(); // 0x00C14BD0

        keen::SteamAchievements steamAchievements; // 0xD8
        keen::SteamStats steamStats; // 0x140
        const char* pPresenceStrings[64]; // 0x490
        uint32 presenceStringCount; // 0x590
        SendRankingStep sendRankingStep; // 0x594
        uint64 sendRankingCall; // 0x598
        ReceiveRankingStep receiveRankingStep; // 0x5A0
        uint64 receiveRankingCall; // 0x5A8
        INetworkListManager* pNetworkListManager; // 0x5B0
    };

    namespace systemservices
    {
        SystemServices* createSystemServices(keen::MemoryAllocator* pAllocator, const SystemServicesParameters* pParams, keen::UserAccountSystem*); // 0x00C152F0 (:23)
        void destroySystemServices(keen::MemoryAllocator* pAllocator, SystemServices* pServices); // 0x00C15580 (:68)
        void updateSystemServicesPlatform(SystemServices* pServices, float timeStep); // 0x00C13A50 (:83)
        bool doesSystemHaveNetworkAccess(SystemServices* pServices); // 0x00C131B0 (:150)
        bool updateAchievementProgress(SystemServices* pServices, const keen::UserAccount& userAccount, uint32 achievementId, uint32 status); // 0x00C13B90 (:168)
        void showAchievementsUi(const keen::UserAccount& userAccount); // 0x00C131F0 (:179)
        void showRemoteUserUi(SystemServices* pServices, const keen::LocalPlayerIdStructureType* askingUser, const NamedRemoteUserId& userInfo); // 0x00C13BD0 (:185)
        bool setPresence(SystemServices* pServices, uint32 stringIndex); // 0x00C13C20 (:198)
        bool startGetRankingListPlatform(SystemServices* pServices); // 0x00C13C60 (:212)
        bool getRankingListResultPlatform(SystemServices* pServices); // 0x00C13CF0 (:228)
        bool startSendRankingEntryPlatform(SystemServices* pServices); // 0x00C14060 (:321)
        void createRemoteUserIdFromUserAccountId(RemoteUserId* pResult, keen::UserAccountId id); // 0x00C140F0 (:337)
        void createUserAccountIdFromRemoteUserId(keen::UserAccountId* pResult, const RemoteUserId& id); // 0x00C13220 (:355)
        void setInteractionResponse(SystemServices* pServices, uint32); // 0x00C13250 (:371)
        bool updateHeroStatSet(SystemServices* pServices, const keen::UserAccount& userAccount, const uint32 statId, double newValue); // 0x00C13270 (:383)
        bool updateHeroStatAdd(SystemServices* pServices, const keen::UserAccount& userAccount, const uint32 statId, double addend); // 0x00C132A0 (:393)
        bool isDataChunkAvailable(SystemServices* pServices, const uint32 chunkId, float* pDownloadProgress); // 0x00C132D0 (:406)
        void prioritizeDataChunk(SystemServices* pServices, const uint32 chunkId); // 0x00C132E0 (:426)
    }
}
