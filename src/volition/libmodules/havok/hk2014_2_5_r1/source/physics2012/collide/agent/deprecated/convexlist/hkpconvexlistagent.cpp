// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/deprecated/convexlist/hkpconvexlistagent.cpp
//
// Havok 2014.2.5 (havok_impl.md HK6, stub_impl.md S4): the (deprecated) convex list agent, from rfg.exe. A convex list
// is a convex hull over child convex shapes: the agent runs the predictive GSK against the hull ("GSK mode") and
// switches to a 1n agent track over the children ("stream mode") when a point lands on two children or the hull
// penetrates deeper than twice the allowed penetration; in stream mode it checks the hull again every 25 frames.
// The list is always B (the symmetric agent swaps a list A).
//
// The shipped build records the "CvxLst"/"CvxList"/"CvsListAgent" monitor timers (TLS hkMonitorStream); they are not
// reproduced. No RFG data has a convex list shape (hkpConvexListShape is not ported), so nothing creates this agent.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/deprecated/convexlist/hkpconvexlistagent.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/swepttransform/hkswepttransformutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/router/hkmemoryrouter.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/physics/motionstate/hkmotionstate.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcdbody.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcdpoint.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcollidable.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/compoundagent/shapecollection/hkpshapecollectionagent.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/deprecated/convexlist/hkpconvexlistutils.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpcollisioninput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpprocesscollisioninput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpprocesscollisionoutput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/util/linearcast/hkpiterativelinearcastagent.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/util/symmetric/hkpsymmetricagent.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/util/symmetric/hkpsymmetricagentlinearcast.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/hkpagent3.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/machine/1n/hkpagent1nmachine.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/hkpcollisiondispatcher.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/filter/hkpconvexlistfilter.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/query/collector/bodypaircollector/hkpflagcdbodypaircollector.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/query/collector/pointcollector/hkpclosestcdpointcollector.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/query/collector/pointcollector/hkpsimpleclosestcontactcollector.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/deprecated/convexlist/hkpconvexlistshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/gskmanifold/hkpgskmanifoldutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/hkpgsk.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/hkpgskexact.h"

#include <new>

using namespace hkGskExact;

// in stream mode the hull is checked again after this many frames
static const short HK_CONVEX_LIST_STREAM_MODE_FRAMES = 25;

// 0x00DB6F80
void hkpConvexListAgent::registerAgent(hkpCollisionDispatcher* dispatcher)
{
    hkpCollisionDispatcher::AgentFuncs af;
    af.m_createFunc = createConvexListConvexAgent;
    af.m_getPenetrationsFunc = hkpSymmetricAgentLinearCast<hkpConvexListAgent>::staticGetPenetrations;
    af.m_getClosestPointFunc = hkpSymmetricAgentLinearCast<hkpConvexListAgent>::staticGetClosestPoints;
    af.m_linearCastFunc = hkpSymmetricAgentLinearCast<hkpConvexListAgent>::staticLinearCast;
    af.m_isFlipped = true;
    af.m_isPredictive = true;
    dispatcher->registerCollisionAgent(af, hkcdShapeType::CONVEX_LIST, hkcdShapeType::CONVEX);

    af.m_createFunc = createConvexConvexListAgent;
    af.m_getPenetrationsFunc = staticGetPenetrations;
    af.m_getClosestPointFunc = staticGetClosestPoints;
    af.m_linearCastFunc = staticLinearCast;
    af.m_isFlipped = false;
    af.m_isPredictive = true;
    dispatcher->registerCollisionAgent(af, hkcdShapeType::CONVEX, hkcdShapeType::CONVEX_LIST);

    af.m_createFunc = createConvexListConvexListAgent;
    af.m_getPenetrationsFunc = staticGetPenetrations;
    af.m_getClosestPointFunc = staticGetClosestPoints;
    af.m_linearCastFunc = staticLinearCast;
    af.m_isFlipped = false;
    af.m_isPredictive = true;
    dispatcher->registerCollisionAgent(af, hkcdShapeType::CONVEX_LIST, hkcdShapeType::CONVEX_LIST);
}

// 0x00DB5E40
void hkpConvexListAgent::invalidateTim(const hkpCollisionInput& input)
{
    if (m_inGskMode)
    {
        hkpGskBaseAgent::invalidateTim(input);
    }
    else
    {
        hkAgent1nMachine_InvalidateTim(getStream().m_agentTrack, input);
    }
}

