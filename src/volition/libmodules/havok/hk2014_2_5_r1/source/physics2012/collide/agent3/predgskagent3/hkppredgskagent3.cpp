// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/predgskagent3/hkppredgskagent3.cpp
//
// Havok 2014.2.5 (havok_impl.md HK6): the predictive convex-convex agent3, the main convex pair agent of the agent
// machines, from rfg.exe. Its data is the GSK cache followed by the feature manifold (rounded up to 16 bytes). No SDK
// source exists for 2014.2.5; ported from the binary.
//
// The shipped build records the "PredGskf3" monitor timer (TLS hkMonitorStream); it is not reproduced.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/predgskagent3/hkppredgskagent3.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/hkmath.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcdbody.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcollidable.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpcollisionqualityinfo.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpprocesscollisioninput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpprocesscollisionoutput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/hkpagent3.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/hkpconvexshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/triangle/hkptriangleshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/agent/hkpgskagentutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/continuous/hkpcontinuousgsk.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/gskmanifold/hkpgskmanifold.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/gskmanifold/hkpgskmanifoldutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/hkpgsk.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/hkpgskexact.h"

using namespace hkGskExact;

static inline hkpGskCache& hkPredGskAgent3_cache(void* agentData)
{
    return *static_cast<hkpGskCache*>(agentData);
}

static inline hkpGskManifold& hkPredGskAgent3_manifold(void* agentData)
{
    return *reinterpret_cast<hkpGskManifold*>(static_cast<char*>(agentData) + 0xC);
}

// The end of the agent data: the cache (0xC), the manifold header (4), its points and vertex ids, rounded to 16 bytes.
static inline void* hkPredGskAgent3_end(void* agentData)
{
    const hkpGskManifold& m = hkPredGskAgent3_manifold(agentData);
    const int size = (((m.m_numVertsA + m.m_numVertsB + m.m_numContactPoints * 4) * 2 + 0x1F) & ~0xF);
    return static_cast<char*>(agentData) + size;
}

// 0x00DC55F0
void hkPredGskAgent3::initAgentFunc(hkpCollisionDispatcher::Agent3Funcs& f)
{
    f.m_createFunc = create;
    f.m_processFunc = process;
    f.m_sepNormalFunc = sepNormal;
    f.m_cleanupFunc = cleanup;
    f.m_removePointFunc = removePoint;
    f.m_commitPotentialFunc = commitPotential;
    f.m_createZombieFunc = createZombie;
    f.m_destroyFunc = destroy;
    f.m_isPredictive = true;
}

// 0x00DC5630
void hkPredGskAgent3::registerAgent3(hkpCollisionDispatcher* dispatcher, hkcdShapeType::ShapeTypeEnum typeA, hkcdShapeType::ShapeTypeEnum typeB)
{
    hkpCollisionDispatcher::Agent3Funcs f;
    initAgentFunc(f);
    dispatcher->registerAgent3(f, typeA, typeB);
}

// 0x00DC5090
void* hkPredGskAgent3::create(const hkpAgent3Input& input, hkpAgentEntry* entry, void* agentData)
{
    const hkpConvexShape* shapeA = static_cast<const hkpConvexShape*>(input.m_bodyA->getShape());
    const hkpConvexShape* shapeB = static_cast<const hkpConvexShape*>(input.m_bodyB->getShape());
    hkpGskCache& cache = hkPredGskAgent3_cache(agentData);
    if (shapeB->m_type.m_storage == hkcdShapeType::TRIANGLE)
    {
        cache.initTriangle(shapeA, static_cast<const hkpTriangleShape*>(shapeB), input.m_aTb);
    }
    else
    {
        cache.init(shapeA, shapeB, input.m_aTb);
    }
    entry->m_numContactPoints = 0;
    cache.m_gskFlags &= ~hkpGskCache::GSK_FLAGS_DISABLE_CONTACT_TIMS;
    hkPredGskAgent3_manifold(agentData).init();
    return hkPredGskAgent3_end(agentData);
}

