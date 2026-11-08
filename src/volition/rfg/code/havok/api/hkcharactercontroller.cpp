// Original: volition/rfg/code/havok/api/hkcharactercontroller.cpp
//
// The character controller of a human (player_minimal.md P1), from rfg.exe: an hk_custom_character_proxy with an
// hkpCachingShapePhantom whose shape follows the human's stance (controller_shape_mode), and MyCharacterListener,
// which turns the proxy's contacts into game reactions (ground object, pushing debris and humans, mover damage) and
// edits the solver's planes (no static friction, extra planes for NPCs on steep slopes).
//
// One frame of a human (human_process_controllers): update_state_for_integration turns the desired velocity, gravity
// and jump into the proxy's velocity (support check, slopes, the big step probe, sliding), then the mt_integrate_*
// steps run the proxies of all humans together and post_integrate clears the per-frame filter state.
//
// Much of the Volition arithmetic here is in double (the source's double literals): every expression that rounds
// once is written as the float operation, chains are emulated in double as the binary does (see the comments).
// do_frame / do_frame_internal and cast_ray are not ported (not reached by the human integration).

#include "volition/rfg/code/havok/api/hkcharactercontroller.h"

#include <xmmintrin.h>

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/hkmath.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/matrix/hktransformf.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/geometry/aabb/hkaabb.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/physics/hkstepinfo.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/internal/simplexsolver/hksimplexsolver.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcollidable.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/query/castutil/hkplinearcastinput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/query/collector/pointcollector/hkpallcdpointcollector.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/query/collector/pointcollector/hkpclosestcdpointcollector.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/box/hkpboxshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/capsule/hkpcapsuleshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/convextranslate/hkpconvextranslateshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/sphere/hkpsphereshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkprigidbody.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/motion/hkpmotion.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/phantom/hkpcachingshapephantom.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/phantom/hkpphantomoverlaplistener.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/hkpworld.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/utilities/charactercontrol/characterproxy/hkpcharacterproxy.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/utilities/charactercontrol/characterproxy/hkpcharacterproxycinfo.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/utilities/charactercontrol/characterproxy/hkpcharacterproxylistener.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/utilities/charactercontrol/hkpcharactercontrol.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/utilities/charactercontrol/statemachine/util/hkpcharactermovementutil.h"
#include "volition/rfg/code/camera/camera_shake.h"
#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/rfg/code/geom/character_instance.h"
#include "volition/rfg/code/gamewide/tweak_table.h"
#include "volition/rfg/code/geom/vanim.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/havok/havok_collision_events.h"
#include "volition/rfg/code/havok/havok_raycast.h"
#include "volition/rfg/code/havok/havok_ragdoll.h"
#include "volition/rfg/code/havok/hk_custom_character_proxy.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_game/multi_game_object_backpack.h"
#include "volition/rfg/code/objects/gameplay_properties.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_collide.h"
#include "volition/rfg/code/objects/humans/human_combat.h"
#include "volition/rfg/code/objects/humans/human_combat_turret.h"
#include "volition/rfg/code/objects/humans/human_info.h"
#include "volition/rfg/code/objects/humans/human_move.h"
#include "volition/rfg/code/objects/humans/human_ragdoll.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/level_object_properties.h"
#include "volition/rfg/code/objects/movers/object_mover.h"
#include "volition/rfg/code/objects/object.h"
#include "volition/rfg/code/objects/object.inl"
#include "volition/rfg/code/objects/object_item.h"
#include "volition/rfg/code/objects/object_type.h"
#include "volition/rfg/code/objects/projectile.h"
#include "volition/rfg/code/scripting/action_script/global_event_data.h"
#include "volition/rfg/code/scripting/action_script/global_event_manager.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/vlib/code/math/bbox.h"
#include "volition/vlib/code/math/ix.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/os/frametime.h"
#include "volition/vlib/code/vlib_core/vlib_assert.h"

#include <cmath>

// ---- file-static globals
static const float VEHICLE_VS_MOVERS_MASS_MULTIPLIER = 5.0f; // 0x012F8EE0 (also the 5 m/s cap of a human push)
static bool Allow_vi_pass_thru = true; // 0x0165FB59
// base_array<hkpPhantom*> Havok_add_phantom_list (0x0165FF04) is used here too; placed in havok.cpp.
static float Character_collision_push_away_mass; // 0x0262271C
// The original places Player_controller_shapes in player.cpp; player.cpp is not ported yet (P4), so it is defined
// here.
hkpShape* Player_controller_shapes[8]; // 0x0264272C
static float Character_collision_push_away_rvec_factor; // 0x0264274C
static float Character_collision_stumble_min; // 0x02642F88
static float Character_collision_push_away_dim; // 0x02666050
static float Character_collision_ragdoll_min; // 0x0276D2B0
static hkpShape* Npc_controller_shapes[8]; // 0x0276D2BC
static float Character_collision_push_away_impulse_factor; // 0x0276D2DC
// hkpRigidBody* Hk_rigid_bodies[1048576] (0x0276E980) is used here too; placed in havok.cpp.
static hkpConvexShape* Dead_shape; // 0x02B82204
// cs_wrapper Havok_add_phantom_wrapper (0x02B95260) is used here too; placed in havok.cpp.
// The collision tweaks: 0 until tweak_table.xtbl is loaded (tweak_table_init)
static tweak_table_item Tweak_table_item_Character_collision_stumble_min("Character_collision_stumble_min", &Character_collision_stumble_min); // 0x02B951E4
static tweak_table_item Tweak_table_item_Character_collision_ragdoll_min("Character_collision_ragdoll_min", &Character_collision_ragdoll_min); // 0x02B83C2C
static tweak_table_item Tweak_table_item_Character_collision_push_away_mass("Character_collision_push_away_mass", &Character_collision_push_away_mass); // 0x02B95160
static tweak_table_item Tweak_table_item_Character_collision_push_away_dim("Character_collision_push_away_dim", &Character_collision_push_away_dim); // 0x02B822C8
static tweak_table_item Tweak_table_item_Character_collision_push_away_impulse_factor("Character_collision_push_away_impulse_factor", &Character_collision_push_away_impulse_factor); // 0x02B952E4
static tweak_table_item Tweak_table_item_Character_collision_push_away_rvec_factor("Character_collision_push_away_rvec_factor", &Character_collision_push_away_rvec_factor); // 0x02B83D50
// Only read here (update_state_for_integration): .data 1 at 0x01660C05 and .bss 0 at 0x02B82208.
static bool allow_bigstep = true; // 0x01660C05
static bool Player_uses_npc_collision = false; // 0x02B82208
extern float hkConvexShapeDefaultRadius; // hkpconvexshape.cpp (Havok)
// 0x0165FE00: the phantoms' overlap listener (the constructor adds it to every controller phantom)
static phantom_overlap_listener Phantom_overlap_listener;

// hkpWorldObject property keys of the game (havok.h): the object of a world object, the ragdoll of a body, and the
// keys the controller sets while swapping its phantom's shape
static const unsigned int CC_PROPERTY_OBJECT = 0;
static const unsigned int CC_PROPERTY_PUSHED = 9; // set on a body whose push was capped
static const unsigned int CC_PROPERTY_SHAPE_SWAP = 10; // while the phantom's shape is replaced
static const unsigned int CC_PROPERTY_SHAPE_CHANGED = 11; // cleared again by post_integrate

// ---- internal functions
static bool not_tied_up(const human* const hp); // 0x00814BF0 (:136)
static hkpShape* create_spherical_controller_shape(float elevation, float sphere_radius); // 0x00814ED0 (:1134)
static hkpCapsuleShape* create_capsule_controller_shape(float total_height, float radius); // 0x00814FB0 (:1159)
static void initialize_player_controller_shapes(); // 0x00815080 (:1216)
static void initialize_npc_controller_shapes(); // 0x00815190 (:1254)
static controller_shape_mode get_desired_controller_shape(const object* objp); // 0x00815280 (:1283)

// Havok's SSE dot3 (MULPS, then (y + x) + z)
static inline float hk_cc_dot3(const hkVector4f& a, const hkVector4f& b)
{
    const float x = a(0) * b(0);
    const float y = a(1) * b(1);
    const float z = a(2) * b(2);
    return (y + x) + z;
}

// The 23-bit accurate inverse square root of Havok's SSE math (one Newton step) with the SET_ZERO mask:
// (3 - (r * x) * r) * (r * 0.5), zero where x <= 0
static inline __m128 hk_cc_inv_sqrt23(__m128 x)
{
    const __m128 r = _mm_rsqrt_ps(x);
    const __m128 t = _mm_mul_ps(_mm_mul_ps(r, x), r);
    const __m128 inv = _mm_mul_ps(_mm_sub_ps(_mm_set1_ps(3.0f), t), _mm_mul_ps(r, _mm_set1_ps(0.5f)));
    return _mm_andnot_ps(_mm_cmple_ps(x, _mm_setzero_ps()), inv);
}

// The 23-bit accurate length of the xyz part, zero for a zero vector: ((3 - r x r) * (r * 0.5)) * x masked
static inline float hk_cc_length3(const hkVector4f& v)
{
    const __m128 sq = _mm_mul_ps(v.m_quad, v.m_quad);
    const __m128 sum = _mm_add_ps(_mm_add_ps(_mm_shuffle_ps(sq, sq, 0x55), _mm_shuffle_ps(sq, sq, 0x00)), _mm_shuffle_ps(sq, sq, 0xAA));
    const __m128 r = _mm_rsqrt_ps(sum);
    const __m128 t = _mm_mul_ps(_mm_mul_ps(r, sum), r);
    const __m128 len = _mm_mul_ps(_mm_mul_ps(_mm_sub_ps(_mm_set1_ps(3.0f), t), _mm_mul_ps(r, _mm_set1_ps(0.5f))), sum);
    return _mm_cvtss_f32(_mm_andnot_ps(_mm_cmple_ps(sum, _mm_setzero_ps()), len));
}

static inline bool hk_cc_has_property(const hkpWorldObject* wo, unsigned int key)
{
    return wo->hasProperty(key, hkpWorldObject::MULTI_THREADING_CHECKS_ENABLE);
}

// ---- header inlines (global_event_data.h)
// 0x0080D2F0 (global_event_data.h:277)
mover_human_special_collision_event::mover_human_special_collision_event()
    : event_data(EVENT_MOVER_HUMAN_SPECIAL_COLLISION)
{
}

// ---- MyCharacterListener

// 0x00814C20 (:154)
void* MyCharacterListener::operator new(unsigned int nbytes)
{
    return hkMemoryRouter::getInstance().heap().blockAlloc(nbytes);
}

