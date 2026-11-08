// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/listener/hkpislandpostintegratelistener.h
//
// Havok 2014.2.5 (havok_impl.md HK7): island post-integrate listener. Virtuals from rfg.pdb. Hand-maintained.
#pragma once

class hkStepInfo;
class hkpSimulationIsland;

// size 0x4
class hkpIslandPostIntegrateListener
{
public:
    virtual ~hkpIslandPostIntegrateListener() {} // vtbl+0x00
    virtual void postIntegrateCallback(hkpSimulationIsland* island, const hkStepInfo& stepInfo) = 0; // vtbl+0x04
};
