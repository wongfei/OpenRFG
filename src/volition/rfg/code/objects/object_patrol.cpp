// Original: volition/rfg/code/objects/object_patrol.cpp
//
// Patrols (stub_impl.md S9): a named path given by its root nav point. The list of its nodes is rebuilt by walking the
// links from the root each time, choosing at branches.

#include "volition/rfg/code/objects/object_patrol.h"

#include "volition/rfg/code/destruction/rfg_mover.h"
#include "volition/rfg/code/gameplay/gameflow/gfm_orders_manager.h"
#include "volition/rfg/code/level/rfg_prop_name_shared.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/object_navpoint.h"
#include "volition/rfg/code/objects/object_path_road.h"
#include "volition/rfg/code/objects/object_prop_names_shared.h"
#include "volition/rfg/code/objects/object_spawn_node.h"
#include "volition/vlib/code/math/ix.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/memory/object_pool.h"

extern gfm_orders_manager Orders_manager;

// Magic numbers: rfg.exe's pool; a location within 3 m of a node counts as at it; nav point type 1 is "patrol"; a road
// within 60 m makes a node near a road; a bridge mover more than 1% destroyed is not crossed.
static object_pool<object_patrol,72,1> object_patrolpool; // 0x033C31B0
static const float NEAR_PATROL_POINT_DIST_SQRD = 9.0f; // 0x016AD290
static const float ON_SEGMENT_DIST_SQRD = 3.0f;
static const signed char NAVPOINT_TYPE_PATROL = 1;
static const float NEAR_ROAD_DIST = 60.0f;
static const float BRIDGE_DESTROYED_PERCENT = 0.01f;

// 0x00A69410 (:121): the nodes in path order from the root. True if the path has an end (a dead end or a branch's end),
// false if it came back to the root or it is malformed: a node with a type other than patrol, or too many links that are
// not branch links. Branch choices are remembered in next_path_navpoint, so a list rebuilt later takes the same way.
bool build_patrol_point_list(navpoint* root_nav, farray<navpoint*,512>& nav_list)
{
    nav_list.clear();
    if (root_nav == nullptr || root_nav->type != NAVPOINT_TYPE_PATROL)
    {
        return false;
    }
    nav_list.add(root_nav);
    int root_links = root_nav->num_links;
    if (root_links == 0)
    {
        return false;
    }
    navpoint* next;
    if (root_links == 1)
    {
        next = root_nav->links[0];
    }
    else
    {
        int branches = 0;
        navpoint* bridge = nullptr;
        for (int i = 0; i < root_links; i++)
        {
            if (root_nav->links[i]->nav_flags.start_branch)
            {
                branches++;
                if (root_nav->links[i]->nav_flags.bridge)
                {
                    bridge = root_nav->links[i];
                }
            }
        }
        if (branches != root_links)
        {
            return false;
        }
        next = root_nav->next_path_navpoint;
        bool known = false;
        if (next != nullptr)
        {
            for (int i = 0; i < root_links; i++)
            {
                if (root_nav->links[i] == next)
                {
                    known = true;
                    break;
                }
            }
        }
        if (!known)
        {
            next = bridge != nullptr ? bridge : root_nav->links[int_rand_range(0, branches - 1)];
            root_nav->next_path_navpoint = next;
        }
    }
    navpoint* prev = root_nav;
    root_nav->prev_path_navpoint = nullptr;
    do
    {
        navpoint* node = next;
        if (node == nullptr || node->type != NAVPOINT_TYPE_PATROL)
        {
            return false;
        }
        nav_list.add(node);
        int links = node->num_links;
        if (links < 1)
        {
            return false;
        }
        if (links > 2)
        {
            int branch_links = 0;
            for (int i = 0; i < links; i++)
            {
                if (node->links[i]->nav_flags.start_branch || node->links[i]->nav_flags.end_branch)
                {
                    branch_links++;
                }
            }
            if (branch_links < links - 1)
            {
                return false;
            }
        }
        if (links == 1)
        {
            return node->nav_flags.single_node_link;
        }
        if (links == 2)
        {
            int end_links = 0;
            for (int i = 0; i < 2; i++)
            {
                if (node->links[i] != nullptr && node->links[i]->nav_flags.end_branch)
                {
                    end_links++;
                }
            }
            if (end_links == 2)
            {
                return true;
            }
            next = node->links[0] == prev ? node->links[1] : node->links[0];
        }
        else
        {
            next = node->next_path_navpoint;
            bool known = false;
            if (next != nullptr)
            {
                for (int i = 0; i < links; i++)
                {
                    if (node->links[i] == next)
                    {
                        known = true;
                        break;
                    }
                }
            }
            if (!known)
            {
                next = nullptr;
                int branches = 0;
                navpoint* plain = nullptr;
                for (int i = 0; i < links; i++)
                {
                    navpoint* link = node->links[i];
                    if (link == prev)
                    {
                        continue;
                    }
                    if (link->nav_flags.start_branch)
                    {
                        if (link->nav_flags.bridge)
                        {
                            next = link;
                        }
                        branches++;
                    }
                    else if (!link->nav_flags.end_branch)
                    {
                        plain = link;
                    }
                }
                if (branches < 1)
                {
                    next = plain;
                }
                else if (next == nullptr)
                {
                    // Original: stays on the node itself if the random pick finds nothing.
                    next = node;
                    int pick = int_rand_range(0, branches - 1);
                    int seen = 0;
                    for (int i = 0; i < node->num_links; i++)
                    {
                        if (node->links[i] != prev && node->links[i]->nav_flags.start_branch)
                        {
                            if (pick == seen)
                            {
                                next = node->links[i];
                                break;
                            }
                            seen++;
                        }
                    }
                }
                node->next_path_navpoint = next;
                node->prev_path_navpoint = prev;
            }
        }
        if (next == root_nav)
        {
            return false;
        }
        prev = node;
    } while (nav_list.size() < nav_list.capacity());
    // Original: Int3 critical function here (more than 512 nodes).
    return false;
}

