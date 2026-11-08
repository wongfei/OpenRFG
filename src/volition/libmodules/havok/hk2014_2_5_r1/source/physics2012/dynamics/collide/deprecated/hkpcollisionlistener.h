// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/collide/deprecated/hkpcollisionlistener.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/collide/contactlistener/hkpcontactlistener.h"

struct hkpContactPointAddedEvent;
struct hkpContactPointConfirmedEvent;
class hkpContactPointEvent;
struct hkpContactPointRemovedEvent;
struct hkpContactProcessEvent;

// size 0x4
class hkpCollisionListener : public hkpContactListener
{
public:
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpCollisionListener); // delete(void* p, unsigned int nbytes) 0x0081B270
    virtual void contactPointCallback(const hkpContactPointEvent& event); // vtbl+0x00 0x00E03CB0
    virtual ~hkpCollisionListener(); // vtbl+0x0C 0x0080D3F0 (.h:61)
    virtual void contactPointAddedCallback(hkpContactPointAddedEvent& event); // vtbl+0x10 0x0081B230 (.h:29)
    virtual void contactPointRemovedCallback(hkpContactPointRemovedEvent& event); // vtbl+0x14 0x0081B240 (.h:32)
    virtual void contactProcessCallback(hkpContactProcessEvent& event); // vtbl+0x18 0x0081B250 (.h:40)
    virtual void contactPointConfirmedCallback(hkpContactPointConfirmedEvent& event); // vtbl+0x1C 0x0081B260 (.h:50)
    hkpCollisionListener(); // 0x0081B220

};
