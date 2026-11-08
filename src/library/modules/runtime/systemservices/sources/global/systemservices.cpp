// Original: library/modules/runtime/systemservices/sources/global/systemservices.cpp
//
// The platform independent half of the system services (stub_impl.md S2): the ranking (leaderboard) requests and
// their result cache, the send queue, the interactions and the per-frame update; the platform half is
// systemservices_steam.cpp. The inline members of systemservices.hpp and systemservices_internal.hpp are defined here
// as well (the public header declares no platform function).
//
// Deviation (OpenRFG with --no-steam): rfg.exe always has system services (Steam's). Without Steam OpenRFG creates
// none (gameframework.cpp); the functions the game calls answer for the null system at their first line, as the
// stand-ins did.

#include "unit4/systemservices/systemservices.hpp"
#include "keen/base/string.hpp"
#include "library/modules/runtime/systemservices/sources/steam/systemservices_steam.hpp"

#include <math.h>
#include <string.h>

namespace unit4
{
    // Magic number: 0xFFFF, a cache entry that holds no board.
    static const uint16 NoRankingTable = 0xFFFFu;

    // 0x008834E0 (.h:179): the provider data only (guestIndex and isValid are not compared)
    bool RemoteUserId::is_equal(const RemoteUserId& id) const
    {
        return memcmp(providerData, id.providerData, sizeof(providerData)) == 0;
    }

    // 0x00C12E10 (.h:234)
    void SystemServicesInteractionData::clear()
    {
        id = 0;
        user.isValid = false;
    }

    // 0x00C12E20 (.h:238)
    bool SystemServicesInteractionData::isActive() const
    {
        return id != 0;
    }

    // 0x00C138A0 (systemservices_internal.hpp:22)
    void RankingBoardCacheEntry::clear()
    {
        entries.clear();
        totalEntriesOnBoard = 0;
    }

    // 0x00C147F0
    RankingBoardCacheEntry::RankingBoardCacheEntry()
    {
    }

    // 0x00C14990 (systemservices_internal.hpp:51)
    SystemServicesBase::SystemServicesBase()
    {
        rankingSendInProgress = false;
        rankingSendInteraction.clear();
        onlineInteraction.clear();
        currentRankingError = RankingError_None;
    }

    // 0x00C14A10
    SystemServicesBase::~SystemServicesBase()
    {
    }

    // 0x00C14640 (:265)
    RankingReceiveData::RankingReceiveData()
    {
        downloadInProgress = false;
    }

    // 0x00C138B0
    RankingReceiveData::~RankingReceiveData()
    {
    }

    // 0x00C151B0 (:270): every cache empty, with room for one result set. Magic number: 16-byte aligned arrays.
    void RankingReceiveData::createCaches(keen::MemoryAllocator* pAllocator, uint32 numCaches, uint32 resultSetSize)
    {
        cachedBoards.create(pAllocator, numCaches, 0x10u);
        for (uint32 i = 0; i < cachedBoards.getSize(); ++i)
        {
            RankingBoardCacheEntry& board = cachedBoards[i];
            board.entries.create(pAllocator, resultSetSize, 0x10u);
            board.tableId = NoRankingTable;
            board.totalEntriesOnBoard = 0;
            memset(&board.user, 0, sizeof(board.user));
        }
    }

    namespace systemservices
    {
        // 0x00C11070 (.h:292)
        bool updateHeroStatSet(SystemServices* pServices, const keen::UserAccount& userAccount, const uint32 statId, int newValue)
        {
            return updateHeroStatSet(pServices, userAccount, statId, (double)newValue);
        }

        // 0x00C110A0 (.h:297)
        bool updateHeroStatAdd(SystemServices* pServices, const keen::UserAccount& userAccount, const uint32 statId, int addend)
        {
            return updateHeroStatAdd(pServices, userAccount, statId, (double)addend);
        }

        // 0x00C14F30 (:11): queued; a full queue drops its oldest entry
        bool startSendRankingEntry(SystemServices* pServices, const RemoteUserId userId, uint16 tableId, uint32 score, uint32* scoreDetails, uint32 numScoreDetails)
        {
            if (pServices == nullptr) // deviation: no Steam
            {
                return false;
            }
            RankingSendData sendData;
            sendData.tableId = tableId;
            sendData.user = userId;
            sendData.score = score;
            if (numScoreDetails > OnlineSystemLimits_MaxNumScoreDetails)
            {
                numScoreDetails = OnlineSystemLimits_MaxNumScoreDetails;
            }
            sendData.numScoreDetails = numScoreDetails;
            for (uint32 i = 0; i < numScoreDetails; ++i)
            {
                sendData.scoreDetails[i] = scoreDetails[i];
            }
            keen::Queue<RankingSendData>& queue = pServices->rankingSendDataQueue;
            if (queue.getSize() != 0 && queue.isFull())
            {
                queue.pop();
            }
            queue.push(sendData);
            return true;
        }

