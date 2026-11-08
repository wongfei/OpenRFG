// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/collide/hkpsimpleconstraintcontactmgr.cpp
//
// Havok 2014.2.5 (havok_impl.md HK8): the contact manager of colliding rigid bodies, from rfg.exe. Its contact
// constraint (an hkpSimpleContactConstraintData in an hkpConstraintInstance, both members) holds the agent's manifold
// points: the first point adds the constraint to the island, the last one removes it. Each new point gets its
// material (friction and restitution as the geometric means of the bodies'), the shape keys of both bodies and a
// first impulse estimate, then the contact point added callbacks may reject it. TOIs are handled by
// hkLs_doSimpleCollisionResponse (confirmToi). Also here: hkpSimpleContactConstraintAtomUtil::removeAtAndCopy (in
// hkpsimplecontactconstraintatomutil.cpp) and hkpSimpleContactConstraintData::freeContactPoint.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/collide/hkpsimpleconstraintcontactmgr.h"

#include <math.h>
#include <xmmintrin.h>

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/hkmath.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/router/hkmemoryrouter.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcdbody.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcollidable.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpprocesscdpoint.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpprocesscollisiondata.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpprocesscollisioninput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpprocesscollisionoutput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/hkpcollisiondispatcher.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/collide/deprecated/hkpcollisionevents.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/atom/hkpmodifierconstraintatom.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/atom/hkpsimplecontactconstraintatom.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/atom/hkpsimplecontactconstraintatomutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/contact/hkpcontactpointproperties.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/hkpconstraintowner.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkprigidbody.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/util/hkpentitycallbackutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/hkpworld.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/util/hkpworldcallbackutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/util/hkpworldconstraintutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/util/hkpworldoperationutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/dynamics/world/simulation/continuous/hkptoievent.h"

void hkLs_doSimpleCollisionResponse(hkpWorld* world, const hkpToiEvent& event, float rotateNormal, hkArray<hkpEntity*, hkContainerHeapAllocator>& toBeActivated); // hkpsimulation.cpp

// 0x00E1AF80: the constraint's priority comes from the collision quality of the two bodies; the rolling friction
// multiplier is the larger of the bodies', times 1.00390625 (0x3F808000, so its high half rounds up)
hkpSimpleConstraintContactMgr::hkpSimpleConstraintContactMgr(hkpWorld* sm, hkpRigidBody* bodyA, hkpRigidBody* bodyB)
    : hkpDynamicsContactMgr(hkpContactMgr::TYPE_SIMPLE_CONSTRAINT_CONTACT_MGR)
    , m_contactConstraintData(&m_constraint, bodyA, bodyB)
    , m_constraint(hkpConstraintInstance::PRIORITY_PSI)
{
    m_constraint.m_entities[0] = bodyA;
    m_constraint.m_data = &m_contactConstraintData;
    m_constraint.m_entities[1] = bodyB;
    m_constraint.m_uid = 0xFFFFFFFF;
    m_world = sm;
    m_reservedContactPoints = 0;
    m_contactPointCallbackDelay = 0;
    // members are never deleted on their own (their memory size becomes 0; an interlocked update in the binary)
    m_contactConstraintData.setMemorySizeAndFlags(0);
    m_constraint.setMemorySizeAndFlags(0);

    hkpCollisionDispatcher* dispatcher = sm->m_collisionDispatcher;
    const int quality = dispatcher->m_collisionQualityTable[bodyA->m_collidable.m_broadPhaseHandle.m_objectQualityType][bodyB->m_collidable.m_broadPhaseHandle.m_objectQualityType];
    m_constraint.setPriority(hkpConstraintInstance::ConstraintPriority(dispatcher->m_collisionQualityInfo[quality].m_constraintPriority));

    const __m128 a = _mm_set_ss(bodyA->m_material.m_rollingFrictionMultiplier.getReal());
    const __m128 b = _mm_set_ss(bodyB->m_material.m_rollingFrictionMultiplier.getReal());
    const float multiplier = _mm_cvtss_f32(_mm_max_ss(a, b)) * 1.00390625f;
    hkHalf half;
    half = multiplier; // the high 16 bits (hkHalf::operator=)
    m_contactConstraintData.m_atom->m_info.m_rollingFrictionMultiplier = half;
}

