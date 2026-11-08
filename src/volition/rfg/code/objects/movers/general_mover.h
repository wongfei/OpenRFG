// Original: volition/rfg/code/objects/movers/general_mover.h
// Implementation: volition/rfg/code/objects/movers/general_mover.cpp
//
// general_mover: a movable or destructible prop from a .cchk_pc chunk (rfg_destroyable_shape plus a renderable
// instance as stream resource). gobj_index indexes the general objects of the chunk; original_object is the handle the
// mover was split from. collision_type and move_type share one field (anonymous union). In multiplayer mp_sim_state
// holds the received states.
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/query/hkpcdbodypaircollector.h"
#include "volition/rfg/code/multi/multi_game/multi_game_sim_state.h"
#include "volition/rfg/code/objects/level_object_properties_shared.h"
#include "volition/rfg/code/objects/movers/object_mover.h"
#include "volition/rfg/code/objects/obj_enums.h"
#include "volition/rfg/code/objects/object.h"
#include "volition/rfg/code/objects/object_type.h"
#include "volition/vlib/code/time/timestamp.h"

struct c_chunk_base;
class color;
struct explosion_info;
class hkpCdBody;
class matrix;
class rfg_destroyable_shape;
struct rfg_subpiece;
class rl_renderable_instance;
class rl_renderer;
class vector;

// size 0x130
class general_mover : public object_mover
{
public:
    // size 0x8
    struct resource_dependent_data
    {
        rfg_destroyable_shape* shape; // 0x00
        rl_renderable_instance* model_instance_p; // 0x04
    };

    general_mover(); // 0x00A37EA0 (:439)
    static const object_type get_type_static(); // 0x006E17A0 (.h:44)
    static const object_sub_type get_subtype_static(); // 0x006E17B0 (.h:44)
    static const char* get_classname_static(); // 0x00A05DE0 (.h:44)
    static int get_bpo_index_static(); // 0x00756B90 (.h:44)
    virtual const char* get_classname() const; // vtbl+0x0C 0x00A37EE0 (.h:44)
    static unsigned int get_classname_hash(); // 0x00A130D0 (:438)
    static int pool_remaining(); // 0x00A37E90 (:438)
    virtual void pool_free(); // vtbl+0x80 0x00A8A940 (:438)
    virtual bool serialize(obj_prop_block& pblock, bool load); // vtbl+0x14 0x00AE4E90 (:451)
    virtual obj_serialize_mode serialize_mode(); // vtbl+0x18 0x00A37F20 (:593)
    virtual bool stream(bool load); // vtbl+0x1C 0x00AA1970 (:666)
protected:
    virtual void free_this(); // vtbl+0x28 0x00AB37E0 (:954)
    virtual rl_renderable_instance* get_render_model_instance(); // vtbl+0xA8 0x00A38080 (:1470)
    virtual void script_point_destroy(int idx); // vtbl+0xAC 0x00A13180 (:1456)
public:
    virtual void update_render(rl_renderer*); // vtbl+0x44 0x00A5EBA0 (:1519)
    void create_deferred_ri(); // 0x00AA2200 (:1549)
    void setup_nano_dissolve(); // 0x00AA1FF0 (:1475)
    virtual void update_render_opacity(); // vtbl+0x94 0x00AB38C0 (:1588)
    virtual bool enable_object_outline(int layer); // vtbl+0x48 0x00A380A0 (:1622)
    virtual bool disable_object_outline(); // vtbl+0x4C 0x00A131C0 (:1632)
    virtual void get_bbox(vector& bmin, vector& bmax) const; // vtbl+0x04 0x00A130F0 (:1361)
    virtual void get_local_bbox(vector& bmin, vector& bmax) const; // vtbl+0x08 0x00A5E840 (:1372)
    virtual bool mover_should_sync(); // vtbl+0x84 0x00A131E0 (:1642)
    virtual void update_pos_and_orient(const vector& update_pos, const matrix& update_orient, bool set_havok_data); // vtbl+0x30 0x00AB3860 (:1386)
    virtual void update_world_bounding_box(const vector& update_pos, const matrix& update_orient); // vtbl+0xB0 0x00A13130 (:1394)
    virtual bool get_tag_local_coords(int tag_index, vector& tag_pos, matrix* tag_orient) const; // vtbl+0x3C 0x00A5EA40 (:1407)
    virtual bool is_killed() const; // vtbl+0x10 0x00A130E0 (:1172)
    virtual bool apply_damage(float damage, damaged_by_types from_type, const vector& hit_pos, const vector& hit_normal, const object* source, unsigned int hit_shape_key, bool from_server); // vtbl+0x88 0x00A8ACF0 (:1183)
    virtual bool find_anchor_points(); // vtbl+0x8C 0x00A37FB0 (:1318)
    virtual bool debug_render_havok(const vector& center, const matrix& orientp, color c); // vtbl+0xB4 0x00A13210 (:1660)
    virtual int time_since_last_damage() const; // vtbl+0x98 0x00A13220 (:1682)
    virtual float percent_destroyed() const; // vtbl+0xA4 0x00A13240 (:1691)
    void do_frame_pre_sim(); // 0x00A8A950 (:999)
    bool setup_prefix_effect(); // 0x00A5EC30 (:1706)
    void process_prefix_effect(); // 0x00A380E0 (:1776)
    const explosion_info* get_death_explosion_info_pos_and_orient(int i, vector& exp_pos, matrix* exp_orient) const; // 0x00A5EDC0 (:1803)
    bool explodes_on_death() const; // 0x00A132D0 (:1850)
    rl_renderable_instance* get_rl_model_instance() const; // 0x00A37F90 (:1159)
    virtual void set_render_created(); // vtbl+0x9C 0x00A132E0 (:1855)
    unsigned int get_original_object() const; // 0x00756BA0 (.h:155)
    virtual ~general_mover(); // vtbl+0x00 0x00A37EF0

