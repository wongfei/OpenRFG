// Original: volition/rfg/code/objects/object_light.cpp
//
// obj_light, a light placed in a zone: the zone file's properties give its type (omni or spot), color, attenuation
// range, hotspot cone and flags (daytime, nighttime, use_clipping); loading creates its rl_light in the main scene
// (a point light, or a spot light along the object's -uvec) with a clip box, and streaming in attaches an optional
// clip mesh from the parent mover's chunk. update_render shows the light only in the hours its flags allow.

#include "volition/rfg/code/objects/object_light.h"

#include "volition/render_lib/code/rl_color.h"
#include "volition/render_lib/code/rl_compound_mesh_instance.h"
#include "volition/render_lib/code/rl_light.h"
#include "volition/render_lib/code/rl_sphere.h"
#include "volition/rfg/code/gamewide/game_clock.h"
#include "volition/rfg/code/gamewide/game_render.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/level/rfg_chunk_shared.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/movers/object_mover.h"
#include "volition/rfg/code/objects/object_prop_names_shared.h"
#include "volition/vlib/code/math/crc.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/memory/object_pool.h"
#include "volition/vlib/code/util/string_comp.h"

static const int OBJ_LIGHT_POOL_SIZE = 32; // obj_lightpool

// The light flags of the zone files (flags): shown by day, by night, and clipped to the clip box.
static const int OBJ_LIGHT_FLAG_DAYTIME = 0x1;
static const int OBJ_LIGHT_FLAG_NIGHTTIME = 0x2;
static const int OBJ_LIGHT_FLAG_USE_CLIPPING = 0x4;

// ---- globals
// Built by the variadic constructor (0x010778A0): the light type names of type_enum.
static str_data_hash_table<unsigned int, 32, 32, unsigned char> Light_type_table( // 0x03065A48
    "omni", (unsigned int)LIGHT_TYPE_OMNI,
    "spot", (unsigned int)LIGHT_TYPE_SPOT_CIRCLE,
    nullptr, 0u);

// Built by the variadic constructor (0x010778D0): the light flag names of light_flags.
static str_data_hash_table<unsigned int, 32, 32, unsigned char> Light_flags_table( // 0x031A3068
    "nighttime", (unsigned int)OBJ_LIGHT_FLAG_NIGHTTIME,
    "daytime", (unsigned int)OBJ_LIGHT_FLAG_DAYTIME,
    "use_clipping", (unsigned int)OBJ_LIGHT_FLAG_USE_CLIPPING,
    nullptr, 0u);

static object_pool<obj_light, OBJ_LIGHT_POOL_SIZE, 1> obj_lightpool; // 0x033C1450
static obj_class_instantiator obj_light_instantiator("obj_light", alloc_obj_light, types_obj_light, bpo_index_obj_light, classname_obj_light, obj_light::pool_remaining, 0x1D60, OBJ_LIGHT_POOL_SIZE); // 0x0339FB4C

// ---- class registration
// 0x00A199F0 (:58)
const char* classname_obj_light()
{
    return "obj_light";
}

// 0x00A66BF0 (:58)
object* alloc_obj_light()
{
    return obj_lightpool.alloc(0);
}

// 0x00A917D0 (:58)
void obj_light::pool_free()
{
    obj_lightpool.free(this);
}

// 0x00A199D0 (:58)
void types_obj_light(signed char& type, signed char& subtype)
{
    type = OT_LIGHT;
    subtype = OT_SUB_UNDEFINED;
}

// 0x00A3CBF0 (:58)
int obj_light::pool_remaining()
{
    return obj_lightpool.get_free();
}

// 0x00A19A00 (:58)
unsigned int obj_light::get_classname_hash()
{
    return obj_light_instantiator.this_hash;
}

// 0x00A199E0 (:58)
int bpo_index_obj_light()
{
    return 1;
}

// ---- obj_light (header inlines)
// 0x00A19930 (.h:40)
const object_type obj_light::get_type_static()
{
    return OT_LIGHT;
}

