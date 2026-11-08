// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h
//
// Hand-written (havok_impl.md HK0/HK1): Havok's class allocator macros. In the SDK they are defined in hkMemoryRouter.h
// and hkMemoryAllocator.h (Havok 2014.1 Common/Base/Memory/Router/hkMemoryRouter.h); they live here, next to the
// memory classes they take, so the generated headers can use them without including the router (which would make
// include cycles through the allocator headers). The memory class argument is not recorded in rfg.pdb and is left out.
//
// - HK_DECLARE_CLASS_ALLOCATOR() (hkReferencedObject subclasses): operator new takes the bytes from the thread's heap
//   allocator; operator delete frees the object's memory size (hkReferencedObject::getMemorySizeAndFlags, the 16 bits
//   at +6) unless that is 0xFFFF (AUTO_MEMSIZE, set by the constructor), in which case it frees the size the compiler
//   passes (sizeof the most derived class).
// - HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(X): the same on the heap, always sizeof(X).
// - HK_DECLARE_PLACEMENT_ALLOCATOR() (the memory classes: allocators, router, memory system): placement new only,
//   and the deletes do nothing (these objects live in static or caller storage).
// Each also declares placement new and the matching placement deletes. The operators read the router's thread-local
// slot through hkMemory::heapBlockAlloc/heapBlockFree (hkmemoryrouter.cpp), which do what the SDK's inline operators
// do (TlsGetValue, then heap()->blockAlloc/blockFree).
//
// HK_DECLARE_POD_TYPE (hkTraitBase.h in the SDK) declares `hkBool::CompileTimeTrueType operator==(const
// hkTrait::TraitBool<true>&) const`, which hkTrait::IsPodType detects; the containers then copy elements with memcpy.
// It is a declaration only (the PDB lists it as `char operator==(const hkTrait::TraitBool<1>&) const`).
#pragma once

#include <stddef.h>

namespace hkMemory
{
void* heapBlockAlloc(unsigned int nbytes); // 0x00CE77F0 (hkmemoryrouter.cpp:112)
void heapBlockFree(void* p, unsigned int nbytes); // 0x00CE7810 (hkmemoryrouter.cpp:117)
}

#define HK_DECLARE_CLASS_ALLOCATOR(...) \
    static void* operator new(size_t nbytes) { return hkMemory::heapBlockAlloc((unsigned int)nbytes); } \
    static void operator delete(void* p, size_t nbytes) \
    { \
        const unsigned short memSize = *(const unsigned short*)((const char*)p + 6); /* hkReferencedObject memory size */ \
        hkMemory::heapBlockFree(p, memSize == 0xFFFF ? (unsigned int)nbytes : memSize); \
    } \
    static void* operator new(size_t, void* p) { return p; } \
    static void* operator new[](size_t, void* p) { return p; } \
    static void operator delete(void*, void*) {} \
    static void operator delete[](void*, void*) {}

#define HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(X) \
    static void* operator new(size_t) { return hkMemory::heapBlockAlloc(sizeof(X)); } \
    static void operator delete(void* p, size_t) \
    { \
        if (p != nullptr) \
        { \
            hkMemory::heapBlockFree(p, sizeof(X)); \
        } \
    } \
    static void* operator new(size_t, void* p) { return p; } \
    static void* operator new[](size_t, void* p) { return p; } \
    static void operator delete(void*, void*) {} \
    static void operator delete[](void*, void*) {}

#define HK_DECLARE_PLACEMENT_ALLOCATOR() \
    static void* operator new(size_t, void* p) { return p; } \
    static void operator delete(void*, void*) {} \
    static void operator delete(void*, size_t) {}

#define HK_DECLARE_POD_TYPE()

