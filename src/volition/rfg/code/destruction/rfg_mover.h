// Original: volition/rfg/code/destruction/rfg_mover.h
// Implementation: volition/rfg/code/destruction/rfg_mover.cpp
//
// rfg_mover, a destructible building or structure: an rfg_destroyable_shape (from the chunk) with its destroyable
// model instance as stream resource, damage state, stress test scheduling (stress_next/prev link the stress queues of
// rfg_stress.cpp), regrow (restore) on stream and the dynamic links that tie movers to each other.
// rfg_mover_do_frame_pre_sim / rfg_do_frame_pre (rfg_destruction.cpp, decompiled) clears per-frame shape flags, queues
// movers whose stress_test_delay elapsed for stress evaluation, switches the Havok collision type when the destruction
// mode timer ends (RFGM_* -> COLLIDE_RFG_*), slices large broken shapes, handles regrow begin/finish (one per frame)
// and rebuild requests (world::restore_object), then advances the stress evaluation state machine.
// explosion_event (from Explosion_event_pool, 20 entries) is the pending explosion of a mover (explosion_event_index).
// rfg_mover_temporary_data holds per-mover buffers allocated while it is being destroyed (world anchors, dynamic links,
// shape state, decals). Placement of explosion_event, rfg_move_type, rfg_mover_flags and rfg_mover_temporary_data
// inferred.
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/query/hkpcdbodypaircollector.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/level_object_properties_shared.h"
#include "volition/rfg/code/objects/movers/object_mover.h"
#include "volition/rfg/code/objects/obj_enums.h"
#include "volition/rfg/code/objects/object.h"
#include "volition/rfg/code/objects/object_type.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/time/timestamp.h"

class color;
struct explosion_info;
struct glass_obj;
class hkpCdBody;
class hkpWorldObject;
class matrix;
struct multi_rfg_mover_info;
class rfg_destroyable_shape;
struct rfg_dynamic_link;
struct rfg_subpiece;
class rl_destroyable_model_instance;
class rl_renderable_instance;
class rl_renderer;

enum rfg_move_type
{
    RFGM_INVALID = -1,
    RFGM_FIXED = 0,
    RFGM_NORMAL = 1,
    RFGM_LITE = 2,
    RFGM_ULTRA_LITE = 3,
    RFGM_WORLD_ONLY = 4,
    RFGM_NO_COLLISION = 5,
};

// size 0x4
struct rfg_mover_flags
{
    unsigned int under_stress : 1; // 0x00:0
    unsigned int run_stress_on_stream : 1; // 0x00:1
    unsigned int collapse_effect : 1; // 0x00:2
    unsigned int invulnerable : 1; // 0x00:3
    unsigned int game_destroyed : 1; // 0x00:4
    unsigned int fully_destroyed : 1; // 0x00:5
    unsigned int large_explosion : 1; // 0x00:6
    unsigned int pristine : 1; // 0x00:7
    unsigned int mp_no_fade_destroy : 1; // 0x00:8
    unsigned int render_xform_dirty : 1; // 0x00:9
    unsigned int is_ai_structure : 1; // 0x00:10
    unsigned int is_ai_building : 1; // 0x00:11
    unsigned int is_ai_subpiece : 1; // 0x00:12
    unsigned int is_ai_composite : 1; // 0x00:13
    unsigned int updating_anchors : 1; // 0x00:14
    unsigned int plume_on_death : 1; // 0x00:15
    unsigned int one_of_many : 1; // 0x00:16
    unsigned int mining : 1; // 0x00:17
    unsigned int supply_crate : 1; // 0x00:18
    unsigned int regrow_on_stream_begin : 1; // 0x00:19
    unsigned int regrow_on_stream_finish : 1; // 0x00:20
    unsigned int inherit_damage_pct : 1; // 0x00:21
    unsigned int rebuild : 1; // 0x00:22
    unsigned int propaganda : 1; // 0x00:23
    unsigned int building : 1; // 0x00:24
};

// size 0x18
struct rfg_mover_temporary_data
{
    enum data_type
    {
        INVALID_DATA_TYPE = -1,
        WORLD_ANCHOR = 0,
        DYNAMIC_LINK = 1,
        SHAPE_STATE = 2,
        DECAL_LIST = 3,
        NUM_STATE_DATA_TYPES = 4,
    };

