// Original: volition/rfg/code/effects/object_effect.h
// Implementation: volition/rfg/code/effects/object_effect.cpp
//
// object_effect, a placed or spawned effect object (object type OT_EFFECT): a visual effect instance and sound,
// attached or at a position, with expression variables (registered_expression_variable, from a pool of 3072) that
// drive effect parameters. object_effect_process_all advances them once per frame. Placement of the enums inferred.
#pragma once

#include "volition/render_lib/code/linked_list_p.h"
#include "volition/rfg/code/objects/obj_enums.h"
#include "volition/rfg/code/objects/object.h"
#include "volition/rfg/code/objects/object_type.h"
#include "volition/vlib/code/math/bbox.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/memory/object_pool.h"

struct effect_info;
class mempool;
class rl_color_float;
class rl_handle;
class rl_renderer;
class vfx_effect;
struct tracer;
enum weapon_tracer_type; // rfg's tracers.h, for the tracers' friend declarations (MSVC opaque enum, as in the Havok headers)

enum object_effect_category
{
    EFFECTS_CATEGORY_NONE = 0,
    EFFECTS_CATEGORY_GLASS = 1,
    EFFECTS_CATEGORY_EXPLOSION = 2,
    EFFECTS_CATEGORY_MAX = 3,
};

enum object_effect_expression_variable_e
{
    EFFECTS_VAR_OBJECT_SPEED = 0,
    EFFECTS_VAR_OBJECT_SIZE = 1,
    EFFECTS_VAR_RANDOM_0_TO_1 = 2,
    EFFECTS_VAR_WIND_SPEED = 3,
    EFFECTS_VAR_WIND_LOS = 4,
    EFFECTS_VAR_JETPACK_FUEL_PERCENT = 5,
    EFFECTS_VAR_BACKPACK_FUEL_COLOR = 6,
    EFFECTS_VAR_BACKPACK_FUEL_LIFE = 7,
    EFFECTS_VAR_VEHICLE_SMOKE_PERCENT = 8,
    EFFECTS_VAR_WEAPON_HEAT_PERCENT = 9,
    EFFECTS_VAR_PERCENT = 10,
    EFFECTS_VAR_PARTICLE_LOAD = 11,
    EFFECTS_VAR_SCALE_X = 12,
    EFFECTS_VAR_SCALE_Y = 13,
    EFFECTS_VAR_COUNT = 14,
};

// size 0x184
class object_effect : public object
{
public:
    enum stop_styles
    {
        SS_INVALID = -1,
        SS_STOP_LOOPING = 0,
        SS_STOP_EMITTING = 1,
        SS_NUM_STOP_STYLES = 2,
    };

    typedef void (*update_callback)(object_effect*, void*);
    typedef void (*update_render_callback)(object_effect*);

    // size 0xC
    class props : public obj_prop_block
    {
    public:
        props(unsigned int visual_handle, int sound_handle, int sound_alr_handle, const vector& _position, const matrix& _orient, bool looping, unsigned int host_handle, int host_tag, unsigned int source_handle, float volume, bool attach_to_camera); // 0x00744EA0 (.h:86)
        ~props(); // 0x00742890

    };

    // size 0x14
    struct registered_expression_variable
    {
        registered_expression_variable* get_prev() const;
        void set_prev(registered_expression_variable*);
        registered_expression_variable* get_next() const;
        void set_next(registered_expression_variable*);

        registered_expression_variable* prev; // 0x00
        registered_expression_variable* next; // 0x04
        object_effect_expression_variable_e m_index; // 0x08
        unsigned int m_crc; // 0x0C
        float m_value; // 0x10
    };

    // size 0x4
    struct resource_dependent_data
    {
        vfx_effect* vfx_effect_p; // 0x00
    };

    // size 0x44
    struct visual_effect_object
    {
        visual_effect_object(); // 0x00743C70

        unsigned int handle; // 0x00
        bool looping; // 0x04
        bool do_update_render; // 0x05
        bool kill_particles_on_stop; // 0x06
        float kill_particles_fade_time_s; // 0x08
        float kill_particles_rem_fade_time_s; // 0x0C
        bbox bbox_initial; // 0x10
        bbox bbox_current; // 0x28
        unsigned int flags; // 0x40
    };

    // size 0x18
    struct sound_effect_object
    {
        int cue_id; // 0x00
        int alr_cue_id; // 0x04
        int play_id; // 0x08
        int alr_id; // 0x0C
        float volume; // 0x10
        bool kill_oneshot_when_done; // 0x14
        bool follow_effect_pos; // 0x15
    };

