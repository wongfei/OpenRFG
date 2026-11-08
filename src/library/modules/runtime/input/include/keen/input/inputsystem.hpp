// Original: library/modules/runtime/input/include/keen/input/inputsystem.hpp
// Implementation: library/modules/runtime/input/sources/global/inputsystem.cpp
//
// The input system API: creation parameters, controller enumeration and information, Steam controller actions and axis
// modes, rumble, keyboard/mouse state and the event queue (see inputevents.hpp). InputSystem itself is defined in
// sources/global/inputsystem_private.hpp. input::GenericPadInput is the platform independent pad state (buttons in an
// input::ButtonBitfield of 128 bits; placement inferred). Placement of SteamControllerActionOrigin (also used by
// Volition controls_bake_steam_origins_string) inferred.
#pragma once

#include "keen/base/types.hpp"
#include "keen/input/inputevents.hpp"

namespace keen
{
    struct InputSystem;
    struct LocalPlayerIdStructureType;
    class MemoryAllocator;
    struct PlatformControllerState;
    struct SteamControllerAction;
    class SteamworksSystem;
    struct float2;

    enum SteamControllerAxisMode
    {
        SteamControllerAxisMode_XAndY = 0,
        SteamControllerAxisMode_XOnly = 1,
        SteamControllerAxisMode_YOnly = 2,
    };

    enum WindowsControllerApi
    {
        WindowsControllerApi_XInput = 0,
        WindowsControllerApi_DInput = 1,
        WindowsControllerApi_SteamController = 2,
        WindowsControllerApi_Count = 3,
    };

    enum SteamControllerActionType
    {
        SteamControllerActionType_Set = 0,
        SteamControllerActionType_Button = 1,
        SteamControllerActionType_Axis = 2,
    };

    enum InputSystemControllerAutoCatchType
    {
        InputSystemControllerAutoCatchType_AnyButton = 0,
        InputSystemControllerAutoCatchType_Anything = 1,
        InputSystemControllerAutoCatchType_StartButtonOnly = 2,
        InputSystemControllerAutoCatchType_Disabled = 3,
        InputSystemControllerAutoCatchType_AnythingIncludingControllerConnected = 4,
        InputSystemControllerAutoCatchType_ControllerConnected = 5,
    };

