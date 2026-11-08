// Original: library/modules/runtime/savedata/include/keen/savedata/savedatasystem.hpp
// Implementation: library/modules/runtime/savedata/sources/global/savedatasystem.cpp
//
// The save data system: creation parameters (filled by rfg::Application::fillSavedataParameters), the operations
// (save, load, delete, enumerate) and their status/results, save data and profiles. Placement of the SaveData records
// and of SaveDataSystem inferred.
#pragma once

#include "keen/base/types.hpp"
#include "keen/base/sizedarray.hpp"

namespace keen
{
    namespace SaveData
    {
        struct SaveData;
        struct SaveDataFormat;
        class SaveDataProviderWin32Interface;
        struct SaveDataSystemCreationProviderParameters;
    }
}

namespace keen
{
    class MemoryAllocator;
    struct UserAccountId;

    namespace SaveData
    {
        enum SaveDataSystemType
        {
            SaveDataSystemType_Invalid = 0,
            SaveDataSystemType_Win32Dev = 1,
            SaveDataSystemType_Win32Native = 2,
            SaveDataSystemType_Steam = 3,
            SaveDataSystemType_Ps3 = 4,
            SaveDataSystemType_Xb360 = 5,
            SaveDataSystemType_Ps4 = 6,
            SaveDataSystemType_OSXNative = 7,
            SaveDataSystemType_XBone = 8,
            SaveDataSystemType_PsV = 9,
            SaveDataSystemType_Switch = 10,
            SaveDataSystemType_Count = 11,
        };

        enum CurrentOperation
        {
            CurrentOperation_Idle = 0,
            CurrentOperation_CheckingSaveDataSet = 1,
            CurrentOperation_CreatingSaveDataSet = 2,
            CurrentOperation_ErasingSaveDataSet = 3,
            CurrentOperation_CreatingSaveData = 4,
            CurrentOperation_ErasingSaveData = 5,
            CurrentOperation_ReadingSaveData = 6,
            CurrentOperation_WritingSaveData = 7,
            CurrentOperation_ErasingFromListPSx = 8,
            CurrentOperation_RecheckingSaveDataSetPSP = 9,
            CurrentOperation_CreatingProfile = 10,
            CurrentOperation_UpdatingProfile = 11,
            CurrentOperation_ErasingProfile = 12,
        };

        enum OperationResult
        {
            OperationResult_Ok = 0,
            OperationResult_Missing = 1,
            OperationResult_Corrupted = 2,
            OperationResult_Error = 3,
            OperationResult_NotFinished = 4,
            OperationResult_InsufficientSpace = 5,
            OperationResult_MaxInstallmentsReached = 6,
            OperationResult_UserCanceled = 7,
            OperationResult_MSUnknownPSP = 8,
        };

        enum OperationStatus
        {
            OperationStatus_Nop = 0,
            OperationStatus_Busy = 1,
            OperationStatus_Finished = 2,
        };

        // size 0x20
        struct SaveDataSystemCreationParameters
        {
            SaveDataSystemCreationParameters(); // 0x00458A30 (.h:129)

            SaveDataSystemType saveDataSystemType; // 0x00
            uint32 maxProfileCount; // 0x04
            const SaveDataFormat* pSaveDataFormats; // 0x08
            uint32 saveDataFormatCount; // 0x0C
            bool suppressSystemDialogs; // 0x10
            const SaveDataFormat* pGlobalSaveDataFormats; // 0x14
            uint32 globalsaveDataFormatCount; // 0x18
            SaveDataSystemCreationProviderParameters* pProviderParameters; // 0x1C
        };

        // size 0x1
        struct SaveDataSystemCreationProviderParameters
        {
        };

        // size 0x10
        struct SaveDataFormat
        {
            uint32 minimumInstallments; // 0x00
            uint32 maximumInstallments; // 0x04
            uint32 numSegments; // 0x08
            const uint32* pSegmentSizes; // 0x0C
        };

        // size 0x41
        struct Profile
        {
            bool isUsed; // 0x00
            char name[64]; // 0x01
        };

        // size 0x8
        struct SaveData
        {
            uint32 formatIndex; // 0x00
            uint32 id; // 0x04
        };

        // size 0xC
        struct StorageOperation
        {
            uint32 segmentIndex; // 0x00
            void* pBuffer; // 0x04
            StorageOperation* pNext; // 0x08
        };

    }

