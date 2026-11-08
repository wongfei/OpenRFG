// Original: volition/rfg/code/havok/havok_memory.cpp
//
// Havok memory (decompiled). havok_memory_init builds the Havok memory system in static storage: a large block
// allocator over all of Havok_mempool, a free list allocator on top of it and a free list memory system, with a heap
// soft limit 2 MB below the pool size, then starts the Havok base system with the error handler. When the large block
// allocator cannot allocate, havok_memory_listener marks the memory system as erroring and frees game objects
// (broadphase objects, load balanced debris, ragdolls). The world's memory watch dog removes up to 50 objects per
// allocator from the simulation islands, highest auto remove level first, and is also run each frame when less than
// 512 KB are left in one block. Worker threads get their memory router in thread-local storage.
// Compiled in rfg_havok_unity.cpp.
//
// Notes (decompiled behaviour, kept as is):
// - havok_mem_display_stats and havok_memory_listener::allocationFailure format their report into a local buffer
//   that is never printed (the print was compiled out of the retail build).
// - watch_dog_remove_objects_from_island and havok_memory_watch_dog_free_memory destroy World's null object when an
//   entity has no object property (world::destroy_object ignores it); either way the entity counts as removed.
// - freeHeapMemoryTillRequestedAmountIsAvailable runs its removal loop per memory statistics entry, with a remove
//   level from that entry's total available memory, and counts entries in a float.
// - havok_memory_init rounds Havok_mem_largest_runtime_block up to the largest of its steps first, so only 0xFD800
//   is ever chosen and larger blocks stay as they are.
// - havok_memory_shutdown destroys the listener with its deleting destructor, which does not free (the listener lives
//   in Havok_memory_listener_local).

#include "volition/rfg/code/havok/havok_memory.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/allocator/freelist/hkfreelistallocator.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/router/hkmemoryrouter.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/system/freelist/hkfreelistmemorysystem.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/system/hkmemorysystem.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/error/hkdefaulterror.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/hkbasesystem.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkpentity.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/hkpsimulationisland.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/hkpworld.h"
#include "volition/rfg/code/destruction/rfg_load_balance.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/havok/havok_bp.h"
#include "volition/rfg/code/havok/havok_ragdoll.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/rfg.h"
#include "volition/vlib/code/memory/vmemory.h"
#include "volition/vlib/code/os/frametime.h"
#include "volition/vlib/code/util/string_util.h"

#include "keen/base/string.hpp"
#include "keen/base/trace.hpp"
#include "keen/debugrenderer/debugrenderer.hpp"

#include <new>

namespace
{
    // size 0x5A90
    // Static storage of the three Havok memory objects (16-byte aligned blocks, as in the Havok demos).
    struct FreeList
    {
        alignas(16) unsigned char m_largeBlock[220]; // 0x00, hkLargeBlockAllocator
        alignas(16) unsigned char m_allocator[2752]; // 0xE0, hkFreeListAllocator
        alignas(16) unsigned char m_system[20204]; // 0xBA0, hkFreeListMemorySystem
    };
    static_assert(sizeof(FreeList) == 0x5A90, "FreeList size");
    static_assert(sizeof(hkLargeBlockAllocator) == 220, "hkLargeBlockAllocator size");
    static_assert(sizeof(hkFreeListAllocator) == 2752, "hkFreeListAllocator size");
    static_assert(sizeof(hkFreeListMemorySystem) == 20204, "hkFreeListMemorySystem size");

    FreeList s_buffer; // 0x02767820
    hkMemorySystem* s_system; // 0x02B82298
    void (*s_onQuitFunc)(); // 0x02B82294
    hkMemoryAllocator* s_freeListAllocator; // 0x02B82290
    hkMemoryAllocator* s_largeBlockAllocator; // 0x02B8228C

    // 0x008119D0 (:142)
    void onQuit()
    {
        if (s_freeListAllocator != nullptr)
        {
            s_freeListAllocator->~hkMemoryAllocator(); // in place: destructed, not freed
            s_freeListAllocator = nullptr;
        }
        if (s_largeBlockAllocator != nullptr)
        {
            s_largeBlockAllocator->~hkMemoryAllocator();
            s_largeBlockAllocator = nullptr;
        }
    }
}

