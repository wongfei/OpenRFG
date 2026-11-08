// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/listener/hkpworldpostintegratelistener.h
//
// Havok 2014.2.5 (havok_impl.md HK7): world post-integrate listener. Virtuals from rfg.pdb. Hand-maintained.
#pragma once

class hkStepInfo;
class hkpWorld;

// size 0x4
class hkpWorldPostIntegrateListener
{
public:
    virtual ~hkpWorldPostIntegrateListener() {} // vtbl+0x00
    virtual void postIntegrateCallback(hkpWorld* world, const hkStepInfo& stepInfo) = 0; // vtbl+0x04
};
