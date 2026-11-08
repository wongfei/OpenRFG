// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/convexlist3/hkpconvexlistagent3.cpp
//
// Havok 2014.2.5 (havok_impl.md HK6, stub_impl.md S4): the convex list agent3, from rfg.exe; the agent machine version
// of hkpConvexListAgent (deprecated/convexlist). Its data starts with the GSK cache, whose flags byte holds the mode
// (GSK_FLAGS_CONVEX_LIST_IN_GSK_MODE) and whether process ran (GSK_FLAGS_PROCESS_FUNCTION_CALLED). In GSK mode the rest
// is the predictive GSK agent3's manifold; in stream mode a frame counter (+0x0C) and a 1n agent track over the
// children (+0x10).
//
// The shipped build records the "CvxLst" monitor timers (TLS hkMonitorStream); they are not reproduced. No RFG data
// has a convex list shape (hkpConvexListShape is not ported), so nothing creates this agent.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/convexlist3/hkpconvexlistagent3.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/hkmath.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/router/hkmemoryrouter.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcdbody.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcollidable.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/deprecated/convexlist/hkpconvexlistutils.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpcollisioninput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpprocesscollisioninput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpprocesscollisionoutput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/hkpagent3.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/machine/1n/hkpagent1nmachine.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/machine/1n/hkpagent1ntrack.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/predgskagent3/hkppredgskagent3.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/deprecated/convexlist/hkpconvexlistshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/gskmanifold/hkpgskmanifold.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/hkpgsk.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/hkpgskcache.h"

#include <new>

// in stream mode the hull is checked again after this many frames
static const short HK_CONVEX_LIST_STREAM_MODE_FRAMES = 25;

static inline hkpGskCache& hkConvexListAgent3_cache(void* agentData)
{
    return *static_cast<hkpGskCache*>(agentData);
}

static inline bool hkConvexListAgent3_inGskMode(const void* agentData)
{
    return (static_cast<const hkpGskCache*>(agentData)->m_gskFlags & hkpGskCache::GSK_FLAGS_CONVEX_LIST_IN_GSK_MODE) != 0;
}

// the stream's frame counter
static inline short& hkConvexListAgent3_counter(void* agentData)
{
    return *reinterpret_cast<short*>(static_cast<char*>(agentData) + 0xC);
}

static inline hkpAgent1nTrack& hkConvexListAgent3_track(void* agentData)
{
    return *reinterpret_cast<hkpAgent1nTrack*>(static_cast<char*>(agentData) + 0x10);
}

// the GSK mode's manifold (the predictive GSK agent3's data after the cache)
static inline hkpGskManifold& hkConvexListAgent3_manifold(void* agentData)
{
    return *reinterpret_cast<hkpGskManifold*>(static_cast<char*>(agentData) + 0xC);
}

// switchToGskMode (0x00DB5960, inlined): the stream's agents go, the manifold starts empty
static inline void hkConvexListAgent3_switchToGskMode(const hkpAgent3ProcessInput& input, void* agentData, hkpProcessCollisionOutput& output)
{
    hkAgent1nMachine_Destroy(hkConvexListAgent3_track(agentData), input.m_input->m_dispatcher, input.m_contactMgr, *output.m_constraintOwner.val());
    hkConvexListAgent3_cache(agentData).m_gskFlags |= hkpGskCache::GSK_FLAGS_CONVEX_LIST_IN_GSK_MODE;
    hkConvexListAgent3_manifold(agentData).init();
}

// switchToStreamMode (0x00DB5D90, inlined): the manifold's points go, the stream starts empty
static inline void hkConvexListAgent3_switchToStreamMode(const hkpAgent3ProcessInput& input, hkpAgentEntry* entry, void* agentData, hkpProcessCollisionOutput& output)
{
    hkPredGskAgent3::cleanup(entry, agentData, input.m_contactMgr, *output.m_constraintOwner.val());
    hkConvexListAgent3_cache(agentData).m_gskFlags &= ~hkpGskCache::GSK_FLAGS_CONVEX_LIST_IN_GSK_MODE;
    new (&hkConvexListAgent3_track(agentData)) hkpAgent1nTrack();
    hkAgent1nMachine_Create(hkConvexListAgent3_track(agentData));
    hkConvexListAgent3_counter(agentData) = HK_CONVEX_LIST_STREAM_MODE_FRAMES;
}

// 0x00DB5580
void hkConvexListAgent3::initAgentFunc(hkpCollisionDispatcher::Agent3Funcs& f)
{
    f.m_createFunc = create;
    f.m_processFunc = process;
    f.m_sepNormalFunc = sepNormal;
    f.m_cleanupFunc = cleanup;
    f.m_removePointFunc = removePoint;
    f.m_commitPotentialFunc = commitPotential;
    f.m_createZombieFunc = createZombie;
    f.m_updateFilterFunc = updateFilter;
    f.m_invalidateTimFunc = invalidateTim;
    f.m_warpTimeFunc = warpTime;
    f.m_destroyFunc = destroy;
    f.m_isPredictive = true;
}

