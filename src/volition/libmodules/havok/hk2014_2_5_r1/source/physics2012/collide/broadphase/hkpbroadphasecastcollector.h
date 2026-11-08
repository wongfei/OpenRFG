// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/broadphase/hkpbroadphasecastcollector.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once


class hkpBroadPhaseHandle;

// size 0x4
class hkpBroadPhaseCastCollector
{
public:
    virtual ~hkpBroadPhaseCastCollector(); // vtbl+0x00 0x008125F0 (.h:21)
    virtual float addBroadPhaseHandle(const hkpBroadPhaseHandle*, int) = 0; // vtbl+0x04
    hkpBroadPhaseCastCollector(); // 0x00812630

    // 0x00 vfptr
};
