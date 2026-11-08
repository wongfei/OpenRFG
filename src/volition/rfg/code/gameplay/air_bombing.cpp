// Original: volition/rfg/code/gameplay/air_bombing.cpp
//
// Air bombing: scripted air strikes (air_bombing.xtbl) that launch projectiles from a far origin at a focus object, with
// attack and avoid lists of objects, a forced target and the grinder as the target designator.

#include "volition/rfg/code/gameplay/air_bombing.h"

#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/rfg/code/code_effects/explosion_shared.h"
#include "volition/rfg/code/effects/effects.h"
#include "volition/rfg/code/effects/effects_shared.h"
#include "volition/rfg/code/objects/level_object_properties.h"
#include "volition/rfg/code/rfg.h"
#include "volition/rfg/code/effects/object_effect.h"
#include "volition/rfg/code/havok/havok_raycast.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human_info.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/movers/general_mover.h"
#include "volition/rfg/code/objects/object.h"
#include "volition/rfg/code/vehicle/automobile.h"
#include "volition/rfg/code/objects/object_dummy.h"
#include "volition/rfg/code/objects/object_item.h"
#include "volition/rfg/code/objects/projectile.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/render_lib/code/gr/gr.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/os/vconsole.h"
#include "volition/vlib/code/math/ix.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/os/frametime.h"
#include "volition/vlib/code/memory/rfg_split_mempool.h"
#include "volition/vlib/code/memory/static_mempool.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/vlib/code/util/fastxml.h"
#include "volition/vlib/code/util/string_comp.h"
#include "volition/vlib/code/util/string_util.h"
#include "volition/vlib/code/util/varray.h"
#include "volition/render_lib/code/gr/grd.h"

#include "keen/base/trace.hpp"

#include <math.h>
#include <string.h>

// Magic number: 0.017453 (0x3F91DF3300000000 as a double), rfg.exe's degrees to radians.
static const double AIR_BOMB_DEG_TO_RAD = 0.017453;
// Magic number: 8 air bombs at a time, the size of Air_bombs.
static const int MAX_AIR_BOMBS = 8;

// ---- file statics
static unsigned int Air_bomb_scatter_effect_handle = 0xFFFFFFFF; // 0x016513B8
static float Air_bomb_attack_probability = 0.5f; // 0x016513BC
static float Air_bomb_interest_probability = 0.2f; // 0x016513C0
static float Air_bomb_interest_radius_squared = 900.0f; // 0x016513C4
static float Air_bomb_avoid_additional_padding = 3.0f; // 0x016513D4
static farray<obj_timed_list_item,16> Air_bomb_avoid_list; // 0x016573F8
static farray<obj_timed_list_item,16> Air_bomb_attack_list; // 0x016574C8
static bool Air_bomb_debug; // 0x02229601, a debug switch nothing in rfg.exe writes
bool Air_bomb_weapon; // 0x02229602
static bool Target_player; // 0x02229603
static air_bomb_info* Air_bomb_infos; // 0x0222960C
static int Num_air_bomb_infos; // 0x02229610
static bool Force_attack_list; // 0x02229614
static air_bomb Air_bombs[MAX_AIR_BOMBS]; // 0x02246588

static void air_bomb_load_table(bool reload);

// 0x00772760 (:69)
// Original: -1 as the origin never finds an object, so air_bomb_start always fails from here.
void unused_dcf_air_bomb_start()
{
    if (Console_command == 0)
    {
        if (Console_help == 0)
        {
            return;
        }
    }
    else if (console_get_arg(2, 1))
    {
        const int index = air_bomb_definition_get_index_from_name(Console_arg);
        if (index > -1)
        {
            air_bomb_start(&Air_bomb_infos[index], player_get_local_player()->get_handle(), 0xFFFFFFFF, true);
            return;
        }
        console_output("Invalid air bomb definition name.", nullptr, false);
        return;
    }
    console_output("Usage: air_bomb_start <air_bomb_name>", nullptr, false);
}

// 0x007727E0 (:91)
void unused_dcf_air_bomb_reload()
{
    if (Console_command != 0)
    {
        cf_refresh_file_size("air_bombing.xtbl");
        air_bomb_load_table(true);
    }
}

// 0x00772800 (:203)
air_bomb::air_bomb()
{
    clear();
}

