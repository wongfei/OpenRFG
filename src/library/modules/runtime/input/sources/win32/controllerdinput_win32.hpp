// Original: library/modules/runtime/input/sources/win32/controllerdinput_win32.hpp
//
// ControllerDInputWin32, a DirectInput game controller with a configurable mapping (DInputControllerConfig), and
// dxDocu_IsXInputDevice (isdinputdevicexinput.cpp: filters XInput devices out of DirectInput). Placement of the config
// and the function inferred.
#pragma once

#include "keen/base/types.hpp"
#include <windows.h>
#include <dinput.h>
#include "keen/input/controllermapping.hpp"
#include "keen/input/inputevents.hpp"

struct DIDEVICEINSTANCEA;
struct IDirectInputDevice8A;
struct _GUID;

namespace keen
{
    namespace input
    {
        struct GenericPadInput;
    }
}

namespace keen
{
    struct DInputControllerConfig;

    // size 0x8
    class ControllerDInputWin32
    {
    public:
        ControllerDInputWin32(); // 0x00C75010 (:16)
        void create(IDirectInputDevice8A* pDevice, const DInputControllerConfig* pConfig); // 0x00C75020 (:22)
        void destroy(); // 0x00C75070 (:39)
        void fillInGenericPadInput(input::GenericPadInput* pInput); // 0x00C77750 (:87)
        ControllerType getControllerType() const; // 0x00C750A0 (:50)
        bool isDeviceInstance(const DIDEVICEINSTANCEA& instance) const; // 0x00C78260 (:251)
        bool isUsed() const; // 0x00C746C0 (.h:31)

    protected:
        IDirectInputDevice8A* m_pDevice; // 0x00
        const DInputControllerConfig* m_pConfig; // 0x04
    };

    // size 0x1D8
    struct DInputControllerConfig
    {
        struct
        {
            uint32 guid0; // 0x00
            uint16 guid1; // 0x04
            uint16 guid2; // 0x06
            uint8 guid3[8]; // 0x08
        } guid; // 0x00
        ControllerType type; // 0x10
        int axisIndex[8]; // 0x14
        AxisFunction axisFunction[8]; // 0x34
        uint64 buttonIds[32]; // 0x58
        uint64 povButtonIds[4][4]; // 0x158
    };

    int dxDocu_IsXInputDevice(const _GUID* pGuidProductFromDirectInput); // 0x00C75B90 (isdinputdevicexinput.cpp:23)
}
