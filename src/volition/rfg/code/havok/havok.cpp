// Original: volition/rfg/code/havok/havok.cpp
//
// The game side of the Havok world (decompiled): the world and its filters (havok_system_init: the collision layer
// matrix of havok_game_filter), rigid bodies by handle (Hk_rigid_bodies, a 2^20 slot table), batched entity and
// phantom adds/removes, the fixed-step simulation (havok_simulate), the object collectors and the body accessors.
// Compiled first in the Havok unity file (rfg_havok_unity.cpp), so its file statics (Collision_models, ...) are read
// by havok_cm_manager.cpp too.
//
// hkpWorldObject properties the game stores (keys from the binary, names inferred, see havok.h): 0 the object,
// 2 the handle of the object it ignores (havok_game_filter), 3 / 4 the physical / effect material.
//
// Partial:
// - unused_dcf_havok_mem and havok_open_write_stream (Havok memory statistics to a file stream) are not implemented.
// - The havok_collect_nav_cells_old_and_slow functions are not implemented.
// - Of the constraint functions havok_create_spring_constraint and havok_create_path_constraint (stiff spring and
//   point to path data) are not implemented.
//
// Notes (decompiled behaviour, kept as is):
// - get_highest_point moves the ray into body space with the body rotation itself (setRotatedDir) and back with its
//   transpose, which is only right for the unrotated terrain bodies it is used on.
// - havok_set_status (and its inlined copies in havok_update_status) folds the leftover step time into one step
//   (fmod) even when the status does not change.

#include "volition/rfg/code/havok/havok.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/debugutil/memoryexceptiontestingutil/hkmemoryexceptiontestingutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/error/hkerror.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/hkbasesystem.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/physics/hkstepinfo.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraint/data/ballandsocket/hkpballandsocketconstraintdata.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraint/data/hkpconstraintdata.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraint/data/limitedhinge/hkplimitedhingeconstraintdata.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraint/data/prismatic/hkpprismaticconstraintdata.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraint/data/ragdoll/hkpragdollconstraintdata.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraint/motor/velocity/hkpvelocityconstraintmotor.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/breakable/hkpbreakableconstraintdata.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics/constraint/data/wrapper/hkpwrappedconstraintdata.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/hkpprocesscollisioninput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/broadphase/3axissweep/hkp3axissweep.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/broadphase/hkpbroadphase.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/broadphase/hkpbroadphasehandlepair.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/broadphase/hkptypedbroadphasehandle.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/misc/bv/hkpbvshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/system/hkmemorysystem.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcollidable.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/hkpagentregisterutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/hkpcollisiondispatcher.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/box/hkpboxshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/capsule/hkpcapsuleshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/cylinder/hkpcylindershape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/sphere/hkpsphereshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/heightfield/sampledheightfield/hkpsampledheightfieldshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/heightfield/trisampledheightfield/hkptrisampledheightfieldbvtreeshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/heightfield/trisampledheightfield/hkptrisampledheightfieldcollection.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/query/hkpshaperaycastinput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/query/hkpshaperaycastoutput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/action/hkpunaryaction.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/collide/filter/constraint/hkpconstraintcollisionfilter.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/setup/hkpconstraintsolversetup.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/motion/hkpmotion.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/phantom/hkpaabbphantom.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/phantom/hkpsimpleshapephantom.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/hkpsimulationisland.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/hkpworld.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/hkpworldcinfo.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/util/hkpworldoperationutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/utilities/dynamics/inertia/hkpinertiatensorcomputer.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/utilities/dynamics/keyframe/hkpkeyframeutility.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshapebuffer.h"

#include "volition/rfg/code/destruction/rfg_materials.h"
#include "volition/rfg/code/destruction/rfg_shapes.h"

#include "volition/rfg/code/gamewide/gamestate_gameplay.h"
#include "volition/rfg/code/havok/api/hkcharactercontroller.h"
#include "volition/rfg/code/havok/havok_bp.h"
#include "volition/rfg/code/havok/havok_cm_manager.h"
#include "volition/rfg/code/havok/havok_level_shape.h"
#include "volition/rfg/code/havok/havok_mopp_code.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/compound/tree/mopp/hkpmoppbvtreeshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/mopp/code/hkpmoppcode.h"
#include "volition/rfg/code/havok/havok_collision_callbacks.h"
#include "volition/rfg/code/havok/havok_collision_events.h"
#include "volition/rfg/code/havok/havok_convert.h"
#include "volition/rfg/code/havok/havok_game_filter.h"
#include "volition/rfg/code/havok/havok_game_world.h"
#include "volition/rfg/code/havok/havok_memory.h"
#include "volition/rfg/code/havok/havok_ragdoll.h"
#include "volition/rfg/code/havok/havok_raycast.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/objects/object.h"
#include "volition/rfg/code/objects/object_zone.h"
#include "volition/rfg/code/pathfinding/bb_pathfinding_data.h"
#include "volition/rfg/code/rfg.h"
#include "volition/rfg/code/vehicle/vehicle_system.h"
#include "volition/vlib/code/math/bbox.h"
#include "volition/vlib/code/math/ix.h"
#include "volition/vlib/code/math/matrix_interpolate.h"
#include "volition/vlib/code/math/quaternion.h"
#include "volition/vlib/code/memory/rfg_split_mempool.h"
#include "volition/vlib/code/memory/vmemory.h"
#include "volition/vlib/code/os/cs_wrapper.h"
#include "volition/vlib/code/os/frametime.h"
#include "volition/vlib/code/os/mutex.h"
#include "volition/vlib/code/os/os.h"
#include "volition/vlib/code/os/scheduler.h"
#include "volition/vlib/code/os/thread.h"
#include "volition/vlib/code/os/vconsole.h"
#include "volition/vlib/code/util/varray.h"

#include "keen/base/trace.hpp"

#include <intrin.h>
#include <math.h>

static const int MAX_COLLISION_MODELS = 0x7FFF; // 32767 collision model table entries
static const unsigned int MAX_RIGID_BODIES = 0x100000; // 2^20 rigid body handles (wrap mask 0xFFFFF)
static const unsigned int HAVOK_SDK_VERSION_NUMBER = 20140205; // 0x13350AD, Havok 2014.2.5

extern float hkConvexShapeDefaultRadius; // 0x016F50EC (Havok global)

// ---- collision filter infos (collision layer in the low 16 bits, system group in the high 16)
const unsigned int COLLIDE_WORLD = 0x00010002; // 0x0165FE04
const unsigned int COLLIDE_PLAYER = 0x00000006; // 0x0165FE08
const unsigned int COLLIDE_NPC = 0x00000007; // 0x0165FE0C
const unsigned int COLLIDE_HUMAN_MINIMUM = 0x0000003F; // 0x0165FE10
const unsigned int COLLIDE_TURRET = 0x00090008; // 0x0165FE14
const unsigned int COLLIDE_VEHICLE = 0x00000009; // 0x0165FE18
const unsigned int COLLIDE_WALKER = 0x0000000C; // 0x0165FE1C
const unsigned int COLLIDE_WALKER_HULL = 0x0000000E; // 0x0165FE20
const unsigned int COLLIDE_FLYER = 0x0000000F; // 0x0165FE24
const unsigned int COLLIDE_RAGDOLL = 0x00020005; // 0x0165FE28
const unsigned int COLLIDE_MOVER = 0x00000003; // 0x0165FE2C
const unsigned int COLLIDE_ITEM = 0x00090004; // 0x0165FE30
const unsigned int COLLIDE_ONLY_FOR_OBJECT_COLLECTION = 0x0006003B; // 0x0165FE34
const unsigned int COLLIDE_HUMAN_HIT_DETECT = 0x00060031; // 0x0165FE38
const unsigned int COLLIDE_PROJECTILE = 0x00050010; // 0x0165FE3C
const unsigned int COLLIDE_PROJECTILE_ATTACHED = 0x00060011; // 0x0165FE40
const unsigned int COLLIDE_PERSONAL_SHIELD = 0x00060012; // 0x0165FE44
const unsigned int COLLIDE_OBJECT_COLLECTOR = 0x00000013; // 0x0165FE48
const unsigned int COLLIDE_OBJECT_COLLECTOR_NO_ITEMS = 0x00000014; // 0x0165FE4C
const unsigned int COLLIDE_OBJECT_COLLECTOR_VEHICLES_ONLY = 0x00000015; // 0x0165FE50
const unsigned int COLLIDE_NAVMESH = 0x00000017; // 0x0165FE54
const unsigned int COLLIDE_VEHICLE_NAVMESH = 0x00000018; // 0x0165FE58
const unsigned int COLLIDE_NO_COLLISION = 0x00060016; // 0x0165FE5C
const unsigned int COLLIDE_HUMAN_ON_LADDER = 0x00000032; // 0x0165FE60
const unsigned int COLLIDE_RFG_INITIAL_POSITION = 0x00080027; // 0x0165FE64
const unsigned int COLLIDE_RFG_NORMAL = 0x00000028; // 0x0165FE68
const unsigned int COLLIDE_RFG_NORMAL_FAST = 0x00060029; // 0x0165FE6C
const unsigned int COLLIDE_RFG_LITE = 0x0006002A; // 0x0165FE70
const unsigned int COLLIDE_RFG_ULTRA_LITE = 0x0006002B; // 0x0165FE74
const unsigned int COLLIDE_RFG_WORLD_ONLY = 0x0006002C; // 0x0165FE78
const unsigned int COLLIDE_RAY_RFG_DESTROYABLE = 0x0000002D; // 0x0165FE7C
const unsigned int COLLIDE_RFG_STRESS = 0x0000002E; // 0x0165FE80
const unsigned int COLLIDE_RAY_DEFAULT = 0x00000019; // 0x0165FE84
const unsigned int COLLIDE_RAY_TERRAIN_ONLY = 0x0000001B; // 0x0165FE88
const unsigned int COLLIDE_RAY_WORLD_ONLY = 0x0000001C; // 0x0165FE8C
const unsigned int COLLIDE_RAY_WALKING_SURFACE_ONLY = 0x0000001D; // 0x0165FE90
const unsigned int COLLIDE_KEYFRAMED = 0x00070024; // 0x0165FE94
const unsigned int COLLIDE_RAY_NAVMESH_ONLY = 0x0000001F; // 0x0165FE98
const unsigned int COLLIDE_RAY_RAGDOLL_ONLY = 0x00000020; // 0x0165FE9C
const unsigned int COLLIDE_RAY_VEHICLE_NAVMESH_ONLY = 0x00000022; // 0x0165FEA0
const unsigned int COLLIDE_RAY_VEHICLE_ENTER_EXIT = 0x00000023; // 0x0165FEA4
const unsigned int COLLIDE_RAY_CITY = 0x0000001E; // 0x0165FEA8
const unsigned int COLLIDE_RAY_PLAYER_COVER = 0x00000021; // 0x0165FEAC
const unsigned int COLLIDE_DEAD_HUMAN = 0x00000025; // 0x0165FEB0
const unsigned int COLLIDE_PHYSICAL_OBJECT = 0x00060026; // 0x0165FEB4
const unsigned int COLLIDE_PHYSICAL_OBJECT_SELF = 0x00060026; // 0x0165FEB8
const unsigned int COLLIDE_CAMERA = 0x00000030; // 0x0165FEBC
const unsigned int COLLIDE_HUMAN_NOT_OTHER_HUMANS = 0x00000034; // 0x0165FEC0
const unsigned int COLLIDE_WORLD_ANCHOR = 0x00060036; // 0x0165FEC4
const unsigned int COLLIDE_DYNAMIC_LINK = 0x00060037; // 0x0165FEC8
const unsigned int COLLIDE_DELETE_OBJECT = 0x00000038; // 0x0165FECC
const unsigned int COLLIDE_RAY_VEHICLE_WHEEL = 0x00000039; // 0x0165FED0
const unsigned int COLLIDE_EFFECT_RIGID_BODY = 0x0000003A; // 0x0165FED4
const unsigned int COLLIDE_CLOTH_SIM = 0x0000003C; // 0x0165FED8
const unsigned int COLLIDE_INVISIBLE_BARRIER = 0x0006003D; // 0x0165FEDC
const unsigned int COLLIDE_FORCE_FIELD = 0x0000003E; // 0x0165FEE0

// ---- globals
hkpWorld* Havok_world; // 0x02B82284
bbox Havok_safe_broadphase; // 0x02B83D98

// The layer table of Havok_filter. A file static of havok_game_filter.cpp in the PDB (0x02642780), placed here because
// havok_system_init uses it and havok.cpp comes first in the unity file.
static unsigned int Havok_game_filter_layer_lookup_table_buffer[havok_game_filter::NUM_UINTS_IN_LAYER_TABLE]; // 0x02642780

// size 0x8
// A collision model replacement requested during the step (havok_body_replace_collision_model). Placement inferred.
struct cm_to_replace
{
    unsigned int m_havok_body_handle; // 0x00
    unsigned int m_cm_to_use; // 0x04
};

// ---- file statics
static bool Havok_use_single_step = true; // 0x0165FB5B
static bool allow_bpo_updates = true; // 0x0165FCCA
static float Havok_step_time = 0.033333f; // 0x0165FCCC
static int Num_collision_models = -1; // 0x0165FDDC, -1 until havok_cm_manager_init
static base_array<hkpEntity*> Havok_add_entity_list; // 0x0165FEE4
static base_array<hkpEntity*> Havok_remove_entity_list; // 0x0165FEF4
static base_array<hkpPhantom*> Havok_add_phantom_list; // 0x0165FF04
static base_array<hkpPhantom*> Havok_remove_phantom_list; // 0x0165FF14
static havok_system_info Havok_system_info[NUM_HAVOK_SYSTEMS] = { havok_system_info("Memory"), havok_system_info("Simulation") }; // 0x0165FF24
static GeneralCollisionListener Collision_listener; // 0x0165FF60
static game_post_collide_listener Post_collide_listener; // 0x0165FF64
static game_post_simulation_listener Post_simulation_listener; // 0x0165FF68
static game_constraint_listener Constraint_listener; // 0x0165FF6C
static const hkpShape* Collision_models[MAX_COLLISION_MODELS]; // 0x02643070
static cm_to_replace Collision_models_to_replace[100]; // 0x02663078
static rfg_object_broadphase* Rfg_broadphase; // 0x02666058
static bool Hk_rigid_body_slot_used[MAX_RIGID_BODIES]; // 0x02667818
static hkpRigidBody* Hk_rigid_bodies[MAX_RIGID_BODIES]; // 0x0276E980
static bool Havok_world_is_simulating; // 0x02B8220A
static bool Batch_init_active; // 0x02B8220B
static hkpSimpleShapePhantom* Sphere_collector; // 0x02B82210
static hkpSimpleShapePhantom* Box_collector; // 0x02B82214
static hkpSimpleShapePhantom* Capsule_collector; // 0x02B82218
static unsigned int Num_collision_models_to_replace; // 0x02B8221C
static hkpConstraintCollisionFilter* Constraint_filter; // 0x02B82220
static hkMemoryRouter* Havok_job_memory[6]; // 0x02B82224
static float Havok_step_time_leftover; // 0x02B8223C
static float Havok_step_time_leftover_last; // 0x02B82240
static float Havok_interpolate_percent; // 0x02B82244
static int Havok_cur_step_this_frame; // 0x02B82248
static int Havok_num_steps_this_frame; // 0x02B8224C
static bool Havok_system_init; // 0x02B82250
static unsigned int Current_free_hk_body_handle; // 0x02B82254
static bool Allow_default_cm_removal; // 0x02B8225F
static unsigned int Thread_id; // 0x02B82264
static hkpWorld* Havok_mt_query_world; // 0x02B822B8
static mutex Hk_rigid_bodies_lock; // 0x02B83CA4
static cs_wrapper Havok_remove_entity_wrapper(0); // 0x02B83D5C
static cs_wrapper Havok_remove_phantom_wrapper(0); // 0x02B83DC0
static cs_wrapper Havok_add_entity_wrapper(0); // 0x02B83DFC
static cs_wrapper Havok_job_memory_cs(0); // 0x02B951A8
static cs_wrapper Havok_add_phantom_wrapper(0); // 0x02B95260

// The world object owning a collidable (Havok's hkpGetWorldObject).
static hkpWorldObject* havok_world_object(const hkpCollidable* collidable)
{
    return (hkpWorldObject*)collidable->getOwner();
}

// ---- havok_system_info
// 0x0080DD10 (:237)
havok_system_info::havok_system_info(const char* n)
{
    delay.invalidate();
    status = HAVOK_STATUS_OK;
    name = n;
}

// 0x0080DD30 (:244)
// The status falls back to OK when its delay runs out.
void havok_system_info::do_frame()
{
    if (delay.elapsed())
    {
        delay.invalidate();
        status = HAVOK_STATUS_OK;
    }
}

// ---- havok_rigid_body
// 0x0080DD50 (:264)
havok_rigid_body::havok_rigid_body(const hkpRigidBodyCinfo& cinfo)
    : hkpRigidBody(cinfo)
{
}

// 0x0080DD70 (:269)
havok_rigid_body::~havok_rigid_body()
{
}

// ---- havok_region_callback_shape
// 0x00816430 (:301)
havok_region_callback_shape::havok_region_callback_shape(bool (*phantom_enter_callback)(object*, object*), bool (*phantom_leave_callback)(object*, object*))
    : hkpPhantomCallbackShape()
{
    enter_callback = phantom_enter_callback;
    leave_callback = phantom_leave_callback;
}

// 0x008224E0 (:308)
// Calls the enter callback with (phantom object, entering object), unless the entering world object has property 5
// or 11.
void havok_region_callback_shape::phantomEnterEvent(const hkpCollidable* collidableA, const hkpCollidable* collidableB, const hkpCollisionInput& env)
{
    bool (*callback)(object*, object*) = enter_callback;
    if (callback == nullptr)
    {
        return;
    }
    hkpWorldObject* wp = havok_world_object(collidableB);
    if (wp->hasProperty(5, hkpWorldObject::MULTI_THREADING_CHECKS_ENABLE) || wp->hasProperty(11, hkpWorldObject::MULTI_THREADING_CHECKS_ENABLE))
    {
        return;
    }
    object* entering = object_from_world_obj(wp);
    callback(object_from_world_obj(havok_world_object(collidableA)), entering);
}

// 0x00822550 (:335)
// As phantomEnterEvent, with property 10 instead of 11.
void havok_region_callback_shape::phantomLeaveEvent(const hkpCollidable* collidableA, const hkpCollidable* collidableB)
{
    bool (*callback)(object*, object*) = leave_callback;
    if (callback == nullptr)
    {
        return;
    }
    hkpWorldObject* wp = havok_world_object(collidableB);
    if (wp->hasProperty(5, hkpWorldObject::MULTI_THREADING_CHECKS_ENABLE) || wp->hasProperty(10, hkpWorldObject::MULTI_THREADING_CHECKS_ENABLE))
    {
        return;
    }
    object* leaving = object_from_world_obj(wp);
    callback(object_from_world_obj(havok_world_object(collidableA)), leaving);
}

// 0x00816460
havok_region_callback_shape::~havok_region_callback_shape()
{
}

// ---- console
// 0x0080DDA0 (:607)
// Toggles the fixed 1/30 s step (otherwise 1/60 s).
void unused_dcf_Havok_use_single_step()
{
    if (console_get_arg(0xC0, 0)) // CONSOLE_ARG_BOOL | CONSOLE_ARG_NONE
    {
        Havok_use_single_step = ((Console_arg_type >> 6) & 1) != 0;
    }
    else
    {
        Havok_use_single_step = !Havok_use_single_step;
    }
    if (!Havok_use_single_step)
    {
        Havok_step_time = 0.016666668f; // 1/60 s
    }
    else
    {
        Havok_step_time = 0.033333335f; // 1/30 s
    }
    if (Havok_world != nullptr)
    {
        Havok_world->m_collisionDispatcher->m_expectedMinPsiDeltaTime = Havok_step_time;
    }
}

// ---- rigid body handles
// 0x0080DE10 (:764)
// The next free slot from Current_free_hk_body_handle (wrapping); when every slot is used the current one is
// overwritten.
unsigned int havok_generate_rigid_body_handle(hkpRigidBody* hk_rigid_body)
{
    Hk_rigid_bodies_lock.lock(true);
    unsigned int handle = Current_free_hk_body_handle;
    if (Hk_rigid_body_slot_used[handle])
    {
        do
        {
            handle = (handle + 1) & (MAX_RIGID_BODIES - 1);
        } while (Hk_rigid_body_slot_used[handle] && handle != Current_free_hk_body_handle);
    }
    Current_free_hk_body_handle = (handle + 1) & (MAX_RIGID_BODIES - 1);
    Hk_rigid_body_slot_used[handle] = true;
    Hk_rigid_bodies[handle] = hk_rigid_body;
    Hk_rigid_bodies_lock.unlock();
    return handle;
}

// 0x0080DE80 (:801)
void havok_free_rigid_body_handle(unsigned int handle)
{
    Hk_rigid_bodies_lock.lock(true);
    Hk_rigid_bodies[handle] = nullptr;
    Hk_rigid_body_slot_used[handle] = false;
    Hk_rigid_bodies_lock.unlock();
}

// 0x0080DEB0 (:812)
hkpRigidBody* havok_get_pointer_from_rigid_body_handle(unsigned int handle)
{
    if (handle == 0xFFFFFFFF)
    {
        return nullptr;
    }
    return Hk_rigid_bodies[handle];
}

// ---- batched adds and removes
// 0x0083DB00 (:823)
// Batched entities are added by havok_add_entity_flush (a full list is flushed first); the batch is activated when
// any of its entities asks for it.
void havok_add_entity(hkpEntity* ep, bool init_active, bool batch)
{
    if (ep == nullptr)
    {
        return;
    }
    if (!batch)
    {
        Havok_world->addEntity(ep, (hkpEntityActivation)init_active);
        return;
    }
    Havok_add_entity_wrapper.enter();
    if (Havok_add_entity_list.full())
    {
        havok_add_entity_flush();
    }
    Havok_add_entity_list.add(ep);
    Batch_init_active = Batch_init_active || init_active;
    Havok_add_entity_wrapper.leave();
}

// 0x0083CBB0 (:852)
// Up to 5 entities are added one by one; more go in batches sized to the largest free Havok block (0xC00 bytes per
// entity).
void havok_add_entity_flush()
{
    Havok_add_entity_wrapper.enter();
    const hkpEntityActivation activation = (hkpEntityActivation)(Batch_init_active != false);
    if (Havok_add_entity_list.size() < 6)
    {
        for (int i = 0; i < Havok_add_entity_list.size(); i++)
        {
            Havok_world->addEntity(Havok_add_entity_list[i], activation);
        }
    }
    else
    {
        const unsigned int largest_block = havok_memory_get_largest_block();
        hkpEntity** batch = Havok_add_entity_list.elements();
        for (unsigned int remaining = Havok_add_entity_list.size(); remaining != 0;)
        {
            unsigned int count = largest_block / 0xC00; // bytes Havok needs per added entity
            if (remaining <= count)
            {
                count = remaining;
            }
            Havok_world->addEntityBatch(batch, count, activation);
            batch += count;
            remaining -= count;
        }
    }
    Havok_add_entity_list.remove_all();
    Batch_init_active = false;
    Havok_add_entity_wrapper.leave();
}

// 0x00816500 (:890)
// Takes a pending entity out of the add list (keeping the order); its index, or -1 when it is not pending.
static int havok_remove_entity_from_list(hkpEntity* ep)
{
    Havok_add_entity_wrapper.enter();
    for (int i = 0; i < Havok_add_entity_list.size(); i++)
    {
        if (Havok_add_entity_list[i] == ep)
        {
            Havok_add_entity_list.remove(i);
            Havok_add_entity_wrapper.leave();
            return i;
        }
    }
    Havok_add_entity_wrapper.leave();
    return -1;
}

// 0x008225C0 (:904)
// An entity still waiting to be added is just dropped from the add list.
void havok_remove_entity(hkpEntity* ep, bool batch)
{
    if (havok_remove_entity_from_list(ep) != -1)
    {
        return;
    }
    if (!batch)
    {
        Havok_world->removeEntity(ep);
        return;
    }
    Havok_remove_entity_wrapper.enter();
    if (Havok_remove_entity_list.full())
    {
        havok_remove_entity_flush();
    }
    Havok_remove_entity_list.add(ep);
    Havok_remove_entity_wrapper.leave();
}

// 0x00816580 (:932)
void havok_remove_entity_flush()
{
    Havok_remove_entity_wrapper.enter();
    if (Havok_remove_entity_list.size() < 8)
    {
        for (int i = 0; i < Havok_remove_entity_list.size(); i++)
        {
            Havok_world->removeEntity(Havok_remove_entity_list[i]);
        }
    }
    else
    {
        Havok_world->removeEntityBatch(Havok_remove_entity_list.elements(), Havok_remove_entity_list.size());
    }
    Havok_remove_entity_list.remove_all();
    Havok_remove_entity_wrapper.leave();
}

// 0x00822670 (:949)
void havok_add_phantom(hkpPhantom* pp, bool batch)
{
    if (!batch)
    {
        Havok_world->addPhantom(pp);
        return;
    }
    Havok_add_phantom_wrapper.enter();
    if (Havok_add_phantom_list.full())
    {
        havok_add_phantom_flush();
    }
    Havok_add_phantom_list.add(pp);
    Havok_add_phantom_wrapper.leave();
}

// 0x008165F0 (:969)
void havok_add_phantom_flush()
{
    Havok_add_phantom_wrapper.enter();
    if (Havok_add_phantom_list.size() > 5)
    {
        Havok_world->addPhantomBatch(Havok_add_phantom_list.elements(), Havok_add_phantom_list.size());
    }
    else
    {
        for (int i = 0; i < Havok_add_phantom_list.size(); i++)
        {
            Havok_world->addPhantom(Havok_add_phantom_list[i]);
        }
    }
    Havok_add_phantom_list.remove_all();
    Havok_add_phantom_wrapper.leave();
}

// 0x00816670 (:987)
static int havok_remove_phantom_from_list(hkpPhantom* pp)
{
    Havok_add_phantom_wrapper.enter();
    for (int i = 0; i < Havok_add_phantom_list.size(); i++)
    {
        if (Havok_add_phantom_list[i] == pp)
        {
            Havok_add_phantom_list.remove(i);
            Havok_add_phantom_wrapper.leave();
            return i;
        }
    }
    Havok_add_phantom_wrapper.leave();
    return -1;
}

