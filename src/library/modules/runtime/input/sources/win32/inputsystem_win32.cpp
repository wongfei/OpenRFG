// Original: library/modules/runtime/input/sources/win32/inputsystem_win32.cpp
//
// The win32 platform part of the input system: the window procedure the input system subclasses its window with, the
// mouse cursor, the win32 message translation into input events, the XInput/DirectInput/Steam controller slots and
// the per-frame platform events. The Steam controllers come from SteamController() (stub_impl.md S2); without Steam
// (OpenRFG with --no-steam) there is no controller interface and so no Steam controller.
//
// How rfg.exe hides the cursor and locks it in the window while the mouse looks around:
//   - the gameplay UI hides its own cursor (ui_cursor_set_visible(false)), which turns on vlib's relative mouse
//     movement (mouse_auto_centering(true)); vlib's mouse is visible only while the game window is not the active one
//     (mouse_generic_poll: Mouse_visible = !Os_app_active, which the focus lost/restored events set);
//   - every main thread frame keen::rfg::Application::updateTime hands both to keen (mouse_generic_update_front_thread):
//     setMouseRelativeInput centres the cursor in the focused window (SetCursorPos) and remembers the centre;
//     setMouseCursorVisible hides it (ShowCursor) and clips it to the window's client area (ClipCursor) while the
//     movement is relative;
//   - each input update (createPlatformEvents) centres it again while the window has the focus, and a WM_MOUSEMOVE
//     away from the remembered position becomes a relative movement event (rfg.exe registers no raw input, so
//     WM_INPUT's relative movement does not come);
//   - win32MsgProc clips again on WM_SIZE and WM_SETFOCUS and shows the cursor on WM_KILLFOCUS; the focus lost event
//     the same message makes then makes vlib's mouse visible, which shows the cursor and frees the clip the next frame.

//
// The file is compiled in two parts (orig_main.md OM2) so that the test programs keep their own message translation:
//   - keen_input compiles the part above (the default);
//   - keen_input_system compiles the rest (OPENRFG_INPUT_SYSTEM): the controller state, the message translation, the
//     DirectInput enumeration and the per-frame platform events. Programs that run the original framework link both.

#include "keen/input/win32/inputsystem_win32.hpp"
#include "library/modules/runtime/input/sources/global/inputsystem_private.hpp"

#include <stdlib.h>
#include <windows.h>

#if !defined(OPENRFG_INPUT_SYSTEM)
namespace keen
{
    WindowState s_windowStates[4]; // 0x038BD540

    // The client area of a window in the coordinates of its parent (the screen's for a top level window).
    static void getClientRectInParent(HWND hWnd, RECT* pRect)
    {
        GetClientRect(hWnd, pRect);
        MapWindowPoints(hWnd, GetParent(hWnd), (POINT*)pRect, 2);
    }

    // 0x00C75160 (:46)
    // Original: hMainWnd is not set (the states are static, so it starts at null).
    WindowState::WindowState()
    {
        pOldMsgProc = nullptr;
        pOldMainWindowMsgProc = nullptr;
        hWnd = nullptr;
        captureCount = 0;
        hasFocus = false;
        for (uint32 i = 0; i < 8; i++)
        {
            pInputSystems[i] = nullptr;
        }
    }

    // 0x00C7D070 (:1151)
    // Each input system of the window sees the message first: the cursor, while it is invisible, is shown when the
    // window loses the focus and hidden and clipped again when it gets it back, and clipped again when the window is
    // resized; then the message's input events. After them the mouse capture: SetCapture on the first button down,
    // ReleaseCapture when the last one is up or the focus is lost.
    // Original behaviour kept: WM_SETFOCUS hides the cursor only when it clips it (WM_KILLFOCUS shows it either way);
    // WM_KILLFOCUS releases the capture but leaves captureCount as it is; WM_DEVICECHANGE sent to a child window goes
    // to the main window's old procedure.
    long __stdcall win32MsgProc(HWND__* hWnd, uint32 uMsg, uint32 wParam, long lParam)
    {
        uint32 index = 0;
        while (hWnd != s_windowStates[index].hWnd && hWnd != s_windowStates[index].hMainWnd)
        {
            index++;
            if (index >= 4)
            {
                exit(1);
            }
        }
        WindowState& state = s_windowStates[index];
        for (uint32 i = 0; i < 8; i++)
        {
            InputSystem* pInputSystem = state.pInputSystems[i];
            if (pInputSystem == nullptr)
            {
                break;
            }
            PlatformControllerState& controllerState = pInputSystem->controllerState;
            if (uMsg == WM_KILLFOCUS)
            {
                if (!controllerState.mouseVisible)
                {
                    ShowCursor(TRUE);
                }
            }
            else if (uMsg == WM_SIZE)
            {
                if (!controllerState.mouseVisible && controllerState.clipMouseCursorWhileInvisible)
                {
                    RECT rect;
                    getClientRectInParent(hWnd, &rect);
                    ClipCursor(&rect);
                }
            }
            else if (uMsg == WM_SETFOCUS)
            {
                if (!controllerState.mouseVisible && controllerState.clipMouseCursorWhileInvisible)
                {
                    ShowCursor(FALSE);
                    RECT rect;
                    getClientRectInParent(hWnd, &rect);
                    ClipCursor(&rect);
                }
            }
            else if (uMsg == WM_DEVICECHANGE)
            {
                controllerState.deviceChangeReceived = true;
                if (hWnd != state.hMainWnd || state.hWnd != state.hMainWnd)
                {
                    return CallWindowProcW((WNDPROC)state.pOldMainWindowMsgProc, hWnd, uMsg, wParam, lParam);
                }
            }
            generateInputEventForWin32Message(pInputSystem, uMsg, wParam, lParam);
        }
        switch (uMsg)
        {
        case WM_SETFOCUS:
            state.hasFocus = true;
            break;
        case WM_KILLFOCUS:
            state.hasFocus = false;
            if (state.captureCount != 0)
            {
                ReleaseCapture();
            }
            break;
        case WM_LBUTTONDOWN:
        case WM_RBUTTONDOWN:
        case WM_MBUTTONDOWN:
            if (state.captureCount++ == 0)
            {
                SetCapture(hWnd);
            }
            break;
        case WM_LBUTTONUP:
        case WM_RBUTTONUP:
        case WM_MBUTTONUP:
            if (state.captureCount != 0 && --state.captureCount == 0)
            {
                ReleaseCapture();
            }
            break;
        default:
            break;
        }
        return CallWindowProcW((WNDPROC)state.pOldMsgProc, hWnd, uMsg, wParam, lParam);
    }

