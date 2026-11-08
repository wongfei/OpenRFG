// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraintsolver/accumulator/hkpvelocityaccumulator.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4f.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"

class hkMatrix3f;
class hkQuaternionf;
class hkRotationf;

// size 0x80
class alignas(16) hkpVelocityAccumulator
{
public:
    enum hkpAccumulatorType
    {
        HK_RIGID_BODY = 0,
        HK_KEYFRAMED_RIGID_BODY = 1,
        HK_NO_GRAVITY_RIGID_BODY = 2,
        HK_END = 3,
    };

    enum hkpAccumulatorContext
    {
        ACCUMULATOR_CONTEXT_BUILD_JACOBIANS = 0,
        ACCUMULATOR_CONTEXT_SOLVER = 1,
    };

    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpVelocityAccumulator);
    void setCenterOfMassInWorld(const hkVector4f& worldCenterOfMass); // 0x00E124C0
    const hkVector4f& getCenterOfMassInWorld() const; // 0x00D728F0
    void setCoreFromWorldMatrix(const hkMatrix3f& rot); // 0x00E0D670
    void setCoreFromWorldMatrixToIdentity(); // 0x00B712C0 (.h:100)
    const hkRotationf& getCoreFromWorldMatrix() const; // 0x00B62EF0 (.h:111)
    const hkVector4f& getSumLinearVel() const; // 0x00E12400
    hkVector4f& getSumLinearVel(); // 0x00E0B710
    const hkVector4f& getSumAngularVel() const; // 0x00E12410
    hkVector4f& getSumAngularVel(); // 0x00E0B720
    const hkQuaternionf& getCoreFromWorldQuaternion() const; // 0x00E9BA30
    hkQuaternionf& getCoreFromWorldQuaternion(); // 0x00E0B730
    void convertToSolverType(); // 0x00E0D7F0
    void setFixed(); // 0x00B712F0 (.h:191)
    hkpVelocityAccumulator(); // 0x00B73850

    hkEnum<enum hkpAccumulatorType,unsigned char> m_type; // 0x00
    hkEnum<enum hkpAccumulatorContext,unsigned char> m_context; // 0x01
    unsigned int m_deactivationClass; // 0x04
    float m_gravityFactor; // 0x08
    hkVector4f m_linearVel; // 0x10
    hkVector4f m_angularVel; // 0x20
    hkVector4f m_invMasses; // 0x30
    hkVector4f m_scratch0; // 0x40
    hkVector4f m_scratch1; // 0x50
    hkVector4f m_scratch2; // 0x60
    hkVector4f m_scratch3; // 0x70
};

// 0x00B712F0 (.h:191)
inline void hkpVelocityAccumulator::setFixed()
{
    m_gravityFactor = 0.0f;
    m_type.m_storage = HK_KEYFRAMED_RIGID_BODY;
    m_context.m_storage = ACCUMULATOR_CONTEXT_BUILD_JACOBIANS;
    m_invMasses.setZero();
    m_linearVel.setZero();
    m_angularVel.setZero();
    m_scratch3.setZero();
    m_scratch0 = hkVector4f::getConstant<HK_QUADREAL_1000>();
    m_scratch1 = hkVector4f::getConstant<HK_QUADREAL_0100>();
    m_scratch2 = hkVector4f::getConstant<HK_QUADREAL_0010>();
}