// 0x0075C150 (:209)
bool air_bomb::is_running() const
{
    return info != nullptr;
}

// 0x0075C160 (:212)
const air_bomb_info* air_bomb::get_current_info() const
{
    return info;
}

// 0x0075C180 (:254)
// LTCG: the info in ESI, the element in EAX and reload on the stack.
static void parse_air_bomb_info(air_bomb_info* info, xml_element* a, bool reload)
{
    if (!reload)
    {
        const char* name = xml_table_get_req_string_ref(a, "Name");
        const unsigned int size = (unsigned int)strlen(name) + 1;
        info->name = (char*)Permanent_mempool.alloc(size, 1);
        strncpy_safe(info->name, name, size);
    }
    xml_table_get_req_float(info->height, a, "Height");
    xml_table_get_req_float(info->radius, a, "Radius");
    xml_table_get_req_float(info->veh_radius, a, "Vehicle_radius");
    xml_table_get_req_float(info->rate_min, a, "Rate_min");
    xml_table_get_req_float(info->rate_max, a, "Rate_max");
    xml_table_get_req_float(info->angle_min, a, "Angle_min");
    xml_table_get_req_float(info->angle_max, a, "Angle_max");
    info->angle_min = (float)((double)info->angle_min * AIR_BOMB_DEG_TO_RAD);
    info->angle_max = (float)((double)info->angle_max * AIR_BOMB_DEG_TO_RAD);
    if (!xml_table_get_opt_float(info->min_player_fvec_dot_prod, a, "Min_player_look_dot_prod"))
    {
        info->min_player_fvec_dot_prod = -1.0f;
    }
    const char* projectile = xml_table_get_opt_string_ref(a, "Projectile");
    info->w_info = projectile != nullptr ? weapons_get_weapon_info(projectile) : nullptr;
    if (info->w_info == nullptr)
    {
        return;
    }
    xml_table_get_req_int(info->num_projectiles, a, "Num_projectiles");
    float spread;
    info->min_spread = xml_table_get_opt_float(spread, a, "Min_spread") ? (float)((double)spread * AIR_BOMB_DEG_TO_RAD) : 0.0f;
    if (xml_table_get_opt_float(spread, a, "Max_spread"))
    {
        info->max_spread = (float)((double)spread * AIR_BOMB_DEG_TO_RAD);
    }
    else
    {
        info->max_spread = info->min_spread;
    }
    if (!xml_table_get_opt_float(info->min_projectile_delay, a, "Min_Proj_Delay"))
    {
        info->min_projectile_delay = 0.0f;
    }
    if (!xml_table_get_opt_float(info->max_projectile_delay, a, "Max_Proj_Delay"))
    {
        info->max_projectile_delay = info->min_projectile_delay;
    }
    // The drop time along the mean launch angle. rfg.exe computes the first value too and overwrites it.
    // The mean angle is narrowed to float before the sine (0x0075C682).
    const float mean_angle = (float)(((double)info->angle_max + (double)info->angle_min) * 0.5);
    info->projectile_travel_time = (float)((double)info->height / ((double)(float)sin((double)mean_angle) * (double)info->w_info->projectile_info.start_speed));
}

// 0x0075C3E0 (:333)
static void air_bomb_load_table(bool reload)
{
    xml_element* root = xml_table_parse("air_bombing.xtbl");
    if (!reload)
    {
        Num_air_bomb_infos = xml_table_count(root, "Air_Bomb");
        // Magic number: << 6, sizeof(air_bomb_info).
        Air_bomb_infos = (air_bomb_info*)Permanent_mempool.alloc(Num_air_bomb_infos * sizeof(air_bomb_info), 0);
    }
    int index = 0;
    for (xml_element* elem = xml_table_find_opt(root, "Air_Bomb"); elem != nullptr; elem = xml_table_find_next(root, elem, "Air_Bomb"))
    {
        // Deviation from rfg.exe: it inlines parse_air_bomb_info here.
        parse_air_bomb_info(&Air_bomb_infos[index++], elem, reload);
    }
    xml_table_close(root);
}

