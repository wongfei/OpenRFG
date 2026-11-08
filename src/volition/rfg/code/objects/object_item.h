// Original: volition/rfg/code/objects/object_item.h
// Implementation: volition/rfg/code/objects/object_item.cpp
//
// item, a physical pickup or prop object (object type OT_ITEM; weapons and projectiles derive from it), and item_info,
// the table entry of an item class (items.xtbl).
// - item_info: meshes, rig and animation set, the streamed container (srid, and high_res_srid for the high resolution
//   weapon textures, GSC_WEAPON_*), physics, pickup behaviour (touch callback) and up to 6 props (prop_type slots).
// - item: its item_info, renderable instance (stream resource), damage info, owner/holder, attachment and fading
//   state. object_item_process / _postsim / _post_transform are the per-frame passes of gamestate_gp_simulate_frame.
// Placement of item_info, prop_type and the item flag structs inferred.
#pragma once

#include "volition/rfg/code/multi/multi_game/multi_game_sim_state.h"
#include "volition/rfg/code/objects/level_object_properties_shared.h"
#include "volition/rfg/code/objects/obj_enums.h"
#include "volition/rfg/code/objects/object.h"
#include "volition/rfg/code/objects/object_render_distance.h"
#include "volition/rfg/code/objects/object_type.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/time/timestamp.h"

class character_instance;
class checksum_stri;
class cloth_simulation;
struct explosion_info;
class hkpWorldObject;
class human;
struct inv_item_info;
class item;
struct material;
class material_effect_parameters;
class matrix;
class mempool;
struct physics_create_info;
class player;
class rl_mesh_instance;
class rl_renderer;
struct static_mesh;
struct weapon_fire_info;
struct weapon_info;
struct xml_element;

enum prop_type
{
    PROP_TYPE_NONE = -1,
    PROP_TYPE_AMMO = 0,
    PROP_TYPE_CLIP = 1,
    PROP_TYPE_ACCESSORY1 = 2,
    PROP_TYPE_SCOPE = 3,
    PROP_TYPE_SILENCER = 4,
    PROP_TYPE_CODE_DESIGNATED = 5,
    PROP_TYPE_HEAD_PROP = 6,
    PROP_TYPE_GRENADE_1 = 7,
    PROP_TYPE_GRENADE_2 = 8,
    PROP_TYPE_GRENADE_3 = 9,
    PROP_TYPE_ACCESSORY_MISC = 10,
    PROP_TYPE_SATCHEL_CHARGE = 11,
    PROP_TYPE_EYE_WEAR = 12,
    PROP_TYPE_TORSO = 13,
    PROP_TYPE_EARRING1 = 14,
    PROP_TYPE_EARRING2 = 15,
    PROP_TYPE_WAIST = 16,
    PROP_TYPE_JETPACK = 17,
    PROP_TYPE_MP_FLAG = 18,
    PROP_TYPE_BACK = 19,
    NUM_PROP_TYPES = 20,
};

// size 0xF0
struct item_info
{
    // size 0x10
    struct item_prop
    {
        char* prop_name; // 0x00
        char* prop_tag; // 0x04
        prop_type type; // 0x08
        struct
        {
            unsigned int offhand : 1; // 0x00:0
        } flags; // 0x0C
    };

    item_info(); // 0x00A19920

