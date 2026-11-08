// New code, not recovered (no such file in rfg.pdb): rfg.exe names SaveDataSystemType_Win32Native but only creates the
// Steam system (savedatasystem_win32.cpp).
//
// SaveDataProviderWin32Native keeps save data in a local folder: "keen_savegame_<format>_<id>.sav", byte for byte what
// SaveDataProviderWin32Steam stores in Steam's remote storage. OpenRFG uses it with --no-steam, --no-steam-cloud-saves or --save-dir (application.cpp deviation 4), so
// that such a run never touches the user's Steam Cloud saves. A write goes to a temporary file first and replaces
// the old file only when complete; the previous file stays as ".bak".
#pragma once

#include "keen/base/types.hpp"
#include "keen/savedata/savedatasystem.hpp"
#include "keen/savedata/win32/savedatasystem_win32.hpp"

namespace keen
{
    class MemoryAllocator;

    namespace SaveData
    {
        class SaveDataProviderWin32Native : public SaveDataProviderWin32Interface
        {
        public:
            SaveDataProviderWin32Native();
            virtual ~SaveDataProviderWin32Native();
            virtual bool create(MemoryAllocator* pAllocator, const SaveDataSystemCreationParameters& parameters);
            virtual void destroy(MemoryAllocator* pAllocator);
            virtual bool enumerateProfiles(SizedArray<Profile>* pResult);
            virtual bool enumerateSaveData(SizedArray<SaveData>* pResult, uint32 profileIndex, uint32 formatIndex);
            virtual bool connectProfileWithUser(uint32 profileIndex, UserAccountId userId);
            virtual bool disconnectProfile(uint32 profileIndex);
            virtual bool isProfileConnected(uint32 profileIndex) const;
            virtual void updateCreateProfile(uint32 profileIndex, const Profile& profile);
            virtual void updateUpdateProfile(uint32 profileIndex, const Profile& profile);
            virtual void updateEraseProfile(uint32 profileIndex);
            virtual void updateConnectProfile(uint32 profileIndex, const Profile& profile);
            virtual void updateDisconnectProfile(uint32 profileIndex, const Profile& profile);
            virtual void updateCheckSaveDataSet(uint32 profileIndex);
            virtual void updateCreateSaveDataSet(uint32 profileIndex);
            virtual void updateEraseSaveDataSet(uint32 profileIndex);
            virtual void updateCreateSaveData(SaveData* pSaveData, uint32 profileIndex, const StorageOperation& operation);
            virtual void updateWriteSaveData(uint32 profileIndex, const SaveData& saveData, const StorageOperation& operation);
            virtual void updateReadSaveData(StorageOperation* pOperation, uint32 profileIndex, const SaveData& saveData);
            virtual void updateEraseSaveData(uint32 profileIndex, const SaveData& saveData);
            virtual bool hasFinishedOperation();
            virtual OperationResult getOperationResult();
            virtual uint32 getSegmentCount(uint32 formatIndex) const;

        private:
            // Magic number: MAX_PATH.
            enum
            {
                MaxPathLength = 260,
            };

            void setOperationFinished(OperationResult result);
            void generateFilePath(char* pPath, uint32 capacity, const SaveData& saveData) const;
            uint32 getFormatSize(uint32 formatIndex) const;
            uint32 getSegmentOffset(uint32 formatIndex, uint32 segmentIndex) const;
            bool enumerateSaveData(OperationResult* pResultCode, SizedArray<SaveData>* pResult, uint32 formatIndex) const;
            OperationResult writeSaveData(const SaveData& saveData, const StorageOperation& operation);

            bool m_isOperationFinished;
            OperationResult m_operationResult;
            MemoryAllocator* m_pAllocator;
            const SaveDataFormat* m_pFormats; // the application's static table
            uint32 m_formatCount;
            uint32 m_connectedProfileIndex;
            char m_directory[MaxPathLength];
        };
    }
}
