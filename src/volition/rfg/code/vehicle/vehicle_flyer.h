// Original: volition/rfg/code/vehicle/vehicle_flyer.h
// Implementation: volition/rfg/code/vehicle/vehicle_flyer.cpp
//
// flyer, an aircraft (hover, pitch and yaw control).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/action/hkpunaryaction.h"
#include "volition/rfg/code/objects/object.h"
#include "volition/rfg/code/objects/object_type.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/time/timestamp.h"

class flyer_control_action;
struct hkContainerHeapAllocator;
class hkStepInfo;
class hkpAction;
class hkpEntity;
class hkpPhantom;
class hkpRigidBody;

// size 0x41E8
class flyer : public vehicle
{
public:
    // size 0x1
    struct resource_dependent_data
    {
    };

    flyer(); // 0x00BA14C0 (:188)
    static const object_type get_type_static(); // 0x005DD590 (.h:29)
    static const object_sub_type get_subtype_static(); // 0x005DD5A0 (.h:29)
    static const char* get_classname_static(); // 0x00B630A0 (.h:29)
    static int get_bpo_index_static(); // 0x00B630B0 (.h:29)
    virtual const char* get_classname() const; // vtbl+0x0C 0x00BA1500 (.h:29)
    static unsigned int get_classname_hash(); // 0x00B66EA0 (:187)
    static int pool_remaining(); // 0x00B7E160 (:187)
    virtual void pool_free(); // vtbl+0x80 0x00B97EB0 (:187)
    virtual hkpRigidBody* get_supporting_rigid_body() const; // vtbl+0x88 0x00B66EB0 (:193)
    virtual bool serialize(obj_prop_block& pblock, bool load); // vtbl+0x14 0x00B8F250 (:213)
    virtual bool stream(bool load); // vtbl+0x1C 0x00BAAAE0 (:261)
    virtual float get_energy_scale(); // vtbl+0x64 0x00B66EC0 (:200)
    void kill_effects(); // 0x00B8F350 (:359)
    void start_effects();
    void engine_effects_start(); // 0x00B66ED0 (:380)
    void engine_effects_stop(); // 0x00B7E170 (:450)
    virtual void engine_start(bool is_player); // vtbl+0x8C 0x00B7E4B0 (:637)
    virtual void engine_stop(bool is_player, bool unused); // vtbl+0x90 0x00B7E4F0 (:652)
    const vector& get_com_ws(); // 0x00BC2E80 (.h:163)
    const vector& get_thrust_up_ws();
    const matrix& get_thrust_ws();
    const matrix& get_thrust_cs();
    void update_flight_info(); // 0x00B7E2F0 (:531)
    void set_request_urgency(float urg); // 0x00B67230 (:551)
    void set_request_max_velocity_mag(float max_vel); // 0x00B67280 (:556)
    void set_request_flyto_point(const vector& new_pt, bool stop_at); // 0x00B7E380 (:562)
    void set_request_flyto_direction(const vector& new_vec); // 0x00B7E3E0 (:571)
    void set_request_halt(); // 0x00B7E480 (:583)
    void clear_requested_movement(); // 0x00B672E0 (:589)
    bool is_movement_requested(); // 0x00B67310 (:599)
    void set_request_lookat_vector(const vector& new_vec); // 0x00B67330 (:606)
    void clear_requested_lookat_vector(); // 0x00B673D0 (:629)
    void flight_compute_physics(float step_time); // 0x00B7E800 (:1522)
    bool get_engine_on();
    void set_engine_on(bool);
    bool get_landing_gear_up(); // 0x00B673E0 (:681)
    void set_landing_gear_up(bool go_up); // 0x00B673F0 (:687)
    void suspend_engine_stop(); // 0x00B67210 (:441)
protected:
    virtual void free_this(); // vtbl+0x28 0x00BB3CD0 (:347)
public:
    virtual ~flyer(); // vtbl+0x00 0x00BA1510

