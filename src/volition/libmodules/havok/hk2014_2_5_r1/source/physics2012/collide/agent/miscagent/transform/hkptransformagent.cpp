// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/miscagent/transform/hkptransformagent.cpp
//
// Havok 2014.2.5 (havok_impl.md HK6): the transform shape agent2, from rfg.exe. A is the transform shape; its child is
// collided with B through a child agent in the transformed frame (B as transform shape: hkpSymmetricAgent). No SDK
// source exists for 2014.2.5; ported from the binary.
//
// The shipped build records the "TtTransform" monitor timers (TLS hkMonitorStream); they are not reproduced.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/miscagent/transform/hkptransformagent.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/hkmath.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/physics/motionstate/hkmotionstate.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcdbody.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpcollisioninput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpprocesscollisioninput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/query/hkpcdbodypaircollector.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/query/hkpcdpointcollector.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/query/hkplinearcastcollisioninput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/util/symmetric/hkpsymmetricagent.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/hkpcollisiondispatcher.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/misc/transform/hkptransformshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/hkpgskexact.h"

#include <string.h>

using namespace hkGskExact;

static inline const hkpTransformShape* hkTransformAgent_shape(const hkpCdBody& body)
{
    return static_cast<const hkpTransformShape*>(body.getShape());
}

static inline hkcdShapeType::ShapeTypeEnum hkTransformAgent_type(const hkpCdBody& body)
{
    return static_cast<hkcdShapeType::ShapeTypeEnum>(body.getShape()->m_type.m_storage);
}

// hkQuaternion::setMul (inlined): xyz = (q0 x q1 + q1 * w0) + q0 * w1, w = w1 * w0 - q0.q1
static inline Q hkTransformAgent_quatMul(Q q0, Q q1)
{
    const Q w0 = bcast<3>(q0);
    const Q w1 = bcast<3>(q1);
    const Q xyz = add(add(cross(q0, q1), mul(q1, w0)), mul(q0, w1));
    const Q w = sub(mul(w1, w0), dot3(q0, q1));
    return setXYZ_W(xyz, w);
}

// 0x00DC6B40: transform vs any, any vs transform (the symmetric agent)
void hkpTransformAgent::registerAgent(hkpCollisionDispatcher* dispatcher)
{
    hkpCollisionDispatcher::AgentFuncs af;
    af.m_createFunc = createTransformBAgent;
    af.m_getPenetrationsFunc = hkpSymmetricAgentLinearCast<hkpTransformAgent>::staticGetPenetrations;
    af.m_getClosestPointFunc = hkpSymmetricAgentLinearCast<hkpTransformAgent>::staticGetClosestPoints;
    af.m_linearCastFunc = hkpSymmetricAgentLinearCast<hkpTransformAgent>::staticLinearCast;
    af.m_isFlipped = true;
    af.m_isPredictive = true;
    dispatcher->registerCollisionAgent(af, hkcdShapeType::ALL_SHAPE_TYPES, hkcdShapeType::TRANSFORM);

    af.m_createFunc = createTransformAAgent;
    af.m_getPenetrationsFunc = staticGetPenetrations;
    af.m_getClosestPointFunc = staticGetClosestPoints;
    af.m_linearCastFunc = staticLinearCast;
    af.m_isFlipped = false;
    af.m_isPredictive = true;
    dispatcher->registerCollisionAgent(af, hkcdShapeType::TRANSFORM, hkcdShapeType::ALL_SHAPE_TYPES);
}

// 0x00DC6C20 (inlined in createTransformAAgent and createTransformBAgent)
// The child agent, created with the child in a copy of A's motion state whose transform is A's times the shape's (the
// swept transform stays A's).
hkpTransformAgent::hkpTransformAgent(const hkpCdBody& bodyAIn, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* mgr)
    : hkpCollisionAgent(mgr)
{
    const hkpTransformShape* shape = hkTransformAgent_shape(bodyAIn);
    alignas(16) hkMotionState motion;
    memcpy(&motion, bodyAIn.m_motion, sizeof(hkMotionState));
    motion.getTransform().setMul(bodyAIn.getTransform(), shape->m_transform);
    hkpCdBody childBody(&bodyAIn, &motion);
    childBody.setShape(shape->m_childShape.m_childShape, 0);
    m_childAgent = input.m_dispatcher->getNewCollisionAgent(childBody, bodyB, input, mgr);
}

