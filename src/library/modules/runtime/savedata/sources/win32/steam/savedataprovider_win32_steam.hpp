// Original: library/modules/runtime/savedata/sources/win32/steam/savedataprovider_win32_steam.hpp
// Implementation: library/modules/runtime/savedata/sources/win32/steam/savedataprovider_win32_steam.cpp
//
// SaveDataProviderWin32Steam, saves through Steam Cloud (remote storage) in segments.
#pragma once

#include "keen/base/types.hpp"
#include "keen/base/array.hpp"
#include "keen/base/sizedarray.hpp"
#include "keen/base/win32/event_win32.hpp"
#include "keen/base/win32/thread_win32.hpp"
#include "keen/savedata/savedatasystem.hpp"
#include "keen/savedata/win32/savedatasystem_win32.hpp"
#include "keen/useraccount/useraccounttypes.hpp"

class ISteamRemoteStorage;

namespace keen
{
    class MemoryAllocator;

    namespace SaveData
    {
        // size 0x294
        class SaveDataProviderWin32Steam : public SaveDataProviderWin32Interface
        {
        public:
            enum
            {
                STEAM_MAX_FILENAME_LENGTH = 256,
            };

            enum WriteMode
            {
                WriteMode_FillSpace = 0,
                WriteMode_SkipSpace = 1,
            };

            // size 0x14
            struct SaveDataFormatSteam
            {
                // size 0x8
                struct Segment
                {
                    uint32 size; // 0x00
                    uint32 offset; // 0x04
                };

                SaveDataFormatSteam(); // 0x00CCE8F0
                ~SaveDataFormatSteam(); // 0x00CCE910

                Array<Segment> segments; // 0x00
                uint32 totalSize; // 0x08
                uint32 minimumInstallments; // 0x0C
                uint32 maximumInstallments; // 0x10
            };

            SaveDataProviderWin32Steam(); // 0x00CCDED0 (:21)
            virtual ~SaveDataProviderWin32Steam(); // vtbl+0x00 0x00CCDF40 (.h:20)
            virtual bool create(MemoryAllocator* pAllocator, const SaveDataSystemCreationParameters& parameters); // vtbl+0x04 0x00CCF830 (:31)
            virtual void destroy(MemoryAllocator* pAllocator); // vtbl+0x08 0x00CD0020 (:82)
            virtual bool enumerateProfiles(SizedArray<Profile>* pResult); // vtbl+0x0C 0x00CCDFB0 (:124)
        private:
            bool enumerateSaveData(OperationResult* pResultCode, SizedArray<SaveData>* pResult, uint32 profileIndex, uint32 formatIndex); // 0x00CCE020 (:150)
        public:
            virtual bool enumerateSaveData(SizedArray<SaveData>* pResult, uint32 profileIndex, uint32 formatIndex); // vtbl+0x10 0x00CCEE80 (:144)
            virtual bool connectProfileWithUser(uint32 profileIndex, UserAccountId userId); // vtbl+0x14 0x00CCD2C0 (:234)
            virtual bool disconnectProfile(uint32 profileIndex); // vtbl+0x18 0x00CCE1C0 (:256)
            virtual bool isProfileConnected(uint32 profileIndex) const; // vtbl+0x1C 0x00CCD3D0 (:276)
            virtual void updateCreateProfile(uint32 profileIndex, const Profile& profile); // vtbl+0x20 0x00CCE220 (:281)
            virtual void updateUpdateProfile(uint32 profileIndex, const Profile& profile); // vtbl+0x24 0x00CCE240 (:289)
            virtual void updateEraseProfile(uint32 profileIndex); // vtbl+0x28 0x00CCE260 (:297)
            virtual void updateConnectProfile(uint32 profileIndex, const Profile& profile); // vtbl+0x2C 0x00CCD3F0 (:304)
            virtual void updateDisconnectProfile(uint32 profileIndex, const Profile& profile); // vtbl+0x30 0x00CCD400 (:310)
            virtual void updateCheckSaveDataSet(uint32 profileIndex); // vtbl+0x34 0x00CCEEA0 (:316)
            virtual void updateCreateSaveDataSet(uint32 profileIndex); // vtbl+0x38 0x00CCE280 (:356)
            virtual void updateEraseSaveDataSet(uint32 profileIndex); // vtbl+0x3C 0x00CCE290 (:399)
            virtual void updateCreateSaveData(SaveData* pSaveData, uint32 profileIndex, const StorageOperation& operation); // vtbl+0x40 0x00CCF000 (:438)
            virtual void updateWriteSaveData(uint32 profileIndex, const SaveData& saveData, const StorageOperation& operation); // vtbl+0x44 0x00CCF060 (:463)
            virtual void updateReadSaveData(StorageOperation* pOperation, uint32 profileIndex, const SaveData& saveData); // vtbl+0x48 0x00CCE370 (:472)
            virtual void updateEraseSaveData(uint32 profileIndex, const SaveData& saveData); // vtbl+0x4C 0x00CCE4F0 (:539)
            virtual bool hasFinishedOperation(); // vtbl+0x50 0x00CCD410 (:573)
            virtual OperationResult getOperationResult(); // vtbl+0x54 0x00CCD420 (:578)
            virtual uint32 getSegmentCount(uint32 formatIndex) const; // vtbl+0x58 0x00CCE5C0 (:594)
            void setSaveDataFolderName(const char*);
            bool hasSteamRemoteStorageFailed() const; // 0x00CCCDC0 (.h:53)
        private:
            void setOperationBusy(); // 0x00CCD430 (:583)
            void setOperationFinished(OperationResult result); // 0x00CCD440 (:588)
            static bool containsData(uint8* pDataBuffer, uint32 bufferSize); // 0x00CCD450 (:618)
            uint32 getUnusedSaveDataId(uint32 formatIndex); // 0x00CCE5E0 (:633)
            void generateFileName(char* pFileName, uint32 maxFileNameSize, const SaveData& saveData); // 0x00CCD480 (:685)
            bool getSaveDataInfoFromFileName(SaveData* pSaveData, const char* pFileName); // 0x00CCD4B0 (:693)
            OperationResult writeSaveData(uint32 profileIndex, const SaveData& saveData, const StorageOperation& operation); // 0x00CCE6F0 (:731)
            OperationResult writeFileToSteamStorage(const char* pFileName, const void* pData, uint32 memorySize); // 0x00CCD5E0 (:763)
            static int steamSaveDoWork(const Thread& thread); // 0x00CCD250 (:102)

            bool isCurrentOperationFinished; // 0x04
            OperationResult operationResult; // 0x08
            wchar_t saveDataFolderName[128]; // 0x0C
            MemoryAllocator* pMemoryAllocator; // 0x10C
            ISteamRemoteStorage* pSteamStorage; // 0x110
            uint32 connectedUserProfileIndex; // 0x114
            UserAccountId connectedUserAccountId; // 0x118
            Array<SaveDataFormatSteam> saveDataFormats; // 0x124
            Event helperThreadEvent; // 0x12C
            Thread helperThread; // 0x130
            bool asyncReadStarted; // 0x184
            bool asyncReadFinished; // 0x185
            char readFileName[256]; // 0x186
            uint8* pReadBuffer; // 0x288
            int asyncRequestedSize; // 0x28C
            int asyncReadBytes; // 0x290
        };

    }
}