// 0x00822700 (:1000)
void havok_remove_phantom(hkpPhantom* pp, bool batch)
{
    if (havok_remove_phantom_from_list(pp) != -1)
    {
        return;
    }
    if (!batch)
    {
        Havok_world->removePhantom(pp);
        return;
    }
    Havok_remove_phantom_wrapper.enter();
    if (Havok_remove_phantom_list.full())
    {
        havok_remove_phantom_flush();
    }
    Havok_remove_phantom_list.add(pp);
    Havok_remove_phantom_wrapper.leave();
}

// 0x008166F0 (:1027)
void havok_remove_phantom_flush()
{
    Havok_remove_phantom_wrapper.enter();
    if (Havok_remove_phantom_list.size() > 7)
    {
        Havok_world->removePhantomBatch(Havok_remove_phantom_list.elements(), Havok_remove_phantom_list.size());
    }
    else
    {
        for (int i = 0; i < Havok_remove_phantom_list.size(); i++)
        {
            Havok_world->removePhantom(Havok_remove_phantom_list[i]);
        }
    }
    Havok_remove_phantom_list.remove_all();
    Havok_remove_phantom_wrapper.leave();
}

// ---- constraints (helpers)
// 0x0080DED0 (:1045)
// The constraint data under breakable and malleable wrappers.
static hkpConstraintData* havok_constraint_get_base_data(hkpConstraintInstance* ci)
{
    if (ci == nullptr)
    {
        return nullptr;
    }
    hkpConstraintData* data = ci->m_data;
    if (data != nullptr)
    {
        if (data->getType() == hkpConstraintData::CONSTRAINT_TYPE_BREAKABLE)
        {
            data = const_cast<hkpConstraintData*>(static_cast<hkpWrappedConstraintData*>(data)->getWrappedConstraintData());
        }
        if (data != nullptr && data->getType() == hkpConstraintData::CONSTRAINT_TYPE_MALLEABLE)
        {
            data = const_cast<hkpConstraintData*>(static_cast<hkpWrappedConstraintData*>(data)->getWrappedConstraintData());
        }
    }
    return data;
}

// 0x0080DF20 (:1067)
// World pivots (and axes) of a ball and socket, limited hinge, prismatic or ragdoll constraint, from the local
// transforms stored in its atoms (entity 0 is the child, entity 1 the host).
static bool havok_constraint_get_pivots(hkpConstraintInstance* ci, hkVector4f* host_pivot, hkVector4f* child_pivot, hkVector4f* host_axis, hkVector4f* child_axis)
{
    if (ci == nullptr)
    {
        return false;
    }
    hkpConstraintData* data = havok_constraint_get_base_data(ci);
    // Offsets into the constraint data of the local pivots and axes: hkpSetLocalTranslationsConstraintAtom (ball and
    // socket: translations A and B) or hkpSetLocalTransformsConstraintAtom (transform A at 0x20, B at 0x60; the
    // axis is column 0, the pivot the translation).
    const unsigned char* base = (const unsigned char*)data;
    const hkVector4f* child_local_pivot;
    const hkVector4f* host_local_pivot;
    const hkVector4f* child_local_axis = nullptr;
    const hkVector4f* host_local_axis = nullptr;
    switch (data->getType())
    {
        case hkpConstraintData::CONSTRAINT_TYPE_BALLANDSOCKET:
            child_local_pivot = (const hkVector4f*)(base + 0x20);
            host_local_pivot = (const hkVector4f*)(base + 0x30);
            break;
        case hkpConstraintData::CONSTRAINT_TYPE_LIMITEDHINGE:
        case hkpConstraintData::CONSTRAINT_TYPE_PRISMATIC:
            child_local_pivot = (const hkVector4f*)(base + 0x50);
            child_local_axis = (const hkVector4f*)(base + 0x20);
            host_local_pivot = (const hkVector4f*)(base + 0x90);
            host_local_axis = (const hkVector4f*)(base + 0x60);
            break;
        case hkpConstraintData::CONSTRAINT_TYPE_RAGDOLL:
            child_local_pivot = (const hkVector4f*)(base + 0x50);
            host_local_pivot = (const hkVector4f*)(base + 0x90);
            break;
        default:
            return false;
    }
    if (child_pivot != nullptr)
    {
        const hkTransformf& transform = ci->m_entities[0]->m_motion.m_motionState.m_transform;
        child_pivot->setRotatedDir(transform.m_rotation, *child_local_pivot);
        child_pivot->add(transform.m_translation);
    }
    if (child_axis != nullptr && child_local_axis != nullptr)
    {
        child_axis->setRotatedDir(ci->m_entities[0]->m_motion.m_motionState.m_transform.m_rotation, *child_local_axis);
    }
    if (host_pivot != nullptr)
    {
        const hkTransformf& transform = ci->m_entities[1]->m_motion.m_motionState.m_transform;
        host_pivot->setRotatedDir(transform.m_rotation, *host_local_pivot);
        host_pivot->add(transform.m_translation);
    }
    if (host_axis != nullptr && host_local_axis != nullptr)
    {
        host_axis->setRotatedDir(ci->m_entities[1]->m_motion.m_motionState.m_transform.m_rotation, *host_local_axis);
    }
    return true;
}

// ---- constraints
// 0x0083CD40 (:5392)
// A constraint between the child (entity A) and the host (entity B) in the world: the data (with a positive threshold
// wrapped in a breakable constraint with that solver result limit, kept in the world when broken) is taken over (its
// creator's reference goes); bodies still waiting to be added are added first; the child's collisions with the bodies
// it is constrained to are re-filtered.
hkpConstraintInstance* havok_constraint_create(hkpRigidBody* child_rb, hkpRigidBody* host_rb, hkpConstraintData* cd, float threshold, hkpConstraintInstance::ConstraintPriority priority)
{
    if (0.0f < threshold)
    {
        hkpBreakableConstraintData* breakable = new hkpBreakableConstraintData(cd);
        breakable->m_solverResultLimit = threshold;
        breakable->m_removeWhenBroken = false;
        cd->removeReference();
        cd = breakable;
    }
    if (child_rb->m_world == nullptr || host_rb->m_world == nullptr)
    {
        havok_add_entity_flush();
    }
    hkpConstraintInstance* ci = new hkpConstraintInstance(child_rb, host_rb, cd, priority);
    Havok_world->addConstraint(ci);
    cd->removeReference();
    if (child_rb->m_world != nullptr)
    {
        Havok_world->updateCollisionFilterOnEntity(child_rb, HK_UPDATE_FILTER_ON_ENTITY_DISABLE_ENTITY_ENTITY_COLLISIONS_ONLY, HK_UPDATE_COLLECTION_FILTER_IGNORE_SHAPE_COLLECTIONS);
    }
    return ci;
}

// 0x0083CE20 (:5421)
// The constraint again between new bodies: its data (under a breakable or malleable wrapper) is set up in world space
// at the child's current pivot (a hinge or prismatic keeps its axis, a ragdoll joint its twist and plane axes, as the
// old host has them now), a new instance takes the data, the old one leaves the world. Original: other constraint
// types return null and the old instance stays in the world.
hkpConstraintInstance* havok_constraint_transfer(hkpConstraintInstance* ci, hkpRigidBody* child_rb, hkpRigidBody* host_rb)
{
    if (child_rb->m_world == nullptr || host_rb->m_world == nullptr)
    {
        havok_add_entity_flush();
    }
    hkpConstraintData* base_data = havok_constraint_get_base_data(ci);
    hkVector4f child_pivot;
    havok_constraint_get_pivots(ci, nullptr, &child_pivot, nullptr, nullptr);
    const hkTransformf& child_transform = child_rb->m_motion.m_motionState.m_transform;
    const hkTransformf& host_transform = host_rb->m_motion.m_motionState.m_transform;
    switch (base_data->getType())
    {
        case hkpConstraintData::CONSTRAINT_TYPE_LIMITEDHINGE:
        {
            hkpLimitedHingeConstraintData* hinge = static_cast<hkpLimitedHingeConstraintData*>(base_data);
            hkVector4f axis;
            axis.setRotatedDir(host_transform.m_rotation, hinge->m_atoms.m_transforms.m_transformB.m_rotation.m_col0);
            hinge->setInWorldSpace(child_transform, host_transform, child_pivot, axis);
            break;
        }
        case hkpConstraintData::CONSTRAINT_TYPE_PRISMATIC:
        {
            hkpPrismaticConstraintData* prismatic = static_cast<hkpPrismaticConstraintData*>(base_data);
            hkVector4f axis;
            axis.setRotatedDir(host_transform.m_rotation, prismatic->m_atoms.m_transforms.m_transformB.m_rotation.m_col0);
            prismatic->setInWorldSpace(child_transform, host_transform, child_pivot, axis);
            break;
        }
        case hkpConstraintData::CONSTRAINT_TYPE_BALLANDSOCKET:
            static_cast<hkpBallAndSocketConstraintData*>(base_data)->setInWorldSpace(child_transform, host_transform, child_pivot);
            break;
        case hkpConstraintData::CONSTRAINT_TYPE_RAGDOLL:
        {
            hkpRagdollConstraintData* ragdoll = static_cast<hkpRagdollConstraintData*>(base_data);
            hkVector4f twist_axis;
            hkVector4f plane_axis;
            twist_axis.setRotatedDir(host_transform.m_rotation, ragdoll->m_atoms.m_transforms.m_transformB.m_rotation.m_col0);
            plane_axis.setRotatedDir(host_transform.m_rotation, ragdoll->m_atoms.m_transforms.m_transformB.m_rotation.m_col1);
            ragdoll->setInWorldSpace(child_transform, host_transform, child_pivot, twist_axis, plane_axis);
            break;
        }
        default:
            return nullptr;
    }
    hkpConstraintData* cd = ci->m_data;
    hkpConstraintInstance* transferred = havok_constraint_create(child_rb, host_rb, cd, -1.0f, hkpConstraintInstance::PRIORITY_PSI);
    // havok_constraint_create took one reference, the old instance still holds its own
    cd->addReference();
    Havok_world->removeConstraint(ci);
    ci->removeReference();
    return transferred;
}

// 0x00817DE0 (:5491)
void havok_constraint_release(hkpConstraintInstance* ci)
{
    Havok_world->removeConstraint(ci);
    ci->removeReference();
}

// 0x0080E620 (:5500)
// The friction of a hinge (torque), a path (force) or a prismatic constraint (force); other types have none.
void havok_constraint_set_friction(hkpConstraintInstance* ci, float friction)
{
    hkpConstraintData* data = havok_constraint_get_base_data(ci);
    if (data == nullptr)
    {
        return;
    }
    switch (data->getType())
    {
        case hkpConstraintData::CONSTRAINT_TYPE_LIMITEDHINGE:
            static_cast<hkpLimitedHingeConstraintData*>(data)->setMaxFrictionTorque(friction);
            return;
        case hkpConstraintData::CONSTRAINT_TYPE_POINTTOPATH:
            // hkpPointToPathConstraintData (not ported; havok_create_path_constraint is a stand-in): m_maxFrictionForce
            // at 0x24
            *reinterpret_cast<float*>(reinterpret_cast<char*>(data) + 0x24) = friction;
            return;
        case hkpConstraintData::CONSTRAINT_TYPE_PRISMATIC:
            static_cast<hkpPrismaticConstraintData*>(data)->setMaxFrictionForce(friction);
            return;
        default:
            return;
    }
}

// The velocity motor havok_constraint_add_motor creates (the constructor, inlined): tau 0.8, the force range
// +-max_force, not driven by the constraint's targets
static hkpVelocityConstraintMotor* havok_constraint_new_velocity_motor(float desired_vel, float max_force)
{
    hkpVelocityConstraintMotor* motor = new hkpVelocityConstraintMotor();
    motor->m_velocityTarget = desired_vel;
    motor->m_type = hkpConstraintMotor::TYPE_VELOCITY;
    motor->m_maxForce = max_force;
    motor->m_minForce = -max_force;
    motor->m_tau = 0.8f; // 0x012BBF80
    motor->m_useVelocityTargetFromConstraintTargets = false;
    return motor;
}

// 0x00817E10 (:5530)
// A velocity motor on a hinge (switched on: the hinge's friction goes off) or a prismatic constraint, unless it has one.
// max_force is scaled by 10000 (in double).
// Original: the prismatic constraint's motor stays switched off (its atom's m_isEnabled is never set).
void havok_constraint_add_motor(hkpConstraintInstance* ci, float desired_vel, float max_force)
{
    hkpConstraintData* data = havok_constraint_get_base_data(ci);
    if (data == nullptr)
    {
        return;
    }
    // Magic number: the game's force to Havok's
    max_force = (float)((double)max_force * 10000.0); // 0x012C2A90
    switch (data->getType())
    {
        case hkpConstraintData::CONSTRAINT_TYPE_LIMITEDHINGE:
        {
            hkpLimitedHingeConstraintData* hinge = static_cast<hkpLimitedHingeConstraintData*>(data);
            if (hinge->m_atoms.m_angMotor.m_motor != nullptr)
            {
                return;
            }
            hkpVelocityConstraintMotor* motor = havok_constraint_new_velocity_motor(desired_vel, max_force);
            hinge->setMotor(motor);
            motor->removeReference();
            hinge->setMotorEnabled(ci->m_internal->m_runtime, true);
            return;
        }
        case hkpConstraintData::CONSTRAINT_TYPE_PRISMATIC:
        {
            hkpPrismaticConstraintData* prismatic = static_cast<hkpPrismaticConstraintData*>(data);
            if (prismatic->m_atoms.m_motor.m_motor != nullptr)
            {
                return;
            }
            hkpVelocityConstraintMotor* motor = havok_constraint_new_velocity_motor(desired_vel, max_force);
            prismatic->setMotor(motor);
            motor->removeReference();
            return;
        }
        default:
            return;
    }
}

// 0x0080E6C0 (:5582)
// A motor's velocity target and force range. Original: max_force is not scaled here (havok_constraint_add_motor
// scales it by 10000).
void havok_constraint_update_motor(hkpConstraintInstance* ci, float desired_vel, float max_force)
{
    hkpConstraintData* data = havok_constraint_get_base_data(ci);
    if (data == nullptr)
    {
        return;
    }
    hkpConstraintMotor* motor;
    switch (data->getType())
    {
        case hkpConstraintData::CONSTRAINT_TYPE_LIMITEDHINGE:
            motor = static_cast<hkpLimitedHingeConstraintData*>(data)->m_atoms.m_angMotor.m_motor;
            break;
        case hkpConstraintData::CONSTRAINT_TYPE_PRISMATIC:
            motor = static_cast<hkpPrismaticConstraintData*>(data)->m_atoms.m_motor.m_motor;
            break;
        default:
            return;
    }
    if (motor == nullptr)
    {
        return;
    }
    // the motor is taken to be a velocity motor (the only kind havok_constraint_add_motor creates)
    hkpVelocityConstraintMotor* velocity_motor = static_cast<hkpVelocityConstraintMotor*>(motor);
    velocity_motor->m_maxForce = max_force;
    velocity_motor->m_minForce = -max_force;
    velocity_motor->m_velocityTarget = desired_vel;
}

// 0x0080E760 (:5620)
// The motor goes (a hinge's is switched off first, its friction back on).
void havok_constraint_remove_motor(hkpConstraintInstance* ci)
{
    hkpConstraintData* data = havok_constraint_get_base_data(ci);
    if (data == nullptr)
    {
        return;
    }
    switch (data->getType())
    {
        case hkpConstraintData::CONSTRAINT_TYPE_LIMITEDHINGE:
        {
            hkpLimitedHingeConstraintData* hinge = static_cast<hkpLimitedHingeConstraintData*>(data);
            hinge->setMotorEnabled(ci->m_internal->m_runtime, false);
            hinge->setMotor(nullptr);
            return;
        }
        case hkpConstraintData::CONSTRAINT_TYPE_PRISMATIC:
            static_cast<hkpPrismaticConstraintData*>(data)->setMotor(nullptr);
            return;
        default:
            return;
    }
}

// 0x0080E7F0 (:5650)
bool havok_constraint_has_motor(hkpConstraintInstance* ci)
{
    hkpConstraintData* data = havok_constraint_get_base_data(ci);
    if (data == nullptr)
    {
        return false;
    }
    switch (data->getType())
    {
        case hkpConstraintData::CONSTRAINT_TYPE_LIMITEDHINGE:
            return static_cast<hkpLimitedHingeConstraintData*>(data)->m_atoms.m_angMotor.m_motor != nullptr;
        case hkpConstraintData::CONSTRAINT_TYPE_PRISMATIC:
            return static_cast<hkpPrismaticConstraintData*>(data)->m_atoms.m_motor.m_motor != nullptr;
        default:
            return false;
    }
}

// 0x0083D090 (:5691)
// A prismatic constraint (unbreakable for a threshold of 0 or less) holding the child to the host: no rotation, travel
// along the axis (world space) through the child's position within [min_limit, max_limit]. Null without both bodies.
hkpConstraintInstance* havok_create_slide_constraint(unsigned int host_handle, unsigned int child_handle, const vector& axis, float min_limit, float max_limit, float threshold)
{
    hkpRigidBody* host_rb = host_handle == 0xFFFFFFFF ? nullptr : Hk_rigid_bodies[host_handle];
    hkpRigidBody* child_rb = child_handle == 0xFFFFFFFF ? nullptr : Hk_rigid_bodies[child_handle];
    if (host_rb == nullptr || child_rb == nullptr)
    {
        return nullptr;
    }
    hkVector4f hk_axis;
    hk_axis.set(axis.x, axis.y, axis.z, 0.0f);
    const hkVector4f pivot = child_rb->m_motion.m_motionState.m_transform.m_translation;
    hkpPrismaticConstraintData* data = new hkpPrismaticConstraintData();
    data->setInWorldSpace(child_rb->m_motion.m_motionState.m_transform, host_rb->m_motion.m_motionState.m_transform, pivot, hk_axis);
    data->setMaxLinearLimit(max_limit);
    data->setMinLinearLimit(min_limit);
    return havok_constraint_create(child_rb, host_rb, data, threshold, hkpConstraintInstance::PRIORITY_PSI);
}

// 0x0083D1C0 (:5729)
// A limited hinge (unlimited: +-1e14) holding the child to the host around the axis through the pivot (world space);
// its motor is switched off (the friction on). Null without both bodies.
hkpConstraintInstance* havok_create_hinge_constraint(unsigned int host_handle, unsigned int child_handle, const vector& hinge_pos, const vector& hinge_axis, bool limited, float min_angle, float max_angle, float threshold)
{
    hkpRigidBody* host_rb = host_handle == 0xFFFFFFFF ? nullptr : Hk_rigid_bodies[host_handle];
    hkpRigidBody* child_rb = child_handle == 0xFFFFFFFF ? nullptr : Hk_rigid_bodies[child_handle];
    if (host_rb == nullptr || child_rb == nullptr)
    {
        return nullptr;
    }
    hkVector4f axis;
    axis.set(hinge_axis.x, hinge_axis.y, hinge_axis.z, 0.0f);
    hkVector4f pivot;
    pivot.set(hinge_pos.x, hinge_pos.y, hinge_pos.z, 0.0f);
    hkpLimitedHingeConstraintData* data = new hkpLimitedHingeConstraintData();
    data->setInWorldSpace(child_rb->m_motion.m_motionState.m_transform, host_rb->m_motion.m_motionState.m_transform, pivot, axis);
    if (limited)
    {
        data->setMinAngularLimit(min_angle);
        data->setMaxAngularLimit(max_angle);
    }
    else
    {
        // Magic numbers: effectively no limit
        data->setMinAngularLimit(-1.0e14f); // 0x012F94C4
        data->setMaxAngularLimit(1.0e14f); // 0x012F94C0
    }
    hkpConstraintInstance* ci = havok_constraint_create(child_rb, host_rb, data, threshold, hkpConstraintInstance::PRIORITY_PSI);
    data->setMotorEnabled(ci->m_internal->m_runtime, false);
    return ci;
}

// 0x0083D350 (:5774)
// A ball and socket holding the child to the host at the pivot (world space). Null without both bodies.
hkpConstraintInstance* havok_create_point_constraint(unsigned int host_handle, unsigned int child_handle, const vector& pos, float threshold)
{
    hkpRigidBody* host_rb = host_handle == 0xFFFFFFFF ? nullptr : Hk_rigid_bodies[host_handle];
    hkpRigidBody* child_rb = child_handle == 0xFFFFFFFF ? nullptr : Hk_rigid_bodies[child_handle];
    if (host_rb == nullptr || child_rb == nullptr)
    {
        return nullptr;
    }
    hkVector4f pivot;
    pivot.set(pos.x, pos.y, pos.z, 0.0f);
    hkpBallAndSocketConstraintData* data = new hkpBallAndSocketConstraintData();
    data->setInWorldSpace(child_rb->m_motion.m_motionState.m_transform, host_rb->m_motion.m_motionState.m_transform, pivot);
    return havok_constraint_create(child_rb, host_rb, data, threshold, hkpConstraintInstance::PRIORITY_PSI);
}

// 0x0083D690 (:5864)
// A ragdoll joint holding the child to the host: the twist range from the horizontal field of view, the planes range
// from the arguments, the cone from the vertical field of view (turned to its middle), around the info's pivot and
// axes (world space). Null without both bodies.
hkpConstraintInstance* havok_create_ragdoll_constraint(unsigned int host_handle, unsigned int child_handle, const ragdoll_constraint_info& rci, float min_planar_ang, float max_planar_ang, float threshold)
{
    hkpRigidBody* host_rb = host_handle == 0xFFFFFFFF ? nullptr : Hk_rigid_bodies[host_handle];
    hkpRigidBody* child_rb = child_handle == 0xFFFFFFFF ? nullptr : Hk_rigid_bodies[child_handle];
    if (host_rb == nullptr || child_rb == nullptr)
    {
        return nullptr;
    }
    hkpRagdollConstraintData* data = new hkpRagdollConstraintData();
    data->m_atoms.m_planesLimit.m_minAngle = min_planar_ang;
    data->m_atoms.m_planesLimit.m_maxAngle = max_planar_ang;
    data->m_atoms.m_twistLimit.m_minAngle = rci.h_fov_neg;
    data->m_atoms.m_twistLimit.m_maxAngle = rci.h_fov_pos;
    hkVector4f pivot;
    hkVector4f twist_axis;
    hkVector4f plane_axis;
    pivot.set(rci.pivot.x, rci.pivot.y, rci.pivot.z, 0.0f);
    twist_axis.set(rci.twist_axis.x, rci.twist_axis.y, rci.twist_axis.z, 0.0f);
    plane_axis.set(rci.plane_axis.x, rci.plane_axis.y, rci.plane_axis.z, 0.0f);
    data->setInWorldSpace(child_rb->m_motion.m_motionState.m_transform, host_rb->m_motion.m_motionState.m_transform, pivot, twist_axis, plane_axis);
    data->setAsymmetricConeAngle(rci.v_fov_neg, rci.v_fov_pos);
    return havok_constraint_create(child_rb, host_rb, data, threshold, hkpConstraintInstance::PRIORITY_PSI);
}

// ---- collectors
// 0x008227F0 (:1666)
// Phantoms used as query shapes: a unit sphere, a 1 m box and a capsule along y, outside the collision (no
// collision layer).
static void havok_collectors_init()
{
    hkpSphereShape* sphere = new hkpSphereShape(1.0f);
    Sphere_collector = new hkpSimpleShapePhantom(sphere, hkTransformf::getIdentity(), COLLIDE_NO_COLLISION);
    Havok_world->addPhantom(Sphere_collector);
    sphere->removeReference();

    hkVector4f half_extents;
    half_extents.set(1.0f, 1.0f, 1.0f, 0.0f);
    hkpBoxShape* box = new hkpBoxShape(half_extents, hkConvexShapeDefaultRadius);
    Box_collector = new hkpSimpleShapePhantom(box, hkTransformf::getIdentity(), COLLIDE_NO_COLLISION);
    Havok_world->addPhantom(Box_collector);
    box->removeReference();

    hkVector4f vertex_a;
    hkVector4f vertex_b;
    vertex_a.set(0.0f, 0.0f, 0.0f, 0.0f);
    vertex_b.set(0.0f, 1.0f, 0.0f, 0.0f);
    hkpCapsuleShape* capsule = new hkpCapsuleShape(vertex_a, vertex_b, 0.5f);
    Capsule_collector = new hkpSimpleShapePhantom(capsule, hkTransformf::getIdentity(), COLLIDE_NO_COLLISION);
    Havok_world->addPhantom(Capsule_collector);
    capsule->removeReference();
}

// 0x008229D0 (:1704)
static void havok_collectors_free()
{
    if (havok_remove_phantom_from_list(Sphere_collector) == -1)
    {
        Havok_world->removePhantom(Sphere_collector);
    }
    Sphere_collector->removeReference();
    Sphere_collector = nullptr;
    if (havok_remove_phantom_from_list(Box_collector) == -1)
    {
        Havok_world->removePhantom(Box_collector);
    }
    Box_collector->removeReference();
    Box_collector = nullptr;
    if (havok_remove_phantom_from_list(Capsule_collector) == -1)
    {
        Havok_world->removePhantom(Capsule_collector);
    }
    Capsule_collector->removeReference();
    Capsule_collector = nullptr;
}

// 0x00822A70 (:1730)
// LTCG: rb in ESI. A navmesh body keeps its zone as the object; other bodies with an object get the object and
// their physical and effect materials.
static void havok_body_setup_user_properties(hkpRigidBody* rb, const physics_create_info& pci)
{
    if (pci.shape_type == HAVOK_SHAPE_TYPE_NAVMESH)
    {
        if (!rb->hasProperty(HAVOK_PROPERTY_OBJECT, hkpWorldObject::MULTI_THREADING_CHECKS_ENABLE))
        {
            rb->addProperty(HAVOK_PROPERTY_OBJECT, hkSimplePropertyValue((void*)pci.obj_ptr));
        }
        return;
    }
    if (pci.obj_ptr == nullptr)
    {
        return;
    }
    if (!rb->hasProperty(HAVOK_PROPERTY_OBJECT, hkpWorldObject::MULTI_THREADING_CHECKS_ENABLE))
    {
        rb->addProperty(HAVOK_PROPERTY_OBJECT, hkSimplePropertyValue((void*)pci.obj_ptr));
    }
    if (!rb->hasProperty(HAVOK_PROPERTY_PHYSICAL_MATERIAL, hkpWorldObject::MULTI_THREADING_CHECKS_ENABLE))
    {
        rb->addProperty(HAVOK_PROPERTY_PHYSICAL_MATERIAL, hkSimplePropertyValue((unsigned int)pci.physical_material));
    }
    if (!rb->hasProperty(HAVOK_PROPERTY_EFFECT_MATERIAL, hkpWorldObject::MULTI_THREADING_CHECKS_ENABLE))
    {
        rb->addProperty(HAVOK_PROPERTY_EFFECT_MATERIAL, hkSimplePropertyValue((unsigned int)pci.effect_material));
    }
}

