// Original: library/modules/thirdparty/emgiec/emgiec_steam_142/sdk/public/steam/isteamcontroller.h
//
// Steamworks SDK 1.42, third party (stub_impl.md S2): ISteamController (SteamController006), the Steam controller
// actions keen::ControllerSteam reads, from the PDB's vtable. The action data structs are packed to 1 as in the SDK
// (ControllerAnalogActionData_t is 0xD bytes in the PDB).
#pragma once

#include "library/modules/thirdparty/emgiec/emgiec_steam_142/sdk/public/steam/steamclientpublic.h"
#include "library/modules/thirdparty/emgiec/emgiec_steam_142/sdk/public/steam/steamtypes.h"

// Magic number: 16, STEAM_CONTROLLER_MAX_COUNT (the handle array GetConnectedControllers fills).
#define STEAM_CONTROLLER_MAX_COUNT 16
// Magic number: 8, STEAM_CONTROLLER_MAX_ORIGINS (the origin array of Get*ActionOrigins).
#define STEAM_CONTROLLER_MAX_ORIGINS 8

enum ESteamControllerPad : int;
enum ESteamInputType : int;

enum EControllerSourceMode
{
    k_EControllerSourceMode_None = 0,
    k_EControllerSourceMode_Dpad = 1,
    k_EControllerSourceMode_Buttons = 2,
    k_EControllerSourceMode_FourButtons = 3,
    k_EControllerSourceMode_AbsoluteMouse = 4,
    k_EControllerSourceMode_RelativeMouse = 5,
    k_EControllerSourceMode_JoystickMove = 6,
    k_EControllerSourceMode_JoystickMouse = 7,
    k_EControllerSourceMode_JoystickCamera = 8,
    k_EControllerSourceMode_ScrollWheel = 9,
    k_EControllerSourceMode_Trigger = 10,
    k_EControllerSourceMode_TouchMenu = 11,
    k_EControllerSourceMode_MouseJoystick = 12,
    k_EControllerSourceMode_MouseRegion = 13,
    k_EControllerSourceMode_RadialMenu = 14,
    k_EControllerSourceMode_SingleButton = 15,
    k_EControllerSourceMode_Switches = 16,
};

// The origins (k_EControllerActionOrigin_None = 0 ... k_EControllerActionOrigin_Count = 196 in 1.42); the port passes
// them through as values (keen::SteamControllerActionOrigin), so only the bounds are named.
enum EControllerActionOrigin
{
    k_EControllerActionOrigin_None = 0,
    k_EControllerActionOrigin_Count = 196,
};

#pragma pack(push, 1)

// size 0xD
struct ControllerAnalogActionData_t
{
    EControllerSourceMode eMode; // 0x00
    float x; // 0x04
    float y; // 0x08
    bool bActive; // 0x0C
};

// size 0x2
struct ControllerDigitalActionData_t
{
    bool bState; // 0x00
    bool bActive; // 0x01
};

// size 0x28
struct ControllerMotionData_t
{
    float rotQuatX; // 0x00
    float rotQuatY; // 0x04
    float rotQuatZ; // 0x08
    float rotQuatW; // 0x0C
    float posAccelX; // 0x10
    float posAccelY; // 0x14
    float posAccelZ; // 0x18
    float rotVelX; // 0x1C
    float rotVelY; // 0x20
    float rotVelZ; // 0x24
};

#pragma pack(pop)