// 0x00E1B0A0: a constraint with points is still in the world
hkpSimpleConstraintContactMgr::~hkpSimpleConstraintContactMgr()
{
    if (m_contactConstraintData.m_atom->m_numContactPoints != 0)
    {
        hkpWorldOperationUtil::removeConstraintImmediately(m_world, &m_constraint, hkpWorldOperationUtil::DO_FIRE_CALLBACKS);
    }
    m_constraint.m_entities[0] = nullptr;
    m_constraint.m_entities[1] = nullptr;
    m_constraint.m_data = nullptr;
}

// 0x00E1B8B0: the reflection class (hkpSimpleConstraintContactMgrClass) is not part of this port
const hkClass* hkpSimpleConstraintContactMgr::getClassType() const
{
    return nullptr;
}

// 0x00E1A4A0
hkpConstraintInstance* hkpSimpleConstraintContactMgr::getConstraintInstance()
{
    return &m_constraint;
}

// 0x00E1A4B0
const hkpConstraintInstance* hkpSimpleConstraintContactMgr::getConstraintInstance() const
{
    return &m_constraint;
}

// 0x00E1E2D0
hkpSimpleContactConstraintAtom* hkpSimpleConstraintContactMgr::getAtom()
{
    return m_contactConstraintData.m_atom;
}

// 0x00E1A4C0: the reservation stays two points below the atom's maximum (the counter is 8 bits)
hkResult hkpSimpleConstraintContactMgr::reserveContactPointsImpl(int numPoints)
{
    const hkpSimpleContactConstraintAtom* atom = m_contactConstraintData.m_atom;
    if (m_reservedContactPoints + atom->m_numContactPoints + numPoints > atom->m_maxNumContactPoints - 2)
    {
        return HK_FAILURE;
    }
    m_reservedContactPoints = (unsigned char)(m_reservedContactPoints + numPoints);
    return HK_SUCCESS;
}

// 0x00E1B360
void hkpSimpleConstraintContactMgr::setRollingFrictionMultiplier(float multiplier)
{
    hkHalf half;
    half = multiplier * 1.00390625f; // 0x3F808000
    m_contactConstraintData.m_atom->m_info.m_rollingFrictionMultiplier = half;
}

// 0x00E1A510
void hkpSimpleConstraintContactMgr::toiCollisionResponseBeginCallback(const hkContactPoint& cp, hkpSimpleConstraintInfoInitInput& inA, hkpBodyVelocity& velA, hkpSimpleConstraintInfoInitInput& inB, hkpBodyVelocity& velB)
{
    m_contactConstraintData.collisionResponseBeginCallback(cp, inA, velA, inB, velB);
}

// 0x00E1A520
void hkpSimpleConstraintContactMgr::toiCollisionResponseEndCallback(const hkContactPoint& cp, float impulseApplied, hkpSimpleConstraintInfoInitInput& inA, hkpBodyVelocity& velA, hkpSimpleConstraintInfoInitInput& inB, hkpBodyVelocity& velB)
{
    m_contactConstraintData.collisionResponseEndCallback(cp, impulseApplied, inA, velA, inB, velB);
}