// 0x00DC50F0: the separating normal in world space, w the distance less the radius sum
void hkPredGskAgent3::sepNormal(const hkpAgent3Input& input, hkpAgentEntry* entry, void* agentData, hkVector4f& separatingNormalOut)
{
    const hkpConvexShape* shapeA = static_cast<const hkpConvexShape*>(input.m_bodyA->getShape());
    const hkpConvexShape* shapeB = static_cast<const hkpConvexShape*>(input.m_bodyB->getShape());
    hkpGskCache& cache = hkPredGskAgent3_cache(agentData);
    hkpGsk gsk;
    gsk.init(shapeA, shapeB, cache);
    hkVector4f normal;
    gsk.getClosestFeature(shapeA, shapeB, input.m_aTb, normal);
    gsk.checkForChangesAndUpdateCache(cache);
    const Q n = rotate(input.m_bodyA->getTransform().getRotation(), normal.m_quad);
    const float radiusSum = shapeB->m_radius + shapeA->m_radius;
    separatingNormalOut.m_quad = setXYZ_W(n, sub(bcast<3>(normal.m_quad), splat(radiusSum)));
}

// 0x00DC4F80
void hkPredGskAgent3::removePoint(hkpAgentEntry* entry, void* agentData, unsigned short idToRemove)
{
    hkpGskManifold& manifold = hkPredGskAgent3_manifold(agentData);
    for (int i = 0; i < manifold.m_numContactPoints; i++)
    {
        if (manifold.m_contactPoints[i].m_id == idToRemove)
        {
            hkGskManifold_removePoint(manifold, i);
            return;
        }
    }
}

// 0x00DC4FC0
void hkPredGskAgent3::commitPotential(hkpAgentEntry* entry, void* agentData, unsigned short idToCommit)
{
    hkpGskManifold& manifold = hkPredGskAgent3_manifold(agentData);
    for (int i = 0; i < manifold.m_numContactPoints; i++)
    {
        if (manifold.m_contactPoints[i].m_id == 0xFFFF)
        {
            manifold.m_contactPoints[i].m_id = idToCommit;
            return;
        }
    }
}

// 0x00DC5000
void hkPredGskAgent3::createZombie(hkpAgentEntry* entry, void* agentData, unsigned short idToConvert)
{
    hkpGskManifold& manifold = hkPredGskAgent3_manifold(agentData);
    for (int i = 0; i < manifold.m_numContactPoints; i++)
    {
        if (manifold.m_contactPoints[i].m_id == idToConvert)
        {
            manifold.m_contactPoints[i].m_dimA = 0;
            manifold.m_contactPoints[i].m_dimB = 0;
            return;
        }
    }
}

// 0x00DC5030
void* hkPredGskAgent3::cleanup(hkpAgentEntry* entry, void* agentData, hkpContactMgr* mgr, hkpConstraintOwner& constraintOwner)
{
    hkGskManifold_cleanup(hkPredGskAgent3_manifold(agentData), mgr, constraintOwner);
    void* end = hkPredGskAgent3_end(agentData);
    entry->m_numContactPoints = 0;
    return end;
}

// 0x00DC5070
void hkPredGskAgent3::destroy(hkpAgentEntry* entry, void* agentData, hkpContactMgr* mgr, hkpConstraintOwner& constraintOwner, hkpCollisionDispatcher* dispatcher)
{
    cleanup(entry, agentData, mgr, constraintOwner);
}

