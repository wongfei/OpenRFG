// Original: volition/rfg/code/objects/object_debris.cpp
//
// object_debris (decompiled; header comment in object_debris.h). Compiled in rfg_objects_unity.cpp.
//
// Debris pieces (vehicle parts, props knocked off) are physics bodies created from a debris_create_info, with a mesh
// instance (their own, or the source's static mesh). They stay at least 5 s; in single player (and on a server)
// non-persistent debris more than 70 m from the player and behind the camera disappears. Fading debris (from the load
// balancer or explosions) is removed when its fade time is up.
//
// Notes (decompiled behaviour, kept as is):
// - object_debris_free_all and the console command walk the type list while destroying from it.

#include "volition/rfg/code/objects/object_debris.h"

#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/rfg/code/code_effects/explosion.h"
#include "volition/rfg/code/effects/object_effect.h"
#include "volition/rfg/code/geom/static_mesh_shared.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/level/world.inl"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/object_prop_names_shared.h"
#include "volition/render_lib/code/rl_mesh.h"
#include "volition/render_lib/code/rl_mesh_instance.h"
#include "volition/render_lib/code/rl_renderable_instance.h"
#include "volition/stream2/code/stream2.h"
#include "volition/vlib/code/math/bbox.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/memory/object_pool.h"
#include "volition/vlib/code/os/vconsole.h"
#include "volition/rfg/code/gamewide/game_render.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hkreferencedobject.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshape.h"

static const int OBJECT_DEBRIS_POOL_SIZE = 500;
static const int OBJECT_DEBRIS_POOL_USAGE = 0x32EC0; // memory footprint registered with the instantiator
static const int MIN_EXISTENCE_TIME = 5000; // ms before debris may disappear
static const float FAR_FROM_PLAYER_SQR = 4900.0f; // 70 m
static const float MAX_EXPLOSION_LINEAR_VELOCITY = 10.0f;
static const float MAX_EXPLOSION_ANGULAR_VELOCITY = 31.415928f; // 5 turns per second

// ---- file-static globals
static const float DEFAULT_BUILDING_DAMAGE_SOUND_DIST = 30.0f; // 0x01311004
static int Retrievable_salvage_delay = 500; // 0x01694580
static bool Object_debris_init; // 0x0302F134
static object_pool<object_debris, OBJECT_DEBRIS_POOL_SIZE, 1> object_debrispool; // 0x030C4F50
static obj_class_instantiator object_debris_instantiator("object_debris", alloc_object_debris, types_object_debris, bpo_index_object_debris, classname_object_debris, object_debris::pool_remaining, OBJECT_DEBRIS_POOL_USAGE, OBJECT_DEBRIS_POOL_SIZE); // 0x0339FB3C (0x01076FB0)

// 0x00A628A0 (:70)
// Console command: destroys all debris.
void unused_dcf_clear_debris()
{
    if (Console_command == 0)
    {
        return;
    }
    const int num = World.get_num_objects<object_debris>();
    for (int i = 0; i < num; i++)
    {
        World.destroy_object(World.get_object<object_debris>(i), false); // kept: the list shrinks while it is walked
    }
}

// 0x00A62900 (:101)
static void object_debris_free_all()
{
    const int num = World.get_num_objects<object_debris>();
    for (int i = 0; i < num; i++)
    {
        World.destroy_object(World.get_object<object_debris>(i), false); // kept: the list shrinks while it is walked
    }
}

// 0x00A3A990 (:109)
// Placement and the whole create info.
obj_debris_props::obj_debris_props(const debris_create_info* ci)
{
    object_properties combined;
    combined.pos = ci->m_position;
    combined.orient = ci->m_orientation;
    set(OBJ_PROP_COMBINED, combined);
    set(DEBRIS_PROP_CI, *ci);
}

// 0x00744010
obj_debris_props::~obj_debris_props()
{
}

// ---- class registration (:128)
// 0x00A8FFB0
void object_debris::pool_free()
{
    object_debrispool.free(this);
}

// 0x00A17140
const char* classname_object_debris()
{
    return "object_debris";
}

// 0x00A3AA90
int object_debris::pool_remaining()
{
    return object_debrispool.get_free();
}

// 0x00A62960
object* alloc_object_debris()
{
    return object_debrispool.alloc(0);
}

// 0x00A17150
unsigned int object_debris::get_classname_hash()
{
    return object_debris_instantiator.this_hash;
}

// 0x00A17130
int bpo_index_object_debris()
{
    return 0;
}