    char* name; // 0x00
    char* smesh_name; // 0x04
    char* cmesh_name; // 0x08
    char* rig_name; // 0x0C
    char* anim_set_name; // 0x10
    char* pickup_smesh_name; // 0x14
    unsigned int srid; // 0x18
    unsigned int high_res_srid; // 0x1C
    bool multiplayer_only; // 0x20
    bool shared_backpack; // 0x21
    int flags; // 0x24
    bool no_peg_load; // 0x28
    unsigned int material_effect_id; // 0x2C
    int num_props; // 0x30
    item_prop props[6]; // 0x34
    struct
    {
        bool high_res_texture_requested : 1; // 0x00:0
        bool high_res_texture_loaded : 1; // 0x00:1
        bool high_res_texture_request_unload : 1; // 0x00:2
    } special_flags; // 0x94
    material* high_res_material; // 0x98
    float mass; // 0x9C
    float linear_damping; // 0xA0
    float angular_damping; // 0xA4
    float restitution; // 0xA8
    float friction; // 0xAC
    vector angular_velocity; // 0xB0
    int respawn_delay; // 0xBC
    float touch_arg1; // 0xC0
    float touch_arg2; // 0xC4
    bool (*touch)(human*, item*, bool&, bool&); // 0xC8
    int damage_flags; // 0xCC
    int hitpoints; // 0xD0
    int pickup_snd; // 0xD4
    int pickup_snd_2; // 0xD8
    int spin_time; // 0xDC
    int m_foley_collision; // 0xE0
    int cloth_foley; // 0xE4
    inv_item_info* safehouse_weapon; // 0xE8
    const explosion_info* m_explosion_info; // 0xEC
};

// size 0x4
struct obj_item_flags
{
    obj_item_flags(); // 0x005DC630 (.h:109)

    unsigned int respawn : 1; // 0x00:0
    unsigned int no_physics : 1; // 0x00:1
    unsigned int simulate : 1; // 0x00:2
    unsigned int props_created : 1; // 0x00:3
    unsigned int item_held : 1; // 0x00:4
    unsigned int no_save : 1; // 0x00:5
    unsigned int preplaced : 1; // 0x00:6
    unsigned int update_attach_point : 1; // 0x00:7
    unsigned int dropped_by_player : 1; // 0x00:8
    unsigned int touched_last_frame : 1; // 0x00:9
    unsigned int character_prop : 1; // 0x00:10
    unsigned int attached_cloth_sim : 1; // 0x00:11
    unsigned int doing_errode : 1; // 0x00:12
    unsigned int cast_shadows : 1; // 0x00:13
    unsigned int cast_drop_shadows : 1; // 0x00:14
    unsigned int xray_visible : 1; // 0x00:15
    unsigned int dont_dampen_impulse : 1; // 0x00:16
    unsigned int shield_initialized : 1; // 0x00:17
};

// size 0x4
struct special_purpose_flags
{
    unsigned int is_ammo_box : 1; // 0x00:0
    unsigned int use_enabled : 1; // 0x00:1
    unsigned int arc_welder_is_arcing : 1; // 0x00:2
    unsigned int fading_from_nano : 1; // 0x00:3
    unsigned int using_high_res_texture : 1; // 0x00:4
};

// size 0x8
struct item_damage_info
{
    int hitpoints; // 0x00
    struct
    {
        unsigned int can_explode : 1; // 0x00:0
        unsigned int damaged_by_bullets : 1; // 0x00:1
        unsigned int damaged_by_collisions : 1; // 0x00:2
        unsigned int damaged_by_explosions : 1; // 0x00:3
        unsigned int damaged_by_melee : 1; // 0x00:4
        unsigned int damaged_by_vehicles : 1; // 0x00:5
    } flags; // 0x04
};

// size 0x140
class item : public object
{
public:
    // size 0x10
    struct resource_dependent_data
    {
        static_mesh* smesh; // 0x00
        rl_mesh_instance* smesh_instance_p; // 0x04
        static_mesh* pickup_smesh; // 0x08
        rl_mesh_instance* pickup_smesh_instance_p; // 0x0C
    };

