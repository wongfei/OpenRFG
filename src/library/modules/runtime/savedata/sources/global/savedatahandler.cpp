// Original: library/modules/runtime/savedata/sources/global/savedatahandler.cpp
//
// SaveDataHandler, the game-side save flow (stub_impl.md S2): per local user ("client") the start-up flow (check the
// save data set, read the one save game or create it, with a user interaction for each failure) and the write flow
// (a save request bumps the client's save game id; the idle handler writes clients whose id is ahead of the last
// written one). One operation of the save data system runs at a time; the interactions go to the local game session.
//
// Deviation: rfg.exe always has a save data system. OpenRFG has one too (the local one by default, application.cpp
// deviation 4); only the test programs that create the local game session themselves pass none, and update() then does
// nothing (its first line).

#include "keen/savedata/savedatahandler.hpp"
#include "keen/base/memoryallocator.hpp"
#include "keen/base/thread.hpp"
#include "keen/base/trace.hpp"
#include "keen/savedata/win32/savedatasystem_win32.hpp"

#include <stdlib.h>
#include <string.h>

namespace keen
{
    // Magic number: 0xFFFF, no active client.
    static const uint32 NoActiveClient = 0xFFFFu;

    // 0x00C11000 (.h:17)
    SaveDataInteractionData::SaveDataInteractionData()
    {
        id = kaiko::GameSessionInteractionId_Invalid;
        responseOptionCount = 0;
    }

    // 0x00CCD9F0 (.h:130)
    SaveDataHandler::SaveGameData::SaveGameData()
    {
        id = 0;
    }

    // 0x00C11330
    SaveDataHandler::SaveGameData::~SaveGameData()
    {
    }

    // 0x00CCE920 (.h:136). Magic number: the data is 256-byte aligned.
    bool SaveDataHandler::SaveGameData::create(MemoryAllocator* pAllocator, uint32 size)
    {
        if (!data.create(pAllocator, size, 0x100u))
        {
            return false;
        }
        id = 0;
        return true;
    }

    // 0x00CCE960 (.h:145)
    void SaveDataHandler::SaveGameData::destroy(MemoryAllocator* pAllocator)
    {
        data.destroy(pAllocator);
        id = 0;
    }

    // 0x00CCDA00 (.h:151)
    void SaveDataHandler::SaveGameData::clearData()
    {
        memset(data.getStart(), 0, data.getSize());
        id = 0;
    }

    // 0x00CCDA20 (.h:157)
    void SaveDataHandler::SaveGameData::copy(const SaveGameData& other)
    {
        memcpy(data.getStart(), other.data.getStart(), data.getSize());
        id = other.id;
    }

    // 0x00CCE8D0
    SaveDataHandler::SaveGameClientData::SaveGameClientData()
    {
    }

    // 0x00CCE900
    SaveDataHandler::SaveGameClientData::~SaveGameClientData()
    {
    }

    // 0x00CCE990 (:21)
    SaveDataHandler::SaveDataHandler()
    {
        m_pSaveDataSystem = nullptr;
        m_state = SaveFlowState_Invalid;
    }

    // 0x00C12240
    SaveDataHandler::~SaveDataHandler()
    {
    }

    // 0x00CCF4F0 (:27): a client per local player, each with one save data slot and its save game buffer
    void SaveDataHandler::create(MemoryAllocator* pMemoryAllocator, SaveDataSystem* pSaveDataSystem, uint32 localPlayerCount, uint32 saveDataSize)
    {
        // Magic number: 16-byte aligned arrays.
        m_clientData.create(pMemoryAllocator, localPlayerCount, 0x10u);
        for (uint32 i = 0; i < m_clientData.getSize(); ++i)
        {
            SaveGameClientData& client = m_clientData[i];
            client.currentSaveDatas.create(pMemoryAllocator, 1u, 0x10u);
            client.saveData.create(pMemoryAllocator, saveDataSize);
            client.userAccountId.isValid = false;
            client.state = SaveGameClientState_Free;
            client.lastSavedDataId = 0;
            client.isNewlyCreatedSaveGame = false;
            client.isSaveEnabled = false;
            client.isShutdownRequested = false;
        }
        m_pSaveDataSystem = pSaveDataSystem;
        m_saveDataForIO.create(pMemoryAllocator, saveDataSize);
        m_defaultSaveGameData.create(pMemoryAllocator, saveDataSize, 0x10u);
        // Magic number: room for 16 profiles.
        m_profileList.create(pMemoryAllocator, 16u, 0x10u);
        m_pCheckSaveGameDataCallback = nullptr;
        m_state = SaveFlowState_Idle;
        m_isDefaultSaveGameDataSet = false;
        m_activeClientIndex = NoActiveClient;
    }

