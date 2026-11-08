// Original: library/modules/runtime/savedata/sources/win32/savedatasystem_win32.cpp
//
// The Win32 save data system (stub_impl.md S2): one operation at a time, started by the start* calls and advanced by
// getCurrentOperationStatus, which runs the provider's update for the current operation. The only provider is Steam
// (SaveDataProviderWin32Steam, Steam Cloud).
//
// Deviation (application.cpp deviation 4): rfg.exe always runs with Steam and the Steam provider and exits on any other
// type. OpenRFG also creates the local SaveDataSystemType_Win32Native, SaveDataProviderWin32Native (new code), and uses
// it with --no-steam, --no-steam-cloud-saves or --save-dir. The functions here run unchanged on either.

#include "keen/savedata/savedatasystem.hpp"
#include "keen/base/memoryallocator.hpp"
#include "keen/base/thread.hpp"
#include "keen/base/trace.hpp"
#include "keen/savedata/win32/savedatasystem_win32.hpp"
#include "keen/useraccount/useraccounttypes.hpp"
#include "library/modules/runtime/savedata/sources/win32/savedataprovider_win32_native.hpp"
#include "library/modules/runtime/savedata/sources/win32/steam/savedataprovider_win32_steam.hpp"

#include <new>
#include <stdlib.h>

namespace keen
{
    // 0x00CCCF20 (:182): the operation starts when none is running
    static bool startOperation(SaveDataSystem* pManager, SaveData::CurrentOperation operation)
    {
        if (pManager->currentOperation != SaveData::CurrentOperation_Idle)
        {
            return false;
        }
        pManager->currentOperation = operation;
        pManager->currentOperationStatus = SaveData::OperationStatus_Busy;
        return true;
    }

    namespace SaveData
    {
        // 0x00CCD160 (:440): every segment of the format is named at most once and the list ends within the segment count
        static bool validateStorageOperation(const SaveDataProviderWin32Interface* pProvider, const StorageOperation* pOperation, uint32 formatIndex)
        {
            if (pOperation == nullptr)
            {
                debug::nativePrintArgs("[savedata] Invalid storage operation: No operation specified.\n");
                return false;
            }
            const uint32 segmentCount = pProvider->getSegmentCount(formatIndex);
            for (uint32 segmentIndex = 0; segmentIndex < segmentCount; ++segmentIndex)
            {
                bool found = false;
                const StorageOperation* pCurrent = pOperation;
                uint32 remaining = segmentCount;
                do
                {
                    if (pCurrent->segmentIndex == segmentIndex)
                    {
                        if (found)
                        {
                            debug::nativePrintArgs("[savedata] Invalid storage operation: Multiply referred segment.\n");
                            return false;
                        }
                        found = true;
                    }
                    pCurrent = pCurrent->pNext;
                    if (remaining == 0)
                    {
                        debug::nativePrintArgs("[savedata] Invalid storage operation: Endless loop.\n");
                        return false;
                    }
                    --remaining;
                }
                while (pCurrent != nullptr);
            }
            return true;
        }

        // 0x00CCEE00 (:50). Only the Steam system exists on Win32 in rfg.exe; another type exits. Deviation: the native
        // system exists too.
        SaveDataSystem* create(MemoryAllocator* pAllocator, const SaveDataSystemCreationParameters& parameters)
        {
            SaveDataSystem* pManager = (SaveDataSystem*)pAllocator->allocate(sizeof(SaveDataSystem), 4u, 0u, nullptr);
            pManager->pAllocator = pAllocator;
            SaveDataProviderWin32Interface* pProvider = nullptr;
            if (parameters.saveDataSystemType == SaveDataSystemType_Steam)
            {
                void* pProviderMemory = pAllocator->allocate(sizeof(SaveDataProviderWin32Steam), 4u, 0u, nullptr);
                pProvider = pProviderMemory != nullptr ? new (pProviderMemory) SaveDataProviderWin32Steam() : nullptr;
            }
            else if (parameters.saveDataSystemType == SaveDataSystemType_Win32Native)
            {
                void* pProviderMemory = pAllocator->allocate(sizeof(SaveDataProviderWin32Native), 4u, 0u, nullptr);
                pProvider = pProviderMemory != nullptr ? new (pProviderMemory) SaveDataProviderWin32Native() : nullptr;
            }
            else
            {
                exit(1);
            }
            pManager->pProvider = pProvider;
            pManager->currentOperation = CurrentOperation_Idle;
            pManager->currentOperationStatus = OperationStatus_Nop;
            pManager->pOperationProfile = nullptr;
            pManager->pOperationSaveDataInput = nullptr;
            pManager->pOperationSaveDataOutput = nullptr;
            pManager->pOperationStorageData = nullptr;
            // Original behaviour kept: the provider's result is not checked.
            pProvider->create(pAllocator, parameters);
            return pManager;
        }

