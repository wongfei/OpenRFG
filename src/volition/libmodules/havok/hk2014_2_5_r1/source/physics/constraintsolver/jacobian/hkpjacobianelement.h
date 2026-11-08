// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/jacobian/hkpjacobianelement.h
//
// Havok 2014.2.5 (havok_impl.md HK8): the solver's Jacobian rows, from rfg.pdb (the SDK keeps this header private).
// The w lanes carry scalars: hkp1Lin2AngJacobian keeps the right hand side in m_linear0.w, the effective mass in
// m_angular[0].w and a user value in m_angular[1].w; hkp2AngJacobian the effective mass in m_angular[0].w and the
// angular right hand side in m_angular[1].w; hkp2Lin2AngJacobian the right hand side in m_linear[0].w and the effective
// mass in m_angular[0].w. The first byte of a schema's first float holds the schema type (hkpjacobianschema.h).
// Hand-maintained.
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/hkmath.h"

class hkpVelocityAccumulator;
class hkp2AngJacobian;

// size 0x30
class hkp1Lin2AngJacobian
{
public:
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkp1Lin2AngJacobian);
    hkp2AngJacobian& as2AngJacobian_internal() { return *reinterpret_cast<hkp2AngJacobian*>(&m_angular[0]); } // 0x01048570
    void setRHS(const hkSimdFloat32& v) { m_linear0.setW(v); } // 0x00D73D50
    void setRHS(float v) { m_linear0.m_quad.m128_f32[3] = v; } // 0x00D73D40
    const float& getRHS() const { return m_linear0.m_quad.m128_f32[3]; }
    float& getRHS() { return m_linear0.m_quad.m128_f32[3]; } // 0x00EA8F10
    const hkSimdFloat32 getRhsSr() const { return m_linear0.getComponent<3>(); } // 0x00EA8F20
    float getEffMass() const { return m_angular[0].m_quad.m128_f32[3]; } // 0x00D72900
    hkSimdFloat32 getEffMassSr() const { return m_angular[0].getComponent<3>(); } // 0x00EA8F40
    void setEffMass(const hkSimdFloat32& v) { m_angular[0].setW(v); } // 0x00D72910
    void setEffMass(float v) { m_angular[0].m_quad.m128_f32[3] = v; }
    float getUserValue() const { return m_angular[1].m_quad.m128_f32[3]; } // 0x00D72930
    hkSimdFloat32 getUserValueSr() const { return m_angular[1].getComponent<3>(); } // 0x00E98BA0
    void setUserValue(const hkSimdFloat32& v) { m_angular[1].setW(v); } // 0x00D72940
    void setUserValue(float v) { m_angular[1].m_quad.m128_f32[3] = v; } // 0x00D73D70
    hkSimdFloat32 getDiag(const hkpVelocityAccumulator& mA, const hkpVelocityAccumulator& mB) const; // 0x00D72E30
    hkSimdFloat32 getNonDiag(const hkpVelocityAccumulator& mA, const hkpVelocityAccumulator& mB, const hkp1Lin2AngJacobian& jacB) const; // 0x00D72AE0
    hkSimdFloat32 getNonDiagSameObjects(const hkpVelocityAccumulator& mA, const hkpVelocityAccumulator& mB, const hkp1Lin2AngJacobian& jacB) const; // 0x00D72B50
    hkSimdFloat32 getNonDiagDifferentObjects(const hkpVelocityAccumulator& mA, const hkp1Lin2AngJacobian& jacB) const; // 0x00D72BC0
    hkSimdFloat32 getNonDiagDifferentObjects_With2ndBodyFromFirstObject(const hkpVelocityAccumulator& mA, const hkp1Lin2AngJacobian& jacB) const; // 0x01048780

    hkVector4f m_linear0; // 0x00
    hkVector4f m_angular[2]; // 0x10
};

