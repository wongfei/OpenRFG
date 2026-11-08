// Original: volition/rfg/code/objects/object.cpp
//
// Partial: the object base class (every virtual, the child list, names, streaming flags), the class registry
// (obj_class_instantiator, object.inl in the original) and the small free helpers. Not implemented yet: the attached
// child transforms (set_child with prop points, adjust_child_transforms, adjust_transforms; object_do_frame_postsim since MP8)
// and the debug commands. The level hooks (object_level_init_pre / _post / _shutdown) since multiplayer.md MP1.
//
// The registry: every class registers itself from the constructor of its global <class>_instantiator, under
// str_hash_string(class name) % hash_size (762). Registration order fills class_index. A lookup of an unregistered hash
// finds an empty bucket (null alloc function) and returns -1 / nullptr; the helpers that call through type_funcs
// without that check (get_type, object_type_by_hash) assume a registered class.
//
// Objects queue themselves for the render update (Object_update_render_list) whenever their placement, visibility or
// render flags change; obj_flags.render_flags remembers what changed (1 visibility, 4 placement).
//
// Notes (decompiled behaviour, kept as is):
// - serialize(load) reads the position from OBJ_PROP_COMBINED into an uninitialized local when neither it nor
//   OBJ_PROP_JUST_POS is present.
// - set_child / remove_child free attach_info into Attach_info_pool (file static in human.cpp).

#include "volition/rfg/code/objects/object.h"

#include "keen/base/trace.hpp"
#include "volition/rfg/code/code_effects/glass.h"
#include "volition/rfg/code/destruction/rfg_materials.h"
#include "volition/rfg/code/destruction/rfg_mover.h"
#include "volition/rfg/code/effects/object_effect.h"
#include "volition/rfg/code/game_render/game_render_task.h"
#include "volition/rfg/code/gamewide/game_render.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/havok/havok_bp.h"
#include "volition/rfg/code/havok/havok_collision_events.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_game/multi_game_object_movers.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/movers/object_mover.h"
#include "volition/rfg/code/objects/object_constraint.h"
#include "volition/rfg/code/objects/object_safehouse.h"
#include "volition/rfg/code/objects/object_debris.h"
#include "volition/rfg/code/objects/object_item.h"
#include "volition/rfg/code/objects/object_prop_names_shared.h"
#include "volition/rfg/code/objects/object_skybox.h"
#include "volition/rfg/code/objects/projectile.h"
#include "volition/rfg/code/objects/turret.h"
#include "volition/rfg/code/scripting/public_address.h"
#include "volition/rfg/code/vehicle/vehicle_info.h"
#include "volition/vlib/code/memory/object_pool.h"
#include "volition/vlib/code/util/string_util.h"

#include <stdio.h>
#include <string.h>

// ---- globals
float Shard_fade_dist0_sqr = 1225.0f; // 0x0169453C, 35 m: shards with an odd subpiece index fade beyond this
float Shard_fade_dist1_sqr = 2500.0f; // 0x01694540, 50 m: the other shards
float RFG_object_energy_scale = 1.5f; // 0x01694544 (.data)

// ---- file-static globals
static farray<object*,16384> Object_update_render_list; // 0x0169D028
// The render update task of the objects (registered by object_system_init).
static game_render_task Object_update_render_task; // 0x030FCDDC
// projectile.cpp's charges and object_safehouse.cpp's safehouse of the player, placed in this earlier file of the unity
// (object_level_init_pre / _post write them).
static projectile* Charge_list; // 0x0302F220

// File static in human.cpp, shared with this file through LTCG; defined by the human TU (or its stand-in).
extern object_pool<attach_info_data,512,1> Attach_info_pool; // 0x0304BAA0

// ---- class registry (object.inl in the original)

bool obj_class_instantiator::li_init; // 0x0302F11D
int obj_class_instantiator::hash_size = 762; // 0x01694538
object* (*obj_class_instantiator::alloc_funcs[768])(); // 0x0301AFD0
void (*obj_class_instantiator::type_funcs[768])(signed char&, signed char&); // 0x0301BBD0
int (*obj_class_instantiator::bpo_index_funcs[768])(); // 0x0302AD70
const char* (*obj_class_instantiator::classname_funcs[768])(); // 0x03025AA0
int obj_class_instantiator::pool_usage[768]; // 0x0302A168
int obj_class_instantiator::max_objects[768]; // 0x03024E98
int obj_class_instantiator::class_index[768]; // 0x0301A240
int obj_class_instantiator::num_registered_classes; // 0x0302F120