    unsigned char* data_buf[4]; // 0x00
    unsigned short data_size[4]; // 0x10
};

// A subpiece damaged this frame (Damaged_sp_event_pool, linked by index from damaged_subpiece_event_first_index);
// process_jitter consumes them. Placement inferred.
// size 0x4
struct damaged_subpiece_event
{
    unsigned short subpiece_index; // 0x00
    unsigned short next_event_index; // 0x02
};

// A pending repair sphere in the mover's local space (Repair_event_pool, circular list from repair_event_list);
// process_repair_events consumes them. Placement inferred.
// size 0x14
struct repair_event
{
    repair_event(); // 0x00719840

    vector pos; // 0x00
    float radius; // 0x0C
    unsigned short next; // 0x10
    unsigned short prev; // 0x12
};

// size 0x1C
struct explosion_event
{
    explosion_event(); // 0x00719830

    vector pos; // 0x00
    float fake_radius; // 0x0C
    float fake_inner_radius; // 0x10
    unsigned int flags; // 0x14
    const explosion_info* info; // 0x18
};

// size 0x16C
class rfg_mover : public object_mover
{
public:
    // size 0x8
    struct resource_dependent_data
    {
        rfg_destroyable_shape* shape; // 0x00
        rl_destroyable_model_instance* model_instance_p; // 0x04
    };

