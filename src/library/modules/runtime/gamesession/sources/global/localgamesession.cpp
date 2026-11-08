// Original: library/modules/runtime/gamesession/sources/global/localgamesession.cpp
//
// Partial (multiplayer.md MP2): the members the remote game session and the multiplayer platform layer call: the
// local users' queries, the profile and remote user id of a user, presence, and the interaction query (controller
// disconnected, sign-in UI, skipped sign-in, signed out or switched account, then the save data handler's and the
// system services' interactions). orig_main.md OM3 adds create/update/destroy, the user removal and the interaction
// answer, which keen::rfg::Application runs. stub_impl.md S2 adds the rest: adding a local player, saving, the
// achievements and the hero stats. A local player id is an index into the user data (0xFF: none). The inline
// functions of localgamesession.hpp are defined here too.
//
// Deviation (OpenRFG with --no-steam; application.cpp deviation 4): rfg.exe always has a user account system. Without
// one the player has no account, and goes straight to the save data as an anonymous user: never signed in, so no
// achievements or online functions (hasExpectedUserId stays false), but its local saves work.
//
// Note (decompiled behaviour, kept as is): setInteractionResponse ends the process on an unknown user account answer.

#include "unit4/gamesession/localgamesession.hpp"
#include "keen/base/string.hpp"
#include "keen/savedata/savedatahandler.hpp"
#include "keen/useraccount/steam/useraccountsystem_steam.hpp"
#include "library/modules/runtime/systemservices/sources/steam/systemservices_steam.hpp"
#include "keen/base/memoryallocator.hpp"
#include "keen/input/inputsystem.hpp"
#include "keen/useraccount/useraccountsystem.hpp"

#include <new>
#include <stdlib.h>
#include <string.h>

namespace unit4
{
    static const keen::LocalPlayerIdStructureType* const No_local_player = (const keen::LocalPlayerIdStructureType*)0xFF; // magic number: no local player

    static uint32 player_index(const keen::LocalPlayerIdStructureType* playerId)
    {
        return (uint32)(uintptr_t)playerId & 0xFF;
    }

    // 0x00458A60 (:51)
    SessionInteractionData::SessionInteractionData()
    {
        id = (uint32)kaiko::GameSessionInteractionId_Invalid;
        responseOptionCount = 0;
        params.parameterCount = 0;
        user.isValid = false;
    }

    // 0x007C6CC0
    SaveDataDescriptionInterface::SaveDataDescriptionInterface()
    {
    }

    // 0x007B9A40 (localgamesession.hpp:43)
    SaveDataDescriptionInterface::~SaveDataDescriptionInterface()
    {
    }

    // 0x00C11380 (:19): all zeros
    void SaveDataDescriptionInterface::initializeSaveDataToDefault(void* pData)
    {
        memset(pData, 0, getSaveDataSize());
    }

    // 0x00459780
    LocalGameSession::LocalGameSession()
    {
    }

    // 0x00459790
    LocalGameSession::~LocalGameSession()
    {
    }

    // 0x00C123B0
    bool LocalGameSession::didLocalPlayerExit(const keen::LocalPlayerIdStructureType* playerId) const
    {
        const uint32 index = player_index(playerId);
        if (index < m_userData.getSize() && m_userData[index].state != LocalUserState_Unused)
        {
            return m_userData[index].state == LocalUserState_Exited;
        }
        return false;
    }

    // 0x00C11490
    bool LocalGameSession::hasLocalPlayer(const keen::LocalPlayerIdStructureType* playerId) const
    {
        const uint32 index = player_index(playerId);
        if (index < m_userData.getSize())
        {
            return m_userData[index].state != LocalUserState_Unused;
        }
        return false;
    }

    // 0x00C114C0: a user with save data, or a guest
    bool LocalGameSession::isLocalPlayerReady(const keen::LocalPlayerIdStructureType* playerId) const
    {
        const uint32 index = player_index(playerId);
        if (index >= m_userData.getSize() || m_userData[index].state == LocalUserState_Unused)
        {
            return false;
        }
        return m_userData[index].state == LocalUserState_Savedata || m_userData[index].playerType == LocalPlayerType_Guest;
    }

