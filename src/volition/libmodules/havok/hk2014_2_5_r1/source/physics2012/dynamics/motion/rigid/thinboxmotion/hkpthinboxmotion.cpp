// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/motion/rigid/thinboxmotion/hkpthinboxmotion.cpp
//
// Havok 2014.2.5 (havok_impl.md HK7): the thin box motion (a box motion of its own type, which the integrator
// treats separately), from rfg.exe.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/motion/rigid/thinboxmotion/hkpthinboxmotion.h"

// 0x00E28570
hkpThinBoxMotion::hkpThinBoxMotion(const hkVector4f& position, const hkQuaternionf& rotation)
    : hkpBoxMotion(position, rotation)
{
    m_type.m_storage = MOTION_THIN_BOX_INERTIA;
}

// 0x00DDEF60 (hkpthinboxmotion.h)
hkpThinBoxMotion::hkpThinBoxMotion(hkFinishLoadedObjectFlag flag)
    : hkpBoxMotion(flag)
{
}

// 0x00DDEFD0
hkpThinBoxMotion::~hkpThinBoxMotion()
{
}