// 0x00E1A580: a TOI point is not in the atom (id 0xFFFF)
void hkpSimpleConstraintContactMgr::removeToiImpl(hkpConstraintOwner& constraintOwner, hkpContactPointProperties& properties)
{
    hkpEntity* entityA = m_constraint.m_entities[0];
    hkpEntity* entityB = m_constraint.m_entities[1];
    hkpContactPointRemovedEvent event(0xFFFF, this, &constraintOwner, &properties, entityA, entityB);
    hkpWorldCallbackUtil::fireContactPointRemoved(m_world, event);
    if (entityA->m_contactListeners.getSize() != 0)
    {
        hkpEntityCallbackUtil::fireContactPointRemovedInternal(entityA, event);
    }
    if (entityB->m_contactListeners.getSize() != 0)
    {
        hkpEntityCallbackUtil::fireContactPointRemovedInternal(entityB, event);
    }
}

// 0x00E1A600: the TOI's properties and the added callbacks (type TOI); a rejected TOI is removed again (and the
// rejection returned)
hkpContactMgr::ToiAccept hkpSimpleConstraintContactMgr::addToiImpl(const hkpCdBody& a, const hkpCdBody& b, const hkpProcessCollisionInput& input, hkpProcessCollisionOutput& output, float toi, hkContactPoint& cp, const hkpGskCache* gskCache, float& projectedVelocity, hkpContactPointProperties& properties)
{
    hkpEntity* entityA = setContactPointProperties(a, b, 7, &properties);
    hkpEntity* entityB = (hkpEntity*)((unsigned int)m_constraint.m_entities[1] ^ (unsigned int)m_constraint.m_entities[0] ^ (unsigned int)entityA);
    hkpToiPointAddedEvent event(this, &input, &output, &a, &b, &cp, gskCache, &properties, toi, projectedVelocity);
    hkpWorldCallbackUtil::fireContactPointAdded(m_world, event);
    if (entityA->m_contactListeners.getSize() != 0)
    {
        hkpEntityCallbackUtil::fireContactPointAddedInternal(entityA, event);
    }
    if (entityB->m_contactListeners.getSize() != 0)
    {
        hkpEntityCallbackUtil::fireContactPointAddedInternal(entityB, event);
    }
    if (event.m_status == HK_CONTACT_POINT_REJECT)
    {
        removeToiImpl(*output.m_constraintOwner.val(), properties);
        return TOI_REJECT;
    }
    projectedVelocity = event.m_projectedVelocity;
    return TOI_ACCEPT;
}

// 0x00E1A840: the removed callbacks see the point still in the atom; the last point takes the constraint out of the
// island
void hkpSimpleConstraintContactMgr::removeContactPointImpl(unsigned short cpId, hkpConstraintOwner& constraintOwner)
{
    hkpEntity* entityB = m_constraint.m_entities[1];
    hkpEntity* entityA = m_constraint.m_entities[0];
    hkpContactPointProperties* properties = m_contactConstraintData.getContactPointProperties(cpId);
    hkpContactPointRemovedEvent event(cpId, this, &constraintOwner, properties, entityA, entityB);
    hkpWorldCallbackUtil::fireContactPointRemoved(m_world, event);
    if (entityA->m_contactListeners.getSize() != 0)
    {
        hkpEntityCallbackUtil::fireContactPointRemovedInternal(entityA, event);
    }
    if (entityB->m_contactListeners.getSize() != 0)
    {
        hkpEntityCallbackUtil::fireContactPointRemovedInternal(entityB, event);
    }
    if (m_contactConstraintData.freeContactPoint(constraintOwner, cpId) == 0)
    {
        constraintOwner.removeConstraintFromCriticalLockedIsland(&m_constraint);
    }
}

// 0x00E1A910
hkContactPoint* hkpSimpleConstraintContactMgr::getContactPoint(unsigned short id)
{
    return &m_contactConstraintData.getContactPoint(id);
}

// 0x00E1A930
hkpContactPointProperties* hkpSimpleConstraintContactMgr::getContactPointProperties(unsigned short id)
{
    return m_contactConstraintData.getContactPointProperties(id);
}

// 0x00E1ADD0
void hkpSimpleConstraintContactMgr::getAllContactPointIds(hkArray<unsigned short, hkContainerHeapAllocator>& contactPointIds) const
{
    m_contactConstraintData.m_idMgrA.getAllUsedIds(contactPointIds);
}