    // 0x00C11510
    uint32 LocalGameSession::getLocalPlayerCount() const
    {
        uint32 count = 0;
        for (uint32 i = 0; i < m_userData.getSize(); i++)
        {
            if (m_userData[i].state != LocalUserState_Unused)
            {
                count++;
            }
        }
        return count;
    }

    // 0x00C113A0 (:338): an unused slot starts waiting for its account (update() takes it from there)
    bool LocalGameSession::addLocalPlayer(const keen::LocalPlayerIdStructureType* playerId, LocalPlayerType type)
    {
        const uint32 index = player_index(playerId);
        if (m_userData[index].state != LocalUserState_Unused)
        {
            return false;
        }
        LocalUserData& user = m_userData[index];
        user.state = LocalUserState_NoAccount;
        user.stateBeforeProfileChange = LocalUserState_NoAccount;
        user.playerType = type;
        // Magic number: 0xFFFF, no save profile yet.
        user.saveProfileIndex = 0xFFFF;
        user.hasExpectedUserId = false;
        user.skippedSignIn = false;
        user.controllerDisconnected = false;
        keen::UserAccount account;
        if (keen::useraccount::getUserAccount(&account, m_pUserAccountSystem, playerId) && account.isLoggedIn)
        {
            systemservices::setupAchievements(m_pSystemServices, account);
        }
        return true;
    }

    // 0x00C11540 (:440): the save game of a user whose save data is in
    void* LocalGameSession::getSaveData(const keen::LocalPlayerIdStructureType* playerId)
    {
        const uint32 index = player_index(playerId);
        if (index >= m_userData.getSize() || m_userData[index].state == LocalUserState_Unused || m_userData[index].state != LocalUserState_Savedata)
        {
            return nullptr;
        }
        uint32 saveDataSize;
        return m_pSaveDataHandler->getSaveGameData(&saveDataSize, m_userData[index].saveProfileIndex);
    }

    // 0x00C115A0 (:461)
    void LocalGameSession::startSave(const keen::LocalPlayerIdStructureType* playerId)
    {
        const uint32 index = player_index(playerId);
        if (index < m_userData.getSize() && m_userData[index].state != LocalUserState_Unused)
        {
            m_pSaveDataHandler->requestSave(m_userData[index].saveProfileIndex);
        }
    }

    // 0x00C11100 (:473)
    bool LocalGameSession::isSaveInProgress() const
    {
        return m_pSaveDataHandler->isSaveInProgress();
    }

    // 0x00C11A40 (:662): for a user signed in with its expected account
    bool LocalGameSession::setAchievementProgress(const keen::LocalPlayerIdStructureType* playerId, uint32 achievementId, uint32 status)
    {
        const LocalUserData& user = m_userData[player_index(playerId)];
        if (!user.hasExpectedUserId || !user.expectedUser.isLoggedIn)
        {
            return false;
        }
        return systemservices::updateAchievementProgress(m_pSystemServices, user.expectedUser, achievementId, status);
    }

    // 0x00C11A80 (:685)
    void LocalGameSession::showAchievementsUi(const keen::LocalPlayerIdStructureType* playerId)
    {
        const LocalUserData& user = m_userData[player_index(playerId)];
        if (user.hasExpectedUserId && user.expectedUser.isLoggedIn)
        {
            systemservices::showAchievementsUi(user.expectedUser);
        }
    }

    // 0x00C11BB0 (:852)
    bool LocalGameSession::updateHeroStatSet(const keen::LocalPlayerIdStructureType* playerId, const uint32 statId, int newValue)
    {
        const LocalUserData& user = m_userData[player_index(playerId)];
        if (!user.hasExpectedUserId || !user.expectedUser.isLoggedIn)
        {
            return false;
        }
        return systemservices::updateHeroStatSet(m_pSystemServices, user.expectedUser, statId, (double)newValue);
    }

    // 0x00C11C00 (:868)
    bool LocalGameSession::updateHeroStatSet(const keen::LocalPlayerIdStructureType* playerId, const uint32 statId, double newValue)
    {
        const LocalUserData& user = m_userData[player_index(playerId)];
        if (!user.hasExpectedUserId || !user.expectedUser.isLoggedIn)
        {
            return false;
        }
        return systemservices::updateHeroStatSet(m_pSystemServices, user.expectedUser, statId, newValue);
    }