// 0x00814DB0 (:154): the object's own memory size when it has one
void MyCharacterListener::operator delete(void* p, unsigned int nbytes)
{
    const unsigned short memSize = (unsigned short)((hkReferencedObject*)p)->getMemorySizeAndFlags();
    if (memSize != 0xFFFF)
    {
        nbytes = memSize;
    }
    hkMemoryRouter::getInstance().heap().blockFree(p, nbytes);
}

// 0x00814C40 (:156): the slope is kept as its cosine (Havok's inline cos)
MyCharacterListener::MyCharacterListener(const hkVector4f& up, const float maxSlope, unsigned int obj_handle)
{
    m_up = up;
    m_obj_handle = obj_handle;
    m_maxSlope = hkMath::cos(maxSlope);
}

// 0x00814D90
MyCharacterListener::~MyCharacterListener()
{
}

// 0x00821E70 (:182): two humans' proxies touch. Ragdolls the other human when this one should, else a human on a
// ladder or leaving a vehicle pushes the other one's controller (pushed_vec, used by its next
// update_state_for_integration), else the other one flinches when this one pushes it.
void MyCharacterListener::characterInteractionCallback(hkpCharacterProxy* proxy, hkpCharacterProxy* other_proxy, const hkContactPoint& contact)
{
    hkpShapePhantom* phantom = proxy->getShapePhantom();
    if (phantom == nullptr || !hk_cc_has_property(phantom, CC_PROPERTY_OBJECT))
    {
        return;
    }
    human* hp1 = (human*)(hkUlong)phantom->getProperty(CC_PROPERTY_OBJECT, hkpWorldObject::MULTI_THREADING_CHECKS_ENABLE).m_data;
    if (hp1 == nullptr || hp1->get_obj_type() != OT_HUMAN || hp1->is_killed())
    {
        return;
    }
    human* hp2 = (human*)object_from_world_obj(other_proxy->getShapePhantom());
    if (hp2 == nullptr || hp2->get_obj_type() != OT_HUMAN || hp2->is_killed())
    {
        return;
    }
    human_collide_human(hp1, hp2);
    bool exiting = hp1->cur_state == HS_VEHICLE_EXITING;
    if (Allow_vi_pass_thru && (hp2->cur_state == HS_VEHICLE_ENTERING || hp2->cur_state == HS_PLAYER_PATHING))
    {
        exiting = false;
    }
    if (human_should_ragdoll_other_human(hp1, hp2))
    {
        human_ragdoll_start(hp2, false, 0xFFFFFFFF);
        hp2->play_voice_line(VOICE_LINE_FLINCH, nullptr, nullptr, AUDIOLIB_CUE_PRIORITY_VOICE_AMBIENT, false, true);
        return;
    }
    if (hp1->ladder_handle != 0xFFFFFFFF || exiting)
    {
        if (!hp2->rdd.is_loaded())
        {
            return;
        }
        vector diff;
        diff.x = hp2->pos.x - hp1->pos.x;
        diff.y = hp2->pos.y - hp1->pos.y;
        diff.z = hp2->pos.z - hp1->pos.z;
        character_controller* cc = hp2->rdd.get_data()->controller;
        cc->pushed_vec = diff.copy_flatten_normalize();
        const hkVector4f& v = proxy->getLinearVelocity();
        vector vel;
        vel.x = v(0);
        vel.y = v(1);
        vel.z = v(2);
        float speed = vel.mag_and_normalize_safe();
        const vector dir = vel.copy_normalize();
        cc = hp2->rdd.get_data()->controller;
        // (d.y * p.y + p.x * d.x) + d.z * p.z in double
        const float approach = (float)(((double)dir.y * (double)cc->pushed_vec.y + (double)cc->pushed_vec.x * (double)dir.x) + (double)dir.z * (double)cc->pushed_vec.z);
        if (approach > 0.0f)
        {
            cc->flags.is_pushed = 1;
            if ((float)(5.0 - (double)speed) < 0.0f) // 0x4014000000000000
            {
                speed = VEHICLE_VS_MOVERS_MASS_MULTIPLIER;
            }
            cc = hp2->rdd.get_data()->controller;
            const float scale = approach * speed;
            cc->pushed_vec.x = cc->pushed_vec.x * scale;
            cc->pushed_vec.y = cc->pushed_vec.y * scale;
            cc->pushed_vec.z = cc->pushed_vec.z * scale;
        }
        return;
    }
    if (human_should_push_other_human(hp1, hp2) && hp2->rdd.is_loaded())
    {
        vector diff;
        diff.x = hp2->pos.x - hp1->pos.x;
        diff.y = hp2->pos.y - hp1->pos.y;
        diff.z = hp2->pos.z - hp1->pos.z;
        human_combat_play_flinch(hp2, ZERO_VECTOR, diff, HIT_TORSO, 20); // 20% large flinch
    }
}

