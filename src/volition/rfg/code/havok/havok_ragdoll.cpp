// Original: volition/rfg/code/havok/havok_ragdoll.cpp
//
// Ragdolls (decompiled; multiplayer.md MP7): Max_ragdolls slots (8 static ones in Base_ragdolls, up to 16 with the
// per-level memory of havok_ragdoll_add_ragdoll_memory). havok_ragdoll_bind_to_human takes a free slot or evicts the
// least important bound one (priority, dead, not a player, ...), then create_ragdoll builds the 12 bodies from the
// rig's bind pose (a translated sphere for the head, capsules for the rest; bullet quality, box inertia x4) and 11
// joints (7 hkpRagdollConstraintData, 4 hkpLimitedHingeConstraintData at the knees and elbows; inertias optimized
// along the tree). The bodies are queued for the world in havok_ragdoll_process_pre (add_ragdoll), which also blends
// the joint limits' tau from 0 to 1 over RAGDOLL_BLEND_TAU_TIME ms and moves the faked bodies of tag constraints.
// havok_sync_ragdoll_to_anim poses the bodies from the animation (or drives them there by velocities);
// havok_sync_anim_to_ragdoll / havok_blend_anim_to_ragdoll write the bodies back into the instance's model and
// vertex-to-model matrices. Up to 4 supplemental constraints pin a body to a point or a tag (a keyframed 1 m sphere
// that does not collide, joined by a ball and socket). Compiled in rfg_havok_unity.cpp after havok_mt_query.cpp, so
// the statics of havok.cpp (Hk_rigid_bodies, the entity add list) are shared.
//
// hkpWorldObject properties: 0 the human, 1 the my_ragdoll, 7 = 1 (a ragdoll body), 10 / 11 = 1 (not on the region
// callback shapes' enter / leave lists, all but the pelvis), 2 the ignore handle (havok_ragdoll_set_ignore_property).
// Property 6 is read as the body handle (havok_rigid_body_get_handle) but nothing in rfg.exe ever adds it: every
// addProperty / havok_set_property call site and every immediate store of 6 was checked (2026-10-21).
//
// Notes (decompiled behaviour, kept as is):
// - havok_ragdoll_constrain_to_point / _to_tag pivot a capsule bone at the sum of its two vertices (not their middle).
// - havok_ragdoll_constrain_to_tag never releases its ball and socket data (one reference leaks per constraint), and
//   gives up (-1, nothing printed) when the tagged object is a vehicle.
// - havok_ragdoll_cast_ray stores the impulse of a ray cast before the ragdoll is in the world, but
//   havok_ragdoll_cast_saved_ray then applies the saved start position as a point impulse at the end position.
// - havok_sync_ragdoll_to_anim computes the pelvis matrix from the bone end before it is overwritten by the generic
//   bone start one (no effect), and fetches the animation's bone velocities without using them (not ported).
// - havok_ragdoll_emergency_cleanup and havok_ragdoll_bind_to_human write to the human of a bound slot without checking
//   that it still exists.
// - havok_ragdoll_get_pos with RAGDOLL_ROOT reads bodies[-1].
// - havok_collect_ragdolls_fast never checks max_collected.

#include "volition/rfg/code/havok/havok_ragdoll.h"

#include "volition/anim_lib/code/anim_lib.h"
#include "volition/anim_lib/code/anim_lib_types.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/matrix/hkrotationf.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/matrix/hktransformf.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/quaternion/hkquaternionf.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/geometry/aabb/hkaabb.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraint/data/ballandsocket/hkpballandsocketconstraintdata.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraint/data/limitedhinge/hkplimitedhingeconstraintdata.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraint/data/ragdoll/hkpragdollconstraintdata.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/capsule/hkpcapsuleshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/convextranslate/hkpconvextranslateshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/sphere/hkpsphereshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/hkpconstraintinstance.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkprigidbody.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkprigidbodycinfo.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/hkpworld.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/solver/hkpsolversse.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/utilities/dynamics/inertia/hkpinertiatensorcomputer.h"
#include "volition/rfg/code/destruction/rfg_collisions.h"
#include "volition/rfg/code/gameplay/wrecking_crew/wrecking_crew.h"
#include "volition/rfg/code/geom/character.h"
#include "volition/rfg/code/geom/character_instance.h"
#include "volition/rfg/code/havok/api/hkcharactercontroller.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/havok/havok_collision_events.h"
#include "volition/rfg/code/havok/havok_raycast.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_collide.h"
#include "volition/rfg/code/objects/humans/human_info.h"
#include "volition/rfg/code/objects/humans/human_move.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/level_object_properties.h"
#include "volition/rfg/code/objects/movers/object_mover.h"
#include "volition/rfg/code/objects/object.h"
#include "volition/rfg/code/scripting/action_script/global_event_data.h"
#include "volition/rfg/code/scripting/action_script/global_event_manager.h"
#include "volition/vlib/code/math/bbox.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/math/ix.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/quaternion.h"
#include "volition/vlib/code/os/frametime.h"
#include "volition/vlib/code/time/timestamp.h"
#include "volition/vlib/code/util/varray.h"

#include "keen/base/trace.hpp"

#include <math.h>
#include <new>
#include <stddef.h>
#include <string.h>

using namespace hkSolverSse;

// ---- file-private types (rfg.pdb)
// size 0xF4
struct ragdoll_info
{
    int num_bones; // 0x00
    int bone_start_idx[12]; // 0x04
    int bone_end_idx[12]; // 0x34
    float radius[12]; // 0x64
    float mass[12]; // 0x94
    hkcdShapeType::ShapeTypeEnum shapeType[12]; // 0xC4
};

// size 0x1190
struct my_ragdoll
{
    my_ragdoll(); // 0x00824E60
    ~my_ragdoll(); // 0x00824ED0

    bool interpolate_relative_orient[64]; // 0x00
    matrix relative_orient[64]; // 0x40
    vector relative_offset[64]; // 0x940
    int anim_to_ragdoll_bone[64]; // 0xC40
    bool blown_off[64]; // 0xD40
    bool detached[12]; // 0xD80
    ragdoll_info info; // 0xD8C
    character_instance* char_inst; // 0xE80
    farray<hkpRigidBody*,12> bodies; // 0xE84
    farray<hkpConstraintInstance*,11> constraints; // 0xEC4
    hkVector4f bodyOffset[12]; // 0xF00
    bool sync_to_anim; // 0xFC0
    bool sweep; // 0xFC1
    bool change_to_active; // 0xFC2
    bool has_gravity; // 0xFC3
    bool added_to_world; // 0xFC4
    bool cast_ray; // 0xFC5
    bool should_destroy; // 0xFC6
    bool add_constraints; // 0xFC7
    ragdoll_priority priority; // 0xFC8
    int ragdoll_trigger; // 0xFCC
    unsigned int next_limb_check; // 0xFD0
    bool limb_penetrated[6]; // 0xFD4
    int blend_frame; // 0xFDC
    int contact_hits; // 0xFE0
    int vehicle_contact_hits; // 0xFE4
    vector cast_ray_start_pos; // 0xFE8
    vector cast_ray_end_pos; // 0xFF4
    float cast_ray_impulse; // 0x1000
    bool bone_flinch_state[12]; // 0x1004
    float bone_flinch_blend[12]; // 0x1010
    float bone_flinch_time[12]; // 0x1040
    int m_body_part_snd_handles[12]; // 0x1070
    timestamp m_active_timestamp; // 0x10A0
    timestamp ignore_property_ts; // 0x10A4
    unsigned int ignore_handle; // 0x10A8
    timestamp impact_audio_timeout; // 0x10AC
    supplemental_constraint ext_constraint[4]; // 0x10B0
    timestamp m_start_time; // 0x1180
    unsigned int human_handle; // 0x1184
};

static_assert(offsetof(my_ragdoll, info) == 0xD8C, "my_ragdoll::info");
static_assert(offsetof(my_ragdoll, bodies) == 0xE84, "my_ragdoll::bodies");
static_assert(offsetof(my_ragdoll, bodyOffset) == 0xF00, "my_ragdoll::bodyOffset");
static_assert(offsetof(my_ragdoll, ext_constraint) == 0x10B0, "my_ragdoll::ext_constraint");
static_assert(sizeof(my_ragdoll) == 0x1190, "my_ragdoll");
static_assert(sizeof(supplemental_constraint) == 0x34, "supplemental_constraint");

// ---- file-static globals
static const int Ragdoll_rigid_body_parent_ids[12] = {3, -1, 1, 2, 1, 1, 4, 5, 3, 3, 8, 9}; // 0x012F8EE4
static const int Ragdoll_flinch_bones[12][9] = {3, 8, 9, 10, 11, 2, -1, -1, -1, 0, 3, 8, 9, 10, 11, 4, 5, 2, 0, 8, 9, 10, 11, 2, -1, -1, -1, 2, 1, 3, 0, 9, 10, 11, 8, -1, 2, 1, 3, 0, 9, 10, 11, 8, -1, 4, 1, -1, -1, -1, -1, -1, -1, -1, 5, 1, -1, -1, -1, -1, -1, -1, -1, 10, 3, 0, 2, -1, -1, -1, -1, -1, 11, 3, 0, 2, -1, -1, -1, -1, -1, 8, -1, -1, -1, -1, -1, -1, -1, -1, 9, -1, -1, -1, -1, -1, -1, -1, -1, 0, 0, 0, 0, 0, 0, 0, 0, 0}; // 0x012F8F18
static const float Ragdoll_flinch_impulse_scaler[12] = {0.5f, 2.0f, 5.0f, 5.0f, 2.0f, 2.0f, 0.8f, 0.8f, 0.01f, 0.01f, 0.125f, 0.125f}; // 0x012F90C8
static const float Ragdoll_flinch_recovery_scaler[12] = {2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 2.0f, 9.0f, 9.0f, 5.0f, 5.0f}; // 0x012F90F8
static int Max_ragdolls = 8; // 0x0165FDF0
static float RAGDOLL_RESTITUTION = 0.075f; // 0x0165FDF4
static unsigned int RAGDOLL_BLEND_TAU_TIME = 0x1F4; // 0x0165FDF8, 500 ms
static bool Check_ragdoll_hits = true; // 0x0165FDFC
// The joint tuning of create_ragdoll (.data / .bss; names from rfg.pdb). The test_value* are limits in degrees.
static int max_hits = 200; // 0x016609B0: contact hits (decayed by 15 a frame) after which a ragdoll should be destroyed
static float factor_hint = 1.5f; // 0x016609B4, hkpInertiaTensorComputer::optimizeInertiasOfConstraintTree
static float test_value72 = 80.0f; // 0x016609B8
static float test_value71 = 10.0f; // 0x016609BC
static float test_value62 = 80.0f; // 0x016609C0
static float test_value61 = 10.0f; // 0x016609C4
static float test_value51 = -10.0f; // 0x016609C8
static float test_value52 = 45.0f; // 0x016609CC
static float test_value53 = -70.0f; // 0x016609D0
static float test_value54 = 2.5f; // 0x016609D4
static float test_value55 = -20.0f; // 0x016609D8
static float test_value56 = 20.0f; // 0x016609DC
static float test_value41 = -45.0f; // 0x016609E0
static float test_value42 = 10.0f; // 0x016609E4
static float test_value43 = -70.0f; // 0x016609E8
static float test_value44 = 2.5f; // 0x016609EC
static float test_value45 = -20.0f; // 0x016609F0
static float test_value46 = 20.0f; // 0x016609F4
static float test_value32 = 110.0f; // 0x016609F8
static float test_value22 = 110.0f; // 0x016609FC
static float test_value11 = -85.0f; // 0x01660A00
static float test_value12 = -1.0f; // 0x01660A04
static float test_value13 = -10.0f; // 0x01660A08
static float test_value15 = -15.0f; // 0x01660A0C
static float test_value16 = 15.0f; // 0x01660A10
static float test_value1 = -85.0f; // 0x01660A14
static float test_value2 = -1.0f; // 0x01660A18
static float test_value4 = 10.0f; // 0x01660A1C
static float test_value5 = -15.0f; // 0x01660A20
static float test_value6 = 15.0f; // 0x01660A24
static float linear_dampening = 0.2f; // 0x01660A28
static float angular_dampening = 2.0f; // 0x01660A2C
static float ground_friction = 7.0f; // 0x01660A30
static float test_value31; // 0x02B952FC
static float test_value21; // 0x02B95300
static float test_value14; // 0x02B95304
static float test_value3; // 0x02B95330
// base_array<hkpEntity*> Havok_add_entity_list (0x0165FEE4) is used here too; placed in havok.cpp.
// object_pool<rfg_mover,4000,1> rfg_moverpool (0x01F59580) is used here too; placed in rfg_mover.cpp.
// bool Hk_rigid_body_slot_used[1048576] (0x02667818) is used here too; placed in havok.cpp.
static my_ragdoll* Ragdolls[16]; // 0x0276D2E0
// hkpRigidBody* Hk_rigid_bodies[1048576] (0x0276E980) is used here too; placed in havok.cpp.
// bool Batch_init_active (0x02B8220B) is used here too; placed in havok.cpp.
static int Num_ragdolls; // 0x02B822BC
static int Num_additional_ragdolls; // 0x02B822C0
static int Ragdoll_created_this_frame; // 0x02B822C4
// mutex Hk_rigid_bodies_lock (0x02B83CA4) is used here too; placed in havok.cpp.
// cs_wrapper Havok_add_entity_wrapper (0x02B83DFC) is used here too; placed in havok.cpp.
static my_ragdoll Base_ragdolls[8]; // 0x02B83E40
// unsigned int $S6 (0x02B953C0) guards create_ragdoll's static twist / plane axes (function-local statics here).

// Magic number: (double)0.017453f, the degree to radian factor create_ragdoll multiplies the test_value* by.
static const double RAGDOLL_DEG_TO_RAD = (double)0.017453f;

// ---- internal helpers (inlined in the binary)
// A human by handle: null for an invalid handle, a missing object or one that is not a human.
static human* ragdoll_human_from_handle(unsigned int handle)
{
    if (handle == 0xFFFFFFFF)
    {
        return nullptr;
    }
    object* objp = World.object_from_handle(handle);
    if (objp == nullptr || objp->get_obj_type() != OT_HUMAN)
    {
        return nullptr;
    }
    return (human*)objp;
}

static hkVector4f ragdoll_vec4(const vector& v)
{
    hkVector4f r;
    r.m_quad = _mm_setr_ps(v.x, v.y, v.z, 0.0f);
    return r;
}

static vector ragdoll_vec3(const hkVector4f& v)
{
    return vector(v.m_quad.m128_f32[0], v.m_quad.m128_f32[1], v.m_quad.m128_f32[2]);
}

// A body's rotation as a vlib matrix (the hkRotationf columns as the rows rvec, uvec, fvec).
static void ragdoll_rotation_to_matrix(const hkRotationf& r, matrix& m)
{
    m.rvec = ragdoll_vec3(r.m_col0);
    m.uvec = ragdoll_vec3(r.m_col1);
    m.fvec = ragdoll_vec3(r.m_col2);
}

// The handle of a ragdoll body removed from the world (or the add list) without being activated, then released.
// Original bug kept: the handle comes from property 6, which is never set, so this always returns at once. Every
// ragdoll body (and its handle slot) leaks: it stays in the world, joints and all, until the level's world goes away.
static void ragdoll_free_body(hkpRigidBody* rb, bool remove_from_world)
{
    const unsigned int handle = havok_rigid_body_get_handle(rb);
    if (handle == 0xFFFFFFFF || Hk_rigid_bodies[handle] == nullptr)
    {
        return;
    }
    hkpEntity* ep = Hk_rigid_bodies[handle];
    if (remove_from_world && havok_remove_entity_from_list(ep) == -1)
    {
        Havok_world->removeEntity(ep);
    }
    ep->removeReference();
    havok_free_rigid_body_handle(handle);
}

// ---- my_ragdoll
// 0x00824E60
my_ragdoll::my_ragdoll()
{
}

// 0x00824ED0
my_ragdoll::~my_ragdoll()
{
}

// 0x00811BD0
supplemental_constraint::supplemental_constraint()
{
}

// ---- internal functions
// 0x00811BE0 (:533)
static my_ragdoll* get_ragdoll_by_index(int idx)
{
    if (idx >= 0 && idx < Max_ragdolls)
    {
        return Ragdolls[idx];
    }
    return nullptr;
}

