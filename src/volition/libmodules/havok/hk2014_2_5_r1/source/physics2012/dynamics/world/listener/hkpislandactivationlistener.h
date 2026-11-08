// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/listener/hkpislandactivationlistener.h
//
// Havok 2014.2.5 (havok_impl.md HK7): island activation listener. Virtuals from rfg.pdb. Hand-maintained.
#pragma once

class hkpSimulationIsland;

// size 0x4
class hkpIslandActivationListener
{
public:
    virtual ~hkpIslandActivationListener() {} // vtbl+0x00
    virtual void islandActivatedCallback(hkpSimulationIsland* island) = 0; // vtbl+0x04
    virtual void islandDeactivatedCallback(hkpSimulationIsland* island) = 0; // vtbl+0x08
};
