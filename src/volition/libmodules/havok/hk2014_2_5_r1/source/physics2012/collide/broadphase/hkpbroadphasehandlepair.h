// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/broadphase/hkpbroadphasehandlepair.h
//
// Seam only (Havok 2014.2.5).
#pragma once


class hkpBroadPhaseHandle;

// size 0x8
class hkpBroadPhaseHandlePair
{
public:
    hkpBroadPhaseHandle* m_a; // 0x00
    hkpBroadPhaseHandle* m_b; // 0x04
};