// 0x00A19940 (.h:40)
const object_sub_type obj_light::get_subtype_static()
{
    return OT_SUB_UNDEFINED;
}

// 0x00A19950 (.h:40)
const char* obj_light::get_classname_static()
{
    return "obj_light";
}

// 0x00A19960 (.h:40)
int obj_light::get_bpo_index_static()
{
    return 1;
}

// 0x00A19A30 (.h:40)
const char* obj_light::get_classname() const
{
    return "obj_light";
}

// ---- obj_light
// 0x00A19A10 (:59)
obj_light::obj_light()
{
}

// 0x00A19A40
obj_light::~obj_light()
{
}

// 0x00AA9030 (:71)
// Loading: the attenuation (atten_end, or atten_start + atten_range), the flags (daytime and nighttime by default),
// the type (light_type, or the type_enum name), the color and the hotspot angles (degrees; the falloff angle is added
// to the hotspot angle). A light reaching past 100 m is rejected. The rl_light is created at the object's frame: a
// spot light (spot types) with its bounding sphere half the range along -uvec, otherwise a point light; its clip box
// is min_clip/max_clip with use_clipping, else the bounding box. Saving writes the type, color, attenuation, hotspot
// and direction.
// Notes (decompiled behaviour, kept as is): saving writes no flags or clip properties; a rejected light keeps the
// base object's state.
bool obj_light::serialize(obj_prop_block& pblock, bool load)
{
    if (!object::serialize(pblock, load))
    {
        return false;
    }
    if (!load)
    {
        pblock.set(LIGHT_PROP_TYPE, light_type);
        pblock.set(LIGHT_PROP_COLOR, clr_orig);
        pblock.set(LIGHT_PROP_ATTEN_START, atten_start);
        pblock.set(LIGHT_PROP_ATTEN_END, atten_end);
        pblock.set(LIGHT_PROP_HOTSPOT_SIZE, hotspot_size);
        pblock.set(LIGHT_PROP_HOTSPOT_FALL_SIZE, hotspot_falloff_size);
        pblock.set(LIGHT_PROP_FVEC, fvec);
        return true;
    }
    flags = 0;
    obj_prop* prop;
    if (pblock.get_internal(prop, LIGHT_PROP_ATTEN_START, 5) && prop->size == 4)
    {
        atten_start = *(const float*)(prop + 1);
    }
    if (pblock.get_internal(prop, LIGHT_PROP_ATTEN_END, 5) && prop->size == 4)
    {
        atten_end = *(const float*)(prop + 1);
    }
    else
    {
        if (pblock.get_internal(prop, LIGHT_PROP_ATTEN_RANGE, 5) && prop->size == 4)
        {
            atten_end = *(const float*)(prop + 1);
        }
        atten_end = atten_start + atten_end;
    }
    flags = OBJ_LIGHT_FLAG_DAYTIME | OBJ_LIGHT_FLAG_NIGHTTIME;
    pblock.get_str_as_flags_optional(LIGHT_PROP_FLAGS, flags, Light_flags_table);
    // Magic number: lights reach at most 100 m.
    if (100.0f < atten_end)
    {
        return false;
    }
    light_type = LIGHT_TYPE_OMNI;
    if (!pblock.get_optional(LIGHT_PROP_TYPE, light_type))
    {
        pblock.get_str_as_enum_optional(LIGHT_PROP_TYPE_ENUM, light_type, Light_type_table);
    }
    pblock.get(LIGHT_PROP_COLOR, clr_orig);
    if (pblock.get_internal(prop, LIGHT_PROP_HOTSPOT_SIZE, 5) && prop->size == 4)
    {
        hotspot_size = *(const float*)(prop + 1);
    }
    if (pblock.get_internal(prop, LIGHT_PROP_HOTSPOT_FALL_SIZE, 5) && prop->size == 4)
    {
        hotspot_falloff_size = *(const float*)(prop + 1);
    }
    // Magic number: 0.017453, degrees to radians.
    hotspot_size = hotspot_size * 0.017453f;
    hotspot_falloff_size = hotspot_falloff_size * 0.017453f + hotspot_size;
    fvec = vector(-orient.uvec.x, -orient.uvec.y, -orient.uvec.z);
    clr = clr_orig;

    m_light_p = rl_light::create();
    m_light_p->set_position(pos);
    m_light_p->set_rotation(orient);
    vector default_clip_min;
    vector default_clip_max;
    if (light_type == LIGHT_TYPE_SPOT_RECT || light_type == LIGHT_TYPE_SPOT_CIRCLE)
    {
        m_light_p->set_light_type(rl_light::LIGHT_TYPE_SPOT_LIGHT);
        const float half_range = atten_end * 0.5f;
        const vector center(half_range * fvec.x, fvec.y * half_range, fvec.z * half_range);
        default_clip_min = vector(center.x - half_range, center.y - half_range, center.z - half_range);
        default_clip_max = vector(center.x + half_range, center.y + half_range, center.z + half_range);
        m_light_p->set_local_bounding_sphere(rl_sphere(center, half_range));
    }
    else
    {
        m_light_p->set_light_type(rl_light::LIGHT_TYPE_POINT);
        default_clip_min = vector(atten_end * -1.0f, atten_end * -1.0f, atten_end * -1.0f);
        default_clip_max = vector(atten_end, atten_end, atten_end);
        m_light_p->set_local_bounding_sphere(rl_sphere(ZERO_VECTOR, atten_end));
    }
    vector clip_min;
    vector clip_max;
    if (pblock.get_optional(LIGHT_PROP_CLIP_MIN, clip_min) && pblock.get_optional(LIGHT_PROP_CLIP_MAX, clip_max) && (flags & OBJ_LIGHT_FLAG_USE_CLIPPING) != 0)
    {
        m_light_p->set_clip_min(clip_min);
        m_light_p->set_clip_max(clip_max);
    }
    else
    {
        m_light_p->set_clip_min(default_clip_min);
        m_light_p->set_clip_max(default_clip_max);
    }
    char clip_mesh_name[64];
    if (pblock.get_optional(LIGHT_PROP_CLIP_MESH, clip_mesh_name) && v_stricmp(clip_mesh_name, "NONE") != 0)
    {
        m_clip_mesh_name_crc = crc_stri(clip_mesh_name, 0);
    }
    else
    {
        m_clip_mesh_name_crc = 0;
    }
    m_light_p->set_color(rl_color_float(clr_orig.x, clr_orig.y, clr_orig.z, 1.0f));
    m_light_p->set_attenuation_start(atten_start);
    m_light_p->set_attenuation_end(atten_end);
    m_light_p->set_hotspot_size(hotspot_size);
    m_light_p->set_hotspot_falloff_size(hotspot_falloff_size);
    game_render_scene_update(m_light_p);
    // The streaming box clamped to the safe Havok broadphase.
    for (int axis = 0; axis < 3; axis++)
    {
        float* bmin_p = &last_known_bmin.x + axis;
        float* bmax_p = &last_known_bmax.x + axis;
        const float safe_min = (&Havok_safe_broadphase.min.x)[axis];
        const float safe_max = (&Havok_safe_broadphase.max.x)[axis];
        if (*bmin_p - safe_min < 0.0f)
        {
            *bmin_p = safe_min;
        }
        if (0.0f <= *bmin_p - safe_max)
        {
            *bmin_p = safe_max;
        }
        if (*bmax_p - safe_min < 0.0f)
        {
            *bmax_p = safe_min;
        }
        if (0.0f <= *bmax_p - safe_max)
        {
            *bmax_p = safe_max;
        }
    }
    return true;
}

