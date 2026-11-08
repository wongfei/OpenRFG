// Original: library/modules/runtime/systemservices/include/unit4/systemservices/systemservices.hpp
// Implementation: library/modules/runtime/systemservices/sources/global/systemservices.cpp
//
// System services: achievements, presence, stats (StatUpdateData) and leaderboards (ranking queries and results), with
// remote user ids and interactions; created in rfg::Application::initialize. SystemServices itself is defined in
// sources/steam/systemservices_steam.hpp. Placement of the ranking types (and of RankingBoardCacheEntry, which
// SystemServicesInteractionData holds by value) inferred.
#pragma once

#include "keen/base/types.hpp"
#include "keen/base/array.hpp"
#include "keen/base/memoryallocator.hpp"
#include "keen/base/sizedarray.hpp"
#include "keen/steam/steamworkssystem.hpp"
#include "keen/useraccount/useraccounttypes.hpp"

namespace unit4
{
    struct SystemServices;

    enum SystemServicesLimits
    {
        OnlineSystemLimits_MaxPlayerDisplayNameSize = 64,
        OnlineSystemLimits_MaxGameInfoDisplayNameSize = 64,
        OnlineSystemLimits_MaxFriendCountPerQuery = 32,
        OnlineSystemLimits_MaxPresenceStrings = 64,
        OnlineSystemLimits_MaxPresenceStringLength = 64,
        OnlineSystemLimits_MaxRankingListResults = 100,
        OnlineSystemLimits_MaxNumTrackedDataChunks = 32,
        OnlineSystemLimits_MaxNumScoreDetails = 16,
    };

    enum RankingError
    {
        RankingError_None = 0,
        RankingError_NotOnline = 8192,
        RankingError_ConnectionFailed = 8193,
        RankingError_ForbiddenByParentalControl = 8196,
        RankingError_xxx = 8197,
        RankingError_FailedRetryOrDisable = 8198,
    };

    enum RankingListQueryType
    {
        RankingListQueryType_AllWithOffset = 0,
        RankingListQueryType_AllCenteredOnUser = 1,
        RankingListQueryType_FriendsWithOffset = 2,
        RankingListQueryType_FriendsCenteredOnUser = 3,
    };

    // size 0x44
    struct StatUpdateData
    {
        uint32 statId; // 0x00
        char statName[64]; // 0x04
    };

    // size 0x1C
    struct SystemServicesInteractionData
    {
        void clear(); // 0x00C12E10 (.h:234)
        void set(uint32, uint32, uint32);
        void set(uint32, uint32);
        bool isActive() const; // 0x00C12E20 (.h:238)

        uint32 id; // 0x00
        uint32 responseOptions[2]; // 0x04
        uint32 responseOptionCount; // 0x0C
        keen::UserAccountId user; // 0x10
    };

    // size 0x1
    struct SystemServicesCredentials
    {
    };

    // size 0xA
    struct RemoteUserId
    {
        bool is_equal(const RemoteUserId& id) const; // 0x008834E0 (.h:179)

        static const uint32 PROVIDERDATASIZE; // 0x012B80CC
        char providerData[8]; // 0x00
        uint8 guestIndex; // 0x08
        bool isValid; // 0x09
    };

    // size 0x4A
    struct NamedRemoteUserId
    {
        RemoteUserId id; // 0x00
        char displayName[64]; // 0x0A
    };

    // size 0x98
    struct RankingListEntry
    {
        void clear();

        NamedRemoteUserId userInfo; // 0x00
        uint32 rank; // 0x4C
        uint32 score; // 0x50
        uint32 scoreDetails[16]; // 0x54
        uint32 numScoreDetails; // 0x94
    };

    // size 0x24
    struct RankingBoardCacheEntry
    {
        void clear(); // 0x00C138A0 (systemservices_internal.hpp:22)
        RankingBoardCacheEntry(); // 0x00C147F0

