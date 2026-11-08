// Original: library/modules/runtime/gameframework/sources/win32/gameframeworksystem_win32.hpp (private)
#pragma once

struct HWND__;

namespace keen
{
    // Platform part of GameFrameworkSystem (its base class).
    // size 0xC
    struct GameFrameworkSystemNative
    {
        HWND__* hwnd; // 0x00
        bool quit; // 0x04
        bool showBreakHandlerMessage; // 0x05
        bool quitWithEscape; // 0x06
        bool resizeableWindow; // 0x07
        int triggerAltEnter; // 0x08
    };
}
