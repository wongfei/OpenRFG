// Original: library/modules/runtime/base/include/keen/base/win32/event_win32.hpp
// Implementation: library/modules/runtime/base/sources/win32/event_win32.cpp
#pragma once

#include "keen/base/types.hpp"

namespace keen
{
    // Win32 event object (CreateEvent).
    // size 0x4
    class Event
    {
    public:
        enum
        {
            TimeOutInfinite = -1,
        };

        Event(); // 0x00BED5A0
        ~Event(); // 0x00BED5B0
        void create(const char* pDebugname, uint32 initialValue); // 0x00C07370
        void destroy(); // 0x00BF42B0
        void sleepUntilSignaled(); // 0x00BED5C0
        bool sleepUntilSignaledTimeOut(uint32 timeOutInMS); // 0x00BED5D0
        void signal(); // 0x00BED5F0
        void reset(); // 0x00BED600

    private:
        void* m_eventHandle; // 0x00
    };
}
