// Original: volition/rfg/code/havok/havok.h
// Implementation: volition/rfg/code/havok/havok.cpp
//
// The game side of the Havok world: bodies (havok_body_* by handle), entities and phantoms added in batches, status of
// the Havok subsystems (havok_system_info per havok_system: status and a delay timer) and the step.
// havok_simulate (decompiled) flushes pending entity/phantom adds, then steps the world at a fixed 1/60 s
// (Havok_step_time; or one Frametime step when Havok_use_single_step) as many times as the accumulated time allows,
// keeps the remainder in Havok_step_time_leftover, computes Havok_interpolate_percent for rendering, then processes
// islands, ragdolls, collision events, replaced collision models and copies the body transforms back to the objects
// (havok_update_pos_and_orient). havok_rigid_body is the hkpRigidBody subclass the game creates. Placement of
// havok_system_info inferred.
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4f.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcollidablequalitytype.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/query/hkpcdbodypaircollector.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/misc/phantomcallback/hkpphantomcallbackshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/action/hkpunaryaction.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/hkpconstraintinstance.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkprigidbody.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkprigidbodycinfo.h"
#include "volition/rfg/code/objects/object_type.h"
#include "volition/vlib/code/math/bbox.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/time/timestamp.h"

struct bb_pfg_layer;
struct cpu_job;
union cpu_job_arg;
class game_aabb_phantom;
struct hkContainerHeapAllocator;
class hkStepInfo;
class hkpAction;
class hkpCdBody;
class hkpCollidable;
struct hkpCollisionInput;
class hkpConstraintData;
class hkpEntity;
class hkpParametricCurve;
class hkpPhantom;
class hkpWorld;
class hkpShape;
class hkpSimpleShapePhantom;
class hkpWorldObject;
struct nav_cell;
class obj_zone;
class object;
class raycast_input;

enum havok_system_status
{
    HAVOK_STATUS_INVALID = -1,
    HAVOK_STATUS_OK = 0,
    HAVOK_STATUS_WARNING = 1,
    HAVOK_STATUS_ERROR = 2,
    NUM_HAVOK_STATUS_STATES = 3,
};

// size 0xC
struct havok_system_info
{
    havok_system_info(const char* n); // 0x0080DD10 (:237)
    void do_frame(); // 0x0080DD30 (:244)

    havok_system_status status; // 0x00
    timestamp delay; // 0x04
    const char* name; // 0x08
};

// size 0x220
class havok_rigid_body : public hkpRigidBody
{
public:
    havok_rigid_body(const hkpRigidBodyCinfo& cinfo); // 0x0080DD50 (:264)
    virtual ~havok_rigid_body(); // vtbl+0x00 0x0080DD70 (:269)

};

enum havok_system
{
    HAVOK_SYSTEM_INVALID = -1,
    HAVOK_SYSTEM_MEMORY = 0,
    HAVOK_SYSTEM_SIMULATION = 1,
    NUM_HAVOK_SYSTEMS = 2,
};

enum havok_auto_remove_level
{
    HAVOK_AUTO_REMOVE_INVALID = -1,
    HAVOK_AUTO_REMOVE_NEVER = 0,
    HAVOK_AUTO_REMOVE_LAST = 1,
    HAVOK_AUTO_REMOVE_FOURTH = 2,
    HAVOK_AUTO_REMOVE_THIRD = 3,
    HAVOK_AUTO_REMOVE_SECOND = 4,
    HAVOK_AUTO_REMOVE_FIRST = 5,
    NUM_HAVOK_AUTO_REMOVE_LEVELS = 6,
};

// Keys of the hkpWorldObject properties the game stores (values from the binary; names inferred).
enum havok_property_key
{
    HAVOK_PROPERTY_OBJECT = 0, // object* of the body or phantom (the zone for navmesh bodies)
    HAVOK_PROPERTY_IGNORE_HANDLE = 2, // object handle the body does not collide with (havok_game_filter)
    HAVOK_PROPERTY_PHYSICAL_MATERIAL = 3, // rfg physical material index
    HAVOK_PROPERTY_EFFECT_MATERIAL = 4, // rfg effect material index
    HAVOK_PROPERTY_CUSTOM_FORCE = 15, // havok_const_accl_action* (havok_body_add_custom_force)
};

enum havok_shape_type
{
    HAVOK_SHAPE_TYPE_UNDEFINED = 0,
    HAVOK_SHAPE_TYPE_SPHERE = 1,
    HAVOK_SHAPE_TYPE_CAPSULE = 2,
    HAVOK_SHAPE_TYPE_BOX = 3,
    HAVOK_SHAPE_TYPE_COLLISION_MODEL = 4,
    HAVOK_SHAPE_TYPE_DESTROYABLE = 5,
    HAVOK_SHAPE_TYPE_VEHICLE = 6,
    HAVOK_SHAPE_TYPE_WORLD = 7,
    HAVOK_SHAPE_TYPE_NAVMESH = 8,
    HAVOK_SHAPE_TYPE_CYLINDER = 9,
    NUM_HAVOK_SHAPE_TYPES = 10,
};

