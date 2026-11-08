// Original: volition/rfg/code/objects/movers/general_mover.cpp
//
// general_mover, a prop: either a general object of a chunk (a mesh with a collision model, static until destroyed or
// dislodged) or a piece broken off a destroyable shape (movable, rendered as a sub instance split off the rfg_mover's
// model). Decompiled; header comment in general_mover.h. Compiled in rfg_objects_unity.cpp.
//
// gm_flags, from the decompiled uses: 0x1 (cleared on free), 0x2 explode on death (pending), 0x4 play the death
// effect / swap to the corpse model (pending), 0x8 make movable (dislodged), 0x10 prefix effect running, 0x20 streamed
// out, 0x80 the piece is a shard, 0x100 killed, 0x200 corpse mesh pending, 0x400 delete, 0x800 death explosion done,
// 0x1000 render instance built from the shape base (not split off a donor).
//
// Partial: the mover fader set up by setup_nano_dissolve is not recovered (it only exists for rendering):
// setup_nano_dissolve creates no fader.
//
// Notes (decompiled behaviour, kept as is):
// - create_deferred_ri (also inlined in general_mover_pre_stream_process_all) reads the instance of the resource data
//   without checking that it is loaded.
// - general_mover::apply_damage with a pending prefix effect (gm_flags 0x10) only restarts the death timer: the
//   destroyed event, activity death and control drop are skipped for that hit.
// - get_tag_local_coords returns false without touching the outputs when the chunk is missing.

#include "volition/rfg/code/objects/movers/general_mover.h"

#include "volition/rfg/code/code_effects/explosion.h"
#include "volition/rfg/code/code_effects/material_effects.h"
#include "volition/rfg/code/destruction/rfg_collisions.h"
#include "volition/rfg/code/destruction/rfg_dynamics.h"
#include "volition/rfg/code/destruction/rfg_load_balance.h"
#include "volition/rfg/code/destruction/rfg_materials.h"
#include "volition/rfg/code/destruction/rfg_mover.h"
#include "volition/rfg/code/destruction/rfg_shapes.h"
#include "volition/rfg/code/effects/effects.h"
#include "volition/rfg/code/effects/object_effect.h"
#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/rfg/code/game_render/game_render_task.h"
#include "volition/rfg/code/gameplay/activities/activity_register_damage.h"
#include "volition/rfg/code/gameplay/wrecking_crew/wrecking_crew.h"
#include "volition/rfg/code/gamewide/game_render.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/havok/havok_bp.h"
#include "volition/rfg/code/havok/havok_convert.h"
#include "volition/rfg/code/interface/ui_document_hud_missions.h"
#include "volition/rfg/code/level/rfg_chunk_shared.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/level/world.inl"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_objects.h"
#include "volition/rfg/code/multi/multi_game/multi_game_object_movers.h"
#include "volition/rfg/code/multi/multi_game/multi_game_weapons.h"
#include "volition/rfg/code/objects/gameplay_properties.h"
#include "volition/rfg/code/objects/level_object_properties.h"
#include "volition/rfg/code/objects/lod_properties.h"
#include "volition/rfg/code/objects/movers/mover_fader.h"
#include "volition/rfg/code/objects/object_prop_names_shared.h"
#include "volition/rfg/code/pathfinding/nav_cell_detour.h"
#include "volition/rfg/code/scripting/action_script/global_event_data.h"
#include "volition/rfg/code/scripting/action_script/global_event_manager.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/render_lib/code/rl_aabb.h"
#include "volition/render_lib/code/rl_compound_mesh_instance.h"
#include "volition/render_lib/code/rl_destroyable_model_instance.h"
#include "volition/render_lib/code/rl_mesh_instance.h"
#include "volition/render_lib/code/rl_renderable_instance.h"
#include "volition/vlib/code/math/bbox.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/memory/object_pool.h"
#include "volition/vlib/code/util/color.h"
#include "volition/vlib/code/util/string_comp.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/matrix/hktransformf.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/geometry/aabb/hkaabb.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/convextransform/hkpconvextransformshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/misc/transform/hkptransformshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkprigidbody.h"

static const int GENERAL_MOVER_POOL_SIZE = 3072; // general_moverpool
static const int GM_DESTROYABLE_HITPOINTS = 5000; // pieces of destroyable shapes start with these
static const int GM_DESTROYABLE_DISLODGE_HITPOINTS = 50;
static const float GM_FADE_DIST_SQR = 1e+08f; // 10 km: movers fade out of view beyond this
static const unsigned int GM_PROPERTY_MATERIAL_FLAGS = 8; // havok_set_property key: material flags of a piece
static const int NUM_PREFIX_EFFECTS = 2; // level_object_base_properties::m_prefix_effect_infos

// ---- file-static globals
static float Mover_jitter_max_angle = 5.0f; // 0x01694528, degrees
static bool Jitter_movers_from_destruction = true; // 0x0169452C
static general_mover* Pre_stream_mover_list_gen_mover; // 0x0302F10C, movers waiting for their render instance
static float Mover_jitter_max_translation; // 0x0302F110
static game_render_task Pre_stream_render_task_gen_mover; // 0x030FCE48, also registered by object_mover_system_init
static object_pool<general_mover, GENERAL_MOVER_POOL_SIZE, 1> general_moverpool; // 0x031A3A30
static obj_class_instantiator general_mover_instantiator("general_mover", alloc_general_mover, types_general_mover, bpo_index_general_mover, classname_general_mover, general_mover::pool_remaining, 0xE7C40, GENERAL_MOVER_POOL_SIZE); // 0x031A018C (0x01075D40)

// ---- general_mover (header inlines)
// 0x006E17A0 (.h:44)
const object_type general_mover::get_type_static()
{
    return OT_MOVER;
}

// 0x006E17B0 (.h:44)
const object_sub_type general_mover::get_subtype_static()
{
    return OT_SUB_MOVER_GENERAL;
}

// 0x00A05DE0 (.h:44)
const char* general_mover::get_classname_static()
{
    return "general_mover";
}

// 0x00756B90 (.h:44)
int general_mover::get_bpo_index_static()
{
    return 0;
}

// 0x00A37EE0 (.h:44)
const char* general_mover::get_classname() const
{
    return "general_mover";
}

// 0x00756BA0 (.h:155)
unsigned int general_mover::get_original_object() const
{
    return original_object;
}

// 0x00A37EF0
general_mover::~general_mover()
{
}

// ---- internal functions
// 0x00A12FF0 (:105)
// LTCG: gm_p in EAX. Appends the mover to the circular pre-stream list (once).
static void general_mover_add_to_pre_stream_list(general_mover* gm_p)
{
    if (gm_p->pre_stream_next != nullptr && gm_p->pre_stream_prev != nullptr)
    {
        return;
    }
    general_mover* head = Pre_stream_mover_list_gen_mover;
    if (head == nullptr)
    {
        gm_p->pre_stream_next = gm_p;
        gm_p->pre_stream_prev = gm_p;
        Pre_stream_mover_list_gen_mover = gm_p;
        return;
    }
    gm_p->pre_stream_prev = head->pre_stream_prev;
    gm_p->pre_stream_next = head;
    head->pre_stream_prev->pre_stream_next = gm_p;
    head->pre_stream_prev = gm_p;
}

// 0x00A37BB0 (:114)
// LTCG: objp in ECX. The human behind an attack: the attacker itself or a vehicle's most recent driver.
static unsigned int resolve_attacker(const object* objp)
{
    if (objp != nullptr)
    {
        if (objp->get_obj_type() == OT_HUMAN)
        {
            return objp->get_handle();
        }
        if (objp->get_obj_type() == OT_VEHICLE)
        {
            return ((const vehicle*)objp)->most_recent_driver;
        }
    }
    return 0xFFFFFFFF;
}

// ---- anchor_collector
// 0x00A13050 (:134)
anchor_collector::anchor_collector()
{
    m_earlyOut.m_bool = 0;
    has_hit = false;
}

// 0x00A13070
anchor_collector::~anchor_collector()
{
}

