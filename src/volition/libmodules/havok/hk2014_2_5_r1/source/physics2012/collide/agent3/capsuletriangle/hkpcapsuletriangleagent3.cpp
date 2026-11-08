// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/capsuletriangle/hkpcapsuletriangleagent3.cpp
//
// Havok 2014.2.5 (havok_impl.md HK6): the capsule-triangle agent3, from rfg.exe: the manifold of
// hkCollideCapsuleUtilCapsVsTri (up to three points), welded at the triangle's edges, with potential contacts for the
// 1n machine's welding. The agent data: the three contact point ids (0x00), the triangle's distance cache (0x08);
// 0x20 bytes in all. No SDK source exists for 2014.2.5; ported from the binary.
//
// The shipped build records the "TtCapsTri3" monitor timer; it is not reproduced.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/capsuletriangle/hkpcapsuletriangleagent3.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/hkmath.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/physics/contactpoint/hkcontactpoint.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcdbody.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/contactmgr/hkpcontactmgr.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpcollisioninput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpprocesscdpoint.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpprocesscollisioninput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpprocesscollisionoutput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/hkpagent3.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/hkpcollisiondispatcher.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/capsule/hkpcapsuleshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/triangle/hkptriangleshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/util/hkpcollidecapsuleutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/hkpgskexact.h"

using namespace hkGskExact;

static inline unsigned short* hkCapsuleTriangleAgent3_ids(void* agentData)
{
    return static_cast<unsigned short*>(agentData);
}

static inline hkpCollideTriangleUtil::PointTriangleDistanceCache& hkCapsuleTriangleAgent3_cache(void* agentData)
{
    return *reinterpret_cast<hkpCollideTriangleUtil::PointTriangleDistanceCache*>(static_cast<char*>(agentData) + 8);
}

// The capsule's axis and the triangle in world space, through hkCollideCapsuleUtilCapsVsTri with no tolerance
// (HK_REAL_MAX, 0x7F7FFFEE).
static void hkCapsuleTriangleAgent3_capsVsTri(const hkpAgent3Input& input, void* agentData, int searchManifold, hkContactPoint* points, hkpFeatureOutput* featuresOut)
{
    const hkpCdBody& bodyA = *input.m_bodyA.val();
    const hkpCdBody& bodyB = *input.m_bodyB.val();
    const hkpCapsuleShape* capsule = static_cast<const hkpCapsuleShape*>(bodyA.getShape());
    const hkpTriangleShape* triangle = static_cast<const hkpTriangleShape*>(bodyB.getShape());
    const hkTransformf& tA = bodyA.getTransform();
    const hkTransformf& tB = bodyB.getTransform();
    alignas(16) hkVector4f axis[2];
    axis[1].m_quad = transformPos(tA, capsule->m_vertexB.m_quad);
    axis[0].m_quad = transformPos(tA, capsule->m_vertexA.m_quad);
    alignas(16) hkVector4f tri[3];
    tri[2].m_quad = transformPos(tB, triangle->m_vertexC.m_quad);
    tri[1].m_quad = transformPos(tB, triangle->m_vertexB.m_quad);
    tri[0].m_quad = transformPos(tB, triangle->m_vertexA.m_quad);
    hkCollideCapsuleUtilCapsVsTri(axis, capsule->m_radius, tri, triangle->m_radius, hkCapsuleTriangleAgent3_cache(agentData), lane0(bits(0x7F7FFFEE)), searchManifold, points, featuresOut);
}

// 0x00DBBB30: no contact points, the distance cache of B's triangle
void* hkCapsuleTriangleAgent3::create(const hkpAgent3Input& input, hkpAgentEntry* entry, void* agentData)
{
    const hkpTriangleShape* triangle = static_cast<const hkpTriangleShape*>(input.m_bodyB.val()->getShape());
    entry->m_numContactPoints = 0;
    unsigned short* ids = hkCapsuleTriangleAgent3_ids(agentData);
    ids[0] = 0xFFFF;
    ids[1] = 0xFFFF;
    ids[2] = 0xFFFF;
    hkpCollideTriangleUtil::setupPointTriangleDistanceCache(&triangle->m_vertexA, hkCapsuleTriangleAgent3_cache(agentData));
    return static_cast<char*>(agentData) + 0x20;
}