// 0x00E1A990: the TOI's contact point callbacks (world, then each body's listeners); the TOI is handled unless the
// constraint is ignored or the point disabled
hkBool hkpSimpleConstraintContactMgr::fireCallbacksForEarliestToi(hkpToiEvent& event, float& rotateNormal)
{
    hkpContactPointEvent cpEvent(hkpCollisionEvent::SOURCE_WORLD, static_cast<hkpRigidBody*>(event.m_entities[0]), static_cast<hkpRigidBody*>(event.m_entities[1]), this, hkpContactPointEvent::TYPE_TOI, &event.m_contactPoint, &event.m_properties, &event.m_seperatingVelocity, &rotateNormal, false, false, false, event.m_extendedUserDatas, nullptr, nullptr);
    hkpWorldCallbackUtil::fireContactPointCallback(event.m_entities[0]->m_world, cpEvent);
    cpEvent.m_source = hkpCollisionEvent::SOURCE_A;
    if (event.m_entities[0]->m_contactListeners.getSize() != 0)
    {
        hkpEntityCallbackUtil::fireContactPointCallbackInternal(event.m_entities[0], cpEvent);
    }
    cpEvent.m_source = hkpCollisionEvent::SOURCE_B;
    if (event.m_entities[1]->m_contactListeners.getSize() != 0)
    {
        hkpEntityCallbackUtil::fireContactPointCallbackInternal(event.m_entities[1], cpEvent);
    }
    const hkpModifierConstraintAtom* modifiers = m_constraint.m_constraintModifiers;
    if ((modifiers == nullptr || modifiers->m_type.m_storage != hkpConstraintAtom::TYPE_MODIFIER_IGNORE_CONSTRAINT) && (event.m_properties.m_flags & hkContactPointMaterial::CONTACT_IS_DISABLED) == 0)
    {
        return true;
    }
    return false;
}

// 0x00E1AA80: an enabled TOI between bodies that are not both fixed or keyframed gets the simple collision response
void hkpSimpleConstraintContactMgr::confirmToi(hkpToiEvent& event, float rotateNormal, hkArray<hkpEntity*, hkContainerHeapAllocator>& outToBeActivated)
{
    if ((event.m_properties.m_flags & hkContactPointMaterial::CONTACT_IS_DISABLED) != 0)
    {
        return;
    }
    const unsigned char typeA = event.m_entities[0]->m_motion.m_type.m_storage;
    const unsigned char typeB = event.m_entities[1]->m_motion.m_type.m_storage;
    const bool fixedA = typeA == hkpMotion::MOTION_FIXED || typeA == hkpMotion::MOTION_KEYFRAMED;
    const bool fixedB = typeB == hkpMotion::MOTION_FIXED || typeB == hkpMotion::MOTION_KEYFRAMED;
    if (!fixedA || !fixedB)
    {
        hkLs_doSimpleCollisionResponse(event.m_entities[0]->m_world, event, rotateNormal, outToBeActivated);
    }
}

