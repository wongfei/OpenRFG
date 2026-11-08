// Original: library/modules/runtime/input/include/keen/input/inputevents.hpp
// Implementation: library/modules/runtime/input/sources/global/inputevents.cpp
//
// Recovery scope: the event record and the enums it references. The original header is much larger
// (key codes and per-controller button tables); those are added when Tier 1 input work needs them.
#pragma once

#include "keen/base/localplayerid.hpp"
#include "keen/base/types.hpp"

namespace keen
{
    enum ControllerClass
    {
        ControllerClass_None = 0,
        ControllerClass_Keyboard = 1,
        ControllerClass_Mouse = 2,
        ControllerClass_Controller = 3,
        ControllerClass_Touch = 4,
        ControllerClass_Count = 5,
    };

    enum ControllerType
    {
        ControllerType_Unkown = 0,
        ControllerType_Keyboard = 1,
        ControllerType_Mouse = 2,
        ControllerType_PS3 = 3,
        ControllerType_DualShock4 = 4,
        ControllerType_Xbox360 = 5,
        ControllerType_3ds = 6,
        ControllerType_WiiUGamePad = 7,
        ControllerType_WiiMote = 8,
        ControllerType_OSX = 9,
        ControllerType_XBone = 10,
        ControllerType_PSVita = 11,
        ControllerType_Steam = 12,
        ControllerType_JoyConPair = 13,
        ControllerType_JoyConSingle = 14,
        ControllerType_SwitchProController = 15,
        ControllerType_Count = 16,
    };

    enum InputEventType
    {
        InputEventType_GameButtonDown = 0,
        InputEventType_GameButtonUp = 1,
        InputEventType_GameAxis = 2,
        InputEventType_GameAxis2d = 3,
        InputEventType_ControllerConnected = 4,
        InputEventType_ControllerDisconnected = 5,
        InputEventType_ControllerTypeChanged = 6,
        InputEventType_RawButtonDown = 7,
        InputEventType_RawButtonUp = 8,
        InputEventType_RawAxis = 9,
        InputEventType_Key = 10,
        InputEventType_MouseMove = 11,
        InputEventType_MouseRelativeMovement = 12,
        InputEventType_MouseWheel = 13,
        InputEventType_FocusLost = 14,
        InputEventType_FocusRestored = 15,
        InputEventType_TouchBegin = 16,
        InputEventType_TouchEnd = 17,
        InputEventType_TouchCancel = 18,
        InputEventType_TouchMove = 19,
        InputEventType_VirtualKeyboardOpened = 20,
        InputEventType_VirtualKeyboardText = 21,
        InputEventType_VirtualKeyboardClosed = 22,
        InputEventType_Flick = 23,
        InputEventType_Drag = 24,
        InputEventType_DragBegin = 25,
        InputEventType_DragEnd = 26,
        InputEventType_GyroAcceleration = 27,
        InputEventType_GyroAngularVelocity = 28,
        InputEventType_GyroOrientation = 29,
        InputEventType_Count = 30,
    };

    enum StandardButtonId
    {
        StandardButtonId_Unknown = 0,
        StandardButtonId_Left = 1,
        StandardButtonId_Right = 2,
        StandardButtonId_Up = 3,
        StandardButtonId_Down = 4,
        StandardButtonId_Confirm = 5,
        StandardButtonId_Exit = 6,
        StandardButtonId_StartPause = 7,
        StandardButtonId_Select = 8,
    };

    enum FlickDirection
    {
        FlickDirection_Left = 0,
        FlickDirection_Right = 1,
        FlickDirection_Up = 2,
        FlickDirection_Down = 3,
    };

    // size 0x8
    struct ConnectedEventData
    {
        ControllerType controllerType; // 0x00
        uint32 features; // 0x04
    };

    // size 0x8
    struct ButtonEventData
    {
        uint32 buttonIndex; // 0x00
        StandardButtonId standardButton; // 0x04
    };

    // size 0xC
    struct AxisEventData
    {
        uint32 axisIndex; // 0x00
        float value; // 0x04
        float previousValue; // 0x08
    };

    // size 0x4
    struct KeyEventData
    {
        uint32 keyCode; // 0x00
    };

    // size 0x8
    struct MouseEventData
    {
        float2 position; // 0x00
    };

    // size 0x4
    struct MouseWheelEventData
    {
        int wheelDelta; // 0x00
    };

    // size 0xC
    struct TouchEventData
    {
        uint32 touchId; // 0x00
        float2 position; // 0x04
    };

    // size 0x10
    struct FlickEventData
    {
        FlickDirection direction; // 0x00
        float speed; // 0x04
        float2 startPosition; // 0x08
    };

    // size 0x10
    struct DragEventData
    {
        float2 startPosition; // 0x00
        float2 deltaPosition; // 0x08
    };

    // size 0xC
    struct Axis2dEventData
    {
        uint32 axisIndex; // 0x00
        float x; // 0x04
        float y; // 0x08
    };

    // size 0x10
    union GyroEventData
    {
        float3 acceleration; // 0x00
        float3 angularVelocity; // 0x00
        float4 orientation; // 0x00
    };

    // size 0x10
    union InputEventData
    {
        ConnectedEventData connected; // 0x00
        ButtonEventData button; // 0x00
        AxisEventData axis; // 0x00
        KeyEventData key; // 0x00
        MouseEventData mouse; // 0x00
        MouseWheelEventData wheel; // 0x00
        TouchEventData touch; // 0x00
        FlickEventData flick; // 0x00
        DragEventData drag; // 0x00
        Axis2dEventData axis2d; // 0x00
        GyroEventData gyro; // 0x00
    };

    // One event from input::fetchNextInputEvent; dispatched to GameApplication::processInputEvent.
    // size 0x14
    struct InputEvent
    {
        void setLocalPlayerId(const LocalPlayerIdStructureType* id); // 0x00C74320
        const LocalPlayerIdStructureType* getLocalPlayerId() const;
        ControllerType getControllerType() const; // 0x004589E0

        uint8 controllerId; // 0x00
        uint8 controllerClass; // 0x01 ControllerClass (inferred)
        uint8 localPlayerId; // 0x02
        uint8 type; // 0x03 InputEventType (inferred)
        InputEventData data; // 0x04
    };
}