// Collision filter infos (physics_create_info::collision_type; collision layer in the low 16 bits, system group in
// the high 16), const globals of havok.cpp with the values of rfg.exe.
extern const unsigned int COLLIDE_WORLD; // 0x0165FE04, 0x00010002
extern const unsigned int COLLIDE_PLAYER; // 0x0165FE08, 0x00000006
extern const unsigned int COLLIDE_NPC; // 0x0165FE0C, 0x00000007
extern const unsigned int COLLIDE_HUMAN_MINIMUM; // 0x0165FE10, 0x0000003F
extern const unsigned int COLLIDE_TURRET; // 0x0165FE14, 0x00090008
extern const unsigned int COLLIDE_VEHICLE; // 0x0165FE18, 0x00000009
extern const unsigned int COLLIDE_WALKER; // 0x0165FE1C, 0x0000000C
extern const unsigned int COLLIDE_WALKER_HULL; // 0x0165FE20, 0x0000000E
extern const unsigned int COLLIDE_FLYER; // 0x0165FE24, 0x0000000F
extern const unsigned int COLLIDE_RAGDOLL; // 0x0165FE28, 0x00020005
extern const unsigned int COLLIDE_MOVER; // 0x0165FE2C, 0x00000003
extern const unsigned int COLLIDE_ITEM; // 0x0165FE30, 0x00090004
extern const unsigned int COLLIDE_ONLY_FOR_OBJECT_COLLECTION; // 0x0165FE34, 0x0006003B
extern const unsigned int COLLIDE_HUMAN_HIT_DETECT; // 0x0165FE38, 0x00060031
extern const unsigned int COLLIDE_PROJECTILE; // 0x0165FE3C, 0x00050010
extern const unsigned int COLLIDE_PROJECTILE_ATTACHED; // 0x0165FE40, 0x00060011
extern const unsigned int COLLIDE_PERSONAL_SHIELD; // 0x0165FE44, 0x00060012
extern const unsigned int COLLIDE_OBJECT_COLLECTOR; // 0x0165FE48, 0x00000013
extern const unsigned int COLLIDE_OBJECT_COLLECTOR_NO_ITEMS; // 0x0165FE4C, 0x00000014
extern const unsigned int COLLIDE_OBJECT_COLLECTOR_VEHICLES_ONLY; // 0x0165FE50, 0x00000015
extern const unsigned int COLLIDE_NAVMESH; // 0x0165FE54, 0x00000017
extern const unsigned int COLLIDE_VEHICLE_NAVMESH; // 0x0165FE58, 0x00000018
extern const unsigned int COLLIDE_NO_COLLISION; // 0x0165FE5C, 0x00060016
extern const unsigned int COLLIDE_HUMAN_ON_LADDER; // 0x0165FE60, 0x00000032
extern const unsigned int COLLIDE_RFG_INITIAL_POSITION; // 0x0165FE64, 0x00080027
extern const unsigned int COLLIDE_RFG_NORMAL; // 0x0165FE68, 0x00000028
extern const unsigned int COLLIDE_RFG_NORMAL_FAST; // 0x0165FE6C, 0x00060029
extern const unsigned int COLLIDE_RFG_LITE; // 0x0165FE70, 0x0006002A
extern const unsigned int COLLIDE_RFG_ULTRA_LITE; // 0x0165FE74, 0x0006002B
extern const unsigned int COLLIDE_RFG_WORLD_ONLY; // 0x0165FE78, 0x0006002C
extern const unsigned int COLLIDE_RAY_RFG_DESTROYABLE; // 0x0165FE7C, 0x0000002D
extern const unsigned int COLLIDE_RFG_STRESS; // 0x0165FE80, 0x0000002E
extern const unsigned int COLLIDE_RAY_DEFAULT; // 0x0165FE84, 0x00000019
extern const unsigned int COLLIDE_RAY_TERRAIN_ONLY; // 0x0165FE88, 0x0000001B
extern const unsigned int COLLIDE_RAY_WORLD_ONLY; // 0x0165FE8C, 0x0000001C
extern const unsigned int COLLIDE_RAY_WALKING_SURFACE_ONLY; // 0x0165FE90, 0x0000001D
extern const unsigned int COLLIDE_KEYFRAMED; // 0x0165FE94, 0x00070024
extern const unsigned int COLLIDE_RAY_NAVMESH_ONLY; // 0x0165FE98, 0x0000001F
extern const unsigned int COLLIDE_RAY_RAGDOLL_ONLY; // 0x0165FE9C, 0x00000020
extern const unsigned int COLLIDE_RAY_VEHICLE_NAVMESH_ONLY; // 0x0165FEA0, 0x00000022
extern const unsigned int COLLIDE_RAY_VEHICLE_ENTER_EXIT; // 0x0165FEA4, 0x00000023
extern const unsigned int COLLIDE_RAY_CITY; // 0x0165FEA8, 0x0000001E
extern const unsigned int COLLIDE_RAY_PLAYER_COVER; // 0x0165FEAC, 0x00000021
extern const unsigned int COLLIDE_DEAD_HUMAN; // 0x0165FEB0, 0x00000025
extern const unsigned int COLLIDE_PHYSICAL_OBJECT; // 0x0165FEB4, 0x00060026
extern const unsigned int COLLIDE_PHYSICAL_OBJECT_SELF; // 0x0165FEB8, 0x00060026
extern const unsigned int COLLIDE_CAMERA; // 0x0165FEBC, 0x00000030
extern const unsigned int COLLIDE_HUMAN_NOT_OTHER_HUMANS; // 0x0165FEC0, 0x00000034
extern const unsigned int COLLIDE_WORLD_ANCHOR; // 0x0165FEC4, 0x00060036
extern const unsigned int COLLIDE_DYNAMIC_LINK; // 0x0165FEC8, 0x00060037
extern const unsigned int COLLIDE_DELETE_OBJECT; // 0x0165FECC, 0x00000038
extern const unsigned int COLLIDE_RAY_VEHICLE_WHEEL; // 0x0165FED0, 0x00000039
extern const unsigned int COLLIDE_EFFECT_RIGID_BODY; // 0x0165FED4, 0x0000003A
extern const unsigned int COLLIDE_CLOTH_SIM; // 0x0165FED8, 0x0000003C
extern const unsigned int COLLIDE_INVISIBLE_BARRIER; // 0x0165FEDC, 0x0006003D
extern const unsigned int COLLIDE_FORCE_FIELD; // 0x0165FEE0, 0x0000003E