    // 0x00C11C50 (:885)
    bool LocalGameSession::updateHeroStatAdd(const keen::LocalPlayerIdStructureType* playerId, const uint32 statId, int addend)
    {
        const LocalUserData& user = m_userData[player_index(playerId)];
        if (!user.hasExpectedUserId || !user.expectedUser.isLoggedIn)
        {
            return false;
        }
        return systemservices::updateHeroStatAdd(m_pSystemServices, user.expectedUser, statId, (double)addend);
    }

    // 0x00C11CA0 (:901)
    bool LocalGameSession::updateHeroStatAdd(const keen::LocalPlayerIdStructureType* playerId, const uint32 statId, double addend)
    {
        const LocalUserData& user = m_userData[player_index(playerId)];
        if (!user.hasExpectedUserId || !user.expectedUser.isLoggedIn)
        {
            return false;
        }
        return systemservices::updateHeroStatAdd(m_pSystemServices, user.expectedUser, statId, addend);
    }

    // 0x007CAF90 (localgamesession.hpp:222): a user with save data whose saving got disabled
    bool LocalGameSession::isSavingDisabledForUser(const keen::LocalPlayerIdStructureType* playerId)
    {
        const LocalUserData& user = m_userData[player_index(playerId)];
        return user.state == LocalUserState_Savedata && !m_pSaveDataHandler->isSaveEnabled(user.saveProfileIndex);
    }

    // 0x00C115F0
    bool LocalGameSession::isInteractionRequired(SessionInteractionData* pInteractionData) const
    {
        LocalGameSession* pThis = const_cast<LocalGameSession*>(this); // the original records the starter
        for (uint32 i = 0; i < m_userData.getSize(); i++)
        {
            if (m_userData[i].state != LocalUserState_Unused && m_userData[i].controllerDisconnected)
            {
                pInteractionData->responseOptionCount = 0;
                pInteractionData->id = kaiko::GameSessionInteractionId_GenericControllerDisconnected;
                pThis->m_lastInteractionStartedBy = InteractionStarter_Input;
                return true;
            }
        }
        if (m_showingSignInUi)
        {
            pInteractionData->responseOptionCount = 0;
            pInteractionData->id = kaiko::GameSessionInteractionId_DisplayingSignInUi;
            pThis->m_lastInteractionStartedBy = InteractionStarter_UserAccountSystem;
            return true;
        }
        for (uint32 i = 0; i < m_userData.getSize(); i++)
        {
            const LocalUserState state = m_userData[i].state;
            if (state == LocalUserState_SkippedSignIn)
            {
                pInteractionData->id = kaiko::GameSessionInteractionId_SignInSkipped;
                pInteractionData->responseOptionCount = 2;
                pInteractionData->responseOptions[0] = kaiko::GameSessionResponseId_RetrySignIn;
                pInteractionData->responseOptions[1] = kaiko::GameSessionResponseId_SkipSignIn;
                pThis->m_lastInteractionStartedBy = InteractionStarter_UserAccountSystem;
                return true;
            }
            if (state == LocalUserState_SignedOut || state == LocalUserState_SwitchedAccount)
            {
                pInteractionData->id = state == LocalUserState_SignedOut ? kaiko::GameSessionInteractionId_SignedOut : kaiko::GameSessionInteractionId_SwitchedAccount;
                pInteractionData->responseOptionCount = 2;
                pInteractionData->responseOptions[0] = kaiko::GameSessionResponseId_RepeatSignIn;
                pInteractionData->responseOptions[1] = kaiko::GameSessionResponseId_Exit;
                pThis->m_lastInteractionStartedBy = InteractionStarter_UserAccountSystem;
                pInteractionData->params.parameterCount = 1;
                fillInteractionUserNameParam(pInteractionData->params.parameters[0], (const char*)m_userData[i].expectedUser.name.providerData);
                return true;
            }
        }
        keen::SaveDataInteractionData saveDataInteraction;
        saveDataInteraction.id = kaiko::GameSessionInteractionId_Invalid;
        saveDataInteraction.responseOptionCount = 0;
        if (m_pSaveDataHandler->isInteractionRequired(&saveDataInteraction))
        {
            pInteractionData->id = saveDataInteraction.id;
            pInteractionData->responseOptionCount = saveDataInteraction.responseOptionCount > 1 ? 2 : saveDataInteraction.responseOptionCount;
            pInteractionData->user.isValid = false;
            for (uint32 i = 0; i < pInteractionData->responseOptionCount; i++)
            {
                pInteractionData->responseOptions[i] = saveDataInteraction.responseOptions[i];
            }
            pThis->m_lastInteractionStartedBy = InteractionStarter_SaveData;
            return true;
        }
        SystemServicesInteractionData servicesInteraction;
        if (!systemservices::isInteractionRequired(&servicesInteraction, m_pSystemServices))
        {
            return false;
        }
        memcpy(pInteractionData->user.providerData, servicesInteraction.user.providerData, sizeof(pInteractionData->user.providerData));
        pInteractionData->id = servicesInteraction.id;
        pInteractionData->user.isValid = servicesInteraction.user.isValid;
        pInteractionData->responseOptionCount = servicesInteraction.responseOptionCount > 1 ? 2 : servicesInteraction.responseOptionCount;
        for (uint32 i = 0; i < pInteractionData->responseOptionCount; i++)
        {
            pInteractionData->responseOptions[i] = servicesInteraction.responseOptions[i];
        }
        pThis->m_lastInteractionStartedBy = InteractionStarter_SystemServices;
        return true;
    }

