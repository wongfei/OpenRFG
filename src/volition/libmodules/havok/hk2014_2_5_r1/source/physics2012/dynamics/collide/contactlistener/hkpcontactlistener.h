// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/collide/contactlistener/hkpcontactlistener.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once


class hkpCollisionEvent;
struct hkpContactPointAddedEvent;
class hkpContactPointEvent;
struct hkpContactPointRemovedEvent;
struct hkpContactProcessEvent;

// size 0x4
class hkpContactListener
{
public:
    virtual void contactPointCallback(const hkpContactPointEvent& event); // vtbl+0x00 0x0080D370 (.h:34)
    virtual void collisionAddedCallback(const hkpCollisionEvent& event); // vtbl+0x04 0x0080D380 (.h:50)
    virtual void collisionRemovedCallback(const hkpCollisionEvent& event); // vtbl+0x08 0x0080D390 (.h:53)
    void registerForEndOfStepContactPointCallbacks(const hkpCollisionEvent&);
    void unregisterForEndOfStepContactPointCallbacks(const hkpCollisionEvent&);
    virtual ~hkpContactListener(); // vtbl+0x0C 0x0080D360 (.h:69)
    virtual void contactPointAddedCallback(hkpContactPointAddedEvent& event); // vtbl+0x10 0x0080D3A0 (.h:77)
    virtual void contactPointRemovedCallback(hkpContactPointRemovedEvent& event); // vtbl+0x14 0x0080D3B0 (.h:78)
    virtual void contactProcessCallback(hkpContactProcessEvent& event); // vtbl+0x18 0x0080D3C0 (.h:79)
    hkpContactListener(); // 0x0080FBB0

    // 0x00 vfptr
};