        uint16 tableId; // 0x00
        RemoteUserId user; // 0x02
        RankingListQueryType type; // 0x0C
        keen::SizedArray<RankingListEntry> entries; // 0x10
        uint32 totalEntriesOnBoard; // 0x1C
        uint32 downloadTimeInMs; // 0x20
    };

    // size 0x28
    struct RankingReceiveData
    {
        RankingReceiveData(); // 0x00C14640 (:265)
        void createCaches(keen::MemoryAllocator* pAllocator, uint32 numCaches, uint32 resultSetSize); // 0x00C151B0 (:270)
        ~RankingReceiveData(); // 0x00C138B0

        keen::Array<RankingBoardCacheEntry> cachedBoards; // 0x00
        bool servingFromCache; // 0x08
        bool downloadInProgress; // 0x09
        uint16 tableId; // 0x0A
        RemoteUserId user; // 0x0C
        RankingListQueryType type; // 0x18
        uint32 offset; // 0x1C
        uint32 entriesToQuery; // 0x20
        uint32 cacheToUseForResult; // 0x24
    };

    // size 0x13C
    struct SystemServicesParameters
    {
        SystemServicesParameters(); // 0x00459730 (.h:79)

        const uint32* pAchievementIdMap; // 0x00
        const uint32* pAchievementUnlockCountMap; // 0x04
        uint32 platinumAchievementId; // 0x08
        uint32 achievementCount; // 0x0C
        const char* pPresenceStrings[64]; // 0x10
        uint32 presenceStringCount; // 0x110
        SystemServicesCredentials credentials; // 0x114
        uint32 rankingTableCount; // 0x118
        uint32 rankingCacheCount; // 0x11C
        uint32 rankingResultSetSize; // 0x120
        uint32 rankingSendQueueSize; // 0x124
        keen::SteamworksSystem* pSteamworksSystem; // 0x128
        const StatUpdateData* pStatUpdateMap; // 0x12C
        uint32 statUpdateCount; // 0x130
        const uint32* pTrackedChunksMap; // 0x134
        uint32 trackedChunksCount; // 0x138
    };

    // size 0xC
    struct RankingResult
    {
        RankingListEntry* pEntries; // 0x00
        uint32 filledEntriesCount; // 0x04
        uint32 totalRankingEntries; // 0x08
    };

    // size 0x54
    struct RankingSendData
    {
        uint16 tableId; // 0x00
        RemoteUserId user; // 0x02
        uint32 score; // 0x0C
        uint32 scoreDetails[16]; // 0x10
        uint32 numScoreDetails; // 0x50
    };

    namespace systemservices
    {
        bool updateHeroStatSet(SystemServices* pServices, const keen::UserAccount& userAccount, const uint32 statId, int newValue); // 0x00C11070 (.h:292)
        bool updateHeroStatAdd(SystemServices* pServices, const keen::UserAccount& userAccount, const uint32 statId, int addend); // 0x00C110A0 (.h:297)
        bool startSendRankingEntry(SystemServices* pServices, const RemoteUserId userId, uint16 tableId, uint32 score, uint32* scoreDetails, uint32 numScoreDetails); // 0x00C14F30 (:11)
        bool startGetRankingList(SystemServices* pServices, uint16 tableId, uint32 maxNumEntries, RankingListQueryType queryType, const RemoteUserId* pUserId, uint32 entriesOffset); // 0x00C14120 (:45)
        bool getRankingListResult(SystemServices* pServices, RankingResult* pResultSet, uint32 capacity); // 0x00C14370 (:160)
        void mergeSentScoreIntoCache(RankingReceiveData* pReceiveData, const RankingSendData& sendData, const RemoteUserId& systemUserId); // 0x00C14460 (:207)
        RankingError getRankingError(SystemServices* pServices); // 0x00C132F0 (:260)
        void updateSystemServices(SystemServices* pServices, float timeStep); // 0x00C15000 (:282)
        bool isInteractionRequired(SystemServicesInteractionData* pData, SystemServices* pServices); // 0x00C13300 (:312)
        void setupAchievements(SystemServices*, const keen::UserAccount&); // 0x00C13390 (:332)
    }
}
