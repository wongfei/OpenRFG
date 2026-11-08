// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/broadphase/hkpbroadphaselistener.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once


class hkpTypedBroadPhaseHandlePair;

// size 0x4
class hkpBroadPhaseListener
{
public:
    virtual ~hkpBroadPhaseListener(); // vtbl+0x00 0x00D9A640
    virtual void addCollisionPair(hkpTypedBroadPhaseHandlePair&) = 0; // vtbl+0x04
    virtual void removeCollisionPair(hkpTypedBroadPhaseHandlePair&) = 0; // vtbl+0x08
    hkpBroadPhaseListener(); // 0x00D9A670

    // 0x00 vfptr
};