        // 0x00CCDD70 (:94): waits for the running operation first
        void destroy(SaveDataSystem* pManager)
        {
            while (pManager->currentOperation != CurrentOperation_Idle && getCurrentOperationStatus(pManager) != OperationStatus_Finished)
            {
                // Magic number: polls every 5 ms.
                sleepCurrentThreadMilliseconds(5u);
            }
            pManager->pProvider->destroy(pManager->pAllocator);
            SaveDataProviderWin32Interface* pProvider = pManager->pProvider;
            MemoryAllocator* pAllocator = pManager->pAllocator;
            if (pProvider != nullptr)
            {
                pProvider->~SaveDataProviderWin32Interface();
                pAllocator->free(pProvider);
            }
            pManager->pAllocator->free(pManager);
        }

        // 0x00CCCDD0 (:112)
        CurrentOperation getCurrentOperation(const SaveDataSystem* pManager)
        {
            return pManager->currentOperation;
        }

        // 0x00CCCDE0 (:119): a busy operation advances by one provider update
        OperationStatus getCurrentOperationStatus(SaveDataSystem* pManager)
        {
            if (pManager->currentOperationStatus == OperationStatus_Busy)
            {
                SaveDataProviderWin32Interface* pProvider = pManager->pProvider;
                switch (pManager->currentOperation)
                {
                case CurrentOperation_CheckingSaveDataSet:
                    pProvider->updateCheckSaveDataSet(pManager->operationProfileIndex);
                    break;
                case CurrentOperation_CreatingSaveDataSet:
                    pProvider->updateCreateSaveDataSet(pManager->operationProfileIndex);
                    break;
                case CurrentOperation_ErasingSaveDataSet:
                    pProvider->updateEraseSaveDataSet(pManager->operationProfileIndex);
                    break;
                case CurrentOperation_CreatingSaveData:
                    pProvider->updateCreateSaveData(pManager->pOperationSaveDataOutput, pManager->operationProfileIndex, *pManager->pOperationStorageData);
                    break;
                case CurrentOperation_ErasingSaveData:
                    pProvider->updateEraseSaveData(pManager->operationProfileIndex, *pManager->pOperationSaveDataInput);
                    break;
                case CurrentOperation_ReadingSaveData:
                    pProvider->updateReadSaveData(pManager->pOperationStorageData, pManager->operationProfileIndex, *pManager->pOperationSaveDataInput);
                    break;
                case CurrentOperation_WritingSaveData:
                    pProvider->updateWriteSaveData(pManager->operationProfileIndex, *pManager->pOperationSaveDataInput, *pManager->pOperationStorageData);
                    break;
                case CurrentOperation_CreatingProfile:
                    pProvider->updateCreateProfile(pManager->operationProfileIndex, *pManager->pOperationProfile);
                    break;
                case CurrentOperation_UpdatingProfile:
                    pProvider->updateUpdateProfile(pManager->operationProfileIndex, *pManager->pOperationProfile);
                    break;
                case CurrentOperation_ErasingProfile:
                    pProvider->updateEraseProfile(pManager->operationProfileIndex);
                    break;
                default:
                    exit(1);
                }
                if (pProvider->hasFinishedOperation())
                {
                    pManager->currentOperationStatus = OperationStatus_Finished;
                }
            }
            return pManager->currentOperationStatus;
        }

        // 0x00CCCF40 (:197)
        OperationResult finishOperation(SaveDataSystem* pManager)
        {
            if (pManager->currentOperationStatus != OperationStatus_Finished)
            {
                return OperationResult_NotFinished;
            }
            pManager->currentOperation = CurrentOperation_Idle;
            pManager->currentOperationStatus = OperationStatus_Nop;
            return pManager->pProvider->getOperationResult();
        }

        // 0x00CCCF70 (:214)
        bool enumerateProfiles(SizedArray<Profile>* pResult, const SaveDataSystem* pManager)
        {
            return pManager->pProvider->enumerateProfiles(pResult);
        }

        // 0x00CCCF90 (:223)
        bool enumerateSaveData(SizedArray<SaveData>* pResult, const SaveDataSystem* pManager, uint32 profileIndex, uint32 formatIndex)
        {
            return pManager->pProvider->enumerateSaveData(pResult, profileIndex, formatIndex);
        }

        // 0x00CCCFB0 (:233)
        bool startCreateProfile(SaveDataSystem* pManager, uint32 profileIndex, const Profile& profile)
        {
            if (!startOperation(pManager, CurrentOperation_CreatingProfile))
            {
                return false;
            }
            pManager->operationProfileIndex = profileIndex;
            pManager->pOperationProfile = &profile;
            return true;
        }

        // 0x00CCCFE0 (:250)
        bool startUpdateProfile(SaveDataSystem* pManager, uint32 profileIndex, const Profile& profile)
        {
            if (!startOperation(pManager, CurrentOperation_UpdatingProfile))
            {
                return false;
            }
            pManager->operationProfileIndex = profileIndex;
            pManager->pOperationProfile = &profile;
            return true;
        }

