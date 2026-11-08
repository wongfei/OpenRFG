// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/predgskcylinderagent3/hkppredgskcylinderagent3.cpp
//
// Havok 2014.2.5 (havok_impl.md HK6): the predictive GSK cylinder agent3, from rfg.exe: hkPredGskAgent3 with each
// cylinder body replaced by a capsule around it while the capsule representation is on (gsk flags 4 and 8 for A and
// B). A cylinder switches to its capsule when its axis lies almost in the contact plane (|cos| < 0.05) or the capsules
// are separated by more than 1 cm, and back when the axis tilts more (|cos| > 0.1) and the capsules penetrate; the
// manifold is dropped at each switch. The capsules are heap shapes made each step (radius: cylinder radius + convex
// radius + 2 mm) in temporary bodies that replace the input's body pointers. No SDK source exists for 2014.2.5; ported
// from the binary.
//
// The shipped build records the "TtPredGskf3" monitor timer; it is not reproduced.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/predgskcylinderagent3/hkppredgskcylinderagent3.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/hkmath.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/physics/motionstate/hkmotionstate.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcdbody.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcollidable.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/contactmgr/hkpcontactmgr.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpcollisionqualityinfo.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpprocesscollisioninput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpprocesscollisionoutput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/hkpagent3.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/predgskagent3/hkppredgskagent3.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/hkpcollisiondispatcher.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/capsule/hkpcapsuleshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/cylinder/hkpcylindershape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/agent/hkpgskagentutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/continuous/hkpcontinuousgsk.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/gskmanifold/hkpgskmanifold.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/gskmanifold/hkpgskmanifoldutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/hkpgskcache.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/hkpgskexact.h"

#include <string.h>

using namespace hkGskExact;

static inline hkpGskCache& hkPredGskCylinderAgent3_cache(void* agentData)
{
    return *static_cast<hkpGskCache*>(agentData);
}

static inline hkpGskManifold& hkPredGskCylinderAgent3_manifold(void* agentData)
{
    return *reinterpret_cast<hkpGskManifold*>(static_cast<char*>(agentData) + 0xC);
}

// The capsule around a cylinder. The radius sum runs on the x87 stack in the original (getCylinderRadius returns in
// ST0, then two FADDs at the runtime's double precision, one FSTP to float).
static hkpCapsuleShape* hkPredGskCylinderAgent3_capsule(const hkpCylinderShape* cylinder)
{
    const float radius = (float)(((double)cylinder->getCylinderRadius() + (double)cylinder->m_radius) + (double)0.002f); // 0x3B03126F
    return new hkpCapsuleShape(cylinder->m_vertexA, cylinder->m_vertexB, radius);
}

// The bodies of the flagged sides replaced by capsule bodies (the cylinder body as parent, its motion, no key).
struct hkPredGskCylinderAgent3_Replacement
{
    hkPadSpu<const hkpCdBody*>* m_bodyPtrs[2];
    const hkpCdBody* m_saved[2];
    hkpCdBody m_bodies[2];

    hkPredGskCylinderAgent3_Replacement(const hkpAgent3Input& input)
    {
        m_bodyPtrs[0] = const_cast<hkPadSpu<const hkpCdBody*>*>(&input.m_bodyA);
        m_bodyPtrs[1] = const_cast<hkPadSpu<const hkpCdBody*>*>(&input.m_bodyB);
        m_saved[0] = nullptr;
        m_saved[1] = nullptr;
    }

    void replace(const unsigned int* useCapsule)
    {
        for (int i = 0; i < 2; i++)
        {
            if (!useCapsule[i])
            {
                m_saved[i] = nullptr;
                continue;
            }
            const hkpCdBody* body = m_bodyPtrs[i]->val();
            hkpCapsuleShape* capsule = hkPredGskCylinderAgent3_capsule(static_cast<const hkpCylinderShape*>(body->getShape()));
            hkpCdBody& replacement = m_bodies[i];
            replacement.m_parent = body;
            replacement.m_motion = body->m_motion;
            replacement.m_shape = capsule;
            replacement.m_shapeKey = 0xFFFFFFFF;
            m_saved[i] = body;
            *m_bodyPtrs[i] = &replacement;
        }
    }