    // 0x00CCFA30 (:54): waits for the running operation first
    void SaveDataHandler::destroy(MemoryAllocator* pMemoryAllocator)
    {
        if (m_pSaveDataSystem != nullptr)
        {
            while (SaveData::getCurrentOperationStatus(m_pSaveDataSystem) == SaveData::OperationStatus_Busy)
            {
                // Magic number: polls every 5 ms.
                sleepCurrentThreadMilliseconds(5u);
            }
        }
        m_profileList.destroy(pMemoryAllocator);
        m_defaultSaveGameData.destroy(pMemoryAllocator);
        for (uint32 i = 0; i < m_clientData.getSize(); ++i)
        {
            m_clientData[i].currentSaveDatas.destroy(pMemoryAllocator);
            m_clientData[i].saveData.destroy(pMemoryAllocator);
        }
        m_saveDataForIO.destroy(pMemoryAllocator);
        m_clientData.destroy(pMemoryAllocator);
        m_pSaveDataSystem = nullptr;
        m_state = SaveFlowState_Invalid;
    }

    // 0x00CCDA50 (:80). Original behaviour kept: dataSize is not used (the handler's save data size is copied) and
    // m_isDefaultSaveGameDataSet is not set.
    void SaveDataHandler::setDefaultSaveGameData(const void* pDefaultSaveGameData, uint32)
    {
        memcpy(m_defaultSaveGameData.getStart(), pDefaultSaveGameData, m_defaultSaveGameData.getSize());
    }

    // 0x00CCCCE0 (:90)
    void SaveDataHandler::setCheckSaveGameDataCallback(bool (*pCallbackFunction)(const void*, uint32))
    {
        m_pCheckSaveGameDataCallback = pCallbackFunction;
    }

    // 0x00CCCCF0 (:98): every flow but idle needs the session to wait; a flow waiting for the user has an interaction
    bool SaveDataHandler::isInteractionRequired(SaveDataInteractionData* pInteractionData) const
    {
        if (m_state == SaveFlowState_Idle)
        {
            return false;
        }
        if (pInteractionData != nullptr)
        {
            if (m_currentUserInteraction.id == kaiko::GameSessionInteractionId_Invalid)
            {
                pInteractionData->id = kaiko::GameSessionInteractionId_WaitingForProcess;
                pInteractionData->responseOptionCount = 0;
                return true;
            }
            *pInteractionData = m_currentUserInteraction;
        }
        return true;
    }