// 0x00A37BD0 (:142)
// A static general mover overlapping something is anchored; overlapping a piece of an rfg_mover links it to that
// piece dynamically (the mover then moves with it) instead.
void anchor_collector::addCdBodyPair(const hkpCdBody& bodyA, const hkpCdBody& bodyB)
{
    const hkpCdBody* root_a = &bodyA;
    while (root_a->m_parent != nullptr)
    {
        root_a = root_a->m_parent;
    }
    object* obj_a = object_from_world_obj((hkpWorldObject*)((const hkpCollidable*)root_a)->getOwner()); // the root body is a collidable
    const hkpCdBody* root_b = &bodyB;
    while (root_b->m_parent != nullptr)
    {
        root_b = root_b->m_parent;
    }
    object* obj_b = object_from_world_obj((hkpWorldObject*)((const hkpCollidable*)root_b)->getOwner());
    general_mover* gm = World.object_cast<general_mover>(obj_a);
    rfg_mover* rm = World.object_cast<rfg_mover>(obj_b);
    unsigned int shape_key = bodyB.m_shapeKey;
    if (gm == nullptr)
    {
        rm = World.object_cast<rfg_mover>(obj_a);
        gm = World.object_cast<general_mover>(obj_b);
        shape_key = bodyA.m_shapeKey;
        if (gm == nullptr)
        {
            return;
        }
    }
    if ((gm->mover_flags & 0xC) != 0)
    {
        return;
    }
    if (rm == nullptr || shape_key == 0xFFFFFFFF)
    {
        m_earlyOut.m_bool = 1;
        has_hit = true;
        return;
    }
    if (rfg_mover_create_dynamic_link(rm, (unsigned short)shape_key, gm, 0xFFFF))
    {
        gm->mover_flags |= 0x8000000;
        gm->move_type &= ~0x8;
        has_hit = true;
    }
}

// 0x00AA1350 (:183)
// LTCG: subpiece_index in ESI. Splits the piece's sub instance off the donor rfg_mover's instance into a new instance
// (the dlod's whole-mesh sub instance gives way to its pieces). A fading piece hands its current transform over and
// its fader is destroyed. The sub instance is placed at the piece's transform relative to its base pose, or jittered
// slightly (Jitter_movers_from_destruction) unless the mover asks not to be (mover_flags 0x2). Returns nullptr when
// the split fails; the donor then gets the sub instance back.
static rl_destroyable_model_instance* build_new_rl_instance_from_shape(rfg_destroyable_shape* shape, int subpiece_index, rl_destroyable_model_instance* donor_ri, unsigned int donor_handle, general_mover* gm)
{
    mempool_base* pool = Vlib_scratch_pool;
    mempool_mark_scope scope(pool);
    rfg_base_destroyable_shape* base = shape->parent;
    const int dlod_index = base->base_objects.to_pointer()[subpiece_index].dlod_key & 0xFFFF;
    rfg_dlod* dlod = shape->get_dlod(dlod_index);
    int split_index = (unsigned short)subpiece_index; // a short, stored in an int slot
    rl_split_info split;
    split.m_num_sub_insts_to_split = 1;
    split.m_sub_insts_to_split = (short*)&split_index;
    split.m_sub_insts_to_enable = (short*)pool->alloc(dlod->max_pieces * 2, 0);
    split.m_num_sub_insts_to_enable = 0;
    split.m_sub_insts_to_disable = (short*)pool->alloc(2, 0);
    split.m_num_sub_insts_to_disable = 0;
    rfg_subpiece* sp = &shape->get_object_buffer()[subpiece_index];
    const rfg_subpiece_base* sp_base = &base->base_objects.to_pointer()[subpiece_index];
    vector pos = ZERO_VECTOR;
    matrix orient(IDENTITY_MATRIX);
    // the sub instance info doubles as the update (the original reuses the stack slot)
    rl_sub_instance_update upd;
    if ((sp->flags & 0x10000) != 0)
    {
        // fading: take the fader's transform over
        if ((sp->flags & 0x8000) != 0)
        {
            mover_fader_get_current_transform(donor_handle, subpiece_index, orient, pos);
            pos -= sp_base->pos;
            matrix local;
            base->get_subpiece_orient_local(sp_base, local);
            orient = orient * local.copy_transpose();
        }
        mover_fader_destroy(donor_handle, subpiece_index);
        sp->flags &= ~0x30000u;
        dlod->flags |= 0x10;
    }
    else if ((sp->flags & 0x8000) != 0)
    {
        upd.m_info.m_disable_deferred = 0;
        donor_ri->get_sub_instance_info(upd.m_info, subpiece_index);
        pos = upd.m_info.m_transform.m_translation - sp_base->pos;
        matrix local;
        base->get_subpiece_orient_local(sp_base, local);
        orient = upd.m_info.m_transform.m_rotation * local.copy_transpose();
    }
    sp->flags &= ~0x20000u;
    if ((dlod->flags & 0x20) != 0)
    {
        // the dlod showed its whole mesh: back to the pieces
        split.m_sub_insts_to_disable[split.m_num_sub_insts_to_disable++] = (short)(base->num_objects + dlod_index);
        dlod->get_subpiece_indices(split.m_num_sub_insts_to_enable, split.m_sub_insts_to_enable);
        dlod->flags &= ~0x20;
    }
    rl_destroyable_model_instance* new_ri = donor_ri->split(split);
    if ((dlod->flags & 2) != 0)
    {
        short dlod_sub_inst = (short)(base->num_objects + dlod_index);
        donor_ri->unclaim_sub_instances(&dlod_sub_inst, 1);
        dlod->flags &= ~2;
    }
    if (new_ri == nullptr)
    {
        donor_ri->unclaim_sub_instances((short*)&split_index, 1);
        return nullptr;
    }
    new_ri->set_material_map(material_effects_get_original_material_map(donor_ri));
    upd.m_info.m_disable_deferred = 0;
    upd.m_info.m_is_active = 1;
    upd.m_base_index = subpiece_index;
    upd.m_info.m_submesh_id = base->base_extra_data.to_pointer()[subpiece_index].render_subpiece;
    upd.m_info.m_transform = IDENTITY_MATRIX43;
    if ((sp->flags & 0x8000) != 0)
    {
        upd.m_info.m_transform.m_rotation = orient;
        upd.m_info.m_transform.m_translation = pos;
    }
    else if (Jitter_movers_from_destruction && (gm->mover_flags & 2) == 0)
    {
        upd.m_info.m_transform.m_translation.x = fl_rand_range(-1.0f, 1.0f) * Mover_jitter_max_translation;
        upd.m_info.m_transform.m_translation.y = fl_rand_range(-1.0f, 1.0f) * Mover_jitter_max_translation;
        upd.m_info.m_transform.m_translation.z = fl_rand_range(-1.0f, 1.0f) * Mover_jitter_max_translation;
        vector axis;
        axis.rand_sphere();
        // degrees to radians: the double constant 0.017453 (0x012BFA28, a float literal widened), x87 in double
        const float angle = (float)((double)fl_rand() * Mover_jitter_max_angle * (double)0.017453f);
        upd.m_info.m_transform.m_rotation.set(axis, angle);
    }
    new_ri->update_instances(&upd, 1);
    new_ri->calculate_local_aabb();
    return new_ri;
}

// 0x00A5E6F0 (:317)
// LTCG: subpiece_index in EDI. A destroyable model instance showing one piece of the shape's base.
static rl_destroyable_model_instance* build_new_rl_instance_from_shape_base(rfg_base_destroyable_shape* shape_base, c_chunk_base* base_chunk, int subpiece_index)
{
    rl_destroyable_model_instance* ri = rl_destroyable_model_instance::create();
    if (ri == nullptr)
    {
        return nullptr;
    }
    rl_sub_instance_info info;
    info.m_is_active = 1;
    info.m_disable_deferred = 0;
    info.m_submesh_id = shape_base->base_extra_data.to_pointer()[subpiece_index].render_subpiece;
    info.m_transform = IDENTITY_MATRIX43;
    if (!ri->init(base_chunk->mesh_p.to_pointer(), base_chunk->material_map_p.to_pointer(), nullptr, &info, 1, nullptr, 0))
    {
        ri->release();
        return nullptr;
    }
    ri->calculate_local_aabb();
    return ri;
}