    object_stream_resource<resource_dependent_data> f_rdd; // 0x40B8
    flyer_control_action* ctl_action; // 0x40BC
    float req_urgency; // 0x40C0
    float req_max_vel; // 0x40C4
    vector req_point; // 0x40C8
    vector req_vel; // 0x40D4
    vector req_lookat; // 0x40E0
    bool req_point_isvalid; // 0x40EC
    bool req_vel_isvalid; // 0x40ED
    bool req_lookat_isvalid; // 0x40EE
    bool req_stop_at_point; // 0x40EF
    bool req_halt; // 0x40F0
    bool wing_is_detached; // 0x40F1
    float vehicle_mass; // 0x40F4
    vector chassis_com_cs; // 0x40F8
    vector chassis_com_ws; // 0x4104
    matrix thrust_mat_cs; // 0x4110
    matrix thrust_mat_ws; // 0x4134
    vector thrust_up_target; // 0x4158
    vector hover_noise_vec; // 0x4164
    vector hover_noise_vec_new; // 0x4170
    vector hover_noise_vec_old; // 0x417C
    timestamp hover_noise_timer; // 0x4188
    float hover_noise_dur; // 0x418C
    timestamp engine_off_suspension_period; // 0x4190
    float turbine_rpm_target; // 0x4194
    float turbine_rpm; // 0x4198
    float turbine_load; // 0x419C
    int l_wing; // 0x41A0
    int r_wing; // 0x41A4
    int l_canard; // 0x41A8
    int r_canard; // 0x41AC
    unsigned int wingtip_effect[2]; // 0x41B0
    unsigned int front_thruster[2]; // 0x41B8
    unsigned int center_thruster[2]; // 0x41C0
    unsigned int main_engines[2]; // 0x41C8
    unsigned int jetwash; // 0x41D0
    float engine_force; // 0x41D4
    timestamp startup_timer; // 0x41D8
    timestamp last_player_damage; // 0x41DC
    timestamp catch_fire_timer; // 0x41E0
    struct
    {
        unsigned char landing_gear_up : 1; // 0x00:0
        unsigned char landing_gear_collision_off : 1; // 0x00:1
        unsigned char needs_to_takeoff : 1; // 0x00:2
        unsigned char has_lift : 1; // 0x00:3
        unsigned char stay_at_constant_height : 1; // 0x00:4
        unsigned char on_ground : 1; // 0x00:5
    } flyer_flags; // 0x41E4
protected:
    static const vector GRAVITY_VECTOR; // 0x016BA5E0
    static const vector ANTIGRAVITY_VECTOR; // 0x016BA5EC
};

// size 0xC
class obj_flyer_props : public obj_prop_block
{
public:
    obj_flyer_props(int, const vector&, const matrix&, bool);

};

// size 0x20
class flyer_control_action : public hkpUnaryAction
{
public:
    flyer_control_action(hkpRigidBody* rb, flyer* flyer_p); // 0x00B66E20 (:131)
    virtual void applyAction(const hkStepInfo& stepInfo); // vtbl+0x10 0x00B8F140 (:139)
    virtual hkpAction* clone(const hkArray<hkpEntity*,hkContainerHeapAllocator>& entities, const hkArray<hkpPhantom*,hkContainerHeapAllocator>& phantoms) const; // vtbl+0x20 0x00B7E0B0 (:125)
    virtual ~flyer_control_action(); // vtbl+0x00 0x00B66E50

private:
    flyer* fp; // 0x1C
};

object* alloc_flyer(); // 0x00B8F240 (:187)
int bpo_index_flyer(); // 0x00B66E80 (:187)
void types_flyer(signed char& type, signed char& subtype); // 0x00B66E70 (:187)
const char* classname_flyer(); // 0x00B66E90 (:187)
void flyer_system_init(); // 0x00B674D0 (:719)
void flyer_get_velocity(const flyer* fp, vector& vel); // 0x00B674E0 (:725)
void flyer_build(flyer* fp); // 0x00B7E550 (:734)
void flyer_steer(flyer* fp); // 0x00B675D0 (:802)
void flyer_compute_world_turst_matrix(const matrix& chassis_mat, const vector& down_thruster_up, matrix& thrust_mat_local, matrix& thrust_mat_world); // 0x00B675E0 (:881)
float flyer_compute_target_curve(float target_dist, float vel, float vel_max, float vel_delta_max, float frame_deltat); // 0x00B67790 (:912)
void flyer_compute_target_time_deltav(float vel, float deltav_max, float& time_to_stop, float& dist_to_stop); // 0x00B67AD0 (:1059)
void flyer_compute_target_time_startstop(float dist, float deltav_max, float vel_max, float& time_to_stop); // 0x00B67B30 (:1073)
float flyer_compute_target_time(float target_dist, float vel, float vel_max, float vel_delta_max); // 0x00B67C30 (:1314)
void flyer_do_frame_presim(flyer* fp); // 0x00B68490 (:2111)
void flyer_do_frame_postsim(flyer* fp); // 0x00BAAD30 (:2128)
void flyer_blow_up(flyer* fp); // 0x00B8F390 (:2522)
void flyer_set_controls_when_abandoned(flyer* fp); // 0x00B684A0 (:2559)

// 0x005DD590 (.h:29)
inline const object_type flyer::get_type_static()
{
    return OT_VEHICLE;
}

// 0x005DD5A0 (.h:29)
inline const object_sub_type flyer::get_subtype_static()
{
    return OT_SUB_VEHICLE_FLYER;
}