// ---- globals
static hkMemorySystem* Havok_memory; // 0x02B822A0
static hkWorldMemoryAvailableWatchDog* Havok_memory_watch_dog; // 0x02B822A8
static havok_memory_listener* Havok_memory_listener; // 0x02B822A4
alignas(4) static unsigned char Havok_memory_listener_local[4]; // 0x02642F90, the havok_memory_listener
static hkDefaultError* Havok_error_handler; // 0x02B822AC
alignas(16) static unsigned char Havok_error_handler_memory[68]; // 0x026677D0, the hkDefaultError
static bool s_showHavokMemory; // 0x02B82288
static int Garbage_collect_framecount = -1; // 0x0165FDEC, frame of the last watch dog removal (-1: none pending)
unsigned int Havok_mem_largest_runtime_block; // 0x02B8229C

// Memory router of each worker thread (havok_create_thread_memory_router).
alignas(16) static __declspec(thread) unsigned char thread_router[64]; // TLS +0x860, an hkMemoryRouter

// 0x00811A10 (:210)
static void havok_report_function(const char* message, void* report_object)
{
}

// 0x0081C3E0 (:223)
static unsigned int get_largest_block(const hkMemoryAllocator::MemoryStatistics& stats)
{
    unsigned int largest = (unsigned int)stats.m_available;
    if (largest < (unsigned int)stats.m_largestBlock)
    {
        largest = (unsigned int)stats.m_largestBlock;
    }
    return largest;
}

// 0x0083C000 (:275)
static void havok_mem_display_stats(const char* label)
{
    char buffer[512];
    hkMemorySystem::MemoryStatistics stats;
    hkMemorySystem::getInstance().getMemoryStatistics(stats);
    for (int i = 0; i < stats.m_entries.getSize(); i++)
    {
        const hkMemorySystem::MemoryStatistics::Entry& entry = stats.m_entries[i];
        long largest = entry.m_allocatorStats.m_available;
        if (largest < entry.m_allocatorStats.m_largestBlock)
        {
            largest = entry.m_allocatorStats.m_largestBlock;
        }
        keen::formatString(buffer, sizeof(buffer), "%30s - (%s) used: %s free: %s largest block: %s\n",
                           entry.m_allocatorName.cString(), label, str_commas((uint64_t)entry.m_allocatorStats.m_inUse),
                           str_commas((uint64_t)entry.m_allocatorStats.m_totalAvailable), str_commas((unsigned int)largest));
    }
}

// 0x00811A20 (:290)
static void havok_memory_cleanup()
{
}

// 0x0083C0E0 (:297)
static void havok_reclaim_memory()
{
    havok_bpo_emergency_cleanup();
    havok_mem_display_stats("bpo");
    rfgl_emergency_cleanup();
    havok_mem_display_stats("rfgl");
    havok_ragdoll_emergency_cleanup();
    havok_mem_display_stats("ragdoll");
    havok_mem_display_stats("profile");
}

// The object of an entity (property 0), or null.
static object* watch_dog_entity_object(hkpEntity* entity)
{
    for (int i = 0; i < entity->m_properties.getSize(); i++)
    {
        if (entity->m_properties[i].m_key == HAVOK_PROPERTY_OBJECT)
        {
            return (object*)entity->m_properties[i].m_value.m_data;
        }
    }
    return nullptr;
}

// 0x0081C3F0 (:325)
// Destroys the objects of the island's entities at or above current_auto_remove_level (last first); the highest
// level below it goes to max_found_auto_remove_level_out.
static int watch_dog_remove_objects_from_island(hkpSimulationIsland* island, int current_auto_remove_level, int& max_found_auto_remove_level_out)
{
    int num_removed = 0;
    for (int i = island->m_entities.getSize() - 1; i >= 0; i--)
    {
        hkpEntity* entity = island->m_entities[i];
        const int level = entity->m_autoRemoveLevel;
        if (level < current_auto_remove_level)
        {
            if (level > max_found_auto_remove_level_out)
            {
                max_found_auto_remove_level_out = level;
            }
            continue;
        }
        World.destroy_object(watch_dog_entity_object(entity), false);
        num_removed++;
    }
    return num_removed;
}

// 0x0081C480 (:342)
// LTCG: block_size in EAX (num_blocks unused).
static void pre_allocate_runtime_block(unsigned int block_size, unsigned int num_blocks)
{
    if (Havok_mem_largest_runtime_block <= block_size)
    {
        Havok_mem_largest_runtime_block = block_size;
    }
}

// ---- havok_memory_listener
// 0x0081C610
havok_memory_listener::havok_memory_listener()
{
}

// 0x0081C620
havok_memory_listener::~havok_memory_listener()
{
}

