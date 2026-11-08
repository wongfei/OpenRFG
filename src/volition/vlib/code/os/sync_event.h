// Original: volition/vlib/code/os/sync_event.h
// Implementation: volition/vlib/code/os/sync_event.cpp
//
// Thin wrappers over keen::Event. Placement of semaphore_event inferred (implemented in sync_event.cpp).
#pragma once

#include "keen/base/event.hpp"

// size 0x4
class sync_event
{
public:
    sync_event(bool must_manual_reset, bool initial_statep); // 0x0058C030 (:30)
    ~sync_event(); // 0x0058C060 (:41)
    void signal(); // 0x0058C070 (:48)
    void reset(); // 0x0058C080 (:55)
    void wait(); // 0x0058C090 (:62)

private:
    keen::Event keen_event; // 0x00
};

// size 0x4
class semaphore_event
{
public:
    semaphore_event(); // 0x0058C0A0 (:73)
    ~semaphore_event(); // 0x0058C0C0 (:78)
    void signal(); // 0x0058C0D0 (:83)
    void reset(); // 0x0058C0F0 (:93)
    void wait(); // 0x0058C0E0 (:88)

private:
    keen::Event keen_event; // 0x00
};
