// Original: volition/rfg/code/gameplay/gameflow/squad_spawn_manager.cpp
//
// The squad spawn manager (stub_impl.md S9): each frame moves squads nobody can see, spawns and despawns the members of
// squads by distance to the player, and keeps the load radius around 40 to 50 NPCs.

#include "volition/rfg/code/gameplay/gameflow/squad_spawn_manager.h"

#include "volition/rfg/code/gameplay/gameflow/gfm_orders_manager.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/object_squad.h"
#include "volition/vlib/code/util/varray.h"

#include <math.h>

extern gfm_orders_manager Orders_manager; // 0x0222DE40 (gameplay.cpp)
extern float Frametime;

// Magic numbers: squads load within 250 m and unload beyond 300 m; a member count above 50 shrinks the radius 5% to the
// farthest loaded squad, at or below 40 grows it 5%; the unload radius is the load radius / 0.8333333.
static const float MAX_SQUAD_LOAD_DISTANCE = 250.0f; // 0x01657834
static const float MAX_SQUAD_LOAD_DISTANCE_SQRD = MAX_SQUAD_LOAD_DISTANCE * MAX_SQUAD_LOAD_DISTANCE; // 0x0226EDB8
static const float MAX_SQUAD_UNLOAD_DISTANCE_SQRD = 90000.0f; // 0x01657830
static const int MAX_NPC_MEMBER_COUNT = 50;
static const int MIN_NPC_MEMBER_COUNT_TO_CHECK = 49;
static const int MIN_NPC_MEMBER_GROW_COUNT = 40; // 0x02246AE4
static const float UNLOAD_RATIO = 0.8333333f;

static squad_member_manager_data Squad_member_manager; // 0x02266238

// 0x0075E4A0 (:88)
squad_evaluation_struct::squad_evaluation_struct()
    : squad(nullptr), distance_sqrd(0.0f), should_be_loaded(false)
{
}

// 0x00774350 (:106)
squad_member_manager_data::squad_member_manager_data()
{
    reset();
}

// 0x0075E4C0 (:323)
void squad_member_manager_data::reset()
{
    squad_evaluation.set(0);
    attempt_spawn.set(0);
    attempt_despawn.set(0);
    // Magic number: a minute.
    can_poll_squads.set(60000);
    clean_up_period.set(0);
    attempt_respawn.set(0);
    current_squad_load_radius_sqrd = MAX_SQUAD_LOAD_DISTANCE_SQRD;
    current_squad_unload_radius_sqrd = MAX_SQUAD_UNLOAD_DISTANCE_SQRD;
    sm_flags.squads_dirty = false;
    sm_flags.dead_squad = false;
}

// 0x007743E0 (:176): the members the radii keep loaded. A squad already loaded stays until the unload radius.
static int evaluate_squad_list(farray<squad_evaluation_struct,100>& squad_evaluation_list, float load_radius, float unload_radius)
{
    int members = 0;
    for (int i = 0; i < squad_evaluation_list.size(); i++)
    {
        squad_evaluation_struct& entry = squad_evaluation_list[i];
        entry.should_be_loaded = false;
        if (entry.distance_sqrd <= load_radius || (!entry.squad->simulated() && entry.distance_sqrd < unload_radius))
        {
            entry.should_be_loaded = true;
            members += entry.squad->get_max_member_count();
        }
    }
    return members;
}

// 0x00774470 (:205)
static void assign_squad_load_status(farray<squad_evaluation_struct,100>& squad_evaluation_list)
{
    for (int i = 0; i < squad_evaluation_list.size(); i++)
    {
        if (squad_evaluation_list[i].should_be_loaded)
        {
            squad_evaluation_list[i].squad->set_load_dist_flag();
        }
        else
        {
            squad_evaluation_list[i].squad->set_unload_dist_flag();
        }
    }
}

// 0x007744C0 (:218)
static void reduce_squad_load_radius(farray<squad_evaluation_struct,100>& squad_evaluation_list, float& load_radius_sqrd, float& unload_radius_sqrd)
{
    int farthest = -1;
    for (int i = 0; i < squad_evaluation_list.size(); i++)
    {
        if (squad_evaluation_list[i].should_be_loaded && (farthest < 0 || squad_evaluation_list[farthest].distance_sqrd < squad_evaluation_list[i].distance_sqrd))
        {
            farthest = i;
        }
    }
    if (farthest < 0)
    {
        return;
    }
    float radius = sqrtf(squad_evaluation_list[farthest].distance_sqrd) * 0.95f;
    load_radius_sqrd = radius * 0.8333333f * radius * 0.8333333f;
    unload_radius_sqrd = radius * radius;
    squad_evaluation_list[farthest].should_be_loaded = false;
    assign_squad_load_status(squad_evaluation_list);
}

