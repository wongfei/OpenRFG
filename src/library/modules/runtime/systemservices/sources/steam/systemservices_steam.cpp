// Original: library/modules/runtime/systemservices/sources/steam/systemservices_steam.cpp
//
// The Steam half of the system services (stub_impl.md S2): achievements and stats through Steam user stats, the
// leaderboards ("LB<table>": find, then upload or download, as asynchronous Steam calls polled every frame), rich
// presence, the friends overlay, and Windows' network list manager for the network check.
//
// Deviation (OpenRFG with --no-steam): see systemservices.cpp; the functions the game calls answer for the null
// system at their first line.

#include "library/modules/runtime/systemservices/sources/steam/systemservices_steam.hpp"
#include "keen/base/string.hpp"
#include "keen/base/trace.hpp"
#include "keen/steam/steamworkssystem.hpp"
#include "library/modules/runtime/systemservices/sources/steam/onlinehelpers_steam.hpp"
#include "library/modules/thirdparty/emgiec/emgiec_steam_142/sdk/public/steam/steam_api_internal.h"

#include <new>
#include <string.h>
#include <windows.h>
#include <netlistmgr.h>

namespace unit4
{
    // 0x00C13190 (:78)
    static void createLeadboardName(char* pBuffer, uint32 capacity, uint16 tableId)
    {
        keen::formatString(pBuffer, capacity, "LB%d", tableId);
    }

    // 0x00C14E80
    SystemServices::SystemServices()
    {
    }

    // 0x00C14BD0
    SystemServices::~SystemServices()
    {
    }

    namespace systemservices
    {
        // Magic number: room for "LB<table>".
        static const uint32 LeaderboardNameSize = 16u;

        // 0x00C152F0 (:23)
        SystemServices* createSystemServices(keen::MemoryAllocator* pAllocator, const SystemServicesParameters* pParams, keen::UserAccountSystem*)
        {
            // Magic number: 8-byte aligned (the Steam call handles).
            void* pMemory = pAllocator->allocate(sizeof(SystemServices), 8u, 0u, nullptr);
            SystemServices* pServices = pMemory != nullptr ? new (pMemory) SystemServices() : nullptr;
            pServices->steamAchievements.create(keen::getAppId(pParams->pSteamworksSystem), pParams->pAchievementIdMap, pParams->pAchievementUnlockCountMap, pParams->achievementCount, pParams->platinumAchievementId);
            pServices->steamStats.create(keen::getAppId(pParams->pSteamworksSystem), pParams->pStatUpdateMap, pParams->statUpdateCount);
            for (uint32 i = 0; i < OnlineSystemLimits_MaxPresenceStrings; ++i)
            {
                pServices->pPresenceStrings[i] = pParams->pPresenceStrings[i];
            }
            pServices->presenceStringCount = pParams->presenceStringCount;
            if (pParams->rankingTableCount != 0)
            {
                pServices->rankingReceiveData.createCaches(pAllocator, pParams->rankingCacheCount, pParams->rankingResultSetSize);
            }
            // Magic number: 16-byte aligned queue.
            pServices->rankingSendDataQueue.create(pAllocator, pParams->rankingSendQueueSize, 0x10u);
            pServices->currentRankingError = RankingError_None;
            pServices->currentTimeInMs = 0;
            pServices->sendRankingStep = SystemServices::SendRankingStep_Invalid;
            pServices->sendRankingCall = 0;
            pServices->receiveRankingStep = SystemServices::ReceiveRankingStep_Invalid;
            pServices->receiveRankingCall = 0;
            CoInitializeEx(nullptr, COINIT_MULTITHREADED);
            if (FAILED(CoCreateInstance(CLSID_NetworkListManager, nullptr, CLSCTX_ALL, IID_INetworkListManager, (void**)&pServices->pNetworkListManager)))
            {
                keen::debug::nativePrintArgs("[systemservices] Failed creating INetworkListManager instance!\n");
                pServices->~SystemServices();
                pAllocator->free(pServices);
                return nullptr;
            }
            return pServices;
        }