// 0x00DB5E70
void hkpConvexListAgent::warpTime(float oldTime, float newTime, const hkpCollisionInput& input)
{
    if (m_inGskMode)
    {
        hkpGskBaseAgent::warpTime(oldTime, newTime, input);
    }
    else
    {
        hkAgent1nMachine_WarpTime(getStream().m_agentTrack, oldTime, newTime, input);
    }
}

// 0x00DB5ED0: the stream's agents keep their own points
void hkpConvexListAgent::removePoint(unsigned short idToRemove)
{
    if (m_inGskMode)
    {
        hkpGskfAgent::removePoint(idToRemove);
    }
}

// 0x00DB5EF0
void hkpConvexListAgent::commitPotential(unsigned short idToCommit)
{
    if (m_inGskMode)
    {
        hkpGskfAgent::commitPotential(idToCommit);
    }
}

// 0x00DB5F10
void hkpConvexListAgent::createZombie(unsigned short idTobecomeZombie)
{
    if (m_inGskMode)
    {
        hkpGskfAgent::createZombie(idTobecomeZombie);
    }
}

// 0x00DB5F30: the stream's agents go, the manifold starts empty
void hkpConvexListAgent::switchToGskMode(hkpConstraintOwner& constraintOwner)
{
    hkAgent1nMachine_Destroy(getStream().m_agentTrack, m_dispatcher, m_contactMgr, constraintOwner);
    m_manifold.init();
    m_inGskMode = true;
}

// 0x00DB5F70
void hkpConvexListAgent::cleanup(hkpConstraintOwner& constraintOwner)
{
    if (m_inGskMode)
    {
        hkGskManifold_cleanup(m_manifold, m_contactMgr, constraintOwner);
    }
    else
    {
        hkAgent1nMachine_Destroy(getStream().m_agentTrack, m_dispatcher, m_contactMgr, constraintOwner);
    }
    delete this;
}

// 0x00DB5FC0: the GSK cache is set up again with the list's hull (the base did it for the same shapes)
hkpConvexListAgent::hkpConvexListAgent(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* mgr)
    : hkpPredGskfAgent(bodyA, bodyB, input, mgr)
{
    m_dispatcher = input.m_dispatcher.val();
    m_inGskMode = true;
    m_processFunctionCalled = false;
    // m_inStreamModeCounter is set when the agent switches to stream mode
    const hkpConvexShape* shapeA = static_cast<const hkpConvexShape*>(bodyA.getShape());
    const hkpConvexShape* shapeB = static_cast<const hkpConvexShape*>(bodyB.getShape());
    alignas(16) hkTransformf aTb;
    aTb.setMulInverseMul(bodyA.getTransform(), bodyB.getTransform());
    m_cache.init(shapeA, shapeB, aTb);
    m_separatingNormal.m_quad.m128_f32[3] = -1.0f;
    m_timeOfSeparatingNormal = -1.0f;
}

// 0x00DB71A0
hkpConvexListAgent::~hkpConvexListAgent()
{
}

// 0x00DB6050: only the stream has child agents to filter
void hkpConvexListAgent::updateShapeCollectionFilter(const hkpCdBody& bodyA, const hkpCdBody& listShapeBodyB, const hkpCollisionInput& input, hkpConstraintOwner& constraintOwner)
{
    if (!m_inGskMode)
    {
        hkpAgent1nMachine_VisitorInput vin;
        vin.m_contactMgr = m_contactMgr;
        vin.m_input = &input;
        vin.m_bodyA = &bodyA;
        vin.m_constraintOwner = &constraintOwner;
        vin.m_collectionBodyB = &listShapeBodyB;
        vin.m_containerShapeB = listShapeBodyB.getShape()->getContainer();
        hkAgent1nMachine_UpdateShapeCollectionFilter(getStream().m_agentTrack, vin);
    }
}

