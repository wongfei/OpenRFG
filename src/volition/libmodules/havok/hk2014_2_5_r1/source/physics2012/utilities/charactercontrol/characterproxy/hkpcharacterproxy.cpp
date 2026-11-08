// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/utilities/charactercontrol/characterproxy/hkpcharacterproxy.cpp
//
// Havok 2014.2.5 (player_minimal.md P1): the character proxy, from rfg.exe. A shape phantom that moves by linear
// casts; the contact manifold of the last step becomes the planes of the simplex solver. Ported are the members the
// game reaches (RFG integrates through hk_custom_character_proxy, so integrate/integrateImplementation, the trigger
// volume functions, getCinfo and checkSupportDeprecated are not ported).
//
// x87 code in rfg.exe: extractSurfaceConstraintInfo moves the plane by its velocity in double precision (the
// default x87 precision control of the game); that step is emulated in double.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/utilities/charactercontrol/characterproxy/hkpcharacterproxy.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/localarray/hklocalarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/matrix/hkmatrix3f.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hksimdfloat32.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/physics/hkstepinfo.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/internal/simplexsolver/hksimplexsolver.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcollidable.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpcollisioninput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpprocesscollisioninput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/query/castutil/hkplinearcastinput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/query/collector/pointcollector/hkpallcdpointcollector.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkprigidbody.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/motion/hkpmotion.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/phantom/hkpphantom.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/phantom/hkpshapephantom.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/hkpworld.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/utilities/charactercontrol/characterproxy/hkpcharacterproxycinfo.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/utilities/charactercontrol/characterproxy/hkpcharacterproxylistener.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/utilities/charactercontrol/hkpcharactercontrol.h"

// hkpWorldObject property keys (hkpPropertyKeys): the proxy that owns a phantom, the trigger volume of a body
static const unsigned int HK_PROPERTY_CHARACTER_PROXY = 0x1300;
static const unsigned int HK_PROPERTY_TRIGGER_VOLUME = 0x1310;

// hkpGetPhantom (hkpphantom.inl): the owner of a phantom's collidable
static inline hkpPhantom* hkpCharacterProxy_getPhantom(const hkpCollidable* collidable)
{
    if (collidable->m_broadPhaseHandle.m_type == hkpWorldObject::BROAD_PHASE_PHANTOM)
    {
        return (hkpPhantom*)collidable->getOwner();
    }
    return nullptr;
}

static inline bool hkpCharacterProxy_isFixedOrKeyframed(const hkpRigidBody* body)
{
    const unsigned char type = body->getRigidMotion()->m_type.m_storage;
    return type == hkpMotion::MOTION_FIXED || type == hkpMotion::MOTION_KEYFRAMED;
}

// ---- hkpCharacterProxyCinfo / hkpCharacterControllerCinfo / hkpCharacterProxyListener (inline in the SDK headers)

// 0x0080CDC0 (hkpcharactercontrollercinfo.h:23)
hkpCharacterControllerCinfo::hkpCharacterControllerCinfo()
{
}

// 0x0080CDD0
hkpCharacterControllerCinfo::~hkpCharacterControllerCinfo()
{
}

// 0x0080CDE0 (hkpcharacterproxycinfo.h:28)
hkpCharacterProxyCinfo::hkpCharacterProxyCinfo()
{
    m_position.setZero();
    m_velocity.setZero();
    m_dynamicFriction = 1.0f;
    m_staticFriction = 0.0f;
    m_keepContactTolerance = 0.1f;
    m_up.setConstant<HK_QUADREAL_0100>();
    m_extraUpStaticFriction = 0.0f;
    m_extraDownStaticFriction = 0.0f;
    m_shapePhantom = nullptr;
    m_keepDistance = 0.05f;
    m_contactAngleSensitivity = 10.0f;
    m_userPlanes = 4;
    m_maxCharacterSpeedForSolver = 10.0f;
    m_characterStrength = HK_REAL_MAX; // 0x7F7FFFEE
    m_characterMass = 0.0f;
    m_maxSlope = 1.57079637f; // HK_REAL_PI / 2, 0x3FC90FDB
    m_penetrationRecoverySpeed = 1.0f;
    m_maxCastIterations = 10;
    m_refreshManifoldInCheckSupport.m_bool = 0;
}

// 0x0080CE90
hkpCharacterProxyCinfo::~hkpCharacterProxyCinfo()
{
}

// 0x0080D630
hkpCharacterProxyListener::hkpCharacterProxyListener()
{
}

// 0x0080CEA0
hkpCharacterProxyListener::~hkpCharacterProxyListener()
{
}

// 0x0080CEB0
void hkpCharacterProxyListener::processConstraintsCallback(const hkpCharacterProxy* proxy, const hkArray<hkpRootCdPoint, hkContainerHeapAllocator>& manifold, hkSimplexSolverInput& input)
{
}

// 0x0080CEC0
void hkpCharacterProxyListener::contactPointAddedCallback(const hkpCharacterProxy* proxy, const hkpRootCdPoint& point)
{
}

// 0x0080CED0
void hkpCharacterProxyListener::contactPointRemovedCallback(const hkpCharacterProxy* proxy, const hkpRootCdPoint& point)
{
}

// 0x0080CEE0
void hkpCharacterProxyListener::characterInteractionCallback(hkpCharacterProxy* proxy, hkpCharacterProxy* otherProxy, const hkContactPoint& contact)
{
}

