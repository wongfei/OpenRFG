// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkprigidbodycinfo.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/matrix/hkmatrix3f.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/quaternion/hkquaternionf.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4f.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcollidablequalitytype.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/common/hkpmaterial.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/motion/hkpmotion.h"

class hkLocalFrame;
struct hkMassProperties;
class hkTransformf;
class hkpShape;

// size 0xD0
class hkpRigidBodyCinfo
{
public:
    enum SolverDeactivation
    {
        SOLVER_DEACTIVATION_INVALID = 0,
        SOLVER_DEACTIVATION_OFF = 1,
        SOLVER_DEACTIVATION_LOW = 2,
        SOLVER_DEACTIVATION_MEDIUM = 3,
        SOLVER_DEACTIVATION_HIGH = 4,
        SOLVER_DEACTIVATION_MAX = 5,
    };

    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpRigidBodyCinfo);
    hkpRigidBodyCinfo(); // 0x00DF4F00
    void setMassProperties(const hkMassProperties& mp); // 0x00DF5030
    void setTransform(const hkTransformf& transform); // 0x00DF4EE0

    unsigned int m_collisionFilterInfo; // 0x00
    const hkpShape* m_shape; // 0x04
    hkVector4f m_position; // 0x10
    hkQuaternionf m_rotation; // 0x20
    hkVector4f m_linearVelocity; // 0x30
    hkVector4f m_angularVelocity; // 0x40
    hkMatrix3f m_inertiaTensor; // 0x50
    hkVector4f m_centerOfMass; // 0x80
    float m_mass; // 0x90
    float m_linearDamping; // 0x94
    float m_angularDamping; // 0x98
    float m_gravityFactor; // 0x9C
    float m_friction; // 0xA0
    float m_rollingFrictionMultiplier; // 0xA4
    float m_restitution; // 0xA8
    float m_maxLinearVelocity; // 0xAC
    float m_maxAngularVelocity; // 0xB0
    hkEnum<enum hkpMotion::MotionType,signed char> m_motionType; // 0xB4
    hkBool m_enableDeactivation; // 0xB5
    hkEnum<enum SolverDeactivation,signed char> m_solverDeactivation; // 0xB6
    float m_allowedPenetrationDepth; // 0xB8
    float m_timeFactor; // 0xBC
    hkLocalFrame* m_localFrame; // 0xC0
    hkEnum<enum hkpMaterial::ResponseType,signed char> m_collisionResponse; // 0xC4
    unsigned short m_contactPointCallbackDelay; // 0xC6
    hkEnum<enum hkpCollidableQualityType,signed char> m_qualityType; // 0xC8
    signed char m_autoRemoveLevel; // 0xC9
    unsigned char m_responseModifierFlags; // 0xCA
    signed char m_numShapeKeysInContactPointProperties; // 0xCB
    hkBool m_forceCollideOntoPpu; // 0xCC
};
