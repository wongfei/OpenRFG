// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/broadphase/hkpnullbroadphaselistener.cpp
//
// Havok 2014.2.5 (havok_impl.md HK4): the listener that ignores pairs (the dispatcher's default), from rfg.exe. The
// listener interface's and this class's constructors and destructors are inline in the SDK headers.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/broadphase/hkpnullbroadphaselistener.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/broadphase/hkpbroadphaselistener.h"

// 0x00D9A670
hkpBroadPhaseListener::hkpBroadPhaseListener()
{
}

// 0x00D9A640
hkpBroadPhaseListener::~hkpBroadPhaseListener()
{
}

// 0x00D9AC10 (.h:21)
hkpNullBroadPhaseListener::hkpNullBroadPhaseListener()
{
}

// 0x00D9AC40
hkpNullBroadPhaseListener::~hkpNullBroadPhaseListener()
{
}

// 0x00DB1160
void hkpNullBroadPhaseListener::addCollisionPair(hkpTypedBroadPhaseHandlePair& pair)
{
}

// 0x00DB1170
void hkpNullBroadPhaseListener::removeCollisionPair(hkpTypedBroadPhaseHandlePair& pair)
{
}