// 0x00772840 (:374)
// The objects that can blow up beside a strike: vehicles and general movers flagged as explosive.
static bool exploding_object_collector(object* hit_objp, unsigned int shape_key, const void* user_data)
{
    if (hit_objp == nullptr)
    {
        return false;
    }
    if (hit_objp->get_obj_type() == OT_VEHICLE)
    {
        return true;
    }
    if (hit_objp->get_obj_type() == OT_MOVER && hit_objp->get_sub_type() == OT_SUB_MOVER_GENERAL)
    {
        return ((general_mover*)hit_objp)->m_level_obj_properties->m_base_properties->m_flags.m_death_explosion;
    }
    return false;
}

// 0x0075C6E0 (:380)
// Original: stopping any one bomb also clears the two switches every bomb shares.
void air_bomb::clear()
{
    info = nullptr;
    focus = 0xFFFFFFFF;
    left_over_time = 0.0f;
    next_projectile = 0;
    attacking = false;
    Target_player = false;
    Force_attack_list = false;
    force_attack_handle = 0xFFFFFFFF;
}

// 0x007A33B0 (:396)
// Picks where the next volley lands, in order: the forced target, an attack list object (with a chance that grows as its
// timer runs out), something interesting near the player (a dummy marker, a vehicle's or a human's next position), or a
// random spot around the focus. Then the launch directions, which each have to pass valid_projectile_target.
bool air_bomb::setup_projectiles()
{
    launch_position = position;
    const player* pp = player_get_local_player();
    object* focus_obj = World.object_from_handle(focus);
    if (focus_obj != nullptr)
    {
        position = focus_obj->pos;
    }
    bool has_target = false;
    attacking = false;
    const float radius = (pp == nullptr || pp->vehicle_handle == 0xFFFFFFFF) ? info->radius : info->veh_radius;
    unsigned int seek_target = 0xFFFFFFFF;
    if (Air_bomb_attack_list.size() == 0)
    {
        Force_attack_list = false;
    }
    if (force_attack_handle != 0xFFFFFFFF)
    {
        object* forced = World.object_from_handle(force_attack_handle);
        has_target = forced != nullptr;
        if (has_target)
        {
            // Original: the middle half of the box, so the middle of each axis is what the volley can hit.
            vector bmin;
            vector bmax;
            forced->get_bbox(bmin, bmax);
            const vector size((bmax.x - bmin.x) * 0.5f, (bmax.y - bmin.y) * 0.5f, (bmax.z - bmin.z) * 0.5f);
            launch_position.x = fl_rand_range(bmin.x + size.x, bmax.x - size.x);
            launch_position.y = fl_rand_range(bmin.y + size.y, bmax.y - size.y);
            launch_position.z = fl_rand_range(bmin.z + size.z, bmax.z - size.z);
            attacking = true;
        }
        force_attack_handle = 0xFFFFFFFF;
    }
    const float attack_probability = Air_bomb_attack_probability;
    if (fl_rand() <= attack_probability || Force_attack_list || Air_bomb_weapon)
    {
        if (!has_target)
        {
            for (int i = 0; i < Air_bomb_attack_list.size(); i++)
            {
                object* target = World.object_from_handle(Air_bomb_attack_list[i].handle);
                if (target == nullptr || target->is_killed())
                {
                    Air_bomb_attack_list.remove(i);
                    i--;
                    continue;
                }
                const float percent = Air_bomb_attack_list[i].timer.percent();
                if (percent < fl_rand() && ix_point_in_sphere(position, radius, target->pos))
                {
                    vector bmin;
                    vector bmax;
                    target->get_bbox(bmin, bmax);
                    launch_position.x = fl_rand_range(bmin.x, bmax.x);
                    launch_position.y = fl_rand_range(bmin.y, bmax.y);
                    launch_position.z = fl_rand_range(bmin.z, bmax.z);
                    attacking = true;
                    seek_target = Air_bomb_attack_list[i].handle;
                    has_target = true;
                    break;
                }
            }
        }
        // The grinder only strikes what it has designated.
        if (Air_bomb_weapon && !has_target)
        {
            return false;
        }
    }
    bool have_spot = has_target;
    if (!have_spot && pp != nullptr && fl_rand() <= Air_bomb_interest_probability)
    {
        // An air bomb point marks a spot, and the first strike there uses it up.
        const int num_dummies = World.get_num_objects<object_dummy>();
        for (int i = 0; i < num_dummies && !have_spot; i++)
        {
            object_dummy* dummy = World.get_object<object_dummy>(i);
            if (dummy->get_dummy_type() == DUMMY_OBJECT_TYPE_AIR_BOMB_POINT && vec_dist_squared(pp->pos, dummy->pos) < Air_bomb_interest_radius_squared)
            {
                launch_position = dummy->pos;
                World.destroy_object(dummy, false);
                have_spot = true;
            }
        }
        const int num_vehicles = World.get_num_objects<vehicle>();
        for (int i = 0; i < num_vehicles && !have_spot; i++)
        {
            vehicle* vp = World.get_object<vehicle>(i);
            if (!vp->is_killed() && vec_dist_squared(pp->pos, vp->pos) < Air_bomb_interest_radius_squared)
            {
                vector velocity;
                vehicle_get_velocity(vp, velocity);
                const float time = info->projectile_travel_time;
                launch_position.set(velocity.x * time + vp->pos.x, vp->pos.y + velocity.y * time, vp->pos.z + velocity.z * time);
                have_spot = true;
            }
        }
        const int num_humans = World.get_num_objects<human>();
        for (int i = 0; i < num_humans && !have_spot; i++)
        {
            human* hp = World.get_object<human>(i);
            if (!hp->is_killed() && !object_is_player(hp) && vec_dist_squared(pp->pos, hp->pos) < Air_bomb_interest_radius_squared)
            {
                const float time = info->projectile_travel_time;
                const vector& velocity = hp->velocity;
                launch_position.set(time * velocity.x + hp->pos.x, hp->pos.y + velocity.y * time, hp->pos.z + velocity.z * time);
                have_spot = true;
            }
        }
    }
    if (!have_spot)
    {
        launch_position.x += fl_rand_range(-radius, radius);
        launch_position.z += fl_rand_range(-radius, radius);
    }

    // The first projectile flies from the origin's side at a random angle below level, and the launch point is moved
    // back along it by the bomb's height.
    vector flat(launch_position.x - origin_position.x, 0.0f, launch_position.z - origin_position.z);
    flat.normalize();
    const float angle = fl_rand_range(info->angle_min, info->angle_max);
    const float sin_angle = (float)sin((double)angle);
    const float cos_angle = (float)cos((double)angle);
    projectile_info* first = &projectiles[0];
    first->launch_direction.set(flat.x * cos_angle - UP_VECTOR.x * sin_angle, cos_angle * flat.y - UP_VECTOR.y * sin_angle, flat.z * cos_angle - UP_VECTOR.z * sin_angle);
    first->launch_direction.normalize();
    first->launch_velocity = info->w_info->projectile_info.start_speed;
    first->seek_target = seek_target;
    launch_position.x -= first->launch_direction.x * info->height;
    launch_position.y -= first->launch_direction.y * info->height;
    launch_position.z -= first->launch_direction.z * info->height;
    if (!valid_projectile_target(launch_position, first->launch_direction, radius, attacking))
    {
        return false;
    }

    // Up to 8 tries for the other projectiles of the volley, each spread around the first and slowed so that they land
    // one after another. Magic number: 6.2831855, PI2.
    int count = 1;
    float delta_speed = 0.0f;
    for (int attempt = 1; attempt < 8; attempt++)
    {
        const float spread_angle = fl_rand_range(0.0f, 6.2831855f);
        const float spread = fl_rand_range(info->min_spread, info->max_spread);
        const float yaw = (float)cos((double)spread_angle) * spread;
        const float pitch = (float)sin((double)spread_angle) * spread;
        matrix rotation(pitch, 0.0f, yaw);
        projectiles[count].launch_direction = first->launch_direction.copy_unrotate(rotation);
        projectiles[count].launch_direction.normalize();
        if (valid_projectile_target(launch_position, projectiles[count].launch_direction, radius, attacking))
        {
            const float delay = fl_rand_range(info->min_projectile_delay, info->max_projectile_delay);
            delta_speed = delta_speed - info->height / (info->projectile_travel_time / delay + 1.0f);
            projectiles[count].launch_velocity = info->w_info->projectile_info.start_speed + delta_speed;
            projectiles[count].seek_target = seek_target;
            count++;
            if (count >= info->num_projectiles)
            {
                break;
            }
        }
    }
    if (count < info->num_projectiles)
    {
        return false;
    }
    if (count > 1)
    {
        // Magic numbers: effects_play's defaults: no parent bone (-1), no target, full scale.
        const unsigned int effect = effects_play(Air_bomb_scatter_effect_handle, launch_position, IDENTITY_MATRIX, false, 0xFFFFFFFF, -1, 0xFFFFFFFF, 1.0f, false, EFFECTS_CATEGORY_NONE);
        if (effect != 0xFFFFFFFF)
        {
            object_effect* ep = World.object_from_handle<object_effect>(effect);
            // An effect out of the streamed world is not kept.
            if (ep != nullptr && !ep->obj_flags.streamed)
            {
                World.destroy_object(ep, false);
            }
        }
    }
    next_projectile = 0;
    return true;
}