// 0x00DB55E0
void hkConvexListAgent3::registerAgent3(hkpCollisionDispatcher* dispatcher)
{
    hkpCollisionDispatcher::Agent3Funcs f;
    initAgentFunc(f);
    dispatcher->registerAgent3(f, hkcdShapeType::CONVEX, hkcdShapeType::CONVEX_LIST);
    dispatcher->registerAgent3(f, hkcdShapeType::CONVEX_LIST, hkcdShapeType::CONVEX_LIST);
}

// 0x00DB4F10
void hkConvexListAgent3::invalidateTim(hkpAgentEntry* entry, void* agentData, const hkpCollisionInput& input)
{
    if (!hkConvexListAgent3_inGskMode(agentData))
    {
        hkAgent1nMachine_InvalidateTim(hkConvexListAgent3_track(agentData), input);
    }
}

// 0x00DB4F30
void hkConvexListAgent3::warpTime(hkpAgentEntry* entry, void* agentData, float oldTime, float newTime, const hkpCollisionInput& input)
{
    if (!hkConvexListAgent3_inGskMode(agentData))
    {
        hkAgent1nMachine_WarpTime(hkConvexListAgent3_track(agentData), oldTime, newTime, input);
    }
}

// 0x00DB4F70: a new pair starts in GSK mode
void* hkConvexListAgent3::create(const hkpAgent3Input& input, hkpAgentEntry* entry, void* agentData)
{
    void* end = hkPredGskAgent3::create(input, entry, agentData);
    hkpGskCache& cache = hkConvexListAgent3_cache(agentData);
    cache.m_gskFlags = (unsigned char)((cache.m_gskFlags & ~hkpGskCache::GSK_FLAGS_PROCESS_FUNCTION_CALLED) | hkpGskCache::GSK_FLAGS_CONVEX_LIST_IN_GSK_MODE);
    return end;
}

// 0x00DB4FA0
void hkConvexListAgent3::destroy(hkpAgentEntry* entry, void* agentData, hkpContactMgr* mgr, hkpConstraintOwner& constraintOwner, hkpCollisionDispatcher* dispatcher)
{
    if (hkConvexListAgent3_inGskMode(agentData))
    {
        hkPredGskAgent3::destroy(entry, agentData, mgr, constraintOwner, dispatcher);
    }
    else
    {
        hkAgent1nMachine_Destroy(hkConvexListAgent3_track(agentData), dispatcher, mgr, constraintOwner);
    }
}

// 0x00DB4FD0: Original bug kept: the predictive GSK cleanup runs in either mode; in stream mode it reads the frame
// counter and the track as a manifold
void* hkConvexListAgent3::cleanup(hkpAgentEntry* entry, void* agentData, hkpContactMgr* mgr, hkpConstraintOwner& constraintOwner)
{
    return hkPredGskAgent3::cleanup(entry, agentData, mgr, constraintOwner);
}

// 0x00DB4FE0: the stream's agents keep their own points
void hkConvexListAgent3::removePoint(hkpAgentEntry* entry, void* agentData, unsigned short idToRemove)
{
    if (hkConvexListAgent3_inGskMode(agentData))
    {
        hkPredGskAgent3::removePoint(entry, agentData, idToRemove);
    }
}

// 0x00DB5000
void hkConvexListAgent3::commitPotential(hkpAgentEntry* entry, void* agentData, unsigned short idToCommit)
{
    if (hkConvexListAgent3_inGskMode(agentData))
    {
        hkPredGskAgent3::commitPotential(entry, agentData, idToCommit);
    }
}

// 0x00DB5020
void hkConvexListAgent3::createZombie(hkpAgentEntry* entry, void* agentData, unsigned short idToConvert)
{
    if (hkConvexListAgent3_inGskMode(agentData))
    {
        hkPredGskAgent3::createZombie(entry, agentData, idToConvert);
    }
}

// 0x00DB5040: only the stream has child agents to filter
void hkConvexListAgent3::updateFilter(hkpAgentEntry* entry, void* agentData, const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* mgr, hkpConstraintOwner& constraintOwner)
{
    if (!hkConvexListAgent3_inGskMode(agentData))
    {
        hkpAgent1nMachine_VisitorInput vin;
        vin.m_containerShapeB = bodyB.getShape()->getContainer();
        vin.m_bodyA = &bodyA;
        vin.m_input = &input;
        vin.m_collectionBodyB = &bodyB;
        vin.m_contactMgr = mgr;
        vin.m_constraintOwner = &constraintOwner;
        hkAgent1nMachine_UpdateShapeCollectionFilter(hkConvexListAgent3_track(agentData), vin);
    }
}

