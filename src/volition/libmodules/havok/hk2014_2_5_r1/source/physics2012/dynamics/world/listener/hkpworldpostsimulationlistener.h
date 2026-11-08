// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/listener/hkpworldpostsimulationlistener.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once


class hkpEntity;
class hkpWorld;

// size 0x4
class hkpWorldPostSimulationListener
{
public:
    virtual ~hkpWorldPostSimulationListener(); // vtbl+0x00 0x0080D4F0 (.h:21)
    virtual void postSimulationCallback(hkpWorld*) = 0; // vtbl+0x04
    virtual void inactiveEntityMovedCallback(hkpEntity* entity); // vtbl+0x08 0x0080D500 (.h:27)
    hkpWorldPostSimulationListener(); // 0x0080FBF0

    // 0x00 vfptr
};