// 0x0079BB20 (:647)
// A launch is valid when its ray hits within reach of the bomb, the blast (grown over what would blow up beside it)
// misses the player and the avoid list, and the player could be looking at it.
bool air_bomb::valid_projectile_target(const vector& pos, const vector& dir, float radius, bool attackingp)
{
    raycast_input input;
    input.radius = 0.0f;
    input.start = pos;
    input.m_filter_info = COLLIDE_RAY_DEFAULT;
    input.filter_params = nullptr;
    input.filter_callback = nullptr;
    input.m_hit_collector_type = raycast_input::COLLECT_CLOSEST_HIT;
    input.m_flags = 0;
    // Magic number: the ray is twice the bomb's height.
    const float height = info->height;
    input.end.set(pos.x + height * dir.x * 2.0f, pos.y + dir.y * height * 2.0f, pos.z + dir.z * height * 2.0f);
    raycast_output output;
    if (!havok_raycast(input, output))
    {
        return false;
    }
    const vector& hit = output.m_hits[0].hit_pos;
    if (!attackingp)
    {
        const float dx = hit.x - position.x;
        const float dz = hit.z - position.z;
        if (radius * radius < dz * dz + dx * dx)
        {
            return false;
        }
    }
    float blast_radius = info->w_info->m_explosion_info->m_secondary_radius;
    unsigned int exploding[16];
    const int num_exploding = havok_collect_objects_fast(hit, blast_radius, exploding, 16, COLLIDE_MOVER, exploding_object_collector, nullptr);
    for (int i = 0; i < num_exploding; i++)
    {
        object* objp = World.object_from_handle(exploding[i]);
        if (objp == nullptr)
        {
            continue;
        }
        if (objp->get_obj_type() == OT_MOVER)
        {
            // Original: a rfg mover here is passed on as a null general mover.
            general_mover* gm = objp->get_sub_type() == OT_SUB_MOVER_GENERAL ? (general_mover*)objp : nullptr;
            vector explosion_pos;
            const explosion_info* explosion = gm->get_death_explosion_info_pos_and_orient(0, explosion_pos, nullptr);
            if (explosion != nullptr)
            {
                const float reach = vec_dist(explosion_pos, hit) + explosion->m_secondary_radius;
                if (blast_radius < reach)
                {
                    blast_radius = reach;
                }
            }
        }
        else if (objp->get_obj_type() == OT_VEHICLE && !objp->is_killed())
        {
            const explosion_info* explosion = ((vehicle*)objp)->info->m_explosion_info;
            if (explosion != nullptr)
            {
                vector tag_pos;
                objp->get_tag_world_coords(objp->get_tag_index_from_name("gas_tank"), tag_pos, nullptr, nullptr, nullptr);
                const float reach = vec_dist(tag_pos, hit) + explosion->m_secondary_radius;
                if (blast_radius < reach)
                {
                    blast_radius = reach;
                }
            }
        }
    }
    const player* pp = player_get_local_player();
    if (pp != nullptr)
    {
        float speed;
        vehicle* vp = pp->vehicle_handle != 0xFFFFFFFF ? World.object_from_handle<vehicle>(pp->vehicle_handle) : nullptr;
        if (vp != nullptr && vp->get_sub_type() == OT_SUB_VEHICLE_AUTO)
        {
            // Magic number: 0.44704, mph to metres per second.
            speed = vp->info->enforced_max_speed * 0.44704f;
        }
        else
        {
            speed = pp->h_info->get_max_speed();
        }
        if (Target_player || Air_bomb_weapon)
        {
            speed = 0.0f;
        }
        const float player_reach = info->projectile_travel_time * speed;
        vector to_hit(hit.x - pp->pos.x, hit.y - pp->pos.y, hit.z - pp->pos.z);
        const float dist = to_hit.mag_and_normalize_safe();
        if (player_reach + blast_radius > dist && !Target_player && !Air_bomb_weapon)
        {
            if (!attackingp)
            {
                return false;
            }
        }
        else if (!attackingp && !Target_player && !Air_bomb_weapon)
        {
            const vector& fvec = Rfg_camera.ideal_orient.fvec;
            if (to_hit.y * fvec.y + to_hit.x * fvec.x + to_hit.z * fvec.z < info->min_player_fvec_dot_prod)
            {
                return false;
            }
        }
    }
    for (int i = 0; i < Air_bomb_avoid_list.size(); i++)
    {
        if (Air_bomb_avoid_list[i].timer.elapsed())
        {
            Air_bomb_avoid_list.remove(i);
            i--;
            continue;
        }
        object* objp = World.object_from_handle(Air_bomb_avoid_list[i].handle);
        if (objp == nullptr)
        {
            continue;
        }
        vector bmin;
        vector bmax;
        objp->get_bbox(bmin, bmax);
        const float padding = Air_bomb_avoid_additional_padding;
        bmin.set(bmin.x - padding, bmin.y - padding, bmin.z - padding);
        // Magic number: 50 m of open air above the object is off limits too.
        bmax.set(bmax.x + padding, bmax.y + padding + 50.0f, bmax.z + padding);
        if (ix_sphere_in_box(hit, blast_radius, bmin, bmax))
        {
            return false;
        }
    }
    return true;
}