// 0x00A5E7D0 (:353)
// LTCG: base_chunk in EAX. A mesh instance of the object's sub mesh.
static rl_mesh_instance* build_new_rl_instance_from_object_base(general_object_base* obj_base, c_chunk_base* base_chunk)
{
    rl_mesh_instance* mi = rl_mesh_instance::create();
    if (mi == nullptr)
    {
        return nullptr;
    }
    if (!mi->init(base_chunk->mesh_p.to_pointer(), base_chunk->material_map_p.to_pointer(), obj_base->mesh_subpieces[0]))
    {
        mi->release();
        return nullptr;
    }
    mi->calculate_local_aabb();
    return mi;
}

// 0x00AB35F0 (:373)
// Pre-stream render task: builds the deferred render instances of the movers whose render was created and empties
// the list.
static void general_mover_pre_stream_process_all(void*, rl_renderer*)
{
    general_mover* gm = Pre_stream_mover_list_gen_mover;
    while (gm != nullptr)
    {
        general_mover* next = gm->pre_stream_next != Pre_stream_mover_list_gen_mover ? gm->pre_stream_next : nullptr;
        if ((gm->mover_flags & 0x80) != 0)
        {
            gm->create_deferred_ri(); // inlined
            gm->mover_flags &= ~0x80u;
        }
        if (Pre_stream_mover_list_gen_mover == gm && (Pre_stream_mover_list_gen_mover = gm->pre_stream_next) == gm)
        {
            gm->pre_stream_prev = nullptr;
            gm->pre_stream_next = nullptr;
            Pre_stream_mover_list_gen_mover = nullptr;
        }
        else
        {
            gm->pre_stream_prev->pre_stream_next = gm->pre_stream_next;
            gm->pre_stream_next->pre_stream_prev = gm->pre_stream_prev;
            gm->pre_stream_prev = nullptr;
            gm->pre_stream_next = nullptr;
        }
        gm = next;
    }
}

// 0x00A37D20 (:388)
// LTCG: shape in EAX. Unwraps translate/transform shapes down to the shape they place.
static const hkpShape* get_base_shape(const hkpShape* shape)
{
    for (;;)
    {
        const hkcdShapeType::ShapeTypeEnum type = (hkcdShapeType::ShapeTypeEnum)shape->m_type.m_storage;
        if (type == hkcdShapeType::CONVEX_TRANSLATE || type == hkcdShapeType::CONVEX_TRANSFORM)
        {
            shape = ((const hkpConvexTransformShapeBase*)shape)->m_childShape.getChild();
        }
        else if (type == hkcdShapeType::TRANSFORM)
        {
            shape = ((const hkpTransformShape*)shape)->getChildShape();
        }
        else
        {
            return shape;
        }
    }
}

// 0x00A37D40 (:407)
// LTCG: local_bmin in ESI, local_bmax in EDI. The bounding box of the placed shape in its own space.
static void get_local_bbox_from_shape(const hkpShape* shape, vector& local_bmin, vector& local_bmax)
{
    const hkcdShapeType::ShapeTypeEnum type = (hkcdShapeType::ShapeTypeEnum)shape->m_type.m_storage;
    if (type == hkcdShapeType::CONVEX_TRANSLATE || type == hkcdShapeType::CONVEX_TRANSFORM || type == hkcdShapeType::TRANSFORM)
    {
        shape = get_base_shape(shape);
    }
    hkAabb aabb;
    shape->getAabb(hkTransformf::getIdentity(), 0.0f, aabb);
    havok_to_game_aabb(local_bmin, local_bmax, aabb);
}

// 0x00A37DB0 (:415)
// LTCG: handle in ESI. Mass properties of the body's base shape box with the given mass.
static void recalc_and_set_mass_props(unsigned int handle, float mass)
{
    const hkpShape* shape = havok_body_get_shape(handle);
    if (shape == nullptr)
    {
        return;
    }
    const hkcdShapeType::ShapeTypeEnum type = (hkcdShapeType::ShapeTypeEnum)shape->m_type.m_storage;
    if (type == hkcdShapeType::CONVEX_TRANSLATE || type == hkcdShapeType::CONVEX_TRANSFORM || type == hkcdShapeType::TRANSFORM)
    {
        shape = get_base_shape(shape);
    }
    hkAabb aabb;
    shape->getAabb(hkTransformf::getIdentity(), 0.0f, aabb);
    vector bmin;
    vector bmax;
    havok_to_game_aabb(bmin, bmax, aabb);
    vector center_of_mass;
    matrix inertia_tensor;
    havok_body_calc_mass_props(bmin, bmax, mass, center_of_mass, inertia_tensor);
    havok_body_set_mass_props(handle, mass, center_of_mass, inertia_tensor);
}

// ---- class registration (:438)
// 0x00A130D0
unsigned int general_mover::get_classname_hash()
{
    return general_mover_instantiator.this_hash;
}

// 0x00A37E90
int general_mover::pool_remaining()
{
    return general_moverpool.get_free();
}

// 0x00A5E830
object* alloc_general_mover()
{
    return general_moverpool.alloc(0);
}

// 0x00A130B0
int bpo_index_general_mover()
{
    return 0;
}

// 0x00A130A0
void types_general_mover(signed char& type, signed char& subtype)
{
    type = OT_MOVER;
    subtype = OT_SUB_MOVER_GENERAL;
}

// 0x00A8A940
void general_mover::pool_free()
{
    general_moverpool.free(this);
}

// 0x00A130C0
const char* classname_general_mover()
{
    return "general_mover";
}

// ---- general_mover
// 0x00A37EA0 (:439)
general_mover::general_mover()
{
}

// 0x00AE4E90 (:451)
// Load: flags and original object; static props get their collision type (force fields their own), pieces of
// destroyable shapes their shape, piece index and move type (independent of their parent beyond move type 2) and
// destroyable hitpoints; saved hitpoints. Save: nothing while fading out.
bool general_mover::serialize(obj_prop_block& pblock, bool load)
{
    if (!object_mover::serialize(pblock, load))
    {
        return false;
    }
    obj_prop* prop;
    if (load)
    {
        pre_stream_prev = nullptr;
        pre_stream_next = nullptr;
        fade_out_timer.invalidate();
        last_damaged_timer.invalidate();
        if (pblock.get_internal(prop, GM_PROP_FLAGS, 5) && prop->size == sizeof(unsigned int))
        {
            gm_flags = *(unsigned int*)prop->data;
        }
        original_object = 0xFFFFFFFF;
        if (pblock.get_internal(prop, GM_PROP_ORIGINAL_OBJECT, 5) && prop->size == sizeof(unsigned int))
        {
            original_object = *(unsigned int*)prop->data;
        }
        if ((mover_flags & 0x4) == 0)
        {
            collision_type = mover_is_type(MOVER_TYPE_FORCE_FIELD) ? COLLIDE_FORCE_FIELD : COLLIDE_MOVER;
            pblock.get_optional(GM_PROP_COLLISION_TYPE, collision_type);
        }
        else
        {
            rdd.set_loaded(true); // the shape pointer is read into the resource data
            resource_dependent_data* data = rdd.get_data();
            data->shape = nullptr;
            pblock.get_optional(GM_PROP_SHAPE_POINTER, data->shape);
            if (pblock.get_internal(prop, GM_PROP_SUBPIECE_INDEX, 5) && prop->size == sizeof(unsigned int))
            {
                subpiece_index = *(unsigned int*)prop->data;
            }
            rdd.set_loaded(false);
            if (pblock.get_internal(prop, GM_PROP_MOVE_TYPE, 5) && prop->size == sizeof(int))
            {
                move_type = *(int*)prop->data;
            }
            if (move_type > 2)
            {
                obj_flags.is_dependent = false;
                bp_handle = havok_bpo_update_flags(bp_handle, World.calc_bpo_flags(this));
            }
            if (checking_reset < 1)
            {
                m_level_obj_properties->m_hitpoints = GM_DESTROYABLE_HITPOINTS;
                m_level_obj_properties->m_dislodge_hitpoints = GM_DESTROYABLE_DISLODGE_HITPOINTS;
            }
        }
        destruction_uid = 0xFFFFFFFF;
        if (pblock.get_internal(prop, GM_PROP_DESTRUCT_UID, 5) && prop->size == sizeof(unsigned int))
        {
            destruction_uid = *(unsigned int*)prop->data;
        }
        if (checking_reset < 1)
        {
            pblock.get_optional(GM_PROP_HITPOINTS, m_level_obj_properties->m_hitpoints);
            pblock.get_optional(GM_PROP_DISLODGE_HITPOINTS, m_level_obj_properties->m_dislodge_hitpoints);
        }
        const bool sync = mover_should_sync();
        remote_obj_flags.client_sync = sync;
        if (sync)
        {
            reference_position_index = object_mover_get_reference_pos_index(pos);
        }
        mp_sim_state = nullptr;
        return true;
    }
    if (fade_out_timer.valid())
    {
        return false;
    }
    pblock.set(GM_PROP_FLAGS, gm_flags & 0xFFFFFBE1); // drops the pending death states and the delete flag
    if ((mover_flags & 0x4) != 0)
    {
        pblock.set(GM_PROP_MOVE_TYPE, move_type);
        pblock.set(GM_PROP_SUBPIECE_INDEX, subpiece_index);
        pblock.set(GM_PROP_DESTRUCT_UID, destruction_uid);
        return true;
    }
    if (collision_type != COLLIDE_MOVER)
    {
        pblock.set(GM_PROP_COLLISION_TYPE, collision_type);
    }
    const level_object_inst_properties* props = m_level_obj_properties;
    if (props->m_hitpoints != props->m_base_properties->m_max_hitpoints && (gm_flags & 0x12) == 0 && (gm_flags & 0x800) == 0)
    {
        pblock.set(GM_PROP_HITPOINTS, props->m_hitpoints);
    }
    if (props->m_dislodge_hitpoints != props->m_base_properties->m_max_dislodge_hitpoints)
    {
        pblock.set(GM_PROP_DISLODGE_HITPOINTS, props->m_dislodge_hitpoints);
    }
    return true;
}