    rfg_mover(); // 0x0071E750 (:1594)
    static const object_type get_type_static(); // 0x005DBEB0 (.h:131)
    static const object_sub_type get_subtype_static(); // 0x005DBEC0 (.h:131)
    static const char* get_classname_static(); // 0x00711080 (.h:131)
    static int get_bpo_index_static(); // 0x005DBED0 (.h:131)
    virtual const char* get_classname() const; // vtbl+0x0C 0x0071E7A0 (.h:131)
    static unsigned int get_classname_hash(); // 0x007166A0 (:1593)
    static int pool_remaining(); // 0x0071E740 (:1593)
    virtual void pool_free(); // vtbl+0x80 0x0072EB60 (:1593)
private:
    bool alloc_state_data(); // 0x00716730 (:1653)
    bool dealloc_state_data(); // 0x00716790 (:1676)
    void alloc_data_buf(rfg_mover_temporary_data::data_type type, unsigned char* data_buf, int buf_size); // 0x007167D0 (:1698)
    void dealloc_data_buf(rfg_mover_temporary_data::data_type type); // 0x00716870 (:1730)
    unsigned char* get_data_buf(rfg_mover_temporary_data::data_type type); // 0x007168C0 (:1743)
    unsigned int get_data_buf_size(rfg_mover_temporary_data::data_type type); // 0x007168E0 (:1750)
public:
    virtual rl_renderable_instance* get_render_model_instance(); // vtbl+0xA8 0x0071F880 (:5224)
protected:
    virtual void script_point_destroy(int idx); // vtbl+0xAC 0x00727090 (:5174)
public:
    virtual bool serialize(obj_prop_block& pblock, bool load); // vtbl+0x14 0x00725DE0 (:1764)
    virtual obj_serialize_mode serialize_mode(); // vtbl+0x18 0x00716900 (:2108)
    virtual bool stream(bool load); // vtbl+0x1C 0x00740BF0 (:2178)
    virtual bool serialize_restore_if_missing_in_save_file(obj_prop_block& pblock); // vtbl+0x20 0x00716980 (:2493)
    virtual bool serialize_restore_if_resetting_destruction(obj_prop_block& pblock); // vtbl+0x24 0x007169D0 (:2523)
    virtual bool is_killed() const; // vtbl+0x10 0x00716A10 (:2940)
    virtual bool apply_damage(float damage, damaged_by_types from_type, const vector& hit_pos, const vector& hit_normal, const object* source, unsigned int hit_shape_key, bool from_server); // vtbl+0x88 0x00734DB0 (:2948)
    virtual bool find_anchor_points(); // vtbl+0x8C 0x00726540 (:3111)
    virtual void release_anchor_points(); // vtbl+0x90 0x0071EDF0 (:3184)
    virtual void update_pos_and_orient(const vector& update_pos, const matrix& update_orient, bool set_havok_data); // vtbl+0x30 0x0071F130 (:3673)
    virtual bool get_tag_local_coords(int tag_index, vector& tag_pos, matrix* tag_orient) const; // vtbl+0x3C 0x00726E00 (:3692)
    virtual void get_bbox(vector& _bmin, vector& _bmax) const; // vtbl+0x04 0x00716C70 (:3725)
    virtual void get_local_bbox(vector& _bmin, vector& _bmax) const; // vtbl+0x08 0x0071F1A0 (:3732)
    float get_fully_destroyed_pct(); // 0x00716CB0 (:3746)
    virtual bool mover_should_sync(); // vtbl+0x84 0x007171A0 (:5285)
    virtual int attach_index(const vector& posp, unsigned int parent_objp_body_handle); // vtbl+0x50 0x0073C8F0 (:3757)
    virtual bool attach_index_is_valid(int index); // vtbl+0x54 0x0071F2E0 (:3764)
    virtual float get_mass() const; // vtbl+0x58 0x0071F310 (:3771)
    virtual unsigned char get_physical_material(unsigned int shape_key, const vector* contact_point, const hkpWorldObject* hk_objp) const; // vtbl+0x5C 0x00726E90 (:3782)
    virtual unsigned char get_effect_material(unsigned int shape_key, const vector* contact_point, const hkpWorldObject* hk_objp) const; // vtbl+0x60 0x00726ED0 (:3800)
    virtual bool debug_render_havok(const vector& center, const matrix& orientp, color c); // vtbl+0xB0 0x00716D10 (:4031)
    void add_to_world(rfg_move_type initial_move_type); // 0x007267E0 (:3210)
    void set_move_type(rfg_move_type mt, bool force); // 0x0071EF10 (:3287)
    rfg_move_type get_move_type() const; // 0x005DBEE0 (.h:269)
    virtual float percent_destroyed() const; // vtbl+0xA4 0x00716A20 (:3308)
    bool get_sub_piece_percent_destroyed(float& percent_destroyed, int sub_piece_index) const; // 0x0072EEF0 (:3371)
    bool get_sub_piece_percent_destroyed(float& percent_destroyed, const char* sub_piece_name) const; // 0x0072EDF0 (:3348)
    void maybe_post_damage_sound(const object* source, vector hit_pos, float damage_amt); // 0x0072EF40 (:3394)
    void play_collapse(rfg_mover* other_mover); // 0x0072ECA0 (:3315)
    explosion_event* get_explosion_event(); // 0x0071EFF0 (:3438)
    bool create_explosion_event(const vector& posp, float outer_radius, float inner_radius, unsigned int flagsp); // 0x00726B60 (:3472)
    bool create_explosion_event(const vector& posp, const explosion_info* info, unsigned int flagsp); // 0x00726AD0 (:3449)
    void release_explosion_event(); // 0x0072F000 (:3496)
    damaged_subpiece_event* get_damaged_subpiece_events(); // 0x0071F030 (:3506)
    damaged_subpiece_event* get_next_damaged_subpiece_event(damaged_subpiece_event* cur_event); // 0x0071F060 (:3516)
    bool add_damaged_subpiece_event(unsigned int sp_key); // 0x00726C00 (:3526)
    void release_damaged_subpiece_events(); // 0x0072F050 (:3542)
    bool record_repair_event(const vector& posp, float radius); // 0x00726C50 (:3556)
    void start_smoke_plume_effect(); // 0x00716A50 (:3577)
    void stop_smoke_plume_effect(); // 0x0071F090 (:3602)
    bool player_damage_only() const; // 0x00716B50 (:3620)
    bool allow_auto_restore() const; // 0x00716C30 (:3636)
    bool is_tracked_mover() const; // 0x00716C50 (:3647)
    void update_physics_properties(); // 0x0071F0D0 (:3659)
    bool can_check_building() const; // 0x0071F7A0 (:4289)
    bool has_building_size() const; // 0x00717060 (:4295)
    bool has_building_mass() const; // 0x00717110 (:4320)
    virtual int time_since_last_damage() const; // vtbl+0x98 0x0071F7C0 (:4333)
    int query_closest_subpiece_aabb(const vector& bb_min, const vector& bb_max, const vector& test_pos); // 0x00735790 (:3859)
    int query_closest_subpiece_sphere(const vector& test_pos, const float radius); // 0x00735A70 (:3911)
    int query_obb_for_obstacles(const vector& test_pos, const matrix& test_orient, const vector& bb_min, const vector& bb_max, rfg_subpiece** havok_objects, int max_objects); // 0x00735980 (:3879)
    int query_sphere_for_obstacles(const vector& test_pos, const float radius, rfg_subpiece** havok_objects, int max_objects); // 0x00735B80 (:3929)
    virtual void update_render(rl_renderer* pRenderer); // vtbl+0x44 0x0073F3A0 (rfg_mover_render.cpp:497)
    virtual void update_render_opacity(); // vtbl+0x94 0x00717160 (:5234)
    virtual bool enable_object_outline(int layer); // vtbl+0x48 0x0071F8A0 (:5242)
    virtual bool disable_object_outline(); // vtbl+0x4C 0x00717180 (:5252)
    bool is_shatterable(unsigned int shape_key) const; // 0x00726FE0 (:4004)
    bool can_be_damaged_by_bullets(unsigned int shape_key) const; // 0x00727020 (:4016)
    void create_deferred_ri(); // 0x0073C910 (:4344)
    void update_render_subpieces(); // 0x00741680 (:4381)
    void do_frame_report(); // 0x0071F350 (:4174)
    void toggle_invulnerable(bool immediate); // 0x0071F930 (:5275)
    rfg_destroyable_shape* get_shape() const; // 0x005FB400 (.h:352)
    bool is_movable() const; // 0x00711090 (.h:357)
    void add_glass_object(glass_obj* glass_obj_p); // 0x00717390 (:5652)
    void remove_glass_object(glass_obj* glass_obj_p); // 0x00717400 (:5660)
    void maybe_repair_dlod(int dlod_index); // 0x0073CA20 (:4998)
    void transfer_decals_to_fader(rl_renderable_instance* fader_ri, unsigned int fader_uid); // 0x0071F830 (:5041)
    virtual void set_render_created(); // vtbl+0x9C 0x007171B0 (:5290)
    virtual void set_render_changed(); // vtbl+0xA0 0x00717210 (:5296)
    void maybe_flag_restore(); // 0x00717270 (:5307)
protected:
    virtual void free_this(); // vtbl+0x28 0x0072EB70 (:2906)
private:
    bool compress_state(rfg_destroyable_shape* shape, unsigned int chunk_destruction_crc); // 0x0071E9B0 (:2544)
    bool decompress_state(rfg_destroyable_shape* shape, unsigned int chunk_destruction_crc); // 0x0073C3F0 (:2725)
    void add_decals(); // 0x0071ECC0 (:2681)
    void delete_subpieces(unsigned short subpieces, int num_subpieces); // 0x0072F0B0 (:4456)
    void create_subpieces(unsigned short subpieces, int num_subpieces); // 0x0073D830 (:4612)
    void process_fading_subpieces(); // 0x00717140 (:4792)
    void repair_jittered_shards(int dlod_index); // 0x00717150 (:4864)
    void process_repair_events(); // 0x0073DE80 (:5058)
    void process_jitter(); // 0x0073E4A0 (rfg_mover_render.cpp:111)
    void update_damage_percent(); // 0x0071E8A0 (:1623)
    void calculate_structural_mass(float& base_structural_mass); // 0x0071E7E0 (:1599)
public:
    virtual ~rfg_mover(); // vtbl+0x00 0x0071E7B0

