// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/agent/hkpgskagentutil.cpp
//
// Havok 2014.2.5 (havok_impl.md HK6): the convex-convex step of the GSK agents (agent3 and agent2), from rfg.exe:
// the GSK closest point, triangle welding, then the manifold's points and the new point into the output. No SDK
// reference exists; ported from the binary.
//
// The shipped build records the "Gsk" monitor timer (TLS hkMonitorStream); it is not reproduced.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/agent/hkpgskagentutil.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcdbody.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/contactmgr/hkpcontactmgr.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpcollisionqualityinfo.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpprocesscdpoint.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpprocesscollisioninput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpprocesscollisionoutput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/hkpagent3.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/triangle/hkptriangleshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/gskmanifold/hkpgskmanifold.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/gskmanifold/hkpgskmanifoldutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/hkpgsk.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/hkpgskcache.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/hkpgskexact.h"

using namespace hkGskExact;

// The agent data flag (byte 0xB of the agent's data) telling that welding modified the last GSK point.
static const unsigned char HK_GSK_AGENT_WELDED_FLAG = 0x10;

// -xyz, w kept
static inline Q hkGskAgentUtil_neg3(Q v)
{
    return _mm_xor_ps(v, _mm_castsi128_ps(_mm_set_epi32(0, (int)0x80000000, (int)0x80000000, (int)0x80000000)));
}

