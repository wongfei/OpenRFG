// Original: volition/rfg/code/objects/object_roadblock_node.cpp
//
// gameflow.md GF4: roadblocks. Registers the class, so zones now create these nodes, and the boot reads
// roadblock_formation.xtbl and roadblock_types.xtbl. A started roadblock places a bounding box on the road with barricades,
// turrets, guard nodes and squad spawn nodes as its children.

#include "volition/rfg/code/objects/object_roadblock_node.h"

#include "volition/rfg/code/destruction/rfg_mover.h"
#include "volition/rfg/code/gameplay/gameflow/gameflow_enums.h"
#include "volition/rfg/code/gameplay/gameflow/gfm_orders_manager.h"
#include "volition/rfg/code/gamewide/game_render.h"
#include "volition/rfg/code/level/rfg_prop_block_shared.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/movers/object_mover.h"
#include "volition/rfg/code/objects/object_bounding_box.h"
#include "volition/rfg/code/objects/object_dummy.h"
#include "volition/rfg/code/objects/object_guard_node.h"
#include "volition/rfg/code/objects/object_path_road.h"
#include "volition/rfg/code/objects/object_prop_names_shared.h"
#include "volition/rfg/code/objects/object_spawn_node.h"
#include "volition/rfg/code/objects/object_squad.h"
#include "volition/rfg/code/objects/turret.h"
#include "volition/rfg/code/objects/turret_info.h"
#include "volition/rfg/code/pathfinding/bb_pathfinding.h"
#include "volition/rfg/code/rfg.h"
#include "volition/rfg/code/spawn/spawn_object_group.h"
#include "volition/vlib/code/math/bbox.h"
#include "volition/vlib/code/math/crc.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/memory/object_pool.h"
#include "volition/vlib/code/memory/rfg_split_mempool.h"
#include "volition/vlib/code/memory/static_mempool.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/vlib/code/time/timestamp.h"
#include "volition/vlib/code/util/fastxml.h"
#include "volition/vlib/code/util/string_pool.h"
#include "volition/vlib/code/util/string_util.h"

#include <math.h>
#include <string.h>

// size 0x444 (object_roadblock_node_internal.h:204)
class roadblock_construction_info
{
public:
    roadblock_construction_info(); // 0x00A6A4A0
    void reset(); // 0x00A3D890
    ~roadblock_construction_info(); // 0x00A6AEE0

    vector pos; // 0x00
    matrix orient; // 0x0C
    object_bounding_box* bbp; // 0x30
    farray<object_dummy*, 256> dummy_node_list; // 0x34
};

static farray<unsigned int, 32> Roadblock_clean_up_list; // 0x016ADFE0
static timestamp Roadblock_clean_up_period; // 0x016AE070
static string_pool Roadblock_string_pool(nullptr); // 0x031A2A50
static char* Roadblock_name_string_pool_data; // 0x0302F168
static object_pool<roadblock_data_type, 16, 1> Roadblock_type_pool; // 0x0304B640
static object_pool<roadblock_squad_info, 32, 1> Roadblock_squad_pool; // 0x031A35D0
static object_pool<roadblock_formation_info, 8, 1> Roadblock_formation_pool; // 0x034FDF10
static object_pool<object_roadblock_node, 64, 1> object_roadblock_nodepool; // 0x0303CAE0

static const float DEFAULT_TURRET_ROTATIONAL_LIMIT = 90.0f; // 0x01315398
static const float GUARD_NODE_OFFSET_DISTANCE = 1.5f; // 0x016AEFCC

// Magic number: rfg.exe widens this float constant (__real@3F91DF3300000000) to turn the tables' degrees into radians.
static const float DEGREES_TO_RADIANS = 0.017453f;

// Magic numbers: a mover stays while the player is within 20 metres, or within 100 metres and it is on screen.
static const float MOVER_KEEP_DIST_SQRD = 400.0f;
static const float MOVER_VISIBLE_DIST_SQRD = 10000.0f;

// rfg.exe rounds road_dist down this way before using it as a road point index (the inlined SSE2 floor).
static int road_dist_to_index(float road_dist)
{
    return (int)(float)floor((double)road_dist);
}

// 0x00A1BE20 (:200)
static bool rfgmover_is_offscreen(const rfg_mover* rfgmp)
{
    if (!rfgmp->obj_flags.streamed)
    {
        return true;
    }
    vector bmin;
    vector bmax;
    rfgmp->get_local_bbox(bmin, bmax);
    bmin *= 1.25f; // Magic number: a quarter of margin around the box
    bmax *= 1.25f;
    return game_render_cull_oriented_bounding_box(bmin, bmax, rfgmp->pos, rfgmp->orient);
}

// 0x00A1BF00 (:218). rfg.exe: LTCG, rfgmp in ESI. True when there is nothing left to wait for. Original: true with no
// player too, with the mover left standing.
static bool clean_up_roadblock_mover(rfg_mover* rfgmp)
{
    if (rfgmp == nullptr)
    {
        return false;
    }
    if (Player == nullptr)
    {
        return true;
    }
    const float dist = vec_dist_squared(Player->pos, rfgmp->pos);
    if (dist < MOVER_KEEP_DIST_SQRD)
    {
        return false;
    }
    if (dist <= MOVER_VISIBLE_DIST_SQRD && !rfgmover_is_offscreen(rfgmp))
    {
        return false;
    }
    World.destroy_object(rfgmp, false);
    return true;
}

// 0x00A3D8E0 (:253). rfg.exe: LTCG, objp in EAX. No ported caller, as in rfg.exe, where the calls inline it.
static bool clean_up_roadblock_mover(object* objp)
{
    if (objp == nullptr)
    {
        return false;
    }
    if (objp->get_obj_type() == OT_MOVER && objp->get_sub_type() == OT_SUB_MOVER_RFG)
    {
        return clean_up_roadblock_mover(static_cast<rfg_mover*>(objp));
    }
    return true;
}

