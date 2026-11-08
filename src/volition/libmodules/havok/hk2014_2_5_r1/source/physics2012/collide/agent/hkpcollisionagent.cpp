// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpcollisionagent.cpp
//
// Havok 2014.2.5 (havok_impl.md HK6): the default virtuals of the collision agent base, from rfg.exe. Agents without
// tims, potential contacts or zombies keep them.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpcollisionagent.h"

// 0x03A28814: set by the hkpHeightFieldAgent optional component; registerAllAgents calls it when set.
void (*hkpCollisionAgent::registerHeightFieldAgent)(hkpCollisionDispatcher*) = nullptr;

// 0x00D9CDE0
void hkpCollisionAgent::invalidateTim(const hkpCollisionInput& input)
{
}

// 0x00D9CDF0
void hkpCollisionAgent::warpTime(float oldTime, float newTime, const hkpCollisionInput& input)
{
}

// 0x00D9CE00
void hkpCollisionAgent::removePoint(unsigned short idToRemove)
{
}

// 0x00D9CE10
void hkpCollisionAgent::commitPotential(unsigned short newId)
{
}

// 0x00D9CE20
void hkpCollisionAgent::createZombie(unsigned short idTobecomeZombie)
{
}
