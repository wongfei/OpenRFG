// Original: library/modules/runtime/base/sources/win32/mutex_win32.cpp
//
// keen::Mutex over a Win32 CRITICAL_SECTION stored inline in m_criticalSectionData.

#include "keen/base/mutex.hpp"
#include "keen/base/string.hpp"
#include "keen/base/thread.hpp"

#include <windows.h>

static_assert(sizeof(CRITICAL_SECTION) == sizeof(uint32) * keen::Mutex::CriticalSectionDataSize, "CRITICAL_SECTION size");

namespace keen
{
    // 0x00BED800
    Mutex::Mutex()
    {
    }

    // 0x00BED810
    Mutex::~Mutex()
    {
    }

    // 0x00C07410
    void Mutex::create(const char* pDebugName)
    {
        formatString(m_name, sizeof(m_name), "%s@%x:%x", pDebugName, this, GetCurrentProcessId());
        InitializeCriticalSection((CRITICAL_SECTION*)m_criticalSectionData);
    }

    // 0x00BED820
    void Mutex::destroy()
    {
        DeleteCriticalSection((CRITICAL_SECTION*)m_criticalSectionData);
    }

    // 0x00BFC360
    bool Mutex::lock(uint32 timeOut) const
    {
        CRITICAL_SECTION* pCriticalSection = (CRITICAL_SECTION*)m_criticalSectionData;
        if (timeOut == 0)
        {
            EnterCriticalSection(pCriticalSection);
            return true;
        }

        // The time out is in milliseconds, measured with the performance counter.
        LARGE_INTEGER frequency;
        LARGE_INTEGER counter;
        QueryPerformanceFrequency(&frequency);
        QueryPerformanceCounter(&counter);
        const uint32 startTime = (uint32)((counter.QuadPart * 1000) / frequency.QuadPart);
        while (!TryEnterCriticalSection(pCriticalSection))
        {
            Sleep(0);
            QueryPerformanceFrequency(&frequency);
            QueryPerformanceCounter(&counter);
            const uint32 time = (uint32)((counter.QuadPart * 1000) / frequency.QuadPart);
            if (time - startTime >= timeOut)
            {
                return false;
            }
        }
        return true;
    }

    // 0x00BED830
    bool Mutex::tryLock() const
    {
        return TryEnterCriticalSection((CRITICAL_SECTION*)m_criticalSectionData) == TRUE;
    }

    // 0x00BED850
    void Mutex::unlock() const
    {
        LeaveCriticalSection((CRITICAL_SECTION*)m_criticalSectionData);
    }

    // 0x00458A80
    MutexLock::MutexLock(Mutex* pMutex)
    {
        m_pMutex = pMutex;
        pMutex->lock(0);
    }

    // 0x00458AA0
    MutexLock::~MutexLock()
    {
        if (m_pMutex != nullptr)
        {
            m_pMutex->unlock();
        }
    }
}
