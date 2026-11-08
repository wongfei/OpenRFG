// Original: volition/rfg/code/destruction/rfg_shapes.cpp
//
// Destructible shapes (decompiled; header comment in rfg_shapes.h). Also defines the inline functions of
// rfg_shapes.inl and rfg_shapes.h. Compiled in rfg_destruction_unity.cpp: the physical material table
// (RFG_physical_materials) is a file static of rfg_materials.cpp.
//
// Subpiece lists: pieces are linked by 16-bit indices into objects[] (next/prev for the shape's child list
// child_obj_list and the transfer list transfer_child_obj_list, shard_next/shard_prev for the shardify queue), as
// circular lists whose head is the first index (0xFFFF = empty). A piece belongs to the shape when its
// my_owner_gm_handle points at it; with flag 0x100000 the field holds the handle of a general_mover instead (a piece
// that broke off and became its own object).
// Subpiece flags used here: 0x1 world anchor, 0x4 faded, 0x10 in the shardify queue, 0x80 deleted (sorted to the
// front of the shardify queue), 0x800 shardify requested, 0x2000/0x4000 fade state, 0x100000 owner is a general
// mover, 0x20000000 in the child list, 0x40000000 in the transfer list. Shape flags: 0x8 destruction mode, 0x10 has
// a world anchor, 0x400 bounds dirty, 0x800 rebuild the rbb tree, 0x1000 has a piece with base flag 0x40, 0x2000
// check base flag 0x40, 0x4000 a piece faded, 0x8000 Havok shape data dirty.
//
// Partial: save (cruncher builds only) prints its error as the original does.
//
// Notes (decompiled behaviour, kept as is):
// - rfg_subpiece::restore_link swaps when index is greater than num_links, the opposite test of remove_link, so a
//   link restored from below num_links is not moved back into place.

#include "volition/rfg/code/destruction/rfg_shapes.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/matrix/hktransformf.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/geometry/aabb/hkaabb.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/convextransform/hkpconvextransformshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/convextranslate/hkpconvextranslateshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/query/hkpshaperaycastoutput.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/geometry/sphere/hksphere.h"
#include "volition/rfg/code/destruction/rfg_dynamics.h"
#include "volition/rfg/code/destruction/rfg_mover.h"
#include "volition/rfg/code/destruction/rfg_rbb_tree.h"
#include "volition/rfg/code/destruction/rfg_stress.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/havok/havok_cm_manager.h"
#include "volition/rfg/code/havok/havok_convert.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/objects/movers/general_mover.h"
#include "volition/rfg/code/rfg.h"
#include "volition/vlib/code/math/bbox.h"
#include "volition/vlib/code/math/crc.h"
#include "volition/vlib/code/math/fp_aabb.h"
#include "volition/vlib/code/math/ix.h"
#include "volition/vlib/code/math/plane.h"
#include "volition/vlib/code/math/polyclip.h"
#include "volition/vlib/code/memory/page_mempool.h"
#include "volition/vlib/code/memory/split_memmgr.h"
#include "volition/vlib/code/os/cs_wrapper.h"
#include "volition/vlib/code/os/frametime.h"

#include "keen/base/trace.hpp"

#include <math.h>
#include <stdlib.h>
#include <string.h>

data_hash_table<unsigned int,rfg_destroyable_shape*,&hash_uint,9216,unsigned short> Destroyable_uid_hash(false); // 0x01EF8310
float Use_parent_rbb_pct = 0.75f; // 0x0165047C
shard_impulse_info Shard_impulse_info_pool[500]; // 0x01F14470
int Num_shard_impulse_info; // 0x01EF82DC

// ---- file-static globals
static int Recalc_rbb_count = 250; // 0x01650470
static float Recalc_rbb_pct = 0.5f; // 0x01650474
static int Use_parent_rbb_count = 125; // 0x01650478
static int Sort_obj_index = -1; // 0x01650480
static unsigned char RFG_shape_rbb_buf[294400]; // 0x01E38408
static rfg_destroyable_shape* Sort_shape; // 0x01EF82E0
static cs_wrapper Rbb_tree_wrapper(0); // 0x01F13318
static page_mempool RFG_shape_rbb_mempool; // 0x01F133A8

static const unsigned int SUBPIECE_GM_OWNER = 0x100000; // my_owner_gm_handle holds a general_mover handle

// The shape a piece belongs to (rfg_subpiece::get_destroyable, inlined everywhere).
static rfg_destroyable_shape* owner_shape(const rfg_subpiece* sp)
{
    return sp->get_destroyable();
}

// Index of a piece in its shape's objects[].
static unsigned short piece_index(const rfg_destroyable_shape* shape, const rfg_subpiece* sp)
{
    return (unsigned short)(sp - shape->get_object_buffer());
}

// 0x0071FCE0 (:199)
// LTCG: old_shape in ESI, pos in ECX, shape_size in EDX. Wraps a piece's convex shape in its transform inside the
// destroyable (a translate shape when the orientation is the identity); with a buffer, the new shape pointer is also
// stored there. Every new shape counts 16 bytes of the transform buffer.
static hkpShape* create_transform_shape(hkpShape* old_shape, const vector& pos, const matrix& orient, unsigned char* buffer, int& shape_size)
{
    if (old_shape == nullptr)
    {
        return nullptr;
    }
    hkpShape* shape;
    if (orient.fvec * IDENTITY_MATRIX.fvec < 0.999f || orient.uvec * IDENTITY_MATRIX.uvec < 0.999f || orient.rvec * IDENTITY_MATRIX.rvec < 0.999f)
    {
        hkTransformf transform;
        game_to_havok_transform(transform, pos, orient);
        shape = new hkpConvexTransformShape((const hkpConvexShape*)old_shape, transform, hkpShapeContainer::REFERENCE_POLICY_INCREMENT);
    }
    else
    {
        hkVector4f translation;
        game_to_havok_vector(translation, pos);
        shape = new hkpConvexTranslateShape((const hkpConvexShape*)old_shape, translation, hkpShapeContainer::REFERENCE_POLICY_INCREMENT);
    }
    if (buffer != nullptr)
    {
        *(hkpShape**)buffer = shape;
    }
    shape_size = 0x10;
    return shape;
}

// ---- rfg_dlod
// 0x00736420 (:517)
// The shape that owns all the dlod's pieces, recomputed when the dlod is dirty (flag 0x4); nullptr when the pieces
// belong to different shapes.
rfg_destroyable_shape* rfg_dlod::get_owner()
{
    if ((flags & 4) != 0)
    {
        bool found = false;
        owner.set_offset(-1);
        rfg_subpiece* pieces = subpieces.to_pointer();
        for (int i = 0; i < max_pieces; i++)
        {
            rfg_destroyable_shape* piece_owner = owner_shape(&pieces[i]);
            if (found)
            {
                if ((rfg_destroyable_shape*)owner != piece_owner)
                {
                    owner.set_offset(-1);
                    break;
                }
            }
            else
            {
                if (piece_owner != nullptr)
                {
                    owner.from_pointer(piece_owner);
                }
                else
                {
                    owner.set_offset(-1);
                }
                found = true;
            }
        }
        if (!found)
        {
            owner.set_offset(-1);
        }
        flags &= ~4;
    }
    return (rfg_destroyable_shape*)owner;
}

// 0x007276E0 (:551)
// Appends the indices of the dlod's pieces that still belong to a shape.
void rfg_dlod::get_subpiece_indices(int& num_indices, short* indices)
{
    rfg_subpiece* pieces = subpieces.to_pointer();
    for (int i = 0; i < max_pieces; i++)
    {
        rfg_destroyable_shape* shape = owner_shape(&pieces[i]);
        if (shape != nullptr)
        {
            indices[num_indices++] = (short)(&pieces[i] - shape->get_object_buffer());
        }
    }
}

// 0x00727770 (:564)
// While the dlod is marked for recheck (0x10), it renders only when none of its pieces is fading (0x30000).
bool rfg_dlod::should_render()
{
    const unsigned char f = flags;
    if ((f & 0x10) == 0)
    {
        return (f >> 3 & 1) != 0;
    }
    flags = f & 0xE7;
    rfg_subpiece* pieces = subpieces.to_pointer();
    for (int i = 0; i < max_pieces; i++)
    {
        if ((pieces[i].flags & 0x30000) != 0)
        {
            return false;
        }
    }
    flags = (f & 0xE7) | 8;
    return true;
}