// New helper (not in rfg.pdb): havok_collide_human_contact_handler (havok_collision_callbacks.cpp, earlier in the unity
// TU, before my_ragdoll is defined) sets the slot's active stamp inline in rfg.exe.
static void havok_ragdoll_keep_active(int ragdoll_idx, int ms)
{
    my_ragdoll* rp = get_ragdoll_by_index(ragdoll_idx);
    if (rp != nullptr)
    {
        rp->m_active_timestamp.set(ms);
    }
}

// 0x0081C6C0 (:540)
// The world holds the constraints from here on: their creation reference is dropped.
static void add_constraints_to_world(my_ragdoll& ragdoll)
{
    for (int i = 0; i < ragdoll.constraints.size(); i++)
    {
        if (ragdoll.constraints[i] != nullptr)
        {
            Havok_world->addConstraint(ragdoll.constraints[i]);
            ragdoll.constraints[i]->removeReference();
        }
    }
}

// 0x0083E260 (:557)
// The bodies not in a world yet go to the entity add list (activated) and are added right away; then the joints, the
// ignore property and properties 11 / 10 on all bodies but the pelvis.
static void add_ragdoll(my_ragdoll& ragdoll)
{
    for (int i = 0; i < ragdoll.bodies.size(); i++)
    {
        hkpRigidBody* rb = ragdoll.bodies[i];
        if (rb != nullptr && rb->m_world == nullptr)
        {
            havok_add_entity(rb, true, true);
        }
    }
    havok_add_entity_flush();
    if (ragdoll.add_constraints)
    {
        add_constraints_to_world(ragdoll);
    }
    if (ragdoll.ignore_handle != 0xFFFFFFFF)
    {
        for (int i = 0; i < NUM_RAGDOLL_BONES; i++)
        {
            hkpRigidBody* rb = ragdoll.bodies[i];
            if (rb != nullptr && rb->hasProperty(HAVOK_PROPERTY_IGNORE_HANDLE, hkpWorldObject::MULTI_THREADING_CHECKS_ENABLE))
            {
                continue;
            }
            havok_set_ignore_property(rb, ragdoll.ignore_handle);
        }
    }
    for (int i = 0; i < NUM_RAGDOLL_BONES; i++)
    {
        if (i == RAGDOLL_BODY)
        {
            continue;
        }
        hkpRigidBody* rb = ragdoll.bodies[i];
        if (!rb->hasProperty(11, hkpWorldObject::MULTI_THREADING_CHECKS_ENABLE))
        {
            rb->addProperty(11, hkSimplePropertyValue(1));
        }
        if (!rb->hasProperty(10, hkpWorldObject::MULTI_THREADING_CHECKS_ENABLE))
        {
            rb->addProperty(10, hkSimplePropertyValue(1));
        }
    }
    ragdoll.added_to_world = true;
}

// 0x00824EF0 (:605)
// LTCG: the ragdoll in ECX, idx in EAX.
static void remove_constraint(my_ragdoll* ragdoll, int idx)
{
    supplemental_constraint& scp = ragdoll->ext_constraint[idx];
    if (scp.m_constraint != nullptr)
    {
        if (scp.m_flags.constraint_added)
        {
            Havok_world->removeConstraint(scp.m_constraint);
            scp.m_flags.constraint_added = 0;
        }
        scp.m_constraint->removeReference();
        scp.m_constraint = nullptr;
    }
    if (scp.m_body_a != nullptr)
    {
        scp.m_body_a->removeReference();
        scp.m_body_a = nullptr;
    }
    if (scp.m_body_b != nullptr)
    {
        scp.m_body_b->removeReference();
        if (scp.m_flags.faked_body)
        {
            const unsigned int handle = havok_rigid_body_get_handle(scp.m_body_b);
            if (handle != 0xFFFFFFFF && Hk_rigid_bodies[handle] != nullptr)
            {
                havok_body_free(handle, true, true, false);
            }
        }
        scp.m_body_b = nullptr;
    }
    scp.m_flags.active = 0;
}

// 0x00825000 (:639)
// The supplemental constraints, the joints when the world does not hold them, then the bodies (removed from the world
// or the add list when the ragdoll was added) and their handles.
static void remove_ragdoll(my_ragdoll* ragdoll)
{
    for (int i = 0; i < 4; i++)
    {
        remove_constraint(ragdoll, i);
    }
    if (!ragdoll->added_to_world || !ragdoll->add_constraints)
    {
        for (int i = 0; i < ragdoll->constraints.size(); i++)
        {
            if (ragdoll->constraints[i] != nullptr)
            {
                ragdoll->constraints[i]->removeReference();
                ragdoll->constraints[i] = nullptr;
            }
        }
    }
    for (int i = 0; i < ragdoll->bodies.size(); i++)
    {
        if (ragdoll->bodies[i] == nullptr)
        {
            continue;
        }
        ragdoll_free_body(ragdoll->bodies[i], ragdoll->added_to_world);
        ragdoll->bodies[i] = nullptr;
    }
}

// 0x0082DB10 (:706)
// LTCG: rbAttached in ECX, pivot in EDX. The joint spaces in each body from the world pivot and axes (normalized), the
// twist axis of the reference body turned about its plane axis by the middle of the cone limits so the cone is
// symmetric (half its range); all limit tau factors 0 (zero effective mass until havok_ragdoll_process_pre blends
// them in), priority PSI.
static hkpConstraintInstance* create_ragdoll_contraint(hkpRigidBody* rbReference, hkpRigidBody* rbAttached, float coneMin, float coneMax, float planeMin, float planeMax, float twistMin, float twistMax, const hkVector4f& pivot, const hkVector4f& twistAxis, const hkVector4f& planeAxis, float max_friction_torque)
{
    hkVector4f pivotA;
    pivotA.setTransformedInversePos(rbReference->m_motion.m_motionState.m_transform, pivot);
    hkVector4f twistA;
    twistA.setRotatedInverseDir(rbReference->m_motion.m_motionState.m_sweptTransform.m_rotation1, twistAxis);
    twistA.m_quad = _mm_mul_ps(invSqrt23(hadd3(_mm_mul_ps(twistA.m_quad, twistA.m_quad))), twistA.m_quad);
    hkVector4f planeA;
    planeA.setRotatedInverseDir(rbReference->m_motion.m_motionState.m_sweptTransform.m_rotation1, planeAxis);
    planeA.m_quad = _mm_mul_ps(invSqrt23(hadd3(_mm_mul_ps(planeA.m_quad, planeA.m_quad))), planeA.m_quad);
    hkVector4f pivotB;
    pivotB.setTransformedInversePos(rbAttached->m_motion.m_motionState.m_transform, pivot);
    hkVector4f twistB;
    twistB.setRotatedInverseDir(rbAttached->m_motion.m_motionState.m_sweptTransform.m_rotation1, twistAxis);
    twistB.m_quad = _mm_mul_ps(invSqrt23(hadd3(_mm_mul_ps(twistB.m_quad, twistB.m_quad))), twistB.m_quad);
    hkVector4f planeB;
    planeB.setRotatedInverseDir(rbAttached->m_motion.m_motionState.m_sweptTransform.m_rotation1, planeAxis);
    planeB.m_quad = _mm_mul_ps(invSqrt23(hadd3(_mm_mul_ps(planeB.m_quad, planeB.m_quad))), planeB.m_quad);

    const float cone_half = (float)(((double)coneMax - (double)coneMin) * 0.5);
    const float cone_center = (float)(((double)coneMin + (double)coneMax) * 0.5);
    if (fabsf(cone_center) > 1.1920929e-07f) // FLT_EPSILON
    {
        hkQuaternionf q;
        q.setAxisAngle(planeA, -cone_center);
        twistA.setRotatedDir(q, twistA);
    }

    hkpRagdollConstraintData* data = new hkpRagdollConstraintData();
    data->setInBodySpace(pivotA, pivotB, planeA, planeB, twistA, twistB);
    data->m_atoms.m_coneLimit.m_maxAngle = cone_half;
    data->m_atoms.m_planesLimit.m_minAngle = planeMin;
    data->m_atoms.m_planesLimit.m_maxAngle = planeMax;
    data->m_atoms.m_twistLimit.m_minAngle = twistMin;
    data->m_atoms.m_twistLimit.m_maxAngle = twistMax;
    data->setMaxFrictionTorque(max_friction_torque);
    data->m_atoms.m_twistLimit.m_angularLimitsTauFactor = 0.0f;
    data->m_atoms.m_coneLimit.m_angularLimitsTauFactor = 0.0f;
    data->m_atoms.m_planesLimit.m_angularLimitsTauFactor = 0.0f;
    hkpConstraintInstance* instance = new hkpConstraintInstance(rbReference, rbAttached, data, hkpConstraintInstance::PRIORITY_PSI);
    data->removeReference();
    return instance;
}

// A knee or elbow (inlined in create_ragdoll): the hinge axis is the cross product of the limb and its parent limb
// (both normalized; the products and differences in double), limits in degrees, no tau.
static hkpConstraintInstance* create_ragdoll_hinge(hkpRigidBody* rbA, hkpRigidBody* rbB, const vector& own_start, const vector& own_end, const vector& parent_start, const vector& parent_end, float min_degrees, float max_degrees, float max_friction_torque)
{
    const hkVector4f pivot = rbA->m_motion.m_motionState.m_transform.m_translation;
    vector own(own_end.x - own_start.x, own_end.y - own_start.y, own_end.z - own_start.z);
    vector par(parent_start.x - parent_end.x, parent_start.y - parent_end.y, parent_start.z - parent_end.z);
    own.normalize();
    par.normalize();
    vector axis((float)((double)par.z * (double)own.y - (double)par.y * (double)own.z), (float)((double)own.z * (double)par.x - (double)par.z * (double)own.x), (float)((double)par.y * (double)own.x - (double)own.y * (double)par.x));
    axis.normalize();
    hkpLimitedHingeConstraintData* data = new hkpLimitedHingeConstraintData();
    data->setMinAngularLimit((float)((double)min_degrees * RAGDOLL_DEG_TO_RAD));
    data->setMaxAngularLimit((float)((double)max_degrees * RAGDOLL_DEG_TO_RAD));
    data->setInWorldSpace(rbA->m_motion.m_motionState.m_transform, rbB->m_motion.m_motionState.m_transform, pivot, ragdoll_vec4(axis));
    data->setMaxFrictionTorque(max_friction_torque);
    data->setAngularLimitsTauFactor(0.0f);
    hkpConstraintInstance* instance = new hkpConstraintInstance(rbA, rbB, data, hkpConstraintInstance::PRIORITY_PSI);
    data->removeReference();
    return instance;
}

static void ragdoll_set_body_offset(my_ragdoll& ragdoll, int body, int parent)
{
    ragdoll.bodyOffset[body].m_quad = _mm_sub_ps(ragdoll.bodies[body]->m_motion.m_motionState.m_transform.m_translation.m_quad, ragdoll.bodies[parent]->m_motion.m_motionState.m_transform.m_translation.m_quad);
}

// 0x00811C00 (:764)
// LTCG: info in ESI, cip in EAX. The 12 bodies' rig bones (by name), radii and masses.
static void get_ragdoll_info(ragdoll_info& info, character_instance* cip)
{
    anim_rig* rig = animlib_get_rig(cip->aim_handle);
    static const char* const start_names[12] = {"head", "root", "spine1", "spine2", "r-thigh", "l-thigh", "r-calf", "l-calf", "r-upperarm", "l-upperarm", "r-foretwist", "l-foretwist"};
    static const char* const end_names[12] = {"head", "spine1", "spine2", "head", "r-calf", "l-calf", "r-foot", "l-foot", "r-foretwist", "l-foretwist", "r-hand", "l-hand"};
    static const float radius[12] = {0.125f, 0.16f, 0.17f, 0.17f, 0.12f, 0.12f, 0.1f, 0.1f, 0.08f, 0.08f, 0.07f, 0.07f};
    static const float mass[12] = {4.0f, 20.0f, 20.0f, 20.0f, 5.5f, 5.5f, 4.5f, 4.5f, 2.0f, 2.0f, 1.8f, 1.8f};
    int start_idx[12];
    int end_idx[12];
    for (int i = 0; i < 12; i++)
    {
        start_idx[i] = animlib_find_bone_by_substring(rig, start_names[i]);
    }
    for (int i = 0; i < 12; i++)
    {
        end_idx[i] = animlib_find_bone_by_substring(rig, end_names[i]);
    }
    for (int i = 0; i < 12; i++)
    {
        info.bone_start_idx[i] = start_idx[i];
        info.bone_end_idx[i] = end_idx[i];
        info.radius[i] = radius[i];
        info.mass[i] = mass[i];
        info.shapeType[i] = (hkcdShapeType::ShapeTypeEnum)4; // Magic number: 4 for every body (unused)
    }
    info.num_bones = 12;
}

// 0x008120F0 (:817)
// LTCG: info in EDX, bone_idx in ESI. The body whose start is the rig bone; -1 when none is.
static int havok_bone_to_ragdoll(ragdoll_info& info, int bone_idx)
{
    for (int i = 0; i < info.num_bones; i++)
    {
        if (info.bone_start_idx[i] == bone_idx)
        {
            return i;
        }
    }
    return -1;
}