        // 0x00C15580 (:68). Original behaviour kept: the ranking caches and the send queue are not freed.
        void destroySystemServices(keen::MemoryAllocator* pAllocator, SystemServices* pServices)
        {
            pServices->pNetworkListManager->Release();
            pServices->steamAchievements.destroy();
            pServices->steamStats.destroy();
            pServices->~SystemServices();
            pAllocator->free(pServices);
        }

        // 0x00C13A50 (:83): Steam's callbacks, the ranking upload's steps, the stats
        void updateSystemServicesPlatform(SystemServices* pServices, float timeStep)
        {
            SW_CCSys_ProcessApiCb();
            if (pServices->sendRankingCall != 0)
            {
                bool failed;
                if (SteamUtils()->IsAPICallCompleted(pServices->sendRankingCall, &failed))
                {
                    bool done = true;
                    if (!failed)
                    {
                        const SystemServices::SendRankingStep step = pServices->sendRankingStep;
                        if (step == SystemServices::SendRankingStep_FindLeaderboard)
                        {
                            LeaderboardFindResult_t findResult;
                            if (SteamUtils()->GetAPICallResult(pServices->sendRankingCall, &findResult, sizeof(findResult), LeaderboardFindResult_t::k_iCallback, &failed))
                            {
                                const RankingSendData& sendData = pServices->currentRankingSendData;
                                pServices->sendRankingCall = SteamUserStats()->UploadLeaderboardScore(findResult.m_hSteamLeaderboard, k_ELeaderboardUploadScoreMethodKeepBest, (int)sendData.score,
                                                                                                      (const int*)sendData.scoreDetails, (int)sendData.numScoreDetails);
                                pServices->sendRankingStep = SystemServices::SendRankingStep_UploadScore;
                            }
                            done = false;
                        }
                        else
                        {
                            pServices->sendRankingCall = 0;
                            done = step == SystemServices::SendRankingStep_UploadScore;
                        }
                    }
                    else
                    {
                        pServices->sendRankingCall = 0;
                    }
                    if (done)
                    {
                        pServices->sendRankingStep = SystemServices::SendRankingStep_Invalid;
                        pServices->rankingSendInProgress = false;
                    }
                }
            }
            pServices->steamStats.update(timeStep);
        }

        // 0x00C131B0 (:150)
        bool doesSystemHaveNetworkAccess(SystemServices* pServices)
        {
            if (pServices == nullptr) // deviation: no Steam (the tests run on loopback, which needs no network)
            {
                return true;
            }
            VARIANT_BOOL isConnected = VARIANT_FALSE;
            return SUCCEEDED(pServices->pNetworkListManager->get_IsConnected(&isConnected)) && isConnected == VARIANT_TRUE;
        }

        // 0x00C13B90 (:168): guests have no achievements
        bool updateAchievementProgress(SystemServices* pServices, const keen::UserAccount& userAccount, uint32 achievementId, uint32 status)
        {
            if (pServices == nullptr) // deviation: no Steam
            {
                return false;
            }
            if (userAccount.guestIndex != 0)
            {
                return false;
            }
            // Magic number: 0x7FFFFFFF, the highest status (completed).
            if (status > 0x7FFFFFFEu)
            {
                status = 0x7FFFFFFFu;
            }
            return pServices->steamAchievements.updateAchievementStatus(achievementId, status);
        }

        // 0x00C131F0 (:179): Steam has its own overlay
        void showAchievementsUi(const keen::UserAccount&)
        {
        }

        // 0x00C13BD0 (:185): the Steam overlay's profile of the user
        void showRemoteUserUi(SystemServices*, const keen::LocalPlayerIdStructureType*, const NamedRemoteUserId& userInfo)
        {
            const CSteamID steamId = convertToCSteamID(userInfo.id);
            if (steamId.IsValid())
            {
                SteamFriends()->ActivateGameOverlayToUser("steamid", steamId);
            }
        }

