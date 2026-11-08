// Original: volition/rfg/code/gameplay/gameflow/gfm_orders_manager.cpp
//
// Orders (stub_impl.md S9): what a squad is told to do (a type, a team, a target object or position). The manager owns the
// 128 orders; a handle packs the pool index with an instance id, so a handle goes stale when its orders are freed.

#include "volition/rfg/code/gameplay/gameflow/gfm_orders_manager.h"

#include "volition/rfg/code/destruction/rfg_mover.h"
#include "volition/rfg/code/gameplay/activities/gfm_reinforcement_coordinator_internal.h"
#include "volition/rfg/code/gameplay/activities/gfm_smash_building_coordinator.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/object_squad.h"

// Magic numbers: a handle's low 8 bits are the pool index (128 orders), the rest the instance id; clean up above 96 in use;
// polls every 0.75 to 1.5 s for dead targets and every 2 to 4 s for moved ones.
static const unsigned int ORDERS_INDEX_MASK = 0xFF;
static const int ORDERS_INSTANCE_SHIFT = 8;
static const int MAX_ORDERS_INSTANCE = 0xFFFFFF;
static const unsigned int ORDERS_CLEANUP_THRESHOLD = 0x60;
static const int FIRST_ORDERS_INSTANCE = 1;

static const gfm_orders_flags DEFAULT_ORDERS_FLAGS(true, false); // 0x02270D39

extern gfm_smash_building_coordinator Smash_building_coordinator; // gameplay.cpp

// 0x0075DC20 (:84)
void gfm_orders_flags::reset()
{
    destroy_on_death = false;
    limited_reassignment = false;
    had_squad_assigned = false;
}

// 0x0075DC30 (:89)
gfm_orders_flags::gfm_orders_flags(bool destroy_on_death_, bool limited_reassignment_)
{
    reset();
    destroy_on_death = destroy_on_death_;
    limited_reassignment = limited_reassignment_;
}

// 0x005DDAE0 (.h:71)
gfm_orders_flags::gfm_orders_flags()
{
    reset();
}

// 0x005DDAF0 (.h:81)
gfm_orders_creation_params::gfm_orders_creation_params()
{
    reset();
}

// 0x0075DC60 (:109)
void gfm_orders_creation_params::reset()
{
    orders_type = GFM_ORDERS_TYPE_INVALID;
    team = HUMAN_TEAM_NONE;
    location_target.zero();
    object_target = 0xFFFFFFFF;
    ofp = DEFAULT_ORDERS_FLAGS;
}

// 0x0075DC90 (:119)
bool gfm_orders_creation_params::is_valid() const
{
    return (int)orders_type >= 0 && (int)orders_type < NUM_GFM_ORDERS_TYPES && !location_target.is_null() && (int)team >= 0 && (int)team < NUM_HUMAN_TEAMS;
}

// 0x0075DCC0 (:142)
human_teams gfm_orders_creation_params::get_team() const
{
    return team;
}

// 0x0077E260 (.h:123)
gfm_orders::gfm_orders()
{
    clear_orders();
}

// 0x0075DCD0 (:152)
void gfm_orders::clear_orders()
{
    handle = 0xFFFFFFFF;
    object_target = 0xFFFFFFFF;
    location_target.zero();
    orders_type = GFM_ORDERS_TYPE_INVALID;
    team = HUMAN_TEAM_NONE;
    squad_change_period.set(0);
    orders_flags.reset();
}

// 0x0075DD10 (:171)
bool gfm_orders::create_orders(const gfm_orders_creation_params& creation_params)
{
    if (!creation_params.is_valid())
    {
        return false;
    }
    orders_type = creation_params.orders_type;
    object_target = creation_params.object_target;
    location_target = creation_params.location_target;
    team = creation_params.team;
    orders_flags = creation_params.ofp;
    return true;
}

// 0x0075DD80 (:186)
bool gfm_orders::reinforcement_orders() const
{
    return orders_type == GFM_ORDERS_TYPE_ATTACK;
}

// 0x0075DD90 (:194)
const vector& gfm_orders::get_orders_pos() const
{
    return location_target;
}

// 0x0075DDA0 (:201)
void gfm_orders::update_orders_position(const vector& new_pos)
{
    location_target = new_pos;
}