// size 0x104
struct physics_create_info
{
    physics_create_info(); // 0x008168F0 (:2130)

    unsigned int collision_type; // 0x00
    vector pos; // 0x04
    matrix orient; // 0x10
    vector size; // 0x34
    vector top; // 0x40
    vector bottom; // 0x4C
    float radius; // 0x58
    float angular_damping; // 0x5C
    float linear_damping; // 0x60
    float max_angular_vel; // 0x64
    float max_linear_vel; // 0x68
    float mass; // 0x6C
    float friction; // 0x70
    float restitution; // 0x74
    object* obj_ptr; // 0x78
    matrix precalc_tensor; // 0x7C
    vector precalc_center_of_mass; // 0xA0
    int flags; // 0xAC
    vector local_pos; // 0xB0
    matrix local_orient; // 0xBC
    unsigned int cm_index; // 0xE0
    hkpCollidableQualityType simulation_quality_type; // 0xE4
    hkpRigidBodyCinfo::SolverDeactivation solver_deactivation; // 0xE8
    int motion_type; // 0xEC
    const hkpShape* shape; // 0xF0
    bool (*enter_callback)(object*, object*); // 0xF4
    bool (*leave_callback)(object*, object*); // 0xF8
    havok_shape_type shape_type; // 0xFC
    unsigned char physical_material; // 0x100
    unsigned char effect_material; // 0x101
    unsigned char auto_remove_level; // 0x102
    unsigned char padding; // 0x103
};

// size 0x34
struct ragdoll_constraint_info
{
    ragdoll_constraint_info(); // 0x00A15F40

    float h_fov_pos; // 0x00
    float h_fov_neg; // 0x04
    float v_fov_pos; // 0x08
    float v_fov_neg; // 0x0C
    vector pivot; // 0x10
    vector twist_axis; // 0x1C
    vector plane_axis; // 0x28
};

// size 0x44
struct collect_objects_filter_info
{
    enum filter_type
    {
        SPHERE = 0,
        OBB = 1,
    };

    collect_objects_filter_info() {} // 0x0080EB50, empty (the vector and matrix members are left uninitialized)

    filter_type type; // 0x00
    float radius; // 0x04
    vector center; // 0x08
    matrix orient; // 0x14
    vector extents; // 0x38
};

// size 0x18
class havok_region_callback_shape : public hkpPhantomCallbackShape
{
public:
    havok_region_callback_shape(bool (*phantom_enter_callback)(object*, object*), bool (*phantom_leave_callback)(object*, object*)); // 0x00816430 (:301)
    virtual void phantomEnterEvent(const hkpCollidable* collidableA, const hkpCollidable* collidableB, const hkpCollisionInput& env); // vtbl+0x48 0x008224E0 (:308)
    virtual void phantomLeaveEvent(const hkpCollidable* collidableA, const hkpCollidable* collidableB); // vtbl+0x4C 0x00822550 (:335)
    virtual ~havok_region_callback_shape(); // vtbl+0x00 0x00816460

protected:
    bool (*enter_callback)(object*, object*); // 0x10
    bool (*leave_callback)(object*, object*); // 0x14
};