    // 0x00C11110
    bool LocalGameSession::setPresence(uint32 stringIndex)
    {
        return systemservices::setPresence(m_pSystemServices, stringIndex);
    }

    // 0x00C11AB0: the user's expected account (the one it signed in with)
    bool LocalGameSession::getProfile(keen::UserAccount& outProfile, const keen::LocalPlayerIdStructureType* localPlayerId)
    {
        if (localPlayerId == No_local_player || !m_userData[player_index(localPlayerId)].hasExpectedUserId)
        {
            return false;
        }
        outProfile = m_userData[player_index(localPlayerId)].expectedUser;
        return true;
    }

    // 0x00C11AF0
    bool LocalGameSession::getRemoteUserId(RemoteUserId& outId, const keen::LocalPlayerIdStructureType* localPlayerId)
    {
        if (localPlayerId == No_local_player || !m_userData[player_index(localPlayerId)].hasExpectedUserId)
        {
            return false;
        }
        systemservices::createRemoteUserIdFromUserAccountId(&outId, m_userData[player_index(localPlayerId)].expectedUser.id);
        return true;
    }

    // 0x00883600 (:132)
    SystemServices* LocalGameSession::getSystemServices()
    {
        return m_pSystemServices;
    }

    // 0x009702D0 (:213)
    bool LocalGameSession::hasLocalPlayerSkippedSignIn(const keen::LocalPlayerIdStructureType* playerId)
    {
        const LocalUserData& user = m_userData[player_index(playerId)];
        return user.state != LocalUserState_Unused && user.skippedSignIn;
    }

    // 0x00970300 (:231): the first user with save data and an account that is logged in
    const keen::LocalPlayerIdStructureType* LocalGameSession::getFirstSignedInUser()
    {
        for (uint32 i = 0; i < m_userData.getSize(); i = (uint8)(i + 1))
        {
            const LocalUserData& user = m_userData[i];
            if (user.state == LocalUserState_Savedata && user.playerType == LocalPlayerType_WithAccount && keen::useraccount::isUserLoggedIn(m_pUserAccountSystem, user.expectedUser.id))
            {
                return (const keen::LocalPlayerIdStructureType*)(uintptr_t)i;
            }
        }
        return No_local_player;
    }

    // 0x00C11010 (:246): names of 64 bytes or more are cut to 61 bytes and "..."
    void LocalGameSession::fillInteractionUserNameParam(kaiko::GameSessionInteractionParameter& param, const char* pUserName) const
    {
        param.type = kaiko::GameSessionInteractionParameterType_UserNameString;
        if (keen::getStringLength(pUserName) >= sizeof(param.value.stringValue))
        {
            const uint32 length = keen::copyUTF8String(param.value.stringValue, sizeof(param.value.stringValue) - 3, pUserName);
            param.value.stringValue[length] = '.';
            param.value.stringValue[length + 1] = '.';
            param.value.stringValue[length + 2] = '.';
            param.value.stringValue[length + 3] = '\0';
            return;
        }
        keen::copyUTF8String(param.value.stringValue, sizeof(param.value.stringValue), pUserName);
    }

