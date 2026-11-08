// Original: volition/vlib/code/os/os.cpp
//
// The vlib OS layer on win32. In rfg.exe the window and its message loop belong to keen's GameFramework; the vlib
// window (os_create_window, win32_message_handler) is still linked but nothing creates it, so the session and power
// state it tracks stays at its defaults. The game writes Os_app_active from its focus events (application.cpp).

#include "volition/vlib/code/os/os.h"

#include "keen/base/calendartime.hpp"
#include "keen/base/string.hpp"

#include <string.h>
#include <windows.h>
#include <wtsapi32.h>

// size 0x4
struct os_hook_item
{
    bool (*function)(unsigned int, unsigned int, unsigned int); // 0x00
};

static wchar_t Os_app_class[128]; // 0x01CDAE50, never written: the window class name is empty
static wchar_t Os_app_title[128]; // 0x01CDB910
static os_hook_item Os_hooks[32]; // 0x01CE25C8
static bool Os_app_power_suspend; // 0x01CE872F
static HWND Os_app_hwnd; // 0x01CE875C
static bool Os_app_session_lock; // 0x01CE8764
static bool Os_app_session_disconnect; // 0x01CE8765
static bool Os_app_restore; // 0x01CE8766
static bool Os_window_closed; // 0x01CE8768
static void (*Os_focus_change_callback)(bool, bool); // 0x01CE876C
static int Os_num_hooks; // 0x01CE8770
static bool Os_app_fullscreen = true; // 0x01519590, true in .data
bool Os_app_active = true; // 0x01519591, true in .data

// 0x0058B000 (:116)
unsigned long os_get_window_style(const bool fullscreen)
{
    // WS_POPUP | WS_VISIBLE, windowed adds WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX
    return fullscreen ? 0x90000000 : 0x90CA0000;
}

// 0x005AD7D0 (:138)
bool os_get_client_to_screen_coords(int* screen_x, int* screen_y, const int client_x, const int client_y)
{
    if (screen_x == nullptr || screen_y == nullptr)
    {
        return false;
    }
    POINT point;
    point.x = client_x;
    point.y = client_y;
    BOOL result = ClientToScreen(Os_app_hwnd, &point);
    *screen_x = point.x;
    *screen_y = point.y;
    return result == TRUE;
}

// 0x0058B020 (:169)
bool os_should_close()
{
    return Os_window_closed;
}

// 0x0058B030 (:174)
void os_set_fullscreen(const bool fullscreen)
{
    Os_app_fullscreen = fullscreen;
}

// 0x0058B040 (:179)
static bool os_should_suspend()
{
    return Os_app_power_suspend || Os_app_session_disconnect || Os_app_session_lock;
}

// 0x0058B070 (:184)
static void os_focus_change_callback(bool focused, bool active)
{
    if (Os_focus_change_callback != nullptr)
    {
        Os_focus_change_callback(focused, active);
    }
}

// 0x0058B090 (:191)
static void os_handle_suspend(HWND__* hwnd, bool was_suspended)
{
    if (os_should_suspend())
    {
        if (!was_suspended && !IsIconic(hwnd))
        {
            os_focus_change_callback(Os_app_active, false);
            OutputDebugStringA("Minimize!\n");
            CloseWindow(hwnd);
            Os_app_restore = true;
            return;
        }
        if (os_should_suspend())
        {
            return;
        }
    }
    if (was_suspended && Os_app_restore)
    {
        OutputDebugStringA("Restore!\n");
        OpenIcon(hwnd);
        Os_app_restore = false;
        os_focus_change_callback(Os_app_active, true);
    }
}

// 0x0058B150 (:209)
void os_received_input()
{
    SetThreadExecutionState(ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED); // 3: keep the screen saver and sleep away
}

// 0x0058B160 (:215)
void os_set_focus_change_callback(void (*callback)(bool, bool))
{
    Os_focus_change_callback = callback;
}