// 0x00E1AAD0: no point beyond the reservation limit (0xFFFF). The first point adds the constraint to the island. The
// point gets its properties and a projected velocity (the approach speed along the normal) for the added callbacks
// (type manifold, with the process callback delay); a rejected point is removed again. Without confirm callbacks
// (no restitution, no entity listeners) the point gets its first impulse right away, -0.2 * velocity / (inverse
// mass A + inverse mass B); otherwise the constraint asks for a new contact point callback.
unsigned short hkpSimpleConstraintContactMgr::addContactPointImpl(const hkpCdBody& a, const hkpCdBody& b, const hkpProcessCollisionInput& input, hkpProcessCollisionOutput& output, const hkpGskCache* contactCache, hkContactPoint& cp)
{
    const hkpSimpleContactConstraintAtom* atom = m_contactConstraintData.m_atom;
    if (m_reservedContactPoints + atom->m_numContactPoints > atom->m_maxNumContactPoints - 2)
    {
        return 0xFFFF;
    }
    if (atom->m_numContactPoints == 0)
    {
        output.m_constraintOwner.val()->addConstraintToCriticalLockedIsland(&m_constraint);
    }
    hkContactPoint* dcp;
    hkpContactPointProperties* props;
    const unsigned short id = m_contactConstraintData.allocateContactPoint(*output.m_constraintOwner.val(), &dcp, &props);
    *dcp = cp;
    hkpEntity* entityA = setContactPointProperties(a, b, 10000, props);
    hkpEntity* entityB = (hkpEntity*)((unsigned int)m_constraint.m_entities[1] ^ (unsigned int)m_constraint.m_entities[0] ^ (unsigned int)entityA);

    // the point velocities (angular x arm + linear) along the normal
    hkVector4f armA;
    armA.setSub(cp.m_position, entityA->m_motion.m_motionState.m_sweptTransform.m_centerOfMass1);
    hkVector4f velA;
    velA.setCross(entityA->m_motion.m_angularVelocity, armA);
    hkVector4f armB;
    armB.setSub(cp.m_position, entityB->m_motion.m_motionState.m_sweptTransform.m_centerOfMass1);
    hkVector4f velB;
    velB.setCross(entityB->m_motion.m_angularVelocity, armB);
    velB.add(entityB->m_motion.m_linearVelocity);
    velA.add(entityA->m_motion.m_linearVelocity);
    hkVector4f relVel;
    relVel.setSub(velA, velB);
    const hkSimdFloat32 projectedVelocity = relVel.dot<3>(cp.m_separatingNormal);

    props->m_impulseApplied = 0.0f;
    props->m_userData = 0;
    hkpManifoldPointAddedEvent event(id, this, &input, &output, &a, &b, &cp, contactCache, props, projectedVelocity.getReal());
    event.m_nextProcessCallbackDelay = m_type == TYPE_SIMPLE_CONSTRAINT_CONTACT_MGR ? m_contactPointCallbackDelay : 0;
    hkpWorldCallbackUtil::fireContactPointAdded(m_world, event);
    if (entityA->m_contactListeners.getSize() != 0)
    {
        hkpEntityCallbackUtil::fireContactPointAddedInternal(entityA, event);
    }
    if (entityB->m_contactListeners.getSize() != 0)
    {
        hkpEntityCallbackUtil::fireContactPointAddedInternal(entityB, event);
    }
    if (event.m_status == HK_CONTACT_POINT_REJECT)
    {
        removeContactPointImpl(id, *output.m_constraintOwner.val());
        return 0xFFFF;
    }
    m_contactPointCallbackDelay = event.m_nextProcessCallbackDelay;
    if (input.m_allowToSkipConfirmedCallbacks.m_bool && (float)props->m_restitution * 0.0078125f == 0.0f && entityA->m_contactListeners.getSize() == 0 && entityB->m_contactListeners.getSize() == 0)
    {
        // 1 / (invMassB + invMassA + 1e-10) with one Newton step; 1e-10f is 0x2EDBE6FF, -0.2f is 0xBE4CCCCD
        const __m128 invMassA = _mm_shuffle_ps(entityA->m_motion.m_inertiaAndMassInv.m_quad, entityA->m_motion.m_inertiaAndMassInv.m_quad, 0xFF);
        const __m128 invMassB = _mm_shuffle_ps(entityB->m_motion.m_inertiaAndMassInv.m_quad, entityB->m_motion.m_inertiaAndMassInv.m_quad, 0xFF);
        const __m128 sum = _mm_add_ps(_mm_add_ps(invMassB, invMassA), _mm_set1_ps(1e-10f));
        const __m128 r0 = _mm_rcp_ps(sum);
        const __m128 r1 = _mm_mul_ps(_mm_sub_ps(g_vectorfConstants[HK_QUADREAL_2], _mm_mul_ps(r0, sum)), r0);
        const __m128 impulse = _mm_mul_ps(_mm_mul_ps(_mm_set1_ps(-0.2f), r1), projectedVelocity.m_real);
        props->m_impulseApplied = _mm_cvtss_f32(impulse);
        props->m_internalSolverData = 0.0f;
        props->m_internalDataA = 0.0f;
        return id;
    }
    output.m_constraintOwner.val()->addCallbackRequest(&m_constraint, hkpConstraintAtom::CALLBACK_REQUEST_NEW_CONTACT_POINT);
    return id;
}