// 0x0080CEF0
void hkpCharacterProxyListener::objectInteractionCallback(hkpCharacterProxy* proxy, const hkpCharacterObjectInteractionEvent& input, hkpCharacterObjectInteractionResult& output)
{
}

// ---- hkpCharacterProxy

// 0x00F1A0C0
hkpCharacterProxy::hkpCharacterProxy(const hkpCharacterProxyCinfo& info)
{
    m_shapePhantom = nullptr;
    updateFromCinfo(info);
}

// 0x00F19D20: the listeners come off the bodies, phantoms and trigger bodies; the phantom loses its proxy property
// and the proxy's reference
hkpCharacterProxy::~hkpCharacterProxy()
{
    for (int i = 0; i < m_bodies.getSize(); i++)
    {
        m_bodies[i]->removeEntityListener(this);
    }
    m_bodies.clear();
    for (int i = 0; i < m_phantoms.getSize(); i++)
    {
        m_phantoms[i]->removePhantomListener(this);
    }
    m_phantoms.clear();
    for (int i = 0; i < m_overlappingTriggerVolumes.getSize(); i++)
    {
        // hkpTriggerVolume::m_triggerBody (+0x10); trigger volumes are not used by RFG
        (*(hkpRigidBody**)((char*)m_overlappingTriggerVolumes[i] + 0x10))->removeEntityListener(this);
    }
    m_shapePhantom->removeProperty(HK_PROPERTY_CHARACTER_PROXY);
    m_shapePhantom->removeReference();
}

// 0x00F19A30: the phantom is referenced (the old one released), the slope becomes its cosine, and the manifold and
// the listened bodies and phantoms are cleared
void hkpCharacterProxy::updateFromCinfo(const hkpCharacterProxyCinfo& info)
{
    info.m_shapePhantom->addReference();
    if (m_shapePhantom != nullptr)
    {
        m_shapePhantom->removeReference();
    }
    m_velocity = info.m_velocity;
    m_dynamicFriction = info.m_dynamicFriction;
    m_staticFriction = info.m_staticFriction;
    m_keepContactTolerance = info.m_keepContactTolerance;
    m_extraUpStaticFriction = info.m_extraUpStaticFriction;
    m_extraDownStaticFriction = info.m_extraDownStaticFriction;
    m_keepDistance = info.m_keepDistance;
    m_shapePhantom = info.m_shapePhantom;
    m_contactAngleSensitivity = info.m_contactAngleSensitivity;
    m_userPlanes = info.m_userPlanes;
    m_maxCharacterSpeedForSolver = info.m_maxCharacterSpeedForSolver;
    m_characterStrength = info.m_characterStrength;
    m_characterMass = info.m_characterMass;
    m_maxSlopeCosine = hkMath::cos(info.m_maxSlope);
    m_penetrationRecoverySpeed = info.m_penetrationRecoverySpeed;
    m_maxCastIterations = info.m_maxCastIterations;
    m_refreshManifoldInCheckSupport = info.m_refreshManifoldInCheckSupport.m_bool != 0;
    m_up = info.m_up;
    m_up.normalize<3, HK_ACC_23_BIT, HK_SQRT_SET_ZERO>();
    if (m_shapePhantom->m_world != nullptr)
    {
        setPosition(info.m_position);
    }
    m_oldDisplacement.setZero();
    for (int i = 0; i < m_bodies.getSize(); i++)
    {
        m_bodies[i]->removeEntityListener(this);
    }
    m_bodies.clear();
    for (int i = 0; i < m_phantoms.getSize(); i++)
    {
        m_phantoms[i]->removePhantomListener(this);
    }
    m_phantoms.clear();
    m_manifold.clear();
    if (!m_shapePhantom->hasProperty(HK_PROPERTY_CHARACTER_PROXY, hkpWorldObject::MULTI_THREADING_CHECKS_ENABLE))
    {
        hkSimplePropertyValue value;
        value.m_data = (hkUlong)this;
        m_shapePhantom->addProperty(HK_PROPERTY_CHARACTER_PROXY, value);
    }
}

// 0x00F18810
const hkArray<hkpRootCdPoint, hkContainerHeapAllocator>& hkpCharacterProxy::getManifold() const
{
    return m_manifold;
}

// 0x00F18820: the phantom's translation
const hkVector4f& hkpCharacterProxy::getPosition() const
{
    return m_shapePhantom->getCollidable()->getTransform().getTranslation();
}

// 0x00F18830: the broadphase box grows by the contact tolerance and the keep distance
void hkpCharacterProxy::setPosition(const hkVector4f& position)
{
    m_shapePhantom->setPosition(position, m_keepContactTolerance + m_keepDistance);
}

// 0x00F18860
const hkVector4f& hkpCharacterProxy::getLinearVelocity() const
{
    return m_velocity;
}

// 0x00F18870
void hkpCharacterProxy::setLinearVelocity(const hkVector4f& vel)
{
    m_velocity = vel;
}

// 0x00F18890
const hkpShapePhantom* hkpCharacterProxy::getShapePhantom() const
{
    return m_shapePhantom;
}

// 0x00F18880
hkpShapePhantom* hkpCharacterProxy::getShapePhantom()
{
    return m_shapePhantom;
}

// 0x00F19580
void hkpCharacterProxy::addCharacterProxyListener(hkpCharacterProxyListener* cpl)
{
    m_listeners.pushBack(cpl);
}

