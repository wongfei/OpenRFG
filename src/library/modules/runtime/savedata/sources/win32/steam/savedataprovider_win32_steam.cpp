// Original: library/modules/runtime/savedata/sources/win32/steam/savedataprovider_win32_steam.cpp
//
// The Steam save data provider (stub_impl.md S2): save data are files "autocloud/save/keen_savegame_<format>_<id>.sav"
// in Steam's remote storage (Steam Cloud), one per installment of a format, each the format's segments back to back.
// Reads go through a helper thread (FileRead blocks); everything else runs in the caller's update. The single profile
// is the Steam user; one user can be connected.

#include "library/modules/runtime/savedata/sources/win32/steam/savedataprovider_win32_steam.hpp"
#include "keen/base/filepath.hpp"
#include "keen/base/memoryallocator.hpp"
#include "keen/base/string.hpp"
#include "keen/base/stringscanner.hpp"
#include "keen/base/stringtokenizer.hpp"
#include "keen/base/trace.hpp"
#include "keen/steam/steamworkssystem.hpp"
#include "library/modules/thirdparty/emgiec/emgiec_steam_142/sdk/public/steam/steam_api_internal.h"

#include <string.h>

namespace keen
{
    namespace SaveData
    {
        // Magic number: 0xFFFF, no connected profile.
        static const uint32 NoConnectedProfile = 0xFFFFu;

        // 0x00CCD240
        SaveDataProviderWin32Interface::SaveDataProviderWin32Interface()
        {
        }

        // 0x00CCCCD0 (.h:34)
        SaveDataProviderWin32Interface::~SaveDataProviderWin32Interface()
        {
        }

        // 0x00CCE8F0
        SaveDataProviderWin32Steam::SaveDataFormatSteam::SaveDataFormatSteam()
        {
        }

        // 0x00CCE910
        SaveDataProviderWin32Steam::SaveDataFormatSteam::~SaveDataFormatSteam()
        {
        }

        // 0x00CCDED0 (:21)
        SaveDataProviderWin32Steam::SaveDataProviderWin32Steam()
        {
            pSteamStorage = nullptr;
            pMemoryAllocator = nullptr;
            connectedUserAccountId.isValid = false;
            asyncReadStarted = false;
            connectedUserProfileIndex = NoConnectedProfile;
            memset(connectedUserAccountId.providerData, 0, sizeof(connectedUserAccountId.providerData));
        }

        // 0x00CCDF40 (.h:20)
        SaveDataProviderWin32Steam::~SaveDataProviderWin32Steam()
        {
        }

        // 0x00CCF830 (:31): the formats' segment layouts and the read helper thread
        bool SaveDataProviderWin32Steam::create(MemoryAllocator* pAllocator, const SaveDataSystemCreationParameters& parameters)
        {
            pMemoryAllocator = pAllocator;
            if (!isValidSteamworksSystem(((const SaveDataSystemCreationProviderParametersWin32*)parameters.pProviderParameters)->pSteamworksSystem))
            {
                debug::nativePrintArgs("[savedata] Steam provider couldn't be created because steamworks system is not valid.\n");
                return false;
            }
            pSteamStorage = SteamRemoteStorage();
            if (pSteamStorage == nullptr)
            {
                debug::nativePrintArgs("SteamRemoteStorage() return nullptr!\n");
            }
            // Magic number: 16-byte aligned arrays.
            saveDataFormats.create(pAllocator, parameters.saveDataFormatCount, 0x10u);
            for (uint32 i = 0; i < parameters.saveDataFormatCount; ++i)
            {
                const SaveDataFormat& format = parameters.pSaveDataFormats[i];
                SaveDataFormatSteam& formatSteam = saveDataFormats[i];
                formatSteam.minimumInstallments = format.minimumInstallments;
                formatSteam.maximumInstallments = format.maximumInstallments;
                formatSteam.segments.create(pAllocator, format.numSegments, 0x10u);
                formatSteam.totalSize = 0;
                for (uint32 j = 0; j < format.numSegments; ++j)
                {
                    formatSteam.segments[j].size = format.pSegmentSizes[j];
                    formatSteam.segments[j].offset = formatSteam.totalSize;
                    formatSteam.totalSize += format.pSegmentSizes[j];
                }
            }
            helperThreadEvent.create("Steam Save Helper Event", 0u);
            // Magic numbers: priority 0, a 4 KB stack, any core.
            helperThread.create("Steam Save Helper", steamSaveDoWork, 0, 0x1000u, 0xFFFFu);
            helperThread.start(this);
            return true;
        }

