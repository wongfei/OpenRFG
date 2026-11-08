// Original: volition/rfg/code/destruction/rfg_dynamics.h
// Implementation: volition/rfg/code/destruction/rfg_dynamics.cpp
//
// Destruction dynamics: damage reaches movers as pre-collide events (rfg_dyn_add_pre_collide_*: explosion, direct
// damage, link broken, shatter, shape cutter, repair, slice, shardify, multiplayer destroy...), each an
// rfg_collision_event_base with the physics of both sides (raw or compressed for the network) and the event's data
// (direct subpieces, broken links, invulnerable movers share one array; damage/radius and unique id/repair duration
// share fields). rfg_dyn_do_frame_pre / rfg_dyn_do_collide_post / rfg_dyn_do_frame_post apply them: subpieces break
// off into shards (rfg_shard_info: mass, impulse, bounds, move type), new movers are created through the shardify
// jobs (rfg_shardify_info, lock-free work nodes), and rfg_destruction_event records the destroyed regions for the
// dynamic hole requests (rfg_destruction_event_do_frame). Placement of the types inferred where the PDB records no
// inline code.
#pragma once

#include "volition/rfg/code/destruction/rfg_mover.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/os/lockfree_queues.h"
#include "volition/vlib/code/time/timestamp.h"

struct destruction_flags;
class dynamic_hole_request;
struct explosion_info;
struct hkpContactPointAddedEvent;
struct hkpContactPointRemovedEvent;
class hkpDynamicsContactMgr;
class human;
class object;
class object_mover;
class rfg_destroyable_shape;
struct rfg_link_base;
struct rfg_subpiece;
struct rfg_subpiece_base;

enum dynamic_link_state
{
    DLS_INVALID = -1,
    DLS_SUPPORTED = 0,
    DLS_NEUTRAL = 1,
    DLS_SUPPORTING = 2,
    NUM_DYNAMIC_LINK_STATES = 3,
};

enum destruction_event_type
{
    DET_INVALID = -1,
    DET_EXPLOSION = 0,
    DET_DIRECT_DAMAGE = 1,
    DET_SP_ARRAY = 2,
    DET_OBJECT_SIMPLE = 3,
    DET_OBJECT_COMPLEX = 4,
    DET_SHATTER = 5,
    DET_SHAPE_CUTTER = 6,
    DET_REPAIR = 7,
    DET_MP_DESTROY = 8,
    DET_MP_VEHICLE = 9,
    DET_SHARDIFY = 10,
    DET_DESTRUCTION_MODE = 11,
    DET_LINK_BROKEN = 12,
    DET_SLICE = 13,
    NUM_DET_TYPES = 14,
};

// One shardify delete count reported to the network (multi_game_destruction_frame_server_add_shardify_delete_counts).
// Placement inferred.
// size 0x8
struct shardify_delete_record
{
    rfg_mover* mover; // 0x00
    int delete_count; // 0x04
};

// One piece and its damage for rfg_dyn_add_pre_collide_direct_damage. Placement inferred.
// size 0x8
struct sp_combined_direct_damage_raw
{
    unsigned int shape_key; // 0x00
    unsigned int damage; // 0x04
};

// size 0x4
struct comp_sp_combined_direct_damage
{
    unsigned short compressed_shape_key; // 0x00
    unsigned short compressed_damage; // 0x02
};

// The constructors of the collision event types are compiler generated (empty in rfg.exe: 0x00711740, 0x0071B980,
// 0x007238D0, 0x0072C710; copies 0x00719850, 0x00723140, 0x0072B250, 0x00732300, 0x00719790).
// size 0x18
struct rfg_collision_physics_base_raw
{

    vector rcollision_point_local; // 0x00
    vector rcollision_normal_local; // 0x0C
};

// size 0x1C
struct rfg_collision_physics_base_compressed
{
    unsigned int ccollision_point_local_r; // 0x00
    unsigned int ccollision_point_local_theta; // 0x04
    unsigned int ccollision_point_local_phi; // 0x08
    unsigned int ccollision_dir_local_r; // 0x0C
    unsigned int ccollision_dir_local_theta; // 0x10
    unsigned int ccollision_dir_local_phi; // 0x14
    unsigned short cobj_network_handle; // 0x18
    unsigned short cdamage; // 0x1A
};

// size 0x58
struct rfg_collision_physics_base
{

    rfg_collision_physics_base_raw raw; // 0x00
    rfg_collision_physics_base_compressed comp; // 0x18
    vector collision_point_local; // 0x34
    vector collision_normal_local; // 0x40
    unsigned int shape_key; // 0x4C
    unsigned int obj_handle; // 0x50
    unsigned char misc; // 0x54
};