// 0x00E1ADE0: the process callbacks run every (delay + 1)th step (the delay is the smaller of the bodies'), then the
// constraint asks for contact point callbacks; the agent's points are copied into the atom every step
void hkpSimpleConstraintContactMgr::processContactImpl(const hkpCollidable& a, const hkpCollidable& b, const hkpProcessCollisionInput& input, hkpProcessCollisionData& collisionData)
{
    unsigned short delay = (unsigned short)(m_contactPointCallbackDelay - 1);
    if (m_contactPointCallbackDelay == 0)
    {
        hkpEntity* entityA = static_cast<hkpEntity*>(a.getOwner());
        hkpEntity* entityB = static_cast<hkpEntity*>(b.getOwner());
        delay = entityA->m_contactPointCallbackDelay < entityB->m_contactPointCallbackDelay ? entityA->m_contactPointCallbackDelay : entityB->m_contactPointCallbackDelay;

        hkpContactProcessEvent event(this, &a, &b, &collisionData);
        hkpContactPointProperties** properties = event.m_contactPointProperties;
        for (hkpProcessCdPoint* point = collisionData.getFirstContactPoint(); point < collisionData.m_firstFreeContactPoint.val(); point++)
        {
            *properties++ = m_contactConstraintData.getContactPointProperties(point->m_contactPointId);
        }
        hkpWorldCallbackUtil::fireContactProcess(m_world, event);
        if (entityA->m_contactListeners.getSize() != 0)
        {
            hkpEntityCallbackUtil::fireContactProcessInternal(entityA, event);
        }
        if (entityB->m_contactListeners.getSize() != 0)
        {
            hkpEntityCallbackUtil::fireContactProcessInternal(entityB, event);
        }
        collisionData.m_constraintOwner.val()->addCallbackRequest(m_contactConstraintData.m_constraint, hkpConstraintAtom::CALLBACK_REQUEST_CONTACT_POINT_CALLBACK);
    }
    m_contactPointCallbackDelay = delay;
    for (hkpProcessCdPoint* point = collisionData.getFirstContactPoint(); point < collisionData.m_firstFreeContactPoint.val(); point++)
    {
        m_contactConstraintData.getContactPoint(point->m_contactPointId) = point->m_contact;
    }
}

// 0x00E1B890
void hkpSimpleConstraintContactMgr::cleanup()
{
    delete this;
}

// 0x00E1B8A0
hkpContactMgr::Type hkpSimpleConstraintContactMgr::getType() const
{
    return TYPE_SIMPLE_CONSTRAINT_CONTACT_MGR;
}

// 0x00E1B6B0
hkBool hkpSimpleConstraintContactMgr::isConstraintDisabled() const
{
    const hkpModifierConstraintAtom* modifiers = m_constraint.m_constraintModifiers;
    return modifiers != nullptr && modifiers->m_type.m_storage == hkpConstraintAtom::TYPE_MODIFIER_IGNORE_CONSTRAINT;
}

