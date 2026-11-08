// Original: library/modules/runtime/gamesessioninteractions/include/kaiko/gamesessioninteractions/gamesessioninteractions.hpp
// Implementation: library/modules/runtime/gamesessioninteractions/sources/global/gamesessioninteractions.cpp
//
// Ids, parameters and localization tags for platform interactions (save errors, sign-in changes,
// controller loss, network errors) that the game must present to the user.
#pragma once

#include "keen/base/types.hpp"

namespace kaiko
{
    enum GameSessionInteractionId
    {
        GameSessionInteractionId_SelectionCanceled = 4097,
        GameSessionInteractionId_SaveGameCorrupt = 4098,
        GameSessionInteractionId_EraseFailed = 4099,
        GameSessionInteractionId_NotEnoughSpace_RetryDisable = 4100,
        GameSessionInteractionId_NotEnoughSpace_EraseDisable = 4101,
        GameSessionInteractionId_CreateFailed = 4102,
        GameSessionInteractionId_ReadError = 4103,
        GameSessionInteractionId_ReadError_Erase = 4104,
        GameSessionInteractionId_UserBindingError = 4105,
        GameSessionInteractionId_WriteError = 4106,
        GameSessionInteractionId_Overwrite = 4107,
        GameSessionInteractionId_ConfirmDisable = 4108,
        GameSessionInteractionId_ConfirmErase = 4109,
        GameSessionInteractionId_InformDeviceChanged = 4110,
        GameSessionInteractionId_WaitingForProcess = 4111,
        GameSessionInteractionId_NotOnline = 8192,
        GameSessionInteractionId_ConnectionFailed = 8193,
        GameSessionInteractionId_RankingSendFailed_NotOnline = 8194,
        GameSessionInteractionId_RankingSendFailed_ConnectionFailed = 8195,
        GameSessionInteractionId_RankingSendFailed_ForbiddenByParentalControl = 8196,
        GameSessionInteractionId_GotDisconnected = 8197,
        GameSessionInteractionId_ForbiddenByParentalControl = 8196,
        GameSessionInteractionId_xxx = 8197,
        GameSessionInteractionId_FailedRetryOrDisable = 8198,
        GameSessionInteractionId_ShowProfileRestricted = 8199,
        GameSessionInteractionId_ShowProfileError = 8200,
        GameSessionInteractionId_GenericControllerDisconnected = 12289,
        GameSessionInteractionId_WiiUProControllerDisconnected = 12290,
        GameSessionInteractionId_DisplayingSignInUi = 16385,
        GameSessionInteractionId_SignInSkipped = 16386,
        GameSessionInteractionId_SignedOut = 16387,
        GameSessionInteractionId_SwitchedAccount = 16388,
        GameSessionInteractionId_CreatePartyCancelled = 20480,
        GameSessionInteractionId_KickedFromLobby = 20481,
        GameSessionInteractionId_JoinLobbyFailed_Unknown = 20482,
        GameSessionInteractionId_JoinLobbyFailed_Timeout = 20483,
        GameSessionInteractionId_JoinLobbyFailed_Locked = 20484,
        GameSessionInteractionId_JoinLobbyFailed_NotAllowedToJoin = 20485,
        GameSessionInteractionId_JoinLobbyFailed_WrongPassword = 20486,
        GameSessionInteractionId_JoinLobbyFailed_Full = 20487,
        GameSessionInteractionId_JoinLobbyFailed_NotFound = 20488,
        GameSessionInteractionId_CreateLobbyFailed_Unknown = 20489,
        GameSessionInteractionId_CreateLobbyFailed_Timeout = 20490,
        GameSessionInteractionId_General_Timeout = 20491,
        GameSessionInteractionId_HostMigrationFailed = 20492,
        GameSessionInteractionId_SessionLost = 20493,
        GameSessionInteractionId_ServerNotAvailable = 20494,
        GameSessionInteractionId_NotSignedIn = 20495,
        GameSessionInteractionId_NoNetworkAccess = 20496,
        GameSessionInteractionId_NoInternetAccess = 20497,
        GameSessionInteractionId_Unknown = 20498,
        GameSessionInteractionId_ServiceUnavailable = 20499,
        GameSessionInteractionId_UserBlocked = 20501,
        GameSessionInteractionId_NoMultiplayerUnknown = 20502,
        GameSessionInteractionId_NoMultiplayerSubscriptionRequired = 20503,
        GameSessionInteractionId_NoMultiplayerBanned = 20504,
        GameSessionInteractionId_NoMultiplayerAgeRestricted = 20505,
        GameSessionInteractionId_NoVoiceChatUnknown = 20506,
        GameSessionInteractionId_NoVoiceChatSubscriptionRequired = 20507,
        GameSessionInteractionId_NoVoiceChatBanned = 20508,
        GameSessionInteractionId_NoVoiceChatAgeRestricted = 20509,
        GameSessionInteractionId_NetworkConnectionLost = 20510,
        GameSessionInteractionId_InternetConnectionLost = 20511,
        GameSessionInteractionId_AskToEnableParty = 20512,
        GameSessionInteractionId_NoMultiplayerNotSignedIn = 20513,
        GameSessionInteractionId_NoMultiplayerSystemupdateRequired = 20514,
        GameSessionInteractionId_NoMultiplayerGameUpdateRequired = 20515,
        GameSessionInteractionId_WaitForPermissionCheck = 20516,
        GameSessionInteractionId_NoMultiplayerRestricted = 20517,
        GameSessionInteractionId_SignedOutMultiplayer = 20518,
        GameSessionInteractionId_ErrorViewerRequired = 24576,
        GameSessionInteractionId_None = 0,
        GameSessionInteractionId_Invalid = -1,
    };