// 0x00710CE0 (object.inl:53)
obj_class_instantiator::obj_class_instantiator(const char* name, object* (*af)(), void (*tf)(signed char&, signed char&), int (*bpf)(), const char* (*nf)(), int (*rf)(), int _pool_usage, int _max_objects)
{
    // The first registration clears the table (registrations run as dynamic initializers, in any order).
    if (!li_init)
    {
        for (int i = 0; i < hash_size; i++)
        {
            alloc_funcs[i] = nullptr;
        }
        num_registered_classes = 0;
        li_init = true;
    }
    this_hash = str_hash_string(name);
    const unsigned int bucket = this_hash % (unsigned int)hash_size;
    if (alloc_funcs[bucket] != nullptr)
    {
        char message[512];
        sprintf(message, "Fatal error : name collision in object alloc functions (%s)! Get DaveB!", name);
        keen::debug::nativePrintArgs(message);
    }
    type_funcs[bucket] = tf;
    bpo_index_funcs[bucket] = bpf;
    pool_usage[bucket] = _pool_usage;
    alloc_funcs[bucket] = af;
    max_objects[bucket] = _max_objects;
    class_index[num_registered_classes] = bucket;
    classname_funcs[bucket] = nf;
    num_registered_classes++;
}

// 0x00919D40 (object.inl:119)
int obj_class_instantiator::get_class_index(unsigned int str_hash)
{
    const unsigned int bucket = str_hash % (unsigned int)hash_size;
    return alloc_funcs[bucket] != nullptr ? (int)bucket : -1;
}

// 0x00919D60 (object.inl:134)
object* obj_class_instantiator::alloc_object(unsigned int str_hash)
{
    const unsigned int bucket = str_hash % (unsigned int)hash_size;
    if (alloc_funcs[bucket] == nullptr || (int)bucket < 0)
    {
        return nullptr;
    }
    object* objp = alloc_funcs[bucket]();
    if (objp == nullptr)
    {
        return nullptr;
    }
    type_funcs[bucket](objp->obj_type, objp->sub_type);
    return objp;
}

// 0x00919DB0 (object.inl:160)
int obj_class_instantiator::bpo_index(unsigned int str_hash)
{
    return bpo_index_funcs[get_class_index(str_hash)]();
}

// 0x00A05490 (object.inl:175)
object_type obj_class_instantiator::get_type(unsigned int str_hash)
{
    signed char type;
    signed char sub_type;
    type_funcs[get_class_index(str_hash)](type, sub_type);
    return (object_type)type;
}

// 0x00919DE0 (object.inl:197)
void obj_class_instantiator::get_pool_usage_by_class_index(int cindex, int& total_bytes, int& _max_objects)
{
    const int bucket = class_index[cindex];
    total_bytes = pool_usage[bucket];
    _max_objects = max_objects[bucket];
}

// 0x00919E10 (object.inl:218)
int obj_class_instantiator::get_class_index_by_types(object_type obj_type, object_sub_type sub_type)
{
    for (int i = 0; i < num_registered_classes; i++)
    {
        signed char type;
        signed char sub;
        type_funcs[class_index[i]](type, sub);
        if (type == obj_type && sub == sub_type)
        {
            return i;
        }
    }
    return -1;
}

// 0x00919E80 (object.inl:252)
int obj_class_instantiator::get_class_index_by_subtype(object_sub_type sub_type)
{
    for (int i = 0; i < num_registered_classes; i++)
    {
        signed char type;
        signed char sub;
        type_funcs[class_index[i]](type, sub);
        if (sub == sub_type)
        {
            return i;
        }
    }
    return -1;
}

// ---- property blocks of objects

// 0x00A38B00 (:212)
obj_prop_block::obj_prop_block(const vector& pos, const matrix& orient, const char* name)
{
    // The placement as one OBJ_PROP_COMBINED property, and the display name when there is one.
    object_properties props;
    props.pos = pos;
    props.orient = orient;
    obj_prop* prop = set_internal(OBJ_PROP_COMBINED, sizeof(object_properties), 5);
    if (prop != nullptr)
    {
        memcpy(prop->data, &props, sizeof(object_properties));
    }
    if (name != nullptr)
    {
        set(OBJ_PROP_DISPLAY_NAME, name);
    }
}

// 0x00A14100 (:231)
obj_prop_block::obj_prop_block()
{
}

// 0x005DBB40
obj_prop_block::~obj_prop_block()
{
}

// 0x0091D030
object_properties::object_properties()
{
}

// ---- attach info

// 0x00A26480
attach_info_data::attach_info_data()
{
}

// ---- object

// 0x00710C10 (.h:297)
object::object()
{
    checking_reset = 0;
    name_index = 0xFFFF;
}

// 0x00710C30 (.h:298)
object::~object()
{
}

// Queues the object for the render update task with the given render flags (inlined in several members). Cover nodes
// and navpoints are never rendered.
static void queue_render_update(object* objp, object_flags& flags, unsigned char render_flags)
{
    if (objp->get_obj_type() == OT_NAVPOINT || objp->get_obj_type() == OT_COVER_NODE)
    {
        return;
    }
    if (flags.render_flags == 0)
    {
        if (Object_update_render_list.full())
        {
            return;
        }
        Object_update_render_list.add(objp);
    }
    flags.render_flags |= render_flags;
}

