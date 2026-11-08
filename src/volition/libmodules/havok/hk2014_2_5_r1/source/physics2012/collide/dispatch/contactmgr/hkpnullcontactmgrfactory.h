// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/contactmgr/hkpnullcontactmgrfactory.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/contactmgr/hkpcontactmgrfactory.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/contactmgr/hkpnullcontactmgr.h"

class hkpCollidable;
struct hkpCollisionInput;
class hkpContactMgr;

// size 0x14
class hkpNullContactMgrFactory : public hkpContactMgrFactory
{
public:
    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int nbytes) 0x00DE9F80, delete(void* p, unsigned int nbytes) 0x00DE9FF0
    hkpNullContactMgrFactory(); // 0x00DE9FA0
    virtual ~hkpNullContactMgrFactory(); // vtbl+0x00 0x00DE9FE0
    virtual hkpContactMgr* createContactMgr(const hkpCollidable& a, const hkpCollidable& b, const hkpCollisionInput& env); // vtbl+0x10 0x00DE9FD0

    hkpNullContactMgr m_contactMgr; // 0x08
};