// 0x00DC5210
// distAtT1 is the separating distance at the step end estimated by the machine (TIM). A pair that may penetrate deeper
// than the minimum separation gets a time of impact (continuous physics). A pair beyond the manifold TIM distance (with
// contact TIMs allowed) only verifies its manifold; otherwise the GSK runs.
void* hkPredGskAgent3::process(const hkpAgent3ProcessInput& input, hkpAgentEntry* entry, void* agentData, hkVector4f* separatingNormal, hkpProcessCollisionOutput& output)
{
    hkpGskCache& cache = hkPredGskAgent3_cache(agentData);
    hkpGskManifold& manifold = hkPredGskAgent3_manifold(agentData);
    const Q distAtT1 = splat(input.m_distAtT1);
    const hkpCollisionQualityInfo* quality = input.m_input->m_collisionQualityInfo;
    int needNewPoint = 0;

    bool runGsk = true;
    bool toi = false;
    if (quality->m_useContinuousPhysics.val())
    {
        const Q allowedA = splat(input.m_bodyA->getRootCollidable()->m_allowedPenetrationDepth);
        const Q allowedB = splat(input.m_bodyB->getRootCollidable()->m_allowedPenetrationDepth);
        const Q allowed = _mm_min_ps(allowedA, allowedB);
        const Q dist0 = bcast<3>(separatingNormal->m_quad);
        const Q minSeparation = _mm_min_ps(mul(splat(quality->m_minSeparation), allowed), add(mul(splat(quality->m_minExtraSeparation), allowed), dist0));
        if (lessOrNaN(distAtT1, minSeparation))
        {
            const Q toiSeparation = _mm_min_ps(mul(splat(quality->m_toiSeparation), allowed), add(mul(splat(quality->m_toiExtraSeparation), allowed), dist0));
            hkSimdFloat32 allowedSr;
            allowedSr.m_real = allowed;
            hkSimdFloat32 minSeparationSr;
            minSeparationSr.m_real = minSeparation;
            hkSimdFloat32 toiSeparationSr;
            toiSeparationSr.m_real = toiSeparation;
            hk4dGskCollideCalcToi(input, allowedSr, minSeparationSr, toiSeparationSr, cache, *separatingNormal, output);
            toi = true;
        }
    }
    if (!toi && greater(distAtT1, splat(quality->m_manifoldTimDistance)) && !(cache.m_gskFlags & hkpGskCache::GSK_FLAGS_DISABLE_CONTACT_TIMS))
    {
        separatingNormal->m_quad = setXYZ_W(separatingNormal->m_quad, distAtT1);
        runGsk = false;
        if (manifold.m_numContactPoints)
        {
            hkpGskManifoldWork work;
            hkGskManifold_init(manifold, *separatingNormal, *separatingNormal, *input.m_bodyA, *input.m_bodyB, input.m_input->m_tolerance, work);
            needNewPoint = hkGskManifold_verifyAndGetPoints(manifold, work, 0, output, input.m_contactMgr);
            if (needNewPoint && (cache.m_gskFlags & 0x80))
            {
                output.m_firstFreeContactPoint = output.m_firstFreeContactPoint.val() - manifold.m_numContactPoints;
                runGsk = true;
            }
            else if (manifold.m_numContactPoints && output.m_potentialContacts.val() != nullptr)
            {
                // the first of the manifold's points represents the pair
                *output.m_potentialContacts->m_firstFreeRepresentativeContact = output.m_firstFreeContactPoint.val() - manifold.m_numContactPoints;
                output.m_potentialContacts->m_firstFreeRepresentativeContact++;
            }
        }
    }
    if (runGsk)
    {
        hkGskAgentUtil_processCollisionNoTim(input, entry, agentData, cache, manifold, *separatingNormal, needNewPoint, output);
    }
    entry->m_numContactPoints = manifold.m_numContactPoints;
    return hkPredGskAgent3_end(agentData);
}

// 0x00DB4E70 (.h:47)
int hkPredGskAgent3::getGskFlag(const void* agentData, hkpGskCache::GskFlagValues mask)
{
    return static_cast<const hkpGskCache*>(agentData)->m_gskFlags & mask;
}

// 0x00DB4E80 (.h:48)
void hkPredGskAgent3::setGskFlagToTrue(void* agentData, hkpGskCache::GskFlagValues mask)
{
    static_cast<hkpGskCache*>(agentData)->m_gskFlags |= (unsigned char)mask;
}

// 0x00DB4E90 (.h:49)
void hkPredGskAgent3::setGskFlagToFalse(void* agentData, hkpGskCache::GskFlagValues mask)
{
    static_cast<hkpGskCache*>(agentData)->m_gskFlags &= (unsigned char)~mask;
}