// 0x00A3E140 (:1492). rfg.exe: LTCG, roadblock_obj_handle in EAX.
static bool clean_up_roadblock_mover(unsigned int roadblock_obj_handle)
{
    if (roadblock_obj_handle != 0xFFFFFFFF)
    {
        object* objp = World.object_from_handle(roadblock_obj_handle);
        if (objp != nullptr && objp->get_obj_type() == OT_MOVER && objp->get_sub_type() == OT_SUB_MOVER_RFG)
        {
            return clean_up_roadblock_mover(static_cast<rfg_mover*>(objp));
        }
    }
    return true;
}

// A mover too close to remove now goes when it next streams out, or from the clean up list. rfg.exe inlines it twice in
// cleanup_roadblock.
static void defer_mover_clean_up(object* objp)
{
    objp->obj_flags.no_save = 1;
    objp->obj_flags.destroy_on_stream = 1;
    if (!objp->obj_flags.streamed)
    {
        World.destroy_object(objp, false);
    }
    Roadblock_clean_up_list.add(objp->get_handle());
}

// 0x00A6A510 (:324)
roadblock_formation_info* find_roadblock_formation(const char* roadblock_name)
{
    for (roadblock_formation_info* info = Roadblock_formation_pool.get_first(0); info != nullptr; info = Roadblock_formation_pool.get_next(info))
    {
        if (v_stricmp(roadblock_name, info->formation_name) == 0)
        {
            return info;
        }
    }
    return nullptr;
}

// 0x00A6A580 (:341)
roadblock_data_type* find_roadblock_info(const char* roadblock_name)
{
    if (roadblock_name == nullptr)
    {
        return nullptr;
    }
    const unsigned int crc = crc_stri(roadblock_name, 0);
    for (roadblock_data_type* info = Roadblock_type_pool.get_first(0); info != nullptr; info = Roadblock_type_pool.get_next(info))
    {
        if (crc == info->name_crc && v_stricmp(roadblock_name, info->roadblock_name) == 0)
        {
            return info;
        }
    }
    return nullptr;
}

// 0x00A1BF70 (:363). rfg.exe: LTCG, objp in EDI.
static void destroy_all_child_objects(object* objp)
{
    object* child = objp->child_ptr;
    while (child != nullptr)
    {
        object* next = child->child_next;
        const bool last = next == objp->child_ptr;
        World.destroy_object(child, false);
        child = last ? nullptr : next;
    }
}

// 0x00A3D860 (object_roadblock_node_internal.h:123)
roadblock_squad_info::roadblock_squad_info()
{
    reset();
}

// 0x00A1BFA0 (:377)
void roadblock_squad_info::reset()
{
    offset_pos.zero();
    rotation = 0.0f;
    squad_def = nullptr;
    veh_group = nullptr;
    squad_flags.allow_pursuit = false;
    squad_flags.stay_in_vehicle = false;
}

// 0x00A92B10 (:388). stay_in_vehicle is read only when there is a vehicle type.
bool roadblock_squad_info::load(xml_element* xml_root)
{
    reset();
    xml_element* element = xml_find(xml_root, "squad_def");
    if (element == nullptr)
    {
        return false;
    }
    squad_def = squad_definition_from_name(element->text);
    if (squad_def == nullptr)
    {
        return false;
    }
    element = xml_find(xml_root, "offset_x");
    if (element == nullptr)
    {
        return false;
    }
    const float offset_x = xml_get_float(element, 0.0f);
    element = xml_find(xml_root, "offset_z");
    if (element == nullptr)
    {
        return false;
    }
    offset_pos.x = offset_x;
    offset_pos.z = xml_get_float(element, 0.0f);
    offset_pos.y = 0.0f;
    element = xml_find(xml_root, "vehicle_type");
    if (element != nullptr)
    {
        veh_group = sog_get_vehicle_group(element->text);
        if (veh_group == nullptr)
        {
            return false;
        }
        element = xml_find(xml_root, "stay_in_vehicle");
        if (element != nullptr)
        {
            squad_flags.stay_in_vehicle = xml_get_bool(element, false);
        }
    }
    element = xml_find(xml_root, "allow_pursuit");
    if (element != nullptr)
    {
        squad_flags.allow_pursuit = xml_get_bool(element, false);
    }
    element = xml_find(xml_root, "offset_rotation");
    if (element == nullptr)
    {
        return false;
    }
    rotation = (float)((double)xml_get_float(element, 0.0f) * (double)DEGREES_TO_RADIANS);
    return true;
}

// 0x00A1C0D0. Original: leaves the members unset.
roadblock_barrier_info::roadblock_barrier_info()
{
}

// 0x00A1BFC0 (:449)
void roadblock_barrier_info::reset()
{
    offset_pos.zero();
    rotation = 0.0f;
}

// 0x00A1BFE0 (:455)
bool roadblock_barrier_info::load(xml_element* xml_root)
{
    reset();
    xml_element* element = xml_find(xml_root, "offset_x");
    if (element == nullptr)
    {
        return false;
    }
    const float offset_x = xml_get_float(element, 0.0f);
    element = xml_find(xml_root, "offset_z");
    if (element == nullptr)
    {
        return false;
    }
    const float offset_z = xml_get_float(element, 0.0f);
    element = xml_find(xml_root, "offset_rotation");
    if (element == nullptr)
    {
        return false;
    }
    const float degrees = xml_get_float(element, 0.0f);
    offset_pos.x = offset_x;
    offset_pos.y = 0.0f;
    rotation = (float)((double)degrees * (double)DEGREES_TO_RADIANS);
    offset_pos.z = offset_z;
    return true;
}

// 0x00A3D840 (object_roadblock_node_internal.h:85)
roadblock_turret_info::roadblock_turret_info()
{
    reset();
}