// One segment of the path as find_next_path_node sees it.
struct patrol_segment
{
    navpoint* start;
    navpoint* end;
    float node_dist_sqrd;
    float seg_dist_sqrd;
    float seg_u;
    vector closest;
    unsigned int next_handle;
};

// 0x00A3D120 (:202): the node to walk to from a position: past a node within 3 m, else the far end of the nearest segment.
static unsigned int find_next_path_node(farray<navpoint*,512>& nav_list, const vector& npc_pos, bool circular_path)
{
    int num = nav_list.size();
    if (num == 1)
    {
        return nav_list[0]->get_handle();
    }
    patrol_segment segments[512];
    int last = num - 1;
    for (int i = 0; i < last; i++)
    {
        segments[i].start = nav_list[i];
        segments[i].end = nav_list[i + 1];
        segments[i].node_dist_sqrd = vec_dist_squared(segments[i].start->pos, npc_pos);
        if (segments[i].node_dist_sqrd < NEAR_PATROL_POINT_DIST_SQRD)
        {
            return segments[i].end->get_handle();
        }
    }
    int count = last;
    if (circular_path)
    {
        segments[last].start = nav_list[last];
        segments[last].end = nav_list[0];
        segments[last].node_dist_sqrd = vec_dist_squared(segments[last].start->pos, npc_pos);
        count = num;
        if (segments[last].node_dist_sqrd < NEAR_PATROL_POINT_DIST_SQRD)
        {
            return segments[last].end->get_handle();
        }
    }
    patrol_segment* best = segments;
    for (int i = 0; i < count; i++)
    {
        patrol_segment& seg = segments[i];
        vector p0 = seg.start->pos;
        vector p1 = seg.end->pos;
        seg.seg_u = ix_closest_point_on_line_segment(seg.closest, npc_pos, p0, p1);
        seg.next_handle = seg.seg_u > 0.0f ? seg.end->get_handle() : seg.start->get_handle();
        seg.seg_dist_sqrd = vec_dist_squared(npc_pos, seg.closest);
        if (seg.seg_dist_sqrd < ON_SEGMENT_DIST_SQRD)
        {
            return seg.next_handle;
        }
        if (seg.seg_dist_sqrd < best->seg_dist_sqrd)
        {
            best = &seg;
        }
    }
    return best->next_handle;
}