// 0x00DB60B0: the hull's closest point when it is far enough (the list's threshold), else the children's
void hkpConvexListAgent::staticGetClosestPoints(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdPointCollector& collector)
{
    {
        hkpFlagCdBodyPairCollector checker;
        hkpGskBaseAgent::staticGetPenetrations(bodyA, bodyB, input, checker);
        if (checker.hasHit())
        {
            goto getClosestPointsOfChildren;
        }
    }
    {
        hkpClosestCdPointCollector closestPoint;
        hkpGskBaseAgent::staticGetClosestPoints(bodyA, bodyB, input, closestPoint);
        if (!closestPoint.hasHit())
        {
            return;
        }
        const hkpConvexListShape* cls = static_cast<const hkpConvexListShape*>(bodyB.getShape());
        if (closestPoint.getHitContact().getDistance() > cls->m_minDistanceToUseConvexHullForGetClosestPoints)
        {
            hkpCdPoint event(bodyA, bodyB, closestPoint.getHitContact());
            collector.addCdPoint(event);
            return;
        }
    }
getClosestPointsOfChildren:
    hkpSymmetricAgentLinearCast<hkpShapeCollectionAgent>::staticGetClosestPoints(bodyA, bodyB, input, collector);
}

// 0x00DB6270
void hkpConvexListAgent::getClosestPoints(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdPointCollector& collector)
{
    staticGetClosestPoints(bodyA, bodyB, input, collector);
}

// 0x00DB6290: the children only when the hull penetrates
void hkpConvexListAgent::staticGetPenetrations(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdBodyPairCollector& collector)
{
    hkpFlagCdBodyPairCollector checker;
    hkpGskBaseAgent::staticGetPenetrations(bodyA, bodyB, input, checker);
    if (checker.hasHit())
    {
        hkpSymmetricAgentLinearCast<hkpShapeCollectionAgent>::staticGetPenetrations(bodyA, bodyB, input, collector);
    }
}

// 0x00DB6380
void hkpConvexListAgent::getPenetrations(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdBodyPairCollector& collector)
{
    staticGetPenetrations(bodyA, bodyB, input, collector);
}

// 0x00DB63A0: the children only when the cast hits the hull
void hkpConvexListAgent::staticLinearCast(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpLinearCastCollisionInput& input, hkpCdPointCollector& collector, hkpCdPointCollector* startCollector)
{
    hkpSimpleClosestContactCollector checker;
    hkpIterativeLinearCastAgent::staticLinearCast(bodyA, bodyB, input, checker, &checker);
    if (checker.hasHit())
    {
        hkpSymmetricAgentLinearCast<hkpShapeCollectionAgent>::staticLinearCast(bodyA, bodyB, input, collector, startCollector);
    }
}

// 0x00DB64C0
void hkpConvexListAgent::linearCast(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpLinearCastCollisionInput& input, hkpCdPointCollector& collector, hkpCdPointCollector* startCollector)
{
    staticLinearCast(bodyA, bodyB, input, collector, startCollector);
}

// 0x00DB64F0: the list is A; the filter picks hull, children or plain convex
hkpCollisionAgent* hkpConvexListAgent::createConvexListConvexAgent(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* mgr)
{
    if (mgr != nullptr)
    {
        switch (input.m_convexListFilter->getConvexListCollisionType(bodyA, bodyB, input))
        {
        case hkpConvexListFilter::TREAT_CONVEX_LIST_AS_NORMAL:
            return new hkpSymmetricAgent<hkpConvexListAgent>(bodyA, bodyB, input, mgr);
        case hkpConvexListFilter::TREAT_CONVEX_LIST_AS_LIST:
            return new hkpShapeCollectionAgent(bodyA, bodyB, input, mgr);
        case hkpConvexListFilter::TREAT_CONVEX_LIST_AS_CONVEX:
            return new hkpPredGskfAgent(bodyA, bodyB, input, mgr);
        }
        return nullptr;
    }
    return new hkpShapeCollectionAgent(bodyA, bodyB, input, mgr);
}

// 0x00DB6620: the manifold's points go; the stream starts empty with a TIM distance of 0
void hkpConvexListAgent::switchToStreamMode(hkpConstraintOwner& constraintOwner)
{
    hkGskManifold_cleanup(m_manifold, m_contactMgr, constraintOwner);
    m_inGskMode = false;
    new (&getStream().m_agentTrack) hkpAgent1nTrack();
    hkAgent1nMachine_Create(getStream().m_agentTrack);
    m_inStreamModeCounter = HK_CONVEX_LIST_STREAM_MODE_FRAMES;
    getStream().m_inStreamModeTimDist = 0.0f;
}

