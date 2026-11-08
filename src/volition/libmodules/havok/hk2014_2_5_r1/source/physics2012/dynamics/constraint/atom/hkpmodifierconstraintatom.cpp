// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/atom/hkpmodifierconstraintatom.cpp
//
// Havok 2014.2.5 (havok_impl.md HK8): the response modifiers of a contact constraint, from rfg.exe: their solver
// sizes and their effect on the simple (TOI) collision response. Only the mass changer is created by the game
// (hkpResponseModifier::setInvMassScalingForContact); the others are ported for the dispatch they share.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/atom/hkpmodifierconstraintatom.h"

#include <xmmintrin.h>

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/hkmath.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/physics/contactpoint/hkcontactpoint.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraint/data/hkpconstraintinfo.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/solver/simpleconstraints/hkpsimpleconstraintutil.h"

// the velocities the soft contact modifier saves before the response (0x03A2A0A0, file static)
static hkpBodyVelocity s_bodyVelocities[2];

// 0x00E30740: the extra schema bytes of each modifier type and the response modifier flag it sets (the ignore
// modifier instead removes the sizes of the constraint's own atoms)
int hkpModifierConstraintAtom::addModifierDataToConstraintInfo(hkpConstraintInfo& cinfo, unsigned char& usedFlagsOut) const
{
    switch (m_type.m_storage)
    {
    case TYPE_MODIFIER_SOFT_CONTACT:
        usedFlagsOut |= 8; // IMPULSE_SCALING
        return 0;
    case TYPE_MODIFIER_MASS_CHANGER:
        cinfo.m_sizeOfSchemas += 0x50; // hkpMassChangerModifierConstraintAtom::ADDITIONAL_SCHEMA_SIZE
        usedFlagsOut |= 1; // MASS_SCALING
        return 0;
    case TYPE_MODIFIER_VISCOUS_SURFACE:
        usedFlagsOut |= 0x10; // VISCOUS_SURFACE
        return 0;
    case TYPE_MODIFIER_MOVING_SURFACE:
        cinfo.m_sizeOfSchemas += 0x50; // hkpMovingSurfaceModifierConstraintAtom::ADDITIONAL_SCHEMA_SIZE
        usedFlagsOut |= 4; // SURFACE_VELOCITY
        return 0;
    case TYPE_MODIFIER_IGNORE_CONSTRAINT:
        return static_cast<const hkpIgnoreModifierConstraintAtom*>(this)->getConstraintInfo(cinfo);
    case TYPE_MODIFIER_CENTER_OF_MASS_CHANGER:
        cinfo.m_sizeOfSchemas += 0xD0; // hkpCenterOfMassChangerModifierConstraintAtom::ADDITIONAL_SCHEMA_SIZE
        usedFlagsOut |= 2; // CENTER_OF_MASS_DISPLACEMENT
        return 0;
    default:
        return 0;
    }
}

// 0x00E307D0: every modifier down to the constraint's atoms; the callback requests are or'ed
int hkpModifierConstraintAtom::addAllModifierDataToConstraintInfo(hkpModifierConstraintAtom* modifier, hkpConstraintInfo& cinfo, unsigned char& usedFlagsOut)
{
    int callbackRequest = 0;
    while (modifier->m_type.m_storage > TYPE_CONTACT)
    {
        callbackRequest |= modifier->addModifierDataToConstraintInfo(cinfo, usedFlagsOut);
        modifier = static_cast<hkpModifierConstraintAtom*>(modifier->m_child);
    }
    return callbackRequest;
}

// 0x00E306D0: the sizes of the constraint's atoms under the modifiers come off the info. A contact (or bridge) atom
// keeps them: no in-scope constraint has other atoms, whose sizes hkpConstraintData::getConstraintInfoUtil
// (0x00D70A40, not ported) would compute.
int hkpIgnoreModifierConstraintAtom::getConstraintInfo(hkpConstraintInfo& info) const
{
    const hkpConstraintAtom* atom = this;
    while (atom->m_type.m_storage > TYPE_CONTACT)
    {
        atom = static_cast<const hkpModifierConstraintAtom*>(atom)->m_child;
    }
    return 0;
}