// ---- rfg_base_destroyable_shape
// 0x00736510 (:592)
rfg_base_destroyable_shape::rfg_base_destroyable_shape()
{
    inst_data.set_offset(-1);
    transform_buffer.set_offset(-1);
    base_objects.from_pointer(nullptr);
    base_extra_data.from_pointer(nullptr);
    num_objects = 0;
    base_links.from_pointer(nullptr);
    num_links = 0;
    base_dlods.from_pointer(nullptr);
    num_dlods = 0;
    aabb_tree.from_pointer(nullptr);
}

// 0x00717610 (:607)
void rfg_base_destroyable_shape::save(cfile* f)
{
    keen::debug::nativePrintArgs("You should never be calling rfg_base_destroyable_shape::save() except in a cruncher build!");
}

// 0x00736560 (:714)
// Resolves each piece's collision model (the default model when it has none) and wraps it in the piece's transform
// (spheres, boxes and convex vertices; translate/transform shapes are used as they are).
void rfg_base_destroyable_shape::init_subpiece_shapes()
{
    rfg_subpiece_base* objects = base_objects.to_pointer();
    rfg_subpiece_base_extra_data* extra = base_extra_data.to_pointer();
    for (int i = 0; i < num_objects; i++)
    {
        const hkpShape* model = havok_cm_manager_get(extra[i].collision_model);
        extra[i].shape.from_pointer((hkpShape*)model);
        if ((hkpShape*)extra[i].shape == nullptr)
        {
            extra[i].shape.from_pointer((hkpShape*)havok_cm_manager_get(0));
        }
        matrix orient;
        get_subpiece_orient_local(&objects[i], orient);
        hkpShape* shape = (hkpShape*)extra[i].shape;
        switch (shape->m_type.m_storage)
        {
            case hkcdShapeType::SPHERE:
            case hkcdShapeType::BOX:
            case hkcdShapeType::CONVEX_VERTICES:
            {
                // The new shape's pointer is written at the transform buffer position, which then advances.
                int shape_size;
                hkpShape* transformed = create_transform_shape(shape, objects[i].pos, orient, transform_buffer.to_pointer(), shape_size);
                extra[i].shape.from_pointer(transformed);
                transform_buffer.set_offset(transform_buffer.get_offset() + shape_size);
                break;
            }
            case hkcdShapeType::CONVEX_TRANSLATE:
            case hkcdShapeType::CONVEX_TRANSFORM:
            case hkcdShapeType::TRANSFORM:
                break;
            default:
                keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
                break;
        }
    }
}

// 0x007277C0 (:762)
// Link strength from the two pieces' materials and the contact area; links between two indestructible pieces get
// flag 2.
void rfg_base_destroyable_shape::reset_link_strengths()
{
    rfg_subpiece_base* objects = base_objects.to_pointer();
    rfg_link_base* links = base_links.to_pointer();
    for (int i = 0; i < num_links; i++)
    {
        rfg_subpiece_base* sp_0 = &objects[links[i].obj[0]];
        rfg_subpiece_base* sp_1 = &objects[links[i].obj[1]];
        links[i].yield_max = rfg_shape_calc_link_strength(sp_0->physical_material_index, sp_0, sp_1->physical_material_index, sp_1, links[i].area);
        if ((sp_0->flags & 0x20) != 0 && (sp_1->flags & 0x20) != 0)
        {
            links[i].flags |= 2;
        }
    }
}

// 0x00727860 (:785)
// Releases the pieces' shapes; the data stays in the chunk.
void rfg_base_destroyable_shape::delete_me()
{
    rfg_subpiece_base_extra_data* extra = base_extra_data.to_pointer();
    if (extra != nullptr)
    {
        for (int i = 0; i < num_objects; i++)
        {
            hkpShape* shape = (hkpShape*)extra[i].shape;
            if (shape != nullptr)
            {
                shape->removeReference();
            }
        }
    }
    base_extra_data.set_offset(0);
    base_objects.set_offset(0);
    base_links.set_offset(0);
    base_dlods.set_offset(0);
    aabb_tree.set_offset(0);
    base_structural_mass = 0.0f;
}

// ---- rfg_subpiece
// 0x00736710 (:859)
// An rfg_mover owns its pieces through its shape; a general_mover through its handle.
void rfg_subpiece::set_owner(object_mover* om)
{
    if (om != nullptr)
    {
        if (om->get_sub_type() == OT_SUB_MOVER_RFG)
        {
            rfg_mover* rm = om->get_obj_type() == OT_MOVER ? (rfg_mover*)om : nullptr;
            rfg_destroyable_shape* shape = rm->rdd.is_loaded() ? rm->rdd.get_data()->shape : nullptr;
            if (shape == nullptr)
            {
                my_owner_gm_handle.set_offset(-1);
            }
            else
            {
                my_owner_gm_handle.from_pointer(shape);
            }
            flags &= ~SUBPIECE_GM_OWNER;
            return;
        }
        if (om->get_sub_type() == OT_SUB_MOVER_GENERAL)
        {
            flags |= SUBPIECE_GM_OWNER;
            my_owner_gm_handle.set_offset((int)om->get_handle());
            return;
        }
    }
    my_owner_gm_handle.set_offset(-1);
    flags &= ~SUBPIECE_GM_OWNER;
}

// 0x007278D0 (:900)
// Orders a piece's links by the link base index of the other piece... by the extra data hash of the other piece.
static int subpiece_sort(const void* elem1, const void* elem2)
{
    rfg_link_base* links = Sort_shape->parent->base_links.to_pointer();
    const rfg_link_base& l1 = links[*(const unsigned short*)elem1];
    const rfg_link_base& l2 = links[*(const unsigned short*)elem2];
    const short other1 = l1.obj[0] == Sort_obj_index ? l1.obj[1] : l1.obj[0];
    const short other2 = l2.obj[0] == Sort_obj_index ? l2.obj[1] : l2.obj[0];
    rfg_subpiece_base_extra_data* extra = Sort_shape->parent->base_extra_data.to_pointer();
    return (int)(extra[other1].h - extra[other2].h);
}

// 0x00727950 (:930)
void rfg_subpiece::sort_links()
{
    rfg_destroyable_shape* shape = get_destroyable();
    if (shape != nullptr)
    {
        Sort_obj_index = (int)(this - shape->get_object_buffer());
        Sort_shape = shape;
        qsort(links.to_pointer(), num_links, 2, subpiece_sort);
        Sort_shape = nullptr;
        Sort_obj_index = -1;
    }
}

// 0x0071FFF0 (:953)
object_mover* rfg_subpiece::get_owner() const
{
    if ((flags & SUBPIECE_GM_OWNER) == 0)
    {
        rfg_destroyable_shape* shape = (rfg_destroyable_shape*)my_owner_gm_handle;
        return shape != nullptr ? (object_mover*)shape->get_mover() : nullptr;
    }
    const unsigned int handle = (unsigned int)my_owner_gm_handle.get_offset();
    if (handle != 0xFFFFFFFF)
    {
        object* objp = World.object_from_handle(handle);
        if (objp != nullptr && objp->get_obj_type() == OT_MOVER && objp->get_sub_type() == OT_SUB_MOVER_GENERAL)
        {
            return (object_mover*)objp;
        }
    }
    return nullptr;
}

// 0x00736790 (:967)
void rfg_subpiece::set_destroyable(rfg_destroyable_shape* shape)
{
    if (shape == nullptr)
    {
        my_owner_gm_handle.set_offset(-1);
    }
    else
    {
        my_owner_gm_handle.from_pointer(shape);
    }
    flags &= ~SUBPIECE_GM_OWNER;
}

// 0x0071AC40 (rfg_shapes.inl:86)
rfg_destroyable_shape* rfg_subpiece::get_destroyable() const
{
    if ((flags & SUBPIECE_GM_OWNER) != 0)
    {
        return nullptr;
    }
    const int offset = my_owner_gm_handle.get_offset();
    if (offset == -1 || offset == 0)
    {
        return nullptr;
    }
    return (rfg_destroyable_shape*)((char*)&my_owner_gm_handle + offset);
}

// 0x00710A70 (rfg_shapes.inl:95)
bool rfg_subpiece::is_modified()
{
    return modified_framecount == (unsigned char)Framecount;
}

// 0x00710A80 (rfg_shapes.inl:101)
void rfg_subpiece::set_modified(bool modified)
{
    modified_framecount = modified ? (unsigned char)Framecount : (unsigned char)(Framecount - 1);
}

// 0x007235B0 (rfg_shapes.inl:119)
// Moves link index past the end of the active links (swap with the last active one).
void rfg_subpiece::remove_link(int index)
{
    --num_links;
    if (index < num_links)
    {
        unsigned short* l = links.to_pointer();
        const unsigned short t = l[index];
        l[index] = l[num_links];
        l[num_links] = t;
    }
}