// 0x0082DEC0 (:998)
// LTCG: hp in ECX, index on the stack. Builds slot index's bodies and joints from the human's bind pose.
static bool create_ragdoll(const human* hp, int index)
{
    character_instance* cip = hp->char_inst;
    my_ragdoll* ragdoll = get_ragdoll_by_index(index);
    if (ragdoll == nullptr)
    {
        return false;
    }
    ragdoll->contact_hits = 0;
    ragdoll->vehicle_contact_hits = 0;
    memset(ragdoll->ext_constraint, 0, sizeof(ragdoll->ext_constraint));
    get_ragdoll_info(ragdoll->info, cip);
    if (ragdoll->info.num_bones == -1)
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        return false;
    }
    ragdoll->char_inst = nullptr;
    ragdoll->has_gravity = true;
    ragdoll->cast_ray = false;
    ragdoll->should_destroy = false;
    ragdoll->impact_audio_timeout.set(0);

    vector start[12];
    vector end[12];
    hkpRigidBodyCinfo cinfo;
    hkSimdFloat32 inertia_scale;
    inertia_scale.m_real = _mm_set1_ps(4.0f); // Magic number: the inertia of every body x4
    for (int i = 0; i < ragdoll->info.num_bones; i++)
    {
        ragdoll->bone_flinch_state[i] = false;
        start[i] = animlib_get_base_pose_bone_pos(cip->aim_handle, ragdoll->info.bone_start_idx[i]);
        end[i] = animlib_get_base_pose_bone_pos(cip->aim_handle, ragdoll->info.bone_end_idx[i]);
        if (cip->character_scale != 1.0f)
        {
            const float s = cip->character_scale;
            start[i] = vector(s * start[i].x, s * start[i].y, s * start[i].z);
            end[i] = vector(s * end[i].x, s * end[i].y, s * end[i].z);
        }
        const hkVector4f start4 = ragdoll_vec4(start[i]);
        hkVector4f d;
        d.m_quad = _mm_sub_ps(ragdoll_vec4(end[i]).m_quad, start4.m_quad);
        hkpShape* shape;
        if (i == RAGDOLL_HEAD)
        {
            // the head sphere sits a radius above the bone (the body's origin)
            hkpSphereShape* sphere = new hkpSphereShape(ragdoll->info.radius[0]);
            hkVector4f translation;
            translation.m_quad = _mm_setr_ps(0.0f, ragdoll->info.radius[0], 0.0f, 0.0f);
            shape = new hkpConvexTranslateShape(sphere, translation, hkpShapeContainer::REFERENCE_POLICY_INCREMENT);
            sphere->removeReference();
        }
        else
        {
            // the capsule starts a radius into the limb; it ends a radius short unless it is a calf or forearm
            hkVector4f dn;
            dn.m_quad = _mm_mul_ps(_mm_mul_ps(invSqrt23(hadd4(_mm_mul_ps(d.m_quad, d.m_quad))), d.m_quad), _mm_set1_ps(ragdoll->info.radius[i]));
            if (i != RAGDOLL_LR_LEG && i != RAGDOLL_LL_LEG && i != RAGDOLL_LR_ARM && i != RAGDOLL_LL_ARM)
            {
                d.m_quad = _mm_sub_ps(d.m_quad, dn.m_quad);
            }
            shape = new hkpCapsuleShape(dn, d, ragdoll->info.radius[i]);
        }
        cinfo.m_collisionFilterInfo = COLLIDE_RAGDOLL;
        cinfo.m_shape = shape;
        cinfo.m_position = start4;
        cinfo.m_rotation.m_vec.m_quad = g_vectorfConstants[HK_QUADREAL_0001];
        cinfo.m_friction = ground_friction;
        cinfo.m_angularDamping = angular_dampening;
        cinfo.m_linearDamping = linear_dampening;
        cinfo.m_mass = ragdoll->info.mass[i];
        cinfo.m_motionType = hkpMotion::MOTION_BOX_INERTIA;
        cinfo.m_restitution = RAGDOLL_RESTITUTION;
        hkpInertiaTensorComputer::setShapeVolumeMassProperties(shape, cinfo.m_mass, cinfo);
        cinfo.m_inertiaTensor.mul(inertia_scale);
        if (i == RAGDOLL_HEAD)
        {
            // Magic numbers: the head's center of mass a quarter radius higher, and it bounces fully
            hkVector4f up;
            up.m_quad = _mm_setr_ps(0.0f, (float)((double)ragdoll->info.radius[0] * 0.25), 0.0f, 0.0f);
            cinfo.m_centerOfMass.m_quad = _mm_add_ps(cinfo.m_centerOfMass.m_quad, up.m_quad);
            cinfo.m_restitution = 1.0f;
        }
        cinfo.m_qualityType = HK_COLLIDABLE_QUALITY_BULLET;
        havok_rigid_body* rb = new havok_rigid_body(cinfo);
        havok_generate_rigid_body_handle(rb);
        ragdoll->bodies.add(rb);
        shape->removeReference();
        if (!rb->hasProperty(1, hkpWorldObject::MULTI_THREADING_CHECKS_ENABLE))
        {
            rb->addProperty(1, hkSimplePropertyValue((void*)ragdoll));
        }
        if (!rb->hasProperty(7, hkpWorldObject::MULTI_THREADING_CHECKS_ENABLE))
        {
            rb->addProperty(7, hkSimplePropertyValue(1));
        }
        if (!rb->hasProperty(HAVOK_PROPERTY_OBJECT, hkpWorldObject::MULTI_THREADING_CHECKS_ENABLE))
        {
            rb->addProperty(HAVOK_PROPERTY_OBJECT, hkSimplePropertyValue((void*)hp));
        }
    }

    // the joints: the pivot is the attached body's origin (the start of its bone)
    hkpRigidBody** b = ragdoll->bodies.elements();
    hkVector4f twist;
    hkVector4f plane;
    // neck: upper body -> head
    twist.m_quad = _mm_setr_ps(0.0f, 1.0f, 0.0f, 0.0f); // 0x012F91F0
    plane.m_quad = _mm_setr_ps(0.0f, 0.0f, 1.0f, 0.0f); // 0x012F9270
    ragdoll->constraints.add(create_ragdoll_contraint(b[3], b[0], -0.261794984f, 0.261794984f, -1.57077003f, 1.57077003f, -0.610854983f, 0.610854983f, b[0]->m_motion.m_motionState.m_transform.m_translation, twist, plane, 0.3f));
    ragdoll_set_body_offset(*ragdoll, 0, 3);
    // spine: pelvis -> spine, spine -> upper body
    twist.m_quad = _mm_setr_ps(0.0f, 0.0f, 1.0f, 0.0f); // 0x012F9270
    plane.m_quad = _mm_setr_ps(0.0f, -1.0f, 0.0f, 0.0f); // 0x012F9280
    ragdoll->constraints.add(create_ragdoll_contraint(b[1], b[2], -0.436325014f, 0.436325014f, -0.349059999f, 0.174529999f, -0.0872649997f, 0.0872649997f, b[2]->m_motion.m_motionState.m_transform.m_translation, twist, plane, 0.7f));
    ragdoll_set_body_offset(*ragdoll, 2, 1);
    ragdoll->constraints.add(create_ragdoll_contraint(b[2], b[3], -0.436325014f, 0.436325014f, -0.349059999f, 0.174529999f, -0.0872649997f, 0.0872649997f, b[3]->m_motion.m_motionState.m_transform.m_translation, twist, plane, 0.7f));
    ragdoll_set_body_offset(*ragdoll, 3, 2);
    // hips: pelvis -> thighs
    twist.m_quad = _mm_setr_ps(0.0f, -1.0f, 0.0f, 0.0f); // 0x012F9280
    plane.m_quad = _mm_setr_ps(-1.0f, 0.0f, 0.0f, 0.0f); // 0x012F9290
    ragdoll->constraints.add(create_ragdoll_contraint(b[1], b[4], (float)((double)test_value1 * RAGDOLL_DEG_TO_RAD), (float)((double)test_value2 * RAGDOLL_DEG_TO_RAD), (float)((double)test_value3 * RAGDOLL_DEG_TO_RAD), (float)((double)test_value4 * RAGDOLL_DEG_TO_RAD), (float)((double)test_value5 * RAGDOLL_DEG_TO_RAD), (float)((double)test_value6 * RAGDOLL_DEG_TO_RAD), b[4]->m_motion.m_motionState.m_transform.m_translation, twist, plane, 0.7f));
    ragdoll_set_body_offset(*ragdoll, 4, 1);
    ragdoll->constraints.add(create_ragdoll_contraint(b[1], b[5], (float)((double)test_value11 * RAGDOLL_DEG_TO_RAD), (float)((double)test_value12 * RAGDOLL_DEG_TO_RAD), (float)((double)test_value13 * RAGDOLL_DEG_TO_RAD), (float)((double)test_value14 * RAGDOLL_DEG_TO_RAD), (float)((double)test_value15 * RAGDOLL_DEG_TO_RAD), (float)((double)test_value16 * RAGDOLL_DEG_TO_RAD), b[5]->m_motion.m_motionState.m_transform.m_translation, twist, plane, 0.7f));
    ragdoll_set_body_offset(*ragdoll, 5, 1);
    // knees: calf -> thigh
    ragdoll->constraints.add(create_ragdoll_hinge(b[6], b[4], start[6], end[6], start[4], end[4], test_value21, test_value22, 0.5f));
    ragdoll_set_body_offset(*ragdoll, 6, 4);
    ragdoll->constraints.add(create_ragdoll_hinge(b[7], b[5], start[7], end[7], start[5], end[5], test_value31, test_value32, 0.5f));
    ragdoll_set_body_offset(*ragdoll, 7, 5);
    // shoulders: upper body -> upper arms, twisting along the arm
    plane.m_quad = _mm_setr_ps(0.0f, 0.0f, 1.0f, 0.0f);
    vector arm(end[8].x - start[8].x, end[8].y - start[8].y, end[8].z - start[8].z);
    arm.normalize_safe();
    twist = ragdoll_vec4(arm);
    ragdoll->constraints.add(create_ragdoll_contraint(b[3], b[8], (float)((double)test_value41 * RAGDOLL_DEG_TO_RAD), (float)((double)test_value42 * RAGDOLL_DEG_TO_RAD), (float)((double)test_value43 * RAGDOLL_DEG_TO_RAD), (float)((double)test_value44 * RAGDOLL_DEG_TO_RAD), (float)((double)test_value45 * RAGDOLL_DEG_TO_RAD), (float)((double)test_value46 * RAGDOLL_DEG_TO_RAD), b[8]->m_motion.m_motionState.m_transform.m_translation, twist, plane, 0.7f));
    ragdoll_set_body_offset(*ragdoll, 8, 3);
    arm = vector(end[9].x - start[9].x, end[9].y - start[9].y, end[9].z - start[9].z);
    arm.normalize_safe();
    twist = ragdoll_vec4(arm);
    ragdoll->constraints.add(create_ragdoll_contraint(b[3], b[9], (float)((double)test_value51 * RAGDOLL_DEG_TO_RAD), (float)((double)test_value52 * RAGDOLL_DEG_TO_RAD), (float)((double)test_value53 * RAGDOLL_DEG_TO_RAD), (float)((double)test_value54 * RAGDOLL_DEG_TO_RAD), (float)((double)test_value55 * RAGDOLL_DEG_TO_RAD), (float)((double)test_value56 * RAGDOLL_DEG_TO_RAD), b[9]->m_motion.m_motionState.m_transform.m_translation, twist, plane, 0.7f));
    ragdoll_set_body_offset(*ragdoll, 9, 3);
    // elbows: forearm -> upper arm
    ragdoll->constraints.add(create_ragdoll_hinge(b[10], b[8], start[10], end[10], start[8], end[8], test_value61, test_value62, 0.1f));
    ragdoll_set_body_offset(*ragdoll, 10, 8);
    ragdoll->constraints.add(create_ragdoll_hinge(b[11], b[9], start[11], end[11], start[9], end[9], test_value71, test_value72, 0.1f));
    ragdoll_set_body_offset(*ragdoll, 11, 9);

    hkpConstraintInstance* constraints[11];
    for (int i = 0; i < 11; i++)
    {
        constraints[i] = ragdoll->constraints[i];
    }
    hkpInertiaTensorComputer::optimizeInertiasOfConstraintTree(constraints, 11, b[0], factor_hint);
    return true;
}

// ---- matrices
// 0x00812110 (:830)
// Saves the instance's model matrices as the relative orients and offsets (identities without matrices).
void havok_ragdoll_store_matrices(int idx, character_instance* cip)
{
    my_ragdoll* ragdoll = get_ragdoll_by_index(idx);
    matrix43* matrices = nullptr;
    if (animlib_get_instanced_model_matrices(cip->aim_handle, &matrices) && matrices != nullptr && ragdoll != nullptr)
    {
        ragdoll->contact_hits = 0;
        ragdoll->vehicle_contact_hits = 0;
        const int num_bones = animlib_get_num_bones(cip->aim_handle);
        for (int i = 0; i < num_bones; i++)
        {
            ragdoll->relative_offset[i] = matrices[i].m_translation;
            ragdoll->relative_orient[i] = matrices[i].m_rotation;
        }
        return;
    }
    const int num_bones = animlib_get_num_bones(cip->aim_handle);
    for (int i = 0; i < num_bones; i++)
    {
        ragdoll->relative_offset[i].zero();
        ragdoll->relative_orient[i].make_identity();
    }
}

// 0x00812220 (:861)
void havok_ragdoll_recall_matrices(int idx, character_instance* cip)
{
    my_ragdoll* ragdoll = get_ragdoll_by_index(idx);
    matrix43* matrices = nullptr;
    animlib_get_instanced_model_matrices(cip->aim_handle, &matrices);
    if (matrices == nullptr || ragdoll == nullptr)
    {
        return;
    }
    const int num_bones = animlib_get_num_bones(cip->aim_handle);
    for (int i = 0; i < num_bones; i++)
    {
        matrices[i].m_translation = ragdoll->relative_offset[i];
        matrices[i].m_rotation = ragdoll->relative_orient[i];
    }
    animlib_validate_instanced_model_matrices(cip->aim_handle);
}

// 0x0081C750 (:884)
// For the bones without a body: the offset and orient relative to the parent bone (from the model matrices, else the
// rig's bind translation and the identity), interpolated towards the animation later.
void havok_ragdoll_init_relative_orients(int idx, character_instance* cip)
{
    my_ragdoll* ragdoll = get_ragdoll_by_index(idx);
    matrix43* matrices = nullptr;
    const bool have_matrices = animlib_get_instanced_model_matrices(cip->aim_handle, &matrices);
    if (ragdoll == nullptr)
    {
        return;
    }
    const int num_bones = animlib_get_num_bones(cip->aim_handle);
    for (int bone = 0; bone < num_bones; bone++)
    {
        const int body = havok_bone_to_ragdoll(ragdoll->info, bone);
        ragdoll->anim_to_ragdoll_bone[bone] = body;
        if (body != -1)
        {
            continue;
        }
        vector offset;
        matrix orient;
        if (have_matrices && matrices != nullptr)
        {
            const int parent = animlib_get_bone_parent_index(cip->aim_handle, bone);
            if (parent == -1)
            {
                continue;
            }
            vector parent_pos;
            matrix parent_orient;
            matrices[bone].get_translation(offset);
            matrices[parent].get_translation(parent_pos);
            matrices[bone].get_orientation(orient);
            matrices[parent].get_orientation(parent_orient);
            if (cip->character_scale != 1.0f)
            {
                orient.rvec.normalize();
                orient.uvec.normalize();
                orient.fvec.normalize();
                parent_orient.rvec.normalize();
                parent_orient.uvec.normalize();
                parent_orient.fvec.normalize();
            }
            offset = vector(offset.x - parent_pos.x, offset.y - parent_pos.y, offset.z - parent_pos.z);
            offset.rotate(parent_orient);
            parent_orient.transpose();
            orient = orient * parent_orient;
        }
        else
        {
            if (!cip->rdd.is_loaded())
            {
                return;
            }
            orient.make_identity();
            // Original: the bind translation of the rig bone with the same index (not mapped through the entity)
            offset = cip->rdd.get_data()->base_character->rig->bones[bone].rel_bone_translation;
        }
        ragdoll->relative_offset[bone] = offset;
        ragdoll->relative_orient[bone] = orient;
        ragdoll->relative_orient[bone].orthogonalize();
        ragdoll->interpolate_relative_orient[bone] = true;
    }
}

// ---- collision filter, position, velocity
// 0x0081CA60 (:962)
unsigned int havok_ragdoll_get_collision_filter(int ragdoll_idx)
{
    my_ragdoll* ragdoll = get_ragdoll_by_index(ragdoll_idx);
    if (ragdoll == nullptr)
    {
        return COLLIDE_NO_COLLISION;
    }
    return ragdoll->bodies[0]->m_collidable.m_broadPhaseHandle.m_collisionFilterInfo;
}

// 0x0081CA90 (:972)
void havok_ragdoll_set_collision_filter(int ragdoll_idx, unsigned int collision)
{
    my_ragdoll* ragdoll = get_ragdoll_by_index(ragdoll_idx);
    if (ragdoll == nullptr || ragdoll->bodies[0]->m_collidable.m_broadPhaseHandle.m_collisionFilterInfo == collision)
    {
        return;
    }
    for (int i = 0; i < ragdoll->bodies.size(); i++)
    {
        hkpRigidBody* rb = ragdoll->bodies[i];
        rb->m_collidable.m_broadPhaseHandle.m_collisionFilterInfo = collision;
        if (rb->m_world != nullptr)
        {
            Havok_world->updateCollisionFilterOnEntity(rb, HK_UPDATE_FILTER_ON_ENTITY_FULL_CHECK, HK_UPDATE_COLLECTION_FILTER_PROCESS_SHAPE_COLLECTIONS);
        }
    }
}

// 0x0081CB00 (:2184)
void havok_ragdoll_get_pos(int ragdoll_idx, vector& pos, const ragdoll_part body_part)
{
    my_ragdoll* ragdoll = get_ragdoll_by_index(ragdoll_idx);
    if (ragdoll == nullptr)
    {
        return;
    }
    pos = ragdoll_vec3(ragdoll->bodies.elements()[body_part]->m_motion.m_motionState.m_transform.m_translation);
}

// 0x0081CB80 (:2199)
// The pelvis velocity; zero before the ragdoll is in the world.
void havok_ragdoll_get_vel(int ragdoll_idx, vector& vel)
{
    my_ragdoll* ragdoll = get_ragdoll_by_index(ragdoll_idx);
    if (ragdoll == nullptr || !ragdoll->added_to_world)
    {
        vel.zero();
        return;
    }
    vel = ragdoll_vec3(ragdoll->bodies[RAGDOLL_BODY]->m_motion.m_linearVelocity);
}

// 0x00812300 (:2215)
// A damped spring (the half distance x 86.6 as an impulse at the body's center, then -20 dt^2 x 86.6 of its velocity).
void apply_simple_spring_constraint(supplemental_constraint* scp)
{
    if (scp == nullptr || !scp->m_flags.active || scp->handle != 0xFFFFFFFF)
    {
        return;
    }
    hkVector4f a = ragdoll_vec4(scp->m_point_a);
    a.setTransformedPos(scp->m_body_a->m_motion.m_motionState.m_transform, a);
    // Magic numbers: 0.5 and 86.6 (the spring), -20 (the damping)
    hkVector4f impulse;
    impulse.m_quad = _mm_setr_ps((float)((double)(scp->m_point_b.x - a.m_quad.m128_f32[0]) * 0.5 * 86.6), (float)((double)(scp->m_point_b.y - a.m_quad.m128_f32[1]) * 0.5 * 86.6), (float)((double)(scp->m_point_b.z - a.m_quad.m128_f32[2]) * 0.5 * 86.6), (float)((double)(0.0f - a.m_quad.m128_f32[3]) * 0.5 * 86.6));
    scp->m_body_a->activate();
    scp->m_body_a->m_motion.applyLinearImpulse(impulse);
    const float damp = (float)((double)Frametime * -20.0 * (double)Frametime * 86.6);
    hkVector4f damping;
    damping.m_quad = _mm_mul_ps(_mm_set1_ps(damp), scp->m_body_a->m_motion.m_linearVelocity.m_quad);
    scp->m_body_a->activate();
    scp->m_body_a->m_motion.applyLinearImpulse(damping);
}

