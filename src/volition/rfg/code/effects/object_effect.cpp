// Original: volition/rfg/code/effects/object_effect.cpp
//
// object_effect (decompiled; header comment in object_effect.h). Compiled in the effect library (rfg_effects, VFX.md
// 2.6); the programs without it keep the stand-ins of their stub files.
//
// An effect object is created from its property block (effects_play builds one with object_effect::props): the visual
// effect handle (an index into Visual_effect_info), the sound cue and attack/loop/release cue, looping, the host and
// host tag it follows, the source object the sound plays on, volume and camera attachment. The visual (a vfx_effect
// from the template the visual effect info names) is created when the object streams in, unless the effect's LOD puts
// it beyond its fade distance: then it is destroyed, or queued for creation when it comes back in range
// (restore_faded_object, looping effects only). Every frame (object_effect_process_all, simulation thread) the effect
// follows its host or the camera, updates its bounding box, moves its sound, fades or releases its visual by distance
// (process_lod) and updates the vfx_effect; a finished non-looping effect stops emitting and is destroyed once its
// particles are gone. update_render (the render handshake) builds the effect's render data. The expression variables
// (registered_expression_variable, pooled) are matched by name crc against the template's expression parameters.
//
// The physics plugin callbacks (vfx_physics_*_cb) give vfxlib's rigid body renderable filters their shapes (Havok
// collision models) and bodies: each body is an object_debris of collision type COLLIDE_EFFECT_RIGID_BODY.

#include "volition/rfg/code/effects/object_effect.h"

#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/audio_lib/code/audiolib_att_loop_rel.h"
#include "volition/render_lib/code/rl_corona_instance.h"
#include "volition/render_lib/code/rl_renderable_particles_instance_group.h"
#include "volition/render_lib/code/rl_skybox_renderable_instance.h"
#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/rfg/code/code_effects/los_blocker.h"
#include "volition/rfg/code/destruction/rfg_materials.h"
#include "volition/rfg/code/effects/effects.h"
#include "volition/rfg/code/effects/effects_shared.h"
#include "volition/rfg/code/game_render/fullscreen_effects/fb_accum_blur.h"
#include "volition/rfg/code/game_render/game_render_task.h"
#include "volition/rfg/code/gameplay/object_district.h"
#include "volition/rfg/code/gamewide/game_clock.h"
#include "volition/rfg/code/gamewide/game_render.h"
#include "volition/rfg/code/gamewide/game_time_of_day.h"
#include "volition/rfg/code/gamewide/gameaudio.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/havok/havok_cm_manager.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/level/world.inl"
#include "volition/rfg/code/objects/object.inl"
#include "volition/rfg/code/objects/object_debris.h"
#include "volition/rfg/code/objects/object_prop_names_shared.h"
#include "volition/rfg/code/weather/wind.h"
#include "volition/vfxlib/code/vfx.h"
#include "volition/vfxlib/code/vfx_effect.h"
#include "volition/vfxlib/code/vfx_effect_template.h"
#include "volition/vfxlib/code/vfx_particle_system_filter_instance.h"
#include "volition/vfxlib/code/vfx_particle_system_manager.h"
#include "volition/vlib/code/math/bbox.h"
#include "volition/vlib/code/math/crc.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/memory/object_pool.h"
#include "volition/vlib/code/os/frametime.h"
#include "volition/vlib/code/os/vconsole.h"

static const int OBJECT_EFFECT_POOL_SIZE = 512;
static const int OBJECT_EFFECT_POOL_USAGE = 0x31240; // memory footprint registered with the instantiator
static const float MPH_TO_METERS_PER_SECOND = 0.44704f;
static const float DEGREES_TO_RADIANS = 0.017453f; // as rounded in rfg.exe
// Positions at or beyond 1e7 (a broken host transform) are parked at 1e6.
static const float EFFECT_POSITION_LIMIT = 1e+07f;
static const float EFFECT_POSITION_PARKED = 1e+06f;
// The effect has moved when a coordinate differs from where it started by this much.
static const float EFFECT_MOVED_EPSILON = 0.0001f;
// A just created visual is updated by its template's start time plus this, so that it has simulated something.
static const float VISUAL_CREATE_TIME_EPSILON = 1e-07f;
// Elapsed time within this of the template's duration counts as the end of a pass.
static const float EFFECT_DURATION_EPSILON = 0.0001f;
static const int SOUND_FADE_OUT_ON_RELEASE_MS = -1; // audiolib's default fade
static const int SOUND_FADE_OUT_ON_STOP_MS = 1000;
static const int ALR_STOP_FADE_MS = 100;
// The sun or moon effect's coronas fade in over the orbital's first 0.02 of height above the horizon.
static const float ORBITAL_CORONA_FADE_HEIGHT = 0.02f;

// ---- file-local types (rfg.pdb)
// size 0x14
// A named expression variable; the 14 instances below register themselves (in order) at static initialization and
// object_effect_system_init gives them their name crc and their slot in Expression_var_lut.
struct expression_var
{
    expression_var(const char* str_p, int lut_index); // 0x00743BC0

    static expression_var* registrator_list; // 0x02182E08
    expression_var* next; // 0x00
    expression_var* prev; // 0x04
    const char* m_str_p; // 0x08
    unsigned int m_crc; // 0x0C
    int m_lut_index; // 0x10
};
static_assert(sizeof(expression_var) == 0x14, "expression_var");

expression_var* expression_var::registrator_list; // 0x02182E08

// 0x00743BC0 (:65)
// Appends the variable to the circular registration list.
expression_var::expression_var(const char* str_p, int lut_index)
{
    m_str_p = str_p;
    m_lut_index = lut_index;
    if (registrator_list == nullptr)
    {
        next = this;
        prev = this;
        registrator_list = this;
        return;
    }
    prev = registrator_list->prev;
    next = registrator_list;
    registrator_list->prev->next = this;
    registrator_list->prev = this;
}

// ---- file-static globals
static unsigned int Highest_priority_radial_blur = 0xFFFFFFFF; // 0x01650B08
static expression_var Expression_var_object_speed("object_speed", EFFECTS_VAR_OBJECT_SPEED); // 0x01650B0C
static expression_var Expression_var_object_size("object_size", EFFECTS_VAR_OBJECT_SIZE); // 0x01650B20
static expression_var Expression_var_random_0_to_1("random_0_to_1", EFFECTS_VAR_RANDOM_0_TO_1); // 0x01650B34
static expression_var Expression_var_wind_speed("wind_speed", EFFECTS_VAR_WIND_SPEED); // 0x01650B48
static expression_var Expression_var_wind_los("wind_los", EFFECTS_VAR_WIND_LOS); // 0x01650B5C
static expression_var Expression_var_jetpack_fuel_percent("jetpack_fuel_percent", EFFECTS_VAR_JETPACK_FUEL_PERCENT); // 0x01650B70
static expression_var Expression_var_backpack_color("backpack_color", EFFECTS_VAR_BACKPACK_FUEL_COLOR); // 0x01650B84
static expression_var Expression_var_backpack_life("backpack_life", EFFECTS_VAR_BACKPACK_FUEL_LIFE); // 0x01650B98
static expression_var Expression_var_vehicle_smoke_percent("vehicle_smoke_percent", EFFECTS_VAR_VEHICLE_SMOKE_PERCENT); // 0x01650BAC
static expression_var Expression_var_weapon_heat_percent("weapon_heat_percent", EFFECTS_VAR_WEAPON_HEAT_PERCENT); // 0x01650BC0
static expression_var Expression_var_effect_percent("effect_percent", EFFECTS_VAR_PERCENT); // 0x01650BD4
static expression_var Expression_var_particle_load("particle_load", EFFECTS_VAR_PARTICLE_LOAD); // 0x01650BE8
static expression_var Expression_var_glass_scale_x("glass_scale_x", EFFECTS_VAR_SCALE_X); // 0x01650BFC
static expression_var Expression_var_glass_scale_y("glass_scale_y", EFFECTS_VAR_SCALE_Y); // 0x01650C10
static expression_var* Expression_var_lut[EFFECTS_VAR_COUNT]; // 0x02106310
static int Object_effect_count[EFFECTS_CATEGORY_MAX]; // 0x02182810
static object_effect* Last_effect; // 0x02182E10, the effect being updated (for crash reports)
bool Effects_force_update_render; // 0x02182E0C
static game_render_task Update_render_task_object; // 0x02182E4C
const int object_effect::REGISTERED_EXPRESSION_VAR_POOL_MAX = 3072; // 0x012C7054
object_pool<object_effect::registered_expression_variable, 3072, 1> object_effect::ms_expression_var_pool; // 0x021DCD90
static object_pool<object_effect, OBJECT_EFFECT_POOL_SIZE, 1> object_effectpool; // 0x021EF9E0
obj_class_instantiator object_effect_instantiator("object_effect", alloc_object_effect, types_object_effect, bpo_index_object_effect, classname_object_effect, object_effect::pool_remaining, OBJECT_EFFECT_POOL_USAGE, OBJECT_EFFECT_POOL_SIZE); // 0x021DCCF8 (0x0106A810)