// 0x00822B50 (:1797)
// Replacements requested during the step are applied after it; a request made while the world is still simulating
// is queued again (up to 100).
static void havok_replace_collision_models_end_sim()
{
    const unsigned int num = Num_collision_models_to_replace;
    for (unsigned int i = 0; i < num; i++)
    {
        const unsigned int cm_to_use = Collision_models_to_replace[i].m_cm_to_use;
        const unsigned int handle = Collision_models_to_replace[i].m_havok_body_handle;
        if (Havok_world_is_simulating)
        {
            if (Num_collision_models_to_replace < 100)
            {
                Collision_models_to_replace[Num_collision_models_to_replace].m_havok_body_handle = handle;
                Collision_models_to_replace[Num_collision_models_to_replace].m_cm_to_use = cm_to_use;
                Num_collision_models_to_replace++;
            }
        }
        else if (handle != 0xFFFFFFFF && Hk_rigid_bodies[handle] != nullptr)
        {
            Hk_rigid_bodies[handle]->setShape(havok_cm_manager_get(cm_to_use));
        }
    }
    Num_collision_models_to_replace = 0;
}

// ---- interpolation
// 0x0080E080 (:1814)
matrix43 havok_interpolate_transform(const matrix43& old_transform, const matrix43& new_transform)
{
    return m43_slerp_matrix(old_transform, new_transform, Havok_interpolate_percent);
}

// 0x0080E0B0 (:1819)
void havok_interpolate_object(object& obj, const matrix43& old_transform, const matrix43& new_transform)
{
    const matrix43 transform = m43_slerp_matrix(old_transform, new_transform, Havok_interpolate_percent);
    obj.update_pos_and_orient(transform.m_translation, transform.m_rotation, false);
}

// 0x0080E100 (:1825)
void havok_interpolate_object(object& obj, const matrix43& new_transform)
{
    const matrix43 old_transform(obj.pos, obj.orient);
    const matrix43 transform = m43_slerp_matrix(old_transform, new_transform, Havok_interpolate_percent);
    obj.update_pos_and_orient(transform.m_translation, transform.m_rotation, false);
}

// 0x00822BE0 (:1833)
// Copies the transforms of the bodies in active islands to their objects (not for bodies with property 7, nor for
// vehicles and turrets), as an orient (body rotation columns) and position.
static void havok_update_pos_and_orient()
{
    hkArray<hkpSimulationIsland*, hkContainerHeapAllocator>& islands = Havok_world->m_activeSimulationIslands;
    for (int i = 0; i < islands.getSize(); i++)
    {
        hkpSimulationIsland* island = islands[i];
        if (island->m_isInActiveIslandsArray == 0)
        {
            continue;
        }
        for (int e = 0; e < island->m_entities.getSize(); e++)
        {
            hkpEntity* entity = island->m_entities[e];
            if (entity->hasProperty(7, hkpWorldObject::MULTI_THREADING_CHECKS_ENABLE) ||
                !entity->hasProperty(HAVOK_PROPERTY_OBJECT, hkpWorldObject::MULTI_THREADING_CHECKS_ENABLE))
            {
                continue;
            }
            object* objp = (object*)entity->getProperty(HAVOK_PROPERTY_OBJECT, hkpWorldObject::MULTI_THREADING_CHECKS_ENABLE).getPtr();
            if (objp == nullptr || objp->get_obj_type() == OT_VEHICLE || objp->get_obj_type() == OT_TURRET) // moved by their own code
            {
                continue;
            }
            const hkTransformf& transform = entity->getCollidable()->getTransform();
            const float* r = (const float*)&transform;
            matrix orient;
            orient.rvec.set(r[0], r[1], r[2]);
            orient.uvec.set(r[4], r[5], r[6]);
            orient.fvec.set(r[8], r[9], r[10]);
            vector pos(r[12], r[13], r[14]);
            objp->update_pos_and_orient(pos, orient, false);
        }
    }
}

// 0x0080E160 (:1887)
// LTCG: collidable in EDX, saved filter in ECX. Disabling saves the filter info and moves the collidable to the no
// collision layer; enabling restores the saved filter info.
static void havok_collidable_enable_collision(hkpCollidable* collidable_ptr, unsigned int* saved_collision_filter_ptr, bool enable)
{
    const unsigned int filter_info = collidable_ptr->m_broadPhaseHandle.m_collisionFilterInfo;
    if (enable)
    {
        if (filter_info != *saved_collision_filter_ptr)
        {
            collidable_ptr->m_broadPhaseHandle.m_collisionFilterInfo = *saved_collision_filter_ptr;
        }
    }
    else if (filter_info != COLLIDE_NO_COLLISION)
    {
        *saved_collision_filter_ptr = filter_info;
        collidable_ptr->m_broadPhaseHandle.m_collisionFilterInfo = COLLIDE_NO_COLLISION;
    }
}

// 0x0080E190 (:2044)
// LTCG: info in EAX, objp in ECX. Whether the object's box touches the filter sphere or oriented box.
static bool havok_collect_objects_filter(object* objp, const collect_objects_filter_info* info)
{
    if (objp == nullptr)
    {
        return false;
    }
    vector bmin;
    vector bmax;
    if (info->type == collect_objects_filter_info::SPHERE)
    {
        objp->get_bbox(bmin, bmax);
        return ix_sphere_in_box(info->center, info->radius, bmin, bmax);
    }
    if (info->type == collect_objects_filter_info::OBB)
    {
        objp->get_local_bbox(bmin, bmax);
        vector extents;
        vector center;
        bbox_extract_obb_info(bmin, bmax, objp->pos, objp->orient, extents, center);
        return ix_oriented_box_box(info->extents, info->center, info->orient, extents, center, objp->orient);
    }
    return false;
}

// ---- multiplayer
// 0x0080E250 (:2071)
static void havok_multiplayer_level_init_pre()
{
    Havok_filter->disableCollisionsBetween(44, 7); // RFG_WORLD_ONLY, NPC
    Havok_filter->disableCollisionsBetween(44, 6); // RFG_WORLD_ONLY, PLAYER
    Havok_filter->disableCollisionsBetween(44, 16); // RFG_WORLD_ONLY, PROJECTILE
}

// 0x0080E290 (:2080)
static void havok_multiplayer_level_shutdown()
{
    Havok_filter->enableCollisionsBetween(44, 7); // RFG_WORLD_ONLY, NPC
    Havok_filter->enableCollisionsBetween(44, 6); // RFG_WORLD_ONLY, PLAYER
    Havok_filter->enableCollisionsBetween(44, 16); // RFG_WORLD_ONLY, PROJECTILE
}

// 0x00816770 (:2092)
// The simulation is in warning after a 15 s and in error after a 30 s Havok step; the memory system is in error when
// the solver cannot allocate. Then the out of memory state is cleared.
static void havok_update_status()
{
    for (int i = 0; i < NUM_HAVOK_SYSTEMS; i++)
    {
        Havok_system_info[i].do_frame();
    }
    const int sim_time = gamestate_gp_last_havok_sim_time();
    if (sim_time > 30000) // ms
    {
        havok_set_status(HAVOK_SYSTEM_SIMULATION, HAVOK_STATUS_ERROR);
    }
    else if (sim_time > 15000) // ms
    {
        havok_set_status(HAVOK_SYSTEM_SIMULATION, HAVOK_STATUS_WARNING);
    }
    if (!hkMemorySystem::getInstance().solverCanAllocSingleBlock(0))
    {
        havok_set_status(HAVOK_SYSTEM_MEMORY, HAVOK_STATUS_ERROR);
    }
    hkSetOutOfMemoryState(hkMemoryAllocator::MEMORY_STATE_OK);
}

// 0x008168F0 (:2130)
physics_create_info::physics_create_info()
{
    mass = 1.0f;
    max_linear_vel = 100.0f;
    linear_damping = 0.01f;
    max_angular_vel = 4.0f;
    angular_damping = 0.05f;
    friction = 0.5f;
    restitution = 0.05f;
    obj_ptr = nullptr;
    collision_type = COLLIDE_NO_COLLISION;
    flags = Havok_world_is_simulating ? 4 : 0; // 4: batched add (bodies created during the step are added after it)
    cm_index = 0xFFFFFFFF;
    simulation_quality_type = HK_COLLIDABLE_QUALITY_DEBRIS;
    solver_deactivation = hkpRigidBodyCinfo::SOLVER_DEACTIVATION_LOW;
    motion_type = 1; // hkpMotion::MOTION_DYNAMIC
    shape = nullptr;
    enter_callback = nullptr;
    leave_callback = nullptr;
    shape_type = HAVOK_SHAPE_TYPE_UNDEFINED;
    physical_material = Rfg_physical_material_default_index;
    effect_material = Rfg_effect_material_default_index;
    auto_remove_level = 0;
}

