// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/contact/hkpsimplecontactconstraintdata.cpp
//
// Havok 2014.2.5 (havok_impl.md HK8): the data of a contact constraint, from rfg.exe. It owns the contact atom
// (points and properties), maps contact point ids to atom indices (hkpDynamicsCpIdMgr) and keeps its owner's and its
// constraint's solver sizes in step as points come and go. The destructor (hkpsimplecontactconstraintdata.inl) and
// the id manager's out-of-line members (hkpdynamicscpidmgr.inl) are here too.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/contact/hkpsimplecontactconstraintdata.h"

#include <xmmintrin.h>

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/hkmath.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/accumulator/hkpvelocityaccumulator.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/constraint/hkpconstraintqueryin.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpprocesscollisioninput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/collide/hkpsimpleconstraintcontactmgr.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/atom/hkpmodifierconstraintatom.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/atom/hkpsimplecontactconstraintatom.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/atom/hkpsimplecontactconstraintatomutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/contact/hkpcontactimpulselimitbreachedlistener.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/contact/hkpcontactpointproperties.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/contact/hkpsimplecontactconstraintutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/hkpconstraintinstance.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/hkpconstraintowner.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/response/hkpsimplecollisionresponse.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkprigidbody.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/util/hkpentitycallbackutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/hkpworld.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/util/hkpworldcallbackutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/util/hkpworldconstraintutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/solver/simpleconstraints/hkpsimpleconstraintutil.h"

// ---- hkpDynamicsCpIdMgr (hkpdynamicscpidmgr.inl)

// 0x00E30350
hkpDynamicsCpIdMgr::hkpDynamicsCpIdMgr()
{
}

// 0x00E1B790
hkpDynamicsCpIdMgr::~hkpDynamicsCpIdMgr()
{
}

// 0x00E301F0: the last free id, else a new one at the end
int hkpDynamicsCpIdMgr::newId(int value)
{
    for (int i = m_values.getSize() - 1; i >= 0; i--)
    {
        if (m_values[i] == FREE_VALUE)
        {
            m_values[i] = (unsigned char)value;
            return i;
        }
    }
    m_values.pushBack((unsigned char)value);
    return m_values.getSize() - 1;
}

// 0x00E1B290
void hkpDynamicsCpIdMgr::freeId(int id)
{
    m_values[id] = FREE_VALUE;
}

// 0x00E1B2A0
void hkpDynamicsCpIdMgr::decrementValuesGreater(int relIndex)
{
    for (int i = m_values.getSize() - 1; i >= 0; i--)
    {
        if (m_values[i] != FREE_VALUE && m_values[i] > relIndex)
        {
            m_values[i]--;
        }
    }
}

// 0x00E1B6F0
void hkpDynamicsCpIdMgr::getAllUsedIds(hkArray<unsigned short, hkContainerHeapAllocator>& ids) const
{
    for (int i = 0; i < m_values.getSize(); i++)
    {
        if (m_values[i] != FREE_VALUE)
        {
            ids.pushBack((unsigned short)i);
        }
    }
}

// 0x00E1B2D0
unsigned char hkpDynamicsCpIdMgr::getValueAt(int id) const
{
    return m_values[id];
}

// ---- hkpSimpleContactConstraintData

// 0x00E2FD40: one reserved point; the maximum number of points is what fits in 0x3400 bytes of points and properties
hkpSimpleContactConstraintData::hkpSimpleContactConstraintData(hkpConstraintInstance* constraint, hkpRigidBody* bodyA, hkpRigidBody* bodyB)
{
    m_constraint = constraint;
    const int numExtraUserDatasA = bodyA->m_numShapeKeysInContactPointProperties;
    const int numExtraUserDatasB = bodyB->m_numShapeKeysInContactPointProperties;
    const int bytesPerPoint = (numExtraUserDatasA + numExtraUserDatasB) * 4 + 0x34; // hkContactPoint + hkpContactPointProperties + user datas
    m_atom = hkpSimpleContactConstraintAtomUtil::allocateAtom(1, numExtraUserDatasA, numExtraUserDatasB, 0x3400 / bytesPerPoint);
    m_atom->m_info.init();
    m_atomSize = m_atom->m_sizeOfAllAtoms;
}

