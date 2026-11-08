// Original: library/modules/runtime/input/include/keen/input/win32/controller_nds_win32.hpp
//
// ControllerNds, a DirectInput pad with a fixed button mapping (ControllerNdsMapping). Placement of the mapping
// inferred.
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    struct ControllerNdsMapping;

    // size 0x2
    class ControllerNds
    {
    public:
        enum ButtonId
        {
            ButtonId_A = 1,
            ButtonId_B = 2,
            ButtonId_Select = 4,
            ButtonId_Start = 8,
            ButtonId_Right = 16,
            ButtonId_Left = 32,
            ButtonId_Up = 64,
            ButtonId_Down = 128,
            ButtonId_R = 256,
            ButtonId_L = 512,
            ButtonId_X = 1024,
            ButtonId_Y = 2048,
            ButtonId_Debug = 8192,
            ButtonId_Force32Bit = 0x7FFFFFFF,
        };

        ControllerNds(); // 0x00C78340 (:28)
        void initialize(); // 0x00C750B0 (:33)
        void shutdown(); // 0x00C750C0 (:37)
        void update(); // 0x00C750D0 (:41)
        void reset(); // 0x00C750E0 (:46)
        void setPressed(ButtonId buttonId, bool pressed); // 0x00C750F0 (:51)
        bool isPressed(ButtonId) const;
        void updateEmulation(const ControllerNdsMapping* pControllerMapping, uint32 controllerMappingCount, uint32 windowsMsg, uint32 wParam, uint32 lParam); // 0x00C78350 (:74)
    private:
        void handleKeyEvent(const ControllerNdsMapping* pControllerMapping, uint32 controllerMappingCount, uint32 keyCode, bool pressed); // 0x00C75110 (:121)

        uint16 m_pressed; // 0x00
    };

    // size 0x8
    struct ControllerNdsMapping
    {
        uint16 virtKeyCode; // 0x00
        int buttonId; // 0x04
    };
}