    friend class general_mover; // create_deferred_ri reads the donor's resource data (inferred)

private:
    rfg_move_type move_type; // 0xF8
    float damage_pct; // 0xFC
    float current_structural_mass; // 0x100
    rfg_mover_temporary_data* state_data; // 0x104
public:
    float game_destroyed_pct; // 0x108
    rfg_mover_flags flags; // 0x10C
    multi_rfg_mover_info* mp_info; // 0x110
    unsigned short explosion_event_index; // 0x114
    unsigned short damaged_subpiece_event_first_index; // 0x116
    unsigned short repair_event_list; // 0x118
    short stress_test_count; // 0x11A
    timestamp stress_test_delay; // 0x11C
    timestamp slice_delay; // 0x120
    rfg_mover* stress_next; // 0x124
    rfg_mover* stress_prev; // 0x128
    unsigned int restricted_area; // 0x12C
    int salvage_num_pieces; // 0x130
    unsigned int destruction_uid; // 0x134
    int damage_report_handle; // 0x138
    int stream_out_time; // 0x13C
    timestamp stress_sound_propagate_timer; // 0x140
    float accumulated_load_amount; // 0x144
    unsigned int process_shardify_candidate_index; // 0x148
    object_stream_resource<resource_dependent_data> rdd; // 0x14C
    unsigned int dead_smoke_plume_effect; // 0x158
    rfg_mover* pre_stream_next; // 0x15C
    rfg_mover* pre_stream_prev; // 0x160
    glass_obj* m_glass_obj_list; // 0x164
protected:
    timestamp damage_sound_propagate_timer; // 0x168
};