// size 0xEC
struct rfg_collision_event_base
{
    unsigned short get_direct_sps(int idx) const; // 0x007110A0 (.h:205)
    void set_direct_sps(int idx, unsigned short val); // 0x007110B0 (.h:210)
    const comp_sp_combined_direct_damage& get_combined_direct_damage(int idx) const; // 0x007110D0 (.h:216)
    void set_combined_direct_damage(int idx, unsigned short compressed_damage, unsigned short compressed_shape_key); // 0x007110E0 (.h:221)
    unsigned int get_link_index(int idx) const; // 0x00711110 (.h:228)
    void set_link_index(int idx, unsigned int val); // 0x0094CFE0 (.h:233)
    const unsigned int* get_link_indices() const; // 0x0094D000 (.h:238)
    void set_link_indices(unsigned int num_indices, const unsigned int* indices); // 0x00711120 (.h:242)
    void set_slice_index(unsigned int slice_index); // 0x00711160 (.h:249)
    unsigned int get_slice_index() const; // 0x00711170 (.h:254)
    const unsigned int* get_invulnerable_mover_handles() const; // 0x00711180 (.h:260)
    void add_invulnerable_mover_handle(unsigned int handle); // 0x00711190 (.h:264)

    rfg_collision_physics_base base_a; // 0x00
    rfg_collision_physics_base base_b; // 0x58
    destruction_event_type destruction_type; // 0xB0
    union
    {
        unsigned int damage; // 0xB4
        float radius; // 0xB4
    };
    union
    {
        unsigned int exp_unique_id; // 0xB8
        unsigned int repair_duration; // 0xB8
    };
    float exp_radius_multiplier; // 0xBC
    unsigned int compressed_exp_radius_multiplier; // 0xC0
    unsigned int owner; // 0xC4
    unsigned char flags; // 0xC8
    unsigned char data_count; // 0xC9
private:
    union
    {
        unsigned short direct_sps[15]; // 0xCC
        comp_sp_combined_direct_damage combined_direct_damage[7]; // 0xCC
        unsigned int link_indices[7]; // 0xCC
        unsigned int invulnerable_mover_handles[7]; // 0xCC
    };
};

// size 0x50
struct rfg_shard_info
{
    void reset(); // 0x00711750 (:283)
    bool valid(); // 0x007117C0 (:297)
    void add_subpiece(rfg_destroyable_shape* shape, rfg_subpiece* sp); // 0x007117D0 (:302)
    void remove_subpiece(rfg_destroyable_shape* shape, rfg_subpiece* sp, bool clear_flags_and_impulse); // 0x007118B0 (:310)
    void finalize(rfg_destroyable_shape* shape); // 0x007238E0 (:326)

    object_mover* mp; // 0x00
    float mass; // 0x04
    vector impulse; // 0x08
    vector impulse_pos; // 0x14
    vector average_pos; // 0x20
    vector bmin; // 0x2C
    vector bmax; // 0x38
    rfg_move_type move_type; // 0x44
    unsigned short q; // 0x48
    short num_subpieces; // 0x4A
    unsigned short flags; // 0x4C
    unsigned short padding; // 0x4E
};

// size 0x10
struct rfg_shardify_info
{
    void init(rfg_mover* _rm); // 0x00711950 (:449)

    rfg_mover* rm; // 0x00
    rfg_shard_info* q_shard; // 0x04
    int num_q_shards; // 0x08
    int destruction_flags; // 0x0C
};

// size 0x10
class rfg_dyn_collision_energy_info
{
public:
    rfg_dyn_collision_energy_info(object* _obj_a, object* _obj_b, const vector& _collision_point_world); // 0x00714AE0 (:5803)

    float energy; // 0x00
    object* obj_a; // 0x04
    object* obj_b; // 0x08
    const vector& collision_point_world; // 0x0C
};

// size 0x4
struct destruction_flags
{
    unsigned int in_use : 1; // 0x00:0
    unsigned int modified : 1; // 0x00:1
    unsigned int is_bridge : 1; // 0x00:2
    unsigned int is_glass : 1; // 0x00:3
};

// size 0x54
class rfg_destruction_event
{
public:
    rfg_destruction_event(); // 0x0071CE10 (:7460)
    void reset(); // 0x00715460 (:7469)

    timestamp last_modified_timestamp; // 0x00
    vector bmin; // 0x04
    vector bmax; // 0x10
    matrix orient; // 0x1C
    vector pos; // 0x40
    destruction_flags flags; // 0x4C
    dynamic_hole_request* request; // 0x50
};

// size 0x24
struct rfg_collision_havok_base
{
    rfg_subpiece* affected_subpiece_a; // 0x00
    rfg_subpiece* affected_subpiece_b; // 0x04
    unsigned char count; // 0x08
    hkpDynamicsContactMgr* mgr; // 0x0C
    unsigned short id[10]; // 0x10
};

