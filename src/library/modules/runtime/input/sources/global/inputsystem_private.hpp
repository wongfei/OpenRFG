// Original: library/modules/runtime/input/sources/global/inputsystem_private.hpp
//
// InputSystem, the input system state (event queue, controllers, keyboard and mouse, platform state). Placement
// inferred.
#pragma once

#include "keen/base/types.hpp"
#include "keen/base/queue.hpp"
#include "keen/base/sizedarray.hpp"
#include "keen/input/controllermapping.hpp"
#include "keen/input/gesturehelper.hpp"
#include "keen/input/inputevents.hpp"
#include "keen/input/inputsystem.hpp"
#include "library/modules/runtime/input/sources/global/inputsystem_platform.hpp"

namespace keen
{
    struct LocalPlayerIdStructureType;

    // size 0x13E0
    struct InputSystem
    {
        InputSystem(); // 0x00C7B0F0
        ~InputSystem(); // 0x00C7DB40

        Queue<InputEvent> eventQueue; // 0x00
        PlatformControllerState controllerState; // 0x18
        SizedArray<InputEvent> storedEvents; // 0x11A0
        ControllerInfo controllerInfos[16]; // 0x11AC
        InputSystemControllerAutoCatchType autoCatchType; // 0x132C
        const LocalPlayerIdStructureType* autoCatchPlayerId; // 0x1330
        MappedAxisButtonState mappedAxisButtonStates[7][16]; // 0x1334
        GestureHelper gestureHelper; // 0x13A4
    };

    namespace input
    {
        // The event queue and the stored events (inputsystem.cpp), shared with the platform part and the gestures.
        void storeEvent(InputSystem* pInputSystem, const InputEvent& inputEvent); // 0x00C76A40 (inputsystem.cpp:833)
        void removeEvent(InputSystem* pInputSystem, const InputEvent& inputEvent); // 0x00C7AEF0 (inputsystem.cpp:806)
        void addInputEvent(InputSystem* pInputSystem, const InputEvent& inputEvent); // 0x00C7ACF0 (inputsystem.cpp:858)
        void sendResetEvents(InputSystem* pInputSystem, uint32 controllerId); // 0x00C7AFC0 (inputsystem.cpp:879)
    }
}