// 0x00772880 (:787)
void air_bomb::launch_next_projectile()
{
    if (next_projectile >= info->num_projectiles || (Air_bomb_weapon && Air_bomb_attack_list.size() == 0))
    {
        return;
    }
    const projectile_info& next = projectiles[next_projectile];
    matrix orient;
    orient.make_quick(next.launch_direction);
    const vector velocity(next.launch_direction.x * next.launch_velocity, next.launch_direction.y * next.launch_velocity, next.launch_direction.z * next.launch_velocity);
    obj_item_flags flags;
    flags.simulate = 1;
    flags.cast_shadows = 1;
    flags.cast_drop_shadows = 1;
    const weapon_info* w_info = info->w_info;
    obj_projectile_props props(0xFFFFFFFF, w_info->projectile_info.obj_item_info, flags, w_info, launch_position, orient, velocity, ZERO_VECTOR, 0xFFFFFFFF);
    projectile* proj = World.create_object_global<projectile>(props, 0xFFFFFFFF);
    if (next.seek_target != 0xFFFFFFFF)
    {
        object* target = World.object_from_handle(next.seek_target);
        if (proj != nullptr && target != nullptr && !target->is_killed())
        {
            proj->set_heat_seeking_target(next.seek_target);
        }
    }
    next_projectile++;
}