// 0x00DC6BE0
hkpTransformAgent::~hkpTransformAgent()
{
}

// 0x00DC64C0
hkpCollisionAgent* hkpTransformAgent::createTransformAAgent(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* mgr)
{
    return new hkpTransformAgent(bodyA, bodyB, input, mgr);
}

// 0x00DC6A40: the child of B's transform shape against A
hkpCollisionAgent* hkpTransformAgent::createTransformBAgent(const hkpCdBody& bodyA, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpContactMgr* mgr)
{
    return new hkpSymmetricAgent<hkpTransformAgent>(bodyA, bodyB, input, mgr);
}

// 0x00DC6270
void hkpTransformAgent::cleanup(hkpConstraintOwner& constraintOwner)
{
    m_childAgent->cleanup(constraintOwner);
    delete this;
}

// 0x00DC6290
void hkpTransformAgent::invalidateTim(const hkpCollisionInput& input)
{
    m_childAgent->invalidateTim(input);
}

// 0x00DC62A0
void hkpTransformAgent::warpTime(float oldTime, float newTime, const hkpCollisionInput& input)
{
    m_childAgent->warpTime(oldTime, newTime, input);
}

// 0x00DC62D0
void hkpTransformAgent::removePoint(unsigned short idToRemove)
{
    m_childAgent->removePoint(idToRemove);
}

// 0x00DC62E0
void hkpTransformAgent::commitPotential(unsigned short newId)
{
    m_childAgent->commitPotential(newId);
}

// 0x00DC62F0
void hkpTransformAgent::createZombie(unsigned short idTobecomeZombie)
{
    m_childAgent->createZombie(idTobecomeZombie);
}

// 0x00DC65C0
// The child in A's motion state moved by the shape's transform: the transform, both swept rotations (quaternion
// products) and the local centre of mass (into the child's space); the centres of mass, delta angle and radius kept.
void hkpTransformAgent::processCollision(const hkpCdBody& bodyAin, const hkpCdBody& bodyB, const hkpProcessCollisionInput& input, hkpProcessCollisionOutput& result)
{
    const hkpTransformShape* shape = hkTransformAgent_shape(bodyAin);
    const hkMotionState& msA = *static_cast<const hkMotionState*>(bodyAin.m_motion);
    alignas(16) hkMotionState motion;
    motion.getTransform().setMul(bodyAin.getTransform(), shape->m_transform);
    hkSweptTransformf& swept = motion.m_sweptTransform;
    const hkSweptTransformf& sweptA = msA.m_sweptTransform;
    swept.m_centerOfMass0 = sweptA.m_centerOfMass0;
    swept.m_centerOfMass1 = sweptA.m_centerOfMass1;
    swept.m_rotation0.m_vec.m_quad = hkTransformAgent_quatMul(sweptA.m_rotation0.m_vec.m_quad, shape->m_rotation.m_vec.m_quad);
    swept.m_rotation1.m_vec.m_quad = hkTransformAgent_quatMul(sweptA.m_rotation1.m_vec.m_quad, shape->m_rotation.m_vec.m_quad);
    swept.m_centerOfMassLocal.m_quad = rotateInverse(shape->m_transform.getRotation(), sub(sweptA.m_centerOfMassLocal.m_quad, shape->m_transform.getTranslation().m_quad));
    motion.m_deltaAngle = msA.m_deltaAngle;
    motion.m_objectRadius = msA.m_objectRadius;
    hkpCdBody childBody(&bodyAin, &motion);
    childBody.setShape(shape->m_childShape.m_childShape, 0);
    m_childAgent->processCollision(childBody, bodyB, input, result);
}

// 0x00DC6370
void hkpTransformAgent::getClosestPoints(const hkpCdBody& bodyAin, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdPointCollector& pointDetails)
{
    const hkpTransformShape* shape = hkTransformAgent_shape(bodyAin);
    alignas(16) hkTransformf transform;
    transform.setMul(bodyAin.getTransform(), shape->m_transform);
    hkpCdBody childBody(&bodyAin, &transform);
    childBody.setShape(shape->m_childShape.m_childShape, 0);
    m_childAgent->getClosestPoints(childBody, bodyB, input, pointDetails);
}