// 0x00A14110 (:241)
void object::get_bbox(vector& bmin, vector& bmax) const
{
    bmin = last_known_bmin;
    bmax = last_known_bmax;
}

// 0x00A14150 (:247)
void object::get_local_bbox(vector& bmin, vector& bmax) const
{
    bmin.x = last_known_bmin.x - pos.x;
    bmin.y = last_known_bmin.y - pos.y;
    bmin.z = last_known_bmin.z - pos.z;
    bmax.x = last_known_bmax.x - pos.x;
    bmax.y = last_known_bmax.y - pos.y;
    bmax.z = last_known_bmax.z - pos.z;
}

// 0x00A14250 (:253)
const char* object::get_name() const
{
    return World.get_object_name(this);
}

// 0x00A14260 (:259)
void object::set_name(const char* _name)
{
    World.set_object_name(this, _name);
}

// 0x00A14280 (:267)
bool object::has_child()
{
    return child_ptr != nullptr;
}

// 0x00A14290 (:273)
void object::hierarchy_notify(hierarchy_notify_code code)
{
}

// 0x00AA2BB0 (:278)
bool object::set_child(object* child_objp)
{
    // Persistent objects never become children.
    if (child_objp == nullptr || child_objp->obj_flags.persistent)
    {
        return false;
    }
    if (child_ptr == nullptr)
    {
        child_objp->child_next = child_objp;
        child_objp->child_prev = child_objp;
        child_ptr = child_objp;
    }
    else
    {
        child_objp->child_prev = child_ptr->child_prev;
        child_objp->child_next = child_ptr;
        child_ptr->child_prev->child_next = child_objp;
        child_ptr->child_prev = child_objp;
    }
    child_objp->parent = handle;
    World.set_child(child_objp, this);
    if (child_objp->attach_info != nullptr)
    {
        Attach_info_pool.free(child_objp->attach_info);
        child_objp->attach_info = nullptr;
    }
    child_objp->set_is_visible(obj_flags._dont_use_me);
    return true;
}

// 0x00A8C1F0 (:375)
bool object::remove_child(object* child_objp)
{
    if (child_objp == nullptr || child_objp->parent != handle)
    {
        return false;
    }
    if (child_ptr == child_objp && child_objp->child_next == child_objp)
    {
        child_objp->child_next = nullptr;
        child_objp->child_prev = nullptr;
        child_ptr = nullptr;
    }
    else
    {
        if (child_ptr == child_objp)
        {
            child_ptr = child_objp->child_next;
        }
        child_objp->child_prev->child_next = child_objp->child_next;
        child_objp->child_next->child_prev = child_objp->child_prev;
        child_objp->child_next = nullptr;
        child_objp->child_prev = nullptr;
    }
    child_objp->parent = 0xFFFFFFFF;
    if (child_objp->attach_info != nullptr)
    {
        Attach_info_pool.free(child_objp->attach_info);
        child_objp->attach_info = nullptr;
    }
    World.set_child(child_objp, nullptr);
    return true;
}

// 0x00AA2C40 (:319)
// Attaches a child at a prop point of this object (tag index, -1: the origin) by a prop point of the child; with
// maintain_transform the child keeps its current placement relative to the attach point.
bool object::set_child(object* child_objp, int parent_prop_point, int child_prop_point, bool maintain_transform, bool update_physics)
{
    if (child_objp->parent != handle && !set_child(child_objp))
    {
        return false;
    }
    if (child_objp->attach_info == nullptr)
    {
        child_objp->attach_info = Attach_info_pool.alloc(0);
        if (child_objp->attach_info == nullptr)
        {
            return false;
        }
    }
    attach_info_data* ai = child_objp->attach_info;
    ai->parent_prop_point = parent_prop_point;
    ai->child_prop_point = child_prop_point;
    ai->update_physics = update_physics;
    ai->updated = 0;
    ai->parent_handle = handle;
    if (!maintain_transform)
    {
        ai->use_relative_transform = 0;
        return true;
    }
    ai->use_relative_transform = 1;
    vector parent_pos;
    matrix parent_orient;
    if (parent_prop_point < 0)
    {
        parent_orient = orient;
        parent_pos = pos;
    }
    else
    {
        get_tag_world_coords(parent_prop_point, parent_pos, &parent_orient, nullptr, nullptr);
    }
    vector child_pos;
    matrix child_orient;
    if (child_prop_point < 0)
    {
        child_pos = child_objp->pos;
        child_orient = child_objp->orient;
    }
    else
    {
        // Original: the child's prop point is looked up on this object (the parent), not on the child.
        get_tag_world_coords(child_prop_point, child_pos, &child_orient, nullptr, nullptr);
    }
    ai = child_objp->attach_info;
    ai->relative_transform.m_translation.x = child_pos.x - parent_pos.x;
    ai->relative_transform.m_translation.y = child_pos.y - parent_pos.y;
    ai->relative_transform.m_translation.z = child_pos.z - parent_pos.z;
    child_objp->attach_info->relative_transform.m_translation.rotate(parent_orient);
    const matrix rel = child_orient * parent_orient.copy_transpose();
    child_objp->attach_info->relative_transform.m_rotation = rel;
    return true;
}