// 0x00827BB0 (:258): the proxy hits a body. The impulse the proxy applies (output.m_objectImpulse, at the body's
// centre for the sideways push) depends on what the body is: falling debris and movers damage, flinch or ragdoll the
// human; light debris is pushed aside; vehicles and other bodies get a push against the human's velocity; items
// with special_flags.is_ammo_box and static movers are not pushed at all.
void MyCharacterListener::objectInteractionCallback(hkpCharacterProxy* proxy, const hkpCharacterObjectInteractionEvent& input, hkpCharacterObjectInteractionResult& output)
{
    if (input.m_body == nullptr)
    {
        return;
    }
    object* objp = object_from_world_obj(input.m_body);
    if (objp == nullptr)
    {
        return;
    }
    human* hp = nullptr;
    if (m_obj_handle != 0xFFFFFFFF)
    {
        object* o = World.object_from_handle(m_obj_handle);
        if (o != nullptr && o->get_obj_type() == OT_HUMAN)
        {
            hp = (human*)o;
        }
    }
    bool stuck_contact = false;
    if (hp == nullptr)
    {
        return;
    }

    vector normal(input.m_normal(0), input.m_normal(1), input.m_normal(2));
    vector position(input.m_position(0), input.m_position(1), input.m_position(2));
    bool push_body = true;
    hkVector4f pointVel;
    input.m_body->getPointVelocity(input.m_position, pointVel);
    const float pointSpeed = hk_cc_length3(pointVel);
    const float bodySpeed = hk_cc_length3(input.m_body->getLinearVelocity());
    // min(pointSpeed, bodySpeed) through an x87 difference rounded to float
    const float speed = !((float)(pointSpeed - bodySpeed) >= 0.0f) ? pointSpeed : bodySpeed;
    vector dir;
    vector contactVel;
    if (0.01f < speed) // 0x3C23D70A
    {
        // the point velocity's direction (double divisions) scaled by the speed
        dir.x = (float)((double)pointVel(0) / (double)pointSpeed);
        dir.y = (float)((double)pointVel(1) / (double)pointSpeed);
        dir.z = (float)((double)pointVel(2) / (double)pointSpeed);
        contactVel.x = dir.x * speed;
        contactVel.y = dir.y * speed;
        contactVel.z = dir.z * speed;
    }
    else
    {
        dir = normal;
        contactVel.zero();
    }

    switch (objp->get_obj_type())
    {
        case OT_ITEM:
        {
            if (objp->get_sub_type() == 8) // projectiles
            {
                return;
            }
            if (objp->get_sub_type() != 7) // not a weapon
            {
                if (((item*)objp)->special_flags.is_ammo_box == 0)
                {
                    return;
                }
                output.m_objectImpulse.setZero(); // DAT_012F9230
                return;
            }
            weapon* wp = World.object_cast<weapon>(objp);
            if (!wp->w_info->flags.non_inventory || wp->item_flags.item_held != 0)
            {
                return;
            }
            wp->collide(nullptr, World.object_from_handle(m_obj_handle));
            return;
        }
        case OT_VEHICLE:
        {
            float relSpeed = speed;
            if (hp->actual_velocity.mag_squared() > 0.010000001f) // 0x3C23D70B
            {
                const vector a = contactVel.copy_normalize_safe();
                const vector b = hp->actual_velocity.copy_normalize_safe();
                const float d = (float)(((double)b.y * (double)a.y + (double)b.x * (double)a.x) + (double)a.z * (double)b.z);
                // -d * actual_velocity_mag * 0.5 + speed, as one double chain
                relSpeed = (float)((double)(-d) * (double)hp->actual_velocity_mag * 0.5 + (double)speed);
                if ((float)(0.0 - (double)relSpeed) >= 0.0f)
                {
                    relSpeed = 0.0f;
                }
            }
            human_collide_vehicle(position, normal, relSpeed, m_obj_handle, objp->get_handle(), 0xFFFFFFFF, false);
            push_body = false;
            break;
        }
        case OT_MOVER:
        case OT_DEBRIS:
        case OT_TURRET:
        {
            if (objp->get_obj_type() == OT_MOVER && ((object_mover*)objp)->m_level_obj_properties->m_flags.m_generate_human_collision_event)
            {
                mover_human_special_collision_event ev;
                ev.mover_handle = objp->get_handle();
                ev.human_handle = hp->get_handle();
                gem_post_event(ev);
            }
            if (hp->hflags.invulnerable_to_debris == 0)
            {
                const double height = (double)position.y - (double)hp->pos.y;
                object_hit_location hitLoc = HIT_GENERAL;
                if (height > (double)1.65f) // 0x3FFA666660000000
                {
                    hitLoc = HIT_HEAD;
                    normal = dir;
                }
                if ((double)0.9f > height) // 0x3FECCCCCC0000000
                {
                    hitLoc = (normal * hp->orient.rvec > 0.0f) ? HIT_LEFT_LEG : HIT_RIGHT_LEG;
                }
                float damage = human_collide_mover_damage(hp, objp, position, contactVel, normal);
                if ((hp->ragdoll_state == HUMAN_RAGDOLL_STATE_NONE || hp->ragdoll_state == HUMAN_RAGDOLL_STATE_ACTIVE_ON_IMPACT) &&
                    !character_controller_should_push_away(objp, false) && hp->orient.uvec.y > 0.98f && hp->rdd.is_loaded())
                {
                    // the capsule's segment (shortened in the jump shape) against a sphere around the contact
                    vector bottom;
                    vector top;
                    float radius;
                    character_controller* cc = hp->rdd.get_data()->controller;
                    cc->get_capsule_data(bottom, top, radius);
                    if (hp->rdd.get_data()->controller->m_controller_shape_mode == CSM_JUMP)
                    {
                        top.y = top.y - 0.3f;
                    }
                    vector mid;
                    mid.x = (top.x + bottom.x) * 0.5f;
                    mid.y = (bottom.y + top.y) * 0.5f;
                    mid.z = (bottom.z + top.z) * 0.5f;
                    radius = radius * 0.6f;
                    vector p1(top.x + hp->pos.x, hp->pos.y + top.y, top.z + hp->pos.z);
                    vector p0(mid.x + hp->pos.x, mid.y + hp->pos.y, mid.z + hp->pos.z);
                    vector hitPoint;
                    if (ix_linesegment_sphere(hitPoint, p0, p1, position, radius))
                    {
                        if (hp->object_filter_handle == 0xFFFFFFFF)
                        {
                            stuck_contact = true;
                            if (hp->rdd.get_data()->controller->human_is_potentially_stuck > 5 && objp->get_obj_type() != OT_ZONE)
                            {
                                hp->hflags.filter_handle_valid = 1;
                                hp->object_filter_handle = objp->get_handle();
                            }
                        }
                        vector q1(top.x + hp->pos.x, hp->pos.y + top.y, top.z + hp->pos.z);
                        vector q0(mid.x + hp->pos.x, mid.y + hp->pos.y, mid.z + hp->pos.z);
                        if (ix_linesegment_sphere(hitPoint, q0, q1, position, radius * 0.4f))
                        {
                            damage = Character_collision_ragdoll_min;
                        }
                    }
                }
                if (damage > 0.0f)
                {
                    if ((!Multiplayer || object_is_local_player(hp)) && not_tied_up(hp))
                    {
                        const bool down = human_is_dead(hp) || human_is_ragdolled(hp);
                        bool ragdoll;
                        if (human_is_immune_to_ragdoll(hp))
                        {
                            ragdoll = down;
                        }
                        else
                        {
                            ragdoll = down || damage > Character_collision_ragdoll_min ||
                                      ((int)hitLoc > HIT_HEAD && damage > Character_collision_stumble_min);
                        }
                        if (ragdoll)
                        {
                            human_ragdoll_start(hp, false, 0xFFFFFFFF);
                            havok_ragdoll_apply_point_impulse(hp->ragdoll_idx, dir, position);
                            havok_ragdoll_apply_velocity(hp->ragdoll_idx, dir, true);
                            push_body = false;
                        }
                        else
                        {
                            const unsigned int large = (damage > Character_collision_stumble_min || (double)0.9f > (double)position.y - (double)hp->pos.y) ? 100 : 0;
                            human_combat_play_flinch(hp, position, normal, hitLoc, large);
                        }
                    }
                    if (hp->get_handle() == Rfg_camera.target_handle)
                    {
                        const char* name = (normal * hp->orient.rvec > 0.0f) ? "shard_impact_left" : "shard_impact_right";
                        camera_shake_play(name, (float)((double)damage / (double)(float)hp->max_hit_points), false);
                    }
                }
            }
            if (objp->get_obj_type() == OT_MOVER &&
                (!havok_body_is_movable(objp->havok_handle) || !((object_mover*)objp)->m_level_obj_properties->m_flags.m_movable_by_humans))
            {
                // static movers are not pushed (returns before the stuck count below)
                output.m_objectImpulse.setZero();
                return;
            }
            break;
        }
        default:
            break;
    }

    if (!(input.m_projectedVelocity > 0.0f))
    {
        output.m_objectImpulse.setZero();
    }
    else if (hp->hflags.invulnerable_to_debris != 0)
    {
        // the body keeps still: the negated linear velocity (w kept)
        const hkVector4f& lv = input.m_body->getLinearVelocity();
        output.m_objectImpulse.set(-lv(0), -lv(1), -lv(2), lv(3));
    }
    else
    {
        vector vel = hp->velocity;
        // x87: the squared speed in double against the float magnitude
        const double speedD = (double)speed;
        if (speedD * speedD > (double)vel.mag_squared() && push_body)
        {
            // stop the body along the normal
            const float mass = input.m_body->getMass();
            const float a = mass * normal.x;
            const float b = mass * normal.y;
            const float c = normal.z * mass;
            const float s = -(float)(((double)b * (double)contactVel.y + (double)a * (double)contactVel.x) + (double)c * (double)contactVel.z);
            output.m_objectImpulse.set(s * normal.x, s * normal.y, s * normal.z, 0.0f);
            if (objp->get_obj_type() == OT_HUMAN)
            {
                const int ragdoll = ((human*)objp)->ragdoll_idx;
                if (ragdoll != -1)
                {
                    vector rv;
                    havok_ragdoll_get_vel(ragdoll, rv);
                    const float dx = -(float)((double)rv.x - (double)vel.x);
                    const float dy = -(float)((double)rv.y - (double)vel.y);
                    const float dz = -(float)((double)rv.z - (double)vel.z);
                    const float s2 = (float)(((double)dy * (double)normal.y + (double)dx * (double)normal.x) + (double)dz * (double)normal.z);
                    dir.x = s2 * normal.x;
                    dir.y = s2 * normal.y;
                    dir.z = s2 * normal.z;
                    rv = dir;
                    havok_ragdoll_set_velocity(ragdoll, rv);
                }
            }
        }
        else if (objp->get_obj_type() == OT_VEHICLE ||
                 (!character_controller_should_push_away(objp, false) &&
                  (!object_is_local_player(hp) || (!human_combat_is_manning_turret(hp) && hp->cur_state != HS_LADDER)) &&
                  hp->hflags.pushes_debris_scripted == 0))
        {
            if (0.0f < normal.y)
            {
                output.m_objectImpulse.setZero(); // DAT_012F9230
            }
            else
            {
                // the human's velocity relative to the contact, at 10 (vehicles) or 75 (others) per unit
                vel.x = (float)((double)vel.x - (double)contactVel.x);
                vel.y = (float)((double)vel.y - (double)contactVel.y);
                vel.z = (float)((double)vel.z - (double)contactVel.z);
                if (!vel.is_null())
                {
                    vel.normalize();
                    const float scale = (objp->get_obj_type() == OT_VEHICLE) ? 10.0f : 75.0f; // 0x4024000000000000 / 0x4052C00000000000
                    vel.x = vel.x * scale;
                    vel.y = vel.y * scale;
                    vel.z = vel.z * scale;
                }
                hkVector4f imp;
                imp.set(vel.x, vel.y, vel.z, 0.0f);
                output.m_objectImpulse = imp;
                if (input.m_objectMassInv > 1.0f)
                {
                    const float m = (float)(1.0 / (double)input.m_objectMassInv);
                    imp.m_quad = _mm_mul_ps(_mm_set1_ps(m), imp.m_quad);
                    output.m_objectImpulse = imp;
                }
                // capped to the event's impulse limit
                const float len = hk_cc_length3(imp);
                const float limit = (float)fabs((double)input.m_objectImpulse);
                if ((double)len > (double)limit)
                {
                    const float k = (float)((double)limit / (double)len);
                    output.m_objectImpulse.m_quad = _mm_mul_ps(_mm_set1_ps(k), imp.m_quad);
                    hkpRigidBody* body = input.m_body;
                    if (!hk_cc_has_property(body, CC_PROPERTY_PUSHED))
                    {
                        havok_set_property(body, CC_PROPERTY_PUSHED, (void*)1);
                    }
                }
            }
        }
        else
        {
            // pushed aside: the human's velocity along the flat direction to the contact plus a side push along the
            // human's right vector, scaled by the body's mass
            vector side(position.x - hp->pos.x, 0.0f, position.z - hp->pos.z);
            side.x = (float)((double)position.x - (double)hp->pos.x);
            side.z = (float)((double)position.z - (double)hp->pos.z);
            side.y = 0.0f;
            side.normalize_safe();
            float d = (float)(((double)vel.y * (double)side.y + (double)side.x * (double)vel.x) + (double)side.z * (double)vel.z);
            if ((float)(0.0 - (double)d) >= 0.0f)
            {
                d = 0.0f;
            }
            vector p(d * side.x, d * side.y, side.z * d);
            vector r;
            if (side * hp->orient.rvec > 0.0f)
            {
                r = hp->orient.rvec;
            }
            else
            {
                r.x = -hp->orient.rvec.x;
                r.y = -hp->orient.rvec.y;
                r.z = -hp->orient.rvec.z;
            }
            p.x = r.x * Character_collision_push_away_rvec_factor + p.x;
            p.y = r.y * Character_collision_push_away_rvec_factor + p.y;
            p.z = r.z * Character_collision_push_away_rvec_factor + p.z;
            // x87: the mass times the factor rounded to float
            const float m = (float)((double)objp->get_mass() * (double)Character_collision_push_away_impulse_factor);
            float ix = m * p.x;
            float iy = p.y * m;
            float iz = p.z * m;
            if (objp->get_obj_type() == OT_HUMAN)
            {
                ix = ix * 0.5f;
                iy = iy * 0.5f;
                iz = iz * 0.5f;
            }
            output.m_objectImpulse.set(ix, iy, iz, 0.0f);
            output.m_impulsePosition = input.m_body->getCenterOfMassInWorld();
        }
    }
    if (stuck_contact && hp->rdd.is_loaded())
    {
        hp->rdd.get_data()->controller->human_is_potentially_stuck++;
    }
}

