// Original: volition/rfg/code/vehicle/vehicle_class.h
// Implementation: volition/rfg/code/vehicle/vehicle_class.cpp
//
// vehicle, the base of automobiles, flyers and walkers (object type OT_VEHICLE): its vehicle_info, components, seats,
// lights, turret mounts, damage areas and pending damage events, driver AI (m_ai), interaction (m_vi) and flags.
// vehicle_do_frame_presim / _postsim run once per vehicle around the Havok step (driven by vehicle_system.cpp).
// Placement of the small types inferred.
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4f.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/action/hkpunaryaction.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/level_object_properties_shared.h"
#include "volition/rfg/code/objects/obj_enums.h"
#include "volition/rfg/code/objects/object.h"
#include "volition/rfg/code/objects/object_render_distance.h"
#include "volition/rfg/code/objects/object_type.h"
#include "volition/rfg/code/vehicle/vehicle_component.h"
#include "volition/rfg/code/vehicle/vehicle_damage_area.h"
#include "volition/rfg/code/vehicle/vehicle_info.h"
#include "volition/rfg/code/vehicle/vehicle_interaction.h"
#include "volition/rfg/code/vehicle/vehicle_shared_defines.h"
#include "volition/rfg/code/vehicle_ai/vehicle_ai.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/os/cs_wrapper.h"
#include "volition/vlib/code/time/timestamp.h"
#include "volition/vlib/code/util/varray.h"
#include <stdint.h>

class anim_rig;
struct explosion_info;
struct havok_vehicle_ai_info;
struct hkContainerHeapAllocator;
class hkStepInfo;
class hkpAction;
class hkpEntity;
class hkpPhantom;
class hkpRigidBody;
class hkpWorldObject;
class human;
struct mesh_deform_sphere_link;
class rl_deformable_mesh_instance;
class rl_renderer;
class turret;
struct v_animate;
class vehicle;
struct vehicle_mesh_light;
struct vehicle_spline_render_update;
class vehicle_subinfo;
struct weapon_fire_info;
struct weapon_info;

enum vehicle_bomb_status
{
    VEHICLE_BOMB_NONE = 0,
    VEHICLE_BOMB_ARMED = 1,
};

enum vehicle_light_type
{
    VLT_INVALID = -1,
    VLT_HEAD_LIGHT = 0,
    VLT_FOG_LIGHT = 1,
    VLT_TAIL_LIGHT = 2,
    VLT_BRAKE_LIGHT = 3,
    VLT_REVERSE_LIGHT = 4,
    VLT_EMERGENCY_LIGHT = 5,
    VLT_STROBE_LIGHT = 6,
    NUM_VEHICLE_LIGHT_TYPES = 7,
};

// size 0x8
struct vehicle_flags
{
    void reset(); // 0x00753DD0 (.h:322)

    unsigned int activity : 1; // 0x00:0
    unsigned int old_activity : 1; // 0x00:1
    unsigned int activity_destroyed : 1; // 0x00:2
    unsigned int convoy_vehicle : 1; // 0x00:3
    unsigned int ambient_spawn : 1; // 0x00:4
    unsigned int death_reported : 1; // 0x00:5
    unsigned int no_ragdoll : 1; // 0x00:6
    unsigned int despawned_squad_vehicle : 1; // 0x00:7
    unsigned int delete_mission_vehicle : 1; // 0x00:8
    unsigned int do_not_destroy_with_squad : 1; // 0x00:9
    unsigned int m_corpse : 1; // 0x00:10
    unsigned int m_despawn_after_fade : 1; // 0x00:11
    unsigned int m_door_collision_enabled : 1; // 0x00:12
    unsigned int m_engine_running : 1; // 0x00:13
    unsigned int m_engine_smoking : 1; // 0x00:14
    unsigned int m_fading_in : 1; // 0x00:15
    unsigned int m_fading_out : 1; // 0x00:16
    unsigned int m_fading_from_nano : 1; // 0x00:17
    unsigned int m_hidden : 1; // 0x00:18
    unsigned int m_hijacked : 1; // 0x00:19
    unsigned int m_scripted_bail : 1; // 0x00:20
    unsigned int m_scripted_abandon : 1; // 0x00:21
    unsigned int m_horn_down : 1; // 0x00:22
    unsigned int m_invulnerable : 1; // 0x00:23
    unsigned int m_mission_invulnerable : 1; // 0x00:24
    unsigned int m_is_player_car : 1; // 0x00:25
    unsigned int m_kill_by_vehicle : 1; // 0x00:26
    unsigned int m_mp_honked_horn : 1; // 0x00:27
    unsigned int m_needs_repair : 1; // 0x00:28
    unsigned int m_occupied : 1; // 0x00:29
    unsigned int m_on_fire : 1; // 0x00:30
    unsigned int m_nano_sim_fire : 1; // 0x00:31
    unsigned int m_nano_sim_smoke : 1; // 0x04:0
    unsigned int m_grenade_inside : 1; // 0x04:1
    unsigned int m_released_from_cutscene : 1; // 0x04:2
    unsigned int m_resource_access_disabled : 1; // 0x04:3
    unsigned int m_reverse_last_frame : 1; // 0x04:4
    unsigned int m_series_of_honks : 1; // 0x04:5
    unsigned int m_siren_dying : 1; // 0x04:6
    unsigned int m_siren_audio_on : 1; // 0x04:7
    unsigned int m_using_horn : 1; // 0x04:8
    unsigned int pass_by_getting_closer : 1; // 0x04:9
    unsigned int m_wheels_on_ground : 1; // 0x04:10
    unsigned int m_all_wheels_on_ground : 1; // 0x04:11
    unsigned int m_only_fire_damage : 1; // 0x04:12
    unsigned int m_headlights_on : 1; // 0x04:13
    unsigned int m_emergency_lights_on : 1; // 0x04:14
    unsigned int high_priority_target : 1; // 0x04:15
    unsigned int player_may_capture : 1; // 0x04:16
    unsigned int disable_for_npc_drivers : 1; // 0x04:17
    unsigned int disable_for_player : 1; // 0x04:18
    unsigned int disable_accelerator : 1; // 0x04:19
    unsigned int locked_in_place : 1; // 0x04:20
    unsigned int fixed_motion : 1; // 0x04:21
    unsigned int disable_turrets_for_npcs : 1; // 0x04:22
    unsigned int immediate_spawn : 1; // 0x04:23
    unsigned int no_player_use : 1; // 0x04:24
    unsigned int invisible_driver : 1; // 0x04:25
};