// ---- collisions
// 0x00825220 (:2291)
// A ragdoll body hit something: contact hits (too many while Check_ragdoll_hits destroys the ragdoll), vehicles
// ignored for 2 s after 250 hits (a dead single player npc fades), an active-on-impact ragdoll activated by a fast
// enough hit, impact effects every 50 ms (not on vehicles or a killzone death), walker / vehicle / mover damage, and
// falling damage from the normal velocity (between the safe and fatal fall speeds, every 500 ms).
void havok_collide_ragdoll(const collision_event* c_event)
{
    human* hp = (human*)c_event->m_object_a;
    if (hp == nullptr || hp->get_obj_type() != OT_HUMAN || hp->ragdoll_state == HUMAN_RAGDOLL_STATE_NONE)
    {
        return;
    }
    my_ragdoll* ragdoll = (my_ragdoll*)havok_get_property(c_event->m_collisions[0].m_hk_rigid_body_a, 1, nullptr, true);
    if (ragdoll == nullptr)
    {
        return;
    }
    ragdoll->contact_hits += c_event->m_num_collisions;
    if (c_event->m_object_b != nullptr && c_event->m_object_b->get_sub_type() == 4)
    {
        ragdoll->vehicle_contact_hits += c_event->m_num_collisions;
        if (ragdoll->vehicle_contact_hits > 250) // Magic number: hits with a vehicle before it is ignored
        {
            ragdoll->vehicle_contact_hits = 0;
            int ragdoll_idx = 0;
            while (ragdoll_idx < Max_ragdolls && Ragdolls[ragdoll_idx] != ragdoll)
            {
                ragdoll_idx++;
            }
            havok_ragdoll_set_ignore_property(ragdoll_idx, c_event->m_object_b->get_handle(), 2000);
            if (human_is_dead(hp) && hp->get_sub_type() == 2 && !Multiplayer)
            {
                ((npc*)hp)->fade_out_and_despawn(500, false);
            }
        }
    }
    if (ragdoll->contact_hits > max_hits && Check_ragdoll_hits)
    {
        ragdoll->should_destroy = true;
    }
    bool play_effects = true;
    if (c_event->m_object_b == nullptr || c_event->m_object_b->get_obj_type() == OT_VEHICLE)
    {
        play_effects = false;
    }
    if (!Multiplayer && !wrecking_crew_is_active() && object_is_local_player(hp) && hp->is_killed() && hp->hflags.killed_by_killzone)
    {
        play_effects = false;
    }
    for (unsigned int i = 0; i < c_event->m_num_collisions; i++)
    {
        const collision_event::collision_info& info = c_event->m_collisions[i];
        // Magic number: 0.8660254, cos 30 degrees (hits flatter than that, or all with ragdoll_on_impact_all_collisions)
        if (hp->ragdoll_state == HUMAN_RAGDOLL_STATE_ACTIVE_ON_IMPACT && info.m_relative_point_velocity.mag_squared() > hp->ragdoll_on_impact_min_vel && (info.m_collision_normal.y < 0.8660254f || hp->hflags.ragdoll_on_impact_all_collisions))
        {
            ragdoll->change_to_active = true;
        }
        if (play_effects && ragdoll->impact_audio_timeout.elapsed())
        {
            rfg_effect_candidate_collide(c_event);
            ragdoll->impact_audio_timeout.set(50);
        }
    }
    const hkpRigidBody* body_b = c_event->m_collisions[0].m_hk_rigid_body_b;
    const unsigned int component = (unsigned int)havok_get_property(body_b, 5, (void*)0xFFFFFFFF, true);
    object* other = c_event->m_object_b;
    if (other != nullptr)
    {
        if (other->get_sub_type() == 6)
        {
            // a walker: the fastest normal velocity plus half the hit count, plus a tenth of all walker hits once
            // fast
            float max_vel = 0.0f;
            for (unsigned int i = 0; i < c_event->m_num_collisions; i++)
            {
                if ((double)c_event->m_collisions[i].m_relative_normal_velocity > (double)max_vel)
                {
                    max_vel = c_event->m_collisions[i].m_relative_normal_velocity;
                }
            }
            float rel_speed = (float)((double)c_event->m_num_collisions * 0.5 + (double)max_vel);
            if (rel_speed > 3.0f && hp->get_sub_type() != 3)
            {
                hp->ragdoll_num_collisions_with_walker += c_event->m_num_collisions;
                rel_speed = (float)((double)(float)hp->ragdoll_num_collisions_with_walker / 10.0 + (double)rel_speed);
            }
            human_collide_vehicle(c_event->m_collisions[0].m_collision_point, c_event->m_collisions[0].m_collision_normal, rel_speed, c_event->m_object_a->get_handle(), c_event->m_object_b->get_handle(), component, false);
        }
        else if (other->get_obj_type() == OT_VEHICLE)
        {
            if (hp->rdd.is_loaded())
            {
                if (hp->rdd.get_data()->controller == nullptr)
                {
                    return; // (no falling damage either)
                }
                vector vel;
                havok_ragdoll_get_vel(hp->ragdoll_idx, vel);
                const vector& a_vel = c_event->m_collisions[0].m_a_point_velocity;
                const vector rel(vel.x - a_vel.x, vel.y - a_vel.y, vel.z - a_vel.z);
                human_collide_vehicle(c_event->m_collisions[0].m_collision_point, c_event->m_collisions[0].m_collision_normal, rel.mag(), c_event->m_object_a->get_handle(), c_event->m_object_b->get_handle(), component, false);
            }
        }
        else if (other->get_obj_type() == OT_MOVER || other->get_obj_type() == OT_DEBRIS)
        {
            // the faster of the mover body's velocity and its point velocity
            const hkVector4f& lv = ((const hkpRigidBody*)body_b)->m_motion.m_linearVelocity;
            const float body_speed2 = lane0(hadd3(_mm_mul_ps(lv.m_quad, lv.m_quad)));
            vector velocity;
            if ((double)c_event->m_collisions[0].m_b_point_velocity.mag_squared() > (double)body_speed2)
            {
                velocity = ragdoll_vec3(lv);
            }
            else
            {
                velocity = c_event->m_collisions[0].m_b_point_velocity;
            }
            human_collide_mover_damage(hp, other, c_event->m_collisions[0].m_collision_point, velocity, c_event->m_collisions[0].m_collision_normal);
            other = c_event->m_object_b;
            if (other != nullptr && other->get_obj_type() == OT_MOVER && ((object_mover*)other)->m_level_obj_properties->m_flags.m_generate_human_collision_event)
            {
                mover_human_special_collision_event ev;
                ev.mover_handle = other->get_handle();
                ev.human_handle = hp->get_handle();
                gem_post_event(ev);
            }
        }
    }
    if (c_event->m_object_b != nullptr && hp->rdd.is_loaded() && hp->rdd.get_data()->controller != nullptr)
    {
        vector controller_vel;
        hp->rdd.get_data()->controller->get_velocity(controller_vel);
        float max_speed = 0.0f;
        for (unsigned int i = 0; i < c_event->m_num_collisions; i++)
        {
            const collision_event::collision_info& info = c_event->m_collisions[i];
            const float speed = info.m_collision_normal.y * info.m_relative_normal_velocity;
            max_speed = max_speed > speed ? max_speed : speed; // max<float> (0x004B18E0)
        }
        const float safe_speed = human_move_fall_get_safe_speed(hp, false);
        const float fatal_speed = human_move_fall_get_fatal_speed(hp, false);
        if (max_speed > safe_speed)
        {
            float damage = fl_parameter(max_speed, safe_speed, fatal_speed, 0.0f, (float)hp->max_hit_points, false);
            if (hp->ragdoll_damaged.elapsed())
            {
                if (hp->h_info->flags.guerrilla_tech)
                {
                    damage = (float)((double)damage * 0.5); // Magic number: guerrilla tech halves the falling damage
                }
                human_damage(hp, nullptr, damage, DBT_NONE, nullptr, nullptr, true, false);
                hp->ragdoll_damaged.set(500);
            }
        }
    }
}

// ---- slots
// 0x00812430 (:2513)
void havok_ragdoll_system_init()
{
    for (int i = 0; i < 8; i++)
    {
        Ragdolls[i] = &Base_ragdolls[i];
    }
}

// 0x0081CC10 (:2522)
void havok_ragdoll_level_init_pre()
{
    for (int i = 0; i < Max_ragdolls; i++)
    {
        Ragdolls[i]->char_inst = nullptr;
        Ragdolls[i]->info.num_bones = -1;
        Ragdolls[i]->bodies.remove_all();
        Ragdolls[i]->constraints.remove_all();
    }
    Num_ragdolls = 0;
    for (int i = Max_ragdolls; i < 16; i++)
    {
        Ragdolls[i] = nullptr;
    }
}

// 0x0081CC80 (:2543)
void havok_ragdoll_level_shutdown()
{
    for (int i = 0; i < Max_ragdolls; i++)
    {
        Ragdolls[i]->char_inst = nullptr;
        Ragdolls[i]->info.num_bones = -1;
        Ragdolls[i]->bodies.remove_all();
        Ragdolls[i]->constraints.remove_all();
    }
    for (int i = Max_ragdolls; i < 16; i++)
    {
        Ragdolls[i] = nullptr;
    }
}

// 0x00812490 (:2558)
int havok_ragdoll_get_ragdoll_size()
{
    return sizeof(my_ragdoll);
}

// 0x00825990 (:2563)
// Extra slots in the level's memory (once; up to 16 slots in all).
void havok_ragdoll_add_ragdoll_memory(int num_ragdolls, void* memory)
{
    if (Num_additional_ragdolls >= 1)
    {
        return;
    }
    int slot = Max_ragdolls;
    for (int i = 0; i < num_ragdolls && slot <= 15; i++, slot++)
    {
        Ragdolls[slot] = (my_ragdoll*)memory;
        if (memory != nullptr)
        {
            new (memory) my_ragdoll();
        }
        memory = (char*)memory + sizeof(my_ragdoll);
    }
    Max_ragdolls = slot;
    Num_additional_ragdolls = num_ragdolls;
}

// 0x0082B370 (:2582)
void havok_ragdoll_remove_ragdoll_memory()
{
    if (Num_additional_ragdolls <= 0)
    {
        return;
    }
    my_ragdoll** slot = &Ragdolls[8];
    do
    {
        (*slot)->~my_ragdoll();
        *slot = nullptr;
        slot++;
        Num_additional_ragdolls--;
    } while (Num_additional_ragdolls != 0);
    Max_ragdolls = 8;
    Num_additional_ragdolls = 0;
}

// 0x0082B3C0 (:2600)
// Unbinds every ragdoll (single player destroys the humans) unless the Havok world is in a critical operation.
void havok_ragdoll_emergency_cleanup()
{
    if (Havok_world->m_criticalOperationsLockCount != 0)
    {
        return;
    }
    for (int ragdoll_idx = 0; ragdoll_idx < Max_ragdolls; ragdoll_idx++)
    {
        my_ragdoll* ragdoll = Ragdolls[ragdoll_idx];
        if (ragdoll->char_inst == nullptr)
        {
            remove_ragdoll(ragdoll);
            continue;
        }
        human* hp = ragdoll_human_from_handle(ragdoll->human_handle);
        if (object_is_player(hp))
        {
            continue;
        }
        hp->ragdoll_idx = -1;
        hp->ragdoll_state = HUMAN_RAGDOLL_STATE_NONE;
        if (!Multiplayer)
        {
            World.destroy_object(hp, false);
        }
        hp->ragdoll_source_human_handle = 0xFFFFFFFF;
        havok_ragdoll_unbind_from_human(ragdoll_idx);
    }
}

// 0x0082FD30 (:2636)
// A free slot, else the bound ragdoll with the highest eviction score: +1 lower priority than the request, +20 its
// human is gone, else +1 deactivated, +1 not _dont_use_me, +2 offscreen, +2 fading out, +6 killed, +6 not a player;
// never a single player player or a corpse npc (-1). The evicted human is reset (single player destroys it). The new
// ragdoll is posed from the animation (immediate: it simulates at once instead of following the animation).
int havok_ragdoll_bind_to_human(const human* hp, bool immediate, ragdoll_priority priority, int trigger, bool add_constraints)
{
    int slot = -1;
    for (int i = 0; i < Max_ragdolls; i++)
    {
        if (Ragdolls[i]->char_inst == nullptr)
        {
            slot = i;
            break;
        }
    }
    if (slot == -1)
    {
        int best_score = -1;
        for (int i = 0; i < Max_ragdolls; i++)
        {
            int score = Ragdolls[i]->priority < priority ? 1 : 0;
            human* other = ragdoll_human_from_handle(Ragdolls[i]->human_handle);
            if (other == nullptr)
            {
                score += 20;
            }
            else
            {
                if (other->ragdoll_state == HUMAN_RAGDOLL_STATE_DEACTIVATED)
                {
                    score++;
                }
                if (!other->obj_flags._dont_use_me)
                {
                    score++;
                }
                if (other->is_offscreen())
                {
                    score += 2;
                }
                if (other->hflags.fading_out)
                {
                    score += 2;
                }
                if (other->is_killed())
                {
                    score += 6;
                }
                if (!object_is_player(other))
                {
                    score += 6;
                }
                else if (!Multiplayer)
                {
                    score = -1;
                }
                human* again = ragdoll_human_from_handle(Ragdolls[i]->human_handle);
                if (again != nullptr && again->get_sub_type() == 2 && ((npc*)again)->npc_flags.spawned_as_corpse)
                {
                    score = -1;
                }
            }
            if (score > best_score)
            {
                slot = i;
                best_score = score;
            }
        }
        if (slot == -1)
        {
            return -1;
        }
        my_ragdoll* evicted = Ragdolls[slot];
        if (evicted->char_inst == nullptr)
        {
            remove_ragdoll(evicted);
        }
        else
        {
            human* other = ragdoll_human_from_handle(evicted->human_handle);
            other->ragdoll_idx = -1;
            other->ragdoll_state = HUMAN_RAGDOLL_STATE_NONE;
            if (!Multiplayer)
            {
                World.destroy_object(other, false);
            }
            other->ragdoll_source_human_handle = 0xFFFFFFFF;
            havok_ragdoll_unbind_from_human(slot);
        }
    }
    if (!create_ragdoll(hp, slot))
    {
        return -1;
    }
    Num_ragdolls++;
    Ragdoll_created_this_frame++;
    my_ragdoll* ragdoll = Ragdolls[slot];
    ragdoll->char_inst = hp->char_inst;
    ragdoll->ragdoll_trigger = trigger;
    ragdoll->sync_to_anim = true;
    ragdoll->sweep = false;
    ragdoll->ignore_handle = 0xFFFFFFFF;
    ragdoll->priority = priority;
    ragdoll->added_to_world = false;
    memset(ragdoll->blown_off, 0, sizeof(ragdoll->blown_off));
    memset(ragdoll->detached, 0, sizeof(ragdoll->detached));
    ragdoll->next_limb_check = 0;
    memset(ragdoll->limb_penetrated, 0, sizeof(ragdoll->limb_penetrated));
    ragdoll->blend_frame = 0;
    ragdoll->change_to_active = false;
    ragdoll->add_constraints = add_constraints;
    ragdoll->m_active_timestamp.set(30000);
    ragdoll->m_start_time.set(0);
    ragdoll->human_handle = hp->get_handle();
    if (immediate)
    {
        ragdoll->sync_to_anim = false;
        ragdoll->blend_frame = 6;
    }
    havok_ragdoll_init_relative_orients(slot, hp->char_inst);
    havok_sync_ragdoll_to_anim(slot, hp->pos, hp->orient, hp->actual_velocity, 0.0f, 1.0f, false, false, false);
    return slot;
}