// 0x007745A0 (:261)
static void enlarge_squad_load_radius(farray<squad_evaluation_struct,100>& squad_evaluation_list, float& load_radius_sqrd, float& unload_radius_sqrd)
{
    int nearest = -1;
    for (int i = 0; i < squad_evaluation_list.size(); i++)
    {
        if (!squad_evaluation_list[i].should_be_loaded && (nearest < 0 || squad_evaluation_list[i].distance_sqrd < squad_evaluation_list[nearest].distance_sqrd))
        {
            nearest = i;
        }
    }
    if (nearest < 0)
    {
        load_radius_sqrd = MAX_SQUAD_LOAD_DISTANCE_SQRD;
        unload_radius_sqrd = MAX_SQUAD_UNLOAD_DISTANCE_SQRD;
        return;
    }
    float radius = sqrtf(squad_evaluation_list[nearest].distance_sqrd) * 1.05f;
    if (radius * radius > MAX_SQUAD_LOAD_DISTANCE_SQRD)
    {
        load_radius_sqrd = MAX_SQUAD_LOAD_DISTANCE_SQRD;
        unload_radius_sqrd = MAX_SQUAD_UNLOAD_DISTANCE_SQRD;
    }
    else
    {
        load_radius_sqrd = radius * radius;
        unload_radius_sqrd = (radius / UNLOAD_RATIO) * (radius / UNLOAD_RATIO);
        squad_evaluation_list[nearest].should_be_loaded = true;
    }
    assign_squad_load_status(squad_evaluation_list);
}

// 0x0078D1A0 (:343)
bool squad_member_manager_data::do_spawning()
{
    if (attempt_spawn.valid() && !attempt_spawn.elapsed())
    {
        return false;
    }
    for (int i = 0; i < World.get_num_objects<object_squad>(); i++)
    {
        object_squad* squad = World.get_object<object_squad>(i);
        if (squad->should_attempt_spawn() && squad->spawn_members(false, nullptr))
        {
            return true;
        }
    }
    // Magic numbers: 60 to 100 ms.
    attempt_spawn.set_rand(60, 100);
    return true;
}

// 0x0078D230 (:387)
bool squad_member_manager_data::do_despawning()
{
    if (attempt_despawn.valid() && !attempt_despawn.elapsed())
    {
        return false;
    }
    // Magic numbers: 0.5 to 1.5 s.
    attempt_despawn.set_rand(500, 1500);
    for (int i = 0; i < World.get_num_objects<object_squad>(); i++)
    {
        object_squad* squad = World.get_object<object_squad>(i);
        if (squad->should_attempt_despawn())
        {
            squad->despawn_members();
        }
    }
    return true;
}

// 0x0078D2C0 (:448)
bool squad_member_manager_data::do_squad_simulation()
{
    if (Frametime == 0.0f)
    {
        return true;
    }
    for (int i = 0; i < World.get_num_objects<object_squad>(); i++)
    {
        object_squad* squad = World.get_object<object_squad>(i);
        if (squad->simulated())
        {
            squad->simulate();
        }
        else
        {
            squad->update_position();
        }
    }
    if (!squad_evaluation.elapsed())
    {
        return true;
    }
    // Magic number: 2 s.
    squad_evaluation.set(2000);
    farray<squad_evaluation_struct,100> squad_evaluation_list;
    for (int i = 0; i < World.get_num_objects<object_squad>(); i++)
    {
        object_squad* squad = World.get_object<object_squad>(i);
        squad_evaluation_struct entry;
        entry.squad = squad;
        entry.distance_sqrd = player_find_shortest_player_xz_dist_sqrd(squad->pos);
        squad_evaluation_list.add(entry);
        squad->set_just_spawned_flag(false);
    }
    int members = evaluate_squad_list(squad_evaluation_list, current_squad_load_radius_sqrd, current_squad_unload_radius_sqrd);
    if (members > MIN_NPC_MEMBER_COUNT_TO_CHECK || current_squad_unload_radius_sqrd != MAX_SQUAD_UNLOAD_DISTANCE_SQRD)
    {
        if (members > MAX_NPC_MEMBER_COUNT)
        {
            reduce_squad_load_radius(squad_evaluation_list, current_squad_load_radius_sqrd, current_squad_unload_radius_sqrd);
            return true;
        }
        if (members <= MIN_NPC_MEMBER_GROW_COUNT)
        {
            enlarge_squad_load_radius(squad_evaluation_list, current_squad_load_radius_sqrd, current_squad_unload_radius_sqrd);
            return true;
        }
    }
    assign_squad_load_status(squad_evaluation_list);
    return true;
}

// 0x0078D4E0 (:518)
bool squad_member_manager_data::do_clean_up()
{
    if (!clean_up_period.elapsed())
    {
        return false;
    }
    // Magic numbers: 0.5 to 1.5 s.
    clean_up_period.set_rand(500, 1500);
    for (int i = 0; i < World.get_num_objects<object_squad>(); i++)
    {
        object_squad* squad = World.get_object<object_squad>(i);
        if (squad->is_player_squad())
        {
            continue;
        }
        if (squad->is_killed())
        {
            World.destroy_object(squad, false);
        }
        else if (squad->should_be_destroyed())
        {
            squad->request_despawn_all_members(NPC_DESPAWN_TYPE_LOW, nullptr, true);
        }
        else if (squad->simulated() && Orders_manager.get_orders_from_handle(squad->get_orders_handle()) == nullptr)
        {
            World.destroy_object(squad, false);
        }
    }
    return true;
}

// 0x0078D5B0 (:588)
bool squad_member_manager_data::do_frame()
{
    bool simulated = do_squad_simulation();
    bool cleaned = do_clean_up();
    bool spawned = do_spawning();
    bool despawned = do_despawning();
    return simulated || cleaned || spawned || despawned;
}

// 0x0075E520 (:627)
void ssm_game_init()
{
}

// 0x0075E530 (:633)
void ssm_level_init_pre()
{
    Squad_member_manager.reset();
}

// 0x0078D600 (:638)
bool ssm_do_frame()
{
    return Squad_member_manager.do_frame();
}