// 0x00A37F20 (:593)
// Movers outside their zone are not saved; loose movable props of the first handle range only sometimes (5%, shards
// 25%: the chance to keep them).
obj_serialize_mode general_mover::serialize_mode()
{
    if (!World.object_physically_in_original_zone(this))
    {
        return OSM_NONE;
    }
    const unsigned int flags = mover_flags;
    if ((flags & 0x20000) == 0 && (flags & 0x4) != 0 && ((get_handle() >> 16) & 0xFF) == 0xFF)
    {
        const float keep_chance = (flags & 0x2) != 0 ? 0.25f : 0.05f;
        if (keep_chance < fl_rand())
        {
            return OSM_NONE;
        }
    }
    return OSM_FULL;
}

// 0x00AA1970 (:666)
// Load: builds the render instance (a mesh for a general object, a piece of the shape for a destroyable piece) and the
// Havok body from the level object (or piece) properties; posts EVENT_MOVER_STREAM_IN. A mover whose chunk, object or
// body is missing is destroyed. Unload: frees the body, the instance and the release effect.
bool general_mover::stream(bool load)
{
    if (!object_mover::stream(load))
    {
        return false;
    }
    if (!load)
    {
        if (!rdd.is_loaded())
        {
            return false;
        }
        gm_flags |= 0x20;
        if (havok_handle != 0xFFFFFFFF)
        {
            rfgl_object_destroyed(this);
            nav_cell_detour_mover_destroyed(this);
            havok_body_free(havok_handle, true, true, false);
            havok_handle = 0xFFFFFFFF;
        }
        if (obj_flags.destroyed && (mover_flags & 0x8000000) != 0)
        {
            rfg_mover_delete_dynamic_link(this);
        }
        resource_dependent_data* data = rdd.get_data();
        if (data->model_instance_p != nullptr)
        {
            data->model_instance_p->release();
            data->model_instance_p = nullptr;
        }
        const unsigned int release_effect = m_level_obj_properties->m_release_effect_instance;
        if (release_effect != 0xFFFFFFFF)
        {
            object* effect = World.object_from_handle(release_effect);
            if (effect != nullptr && effect->get_obj_type() == OT_EFFECT)
            {
                ((object_effect*)effect)->stop(object_effect::SS_STOP_EMITTING);
                m_level_obj_properties->m_release_effect_instance = 0xFFFFFFFF;
            }
        }
        rdd.set_loaded(false);
        return true;
    }

    rdd.set_loaded(true);
    resource_dependent_data* data = rdd.get_data();
    c_chunk_base* chunk = chunk_get_base_by_uid(chunk_uid);
    if (chunk == nullptr)
    {
        World.destroy_object(this, false);
        return false;
    }
    physics_create_info pci;
    const level_object_base_properties* base_props = m_level_obj_properties->m_base_properties;
    unsigned int material_flags = 0; // property GM_PROPERTY_MATERIAL_FLAGS of the body
    if ((mover_flags & 0x4) != 0)
    {
        rfg_base_destroyable_shape* shape_base;
        if (data->shape != nullptr)
        {
            // a piece just broken off: its shape came through the property block
            shape_uid = data->shape->shape_uid;
            shape_base = data->shape->parent;
            gm_flags &= ~0x1000u;
        }
        else
        {
            shape_base = chunk_get_destroyable_object(chunk, chunk_obj_uid, this);
            if (shape_base == nullptr)
            {
                return false;
            }
            data->model_instance_p = build_new_rl_instance_from_shape_base(shape_base, chunk, subpiece_index);
            if (data->model_instance_p == nullptr)
            {
                World.destroy_object(this, false);
                return false;
            }
            mover_flags &= ~0x80u;
            gm_flags |= 0x1000;
        }
        data->shape = nullptr;
        const rfg_subpiece_base* sp_base = &shape_base->base_objects.to_pointer()[subpiece_index];
        const rfg_subpiece_base_extra_data* extra = &shape_base->base_extra_data.to_pointer()[subpiece_index];
        const rfg_physical_material* material = rfg_get_physical_material_by_index(sp_base->physical_material_index);
        pci.cm_index = extra->collision_model;
        matrix local_orient;
        shape_base->get_subpiece_orient_local(sp_base, local_orient);
        pci.mass = sp_base->mass;
        if (pci.mass - 1.0f < 0.0f)
        {
            pci.mass = 1.0f; // at least 1 kg
        }
        pci.physical_material = sp_base->physical_material_index;
        pci.effect_material = rfg_get_effect_material_index(material->effect_material);
        pci.friction = material->friction;
        pci.restitution = material->restitution;
        pci.collision_type = rfg_dyn_get_filter_by_move_type(move_type, false);
        pci.auto_remove_level = rfg_dyn_get_auto_remove_level_by_move_type(move_type, mover_flags);
        pci.solver_deactivation = (hkpRigidBodyCinfo::SolverDeactivation)3;
        vector local_bmin;
        vector local_bmax;
        get_local_bbox_from_shape(extra->shape.to_pointer(), local_bmin, local_bmax);
        havok_body_calc_mass_props(local_bmin, local_bmax, pci.mass, pci.precalc_center_of_mass, pci.precalc_tensor);
        // the center of mass of the piece, in the piece's space
        const vector com_offset = sp_base->center_of_mass - sp_base->pos;
        pci.precalc_center_of_mass = com_offset.copy_rotate(local_orient);
        pci.flags |= 0x1;
        if ((material->flags & 0x80) != 0)
        {
            material_flags = 0x8;
        }
        if ((material->flags & 0x1) != 0)
        {
            material_flags |= 0x10;
        }
        if (sp_base->shape_type == 3)
        {
            gm_flags |= 0x80; // a shard
        }
    }
    else
    {
        gobj_index = chunk_get_general_object_index(chunk, chunk_obj_uid, this);
        if (gobj_index < 0)
        {
            World.destroy_object(this, false);
            return false;
        }
        general_object_base* obj_base = &chunk->general_objects.to_pointer()[gobj_index];
        pci.cm_index = obj_base->collision_models[0];
        data->model_instance_p = build_new_rl_instance_from_object_base(obj_base, chunk);
        if (data->model_instance_p == nullptr)
        {
            World.destroy_object(this, false);
            return false;
        }
        mover_flags &= ~0x80u;
        pci.mass = base_props->m_mass;
        pci.physical_material = base_props->m_physical_material;
        pci.effect_material = base_props->m_effect_material;
        pci.friction = base_props->m_friction;
        pci.restitution = base_props->m_restitution;
        pci.linear_damping = base_props->m_linear_dampening;
        pci.angular_damping = base_props->m_angular_dampening;
        pci.flags |= 0x1;
        pci.collision_type = collision_type;
        havok_body_calc_mass_props(obj_base->bmin, obj_base->bmax, pci.mass, pci.precalc_center_of_mass, pci.precalc_tensor);
    }
    if (data->model_instance_p != nullptr)
    {
        data->model_instance_p->set_casts_shadows((mover_flags & 0x10) != 0);
        data->model_instance_p->set_casts_drop_shadows((mover_flags & 0x4000) != 0);
        if (gameplay_props != nullptr && gameplay_props->lod_properties != nullptr)
        {
            data->model_instance_p->set_lod_off_distance(gameplay_props->lod_properties->m_lod_off_dist); // inlined
        }
    }
    update_world_bounding_box(pos, orient);
    pci.pos = pos;
    pci.orient = orient;
    pci.shape_type = (havok_shape_type)4;
    pci.obj_ptr = this;
    pci.simulation_quality_type = (hkpCollidableQualityType)2;
    pci.motion_type = 1;
    if ((mover_flags & 0x4) != 0)
    {
        pci.flags |= 0x4;
    }
    else
    {
        pci.flags |= 0x8;
        if (child_constraint_ptr == nullptr && host_constraint_ptr == nullptr)
        {
            pci.flags |= 0x4;
        }
    }
    havok_handle = havok_body_create(pci);
    if (havok_handle == 0xFFFFFFFF)
    {
        World.destroy_object(this, false);
        return false;
    }
    hkpRigidBody* rb = havok_body_get_pointer(havok_handle);
    if (rb != nullptr && material_flags != 0)
    {
        havok_set_property(rb, GM_PROPERTY_MATERIAL_FLAGS, (void*)(uintptr_t)material_flags);
    }
    general_mover_set_movable(this, (mover_flags & 0xC) != 0);
    rfgl_object_created(this);
    nav_cell_detour_mover_created(this);
    update_render(nullptr);
    mover_stream_in_event event;
    event.handle = get_handle();
    gem_post_event(event);
    if ((gm_flags & 0x100) != 0)
    {
        gm_flags |= 0x4; // a killed mover streams in as a corpse again
    }
    return true;
}

