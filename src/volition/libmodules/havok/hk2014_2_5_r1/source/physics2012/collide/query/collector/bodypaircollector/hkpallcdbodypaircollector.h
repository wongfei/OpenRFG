// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/query/collector/bodypaircollector/hkpallcdbodypaircollector.h
//
// Seam only (Havok 2014.2.5).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/query/hkpcdbodypaircollector.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/query/collector/bodypaircollector/hkprootcdbodypair.h"

class hkpCdBody;

// size 0x114
class hkpAllCdBodyPairCollector : public hkpCdBodyPairCollector
{
public:
    hkpAllCdBodyPairCollector(); // 0x00831B00 (hkpallcdbodypaircollector.inl:16)
    virtual ~hkpAllCdBodyPairCollector(); // vtbl+0x00 0x00831B50 (hkpallcdbodypaircollector.inl:21)
    virtual void reset(); // vtbl+0x08 0x008224D0 (hkpallcdbodypaircollector.inl:10)
    const hkArray<hkpRootCdBodyPair,hkContainerHeapAllocator>& getHits() const; // 0x0080DC30 (hkpallcdbodypaircollector.inl:26)
public:
    virtual void addCdBodyPair(const hkpCdBody& bodyA, const hkpCdBody& bodyB); // vtbl+0x04 0x00D975B0

    hkInplaceArray<hkpRootCdBodyPair,16,hkContainerHeapAllocator> m_hits; // 0x08
};
