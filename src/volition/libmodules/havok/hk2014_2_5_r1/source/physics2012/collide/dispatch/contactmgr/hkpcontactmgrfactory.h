// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/contactmgr/hkpcontactmgrfactory.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hkreferencedobject.h"

class hkpCollidable;
struct hkpCollisionInput;
class hkpContactMgr;

// size 0x8
class hkpContactMgrFactory : public hkReferencedObject
{
public:
    HK_DECLARE_CLASS_ALLOCATOR(); // delete(void* p, unsigned int nbytes) 0x00DE9DD0
    virtual ~hkpContactMgrFactory(); // vtbl+0x00 0x00DE8670
    hkpContactMgrFactory(); // 0x00DE8660
    virtual hkpContactMgr* createContactMgr(const hkpCollidable&, const hkpCollidable&, const hkpCollisionInput&) = 0; // vtbl+0x10

};