// size 0x4
struct vehicle_spawn_flags
{
    unsigned int high_priority : 1; // 0x00:0
    unsigned int mobile_spawn : 1; // 0x00:1
    unsigned int static_placement : 1; // 0x00:2
    unsigned int snap_to_ground : 1; // 0x00:3
    unsigned int road_spawn : 1; // 0x00:4
};

// size 0x4
struct vehicle_seat_info
{
    unsigned int m_current_occupant_handle; // 0x00
};

// size 0x8
struct vehicle_turret_mount
{
    const vehicle_info::turret_mount_info* info; // 0x00
    unsigned int attached_turret; // 0x04
};

// size 0x38
struct vehicle_light
{
    // size 0x1
    struct vehicle_light_flags
    {
        bool is_on : 1; // 0x00:0
        bool is_broken : 1; // 0x00:1
        bool broken : 1; // 0x00:2
        bool dirty : 1; // 0x00:3
    };

    void reset(); // 0x00B64590 (:199)
    void start_coronas(unsigned int info_one, unsigned int info_two, vehicle* vp); // 0x00B76670 (:227)
    void stop_all_effects(); // 0x00B767D0 (:278)

    int tag_index; // 0x00
    unsigned int effect_handle; // 0x04
    unsigned int coronas[3]; // 0x08
    unsigned int corona_effect_info_handle; // 0x14
    int parent_idx; // 0x18
    vehicle_mesh_light* mesh_light; // 0x1C
    vehicle_light_type type; // 0x20
    int mesh_index_on; // 0x24
    int mesh_index_off; // 0x28
    int component_mesh_index_on; // 0x2C
    int component_mesh_index_off; // 0x30
    vehicle_light_flags flags; // 0x34
};

// size 0x3C
struct vehicle_damage_event
{
    vehicle_damage_event(); // 0x00B645C0

    unsigned int attacker_handle; // 0x00
    const weapon_info* attacker_weapon; // 0x04
    vector damage_position; // 0x08
    vector damage_normal; // 0x14
    vector damage_velocity; // 0x20
    float damage_radius; // 0x2C
    int damage_to_apply; // 0x30
    damaged_by_types from_type; // 0x34
    bool from_server; // 0x38
    bool damage_velocity_valid; // 0x39
};

// size 0x40B8
class vehicle : public object
{
public:
    // mission_clean_invlunerable_flags (inlined).
    void clear_mission_invulnerable()
    {
        if (m_flags.m_mission_invulnerable)
        {
            m_flags.m_invulnerable = 0;
            m_flags.m_mission_invulnerable = 0;
        }
    }
public:
    // size 0x1
    struct rdd_flags
    {
        unsigned char m_pos_on_ground_for_create : 1; // 0x00:0
        unsigned char m_hide_vehicle : 1; // 0x00:1
    };

    // size 0x1
    struct collision_events
    {
        unsigned char m_collided_with_world : 1; // 0x00:0
        unsigned char m_collided_with_vehicle : 1; // 0x00:1
        unsigned char m_collided_with_mover : 1; // 0x00:2
    };

    // size 0x10
    struct vehicle_navcell_info
    {
        vehicle_navcell_info(); // 0x00B645D0

        uint64_t last_visited_nav_cell_handle; // 0x00
        timestamp last_visited_nav_cell_update_timer; // 0x08
    };

    // size 0x8
    struct vehicle_spline_data
    {
        float starting_offset_percent; // 0x00
        int num_treads; // 0x04
    };

    // size 0x3470
    struct resource_dependent_data
    {
        resource_dependent_data(); // 0x00B76850

        vehicle_subinfo* sub_info; // 0x00
        unsigned int m_num_components; // 0x04
        vehicle_component m_components[66]; // 0x08
        vehicle_spline_data spline_data[4]; // 0x2D68
        vehicle_spline_render_update* spline_render_update; // 0x2D88
        unsigned int spline_debris[80]; // 0x2D8C
        int track_pad_component_id; // 0x2ECC
        unsigned int m_wheel_bumper_component; // 0x2ED0
        unsigned int m_chassis_collision_component; // 0x2ED4
        unsigned int m_num_vehicle_lights; // 0x2ED8
        vehicle_light m_vehicle_lights[20]; // 0x2EDC
        unsigned int m_saved_collision_filter; // 0x333C
        rl_deformable_mesh_instance* m_renderable_instance_p; // 0x3340
        anim_rig* rig; // 0x3344
        int component_bone_index[74]; // 0x3348
    };

