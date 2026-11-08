// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/query/collector/bodypaircollector/hkpflagcdbodypaircollector.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/query/hkpcdbodypaircollector.h"

class hkBool;
class hkpCdBody;

// size 0x8
class hkpFlagCdBodyPairCollector : public hkpCdBodyPairCollector
{
public:
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpFlagCdBodyPairCollector); // delete(void* p, unsigned int nbytes) 0x00DB71C0
    hkpFlagCdBodyPairCollector(); // 0x00DB71B0
    virtual ~hkpFlagCdBodyPairCollector(); // vtbl+0x00 0x00DB71F0
    hkBool hasHit() const; // 0x00DB7200
public:
    virtual void addCdBodyPair(const hkpCdBody& bodyA, const hkpCdBody& bodyB); // vtbl+0x04 0x00DD1BA0

};