// 0x007235F0 (rfg_shapes.inl:134)
void rfg_subpiece::restore_link(int index)
{
    if (num_links < index)
    {
        unsigned short* l = links.to_pointer();
        const unsigned short t = l[index];
        l[index] = l[num_links];
        l[num_links] = t;
    }
    ++num_links;
}

// 0x00710AA0 (rfg_shapes.inl:151)
// Accumulates an impulse for the shard this piece becomes (500 records per frame).
void rfg_subpiece::add_impulse(const vector& impulse, const vector& impulse_pos)
{
    shard_impulse_info* info = impulse_info_index != 0xFFFF ? &Shard_impulse_info_pool[impulse_info_index] : nullptr;
    if (info == nullptr)
    {
        if ((unsigned int)Num_shard_impulse_info + 1 > 499)
        {
            return;
        }
        const int index = Num_shard_impulse_info++;
        impulse_info_index = (unsigned short)index;
        info = &Shard_impulse_info_pool[index & 0xFFFF];
        info->impulse_dir.zero();
    }
    info->impulse_dir.x += impulse.x;
    info->impulse_dir.y += impulse.y;
    info->impulse_dir.z += impulse.z;
    info->impulse_pos = impulse_pos;
}

// ---- rfg_dynamic_link
// 0x00717620 (:975)
void rfg_dynamic_link::reset()
{
    shape_key[0] = 0xFFFF;
    shape_key[1] = 0xFFFF;
    anchor_depth[0] = 0xFFFF;
    anchor_depth[1] = 0xFFFF;
    mover_handle[0] = 0xFFFFFFFF;
    mover_handle[1] = 0xFFFFFFFF;
    yield_limit = -1;
}

// 0x00717640 (:988)
void rfg_dynamic_link::copy(const rfg_dynamic_link* rdl)
{
    keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
}

// 0x00717660 (:1006)
// A link is saved by the mover with the higher handle, or by the only side that has a piece.
bool rfg_dynamic_link::should_save(unsigned int handle) const
{
    int self;
    int other;
    if (mover_handle[0] == handle)
    {
        self = 0;
        other = 1;
    }
    else if (mover_handle[1] == handle)
    {
        self = 1;
        other = 0;
    }
    else
    {
        return false;
    }
    if (shape_key[self] == 0xFFFF)
    {
        return false;
    }
    if (shape_key[other] == 0xFFFF)
    {
        return true;
    }
    return mover_handle[other] < mover_handle[self];
}

// ---- rfg_destroyable_shape
// 0x0071AC20 (.h:485)
void* rfg_destroyable_shape::operator new(unsigned int nbytes)
{
    return hkContainerHeapAllocator::s_alloc.blockAlloc((int)nbytes);
}

// 0x00736A90 (.h:485)
// The Havok object header keeps the allocation size (0xFFFF = use nbytes).
void rfg_destroyable_shape::operator delete(void* p, unsigned int nbytes)
{
    if (*(unsigned short*)((char*)p + 6) != 0xFFFF)
    {
        nbytes = *(unsigned short*)((char*)p + 6);
    }
    hkContainerHeapAllocator::s_alloc.blockFree(p, (int)nbytes);
}

// 0x007367C0 (:1041)
// A shape with the uid of an existing one becomes its sibling and shares its piece buffer; otherwise it copies the
// base's template buffer (pieces, links, dlods) into a new one from Level_memmgr and turns its buffer-relative
// offsets into pointers.
rfg_destroyable_shape::rfg_destroyable_shape(rfg_base_destroyable_shape* base, unsigned int _chunk_uid, const vector& pos, const matrix& orient, unsigned int sibling_uid)
    : hkpBvTreeShape(hkcdShapeType::BV_TREE, hkpBvTreeShape::BVTREE_USER) // inline (0x0071AB60)
{
    destruction_mode_timer.invalidate();
    parent = base;
    num_existing_child_shapes = 0;
    child_obj_list = 0xFFFF;
    transfer_child_obj_list = 0xFFFF;
    calc_rbb_child_count = base->num_objects;
    anchor_depth = 0xFFFF;
    structural_mass = 0.0f;
    shardify_q = 0xFFFF;
    base_structural_mass = base->base_structural_mass;
    mass = 0.0f;
    aabb_tree = base->aabb_tree.to_pointer();
    override_tree = nullptr;
    center_of_mass_accum = ZERO_VECTOR;
    flags = 0x2000;
    mover_p = nullptr;

    rfg_destroyable_shape* sibling = nullptr;
    Destroyable_uid_hash.get(sibling_uid, sibling);
    shape_uid = sibling_uid;
    if (sibling != nullptr)
    {
        objects = sibling->objects;
        links = sibling->links;
        dlods = sibling->dlods;
        sib_next = sibling;
        sib_prev = sibling->sib_prev;
        sibling->sib_prev->sib_next = this;
        sibling->sib_prev = this;
        m_userData = 2;
        return;
    }

    rfg_destroyable_shape* self = this;
    Destroyable_uid_hash.insert_key_data(sibling_uid, self);
    sib_next = this;
    sib_prev = this;
    unsigned char* buffer = allocate_buffer();
    if (buffer == nullptr)
    {
        objects = nullptr;
        links = nullptr;
        dlods = nullptr;
        return;
    }
    rfg_destroyable_instance_data* inst = parent->inst_data.to_pointer();
    memcpy(buffer, inst->buffer.to_pointer(), inst->data_size);
    objects = (rfg_subpiece*)(buffer + inst->objects_offset);
    links = (rfg_link*)(buffer + inst->links_offset);
    dlods = (rfg_dlod*)(buffer + inst->dlods_offset);
    for (int i = 0; i < parent->num_objects; i++)
    {
        // the template stores link list offsets from the buffer start
        objects[i].links.from_pointer((unsigned short*)(buffer + objects[i].links.get_offset()));
    }
    rfg_dlod_base* base_dlods = parent->base_dlods.to_pointer();
    for (int i = 0; i < parent->num_dlods; i++)
    {
        dlods[i].subpieces.from_pointer(&objects[base_dlods[i].first_piece]);
    }
    m_userData = 2;
}

// 0x007279C0 (:1140)
unsigned char* rfg_destroyable_shape::allocate_buffer()
{
    if (parent == nullptr || parent->inst_data.to_pointer() == nullptr)
    {
        return nullptr;
    }
    return (unsigned char*)Level_memmgr.alloc(parent->inst_data.to_pointer()->data_size, "rfg_destroyable_shape::allocate_buffer", 4);
}

// 0x007176B0 (:1157)
void rfg_destroyable_shape::deallocate_buffer()
{
    if (objects != nullptr)
    {
        Level_memmgr.free(objects);
    }
}

// 0x00737CA0 (rfg_shapes.inl:178)
void rfg_destroyable_shape::set_shape_state(int index, bool state)
{
    set_shape_state(&objects[index], state);
}

// 0x00736B20 (:1175)
// Adds a free piece to the shape (state true) or takes one of its pieces out (state false), keeping mass, center of
// mass, structural mass and the dlod piece counts up to date.
void rfg_destroyable_shape::set_shape_state(rfg_subpiece* sp, bool state)
{
    rfg_destroyable_shape* current = owner_shape(sp);
    const int index = (int)(sp - objects);
    rfg_subpiece_base* sp_base = &parent->base_objects.to_pointer()[index];
    flags |= 0x8000;
    if (current != this)
    {
        if (state && current == nullptr && sp->get_owner() == nullptr && sp->next == 0xFFFF && sp->prev == 0xFFFF)
        {
            const unsigned short i = (unsigned short)index;
            if (child_obj_list == 0xFFFF)
            {
                sp->next = i;
                sp->prev = i;
                child_obj_list = i;
            }
            else
            {
                sp->prev = objects[child_obj_list].prev;
                sp->next = child_obj_list;
                objects[objects[child_obj_list].prev].next = i;
                objects[child_obj_list].prev = i;
            }
            sp->flags |= 0x20000000;
            sp->set_destroyable(this);
            sp->eval_info_index = 0xFFFF;
            num_existing_child_shapes++;
            if ((sp_base->flags & 0x28) == 0)
            {
                structural_mass = sp_base->mass + structural_mass;
            }
            center_of_mass_accum.x += sp_base->center_of_mass.x * sp_base->mass;
            center_of_mass_accum.y += sp_base->center_of_mass.y * sp_base->mass;
            center_of_mass_accum.z += sp_base->center_of_mass.z * sp_base->mass;
            mass += sp_base->mass;
            rfg_dlod& dlod = dlods[sp_base->dlod_key & 0xFFFF];
            dlod.flags |= 4;
            dlod.active_pieces++;
        }
        return;
    }
    if (state)
    {
        return;
    }
    if ((sp->flags & 0x10) != 0)
    {
        remove_subpiece_from_shardify_q(sp);
    }
    num_existing_child_shapes--;
    if ((sp_base->flags & 0x28) == 0)
    {
        structural_mass -= sp_base->mass;
    }
    const unsigned short i = (unsigned short)index;
    if (child_obj_list == i && sp->next == i)
    {
        sp->next = 0xFFFF;
        sp->prev = 0xFFFF;
        child_obj_list = 0xFFFF;
    }
    else
    {
        if (child_obj_list == i)
        {
            child_obj_list = sp->next;
        }
        objects[sp->prev].next = sp->next;
        objects[sp->next].prev = sp->prev;
        sp->next = 0xFFFF;
        sp->prev = 0xFFFF;
    }
    sp->flags &= ~0x20000000;
    sp->set_destroyable(nullptr);
    center_of_mass_accum.x -= sp_base->center_of_mass.x * sp_base->mass;
    center_of_mass_accum.y -= sp_base->center_of_mass.y * sp_base->mass;
    center_of_mass_accum.z -= sp_base->center_of_mass.z * sp_base->mass;
    mass -= sp_base->mass;
    rfg_dlod& dlod = dlods[sp_base->dlod_key & 0xFFFF];
    dlod.flags |= 5;
    dlod.active_pieces--;
}

