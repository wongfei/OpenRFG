// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/solver/simpleconstraints/hkpsimpleconstraintutil.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0). hkpSimpleConstraintUtilCollideParams from Havok
// 2014.1 (HK7: the TOI collision response fills it); hkpSimpleConstraintInfo, hkpSimpleConstraintInfoInitInput and
// hkpBodyVelocity from the PDB and Havok 2014.1 (HK8). Hand-maintained.
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/hkmath.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"

struct hkpBodyVelocity;

// size 0x190: cached values for solving a simple 3d constraint (the TOI contact)
struct alignas(16) hkpSimpleConstraintInfo
{
    // size 0x80
    struct alignas(16) BodyInfo
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(BodyInfo);
        float getMass() const { return m_mass; }
        float getEnergy(const hkpBodyVelocity& vel) const; // 0x00EB7550

        hkRotationf m_jacT; // 0x00: the transposed Jacobians (velocities)
        hkMatrix3f m_invIjac; // 0x30: applies forces
        hkVector4f m_invMasses; // 0x60: the inertia diagonal in local space
        float m_mass; // 0x70
        const hkTransformf* m_transform; // 0x74
    };

    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpSimpleConstraintInfo);
    float getMass00() const { return m_invMassMatrix.m_col0.m_quad.m128_f32[3]; }
    float& getMass00() { return m_invMassMatrix.m_col0.m_quad.m128_f32[3]; }
    hkSimdFloat32 getMass00Sr() const { return m_invMassMatrix.m_col0.getComponent<3>(); } // 0x00EB8070
    float getInvDetM12() const { return m_invMassMatrix.m_col1.m_quad.m128_f32[3]; }
    float& getInvDetM12() { return m_invMassMatrix.m_col1.m_quad.m128_f32[3]; }
    hkSimdFloat32 getInvDetM12Sr() const { return m_invMassMatrix.m_col1.getComponent<3>(); } // 0x00EB8090

    BodyInfo m_bodyInfo[2]; // 0x00
    hkRotationf m_base; // 0x100: the first column is the normal, the others are perpendicular
    hkMatrix3f m_invMassMatrix; // 0x130: J^-1 * invI * J
    hkMatrix3f m_massMatrix; // 0x160: m_invMassMatrix^-1
};

// size 0x60
struct alignas(16) hkpSimpleConstraintInfoInitInput
{
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpSimpleConstraintInfoInitInput);

    hkVector4f m_massRelPos; // 0x00
    hkMatrix3f m_invInertia; // 0x10
    hkVector4f m_invMasses; // 0x40: the inertia in local space, w the mass
    const hkTransformf* m_transform; // 0x50: the body's transform (used by the constraint modifiers)
};

// size 0x1C
struct hkpSimpleConstraintUtilCollideParams
{
    float m_friction; // 0x00
    float m_restitution; // 0x04
    float m_maxImpulse; // 0x08
    float m_externalSeperatingVelocity; // 0x0C: the approaching velocity calculated outside
    float m_extraSeparatingVelocity; // 0x10
    float m_extraSlope; // 0x14: virtually rotates the surface towards the velocity
    hkBool m_contactImpulseLimitBreached; // 0x18: output
};

// size 0x20
struct alignas(16) hkpBodyVelocity
{
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpBodyVelocity);

    hkVector4f m_linear; // 0x00: world space
    hkVector4f m_angular; // 0x10: world space
};

void hkSimpleConstraintUtil_InitInfo(const hkpSimpleConstraintInfoInitInput& bodyA, const hkpSimpleConstraintInfoInitInput& bodyB, const hkMatrix3f& directions, hkpSimpleConstraintInfo& out); // 0x00EB7AE0
void hkSimpleConstraintUtil_applyImpulse(const hkpSimpleConstraintInfo& info, const hkVector4f& impulse, hkpBodyVelocity& velA, hkpBodyVelocity& velB); // 0x00EB7300
void hkSimpleConstraintUtil_getPointVelocity(const hkpSimpleConstraintInfo& info, hkpBodyVelocity& velA, hkpBodyVelocity& velB, hkVector4f& out); // 0x00EB7420
float hkSimpleConstraintUtil_Collide(const hkpSimpleConstraintInfo& info, hkpSimpleConstraintUtilCollideParams& params, hkpBodyVelocity& velA, hkpBodyVelocity& velB); // 0x00EB7660