// The placement of an attached child (attach_info) from its parent's: the parent's prop point (or origin), the
// relative transform, then back from the child's prop point. Each product and sum is rounded to float.
// Shared by adjust_child_transforms and adjust_transforms (inlined in both).
static void object_compute_attached_transform(const object* parent_objp, const object* child_objp, matrix43& m)
{
    const attach_info_data* ai = child_objp->attach_info;
    if (ai->parent_prop_point < 0)
    {
        m.set_orientation(parent_objp->orient);
        m.set_translation(parent_objp->pos);
    }
    else
    {
        parent_objp->get_tag_world_coords(ai->parent_prop_point, m.m_translation, &m.m_rotation, nullptr, nullptr);
    }
    ai = child_objp->attach_info;
    if (ai->use_relative_transform)
    {
        const vector& t = ai->relative_transform.m_translation;
        const matrix& r = m.m_rotation;
        m.m_translation.x = r.rvec.x * t.x + r.uvec.x * t.y + r.fvec.x * t.z + m.m_translation.x;
        m.m_translation.y = m.m_translation.y + (r.rvec.y * t.x + r.uvec.y * t.y + r.fvec.y * t.z);
        m.m_translation.z = m.m_translation.z + (r.rvec.z * t.x + r.uvec.z * t.y + r.fvec.z * t.z);
        m.m_rotation = ai->relative_transform.m_rotation * m.m_rotation;
    }
    if (child_objp->attach_info->child_prop_point > -1)
    {
        vector tag_pos;
        matrix tag_orient;
        child_objp->get_tag_local_coords(child_objp->attach_info->child_prop_point, tag_pos, &tag_orient);
        m.m_rotation = tag_orient.copy_transpose() * m.m_rotation;
        tag_pos.negate();
        tag_pos.unrotate(m.m_rotation);
        m.m_translation.x = m.m_translation.x + tag_pos.x;
        m.m_translation.y = tag_pos.y + m.m_translation.y;
        m.m_translation.z = tag_pos.z + m.m_translation.z;
    }
}

// 0x00A142A0 (:495)
// Moves the children with this object: move_all moves every child (recursively) to this object's placement;
// otherwise each attached child follows its attach point, or this object's placement when transforms are disabled
// (the child's dont_transform, a disabled parent, or either not streamed), which disables its own children too.
void object::adjust_child_transforms(bool parent_disabled, bool move_all)
{
    object* child = child_ptr;
    if (move_all)
    {
        if (child == nullptr)
        {
            return;
        }
        for (;;)
        {
            child->update_pos_and_orient(pos, orient, false);
            if (child->child_ptr != nullptr)
            {
                child->adjust_child_transforms(parent_disabled, move_all);
            }
            child = child->child_next;
            if (child == child_ptr || child == nullptr)
            {
                return;
            }
        }
    }
    do
    {
        if (child == nullptr)
        {
            return;
        }
        if (child->attach_info != nullptr)
        {
            child->attach_info->updated = 1;
            if (child->obj_flags.dont_transform || parent_disabled || !obj_flags.streamed || !child->obj_flags.streamed)
            {
                child->update_pos_and_orient(pos, orient, child->attach_info->update_physics);
                if (child->child_ptr != nullptr)
                {
                    child->adjust_child_transforms(true, false);
                }
            }
            else
            {
                matrix43 m;
                object_compute_attached_transform(this, child, m);
                child->update_pos_and_orient(m.m_translation, m.m_rotation, child->attach_info->update_physics);
                if (child->child_ptr != nullptr)
                {
                    child->adjust_child_transforms(false, false);
                }
            }
        }
        child = child->child_next;
    } while (child != child_ptr);
}

// 0x00A146E0 (:568)
// Places this attached object (and its streamed, attached ancestors) from the topmost streamed ancestor down,
// writing pos/orient directly. Gives up beyond 19 ancestors.
void object::adjust_transforms()
{
    // Magic number: room for this object and 19 ancestors.
    object* chain[20];
    chain[0] = this;
    int count = 1;
    if (attach_info == nullptr || !obj_flags.streamed)
    {
        return;
    }
    unsigned int h = parent;
    if (h != 0xFFFFFFFF)
    {
        int n = 1;
        for (;;)
        {
            // Original: a parent handle that no longer resolves is dereferenced (null).
            object* p = World.object_from_handle(h);
            count = n;
            if (!p->obj_flags.streamed)
            {
                break;
            }
            if (n > 19)
            {
                return;
            }
            chain[n] = p;
            count = n + 1;
            if (p->attach_info == nullptr)
            {
                break;
            }
            h = p->parent;
            if (h == 0xFFFFFFFF)
            {
                break;
            }
            n = count;
        }
    }
    for (int i = count - 2; i > -1; i--)
    {
        object* child = chain[i];
        matrix43 m;
        object_compute_attached_transform(chain[i + 1], child, m);
        child->pos = m.m_translation;
        child->orient = m.m_rotation;
    }
}