// 0x0082C600 (:585): before the solve. Resets and finds the human's ground object (the steepest-up contact other
// than itself), zeroes the plane velocities of items, movers, debris and ragdolls, flags external forces, records
// the cover normal of a player pressed against a still wall facing the camera, and sets the frictions: none on
// walkable planes; on too steep planes a player gets extra up friction (a push up the slope) and an NPC an extra
// vertical plane.
void MyCharacterListener::processConstraintsCallback(const hkpCharacterProxy* proxy, const hkArray<hkpRootCdPoint, hkContainerHeapAllocator>& manifold, hkSimplexSolverInput& input)
{
    human* hp = nullptr;
    character_controller* cc = nullptr;
    if (m_obj_handle != 0xFFFFFFFF)
    {
        object* o = World.object_from_handle(m_obj_handle);
        if (o != nullptr && o->get_obj_type() == OT_HUMAN)
        {
            hp = (human*)o;
            if (!hp->rdd.is_loaded())
            {
                return;
            }
            cc = hp->rdd.get_data()->controller;
        }
    }
    if (hp != nullptr)
    {
        hp->ground_obj_contact_pos = ZERO_VECTOR;
        hp->ground_obj_handle = 0xFFFFFFFF;
        hp->ground_obj_normal = 0.0f;
        hp->ground_obj_alt_body_idx = -1;
        hp->ground_obj_shape_key = -1;
    }
    const int numInitial = input.m_numConstraints;
    float bestNormalY = 0.7f; // 0x3F333333
    for (int i = 0; i < input.m_numConstraints; i++)
    {
        // the constraints added below (and the user planes) have no manifold point
        if (i >= manifold.getSize())
        {
            continue;
        }
        const hkpRootCdPoint& point = manifold[i];
        hkSurfaceConstraintInfo& sci = input.m_constraints[i];
        const hkpCollidable* collidable = point.m_rootCollidableB;
        hkpRigidBody* body = hkpGetRigidBody(collidable);
        const float normalY = point.m_contact.getSeparatingNormal()(1);
        const float upDot = hk_cc_dot3(sci.m_plane, m_up);
        object* other = nullptr;
        if (hp == nullptr)
        {
            if (body != nullptr)
            {
                other = object_from_world_obj(body);
                if (other != nullptr && other->get_obj_type() > OT_HUMAN && (other->get_obj_type() <= OT_MOVER || other->get_obj_type() == OT_DEBRIS))
                {
                    sci.m_velocity.setZero();
                }
            }
        }
        else if (body == nullptr)
        {
            // a phantom: another human's controller stops the plane
            object* owner = (object*)havok_get_property((const hkpWorldObject*)collidable->getOwner(), CC_PROPERTY_OBJECT, nullptr, true);
            if (owner != nullptr && owner->get_obj_type() == OT_HUMAN)
            {
                sci.m_velocity.setZero();
            }
        }
        else
        {
            other = object_from_world_obj(body);
            if (other != nullptr)
            {
                if (hp->get_handle() != other->get_handle() && (double)normalY > (double)bestNormalY)
                {
                    bestNormalY = normalY;
                    hp->ground_obj_contact_pos.x = point.m_contact.getPosition()(0);
                    hp->ground_obj_contact_pos.y = point.m_contact.getPosition()(1);
                    hp->ground_obj_contact_pos.z = point.m_contact.getPosition()(2);
                    hp->ground_obj_handle = other->get_handle();
                    hp->ground_obj_alt_body_idx = (int)other->get_alt_hk_body_handle_index(havok_rigid_body_get_handle(body));
                    hp->ground_obj_shape_key = (int)point.m_shapeKeyB;
                    hp->ground_obj_normal = normalY;
                }
                switch (other->get_obj_type())
                {
                    case OT_HUMAN:
                        if (((human*)other)->ragdoll_idx >= 0)
                        {
                            sci.m_velocity.setZero();
                        }
                        break;
                    case OT_ITEM:
                    case OT_MOVER:
                    case OT_DEBRIS:
                        sci.m_velocity.setZero();
                        break;
                    default:
                        break;
                }
            }
        }
        // Original bug kept: hp is dereferenced here (and below) without a check; the listener's human exists
        // whenever a proxy integrates.
        if (hk_cc_dot3(sci.m_velocity, sci.m_velocity) > 0.25f)
        {
            hp->hflags.external_force_applied = 1;
        }
        if (body != nullptr && 0.35f > upDot) // 0x3EB33333
        {
            object* wall = object_from_world_obj(body);
            const bool skip = wall != nullptr && (wall->get_obj_type() == OT_ZONE || (wall->get_obj_type() == OT_MOVER && (((object_mover*)wall)->gameplay_props->chunk_flags & 0x10000) != 0));
            if (!skip && (double)0.1f > (double)hk_cc_dot3(body->getLinearVelocity(), body->getLinearVelocity()) && object_is_player(hp))
            {
                vector cn(sci.m_plane(0), sci.m_plane(1), sci.m_plane(2));
                if (Rfg_camera.real_orient.fvec.copy_flatten_normalize() * cn < -0.4f) // 0xBFD99999A0000000
                {
                    player* pp = (player*)hp;
                    pp->cover_collision_normal = cn;
                    pp->cover_collision_normal.flatten_normalize_safe();
                    hp->hflags.check_for_cover = 1;
                    // Original bug kept in spirit: the binary reads the wall's sub_type even when no object was found
                    // (a null read); here a missing object counts as not a vehicle.
                    pp->pflags.cover_against_vehicle = (wall != nullptr && wall->get_sub_type() == 4) ? 1 : 0;
                }
            }
        }
        sci.m_dynamicFriction = 1.0f;
        bool clearAll = !(upDot > 0.01f); // 0x3C23D70A
        if (!clearAll)
        {
            float maxSlope = m_maxSlope;
            if (hp->hflags.allow_steep_slopes != 0 || (other != nullptr && other->get_sub_type() == 0))
            {
                maxSlope = 0.38f; // 0x3EC28F5C
            }
            if (!((double)maxSlope > (double)upDot) || hp->cur_state == HS_JUMP || hp->cur_state == HS_FALL || cc->flags.big_step)
            {
                clearAll = true;
            }
            else
            {
                const bool playerCollision = hp->get_sub_type() == 3 && !Player_uses_npc_collision;
                if (playerCollision)
                {
                    // (1 - upDot - (near a big step ? 0.45 : 0.25)) * 15 in double
                    const double margin = cc->flags.near_big_step ? (double)0.45f : 0.25; // 0x3FDCCCCCC0000000 / 0x3FD0000000000000
                    sci.m_extraUpStaticFriction = (float)(((1.0 - (double)upDot) - margin) * 15.0);
                }
                else
                {
                    sci.m_extraUpStaticFriction = 0.0f;
                    // a vertical plane along the slope (the plane minus its up part), unless the user planes are full
                    const int num = input.m_numConstraints;
                    if (num == cc->m_characterProxy->m_userPlanes + numInitial || i >= manifold.getSize())
                    {
                        continue;
                    }
                    hkSurfaceConstraintInfo& extra = input.m_constraints[num];
                    input.m_numConstraints = num + 1;
                    extra = sci;
                    hkVector4f plane;
                    plane.m_quad = _mm_add_ps(_mm_mul_ps(m_up.m_quad, _mm_set1_ps(-upDot)), extra.m_plane.m_quad);
                    const __m128 sq = _mm_mul_ps(plane.m_quad, plane.m_quad);
                    const __m128 sum = _mm_add_ps(_mm_add_ps(_mm_shuffle_ps(sq, sq, 0x55), _mm_shuffle_ps(sq, sq, 0x00)), _mm_shuffle_ps(sq, sq, 0xAA));
                    extra.m_plane = plane;
                    extra.m_plane.m_quad = _mm_mul_ps(hk_cc_inv_sqrt23(sum), plane.m_quad);
                }
                if (0.0f > sci.m_extraUpStaticFriction || (cc->flags.flat_surface_ahead || cc->flags.big_step))
                {
                    clearAll = true;
                }
            }
        }
        if (clearAll)
        {
            sci.m_extraUpStaticFriction = 0.0f;
        }
        sci.m_staticFriction = 0.0f;
        sci.m_extraDownStaticFriction = 0.0f;
    }
}

// ---- phantom_overlap_listener

// 0x00814E40
phantom_overlap_listener::phantom_overlap_listener()
{
}

// 0x0080D680
phantom_overlap_listener::~phantom_overlap_listener()
{
}

// 0x00814E50 (:950)
void phantom_overlap_listener::collidableRemovedCallback(const hkpCollidableRemovedEvent& remove_event)
{
}

// 0x00828F80 (:954): a controller phantom starts overlapping a body. Projectiles hit the human (a collision event at
// the hit location) unless they miss or are an ally's fresh shot; either way they do not stay in the overlap set. A
// keyframed mover the humans may move becomes movable.
void phantom_overlap_listener::collidableAddedCallback(const hkpCollidableAddedEvent& added_event)
{
    const hkpCollidable* collidable = added_event.m_collidable;
    if (collidable->m_broadPhaseHandle.m_type != hkpWorldObject::BROAD_PHASE_ENTITY)
    {
        return;
    }
    hkpWorldObject* wp = (hkpWorldObject*)collidable->getOwner();
    if (wp == nullptr)
    {
        return;
    }
    object* objp = object_from_world_obj(wp);
    if (objp == nullptr)
    {
        return;
    }
    if (objp->get_obj_type() == OT_MOVER)
    {
        object_mover* mp = (object_mover*)objp;
        if (((hkpRigidBody*)wp)->getMotionType() == hkpMotion::MOTION_KEYFRAMED && (mp->mover_flags & 4) == 0 &&
            !havok_body_is_movable(mp->havok_handle) && mp->m_level_obj_properties->m_flags.m_movable_by_humans)
        {
            havok_body_set_movable(objp->havok_handle, true);
        }
        return;
    }
    if (objp->get_obj_type() != OT_ITEM || objp->get_sub_type() != 8)
    {
        return;
    }
    human* hp = (human*)object_from_world_obj(added_event.m_phantom);
    if (hp == nullptr || hp->get_obj_type() != OT_HUMAN || objp->havok_handle == 0xFFFFFFFF || objp->obj_flags.destroyed)
    {
        added_event.m_collidableAccept = HK_COLLIDABLE_REJECT;
        return;
    }
    projectile* pp = World.object_cast<projectile>(objp);
    player* owner = World.object_from_handle<player>(pp->get_owner_handle());
    // x87: the autoaim radius plus the bounding radius rounded to float
    const float radius = (float)((double)weapons_get_autoaim_radius(owner) + (double)pp->get_bounding_radius());
    vector dir;
    havok_body_get_linear_velocity(pp->havok_handle, dir);
    dir.normalize_safe();
    int bone = -1;
    // the binary reads age_timer.time_since() directly (get_age inlined)
    if (owner == nullptr || Multiplayer || !human_teams_are_allies(owner, hp) || pp->get_age() >= 130)
    {
        vector intersection;
        vector hitNormal;
        if (human_get_hit_location(hp, objp->pos, dir, intersection, radius, &bone, &hitNormal) != HIT_NONE)
        {
            collision_event::collision_info info;
            info.m_collision_point = intersection;
            info.m_collision_normal = hitNormal;
            info.m_shapekey_b = (unsigned int)bone;
            havok_add_collision_event(pp, hp, info);
            return;
        }
    }
    added_event.m_collidableAccept = HK_COLLIDABLE_REJECT;
}

// ---- controller shapes

// 0x00814BF0 (:136): NPCs can be tied up. LTCG: hp in EAX.
static bool not_tied_up(const human* const hp)
{
    if (hp != nullptr && hp->get_obj_type() == OT_HUMAN && hp->get_sub_type() == 2)
    {
        return !((const npc*)hp)->npc_flags.tied_up;
    }
    return true;
}

// 0x00814ED0 (:1134): a sphere, raised by elevation + 0.06 (double literal) when elevated
static hkpShape* create_spherical_controller_shape(float elevation, float sphere_radius)
{
    hkpSphereShape* sphere = new hkpSphereShape(sphere_radius);
    if (elevation == 0.0f)
    {
        return sphere;
    }
    hkVector4f translation;
    translation.set(0.0f, (float)((double)elevation + (double)0.06f), 0.0f, 0.0f); // 0x3FAEB851E0000000
    hkpConvexTranslateShape* shape = new hkpConvexTranslateShape(sphere, translation, hkpShapeContainer::REFERENCE_POLICY_INCREMENT);
    sphere->removeReference();
    return shape;
}

