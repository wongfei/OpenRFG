// Original: library/modules/runtime/input/include/keen/input/xinput/controller_xinput.hpp
// Implementation: library/modules/runtime/input/sources/xinput/controller_xinput.cpp
//
// ControllerXb360, an XInput controller: state polling, generic pad conversion and rumble.
#pragma once

#include "keen/base/types.hpp"
#include <windows.h>
#include <xinput.h>
#include "keen/input/inputevents.hpp"

struct _XINPUT_STATE;

namespace keen
{
    namespace input
    {
        struct GenericPadInput;
    }
}

namespace keen
{
    // size 0xC
    class ControllerXb360
    {
    public:
        ControllerXb360(); // 0x00C75EC0 (:62)
        void fillInGenericPadInput(input::GenericPadInput* pInput, uint32 controllerId); // 0x00C7D4D0 (:69)
        bool isConnected() const;
        bool updateState(uint32 controllerId); // 0x00C7D9B0 (:123)
        void setRumble(uint32 controllerId, float mainActuatorValue, float subActuatorValue); // 0x00C79300 (:151)
        ControllerType getControllerType(); // 0x00C746A0 (.h:36)
        void setControllerType(ControllerType controllerType); // 0x00C746B0 (.h:37)
    private:
        bool updateStateInternal(_XINPUT_STATE* pState, uint32 controllerId); // 0x00C79290 (:129)
        void setRumbleInternal(uint32 controllerId); // 0x00C75ED0 (:159)

        bool m_isConnected; // 0x00
        bool m_retryVibration; // 0x01
        _XINPUT_VIBRATION m_vibrationState; // 0x02
        ControllerType m_controllerType; // 0x08
    };
}
