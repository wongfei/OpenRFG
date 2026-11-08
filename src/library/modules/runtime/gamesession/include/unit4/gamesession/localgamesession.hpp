// Original: library/modules/runtime/gamesession/include/unit4/gamesession/localgamesession.hpp
// Implementation: library/modules/runtime/gamesession/sources/global/localgamesession.cpp
#pragma once

#include "keen/base/array.hpp"
#include "keen/base/localplayerid.hpp"
#include "keen/base/types.hpp"
#include "keen/input/inputsystem.hpp"
#include "keen/useraccount/useraccounttypes.hpp"
#include "kaiko/gamesessioninteractions/gamesessioninteractions.hpp"
#include "unit4/systemservices/systemservices.hpp"

namespace keen
{
    class MemoryAllocator;
    struct SaveDataSystem;
    class SaveDataHandler;
    struct UserAccountSystem;
    struct InputSystem;
}

namespace unit4
{
    // Placement inferred.
    enum LocalPlayerType
    {
        LocalPlayerType_WithAccount = 0,
        LocalPlayerType_Guest = 1,
    };

    // The game describes its save data through this interface (rfg: keen::rfg::RfgSaveInterface).
    // size 0x4
    class SaveDataDescriptionInterface
    {
    public:
        virtual ~SaveDataDescriptionInterface(); // vtbl+0x00 0x007B9A40 (:43)
        virtual uint32 getSaveDataSize() const = 0; // vtbl+0x04
        virtual bool isSaveDataValid(const void*) = 0; // vtbl+0x08
        virtual void initializeSaveDataToDefault(void* pData); // vtbl+0x0C 0x00C11380
        SaveDataDescriptionInterface(); // 0x007C6CC0

        // 0x00 vfptr
    };

    // A pending interaction (e.g. "save data corrupt") with the response options the user can pick.
    // size 0x130
    struct SessionInteractionData
    {
        enum
        {
            MaxResponseOptionCount = 2,
        };

        SessionInteractionData(); // 0x00458A60 (:51)

        uint32 id; // 0x00 a kaiko::GameSessionInteractionId (inferred from use)
        uint32 responseOptions[2]; // 0x04 kaiko::GameSessionResponseId values (inferred from use)
        uint32 responseOptionCount; // 0x0C
        keen::UserAccountId user; // 0x10
        kaiko::GameSessionInteractionParameterList params; // 0x1C
    };

    // Owns the local users (sign-in state, save data, controller binding) and turns platform events
    // into SessionInteractionData for the game to show. Updated every simulation frame from
    // rfg::Application::updateFromSimulationThread.
    // size 0x40
    class LocalGameSession
    {
    public:
        enum LocalUserState
        {
            LocalUserState_Unused = 0,
            LocalUserState_NoAccount = 1,
            LocalUserState_NoSavedata = 2,
            LocalUserState_Savedata = 3,
            LocalUserState_SkippedSignIn = 4,
            LocalUserState_SignedOut = 5,
            LocalUserState_SwitchedAccount = 6,
            LocalUserState_Exited = 7,
        };

        enum InteractionStarter
        {
            InteractionStarter_Invalid = 0,
            InteractionStarter_SaveData = 1,
            InteractionStarter_SystemServices = 2,
            InteractionStarter_Input = 3,
            InteractionStarter_UserAccountSystem = 4,
        };

        // size 0x124
        struct LocalUserData
        {
            bool skippedSignIn; // 0x00
            bool hasExpectedUserId; // 0x01
            keen::UserAccount expectedUser; // 0x02
            LocalUserState state; // 0x110
            LocalUserState stateBeforeProfileChange; // 0x114
            LocalPlayerType playerType; // 0x118
            uint32 saveProfileIndex; // 0x11C
            bool controllerDisconnected; // 0x120
        };