    // 0x00CCF120 (:122): the user's answer to the current interaction
    void SaveDataHandler::setInteractionResponse(kaiko::GameSessionResponseId responseId)
    {
        m_currentUserInteraction.id = kaiko::GameSessionInteractionId_Invalid;
        switch (m_state)
        {
        case SaveFlowState_StartUp_SelectionCanceled:
        case SaveFlowState_StartUp_EraseFailed:
            if (responseId != kaiko::GameSessionResponseId_RetryFunction)
            {
                break;
            }
            m_state = SaveFlowState_StartUp_CheckSaveDataSet;
            if (SaveData::startCheckSaveDataSet(m_pSaveDataSystem, m_activeClientIndex))
            {
                m_clientData[m_activeClientIndex].currentSaveDatas.clear();
                return;
            }
            switchToState(SaveFlowState_Startup_End_Disable);
            return;
        case SaveFlowState_StartUp_Corrupt:
            if (responseId != kaiko::GameSessionResponseId_Erase)
            {
                break;
            }
            m_state = SaveFlowState_StartUp_ConfirmErase;
            startUserInteraction(kaiko::GameSessionInteractionId_ConfirmErase, kaiko::GameSessionResponseId_Cancel, kaiko::GameSessionResponseId_Erase);
            return;
        case SaveFlowState_StartUp_ConfirmErase:
            if (responseId != kaiko::GameSessionResponseId_Erase)
            {
                m_state = SaveFlowState_StartUp_Corrupt;
                startUserInteraction(kaiko::GameSessionInteractionId_SaveGameCorrupt, kaiko::GameSessionResponseId_Erase, kaiko::GameSessionResponseId_Disable);
                return;
            }
            m_state = SaveFlowState_StartUp_Erase;
            if (!SaveData::startEraseSaveDataSet(m_pSaveDataSystem, m_activeClientIndex))
            {
                switchToState(SaveFlowState_StartUp_EraseFailed);
            }
            return;
        case SaveFlowState_StartUp_InformUserNewDevice:
            m_state = SaveFlowState_StartUp_CreateNewSet;
            if (!SaveData::startCreateSaveDataSet(m_pSaveDataSystem, m_activeClientIndex))
            {
                switchToState(SaveFlowState_StartUp_CreationFailed);
            }
            return;
        case SaveFlowState_StartUp_CreationFailed:
        case SaveFlowState_StartUp_InsufficientSpace:
            if (responseId != kaiko::GameSessionResponseId_RetryFunction)
            {
                break;
            }
            switchToState(SaveFlowState_StartUp_CheckSaveDataSet);
            return;
        case SaveFlowState_Startup_UserBindingError:
            if (responseId != kaiko::GameSessionResponseId_Erase)
            {
                break;
            }
            switchToState(SaveFlowState_Startup_UserBindingErrorConfirmErase);
            return;
        case SaveFlowState_Startup_UserBindingErrorConfirmErase:
            switchToState(responseId == kaiko::GameSessionResponseId_Erase ? SaveFlowState_StartUp_Erase : SaveFlowState_Startup_UserBindingError);
            return;
        case SaveFlowState_Startup_ReadError:
            switchToState(responseId == kaiko::GameSessionResponseId_RetryFunction ? SaveFlowState_Startup_ReadErrorErase : SaveFlowState_Startup_ConfirmDisable);
            return;
        case SaveFlowState_Startup_ReadErrorConfirmErase:
            switchToState(responseId == kaiko::GameSessionResponseId_Erase ? SaveFlowState_StartUp_Erase : SaveFlowState_Startup_ReadErrorErase);
            return;
        case SaveFlowState_Startup_ReadErrorErase:
            switchToState(responseId == kaiko::GameSessionResponseId_RetryFunction ? SaveFlowState_StartUp_CheckSaveDataSet : SaveFlowState_Startup_ReadErrorConfirmErase);
            return;
        case SaveFlowState_Startup_ConfirmDisable:
            switchToState(SaveFlowState_Startup_End_Disable);
            return;
        case SaveFlowState_Write_WriteError:
            switchToState(responseId == kaiko::GameSessionResponseId_RetryFunction ? SaveFlowState_Write_Writing : SaveFlowState_Startup_ConfirmDisable);
            return;
        case SaveFlowState_Write_Overwrite:
            switchToState(responseId == kaiko::GameSessionResponseId_Overwrite ? SaveFlowState_Write_Writing : SaveFlowState_StartUp_CheckSaveDataSet);
            return;
        default:
            exit(1);
        }
        // Any other answer: confirm that saving gets disabled.
        m_state = SaveFlowState_Startup_ConfirmDisable;
        startUserInteraction(kaiko::GameSessionInteractionId_ConfirmDisable, kaiko::GameSessionResponseId_Disable, kaiko::GameSessionResponseId_None);
    }

    // 0x00CCDA70 (:315): the first free client starts its start-up flow at the next idle update
    bool SaveDataHandler::registerUser(uint32* pProfileIndex, const UserAccountId& userAccountId)
    {
        for (uint32 i = 0; i < m_clientData.getSize(); ++i)
        {
            SaveGameClientData& client = m_clientData[i];
            if (client.state == SaveGameClientState_Free)
            {
                client.currentSaveDatas.clear();
                client.saveData.clearData();
                client.lastSavedDataId = 0;
                client.isSaveEnabled = false;
                client.userAccountId = userAccountId;
                client.state = SaveGameClientState_Startup;
                *pProfileIndex = i;
                return true;
            }
        }
        debug::nativePrintArgs("[savedatahandler] Couldn't find a free profile index for a new user!\n");
        return false;
    }

    // 0x00CCDAF0 (:358): the client is freed at the next idle update (after its pending write unless immediately)
    bool SaveDataHandler::unregisterUser(uint32 profileIndex, bool immediately)
    {
        m_clientData[profileIndex].isShutdownRequested = true;
        if (immediately)
        {
            m_clientData[profileIndex].isSaveEnabled = false;
        }
        return true;
    }

    // 0x00CCDB20 (:370)
    bool SaveDataHandler::isUserRegistered(uint32 profileIndex) const
    {
        return m_clientData[profileIndex].state == SaveGameClientState_Ready;
    }

    // 0x00CCDB40 (:376)
    bool SaveDataHandler::isUserUnregistered(uint32 profileIndex) const
    {
        return m_clientData[profileIndex].state == SaveGameClientState_Free;
    }

    // 0x00CCDB60 (:383): the operation's result once the system reports it finished
    bool SaveDataHandler::finishCurrentOperation(SaveData::OperationResult* pResult, SaveData::CurrentOperation)
    {
        if (SaveData::getCurrentOperationStatus(m_pSaveDataSystem) != SaveData::OperationStatus_Finished)
        {
            return false;
        }
        *pResult = SaveData::finishOperation(m_pSaveDataSystem);
        return true;
    }