// 0x00DB6680
void hkpConvexListAgent::processCollision(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpProcessCollisionInput& input, hkpProcessCollisionOutput& output)
{
    const hkpConvexListShape* cls = static_cast<const hkpConvexListShape*>(bodyB.getShape());

    alignas(16) hkVector4f timInfo;
    hkSweptTransformUtil::calcTimInfo(*bodyA.getMotionState(), *bodyB.getMotionState(), input.m_stepInfo.m_deltaTime, timInfo);

    hkpProcessCollisionOutputBackup outputBackup(output);

    if (m_inGskMode)
    {
    gskMode:
        if (m_separatingNormal.m_quad.m128_f32[3] > input.m_tolerance.val())
        {
            // n.linear + angular, the four lanes summed as (x + z) + (y + w); the subtraction is x87 (exact in float)
            const Q sep = m_separatingNormal.m_quad;
            const Q t = setXYZ_W(mul(sep, timInfo.m_quad), timInfo.m_quad);
            const Q pairs = _mm_add_ps(_mm_shuffle_ps(t, t, 0x4E), t);
            const Q sum = _mm_add_ps(_mm_shuffle_ps(pairs, pairs, 0xB1), pairs);
            m_separatingNormal.m_quad.m128_f32[3] = m_separatingNormal.m_quad.m128_f32[3] - lane0(sum);
            if (m_separatingNormal.m_quad.m128_f32[3] > input.m_tolerance.val())
            {
                if (m_manifold.m_numContactPoints != 0)
                {
                    hkGskManifold_cleanup(m_manifold, m_contactMgr, *output.m_constraintOwner.val());
                }
                goto end;
            }
        }
        {
            hkpMapPointsToSubShapeContactMgr mgr(m_contactMgr);
            m_contactMgr = &mgr;
            hkpPredGskfAgent::processCollision(bodyA, bodyB, input, output);
            m_contactMgr = mgr.m_contactMgr;
            if (!mgr.m_invalidPointHit)
            {
                if (m_manifold.m_numContactPoints == 0 || m_processFunctionCalled)
                {
                    goto end;
                }
                // a new pair penetrating deeper than twice the allowed depth goes to the children
                const float a = bodyA.getRootCollidable()->m_allowedPenetrationDepth;
                const float b = bodyB.getRootCollidable()->m_allowedPenetrationDepth;
                const float allowedPenetration = (a < b) ? a : b; // minss
                if (!(-(allowedPenetration + allowedPenetration) > m_separatingNormal.m_quad.m128_f32[3])) // x87 fcomi, jbe
                {
                    goto end;
                }
            }
            outputBackup.rollbackOutput(bodyA, bodyB, output, m_contactMgr);
            switchToStreamMode(*output.m_constraintOwner.val());
        }
    }

    // stream mode
    if (m_inStreamModeCounter-- < 0)
    {
        m_inStreamModeCounter = HK_CONVEX_LIST_STREAM_MODE_FRAMES;
        alignas(16) hkTransformf aTb;
        aTb.setMulInverseMul(bodyA.getTransform(), bodyB.getTransform());
        hkpGsk::GetClosesetPointInput gskInput;
        gskInput.m_shapeA = static_cast<const hkpConvexShape*>(bodyA.getShape());
        gskInput.m_shapeB = static_cast<const hkpConvexShape*>(bodyB.getShape());
        gskInput.m_transformA = &bodyA.getTransform();
        gskInput.m_aTb = &aTb;
        gskInput.m_collisionTolerance = input.m_tolerance.val();
        alignas(16) hkVector4f pointOnB;
        if (hkpGsk::getClosestPoint(gskInput, m_cache, m_separatingNormal, pointOnB) != HK_SUCCESS)
        {
            // no closest point of the hull: back to the GSK
            switchToGskMode(*output.m_constraintOwner.val());
            outputBackup.rollbackOutput(bodyA, bodyB, output, m_contactMgr);
            goto gskMode;
        }
        getStream().m_inStreamModeTimDist = -m_separatingNormal.m_quad.m128_f32[3];
    }
    {
        // the TIM distance shrinks by the step's linear movement (|timInfo.xyz|, rsqrt with one Newton step, 0 for
        // a zero vector); the subtraction is x87 (exact in float)
        const Q len2 = hadd3(mul(timInfo.m_quad, timInfo.m_quad));
        const Q len = _mm_andnot_ps(_mm_cmple_ps(len2, _mm_setzero_ps()), mul(invSqrt23(len2), len2));
        getStream().m_inStreamModeTimDist = getStream().m_inStreamModeTimDist - lane0(len);

        hkpAgent3ProcessInput in3;
        in3.m_bodyA = &bodyA;
        in3.m_bodyB = &bodyB;
        in3.m_overrideBodyA = nullptr;
        in3.m_contactMgr = m_contactMgr;
        in3.m_input = &input;
        in3.m_linearTimInfo = timInfo;
        in3.m_aTb.setMulInverseMul(bodyA.getTransform(), bodyB.getTransform());

        // every child, then HK_INVALID_SHAPE_KEY
        const int size = cls->m_childShapes.getSize();
        hkLifoAllocator& stack = hkMemoryRouter::getInstance().stack();
        const int bytes = (size * (int)sizeof(unsigned int) + (int)sizeof(unsigned int) + 127) & ~127; // hkAllocateStack: 128-byte multiples
        unsigned int* hitList = static_cast<unsigned int*>(stack.fastBlockAlloc(bytes));
        for (int i = 0; i < size; i++)
        {
            hitList[i] = (unsigned int)i;
        }
        hitList[size] = 0xFFFFFFFF; // HK_INVALID_SHAPE_KEY
        hkAgent1nMachine_Process(getStream().m_agentTrack, in3, cls, hitList, output);
        stack.fastBlockFree(hitList, bytes);
    }

end:
    m_processFunctionCalled = true;
}