// 0x0078BD10 (:207)
void gfm_orders::populate_building_list(const farray<object const*,16>& target_building_list)
{
    building_list.clear();
    for (int i = 0; i < target_building_list.size(); i++)
    {
        building_list.add(target_building_list[i]->get_handle());
    }
}

// Inlined in get_best_building and any_valid_building.
static rfg_mover* usable_building(unsigned int handle)
{
    rfg_mover* mover = handle != 0xFFFFFFFF ? World.object_from_handle<rfg_mover>(handle) : nullptr;
    return mover != nullptr && mover->can_check_building() && mover->has_building_size() && mover->has_building_mass() && !mover->is_killed() ? mover : nullptr;
}

// 0x007A5020 (:220): the nearest building nothing else attacks, else the nearest.
rfg_mover* gfm_orders::get_best_building(const npc* np)
{
    rfg_mover* nearest = nullptr;
    rfg_mover* nearest_free = nullptr;
    float nearest_dist = 3.4028235e+38f;
    float nearest_free_dist = 3.4028235e+38f;
    for (int i = 0; i < building_list.size(); i++)
    {
        rfg_mover* mover = usable_building(building_list[i]);
        if (mover == nullptr)
        {
            continue;
        }
        float dist = vec_dist_squared(mover->pos, np->pos);
        if (dist < nearest_dist)
        {
            nearest_dist = dist;
            nearest = mover;
        }
        if (!Smash_building_coordinator.is_something_else_attacking(mover->get_handle(), this) && dist < nearest_free_dist)
        {
            nearest_free_dist = dist;
            nearest_free = mover;
        }
    }
    return nearest_free != nullptr ? nearest_free : nearest;
}

// 0x007738F0 (:273)
bool gfm_orders::any_valid_building()
{
    for (int i = 0; i < building_list.size(); i++)
    {
        if (usable_building(building_list[i]) != nullptr)
        {
            return true;
        }
    }
    return false;
}

// 0x0075DDC0 (:303)
void gfm_orders::set_target_object(unsigned int new_object_target)
{
    if (new_object_target != 0xFFFFFFFF)
    {
        object_target = new_object_target;
    }
}

// 0x0075DDD0 (:309)
void gfm_orders::set_target_position(const vector& new_pos)
{
    if (!new_pos.is_null())
    {
        location_target = new_pos;
    }
}

// 0x0075DE00 (:315)
void gfm_orders::set_orders_type(gfm_orders_types type)
{
    orders_type = type;
}

// 0x0075DE10 (:320)
object* gfm_orders::get_target_object() const
{
    return World.object_from_handle(object_target);
}

// 0x0079CA50 (:326)
bool gfm_orders::has_squad_assigned() const
{
    return get_assigned_squad_object() != nullptr;
}

// 0x0078BD70 (:332)
object_squad* gfm_orders::get_assigned_squad_object() const
{
    for (int i = 0; i < World.get_num_objects<object_squad>(); i++)
    {
        object_squad* squad = World.get_object<object_squad>(i);
        if (squad->get_base_orders_handle() == handle || squad->has_orders(handle))
        {
            return squad;
        }
    }
    return nullptr;
}

// 0x0078BDD0 (:347)
unsigned int gfm_orders::get_assigned_squad_handle() const
{
    const object_squad* squad = get_assigned_squad_object();
    return squad != nullptr ? squad->get_handle() : 0xFFFFFFFF;
}

// 0x007540E0 (.h:177)
bool gfm_orders::destroy_when_free() const
{
    return orders_flags.destroy_on_death;
}

// 0x005DDB10 (.h:182)
vector gfm_orders::get_location() const
{
    return location_target;
}

// 0x007540F0 (.h:185)
unsigned int gfm_orders::get_orders_handle() const
{
    return handle;
}

// 0x005DDB30 (.h:187)
unsigned int gfm_orders::get_target_object_handle() const
{
    return object_target;
}

// 0x005DDB40 (.h:204)
gfm_orders_types gfm_orders::get_orders_type() const
{
    return orders_type;
}

// 0x00754100 (.h:205)
human_teams gfm_orders::get_team() const
{
    return team;
}

// 0x00754110 (.h:208)
void gfm_orders::assign_handle(unsigned int new_handle)
{
    handle = new_handle;
}

// 0x007A8070 (.h:259)
gfm_orders_manager::gfm_orders_manager()
    : master_orders_pool(nullptr, &gfm_orders::clear_orders, 0)
{
    level_init();
}

