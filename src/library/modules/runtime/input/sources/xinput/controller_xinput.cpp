// Original: library/modules/runtime/input/sources/xinput/controller_xinput.cpp
//
// An XInput pad (controller ids 2-5): its connection state, a rumble that is retried while the pad is busy, and the
// generic pad state (buttons, sticks with their dead zones, triggers).
//
// orig_main.md OM2: what the input system's update reaches; stub_impl.md S2: the rumble (input::setRumble).

#include "keen/input/xinput/controller_xinput.hpp"
#include "keen/input/inputsystem.hpp"

#include <stdlib.h>
#include <string.h>

namespace keen
{
    // |value| as the original computes it: value - (-value) >= 0 picks value.
    static float padAbs(float value)
    {
        const float negated = -value;
        return (float)((double)value - (double)negated) >= 0.0f ? value : negated;
    }

    // A stick axis from -32768..32767 to -1..1 without the dead zone (rescaled to start at 0 past it). The arithmetic
    // is in double on the float constants, rounded to float.
    static float getStickValue(short raw, float deadZone, float scale)
    {
        const float value = (float)((double)(float)raw * (double)(1.0f / 32767.0f));
        if (padAbs(value) < deadZone)
        {
            return 0.0f;
        }
        const float sign = value >= 0.0f ? 1.0f : -1.0f;
        return (float)(((double)value - (double)sign * (double)deadZone) * (double)scale);
    }

    // A trigger from 0..255 to 0..1 without its threshold.
    static float getTriggerValue(uint8 raw)
    {
        const float value = (float)((double)(float)raw * (double)(1.0f / 255.0f));
        if (padAbs(value) < 30.0f / 255.0f)
        {
            return 0.0f;
        }
        return (float)(((double)value - (double)(30.0f / 255.0f)) / (double)(225.0f / 255.0f));
    }

    // 0x00C75EC0 (:62)
    ControllerXb360::ControllerXb360()
    {
        m_isConnected = false;
        m_retryVibration = false;
        m_controllerType = ControllerType_Xbox360;
    }

    // 0x00C7D4D0 (:69)
    // XInput's buttons to the generic ones: d-pad left/right/up/down 0-3, A 4, B 5, Y 6, X 7, shoulders 8 and 10,
    // triggers (as buttons) 9 and 11, thumbs 12 and 13, start 14, back 15; the axes: left stick 0/1, right stick 2/3,
    // triggers 4/5.
    // Magic numbers: XInput's dead zones (7849 and 8689 of 32767, the scales 1 / (1 - dead zone)), the trigger
    // threshold 30 of 255, and a trigger counts as a button above 141.
    // Original behaviour kept: an XInputSetState error other than "busy" or "not connected" exits the process.
    void ControllerXb360::fillInGenericPadInput(input::GenericPadInput* pInput, uint32 controllerId)
    {
        XINPUT_STATE state;
        if (XInputGetState(controllerId, &state) != ERROR_SUCCESS)
        {
            m_isConnected = false;
            pInput->isConnected = false;
            return;
        }
        m_isConnected = true;
        if (m_retryVibration)
        {
            const DWORD result = XInputSetState(controllerId, &m_vibrationState);
            if (result == ERROR_SUCCESS)
            {
                m_retryVibration = false;
            }
            else if (result == ERROR_BUSY)
            {
                m_retryVibration = true;
            }
            else if (result != ERROR_DEVICE_NOT_CONNECTED)
            {
                exit(1);
            }
        }
        memset(pInput, 0, sizeof(input::GenericPadInput));
        pInput->isConnected = true;
        const WORD buttons = state.Gamepad.wButtons;
        if ((buttons & XINPUT_GAMEPAD_DPAD_RIGHT) != 0)
        {
            pInput->buttons.setBit(1);
        }
        if ((buttons & XINPUT_GAMEPAD_DPAD_LEFT) != 0)
        {
            pInput->buttons.setBit(0);
        }
        if ((buttons & XINPUT_GAMEPAD_DPAD_UP) != 0)
        {
            pInput->buttons.setBit(2);
        }
        if ((buttons & XINPUT_GAMEPAD_DPAD_DOWN) != 0)
        {
            pInput->buttons.setBit(3);
        }
        if ((buttons & XINPUT_GAMEPAD_A) != 0)
        {
            pInput->buttons.setBit(4);
        }
        if ((buttons & XINPUT_GAMEPAD_B) != 0)
        {
            pInput->buttons.setBit(5);
        }
        if ((buttons & XINPUT_GAMEPAD_X) != 0)
        {
            pInput->buttons.setBit(7);
        }
        if ((buttons & XINPUT_GAMEPAD_Y) != 0)
        {
            pInput->buttons.setBit(6);
        }
        if ((buttons & XINPUT_GAMEPAD_LEFT_SHOULDER) != 0)
        {
            pInput->buttons.setBit(8);
        }
        if ((buttons & XINPUT_GAMEPAD_RIGHT_SHOULDER) != 0)
        {
            pInput->buttons.setBit(10);
        }
        if ((buttons & XINPUT_GAMEPAD_LEFT_THUMB) != 0)
        {
            pInput->buttons.setBit(12);
        }
        if ((buttons & XINPUT_GAMEPAD_RIGHT_THUMB) != 0)
        {
            pInput->buttons.setBit(13);
        }
        if ((buttons & XINPUT_GAMEPAD_START) != 0)
        {
            pInput->buttons.setBit(14);
        }
        if ((buttons & XINPUT_GAMEPAD_BACK) != 0)
        {
            pInput->buttons.setBit(15);
        }
        if (state.Gamepad.bLeftTrigger > 0x8Du)
        {
            pInput->buttons.low |= 0x200u;
        }
        if (state.Gamepad.bRightTrigger > 0x8Du)
        {
            pInput->buttons.low |= 0x800u;
        }
        pInput->axis[0] = getStickValue(state.Gamepad.sThumbLX, 7849.0f / 32767.0f, 1.0f / (1.0f - 7849.0f / 32767.0f));
        pInput->axis[1] = getStickValue(state.Gamepad.sThumbLY, 7849.0f / 32767.0f, 1.0f / (1.0f - 7849.0f / 32767.0f));
        pInput->axis[2] = getStickValue(state.Gamepad.sThumbRX, 8689.0f / 32767.0f, 1.0f / (1.0f - 8689.0f / 32767.0f));
        pInput->axis[3] = getStickValue(state.Gamepad.sThumbRY, 8689.0f / 32767.0f, 1.0f / (1.0f - 8689.0f / 32767.0f));
        pInput->axis[4] = getTriggerValue(state.Gamepad.bLeftTrigger);
        pInput->axis[5] = getTriggerValue(state.Gamepad.bRightTrigger);
    }

