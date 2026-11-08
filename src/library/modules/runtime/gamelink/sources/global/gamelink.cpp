// Original: library/modules/runtime/gamelink/sources/global/gamelink.cpp
//
// The GameLink base: state, the sticky error (setError only raises it to a more severe one) and the error to
// interaction map. The inline functions of gamelink.hpp are defined here too (the creation and session parameter
// defaults, lobby and station comparisons).

#include "kaiko/gamelink/gamelink.hpp"
#include "keen/base/trace.hpp"

#include <string.h>

namespace kaiko
{
    // 0x00941810 (.h:101)
    GameLinkGeneralCreationParameters::GameLinkGeneralCreationParameters()
    {
        maxNumLobbyMembers = 8;
        maxNumPartyMembers = 4;
        maxNumMatchmakingResults = 10;
        findAndJoinTimeout = 10000; // ms
        pSoundSystem = nullptr;
        pSystemServices = nullptr;
    }

    // 0x00941830 (.h:117)
    GameLinkLanCreationParameters::GameLinkLanCreationParameters()
    {
        maxNumStoredUserDefinedPackets = 100;
        localUserName[0] = '\0';
    }

    // 0x00941840 (.h:136)
    GameLinkInternetCreationParameters::GameLinkInternetCreationParameters()
    {
        vacEnabled = true;
    }

    // 0x00970290 (.h:189)
    GameLinkSessionParameters::GameLinkSessionParameters()
    {
        memset(this, 0, sizeof(*this));
        connectionType = GameLinkConnectionType_Default;
    }

    // 0x00458C30
    GameLinkInvitationData::GameLinkInvitationData()
    {
    }

    // 0x00941850 (.h:217)
    bool GameLinkRemoteStation::is_equal(const GameLinkRemoteStation* pOther) const
    {
        if (pOther == nullptr)
        {
            return false;
        }
        return id.is_equal(pOther->id);
    }

    // 0x005D0D40 (.h:266): the whole id (address and padding), as memcmp
    bool GameLinkLobby::is_equal(const GameLinkLobby& other) const
    {
        return memcmp(&id, &other.id, sizeof(id)) == 0;
    }

    // 0x005D0DD0 (.h:296)
    bool GameLinkLobby::is_valid() const
    {
        return id.asUint64 != 0;
    }

    namespace gamelink
    {
        // 0x00CD0130 (.h:336)
        GameLink::GameLink()
        {
            m_state = GameLinkState_Offline;
            m_error = GameLinkError_None;
            m_platformErrorCode = 0;
            m_gameplayRunning = false;
        }

        // 0x00CD0150 (.h:338)
        GameLink::~GameLink()
        {
        }

        // 0x005D0DE0 (.h:661)
        GameLinkState GameLink::getCurrentState() const
        {
            return m_state;
        }

        // 0x00883570 (.h:670)
        GameLinkError GameLink::getError(int* pOutPlatformErrorCode)
        {
            if (pOutPlatformErrorCode != nullptr)
            {
                *pOutPlatformErrorCode = m_platformErrorCode;
            }
            return m_error;
        }

        // 0x00CD0160 (.h:681)
        void GameLink::clearError()
        {
            m_error = GameLinkError_None;
            m_platformErrorCode = 0;
        }

        // 0x00941870 (.h:697)
        void GameLink::setGameplayRunning(bool running)
        {
            m_gameplayRunning = running;
        }

        // 0x00CD0170 (.h:707): only a more severe error (a higher value) replaces the current one
        bool GameLink::setError(GameLinkError error, int platformErrorCode)
        {
            if (m_error < error)
            {
                m_error = error;
                m_platformErrorCode = platformErrorCode;
                keen::debug::nativePrintArgs("[gamelink] setError: %d (platformErrorCode: 0x%x)\n", error, platformErrorCode);
                return true;
            }
            return false;
        }

        // 0x00CD01A0 (.h:719): ids 1..0xFFFFFFFE, 0 is never handed out
        uint32 GameLink::generateRequestId()
        {
            static uint32 s_nextRequestId; // 0x03A260AC
            s_nextRequestId++;
            if (s_nextRequestId == 0xFFFFFFFFu)
            {
                s_nextRequestId = 1;
            }
            return s_nextRequestId;
        }

        // 0x00CD07F0 (:8): linear search of the header's map; GameSessionInteractionId_Invalid for an unmapped error
        GameSessionInteractionId GameLink::getInteractionId(GameLinkError error)
        {
            for (uint32 i = 0; i < sizeof(s_mapErrorInteractionId) / sizeof(s_mapErrorInteractionId[0]); i++)
            {
                if (s_mapErrorInteractionId[i].error == error)
                {
                    return s_mapErrorInteractionId[i].interactionId;
                }
            }
            return GameSessionInteractionId_Invalid;
        }

        // 0x00941880 (.h:757)
        void clearInvitationData()
        {
            setInvitationData(nullptr);
        }
    }
}