// 0x00AB37E0 (:954)
// Releases the piece of the shape the mover was made of.
void general_mover::free_this()
{
    gm_flags &= ~0x1u;
    if (Multiplayer)
    {
        multi_core_object_process_object_free(this);
    }
    if (subpiece_index != 0xFFFFFFFF)
    {
        rfg_destroyable_shape* shape = rfg_shape_get_shape_from_uid(shape_uid);
        if (shape != nullptr)
        {
            rfg_subpiece* sp = shape->get_subpiece(subpiece_index);
            if (sp != nullptr && sp->get_owner() == this)
            {
                sp->set_owner(nullptr);
            }
        }
        subpiece_index = 0xFFFFFFFF;
    }
    object_mover::free_this();
}

// 0x00A8A950 (:999)
// The death sequence of a streamed mover: delete requests, dislodging (made movable), the death effect and the
// corpse model (or deletion without one), the prefix effect, the death explosion and the release effect.
void general_mover::do_frame_pre_sim()
{
    if (!obj_flags.streamed)
    {
        return;
    }
    if ((gm_flags & 0x400) != 0)
    {
        World.destroy_object(this, false);
        return;
    }
    if ((gm_flags & 0x8) != 0)
    {
        if (havok_handle != 0xFFFFFFFF && !obj_flags.destroyed && !havok_body_is_movable(havok_handle))
        {
            havok_body_set_movable(havok_handle, true);
        }
        if (is_killed())
        {
            recalc_and_set_mass_props(havok_handle, m_level_obj_properties->m_base_properties->m_corpse_mass);
        }
        havok_body_force_activate(havok_handle);
        mover_flags |= 0x8;
        m_level_obj_properties->m_flags.m_movable_by_humans = true;
        gm_flags &= ~0x8u;
    }
    if ((gm_flags & 0x4) != 0)
    {
        gm_flags &= ~0x4u;
        const unsigned int death_effect = m_level_obj_properties->m_base_properties->m_death_effect;
        if (death_effect != 0xFFFFFFFF)
        {
            effects_play(death_effect, pos, orient, false, get_handle(), -1, 0xFFFFFFFF, 1.0f, false, EFFECTS_CATEGORY_NONE);
        }
        const c_chunk_base* chunk = chunk_get_base_by_uid(chunk_uid);
        const general_object_base* objects = chunk != nullptr ? chunk->general_objects.to_pointer() : nullptr;
        if (objects != nullptr)
        {
            const general_object_base* obj_base = &objects[gobj_index];
            const unsigned short corpse_model = obj_base->collision_models[1];
            if (corpse_model == 0xFFFF || (mover_flags & 0x4) != 0)
            {
                if (!Multiplayer || multi_is_server())
                {
                    World.destroy_object(this, false);
                }
                else
                {
                    set_render_opacity(0);
                }
            }
            else
            {
                gm_flags |= 0x200; // the corpse mesh is set up by update_render
                update_pos_and_orient(pos, orient, true);
                if (obj_base->collision_models[0] != obj_base->collision_models[1])
                {
                    havok_body_replace_collision_model(havok_handle, corpse_model);
                }
                if (havok_body_is_movable(havok_handle))
                {
                    recalc_and_set_mass_props(havok_handle, m_level_obj_properties->m_base_properties->m_corpse_mass);
                }
                level_object_inst_properties* props = m_level_obj_properties;
                if (props->m_flags.m_dislodge_on_death)
                {
                    gm_flags |= 0x8;
                }
                if ((gm_flags & 0x100) == 0 && props->m_base_properties->m_release_effect != 0xFFFFFFFF)
                {
                    props->m_death_timer.set(props->m_base_properties->m_release_delay_ms);
                }
                object* child = child_ptr;
                while (child != nullptr)
                {
                    if (child->get_obj_type() == OT_EFFECT)
                    {
                        ((object_effect*)child)->handle_host_death();
                    }
                    child = child->child_next;
                    if (child == child_ptr)
                    {
                        break;
                    }
                }
            }
        }
        gm_flags |= 0x100;
    }
    const unsigned int flags = gm_flags;
    if ((flags & 0x100) == 0)
    {
        if ((flags & 0x10) != 0)
        {
            process_prefix_effect();
            return;
        }
        if ((flags & 0x2) != 0)
        {
            human* owner = World.object_from_handle<human>(final_attacker);
            vector exp_pos;
            matrix exp_orient;
            const explosion_info* e_info = get_death_explosion_info_pos_and_orient(0, exp_pos, &exp_orient);
            if (e_info != nullptr)
            {
                const weapon_info* w_info = nullptr;
                if (Multiplayer)
                {
                    w_info = multi_gw_get_custom_weapon_info(MCW_EXPLOSIVE_OBJECT);
                }
                explosion_create(e_info, this, (object*)owner, exp_pos, exp_orient, &exp_orient.uvec, w_info, false);
            }
            gm_flags = (gm_flags & ~0x2u) | 0x800;
        }
        return;
    }
    // killed: the release effect loops for its time after the release delay
    level_object_inst_properties* props = m_level_obj_properties;
    if (!props->m_death_timer.elapsed())
    {
        return;
    }
    if (props->m_release_effect_instance == 0xFFFFFFFF)
    {
        props->m_release_effect_instance = effects_play(props->m_base_properties->m_release_effect, pos, orient, true, get_handle(), -1, 0xFFFFFFFF, 1.0f, false, EFFECTS_CATEGORY_NONE);
    }
    if (props->m_base_properties->m_release_looping_ms < props->m_death_timer.time_since())
    {
        object_effect* effect = World.object_from_handle<object_effect>(props->m_release_effect_instance);
        if (effect != nullptr)
        {
            effect->stop(object_effect::SS_STOP_EMITTING);
        }
        props->m_release_effect_instance = 0xFFFFFFFF;
        props->m_death_timer.invalidate();
    }
}

// 0x00A37F90 (:1159)
rl_renderable_instance* general_mover::get_rl_model_instance() const
{
    if (!rdd.is_loaded())
    {
        return nullptr;
    }
    return rdd.get_data()->model_instance_p;
}

// 0x00A130E0 (:1172)
bool general_mover::is_killed() const
{
    return (gm_flags & 0x100) != 0;
}