    // 0x00C7D9B0 (:123)
    // The connection state, and the pending rumble retried (updateStateInternal inlined).
    bool ControllerXb360::updateState(uint32 controllerId)
    {
        XINPUT_STATE state;
        if (XInputGetState(controllerId, &state) != ERROR_SUCCESS)
        {
            m_isConnected = false;
            return false;
        }
        m_isConnected = true;
        if (m_retryVibration)
        {
            const DWORD result = XInputSetState(controllerId, &m_vibrationState);
            if (result == ERROR_SUCCESS)
            {
                m_retryVibration = false;
            }
            else if (result == ERROR_BUSY)
            {
                m_retryVibration = true;
            }
            else if (result != ERROR_DEVICE_NOT_CONNECTED)
            {
                exit(1);
            }
        }
        return true;
    }

    // 0x00C79300 (:151): the motor speeds (0..1 to 0..65535) sent at once, or retried by updateState while the pad
    // is busy. Original behaviour kept: another XInput error ends the process.
    void ControllerXb360::setRumble(uint32 controllerId, float mainActuatorValue, float subActuatorValue)
    {
        // Magic number: 65535, XInput's full motor speed.
        m_vibrationState.wLeftMotorSpeed = (WORD)(int)((double)mainActuatorValue * 65535.0);
        m_vibrationState.wRightMotorSpeed = (WORD)(int)((double)subActuatorValue * 65535.0);
        setRumbleInternal(controllerId);
    }

    // 0x00C75ED0 (:159)
    void ControllerXb360::setRumbleInternal(uint32 controllerId)
    {
        if (!m_isConnected)
        {
            return;
        }
        const DWORD result = XInputSetState(controllerId, &m_vibrationState);
        if (result == ERROR_SUCCESS)
        {
            m_retryVibration = false;
        }
        else if (result == ERROR_BUSY)
        {
            m_retryVibration = true;
        }
        else if (result != ERROR_DEVICE_NOT_CONNECTED)
        {
            exit(1);
        }
    }

    // 0x00C746A0 (.h:36)
    ControllerType ControllerXb360::getControllerType()
    {
        return m_controllerType;
    }

    // 0x00C746B0 (.h:37)
    void ControllerXb360::setControllerType(ControllerType controllerType)
    {
        m_controllerType = controllerType;
    }
}