    item(); // 0x00A18F90 (:2806)
    static const object_type get_type_static(); // 0x006E17C0 (.h:165)
    static const object_sub_type get_subtype_static(); // 0x006E17D0 (.h:165)
    static const char* get_classname_static(); // 0x00A05E10 (.h:165)
    static int get_bpo_index_static(); // 0x00756BB0 (.h:165)
    virtual const char* get_classname() const; // vtbl+0x0C 0x00A18FF0 (.h:165)
    static unsigned int get_classname_hash(); // 0x00A18F80 (:2805)
    static int pool_remaining(); // 0x00A3C100 (:2805)
    virtual void pool_free(); // vtbl+0x80 0x00A91480 (:2805)
    void set_casts_transparent_shadows_plus_child_items(bool do_cast); // 0x00A3C830 (:4071)
    void set_opacity_plus_child_items(float alpha); // 0x00A65F80 (:4087)
    void set_stealth_plus_child_items(float percent); // 0x00A3C870 (:4105)
    virtual bool serialize(obj_prop_block& pblock, bool load); // vtbl+0x14 0x00AA7B90 (:2818)
    virtual obj_serialize_mode serialize_mode(); // vtbl+0x18 0x00A19000 (:3029)
    virtual bool stream(bool load); // vtbl+0x1C 0x00AA82E0 (:3046)
    virtual void hierarchy_notify(hierarchy_notify_code code); // vtbl+0x70 0x00A19010 (:3319)
    virtual bool is_owned() const; // vtbl+0x84 0x00A19030 (:3326)
    unsigned int get_owner_handle() const; // 0x005DC650 (.h:256)
    void enable_physics(); // 0x00A65F70 (:4023)
    void disable_physics(); // 0x00A19250 (:4031)
    void set_owner(unsigned int owner, bool update_other_attributes); // 0x00A916D0 (:4039)
    void unset_owner(bool update_other_attributes); // 0x00A91730 (:4054)
    bool is_offscreen() const; // 0x00A64AC0 (:2106)
    float get_bounding_radius() const; // 0x00A3C290 (:3562)
    bool stream_in_high_res(); // 0x00A3C080 (:2725)
    bool stream_out_high_res(); // 0x00A18F20 (:2759)
    virtual void get_bbox(vector& bmin, vector& bmax) const; // vtbl+0x04 0x00A19040 (:3629)
    virtual void get_local_bbox(vector& bmin, vector& bmax) const; // vtbl+0x08 0x00A65B60 (:3641)
    virtual void update_render(rl_renderer*); // vtbl+0x44 0x00A65190 (:3331)
    bool should_render_pickup_smesh() const; // 0x00A3C130 (:3497)
    virtual bool enable_object_outline(int layer); // vtbl+0x48 0x00A3C150 (:3512)
    virtual bool disable_object_outline(); // vtbl+0x4C 0x00A3C210 (:3537)
    virtual int get_tag_index_from_name(const char* tag_name) const; // vtbl+0x34 0x00A3C2F0 (:3686)
    virtual bool get_tag_world_coords(int tag_index, vector& tag_pos, matrix* tag_orient, vector* obj_pos, matrix* obj_orient) const; // vtbl+0x38 0x00A3C390 (:3749)
    virtual bool get_tag_local_coords(int tag_index, vector& tag_pos, matrix* tag_orient) const; // vtbl+0x3C 0x00A3C510 (:3784)
    virtual void hold_item(human* hp, bool held); // vtbl+0x88 0x00AA8E20 (:3810)
    virtual bool apply_damage(float damage, damaged_by_types from_type, const vector& hit_pos, const vector& hit_normal, const object* source, unsigned int hit_shape_key, bool from_server); // vtbl+0x8C 0x00A91660 (:3897)
    bool is_shield() const; // 0x00A05E20 (.h:294)
    bool is_personal_shield() const;
    bool is_virtual_shield() const; // 0x00A05E40 (.h:296)
    vector get_shield_normal() const; // 0x00A05E50 (.h:297)
    virtual void process_bullet_hit(human* attacker, float damage, const weapon_fire_info& fire_info); // vtbl+0x68 0x00A19090 (:3918)
    virtual bool process_melee_hit(object* attacker, float damage, const vector& impulse, const vector& hit_pos, const vector& hit_normal, unsigned int shape_key, const weapon_info& w_info); // vtbl+0x6C 0x00A3C5E0 (:3938)
    virtual float get_energy_scale(); // vtbl+0x64 0x00A191F0 (:3970)
    void start_nano_rifle_fade_out(material_effect_parameters& effect_params, const int fading_material_effect); // 0x00A3C780 (:3984)
    virtual unsigned char get_physical_material(unsigned int shape_key, const vector* contact_point, const hkpWorldObject* hk_objp) const; // vtbl+0x5C 0x00A19230 (:4010)
    virtual unsigned char get_effect_material(unsigned int shape_key, const vector* contact_point, const hkpWorldObject* hk_objp) const; // vtbl+0x60 0x00A19240 (:4018)
    void respawn(); // 0x00A912E0 (:2666)
    bool waiting_to_respawn() const; // 0x00A18E70 (:2678)
    bool should_respawn() const; // 0x00A18E90 (:2696)
    void update_animlib(float delta_time, bool force_transform); // 0x00A64C30 (:2141)
    void explode(); // 0x00A91510 (:3848)
protected:
    virtual void free_this(); // vtbl+0x28 0x00A91490 (:3592)
public:
    virtual ~item(); // vtbl+0x00 0x009587C0