// 0x00DC63E0
void hkpTransformAgent::getPenetrations(const hkpCdBody& bodyAin, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdBodyPairCollector& collector)
{
    const hkpTransformShape* shape = hkTransformAgent_shape(bodyAin);
    alignas(16) hkTransformf transform;
    transform.setMul(bodyAin.getTransform(), shape->m_transform);
    hkpCdBody childBody(&bodyAin, &transform);
    childBody.setShape(shape->m_childShape.m_childShape, 0);
    m_childAgent->getPenetrations(childBody, bodyB, input, collector);
}

// 0x00DC6300
void hkpTransformAgent::linearCast(const hkpCdBody& bodyAin, const hkpCdBody& bodyB, const hkpLinearCastCollisionInput& input, hkpCdPointCollector& collector, hkpCdPointCollector* startCollector)
{
    const hkpTransformShape* shape = hkTransformAgent_shape(bodyAin);
    alignas(16) hkTransformf transform;
    transform.setMul(bodyAin.getTransform(), shape->m_transform);
    hkpCdBody childBody(&bodyAin, &transform);
    childBody.setShape(shape->m_childShape.m_childShape, 0);
    m_childAgent->linearCast(childBody, bodyB, input, collector, startCollector);
}

// 0x00DC6450
void hkpTransformAgent::updateShapeCollectionFilter(const hkpCdBody& bodyAin, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpConstraintOwner& constraintOwner)
{
    const hkpTransformShape* shape = hkTransformAgent_shape(bodyAin);
    alignas(16) hkTransformf transform;
    transform.setMul(bodyAin.getTransform(), shape->m_transform);
    hkpCdBody childBody(&bodyAin, &transform);
    childBody.setShape(shape->m_childShape.m_childShape, 0);
    m_childAgent->updateShapeCollectionFilter(childBody, bodyB, input, constraintOwner);
}

// 0x00DC6940
void hkpTransformAgent::staticGetClosestPoints(const hkpCdBody& bodyAin, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdPointCollector& pointDetails)
{
    const hkpTransformShape* shape = hkTransformAgent_shape(bodyAin);
    alignas(16) hkTransformf transform;
    transform.setMul(bodyAin.getTransform(), shape->m_transform);
    hkpCdBody childBody(&bodyAin, &transform);
    childBody.setShape(shape->m_childShape.m_childShape, 0);
    const hkpCollisionDispatcher* dispatcher = input.m_dispatcher;
    dispatcher->getGetClosestPointsFunc(hkTransformAgent_type(childBody), hkTransformAgent_type(bodyB))(childBody, bodyB, input, pointDetails);
}

// 0x00DC69C0
void hkpTransformAgent::staticGetPenetrations(const hkpCdBody& bodyAin, const hkpCdBody& bodyB, const hkpCollisionInput& input, hkpCdBodyPairCollector& collector)
{
    const hkpTransformShape* shape = hkTransformAgent_shape(bodyAin);
    alignas(16) hkTransformf transform;
    transform.setMul(bodyAin.getTransform(), shape->m_transform);
    hkpCdBody childBody(&bodyAin, &transform);
    childBody.setShape(shape->m_childShape.m_childShape, 0);
    const hkpCollisionDispatcher* dispatcher = input.m_dispatcher;
    dispatcher->getGetPenetrationsFunc(hkTransformAgent_type(childBody), hkTransformAgent_type(bodyB))(childBody, bodyB, input, collector);
}

// 0x00DC68C0
void hkpTransformAgent::staticLinearCast(const hkpCdBody& bodyAin, const hkpCdBody& bodyB, const hkpLinearCastCollisionInput& input, hkpCdPointCollector& collector, hkpCdPointCollector* startCollector)
{
    const hkpTransformShape* shape = hkTransformAgent_shape(bodyAin);
    alignas(16) hkTransformf transform;
    transform.setMul(bodyAin.getTransform(), shape->m_transform);
    hkpCdBody childBody(&bodyAin, &transform);
    childBody.setShape(shape->m_childShape.m_childShape, 0);
    const hkpCollisionDispatcher* dispatcher = input.m_dispatcher;
    dispatcher->getLinearCastFunc(hkTransformAgent_type(childBody), hkTransformAgent_type(bodyB))(childBody, bodyB, input, collector, startCollector);
}