// ---- system
// 0x00839A20 (:2181)
// Creates the world (gravity -9.8, broadphase +-4343.5 x +-1000 x +-4343.5 m; the safe broadphase is 1 m inside),
// the game filter (behind a constraint filter) with the collision layer matrix, the broadphase border, the
// collectors and the per-thread Havok memory (one setup job per Havok scheduler controller).
void havok_system_init()
{
    if (Havok_system_init)
    {
        return;
    }
    Havok_system_init = true;
    havok_memory_init();
    Havok_add_entity_list.init((hkpEntity**)Permanent_mempool.alloc(0x1000, 0), 0x400, 0);
    Havok_remove_entity_list.init((hkpEntity**)Permanent_mempool.alloc(0x1000, 0), 0x400, 0);
    Havok_add_phantom_list.init((hkpPhantom**)Permanent_mempool.alloc(0x200, 0), 0x80, 0);
    Havok_remove_phantom_list.init((hkpPhantom**)Permanent_mempool.alloc(0x200, 0), 0x80, 0);

    hkpWorldCinfo info;
    info.setupSolverInfo(hkpWorldCinfo::SOLVER_TYPE_4ITERS_MEDIUM);
    info.m_gravity.set(0.0f, -9.8f, 0.0f, 0.0f);
    info.m_broadPhaseWorldAabb.m_min.set(-4343.5f, -1000.0f, -4343.5f, 0.0f);
    info.m_broadPhaseWorldAabb.m_max.set(4343.5f, 1000.0f, 4343.5f, 0.0f);
    Havok_safe_broadphase.min.x = -4342.5f;
    Havok_safe_broadphase.min.z = -4342.5f;
    Havok_safe_broadphase.max.x = 4342.5f;
    Havok_safe_broadphase.max.z = 4342.5f;
    info.m_collisionTolerance = 0.04f;
    info.m_contactRestingVelocity = 0.2f;
    Havok_safe_broadphase.min.y = -999.0f;
    info.m_expectedMaxLinearVelocity = 100.0f;
    Havok_safe_broadphase.max.y = 999.0f;
    info.m_expectedMinPsiDeltaTime = Havok_step_time;
    info.m_broadPhaseBorderBehaviour.m_storage = hkpWorldCinfo::BROADPHASE_BORDER_DO_NOTHING;
    info.m_simulationType.m_storage = hkpWorldCinfo::SIMULATION_TYPE_CONTINUOUS;
    Havok_filter = new havok_game_filter(11, Havok_game_filter_layer_lookup_table_buffer);
    Constraint_filter = new hkpConstraintCollisionFilter(Havok_filter);
    info.m_collisionFilter = Constraint_filter;
    hkConvexShapeDefaultRadius = 0.01f;
    hkpCylinderShape::setNumberOfVirtualSideSegments(64);
    Havok_world = new hkpWorld(info, HAVOK_SDK_VERSION_NUMBER);
    // the world (through the constraint filter) holds the filters now
    Constraint_filter->removeReference();
    Havok_filter->removeReference();
    game_broad_phase_border* border = new game_broad_phase_border(Havok_world, hkpWorldCinfo::BROADPHASE_BORDER_REMOVE_ENTITY);
    Havok_world->setBroadPhaseBorder(border);
    border->removeReference();
    hkpAgentRegisterUtil::registerAllAgents(Havok_world->m_collisionDispatcher);

    // The collision layer matrix (layer names from the COLLIDE_* collision filter infos; layers 0, 1, 10, 11, 13,
    // 26, 35, 47, 51 and 53 have no named filter info).
    Havok_filter->disable_collisions_for_layer(22); // NO_COLLISION
    Havok_filter->disableCollisionsBetween(6, 4); // PLAYER, ITEM
    Havok_filter->disableCollisionsBetween(7, 4); // NPC, ITEM
    Havok_filter->disableCollisionsBetween(4, 5); // ITEM, RAGDOLL
    Havok_filter->disableCollisionsBetween(4, 16); // ITEM, PROJECTILE
    Havok_filter->disableCollisionsBetween(4, 3); // ITEM, MOVER
    Havok_filter->disableCollisionsBetween(19, 2); // OBJECT_COLLECTOR, WORLD
    Havok_filter->disableCollisionsBetween(19, 5); // OBJECT_COLLECTOR, RAGDOLL
    Havok_filter->enableCollisionsBetween(19, 9); // OBJECT_COLLECTOR, VEHICLE
    Havok_filter->enableCollisionsBetween(19, 10); // OBJECT_COLLECTOR, ?
    Havok_filter->disableCollisionsBetween(19, 11); // OBJECT_COLLECTOR, ?
    Havok_filter->disableCollisionsBetween(20, 2); // OBJECT_COLLECTOR_NO_ITEMS, WORLD
    Havok_filter->enableCollisionsBetween(20, 9); // OBJECT_COLLECTOR_NO_ITEMS, VEHICLE
    Havok_filter->enableCollisionsBetween(20, 10); // OBJECT_COLLECTOR_NO_ITEMS, ?
    Havok_filter->disableCollisionsBetween(20, 11); // OBJECT_COLLECTOR_NO_ITEMS, ?
    Havok_filter->disableCollisionsBetween(20, 16); // OBJECT_COLLECTOR_NO_ITEMS, PROJECTILE
    Havok_filter->disableCollisionsBetween(20, 4); // OBJECT_COLLECTOR_NO_ITEMS, ITEM
    Havok_filter->disable_collisions_for_layer(21); // OBJECT_COLLECTOR_VEHICLES_ONLY
    Havok_filter->enableCollisionsBetween(21, 9); // OBJECT_COLLECTOR_VEHICLES_ONLY, VEHICLE
    Havok_filter->enableCollisionsBetween(21, 10); // OBJECT_COLLECTOR_VEHICLES_ONLY, ?
    Havok_filter->disableCollisionsBetween(21, 11); // OBJECT_COLLECTOR_VEHICLES_ONLY, ?
    Havok_filter->enableCollisionsBetween(21, 62); // OBJECT_COLLECTOR_VEHICLES_ONLY, FORCE_FIELD
    Havok_filter->disableCollisionsBetween(36, 2); // KEYFRAMED, WORLD
    Havok_filter->disableCollisionsBetween(36, 4); // KEYFRAMED, ITEM
    Havok_filter->disable_collisions_for_layer(37); // DEAD_HUMAN
    Havok_filter->enableCollisionsBetween(37, 19); // DEAD_HUMAN, OBJECT_COLLECTOR
    Havok_filter->enableCollisionsBetween(37, 25); // DEAD_HUMAN, RAY_DEFAULT
    Havok_filter->enableCollisionsBetween(37, 62); // DEAD_HUMAN, FORCE_FIELD
    Havok_filter->enableCollisionsBetween(37, 16); // DEAD_HUMAN, PROJECTILE
    Havok_filter->disableCollisionsBetween(38, 4); // PHYSICAL_OBJECT, ITEM
    Havok_filter->disableCollisionsBetween(47, 2); // ?, WORLD
    Havok_filter->disableCollisionsBetween(47, 4); // ?, ITEM
    Havok_filter->enableCollisionsBetween(47, 9); // ?, VEHICLE
    Havok_filter->enableCollisionsBetween(47, 10); // ?, ?
    Havok_filter->disableCollisionsBetween(47, 11); // ?, ?
    Havok_filter->disable_collisions_for_layer(48); // CAMERA
    Havok_filter->enableCollisionsBetween(48, 2); // CAMERA, WORLD
    Havok_filter->enableCollisionsBetween(48, 9); // CAMERA, VEHICLE
    Havok_filter->enableCollisionsBetween(48, 11); // CAMERA, ?
    Havok_filter->disableCollisionsBetween(48, 10); // CAMERA, ?
    Havok_filter->disableCollisionsBetween(4, 40); // ITEM, RFG_NORMAL
    Havok_filter->copy_layer(41, 40); // RFG_NORMAL_FAST from RFG_NORMAL
    Havok_filter->disableCollisionsBetween(41, 42); // RFG_NORMAL_FAST, RFG_LITE
    Havok_filter->disableCollisionsBetween(41, 43); // RFG_NORMAL_FAST, RFG_ULTRA_LITE
    Havok_filter->disableCollisionsBetween(41, 44); // RFG_NORMAL_FAST, RFG_WORLD_ONLY
    Havok_filter->copy_layer(42, 41); // RFG_LITE from RFG_NORMAL_FAST
    Havok_filter->copy_layer(43, 42); // RFG_ULTRA_LITE from RFG_LITE
    Havok_filter->disableCollisionsBetween(43, 9); // RFG_ULTRA_LITE, VEHICLE
    Havok_filter->disableCollisionsBetween(43, 10); // RFG_ULTRA_LITE, ?
    Havok_filter->disableCollisionsBetween(43, 11); // RFG_ULTRA_LITE, ?
    Havok_filter->disable_collisions_for_layer(44); // RFG_WORLD_ONLY
    Havok_filter->enableCollisionsBetween(44, 2); // RFG_WORLD_ONLY, WORLD
    Havok_filter->enableCollisionsBetween(44, 16); // RFG_WORLD_ONLY, PROJECTILE
    Havok_filter->enableCollisionsBetween(44, 7); // RFG_WORLD_ONLY, NPC
    Havok_filter->enableCollisionsBetween(44, 6); // RFG_WORLD_ONLY, PLAYER
    Havok_filter->enableCollisionsBetween(44, 25); // RFG_WORLD_ONLY, RAY_DEFAULT
    Havok_filter->enableCollisionsBetween(44, 19); // RFG_WORLD_ONLY, OBJECT_COLLECTOR
    Havok_filter->enableCollisionsBetween(44, 20); // RFG_WORLD_ONLY, OBJECT_COLLECTOR_NO_ITEMS
    Havok_filter->enableCollisionsBetween(44, 62); // RFG_WORLD_ONLY, FORCE_FIELD
    Havok_filter->disable_collisions_for_layer(46); // RFG_STRESS
    Havok_filter->enableCollisionsBetween(46, 7); // RFG_STRESS, NPC
    Havok_filter->enableCollisionsBetween(46, 6); // RFG_STRESS, PLAYER
    Havok_filter->enableCollisionsBetween(46, 5); // RFG_STRESS, RAGDOLL
    Havok_filter->enableCollisionsBetween(46, 37); // RFG_STRESS, DEAD_HUMAN
    Havok_filter->enableCollisionsBetween(46, 9); // RFG_STRESS, VEHICLE
    Havok_filter->enableCollisionsBetween(46, 10); // RFG_STRESS, ?
    Havok_filter->disableCollisionsBetween(46, 11); // RFG_STRESS, ?
    Havok_filter->enableCollisionsBetween(46, 3); // RFG_STRESS, MOVER
    Havok_filter->enableCollisionsBetween(46, 40); // RFG_STRESS, RFG_NORMAL
    Havok_filter->enableCollisionsBetween(46, 41); // RFG_STRESS, RFG_NORMAL_FAST
    Havok_filter->disable_collisions_for_layer(54); // WORLD_ANCHOR
    Havok_filter->enableCollisionsBetween(54, 2); // WORLD_ANCHOR, WORLD
    Havok_filter->disable_collisions_for_layer(55); // DYNAMIC_LINK
    Havok_filter->enableCollisionsBetween(55, 40); // DYNAMIC_LINK, RFG_NORMAL
    Havok_filter->disable_collisions_for_layer(51); // ?
    Havok_filter->enableCollisionsBetween(51, 25); // ?, RAY_DEFAULT
    Havok_filter->enableCollisionsBetween(51, 19); // ?, OBJECT_COLLECTOR
    Havok_filter->enableCollisionsBetween(51, 62); // ?, FORCE_FIELD
    Havok_filter->enableCollisionsBetween(51, 2); // ?, WORLD
    Havok_filter->enableCollisionsBetween(51, 39); // ?, RFG_INITIAL_POSITION
    Havok_filter->enableCollisionsBetween(9, 1); // VEHICLE, ?
    Havok_filter->enableCollisionsBetween(11, 1); // ?, ?
    Havok_filter->disableCollisionsBetween(10, 1); // ?, ?
    Havok_filter->enableCollisionsBetween(9, 2); // VEHICLE, WORLD
    Havok_filter->enableCollisionsBetween(10, 2); // ?, WORLD
    Havok_filter->disableCollisionsBetween(11, 2); // ?, WORLD
    Havok_filter->enableCollisionsBetween(9, 3); // VEHICLE, MOVER
    Havok_filter->enableCollisionsBetween(11, 3); // ?, MOVER
    Havok_filter->disableCollisionsBetween(10, 3); // ?, MOVER
    Havok_filter->enableCollisionsBetween(9, 4); // VEHICLE, ITEM
    Havok_filter->enableCollisionsBetween(11, 4); // ?, ITEM
    Havok_filter->disableCollisionsBetween(10, 4); // ?, ITEM
    Havok_filter->enableCollisionsBetween(9, 5); // VEHICLE, RAGDOLL
    Havok_filter->enableCollisionsBetween(11, 5); // ?, RAGDOLL
    Havok_filter->disableCollisionsBetween(10, 5); // ?, RAGDOLL
    Havok_filter->enableCollisionsBetween(9, 6); // VEHICLE, PLAYER
    Havok_filter->enableCollisionsBetween(11, 6); // ?, PLAYER
    Havok_filter->disableCollisionsBetween(10, 6); // ?, PLAYER
    Havok_filter->enableCollisionsBetween(9, 7); // VEHICLE, NPC
    Havok_filter->enableCollisionsBetween(11, 7); // ?, NPC
    Havok_filter->disableCollisionsBetween(10, 7); // ?, NPC
    Havok_filter->enableCollisionsBetween(9, 9); // VEHICLE, VEHICLE
    Havok_filter->enableCollisionsBetween(10, 9); // ?, VEHICLE
    Havok_filter->disableCollisionsBetween(11, 9); // ?, VEHICLE
    Havok_filter->enableCollisionsBetween(10, 10); // ?, ?
    Havok_filter->disableCollisionsBetween(11, 10); // ?, ?
    Havok_filter->disableCollisionsBetween(11, 11); // ?, ?
    Havok_filter->enableCollisionsBetween(9, 16); // VEHICLE, PROJECTILE
    Havok_filter->enableCollisionsBetween(11, 16); // ?, PROJECTILE
    Havok_filter->disableCollisionsBetween(10, 16); // ?, PROJECTILE
    Havok_filter->enableCollisionsBetween(9, 25); // VEHICLE, RAY_DEFAULT
    Havok_filter->enableCollisionsBetween(11, 25); // ?, RAY_DEFAULT
    Havok_filter->disableCollisionsBetween(10, 25); // ?, RAY_DEFAULT
    Havok_filter->enableCollisionsBetween(9, 26); // VEHICLE, ?
    Havok_filter->enableCollisionsBetween(11, 26); // ?, ?
    Havok_filter->disableCollisionsBetween(10, 26); // ?, ?
    Havok_filter->enableCollisionsBetween(9, 35); // VEHICLE, RAY_VEHICLE_ENTER_EXIT
    Havok_filter->enableCollisionsBetween(11, 35); // ?, RAY_VEHICLE_ENTER_EXIT
    Havok_filter->disableCollisionsBetween(10, 35); // ?, RAY_VEHICLE_ENTER_EXIT
    Havok_filter->enableCollisionsBetween(9, 36); // VEHICLE, KEYFRAMED
    Havok_filter->enableCollisionsBetween(10, 36); // ?, KEYFRAMED
    Havok_filter->disableCollisionsBetween(11, 36); // ?, KEYFRAMED
    Havok_filter->enableCollisionsBetween(9, 38); // VEHICLE, PHYSICAL_OBJECT
    Havok_filter->enableCollisionsBetween(11, 38); // ?, PHYSICAL_OBJECT
    Havok_filter->disableCollisionsBetween(10, 38); // ?, PHYSICAL_OBJECT
    Havok_filter->enableCollisionsBetween(9, 39); // VEHICLE, RFG_INITIAL_POSITION
    Havok_filter->enableCollisionsBetween(10, 39); // ?, RFG_INITIAL_POSITION
    Havok_filter->disableCollisionsBetween(11, 39); // ?, RFG_INITIAL_POSITION
    Havok_filter->disableCollisionsBetween(47, 39); // ?, RFG_INITIAL_POSITION
    Havok_filter->enableCollisionsBetween(9, 40); // VEHICLE, RFG_NORMAL
    Havok_filter->enableCollisionsBetween(10, 40); // ?, RFG_NORMAL
    Havok_filter->disableCollisionsBetween(11, 40); // ?, RFG_NORMAL
    Havok_filter->disableCollisionsBetween(47, 40); // ?, RFG_NORMAL
    Havok_filter->enableCollisionsBetween(9, 41); // VEHICLE, RFG_NORMAL_FAST
    Havok_filter->enableCollisionsBetween(10, 41); // ?, RFG_NORMAL_FAST
    Havok_filter->disableCollisionsBetween(11, 41); // ?, RFG_NORMAL_FAST
    Havok_filter->disableCollisionsBetween(47, 41); // ?, RFG_NORMAL_FAST
    Havok_filter->enableCollisionsBetween(9, 42); // VEHICLE, RFG_LITE
    Havok_filter->enableCollisionsBetween(10, 42); // ?, RFG_LITE
    Havok_filter->disableCollisionsBetween(11, 42); // ?, RFG_LITE
    Havok_filter->disableCollisionsBetween(47, 42); // ?, RFG_LITE
    Havok_filter->enableCollisionsBetween(9, 62); // VEHICLE, FORCE_FIELD
    Havok_filter->enableCollisionsBetween(10, 62); // ?, FORCE_FIELD
    Havok_filter->disableCollisionsBetween(11, 62); // ?, FORCE_FIELD
    Havok_filter->copy_layer(52, 7); // HUMAN_NOT_OTHER_HUMANS from NPC
    Havok_filter->disableCollisionsBetween(52, 7); // HUMAN_NOT_OTHER_HUMANS, NPC
    Havok_filter->disableCollisionsBetween(52, 52); // HUMAN_NOT_OTHER_HUMANS, HUMAN_NOT_OTHER_HUMANS
    Havok_filter->disable_collisions_for_layer(58); // EFFECT_RIGID_BODY
    Havok_filter->enableCollisionsBetween(58, 2); // EFFECT_RIGID_BODY, WORLD
    Havok_filter->enableCollisionsBetween(58, 39); // EFFECT_RIGID_BODY, RFG_INITIAL_POSITION
    Havok_filter->enableCollisionsBetween(58, 62); // EFFECT_RIGID_BODY, FORCE_FIELD
    Havok_filter->disableCollisionsBetween(16, 16); // PROJECTILE, PROJECTILE
    Havok_filter->enableCollisionsBetween(16, 7); // PROJECTILE, NPC
    Havok_filter->disable_collisions_for_layer(17); // PROJECTILE_ATTACHED
    Havok_filter->enableCollisionsBetween(17, 25); // PROJECTILE_ATTACHED, RAY_DEFAULT
    Havok_filter->enableCollisionsBetween(17, 19); // PROJECTILE_ATTACHED, OBJECT_COLLECTOR
    Havok_filter->enableCollisionsBetween(17, 16); // PROJECTILE_ATTACHED, PROJECTILE
    Havok_filter->enableCollisionsBetween(17, 62); // PROJECTILE_ATTACHED, FORCE_FIELD
    Havok_filter->copy_layer(8, 4); // TURRET from ITEM
    Havok_filter->enableCollisionsBetween(8, 25); // TURRET, RAY_DEFAULT
    Havok_filter->enableCollisionsBetween(8, 16); // TURRET, PROJECTILE
    Havok_filter->enableCollisionsBetween(8, 5); // TURRET, RAGDOLL
    Havok_filter->enableCollisionsBetween(8, 6); // TURRET, PLAYER
    Havok_filter->enableCollisionsBetween(8, 7); // TURRET, NPC
    Havok_filter->enableCollisionsBetween(8, 24); // TURRET, VEHICLE_NAVMESH
    Havok_filter->enableCollisionsBetween(8, 9); // TURRET, VEHICLE
    Havok_filter->enableCollisionsBetween(8, 10); // TURRET, ?
    Havok_filter->disableCollisionsBetween(8, 11); // TURRET, ?
    Havok_filter->enableCollisionsBetween(8, 3); // TURRET, MOVER
    Havok_filter->disableCollisionsBetween(8, 2); // TURRET, WORLD
    Havok_filter->disableCollisionsBetween(8, 39); // TURRET, RFG_INITIAL_POSITION
    Havok_filter->copy_layer(50, 7); // HUMAN_ON_LADDER from NPC
    Havok_filter->disableCollisionsBetween(50, 6); // HUMAN_ON_LADDER, PLAYER
    Havok_filter->disableCollisionsBetween(50, 50); // HUMAN_ON_LADDER, HUMAN_ON_LADDER
    Havok_filter->disableCollisionsBetween(50, 39); // HUMAN_ON_LADDER, RFG_INITIAL_POSITION
    Havok_filter->disable_collisions_for_layer(18); // PERSONAL_SHIELD
    Havok_filter->enableCollisionsBetween(18, 43); // PERSONAL_SHIELD, RFG_ULTRA_LITE
    Havok_filter->enableCollisionsBetween(18, 4); // PERSONAL_SHIELD, ITEM
    Havok_filter->enableCollisionsBetween(18, 16); // PERSONAL_SHIELD, PROJECTILE
    Havok_filter->enableCollisionsBetween(18, 62); // PERSONAL_SHIELD, FORCE_FIELD
    Havok_filter->disableCollisionsBetween(25, 19); // RAY_DEFAULT, OBJECT_COLLECTOR
    Havok_filter->disableCollisionsBetween(25, 20); // RAY_DEFAULT, OBJECT_COLLECTOR_NO_ITEMS
    Havok_filter->disableCollisionsBetween(25, 5); // RAY_DEFAULT, RAGDOLL
    Havok_filter->disableCollisionsBetween(25, 4); // RAY_DEFAULT, ITEM
    Havok_filter->disableCollisionsBetween(25, 7); // RAY_DEFAULT, NPC
    Havok_filter->disableCollisionsBetween(25, 6); // RAY_DEFAULT, PLAYER
    Havok_filter->disable_collisions_for_layer(49); // HUMAN_HIT_DETECT
    Havok_filter->enableCollisionsBetween(49, 25); // HUMAN_HIT_DETECT, RAY_DEFAULT
    Havok_filter->enableCollisionsBetween(49, 62); // HUMAN_HIT_DETECT, FORCE_FIELD
    Havok_filter->enableCollisionsBetween(7, 25); // NPC, RAY_DEFAULT
    Havok_filter->enableCollisionsBetween(18, 25); // PERSONAL_SHIELD, RAY_DEFAULT
    Havok_filter->disable_collisions_for_layer(59); // ONLY_FOR_OBJECT_COLLECTION
    Havok_filter->enableCollisionsBetween(59, 19); // ONLY_FOR_OBJECT_COLLECTION, OBJECT_COLLECTOR
    Havok_filter->disable_collisions_for_layer(28); // RAY_WORLD_ONLY
    Havok_filter->enableCollisionsBetween(28, 2); // RAY_WORLD_ONLY, WORLD
    Havok_filter->enableCollisionsBetween(28, 39); // RAY_WORLD_ONLY, RFG_INITIAL_POSITION
    Havok_filter->disable_collisions_for_layer(29); // RAY_WALKING_SURFACE_ONLY
    Havok_filter->enableCollisionsBetween(29, 2); // RAY_WALKING_SURFACE_ONLY, WORLD
    Havok_filter->enableCollisionsBetween(29, 39); // RAY_WALKING_SURFACE_ONLY, RFG_INITIAL_POSITION
    Havok_filter->enableCollisionsBetween(29, 40); // RAY_WALKING_SURFACE_ONLY, RFG_NORMAL
    Havok_filter->enableCollisionsBetween(29, 41); // RAY_WALKING_SURFACE_ONLY, RFG_NORMAL_FAST
    Havok_filter->enableCollisionsBetween(29, 42); // RAY_WALKING_SURFACE_ONLY, RFG_LITE
    Havok_filter->enableCollisionsBetween(29, 43); // RAY_WALKING_SURFACE_ONLY, RFG_ULTRA_LITE
    Havok_filter->enableCollisionsBetween(29, 44); // RAY_WALKING_SURFACE_ONLY, RFG_WORLD_ONLY
    Havok_filter->enableCollisionsBetween(29, 9); // RAY_WALKING_SURFACE_ONLY, VEHICLE
    Havok_filter->enableCollisionsBetween(29, 11); // RAY_WALKING_SURFACE_ONLY, ?
    Havok_filter->disableCollisionsBetween(29, 10); // RAY_WALKING_SURFACE_ONLY, ?
    Havok_filter->disable_collisions_for_layer(27); // RAY_TERRAIN_ONLY
    Havok_filter->enableCollisionsBetween(27, 2); // RAY_TERRAIN_ONLY, WORLD
    Havok_filter->disable_collisions_for_layer(30); // RAY_CITY
    Havok_filter->enableCollisionsBetween(30, 2); // RAY_CITY, WORLD
    Havok_filter->enableCollisionsBetween(30, 36); // RAY_CITY, KEYFRAMED
    Havok_filter->enableCollisionsBetween(30, 3); // RAY_CITY, MOVER
    Havok_filter->copy_layer(33, 40); // RAY_PLAYER_COVER from RFG_NORMAL
    Havok_filter->disableCollisionsBetween(33, 7); // RAY_PLAYER_COVER, NPC
    Havok_filter->disableCollisionsBetween(33, 5); // RAY_PLAYER_COVER, RAGDOLL
    Havok_filter->disableCollisionsBetween(33, 6); // RAY_PLAYER_COVER, PLAYER
    Havok_filter->disableCollisionsBetween(33, 2); // RAY_PLAYER_COVER, WORLD
    Havok_filter->disableCollisionsBetween(33, 16); // RAY_PLAYER_COVER, PROJECTILE
    Havok_filter->enableCollisionsBetween(47, 33); // ?, RAY_PLAYER_COVER
    Havok_filter->disable_collisions_for_layer(32); // RAY_RAGDOLL_ONLY
    Havok_filter->enableCollisionsBetween(32, 5); // RAY_RAGDOLL_ONLY, RAGDOLL
    Havok_filter->disable_collisions_for_layer(23); // NAVMESH
    Havok_filter->disable_collisions_for_layer(31); // RAY_NAVMESH_ONLY
    Havok_filter->enableCollisionsBetween(31, 23); // RAY_NAVMESH_ONLY, NAVMESH
    Havok_filter->disable_collisions_for_layer(24); // VEHICLE_NAVMESH
    Havok_filter->disable_collisions_for_layer(34); // RAY_VEHICLE_NAVMESH_ONLY
    Havok_filter->enableCollisionsBetween(34, 24); // RAY_VEHICLE_NAVMESH_ONLY, VEHICLE_NAVMESH
    Havok_filter->disableCollisionsBetween(35, 19); // RAY_VEHICLE_ENTER_EXIT, OBJECT_COLLECTOR
    Havok_filter->disableCollisionsBetween(35, 20); // RAY_VEHICLE_ENTER_EXIT, OBJECT_COLLECTOR_NO_ITEMS
    Havok_filter->disableCollisionsBetween(35, 5); // RAY_VEHICLE_ENTER_EXIT, RAGDOLL
    Havok_filter->disableCollisionsBetween(35, 4); // RAY_VEHICLE_ENTER_EXIT, ITEM
    Havok_filter->disableCollisionsBetween(35, 16); // RAY_VEHICLE_ENTER_EXIT, PROJECTILE
    Havok_filter->disableCollisionsBetween(35, 7); // RAY_VEHICLE_ENTER_EXIT, NPC
    Havok_filter->disableCollisionsBetween(35, 6); // RAY_VEHICLE_ENTER_EXIT, PLAYER
    Havok_filter->disableCollisionsBetween(35, 43); // RAY_VEHICLE_ENTER_EXIT, RFG_ULTRA_LITE
    Havok_filter->enableCollisionsBetween(35, 62); // RAY_VEHICLE_ENTER_EXIT, FORCE_FIELD
    Havok_filter->disable_collisions_for_layer(57); // RAY_VEHICLE_WHEEL
    Havok_filter->enableCollisionsBetween(57, 2); // RAY_VEHICLE_WHEEL, WORLD
    Havok_filter->enableCollisionsBetween(57, 39); // RAY_VEHICLE_WHEEL, RFG_INITIAL_POSITION
    Havok_filter->enableCollisionsBetween(57, 40); // RAY_VEHICLE_WHEEL, RFG_NORMAL
    Havok_filter->enableCollisionsBetween(57, 41); // RAY_VEHICLE_WHEEL, RFG_NORMAL_FAST
    Havok_filter->enableCollisionsBetween(57, 42); // RAY_VEHICLE_WHEEL, RFG_LITE
    Havok_filter->enableCollisionsBetween(57, 3); // RAY_VEHICLE_WHEEL, MOVER
    Havok_filter->enableCollisionsBetween(57, 47); // RAY_VEHICLE_WHEEL, ?
    Havok_filter->enableCollisionsBetween(57, 38); // RAY_VEHICLE_WHEEL, PHYSICAL_OBJECT
    Havok_filter->enableCollisionsBetween(57, 9); // RAY_VEHICLE_WHEEL, VEHICLE
    Havok_filter->enableCollisionsBetween(57, 10); // RAY_VEHICLE_WHEEL, ?
    Havok_filter->disableCollisionsBetween(57, 11); // RAY_VEHICLE_WHEEL, ?
    Havok_filter->enableCollisionsBetween(57, 8); // RAY_VEHICLE_WHEEL, TURRET
    Havok_filter->enableCollisionsBetween(57, 4); // RAY_VEHICLE_WHEEL, ITEM
    Havok_filter->disable_collisions_for_layer(45); // RAY_RFG_DESTROYABLE
    Havok_filter->enableCollisionsBetween(45, 39); // RAY_RFG_DESTROYABLE, RFG_INITIAL_POSITION
    Havok_filter->enableCollisionsBetween(45, 40); // RAY_RFG_DESTROYABLE, RFG_NORMAL
    Havok_filter->enableCollisionsBetween(45, 41); // RAY_RFG_DESTROYABLE, RFG_NORMAL_FAST
    Havok_filter->enableCollisionsBetween(45, 3); // RAY_RFG_DESTROYABLE, MOVER
    Havok_filter->enableCollisionsBetween(45, 42); // RAY_RFG_DESTROYABLE, RFG_LITE
    Havok_filter->enableCollisionsBetween(45, 43); // RAY_RFG_DESTROYABLE, RFG_ULTRA_LITE
    Havok_filter->enableCollisionsBetween(45, 44); // RAY_RFG_DESTROYABLE, RFG_WORLD_ONLY
    Havok_filter->copy_layer(60, 4); // CLOTH_SIM from ITEM
    Havok_filter->disableCollisionsBetween(60, 9); // CLOTH_SIM, VEHICLE
    Havok_filter->disableCollisionsBetween(60, 10); // CLOTH_SIM, ?
    Havok_filter->disableCollisionsBetween(60, 11); // CLOTH_SIM, ?
    Havok_filter->disableCollisionsBetween(60, 5); // CLOTH_SIM, RAGDOLL
    Havok_filter->disable_collisions_for_layer(61); // INVISIBLE_BARRIER
    Havok_filter->enableCollisionsBetween(61, 5); // INVISIBLE_BARRIER, RAGDOLL
    Havok_filter->enableCollisionsBetween(61, 6); // INVISIBLE_BARRIER, PLAYER
    Havok_filter->enableCollisionsBetween(61, 7); // INVISIBLE_BARRIER, NPC
    Havok_filter->enableCollisionsBetween(61, 9); // INVISIBLE_BARRIER, VEHICLE
    Havok_filter->enableCollisionsBetween(61, 10); // INVISIBLE_BARRIER, ?
    Havok_filter->disableCollisionsBetween(61, 11); // INVISIBLE_BARRIER, ?
    Havok_filter->enableCollisionsBetween(61, 37); // INVISIBLE_BARRIER, DEAD_HUMAN
    Havok_filter->enableCollisionsBetween(61, 50); // INVISIBLE_BARRIER, HUMAN_ON_LADDER
    Havok_filter->enableCollisionsBetween(61, 51); // INVISIBLE_BARRIER, ?
    Havok_filter->enableCollisionsBetween(61, 52); // INVISIBLE_BARRIER, HUMAN_NOT_OTHER_HUMANS
    Havok_filter->enableCollisionsBetween(61, 35); // INVISIBLE_BARRIER, RAY_VEHICLE_ENTER_EXIT
    Havok_filter->disable_collisions_for_layer(56); // DELETE_OBJECT
    Havok_filter->enableCollisionsBetween(56, 0); // DELETE_OBJECT, ?
    Havok_filter->enableCollisionsBetween(56, 1); // DELETE_OBJECT, ?
    Havok_filter->enableCollisionsBetween(56, 3); // DELETE_OBJECT, MOVER
    Havok_filter->enableCollisionsBetween(56, 4); // DELETE_OBJECT, ITEM
    Havok_filter->enableCollisionsBetween(56, 18); // DELETE_OBJECT, PERSONAL_SHIELD
    Havok_filter->enableCollisionsBetween(56, 6); // DELETE_OBJECT, PLAYER
    Havok_filter->enableCollisionsBetween(56, 7); // DELETE_OBJECT, NPC
    Havok_filter->enableCollisionsBetween(56, 5); // DELETE_OBJECT, RAGDOLL
    Havok_filter->enableCollisionsBetween(56, 8); // DELETE_OBJECT, TURRET
    Havok_filter->enableCollisionsBetween(56, 9); // DELETE_OBJECT, VEHICLE
    Havok_filter->enableCollisionsBetween(56, 10); // DELETE_OBJECT, ?
    Havok_filter->disableCollisionsBetween(56, 11); // DELETE_OBJECT, ?
    Havok_filter->enableCollisionsBetween(56, 16); // DELETE_OBJECT, PROJECTILE
    Havok_filter->enableCollisionsBetween(56, 17); // DELETE_OBJECT, PROJECTILE_ATTACHED
    Havok_filter->enableCollisionsBetween(56, 22); // DELETE_OBJECT, NO_COLLISION
    Havok_filter->enableCollisionsBetween(56, 37); // DELETE_OBJECT, DEAD_HUMAN
    Havok_filter->enableCollisionsBetween(56, 38); // DELETE_OBJECT, PHYSICAL_OBJECT
    Havok_filter->enableCollisionsBetween(56, 40); // DELETE_OBJECT, RFG_NORMAL
    Havok_filter->enableCollisionsBetween(56, 41); // DELETE_OBJECT, RFG_NORMAL_FAST
    Havok_filter->enableCollisionsBetween(56, 42); // DELETE_OBJECT, RFG_LITE
    Havok_filter->enableCollisionsBetween(56, 43); // DELETE_OBJECT, RFG_ULTRA_LITE
    Havok_filter->enableCollisionsBetween(56, 44); // DELETE_OBJECT, RFG_WORLD_ONLY
    Havok_filter->enableCollisionsBetween(56, 51); // DELETE_OBJECT, ?
    Havok_filter->enableCollisionsBetween(56, 50); // DELETE_OBJECT, HUMAN_ON_LADDER
    Havok_filter->enableCollisionsBetween(56, 60); // DELETE_OBJECT, CLOTH_SIM
    Havok_filter->enableCollisionsBetween(56, 58); // DELETE_OBJECT, EFFECT_RIGID_BODY
    Havok_filter->disable_collisions_for_layer(63); // HUMAN_MINIMUM
    Havok_filter->enableCollisionsBetween(63, 2); // HUMAN_MINIMUM, WORLD
    Havok_filter->enableCollisionsBetween(63, 39); // HUMAN_MINIMUM, RFG_INITIAL_POSITION
    Havok_filter->enableCollisionsBetween(63, 61); // HUMAN_MINIMUM, INVISIBLE_BARRIER
    Havok_filter->enableCollisionsBetween(63, 62); // HUMAN_MINIMUM, FORCE_FIELD
    Havok_filter->copy_layer(53, 9); // ? from VEHICLE
    Havok_filter->disableCollisionsBetween(53, 7); // ?, NPC
    Havok_filter->copy_layer(12, 9); // WALKER from VEHICLE
    Havok_filter->disableCollisionsBetween(12, 11); // WALKER, ?
    Havok_filter->enableCollisionsBetween(12, 10); // WALKER, ?
    Havok_filter->copy_layer(13, 12); // ? from WALKER
    Havok_filter->disableCollisionsBetween(13, 12); // ?, WALKER
    Havok_filter->disableCollisionsBetween(13, 9); // ?, VEHICLE
    Havok_filter->disableCollisionsBetween(13, 3); // ?, MOVER
    Havok_filter->disableCollisionsBetween(13, 43); // ?, RFG_ULTRA_LITE
    Havok_filter->disableCollisionsBetween(13, 42); // ?, RFG_LITE
    Havok_filter->disable_collisions_for_layer(14); // WALKER_HULL
    Havok_filter->enableCollisionsBetween(14, 2); // WALKER_HULL, WORLD
    Havok_filter->enableCollisionsBetween(14, 39); // WALKER_HULL, RFG_INITIAL_POSITION
    Havok_filter->enableCollisionsBetween(14, 14); // WALKER_HULL, WALKER_HULL
    Havok_filter->enableCollisionsBetween(14, 62); // WALKER_HULL, FORCE_FIELD
    Havok_filter->enableCollisionsBetween(14, 61); // WALKER_HULL, INVISIBLE_BARRIER
    Havok_filter->enableCollisionsBetween(14, 8); // WALKER_HULL, TURRET
    Havok_filter->enableCollisionsBetween(14, 56); // WALKER_HULL, DELETE_OBJECT
    Havok_filter->enableCollisionsBetween(14, 9); // WALKER_HULL, VEHICLE
    Havok_filter->enableCollisionsBetween(14, 10); // WALKER_HULL, ?
    Havok_filter->disableCollisionsBetween(14, 11); // WALKER_HULL, ?
    Havok_filter->copy_layer(15, 9); // FLYER from VEHICLE
    Havok_filter->disableCollisionsBetween(15, 61); // FLYER, INVISIBLE_BARRIER
    havok_collision_callbacks_system_init();
    havok_collectors_init();
    havok_cm_manager_init();
    character_controller_system_init();

    // One setup job per Havok controller; each waits until all of them have started (the counter reaches num).
    const int num = scheduler_get_num_havok_controllers();
    event_trigger* trig = scheduler_get_event_trigger();
    volatile int counter = 0;
    for (int i = num; i > 0; i--)
    {
        cpu_job job;
        job.trigger = nullptr;
        job.flags = 0;
        job.scratch_buffer = nullptr;
        job.scratch_buffer_size = 0;
        job.thread_id = get_current_thread_id();
        job.arg1.pointer_val = (void*)&counter;
        job.job_func = havok_setup_thread_job;
        job.type = ccjt_havok;
        job.arg2.int_val = num;
        scheduler_add_job(nullptr, &job, trig);
    }
    scheduler_relinquish_cpu(trig, ccjt_any);
    Havok_mt_query_world = Havok_world;
}

// 0x008294B0 (:3002)
// Sets up the Havok memory of the controller the job runs on, then waits for the other setup jobs (arg1 points to
// the started-job counter, arg2 is the number of jobs) so each controller runs exactly one.
void havok_setup_thread_job(cpu_job* de_job, cpu_job_arg arg1, cpu_job_arg arg2)
{
    cpu_controller* controller = scheduler_get_this_context_controller();
    if (controller != nullptr)
    {
        const int hw_thread_num = controller->cpu_thread_num;
        Havok_job_memory_cs.enter();
        scheduler_get_controller(hw_thread_num);
        hkMemoryRouter* router = havok_create_thread_memory_router();
        Havok_job_memory[hw_thread_num] = router;
        hkBaseSystem::initThread(router);
        Havok_job_memory_cs.leave();
        controller->havok_memory_initialized = true;
    }
    volatile long* counter = (volatile long*)arg1.pointer_val;
    _InterlockedIncrement(counter);
    while ((unsigned int)*counter != arg2.int_val)
    {
        os_sleep(1);
    }
}

// 0x0082CC20 (:3026)
static void havok_kill_thread_job(cpu_job* de_job, cpu_job_arg arg1, cpu_job_arg arg2)
{
    cpu_controller* controller = scheduler_get_this_context_controller();
    if (controller != nullptr && controller->havok_memory_initialized)
    {
        const int hw_thread_num = controller->cpu_thread_num;
        hkBaseSystem::quitThread();
        havok_destroy_thread_memory_router(Havok_job_memory[hw_thread_num]);
        Havok_job_memory[hw_thread_num] = nullptr;
        controller->havok_memory_initialized = false;
    }
    volatile long* counter = (volatile long*)arg1.pointer_val;
    _InterlockedIncrement(counter);
    while ((unsigned int)*counter != arg2.int_val)
    {
        os_sleep(1);
    }
}

// 0x0082CC90 (:3050)
// Removes the listeners, the collectors, the default collision model and the broadphase objects, releases the
// per-thread Havok memory (one kill job per Havok controller but one, unlike the setup) and the world.
void havok_system_shutdown()
{
    if (!Havok_system_init)
    {
        return;
    }
    Havok_system_init = false;
    Thread_id = get_current_thread_id();
    Havok_world->removeConstraintListener(&Constraint_listener);
    Havok_world->removeWorldPostCollideListener(&Post_collide_listener);
    Havok_world->removeWorldPostSimulationListener(&Post_simulation_listener);
    Havok_world->removeContactListener(&Collision_listener);
    havok_collectors_free();
    havok_cm_manager_shutdown();
    havok_bpo_system_shutdown();
    Havok_world->removeConstraintListener(Constraint_filter);
    Havok_world->setBroadPhaseBorder(nullptr);
    character_controller_system_shutdown();
    event_trigger* trig = scheduler_get_event_trigger();
    const int num = scheduler_get_num_havok_controllers() - 1;
    volatile int counter = 0;
    for (int i = num; i > 0; i--)
    {
        cpu_job job;
        job.trigger = nullptr;
        job.flags = 0;
        job.scratch_buffer = nullptr;
        job.scratch_buffer_size = 0;
        job.thread_id = get_current_thread_id();
        job.arg1.pointer_val = (void*)&counter;
        job.job_func = havok_kill_thread_job;
        job.type = ccjt_havok;
        job.arg2.int_val = num;
        scheduler_add_job(nullptr, &job, trig);
    }
    scheduler_relinquish_cpu(trig, ccjt_any);
    havok_remove_entity_flush();
    havok_remove_phantom_flush();
    if (Havok_world != nullptr)
    {
        Havok_world->removeReference();
        Havok_world = nullptr;
    }
    havok_memory_shutdown();
}

// 0x0083CC80 (:3159)
// In multiplayer, the level geometry layer stops colliding with characters and projectiles.
void havok_level_init_pre()
{
    vehicle_level_init_pre();
    havok_ragdoll_level_init_pre();
    if (Multiplayer)
    {
        havok_multiplayer_level_init_pre();
    }
    havok_bpo_level_init_pre();
    hkMemorySystem::getInstance().garbageCollect();
}

// 0x0080E2D0 (:3184)
void havok_level_init_post()
{
}

// 0x0083CCE0 (:3190)
void havok_level_shutdown()
{
    havok_ragdoll_level_shutdown();
    if (Multiplayer)
    {
        havok_multiplayer_level_shutdown();
    }
    havok_bpo_level_shutdown();
    hkMemorySystem::getInstance().garbageCollect();
}