// 0x0083C120 (:368)
// The large block allocator is out of memory: free what the game can spare.
void havok_memory_listener::cannotAllocate(unsigned int size)
{
    havok_set_status(HAVOK_SYSTEM_MEMORY, HAVOK_STATUS_ERROR); // inlined
    havok_reclaim_memory(); // inlined
}

// 0x0083C1C0 (:414)
// Out of memory even after cannotAllocate: report and hang rather than crash.
void havok_memory_listener::allocationFailure(unsigned int size)
{
    char buffer[512];
    hkMemorySystem* system = hkMemorySystem::getInstancePtr();
    if (system != nullptr)
    {
        hkMemorySystem::MemoryStatistics stats;
        system->getMemoryStatistics(stats);
        for (int i = 0; i < stats.m_entries.getSize(); i++)
        {
            const hkMemorySystem::MemoryStatistics::Entry& entry = stats.m_entries[i];
            long largest = entry.m_allocatorStats.m_largestBlock;
            if (largest <= entry.m_allocatorStats.m_available)
            {
                largest = entry.m_allocatorStats.m_available;
            }
            keen::formatString(buffer, sizeof(buffer), "(%s) used: %s free: %s largest block: %s requested size: %s\n",
                               entry.m_allocatorName.cString(), str_commas((uint64_t)entry.m_allocatorStats.m_inUse),
                               str_commas((uint64_t)entry.m_allocatorStats.m_totalAvailable), str_commas((uint64_t)(unsigned int)largest),
                               str_commas((uint64_t)size));
        }
    }
    keen::debug::nativePrintArgs("*** hkLimitedMemoryListener::allocationFailure() just hit, entering infinite loop to avoid hard crash ***");
    for (;;)
    {
    }
}

// ---- havok_memory_watch_dog
// 0x00811A30 (:442)
havok_memory_watch_dog::havok_memory_watch_dog()
{
}

// 0x00811A60 (:448)
int havok_memory_watch_dog::getAmountOfFreeHeapMemoryRequested()
{
    return 0x200000; // 2 MB
}

// 0x0083C2B0 (:454)
// Removes up to 50 objects per memory statistics entry, from auto remove level 5 down to the level its total
// available memory allows ((available * 5 - 10 MB) / 2 MB, clamped to 0..4). When nothing was removed the search
// waits 500 ms; otherwise a garbage collection follows on the next frame (havok_memory_watch_dog_do_frame).
void havok_memory_watch_dog::freeHeapMemoryTillRequestedAmountIsAvailable(hkpWorld* world)
{
    havok_set_status(HAVOK_SYSTEM_MEMORY, HAVOK_STATUS_WARNING); // inlined
    if (search_delay.elapsed())
    {
        search_delay.invalidate();
    }
    if (search_delay.valid())
    {
        return;
    }
    hkMemorySystem::MemoryStatistics stats;
    hkMemorySystem::getInstance().getMemoryStatistics(stats);
    for (float entry_index = 0.0f; (int)entry_index < stats.m_entries.getSize(); entry_index = (float)((int)entry_index + 1))
    {
        const hkMemorySystem::MemoryStatistics::Entry& entry = stats.m_entries[(int)entry_index];
        int min_level = (entry.m_allocatorStats.m_totalAvailable * 5 - 0xA00000) / 0x200000;
        if (min_level >= 4)
        {
            min_level = 4;
        }
        else if (min_level < 1)
        {
            min_level = 0;
        }
        int num_removed = 0;
        int level = 5; // HAVOK_AUTO_REMOVE_FIRST
        while (level > min_level)
        {
            int max_found_level = 0;
            // Inactive islands (last first), then active islands.
            for (int i = -world->m_inactiveSimulationIslands.getSize(); num_removed < 50 && i < world->m_activeSimulationIslands.getSize(); i++)
            {
                hkpSimulationIsland* island = i < 0 ? world->m_inactiveSimulationIslands[-1 - i] : world->m_activeSimulationIslands[i];
                num_removed += watch_dog_remove_objects_from_island(island, level, max_found_level);
            }
            level = max_found_level;
            if (num_removed >= 50)
            {
                break;
            }
        }
        if (num_removed < 1)
        {
            search_delay.set(500);
        }
        else
        {
            Garbage_collect_framecount = Framecount;
        }
    }
}

// 0x0081C4E0 (:521)
void havok_memory_watch_dog::reduceConstraintsInIsland(const hkWorldMemoryAvailableWatchDog::MemUsageInfo& info, int numIterations)
{
    int max_found_level = 0;
    watch_dog_remove_objects_from_island(info.m_largestSimulationIsland, 5, max_found_level);
}