        // 0x00C14120 (:45): the board goes into the matching cache entry, else a free one, else the oldest. An error
        // since the last request empties every cache first.
        bool startGetRankingList(SystemServices* pServices, uint16 tableId, uint32 maxNumEntries, RankingListQueryType queryType, const RemoteUserId* pUserId, uint32 entriesOffset)
        {
            if (pServices == nullptr) // deviation: no Steam
            {
                return false;
            }
            RankingReceiveData& receiveData = pServices->rankingReceiveData;
            if (receiveData.downloadInProgress)
            {
                return false;
            }
            const bool hadError = pServices->currentRankingError != RankingError_None;
            receiveData.tableId = tableId;
            pServices->currentRankingError = RankingError_None;
            receiveData.type = queryType;
            receiveData.servingFromCache = false;
            if (pUserId != nullptr)
            {
                receiveData.user = *pUserId;
            }
            receiveData.offset = entriesOffset;
            receiveData.entriesToQuery = maxNumEntries;
            if (hadError)
            {
                for (uint32 i = 0; i < receiveData.cachedBoards.getSize(); ++i)
                {
                    receiveData.cachedBoards[i].clear();
                    receiveData.cachedBoards[i].tableId = NoRankingTable;
                }
            }
            if (!startGetRankingListPlatform(pServices))
            {
                receiveData.downloadInProgress = false;
                return false;
            }
            uint32 cacheIndex = 0xFFFFFFFFu;
            for (uint32 i = 0; i < receiveData.cachedBoards.getSize(); ++i)
            {
                const RankingBoardCacheEntry& board = receiveData.cachedBoards[i];
                if (board.tableId == tableId && board.type == queryType && (queryType == RankingListQueryType_AllWithOffset || (pUserId != nullptr && pUserId->is_equal(board.user))))
                {
                    cacheIndex = i;
                    break;
                }
                if (board.tableId == NoRankingTable && cacheIndex == 0xFFFFFFFFu)
                {
                    cacheIndex = i;
                }
            }
            if (cacheIndex == 0xFFFFFFFFu)
            {
                uint32 oldestAge = 0;
                for (uint32 i = 0; i < receiveData.cachedBoards.getSize(); ++i)
                {
                    const uint32 age = pServices->currentTimeInMs - receiveData.cachedBoards[i].downloadTimeInMs;
                    if (age > oldestAge)
                    {
                        oldestAge = age;
                        cacheIndex = i;
                    }
                }
            }
            if (cacheIndex == 0xFFFFFFFFu)
            {
                cacheIndex = 0;
            }
            receiveData.cacheToUseForResult = cacheIndex;
            receiveData.downloadInProgress = true;
            receiveData.cachedBoards[cacheIndex].tableId = NoRankingTable;
            return true;
        }

        // 0x00C14370 (:160): once the download finished, the board is cached and up to capacity entries copied out
        // (the rest of the result set cleared)
        bool getRankingListResult(SystemServices* pServices, RankingResult* pResultSet, uint32 capacity)
        {
            if (pServices == nullptr) // deviation: no Steam
            {
                return false;
            }
            RankingReceiveData& receiveData = pServices->rankingReceiveData;
            if (!receiveData.downloadInProgress || (!receiveData.servingFromCache && !getRankingListResultPlatform(pServices)))
            {
                return false;
            }
            receiveData.downloadInProgress = false;
            RankingBoardCacheEntry& board = receiveData.cachedBoards[receiveData.cacheToUseForResult];
            board.tableId = receiveData.tableId;
            board.type = receiveData.type;
            board.user = receiveData.user;
            board.downloadTimeInMs = pServices->currentTimeInMs;
            pResultSet->totalRankingEntries = board.totalEntriesOnBoard;
            const uint32 count = board.entries.getSize() < capacity ? board.entries.getSize() : capacity;
            pResultSet->filledEntriesCount = count;
            for (uint32 i = 0; i < count; ++i)
            {
                pResultSet->pEntries[i] = board.entries[i];
            }
            for (uint32 i = count; i < capacity; ++i)
            {
                pResultSet->pEntries[i].rank = 0;
                pResultSet->pEntries[i].score = 0;
                pResultSet->pEntries[i].userInfo.displayName[0] = '\0';
            }
            return true;
        }