    static const object_type get_type_static(); // 0x005DCD10 (.h:349)
    static const object_sub_type get_subtype_static(); // 0x005DCD20 (.h:349)
    static const char* get_classname_static();
    static int get_bpo_index_static(); // 0x00753DE0 (.h:349)
    virtual const char* get_classname() const; // vtbl+0x0C 0x00B9DAB0 (.h:349)
    virtual void pool_free(); // vtbl+0x80 0x00B9DAC0 (.h:349)
    vehicle(); // 0x00B9D9A0 (:536)
    void vehicle_cover_process(); // 0x00B97350 (:4451)
    void vehicle_cover_destroy_nodes(); // 0x00B77410 (:4442)
    bool created_from_managed_layer() const; // 0x00B65DA0 (:10823)
    virtual void update_pos_and_orient(const vector& update_pos, const matrix& update_orient, bool set_havok_data); // vtbl+0x30 0x00BB3AC0 (:10835)
    virtual void update_render(rl_renderer*); // vtbl+0x44 0x00B9DAF0 (:10896)
    float get_vehicle_opacity() const; // 0x00B65D00 (:10796)
    bool initialize_on_create(vehicle_info* vip, const vector& pos_in, const matrix& orient_in, bool pos_at_ground); // 0x00B64A90 (:2670)
    void component_detach(vehicle_component* component, bool use_impact_sound, unsigned int delay_before_first_impact_sound, damaged_by_types from_type, bool explode, bool from_nano); // 0x00B9EDD0 (vehicle_component.cpp:2837)
    float get_ramming_damage_percentage() const; // 0x00B64A50 (:2623)
    float get_electrical_damage_percentage() const; // 0x00B63030 (.h:649)
    void process_electrical_damage(float damage, human* attacker); // 0x00BB3890 (:2637)
    bool is_occupied() const; // 0x00753DF0 (.h:653)
    bool is_corpse() const; // 0x005DCD30 (.h:656)
    bool is_on_fire() const; // 0x005DCD40 (.h:659)
    virtual bool is_killed() const; // vtbl+0x10 0x00B64AA0 (:2884)
    virtual void get_bbox(vector& bmin, vector& bmax) const; // vtbl+0x04 0x00B76FC0 (:2912)
    virtual void get_local_bbox(vector& bmin, vector& bmax) const; // vtbl+0x08 0x00B770E0 (:2934)
    virtual void get_local_bbox_slow(vector& bmin, vector& bmax) const; // vtbl+0x84 0x00B77140 (:2956)
    virtual int attach_index(const vector& posp, unsigned int parent_objp_body_handle); // vtbl+0x50 0x00B8B870 (:2993)
    virtual bool attach_index_is_valid(int index); // vtbl+0x54 0x00B64AB0 (:3024)
    virtual unsigned char get_physical_material(unsigned int shape_key, const vector* contact_point, const hkpWorldObject* hk_objp) const; // vtbl+0x5C 0x00B972C0 (:3062)
    virtual unsigned char get_effect_material(unsigned int shape_key, const vector* contact_point, const hkpWorldObject* hk_objp) const; // vtbl+0x60 0x00B64B10 (:3090)
    virtual float get_energy_scale(); // vtbl+0x64 0x00B64B40 (:3097)
    void start_nano_rifle_fade_out(bool death_full_fade, human* attacker, vector world_hit_pos); // 0x00BB6190 (:5777)
    virtual void process_bullet_hit(human* attacker, float damage, const weapon_fire_info& fire_info); // vtbl+0x68 0x00BC27A0 (:5834)
    virtual bool process_melee_hit(object* attacker, float damage, const vector& impulse, const vector& hit_pos, const vector& hit_normal, unsigned int shape_key, const weapon_info& w_info); // vtbl+0x6C 0x00BC2920 (:5873)
    virtual hkpRigidBody* get_supporting_rigid_body() const; // vtbl+0x88 0x00B64B60 (:3103)
    virtual bool get_tag_world_coords(int tag_index, vector& tag_pos, matrix* tag_orient, vector* obj_pos, matrix* obj_orient) const; // vtbl+0x38 0x00B65480 (:7008)
    virtual bool get_tag_local_coords(int tag_index, vector& tag_pos, matrix* tag_orient) const; // vtbl+0x3C 0x00B78880 (:6822)
    virtual bool serialize(obj_prop_block& pblock, bool load); // vtbl+0x14 0x00B8B8C0 (:3117)
    virtual obj_serialize_mode serialize_mode(); // vtbl+0x18 0x00B64B70 (:3361)
    virtual bool stream(bool load); // vtbl+0x1C 0x00BA8DE0 (:3373)
    virtual bool ready_to_stream_out(); // vtbl+0x74 0x00B65DD0 (:11135)
protected:
    virtual void free_this(); // vtbl+0x28 0x00BB3A00 (:4287)
public:
    virtual void destroy_notify(); // vtbl+0x2C 0x00BB3950 (:4229)
    bool not_allowed_to_ragdoll() const; // 0x00B64C70 (:4995)
    bool in_vehicle(human* hp) const; // 0x00B64C80 (:5001)
    bool is_offscreen() const; // 0x00B64CA0 (:5007)
    bool current_activity_spawned() const;
    int get_num_tags() const; // 0x00B78300 (:6426)
    virtual int get_tag_index_from_name(const char* tag_name) const; // vtbl+0x34 0x00B8BFD0 (:6460)
    int get_component_tag_index_from_name(int component_idx, const char* tag_name) const; // 0x00B783B0 (:6521)
    int get_nth_tag_index_from_name(const char* tag_name, int num_tag) const; // 0x00B78450 (:6548)
    int get_nth_tag_index(int index) const; // 0x00B78630 (:6667)
    int get_component_index_from_name(const char* comp_name, int start_from) const; // 0x00B78800 (:6760)
    int get_component_index_from_tag_index(int tag_index) const; // 0x00B65410 (:6781)
    int get_light_index_from_tag_index(int tag_index) const; // 0x00B65440 (:6796)
    float get_vehicle_damage() const; // 0x00B65300 (:6401)
    float get_reported_vehicle_damage() const; // 0x00B65380 (:6414)
    void update_last_damage_reported(); // 0x00B65390 (:6420)
    bool is_ambient() const; // 0x00B8B820 (:2890)
    virtual unsigned int get_alt_hk_body_handle(unsigned int index) const; // vtbl+0x78 0x00B77460 (:4579)
    virtual unsigned int get_alt_hk_body_handle_index(unsigned int alt_handle) const; // vtbl+0x7C 0x00B774B0 (:4614)
    unsigned int get_component_idx_from_alt_body_index(unsigned int alt_index) const; // 0x00B77510 (:4646)
    virtual void engine_start(bool player); // vtbl+0x8C 0x00B77540 (:4677)
    virtual void engine_stop(bool player, bool streamout); // vtbl+0x90 0x00B77690 (:4729)
    void check_for_vehicle_intersection(); // 0x00B64B80 (:4185)
    bool stream_area_clear(const vector&);
    void move_streaming_vehicle();
    bool move_streaming_vehicle_road();
    bool get_infinite_mass() const; // 0x0080D300 (.h:786)
    void set_infinite_mass(bool set); // 0x00B773D0 (:4353)
    bool get_extra_mass() const; // 0x0080D310 (.h:789)
    void set_extra_mass(bool set); // 0x00753E00 (.h:790)
    float get_extra_mass_value() const; // 0x0080D320 (.h:791)
    void set_extra_mass_value(float set); // 0x00753E10 (.h:792)
    void disable_turret_fov_limits(); // 0x00B7A400 (:11163)
private:
    void clean_up_debris(); // 0x00B77310 (:4198)
public:
    virtual ~vehicle(); // vtbl+0x00 0x00B9C110