// 0x00814FB0 (:1159): a vertical capsule standing on the origin. LTCG: total_height on the stack, radius in XMM1.
static hkpCapsuleShape* create_capsule_controller_shape(float total_height, float radius)
{
    hkVector4f vertexA;
    vertexA.set(0.0f, (float)((double)radius + (double)0.06f), 0.0f, 0.0f);
    hkVector4f vertexB;
    vertexB.set(0.0f, total_height - radius, 0.0f, 0.0f);
    return new hkpCapsuleShape(vertexA, vertexB, radius);
}

// 0x00815080 (:1216): the order follows controller_shape_mode
static void initialize_player_controller_shapes()
{
    Player_controller_shapes[CSM_STAND] = create_capsule_controller_shape(1.8f, 0.4f);
    Player_controller_shapes[CSM_CROUCH] = create_capsule_controller_shape(1.2f, 0.4f);
    Player_controller_shapes[CSM_PRONE] = create_spherical_controller_shape(0.4f, 0.4f);
    Player_controller_shapes[CSM_STAND_NARROW] = create_capsule_controller_shape(1.8f, 0.4f);
    Player_controller_shapes[CSM_CROUCH_NARROW] = create_capsule_controller_shape(1.2f, 0.4f);
    Player_controller_shapes[CSM_VEHICLE] = create_spherical_controller_shape(0.4f, 0.3f);
    Player_controller_shapes[CSM_DIVE_THROUGH_HOLE] = create_capsule_controller_shape(0.85f, 0.4f);
    Player_controller_shapes[CSM_JUMP] = create_capsule_controller_shape(1.5f, 0.4f);
}

// 0x00815190 (:1254): NPCs have no dive shape
static void initialize_npc_controller_shapes()
{
    Npc_controller_shapes[CSM_STAND] = create_capsule_controller_shape(1.8f, 0.45f);
    Npc_controller_shapes[CSM_CROUCH] = create_capsule_controller_shape(1.325f, 0.45f);
    Npc_controller_shapes[CSM_PRONE] = create_spherical_controller_shape(0.45f, 0.45f);
    Npc_controller_shapes[CSM_STAND_NARROW] = create_capsule_controller_shape(1.8f, 0.3f);
    Npc_controller_shapes[CSM_CROUCH_NARROW] = create_capsule_controller_shape(1.325f, 0.3f);
    Npc_controller_shapes[CSM_VEHICLE] = create_spherical_controller_shape(0.45f, 0.33749998f);
    Npc_controller_shapes[CSM_JUMP] = create_capsule_controller_shape(1.5f, 0.45f);
}

// 0x00815800 (:2486): the dead shape is a box translated up by its half height (resized by update_dead_shape)
void character_controller_system_init()
{
    initialize_player_controller_shapes();
    initialize_npc_controller_shapes();
    hkVector4f halfExtents;
    halfExtents.set(0.33f, 1.0f, 0.33f, 0.0f); // DAT_012F9220
    hkpBoxShape* box = new hkpBoxShape(halfExtents, hkConvexShapeDefaultRadius);
    hkVector4f translation;
    translation.set(0.0f, 1.0f, 0.0f, 0.0f);
    Dead_shape = new hkpConvexTranslateShape(box, translation, hkpShapeContainer::REFERENCE_POLICY_INCREMENT);
    box->removeReference();
}

// 0x00815900 (:2510)
void character_controller_system_shutdown()
{
    for (int i = 0; i < NUM_STANDARD_CONTROLLER_SHAPE_MODES; i++)
    {
        if (Player_controller_shapes[i] != nullptr)
        {
            Player_controller_shapes[i]->removeReference();
            Player_controller_shapes[i] = nullptr;
        }
        if (Npc_controller_shapes[i] != nullptr)
        {
            Npc_controller_shapes[i]->removeReference();
            Npc_controller_shapes[i] = nullptr;
        }
    }
    // Original: the dead shape is released but the pointer is not cleared.
    Dead_shape->removeReference();
}

// 0x00815280 (:1283): the shape for the human's state and stance. LTCG: objp in EAX.
static controller_shape_mode get_desired_controller_shape(const object* objp)
{
    if (objp == nullptr || objp->get_obj_type() != OT_HUMAN)
    {
        return CSM_STAND;
    }
    const human* hp = (const human*)objp;
    if ((hp->is_killed() && hp->ragdoll_state != HUMAN_RAGDOLL_STATE_ACTIVE_ON_IMPACT) ||
        (human_is_ragdolled(hp) && hp->ragdoll_state != (human_ragdoll_state)2 && hp->ragdoll_state != HUMAN_RAGDOLL_STATE_ACTIVE_ON_IMPACT))
    {
        return CSM_DEAD;
    }
    const human_states state = hp->cur_state;
    if (state == HS_COVER)
    {
        if (hp->cur_stance == HST_CROUCH || hp->hflags.crouch_cover)
        {
            return CSM_CROUCH_NARROW;
        }
        return CSM_STAND_NARROW;
    }
    if (hp->hflags.dive_capsule)
    {
        return CSM_DIVE_THROUGH_HOLE;
    }
    if (state == HS_JUMP && hp->char_inst->vp->default_group_index != ANIMATION_GROUP_2H_CARRY)
    {
        const bool jetpack = hp->get_sub_type() == 3 && ((const player*)hp)->pflags.jetpack_started;
        // Original: the controller is read through rdd.get_data() without a loaded check
        if (!hp->rdd.get_data()->controller->flags.is_supported && !jetpack)
        {
            return CSM_JUMP;
        }
    }
    if (hp->movement_submode == HMSM_FALL_FREE)
    {
        return CSM_PRONE;
    }
    if (state == HS_VEHICLE_DRIVING || state == HS_VEHICLE_SITTING)
    {
        return (hp->char_inst->get_animation_group() != ANIMATION_GROUP_TURRET_MACHINEGUN) ? CSM_VEHICLE : CSM_STAND;
    }
    switch (hp->cur_stance)
    {
        case HST_STAND:
            return CSM_STAND;
        case HST_CROUCH:
            return CSM_CROUCH;
        case HST_PRONE:
            return CSM_PRONE;
        default:
            VLIB_CRITICAL_FAILURE(); // line 1357 of the original
            return CSM_STAND;
    }
}

// ---- character_controller

// 0x00837950 (:1522): the phantom starts with the standing shape, carries the object as its property and is added
// in the next phantom batch; the listener's slope is 50 degrees
character_controller::character_controller(unsigned int object_handle, unsigned int collide_filter)
{
    sliding_buildup.invalidate();
    stepdown_ts.invalidate();
    supported_ts.invalidate();
    object* objp = World.object_from_handle(object_handle);
    hkpShape** shapes = object_is_player(object_handle) ? Player_controller_shapes : Npc_controller_shapes;
    for (int i = 0; i < NUM_STANDARD_CONTROLLER_SHAPE_MODES; i++)
    {
        m_controller_shape[i] = shapes[i];
        if (shapes[i] != nullptr)
        {
            shapes[i]->addReference();
        }
    }
    Dead_shape->addReference();
    m_controller_shape[CSM_DEAD] = Dead_shape;

    hkTransformf transform;
    transform.getRotation().getColumn(0).set(objp->orient.rvec.x, objp->orient.rvec.y, objp->orient.rvec.z, 0.0f);
    transform.getRotation().getColumn(1).set(objp->orient.uvec.x, objp->orient.uvec.y, objp->orient.uvec.z, 0.0f);
    transform.getRotation().getColumn(2).set(objp->orient.fvec.x, objp->orient.fvec.y, objp->orient.fvec.z, 0.0f);
    transform.getTranslation().set(objp->pos.x, objp->pos.y, objp->pos.z, 0.0f);
    hkpCachingShapePhantom* phantom = new hkpCachingShapePhantom(m_controller_shape[CSM_STAND], transform, collide_filter);
    m_phantom = phantom;
    m_controller_shape_mode = CSM_STAND;
    phantom->addPhantomOverlapListener(&Phantom_overlap_listener);
    if (!hk_cc_has_property(m_phantom, CC_PROPERTY_OBJECT))
    {
        m_phantom->addProperty(CC_PROPERTY_OBJECT, hkSimplePropertyValue((void*)objp));
    }
    havok_add_phantom(m_phantom, true);
    flags.phantom_added_to_world = 0;

    hkpCharacterProxyCinfo cpci;
    cpci.m_position = transform.getTranslation();
    cpci.m_shapePhantom = m_phantom;
    m_characterProxy = new hk_custom_character_proxy(cpci);
    *(unsigned int*)&flags = 0;
    sliding_buildup.set(5000);
    m_saved_collision_filter = collide_filter;
    hkVector4f up;
    up.set(0.0f, 1.0f, 0.0f, 0.0f); // DAT_012F91F0
    m_listener = new MyCharacterListener(up, 0.87266463f, object_handle); // 0x3F5F65FE, 50 degrees
    m_characterProxy->addCharacterProxyListener(m_listener);
    m_currentAngle = 0.0f;
    m_frame_time_left = 0.0f;
}

// 0x008291B0 (:1606)
character_controller::~character_controller()
{
    m_listener->removeReference();
    m_listener = nullptr;
    m_phantom->removeReference();
    havok_remove_phantom(m_phantom, true);
    m_phantom = nullptr;
    m_characterProxy->removeReference();
    m_characterProxy = nullptr;
    for (int i = 0; i < NUM_CONTROLLER_SHAPE_MODES; i++)
    {
        if (m_controller_shape[i] != nullptr)
        {
            m_controller_shape[i]->removeReference();
            m_controller_shape[i] = nullptr;
        }
    }
}

// 0x00814E90 (:1110)
void character_controller::init_dead_shape()
{
    Dead_shape->addReference();
    m_controller_shape[CSM_DEAD] = Dead_shape;
}

// 0x00815660 (:1477): while dead, the box follows the corpse's bounding box (centred on its middle)
void character_controller::update_dead_shape(const vector& bmin, const vector& bmax, const matrix& orient)
{
    const hkpShape* shape = m_phantom->getCollidable()->getShape();
    if (shape != m_controller_shape[CSM_DEAD])
    {
        return;
    }
    hkpConvexTranslateShape* translate = (hkpConvexTranslateShape*)shape;
    hkpBoxShape* box = (hkpBoxShape*)translate->getChildShape();
    vector wmin;
    vector wmax;
    bbox_transform(wmin, wmax, orient, ZERO_VECTOR, bmin, bmax);
    const float cx = (wmax.x + wmin.x) * 0.5f;
    const float cy = (wmin.y + wmax.y) * 0.5f;
    const float cz = (wmin.z + wmax.z) * 0.5f;
    hkVector4f halfExtents;
    halfExtents.set(wmax.x - cx, wmax.y - cy, wmax.z - cz, 0.0f);
    box->setHalfExtents(halfExtents);
    translate->m_translation.set(cx, cy, cz, 0.0f);
}

