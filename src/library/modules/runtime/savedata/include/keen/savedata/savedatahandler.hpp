// Original: library/modules/runtime/savedata/include/keen/savedata/savedatahandler.hpp
// Implementation: library/modules/runtime/savedata/sources/global/savedatahandler.cpp
//
// SaveDataHandler, the game-side save flow (save/load requests, interactions with the game session, client states).
// Placement of SaveFlowState inferred.
#pragma once

#include "keen/base/types.hpp"
#include "keen/base/array.hpp"
#include "keen/base/sizedarray.hpp"
#include "kaiko/gamesessioninteractions/gamesessioninteractions.hpp"
#include "keen/savedata/savedatasystem.hpp"
#include "keen/useraccount/useraccounttypes.hpp"

namespace keen
{
    class MemoryAllocator;

    enum SaveFlowState
    {
        SaveFlowState_Invalid = -1,
        SaveFlowState_Idle = 0,
        SaveFlowState_StartUp_CheckSaveDataSet = 1,
        SaveFlowState_StartUp_SelectionCanceled = 2,
        SaveFlowState_StartUp_Corrupt = 3,
        SaveFlowState_StartUp_ConfirmErase = 4,
        SaveFlowState_StartUp_Erase = 5,
        SaveFlowState_StartUp_EraseFailed = 6,
        SaveFlowState_StartUp_InformUserNewDevice = 7,
        SaveFlowState_StartUp_CreateNewSet = 8,
        SaveFlowState_StartUp_CreateNewSaveGame = 9,
        SaveFlowState_StartUp_CreationFailed = 10,
        SaveFlowState_StartUp_InsufficientSpace = 11,
        SaveFlowState_StartUp_EraseFromList = 12,
        SaveFlowState_Startup_Read = 13,
        SaveFlowState_Startup_UserBindingError = 14,
        SaveFlowState_Startup_UserBindingErrorConfirmErase = 15,
        SaveFlowState_Startup_ReadError = 16,
        SaveFlowState_Startup_ReadErrorConfirmErase = 17,
        SaveFlowState_Startup_ReadErrorErase = 18,
        SaveFlowState_Startup_ConfirmDisable = 19,
        SaveFlowState_Startup_End_Usable = 20,
        SaveFlowState_Startup_End_Disable = 21,
        SaveFlowState_Write_Writing = 22,
        SaveFlowState_Write_WriteError = 23,
        SaveFlowState_Write_Overwrite = 24,
        SaveFlowState_Write_Disable = 25,
        SaveFlowState_Write_Done = 26,
        SaveFlowState_Count = 27,
    };

    // size 0x10
    struct SaveDataInteractionData
    {
        enum
        {
            MaxResponseOptionCount = 2,
        };

        SaveDataInteractionData(); // 0x00C11000 (.h:17)

        kaiko::GameSessionInteractionId id; // 0x00
        kaiko::GameSessionResponseId responseOptions[2]; // 0x04
        uint32 responseOptionCount; // 0x0C
    };

    // size 0x58
    class SaveDataHandler
    {
    public:
        enum SaveGameClientState
        {
            SaveGameClientState_Free = 0,
            SaveGameClientState_Startup = 1,
            SaveGameClientState_Ready = 2,
            SaveGameClientState_Count = 3,
        };

        // size 0xC
        struct SaveGameData
        {
            SaveGameData(); // 0x00CCD9F0 (.h:130)
            bool create(MemoryAllocator* pAllocator, uint32 size); // 0x00CCE920 (.h:136)
            void destroy(MemoryAllocator* pAllocator); // 0x00CCE960 (.h:145)
            void clearData(); // 0x00CCDA00 (.h:151)
            void copy(const SaveGameData& other); // 0x00CCDA20 (.h:157)
            ~SaveGameData(); // 0x00C11330

            Array<uint8> data; // 0x00
            uint32 id; // 0x08
        };

        // size 0x30
        struct SaveGameClientData
        {
            SaveGameClientData(); // 0x00CCE8D0
            ~SaveGameClientData(); // 0x00CCE900