    // the capsules lose their reference; the input gets its bodies back
    void restore()
    {
        for (int i = 0; i < 2; i++)
        {
            if (m_saved[i])
            {
                m_bodyPtrs[i]->val()->getShape()->removeReference();
                *m_bodyPtrs[i] = m_saved[i];
            }
        }
    }
};

// 0x00DC4210 (LTCG: agentData in EDX): the separating distance of the capsule representations, from a fresh cache.
static Q hkPredGskCylinderAgent3_separatingDistanceUsingCapsuleRepresentations(const hkpAgent3ProcessInput& input, hkpAgentEntry* entry, void* agentData)
{
    hkpAgent3ProcessInput copy = input;
    const unsigned char flags = hkPredGskCylinderAgent3_cache(agentData).m_gskFlags;
    const unsigned int cylinders[2] = { flags & 1u, flags & 2u };
    hkPredGskCylinderAgent3_Replacement replacement(copy);
    replacement.replace(cylinders);
    alignas(16) hkpGskCache cache;
    cache.init(static_cast<const hkpConvexShape*>(copy.m_bodyA.val()->getShape()), static_cast<const hkpConvexShape*>(copy.m_bodyB.val()->getShape()), input.m_aTb);
    alignas(16) hkVector4f normal;
    hkPredGskAgent3::sepNormal(copy, entry, &cache, normal);
    replacement.restore();
    return bcast<3>(normal.m_quad);
}

// 0x00DC41C0: the predictive GSK agent3, the cylinder sides flagged and starting as capsules
void* hkPredGskCylinderAgent3::create(const hkpAgent3Input& input, hkpAgentEntry* entry, void* agentData)
{
    void* end = hkPredGskAgent3::create(input, entry, agentData);
    hkpGskCache& cache = hkPredGskCylinderAgent3_cache(agentData);
    cache.m_gskFlags &= 0xF0;
    if (input.m_bodyA.val()->getShape()->m_type.m_storage == hkcdShapeType::CYLINDER)
    {
        cache.m_gskFlags |= hkpGskCache::GSK_FLAGS_SHAPE_A_IS_CYLINDER | hkpGskCache::GSK_FLAGS_REPLACE_SHAPE_A_WITH_CAPSULE;
    }
    if (input.m_bodyB.val()->getShape()->m_type.m_storage == hkcdShapeType::CYLINDER)
    {
        cache.m_gskFlags |= hkpGskCache::GSK_FLAGS_SHAPE_B_IS_CYLINDER | hkpGskCache::GSK_FLAGS_REPLACE_SHAPE_B_WITH_CAPSULE;
    }
    return end;
}

// A cylinder vertex id seen from its capsule: the capsule end on the same side (vertex id bit 7: the cylinder's top
// or bottom ring), 0 or 16.
static inline unsigned short hkPredGskCylinderAgent3_capsuleVertex(unsigned short cylinderVertex)
{
    return (unsigned short)((1 - ((cylinderVertex >> 7) & 1)) << 4);
}