// size 0x110
struct rfg_collision_event
{
    bool add_contact_info(hkpDynamicsContactMgr* mgr, unsigned short id); // 0x00711640 (:190)
    bool remove_contact_info(hkpDynamicsContactMgr* mgr, unsigned short id); // 0x00711690 (:204)
    bool is_first_event(hkpDynamicsContactMgr* mgr, unsigned short id); // 0x00711710 (:229)

    rfg_collision_event_base collision_base; // 0x00
    rfg_collision_havok_base havok_base; // 0xEC
};

// size 0x34
class rfg_dyn_point_collision_energy_info : public rfg_dyn_collision_energy_info
{
public:
    rfg_dyn_point_collision_energy_info(object* _obj_a, object* _obj_b, const vector& _collision_point_world); // 0x00714B00 (:5810)

    float mass_a; // 0x10
    float mass_b; // 0x14
    vector vel_a; // 0x18
    vector vel_b; // 0x24
    unsigned char event_flags; // 0x30
};

void unused_dcf_load_balance_explosion_pct(); // 0x00711990 (:599)
// template instance: void swap_dynamics<float>(float& v1, float& v2); // 0x00719110 (:705)
// template instance: void swap_dynamics<rfg_shard_info>(rfg_shard_info& v1, rfg_shard_info& v2); // 0x00722260 (:705)
// template instance: void swap_dynamics<vector>(vector& v1, vector& v2); // 0x00719130 (:705)
// template instance: void swap_dynamics<unsigned int>(unsigned int& v1, unsigned int& v2); // 0x007190F0 (:705)
// template instance: void swap_dynamics<rfg_collision_event>(rfg_collision_event& v1, rfg_collision_event& v2); // 0x00737C10 (:705)
// template instance: void swap_dynamics<object*>(object*& v1, object*& v2); // 0x007190D0 (:705)
// The destruction events processed per frame over the last 360 frames (multiplayer server; multi_game.cpp allocates it).
extern unsigned int* Destruction_event_history; // 0x01EF8274

