// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/broadphase/hkptypedbroadphasedispatcher.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/broadphase/hkpnullbroadphaselistener.h"

struct hkContainerHeapAllocator;
class hkpBroadPhaseHandlePair;
class hkpBroadPhaseListener;
class hkpCollidableCollidableFilter;
class hkpTypedBroadPhaseHandlePair;

// size 0x10C
class hkpTypedBroadPhaseDispatcher
{
public:
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpTypedBroadPhaseDispatcher); // new(unsigned int nbytes) 0x00DEA0D0, delete(void* p, unsigned int nbytes) 0x00DEA0F0
    hkpTypedBroadPhaseDispatcher(); // 0x00D9A6C0
    ~hkpTypedBroadPhaseDispatcher(); // 0x00D9A790
    static void removeDuplicates(hkArray<hkpBroadPhaseHandlePair,hkContainerHeapAllocator>& newPairs, hkArray<hkpBroadPhaseHandlePair,hkContainerHeapAllocator>& delPairs); // 0x00D9A860
    void addPairs(hkpTypedBroadPhaseHandlePair* newPairs, int numNewPairs, const hkpCollidableCollidableFilter* filter) const; // 0x00D9A7B0
    void removePairs(hkpTypedBroadPhaseHandlePair* deletedPairs, int numDeletedPairs) const; // 0x00D9A820
    hkpBroadPhaseListener* getBroadPhaseListener(int, int);
    void setBroadPhaseListener(hkpBroadPhaseListener* listener, int a, int b); // 0x00DE8680
    hkpBroadPhaseListener* getNullBroadPhaseListener();

public:
    hkpBroadPhaseListener* m_broadPhaseListeners[8][8]; // 0x00
    hkpNullBroadPhaseListener m_nullBroadPhaseListener; // 0x100
};