// The visual effect info of a handle, or null (Visual_effect_info + handle, inlined everywhere).
static visual_effect_info* object_effect_visual_info(unsigned int handle)
{
    return handle < (unsigned int)Num_visual_effect_types ? &Visual_effect_info[handle] : nullptr;
}

// ---- class registration (:104)
// 0x00743C50
const char* classname_object_effect()
{
    return "object_effect";
}

// 0x00749480
object* alloc_object_effect()
{
    return object_effectpool.alloc(0);
}

// 0x00743C30
void types_object_effect(signed char& type, signed char& subtype)
{
    type = OT_EFFECT;
    subtype = OT_SUB_UNDEFINED;
}

// 0x00746150
int object_effect::pool_remaining()
{
    return object_effectpool.get_free();
}

// 0x0074BEF0
void object_effect::pool_free()
{
    object_effectpool.free(this);
}

// 0x00743C60
unsigned int object_effect::get_classname_hash()
{
    return object_effect_instantiator.this_hash;
}

// 0x00743C40
int bpo_index_object_effect()
{
    return 1;
}

// 0x007422A0 (.h:68)
const char* object_effect::get_classname_static()
{
    return "object_effect";
}

// 0x007422B0 (.h:68)
int object_effect::get_bpo_index_static()
{
    return 1;
}

// 0x00746190 (.h:68)
const char* object_effect::get_classname() const
{
    return "object_effect";
}

// 0x00743C70
object_effect::visual_effect_object::visual_effect_object()
{
}

// 0x00746160 (:105)
object_effect::object_effect()
{
    // m_exp_vars: an empty list from its constructor
}

// 0x007461A0
object_effect::~object_effect()
{
}

// 0x00744EA0 (.h:86)
object_effect::props::props(unsigned int visual_handle, int sound_handle, int sound_alr_handle, const vector& _position, const matrix& _orient, bool looping, unsigned int host_handle, int host_tag, unsigned int source_handle, float volume, bool attach_to_camera)
    : obj_prop_block(_position, _orient, nullptr)
{
    set(OBJECT_EFFECT_VISUAL_EFFECT_HANDLE, visual_handle);
    set(OBJECT_EFFECT_SOUND_EFFECT_HANDLE, sound_handle);
    set(OBJECT_EFFECT_SOUND_ALR_HANDLE, sound_alr_handle);
    set(OBJECT_EFFECT_LOOPING, looping);
    set(OBJECT_EFFECT_HOST_HANDLE, host_handle);
    set(OBJECT_EFFECT_HOST_TAG, host_tag);
    set(OBJECT_EFFECT_SOURCE_HANDLE, source_handle);
    set(OBJECT_EFFECT_VOLUME, volume);
    set(OBJECT_EFFECT_ATTACHED_TO_CAMERA, attach_to_camera);
}

// 0x00742890
object_effect::props::~props()
{
}

// 0x0074CD10 (:122)
// The "effects_list" console command: counts the live effects per visual effect name, streamed and not streamed
// separately. The output is compiled out of retail; only the counting and the walk over the tables remain.
void unused_dcf_effects_list()
{
    if (Console_command == 0)
    {
        return;
    }
    str_data_hash_table<int, 512, 32, unsigned short> streamed_counts;
    str_data_hash_table<int, 512, 32, unsigned short> other_counts;
    const int num = World.get_num_objects<object_effect>();
    for (int i = 0; i < num; ++i)
    {
        object_effect* effect_p = World.get_object<object_effect>(i);
        visual_effect_info* info_p = object_effect_visual_info(effect_p->get_visual_effect_info_handle());
        if (info_p == nullptr)
        {
            continue;
        }
        str_data_hash_table<int, 512, 32, unsigned short>& table = effect_p->streamed() ? streamed_counts : other_counts;
        int* count_p = table.get_ptr(info_p->name);
        if (count_p == nullptr)
        {
            table.insert_key_data(info_p->name, 1);
        }
        else
        {
            ++*count_p;
        }
    }
    const char* key;
    for (bool more = streamed_counts.get_first_key(key); more; more = streamed_counts.get_next_key(key, key))
    {
    }
    for (bool more = other_counts.get_first_key(key); more; more = other_counts.get_next_key(key, key))
    {
    }
}

// 0x007461C0 (:174)
void object_effect::reset()
{
    rdd.set_loaded(false);
    m_visual_creation_lod_queued = false;
    m_restored_lod = false;
    host_tag_handle = -1;
    current_time = 0.0f;
    stop_style = SS_INVALID;
    start_position.zero();
    visual_effect.kill_particles_fade_time_s = 0.0f;
    visual_effect.kill_particles_rem_fade_time_s = 0.0f;
    m_pre_update_cb = nullptr;
    m_post_update_cb = nullptr;
    m_post_update_render_cb = nullptr;
    attached_to_camera = false;
    visual_effect.looping = false;
    visual_effect.do_update_render = false;
    visual_effect.kill_particles_on_stop = false;
    sound_effect.cue_id = -1;
    sound_effect.alr_cue_id = -1;
    sound_effect.volume = 1.0f;
    m_update_expressions = true;
    stop_when_host_destroyed = false;
    m_local_position = ZERO_VECTOR;
    m_local_orient = IDENTITY_MATRIX;
    m_skybox_attached_orbital = -1;
    m_category = EFFECTS_CATEGORY_NONE;
    m_effect_info_p = nullptr;
    m_exp_vars.init();
}