// 0x00E1B7D0 (hkpsimplecontactconstraintdata.inl)
hkpSimpleContactConstraintData::~hkpSimpleContactConstraintData()
{
    if (m_atom != nullptr)
    {
        hkpSimpleContactConstraintAtomUtil::deallocateAtom(m_atom);
    }
}

// 0x00E2F610: the data is a member of the manager (at 0x14)
hkpSimpleConstraintContactMgr* hkpSimpleContactConstraintData::getSimpleConstraintContactMgr() const
{
    return reinterpret_cast<hkpSimpleConstraintContactMgr*>(const_cast<char*>(reinterpret_cast<const char*>(this) - 0x14));
}

// 0x00F20C60
int hkpSimpleContactConstraintData::getNumContactPoints() const
{
    return m_atom->m_numContactPoints;
}

// 0x00E1B2E0
hkContactPoint& hkpSimpleContactConstraintData::getContactPoint(int id)
{
    return m_atom->getContactPoints()[m_idMgrA.getValueAt(id)];
}

// 0x00E1B300
hkpContactPointProperties* hkpSimpleContactConstraintData::getContactPointProperties(int id)
{
    const int index = m_idMgrA.getValueAt(id);
    if (index == hkpDynamicsCpIdMgr::FREE_VALUE)
    {
        return nullptr;
    }
    return m_atom->getContactPointPropertiesStream(index)->asProperties();
}

// 0x00E2FC10: the new point's properties start with zero results and the "new" flag; a point after one that does not
// use the second solver path inherits it unless that one has a max impulse. The owner and the constraint grow by the
// schema sizes of one more point (and of 3d friction from the second point). The area changed flag is set.
unsigned short hkpSimpleContactConstraintData::allocateContactPoint(hkpConstraintOwner& constraintOwner, hkContactPoint** cpOut, hkpContactPointProperties** cpPropsOut)
{
    m_atom->m_info.m_flags |= hkpSimpleContactConstraintDataInfo::HK_FLAG_AREA_CHANGED;
    const int index = m_atom->m_numContactPoints;
    hkpSimpleContactConstraintAtom* oldAtom = m_atom;
    hkPadSpu<bool> atomReallocated(false);
    hkpSimpleContactConstraintAtom* atom = hkpSimpleContactConstraintAtomUtil::expandOne(oldAtom, atomReallocated);
    m_atom = atom;

    const int striding = atom->m_contactPointPropertiesStriding;
    hkpContactPointProperties* props = reinterpret_cast<hkpContactPointProperties*>(reinterpret_cast<char*>(atom->getContactPointPropertiesStream()) + striding * index);
    props->m_impulseApplied = 0.0f;
    props->m_internalSolverData = 0.0f;
    props->m_flags = hkContactPointMaterial::CONTACT_IS_NEW;
    if (index != 0)
    {
        const hkpContactPointProperties* prev = reinterpret_cast<const hkpContactPointProperties*>(reinterpret_cast<const char*>(props) - striding);
        if ((prev->m_flags & hkContactPointMaterial::CONTACT_USES_SOLVER_PATH2) == 0 && prev->m_maxImpulse.m_value == 0)
        {
            props->m_flags = hkContactPointMaterial::CONTACT_IS_NEW | hkContactPointMaterial::CONTACT_USES_SOLVER_PATH2;
        }
    }
    *cpOut = atom->getContactPoints() + index;
    *cpPropsOut = props;

    // the schema growth from index to index + 1 points (see hkpSimpleContactConstraintAtom::addToConstraintInfo)
    const int numPoints = index + 1;
    hkpConstraintInfo delta;
    delta.m_maxSizeOfSchema = (numPoints >> 1) * hkpJacobianSchemaInfo::PairContact::Sizeof + 0xA0 + (numPoints & 1) * hkpJacobianSchemaInfo::SingleContact::Sizeof;
    const int thirdFriction = index == 1 ? 1 : 0;
    delta.m_sizeOfSchemas = thirdFriction ? hkpJacobianSchemaInfo::Friction3D::Sizeof - hkpJacobianSchemaInfo::Friction2D::Sizeof : 0;
    delta.m_numSolverElemTemps = thirdFriction;
    delta.m_numSolverResults = thirdFriction + 1;
    if ((numPoints & 1) == 0)
    {
        delta.m_sizeOfSchemas += hkpJacobianSchemaInfo::PairContact::Sizeof - hkpJacobianSchemaInfo::SingleContact::Sizeof;
    }
    else
    {
        delta.m_sizeOfSchemas += hkpJacobianSchemaInfo::SingleContact::Sizeof;
    }
    delta.m_numSolverElemTemps += 1;
    constraintOwner.addConstraintInfo(m_constraint, delta);

    if (atomReallocated.val())
    {
        hkpWorldConstraintUtil::updateFatherOfMovedAtom(m_constraint, oldAtom, m_atom, atom->m_sizeOfAllAtoms);
        m_atomSize = atom->m_sizeOfAllAtoms;
    }
    return (unsigned short)m_idMgrA.newId(index);
}

