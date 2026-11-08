// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/broadphase/hkpbroadphasehandle.h
//
// Seam only (Havok 2014.2.5).
#pragma once


class hkClass;
class hkFinishLoadedObjectFlag;

// size 0x4
class hkpBroadPhaseHandle
{
public:
    static const hkClass& staticClass(); // 0x00D7DD50
    hkpBroadPhaseHandle(hkFinishLoadedObjectFlag flag); // 0x00D7D5A0
    hkpBroadPhaseHandle(); // 0x00D8B8E0

    unsigned int m_id; // 0x00
};