// 0x007494C0 (:225)
// Save: the handles, looping, host and host tag. Load: the effect by name (effects.xtbl, its visual, sounds and
// flags), or the visual and sounds by name or handle; an object with neither fails (and is reset). Then looping, the
// host it attaches to, the source, the template's container (requested as a streaming resource), volume, camera
// attachment and the initial bounding box.
bool object_effect::serialize(obj_prop_block& pblock, bool load)
{
    if (!object::serialize(pblock, load))
    {
        return false;
    }
    if (!load)
    {
        pblock.set(OBJECT_EFFECT_VISUAL_EFFECT_HANDLE, visual_effect.handle);
        pblock.set(OBJECT_EFFECT_SOUND_EFFECT_HANDLE, sound_effect.cue_id);
        pblock.set(OBJECT_EFFECT_SOUND_ALR_HANDLE, sound_effect.alr_cue_id);
        pblock.set(OBJECT_EFFECT_LOOPING, visual_effect.looping);
        const unsigned int host_handle = get_parent();
        if (host_handle != 0xFFFFFFFF)
        {
            pblock.set(OBJECT_EFFECT_HOST_HANDLE, host_handle);
        }
        if (host_tag_handle >= 0)
        {
            pblock.set(OBJECT_EFFECT_HOST_TAG, host_tag_handle);
        }
        return true;
    }
    reset();
    char name[68];
    if (pblock.get_optional(OBJECT_EFFECT_EFFECT_TYPE, name))
    {
        const unsigned int effect_handle = effects_get_handle(name);
        if (effect_handle == 0xFFFFFFFF)
        {
            reset();
            return false;
        }
        if (effect_handle < Num_effect_types)
        {
            const effect_info& info = Effect_info[effect_handle];
            visual_effect.handle = info.visual_handle;
            visual_effect.kill_particles_on_stop = info.flags.vfx_kill_particles_on_stop;
            visual_effect.kill_particles_fade_time_s = info.vfx_kill_particles_fade_time_s;
            sound_effect.cue_id = info.sound_handle;
            sound_effect.alr_cue_id = info.sound_alr_handle;
            sound_effect.kill_oneshot_when_done = info.flags.sound_kill_oneshot_when_done;
            sound_effect.follow_effect_pos = info.flags.sound_follows_effect_pos;
        }
    }
    else
    {
        if (pblock.get_optional(OBJECT_EFFECT_VISUAL_EFFECT, name))
        {
            visual_effect.handle = effects_get_visual_effect_info_handle(name);
        }
        else
        {
            pblock.get_optional(OBJECT_EFFECT_VISUAL_EFFECT_HANDLE, visual_effect.handle);
        }
        if (pblock.get_optional(OBJECT_EFFECT_SOUND_EFFECT, name))
        {
            audiolib_cue_get_id(name, &sound_effect.cue_id);
        }
        else
        {
            pblock.get_optional(OBJECT_EFFECT_SOUND_EFFECT_HANDLE, sound_effect.cue_id);
        }
        if (pblock.get_optional(OBJECT_EFFECT_SOUND_ALR, name))
        {
            audiolib_att_loop_rel_get_cue_id(name, &sound_effect.alr_cue_id);
        }
        else
        {
            pblock.get_optional(OBJECT_EFFECT_SOUND_ALR_HANDLE, sound_effect.alr_cue_id);
        }
    }
    if (visual_effect.handle == 0xFFFFFFFF && sound_effect.cue_id == -1 && sound_effect.alr_cue_id == -1)
    {
        reset();
        return false;
    }
    visual_effect.looping = false;
    pblock.get_optional(OBJECT_EFFECT_LOOPING, visual_effect.looping);
    unsigned int host_handle = 0xFFFFFFFF;
    pblock.get_optional(OBJECT_EFFECT_HOST_HANDLE, host_handle);
    host_tag_handle = -1;
    pblock.get_optional(OBJECT_EFFECT_HOST_TAG, host_tag_handle);
    if (World.object_from_handle(host_handle) != nullptr)
    {
        attach_to_host(host_handle, host_tag_handle);
    }
    source_handle = 0xFFFFFFFF;
    pblock.get_optional(OBJECT_EFFECT_SOURCE_HANDLE, source_handle);
    const visual_effect_info* info_p = object_effect_visual_info(visual_effect.handle);
    if (info_p != nullptr && info_p->vfx_filename[0] != '\0')
    {
        const unsigned int container_srid = stream2_container_find(info_p->vfx_filename);
        if (container_srid == 0)
        {
            return false;
        }
        request_streaming_resource(container_srid, STREAM_PRIORITY_AUDIO_CACHE_LOAD);
    }
    sound_effect.volume = 1.0f;
    pblock.get_optional(OBJECT_EFFECT_VOLUME, sound_effect.volume);
    attached_to_camera = false;
    pblock.get_optional(OBJECT_EFFECT_ATTACHED_TO_CAMERA, attached_to_camera);
    attached_to_camera_orient = false;
    pblock.get_optional(OBJECT_EFFECT_ATTACHED_TO_CAMERA_ORIENT, attached_to_camera_orient);
    get_bbox(visual_effect.bbox_initial.min, visual_effect.bbox_initial.max);
    visual_effect.bbox_current = visual_effect.bbox_initial;
    return true;
}

// 0x00743C80 (:354)
// Effects that were stopped are not saved.
obj_serialize_mode object_effect::serialize_mode()
{
    return (obj_serialize_mode)(stop_style == SS_INVALID);
}

// 0x0074D060 (:370)
// Stream in: start the sounds, find the template and create the visual (or leave it to process_lod, or destroy the
// effect, when it is already beyond its LOD fade distance), start its radial blur when it outranks the one playing,
// set the random expression variable, the line of sight blocker, the render flags, a stop style set while it was
// streamed out, and the district's effect tint. Stream out: release everything.
bool object_effect::stream(bool load)
{
    if (!object::stream(load))
    {
        return false;
    }
    if (!load)
    {
        visual_effect.flags = 0;
        release();
        rdd.set_loaded(false);
        if (Highest_priority_radial_blur == get_handle())
        {
            fb_accum_blur_kill_radial_blur();
            Highest_priority_radial_blur = 0xFFFFFFFF;
        }
        return true;
    }
    rdd.set_loaded(true);
    const stop_styles saved_stop_style = stop_style;
    if (sound_effect.cue_id == -1)
    {
        sound_effect.play_id = -1;
    }
    else if (source_handle == 0xFFFFFFFF || World.object_from_handle(source_handle) == nullptr)
    {
        sound_effect.play_id = gameaudio_play_cross_linked_sound(sound_effect.cue_id, get_handle(), nullptr, nullptr);
    }
    else
    {
        sound_effect.play_id = gameaudio_play_cross_linked_sound(sound_effect.cue_id, source_handle, nullptr, nullptr);
    }
    if (sound_effect.alr_cue_id == -1)
    {
        sound_effect.alr_id = -1;
    }
    else
    {
        audiolib_att_loop_rel_start(sound_effect.alr_cue_id, &sound_effect.alr_id, false, &pos, nullptr);
    }
    visual_effect_info* info_p = object_effect_visual_info(visual_effect.handle);
    if (info_p != nullptr && info_p->vfx_filename[0] != '\0')
    {
        info_p->template_p = effects_template_find(info_p->vfx_filename);
        if (info_p->template_p != nullptr)
        {
            if (info_p->m_lod.enabled && Effects_lod_enabled)
            {
                const float fade_end = info_p->m_lod.update_render_fade_end;
                const vector to_camera(pos.x - Rfg_camera.real_pos.x, pos.y - Rfg_camera.real_pos.y, pos.z - Rfg_camera.real_pos.z);
                if (fade_end * fade_end < to_camera.mag_squared())
                {
                    if (!info_p->m_lod.restore_faded_object || !visual_effect.looping)
                    {
                        World.destroy_object(this, false);
                        m_visual_creation_lod_queued = false;
                    }
                    else
                    {
                        m_visual_creation_lod_queued = true;
                    }
                }
            }
            if (!m_visual_creation_lod_queued)
            {
                visual_create();
            }
        }
        if (info_p->radial_blur_info_p != nullptr)
        {
            // The playing blur keeps its rank while it lasts. Inferred: the decompile leaves the rank in a register
            // when the playing effect has no blur info, read here as "no rank" (the lowest priority).
            int playing_priority = -0x7FFFFFFF;
            object_effect* playing_p = World.object_from_handle<object_effect>(Highest_priority_radial_blur);
            if (playing_p != nullptr)
            {
                const visual_effect_info* playing_info_p = effects_get_visual_effect_info(playing_p->visual_effect.handle);
                if (playing_info_p != nullptr && playing_info_p->radial_blur_info_p != nullptr && playing_p->current_time < playing_info_p->radial_blur_info_p->m_duration)
                {
                    playing_priority = playing_info_p->radial_blur_info_p->m_priority;
                }
            }
            if (playing_priority < info_p->radial_blur_info_p->m_priority)
            {
                Highest_priority_radial_blur = get_handle();
                effects_play_radial_blur(info_p->radial_blur_info_p, pos);
            }
        }
    }
    else
    {
        info_p = nullptr;
    }
    set_expression_variable(EFFECTS_VAR_RANDOM_0_TO_1, fl_rand());
    if (info_p != nullptr && (info_p->flags & 4) != 0) // 4: the visual effect blocks line of sight
    {
        los_blocker_create(pos, info_p->radius, info_p->los_blocker_opacity, info_p->los_blocker_life_span, get_handle());
    }
    set_render_flags(8);
    if (saved_stop_style != SS_INVALID)
    {
        // Original: a no-op, since stop only acts on a style above the current one.
        stop(stop_style);
    }
    district* district_p = nullptr;
    unsigned int district_handle = district_handle_from_pos(pos);
    if (district_handle != 0xFFFFFFFF || (district_handle = district_get_default()) != 0xFFFFFFFF)
    {
        object* objp = World.object_from_handle(district_handle);
        if (objp != nullptr && objp->get_obj_type() == OT_DISTRICT)
        {
            district_p = (district*)objp;
        }
    }
    vfx_effect* effect_p = get_vfx_effect();
    if (effect_p != nullptr)
    {
        // Original bug kept: rfg.exe reads the tint through a null district (address 0x13C) when the position has
        // no district and there is no default one.
        effect_p->set_effect_material_tint(district_p->get_vfx_tint());
    }
    return true;
}