        // 0x00CD0020 (:82). Original behaviour kept: the helper event is not destroyed.
        // Deviation: retail signals the helper event before it requests the quit. The event is auto-reset, so a helper
        // that wakes between the two (e.g. preempting this thread on the same core) consumes the signal, still sees no
        // quit request, sleeps again, and the infinite join below never returns (seen as a hang on game exit). The quit
        // is requested first here: whether the helper is asleep, in its loop body, or between its quit test and the
        // wait, the signal stays set until it is consumed, so the helper always reaches the quit test after the request.
        void SaveDataProviderWin32Steam::destroy(MemoryAllocator* pAllocator)
        {
            helperThread.requestQuit();
            helperThreadEvent.signal();
            helperThread.join(0xFFFFFFFFu);
            helperThread.destroy();
            for (uint32 i = 0; i < saveDataFormats.getSize(); ++i)
            {
                saveDataFormats[i].segments.destroy(pAllocator);
            }
            saveDataFormats.destroy(pAllocator);
            pSteamStorage = nullptr;
            pMemoryAllocator = nullptr;
        }

        // 0x00CCD250 (:102): the helper thread runs the blocking read the update started
        int SaveDataProviderWin32Steam::steamSaveDoWork(const Thread& thread)
        {
            SaveDataProviderWin32Steam* pProvider = (SaveDataProviderWin32Steam*)thread.getArgument();
            if (pProvider == nullptr)
            {
                return 0;
            }
            while (!thread.isQuitRequested())
            {
                pProvider->helperThreadEvent.sleepUntilSignaled();
                if (pProvider->asyncReadStarted && !pProvider->asyncReadFinished)
                {
                    pProvider->asyncReadBytes = pProvider->pSteamStorage->FileRead(pProvider->readFileName, pProvider->pReadBuffer, pProvider->asyncRequestedSize);
                    pProvider->asyncReadFinished = true;
                }
            }
            return 0;
        }

        // 0x00CCDFB0 (:124): one profile, named after the Steam user
        bool SaveDataProviderWin32Steam::enumerateProfiles(SizedArray<Profile>* pResult)
        {
            Profile profile;
            profile.isUsed = true;
            copyString(profile.name, sizeof(profile.name), SteamFriends()->GetPersonaName());
            pResult->clear();
            pResult->pushBack(profile);
            return true;
        }

        // 0x00CCEE80 (:144)
        bool SaveDataProviderWin32Steam::enumerateSaveData(SizedArray<SaveData>* pResult, uint32 profileIndex, uint32 formatIndex)
        {
            OperationResult result;
            return enumerateSaveData(&result, pResult, profileIndex, formatIndex);
        }

        // 0x00CCE020 (:150): the save data files of the format with the format's size (a file of another size makes
        // the result Corrupted unless a good one was found)
        bool SaveDataProviderWin32Steam::enumerateSaveData(OperationResult* pResultCode, SizedArray<SaveData>* pResult, uint32, uint32 formatIndex)
        {
            pResult->clear();
            *pResultCode = OperationResult_Missing;
            if (pSteamStorage == nullptr)
            {
                return true;
            }
            const int fileCount = pSteamStorage->GetFileCount();
            for (int i = 0; i < fileCount; ++i)
            {
                int fileSize;
                FilePath path;
                path.setCompletePath(pSteamStorage->GetFileNameAndSize(i, &fileSize), false);
                SaveData saveData;
                if (!getSaveDataInfoFromFileName(&saveData, path.getFileNameWithExtension()) || saveData.formatIndex != formatIndex || pResult->getSize() == pResult->getCapacity())
                {
                    continue;
                }
                const uint32 storedSize = (uint32)pSteamStorage->GetFileSize(path.getCompletePath());
                if (storedSize == saveDataFormats[formatIndex].totalSize)
                {
                    pResult->pushBack(saveData);
                    *pResultCode = OperationResult_Ok;
                }
                else
                {
                    debug::nativePrintArgs("[savedata] %i of %u bytes read from file '%s'.\n", storedSize, saveDataFormats[formatIndex].totalSize, path.getCompletePath());
                    if (*pResultCode != OperationResult_Ok)
                    {
                        *pResultCode = OperationResult_Corrupted;
                    }
                }
            }
            return true;
        }