// 0x0071AC60 (rfg_shapes.inl:186)
bool rfg_destroyable_shape::get_shape_state(const rfg_subpiece* sp) const
{
    return owner_shape(sp) == this;
}

// 0x0071ACA0 (rfg_shapes.inl:207)
bool rfg_destroyable_shape::get_shape_state(int index) const
{
    return owner_shape(&objects[index]) == this;
}

// 0x00710B80 (rfg_shapes.inl:213)
unsigned int rfg_destroyable_shape::get_subpiece_index(const rfg_subpiece* sp) const
{
    return (unsigned int)(sp - objects);
}

// 0x00710BA0 (rfg_shapes.inl:234)
rfg_subpiece* rfg_destroyable_shape::get_subpiece(unsigned int key) const
{
    return &objects[key];
}

// 0x00723630 (rfg_shapes.inl:254)
rfg_subpiece_base* rfg_destroyable_shape::get_subpiece_base(unsigned int key) const
{
    return &parent->base_objects.to_pointer()[key];
}

// 0x00723650 (rfg_shapes.inl:282)
rfg_subpiece_base* rfg_destroyable_shape::get_subpiece_base(const rfg_subpiece* sp) const
{
    return &parent->base_objects.to_pointer()[sp - objects];
}

// 0x00723680 (rfg_shapes.inl:287)
rfg_subpiece_base_extra_data* rfg_destroyable_shape::get_subpiece_base_extra_data(unsigned int key) const
{
    return &parent->base_extra_data.to_pointer()[key];
}

// 0x007236A0 (rfg_shapes.inl:314)
rfg_subpiece_base_extra_data* rfg_destroyable_shape::get_subpiece_base_extra_data(const rfg_subpiece* sp) const
{
    return &parent->base_extra_data.to_pointer()[sp - objects];
}

// 0x00710940 (.h:563)
rfg_mover* rfg_destroyable_shape::get_mover() const
{
    return mover_p;
}

// 0x00710950 (.h:564)
void rfg_destroyable_shape::set_mover(rfg_mover* _mover_p)
{
    mover_p = _mover_p;
}

// 0x00710BB0 (rfg_shapes.inl:320)
const rfg_rbb_node* rfg_destroyable_shape::get_rbb_tree() const
{
    return aabb_tree;
}

// 0x00727CA0 (:1581)
// World position and orientation of a piece: its dlod's orientation and its position, in the mover's frame.
void rfg_destroyable_shape::get_subpiece_pos_orient_uncached(const rfg_subpiece* sp, vector& pos, matrix& orient)
{
    rfg_mover* rm = mover_p;
    if (rm == nullptr)
    {
        pos.zero();
        orient = IDENTITY_MATRIX;
        return;
    }
    rfg_subpiece_base* sp_base = &parent->base_objects.to_pointer()[sp - objects];
    orient = parent->base_dlods.to_pointer()[sp_base->dlod_key & 0xFFFF].orient;
    const vector offset = sp_base->pos.copy_unrotate(rm->orient);
    pos.x = rm->pos.x + offset.x;
    pos.y = rm->pos.y + offset.y;
    pos.z = rm->pos.z + offset.z;
    orient = orient * rm->orient;
}

// 0x00710BC0 (rfg_shapes.inl:391)
int rfg_destroyable_shape::get_dlod_index(const rfg_dlod* dlod) const
{
    return (int)(dlod - dlods);
}

// 0x00710BE0 (rfg_shapes.inl:400)
rfg_dlod* rfg_destroyable_shape::get_dlod(int index) const
{
    return &dlods[index];
}

// 0x007236D0 (rfg_shapes.inl:408)
rfg_dlod_base* rfg_destroyable_shape::get_dlod_base(int index) const
{
    return &parent->base_dlods.to_pointer()[index];
}

// 0x007236F0 (rfg_shapes.inl:416)
rfg_dlod_base* rfg_destroyable_shape::get_dlod_base(const rfg_dlod* dlod) const
{
    return &parent->base_dlods.to_pointer()[dlod - dlods];
}

// 0x00727E40 (:1601)
void rfg_destroyable_shape::get_dlod_pos_orient_uncached(const rfg_dlod* dlod, vector& pos, matrix& orient)
{
    rfg_mover* rm = mover_p;
    if (rm == nullptr)
    {
        pos.zero();
        orient = IDENTITY_MATRIX;
        return;
    }
    rfg_dlod_base* dlod_base = &parent->base_dlods.to_pointer()[dlod - dlods];
    const vector offset = dlod_base->pos.copy_unrotate(rm->orient);
    pos.x = offset.x + rm->pos.x;
    pos.y = offset.y + rm->pos.y;
    pos.z = offset.z + rm->pos.z;
    orient = dlod_base->orient * rm->orient;
}

// 0x00710BF0 (rfg_shapes.inl:421)
int rfg_destroyable_shape::get_link_index(const rfg_link* link) const
{
    return (int)(link - links);
}

// 0x00723770 (rfg_shapes.inl:451)
rfg_link* rfg_destroyable_shape::get_link(const rfg_subpiece* sp, int index) const
{
    return &links[sp->links.to_pointer()[index]];
}

// 0x00710C00 (rfg_shapes.inl:430)
rfg_link* rfg_destroyable_shape::get_link(int index) const
{
    return &links[index];
}

// 0x00723790 (rfg_shapes.inl:459)
rfg_link_base* rfg_destroyable_shape::get_link_base(const rfg_subpiece* sp, int index) const
{
    return &parent->base_links.to_pointer()[sp->links.to_pointer()[index]];
}

// 0x00723750 (rfg_shapes.inl:446)
rfg_link_base* rfg_destroyable_shape::get_link_base(const rfg_link* l) const
{
    return &parent->base_links.to_pointer()[l - links];
}

// 0x00723730 (rfg_shapes.inl:438)
rfg_link_base* rfg_destroyable_shape::get_link_base(int index) const
{
    return &parent->base_links.to_pointer()[index];
}

// 0x0071ACE0 (rfg_shapes.inl:469)
// The first piece of the child list when the shape has no world anchor (flags 0x30) and that piece has links.
rfg_subpiece* rfg_destroyable_shape::get_shardify_anchor() const
{
    rfg_subpiece* sp = child_obj_list != 0xFFFF ? &objects[child_obj_list] : nullptr;
    if ((flags & 0x30) != 0 || sp == nullptr || sp->num_links == 0)
    {
        return nullptr;
    }
    return sp;
}

// 0x00710960 (.h:590)
bool rfg_destroyable_shape::is_valid() const
{
    return objects != nullptr;
}

