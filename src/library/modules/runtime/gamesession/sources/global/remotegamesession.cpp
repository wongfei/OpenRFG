// Original: library/modules/runtime/gamesession/sources/global/remotegamesession.cpp
//
// RemoteGameSession: owns both GameLink implementations by value (LAN and internet, each created on demand) and the
// one the current session runs on (m_pGameLinkInterface, chosen by the connection type in beginGameLinkSession). It
// turns the link's errors, a lost network and a link that dropped offline into a confirmation interaction for the
// game (rfg: multi_core_platform_internal_system_process shows it as a message box). The inline functions of
// remotegamesession.hpp are defined here too.

#include "unit4/gamesession/remotegamesession.hpp"
#include "unit4/gamesession/localgamesession.hpp"
#include "keen/base/trace.hpp"
#include "library/modules/runtime/systemservices/sources/steam/systemservices_steam.hpp"

#include <stdlib.h>
#include <string.h>

namespace unit4
{
    // 0x00941A10 (.h:37)
    RemoteGameSessionCreationParameters::RemoteGameSessionCreationParameters()
    {
        pLocalGameSession = nullptr;
        pGameLinkLanParameters = nullptr;
        pGameLinkInternetParameters = nullptr;
        pGameLinkWirelessParameters = nullptr;
    }

    // 0x009706B0 (the members' constructors)
    RemoteGameSession::RemoteGameSession()
    {
    }

    // 0x00941A40 (.h:57)
    RemoteGameSession::~RemoteGameSession()
    {
    }

    // 0x00C11CF0 (:18): at least one of the LAN and internet links must be requested, else the game exits
    void RemoteGameSession::create(keen::MemoryAllocator* pAllocator, const RemoteGameSessionCreationParameters& params)
    {
        m_mainLocalPlayerId = (const keen::LocalPlayerIdStructureType*)0xFF; // magic number: no local player
        if (params.pGameLinkLanParameters == nullptr && params.pGameLinkInternetParameters == nullptr)
        {
            exit(1);
        }
        m_pLocalGameSession = params.pLocalGameSession;
        m_lastInteractionWasLocalGameSession = false;
        m_requiredConfirmationInteraction = 0;
        m_platformErrorCode = 0;
        m_gameLinkLanCreated = false;
        if (params.pGameLinkLanParameters != nullptr)
        {
            m_gameLinkLan.create(pAllocator, *params.pGameLinkLanParameters, params.gameLinkGeneralParameters);
            m_gameLinkLanCreated = true;
        }
        m_gameLinkInternetCreated = false;
        if (params.pGameLinkInternetParameters != nullptr)
        {
            m_gameLinkInternet.create(pAllocator, *params.pGameLinkInternetParameters, params.gameLinkGeneralParameters);
            m_gameLinkInternetCreated = true;
        }
        m_pGameLinkInterface = nullptr;
        m_isInCriticalErrorState = false;
        memset(&m_gameLinkSessionParams, 0, sizeof(m_gameLinkSessionParams));
        m_operationHandleCheckOnlineMultiplayerPermissions = 1;
    }

    // 0x00C11130 (:88)
    void RemoteGameSession::destroy(keen::MemoryAllocator* pAllocator)
    {
        if (m_gameLinkLanCreated)
        {
            m_gameLinkLan.destroy(pAllocator);
            m_gameLinkLanCreated = false;
        }
        if (m_gameLinkInternetCreated)
        {
            m_gameLinkInternet.destroy(pAllocator);
            m_gameLinkInternetCreated = false;
        }
    }

    // 0x00C11DA0 (:122): both links update every frame (an idle one does nothing); the session's link losing the
    // network or going offline is a critical error
    void RemoteGameSession::update(float timeStep)
    {
        m_gameLinkLan.update(timeStep);
        m_gameLinkInternet.update(timeStep);
        if (m_pGameLinkInterface == nullptr)
        {
            return;
        }
        SystemServices* pServices = m_pLocalGameSession->getSystemServices();
        if ((m_pGameLinkInterface == &m_gameLinkLan || m_pGameLinkInterface == &m_gameLinkInternet) && !systemservices::doesSystemHaveNetworkAccess(pServices))
        {
            m_requiredConfirmationInteraction = kaiko::GameSessionInteractionId_NetworkConnectionLost;
            m_platformErrorCode = 0;
            m_isInCriticalErrorState = true;
            return;
        }
        if (m_pGameLinkInterface->getCurrentState() == kaiko::GameLinkState_Offline)
        {
            m_requiredConfirmationInteraction = kaiko::gamelink::GameLink::getInteractionId(m_pGameLinkInterface->getError(nullptr));
            m_platformErrorCode = 0;
            m_isInCriticalErrorState = true;
        }
    }