// size 0x20
class hkp2AngJacobian
{
public:
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkp2AngJacobian);
    float getEffMass() const { return m_angular[0].m_quad.m128_f32[3]; } // 0x00E98B90
    hkSimdFloat32 getEffMassSr() const { return m_angular[0].getComponent<3>(); } // 0x00E9B890
    void setEffMass(const hkSimdFloat32& v) { m_angular[0].setW(v); } // 0x00D73D00
    void setEffMass(float v) { m_angular[0].m_quad.m128_f32[3] = v; }
    void setAngularRhs(const hkSimdFloat32& v) { m_angular[1].setW(v); } // 0x00D73D20
    void setAngularRhs(float v) { m_angular[1].m_quad.m128_f32[3] = v; } // 0x00E228C0
    float getAngularRhs() const { return m_angular[1].m_quad.m128_f32[3]; }
    float& getAngularRhs() { return m_angular[1].m_quad.m128_f32[3]; }
    hkSimdFloat32 getAngularRhsSr() const { return m_angular[1].getComponent<3>(); } // 0x00EA8EF0
    const hkSimdFloat32 getRhsSr() const { return m_angular[1].getComponent<3>(); }
    template<typename ACCUM> hkSimdFloat32 getAngularDiag(const ACCUM& mA, const ACCUM& mB) const; // <hkpVelocityAccumulator> 0x00D74040
    hkSimdFloat32 getNonDiagOptimized(const hkpVelocityAccumulator& mA, const hkpVelocityAccumulator& mB, const hkp2AngJacobian& jacB) const; // 0x010486A0
    hkSimdFloat32 getNonDiagSameObjects(const hkpVelocityAccumulator& mA, const hkpVelocityAccumulator& mB, const hkp2AngJacobian& jacB) const; // 0x010486F0
    hkSimdFloat32 getNonDiagDifferentObjects_With2ndBodyFromFirstObject(const hkpVelocityAccumulator& mA, const hkp2AngJacobian& jacB) const; // 0x01048740

    hkVector4f m_angular[2]; // 0x00
};

// size 0x40
class hkp2Lin2AngJacobian
{
public:
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkp2Lin2AngJacobian);
    void setRHS(const hkSimdFloat32& v) { m_linear[0].setW(v); } // 0x00E98BC0
    void setRHS(float v) { m_linear[0].m_quad.m128_f32[3] = v; }
    float getRHS() const { return m_linear[0].m_quad.m128_f32[3]; }
    hkSimdFloat32 getRhsSr() const { return m_linear[0].getComponent<3>(); } // 0x00EA8F60
    float getEffMass() const { return m_angular[0].m_quad.m128_f32[3]; }
    hkSimdFloat32 getEffMassSr() const { return m_angular[0].getComponent<3>(); } // 0x00EA8F80
    void setEffMass(const hkSimdFloat32& v) { m_angular[0].setW(v); } // 0x00E98BE0
    void setEffMass(float v) { m_angular[0].m_quad.m128_f32[3] = v; }
    hkSimdFloat32 getDiag(const hkpVelocityAccumulator& mA, const hkpVelocityAccumulator& mB, const hkSimdFloat32& leverageRatio) const; // 0x00E99370

    hkVector4f m_linear[2]; // 0x00
    hkVector4f m_angular[2]; // 0x20
};

// size 0x60: the two friction directions of a contact (or the two points of a pair contact)
class hkpJacDouble2Bil
{
public:
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpJacDouble2Bil);

    hkp1Lin2AngJacobian m_jac0; // 0x00
    hkp1Lin2AngJacobian m_jac1; // 0x30
};

// size 0x80: two friction directions and the angular friction around the normal
class hkpJacTriple2Bil1Ang
{
public:
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpJacTriple2Bil1Ang);

    hkp1Lin2AngJacobian m_jac0; // 0x00
    hkp1Lin2AngJacobian m_jac1; // 0x30
    hkp2AngJacobian m_jac2; // 0x60
};
