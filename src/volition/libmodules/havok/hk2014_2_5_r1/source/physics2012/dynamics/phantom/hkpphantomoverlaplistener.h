// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/phantom/hkpphantomoverlaplistener.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0); the events and hkpCollidableAccept added by
// hand in HK7 (layouts from rfg.pdb). Hand-maintained (nobuild in the hdrgen specs).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"

class hkpCollidable;
class hkpPhantom;

enum hkpCollidableAccept
{
    HK_COLLIDABLE_ACCEPT = 0,
    HK_COLLIDABLE_REJECT = 1,
};

// size 0xC
struct hkpCollidableAddedEvent
{
    const hkpPhantom* m_phantom; // 0x00
    const hkpCollidable* m_collidable; // 0x04
    mutable hkpCollidableAccept m_collidableAccept; // 0x08 (listeners may reject)
};

// size 0xC
struct hkpCollidableRemovedEvent
{
    const hkpPhantom* m_phantom; // 0x00
    const hkpCollidable* m_collidable; // 0x04
    hkBool m_collidableWasAdded; // 0x08
};

// size 0x4
class hkpPhantomOverlapListener
{
public:
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpPhantomOverlapListener); // delete(void* p, unsigned int nbytes) 0x008142D0
    virtual void collidableAddedCallback(const hkpCollidableAddedEvent&) = 0; // vtbl+0x00
    virtual void collidableRemovedCallback(const hkpCollidableRemovedEvent&) = 0; // vtbl+0x04
    virtual ~hkpPhantomOverlapListener(); // vtbl+0x08 0x0080CD90 (.h:87)
    hkpPhantomOverlapListener(); // 0x0080D690

    // 0x00 vfptr
};