// 0x00E2F550
int hkpSimpleContactConstraintData::getType() const
{
    return CONSTRAINT_TYPE_CONTACT;
}

// 0x00E2F570: the atom's own info (hkpSimpleContactConstraintAtom::addToConstraintInfo, inlined)
void hkpSimpleContactConstraintData::getConstraintInfo(hkpConstraintData::ConstraintInfo& infoOut) const
{
    infoOut.m_atoms = m_atom;
    infoOut.m_sizeOfAllAtoms = m_atom->m_sizeOfAllAtoms;
    infoOut.m_maxSizeOfSchema = 0;
    infoOut.m_sizeOfSchemas = 0;
    infoOut.m_numSolverResults = 0;
    infoOut.m_numSolverElemTemps = 0;
    m_atom->addToConstraintInfo(infoOut);
}

// 0x00E2F540
hkBool hkpSimpleContactConstraintData::isValid() const
{
    return true;
}

// 0x00E2F510: no runtime
void hkpSimpleContactConstraintData::getRuntimeInfo(hkBool wantRuntime, hkpConstraintData::RuntimeInfo& infoOut) const
{
    infoOut.m_sizeOfExternalRuntime = 0;
    infoOut.m_numSolverResults = 0;
}

// 0x00E2F530
hkpSolverResults* hkpSimpleContactConstraintData::getSolverResults(hkpConstraintRuntime* runtime) const
{
    return nullptr;
}

// 0x00E2FB40: the modifiers from the outside in
void hkpSimpleContactConstraintData::collisionResponseBeginCallback(const hkContactPoint& cp, hkpSimpleConstraintInfoInitInput& inA, hkpBodyVelocity& velA, hkpSimpleConstraintInfoInitInput& inB, hkpBodyVelocity& velB)
{
    hkpConstraintAtom* atom = m_constraint->m_constraintModifiers;
    if (atom == nullptr)
    {
        return;
    }
    while (atom->m_type.m_storage > hkpConstraintAtom::TYPE_CONTACT)
    {
        switch (atom->m_type.m_storage)
        {
        case hkpConstraintAtom::TYPE_MODIFIER_SOFT_CONTACT:
            static_cast<hkpSoftContactModifierConstraintAtom*>(atom)->collisionResponseBeginCallback(cp, inA, velA, inB, velB);
            break;
        case hkpConstraintAtom::TYPE_MODIFIER_MASS_CHANGER:
            static_cast<hkpMassChangerModifierConstraintAtom*>(atom)->collisionResponseBeginCallback(cp, inA, velA, inB, velB);
            break;
        case hkpConstraintAtom::TYPE_MODIFIER_MOVING_SURFACE:
            static_cast<hkpMovingSurfaceModifierConstraintAtom*>(atom)->collisionResponseBeginCallback(cp, inA, velA, inB, velB);
            break;
        case hkpConstraintAtom::TYPE_MODIFIER_CENTER_OF_MASS_CHANGER:
            static_cast<hkpCenterOfMassChangerModifierConstraintAtom*>(atom)->collisionResponseBeginCallback(cp, inA, velA, inB, velB);
            break;
        default:
            break;
        }
        atom = static_cast<hkpModifierConstraintAtom*>(atom)->m_child;
    }
}