    static const int c_lod_distance_max; // 0x012C70AC
    static lod_info ms_lod_distances[4]; // 0x016BA4B8
    object_stream_resource<resource_dependent_data> rdd; // 0x9C
    object_spawn_priority spawn_priority; // 0x3510
    const vehicle_info* info; // 0x3514
    vector last_pos; // 0x3518
    matrix last_orient; // 0x3524
    vector last_velocity; // 0x3548
    vector last_angular_velocity; // 0x3554
    float forward_velocity; // 0x3560
    float last_forward_velocity; // 0x3564
    vehicle_ai vai; // 0x3568
    vi_instance vii; // 0x38A0
    int aim_handle; // 0x3CC8
    v_animate* vp; // 0x3CCC
    matrix43 last_anim_transform; // 0x3CD0
    timestamp pf_failure_timeout; // 0x3D00
    timestamp disable_for_npc_drivers_timer; // 0x3D04
    timestamp no_npc_entry_timer; // 0x3D08
    object_render_distance render_distance; // 0x3D0C
    vehicle_seat_info m_seat_info[11]; // 0x3D14
    vehicle_damage_area m_damage_areas[1]; // 0x3D40
    int m_num_turret_mounts; // 0x3D54
    vehicle_turret_mount m_turret_mounts[2]; // 0x3D58
    vehicle_bomb_status m_bomb_status; // 0x3D68
    collision_events m_collisions_this_frame; // 0x3D6C
    timestamp m_walker_velocity_hack; // 0x3D70
    timestamp m_walker_velocity_hack_followup; // 0x3D74
    timestamp m_walker_thrown_energy_scaled; // 0x3D78
    float saved_angular_dampening; // 0x3D7C
    int walker_vehicle_collisions; // 0x3D80
    timestamp m_vehicle_vs_building_damage; // 0x3D84
    vehicle_damage_event damage_events[5]; // 0x3D88
    int num_damage_events; // 0x3EB4
    cs_wrapper damage_events_cs; // 0x3EB8
    timestamp m_corpse_timer; // 0x3EF4
    timestamp m_corpse_absolute_longest_timer; // 0x3EF8
    timestamp on_fire_timestamp; // 0x3EFC
    unsigned int m_corpse_effect; // 0x3F00
    unsigned int m_exhaust_effect_normal[4]; // 0x3F04
    unsigned int m_exhaust_effect_burst[4]; // 0x3F14
    vehicle_flags m_flags; // 0x3F24
    int m_turret_autofire_ms; // 0x3F2C
    unsigned int m_num_sub_pieces; // 0x3F30
    vehicle_spawn_flags stream_placement_flags; // 0x3F34
    rdd_flags m_rdd_flags; // 0x3F38
    unsigned int m_killer_handle; // 0x3F3C
    unsigned int most_recent_driver; // 0x3F40
    human_teams team_of_most_recent_driver; // 0x3F44
    timestamp most_recent_driver_exit_time; // 0x3F48
    timestamp damage_sound_propagate_timer; // 0x3F4C
    const weapon_info* m_killer_weapon; // 0x3F50
    int ramming_damage_taken; // 0x3F54
    float m_electrical_damage_percent; // 0x3F58
    float damage_percent; // 0x3F5C
    unsigned short damage_func_handle; // 0x3F60
    unsigned short destroy_func_handle; // 0x3F62
    unsigned short on_enter_func_handle; // 0x3F64
    unsigned short on_exit_func_handle; // 0x3F66
    unsigned short on_take_damage_handle; // 0x3F68
    unsigned short on_collision_handle; // 0x3F6A
    unsigned short on_hit_ped_handle; // 0x3F6C
    unsigned int reserved_by; // 0x3F70
    timestamp m_sound_delay_after_explosion; // 0x3F74
    float m_fire_damage_rate; // 0x3F78
    float m_fire_fractional_damage; // 0x3F7C
    timestamp m_fade_timer; // 0x3F80
    int m_fade_time; // 0x3F84
    unsigned int m_num_mesh_deform_spheres; // 0x3F88
    mesh_deform_sphere_link* m_mesh_deform_sphere_list; // 0x3F8C
    mesh_deform_sphere_link* m_mesh_deform_pending_list; // 0x3F90
    unsigned char m_tag_deform_weights[32]; // 0x3F94
    vehicle_navcell_info m_navcell_info; // 0x3FB8
    timestamp emergency_light_timer; // 0x3FC8
    float stream_load_distance_sqr; // 0x3FCC
    float stream_unload_distance_sqr; // 0x3FD0
    int engine_inst; // 0x3FD4
    int engine_started_inst; // 0x3FD8
    timestamp engine_high_load_start_timer; // 0x3FDC
    int pass_by_play_id; // 0x3FE0
    float pass_by_distance; // 0x3FE4
    unsigned int m_nav_cell_detour_request_handle; // 0x3FE8
    unsigned int m_nav_cell_detour_component_request_handles[8]; // 0x3FEC
    unsigned int m_num_nav_cell_detour_component_request_handles; // 0x400C
    float last_damage_reported; // 0x4010
    unsigned int spawn_node_handle; // 0x4014
    unsigned int squad_handle; // 0x4018
    timestamp vehicle_check_cover_timestamp; // 0x401C
    timestamp vehicle_cover_timestamp; // 0x4020
    vector vehicle_cover_creation_pos; // 0x4024
    farray<unsigned int,10> vehicle_cover_nodes; // 0x4030
    int vehicle_check_cover_index; // 0x4068
    int vehicle_cover_index; // 0x406C
    cs_wrapper component_rinstance_cs; // 0x4070
private:
    bool infinite_mass; // 0x40AC
    bool extra_mass; // 0x40AD
    float extra_mass_value; // 0x40B0
};

