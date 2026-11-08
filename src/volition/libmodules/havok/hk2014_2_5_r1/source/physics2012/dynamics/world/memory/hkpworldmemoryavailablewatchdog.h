// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/memory/hkpworldmemoryavailablewatchdog.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hkreferencedobject.h"

class hkClass;
class hkFinishLoadedObjectFlag;
struct hkInternalClassMember;
class hkpSimulationIsland;
class hkpWorld;

// size 0x8
class hkWorldMemoryAvailableWatchDog : public hkReferencedObject
{
public:
    // size 0xC
    struct MemUsageInfo
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(MemUsageInfo);

        int m_maxRuntimeBlockSize; // 0x00
        int m_sumRuntimeBlockSize; // 0x04
        hkpSimulationIsland* m_largestSimulationIsland; // 0x08
    };

    static const hkClass& staticClass(); // 0x00DEDBA0
    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int nbytes) 0x00814540, delete(void* p, unsigned int nbytes) 0x00814560
    hkWorldMemoryAvailableWatchDog(hkFinishLoadedObjectFlag flag); // 0x00DEFC50
    hkWorldMemoryAvailableWatchDog(); // 0x0080CF10 (hkpworldmemoryavailablewatchdog.inl:10)
    virtual ~hkWorldMemoryAvailableWatchDog(); // vtbl+0x00 0x0080CF20
    virtual int getAmountOfFreeHeapMemoryRequested() = 0; // vtbl+0x10
    virtual void freeHeapMemoryTillRequestedAmountIsAvailable(hkpWorld*) = 0; // vtbl+0x14
    virtual void reduceConstraintsInIsland(const MemUsageInfo&, int) = 0; // vtbl+0x18

    static const hkInternalClassMember Members[];
};
