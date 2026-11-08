// Original: volition/rfg/code/havok/havok_memory.h
// Implementation: volition/rfg/code/havok/havok_memory.cpp
//
// Havok memory setup: the memory router and large block allocator, a listener that reports allocation failures, and
// the world memory watch dog: when Havok runs out of memory it marks the memory system as warning and removes up to
// 50 objects from the simulation islands, from HAVOK_AUTO_REMOVE_FIRST downwards (havok_auto_remove_level), then waits
// 500 ms before searching again (decompiled). Thread memory routers are created per worker thread.
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/allocator/largeblock/hklargeblockallocator.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/memory/hkpworldmemoryavailablewatchdog.h"
#include "volition/vlib/code/time/timestamp.h"

class hkMemoryRouter;
class hkpSimulationIsland;
class hkpWorld;

// size 0x4
class havok_memory_listener : public hkLargeBlockAllocator::LimitedMemoryListener
{
    virtual void cannotAllocate(unsigned int size); // vtbl+0x04 0x0083C120 (:368)
    virtual void allocationFailure(unsigned int size); // vtbl+0x08 0x0083C1C0 (:414)
public:
    havok_memory_listener(); // 0x0081C610
    virtual ~havok_memory_listener(); // vtbl+0x00 0x0081C620

};

// size 0xC
class havok_memory_watch_dog : public hkWorldMemoryAvailableWatchDog
{
public:
    havok_memory_watch_dog(); // 0x00811A30 (:442)
    virtual int getAmountOfFreeHeapMemoryRequested(); // vtbl+0x10 0x00811A60 (:448)
    virtual void freeHeapMemoryTillRequestedAmountIsAvailable(hkpWorld* world); // vtbl+0x14 0x0083C2B0 (:454)
    virtual void reduceConstraintsInIsland(const hkWorldMemoryAvailableWatchDog::MemUsageInfo& info, int numIterations); // vtbl+0x18 0x0081C4E0 (:521)
    virtual ~havok_memory_watch_dog(); // vtbl+0x00 0x00811A50

private:
    timestamp search_delay; // 0x08
};

void havok_memory_watch_dog_do_frame(hkpWorld* world); // 0x0083D840 (:529)
void havok_memory_watch_dog_free_memory(hkpWorld* world); // 0x00811A70 (:568)
void havok_memory_watch_dog_free_memory(hkpWorld* world, hkpSimulationIsland* island, int desired_remove_count); // 0x0081C510 (:575)
void havok_memory_init(); // 0x00824B70 (:599)
extern unsigned int Havok_mem_largest_runtime_block; // 0x02B8229C, largest solver buffer a simulation island may need
void havok_memory_shutdown(); // 0x00824DD0 (:696)
void havok_memory_level_init(); // 0x00811AA0 (:731)
void havok_memory_level_shutdown(); // 0x00811AB0 (:740)
unsigned int havok_memory_get_largest_block(); // 0x0083C480 (:750)
hkMemoryRouter* havok_create_thread_memory_router(); // 0x0081C640 (:771)
void havok_destroy_thread_memory_router(hkMemoryRouter* router); // 0x0082B080 (:789)