// 0x00A8C270 (:402)
bool object::remove_from_parent()
{
    object* parent_objp = World.object_from_handle(parent);
    if (parent_objp == nullptr)
    {
        return false;
    }
    return parent_objp->remove_child(this);
}

// 0x00A5FC10 (:418)
void object::collect_children(base_array<object*>& obj_list, bool (*cb_func)(object*, unsigned int, const void*), void* cb_data) const
{
    if (obj_list.full() || child_ptr == nullptr)
    {
        return;
    }
    object* child = child_ptr;
    do
    {
        if (cb_func == nullptr || cb_func(child, 0xFFFFFFFF, cb_data))
        {
            obj_list.add(child);
            if (obj_list.full())
            {
                return;
            }
        }
        child = child->child_next;
    } while (child != child_ptr && child != nullptr);
}

// 0x00A14AE0 (:630)
float object::get_mass() const
{
    if (havok_handle != 0xFFFFFFFF)
    {
        return havok_body_get_mass(havok_handle);
    }
    return 0.0f;
}

// 0x00A14B00 (:645)
unsigned char object::get_physical_material(unsigned int shape_key, const vector* contact_point, const hkpWorldObject* hk_objp) const
{
    if (hk_objp == nullptr)
    {
        hk_objp = (const hkpWorldObject*)havok_body_get_pointer(havok_handle);
        if (hk_objp == nullptr)
        {
            return 0xFF;
        }
    }
    return (unsigned char)(unsigned int)havok_get_property(hk_objp, 3, (void*)(unsigned int)Rfg_physical_material_default_index, true);
}

// 0x00A14B40 (:664)
unsigned char object::get_effect_material(unsigned int shape_key, const vector* contact_point, const hkpWorldObject* hk_objp) const
{
    if (hk_objp == nullptr)
    {
        hk_objp = (const hkpWorldObject*)havok_body_get_pointer(havok_handle);
        if (hk_objp == nullptr)
        {
            return 0xFF;
        }
    }
    return (unsigned char)(unsigned int)havok_get_property(hk_objp, 4, (void*)(unsigned int)Rfg_effect_material_default_index, true);
}

// 0x00A14B80 (:681)
float object::get_energy_scale()
{
    return RFG_object_energy_scale;
}

// Destroys the constraints this object hosts or is the child of (remove_constraints and destroy_notify).
static void destroy_constraints(constraint* child_list, constraint* host_list)
{
    for (constraint* cp = child_list; cp != nullptr;)
    {
        World.destroy_object(cp, false);
        cp = cp->child_constraint_next;
        if (cp == child_list)
        {
            break;
        }
    }
    for (constraint* cp = host_list; cp != nullptr;)
    {
        World.destroy_object(cp, false);
        cp = cp->host_constraint_next;
        if (cp == host_list)
        {
            break;
        }
    }
}

// 0x00A14B90 (:687)
void object::remove_constraints()
{
    destroy_constraints(child_constraint_ptr, host_constraint_ptr);
}

// 0x00A14BF0 (:702)
void object::destroy_on_stream()
{
    obj_flags.no_save = 1;
    obj_flags.destroy_on_stream = 1;
    if (!obj_flags.streamed)
    {
        World.destroy_object(this, false);
    }
}

// 0x00705380 (.h:384)
void object::destroy_on_next_stream()
{
    obj_flags.no_save = 1;
    obj_flags.destroy_on_stream = 1;
}

// 0x00A14C10 (:718)
bool object::request_streaming_resource(unsigned int _srid, stream2_priority priority)
{
    if (srid == 0 && _srid != 0)
    {
        srid = _srid;
        World.stream_request(this, _srid, priority);
        return true;
    }
    return false;
}

// 0x00A14C50 (:741)
void object::reset()
{
    memset(&obj_flags, 0, sizeof(obj_flags));
    memset(&remote_obj_flags, 0, sizeof(remote_obj_flags));
    havok_handle = 0xFFFFFFFF;
    contact_info.m_contact_list = nullptr;
    child_ptr = nullptr;
    child_prev = nullptr;
    child_next = nullptr;
    child_constraint_ptr = nullptr;
    host_constraint_ptr = nullptr;
    attach_info = nullptr;
    flagged_next = nullptr;
    flagged_prev = nullptr;
    srid = 0;
    bp_handle = nullptr;
}

