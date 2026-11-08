// Original: volition/rfg/code/havok/hk_custom_character_proxy.cpp
//
// The character proxy of the character controller (player_minimal.md P1), from rfg.exe: Havok's hkpCharacterProxy
// integration split into steps, so that the linear casts of all characters run together as scheduler jobs
// (havok_mt_query). One step of a character: mt_integrate_start_initial_casts casts the last displacement; then, per
// iteration, mt_integrate_process_casts solves the manifold and casts the solution, mt_integrate_further_casts moves
// to the first hit and casts again while time is left; mt_push_objects pushes the debris the casts touched and
// mt_integrate_finish moves the phantom and applies the surface interactions.
//
// Some of this file's arithmetic is in double (the source's double literals): surfaceDistance, the manifold refresh
// tolerance and the start point distance check of the collector. The SDK's hkpCharacterProxy::findSurface (which
// uses the SDK's surfaceDistance) is called from here unchanged, as in the original.

#include "volition/rfg/code/havok/hk_custom_character_proxy.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/algorithm/sort/hksort.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/localarray/hklocalarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hksimdfloat32.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcdbody.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcdpoint.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcollidable.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpprocesscollisioninput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkprigidbody.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/motion/hkpmotion.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/phantom/hkpcachingshapephantom.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/phantom/hkpphantom.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/phantom/hkpshapephantom.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/hkpworld.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/utilities/charactercontrol/characterproxy/hkpcharacterproxycinfo.h"
#include "volition/rfg/code/destruction/rfg_mover.h"
#include "volition/rfg/code/destruction/rfg_shapes.h"
#include "volition/rfg/code/geom/character_instance.h"
#include "volition/rfg/code/havok/api/hkcharactercontroller.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/havok/havok_mt_query.h"
#include "volition/rfg/code/havok/havok_ragdoll.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_game/multi_game_object_backpack.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/object_type.h"
#include "volition/vlib/code/math/vector3.h"

// ---- file-static globals
// $S7 (0x02B953C8): compiler guard flags of function-local statics, not declared.

// hkpWorldObject property keys of the game (havok.h): the object of a world object, the ragdoll of a body
static const unsigned int HAVOK_PROPERTY_RAGDOLL = 1;

// hkpGetRigidBody / hkpGetPhantom (inlined): the owner of an entity's or a phantom's collidable
static inline hkpPhantom* hk_custom_character_proxy_get_phantom(const hkpCollidable* collidable)
{
    if (collidable->m_broadPhaseHandle.m_type == hkpWorldObject::BROAD_PHASE_PHANTOM)
    {
        return (hkpPhantom*)collidable->getOwner();
    }
    return nullptr;
}

// hkpCdBody::getRootCollidable (inlined)
static inline const hkpCollidable* hk_custom_character_proxy_root(const hkpCdBody* body)
{
    while (body->m_parent != nullptr)
    {
        body = body->m_parent;
    }
    return (const hkpCollidable*)body;
}

// The property of a world object as a pointer (hkpWorldObject::hasProperty + getProperty, inlined), null when the
// object has none
static inline void* hk_custom_character_proxy_property(const hkpWorldObject* wo, unsigned int key)
{
    if (wo == nullptr || !wo->hasProperty(key, hkpWorldObject::MULTI_THREADING_CHECKS_ENABLE))
    {
        return nullptr;
    }
    return (void*)(hkUlong)wo->getProperty(key, hkpWorldObject::MULTI_THREADING_CHECKS_ENABLE).m_data;
}

// hkpCharacterProxy::addEntityOrPhantom as inlined in refreshManifold_custom and mt_integrate_finish: the listener is
// added before the body or phantom is pushed (the SDK's copy pushes first), which orders their allocations
static void hk_custom_character_proxy_listen(hkpCharacterProxy* proxy, const hkpCollidable* collidable, hkArray<hkpRigidBody*, hkContainerHeapAllocator>& bodies, hkArray<hkpPhantom*, hkContainerHeapAllocator>& phantoms)
{
    hkpRigidBody* body = hkpGetRigidBody(collidable);
    if (body != nullptr && bodies.indexOf(body) == -1)
    {
        body->addEntityListener(proxy);
        bodies.pushBack(body);
    }
    hkpPhantom* phantom = hk_custom_character_proxy_get_phantom(collidable);
    if (phantom != nullptr && phantoms.indexOf(phantom) == -1)
    {
        phantom->addPhantomListener(proxy);
        phantoms.pushBack(phantom);
    }
}