// 0x0074BF00 (:499)
// Releases the visual, fades out the sound (oneshots only when kill_oneshot_when_done, loops always), stops the
// attack/loop/release sound, frees the expression variables and leaves the category count.
void object_effect::release()
{
    if (rdd.is_loaded())
    {
        resource_dependent_data* data_p = rdd.get_data();
        if (data_p->vfx_effect_p != nullptr)
        {
            data_p->vfx_effect_p->release();
            rdd.get_data()->vfx_effect_p = nullptr;
        }
    }
    if (sound_effect.play_id != -1)
    {
        if (sound_effect.kill_oneshot_when_done || audiolib_cue_is_looping(sound_effect.cue_id) != 0)
        {
            audiolib_play_fade_out(sound_effect.play_id, SOUND_FADE_OUT_ON_RELEASE_MS);
        }
        sound_effect.play_id = -1;
    }
    if (sound_effect.alr_id != -1)
    {
        audiolib_att_loop_rel_stop(sound_effect.alr_id, ALR_STOP_FADE_MS);
        sound_effect.alr_id = -1;
    }
    registered_expression_variable* head_p = m_exp_vars.first();
    registered_expression_variable* var_p = head_p;
    while (var_p != nullptr)
    {
        registered_expression_variable* next_p = var_p->next;
        ms_expression_var_pool.free(var_p);
        var_p = next_p != head_p ? next_p : nullptr;
    }
    m_exp_vars.init();
    if (m_category != EFFECTS_CATEGORY_NONE)
    {
        --Object_effect_count[m_category];
        m_category = EFFECTS_CATEGORY_NONE;
    }
}

// 0x007462F0 (:538)
// Creates the vfx_effect from the template, emitting and with coronas on, at the object's transform, and advances it
// by the template's start time.
void object_effect::visual_create()
{
    resource_dependent_data* data_p = rdd.get_data();
    const visual_effect_info* info_p = object_effect_visual_info(visual_effect.handle);
    data_p->vfx_effect_p = vfx_effect_create(info_p->template_p);
    vfx_effect* effect_p = data_p->vfx_effect_p;
    if (effect_p == nullptr)
    {
        return;
    }
    visual_effect.flags |= 3;
    effect_p->m_elapsed_time = 0.0f;
    effect_p->enable_emission(true);
    effect_p->enable_coronas(true);
    effect_p->set_position(pos);
    effect_p->set_rotation(orient);
    effect_p->update(info_p->m_start_time + VISUAL_CREATE_TIME_EPSILON);
}

// 0x007463E0 (:558)
void object_effect::visual_release()
{
    resource_dependent_data* data_p = rdd.get_data();
    if (data_p->vfx_effect_p != nullptr)
    {
        data_p->vfx_effect_p->release();
        rdd.get_data()->vfx_effect_p = nullptr;
    }
}

// 0x00746430 (:570)
// Moves the effect under a new host (zones cannot host effects). Without a tag the current offset from the host
// becomes the local transform; with a tag the local transform is kept relative to the tag.
void object_effect::attach_to_host(unsigned int new_host_handle, int tag_handle)
{
    if (get_parent() != new_host_handle)
    {
        object* old_host_p = World.object_from_handle(get_parent());
        if (old_host_p != nullptr)
        {
            old_host_p->remove_child(this);
        }
        object* new_host_p = World.object_from_handle(new_host_handle);
        if (new_host_p == nullptr || new_host_p->get_obj_type() == OT_ZONE)
        {
            return;
        }
        if (!new_host_p->set_child(this))
        {
            host_tag_handle = -1;
            return;
        }
    }
    host_tag_handle = tag_handle;
    if (tag_handle == -1)
    {
        object* host_p = World.object_from_handle(new_host_handle);
        if (host_p == nullptr)
        {
            return;
        }
        m_local_position.x = pos.x - host_p->pos.x;
        m_local_position.y = pos.y - host_p->pos.y;
        m_local_position.z = pos.z - host_p->pos.z;
        m_local_position.rotate(host_p->orient);
        m_local_orient = orient * host_p->orient.copy_transpose();
    }
    update_pos_and_orient_internal();
}

// 0x00743C90 (:607)
void object_effect::deattach_from_host()
{
    object* host_p = World.object_from_handle(get_parent());
    if (host_p != nullptr)
    {
        host_p->remove_child(this);
    }
}

// 0x00743CB0 (:618)
// The world transform from the host (or its tag) and the local transform.
void object_effect::update_pos_and_orient_internal()
{
    object* host_p = World.object_from_handle(get_parent());
    if (host_p == nullptr)
    {
        return;
    }
    vector new_pos;
    matrix new_orient;
    if (host_tag_handle == -1)
    {
        const vector offset = m_local_position.copy_unrotate(host_p->orient);
        new_pos.z = host_p->pos.z + offset.z;
        new_pos.y = host_p->pos.y + offset.y;
        new_pos.x = host_p->pos.x + offset.x;
        new_orient = m_local_orient * host_p->orient;
    }
    else
    {
        vector tag_pos;
        matrix tag_orient;
        host_p->get_tag_world_coords(host_tag_handle, tag_pos, &tag_orient, nullptr, nullptr);
        const vector offset = m_local_position.copy_unrotate(tag_orient);
        new_pos.z = offset.z + tag_pos.z;
        new_pos.y = offset.y + tag_pos.y;
        new_pos.x = tag_pos.x + offset.x;
        new_orient = m_local_orient * tag_orient;
    }
    update_pos_and_orient(new_pos, new_orient, true);
}

