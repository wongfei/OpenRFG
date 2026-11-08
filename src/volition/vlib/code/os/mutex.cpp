// Original: volition/vlib/code/os/mutex.cpp
//
// mutex is a keen::Mutex named "rfg_mutex". A non-blocking lock tries once and does not report whether it got the
// lock (decompiled behaviour, kept as is).

#include "volition/vlib/code/os/mutex.h"

// 0x0058AFA0 (:6)
mutex::mutex()
{
    keen_mutex.create("rfg_mutex");
}

// 0x0058AFC0 (:13)
mutex::~mutex()
{
    keen_mutex.destroy();
}

// 0x0058AFD0 (:20)
void mutex::lock(bool blocking)
{
    if (blocking)
    {
        keen_mutex.lock(0);
        return;
    }
    keen_mutex.tryLock();
}

// 0x0058AFF0 (:31)
void mutex::unlock()
{
    keen_mutex.unlock();
}
