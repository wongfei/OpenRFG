// The stand-in trace (stub_impl.md S0; new code, not recovered): every stand-in records its calls, and a program can
// write the stand-ins it reached to a file (OpenRFG --stub-trace FILE). tools/headless/stub_inventory.py --trace reads
// the file and marks the entries reached.
//
// Each stand-in's first statement is GAME_STUB_HIT("<name>"): the generated ones name their linker signature (their
// "// sym:" line), the hand-written ones their qualified name. The record is a static entry per stand-in, pushed onto a
// lock-free list at its first call; later calls only count. With a trace file open, the first call of each stand-in
// writes a "hit <name>" line at once (flushed, so a crash keeps what was reached), and the program's exit writes a
// "calls <n> <name>" line per stand-in reached.
//
// Header-only (C++17 inline variables): every stand-in file includes it, also those of the programs that link only
// one scope of src/stubs (om2 links only the framework scope).
#pragma once

#include <intrin.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct stub_trace_entry
{
    const char* name;
    volatile long calls;
    stub_trace_entry* next;
    bool written; // its "hit" line is in the trace file (under Stub_trace_lock)
};

inline stub_trace_entry* volatile Stub_trace_head; // the stand-ins reached, last first
inline FILE* Stub_trace_file;                      // set by stub_trace_open
inline volatile long Stub_trace_lock;
inline void (*Stub_trace_hit_hook)(const char* name); // at each stand-in's first call (the game trace, game_trace.h)

inline void stub_trace_lock()
{
    while (_InterlockedExchange(&Stub_trace_lock, 1) != 0)
    {
        _mm_pause();
    }
}

inline void stub_trace_unlock()
{
    _InterlockedExchange(&Stub_trace_lock, 0);
}

// Writes an entry's "hit" line once (under the lock).
inline void stub_trace_write_hit(stub_trace_entry* entry)
{
    if (Stub_trace_file != nullptr && !entry->written)
    {
        entry->written = true;
        fprintf(Stub_trace_file, "hit %s\n", entry->name);
        fflush(Stub_trace_file);
    }
}

inline void stub_trace_hit(stub_trace_entry* entry)
{
    if (_InterlockedIncrement(&entry->calls) != 1)
    {
        return;
    }
    if (Stub_trace_hit_hook != nullptr)
    {
        Stub_trace_hit_hook(entry->name);
    }
    stub_trace_entry* head;
    do
    {
        head = Stub_trace_head;
        entry->next = head;
    } while (_InterlockedCompareExchangePointer((void* volatile*)&Stub_trace_head, entry, head) != head);
    if (Stub_trace_file != nullptr)
    {
        stub_trace_lock();
        stub_trace_write_hit(entry);
        stub_trace_unlock();
    }
}

inline void stub_trace_write_calls()
{
    stub_trace_lock();
    if (Stub_trace_file != nullptr)
    {
        for (stub_trace_entry* e = Stub_trace_head; e != nullptr; e = e->next)
        {
            fprintf(Stub_trace_file, "calls %ld %s\n", e->calls, e->name);
        }
        fflush(Stub_trace_file);
    }
    stub_trace_unlock();
}

// The calls of a stand-in so far, by its GAME_STUB_HIT name (0 when not reached): the test programs' count of the
// stand-ins they share with OpenRFG, which keep no counters of their own (stub_impl.md S1).
inline long stub_trace_calls(const char* name)
{
    for (stub_trace_entry* e = Stub_trace_head; e != nullptr; e = e->next)
    {
        if (strcmp(e->name, name) == 0)
        {
            return e->calls;
        }
    }
    return 0;
}

// Opens the trace file (false if it cannot be written): the stand-ins reached so far are written at once, the calls
// at exit (atexit).
inline bool stub_trace_open(const char* path)
{
    FILE* file = nullptr;
    if (fopen_s(&file, path, "w") != 0 || file == nullptr)
    {
        return false;
    }
    stub_trace_lock();
    Stub_trace_file = file;
    for (stub_trace_entry* e = Stub_trace_head; e != nullptr; e = e->next)
    {
        stub_trace_write_hit(e);
    }
    stub_trace_unlock();
    atexit(stub_trace_write_calls);
    return true;
}

// The first statement of a stand-in. The entry is constant-initialized (no guard, no static initialization order).
#define GAME_STUB_HIT(name) \
    do \
    { \
        static stub_trace_entry stub_trace_entry_ = {name, 0, nullptr, false}; \
        stub_trace_hit(&stub_trace_entry_); \
    } while (0)