    // 0x00C7D2D0 (:1268)
    // The window's state (its own or the first free one) gets the input system; the first one subclasses the window and
    // its top level parent with win32MsgProc. The focus is the top level window's.
    // Original behaviour kept: a fifth window writes past s_windowStates.
    void registerWindowProcWithInputSystem(HWND__* hWnd, InputSystem* pInputSystem)
    {
        if (hWnd == nullptr)
        {
            return;
        }
        uint32 index = 0;
        do
        {
            if (s_windowStates[index].hWnd == hWnd || s_windowStates[index].hWnd == nullptr)
            {
                break;
            }
            index++;
        } while (index < 4);
        WindowState& state = s_windowStates[index];
        state.hWnd = hWnd;
        HWND mainWnd = hWnd;
        for (HWND parent = GetParent(mainWnd); parent != nullptr; parent = GetParent(mainWnd))
        {
            mainWnd = parent;
        }
        state.hMainWnd = mainWnd;
        for (uint32 i = 0; i < 8; i++)
        {
            if (state.pInputSystems[i] == nullptr)
            {
                state.pInputSystems[i] = pInputSystem;
                if (state.pOldMsgProc == nullptr)
                {
                    state.pOldMsgProc = (WNDPROC)SetWindowLongW(hWnd, GWL_WNDPROC, (LONG)&win32MsgProc);
                    if (hWnd != mainWnd)
                    {
                        state.pOldMainWindowMsgProc = (WNDPROC)SetWindowLongW(mainWnd, GWL_WNDPROC, (LONG)&win32MsgProc);
                    }
                }
                state.hasFocus = GetFocus() == mainWnd && GetForegroundWindow() == mainWnd;
                state.captureCount = 0;
                return;
            }
        }
        exit(1);
    }

    // 0x00C75290 (:1313)
    // The first window with the input system gets its old window procedures back.
    // Original behaviour kept: the input system stays in the window's state.
    void unregisterWindowProc(InputSystem* pInputSystem)
    {
        for (uint32 index = 0; index < 4; index++)
        {
            WindowState& state = s_windowStates[index];
            if (state.pOldMsgProc == nullptr)
            {
                continue;
            }
            for (uint32 i = 0; i < 8; i++)
            {
                if (state.pInputSystems[i] == pInputSystem)
                {
                    SetWindowLongW(state.hWnd, GWL_WNDPROC, (LONG)state.pOldMsgProc);
                    state.pOldMsgProc = nullptr;
                    if (state.hWnd != state.hMainWnd)
                    {
                        SetWindowLongW(state.hMainWnd, GWL_WNDPROC, (LONG)state.pOldMainWindowMsgProc);
                        state.pOldMainWindowMsgProc = nullptr;
                    }
                    return;
                }
            }
        }
    }

    // 0x00C75430 (:1733)
    // The cursor to the centre of the client area of the first window with the focus. Returns the centre in client
    // coordinates (WM_MOUSEMOVE's), (0, 0) without a focused window.
    float2 centerMouseCursor(InputSystem*)
    {
        for (uint32 index = 0; index < 4; index++)
        {
            if (s_windowStates[index].hasFocus)
            {
                RECT rect;
                getClientRectInParent(s_windowStates[index].hWnd, &rect);
                const int x = (rect.right - rect.left) / 2 + rect.left;
                const int y = (rect.bottom - rect.top) / 2 + rect.top;
                SetCursorPos(x, y);
                float2 centre;
                centre.x = (float)(x - rect.left);
                centre.y = (float)(y - rect.top);
                return centre;
            }
        }
        float2 none;
        none.x = 0.0f;
        none.y = 0.0f;
        return none;
    }

    namespace input
    {
        // 0x00C75320 (:1670)
        // On a change: ShowCursor until its display counter shows (>= 0) or hides (< 0) the cursor; a hidden cursor is
        // clipped to the client area of the windows with the focus (or freed when it is not to be clipped), a shown one
        // is freed.
        void setMouseCursorVisible(InputSystem* pInputSystem, bool visible, bool clipMouseCursorWhileInvisible)
        {
            PlatformControllerState& state = pInputSystem->controllerState;
            if (state.mouseVisible == visible && state.clipMouseCursorWhileInvisible == clipMouseCursorWhileInvisible)
            {
                return;
            }
            if (visible)
            {
                while (ShowCursor(TRUE) < 0)
                {
                }
            }
            else
            {
                while (ShowCursor(FALSE) >= 0)
                {
                }
            }
            state.mouseVisible = visible;
            state.clipMouseCursorWhileInvisible = clipMouseCursorWhileInvisible;
            if (visible)
            {
                ClipCursor(nullptr);
                return;
            }
            for (uint32 index = 0; index < 4; index++)
            {
                if (!s_windowStates[index].hasFocus)
                {
                    continue;
                }
                if (state.clipMouseCursorWhileInvisible)
                {
                    RECT rect;
                    getClientRectInParent(s_windowStates[index].hWnd, &rect);
                    ClipCursor(&rect);
                }
                else
                {
                    ClipCursor(nullptr);
                }
            }
        }

        // 0x00C75420 (:1714)
        bool isMouseCursorVisible(InputSystem* pInputSystem)
        {
            return pInputSystem->controllerState.mouseVisible;
        }

        // 0x00C790B0 (:1722)
        // Relative movement centres the cursor (each call, so every frame while it is on).
        void setMouseRelativeInput(InputSystem* pInputSystem, bool enabled)
        {
            pInputSystem->controllerState.mousePositionRelative = enabled;
            if (enabled)
            {
                pInputSystem->controllerState.lastMousePosition = centerMouseCursor(pInputSystem);
            }
        }

        // 0x00C757E0 (:1943)
        bool hasFocus(const InputSystem*)
        {
            for (uint32 index = 0; index < 4; index++)
            {
                if (s_windowStates[index].hWnd != nullptr && s_windowStates[index].hasFocus)
                {
                    return true;
                }
            }
            return false;
        }
    }
}
#else
#include "keen/base/filepath.hpp"
#include "keen/base/inifile.hpp"
#include "keen/base/memoryallocator.hpp"
#include "keen/base/nativefile.hpp"
#include "keen/base/string.hpp"
#include "keen/base/stringencoding.hpp"
#include "keen/base/stringscanner.hpp"
#include "keen/base/trace.hpp"

#include <string.h>

namespace keen
{
    // Lets the pads (and the cursor centring) run while no window of the input system has the focus.
    static bool s_allowBackgroundInput = true; // 0x016F0120

    // The .padcfg "type" names.
    static const char* const s_pNames[4] = {"DualShock4", "Xbox360", "XBone", "SwitchProController"}; // 0x016F0818
    static const ControllerType s_types[4] = {ControllerType_DualShock4, ControllerType_Xbox360, ControllerType_XBone, ControllerType_SwitchProController}; // 0x016F0808