// 0x00A8ACF0 (:1183)
// Movable props take damage scaled by 200/mass and fade out (with a load balance effect) once destroyed, if their
// material allows it or a heavy vehicle hit them. Static props run the death sequence: explosion or death effect,
// prefix effect, destroyed event, control drop and the HUD notice; dislodge damage makes them movable. False for
// melee hits that did not move the prop.
bool general_mover::apply_damage(float damage, damaged_by_types from_type, const vector& hit_pos, const vector& hit_normal, const object* source, unsigned int hit_shape_key, bool from_server)
{
    if (Multiplayer && !multi_is_server() && !from_server)
    {
        return true;
    }
    if (wrecking_crew_is_active() && wrecking_crew_get_state() < WCS_PLAYERS_TURN)
    {
        return true;
    }
    if ((mover_flags & 0x4) != 0)
    {
        if (script_point_apply_damage(source, damage, from_type, hit_shape_key, from_server))
        {
            return true;
        }
        const double scaled_damage = (double)damage * 200.0;
        m_level_obj_properties->apply_damage((int)(scaled_damage / get_mass()), from_type, from_server);
        const bool heavy_vehicle = source != nullptr && source->get_obj_type() == OT_VEHICLE && ((const vehicle*)source)->get_infinite_mass();
        if (!heavy_vehicle)
        {
            if (m_level_obj_properties->m_hitpoints > 0)
            {
                return true;
            }
            const rfg_physical_material* material = rfg_get_physical_material_by_index(get_physical_material(0xFFFFFFFF, nullptr, nullptr));
            if ((material->flags & 0x200) == 0)
            {
                return true;
            }
        }
        if (havok_handle != 0xFFFFFFFF && !obj_flags.destroyed && !havok_body_is_movable(havok_handle))
        {
            havok_body_set_movable(havok_handle, true);
        }
        vector bmin;
        vector bmax;
        get_local_bbox(bmin, bmax);
        const float size_x = bmax.x - bmin.x;
        const float size_y = bmax.y - bmin.y;
        const float size_z = bmax.z - bmin.z;
        float size = size_x - size_y < 0.0f ? size_y : size_x;
        if (size - size_z < 0.0f)
        {
            size = size_z;
        }
        const unsigned char effect_material = get_effect_material(0xFFFFFFFF, nullptr, nullptr);
        const material_event_size event_size = rfg_effect_candidate_get_size(effect_material, MATERIAL_EVENT_LOAD_BALANCE, size);
        rfg_effect_candidate_create(effect_material, pos, MATERIAL_EVENT_LOAD_BALANCE, event_size, INVALID_MATERIAL_EVENT_SHAPE_TYPE, false, 0, nullptr, nullptr, 0xFFFFFFFF, 0xFF, 0xFF);
        if (!Multiplayer || multi_is_server())
        {
            object_mover_mark_for_fade(this, 0);
        }
        return true;
    }

    if (Multiplayer && multi_is_server())
    {
        multi_game_object_mover_apply_damage(get_handle(), (int)damage, from_type);
    }
    const bool was_dead = m_level_obj_properties->m_hitpoints <= 0;
    const bool was_dislodged = m_level_obj_properties->m_dislodge_hitpoints <= 0;
    m_level_obj_properties->apply_damage((int)damage, from_type, from_server);
    if (!was_dead && m_level_obj_properties->m_hitpoints <= 0)
    {
        level_object_inst_properties* props = m_level_obj_properties;
        gm_flags |= props->m_base_properties->m_flags.m_death_explosion ? 0x2 : 0x4;
        if (source != nullptr)
        {
            final_attacker = resolve_attacker(source); // inlined
        }
        bool skip_death = false;
        if ((gm_flags & 0x2) != 0)
        {
            if ((gm_flags & 0x10) != 0)
            {
                props->m_death_timer.set(0);
                skip_death = true; // see the notes
            }
            else if (setup_prefix_effect())
            {
                m_level_obj_properties->apply_damage(-m_level_obj_properties->m_base_properties->m_prefix_hitpoints, from_type, from_server);
                gm_flags |= 0x10;
            }
        }
        if (!skip_death)
        {
            object_mover_maybe_add_to_destroyed_event_queue(this);
            activity_register_death(this, source);
            drop_control();
            if (get_parent() == 0xFFFFFFFF && !World.object_was_destroyed(get_handle()))
            {
                hud_ui_mission_set_building_destroyed(World.get_object_name(this), get_importance_level(), true);
            }
        }
    }
    if (!was_dislodged && m_level_obj_properties->m_dislodge_hitpoints <= 0)
    {
        gm_flags |= 0x8;
    }
    object_mover_maybe_add_to_damaged_event_queue(this);
    last_damaged_timer.set(0);
    if (from_type == DBT_MELEE && !havok_body_is_movable(havok_handle) && (gm_flags & 0x8) == 0)
    {
        return false;
    }
    return true;
}

// 0x00A37FB0 (:1318)
// Static props in a world are anchored when they overlap something (world anchors; for a child of an rfg_mover that
// allows dynamic links, world geometry only, the links come from the collector); otherwise they become movable.
bool general_mover::find_anchor_points()
{
    if ((mover_flags & 0x1000000) != 0)
    {
        return true;
    }
    hkpRigidBody* rb = havok_body_get_pointer(havok_handle);
    if (rb == nullptr || rb->m_world == nullptr)
    {
        return false;
    }
    unsigned int col_type = COLLIDE_WORLD_ANCHOR;
    if (m_level_obj_properties->m_base_properties->m_flags.m_allow_dynamic_link && get_parent() != 0xFFFFFFFF)
    {
        object* parent_obj = World.object_from_handle(get_parent());
        if (parent_obj != nullptr && parent_obj->get_obj_type() == OT_MOVER && parent_obj->get_sub_type() == OT_SUB_MOVER_RFG)
        {
            ((object_mover*)parent_obj)->find_anchor_points();
            col_type = COLLIDE_RAY_WORLD_ONLY;
        }
    }
    anchor_collector collector;
    create_anchors(col_type, collector);
    general_mover_set_movable(this, !collector.has_hit);
    mover_flags |= 0x1000000;
    return true;
}

// 0x00A130F0 (:1361)
void general_mover::get_bbox(vector& bmin, vector& bmax) const
{
    bmin = last_known_bmin;
    bmax = last_known_bmax;
}

// 0x00A5E840 (:1372)
// The render instance's local box, else the last known world box relative to the position.
void general_mover::get_local_bbox(vector& bmin, vector& bmax) const
{
    if (rdd.is_loaded() && rdd.get_data()->model_instance_p != nullptr)
    {
        const rl_aabb aabb = rdd.get_data()->model_instance_p->get_local_aabb(); // inlined (center -/+ half extents)
        bmin = aabb.min;
        bmax = aabb.max;
        return;
    }
    bmin = last_known_bmin - pos;
    bmax = last_known_bmax - pos;
}

// 0x00AB3860 (:1386)
// Single player: a prop that left its zone is deleted.
void general_mover::update_pos_and_orient(const vector& update_pos, const matrix& update_orient, bool set_havok_data)
{
    object::update_pos_and_orient(update_pos, update_orient, set_havok_data);
    mover_flags |= 0x40000;
    update_world_bounding_box(pos, orient);
    if (!Multiplayer && !World.object_physically_in_original_zone(this))
    {
        World.destroy_object(this, false);
    }
}

// 0x00A13130 (:1394)
// The local box transformed by the current placement (the arguments are not used).
void general_mover::update_world_bounding_box(const vector& update_pos, const matrix& update_orient)
{
    vector local_bmin;
    vector local_bmax;
    get_local_bbox(local_bmin, local_bmax);
    bbox_instance(last_known_bmin, last_known_bmax, orient, pos, local_bmin, local_bmax);
}

