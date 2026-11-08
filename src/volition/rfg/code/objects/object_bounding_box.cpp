// Original: volition/rfg/code/objects/object_bounding_box.cpp
//
// Bounding box objects: a placed box region. A box of type gps_target with a dummy child gives the vehicle AI a rally
// point (vehicle_ai_actions.cpp). Compiled in rfg_objects_unity.cpp. stub_misc.md group 3: the whole file.

#include "volition/rfg/code/objects/object_bounding_box.h"

#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/havok/havok_collision_events.h"
#include "volition/rfg/code/level/rfg_prop_block_shared.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/object_prop_names_shared.h"
#include "volition/vlib/code/math/bbox2d.h"
#include "volition/vlib/code/math/ix.h"
#include "volition/vlib/code/math/vector2.h"

static object_pool<object_bounding_box,64,1> object_bounding_boxpool; // pool capacity from the instantiator below

// Built by the variadic constructor (0x010767C0): the names of bounding_box_type.
static str_data_hash_table<unsigned int, 32, 32, unsigned char> Bounding_box_type_table( // 0x034FE720
    "None", (unsigned int)BBT_NONE,
    "GPS Target", (unsigned int)BBT_GPS_TARGET,
    nullptr, 0u);

// 0x00A15CB0 (:32)
void types_object_bounding_box(signed char& type, signed char& subtype)
{
    type = OT_BOUNDING_BOX;
    subtype = OT_SUB_UNDEFINED;
}

// 0x00A15CC0 (:32)
int bpo_index_object_bounding_box()
{
    return 1;
}

// 0x00A15CD0 (:32)
const char* classname_object_bounding_box()
{
    return "object_bounding_box";
}

// 0x00A615B0 (:32)
object* alloc_object_bounding_box()
{
    return object_bounding_boxpool.alloc(0);
}

// Magic numbers: rfg.exe's arguments (0x01076810), the pool's bytes and capacity.
static obj_class_instantiator object_bounding_box_instantiator("object_bounding_box", alloc_object_bounding_box, types_object_bounding_box, bpo_index_object_bounding_box, classname_object_bounding_box, object_bounding_box::pool_remaining, 0x3580, 0x40);

// 0x00A15CE0 (:32)
unsigned int object_bounding_box::get_classname_hash()
{
    return object_bounding_box_instantiator.this_hash;
}

// 0x00A395D0 (:32)
int object_bounding_box::pool_remaining()
{
    return object_bounding_boxpool.get_free();
}

// 0x00A8DD60 (:32)
void object_bounding_box::pool_free()
{
    object_bounding_boxpool.free(this);
}

// 0x00A15CF0 (:33)
object_bounding_box::object_bounding_box()
{
}

// 0x00A15D20
object_bounding_box::~object_bounding_box()
{
}

// 0x00AA4E80 (:45)
bool object_bounding_box::serialize(obj_prop_block& pblock, bool load)
{
    if (!object::serialize(pblock, load))
    {
        return false;
    }
    if (load)
    {
        pblock.get(BOUNDING_BOX_PROP_BOX, local_bb);
        if (!pblock.get_str_as_enum_optional(BOUNDING_BOX_PROP_TYPE, type, Bounding_box_type_table))
        {
            type = BBT_NONE;
        }
        bbox_instance(bb, orient, pos, local_bb);
        if (!obj_flags.original)
        {
            obj_flags.no_save = 1;
        }
    }
    else
    {
        // Original: the type is not written back.
        pblock.set(BOUNDING_BOX_PROP_BOX, local_bb);
    }
    return true;
}

// 0x00A15D30 (:89)
bool object_bounding_box::stream(bool load)
{
    if (!load && obj_flags.destroy_on_stream)
    {
        World.destroy_object(this, false);
    }
    return true;
}

// 0x00A8DD70 (:123): object::free_this, inlined in rfg.exe.
void object_bounding_box::free_this()
{
    object::free_this();
}

// 0x00A15D50 (:129)
void object_bounding_box::get_bbox(vector& bmin, vector& bmax) const
{
    bmin = bb.min;
    bmax = bb.max;
}

// 0x00A15D90 (:136)
void object_bounding_box::get_local_bbox(vector& bmin, vector& bmax) const
{
    bmin = local_bb.min;
    bmax = local_bb.max;
}

// 0x00A15DD0 (:146): object::update_pos_and_orient without its render update. Original: bb stays where it was set on load.
void object_bounding_box::update_pos_and_orient(const vector& update_pos, const matrix& update_orient, bool set_havok_data)
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
}

// 0x00A15E70 (:161): as its parent.
bool object_bounding_box::is_killed() const
{
    const object* parent_objp = World.object_from_handle(get_parent());
    return parent_objp != nullptr && parent_objp->is_killed();
}

// 0x00A39600 (:177): the position of the box's dummy child.
bool object_bbox_get_rally_point(const object_bounding_box* bbp, vector& rally_point)
{
    if (bbp->type != BBT_GPS_TARGET)
    {
        return false;
    }
    const object* first = bbp->child_ptr;
    for (const object* child = first; child != nullptr; child = child->child_next)
    {
        if (child->get_obj_type() == OT_DUMMY)
        {
            rally_point = child->pos;
            return true;
        }
        if (child->child_next == first)
        {
            break; // the children are a ring
        }
    }
    return false;
}

// 0x00A615C0 (:199): the GPS target box around a point: its world box first (xz), then its own box in its own space.
const object_bounding_box* object_bbox_find_gps(const vector& point)
{
    const vector2 point_2d(point.x, point.z);
    const int num_boxes = World.get_num_objects<object_bounding_box>();
    for (int i = 0; i < num_boxes; i++)
    {
        const object_bounding_box* bbp = World.get_object<object_bounding_box>(i);
        if (bbp->type != BBT_GPS_TARGET)
        {
            continue;
        }
        vector bmin;
        vector bmax;
        bbp->get_bbox(bmin, bmax);
        bbox2d world_box;
        world_box.min.x = bmin.x;
        world_box.min.y = bmin.z;
        world_box.max.x = bmax.x;
        world_box.max.y = bmax.z;
        if (!ix2d_point_bbox(point_2d, world_box))
        {
            continue;
        }
        vector local_point = point;
        vec_change_to_local_coords(local_point, bbp->pos, bbp->orient);
        const vector2 local_2d(local_point.x, local_point.z);
        bbp->get_local_bbox(bmin, bmax);
        bbox2d local_box;
        local_box.min.x = bmin.x;
        local_box.min.y = bmin.z;
        local_box.max.x = bmax.x;
        local_box.max.y = bmax.z;
        if (ix2d_point_bbox(local_2d, local_box))
        {
            return bbp;
        }
    }
    return nullptr;
}