// 0x00825A20 (:2808)
bool havok_ragdoll_unbind_from_human(int ragdoll_idx)
{
    my_ragdoll* ragdoll = get_ragdoll_by_index(ragdoll_idx);
    if (ragdoll == nullptr)
    {
        return false;
    }
    ragdoll->char_inst = nullptr;
    remove_ragdoll(ragdoll);
    Num_ragdolls--;
    ragdoll->bodies.remove_all();
    ragdoll->constraints.remove_all();
    return true;
}

// 0x00825A80 (:2833)
void havok_ragdoll_unsync(int ragdoll_idx, const vector& pos, const matrix& orient, const vector& vel, float force_multiplier)
{
    my_ragdoll* ragdoll = get_ragdoll_by_index(ragdoll_idx);
    if (ragdoll == nullptr || !ragdoll->sync_to_anim)
    {
        return;
    }
    ragdoll->sync_to_anim = false;
    havok_sync_ragdoll_to_anim(ragdoll_idx, pos, orient, vel, 0.0f, force_multiplier, false, false, false);
}

// 0x0081CCF0 (:2842)
// Original: no check that the slot exists.
void havok_ragdoll_add_constraints_to_world(int ragdoll_idx)
{
    my_ragdoll* ragdoll = get_ragdoll_by_index(ragdoll_idx);
    if (!ragdoll->add_constraints)
    {
        ragdoll->add_constraints = true;
        add_constraints_to_world(*ragdoll);
    }
}

// 0x008124A0 (:2851)
bool havok_ragdoll_is_synced(int ragdoll_idx)
{
    my_ragdoll* ragdoll = get_ragdoll_by_index(ragdoll_idx);
    return ragdoll != nullptr && ragdoll->sync_to_anim;
}

// 0x008124D0 (:2856)
bool havok_ragdoll_is_added_to_world(int ragdoll_idx)
{
    my_ragdoll* ragdoll = get_ragdoll_by_index(ragdoll_idx);
    return ragdoll != nullptr && ragdoll->added_to_world;
}

// 0x00825AF0 (:2862)
// Gravity is cancelled by an opposite custom force on every body.
void havok_ragdoll_set_gravity(int ragdoll_idx, bool gravity)
{
    my_ragdoll* ragdoll = get_ragdoll_by_index(ragdoll_idx);
    if (ragdoll == nullptr || gravity == ragdoll->has_gravity)
    {
        return;
    }
    // Magic number: 9.8 m/s^2, the gravity the custom force cancels
    if (gravity)
    {
        ragdoll->has_gravity = true;
        const vector accl(0.0f, -9.8f, 0.0f);
        for (int i = 0; i < NUM_RAGDOLL_BONES; i++)
        {
            havok_body_add_custom_force(ragdoll->bodies[i], accl);
        }
        return;
    }
    ragdoll->has_gravity = false;
    const vector accl(-0.0f, 9.8f, -0.0f);
    for (int i = 0; i < NUM_RAGDOLL_BONES; i++)
    {
        havok_body_add_custom_force(ragdoll->bodies[i], accl);
    }
}

// 0x00812500 (:2886)
bool havok_ragdoll_did_hit_wall(int ragdoll_idx)
{
    my_ragdoll* ragdoll = get_ragdoll_by_index(ragdoll_idx);
    return ragdoll != nullptr && ragdoll->change_to_active;
}

// 0x00812530 (:2892)
void havok_ragdoll_set_hit_wall(int ragdoll_idx)
{
    my_ragdoll* ragdoll = get_ragdoll_by_index(ragdoll_idx);
    if (ragdoll != nullptr)
    {
        ragdoll->change_to_active = true;
    }
}

// 0x00812560 (:2900)
bool havok_ragdoll_should_destroy(int ragdoll_idx)
{
    my_ragdoll* ragdoll = get_ragdoll_by_index(ragdoll_idx);
    return ragdoll != nullptr && ragdoll->should_destroy;
}

// 0x0081CD30 (:2912)
bool havok_ragdoll_is_simulating(int ragdoll_idx)
{
    my_ragdoll* ragdoll = get_ragdoll_by_index(ragdoll_idx);
    if (ragdoll == nullptr)
    {
        return false;
    }
    for (unsigned int i = 0; i < (unsigned int)ragdoll->bodies.size(); i++)
    {
        if (ragdoll->bodies[i] != nullptr && ragdoll->bodies[i]->isActive().m_bool)
        {
            return true;
        }
    }
    return false;
}

// 0x0081CDA0 (:2934)
void havok_ragdoll_set_velocity(int ragdoll_idx, const vector& velocity)
{
    my_ragdoll* ragdoll = ragdoll_idx != -1 ? get_ragdoll_by_index(ragdoll_idx) : nullptr;
    if (ragdoll == nullptr)
    {
        return;
    }
    const hkVector4f v = ragdoll_vec4(velocity);
    for (int i = 0; i < ragdoll->info.num_bones; i++)
    {
        ragdoll->bodies[i]->activate();
        ragdoll->bodies[i]->m_motion.setLinearVelocity(v);
    }
}

// 0x0081CE50 (:2972)
// Shifts every body's velocity so that their average becomes velocity.
void havok_ragdoll_set_avg_velocity(int ragdoll_idx, const vector& velocity)
{
    my_ragdoll* ragdoll = ragdoll_idx != -1 ? get_ragdoll_by_index(ragdoll_idx) : nullptr;
    if (ragdoll == nullptr)
    {
        return;
    }
    const int num_bones = ragdoll->info.num_bones;
    float sx = 0.0f;
    float sy = 0.0f;
    float sz = 0.0f;
    for (int i = 0; i < num_bones; i++)
    {
        const hkVector4f& lv = ragdoll->bodies[i]->m_motion.m_linearVelocity;
        sx = lv.m_quad.m128_f32[0] + sx;
        sy = lv.m_quad.m128_f32[1] + sy;
        sz = lv.m_quad.m128_f32[2] + sz;
    }
    hkVector4f delta;
    delta.m_quad = _mm_setr_ps(velocity.x - sx / (float)num_bones, velocity.y - sy / (float)num_bones, velocity.z - sz / (float)num_bones, 0.0f);
    for (int i = 0; i < ragdoll->info.num_bones; i++)
    {
        hkpRigidBody* rb = ragdoll->bodies[i];
        hkVector4f v;
        v.m_quad = _mm_add_ps(rb->m_motion.m_linearVelocity.m_quad, delta.m_quad);
        rb->activate();
        rb->m_motion.setLinearVelocity(v);
    }
}

// 0x0081CFE0 (:3024)
// Moves every body by the pelvis's offset to pos.
void havok_ragdoll_set_pos(int ragdoll_idx, const vector& pos)
{
    my_ragdoll* ragdoll = ragdoll_idx != -1 ? get_ragdoll_by_index(ragdoll_idx) : nullptr;
    if (ragdoll == nullptr)
    {
        return;
    }
    const hkVector4f& pelvis = ragdoll->bodies[RAGDOLL_BODY]->m_motion.m_motionState.m_transform.m_translation;
    hkVector4f delta;
    delta.m_quad = _mm_setr_ps(pos.x - pelvis.m_quad.m128_f32[0], pos.y - pelvis.m_quad.m128_f32[1], pos.z - pelvis.m_quad.m128_f32[2], 0.0f);
    for (int i = 0; i < ragdoll->info.num_bones; i++)
    {
        hkpRigidBody* rb = ragdoll->bodies[i];
        hkVector4f p;
        p.m_quad = _mm_add_ps(rb->m_motion.m_motionState.m_transform.m_translation.m_quad, delta.m_quad);
        rb->setPosition(p);
    }
}

// A velocity capped at 8 m/s (inlined in the velocity appliers).
static vector ragdoll_cap_velocity(const vector& velocity)
{
    vector v = velocity;
    if ((float)sqrt((double)(float)((double)v.x * (double)v.x + (double)v.y * (double)v.y + (double)v.z * (double)v.z)) > 8.0f) // Magic number: 8 m/s
    {
        v.normalize();
        v = vector(v.x * 8.0f, v.y * 8.0f, v.z * 8.0f);
    }
    return v;
}

// 0x0081D100 (:3061)
// The velocity as an impulse on every body (by its mass).
void havok_ragdoll_apply_velocity(int ragdoll_idx, const vector& velocity, bool cap_velocity)
{
    my_ragdoll* ragdoll = ragdoll_idx != -1 ? get_ragdoll_by_index(ragdoll_idx) : nullptr;
    if (ragdoll == nullptr)
    {
        return;
    }
    ragdoll->sync_to_anim = false;
    ragdoll->m_active_timestamp.set(30000);
    const vector v = cap_velocity ? ragdoll_cap_velocity(velocity) : velocity;
    const hkVector4f v4 = ragdoll_vec4(v);
    for (int i = 0; i < ragdoll->info.num_bones; i++)
    {
        hkpRigidBody* rb = ragdoll->bodies[i];
        const float mass = rb->m_motion.getMass();
        hkVector4f impulse;
        impulse.m_quad = _mm_mul_ps(_mm_set1_ps(mass), v4.m_quad);
        rb->activate();
        rb->m_motion.applyLinearImpulse(impulse);
    }
}

// 0x0081D2B0 (:3103)
// The (capped) velocity as an impulse at pos on every body.
void havok_ragdoll_apply_point_velocity(int ragdoll_idx, const vector& velocity, const vector& pos)
{
    my_ragdoll* ragdoll = ragdoll_idx != -1 ? get_ragdoll_by_index(ragdoll_idx) : nullptr;
    if (ragdoll == nullptr)
    {
        return;
    }
    ragdoll->sync_to_anim = false;
    ragdoll->m_active_timestamp.set(30000);
    const hkVector4f v4 = ragdoll_vec4(ragdoll_cap_velocity(velocity));
    const hkVector4f p4 = ragdoll_vec4(pos);
    for (int i = 0; i < ragdoll->info.num_bones; i++)
    {
        hkpRigidBody* rb = ragdoll->bodies[i];
        const float mass = rb->m_motion.getMass();
        hkVector4f impulse;
        impulse.m_quad = _mm_mul_ps(_mm_set1_ps(mass), v4.m_quad);
        rb->activate();
        rb->m_motion.applyPointImpulse(impulse, p4);
    }
}

// 0x0083C4F0 (:3142)
// A ray against the ragdoll's bodies only; the hit body gets the impulse along the ray at the hit, scaled by its
// flinch scaler (the arms pass it to their parent body: upper arms to the upper body, thighs to the pelvis, forearms to
// the upper arms; a quarter for a body already flinching) and the bodies it shakes start flinching. Before the
// ragdoll is in the world the ray is saved for havok_ragdoll_cast_saved_ray.
void havok_ragdoll_cast_ray(int ragdoll_idx, const vector& start_pos, const vector& end_pos, float impulse)
{
    my_ragdoll* ragdoll = ragdoll_idx != -1 ? get_ragdoll_by_index(ragdoll_idx) : nullptr;
    if (ragdoll == nullptr)
    {
        return;
    }
    if (!ragdoll->added_to_world)
    {
        ragdoll->cast_ray = true;
        ragdoll->cast_ray_start_pos = start_pos;
        ragdoll->cast_ray_end_pos = end_pos;
        ragdoll->cast_ray_impulse = impulse;
        return;
    }
    raycast_input in;
    in.start = start_pos;
    in.end = end_pos;
    in.radius = 0.0f;
    in.filter_params = nullptr;
    in.filter_callback = nullptr;
    in.m_filter_info = COLLIDE_RAY_RAGDOLL_ONLY;
    in.m_hit_collector_type = raycast_input::COLLECT_CLOSEST_HIT;
    in.m_flags = 0;
    raycast_output out;
    if (!havok_raycast(in, out))
    {
        return;
    }
    ragdoll->sync_to_anim = false;
    ragdoll->m_active_timestamp.set(30000);
    const raycast_output::hit_info& hit = out.m_hits[0];
    hkpEntity* hit_body = hit.hit_havok_handle == 0xFFFFFFFF ? nullptr : Hk_rigid_bodies[hit.hit_havok_handle];
    const hkVector4f hit_pos = ragdoll_vec4(hit.hit_pos);
    vector dir(end_pos.x - start_pos.x, end_pos.y - start_pos.y, end_pos.z - start_pos.z);
    dir.normalize_safe();
    dir = vector(dir.x * impulse, dir.y * impulse, dir.z * impulse);
    for (int bone = 0; bone < NUM_RAGDOLL_BONES; bone++)
    {
        if (ragdoll->bodies[bone] != hit_body)
        {
            continue;
        }
        int target = bone;
        if (bone == RAGDOLL_UR_ARM || bone == RAGDOLL_UL_ARM)
        {
            target = RAGDOLL_UPPER_BODY;
        }
        if (bone == RAGDOLL_UR_LEG || bone == RAGDOLL_UL_LEG)
        {
            target = RAGDOLL_BODY;
        }
        if (bone == RAGDOLL_LR_ARM)
        {
            target = RAGDOLL_UR_ARM;
        }
        else if (bone == RAGDOLL_LL_ARM)
        {
            target = RAGDOLL_UL_ARM;
        }
        if (ragdoll->bone_flinch_state[bone])
        {
            dir = vector(dir.x * 0.25f, dir.y * 0.25f, dir.z * 0.25f); // Magic number: a quarter for a flinching body
        }
        ragdoll->bone_flinch_state[bone] = true;
        ragdoll->bone_flinch_blend[bone] = 0.25f;
        if (ragdoll->bone_flinch_state[target] && ragdoll->bone_flinch_blend[target] > 0.5f)
        {
            ragdoll->bone_flinch_blend[target] = 0.0f;
        }
        const float scaler = Ragdoll_flinch_impulse_scaler[target];
        dir = vector(dir.x * scaler, dir.y * scaler, dir.z * scaler);
        hit_body = ragdoll->bodies[target];
        for (int k = 0; k < 9; k++)
        {
            const int flinch = Ragdoll_flinch_bones[bone][k];
            if (flinch != -1)
            {
                ragdoll->bone_flinch_state[flinch] = true;
                ragdoll->bone_flinch_blend[flinch] = 0.0f;
            }
        }
        break;
    }
    hkVector4f imp;
    imp.m_quad = _mm_setr_ps(dir.x, dir.y, dir.z, 0.0f);
    hit_body->activate();
    ((hkpRigidBody*)hit_body)->m_motion.applyPointImpulse(imp, hit_pos);
}

// 0x00825BE0 (:3230)
// Original bug kept: the saved ray start is applied as a point impulse at the saved end (see the file comment).
void havok_ragdoll_cast_saved_ray(int ragdoll_idx)
{
    my_ragdoll* ragdoll = get_ragdoll_by_index(ragdoll_idx);
    if (ragdoll == nullptr || !ragdoll->cast_ray)
    {
        return;
    }
    havok_ragdoll_apply_point_impulse(ragdoll_idx, ragdoll->cast_ray_start_pos, ragdoll->cast_ray_end_pos);
    ragdoll->cast_ray = false;
}