// 0x00F18F30: the original does not check the index (a missing listener removes slot -1)
void hkpCharacterProxy::removeCharacterProxyListener(hkpCharacterProxyListener* cpl)
{
    const int i = m_listeners.indexOf(cpl);
    m_listeners.m_size--;
    if (m_listeners.m_size != i)
    {
        m_listeners.m_data[i] = m_listeners.m_data[m_listeners.m_size];
    }
}

// 0x00F18B00
void hkpCharacterProxy::fireContactAdded(const hkpRootCdPoint& point) const
{
    for (int i = m_listeners.getSize() - 1; i >= 0; i--)
    {
        m_listeners[i]->contactPointAddedCallback(this, point);
    }
}

// 0x00F18B30
void hkpCharacterProxy::fireContactRemoved(const hkpRootCdPoint& point) const
{
    for (int i = m_listeners.getSize() - 1; i >= 0; i--)
    {
        m_listeners[i]->contactPointRemovedCallback(this, point);
    }
}

// 0x00F18AC0: the listeners get the proxy's own manifold, whatever manifold is passed (kept as the original)
void hkpCharacterProxy::fireConstraintsProcessed(const hkArray<hkpRootCdPoint, hkContainerHeapAllocator>& manifold, hkSimplexSolverInput& input) const
{
    for (int i = m_listeners.getSize() - 1; i >= 0; i--)
    {
        m_listeners[i]->processConstraintsCallback(this, m_manifold, input);
    }
}

// 0x00F18A80
void hkpCharacterProxy::fireObjectInteraction(const hkpCharacterObjectInteractionEvent& input, hkpCharacterObjectInteractionResult& output)
{
    for (int i = m_listeners.getSize() - 1; i >= 0; i--)
    {
        m_listeners[i]->objectInteractionCallback(this, input, output);
    }
}

// 0x00F18A40
void hkpCharacterProxy::fireCharacterInteraction(hkpCharacterProxy* otherProxy, const hkContactPoint& contact)
{
    for (int i = m_listeners.getSize() - 1; i >= 0; i--)
    {
        m_listeners[i]->characterInteractionCallback(this, otherProxy, contact);
    }
}

// 0x00F18B60: the contact plane pushed out by the keep distance. A moving body gives the plane a velocity (a fixed
// or keyframed one its point velocity, a dynamic one the velocity of its last step) and moves the plane back by the
// time already travelled; penetrations become a separating velocity.
void hkpCharacterProxy::extractSurfaceConstraintInfo(const hkpRootCdPoint& hit, hkSurfaceConstraintInfo& surface, float timeTravelled) const
{
    surface.m_plane = hit.m_contact.m_separatingNormal;
    surface.m_plane.m_quad.m128_f32[3] = surface.m_plane.m_quad.m128_f32[3] - m_keepDistance;
    surface.m_staticFriction = m_staticFriction;
    surface.m_dynamicFriction = m_dynamicFriction;
    surface.m_extraUpStaticFriction = m_extraUpStaticFriction;
    surface.m_extraDownStaticFriction = m_extraDownStaticFriction;
    surface.m_velocity.setZero();
    surface.m_priority = 0;

    const hkpRigidBody* body = hkpGetRigidBody(hit.m_rootCollidableB);
    if (body != nullptr)
    {
        const hkpMotion* motion = body->getRigidMotion();
        const unsigned char motionType = motion->m_type.m_storage;
        if (motionType == hkpMotion::MOTION_FIXED || motionType == hkpMotion::MOTION_KEYFRAMED)
        {
            body->getPointVelocity(hit.m_contact.m_position, surface.m_velocity);
        }
        else
        {
            // the velocity of the body's last step (from its swept transform)
            const hkSweptTransformf& swept = motion->m_motionState.getSweptTransform();
            const hkSimdFloat32 invDeltaTime = swept.m_centerOfMass1.getComponent<3>();
            hkVector4f angularVelocity;
            angularVelocity.setMul(motion->m_motionState.m_deltaAngle, invDeltaTime);
            hkVector4f linearVelocity;
            linearVelocity.setSub(swept.m_centerOfMass1, swept.m_centerOfMass0);
            linearVelocity.mul(invDeltaTime);
            hkVector4f relPos;
            relPos.setSub(hit.m_contact.m_position, swept.m_centerOfMass1);
            surface.m_velocity.setCross(angularVelocity, relPos);
            surface.m_velocity.setAdd(linearVelocity, surface.m_velocity);
        }
        // x87: plane.w - dot * timeTravelled in double precision
        const float planeVelocity = surface.m_plane.dot<3>(surface.m_velocity).getReal();
        surface.m_plane.m_quad.m128_f32[3] = (float)((double)surface.m_plane.m_quad.m128_f32[3] - (double)planeVelocity * (double)timeTravelled);
        if (motionType == hkpMotion::MOTION_FIXED)
        {
            surface.m_priority = 2;
        }
        if (motionType == hkpMotion::MOTION_KEYFRAMED)
        {
            surface.m_priority = 1;
        }
    }

    // ucomiss/jnc: NaN distances recover too
    const hkSimdFloat32 distance = surface.m_plane.getComponent<3>();
    const hkSimdFloat32 zero = hkSimdFloat32::getConstant<HK_QUADREAL_0>();
    if (!(distance.getReal() >= (zero - hkSimdFloat32::getConstant<HK_QUADREAL_EPS>()).getReal()))
    {
        surface.m_velocity.addMul(hit.m_contact.m_separatingNormal, hkSimdFloat32::fromFloat(m_penetrationRecoverySpeed) * (zero - distance));
        surface.m_plane.zeroComponent<3>();
    }
}

