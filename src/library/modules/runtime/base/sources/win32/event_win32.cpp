// Original: library/modules/runtime/base/sources/win32/event_win32.cpp
//
// keen::Event over a Win32 auto-reset event. The OS name is "<debug name>@<this>:<process id>".

#include "keen/base/event.hpp"
#include "keen/base/string.hpp"
#include "keen/base/stringencoding.hpp"

#include <windows.h>

namespace keen
{
    // 0x00BED5A0
    Event::Event()
    {
        m_eventHandle = nullptr;
    }

    // 0x00BED5B0
    Event::~Event()
    {
    }

    // 0x00C07370
    void Event::create(const char* pDebugname, uint32 initialValue)
    {
        char name[128];
        wchar_t wideName[260];
        formatString(name, sizeof(name), "%s@%x:%x", pDebugname, this, GetCurrentProcessId());
        convertUTF8ToUTF16((uint16*)wideName, sizeof(wideName), name);
        m_eventHandle = CreateEventW(nullptr, FALSE, initialValue != 0, wideName);
    }

    // 0x00BF42B0
    void Event::destroy()
    {
        SetEvent(m_eventHandle);
        CloseHandle(m_eventHandle);
        m_eventHandle = nullptr;
    }

    // 0x00BED5C0
    void Event::sleepUntilSignaled()
    {
        if (m_eventHandle != nullptr)
        {
            WaitForSingleObject(m_eventHandle, INFINITE);
        }
    }

    // 0x00BED5D0
    bool Event::sleepUntilSignaledTimeOut(uint32 timeOutInMS)
    {
        if (m_eventHandle == nullptr)
        {
            return false;
        }
        return WaitForSingleObject(m_eventHandle, timeOutInMS) == WAIT_OBJECT_0;
    }

    // 0x00BED5F0
    void Event::signal()
    {
        SetEvent(m_eventHandle);
    }

    // 0x00BED600
    void Event::reset()
    {
        ResetEvent(m_eventHandle);
    }
}
