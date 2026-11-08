// Original: volition/rfg/code/havok/api/hkcharactercontroller.h
// Implementation: volition/rfg/code/havok/api/hkcharactercontroller.cpp
//
// The character controller of a human (human resource data): a Havok character proxy with a phantom whose shape
// depends on the stance (controller_shape_mode: standing, crouching, dead...). do_frame moves it with the desired
// velocity, gravity and jump; the mt_integrate_* steps run the proxies of all humans together
// (human_process_controllers). phantom_overlap_listener collects what the phantom overlaps.
// Placement of controller_shape_mode and phantom_overlap_listener inferred.
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4f.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hkreferencedobject.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/phantom/hkpphantomoverlaplistener.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/utilities/charactercontrol/characterproxy/hkpcharacterproxylistener.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/time/timestamp.h"

class MyCharacterListener;
class hkContactPoint;
struct hkContainerHeapAllocator;
struct hkSimplexSolverInput;
class hk_custom_character_proxy;
struct hkpCharacterObjectInteractionEvent;
struct hkpCharacterObjectInteractionResult;
class hkpCharacterProxy;
struct hkpCollidableAddedEvent;
struct hkpCollidableRemovedEvent;
struct hkpRootCdPoint;
class hkpShape;
class hkpShapePhantom;
class human;
class matrix;
class object;
class raycast_input;
class raycast_output;

enum controller_shape_mode
{
    CSM_STAND = 0,
    CSM_CROUCH = 1,
    CSM_PRONE = 2,
    CSM_STAND_NARROW = 3,
    CSM_CROUCH_NARROW = 4,
    CSM_VEHICLE = 5,
    CSM_DIVE_THROUGH_HOLE = 6,
    CSM_JUMP = 7,
    NUM_STANDARD_CONTROLLER_SHAPE_MODES = 8,
    CSM_DEAD = 8,
    NUM_CONTROLLER_SHAPE_MODES = 9,
};

// size 0x64
class character_controller
{
public:
    character_controller(unsigned int object_handle, unsigned int collide_filter); // 0x00837950 (:1522)
    ~character_controller(); // 0x008291B0 (:1606)
    void do_frame(human* hp, vector& desired_vel, float jump_height, float delta_time, bool check_support, bool use_velocity, bool use_velocity_ignore_y); // 0x0083D940 (:1645)
    void do_frame_internal(human* hp, vector& desired_vel, float jump_height, float delta_time, bool check_support, bool use_velocity, bool use_velocity_ignore_y); // 0x0083CB10 (:1672)
    void set_pos(const vector& pos); // 0x008153D0 (:1368)
    void get_pos(vector& pos); // 0x00815430 (:1375)
    void set_velocity(const vector& vel); // 0x008154C0 (:1388)
    void get_velocity(vector& vel); // 0x00815460 (:1381)
    void set_shape(controller_shape_mode which, bool forced); // 0x00834920 (:1400)
    void get_capsule_data(vector& bottom, vector& top, float& radius); // 0x00815520 (:1409)
    void update_collision_shape_mode(const object* objp); // 0x00834950 (:1442)
    bool swapPhantomShape(hkpShape* newShape, bool forced); // 0x008314C0 (:2374)
    bool using_dead_shape(); // 0x0080D6A0 (:1434)
    void update_object_handle(unsigned int);
    bool cast_ray(const raycast_input& input, raycast_output& output); // 0x00839840 (:2449)
    void update_state_for_integration(human* hp, vector& desired_vel, float jump_height, float delta_time, bool check_support, bool use_velocity, bool use_velocity_ignore_y); // 0x00837D90 (:1705)
    void update_broadphase_for_integration(float time_step); // 0x00829260 (:2224)
    void mt_integrate_begin(float delta_time); // 0x008314A0 (:2262)
    void mt_integrate_process_casts(float time_step); // 0x008397A0 (:2272)
    void mt_integrate_further_casts(hkArray<human*,hkContainerHeapAllocator>& humans_to_integrate, human* p_human, bool last_iteration); // 0x00834A20 (:2294)
    void mt_push_objects(); // 0x0082CC00 (:2308)
    void mt_integrate_finish(); // 0x0082CC10 (:2315)
    void post_integrate(human* hp); // 0x00822240 (:2337)
private:
    void update_contacted_objects();
    void init_dead_shape(); // 0x00814E90 (:1110)
    void update_dead_shape(const vector& bmin, const vector& bmax, const matrix& orient); // 0x00815660 (:1477)

public:
    hk_custom_character_proxy* m_characterProxy; // 0x00
    float m_currentAngle; // 0x04
    hkpShapePhantom* m_phantom; // 0x08
    controller_shape_mode m_controller_shape_mode; // 0x0C
    hkpShape* m_controller_shape[9]; // 0x10
    MyCharacterListener* m_listener; // 0x34
    struct
    {
        unsigned int disable_gravity : 1; // 0x00:0
        unsigned int big_step : 1; // 0x00:1
        unsigned int is_supported : 1; // 0x00:2
        unsigned int sliding : 1; // 0x00:3
        unsigned int flat_surface_ahead : 1; // 0x00:4
        unsigned int no_surface_ahead : 1; // 0x00:5
        unsigned int near_big_step : 1; // 0x00:6
        unsigned int check_ragdoll : 1; // 0x00:7
        unsigned int is_pushed : 1; // 0x00:8
        unsigned int phantom_added_to_world : 1; // 0x00:9
    } flags; // 0x38
    timestamp sliding_buildup; // 0x3C
    timestamp stepdown_ts; // 0x40
    timestamp supported_ts; // 0x44
    vector pushed_vec; // 0x48
    float last_frame_vertical; // 0x54
    int human_is_potentially_stuck; // 0x58
    unsigned int m_saved_collision_filter; // 0x5C
private:
    float m_frame_time_left; // 0x60
};

