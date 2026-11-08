// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/collide/contactlistener/util/hkpcollisioncallbackutil.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/collide/contactlistener/util/hkpendofstepcallbackutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/hkpconstraintlistener.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/extensions/hkpworldextension.h"

class hkpConstraintInstance;
class hkpWorld;

// size 0x50
class hkpCollisionCallbackUtil : public hkpWorldExtension, public hkpConstraintListener
{
public:
    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int nbytes) 0x00DEAA90, delete(void* p, unsigned int nbytes) 0x00DED780
    hkpCollisionCallbackUtil(); // 0x00DED740
    static void requireCollisionCallbackUtil(hkpWorld* world); // 0x00DED8C0
    static void releaseCollisionCallbackUtil(hkpWorld* world); // 0x00F21F60
    static hkpCollisionCallbackUtil* findCollisionCallbackUtil(const hkpWorld* world); // 0x00DEBAE0
    virtual ~hkpCollisionCallbackUtil(); // vtbl+0x00 0x00DED7C0
    virtual void constraintAddedCallback(hkpConstraintInstance* constraint); // vtbl+0x04 0x00E1FA80
    virtual void constraintRemovedCallback(hkpConstraintInstance* constraint); // vtbl+0x08 0x00E1FB60
    virtual void performAttachments(hkpWorld* world); // vtbl+0x10 0x00E1F9E0
    virtual void performDetachments(hkpWorld* world); // vtbl+0x14 0x00E1FA30

    hkpEndOfStepCallbackUtil m_endOfStepCallbackUtil; // 0x18
    static const int s_id; // 0x0135BB8C
};
