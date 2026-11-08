// Original: library/modules/runtime/base/sources/win32/semaphore_win32.cpp
//
// keen::Semaphore over a named Win32 semaphore; m_value mirrors the count for debugging only.

#include "keen/base/semaphore.hpp"
#include "keen/base/string.hpp"
#include "keen/base/stringencoding.hpp"

#include <windows.h>

namespace keen
{
    // 0x00BEDBB0
    Semaphore::Semaphore()
    {
        m_semaphoreHandle = nullptr;
        m_value = 0;
    }

    // 0x00BEDBC0
    Semaphore::~Semaphore()
    {
    }

    // 0x00C074E0
    void Semaphore::create(const char* pDebugname, int initialValue, int maxValue)
    {
        char name[128];
        wchar_t wideName[260];
        formatString(name, sizeof(name), "%s@%x:%x", pDebugname, this, GetCurrentProcessId());
        convertUTF8ToUTF16((uint16*)wideName, sizeof(wideName), name);
        m_semaphoreHandle = CreateSemaphoreW(nullptr, initialValue, maxValue, wideName);
        m_value = initialValue;
    }

    // 0x00BEDBD0
    void Semaphore::destroy()
    {
        CloseHandle(m_semaphoreHandle);
        m_semaphoreHandle = nullptr;
    }

    // 0x00BEDC20
    void Semaphore::incrementValue(int value)
    {
        ReleaseSemaphore(m_semaphoreHandle, value, nullptr);
        InterlockedExchangeAdd((volatile LONG*)&m_value, value);
    }

    // 0x00BEDBF0
    bool Semaphore::tryDecrementValue(uint32 timeOutInMS)
    {
        if (WaitForSingleObject(m_semaphoreHandle, timeOutInMS) != WAIT_OBJECT_0)
        {
            return false;
        }
        InterlockedDecrement((volatile LONG*)&m_value);
        return true;
    }
}