// 0x0058B170 (:227)
static LRESULT CALLBACK win32_message_handler(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    for (int i = 0; i < Os_num_hooks; i++)
    {
        if (Os_hooks[i].function(msg, wParam, lParam))
        {
            break;
        }
    }
    bool was_suspended = os_should_suspend();
    switch (msg)
    {
    case WM_DESTROY:
    case WM_CLOSE:
    case WM_QUIT:
        WTSUnRegisterSessionNotification(hwnd);
        // fall through
    case WM_ENDSESSION:
        Os_window_closed = true;
        return 0;
    case WM_ACTIVATEAPP:
        Os_app_active = wParam != 0;
        // original: calls the callback without the null check of os_focus_change_callback
        Os_focus_change_callback(Os_app_active, !was_suspended);
        return 0;
    case WM_KEYDOWN:
    case WM_KEYUP:
    case WM_SYSKEYDOWN:
    case WM_SYSKEYUP:
        // the Windows keys do nothing while the game is active
        if (Os_app_active && (wParam == VK_LWIN || wParam == VK_RWIN))
        {
            return 0;
        }
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    case WM_SYSCOMMAND:
        if (wParam == SC_SCREENSAVE)
        {
            if (Os_app_active)
            {
                return 0;
            }
        }
        else if (wParam == SC_KEYMENU)
        {
            return 0;
        }
        return DefWindowProcW(hwnd, WM_SYSCOMMAND, wParam, lParam);
    case WM_POWERBROADCAST:
        if (wParam == PBT_APMSUSPEND)
        {
            OutputDebugStringA("Suspend!\n");
            Os_app_power_suspend = true;
        }
        else if (wParam == PBT_APMRESUMEAUTOMATIC)
        {
            OutputDebugStringA("Resume!\n");
            Os_app_power_suspend = false;
        }
        else
        {
            return 0;
        }
        os_handle_suspend(hwnd, was_suspended);
        return 1;
    case WM_WTSSESSION_CHANGE:
        switch (wParam)
        {
        case WTS_CONSOLE_CONNECT:
            OutputDebugStringA("Logon!\n");
            Os_app_session_disconnect = false;
            break;
        case WTS_CONSOLE_DISCONNECT:
            OutputDebugStringA("Logoff!\n");
            Os_app_session_disconnect = true;
            break;
        case WTS_SESSION_LOCK:
            OutputDebugStringA("Lock!\n");
            Os_app_session_lock = true;
            break;
        case WTS_SESSION_UNLOCK:
            OutputDebugStringA("Unlock!\n");
            Os_app_session_lock = false;
            break;
        default:
            return 0;
        }
        os_handle_suspend(hwnd, was_suspended);
        return 1;
    default:
        return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

// 0x005AD830 (:371)
bool os_create_window(int w, int h, bool fullscreen, const wchar_t* const device_name)
{
    static bool already_created = false; // 0x01D584B0
    if (already_created)
    {
        return true;
    }
    WNDCLASSEXW wc;
    memset(&wc, 0, sizeof(wc));
    HINSTANCE instance = GetModuleHandleW(nullptr);
    wc.lpszClassName = Os_app_class;
    wc.lpfnWndProc = win32_message_handler;
    wc.style = CS_VREDRAW | CS_HREDRAW | CS_OWNDC; // 0x23
    wc.cbSize = sizeof(wc);
    wc.hInstance = instance;
    // resource 101: the game icon, 32x32 and 16x16, LR_DEFAULTSIZE
    wc.hIcon = (HICON)LoadImageW(instance, MAKEINTRESOURCEW(101), IMAGE_ICON, 32, 32, LR_DEFAULTSIZE);
    wc.hIconSm = (HICON)LoadImageW(instance, MAKEINTRESOURCEW(101), IMAGE_ICON, 16, 16, LR_DEFAULTSIZE);
    wc.hCursor = nullptr;
    wc.lpszMenuName = nullptr;
    wc.cbClsExtra = 0;
    wc.cbWndExtra = 0;
    ATOM atom = RegisterClassExW(&wc);
    if (atom == 0)
    {
        return false;
    }
    Os_app_fullscreen = fullscreen;
    DWORD style = os_get_window_style(fullscreen);
    DEVMODEW mode;
    memset(&mode, 0, sizeof(mode));
    mode.dmSize = sizeof(mode);
    EnumDisplaySettingsW(device_name, ENUM_REGISTRY_SETTINGS, &mode);
    // centred on the display, clamped to its top left corner
    RECT rect;
    rect.left = (int)mode.dmPelsWidth / 2 - w / 2;
    rect.top = (int)mode.dmPelsHeight / 2 - h / 2;
    rect.bottom = rect.top + h;
    rect.right = rect.left + w;
    AdjustWindowRectEx(&rect, style, FALSE, 0);
    Os_app_hwnd = CreateWindowExW(0, Os_app_class, Os_app_title, style, rect.left < 0 ? 0 : rect.left,
                                  rect.top < 0 ? 0 : rect.top, rect.right - rect.left, rect.bottom - rect.top, nullptr,
                                  nullptr, instance, nullptr);
    ShowCursor(FALSE);
    ClipCursor(nullptr);
    ShowWindow(Os_app_hwnd, SW_SHOW);
    SetActiveWindow(Os_app_hwnd);
    SetForegroundWindow(Os_app_hwnd);
    WTSRegisterSessionNotification(Os_app_hwnd, NOTIFY_FOR_THIS_SESSION);
    already_created = true;
    return true;
}

// 0x0058B420 (:619)
void os_sleep(int time_milliseconds)
{
    Sleep(time_milliseconds);
}

// 0x0058B430 (:624)
void os_micro_sleep(int time_microseconds)
{
    Sleep((unsigned int)time_microseconds / 1000);
}

// 0x0058B440 (:637)
unsigned int os_get_window()
{
    return (unsigned int)Os_app_hwnd;
}

// 0x005ADA70 (:649)
bool os_foreground()
{
    return Os_app_active;
}

// 0x0058B450 (:654)
bool os_minimized()
{
    return false;
}

// 0x0058B460 (:670)
void os_hook_message(bool (*function)(unsigned int, unsigned int, unsigned int))
{
    // original: no check against the 32 slots
    Os_hooks[Os_num_hooks].function = function;
    Os_num_hooks++;
}

// 0x0058B480 (:681)
void os_unhook_message(bool (*function)(unsigned int, unsigned int, unsigned int))
{
    for (int i = Os_num_hooks; i > 0; i--)
    {
        if (Os_hooks[i - 1].function == function)
        {
            // original bug kept: moves Os_num_hooks - i bytes, not hooks (4 bytes each)
            memmove(&Os_hooks[i - 1], &Os_hooks[i], Os_num_hooks - i);
            Os_num_hooks--;
            return;
        }
    }
}

// 0x0058B4D0 (:696)
void os_set_title(const wchar_t* title)
{
    wcscpy(Os_app_title, title);
    SetWindowTextW(Os_app_hwnd, Os_app_title);
}

// 0x0058B510 (:708)
void os_poll()
{
}

// 0x0058B520 (:713)
void os_close()
{
    PostMessageW(Os_app_hwnd, WM_DESTROY, 0, 0);
}

// 0x0058B540 (:725)
void os_get_date(char* out, unsigned int capacity)
{
    int64_t now = keen::getCurrentCalendarTime();
    keen::CalendarDateTime date;
    if (!keen::fillLocalCalendarTime(&date, now) && !keen::fillUTCCalendarTime(&date, now))
    {
        *out = '\0';
        return;
    }
    // MM/DD/YY
    keen::formatString(out, capacity, "%02d/%02d/%02d", date.month + 1, date.dayOfMonth, date.year % 100);
}

// 0x0058B5F0 (:744)
void os_get_time(char* out, unsigned int capacity)
{
    int64_t now = keen::getCurrentCalendarTime();
    keen::CalendarDateTime date;
    if (!keen::fillLocalCalendarTime(&date, now) && !keen::fillUTCCalendarTime(&date, now))
    {
        *out = '\0';
        return;
    }
    keen::formatString(out, capacity, "%02d:%02d:%02d", date.hour, date.minutes, date.seconds);
}