// 0x00A1C0B0 (:488)
void roadblock_turret_info::reset()
{
    t_info = nullptr;
    offset_pos.zero();
    rotation = 0.0f;
}

// 0x00A3D900 (:495)
bool roadblock_turret_info::load(xml_element* xml_root)
{
    reset();
    xml_element* element = xml_find(xml_root, "turret_type");
    if (element == nullptr)
    {
        return false;
    }
    t_info = turret_find_info(element->text);
    if (t_info == nullptr)
    {
        return false;
    }
    element = xml_find(xml_root, "offset_x");
    if (element == nullptr)
    {
        return false;
    }
    const float offset_x = xml_get_float(element, 0.0f);
    element = xml_find(xml_root, "offset_z");
    if (element == nullptr)
    {
        return false;
    }
    const float offset_z = xml_get_float(element, 0.0f);
    element = xml_find(xml_root, "offset_rotation");
    if (element == nullptr)
    {
        return false;
    }
    const float degrees = xml_get_float(element, 0.0f);
    offset_pos.x = offset_x;
    offset_pos.y = 0.0f;
    rotation = (float)((double)degrees * (double)DEGREES_TO_RADIANS);
    offset_pos.z = offset_z;
    return true;
}

// 0x00A6A450 (object_roadblock_node_internal.h:103)
roadblock_formation_info::roadblock_formation_info()
{
    reset();
}

// 0x00A3DA00 (:536)
void roadblock_formation_info::reset()
{
    formation_name = nullptr;
    barricade_info_list.clear();
    turret_info_list.clear();
}

// 0x00A6A620 (:544). A formation needs a barricade; turrets are optional.
bool roadblock_formation_info::load(const char* new_formation_name, xml_element* root)
{
    reset();
    if (new_formation_name == nullptr)
    {
        return false;
    }
    formation_name = new_formation_name;
    xml_element* grid = xml_find(root, "barricade_pos_grid");
    if (grid == nullptr)
    {
        return false;
    }
    for (xml_element* entry = grid->elements; entry != nullptr; entry = entry->next)
    {
        roadblock_barrier_info barrier;
        barrier.reset();
        if (barrier.load(entry))
        {
            barricade_info_list.add(barrier);
        }
    }
    grid = xml_find(root, "turret_pos_grid");
    if (grid != nullptr)
    {
        for (xml_element* entry = grid->elements; entry != nullptr; entry = entry->next)
        {
            roadblock_turret_info turret_entry;
            if (turret_entry.load(entry))
            {
                turret_info_list.add(turret_entry);
            }
        }
    }
    return barricade_info_list.size() > 0;
}

// 0x00AAB320 (object_roadblock_node_internal.h:149)
roadblock_data_type::roadblock_data_type()
{
    reset();
}

// 0x00A92C80 (:588)
void roadblock_data_type::reset()
{
    roadblock_name = nullptr;
    name_crc = 0;
    formation_info = nullptr;
    for (int i = 0; i < squad_list.size(); i++)
    {
        Roadblock_squad_pool.free(const_cast<roadblock_squad_info*>(squad_list[i]));
    }
    squad_list.clear();
}

// 0x00A92D70 (:607). Original quirk kept: barrier_pattern is optional here, though place_bbox reads formation_info
// unchecked. A failed load holds no squads: reset frees the old ones, and it fails only when no squad could be loaded.
bool roadblock_data_type::load(const char* new_roadblock_name, xml_element* xml_root)
{
    if (new_roadblock_name == nullptr || xml_root == nullptr)
    {
        return false;
    }
    reset();
    roadblock_name = new_roadblock_name;
    name_crc = crc_stri(new_roadblock_name, 0);
    xml_element* pattern = xml_find(xml_root, "barrier_pattern");
    if (pattern != nullptr)
    {
        formation_info = find_roadblock_formation(pattern->text);
        if (formation_info == nullptr)
        {
            return false;
        }
    }
    xml_element* grid = xml_find(xml_root, "squad_grid");
    if (grid == nullptr)
    {
        return false;
    }
    for (xml_element* entry = grid->elements; entry != nullptr; entry = entry->next)
    {
        roadblock_squad_info* squad = Roadblock_squad_pool.alloc(0);
        if (squad == nullptr)
        {
            continue;
        }
        squad->reset();
        if (squad->load(entry))
        {
            const roadblock_squad_info* const_squad = squad;
            squad_list.add(const_squad);
        }
        else
        {
            Roadblock_squad_pool.free(squad);
        }
    }
    return squad_list.size() > 0;
}

// 0x00A3D820 (.h:44)
roadblock_squad_instance_info::roadblock_squad_instance_info()
{
    reset();
}

// 0x00A1BDA0 (.h:52)
void roadblock_squad_instance_info::reset()
{
    squad_flags.allow_pursuit = false;
    squad_flags.stay_in_vehicle = false;
    squad_handle = 0xFFFFFFFF;
    vehicle_handle = 0xFFFFFFFF;
    spawn_node_handle = 0xFFFFFFFF;
}

// 0x00A6A4A0 (object_roadblock_node_internal.h:204)
roadblock_construction_info::roadblock_construction_info()
{
    reset();
}

// 0x00A3D890 (object_roadblock_node_internal.h:215)
void roadblock_construction_info::reset()
{
    pos.zero();
    orient.make_identity();
    bbp = nullptr;
    dummy_node_list.clear();
}

// 0x00A6AEE0
roadblock_construction_info::~roadblock_construction_info()
{
}

// 0x00A1C0F0 (:663)
void types_object_roadblock_node(signed char& type, signed char& subtype)
{
    type = OT_ROADBLOCK_NODE;
    subtype = OT_SUB_UNDEFINED;
}

// 0x00A1C100 (:663)
int bpo_index_object_roadblock_node()
{
    return 1;
}