// 0x0072FBF0 (:1269)
// Pins a piece to the world: it no longer counts as structural mass, and its anchor strength is the material's link
// strength times its footprint (x by z, 8.8 fixed point); indestructible pieces never break loose.
void rfg_destroyable_shape::create_world_anchor(rfg_mover* rm, rfg_subpiece* sp)
{
    if (owner_shape(sp) != this)
    {
        return;
    }
    rfg_dyn_link_update_depth(rm, this, 0);
    rfg_subpiece_base* sp_base = &parent->base_objects.to_pointer()[sp - objects];
    if ((sp_base->flags & 0x28) == 0)
    {
        structural_mass -= sp_base->mass;
        base_structural_mass -= sp_base->mass;
    }
    sp->flags |= 1;
    flags |= 0x10;
    rm->mover_flags |= 0x4000000;
    if ((sp_base->flags & 0x20) != 0)
    {
        sp->anchor_yield = 0x7FFFFFFF;
        return;
    }
    const rfg_physical_material* material = Rfg_physical_material_default;
    if (sp_base->physical_material_index != 0xFF && sp_base->physical_material_index < Num_rfg_physical_materials)
    {
        material = &RFG_physical_materials[sp_base->physical_material_index];
    }
    const long long strength = (long long)(int)(material->link_strength << 8) *
                               (long long)(int)((sp_base->prel_bmax.z - sp_base->prel_bmin.z) * (sp_base->prel_bmax.x - sp_base->prel_bmin.x) * 256.0f);
    sp->anchor_yield = (int)(strength >> 16);
}

// 0x0072FD40 (:1310)
// Breaks every link of the piece (the neighbours are queued for shardify) and queues the piece itself as deleted.
void rfg_destroyable_shape::delete_subpiece(rfg_subpiece* sp)
{
    if (sp == nullptr || owner_shape(sp) != this)
    {
        return;
    }
    rfg_link_base* base_links = parent->base_links.to_pointer();
    const short self = (short)(sp - objects);
    while (sp->num_links != 0)
    {
        rfg_link* l = &links[sp->links.to_pointer()[0]];
        const rfg_link_base& lb = base_links[l - links];
        const short other = lb.obj[0] == self ? lb.obj[1] : lb.obj[0];
        add_subpiece_to_shardify_q(&objects[other]);
        remove_link(l);
    }
    sp->flags |= 0x80;
    add_subpiece_to_shardify_q(sp);
}

// 0x0072FE10 (:1389)
// Starts fading a piece out (fade_in false) or finishes taking it over again (fade_in true: the object that now owns
// it, a general_mover shard or another rfg_mover's shape, gives it up). The piece goes to the transfer list and the
// mover's render is flagged changed (set_render_changed, vtbl+0xA0). Returns whether the piece was added to the
// transfer list.
bool rfg_destroyable_shape::fade_subpiece(rfg_subpiece* sp, bool fade_in)
{
    bool added = false;
    if (fade_in)
    {
        object_mover* owner = sp->get_owner();
        if (owner != nullptr)
        {
            if (owner->get_sub_type() == OT_SUB_MOVER_RFG)
            {
                rfg_destroyable_shape* shape = owner_shape(sp);
                if (shape != this)
                {
                    shape->delete_subpiece(sp);
                }
            }
            else if (owner->get_sub_type() == OT_SUB_MOVER_GENERAL)
            {
                general_mover* gm = (general_mover*)owner;
                if (gm->get_obj_type() == OT_MOVER && gm->shape_uid == shape_uid && gm->subpiece_index == (unsigned int)(sp - objects))
                {
                    if (Multiplayer && !multi_is_server())
                    {
                        gm->remote_obj_flags.client_destroy_permission = 1;
                    }
                    World.destroy_object(gm, false);
                }
            }
            else
            {
                keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
            }
        }
        if ((sp->flags & 0x2000) == 0)
        {
            sp->flags |= 4;
            flags |= 0x4000;
        }
        if ((sp->flags & 4) == 0 && sp->next == 0xFFFF && sp->prev == 0xFFFF)
        {
            if (mover_p == nullptr)
            {
                return false;
            }
            mover_p->set_render_changed();
            mover_p->set_render_flags(8);
            const unsigned short i = (unsigned short)(sp - objects);
            if (transfer_child_obj_list == 0xFFFF)
            {
                sp->next = i;
                sp->prev = i;
                transfer_child_obj_list = i;
            }
            else
            {
                sp->prev = objects[transfer_child_obj_list].prev;
                sp->next = transfer_child_obj_list;
                objects[objects[transfer_child_obj_list].prev].next = i;
                objects[transfer_child_obj_list].prev = i;
            }
            sp->flags |= 0x40000000;
            added = true;
        }
        sp->flags |= fade_in ? 0x2000 : 0x4000;
        return added;
    }
    if (sp->next != 0xFFFF || sp->prev != 0xFFFF)
    {
        return false;
    }
    const unsigned short i = (unsigned short)(sp - objects);
    if (transfer_child_obj_list == 0xFFFF)
    {
        sp->next = i;
        sp->prev = i;
        transfer_child_obj_list = i;
    }
    else
    {
        sp->prev = objects[transfer_child_obj_list].prev;
        sp->next = transfer_child_obj_list;
        objects[objects[transfer_child_obj_list].prev].next = i;
        objects[transfer_child_obj_list].prev = i;
    }
    sp->flags |= 0x40000000;
    if (mover_p == nullptr)
    {
        return true;
    }
    mover_p->set_render_changed();
    mover_p->set_render_flags(8);
    sp->flags |= 0x4000;
    return true;
}

// 0x007279F0 (:1490)
// Marks the link broken (yield limit -1 or below) and moves it out of both pieces' active link lists.
void rfg_destroyable_shape::remove_link(rfg_link* l)
{
    rfg_link_base* base_links = parent->base_links.to_pointer();
    const unsigned int index = (unsigned int)(l - links);
    if (l->yield_limit > -2)
    {
        l->yield_limit = -1;
    }
    for (int side = 0; side < 2; side++)
    {
        rfg_subpiece& piece = objects[base_links[index].obj[side]];
        unsigned short* list = piece.links.to_pointer();
        for (int k = 0; k < piece.num_links; k++)
        {
            if (list[k] == index)
            {
                piece.num_links--;
                if (k < piece.num_links)
                {
                    const unsigned short t = list[k];
                    list[k] = list[piece.num_links];
                    list[piece.num_links] = t;
                }
                break;
            }
        }
    }
}

// 0x007176D0 (:1518)
// A negative damage breaks the link outright.
void rfg_destroyable_shape::damage_link(rfg_link* l, int damage)
{
    if (damage < 0)
    {
        damage = l->yield_limit;
    }
    l->yield_limit -= damage;
}

// 0x00727B10 (:1529)
// Clamps the yield limit to [0, yield_max] and adds repair (negative = full repair), up to yield_max.
void rfg_destroyable_shape::repair_link(rfg_link* l, int repair)
{
    int current = l->yield_limit;
    const int yield_max = parent->base_links.to_pointer()[l - links].yield_max;
    if (current < 0)
    {
        current = 0;
    }
    else if (yield_max < current)
    {
        current = yield_max;
    }
    if (repair < 0)
    {
        repair = yield_max;
    }
    l->yield_limit = current;
    if (yield_max - current < repair)
    {
        l->yield_limit = yield_max - current + current;
        return;
    }
    l->yield_limit = repair + current;
}

// 0x00727B60 (:1549)
// Restores the link's strength and puts it back into both pieces' active link lists.
void rfg_destroyable_shape::restore_link(rfg_link* l)
{
    rfg_link_base* lb = &parent->base_links.to_pointer()[l - links];
    const unsigned int index = (unsigned int)(l - links);
    l->yield_limit = lb->yield_max;
    rfg_subpiece_base* base_objects = parent->base_objects.to_pointer();
    for (int side = 0; side < 2; side++)
    {
        const int obj = lb->obj[side];
        rfg_subpiece& piece = objects[obj];
        const unsigned int total = base_objects[obj].num_links;
        unsigned short* list = piece.links.to_pointer();
        for (unsigned int k = piece.num_links; k < total; k++)
        {
            if (list[k] == index)
            {
                if ((int)piece.num_links < (int)k)
                {
                    const unsigned short t = list[k];
                    list[k] = list[piece.num_links];
                    list[piece.num_links] = t;
                }
                piece.num_links++;
                break;
            }
        }
    }
}

// 0x00727F90 (:1621)
unsigned int rfg_destroyable_shape::get_dlod_index_from_name(const char* name)
{
    const unsigned int hash = crc_stri(name, 0);
    rfg_dlod_base* base_dlods = parent->base_dlods.to_pointer();
    for (unsigned int i = 0; i < (unsigned int)parent->num_dlods; i++)
    {
        if (base_dlods[i].name_hash == hash)
        {
            return i;
        }
    }
    return 0xFFFFFFFF;
}

// 0x007176F0 (:1638)
bool rfg_destroyable_shape::dlod_damaged_this_frame(int dlod_index)
{
    return (dlods[dlod_index].flags & 1) != 0;
}

// 0x00717710 (:1644)
float rfg_destroyable_shape::dlod_percent_damaged(int dlod_index)
{
    const unsigned int max_pieces = dlods[dlod_index].max_pieces;
    return (float)(int)(max_pieces - dlods[dlod_index].active_pieces) / (float)max_pieces;
}