// 0x00A69780 (:261)
static patrol_node_handle pick_next_node_looping(unsigned int patrol_handle, int index, const farray<navpoint*,512>& nav_list)
{
    int last = nav_list.size() - 1;
    if (index < last)
    {
        return patrol_node_handle(patrol_handle, nav_list[index + 1]->get_handle(), false, index + 1 == last);
    }
    return patrol_node_handle(patrol_handle, nav_list[0]->get_handle(), false, nav_list.size() == 1);
}

// 0x00A697D0 (:283)
static patrol_node_handle pick_next_node_forward(unsigned int patrol_handle, int index, const farray<navpoint*,512>& nav_list)
{
    int last = nav_list.size() - 1;
    if (nav_list.size() == 1)
    {
        return patrol_node_handle(patrol_handle, nav_list[0]->get_handle(), false, true);
    }
    if (index < last)
    {
        return patrol_node_handle(patrol_handle, nav_list[index + 1]->get_handle(), false, index + 1 == last);
    }
    // At the end of a path that does not loop: turns around.
    return patrol_node_handle(patrol_handle, nav_list[index - 1]->get_handle(), true, index == 1);
}

// 0x00A69850 (:309)
static patrol_node_handle pick_next_node_backward(unsigned int patrol_handle, int index, const farray<navpoint*,512>& nav_list)
{
    if (nav_list.size() == 1)
    {
        return patrol_node_handle(patrol_handle, nav_list[0]->get_handle(), false, true);
    }
    if (index < 1)
    {
        return patrol_node_handle(patrol_handle, nav_list[1]->get_handle(), false, nav_list.size() == 2);
    }
    return patrol_node_handle(patrol_handle, nav_list[index - 1]->get_handle(), true, index == 1);
}

// 0x00A3D360 (:506)
patrol_node_handle::patrol_node_handle()
{
    reset();
}

// 0x00A3D380 (:511)
patrol_node_handle::patrol_node_handle(unsigned int patrol_handle_, unsigned int nav_handle_, bool reverse_, bool last_node_)
{
    *(unsigned char*)&node_flags = 0;
    patrol_handle = patrol_handle_;
    nav_handle = nav_handle_;
    node_flags.reverse = reverse_;
    node_flags.last_node = last_node_;
}

// 0x00A1AAB0 (:520)
void patrol_node_handle::reset()
{
    patrol_handle = 0xFFFFFFFF;
    nav_handle = 0xFFFFFFFF;
    *(unsigned char*)&node_flags = 0;
}

// 0x00A1AAD0 (:527)
bool patrol_node_handle::is_valid() const
{
    return nav_handle != 0xFFFFFFFF && patrol_handle != 0xFFFFFFFF;
}

// 0x00A92990 (:535)
bool patrol_node_handle::is_near_road() const
{
    if (!node_flags.node_checked)
    {
        object* navp = World.object_from_handle(nav_handle);
        if (navp != nullptr)
        {
            vector closest;
            float u;
            object_path_road* road = object_road_get_closest_road(navp->pos, false, closest, u, NEAR_ROAD_DIST, false, nullptr, nullptr, false);
            // Original: a const method caching in the handle.
            patrol_node_handle* self = const_cast<patrol_node_handle*>(this);
            self->node_flags.near_road = road != nullptr;
            self->node_flags.node_checked = 1;
        }
    }
    return node_flags.near_road;
}

// 0x00A3D3C0 (:548)
bool patrol_node_handle::dont_follow_road() const
{
    if (nav_handle != 0xFFFFFFFF)
    {
        navpoint* navp = World.object_from_handle<navpoint>(nav_handle);
        if (navp != nullptr)
        {
            return navp->nav_flags.dont_follow_road;
        }
    }
    return false;
}

// 0x00A1AAF0 (:559)
bool patrol_node_handle::is_streamed() const
{
    object* navp = World.object_from_handle(nav_handle);
    if (navp == nullptr)
    {
        return false;
    }
    if (navp->get_parent() == 0xFFFFFFFF)
    {
        return true;
    }
    return navp->obj_flags.streamed;
}

