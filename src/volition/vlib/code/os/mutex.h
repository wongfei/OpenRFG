// Original: volition/vlib/code/os/mutex.h
// Implementation: volition/vlib/code/os/mutex.cpp
//
// Volition mutex on top of keen::Mutex, plus a scoped lock.
#pragma once

#include "keen/base/mutex.hpp"

// size 0x38
class mutex
{
public:
    mutex(); // 0x0058AFA0 (:6)
    ~mutex(); // 0x0058AFC0 (:13)
    void lock(bool blocking); // 0x0058AFD0 (:20)
    void unlock(); // 0x0058AFF0 (:31)

private:
    keen::Mutex keen_mutex; // 0x00
};

// size 0x4
class mutex_lock
{
public:
    mutex_lock(mutex& the_mutex, bool blocking); // 0x0080D2C0 (.h:48)
protected:
    mutex_lock(); // declared, never emitted
public:
    ~mutex_lock(); // 0x0080D2E0 (.h:49)

private:
    mutex* m_mutex; // 0x00
};