// 0x00A5EA40 (:1407)
// Script points of the chunk; for a piece of a destroyable shape relative to the piece.
bool general_mover::get_tag_local_coords(int tag_index, vector& tag_pos, matrix* tag_orient) const
{
    if (!rdd.is_loaded())
    {
        if (tag_orient != nullptr)
        {
            tag_orient->make_identity();
        }
        tag_pos.zero();
        return false;
    }
    const c_chunk_base* chunk = chunk_get_base_by_uid(chunk_uid);
    if (chunk == nullptr)
    {
        return false; // see the notes
    }
    if (tag_index < 0 || tag_index >= chunk->num_script_points)
    {
        return false;
    }
    const c_script_point& point = chunk->script_points.to_pointer()[tag_index];
    point.transform.get_translation(tag_pos);
    if (tag_orient != nullptr)
    {
        point.transform.get_orientation(*tag_orient);
    }
    if ((mover_flags & 0x4) != 0)
    {
        rfg_base_destroyable_shape* shape_base = chunk_get_destroyable_object(chunk, chunk_obj_uid, nullptr);
        const rfg_subpiece_base* sp_base = &shape_base->base_objects.to_pointer()[subpiece_index];
        matrix local_orient;
        shape_base->get_subpiece_orient_local(sp_base, local_orient);
        tag_pos -= sp_base->pos;
        tag_pos.rotate(local_orient);
        if (tag_orient != nullptr)
        {
            tag_orient->rotate(local_orient);
        }
    }
    return true;
}

// 0x00A13180 (:1456)
// A destroyed script point takes the whole prop (multiplayer clients only hide it).
void general_mover::script_point_destroy(int idx)
{
    if (Multiplayer && !multi_is_server())
    {
        set_render_opacity(0);
        return;
    }
    World.destroy_object(this, false);
}

// 0x00A38080 (:1470)
rl_renderable_instance* general_mover::get_render_model_instance()
{
    if (!rdd.is_loaded())
    {
        return nullptr;
    }
    return rdd.get_data()->model_instance_p;
}

// 0x00AA1FF0 (:1475)
// A mover fader dissolving the render instance with the nano material over the remaining fade time (at least 0.3 s);
// see the header comment (Partial).
void general_mover::setup_nano_dissolve()
{
    if (!rdd.is_loaded() || rdd.get_data()->model_instance_p == nullptr)
    {
        return;
    }
}

// 0x00A5EBA0 (:1519)
// A pending corpse mesh switches the mesh instance to the corpse sub mesh.
void general_mover::update_render(rl_renderer*)
{
    if ((gm_flags & 0x200) != 0 && obj_flags.streamed)
    {
        gm_flags &= ~0x200u;
        if (!rdd.is_loaded() || rdd.get_data()->model_instance_p == nullptr)
        {
            return;
        }
        const c_chunk_base* chunk = chunk_get_base_by_uid(chunk_uid);
        gobj_index = chunk_get_general_object_index(chunk, chunk_obj_uid, this);
        if (gobj_index < 0)
        {
            return;
        }
        ((rl_mesh_instance*)rdd.get_data()->model_instance_p)->set_submesh_index(chunk->general_objects.to_pointer()[gobj_index].mesh_subpieces[1]); // inlined
    }
    object_mover::update_render(nullptr);
}

// 0x00AA2200 (:1549)
// A piece broken off an rfg_mover gets its render instance split off the donor's model (see the notes).
void general_mover::create_deferred_ri()
{
    resource_dependent_data* data = rdd.is_loaded() ? rdd.get_data() : nullptr;
    if (data->model_instance_p != nullptr || creator_handle == 0xFFFFFFFF)
    {
        return;
    }
    object* creator = World.object_from_handle(creator_handle);
    if (creator == nullptr || creator->get_obj_type() != OT_MOVER || creator->get_sub_type() != OT_SUB_MOVER_RFG)
    {
        return;
    }
    rfg_mover* donor = (rfg_mover*)creator;
    if (!donor->rdd.is_loaded() || donor->rdd.get_data()->shape == nullptr || donor->rdd.get_data()->model_instance_p == nullptr)
    {
        return;
    }
    rfg_destroyable_shape* shape = donor->rdd.get_data()->shape;
    data->model_instance_p = build_new_rl_instance_from_shape(shape, subpiece_index, donor->rdd.get_data()->model_instance_p, creator_handle, this);
    creator_handle = 0xFFFFFFFF;
    if (data->model_instance_p == nullptr)
    {
        if (!Multiplayer)
        {
            World.destroy_object(this, false);
        }
        return;
    }
    data->model_instance_p->set_casts_shadows((mover_flags & 0x10) != 0);
    data->model_instance_p->set_casts_drop_shadows((mover_flags & 0x4000) != 0);
    if (gameplay_props != nullptr && gameplay_props->lod_properties != nullptr)
    {
        data->model_instance_p->set_lod_off_distance(gameplay_props->lod_properties->m_lod_off_dist); // inlined
    }
    if ((shape->get_subpiece(subpiece_index)->flags & 0x8000) != 0)
    {
        setup_nano_dissolve();
    }
}

// 0x00AB38C0 (:1588)
// Fades by camera distance: 10 km for props, 35/50 m for resting shards (alternating by piece index).
void general_mover::update_render_opacity()
{
    if (havok_handle == 0xFFFFFFFF)
    {
        return;
    }
    const unsigned int flags = mover_flags;
    const vector delta = pos - Rfg_camera.real_pos;
    float max_dist_squared = GM_FADE_DIST_SQR;
    if ((flags & 0x4) != 0 && (flags & 0x2) == 0)
    {
        if ((flags & 0x400) != 0 || !havok_body_is_active(havok_handle))
        {
            max_dist_squared = (subpiece_index & 1) != 0 ? Shard_fade_dist0_sqr : Shard_fade_dist1_sqr;
        }
    }
    handle_fading(delta.y * delta.y + delta.x * delta.x + delta.z * delta.z, max_dist_squared);
}

// 0x00A380A0 (:1622)
bool general_mover::enable_object_outline(int layer)
{
    rl_renderable_instance* ri = get_render_model_instance();
    if (ri != nullptr && (unsigned int)layer < 4)
    {
        ri->set_outline_enabled(layer); // inlined
    }
    return true;
}

// 0x00A131C0 (:1632)
bool general_mover::disable_object_outline()
{
    rl_renderable_instance* ri = get_render_model_instance();
    if (ri != nullptr)
    {
        ri->set_outline_disabled(); // inlined
    }
    return true;
}

// 0x00A131E0 (:1642)
// Multiplayer syncs static props and the pieces with a destruction id.
bool general_mover::mover_should_sync()
{
    return Multiplayer && ((mover_flags & 0x4) == 0 || destruction_uid != 0xFFFFFFFF);
}

// 0x00A13210 (:1660)
bool general_mover::debug_render_havok(const vector& center, const matrix& orientp, color c)
{
    return false;
}

// 0x00A13220 (:1682)
int general_mover::time_since_last_damage() const
{
    if (!last_damaged_timer.valid())
    {
        return -1;
    }
    return last_damaged_timer.time_since();
}

// 0x00A13240 (:1691)
float general_mover::percent_destroyed() const
{
    if ((gm_flags & 0x10) != 0)
    {
        return 1.0f;
    }
    const level_object_inst_properties* props = m_level_obj_properties;
    float pct = 1.0f - (float)props->m_hitpoints / (float)props->m_base_properties->m_max_hitpoints;
    if (pct - 0.0f < 0.0f)
    {
        pct = 0.0f;
    }
    if (0.0f <= pct - 1.0f)
    {
        pct = 1.0f;
    }
    return pct;
}