            SizedArray<SaveData::SaveData> currentSaveDatas; // 0x00
            SaveGameData saveData; // 0x0C
            UserAccountId userAccountId; // 0x18
            uint32 lastSavedDataId; // 0x24
            SaveGameClientState state; // 0x28
            bool isSaveEnabled; // 0x2C
            bool isShutdownRequested; // 0x2D
            bool isNewlyCreatedSaveGame; // 0x2E
        };

        SaveDataHandler(); // 0x00CCE990 (:21)
        void create(MemoryAllocator* pMemoryAllocator, SaveDataSystem* pSaveDataSystem, uint32 localPlayerCount, uint32 saveDataSize); // 0x00CCF4F0 (:27)
        void destroy(MemoryAllocator* pMemoryAllocator); // 0x00CCFA30 (:54)
        void setDefaultSaveGameData(const void* pDefaultSaveGameData, uint32 dataSize); // 0x00CCDA50 (:80)
        void setCheckSaveGameDataCallback(bool (*pCallbackFunction)(const void*, uint32)); // 0x00CCCCE0 (:90)
        bool isInteractionRequired(SaveDataInteractionData* pInteractionData) const; // 0x00CCCCF0 (:98)
        void setInteractionResponse(kaiko::GameSessionResponseId responseId); // 0x00CCF120 (:122)
        bool registerUser(uint32* pProfileIndex, const UserAccountId& userAccountId); // 0x00CCDA70 (:315)
        bool unregisterUser(uint32 profileIndex, bool immediately); // 0x00CCDAF0 (:358)
        bool isUserRegistered(uint32 profileIndex) const; // 0x00CCDB20 (:370)
        bool isUserUnregistered(uint32 profileIndex) const; // 0x00CCDB40 (:376)
        void update(); // 0x00CCFB50 (:398)
        bool isSaveEnabled(uint32 profileIndex) const; // 0x00CCDBC0 (:662)
        bool requestSave(uint32 profileIndex); // 0x00CCDBE0 (:672)
        bool isSaveInProgress() const; // 0x00CCDC10 (:686)
        void* getSaveGameData(uint32* pSaveDataSize, uint32 profileIndex); // 0x00CCDC60 (:699)
        bool hasNewlyCreatedSaveGame(uint32 profileIndex); // 0x00CCDD50 (:1055)
        bool getProfile(SaveData::Profile&, uint32);
    private:
        bool finishCurrentOperation(SaveData::OperationResult* pResult, SaveData::CurrentOperation operation); // 0x00CCDB60 (:383)
        void startNextOperation(); // 0x00CCF690 (:709)
        bool connectUser(uint32 profileIndex); // 0x00CCDC90 (:1024)
        bool disconnectUser(uint32 profileIndex); // 0x00CCDD10 (:1043)
        void startStartupFlow(uint32 profileIndex); // 0x00CCF3B0 (:770)
        void startWriteFlow(uint32 profileIndex); // 0x00CCCD40 (:778)
        void switchToState(SaveFlowState newState); // 0x00CCE9D0 (:808)
        void startUserInteraction(kaiko::GameSessionInteractionId id, kaiko::GameSessionResponseId firstOption, kaiko::GameSessionResponseId secondOption); // 0x00CCCD60 (:788)
    public:
        ~SaveDataHandler(); // 0x00C12240

    private:
        SaveFlowState m_state; // 0x00
        uint32 m_activeClientIndex; // 0x04
        SaveDataInteractionData m_currentUserInteraction; // 0x08
        SaveDataSystem* m_pSaveDataSystem; // 0x18
        SaveGameData m_saveDataForIO; // 0x1C
        Array<SaveGameClientData> m_clientData; // 0x28
        Array<uint8> m_defaultSaveGameData; // 0x30
        bool (*m_pCheckSaveGameDataCallback)(const void*, uint32); // 0x38
        SaveData::StorageOperation m_saveDataOp; // 0x3C
        bool m_isDefaultSaveGameDataSet; // 0x48
        SizedArray<SaveData::Profile> m_profileList; // 0x4C
    };
}