// 0x00A3D3F0 (:579)
bool patrol_node_handle::get_pos(vector& pos) const
{
    if (nav_handle != 0xFFFFFFFF)
    {
        navpoint* navp = World.object_from_handle<navpoint>(nav_handle);
        if (navp != nullptr)
        {
            pos = navp->pos;
            return true;
        }
    }
    return false;
}

// 0x00A3D430 (:595)
const rfg_mover* patrol_node_handle::is_inside_building() const
{
    object* navp = World.object_from_handle(nav_handle);
    if (navp != nullptr && navp->get_parent() != 0xFFFFFFFF)
    {
        return World.object_from_handle<rfg_mover>(navp->get_parent());
    }
    return nullptr;
}

// 0x00A05810 (.h:48)
bool patrol_node_handle::is_reversed() const
{
    return node_flags.reverse;
}

// 0x00A05820 (.h:51)
bool patrol_node_handle::is_last_node() const
{
    return node_flags.last_node;
}

// 0x00A1AB30 (:621)
void types_object_patrol(signed char& type, signed char& subtype)
{
    type = OT_PATROL;
    subtype = -1;
}

// 0x00A1AB40 (:621)
int bpo_index_object_patrol()
{
    return 1;
}

// 0x00A1AB50 (:621)
const char* classname_object_patrol()
{
    return "object_patrol";
}

// 0x00A698E0 (:621)
object* alloc_object_patrol()
{
    return object_patrolpool.alloc(0);
}

static obj_class_instantiator object_patrol_instantiator("object_patrol", alloc_object_patrol, types_object_patrol, bpo_index_object_patrol, classname_object_patrol, object_patrol::pool_remaining, 0x3210, 72);

// 0x00A1AB60 (:621)
unsigned int object_patrol::get_classname_hash()
{
    return object_patrol_instantiator.this_hash;
}

// 0x00A3D480 (:621)
int object_patrol::pool_remaining()
{
    return object_patrolpool.get_free();
}

// 0x00A92A10 (:621)
void object_patrol::pool_free()
{
    object_patrolpool.free(this);
}

// 0x00A3D490 (:622)
object_patrol::object_patrol()
{
}

// 0x00A3D4D0
object_patrol::~object_patrol()
{
}

// 0x005DBDB0 (.h:93)
const object_type object_patrol::get_type_static()
{
    return OT_PATROL;
}

// 0x005DBDC0 (.h:93)
const object_sub_type object_patrol::get_subtype_static()
{
    return OT_SUB_UNDEFINED;
}

// 0x00A05830 (.h:93)
const char* object_patrol::get_classname_static()
{
    return "object_patrol";
}

// 0x00753A40 (.h:93)
int object_patrol::get_bpo_index_static()
{
    return 1;
}

// 0x00A3D4C0 (.h:93)
const char* object_patrol::get_classname() const
{
    return "object_patrol";
}

// 0x00AAA800 (:630)
bool object_patrol::serialize(obj_prop_block& pblock, bool load)
{
    if (!object::serialize(pblock, load))
    {
        return false;
    }
    if (load)
    {
        root_node_handle.reset();
        *(unsigned int*)&patrol_flags = 0;
        unsigned int root = 0;
        if (!pblock.get_optional(PATROL_PROP_NAV_POINT, root) || root == 0)
        {
            return false;
        }
        root_node_handle = patrol_node_handle(get_handle(), root, false, false);
        bool value = true;
        if (pblock.get_optional(PATROL_PROP_LOOPING_PATROL, value))
        {
            patrol_flags.looping_patrol = value;
        }
        else
        {
            patrol_flags.looping_patrol = 1;
        }
        patrol_flags.courier_patrol = pblock.get_optional(PATROL_PROP_COURIER_PATROL, value) && value;
        patrol_flags.ASD_truck_patrol = pblock.get_optional(PATROL_PROP_ASD_PATROL, value) && value;
        patrol_flags.override_patrol = pblock.get_optional(PATROL_PROP_OVERRIDE_PATROL, value) && value;
        patrol_flags.combat_ready = pblock.get_optional(PATROL_PROP_COMBAT_READY, value) && value;
        patrol_flags.marauder_raid = pblock.get_optional(PATROL_PROP_MARAUDER_RAID, value) && value;
        patrol_flags.no_check_in = pblock.get_optional(PATROL_PROP_NO_CHECK_IN, value) && value;
        if (root_node_handle.nav_handle == 0xFFFFFFFF)
        {
            return false;
        }
    }
    else
    {
        pblock.set(PATROL_PROP_NAV_POINT, root_node_handle.nav_handle);
    }
    return true;
}