// 0x00717750 (:1652)
// Inertia tensor of a solid box of the shape's bounds: diag(m/3 * (y²+z²), m/3 * (x²+z²), m/3 * (x²+y²)) of the
// half extents.
void rfg_destroyable_shape::get_tensor(matrix& tensor)
{
    const float m = mass * 0.33333334f;
    float x = (local_bmax.x - local_bmin.x) * 0.5f;
    float y = (local_bmax.y - local_bmin.y) * 0.5f;
    float z = (local_bmax.z - local_bmin.z) * 0.5f;
    x *= x;
    y *= y;
    z *= z;
    tensor.rvec.x = (z + y) * m;
    tensor.rvec.y = 0.0f;
    tensor.rvec.z = 0.0f;
    tensor.uvec.x = 0.0f;
    tensor.uvec.y = (z + x) * m;
    tensor.uvec.z = 0.0f;
    tensor.fvec.x = 0.0f;
    tensor.fvec.y = 0.0f;
    tensor.fvec.z = (y + x) * m;
}

// 0x00727FE0 (:1669)
// Updates the destruction mode timer, the local bounds and center of mass, and the mover's Havok shape data and
// world bounds.
void rfg_destroyable_shape::recalc_shape_properties()
{
    if (num_existing_child_shapes > 0)
    {
        if ((flags & 8) != 0 && mass < 50000.0f)
        {
            flags &= ~8;
            if (destruction_mode_timer.time_until() > 10000)
            {
                destruction_mode_timer.set(10000);
            }
        }
        if ((flags & 0x400) != 0)
        {
            local_bmin = vector(3.4028235e+38f, 3.4028235e+38f, 3.4028235e+38f);
            local_bmax = vector(-3.4028235e+38f, -3.4028235e+38f, -3.4028235e+38f);
            if (child_obj_list != 0xFFFF)
            {
                rfg_subpiece_base* base_objects = parent->base_objects.to_pointer();
                for (rfg_subpiece* sp = &objects[child_obj_list]; sp != nullptr; sp = &objects[sp->next])
                {
                    const rfg_subpiece_base* sp_base = &base_objects[sp - objects];
                    bbox_expand(local_bmin, local_bmax, sp_base->prel_bmin, sp_base->prel_bmax);
                    if (sp->next == child_obj_list)
                    {
                        break;
                    }
                }
            }
            flags &= ~0x400;
        }
        if ((flags & 0x2000) != 0)
        {
            flags &= ~0x2000;
            if (child_obj_list != 0xFFFF)
            {
                rfg_subpiece_base* base_objects = parent->base_objects.to_pointer();
                for (rfg_subpiece* sp = &objects[child_obj_list]; sp != nullptr; sp = &objects[sp->next])
                {
                    if ((base_objects[sp - objects].flags & 0x40) != 0)
                    {
                        flags |= 0x1000;
                        break;
                    }
                    if (sp->next == child_obj_list)
                    {
                        break;
                    }
                }
            }
        }
        center_of_mass.x = center_of_mass_accum.x / mass;
        center_of_mass.y = center_of_mass_accum.y / mass;
        center_of_mass.z = center_of_mass_accum.z / mass;
    }
    rfg_mover* rm = mover_p;
    if ((flags & 0x8000) != 0 && rm != nullptr)
    {
        flags &= ~0x8000;
        havok_body_update_cached_shape_data(rm->havok_handle);
        bbox_instance(rm->last_known_bmin, rm->last_known_bmax, rm->orient, rm->pos, local_bmin, local_bmax);
    }
}

// 0x0073F4C0 (:1745)
// Rebuilds the rbb tree over the existing pieces when many pieces are gone (more than Recalc_rbb_count, or fewer than
// Recalc_rbb_pct of them left), or on request (flag 0x800: back to the parent's tree while most pieces are left).
// The new tree goes to RFG_shape_rbb_mempool; the local bounds follow the tree root.
void rfg_destroyable_shape::recalc_rbb_tree()
{
    const rfg_rbb_node* source = aabb_tree;
    if ((flags & 0x800) == 0)
    {
        if (calc_rbb_child_count < 0xB)
        {
            return;
        }
        if (calc_rbb_child_count - num_existing_child_shapes <= Recalc_rbb_count &&
            Recalc_rbb_pct <= (float)num_existing_child_shapes / (float)calc_rbb_child_count)
        {
            return;
        }
    }
    else
    {
        const int total = parent->num_objects;
        if (Use_parent_rbb_pct < (float)num_existing_child_shapes / (float)total && total - num_existing_child_shapes <= Use_parent_rbb_count)
        {
            calc_rbb_child_count = total;
            release_rbb_tree();
            return;
        }
        flags &= ~0x800;
        source = parent->aabb_tree.to_pointer();
    }
    stress_evaluation_start(mover_p, true, false);
    const unsigned int new_size = rfg_rbb_build_tree(this, source);
    Rbb_tree_wrapper.enter();
    void* mem = RFG_shape_rbb_mempool.realloc(override_tree, new_size);
    if (mem == nullptr)
    {
        if (override_tree != nullptr)
        {
            release_rbb_tree();
        }
        if (RFG_shape_rbb_mempool.can_alloc(new_size, 0))
        {
            mem = RFG_shape_rbb_mempool.alloc(new_size, 0);
        }
    }
    Rbb_tree_wrapper.leave();
    mempool pool(mem, RFG_shape_rbb_mempool.size(mem), "rbb_dyn_buf", 1, 0);
    rfg_rbb_node* tree = rfg_rbb_build_finish(&pool);
    Rbb_tree_wrapper.enter();
    if (tree == nullptr)
    {
        RFG_shape_rbb_mempool.release_bytes(override_tree);
        override_tree = nullptr;
        aabb_tree = parent->aabb_tree.to_pointer();
    }
    else
    {
        RFG_shape_rbb_mempool.shrink_to_fit(tree, new_size);
        calc_rbb_child_count = num_existing_child_shapes;
        override_tree = tree;
        aabb_tree = tree;
    }
    Rbb_tree_wrapper.leave();
    aabb_tree->aabb.get(local_bmin, local_bmax);
}

// 0x00720050 (:1841)
// Queues a piece of this shape for shardify (once); deleted pieces (0x80) go to the front.
void rfg_destroyable_shape::add_subpiece_to_shardify_q(rfg_subpiece* sp)
{
    const unsigned int f = sp->flags;
    if ((f & 0x10) != 0 || owner_shape(sp) != this)
    {
        return;
    }
    sp->flags = f | 0x810;
    const unsigned short i = (unsigned short)(sp - objects);
    if ((sp->flags & 0x80) != 0)
    {
        if (shardify_q == 0xFFFF)
        {
            sp->shard_next = i;
            sp->shard_prev = i;
        }
        else
        {
            objects[objects[shardify_q].shard_prev].shard_next = i;
            sp->shard_prev = objects[shardify_q].shard_prev;
            objects[shardify_q].shard_prev = i;
            sp->shard_next = shardify_q;
        }
        shardify_q = i;
    }
    else if (shardify_q == 0xFFFF)
    {
        sp->shard_next = i;
        sp->shard_prev = i;
        shardify_q = i;
    }
    else
    {
        sp->shard_prev = objects[shardify_q].shard_prev;
        sp->shard_next = shardify_q;
        objects[objects[shardify_q].shard_prev].shard_next = i;
        objects[shardify_q].shard_prev = i;
    }
    if (mover_p != nullptr)
    {
        rfg_dyn_add_to_process_shardify_list(mover_p);
    }
}

// 0x00720240 (:1887)
void rfg_destroyable_shape::remove_subpiece_from_shardify_q(rfg_subpiece* sp)
{
    const unsigned int f = sp->flags;
    if ((f & 0x10) == 0 || owner_shape(sp) != this)
    {
        return;
    }
    sp->flags = f & ~0x10;
    const unsigned short i = (unsigned short)(sp - objects);
    if (shardify_q == i)
    {
        if (sp->shard_next == i)
        {
            sp->shard_next = 0xFFFF;
            sp->shard_prev = 0xFFFF;
            shardify_q = 0xFFFF;
            return;
        }
        shardify_q = sp->shard_next;
    }
    objects[sp->shard_prev].shard_next = sp->shard_next;
    objects[sp->shard_next].shard_prev = sp->shard_prev;
    sp->shard_next = 0xFFFF;
    sp->shard_prev = 0xFFFF;
}

// 0x00728210 (:1906)
// Back to the parent's tree.
void rfg_destroyable_shape::release_rbb_tree()
{
    Rbb_tree_wrapper.enter();
    RFG_shape_rbb_mempool.release_bytes(override_tree);
    override_tree = nullptr;
    aabb_tree = parent->aabb_tree.to_pointer();
    Rbb_tree_wrapper.leave();
}