// 0x00834920 (:1400)
void character_controller::set_shape(controller_shape_mode which, bool forced)
{
    if (which < NUM_CONTROLLER_SHAPE_MODES && m_controller_shape[which] != nullptr && swapPhantomShape(m_controller_shape[which], forced))
    {
        m_controller_shape_mode = which;
    }
}

// 0x00834950 (:1442): leaving the jump shape for the standing one makes the human crouch first (its stand-up test
// then decides), and the swap is forced
void character_controller::update_collision_shape_mode(const object* objp)
{
    const controller_shape_mode desired = get_desired_controller_shape(objp);
    if (desired != m_controller_shape_mode)
    {
        bool swap = true;
        if (m_controller_shape_mode == CSM_JUMP && desired == CSM_STAND)
        {
            // Original: hp is null for a non-human object here (never, the jump shape is a human's)
            human* hp = (objp != nullptr && objp->get_obj_type() == OT_HUMAN) ? (human*)objp : nullptr;
            hp->cur_stance = HST_CROUCH;
            hp->crouch_to_stand_test_ts.set(0);
            human_move_stop_crouching(hp);
            if (hp->cur_stance == HST_CROUCH && hp->cur_movement_state == HMS_SPRINT)
            {
                hp->cur_movement_state = HMS_RUN;
            }
        }
        else if ((int)desired < 0 || (int)desired > CSM_DEAD)
        {
            swap = false;
        }
        if (swap && m_controller_shape[desired] != nullptr && swapPhantomShape(m_controller_shape[desired], true))
        {
            m_controller_shape_mode = desired;
        }
    }
    if (m_controller_shape_mode == CSM_DEAD && objp != nullptr && objp->get_obj_type() == OT_HUMAN)
    {
        const human* hp = (const human*)objp;
        update_dead_shape(hp->approx_local_bmin, hp->approx_local_bmax, objp->orient);
    }
}

// 0x008314C0 (:2374): the phantom's shape is replaced at the proxy's position. Unless forced, the new shape must not
// start more than 0.01 inside anything (a zero length cast from the position), else the old shape is restored.
bool character_controller::swapPhantomShape(hkpShape* newShape, bool forced)
{
    const hkpShape* oldShape = m_phantom->getCollidable()->getShape();
    if (newShape == oldShape)
    {
        return true;
    }
    if (m_phantom == nullptr)
    {
        return false;
    }
    if (!hk_cc_has_property(m_phantom, CC_PROPERTY_SHAPE_CHANGED))
    {
        havok_set_property(m_phantom, CC_PROPERTY_SHAPE_CHANGED, (void*)1);
    }
    if (!hk_cc_has_property(m_phantom, CC_PROPERTY_SHAPE_SWAP))
    {
        m_phantom->addProperty(CC_PROPERTY_SHAPE_SWAP, hkSimplePropertyValue(1)); // value 1 (low dword 1, high 0)
    }
    const hkVector4f position = m_characterProxy->getPosition();
    m_phantom->setShape(newShape);
    havok_remove_property(m_phantom, CC_PROPERTY_SHAPE_SWAP);
    bool ok = true;
    if (!forced)
    {
        hkpClosestCdPointCollector castCollector;
        hkpAllCdPointCollector startCollector;
        hkpLinearCastInput lci;
        lci.m_to = position;
        lci.m_maxExtraPenetration = 0.01f; // 0x3C23D70A
        lci.m_startPointTolerance = 0.01f;
        m_phantom->setPositionAndLinearCast(position, lci, castCollector, &startCollector);
        const hkArray<hkpRootCdPoint, hkContainerHeapAllocator>& hits = startCollector.getHits();
        for (int i = 0; i < hits.getSize(); i++)
        {
            if (hits[i].m_contact.getDistance() < -0.01f)
            {
                m_phantom->setShape(oldShape);
                ok = false;
                break;
            }
        }
    }
    m_characterProxy->setPosition(position);
    return ok;
}

// 0x0080D6A0 (:1434)
bool character_controller::using_dead_shape()
{
    return m_controller_shape_mode == CSM_DEAD;
}

// 0x008153D0 (:1368)
void character_controller::set_pos(const vector& pos)
{
    hkVector4f p;
    p.set(pos.x, pos.y, pos.z, 0.0f);
    m_characterProxy->setPosition(p);
}

// 0x00815430 (:1375)
void character_controller::get_pos(vector& pos)
{
    const hkVector4f& p = m_characterProxy->getPosition();
    pos.x = p(0);
    pos.y = p(1);
    pos.z = p(2);
}

// 0x00815460 (:1381)
void character_controller::get_velocity(vector& vel)
{
    const hkVector4f& v = m_characterProxy->getLinearVelocity();
    vel.x = v(0);
    vel.y = v(1);
    vel.z = v(2);
}

// 0x008154C0 (:1388)
void character_controller::set_velocity(const vector& vel)
{
    hkVector4f v;
    v.set(vel.x, vel.y, vel.z, 0.0f);
    m_characterProxy->setLinearVelocity(v);
}

// 0x00815520 (:1409): the capsule's segment of the current shape mode (local to the proxy). A translated shape
// (spheres, the dead box) gives its centre twice and its child's radius; other shapes the proxy position and 0.
void character_controller::get_capsule_data(vector& bottom, vector& top, float& radius)
{
    const hkpShape* shape = m_controller_shape[m_controller_shape_mode];
    if (shape->getType() == hkcdShapeType::CONVEX_TRANSLATE)
    {
        const hkpConvexTranslateShape* translate = (const hkpConvexTranslateShape*)shape;
        hkVector4f centre;
        translate->getCentre(centre);
        bottom.x = centre(0);
        bottom.y = centre(1);
        bottom.z = centre(2);
        top = bottom;
        radius = translate->getChildShape()->getRadius();
        return;
    }
    if (shape->getType() == hkcdShapeType::CAPSULE)
    {
        const hkpCapsuleShape* capsule = (const hkpCapsuleShape*)shape;
        bottom.x = capsule->m_vertexA(0);
        bottom.y = capsule->m_vertexA(1);
        bottom.z = capsule->m_vertexA(2);
        top.x = capsule->m_vertexB(0);
        top.y = capsule->m_vertexB(1);
        top.z = capsule->m_vertexB(2);
        radius = capsule->getRadius();
        return;
    }
    const hkVector4f& p = m_characterProxy->getPosition();
    bottom.x = p(0);
    bottom.y = p(1);
    bottom.z = p(2);
    top = bottom;
    radius = 0.0f;
}