// 0x00F1C850: the point velocity of the body behind the contact (zero for anything else)
void hkpCharacterProxy::extractSurfaceVelocity(const hkpRootCdPoint& hit, hkVector4f& velocityOut) const
{
    velocityOut.setZero();
    const hkpRigidBody* body = hkpGetRigidBody(hit.m_rootCollidableB);
    if (body != nullptr)
    {
        body->getPointVelocity(hit.m_contact.m_position, velocityOut);
    }
}

// 0x00F1D090 (hkpcharacterproxy.inl): a similarity metric of two contact points (normal angle, plane distance,
// surface velocity)
float hkpCharacterProxy::surfaceDistance(const hkpRootCdPoint& p1, const hkpRootCdPoint& p2) const
{
    // small angle approximation
    hkSimdFloat32 angleSquared = hkSimdFloat32::getConstant<HK_QUADREAL_1>() - p1.m_contact.m_separatingNormal.dot<3>(p2.m_contact.m_separatingNormal);
    angleSquared = angleSquared * hkSimdFloat32::fromFloat(m_contactAngleSensitivity * m_contactAngleSensitivity);

    hkSimdFloat32 planeDistanceSqrd = p1.m_contact.m_separatingNormal.getComponent<3>() - p2.m_contact.m_separatingNormal.getComponent<3>();
    planeDistanceSqrd = planeDistanceSqrd * planeDistanceSqrd;

    hkVector4f p1Vel;
    extractSurfaceVelocity(p1, p1Vel);
    hkVector4f p2Vel;
    extractSurfaceVelocity(p2, p2Vel);
    hkVector4f velDiff;
    velDiff.setSub(p1Vel, p2Vel);
    const hkSimdFloat32 velocityDiffSqrd = velDiff.lengthSquared<3>();

    return (angleSquared * hkSimdFloat32::fromFloat(10.0f) + velocityDiffSqrd * hkSimdFloat32::fromFloat(0.1f) + planeDistanceSqrd).getReal();
}

// 0x00F18F80: the closest manifold point (metric below 0.1), or -1
int hkpCharacterProxy::findSurface(const hkpRootCdPoint& info, hkArray<hkpRootCdPoint, hkContainerHeapAllocator>& manifold) const
{
    int bestIndex = -1;
    float minDistance = 0.1f;
    for (int i = 0; i < manifold.getSize(); i++)
    {
        const float distance = surfaceDistance(info, manifold[i]);
        if (distance < minDistance)
        {
            minDistance = distance;
            bestIndex = i;
        }
    }
    return bestIndex;
}

// 0x00F1CC30 (hkpcharacterproxy.inl): a linear cast hit's distance is the cast fraction; it moves into the position's
// w and the distance becomes the fraction times the displacement into the plane
void hkpCharacterProxy::convertFractionToDistance(const hkpRootCdPoint* cHit, int numHits, const hkVector4f& displacement) const
{
    hkpRootCdPoint* hit = const_cast<hkpRootCdPoint*>(cHit);
    const hkSimdFloat32 zero = hkSimdFloat32::getConstant<HK_QUADREAL_0>();
    for (int i = numHits - 1; i >= 0; i--)
    {
        const hkSimdFloat32 fraction = hit->m_contact.m_separatingNormal.getComponent<3>();
        hkVector4f cpPos = hit->m_contact.m_position;
        cpPos.setComponent<3>(fraction);
        hit->m_contact.m_position = cpPos;
        const hkSimdFloat32 projLength = displacement.dot<3>(hit->m_contact.m_separatingNormal);
        hit->m_contact.m_separatingNormal.setComponent<3>((zero - projLength) * fraction);
        hit++;
    }
}

// 0x00F1CCD0 (hkpcharacterproxy.inl)
void hkpCharacterProxy::convertFractionToDistance(const hkpAllCdPointCollector& collector, const hkVector4f& displacement) const
{
    convertFractionToDistance(collector.getHits().begin(), collector.getHits().getSize(), displacement);
}

// 0x00F1D420 (hkpcharacterproxy.inl): the body or phantom behind a contact is listened to (once)
void hkpCharacterProxy::addEntityOrPhantom(const hkpCollidable* collidable, hkArray<hkpRigidBody*, hkContainerHeapAllocator>& bodies, hkArray<hkpPhantom*, hkContainerHeapAllocator>& phantoms)
{
    hkpRigidBody* body = hkpGetRigidBody(collidable);
    hkpPhantom* phantom = hkpCharacterProxy_getPhantom(collidable);
    if (body != nullptr && bodies.indexOf(body) == -1)
    {
        bodies.pushBack(body);
        body->addEntityListener(this);
    }
    if (phantom != nullptr && phantoms.indexOf(phantom) == -1)
    {
        phantoms.pushBack(phantom);
        phantom->addPhantomListener(this);
    }
}