// 0x00E30810: each body's inverse masses are scaled by its factor; its inverse inertia is scaled uniformly when the
// factor's x, y and z are equal, else in the body's frame (rot * diag(factor) * (rot^-1 * invInertia * rot) * rot^-1,
// with the factor applied to the diagonal only)
void hkpMassChangerModifierConstraintAtom::collisionResponseBeginCallback(const hkContactPoint& cp, hkpSimpleConstraintInfoInitInput& inA, hkpBodyVelocity& velA, hkpSimpleConstraintInfoInitInput& inB, hkpBodyVelocity& velB)
{
    hkpSimpleConstraintInfoInitInput* in = &inA;
    __m128 factor = m_factorA.m_quad;
    for (int i = 2; i != 0; i--)
    {
        in->m_invMasses.m_quad = _mm_mul_ps(in->m_invMasses.m_quad, factor);
        const __m128 fx = _mm_shuffle_ps(factor, factor, 0x00);
        if ((_mm_movemask_ps(_mm_cmpeq_ps(fx, factor)) & 7) == 7)
        {
            hkSimdFloat32 scale;
            scale.m_real = fx;
            in->m_invInertia.mul(scale);
        }
        else
        {
            const hkTransformf* bTc = in->m_transform;
            hkMatrix3f local;
            local.setMulInverseMul(bTc->m_rotation, in->m_invInertia);
            hkMatrix3f m;
            m.setMul(local, bTc->m_rotation);
            // the diagonal (x of column 0, y of column 1, z of column 2) times the factor, put back
            const __m128 maskY = _mm_load_ps((const float*)&hkVector4fComparison_maskToComparison[hkVector4ComparisonMask::MASK_Y * 4]);
            const __m128 maskZ = _mm_load_ps((const float*)&hkVector4fComparison_maskToComparison[hkVector4ComparisonMask::MASK_Z * 4]);
            __m128 diag = _mm_or_ps(_mm_andnot_ps(maskY, m.m_col0.m_quad), _mm_and_ps(maskY, m.m_col1.m_quad));
            diag = _mm_or_ps(_mm_andnot_ps(maskZ, diag), _mm_and_ps(maskZ, m.m_col2.m_quad));
            diag = _mm_mul_ps(diag, factor);
            m.m_col0.m_quad = _mm_move_ss(m.m_col0.m_quad, diag);
            m.m_col1.m_quad = _mm_or_ps(_mm_and_ps(maskY, diag), _mm_andnot_ps(maskY, m.m_col1.m_quad));
            m.m_col2.m_quad = _mm_or_ps(_mm_and_ps(maskZ, diag), _mm_andnot_ps(maskZ, m.m_col2.m_quad));
            local.setMulInverse(m, bTc->m_rotation);
            in->m_invInertia.setMul(bTc->m_rotation, local);
        }
        factor = m_factorB.m_quad;
        in = &inB;
    }
}

// 0x00E303C0
void hkpMassChangerModifierConstraintAtom::collisionResponseEndCallback(const hkContactPoint& cp, float impulseApplied, hkpSimpleConstraintInfoInitInput& inA, hkpBodyVelocity& velA, hkpSimpleConstraintInfoInitInput& inB, hkpBodyVelocity& velB)
{
}

// 0x00E30450: the arms move by the displacements (in world space); the linear velocities lose angular x displacement
void hkpCenterOfMassChangerModifierConstraintAtom::collisionResponseBeginCallback(const hkContactPoint& cp, hkpSimpleConstraintInfoInitInput& inA, hkpBodyVelocity& velA, hkpSimpleConstraintInfoInitInput& inB, hkpBodyVelocity& velB)
{
    hkVector4f dispA;
    dispA._setRotatedDir(inA.m_transform->m_rotation, m_displacementA);
    hkVector4f dispB;
    dispB._setRotatedDir(inB.m_transform->m_rotation, m_displacementB);
    inA.m_massRelPos.sub(dispA);
    inB.m_massRelPos.sub(dispB);
    hkVector4f crossA;
    crossA.setCross(velA.m_angular, dispA);
    hkVector4f crossB;
    crossB.setCross(velB.m_angular, dispB);
    velA.m_linear.sub(crossA);
    velB.m_linear.sub(crossB);
}

// 0x00E30520: the linear velocities get angular x displacement back
void hkpCenterOfMassChangerModifierConstraintAtom::collisionResponseEndCallback(const hkContactPoint& cp, float impulseApplied, hkpSimpleConstraintInfoInitInput& inA, hkpBodyVelocity& velA, hkpSimpleConstraintInfoInitInput& inB, hkpBodyVelocity& velB)
{
    hkVector4f dispA;
    dispA._setRotatedDir(inA.m_transform->m_rotation, m_displacementA);
    hkVector4f dispB;
    dispB._setRotatedDir(inB.m_transform->m_rotation, m_displacementB);
    hkVector4f crossA;
    crossA.setCross(velA.m_angular, dispA);
    hkVector4f crossB;
    crossB.setCross(velB.m_angular, dispB);
    velA.m_linear.add(crossA);
    velB.m_linear.add(crossB);
}