// 0x00A14C90 (:781)
bool object::is_killed() const
{
    return false;
}

// 0x00AA2EA0 (:795)
bool object::serialize(obj_prop_block& pblock, bool load)
{
    if (load)
    {
        if (pblock.get_optional(OBJ_PROP_JUST_POS, pos))
        {
            orient = IDENTITY_MATRIX;
        }
        else
        {
            object_properties props; // left uninitialized when the property is missing (see the file comment)
            pblock.get_optional(OBJ_PROP_COMBINED, props);
            pos = props.pos;
            orient = props.orient;
        }
        char name[512];
        if (pblock.get_optional(OBJ_PROP_DISPLAY_NAME, &name[0]))
        {
            World.set_object_name(this, name);
        }
        obj_flags._dont_use_me = 1;
        mp_dcmo_index = 0xFF;
        queue_render_update(this, obj_flags, 1);
        return true;
    }
    if (orient.fvec.x == FORWARD_VECTOR.x && orient.fvec.y == FORWARD_VECTOR.y && orient.fvec.z == FORWARD_VECTOR.z && orient.rvec.x == RIGHT_VECTOR.x && orient.rvec.y == RIGHT_VECTOR.y && orient.rvec.z == RIGHT_VECTOR.z)
    {
        pblock.set(OBJ_PROP_JUST_POS, pos);
    }
    else
    {
        object_properties props;
        props.pos = pos;
        props.orient = orient;
        pblock.set(OBJ_PROP_COMBINED, props);
    }
    const char* name = World.get_object_name(this);
    if (name != nullptr && *name != '\0')
    {
        pblock.set(OBJ_PROP_DISPLAY_NAME, name);
    }
    return true;
}

// 0x00A14CA0 (:846)
obj_serialize_mode object::serialize_mode()
{
    return OSM_RESTORE;
}

// 0x00A14CB0 (:858)
bool object::stream(bool load)
{
    if (!load && obj_flags.destroy_on_stream)
    {
        World.destroy_object(this, false);
    }
    return true;
}

// 0x00A14CD0 (:880)
bool object::serialize_restore_if_missing_in_save_file(obj_prop_block& pblock)
{
    return false;
}

// 0x00A14CE0 (:887)
bool object::serialize_restore_if_resetting_destruction(obj_prop_block& pblock)
{
    return false;
}

// 0x00A8C2A0 (:905)
void object::free_this()
{
    // Unlinks the object from its parent and its children.
    if (handle == 0xFFFFFFFF)
    {
        return;
    }
    havok_collision_events_object_freed(this);
    if (parent != 0xFFFFFFFF)
    {
        object* parent_objp = World.object_from_handle(parent);
        if (parent_objp != nullptr)
        {
            parent_objp->remove_child(this);
        }
    }
    for (object* child = child_ptr; child != nullptr;)
    {
        object* next = child->child_next != child_ptr ? child->child_next : nullptr;
        object* child_parent = World.object_from_handle(child->parent);
        if (child_parent != nullptr)
        {
            child_parent->remove_child(child);
        }
        child = next;
    }
}

// 0x00A14CF0 (:937)
void object::destroy_notify()
{
    destroy_constraints(child_constraint_ptr, host_constraint_ptr);
}

// 0x00A8C310 (:955)
void object::update_pos_and_orient(const vector& update_pos, const matrix& update_orient, bool set_havok_data)
{
    pos = update_pos;
    orient = update_orient;
    if (bp_handle != nullptr)
    {
        get_bbox(last_known_bmin, last_known_bmax);
        havok_bpo_update(bp_handle, last_known_bmin, last_known_bmax);
    }
    if (set_havok_data && havok_handle != 0xFFFFFFFF)
    {
        havok_body_set_pos_orient(havok_handle, update_pos, update_orient);
    }
    queue_render_update(this, obj_flags, 4);
}

// 0x00A14D00 (:974)
void object::update_streaming_box()
{
    if (bp_handle != nullptr)
    {
        get_bbox(last_known_bmin, last_known_bmax);
        havok_bpo_update(bp_handle, last_known_bmin, last_known_bmax);
    }
}

// 0x00A14D40 (:988)
int object::get_tag_index_from_name(const char* tag_name) const
{
    return -1;
}

// 0x00A14D50 (:994)
bool object::get_tag_world_coords(int tag_index, vector& tag_pos, matrix* tag_orient, vector* obj_pos, matrix* obj_orient) const
{
    // No tags: the object's own placement (tag_orient is written although it is optional).
    tag_pos = pos;
    *tag_orient = orient;
    return false;
}

// 0x00A14DA0 (:1005)
bool object::get_tag_local_coords(int tag_index, vector& tag_pos, matrix* tag_orient) const
{
    tag_pos.zero();
    tag_orient->make_identity();
    return false;
}

