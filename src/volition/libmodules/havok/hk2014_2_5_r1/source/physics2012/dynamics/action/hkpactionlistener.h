// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/action/hkpactionlistener.h
//
// Havok 2014.2.5 (havok_impl.md HK7): the world's action listener. Virtuals from rfg.pdb, the empty defaults of
// Havok 2014.1. Hand-maintained.
#pragma once

class hkpAction;

// size 0x4
class hkpActionListener
{
public:
    virtual ~hkpActionListener() {} // vtbl+0x00
    virtual void actionAddedCallback(hkpAction* action) {} // vtbl+0x04
    virtual void actionRemovedCallback(hkpAction* action) {} // vtbl+0x08
};