// 0x0081D480 (:3240)
// The impulse at pos on every body, by its mass and a falloff (1 - d^2 / (|impulse| / 50 + 0.4), clamped to [0, 1]);
// 1.05x for the body closest to pos, 0.95x for the others; bodies taking more than 0.2 start flinching. Before the
// ragdoll is in the world the impulses are summed for later (the first position kept).
void havok_ragdoll_apply_point_impulse(int ragdoll_idx, const vector& impulse, const vector& pos)
{
    my_ragdoll* ragdoll = ragdoll_idx != -1 ? get_ragdoll_by_index(ragdoll_idx) : nullptr;
    if (ragdoll == nullptr)
    {
        return;
    }
    const float magnitude = (float)sqrt((double)(float)((double)impulse.x * (double)impulse.x + (double)impulse.y * (double)impulse.y + (double)impulse.z * (double)impulse.z));
    if (!ragdoll->added_to_world)
    {
        if (ragdoll->cast_ray)
        {
            ragdoll->cast_ray_start_pos.x = impulse.x + ragdoll->cast_ray_start_pos.x;
            ragdoll->cast_ray_start_pos.y = ragdoll->cast_ray_start_pos.y + impulse.y;
            ragdoll->cast_ray_start_pos.z = impulse.z + ragdoll->cast_ray_start_pos.z;
            return;
        }
        ragdoll->cast_ray = true;
        ragdoll->cast_ray_start_pos = impulse;
        ragdoll->cast_ray_end_pos = pos;
        return;
    }
    ragdoll->sync_to_anim = false;
    ragdoll->m_active_timestamp.set(30000);
    const hkVector4f imp4 = ragdoll_vec4(impulse);
    const hkVector4f pos4 = ragdoll_vec4(pos);
    // the body closest to pos (by its center of mass)
    float best = 1.0e7f; // 0x4B189680
    hkpRigidBody* closest = ragdoll->bodies[0];
    for (int i = 0; i < ragdoll->info.num_bones; i++)
    {
        hkpRigidBody* rb = ragdoll->bodies[i];
        const hkVector4f& com = rb->m_motion.m_motionState.m_sweptTransform.m_centerOfMass1;
        const float dx = com.m_quad.m128_f32[0] - pos.x;
        const float dy = com.m_quad.m128_f32[1] - pos.y;
        const float dz = com.m_quad.m128_f32[2] - pos.z;
        const float d2 = (float)((double)dx * (double)dx + (double)dy * (double)dy + (double)dz * (double)dz);
        if ((double)d2 < (double)best)
        {
            best = d2;
            closest = rb;
        }
    }
    // Magic numbers: the falloff radius^2 grows by 1 per 50 units of impulse from 0.4
    const double falloff = (double)magnitude / 50.0 + (double)0.4f;
    for (int i = 0; i < ragdoll->info.num_bones; i++)
    {
        hkpRigidBody* rb = ragdoll->bodies[i];
        const hkVector4f& com = rb->m_motion.m_motionState.m_sweptTransform.m_centerOfMass1;
        const float dx = com.m_quad.m128_f32[0] - pos.x;
        const float dy = com.m_quad.m128_f32[1] - pos.y;
        const float dz = com.m_quad.m128_f32[2] - pos.z;
        const float d2 = (float)((double)dx * (double)dx + (double)dy * (double)dy + (double)dz * (double)dz);
        float scale = (float)(1.0 - (double)d2 / falloff);
        if ((float)((double)scale - 0.0) < 0.0f)
        {
            scale = 0.0f;
        }
        if ((float)((double)scale - 1.0) >= 0.0f)
        {
            scale = 1.0f;
        }
        const float body_scale = rb->m_motion.getMass() * scale;
        if ((double)body_scale > (double)0.2f) // Magic number: the scaled mass from which a body flinches
        {
            ragdoll->bone_flinch_state[i] = true;
            ragdoll->bone_flinch_blend[i] = 0.0f;
        }
        // Magic numbers: 1.05 for the closest body, 0.95 for the others (as doubles of the float constants)
        const float s = (float)((double)body_scale * (rb == closest ? (double)1.05f : (double)0.95f));
        hkVector4f body_impulse;
        body_impulse.m_quad = _mm_mul_ps(_mm_set1_ps(s), imp4.m_quad);
        rb->activate();
        rb->m_motion.applyPointImpulse(body_impulse, pos4);
    }
}

// 0x0081D910 (:3328)
// The whole impulse on the body closest to pos.
void havok_ragdoll_apply_impulse_to_single_bone(int ragdoll_idx, const vector& impulse, const vector& pos)
{
    my_ragdoll* ragdoll = ragdoll_idx != -1 ? get_ragdoll_by_index(ragdoll_idx) : nullptr;
    if (ragdoll == nullptr)
    {
        return;
    }
    ragdoll->sync_to_anim = false;
    ragdoll->m_active_timestamp.set(30000);
    const hkVector4f imp4 = ragdoll_vec4(impulse);
    const hkVector4f pos4 = ragdoll_vec4(pos);
    float best = 1.0e7f;
    hkpRigidBody* closest = ragdoll->bodies[0];
    for (int i = 0; i < ragdoll->info.num_bones; i++)
    {
        hkpRigidBody* rb = ragdoll->bodies[i];
        const hkVector4f& com = rb->m_motion.m_motionState.m_sweptTransform.m_centerOfMass1;
        const float dx = com.m_quad.m128_f32[0] - pos.x;
        const float dy = com.m_quad.m128_f32[1] - pos.y;
        const float dz = com.m_quad.m128_f32[2] - pos.z;
        const float d2 = (float)((double)dx * (double)dx + (double)dy * (double)dy + (double)dz * (double)dz);
        if ((double)d2 < (double)best)
        {
            closest = rb;
            best = d2;
        }
    }
    closest->activate();
    closest->m_motion.applyPointImpulse(imp4, pos4);
}

// 0x0081DAB0 (:3382)
// A spin about the bodies' average center of mass: the bodies of body_part_filter (bit per body) get mass x
// (angular_velocity x offset) as a linear impulse.
void havok_ragdoll_apply_angular_velocity(int ragdoll_idx, const vector& angular_velocity, unsigned int body_part_filter)
{
    my_ragdoll* ragdoll = ragdoll_idx != -1 ? get_ragdoll_by_index(ragdoll_idx) : nullptr;
    if (ragdoll == nullptr)
    {
        return;
    }
    ragdoll->sync_to_anim = false;
    ragdoll->m_active_timestamp.set(30000);
    const unsigned int num_bones = (unsigned int)ragdoll->info.num_bones;
    Q center = _mm_setzero_ps();
    for (unsigned int i = 0; i < num_bones; i++)
    {
        center = _mm_add_ps(center, ragdoll->bodies[i]->m_motion.m_motionState.m_sweptTransform.m_centerOfMass1.m_quad);
    }
    center = _mm_mul_ps(_mm_set1_ps(1.0f / (float)num_bones), center);
    const Q omega = _mm_setr_ps(angular_velocity.x, angular_velocity.y, angular_velocity.z, 0.0f);
    unsigned int bit = 1;
    for (unsigned int i = 0; i < num_bones; i++, bit = (bit << 1) | (bit >> 31))
    {
        if ((bit & body_part_filter) == 0)
        {
            continue;
        }
        hkpRigidBody* rb = ragdoll->bodies[i];
        const Q offset = _mm_sub_ps(rb->m_motion.m_motionState.m_sweptTransform.m_centerOfMass1.m_quad, center);
        const float mass = rb->m_motion.getMass();
        hkVector4f impulse;
        impulse.m_quad = _mm_mul_ps(_mm_set1_ps(mass), cross(omega, offset));
        rb->activate();
        rb->m_motion.applyLinearImpulse(impulse);
    }
}

// 0x0081DC70 (:3454)
// A random angular impulse on every body (its mass x jerk_scale).
void havok_ragdoll_jerk(int ragdoll_idx, float jerk_scale)
{
    my_ragdoll* ragdoll = get_ragdoll_by_index(ragdoll_idx);
    if (ragdoll == nullptr)
    {
        return;
    }
    ragdoll->sync_to_anim = false;
    ragdoll->m_active_timestamp.set(30000);
    for (int i = 0; i < ragdoll->info.num_bones; i++)
    {
        hkpRigidBody* rb = ragdoll->bodies[i];
        vector v;
        v.rand_sphere();
        const float s = ragdoll->info.mass[i] * jerk_scale;
        hkVector4f impulse;
        impulse.m_quad = _mm_setr_ps(v.x * s, v.y * s, v.z * s, 0.0f);
        rb->activate();
        rb->m_motion.applyAngularImpulse(impulse);
    }
}

// ---- animation
// 0x0081DDB0 (:3489)
// Puts the bodies (pelvis first, then 2..11, then the head) where the animation has their bones: pct blends towards
// the bodies' current state (positions and orientations by a slerp, velocities towards vel and zero spin). A ragdoll
// that was not following the animation, with dont_bash_positions, is driven there by velocities over one frame
// instead. use_ragdoll_pos keeps the pelvis as the character position and leaves the velocities alone.
void havok_sync_ragdoll_to_anim(int ragdoll_idx, const vector& char_pos, const matrix& char_orient, const vector& vel, float pct, float force_multiplier, bool preserve_anim_velocities_very_slow, bool use_ragdoll_pos, bool dont_bash_positions)
{
    my_ragdoll* ragdoll = get_ragdoll_by_index(ragdoll_idx);
    if (ragdoll == nullptr)
    {
        return;
    }
    vector pos = char_pos;
    matrix43* matrices = nullptr;
    const bool have_matrices = animlib_get_instanced_model_matrices(ragdoll->char_inst->aim_handle, &matrices);
    const hkVector4f vel4 = ragdoll_vec4(vel);
    const bool was_synced = ragdoll->sync_to_anim;
    ragdoll->sync_to_anim = false;
    // preserve_anim_velocities_very_slow with pct 0 fetches the animation's bone velocities (animlib_get_bone_velocities)
    // but nothing reads them; not ported.
    static const int order[12] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 0};
    for (int n = 0; n < ragdoll->info.num_bones; n++)
    {
        const int i = order[n];
        hkpRigidBody* rb = ragdoll->bodies[i];
        if (rb == nullptr)
        {
            continue;
        }
        if (i == RAGDOLL_BODY && use_ragdoll_pos)
        {
            pos = ragdoll_vec3(rb->m_motion.m_motionState.m_transform.m_translation);
        }
        // (the pelvis matrix from the bone end computed here in the binary is overwritten below)
        matrix43 m;
        if (have_matrices && matrices != nullptr)
        {
            m = matrices[ragdoll->info.bone_start_idx[i]];
        }
        else if (ragdoll->char_inst->rdd.is_loaded())
        {
            m.m_rotation.make_identity();
            const vector& inv = ragdoll->char_inst->rdd.get_data()->base_character->rig->bones[ragdoll->info.bone_start_idx[i]].inv_translation;
            m.m_translation = vector(-inv.x, -inv.y, -inv.z);
        }
        else
        {
            m.make_identity();
        }
        m.m_translation.unrotate(char_orient);
        m.m_translation = vector(m.m_translation.x + pos.x, m.m_translation.y + pos.y, m.m_translation.z + pos.z);
        m.m_rotation = m.m_rotation * char_orient;
        m.m_rotation.orthogonalize();
        if (!was_synced && dont_bash_positions)
        {
            const hkVector4f& t = rb->m_motion.m_motionState.m_transform.m_translation;
            const hkQuaternionf rot = rb->m_motion.m_motionState.m_sweptTransform.m_rotation1;
            const float inv_dt = (float)(1.0 / (double)Frametime);
            hkVector4f lin;
            lin.m_quad = _mm_setr_ps((m.m_translation.x - t.m_quad.m128_f32[0]) * inv_dt, (m.m_translation.y - t.m_quad.m128_f32[1]) * inv_dt, (m.m_translation.z - t.m_quad.m128_f32[2]) * inv_dt, 0.0f);
            rb->activate();
            rb->m_motion.setLinearVelocity(lin);
            quaternion q;
            q.x = rot.m_vec.m_quad.m128_f32[0];
            q.y = rot.m_vec.m_quad.m128_f32[1];
            q.z = rot.m_vec.m_quad.m128_f32[2];
            q.w = -rot.m_vec.m_quad.m128_f32[3];
            matrix body_orient;
            q.extract_matrix(body_orient);
            const matrix rel = body_orient.copy_inverse() * m.m_rotation;
            float p;
            float b;
            float h;
            rel.extract_angles(p, b, h);
            const float inv_dt2 = (float)(1.0 / (double)Frametime);
            hkVector4f ang;
            ang.m_quad = _mm_setr_ps(p * inv_dt2, h * inv_dt2, b * inv_dt2, 0.0f);
            rb->activate();
            rb->m_motion.setAngularVelocity(ang);
            continue;
        }
        hkVector4f new_pos;
        new_pos.m_quad = _mm_setr_ps(m.m_translation.x, m.m_translation.y, m.m_translation.z, 0.0f);
        const quaternion vq(m.m_rotation);
        hkQuaternionf new_rot;
        new_rot.m_vec.m_quad = _mm_setr_ps(vq.x, vq.y, vq.z, -vq.w);
        if (pct != 0.0f)
        {
            const Q pct4 = _mm_set1_ps(pct);
            new_pos.m_quad = _mm_add_ps(_mm_mul_ps(_mm_sub_ps(rb->m_motion.m_motionState.m_transform.m_translation.m_quad, new_pos.m_quad), pct4), new_pos.m_quad);
            hkSimdFloat32 t;
            t.m_real = pct4;
            new_rot.setSlerp(new_rot, rb->m_motion.m_motionState.m_sweptTransform.m_rotation1, t);
        }
        rb->setPosition(new_pos);
        rb->setRotation(new_rot);
        if (use_ragdoll_pos)
        {
            continue;
        }
        const Q pct4 = _mm_set1_ps(pct);
        const Q zero = _mm_setzero_ps();
        hkVector4f ang;
        ang.m_quad = _mm_add_ps(_mm_mul_ps(_mm_sub_ps(rb->m_motion.m_angularVelocity.m_quad, zero), pct4), zero);
        hkVector4f lin;
        lin.m_quad = _mm_add_ps(_mm_mul_ps(_mm_sub_ps(rb->m_motion.m_linearVelocity.m_quad, vel4.m_quad), pct4), vel4.m_quad);
        rb->activate();
        rb->m_motion.setAngularVelocity(ang);
        rb->activate();
        rb->m_motion.setLinearVelocity(lin);
    }
}

// A body's model matrix from the ragdoll (shared by havok_sync_anim_to_ragdoll and havok_blend_anim_to_ragdoll):
// world position and rotation back into the character's space (raised 1/8 m while following the animation), scaled.
static void ragdoll_body_to_model(my_ragdoll& ragdoll, character_instance* cip, matrix43& model, const vector& world_pos, const matrix& world_rot, const vector& char_pos, const matrix& char_orient)
{
    vector local(world_pos.x - char_pos.x, world_pos.y - char_pos.y, world_pos.z - char_pos.z);
    if (ragdoll.sync_to_anim)
    {
        // Magic number: 0.125 m up (in the character's space)
        const vector up = vector(0.0f, 0.125f, 0.0f).copy_rotate(char_orient);
        local = vector(up.x + local.x, up.y + local.y, up.z + local.z);
    }
    local.rotate(char_orient);
    const matrix rot = world_rot * char_orient.copy_transpose();
    model.set_translation(local);
    model.set_orientation(rot);
    const float scale = cip->character_scale;
    if (scale != 1.0f)
    {
        model.m_rotation.rvec = vector(model.m_rotation.rvec.x * scale, model.m_rotation.rvec.y * scale, model.m_rotation.rvec.z * scale);
        model.m_rotation.uvec = vector(model.m_rotation.uvec.x * scale, model.m_rotation.uvec.y * scale, model.m_rotation.uvec.z * scale);
        model.m_rotation.fvec = vector(model.m_rotation.fvec.x * scale, model.m_rotation.fvec.y * scale, model.m_rotation.fvec.z * scale);
    }
}