    // Windows virtual keys to keen's keyboard button ids (dumped from rfg.exe .data).
    static const ButtonIdMapping s_buttonIdKeyMap[0x3D] = // 0x016F0128
    {
        {0x03, 0x0D}, {0x08, 0x0E}, {0x09, 0x0F}, {0x0D, 0x10}, {0x13, 0x11}, {0x14, 0x01}, {0x1B, 0x12}, {0x20, 0x13},
        {0x21, 0x14}, {0x22, 0x15}, {0x23, 0x16}, {0x24, 0x17}, {0x25, 0x18}, {0x26, 0x19}, {0x27, 0x1A}, {0x28, 0x1B},
        {0x2A, 0x1C}, {0x2D, 0x1D}, {0x2E, 0x1E}, {0x5B, 0x08}, {0x5C, 0x0C}, {0x60, 0x43}, {0x61, 0x44}, {0x62, 0x45},
        {0x63, 0x46}, {0x64, 0x47}, {0x65, 0x48}, {0x66, 0x49}, {0x67, 0x4A}, {0x68, 0x4B}, {0x69, 0x4C}, {0x6A, 0x4D},
        {0x6B, 0x4E}, {0x6D, 0x4F}, {0x6F, 0x50}, {0x70, 0x51}, {0x71, 0x52}, {0x72, 0x53}, {0x73, 0x54}, {0x74, 0x55},
        {0x75, 0x56}, {0x76, 0x57}, {0x77, 0x58}, {0x78, 0x59}, {0x79, 0x5A}, {0x7A, 0x5B}, {0x7B, 0x5C}, {0x7C, 0x5D},
        {0x7D, 0x5E}, {0x7E, 0x5F}, {0x7F, 0x60}, {0x80, 0x61}, {0x81, 0x62}, {0x82, 0x63}, {0x83, 0x64}, {0x84, 0x65},
        {0x85, 0x66}, {0x86, 0x67}, {0x87, 0x68}, {0x90, 0x00}, {0x91, 0x02},
    };

    // The captions of s_buttonIdKeyMap's keys.
    static const char* const s_virtualKeyCaptions[0x3D] = // 0x016F0310
    {
        "Break", "BckSpc", "Tab", "Return", "Pause", "Lock", "Esc", "Space", "PgeUp", "PgeDwn",
        "End", "Home", "Left", "Up", "Right", "Down", "Print", "Insert", "Delete", "L Win",
        "R Win", "Num 0", "Num 1", "Num 2", "Num 3", "Num 4", "Num 5", "Num 6", "Num 7", "Num 8",
        "Num 9", "Num x", "Num +", "Num -", "Num /", "F1", "F2", "F3", "F4", "F5",
        "F6", "F7", "F8", "F9", "F10", "F11", "F12", "F13", "F14", "F15",
        "F16", "F17", "F18", "F19", "F20", "F21", "F22", "F23", "F24", "Nm Lck",
        "Scroll",
    };

    // size 0x8
    struct KeyCaptionMapping
    {
        uint32 id; // 0x00
        const char* pCaption; // 0x04
    };

    // The modifier keys, which have no virtual key in s_buttonIdKeyMap.
    static const KeyCaptionMapping s_captionMap[7] = // 0x01342868
    {
        {0x06, "L Ctrl"}, {0x0A, "R Ctrl"}, {0x05, "L Shift"}, {0x09, "R Shift"}, {0x07, "L Alt"}, {0x0B, "R Alt"}, {0x6B, "Alt Gr"},
    };

    // Scan codes (lParam bits 16-23) to keen's keyboard button ids for the keys without a mapped virtual key; 0xFFFF:
    // none (dumped from rfg.exe .data).
    static const uint32 s_scanCodeToButtonMapping[256] = // 0x016F0408
    {
        0xFFFF, 0x12, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25, 0x26, 0x27, 0x28, 0x1F, 0x6C, 0x6D, 0x0E, 0x0F,
        0x39, 0x3F, 0x2D, 0x3A, 0x3C, 0x41, 0x3D, 0x31, 0x37, 0x38, 0x6E, 0x6F, 0x10, 0x06, 0x29, 0x3B,
        0x2C, 0x2E, 0x2F, 0x30, 0x32, 0x33, 0x34, 0x70, 0x71, 0x72, 0x05, 0x73, 0x42, 0x40, 0x2B, 0x3E,
        0x2A, 0x36, 0x35, 0x74, 0x75, 0x76, 0x09, 0x4D, 0x07, 0x13, 0x01, 0x51, 0x52, 0x53, 0x54, 0x55,
        0x56, 0x57, 0x58, 0x59, 0x5A, 0x00, 0x02, 0x17, 0x19, 0x14, 0xFFFF, 0x18, 0xFFFF, 0x1A, 0xFFFF, 0x16,
        0x1B, 0x15, 0x1D, 0x1E, 0xFFFF, 0xFFFF, 0x77, 0xFFFF, 0xFFFF, 0x11, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
        0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
        0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
        0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
        0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
        0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
        0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
        0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
        0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
        0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
        0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF, 0xFFFF,
    };

    // The context of the DirectInput enumeration: the platform state and the XInput pads counted so far.
    struct DirectInputEnumerationContext
    {
        PlatformControllerState* pState;
        uint32 xinputControllerCount;
    };

    // 0x00C76AF0
    PlatformControllerState::PlatformControllerState()
    {
    }

    // 0x00C76B90
    PlatformControllerState::~PlatformControllerState()
    {
    }

    // 0x00C75190 (:613)
    // One hexadecimal digit; 0xFF when it is none.
    uint8 readNibble(const char* pName)
    {
        const char c = *pName;
        if (c >= '0' && c <= '9')
        {
            return (uint8)(c - '0');
        }
        if (c >= 'a' && c <= 'f')
        {
            return (uint8)(c - 'a' + 10);
        }
        if (c >= 'A' && c <= 'F')
        {
            return (uint8)(c - 'A' + 10);
        }
        return 0xFFu;
    }

    // 0x00C751D0 (:631)
    ControllerType getControllerTypeByName(const char* pName)
    {
        for (uint32 i = 0; i < 4u; ++i)
        {
            if (compareStringNoCase(s_pNames[i], pName) == 0)
            {
                return s_types[i];
            }
        }
        return ControllerType_Unkown;
    }

    // 0x00C75210 (:648)
    static uint32 getButtonIdFromVirtualKey(uint32 virtualKey)
    {
        for (uint32 i = 0; i < 0x3Du; ++i)
        {
            if (s_buttonIdKeyMap[i].virtualKey == virtualKey)
            {
                return s_buttonIdKeyMap[i].index;
            }
        }
        return 0xFFFFu;
    }

