// Original: volition/rfg/code/objects/movers/object_mover.h
// Implementation: volition/rfg/code/objects/movers/object_mover.cpp
//
// object_mover, the base of the destructible and movable level objects (rfg_mover for buildings and destructible
// chunks, general_mover for props). It holds the chunk/shape ids from the zone, team and control, the script event
// callbacks of the level object, render opacity and fade timers. object_mover_do_frame_pre_sim runs
// general_mover_do_frame_pre_sim, rfg_mover_do_frame_pre_sim, then posts the queued damaged/destroyed script events and
// the script point effects (decompiled). object_mover_do_frame_post_sim updates the render opacity of every mover
// (vtbl+0x94) and retries find_anchor_points (vtbl+0x8C) for the movers in Find_anchors_delayed (one per frame while
// loading). Reference positions (object_mover_get_reference_pos) are a table of positions movers are stored relative to.
#pragma once

#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/level_object_properties_shared.h"
#include "volition/rfg/code/objects/obj_enums.h"
#include "volition/rfg/code/objects/object.h"
#include "volition/rfg/code/objects/object_type.h"
#include "volition/rfg/code/scripting/action_script/global_event.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/os/lockfree_queues.h"
#include "volition/vlib/code/time/timestamp.h"
#include "volition/vlib/code/util/hash_table.h"

struct bbox;
struct c_chunk_base;
struct event_data;
struct explosion_info;
struct gameplay_properties;
class navpoint;
class npc;
class rfg_mover;
class hkpCdBodyPairCollector;
class human;
struct level_object_inst_properties;
class matrix;
struct notify_info;
struct object_mover_error_info;
class rl_renderable_instance;
class rl_renderer;
struct weapon_fire_info;
struct weapon_info;