// 0x00A1C110 (:663)
const char* classname_object_roadblock_node()
{
    return "object_roadblock_node";
}

// 0x00A6A740 (:663)
object* alloc_object_roadblock_node()
{
    return object_roadblock_nodepool.alloc(0);
}

// Magic numbers: rfg.exe's arguments (0x0107BB00), the pool's bytes and capacity.
static obj_class_instantiator object_roadblock_node_instantiator("object_roadblock_node", alloc_object_roadblock_node, types_object_roadblock_node, bpo_index_object_roadblock_node, classname_object_roadblock_node, object_roadblock_node::pool_remaining, 0x5B80, 0x40);

// 0x00A1C120 (:663)
unsigned int object_roadblock_node::get_classname_hash()
{
    return object_roadblock_node_instantiator.this_hash;
}

// 0x00A3DA20 (:663)
int object_roadblock_node::pool_remaining()
{
    return object_roadblock_nodepool.get_free();
}

// 0x00A92E80 (:663)
void object_roadblock_node::pool_free()
{
    object_roadblock_nodepool.free(this);
}

// 0x00A1BDB0 (.h:66)
const char* object_roadblock_node::get_classname_static()
{
    return "object_roadblock_node";
}

// 0x00A1BDC0 (.h:66)
int object_roadblock_node::get_bpo_index_static()
{
    return 1;
}

// 0x00A6A780 (.h:66)
const char* object_roadblock_node::get_classname() const
{
    return "object_roadblock_node";
}

// 0x00A1BDD0 (.h:89)
bool object_roadblock_node::is_active() const
{
    return roadblock_flags.active != 0;
}

// 0x00A1BDE0 (.h:101)
unsigned int object_roadblock_node::get_road_handle() const
{
    return road_handle;
}

// 0x00A6A750 (:664)
object_roadblock_node::object_roadblock_node()
{
}

// 0x00A6A790
object_roadblock_node::~object_roadblock_node()
{
}

// 0x00A3DA30 (:669)
void object_roadblock_node::reset_for_reuse()
{
    memset(&roadblock_flags, 0, sizeof(roadblock_flags));
    spawn_node_list.clear();
}

// 0x00A3DA40 (:676)
void object_roadblock_node::reset_all()
{
    roadblock_type = nullptr;
    use_original_orient = false;
    road_center.zero();
    road_dist = -1.0f;
    road_fvec.zero();
    road_rvec.zero();
    road_handle = 0xFFFFFFFF;
    reset_for_reuse(); // rfg.exe inlines it
}

// 0x00AB4320 (:690)
bool object_roadblock_node::stream(bool load)
{
    if (load)
    {
        if (roadblock_flags.active)
        {
            spawn_roadblock_objects();
        }
    }
    else if (obj_flags.destroy_on_stream)
    {
        World.destroy_object(this, false);
    }
    return true;
}

// 0x00A92EC0 (:706)
void object_roadblock_node::destroy_notify()
{
    remove_constraints();
    cleanup_roadblock();
}

// 0x00AAB350 (:717)
bool object_roadblock_node::serialize(obj_prop_block& pblock, bool load)
{
    if (!object::serialize(pblock, load))
    {
        return false;
    }
    if (!load)
    {
        pblock.set(ROADBLOCK_PROP_TYPE, roadblock_type->roadblock_name);
        pblock.set(ROADBLOCK_ORIENT_FLAG, use_original_orient);
        return true;
    }
    reset_all();
    char type_name[256];
    pblock.get(ROADBLOCK_PROP_TYPE, type_name);
    roadblock_type = find_roadblock_info(type_name);
    if (roadblock_type == nullptr)
    {
        return false;
    }
    pblock.get_optional(ROADBLOCK_ORIENT_FLAG, use_original_orient);
    return build_road_data();
}

// 0x00A1C130 (:754). Magic number: zone ids from 0x80 up in the handle's third byte are managed layers.
bool object_roadblock_node::on_managed_layer() const
{
    return ((get_handle() >> 16) & 0xFF) >= 0x80;
}

// 0x00A92ED0 (:764)
void object_roadblock_node::free_this()
{
    object::free_this();
    if (roadblock_flags.active)
    {
        cleanup_roadblock();
    }
}

// 0x00A6A7B0 (:775). Once the player has passed the roadblock's front, squads allowed to pursue attack. True when released.
bool object_roadblock_node::release_squads(const player* pp)
{
    vector to_player((float)((double)pp->pos.x - (double)road_center.x), (float)((double)pp->pos.y - (double)road_center.y), (float)((double)pp->pos.z - (double)road_center.z));
    float facing;
    if (to_player.is_null())
    {
        facing = -1.0f;
    }
    else
    {
        to_player.normalize_safe();
        const double sign = roadblock_flags.reverse ? -1.0 : 1.0;
        facing = (float)((double)to_player.y * (double)road_fvec.y * sign + (double)to_player.x * (double)road_fvec.x * sign + (double)to_player.z * (double)road_fvec.z * sign);
    }
    if (facing > 0.0f)
    {
        return false;
    }
    for (int i = 0; i < spawn_node_list.size(); i++)
    {
        roadblock_squad_instance_info& info = spawn_node_list[i];
        const object_squad_spawn_node* node = info.spawn_node_handle != 0xFFFFFFFF ? World.object_from_handle<object_squad_spawn_node>(info.spawn_node_handle) : nullptr;
        if (node == nullptr)
        {
            continue;
        }
        info.squad_handle = node->get_child_squad_handle();
        info.vehicle_handle = node->get_child_vehicle_handle(); // rfg.exe inlines it
        if (!info.squad_flags.allow_pursuit || info.squad_handle == 0xFFFFFFFF)
        {
            continue;
        }
        object_squad* squad = World.object_from_handle<object_squad>(info.squad_handle);
        if (squad == nullptr)
        {
            continue;
        }
        gfm_orders_creation_params params;
        params.object_target = pp->get_handle();
        params.location_target = pp->pos;
        params.orders_type = GFM_ORDERS_TYPE_ATTACK;
        params.team = squad->override_team;
        if (params.team == HUMAN_TEAM_NONE && squad->definition != nullptr)
        {
            params.team = squad->definition->squad_team;
        }
        const unsigned int orders_handle = Orders_manager.create_new_orders(params);
        if (orders_handle != 0xFFFFFFFF && Orders_manager.get_orders_from_handle(orders_handle) != nullptr)
        {
            squad->assign_orders(orders_handle); // rfg.exe inlines it
        }
    }
    return true;
}