// 0x0081E6C0 (:3702)
// Writes the ragdoll into the instance: bones with a body take its transform (a child body keeps its bind distance
// from its parent body), the others their relative transform below their parent (eased to the bind pose: half way
// each call, until the orient is within acos 0.98 of it). The vertex-to-model matrices follow.
void havok_sync_anim_to_ragdoll(int idx, const vector& char_pos, const matrix& char_orient)
{
    if (idx < 0 || idx >= Max_ragdolls)
    {
        return;
    }
    my_ragdoll* ragdoll = Ragdolls[idx];
    if (ragdoll == nullptr)
    {
        return;
    }
    if (ragdoll->sync_to_anim)
    {
        // still following the animation: the bodies are posed from it and the instance is left alone
        const vector zero(0.0f, 0.0f, 0.0f);
        havok_sync_ragdoll_to_anim(idx, char_pos, char_orient, zero, 0.0f, 1.0f, false, false, false);
        return;
    }
    character_instance* cip = ragdoll->char_inst;
    animlib_clear_non_looping_animations(cip->aim_handle);
    matrix43* model = nullptr;
    animlib_get_instanced_model_matrices(cip->aim_handle, &model);
    if (model == nullptr)
    {
        return;
    }
    matrix43* vertex_to_model = animlib_get_vertex_to_model_matrices(cip->aim_handle);
    const int num_bones = animlib_get_num_bones(cip->aim_handle);
    for (int bone = 0; bone < num_bones; bone++)
    {
        const int body = ragdoll->anim_to_ragdoll_bone[bone];
        const int parent = animlib_get_bone_parent_index(cip->aim_handle, bone);
        const matrix43 inv_bind = animlib_get_bone_inv_transformation(cip->aim_handle, bone);
        if (body == -1)
        {
            const vector rel_translation = animlib_get_bone_rel_translation(cip->aim_handle, bone);
            if (parent != -1)
            {
                if (ragdoll->interpolate_relative_orient[bone])
                {
                    const quaternion current(ragdoll->relative_orient[bone]);
                    quaternion q;
                    pltind_slerp(q, current, IDENTITY_QUATERNION, 0.5f);
                    if (q.w > 0.98f) // Magic number: close enough to the bind pose
                    {
                        ragdoll->interpolate_relative_orient[bone] = false;
                    }
                    q.extract_matrix(ragdoll->relative_orient[bone]);
                    vector& off = ragdoll->relative_offset[bone];
                    off = vector(off.x + (rel_translation.x - off.x) * 0.5f, (rel_translation.y - off.y) * 0.5f + off.y, off.z + (rel_translation.z - off.z) * 0.5f);
                }
                model[bone].set_orientation(ragdoll->relative_orient[bone]);
                model[bone].set_translation(ragdoll->relative_offset[bone]);
                model[bone] = model[bone] * model[parent];
            }
        }
        else
        {
            hkpRigidBody* rb = ragdoll->bodies[body];
            vector world_pos = ragdoll_vec3(rb->m_motion.m_motionState.m_transform.m_translation);
            const int parent_body = Ragdoll_rigid_body_parent_ids[body];
            if (parent_body >= 0)
            {
                // keep the bind distance from the parent body along the current direction
                const vector parent_pos = ragdoll_vec3(ragdoll->bodies[parent_body]->m_motion.m_motionState.m_transform.m_translation);
                vector dir(world_pos.x - parent_pos.x, world_pos.y - parent_pos.y, world_pos.z - parent_pos.z);
                dir.normalize();
                const matrix43 parent_inv_bind = animlib_get_bone_inv_transformation(cip->aim_handle, ragdoll->info.bone_start_idx[parent_body]);
                const float dx = inv_bind.m_translation.x - parent_inv_bind.m_translation.x;
                const float dy = inv_bind.m_translation.y - parent_inv_bind.m_translation.y;
                const float dz = inv_bind.m_translation.z - parent_inv_bind.m_translation.z;
                const float length = sqrtf(dy * dy + dx * dx + dz * dz);
                world_pos = vector(parent_pos.x + dir.x * length, parent_pos.y + dir.y * length, parent_pos.z + dir.z * length);
            }
            matrix world_rot;
            ragdoll_rotation_to_matrix(rb->m_motion.m_motionState.m_transform.m_rotation, world_rot);
            ragdoll_body_to_model(*ragdoll, cip, model[bone], world_pos, world_rot, char_pos, char_orient);
        }
        vertex_to_model[bone] = inv_bind * model[bone];
    }
}

// 0x0081F240 (:3846)
// Like havok_sync_anim_to_ragdoll, blended with the animation by blend_pct (1: the ragdoll): bodies by position lerp
// and orientation slerp; the other bones keep their offset from the parent's new transform, eased between their
// relative orient and the animation's.
void havok_blend_anim_to_ragdoll(int idx, const vector& char_pos, const matrix& char_orient, float blend_pct)
{
    my_ragdoll* ragdoll = get_ragdoll_by_index(idx);
    if (ragdoll == nullptr)
    {
        return;
    }
    character_instance* cip = ragdoll->char_inst;
    matrix43* model = nullptr;
    animlib_get_instanced_model_matrices(cip->aim_handle, &model);
    if (model == nullptr)
    {
        return;
    }
    matrix43* vertex_to_model = animlib_get_vertex_to_model_matrices(cip->aim_handle);
    const int num_bones = animlib_get_num_bones(cip->aim_handle);
    // the inverse of each body-less bone's parent model matrix before the blend (rotation inverted, translation only
    // negated)
    matrix43 parent_inverse[64];
    for (int bone = 0; bone < num_bones; bone++)
    {
        const int parent = animlib_get_bone_parent_index(cip->aim_handle, bone);
        if (ragdoll->anim_to_ragdoll_bone[bone] == -1 && parent != -1)
        {
            parent_inverse[bone] = model[parent];
            parent_inverse[bone].m_rotation.inverse();
            parent_inverse[bone].m_translation = vector(-parent_inverse[bone].m_translation.x, -parent_inverse[bone].m_translation.y, -parent_inverse[bone].m_translation.z);
        }
    }
    for (int bone = 0; bone < num_bones; bone++)
    {
        const int body = ragdoll->anim_to_ragdoll_bone[bone];
        const int parent = animlib_get_bone_parent_index(cip->aim_handle, bone);
        const vector rel_translation = animlib_get_bone_rel_translation(cip->aim_handle, bone);
        const matrix43 inv_bind = animlib_get_bone_inv_transformation(cip->aim_handle, bone);
        if (body == -1)
        {
            if (parent == -1)
            {
                const quaternion relative(ragdoll->relative_orient[bone]);
                matrix anim_orient;
                model[bone].get_orientation(anim_orient);
                const quaternion anim(anim_orient);
                const float t = 1.0f - blend_pct;
                quaternion q;
                pltind_slerp(q, relative, anim, t);
                const vector& off = ragdoll->relative_offset[bone];
                const vector translation(off.x + (rel_translation.x - off.x) * t, (rel_translation.y - off.y) * t + off.y, (rel_translation.z - off.z) * t + off.z);
                matrix orient;
                q.extract_matrix(orient);
                model[bone].set_orientation(orient);
                model[bone].set_translation(translation);
            }
            else
            {
                const matrix parent_turn = model[parent].m_rotation * parent_inverse[bone].m_rotation;
                const vector t_old(parent_inverse[bone].m_translation.x + model[bone].m_translation.x, parent_inverse[bone].m_translation.y + model[bone].m_translation.y, model[bone].m_translation.z + parent_inverse[bone].m_translation.z);
                const matrix anim_rel = model[bone].m_rotation * parent_inverse[bone].m_rotation;
                const quaternion anim(anim_rel);
                const quaternion relative(ragdoll->relative_orient[bone]);
                quaternion q;
                pltind_slerp(q, anim, relative, blend_pct);
                q.extract_matrix(model[bone].m_rotation);
                model[bone].m_translation = ragdoll->relative_offset[bone];
                model[bone] = model[bone] * model[parent];
                const float t = 1.0f - blend_pct;
                const matrix43& p = model[parent];
                vector& out = model[bone].m_translation;
                const float x = blend_pct * out.x + (p.m_translation.x + parent_turn.rvec.x * t_old.x + parent_turn.uvec.x * t_old.y + parent_turn.fvec.x * t_old.z) * t;
                const float z = out.z * blend_pct + (p.m_translation.z + parent_turn.rvec.z * t_old.x + parent_turn.uvec.z * t_old.y + parent_turn.fvec.z * t_old.z) * t;
                const float y = out.y * blend_pct + (p.m_translation.y + parent_turn.rvec.y * t_old.x + parent_turn.uvec.y * t_old.y + parent_turn.fvec.y * t_old.z) * t;
                out = vector(x, y, z);
            }
        }
        else
        {
            hkpRigidBody* rb = ragdoll->bodies[body];
            vector anim_pos;
            model[bone].get_translation(anim_pos);
            anim_pos.unrotate(char_orient);
            anim_pos = vector(char_pos.x + anim_pos.x, char_pos.y + anim_pos.y, char_pos.z + anim_pos.z);
            matrix anim_orient;
            model[bone].get_orientation(anim_orient);
            if (cip->character_scale != 1.0f)
            {
                // (the model matrix is unscaled in place; the orientation taken above stays scaled until orthogonalized)
                const float inv = 1.0f / cip->character_scale;
                model[bone].m_rotation.rvec = vector(model[bone].m_rotation.rvec.x * inv, inv * model[bone].m_rotation.rvec.y, model[bone].m_rotation.rvec.z * inv);
                model[bone].m_rotation.uvec = vector(inv * model[bone].m_rotation.uvec.x, inv * model[bone].m_rotation.uvec.y, model[bone].m_rotation.uvec.z * inv);
                model[bone].m_rotation.fvec = vector(inv * model[bone].m_rotation.fvec.x, model[bone].m_rotation.fvec.y * inv, inv * model[bone].m_rotation.fvec.z);
            }
            matrix world_anim = anim_orient * char_orient;
            world_anim.orthogonalize();
            const quaternion vq(world_anim);
            hkQuaternionf anim_rot;
            anim_rot.m_vec.m_quad = _mm_setr_ps(vq.x, vq.y, vq.z, -vq.w);
            Q pos4 = _mm_setr_ps(anim_pos.x, anim_pos.y, anim_pos.z, 0.0f);
            hkQuaternionf blended;
            const hkQuaternionf* rot = &anim_rot;
            if (blend_pct > 0.0f)
            {
                if (blend_pct < 1.0f)
                {
                    const Q pct4 = _mm_set1_ps(blend_pct);
                    pos4 = _mm_add_ps(_mm_mul_ps(_mm_sub_ps(rb->m_motion.m_motionState.m_transform.m_translation.m_quad, pos4), pct4), pos4);
                    hkSimdFloat32 t;
                    t.m_real = pct4;
                    blended.setSlerp(anim_rot, rb->m_motion.m_motionState.m_sweptTransform.m_rotation1, t);
                    rot = &blended;
                }
                else
                {
                    pos4 = rb->m_motion.m_motionState.m_transform.m_translation.m_quad;
                    rot = &rb->m_motion.m_motionState.m_sweptTransform.m_rotation1;
                }
            }
            hkRotationf r;
            r.set(*rot);
            matrix world_rot;
            ragdoll_rotation_to_matrix(r, world_rot);
            const vector world_pos(pos4.m128_f32[0], pos4.m128_f32[1], pos4.m128_f32[2]);
            ragdoll_body_to_model(*ragdoll, cip, model[bone], world_pos, world_rot, char_pos, char_orient);
        }
        vertex_to_model[bone] = inv_bind * model[bone];
    }
}

// ---- state
// 0x00820580 (:4126)
// Active while following the animation or not in the world yet, else while a body simulates.
bool havok_ragdoll_is_active(int ragdoll_idx)
{
    my_ragdoll* ragdoll = get_ragdoll_by_index(ragdoll_idx);
    if (ragdoll == nullptr)
    {
        return false;
    }
    if (ragdoll->added_to_world && !ragdoll->sync_to_anim)
    {
        return havok_ragdoll_is_simulating(ragdoll_idx);
    }
    return true;
}

// 0x008205D0 (:4160)
// The bodies stop colliding with handle's object (for time_ms; negative: until removed).
void havok_ragdoll_set_ignore_property(int ragdoll_idx, unsigned int handle, int time_ms)
{
    my_ragdoll* ragdoll = get_ragdoll_by_index(ragdoll_idx);
    if (ragdoll == nullptr)
    {
        return;
    }
    if (time_ms < 0)
    {
        ragdoll->ignore_property_ts.invalidate();
    }
    else
    {
        ragdoll->ignore_property_ts.set(time_ms);
    }
    ragdoll->ignore_handle = handle;
    if (!ragdoll->added_to_world)
    {
        return;
    }
    for (int i = 0; i < NUM_RAGDOLL_BONES; i++)
    {
        hkpRigidBody* rb = ragdoll->bodies[i];
        if (rb == nullptr)
        {
            keen::debug::nativePrintArgs("havok_ragdoll_set_ignore_property: ragdoll->bodies[%d] == nullptr!\n", i);
            continue;
        }
        if (!rb->hasProperty(HAVOK_PROPERTY_IGNORE_HANDLE, hkpWorldObject::MULTI_THREADING_CHECKS_ENABLE))
        {
            havok_set_ignore_property(rb, handle);
        }
    }
}

// 0x00820690 (:4194)
void havok_ragdoll_remove_ignore_property(int ragdoll_idx)
{
    my_ragdoll* ragdoll = get_ragdoll_by_index(ragdoll_idx);
    if (ragdoll == nullptr)
    {
        return;
    }
    for (int i = 0; i < NUM_RAGDOLL_BONES; i++)
    {
        hkpRigidBody* rb = ragdoll->bodies[i];
        if (rb != nullptr && rb->hasProperty(HAVOK_PROPERTY_IGNORE_HANDLE, hkpWorldObject::MULTI_THREADING_CHECKS_ENABLE))
        {
            havok_remove_ignore_property(rb);
        }
    }
    ragdoll->ignore_property_ts.invalidate();
}

// 0x00812590 (:4217)
void havok_ragdoll_reset_active_timer(int ragdoll_idx)
{
    my_ragdoll* ragdoll = get_ragdoll_by_index(ragdoll_idx);
    if (ragdoll != nullptr)
    {
        ragdoll->m_active_timestamp.set(30000);
    }
}

// 0x0083E440 (:4236)
void havok_ragdoll_add_to_world(unsigned int ragdoll_idx)
{
    my_ragdoll* ragdoll = get_ragdoll_by_index((int)ragdoll_idx);
    if (ragdoll != nullptr && !ragdoll->added_to_world)
    {
        add_ragdoll(*ragdoll);
    }
}

// 0x0083E470 (:4247)
// Every bound ragdoll: contact hits decay by 15, the collision filter (an active-on-impact ragdoll collides like its
// human, the human filters becoming COLLIDE_RAGDOLL), added to the world (else its joints), the ignore property
// expired, the joint limits' tau blended in over RAGDOLL_BLEND_TAU_TIME ms, and the supplemental constraints added
// with their faked bodies moved to their tags.
void havok_ragdoll_process_pre()
{
    for (int ragdoll_idx = 0; ragdoll_idx < Max_ragdolls; ragdoll_idx++)
    {
        my_ragdoll* ragdoll = get_ragdoll_by_index(ragdoll_idx);
        ragdoll->change_to_active = false;
        if (ragdoll->char_inst == nullptr)
        {
            continue;
        }
        ragdoll->contact_hits -= 15; // Magic number: contact hits forgiven per frame
        if (ragdoll->contact_hits < 0)
        {
            ragdoll->contact_hits = 0;
        }
        human* hp = ragdoll_human_from_handle(ragdoll->human_handle);
        unsigned int collision = COLLIDE_RAGDOLL;
        bool set_filter = true;
        if (hp->ragdoll_state == HUMAN_RAGDOLL_STATE_ACTIVE_ON_IMPACT)
        {
            unsigned int filter;
            if (human_get_collision_filter(filter, hp))
            {
                if (filter == COLLIDE_NO_COLLISION || filter == COLLIDE_NPC || filter == COLLIDE_PLAYER || filter == COLLIDE_DEAD_HUMAN)
                {
                    collision = COLLIDE_RAGDOLL;
                }
                else
                {
                    collision = filter;
                }
            }
            else
            {
                set_filter = false;
            }
        }
        if (set_filter)
        {
            havok_ragdoll_set_collision_filter(ragdoll_idx, collision);
        }
        if (!ragdoll->added_to_world)
        {
            add_ragdoll(*ragdoll);
            continue;
        }
        if (!ragdoll->add_constraints)
        {
            ragdoll->add_constraints = true;
            add_constraints_to_world(*ragdoll);
        }
        if (ragdoll->ignore_property_ts.valid() && ragdoll->ignore_property_ts.elapsed())
        {
            havok_ragdoll_remove_ignore_property(ragdoll_idx);
        }
        const float tau = fl_parameter((float)ragdoll->m_start_time.time_since(), 0.0f, (float)RAGDOLL_BLEND_TAU_TIME, true);
        const int num_constraints = ragdoll->constraints.size();
        for (int i = 0; i < num_constraints; i++)
        {
            hkpConstraintData* data = ragdoll->constraints[i]->m_data;
            const int type = data->getType();
            if (type == 2) // hkpConstraintData::CONSTRAINT_TYPE_LIMITEDHINGE
            {
                ((hkpLimitedHingeConstraintData*)data)->setAngularLimitsTauFactor(tau);
            }
            else if (type == 7) // hkpConstraintData::CONSTRAINT_TYPE_RAGDOLL
            {
                hkpRagdollConstraintData* rd = (hkpRagdollConstraintData*)data;
                rd->m_atoms.m_twistLimit.m_angularLimitsTauFactor = tau;
                rd->m_atoms.m_coneLimit.m_angularLimitsTauFactor = tau;
                rd->m_atoms.m_planesLimit.m_angularLimitsTauFactor = tau;
            }
            else
            {
                keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
            }
        }
        for (int i = 0; i < 4; i++)
        {
            supplemental_constraint& scp = ragdoll->ext_constraint[i];
            if (!scp.m_flags.active)
            {
                continue;
            }
            if (!scp.m_flags.constraint_added)
            {
                Havok_world->addConstraint(scp.m_constraint);
                scp.m_flags.constraint_added = 1;
            }
            if (scp.m_flags.faked_body && scp.handle != 0xFFFFFFFF)
            {
                human* tagged = ragdoll_human_from_handle(scp.handle);
                vector pos;
                animlib_get_tag_world_coords(tagged->char_inst->aim_handle, scp.tag_index, tagged->pos, tagged->orient, pos, nullptr);
                scp.m_body_b->setPosition(ragdoll_vec4(pos));
            }
        }
    }
}

