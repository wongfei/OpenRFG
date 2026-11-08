// Original: library/modules/runtime/savedata/include/keen/savedata/win32/savedatasystem_win32.hpp
//
// Win32 save data providers: the provider parameters and the provider interface. Placement of the interface inferred.
#pragma once

#include "keen/base/types.hpp"
#include "keen/base/sizedarray.hpp"
#include "keen/savedata/savedatasystem.hpp"

namespace keen
{
    class MemoryAllocator;
    class SteamworksSystem;
    struct UserAccountId;

    namespace SaveData
    {
        // size 0x8
        struct SaveDataSystemCreationProviderParametersWin32 : public SaveDataSystemCreationProviderParameters
        {
            SaveDataSystemCreationProviderParametersWin32(); // 0x00458A50 (.h:17)

            SteamworksSystem* pSteamworksSystem; // 0x00
            const char* pApplicationName; // 0x04
            const char* pSaveDirectory; // port only (not in rfg.exe, which has size 0x8): SaveDataProviderWin32Native's folder, null: the default
        };

        // size 0x4
        class SaveDataProviderWin32Interface
        {
        public:
            virtual ~SaveDataProviderWin32Interface() = 0; // vtbl+0x00 0x00CCCCD0 (.h:34)
            virtual bool create(MemoryAllocator*, const SaveDataSystemCreationParameters&) = 0; // vtbl+0x04
            virtual void destroy(MemoryAllocator*) = 0; // vtbl+0x08
            virtual bool enumerateProfiles(SizedArray<Profile>*) = 0; // vtbl+0x0C
            virtual bool enumerateSaveData(SizedArray<SaveData>*, uint32, uint32) = 0; // vtbl+0x10
            virtual bool connectProfileWithUser(uint32, UserAccountId) = 0; // vtbl+0x14
            virtual bool disconnectProfile(uint32) = 0; // vtbl+0x18
            virtual bool isProfileConnected(uint32) const = 0; // vtbl+0x1C
            virtual void updateCreateProfile(uint32, const Profile&) = 0; // vtbl+0x20
            virtual void updateUpdateProfile(uint32, const Profile&) = 0; // vtbl+0x24
            virtual void updateEraseProfile(uint32) = 0; // vtbl+0x28
            virtual void updateConnectProfile(uint32, const Profile&) = 0; // vtbl+0x2C
            virtual void updateDisconnectProfile(uint32, const Profile&) = 0; // vtbl+0x30
            virtual void updateCheckSaveDataSet(uint32) = 0; // vtbl+0x34
            virtual void updateCreateSaveDataSet(uint32) = 0; // vtbl+0x38
            virtual void updateEraseSaveDataSet(uint32) = 0; // vtbl+0x3C
            virtual void updateCreateSaveData(SaveData*, uint32, const StorageOperation&) = 0; // vtbl+0x40
            virtual void updateWriteSaveData(uint32, const SaveData&, const StorageOperation&) = 0; // vtbl+0x44
            virtual void updateReadSaveData(StorageOperation*, uint32, const SaveData&) = 0; // vtbl+0x48
            virtual void updateEraseSaveData(uint32, const SaveData&) = 0; // vtbl+0x4C
            virtual bool hasFinishedOperation() = 0; // vtbl+0x50
            virtual OperationResult getOperationResult() = 0; // vtbl+0x54
            virtual uint32 getSegmentCount(uint32) const = 0; // vtbl+0x58
            SaveDataProviderWin32Interface(); // 0x00CCD240

            // 0x00 vfptr
        };

    }
}