    // ---- orig_main.md OM3: the session's life cycle, which keen::rfg::Application runs (create in initialize, update
    // every frame from updateFromSimulationThread, destroy in shutdown), and the answer to an interaction.

    LocalGameSession* LocalGameSession::m_pInstance; // 0x038B70D0, for checkSaveGameData

    // 0x00C12250
    // The save data handler (with the game's default save as its default data), the services and the local users, all
    // unused until a player is added.
    void LocalGameSession::create(keen::MemoryAllocator* pAllocator, keen::SaveDataSystem* pSaveDataSystem, SystemServices* pSystemServices, keen::UserAccountSystem* pAccountSystem, uint32 numLocalUsers,
                                  SaveDataDescriptionInterface* pSaveDataInterface, keen::InputSystem* pInputSystem)
    {
        m_pInstance = this;
        m_pSaveDataInterface = pSaveDataInterface;
        void* pHandlerMemory = pAllocator->allocate(sizeof(keen::SaveDataHandler), 4, 0, nullptr);
        m_pSaveDataHandler = pHandlerMemory != nullptr ? new (pHandlerMemory) keen::SaveDataHandler() : nullptr;
        m_pSaveDataHandler->create(pAllocator, pSaveDataSystem, numLocalUsers, pSaveDataInterface->getSaveDataSize());
        void* pDefaultData = pAllocator->allocate(pSaveDataInterface->getSaveDataSize(), 0x10, 0, nullptr);
        pSaveDataInterface->initializeSaveDataToDefault(pDefaultData);
        m_pSaveDataHandler->setDefaultSaveGameData(pDefaultData, pSaveDataInterface->getSaveDataSize());
        pAllocator->free(pDefaultData);
        m_pSaveDataHandler->setCheckSaveGameDataCallback(checkSaveGameData);
        m_pUserAccountSystem = pAccountSystem;
        m_pSystemServices = pSystemServices;
        if (numLocalUsers != 0)
        {
            m_userData.create(pAllocator, numLocalUsers, 0x10);
            for (uint32 i = 0; i < numLocalUsers; i++)
            {
                LocalUserData& user = m_userData[i];
                user.state = LocalUserState_Unused;
                user.stateBeforeProfileChange = LocalUserState_Unused;
                // Magic number: 0xFFFF, no save profile.
                user.saveProfileIndex = 0xFFFF;
                user.skippedSignIn = false;
                user.hasExpectedUserId = false;
                user.controllerDisconnected = false;
            }
        }
        m_lastInteractionStartedBy = InteractionStarter_Invalid;
        m_showingSignInUi = false;
        // Magic number: 1, no user account operation.
        m_userAccountOperationHandle = 1;
        m_pInputSystem = pInputSystem;
        memset(&m_lastUsedControllerInfo, 0, sizeof(m_lastUsedControllerInfo));
        m_lastUsedControllerInfo.controllerClass = keen::ControllerClass_None;
        m_lastUsedControllerInfo.type = keen::ControllerType_Unkown;
    }

    // 0x00C12B60
    void LocalGameSession::destroy(keen::MemoryAllocator* pAllocator)
    {
        m_userData.destroy(pAllocator);
        m_pSaveDataHandler->destroy(pAllocator);
        if (m_pSaveDataHandler != nullptr)
        {
            // Original: freed without the destructor.
            pAllocator->free(m_pSaveDataHandler);
        }
        m_pSaveDataHandler = nullptr;
        m_pUserAccountSystem = nullptr;
        m_pSystemServices = nullptr;
    }

    // 0x00C110D0
    // The save data handler's check of loaded data: the game's size and the game's validation.
    bool LocalGameSession::checkSaveGameData(const void* pData, uint32 dataSize)
    {
        if (dataSize != m_pInstance->m_pSaveDataInterface->getSaveDataSize())
        {
            return false;
        }
        return m_pInstance->m_pSaveDataInterface->isSaveDataValid(pData);
    }