    enum SteamControllerActionOrigin
    {
        SteamControllerActionOrigin_None = 0,
        SteamControllerActionOrigin_A = 1,
        SteamControllerActionOrigin_B = 2,
        SteamControllerActionOrigin_X = 3,
        SteamControllerActionOrigin_Y = 4,
        SteamControllerActionOrigin_LeftBumper = 5,
        SteamControllerActionOrigin_RightBumper = 6,
        SteamControllerActionOrigin_LeftGrip = 7,
        SteamControllerActionOrigin_RightGrip = 8,
        SteamControllerActionOrigin_Start = 9,
        SteamControllerActionOrigin_Back = 10,
        SteamControllerActionOrigin_LeftPad_Touch = 11,
        SteamControllerActionOrigin_LeftPad_Swipe = 12,
        SteamControllerActionOrigin_LeftPad_Click = 13,
        SteamControllerActionOrigin_LeftPad_DPadNorth = 14,
        SteamControllerActionOrigin_LeftPad_DPadSouth = 15,
        SteamControllerActionOrigin_LeftPad_DPadWest = 16,
        SteamControllerActionOrigin_LeftPad_DPadEast = 17,
        SteamControllerActionOrigin_RightPad_Touch = 18,
        SteamControllerActionOrigin_RightPad_Swipe = 19,
        SteamControllerActionOrigin_RightPad_Click = 20,
        SteamControllerActionOrigin_RightPad_DPadNorth = 21,
        SteamControllerActionOrigin_RightPad_DPadSouth = 22,
        SteamControllerActionOrigin_RightPad_DPadWest = 23,
        SteamControllerActionOrigin_RightPad_DPadEast = 24,
        SteamControllerActionOrigin_LeftTrigger_Pull = 25,
        SteamControllerActionOrigin_LeftTrigger_Click = 26,
        SteamControllerActionOrigin_RightTrigger_Pull = 27,
        SteamControllerActionOrigin_RightTrigger_Click = 28,
        SteamControllerActionOrigin_LeftStick_Move = 29,
        SteamControllerActionOrigin_LeftStick_Click = 30,
        SteamControllerActionOrigin_LeftStick_DPadNorth = 31,
        SteamControllerActionOrigin_LeftStick_DPadSouth = 32,
        SteamControllerActionOrigin_LeftStick_DPadWest = 33,
        SteamControllerActionOrigin_LeftStick_DPadEast = 34,
        SteamControllerActionOrigin_Gyro_Move = 35,
        SteamControllerActionOrigin_Gyro_Pitch = 36,
        SteamControllerActionOrigin_Gyro_Yaw = 37,
        SteamControllerActionOrigin_Gyro_Roll = 38,
        SteamControllerActionOrigin_PS4_X = 39,
        SteamControllerActionOrigin_PS4_Circle = 40,
        SteamControllerActionOrigin_PS4_Triangle = 41,
        SteamControllerActionOrigin_PS4_Square = 42,
        SteamControllerActionOrigin_PS4_LeftBumper = 43,
        SteamControllerActionOrigin_PS4_RightBumper = 44,
        SteamControllerActionOrigin_PS4_Options = 45,
        SteamControllerActionOrigin_PS4_Share = 46,
        SteamControllerActionOrigin_PS4_LeftPad_Touch = 47,
        SteamControllerActionOrigin_PS4_LeftPad_Swipe = 48,
        SteamControllerActionOrigin_PS4_LeftPad_Click = 49,
        SteamControllerActionOrigin_PS4_LeftPad_DPadNorth = 50,
        SteamControllerActionOrigin_PS4_LeftPad_DPadSouth = 51,
        SteamControllerActionOrigin_PS4_LeftPad_DPadWest = 52,
        SteamControllerActionOrigin_PS4_LeftPad_DPadEast = 53,
        SteamControllerActionOrigin_PS4_RightPad_Touch = 54,
        SteamControllerActionOrigin_PS4_RightPad_Swipe = 55,
        SteamControllerActionOrigin_PS4_RightPad_Click = 56,
        SteamControllerActionOrigin_PS4_RightPad_DPadNorth = 57,
        SteamControllerActionOrigin_PS4_RightPad_DPadSouth = 58,
        SteamControllerActionOrigin_PS4_RightPad_DPadWest = 59,
        SteamControllerActionOrigin_PS4_RightPad_DPadEast = 60,
        SteamControllerActionOrigin_PS4_CenterPad_Touch = 61,
        SteamControllerActionOrigin_PS4_CenterPad_Swipe = 62,
        SteamControllerActionOrigin_PS4_CenterPad_Click = 63,
        SteamControllerActionOrigin_PS4_CenterPad_DPadNorth = 64,
        SteamControllerActionOrigin_PS4_CenterPad_DPadSouth = 65,
        SteamControllerActionOrigin_PS4_CenterPad_DPadWest = 66,
        SteamControllerActionOrigin_PS4_CenterPad_DPadEast = 67,
        SteamControllerActionOrigin_PS4_LeftTrigger_Pull = 68,
        SteamControllerActionOrigin_PS4_LeftTrigger_Click = 69,
        SteamControllerActionOrigin_PS4_RightTrigger_Pull = 70,
        SteamControllerActionOrigin_PS4_RightTrigger_Click = 71,
        SteamControllerActionOrigin_PS4_LeftStick_Move = 72,
        SteamControllerActionOrigin_PS4_LeftStick_Click = 73,
        SteamControllerActionOrigin_PS4_LeftStick_DPadNorth = 74,
        SteamControllerActionOrigin_PS4_LeftStick_DPadSouth = 75,
        SteamControllerActionOrigin_PS4_LeftStick_DPadWest = 76,
        SteamControllerActionOrigin_PS4_LeftStick_DPadEast = 77,
        SteamControllerActionOrigin_PS4_RightStick_Move = 78,
        SteamControllerActionOrigin_PS4_RightStick_Click = 79,
        SteamControllerActionOrigin_PS4_RightStick_DPadNorth = 80,
        SteamControllerActionOrigin_PS4_RightStick_DPadSouth = 81,
        SteamControllerActionOrigin_PS4_RightStick_DPadWest = 82,
        SteamControllerActionOrigin_PS4_RightStick_DPadEast = 83,
        SteamControllerActionOrigin_PS4_DPad_North = 84,
        SteamControllerActionOrigin_PS4_DPad_South = 85,
        SteamControllerActionOrigin_PS4_DPad_West = 86,
        SteamControllerActionOrigin_PS4_DPad_East = 87,
        SteamControllerActionOrigin_PS4_Gyro_Move = 88,
        SteamControllerActionOrigin_PS4_Gyro_Pitch = 89,
        SteamControllerActionOrigin_PS4_Gyro_Yaw = 90,
        SteamControllerActionOrigin_PS4_Gyro_Roll = 91,
        SteamControllerActionOrigin_XBoxOne_A = 92,
        SteamControllerActionOrigin_XBoxOne_B = 93,
        SteamControllerActionOrigin_XBoxOne_X = 94,
        SteamControllerActionOrigin_XBoxOne_Y = 95,
        SteamControllerActionOrigin_XBoxOne_LeftBumper = 96,
        SteamControllerActionOrigin_XBoxOne_RightBumper = 97,
        SteamControllerActionOrigin_XBoxOne_Menu = 98,
        SteamControllerActionOrigin_XBoxOne_View = 99,
        SteamControllerActionOrigin_XBoxOne_LeftTrigger_Pull = 100,
        SteamControllerActionOrigin_XBoxOne_LeftTrigger_Click = 101,
        SteamControllerActionOrigin_XBoxOne_RightTrigger_Pull = 102,
        SteamControllerActionOrigin_XBoxOne_RightTrigger_Click = 103,
        SteamControllerActionOrigin_XBoxOne_LeftStick_Move = 104,
        SteamControllerActionOrigin_XBoxOne_LeftStick_Click = 105,
        SteamControllerActionOrigin_XBoxOne_LeftStick_DPadNorth = 106,
        SteamControllerActionOrigin_XBoxOne_LeftStick_DPadSouth = 107,
        SteamControllerActionOrigin_XBoxOne_LeftStick_DPadWest = 108,
        SteamControllerActionOrigin_XBoxOne_LeftStick_DPadEast = 109,
        SteamControllerActionOrigin_XBoxOne_RightStick_Move = 110,
        SteamControllerActionOrigin_XBoxOne_RightStick_Click = 111,
        SteamControllerActionOrigin_XBoxOne_RightStick_DPadNorth = 112,
        SteamControllerActionOrigin_XBoxOne_RightStick_DPadSouth = 113,
        SteamControllerActionOrigin_XBoxOne_RightStick_DPadWest = 114,
        SteamControllerActionOrigin_XBoxOne_RightStick_DPadEast = 115,
        SteamControllerActionOrigin_XBoxOne_DPad_North = 116,
        SteamControllerActionOrigin_XBoxOne_DPad_South = 117,
        SteamControllerActionOrigin_XBoxOne_DPad_West = 118,
        SteamControllerActionOrigin_XBoxOne_DPad_East = 119,
        SteamControllerActionOrigin_XBox360_A = 120,
        SteamControllerActionOrigin_XBox360_B = 121,
        SteamControllerActionOrigin_XBox360_X = 122,
        SteamControllerActionOrigin_XBox360_Y = 123,
        SteamControllerActionOrigin_XBox360_LeftBumper = 124,
        SteamControllerActionOrigin_XBox360_RightBumper = 125,
        SteamControllerActionOrigin_XBox360_Start = 126,
        SteamControllerActionOrigin_XBox360_Back = 127,
        SteamControllerActionOrigin_XBox360_LeftTrigger_Pull = 128,
        SteamControllerActionOrigin_XBox360_LeftTrigger_Click = 129,
        SteamControllerActionOrigin_XBox360_RightTrigger_Pull = 130,
        SteamControllerActionOrigin_XBox360_RightTrigger_Click = 131,
        SteamControllerActionOrigin_XBox360_LeftStick_Move = 132,
        SteamControllerActionOrigin_XBox360_LeftStick_Click = 133,
        SteamControllerActionOrigin_XBox360_LeftStick_DPadNorth = 134,
        SteamControllerActionOrigin_XBox360_LeftStick_DPadSouth = 135,
        SteamControllerActionOrigin_XBox360_LeftStick_DPadWest = 136,
        SteamControllerActionOrigin_XBox360_LeftStick_DPadEast = 137,
        SteamControllerActionOrigin_XBox360_RightStick_Move = 138,
        SteamControllerActionOrigin_XBox360_RightStick_Click = 139,
        SteamControllerActionOrigin_XBox360_RightStick_DPadNorth = 140,
        SteamControllerActionOrigin_XBox360_RightStick_DPadSouth = 141,
        SteamControllerActionOrigin_XBox360_RightStick_DPadWest = 142,
        SteamControllerActionOrigin_XBox360_RightStick_DPadEast = 143,
        SteamControllerActionOrigin_XBox360_DPad_North = 144,
        SteamControllerActionOrigin_XBox360_DPad_South = 145,
        SteamControllerActionOrigin_XBox360_DPad_West = 146,
        SteamControllerActionOrigin_XBox360_DPad_East = 147,
        SteamControllerActionOrigin_SteamV2_A = 148,
        SteamControllerActionOrigin_SteamV2_B = 149,
        SteamControllerActionOrigin_SteamV2_X = 150,
        SteamControllerActionOrigin_SteamV2_Y = 151,
        SteamControllerActionOrigin_SteamV2_LeftBumper = 152,
        SteamControllerActionOrigin_SteamV2_RightBumper = 153,
        SteamControllerActionOrigin_SteamV2_LeftGrip = 154,
        SteamControllerActionOrigin_SteamV2_RightGrip = 155,
        SteamControllerActionOrigin_SteamV2_LeftGrip_Upper = 156,
        SteamControllerActionOrigin_SteamV2_RightGrip_Upper = 157,
        SteamControllerActionOrigin_SteamV2_LeftBumper_Pressure = 158,
        SteamControllerActionOrigin_SteamV2_RightBumper_Pressure = 159,
        SteamControllerActionOrigin_SteamV2_LeftGrip_Pressure = 160,
        SteamControllerActionOrigin_SteamV2_RightGrip_Pressure = 161,
        SteamControllerActionOrigin_SteamV2_LeftGrip_Upper_Pressure = 162,
        SteamControllerActionOrigin_SteamV2_RightGrip_Upper_Pressure = 163,
        SteamControllerActionOrigin_SteamV2_Start = 164,
        SteamControllerActionOrigin_SteamV2_Back = 165,
        SteamControllerActionOrigin_SteamV2_LeftPad_Touch = 166,
        SteamControllerActionOrigin_SteamV2_LeftPad_Swipe = 167,
        SteamControllerActionOrigin_SteamV2_LeftPad_Click = 168,
        SteamControllerActionOrigin_SteamV2_LeftPad_Pressure = 169,
        SteamControllerActionOrigin_SteamV2_LeftPad_DPadNorth = 170,
        SteamControllerActionOrigin_SteamV2_LeftPad_DPadSouth = 171,
        SteamControllerActionOrigin_SteamV2_LeftPad_DPadWest = 172,
        SteamControllerActionOrigin_SteamV2_LeftPad_DPadEast = 173,
        SteamControllerActionOrigin_SteamV2_RightPad_Touch = 174,
        SteamControllerActionOrigin_SteamV2_RightPad_Swipe = 175,
        SteamControllerActionOrigin_SteamV2_RightPad_Click = 176,
        SteamControllerActionOrigin_SteamV2_RightPad_Pressure = 177,
        SteamControllerActionOrigin_SteamV2_RightPad_DPadNorth = 178,
        SteamControllerActionOrigin_SteamV2_RightPad_DPadSouth = 179,
        SteamControllerActionOrigin_SteamV2_RightPad_DPadWest = 180,
        SteamControllerActionOrigin_SteamV2_RightPad_DPadEast = 181,
        SteamControllerActionOrigin_SteamV2_LeftTrigger_Pull = 182,
        SteamControllerActionOrigin_SteamV2_LeftTrigger_Click = 183,
        SteamControllerActionOrigin_SteamV2_RightTrigger_Pull = 184,
        SteamControllerActionOrigin_SteamV2_RightTrigger_Click = 185,
        SteamControllerActionOrigin_SteamV2_LeftStick_Move = 186,
        SteamControllerActionOrigin_SteamV2_LeftStick_Click = 187,
        SteamControllerActionOrigin_SteamV2_LeftStick_DPadNorth = 188,
        SteamControllerActionOrigin_SteamV2_LeftStick_DPadSouth = 189,
        SteamControllerActionOrigin_SteamV2_LeftStick_DPadWest = 190,
        SteamControllerActionOrigin_SteamV2_LeftStick_DPadEast = 191,
        SteamControllerActionOrigin_SteamV2_Gyro_Move = 192,
        SteamControllerActionOrigin_SteamV2_Gyro_Pitch = 193,
        SteamControllerActionOrigin_SteamV2_Gyro_Yaw = 194,
        SteamControllerActionOrigin_SteamV2_Gyro_Roll = 195,
        SteamControllerActionOrigin_Count = 196,
        SteamControllerActionOrigin_SwitchProCon_A = 196,
        SteamControllerActionOrigin_SwitchProCon_B = 197,
        SteamControllerActionOrigin_SwitchProCon_X = 198,
        SteamControllerActionOrigin_SwitchProCon_Y = 199,
        SteamControllerActionOrigin_SwitchProCon_L = 200,
        SteamControllerActionOrigin_SwitchProCon_R = 201,
        SteamControllerActionOrigin_SwitchProCon_Plus = 202,
        SteamControllerActionOrigin_SwitchProCon_Minus = 203,
        SteamControllerActionOrigin_SwitchProCon_Capture = 204,
        SteamControllerActionOrigin_SwitchProCon_ZL_Pull = 205,
        SteamControllerActionOrigin_SwitchProCon_ZL_Click = 206,
        SteamControllerActionOrigin_SwitchProCon_ZR_Pull = 207,
        SteamControllerActionOrigin_SwitchProCon_ZR_Click = 208,
        SteamControllerActionOrigin_SwitchProCon_LeftStick_Move = 209,
        SteamControllerActionOrigin_SwitchProCon_LeftStick_Click = 210,
        SteamControllerActionOrigin_SwitchProCon_LeftStick_DPadNorth = 211,
        SteamControllerActionOrigin_SwitchProCon_LeftStick_DPadSouth = 212,
        SteamControllerActionOrigin_SwitchProCon_LeftStick_DPadWest = 213,
        SteamControllerActionOrigin_SwitchProCon_LeftStick_DPadRight = 214,
        SteamControllerActionOrigin_SwitchProCon_RightStick_Move = 215,
        SteamControllerActionOrigin_SwitchProCon_RichtStick_Click = 216,
        SteamControllerActionOrigin_SwitchProCon_RightStick_DPadNorth = 217,
        SteamControllerActionOrigin_SwitchProCon_RightStick_DPadSouth = 218,
        SteamControllerActionOrigin_SwitchProCon_RightStick_DPadWest = 219,
        SteamControllerActionOrigin_SwitchProCon_RightStick_DPadEast = 220,
        SteamControllerActionOrigin_SwitchProCon_DPad_North = 221,
        SteamControllerActionOrigin_SwitchProCon_DPad_South = 222,
        SteamControllerActionOrigin_SwitchProCon_DPad_West = 223,
        SteamControllerActionOrigin_SwitchProCon_DPad_East = 224,
    };

