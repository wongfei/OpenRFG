// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/debugutil/multithreadcheck/hkmultithreadcheck.cpp
//
// Havok 2014.2.5 (havok_impl.md HK1): the multithreading checks. The shipped build compiles the marking and access
// checks to nothing (the inline wrappers of hkmultithreadcheck.h are empty); only the enable state is kept.
// Not reimplemented: the *Impl marking functions (never called in the shipped build) and the stack trace tree of
// staticInit/staticQuit (hkStackTracer, S phase).

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/debugutil/multithreadcheck/hkmultithreadcheck.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/router/hkmemoryrouter.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/thread/criticalsection/hkcriticalsection.h"

#include <new>

// ---- globals
hkCriticalSection* hkMultiThreadCheck::s_criticalSection; // 0x03A27010

// 0x00CE7430 (.h:208)
// Unmarked, checking disabled (the top bit of the mark count).
hkMultiThreadCheck::hkMultiThreadCheck()
{
    m_threadId = (unsigned int)UNMARKED;
    m_markCount = 0x8000;
}

// 0x00CFE440 (.h:127)
bool hkMultiThreadCheck::isCheckingEnabled() const
{
    return (m_markCount & 0x8000) == 0 && s_criticalSection != nullptr;
}

// 0x00DF8D10 (.h:68)
void hkMultiThreadCheck::markForRead(ReadMode mode)
{
}

// 0x00DB0F60 (.h:76)
void hkMultiThreadCheck::markForWrite()
{
}

// 0x00DE8B00 (.h:82)
bool hkMultiThreadCheck::isMarkedForWrite()
{
    return true;
}

// 0x00DE8B10 (.h:91)
bool hkMultiThreadCheck::isMarkedForReadRecursive()
{
    return true;
}

// 0x00DF8D20 (.h:103)
void hkMultiThreadCheck::unmarkForRead()
{
}

// 0x00DB0F70 (.h:111)
void hkMultiThreadCheck::unmarkForWrite()
{
}

// 0x00CFE610 (:235)
void hkMultiThreadCheck::accessCheck(AccessType type) const
{
}

// 0x00CFE620 (:258)
void hkMultiThreadCheck::accessCheckWithParent(const hkMultiThreadCheck* parentLock, AccessType parentType, const hkMultiThreadCheck& lock, AccessType type)
{
}

// 0x00CFE630 (:317)
void hkMultiThreadCheck::disableChecks()
{
    m_markCount |= 0x8000;
}

// 0x00CFE640 (:322)
void hkMultiThreadCheck::enableChecks()
{
    if ((m_markCount & 0x8000) != 0 || s_criticalSection == nullptr)
    {
        m_threadId = (unsigned int)UNMARKED;
        m_markCount = 0;
    }
}

// 0x00CFE670 (:332)
void hkMultiThreadCheck::reenableChecks()
{
    m_markCount &= 0x7FFF;
}

// 0x00CFE680 (:337)
void hkMultiThreadCheck::globalCriticalSectionLock()
{
}

// 0x00CFE690 (:347)
void hkMultiThreadCheck::globalCriticalSectionUnlock()
{
}

// 0x00CFE6A0 (:21)
void hkMultiThreadCheck::staticInit(hkMemoryAllocator* memoryAllocator)
{
    void* p = hkMemoryRouter::getInstance().heap().blockAlloc(sizeof(hkCriticalSection));
    s_criticalSection = p != nullptr ? new (p) hkCriticalSection(1000) : nullptr; // 1000: spin count
}

// 0x00CFE720 (:31)
void hkMultiThreadCheck::staticQuit()
{
    hkCriticalSection* section = s_criticalSection;
    if (section != nullptr)
    {
        section->~hkCriticalSection();
        hkMemoryRouter::getInstance().heap().blockFree(section, sizeof(hkCriticalSection));
        s_criticalSection = nullptr;
    }
}
