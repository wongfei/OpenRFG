// Original: library/modules/runtime/input/sources/global/inputsystem_platform.hpp
//
// PlatformControllerState, the per-platform controller state the input system holds (XInput, DirectInput and Steam
// controllers, last generic states). Placement inferred.
#pragma once

#include "keen/base/types.hpp"
#include "keen/base/sizedarray.hpp"
#include "keen/input/inputsystem.hpp"
#include "keen/input/xinput/controller_xinput.hpp"
#include "library/modules/runtime/input/sources/steam/controllersteam.hpp"
#include "library/modules/runtime/input/sources/win32/controllerdinput_win32.hpp"

struct IDirectInput8A;

namespace keen
{
    // size 0x1188
    struct PlatformControllerState
    {
        PlatformControllerState(); // 0x00C76AF0
        ~PlatformControllerState(); // 0x00C76B90

        ControllerXb360 controllers[4]; // 0x00
        bool mouseVisible; // 0x30
        bool clipMouseCursorWhileInvisible; // 0x31
        bool mousePositionRelative; // 0x32
        bool deviceChangeReceived; // 0x33
        float2 lastMousePosition; // 0x34
        IDirectInput8A* pDirect8; // 0x3C
        ControllerDInputWin32 directInputControllers[8]; // 0x40
        uint32 directInputControllerCount; // 0x80
        input::GenericPadInput lastState[16]; // 0x88
        uint32 mouseWheelButtonFlags; // 0x1088
        ControllerSteam steamController[4]; // 0x1090
        uint32 steamControllerCount; // 0x1170
        SizedArray<DInputControllerConfig> directInputControllerConfigs; // 0x1174
        bool apiHidden[3]; // 0x1180
    };
}
