// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/util/symmetric/hkpsymmetricagentlinearcast.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0); the template is ported from the 2014.1 SDK
// (HK6). Hand-maintained.
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4f.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/query/hkpcdbodypaircollector.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/query/hkpcdpointcollector.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpprocesscollisionoutput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/query/hkplinearcastcollisioninput.h"

class hkpCdBody;
struct hkpCdPoint;
struct hkpCollisionInput;
class hkpConstraintOwner;
class hkpContactMgr;
struct hkpLinearCastCollisionInput;
struct hkpProcessCollisionInput;
struct hkpProcessCollisionOutput;

// size 0xC
class hkpSymmetricAgentFlipCollector : public hkpCdPointCollector
{
public:
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpSymmetricAgentFlipCollector); // delete(void* p, unsigned int nbytes) 0x00D78910
    hkpSymmetricAgentFlipCollector(hkpCdPointCollector& c); // 0x00D78410
    virtual ~hkpSymmetricAgentFlipCollector(); // vtbl+0x00 0x00D78430
    virtual void addCdPoint(const hkpCdPoint& point); // vtbl+0x04 0x00D9CE60

    hkpCdPointCollector& m_collector; // 0x08
};

// size 0x30
class alignas(16) hkpSymmetricAgentFlipCastCollector : public hkpCdPointCollector
{
public:
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpSymmetricAgentFlipCastCollector); // delete(void* p, unsigned int nbytes) 0x00D78970
    hkpSymmetricAgentFlipCastCollector(const hkVector4f& path, hkpCdPointCollector& c); // 0x00D78440
    virtual ~hkpSymmetricAgentFlipCastCollector(); // vtbl+0x00 0x00D78480
    virtual void addCdPoint(const hkpCdPoint& point); // vtbl+0x04 0x00D9CEE0

    alignas(16) hkVector4f m_path; // 0x10
    hkpCdPointCollector& m_collector; // 0x20
};

// size 0xC
class hkpSymmetricAgentFlipBodyCollector : public hkpCdBodyPairCollector
{
public:
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpSymmetricAgentFlipBodyCollector); // delete(void* p, unsigned int nbytes) 0x00D789D0
    hkpSymmetricAgentFlipBodyCollector(hkpCdBodyPairCollector& c); // 0x00D78490
    virtual ~hkpSymmetricAgentFlipBodyCollector(); // vtbl+0x00 0x00D784B0
    virtual void addCdBodyPair(const hkpCdBody& bodyA, const hkpCdBody& bodyB); // vtbl+0x04 0x00D9CE30

    hkpCdBodyPairCollector& m_collector; // 0x08
};

// size (<hkpBvTreeAgent>) 0x40
// Runs AGENT with bodies A and B swapped (AGENT handles the pair (B, A)) and flips the results back: points and
// manifolds move to the other body with the normal reversed, linear casts run along the reversed path. Ported from
// hkpSymmetricAgentLinearCast.inl of the 2014.1 SDK; rfg.exe has about 100 instances (havok_impl.md HK6).
template<typename AGENT>
class hkpSymmetricAgentLinearCast : public AGENT
{
public:
    hkpSymmetricAgentLinearCast(const hkpCdBody& A, const hkpCdBody& B, const hkpCollisionInput& input, hkpContactMgr* mgr)
        : AGENT(B, A, input, mgr)
    {
    }

    // <hkpBvTreeAgent> 0x00D795F0
    hkpSymmetricAgentLinearCast(hkpContactMgr* mgr)
        : AGENT(mgr)
    {
    }

    // vtbl+0x00 <hkpBvTreeAgent> 0x00D795A0: implicit

    // <hkpBvTreeAgent> 0x00D7AB20
    static void staticGetPenetrations(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdBodyPairCollector& collector)
    {
        hkpSymmetricAgentFlipBodyCollector flip(collector);
        AGENT::staticGetPenetrations(bodyB, bodyA, input, flip);
    }

    // vtbl+0x10 <hkpBvTreeAgent> 0x00D7ACC0
    virtual void getPenetrations(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdBodyPairCollector& collector)
    {
        hkpSymmetricAgentFlipBodyCollector flip(collector);
        AGENT::getPenetrations(bodyB, bodyA, input, flip);
    }

    // <hkpBvTreeAgent> 0x00D78EC0
    static void staticGetClosestPoints(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdPointCollector& collector)
    {
        hkpSymmetricAgentFlipCollector flip(collector);
        AGENT::staticGetClosestPoints(bodyB, bodyA, input, flip);
    }

    // <hkpBvTreeAgent> 0x00D7AB60
    static void staticLinearCast(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpLinearCastCollisionInput& input, hkpCdPointCollector& collector, hkpCdPointCollector* startCollector)
    {
        hkpLinearCastCollisionInput flippedInput = input;
        flippedInput.m_path.setNeg<4>(input.m_path);
        hkpSymmetricAgentFlipCastCollector flip(input.m_path, collector);
        if (startCollector != nullptr)
        {
            hkpSymmetricAgentFlipCollector startFlip(*startCollector);
            AGENT::staticLinearCast(bodyB, bodyA, flippedInput, flip, &startFlip);
        }
        else
        {
            AGENT::staticLinearCast(bodyB, bodyA, flippedInput, flip, nullptr);
        }
    }

    // vtbl+0x14 <hkpBvTreeAgent> 0x00D797A0
    virtual void getClosestPoints(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdPointCollector& collector)
    {
        hkpSymmetricAgentFlipCollector flip(collector);
        AGENT::getClosestPoints(bodyB, bodyA, input, flip);
    }

    // vtbl+0x1C <hkpBvTreeAgent> 0x00D7AC30
    // The new contact points are flipped in place; a new TOI too.
    virtual void processCollision(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpProcessCollisionInput& input, hkpProcessCollisionOutput& result)
    {
        hkpProcessCdPoint* pp = result.m_firstFreeContactPoint;
        float oldToi = result.m_toi.m_time;
        AGENT::processCollision(bodyB, bodyA, input, result);
        for (; pp < result.m_firstFreeContactPoint.val(); pp++)
        {
            pp->m_contact.setFlipped(pp->m_contact);
        }
        if (oldToi != result.m_toi.m_time.val())
        {
            result.m_toi.flip();
        }
    }

    // vtbl+0x24 <hkpBvTreeAgent> 0x00D79780
    virtual void updateShapeCollectionFilter(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpConstraintOwner& constraintOwner)
    {
        AGENT::updateShapeCollectionFilter(bodyB, bodyA, input, constraintOwner);
    }
};