// 0x00E2FE00: the modifiers from the inside out (collected into a 16-entry local array first)
void hkpSimpleContactConstraintData::collisionResponseEndCallback(const hkContactPoint& cp, float impulseApplied, hkpSimpleConstraintInfoInitInput& inA, hkpBodyVelocity& velA, hkpSimpleConstraintInfoInitInput& inB, hkpBodyVelocity& velB)
{
    if (m_constraint->m_constraintModifiers == nullptr)
    {
        return;
    }
    hkInplaceArray<hkpModifierConstraintAtom*, 16, hkContainerHeapAllocator> modifiers;
    hkpConstraintAtom* atom = m_constraint->m_constraintModifiers;
    while (atom->m_type.m_storage > hkpConstraintAtom::TYPE_CONTACT)
    {
        modifiers.pushBack(static_cast<hkpModifierConstraintAtom*>(atom));
        atom = static_cast<hkpModifierConstraintAtom*>(atom)->m_child;
    }
    for (int i = modifiers.getSize() - 1; i >= 0; i--)
    {
        hkpModifierConstraintAtom* modifier = modifiers[i];
        switch (modifier->m_type.m_storage)
        {
        case hkpConstraintAtom::TYPE_MODIFIER_SOFT_CONTACT:
            static_cast<hkpSoftContactModifierConstraintAtom*>(modifier)->collisionResponseEndCallback(cp, impulseApplied, inA, velA, inB, velB);
            break;
        case hkpConstraintAtom::TYPE_MODIFIER_MASS_CHANGER:
            static_cast<hkpMassChangerModifierConstraintAtom*>(modifier)->collisionResponseEndCallback(cp, impulseApplied, inA, velA, inB, velB);
            break;
        case hkpConstraintAtom::TYPE_MODIFIER_MOVING_SURFACE:
            static_cast<hkpMovingSurfaceModifierConstraintAtom*>(modifier)->collisionResponseEndCallback(cp, impulseApplied, inA, velA, inB, velB);
            break;
        case hkpConstraintAtom::TYPE_MODIFIER_CENTER_OF_MASS_CHANGER:
            static_cast<hkpCenterOfMassChangerModifierConstraintAtom*>(modifier)->collisionResponseEndCallback(cp, impulseApplied, inA, velA, inB, velB);
            break;
        default:
            break;
        }
    }
}