// 0x00710C40 (.h:344)
const rl_renderable_mesh* object::get_static_mesh() const
{
    return nullptr;
}

// 0x00A8C3E0 (:1015)
void object::set_is_visible(bool is_visible)
{
    obj_flags._dont_use_me = is_visible;
    queue_render_update(this, obj_flags, 1);
}

// 0x006C3970 (.h:348)
bool object::get_is_visible() const
{
    return obj_flags._dont_use_me;
}

// 0x00A5FCA0 (:1021)
void object::set_render_flags(int flags)
{
    queue_render_update(this, obj_flags, (unsigned char)(flags & 0xF));
}

// 0x00A14DC0 (:1044)
void object::update_render(rl_renderer*)
{
}

// 0x00710C50 (.h:352)
bool object::enable_object_outline(int layer)
{
    return false;
}

// 0x00710C60 (.h:353)
bool object::disable_object_outline()
{
    return false;
}

// 0x00710C70 (.h:357)
int object::attach_index(const vector& _pos, unsigned int parent_objp_body_handle)
{
    return -1;
}

// 0x00710C80 (.h:358)
bool object::attach_index_is_valid(int index)
{
    return true;
}

// 0x00710C90 (.h:371)
void object::process_bullet_hit(human* attacker, float damage, const weapon_fire_info& fire_info)
{
}

// 0x00710CA0 (.h:374)
bool object::process_melee_hit(object* attacker, float damage, const vector& impulse, const vector& hit_pos, const vector& hit_normal, unsigned int shape_key, const weapon_info& w_info)
{
    return true;
}

// 0x00710CB0 (.h:424)
bool object::ready_to_stream_out()
{
    return true;
}

// 0x005DB950 (.h:432)
unsigned int object::get_handle() const
{
    return handle;
}

// 0x005DB960 (.h:433)
unsigned int object::get_parent() const
{
    return parent;
}

// 0x005DB970 (.h:435)
object_type object::get_obj_type() const
{
    return (object_type)obj_type;
}

// 0x005DB980 (.h:436)
object_sub_type object::get_sub_type() const
{
    return (object_sub_type)sub_type;
}

// 0x005DB990 (.h:438)
bool object::streamed() const
{
    return obj_flags.streamed;
}

// 0x00A05480 (.h:439)
unsigned int object::get_srid() const
{
    return srid;
}

// 0x00A14DD0 (:1048)
void object::set_persistent(bool set)
{
    // An object can only become persistent under a persistent parent, and stops only when no child is persistent.
    if (set)
    {
        if (parent != 0xFFFFFFFF)
        {
            object* parent_objp = World.object_from_handle(parent);
            if (parent_objp == nullptr || !parent_objp->obj_flags.persistent)
            {
                return;
            }
        }
    }
    else
    {
        for (object* child = child_ptr; child != nullptr;)
        {
            if (child->obj_flags.persistent)
            {
                return;
            }
            child = child->child_next;
            if (child == child_ptr)
            {
                break;
            }
        }
    }
    obj_flags.persistent = set;
}

// 0x00919D10 (.h:442)
bool object::is_persistent() const
{
    return obj_flags.persistent;
}

// 0x00919D20 (.h:443)
bool object::is_original() const
{
    return obj_flags.original;
}

// 0x00A14E30 (:1070)
void object::set_streaming_dependent(bool streaming_dependent)
{
    obj_flags.is_dependent = streaming_dependent && obj_flags.bpo_index == 0;
    bp_handle = havok_bpo_update_flags(bp_handle, World.calc_bpo_flags(this));
}

// 0x00A14E80 (:1116)
unsigned int object::get_alt_hk_body_handle(unsigned int index) const
{
    if (index != 0 || havok_handle == 0xFFFFFFFF)
    {
        return 0xFFFFFFFF;
    }
    return havok_body_get_pointer(havok_handle) != nullptr ? havok_handle : 0xFFFFFFFF;
}

// 0x00A14EB0 (:1137)
unsigned int object::get_alt_hk_body_handle_index(unsigned int alt_handle) const
{
    if (alt_handle != 0xFFFFFFFF && havok_body_get_pointer(alt_handle) != nullptr)
    {
        return alt_handle != havok_handle ? 0xFFFFFFFF : 0;
    }
    return 0xFFFFFFFF;
}

// 0x00919D30 (.h:482)
havok_bpo* object::get_bpo_handle()
{
    return bp_handle;
}

// 0x00A2C4D0 (.h:585)
int object::get_lod_by_distance(float distance, const lod_info* lod_infos, unsigned int num_lods)
{
    // The first lod whose distance is not below distance, clamped to the last one.
    unsigned int lod = 0;
    while (lod < num_lods && lod_infos[lod].dist < distance)
    {
        lod++;
    }
    if (num_lods - 1 <= lod)
    {
        lod = num_lods - 1;
    }
    return (int)lod;
}

// ---- free functions