// 0x0080E2E0 (:3210)
void havok_mark_for_write()
{
}

// 0x0080E2F0 (:3218)
void havok_unmark_for_write()
{
}

// 0x0080E300 (:3225)
void havok_mark_for_read()
{
}

// 0x0080E310 (:3232)
void havok_unmark_for_read()
{
}

// 0x00822E00 (:3239)
void havok_get_broadphase_extents(vector& bmin, vector& bmax)
{
    hkpWorldCinfo info;
    Havok_world->getCinfo(info);
    const float* mn = (const float*)&info.m_broadPhaseWorldAabb.m_min;
    const float* mx = (const float*)&info.m_broadPhaseWorldAabb.m_max;
    bmin.x = mn[0];
    bmin.y = mn[1];
    bmin.z = mn[2];
    bmax.x = mx[0];
    bmax.y = mx[1];
    bmax.z = mx[2];
}

// 0x00834BC0 (:3248)
// The navmesh collision model of a path finding layer: a MOPP tree over the layer's cells (havok_navmesh_shape), added
// to the collision model table. The MOPP tree keeps the shape and the code referenced. Original bug kept: a MOPP blob
// that fails to load (null code) is dereferenced by the tree and by removeReference.
unsigned int havok_create_navmesh_shape(const unsigned char* mopp_buf, bb_pfg_layer* pfg_data)
{
    havok_navmesh_shape* navmesh = new havok_navmesh_shape(pfg_data);
    const hkpMoppCode* code = havok_mopp_code_load_in_place(reinterpret_cast<const havok_mopp_code_header*>(mopp_buf));
    hkpMoppBvTreeShape* tree = new hkpMoppBvTreeShape(navmesh, code);
    navmesh->removeReference();
    const_cast<hkpMoppCode*>(code)->removeReference();
    return havok_cm_manager_add(tree);
}

// 0x0083F0D0 (:3274)
// A fixed body of the zone's navmesh collision model, sized by the zone box.
unsigned int havok_create_navmesh_rb(unsigned int cm_index, obj_zone* zone, const vector& pos, const matrix& orient, bool vehicle_navmesh)
{
    vector bmin;
    vector bmax;
    zone->get_bbox(bmin, bmax);
    physics_create_info pci;
    pci.size.z = bmax.z - bmin.z;
    pci.size.y = bmax.y - bmin.y;
    pci.size.x = bmax.x - bmin.x;
    pci.pos = pos;
    pci.orient = orient;
    pci.cm_index = cm_index;
    pci.shape_type = HAVOK_SHAPE_TYPE_NAVMESH;
    pci.mass = 1.0f;
    pci.friction = 1.0f;
    pci.restitution = 1.0f;
    pci.precalc_center_of_mass.zero();
    pci.precalc_tensor.make_identity();
    pci.flags |= 0x1D;
    pci.obj_ptr = zone;
    pci.simulation_quality_type = HK_COLLIDABLE_QUALITY_FIXED;
    pci.motion_type = 5; // hkpMotion::MOTION_FIXED
    pci.collision_type = vehicle_navmesh ? COLLIDE_VEHICLE_NAVMESH : COLLIDE_NAVMESH;
    return havok_body_create(pci);
}

// 0x00822ED0 (:3302)
void havok_initialize_job_memory(int thread_num)
{
    Havok_job_memory_cs.enter();
    scheduler_get_controller(thread_num);
    hkMemoryRouter* router = havok_create_thread_memory_router();
    Havok_job_memory[thread_num] = router;
    hkBaseSystem::initThread(router);
    Havok_job_memory_cs.leave();
}

// 0x0080E320 (:3322)
bool havok_ok_to_use_thread(int thread_num)
{
    return scheduler_get_controller(thread_num)->havok_memory_initialized;
}

// 0x0080E340 (:3333)
unsigned int havok_num_scheduler_threads()
{
    return 4;
}

// 0x00831BA0 (:3442)
// Keeps simulation islands small enough for the solver: islands of 64 or more entities whose solver buffer exceeds
// the largest runtime block are split (or thinned by the memory watch dog), and islands without constraints are
// merged into constrained islands while the result stays below 64 entities (round robin over the targets).
static void havok_sim_islands_process_post()
{
    hkpWorld* world = Havok_world;
    hkArray<hkpSimulationIsland*, hkContainerHeapAllocator> small_islands; // constrained, below 64 entities
    hkArray<hkpSimulationIsland*, hkContainerHeapAllocator> free_islands; // without constraints
    for (int i = 0; i < world->m_activeSimulationIslands.getSize(); i++)
    {
        hkpSimulationIsland* island = world->m_activeSimulationIslands[i];
        if (island->m_numConstraints == 0)
        {
            free_islands.pushBack(island);
        }
        else if (island->m_entities.getSize() < 64) // entities per island
        {
            small_islands.pushBack(island);
        }
        else if ((unsigned int)hkpConstraintSolverSetup::calcBufferSize(*island) > Havok_mem_largest_runtime_block)
        {
            hkpWorldOperationUtil::splitSimulationIsland(Havok_world, island);
            if ((unsigned int)hkpConstraintSolverSetup::calcBufferSize(*island) > Havok_mem_largest_runtime_block)
            {
                havok_memory_watch_dog_free_memory(Havok_world, island, island->m_entities.getSize() - 64);
            }
        }
    }
    if (small_islands.getSize() > 0)
    {
        int next_target = 0;
        for (int f = 0; f < free_islands.getSize(); f++)
        {
            hkpSimulationIsland* island = free_islands[f];
            for (int t = 0; t < small_islands.getSize(); t++)
            {
                const int target_index = (next_target + t) % small_islands.getSize();
                hkpSimulationIsland* target = small_islands[target_index];
                if (target->m_entities.getSize() + island->m_entities.getSize() < 64)
                {
                    hkpWorldOperationUtil::internalMergeTwoIslands(Havok_world, target, island);
                    if (target->m_entities.getSize() == 0)
                    {
                        small_islands[target_index] = free_islands[f];
                    }
                    next_target++;
                    break;
                }
            }
        }
    }
}

// 0x0083F280 (:3536)
// Steps the world at 1/60 s as many times as the accumulated frame time allows (or once by the frame time with
// Havok_use_single_step), keeps the remainder, computes the render interpolation factor, then post-processes the
// islands, ragdolls, collision events, replaced collision models and object transforms.
void havok_simulate()
{
    Havok_world_is_simulating = true;
    havok_ragdoll_process_pre();
    havok_add_entity_flush();
    havok_add_phantom_flush();
    havok_update_status();
    float leftover = Frametime + Havok_step_time_leftover;
    Havok_step_time_leftover_last = Havok_step_time_leftover;
    if (!Havok_use_single_step)
    {
        Havok_step_time = 0.016666668f; // 1/60 s
        Havok_num_steps_this_frame = (int)(leftover / 0.016666668f);
    }
    else
    {
        Havok_num_steps_this_frame = 1;
        Havok_step_time = Frametime;
    }
    Havok_step_time_leftover = leftover;
    for (int step = 0; step < Havok_num_steps_this_frame; step++)
    {
        Havok_cur_step_this_frame = step;
        Havok_world->stepDeltaTime(Havok_step_time);
        leftover = Havok_step_time_leftover - Havok_step_time;
        Havok_step_time_leftover = leftover;
    }
    Havok_interpolate_percent = (((float)Havok_num_steps_this_frame * Havok_step_time + leftover) - Havok_step_time_leftover_last) /
                                ((float)(Havok_num_steps_this_frame + 1) * Havok_step_time - Havok_step_time_leftover_last);
    Havok_step_time_leftover = leftover;
    havok_sim_islands_process_post();
    havok_ragdoll_process_post();
    havok_collision_events_end_sim();
    Havok_world_is_simulating = false;
    havok_replace_collision_models_end_sim();
    havok_update_pos_and_orient();
    havok_memory_watch_dog_do_frame(Havok_world);
}

// 0x0080E350 (:3712)
bool havok_get_is_terminal_havok_step()
{
    return Havok_cur_step_this_frame == Havok_num_steps_this_frame - 1;
}

// 0x00822F10 (:3718)
// LTCG: y in a register, norm in EDI. The highest point of the body's shape below y = 1000 at (x, z): a vertical ray
// from +1000 to -1000 cast in body space.
static bool get_highest_point(const hkpRigidBody* rb, float& y, float x, float z, vector* norm)
{
    if (rb == nullptr)
    {
        return false;
    }
    const hkpShape* shape = rb->m_collidable.m_shape;
    if (shape == nullptr)
    {
        return false;
    }
    const hkTransformf& transform = rb->m_motion.m_motionState.m_transform;
    hkpShapeRayCastInput input;
    input.m_from.set(x, 1000.0f, z, 0.0f);
    input.m_from.sub(transform.m_translation);
    input.m_from.setRotatedDir(transform.m_rotation, input.m_from);
    input.m_to.set(x, -1000.0f, z, 0.0f);
    input.m_to.sub(transform.m_translation);
    input.m_to.setRotatedDir(transform.m_rotation, input.m_to);
    input.m_filterInfo = 0;
    input.m_rayShapeCollectionFilter = nullptr;
    hkpShapeRayCastOutput output;
    if (!shape->castRay(input, output).m_bool)
    {
        return false;
    }
    // from + (to - from) * fraction
    const float* from = (const float*)&input.m_from;
    const float* to = (const float*)&input.m_to;
    const float t = output.m_hitFraction;
    hkVector4f hit;
    hit.set((to[0] - from[0]) * t + from[0], (to[1] - from[1]) * t + from[1], (to[2] - from[2]) * t + from[2], (to[3] - from[3]) * t + from[3]);
    hit.setRotatedInverseDir(transform.m_rotation, hit);
    hit.add(transform.m_translation);
    y = ((const float*)&hit)[1];
    if (norm != nullptr)
    {
        hkVector4f normal;
        normal.setRotatedInverseDir(transform.m_rotation, output.m_normal);
        const float* n = (const float*)&normal;
        norm->x = n[0];
        norm->y = n[1];
        norm->z = n[2];
    }
    return true;
}

// 0x0083B3F0 (:3758)
// The terrain height at (x, z) in the zone there: a terrain-only ray when use_stitch_pieces is set, otherwise the
// height field sample of the zone body (for a bv tree, the height field of its collection), or the highest point of
// any other shape. norm defaults to up.
bool havok_terrain_height(float& y, float x, float z, bool use_stitch_pieces, vector* norm)
{
    y = 0.0f;
    if (norm != nullptr)
    {
        *norm = UP_VECTOR;
    }
    const vector zone_pos(x, 0.0f, z);
    obj_zone* zone = World.get_zone_by_pos(zone_pos);
    if (zone == nullptr || zone->havok_handle == 0xFFFFFFFF)
    {
        return false;
    }
    hkpRigidBody* rb = Hk_rigid_bodies[zone->havok_handle];
    if (rb == nullptr)
    {
        return false;
    }
    if (use_stitch_pieces)
    {
        raycast_input in;
        in.radius = 0.0f;
        in.start.set(x, 1000.0f, z);
        in.end.set(x, -1000.0f, z);
        in.filter_params = nullptr;
        in.m_hit_collector_type = raycast_input::COLLECT_CLOSEST_HIT;
        in.m_flags = 0;
        in.m_filter_info = COLLIDE_RAY_TERRAIN_ONLY;
        in.filter_callback = nullptr;
        raycast_output out;
        bool hit = false;
        if (havok_raycast(in, out))
        {
            y = out.m_hits[0].hit_pos.y;
            if (norm != nullptr)
            {
                *norm = out.m_hits[0].hit_normal;
            }
            hit = true;
        }
        return hit;
    }
    const hkpShape* shape = rb->m_collidable.m_shape;
    if (shape == nullptr)
    {
        return false;
    }
    const hkpSampledHeightFieldShape* field = nullptr;
    const unsigned char type = shape->m_type.m_storage;
    if (type == hkcdShapeType::SAMPLED_HEIGHT_FIELD)
    {
        field = (const hkpSampledHeightFieldShape*)shape;
    }
    else if (type == hkcdShapeType::TRI_SAMPLED_HEIGHT_FIELD_BV_TREE || type == hkcdShapeType::BV_TREE)
    {
        const hkpTriSampledHeightFieldCollection* collection = (const hkpTriSampledHeightFieldCollection*)((const hkpTriSampledHeightFieldBvTreeShape*)shape)->m_childContainer.m_childShape;
        if (collection == nullptr ||
            (collection->m_type.m_storage != hkcdShapeType::COLLECTION && collection->m_type.m_storage != hkcdShapeType::TRI_SAMPLED_HEIGHT_FIELD_COLLECTION))
        {
            return false;
        }
        field = collection->getHeightFieldShape();
        if (field == nullptr || field->m_type.m_storage != hkcdShapeType::SAMPLED_HEIGHT_FIELD)
        {
            return false;
        }
    }
    else
    {
        return get_highest_point(rb, y, x, z, norm);
    }
    // Sample the 512 x 512 height field in body space, clamped inside its last cell.
    const float* translation = (const float*)&rb->m_motion.m_motionState.m_transform.m_translation;
    float local_x = x - translation[0];
    float local_z = z - translation[2];
    if (local_x - 0.01f < 0.0f)
    {
        local_x = 0.01f;
    }
    if (local_x - 510.99f >= 0.0f)
    {
        local_x = 510.99f;
    }
    if (local_z - 0.01f < 0.0f)
    {
        local_z = 0.01f;
    }
    if (local_z - 510.99f >= 0.0f)
    {
        local_z = 510.99f;
    }
    const int x_pos = (int)floorf(local_x);
    const int z_pos = (int)floorf(local_z);
    hkVector4f normal;
    int triangle_index;
    field->getHeightAndNormalAt(x_pos, z_pos, local_x - (float)x_pos, local_z - (float)z_pos, normal, y, triangle_index);
    if (norm != nullptr)
    {
        const float* n = (const float*)&normal;
        norm->x = n[0];
        norm->y = n[1];
        norm->z = n[2];
    }
    return true;
}

// ---- bodies
// 0x0082CDF0 (:3857); LTCG: test_extent ECX, test_trans EDX, the others on the stack. Raises y to the top of the children
// of a bv tree shape inside the oriented box; true when the box touches any. Shapes that are no bv tree answer false.
// The dispatcher's alternate type table is what tells them apart. Only MOPP, height field and destroyable trees are
// queried; the other tree types find nothing.
static bool havok_terrain_height_internal(float& y, const hkTransformf& test_trans, const hkVector4f& test_extent, const hkpShape* shape, const hkTransformf& shape_trans)
{
    if (!Havok_world->m_collisionDispatcher->hasAlternateType(shape->getType(), hkcdShapeType::BV_TREE))
    {
        return false;
    }
    hkTransformf obb_to_shape;
    obb_to_shape.m_translation.setTransformedInversePos(shape_trans, test_trans.m_translation);
    obb_to_shape.m_rotation.setMulInverse(test_trans.m_rotation, shape_trans.m_rotation);
    const hkpShapeContainer* container = shape->getContainer();
    const hkpBvTreeShape* tree = (const hkpBvTreeShape*)shape;
    hkArray<unsigned int, hkContainerHeapAllocator> hits;
    // rfg.exe calls the slots directly: vtbl+0x5C for a user tree, vtbl+0x54 for the MOPP and height field trees.
    switch ((int)tree->m_bvTreeType)
    {
    case hkpBvTreeShape::BVTREE_USER:
        static_cast<const rfg_destroyable_shape*>(tree)->queryObb(obb_to_shape, test_extent, 0.0f, hits);
        break;
    case hkpBvTreeShape::BVTREE_MOPP:
        static_cast<const hkMoppBvTreeShapeBase*>(tree)->queryObb(obb_to_shape, test_extent, 0.0f, hits);
        break;
    case hkpBvTreeShape::BVTREE_TRISAMPLED_HEIGHTFIELD:
        static_cast<const hkpTriSampledHeightFieldBvTreeShape*>(tree)->queryObb(obb_to_shape, test_extent, 0.0f, hits);
        break;
    default:
        break;
    }
    const bool found = hits.getSize() > 0;
    hkpShapeBuffer buffer;
    for (int i = 0; i < hits.getSize(); i++)
    {
        const hkpShape* child = container->getChildShape(hits[i], buffer);
        hkAabb aabb;
        child->getAabb(shape_trans, 0.0f, aabb);
        vector child_min;
        vector child_max;
        havok_to_game_aabb(child_min, child_max, aabb);
        if ((float)((double)y - (double)child_max.y) < 0.0f)
        {
            y = child_max.y;
        }
    }
    return found;
}

// 0x0082CF90 (:3921). The highest ground under an oriented box: y is the top of the terrain (the zone bodies at the
// corners of the box's world bounds, at most 4) and of the bodies with the stitch piece property in those bounds, either
// their whole world box when it touches the oriented box (use_simple_stitch_piece_test) or their children in it.
// Starts at -FLT_MAX (the constant at 0x012BEF6C); false when nothing was found. Without scratch memory it answers false
// even if the terrain was found, as the original. Magic numbers: 0x8000 bytes of scratch are 4096 broadphase pairs; 14 is
// the stitch piece property (name inferred).
bool havok_terrain_height(float& y, const vector& local_bmin, const vector& local_bmax, const vector& pos, const matrix& orient, bool use_simple_stitch_piece_test)
{
    y = -3.4028235e+38f;
    vector world_min;
    vector world_max;
    bbox_instance(world_min, world_max, orient, pos, local_bmin, local_bmax);
    hkAabb aabb;
    aabb.m_min.set(world_min.x, world_min.y, world_min.z, 0.0f);
    aabb.m_max.set(world_max.x, world_max.y, world_max.z, 0.0f);
    vector extents;
    vector center;
    bbox_extract_obb_info(local_bmin, local_bmax, pos, orient, extents, center);
    hkVector4f test_extent;
    test_extent.set(extents.x, extents.y, extents.z, 0.0f);
    hkTransformf test_trans;
    test_trans.m_rotation.m_col0.set(orient.rvec.x, orient.rvec.y, orient.rvec.z, 0.0f);
    test_trans.m_rotation.m_col1.set(orient.uvec.x, orient.uvec.y, orient.uvec.z, 0.0f);
    test_trans.m_rotation.m_col2.set(orient.fvec.x, orient.fvec.y, orient.fvec.z, 0.0f);
    test_trans.m_translation.set(center.x, center.y, center.z, 0.0f);
    bool found = false;
    const vector corners[4] = {
        vector(world_min.x, 0.0f, world_min.z),
        vector(world_min.x, 0.0f, world_max.z),
        vector(world_max.x, 0.0f, world_min.z),
        vector(world_max.x, 0.0f, world_max.z),
    };
    hkpRigidBody* bodies[4] = {};
    int num_bodies = 0;
    for (const vector& corner : corners)
    {
        const obj_zone* zone = World.get_zone_by_pos(corner);
        if (zone == nullptr || zone->havok_handle == 0xFFFFFFFF)
        {
            continue;
        }
        hkpRigidBody* body = Hk_rigid_bodies[zone->havok_handle];
        if (body == nullptr)
        {
            continue;
        }
        for (hkpRigidBody*& slot : bodies)
        {
            if (slot == body)
            {
                break;
            }
            if (slot == nullptr)
            {
                slot = body;
                num_bodies++;
                break;
            }
        }
    }
    for (int i = 0; i < num_bodies; i++)
    {
        const hkpCollidable* collidable = bodies[i]->getCollidable();
        found |= havok_terrain_height_internal(y, test_trans, test_extent, collidable->getShape(), collidable->getTransform());
    }
    mempool_mark_scope scope(Vlib_scratch_pool);
    hkpBroadPhaseHandlePair* buffer = (hkpBroadPhaseHandlePair*)Vlib_scratch_pool->alloc(0x8000, 0);
    if (buffer == nullptr)
    {
        return false;
    }
    hkArray<hkpBroadPhaseHandlePair, hkContainerHeapAllocator> pairs;
    pairs.m_data = buffer;
    pairs.m_size = 0;
    pairs.m_capacityAndFlags = (0x8000 / (int)sizeof(hkpBroadPhaseHandlePair)) | hkArrayBase<hkpBroadPhaseHandlePair>::DONT_DEALLOCATE_FLAG;
    Havok_world->m_broadPhase->querySingleAabb(aabb, pairs);
    for (int i = 0; i < pairs.getSize(); i++)
    {
        const hkpTypedBroadPhaseHandle* handle = (const hkpTypedBroadPhaseHandle*)pairs[i].m_b;
        const hkpCdBody* root = (const hkpCollidable*)handle->getOwner();
        while (root->m_parent != nullptr)
        {
            root = root->m_parent;
        }
        if (havok_get_property(havok_world_object((const hkpCollidable*)root), 14, nullptr, true) == nullptr)
        {
            continue;
        }
        const hkpShape* shape = root->getShape();
        const hkTransformf& shape_trans = root->getTransform();
        if (!use_simple_stitch_piece_test)
        {
            found |= havok_terrain_height_internal(y, test_trans, test_extent, shape, shape_trans);
            continue;
        }
        hkAabb shape_aabb;
        shape->getAabb(hkTransformf::getIdentity(), 0.0f, shape_aabb);
        vector shape_min;
        vector shape_max;
        havok_to_game_aabb(shape_min, shape_max, shape_aabb);
        vector body_pos;
        matrix body_orient;
        havok_to_game_transform(body_pos, body_orient, shape_trans);
        vector body_extents;
        vector body_center;
        bbox_extract_obb_info(shape_min, shape_max, body_pos, body_orient, body_extents, body_center);
        if (ix_oriented_box_box(extents, center, orient, body_extents, body_center, body_orient))
        {
            bbox_instance(world_min, world_max, body_orient, body_pos, shape_min, shape_max);
            if ((float)((double)y - (double)world_max.y) < 0.0f)
            {
                y = world_max.y;
            }
            found = true;
        }
    }
    return found;
}

// 0x00823110 (:4168)
void havok_body_free(unsigned int handle, bool force_activate, bool remove_from_world, bool batch_remove)
{
    if (handle == 0xFFFFFFFF || Hk_rigid_bodies[handle] == nullptr)
    {
        return;
    }
    hkpRigidBody* rb = Hk_rigid_bodies[handle];
    if (force_activate && !rb->isActive().m_bool)
    {
        rb->activate();
    }
    if (remove_from_world)
    {
        havok_remove_entity(rb, batch_remove);
    }
    rb->removeReference();
    havok_free_rigid_body_handle(handle);
}

// 0x00823190 (:4199)
// Removes the phantom from the world when only the caller still holds it.
void havok_phantom_release(hkpSimpleShapePhantom* handle)
{
    if (handle != nullptr && handle->getReferenceCount() == 1)
    {
        havok_remove_phantom(handle, true);
    }
}

// 0x008231C0 (:4210)
// Drops the creation reference and removes the phantom when only the world and the caller still hold it.
void havok_aabb_phantom_release(game_aabb_phantom* handle)
{
    if (handle != nullptr && handle->getReferenceCount() == 2)
    {
        handle->removeReference();
        havok_remove_phantom(handle, true);
    }
}

// 0x0083DBA0 (:4222)
// Builds the body's shape from the create info (owned shapes are released once the body holds them; collision models,
// destroyable and world shapes stay with their owners), wrapped in a bounding volume shape with a
// havok_region_callback_shape when there are enter/leave callbacks, then the body (handle from
// havok_generate_rigid_body_handle). Flags: 1 use the precalculated inertia (a zero column falls back to identity),
// 2 an offset box (local_pos / local_orient), 4 batched add, 8 added inactive.
unsigned int havok_body_create(physics_create_info& pci)
{
    hkpRigidBodyCinfo cinfo;
    switch (pci.shape_type)
    {
        case HAVOK_SHAPE_TYPE_SPHERE:
            cinfo.m_shape = new hkpSphereShape(pci.radius);
            break;
        case HAVOK_SHAPE_TYPE_CAPSULE:
        {
            hkVector4f bottom;
            hkVector4f top;
            bottom.set(pci.bottom.x, pci.bottom.y, pci.bottom.z, 0.0f);
            top.set(pci.top.x, pci.top.y, pci.top.z, 0.0f);
            cinfo.m_shape = new hkpCapsuleShape(bottom, top, pci.radius);
            break;
        }
        case HAVOK_SHAPE_TYPE_BOX:
        {
            const vector half_extents(pci.size.x * 0.5f, pci.size.y * 0.5f, pci.size.z * 0.5f);
            if ((pci.flags & 2) == 0)
            {
                hkVector4f extents;
                extents.set(half_extents.x, half_extents.y, half_extents.z, 0.0f);
                cinfo.m_shape = new hkpBoxShape(extents, hkConvexShapeDefaultRadius);
            }
            else
            {
                const quaternion rotation(pci.local_orient);
                cinfo.m_shape = havok_cm_manager_create_box_shape(&rotation, &pci.local_pos, &half_extents);
            }
            break;
        }
        case HAVOK_SHAPE_TYPE_COLLISION_MODEL:
        case HAVOK_SHAPE_TYPE_WORLD:
        case HAVOK_SHAPE_TYPE_NAVMESH:
            cinfo.m_shape = havok_cm_manager_get(pci.cm_index);
            break;
        case HAVOK_SHAPE_TYPE_DESTROYABLE:
        case HAVOK_SHAPE_TYPE_VEHICLE:
            cinfo.m_shape = pci.shape;
            break;
        case HAVOK_SHAPE_TYPE_CYLINDER:
        {
            hkVector4f bottom;
            hkVector4f top;
            bottom.set(pci.bottom.x, pci.bottom.y, pci.bottom.z, 0.0f);
            top.set(pci.top.x, pci.top.y, pci.top.z, 0.0f);
            cinfo.m_shape = new hkpCylinderShape(bottom, top, pci.radius, hkConvexShapeDefaultRadius);
            break;
        }
        default:
            return 0xFFFFFFFF;
    }
    if (pci.enter_callback != nullptr || pci.leave_callback != nullptr)
    {
        havok_region_callback_shape* region = new havok_region_callback_shape(pci.enter_callback, pci.leave_callback);
        hkpBvShape* bv = new hkpBvShape(cinfo.m_shape, region);
        cinfo.m_shape->removeReference();
        region->removeReference();
        cinfo.m_shape = bv;
    }
    cinfo.m_mass = pci.mass;
    cinfo.m_position.set(pci.pos.x, pci.pos.y, pci.pos.z, 0.0f);
    cinfo.m_qualityType.m_storage = (signed char)pci.simulation_quality_type;
    cinfo.m_solverDeactivation.m_storage = (signed char)pci.solver_deactivation;
    cinfo.m_motionType.m_storage = (signed char)pci.motion_type;
    cinfo.m_friction = pci.friction;
    cinfo.m_restitution = pci.restitution;
    cinfo.m_autoRemoveLevel = pci.auto_remove_level;
    cinfo.m_angularDamping = pci.angular_damping;
    cinfo.m_linearDamping = pci.linear_damping;
    cinfo.m_maxAngularVelocity = pci.max_angular_vel;
    cinfo.m_collisionFilterInfo = pci.collision_type;
    cinfo.m_maxLinearVelocity = pci.max_linear_vel;
    const quaternion rotation(pci.orient);
    cinfo.m_rotation.m_vec.set(rotation.x, rotation.y, rotation.z, -rotation.w); // Havok's w is negated
    if ((pci.flags & 1) == 0)
    {
        cinfo.m_inertiaTensor.setIdentity();
        cinfo.m_centerOfMass.set(0.0f, 0.0f, 0.0f, 0.0f);
    }
    else
    {
        game_to_havok_tensor(cinfo.m_inertiaTensor, pci.precalc_tensor);
        cinfo.m_centerOfMass.set(pci.precalc_center_of_mass.x, pci.precalc_center_of_mass.y, pci.precalc_center_of_mass.z, 0.0f);
        const float* c0 = (const float*)&cinfo.m_inertiaTensor.m_col0;
        const float* c1 = (const float*)&cinfo.m_inertiaTensor.m_col1;
        const float* c2 = (const float*)&cinfo.m_inertiaTensor.m_col2;
        if (c0[0] * c0[0] + c0[1] * c0[1] + c0[2] * c0[2] == 0.0f || c1[0] * c1[0] + c1[1] * c1[1] + c1[2] * c1[2] == 0.0f ||
            c2[0] * c2[0] + c2[1] * c2[1] + c2[2] * c2[2] == 0.0f)
        {
            cinfo.m_inertiaTensor.setIdentity();
        }
    }
    havok_rigid_body* rb = new havok_rigid_body(cinfo);
    const unsigned int handle = havok_generate_rigid_body_handle(rb);
    if (handle == 0xFFFFFFFF)
    {
        return 0xFFFFFFFF;
    }
    havok_body_setup_user_properties(rb, pci);
    havok_add_entity(rb, (pci.flags & 8) == 0, (pci.flags & 4) != 0);
    if (pci.shape_type != HAVOK_SHAPE_TYPE_COLLISION_MODEL && pci.shape_type != HAVOK_SHAPE_TYPE_DESTROYABLE &&
        pci.shape_type != HAVOK_SHAPE_TYPE_WORLD && pci.shape_type != HAVOK_SHAPE_TYPE_NAVMESH)
    {
        cinfo.m_shape->removeReference();
    }
    return handle;
}