// 0x00E1B390: friction sqrt(fA * fB) and restitution sqrt(rA * rB) * 128 (truncated), no max impulse; the shape keys
// of each body's hierarchy (from the leaf, skipping an invalid leaf key) go into the extended user datas, A's first,
// at most maxNumExtraDataInEvent in all. Returns the entity of body a.
hkpRigidBody* hkpSimpleConstraintContactMgr::setContactPointProperties(const hkpCdBody& a, const hkpCdBody& b, int maxNumExtraDataInEvent, hkpContactPointProperties* cpi)
{
    hkpEntity* entityA = m_constraint.m_entities[0];
    hkpEntity* entityB = m_constraint.m_entities[1];
    float friction = _mm_cvtss_f32(_mm_sqrt_ss(_mm_set_ss(entityB->m_material.m_friction * entityA->m_material.m_friction)));
    cpi->m_friction = friction;
    const float restitution = _mm_cvtss_f32(_mm_sqrt_ss(_mm_set_ss(entityB->m_material.m_restitution * entityA->m_material.m_restitution)));
    cpi->m_restitution = (unsigned char)(int)(restitution * 128.0f); // 0x43000000, truncated
    cpi->m_maxImpulse.m_value = 0;

    hkpEntity* ownerOfA = static_cast<hkpEntity*>(a.getRootCollidable()->getOwner());
    const hkpSimpleContactConstraintAtom* atom = m_contactConstraintData.m_atom;
    int numB = atom->m_numUserDatasForBodyB;
    int numA = atom->m_numUserDatasForBodyA;
    if (numA + numB == 0)
    {
        return static_cast<hkpRigidBody*>(ownerOfA);
    }
    const hkpCdBody* bodyA;
    const hkpCdBody* bodyB;
    hkpEntity* result;
    if (ownerOfA == entityA)
    {
        bodyA = &a;
        bodyB = &b;
        result = ownerOfA;
    }
    else
    {
        bodyA = &b;
        bodyB = &a;
        result = entityB;
    }
    if (numA >= maxNumExtraDataInEvent)
    {
        numA = maxNumExtraDataInEvent;
    }
    if (numB >= maxNumExtraDataInEvent - numA)
    {
        numB = maxNumExtraDataInEvent - numA;
    }
    unsigned int* userDatas = reinterpret_cast<unsigned int*>(cpi + 1);
    if (bodyA->m_shapeKey == 0xFFFFFFFF && bodyA->m_parent != nullptr)
    {
        bodyA = bodyA->m_parent;
    }
    for (int i = 0; bodyA != nullptr && i < numA; i++)
    {
        userDatas[i] = bodyA->m_shapeKey;
        bodyA = bodyA->m_parent;
    }
    if (bodyB->m_shapeKey == 0xFFFFFFFF && bodyB->m_parent != nullptr)
    {
        bodyB = bodyB->m_parent;
    }
    for (int i = 0; bodyB != nullptr && i < numB; i++)
    {
        userDatas[numA + i] = bodyB->m_shapeKey;
        bodyB = bodyB->m_parent;
    }
    return static_cast<hkpRigidBody*>(result);
}

// 0x00E1A970
hkpSimpleConstraintContactMgr::Factory::Factory(hkpWorld* mgr)
    : m_world(mgr)
{
}

// 0x00E1B590
hkpSimpleConstraintContactMgr::Factory::~Factory()
{
}

// 0x00E1B100
hkpContactMgr* hkpSimpleConstraintContactMgr::Factory::createContactMgr(const hkpCollidable& a, const hkpCollidable& b, const hkpCollisionInput& input)
{
    return new hkpSimpleConstraintContactMgr(m_world, static_cast<hkpRigidBody*>(static_cast<hkpEntity*>(a.getOwner())), static_cast<hkpRigidBody*>(static_cast<hkpEntity*>(b.getOwner())));
}