// 0x00813BC0
character_proxy_contact_to_push::character_proxy_contact_to_push()
{
}

// ---- character_proxy_cd_point_collector

// 0x008337F0
character_proxy_cd_point_collector::character_proxy_cd_point_collector()
{
}

// 0x00833850
character_proxy_cd_point_collector::~character_proxy_cd_point_collector()
{
}

// 0x00833820 (hk_custom_character_proxy.h:57)
void character_proxy_cd_point_collector::operator delete(void* p, unsigned int nbytes)
{
    if (p != nullptr)
    {
        hkMemoryRouter::getInstance().heap().blockFree(p, sizeof(character_proxy_cd_point_collector));
    }
}

// 0x00830250: the points of a character's casts. Points with other characters' phantoms within 0.22 m sideways are
// dropped, unless one of the two is in a melee attack (anim action 0x3DE). Bodies: the character's filter object is
// remembered as touched and dropped; a player with the rhino backpack ignores walls (and floors with jump boost) of
// destroyable building pieces; ragdoll bodies and debris the character pushes away become contacts to push (not
// points).
void character_proxy_cd_point_collector::addCdPoint(const hkpCdPoint& event)
{
    const hkpCollidable* rootA = hk_custom_character_proxy_root(&event.m_cdBodyA);
    human* owner = (human*)hk_custom_character_proxy_property((const hkpWorldObject*)rootA->getOwner(), HAVOK_PROPERTY_OBJECT);

    const hkpCollidable* rootB = hk_custom_character_proxy_root(&event.m_cdBodyB);
    hkpWorldObject* worldObjectB = (hkpWorldObject*)rootB->getOwner();
    if (rootB->m_broadPhaseHandle.m_type == hkpWorldObject::BROAD_PHASE_ENTITY && worldObjectB != nullptr)
    {
        object* objp = object_from_world_obj(worldObjectB);
        if (objp != nullptr && owner != nullptr && owner->get_obj_type() == OT_HUMAN)
        {
            if (owner->object_filter_handle == objp->get_handle())
            {
                owner->hflags.filter_handle_valid = 1;
                return;
            }
            if (owner->get_sub_type() == OT_SUB_HUMAN_PLAYER)
            {
                multi_object_backpack* backpack = ((player*)owner)->backpack;
                if (backpack != nullptr)
                {
                    const float nx = event.m_contact.m_separatingNormal.m_quad.m128_f32[0];
                    const float ny = event.m_contact.m_separatingNormal.m_quad.m128_f32[1];
                    const float nz = event.m_contact.m_separatingNormal.m_quad.m128_f32[2];
                    // double sum, rounded to float, against (double)0.95f
                    const float upDot = (float)((double)ny * (double)UP_VECTOR.y + (double)nx * (double)UP_VECTOR.x + (double)nz * (double)UP_VECTOR.z);
                    if (backpack->get_backpack_abilities()->rhino &&
                        (backpack->get_backpack_abilities()->jump_boost || (double)0.95f > (double)upDot) &&
                        objp->get_obj_type() == OT_MOVER)
                    {
                        if (objp->get_sub_type() != OT_SUB_MOVER_RFG)
                        {
                            return;
                        }
                        rfg_mover* mover = World.object_cast<rfg_mover>(objp);
                        if (mover != nullptr && !mover->flags.invulnerable)
                        {
                            rfg_destroyable_shape* shape = mover->get_shape();
                            if (shape != nullptr)
                            {
                                rfg_subpiece* subpiece = shape->get_subpiece(event.m_cdBodyB.m_shapeKey);
                                if (subpiece != nullptr)
                                {
                                    rfg_subpiece_base* base = shape->get_subpiece_base(subpiece);
                                    if (base != nullptr && (base->flags & 0x20) == 0)
                                    {
                                        return;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }

        if (worldObjectB->hasProperty(HAVOK_PROPERTY_RAGDOLL, hkpWorldObject::MULTI_THREADING_CHECKS_ENABLE))
        {
            // a ragdoll body (read as a human without a type check, as the original)
            human* ragdolled = (human*)objp;
            if (ragdolled != nullptr && ragdolled->ragdoll_state == HUMAN_RAGDOLL_STATE_ACTIVE_ON_IMPACT)
            {
                if (havok_get_property((const hkpWorldObject*)rootA->getOwner(), HAVOK_PROPERTY_OBJECT, nullptr, true) == objp)
                {
                    return;
                }
                havok_ragdoll_set_hit_wall(ragdolled->ragdoll_idx);
            }
            character_proxy_contact_to_push& push = m_contacts_to_push->expandOne();
            push.m_contact.m_position = event.m_contact.m_position;
            push.m_contact.m_separatingNormal = event.m_contact.m_separatingNormal;
            push.m_collidable = hk_custom_character_proxy_root(&event.m_cdBodyB);
            return;
        }
        if (objp != nullptr)
        {
            const bool pushesAllDebris = owner->cur_state == HS_LADDER;
            if (character_controller_should_push_away(objp, pushesAllDebris) &&
                (double)0.95f > (double)event.m_contact.m_separatingNormal.m_quad.m128_f32[1]) // 0x3FEE666660000000
            {
                character_proxy_contact_to_push& push = m_contacts_to_push->expandOne();
                push.m_contact.m_position = event.m_contact.m_position;
                push.m_contact.m_separatingNormal = event.m_contact.m_separatingNormal;
                push.m_collidable = hk_custom_character_proxy_root(&event.m_cdBodyB);
                return;
            }
        }
    }
    else if (worldObjectB != nullptr)
    {
        // another character's phantom
        if (worldObjectB->hasProperty(HAVOK_PROPERTY_OBJECT, hkpWorldObject::MULTI_THREADING_CHECKS_ENABLE))
        {
            human* other = (human*)(hkUlong)worldObjectB->getProperty(HAVOK_PROPERTY_OBJECT, hkpWorldObject::MULTI_THREADING_CHECKS_ENABLE).m_data;
            // the owner is not checked for null (as the original)
            if (other != nullptr && other->get_obj_type() == OT_HUMAN && !other->char_inst->is_playing_action(0x3DE) &&
                !owner->char_inst->is_playing_action(0x3DE))
            {
                static const float dist = 0.22f; // 0x01660A34
                static const float dist_squared = (float)((double)dist * (double)dist);
                const float dx = (float)((double)event.m_contact.m_position.m_quad.m128_f32[0] - (double)owner->pos.x);
                const float dz = (float)((double)event.m_contact.m_position.m_quad.m128_f32[2] - (double)owner->pos.z);
                const float d2 = (float)((double)dx * (double)dx + (double)dz * (double)dz);
                if ((double)d2 > (double)dist_squared)
                {
                    return;
                }
            }
        }
    }
    hkpAllCdPointCollector::addCdPoint(event);
}

// ---- hk_custom_character_proxy

// 0x008208A0
hk_custom_character_proxy::multithreaded_integration::multithreaded_integration()
{
    m_linearcast_input.m_maxExtraPenetration = 1.192092896e-07f; // HK_REAL_EPSILON
    m_linearcast_input.m_startPointTolerance = 1.192092896e-07f;
}

// 0x008208C0
hk_custom_character_proxy::apply_surface_interactions_params::apply_surface_interactions_params()
{
}

// 0x00837520: both collectors queue their contacts to push into the proxy's list
hk_custom_character_proxy::hk_custom_character_proxy(const hkpCharacterProxyCinfo& c_info)
    : hkpCharacterProxy(c_info)
{
    m_cast_collector.m_contacts_to_push = &m_contacts_to_push;
    m_start_collector.m_contacts_to_push = &m_contacts_to_push;
}

// 0x00833920
hk_custom_character_proxy::~hk_custom_character_proxy()
{
}

// 0x0080D2A0 (hk_custom_character_proxy.h:87)
character_proxy_cd_point_collector& hk_custom_character_proxy::get_cast_collector()
{
    return m_cast_collector;
}

// 0x0080D2B0 (hk_custom_character_proxy.h:93)
character_proxy_cd_point_collector& hk_custom_character_proxy::get_start_point_collector()
{
    return m_start_collector;
}

// 0x0082B480: a body the character walks into gets the character's approach speed times its mass as an impulse,
// along the horizontal contact normal
void hk_custom_character_proxy::push_rigid_body(hkpRigidBody* p_rigid_body, const hkContactPoint& contact_point)
{
    hkVector4f relVel;
    relVel.setSub(m_velocity, p_rigid_body->getRigidMotion()->m_linearVelocity);
    float approach = relVel.dot<3>(contact_point.m_separatingNormal).getReal();
    // fcomip/jnc: NaN pushes
    if (!(approach >= 0.0f))
    {
        approach = p_rigid_body->getRigidMotion()->getMass() * approach;
        hkVector4f direction = contact_point.m_separatingNormal;
        direction.m_quad.m128_f32[1] = 0.0f;
        // normalizeIfNotZero<3> (23 bit, zero for a zero length)
        const hkSimdFloat32 length2 = direction.lengthSquared<3>();
        if ((hkSimdFloat32::getConstant<HK_QUADREAL_0>() - length2).getReal() < 0.0f)
        {
            direction.mul(length2.sqrtInverse<HK_ACC_23_BIT, HK_SQRT_SET_ZERO>());
        }
        hkVector4f impulse;
        impulse.setMul(hkSimdFloat32::fromFloat(approach), direction);
        p_rigid_body->activate();
        p_rigid_body->getRigidMotion()->applyLinearImpulse(impulse);
    }
}

// 0x008208D0: as hkpCharacterProxy::convertFractionToDistance; the projection is negated by its sign bit (the SDK
// subtracts it from zero)
void hk_custom_character_proxy::convertFractionToDistance(const hkpRootCdPoint* cHit, int numHits, const hkVector4f& displacement) const
{
    hkpRootCdPoint* hit = const_cast<hkpRootCdPoint*>(cHit);
    for (int i = numHits - 1; i >= 0; i--)
    {
        const float fraction = hit->m_contact.m_separatingNormal.m_quad.m128_f32[3];
        hit->m_contact.m_position.m_quad.m128_f32[3] = fraction;
        const float projLength = displacement.dot<3>(hit->m_contact.m_separatingNormal).getReal();
        hit->m_contact.m_separatingNormal.m_quad.m128_f32[3] = -projLength * fraction;
        hit++;
    }
}

// 0x008126C0
void hk_custom_character_proxy::extractSurfaceVelocity(const hkpRootCdPoint& hit, hkVector4f& velocityOut) const
{
    velocityOut.setZero();
    const hkpRigidBody* body = hkpGetRigidBody(hit.m_rootCollidableB);
    if (body != nullptr)
    {
        body->getPointVelocity(hit.m_contact.m_position, velocityOut);
    }
}

// 0x00826860: hkpCharacterProxy::surfaceDistance with the source's double literals: the angle term in double, and
// the sum on the x87 stack (53-bit)
float hk_custom_character_proxy::surfaceDistance(const hkpRootCdPoint& p1, const hkpRootCdPoint& p2) const
{
    const float normalDot = p1.m_contact.m_separatingNormal.dot<3>(p2.m_contact.m_separatingNormal).getReal();
    const double sensitivity2 = (double)m_contactAngleSensitivity * (double)m_contactAngleSensitivity;
    const float oneMinusDot = (float)(1.0 - (double)normalDot);
    const float angleSquared = (float)((double)oneMinusDot * sensitivity2);
    const float planeDistance = p1.m_contact.m_separatingNormal.m_quad.m128_f32[3] - p2.m_contact.m_separatingNormal.m_quad.m128_f32[3];
    const float planeDistanceSqrd = planeDistance * planeDistance;

    hkVector4f p1Vel;
    extractSurfaceVelocity(p1, p1Vel);
    hkVector4f p2Vel;
    extractSurfaceVelocity(p2, p2Vel);
    hkVector4f velDiff;
    velDiff.setSub(p1Vel, p2Vel);
    const float velocityDiffSqrd = velDiff.lengthSquared<3>().getReal();

    return (float)((double)velocityDiffSqrd * (double)0.1f + (double)angleSquared * 10.0 + (double)planeDistanceSqrd); // 0.1 as 0x3FB99999A0000000
}

// 0x00830100: the SDK's addMaxSlopePlane (same arithmetic; the slope negated by its sign bit). LTCG: index in EAX,
// surfaceConstraintsInOut in EDI.
static bool addMaxSlopePlane(float maxSlopeCos, const hkVector4f& up, int index, hkArray<hkSurfaceConstraintInfo, hkContainerHeapAllocator>& surfaceConstraintsInOut)
{
    const float surfaceVerticalComponent = surfaceConstraintsInOut[index].m_plane.dot<3>(up).getReal();
    if (surfaceVerticalComponent > 0.01f && (double)maxSlopeCos > (double)surfaceVerticalComponent)
    {
        hkSurfaceConstraintInfo& newConstraint = surfaceConstraintsInOut.expandOne();
        newConstraint = surfaceConstraintsInOut[index];
        newConstraint.m_plane.addMul(up, hkSimdFloat32::fromFloat(-surfaceVerticalComponent));
        newConstraint.m_plane.normalize<3, HK_ACC_23_BIT, HK_SQRT_SET_ZERO>();
        return true;
    }
    return false;
}

// 0x0082B7E0: the phantom gets the integration transform, the collectors are reset and the phantom's shape is cast
// to the target against its cached overlaps, as a scheduler job (with the start point collector for the first cast of
// a step: the penetrations at the start)
void hk_custom_character_proxy::mt_integrate_linear_cast(bool initial_penetration_check)
{
    m_shapePhantom->getMotionState()->getTransform() = m_mti.m_transform;
    m_cast_collector.reset();
    m_start_collector.reset();
    // the phantom's collision details (hkpSimpleShapePhantom and hkpCachingShapePhantom share their layout)
    hkpCachingShapePhantom* phantom = (hkpCachingShapePhantom*)m_shapePhantom;
    const unsigned int numCollidables = phantom->m_collisionDetails.getSize();
    if (numCollidables != 0)
    {
        havok_mt_query_pair_linearcast(&phantom->m_collidable, m_mti.m_linearcast_input.m_to, &phantom->m_collisionDetails.begin()->m_collidable, numCollidables,
                                       sizeof(hkpCachingShapePhantom::CollisionDetail), m_mti.m_linearcast_input.m_startPointTolerance,
                                       m_mti.m_linearcast_input.m_maxExtraPenetration, m_cast_collector, initial_penetration_check ? &m_start_collector : nullptr);
    }
}

// 0x00833BC0: hkpCharacterProxy::updateManifold without the listened bodies (multithreaded); the listeners are told
// before a point is added, and this class's surfaceDistance is used except in findSurface
void hk_custom_character_proxy::updateManifold_mt(const hkpAllCdPointCollector& startPointCollector, const hkpAllCdPointCollector& castCollector)
{
    const int numStartHits = startPointCollector.getHits().getSize();
    hkLocalArray<hkpRootCdPoint> startPointHits(numStartHits);
    float minimumDistance = HK_REAL_MAX; // 0x7F7FFFEE
    for (int i = 0; i < numStartHits; i++)
    {
        const hkpRootCdPoint& hit = startPointCollector.getHits()[i];
        startPointHits.pushBackUnchecked(hit);
        if (hit.m_contact.m_separatingNormal.m_quad.m128_f32[3] < minimumDistance)
        {
            minimumDistance = hit.m_contact.m_separatingNormal.m_quad.m128_f32[3];
        }
    }

    for (int s = m_manifold.getSize() - 1; s >= 0; s--)
    {
        int bestIndex = -1;
        float minDistance = 1.1f;
        for (int h = 0; h < startPointHits.getSize(); h++)
        {
            const float distance = surfaceDistance(startPointHits[h], m_manifold[s]);
            if (distance < minDistance)
            {
                minDistance = distance;
                bestIndex = h;
            }
        }
        if (bestIndex >= 0)
        {
            if (startPointHits[bestIndex].m_rootCollidableB != m_manifold[s].m_rootCollidableB)
            {
                fireContactRemoved(m_manifold[s]);
                fireContactAdded(startPointHits[bestIndex]);
            }
            m_manifold[s] = startPointHits[bestIndex];
            startPointHits.removeAt(bestIndex);
        }
        else
        {
            fireContactRemoved(m_manifold[s]);
            m_manifold.removeAt(s);
        }
    }

    for (int h = 0; h < startPointHits.getSize(); h++)
    {
        const hkpRootCdPoint& hit = startPointHits[h];
        if (hit.m_contact.m_separatingNormal.m_quad.m128_f32[3] == minimumDistance)
        {
            const int index = findSurface(hit, m_manifold);
            if (index < 0)
            {
                fireContactAdded(hit);
                m_manifold.pushBack(hit);
            }
            else
            {
                if (hit.m_rootCollidableB != m_manifold[index].m_rootCollidableB)
                {
                    fireContactRemoved(m_manifold[index]);
                    fireContactAdded(hit);
                }
                m_manifold[index] = hit;
            }
        }
    }

    if (castCollector.getHits().getSize() > 0)
    {
        const hkpRootCdPoint& surface = castCollector.getHits()[0];
        if (findSurface(surface, m_manifold) == -1)
        {
            fireContactAdded(surface);
            m_manifold.pushBack(surface);
        }
    }

    for (int e = m_manifold.getSize() - 1; e > 0; e--)
    {
        for (int i = e - 1; i >= 0; i--)
        {
            // against (double)0.1f, 0x3FB99999A0000000
            if (surfaceDistance(m_manifold[e], m_manifold[i]) < 0.1f)
            {
                fireContactRemoved(m_manifold[e]);
                m_manifold.removeAt(e);
                break;
            }
        }
    }
}

// 0x00837640: the start points at the current position replace the manifold, then the bodies and phantoms of the
// manifold are listened to
void hk_custom_character_proxy::refreshManifold_custom(character_proxy_cd_point_collector& startPointCollector, character_proxy_cd_point_collector& castCollector)
{
    startPointCollector.reset();
    hkpCollisionInput input = *m_shapePhantom->m_world->m_collisionInput;
    input.m_tolerance = (float)((double)m_keepContactTolerance + (double)m_keepDistance + (double)input.m_tolerance.val());
    m_shapePhantom->getClosestPoints(startPointCollector, &input);
    updateManifold_mt(startPointCollector, castCollector);

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
    for (int i = 0; i < m_manifold.getSize(); i++)
    {
        hk_custom_character_proxy_listen(this, m_manifold[i].m_rootCollidableB, m_bodies, m_phantoms);
    }
}

// 0x00830780: the step starts from the phantom's transform with the whole time step; the first cast repeats the last
// displacement, with the start points within the keep distance and contact tolerance
void hk_custom_character_proxy::mt_integrate_start_initial_casts(float time_step)
{
    m_mti.m_transform = m_shapePhantom->getCollidable()->getTransform();
    m_mti.m_remaining_time = time_step;
    m_contacts_to_push.clear();
    m_mti.m_linearcast_input.m_to.setAdd(m_mti.m_transform.getTranslation(), m_oldDisplacement);
    m_mti.m_linearcast_input.m_startPointTolerance = m_keepContactTolerance + m_keepDistance;
    m_mti.m_linearcast_input.m_maxExtraPenetration = 0.01f; // 0x3C23D70A
    mt_integrate_linear_cast(true);
}

// 0x00834290: the manifold is updated from the casts and solved for the remaining time. True when the solution is
// the last displacement (within 1 mm per axis): no further cast is needed.
bool hk_custom_character_proxy::mt_integrate_solve_casts(const hkStepInfo& stepInfo, const hkVector4f& worldGravity)
{
    const float remainingTime = m_mti.m_remaining_time;
    if (m_cast_collector.getHits().getSize() > 0)
    {
        m_cast_collector.sortHits();
        convertFractionToDistance(m_cast_collector.getHits().begin(), m_cast_collector.getHits().getSize(), m_oldDisplacement);
    }
    updateManifold_mt(m_start_collector, m_cast_collector);

    hkLocalArray<hkSurfaceConstraintInfo> surfaceConstraints(m_manifold.getSize() + m_userPlanes + 10);
    surfaceConstraints.setSizeUnchecked(m_manifold.getSize());
    for (int i = 0; i < m_manifold.getSize(); i++)
    {
        extractSurfaceConstraintInfo(m_manifold[i], surfaceConstraints[i], stepInfo.m_deltaTime.val() - remainingTime);
        addMaxSlopePlane(m_maxSlopeCosine, m_up, i, surfaceConstraints);
    }
    surfaceConstraints.reserve(surfaceConstraints.getSize() + m_userPlanes);

    hkSimplexSolverInput input;
    hkLocalArray<hkSurfaceConstraintInteraction> surfaceInteractions(surfaceConstraints.getSize() + m_userPlanes);
    input.m_velocity = m_velocity;
    input.m_deltaTime = remainingTime;
    // the time to cover half the keep distance
    const hkSimdFloat32 speed2 = m_velocity.lengthSquared<3>();
    if (speed2.getReal() == 0.0f)
    {
        input.m_minDeltaTime = 0.0f;
    }
    else
    {
        input.m_minDeltaTime = (float)((double)speed2.sqrtInverse<HK_ACC_23_BIT, HK_SQRT_SET_ZERO>().getReal() * (double)m_keepDistance * 0.5);
    }
    input.m_constraints = surfaceConstraints.begin();
    input.m_numConstraints = surfaceConstraints.getSize();
    input.m_maxSurfaceVelocity.set(m_maxCharacterSpeedForSolver, m_maxCharacterSpeedForSolver, m_maxCharacterSpeedForSolver, 0.0f);
    input.m_upVector = m_up;
    fireConstraintsProcessed(m_manifold, input);

    m_mti.m_simplex_solver_output.m_planeInteractions = surfaceInteractions.begin();
    hkSimplexSolverSolve(input, m_mti.m_simplex_solver_output);

    m_apply_surface_interactions_params.m_step_info = stepInfo;
    m_apply_surface_interactions_params.m_world_gravity = worldGravity;
    m_apply_surface_interactions_params.m_velocity = m_velocity;

    hkVector4f diff;
    diff.setSub(m_oldDisplacement, m_mti.m_simplex_solver_output.m_position);
    diff.setAbs(diff);
    return (_mm_movemask_ps(_mm_cmple_ps(diff.m_quad, _mm_set1_ps(0.001f))) & 7) == 7; // 0x3A83126F
}

// 0x00837870: the step's solve, then the cast of the solution unless the solution repeats the last displacement
void hk_custom_character_proxy::mt_integrate_process_casts(const hkStepInfo& step_info, const hkVector4f& world_gravity)
{
    if (!mt_integrate_solve_casts(step_info, world_gravity))
    {
        m_mti.m_linearcast_input.m_to.setAdd(m_mti.m_simplex_solver_output.m_position, m_mti.m_transform.getTranslation());
        mt_integrate_linear_cast(false);
        return;
    }
    m_cast_collector.reset();
    m_start_collector.reset();
}

// 0x00830830: move to the first new cast hit (the hits on known surfaces are dropped; the first new one joins the
// manifold), or the whole solution when nothing new was hit
void hk_custom_character_proxy::mt_integrate_move()
{
    bool noNewHit = false;
    if (m_cast_collector.getHits().getSize() > 0)
    {
        m_cast_collector.sortHits();
        convertFractionToDistance(m_cast_collector.getHits().begin(), m_cast_collector.getHits().getSize(), m_mti.m_simplex_solver_output.m_position);
        hkArray<hkpRootCdPoint, hkContainerHeapAllocator>& hits = m_cast_collector.getHits();
        while (findSurface(hits[0], m_manifold) != -1)
        {
            hits.removeAtAndCopy(0);
            if (hits.getSize() < 1)
            {
                noNewHit = true;
                break;
            }
        }
        if (hits.getSize() > 0)
        {
            fireContactAdded(hits[0]);
            m_manifold.pushBack(hits[0]);
        }
        if (!noNewHit)
        {
            const double timeUsed = moveToLinearCastHitPosition(m_mti.m_simplex_solver_output, hits[0], m_mti.m_linearcast_input, m_mti.m_transform.getTranslation());
            // x87: (double)remaining - the exact product, then rounded
            m_mti.m_remaining_time = (float)((double)m_mti.m_remaining_time - timeUsed);
            m_oldDisplacement = m_mti.m_simplex_solver_output.m_position;
            return;
        }
    }
    m_mti.m_transform.getTranslation().add(m_mti.m_simplex_solver_output.m_position);
    m_mti.m_remaining_time = m_mti.m_remaining_time - m_mti.m_simplex_solver_output.m_deltaTime;
    m_oldDisplacement = m_mti.m_simplex_solver_output.m_position;
}

// 0x00833B50: move, then cast the last displacement again while time is left (true: another iteration)
bool hk_custom_character_proxy::mt_integrate_further_casts(bool last_iteration)
{
    mt_integrate_move();
    if (m_mti.m_remaining_time > 1.192092896e-07f && !last_iteration) // HK_REAL_EPSILON
    {
        m_mti.m_linearcast_input.m_to.setAdd(m_mti.m_transform.getTranslation(), m_oldDisplacement);
        mt_integrate_linear_cast(true);
        return true;
    }
    m_cast_collector.reset();
    m_start_collector.reset();
    return false;
}

// 0x0082B5C0: the rigid bodies of the queued contacts are pushed
void hk_custom_character_proxy::mt_push_objects()
{
    const int numContacts = m_contacts_to_push.getSize();
    for (int i = 0; i < numContacts; i++)
    {
        character_proxy_contact_to_push& push = m_contacts_to_push[i];
        hkpRigidBody* body = hkpGetRigidBody(push.m_collidable);
        if (body != nullptr)
        {
            push_rigid_body(body, push.m_contact);
        }
    }
    m_contacts_to_push.clear();
}

// 0x0082B610: the solver's velocity becomes the proxy's, the phantom moves to the result and the manifold's bodies
// and phantoms are listened to; the surface interactions use the velocity from before the solve
void hk_custom_character_proxy::mt_integrate_finish()
{
    m_velocity = m_mti.m_simplex_solver_output.m_velocity;
    m_shapePhantom->setPosition(m_mti.m_transform.getTranslation(), m_mti.m_linearcast_input.m_startPointTolerance);

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
    for (int i = 0; i < m_manifold.getSize(); i++)
    {
        hk_custom_character_proxy_listen(this, m_manifold[i].m_rootCollidableB, m_bodies, m_phantoms);
    }

    const hkVector4f solvedVelocity = m_velocity;
    m_velocity = m_apply_surface_interactions_params.m_velocity;
    applySurfaceInteractions(m_apply_surface_interactions_params.m_step_info, m_apply_surface_interactions_params.m_world_gravity, nullptr, m_manifold);
    m_velocity = solvedVelocity;
}