// 0x00E84330
void hkGskAgentUtil_processCollisionNoTim(const hkpAgent3Input& input, hkpAgentEntry* entry, void* agentData, hkpGskCache& gskCache, hkpGskManifold& gskManifold, hkVector4f& separatingNormal, int explicitlyAllowNewPoint, hkpProcessCollisionOutput& output)
{
    const hkpCdBody& bodyA = *input.m_bodyA;
    const hkpCdBody& bodyB = *input.m_bodyB;
    const hkpConvexShape* shapeA = static_cast<const hkpConvexShape*>(bodyA.getShape());
    const hkpConvexShape* shapeB = static_cast<const hkpConvexShape*>(bodyB.getShape());

    hkpGsk::GetClosesetPointInput gskInput;
    gskInput.m_aTb = &input.m_aTb;
    gskInput.m_transformA = &bodyA.getTransform();
    gskInput.m_shapeA = shapeA;
    gskInput.m_shapeB = shapeB;
    gskInput.m_collisionTolerance = input.m_input->m_tolerance;
    unsigned char& agentFlags = static_cast<unsigned char*>(agentData)[0xB];
    agentFlags &= ~HK_GSK_AGENT_WELDED_FLAG;

    hkVector4f point;
    if (hkpGsk::getClosestPoint(gskInput, gskCache, separatingNormal, point) == HK_FAILURE)
    {
        if (gskManifold.m_numContactPoints)
        {
            hkGskManifold_cleanup(gskManifold, input.m_contactMgr, *output.m_constraintOwner);
        }
        return;
    }

    // welding against a triangle on either side (A's normal flipped for the call)
    hkVector4f weldedNormal = separatingNormal;
    int weldResult = -1;
    if (shapeB->m_type.m_storage == hkcdShapeType::TRIANGLE)
    {
        weldResult = shapeB->weldContactPoint(&gskCache.m_vertices[gskCache.m_dimA], gskCache.m_dimB, point, &bodyB.getTransform(), shapeA, &bodyA.getTransform(), weldedNormal);
    }
    else if (shapeA->m_type.m_storage == hkcdShapeType::TRIANGLE && static_cast<const hkpTriangleShape*>(shapeA)->m_weldingType.m_storage != hkpWeldingUtility::WELDING_TYPE_NONE)
    {
        weldedNormal.m_quad = hkGskAgentUtil_neg3(weldedNormal.m_quad);
        weldResult = shapeA->weldContactPoint(&gskCache.m_vertices[0], gskCache.m_dimA, point, &bodyA.getTransform(), shapeB, &bodyB.getTransform(), weldedNormal);
        weldedNormal.m_quad = hkGskAgentUtil_neg3(weldedNormal.m_quad);
    }
    if (weldResult == hkpConvexShape::WELD_RESULT_REJECT_CONTACT_POINT)
    {
        hkGskManifold_cleanup(gskManifold, input.m_contactMgr, *output.m_constraintOwner);
        return;
    }
    if (weldResult == hkpConvexShape::WELD_RESULT_ACCEPT_CONTACT_POINT_MODIFIED)
    {
        agentFlags |= HK_GSK_AGENT_WELDED_FLAG;
    }

    const int existsFlags = hkGskManifold_doesPointExistAndResort(gskManifold, gskCache);
    explicitlyAllowNewPoint |= existsFlags & HK_GSK_MANIFOLD_FEATURE_WITHIN_KEEP_DISTANCE_REMOVED;
    hkpProcessCdPoint* const resultArray = output.m_firstFreeContactPoint;
    const int exists = existsFlags & HK_GSK_MANIFOLD_POINT_IN_MANIFOLD;

    // the manifold's other points (the existing point is index 0 and comes from the GSK)
    if (gskManifold.m_numContactPoints > exists)
    {
        hkpGskManifoldWork work;
        hkGskManifold_init(gskManifold, separatingNormal, weldedNormal, bodyA, bodyB, input.m_input->m_tolerance, work);
        explicitlyAllowNewPoint |= hkGskManifold_verifyAndGetPoints(gskManifold, work, exists, output, input.m_contactMgr);
    }

    hkpProcessCdPoint* cp = output.m_firstFreeContactPoint;
    cp->m_contact.m_position = point;
    cp->m_contact.m_separatingNormal = weldedNormal;
    cp->m_isShortestPoint = 0;
    hkpProcessCdPoint* representative = cp;
    if (exists)
    {
        cp->m_contactPointId = gskManifold.m_contactPoints[0].m_id;
        output.m_firstFreeContactPoint = cp + 1;
    }
    else
    {
        const hkpCollisionQualityInfo* quality = input.m_input->m_collisionQualityInfo;
        const float createDistance = (gskCache.m_dimA + gskCache.m_dimB == 4) ? quality->m_create4dContact : quality->m_createContact;
        if (lessOrNaN(bcast<3>(separatingNormal.m_quad), splat(createDistance)) || explicitlyAllowNewPoint)
        {
            const hkpGskManifoldUtilMgrHandling mgrHandling = input.m_input->m_enableDeprecatedWelding.m_bool ? HK_GSK_MANIFOLD_NO_ID_FOR_POTENTIALS : HK_GSK_MANIFOLD_CREATE_ID_ALWAYS;
            const hkpGskManifoldAddStatus status = hkGskManifold_addPoint(gskManifold, bodyA, bodyB, *input.m_input, output, gskCache, cp, resultArray, input.m_contactMgr, mgrHandling);
            if (status == HK_GSK_MANIFOLD_POINT_ADDED)
            {
                if (cp->m_contactPointId != 0xFFFF)
                {
                    output.m_firstFreeContactPoint = cp + 1;
                }
                else if (output.m_potentialContacts.val() != nullptr && entry != nullptr)
                {
                    // a potential contact: the id is created when the point is confirmed
                    if (input.m_contactMgr->reserveContactPointsImpl(1) == HK_SUCCESS)
                    {
                        hkpProcessCollisionOutput::ContactRef* ref = output.m_potentialContacts->m_firstFreePotentialContact;
                        output.m_potentialContacts->m_firstFreePotentialContact = ref + 1;
                        ref->m_agentEntry = entry;
                        ref->m_contactPoint = cp;
                        ref->m_agentData = agentData;
                        output.m_firstFreeContactPoint = cp + 1;
                    }
                    else
                    {
                        hkGskManifold_removePoint(gskManifold, 0);
                        representative = resultArray;
                    }
                }
                else
                {
                    const unsigned short id = input.m_contactMgr->addContactPointImpl(bodyA, bodyB, *input.m_input, output, &gskCache, cp->m_contact);
                    cp->m_contactPointId = id;
                    if (id == 0xFFFF)
                    {
                        hkGskManifold_removePoint(gskManifold, 0);
                        representative = resultArray;
                    }
                    else
                    {
                        gskManifold.m_contactPoints[0].m_id = id;
                        output.m_firstFreeContactPoint = cp + 1;
                    }
                }
            }
            else if (status == HK_GSK_MANIFOLD_POINT_REJECTED)
            {
                representative = resultArray;
            }
            else if (status == HK_GSK_MANIFOLD_TWO_POINT2_REJECTED)
            {
                output.m_firstFreeContactPoint = output.m_firstFreeContactPoint.val() - 1;
                representative = resultArray;
            }
            else
            {
                representative = resultArray + status; // replaced that point
            }
        }
    }

    // one representative contact of this agent for the potential contacts
    if (output.m_potentialContacts.val() != nullptr && representative < output.m_firstFreeContactPoint.val())
    {
        *output.m_potentialContacts->m_firstFreeRepresentativeContact = representative;
        output.m_potentialContacts->m_firstFreeRepresentativeContact++;
    }
}