// size 0x30
class vehicle_antigravity : public hkpUnaryAction
{
public:
    vehicle_antigravity(hkpRigidBody* rb, const vector& acceleration); // 0x00B64640 (:558)
    virtual void applyAction(const hkStepInfo& stepInfo); // vtbl+0x10 0x00B769B0 (:563)
    virtual hkpAction* clone(const hkArray<hkpEntity*,hkContainerHeapAllocator>& entities, const hkArray<hkpPhantom*,hkContainerHeapAllocator>& phantoms) const; // vtbl+0x20 0x00B768E0 (:548)
    virtual ~vehicle_antigravity(); // vtbl+0x00 0x00B64680

private:
    hkVector4f acceleration_due_to_force; // 0x20
};

void unused_dcf_vehicle_bomb(); // 0x00B96F40 (:440)
void vehicle_update_passengers_pos_orient(vehicle* vp); // 0x00BB3700 (:997)
void vehicle_turret_turn_off_components(vehicle* vp, vehicle_component* vcp); // 0x00BA8850 (:1222)
void vehicle_build_turrets(vehicle* vp); // 0x00BA8A70 (:1246)
void vehicle_destroy_turrets(vehicle* vp); // 0x00B76B70 (:1349)
void vehicle_setup_havok_for_corpse(vehicle* vp); // 0x00B76BE0 (:1365)
void vehicle_blow_up(vehicle* vp, human* attacker, bool was_vehicle, const weapon_info* orig_weapon, bool explode); // 0x00BB6900 (:1440)
void vehicle_apply_hit_points(vehicle_damage_area* area, int hit_points); // 0x00B76D20 (:1895)
void vehicle_class_system_init(); // 0x00B76F90 (:2580)
void vehicle_class_level_init(); // 0x00B64A40 (:2600)
void vehicle_destroy_with_passengers(vehicle* vp); // 0x00BB3850 (:2606)
int vehicle_get_num_seats(const vehicle* vp); // 0x00B64BB0 (:4787)
int vehicle_get_num_seats(unsigned int v_handle); // 0x00B77750 (:4811)
void vehicle_get_angular_velocity(const vehicle* vp, vector& vel); // 0x00B77790 (:4821)
void vehicle_get_last_velocity(const vehicle* vp, vector& vel); // 0x00B64BD0 (:4838)
void vehicle_get_last_angular_velocity(const vehicle* vp, vector& vel); // 0x00B64BF0 (:4844)
void vehicle_get_velocity(const vehicle* vp, vector& vel); // 0x00B77840 (:4855)
void vehicle_get_velocity(unsigned int v_handle, vector& vel); // 0x00B778F0 (:4877)
unsigned int vehicle_get_hit_points(const vehicle* vp); // 0x00B64C10 (:4894)
unsigned int vehicle_get_max_hit_points(const vehicle* vp); // 0x00B64C20 (:4906)
void vehicle_get_aligned_box(vector& bmin, vector& bmax, vector& pos, matrix& orient, const vehicle* vp); // 0x00B77920 (:4920)
void vehicle_get_last_pos(const vehicle* vp, vector& last_pos); // 0x00B64C30 (:4943)
void vehicle_get_curr_pos(const vehicle* vp, vector& pos); // 0x00B64C50 (:4954)
int vehicle_get_num_passengers(const vehicle* vp); // 0x00B64D70 (:5024)
int vehicle_get_num_passengers(unsigned int v_handle); // 0x00B779C0 (:5047)
human* vehicle_passenger_in_seat(const vehicle* vp, vehicle_seat_index seat_index); // 0x00B779F0 (:5062)
bool vehicle_player_is_occupant(const vehicle* vp); // 0x00B77A80 (:5087)
bool vehicle_human_is_riding_with_non_exiting_player(const human* hp); // 0x00B77AE0 (:5105)
human* vehicle_passenger_in_seat(unsigned int v_handle, vehicle_seat_index seat_index); // 0x00B77BB0 (:5153)
human* vehicle_passenger_entering_seat(const vehicle* vp, vehicle_seat_index seat_index); // 0x00B77BE0 (:5166)
human* vehicle_passenger_entering_seat(unsigned int v_handle, vehicle_seat_index seat_index); // 0x00B77C40 (:5188)
human* vehicle_passenger_in_or_entering_seat(const vehicle* vp, vehicle_seat_index seat_index); // 0x00B77C70 (:5202)
human* vehicle_passenger_in_or_entering_seat(unsigned int v_handle, vehicle_seat_index seat_index); // 0x00B77CE0 (:5219)
human* vehicle_passenger_in_entering_or_reserving_seat(const vehicle* vp, vehicle_seat_index seat_index); // 0x00B77D10 (:5226)
human* vehicle_passenger_in_entering_or_reserving_seat(unsigned int v_handle, vehicle_seat_index seat_index); // 0x00B77D80 (:5245)
vehicle_seat_index vehicle_passenger_get_seat(const vehicle* vp, const human* hp); // 0x00B64D80 (:5289)
vehicle_seat_index vehicle_seat_name_to_index(const char* name); // 0x00B64DB0 (:5299)
human* vehicle_get_first_passenger(const vehicle* vp, vehicle_seat_index index); // 0x00B77DB0 (:5325)
human* vehicle_most_recent_driver(const vehicle* vp); // 0x00B77DF0 (:5346)
unsigned int vehicle_get_door_index_from_seat(const vehicle* vp, vehicle_seat_index seat_index); // 0x00B77E50 (:5362)
const vehicle_turret_mount* vehicle_get_seat_turret_mount(const vehicle* vp, vehicle_seat_index seat_index); // 0x00B64DF0 (:5389)
vehicle_turret_mount* vehicle_get_seat_turret_mount(vehicle* vp, vehicle_seat_index seat_index); // 0x00B64E40 (:5399)
vehicle_seat_index vehicle_get_seat_for_turret(const vehicle* vp, turret* tp); // 0x00B64E90 (:5409)
human* vehicle_get_human_in_gunner_seat(const vehicle* vp); // 0x00B8BE00 (:5421)
turret* vehicle_get_seat_turret(const vehicle* vp, vehicle_seat_index seat_index); // 0x00B77E80 (:5444)
vehicle_turret_mount* vehicle_get_turret_mount(vehicle* vp, int index); // 0x00B64ED0 (:5453)
bool vehicle_all_turrets_locked(vehicle* vp); // 0x00B77EC0 (:5463)
void vehicle_open_door(vehicle* vp, unsigned int door_index, float open_duration); // 0x00BAA660 (:5545)
void vehicle_close_door(vehicle* vp, unsigned int door_index, float close_duration); // 0x00BAA6A0 (:5568)
bool vehicle_door_is_fully_open(const vehicle* vp, unsigned int door_index); // 0x00B77F30 (:5591)
bool vehicle_door_is_fully_closed(const vehicle* vp, unsigned int door_index); // 0x00B77F80 (:5614)
float vehicle_get_door_open_pct(const vehicle* vp, unsigned int door_index); // 0x00B77FD0 (:5639)
void vehicle_check_door_for_seat(vehicle* vp, vehicle_seat_index seat_index, bool& door_exists, bool& door_is_attached); // 0x00B78040 (:5669)
int vehicle_get_type(const vehicle* vehicle_ptr); // 0x00B8BEE0 (:5700)
float vehicle_get_real_value(const vehicle* vehicle_ptr); // 0x00B8BF20 (:5711)
void vehicle_apply_linear_velocity(vehicle* target, const vector* linear_velocity); // 0x00B64EF0 (:5721)
void vehicle_apply_angular_impulse(vehicle* target, const vector* angular_impulse); // 0x00B64F90 (:5736)
void vehicle_apply_point_velocity(vehicle* vp, const vector* velocity, const vector* position); // 0x00B65030 (:5758)
void vehicle_process_explosion_hit(vehicle* target, human* attacker, object* source, const vector& explosion_pos, float radius, unsigned int damage, const vector* linear_impulse, const vector* angular_impulse, const weapon_info* w_info, const explosion_info* p_explosion_info); // 0x00BC2AD0 (:5916)
bool vehicle_is_corpse(const vehicle* vp); // 0x00B650E0 (:6117)
bool vehicle_is_on_fire(const vehicle* vehicle_ptr); // 0x00B650F0 (:6128)
bool vehicle_is_on_fire(unsigned int v_handle); // 0x00B780B0 (:6139)
bool vehicle_is_hidden(const vehicle* vp); // 0x00B65110 (:6155)
bool vehicle_human_in_vehicle(const vehicle* vp, const human* hp); // 0x00B65130 (:6166)
bool vehicle_player_in_vehicle(const vehicle* vp); // 0x00B65160 (:6181)
bool vehicle_is_player_car(const vehicle* vehicle_ptr); // 0x00B65190 (:6196)
bool vehicle_has_gunner_seat(const vehicle* vp); // 0x00B780E0 (:6219)
void vehicle_set_player_disabled(vehicle* vp, bool disabled); // 0x00B651B0 (:6245)
void vehicle_set_accelerator_disabled(vehicle* vp, bool disabled); // 0x00B651E0 (:6254)
void vehicle_get_center_of_mass_pos_orient(const vehicle* vp, vector& com_position, matrix* com_orient); // 0x00B65210 (:6265)
bool vehicle_is_vehicle_at_end_of_line(vehicle* vp); // 0x00B652A0 (:6284)
void vehicle_fade_in(vehicle* vp, int time); // 0x00B652C0 (:6294)
void vehicle_fade_out(vehicle* vp, int time, bool despawn_after_fade, bool fading_from_nano); // 0x00B78140 (:6309)
void vehicle_hide(unsigned int v_handle, bool hide); // 0x00B79040 (:7039)
void vehicle_hide_all_passengers(unsigned int v_handle, bool hide); // 0x00B791B0 (:7095)
bool vehicle_is_flipped(unsigned int v_handle); // 0x00B975F0 (:7116)
bool vehicle_is_flipped(const vehicle* vp); // 0x00B8C120 (:7125)
bool vehicle_is_usable(const human* hp, const vi_enter_data* parameters, const vehicle* vp, const bool ignore_measurements, const bool allow_flipped); // 0x00B8C160 (:7144)
void vehicle_flip(unsigned int handle); // 0x00B79220 (:7338)
void vehicle_flip_upside_down(unsigned int handle, bool flip_45_degrees); // 0x00B79340 (:7363)
bool vehicle_apply_damage(vehicle* vp, object* attacker, const weapon_info* attacker_weapon, damaged_by_types from_type, const vector* damage_position, const vector* damage_normal, float damage_radius, int damage_to_apply, const vector* damage_velocity, bool from_server); // 0x00BBFC60 (:7559)
bool vehicle_queue_damage(vehicle* vp, object* attacker, const weapon_info* attacker_weapon, damaged_by_types from_type, const vector* damage_position, const vector* damage_normal, float damage_radius, int damage_to_apply, const vector* damage_velocity, bool from_server); // 0x00B65560 (:7950)
void vehicle_dequeue_damage(vehicle* vp); // 0x00BC08A0 (:8001)
void vehicle_process_animations(vehicle* vp); // 0x00B79530 (:8035)
bool vehicle_get_ai_info(unsigned int v_handle, havok_vehicle_ai_info& info); // 0x00B8C790 (:8084)
bool vehicle_is_resource_loaded(unsigned int v_handle); // 0x00B79670 (:8163)
hkpRigidBody* vehicle_get_rigid_body(unsigned int v_handle); // 0x00B796A0 (:8180)
bool vehicle_calculate_ground_pos_and_orient(vector& out_pos, matrix& out_orient, const vector& start_pos, const vector& start_fvec, float raycast_y_offset, bool allow_upwards_raycast); // 0x00B97650 (:8202)
bool vehicle_reposition_point_has_obstacles(vehicle* vp, vector& new_pos, matrix& new_orient); // 0x00B8C9A0 (:8287)
bool vehicle_reposition_on_ground(vehicle* vp); // 0x00B97B60 (:8321)
void vehicle_init_damage_areas(vehicle* vp); // 0x00B65660 (:8346)
void vehicle_freeze(unsigned int v_handle); // 0x00B8CB30 (:8372)
void vehicle_lock_position(unsigned int v_handle, bool lock); // 0x00B796D0 (:8398)
const char* vehicle_get_info_name(const vehicle* vp); // 0x00B656B0 (:8434)
void vehicle_get_common_info(unsigned int v_handle, vector& pos, matrix& orient, float& speed, bool& moving_in_reverse); // 0x00B79750 (:8488)
bool vehicle_get_speed(const vehicle* vp, float& speed); // 0x00B79860 (:8522)
void vehicle_get_chassis_bbox(unsigned int v_handle, vector& bmin, vector& bmax); // 0x00B798E0 (:8555)
void vehicle_set_damage_trigger(unsigned int v_handle, unsigned short func_handle, float damage_percent); // 0x00B79930 (:8596)
void vehicle_set_destroy_trigger(unsigned int v_handle, unsigned short func_handle); // 0x00B79970 (:8614)
unsigned int vehicle_get_number_of_components(unsigned int v_handle); // 0x00B799A0 (:8632)
void vehicle_set_invulnerable(unsigned int v_handle, bool invulnerable); // 0x00B799E0 (:8651)
void vehicle_set_npc_ray_override(unsigned int v_handle, bool ride); // 0x00B79A20 (:8661)
void vehicle_set_components_strongest(unsigned int v_handle, bool strongest); // 0x00B8CC30 (:8686)
void vehicle_set_hit_points(unsigned int v_handle, unsigned int hit_points); // 0x00B79A70 (:8715)
void vehicle_get_smoking_and_fire(unsigned int v_handle, bool& smoking, bool& on_fire); // 0x00B79AB0 (:8729)
void vehicle_set_smoking_and_fire(unsigned int v_handle, bool smoking, bool on_fire, bool from_nano); // 0x00B8CCB0 (:8747)
bool vehicle_all_passengers_have_resources_loaded(unsigned int v_handle); // 0x00B79B00 (:8874)
bool vehicle_get_honked_horn(unsigned int v_handle); // 0x00B79B80 (:8899)
void vehicle_turnoff_horn(unsigned int v_handle); // 0x00B79BB0 (:8914)
void vehicle_set_max_hp(int new_max, unsigned int v_handle); // 0x00B8CFE0 (:8932)
bool vehicle_collisions_disabled(unsigned int v_handle); // 0x00B79BE0 (:8966)
void vehicle_disable_collisions(unsigned int v_handle, bool disable); // 0x00B79C40 (:8983)
void vehicle_set_chassis_variant(unsigned int v_handle, unsigned int num_rands, const char* rand_set); // 0x00B79DA0 (:9069)
void vehicle_update_orient_pos_from_havok(vehicle* vp); // 0x00B79DC0 (:9100)
void vehicle_do_frame_presim(vehicle* vp); // 0x00BBB570 (:9166)
void compute_tread_wheel_ground_cast_positions(const vehicle* vp, const vehicle_component* comp, vector& start, vector& end, float& length, float& radius); // 0x00B656C0 (:9428)
void vehicle_do_frame_postsim(vehicle* vp); // 0x00BC0950 (:9456)
bool vehicle_post_sim_fade_despawn(vehicle* vp); // 0x00B79FB0 (:9995)
void vehicle_post_sim_update_corpse(vehicle* vp); // 0x00B7A0A0 (:10046)
void vehicle_free_havok_data(vehicle* vp); // 0x00B97BF0 (:10089)
unsigned int vehicle_get_collision_filter(const vehicle* vp); // 0x00B65980 (:10123)
unsigned int vehicle_get_collision_filter(const vehicle* vp, unsigned int alt_body_idx, unsigned int shapekey); // 0x00B7A1A0 (:10133)
unsigned int vehicle_get_component_index_from_shape_key(const vehicle* vp, unsigned int shape_key, unsigned int alt_body_index); // 0x00B8D0A0 (:10158)
void vehicle_enable_human_door_collisions(vehicle* vp, bool enable); // 0x00B7A210 (:10404)
void vehicle_catch_fire(vehicle* vp, bool from_nano); // 0x00B8D340 (:10442)
void vehicle_switch_headlights(unsigned int vehicle_handle, unsigned int ignored, bool is_on); // 0x00B7A2D0 (:10490)
void vehicle_detach_turrets(vehicle* vp); // 0x00B7A360 (:10614)
bool vehicle_engine_is_running(const vehicle* const vp); // 0x00B659A0 (:10647)
void vehicle_set_driver_inputs_due_to_crash(vehicle* vp, float steering_param); // 0x00B8D470 (:10659)
float calculate_vehicle_steering_param_for_crash(const vehicle* p_vehicle, const vector* collision_point, const vector* collision_direction); // 0x00B659C0 (:10678)
float vehicle_get_dist_to_back_bumper(const vehicle* vp); // 0x00B65C70 (:10726)
float vehicle_get_dist_to_front_bumper(const vehicle* vp); // 0x00B65CA0 (:10741)
void vehicle_set_bomb_status(vehicle* vp, vehicle_bomb_status status); // 0x00B65CC0 (:10755)
void vehicle_set_scripted_car_bomb(vehicle* vp); // 0x00B65CE0 (:10771)
vehicle_ai* vehicle_get_vehicle_ai(unsigned int v_handle); // 0x00B7A3D0 (:10783)
void vehicle_set_controls_when_abandoned(const vehicle* vp); // 0x00B8D4B0 (:11173)
void vehicle_set_controls_when_abandoned(unsigned int v_handle); // 0x00B8D4F0 (:11199)
void vehicle_mp_force_blow_up(vehicle* vp); // 0x00BB75D0 (:11206)
bool vehicle_is_reserved(const vehicle* vp); // 0x00B7A440 (:11214)
void vehicle_reserve_vehicle(vehicle* vp, human* hp); // 0x00BBBBA0 (:11234)
bool is_vehicle_on_bridge(vehicle* vp, unsigned int& navpoint_handle); // 0x00B7A4A0 (:11247)
int vehicle_get_smoke_threshold(vehicle_damage_area* engine); // 0x00B7A5E0 (:11314)
float vehicle_get_smoke_amount(vehicle_damage_area* engine); // 0x00B7A610 (:11324)
vehicle_bomb_status vehicle_get_bomb_status(const vehicle* vp); // 0x00753E50 (.h:1277)