unsigned short rfg_dyn_get_next_destruction_frame(); // 0x00711AF0 (:716)
void remove_destruction_event(rfg_destruction_event* rde); // 0x0072C720 (:741)
void rfg_destruction_event_do_frame(); // 0x0072C850 (:909)
bool rfg_dyn_link_get_linked_data(rfg_mover* rm, rfg_subpiece* sp, object_mover*& other_mp, rfg_subpiece*& other_sp, rfg_subpiece_base*& other_sp_base); // 0x00724140 (:1704)
dynamic_link_state rfg_dyn_link_get_state(rfg_mover* rm, rfg_subpiece* sp); // 0x00724250 (:1740)
void rfg_dyn_link_update_depth(rfg_mover* rm, rfg_destroyable_shape* shape, unsigned short depth); // 0x0072C9E0 (:1766)
void rfg_dyn_link_set_depth(rfg_mover* rm, rfg_subpiece* sp, unsigned short depth, bool force_other); // 0x007242D0 (:1785)
bool rfg_dyn_link_get_depth(rfg_mover* rm, rfg_subpiece* sp, unsigned short& this_depth, unsigned short& other_depth); // 0x007243B0 (:1822)
void rfg_dyn_system_init(); // 0x00724780 (:4968)
void rfg_dyn_system_shutdown(); // 0x00713750 (:4984)
void rfg_dyn_level_init_pre(); // 0x0072DA00 (:4993)
void rfg_dyn_level_shutdown(); // 0x00713770 (:5023)
void rfg_dyn_do_frame_pre(); // 0x0073A550 (:5028)
void rfg_dyn_do_frame_post(); // 0x00734960 (:5107)
bool rfg_dyn_add_pre_collide_explosion(rfg_mover* mover, const object* owner, const explosion_info* e_info, const vector& explosion_pos_local, const vector& explosion_dir_local, float radius_mult, bool from_server); // 0x00713780 (:5121)
bool rfg_dyn_add_pre_collide_mp_vehicle(rfg_mover* mover, const object* owner, unsigned int damage, const vector& half_extents, const vector& world_pos, const matrix& world_orient, bool from_server); // 0x00713860 (:5159)
bool rfg_dyn_add_pre_collide_direct_damage(rfg_mover* mover, const object* owner, int sp_count, const sp_combined_direct_damage_raw* combined_sps, const vector& pos_local, const vector& normal_local, bool from_server); // 0x007139D0 (:5228)
bool rfg_dyn_add_pre_collide_link_broken(rfg_mover* mover, unsigned int num_broken_links, const unsigned int* link_indices, unsigned char flags, bool from_server); // 0x00713BB0 (:5311)
bool rfg_dyn_add_pre_collide_sp_array(rfg_mover* mover, const object* owner, unsigned int* shape_keys, int num_keys, bool from_server); // 0x00713DB0 (:5330)
bool rfg_dyn_add_pre_collide_shatter(rfg_mover* mover, const object* owner, unsigned int shape_key, unsigned int damage, const vector& local_pos, const vector& local_dir, bool from_server); // 0x00713F60 (:5376)
bool rfg_dyn_add_pre_collide_shape_cutter(rfg_mover* mover, const object* owner, float radius, const vector& local_pos, unsigned char exp_info_unique_id, bool from_server); // 0x0071C240 (:5410)
bool rfg_dyn_add_pre_collide_repair(rfg_mover* mover, const object* owner, const vector& local_pos, const float radius, const int duration, bool from_server); // 0x007140E0 (:5452)
bool rfg_dyn_add_pre_collide_mp_destroy(rfg_mover* mover, const object* owner, bool from_server); // 0x00714270 (:5484)
bool rfg_dyn_add_pre_collide_shardify(bool from_server); // 0x007143D0 (:5509)
bool rfg_dyn_add_pre_collide_destruction_mode(const unsigned int* invulnerable_mover_handles, int num_handles, bool from_server); // 0x007144F0 (:5520)
// Port addition (test seam, multiplayer.md MP4; no rfg.exe counterpart): the destruction mode is still the pregame one
// (a multiplayer level before its round begins; Destruction_mode == DM_PREGAME).
bool rfg_dyn_openrfg_is_pregame_destruction();
bool rfg_dyn_add_pre_collide_slice(rfg_mover* rmp, const vector& base_pos, int slice_index, float angle0, float angle1, bool from_server); // 0x007146B0 (:5541)
void rfg_dyn_repair_sphere(const vector& pos, const float radius, const int duration, human* hp); // 0x00714820 (:5563)
bool rfg_dyn_multi_add_pre_collide_contact(object* obj_a, object* obj_b, unsigned int damage, const vector& collision_point_local_a, const vector& collision_normal_local_a, const vector& collision_point_local_b, const vector& collision_normal_local_b, unsigned int shape_key_a, unsigned int shape_key_b, bool apply_impulse); // 0x0072DBA0 (:5641)
bool rfg_dyn_compute_collision_energy(rfg_dyn_collision_energy_info& energy_info); // 0x0071C850 (:5863)
bool rfg_dyn_add_pre_collide_contact(hkpContactPointAddedEvent& add_event, object* obj_a, object* obj_b); // 0x0072DDB0 (:5884)
void rfg_dyn_remove_pre_collide_contact(hkpContactPointRemovedEvent& remove_event); // 0x00714B60 (:6130)
void rfg_dyn_reset_destruction_frame(unsigned short frame); // 0x00714C30 (:6160)
unsigned short rfg_dyn_get_active_destruction_frame(); // 0x00714C40 (:6169)
void process_setup(lf_node<rfg_shardify_info>* this_node); // 0x0073AE70 (:6861)
void process_work(lf_node<rfg_shardify_info>* this_node); // 0x0073D510 (:6873)
void process_cleanup(lf_node<rfg_shardify_info>* this_node); // 0x007406B0 (:6882)
void swc_process_all(); // 0x007406D0 (:6897)
void mark_shardify_pools(); // 0x007150B0 (:6974)
void restore_to_mark_shardify_pools(); // 0x007150F0 (:6988)
void rfg_dyn_do_collide_post(); // 0x00740790 (:7002)
int rfg_dyn_get_linked_index(const rfg_destroyable_shape* shape, int sp_index, const rfg_link_base* fl); // 0x00715140 (:7244)
rfg_subpiece* rfg_dyn_get_linked_subpiece(const rfg_destroyable_shape* shape, const rfg_subpiece* sp, const rfg_link_base* fl); // 0x00715160 (:7255)
bool rfg_dyn_is_shard_large(const vector& bmin, const vector& bmax); // 0x007151B0 (:7262)
bool rfg_dyn_is_shard_small(const rfg_mover* rmp); // 0x0071CC50 (:7273)
float rfg_dyn_get_impulse_scalar(float mass, unsigned char material_index); // 0x0071CD60 (:7288)
bool rfg_dyn_maybe_delete(object_mover* obj_mover, const float mass, const vector& impulse, const float max_impulse_mag); // 0x00715290 (:7336)
unsigned int rfg_dyn_get_filter_by_move_type(int mt, int shape_flags); // 0x00715320 (:7345)
unsigned char rfg_dyn_get_auto_remove_level_by_move_type(int mt, int mover_flags); // 0x00715380 (:7385)
void rfg_dyn_add_to_process_shardify_list(rfg_mover* rmp); // 0x007153C0 (:7412)
void rfg_dyn_remove_from_process_shardify_list(rfg_mover* rmp); // 0x00715400 (:7430)