    // 0x00CCFB50 (:398)
    void SaveDataHandler::update()
    {
        if (m_pSaveDataSystem == nullptr) // deviation: no Steam, no save data system
        {
            return;
        }
        // A client that shuts down while its flow waits ends the flow.
        if (m_state != SaveFlowState_Idle && m_pSaveDataSystem->currentOperation == SaveData::CurrentOperation_Idle && m_activeClientIndex != NoActiveClient &&
            m_clientData[m_activeClientIndex].isShutdownRequested)
        {
            m_state = SaveFlowState_Idle;
            m_activeClientIndex = NoActiveClient;
            m_currentUserInteraction.id = kaiko::GameSessionInteractionId_Invalid;
        }
        SaveData::OperationResult result = SaveData::OperationResult_Ok;
        switch (m_state)
        {
        case SaveFlowState_Idle:
            startNextOperation();
            return;
        case SaveFlowState_StartUp_CheckSaveDataSet:
        {
            if (!finishCurrentOperation(&result, SaveData::CurrentOperation_CheckingSaveDataSet))
            {
                return;
            }
            SaveGameClientData& client = m_clientData[m_activeClientIndex];
            switch (result)
            {
            case SaveData::OperationResult_Ok:
                if (SaveData::enumerateSaveData(&client.currentSaveDatas, m_pSaveDataSystem, m_activeClientIndex, 0u) && client.currentSaveDatas.getSize() == 1)
                {
                    if (client.state == SaveGameClientState_Ready)
                    {
                        m_state = SaveFlowState_Write_Overwrite;
                        startUserInteraction(kaiko::GameSessionInteractionId_Overwrite, kaiko::GameSessionResponseId_Overwrite, kaiko::GameSessionResponseId_DeviceSelection);
                        return;
                    }
                    m_saveDataOp.pBuffer = m_saveDataForIO.data.getStart();
                    m_state = SaveFlowState_Startup_Read;
                    m_saveDataOp.pNext = nullptr;
                    m_saveDataOp.segmentIndex = 0;
                    if (!SaveData::startReadSaveData(m_pSaveDataSystem, m_activeClientIndex, client.currentSaveDatas[0], &m_saveDataOp))
                    {
                        switchToState(SaveFlowState_Startup_ReadError);
                    }
                    return;
                }
                m_state = SaveFlowState_StartUp_CreateNewSet;
                if (!SaveData::startCreateSaveDataSet(m_pSaveDataSystem, m_activeClientIndex))
                {
                    switchToState(SaveFlowState_StartUp_CreationFailed);
                }
                return;
            case SaveData::OperationResult_UserCanceled:
                m_state = SaveFlowState_StartUp_SelectionCanceled;
                startUserInteraction(kaiko::GameSessionInteractionId_SelectionCanceled, kaiko::GameSessionResponseId_RetryFunction, kaiko::GameSessionResponseId_Disable);
                return;
            case SaveData::OperationResult_Missing:
                if (client.state == SaveGameClientState_Ready)
                {
                    m_state = SaveFlowState_StartUp_InformUserNewDevice;
                    startUserInteraction(kaiko::GameSessionInteractionId_InformDeviceChanged, kaiko::GameSessionResponseId_Ok, kaiko::GameSessionResponseId_None);
                    return;
                }
                m_state = SaveFlowState_StartUp_CreateNewSet;
                if (!SaveData::startCreateSaveDataSet(m_pSaveDataSystem, m_activeClientIndex))
                {
                    switchToState(SaveFlowState_StartUp_CreationFailed);
                }
                return;
            case SaveData::OperationResult_Corrupted:
                m_state = SaveFlowState_StartUp_Corrupt;
                startUserInteraction(kaiko::GameSessionInteractionId_SaveGameCorrupt, kaiko::GameSessionResponseId_Erase, kaiko::GameSessionResponseId_Disable);
                return;
            case SaveData::OperationResult_InsufficientSpace:
                m_state = SaveFlowState_StartUp_InsufficientSpace;
                startUserInteraction(kaiko::GameSessionInteractionId_NotEnoughSpace_RetryDisable, kaiko::GameSessionResponseId_RetryFunction, kaiko::GameSessionResponseId_Disable);
                return;
            default:
                m_state = SaveFlowState_Startup_End_Disable;
                client.isSaveEnabled = false;
                client.state = SaveGameClientState_Ready;
                switchToState(SaveFlowState_Idle);
                return;
            }
        }
        case SaveFlowState_StartUp_Erase:
            if (!finishCurrentOperation(&result, SaveData::CurrentOperation_ErasingSaveDataSet))
            {
                return;
            }
            m_clientData[m_activeClientIndex].currentSaveDatas.clear();
            switchToState(result == SaveData::OperationResult_Ok ? SaveFlowState_StartUp_CheckSaveDataSet : SaveFlowState_StartUp_EraseFailed);
            return;
        case SaveFlowState_StartUp_CreateNewSet:
            if (!finishCurrentOperation(&result, SaveData::CurrentOperation_CreatingSaveDataSet))
            {
                return;
            }
            if (result == SaveData::OperationResult_Ok)
            {
                switchToState(SaveFlowState_StartUp_CreateNewSaveGame);
            }
            else if (result == SaveData::OperationResult_InsufficientSpace)
            {
                switchToState(SaveFlowState_StartUp_InsufficientSpace);
            }
            else
            {
                switchToState(SaveFlowState_StartUp_CreationFailed);
            }
            return;
        case SaveFlowState_StartUp_CreateNewSaveGame:
            if (!finishCurrentOperation(&result, SaveData::CurrentOperation_CreatingSaveData))
            {
                return;
            }
            if (result == SaveData::OperationResult_Ok)
            {
                if (!SaveData::enumerateSaveData(&m_clientData[m_activeClientIndex].currentSaveDatas, m_pSaveDataSystem, m_activeClientIndex, 0u))
                {
                    switchToState(SaveFlowState_StartUp_CreationFailed);
                    return;
                }
                switchToState(SaveFlowState_Startup_End_Usable);
            }
            else if (result == SaveData::OperationResult_InsufficientSpace)
            {
                switchToState(SaveFlowState_StartUp_InsufficientSpace);
            }
            else
            {
                switchToState(SaveFlowState_StartUp_CreationFailed);
            }
            return;
        case SaveFlowState_StartUp_EraseFromList:
            if (!finishCurrentOperation(&result, SaveData::CurrentOperation_ErasingFromListPSx))
            {
                return;
            }
            switchToState(SaveFlowState_StartUp_CheckSaveDataSet);
            return;
        case SaveFlowState_Startup_Read:
            if (!finishCurrentOperation(&result, SaveData::CurrentOperation_ReadingSaveData))
            {
                return;
            }
            if (result == SaveData::OperationResult_Ok)
            {
                SaveGameClientData& client = m_clientData[m_activeClientIndex];
                if (m_pCheckSaveGameDataCallback == nullptr || m_pCheckSaveGameDataCallback(m_saveDataForIO.data.getStart(), m_saveDataForIO.data.getSize()))
                {
                    client.saveData.copy(m_saveDataForIO);
                    client.lastSavedDataId = m_saveDataForIO.id;
                    switchToState(SaveFlowState_Startup_End_Usable);
                    return;
                }
                switchToState(SaveFlowState_Startup_ReadError);
            }
            else if (result == SaveData::OperationResult_Corrupted)
            {
                switchToState(SaveFlowState_StartUp_Corrupt);
            }
            else
            {
                switchToState(SaveFlowState_Startup_ReadError);
            }
            return;
        case SaveFlowState_Write_Writing:
            if (!finishCurrentOperation(&result, SaveData::CurrentOperation_WritingSaveData))
            {
                return;
            }
            if (result == SaveData::OperationResult_Ok)
            {
                m_state = SaveFlowState_Write_Done;
                SaveGameClientData& client = m_clientData[m_activeClientIndex];
                client.lastSavedDataId = m_saveDataForIO.id;
                // A save requested while writing: write again.
                if (client.lastSavedDataId < client.saveData.id)
                {
                    const uint32 clientIndex = m_activeClientIndex;
                    m_activeClientIndex = NoActiveClient;
                    startWriteFlow(clientIndex);
                    return;
                }
                switchToState(SaveFlowState_Idle);
            }
            else if (result == SaveData::OperationResult_Missing)
            {
                switchToState(SaveFlowState_StartUp_CheckSaveDataSet);
            }
            else
            {
                m_state = SaveFlowState_Write_WriteError;
                startUserInteraction(kaiko::GameSessionInteractionId_WriteError, kaiko::GameSessionResponseId_RetryFunction, kaiko::GameSessionResponseId_Disable);
            }
            return;
        default:
            return;
        }
    }

