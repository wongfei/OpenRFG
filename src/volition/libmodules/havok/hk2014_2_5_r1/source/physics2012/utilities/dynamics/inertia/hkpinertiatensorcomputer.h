// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/utilities/dynamics/inertia/hkpinertiatensorcomputer.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/geometryutilities/inertia/hkinertiatensorcomputer.h"

struct hkMassProperties;
class hkSimdFloat32;
class hkVector4f;
class hkcdShape;
class hkpConstraintInstance;
class hkpRigidBody;
class hkpRigidBodyCinfo;
class hkpShape;

// size 0x1
class hkpInertiaTensorComputer : public hkInertiaTensorComputer
{
public:
    static void computeShapeVolumeMassProperties(const hkcdShape* shape, float mass, hkMassProperties& result); // 0x00F1EB00
    static void setShapeVolumeMassProperties(const hkpShape* shape, float mass, hkpRigidBodyCinfo& bodyInfo); // 0x00F1EC50
    static void setMassProperties(const hkMassProperties& massProperties, hkpRigidBodyCinfo& bodyInfo); // 0x00F1D840
    static void setAndScaleToDensity(const hkMassProperties& props, const hkSimdFloat32& density, hkpRigidBodyCinfo& bodyInfo); // 0x00F1DA70
    static void setAndScaleToMass(const hkMassProperties& props, const hkSimdFloat32& mass, hkpRigidBodyCinfo& bodyInfo); // 0x00F1DA00
    static void clipInertia(float maxInertiaRatio, hkpRigidBodyCinfo& bodyInfo); // 0x00F1DAB0
    static void optimizeInertiasOfConstraintTree(hkpConstraintInstance* const * constraints, int numConstraints, hkpRigidBody* rootBody, float inertiaFactorHint); // 0x00F1DE40

};

void scaleVolumeMassProperties(const hkVector4f& scale, hkMassProperties& massProperties); // 0x00F1D880
