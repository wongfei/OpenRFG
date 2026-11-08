// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/phantom/hkpphantombroadphaselistener.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hkreferencedobject.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/broadphase/hkpbroadphaselistener.h"

class hkpTypedBroadPhaseHandlePair;

// size 0xC
class hkpPhantomBroadPhaseListener : public hkReferencedObject, public hkpBroadPhaseListener
{
public:
    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int nbytes) 0x00DEA5C0, delete(void* p, unsigned int nbytes) 0x00DEA5E0
    hkpPhantomBroadPhaseListener(); // 0x00DE87A0
    virtual ~hkpPhantomBroadPhaseListener(); // vtbl+0x00 0x00DE87C0
    virtual void addCollisionPair(hkpTypedBroadPhaseHandlePair& pair); // vtbl+0x04 0x00E16280
    virtual void removeCollisionPair(hkpTypedBroadPhaseHandlePair& pair); // vtbl+0x08 0x00E162E0

};