    // 0x00CCDBC0 (:662)
    bool SaveDataHandler::isSaveEnabled(uint32 profileIndex) const
    {
        if (profileIndex == NoActiveClient)
        {
            return false;
        }
        return m_clientData[profileIndex].isSaveEnabled;
    }

    // 0x00CCDBE0 (:672): the idle handler writes the client once its save game id is ahead of the last written one
    bool SaveDataHandler::requestSave(uint32 profileIndex)
    {
        SaveGameClientData& client = m_clientData[profileIndex];
        if (!client.isSaveEnabled || client.state != SaveGameClientState_Ready)
        {
            return false;
        }
        client.saveData.id++;
        return true;
    }

    // 0x00CCDC10 (:686)
    bool SaveDataHandler::isSaveInProgress() const
    {
        for (uint32 i = 0; i < m_clientData.getSize(); ++i)
        {
            if (m_clientData[i].lastSavedDataId < m_clientData[i].saveData.id)
            {
                return true;
            }
        }
        return m_state == SaveFlowState_Write_Writing || m_state == SaveFlowState_StartUp_Erase || m_state == SaveFlowState_StartUp_CreateNewSet ||
               m_state == SaveFlowState_StartUp_CreateNewSaveGame;
    }

    // 0x00CCDC60 (:699)
    void* SaveDataHandler::getSaveGameData(uint32* pSaveDataSize, uint32 profileIndex)
    {
        *pSaveDataSize = m_clientData[profileIndex].saveData.data.getSize();
        return m_clientData[profileIndex].saveData.data.getStart();
    }