// 0x0075C720 (:826)
void air_bomb::clear_pending_launches()
{
    if (info != nullptr)
    {
        next_projectile = info->num_projectiles;
    }
}

// 0x0075C730 (:835)
void air_bomb::set_foce_attack_object(unsigned int target_handle)
{
    if (info != nullptr)
    {
        next_projectile = info->num_projectiles;
        force_attack_handle = target_handle;
        left_over_time = 0.0f;
    }
}

// 0x007A40F0 (:851)
// left_over_time counts up to zero, and setup_projectiles winds it back by the rate; launches run while it is negative.
void air_bomb::process()
{
    if (info == nullptr)
    {
        return;
    }
    left_over_time += Frametime;
    if (left_over_time < 0.0f)
    {
        launch_next_projectile();
        return;
    }
    if (setup_projectiles())
    {
        left_over_time -= fl_rand_range(info->rate_min, info->rate_max);
    }
}

// 0x0075C760 (:901)
bool air_bomb::initialize(const air_bomb_info* ab_info, unsigned int follow_obj, unsigned int origin_obj, bool following)
{
    const object* follow = World.object_from_handle(follow_obj);
    if (follow == nullptr)
    {
        return false;
    }
    const object* origin = World.object_from_handle(origin_obj);
    if (origin == nullptr)
    {
        return false;
    }
    if (following)
    {
        focus = follow_obj;
    }
    position = follow->pos;
    origin_position = origin->pos;
    if (ab_info == nullptr)
    {
        return false;
    }
    info = ab_info;
    return true;
}