// 0x008231F0 (:4390)
// A simple shape phantom (sphere, capsule or box) at the create info's position, added in the next phantom batch;
// the returned phantom is held by the world only.
hkpSimpleShapePhantom* havok_phantom_create(physics_create_info& pci)
{
    hkpRigidBodyCinfo cinfo; // unused (constructed by the original)
    hkpShape* shape;
    if (pci.shape_type == HAVOK_SHAPE_TYPE_SPHERE)
    {
        shape = new hkpSphereShape(pci.radius);
    }
    else if (pci.shape_type == HAVOK_SHAPE_TYPE_CAPSULE)
    {
        hkVector4f bottom;
        hkVector4f top;
        bottom.set(pci.bottom.x, pci.bottom.y, pci.bottom.z, 0.0f);
        top.set(pci.top.x, pci.top.y, pci.top.z, 0.0f);
        shape = new hkpCapsuleShape(bottom, top, pci.radius);
    }
    else if (pci.shape_type == HAVOK_SHAPE_TYPE_BOX)
    {
        hkVector4f half_extents;
        half_extents.set(pci.size.x * 0.5f, pci.size.y * 0.5f, pci.size.z * 0.5f, 0.0f);
        shape = new hkpBoxShape(half_extents, hkConvexShapeDefaultRadius);
    }
    else
    {
        return nullptr;
    }
    pci.shape = shape;
    hkTransformf transform;
    game_to_havok_transform(transform, pci.pos, pci.orient);
    hkpSimpleShapePhantom* phantom = new hkpSimpleShapePhantom(pci.shape, transform, pci.collision_type);
    havok_add_phantom(phantom, true);
    pci.shape->removeReference();
    phantom->removeReference();
    if (pci.obj_ptr != nullptr && !phantom->hasProperty(HAVOK_PROPERTY_OBJECT, hkpWorldObject::MULTI_THREADING_CHECKS_ENABLE))
    {
        phantom->addProperty(HAVOK_PROPERTY_OBJECT, hkSimplePropertyValue((void*)pci.obj_ptr));
    }
    return phantom;
}

// 0x008169D0 (:4447)
void havok_phantom_set_position_and_orient(hkpSimpleShapePhantom* handle, const vector& pos, const matrix& orient)
{
    if (handle == nullptr)
    {
        return;
    }
    hkTransformf transform;
    game_to_havok_transform(transform, pos, orient);
    handle->setTransform(transform);
}

// 0x00816A90 (:4458)
void havok_phantom_set_collision_type(hkpPhantom* phantom, unsigned int collision_type, bool full_shape_check)
{
    if (phantom == nullptr)
    {
        return;
    }
    phantom->m_collidable.m_broadPhaseHandle.m_collisionFilterInfo = collision_type;
    if (phantom->m_world != nullptr)
    {
        Havok_world->updateCollisionFilterOnPhantom(phantom, (hkpUpdateCollectionFilterMode)full_shape_check);
    }
}

// 0x00823500 (:4471)
// A game_aabb_phantom (deletes the objects that enter it) on the delete object layer, added in the next phantom
// batch.
game_aabb_phantom* havok_aabb_phantom_create(const vector& bmin, const vector& bmax)
{
    hkAabb aabb;
    aabb.m_min.set(bmin.x, bmin.y, bmin.z, 0.0f);
    aabb.m_max.set(bmax.x, bmax.y, bmax.z, 0.0f);
    game_aabb_phantom* phantom = new game_aabb_phantom(aabb);
    havok_add_phantom(phantom, true);
    return phantom;
}

// 0x00816AD0 (:4485)
void havok_aabb_phantom_update_aabb(game_aabb_phantom* handle, const vector& bmin, const vector& bmax)
{
    hkAabb aabb;
    aabb.m_min.set(bmin.x, bmin.y, bmin.z, 0.0f);
    aabb.m_max.set(bmax.x, bmax.y, bmax.z, 0.0f);
    handle->setAabb(aabb);
}

// ---- properties
// 0x00816B50 (:4498)
bool havok_has_property(hkpWorldObject* obj, unsigned int prop)
{
    return obj->hasProperty(prop, hkpWorldObject::MULTI_THREADING_CHECKS_ENABLE);
}

// 0x00816B80 (:4504)
// An existing property keeps its value.
void havok_set_property(hkpWorldObject* obj, unsigned int prop, void* const value)
{
    if (!obj->hasProperty(prop, hkpWorldObject::MULTI_THREADING_CHECKS_ENABLE))
    {
        obj->addProperty(prop, hkSimplePropertyValue(value));
    }
}

// 0x00816BC0 (:4512)
void havok_remove_property(hkpWorldObject* obj, unsigned int prop)
{
    if (obj->hasProperty(prop, hkpWorldObject::MULTI_THREADING_CHECKS_ENABLE))
    {
        obj->removeProperty(prop);
    }
}

// 0x00816C10 (:4520)
void* havok_get_property(const hkpWorldObject* obj, unsigned int prop, void* default_value, bool optional)
{
    if (obj != nullptr && obj->hasProperty(prop, hkpWorldObject::MULTI_THREADING_CHECKS_ENABLE))
    {
        return obj->getProperty(prop, hkpWorldObject::MULTI_THREADING_CHECKS_ENABLE).getPtr();
    }
    return default_value;
}

// 0x00816C60 (:4539)
// The body stops colliding with the object of ignore_handle (havok_game_filter); an existing ignore handle is kept.
bool havok_set_ignore_property(hkpRigidBody* rb, unsigned int ignore_handle)
{
    if (!rb->hasProperty(HAVOK_PROPERTY_IGNORE_HANDLE, hkpWorldObject::MULTI_THREADING_CHECKS_ENABLE))
    {
        rb->addProperty(HAVOK_PROPERTY_IGNORE_HANDLE, hkSimplePropertyValue(ignore_handle));
    }
    if (rb->m_world != nullptr)
    {
        Havok_world->updateCollisionFilterOnEntity(rb, HK_UPDATE_FILTER_ON_ENTITY_FULL_CHECK, HK_UPDATE_COLLECTION_FILTER_IGNORE_SHAPE_COLLECTIONS);
    }
    return true;
}

// 0x00816CB0 (:4550)
bool havok_set_ignore_property(hkpPhantom* phantom, unsigned int ignore_handle)
{
    if (!phantom->hasProperty(HAVOK_PROPERTY_IGNORE_HANDLE, hkpWorldObject::MULTI_THREADING_CHECKS_ENABLE))
    {
        phantom->addProperty(HAVOK_PROPERTY_IGNORE_HANDLE, hkSimplePropertyValue(ignore_handle));
    }
    if (phantom->m_world != nullptr)
    {
        Havok_world->updateCollisionFilterOnPhantom(phantom, HK_UPDATE_COLLECTION_FILTER_IGNORE_SHAPE_COLLECTIONS);
    }
    return true;
}

// 0x00816D00 (:4563)
bool havok_remove_ignore_property(hkpRigidBody* rb)
{
    if (rb->hasProperty(HAVOK_PROPERTY_IGNORE_HANDLE, hkpWorldObject::MULTI_THREADING_CHECKS_ENABLE))
    {
        rb->removeProperty(HAVOK_PROPERTY_IGNORE_HANDLE);
    }
    if (rb->m_world != nullptr)
    {
        Havok_world->updateCollisionFilterOnEntity(rb, HK_UPDATE_FILTER_ON_ENTITY_DISABLE_ENTITY_ENTITY_COLLISIONS_ONLY, HK_UPDATE_COLLECTION_FILTER_IGNORE_SHAPE_COLLECTIONS);
    }
    return true;
}

// 0x00816D60 (:4574)
bool havok_remove_ignore_property(hkpPhantom* phantom)
{
    if (phantom->hasProperty(HAVOK_PROPERTY_IGNORE_HANDLE, hkpWorldObject::MULTI_THREADING_CHECKS_ENABLE))
    {
        phantom->removeProperty(HAVOK_PROPERTY_IGNORE_HANDLE);
    }
    if (phantom->m_world != nullptr)
    {
        Havok_world->updateCollisionFilterOnPhantom(phantom, HK_UPDATE_COLLECTION_FILTER_IGNORE_SHAPE_COLLECTIONS);
    }
    return true;
}

// 0x00816DB0 (:4585)
bool havok_has_ignore_property(hkpRigidBody* rb)
{
    return rb != nullptr && rb->hasProperty(HAVOK_PROPERTY_IGNORE_HANDLE, hkpWorldObject::MULTI_THREADING_CHECKS_ENABLE);
}

// 0x00816DE0 (:4590)
bool havok_has_ignore_property(hkpPhantom* phantom)
{
    return phantom != nullptr && phantom->hasProperty(HAVOK_PROPERTY_IGNORE_HANDLE, hkpWorldObject::MULTI_THREADING_CHECKS_ENABLE);
}

// 0x0080E370 (:4602)
void havok_entity_update_collision_filter(hkpEntity* entity, bool full_entity_check, bool full_shape_check)
{
    if (entity->m_world != nullptr)
    {
        Havok_world->updateCollisionFilterOnEntity(entity, (hkpUpdateCollisionFilterOnEntityMode)!full_entity_check, (hkpUpdateCollectionFilterMode)full_shape_check);
    }
}

// 0x0080E3A0 (:4624)
void havok_phantom_update_collision_filter(hkpPhantom* phantom, bool full_shape_check)
{
    if (phantom->m_world != nullptr)
    {
        Havok_world->updateCollisionFilterOnPhantom(phantom, (hkpUpdateCollectionFilterMode)full_shape_check);
    }
}

// ---- body pose
// 0x00829530 (:4637)
void havok_body_get_pos_orient(unsigned int handle, vector& pos, matrix& orient)
{
    havok_body_get_pos_orient(handle != 0xFFFFFFFF ? Hk_rigid_bodies[handle] : nullptr, pos, orient);
}

// 0x00823610 (:4643)
// The orient rows are the Havok rotation columns.
void havok_body_get_pos_orient(hkpRigidBody* rb, vector& pos, matrix& orient)
{
    if (rb == nullptr)
    {
        return;
    }
    const hkTransformf& transform = rb->m_motion.m_motionState.m_transform;
    const float* r = (const float*)&transform.m_rotation;
    const float* t = (const float*)&transform.m_translation;
    pos.set(t[0], t[1], t[2]);
    orient.rvec.set(r[0], r[1], r[2]);
    orient.uvec.set(r[4], r[5], r[6]);
    orient.fvec.set(r[8], r[9], r[10]);
}

// 0x00823700 (:4651)
// Invalid positions or orients are ignored.
void havok_body_set_pos_orient(unsigned int handle, const vector& pos, const matrix& orient)
{
    if (handle == 0xFFFFFFFF || Hk_rigid_bodies[handle] == nullptr)
    {
        return;
    }
    havok_body_set_pos_orient(Hk_rigid_bodies[handle], pos, orient);
}

// 0x00816E10 (:4657)
void havok_body_set_pos_orient(hkpRigidBody* rb, const vector& pos, const matrix& orient)
{
    if (rb == nullptr || !pos.is_valid() || !orient.is_valid())
    {
        return;
    }
    hkTransformf transform;
    transform.m_translation.set(pos.x, pos.y, pos.z, 0.0f);
    game_to_havok_matrix(transform.m_rotation, orient);
    rb->setTransform(transform);
}

// 0x00816EA0 (:4685)
void havok_body_set_position(unsigned int handle, const vector& pos)
{
    hkVector4f position;
    position.set(pos.x, pos.y, pos.z, 0.0f);
    if (handle != 0xFFFFFFFF && Hk_rigid_bodies[handle] != nullptr)
    {
        Hk_rigid_bodies[handle]->setPosition(position);
    }
}

// 0x00816F00 (:4698)
void havok_body_set_orient(unsigned int handle, const matrix& orient)
{
    const quaternion q(orient);
    hkQuaternionf rotation;
    rotation.m_vec.set(q.x, q.y, q.z, -q.w); // Havok's w is negated
    if (handle != 0xFFFFFFFF && Hk_rigid_bodies[handle] != nullptr)
    {
        Hk_rigid_bodies[handle]->setRotation(rotation);
    }
}

// 0x00829560 (:4716)
// The box of the body's shape at the given position and orient.
bool havok_body_get_aabb(unsigned int handle, const vector& pos, const matrix& orient, vector& bmin, vector& bmax)
{
    if (handle == 0xFFFFFFFF || Hk_rigid_bodies[handle] == nullptr)
    {
        return false;
    }
    const hkpShape* shape = Hk_rigid_bodies[handle]->m_collidable.m_shape;
    if (shape == nullptr)
    {
        return false;
    }
    hkTransformf transform;
    game_to_havok_transform(transform, pos, orient);
    hkAabb aabb;
    shape->getAabb(transform, 0.0f, aabb);
    const float* mn = (const float*)&aabb.m_min;
    const float* mx = (const float*)&aabb.m_max;
    bmin.set(mn[0], mn[1], mn[2]);
    bmax.set(mx[0], mx[1], mx[2]);
    return true;
}

// ---- velocities and mass
// 0x008237A0 (:4738)
void havok_body_get_linear_velocity(unsigned int handle, vector& vel)
{
    if (handle != 0xFFFFFFFF && Hk_rigid_bodies[handle] != nullptr)
    {
        havok_body_get_linear_velocity(Hk_rigid_bodies[handle], vel);
    }
}

// 0x00816FA0 (:4743)
void havok_body_get_linear_velocity(hkpRigidBody* rb, vector& vel)
{
    if (rb != nullptr)
    {
        const float* v = (const float*)&rb->m_motion.m_linearVelocity;
        vel.set(v[0], v[1], v[2]);
    }
}

// 0x008237E0 (:4749)
void havok_body_set_linear_velocity(unsigned int handle, const vector& velocity)
{
    havok_body_set_linear_velocity(handle != 0xFFFFFFFF ? Hk_rigid_bodies[handle] : nullptr, velocity);
}

// 0x00816FD0 (:4754)
void havok_body_set_linear_velocity(hkpRigidBody* rb, const vector& velocity)
{
    hkVector4f v;
    v.set(velocity.x, velocity.y, velocity.z, 0.0f);
    if (rb != nullptr)
    {
        rb->activate();
        rb->m_motion.setLinearVelocity(v);
    }
}

// 0x00823860 (:4765)
void havok_body_get_mass_props(unsigned int handle, float& mass, vector& center_of_mass, matrix& inertia_tensor)
{
    havok_body_get_mass_props(handle != 0xFFFFFFFF ? Hk_rigid_bodies[handle] : nullptr, mass, center_of_mass, inertia_tensor);
}

// 0x00817040 (:4769)
// Keyframed and fixed bodies report the mass properties of their stored dynamic motion.
void havok_body_get_mass_props(hkpRigidBody* rb, float& mass, vector& center_of_mass, matrix& inertia_tensor)
{
    if (rb == nullptr)
    {
        return;
    }
    hkpMotion* motion = &rb->m_motion;
    const unsigned char type = rb->m_motion.m_type.m_storage;
    if (type == hkpMotion::MOTION_KEYFRAMED || type == hkpMotion::MOTION_FIXED)
    {
        hkpMotion* stored = rb->getStoredDynamicMotion();
        if (stored != nullptr)
        {
            motion = stored;
        }
    }
    mass = motion->getMass();
    const float* com = (const float*)&motion->m_motionState.m_sweptTransform.m_centerOfMassLocal;
    center_of_mass.set(com[0], com[1], com[2]);
    hkMatrix3f inertia;
    motion->getInertiaLocal(inertia);
    const float* c0 = (const float*)&inertia.m_col0;
    const float* c1 = (const float*)&inertia.m_col1;
    const float* c2 = (const float*)&inertia.m_col2;
    inertia_tensor.rvec.set(c0[0], c0[1], c0[2]);
    inertia_tensor.uvec.set(c1[0], c1[1], c1[2]);
    inertia_tensor.fvec.set(c2[0], c2[1], c2[2]);
}

// 0x00817170 (:4795)
// Not for fixed bodies.
void havok_body_set_mass_props(hkpRigidBody* rb, float mass, const vector& center_of_mass, const matrix& inertia_tensor)
{
    if (rb == nullptr || rb->m_motion.m_type.m_storage == hkpMotion::MOTION_FIXED)
    {
        return;
    }
    rb->setMass(mass);
    hkVector4f com;
    com.set(center_of_mass.x, center_of_mass.y, center_of_mass.z, 0.0f);
    rb->setCenterOfMassLocal(com);
    hkMatrix3f inertia;
    inertia.m_col0.set(inertia_tensor.rvec.x, inertia_tensor.rvec.y, inertia_tensor.rvec.z, 0.0f);
    inertia.m_col1.set(inertia_tensor.uvec.x, inertia_tensor.uvec.y, inertia_tensor.uvec.z, 0.0f);
    inertia.m_col2.set(inertia_tensor.fvec.x, inertia_tensor.fvec.y, inertia_tensor.fvec.z, 0.0f);
    rb->setInertiaLocal(inertia);
}

// 0x00817260 (:4812)
void havok_body_set_mass_props(unsigned int handle, float mass, const vector& center_of_mass, const matrix& inertia_tensor)
{
    havok_body_set_mass_props(handle != 0xFFFFFFFF ? Hk_rigid_bodies[handle] : nullptr, mass, center_of_mass, inertia_tensor);
}

// 0x0080E3D0 (:4818)
// Solid box inertia: m / 12 * (b^2 + c^2) on each axis.
void havok_body_calc_mass_props(const vector& local_bmin, const vector& local_bmax, float mass, vector& center_of_mass, matrix& inertia_tensor)
{
    center_of_mass.zero();
    const float dx = local_bmax.x - local_bmin.x;
    const float dy = local_bmax.y - local_bmin.y;
    const float dz = local_bmax.z - local_bmin.z;
    const float x2 = dx * dx;
    const float y2 = dy * dy;
    const float z2 = dz * dz;
    const float k = mass / 12.0f;
    inertia_tensor.rvec.set((z2 + y2) * k, 0.0f, 0.0f);
    inertia_tensor.uvec.set(0.0f, (z2 + x2) * k, 0.0f);
    inertia_tensor.fvec.set(0.0f, 0.0f, (y2 + x2) * k);
}

// 0x0080E4E0 (:4837)
// Solid sphere inertia: 2/5 m r^2.
void havok_body_calc_mass_props(const float radius, float mass, vector& center_of_mass, matrix& inertia_tensor)
{
    center_of_mass.zero();
    const float i = mass * 0.4f * radius * radius;
    inertia_tensor.rvec.set(i, 0.0f, 0.0f);
    inertia_tensor.uvec.set(0.0f, i, 0.0f);
    inertia_tensor.fvec.set(0.0f, 0.0f, i);
}

// 0x008172A0 (:4852)
void havok_body_calc_mass_props(const hkpShape* shape, float mass, vector& center_of_mass, matrix& inertia_tensor)
{
    hkpRigidBodyCinfo cinfo;
    hkpInertiaTensorComputer::setShapeVolumeMassProperties(shape, mass, cinfo);
    const float* c0 = (const float*)&cinfo.m_inertiaTensor.m_col0;
    const float* c1 = (const float*)&cinfo.m_inertiaTensor.m_col1;
    const float* c2 = (const float*)&cinfo.m_inertiaTensor.m_col2;
    inertia_tensor.rvec.set(c0[0], c0[1], c0[2]);
    inertia_tensor.uvec.set(c1[0], c1[1], c1[2]);
    inertia_tensor.fvec.set(c2[0], c2[1], c2[2]);
    const float* com = (const float*)&cinfo.m_centerOfMass;
    center_of_mass.set(com[0], com[1], com[2]);
}

// 0x00823890 (:4863)
void havok_body_set_linear_dampening(unsigned int handle, float linear_dampening)
{
    if (handle != 0xFFFFFFFF && Hk_rigid_bodies[handle] != nullptr)
    {
        Hk_rigid_bodies[handle]->m_motion.m_motionState.m_linearDamping = linear_dampening;
    }
}

// 0x008173B0 (:4868)
void havok_body_set_linear_dampening(hkpRigidBody* rb, float linear_dampening)
{
    if (rb != nullptr)
    {
        rb->m_motion.m_motionState.m_linearDamping = linear_dampening;
    }
}

// 0x008238D0 (:4874)
void havok_body_set_angular_dampening(unsigned int handle, float angular_dampening)
{
    if (handle != 0xFFFFFFFF && Hk_rigid_bodies[handle] != nullptr)
    {
        Hk_rigid_bodies[handle]->m_motion.m_motionState.m_angularDamping = angular_dampening;
    }
}

// 0x008173F0 (:4879)
void havok_body_set_angular_dampening(hkpRigidBody* rb, float angular_dampening)
{
    if (rb != nullptr)
    {
        rb->m_motion.m_motionState.m_angularDamping = angular_dampening;
    }
}

// 0x00823910 (:4885)
void havok_body_set_angular_velocity(unsigned int handle, const vector& ang_velocity)
{
    havok_body_set_angular_velocity(handle != 0xFFFFFFFF ? Hk_rigid_bodies[handle] : nullptr, ang_velocity);
}

// 0x00817430 (:4890)
void havok_body_set_angular_velocity(hkpRigidBody* rb, const vector& ang_velocity)
{
    hkVector4f v;
    v.set(ang_velocity.x, ang_velocity.y, ang_velocity.z, 0.0f);
    if (rb != nullptr)
    {
        rb->activate();
        rb->m_motion.setAngularVelocity(v);
    }
}

// 0x00823990 (:4901)
float havok_body_get_linear_dampening(unsigned int handle)
{
    if (handle != 0xFFFFFFFF && Hk_rigid_bodies[handle] != nullptr)
    {
        return havok_body_get_linear_dampening(Hk_rigid_bodies[handle]);
    }
    return 0.0f;
}

// 0x008174A0 (:4906)
float havok_body_get_linear_dampening(hkpRigidBody* rb)
{
    if (rb == nullptr)
    {
        return 0.0f;
    }
    float damping;
    rb->m_motion.m_motionState.m_linearDamping.store(&damping);
    return damping;
}

// 0x008239D0 (:4912)
float havok_body_get_angular_dampening(unsigned int handle)
{
    if (handle != 0xFFFFFFFF && Hk_rigid_bodies[handle] != nullptr)
    {
        return havok_body_get_angular_dampening(Hk_rigid_bodies[handle]);
    }
    return 0.0f;
}

// 0x008174C0 (:4917)
float havok_body_get_angular_dampening(hkpRigidBody* rb)
{
    if (rb == nullptr)
    {
        return 0.0f;
    }
    float damping;
    rb->m_motion.m_motionState.m_angularDamping.store(&damping);
    return damping;
}

// 0x008174E0 (:4923)
void havok_body_get_angular_velocity(hkpRigidBody* rb, vector& vel)
{
    if (rb != nullptr)
    {
        const float* v = (const float*)&rb->m_motion.m_angularVelocity;
        vel.set(v[0], v[1], v[2]);
    }
}

// 0x00817510 (:4928)
void havok_body_get_angular_velocity(unsigned int handle, vector& vel)
{
    if (handle != 0xFFFFFFFF && Hk_rigid_bodies[handle] != nullptr)
    {
        havok_body_get_angular_velocity(Hk_rigid_bodies[handle], vel);
    }
}

// 0x00823A10 (:4934)
void havok_body_get_point_velocity(unsigned int handle, const vector& point, vector& velocity)
{
    if (handle != 0xFFFFFFFF && Hk_rigid_bodies[handle] != nullptr)
    {
        havok_body_get_point_velocity(Hk_rigid_bodies[handle], point, velocity);
    }
}