// 0x00DBBBE0: the normal of the closer of the first two points
void hkCapsuleTriangleAgent3::sepNormal(const hkpAgent3Input& input, hkpAgentEntry* entry, void* agentData, hkVector4f& separatingNormalOut)
{
    alignas(16) hkContactPoint points[3];
    hkCapsuleTriangleAgent3_capsVsTri(input, agentData, 0, points, nullptr);
    const Q n0 = points[0].m_separatingNormal.m_quad;
    const Q n1 = points[1].m_separatingNormal.m_quad;
    separatingNormalOut.m_quad = selectQ(_mm_cmplt_ps(bcast<3>(n0), bcast<3>(n1)), n0, n1);
}

// 0x00DBBB70: the contact points removed
void* hkCapsuleTriangleAgent3::cleanup(hkpAgentEntry* entry, void* agentData, hkpContactMgr* mgr, hkpConstraintOwner& constraintOwner)
{
    unsigned short* ids = hkCapsuleTriangleAgent3_ids(agentData);
    for (int i = 0; i < 3; i++)
    {
        if (ids[i] != 0xFFFF)
        {
            mgr->removeContactPointImpl(ids[i], constraintOwner);
            ids[i] = 0xFFFF;
        }
    }
    return static_cast<char*>(agentData) + 0x20;
}

// 0x00DBBAC0
void hkCapsuleTriangleAgent3::removePoint(hkpAgentEntry* entry, void* agentData, unsigned short idToRemove)
{
    unsigned short* ids = hkCapsuleTriangleAgent3_ids(agentData);
    for (int i = 0; i < 3; i++)
    {
        if (ids[i] == idToRemove)
        {
            ids[i] = 0xFFFF;
            return;
        }
    }
}

// 0x00DBBAF0: into the first free slot
void hkCapsuleTriangleAgent3::commitPotential(hkpAgentEntry* entry, void* agentData, unsigned short idToCommit)
{
    unsigned short* ids = hkCapsuleTriangleAgent3_ids(agentData);
    for (int i = 0; i < 3; i++)
    {
        if (ids[i] == 0xFFFF)
        {
            ids[i] = idToCommit;
            return;
        }
    }
}

// 0x00DBBB20
void hkCapsuleTriangleAgent3::createZombie(hkpAgentEntry* entry, void* agentData, unsigned short idToConvert)
{
}

// 0x00DBBBC0
void hkCapsuleTriangleAgent3::destroy(hkpAgentEntry* entry, void* agentData, hkpContactMgr* mgr, hkpConstraintOwner& constraintOwner, hkpCollisionDispatcher* dispatcher)
{
    cleanup(entry, agentData, mgr, constraintOwner);
}