// 0x00DB6C80: the list is B
hkpCollisionAgent* hkpConvexListAgent::createConvexConvexListAgent(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* mgr)
{
    if (mgr != nullptr)
    {
        switch (input.m_convexListFilter->getConvexListCollisionType(bodyB, bodyA, input))
        {
        case hkpConvexListFilter::TREAT_CONVEX_LIST_AS_NORMAL:
            return new hkpConvexListAgent(bodyA, bodyB, input, mgr);
        case hkpConvexListFilter::TREAT_CONVEX_LIST_AS_LIST:
            return new hkpSymmetricAgent<hkpShapeCollectionAgent>(bodyA, bodyB, input, mgr);
        case hkpConvexListFilter::TREAT_CONVEX_LIST_AS_CONVEX:
            return new hkpPredGskfAgent(bodyA, bodyB, input, mgr);
        }
        return nullptr;
    }
    return new hkpSymmetricAgent<hkpShapeCollectionAgent>(bodyA, bodyB, input, mgr);
}

// 0x00DB6DC0: A's treatment first; a list A treated as convex hands the choice to B
hkpCollisionAgent* hkpConvexListAgent::createConvexListConvexListAgent(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* mgr)
{
    if (mgr == nullptr)
    {
        return new hkpShapeCollectionAgent(bodyA, bodyB, input, mgr);
    }
    switch (input.m_convexListFilter->getConvexListCollisionType(bodyA, bodyB, input))
    {
    case hkpConvexListFilter::TREAT_CONVEX_LIST_AS_NORMAL:
        return new hkpSymmetricAgent<hkpConvexListAgent>(bodyA, bodyB, input, mgr);
    case hkpConvexListFilter::TREAT_CONVEX_LIST_AS_LIST:
        return new hkpShapeCollectionAgent(bodyA, bodyB, input, mgr);
    case hkpConvexListFilter::TREAT_CONVEX_LIST_AS_CONVEX:
        switch (input.m_convexListFilter->getConvexListCollisionType(bodyB, bodyA, input))
        {
        case hkpConvexListFilter::TREAT_CONVEX_LIST_AS_NORMAL:
            return new hkpConvexListAgent(bodyA, bodyB, input, mgr);
        case hkpConvexListFilter::TREAT_CONVEX_LIST_AS_LIST:
            return new hkpSymmetricAgent<hkpShapeCollectionAgent>(bodyA, bodyB, input, mgr);
        case hkpConvexListFilter::TREAT_CONVEX_LIST_AS_CONVEX:
            return new hkpPredGskfAgent(bodyA, bodyB, input, mgr);
        }
        break;
    }
    return nullptr;
}

// 0x00DB7050: the stream data lives in the manifold's storage
hkpConvexListAgent::StreamData& hkpConvexListAgent::getStream()
{
    return *reinterpret_cast<StreamData*>(&m_manifold);
}