// 0x00A1AB70 (:734)
bool object_patrol::serialize_restore_if_missing_in_save_file(obj_prop_block& pblock)
{
    return false;
}

// 0x00A1AB80 (:739)
bool object_patrol::serialize_restore_if_resetting_destruction(obj_prop_block& pblock)
{
    return true;
}

// 0x00A3D500 (:745)
const navpoint* object_patrol::get_root_nav() const
{
    return root_node_handle.nav_handle != 0xFFFFFFFF ? World.object_from_handle<navpoint>(root_node_handle.nav_handle) : nullptr;
}

// 0x00A1AB90 (:753)
void object_patrol::get_bbox(vector& bmin, vector& bmax) const
{
    get_local_bbox(bmin, bmax);
    bmin.x += pos.x;
    bmin.y += pos.y;
    bmin.z += pos.z;
    bmax.x += pos.x;
    bmax.y += pos.y;
    bmax.z += pos.z;
}

// 0x00A1AC60 (:763)
void object_patrol::get_local_bbox(vector& bmin, vector& bmax) const
{
    bmin.x = -0.5f;
    bmin.y = -0.5f;
    bmin.z = -0.5f;
    bmax.x = 0.5f;
    bmax.y = 0.5f;
    bmax.z = 0.5f;
}

// 0x00A92A20 (:776)
void object_patrol::free_this()
{
    Orders_manager.destroy_all_orders_with_target(get_handle());
    object::free_this();
}

// 0x00A1ACA0 (:796)
bool object_patrol::is_root_node(const patrol_node_handle& node_handle) const
{
    return node_handle.is_valid() && node_handle.nav_handle == root_node_handle.nav_handle && node_handle.patrol_handle == root_node_handle.patrol_handle;
}

// 0x00A698F0 (:806)
const navpoint* object_patrol::get_nearest_nav(const vector& point) const
{
    navpoint* root = root_node_handle.nav_handle != 0xFFFFFFFF ? World.object_from_handle<navpoint>(root_node_handle.nav_handle) : nullptr;
    if (root == nullptr)
    {
        return nullptr;
    }
    farray<navpoint*,512> nav_list;
    build_patrol_point_list(root, nav_list);
    if (nav_list.size() < 1)
    {
        return nullptr;
    }
    navpoint* nearest = nav_list[0];
    float nearest_dist = vec_dist_squared(point, nearest->pos);
    for (int i = 1; i < nav_list.size(); i++)
    {
        float dist = vec_dist_squared(nav_list[i]->pos, point);
        if (dist < nearest_dist)
        {
            nearest_dist = dist;
            nearest = nav_list[i];
        }
    }
    return nearest;
}

// 0x00A699F0 (:846)
patrol_node_handle object_patrol::get_next_patrol_point_handle(const vector& location) const
{
    if (vec_dist_squared(location, pos) > NEAR_PATROL_POINT_DIST_SQRD && root_node_handle.nav_handle != 0xFFFFFFFF)
    {
        navpoint* root = World.object_from_handle<navpoint>(root_node_handle.nav_handle);
        if (root != nullptr)
        {
            farray<navpoint*,512> nav_list;
            bool has_end = build_patrol_point_list(root, nav_list);
            if (has_end || nav_list.size() > 0)
            {
                // Original: the third parameter means "circular" but gets the list's "has an end" result.
                unsigned int next = find_next_path_node(nav_list, location, has_end);
                return patrol_node_handle(get_handle(), next, false, next == nav_list[nav_list.size() - 1]->get_handle());
            }
        }
    }
    return root_node_handle;
}