    // size 0x18
    struct ControllerInfo
    {
        ControllerClass controllerClass; // 0x00
        ControllerType type; // 0x04
        uint32 features; // 0x08
        uint32 buttonCount; // 0x0C
        uint32 axisCount; // 0x10
        uint8 localPlayerId; // 0x14
    };

    namespace input
    {
        // size 0x10
        struct ButtonBitfield
        {
            void clear();
            void setBit(uint64 bit); // 0x00C74530 (inputsystem_private.hpp:42)
            bool getBitSet(uint64 bit) const; // 0x00C745C0 (inputsystem_private.hpp:48)

            static const uint32 MaxNumButtons; // 0x013427F0
            uint64 low; // 0x00
            uint64 high; // 0x08
        };

        // size 0x100
        struct GenericPadInput
        {
            void clear(); // 0x00C76A20 (:48)

            ButtonBitfield buttons; // 0x00
            float axis[48]; // 0x10
            float3 acceleration; // 0xD0
            float3 angularVelocity; // 0xDC
            float4 orientation; // 0xE8
            bool isConnected; // 0xF8
        };

    }

    // size 0x1C
    struct InputSystemCreationParameters
    {
        enum
        {
            DefaultMaxEventCount = 64,
        };

        InputSystemCreationParameters(); // 0x00C0DFE0 (.h:51)