    // 0x00C11E70 (:179): the local session's interactions first (answered false here: the local session shows its
    // own), then the link's error, then a pending confirmation
    bool RemoteGameSession::isInteractionRequired(SessionInteractionData* pInteractionData)
    {
        if (m_pLocalGameSession->isInteractionRequired(pInteractionData))
        {
            m_lastInteractionWasLocalGameSession = true;
            return false;
        }
        m_lastInteractionWasLocalGameSession = false;
        if (m_pGameLinkInterface != nullptr)
        {
            int platformErrorCode = 0;
            const kaiko::GameLinkError error = m_pGameLinkInterface->getError(&platformErrorCode);
            if (error != kaiko::GameLinkError_None)
            {
                if (error == kaiko::GameLinkError_WaitForPermissionCheck)
                {
                    pInteractionData->id = kaiko::gamelink::GameLink::getInteractionId(kaiko::GameLinkError_WaitForPermissionCheck);
                    pInteractionData->responseOptionCount = 0;
                    pInteractionData->user.isValid = false;
                    pInteractionData->params.parameterCount = 0;
                    return true;
                }
                if (error == kaiko::GameLinkError_AskToEnableParty)
                {
                    pInteractionData->id = kaiko::gamelink::GameLink::getInteractionId(kaiko::GameLinkError_AskToEnableParty);
                    pInteractionData->responseOptionCount = 2;
                    pInteractionData->responseOptions[0] = kaiko::GameSessionResponseId_Ok;
                    pInteractionData->responseOptions[1] = kaiko::GameSessionResponseId_Cancel;
                    pInteractionData->user.isValid = false;
                    pInteractionData->params.parameterCount = 0;
                    return true;
                }
                m_requiredConfirmationInteraction = kaiko::gamelink::GameLink::getInteractionId(error);
                m_platformErrorCode = platformErrorCode;
            }
        }
        if (m_requiredConfirmationInteraction == 0)
        {
            return false;
        }
        pInteractionData->id = m_requiredConfirmationInteraction;
        pInteractionData->responseOptionCount = 1;
        pInteractionData->responseOptions[0] = kaiko::GameSessionResponseId_Ok;
        pInteractionData->user.isValid = false;
        if (m_platformErrorCode != 0)
        {
            pInteractionData->params.parameters[0].type = kaiko::GameSessionInteractionParameterType_PlatformErrorCode;
            pInteractionData->params.parameterCount = 1;
            pInteractionData->params.parameters[0].value.sint32Value = m_platformErrorCode;
            return true;
        }
        pInteractionData->params.parameterCount = 0;
        return true;
    }

    // 0x00C11170 (:285): the answer clears the link's error (Ok on "enable party" also enables it) and the confirmation
    void RemoteGameSession::setInteractionResponse(uint32 id)
    {
        if (m_lastInteractionWasLocalGameSession)
        {
            return;
        }
        kaiko::gamelink::GameLink* pLink = m_pGameLinkInterface;
        if (pLink != nullptr)
        {
            const kaiko::GameLinkError error = pLink->getError(nullptr);
            if (error == kaiko::GameLinkError_AskToEnableParty)
            {
                if (id == kaiko::GameSessionResponseId_Ok)
                {
                    pLink->setPartyEnabled(true);
                    pLink = m_pGameLinkInterface;
                }
                pLink->clearError();
            }
            else if (error != kaiko::GameLinkError_None && error != kaiko::GameLinkError_WaitForPermissionCheck)
            {
                pLink->clearError();
            }
        }
        if ((m_requiredConfirmationInteraction != 0 && id == kaiko::GameSessionResponseId_Ok) || m_requiredConfirmationInteraction == (uint32)kaiko::GameSessionInteractionId_ErrorViewerRequired)
        {
            m_requiredConfirmationInteraction = 0;
            m_platformErrorCode = 0;
        }
    }

    // 0x00C111E0 (:324)
    void RemoteGameSession::clearRequiredConfirmationInteraction()
    {
        m_requiredConfirmationInteraction = 0;
        m_platformErrorCode = 0;
    }

    // 0x00C111F0 (:330)
    bool RemoteGameSession::hasVoiceChatPermissions()
    {
        if (m_pGameLinkInterface == nullptr)
        {
            return false;
        }
        return m_pGameLinkInterface->hasVoiceChatPermissions();
    }

    // 0x00C11210 (:338)
    bool RemoteGameSession::doesSystemHaveNetworkAccess()
    {
        return systemservices::doesSystemHaveNetworkAccess(m_pLocalGameSession->getSystemServices());
    }

    // 0x00C11220 (:344): on the internet link voice chat needs the permission (else it is switched off)
    bool RemoteGameSession::setVoiceChatEnabled(bool isEnabled)
    {
        if (m_pGameLinkInterface == nullptr)
        {
            return false;
        }
        if (isEnabled && m_pGameLinkInterface == &m_gameLinkInternet && !m_gameLinkInternet.hasVoiceChatPermissions())
        {
            m_pGameLinkInterface->setVoiceChatEnabled(false);
            return false;
        }
        m_pGameLinkInterface->setVoiceChatEnabled(isEnabled);
        return true;
    }