// 0x00A92EF0 (:842). Original quirk kept: reads Player unchecked. Magic numbers: squads go within 50 metres, a roadblock
// outside a managed layer is cleared past 400 metres.
void object_roadblock_node::do_frame()
{
    player* pp = Player;
    const float dist = vec_dist_squared(pp->pos, road_center);
    if (!roadblock_flags.squads_released && dist < 2500.0f)
    {
        roadblock_flags.squads_released = release_squads(pp);
    }
    if (!on_managed_layer() && dist > 160000.0f)
    {
        cleanup_roadblock();
    }
}

// 0x00A6AAB0 (:870). Humans, vehicles, squads and nested roadblocks should not be children here: rfg.exe flags them and
// destroys them anyway.
void object_roadblock_node::cleanup_roadblock()
{
    if (!roadblock_flags.active)
    {
        return;
    }
    farray<object*, 128> descendants;
    world_collect_descendants(this, descendants, nullptr, nullptr);
    for (int i = 0; i < descendants.size(); i++)
    {
        object* objp = descendants[i];
        switch (objp->get_obj_type())
        {
        case OT_HUMAN:
        case OT_VEHICLE:
        case OT_SQUAD:
        case OT_ROADBLOCK_NODE:
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
            World.destroy_object(objp, false);
            break;
        case OT_MOVER:
            if (objp->get_sub_type() == OT_SUB_MOVER_RFG && !clean_up_roadblock_mover(static_cast<rfg_mover*>(objp)))
            {
                defer_mover_clean_up(objp);
            }
            break;
        default:
            World.destroy_object(objp, false);
            break;
        }
    }
    const int num_movers = World.get_num_objects<rfg_mover>();
    for (int i = 0; i < num_movers; i++)
    {
        rfg_mover* rfgmp = World.get_object<rfg_mover>(i);
        if (rfgmp->was_dynamically_created() && vec_dist_squared(pos, rfgmp->pos) < 400.0f && Player != nullptr) // Magic number: 20 metres
        {
            if (!clean_up_roadblock_mover(rfgmp)) // rfg.exe inlines it
            {
                defer_mover_clean_up(rfgmp);
            }
        }
    }
    object_path_road* road = road_handle != 0xFFFFFFFF ? World.object_from_handle<object_path_road>(road_handle) : nullptr;
    if (road != nullptr)
    {
        road->remove_road_block_at_index(road_dist_to_index(road_dist));
    }
    reset_for_reuse(); // rfg.exe inlines it
}

// 0x00A6ADE0 (:964)
bool object_roadblock_node::build_road_data()
{
    // Magic number: the road must be within 60 metres.
    const object_path_road* road = object_road_get_closest_road(pos, false, road_center, road_dist, 60.0f, false, nullptr, nullptr, false);
    if (road == nullptr || road_center.is_null())
    {
        return false;
    }
    road_handle = road->get_handle();
    road->get_tangent(road_fvec, road_dist);
    road_fvec.y = 0.0f;
    if (road_fvec.is_null())
    {
        return false;
    }
    road_fvec.normalize_safe();
    road_rvec.x = road_fvec.z;
    road_rvec.y = 0.0f;
    road_rvec.z = -road_fvec.x;
    road_width = road->get_width(road_dist);
    return road_width >= 0.0f;
}

// 0x00ABA590 (:995). Original quirk kept: active stays set when the bounding box fails.
bool object_roadblock_node::spawn_roadblock_dynamic(const vector& target_pos)
{
    roadblock_construction_info construction_info;
    destroy_all_child_objects(this); // rfg.exe inlines it
    roadblock_flags.active = 1;
    if (use_original_orient)
    {
        construction_info.orient.fvec = orient.fvec;
        construction_info.orient.rvec = orient.rvec;
    }
    else
    {
        vector to_target((float)((double)target_pos.x - (double)road_center.x), (float)((double)target_pos.y - (double)road_center.y), (float)((double)target_pos.z - (double)road_center.z));
        if (to_target.is_null())
        {
            to_target = FORWARD_VECTOR;
        }
        else
        {
            to_target.normalize();
        }
        const double facing = (double)to_target.y * (double)road_fvec.y + (double)to_target.x * (double)road_fvec.x + (double)to_target.z * (double)road_fvec.z;
        roadblock_flags.reverse = (float)facing < 0.0f;
        if (roadblock_flags.reverse)
        {
            construction_info.orient.fvec.set(-road_fvec.x, -road_fvec.y, -road_fvec.z);
            construction_info.orient.rvec.set(-road_rvec.x, -road_rvec.y, -road_rvec.z);
        }
        else
        {
            construction_info.orient.fvec = road_fvec;
            construction_info.orient.rvec = road_rvec;
        }
    }
    construction_info.pos = road_center;
    construction_info.orient.uvec = UP_VECTOR;
    place_bbox(construction_info);
    if (construction_info.bbp == nullptr)
    {
        return false;
    }
    place_barricades(construction_info);
    place_turrets(construction_info);
    place_guard_nodes(construction_info);
    place_squads(construction_info);
    object_path_road* road = road_handle != 0xFFFFFFFF ? World.object_from_handle<object_path_road>(road_handle) : nullptr;
    if (road != nullptr)
    {
        road->road_block_road(road_dist_to_index(road_dist)); // rfg.exe inlines it
    }
    return true;
}