// size 0x4
class ISteamController
{
public:
    virtual bool Init() = 0; // vtbl+0x00
    virtual bool Shutdown() = 0; // vtbl+0x04
    virtual void RunFrame() = 0; // vtbl+0x08
    virtual int GetConnectedControllers(ControllerHandle_t* handlesOut) = 0; // vtbl+0x0C
    virtual bool ShowBindingPanel(ControllerHandle_t controllerHandle) = 0; // vtbl+0x10
    virtual ControllerActionSetHandle_t GetActionSetHandle(const char* pszActionSetName) = 0; // vtbl+0x14
    virtual void ActivateActionSet(ControllerHandle_t controllerHandle, ControllerActionSetHandle_t actionSetHandle) = 0; // vtbl+0x18
    virtual ControllerActionSetHandle_t GetCurrentActionSet(ControllerHandle_t controllerHandle) = 0; // vtbl+0x1C
    virtual void ActivateActionSetLayer(ControllerHandle_t controllerHandle, ControllerActionSetHandle_t actionSetLayerHandle) = 0; // vtbl+0x20
    virtual void DeactivateActionSetLayer(ControllerHandle_t controllerHandle, ControllerActionSetHandle_t actionSetLayerHandle) = 0; // vtbl+0x24
    virtual void DeactivateAllActionSetLayers(ControllerHandle_t controllerHandle) = 0; // vtbl+0x28
    virtual int GetActiveActionSetLayers(ControllerHandle_t controllerHandle, ControllerActionSetHandle_t* handlesOut) = 0; // vtbl+0x2C
    virtual ControllerDigitalActionHandle_t GetDigitalActionHandle(const char* pszActionName) = 0; // vtbl+0x30
    virtual ControllerDigitalActionData_t GetDigitalActionData(ControllerHandle_t controllerHandle, ControllerDigitalActionHandle_t digitalActionHandle) = 0; // vtbl+0x34
    virtual int GetDigitalActionOrigins(ControllerHandle_t controllerHandle, ControllerActionSetHandle_t actionSetHandle, ControllerDigitalActionHandle_t digitalActionHandle, EControllerActionOrigin* originsOut) = 0; // vtbl+0x38
    virtual ControllerAnalogActionHandle_t GetAnalogActionHandle(const char* pszActionName) = 0; // vtbl+0x3C
    virtual ControllerAnalogActionData_t GetAnalogActionData(ControllerHandle_t controllerHandle, ControllerAnalogActionHandle_t analogActionHandle) = 0; // vtbl+0x40
    virtual int GetAnalogActionOrigins(ControllerHandle_t controllerHandle, ControllerActionSetHandle_t actionSetHandle, ControllerAnalogActionHandle_t analogActionHandle, EControllerActionOrigin* originsOut) = 0; // vtbl+0x44
    virtual void StopAnalogActionMomentum(ControllerHandle_t controllerHandle, ControllerAnalogActionHandle_t eAction) = 0; // vtbl+0x48
    virtual void TriggerHapticPulse(ControllerHandle_t controllerHandle, ESteamControllerPad eTargetPad, uint16 usDurationMicroSec) = 0; // vtbl+0x4C
    virtual void TriggerRepeatedHapticPulse(ControllerHandle_t controllerHandle, ESteamControllerPad eTargetPad, uint16 usDurationMicroSec, uint16 usOffMicroSec, uint16 unRepeat, uint32 nFlags) = 0; // vtbl+0x50
    virtual void TriggerVibration(ControllerHandle_t controllerHandle, uint16 usLeftSpeed, uint16 usRightSpeed) = 0; // vtbl+0x54
    virtual void SetLEDColor(ControllerHandle_t controllerHandle, uint8 nColorR, uint8 nColorG, uint8 nColorB, uint32 nFlags) = 0; // vtbl+0x58
    virtual int GetGamepadIndexForController(ControllerHandle_t ulControllerHandle) = 0; // vtbl+0x5C
    virtual ControllerHandle_t GetControllerForGamepadIndex(int nIndex) = 0; // vtbl+0x60
    virtual ControllerMotionData_t GetMotionData(ControllerHandle_t controllerHandle) = 0; // vtbl+0x64
    virtual bool ShowDigitalActionOrigins(ControllerHandle_t controllerHandle, ControllerDigitalActionHandle_t digitalActionHandle, float flScale, float flXPosition, float flYPosition) = 0; // vtbl+0x68
    virtual bool ShowAnalogActionOrigins(ControllerHandle_t controllerHandle, ControllerAnalogActionHandle_t analogActionHandle, float flScale, float flXPosition, float flYPosition) = 0; // vtbl+0x6C
    virtual const char* GetStringForActionOrigin(EControllerActionOrigin eOrigin) = 0; // vtbl+0x70
    virtual const char* GetGlyphForActionOrigin(EControllerActionOrigin eOrigin) = 0; // vtbl+0x74
    virtual ESteamInputType GetInputTypeForHandle(ControllerHandle_t controllerHandle) = 0; // vtbl+0x78

    // 0x00 vfptr
};