// 0x00F197D0: walk only as far as the cast allows, keeping m_keepDistance to the surface along the movement (an
// embedded object stops the character but does not push it back). Returns the time used: the x87 product in ST0,
// exact in double, which the callers subtract before rounding (hence double, not the PDB's float).
double hkpCharacterProxy::moveToLinearCastHitPosition(const hkSimplexSolverOutput& output, const hkpRootCdPoint& hit, const hkpLinearCastInput& castInput, hkVector4f& position)
{
    const hkSimdFloat32 displacementLength = output.m_position.length<3, HK_ACC_23_BIT, HK_SQRT_SET_ZERO>();
    hkSimdFloat32 angleBetweenMovementAndSurface;
    angleBetweenMovementAndSurface.setDiv<HK_ACC_23_BIT, HK_DIV_IGNORE>(hit.m_contact.m_separatingNormal.dot<3>(output.m_position), displacementLength);
    hkSimdFloat32 keepDistanceAlongMovement;
    keepDistanceAlongMovement.setDiv<HK_ACC_23_BIT, HK_DIV_IGNORE>(hkSimdFloat32::fromFloat(m_keepDistance), hkSimdFloat32::getConstant<HK_QUADREAL_0>() - angleBetweenMovementAndSurface);
    hkSimdFloat32 keepFraction;
    keepFraction.setDiv<HK_ACC_23_BIT, HK_DIV_IGNORE>(keepDistanceAlongMovement, displacementLength);
    // the cast fraction is in the position's w (convertFractionToDistance)
    hkSimdFloat32 fraction = hit.m_contact.m_position.getComponent<3>() - keepFraction;
    // max(0, f) then min(1, f), as the original (NaN stays NaN)
    fraction.m_real = _mm_min_ps(g_vectorfConstants[HK_QUADREAL_1], _mm_max_ps(g_vectorfConstants[HK_QUADREAL_0], fraction.m_real));
    hkVector4f displacement;
    displacement.setSub(castInput.m_to, position);
    position.addMul(displacement, fraction);
    return (double)fraction.getReal() * (double)output.m_deltaTime;
}

// 0x00F198F0: a surface steeper than the maximum slope (but not a wall) gets an extra vertical plane at the end of
// the constraints. LTCG: index in EAX, surfaceConstraintsInOut in EDI.
static bool addMaxSlopePlane(float maxSlopeCos, const hkVector4f& up, int index, hkArray<hkSurfaceConstraintInfo, hkContainerHeapAllocator>& surfaceConstraintsInOut)
{
    const float surfaceVerticalComponent = surfaceConstraintsInOut[index].m_plane.dot<3>(up).getReal();
    if (surfaceVerticalComponent > 0.01f && maxSlopeCos > surfaceVerticalComponent)
    {
        hkSurfaceConstraintInfo& newConstraint = surfaceConstraintsInOut.expandOne();
        newConstraint = surfaceConstraintsInOut[index];
        newConstraint.m_plane.addMul(hkSimdFloat32::getConstant<HK_QUADREAL_0>() - hkSimdFloat32::fromFloat(surfaceVerticalComponent), up);
        newConstraint.m_plane.normalize<3, HK_ACC_23_BIT, HK_SQRT_SET_ZERO>();
        return true;
    }
    return false;
}

// 0x00F1A140: the start points at the current position (closest points within the keep distance and contact
// tolerance) replace the manifold
void hkpCharacterProxy::refreshManifold(hkpAllCdPointCollector& startPointCollector)
{
    startPointCollector.reset();
    hkpCollisionInput input = *m_shapePhantom->m_world->m_collisionInput;
    input.m_tolerance = m_keepContactTolerance + m_keepDistance + input.m_tolerance.val();
    m_shapePhantom->getClosestPoints(startPointCollector, &input);

    hkpAllCdPointCollector castCollector;
    hkBool isMultithreaded;
    isMultithreaded.m_bool = 0;
    updateManifold(startPointCollector, castCollector, m_manifold, m_bodies, m_phantoms, isMultithreaded);
}

