// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/listener/hkpworldpostcollidelistener.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once


class hkStepInfo;
class hkpWorld;

// size 0x4
class hkpWorldPostCollideListener
{
public:
    virtual ~hkpWorldPostCollideListener(); // vtbl+0x00 0x0080D400 (.h:23)
    virtual void postCollideCallback(hkpWorld*, const hkStepInfo&) = 0; // vtbl+0x04
    hkpWorldPostCollideListener(); // 0x0080FBD0

    // 0x00 vfptr
};