// size 0xF8
class object_mover : public object
{
    // objective_highlight.cpp's glow outlines take the render instance through the virtual (friends are not recorded
    // in rfg.pdb; both are file-local in rfg.exe, so external here to be friends).
    friend bool glow_set_rl_outline(object* op, int idx);
    friend bool clear_glow_effect(object* op);
public:
    static const object_type get_type_static(); // 0x005DBE60 (.h:122)
    static const object_sub_type get_subtype_static(); // 0x005DBE70 (.h:122)
    static const char* get_classname_static();
    static int get_bpo_index_static(); // 0x005DBE80 (.h:122)
    virtual const char* get_classname() const; // vtbl+0x0C 0x007166E0 (.h:122)
    virtual void pool_free(); // vtbl+0x80 0x007166F0 (.h:122)
    virtual void update_pos_and_orient(const vector& update_pos, const matrix& update_orient, bool set_havok_data); // vtbl+0x30 0x00AA2500 (:344)
    virtual int get_tag_index_from_name(const char* tag_name) const; // vtbl+0x34 0x00A5F240 (:367)
    virtual bool get_tag_world_coords(int tag_index, vector& tag_pos, matrix* tag_orient, vector* obj_pos, matrix* obj_orient) const; // vtbl+0x38 0x00A136C0 (:393)
    virtual void get_bbox(vector&, vector&) const = 0; // vtbl+0x04
    virtual void get_local_bbox(vector&, vector&) const = 0; // vtbl+0x08
    virtual bool mover_should_sync() = 0; // vtbl+0x84
    virtual bool apply_damage(float, damaged_by_types, const vector&, const vector&, const object*, unsigned int, bool) = 0; // vtbl+0x88
    virtual bool find_anchor_points() = 0; // vtbl+0x8C
    virtual void release_anchor_points(); // vtbl+0x90 0x00A5F2C0 (:421)
    virtual float get_energy_scale(); // vtbl+0x64 0x00A137A0 (:437)
    virtual void process_bullet_hit(human* attacker, float damage, const weapon_fire_info& fire_info); // vtbl+0x68 0x00AA2610 (:1174)
    virtual bool process_melee_hit(object* attacker, float damage, const vector& impulse, const vector& hit_pos, const vector& hit_normal, unsigned int shape_key, const weapon_info& w_info); // vtbl+0x6C 0x00A139C0 (:1496)
    virtual bool serialize(obj_prop_block& pblock, bool load); // vtbl+0x14 0x00ADFF30 (:451)
    virtual bool stream(bool load); // vtbl+0x1C 0x00A8BCB0 (:781)
    virtual bool serialize_restore_if_missing_in_save_file(obj_prop_block& pblock); // vtbl+0x20 0x00A137B0 (:720)
    virtual bool serialize_restore_if_resetting_destruction(obj_prop_block& pblock); // vtbl+0x24 0x00A137C0 (:725)
    int mover_is_type(mover_flag_types mover_type) const; // 0x005DBE90 (.h:214)
    int get_mover_type_flags() const;
    bool show_on_map() const; // 0x007542A0 (.h:220)
    void set_ignore_importance(bool should_ignore); // 0x00A137F0 (:740)
    bool ignore_importance_level() const; // 0x00A13820 (:750)
    bool was_dynamically_created() const; // 0x00A05DC0 (.h:229)
    object_importance_level get_importance_level() const; // 0x00A13830 (:756)
    virtual void update_render_opacity() = 0; // vtbl+0x94
    unsigned short get_render_opacity() const; // 0x00A05DD0 (.h:237)
    void set_render_opacity(unsigned short render_opacity); // 0x00710EF0 (.h:242)
    void check_liberation(); // 0x00A13860 (:876)
    virtual int time_since_last_damage() const = 0; // vtbl+0x98
    virtual void set_render_created(); // vtbl+0x9C 0x00710F00 (.h:252)
    virtual void set_render_changed(); // vtbl+0xA0 0x00710F10 (.h:253)
    virtual float percent_destroyed() const = 0; // vtbl+0xA4
    void script_point_kill(int idx, bool delayed); // 0x00A38310 (:886)
    void script_point_finish_death(int idx); // 0x00A38430 (:925)
    void script_point_do_frame(int idx); // 0x00A384D0 (:948)
    bool script_points_process_explosion_hit(const object* source, const explosion_info* e_info, const vector& posp, const vector* dir); // 0x00A5F330 (:1032)
    bool has_script_point_effect(unsigned int shape_key, int* return_idx) const; // 0x00A138B0 (:1060)
protected:
    virtual void free_this(); // vtbl+0x28 0x00AA2530 (:1096)
    void create_anchors(unsigned int col_type, hkpCdBodyPairCollector& collector); // 0x00A138F0 (:1133)
    virtual rl_renderable_instance* get_render_model_instance(); // vtbl+0xA8 0x00A13940 (:1167)
    virtual void update_render(rl_renderer*); // vtbl+0x44 0x00A387E0 (:1995)
    int handle_fading(float dist_squared, float max_dist_squared); // 0x00AA2AA0 (:1852)
    int handle_fading_in(int fade_duration); // 0x00A8BE90 (:1916)
    int handle_fading_out(int fade_duration); // 0x00A8C000 (:1956)
    void drop_control(); // 0x00A13D70 (:2024)
    void check_for_movement_delete(); // 0x00A13690 (:354)
    bool script_point_apply_damage(const object* source, float damage, damaged_by_types from_type, unsigned int hit_shape_key, bool from_server); // 0x00A38940 (:2059)
    virtual void script_point_destroy(int) = 0; // vtbl+0xAC
private:
    void set_control_value(); // 0x00A13AF0 (:1582)
    void adjust_control_value(); // 0x00A13A60 (:1541)
public:
    object_mover(); // 0x007166B0
    virtual ~object_mover(); // vtbl+0x00 0x00716700

private:
    friend class cartographer; // get_structure_importance_icon reads mover_type_bit_flags (inferred)
    friend float activity_register_damage_get_cost(const object* objp); // reads mover_type_bit_flags (inferred)
    friend class flee_to_safe_building_action; // get_cost reads mover_type_bit_flags (inferred)
    friend void ai_building_process_streaming(rfg_mover* building, bool stream_in); // reads mover_type_bit_flags (inferred)
    friend navpoint* find_nearest_safe_trans_point(const npc* np, const object* flee_objp); // reads mover_type_bit_flags (inferred)
    friend void restore_clean_up_object(hash_table<unsigned int,&hash_uint,2048,unsigned short>& restored_shapes, object* objp); // reads mover_type_bit_flags (inferred)
    friend bool restore_post_zone_serialize_cleanup_helper(hash_table<unsigned int,&hash_uint,2048,unsigned short>& restored_shapes, bbox& root_box, unsigned int handle, unsigned short zone_gid); // reads mover_type_bit_flags (inferred)
    int mover_type_bit_flags; // 0x9C
    unsigned short m_render_opacity; // 0xA0
public:
    unsigned short reference_position_index; // 0xA2
    unsigned int chunk_uid; // 0xA4
    unsigned int shape_uid; // 0xA8
    unsigned int chunk_obj_uid; // 0xAC
    unsigned int destruction_checksum; // 0xB0
    level_object_inst_properties* m_level_obj_properties; // 0xB4
    event_result (*on_stream_in_callback)(const event_data&, notify_info*); // 0xB8
    event_result (*on_damaged_callback)(const event_data&, notify_info*); // 0xBC
    event_result (*on_destroyed_callback)(const event_data&, notify_info*); // 0xC0
    event_result (*on_use_action_callback)(const event_data&, notify_info*); // 0xC4
    const gameplay_properties* gameplay_props; // 0xC8
    human_teams team; // 0xCC
    float control; // 0xD0
    unsigned int nav_cell_detour_request_handle; // 0xD4
    unsigned int creator_handle; // 0xD8
    unsigned int final_attacker; // 0xDC
    unsigned int mover_flags; // 0xE0
    int fade_start_time; // 0xE4
    timestamp fade_out_timer; // 0xE8
    timestamp melee_damage_delay; // 0xEC
    timestamp inactive_fade_time; // 0xF0
    object_mover_error_info* m_mp_error_info; // 0xF4
};