// 0x00F1A230: solve a short step in the direction against the static manifold (plane velocities removed). No
// movement means unsupported, a stop means supported, and a slide off a slope steeper than the maximum is sliding.
// The support normal and velocity average the touched planes that face the direction.
void hkpCharacterProxy::checkSupportWithCollector(const hkVector4f& direction, hkpSurfaceInfo& ground, hkpAllCdPointCollector& startPointCollector)
{
    // "TtcheckSupport" monitor timer: not reproduced (see hkbasesystem.cpp)
    if (m_refreshManifoldInCheckSupport)
    {
        refreshManifold(startPointCollector);
    }

    hkLocalArray<hkSurfaceConstraintInfo> constraints(m_manifold.getSize() + m_userPlanes + 10);
    constraints.setSizeUnchecked(m_manifold.getSize());
    for (int i = 0; i < m_manifold.getSize(); i++)
    {
        extractSurfaceConstraintInfo(m_manifold[i], constraints[i], 0.0f);
        addMaxSlopePlane(m_maxSlopeCosine, m_up, i, constraints);
    }
    constraints.reserve(constraints.getSize() + m_userPlanes);

    hkLocalArray<hkSurfaceConstraintInteraction> interactions(constraints.getSize() + m_userPlanes);
    hkLocalArray<hkVector4f> storedVelocities(constraints.getSize() + m_userPlanes);

    hkSimplexSolverInput input;
    hkSimplexSolverOutput output;
    {
        input.m_position.setZero();
        input.m_constraints = constraints.begin();
        input.m_numConstraints = constraints.getSize();
        input.m_velocity = direction;
        input.m_deltaTime = 1.0f / 60.0f; // 0x3C888889
        input.m_minDeltaTime = 1.0f / 60.0f;
        input.m_upVector = m_up;
        input.m_maxSurfaceVelocity.setAll(m_maxCharacterSpeedForSolver);
        output.m_planeInteractions = interactions.begin();

        // the listeners may add planes (into the reserved space)
        fireConstraintsProcessed(m_manifold, input);

        storedVelocities.setSize(input.m_numConstraints);
        interactions.setSize(input.m_numConstraints);
        constraints.setSize(input.m_numConstraints);

        // a query of the static geometry
        for (int i = 0; i < input.m_numConstraints; i++)
        {
            storedVelocities[i] = constraints[i].m_velocity;
            constraints[i].m_velocity.setZero();
        }
        hkSimplexSolverSolve(input, output);
    }

    ground.m_surfaceVelocity.setZero();
    ground.m_surfaceNormal.setZero();

    hkVector4f velocityDiff;
    velocityDiff.setSub(output.m_velocity, direction);
    velocityDiff.setAbs(velocityDiff);
    const __m128 unchanged = _mm_cmple_ps(velocityDiff.m_quad, _mm_set1_ps(0.001f)); // 0x3A83126F
    if ((_mm_movemask_ps(unchanged) & 7) != 7)
    {
        if (output.m_velocity.lengthSquared<3>().getReal() < 0.001f)
        {
            ground.m_supportedState = hkpSurfaceInfo::SUPPORTED;
        }
        else
        {
            output.m_velocity.normalize<3, HK_ACC_23_BIT, HK_SQRT_SET_ZERO>();
            const hkSimdFloat32 angleSin = output.m_velocity.dot<3>(direction);
            const hkSimdFloat32 cosSqr = hkSimdFloat32::getConstant<HK_QUADREAL_1>() - angleSin * angleSin;
            // ucomiss/setnc: NaN slides
            ground.m_supportedState = (cosSqr.getReal() >= m_maxSlopeCosine * m_maxSlopeCosine) ? hkpSurfaceInfo::SUPPORTED : hkpSurfaceInfo::SLIDING;
        }

        int numTouching = 0;
        for (int i = 0; i < input.m_numConstraints; i++)
        {
            // touched planes that face the direction
            if (interactions[i].m_touched && constraints[i].m_plane.dot<3>(direction).getReal() < -0.08f) // 0xBDA3D70A
            {
                ground.m_surfaceNormal.add(constraints[i].m_plane);
                ground.m_surfaceVelocity.add(storedVelocities[i]);
                numTouching++;
            }
        }
        if (numTouching > 0)
        {
            ground.m_surfaceNormal.normalize<3, HK_ACC_23_BIT, HK_SQRT_SET_ZERO>();
            hkSimdFloat32 invNumTouching;
            invNumTouching.setReciprocal<HK_ACC_23_BIT, HK_DIV_IGNORE>(hkSimdFloat32::fromInt32(numTouching));
            ground.m_surfaceVelocity.mul(invNumTouching);
        }
        else
        {
            ground.m_supportedState = hkpSurfaceInfo::UNSUPPORTED;
        }
    }
    else
    {
        ground.m_supportedState = hkpSurfaceInfo::UNSUPPORTED;
    }
    ground.m_surfaceDistanceExcess = 0.0f;
    // "Et" monitor timer end: not reproduced
}

// 0x00F1C760
void hkpCharacterProxy::checkSupport(const hkVector4f& direction, hkpSurfaceInfo& ground)
{
    hkpAllCdPointCollector startPointCollector;
    checkSupportWithCollector(direction, ground, startPointCollector);
}

// 0x00F1C060: the new start points replace the closest manifold points (or remove them), the closest new ones and
// the first cast hit are added, and near duplicates are removed. Single threaded, the bodies and phantoms of the
// manifold are listened to.
void hkpCharacterProxy::updateManifold(const hkpAllCdPointCollector& startCollector, const hkpAllCdPointCollector& castCollector, hkArray<hkpRootCdPoint, hkContainerHeapAllocator>& manifold, hkArray<hkpRigidBody*, hkContainerHeapAllocator>& bodies, hkArray<hkpPhantom*, hkContainerHeapAllocator>& phantoms, hkBool isMultithreaded)
{
    if (!isMultithreaded.m_bool)
    {
        for (int i = 0; i < bodies.getSize(); i++)
        {
            bodies[i]->removeEntityListener(this);
        }
        for (int i = 0; i < phantoms.getSize(); i++)
        {
            phantoms[i]->removePhantomListener(this);
        }
        bodies.clear();
        phantoms.clear();
    }

    // the start points and their minimum distance
    const int numStartHits = startCollector.getHits().getSize();
    hkLocalArray<hkpRootCdPoint> startPointHits(numStartHits);
    float minimumDistance = HK_REAL_MAX; // HK_QUADREAL_MAX, 0x7F7FFFEE
    for (int i = 0; i < numStartHits; i++)
    {
        const hkpRootCdPoint& hit = startCollector.getHits()[i];
        startPointHits.pushBackUnchecked(hit);
        if (hit.m_contact.m_separatingNormal.m_quad.m128_f32[3] < minimumDistance)
        {
            minimumDistance = hit.m_contact.m_separatingNormal.m_quad.m128_f32[3];
        }
    }

    for (int s = manifold.getSize() - 1; s >= 0; s--)
    {
        int bestIndex = -1;
        float minDistance = 1.1f;
        for (int h = 0; h < startPointHits.getSize(); h++)
        {
            const float distance = surfaceDistance(startPointHits[h], manifold[s]);
            if (distance < minDistance)
            {
                minDistance = distance;
                bestIndex = h;
            }
        }
        if (bestIndex >= 0)
        {
            if (manifold[s].m_rootCollidableB != startPointHits[bestIndex].m_rootCollidableB)
            {
                fireContactRemoved(manifold[s]);
                fireContactAdded(startPointHits[bestIndex]);
            }
            manifold[s] = startPointHits[bestIndex];
            startPointHits.removeAt(bestIndex);
        }
        else
        {
            fireContactRemoved(manifold[s]);
            manifold.removeAt(s);
        }
    }

    for (int h = 0; h < startPointHits.getSize(); h++)
    {
        const hkpRootCdPoint& hit = startPointHits[h];
        if (hit.m_contact.m_separatingNormal.m_quad.m128_f32[3] == minimumDistance)
        {
            const int index = findSurface(hit, manifold);
            if (index < 0)
            {
                manifold.pushBack(hit);
                fireContactAdded(hit);
            }
            else
            {
                if (hit.m_rootCollidableB != manifold[index].m_rootCollidableB)
                {
                    fireContactRemoved(manifold[index]);
                    fireContactAdded(hit);
                }
                manifold[index] = hit;
            }
        }
    }

    if (castCollector.getHits().getSize() > 0)
    {
        const hkpRootCdPoint& surface = castCollector.getHits()[0];
        const int index = findSurface(surface, manifold);
        if (index == -1)
        {
            manifold.pushBack(surface);
            fireContactAdded(surface);
        }
    }

    for (int e = manifold.getSize() - 1; e > 0; e--)
    {
        for (int i = e - 1; i >= 0; i--)
        {
            if (surfaceDistance(manifold[e], manifold[i]) < 0.1f)
            {
                fireContactRemoved(manifold[e]);
                manifold.removeAt(e);
                break;
            }
        }
    }

    if (!isMultithreaded.m_bool)
    {
        for (int i = 0; i < manifold.getSize(); i++)
        {
            addEntityOrPhantom(manifold[i].m_rootCollidableB, bodies, phantoms);
        }
    }
}