// 0x00825C50 (:4353)
// Detached limbs leave the world with their child body (arms 9/11 and 8/10, legs 5/7 and 4/6).
void havok_ragdoll_process_post()
{
    static const int limbs[4][4] = {
        {RAGDOLL_UL_ARM, RAGDOLL_LL_ARM, 8, 10},
        {RAGDOLL_UR_ARM, RAGDOLL_LR_ARM, 7, 9},
        {RAGDOLL_UL_LEG, RAGDOLL_LL_LEG, 4, 6},
        {RAGDOLL_UR_LEG, RAGDOLL_LR_LEG, 3, 5},
    }; // body, child body, their joints
    for (int ragdoll_idx = 0; ragdoll_idx < Max_ragdolls; ragdoll_idx++)
    {
        my_ragdoll* ragdoll = get_ragdoll_by_index(ragdoll_idx);
        if (ragdoll->char_inst == nullptr || !ragdoll->added_to_world)
        {
            continue;
        }
        for (int l = 0; l < 4; l++)
        {
            const int body = limbs[l][0];
            const int child = limbs[l][1];
            if (!ragdoll->detached[body] || ragdoll->bodies[body] == nullptr)
            {
                continue;
            }
            havok_body_free(havok_rigid_body_get_handle(ragdoll->bodies[body]), true, true, false);
            havok_body_free(havok_rigid_body_get_handle(ragdoll->bodies[child]), true, true, false);
            ragdoll->bodies[body] = nullptr;
            ragdoll->bodies[child] = nullptr;
            ragdoll->constraints[limbs[l][2]] = nullptr;
            ragdoll->constraints[limbs[l][3]] = nullptr;
        }
    }
    Ragdoll_created_this_frame = 0;
}

// 0x00820700 (:4436)
void havok_ragdoll_deactivate(int ragdoll_idx)
{
    my_ragdoll* ragdoll = get_ragdoll_by_index(ragdoll_idx);
    if (ragdoll == nullptr || !ragdoll->added_to_world)
    {
        return;
    }
    ragdoll->contact_hits = 0;
    for (int i = 0; i < ragdoll->info.num_bones; i++)
    {
        ragdoll->bodies[i]->deactivate();
    }
}

// 0x00820760 (:4453)
void havok_ragdoll_activate(int ragdoll_idx)
{
    my_ragdoll* ragdoll = get_ragdoll_by_index(ragdoll_idx);
    if (ragdoll == nullptr)
    {
        return;
    }
    for (int i = 0; i < ragdoll->info.num_bones; i++)
    {
        ragdoll->bodies[i]->activate();
    }
}

// 0x008261C0 (:4475)
// The bodies' bounding box in the frame (world_pos, world_orient), grown by 0.15.
void havok_ragdoll_get_local_bbox(vector& bmin, vector& bmax, int ragdoll_idx, const vector& world_pos, const matrix& world_orient)
{
    my_ragdoll* ragdoll = get_ragdoll_by_index(ragdoll_idx);
    if (ragdoll == nullptr)
    {
        return;
    }
    hkTransformf frame;
    frame.m_rotation.m_col0 = ragdoll_vec4(world_orient.rvec);
    frame.m_rotation.m_col1 = ragdoll_vec4(world_orient.uvec);
    frame.m_rotation.m_col2 = ragdoll_vec4(world_orient.fvec);
    frame.m_translation = ragdoll_vec4(world_pos);
    bmin = vector(3.4028235e+38f, 3.4028235e+38f, 3.4028235e+38f); // FLT_MAX
    bmax = vector(-3.4028235e+38f, -3.4028235e+38f, -3.4028235e+38f);
    for (int i = 0; i < ragdoll->info.num_bones; i++)
    {
        hkpRigidBody* rb = ragdoll->bodies[i];
        hkTransformf local;
        local.setMulInverseMul(frame, rb->m_motion.m_motionState.m_transform);
        hkAabb aabb;
        rb->m_collidable.m_shape->getAabb(local, 0.0f, aabb);
        bbox_expand(bmin, bmax, ragdoll_vec3(aabb.m_min), ragdoll_vec3(aabb.m_max));
    }
    // Magic number: 0.15 of slack on every side
    bmin = vector((float)((double)bmin.x - 0.15), (float)((double)bmin.y - 0.15), (float)((double)bmin.z - 0.15));
    bmax = vector((float)((double)bmax.x + 0.15), (float)((double)bmax.y + 0.15), (float)((double)bmax.z + 0.15));
}

// The local pivot of a supplemental constraint on a bone (inlined in the constrain functions): the head sphere's
// center, else the sum of the capsule's vertices (see the file comment).
static vector ragdoll_bone_pivot(my_ragdoll& ragdoll, ragdoll_part bone)
{
    if (bone == RAGDOLL_HEAD)
    {
        const hkpConvexTranslateShape* shape = (const hkpConvexTranslateShape*)ragdoll.bodies[0]->m_collidable.m_shape;
        const hkpConvexShape* child = (const hkpConvexShape*)shape->m_childShape.getChild();
        return vector(shape->m_translation.m_quad.m128_f32[0], (float)((double)child->m_radius + (double)shape->m_translation.m_quad.m128_f32[1]), shape->m_translation.m_quad.m128_f32[2]);
    }
    const hkpCapsuleShape* capsule = (const hkpCapsuleShape*)ragdoll.bodies[bone]->m_collidable.m_shape;
    const Q sum = _mm_add_ps(capsule->m_vertexB.m_quad, capsule->m_vertexA.m_quad);
    return vector(sum.m128_f32[0], sum.m128_f32[1], sum.m128_f32[2]);
}

// A keyframed 1 m sphere that collides with nothing, at position (inlined in the constrain functions).
static unsigned int ragdoll_create_faked_body(const hkVector4f& position)
{
    hkpSphereShape* sphere = new hkpSphereShape(1.0f);
    hkpRigidBodyCinfo cinfo;
    cinfo.m_collisionFilterInfo = COLLIDE_NO_COLLISION;
    cinfo.m_shape = sphere;
    cinfo.m_position = position;
    cinfo.m_rotation.m_vec.m_quad = g_vectorfConstants[HK_QUADREAL_0001];
    cinfo.m_motionType = hkpMotion::MOTION_KEYFRAMED;
    havok_rigid_body* rb = new havok_rigid_body(cinfo);
    const unsigned int handle = havok_generate_rigid_body_handle(rb);
    sphere->removeReference();
    return handle;
}

// 0x0083E750 (:4529)
// Pins the bone (RAGDOLL_ROOT: the pelvis center) to pos with a ball and socket to a faked body; the slot or -1.
int havok_ragdoll_constrain_to_point(int ragdoll_idx, ragdoll_part bone, const vector& pos)
{
    my_ragdoll* ragdoll = get_ragdoll_by_index(ragdoll_idx);
    if (ragdoll == nullptr)
    {
        return -1;
    }
    ragdoll->sync_to_anim = false;
    int slot = 0;
    while (slot < 4 && ragdoll->ext_constraint[slot].m_flags.active)
    {
        slot++;
    }
    if (slot >= 4)
    {
        keen::debug::nativePrintArgs("Ragdoll has too many Supp Constraints, Bad!  This is non-fatal.");
        return -1;
    }
    supplemental_constraint& scp = ragdoll->ext_constraint[slot];
    vector pivot;
    if (bone == RAGDOLL_ROOT)
    {
        pivot = ZERO_VECTOR;
        bone = RAGDOLL_BODY;
    }
    else
    {
        pivot = ragdoll_bone_pivot(*ragdoll, bone);
    }
    const unsigned int handle = ragdoll_create_faked_body(ragdoll_vec4(pos));
    hkpBallAndSocketConstraintData* data = new hkpBallAndSocketConstraintData();
    data->m_atoms.m_pivots.m_translationA = ragdoll_vec4(pivot);
    data->m_atoms.m_pivots.m_translationB.m_quad = _mm_setzero_ps();
    hkpRigidBody* faked = handle == 0xFFFFFFFF ? nullptr : Hk_rigid_bodies[handle];
    scp.m_constraint = new hkpConstraintInstance(ragdoll->bodies[bone], faked, data, hkpConstraintInstance::PRIORITY_PSI);
    data->removeReference();
    if (faked != nullptr)
    {
        havok_add_entity(faked, false, true);
    }
    scp.m_flags.constraint_added = 0;
    scp.m_flags.active = 1;
    scp.m_flags.faked_body = 1;
    scp.m_body_a = ragdoll->bodies[bone];
    scp.m_body_a->addReference();
    scp.m_body_b = faked;
    scp.m_body_b->addReference();
    scp.m_point_a = pivot;
    scp.m_type = SCT_POINT_POINT;
    scp.m_point_b = pos;
    scp.handle = 0xFFFFFFFF;
    return slot;
}

// 0x0083EC10 (:4650)
// Pins the bone (RAGDOLL_ROOT: 0.1 m below the pelvis center) to a tag of a human; the faked body follows the tag
// (havok_ragdoll_process_pre). The slot or -1 (also for a vehicle).
int havok_ragdoll_constrain_to_tag(int ragdoll_idx, ragdoll_part bone, unsigned int object_handle, unsigned int tag_idx)
{
    my_ragdoll* ragdoll = get_ragdoll_by_index(ragdoll_idx);
    if (ragdoll == nullptr)
    {
        return -1;
    }
    ragdoll->sync_to_anim = false;
    int slot = 0;
    while (slot < 4 && ragdoll->ext_constraint[slot].m_flags.active)
    {
        slot++;
    }
    if (slot >= 4)
    {
        keen::debug::nativePrintArgs("Ragdoll has too many Supp Constraints, Bad!  This is non-fatal.");
        return -1;
    }
    supplemental_constraint& scp = ragdoll->ext_constraint[slot];
    vector pivot;
    if (bone == RAGDOLL_ROOT)
    {
        // Magic number: (double)-0.1f, 0.1 m down
        pivot = vector((float)((double)UP_VECTOR.x * (double)-0.1f), (float)((double)UP_VECTOR.y * (double)-0.1f), (float)((double)UP_VECTOR.z * (double)-0.1f));
        bone = RAGDOLL_BODY;
    }
    else
    {
        pivot = ragdoll_bone_pivot(*ragdoll, bone);
    }
    object* objp = World.object_from_handle(object_handle);
    if (objp->get_obj_type() == OT_VEHICLE)
    {
        return -1;
    }
    hkVector4f origin;
    origin.m_quad = _mm_setzero_ps();
    const unsigned int handle = ragdoll_create_faked_body(origin);
    scp.m_flags.faked_body = 1;
    hkpRigidBody* faked = nullptr;
    if (handle != 0xFFFFFFFF)
    {
        faked = Hk_rigid_bodies[handle];
    }
    scp.handle = object_handle;
    scp.tag_index = tag_idx;
    const vector pivot_b = ZERO_VECTOR;
    if (faked != nullptr)
    {
        havok_add_entity(faked, false, true);
    }
    // Original: the data's creation reference is never released (see the file comment).
    hkpBallAndSocketConstraintData* data = new hkpBallAndSocketConstraintData();
    data->m_atoms.m_pivots.m_translationA = ragdoll_vec4(pivot);
    data->m_atoms.m_pivots.m_translationB = ragdoll_vec4(pivot_b);
    scp.m_constraint = new hkpConstraintInstance(ragdoll->bodies[bone], faked, data, hkpConstraintInstance::PRIORITY_PSI);
    scp.m_flags.constraint_added = 0;
    scp.m_flags.active = 1;
    scp.m_body_a = ragdoll->bodies[bone];
    scp.m_body_a->addReference();
    scp.m_body_b = faked;
    scp.m_body_b->addReference();
    scp.m_point_a = pivot;
    scp.m_type = SCT_POINT_POINT;
    scp.m_point_b = pivot_b;
    return slot;
}

// 0x00826420 (:4776)
void havok_ragdoll_remove_constraint(int ragdoll_idx, int which_constraint)
{
    my_ragdoll* ragdoll = get_ragdoll_by_index(ragdoll_idx);
    if (ragdoll != nullptr && (unsigned int)which_constraint < 4)
    {
        remove_constraint(ragdoll, which_constraint);
    }
}

// 0x00826450 (:4788)
// The active supplemental constraints to body (as their other body).
void havok_ragdoll_remove_constraints(int ragdoll_idx, hkpRigidBody* body)
{
    my_ragdoll* ragdoll = get_ragdoll_by_index(ragdoll_idx);
    if (ragdoll == nullptr || body == nullptr)
    {
        return;
    }
    for (int i = 0; i < 4; i++)
    {
        if (ragdoll->ext_constraint[i].m_flags.active && ragdoll->ext_constraint[i].m_body_b == body)
        {
            remove_constraint(ragdoll, i);
        }
    }
}

// 0x008264B0 (:4807)
// Advances the flinching bodies (their blend grows by dt x scaler x (time + 0.6) until past 1); the ragdoll loses its
// gravity while it flinches. True while a body still flinches.
bool havok_ragdoll_increment_flinches(int ragdoll_idx)
{
    my_ragdoll* ragdoll = get_ragdoll_by_index(ragdoll_idx);
    if (ragdoll == nullptr)
    {
        return false;
    }
    if (ragdoll->added_to_world && ragdoll->has_gravity)
    {
        havok_ragdoll_set_gravity(ragdoll_idx, false);
    }
    bool flinching = false;
    for (int i = 0; i < NUM_RAGDOLL_BONES; i++)
    {
        if (!ragdoll->bone_flinch_state[i])
        {
            continue;
        }
        ragdoll->bone_flinch_time[i] = (float)((double)Frametime + (double)ragdoll->bone_flinch_time[i]);
        // Magic number: (double)0.6f added to the flinch time
        ragdoll->bone_flinch_blend[i] = (float)((double)Frametime * (double)Ragdoll_flinch_recovery_scaler[i] * ((double)ragdoll->bone_flinch_time[i] + (double)0.6f) + (double)ragdoll->bone_flinch_blend[i]);
        if (ragdoll->bone_flinch_blend[i] > 1.0f)
        {
            ragdoll->bone_flinch_state[i] = false;
            ragdoll->bone_flinch_time[i] = 0.0f;
        }
        if (ragdoll->bone_flinch_state[i])
        {
            flinching = true;
        }
    }
    return flinching;
}

// 0x008125C0 (:4845)
int havok_ragdoll_get_time(int ragdoll_idx)
{
    my_ragdoll* ragdoll = get_ragdoll_by_index(ragdoll_idx);
    if (ragdoll == nullptr)
    {
        return 0;
    }
    return ragdoll->m_start_time.time_since();
}

// 0x008265A0 (:4855)
// The humans of the bound ragdolls whose box (along pelvis -> head) touches the sphere.
int havok_collect_ragdolls_fast(unsigned int* collected, const int max_collected, const vector& center, const float radius)
{
    int num = 0;
    for (int ragdoll_idx = 0; ragdoll_idx < Max_ragdolls; ragdoll_idx++)
    {
        my_ragdoll* ragdoll = Ragdolls[ragdoll_idx];
        if (ragdoll->char_inst == nullptr)
        {
            continue;
        }
        vector pelvis;
        vector head;
        havok_ragdoll_get_pos(ragdoll_idx, pelvis, RAGDOLL_BODY);
        havok_ragdoll_get_pos(ragdoll_idx, head, RAGDOLL_HEAD);
        const vector up(head.x - pelvis.x, head.y - pelvis.y, head.z - pelvis.z);
        matrix orient;
        orient.make(up);
        vector bmin;
        vector bmax;
        havok_ragdoll_get_local_bbox(bmin, bmax, ragdoll_idx, pelvis, orient);
        vector unused = pelvis;
        vec_change_to_local_coords(unused, pelvis, orient); // computed and not used
        const vector box_max(bmax.x + pelvis.x, bmax.y + pelvis.y, bmax.z + pelvis.z);
        const vector box_min(bmin.x + pelvis.x, bmin.y + pelvis.y, bmin.z + pelvis.z);
        if (ix_sphere_in_box(center, radius, box_min, box_max))
        {
            collected[num] = Ragdolls[ragdoll_idx]->human_handle;
            num++;
        }
    }
    return num;
}