// size 0x30
class havok_const_accl_action : public hkpUnaryAction
{
public:
    havok_const_accl_action(hkpRigidBody* rb, const vector& accl); // 0x008180E0 (:6089)
    virtual void applyAction(const hkStepInfo& stepInfo); // vtbl+0x10 0x008181E0 (:6098)
    virtual hkpAction* clone(const hkArray<hkpEntity*,hkContainerHeapAllocator>& entities, const hkArray<hkpPhantom*,hkContainerHeapAllocator>& phantoms) const; // vtbl+0x20 0x00818280 (:6117)
    void add_force(const vector& accl); // 0x00818310 (:6128)
    const hkVector4f& get_force(); // 0x0080EA90 (:6135)
    virtual ~havok_const_accl_action(); // vtbl+0x00 0x00818140

private:
    hkVector4f const_accl; // 0x20
};

// size 0x20
class object_collector : public hkpCdBodyPairCollector
{
public:
    object_collector(); // 0x0080EAA0 (:6152)
    virtual void addCdBodyPair(const hkpCdBody& bodyA, const hkpCdBody& bodyB); // vtbl+0x04 0x00823E40 (:6162)
    void add_object(object* hit_objp, unsigned int shape_key); // 0x00818340 (:6183)
    void set_buffer(unsigned int* list, int size); // 0x00818380 (:6190)
    void set_collector_callback(bool (*_collect_cb)(object*, unsigned int, const void*)); // 0x0080EAC0 (:6201)
    void set_bail_on_collide(); // 0x0080EAD0 (:6206)
    void set_collector_callback_user_data(const void* user_data); // 0x0080EAE0 (:6212)
private:
    void add_unique_object(unsigned int object_handle); // 0x0080EAF0 (:6220)
public:
    virtual ~object_collector(); // vtbl+0x00 0x0080EB40

    unsigned int* objects; // 0x08
    int max_objects; // 0x0C
    int count; // 0x10
private:
    bool bail_on_collide; // 0x14
    bool (*collect_cb)(object*, unsigned int, const void*); // 0x18
    const void* m_cb_user_data; // 0x1C
};