// 0x00F190C0: other characters get an interaction callback; dynamic bodies get the impulse that stops the
// character's approach (limited by m_characterStrength) plus the character's weight on them, through the listeners.
// With a command (multithreaded integration) the impulses are queued instead of applied; RFG passes none.
void hkpCharacterProxy::applySurfaceInteractions(const hkStepInfo& stepInfo, const hkVector4f& worldGravity, hkpCharacterProxyIntegrateCommand* command, hkArray<hkpRootCdPoint, hkContainerHeapAllocator>& manifold)
{
    // hkpCharacterProxyIntegrateCommand (not declared): +0x08 free impulse slots, +0x10 the slots, 0x30 bytes each
    // (impulse, position, body at +0x20); the first free slot has a null body
    struct ObjectImpulse
    {
        hkVector4f m_impulse;
        hkVector4f m_position;
        hkpRigidBody* m_body;
    };
    const bool multithreaded = command != nullptr;
    ObjectImpulse* nextImpulse = nullptr;
    if (multithreaded)
    {
        nextImpulse = *(ObjectImpulse**)((char*)command + 0x10);
        if (*(int*)((char*)command + 0x8) > 0)
        {
            while (nextImpulse->m_body != nullptr)
            {
                nextImpulse++;
            }
        }
    }

    hkpCharacterObjectInteractionEvent input;
    input.m_timestep = stepInfo.m_deltaTime.val();
    const hkSimdFloat32 zero = hkSimdFloat32::getConstant<HK_QUADREAL_0>();
    for (int s = 0; s < manifold.getSize(); s++)
    {
        const hkpCollidable* rootB = manifold[s].m_rootCollidableB;
        const hkpWorldObject* owner = (const hkpWorldObject*)rootB->getOwner();

        // another character
        if (owner->hasProperty(HK_PROPERTY_CHARACTER_PROXY, hkpWorldObject::MULTI_THREADING_CHECKS_ENABLE))
        {
            hkpCharacterProxy* otherChar = (hkpCharacterProxy*)(hkUlong)owner->getProperty(HK_PROPERTY_CHARACTER_PROXY, hkpWorldObject::MULTI_THREADING_CHECKS_ENABLE).m_data;
            if (otherChar != nullptr)
            {
                fireCharacterInteraction(otherChar, manifold[s].m_contact);
                continue;
            }
        }
        if (multithreaded && *(int*)((char*)command + 0x8) == 0)
        {
            continue;
        }

        hkpRigidBody* body = hkpGetRigidBody(rootB);
        if (body == nullptr || hkpCharacterProxy_isFixedOrKeyframed(body))
        {
            continue;
        }

        // the contact position with the distance in w
        input.m_position.setXYZ_W(manifold[s].m_contact.m_position, manifold[s].m_contact.m_separatingNormal);
        input.m_normal = manifold[s].m_contact.m_separatingNormal;
        input.m_body = body;

        hkVector4f pointVel;
        body->getPointVelocity(input.m_position, pointVel);
        pointVel.sub(m_velocity);
        const hkSimdFloat32 projectedVelocity = pointVel.dot<3>(input.m_normal);
        input.m_projectedVelocity = projectedVelocity.getReal();

        // the velocity change that stops the approach, plus the penetration recovery
        hkSimdFloat32 deltaVelocity = hkSimdFloat32::fromFloat(0.9f) * (zero - projectedVelocity); // 0x3F666666
        const hkSimdFloat32 distance = input.m_position.getComponent<3>();
        // ucomiss/jnc: NaN recovers
        if (!(distance.getReal() >= zero.getReal()))
        {
            deltaVelocity = deltaVelocity + hkSimdFloat32::fromFloat(stepInfo.m_invDeltaTime.val()) * distance * hkSimdFloat32::fromFloat(0.4f); // 0x3ECCCCCD
        }

        hkpCharacterObjectInteractionResult output;
        output.m_objectImpulse.setZero();
        output.m_impulsePosition = input.m_position;
        if (!(deltaVelocity.getReal() >= zero.getReal()))
        {
            hkMatrix3f worldInvInertia;
            body->getRigidMotion()->getInertiaInvWorld(worldInvInertia);
            hkVector4f r;
            r.setSub(input.m_position, body->getCenterOfMassInWorld());
            hkVector4f jacAng;
            jacAng.setCross(r, input.m_normal);
            hkVector4f rc;
            rc._setRotatedDir(worldInvInertia, jacAng);
            const hkSimdFloat32 objectMassInv = rc.dot<3>(jacAng) + body->getRigidMotion()->m_inertiaAndMassInv.getComponent<3>();
            input.m_objectMassInv = objectMassInv.getReal();
            hkSimdFloat32 objectImpulse;
            objectImpulse.setDiv<HK_ACC_23_BIT, HK_DIV_IGNORE>(deltaVelocity, objectMassInv);
            input.m_objectImpulse = objectImpulse.getReal();
            const float maxPushImpulse = -(m_characterStrength * stepInfo.m_deltaTime.val());
            if (maxPushImpulse > input.m_objectImpulse)
            {
                input.m_objectImpulse = maxPushImpulse;
            }
            output.m_objectImpulse.setMul(input.m_normal, hkSimdFloat32::fromFloat(input.m_objectImpulse));
        }
        else
        {
            input.m_objectImpulse = 0.0f;
            input.m_objectMassInv = body->getRigidMotion()->m_inertiaAndMassInv.m_quad.m128_f32[3];
        }

        // the character's weight
        hkVector4f gravityStep;
        gravityStep.setMul(hkSimdFloat32::fromFloat(stepInfo.m_deltaTime.val()), worldGravity);
        float gravityProjected = gravityStep.dot<3>(input.m_normal).getReal();
        if (0.0f > input.m_projectedVelocity)
        {
            gravityProjected = gravityProjected - input.m_projectedVelocity;
        }
        if (-1.192092896e-07f > gravityProjected) // -HK_REAL_EPSILON, 0xB4000000
        {
            output.m_objectImpulse.addMul(input.m_normal, hkSimdFloat32::fromFloat(m_characterMass * gravityProjected));
        }

        fireObjectInteraction(input, output);

        hkSimdFloat32 impulseLength2;
        impulseLength2.setAbs(output.m_objectImpulse.lengthSquared<3>() - zero);
        // ucomiss/jc: NaN skips
        if (impulseLength2.getReal() >= 1.0e-7f) // 0x33D6BF95
        {
            if (multithreaded)
            {
                nextImpulse->m_body = body;
                nextImpulse->m_position = output.m_impulsePosition;
                nextImpulse->m_impulse = output.m_objectImpulse;
                nextImpulse++;
                if (--*(int*)((char*)command + 0x8) != 0)
                {
                    nextImpulse->m_body = nullptr;
                }
            }
            else
            {
                body->applyPointImpulse(output.m_objectImpulse, output.m_impulsePosition);
            }
        }
    }
}