// 0x00837D90 (:1705): the proxy velocity for this frame. With a support check (or a fast upward velocity), the
// ground decides: too steep slopes are unsupported for players (and slide), climbing faster than last frame is
// clamped, a fast upward motion off the surface counts as unsupported. Players and big-step humans probe for a step
// ahead (a ray down in front of the capsule) and step up onto it. calculateMovement then blends the desired velocity
// into the surface frame; slopes reduce the gain, too steep ones make the human slide down. Gravity, stepping down
// and the jump velocity complete the vertical part.
void character_controller::update_state_for_integration(human* hp, vector& desired_vel, float jump_height, float delta_time, bool check_support, bool use_velocity, bool use_velocity_ignore_y)
{
    hkVector4f up;
    up.set(0.0f, 1.0f, 0.0f, 0.0f); // DAT_012F91F0
    hkVector4f down;
    down.set(0.0f, -1.0f, 0.0f, 0.0f); // DAT_012F9280
    hkVector4f forward;
    forward.set(0.0f, 0.0f, 1.0f, 0.0f); // DAT_012F9270
    hkVector4f zero;
    zero.setZero();
    flags.check_ragdoll = 0;
    m_currentAngle = 0.0f;
    hkVector4f vel = m_characterProxy->getLinearVelocity();
    // getPosition() is called here; its result is not used.

    player* pp = nullptr;
    if (hp != nullptr && hp->get_obj_type() == OT_HUMAN && hp->get_sub_type() == 3)
    {
        pp = (player*)hp;
        if (pp->backpack != nullptr && pp->backpack->get_backpack_abilities()->jump_boost)
        {
            flags.big_step = 0;
        }
    }

    hkVector4f normal;
    hkVector4f surfaceVelocity;
    int state;
    float upDot;
    if (!check_support && !(use_velocity && use_velocity_ignore_y) && !(vel(1) > 9.0f))
    {
        normal = up;
        surfaceVelocity.setZero();
        state = hkpSurfaceInfo::SUPPORTED;
        upDot = 1.0f;
    }
    else
    {
        hkpSurfaceInfo ground;
        m_characterProxy->checkSupport(down, ground);
        state = ground.m_supportedState;
        if (object_is_player(hp) && ((player*)hp)->pflags.jetpack_started && 3.0f > hp->falling_vel_mag)
        {
            state = hkpSurfaceInfo::UNSUPPORTED;
        }
        normal = ground.m_surfaceNormal;
        surfaceVelocity = ground.m_surfaceVelocity;
        upDot = hk_cc_dot3(normal, up);

        bool steepChecks = !(flags.flat_surface_ahead || flags.near_big_step);
        if (state != hkpSurfaceInfo::UNSUPPORTED && 0.7f > upDot && steepChecks && object_is_player(hp) && !Player_uses_npc_collision &&
            hp->hflags.allow_steep_slopes == 0)
        {
            // too steep for a player: below ~57 degrees off up it is a fall, and the desired velocity loses its part
            // into the slope
            if (0.55f > upDot) // 0x3F0CCCCD
            {
                flags.check_ragdoll = 1;
                state = hkpSurfaceInfo::UNSUPPORTED;
            }
            vector n(normal(0), 0.0f, normal(2));
            n.normalize_safe();
            const vector dv = desired_vel;
            const float d = (float)(((double)dv.y * (double)n.y + (double)n.x * (double)dv.x) + (double)dv.z * (double)n.z);
            if (0.0f > d)
            {
                desired_vel.x = dv.x - n.x * d;
                desired_vel.y = dv.y - d * n.y;
                desired_vel.z = dv.z - n.z * d;
            }
        }
        if (steepChecks && object_is_player(hp) && !Player_uses_npc_collision && hp->hflags.allow_steep_slopes == 0)
        {
            // climbing faster than last frame (not jumping) is clamped to last frame's vertical speed
            const float lfv = last_frame_vertical;
            if (vel(1) > lfv && !flags.is_supported && jump_height == 0.0f && !pp->pflags.jetpack_started && (double)lfv + 5.0 > (double)vel(1))
            {
                vel(1) = lfv;
            }
        }
        last_frame_vertical = vel(1);
        if (vel(1) > 1.5f)
        {
            if (state == hkpSurfaceInfo::UNSUPPORTED)
            {
                upDot = 1.0f;
            }
            else
            {
                // moving up off the surface
                vector v(vel(0), vel(1), vel(2));
                v.normalize_safe();
                const float d = (float)(((double)v.y * (double)normal(1) + (double)v.x * (double)normal(0)) + (double)v.z * (double)normal(2));
                if ((double)fabsf(d) > (double)0.05f || (0.7f > upDot && last_frame_vertical > 0.0f)) // 0x3FA99999A0000000
                {
                    upDot = 1.0f;
                    state = hkpSurfaceInfo::UNSUPPORTED;
                }
            }
        }
        else if (state == hkpSurfaceInfo::UNSUPPORTED)
        {
            upDot = 1.0f;
        }
        else if (hp->ground_obj_normal == 0.0f)
        {
            flags.check_ragdoll = 1;
        }
    }
    // HK_TIMER_BEGIN("update_character_state") (monitors are not reproduced)

    bool slidingPrev = flags.sliding;
    const bool supportedPrev = flags.is_supported;
    const bool bigPrev = flags.big_step;
    flags.is_supported = (state != hkpSurfaceInfo::UNSUPPORTED) ? 1 : 0;
    hkVector4f newVel = vel;
    const bool useBigSteps = (hp->get_sub_type() == 3 && !Player_uses_npc_collision) || hp->hflags.use_bigsteps != 0;
    float gain;
    if (state == hkpSurfaceInfo::UNSUPPORTED && !use_velocity && !flags.big_step)
    {
        gain = 0.05f; // 0x3D4CCCCD
        slidingPrev = false;
    }
    else
    {
        gain = 1.0f;
        if (!supportedPrev)
        {
            supported_ts.set(200);
        }
    }

    hkpCharacterMovementUtil::hkpMovementUtilInput mi;
    mi.m_forward = forward;
    mi.m_up = up;
    mi.m_surfaceNormal = (state != hkpSurfaceInfo::UNSUPPORTED) ? normal : up;
    mi.m_gain = gain;
    mi.m_desiredVelocitySpace = hkpCharacterMovementUtil::hkpMovementUtilInput::SURFACE_SPACE;
    mi.m_maxVelocityDelta = 100.0f;
    mi.m_surfaceVelocity = surfaceVelocity;

    if (use_velocity)
    {
        newVel.set(desired_vel.x, desired_vel.y, desired_vel.z, 0.0f);
    }
    else
    {
        mi.m_currentVelocity = vel;
        mi.m_desiredVelocity.set(-desired_vel.z, desired_vel.x, desired_vel.y, 0.0f);
        // the slope's down direction d = n x (n x UP), each component in double
        const float nx = mi.m_surfaceNormal(0);
        const float ny = mi.m_surfaceNormal(1);
        const float nz = mi.m_surfaceNormal(2);
        const float cx = (float)((double)ny * (double)UP_VECTOR.z - (double)nz * (double)UP_VECTOR.y);
        const float cy = (float)((double)nz * (double)UP_VECTOR.x - (double)nx * (double)UP_VECTOR.z);
        const float cz = (float)((double)nx * (double)UP_VECTOR.y - (double)ny * (double)UP_VECTOR.x);
        flags.flat_surface_ahead = 0;
        flags.near_big_step = 0;
        vector slopeDown;
        slopeDown.x = (float)((double)ny * (double)cz - (double)nz * (double)cy);
        slopeDown.y = (float)((double)cx * (double)nz - (double)nx * (double)cz);
        slopeDown.z = (float)((double)nx * (double)cy - (double)cx * (double)ny);

        bool attempt = false;
        if (!useBigSteps)
        {
            attempt = true;
            flags.big_step = 0;
            flags.no_surface_ahead = 0;
        }
        else if (state == hkpSurfaceInfo::UNSUPPORTED && !bigPrev)
        {
            flags.big_step = 0;
            flags.no_surface_ahead = 0;
        }
        else
        {
            // the step probe: a ray down from 0.56 ahead of the next position, 0.7 up, 2.2 long
            raycast_input in;
            in.m_filter_info = COLLIDE_RAY_DEFAULT;
            raycast_output out;
            if (desired_vel.is_null())
            {
                flags.big_step = 0;
            }
            else
            {
                const vector dir = desired_vel.copy_normalize_safe();
                vector start;
                start.x = (desired_vel.x * Frametime + hp->pos.x) + dir.x * 0.56f; // 0x3FE1EB8520000000
                start.y = (hp->pos.y + desired_vel.y * Frametime) + dir.y * 0.56f;
                start.z = (hp->pos.z + desired_vel.z * Frametime) + dir.z * 0.56f;
                in.start = start;
                in.start.y = start.y + 0.7f; // 0x3FE6666660000000
                in.end = in.start;
                in.end.y = in.start.y - 2.2f; // 0x40019999A0000000
                in.m_filter_info = COLLIDE_RAY_PLAYER_COVER;
                if (!havok_raycast(in, out) || !allow_bigstep)
                {
                    flags.big_step = 0;
                    flags.no_surface_ahead = 1;
                }
                else
                {
                    flags.no_surface_ahead = 0;
                    const raycast_output::hit_info& hit = out.m_hits[0];
                    const float horiz2 = (float)((double)desired_vel.z * (double)desired_vel.z + (double)desired_vel.x * (double)desired_vel.x);
                    bool probe = true;
                    if (hit.hit_normal.y > 0.72f || flags.big_step) // 0x3F3851EC
                    {
                        flags.near_big_step = 1;
                    }
                    else
                    {
                        probe = false;
                    }
                    if (probe && !(horiz2 > 1.2f) && !flags.big_step) // 0x3F99999A
                    {
                        probe = false;
                    }
                    if (probe && !flags.big_step)
                    {
                        // only from a supported, slow, not rising human
                        const float av2 = (float)((double)hp->actual_velocity.x * (double)hp->actual_velocity.x + (double)hp->actual_velocity.z * (double)hp->actual_velocity.z);
                        if (state == hkpSurfaceInfo::UNSUPPORTED || !(0.3f > hp->actual_velocity.y) || !(12.0f > av2)) // 0x3E99999A
                        {
                            probe = false;
                        }
                    }
                    if (probe)
                    {
                        float stepY = 0.0f;
                        bool found = false;
                        if (hit.hit_pos.y > hp->pos.y && hit.hit_normal.y > 0.7f)
                        {
                            stepY = hit.hit_pos.y;
                            found = true;
                        }
                        else if (hp->get_sub_type() == 3)
                        {
                            // players: a 0.25 sphere cast 0.4 higher
                            in.radius = 0.25f;
                            in.start.y = in.start.y + 0.4f; // 0x3FD99999A0000000
                            in.m_filter_info = COLLIDE_RAY_PLAYER_COVER;
                            raycast_output out2;
                            if (havok_spherecast(in, out2))
                            {
                                const raycast_output::hit_info& h2 = out2.m_hits[0];
                                if (h2.hit_pos.y > hp->pos.y && h2.hit_normal.y > 0.6f && (double)in.start.y - (double)0.1f > (double)h2.hit_center_pos.y) // 0x3F19999A
                                {
                                    found = true;
                                    stepY = h2.hit_pos.y;
                                }
                            }
                        }
                        if (!found)
                        {
                            flags.big_step = 0;
                        }
                        else
                        {
                            // nothing between the step and a point half a metre back: step up
                            in.end = in.start;
                            in.start.x = in.start.x - dir.x * 0.5f;
                            in.start.y = in.start.y - dir.y * 0.5f;
                            in.start.z = in.start.z - dir.z * 0.5f;
                            raycast_output out3;
                            if (havok_raycast(in, out3))
                            {
                                flags.big_step = 0;
                            }
                            else
                            {
                                flags.flat_surface_ahead = 1;
                                attempt = true;
                                if (flags.big_step || (double)0.52f > (double)in.start.y - (double)stepY) // 0x3FE0A3D700000000
                                {
                                    // x87: |desired| * 10 * Frametime + z, rounded once
                                    const double lift = ((double)desired_vel.mag() * 10.0) * (double)Frametime;
                                    mi.m_desiredVelocity(2) = (float)(lift + (double)mi.m_desiredVelocity(2));
                                    flags.big_step = 1;
                                }
                            }
                        }
                    }
                }
            }
        }

        // slopes cut the gain when walking up them
        const float n0 = mi.m_surfaceNormal(0);
        const float n1 = mi.m_surfaceNormal(1);
        const float n2 = mi.m_surfaceNormal(2);
        if (!attempt && (double)0.8f > (double)upDot && !desired_vel.is_null()) // 0x3FE99999A0000000
        {
            const float d = (float)(((double)desired_vel.y * (double)n1 + (double)desired_vel.x * (double)n0) + (double)desired_vel.z * (double)n2);
            if (0.0f > d)
            {
                const float g = (float)(1.0 - ((double)0.8f - (double)upDot) * 10.0);
                mi.m_gain = ((float)(0.0 - (double)g) < 0.0f) ? g : 0.0f;
            }
        }
        bool slidingFlag;
        if (useBigSteps && !supportedPrev && state != hkpSurfaceInfo::UNSUPPORTED && (double)0.8f > (double)upDot)
        {
            slidingFlag = true;
            slidingPrev = true;
        }
        else
        {
            slidingFlag = slidingPrev;
        }
        if (slidingFlag && sliding_buildup.elapsed())
        {
            mi.m_gain = 0.05f;
        }
        hkpCharacterMovementUtil::calculateMovement(mi, newVel);
        flags.sliding = 0;

        if (!attempt && (0.72f > upDot || ((double)0.8f > (double)upDot && !desired_vel.is_null() && useBigSteps)))
        {
            // sliding down: blend the solution with the current velocity and add gravity along the slope
            float s = (float)(((double)0.8f - (double)upDot) * 10.0);
            if ((float)((double)s - 1.0) >= 0.0f)
            {
                s = 1.0f;
            }
            const float k = (float)(1.0 - (double)s);
            const float ax = k * newVel(0);
            const float ay = k * newVel(1);
            const float az = k * newVel(2);
            float bx = s * vel(0);
            float by = s * vel(1);
            float bz = s * vel(2);
            if (s == 1.0f)
            {
                // fully sliding: the current velocity loses its part into the plane
                const float d = (float)(((double)by * (double)n1 + (double)n0 * (double)bx) + (double)bz * (double)n2);
                bx = bx - d * n0;
                by = by - d * n1;
                bz = bz - n2 * d;
            }
            const float qx = (slopeDown.x * delta_time) * 9.8f * s; // 0x40239999A0000000
            const float qy = (slopeDown.y * delta_time) * 9.8f * s;
            const float qz = (slopeDown.z * delta_time) * 9.8f * s;
            vector out;
            out.x = (qx + ax) + bx;
            out.y = (qy + ay) + by;
            out.z = (qz + az) + bz;
            if (0.7f > upDot || (slidingFlag && out.mag() > 3.0f && -1.0f > out.y))
            {
                flags.sliding = 1;
            }
            newVel.set(out.x, out.y, out.z, 0.0f);
        }
        else if (slidingFlag)
        {
            // still sliding while faster than desired, or fast and off the desired direction
            const vector v(vel(0), vel(1), vel(2));
            const vector ndv = desired_vel.copy_normalize_safe();
            const vector nv = v.copy_normalize_safe();
            const float d = (float)(((double)nv.y * (double)ndv.y + (double)nv.x * (double)ndv.x) + (double)nv.z * (double)ndv.z);
            const float vmag = (float)sqrt((double)(float)(((double)v.y * (double)v.y + (double)v.x * (double)v.x) + (double)v.z * (double)v.z));
            const float dmag2 = (float)(((double)desired_vel.y * (double)desired_vel.y + (double)desired_vel.x * (double)desired_vel.x) + (double)desired_vel.z * (double)desired_vel.z);
            float m = (float)((double)(float)sqrt((double)dmag2) + 0.5);
            if ((float)(1.0 - (double)m) >= 0.0f)
            {
                m = 1.0f;
            }
            if ((double)vmag > (double)m || (vmag > 1.0f && (double)0.8f > (double)d))
            {
                flags.sliding = 1;
            }
        }
        if (!flags.sliding)
        {
            sliding_buildup.set(500);
        }
    }

    if (state == hkpSurfaceInfo::UNSUPPORTED && (!use_velocity || use_velocity_ignore_y) && !flags.big_step)
    {
        // in the air the vertical part stays the current one
        hkVector4f t;
        t.m_quad = _mm_add_ps(_mm_mul_ps(_mm_sub_ps(zero.m_quad, _mm_set1_ps(hk_cc_dot3(newVel, up))), up.m_quad), newVel.m_quad);
        newVel.m_quad = _mm_add_ps(t.m_quad, _mm_mul_ps(_mm_set1_ps(hk_cc_dot3(vel, up)), up.m_quad));
    }
    if (!flags.big_step && ((supportedPrev && state == hkpSurfaceInfo::UNSUPPORTED && !slidingPrev) || bigPrev) && vel(1) > 0.0f && 9.0f > vel(1))
    {
        // just left the ground (or a big step) going up: the current vertical speed is removed
        newVel.m_quad = _mm_add_ps(_mm_mul_ps(_mm_sub_ps(zero.m_quad, _mm_set1_ps(hk_cc_dot3(vel, up))), up.m_quad), newVel.m_quad);
    }
    if (hp->cur_state != HS_JUMP && hp->cur_state != HS_FALL && !flags.big_step && !use_velocity)
    {
        if (supportedPrev && state == hkpSurfaceInfo::UNSUPPORTED && supported_ts.elapsed() && !flags.no_surface_ahead)
        {
            stepdown_ts.set(200);
        }
        if (hp->cur_state == HS_STANDARD && newVel(1) > 0.0f && 9.0f > newVel(1))
        {
            newVel(1) = newVel(1) * 0.5f;
        }
        if (stepdown_ts.valid() && !stepdown_ts.elapsed())
        {
            // stepping down: pulled down at 9 m/s per second
            newVel(1) = (float)((double)newVel(1) - (double)delta_time * 9.0);
            if (state != hkpSurfaceInfo::UNSUPPORTED)
            {
                supported_ts.set(stepdown_ts.time_until());
            }
        }
    }
    if (state == hkpSurfaceInfo::UNSUPPORTED)
    {
        supported_ts.invalidate();
    }
    if (!flags.disable_gravity && state == hkpSurfaceInfo::UNSUPPORTED && !flags.big_step)
    {
        newVel.m_quad = _mm_add_ps(_mm_mul_ps(_mm_set1_ps(delta_time), Havok_world->m_gravity.m_quad), newVel.m_quad);
    }
    if (jump_height > 0.0f)
    {
        // the take-off speed sqrt(2 g h) against gravity's direction
        const __m128 g = Havok_world->m_gravity.m_quad;
        const __m128 sq = _mm_mul_ps(g, g);
        const __m128 sum = _mm_add_ps(_mm_add_ps(_mm_shuffle_ps(sq, sq, 0x55), _mm_shuffle_ps(sq, sq, 0x00)), _mm_shuffle_ps(sq, sq, 0xAA));
        const __m128 inv = hk_cc_inv_sqrt23(sum);
        const float gLen = _mm_cvtss_f32(_mm_mul_ps(sum, inv));
        const float v2 = (float)((double)gLen * (double)fabsf(jump_height) * 2.0);
        const float s = -_mm_cvtss_f32(_mm_sqrt_ss(_mm_set_ss(v2)));
        hkVector4f j;
        j.m_quad = _mm_mul_ps(_mm_mul_ps(inv, g), _mm_set1_ps(s));
        float jy = j(1);
        if (-3.0f > newVel(1))
        {
            const float t = (float)((double)jy - 2.0);
            if ((float)((double)jy - (double)t) >= 0.0f)
            {
                jy = t;
            }
        }
        flags.is_supported = 0;
        newVel(1) = jy;
        last_frame_vertical = jy;
    }
    // HK_TIMER_END()
    if (flags.phantom_added_to_world)
    {
        update_collision_shape_mode(hp);
    }
    if (flags.is_pushed)
    {
        flags.is_pushed = 0;
        hkVector4f push;
        push.set(pushed_vec.x, pushed_vec.y, pushed_vec.z, 0.0f);
        newVel.m_quad = _mm_add_ps(push.m_quad, newVel.m_quad);
    }
    m_characterProxy->setLinearVelocity(newVel);
    m_characterProxy->m_refreshManifoldInCheckSupport = false;
}