// 0x00A17120
void types_object_debris(signed char& type, signed char& subtype)
{
    type = OT_DEBRIS;
    subtype = OT_SUB_UNDEFINED;
}

// 0x00A17160 (:129)
object_debris::object_debris()
{
    // the timestamps invalidate themselves, debris_create_info() clears the create info
}

// 0x00A171C0
object_debris::~object_debris()
{
}

// 0x00A171D0 (:138)
void object_debris::get_bbox(vector& bmin, vector& bmax) const
{
    vector local_min;
    vector local_max;
    get_local_bbox(local_min, local_max);
    bbox_instance(bmin, bmax, orient, pos, local_min, local_max);
}

// 0x00A62970 (:154)
// The submesh's box (of the static mesh, or of the instance's mesh); 1 m without an instance.
void object_debris::get_local_bbox(vector& bmin, vector& bmax) const
{
    if (m_renderable_instance_p == nullptr)
    {
        bmin.set(-0.5f, -0.5f, -0.5f);
        bmax.set(0.5f, 0.5f, 0.5f);
        return;
    }
    if (m_mesh == nullptr)
    {
        ((const rl_mesh*)m_renderable_instance_p->get_renderable())->get_subpiece_local_bb(m_submesh_id, bmin, bmax);
        return;
    }
    m_mesh->m_shared_header.m_mesh.to_pointer()->get_subpiece_local_bb(m_submesh_id, bmin, bmax);
}

// 0x00A17220 (:169)
// Movable debris is pushed against the hit normal by the damage.
void object_debris::process_bullet_hit(human* attacker, float damage, const weapon_fire_info& fire_info)
{
    if (havok_handle == 0xFFFFFFFF || !havok_body_is_movable(havok_handle))
    {
        return;
    }
    const vector impulse(-fire_info.hit_normal.x * damage, -fire_info.hit_normal.y * damage, -fire_info.hit_normal.z * damage);
    havok_body_apply_point_impulse(havok_handle, impulse, fire_info.hit_pos);
}

// 0x00A172F0 (:182)
// A debris quality body (removable at level 4) of the given shape (or collision model), with the precalculated inertia
// when there is one.
void object_debris::create_havok_body(const debris_create_info* dci)
{
    physics_create_info pci;
    pci.collision_type = dci->m_collision_type;
    pci.pos = pos;
    pci.orient = orient;
    pci.physical_material = dci->m_physical_material;
    pci.effect_material = dci->m_effect_material;
    pci.cm_index = dci->m_cm_index;
    pci.shape = dci->m_shape;
    pci.mass = dci->m_mass;
    pci.friction = dci->m_friction;
    pci.restitution = dci->m_restitution;
    pci.linear_damping = dci->m_linear_damp;
    pci.shape_type = (havok_shape_type)((pci.shape != nullptr ? 1 : 0) + HAVOK_SHAPE_TYPE_COLLISION_MODEL);
    pci.angular_damping = dci->m_angular_damp;
    pci.simulation_quality_type = HK_COLLIDABLE_QUALITY_DEBRIS;
    pci.solver_deactivation = hkpRigidBodyCinfo::SOLVER_DEACTIVATION_MEDIUM;
    pci.auto_remove_level = 4;
    pci.flags |= 4;
    if (dci->m_has_precalc_inertia)
    {
        pci.precalc_tensor = dci->m_precalc_tensor;
        pci.precalc_center_of_mass = dci->m_precalc_center_of_mass;
        pci.flags |= 1;
    }
    pci.obj_ptr = this;
    havok_handle = havok_body_create(pci);
}

// 0x00A17490 (:229)
void object_debris::free_havok_body()
{
    if (havok_handle != 0xFFFFFFFF)
    {
        havok_body_free(havok_handle, true, true, false);
        havok_handle = 0xFFFFFFFF;
    }
}