// 0x007499B0 (:641)
// Each template expression whose first parameter names a registered variable gets its value.
void object_effect::update_expressions()
{
    vfx_effect* effect_p = rdd.get_data()->vfx_effect_p;
    if (effect_p == nullptr || !m_update_expressions)
    {
        return;
    }
    const int num_expressions = effect_p->get_expression_count();
    for (int expression_index = 0; expression_index < num_expressions; ++expression_index)
    {
        const unsigned int variable_crc = effect_p->get_variable_handle(expression_index, 0);
        registered_expression_variable* head_p = m_exp_vars.first();
        registered_expression_variable* var_p = head_p;
        while (var_p != nullptr)
        {
            if (var_p->m_crc == variable_crc)
            {
                effect_p->set_variable_value(expression_index, 0, var_p->m_value);
                break;
            }
            var_p = var_p->next;
            if (var_p == head_p)
            {
                break;
            }
        }
    }
}

// 0x00749A50 (:668)
// Without a visual, a queued effect creates it again once back inside its fade distance (restoring its elapsed time;
// an effect told to stop emitting is destroyed instead). With a visual: beyond the fade end it is released and queued
// (restore_faded_object) or stopped; between the fade start and end it fades out.
void object_effect::process_lod()
{
    vfx_effect* effect_p = rdd.get_data()->vfx_effect_p;
    if (effect_p == nullptr)
    {
        if (!m_visual_creation_lod_queued)
        {
            return;
        }
        const visual_effect_info* info_p = object_effect_visual_info(visual_effect.handle);
        if (!info_p->m_lod.enabled || !Effects_lod_enabled)
        {
            return;
        }
        const float dx = pos.x - Rfg_camera.real_pos.x;
        const float dy = pos.y - Rfg_camera.real_pos.y;
        const float dz = pos.z - Rfg_camera.real_pos.z;
        const float fade_end = info_p->m_lod.update_render_fade_end;
        if (fade_end * fade_end < dx * dx + dy * dy + dz * dz)
        {
            return;
        }
        if (stop_style == SS_STOP_EMITTING)
        {
            World.destroy_object(this, false);
            return;
        }
        visual_create();
        effect_p = rdd.get_data()->vfx_effect_p;
        if (effect_p != nullptr)
        {
            effect_p->m_elapsed_time = m_lod_restore_time;
        }
        start_position = pos;
        m_visual_creation_lod_queued = false;
        m_restored_lod = true;
        if (effect_p == nullptr)
        {
            return;
        }
    }
    const visual_effect_info* info_p = object_effect_visual_info(visual_effect.handle);
    if (!info_p->m_lod.enabled || !Effects_lod_enabled)
    {
        return;
    }
    const float dx = pos.x - Rfg_camera.real_pos.x;
    const float dy = pos.y - Rfg_camera.real_pos.y;
    const float dz = pos.z - Rfg_camera.real_pos.z;
    const float fade_end = info_p->m_lod.update_render_fade_end * info_p->m_lod.update_render_fade_end;
    const float dist_sq = dx * dx + dy * dy + dz * dz;
    const float fade_start = info_p->m_lod.update_render_fade_start * info_p->m_lod.update_render_fade_start;
    if (fade_end < dist_sq)
    {
        if (info_p->m_lod.restore_faded_object)
        {
            m_lod_restore_time = effect_p->m_elapsed_time;
            visual_release();
            m_visual_creation_lod_queued = true;
            return;
        }
        m_visual_creation_lod_queued = false;
        stop(SS_STOP_EMITTING);
        return;
    }
    if (fade_start < dist_sq)
    {
        effect_p->alpha_fade = 1.0f - (dist_sq - fade_start) / (fade_end - fade_start);
    }
}

// 0x00749D30 (:725)
void object_effect::update(float delta_time)
{
    if (obj_flags.destroyed || !rdd.is_loaded())
    {
        return;
    }
    current_time += delta_time;
    visual_effect.do_update_render = false;
    process_lod();
    vfx_effect* effect_p = rdd.get_data()->vfx_effect_p;
    if (m_pre_update_cb != nullptr)
    {
        m_pre_update_cb(this, update_cb_data);
    }
    update_expressions();
    update_pos_and_orient_internal();
    if (attached_to_camera)
    {
        pos = Rfg_camera.real_pos;
    }
    if (attached_to_camera_orient)
    {
        orient = Rfg_camera.real_orient;
    }
    if (EFFECT_POSITION_LIMIT <= pos.x || EFFECT_POSITION_LIMIT <= pos.y || EFFECT_POSITION_LIMIT <= pos.z)
    {
        pos.x = EFFECT_POSITION_PARKED;
        pos.y = EFFECT_POSITION_PARKED;
        pos.z = EFFECT_POSITION_PARKED;
    }
    else
    {
        const float dx = pos.x - start_position.x;
        const float dy = pos.y - start_position.y;
        const float dz = pos.z - start_position.z;
        if (EFFECT_MOVED_EPSILON <= dx || EFFECT_MOVED_EPSILON <= -dx || EFFECT_MOVED_EPSILON <= dy || EFFECT_MOVED_EPSILON <= -dy || EFFECT_MOVED_EPSILON <= dz || EFFECT_MOVED_EPSILON <= -dz)
        {
            get_bbox(visual_effect.bbox_current.min, visual_effect.bbox_current.max);
            bbox_expand(visual_effect.bbox_current.min, visual_effect.bbox_current.max, visual_effect.bbox_initial.min, visual_effect.bbox_initial.max);
        }
    }
    if (audiolib_play_get_state(sound_effect.play_id, nullptr) == AUDIOLIB_ERR_NONE)
    {
        if (sound_effect.follow_effect_pos)
        {
            audiolib_play_set_3d_position(sound_effect.play_id, &pos, nullptr, nullptr);
        }
    }
    else
    {
        sound_effect.play_id = -1;
    }
    if (sound_effect.alr_id != -1)
    {
        audiolib_att_loop_rel_set_pos(sound_effect.alr_id, &pos, nullptr);
    }
    if (effect_p != nullptr)
    {
        effect_p->set_position(pos);
        effect_p->set_rotation(orient);
        if (current_time - delta_time == 0.0f || m_restored_lod)
        {
            start_position = pos;
            get_bbox(visual_effect.bbox_initial.min, visual_effect.bbox_initial.max);
            visual_effect.bbox_current = visual_effect.bbox_initial;
            effect_p->m_world_aabb.min = visual_effect.bbox_current.min;
            effect_p->m_world_aabb.max = visual_effect.bbox_current.max;
            effect_p->m_render_flags |= 4;
            effect_p->m_world_bounding_sphere.compute_from_aabb(effect_p->m_world_aabb);
            m_restored_lod = false;
        }
        visual_effect.do_update_render = true;
        const visual_effect_info* info_p = object_effect_visual_info(visual_effect.handle);
        // View dependent effects out of view stop simulating (and drawing) once past their minimum time.
        bool updated = true;
        if (!info_p->m_lod.enabled || !info_p->m_lod.update_view_dependent || !Effects_lod_enabled || game_render_is_object_visible(pos, effect_p->m_world_bounding_sphere.m_radius) || (visual_effect.do_update_render = false, effect_p->m_elapsed_time <= info_p->m_lod.view_dependent_minimum_time))
        {
            effect_p->update(delta_time);
        }
        else
        {
            updated = false;
        }
        const float duration = effect_p->m_effect_template_p->get_duration();
        if (effect_p->m_elapsed_time + delta_time <= duration)
        {
            effect_p->m_elapsed_time = effect_p->m_elapsed_time + delta_time;
        }
        else
        {
            effect_p->m_elapsed_time = duration;
        }
        if (stop_style != SS_STOP_EMITTING && fl_greater_than_equal_robust(effect_p->m_elapsed_time, effect_p->m_effect_template_p->get_duration(), EFFECT_DURATION_EPSILON))
        {
            if (!visual_effect.looping)
            {
                stop(SS_STOP_EMITTING);
            }
            else
            {
                effect_p->m_elapsed_time = effect_p->m_elapsed_time - effect_p->m_effect_template_p->get_duration();
                if (audiolib_cue_is_looping(sound_effect.cue_id) == 0)
                {
                    sound_effect.play_id = gameaudio_play_cross_linked_sound(sound_effect.cue_id, get_handle(), nullptr, nullptr);
                }
            }
        }
        if (stop_style == SS_STOP_EMITTING)
        {
            if (visual_effect.kill_particles_on_stop)
            {
                visual_effect.kill_particles_rem_fade_time_s -= delta_time;
                if (0.0f < visual_effect.kill_particles_rem_fade_time_s)
                {
                    effect_p->alpha_fade = visual_effect.kill_particles_rem_fade_time_s / visual_effect.kill_particles_fade_time_s;
                }
                else
                {
                    World.destroy_object(this, false);
                }
            }
            if (!effect_p->m_is_particle_system_active || !updated)
            {
                World.destroy_object(this, false);
            }
        }
        set_render_flags(8);
    }
    // A sound-only effect ends with its (non-looping) sound.
    if (visual_effect.handle == 0xFFFFFFFF && audiolib_cue_is_looping(sound_effect.cue_id) == 0 && audiolib_play_get_state(sound_effect.play_id, nullptr) != AUDIOLIB_ERR_NONE)
    {
        sound_effect.play_id = -1;
        World.destroy_object(this, false);
    }
    if (m_post_update_cb != nullptr)
    {
        m_post_update_cb(this, update_cb_data);
    }
}