        // 0x00C13C20 (:198): the presence string as the "status" rich presence
        bool setPresence(SystemServices* pServices, uint32 stringIndex)
        {
            if (pServices == nullptr) // deviation: no Steam
            {
                return false;
            }
            if (stringIndex >= pServices->presenceStringCount)
            {
                return false;
            }
            return SteamFriends()->SetRichPresence("status", pServices->pPresenceStrings[stringIndex]);
        }

        // 0x00C13C60 (:212): the download starts by finding the board
        bool startGetRankingListPlatform(SystemServices* pServices)
        {
            if (pServices->receiveRankingCall != 0)
            {
                return false;
            }
            char name[LeaderboardNameSize];
            createLeadboardName(name, sizeof(name), pServices->rankingReceiveData.tableId);
            pServices->receiveRankingCall = SteamUserStats()->FindLeaderboard(name);
            pServices->receiveRankingStep = SystemServices::ReceiveRankingStep_FindLeaderboard;
            return true;
        }

        // 0x00C13CF0 (:228): true once the download finished (or failed); the entries go into the result cache
        bool getRankingListResultPlatform(SystemServices* pServices)
        {
            if (pServices->receiveRankingCall == 0)
            {
                return true;
            }
            bool failed;
            if (!SteamUtils()->IsAPICallCompleted(pServices->receiveRankingCall, &failed) || failed)
            {
                return false;
            }
            switch (pServices->receiveRankingStep)
            {
            case SystemServices::ReceiveRankingStep_Invalid:
                pServices->receiveRankingCall = 0;
                return true;
            case SystemServices::ReceiveRankingStep_FindLeaderboard:
            {
                LeaderboardFindResult_t findResult;
                if (!SteamUtils()->GetAPICallResult(pServices->receiveRankingCall, &findResult, sizeof(findResult), LeaderboardFindResult_t::k_iCallback, &failed))
                {
                    return false;
                }
                const RankingReceiveData& receiveData = pServices->rankingReceiveData;
                // The board's ranks are 1-based.
                const int rangeStart = (int)receiveData.offset + 1;
                const int rangeEnd = (int)(receiveData.entriesToQuery + receiveData.offset);
                switch (receiveData.type)
                {
                case RankingListQueryType_AllWithOffset:
                    pServices->receiveRankingCall = SteamUserStats()->DownloadLeaderboardEntries(findResult.m_hSteamLeaderboard, k_ELeaderboardDataRequestGlobal, rangeStart, rangeEnd);
                    break;
                case RankingListQueryType_AllCenteredOnUser:
                    pServices->receiveRankingCall = SteamUserStats()->DownloadLeaderboardEntries(findResult.m_hSteamLeaderboard, k_ELeaderboardDataRequestGlobalAroundUser, rangeStart, rangeEnd);
                    break;
                case RankingListQueryType_FriendsWithOffset:
                case RankingListQueryType_FriendsCenteredOnUser:
                    pServices->receiveRankingCall = SteamUserStats()->DownloadLeaderboardEntries(findResult.m_hSteamLeaderboard, k_ELeaderboardDataRequestFriends, rangeStart, rangeEnd);
                    break;
                default:
                    break;
                }
                pServices->receiveRankingStep = SystemServices::ReceiveRankingStep_DownloadLeaderboard;
                return false;
            }
            case SystemServices::ReceiveRankingStep_DownloadLeaderboard:
            {
                LeaderboardScoresDownloaded_t downloaded;
                if (!SteamUtils()->GetAPICallResult(pServices->receiveRankingCall, &downloaded, sizeof(downloaded), LeaderboardScoresDownloaded_t::k_iCallback, &failed))
                {
                    // Original behaviour kept: the step stays (the next request starts by finding the board anyway).
                    pServices->receiveRankingCall = 0;
                    return true;
                }
                RankingBoardCacheEntry& board = pServices->rankingReceiveData.cachedBoards[pServices->rankingReceiveData.cacheToUseForResult];
                board.entries.clear();
                board.totalEntriesOnBoard = 0;
                const int count = downloaded.m_cEntryCount < (int)board.entries.getCapacity() ? downloaded.m_cEntryCount : (int)board.entries.getCapacity();
                for (int i = 0; i < count; ++i)
                {
                    LeaderboardEntry_t entry;
                    RankingListEntry rankingEntry;
                    SteamUserStats()->GetDownloadedLeaderboardEntry(downloaded.m_hSteamLeaderboardEntries, i, &entry, (int*)rankingEntry.scoreDetails, OnlineSystemLimits_MaxNumScoreDetails);
                    rankingEntry.rank = (uint32)entry.m_nGlobalRank;
                    rankingEntry.score = (uint32)entry.m_nScore;
                    rankingEntry.numScoreDetails = (uint32)entry.m_cDetails;
                    keen::copyString(rankingEntry.userInfo.displayName, sizeof(rankingEntry.userInfo.displayName), SteamFriends()->GetFriendPersonaName(entry.m_steamIDUser));
                    rankingEntry.userInfo.id = convertToOnlineSystemUserId(entry.m_steamIDUser, 0);
                    board.entries.pushBack(rankingEntry);
                }
                board.totalEntriesOnBoard = (uint32)SteamUserStats()->GetLeaderboardEntryCount(downloaded.m_hSteamLeaderboard);
                pServices->receiveRankingCall = 0;
                pServices->receiveRankingStep = SystemServices::ReceiveRankingStep_Invalid;
                return true;
            }
            default:
                return false;
            }
        }

