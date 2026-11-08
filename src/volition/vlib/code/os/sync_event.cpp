// Original: volition/vlib/code/os/sync_event.cpp
//
// Both classes wrap a keen::Event (an auto-reset Win32 event). must_manual_reset is ignored, so a sync_event is
// always auto-reset; signal/reset/wait call the Win32 functions on the handle directly (keen::Event::signal,
// reset and sleepUntilSignaled, inlined).

#include "volition/vlib/code/os/sync_event.h"

// 0x0058C030 (:30)
sync_event::sync_event(bool must_manual_reset, bool initial_statep)
{
    keen_event.create("sync_event", (unsigned int)initial_statep);
}

// 0x0058C060 (:41)
sync_event::~sync_event()
{
    keen_event.destroy();
}

// 0x0058C070 (:48)
void sync_event::signal()
{
    keen_event.signal();
}

// 0x0058C080 (:55)
void sync_event::reset()
{
    keen_event.reset();
}

// 0x0058C090 (:62)
void sync_event::wait()
{
    keen_event.sleepUntilSignaled();
}

// 0x0058C0A0 (:73)
semaphore_event::semaphore_event()
{
    keen_event.create("semaphore_event", 0);
}

// 0x0058C0C0 (:78)
semaphore_event::~semaphore_event()
{
    keen_event.destroy();
}

// 0x0058C0D0 (:83)
void semaphore_event::signal()
{
    keen_event.signal();
}

// 0x0058C0E0 (:88)
void semaphore_event::wait()
{
    keen_event.sleepUntilSignaled();
}

// 0x0058C0F0 (:93)
void semaphore_event::reset()
{
    keen_event.reset();
}