// 0x00E1A710 (hkpSimpleContactConstraintData): the point leaves the atom (the later points move down, the id map
// follows), the next point loses the second solver path flag, the atom may shrink, and the owner and the constraint
// shrink by the schema sizes of one point. Returns the number of points left. The point removed and area changed
// flags are set.
int hkpSimpleContactConstraintData::freeContactPoint(hkpConstraintOwner& constraintOwner, unsigned short id)
{
    const int index = m_idMgrA.m_values[id];
    m_idMgrA.m_values[id] = hkpDynamicsCpIdMgr::FREE_VALUE;
    hkpSimpleContactConstraintAtom* oldAtom = m_atom;
    const int numPoints = oldAtom->m_numContactPoints;
    const int thirdFriction = numPoints == 2 ? 1 : 0;
    int schemaDelta = thirdFriction ? hkpJacobianSchemaInfo::Friction3D::Sizeof - hkpJacobianSchemaInfo::Friction2D::Sizeof : 0;
    if ((numPoints & 1) == 0)
    {
        schemaDelta += hkpJacobianSchemaInfo::PairContact::Sizeof - hkpJacobianSchemaInfo::SingleContact::Sizeof;
    }
    else
    {
        schemaDelta += hkpJacobianSchemaInfo::SingleContact::Sizeof;
    }
    hkpSimpleContactConstraintAtomUtil::removeAtAndCopy(oldAtom, index);
    // the point now at the removed index (the old next one)
    hkpContactPointProperties* next = oldAtom->getContactPointPropertiesStream(index)->asProperties();
    next->m_flags &= ~hkContactPointMaterial::CONTACT_USES_SOLVER_PATH2;
    hkPadSpu<bool> atomReallocated(false);
    m_atom = hkpSimpleContactConstraintAtomUtil::optimizeCapacity(m_atom, 1, atomReallocated);
    for (int i = m_idMgrA.m_values.getSize() - 1; i >= 0; i--)
    {
        unsigned char& value = m_idMgrA.m_values[i];
        if (value != hkpDynamicsCpIdMgr::FREE_VALUE && index < value)
        {
            value--;
        }
    }
    if (m_constraint->m_internal != nullptr)
    {
        constraintOwner.m_constraintInfo.m_sizeOfSchemas -= schemaDelta;
        constraintOwner.m_constraintInfo.m_numSolverResults -= thirdFriction + 1;
        constraintOwner.m_constraintInfo.m_numSolverElemTemps -= thirdFriction + 1;
        hkConstraintInternal* internal = m_constraint->m_internal;
        internal->m_numSolverResults = (unsigned short)(internal->m_numSolverResults - (thirdFriction + 1));
        internal->m_numSolverElemTemps -= thirdFriction + 1;
        internal->m_sizeOfSchemas -= schemaDelta;
    }
    if (atomReallocated.val())
    {
        hkpWorldConstraintUtil::updateFatherOfMovedAtom(m_constraint, oldAtom, m_atom, m_atom->m_sizeOfAllAtoms);
        m_atomSize = m_atom->m_sizeOfAllAtoms;
    }
    m_atom->m_info.m_flags |= hkpSimpleContactConstraintDataInfo::HK_FLAG_POINT_REMOVED | hkpSimpleContactConstraintDataInfo::HK_FLAG_AREA_CHANGED;
    return numPoints - 1;
}

// ---- reflection data, generated by tools/hdrgen/reflgen.py from rfg.exe (do not edit)
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/hkclass.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/hkclassmember.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/hkinternalclassmember.h"

extern const hkClass hkReferencedObjectClass;

//
// Class hkpSimpleConstraintContactMgr
//
extern const hkClass hkpSimpleConstraintContactMgrClass;
const hkClass hkpSimpleConstraintContactMgrClass(
    "hkpSimpleConstraintContactMgr",
    &hkReferencedObjectClass, // parent
    128, // object size
    nullptr,
    0, // interfaces
    nullptr,
    0, // enums
    nullptr,
    0, // members
    nullptr, // defaults
    nullptr, // attributes
    0, // flags
    0 // version
    );
// ---- end of generated reflection data