        // 0x00CCD010 (:267)
        bool startEraseProfile(SaveDataSystem* pManager, uint32 profileIndex)
        {
            if (!startOperation(pManager, CurrentOperation_ErasingProfile))
            {
                return false;
            }
            pManager->operationProfileIndex = profileIndex;
            return true;
        }

        // 0x00CCD040 (:282)
        bool connectProfileWithUser(SaveDataSystem* pManager, uint32 profileIndex, UserAccountId userId)
        {
            return pManager->pProvider->connectProfileWithUser(profileIndex, userId);
        }

        // 0x00CCD070 (:291)
        bool disconnectProfile(SaveDataSystem* pManager, uint32 profileIndex)
        {
            return pManager->pProvider->disconnectProfile(profileIndex);
        }

        // 0x00CCD090 (:299)
        bool isProfileConnected(const SaveDataSystem* pManager, uint32 profileIndex)
        {
            return pManager->pProvider->isProfileConnected(profileIndex);
        }

        // 0x00CCD0B0 (:307)
        bool startCheckSaveDataSet(SaveDataSystem* pManager, uint32 profileIndex)
        {
            if (!startOperation(pManager, CurrentOperation_CheckingSaveDataSet))
            {
                return false;
            }
            pManager->operationProfileIndex = profileIndex;
            return true;
        }

        // 0x00CCD0D0 (:321)
        bool startCreateSaveDataSet(SaveDataSystem* pManager, uint32 profileIndex)
        {
            if (!startOperation(pManager, CurrentOperation_CreatingSaveDataSet))
            {
                return false;
            }
            pManager->operationProfileIndex = profileIndex;
            return true;
        }

        // 0x00CCD100 (:335)
        bool startEraseSaveDataSet(SaveDataSystem* pManager, uint32 profileIndex)
        {
            if (!startOperation(pManager, CurrentOperation_ErasingSaveDataSet))
            {
                return false;
            }
            pManager->operationProfileIndex = profileIndex;
            return true;
        }

        // 0x00CCDDE0 (:350)
        bool startCreateSaveData(SaveDataSystem* pManager, uint32 profileIndex, SaveData* pSaveData, StorageOperation* pOperation)
        {
            if (!validateStorageOperation(pManager->pProvider, pOperation, pSaveData->formatIndex) || !startOperation(pManager, CurrentOperation_CreatingSaveData))
            {
                return false;
            }
            pManager->pOperationStorageData = pOperation;
            pManager->pOperationSaveDataOutput = pSaveData;
            pManager->operationProfileIndex = profileIndex;
            return true;
        }

        // 0x00CCDE30 (:372)
        bool startReadSaveData(SaveDataSystem* pManager, uint32 profileIndex, const SaveData& saveData, StorageOperation* pOperation)
        {
            if (!validateStorageOperation(pManager->pProvider, pOperation, saveData.formatIndex) || !startOperation(pManager, CurrentOperation_ReadingSaveData))
            {
                return false;
            }
            pManager->pOperationStorageData = pOperation;
            pManager->pOperationSaveDataInput = &saveData;
            pManager->operationProfileIndex = profileIndex;
            return true;
        }

        // 0x00CCDE80 (:394)
        bool startWriteSaveData(SaveDataSystem* pManager, uint32 profileIndex, const SaveData& saveData, StorageOperation* pOperation)
        {
            if (!validateStorageOperation(pManager->pProvider, pOperation, saveData.formatIndex) || !startOperation(pManager, CurrentOperation_WritingSaveData))
            {
                return false;
            }
            pManager->pOperationStorageData = pOperation;
            pManager->pOperationSaveDataInput = &saveData;
            pManager->operationProfileIndex = profileIndex;
            return true;
        }

        // 0x00CCD130 (:416)
        bool startEraseSaveData(SaveDataSystem* pManager, uint32 profileIndex, const SaveData& saveData)
        {
            if (!startOperation(pManager, CurrentOperation_ErasingSaveData))
            {
                return false;
            }
            pManager->operationProfileIndex = profileIndex;
            pManager->pOperationSaveDataInput = &saveData;
            return true;
        }

        // 0x00CCD1E0 (:484): nothing on Steam (Steam Cloud has no folders)
        void setSaveDataFolderName(SaveDataSystem*, const char*)
        {
        }

        // 0x00CCD1F0 (:501)
        void update(SaveDataSystem*)
        {
        }

        // 0x00CCD200 (:508)
        bool hasSteamRemoteStorageFailed(SaveDataSystem* pManager)
        {
            return static_cast<SaveDataProviderWin32Steam*>(pManager->pProvider)->hasSteamRemoteStorageFailed();
        }
    }
}