        // 0x00CCD2C0 (:234): one user at a time
        bool SaveDataProviderWin32Steam::connectProfileWithUser(uint32 profileIndex, UserAccountId userId)
        {
            if (connectedUserProfileIndex == NoConnectedProfile)
            {
                connectedUserAccountId = userId;
                connectedUserProfileIndex = profileIndex;
                return true;
            }
            if (memcmp(userId.providerData, connectedUserAccountId.providerData, sizeof(userId.providerData)) != 0)
            {
                debug::nativePrintArgs("[savedata] Can't connect a second user.\n");
                return false;
            }
            debug::nativePrintArgs("[savedata] This user is already connected.\n");
            return false;
        }

        // 0x00CCE1C0 (:256)
        bool SaveDataProviderWin32Steam::disconnectProfile(uint32 profileIndex)
        {
            if (connectedUserProfileIndex == NoConnectedProfile)
            {
                debug::nativePrintArgs("[savedata] No user connected.\n");
                return false;
            }
            if (connectedUserProfileIndex != profileIndex)
            {
                debug::nativePrintArgs("[savedata] Profile with index %u is not connected.\n", profileIndex);
                return false;
            }
            connectedUserProfileIndex = NoConnectedProfile;
            memset(connectedUserAccountId.providerData, 0, sizeof(connectedUserAccountId.providerData));
            return true;
        }

        // 0x00CCD3D0 (:276)
        bool SaveDataProviderWin32Steam::isProfileConnected(uint32 profileIndex) const
        {
            return connectedUserProfileIndex == profileIndex;
        }

        // 0x00CCE220 (:281)
        void SaveDataProviderWin32Steam::updateCreateProfile(uint32, const Profile&)
        {
            debug::nativePrintArgs("[savedata] Provider doesn't support creating profiles.\n");
            setOperationFinished(OperationResult_Error);
        }

        // 0x00CCE240 (:289)
        void SaveDataProviderWin32Steam::updateUpdateProfile(uint32, const Profile&)
        {
            debug::nativePrintArgs("[savedata] Provider doesn't support updating profiles.\n");
            setOperationFinished(OperationResult_Error);
        }

        // 0x00CCE260 (:297)
        void SaveDataProviderWin32Steam::updateEraseProfile(uint32)
        {
            debug::nativePrintArgs("[savedata] Provider doesn't support erasing profiles.\n");
            setOperationFinished(OperationResult_Error);
        }

        // 0x00CCD3F0 (:304)
        void SaveDataProviderWin32Steam::updateConnectProfile(uint32, const Profile&)
        {
        }

        // 0x00CCD400 (:310)
        void SaveDataProviderWin32Steam::updateDisconnectProfile(uint32, const Profile&)
        {
        }

        // 0x00CCEEA0 (:316): every format has at least its minimum installments of good files
        void SaveDataProviderWin32Steam::updateCheckSaveDataSet(uint32 profileIndex)
        {
            setOperationBusy();
            for (uint32 formatIndex = 0; formatIndex < saveDataFormats.getSize(); ++formatIndex)
            {
                const SaveDataFormatSteam& format = saveDataFormats[formatIndex];
                SizedArray<SaveData> saveDatas;
                saveDatas.create(pMemoryAllocator, format.maximumInstallments, 0x10u);
                OperationResult result;
                const bool enumerated = enumerateSaveData(&result, &saveDatas, profileIndex, formatIndex);
                if (!enumerated || result != OperationResult_Ok || saveDatas.getSize() < format.minimumInstallments)
                {
                    setOperationFinished(!enumerated ? OperationResult_Error : (result != OperationResult_Ok ? result : OperationResult_Corrupted));
                    saveDatas.destroy(pMemoryAllocator);
                    return;
                }
                saveDatas.destroy(pMemoryAllocator);
            }
            setOperationFinished(OperationResult_Ok);
        }