    enum GameSessionResponseId
    {
        GameSessionResponseId_Ok = 1,
        GameSessionResponseId_Cancel = 2,
        GameSessionResponseId_Exit = 3,
        GameSessionResponseId_RetryFunction = 4099,
        GameSessionResponseId_Disable = 4100,
        GameSessionResponseId_Erase = 4101,
        GameSessionResponseId_DeviceSelection = 4102,
        GameSessionResponseId_Overwrite = 4103,
        GameSessionResponseId_ContinueWithoutConnection = 8193,
        GameSessionResponseId_RetryConnecting = 8194,
        GameSessionResponseId_RetrySignIn = 16385,
        GameSessionResponseId_SkipSignIn = 16386,
        GameSessionResponseId_RepeatSignIn = 16387,
        GameSessionResponseId_None = -1,
    };

    enum GameSessionInteractionOrigin
    {
        GameSessionInteractionOrigin_Generic = 0,
        GameSessionInteractionOrigin_SaveDataHandler = 1,
        GameSessionInteractionOrigin_SystemServices = 2,
        GameSessionInteractionOrigin_InputSystem = 3,
        GameSessionInteractionOrigin_UserAccountSystem = 4,
        GameSessionInteractionOrigin_GameLink = 5,
    };

    enum GameSessionInteractionParameterType
    {
        GameSessionInteractionParameterType_PlatformErrorCode = 0,
        GameSessionInteractionParameterType_UserNameString = 1,
    };

    // size 0x44
    struct GameSessionInteractionParameter
    {
        // size 0x40
        union GameSessionInteractionParameterValue
        {
            int sint32Value; // 0x00
            char stringValue[64]; // 0x00
        };

        GameSessionInteractionParameterType type; // 0x00
        GameSessionInteractionParameterValue value; // 0x04
    };

    // size 0x114
    struct GameSessionInteractionParameterList
    {
        GameSessionInteractionParameter parameters[4]; // 0x00
        uint32 parameterCount; // 0x110
    };

    // size 0x400
    struct GameSessionInteractionFormattedDescriptionText
    {
        char text[1024]; // 0x00
    };

    // size 0x30
    struct GameSessionInteractionLocaTag
    {
        char name[48]; // 0x00
    };

    GameSessionInteractionOrigin getGameSessionInteractionOrigin(GameSessionInteractionId interactionId); // 0x00CBA720
    void generateGameSessionInteractionLocaTag(GameSessionInteractionLocaTag& locaTag, const GameSessionInteractionId interactionId); // 0x00CBA770
    void generateGameSessionResponseLocaTag(GameSessionInteractionLocaTag& locaTag, const GameSessionResponseId interactionId); // 0x00CBA7A0
    void fillVariableGameSessionInteractionParams(GameSessionInteractionFormattedDescriptionText& descToFormat, const GameSessionInteractionParameterList& parameters); // 0x00CBA7D0
}