// 0x00817550 (:4938)
// v + w x (point - center of mass).
void havok_body_get_point_velocity(hkpRigidBody* rb, const vector& point, vector& velocity)
{
    if (rb == nullptr)
    {
        return;
    }
    const float* com = (const float*)&rb->m_motion.m_motionState.m_sweptTransform.m_centerOfMass1;
    const float* w = (const float*)&rb->m_motion.m_angularVelocity;
    const float* v = (const float*)&rb->m_motion.m_linearVelocity;
    const float rx = point.x - com[0];
    const float ry = point.y - com[1];
    const float rz = point.z - com[2];
    velocity.x = (rz * w[1] - w[2] * ry) + v[0];
    velocity.y = (rx * w[2] - w[0] * rz) + v[1];
    velocity.z = (ry * w[0] - w[1] * rx) + v[2];
}

// 0x008175F0 (:4950)
void havok_body_add_angular_velocity(unsigned int handle, vector& ang_velocity)
{
    if (handle == 0xFFFFFFFF || Hk_rigid_bodies[handle] == nullptr)
    {
        return;
    }
    hkpRigidBody* rb = Hk_rigid_bodies[handle];
    const float* w = (const float*)&rb->m_motion.m_angularVelocity;
    hkVector4f v;
    v.set(w[0] + ang_velocity.x, w[1] + ang_velocity.y, w[2] + ang_velocity.z, w[3] + 0.0f);
    rb->activate();
    rb->m_motion.setAngularVelocity(v);
}

// 0x008296B0 (:4961)
void havok_body_add_custom_force(unsigned int handle, const vector& accl)
{
    if (handle != 0xFFFFFFFF && Hk_rigid_bodies[handle] != nullptr)
    {
        havok_body_add_custom_force(Hk_rigid_bodies[handle], accl);
    }
}

// 0x00823AC0 (:4968)
// A constant acceleration action on the body (property 15). Adding to an existing action sums the accelerations; the
// action is removed when the sum is almost zero (squared length below 0.001).
void havok_body_add_custom_force(hkpRigidBody* rb, const vector& accl)
{
    if (rb == nullptr)
    {
        return;
    }
    if (rb->hasProperty(HAVOK_PROPERTY_CUSTOM_FORCE, hkpWorldObject::MULTI_THREADING_CHECKS_ENABLE))
    {
        havok_const_accl_action* action = (havok_const_accl_action*)rb->getProperty(HAVOK_PROPERTY_CUSTOM_FORCE, hkpWorldObject::MULTI_THREADING_CHECKS_ENABLE).getPtr();
        if (action != nullptr)
        {
            action->add_force(accl);
            const float* f = (const float*)&action->get_force();
            if (f[1] * f[1] + f[0] * f[0] + f[2] * f[2] < 0.001f)
            {
                Havok_world->removeAction(action);
                havok_remove_property(rb, HAVOK_PROPERTY_CUSTOM_FORCE);
            }
            return;
        }
    }
    havok_const_accl_action* action = new havok_const_accl_action(rb, accl);
    Havok_world->addAction(action);
    action->removeReference();
    if (!rb->hasProperty(HAVOK_PROPERTY_CUSTOM_FORCE, hkpWorldObject::MULTI_THREADING_CHECKS_ENABLE))
    {
        rb->addProperty(HAVOK_PROPERTY_CUSTOM_FORCE, hkSimplePropertyValue((void*)action));
    }
}

// 0x00817680 (:4992)
void havok_body_remove_custom_force(hkpRigidBody* rb)
{
    if (rb == nullptr || !rb->hasProperty(HAVOK_PROPERTY_CUSTOM_FORCE, hkpWorldObject::MULTI_THREADING_CHECKS_ENABLE))
    {
        return;
    }
    hkpAction* action = (hkpAction*)rb->getProperty(HAVOK_PROPERTY_CUSTOM_FORCE, hkpWorldObject::MULTI_THREADING_CHECKS_ENABLE).getPtr();
    if (action != nullptr)
    {
        Havok_world->removeAction(action);
        if (rb->hasProperty(HAVOK_PROPERTY_CUSTOM_FORCE, hkpWorldObject::MULTI_THREADING_CHECKS_ENABLE))
        {
            rb->removeProperty(HAVOK_PROPERTY_CUSTOM_FORCE);
        }
    }
}

// 0x00817710 (:5001)
void havok_body_remove_custom_force(unsigned int handle)
{
    if (handle != 0xFFFFFFFF && Hk_rigid_bodies[handle] != nullptr)
    {
        havok_body_remove_custom_force(Hk_rigid_bodies[handle]);
    }
}

// ---- activation and impulses
// 0x00817730 (:5008)
void havok_body_force_activate(unsigned int handle)
{
    if (handle != 0xFFFFFFFF && Hk_rigid_bodies[handle] != nullptr)
    {
        Hk_rigid_bodies[handle]->activateAsCriticalOperation();
    }
}

// 0x00817750 (:5016)
void havok_body_force_deactivate(unsigned int handle)
{
    if (handle != 0xFFFFFFFF && Hk_rigid_bodies[handle] != nullptr)
    {
        Hk_rigid_bodies[handle]->deactivateAsCriticalOperation();
    }
}

// 0x00817770 (:5024)
bool havok_body_is_active(unsigned int handle)
{
    if (handle != 0xFFFFFFFF && Hk_rigid_bodies[handle] != nullptr)
    {
        return Hk_rigid_bodies[handle]->isActive().m_bool != 0;
    }
    return false;
}

// 0x00823C20 (:5031)
void havok_body_apply_linear_impulse(unsigned int handle, const vector& linear_impulse)
{
    if (handle != 0xFFFFFFFF && Hk_rigid_bodies[handle] != nullptr)
    {
        havok_body_apply_linear_impulse(Hk_rigid_bodies[handle], linear_impulse);
    }
}

// 0x008177A0 (:5036)
// hkpRigidBody::applyLinearImpulse, inlined: activates the body and applies the impulse to its motion.
void havok_body_apply_linear_impulse(hkpRigidBody* rb, const vector& linear_impulse)
{
    if (rb == nullptr)
    {
        return;
    }
    hkVector4f impulse;
    impulse.set(linear_impulse.x, linear_impulse.y, linear_impulse.z, 0.0f);
    rb->activate();
    rb->m_motion.applyLinearImpulse(impulse);
}

// 0x00817810 (:5044)
void havok_body_apply_angular_impulse(hkpRigidBody* rb, const vector& angular_impulse)
{
    if (rb == nullptr)
    {
        return;
    }
    hkVector4f impulse;
    impulse.set(angular_impulse.x, angular_impulse.y, angular_impulse.z, 0.0f);
    rb->activate();
    rb->m_motion.applyAngularImpulse(impulse);
}

// 0x00817880 (:5051)
void havok_body_apply_angular_impulse(unsigned int handle, const vector& angular_impulse)
{
    if (handle != 0xFFFFFFFF && Hk_rigid_bodies[handle] != nullptr)
    {
        havok_body_apply_angular_impulse(Hk_rigid_bodies[handle], angular_impulse);
    }
}

// 0x00823CA0 (:5057)
void havok_body_apply_point_impulse(unsigned int handle, const vector& point_impulse, const vector& pos)
{
    if (handle != 0xFFFFFFFF && Hk_rigid_bodies[handle] != nullptr)
    {
        havok_body_apply_point_impulse(Hk_rigid_bodies[handle], point_impulse, pos);
    }
}

// 0x00817900 (:5062)
void havok_body_apply_point_impulse(hkpRigidBody* rb, const vector& point_impulse, const vector& pos)
{
    if (rb == nullptr)
    {
        return;
    }
    hkVector4f impulse;
    hkVector4f point;
    impulse.set(point_impulse.x, point_impulse.y, point_impulse.z, 0.0f);
    point.set(pos.x, pos.y, pos.z, 0.0f);
    rb->activate();
    rb->m_motion.applyPointImpulse(impulse, point);
}

// 0x008296D0 (:5077)
// A velocity change as an impulse of mass * velocity.
void havok_body_apply_linear_velocity(unsigned int handle, const vector& velocity)
{
    const float mass = havok_body_get_mass(handle);
    hkVector4f impulse;
    impulse.set(mass * velocity.x, velocity.y * mass, velocity.z * mass, 0.0f);
    if (handle != 0xFFFFFFFF && Hk_rigid_bodies[handle] != nullptr)
    {
        hkpRigidBody* rb = Hk_rigid_bodies[handle];
        rb->activate();
        rb->m_motion.applyLinearImpulse(impulse);
    }
}

// 0x00829790 (:5098)
void havok_body_apply_point_velocity(unsigned int handle, const vector& velocity, const vector& point)
{
    const float mass = havok_body_get_mass(handle);
    hkVector4f impulse;
    hkVector4f at;
    impulse.set(velocity.x * mass, velocity.y * mass, velocity.z * mass, 0.0f);
    at.set(point.x, point.y, point.z, 0.0f);
    if (handle != 0xFFFFFFFF && Hk_rigid_bodies[handle] != nullptr)
    {
        hkpRigidBody* rb = Hk_rigid_bodies[handle];
        rb->activate();
        rb->m_motion.applyPointImpulse(impulse, at);
    }
}

// ---- motion types
// 0x00823D40 (:5121)
// Movable bodies become dynamic (and active); a dynamic body made immovable becomes fixed.
void havok_body_set_movable(hkpRigidBody* rb, bool movable)
{
    if (rb == nullptr)
    {
        return;
    }
    const unsigned char type = rb->m_motion.m_type.m_storage;
    const bool is_movable = type != hkpMotion::MOTION_FIXED && type != hkpMotion::MOTION_KEYFRAMED;
    if (movable)
    {
        rb->setMotionType(hkpMotion::MOTION_DYNAMIC, HK_ENTITY_ACTIVATION_DO_ACTIVATE, HK_UPDATE_FILTER_ON_ENTITY_FULL_CHECK);
        if (!rb->isActive().m_bool)
        {
            rb->activate();
        }
    }
    else if (is_movable)
    {
        rb->setMotionType(hkpMotion::MOTION_FIXED, HK_ENTITY_ACTIVATION_DO_ACTIVATE, HK_UPDATE_FILTER_ON_ENTITY_FULL_CHECK);
    }
}

// 0x00823DA0 (:5143)
void havok_body_set_movable(unsigned int handle, bool movable)
{
    havok_body_set_movable(handle != 0xFFFFFFFF ? Hk_rigid_bodies[handle] : nullptr, movable);
}

// 0x008179A0 (:5155)
void havok_body_apply_hard_keyframe(unsigned int handle, const matrix& next_rotation, const vector& next_position, float time_step)
{
    const quaternion q(next_rotation);
    hkQuaternionf rotation;
    rotation.m_vec.set(q.x, q.y, q.z, -q.w); // Havok's w is negated
    hkVector4f position;
    position.set(next_position.x, next_position.y, next_position.z, 0.0f);
    if (handle != 0xFFFFFFFF && Hk_rigid_bodies[handle] != nullptr)
    {
        hkpKeyFrameUtility::applyHardKeyFrame(position, rotation, 1.0f / time_step, Hk_rigid_bodies[handle]);
    }
}

// 0x00817A90 (:5175)
// The body keeps its activation state.
void havok_body_set_motion_type(unsigned int handle, unsigned int type)
{
    if (handle != 0xFFFFFFFF && Hk_rigid_bodies[handle] != nullptr)
    {
        hkpRigidBody* rb = Hk_rigid_bodies[handle];
        rb->setMotionType((hkpMotion::MotionType)type, (hkpEntityActivation)(rb->isActive().m_bool != 0), HK_UPDATE_FILTER_ON_ENTITY_FULL_CHECK);
    }
}

// 0x00817AD0 (:5189)
int havok_body_get_motion_type(hkpRigidBody* rb)
{
    if (rb == nullptr)
    {
        return hkpMotion::MOTION_INVALID;
    }
    return rb->m_motion.m_type.m_storage;
}

// 0x00817AF0 (:5194)
int havok_body_get_motion_type(unsigned int handle)
{
    if (handle != 0xFFFFFFFF && Hk_rigid_bodies[handle] != nullptr)
    {
        return Hk_rigid_bodies[handle]->m_motion.m_type.m_storage;
    }
    return hkpMotion::MOTION_INVALID;
}

// 0x00817B10 (:5200)
// The motion type a keyframed or fixed body returns to.
int havok_body_get_stored_motion_type(unsigned int handle)
{
    if (handle != 0xFFFFFFFF && Hk_rigid_bodies[handle] != nullptr)
    {
        const hkpMotion* stored = Hk_rigid_bodies[handle]->getStoredDynamicMotion();
        if (stored != nullptr)
        {
            return stored->m_type.m_storage;
        }
    }
    return hkpMotion::MOTION_INVALID;
}

// 0x00817B40 (:5212)
bool havok_body_is_movable(hkpRigidBody* rb)
{
    if (rb != nullptr)
    {
        const unsigned char type = rb->m_motion.m_type.m_storage;
        if (type == hkpMotion::MOTION_FIXED || type == hkpMotion::MOTION_KEYFRAMED)
        {
            return false;
        }
    }
    return true;
}

// 0x00817B60 (:5217)
bool havok_body_is_movable(unsigned int handle)
{
    if (handle != 0xFFFFFFFF && Hk_rigid_bodies[handle] != nullptr)
    {
        const unsigned char type = Hk_rigid_bodies[handle]->m_motion.m_type.m_storage;
        if (type == hkpMotion::MOTION_FIXED || type == hkpMotion::MOTION_KEYFRAMED)
        {
            return false;
        }
    }
    return true;
}

// ---- collision
// 0x0080E550 (:5228)
void havok_body_set_collision_type(hkpRigidBody* rb, unsigned int collision_type, bool full_entity_check, bool full_shape_check)
{
    if (rb == nullptr || rb->m_collidable.m_broadPhaseHandle.m_collisionFilterInfo == collision_type)
    {
        return;
    }
    rb->m_collidable.m_broadPhaseHandle.m_collisionFilterInfo = collision_type;
    if (rb->m_world != nullptr)
    {
        Havok_world->updateCollisionFilterOnEntity(rb, (hkpUpdateCollisionFilterOnEntityMode)!full_entity_check, (hkpUpdateCollectionFilterMode)full_shape_check);
    }
}

// 0x00817B90 (:5243)
void havok_body_set_collision_type(unsigned int handle, unsigned int collision_type, bool full_entity_check, bool full_shape_check)
{
    if (handle != 0xFFFFFFFF)
    {
        havok_body_set_collision_type(Hk_rigid_bodies[handle], collision_type, full_entity_check, full_shape_check);
    }
}

// 0x00817BE0 (:5249)
unsigned int havok_body_get_collision_type(unsigned int handle)
{
    if (handle != 0xFFFFFFFF && Hk_rigid_bodies[handle] != nullptr)
    {
        return Hk_rigid_bodies[handle]->m_collidable.m_broadPhaseHandle.m_collisionFilterInfo;
    }
    return COLLIDE_NO_COLLISION;
}

// 0x00817C00 (:5262)
void havok_body_set_collision_quality(unsigned int handle, hkpCollidableQualityType collision_quality)
{
    if (handle != 0xFFFFFFFF && Hk_rigid_bodies[handle] != nullptr)
    {
        hkpRigidBody* rb = Hk_rigid_bodies[handle];
        if (rb->m_collidable.m_broadPhaseHandle.m_objectQualityType != collision_quality)
        {
            rb->m_collidable.m_broadPhaseHandle.m_objectQualityType = (signed char)collision_quality;
        }
    }
}

// 0x00817C30 (:5278)
hkpCollidableQualityType havok_body_get_collision_quality(unsigned int handle)
{
    if (handle != 0xFFFFFFFF && Hk_rigid_bodies[handle] != nullptr)
    {
        return (hkpCollidableQualityType)Hk_rigid_bodies[handle]->m_collidable.m_broadPhaseHandle.m_objectQualityType;
    }
    return HK_COLLIDABLE_QUALITY_INVALID;
}

// 0x0080E590 (:5293)
// As havok_collidable_enable_collision, then the world's filter is updated.
void havok_body_enable_collision(hkpRigidBody* rigid_body, unsigned int* saved_collision_filter_ptr, bool enable)
{
    havok_collidable_enable_collision(&rigid_body->m_collidable, saved_collision_filter_ptr, enable);
    if (rigid_body->m_world != nullptr)
    {
        Havok_world->updateCollisionFilterOnEntity(rigid_body, HK_UPDATE_FILTER_ON_ENTITY_FULL_CHECK, HK_UPDATE_COLLECTION_FILTER_IGNORE_SHAPE_COLLECTIONS);
    }
}

// 0x00817C50 (:5309)
// During the step the replacement is queued (up to 100) for havok_replace_collision_models_end_sim.
void havok_body_replace_collision_model(unsigned int handle, unsigned int new_cm_index)
{
    if (Havok_world_is_simulating)
    {
        if (Num_collision_models_to_replace < 100)
        {
            Collision_models_to_replace[Num_collision_models_to_replace].m_havok_body_handle = handle;
            Collision_models_to_replace[Num_collision_models_to_replace].m_cm_to_use = new_cm_index;
            Num_collision_models_to_replace++;
        }
        return;
    }
    if (handle != 0xFFFFFFFF && Hk_rigid_bodies[handle] != nullptr)
    {
        Hk_rigid_bodies[handle]->setShape(havok_cm_manager_get(new_cm_index));
    }
}

// 0x00817CC0 (:5332)
// Stops the body (zero linear and angular velocity).
void havok_body_freeze(unsigned int handle)
{
    hkVector4f zero;
    zero.set(0.0f, 0.0f, 0.0f, 0.0f);
    if (handle != 0xFFFFFFFF && Hk_rigid_bodies[handle] != nullptr)
    {
        hkpRigidBody* rb = Hk_rigid_bodies[handle];
        rb->activate();
        rb->m_motion.setLinearVelocity(zero);
        rb->activate();
        rb->m_motion.setAngularVelocity(zero);
    }
}

// 0x00817D40 (:5341)
void havok_body_set_auto_remove_level(unsigned int handle, havok_auto_remove_level level)
{
    if (handle != 0xFFFFFFFF && Hk_rigid_bodies[handle] != nullptr)
    {
        havok_body_set_auto_remove_level(Hk_rigid_bodies[handle], level);
    }
}

// 0x0080E5E0 (:5347)
void havok_body_set_auto_remove_level(hkpEntity* ep, havok_auto_remove_level level)
{
    if (ep != nullptr && level < NUM_HAVOK_AUTO_REMOVE_LEVELS)
    {
        ep->m_autoRemoveLevel = (signed char)level;
    }
}

// 0x00817D70 (:5355)
havok_auto_remove_level havok_body_get_auto_remove_level(unsigned int handle)
{
    if (handle != 0xFFFFFFFF && Hk_rigid_bodies[handle] != nullptr)
    {
        return (havok_auto_remove_level)Hk_rigid_bodies[handle]->m_autoRemoveLevel;
    }
    return HAVOK_AUTO_REMOVE_INVALID;
}

// 0x0080E600 (:5361)
havok_auto_remove_level havok_body_get_auto_remove_level(const hkpEntity* ep)
{
    if (ep != nullptr)
    {
        return (havok_auto_remove_level)ep->m_autoRemoveLevel;
    }
    return HAVOK_AUTO_REMOVE_INVALID;
}

// ---- rigid bodies
// 0x00817FE0 (:5899)
// A havok_rigid_body from the construction info and its handle (0xFFFFFFFF without a body).
unsigned int havok_rigid_body_create(const hkpRigidBodyCinfo& cinfo, hkpRigidBody*& hk_rigid_body)
{
    hk_rigid_body = new havok_rigid_body(cinfo);
    if (hk_rigid_body == nullptr)
    {
        return 0xFFFFFFFF;
    }
    return havok_generate_rigid_body_handle(hk_rigid_body);
}

// 0x0080E870 (:5922)
hkpRigidBody* havok_body_get_pointer(unsigned int handle)
{
    if (handle != 0xFFFFFFFF)
    {
        return Hk_rigid_bodies[handle];
    }
    return nullptr;
}

// 0x00818040 (:5973)
// The handle stored in the body's property 6 (0xFFFFFFFF without one).
unsigned int havok_rigid_body_get_handle(const hkpRigidBody* hk_rigid_body)
{
    if (hk_rigid_body != nullptr && hk_rigid_body->hasProperty(6, hkpWorldObject::MULTI_THREADING_CHECKS_ENABLE))
    {
        const unsigned int handle = (unsigned int)hk_rigid_body->getProperty(6, hkpWorldObject::MULTI_THREADING_CHECKS_ENABLE).m_data;
        if (handle != 0xFFFFFFFF)
        {
            return handle;
        }
    }
    return 0xFFFFFFFF;
}

// 0x00823DD0 (:5994)
float havok_body_get_mass(unsigned int handle)
{
    if (handle != 0xFFFFFFFF && Hk_rigid_bodies[handle] != nullptr)
    {
        return havok_body_get_mass(Hk_rigid_bodies[handle]);
    }
    return 0.0f;
}

// 0x008180A0 (:6005)
// Keyframed and fixed bodies report the mass of their stored dynamic motion.
float havok_body_get_mass(hkpRigidBody* rigid_body)
{
    if (rigid_body == nullptr)
    {
        return 0.0f;
    }
    hkpMotion* motion = &rigid_body->m_motion;
    const unsigned char type = rigid_body->m_motion.m_type.m_storage;
    if (type == hkpMotion::MOTION_KEYFRAMED || type == hkpMotion::MOTION_FIXED)
    {
        hkpMotion* stored = rigid_body->getStoredDynamicMotion();
        if (stored != nullptr)
        {
            motion = stored;
        }
    }
    return motion->getMass();
}

// 0x0080E890 (:6032)
// Clamps the diagonal to at least min_axis and scales the tensor up when the diagonal is shorter than min_diag.
void havok_adjust_tensor(matrix& tensor, float min_diag, float min_axis)
{
    float x = tensor.rvec.x;
    if (x - min_axis < 0.0f)
    {
        x = min_axis;
    }
    float y = tensor.uvec.y;
    tensor.rvec.x = x;
    if (y - min_axis < 0.0f)
    {
        y = min_axis;
    }
    float z = tensor.fvec.z;
    tensor.uvec.y = y;
    if (z - min_axis < 0.0f)
    {
        z = min_axis;
    }
    tensor.fvec.z = z;
    const float length = sqrtf(x * x + y * y + z * z);
    if (length < min_diag)
    {
        const float scale = min_diag / length;
        tensor.rvec.x = x * scale;
        tensor.rvec.y = tensor.rvec.y * scale;
        tensor.rvec.z = scale * tensor.rvec.z;
        tensor.uvec.x = tensor.uvec.x * scale;
        tensor.uvec.y = scale * tensor.uvec.y;
        tensor.uvec.z = tensor.uvec.z * scale;
        tensor.fvec.x = tensor.fvec.x * scale;
        tensor.fvec.y = tensor.fvec.y * scale;
        tensor.fvec.z = scale * tensor.fvec.z;
    }
}

// 0x0080EA30 (:6052)
const hkpShape* havok_body_get_shape(unsigned int handle)
{
    if (handle != 0xFFFFFFFF && Hk_rigid_bodies[handle] != nullptr)
    {
        return Hk_rigid_bodies[handle]->m_collidable.m_shape;
    }
    return nullptr;
}

// 0x0080EA50 (:6065)
// After the body's shape was changed in place: refreshes the cached shape data and the broadphase.
void havok_body_update_cached_shape_data(unsigned int handle)
{
    if (handle != 0xFFFFFFFF && Hk_rigid_bodies[handle] != nullptr)
    {
        hkpRigidBody* rb = Hk_rigid_bodies[handle];
        rb->setCachedShapeData(Havok_world, rb->m_collidable.m_shape);
        hkpRigidBody::updateBroadphaseAndResetCollisionInformationOfWarpedBody(rb);
    }
}

// ---- havok_const_accl_action
// 0x008180E0 (:6089)
havok_const_accl_action::havok_const_accl_action(hkpRigidBody* rb, const vector& accl)
    : hkpUnaryAction(rb, 0)
{
    const_accl.set(accl.x, accl.y, accl.z, 0.0f);
}

// 0x008181E0 (:6098)
// Applies mass * acceleration * dt as a linear impulse.
void havok_const_accl_action::applyAction(const hkStepInfo& stepInfo)
{
    hkpEntity* entity = m_entity;
    const float mass = entity->m_motion.getMass();
    const float dt = stepInfo.m_deltaTime;
    const float* a = (const float*)&const_accl;
    hkVector4f impulse;
    impulse.set(mass * a[0] * dt, mass * a[1] * dt, mass * a[2] * dt, mass * a[3] * dt);
    entity->activate();
    entity->m_motion.applyLinearImpulse(impulse);
}

// 0x00818280 (:6117)
hkpAction* havok_const_accl_action::clone(const hkArray<hkpEntity*, hkContainerHeapAllocator>& entities, const hkArray<hkpPhantom*, hkContainerHeapAllocator>& phantoms) const
{
    const float* a = (const float*)&const_accl;
    return new havok_const_accl_action((hkpRigidBody*)entities[0], vector(a[0], a[1], a[2]));
}

// 0x00818310 (:6128)
void havok_const_accl_action::add_force(const vector& accl)
{
    hkVector4f add;
    add.set(accl.x, accl.y, accl.z, 0.0f);
    const_accl.add(add);
}

// 0x0080EA90 (:6135)
const hkVector4f& havok_const_accl_action::get_force()
{
    return const_accl;
}

// 0x00818140
havok_const_accl_action::~havok_const_accl_action()
{
}

// ---- object_collector
// 0x0080EAA0 (:6152)
object_collector::object_collector()
{
    m_earlyOut.m_bool = 0;
    count = 0;
    collect_cb = nullptr;
    m_cb_user_data = nullptr;
    bail_on_collide = false;
}