// 0x00DB50A0: in stream mode a dummy normal (1000, 1000, 1000) with the distance -HK_REAL_MAX / 2
void hkConvexListAgent3::sepNormal(const hkpAgent3Input& input, hkpAgentEntry* entry, void* agentData, hkVector4f& separatingNormalOut)
{
    if (hkConvexListAgent3_inGskMode(agentData))
    {
        hkPredGskAgent3::sepNormal(input, entry, agentData, separatingNormalOut);
        return;
    }
    const __m128 xyz = g_vectorfConstants[HK_QUADREAL_1000];
    const __m128 w = _mm_mul_ps(g_vectorfConstants[HK_QUADREAL_INV_2], g_vectorfConstants[HK_QUADREAL_MINUS_MAX]);
    separatingNormalOut.m_quad = _mm_shuffle_ps(xyz, _mm_unpackhi_ps(xyz, w), _MM_SHUFFLE(3, 0, 1, 0));
}

// 0x00DB50E0
void* hkConvexListAgent3::process(const hkpAgent3ProcessInput& input, hkpAgentEntry* entry, void* agentData, hkVector4f* separatingNormal, hkpProcessCollisionOutput& output)
{
    const hkpConvexListShape* cls = static_cast<const hkpConvexListShape*>(input.m_bodyB->getShape());
    void* end;

    hkpProcessCollisionOutputBackup outputBackup(output);

    if (!hkConvexListAgent3_inGskMode(agentData))
    {
        goto streamMode;
    }

gskMode:
    {
        hkpAgent3ProcessInput modifiedInput = input;
        hkpMapPointsToSubShapeContactMgr mgr(input.m_contactMgr);
        modifiedInput.m_contactMgr = &mgr;
        end = hkPredGskAgent3::process(modifiedInput, entry, agentData, separatingNormal, output);
        if (!mgr.m_invalidPointHit)
        {
            if (hkConvexListAgent3_manifold(agentData).m_numContactPoints == 0 || (hkConvexListAgent3_cache(agentData).m_gskFlags & hkpGskCache::GSK_FLAGS_PROCESS_FUNCTION_CALLED) != 0)
            {
                goto done;
            }
            // a new pair penetrating deeper than twice the allowed depth goes to the children (rollback on NaN)
            const __m128 a = _mm_set1_ps(input.m_bodyA->getRootCollidable()->m_allowedPenetrationDepth);
            const __m128 b = _mm_set1_ps(input.m_bodyB->getRootCollidable()->m_allowedPenetrationDepth);
            const __m128 m = _mm_min_ps(a, b);
            const float limit = _mm_cvtss_f32(_mm_sub_ps(_mm_setzero_ps(), _mm_add_ps(m, m)));
            if (separatingNormal->m_quad.m128_f32[3] >= limit)
            {
                goto done;
            }
        }
        outputBackup.rollbackOutput(*input.m_bodyA, *input.m_bodyB, output, input.m_contactMgr);
        hkConvexListAgent3_switchToStreamMode(input, entry, agentData, output);
    }

streamMode:
    if (hkConvexListAgent3_counter(agentData)-- < 0)
    {
        hkConvexListAgent3_counter(agentData) = HK_CONVEX_LIST_STREAM_MODE_FRAMES;
        alignas(16) hkTransformf aTb;
        aTb.setMulInverseMul(input.m_bodyA->getTransform(), input.m_bodyB->getTransform());
        hkpGsk::GetClosesetPointInput gskInput;
        gskInput.m_shapeA = static_cast<const hkpConvexShape*>(input.m_bodyA->getShape());
        gskInput.m_shapeB = static_cast<const hkpConvexShape*>(input.m_bodyB->getShape());
        gskInput.m_aTb = &aTb;
        gskInput.m_transformA = &input.m_bodyA->getTransform();
        gskInput.m_collisionTolerance = input.m_input->m_tolerance.val();
        alignas(16) hkVector4f pointOnB;
        if (hkpGsk::getClosestPoint(gskInput, hkConvexListAgent3_cache(agentData), *separatingNormal, pointOnB) != HK_SUCCESS)
        {
            // no closest point of the hull: back to the GSK
            hkConvexListAgent3_switchToGskMode(input, agentData, output);
            outputBackup.rollbackOutput(*input.m_bodyA, *input.m_bodyB, output, input.m_contactMgr);
            goto gskMode;
        }
    }
    {
        // the separating normal keeps its direction; its distance -HK_REAL_MAX / 2 marks the stream
        const __m128 w = _mm_mul_ps(g_vectorfConstants[HK_QUADREAL_INV_2], g_vectorfConstants[HK_QUADREAL_MINUS_MAX]);
        const __m128 n = separatingNormal->m_quad;
        separatingNormal->m_quad = _mm_shuffle_ps(n, _mm_unpackhi_ps(n, w), _MM_SHUFFLE(3, 0, 1, 0));

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
        hkAgent1nMachine_Process(hkConvexListAgent3_track(agentData), input, cls, hitList, output);
        stack.fastBlockFree(hitList, bytes);
        end = static_cast<char*>(agentData) + 0x1C; // counter and track after the cache
    }

done:
    hkConvexListAgent3_cache(agentData).m_gskFlags |= hkpGskCache::GSK_FLAGS_PROCESS_FUNCTION_CALLED;
    return reinterpret_cast<void*>((reinterpret_cast<uintptr_t>(end) + 15) & ~(uintptr_t)15);
}