    // 0x00CCF690 (:709): the first client to start up, the shut-down clients freed, then the first client with a
    // pending save written
    void SaveDataHandler::startNextOperation()
    {
        if (m_pSaveDataSystem->currentOperation != SaveData::CurrentOperation_Idle)
        {
            return;
        }
        for (uint32 i = 0; i < m_clientData.getSize(); ++i)
        {
            SaveGameClientData& client = m_clientData[i];
            if (client.state == SaveGameClientState_Startup)
            {
                connectUser(i);
                startStartupFlow(i);
                return;
            }
            if (client.state == SaveGameClientState_Ready && client.isShutdownRequested && (!client.isSaveEnabled || client.saveData.id == client.lastSavedDataId))
            {
                disconnectUser(i);
                client.state = SaveGameClientState_Free;
                client.isShutdownRequested = false;
                client.isSaveEnabled = false;
                client.lastSavedDataId = 0;
                client.saveData.clearData();
                client.currentSaveDatas.clear();
            }
        }
        for (uint32 i = 0; i < m_clientData.getSize(); ++i)
        {
            const SaveGameClientData& client = m_clientData[i];
            if (client.state == SaveGameClientState_Ready && client.isSaveEnabled && client.lastSavedDataId < client.saveData.id)
            {
                startWriteFlow(i);
                return;
            }
        }
    }

    // 0x00CCF3B0 (:770)
    void SaveDataHandler::startStartupFlow(uint32 profileIndex)
    {
        m_activeClientIndex = profileIndex;
        m_state = SaveFlowState_StartUp_CheckSaveDataSet;
        if (!SaveData::startCheckSaveDataSet(m_pSaveDataSystem, profileIndex))
        {
            switchToState(SaveFlowState_Startup_End_Disable);
            return;
        }
        m_clientData[m_activeClientIndex].currentSaveDatas.clear();
    }

    // 0x00CCCD40 (:778)
    void SaveDataHandler::startWriteFlow(uint32 profileIndex)
    {
        m_activeClientIndex = profileIndex;
        switchToState(SaveFlowState_Write_Writing);
    }

    // 0x00CCCD60 (:788)
    void SaveDataHandler::startUserInteraction(kaiko::GameSessionInteractionId id, kaiko::GameSessionResponseId firstOption, kaiko::GameSessionResponseId secondOption)
    {
        m_currentUserInteraction.id = id;
        m_currentUserInteraction.responseOptions[0] = firstOption;
        m_currentUserInteraction.responseOptions[1] = secondOption;
        m_currentUserInteraction.responseOptionCount = 0;
        if (firstOption != kaiko::GameSessionResponseId_None)
        {
            m_currentUserInteraction.responseOptionCount = 1;
        }
        if (secondOption != kaiko::GameSessionResponseId_None)
        {
            m_currentUserInteraction.responseOptionCount++;
        }
    }

