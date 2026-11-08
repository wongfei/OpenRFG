// Original: library/modules/runtime/base/include/keen/base/win32/mutex_win32.hpp
// Implementation: library/modules/runtime/base/sources/win32/mutex_win32.cpp
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    // CRITICAL_SECTION stored inline (24 bytes on x86) so the header does not need windows.h.
    // size 0x38
    class Mutex
    {
    public:
        enum
        {
            CriticalSectionDataSize = 6,
        };

        Mutex(); // 0x00BED800
        ~Mutex(); // 0x00BED810
        void create(const char* pDebugName); // 0x00C07410
        void destroy(); // 0x00BED820
        bool lock(uint32 timeOut) const; // 0x00BFC360
        bool tryLock() const; // 0x00BED830
        void unlock() const; // 0x00BED850

    private:
        char m_name[32]; // 0x00
        uint32 m_criticalSectionData[6]; // 0x20
    };
}