// 0x005DB4A0 (rfg_shapes.inl:475)
rfg_subpiece* rfg_destroyable_shape::get_object_buffer() const
{
    return objects;
}

// 0x00736F40 (:1923)
// Releases every piece (dynamic links of world-anchored pieces too), empties the transfer list and shardify queue,
// detaches the mover and frees the piece buffer, or hands the uid over to a sibling that shares it.
void rfg_destroyable_shape::delete_me()
{
    if (parent == nullptr)
    {
        return;
    }
    rfg_mover* rm = mover_p;
    while (child_obj_list != 0xFFFF)
    {
        rfg_subpiece* sp = &objects[child_obj_list];
        if ((sp->flags & 2) != 0)
        {
            rfg_mover_delete_dynamic_link(rm, sp, false);
        }
        set_shape_state(sp, false);
        sp->flags = 0;
    }
    while (transfer_child_obj_list != 0xFFFF)
    {
        rfg_subpiece* sp = &objects[transfer_child_obj_list];
        const unsigned short i = (unsigned short)(sp - objects);
        if (transfer_child_obj_list == i && sp->next == i)
        {
            sp->next = 0xFFFF;
            sp->prev = 0xFFFF;
            transfer_child_obj_list = 0xFFFF;
        }
        else
        {
            if (transfer_child_obj_list == i)
            {
                transfer_child_obj_list = sp->next;
            }
            objects[sp->prev].next = sp->next;
            objects[sp->next].prev = sp->prev;
            sp->next = 0xFFFF;
            sp->prev = 0xFFFF;
        }
        sp->flags = 0;
    }
    while (shardify_q != 0xFFFF)
    {
        rfg_subpiece* sp = &objects[shardify_q];
        const unsigned short i = (unsigned short)(sp - objects);
        if (shardify_q == i && sp->shard_next == i)
        {
            sp->shard_next = 0xFFFF;
            sp->shard_prev = 0xFFFF;
            shardify_q = 0xFFFF;
        }
        else
        {
            if (shardify_q == i)
            {
                shardify_q = sp->shard_next;
            }
            objects[sp->shard_prev].shard_next = sp->shard_next;
            objects[sp->shard_next].shard_prev = sp->shard_prev;
            sp->shard_next = 0xFFFF;
            sp->shard_prev = 0xFFFF;
        }
    }
    if (rm != nullptr)
    {
        if (rm->rdd.is_loaded())
        {
            rm->rdd.get_data()->shape = nullptr;
        }
        mover_p = nullptr;
    }
    release_rbb_tree();
    rfg_destroyable_shape* next = sib_next;
    if (next == this)
    {
        if (objects != nullptr)
        {
            Level_memmgr.free(objects);
        }
        Destroyable_uid_hash.remove(shape_uid);
    }
    else
    {
        rfg_destroyable_shape* registered = nullptr;
        if (Destroyable_uid_hash.get(shape_uid, registered) && registered == this)
        {
            Destroyable_uid_hash.remove(shape_uid);
            Destroyable_uid_hash.insert_key_data(shape_uid, next);
        }
        sib_prev->sib_next = sib_next;
        sib_next->sib_prev = sib_prev;
        sib_next = nullptr;
        sib_prev = nullptr;
    }
    parent = nullptr;
    objects = nullptr;
    links = nullptr;
    dlods = nullptr;
    aabb_tree = nullptr;
}

// 0x00736A70
rfg_destroyable_shape::~rfg_destroyable_shape()
{
}

// 0x00730150 (:2026)
// The sphere radius is the w of its position.
void rfg_destroyable_shape::querySphere(const hkSphere& sphere, hkArray<unsigned int,hkContainerHeapAllocator>& hits) const
{
    vector pos;
    havok_to_game_vector(pos, sphere.m_pos);
    rfg_rbb_node::query_sphere(this, pos, ((const float*)&sphere.m_pos.m_quad)[3], hits);
}

// 0x00730190 (:2039)
void rfg_destroyable_shape::querySphere(const vector& pos, const float radius, hkArray<unsigned int,hkContainerHeapAllocator>& hits) const
{
    rfg_rbb_node::query_sphere(this, pos, radius, hits);
}

// 0x007301C0 (:2045)
void rfg_destroyable_shape::queryAabb(const hkAabb& aabb, hkArray<unsigned int,hkContainerHeapAllocator>& hits) const
{
    vector bmin;
    vector bmax;
    havok_to_game_vector(bmin, aabb.m_min);
    havok_to_game_vector(bmax, aabb.m_max);
    rfg_rbb_node::query_aabb(this, bmin, bmax, hits);
}

// 0x00730210 (:2057)
void rfg_destroyable_shape::queryAabb(const vector& bmin, const vector& bmax, hkArray<unsigned int,hkContainerHeapAllocator>& hits) const
{
    rfg_rbb_node::query_aabb(this, bmin, bmax, hits);
}

// 0x00730230 (:2063)
void rfg_destroyable_shape::queryObb(const hkTransformf& obbTransform, const hkVector4f& obbExtent, float tolerance, hkArray<unsigned int,hkContainerHeapAllocator>& hits) const
{
    vector pos;
    matrix orient;
    vector extents;
    havok_to_game_vector(pos, obbTransform.m_translation);
    havok_to_game_matrix(orient, obbTransform.m_rotation);
    havok_to_game_vector(extents, obbExtent);
    rfg_rbb_node::query_obb(this, pos, orient, extents, hits);
}

// 0x00730290 (:2081)
void rfg_destroyable_shape::queryObb(const vector& pos, const matrix& orient, const vector& extents, hkArray<unsigned int,hkContainerHeapAllocator>& hits) const
{
    rfg_rbb_node::query_obb(this, pos, orient, extents, hits);
}

// 0x00728250 (:2088)
hkBool rfg_destroyable_shape::castRay(const hkpShapeRayCastInput& input, hkpShapeRayCastOutput& results) const
{
    rfg_rbb_node::query_ray(this, input, results);
    hkBool hit;
    hit.m_bool = results.m_hitFraction < 1.0f ? 1 : 0;
    return hit;
}

// 0x00728290 (:2094)
void rfg_destroyable_shape::castRayWithCollector(const hkpShapeRayCastInput& in, const hkpCdBody& body, hkpRayHitCollector& out) const
{
    rfg_rbb_node::query_ray(this, in, body, out);
}

// 0x007282B0 (:2099)
// The shape fits the SPU buffer when every child does.
int rfg_destroyable_shape::calcSizeForSpu(const hkpShape::CalcSizeForSpuInput& input, int spuBufferSizeLeft) const
{
    if (spuBufferSizeLeft < (int)sizeof(rfg_destroyable_shape))
    {
        return -1;
    }
    if (child_obj_list != 0xFFFF)
    {
        rfg_subpiece_base_extra_data* extra = parent->base_extra_data.to_pointer();
        for (rfg_subpiece* sp = &objects[child_obj_list]; sp != nullptr; sp = &objects[sp->next])
        {
            const hkpShape* shape = (hkpShape*)extra[sp - objects].shape;
            if (shape->calcSizeForSpu(input, spuBufferSizeLeft) == -1)
            {
                return -1;
            }
            if (sp->next == child_obj_list)
            {
                break;
            }
        }
    }
    return (int)sizeof(rfg_destroyable_shape);
}

// 0x00736A10 (.h:664)
const hkpShapeContainer* rfg_destroyable_shape::getContainer() const
{
    return this;
}

// 0x007178D0 (:2123)
const hkpShape* rfg_destroyable_shape::getChildShape(unsigned int key, hkpShapeBufferStorage& buffer) const
{
    return getChildShapeImpl(key, buffer);
}

// 0x00736A20 (rfg_shapes.inl:330)
unsigned int rfg_destroyable_shape::getFirstKey() const
{
    return child_obj_list == 0xFFFF ? 0xFFFFFFFF : child_obj_list;
}

// 0x00736A40 (rfg_shapes.inl:346)
unsigned int rfg_destroyable_shape::getNextKey(unsigned int oldKey) const
{
    const unsigned short next = objects[oldKey].next;
    return next == child_obj_list ? 0xFFFFFFFF : next;
}

// 0x00736A60 (rfg_shapes.inl:371)
int rfg_destroyable_shape::getNumChildShapes() const
{
    return num_existing_child_shapes;
}

// ---- shard impulses (rfg_shapes.inl)
// 0x00710970 (rfg_shapes.inl:17)
void rfg_shape_shard_impulse_info_clear()
{
    Num_shard_impulse_info = 0;
}

