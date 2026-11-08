// Original: library/modules/runtime/gamesessioninteractions/sources/global/gamesessioninteractions.cpp
//
// The localization tags of an interaction and of a response (multiplayer.md MP2), the module an interaction comes from
// and the parameters filled into a description (stub_impl.md S2).

#include "kaiko/gamesessioninteractions/gamesessioninteractions.hpp"
#include "keen/base/string.hpp"

#include <string.h>

namespace kaiko
{
    // 0x00CBA720 (:9): the id's block of 0x1000
    GameSessionInteractionOrigin getGameSessionInteractionOrigin(GameSessionInteractionId interactionId)
    {
        // Magic numbers: the interaction id blocks (0x1000 save data, 0x2000 system services, 0x3000 input,
        // 0x4000 user accounts, 0x5000 game link).
        switch ((uint32)interactionId & 0xFFFFF000u)
        {
        case 0x1000u:
            return GameSessionInteractionOrigin_SaveDataHandler;
        case 0x2000u:
            return GameSessionInteractionOrigin_SystemServices;
        case 0x3000u:
            return GameSessionInteractionOrigin_InputSystem;
        case 0x4000u:
            return GameSessionInteractionOrigin_UserAccountSystem;
        case 0x5000u:
            return GameSessionInteractionOrigin_GameLink;
        default:
            return GameSessionInteractionOrigin_Generic;
        }
    }

    // 0x00CBA7D0 (:42): the description is a format string with one parameter per interaction parameter (an error
    // code as "KAIKO@<hex>", a user name as is)
    void fillVariableGameSessionInteractionParams(GameSessionInteractionFormattedDescriptionText& descToFormat, const GameSessionInteractionParameterList& parameters)
    {
        keen::StringParameter stringParameters[4];
        // Magic number: room for "KAIKO@xxxxxxxx".
        char errorCodeTexts[4][32];
        for (uint32 i = 0; i < parameters.parameterCount; ++i)
        {
            const GameSessionInteractionParameter& parameter = parameters.parameters[i];
            if (parameter.type == GameSessionInteractionParameterType_PlatformErrorCode)
            {
                keen::formatString(errorCodeTexts[i], sizeof(errorCodeTexts[i]), "KAIKO@%08x", parameter.value.sint32Value);
                stringParameters[i] = errorCodeTexts[i];
            }
            else if (parameter.type == GameSessionInteractionParameterType_UserNameString)
            {
                stringParameters[i] = parameter.value.stringValue;
            }
        }
        char format[sizeof(descToFormat.text)];
        memcpy(format, descToFormat.text, sizeof(format));
        keen::formatString(descToFormat.text, sizeof(descToFormat.text), format, stringParameters, parameters.parameterCount);
    }

    // 0x00CBA770
    void generateGameSessionInteractionLocaTag(GameSessionInteractionLocaTag& locaTag, const GameSessionInteractionId interactionId)
    {
        memset(&locaTag, 0, sizeof(locaTag));
        keen::formatString(locaTag.name, sizeof(locaTag.name), "gamesession_interaction_desc_0x%08X", (uint32)interactionId);
    }

    // 0x00CBA7A0
    void generateGameSessionResponseLocaTag(GameSessionInteractionLocaTag& locaTag, const GameSessionResponseId interactionId)
    {
        memset(&locaTag, 0, sizeof(locaTag));
        keen::formatString(locaTag.name, sizeof(locaTag.name), "gamesession_interaction_response_0x%08X", (uint32)interactionId);
    }
}