        // 0x00C14060 (:321): the upload starts by finding the board (updateSystemServicesPlatform does the rest)
        bool startSendRankingEntryPlatform(SystemServices* pServices)
        {
            if (pServices->sendRankingCall != 0)
            {
                return false;
            }
            char name[LeaderboardNameSize];
            createLeadboardName(name, sizeof(name), pServices->currentRankingSendData.tableId);
            pServices->sendRankingCall = SteamUserStats()->FindLeaderboard(name);
            pServices->sendRankingStep = SystemServices::SendRankingStep_FindLeaderboard;
            return true;
        }

        // 0x00C140F0 (:337): the account's provider data (the Steam id); an invalid account gives an invalid id, whose
        // other fields are left as they are
        void createRemoteUserIdFromUserAccountId(RemoteUserId* pResult, keen::UserAccountId id)
        {
            if (!id.isValid)
            {
                pResult->isValid = false;
                return;
            }
            memcpy(pResult->providerData, id.providerData, sizeof(pResult->providerData));
            pResult->guestIndex = 0;
            pResult->isValid = true;
        }

        // 0x00C13220 (:355)
        void createUserAccountIdFromRemoteUserId(keen::UserAccountId* pResult, const RemoteUserId& id)
        {
            if (!id.isValid)
            {
                pResult->isValid = false;
                return;
            }
            memcpy(pResult->providerData, id.providerData, sizeof(pResult->providerData));
            pResult->isValid = true;
        }

        // 0x00C13250 (:371): any answer ends the ranking send interaction
        void setInteractionResponse(SystemServices* pServices, uint32)
        {
            if (pServices == nullptr) // deviation: no Steam
            {
                return;
            }
            if (pServices->rankingSendInteraction.isActive())
            {
                pServices->rankingSendInProgress = false;
                pServices->rankingSendInteraction.clear();
            }
        }

        // 0x00C13270 (:383)
        bool updateHeroStatSet(SystemServices* pServices, const keen::UserAccount&, const uint32 statId, double newValue)
        {
            if (pServices == nullptr) // deviation: no Steam
            {
                return false;
            }
            return pServices->steamStats.updateHeroStatSet(statId, newValue);
        }

        // 0x00C132A0 (:393)
        bool updateHeroStatAdd(SystemServices* pServices, const keen::UserAccount&, const uint32 statId, double addend)
        {
            if (pServices == nullptr) // deviation: no Steam
            {
                return false;
            }
            return pServices->steamStats.updateHeroStatAdd(statId, addend);
        }

        // 0x00C132D0 (:406): Steam installs every chunk up front
        bool isDataChunkAvailable(SystemServices*, const uint32, float*)
        {
            return true;
        }

        // 0x00C132E0 (:426)
        void prioritizeDataChunk(SystemServices*, const uint32)
        {
        }
    }
}