    // 0x00CCE9D0 (:808): enters the state; a state whose operation cannot start falls through to its failure state
    void SaveDataHandler::switchToState(SaveFlowState newState)
    {
        m_state = newState;
        for (;;)
        {
            if (newState > SaveFlowState_Write_Done)
            {
                exit(1);
            }
            switch (newState)
            {
            case SaveFlowState_Idle:
                m_activeClientIndex = NoActiveClient;
                m_currentUserInteraction.id = kaiko::GameSessionInteractionId_Invalid;
                return;
            case SaveFlowState_StartUp_CheckSaveDataSet:
                if (SaveData::startCheckSaveDataSet(m_pSaveDataSystem, m_activeClientIndex))
                {
                    m_clientData[m_activeClientIndex].currentSaveDatas.clear();
                    return;
                }
                newState = SaveFlowState_Startup_End_Disable;
                break;
            case SaveFlowState_StartUp_SelectionCanceled:
                startUserInteraction(kaiko::GameSessionInteractionId_SelectionCanceled, kaiko::GameSessionResponseId_RetryFunction, kaiko::GameSessionResponseId_Disable);
                return;
            case SaveFlowState_StartUp_Corrupt:
                startUserInteraction(kaiko::GameSessionInteractionId_SaveGameCorrupt, kaiko::GameSessionResponseId_Erase, kaiko::GameSessionResponseId_Disable);
                return;
            case SaveFlowState_StartUp_Erase:
                if (SaveData::startEraseSaveDataSet(m_pSaveDataSystem, m_activeClientIndex))
                {
                    return;
                }
                newState = SaveFlowState_StartUp_EraseFailed;
                break;
            case SaveFlowState_StartUp_EraseFailed:
                startUserInteraction(kaiko::GameSessionInteractionId_EraseFailed, kaiko::GameSessionResponseId_RetryFunction, kaiko::GameSessionResponseId_Disable);
                return;
            case SaveFlowState_StartUp_InformUserNewDevice:
                startUserInteraction(kaiko::GameSessionInteractionId_InformDeviceChanged, kaiko::GameSessionResponseId_Ok, kaiko::GameSessionResponseId_None);
                return;
            case SaveFlowState_StartUp_CreateNewSet:
                if (SaveData::startCreateSaveDataSet(m_pSaveDataSystem, m_activeClientIndex))
                {
                    return;
                }
                newState = SaveFlowState_StartUp_CreationFailed;
                break;
            case SaveFlowState_StartUp_CreateNewSaveGame:
            {
                SaveGameClientData& client = m_clientData[m_activeClientIndex];
                m_saveDataForIO.copy(client.saveData);
                // The client's one save data slot: a new save game replaces the old one.
                if (client.currentSaveDatas.getSize() == 1)
                {
                    client.currentSaveDatas.clear();
                }
                SaveData::SaveData& saveData = client.currentSaveDatas.pushBack();
                saveData.formatIndex = 0;
                saveData.id = 0;
                m_saveDataOp.pBuffer = m_saveDataForIO.data.getStart();
                m_saveDataOp.pNext = nullptr;
                m_saveDataOp.segmentIndex = 0;
                client.isNewlyCreatedSaveGame = true;
                if (SaveData::startCreateSaveData(m_pSaveDataSystem, m_activeClientIndex, &saveData, &m_saveDataOp))
                {
                    return;
                }
                newState = SaveFlowState_StartUp_CreationFailed;
                break;
            }
            case SaveFlowState_StartUp_CreationFailed:
                startUserInteraction(kaiko::GameSessionInteractionId_CreateFailed, kaiko::GameSessionResponseId_RetryFunction, kaiko::GameSessionResponseId_Disable);
                return;
            case SaveFlowState_StartUp_InsufficientSpace:
                startUserInteraction(kaiko::GameSessionInteractionId_NotEnoughSpace_RetryDisable, kaiko::GameSessionResponseId_RetryFunction, kaiko::GameSessionResponseId_Disable);
                return;
            case SaveFlowState_StartUp_EraseFromList:
                return;
            case SaveFlowState_Startup_Read:
                m_saveDataOp.pBuffer = m_saveDataForIO.data.getStart();
                m_saveDataOp.pNext = nullptr;
                m_saveDataOp.segmentIndex = 0;
                if (SaveData::startReadSaveData(m_pSaveDataSystem, m_activeClientIndex, m_clientData[m_activeClientIndex].currentSaveDatas[0], &m_saveDataOp))
                {
                    return;
                }
                newState = SaveFlowState_Startup_ReadError;
                break;
            case SaveFlowState_Startup_UserBindingError:
                startUserInteraction(kaiko::GameSessionInteractionId_UserBindingError, kaiko::GameSessionResponseId_Erase, kaiko::GameSessionResponseId_Disable);
                return;
            case SaveFlowState_Startup_ReadError:
                startUserInteraction(kaiko::GameSessionInteractionId_ReadError, kaiko::GameSessionResponseId_RetryFunction, kaiko::GameSessionResponseId_Disable);
                return;
            case SaveFlowState_Startup_ReadErrorErase:
                startUserInteraction(kaiko::GameSessionInteractionId_ReadError_Erase, kaiko::GameSessionResponseId_RetryFunction, kaiko::GameSessionResponseId_Erase);
                return;
            case SaveFlowState_Startup_ConfirmDisable:
                startUserInteraction(kaiko::GameSessionInteractionId_ConfirmDisable, kaiko::GameSessionResponseId_Disable, kaiko::GameSessionResponseId_None);
                return;
            case SaveFlowState_Startup_End_Usable:
                m_clientData[m_activeClientIndex].isSaveEnabled = true;
                m_clientData[m_activeClientIndex].state = SaveGameClientState_Ready;
                newState = SaveFlowState_Idle;
                break;
            case SaveFlowState_Startup_End_Disable:
                m_clientData[m_activeClientIndex].isSaveEnabled = false;
                m_clientData[m_activeClientIndex].state = SaveGameClientState_Ready;
                newState = SaveFlowState_Idle;
                break;
            case SaveFlowState_Write_Writing:
            {
                SaveGameClientData& client = m_clientData[m_activeClientIndex];
                client.isNewlyCreatedSaveGame = false;
                m_saveDataForIO.copy(client.saveData);
                m_saveDataOp.segmentIndex = 0;
                m_saveDataOp.pBuffer = m_saveDataForIO.data.getStart();
                m_saveDataOp.pNext = nullptr;
                if (SaveData::startWriteSaveData(m_pSaveDataSystem, m_activeClientIndex, client.currentSaveDatas[0], &m_saveDataOp))
                {
                    return;
                }
                debug::nativePrintArgs("[saveflow] Could not start save operation for player with saveDataProfilIndex %u\n", m_activeClientIndex);
                newState = SaveFlowState_Write_WriteError;
                break;
            }
            case SaveFlowState_Write_WriteError:
                startUserInteraction(kaiko::GameSessionInteractionId_WriteError, kaiko::GameSessionResponseId_RetryFunction, kaiko::GameSessionResponseId_Disable);
                return;
            case SaveFlowState_Write_Overwrite:
                startUserInteraction(kaiko::GameSessionInteractionId_Overwrite, kaiko::GameSessionResponseId_Overwrite, kaiko::GameSessionResponseId_DeviceSelection);
                return;
            case SaveFlowState_Write_Disable:
                m_clientData[m_activeClientIndex].isSaveEnabled = false;
                newState = SaveFlowState_Idle;
                break;
            case SaveFlowState_Write_Done:
            {
                SaveGameClientData& client = m_clientData[m_activeClientIndex];
                client.lastSavedDataId = m_saveDataForIO.id;
                if (client.lastSavedDataId < client.saveData.id)
                {
                    const uint32 clientIndex = m_activeClientIndex;
                    m_activeClientIndex = NoActiveClient;
                    startWriteFlow(clientIndex);
                    return;
                }
                newState = SaveFlowState_Idle;
                break;
            }
            default:
                // The confirm-erase states (and Invalid).
                startUserInteraction(kaiko::GameSessionInteractionId_ConfirmErase, kaiko::GameSessionResponseId_Cancel, kaiko::GameSessionResponseId_Erase);
                return;
            }
            m_state = newState;
        }
    }

    // 0x00CCDC90 (:1024): the provider binds the client's slot to its user; the client starts from the default data
    bool SaveDataHandler::connectUser(uint32 profileIndex)
    {
        if (!m_pSaveDataSystem->pProvider->connectProfileWithUser(profileIndex, m_clientData[profileIndex].userAccountId))
        {
            debug::nativePrintArgs("[saveflow] Connecting profile with index %u failed!\n", profileIndex);
            return false;
        }
        SaveGameClientData& client = m_clientData[profileIndex];
        memcpy(client.saveData.data.getStart(), m_defaultSaveGameData.getStart(), client.saveData.data.getSize());
        return true;
    }

    // 0x00CCDD10 (:1043)
    bool SaveDataHandler::disconnectUser(uint32 profileIndex)
    {
        if (!m_pSaveDataSystem->pProvider->disconnectProfile(profileIndex))
        {
            debug::nativePrintArgs("[saveflow] Disconnecting profile with index %u failed!\n", profileIndex);
            return false;
        }
        return true;
    }

    // 0x00CCDD50 (:1055)
    bool SaveDataHandler::hasNewlyCreatedSaveGame(uint32 profileIndex)
    {
        return m_clientData[profileIndex].isNewlyCreatedSaveGame;
    }
}
