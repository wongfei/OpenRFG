// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/collide/hkpresponsemodifier.cpp
//
// Havok 2014.2.5 (havok_impl.md HK7, HK8): the response modifiers of a contact, from rfg.exe. The game uses the mass
// scaling (havok_collision_events.cpp); the other setters have no in-scope caller. The "SetMassChang" monitor timer
// is not reproduced.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/collide/hkpresponsemodifier.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/hkmath.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/router/hkmemoryrouter.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/collide/hkpdynamicscontactmgr.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/atom/hkpmodifierconstraintatom.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/hkpconstraintinstance.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/hkpconstraintowner.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkprigidbody.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/util/hkpworldconstraintutil.h"

// 0x013621D0
const unsigned short hkpResponseModifier::tableOfAdditionalSchemaSizes[] = { 0x00, 0x50, 0xD0, 0x120, 0x50, 0xA0, 0x120, 0x170 };

// the contact's mass changer, created (0x40 bytes from the heap, the inline constructor) and added when missing
static hkpMassChangerModifierConstraintAtom* hkResponseModifier_massChanger(hkpConstraintInstance* instance, hkpConstraintOwner& constraintOwner, bool& created)
{
    hkpModifierConstraintAtom* modifier = hkpWorldConstraintUtil::findModifier(instance, hkpConstraintAtom::TYPE_MODIFIER_MASS_CHANGER);
    created = modifier == nullptr;
    if (modifier == nullptr)
    {
        modifier = static_cast<hkpModifierConstraintAtom*>(hkMemoryRouter::getInstance().heap().blockAlloc(sizeof(hkpMassChangerModifierConstraintAtom)));
        if (modifier != nullptr)
        {
            modifier->m_type = hkpConstraintAtom::TYPE_MODIFIER_MASS_CHANGER;
            modifier->m_modifierAtomSize = sizeof(hkpMassChangerModifierConstraintAtom);
        }
        hkpWorldConstraintUtil::addModifier(instance, constraintOwner, modifier);
    }
    return static_cast<hkpMassChangerModifierConstraintAtom*>(modifier);
}

// 0x00E04470: the factors in the order of the constraint's entities
void hkpResponseModifier::setInvMassScalingForContact(hkpDynamicsContactMgr* manager, hkpRigidBody* bodyA, hkpRigidBody* bodyB, hkpConstraintOwner& constraintOwner, const hkVector4f& factorA, const hkVector4f& factorB)
{
    hkpConstraintInstance* instance = manager->getConstraintInstance();
    if (instance == nullptr)
    {
        return;
    }
    constraintOwner.checkAccessRw();
    bool created;
    hkpMassChangerModifierConstraintAtom* modifier = hkResponseModifier_massChanger(instance, constraintOwner, created);
    if (bodyA == instance->m_entities[0])
    {
        modifier->m_factorA = factorA;
        modifier->m_factorB = factorB;
    }
    else
    {
        modifier->m_factorA = factorB;
        modifier->m_factorB = factorA;
    }
}

// 0x00E04580: one body's factor; a new modifier leaves the other body at 1. An existing modifier whose factors both
// end with a zero (or NaN) w gets w = 1 back in both.
void hkpResponseModifier::setInvMassScalingForContact(hkpDynamicsContactMgr* manager, hkpRigidBody* body, hkpConstraintOwner& constraintOwner, const hkVector4f& factor)
{
    hkpConstraintInstance* instance = manager->getConstraintInstance();
    if (instance == nullptr)
    {
        return;
    }
    constraintOwner.checkAccessRw();
    bool created;
    hkpMassChangerModifierConstraintAtom* modifier = hkResponseModifier_massChanger(instance, constraintOwner, created);
    if (created)
    {
        if (body == instance->m_entities[0])
        {
            modifier->m_factorA = factor;
            modifier->m_factorB.m_quad = g_vectorfConstants[HK_QUADREAL_1];
        }
        else
        {
            modifier->m_factorA.m_quad = g_vectorfConstants[HK_QUADREAL_1];
            modifier->m_factorB = factor;
        }
        return;
    }
    if (body == instance->m_entities[0])
    {
        modifier->m_factorA = factor;
    }
    else
    {
        modifier->m_factorB = factor;
    }
    // UCOMISS/JNZ: equal or unordered
    const float wA = modifier->m_factorA.m_quad.m128_f32[3];
    const float wB = modifier->m_factorB.m_quad.m128_f32[3];
    if (!(wA < 0.0f || wA > 0.0f) && !(wB < 0.0f || wB > 0.0f))
    {
        modifier->m_factorA.m_quad.m128_f32[3] = 1.0f;
        modifier->m_factorB.m_quad.m128_f32[3] = 1.0f;
    }
}