// 0x0078BD00
gfm_orders_manager::~gfm_orders_manager()
{
}

// 0x007AB9E0 (:370)
gfm_orders* gfm_orders_manager::get_orders_object()
{
    gfm_orders* orders = master_orders_pool.alloc(0);
    if (orders == nullptr)
    {
        do_frame_clean_up_old();
        orders = master_orders_pool.alloc(0);
        if (orders == nullptr)
        {
            return nullptr;
        }
    }
    int instance = get_new_instance_id();
    orders->assign_handle(build_orders_handle(master_orders_pool.get_index(orders), instance));
    return orders;
}

// 0x0075DE20 (:399)
int gfm_orders_manager::get_new_instance_id()
{
    int instance = next_orders_instance_id;
    next_orders_instance_id++;
    if (next_orders_instance_id == MAX_ORDERS_INSTANCE)
    {
        // Original: Int3 critical function here.
        next_orders_instance_id = FIRST_ORDERS_INSTANCE;
    }
    return instance;
}

// 0x00754120 (.h:331)
int gfm_orders_manager::decompose_orders_index(unsigned int orders_handle) const
{
    return orders_handle & ORDERS_INDEX_MASK;
}

// 0x00754130 (.h:333)
unsigned int gfm_orders_manager::build_orders_handle(int index, int instance_id) const
{
    return ((unsigned int)index & ORDERS_INDEX_MASK) | ((unsigned int)instance_id << ORDERS_INSTANCE_SHIFT);
}

// 0x007A5160 (:426)
void gfm_orders_manager::level_init()
{
    next_orders_instance_id = FIRST_ORDERS_INSTANCE;
    master_orders_pool.clear();
    death_polling_timer.set(0);
    update_polling_timer.set(0);
    for (unsigned int i = 0; i < master_orders_pool.get_capacity(); i++)
    {
        master_orders_pool.get_entry(i)->clear_orders();
    }
    // Magic number: 10 s.
    load_npc_timestamp.set(10000);
}

// 0x0078BDF0 (:450): attack orders follow their target.
void gfm_orders_manager::do_frame_update()
{
    player* pp = player_get_local_player();
    // Deviation: the headless programs can run without a player, and get_orders_location reads it; rfg.exe always has one.
    vector player_orders_location;
    if (pp != nullptr)
    {
        player_orders_location = Orders_info.get_orders_location();
    }
    for (gfm_orders* orders = master_orders_pool.get_first(0); orders != nullptr; orders = master_orders_pool.get_next(orders))
    {
        if (orders->get_orders_type() != GFM_ORDERS_TYPE_ATTACK)
        {
            continue;
        }
        if (pp != nullptr && orders->get_target_object_handle() == pp->get_handle())
        {
            orders->update_orders_position(player_orders_location);
        }
        else
        {
            object* target = orders->get_target_object();
            if (target != nullptr)
            {
                orders->update_orders_position(target->pos);
            }
        }
    }
}

// 0x007A5220 (:480): defend and ambient orders end with their target.
void gfm_orders_manager::do_frame_death()
{
    gfm_orders* orders = master_orders_pool.get_first(0);
    while (orders != nullptr)
    {
        gfm_orders* next = master_orders_pool.get_next(orders);
        if (orders->get_orders_type() == GFM_ORDERS_TYPE_DEFEND || orders->get_orders_type() == GFM_ORDERS_TYPE_AMBIENT_ACTION)
        {
            object* target = orders->get_target_object();
            if (target == nullptr || (target->is_killed() && !object_is_player(target)))
            {
                master_orders_pool.free(orders);
            }
        }
        orders = next;
    }
}

// 0x007A52D0 (:514): orders no squad holds.
void gfm_orders_manager::do_frame_clean_up_old()
{
    gfm_orders* orders = master_orders_pool.get_first(0);
    while (orders != nullptr)
    {
        gfm_orders* next = master_orders_pool.get_next(orders);
        if (orders->get_assigned_squad_object() == nullptr)
        {
            master_orders_pool.free(orders);
        }
        orders = next;
    }
}