// 0x00710980 (rfg_shapes.inl:23)
shard_impulse_info* rfg_shape_get_shard_impulse_info(rfg_subpiece* sp)
{
    return sp->impulse_info_index != 0xFFFF ? &Shard_impulse_info_pool[sp->impulse_info_index] : nullptr;
}

// 0x00717600
shard_impulse_info::shard_impulse_info()
{
}

// 0x00710930 (.h:360)
void shard_impulse_info::reset()
{
    impulse_dir.zero();
}

// 0x00723550 (rfg_shapes.inl:38)
void rfg_base_destroyable_shape::get_subpiece_orient_local(const rfg_subpiece_base* sp_base, matrix& orient)
{
    orient = base_dlods.to_pointer()[sp_base->dlod_key & 0xFFFF].orient;
}

// 0x007109A0 (rfg_shapes.inl:50)
float rfg_subpiece_base::get_max_dim() const
{
    float d = prel_bmax.x - prel_bmin.x;
    const float dy = prel_bmax.y - prel_bmin.y;
    const float dz = prel_bmax.z - prel_bmin.z;
    if (d - dy < 0.0f)
    {
        d = dy;
    }
    return d - dz >= 0.0f ? d : dz;
}

// 0x00710A40 (rfg_shapes.inl:58)
bool rfg_subpiece_base::is_structural()
{
    return (flags & 0x28) == 0;
}

// 0x00710A50 (rfg_shapes.inl:65)
material_shape_type rfg_subpiece_base::get_override_shape() const
{
    return (material_shape_type)shape_type;
}

// 0x00710A60 (rfg_shapes.inl:70)
void rfg_subpiece_base::set_override_shape(material_shape_type type)
{
    shape_type = (signed char)type;
}

// ---- system
// 0x007202F0 (:2147)
void rfg_shape_system_init()
{
    Destroyable_uid_hash.clear();
    RFG_shape_rbb_mempool.init(RFG_shape_rbb_buf, sizeof(RFG_shape_rbb_buf), 0x80, "Rbb_tree_pool", 1, 0);
}

// 0x007178E0 (:2164)
void rfg_shape_level_init_pre()
{
}

// 0x007178F0 (:2182)
void rfg_shape_level_shutdown()
{
}

// 0x00717900 (:2200)
void rfg_shape_set_thread_ownership()
{
    RFG_shape_rbb_mempool.set_thread_ownership();
}

// 0x00720320 (:2205)
// The weaker material's link strength times the contact area (8.8 fixed point, at least 1/256); two pieces of an
// indestructible material (flag 4 marks the pieces 0x20) never break.
int rfg_shape_calc_link_strength(unsigned char mat_0_index, rfg_subpiece_base* sp_0, unsigned char mat_1_index, rfg_subpiece_base* sp_1, float yield_multiplier)
{
    const rfg_physical_material* mat_0 = Rfg_physical_material_default;
    if (mat_0_index != 0xFF && mat_0_index < Num_rfg_physical_materials)
    {
        mat_0 = &RFG_physical_materials[mat_0_index];
    }
    const rfg_physical_material* mat_1 = Rfg_physical_material_default;
    if (mat_1_index != 0xFF && mat_1_index < Num_rfg_physical_materials)
    {
        mat_1 = &RFG_physical_materials[mat_1_index];
    }
    if ((mat_0->flags & 4) != 0 && sp_0 != nullptr)
    {
        sp_0->flags |= 0x20;
    }
    if ((mat_1->flags & 4) != 0 && sp_1 != nullptr)
    {
        sp_1->flags |= 0x20;
    }
    if (sp_0 != nullptr && sp_1 != nullptr && (sp_0->flags & 0x20) != 0 && (sp_1->flags & 0x20) != 0)
    {
        return 0x7FFFFFFF;
    }
    int area = (int)(yield_multiplier * 256.0f);
    if (area < 2)
    {
        area = 1;
    }
    unsigned int strength = mat_1->link_strength;
    if (mat_0->link_strength < mat_1->link_strength)
    {
        strength = mat_0->link_strength;
    }
    const long long product = (long long)(int)(strength << 8) * (long long)area;
    return (int)(product >> 16);
}

// 0x00717910 (:2237)
unsigned int rfg_shape_get_shape_uid_from_object(const object* objp)
{
    if (objp != nullptr && objp->get_obj_type() == OT_MOVER)
    {
        return ((const object_mover*)objp)->shape_uid;
    }
    return 0xFFFFFFFF;
}

// 0x00728370 (:2252)
rfg_destroyable_shape* rfg_shape_get_shape_from_uid(const unsigned int shape_uid)
{
    rfg_destroyable_shape* shape = nullptr;
    Destroyable_uid_hash.get(shape_uid, shape);
    return shape;
}

// 0x0073CD70 (:2457)
// LTCG: base and the shape position move one stack slot (chunk_uid is passed as base again).
rfg_destroyable_shape* rfg_shape_new(rfg_base_destroyable_shape* base, unsigned int chunk_uid, const vector& pos, const matrix& orient, unsigned int sibling_uid)
{
    if (base == nullptr)
    {
        return nullptr;
    }
    rfg_destroyable_shape* shape = new rfg_destroyable_shape(base, chunk_uid, pos, orient, sibling_uid);
    if (shape == nullptr)
    {
        return nullptr;
    }
    if (shape->is_valid())
    {
        return shape;
    }
    shape->delete_me();
    shape->removeReference();
    return nullptr;
}

// 0x00737220 (:2484)
void rfg_shape_delete(rfg_destroyable_shape* shape)
{
    if (shape != nullptr)
    {
        shape->delete_me();
        shape->removeReference();
    }
}

// 0x00717930 (:2862)
// Contact area of two boxes: the largest face of their intersection box, or, when they only touch, the area of the
// silhouette of box 1 (seen from far behind box 0 along the centre line) clipped by the silhouette prism of box 0.
float rfg_shape_calc_overlap_area(const vector& pos_0, const vector& bmin_0, const vector& bmax_0, const vector& pos_1, const vector& bmin_1, const vector& bmax_1)
{
    float area = 0.0f;
    vector ibmin;
    vector ibmax;
    if (ix_box_box_intersect(bmin_0, bmax_0, bmin_1, bmax_1, ibmin, ibmax))
    {
        const float xy = (ibmax.y - ibmin.y) * (ibmax.x - ibmin.x);
        area = (ibmax.z - ibmin.z) * (ibmax.x - ibmin.x);
        const float yz = (ibmax.z - ibmin.z) * (ibmax.y - ibmin.y);
        if (0.0f <= xy - area)
        {
            area = xy;
        }
        if (area - yz < 0.0f)
        {
            area = yz;
        }
        if (0.0f < area)
        {
            return area;
        }
    }
    vector dir(pos_0.x - pos_1.x, pos_0.y - pos_1.y, pos_0.z - pos_1.z);
    dir.normalize_safe();
    vector view(pos_0.x + dir.x * 100.0f, pos_0.y + dir.y * 100.0f, pos_0.z + dir.z * 100.0f);
    vector silhouette_1[6];
    const int num_1 = bbox_find_silhouette(silhouette_1, view, bmin_1, bmax_1);
    view = vector(pos_1.x - dir.x * 100.0f, pos_1.y - dir.y * 100.0f, pos_1.z - dir.z * 100.0f);
    vector silhouette_0[6];
    const int num_0 = bbox_find_silhouette(silhouette_0, view, bmin_0, bmax_0);
    plane planes[6];
    for (int i = 0; i < num_0; i++)
    {
        const vector along(silhouette_0[i].x + dir.x, silhouette_0[i].y + dir.y, silhouette_0[i].z + dir.z);
        planes[i].set(silhouette_0[i], along, silhouette_0[(i + 1) % num_0]);
    }
    vector clipped[12];
    const int num = polyclip_with_plane_list_3d(num_1, silhouette_1, clipped, planes, num_0);
    if (num == 0)
    {
        return 0.0f;
    }
    // Area of the clipped polygon in the plane perpendicular to dir (shoelace formula).
    const vector origin = clipped[0];
    matrix frame;
    frame.make_quick(dir);
    for (int i = 0; i < num; i++)
    {
        clipped[i].x -= origin.x;
        clipped[i].y -= origin.y;
        clipped[i].z -= origin.z;
        clipped[i].rotate(frame);
    }
    area = 0.0f;
    int next = 1;
    for (int i = 0; i < num; i++)
    {
        area = (clipped[next].y * clipped[i].x - clipped[next].x * clipped[i].y) + area;
        next = (i + 2) % num;
    }
    return area * 0.5f;
}