// 0x00DBBD70: each point within the tolerance gets a contact (with potential contacts: a reserved potential one,
// committed later by the welding) and is welded at the triangle's edges; a rejected point loses its contact (or its
// reservation). The separating normal is the closer first-two point's. Original behaviour kept: m_isShortestPoint is
// set on every point except that closer one, which alone becomes a representative contact.
void* hkCapsuleTriangleAgent3::process(const hkpAgent3ProcessInput& input, hkpAgentEntry* entry, void* agentData, hkVector4f* separatingNormal, hkpProcessCollisionOutput& output)
{
    alignas(16) hkContactPoint points[3];
    hkpFeatureOutput features[3];
    hkCapsuleTriangleAgent3_capsVsTri(input, agentData, 1, points, features);
    const hkpCdBody& bodyA = *input.m_bodyA.val();
    const hkpCdBody& bodyB = *input.m_bodyB.val();
    hkpContactMgr* mgr = input.m_contactMgr.val();
    const int shortest = greaterEqual(bcast<3>(points[0].m_separatingNormal.m_quad), bcast<3>(points[1].m_separatingNormal.m_quad)) ? 1 : 0;
    const Q tolerance = splat(input.m_input.val()->m_tolerance.val());
    unsigned short* ids = hkCapsuleTriangleAgent3_ids(agentData);
    int numContactPoints = 0;
    for (int i = 0; i < 3; i++)
    {
        unsigned short id = ids[i];
        if (greaterEqual(bcast<3>(points[i].m_separatingNormal.m_quad), tolerance))
        {
            if (id != 0xFFFF)
            {
                mgr->removeContactPointImpl(id, *output.m_constraintOwner);
                id = 0xFFFF;
            }
            ids[i] = id;
            continue;
        }
        if (id == 0xFFFF)
        {
            hkpProcessCollisionOutput::PotentialInfo* potentials = output.m_potentialContacts;
            if (potentials)
            {
                if (mgr->reserveContactPointsImpl(1) != HK_SUCCESS)
                {
                    ids[i] = id;
                    continue;
                }
                hkpProcessCollisionOutput::ContactRef* ref = potentials->m_firstFreePotentialContact++;
                ref->m_contactPoint = output.m_firstFreeContactPoint;
                ref->m_agentEntry = entry;
                ref->m_agentData = agentData;
            }
            else
            {
                id = mgr->addContactPointImpl(bodyA, bodyB, *input.m_input.val(), output, nullptr, points[i]);
                if (id == 0xFFFF)
                {
                    ids[i] = id;
                    continue;
                }
            }
        }
        unsigned char numFeatures = features[i].numFeatures;
        if (numFeatures)
        {
            alignas(16) hkVector4f weldedNormal;
            weldedNormal.m_quad = points[i].m_separatingNormal.m_quad;
            alignas(16) hkVector4f contactPoint;
            contactPoint.m_quad = points[i].m_position.m_quad;
            const int weld = bodyB.getShape()->weldContactPoint(features[i].featureIds, numFeatures, contactPoint, &bodyB.getTransform(), static_cast<const hkpConvexShape*>(bodyA.getShape()), &bodyA.getTransform(), weldedNormal);
            points[i].m_position.m_quad = contactPoint.m_quad;
            if (weld == hkpConvexShape::WELD_RESULT_REJECT_CONTACT_POINT)
            {
                if (id != 0xFFFF)
                {
                    mgr->removeContactPointImpl(id, *output.m_constraintOwner);
                    ids[i] = 0xFFFF;
                }
                else if (output.m_potentialContacts.val())
                {
                    // the reservation given back
                    output.m_potentialContacts.val()->m_firstFreePotentialContact--;
                    mgr->reserveContactPointsImpl(-1);
                }
                continue;
            }
            if (weld == hkpConvexShape::WELD_RESULT_ACCEPT_CONTACT_POINT_MODIFIED)
            {
                points[i].m_separatingNormal.m_quad = setXYZ_W(weldedNormal.m_quad, points[i].m_separatingNormal.m_quad);
            }
        }
        hkpProcessCdPoint* cp = output.m_firstFreeContactPoint;
        numContactPoints++;
        output.m_firstFreeContactPoint = cp + 1;
        cp->m_contact.m_position.m_quad = points[i].m_position.m_quad;
        cp->m_contactPointId = id;
        cp->m_contact.m_separatingNormal.m_quad = points[i].m_separatingNormal.m_quad;
        cp->m_isShortestPoint = shortest != i ? 1 : 0;
        if (i == shortest && output.m_potentialContacts.val())
        {
            hkpProcessCollisionOutput::PotentialInfo* potentials = output.m_potentialContacts;
            *potentials->m_firstFreeRepresentativeContact = cp;
            potentials->m_firstFreeRepresentativeContact++;
        }
        ids[i] = id;
    }
    separatingNormal->m_quad = points[shortest].m_separatingNormal.m_quad;
    entry->m_numContactPoints = (unsigned char)numContactPoints;
    return static_cast<char*>(agentData) + 0x20;
}

// 0x00DBC200
void hkCapsuleTriangleAgent3::registerAgent3(hkpCollisionDispatcher* dispatcher)
{
    hkpCollisionDispatcher::Agent3Funcs f;
    f.m_createFunc = create;
    f.m_processFunc = process;
    f.m_sepNormalFunc = sepNormal;
    f.m_cleanupFunc = cleanup;
    f.m_removePointFunc = removePoint;
    f.m_commitPotentialFunc = commitPotential;
    f.m_createZombieFunc = createZombie;
    f.m_destroyFunc = destroy;
    dispatcher->registerAgent3(f, hkcdShapeType::CAPSULE, hkcdShapeType::TRIANGLE);
}