// 0x007A53E0 (:533)
void gfm_orders_manager::do_frame()
{
    if (death_polling_timer.elapsed())
    {
        do_frame_death();
        death_polling_timer.set_rand(750, 1500);
    }
    if (update_polling_timer.elapsed())
    {
        do_frame_update();
        update_polling_timer.set_rand(2000, 4000);
    }
    if (master_orders_pool.get_capacity() - (unsigned int)master_orders_pool.get_free() > ORDERS_CLEANUP_THRESHOLD)
    {
        do_frame_clean_up_old();
    }
}

// 0x00773970 (:561)
gfm_orders* gfm_orders_manager::get_orders_from_handle(unsigned int orders_handle)
{
    if (orders_handle == 0xFFFFFFFF || (orders_handle & ORDERS_INDEX_MASK) >= master_orders_pool.get_capacity())
    {
        return nullptr;
    }
    gfm_orders* orders = master_orders_pool.get_entry(orders_handle & ORDERS_INDEX_MASK);
    return orders->get_orders_handle() == orders_handle ? orders : nullptr;
}

// 0x007ABA70 (:594)
unsigned int gfm_orders_manager::create_new_orders(const gfm_orders_creation_params& creation_params)
{
    gfm_orders* orders = get_orders_object();
    if (orders == nullptr)
    {
        return 0xFFFFFFFF;
    }
    if (orders->create_orders(creation_params))
    {
        return orders->get_orders_handle();
    }
    master_orders_pool.free(orders);
    return 0xFFFFFFFF;
}

// 0x007739B0 (:622)
void gfm_orders_manager::change_orders_priority(unsigned int orders_handle, int new_priority)
{
    // Original: Int3 critical function here.
}

// 0x0075DE60 (:633)
void gfm_orders_manager::change_orders_priority(gfm_orders* orders, int new_priority)
{
    // Original: Int3 critical function here.
}

// 0x007A5450 (:646)
void gfm_orders_manager::destroy_orders(unsigned int orders_handle)
{
    gfm_orders* orders = get_orders_from_handle(orders_handle);
    if (orders != nullptr)
    {
        master_orders_pool.free(orders);
    }
}

// 0x0079CA60 (:658)
void gfm_orders_manager::destroy_orders(gfm_orders* orders)
{
    if (orders != nullptr)
    {
        master_orders_pool.free(orders);
    }
}

// 0x0079CA80 (:665)
void gfm_orders_manager::destroy_free_orders()
{
    gfm_orders* orders = master_orders_pool.get_first(0);
    while (orders != nullptr)
    {
        gfm_orders* next = master_orders_pool.get_next(orders);
        if (orders->destroy_when_free())
        {
            object_squad* squad = orders->get_assigned_squad_object();
            if (squad == nullptr || squad->is_killed())
            {
                master_orders_pool.free(orders);
            }
        }
        orders = next;
    }
}

// 0x0079CBB0 (:688)
void gfm_orders_manager::destroy_all_orders_of_type(gfm_orders_types orders_type)
{
    gfm_orders* orders = master_orders_pool.get_first(0);
    while (orders != nullptr)
    {
        gfm_orders* next = master_orders_pool.get_next(orders);
        if (orders->get_orders_type() == orders_type)
        {
            master_orders_pool.free(orders);
        }
        orders = next;
    }
}

// 0x0079CC70 (:704)
void gfm_orders_manager::destroy_all_orders_with_target(unsigned int target_handle)
{
    gfm_orders* orders = master_orders_pool.get_first(0);
    while (orders != nullptr)
    {
        gfm_orders* next = master_orders_pool.get_next(orders);
        if (orders->get_target_object_handle() == target_handle)
        {
            master_orders_pool.free(orders);
        }
        orders = next;
    }
}

// 0x0079CD30 (:720)
void gfm_orders_manager::alert_level_cleared()
{
    gfm_orders* orders = master_orders_pool.get_first(0);
    while (orders != nullptr)
    {
        gfm_orders* next = master_orders_pool.get_next(orders);
        if (orders->get_team() == HUMAN_TEAM_EDF && (orders->get_orders_type() == GFM_ORDERS_TYPE_ATTACK || orders->get_orders_type() == GFM_ORDERS_TYPE_ROADBLOCK))
        {
            master_orders_pool.free(orders);
        }
        orders = next;
    }
    for (int i = 0; i < World.get_num_objects<object_squad>(); i++)
    {
        World.get_object<object_squad>(i)->evaluate_orders();
    }
}