    // 0x00C7CAB0 (:930)
    // The relative movement of a WM_INPUT mouse report (rfg.exe registers no raw input, so none come).
    static void processWmInput(InputSystem* pInputSystem, long lParam)
    {
        UINT size = 0;
        if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, nullptr, &size, sizeof(RAWINPUTHEADER)) != 0 || size > 0x80u)
        {
            return;
        }
        union
        {
            RAWINPUT input;
            uint8 bytes[0x80];
        } buffer;
        if (GetRawInputData((HRAWINPUT)lParam, RID_INPUT, &buffer.input, &size, sizeof(RAWINPUTHEADER)) == (UINT)-1 ||
            buffer.input.header.dwType != RIM_TYPEMOUSE)
        {
            return;
        }
        if (buffer.input.data.mouse.lLastX != 0 || buffer.input.data.mouse.lLastY != 0)
        {
            float2 relativeMovement;
            relativeMovement.x = (float)buffer.input.data.mouse.lLastX;
            relativeMovement.y = (float)buffer.input.data.mouse.lLastY;
            input::addMouseRelativeMovementEvent(pInputSystem, 1u, relativeMovement);
        }
    }

    // 0x00C7CB70 (:966)
    // The window's messages as input events: keys (keyboard controller 0; the virtual key's id, else the scan code's;
    // the extended right Ctrl/Alt get their own ids, AltGr's Ctrl+Alt pair becomes 0x6B), characters, mouse
    // buttons, wheel and movement (mouse controller 1; relative while relative movement is on: a move away from the last
    // position, which the cursor centring keeps at the centre), raw input and the focus.
    // Magic numbers: keen's mouse buttons are 0 (left), 1 (middle), 2 (right), 7 and 8 (the X buttons); keen's keyboard
    // ids 5/6/7 are Shift/Ctrl/Alt by scan code; with the extended flag Shift (the right one has its own scan code) is
    // 9, Ctrl becomes 10 (right Ctrl) and Alt 11 (right Alt); 0x6B is AltGr.
    // Original behaviour kept: the extended keys get their right-hand ids only when their virtual key has no mapping;
    // the right Alt (Windows sends AltGr as left Ctrl + right Alt) turns a queued left Ctrl of the same direction into
    // AltGr and is dropped itself; sendResetEvents reads past the controller table on a focus change (inputsystem.cpp).
    void generateInputEventForWin32Message(InputSystem* pInputSystem, uint32 messageId, uint32 wParam, long lParam)
    {
        switch (messageId)
        {
        case WM_SETFOCUS:
            input::addFocusLostEvent(pInputSystem, true);
            return;
        case WM_KILLFOCUS:
            input::addFocusLostEvent(pInputSystem, false);
            return;
        case WM_NCXBUTTONDOWN:
        case WM_XBUTTONDOWN:
            input::addRawButtonDownEvent(pInputSystem, 1u, (HIWORD(wParam) != XBUTTON1) + 7u);
            return;
        case WM_NCXBUTTONUP:
        case WM_XBUTTONUP:
            input::addRawButtonUpEvent(pInputSystem, 1u, (HIWORD(wParam) != XBUTTON1) + 7u);
            return;
        case WM_INPUT:
            processWmInput(pInputSystem, lParam);
            return;
        case WM_KEYDOWN:
        case WM_SYSKEYDOWN:
        case WM_KEYUP:
        case WM_SYSKEYUP:
        {
            const bool isDown = messageId == WM_KEYDOWN || messageId == WM_SYSKEYDOWN;
            uint32 buttonId = getButtonIdFromVirtualKey(wParam);
            bool checkAltGr = buttonId == 0xBu;
            if (buttonId == 0xFFFFu)
            {
                buttonId = s_scanCodeToButtonMapping[(lParam >> 16) & 0xFF];
                checkAltGr = buttonId == 0xBu;
                if ((lParam & 0x1000000) != 0)
                {
                    if (buttonId == 6u)
                    {
                        buttonId = 10u;
                        checkAltGr = false;
                    }
                    else if (buttonId == 5u)
                    {
                        buttonId = 9u;
                        checkAltGr = false;
                    }
                    else if (buttonId == 7u)
                    {
                        buttonId = 11u;
                        checkAltGr = true;
                    }
                }
            }
            if (checkAltGr)
            {
                const InputEventType type = isDown ? InputEventType_RawButtonDown : InputEventType_RawButtonUp;
                if (input::containsInputEvent(pInputSystem, type, 0u, 6u))
                {
                    input::replaceInputEvent(pInputSystem, type, 0u, 6u, 0x6Bu);
                    return;
                }
            }
            if (isDown)
            {
                input::addRawButtonDownEvent(pInputSystem, 0u, buttonId);
            }
            else
            {
                input::addRawButtonUpEvent(pInputSystem, 0u, buttonId);
            }
            return;
        }
        case WM_CHAR:
            input::addKeyEvent(pInputSystem, 0u, wParam);
            return;
        case WM_MOUSEMOVE:
        {
            PlatformControllerState& state = pInputSystem->controllerState;
            float2 position;
            position.x = (float)(short)LOWORD(lParam);
            position.y = (float)(short)HIWORD(lParam);
            if (!state.mousePositionRelative)
            {
                input::addMouseMoveEvent(pInputSystem, 1u, position);
                return;
            }
            // The differences in double precision, rounded to float.
            float2 relativeMovement;
            relativeMovement.x = (float)((double)position.x - (double)state.lastMousePosition.x);
            relativeMovement.y = (float)((double)position.y - (double)state.lastMousePosition.y);
            if (relativeMovement.x == 0.0f && relativeMovement.y == 0.0f)
            {
                return;
            }
            input::addMouseRelativeMovementEvent(pInputSystem, 1u, relativeMovement);
            state.lastMousePosition = position;
            return;
        }
        case WM_LBUTTONDOWN:
            input::addRawButtonDownEvent(pInputSystem, 1u, 0u);
            return;
        case WM_LBUTTONUP:
            input::addRawButtonUpEvent(pInputSystem, 1u, 0u);
            return;
        case WM_RBUTTONDOWN:
            input::addRawButtonDownEvent(pInputSystem, 1u, 2u);
            return;
        case WM_RBUTTONUP:
            input::addRawButtonUpEvent(pInputSystem, 1u, 2u);
            return;
        case WM_MBUTTONDOWN:
            input::addRawButtonDownEvent(pInputSystem, 1u, 1u);
            return;
        case WM_MBUTTONUP:
            input::addRawButtonUpEvent(pInputSystem, 1u, 1u);
            return;
        case WM_MOUSEWHEEL:
        {
            const int wheelData = (short)HIWORD(wParam);
            input::addMouseWheelEvent(pInputSystem, 1u, wheelData);
            pInputSystem->controllerState.mouseWheelButtonFlags |= wheelData > 0 ? 1u : 4u;
            return;
        }
        default:
            return;
        }
    }

    // 0x00C78810 (:1003)
    // The *.padcfg files next to the exe (ini files): [global] type and guid0..N (one config per product GUID), and
    // [mapping] button0..31, pov0..3 (four comma separated button ids: left, right, up, down), axisX/Y/Z and
    // rotX/Y/Z (an axis index below 48, optionally ", flip").
    // Magic number: room for 100 configs.
    // Original behaviour kept: a malformed GUID ends the file's GUIDs; the guid's first three groups are read as big
    // endian numbers, the last eight bytes as they are.
    static void scanDirectInputConfigs(PlatformControllerState* pState, MemoryAllocator* pAllocator)
    {
        FilePath searchPath;
        getCurrentExePath(&searchPath);
        searchPath.setFileNameWithExtension("*.padcfg");
        pState->directInputControllerConfigs.create(pAllocator, 100u, 0x10u);
        WIN32_FIND_DATAA findData;
        HANDLE hFind = FindFirstFileA(searchPath.getCompletePath(), &findData);
        if (hFind == INVALID_HANDLE_VALUE)
        {
            return;
        }
        do
        {
            DInputControllerConfig config;
            memset(&config, 0, sizeof(config));
            for (uint32 i = 0; i < 8u; ++i)
            {
                config.axisIndex[i] = -1;
                config.axisFunction[i] = AxisFunction_None;
            }
            FilePath configPath;
            configPath.setCompletePath(searchPath.getCompletePath(), false);
            configPath.setFileNameWithExtension(findData.cFileName);
            char value[64];
            readIniFileString(configPath, "global", "type", value, sizeof(value), "");
            config.type = getControllerTypeByName(value);
            char key[32];
            for (uint32 i = 0; i < 0x20u; ++i)
            {
                formatString(key, sizeof(key), "button%d", i);
                readIniFileString(configPath, "mapping", key, value, sizeof(value), "");
                if (value[0] == '\0')
                {
                    continue;
                }
                const char* pValue = value;
                uint32 buttonId;
                if (readUint32FromDecimalString(&buttonId, &pValue) == StringReadResult_Ok)
                {
                    skipWhiteSpaces(&pValue);
                    if (*pValue == '\0')
                    {
                        config.buttonIds[i] = buttonId;
                    }
                }
            }
            for (uint32 pov = 0; pov < 4u; ++pov)
            {
                formatString(key, sizeof(key), "pov%d", pov);
                readIniFileString(configPath, "mapping", key, value, sizeof(value), "");
                if (value[0] == '\0')
                {
                    continue;
                }
                const char* pValue = value;
                for (uint32 direction = 0; direction < 4u; ++direction)
                {
                    uint32 buttonId;
                    if (readUint32FromDecimalString(&buttonId, &pValue) == StringReadResult_Ok)
                    {
                        config.povButtonIds[pov][direction] = buttonId;
                    }
                    if (direction < 3u)
                    {
                        skipWhiteSpaces(&pValue);
                        if (*pValue == ',')
                        {
                            ++pValue;
                        }
                    }
                }
            }
            for (uint32 axis = 0; axis < 6u; ++axis)
            {
                if (axis < 3u)
                {
                    formatString(key, sizeof(key), "axis%c", (char)('X' + axis));
                }
                else
                {
                    formatString(key, sizeof(key), "rot%c", (char)('X' + axis - 3u));
                }
                readIniFileString(configPath, "mapping", key, value, sizeof(value), "");
                if (value[0] == '\0')
                {
                    continue;
                }
                const char* pValue = value;
                uint32 axisIndex;
                if (readUint32FromDecimalString(&axisIndex, &pValue) != StringReadResult_Ok)
                {
                    continue;
                }
                AxisFunction function = AxisFunction_None;
                skipWhiteSpaces(&pValue);
                if (*pValue == ',')
                {
                    ++pValue;
                    skipWhiteSpaces(&pValue);
                    if (compareString(pValue, "flip") == 0)
                    {
                        function = AxisFunction_Flip;
                    }
                }
                if (axisIndex < 0x30u)
                {
                    config.axisIndex[axis] = (int)axisIndex;
                    config.axisFunction[axis] = function;
                }
            }
            for (uint32 guidIndex = 0;; ++guidIndex)
            {
                formatString(key, sizeof(key), "guid%d", guidIndex);
                readIniFileString(configPath, "global", key, value, sizeof(value), "");
                if (value[0] == '\0')
                {
                    break;
                }
                uint8 guidBytes[16];
                const char* pValue = value;
                bool isValid = true;
                for (uint32 i = 0; i < 16u; ++i, pValue += 2)
                {
                    const uint8 high = readNibble(pValue);
                    const uint8 low = high < 0x10u ? readNibble(pValue + 1) : 0xFFu;
                    if (high >= 0x10u || low >= 0x10u)
                    {
                        isValid = false;
                        break;
                    }
                    guidBytes[i] = (uint8)((high << 4) | low);
                }
                if (!isValid)
                {
                    break;
                }
                config.guid.guid0 = ((((uint32)guidBytes[0] << 8) + guidBytes[1]) << 8 | guidBytes[2]) << 8 | guidBytes[3];
                config.guid.guid1 = (uint16)(guidBytes[4] * 0x100u + guidBytes[5]);
                config.guid.guid2 = (uint16)(guidBytes[6] * 0x100u + guidBytes[7]);
                memcpy(config.guid.guid3, &guidBytes[8], 8u);
                if (!pState->directInputControllerConfigs.isFull())
                {
                    pState->directInputControllerConfigs.pushBack(config);
                }
            }
        } while (FindNextFileA(hFind, &findData) != 0);
        FindClose(hFind);
    }

    // 0x00C78570 (:1100)
    // XInput devices only take the next XInput slot's type (XBone when the product name has "xbox one", else Xbox360);
    // other game controllers get a DirectInput slot with the config of their product GUID.
    static BOOL CALLBACK handleDirectInputControllersEnumeration(const DIDEVICEINSTANCEA* lpddi, void* pvRef)
    {
        DirectInputEnumerationContext* pContext = (DirectInputEnumerationContext*)pvRef;
        PlatformControllerState* pState = pContext->pState;
        if (dxDocu_IsXInputDevice(&lpddi->guidProduct) != 0)
        {
            if (pContext->xinputControllerCount < 4u)
            {
                char productName[128];
                memcpy(productName, lpddi->tszProductName, sizeof(productName));
                changeStringToLowercase(productName);
                pState->controllers[pContext->xinputControllerCount].setControllerType(findString(productName, "xbox one") != nullptr ? ControllerType_XBone : ControllerType_Xbox360);
            }
            else
            {
                debug::nativePrintArgs("[input] We detected more than four XInput devices - this should not happen.\n");
            }
            pContext->xinputControllerCount++;
            return DIENUM_CONTINUE;
        }
        for (uint32 i = 0; i < pState->directInputControllerCount; ++i)
        {
            if (pState->directInputControllers[i].isDeviceInstance(*lpddi))
            {
                return DIENUM_CONTINUE;
            }
        }
        if (pState->directInputControllerCount >= 8u)
        {
            return DIENUM_STOP;
        }
        IDirectInputDevice8A* pDevice = nullptr;
        if (SUCCEEDED(pState->pDirect8->CreateDevice(lpddi->guidInstance, &pDevice, nullptr)))
        {
            const GUID& product = lpddi->guidProduct;
            debug::nativePrintArgs("[input] Direct Input device found : %s, {%08x, %04x, %04x, {%02x, %02x, %02x, %02x, %02x, %02x, %02x, %02x}}\n",
                                   lpddi->tszProductName, product.Data1, product.Data2, product.Data3, product.Data4[0], product.Data4[1], product.Data4[2],
                                   product.Data4[3], product.Data4[4], product.Data4[5], product.Data4[6], product.Data4[7]);
            const DInputControllerConfig* pConfig = nullptr;
            for (uint32 i = 0; i < pState->directInputControllerConfigs.getSize(); ++i)
            {
                if (memcmp(&product, &pState->directInputControllerConfigs[i].guid, sizeof(GUID)) == 0)
                {
                    pConfig = &pState->directInputControllerConfigs[i];
                    break;
                }
            }
            pState->directInputControllers[pState->directInputControllerCount++].create(pDevice, pConfig);
        }
        return DIENUM_CONTINUE;
    }

    // 0x00C78E50 (:1176)
    static void createDirectInputDevices(PlatformControllerState* pState, MemoryAllocator* pAllocator)
    {
        scanDirectInputConfigs(pState, pAllocator);
        pState->directInputControllerCount = 0;
        if (FAILED(DirectInput8Create(GetModuleHandleW(nullptr), DIRECTINPUT_VERSION, IID_IDirectInput8A, (void**)&pState->pDirect8, nullptr)))
        {
            pState->pDirect8 = nullptr;
            return;
        }
        if (pState->pDirect8 != nullptr)
        {
            DirectInputEnumerationContext context;
            context.pState = pState;
            context.xinputControllerCount = 0;
            pState->pDirect8->EnumDevices(DI8DEVCLASS_GAMECTRL, handleDirectInputControllersEnumeration, &context, DIEDFL_ATTACHEDONLY);
        }
    }

    // 0x00C7C9C0 (:1196): the controllers ControllerSteam reports that are new (up to 4), with the first action set
    // active (stub_impl.md S2)
    static void updateSteamControllers(PlatformControllerState* pState)
    {
        uint64 handles[4];
        // Magic number: room for 4 Steam controllers.
        const uint32 handleCount = ControllerSteam::getRegisteredControllers(handles, 4u);
        for (uint32 i = 0; i < handleCount; ++i)
        {
            bool known = false;
            for (uint32 j = 0; j < pState->steamControllerCount; ++j)
            {
                if (pState->steamController[j].isController(handles[i]))
                {
                    known = true;
                    break;
                }
            }
            if (!known && pState->steamControllerCount < 4u)
            {
                pState->steamController[pState->steamControllerCount].create(handles[i]);
                pState->steamControllerCount++;
            }
        }
    }

    namespace input
    {
        // 0x00C7E690 (:1343)
        // The window gets the input system's window procedure; the mouse is visible and absolute; keyboard (0) and
        // mouse (1) connect; the XInput pads are polled and the DirectInput devices created (each connects at once);
        // then the Steam controllers.
        void createPlatformControllerState(PlatformControllerState* pState, InputSystem* pInputSystem, MemoryAllocator* pAllocator, const InputSystemCreationParameters& parameters)
        {
            pState->deviceChangeReceived = false;
            registerWindowProcWithInputSystem((HWND__*)parameters.pWindowHandle, pInputSystem);
            pState->mouseVisible = true;
            pState->mousePositionRelative = false;
            for (uint32 i = 0; i < 4u; ++i)
            {
                pInputSystem->controllerState.controllers[i].updateState(i);
            }
            addControllerConnectedEvent(pInputSystem, 0u);
            addControllerConnectedEvent(pInputSystem, 1u);
            createDirectInputDevices(pState, pAllocator);
            for (uint32 controllerId = 6u; controllerId - 6u < pState->directInputControllerCount; ++controllerId)
            {
                addControllerConnectedEvent(pInputSystem, controllerId);
            }
            memset(pState->lastState, 0, sizeof(pState->lastState));
            pState->mouseWheelButtonFlags = 0;
            pState->steamControllerCount = 0;
            ControllerSteam::registerHandles(parameters.pControllerActions, parameters.controllerActionCount);
            updateSteamControllers(pState);
            pState->apiHidden[0] = false;
            pState->apiHidden[1] = false;
            pState->apiHidden[2] = false;
        }

        // 0x00C75300 (:1384)
        void destroyPlatformControllerState(PlatformControllerState*, InputSystem* pInputSystem, MemoryAllocator*)
        {
            unregisterWindowProc(pInputSystem);
        }

        // 0x00C75310 (:1392)
        void resetSystemIdleState()
        {
            SetThreadExecutionState(ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED);
        }

        // 0x00C7E830 (:1538)
        // A device change re-polls the XInput pads and re-enumerates DirectInput. Each window of the input system updates
        // its focus (the focused window or its top level parent must be the window or its main window, and so must the
        // foreground window); with the focus (or background input allowed) the cursor is centred while the movement is
        // relative and hidden, and the pads make their events (XInput 2-5, DirectInput 6-13, Steam 14-17; a hidden API
        // reports connected pads with nothing pressed). Last, the wheel as buttons 5 (up) and 6 (down): a wheel turn is
        // a button down this frame and a button up the next.
        void createPlatformEvents(InputSystem* pInputSystem, PlatformControllerState* pControllerState)
        {
            if (pControllerState->deviceChangeReceived)
            {
                pControllerState->deviceChangeReceived = false;
                for (uint32 i = 0; i < 4u; ++i)
                {
                    pInputSystem->controllerState.controllers[i].updateState(i);
                }
                if (pControllerState->pDirect8 != nullptr)
                {
                    DirectInputEnumerationContext context;
                    context.pState = pControllerState;
                    context.xinputControllerCount = 0;
                    pControllerState->pDirect8->EnumDevices(DI8DEVCLASS_GAMECTRL, handleDirectInputControllersEnumeration, &context, DIEDFL_ATTACHEDONLY);
                }
                updateSteamControllers(pControllerState);
            }
            for (uint32 index = 0; index < 4u; ++index)
            {
                WindowState& state = s_windowStates[index];
                if (state.hWnd == nullptr || state.hMainWnd == nullptr)
                {
                    continue;
                }
                const HWND hFocus = GetFocus();
                HWND hTopLevel = hFocus;
                for (HWND hWindow = hFocus; hWindow != nullptr; hWindow = GetParent(hWindow))
                {
                    hTopLevel = hWindow;
                }
                const bool hasFocus = (hFocus == state.hWnd || hTopLevel == state.hMainWnd) &&
                                      (GetForegroundWindow() == state.hWnd || GetForegroundWindow() == state.hMainWnd);
                if (hasFocus != state.hasFocus)
                {
                    state.hasFocus = hasFocus;
                }
                if (!s_allowBackgroundInput && !state.hasFocus)
                {
                    continue;
                }
                for (uint32 slot = 0; slot < 8u; ++slot)
                {
                    if (state.pInputSystems[slot] != pInputSystem)
                    {
                        continue;
                    }
                    if (state.hasFocus && pControllerState->mousePositionRelative && !pControllerState->mouseVisible)
                    {
                        pControllerState->lastMousePosition = centerMouseCursor(pInputSystem);
                    }
                    GenericPadInput padInput;
                    for (uint32 i = 0; i < 4u; ++i)
                    {
                        if (!pInputSystem->controllerState.apiHidden[WindowsControllerApi_XInput])
                        {
                            pControllerState->controllers[i].fillInGenericPadInput(&padInput, i);
                        }
                        else
                        {
                            memset(&padInput, 0, sizeof(padInput));
                            padInput.isConnected = true;
                        }
                        generateGenericPadEvents(pInputSystem, &pControllerState->lastState[i], padInput, i + 2u);
                    }
                    for (uint32 i = 0; i < pControllerState->directInputControllerCount; ++i)
                    {
                        if (!pInputSystem->controllerState.apiHidden[WindowsControllerApi_DInput])
                        {
                            pControllerState->directInputControllers[i].fillInGenericPadInput(&padInput);
                        }
                        else
                        {
                            memset(&padInput, 0, sizeof(padInput));
                            padInput.isConnected = true;
                        }
                        generateGenericPadEvents(pInputSystem, &pControllerState->lastState[4u + i], padInput, i + 6u);
                    }
                    for (uint32 i = 0; i < pControllerState->steamControllerCount; ++i)
                    {
                        if (!pInputSystem->controllerState.apiHidden[WindowsControllerApi_SteamController])
                        {
                            pControllerState->steamController[i].fillInGenericPadInput(&padInput);
                        }
                        else
                        {
                            memset(&padInput, 0, sizeof(padInput));
                            padInput.isConnected = true;
                        }
                        generateGenericPadEvents(pInputSystem, &pControllerState->lastState[12u + i], padInput, i + 14u);
                    }
                }
            }
            InputEvent event;
            event.controllerId = 1u;
            const ControllerType mouseType = pInputSystem->controllerInfos[1].type;
            if ((pControllerState->mouseWheelButtonFlags & 2u) != 0)
            {
                event.type = InputEventType_RawButtonUp;
                event.data.button.buttonIndex = 5u;
                event.data.button.standardButton = getStandardButtonIdFromButtonIndex(pInputSystem, mouseType, 5u);
                removeEvent(pInputSystem, event);
                addInputEvent(pInputSystem, event);
            }
            if ((pControllerState->mouseWheelButtonFlags & 8u) != 0)
            {
                event.type = InputEventType_RawButtonUp;
                event.data.button.buttonIndex = 6u;
                event.data.button.standardButton = getStandardButtonIdFromButtonIndex(pInputSystem, mouseType, 6u);
                removeEvent(pInputSystem, event);
                addInputEvent(pInputSystem, event);
            }
            pControllerState->mouseWheelButtonFlags &= ~0xAu;
            if ((pControllerState->mouseWheelButtonFlags & 1u) != 0)
            {
                event.type = InputEventType_RawButtonDown;
                event.data.button.buttonIndex = 5u;
                event.data.button.standardButton = getStandardButtonIdFromButtonIndex(pInputSystem, mouseType, 5u);
                storeEvent(pInputSystem, event);
                addInputEvent(pInputSystem, event);
                pControllerState->mouseWheelButtonFlags = (pControllerState->mouseWheelButtonFlags & ~1u) | 2u;
            }
            if ((pControllerState->mouseWheelButtonFlags & 4u) != 0)
            {
                event.type = InputEventType_RawButtonDown;
                event.data.button.buttonIndex = 6u;
                event.data.button.standardButton = getStandardButtonIdFromButtonIndex(pInputSystem, mouseType, 6u);
                storeEvent(pInputSystem, event);
                addInputEvent(pInputSystem, event);
                pControllerState->mouseWheelButtonFlags = (pControllerState->mouseWheelButtonFlags & ~4u) | 8u;
            }
        }

        // 0x00C75540 (:1783)
        // The pads' standard buttons: Xbox, Xbox One, DualShock 4 and Switch Pro by XInput's layout (the Switch Pro's
        // start/select at 16/17), an unconfigured DirectInput pad by its first two buttons and its first hat.
        StandardButtonId getPlatformStandardButtonIdFromButtonIndex(const InputSystem*, ControllerType controllerType, uint32 buttonIndex)
        {
            if (controllerType == ControllerType_Xbox360 || controllerType == ControllerType_XBone || controllerType == ControllerType_DualShock4 ||
                controllerType == ControllerType_SwitchProController)
            {
                const uint32 startButton = controllerType == ControllerType_SwitchProController ? 0x10u : 0xEu;
                switch (buttonIndex)
                {
                case 0:
                    return StandardButtonId_Left;
                case 1:
                    return StandardButtonId_Right;
                case 2:
                    return StandardButtonId_Up;
                case 3:
                    return StandardButtonId_Down;
                case 4:
                    return StandardButtonId_Confirm;
                case 5:
                    return StandardButtonId_Exit;
                default:
                    if (buttonIndex == startButton)
                    {
                        return StandardButtonId_StartPause;
                    }
                    if (buttonIndex == startButton + 1u)
                    {
                        return StandardButtonId_Select;
                    }
                    return StandardButtonId_Unknown;
                }
            }
            if (controllerType == ControllerType_Unkown)
            {
                switch (buttonIndex)
                {
                case 0:
                    return StandardButtonId_Confirm;
                case 1:
                    return StandardButtonId_Exit;
                case 0x20:
                    return StandardButtonId_Left;
                case 0x21:
                    return StandardButtonId_Right;
                case 0x22:
                    return StandardButtonId_Up;
                case 0x23:
                    return StandardButtonId_Down;
                default:
                    return StandardButtonId_Unknown;
                }
            }
            return StandardButtonId_Unknown;
        }

        // 0x00C756D0 (:1879)
        // Magic numbers: the keyboard has 120 buttons, the mouse 9, an XInput pad 24 buttons and 6 axes, DirectInput and
        // Steam pads 32 and 6.
        void fillPlatformControllerInfo(ControllerInfo* pInfo, PlatformControllerState* pControllerState, uint32 controllerId)
        {
            if (controllerId == 1u)
            {
                pInfo->controllerClass = ControllerClass_Mouse;
                pInfo->type = ControllerType_Mouse;
                pInfo->axisCount = 0;
                pInfo->buttonCount = 9u;
                pInfo->features = 0;
                return;
            }
            if (controllerId == 0u)
            {
                pInfo->controllerClass = ControllerClass_Keyboard;
                pInfo->type = ControllerType_Keyboard;
                pInfo->axisCount = 0;
                pInfo->buttonCount = 0x78u;
                pInfo->features = 0;
                return;
            }
            if (controllerId - 2u < 4u)
            {
                pInfo->controllerClass = ControllerClass_Controller;
                pInfo->type = pControllerState->controllers[controllerId - 2u].getControllerType();
                pInfo->axisCount = 6u;
                pInfo->buttonCount = 0x18u;
                pInfo->features = 0;
                return;
            }
            if (controllerId - 6u < 8u)
            {
                if (controllerId - 6u < pControllerState->directInputControllerCount)
                {
                    pInfo->controllerClass = ControllerClass_Controller;
                    pInfo->type = pControllerState->directInputControllers[controllerId - 6u].getControllerType();
                    pInfo->buttonCount = 0x20u;
                    pInfo->axisCount = 6u;
                    pInfo->features = 0;
                    return;
                }
            }
            else
            {
                if (controllerId < 0xEu)
                {
                    return;
                }
                if (controllerId - 0xEu < pControllerState->steamControllerCount)
                {
                    pInfo->controllerClass = ControllerClass_Controller;
                    pInfo->type = ControllerType_Steam;
                    pInfo->buttonCount = 0x20u;
                    pInfo->axisCount = 6u;
                    pInfo->features = 0;
                    return;
                }
            }
            pInfo->type = ControllerType_Unkown;
            pInfo->controllerClass = ControllerClass_None;
        }

        // 0x00C78EC0 (:1397): every Steam controller
        void setSteamControllerActionSet(InputSystem* pInputSystem, uint32 setIndex)
        {
            for (uint32 i = 0; i < pInputSystem->controllerState.steamControllerCount; ++i)
            {
                pInputSystem->controllerState.steamController[i].setActionSet(setIndex);
            }
        }

        // 0x00C75810
        bool getButtonCaption(char* pBuffer, uint32 capacity, ControllerType controllerType, uint32 buttonIndex)
        {
            if (controllerType != ControllerType_Keyboard)
            {
                return false;
            }
            for (uint32 i = 0; i < 0x3Du; ++i)
            {
                if (s_buttonIdKeyMap[i].index == buttonIndex)
                {
                    copyString(pBuffer, capacity, s_virtualKeyCaptions[i]);
                    return true;
                }
            }
            for (uint32 i = 0; i < 7u; ++i)
            {
                if (s_captionMap[i].id == buttonIndex)
                {
                    copyString(pBuffer, capacity, s_captionMap[i].pCaption);
                    return true;
                }
            }
            for (UINT scanCode = 0; scanCode < 0x100u; ++scanCode)
            {
                if (s_scanCodeToButtonMapping[scanCode] != buttonIndex)
                {
                    continue;
                }
                const UINT virtualKey = MapVirtualKeyW(scanCode, MAPVK_VSC_TO_VK);
                if (virtualKey == 0)
                {
                    return false;
                }
                // '0'-'9' and 'A'-'Z' are their own captions.
                if ((virtualKey > 0x2F && virtualKey < 0x3A) || virtualKey - 0x41u < 0x1Au)
                {
                    if (capacity > 1u)
                    {
                        pBuffer[0] = (char)virtualKey;
                        pBuffer[1] = '\0';
                        return true;
                    }
                    if (capacity == 1u)
                    {
                        pBuffer[0] = '\0';
                        return true;
                    }
                }
                static const BYTE s_keyState[256] = {};
                WCHAR text[64];
                int count = ToUnicodeEx(virtualKey, scanCode, s_keyState, text, 64, 0, GetKeyboardLayout(0));
                if (count != -1)
                {
                    // Magic number: a dead key stays in the layout's state, repeated until the count settles (5 tries).
                    for (int i = 0; i < 5; ++i)
                    {
                        const int previous = count;
                        count = ToUnicodeEx(virtualKey, scanCode, s_keyState, text, 64, 0, GetKeyboardLayout(0));
                        if (previous == count)
                        {
                            break;
                        }
                    }
                }
                if (count == 0)
                {
                    return false;
                }
                uint32 end = (uint32)(count < 0 ? -count : count) + 1u;
                if (end > 0x3Fu)
                {
                    end = 0x3Fu;
                }
                text[end] = L'\0';
                CharUpperW(text);
                convertUTF16ToUTF8(pBuffer, capacity, (const uint16*)text);
                return true;
            }
            return false;
        }

        // 0x00C78F30 (:1410): the first Steam controller's
        uint32 getSteamControllerActionSet(InputSystem* pInputSystem)
        {
            if (pInputSystem->controllerState.steamControllerCount != 0)
            {
                return pInputSystem->controllerState.steamController[0].getActionSet();
            }
            return 0u;
        }

        // 0x00C7D410 (:1487): the first Steam controller's
        void getSteamControllerActionOrigins(const InputSystem* pInputSystem, const SteamControllerAction& action, uint32 setIndex, SteamControllerActionOrigin* pOriginsOut, uint32 bufferLength, uint32* pCountOut)
        {
            if (pInputSystem->controllerState.steamControllerCount != 0)
            {
                const_cast<InputSystem*>(pInputSystem)->controllerState.steamController[0].getActionOrigins(action, setIndex, pOriginsOut, bufferLength, pCountOut);
                return;
            }
            *pCountOut = 0u;
        }

        // 0x00C79090 (:1527)
        const char* getSteamControllerActionOriginText(SteamControllerActionOrigin origin)
        {
            return ControllerSteam::getSteamControllerActionOriginText(origin);
        }

        // 0x00C79060 (:1511): the first Steam controller's binding panel
        bool showSteamControllerBindingPanel(const InputSystem* pInputSystem)
        {
            if (pInputSystem->controllerState.steamControllerCount == 0)
            {
                return false;
            }
            return pInputSystem->controllerState.steamController[0].showBindingPanel();
        }

        // 0x00C75A10 (:2066)
        // The text a key event types (orig_main.md OM5, the console's input): the key's virtual key (from its scan code,
        // else from the virtual key table) through the current keyboard layout, with shift, AltGr (Ctrl + Alt) and
        // num lock as given.
        // Notes (decompiled behaviour, kept as is): the result is terminated one character after its end; the buffer is
        // zeroed here first, so that character is a terminator too (rfg.exe's stack buffer is not cleared).
        void convertKeyEventToText(char* pBuffer, uint32 capacity, uint32 keyEvent, bool holdingShift, bool holdingAltGr, bool numLockActivated)
        {
            if (capacity != 0)
            {
                *pBuffer = '\0';
            }
            UINT scanCode = 0;
            UINT virtualKey = 0;
            for (; scanCode < 0x100u; ++scanCode)
            {
                if (s_scanCodeToButtonMapping[scanCode] == keyEvent)
                {
                    virtualKey = MapVirtualKeyW(scanCode, MAPVK_VSC_TO_VK);
                    break;
                }
            }
            if (virtualKey == 0)
            {
                uint32 i = 0;
                while (s_buttonIdKeyMap[i].index != keyEvent)
                {
                    ++i;
                    if (i > 0x3Cu)
                    {
                        return;
                    }
                }
                virtualKey = s_buttonIdKeyMap[i].virtualKey;
                scanCode = MapVirtualKeyW(virtualKey, MAPVK_VK_TO_VSC);
                if (virtualKey == 0)
                {
                    return;
                }
            }
            BYTE keyState[256] = {};
            // Magic number: 0x80, a pressed key's state bit.
            if (holdingShift)
            {
                keyState[VK_SHIFT] = 0x80;
            }
            if (holdingAltGr)
            {
                keyState[VK_CONTROL] = 0x80;
                keyState[VK_MENU] = 0x80;
            }
            if (numLockActivated)
            {
                keyState[VK_NUMLOCK] = 0x80;
            }
            WCHAR text[64] = {};
            const int count = ToUnicodeEx(virtualKey, scanCode, keyState, text, 64, 0, GetKeyboardLayout(0));
            if (count < 1)
            {
                return;
            }
            uint32 end = (uint32)count + 1u;
            if (end > 0x3Fu)
            {
                end = 0x3Fu;
            }
            text[end] = L'\0';
            convertUTF16ToUTF8(pBuffer, capacity, (const uint16*)text);
        }

        // 0x00C75B60 (:2163)
        void setControllerApiHidden(InputSystem* pInputSystem, WindowsControllerApi api, bool hidden)
        {
            pInputSystem->controllerState.apiHidden[api] = hidden;
        }

        // 0x00C75B80 (:2168)
        bool getControllerApiHidden(InputSystem* pInputSystem, WindowsControllerApi api)
        {
            return pInputSystem->controllerState.apiHidden[api];
        }
    }
}
#endif