// 0x00811A50
havok_memory_watch_dog::~havok_memory_watch_dog()
{
}

// ---- functions
// 0x0083D840 (:529)
// Single player only: optionally draws the memory statistics, runs the watch dog when the largest free block is
// below 512 KB, and garbage collects one frame after the watch dog removed objects.
void havok_memory_watch_dog_do_frame(hkpWorld* world)
{
    if (Multiplayer)
    {
        return;
    }
    if (s_showHavokMemory)
    {
        hkMemorySystem::MemoryStatistics stats;
        Havok_memory->getMemoryStatistics(stats);
        for (int i = 0; i < stats.m_entries.getSize(); i++)
        {
            const hkMemorySystem::MemoryStatistics::Entry& entry = stats.m_entries[i];
            keen::DebugRenderer::drawText(1600, 100 + i * 16, 0xFFFFFFFF, "%s : %d, %d", entry.m_allocatorName.cString(),
                                          entry.m_allocatorStats.m_available, entry.m_allocatorStats.m_allocated);
        }
    }
    world->setMemoryWatchDog(Havok_memory_watch_dog);
    if (havok_memory_get_largest_block() < 0x80000) // 512 KB
    {
        Havok_memory_watch_dog->freeHeapMemoryTillRequestedAmountIsAvailable(world);
    }
    if (Garbage_collect_framecount != -1 && Garbage_collect_framecount != Framecount)
    {
        Garbage_collect_framecount = -1;
        hkMemorySystem::getInstance().garbageCollect();
    }
}

// 0x00811A70 (:568)
void havok_memory_watch_dog_free_memory(hkpWorld* world)
{
    Havok_memory_watch_dog->freeHeapMemoryTillRequestedAmountIsAvailable(world);
}

// 0x0081C510 (:575)
// Destroys the objects of the island's entities, highest auto remove level first, until desired_remove_count were
// removed or no level above 0 is left.
void havok_memory_watch_dog_free_memory(hkpWorld* world, hkpSimulationIsland* island, int desired_remove_count)
{
    int num_removed = 0;
    signed char level = 5; // HAVOK_AUTO_REMOVE_FIRST
    while (num_removed < desired_remove_count)
    {
        if (level < 1)
        {
            return;
        }
        signed char max_found_level = 0;
        int removed = 0;
        for (int i = island->m_entities.getSize() - 1; i >= 0; i--)
        {
            hkpEntity* entity = island->m_entities[i];
            const signed char entity_level = entity->m_autoRemoveLevel;
            if (entity_level < level)
            {
                if (entity_level >= max_found_level)
                {
                    max_found_level = entity_level;
                }
                continue;
            }
            World.destroy_object(watch_dog_entity_object(entity), false);
            removed++;
        }
        num_removed += removed;
        level = max_found_level;
    }
}

// 0x00824B70 (:599)
void havok_memory_init()
{
    Havok_error_handler = new (Havok_error_handler_memory) hkDefaultError(havok_report_function, nullptr);
    hkError::replaceInstanceAndAddReference(Havok_error_handler);

    const int size = Havok_mempool.space_free(); // inlined (end_pos - current_pos)
    void* block = Havok_mempool.alloc_all(0);
    hkLargeBlockAllocator* server = new (s_buffer.m_largeBlock) hkLargeBlockAllocator(block, size);
    hkFreeListAllocator* heap_allocator = new (s_buffer.m_allocator) hkFreeListAllocator(server, server, nullptr);
    hkFreeListMemorySystem* system = new (s_buffer.m_system) hkFreeListMemorySystem(server, heap_allocator, heap_allocator, hkFreeListMemorySystem::DEFAULT_SETUP_FLAGS);
    system->setHeapSoftLimit(0x200000); // 2 MB
    s_largeBlockAllocator = server;
    s_freeListAllocator = heap_allocator;
    s_system = system;
    hkMemorySystem::replaceInstance(system);
    s_onQuitFunc = onQuit;

    hkFlags<hkMemorySystem::FlagBits, int> flags;
    flags.m_storage = hkMemorySystem::FLAG_ALL;
    hkMemoryRouter* memory_router = system->mainInit(hkMemorySystem::FrameInfo(0x100000, 0x8000), flags); // 1 MB solver buffer, 32 KB stack hint

    Havok_memory_listener = new (Havok_memory_listener_local) havok_memory_listener();
    Havok_memory = system;
    server->m_limitedListener = Havok_memory_listener; // setLimitedMemoryListener, inlined
    const unsigned int soft_limit = size - 0x200000; // pool size less 2 MB
    server->setMemorySoftLimit(soft_limit);
    heap_allocator->setMemorySoftLimit(soft_limit);

    // The largest runtime block, rounded up to a step (0x3F60 * 2^n: 15.8 KB .. 990 KB). The steps are tested from
    // the largest, so every block up to 990 KB becomes 0xFD800 (see the notes).
    if (Havok_mem_largest_runtime_block <= 0xFD800)
    {
        Havok_mem_largest_runtime_block = 0xFD800;
    }
    else if (Havok_mem_largest_runtime_block <= 0x7EC00)
    {
        Havok_mem_largest_runtime_block = 0x7EC00;
    }
    else if (Havok_mem_largest_runtime_block <= 0x3F600)
    {
        Havok_mem_largest_runtime_block = 0x3F600;
    }
    else if (Havok_mem_largest_runtime_block <= 0x1FB00)
    {
        Havok_mem_largest_runtime_block = 0x1FB00;
    }
    else if (Havok_mem_largest_runtime_block <= 0xFD80)
    {
        Havok_mem_largest_runtime_block = 0xFD80;
    }
    else if (Havok_mem_largest_runtime_block <= 0x7EC0)
    {
        Havok_mem_largest_runtime_block = 0x7EC0;
    }
    else if (Havok_mem_largest_runtime_block <= 0x3F60)
    {
        Havok_mem_largest_runtime_block = 0x3F60;
    }

    hkBaseSystem::init(memory_router, havok_report_function, nullptr);
    Havok_memory_watch_dog = new havok_memory_watch_dog();
}

