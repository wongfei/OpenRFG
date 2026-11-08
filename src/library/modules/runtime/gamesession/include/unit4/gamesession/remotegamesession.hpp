// Original: library/modules/runtime/gamesession/include/unit4/gamesession/remotegamesession.hpp
// Implementation: library/modules/runtime/gamesession/sources/global/remotegamesession.cpp
//
// Online counterpart of LocalGameSession: network permissions, voice chat and the GameLink session (LAN or internet,
// both held by value and created on demand).
#pragma once

#include "keen/base/types.hpp"
#include "keen/base/memoryallocator.hpp"
#include "kaiko/gamelink/gamelink.hpp"
#include "library/modules/runtime/gamelink/sources/global/lan/gamelinklan.hpp"
#include "library/modules/runtime/gamelink/sources/steam/gamelinkinternet_steam.hpp"

namespace keen
{
    struct LocalPlayerIdStructureType;
    struct UserAccountSystem;
}

namespace unit4
{
    class LocalGameSession;
    struct RemoteUserId;
    struct SessionInteractionData;

    // size 0x24
    struct RemoteGameSessionCreationParameters
    {
        RemoteGameSessionCreationParameters(); // 0x00941A10 (.h:37)

        LocalGameSession* pLocalGameSession; // 0x00
        keen::UserAccountSystem* pUserAccountSystem; // 0x04
        kaiko::GameLinkLanCreationParameters* pGameLinkLanParameters; // 0x08
        kaiko::GameLinkInternetCreationParameters* pGameLinkInternetParameters; // 0x0C
        kaiko::GameLinkWirelessCreationParameters* pGameLinkWirelessParameters; // 0x10
        kaiko::GameLinkGeneralCreationParameters gameLinkGeneralParameters; // 0x14
    };

    // size 0x12000
    class RemoteGameSession
    {
    public:
        virtual ~RemoteGameSession(); // vtbl+0x00 0x00941A40 (.h:57)
        void create(keen::MemoryAllocator* pAllocator, const RemoteGameSessionCreationParameters& params); // 0x00C11CF0 (:18)
        void destroy(keen::MemoryAllocator* pAllocator); // 0x00C11130 (:88)
        void update(float timeStep); // 0x00C11DA0 (:122)
        bool isInteractionRequired(SessionInteractionData* pInteractionData); // 0x00C11E70 (:179)
        void setInteractionResponse(uint32 id); // 0x00C11170 (:285)
        void clearRequiredConfirmationInteraction(); // 0x00C111E0 (:324)
        bool hasVoiceChatPermissions(); // 0x00C111F0 (:330)
        bool doesSystemHaveNetworkAccess(); // 0x00C11210 (:338)
        bool setVoiceChatEnabled(bool isEnabled); // 0x00C11220 (:344)
        void setVoiceChatVolume(float volume); // 0x00C11290 (:367)
        void showRemoteUserUi(const RemoteUserId& userId); // 0x00C11F60 (:376)
        bool beginGameLinkSession(const kaiko::GameLinkSessionParameters& params); // 0x00C11FC0 (:388)
        void endGameLinkSession(); // 0x00C112C0 (:504)
        kaiko::gamelink::GameLink* getGameLinkSession(); // 0x00C11300 (:518)
        bool isInsideGameLinkSession(); // 0x00941A60 (.h:87)
        uint32 getLocalPlayerCount() const;
        bool isInCriticalErrorState(); // 0x00941A70 (.h:99)
        const keen::LocalPlayerIdStructureType* getMainUser(); // 0x00941A80 (.h:104)
        void initializeMainUser_HACK(); // 0x00C121B0 (:525)
        RemoteGameSession(); // 0x009706B0

        // 0x00 vfptr
    private:
        kaiko::GameLinkSessionParameters m_gameLinkSessionParams; // 0x08
        uint32 m_operationHandleCheckOnlineMultiplayerPermissions; // 0x38
        LocalGameSession* m_pLocalGameSession; // 0x3C
        bool m_lastInteractionWasLocalGameSession; // 0x40
        uint32 m_requiredConfirmationInteraction; // 0x44
        int m_platformErrorCode; // 0x48
        bool m_gameLinkLanCreated; // 0x4C
        kaiko::gamelink::GameLinkLan m_gameLinkLan; // 0x50
        bool m_gameLinkInternetCreated; // 0xB38
        kaiko::gamelink::GameLinkInternet m_gameLinkInternet; // 0xB40
        kaiko::gamelink::GameLink* m_pGameLinkInterface; // 0x11FF0
        bool m_isInCriticalErrorState; // 0x11FF4
        const keen::LocalPlayerIdStructureType* m_mainLocalPlayerId; // 0x11FF8
    };
}