// 0x00A1ACE0 (:886)
bool object_patrol::is_member(const patrol_node_handle& node_handle) const
{
    return get_handle() == node_handle.patrol_handle;
}

// 0x00A69B30 (:892)
bool object_patrol::get_last_patrol_node_pos(vector& node_pos) const
{
    navpoint* root = root_node_handle.nav_handle != 0xFFFFFFFF ? World.object_from_handle<navpoint>(root_node_handle.nav_handle) : nullptr;
    if (root != nullptr)
    {
        farray<navpoint*,512> nav_list;
        build_patrol_point_list(root, nav_list);
        if (nav_list.size() > 0)
        {
            node_pos = nav_list[nav_list.size() - 1]->pos;
            return true;
        }
    }
    return false;
}

// 0x00A69BC0 (:920)
bool object_patrol::get_last_patrol_node_handle(patrol_node_handle& last_node_handle) const
{
    navpoint* root = root_node_handle.nav_handle != 0xFFFFFFFF ? World.object_from_handle<navpoint>(root_node_handle.nav_handle) : nullptr;
    if (root == nullptr)
    {
        return false;
    }
    farray<navpoint*,512> nav_list;
    build_patrol_point_list(root, nav_list);
    if (nav_list.size() > 0)
    {
        last_node_handle = patrol_node_handle(get_handle(), nav_list[nav_list.size() - 1]->get_handle(), !patrol_flags.looping_patrol, true);
    }
    return nav_list.size() > 0;
}

// 0x00A1AD00 (:949)
patrol_node_handle object_patrol::get_first_patrol_point_handle() const
{
    return root_node_handle;
}

// 0x00A69C80 (:958)
patrol_node_handle object_patrol::get_next_patrol_point_handle(const patrol_node_handle& node_handle, const vector& location) const
{
    if (get_handle() != node_handle.patrol_handle)
    {
        return get_next_patrol_point_handle(location);
    }
    navpoint* current = node_handle.nav_handle != 0xFFFFFFFF ? World.object_from_handle<navpoint>(node_handle.nav_handle) : nullptr;
    navpoint* root = root_node_handle.nav_handle != 0xFFFFFFFF ? World.object_from_handle<navpoint>(root_node_handle.nav_handle) : nullptr;
    if (current == nullptr || root == nullptr)
    {
        return root_node_handle;
    }
    farray<navpoint*,512> nav_list;
    build_patrol_point_list(root, nav_list);
    int index = -1;
    for (int i = 0; i < nav_list.size(); i++)
    {
        if (nav_list[i] == current)
        {
            index = i;
            break;
        }
    }
    if (index < 0)
    {
        return root_node_handle;
    }
    patrol_node_handle next;
    if (patrol_flags.looping_patrol)
    {
        next = pick_next_node_looping(get_handle(), index, nav_list);
    }
    else if (node_handle.node_flags.reverse)
    {
        next = pick_next_node_backward(get_handle(), index, nav_list);
    }
    else
    {
        next = pick_next_node_forward(get_handle(), index, nav_list);
    }
    if (node_handle.node_flags.reverse)
    {
        return next;
    }
    navpoint* next_nav = World.object_from_handle<navpoint>(next.nav_handle);
    if (next_nav == nullptr || !next_nav->nav_flags.start_branch)
    {
        return next;
    }
    navpoint* branch = nullptr;
    if (next_nav->nav_flags.bridge)
    {
        rfg_mover* bridge = World.object_from_handle<rfg_mover>(next_nav->get_parent());
        if (bridge != nullptr && bridge->percent_destroyed() <= BRIDGE_DESTROYED_PERCENT)
        {
            return next;
        }
        for (int i = 0; i < current->num_links; i++)
        {
            navpoint* link = current->links[i];
            if (link != nullptr && link != current->prev_path_navpoint && link->nav_flags.start_branch && !link->nav_flags.bridge)
            {
                branch = current->links[i];
                current->next_path_navpoint = branch;
                break;
            }
        }
        // Original: no check for no branch but the broken bridge; reads through a null branch.
    }
    else
    {
        for (int i = 0; i < current->num_links; i++)
        {
            navpoint* link = current->links[i];
            if (link != nullptr && link != current->prev_path_navpoint && link != next_nav && link->nav_flags.start_branch)
            {
                if (link->nav_flags.bridge)
                {
                    return next;
                }
                if (fl_rand_chance(0.5f))
                {
                    branch = current->links[i];
                }
            }
        }
        if (branch == nullptr)
        {
            return next;
        }
        current->next_path_navpoint = branch;
    }
    return patrol_node_handle(get_handle(), branch->get_handle(), false, false);
}