// 0x00829260 (:2224): the phantom's broadphase box grows by this step's travel and the proxy's distances
void character_controller::update_broadphase_for_integration(float time_step)
{
    hkAabb aabb;
    m_phantom->calcAabb(aabb);
    const float speed = hk_cc_length3(m_characterProxy->getLinearVelocity());
    const float grow = speed * time_step + m_characterProxy->m_keepDistance + m_characterProxy->m_keepContactTolerance;
    const __m128 g = _mm_set1_ps(grow);
    aabb.m_min.m_quad = _mm_sub_ps(aabb.m_min.m_quad, g);
    aabb.m_max.m_quad = _mm_add_ps(aabb.m_max.m_quad, g);
    m_phantom->updateBroadPhase(aabb);
}

// 0x008314A0 (:2262)
void character_controller::mt_integrate_begin(float delta_time)
{
    m_characterProxy->mt_integrate_start_initial_casts(delta_time);
}

// 0x008397A0 (:2272): the proxy's process step with the world's gravity (inlined in the binary)
void character_controller::mt_integrate_process_casts(float time_step)
{
    hkStepInfo stepInfo;
    stepInfo.m_deltaTime = time_step;
    stepInfo.m_invDeltaTime = 1.0f / time_step;
    m_characterProxy->mt_integrate_process_casts(stepInfo, Havok_world->m_gravity);
}

// 0x00834A20 (:2294): a human whose proxy needs another iteration is queued again
void character_controller::mt_integrate_further_casts(hkArray<human*, hkContainerHeapAllocator>& humans_to_integrate, human* p_human, bool last_iteration)
{
    if (m_characterProxy->mt_integrate_further_casts(last_iteration))
    {
        humans_to_integrate.pushBack(p_human);
    }
}

// 0x0082CC00 (:2308)
void character_controller::mt_push_objects()
{
    m_characterProxy->mt_push_objects();
}

// 0x0082CC10 (:2315)
void character_controller::mt_integrate_finish()
{
    m_characterProxy->mt_integrate_finish();
}

// 0x00822240 (:2337): ordinary NPCs stop pushing humans; the filter object lasts one frame after it was set; the
// shape-changed property is dropped
void character_controller::post_integrate(human* hp)
{
    if (hp != nullptr && hp->get_obj_type() == OT_HUMAN && hp->get_sub_type() == 2 && !hp->h_info->flags.guerrilla_tech && !hp->h_info->flags.guerrilla_rfc &&
        !hp->h_info->flags.old_coot && !hp->h_info->flags.vip && hp->hflags.activity_ha_hostage == 0)
    {
        hp->hflags.pushes_other_humans = 0;
    }
    if (hp->hflags.filter_handle_valid == 0)
    {
        hp->object_filter_handle = 0xFFFFFFFF;
    }
    else
    {
        hp->hflags.filter_handle_valid = 0;
    }
    if (hk_cc_has_property(m_phantom, CC_PROPERTY_SHAPE_CHANGED))
    {
        m_phantom->removeProperty(CC_PROPERTY_SHAPE_CHANGED);
    }
}

// 0x00822310 (:2534): humans always; light or small debris and movable movers that are not keyframed or fixed
bool character_controller_should_push_away(object* op, bool pushes_all_debris)
{
    if (op->get_obj_type() == OT_HUMAN)
    {
        return true;
    }
    if (op->get_obj_type() == OT_MOVER)
    {
        if (!((object_mover*)op)->m_level_obj_properties->m_flags.m_movable_by_humans)
        {
            return false;
        }
    }
    else if (op->get_obj_type() != OT_DEBRIS)
    {
        return false;
    }
    hkpRigidBody* body = havok_get_pointer_from_rigid_body_handle(op->havok_handle);
    if (body != nullptr && (body->getMotionType() == hkpMotion::MOTION_FIXED || body->getMotionType() == hkpMotion::MOTION_KEYFRAMED))
    {
        return false;
    }
    if (!(Character_collision_push_away_mass > op->get_mass()) && !pushes_all_debris)
    {
        return false;
    }
    vector bmin;
    vector bmax;
    op->get_local_bbox(bmin, bmax);
    // the largest dimension
    const float dx = bmax.x - bmin.x;
    float dim = bmax.y - bmin.y;
    const float dz = bmax.z - bmin.z;
    if (!(dim - dz >= 0.0f))
    {
        dim = dz;
    }
    if (dx - dim >= 0.0f)
    {
        dim = dx;
    }
    return dim < Character_collision_push_away_dim || pushes_all_debris;
}