// 0x0075C7E0 (:931)
void air_bomb_init()
{
    air_bomb_load_table(false);
    Air_bomb_scatter_effect_handle = effects_get_handle("wep_howitzer_shell_split_a");
}

// 0x0078B2D0 (:938)
void air_bomb_level_init()
{
    Air_bomb_avoid_list.clear();
    Air_bomb_attack_list.clear();
}

// 0x007729F0 (:944)
void air_bomb_level_shutdown()
{
    air_bomb_stop_all();
}

// 0x007A4190 (:949)
void air_bomb_do_frame()
{
    for (int i = 0; i < MAX_AIR_BOMBS; i++)
    {
        Air_bombs[i].process();
    }
}

// 0x00772A00 (:956)
void air_bomb_render_debug()
{
    if (!Air_bomb_debug)
    {
        return;
    }
    grd_set_color(0, 0xFF, 0, 0xFF);
    for (int i = 0; i < Air_bomb_avoid_list.size(); i++)
    {
        object* objp = World.object_from_handle(Air_bomb_avoid_list[i].handle);
        if (objp != nullptr)
        {
            vector bmin;
            vector bmax;
            objp->get_bbox(bmin, bmax);
            const float padding = Air_bomb_avoid_additional_padding;
            bmin.set(bmin.x - padding, bmin.y - padding, bmin.z - padding);
            bmax.set(bmax.x + padding, bmax.y + padding, bmax.z + padding);
            grd_bbox_aligned(bmin, bmax, GM_FILTER_Z);
        }
    }
    grd_set_color(0xFF, 0, 0, 0xFF);
    for (int i = 0; i < Air_bomb_attack_list.size(); i++)
    {
        object* objp = World.object_from_handle(Air_bomb_attack_list[i].handle);
        if (objp != nullptr)
        {
            vector bmin;
            vector bmax;
            objp->get_bbox(bmin, bmax);
            grd_bbox_aligned(bmin, bmax, GM_FILTER_Z);
        }
    }
}

// 0x0075C800 (:987)
int air_bomb_start(const air_bomb_info* ab_info, unsigned int target_handle, unsigned int origin_handle, bool following)
{
    for (int i = 0; i < MAX_AIR_BOMBS; i++)
    {
        if (!Air_bombs[i].is_running())
        {
            return Air_bombs[i].initialize(ab_info, target_handle, origin_handle, following) ? i : -1;
        }
    }
    return -1;
}

// 0x0075C8A0 (:1011)
// The asm compares unsigned, so the -1 air_bomb_start answers when it fails is refused here.
void air_bomb_stop(air_bomb_handle handle)
{
    if ((unsigned int)handle < (unsigned int)MAX_AIR_BOMBS)
    {
        Air_bombs[handle].clear();
    }
}

// 0x0075C8F0 (:1018)
void air_bomb_stop_type(const air_bomb_info* ab_info)
{
    for (int i = 0; i < MAX_AIR_BOMBS; i++)
    {
        if (Air_bombs[i].get_current_info() == ab_info)
        {
            Air_bombs[i].clear();
        }
    }
}

// 0x0075CAB0 (:1028)
void air_bomb_stop_all()
{
    for (int i = 0; i < MAX_AIR_BOMBS; i++)
    {
        Air_bombs[i].clear();
    }
}

// 0x0078B2E0 (:1037)
// Original: takes the object off the attack list too. A time of zero or less never runs out.
void air_bomb_avoid_list_add(unsigned int avoid_handle, int time)
{
    air_bomb_attack_list_remove(avoid_handle);
    if (Air_bomb_avoid_list.full())
    {
        return;
    }
    obj_timed_list_item item;
    item.handle = avoid_handle;
    if (time >= 1)
    {
        // Magic number: the time is in seconds.
        item.timer.set(time * 1000);
    }
    Air_bomb_avoid_list.add(item);
}

