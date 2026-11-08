// Original: library/modules/runtime/input/sources/win32/controllerdinput_win32.cpp
//
// A DirectInput game controller (controller ids 6-13): its joystick state as a generic pad, through the mapping of a
// .padcfg file (DInputControllerConfig, scanDirectInputConfigs) or, without one, a default layout.
//
// Partial (orig_main.md OM2): what the input system reaches. Not ported: destroy.

#include "library/modules/runtime/input/sources/win32/controllerdinput_win32.hpp"
#include "keen/input/inputsystem.hpp"
#include "keen/base/trace.hpp"

#include <string.h>

namespace keen
{
    // |value| as the original computes it: value - (-value) >= 0 picks value.
    static float dinputAbs(float value)
    {
        const float negated = -value;
        return (float)((double)value - (double)negated) >= 0.0f ? value : negated;
    }

    // An axis from 0..65535 to -1..1 without the dead zone (rescaled to start at 0 past it).
    // Magic numbers: the dead zone 0.09765625 (100 of 1024) and its scale 1 / (1 - 0.09765625).
    static float getAxisValue(LONG raw)
    {
        const float value = (float)((double)(raw - 0x8000) * 3.0517578125e-05);
        if (dinputAbs(value) < 0.09765625f)
        {
            return 0.0f;
        }
        const float sign = value >= 0.0f ? 1.0f : -1.0f;
        return (float)(((double)value - (double)sign * 0.09765625) * 1.1082251082251082);
    }

    // The four directions of a POV hat (hundredths of degrees, centred when the low word is 0xFFFF) as quadrants
    // 0 (north) to 4: west, east, north and south each cover 135 degrees around their direction.
    static void setPovButtons(input::GenericPadInput* pInput, DWORD pov, uint64 left, uint64 right, uint64 up, uint64 down)
    {
        if ((short)pov == -1)
        {
            return;
        }
        // Magic numbers: 36000 hundredths of a degree per turn, 4 quadrants.
        const float quadrant = (float)(((double)(float)pov / 36000.0) * 4.0);
        if (2.25f <= quadrant && quadrant <= 3.75f)
        {
            pInput->buttons.setBit(left);
        }
        if (0.25f <= quadrant && quadrant <= 1.75f)
        {
            pInput->buttons.setBit(right);
        }
        if (quadrant <= 0.75f || 3.25f <= quadrant)
        {
            pInput->buttons.setBit(up);
        }
        if (1.25f <= quadrant && quadrant <= 2.75f)
        {
            pInput->buttons.setBit(down);
        }
    }

    // 0x00C75010 (:16)
    ControllerDInputWin32::ControllerDInputWin32()
    {
        m_pDevice = nullptr;
        m_pConfig = nullptr;
    }

    // 0x00C75020 (:22)
    // Original behaviour kept: a slot that is reused keeps its old config when the new one is null.
    void ControllerDInputWin32::create(IDirectInputDevice8A* pDevice, const DInputControllerConfig* pConfig)
    {
        m_pDevice = pDevice;
        pDevice->SetDataFormat(&c_dfDIJoystick);
        m_pDevice->Acquire();
        if (m_pDevice != nullptr)
        {
            m_pConfig = pConfig;
        }
        if (m_pConfig == nullptr)
        {
            debug::nativePrintArgs("   not recognized as supported controller\n");
        }
    }

    // 0x00C750A0 (:50)
    ControllerType ControllerDInputWin32::getControllerType() const
    {
        if (m_pConfig != nullptr)
        {
            return m_pConfig->type;
        }
        return ControllerType_Unkown;
    }

    // 0x00C77750 (:87)
    // Without a config: buttons 0-31 as they are, the POV hats as buttons 32-47 (left, right, up, down each), the axes
    // X, -Y, Rx, -Ry, Z, Rz. With one: the buttons and hats to the config's buttons, the six axes to the config's axes
    // (flipped when it says so). A lost device is reacquired and reported disconnected.
    void ControllerDInputWin32::fillInGenericPadInput(input::GenericPadInput* pInput)
    {
        DIJOYSTATE state;
        if (m_pDevice->GetDeviceState(sizeof(DIJOYSTATE), &state) != DI_OK)
        {
            pInput->isConnected = false;
            m_pDevice->SetDataFormat(&c_dfDIJoystick);
            m_pDevice->Acquire();
            return;
        }
        memset(pInput, 0, sizeof(input::GenericPadInput));
        pInput->isConnected = true;
        if (m_pConfig == nullptr)
        {
            for (uint32 i = 0; i < 32u; ++i)
            {
                if (state.rgbButtons[i] != 0)
                {
                    pInput->buttons.setBit(i);
                }
            }
            for (uint32 i = 0; i < 4u; ++i)
            {
                const uint64 first = 0x20u + i * 4u;
                setPovButtons(pInput, state.rgdwPOV[i], first, first + 1u, first + 2u, first + 3u);
            }
        }
        else
        {
            for (uint32 i = 0; i < 32u; ++i)
            {
                if (state.rgbButtons[i] != 0)
                {
                    pInput->buttons.setBit(m_pConfig->buttonIds[i]);
                }
            }
            for (uint32 i = 0; i < 4u; ++i)
            {
                const uint64* pIds = m_pConfig->povButtonIds[i];
                setPovButtons(pInput, state.rgdwPOV[i], pIds[0], pIds[1], pIds[2], pIds[3]);
            }
        }
        const LONG axes[6] = {state.lX, state.lY, state.lZ, state.lRx, state.lRy, state.lRz};
        if (m_pConfig != nullptr)
        {
            for (uint32 i = 0; i < 6u; ++i)
            {
                const int axisIndex = m_pConfig->axisIndex[i];
                if (axisIndex == -1)
                {
                    continue;
                }
                float value = getAxisValue(axes[i]);
                if (m_pConfig->axisFunction[i] == AxisFunction_Flip)
                {
                    value = -value;
                }
                pInput->axis[axisIndex] = value;
            }
            return;
        }
        pInput->axis[0] = getAxisValue(state.lX);
        pInput->axis[1] = -getAxisValue(state.lY);
        pInput->axis[2] = getAxisValue(state.lRx);
        pInput->axis[3] = -getAxisValue(state.lRy);
        pInput->axis[4] = getAxisValue(state.lZ);
        pInput->axis[5] = getAxisValue(state.lRz);
    }

    // 0x00C78260 (:251)
    bool ControllerDInputWin32::isDeviceInstance(const DIDEVICEINSTANCEA& instance) const
    {
        if (m_pDevice == nullptr)
        {
            return false;
        }
        DIDEVICEINSTANCEA deviceInstance;
        memset(&deviceInstance, 0, sizeof(deviceInstance));
        deviceInstance.dwSize = sizeof(deviceInstance);
        m_pDevice->GetDeviceInfo(&deviceInstance);
        return memcmp(&deviceInstance.guidInstance, &instance.guidInstance, sizeof(GUID)) == 0;
    }
}
