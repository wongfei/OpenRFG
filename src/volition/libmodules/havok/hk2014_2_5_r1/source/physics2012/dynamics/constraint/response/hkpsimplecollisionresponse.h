// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/response/hkpsimplecollisionresponse.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"

class hkContactPoint;
class hkpDynamicsContactMgr;
class hkpMotion;
class hkpRigidBody;
struct hkpSimpleConstraintUtilCollideParams;
class hkpSimpleContactConstraintData;
class hkpVelocityAccumulator;

// size 0x1
class hkpSimpleCollisionResponse
{
public:
    // size 0xC
    struct SolveSingleOutput
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(SolveSingleOutput);

        float m_impulse; // 0x00
        float m_velocityKeyframedA; // 0x04
        float m_velocityKeyframedB; // 0x08
    };

    // size 0x4
    struct SolveSingleOutput2
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(SolveSingleOutput2);

        float m_impulse; // 0x00
    };

    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpSimpleCollisionResponse);
    static void solveSingleContact(const hkContactPoint& cp, float time, hkpSimpleConstraintUtilCollideParams& params, hkpMotion* bodyA, hkpMotion* bodyB, hkpDynamicsContactMgr* contactMgr, SolveSingleOutput& output); // 0x00E2EAD0
    static void solveSingleContact2(hkpSimpleContactConstraintData* constraintData, const hkContactPoint& cp, hkpSimpleConstraintUtilCollideParams& params, hkpRigidBody* rigidBodyA, hkpRigidBody* rigidBodyB, hkpVelocityAccumulator* bodyA, hkpVelocityAccumulator* bodyB, SolveSingleOutput2& output); // 0x00E2EE80

};