    item* prev; // 0x9C
    item* next; // 0xA0
    item_info* info; // 0xA4
    object_stream_resource<resource_dependent_data> rdd; // 0xA8
    character_instance* char_inst; // 0xBC
    float render_alpha; // 0xC0
    float m_stealth_percent; // 0xC4
    bool casts_transparent_shadows; // 0xC8
    object_render_distance render_distance; // 0xCC
    float touch_arg1_override; // 0xD4
    float touch_arg2_override; // 0xD8
    float mass_override; // 0xDC
    unsigned int collision_type; // 0xE0
    prop_type m_prop_type; // 0xE4
    timestamp deletion_timer; // 0xE8
    timestamp respawn_timer; // 0xEC
    timestamp touch_delay_timer; // 0xF0
    timestamp cloth_deletion_timer; // 0xF4
    timestamp dropped_timer; // 0xF8
    unsigned int m_nav_cell_detour_request_handle; // 0xFC
    obj_item_flags item_flags; // 0x100
    vector last_pos; // 0x104
    unsigned int srid; // 0x110
    cloth_simulation* cloth_sim; // 0x114
    salvage_material_type salvage_material; // 0x118
    unsigned int salvage_effect; // 0x11C
    int m_foley_collision_instance; // 0x120
    multi_sim_state<multi_item_state>* mp_sim_state; // 0x124
    float pickup_value; // 0x128
    special_purpose_flags special_flags; // 0x12C
    item_damage_info damage_info; // 0x130
    bool default_batchable_setting; // 0x138
    unsigned char xray_material; // 0x139
    unsigned char xray_render_alpha; // 0x13A
    // protected in rfg.pdb; written directly by human.cpp's inventory and prop code (friends are not in the PDB).
    unsigned int owner_handle; // 0x13C
};

// size 0xC
class obj_item_props : public obj_prop_block
{
public:
    obj_item_props(const item_info* info, unsigned int srid, const vector& pos, const matrix& orient, const obj_item_flags& flags, unsigned int collision_type, float mass_override); // 0x00A3BE70 (:1704)
    obj_item_props(const item_info* info, const vector& pos, const matrix& orient, const obj_item_flags& flags, unsigned int collision_type, float mass_override, salvage_material_type salvage_material); // 0x00A3BD80 (:1686)
    obj_item_props(const char* item_info_name, const vector& pos, const matrix& orient, const obj_item_flags& flags, unsigned int collision_type, float mass_override, salvage_material_type salvage_material); // 0x00A3BCA0 (:1668)
    ~obj_item_props(); // 0x005DC830

};

extern item_info* Item_infos; // 0x0302F148
extern int Max_item_infos; // 0x0302F144
extern int Num_item_infos; // 0x03017C5C
extern int Num_large_high_res_weapons_loaded; // 0x0302F14C
extern int Num_high_res_weapons_loaded; // 0x0302F150
extern item_info* High_res_weapons_loaded[4]; // 0x03024E74