// 0x00E305D0: the velocities before the response are saved (in a file static)
void hkpSoftContactModifierConstraintAtom::collisionResponseBeginCallback(const hkContactPoint& cp, hkpSimpleConstraintInfoInitInput& inA, hkpBodyVelocity& velA, hkpSimpleConstraintInfoInitInput& inB, hkpBodyVelocity& velB)
{
    s_bodyVelocities[0] = velA;
    s_bodyVelocities[1] = velB;
}

// 0x00E303D0: only the fraction m_tau of the response's velocity change is kept
void hkpSoftContactModifierConstraintAtom::collisionResponseEndCallback(const hkContactPoint& cp, float impulseApplied, hkpSimpleConstraintInfoInitInput& inA, hkpBodyVelocity& velA, hkpSimpleConstraintInfoInitInput& inB, hkpBodyVelocity& velB)
{
    const __m128 tau = _mm_set1_ps(m_tau);
    velA.m_linear.m_quad = _mm_add_ps(_mm_mul_ps(_mm_sub_ps(velA.m_linear.m_quad, s_bodyVelocities[0].m_linear.m_quad), tau), s_bodyVelocities[0].m_linear.m_quad);
    velA.m_angular.m_quad = _mm_add_ps(_mm_mul_ps(_mm_sub_ps(velA.m_angular.m_quad, s_bodyVelocities[0].m_angular.m_quad), tau), s_bodyVelocities[0].m_angular.m_quad);
    velB.m_linear.m_quad = _mm_add_ps(_mm_mul_ps(_mm_sub_ps(velB.m_linear.m_quad, s_bodyVelocities[1].m_linear.m_quad), tau), s_bodyVelocities[1].m_linear.m_quad);
    velB.m_angular.m_quad = _mm_add_ps(_mm_mul_ps(_mm_sub_ps(velB.m_angular.m_quad, s_bodyVelocities[1].m_angular.m_quad), tau), s_bodyVelocities[1].m_angular.m_quad);
}

// the surface velocity without its normal part
static inline __m128 hkMovingSurface_tangentVelocity(const hkVector4f& velocity, const hkContactPoint& cp)
{
    const hkSimdFloat32 normalPart = cp.m_separatingNormal.dot<3>(velocity);
    return _mm_sub_ps(velocity.m_quad, _mm_mul_ps(normalPart.m_real, velocity.m_quad));
}

// 0x00E30610: body B moves with the surface
void hkpMovingSurfaceModifierConstraintAtom::collisionResponseBeginCallback(const hkContactPoint& cp, hkpSimpleConstraintInfoInitInput& inA, hkpBodyVelocity& velA, hkpSimpleConstraintInfoInitInput& inB, hkpBodyVelocity& velB)
{
    const hkVector4f velocity = m_velocity;
    velB.m_linear.m_quad = _mm_add_ps(hkMovingSurface_tangentVelocity(velocity, cp), velB.m_linear.m_quad);
}

// 0x00E30670
void hkpMovingSurfaceModifierConstraintAtom::collisionResponseEndCallback(const hkContactPoint& cp, float impulseApplied, hkpSimpleConstraintInfoInitInput& inA, hkpBodyVelocity& velA, hkpSimpleConstraintInfoInitInput& inB, hkpBodyVelocity& velB)
{
    const hkVector4f velocity = m_velocity;
    velB.m_linear.m_quad = _mm_sub_ps(velB.m_linear.m_quad, hkMovingSurface_tangentVelocity(velocity, cp));
}

// Inlined in the one below.
hkpModifierConstraintAtom::hkpModifierConstraintAtom(hkpConstraintAtom::AtomType type, int size)
    : hkpConstraintAtom(type)
{
    m_modifierAtomSize = (unsigned short)size;
}

// 0x00A2D3D0 (.h:55)
hkpMassChangerModifierConstraintAtom::hkpMassChangerModifierConstraintAtom()
    : hkpModifierConstraintAtom(TYPE_MODIFIER_MASS_CHANGER, sizeof(hkpMassChangerModifierConstraintAtom))
{
}