// 0x00AA65D0 (:245)
// Fading debris is not saved. Saved: the create info with the current materials, mesh, sounds and timers. Loaded: the
// mesh stays referenced (the shape too), at least 5 s of life, retrievable salvage after Retrievable_salvage_delay.
bool object_debris::serialize(obj_prop_block& pblock, bool load)
{
    if (!load && m_fade_stamp.valid())
    {
        return false;
    }
    if (!object::serialize(pblock, load))
    {
        return false;
    }
    if (!load)
    {
        debris_create_info ci = m_create_info;
        ci.m_physical_material = get_physical_material(0xFFFFFFFF, nullptr, nullptr);
        ci.m_effect_material = get_effect_material(0xFFFFFFFF, nullptr, nullptr);
        ci.m_renderable_instance_p = m_renderable_instance_p;
        ci.m_mesh = m_mesh;
        ci.m_num_lod_infos = m_num_lod_infos;
        ci.m_submesh_id = m_submesh_id;
        ci.m_is_persistent = m_is_persistent;
        ci.m_lod_info = m_lod_info;
        ci.m_impact_foley_id = m_impact_foley_id;
        ci.m_is_retrievable = m_retrievable_delay.valid();
        ci.m_retrievable_material_type = m_retrievable_material_type;
        ci.m_delay_before_first_impact_foley = m_impact_foley_delay.time_until() < 1 ? 0 : m_impact_foley_delay.time_until();
        pblock.set(DEBRIS_PROP_CI, ci);
        return true;
    }
    m_create_info = debris_create_info();
    fade_ms = 0;
    debris_flags = 0;
    fading_from_nano = false;
    m_fade_stamp.invalidate();
    pblock.get(DEBRIS_PROP_CI, m_create_info); // inlined
    m_mesh = m_create_info.m_mesh;
    m_renderable_instance_p = m_create_info.m_renderable_instance_p;
    m_submesh_id = m_create_info.m_submesh_id;
    if (m_create_info.m_mesh_srid != 0)
    {
        request_streaming_resource(m_create_info.m_mesh_srid, STREAM_PRIORITY_AUDIO_CACHE_LOAD);
    }
    if (m_create_info.m_shape != nullptr)
    {
        m_create_info.m_shape->addReference();
    }
    if (m_renderable_instance_p != nullptr)
    {
        stream2_request(m_create_info.m_mesh_srid);
        debris_flags |= 2; // releases the mesh container when freed
    }
    m_lod_info = m_create_info.m_lod_info;
    m_num_lod_infos = m_create_info.m_num_lod_infos;
    m_impact_foley_id = m_create_info.m_impact_foley_id;
    m_min_existance_time.set(MIN_EXISTENCE_TIME);
    m_is_persistent = m_create_info.m_is_persistent;
    if (!m_create_info.m_is_retrievable)
    {
        m_retrievable_delay.invalidate();
    }
    else
    {
        m_retrievable_delay.set(Retrievable_salvage_delay);
    }
    m_retrievable_material_type = m_create_info.m_retrievable_material_type;
    if (m_create_info.m_delay_before_first_impact_foley == 0)
    {
        m_impact_foley_delay.invalidate();
    }
    else
    {
        m_impact_foley_delay.set(m_create_info.m_delay_before_first_impact_foley);
    }
    m_fade_stamp.invalidate();
    m_effect_end.invalidate();
    m_effect_instance = 0xFFFFFFFF;
    fading_from_nano = false;
    return true;
}

// 0x00A175D0 (:360)
obj_serialize_mode object_debris::serialize_mode()
{
    return OSM_NONE;
}

// 0x00A62A10 (:373)
// Streamed out: the debris goes (instance, effect and body released). Streamed in: a mesh instance of the static
// mesh's lod 0 submesh when it has none, then its body.
bool object_debris::stream(bool load)
{
    if (!load)
    {
        if (obj_flags.destroy_on_stream)
        {
            World.destroy_object(this, false);
        }
        if (m_renderable_instance_p != nullptr)
        {
            m_renderable_instance_p->release();
            m_renderable_instance_p = nullptr;
        }
        if (m_effect_instance != 0xFFFFFFFF)
        {
            object_effect* effect = World.object_from_handle<object_effect>(m_effect_instance);
            if (effect != nullptr)
            {
                effect->stop(object_effect::SS_STOP_EMITTING);
            }
            m_effect_instance = 0xFFFFFFFF;
            m_effect_end.invalidate();
        }
        free_havok_body(); // inlined
        World.destroy_object(this, false);
        return true;
    }
    if (m_renderable_instance_p == nullptr && m_mesh != nullptr)
    {
        rl_mesh_instance* instance = rl_mesh_instance::create();
        m_renderable_instance_p = instance;
        if (instance != nullptr)
        {
            instance->init(m_mesh->m_shared_header.m_mesh.to_pointer(), m_mesh->m_shared_header.m_material_map.to_pointer(), *m_mesh->m_lod_submesh_id.to_pointer());
            instance->calculate_local_aabb();
            m_submesh_id = *m_mesh->m_lod_submesh_id.to_pointer();
        }
    }
    create_havok_body(&m_create_info);
    return true;
}