        void create(keen::MemoryAllocator* pAllocator, keen::SaveDataSystem* pSaveDataSystem, SystemServices* pSystemServices, keen::UserAccountSystem* pAccountSystem, uint32 numLocalUsers, SaveDataDescriptionInterface* pSaveDataInterface, keen::InputSystem* pInputSystem); // 0x00C12250
        void destroy(keen::MemoryAllocator* pAllocator); // 0x00C12B60
        void update(float timeStep); // 0x00C125F0
        bool addLocalPlayer(const keen::LocalPlayerIdStructureType* playerId, LocalPlayerType type); // 0x00C113A0
        bool hasLocalPlayer(const keen::LocalPlayerIdStructureType* playerId) const; // 0x00C11490
        void removeLocalPlayer(const keen::LocalPlayerIdStructureType* playerId); // 0x00C123F0
        uint32 getLocalPlayerCount() const; // 0x00C11510
        bool didLocalPlayerExit(const keen::LocalPlayerIdStructureType* playerId) const; // 0x00C123B0
        bool isLocalPlayerReady(const keen::LocalPlayerIdStructureType* playerId) const; // 0x00C114C0
        void* getSaveData(const keen::LocalPlayerIdStructureType* playerId); // 0x00C11540
        void startSave(const keen::LocalPlayerIdStructureType* playerId); // 0x00C115A0
        bool isSaveInProgress() const; // 0x00C11100
        bool isInteractionRequired(SessionInteractionData* pInteractionData) const; // 0x00C115F0
        void setInteractionResponse(uint32 id); // 0x00C118B0
        bool setPresence(uint32 stringIndex); // 0x00C11110
        void showAchievementsUi(const keen::LocalPlayerIdStructureType* playerId); // 0x00C11A80
        bool setAchievementProgress(const keen::LocalPlayerIdStructureType* playerId, uint32 achievementId, uint32 status); // 0x00C11A40
        bool getProfile(keen::UserAccount& outProfile, const keen::LocalPlayerIdStructureType* localPlayerId); // 0x00C11AB0
        bool getRemoteUserId(RemoteUserId& outId, const keen::LocalPlayerIdStructureType* localPlayerId); // 0x00C11AF0
        bool updateHeroStatSet(const keen::LocalPlayerIdStructureType* playerId, const uint32 statId, double newValue); // 0x00C11C00
        bool updateHeroStatSet(const keen::LocalPlayerIdStructureType* playerId, const uint32 statId, int newValue); // 0x00C11BB0
        bool updateHeroStatAdd(const keen::LocalPlayerIdStructureType* playerId, const uint32 statId, double addend); // 0x00C11CA0
        bool updateHeroStatAdd(const keen::LocalPlayerIdStructureType* playerId, const uint32 statId, int addend); // 0x00C11C50
        SystemServices* getSystemServices(); // 0x00883600 (:132)
        keen::UserAccountSystem* getUserAccountSystem();

    private:
        static bool checkSaveGameData(const void* pData, uint32 dataSize); // 0x00C110D0
        bool hasSignedOutOrSwitchedAccounts(const keen::LocalPlayerIdStructureType* localPlayerId, LocalUserState& newState); // 0x00C12480
        bool hasConnectedController(const keen::LocalPlayerIdStructureType* localPlayerId); // 0x00C11B40

    public:
        bool hasLocalPlayerSkippedSignIn(const keen::LocalPlayerIdStructureType* playerId); // 0x009702D0 (:213)
        bool isSavingDisabledForUser(const keen::LocalPlayerIdStructureType* playerId); // 0x007CAF90 (:222)
        const keen::LocalPlayerIdStructureType* getFirstSignedInUser(); // 0x00970300 (:231)

    protected:
        void fillInteractionUserNameParam(kaiko::GameSessionInteractionParameter& param, const char* pUserName) const; // 0x00C11010 (:246)

    public:
        LocalGameSession(); // 0x00459780
        ~LocalGameSession(); // 0x00459790

    private:
        SaveDataDescriptionInterface* m_pSaveDataInterface; // 0x00
        keen::SaveDataHandler* m_pSaveDataHandler; // 0x04
        keen::UserAccountSystem* m_pUserAccountSystem; // 0x08
        SystemServices* m_pSystemServices; // 0x0C
        keen::InputSystem* m_pInputSystem; // 0x10
        bool m_showingSignInUi; // 0x14
        keen::Array<LocalUserData> m_userData; // 0x18
        uint32 m_userAccountOperationHandle; // 0x20
        static LocalGameSession* m_pInstance; // 0x038B70D0
        keen::ControllerInfo m_lastUsedControllerInfo; // 0x24
        InteractionStarter m_lastInteractionStartedBy; // 0x3C
    };
}