        uint32 maxEventCount; // 0x00
        void* pWindowHandle; // 0x04
        void* pSecondaryWindowHandle; // 0x08
        SteamworksSystem* pSteamWorksSystem; // 0x0C
        const SteamControllerAction* pControllerActions; // 0x10
        uint32 controllerActionCount; // 0x14
        bool enableGyroSupport; // 0x18
    };

    // size 0x10
    struct SteamControllerAction
    {
        SteamControllerActionType type; // 0x00
        const char* pName; // 0x04
        uint32 buttonIndex; // 0x08
        SteamControllerAxisMode axisMode; // 0x0C
    };

    namespace input
    {
        void initializeGestureHelper(InputSystem* pInputSystem, uint32 controllerId, MemoryAllocator* pAllocator); // 0x00C7AC30 (:87)
        void shutdownGestureHelper(InputSystem* pInputSystem, MemoryAllocator* pAllocator); // 0x00C7AC90 (:95)
        void generateGenericPadEvents(InputSystem* pInputSystem, GenericPadInput* pLastState, const GenericPadInput& currentState, uint32 controllerId); // 0x00C7E0B0 (:103)
        InputSystem* createInputSystem(MemoryAllocator* pAllocator, const InputSystemCreationParameters& parameters); // 0x00C7F140 (:464)
        void destroyInputSystem(InputSystem* pInputSystem, MemoryAllocator* pAllocator); // 0x00C7F270 (:525)
        void setControllerPlayerId(InputSystem* pInputSystem, uint32 controllerId, const LocalPlayerIdStructureType* playerId); // 0x00C7B190 (:554)
        void setControllerPlayerId(InputSystem* pInputSystem, const InputEvent& event, const LocalPlayerIdStructureType* playerId); // 0x00C7B1E0 (:573)
        void clearControllerPlayerId(InputSystem* pInputSystem, const LocalPlayerIdStructureType* playerId); // 0x00C74700 (:580)
        void setControllerAutoCatch(InputSystem* pInputSystem, const LocalPlayerIdStructureType* playerId, InputSystemControllerAutoCatchType type); // 0x00C74720 (:591)
        InputSystemControllerAutoCatchType getControllerAutoCatch(InputSystem* pInputSystem, const LocalPlayerIdStructureType* playerId); // 0x00C74790 (:617)
        void addRawButtonDownEvent(InputSystem* pInputSystem, uint32 controllerId, uint32 buttonIndex); // 0x00C7B210 (:627)
        void addRawButtonUpEvent(InputSystem* pInputSystem, uint32 controllerId, uint32 buttonIndex); // 0x00C7B260 (:639)
        void addRawAxisEvent(InputSystem* pInputSystem, uint32 controllerId, uint32 axisIndex, float axisValue, float previousAxisValue); // 0x00C7B2B0 (:652)
        void addFocusLostEvent(InputSystem* pInputSystem, bool hasFocus); // 0x00C7B390 (:674)
        void addKeyEvent(InputSystem* pInputSystem, uint32 controllerId, uint32 keyCode); // 0x00C7B3D0 (:685)
        void addMouseMoveEvent(InputSystem* pInputSystem, uint32 controllerId, float2 position); // 0x00C7B400 (:695)
        void addMouseRelativeMovementEvent(InputSystem* pInputSystem, uint32 controllerId, float2 relativeMovement); // 0x00C7B430 (:705)
        void addMouseWheelEvent(InputSystem* pInputSystem, uint32 controllerId, int wheelData); // 0x00C7B460 (:715)
        void addGyroAccelerationEvent(InputSystem* pInputSystem, uint32 controllerId, float3 acceleration); // 0x00C7B490 (:725)
        void addGyroAngularVelocityEvent(InputSystem* pInputSystem, uint32 controllerId, float3 angularVelocity); // 0x00C7B4E0 (:736)
        void addGyroOrientationEvent(InputSystem* pInputSystem, uint32 controllerId, float4 orientation); // 0x00C7B530 (:747)
        void addControllerConnectedEvent(InputSystem* pInputSystem, uint32 controllerId); // 0x00C7B580 (:758)
        void addControllerDisconnectedEvent(InputSystem* pInputSystem, uint32 controllerId); // 0x00C7B610 (:780)
        void addControllerTypeChangedEvent(InputSystem* pInputSystem, uint32 controllerId); // 0x00C7B670 (:795)
        void addTouchEvent(InputSystem* pInputSystem, InputEventType type, uint32 controllerId, uint32 touchId, float2 position); // 0x00C7B6B0 (:806)
        void addFlickEvent(InputSystem* pInputSystem, uint32 controllerId, FlickDirection direction, float speed, float2 startPosition); // 0x00C7B6F0 (:816)
        void addDragEvent(InputSystem* pInputSystem, uint32 controllerId, float2 startPosition, float2 deltaPosition); // 0x00C7B740 (:827)
        void addDragBeginEvent(InputSystem* pInputSystem, uint32 controllerId, float2 position); // 0x00C7B780 (:837)
        void addDragEndEvent(InputSystem* pInputSystem, uint32 controllerId, float2 dragStartPosition); // 0x00C7B7B0 (:846)
        bool fetchNextInputEvent(InputEvent* pEvent, InputSystem* pInputSystem); // 0x00C7B7E0 (:935)
        uint32 getInputEventCount(const InputSystem* pInputSystem); // 0x00C76BA0 (:951)
        const InputEvent& getInputEvent(const InputSystem* pInputSystem, uint32 eventIndex); // 0x00C7B840 (:958)
        void updateInputSystem(InputSystem* pInputSystem, float timeStep); // 0x00C7F220 (:965)
        bool isControllerConnected(const InputSystem* pInputSystem, uint32 controllerId); // 0x00C747B0 (:1002)
        bool canControllerRumble(InputSystem* pInputSystem, uint32 controllerId); // 0x00C747E0 (:1013)
        void cancelTouch(InputSystem* pInputSystem, uint32 touchId); // 0x00C74830 (:1036)
        StandardButtonId getStandardButtonIdFromButtonIndex(const InputSystem* pInputSystem, ControllerType controllerType, uint32 buttonIndex); // 0x00C76BB0 (:1058)
        void resetGyroOrientationOrigin(InputSystem* pInputSystem, uint32 controllerId); // 0x00C7B860 (:1097)
        void getControllerInfo(ControllerInfo* pResult, uint32 controllerId); // 0x00C76C50 (:1102)
        void getControllerInfo(ControllerInfo* pResult, const InputEvent& inputEvent); // 0x00C76C90 (:1117)
        uint32 getControllerIdForPlayer(const LocalPlayerIdStructureType* playerId, ControllerClass controllerClass); // 0x00C74840 (:1124)
        const LocalPlayerIdStructureType* getPlayerIdForController(uint32 controllerId); // 0x00C74870 (:1137)
        void setRumble(InputSystem* pInputSystem, uint32 controllerId, float mainActuatorValue, float subActuatorValue); // 0x00C7E5D0 (:1163)
        void setRumble(InputSystem* pInputSystem, uint32 controllerId, const float* pActuatorValues, uint32 actuatorCount); // 0x00C7E650 (:1174)
        bool containsInputEvent(InputSystem* pInputSystem, InputEventType eventType, uint32 controllerId, uint32 buttonIndex); // 0x00C7B8D0 (:1179)
        void replaceInputEvent(InputSystem* pInputSystem, InputEventType eventType, uint32 controllerId, uint32 buttonIndex, uint32 newButtonIndex); // 0x00C7B930 (:1194)
        PlatformControllerState* getPlatformControllerState(InputSystem* pInputSystem); // 0x00C748A0 (:1210)
        ControllerClass getPrimaryInputDeviceClass(InputSystem* pInputSystem, const LocalPlayerIdStructureType* playerId); // 0x00C748B0 (:1217)
        ControllerType getPrimaryInputDeviceType(InputSystem* pInputSystem, const LocalPlayerIdStructureType* playerId, ControllerClass controllerClass); // 0x00C74980 (:1240)
    }