// The memory classes (rfg.pdb; stub_impl.md S4: the packfile chunks and update trackers keep one by value). The
// allocator macros above take none: the shipped build does not track memory by class.
enum HK_MEMORY_CLASS
{
    HK_MEMORY_CLASS_ROOT = 0,
    HK_MEMORY_CLASS_BASE = 1,
    HK_MEMORY_CLASS_COLLIDE = 2,
    HK_MEMORY_CLASS_CONSTRAINT_SOLVER = 3,
    HK_MEMORY_CLASS_DYNAMICS = 4,
    HK_MEMORY_CLASS_PHYSICS = 5,
    HK_MEMORY_CLASS_EXPORT = 6,
    HK_MEMORY_CLASS_SERIALIZE = 7,
    HK_MEMORY_CLASS_GEOMETRY = 8,
    HK_MEMORY_CLASS_DISPLAY = 9,
    HK_MEMORY_CLASS_MATH = 10,
    HK_MEMORY_CLASS_SCENE_DATA = 11,
    HK_MEMORY_CLASS_VEHICLE = 12,
    HK_MEMORY_CLASS_DESTRUCTION_2012 = 13,
    HK_MEMORY_CLASS_DESTRUCTION = 14,
    HK_MEMORY_CLASS_UTILITIES = 15,
    HK_MEMORY_CLASS_ANIMATION = 16,
    HK_MEMORY_CLASS_BEHAVIOR = 17,
    HK_MEMORY_CLASS_CLOTH = 18,
    HK_MEMORY_CLASS_AI = 19,
    HK_MEMORY_CLASS_TOOLS = 20,
    HK_MEMORY_CLASS_DEMO_FRAMEWORK = 21,
    HK_MEMORY_CLASS_DEMO = 22,
    HK_MEMORY_CLASS_MILSIM = 23,
    HK_MEMORY_CLASS_USER = 24,
    HK_MEMORY_CLASS_BASE_CLASS = 25,
    HK_MEMORY_CLASS_STRING = 26,
    HK_MEMORY_CLASS_ARRAY = 27,
    HK_MEMORY_CLASS_SINGLETON = 28,
    HK_MEMORY_CLASS_SCRATCHPAD = 29,
    HK_MEMORY_CLASS_STREAM = 30,
    HK_MEMORY_CLASS_MONITOR = 31,
    HK_MEMORY_CLASS_MAP = 32,
    HK_MEMORY_CLASS_TREE = 33,
    HK_MEMORY_CLASS_HKCLASS = 34,
    HK_MEMORY_CLASS_AGENT = 35,
    HK_MEMORY_CLASS_ALGORITHM = 36,
    HK_MEMORY_CLASS_BROAD_PHASE = 37,
    HK_MEMORY_CLASS_CONTACT = 38,
    HK_MEMORY_CLASS_DISPATCH = 39,
    HK_MEMORY_CLASS_MANIFOLD = 40,
    HK_MEMORY_CLASS_SHAPE = 41,
    HK_MEMORY_CLASS_COLLIDABLE = 42,
    HK_MEMORY_CLASS_CDINFO = 43,
    HK_MEMORY_CLASS_MOPP = 44,
    HK_MEMORY_CLASS_ACTION = 45,
    HK_MEMORY_CLASS_DYNCOLLIDE = 46,
    HK_MEMORY_CLASS_CONSTRAINT = 47,
    HK_MEMORY_CLASS_ENTITY = 48,
    HK_MEMORY_CLASS_MOTION = 49,
    HK_MEMORY_CLASS_WORLD = 50,
    HK_MEMORY_CLASS_RIGID_BODY = 51,
    HK_MEMORY_CLASS_PHANTOM = 52,
    HK_MEMORY_CLASS_SIMISLAND = 53,
    HK_MEMORY_CLASS_CAMERA = 54,
    HK_MEMORY_CLASS_CHARACTER = 55,
    HK_MEMORY_CLASS_VDB = 56,
    HK_MEMORY_CLASS_INERTIA = 57,
    HK_MEMORY_CLASS_ANIM_UNCOMPRESSED = 58,
    HK_MEMORY_CLASS_ANIM_COMPRESSED = 59,
    HK_MEMORY_CLASS_ANIM_DATA = 60,
    HK_MEMORY_CLASS_ANIM_MOTION = 61,
    HK_MEMORY_CLASS_ANIM_RIG = 62,
    HK_MEMORY_CLASS_ANIM_CONTROL = 63,
    HK_MEMORY_CLASS_ANIM_RUNTIME = 64,
    HK_MEMORY_CLASS_ANIM_CACHE = 65,
    HK_MEMORY_CLASS_BEHAVIOR_RUNTIME = 66,
    HK_MEMORY_CLASS_BEHAVIOR_DEMOS = 67,
    HK_MEMORY_CLASS_CLOTH_RUNTIME = 68,
    HK_MEMORY_CLASS_CLOTH_DATA = 69,
    HK_MEMORY_CLASS_CLOTH_SETUP = 70,
    HK_MEMORY_CLASS_AI_NAVMESH = 71,
    HK_MEMORY_CLASS_AI_ASTAR = 72,
    HK_MEMORY_CLASS_AI_STEERING = 73,
    HK_MEMORY_CLASS_AI_SPATIAL = 74,
    HK_MEMORY_CLASS_MILSIM_FLT = 75,
    HK_MEMORY_CLASS_MILSIM_CIGI = 76,
    HK_MEMORY_CLASS_MILSIM_CHARACTER = 77,
    HK_MEMORY_CLASS_MILSIM_VEHICLE = 78,
    HK_MEMORY_CLASS_MILSIM_COMMON = 79,
    HK_MEMORY_CLASS_USER_1 = 80,
    HK_MEMORY_CLASS_USER_2 = 81,
    HK_MEMORY_CLASS_USER_3 = 82,
    HK_MEMORY_CLASS_USER_4 = 83,
    HK_MEMORY_CLASS_PRE_ALLOCATED = 84,
    HK_MEMORY_CLASS_MAX = 85,
};