        // 0x00CCE280 (:356): the set is the files themselves
        void SaveDataProviderWin32Steam::updateCreateSaveDataSet(uint32)
        {
            setOperationFinished(OperationResult_Ok);
        }

        // 0x00CCE290 (:399): every installment file of every format deleted. Original behaviour kept: the minimum
        // installment count bounds the ids, so files with higher ids stay.
        void SaveDataProviderWin32Steam::updateEraseSaveDataSet(uint32)
        {
            setOperationBusy();
            if (pSteamStorage != nullptr)
            {
                for (uint32 formatIndex = 0; formatIndex < saveDataFormats.getSize(); ++formatIndex)
                {
                    for (uint32 id = 0; id < saveDataFormats[formatIndex].minimumInstallments; ++id)
                    {
                        SaveData saveData;
                        saveData.formatIndex = formatIndex;
                        saveData.id = id;
                        char fileName[STEAM_MAX_FILENAME_LENGTH];
                        generateFileName(fileName, sizeof(fileName), saveData);
                        if (pSteamStorage->FileExists(fileName) && !pSteamStorage->FileDelete(fileName))
                        {
                            setOperationFinished(OperationResult_Error);
                            return;
                        }
                    }
                }
            }
            setOperationFinished(OperationResult_Ok);
        }

        // 0x00CCF000 (:438): a new installment at the first unused id. Original behaviour kept: without remote storage
        // the creation succeeds without writing.
        void SaveDataProviderWin32Steam::updateCreateSaveData(SaveData* pSaveData, uint32 profileIndex, const StorageOperation& operation)
        {
            setOperationBusy();
            if (pSteamStorage == nullptr)
            {
                setOperationFinished(OperationResult_Ok);
                return;
            }
            pSaveData->id = getUnusedSaveDataId(pSaveData->formatIndex);
            if (pSaveData->id == 0xFFFFFFFFu)
            {
                setOperationFinished(OperationResult_MaxInstallmentsReached);
                return;
            }
            setOperationFinished(writeSaveData(profileIndex, *pSaveData, operation));
        }

        // 0x00CCF060 (:463)
        void SaveDataProviderWin32Steam::updateWriteSaveData(uint32 profileIndex, const SaveData& saveData, const StorageOperation& operation)
        {
            setOperationBusy();
            setOperationFinished(writeSaveData(profileIndex, saveData, operation));
        }

        // 0x00CCE370 (:472): the first call starts the helper thread's read, later calls wait for it and scatter the
        // file into the operation's segments
        void SaveDataProviderWin32Steam::updateReadSaveData(StorageOperation* pOperation, uint32, const SaveData& saveData)
        {
            const SaveDataFormatSteam& format = saveDataFormats[saveData.formatIndex];
            if (asyncReadStarted)
            {
                if (!asyncReadFinished)
                {
                    return;
                }
                if ((uint32)asyncReadBytes != format.totalSize)
                {
                    debug::nativePrintArgs("[savedata] %i of %u bytes read from file '%s'.\n", asyncReadBytes, format.totalSize, readFileName);
                    pMemoryAllocator->free(pReadBuffer);
                    pReadBuffer = nullptr;
                    // Original behaviour kept: asyncReadStarted stays set, so the next read finds the stale result.
                    setOperationFinished(OperationResult_Corrupted);
                    return;
                }
                for (; pOperation != nullptr; pOperation = pOperation->pNext)
                {
                    const SaveDataFormatSteam::Segment& segment = format.segments[pOperation->segmentIndex];
                    memcpy(pOperation->pBuffer, pReadBuffer + segment.offset, segment.size);
                }
                pMemoryAllocator->free(pReadBuffer);
                operationResult = OperationResult_Ok;
                asyncReadStarted = false;
                asyncReadFinished = false;
                isCurrentOperationFinished = true;
                pReadBuffer = nullptr;
                return;
            }
            setOperationBusy();
            generateFileName(readFileName, sizeof(readFileName), saveData);
            if (pSteamStorage != nullptr && pSteamStorage->FileExists(readFileName))
            {
                // Magic number: 16-byte aligned buffer.
                pReadBuffer = (uint8*)pMemoryAllocator->allocate(format.totalSize, 0x10u, 0u, nullptr);
                asyncRequestedSize = (int)format.totalSize;
                asyncReadStarted = true;
                asyncReadFinished = false;
                helperThreadEvent.signal();
                return;
            }
            debug::nativePrintArgs("[savedata] Save data file '%s' doesn't exist.\n", readFileName);
            setOperationFinished(OperationResult_Missing);
        }