// 0x00A6AEF0 (:1055)
void object_roadblock_node::destroy_all_objects()
{
    farray<object*, 128> descendants;
    world_collect_descendants(this, descendants, nullptr, nullptr);
    for (int i = 0; i < descendants.size(); i++)
    {
        World.destroy_object(descendants[i], false);
    }
}

// 0x00ABA980 (:1073)
bool object_roadblock_node::start(const vector& target_pos)
{
    return roadblock_flags.active ? true : spawn_roadblock_dynamic(target_pos);
}

// 0x00A6AF60 (:1084). No caller in rfg.exe.
void object_roadblock_node::place_single_squad_spawn(const vector& squad_pos, const matrix& squad_orient, const squad_definition* squad_def, const vehicle_spawn_group_info* veh_group, const roadblock_squad_info* squad_info, unsigned int guard_obj_handle)
{
    if (guard_obj_handle == 0xFFFFFFFF || squad_def == nullptr || spawn_node_list.size() >= spawn_node_list.capacity())
    {
        return;
    }
    obj_prop_block block(squad_pos, squad_orient, nullptr);
    block.set(SPAWN_NODE_SQUAD_DEF_NAME, squad_def->name);
    if (veh_group != nullptr)
    {
        block.set(SQUAD_PROP_SQUAD_VEHICLE, true);
        block.set(SQUAD_PROP_VEHICLE_TYPE, veh_group->group_name);
    }
    const gfm_orders_types orders_type = squad_info->squad_flags.stay_in_vehicle && veh_group != nullptr ? GFM_ORDERS_TYPE_ROADBLOCK : GFM_ORDERS_TYPE_DEFEND;
    block.set(SPAWN_NODE_DEFAULT_ORDERS_ID, orders_type);
    const object_squad_spawn_node* node = World.create_object<object_squad_spawn_node>(block, guard_obj_handle);
    if (node != nullptr)
    {
        roadblock_squad_instance_info info;
        info.spawn_node_handle = node->get_handle();
        info.squad_flags = squad_info->squad_flags;
        spawn_node_list.add(info);
    }
}

// 0x00A6B0C0 (:1125). Stops at the first node it cannot create.
void object_roadblock_node::place_squads(roadblock_construction_info& construction_info)
{
    if (construction_info.bbp == nullptr)
    {
        return;
    }
    for (int i = 0; i < roadblock_type->squad_list.size(); i++)
    {
        const roadblock_squad_info* squad_info = roadblock_type->squad_list[i];
        vector node_pos = squad_info->offset_pos;
        vec_change_to_global_coords(node_pos, construction_info.pos, construction_info.orient);
        rfg_snap_to_ground(node_pos);
        matrix node_orient = construction_info.orient;
        node_orient.rotate_around_local_y(squad_info->rotation);
        obj_prop_block block(node_pos, node_orient, nullptr);
        block.set(SPAWN_NODE_SQUAD_DEF_NAME, squad_info->squad_def->name);
        if (squad_info->veh_group != nullptr)
        {
            block.set(SQUAD_PROP_SQUAD_VEHICLE, true);
            block.set(SQUAD_PROP_VEHICLE_TYPE, squad_info->veh_group->group_name);
        }
        const gfm_orders_types orders_type = squad_info->squad_flags.stay_in_vehicle && squad_info->veh_group != nullptr ? GFM_ORDERS_TYPE_ROADBLOCK : GFM_ORDERS_TYPE_DEFEND;
        block.set(SPAWN_NODE_DEFAULT_ORDERS_ID, orders_type);
        const object_squad_spawn_node* node = World.create_object<object_squad_spawn_node>(block, construction_info.bbp->get_handle());
        if (node == nullptr)
        {
            return;
        }
        roadblock_squad_instance_info info;
        info.squad_flags = squad_info->squad_flags;
        info.spawn_node_handle = node->get_handle();
        spawn_node_list.add(info);
    }
}

// 0x00A3DAA0 (:1184). One guard node behind each barricade.
void object_roadblock_node::place_guard_nodes(roadblock_construction_info& construction_info)
{
    if (construction_info.bbp == nullptr)
    {
        return;
    }
    for (int i = 0; i < construction_info.dummy_node_list.size(); i++)
    {
        const object_dummy* dummy = construction_info.dummy_node_list[i];
        const vector node_pos((float)((double)dummy->pos.x - (double)GUARD_NODE_OFFSET_DISTANCE * (double)dummy->orient.fvec.x), (float)((double)dummy->pos.y - (double)dummy->orient.fvec.y * (double)GUARD_NODE_OFFSET_DISTANCE), (float)((double)dummy->pos.z - (double)dummy->orient.fvec.z * (double)GUARD_NODE_OFFSET_DISTANCE));
        obj_prop_block block(node_pos, dummy->orient, nullptr);
        const int zero = 0;
        block.set(GUARD_NODE_PROP_GROUP_ID, zero);
        block.set(GUARD_NODE_PROP_ANGLE_LEFT, zero);
        block.set(GUARD_NODE_PROP_ANGLE_RIGHT, zero);
        World.create_object<object_guard_node>(block, construction_info.bbp->get_handle());
    }
}