// 0x007465A0 (:902)
// Stop styles only escalate. Stop looping: the effect plays its pass to the end. Stop emitting: no new particles or
// coronas (or the kill fade starts), the sounds fade out; an effect without a visual is destroyed at once.
void object_effect::stop(stop_styles style)
{
    if (style == SS_INVALID || style <= stop_style)
    {
        return;
    }
    vfx_effect* effect_p = get_vfx_effect();
    if (effect_p == nullptr)
    {
        visual_effect.flags = 0;
    }
    stop_style = style;
    if (style != SS_STOP_LOOPING)
    {
        if (style != SS_STOP_EMITTING)
        {
            return;
        }
        if (effect_p == nullptr)
        {
            World.destroy_object(this, false);
        }
        else if (!visual_effect.kill_particles_on_stop)
        {
            effect_p->enable_emission(false);
            effect_p->enable_coronas(false);
        }
        else
        {
            visual_effect.kill_particles_rem_fade_time_s = visual_effect.kill_particles_fade_time_s;
        }
        if (sound_effect.play_id != -1)
        {
            if (sound_effect.kill_oneshot_when_done || audiolib_cue_is_looping(sound_effect.cue_id) != 0)
            {
                audiolib_play_fade_out(sound_effect.play_id, SOUND_FADE_OUT_ON_STOP_MS);
            }
            sound_effect.play_id = -1;
        }
        if (sound_effect.alr_id != -1)
        {
            audiolib_att_loop_rel_stop(sound_effect.alr_id, ALR_STOP_FADE_MS);
            sound_effect.alr_id = -1;
        }
    }
    visual_effect.looping = false;
}

// 0x0074A490 (:964)
// The bounds from the current box; an effect attached to a skybox orbital (the sun) follows the orbital and fades its
// coronas in above the horizon; then the vfx render data (still built while paused, so the effect stays on screen),
// the radial blur's position and the post-render callback.
void object_effect::update_render(rl_renderer* pRenderer)
{
    vfx_effect* effect_p = get_vfx_effect();
    if (effect_p == nullptr)
    {
        return;
    }
    update_expressions();
    effect_p->m_render_flags |= 4;
    effect_p->m_world_aabb.min = visual_effect.bbox_current.min;
    effect_p->m_world_aabb.max = visual_effect.bbox_current.max;
    effect_p->m_world_bounding_sphere.m_position.x = (visual_effect.bbox_current.max.x + visual_effect.bbox_current.min.x) * 0.5f;
    effect_p->m_world_bounding_sphere.m_position.y = (visual_effect.bbox_current.max.y + visual_effect.bbox_current.min.y) * 0.5f;
    effect_p->m_world_bounding_sphere.m_position.z = (visual_effect.bbox_current.max.z + visual_effect.bbox_current.min.z) * 0.5f;
    const float dx = visual_effect.bbox_current.min.x - visual_effect.bbox_current.max.x;
    const float dy = visual_effect.bbox_current.min.y - visual_effect.bbox_current.max.y;
    const float dz = visual_effect.bbox_current.min.z - visual_effect.bbox_current.max.z;
    effect_p->m_world_bounding_sphere.m_radius = sqrtf(dx * dx + dy * dy + dz * dz) * 0.5f;
    effect_p->m_render_flags |= 4;
    if (m_skybox_attached_orbital != -1)
    {
        rl_skybox_orbital_params orbital_params;
        game_time_of_day_get_orbital_params(m_skybox_attached_orbital, orbital_params);
        object_effect* orbital_effect_p = game_time_of_day_get_orbital_effect(m_skybox_attached_orbital);
        if (orbital_effect_p != nullptr)
        {
            float corona_fade = (orbital_params.position.y - 0.0f) / ORBITAL_CORONA_FADE_HEIGHT;
            if (corona_fade - 0.0f < 0.0f)
            {
                corona_fade = 0.0f;
            }
            if (0.0f <= corona_fade - 1.0f)
            {
                corona_fade = 1.0f;
            }
            orbital_params.position.y = Rfg_camera.render_pos.y + orbital_params.position.y;
            orbital_params.position.z = orbital_params.position.z + Rfg_camera.render_pos.z;
            orbital_effect_p->pos.x = orbital_params.position.x + Rfg_camera.render_pos.x;
            orbital_effect_p->pos.y = orbital_params.position.y;
            orbital_effect_p->pos.z = orbital_params.position.z;
            game_render_set_sun_shafts_position(orbital_params.position);
            effect_p->set_position(orbital_effect_p->pos);
            vfx_effect* orbital_vfx_p = orbital_effect_p->get_vfx_effect();
            if (orbital_vfx_p != nullptr)
            {
                rl_renderable_particles_instance_group* group_p = orbital_vfx_p->get_renderable_particles_instance_group();
                rl_corona_instance* head_p = group_p->m_coronas.first();
                for (rl_corona_instance* corona_p = head_p; corona_p != nullptr;)
                {
                    corona_p->set_infinite_depth(true);
                    corona_p->set_visibility_fade(corona_fade);
                    corona_p = corona_p->next;
                    if (corona_p == head_p)
                    {
                        break;
                    }
                }
            }
        }
    }
    const bool is_visible = visual_effect.do_update_render || game_is_paused();
    effect_p->update_render(Frametime, is_visible, pRenderer);
    if (Highest_priority_radial_blur == get_handle())
    {
        const visual_effect_info* info_p = object_effect_visual_info(visual_effect.handle);
        if (info_p != nullptr && info_p->radial_blur_info_p != nullptr)
        {
            fb_accum_blur_update_radial_blur(pos);
        }
    }
    if (m_post_update_render_cb != nullptr)
    {
        m_post_update_render_cb(this);
    }
}

// 0x00743EC0 (:1030)
float object_effect::get_visual_radius() const
{
    const visual_effect_info* info_p = object_effect_visual_info(visual_effect.handle);
    return info_p != nullptr ? info_p->radius : 0.0f;
}