    // 0x00C11B40
    // A player with a pad needs it connected; a player without one needs some input device.
    bool LocalGameSession::hasConnectedController(const keen::LocalPlayerIdStructureType* localPlayerId)
    {
        const uint32 controllerId = keen::input::getControllerIdForPlayer(localPlayerId, keen::ControllerClass_Controller);
        if (controllerId == 0xFFFFFFFF)
        {
            const uint32 index = player_index(localPlayerId);
            if (index < m_userData.getSize() && m_userData[index].state != LocalUserState_Unused &&
                keen::input::getPrimaryInputDeviceClass(m_pInputSystem, localPlayerId) == keen::ControllerClass_None)
            {
                return false;
            }
            return true;
        }
        return keen::input::isControllerConnected(m_pInputSystem, controllerId);
    }

    // 0x00C12480
    // A player (with its controller) whose account is gone or changed, or a player who skipped the sign-in and is now
    // signed in (exited, to be added again).
    bool LocalGameSession::hasSignedOutOrSwitchedAccounts(const keen::LocalPlayerIdStructureType* localPlayerId, LocalUserState& newState)
    {
        const uint32 index = player_index(localPlayerId);
        LocalUserData& user = m_userData[index];
        if (user.controllerDisconnected || m_pUserAccountSystem == nullptr) // deviation: no account to lose without Steam
        {
            return false;
        }
        keen::UserAccount account;
        if (user.skippedSignIn && hasConnectedController(localPlayerId))
        {
            if (keen::useraccount::getUserAccount(&account, m_pUserAccountSystem, localPlayerId) && account.isLoggedIn)
            {
                user.stateBeforeProfileChange = user.state;
                newState = LocalUserState_Exited;
                return true;
            }
            return false;
        }
        if (!keen::useraccount::getUserAccount(&account, m_pUserAccountSystem, localPlayerId))
        {
            user.stateBeforeProfileChange = user.state;
            newState = LocalUserState_SignedOut;
            return true;
        }
        if (user.hasExpectedUserId && !keen::useraccount::isUserAccountIdEqual(user.expectedUser.id, account.id))
        {
            user.stateBeforeProfileChange = user.state;
            newState = LocalUserState_SwitchedAccount;
            return true;
        }
        if (!account.isLoggedIn)
        {
            user.stateBeforeProfileChange = user.state;
            newState = LocalUserState_SignedOut;
            return true;
        }
        return false;
    }

