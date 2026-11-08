// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/util/broadphase/hkpentityentitybroadphaselistener.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hkreferencedobject.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/broadphase/hkpbroadphaselistener.h"

class hkpTypedBroadPhaseHandlePair;
class hkpWorld;

// size 0x10
class hkpEntityEntityBroadPhaseListener : public hkReferencedObject, public hkpBroadPhaseListener
{
public:
    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int nbytes) 0x00DEA670, delete(void* p, unsigned int nbytes) 0x00E201B0
    hkpEntityEntityBroadPhaseListener(hkpWorld* world); // 0x00E200D0
    virtual ~hkpEntityEntityBroadPhaseListener(); // vtbl+0x00 0x00E20190
    virtual void addCollisionPair(hkpTypedBroadPhaseHandlePair& pair); // vtbl+0x04 0x00E20100
    virtual void removeCollisionPair(hkpTypedBroadPhaseHandlePair& pair); // vtbl+0x08 0x00E20160

public:
    hkpWorld* m_world; // 0x0C
};