        // 0x00C14460 (:207): the sent score placed into the cached boards of its table (no caller in rfg.exe). Original
        // bugs kept: the user's old entry is looked up by address, not by id, so it is never found; the new entry's
        // score details are left uninitialized.
        void mergeSentScoreIntoCache(RankingReceiveData* pReceiveData, const RankingSendData& sendData, const RemoteUserId& systemUserId)
        {
            for (uint32 boardIndex = 0; boardIndex < pReceiveData->cachedBoards.getSize(); ++boardIndex)
            {
                RankingBoardCacheEntry& board = pReceiveData->cachedBoards[boardIndex];
                if (board.tableId != sendData.tableId)
                {
                    continue;
                }
                keen::SizedArray<RankingListEntry>& entries = board.entries;
                for (RankingListEntry* pEntry = entries.getStart(); pEntry != entries.getEnd(); ++pEntry)
                {
                    if ((const void*)pEntry == (const void*)&systemUserId)
                    {
                        entries.erase(pEntry);
                        break;
                    }
                }
                if (entries.getSize() == entries.getCapacity())
                {
                    entries.popBack();
                }
                RankingListEntry newEntry;
                newEntry.score = sendData.score;
                newEntry.userInfo.id = systemUserId;
                newEntry.rank = 0;
                // Magic string: a placeholder name until the board is downloaded again.
                keen::copyString(newEntry.userInfo.displayName, sizeof(newEntry.userInfo.displayName), "he_who");
                // Descending by score: insert before the first lower score.
                RankingListEntry* pInsert = entries.getStart();
                for (; pInsert != entries.getEnd(); ++pInsert)
                {
                    if (sendData.score > pInsert->score)
                    {
                        newEntry.rank = pInsert->rank - 1u;
                        if (newEntry.rank < 2u)
                        {
                            newEntry.rank = 1u;
                        }
                        break;
                    }
                    newEntry.rank = pInsert->rank + 1u;
                }
                if (pInsert == entries.getEnd())
                {
                    entries.pushBack(newEntry);
                }
                else
                {
                    entries.insert(pInsert, newEntry);
                }
            }
        }

        // 0x00C132F0 (:260)
        RankingError getRankingError(SystemServices* pServices)
        {
            if (pServices == nullptr) // deviation: no Steam
            {
                return RankingError_None;
            }
            return pServices->currentRankingError;
        }

        // 0x00C15000 (:282): the next queued ranking is sent when none is in flight (its table's caches are emptied),
        // then the time and the platform part advance
        void updateSystemServices(SystemServices* pServices, float timeStep)
        {
            if (pServices == nullptr) // deviation: no Steam
            {
                return;
            }
            if (!pServices->rankingSendInProgress && pServices->rankingSendDataQueue.getSize() != 0)
            {
                pServices->currentRankingSendData = pServices->rankingSendDataQueue.peek();
                if (startSendRankingEntryPlatform(pServices))
                {
                    pServices->rankingSendDataQueue.pop();
                    for (uint32 i = 0; i < pServices->rankingReceiveData.cachedBoards.getSize(); ++i)
                    {
                        RankingBoardCacheEntry& board = pServices->rankingReceiveData.cachedBoards[i];
                        if (board.tableId == pServices->currentRankingSendData.tableId)
                        {
                            board.clear();
                            board.tableId = NoRankingTable;
                        }
                    }
                    pServices->currentRankingError = RankingError_None;
                    pServices->rankingSendInProgress = true;
                }
            }
            // Magic number: seconds to milliseconds, rounded to nearest as rfg.exe's x87 conversion (fistp) does.
            pServices->currentTimeInMs += (uint32)(int)lrint((double)timeStep * 1000.0);
            updateSystemServicesPlatform(pServices, timeStep);
        }

        // 0x00C13300 (:312): the ranking send interaction first, then the online one
        bool isInteractionRequired(SystemServicesInteractionData* pData, SystemServices* pServices)
        {
            if (pServices == nullptr) // deviation: no Steam
            {
                return false;
            }
            if (pServices->rankingSendInteraction.isActive())
            {
                *pData = pServices->rankingSendInteraction;
                return true;
            }
            if (pServices->onlineInteraction.isActive())
            {
                *pData = pServices->onlineInteraction;
                return true;
            }
            return false;
        }

        // 0x00C13390 (:332)
        void setupAchievements(SystemServices*, const keen::UserAccount&)
        {
        }
    }
}