// 0x00A3DC40 (:1241). The box covers the road's width and every placement, plus 5 metres each way.
void object_roadblock_node::place_bbox(roadblock_construction_info& construction_info)
{
    static const vector STANDARD_BBOX_OFFSET_SIZE(5.0f, 5.0f, 5.0f); // 0x035B59AC
    bbox box;
    box.min.set(-road_width, -road_width, -road_width);
    box.max.set(road_width, road_width, road_width);
    const roadblock_formation_info* formation = roadblock_type->formation_info;
    for (int i = 0; i < formation->barricade_info_list.size(); i++)
    {
        bbox_expand(box, formation->barricade_info_list[i].offset_pos);
    }
    for (int i = 0; i < formation->turret_info_list.size(); i++)
    {
        bbox_expand(box, formation->turret_info_list[i].offset_pos);
    }
    for (int i = 0; i < roadblock_type->squad_list.size(); i++)
    {
        bbox_expand(box, roadblock_type->squad_list[i]->offset_pos);
    }
    box.max.x += STANDARD_BBOX_OFFSET_SIZE.x;
    box.max.y += STANDARD_BBOX_OFFSET_SIZE.y;
    box.max.z += STANDARD_BBOX_OFFSET_SIZE.z;
    box.min.x -= STANDARD_BBOX_OFFSET_SIZE.x;
    box.min.y -= STANDARD_BBOX_OFFSET_SIZE.y;
    box.min.z -= STANDARD_BBOX_OFFSET_SIZE.z;
    obj_prop_block block(construction_info.pos, construction_info.orient, nullptr); // rfg.exe writes the same property by hand
    block.set(BOUNDING_BOX_PROP_BOX, box);
    construction_info.bbp = World.create_object<object_bounding_box>(block, get_handle());
}

// 0x00A3DFC0 (:1276)
void object_roadblock_node::place_turrets(roadblock_construction_info& construction_info)
{
    const roadblock_formation_info* formation = roadblock_type->formation_info;
    if (formation == nullptr || construction_info.bbp == nullptr)
    {
        return;
    }
    for (int i = 0; i < formation->turret_info_list.size(); i++)
    {
        const roadblock_turret_info& turret_entry = formation->turret_info_list[i];
        vector node_pos = turret_entry.offset_pos;
        vec_change_to_global_coords(node_pos, construction_info.pos, construction_info.orient);
        rfg_snap_to_ground(node_pos);
        matrix node_orient = construction_info.orient;
        node_orient.rotate_around_local_y(turret_entry.rotation);
        obj_prop_block block(node_pos, node_orient, nullptr);
        const float spawn_prob = 1.0f; // Magic number: always
        block.set(TURRET_PROP_TINFO_NAME, turret_entry.t_info->name);
        block.set(TURRET_PROP_ROTATION_LIMIT, DEFAULT_TURRET_ROTATIONAL_LIMIT);
        block.set(SPAWN_NODE_PROP_SPAWN_PROB, spawn_prob);
        World.create_object<object_turret_spawn_node>(block, construction_info.bbp->get_handle());
    }
}

// 0x00AB4360 (:1323)
void object_roadblock_node::place_barricades(roadblock_construction_info& construction_info)
{
    const roadblock_formation_info* formation = roadblock_type->formation_info;
    if (formation == nullptr || formation->barricade_info_list.size() < 1 || construction_info.bbp == nullptr)
    {
        return;
    }
    for (int i = 0; i < formation->barricade_info_list.size(); i++)
    {
        const roadblock_barrier_info& barrier = formation->barricade_info_list[i];
        vector node_pos = barrier.offset_pos;
        vec_change_to_global_coords(node_pos, construction_info.pos, construction_info.orient);
        rfg_snap_to_ground(node_pos);
        matrix node_orient = construction_info.orient;
        node_orient.rotate_around_local_y(barrier.rotation);
        obj_prop_block block(node_pos, node_orient, nullptr);
        const dummy_object_types dummy_type = DUMMY_OBJECT_TYPE_ACTIVITY_BARRICADE;
        block.set(DUMMY_OBJECT_PROP_TYPE_INTERNAL, dummy_type);
        object_dummy* dummy = World.create_object<object_dummy>(block, construction_info.bbp->get_handle());
        if (dummy != nullptr)
        {
            construction_info.dummy_node_list.add(dummy);
        }
    }
    if (obj_flags.streamed)
    {
        spawn_roadblock_objects();
    }
}

// 0x00AAB430 (:1377)
void object_roadblock_node::spawn_roadblock_objects()
{
    if (!roadblock_flags.active)
    {
        return;
    }
    farray<object_dummy*, 256> dummies;
    world_collect_descendants<object_dummy>(this, dummies, nullptr, nullptr);
    for (int i = 0; i < dummies.size(); i++)
    {
        dummies[i]->create_dummy_object();
    }
}

// 0x00A92FB0 (:1406). A slot of that name, or a new one. Original: returning early leaves the table open.
static void load_roadblock_types()
{
    xml_element* root = xml_table_parse("roadblock_types.xtbl");
    if (root == nullptr)
    {
        return;
    }
    for (xml_element* entry = xml_find(root, "roadblock_data"); entry != nullptr; entry = xml_find_next(root, entry, "roadblock_data"))
    {
        xml_element* name = xml_find(entry, "Name");
        if (name == nullptr)
        {
            continue;
        }
        roadblock_data_type* info = find_roadblock_info(name->text);
        const char* roadblock_name;
        if (info == nullptr)
        {
            roadblock_name = Roadblock_string_pool.add_unique(name->text);
            if (roadblock_name == nullptr)
            {
                return;
            }
            info = Roadblock_type_pool.alloc(0);
            if (info == nullptr)
            {
                return;
            }
        }
        else
        {
            roadblock_name = info->roadblock_name;
        }
        if (!info->load(roadblock_name, entry))
        {
            Roadblock_type_pool.free(info);
        }
    }
    xml_table_close(root);
}

