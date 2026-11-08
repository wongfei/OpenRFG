// Original: library/modules/runtime/base/include/keen/base/mutex.hpp
#pragma once

#include "keen/base/win32/mutex_win32.hpp"

namespace keen
{
    // Scoped lock.
    // size 0x4
    class MutexLock
    {
    public:
        MutexLock(Mutex* pMutex); // 0x00458A80
        MutexLock();
        ~MutexLock(); // 0x00458AA0
        void lock(Mutex*);
        void unlock();

    private:
        Mutex* m_pMutex; // 0x00
    };
}