// 0x005DCD10 (.h:349)
inline const object_type vehicle::get_type_static()
{
    return OT_VEHICLE;
}

// 0x005DCD20 (.h:349)
inline const object_sub_type vehicle::get_subtype_static()
{
    return OT_SUB_UNDEFINED;
}

// 0x00753DD0 (.h:322)
inline void vehicle_flags::reset()
{
    memset(this, 0, sizeof(*this));
}

// 0x00753DE0 (.h:349)
inline int vehicle::get_bpo_index_static()
{
    return 0;
}

// 0x00B9DAB0 (.h:349)
inline const char* vehicle::get_classname() const
{
    return "vehicle";
}

// 0x00B9DAC0 (.h:349)
inline void vehicle::pool_free()
{
}

// 0x00B63030 (.h:649)
inline float vehicle::get_electrical_damage_percentage() const
{
    return m_electrical_damage_percent;
}

// 0x00753DF0 (.h:653)
inline bool vehicle::is_occupied() const
{
    return m_flags.m_occupied != 0;
}

// 0x005DCD30 (.h:656)
inline bool vehicle::is_corpse() const
{
    return m_flags.m_corpse != 0;
}

// 0x005DCD40 (.h:659)
inline bool vehicle::is_on_fire() const
{
    return m_flags.m_on_fire != 0;
}

// 0x0080D300 (.h:786)
inline bool vehicle::get_infinite_mass() const
{
    return infinite_mass;
}

// 0x0080D310 (.h:789)
inline bool vehicle::get_extra_mass() const
{
    return extra_mass;
}

// 0x00753E00 (.h:790)
inline void vehicle::set_extra_mass(bool set)
{
    extra_mass = set;
}

// 0x0080D320 (.h:791)
inline float vehicle::get_extra_mass_value() const
{
    return extra_mass_value;
}

// 0x00753E10 (.h:792)
inline void vehicle::set_extra_mass_value(float set)
{
    // never lighter than the vehicle itself
    extra_mass_value = 0.0f <= 1.0f - set ? 1.0f : set;
}

// 0x00753E50 (.h:1277)
inline vehicle_bomb_status vehicle_get_bomb_status(const vehicle* vp)
{
    return vp->m_bomb_status;
}
