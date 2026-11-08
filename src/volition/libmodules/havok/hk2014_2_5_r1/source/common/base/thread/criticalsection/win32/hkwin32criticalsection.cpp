// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/thread/criticalsection/win32/hkwin32criticalsection.cpp
//
// Havok 2014.2.5 (havok_impl.md HK1): the Win32 critical section (an RTL_CRITICAL_SECTION in place), with the inline
// functions of hkwin32criticalsection.inl and hkcriticalsection.h that have out-of-line copies.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/thread/criticalsection/hkcriticalsection.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

static_assert(sizeof(CRITICAL_SECTION) == sizeof(((hkCriticalSection*)nullptr)->m_section), "hkCriticalSection holds an RTL_CRITICAL_SECTION");

// Not reimplemented: hkHardwareInfo::calcNumHardwareThreads (S phase) reads the processor count the same way.
static int calcNumHardwareThreads()
{
    SYSTEM_INFO info;
    GetSystemInfo(&info);
    return (int)info.dwNumberOfProcessors;
}

// 0x00CFE910 (:23)
// A spin count of 0 means 1000 per hardware thread.
hkCriticalSection::hkCriticalSection(int spinCount)
{
    if (spinCount == 0)
    {
        spinCount = calcNumHardwareThreads() * 1000;
    }
    InitializeCriticalSectionAndSpinCount((CRITICAL_SECTION*)m_section, spinCount);
}

// 0x00CE7470 (hkwin32criticalsection.inl:17)
hkCriticalSection::~hkCriticalSection()
{
    DeleteCriticalSection((CRITICAL_SECTION*)m_section);
}

// 0x00CE7480 (hkwin32criticalsection.inl:36)
void hkCriticalSection::enter()
{
    EnterCriticalSection((CRITICAL_SECTION*)m_section);
}

// 0x00CE7490 (hkwin32criticalsection.inl:41)
void hkCriticalSection::leave()
{
    LeaveCriticalSection((CRITICAL_SECTION*)m_section);
}

// 0x00CFE310 (hkwin32criticalsection.inl:59)
unsigned int hkCriticalSection::atomicExchangeAdd(unsigned int* var, int increment)
{
    return (unsigned int)InterlockedExchangeAdd((volatile LONG*)var, increment);
}