// 0x00A93070 (:1451). Original: returning early leaves the table open.
static void load_roadblock_formations()
{
    xml_element* root = xml_table_parse("roadblock_formation.xtbl");
    if (root == nullptr)
    {
        return;
    }
    for (xml_element* entry = xml_find(root, "formation_info"); entry != nullptr; entry = xml_find_next(root, entry, "formation_info"))
    {
        xml_element* name = xml_find(entry, "Name");
        if (name == nullptr)
        {
            continue;
        }
        roadblock_formation_info* info = find_roadblock_formation(name->text);
        const char* formation_name;
        if (info == nullptr)
        {
            formation_name = Roadblock_string_pool.add_unique(name->text);
            if (formation_name == nullptr)
            {
                return;
            }
            info = Roadblock_formation_pool.alloc(0);
            if (info == nullptr)
            {
                return;
            }
        }
        else
        {
            formation_name = info->formation_name;
        }
        if (!info->load(formation_name, entry))
        {
            Roadblock_formation_pool.free(info);
        }
    }
    xml_table_close(root);
}

// 0x00A3E170 (:1504)
static void do_frame_clean_up()
{
    int i = 0;
    while (i < Roadblock_clean_up_list.size())
    {
        const unsigned int handle = Roadblock_clean_up_list[i];
        rfg_mover* rfgmp = handle != 0xFFFFFFFF ? World.object_from_handle<rfg_mover>(handle) : nullptr;
        if (rfgmp != nullptr && Player != nullptr)
        {
            const float dist = vec_dist_squared(Player->pos, rfgmp->pos);
            if (dist < MOVER_KEEP_DIST_SQRD || (dist <= MOVER_VISIBLE_DIST_SQRD && !rfgmover_is_offscreen(rfgmp)))
            {
                i++;
                continue;
            }
            World.destroy_object(rfgmp, false);
        }
        // Deviation from rfg.exe: remove_unsorted stands for its inlined swap. Same effect.
        Roadblock_clean_up_list.remove_unsorted(i);
    }
}

// 0x00AAB490 (:1532). Original: the types go back to the pool unreset, then the squad pool is cleared under them.
void roadblock_system_init()
{
    if (Roadblock_name_string_pool_data == nullptr)
    {
        char* address = static_cast<char*>(Permanent_mempool.alloc(0x200, 0)); // Magic number: 512 bytes of names
        if (address != nullptr)
        {
            Roadblock_name_string_pool_data = address;
            Roadblock_string_pool.init(address, 0x200, nullptr, 0, 0);
        }
        if (Roadblock_name_string_pool_data == nullptr)
        {
            return;
        }
    }
    for (roadblock_data_type* info = Roadblock_type_pool.get_first(0); info != nullptr; info = Roadblock_type_pool.free(info))
    {
    }
    Roadblock_squad_pool.clear();
    for (roadblock_formation_info* info = Roadblock_formation_pool.get_first(0); info != nullptr; info = Roadblock_formation_pool.free(info))
    {
    }
    load_roadblock_formations();
    load_roadblock_types();
}

// 0x00A3E250 (:1565)
void roadblock_level_init_pre()
{
    Roadblock_clean_up_list.clear();
    Roadblock_clean_up_period.set_rand(1000, 3000); // Magic numbers: one to three seconds
}

// 0x00A93130 (:1572)
void roadblock_do_frame()
{
    if (Multiplayer)
    {
        return;
    }
    const int num_nodes = World.get_num_objects<object_roadblock_node>();
    for (int i = 0; i < num_nodes; i++)
    {
        object_roadblock_node* node = World.get_object<object_roadblock_node>(i);
        if (node->is_active())
        {
            node->do_frame();
        }
    }
    if (Roadblock_clean_up_period.elapsed())
    {
        if (Roadblock_clean_up_list.size() > 0)
        {
            do_frame_clean_up();
        }
        Roadblock_clean_up_period.set_rand(1000, 3000); // Magic numbers: one to three seconds
    }
}

// 0x00ABA9A0 (:1597). Original quirk kept: reads Player unchecked.
bool roadblock_spawn(unsigned int roadblock_handle)
{
    if (roadblock_handle == 0xFFFFFFFF)
    {
        return false;
    }
    object_roadblock_node* node = World.object_from_handle<object_roadblock_node>(roadblock_handle);
    return node != nullptr && node->start(Player->pos); // rfg.exe inlines start
}

// 0x00A6B2B0 (:1613)
void roadblock_destroy_all_created_objects()
{
    const int num_turrets = World.get_num_objects<turret>();
    for (int i = 0; i < num_turrets; i++)
    {
        turret* tp = World.get_object<turret>(i);
        if (tp->is_roadblock_turret())
        {
            World.destroy_object(tp, false);
        }
    }
    const int num_movers = World.get_num_objects<rfg_mover>();
    for (int i = 0; i < num_movers; i++)
    {
        rfg_mover* rfgmp = World.get_object<rfg_mover>(i);
        if (rfgmp->was_dynamically_created())
        {
            World.destroy_object(rfgmp, false);
        }
    }
}

// 0x00A6B370 (:1633)
void roadblock_destroy_all()
{
    const int num_nodes = World.get_num_objects<object_roadblock_node>();
    for (int i = 0; i < num_nodes; i++)
    {
        World.get_object<object_roadblock_node>(i)->destroy_all_objects();
    }
}

// 0x00A6B3C0 (:1645)
const object_roadblock_node* roadblock_nearest_block(unsigned int road_handle, const vector& pos)
{
    const object_roadblock_node* nearest = nullptr;
    float nearest_dist = 3.4028235e+38f; // Magic number: FLT_MAX
    for (int i = 0; i < World.get_num_objects<object_roadblock_node>(); i++)
    {
        const object_roadblock_node* node = World.get_object<object_roadblock_node>(i);
        if (!node->is_active() || node->get_road_handle() != road_handle)
        {
            continue;
        }
        const float dist = vec_dist_squared(pos, node->pos);
        if (dist < nearest_dist)
        {
            nearest = node;
            nearest_dist = dist;
        }
    }
    return nearest;
}
