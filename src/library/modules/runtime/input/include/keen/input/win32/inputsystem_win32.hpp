// Original: library/modules/runtime/input/include/keen/input/win32/inputsystem_win32.hpp
//
// WindowState, the window subclassing state of the win32 input system (original window procedures, mouse capture),
// and the inputsystem_win32.cpp functions around it (the window procedure, the mouse cursor's centring).
#pragma once

#include "keen/base/types.hpp"

struct HWND__;

namespace keen
{
    struct InputSystem;

    // size 0x38
    struct WindowState
    {
        WindowState(); // 0x00C75160 (:46)

        long (__stdcall *pOldMsgProc)(HWND__*, uint32, uint32, long); // 0x00
        HWND__* hWnd; // 0x04
        long (__stdcall *pOldMainWindowMsgProc)(HWND__*, uint32, uint32, long); // 0x08
        HWND__* hMainWnd; // 0x0C, the top level parent of hWnd
        uint32 captureCount; // 0x10, mouse buttons held down (SetCapture while non-zero)
        bool hasFocus; // 0x14
        InputSystem* pInputSystems[8]; // 0x18
    };

    extern WindowState s_windowStates[4]; // 0x038BD540

    // The win32 message translation into input events; not ported (the input event queue is not), the driver programs
    // define it (tests/headless/headless_input.cpp).
    void generateInputEventForWin32Message(InputSystem* pInputSystem, uint32 messageId, uint32 wParam, long lParam); // 0x00C7CB70 (inputsystem_win32.cpp:966)
    long __stdcall win32MsgProc(HWND__* hWnd, uint32 uMsg, uint32 wParam, long lParam); // 0x00C7D070 (inputsystem_win32.cpp:1151)
    void registerWindowProcWithInputSystem(HWND__* hWnd, InputSystem* pInputSystem); // 0x00C7D2D0 (inputsystem_win32.cpp:1268)
    void unregisterWindowProc(InputSystem* pInputSystem); // 0x00C75290 (inputsystem_win32.cpp:1313)
    float2 centerMouseCursor(InputSystem* pInputSystem); // 0x00C75430 (inputsystem_win32.cpp:1733)
}