    object_effect(); // 0x00746160 (:105)
    static const object_type get_type_static() { return OT_EFFECT; } // 0x006C7D20 (.h:68)
    static const object_sub_type get_subtype_static() { return OT_SUB_UNDEFINED; } // 0x006C7D30 (.h:68)
    static const char* get_classname_static(); // 0x007422A0 (.h:68)
    static int get_bpo_index_static(); // 0x007422B0 (.h:68)
    virtual const char* get_classname() const; // vtbl+0x0C 0x00746190 (.h:68)
    static unsigned int get_classname_hash(); // 0x00743C60 (:104)
    static int pool_remaining(); // 0x00746150 (:104)
    virtual void pool_free(); // vtbl+0x80 0x0074BEF0 (:104)
    virtual bool serialize(obj_prop_block& pblock, bool load); // vtbl+0x14 0x007494C0 (:225)
    virtual obj_serialize_mode serialize_mode(); // vtbl+0x18 0x00743C80 (:354)
    virtual bool stream(bool load); // vtbl+0x1C 0x0074D060 (:370)
    vfx_effect* get_vfx_effect() const; // 0x006CF300 (.h:292)
    void update(float delta_time); // 0x00749D30 (:725)
    void stop(stop_styles style); // 0x007465A0 (:902)
    virtual void update_render(rl_renderer* pRenderer); // vtbl+0x44 0x0074A490 (:964)
    void render_debug(); // 0x005D4670 (object_effect_debug.cpp:423)
    void render_step_debug(); // 0x005D7190 (object_effect_debug.cpp:266)
    void attach_to_host(unsigned int new_host_handle, int tag_handle); // 0x00746430 (:570)
    void deattach_from_host(); // 0x00743C90 (:607)
    void attach_to_camera(); // 0x00743F50 (:1098)
    void attach_to_camera_orient(); // 0x00743F60 (:1103)
    float get_visual_radius() const; // 0x00743EC0 (:1030)
    virtual void get_bbox(vector& bmin, vector& bmax) const; // vtbl+0x04 0x007466B0 (:1041)
    virtual void get_local_bbox(vector& bmin, vector& bmax) const; // vtbl+0x08 0x00743EF0 (:1066)
    void set_expression_variable(object_effect_expression_variable_e variable_e, float value); // 0x0074A870 (:1108)
    virtual void hierarchy_notify(hierarchy_notify_code code); // vtbl+0x70 0x0074A920 (:1128)
    void handle_host_death(); // 0x00746860 (:1135)
    const vector& get_local_position() const;
    void set_local_position(const vector& local_position); // 0x00710E30 (.h:381)
    const matrix& get_local_orient() const;
    void set_local_orient(const matrix& local_orient); // 0x00710E50 (.h:392)
    void (*get_pre_update_cb() const)(object_effect*, void*);
    void set_pre_update_cb(void (*pre_update_cb)(object_effect*, void*)); // 0x00BD9120 (.h:307)
    void (*get_post_update_cb() const)(object_effect*, void*);
    void set_post_update_cb(void (*)(object_effect*, void*));
    void (*get_post_update_render_cb() const)(object_effect*);
    void set_post_update_render_cb(void (*post_update_render_cb)(object_effect*)); // 0x00B1FC00 (.h:328)
    void* get_update_cb_data() const;
    void set_update_cb_data(void* update_cb_data_p); // 0x00BD9130 (.h:338)
    float get_custom_cb_data() const; // 0x00B1FC10 (.h:343)
    void set_custom_cb_data(float custom_cb_data_p); // 0x00B1FC20 (.h:348)
    bool get_update_expressions() const;
    void set_update_expressions(bool);
    int get_skybox_attached_orbital() const;
    void set_skybox_attached_orbital(int skybox_attached_orbital); // 0x007B9EB0 (.h:408)
    effect_info* get_effect_info() const;
    void set_effect_info(effect_info* effect_info_p); // 0x007422C0 (.h:371)
    void set_material_tint(const rl_color_float& material_tint); // 0x00746880 (:1142)
    unsigned int get_visual_effect_info_handle(); // 0x007422D0 (.h:397)
    void set_category(object_effect_category category); // 0x00743F70 (:1150)
    object_effect_category get_category() const; // 0x0091CF80 (.h:191)
    static int get_num_effects_in_category(object_effect_category category); // 0x00743FA0 (:1161)
protected:
    virtual void free_this(); // vtbl+0x28 0x0074C010 (:1091)
private:
    void update_expressions(); // 0x007499B0 (:641)
    void update_pos_and_orient_internal(); // 0x00743CB0 (:618)
    void reset(); // 0x007461C0 (:174)
    void release(); // 0x0074BF00 (:499)
    void visual_create(); // 0x007462F0 (:538)
    void visual_release(); // 0x007463E0 (:558)
    void process_lod(); // 0x00749A50 (:668)
public:
    virtual ~object_effect(); // vtbl+0x00 0x007461A0

private:
    // weapons.cpp's muzzle smoke callback reads the visual (friends are not recorded in rfg.pdb).
    friend void muzzle_smoke_update_effect_cb(object_effect* effect_p, void* data_p);
    // grinder.cpp's grinder_update_render and weapons.cpp's arc beam read the visual too.
    friend class weapon;
    friend void weapon_create_arc_effect(unsigned int effect, const vector& start_pos, const vector& end_pos, const vector& muzzle_normal);
    friend void weapon_create_curved_tracer(unsigned int effect, const vector& start_pos, const vector& end_pos, const vector& muzzle_normal);
    // effects.cpp's effects_play sets the new effect's sound and kill flags, effect info and category directly.
    friend unsigned int effects_play(const effect_info* info, const vector& pos, const matrix& orient, bool looping, unsigned int host_handle, int host_tag, unsigned int source_handle, float effect_volume, bool attach_to_camera, object_effect_category category);
    // objective_highlight.cpp's marker callbacks read the visual (file-local in rfg.exe, so external here to be friends).
    friend void objective_highlight_target_marker_update_render_cb(object_effect* oep);
    friend void objective_highlight_activity_marker_update_render_cb(object_effect* oep);
    friend void objective_highlight_ground_marker_update_render_cb(object_effect* oep);
    // object_effect_profile.cpp reads the visual.
    friend class object_effect_profile;
    // rfg's tracers.cpp primes the tracer effect's emitters and moves its particles directly (friends are not recorded
    // in rfg.pdb).
    friend void tracers_create(unsigned int owner, const vector& start_pos, const vector& end_pos, unsigned int tracer_effect, weapon_tracer_type w_tracer_type);
    friend void curved_tracers_create(unsigned int owner, const vector& start_pos, const vector& inner_spline_pos, const vector& inner_spline_pos2, const vector& end_pos, unsigned int tracer_effect, weapon_tracer_type w_tracer_type);
    friend void tracers_process();
    // rfg's dust.cpp (dust_update_render) hides the skirting effect's particles and puts decals at them (friends are not
    // recorded in rfg.pdb).
    friend void dust_update_render(void*, rl_renderer*);
    friend void expanding_tracer_particle_spread(object_effect* oe, tracer* tp, vector new_pos);
    object_stream_resource<resource_dependent_data> rdd; // 0x9C
    vector m_local_position; // 0xA4
    matrix m_local_orient; // 0xB0
    int host_tag_handle; // 0xD4
    effect_info* m_effect_info_p; // 0xD8
    unsigned int source_handle; // 0xDC
    stop_styles stop_style; // 0xE0
    vector start_position; // 0xE4
    bool attached_to_camera; // 0xF0
    bool attached_to_camera_orient; // 0xF1
    bool m_update_expressions; // 0xF2
    bool stop_when_host_destroyed; // 0xF3
    float current_time; // 0xF4
    void (*m_pre_update_cb)(object_effect*, void*); // 0xF8
    void (*m_post_update_cb)(object_effect*, void*); // 0xFC
    void (*m_post_update_render_cb)(object_effect*); // 0x100
    void* update_cb_data; // 0x104
    float custom_cb_data; // 0x108
    linked_list_p<registered_expression_variable> m_exp_vars; // 0x10C
    bool m_visual_creation_lod_queued; // 0x114
    float m_lod_restore_time; // 0x118
    bool m_restored_lod; // 0x11C
    int m_skybox_attached_orbital; // 0x120
    object_effect_category m_category; // 0x124
    static const int REGISTERED_EXPRESSION_VAR_POOL_MAX; // 0x012C7054
    static object_pool<registered_expression_variable,3072,1> ms_expression_var_pool; // 0x021DCD90
    visual_effect_object visual_effect; // 0x128
    sound_effect_object sound_effect; // 0x16C
};