// 0x00772BE0 (:1057)
void air_bomb_avoid_list_remove(unsigned int avoid_handle)
{
    for (int i = 0; i < Air_bomb_avoid_list.size(); i++)
    {
        if (Air_bomb_avoid_list[i].handle == avoid_handle)
        {
            Air_bomb_avoid_list.remove(i);
            i--;
        }
    }
}

// 0x00772C50 (:1069)
void air_bomb_avoid_list_clear()
{
    Air_bomb_avoid_list.clear();
}

// 0x0078B3B0 (:1075)
void air_bomb_attack_list_add(unsigned int attack_handle, int time)
{
    air_bomb_avoid_list_remove(attack_handle);
    air_bomb_attack_list_remove(attack_handle);
    if (Air_bomb_attack_list.full())
    {
        return;
    }
    obj_timed_list_item item;
    item.handle = attack_handle;
    item.timer.set(time * 1000);
    Air_bomb_attack_list.add(item);
}

// 0x00772C60 (:1093)
void air_bomb_attack_list_remove(unsigned int attack_handle)
{
    for (int i = 0; i < Air_bomb_attack_list.size(); i++)
    {
        if (Air_bomb_attack_list[i].handle == attack_handle)
        {
            Air_bomb_attack_list.remove(i);
            i--;
        }
    }
}

// 0x00772CD0 (:1105)
void air_bomb_attack_list_clear()
{
    Air_bomb_attack_list.clear();
}

// 0x0075CBF0 (:1110)
void air_bomb_clear_pending_launches()
{
    for (int i = 0; i < MAX_AIR_BOMBS; i++)
    {
        Air_bombs[i].clear_pending_launches();
    }
}

// 0x0078B470 (:1117)
// Humans, movers and vehicles only.
void air_bomb_destroy_target(unsigned int target_handle, int time)
{
    air_bomb_clear_pending_launches();
    Force_attack_list = true;
    object* objp = World.object_from_handle(target_handle);
    if (objp != nullptr && (objp->get_obj_type() == OT_MOVER || objp->get_obj_type() == OT_HUMAN || objp->get_obj_type() == OT_VEHICLE) && !objp->is_killed())
    {
        air_bomb_attack_list_add(target_handle, time);
    }
}

// 0x0078B4C0 (:1135)
void air_bomb_kill_player()
{
    for (int i = 0; i < MAX_AIR_BOMBS; i++)
    {
        if (Air_bombs[i].is_running())
        {
            Air_bomb_attack_list.clear();
            Target_player = true;
            air_bomb_destroy_target(player_get_local_player()->get_handle(), 0);
            return;
        }
    }
}

// 0x0075CC80 (:1149): the first running bomb.
void air_bomb_hit_target(unsigned int target_handle)
{
    for (int i = 0; i < MAX_AIR_BOMBS; i++)
    {
        if (Air_bombs[i].is_running())
        {
            Air_bombs[i].set_foce_attack_object(target_handle);
            return;
        }
    }
}

// 0x0075CCD0 (:1162)
bool air_bomb_is_running()
{
    for (int i = 0; i < MAX_AIR_BOMBS; i++)
    {
        if (Air_bombs[i].is_running())
        {
            return true;
        }
    }
    return false;
}

// 0x00772CE0 (:1173)
bool air_bomb_is_running_by_index(int index)
{
    for (int i = 0; i < MAX_AIR_BOMBS; i++)
    {
        if (Air_bombs[i].get_current_info() == &Air_bomb_infos[index])
        {
            return true;
        }
    }
    return false;
}

// 0x0075CCF0 (:1185)
const air_bomb_info* air_bomb_definition_from_index(int index)
{
    return &Air_bomb_infos[index];
}

// 0x0075CD00 (:1192)
// Original: no name answers the first definition, an unknown one -1 with a message.
int air_bomb_definition_get_index_from_name(const char* name)
{
    if (name == nullptr)
    {
        return 0;
    }
    for (int i = 0; i < Num_air_bomb_infos; i++)
    {
        if (v_stricmp(name, Air_bomb_infos[i].name) == 0)
        {
            return i;
        }
    }
    keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
    return -1;
}