// 0x00A5EC30 (:1706)
// Starts the prefix effects (at their attachment script point, else at the mover) and the death timer: a random
// duration in single player, a third of the range in multiplayer. False without a prefix duration.
bool general_mover::setup_prefix_effect()
{
    const level_object_base_properties* base_props = m_level_obj_properties->m_base_properties;
    if (base_props->m_explosion_prefix_min_duration < 1)
    {
        return false;
    }
    int script_point = -1;
    for (int i = 0; i < NUM_PREFIX_EFFECTS; i++)
    {
        const custom_effect_info& info = base_props->m_prefix_effect_infos[i];
        if (info.effect_handle == 0xFFFFFFFF)
        {
            continue;
        }
        if (info.attachment_point_name != nullptr)
        {
            const c_chunk_base* chunk = chunk_get_base_by_uid(chunk_uid);
            if (chunk != nullptr && chunk->num_script_points > 0)
            {
                script_point = -1;
                const c_script_point* points = chunk->script_points.to_pointer();
                for (int j = 0; j < chunk->num_script_points; j++)
                {
                    if (v_stricmp(points[j].name, info.attachment_point_name) == 0)
                    {
                        script_point = j;
                        break;
                    }
                }
            }
        }
        unsigned int effect;
        if (script_point < 0)
        {
            effect = effects_play(info.effect_handle, pos, orient, info.looping, get_handle(), -1, 0xFFFFFFFF, 1.0f, false, EFFECTS_CATEGORY_NONE);
        }
        else
        {
            effect = effects_play_at_script_point(info.effect_handle, get_handle(), script_point, info.looping, nullptr, nullptr);
        }
        m_level_obj_properties->m_prefix_effects[i] = effect;
    }
    int duration;
    if (!Multiplayer)
    {
        duration = int_rand_range(base_props->m_explosion_prefix_min_duration, base_props->m_explosion_prefix_max_duration);
    }
    else
    {
        duration = (base_props->m_explosion_prefix_max_duration - base_props->m_explosion_prefix_min_duration) / 3 + base_props->m_explosion_prefix_min_duration;
    }
    m_level_obj_properties->m_death_timer.set(duration);
    return true;
}

// 0x00A380E0 (:1776)
// Stops the prefix effects when the death timer runs out (the death explosion follows in do_frame_pre_sim).
void general_mover::process_prefix_effect()
{
    level_object_inst_properties* props = m_level_obj_properties;
    if (!props->m_death_timer.valid() || !props->m_death_timer.elapsed())
    {
        return;
    }
    for (int i = 0; i < NUM_PREFIX_EFFECTS; i++)
    {
        const unsigned int effect_handle = m_level_obj_properties->m_prefix_effects[i];
        if (effect_handle == 0xFFFFFFFF)
        {
            continue;
        }
        object* effect = World.object_from_handle(effect_handle);
        if (effect != nullptr && effect->get_obj_type() == OT_EFFECT)
        {
            ((object_effect*)effect)->stop(object_effect::SS_STOP_EMITTING);
            m_level_obj_properties->m_prefix_effects[i] = 0xFFFFFFFF;
        }
    }
    props->m_death_timer.invalidate();
    gm_flags &= ~0x10u;
}

// 0x00A5EDC0 (:1803)
// The death explosion and its placement: at its attachment script point, else at the mover.
const explosion_info* general_mover::get_death_explosion_info_pos_and_orient(int i, vector& exp_pos, matrix* exp_orient) const
{
    if (i != 0)
    {
        return nullptr;
    }
    const explosion_container& container = m_level_obj_properties->m_base_properties->m_death_explosions[0];
    const explosion_info* e_info = container.exp_info;
    if (e_info == nullptr)
    {
        return nullptr;
    }
    const c_chunk_base* chunk = chunk_get_base_by_uid(chunk_uid);
    if (container.attachment_point_name != nullptr && chunk != nullptr)
    {
        const c_script_point* points = chunk->script_points.to_pointer();
        for (int j = 0; j < chunk->num_script_points; j++)
        {
            if (v_stricmp(points[j].name, container.attachment_point_name) == 0)
            {
                const c_script_point* point = &points[j];
                if (point == nullptr)
                {
                    break;
                }
                exp_pos = point->transform.m_translation;
                exp_pos.unrotate(orient);
                exp_pos += pos;
                if (exp_orient != nullptr)
                {
                    *exp_orient = point->transform.m_rotation * orient;
                }
                return e_info;
            }
        }
    }
    exp_pos = pos;
    if (exp_orient != nullptr)
    {
        *exp_orient = orient;
    }
    return e_info;
}

// 0x00A132D0 (:1850)
bool general_mover::explodes_on_death() const
{
    return m_level_obj_properties->m_flags.m_death_explosion;
}

// 0x00A132E0 (:1855)
// Queues the mover for its deferred render instance (general_mover_pre_stream_process_all).
void general_mover::set_render_created()
{
    mover_flags |= 0x80;
    general_mover_add_to_pre_stream_list(this); // inlined
}

// 0x00AB39D0 (:1862)
void general_mover_system_init()
{
    Pre_stream_render_task_gen_mover.m_callback = general_mover_pre_stream_process_all;
    game_render_pre_stream_render_task_register(&Pre_stream_render_task_gen_mover);
}

// 0x00A13340 (:1869)
void general_mover_level_init_post()
{
    multi_game_general_mover_reset_destroyed_count();
    if (Multiplayer)
    {
        create_level_general_mover_remote_objects();
    }
}

// 0x00A5EF90 (:1878)
void general_mover_level_shutdown()
{
    const int num = World.get_num_objects<general_mover>();
    for (int i = 0; i < num; i++)
    {
        general_mover* gm = World.get_object<general_mover>(i);
        gm->remote_obj_flags.client_destroy_permission = true;
        gm->remote_obj_flags.no_destroy_packet = true;
    }
}

// 0x00A8B0E0 (:1889)
void general_mover_do_frame_pre_sim()
{
    const int num = World.get_num_objects<general_mover>();
    for (int i = 0; i < num; i++)
    {
        World.get_object<general_mover>(i)->do_frame_pre_sim();
    }
}

// 0x00A13360 (:1901)
void general_mover_set_movable(general_mover* mover, bool movable)
{
    if (mover == nullptr || mover->havok_handle == 0xFFFFFFFF || mover->obj_flags.destroyed)
    {
        return;
    }
    if (havok_body_is_movable(mover->havok_handle))
    {
        if (!movable)
        {
            havok_body_set_movable(mover->havok_handle, false);
        }
    }
    else if (movable)
    {
        havok_body_set_movable(mover->havok_handle, true);
    }
}

// 0x00A38170 (:1921)
void validate_general_mover_destruction(const object* obj)
{
}

// ---- gm_prop_block
// 0x00A51800 (.h:172)
gm_prop_block::gm_prop_block(const vector& pos, const matrix& orient, const char* chunk_name, unsigned int chunk_obj_uid, const char* base_prop_name, const char* name, unsigned int collision_type, bool cast_shadows, bool cast_drop_shadows, int mover_type_bit_flags)
    : obj_mover_props(pos, orient, chunk_name, base_prop_name, cast_shadows, cast_drop_shadows, name, mover_type_bit_flags, OBJECT_IMPORTANCE_LEVEL_NONE, HUMAN_TEAM_NONE, 0)
{
    set(OM_PROP_CHUNK_OBJ_UID, chunk_obj_uid);
    set(GM_PROP_COLLISION_TYPE, collision_type);
    set(GM_PROP_FLAGS, 0u);
}

// 0x0071AED0 (.h:197)
// A movable piece of a destroyable shape (OM_PROP_FLAGS gets 0x4); its shape travels as a pointer.
gm_prop_block::gm_prop_block(const vector& pos, const matrix& orient, unsigned int chunk_uid, unsigned int chunk_obj_uid, c_chunk_base* chunk_base, const char* base_prop_name, rfg_subpiece* sp, rfg_destroyable_shape* shape, int move_type, unsigned int destruction_uid, bool cast_shadows, bool cast_drop_shadows, unsigned int original_object)
    : obj_mover_props(pos, orient, chunk_base->filename, base_prop_name, cast_shadows, cast_drop_shadows, nullptr, 0, OBJECT_IMPORTANCE_LEVEL_NONE, HUMAN_TEAM_NONE, 0)
{
    unsigned int flags = 0;
    obj_prop* prop;
    if (get_internal(prop, OM_PROP_FLAGS, 5) && prop->size == sizeof(unsigned int))
    {
        flags = *(unsigned int*)prop->data;
    }
    set(OM_PROP_FLAGS, flags | 0x4);
    set(OM_PROP_CHUNK_UID, chunk_uid);
    set(OM_PROP_CHUNK_OBJ_UID, chunk_obj_uid);
    set(GM_PROP_SHAPE_POINTER, shape);
    set(GM_PROP_DESTRUCT_UID, destruction_uid);
    set(GM_PROP_ORIGINAL_OBJECT, original_object);
    set(GM_PROP_MOVE_TYPE, move_type);
    set(GM_PROP_SUBPIECE_INDEX, (int)shape->get_subpiece_index(sp)); // inlined
    set(GM_PROP_FLAGS, 0u);
}

// 0x00712980
gm_prop_block::~gm_prop_block()
{
}