        // 0x00CCE4F0 (:539): the file is overwritten with zeros (an all-zero file is an unused id, getUnusedSaveDataId)
        void SaveDataProviderWin32Steam::updateEraseSaveData(uint32, const SaveData& saveData)
        {
            char fileName[STEAM_MAX_FILENAME_LENGTH];
            generateFileName(fileName, sizeof(fileName), saveData);
            const uint32 size = saveDataFormats[saveData.formatIndex].totalSize;
            void* pData = pMemoryAllocator->allocate(size, 0x10u, 0u, nullptr);
            memset(pData, 0, size);
            const OperationResult result = writeFileToSteamStorage(fileName, pData, size);
            pMemoryAllocator->free(pData);
            setOperationFinished(result);
        }

        // 0x00CCD410 (:573)
        bool SaveDataProviderWin32Steam::hasFinishedOperation()
        {
            return isCurrentOperationFinished;
        }

        // 0x00CCD420 (:578)
        OperationResult SaveDataProviderWin32Steam::getOperationResult()
        {
            return operationResult;
        }

        // 0x00CCD430 (:583)
        void SaveDataProviderWin32Steam::setOperationBusy()
        {
            isCurrentOperationFinished = false;
        }

        // 0x00CCD440 (:588)
        void SaveDataProviderWin32Steam::setOperationFinished(OperationResult result)
        {
            operationResult = result;
            isCurrentOperationFinished = true;
        }

        // 0x00CCE5C0 (:594)
        uint32 SaveDataProviderWin32Steam::getSegmentCount(uint32 formatIndex) const
        {
            return saveDataFormats[formatIndex].segments.getSize();
        }

        // 0x00CCCDC0 (.h:53)
        bool SaveDataProviderWin32Steam::hasSteamRemoteStorageFailed() const
        {
            return pSteamStorage == nullptr;
        }

        // 0x00CCD450 (:618)
        bool SaveDataProviderWin32Steam::containsData(uint8* pDataBuffer, uint32 bufferSize)
        {
            for (uint32 i = 0; i < bufferSize; ++i)
            {
                if (pDataBuffer[i] != 0)
                {
                    return true;
                }
            }
            return false;
        }

        // 0x00CCE5E0 (:633): the first id without a file or with an all-zero (erased) file. Original behaviour kept:
        // the read buffer leaks when a missing file ends the search.
        uint32 SaveDataProviderWin32Steam::getUnusedSaveDataId(uint32 formatIndex)
        {
            const SaveDataFormatSteam& format = saveDataFormats[formatIndex];
            uint8* pBuffer = (uint8*)pMemoryAllocator->allocate(format.totalSize, 0x10u, 0u, nullptr);
            for (uint32 id = 0; id < format.maximumInstallments; ++id)
            {
                SaveData saveData;
                saveData.formatIndex = formatIndex;
                saveData.id = id;
                char fileName[STEAM_MAX_FILENAME_LENGTH];
                generateFileName(fileName, sizeof(fileName), saveData);
                if (!pSteamStorage->FileExists(fileName))
                {
                    return id;
                }
                if ((uint32)pSteamStorage->FileRead(fileName, pBuffer, format.totalSize) == format.totalSize && !containsData(pBuffer, format.totalSize))
                {
                    pMemoryAllocator->free(pBuffer);
                    return id;
                }
            }
            pMemoryAllocator->free(pBuffer);
            debug::nativePrintArgs("[savedata] No unused id found.\n");
            return 0xFFFFFFFFu;
        }