// 0x007466B0 (:1041)
// With a visual: a cube of the larger of the template's bounding radius and the info's radius, in the object's
// transform; without one the local box at the position.
void object_effect::get_bbox(vector& bmin, vector& bmax) const
{
    const resource_dependent_data* data_p = rdd.get_data();
    if (data_p != nullptr && data_p->vfx_effect_p != nullptr)
    {
        float radius = 0.0f;
        const visual_effect_info* info_p = object_effect_visual_info(visual_effect.handle);
        if (info_p != nullptr)
        {
            radius = data_p->vfx_effect_p->get_template()->get_bounding_radius();
            if (radius <= info_p->radius)
            {
                radius = info_p->radius;
            }
        }
        const vector box_max(radius, radius, radius);
        const vector box_min(radius * -1.0f, radius * -1.0f, radius * -1.0f);
        bbox_instance(bmin, bmax, orient, pos, box_min, box_max);
        return;
    }
    get_local_bbox(bmin, bmax);
    bmin.x = bmin.x + pos.x;
    bmin.y = pos.y + bmin.y;
    bmin.z = pos.z + bmin.z;
    bmax.x = pos.x + bmax.x;
    bmax.y = pos.y + bmax.y;
    bmax.z = pos.z + bmax.z;
}

// 0x00743EF0 (:1066)
// A cube of the visual effect's radius (1 without one).
void object_effect::get_local_bbox(vector& bmin, vector& bmax) const
{
    float radius = 1.0f;
    const visual_effect_info* info_p = object_effect_visual_info(visual_effect.handle);
    if (info_p != nullptr)
    {
        radius = info_p->radius;
    }
    bmin.x = -radius;
    bmin.y = -radius;
    bmin.z = -radius;
    bmax.x = radius;
    bmax.y = radius;
    bmax.z = radius;
}

// 0x0074C010 (:1091)
void object_effect::free_this()
{
    release();
    object::free_this();
}

// 0x00743F50 (:1098)
void object_effect::attach_to_camera()
{
    attached_to_camera = true;
}

// 0x00743F60 (:1103)
void object_effect::attach_to_camera_orient()
{
    attached_to_camera_orient = true;
}

// 0x0074A870 (:1108)
// Sets a registered variable, registering it (from the pool of 3072) on first use.
void object_effect::set_expression_variable(object_effect_expression_variable_e variable_e, float value)
{
    registered_expression_variable* head_p = m_exp_vars.first();
    for (registered_expression_variable* var_p = head_p; var_p != nullptr;)
    {
        if (var_p->m_index == variable_e)
        {
            var_p->m_value = value;
            return;
        }
        var_p = var_p->next;
        if (var_p == head_p)
        {
            break;
        }
    }
    registered_expression_variable* var_p = ms_expression_var_pool.alloc(0);
    if (var_p == nullptr)
    {
        return;
    }
    var_p->m_index = variable_e;
    var_p->m_value = value;
    var_p->m_crc = Expression_var_lut[variable_e]->m_crc;
    m_exp_vars.append(var_p);
}

// 0x0074A920 (:1128)
void object_effect::hierarchy_notify(hierarchy_notify_code code)
{
    if (code == HN_PARENT_DESTROYED && stop_when_host_destroyed)
    {
        stop(SS_STOP_EMITTING);
    }
}

// 0x00746860 (:1135)
void object_effect::handle_host_death()
{
    if (stop_when_host_destroyed)
    {
        stop(SS_STOP_EMITTING);
    }
}

// 0x00746880 (:1142)
void object_effect::set_material_tint(const rl_color_float& material_tint)
{
    vfx_effect* effect_p = get_vfx_effect();
    if (effect_p != nullptr)
    {
        effect_p->set_effect_material_tint(material_tint);
    }
}

// 0x00743F70 (:1150)
// Moves the effect's count to the new category (Object_effect_count, for the explosion limits).
void object_effect::set_category(object_effect_category category)
{
    if (m_category != EFFECTS_CATEGORY_NONE)
    {
        --Object_effect_count[m_category];
    }
    m_category = category;
    if (category != EFFECTS_CATEGORY_NONE)
    {
        ++Object_effect_count[category];
    }
}

// 0x00743FA0 (:1161)
int object_effect::get_num_effects_in_category(object_effect_category category)
{
    return Object_effect_count[category];
}

// ---- inline members (object_effect.h)
// 0x0091CF80 (.h:191)
object_effect_category object_effect::get_category() const
{
    return m_category;
}

// 0x006CF300 (.h:292)
vfx_effect* object_effect::get_vfx_effect() const
{
    const resource_dependent_data* data_p = rdd.get_data();
    return data_p != nullptr ? data_p->vfx_effect_p : nullptr;
}

// 0x00BD9120 (.h:307)
void object_effect::set_pre_update_cb(void (*pre_update_cb)(object_effect*, void*))
{
    m_pre_update_cb = pre_update_cb;
}

// 0x00B1FC00 (.h:328)
void object_effect::set_post_update_render_cb(void (*post_update_render_cb)(object_effect*))
{
    m_post_update_render_cb = post_update_render_cb;
}

// 0x00BD9130 (.h:338)
void object_effect::set_update_cb_data(void* update_cb_data_p)
{
    update_cb_data = update_cb_data_p;
}

// 0x00B1FC10 (.h:343)
float object_effect::get_custom_cb_data() const
{
    return custom_cb_data;
}

// 0x00B1FC20 (.h:348)
void object_effect::set_custom_cb_data(float custom_cb_data_p)
{
    custom_cb_data = custom_cb_data_p;
}

// 0x007422C0 (.h:371)
void object_effect::set_effect_info(effect_info* effect_info_p)
{
    m_effect_info_p = effect_info_p;
}

// 0x00710E30 (.h:381)
void object_effect::set_local_position(const vector& local_position)
{
    m_local_position = local_position;
}

// 0x00710E50 (.h:392)
void object_effect::set_local_orient(const matrix& local_orient)
{
    m_local_orient = local_orient;
}

// 0x007422D0 (.h:397)
unsigned int object_effect::get_visual_effect_info_handle()
{
    return visual_effect.handle;
}

// ---- the vfx physics plugin (:1166)
// 0x00743FB0 (:1166)
// The shape of a rigid body renderable: one Havok collision model loaded from the template's data.
static rl_handle vfx_physics_shape_register_cb(void* data)
{
    rl_handle result;
    result.invalidate();
    unsigned int cm_chunk_size = 0;
    unsigned int num_collision_models = 1;
    unsigned int cm_index;
    if (havok_cm_manager_load_all(&cm_chunk_size, data, &cm_index, 1, &num_collision_models, nullptr))
    {
        result.set(cm_index);
    }
    return result;
}

// 0x00744000 (:1181)
void vfx_physics_shape_unregister_cb(rl_handle shape_handle)
{
    havok_cm_manager_remove_from_list(shape_handle.get());
}