// 0x00E2F620: called while the Jacobians are built when the constraint asked for callbacks. New points fire a contact
// point callback (with their separating velocity, which the listeners may change) and get their first impulse: none
// for fixed pairs, disabled points or ignored constraints; a bouncing point (approaching faster than the resting
// velocity with restitution above 0.3) is solved alone (solveSingleContact2); every other point gets an estimate,
// -0.2 * (1 + restitution) * velocity / (inverse mass A + inverse mass B), and its restitution data. Old points fire a
// callback only when contact point callbacks were requested. The points go front to back while the counter i goes
// from the last index down, so "first" and "last" mean the order of the calls.
void hkSimpleContactConstraintData_fireCallbacks(hkpSimpleContactConstraintData* constraintData, const hkpConstraintQueryIn* in, hkpSimpleContactConstraintAtom* atom, hkpContactPointEvent::Type type)
{
    hkpConstraintInstance* instance = in->m_constraintInstance;
    hkpSimpleConstraintContactMgr* mgr = constraintData->getSimpleConstraintContactMgr();
    hkpEntity* entityA = instance->m_entities[0];
    hkpWorld* world = entityA->m_world;
    hkpEntity* entityB = instance->m_entities[1];
    hkpVelocityAccumulator* accA = const_cast<hkpVelocityAccumulator*>(in->m_bodyA.val());
    hkpVelocityAccumulator* accB = const_cast<hkpVelocityAccumulator*>(in->m_bodyB.val());
    const bool callbackRequested = (instance->m_internal->m_callbackRequest & hkpConstraintAtom::CALLBACK_REQUEST_CONTACT_POINT_CALLBACK) != 0;
    const int striding = atom->m_contactPointPropertiesStriding;

    hkContactPoint* cp = atom->getContactPoints();
    hkpContactPointProperties* props = atom->getContactPointPropertiesStream()->asProperties();
    hkpContactPointProperties* prevProps = nullptr;
    const int lastIndex = atom->m_numContactPoints - 1;
    for (int i = lastIndex; i >= 0; i--)
    {
        if ((props->m_flags & hkContactPointMaterial::CONTACT_IS_NEW) != 0)
        {
            float separatingVelocity = hkpSimpleContactConstraintUtil::calculateSeparatingVelocity(static_cast<hkpRigidBody*>(entityA), static_cast<hkpRigidBody*>(entityB), accA->getCenterOfMassInWorld(), accB->getCenterOfMassInWorld(), cp).getReal();
            {
                hkpContactPointEvent event(hkpCollisionEvent::SOURCE_WORLD, static_cast<hkpRigidBody*>(entityA), static_cast<hkpRigidBody*>(entityB), mgr, type, cp, props, &separatingVelocity, nullptr, callbackRequested, i == lastIndex, i == 0, reinterpret_cast<unsigned int*>(props + 1), accA, accB);
                hkpWorldCallbackUtil::fireContactPointCallback(world, event);
                event.m_source = hkpCollisionEvent::SOURCE_A;
                if (entityA->m_contactListeners.getSize() != 0)
                {
                    hkpEntityCallbackUtil::fireContactPointCallbackInternal(entityA, event);
                }
                event.m_source = hkpCollisionEvent::SOURCE_B;
                if (entityB->m_contactListeners.getSize() != 0)
                {
                    hkpEntityCallbackUtil::fireContactPointCallbackInternal(entityB, event);
                }
            }
            const __m128 sepVel = _mm_set1_ps(separatingVelocity);
            if (i < lastIndex && prevProps->m_maxImpulse.m_value != 0)
            {
                props->m_flags &= ~hkContactPointMaterial::CONTACT_USES_SOLVER_PATH2;
            }

            const __m128 zero = _mm_setzero_ps();
            const int massMaskB = _mm_movemask_ps(_mm_cmpneq_ps(accB->m_invMasses.m_quad, zero));
            const int massMaskA = _mm_movemask_ps(_mm_cmpneq_ps(accA->m_invMasses.m_quad, zero));
            const hkpModifierConstraintAtom* modifiers = mgr->m_constraint.m_constraintModifiers;
            if ((massMaskA | massMaskB) == 0 || (props->m_flags & hkContactPointMaterial::CONTACT_IS_DISABLED) != 0 || (modifiers != nullptr && modifiers->m_type.m_storage == hkpConstraintAtom::TYPE_MODIFIER_IGNORE_CONSTRAINT))
            {
                props->m_internalSolverData = 0.0f;
                props->m_internalDataA = 0.0f;
                props->m_impulseApplied = 0.0f;
                props->m_flags &= ~hkContactPointMaterial::CONTACT_IS_NEW;
            }
            else
            {
                const float restitutionScalar = (float)props->m_restitution * 0.0078125f; // 1 / 128
                const __m128 restitution = _mm_set1_ps(restitutionScalar);
                const hkpWorldDynamicsStepInfo* stepInfo = static_cast<const hkpWorldDynamicsStepInfo*>(world->m_collisionInput->m_dynamicsInfo);
                const __m128 negResting = _mm_sub_ps(zero, _mm_set1_ps(stepInfo->m_solverInfo.m_contactRestingVelocity));
                if (!(_mm_cvtss_f32(sepVel) >= _mm_cvtss_f32(negResting)) && restitutionScalar > 0.3f) // 0x3E99999A (unordered: bounce)
                {
                    hkpSimpleConstraintUtilCollideParams params;
                    params.m_friction = props->getFriction();
                    params.m_restitution = restitutionScalar;
                    params.m_extraSeparatingVelocity = 0.0f;
                    params.m_extraSlope = 0.0f;
                    params.m_externalSeperatingVelocity = _mm_cvtss_f32(sepVel);
                    params.m_maxImpulse = HK_REAL_MAX;
                    if (props->m_maxImpulse.m_value != 0)
                    {
                        params.m_maxImpulse = props->getMaxImpulsePerStep();
                    }
                    hkpSimpleCollisionResponse::SolveSingleOutput2 output;
                    hkpSimpleCollisionResponse::solveSingleContact2(constraintData, *cp, params, static_cast<hkpRigidBody*>(entityA), static_cast<hkpRigidBody*>(entityB), accA, accB, output);
                    if (params.m_contactImpulseLimitBreached.m_bool)
                    {
                        hkpContactImpulseLimitBreachedListenerInfo::SingleImpulseElem info;
                        info.m_type = 1;
                        info.m_isContact = 1;
                        info.m_constraintInstance = in->m_constraintInstance;
                        info.m_properties = props;
                        info.m_contactPoint = cp;
                        hkpWorldCallbackUtil::fireContactImpulseLimitBreached(world, reinterpret_cast<const hkpContactImpulseLimitBreachedListenerInfo*>(&info), 1);
                    }
                    props->m_internalDataA = 0.0f;
                    props->m_impulseApplied = 0.0f;
                    props->m_flags &= ~hkContactPointMaterial::CONTACT_IS_NEW;
                }
                else
                {
                    // 1 / (invMassA + invMassB + 1e-10) with one Newton step; 1e-10f is 0x2EDBE6FF
                    const __m128 invMassB = _mm_shuffle_ps(entityB->m_motion.m_inertiaAndMassInv.m_quad, entityB->m_motion.m_inertiaAndMassInv.m_quad, 0xFF);
                    const __m128 invMassA = _mm_shuffle_ps(entityA->m_motion.m_inertiaAndMassInv.m_quad, entityA->m_motion.m_inertiaAndMassInv.m_quad, 0xFF);
                    const __m128 sum = _mm_add_ps(_mm_add_ps(invMassB, invMassA), _mm_set1_ps(1e-10f));
                    const __m128 r0 = _mm_rcp_ps(sum);
                    const __m128 r1 = _mm_mul_ps(_mm_sub_ps(g_vectorfConstants[HK_QUADREAL_2], _mm_mul_ps(r0, sum)), r0);
                    __m128 impulse = _mm_mul_ps(_mm_set1_ps(-0.2f), r1); // 0xBE4CCCCD
                    impulse = _mm_mul_ps(impulse, sepVel);
                    impulse = _mm_mul_ps(impulse, _mm_add_ps(restitution, g_vectorfConstants[HK_QUADREAL_1]));
                    props->m_impulseApplied = _mm_cvtss_f32(impulse);
                    // the velocity the restitution wants back, as a distance per substep: -1.3 * r * v * dt (0xBFA66666)
                    __m128 restitutionData = _mm_mul_ps(restitution, sepVel);
                    restitutionData = _mm_mul_ps(restitutionData, _mm_set1_ps(in->m_subStepDeltaTime));
                    restitutionData = _mm_mul_ps(restitutionData, _mm_set1_ps(-1.3f));
                    props->m_internalSolverData = _mm_cvtss_f32(restitutionData);
                    const __m128 distance = _mm_shuffle_ps(cp->m_separatingNormal.m_quad, cp->m_separatingNormal.m_quad, 0xFF);
                    props->m_internalDataA = _mm_cvtss_f32(_mm_and_ps(_mm_sub_ps(distance, restitutionData), _mm_cmplt_ps(zero, restitution)));
                    props->m_flags &= ~hkContactPointMaterial::CONTACT_IS_NEW;
                }
            }
        }
        else if (callbackRequested)
        {
            hkpContactPointEvent event(hkpCollisionEvent::SOURCE_WORLD, static_cast<hkpRigidBody*>(entityA), static_cast<hkpRigidBody*>(entityB), mgr, type, cp, props, nullptr, nullptr, true, i == lastIndex, i == 0, reinterpret_cast<unsigned int*>(props + 1), accA, accB);
            hkpWorldCallbackUtil::fireContactPointCallback(world, event);
            event.m_source = hkpCollisionEvent::SOURCE_A;
            if (entityA->m_contactListeners.getSize() != 0)
            {
                hkpEntityCallbackUtil::fireContactPointCallbackInternal(entityA, event);
            }
            event.m_source = hkpCollisionEvent::SOURCE_B;
            if (entityB->m_contactListeners.getSize() != 0)
            {
                hkpEntityCallbackUtil::fireContactPointCallbackInternal(entityB, event);
            }
            if (i < lastIndex && prevProps->m_maxImpulse.m_value != 0)
            {
                props->m_flags &= ~hkContactPointMaterial::CONTACT_USES_SOLVER_PATH2;
            }
        }
        cp++;
        prevProps = props;
        props = reinterpret_cast<hkpContactPointProperties*>(reinterpret_cast<char*>(props) + striding);
    }
    instance->m_internal->m_callbackRequest &= ~(hkpConstraintAtom::CALLBACK_REQUEST_NEW_CONTACT_POINT | hkpConstraintAtom::CALLBACK_REQUEST_CONTACT_POINT_CALLBACK);
}