// 0x00A8FFC0 (:445)
void object_debris::free_this()
{
    if (m_renderable_instance_p != nullptr)
    {
        m_renderable_instance_p->release();
        m_renderable_instance_p = nullptr;
    }
    if (m_create_info.m_shape != nullptr)
    {
        m_create_info.m_shape->removeReference();
    }
    if ((debris_flags & 2) != 0)
    {
        stream2_release(get_srid());
    }
    object::free_this();
}

// 0x00A3AAC0 (:476)
// The instance follows the object; while fading (not waiting) it is hidden once the fade is up and, unless fading
// from the nano rifle, its opacity is the remaining fade in seconds. In single player (and on a server) debris that is
// not persistent and older than 5 s disappears when it is more than 70 m from the player and behind the camera.
void object_debris::update_render(rl_renderer*)
{
    rl_renderable_instance* instance = m_renderable_instance_p;
    if (instance != nullptr)
    {
        instance->set_position(pos); // inlined
        instance->set_rotation(orient); // inlined
        if (m_fade_stamp.valid() && (debris_flags & 1) == 0)
        {
            if (m_fade_stamp.time_until() < 1)
            {
                m_renderable_instance_p->set_is_visible(false); // inlined
            }
            if (!fading_from_nano)
            {
                m_renderable_instance_p->set_opacity((float)m_fade_stamp.time_until() / 1000.0f); // inlined
            }
        }
        else
        {
            m_renderable_instance_p->set_opacity(1.0f); // inlined
        }
        game_render_scene_update(m_renderable_instance_p);
    }
    if (Multiplayer && !multi_is_server())
    {
        return;
    }
    if (m_is_persistent || !m_min_existance_time.elapsed())
    {
        return;
    }
    const vector to_debris = pos - Rfg_camera.real_pos;
    if (Player != nullptr && FAR_FROM_PLAYER_SQR < vec_dist_squared(Player->pos, pos) && Rfg_camera.real_orient.fvec.x * to_debris.x + Rfg_camera.real_orient.fvec.y * to_debris.y + Rfg_camera.real_orient.fvec.z * to_debris.z < 0.0f)
    {
        World.destroy_object(this, false);
    }
}

// 0x00A175E0 (:535)
// After time_until_fade (flag 0x1 while waiting), or at once, over fade_time.
void object_debris_mark_for_fade(object_debris* d_obj, int time_until_fade, int fade_time, bool from_nano)
{
    if (0 < time_until_fade)
    {
        d_obj->debris_flags |= 1;
        d_obj->fade_ms = fade_time;
        d_obj->m_fade_stamp.set(time_until_fade);
        d_obj->fading_from_nano = from_nano;
        return;
    }
    d_obj->m_fade_stamp.set(fade_time);
    d_obj->fading_from_nano = from_nano;
}

// 0x00A17640 (:549)
bool object_debris_is_fading(object_debris* d_obj)
{
    return d_obj != nullptr && d_obj->m_fade_stamp.valid();
}

// 0x00A62B20 (:556)
// Once per frame: temporary debris (not persistent, not loaded from a save, _dont_use_me) goes when its existence time
// is up (single player and servers); other debris stops its effect when it ends and, while fading, goes when the fade
// is up (a delayed fade starts after its wait) and otherwise is queued for the render update.
void object_debris_process()
{
    const int num = World.get_num_objects<object_debris>();
    for (int i = 0; i < num; i++)
    {
        object_debris* debris = World.get_object<object_debris>(i);
        if (!debris->m_is_persistent && !debris->obj_flags._dont_use_me)
        {
            if ((!Multiplayer || multi_is_server()) && debris->m_min_existance_time.elapsed())
            {
                World.destroy_object(debris, false);
            }
            continue;
        }
        if (debris->m_effect_instance != 0xFFFFFFFF && debris->m_effect_end.elapsed())
        {
            object_effect* effect = World.object_from_handle<object_effect>(debris->m_effect_instance);
            if (effect != nullptr)
            {
                effect->stop(object_effect::SS_STOP_EMITTING);
            }
            debris->m_effect_instance = 0xFFFFFFFF;
        }
        if (!debris->m_fade_stamp.valid())
        {
            continue;
        }
        if ((debris->debris_flags & 1) != 0)
        {
            if (debris->m_fade_stamp.elapsed())
            {
                debris->debris_flags &= ~1;
                debris->m_fade_stamp.set(debris->fade_ms);
            }
        }
        else if (debris->m_fade_stamp.elapsed())
        {
            World.destroy_object(debris, false);
        }
        else if (!debris->fading_from_nano)
        {
            debris->set_render_flags(2); // inlined
        }
    }
}