// 0x007468A0 (:1187)
// A rigid body renderable's body: an object_debris (COLLIDE_EFFECT_RIGID_BODY) with the mass from the material's
// density and the template's volume, the material's friction and restitution, and the template's inertia (scaled by
// the mass) and center of mass. The handle is the debris object's.
static rl_handle vfx_physics_rigid_body_create_cb(vfx_physics_rigid_body_create_data* data)
{
    rl_handle result;
    result.invalidate();
    rfg_physical_material* material_p = rfg_get_physical_material_by_checksum(data->m_material_index);
    debris_create_info info;
    info.m_position = data->m_position;
    info.m_orientation = data->m_rotation;
    info.m_collision_type = COLLIDE_EFFECT_RIGID_BODY;
    info.m_mass = material_p->density * data->m_volume;
    info.m_friction = material_p->friction;
    info.m_restitution = material_p->restitution;
    info.m_physical_material = rfg_get_physical_material_index(material_p);
    info.m_effect_material = rfg_get_effect_material_index(material_p->effect_material);
    info.m_cm_index = data->m_shape_handle.get();
    info.m_precalc_tensor.rvec.x = data->m_inertia_tensor.rvec.x * info.m_mass;
    info.m_precalc_tensor.rvec.y = data->m_inertia_tensor.rvec.y * info.m_mass;
    info.m_precalc_tensor.rvec.z = data->m_inertia_tensor.rvec.z * info.m_mass;
    info.m_precalc_tensor.uvec.x = data->m_inertia_tensor.uvec.x * info.m_mass;
    info.m_precalc_tensor.uvec.y = data->m_inertia_tensor.uvec.y * info.m_mass;
    info.m_precalc_tensor.uvec.z = data->m_inertia_tensor.uvec.z * info.m_mass;
    info.m_precalc_tensor.fvec.x = data->m_inertia_tensor.fvec.x * info.m_mass;
    info.m_precalc_tensor.fvec.y = data->m_inertia_tensor.fvec.y * info.m_mass;
    info.m_precalc_tensor.fvec.z = data->m_inertia_tensor.fvec.z * info.m_mass;
    info.m_has_precalc_inertia = true;
    info.m_precalc_center_of_mass = data->m_center_of_mass;
    obj_debris_props debris_props(&info);
    // Original: no null check on the created object.
    object* objp = World.create_object<object_debris>(debris_props, 0xFFFFFFFF);
    result.set(objp->get_handle());
    return result;
}

// 0x00744020 (:1219)
static void vfx_physics_rigid_body_destroy_cb(rl_handle rigid_body_handle)
{
    if (rigid_body_handle.get() != 0xFFFFFFFF)
    {
        object* objp = World.object_from_handle(rigid_body_handle.get());
        if (objp != nullptr)
        {
            World.destroy_object(objp, false);
        }
    }
}

// 0x00744050 (:1232)
// Applies the renderable's forces to the body and returns its transform (identity once the body is gone).
static void vfx_physics_rigid_body_update_cb(vfx_physics_rigid_body_update_data& data)
{
    if (data.m_handle.get() == 0xFFFFFFFF)
    {
        return;
    }
    object* objp = World.object_from_handle(data.m_handle.get());
    if (objp != nullptr && objp->havok_handle != 0xFFFFFFFF)
    {
        havok_body_apply_linear_impulse(objp->havok_handle, data.m_force_vector);
        havok_body_apply_angular_impulse(objp->havok_handle, data.m_angular_force_vector);
        havok_body_get_pos_orient(objp->havok_handle, data.m_transform.m_translation, data.m_transform.m_rotation);
        return;
    }
    data.m_transform.m_rotation.make_identity();
    data.m_transform.m_translation.x = 0.0f;
    data.m_transform.m_translation.y = 0.0f;
    data.m_transform.m_translation.z = 0.0f;
}

// ---- the effect system (:1259)
// 0x0074A940 (:1259)
// The render task: while paused, with Effects_force_update_render, every effect's render data is built here (the
// objects' own update render does not run).
static void object_effect_update_render_all(void*, rl_renderer* pRenderer)
{
    if (!game_is_paused() || !Effects_force_update_render)
    {
        return;
    }
    const int num = World.get_num_objects<object_effect>();
    for (int i = 0; i < num; ++i)
    {
        World.get_object<object_effect>(i)->update_render(pRenderer);
    }
}

// 0x0074A9B0 (:1275)
// The frame's vfx view (the camera), the global wind filter from the weather's wind (mph to m/s, direction in degrees
// around up), the wind speed and particle load expression globals, then every effect's update.
void object_effect_process_all(float delta_time)
{
    vfx_system::set_view_orientation(Rfg_camera.real_orient);
    vfx_system::set_view_position(Rfg_camera.real_pos);
    float wind_mph;
    float wind_dir;
    weather_wind_get_speed(&wind_mph, &wind_dir);
    const float wind_speed = wind_mph * MPH_TO_METERS_PER_SECOND;
    matrix wind_rotation;
    wind_rotation.set(UP_VECTOR, wind_dir * DEGREES_TO_RADIANS);
    vfx_particle_system_filter_instance* wind_p = vfx_system::get_global_wind_instance();
    wind_p->set_world_rotation(wind_rotation);
    wind_p->set_force(wind_speed);
    vfx_expressions_set_global_variable(Expression_var_lut[EFFECTS_VAR_WIND_SPEED]->m_crc, wind_speed);
    const int parametric_max = vfx_particle_system_manager::get_particles_parametric_max();
    const int parametric_used = parametric_max - vfx_system::get_particle_system_manager()->get_num_particles_parametric_free();
    float particle_load = 0.0f;
    if (parametric_used != 0)
    {
        particle_load = (float)parametric_used / (float)parametric_max;
    }
    vfx_expressions_set_global_variable(Expression_var_lut[EFFECTS_VAR_PARTICLE_LOAD]->m_crc, particle_load);
    const int num = World.get_num_objects<object_effect>();
    for (int i = 0; i < num; ++i)
    {
        object_effect* effect_p = World.get_object<object_effect>(i);
        Last_effect = effect_p;
        effect_p->update(delta_time);
    }
}

// 0x0074AB80 (:1318)
int object_effect_get_count()
{
    return (int)object_effectpool.get_capacity() - object_effectpool.get_free();
}

// 0x007440C0 (:1332)
// Gives every registered expression variable its name crc and its slot in the lookup table.
static void register_expression_variables()
{
    expression_var* var_p = expression_var::registrator_list;
    while (var_p != nullptr)
    {
        var_p->m_crc = crc_stri(var_p->m_str_p, 0);
        Expression_var_lut[var_p->m_lut_index] = var_p;
        var_p = var_p->next;
        if (var_p == expression_var::registrator_list)
        {
            break;
        }
    }
}

// 0x0074AB90 (:1341)
void object_effect_system_init(mempool* permanent_pool)
{
    vfx_system_init(permanent_pool);
    vfx_physics_plugin_init(vfx_physics_shape_register_cb, vfx_physics_shape_unregister_cb, vfx_physics_rigid_body_create_cb, vfx_physics_rigid_body_destroy_cb, vfx_physics_rigid_body_update_cb);
    Update_render_task_object.m_callback = object_effect_update_render_all;
    game_render_update_render_task_register(&Update_render_task_object);
    Object_effect_count[0] = 0;
    Object_effect_count[1] = 0;
    Object_effect_count[2] = 0;
    register_expression_variables();
}

// 0x00744100 (:1365)
void object_effect_level_init_pre()
{
}

// 0x0074AC30 (:1371)
// Destroys every effect object.
void object_effect_level_shutdown()
{
    const int num = World.get_num_objects<object_effect>();
    for (int i = 0; i < num; ++i)
    {
        World.destroy_object(World.get_object<object_effect>(i), false);
    }
}

// 0x00744110 (:1385)
const object_pool<object_effect, 512, 1>& object_effect_get_objects()
{
    return object_effectpool;
}

// 0x00744120 (:1390)
const char* object_effect_get_expression_variable_name(object_effect_expression_variable_e variable_e)
{
    return Expression_var_lut[variable_e]->m_str_p;
}

// 0x00744130 (:1395)
void object_effect_expressions_set_global_variable(object_effect_expression_variable_e variable_e, float value)
{
    vfx_expressions_set_global_variable(Expression_var_lut[variable_e]->m_crc, value);
}