    unsigned int gm_flags; // 0xF8
    union
    {
        unsigned int collision_type; // 0xFC
        int move_type; // 0xFC
    };
    unsigned int subpiece_index; // 0x100
    unsigned int name_checksum; // 0x104
    int gobj_index; // 0x108
    multi_sim_state<multi_mover_state>* mp_sim_state; // 0x10C
    unsigned int destruction_uid; // 0x110
    object_stream_resource<resource_dependent_data> rdd; // 0x114
    general_mover* pre_stream_next; // 0x120
    general_mover* pre_stream_prev; // 0x124
private:
    unsigned int original_object; // 0x128
    timestamp last_damaged_timer; // 0x12C
};

// size 0xC
class anchor_collector : public hkpCdBodyPairCollector
{
public:
    anchor_collector(); // 0x00A13050 (:134)
    virtual void addCdBodyPair(const hkpCdBody& bodyA, const hkpCdBody& bodyB); // vtbl+0x04 0x00A37BD0 (:142)
    virtual ~anchor_collector(); // vtbl+0x00 0x00A13070

    bool has_hit; // 0x08
};

// size 0xC
class gm_prop_block : public obj_mover_props
{
public:
    gm_prop_block(const vector& pos, const matrix& orient, unsigned int chunk_uid, unsigned int chunk_obj_uid, c_chunk_base* chunk_base, const char* base_prop_name, rfg_subpiece* sp, rfg_destroyable_shape* shape, int move_type, unsigned int destruction_uid, bool cast_shadows, bool cast_drop_shadows, unsigned int original_object); // 0x0071AED0 (.h:197)
    gm_prop_block(const vector& pos, const matrix& orient, const char* chunk_name, unsigned int chunk_obj_uid, const char* base_prop_name, const char* name, unsigned int collision_type, bool cast_shadows, bool cast_drop_shadows, int mover_type_bit_flags); // 0x00A51800 (.h:172)
    ~gm_prop_block(); // 0x00712980

};

int bpo_index_general_mover(); // 0x00A130B0 (:438)
void types_general_mover(signed char& type, signed char& subtype); // 0x00A130A0 (:438)
const char* classname_general_mover(); // 0x00A130C0 (:438)
object* alloc_general_mover(); // 0x00A5E830 (:438)
void general_mover_system_init(); // 0x00AB39D0 (:1862)
void general_mover_level_init_post(); // 0x00A13340 (:1869)
void general_mover_level_shutdown(); // 0x00A5EF90 (:1878)
void general_mover_do_frame_pre_sim(); // 0x00A8B0E0 (:1889)
void general_mover_set_movable(general_mover* mover, bool movable); // 0x00A13360 (:1901)
void validate_general_mover_destruction(const object* obj); // 0x00A38170 (:1921)