// size 0x4
class phantom_overlap_listener : public hkpPhantomOverlapListener
{
    virtual void collidableRemovedCallback(const hkpCollidableRemovedEvent& remove_event); // vtbl+0x04 0x00814E50 (:950)
    virtual void collidableAddedCallback(const hkpCollidableAddedEvent& added_event); // vtbl+0x00 0x00828F80 (:954)
public:
    phantom_overlap_listener(); // 0x00814E40
    virtual ~phantom_overlap_listener(); // vtbl+0x08 0x0080D680

};

// size 0x30
class MyCharacterListener : public hkReferencedObject, public hkpCharacterProxyListener
{
public:
    struct MustEndWithSemiColon; // no definition in PDB

    static void* operator new(unsigned int, void*);
    static void* operator new(unsigned int nbytes); // 0x00814C20 (:154)
    static void operator delete(void*, void*);
    static void operator delete(void* p, unsigned int nbytes); // 0x00814DB0 (:154)
    static void* operator new[](unsigned int, void*);
    static void operator delete[](void*, void*);
    MyCharacterListener(const hkVector4f& up, const float maxSlope, unsigned int obj_handle); // 0x00814C40 (:156)
    virtual void characterInteractionCallback(hkpCharacterProxy* proxy, hkpCharacterProxy* other_proxy, const hkContactPoint& contact); // vtbl+0x10 0x00821E70 (:182)
    virtual void objectInteractionCallback(hkpCharacterProxy* proxy, const hkpCharacterObjectInteractionEvent& input, hkpCharacterObjectInteractionResult& output); // vtbl+0x14 0x00827BB0 (:258)
    virtual void processConstraintsCallback(const hkpCharacterProxy* proxy, const hkArray<hkpRootCdPoint,hkContainerHeapAllocator>& manifold, hkSimplexSolverInput& input); // vtbl+0x04 0x0082C600 (:585)
    virtual ~MyCharacterListener(); // vtbl+0x00 0x00814D90

private:
    hkVector4f m_up; // 0x10
    unsigned int m_obj_handle; // 0x20
    float m_maxSlope; // 0x24
};

void character_controller_system_init(); // 0x00815800 (:2486)
void character_controller_system_shutdown(); // 0x00815900 (:2510)
bool character_controller_should_push_away(object* op, bool pushes_all_debris); // 0x00822310 (:2534)