// 0x00A66C00 (:223)
// Streaming in: with a clip mesh name and a mover parent, the light clip object of that name in the parent's chunk
// becomes a hidden compound mesh instance (at the clip object's frame in the mover's frame) that clips the light.
// Streaming out releases it.
bool obj_light::stream(bool load)
{
    if (!load)
    {
        if (m_clip_inst_p != nullptr)
        {
            m_light_p->set_clip_mesh(nullptr);
            m_clip_inst_p->release();
            m_clip_inst_p = nullptr;
        }
        return true;
    }
    if (m_clip_mesh_name_crc == 0 || get_parent() == 0xFFFFFFFF)
    {
        return true;
    }
    object* parent_p = World.object_from_handle(get_parent());
    if (parent_p == nullptr || parent_p->get_obj_type() != OT_MOVER)
    {
        return true;
    }
    c_chunk_base* chunk_p = chunk_get_base_by_uid(((object_mover*)parent_p)->chunk_uid);
    if (chunk_p == nullptr)
    {
        return true;
    }
    int index = 0;
    const c_light_clip_object* clip_objects = chunk_p->light_clip_objects.to_pointer();
    for (; index < chunk_p->num_light_clip_objects; index++)
    {
        if (crc_stri(clip_objects[index].name.to_pointer(), 0) == m_clip_mesh_name_crc)
        {
            break;
        }
    }
    if (index >= chunk_p->num_light_clip_objects)
    {
        return true;
    }
    m_clip_inst_p = rl_compound_mesh_instance::create();
    if (m_clip_inst_p == nullptr)
    {
        return true;
    }
    const c_light_clip_object& clip = clip_objects[index];
    rl_sub_instance_info info;
    info.m_submesh_id = clip.submesh_idx;
    info.m_disable_deferred = 0;
    info.m_is_active = 1;
    vector clip_pos = clip.pos;
    clip_pos.unrotate(parent_p->orient);
    clip_pos = vector(parent_p->pos.x + clip_pos.x, parent_p->pos.y + clip_pos.y, parent_p->pos.z + clip_pos.z);
    const matrix clip_orient = matrix(clip.orient) * parent_p->orient;
    info.m_transform.set_orientation(clip_orient);
    info.m_transform.set_translation(clip_pos);
    m_clip_inst_p->init(chunk_p->mesh_p.to_pointer(), chunk_p->material_map_p.to_pointer(), &info, 1);
    if (m_clip_inst_p->get_is_visible())
    {
        m_clip_inst_p->set_is_visible(false);
    }
    m_light_p->set_clip_mesh(m_clip_inst_p);
    return true;
}

