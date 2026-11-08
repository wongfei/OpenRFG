// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkpentityactivationlistener.h
//
// Havok 2014.2.5 (havok_impl.md HK7): entity activation listener. Virtuals from rfg.pdb. Hand-maintained.
#pragma once

class hkpEntity;

// size 0x4
class hkpEntityActivationListener
{
public:
    hkpEntityActivationListener() {} // 0x00AF1400
    virtual ~hkpEntityActivationListener() {} // vtbl+0x00 0x00AED290
    virtual void entityDeactivatedCallback(hkpEntity* entity) = 0; // vtbl+0x04
    virtual void entityActivatedCallback(hkpEntity* entity) = 0; // vtbl+0x08
};