// 0x00824DD0 (:696)
void havok_memory_shutdown()
{
    if (Havok_memory_watch_dog != nullptr)
    {
        Havok_memory_watch_dog->removeReference();
        Havok_memory_watch_dog = nullptr;
    }
    if (Havok_memory_listener != nullptr)
    {
        Havok_memory_listener->~havok_memory_listener(); // the deleting destructor, which does not free
        Havok_memory_listener = nullptr;
    }
    hkBaseSystem::quit();
    hkError::replaceInstance(Havok_error_handler);
    if (s_onQuitFunc != nullptr)
    {
        s_onQuitFunc();
        s_onQuitFunc = nullptr;
    }
    if (Havok_memory != nullptr)
    {
        Havok_memory = nullptr;
    }
}

// 0x00811AA0 (:731)
void havok_memory_level_init()
{
    hkMemorySystem::getInstance().garbageCollect();
}

// 0x00811AB0 (:740)
void havok_memory_level_shutdown()
{
    hkMemorySystem::getInstance().garbageCollect();
}

// 0x0083C480 (:750)
// The largest block any allocator of the memory system can hand out.
unsigned int havok_memory_get_largest_block()
{
    hkMemorySystem::MemoryStatistics stats;
    Havok_memory->getMemoryStatistics(stats);
    unsigned int largest = 0;
    for (int i = 0; i < stats.m_entries.getSize(); i++)
    {
        const unsigned int block = get_largest_block(stats.m_entries[i].m_allocatorStats); // inlined
        if (largest <= block)
        {
            largest = block;
        }
    }
    return largest;
}

// 0x0081C640 (:771)
// The calling thread's memory router, created in thread-local storage on first use.
hkMemoryRouter* havok_create_thread_memory_router()
{
    if (hkMemoryRouter::getInstancePtr() == nullptr)
    {
        hkMemoryRouter* router = new (thread_router) hkMemoryRouter();
        hkFlags<hkMemorySystem::FlagBits, int> flags;
        flags.m_storage = hkMemorySystem::FLAG_ALL;
        Havok_memory->threadInit(*router, "Havok Thread Memory Router", flags);
        return router;
    }
    return hkMemoryRouter::getInstancePtr();
}

// 0x0082B080 (:789)
// Only the calling thread's router (or any router when the thread has none) is shut down.
void havok_destroy_thread_memory_router(hkMemoryRouter* router)
{
    if (router == nullptr)
    {
        return;
    }
    if (router != hkMemoryRouter::getInstancePtr() && hkMemoryRouter::getInstancePtr() != nullptr)
    {
        return;
    }
    hkMemoryRouter::replaceInstance(nullptr);
    hkFlags<hkMemorySystem::FlagBits, int> flags;
    flags.m_storage = hkMemorySystem::FLAG_ALL;
    Havok_memory->threadQuit(*router, flags);
    router->~hkMemoryRouter();
}