// 0x00A17660 (:622)
// Impulses limited to 10 m/s and 5 turns/s (half strength); hard enough hits fade deletable debris.
void object_debris_process_explosion_hit(object_debris* d_obj, const vector* linear_impulse, const vector* angular_impulse, bool can_delete)
{
    vector linear;
    vector angular;
    explosion_modify_impulses(&linear, &angular, linear_impulse, angular_impulse, d_obj->get_mass(), 0.5f, 0.5f, MAX_EXPLOSION_LINEAR_VELOCITY, MAX_EXPLOSION_ANGULAR_VELOCITY);
    const float mass = d_obj->m_create_info.m_mass;
    // 2200 / 25 m/s per kg of mass (squared both sides)
    if (can_delete && mass * mass * 4.84e+06f < (linear_impulse->x * linear_impulse->x + linear_impulse->y * linear_impulse->y + linear_impulse->z * linear_impulse->z) * 625.0f)
    {
        d_obj->m_fade_stamp.set(1000);
        d_obj->fading_from_nano = false;
    }
    havok_body_apply_linear_impulse(d_obj->havok_handle, linear);
    havok_body_apply_angular_impulse(d_obj->havok_handle, angular);
}

// 0x00A17790 (:656)
bool object_debris::process_melee_hit(object* attacker, float damage, const vector& impulse, const vector& hit_pos, const vector& hit_normal, unsigned int shape_key, const weapon_info& w_info)
{
    if (havok_handle != 0xFFFFFFFF)
    {
        havok_body_apply_point_impulse(havok_handle, impulse, hit_pos);
    }
    return true;
}

// 0x00A177B0 (:670)
// Its impact sound for hits faster than 2.5 m/s, at most every 100 ms (after the first delay).
void object_debris_play_collision_sound(object_debris* d_obj, const vector* position, float speed_along_normal)
{
    if (d_obj->m_impact_foley_id < 0)
    {
        return;
    }
    if (d_obj->m_impact_foley_delay.valid() && !d_obj->m_impact_foley_delay.elapsed())
    {
        return;
    }
    if (fl_parameter(speed_along_normal, 2.5f, 30.0f, true) == 0.0f)
    {
        return;
    }
    audiolib_cue_play_3d(d_obj->m_impact_foley_id, position, nullptr, nullptr, nullptr);
    d_obj->m_impact_foley_delay.set(100);
}

// 0x00A17850 (:708)
void object_debris_system_init()
{
    if (!Object_debris_init)
    {
        Object_debris_init = true;
    }
}

// 0x00A17870 (:717)
void object_debris_system_shutdown()
{
    if (Object_debris_init)
    {
        Object_debris_init = false;
    }
}

// 0x00A17890 (:728)
void object_debris_level_init_post()
{
}

// 0x00A62CB0 (:734)
void object_debris_level_shutdown()
{
    object_debris_free_all(); // inlined
}

// 0x00743320 (.h:43)
debris_create_info::debris_create_info()
{
    m_position = ZERO_VECTOR;
    m_orientation = IDENTITY_MATRIX;
    m_mass = 0.0f;
    m_friction = 0.0f;
    m_restitution = 0.0f;
    m_linear_damp = 0.01f;
    m_angular_damp = 0.05f;
    m_mesh_srid = 0;
    m_renderable_instance_p = nullptr;
    m_mesh = nullptr;
    m_submesh_id = 0;
    m_num_lod_infos = 0;
    m_lod_info = nullptr;
    m_collision_type = COLLIDE_PHYSICAL_OBJECT;
    m_cm_index = 0xFFFFFFFF;
    m_impact_foley_id = -1;
    m_deformed_morph_target = -1;
    m_shape = nullptr;
    m_delay_before_first_impact_foley = 0;
    m_is_persistent = false;
    m_has_precalc_inertia = false;
    m_precalc_tensor = IDENTITY_MATRIX;
    m_precalc_center_of_mass = ZERO_VECTOR;
}

// 0x007115E0 (.h:129)
const object_type object_debris::get_type_static()
{
    return OT_DEBRIS;
}

// 0x00A171B0 (.h:129)
const char* object_debris::get_classname() const
{
    return "object_debris";
}

// 0x00A13ED0 (.h:129)
int object_debris::get_bpo_index_static()
{
    return 0;
}

// 0x007115F0 (.h:129)
const object_sub_type object_debris::get_subtype_static()
{
    return OT_SUB_UNDEFINED;
}

// 0x00A13EC0 (.h:129)
const char* object_debris::get_classname_static()
{
    return "object_debris";
}
