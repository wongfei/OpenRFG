// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/query/hkpcdbodypaircollector.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"

class hkpCdBody;

// size 0x8
class hkpCdBodyPairCollector
{
public:
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpCdBodyPairCollector); // delete(void* p, unsigned int nbytes) 0x0071AD40
    hkpCdBodyPairCollector(); // 0x00710ED0 (hkpcdbodypaircollector.inl:16)
    virtual ~hkpCdBodyPairCollector(); // vtbl+0x00 0x00710EE0 (hkpcdbodypaircollector.inl:21)
    virtual void addCdBodyPair(const hkpCdBody&, const hkpCdBody&) = 0; // vtbl+0x04
    virtual void reset(); // vtbl+0x08 0x00710EC0 (hkpcdbodypaircollector.inl:11)
    hkBool getEarlyOut() const; // 0x00D78400

    // 0x00 vfptr
public:
    hkBool m_earlyOut; // 0x04
};