void unused_dcf_Havok_use_single_step(); // 0x0080DDA0 (:607)
void unused_dcf_havok_mem(); // 0x00829420 (:636)
unsigned int havok_generate_rigid_body_handle(hkpRigidBody* hk_rigid_body); // 0x0080DE10 (:764)
void havok_free_rigid_body_handle(unsigned int handle); // 0x0080DE80 (:801)
hkpRigidBody* havok_get_pointer_from_rigid_body_handle(unsigned int handle); // 0x0080DEB0 (:812)
void havok_add_entity(hkpEntity* ep, bool init_active, bool batch); // 0x0083DB00 (:823)
void havok_add_entity_flush(); // 0x0083CBB0 (:852)
void havok_remove_entity(hkpEntity* ep, bool batch); // 0x008225C0 (:904)
void havok_remove_entity_flush(); // 0x00816580 (:932)
void havok_add_phantom(hkpPhantom* pp, bool batch); // 0x00822670 (:949)
void havok_add_phantom_flush(); // 0x008165F0 (:969)
void havok_remove_phantom(hkpPhantom* pp, bool batch); // 0x00822700 (:1000)
void havok_remove_phantom_flush(); // 0x008166F0 (:1027)
matrix43 havok_interpolate_transform(const matrix43& old_transform, const matrix43& new_transform); // 0x0080E080 (:1814)
void havok_interpolate_object(object& obj, const matrix43& old_transform, const matrix43& new_transform); // 0x0080E0B0 (:1819)
void havok_interpolate_object(object& obj, const matrix43& new_transform); // 0x0080E100 (:1825)
void havok_system_init(); // 0x00839A20 (:2181)
void havok_setup_thread_job(cpu_job* de_job, cpu_job_arg arg1, cpu_job_arg arg2); // 0x008294B0 (:3002)
void havok_system_shutdown(); // 0x0082CC90 (:3050)
void havok_level_init_pre(); // 0x0083CC80 (:3159)
void havok_level_init_post(); // 0x0080E2D0 (:3184)
void havok_level_shutdown(); // 0x0083CCE0 (:3190)
void havok_mark_for_write(); // 0x0080E2E0 (:3210)
void havok_unmark_for_write(); // 0x0080E2F0 (:3218)
void havok_mark_for_read(); // 0x0080E300 (:3225)
void havok_unmark_for_read(); // 0x0080E310 (:3232)
void havok_get_broadphase_extents(vector& bmin, vector& bmax); // 0x00822E00 (:3239)
unsigned int havok_create_navmesh_shape(const unsigned char* mopp_buf, bb_pfg_layer* pfg_data); // 0x00834BC0 (:3248)
unsigned int havok_create_navmesh_rb(unsigned int cm_index, obj_zone* zone, const vector& pos, const matrix& orient, bool vehicle_navmesh); // 0x0083F0D0 (:3274)
void havok_initialize_job_memory(int thread_num); // 0x00822ED0 (:3302)
bool havok_ok_to_use_thread(int thread_num); // 0x0080E320 (:3322)
unsigned int havok_num_scheduler_threads(); // 0x0080E340 (:3333)
void havok_simulate(); // 0x0083F280 (:3536)
bool havok_get_is_terminal_havok_step(); // 0x0080E350 (:3712)
bool havok_terrain_height(float& y, float x, float z, bool use_stitch_pieces, vector* norm); // 0x0083B3F0 (:3758)
bool havok_terrain_height(float& y, const vector& local_bmin, const vector& local_bmax, const vector& pos, const matrix& orient, bool use_simple_stitch_piece_test); // 0x0082CF90 (:3921)
void havok_body_free(unsigned int handle, bool force_activate, bool remove_from_world, bool batch_remove); // 0x00823110 (:4168)
void havok_phantom_release(hkpSimpleShapePhantom* handle); // 0x00823190 (:4199)
void havok_aabb_phantom_release(game_aabb_phantom* handle); // 0x008231C0 (:4210)
unsigned int havok_body_create(physics_create_info& pci); // 0x0083DBA0 (:4222)
hkpSimpleShapePhantom* havok_phantom_create(physics_create_info& pci); // 0x008231F0 (:4390)
void havok_phantom_set_position_and_orient(hkpSimpleShapePhantom* handle, const vector& pos, const matrix& orient); // 0x008169D0 (:4447)
void havok_phantom_set_collision_type(hkpPhantom* phantom, unsigned int collision_type, bool full_shape_check); // 0x00816A90 (:4458)
game_aabb_phantom* havok_aabb_phantom_create(const vector& bmin, const vector& bmax); // 0x00823500 (:4471)
void havok_aabb_phantom_update_aabb(game_aabb_phantom* handle, const vector& bmin, const vector& bmax); // 0x00816AD0 (:4485)
bool havok_has_property(hkpWorldObject* obj, unsigned int prop); // 0x00816B50 (:4498)
void havok_set_property(hkpWorldObject* obj, unsigned int prop, void* const value); // 0x00816B80 (:4504)
void havok_remove_property(hkpWorldObject* obj, unsigned int prop); // 0x00816BC0 (:4512)
void* havok_get_property(const hkpWorldObject* obj, unsigned int prop, void* default_value, bool optional); // 0x00816C10 (:4520)
bool havok_set_ignore_property(hkpRigidBody* rb, unsigned int ignore_handle); // 0x00816C60 (:4539)
bool havok_set_ignore_property(hkpPhantom* phantom, unsigned int ignore_handle); // 0x00816CB0 (:4550)
bool havok_remove_ignore_property(hkpRigidBody* rb); // 0x00816D00 (:4563)
bool havok_remove_ignore_property(hkpPhantom* phantom); // 0x00816D60 (:4574)
bool havok_has_ignore_property(hkpRigidBody* rb); // 0x00816DB0 (:4585)
bool havok_has_ignore_property(hkpPhantom* phantom); // 0x00816DE0 (:4590)
void havok_entity_update_collision_filter(hkpEntity* entity, bool full_entity_check, bool full_shape_check); // 0x0080E370 (:4602)
void havok_phantom_update_collision_filter(hkpPhantom* phantom, bool full_shape_check); // 0x0080E3A0 (:4624)
void havok_body_get_pos_orient(unsigned int handle, vector& pos, matrix& orient); // 0x00829530 (:4637)
void havok_body_get_pos_orient(hkpRigidBody* rb, vector& pos, matrix& orient); // 0x00823610 (:4643)
void havok_body_set_pos_orient(unsigned int handle, const vector& pos, const matrix& orient); // 0x00823700 (:4651)
void havok_body_set_pos_orient(hkpRigidBody* rb, const vector& pos, const matrix& orient); // 0x00816E10 (:4657)
void havok_body_set_position(unsigned int handle, const vector& pos); // 0x00816EA0 (:4685)
void havok_body_set_orient(unsigned int handle, const matrix& orient); // 0x00816F00 (:4698)
bool havok_body_get_aabb(unsigned int handle, const vector& pos, const matrix& orient, vector& bmin, vector& bmax); // 0x00829560 (:4716)
void havok_body_get_linear_velocity(unsigned int handle, vector& vel); // 0x008237A0 (:4738)
void havok_body_get_linear_velocity(hkpRigidBody* rb, vector& vel); // 0x00816FA0 (:4743)
void havok_body_set_linear_velocity(unsigned int handle, const vector& velocity); // 0x008237E0 (:4749)
void havok_body_set_linear_velocity(hkpRigidBody* rb, const vector& velocity); // 0x00816FD0 (:4754)
void havok_body_get_mass_props(unsigned int handle, float& mass, vector& center_of_mass, matrix& inertia_tensor); // 0x00823860 (:4765)
void havok_body_get_mass_props(hkpRigidBody* rb, float& mass, vector& center_of_mass, matrix& inertia_tensor); // 0x00817040 (:4769)
void havok_body_set_mass_props(hkpRigidBody* rb, float mass, const vector& center_of_mass, const matrix& inertia_tensor); // 0x00817170 (:4795)
void havok_body_set_mass_props(unsigned int handle, float mass, const vector& center_of_mass, const matrix& inertia_tensor); // 0x00817260 (:4812)
void havok_body_calc_mass_props(const vector& local_bmin, const vector& local_bmax, float mass, vector& center_of_mass, matrix& inertia_tensor); // 0x0080E3D0 (:4818)
void havok_body_calc_mass_props(const float radius, float mass, vector& center_of_mass, matrix& inertia_tensor); // 0x0080E4E0 (:4837)
void havok_body_calc_mass_props(const hkpShape* shape, float mass, vector& center_of_mass, matrix& inertia_tensor); // 0x008172A0 (:4852)
void havok_body_set_linear_dampening(unsigned int handle, float linear_dampening); // 0x00823890 (:4863)
void havok_body_set_linear_dampening(hkpRigidBody* rb, float linear_dampening); // 0x008173B0 (:4868)
void havok_body_set_angular_dampening(unsigned int handle, float angular_dampening); // 0x008238D0 (:4874)
void havok_body_set_angular_dampening(hkpRigidBody* rb, float angular_dampening); // 0x008173F0 (:4879)
void havok_body_set_angular_velocity(unsigned int handle, const vector& ang_velocity); // 0x00823910 (:4885)
void havok_body_set_angular_velocity(hkpRigidBody* rb, const vector& ang_velocity); // 0x00817430 (:4890)
float havok_body_get_linear_dampening(unsigned int handle); // 0x00823990 (:4901)
float havok_body_get_linear_dampening(hkpRigidBody* rb); // 0x008174A0 (:4906)
float havok_body_get_angular_dampening(unsigned int handle); // 0x008239D0 (:4912)
float havok_body_get_angular_dampening(hkpRigidBody* rb); // 0x008174C0 (:4917)
void havok_body_get_angular_velocity(hkpRigidBody* rb, vector& vel); // 0x008174E0 (:4923)
void havok_body_get_angular_velocity(unsigned int handle, vector& vel); // 0x00817510 (:4928)
void havok_body_get_point_velocity(unsigned int handle, const vector& point, vector& velocity); // 0x00823A10 (:4934)
void havok_body_get_point_velocity(hkpRigidBody* rb, const vector& point, vector& velocity); // 0x00817550 (:4938)
void havok_body_add_angular_velocity(unsigned int handle, vector& ang_velocity); // 0x008175F0 (:4950)
void havok_body_add_custom_force(unsigned int handle, const vector& accl); // 0x008296B0 (:4961)
void havok_body_add_custom_force(hkpRigidBody* rb, const vector& accl); // 0x00823AC0 (:4968)
void havok_body_remove_custom_force(hkpRigidBody* rb); // 0x00817680 (:4992)
void havok_body_remove_custom_force(unsigned int handle); // 0x00817710 (:5001)
void havok_body_force_activate(unsigned int handle); // 0x00817730 (:5008)
void havok_body_force_deactivate(unsigned int handle); // 0x00817750 (:5016)
bool havok_body_is_active(unsigned int handle); // 0x00817770 (:5024)
void havok_body_apply_linear_impulse(unsigned int handle, const vector& linear_impulse); // 0x00823C20 (:5031)
void havok_body_apply_linear_impulse(hkpRigidBody* rb, const vector& linear_impulse); // 0x008177A0 (:5036)
void havok_body_apply_angular_impulse(hkpRigidBody* rb, const vector& angular_impulse); // 0x00817810 (:5044)
void havok_body_apply_angular_impulse(unsigned int handle, const vector& angular_impulse); // 0x00817880 (:5051)
void havok_body_apply_point_impulse(unsigned int handle, const vector& point_impulse, const vector& pos); // 0x00823CA0 (:5057)
void havok_body_apply_point_impulse(hkpRigidBody* rb, const vector& point_impulse, const vector& pos); // 0x00817900 (:5062)
void havok_body_apply_linear_velocity(unsigned int handle, const vector& velocity); // 0x008296D0 (:5077)
void havok_body_apply_point_velocity(unsigned int handle, const vector& velocity, const vector& point); // 0x00829790 (:5098)
void havok_body_set_movable(hkpRigidBody* rb, bool movable); // 0x00823D40 (:5121)
void havok_body_set_movable(unsigned int handle, bool movable); // 0x00823DA0 (:5143)
void havok_body_apply_hard_keyframe(unsigned int handle, const matrix& next_rotation, const vector& next_position, float time_step); // 0x008179A0 (:5155)
void havok_body_set_motion_type(unsigned int handle, unsigned int type); // 0x00817A90 (:5175)
int havok_body_get_motion_type(hkpRigidBody* rb); // 0x00817AD0 (:5189)
int havok_body_get_motion_type(unsigned int handle); // 0x00817AF0 (:5194)
int havok_body_get_stored_motion_type(unsigned int handle); // 0x00817B10 (:5200)
bool havok_body_is_movable(hkpRigidBody* rb); // 0x00817B40 (:5212)
bool havok_body_is_movable(unsigned int handle); // 0x00817B60 (:5217)
void havok_body_set_collision_type(hkpRigidBody* rb, unsigned int collision_type, bool full_entity_check, bool full_shape_check); // 0x0080E550 (:5228)
void havok_body_set_collision_type(unsigned int handle, unsigned int collision_type, bool full_entity_check, bool full_shape_check); // 0x00817B90 (:5243)
unsigned int havok_body_get_collision_type(unsigned int handle); // 0x00817BE0 (:5249)
void havok_body_set_collision_quality(unsigned int handle, hkpCollidableQualityType collision_quality); // 0x00817C00 (:5262)
hkpCollidableQualityType havok_body_get_collision_quality(unsigned int handle); // 0x00817C30 (:5278)
void havok_body_enable_collision(hkpRigidBody* rigid_body, unsigned int* saved_collision_filter_ptr, bool enable); // 0x0080E590 (:5293)
void havok_body_replace_collision_model(unsigned int handle, unsigned int new_cm_index); // 0x00817C50 (:5309)
void havok_body_freeze(unsigned int handle); // 0x00817CC0 (:5332)
void havok_body_set_auto_remove_level(unsigned int handle, havok_auto_remove_level level); // 0x00817D40 (:5341)
void havok_body_set_auto_remove_level(hkpEntity* ep, havok_auto_remove_level level); // 0x0080E5E0 (:5347)
havok_auto_remove_level havok_body_get_auto_remove_level(unsigned int handle); // 0x00817D70 (:5355)
havok_auto_remove_level havok_body_get_auto_remove_level(const hkpEntity* ep); // 0x0080E600 (:5361)
hkpConstraintInstance* havok_constraint_create(hkpRigidBody* child_rb, hkpRigidBody* host_rb, hkpConstraintData* cd, float threshold, hkpConstraintInstance::ConstraintPriority priority); // 0x0083CD40 (:5392)
hkpConstraintInstance* havok_constraint_transfer(hkpConstraintInstance* ci, hkpRigidBody* child_rb, hkpRigidBody* host_rb); // 0x0083CE20 (:5421)
void havok_constraint_release(hkpConstraintInstance* ci); // 0x00817DE0 (:5491)
void havok_constraint_set_friction(hkpConstraintInstance* ci, float friction); // 0x0080E620 (:5500)
void havok_constraint_add_motor(hkpConstraintInstance* ci, float desired_vel, float max_force); // 0x00817E10 (:5530)
void havok_constraint_update_motor(hkpConstraintInstance* ci, float desired_vel, float max_force); // 0x0080E6C0 (:5582)
void havok_constraint_remove_motor(hkpConstraintInstance* ci); // 0x0080E760 (:5620)
bool havok_constraint_has_motor(hkpConstraintInstance* ci); // 0x0080E7F0 (:5650)
hkpConstraintInstance* havok_create_slide_constraint(unsigned int host_handle, unsigned int child_handle, const vector& axis, float min_limit, float max_limit, float threshold); // 0x0083D090 (:5691)
hkpConstraintInstance* havok_create_hinge_constraint(unsigned int host_handle, unsigned int child_handle, const vector& hinge_pos, const vector& hinge_axis, bool limited, float min_angle, float max_angle, float threshold); // 0x0083D1C0 (:5729)
hkpConstraintInstance* havok_create_point_constraint(unsigned int host_handle, unsigned int child_handle, const vector& pos, float threshold); // 0x0083D350 (:5774)
hkpConstraintInstance* havok_create_spring_constraint(unsigned int host_handle, unsigned int child_handle, const vector& pos_a, const vector& pos_b, float threshold); // 0x0083D450 (:5804)
hkpConstraintInstance* havok_create_path_constraint(unsigned int host_handle, unsigned int child_handle, const vector& pos, hkpParametricCurve* path, float threshold); // 0x0083D580 (:5835)
hkpConstraintInstance* havok_create_ragdoll_constraint(unsigned int host_handle, unsigned int child_handle, const ragdoll_constraint_info& rci, float min_planar_ang, float max_planar_ang, float threshold); // 0x0083D690 (:5864)
unsigned int havok_rigid_body_create(const hkpRigidBodyCinfo& cinfo, hkpRigidBody*& hk_rigid_body); // 0x00817FE0 (:5899)
hkpRigidBody* havok_body_get_pointer(unsigned int handle); // 0x0080E870 (:5922)
unsigned int havok_rigid_body_get_handle(const hkpRigidBody* hk_rigid_body); // 0x00818040 (:5973)
float havok_body_get_mass(unsigned int handle); // 0x00823DD0 (:5994)
float havok_body_get_mass(hkpRigidBody* rigid_body); // 0x008180A0 (:6005)
void havok_adjust_tensor(matrix& tensor, float min_diag, float min_axis); // 0x0080E890 (:6032)
const hkpShape* havok_body_get_shape(unsigned int handle); // 0x0080EA30 (:6052)
void havok_body_update_cached_shape_data(unsigned int handle); // 0x0080EA50 (:6065)
int havok_collect_objects_fast(const vector& bmin, const vector& bmax, unsigned int* objects, int max_objects, const unsigned int collector_type, bool (*collect_cb)(object*, unsigned int, const void*), const void* cb_user_data, collect_objects_filter_info* filter_info); // 0x00829880 (:6259)
int havok_collect_objects_fast(const vector& pos, float radius, unsigned int* objects, int max_objects, const unsigned int collector_type, bool (*collect_cb)(object*, unsigned int, const void*), const void* cb_user_data); // 0x00829AB0 (:6326)
int havok_collect_objects_fast(const vector& local_bmin, const vector& local_bmax, const vector& pos, const matrix& orient, unsigned int* objects, int max_objects, const unsigned int collector_type, bool (*collect_cb)(object*, unsigned int, const void*), const void* cb_user_data); // 0x00829BA0 (:6345)
int _havok_collect_objects_by_type_fast(const vector& bmin, const vector& bmax, int bpo_index, object_type type, object_sub_type sub_type, unsigned int* objects, int max_objects, bool (*collect_cb)(object*, unsigned int, const void*), const void* cb_user_data, collect_objects_filter_info* filter_info); // 0x00829D90 (:6366)
int _havok_collect_objects_by_type_fast(const vector& pos, float radius, int bpo_index, object_type type, object_sub_type sub_type, unsigned int* objects, int max_objects, bool (*collect_cb)(object*, unsigned int, const void*), const void* cb_user_data); // 0x00829FA0 (:6431)
int _havok_collect_objects_by_type_fast(const vector& local_bmin, const vector& local_bmax, const vector& pos, const matrix& orient, int bpo_index, object_type type, object_sub_type sub_type, unsigned int* objects, int max_objects, bool (*collect_cb)(object*, unsigned int, const void*), const void* cb_user_data); // 0x0082A0A0 (:6450)
int havok_collect_objects(const vector& pos, float radius, unsigned int* objects, int max_objects, const unsigned int collector_type, bool (*collect_cb)(object*, unsigned int, const void*), const void* cb_user_data); // 0x008183E0 (:6471)
int havok_collect_objects(const vector& bmin, const vector& bmax, const vector& pos, const matrix& orient, unsigned int* objects, int max_objects, const unsigned int collector_type, bool (*collect_cb)(object*, unsigned int, const void*), const void* cb_user_data); // 0x008184F0 (:6514)
int havok_collect_objects_in_capsule(raycast_input rin, unsigned int* objects, int max_objects, bool (*collect_cb)(object*, unsigned int, const void*)); // 0x00823F20 (:6565)
int havok_collect_objects_in_capsule(const vector& pos, const float& radius, const float& height, unsigned int* objects, int max_objects, const unsigned int collector_type, bool (*collect_cb)(object*, unsigned int, const void*), const void* cb_user_data); // 0x00823F60 (:6574)
int havok_collect_objects_in_capsule(const vector& pos1, const vector& pos2, const float& radius, unsigned int* objects, int max_objects, const unsigned int collector_type, bool (*collect_cb)(object*, unsigned int, const void*), const void* cb_user_data); // 0x008186B0 (:6584)
int havok_collect_nav_cells_old_and_slow(const vector& pos, float radius, nav_cell** nav_cells, int max_nav_cells, bool vehicle_pfg); // 0x00831DC0 (:6644)
int havok_collect_nav_cells(const vector& pos, float radius, nav_cell** nav_cells, int max_nav_cells, bool vehicle_pfg); // 0x0082D5F0 (:6708)
int havok_collect_nav_cells_old_and_slow(const vector& bmin, const vector& bmax, const vector& pos, const matrix& orient, nav_cell** nav_cells, int max_nav_cells, bool vehicle_pfg); // 0x00831FB0 (:6782)
int havok_collect_nav_cells(const vector& bmin, const vector& bmax, const vector& pos, const matrix& orient, nav_cell** nav_cells, int max_nav_cells, bool vehicle_pfg, void (*collect_callback)(nav_cell*)); // 0x0082D820 (:6860)
bool havok_is_box_empty_oriented(const vector& local_bmin, const vector& local_bmax, const vector& pos, const matrix& orient, const unsigned int& collision_type); // 0x00818850 (:6945)
bool havok_world_is_simulating(); // 0x0080EB60 (:6956)
bool havok_rigid_bodies_are_colliding(hkpRigidBody* ra, hkpRigidBody* rb); // 0x00818890 (:6962)
object* object_from_world_obj(const hkpWorldObject* wp); // 0x00818930 (:6990)
void havok_activate_region(const vector& pos, const matrix& orient, const vector& local_bmin, const vector& local_bmax); // 0x00818980 (:6996)
void havok_set_status(havok_system system, havok_system_status status); // 0x0080EB70 (:7015)
havok_system_status havok_get_status(havok_system system); // 0x0080EBF0 (:7036)

extern hkpWorld* Havok_world; // 0x02B82284, the game's Havok world (havok_level_init_pre)
extern bbox Havok_safe_broadphase; // 0x02B83D98, broadphase box shrunk to a safe margin (havok_system_init, recompute_safe_bp)

// 0x00A15F40: nothing to set
inline ragdoll_constraint_info::ragdoll_constraint_info()
{
}