        // 0x00CCD480 (:685)
        void SaveDataProviderWin32Steam::generateFileName(char* pFileName, uint32 maxFileNameSize, const SaveData& saveData)
        {
            formatString(pFileName, maxFileNameSize, "%skeen_savegame_%u_%u.sav", "autocloud/save/", saveData.formatIndex, saveData.id);
        }

        // 0x00CCD4B0 (:693): "keen_savegame_<format>_<id>..."
        bool SaveDataProviderWin32Steam::getSaveDataInfoFromFileName(SaveData* pSaveData, const char* pFileName)
        {
            char fileName[STEAM_MAX_FILENAME_LENGTH];
            copyString(fileName, sizeof(fileName), pFileName);
            StringTokenizer tokenizer;
            tokenizer.setString(fileName);
            if (compareString(tokenizer.getToken("_"), "keen") != 0 || compareString(tokenizer.getToken("_"), "savegame") != 0)
            {
                return false;
            }
            const char* pFormatToken = tokenizer.getToken("_");
            uint32 formatIndex;
            if (pFormatToken == nullptr || readUint32FromDecimalString(&formatIndex, &pFormatToken) != StringReadResult_Ok)
            {
                return false;
            }
            const char* pIdToken = tokenizer.getToken("_");
            uint32 id;
            if (pIdToken == nullptr || readUint32FromDecimalString(&id, &pIdToken) != StringReadResult_Ok)
            {
                return false;
            }
            pSaveData->formatIndex = formatIndex;
            pSaveData->id = id;
            return true;
        }

        // 0x00CCE6F0 (:731): the operation's segments gathered into one file
        OperationResult SaveDataProviderWin32Steam::writeSaveData(uint32, const SaveData& saveData, const StorageOperation& operation)
        {
            const SaveDataFormatSteam& format = saveDataFormats[saveData.formatIndex];
            char fileName[STEAM_MAX_FILENAME_LENGTH];
            generateFileName(fileName, sizeof(fileName), saveData);
            uint8* pData = (uint8*)pMemoryAllocator->allocate(format.totalSize, 0x10u, 0u, nullptr);
            for (const StorageOperation* pOperation = &operation; pOperation != nullptr; pOperation = pOperation->pNext)
            {
                const SaveDataFormatSteam::Segment& segment = format.segments[pOperation->segmentIndex];
                memcpy(pData + segment.offset, pOperation->pBuffer, segment.size);
            }
            const OperationResult result = writeFileToSteamStorage(fileName, pData, format.totalSize);
            pMemoryAllocator->free(pData);
            return result;
        }

        // 0x00CCD5E0 (:763): a file at least as large is overwritten; otherwise the quota is checked first
        OperationResult SaveDataProviderWin32Steam::writeFileToSteamStorage(const char* pFileName, const void* pData, uint32 memorySize)
        {
            if (pSteamStorage == nullptr)
            {
                return OperationResult_Error;
            }
            bool written;
            if (pSteamStorage->FileExists(pFileName) && (int)memorySize <= pSteamStorage->GetFileSize(pFileName))
            {
                written = pSteamStorage->FileWrite(pFileName, pData, (int)memorySize);
            }
            else
            {
                uint64 totalBytes;
                uint64 availableBytes;
                pSteamStorage->GetQuota(&totalBytes, &availableBytes);
                if (availableBytes < memorySize)
                {
                    debug::nativePrintArgs("[savedata] Current quota of %u not sufficient to write file with size %u. Only %u bytes left.\n", (uint32)totalBytes, memorySize, (uint32)availableBytes);
                    return OperationResult_InsufficientSpace;
                }
                written = pSteamStorage->FileWrite(pFileName, pData, (int)memorySize);
            }
            if (!written)
            {
                debug::nativePrintArgs("[savedata] Writing %u bytes to file '%s' failed.\n", memorySize, pFileName);
                return OperationResult_Error;
            }
            return OperationResult_Ok;
        }
    }
}