// 0x00A69FC0 (:1064)
bool object_patrol::get_all_patrol_nodes(farray<navpoint*,512>& nav_list) const
{
    navpoint* root = root_node_handle.nav_handle != 0xFFFFFFFF ? World.object_from_handle<navpoint>(root_node_handle.nav_handle) : nullptr;
    return root != nullptr && build_patrol_point_list(root, nav_list);
}

// 0x00A3D530 (:1079)
human_movement_states object_patrol::get_foot_speed(const patrol_node_handle& node_handle) const
{
    if (get_handle() == node_handle.patrol_handle && node_handle.nav_handle != 0xFFFFFFFF)
    {
        navpoint* navp = World.object_from_handle<navpoint>(node_handle.nav_handle);
        // A node's speed limit of 0 or less means run.
        if (navp != nullptr && navp->speed_limit <= 0.0f)
        {
            return HMS_RUN;
        }
    }
    return HMS_WALK;
}

// 0x00A1AD20 (:1098)
bool object_patrol::get_node_pos(const patrol_node_handle& node_handle, vector& patrol_point) const
{
    if (get_handle() == node_handle.patrol_handle)
    {
        object* navp = World.object_from_handle(node_handle.nav_handle);
        if (navp != nullptr)
        {
            patrol_point = navp->pos;
            return true;
        }
    }
    return false;
}

// 0x005DBDD0 (.h:146)
bool object_patrol::is_override_patrol() const
{
    return patrol_flags.override_patrol;
}

// 0x005DBDE0 (.h:150)
bool object_patrol::is_paused() const
{
    return patrol_flags.is_paused;
}

// 0x00753A50 (.h:152)
void object_patrol::set_pause_flag(bool paused)
{
    patrol_flags.is_paused = paused;
}

// 0x00753A70 (.h:155)
bool object_patrol::marauder_raid_route() const
{
    return patrol_flags.marauder_raid;
}

// 0x005DBDF0 (.h:158)
patrol_node_handle object_patrol::get_root_node_handle() const
{
    return root_node_handle;
}

// 0x00B1F600 (.h:160)
void object_patrol::set_override_flag(bool override_patrol)
{
    patrol_flags.override_patrol = override_patrol;
}

// 0x005DBE10 (.h:165)
bool object_patrol::no_patrol_check_in() const
{
    return patrol_flags.no_check_in;
}

// 0x005DBE20 (.h:168)
bool object_patrol::combat_ready() const
{
    return patrol_flags.combat_ready;
}

// 0x00A1AD60 (:1129)
bool patrol_handles_are_equal(const patrol_node_handle& patrol_handle1, const patrol_node_handle& patrol_handle2)
{
    return patrol_handle1.nav_handle == patrol_handle2.nav_handle && patrol_handle1.patrol_handle == patrol_handle2.patrol_handle && patrol_handle1.is_last_node() == patrol_handle2.is_last_node() &&
           patrol_handle1.is_reversed() == patrol_handle2.is_reversed();
}

// 0x00A3D580 (:1145)
object_squad_spawn_node* patrol_find_child_squad_node(const object_patrol* patrol)
{
    if (patrol != nullptr)
    {
        object* child = patrol->child_ptr;
        object* first = child;
        while (child != nullptr)
        {
            if (child->get_obj_type() == OT_SQUAD_SPAWN_NODE)
            {
                return (object_squad_spawn_node*)child;
            }
            child = child->child_next;
            if (child == first)
            {
                break;
            }
        }
    }
    return nullptr;
}