// size 0xC
class obj_mover_props : public obj_prop_block
{
public:
    obj_mover_props(const vector& pos, const matrix& orient, const char* chunk_name, const char* base_prop_name, bool cast_shadows, bool cast_drop_shadows, const char* name, int mover_type_bit_flags, object_importance_level importance_level, human_teams team, int civilian_count); // 0x0071ADA0 (.h:320)
    ~obj_mover_props(); // 0x00710F20

};

// size 0x10
struct mover_reference_record
{
    mover_reference_record(); // 0x00A135E0

    vector start_point; // 0x00
    float sum_dist_sqr; // 0x0C
};

extern str_data_hash_table<unsigned int, 32, 32, unsigned char> Chunk_flags_table; // 0x03065F90, chunk flag name -> flag
extern str_data_hash_table<unsigned int, 32, 32, unsigned char> Mover_type_table; // 0x0304B0F0, mover type name -> type flag
extern lf_node_queue<unsigned int> Mover_damaged_event_free_queue; // 0x0169CEE8
extern lf_node_queue<unsigned int> Mover_damaged_event_queue; // 0x0169CF38, handles of movers with a pending damaged event
extern lf_node_queue<unsigned int> Mover_destroyed_event_free_queue; // 0x0169CF88
extern lf_node_queue<unsigned int> Mover_destroyed_event_queue; // 0x0169CFD8, handles of movers with a pending destroyed event
extern lf_node<unsigned int> Mover_damaged_event_nodes[64]; // 0x030266C0
extern lf_node<unsigned int> Mover_destroyed_event_nodes[128]; // 0x03029B60

void object_mover_process_explosion_hit(object_mover* obj_mover, object* owner, float damage, const vector& linear_impulse, const vector& angular_impulse, const explosion_info* p_explosion_info, const vector& world_position, const vector* dir, const weapon_info* w_info); // 0x00AA2770 (:1201)
void object_mover_system_init(); // 0x00AB39F0 (:1302)
unsigned short object_mover_get_reference_pos_index(const vector& pos); // 0x00A38740 (:1325)
void object_mover_get_reference_pos(vector& reference_pos, unsigned short index); // 0x00A13990 (:1347)
void object_mover_level_init_post(); // 0x00A8BDE0 (:1400)
void object_mover_level_init_pre(); // 0x00A139B0 (:1418)
void object_mover_level_shutdown(); // 0x00A5F630 (:1424)
void object_mover_do_frame_pre_sim(); // 0x00A8BE10 (:1442)
void object_mover_do_frame_post_sim(); // 0x00A5F700 (:1455)
void object_mover_process_collision(object_mover* target, object* obj_other, float damage, const vector& hit_pos, const vector& hit_normal, const vector& hit_vel, unsigned int hit_shape_key); // 0x00A13C60 (:1628)
void object_mover_apply_point_velocity(object_mover* obj_mover, const vector* velocity, const vector* point); // 0x00A13CB0 (:1663)
void object_mover_apply_linear_velocity(object_mover* obj_mover, const vector* velocity); // 0x00A13CD0 (:1678)
object_mover* object_mover_create_from_chunk(const vector& pos, const matrix& orient, const char* file_name, unsigned int parent_handle); // 0x00A8BE30 (:1740)
object_mover* object_mover_create_from_chunk(const vector& pos, const matrix& orient, const c_chunk_base* base, unsigned int parent_handle); // 0x00A5FA00 (:1752)
bool object_mover_is_visible(const object_mover* mover); // 0x00A13CF0 (:1810)
bool object_mover_is_fading_for_delete(const object_mover* mover); // 0x00A13D10 (:1816)
void object_mover_mark_for_fade(object_mover* mover, int time_until_fade); // 0x00A13D30 (:1826)
void object_mover_handle_source_exploded(object* obj, const explosion_info* e_info); // 0x00A38A70 (:2139)
void object_mover_maybe_add_to_damaged_event_queue(object_mover* omp); // 0x00A5FA50 (:2176)
void object_mover_maybe_add_to_destroyed_event_queue(object_mover* omp); // 0x00A5FAB0 (:2195)