// 0x00A38BE0 (:1165)
// The queued objects that still exist update their render instances; their render flags are cleared and the queue
// emptied.
static void object_update_render_all(void*, rl_renderer* pRenderer)
{
    for (int i = 0; i < Object_update_render_list.size(); i++)
    {
        object* objp = Object_update_render_list[i];
        if (objp != nullptr)
        {
            if (!objp->obj_flags.destroyed && objp->get_handle() != 0xFFFFFFFF)
            {
                objp->update_render(pRenderer);
            }
            objp->obj_flags.render_flags = 0;
        }
    }
    Object_update_render_list.remove_all();
}

// 0x00AB9DE0 (:1194)
void object_system_init()
{
    human_system_init();
    Object_update_render_task.m_callback = object_update_render_all;
    game_render_update_render_task_register(&Object_update_render_task);
}

// 0x00A38C40 (:1208)
void object_level_init_pre()
{
    Player_died = false;
    Mover_ref_points_inited = false;
    safehouse_level_init_pre(); // inlined in rfg.exe
}

// 0x00AE05C0 (:1220)
// The humans (the player at the player start), then the movers of the load (their bodies, the first frame's
// transforms, their level init), the general movers' destroyed count and network objects (multiplayer), the movers'
// starting points and the public address kiosks.
void object_level_init_post()
{
    human_level_init_post();
    Charge_list = nullptr;
    havok_add_entity_flush();
    rfg_mover_do_frame_pre_sim();
    object_mover_do_frame_post_sim();
    rfg_mover_level_init_post();
    multi_game_general_mover_reset_destroyed_count();
    if (Multiplayer)
    {
        create_level_general_mover_remote_objects();
    }
    object_movers_record_starting_points();
    public_address_kiosk_level_init_post();
}

// 0x00AB3A50 (:1241)
// object_item_level_shutdown (the items and their high-res textures) is inlined in rfg.exe.
void object_level_shutdown()
{
    projectile_destroy_all();
    player_level_shutdown_all();
    npc_free_all();
    object_item_level_shutdown();
    turret_level_shutdown();
    object_mover_level_shutdown();
    object_effect_level_shutdown();
    glass_level_shutdown();
    object_debris_level_shutdown(); // object_debris_free_all (a file static of a later unity file; this is the call)
    object_skybox_level_shutdown(); // object_skybox_free_all, likewise (object_skybox.cpp is in the render library)
    Object_update_render_list.remove_all();
}

// 0x00A5FCF0 (:1286)
// After the simulation every attached child follows its attach point again: the attach infos are marked not updated,
// then the root parent of each one still not updated moves its tree (adjust_child_transforms marks the ones it moved).
void object_do_frame_postsim()
{
    if (Attach_info_pool.get_first(0) == nullptr)
    {
        return;
    }
    for (attach_info_data* ai = Attach_info_pool.get_first(0); ai != nullptr; ai = Attach_info_pool.get_next(ai))
    {
        ai->updated = 0;
    }
    for (attach_info_data* ai = Attach_info_pool.get_first(0); ai != nullptr; ai = Attach_info_pool.get_next(ai))
    {
        if (ai->updated)
        {
            continue;
        }
        // Original: the parent is not null-checked.
        object* root = World.object_from_handle(ai->parent_handle);
        while (root->get_parent() != 0xFFFFFFFF)
        {
            root = World.object_from_handle(root->get_parent());
        }
        root->adjust_child_transforms(false, false);
    }
}

// 0x00A8C440 (:1316)
void object_set_visibility_plus_children(object* objp, bool render)
{
    objp->set_is_visible(render);
    for (object* child = objp->child_ptr; child != nullptr;)
    {
        object_set_visibility_plus_children(child, render);
        child = child->child_next;
        if (child == objp->child_ptr)
        {
            break;
        }
    }
}

// 0x00A14EF0 (:1327)
bool object_is_player(unsigned int handle)
{
    object* objp = World.object_from_handle(handle);
    return objp != nullptr && objp->get_sub_type() == OT_SUB_HUMAN_PLAYER;
}

// 0x00A14F20 (:1333)
bool object_is_player(const object* objp)
{
    return objp != nullptr && objp->get_sub_type() == OT_SUB_HUMAN_PLAYER;
}

// 0x00A14F40 (:1338)
bool object_is_local_player(unsigned int handle)
{
    if (Player == nullptr)
    {
        return false;
    }
    return handle == Player->get_handle();
}

// 0x00A14F60 (:1348)
bool object_is_local_player(const object* objp)
{
    return objp != nullptr && objp == Player;
}

// 0x00A14F80 (:1353)
object_type object_type_by_hash(unsigned int hash)
{
    signed char type;
    signed char sub_type;
    obj_class_instantiator::type_funcs[obj_class_instantiator::get_class_index(hash)](type, sub_type);
    return (object_type)type;
}