// 0x00DC43C0
void* hkPredGskCylinderAgent3::process(const hkpAgent3ProcessInput& input, hkpAgentEntry* entry, void* agentData, hkVector4f* separatingNormal, hkpProcessCollisionOutput& output)
{
    hkpGskCache& cache = hkPredGskCylinderAgent3_cache(agentData);
    hkpGskManifold& manifold = hkPredGskCylinderAgent3_manifold(agentData);
    const Q distAtT1 = splat(input.m_distAtT1);
    const unsigned char flags = cache.m_gskFlags;
    const unsigned int cylinders[2] = { flags & 1u, flags & 2u };
    unsigned int useCapsule[2] = { flags & 4u, flags & 8u };
    hkPredGskCylinderAgent3_Replacement replacement(input);

    alignas(16) hkVector4f normal;
    hkPredGskAgent3::sepNormal(input, entry, agentData, normal);
    const Q zero = _mm_setzero_ps();
    for (int i = 0; i < 2; i++)
    {
        if (!cylinders[i])
        {
            continue;
        }
        const hkpCdBody* body = replacement.m_bodyPtrs[i]->val();
        const hkpCylinderShape* cylinder = static_cast<const hkpCylinderShape*>(body->getShape());
        const Q axis = sub(cylinder->m_vertexB.m_quad, cylinder->m_vertexA.m_quad);
        const Q direction = mul(invSqrt23SetZero(dot3(axis, axis)), axis);
        const Q worldDirection = rotate(body->getTransform().getRotation(), direction);
        const Q cosAngle = abs(dot3(worldDirection, normal.m_quad));
        const Q separation = hkPredGskCylinderAgent3_separatingDistanceUsingCapsuleRepresentations(input, entry, agentData);
        bool switched = false;
        if (useCapsule[i])
        {
            // back to the cylinder: the axis tilted (|cos| > 0.1) and the capsules penetrate
            if (greater(cosAngle, bits(0x3DCCCCCD)) && lessOrNaN(separation, zero))
            {
                useCapsule[i] = 0;
                cache.m_gskFlags &= (unsigned char)~(4 << i);
                switched = true;
            }
        }
        else if (lessOrNaN(cosAngle, bits(0x3D4CCCCD)) || greater(separation, bits(0x3C23D70A)))
        {
            // to the capsule: the axis almost in the plane (|cos| < 0.05), or more than 1 cm apart
            useCapsule[i] = 1;
            cache.m_gskFlags |= (unsigned char)(4 << i);
            switched = true;
        }
        if (switched)
        {
            hkGskManifold_cleanup(manifold, input.m_contactMgr, *output.m_constraintOwner);
            entry->m_numContactPoints = manifold.m_numContactPoints;
        }
    }

    const hkpProcessCollisionInput& collisionInput = *input.m_input.val();
    const hkpCollisionQualityInfo* quality = collisionInput.m_collisionQualityInfo;
    int needNewPoint = 0;
    bool replaced = false;
    bool runGsk = true; // the "L46df" tail: the GSK unless the pair is beyond the tolerance
    bool done = false; // straight to restoring the bodies (separating normal not reset)
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
    if (!toi && greater(distAtT1, splat(quality->m_manifoldTimDistance)) && !(cache.m_gskFlags & hkpGskCache::GSK_FLAGS_DISABLE_CONTACT_TIMS)
        && lessOrNaN(bcast<3>(normal.m_quad), splat(collisionInput.m_tolerance.val())))
    {
        // within the manifold TIM: the manifold only verified
        separatingNormal->m_quad = setXYZ_W(separatingNormal->m_quad, distAtT1);
        done = true;
        if (manifold.m_numContactPoints)
        {
            replacement.replace(useCapsule);
            replaced = true;
            hkpGskManifoldWork work;
            hkGskManifold_init(manifold, *separatingNormal, *separatingNormal, *input.m_bodyA, *input.m_bodyB, collisionInput.m_tolerance, work);
            needNewPoint = hkGskManifold_verifyAndGetPoints(manifold, work, 0, output, input.m_contactMgr);
            if (needNewPoint && (cache.m_gskFlags & hkpGskCache::GSK_FLAGS_ALLOW_QUICKER_CONTACT_POINT_RECREATION))
            {
                output.m_firstFreeContactPoint = output.m_firstFreeContactPoint.val() - manifold.m_numContactPoints;
                done = false;
            }
            else if (manifold.m_numContactPoints && output.m_potentialContacts.val() != nullptr)
            {
                // the first of the manifold's points represents the pair
                *output.m_potentialContacts->m_firstFreeRepresentativeContact = output.m_firstFreeContactPoint.val() - manifold.m_numContactPoints;
                output.m_potentialContacts->m_firstFreeRepresentativeContact++;
            }
        }
    }
    if (!done)
    {
        if (!lessOrNaN(bcast<3>(normal.m_quad), splat(collisionInput.m_tolerance.val())))
        {
            runGsk = false;
            if (manifold.m_numContactPoints)
            {
                hkGskManifold_cleanup(manifold, input.m_contactMgr, *output.m_constraintOwner);
            }
        }
        if (runGsk)
        {
            if (!replaced)
            {
                replacement.replace(useCapsule);
            }
            // the cache's vertex ids of a capsule side mapped to the capsule's ends (an edge 16-0, a triangle cut to
            // that edge, the other side's vertices moved down)
            hkpGskCache* gskCache = &cache;
            alignas(16) hkpGskCache capsuleCache;
            if (useCapsule[0] || useCapsule[1])
            {
                memcpy(&capsuleCache, &cache, 12);
                gskCache = &capsuleCache;
                if (useCapsule[0])
                {
                    capsuleCache.m_vertices[0] = hkPredGskCylinderAgent3_capsuleVertex(capsuleCache.m_vertices[0]);
                    if (capsuleCache.m_dimA > 1)
                    {
                        capsuleCache.m_vertices[0] = 0x10;
                        capsuleCache.m_vertices[1] = 0;
                        if (capsuleCache.m_dimA == 3)
                        {
                            capsuleCache.m_dimA = 2;
                            capsuleCache.m_vertices[2] = capsuleCache.m_vertices[3];
                        }
                    }
                }
                if (useCapsule[1])
                {
                    const int b = capsuleCache.m_dimA;
                    capsuleCache.m_vertices[b] = hkPredGskCylinderAgent3_capsuleVertex(capsuleCache.m_vertices[b]);
                    if (capsuleCache.m_dimB > 1)
                    {
                        capsuleCache.m_vertices[capsuleCache.m_dimA] = 0x10;
                        capsuleCache.m_vertices[capsuleCache.m_dimA + 1] = 0;
                        if (capsuleCache.m_dimB == 3)
                        {
                            capsuleCache.m_dimB = 2;
                        }
                    }
                }
            }
            hkGskAgentUtil_processCollisionNoTim(input, entry, agentData, *gskCache, manifold, *separatingNormal, needNewPoint, output);
        }
        separatingNormal->m_quad = normal.m_quad;
    }
    replacement.restore();
    entry->m_numContactPoints = manifold.m_numContactPoints;
    return static_cast<char*>(agentData) + ((((manifold.m_numVertsA + manifold.m_numContactPoints * 4 + manifold.m_numVertsB) * 2) + 0x1F) & ~0xF);
}

// 0x00DC4D10: the predictive GSK agent3 with its own create and process, cylinder vs convex both ways
void hkPredGskCylinderAgent3::registerAgent3(hkpCollisionDispatcher* dispatcher)
{
    hkpCollisionDispatcher::Agent3Funcs f;
    f.m_createFunc = create;
    f.m_processFunc = process;
    f.m_sepNormalFunc = hkPredGskAgent3::sepNormal;
    f.m_cleanupFunc = hkPredGskAgent3::cleanup;
    f.m_removePointFunc = hkPredGskAgent3::removePoint;
    f.m_commitPotentialFunc = hkPredGskAgent3::commitPotential;
    f.m_createZombieFunc = hkPredGskAgent3::createZombie;
    f.m_destroyFunc = hkPredGskAgent3::destroy;
    f.m_isPredictive = true;
    f.m_ignoreSymmetricVersion = true;
    dispatcher->registerAgent3(f, hkcdShapeType::CYLINDER, hkcdShapeType::CONVEX);
    f.m_reusePreviousEntry = true;
    dispatcher->registerAgent3(f, hkcdShapeType::CONVEX, hkcdShapeType::CYLINDER);
}