void unused_dcf_drop_item(); // 0x00A63C20 (:238)
void unused_dcf_high_resolution_weapons(); // 0x00A63F30 (:307)
void mp_preload_object_items(); // 0x00A909A0 (:423)
void mp_unload_object_items(); // 0x00A182C0 (:456)
void item_parse_table_element(xml_element* item, item_info* infop, mempool* pool); // 0x00A3B710 (:1511)
void item_stream_load(unsigned int srid); // 0x00A3BF60 (:1719)
void item_stream_unload(unsigned int srid); // 0x00A3C000 (:1753)
void item_high_res_stream_load(unsigned int srid); // 0x00A648B0 (:1779)
void item_high_res_stream_unload(unsigned int srid); // 0x00A649E0 (:1827)
void mp_process_item_for_delete(item* itemp, int& mp_sync_item_count, int& mp_oldest_sync_item_age, item*& mp_oldest_sync_item); // 0x00A18CA0 (:1885)
void object_item_process(); // 0x00AA75E0 (:1919)
void object_item_process_post_transform(); // 0x00A64EE0 (:2243)
void object_item_process_postsim(); // 0x00A65030 (:2274)
item_info* object_item_get_info(const char* name); // 0x00A18D30 (:2421)
item_info* object_item_get_info(checksum_stri name_crc); // 0x00A18D80 (:2439)
item_info* object_item_get_info(unsigned int srid); // 0x00A18DE0 (:2450)
int object_item_get_info_index(const char* name); // 0x00A18E20 (:2468)
bool object_item_player_can_auto_pickup(player* pp, const item* itemp); // 0x00A650A0 (:2488)
bool object_item_player_close_enough_to_touch(player* pp, const item* itemp, bool active); // 0x00A90E80 (:2526)
bool object_item_player_touch(player* pp, item* itemp); // 0x00A91180 (:2608)
void object_item_destroy_all_dropped_items(); // 0x00A65120 (:2770)
void object_item_stick_to_ground(item* itemp, vector& start_pos); // 0x00A91320 (:2781)
void types_item(signed char& type, signed char& subtype); // 0x00A18F50 (:2805)
object* alloc_item(); // 0x00A65180 (:2805)
int bpo_index_item(); // 0x00A18F60 (:2805)
const char* classname_item(); // 0x00A18F70 (:2805)
void item_render_all_debug(); // 0x00A19280 (:4067)
void item_process_explosion_hit(item* ip, unsigned int damage, const vector& linear_impulse, const vector& angular_impulse); // 0x00A3C8C0 (:4123)
void object_item_system_init(); // 0x00A91780 (:4192)
void object_item_system_shutdown(); // 0x00A19290 (:4213)
void object_item_level_init_post(); // 0x00A192B0 (:4226)
void object_item_process_high_res_textures(); // 0x00A192C0 (:4232)
void object_item_unload_all_high_res(); // 0x00A19350 (:4252)
void item_calc_collision_params(physics_create_info& pci, const item_info* i_info, vector& local_bmin, vector& local_bmax); // 0x00A19380 (:4270)
unsigned int item_smesh_get_collision_model(static_mesh* smesh, const physics_create_info& pci); // 0x00A196D0 (:4310)
void object_item_level_shutdown(); // 0x00A66000 (:4355)

// 0x005DC630 (.h:109): shadows and simulation on, everything else off.
inline obj_item_flags::obj_item_flags()
{
    respawn = 0;
    shield_initialized = 0;
    dont_dampen_impulse = 0;
    xray_visible = 0;
    cast_drop_shadows = 1;
    cast_shadows = 1;
    doing_errode = 0;
    attached_cloth_sim = 0;
    character_prop = 0;
    touched_last_frame = 0;
    dropped_by_player = 0;
    update_attach_point = 0;
    preplaced = 0;
    no_save = 0;
    item_held = 0;
    props_created = 0;
    simulate = 1;
    no_physics = 0;
}

// 0x005DC650 (.h:256)
inline unsigned int item::get_owner_handle() const
{
    return owner_handle;
}