    uint8 readNibble(const char* pName); // 0x00C75190 (inputsystem_win32.cpp:613)
    ControllerType getControllerTypeByName(const char* pName); // 0x00C751D0 (inputsystem_win32.cpp:631)

    namespace input
    {
        void createPlatformControllerState(PlatformControllerState* pState, InputSystem* pInputSystem, MemoryAllocator* pAllocator, const InputSystemCreationParameters& parameters); // 0x00C7E690 (inputsystem_win32.cpp:1343)
        void destroyPlatformControllerState(PlatformControllerState* pState, InputSystem* pInputSystem, MemoryAllocator* pAllocator); // 0x00C75300 (inputsystem_win32.cpp:1384)
        void resetSystemIdleState(); // 0x00C75310 (inputsystem_win32.cpp:1392)
        void setSteamControllerActionSet(InputSystem* pInputSystem, uint32 setIndex); // 0x00C78EC0 (inputsystem_win32.cpp:1397)
        uint32 getSteamControllerActionSet(InputSystem* pInputSystem); // 0x00C78F30 (inputsystem_win32.cpp:1410)
        void activateSteamControllerActionSetLayer(InputSystem* pInputSystem, uint32 setIndex); // 0x00C78F50 (inputsystem_win32.cpp:1426)
        void deactivateSteamControllerActionSetLayer(InputSystem* pInputSystem, uint32 setIndex); // 0x00C78FB0 (inputsystem_win32.cpp:1439)
        void clearSteamControllerActionSetLayers(InputSystem* pInputSystem); // 0x00C79010 (inputsystem_win32.cpp:1452)
        void getSteamControllerActionOrigins(const InputSystem* pInputSystem, const SteamControllerAction& action, SteamControllerActionOrigin* pOriginsOut, uint32 bufferLength, uint32* pCountOut); // 0x00C7D3D0 (inputsystem_win32.cpp:1464)
        void getSteamControllerActionOrigins(const InputSystem* pInputSystem, const SteamControllerAction& action, uint32 setIndex, SteamControllerActionOrigin* pOriginsOut, uint32 bufferLength, uint32* pCountOut); // 0x00C7D410 (inputsystem_win32.cpp:1487)
        bool showSteamControllerBindingPanel(const InputSystem* pInputSystem); // 0x00C79060 (inputsystem_win32.cpp:1511)
        const char* getSteamControllerActionOriginText(SteamControllerActionOrigin origin); // 0x00C79090 (inputsystem_win32.cpp:1527)
        void createPlatformEvents(InputSystem* pInputSystem, PlatformControllerState* pControllerState); // 0x00C7E830 (inputsystem_win32.cpp:1538)
        void setMouseCursorVisible(InputSystem* pInputSystem, bool visible, bool clipMouseCursorWhileInvisible); // 0x00C75320 (inputsystem_win32.cpp:1670)
        bool isMouseCursorVisible(InputSystem* pInputSystem); // 0x00C75420 (inputsystem_win32.cpp:1714)
        void setMouseRelativeInput(InputSystem* pInputSystem, bool enabled); // 0x00C790B0 (inputsystem_win32.cpp:1722)
        void setPlatformControllerRumble(PlatformControllerState* pControllerState, uint32 controllerIndex, const float* pActuatorValues, uint32 actuatorCount); // 0x00C7D450 (inputsystem_win32.cpp:1756)
        void updatePlatformRumble(PlatformControllerState* pControllerState, float timeStep); // 0x00C75530 (inputsystem_win32.cpp:1776)
        StandardButtonId getPlatformStandardButtonIdFromButtonIndex(const InputSystem*, ControllerType controllerType, uint32 buttonIndex); // 0x00C75540 (inputsystem_win32.cpp:1783)
        void fillPlatformControllerInfo(ControllerInfo* pInfo, PlatformControllerState* pControllerState, uint32 controllerId); // 0x00C756D0 (inputsystem_win32.cpp:1879)
        bool hasFocus(const InputSystem* pInputSystem); // 0x00C757E0 (inputsystem_win32.cpp:1943)
        bool getButtonCaption(char* pBuffer, uint32 capacity, ControllerType controllerType, uint32 buttonIndex); // 0x00C75810 (inputsystem_win32.cpp:1958)
        void convertKeyEventToText(char* pBuffer, uint32 capacity, uint32 keyEvent, bool holdingShift, bool holdingAltGr, bool numLockActivated); // 0x00C75A10 (inputsystem_win32.cpp:2066)
        void resetPlatformGyroOrientationOrigin(InputSystem* pInputSystem, PlatformControllerState* pControllerState, uint32 controllerId); // 0x00C790D0 (inputsystem_win32.cpp:2148)
        void setControllerApiHidden(InputSystem* pInputSystem, WindowsControllerApi api, bool hidden); // 0x00C75B60 (inputsystem_win32.cpp:2163)
        bool getControllerApiHidden(InputSystem* pInputSystem, WindowsControllerApi api); // 0x00C75B80 (inputsystem_win32.cpp:2168)
    }
}