// 0x00823E40 (:6162)
// Collects the objects of both bodies (the owners of their root bodies), filtered by the collect callback.
void object_collector::addCdBodyPair(const hkpCdBody& bodyA, const hkpCdBody& bodyB)
{
    const hkpCdBody* root = &bodyA;
    while (root->m_parent != nullptr)
    {
        root = root->m_parent;
    }
    hkpWorldObject* wp = havok_world_object((const hkpCollidable*)root);
    if (wp != nullptr && wp->hasProperty(HAVOK_PROPERTY_OBJECT, hkpWorldObject::MULTI_THREADING_CHECKS_ENABLE))
    {
        object* objp = (object*)wp->getProperty(HAVOK_PROPERTY_OBJECT, hkpWorldObject::MULTI_THREADING_CHECKS_ENABLE).getPtr();
        if (objp != nullptr && (collect_cb == nullptr || collect_cb(objp, bodyA.m_shapeKey, m_cb_user_data)))
        {
            add_unique_object(objp->get_handle());
        }
    }
    if (&bodyA != &bodyB)
    {
        root = &bodyB;
        while (root->m_parent != nullptr)
        {
            root = root->m_parent;
        }
        object* objp = object_from_world_obj(havok_world_object((const hkpCollidable*)root));
        if (objp != nullptr && (collect_cb == nullptr || collect_cb(objp, bodyB.m_shapeKey, m_cb_user_data)))
        {
            add_unique_object(objp->get_handle());
        }
    }
}

// 0x00818340 (:6183)
void object_collector::add_object(object* hit_objp, unsigned int shape_key)
{
    if (collect_cb != nullptr && !collect_cb(hit_objp, shape_key, m_cb_user_data))
    {
        return;
    }
    add_unique_object(hit_objp->get_handle());
}

// 0x00818380 (:6190)
// Without room for objects, the collector only tells whether anything collides.
void object_collector::set_buffer(unsigned int* list, int size)
{
    objects = list;
    max_objects = size;
    if (size < 1)
    {
        bail_on_collide = true;
    }
    count = 0;
    reset();
}

// 0x0080EAC0 (:6201)
void object_collector::set_collector_callback(bool (*_collect_cb)(object*, unsigned int, const void*))
{
    collect_cb = _collect_cb;
}

// 0x0080EAD0 (:6206)
void object_collector::set_bail_on_collide()
{
    bail_on_collide = true;
}

// 0x0080EAE0 (:6212)
void object_collector::set_collector_callback_user_data(const void* user_data)
{
    m_cb_user_data = user_data;
}

// 0x0080EAF0 (:6220)
// Stops the query at the first object when bailing, or when the buffer is full.
void object_collector::add_unique_object(unsigned int object_handle)
{
    if (bail_on_collide)
    {
        count = 1;
        m_earlyOut.m_bool = 1;
        return;
    }
    if (count == max_objects)
    {
        m_earlyOut.m_bool = 1;
        return;
    }
    for (int i = 0; i < count; i++)
    {
        if (objects[i] == object_handle)
        {
            return;
        }
    }
    objects[count] = object_handle;
    count++;
}

// 0x0080EB40
object_collector::~object_collector()
{
}

// ---- object queries
// 0x00829880 (:6259)
// The objects whose collidables overlap the box in the world broadphase and collide with collector_type (optionally
// filtered by a sphere or oriented box). The broadphase pairs go to a 32 KB buffer of the thread's scratch pool.
int havok_collect_objects_fast(const vector& bmin, const vector& bmax, unsigned int* objects, int max_objects, const unsigned int collector_type, bool (*collect_cb)(object*, unsigned int, const void*), const void* cb_user_data, collect_objects_filter_info* filter_info)
{
    mempool_mark_scope scope(Vlib_scratch_pool);
    hkAabb aabb;
    aabb.m_min.set(bmin.x, bmin.y, bmin.z, 0.0f);
    aabb.m_max.set(bmax.x, bmax.y, bmax.z, 0.0f);
    hkpBroadPhaseHandlePair* buffer = (hkpBroadPhaseHandlePair*)Vlib_scratch_pool->alloc(0x8000, 0); // 4096 pairs
    if (buffer == nullptr)
    {
        return 0;
    }
    hkArray<hkpBroadPhaseHandlePair, hkContainerHeapAllocator> pairs;
    pairs.m_data = buffer;
    pairs.m_size = 0;
    pairs.m_capacityAndFlags = (0x8000 / (int)sizeof(hkpBroadPhaseHandlePair)) | hkArrayBase<hkpBroadPhaseHandlePair>::DONT_DEALLOCATE_FLAG;
    Havok_world->m_broadPhase->querySingleAabb(aabb, pairs);
    object_collector collector;
    collector.set_buffer(objects, max_objects);
    collector.set_collector_callback(collect_cb);
    collector.set_collector_callback_user_data(cb_user_data);
    for (int i = 0; i < pairs.getSize(); i++)
    {
        const hkpTypedBroadPhaseHandle* handle = (const hkpTypedBroadPhaseHandle*)pairs[i].m_b;
        const hkpCollidable* collidable = (const hkpCollidable*)handle->getOwner();
        if (!Havok_filter->isCollisionEnabled(collector_type, collidable->m_broadPhaseHandle.m_collisionFilterInfo).m_bool)
        {
            continue;
        }
        if (filter_info != nullptr)
        {
            const hkpCdBody* root = collidable;
            while (root->m_parent != nullptr)
            {
                root = root->m_parent;
            }
            object* objp = object_from_world_obj(havok_world_object((const hkpCollidable*)root));
            if (!havok_collect_objects_filter(objp, filter_info))
            {
                continue;
            }
        }
        collector.addCdBodyPair(*collidable, *collidable);
        if (collector.count == max_objects)
        {
            break;
        }
    }
    return collector.count;
}

// 0x00829AB0 (:6326)
int havok_collect_objects_fast(const vector& pos, float radius, unsigned int* objects, int max_objects, const unsigned int collector_type, bool (*collect_cb)(object*, unsigned int, const void*), const void* cb_user_data)
{
    const vector bmin(pos.x - radius, pos.y - radius, pos.z - radius);
    const vector bmax(pos.x + radius, pos.y + radius, pos.z + radius);
    collect_objects_filter_info info;
    info.type = collect_objects_filter_info::SPHERE;
    info.radius = radius;
    info.center = pos;
    return havok_collect_objects_fast(bmin, bmax, objects, max_objects, collector_type, collect_cb, cb_user_data, &info);
}

// Whether an orient is not (almost) the identity: some axis is more than about 2.6 degrees off.
static bool havok_orient_is_rotated(const matrix& orient)
{
    return orient.fvec * IDENTITY_MATRIX.fvec < 0.999f || orient.uvec * IDENTITY_MATRIX.uvec < 0.999f || orient.rvec * IDENTITY_MATRIX.rvec < 0.999f;
}

// 0x00829BA0 (:6345)
// The world box of the oriented box; rotated boxes are filtered by the oriented box itself.
int havok_collect_objects_fast(const vector& local_bmin, const vector& local_bmax, const vector& pos, const matrix& orient, unsigned int* objects, int max_objects, const unsigned int collector_type, bool (*collect_cb)(object*, unsigned int, const void*), const void* cb_user_data)
{
    vector bmin;
    vector bmax;
    bbox_instance(bmin, bmax, orient, pos, local_bmin, local_bmax);
    collect_objects_filter_info info;
    collect_objects_filter_info* filter_info = nullptr;
    if (havok_orient_is_rotated(orient))
    {
        info.orient = orient;
        info.type = collect_objects_filter_info::OBB;
        bbox_extract_obb_info(local_bmin, local_bmax, pos, orient, info.extents, info.center);
        filter_info = &info;
    }
    return havok_collect_objects_fast(bmin, bmax, objects, max_objects, collector_type, collect_cb, cb_user_data, filter_info);
}

// 0x00829D90 (:6366)
// The objects of a type (and subtype unless OT_SUB_UNDEFINED) whose broadphase objects overlap the box in the
// object broadphase bpo_index.
int _havok_collect_objects_by_type_fast(const vector& bmin, const vector& bmax, int bpo_index, object_type type, object_sub_type sub_type, unsigned int* objects, int max_objects, bool (*collect_cb)(object*, unsigned int, const void*), const void* cb_user_data, collect_objects_filter_info* filter_info)
{
    mempool_mark_scope scope(Vlib_scratch_pool);
    hkAabb aabb;
    aabb.m_min.set(bmin.x, bmin.y, bmin.z, 0.0f);
    aabb.m_max.set(bmax.x, bmax.y, bmax.z, 0.0f);
    hkpBroadPhaseHandlePair* buffer = (hkpBroadPhaseHandlePair*)Vlib_scratch_pool->alloc(0x8000, 0); // 4096 pairs
    if (buffer == nullptr)
    {
        return 0;
    }
    hkArray<hkpBroadPhaseHandlePair, hkContainerHeapAllocator> pairs;
    pairs.m_data = buffer;
    pairs.m_size = 0;
    pairs.m_capacityAndFlags = (0x8000 / (int)sizeof(hkpBroadPhaseHandlePair)) | hkArrayBase<hkpBroadPhaseHandlePair>::DONT_DEALLOCATE_FLAG;
    if (!allow_bpo_updates)
    {
        get_current_thread_id(); // result unused (a removed thread check)
    }
    Rfg_broadphase[bpo_index].island_broad_phase->querySingleAabb(aabb, pairs);
    object_collector collector;
    collector.set_buffer(objects, max_objects);
    collector.set_collector_callback(collect_cb);
    collector.set_collector_callback_user_data(cb_user_data);
    for (int i = 0; i < pairs.getSize(); i++)
    {
        object* hit_objp = (object*)((const havok_bpo*)pairs[i].m_b)->user_data;
        if (hit_objp == (object*)0xFFFFFFFF || hit_objp->get_obj_type() != type)
        {
            continue;
        }
        if (sub_type != OT_SUB_UNDEFINED && hit_objp->get_sub_type() != sub_type)
        {
            continue;
        }
        if (filter_info != nullptr && !havok_collect_objects_filter(hit_objp, filter_info))
        {
            continue;
        }
        collector.add_object(hit_objp, 0xFFFFFFFF);
        if (collector.count == max_objects)
        {
            break;
        }
    }
    return collector.count;
}

// 0x00829FA0 (:6431)
int _havok_collect_objects_by_type_fast(const vector& pos, float radius, int bpo_index, object_type type, object_sub_type sub_type, unsigned int* objects, int max_objects, bool (*collect_cb)(object*, unsigned int, const void*), const void* cb_user_data)
{
    const vector bmin(pos.x - radius, pos.y - radius, pos.z - radius);
    const vector bmax(pos.x + radius, pos.y + radius, pos.z + radius);
    collect_objects_filter_info info;
    info.type = collect_objects_filter_info::SPHERE;
    info.radius = radius;
    info.center = pos;
    return _havok_collect_objects_by_type_fast(bmin, bmax, bpo_index, type, sub_type, objects, max_objects, collect_cb, cb_user_data, &info);
}

// 0x0082A0A0 (:6450)
int _havok_collect_objects_by_type_fast(const vector& local_bmin, const vector& local_bmax, const vector& pos, const matrix& orient, int bpo_index, object_type type, object_sub_type sub_type, unsigned int* objects, int max_objects, bool (*collect_cb)(object*, unsigned int, const void*), const void* cb_user_data)
{
    vector bmin;
    vector bmax;
    bbox_instance(bmin, bmax, orient, pos, local_bmin, local_bmax);
    collect_objects_filter_info info;
    collect_objects_filter_info* filter_info = nullptr;
    if (havok_orient_is_rotated(orient))
    {
        info.orient = orient;
        info.type = collect_objects_filter_info::OBB;
        bbox_extract_obb_info(local_bmin, local_bmax, pos, orient, info.extents, info.center);
        filter_info = &info;
    }
    return _havok_collect_objects_by_type_fast(bmin, bmax, bpo_index, type, sub_type, objects, max_objects, collect_cb, cb_user_data, filter_info);
}

// 0x008183E0 (:6471)
// The objects penetrating a sphere: the sphere collector phantom is moved and resized, given the collector's
// collision type for the query, then put back on the no collision layer.
int havok_collect_objects(const vector& pos, float radius, unsigned int* objects, int max_objects, const unsigned int collector_type, bool (*collect_cb)(object*, unsigned int, const void*), const void* cb_user_data)
{
    object_collector collector;
    collector.set_buffer(objects, max_objects);
    collector.set_collector_callback(collect_cb);
    collector.set_collector_callback_user_data(cb_user_data);
    hkpSimpleShapePhantom* phantom = Sphere_collector;
    ((hkpSphereShape*)phantom->m_collidable.m_shape)->m_radius = radius;
    hkVector4f position;
    position.set(pos.x, pos.y, pos.z, 0.0f);
    phantom->setPosition(position, 0.0f);
    phantom->m_collidable.m_broadPhaseHandle.m_collisionFilterInfo = collector_type;
    Havok_world->updateCollisionFilterOnPhantom(Sphere_collector, HK_UPDATE_COLLECTION_FILTER_PROCESS_SHAPE_COLLECTIONS);
    Sphere_collector->getPenetrations(collector, nullptr);
    phantom->m_collidable.m_broadPhaseHandle.m_collisionFilterInfo = COLLIDE_NO_COLLISION;
    Havok_world->updateCollisionFilterOnPhantom(Sphere_collector, HK_UPDATE_COLLECTION_FILTER_IGNORE_SHAPE_COLLECTIONS);
    return collector.count;
}

// 0x008184F0 (:6514)
// As above with the box collector, sized and placed as the oriented box.
int havok_collect_objects(const vector& bmin, const vector& bmax, const vector& pos, const matrix& orient, unsigned int* objects, int max_objects, const unsigned int collector_type, bool (*collect_cb)(object*, unsigned int, const void*), const void* cb_user_data)
{
    vector extents;
    vector center;
    bbox_extract_obb_info(bmin, bmax, pos, orient, extents, center);
    hkVector4f half_extents;
    half_extents.set(extents.x, extents.y, extents.z, 0.0f);
    object_collector collector;
    collector.set_buffer(objects, max_objects);
    collector.set_collector_callback(collect_cb);
    collector.set_collector_callback_user_data(cb_user_data);
    hkpSimpleShapePhantom* phantom = Box_collector;
    ((hkpBoxShape*)phantom->m_collidable.m_shape)->setHalfExtents(half_extents);
    hkTransformf transform;
    game_to_havok_transform(transform, center, orient);
    phantom->setTransform(transform);
    phantom->m_collidable.m_broadPhaseHandle.m_collisionFilterInfo = collector_type;
    Havok_world->updateCollisionFilterOnPhantom(Box_collector, HK_UPDATE_COLLECTION_FILTER_PROCESS_SHAPE_COLLECTIONS);
    Box_collector->getPenetrations(collector, nullptr);
    phantom->m_collidable.m_broadPhaseHandle.m_collisionFilterInfo = COLLIDE_NO_COLLISION;
    Havok_world->updateCollisionFilterOnPhantom(Box_collector, HK_UPDATE_COLLECTION_FILTER_IGNORE_SHAPE_COLLECTIONS);
    return collector.count;
}

// 0x00823F20 (:6565)
int havok_collect_objects_in_capsule(raycast_input rin, unsigned int* objects, int max_objects, bool (*collect_cb)(object*, unsigned int, const void*))
{
    return havok_collect_objects_in_capsule(rin.start, rin.end, rin.radius, objects, max_objects, rin.m_filter_info, collect_cb, rin.filter_params);
}

// 0x00823F60 (:6574)
// An upright capsule standing on pos (the ends at pos + radius and pos + radius + height).
int havok_collect_objects_in_capsule(const vector& pos, const float& radius, const float& height, unsigned int* objects, int max_objects, const unsigned int collector_type, bool (*collect_cb)(object*, unsigned int, const void*), const void* cb_user_data)
{
    const vector top(pos.x, height + radius + pos.y, pos.z);
    const vector bottom(pos.x, radius + pos.y, pos.z);
    return havok_collect_objects_in_capsule(bottom, top, radius, objects, max_objects, collector_type, collect_cb, cb_user_data);
}

// The MOPP tree of a zone's (vehicle) nav mesh, or null. Magic numbers: 0x7FFF collision models; shape type 9 is
// hkcdShapeType::MOPP, which is what havok_create_navmesh_shape makes.
static const hkMoppBvTreeShapeBase* get_zone_navmesh_tree(const obj_zone* zone, bool vehicle_pfg)
{
    unsigned int cm_index = vehicle_pfg ? zone->zone_pfg.vehicle_pfg_shape : zone->zone_pfg.pfg_shape;
    if (cm_index >= 0x7FFF)
    {
        return nullptr;
    }
    const hkpShape* shape = Collision_models[cm_index];
    if (shape == nullptr || shape->m_type.m_storage != 9)
    {
        return nullptr;
    }
    return static_cast<const hkMoppBvTreeShapeBase*>(static_cast<const hkpBvTreeShape*>(shape));
}

// 0x0082D5F0 (:6708)
// The cells of the nav meshes in a cube around pos, from the MOPP trees of up to 4 zones. Shape keys are cell indices.
int havok_collect_nav_cells(const vector& pos, float radius, nav_cell** nav_cells, int max_nav_cells, bool vehicle_pfg)
{
    const vector bmin((float)((double)pos.x - (double)radius), (float)((double)pos.y - (double)radius), (float)((double)pos.z - (double)radius));
    const vector bmax((float)((double)pos.x + (double)radius), (float)((double)pos.y + (double)radius), (float)((double)pos.z + (double)radius));
    hkAabb aabb;
    aabb.m_min.set(bmin.x, bmin.y, bmin.z, 0.0f);
    aabb.m_max.set(bmax.x, bmax.y, bmax.z, 0.0f);
    obj_zone* zones[4];
    int num_zones = World.get_obj_zones_by_bbox(bmin, bmax, zones, 4);
    int count = 0;
    for (int i = 0; i < num_zones && count < max_nav_cells; i++)
    {
        const obj_zone* zone = zones[i];
        const hkMoppBvTreeShapeBase* tree = zone != nullptr ? get_zone_navmesh_tree(zone, vehicle_pfg) : nullptr;
        if (tree == nullptr)
        {
            continue;
        }
        hkArray<unsigned int, hkContainerHeapAllocator> keys;
        tree->queryAabb(aabb, keys);
        const bb_pfg_layer* layer = vehicle_pfg ? zone->zone_pfg.vehicle_pfg : zone->zone_pfg.pfg;
        for (int k = 0; k < keys.getSize() && count < max_nav_cells; k++)
        {
            if (keys[k] < layer->num_nav_cells)
            {
                nav_cells[count++] = const_cast<nav_cell*>(layer->nav_cells.to_pointer()) + keys[k];
            }
        }
    }
    return count;
}

// 0x0082D820 (:6860)
// The same for an oriented box. With a callback the search no longer stops at max_nav_cells: every cell goes to the
// callback and the array only keeps the first ones.
int havok_collect_nav_cells(const vector& bmin, const vector& bmax, const vector& pos, const matrix& orient, nav_cell** nav_cells, int max_nav_cells, bool vehicle_pfg, void (*collect_callback)(nav_cell*))
{
    // Magic number: the query's tolerance, half a metre.
    const float OBB_TOLERANCE = 0.5f;
    vector extents, center;
    bbox_extract_obb_info(bmin, bmax, pos, orient, extents, center);
    hkVector4f extent;
    extent.set(extents.x, extents.y, extents.z, 0.0f);
    hkTransformf obb_to_mopp;
    obb_to_mopp.m_rotation.m_col0.set(orient.rvec.x, orient.rvec.y, orient.rvec.z, 0.0f);
    obb_to_mopp.m_rotation.m_col1.set(orient.uvec.x, orient.uvec.y, orient.uvec.z, 0.0f);
    obb_to_mopp.m_rotation.m_col2.set(orient.fvec.x, orient.fvec.y, orient.fvec.z, 0.0f);
    obb_to_mopp.m_translation.set(center.x, center.y, center.z, 0.0f);
    vector world_min, world_max;
    bbox_instance(world_min, world_max, orient, pos, bmin, bmax);
    obj_zone* zones[4];
    int num_zones = World.get_obj_zones_by_bbox(world_min, world_max, zones, 4);
    int count = 0;
    for (int i = 0; i < num_zones && (collect_callback != nullptr || count < max_nav_cells); i++)
    {
        const obj_zone* zone = zones[i];
        const hkMoppBvTreeShapeBase* tree = zone != nullptr ? get_zone_navmesh_tree(zone, vehicle_pfg) : nullptr;
        if (tree == nullptr)
        {
            continue;
        }
        hkArray<unsigned int, hkContainerHeapAllocator> keys;
        tree->queryObb(obb_to_mopp, extent, OBB_TOLERANCE, keys);
        const bb_pfg_layer* layer = vehicle_pfg ? zone->zone_pfg.vehicle_pfg : zone->zone_pfg.pfg;
        for (int k = 0; k < keys.getSize() && (collect_callback != nullptr || count < max_nav_cells); k++)
        {
            if (keys[k] >= layer->num_nav_cells)
            {
                continue;
            }
            nav_cell* cell = const_cast<nav_cell*>(layer->nav_cells.to_pointer()) + keys[k];
            if (collect_callback != nullptr)
            {
                collect_callback(cell);
            }
            if (count < max_nav_cells)
            {
                nav_cells[count++] = cell;
            }
        }
    }
    return count;
}

// 0x008186B0 (:6584)
// As havok_collect_objects with the capsule collector between pos1 and pos2 (placed at pos1).
int havok_collect_objects_in_capsule(const vector& pos1, const vector& pos2, const float& radius, unsigned int* objects, int max_objects, const unsigned int collector_type, bool (*collect_cb)(object*, unsigned int, const void*), const void* cb_user_data)
{
    hkpSimpleShapePhantom* phantom = Capsule_collector;
    hkpCapsuleShape* capsule = (hkpCapsuleShape*)phantom->m_collidable.m_shape;
    capsule->m_radius = radius;
    capsule->m_vertexA.set(0.0f, 0.0f, 0.0f, radius); // the radius is kept in w
    capsule->m_vertexB.set(pos2.x - pos1.x, pos2.y - pos1.y, pos2.z - pos1.z, radius);
    hkTransformf transform;
    transform.m_rotation.setIdentity();
    transform.m_translation.set(pos1.x, pos1.y, pos1.z, 0.0f);
    phantom->setTransform(transform);
    object_collector collector;
    collector.set_buffer(objects, max_objects);
    collector.set_collector_callback(collect_cb);
    phantom->m_collidable.m_broadPhaseHandle.m_collisionFilterInfo = collector_type;
    collector.set_collector_callback_user_data(cb_user_data);
    Havok_world->updateCollisionFilterOnPhantom(Capsule_collector, HK_UPDATE_COLLECTION_FILTER_PROCESS_SHAPE_COLLECTIONS);
    Capsule_collector->getPenetrations(collector, nullptr);
    phantom->m_collidable.m_broadPhaseHandle.m_collisionFilterInfo = COLLIDE_NO_COLLISION;
    Havok_world->updateCollisionFilterOnPhantom(Capsule_collector, HK_UPDATE_COLLECTION_FILTER_IGNORE_SHAPE_COLLECTIONS);
    return collector.count;
}

// 0x00818850 (:6945)
bool havok_is_box_empty_oriented(const vector& local_bmin, const vector& local_bmax, const vector& pos, const matrix& orient, const unsigned int& collision_type)
{
    return havok_collect_objects(local_bmin, local_bmax, pos, orient, nullptr, 0, collision_type, nullptr, nullptr) == 0;
}

// 0x0080EB60 (:6956)
bool havok_world_is_simulating()
{
    return Havok_world_is_simulating;
}

// 0x00818890 (:6962)
// Whether the two bodies penetrate, by the dispatcher's penetration function for their shape types.
bool havok_rigid_bodies_are_colliding(hkpRigidBody* ra, hkpRigidBody* rb)
{
    if (ra == nullptr || rb == nullptr)
    {
        return false;
    }
    const hkpCollidable* a = &ra->m_collidable;
    const hkpCollidable* b = &rb->m_collidable;
    object_collector collector;
    collector.set_buffer(nullptr, 0); // only whether anything collides
    const hkpCollisionInput& input = *Havok_world->m_collisionInput;
    hkpCollisionDispatcher* dispatcher = input.m_dispatcher.val();
    dispatcher->getGetPenetrationsFunc((hkcdShapeType::ShapeTypeEnum)a->m_shape->m_type.m_storage, (hkcdShapeType::ShapeTypeEnum)b->m_shape->m_type.m_storage)(*a, *b, input, collector);
    return collector.count > 0;
}

// 0x00818930 (:6990)
object* object_from_world_obj(const hkpWorldObject* wp)
{
    if (wp != nullptr && wp->hasProperty(HAVOK_PROPERTY_OBJECT, hkpWorldObject::MULTI_THREADING_CHECKS_ENABLE))
    {
        return (object*)wp->getProperty(HAVOK_PROPERTY_OBJECT, hkpWorldObject::MULTI_THREADING_CHECKS_ENABLE).getPtr();
    }
    return nullptr;
}

// 0x00818980 (:6996)
// Activates the bodies in the oriented box's world box, grown by the convex radius.
void havok_activate_region(const vector& pos, const matrix& orient, const vector& local_bmin, const vector& local_bmax)
{
    vector bmin;
    vector bmax;
    bbox_instance(bmin, bmax, orient, pos, local_bmin, local_bmax);
    bmin.x -= hkConvexShapeDefaultRadius;
    bmin.y -= hkConvexShapeDefaultRadius;
    bmin.z -= hkConvexShapeDefaultRadius;
    bmax.x += hkConvexShapeDefaultRadius;
    bmax.y += hkConvexShapeDefaultRadius;
    bmax.z += hkConvexShapeDefaultRadius;
    hkAabb aabb;
    aabb.m_min.set(bmin.x, bmin.y, bmin.z, 0.0f);
    aabb.m_max.set(bmax.x, bmax.y, bmax.z, 0.0f);
    Havok_world->activateRegion(aabb);
}

// ---- status
// 0x0080EB70 (:7015)
// A system's status only rises; it falls back to OK 10 s (errors) or 5 s (warnings) after the last report.
void havok_set_status(havok_system system, havok_system_status status)
{
    if (system >= NUM_HAVOK_SYSTEMS)
    {
        return;
    }
    if ((int)Havok_system_info[system].status <= (int)status)
    {
        Havok_system_info[system].status = status;
        timestamp& delay = Havok_system_info[system].delay;
        const int delta_milliseconds = status == HAVOK_STATUS_ERROR ? 10000 : 5000;
        if (!delay.valid() || delay.time_until() < delta_milliseconds)
        {
            delay.set(delta_milliseconds);
        }
    }
    Havok_step_time_leftover = fmodf(Havok_step_time_leftover, Havok_step_time);
}

// 0x0080EBF0 (:7036)
havok_system_status havok_get_status(havok_system system)
{
    if (system < NUM_HAVOK_SYSTEMS)
    {
        return Havok_system_info[system].status;
    }
    return HAVOK_STATUS_INVALID;
}