// 0x00F18D10: a removed body leaves the manifold and the listened bodies; a trigger body leaves the trigger list
void hkpCharacterProxy::entityRemovedCallback(hkpEntity* entity)
{
    entity->removeEntityListener(this);
    if (entity->hasProperty(HK_PROPERTY_TRIGGER_VOLUME, hkpWorldObject::MULTI_THREADING_CHECKS_ENABLE))
    {
        hkpTriggerVolume* triggerVolume = (hkpTriggerVolume*)(hkUlong)entity->getProperty(HK_PROPERTY_TRIGGER_VOLUME, hkpWorldObject::MULTI_THREADING_CHECKS_ENABLE).m_data;
        // not checked for -1, as the original
        const int i = m_overlappingTriggerVolumes.indexOf(triggerVolume);
        m_overlappingTriggerVolumes.m_size--;
        if (m_overlappingTriggerVolumes.m_size != i)
        {
            m_overlappingTriggerVolumes.m_data[i] = m_overlappingTriggerVolumes.m_data[m_overlappingTriggerVolumes.m_size];
        }
        return;
    }
    for (int i = m_manifold.getSize() - 1; i >= 0; i--)
    {
        if ((hkpEntity*)hkpGetRigidBody(m_manifold[i].m_rootCollidableB) == entity)
        {
            m_manifold.removeAt(i);
        }
    }
    const int index = m_bodies.indexOf((hkpRigidBody*)entity);
    if (index != -1)
    {
        m_bodies.removeAt(index);
    }
}

// 0x00F18E60: a removed phantom leaves the manifold and the listened phantoms (not checked for -1, as the original)
void hkpCharacterProxy::phantomRemovedCallback(hkpPhantom* phantom)
{
    phantom->removePhantomListener(this);
    for (int i = m_manifold.getSize() - 1; i >= 0; i--)
    {
        if (hkpCharacterProxy_getPhantom(m_manifold[i].m_rootCollidableB) == phantom)
        {
            m_manifold.removeAt(i);
        }
    }
    const int index = m_phantoms.indexOf(phantom);
    m_phantoms.m_size--;
    if (m_phantoms.m_size != index)
    {
        m_phantoms.m_data[index] = m_phantoms.m_data[m_phantoms.m_size];
    }
}