    // size 0x2C
    struct SaveDataSystem
    {
        MemoryAllocator* pAllocator; // 0x00
        SaveData::SaveDataSystemType saveDataSystemWin32Type; // 0x04
        SaveData::SaveDataProviderWin32Interface* pProvider; // 0x08
        SaveData::CurrentOperation currentOperation; // 0x0C
        SaveData::OperationStatus currentOperationStatus; // 0x10
        uint32 operationProfileIndex; // 0x14
        const SaveData::Profile* pOperationProfile; // 0x18
        const SaveData::SaveData* pOperationSaveDataInput; // 0x1C
        SaveData::SaveData* pOperationSaveDataOutput; // 0x20
        SaveData::StorageOperation* pOperationStorageData; // 0x24
        bool suppressSystemDialogs; // 0x28
    };

    namespace SaveData
    {
        bool isSaveDataSystemTypeAvailable(SaveDataSystemType type); // 0x00CCCDA0 (:6)
        SaveDataSystem* create(MemoryAllocator* pAllocator, const SaveDataSystemCreationParameters& parameters); // 0x00CCEE00 (savedatasystem_win32.cpp:50)
        void destroy(SaveDataSystem* pManager); // 0x00CCDD70 (savedatasystem_win32.cpp:94)
        CurrentOperation getCurrentOperation(const SaveDataSystem* pManager); // 0x00CCCDD0 (savedatasystem_win32.cpp:112)
        OperationStatus getCurrentOperationStatus(SaveDataSystem* pManager); // 0x00CCCDE0 (savedatasystem_win32.cpp:119)
        OperationResult finishOperation(SaveDataSystem* pManager); // 0x00CCCF40 (savedatasystem_win32.cpp:197)
        bool enumerateProfiles(SizedArray<Profile>* pResult, const SaveDataSystem* pManager); // 0x00CCCF70 (savedatasystem_win32.cpp:214)
        bool enumerateSaveData(SizedArray<SaveData>* pResult, const SaveDataSystem* pManager, uint32 profileIndex, uint32 formatIndex); // 0x00CCCF90 (savedatasystem_win32.cpp:223)
        bool startCreateProfile(SaveDataSystem* pManager, uint32 profileIndex, const Profile& profile); // 0x00CCCFB0 (savedatasystem_win32.cpp:233)
        bool startUpdateProfile(SaveDataSystem* pManager, uint32 profileIndex, const Profile& profile); // 0x00CCCFE0 (savedatasystem_win32.cpp:250)
        bool startEraseProfile(SaveDataSystem* pManager, uint32 profileIndex); // 0x00CCD010 (savedatasystem_win32.cpp:267)
        bool connectProfileWithUser(SaveDataSystem* pManager, uint32 profileIndex, UserAccountId userId); // 0x00CCD040 (savedatasystem_win32.cpp:282)
        bool disconnectProfile(SaveDataSystem* pManager, uint32 profileIndex); // 0x00CCD070 (savedatasystem_win32.cpp:291)
        bool isProfileConnected(const SaveDataSystem* pManager, uint32 profileIndex); // 0x00CCD090 (savedatasystem_win32.cpp:299)
        bool startCheckSaveDataSet(SaveDataSystem* pManager, uint32 profileIndex); // 0x00CCD0B0 (savedatasystem_win32.cpp:307)
        bool startCreateSaveDataSet(SaveDataSystem* pManager, uint32 profileIndex); // 0x00CCD0D0 (savedatasystem_win32.cpp:321)
        bool startEraseSaveDataSet(SaveDataSystem* pManager, uint32 profileIndex); // 0x00CCD100 (savedatasystem_win32.cpp:335)
        bool startCreateSaveData(SaveDataSystem* pManager, uint32 profileIndex, SaveData* pSaveData, StorageOperation* pOperation); // 0x00CCDDE0 (savedatasystem_win32.cpp:350)
        bool startReadSaveData(SaveDataSystem* pManager, uint32 profileIndex, const SaveData& saveData, StorageOperation* pOperation); // 0x00CCDE30 (savedatasystem_win32.cpp:372)
        bool startWriteSaveData(SaveDataSystem* pManager, uint32 profileIndex, const SaveData& saveData, StorageOperation* pOperation); // 0x00CCDE80 (savedatasystem_win32.cpp:394)
        bool startEraseSaveData(SaveDataSystem* pManager, uint32 profileIndex, const SaveData& saveData); // 0x00CCD130 (savedatasystem_win32.cpp:416)
        void setSaveDataFolderName(SaveDataSystem* pManager, const char* pApplicationName); // 0x00CCD1E0 (savedatasystem_win32.cpp:484)
        void update(SaveDataSystem* pManager); // 0x00CCD1F0 (savedatasystem_win32.cpp:501)
        bool hasSteamRemoteStorageFailed(SaveDataSystem* pManager); // 0x00CCD200 (savedatasystem_win32.cpp:508)
    }
}