    // 0x00C125F0
    // The services and the save data handler, then each user's controller and sign-in state (or the sign-in UI's
    // result), and the last used pad.
    void LocalGameSession::update(float timeStep)
    {
        systemservices::updateSystemServices(m_pSystemServices, timeStep);
        m_pSaveDataHandler->update();
        if (m_userAccountOperationHandle == 1)
        {
            for (uint8 i = 0; i < m_userData.getSize(); i++)
            {
                const keen::LocalPlayerIdStructureType* playerId = (const keen::LocalPlayerIdStructureType*)(uintptr_t)i;
                if (m_userData[i].state != LocalUserState_Unused)
                {
                    if (!m_userData[i].controllerDisconnected)
                    {
                        if (!hasConnectedController(playerId))
                        {
                            keen::input::clearControllerPlayerId(m_pInputSystem, playerId);
                            keen::input::setControllerAutoCatch(m_pInputSystem, playerId, keen::InputSystemControllerAutoCatchType_AnyButton);
                            m_userData[i].controllerDisconnected = true;
                        }
                    }
                    else if (hasConnectedController(playerId))
                    {
                        m_userData[i].controllerDisconnected = false;
                    }
                }
                LocalUserData& user = m_userData[i];
                keen::UserAccount account;
                switch (user.state)
                {
                case LocalUserState_NoAccount:
                    if (m_pUserAccountSystem == nullptr) // deviation: without Steam, on to the save data
                    {
                        m_userData[i].state = LocalUserState_NoSavedata;
                    }
                    else if (user.playerType == LocalPlayerType_WithAccount && user.state == LocalUserState_NoAccount)
                    {
                        LocalUserState ignored;
                        if (!keen::useraccount::getUserAccount(&account, m_pUserAccountSystem, playerId) || !account.isLoggedIn || hasSignedOutOrSwitchedAccounts(playerId, ignored))
                        {
                            if (keen::useraccount::getInitialUserAccount(&account, m_pUserAccountSystem))
                            {
                                m_userData[i].expectedUser = account;
                                m_userData[i].hasExpectedUserId = true;
                                m_userData[i].state = LocalUserState_NoSavedata;
                            }
                        }
                        else
                        {
                            m_userData[i].expectedUser = account;
                            m_userData[i].hasExpectedUserId = true;
                            m_userData[i].state = LocalUserState_NoSavedata;
                        }
                    }
                    break;
                case LocalUserState_NoSavedata:
                    if (user.saveProfileIndex == 0xFFFF)
                    {
                        LocalUserState newState;
                        if (m_pUserAccountSystem == nullptr) // deviation: an anonymous user without Steam
                        {
                            keen::UserAccountId noAccount;
                            memset(&noAccount, 0, sizeof(noAccount));
                            m_pSaveDataHandler->registerUser(&m_userData[i].saveProfileIndex, noAccount);
                        }
                        else if (hasSignedOutOrSwitchedAccounts(playerId, newState))
                        {
                            m_userData[i].state = newState;
                        }
                        else if (keen::useraccount::getUserAccount(&account, m_pUserAccountSystem, playerId))
                        {
                            m_pSaveDataHandler->registerUser(&m_userData[i].saveProfileIndex, account.id);
                        }
                        else
                        {
                            m_userData[i].stateBeforeProfileChange = m_userData[i].state;
                            m_userData[i].state = LocalUserState_SignedOut;
                        }
                    }
                    else if (m_pSaveDataHandler->isUserRegistered(user.saveProfileIndex))
                    {
                        m_userData[i].state = LocalUserState_Savedata;
                    }
                    break;
                case LocalUserState_Savedata:
                {
                    LocalUserState newState;
                    if (hasSignedOutOrSwitchedAccounts(playerId, newState))
                    {
                        m_userData[i].state = newState;
                    }
                    break;
                }
                case LocalUserState_SkippedSignIn:
                case LocalUserState_SignedOut:
                case LocalUserState_SwitchedAccount:
                    if (user.state == LocalUserState_SkippedSignIn)
                    {
                        if (hasConnectedController(playerId))
                        {
                            m_userData[i].controllerDisconnected = false;
                            if (keen::useraccount::getUserAccount(&account, m_pUserAccountSystem, playerId) && account.isLoggedIn)
                            {
                                m_userData[i].state = m_userData[i].stateBeforeProfileChange;
                            }
                        }
                    }
                    else if (keen::useraccount::getUserAccount(&account, m_pUserAccountSystem, playerId) && m_userData[i].hasExpectedUserId && account.isLoggedIn &&
                             keen::useraccount::isUserAccountIdEqual(m_userData[i].expectedUser.id, account.id))
                    {
                        m_userData[i].state = m_userData[i].stateBeforeProfileChange;
                    }
                    break;
                default:
                    break;
                }
            }
        }
        else
        {
            const keen::UserAccountAsyncOperationState state = keen::useraccount::getUserAccountOperationState(m_pUserAccountSystem, m_userAccountOperationHandle);
            if (state == keen::UserAccountAsyncOperationState_Invalid)
            {
                m_showingSignInUi = false;
                m_userAccountOperationHandle = 1;
            }
            else if (state == keen::UserAccountAsyncOperationState_Error)
            {
                m_showingSignInUi = false;
                keen::useraccount::getUserAccountOperationResult(m_pUserAccountSystem, m_userAccountOperationHandle);
                m_userAccountOperationHandle = 1;
                // The first user waiting for an account skipped the sign-in, or lost its account.
                for (uint32 i = 0; i < m_userData.getSize(); i++)
                {
                    if (m_userData[i].state == LocalUserState_NoAccount)
                    {
                        m_userData[i].stateBeforeProfileChange = m_userData[i].state;
                        m_userData[i].state = m_userData[i].hasExpectedUserId ? LocalUserState_SignedOut : LocalUserState_SkippedSignIn;
                        break;
                    }
                }
            }
            else if (state == keen::UserAccountAsyncOperationState_Success)
            {
                m_showingSignInUi = false;
                keen::useraccount::getUserAccountOperationResult(m_pUserAccountSystem, m_userAccountOperationHandle);
                m_userAccountOperationHandle = 1;
                // The first user waiting for an account takes the signed in one (unless it is another than expected).
                for (uint32 i = 0; i < m_userData.getSize(); i++)
                {
                    if (m_userData[i].state != LocalUserState_NoAccount)
                    {
                        continue;
                    }
                    keen::UserAccount account;
                    const bool found = keen::useraccount::getUserAccount(&account, m_pUserAccountSystem, (const keen::LocalPlayerIdStructureType*)(uintptr_t)(i & 0xFF));
                    if (m_userData[i].hasExpectedUserId)
                    {
                        if (!found)
                        {
                            break;
                        }
                        if (!keen::useraccount::isUserAccountIdEqual(m_userData[i].expectedUser.id, account.id))
                        {
                            m_userData[i].stateBeforeProfileChange = m_userData[i].state;
                            m_userData[i].state = LocalUserState_SwitchedAccount;
                            break;
                        }
                    }
                    if (account.isLoggedIn)
                    {
                        m_userData[i].state = LocalUserState_NoSavedata;
                        m_userData[i].expectedUser = account;
                        m_userData[i].hasExpectedUserId = true;
                        systemservices::setupAchievements(m_pSystemServices, account);
                    }
                    break;
                }
            }
        }
        // Local player 0's pad.
        const uint32 controllerId = keen::input::getControllerIdForPlayer(nullptr, keen::ControllerClass_Controller);
        if (controllerId != 0xFFFFFFFF)
        {
            keen::input::getControllerInfo(&m_lastUsedControllerInfo, controllerId);
        }
    }

