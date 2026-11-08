// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/util/null/hkpnullagent.cpp
//
// Havok 2014.2.5 (havok_impl.md HK6): the null agent, from rfg.exe. Pairs without an agent get the one shared
// instance; it finds nothing and keeps no state.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/util/null/hkpnullagent.h"

// 0x03A28818 (hkNullAgentInstance): the shared instance (built by a dynamic initializer).
static hkpNullAgent hkNullAgentInstance;

// 0x00D9CFB0
hkpNullAgent::hkpNullAgent()
    : hkpCollisionAgent(nullptr)
{
}

// 0x00D9CFD0
hkpNullAgent::~hkpNullAgent()
{
}

// 0x00D9CFE0
void hkpNullAgent::getPenetrations(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdBodyPairCollector& collector)
{
}

// 0x00D9CF60
void hkpNullAgent::staticGetClosestPoints(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdPointCollector& collector)
{
}

// 0x00D9CF70
void hkpNullAgent::staticGetPenetrations(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdBodyPairCollector& collector)
{
}

// 0x00D9CFF0
void hkpNullAgent::getClosestPoints(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdPointCollector& pointDetails)
{
}

// 0x00D9CF80
void hkpNullAgent::staticLinearCast(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpLinearCastCollisionInput& input, hkpCdPointCollector& collector, hkpCdPointCollector* startCollector)
{
}

// 0x00D9D000
void hkpNullAgent::linearCast(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpLinearCastCollisionInput& input, hkpCdPointCollector& collector, hkpCdPointCollector* startCollector)
{
}

// 0x00D9D010
void hkpNullAgent::processCollision(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpProcessCollisionInput& input, hkpProcessCollisionOutput& result)
{
}

// 0x00D9CF90
hkpCollisionAgent* hkpNullAgent::createNullAgent(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* mgr)
{
    return &hkNullAgentInstance;
}

// 0x00D9CFA0
hkpNullAgent* hkpNullAgent::getNullAgent()
{
    return &hkNullAgentInstance;
}

// 0x00D9D020
void hkpNullAgent::cleanup(hkpConstraintOwner& info)
{
}
