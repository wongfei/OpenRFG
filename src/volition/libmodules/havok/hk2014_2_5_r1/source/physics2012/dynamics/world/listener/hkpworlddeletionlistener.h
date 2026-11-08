// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/listener/hkpworlddeletionlistener.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once


class hkpWorld;

// size 0x4
class hkpWorldDeletionListener
{
public:
    virtual ~hkpWorldDeletionListener(); // vtbl+0x00 0x00DF2E80
    virtual void worldDeletedCallback(hkpWorld*) = 0; // vtbl+0x04
    virtual void worldRemoveAllCallback(hkpWorld* world); // vtbl+0x08 0x008117C0 (.h:28)
    hkpWorldDeletionListener(); // 0x00DF2EB0

    // 0x00 vfptr
};