// 0x00A917E0 (:297)
void obj_light::free_this()
{
    object::free_this();
    m_light_p->release();
}

// 0x00A19A50 (:304)
void obj_light::get_bbox(vector& bmin, vector& bmax) const
{
    get_local_bbox(bmin, bmax);
    bmin = vector(bmin.x + pos.x, pos.y + bmin.y, pos.z + bmin.z);
    bmax = vector(pos.x + bmax.x, pos.y + bmax.y, pos.z + bmax.z);
}

// 0x00A19B20 (:312)
// A 1 m box around the light.
void obj_light::get_local_bbox(vector& bmin, vector& bmax) const
{
    bmin = vector(-0.5f, -0.5f, -0.5f);
    bmax = vector(0.5f, 0.5f, 0.5f);
}

// 0x00A19B60 (:318)
// Shown when the game clock's time of day is one its flags allow (daytime, nighttime), hidden otherwise.
void obj_light::update_render(rl_renderer*)
{
    const bool day_light = game_clock_get_time_of_day() == GC_DAY && (flags & OBJ_LIGHT_FLAG_DAYTIME) != 0;
    const bool night_light = game_clock_get_time_of_day() == GC_NIGHT && (flags & OBJ_LIGHT_FLAG_NIGHTTIME) != 0;
    if (day_light || night_light)
    {
        if (!m_light_p->get_is_visible())
        {
            m_light_p->set_is_visible(true);
        }
        return;
    }
    if (m_light_p->get_is_visible())
    {
        m_light_p->set_is_visible(false);
    }
}

// ---- functions
// 0x00A19BD0 (:335)
void object_light_level_init_pre()
{
}

// 0x00A19BE0 (:339)
void object_light_system_init()
{
}

// 0x00A19BF0 (:343)
void object_light_system_shutdown()
{
}