    // 0x00C123F0
    void LocalGameSession::removeLocalPlayer(const keen::LocalPlayerIdStructureType* playerId)
    {
        const uint32 index = player_index(playerId);
        if (index >= m_userData.getSize() || m_userData[index].state == LocalUserState_Unused)
        {
            return;
        }
        if (m_userData[index].saveProfileIndex != 0xFFFF)
        {
            m_pSaveDataHandler->unregisterUser(m_userData[index].saveProfileIndex, false);
        }
        m_userData[index].saveProfileIndex = 0xFFFF;
        m_userData[index].state = LocalUserState_Unused;
        m_userData[index].stateBeforeProfileChange = LocalUserState_Unused;
        m_userData[index].hasExpectedUserId = false;
        m_userData[index].skippedSignIn = false;
    }

    // 0x00C118B0
    // The answer goes to whoever started the interaction; the user account answers change the users' states.
    void LocalGameSession::setInteractionResponse(uint32 id)
    {
        switch (m_lastInteractionStartedBy)
        {
        case InteractionStarter_SaveData:
            m_pSaveDataHandler->setInteractionResponse((kaiko::GameSessionResponseId)id);
            break;
        case InteractionStarter_SystemServices:
            systemservices::setInteractionResponse(m_pSystemServices, id);
            break;
        case InteractionStarter_UserAccountSystem:
            for (uint32 i = 0; i < m_userData.getSize(); i++)
            {
                LocalUserData& user = m_userData[i];
                if (id == kaiko::GameSessionResponseId_RetrySignIn)
                {
                    if (user.state == LocalUserState_SkippedSignIn)
                    {
                        user.state = LocalUserState_NoAccount;
                    }
                }
                else if (id == kaiko::GameSessionResponseId_RepeatSignIn)
                {
                    if (user.state == LocalUserState_SignedOut || user.state == LocalUserState_SwitchedAccount)
                    {
                        user.state = LocalUserState_NoAccount;
                    }
                }
                else if (id == kaiko::GameSessionResponseId_SkipSignIn)
                {
                    if (user.state == LocalUserState_SkippedSignIn)
                    {
                        user.skippedSignIn = true;
                        user.state = LocalUserState_NoSavedata;
                    }
                }
                else if (id == kaiko::GameSessionResponseId_Exit)
                {
                    if (user.state != LocalUserState_Unused)
                    {
                        user.state = LocalUserState_Exited;
                    }
                }
                else
                {
                    exit(1); // original: an unknown answer ends the process
                }
            }
            break;
        default:
            break;
        }
        m_lastInteractionStartedBy = InteractionStarter_Invalid;
    }
}