    // 0x00C11290 (:367): the internet link's (the only one with voice chat)
    void RemoteGameSession::setVoiceChatVolume(float volume)
    {
        m_gameLinkInternet.setVoiceChatVolume(volume);
    }

    // 0x00C11F60 (:376)
    void RemoteGameSession::showRemoteUserUi(const RemoteUserId& userId)
    {
        const keen::LocalPlayerIdStructureType* pAskingUser = m_pLocalGameSession->getFirstSignedInUser();
        if (pAskingUser != (const keen::LocalPlayerIdStructureType*)0xFF)
        {
            NamedRemoteUserId userInfo;
            userInfo.id = userId; // the display name is left uninitialized (original behaviour)
            systemservices::showRemoteUserUi(m_pLocalGameSession->getSystemServices(), pAskingUser, userInfo);
        }
    }

    // 0x00C11FC0 (:388): Default (LAN) or Internet; the internet link needs a signed-in user (its account becomes the
    // session's main user when the parameters name none)
    bool RemoteGameSession::beginGameLinkSession(const kaiko::GameLinkSessionParameters& params)
    {
        kaiko::GameLinkSessionParameters sessionParams = params;
        const kaiko::GameLinkConnectionType connectionType = params.connectionType;
        if (connectionType == kaiko::GameLinkConnectionType_Default)
        {
            m_pGameLinkInterface = &m_gameLinkLan;
        }
        else if (connectionType == kaiko::GameLinkConnectionType_Internet)
        {
            if (!sessionParams.internet.mainUser.isValid)
            {
                const keen::LocalPlayerIdStructureType* pLocalPlayerId = m_pLocalGameSession->getFirstSignedInUser();
                keen::UserAccount profile;
                if (pLocalPlayerId == (const keen::LocalPlayerIdStructureType*)0xFF || !m_pLocalGameSession->getProfile(profile, pLocalPlayerId))
                {
                    m_platformErrorCode = 0;
                    m_requiredConfirmationInteraction = kaiko::GameSessionInteractionId_NotSignedIn;
                    return false;
                }
                memcpy(sessionParams.internet.mainUser.providerData, profile.id.providerData, sizeof(profile.id.providerData));
                sessionParams.internet.mainUser.isValid = profile.id.isValid;
                m_mainLocalPlayerId = pLocalPlayerId;
            }
            m_pGameLinkInterface = &m_gameLinkInternet;
        }
        if (m_pGameLinkInterface == nullptr)
        {
            keen::debug::nativePrintArgs("[RemoteGameSession] Calling beginGameLinkSession() with a connection type that is not supported on this platform.\n");
            return false;
        }
        // (the retail build walks the local users here without effect: a stripped check)
        if (connectionType == kaiko::GameLinkConnectionType_Internet || connectionType == kaiko::GameLinkConnectionType_Default)
        {
            if (!systemservices::doesSystemHaveNetworkAccess(m_pLocalGameSession->getSystemServices()))
            {
                m_platformErrorCode = 0;
                m_pGameLinkInterface = nullptr;
                m_requiredConfirmationInteraction = kaiko::GameSessionInteractionId_NoNetworkAccess;
                return false;
            }
        }
        if (!m_pGameLinkInterface->beginSession(sessionParams))
        {
            m_requiredConfirmationInteraction = kaiko::gamelink::GameLink::getInteractionId(m_pGameLinkInterface->getError(nullptr));
            m_platformErrorCode = 0;
            m_pGameLinkInterface = nullptr;
            return false;
        }
        m_isInCriticalErrorState = false;
        return true;
    }

    // 0x00C112C0 (:504): there must be a session (not checked)
    void RemoteGameSession::endGameLinkSession()
    {
        m_pGameLinkInterface->clearError();
        m_pGameLinkInterface->endSession();
        m_pGameLinkInterface = nullptr;
        m_isInCriticalErrorState = false;
        m_mainLocalPlayerId = (const keen::LocalPlayerIdStructureType*)0xFF; // magic number: no local player
    }

    // 0x00C11300 (:518)
    kaiko::gamelink::GameLink* RemoteGameSession::getGameLinkSession()
    {
        return m_pGameLinkInterface;
    }

    // 0x00941A60 (.h:87)
    bool RemoteGameSession::isInsideGameLinkSession()
    {
        return m_pGameLinkInterface != nullptr;
    }

    // 0x00941A70 (.h:99)
    bool RemoteGameSession::isInCriticalErrorState()
    {
        return m_isInCriticalErrorState;
    }

    // 0x00941A80 (.h:104)
    const keen::LocalPlayerIdStructureType* RemoteGameSession::getMainUser()
    {
        return m_mainLocalPlayerId;
    }

    // 0x00C121B0 (:525)
    void RemoteGameSession::initializeMainUser_HACK()
    {
        m_mainLocalPlayerId = m_pLocalGameSession->getFirstSignedInUser();
    }
}