// size 0xC
class subpiece_collector : public hkpCdBodyPairCollector
{
public:
    subpiece_collector(); // 0x00716630 (:1041)
    virtual void addCdBodyPair(const hkpCdBody& bodyA, const hkpCdBody& bodyB); // vtbl+0x04 0x00740B10 (:1050)
private:
    void set_as_anchor(rfg_mover* rfgm, short subpiece_index, rfg_mover* other_rfgm, short other_subpiece_index); // 0x0073D540 (:1082)
public:
    virtual ~subpiece_collector(); // vtbl+0x00 0x00716650

    rfg_mover* new_anchor_list; // 0x08
};

// size 0xC
class obj_rfgm_props : public obj_mover_props
{
public:
    obj_rfgm_props(rfg_mover* old_mover, rfg_move_type initial_move_type, const char* chunk_name, rfg_destroyable_shape* new_shape, unsigned int destruction_uid); // 0x0071B0A0 (.h:446)
    obj_rfgm_props(const vector&, const matrix&, rfg_move_type, const char*, const char*, unsigned int, bool, bool, const char*, int, object_importance_level, human_teams, bool, char*);
    ~obj_rfgm_props(); // 0x00712CA0

};

extern float REGROW_MIN_PERCENT_DESTROYED; // 0x0165040C

void unused_dcf_rfg_dynamic_link_dump_info(); // 0x00725A40 (:245)
void unused_dcf_regrow_test(); // 0x0071E660 (:314)
rfg_dynamic_link* rfg_mover_get_dynamic_link(rfg_subpiece* sp); // 0x0071E700 (:1137)
rfg_dynamic_link* rfg_mover_get_dynamic_link(object_mover* movep); // 0x00725C50 (:1143)
bool rfg_mover_create_dynamic_link(rfg_mover* rm, unsigned short subpiece_index, object_mover* other_mp, unsigned short other_subpiece_index); // 0x0073BE40 (:1273)
void rfg_mover_delete_dynamic_link(object_mover* movep); // 0x00734D40 (:1402)
void rfg_mover_delete_dynamic_link(unsigned int handle); // 0x0072E890 (:1416)
void rfg_mover_delete_dynamic_link(rfg_mover* rm, rfg_subpiece* sp, bool force_delete); // 0x0072E940 (:1432)
int bpo_index_rfg_mover(); // 0x00716680 (:1593)
object* alloc_rfg_mover(); // 0x00725DD0 (:1593)
const char* classname_rfg_mover(); // 0x00716690 (:1593)
void types_rfg_mover(signed char& type, signed char& subtype); // 0x00716670 (:1593)
void rfg_mover_fade_in_finished(unsigned int owner, unsigned int uid); // 0x0073D740 (:4579)
void rfg_mover_system_init(); // 0x00741C60 (:5381)
void rfg_mover_level_init_post(); // 0x007172F0 (:5403)
void rfg_mover_level_shutdown(); // 0x00727200 (:5411)
void rfg_mover_set_thread_ownership(); // 0x00717300 (:5424)
void rfg_mover_do_frame_pre_sim(); // 0x0072F520 (:5430)
void validate_rfg_mover_destruction(const object* obj); // 0x0071FAF0 (:5458)
bool rfg_mover_is_world_anchor(const rfg_mover* rm); // 0x00717310 (:5474)
void rfg_mover_set_no_fade_destroy(rfg_mover* rm); // 0x00717330 (:5479)
bool rfg_mover_regrow_begin(rfg_mover* rm); // 0x00735E40 (:5485)
void rfg_mover_regrow_finish(rfg_mover* rm); // 0x00736160 (:5557)
void rfg_mover_init_damage_applied(); // 0x00717340 (:5573)
bool rfg_mover_get_damage_applied(); // 0x00717350 (:5578)
void rfg_mover_rebuild(rfg_mover* rm); // 0x00717360 (:5583)