object* alloc_object_effect(); // 0x00749480 (:104)
int bpo_index_object_effect(); // 0x00743C40 (:104)
void types_object_effect(signed char& type, signed char& subtype); // 0x00743C30 (:104)
const char* classname_object_effect(); // 0x00743C50 (:104)
void unused_dcf_effects_list(); // 0x0074CD10 (:122)
void vfx_physics_shape_unregister_cb(rl_handle shape_handle); // 0x00744000 (:1181)
void object_effect_process_all(float delta_time); // 0x0074A9B0 (:1275)
int object_effect_get_count(); // 0x0074AB80 (:1318)
void object_effect_system_init(mempool* permanent_pool); // 0x0074AB90 (:1341)
void object_effect_level_init_pre(); // 0x00744100 (:1365)
void object_effect_level_shutdown(); // 0x0074AC30 (:1371)
const object_pool<object_effect,512,1>& object_effect_get_objects(); // 0x00744110 (:1385)
const char* object_effect_get_expression_variable_name(object_effect_expression_variable_e variable_e); // 0x00744120 (:1390)
void object_effect_expressions_set_global_variable(object_effect_expression_variable_e variable_e, float value); // 0x00744130 (:1395)

extern obj_class_instantiator object_effect_instantiator; // 0x021DCCF8

// ---- inline members (inline here: rfg_render's game_time_of_day.cpp calls it, and the programs without the effect
// library have no object_effect.cpp)
// 0x007B9EB0 (.h:408)
inline void object_effect::set_skybox_attached_orbital(int skybox_attached_orbital)
{
    m_skybox_attached_orbital = skybox_attached_orbital;
}
