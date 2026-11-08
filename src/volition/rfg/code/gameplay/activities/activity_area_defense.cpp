// Original: volition/rfg/code/gameplay/activities/activity_area_defense.cpp
//
// gameflow.md GF4: hold an area against waves of continuous attacks until enough vehicles or humans are dead. Started from
// a node's use prompt, its waves are Continuous Attack definitions named by activity_area_defense.xtbl.

#include "volition/rfg/code/gameplay/activities/activity_area_defense.h"
#include "volition/rfg/code/gameplay/activities/activity_area_defense_internal.h"

#include "volition/anim_lib/code/anim_lib.h"
#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/rfg/code/code_effects/explosion.h"
#include "volition/rfg/code/control/control_gameplay.h"
#include "volition/rfg/code/event_logging/gamelog.h"
#include "volition/rfg/code/gameplay/activities/activity.h"
#include "volition/rfg/code/gameplay/activities/activity_continuous_attack.h"
#include "volition/rfg/code/gameplay/activities/activity_continuous_attack_internal.h"
#include "volition/rfg/code/gameplay/activities/activity_raid.h"
#include "volition/rfg/code/gameplay/activities/activity_rewards.h"
#include "volition/rfg/code/gameplay/activities/base_activity_data.h"
#include "volition/rfg/code/gameplay/gameflow/alert_level_manager.h"
#include "volition/rfg/code/gameplay/gameflow/alert_level_manager_internal.h"
#include "volition/rfg/code/gameplay/mission_info.h"
#include "volition/rfg/code/gameplay/mission_info_shared.h"
#include "volition/rfg/code/gameplay/object_district.h"
#include "volition/rfg/code/gamewide/gameseq.h"
#include "volition/rfg/code/geom/character_instance.h"
#include "volition/rfg/code/geom/vanim.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/havok/havok_raycast.h"
#include "volition/rfg/code/hud/hud_ui_activity_complete.h"
#include "volition/rfg/code/hud/minimap.h"
#include "volition/rfg/code/interface/ui_document_manager.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_combat_turret.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice_lines.h"
#include "volition/rfg/code/objects/humans/human_weapons.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/movers/general_mover.h"
#include "volition/rfg/code/objects/object_activity_spawn.h"
#include "volition/rfg/code/objects/object_area_defense_node.h"
#include "volition/rfg/code/objects/object_spawn_node.h"
#include "volition/rfg/code/objects/object_squad.h"
#include "volition/rfg/code/objects/object_trigger_region.h"
#include "volition/rfg/code/objects/projectile.h"
#include "volition/rfg/code/objects/turret.h"
#include "volition/rfg/code/rfg.h"
#include "volition/rfg/code/scripting/action_script/script_timer.h"
#include "volition/rfg/code/spawn/spawn_object_group.h"
#include "volition/rfg/code/scripting/dialogue_manager.h"
#include "volition/rfg/code/scripting/public_address.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/vehicle/vehicle_info.h"
#include "volition/rfg/code/vehicle/vehicle_interaction.h"
#include "volition/rfg/code/vehicle/vehicle_system.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/math/crc.h"
#include "volition/vlib/code/math/ix.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/memory/rfg_split_mempool.h"
#include "volition/vlib/code/memory/static_mempool.h"
#include "volition/vlib/code/os/vconsole.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/vlib/code/util/fastxml.h"
#include "volition/vlib/code/util/string_pool.h"
#include "volition/vlib/code/util/string_util.h"
#include "volition/vlib/code/vlib_core/vlib_assert.h"

#include <stdio.h>

extern mission_handle Mission_active_index; // 0x0165150C (gameplay.cpp)
extern unsigned int Activity_node_start_on_load; // 0x01651244 (activity.cpp)

activity_area_defense_data Area_defense_activity_data; // 0x01651960

static const char* AREA_DEFENSE_KILL_TYPE_NAMES[NUM_AD_KILL_TYPES] = {"Vehicles", "Humans"}; // 0x016512F0
static const char* DEFAULT_AD_PERSONA_NAME = "DRIVER"; // 0x012DAE14
static const char* Current_ad_persona_name = DEFAULT_AD_PERSONA_NAME; // 0x016512F8
static unsigned int Area_defense_flee_timer_handle = 0xFFFFFFFF; // 0x016512FC
static voice_line_handle Ad_offcam_start_line_handle = INVALID_VOICE_LINE_HANDLE; // 0x01651300
static voice_line_handle Ad_driver_line_1_handle = INVALID_VOICE_LINE_HANDLE; // 0x01651304
static voice_line_handle Ad_driver_line_2_handle = INVALID_VOICE_LINE_HANDLE; // 0x01651308
static voice_line_handle Ad_driver_win_line_handle = INVALID_VOICE_LINE_HANDLE; // 0x0165130C
static voice_line_handle Area_defense_current_voice_line_handle = INVALID_VOICE_LINE_HANDLE; // 0x01651310
static timestamp Area_defense_out_of_range_timer; // 0x01651DDC, only system_init touches it
static timestamp Area_defense_voice_timestamp; // 0x01651DE0
static activity_area_defense_info* Area_defense_infos; // 0x0222959C
static int Num_area_defense_infos; // 0x022295A0
static int Dlc_num_area_defense_mark; // 0x022295A4
static char* Area_defense_string_pool_data; // 0x022295A8
static object_area_defense_node* Ad_voice_line_node; // 0x022295AC
static string_pool Area_defense_string_pool("Area_Defense_String_Pool"); // 0x02231B30

// Magic number: an index that names no continuous attack (activity_continuous_attack.cpp's INVALID_CONT_ATTACK_INDEX).
static const int NO_CONT_ATTACK_INDEX = 0;

// Magic number: the distance a node can be from the player to spawn or be found, 400 m squared.
static const float MAX_NODE_DIST_SQRD = 160000.0f;

// Magic number: seconds, in ms, that the player can stay outside the area.
static const int FLEE_TIME_MS = 10000;

// rfg.exe inlines this. Original: only clears what mission_never_die says was set for the activity.
static void player_clear_activity_never_die()
{
    player* pp = player_get_local_player();
    if (pp != nullptr && pp->pflags.mission_never_die)
    {
        pp->pflags.never_die = 0;
        pp->pflags.mission_never_die = 0;
    }
}

// 0x007A89B0 (:151). Deviation: leaves out the convoy end point case of the position lookup rfg.exe inlines, which an
// area defense node never is.
void unused_dcf_area_defense_start()
{
    const player* pp = player_get_local_player();
    object_area_defense_node* best = nullptr;
    float best_dist = MAX_NODE_DIST_SQRD;
    for (int i = 0; i < World.get_num_objects<object_area_defense_node>(); i++)
    {
        object_area_defense_node* node = World.get_object<object_area_defense_node>(i);
        const float dist = vec_dist_squared(node->pos, pp->pos);
        if (dist < best_dist)
        {
            best = node;
            best_dist = dist;
        }
    }
    if (best == nullptr)
    {
        console_printf("Unable to find any Area Defense nodes.");
        return;
    }
    const district* dp = district_get_from_pos(best->pos);
    activity_kill_all_except(ACTIVITY_MODE_AREA_DEFENSE);
    if (!Area_defense_activity_data.activity_base::start_activity_spawn(best, dp))
    {
        console_printf("Unable to start closest Area Defense activity.");
    }
}

// 0x00796240 (:178). Room for five more entries is kept for DLC. Original: a reload with more rows returns and leaves the
// table open, an unknown kill type leaves the field as it was, and a row with no Base_Data keeps whatever the entry held.
void load_area_defense_table(bool reload, int dlc_id)
{
    xml_element* root;
    int first = 0;
    if (dlc_id == -1)
    {
        root = xml_table_parse("activity_area_defense.xtbl");
        const int count = (int)xml_count(root, "Activity_area_defense");
        if (reload)
        {
            if (Num_area_defense_infos < count)
            {
                return;
            }
        }
        else
        {
            Num_area_defense_infos = count;
            Area_defense_infos = static_cast<activity_area_defense_info*>(Permanent_mempool.alloc((count + 5) * sizeof(activity_area_defense_info), 0));
            if (Area_defense_string_pool_data == nullptr)
            {
                Area_defense_string_pool_data = static_cast<char*>(Permanent_mempool.alloc(0x400, 0)); // Magic number: 1 KB of names
                if (Area_defense_string_pool_data == nullptr)
                {
                    keen::debug::nativePrintArgs("Insufficient memory to allocate the area defense string table");
                }
            }
        }
        Area_defense_string_pool.init(Area_defense_string_pool_data, 0x400, "Area_Defense_String_Pool", 0, 0);
        Area_defense_string_pool.clear();
    }
    else
    {
        char filename[260];
        Area_defense_string_pool.set_thread_ownership();
        Area_defense_string_pool.mark();
        sprintf(filename, "dlc%02d_%s", dlc_id, "activity_area_defense.xtbl");
        root = xml_table_parse(filename);
        const int count = (int)xml_count(root, "Activity_area_defense");
        first = Num_area_defense_infos;
        Dlc_num_area_defense_mark = Num_area_defense_infos;
        Num_area_defense_infos += count;
    }
    activity_area_defense_info* info = Area_defense_infos + first;
    for (xml_element* entry = xml_table_find_opt(root, "Activity_area_defense"); entry != nullptr; entry = xml_table_find_next(root, entry, "Activity_area_defense"), info++)
    {
        info->name = Area_defense_string_pool.add_unique(xml_table_get_req_string_ref(entry, "Name"));
        xml_table_get_req_float(info->radius, entry, "Area_radius");
        info->radius_squared = info->radius * info->radius;
        xml_table_get_req_int(info->required_kills, entry, "Required_kills");
        xml_element* kill_type = xml_find(entry, "Kill_type");
        for (int i = 0; i < NUM_AD_KILL_TYPES; i++)
        {
            if (v_stricmp(kill_type->text, AREA_DEFENSE_KILL_TYPE_NAMES[i]) == 0)
            {
                info->kill_type = (area_defense_kill_types)i;
                break;
            }
        }
        const char* base_name = xml_table_get_opt_string_ref(entry, "Base_Data");
        if (base_name != nullptr)
        {
            info->base_data = activity_data_find_handle(base_name);
        }
        info->num_reinforcements = 0;
        xml_element* list = xml_find(entry, "Reinforcements");
        for (xml_element* item = xml_find(list, "Reinforcement_item"); item != nullptr; item = xml_find_next(list, item, "Reinforcement_item"))
        {
            area_defense_reinforcement_info reinforcement;
            if (reinforcement.load(item) && info->num_reinforcements < 5) // Magic number: the array's size
            {
                info->reinforcements[info->num_reinforcements++] = reinforcement;
            }
        }
        load_standard_rewards(info->standard_rewards, entry);
        info->persona_name = nullptr;
        info->start_line_name = nullptr;
        info->end_line_name = nullptr;
        info->encourage_a_name = nullptr;
        info->encourage_b_name = nullptr;
        xml_element* voice = xml_find(entry, "voice_info");
        if (voice != nullptr)
        {
            xml_element* line = xml_find(voice, "persona_name");
            if (line != nullptr)
            {
                info->persona_name = Area_defense_string_pool.add_unique(line->text);
            }
            line = xml_find(voice, "ad_start_line");
            if (line != nullptr)
            {
                info->start_line_name = Area_defense_string_pool.add_unique(line->text);
            }
            line = xml_find(voice, "ad_end_line");
            if (line != nullptr)
            {
                info->end_line_name = Area_defense_string_pool.add_unique(line->text);
            }
            line = xml_find(voice, "ad_encourage_a");
            if (line != nullptr)
            {
                info->encourage_a_name = Area_defense_string_pool.add_unique(line->text);
            }
            line = xml_find(voice, "ad_encourage_b");
            if (line != nullptr)
            {
                info->encourage_b_name = Area_defense_string_pool.add_unique(line->text);
            }
        }
    }
    xml_table_close(root);
}

// 0x00756C40 (:326)
void area_defense_unload_dlc()
{
    Num_area_defense_infos = Dlc_num_area_defense_mark;
    Area_defense_string_pool.set_thread_ownership();
    Area_defense_string_pool.restore_to_mark(0);
}

// 0x00756C70 (:334). rfg.exe: LTCG, ad_info in ESI. The handles found before a missing one stay set.
static bool get_voice_line_handles_info(const activity_area_defense_info* ad_info)
{
    if (ad_info == nullptr || ad_info->persona_name == nullptr)
    {
        return false;
    }
    Ad_offcam_start_line_handle = voice_line_handle_from_name(ad_info->start_line_name);
    if (Ad_offcam_start_line_handle == INVALID_VOICE_LINE_HANDLE)
    {
        return false;
    }
    Ad_driver_line_1_handle = voice_line_handle_from_name(ad_info->encourage_a_name);
    if (Ad_driver_line_1_handle == INVALID_VOICE_LINE_HANDLE)
    {
        return false;
    }
    Ad_driver_line_2_handle = voice_line_handle_from_name(ad_info->encourage_b_name);
    if (Ad_driver_line_2_handle == INVALID_VOICE_LINE_HANDLE)
    {
        return false;
    }
    Ad_driver_win_line_handle = voice_line_handle_from_name(ad_info->end_line_name);
    if (Ad_driver_win_line_handle == INVALID_VOICE_LINE_HANDLE)
    {
        return false;
    }
    Current_ad_persona_name = ad_info->persona_name;
    return true;
}

// 0x00769D70 (:378). rfg.exe: LTCG, ad_node in EDI. The table's lines, else a set picked at random: any of 1 to 10 for a
// walker node, otherwise 1 to 7 stepped over 4, 8 and 9, whose lines are the walker's.
static void get_voice_line_handles(object_area_defense_node* ad_node)
{
    if (ad_node != nullptr && ad_node == Ad_voice_line_node)
    {
        return;
    }
    Ad_voice_line_node = ad_node;
    Current_ad_persona_name = DEFAULT_AD_PERSONA_NAME;
    bool walker = false;
    if (ad_node != nullptr)
    {
        if (get_voice_line_handles_info(ad_node->get_area_defense_info()))
        {
            return;
        }
        const base_activity_data* base_data = ad_node->get_base_data();
        if (base_data != nullptr && base_data->v_spawn_group != nullptr && base_data->v_spawn_group->v_info_list.size() > 0)
        {
            const vehicle_info* info = base_data->v_spawn_group->get_index(0);
            walker = info != nullptr && info->vehicle_class == VCL_WALKER;
        }
    }
    int set;
    if (walker)
    {
        set = int_rand_range(1, 10);
    }
    else
    {
        set = int_rand_range(1, 7);
        if (set > 3)
        {
            set++;
        }
        if (set > 7)
        {
            set += 2;
        }
    }
    if (set < 1)
    {
        set = 1;
    }
    else if (set > 10)
    {
        set = 10;
    }
    char number[12];
    char line[128];
    sprintf(number, set < 10 ? "0%d" : "%d", set);
    sprintf(line, "act_ad_%sA_start", number);
    Ad_offcam_start_line_handle = voice_line_handle_from_name(line);
    sprintf(line, "act_ad_%sA_encourage1", number);
    Ad_driver_line_1_handle = voice_line_handle_from_name(line);
    sprintf(line, "act_ad_%sA_encourage2", number);
    Ad_driver_line_2_handle = voice_line_handle_from_name(line);
    sprintf(line, "act_ad_%sA_win", number);
    Ad_driver_win_line_handle = voice_line_handle_from_name(line);
}

// 0x0077F850 (:474): a player kept alive for the activity still dies in a kill region.
static void maybe_kill_player()
{
    player* pp = player_get_local_player();
    if (pp == nullptr || !pp->pflags.never_die)
    {
        return;
    }
    for (int i = 0; i < World.get_num_objects<trigger_region>(); i++)
    {
        trigger_region* region = World.get_object<trigger_region>(i);
        if (!region->is_enabled() || region->get_trigger_region_type() != TRIGGER_REGION_TYPE_KILL_HUMAN)
        {
            continue;
        }
        bool inside;
        const trigger_region_shape shape = region->get_trigger_region_shape();
        if (shape == TRIGGER_REGION_SHAPE_BOX)
        {
            vector bmin;
            vector bmax;
            region->get_local_bbox(bmin, bmax);
            inside = ix_point_in_box_oriented(pp->pos, region->pos, region->orient, bmin, bmax);
        }
        else if (shape == TRIGGER_REGION_SHAPE_SPHERE)
        {
            inside = ix_point_in_sphere(region->pos, region->get_radius(), pp->pos);
        }
        else
        {
            continue;
        }
        if (inside)
        {
            player_clear_activity_never_die();
            human_damage(pp, nullptr, -1.0f, DBT_NONE, nullptr, nullptr, true, false); // Magic number: -1 is all it has
            return;
        }
    }
}

// 0x00753E60 (.h:72). Original: leaves ca_index unset, which nothing reads.
void area_defense_reinforcement_info::reset()
{
    continuous_attack_name = nullptr;
    kill_limit = 0;
    time_limit = 0;
}

// 0x00756CF0 (:525)
bool area_defense_reinforcement_info::load(xml_element* xml_reinforcement_info)
{
    reset();
    continuous_attack_name = Area_defense_string_pool.add_unique(xml_table_get_req_string_ref(xml_reinforcement_info, "Continuous_attack"));
    xml_table_get_req_int(kill_limit, xml_reinforcement_info, "Kill_Limit");
    xml_table_get_req_int(time_limit, xml_reinforcement_info, "Time_Limit");
    return true;
}

// 0x00769F00 (:539)
const int area_defense_reinforcement_info::resolve_continuous_attack_name() const
{
    return continuous_attack_name == nullptr ? NO_CONT_ATTACK_INDEX : (int)crc_stri(continuous_attack_name, 0);
}

// 0x0077DDB0 (.h:105). Original: the rest stays unset until level_init calls reset.
activity_area_defense_data::activity_area_defense_data()
    : activity_base_modal(ACTIVITY_MODE_AREA_DEFENSE, false)
{
}

// 0x00768D30
activity_area_defense_data::~activity_area_defense_data()
{
}

// 0x00796600 (:551)
void activity_area_defense_data::system_init()
{
    Area_defense_flee_timer_handle = 0xFFFFFFFF;
    Area_defense_out_of_range_timer.invalidate();
    Area_defense_voice_timestamp.invalidate();
    Area_defense_current_voice_line_handle = INVALID_VOICE_LINE_HANDLE;
    load_area_defense_table(false, -1);
}

// 0x00769F20 (:564). rfg.exe clears EDI for get_voice_line_handles, so this asks for a random set.
void activity_area_defense_data::level_init()
{
    activity_base_modal::level_init();
    reset();
    get_voice_line_handles(nullptr);
}

// 0x00756D60 (:573)
const char* activity_area_defense_data::get_title() const
{
    return "ACT_AREA_DEFENSE_NAME";
}

// 0x00756D70 (:587)
const char* activity_area_defense_data::get_subtitle() const
{
    const area_defense_kill_types kill_type = current_info->kill_type;
    if (kill_type == AD_KILL_HUMANS)
    {
        return "ACT_AREA_DEFENSE_HUMANS_SUBTITLE";
    }
    if (kill_type != AD_KILL_VEHICLES)
    {
        VLIB_CRITICAL_FAILURE();
    }
    return "ACT_AREA_DEFENSE_VEHICLE_SUBTITLE";
}

// 0x0077F980 (:609)
bool activity_area_defense_data::activity_begin_start_up()
{
    activity_suspend_diversions(ADR_ACTIVITY);
    vehicle_kills = 0;
    human_kills = 0;
    reinforcement_level = 0;
    area_defense_timestamp.invalidate();
    start_continuous_attack = false;
    return activity_base_modal::activity_begin_start_up();
}

// 0x0077F9C0 (:631). Original: can_spawn is not asked, so a finished node can be collected.
bool activity_area_defense_data::collect_spawn_objects(farray<object const*,32>& spawn_objects, const vector& player_pos, unsigned int district_handlep) const
{
    for (int i = 0; i < World.get_num_objects<object_area_defense_node>(); i++)
    {
        const object_area_defense_node* node = World.get_object<object_area_defense_node>(i);
        if (vec_dist_squared(player_pos, node->pos) <= MAX_NODE_DIST_SQRD)
        {
            spawn_objects.add(node); // reports a full list
        }
    }
    return spawn_objects.size() > 0;
}

// 0x00796630 (:646)
bool activity_area_defense_data::activity_initialize(const object* spawn_objp, unsigned int new_district_handle)
{
    if (was_initialized() || is_active() || !activity_base_modal::activity_initialize(spawn_objp, new_district_handle))
    {
        return false;
    }
    stop_all_flares();
    if (spawn_objp == nullptr || spawn_objp->get_obj_type() != OT_AREA_DEFENSE_NODE)
    {
        return false;
    }
    area_defense_node_handle = spawn_objp->get_handle();
    area_defense_center = spawn_objp->pos;
    current_info = static_cast<const object_area_defense_node*>(spawn_objp)->get_area_defense_info();
    return current_info != nullptr;
}

// 0x007ACD20 (:674)
bool activity_area_defense_data::activity_start()
{
    return activity_base_modal::activity_start();
}

// 0x007966C0 (:684)
void activity_area_defense_data::activity_end()
{
    activity_reinstate_diversions(ADR_ACTIVITY);
    player_clear_activity_never_die();
    activity_base_modal::activity_end();
    ui_fade_to_black_fade_in(true);
}

// 0x007A8B50 (:696). rfg.exe asks All_activities[ACTIVITY_MODE_AREA_DEFENSE], which is this object.
void activity_area_defense_data::activity_kill()
{
    stop_all_flares();
    if (!Multiplayer && is_active())
    {
        player_equipment_cleanup();
        activity_reinstate_diversions(ADR_ACTIVITY);
    }
    activity_base_modal::activity_kill();
}

// 0x007B55C0 (:712): the player cannot die between the win and the end of the activity.
void activity_area_defense_data::record_victory()
{
    apply_standard_rewards(current_info->standard_rewards, nullptr);
    player* pp = player_get_local_player();
    if (pp != nullptr && !pp->pflags.never_die)
    {
        pp->pflags.never_die = 1;
        pp->pflags.mission_never_die = 1;
    }
    const human_voice_persona_handle persona = human_voice_persona_handle_from_name(Current_ad_persona_name);
    if (persona != INVALID_HUMAN_VOICE_PERSONA_HANDLE)
    {
        const human_voice_persona* hvpp = human_voice_persona_from_handle(persona);
        if (hvpp != nullptr)
        {
            dialogue_manager_start_dialogue(Ad_driver_win_line_handle, hvpp, DIALOGUE_PRIORITY_HIGH, INVALID_PA_HANDLE, nullptr);
        }
    }
    activity_base_modal::record_victory();
    object_area_defense_node* node = World.object_from_handle<object_area_defense_node>(area_defense_node_handle);
    if (node != nullptr)
    {
        node->set_completed();
    }
}

// 0x00769F70 (:751)
void activity_area_defense_data::render_icons() const
{
    if (!is_active())
    {
        return;
    }
    const object_area_defense_node* node = World.object_from_handle<object_area_defense_node>(area_defense_node_handle);
    if (node != nullptr)
    {
        Cartographer.add_temporary_icon(node->pos, MAP_ELEMENT_AREA_DEFENSE, true, 0xFFFFFFFF, node->is_completed() ? Completed_icon_color : Default_map_item_color, false, node->get_handle());
    }
}

// 0x0077FA70 (:772)
void activity_area_defense_data::activity_complete()
{
    activity_base_modal::activity_complete();
    reset();
}

// 0x007A05C0 (:780)
bool activity_area_defense_data::start_activity_spawn(const object* spawn_objp, const district* dp)
{
    activity_kill_all_except(ACTIVITY_MODE_AREA_DEFENSE);
    return activity_base::start_activity_spawn(spawn_objp, dp);
}

// 0x007A05F0 (:793): out of a tank, a walker or a turret and back on the ground, with the movers there destroyed. A player
// in any other vehicle, or on foot, stays where they are and keeps their state.
void activity_area_defense_data::warp_player_on_end()
{
    player* pp = player_get_local_player();
    if (pp == nullptr)
    {
        return;
    }
    vehicle* vp = World.object_from_handle<vehicle>(pp->vehicle_handle);
    if (vp != nullptr)
    {
        if (!vehicle_is_tank(vp) && vp->get_sub_type() != OT_SUB_VEHICLE_WALKER)
        {
            return;
        }
        if (!human_in_vehicle_interaction(pp, nullptr))
        {
            return;
        }
        vi_exit_data exit_data;
        exit_data.flags.teleport = 1;
        exit_data.flags.maintain_position = 1;
        vehicle_interaction_request_exit(pp, &exit_data);
    }
    else
    {
        if (human_combat_get_manned_turret(pp) == nullptr)
        {
            return;
        }
        human_combat_abandon_turret(pp);
    }
    if (area_defense_node_handle != 0xFFFFFFFF)
    {
        vector ground = pp->pos;
        raycast_input in;
        in.radius = 0.0f;
        in.start = pp->pos;
        in.start.y += 5.0f; // Magic number: from five metres up to a kilometre down
        in.end = in.start;
        in.end.y -= 1000.0f;
        in.filter_params = nullptr;
        in.filter_callback = nullptr;
        in.m_filter_info = COLLIDE_RAY_TERRAIN_ONLY;
        in.m_hit_collector_type = raycast_input::COLLECT_CLOSEST_HIT;
        in.m_flags = 0;
        raycast_output out;
        if (havok_raycast(in, out))
        {
            ground.y = out.m_hits[0].hit_pos.y;
        }
        unsigned int movers[50]; // Magic number: rfg.exe's list size
        const int num_movers = World.collect_objects<general_mover>(ground, 2.0f, movers, 50, nullptr, nullptr);
        for (int i = 0; i < num_movers; i++)
        {
            World.destroy_object(movers[i], false);
        }
        human_teleport(pp, ground, pp->orient, 6.0f, true); // Magic number: rfg.exe's placement range
    }
    if (pp->char_inst->aim_handle != -1)
    {
        animlib_clear_non_looping_animations(pp->char_inst->aim_handle);
    }
    human_set_movement_mode(pp, HMM_NONE, HMSM_NONE);
    human_set_state(pp, HS_STANDARD, HMS_IDLE, -1.0f);
    human_controller_reset(pp);
    human_update_collision_filter(pp, true);
    camera_level_init_post();
    vanim_clear_states(pp->char_inst->vp);
}

// 0x00769FD0 (:886). Only EDF kills count: the vehicle's last driver's team, or the human's own.
void activity_area_defense_data::register_death(const object* target_obj, const object* attacker_obj)
{
    if (!is_active() || active_state != AD_ACTIVE_STATE_RUNNING || target_obj == nullptr)
    {
        return;
    }
    if (target_obj->get_obj_type() == OT_VEHICLE && static_cast<const vehicle*>(target_obj)->team_of_most_recent_driver == HUMAN_TEAM_EDF)
    {
        vehicle_kills++;
    }
    if (target_obj->get_obj_type() == OT_HUMAN && static_cast<const human*>(target_obj)->current_team == HUMAN_TEAM_EDF)
    {
        human_kills++;
    }
}

// 0x0076A020 (:905). rfg.exe asks All_activities[ACTIVITY_MODE_AREA_DEFENSE], which is this object.
void activity_area_defense_data::render_regions() const
{
    static const color AREA_DEFENSE_AREA_ACTIVE_COLOR(50, 200, 50, 150); // 0x02273D58
    static const color AREA_DEFENSE_AREA_INACTIVE_COLOR(128, 128, 128, 150); // 0x02273D54
    if (current_info == nullptr)
    {
        return;
    }
    const bool active = !Multiplayer && is_active();
    Cartographer.add_decal(area_defense_center, MAP_ELEMENT_CIRCLE_FILLED_OUTLINE, current_info->radius, active ? AREA_DEFENSE_AREA_ACTIVE_COLOR : AREA_DEFENSE_AREA_INACTIVE_COLOR, active);
}

// 0x0077FA90 (:921)
const player* activity_area_defense_data::player_in_area_defense_region()
{
    if (current_info == nullptr)
    {
        return nullptr;
    }
    for (int i = 0; i < World.get_num_objects<player>(); i++)
    {
        const player* pp = World.get_object<player>(i);
        if (vec_xz_dist_squared(pp->pos, area_defense_center) <= current_info->radius_squared)
        {
            return pp;
        }
    }
    return nullptr;
}

// 0x007A8BA0 (:939)
void activity_area_defense_data::activity_do_frame_ready()
{
    activity_base::activity_do_frame_ready();
    if (active_state == AD_ACTIVE_STATE_LOAD_LAYERS)
    {
        do_frame_load_layers();
    }
}

// 0x00756DB0 (:949)
bool activity_area_defense_data::activity_clean_up_completed() const
{
    return activity_base_modal::activity_clean_up_completed(); // rfg.exe inlines the same body
}

// 0x007ACD30 (:955): the alert falls back to the bottom of green.
void activity_area_defense_data::activity_do_frame_clean_up()
{
    activity_base::activity_do_frame_clean_up();
    gsm_set_alert_value(Alert_level_ranges[0].min_value);
}

// 0x00756DE0 (:963)
void activity_area_defense_data::start_cleanup()
{
    active_state = AD_ACTIVE_STATE_CLEANUP;
    ui_fade_to_black_fade_out(false);
    control_enable_player_input(false);
}

// 0x007A0890 (:973): tanks, walkers and turrets of the layer go, then hostile NPCs of the continuous attacks.
void activity_area_defense_data::player_equipment_cleanup()
{
    warp_player_on_end();
    for (int i = 0; i < layer_objects.size(); i++)
    {
        const object_vehicle_spawn_node* vehicle_node = World.object_from_handle<object_vehicle_spawn_node>(layer_objects[i]);
        if (vehicle_node != nullptr)
        {
            vehicle* vp = World.object_from_handle<vehicle>(vehicle_node->get_child_vehicle_handle());
            if (vp != nullptr && (vehicle_is_tank(vp) || vp->get_sub_type() == OT_SUB_VEHICLE_WALKER))
            {
                World.destroy_object(vp, false);
            }
        }
        const object_turret_spawn_node* turret_node = World.object_from_handle<object_turret_spawn_node>(layer_objects[i]);
        if (turret_node != nullptr)
        {
            turret* tp = World.object_from_handle<turret>(turret_node->get_child_turret_handle());
            if (tp != nullptr)
            {
                World.destroy_object(tp, false);
            }
        }
    }
    squad_mission_clean_up();
    const player* pp = player_get_local_player();
    if (pp == nullptr)
    {
        return;
    }
    for (int i = 0; i < World.get_num_objects<npc>(); i++)
    {
        npc* np = World.get_object<npc>(i);
        if (human_teams_get_disposition(pp, np) == TEAM_DISPOSITION_HOSTILE && np->npc_flags.activity_pursuit_ca)
        {
            World.destroy_object(np, false);
        }
    }
}

// 0x007B5670 (:1019). rfg.exe asks All_activities[ACTIVITY_MODE_AREA_DEFENSE], which is this object. A lost activity stays
// on the splash screen until its buttons call activity_area_defense_exit or _replay.
void activity_area_defense_data::activity_do_frame_active()
{
    switch (active_state)
    {
    case AD_ACTIVE_STATE_WAIT_PLAYER:
        do_frame_active_wait_player();
        break;
    case AD_ACTIVE_STATE_RUNNING:
        do_frame_active_running();
        break;
    case AD_ACTIVE_STATE_WAIT_SPLASH_SCREEN:
        if (player_wins_activity && !Multiplayer && get_interface_state_duration() > 2000) // Magic number: two seconds of the win
        {
            start_cleanup();
        }
        break;
    case AD_ACTIVE_STATE_CLEANUP:
        do_frame_cleanup();
        break;
    case AD_ACTIVE_STATE_END:
        activity_end();
        break;
    case AD_ACTIVE_STATE_LOAD_LAYERS:
        do_frame_load_layers();
        break;
    default:
        VLIB_CRITICAL_FAILURE();
        activity_kill();
        break;
    }
    maybe_kill_player();
}

// 0x0077FB30 (:1060)
void activity_area_defense_data::stop_all_flares() const
{
    for (int i = 0; i < World.get_num_objects<object_area_defense_node>(); i++)
    {
        object_area_defense_node* node = World.get_object<object_area_defense_node>(i);
        if (node->flare_started())
        {
            node->stop_flare();
        }
    }
}

// 0x007A8BC0 (:1071)
activity_load_state activity_area_defense_data::activity_do_frame_loading()
{
    if (active_state != AD_ACTIVE_STATE_LOAD_LAYERS)
    {
        active_state = AD_ACTIVE_STATE_LOAD_LAYERS;
    }
    do_frame_load_layers();
    return activity_base::activity_do_frame_loading();
}

// 0x007A8BF0 (:1083)
void activity_area_defense_data::activity_do_frame_inactive()
{
    if (Multiplayer)
    {
        return;
    }
    if (!activity_area_defense_can_be_started())
    {
        stop_all_flares();
        return;
    }
    for (int i = 0; i < World.get_num_objects<object_activity_spawn>(); i++)
    {
        const object_activity_spawn* start = World.get_object<object_activity_spawn>(i);
        if (start->get_activity_type() != OT_AREA_DEFENSE_NODE)
        {
            continue;
        }
        object_area_defense_node* node = World.object_from_handle<object_area_defense_node>(start->get_activity_handle());
        if (node == nullptr || !node->can_spawn())
        {
            continue;
        }
        if (!node->flare_started())
        {
            node->start_flare(start);
        }
        Cartographer.add_temporary_icon(start->pos, MAP_ELEMENT_AREA_DEFENSE, false, 0xFFFFFFFF, node->is_completed() ? Completed_icon_color : Default_map_item_color, false, node->get_handle());
    }
}

// 0x00756E00 (:1131)
bool activity_area_defense_data::on_alert(const npc* np) const
{
    return is_active();
}

// 0x00756E10 (:1140)
void activity_area_defense_data::save_activity_state(cfile* save_file)
{
    activity_base::save_activity_state(save_file); // rfg.exe inlines the same body
}

// 0x00756E40 (:1145)
void activity_area_defense_data::load_activity_state(cfile* save_file)
{
    activity_base::load_activity_state(save_file); // rfg.exe inlines the same body
}

// 0x00756E70 (:1160)
void activity_area_defense_data::reset()
{
    area_defense_node_handle = 0xFFFFFFFF;
    active_state = INVALID_AD_ACTIVE_STATE;
    vehicle_kills = 0;
    human_kills = 0;
    reinforcement_level = 0;
    area_defense_timestamp.invalidate();
    start_continuous_attack = false;
    processed_completion = false;
    player_wins_activity = false;
    began_startup = false;
    current_info = nullptr;
    if (Area_defense_flee_timer_handle != 0xFFFFFFFF)
    {
        script_timer_delete(Area_defense_flee_timer_handle);
        Area_defense_flee_timer_handle = 0xFFFFFFFF;
    }
    Area_defense_voice_timestamp.invalidate();
    Area_defense_current_voice_line_handle = INVALID_VOICE_LINE_HANDLE;
    activity_cooldown_period.set(0);
}

// 0x007B1440 (:1191): picks the next wave, the smallest with limits no lower than the current one's, and starts it once
// its kills or seconds are reached. Starting takes a frame of its own: the old attack ends, and the next frame's call
// starts the new one, retrying until it can. Original: reinforcements[0] is the first wave, started by
// do_frame_active_wait_player, and only the first two changes of wave say a line.
void activity_area_defense_data::test_reinforcement_conditions()
{
    if (start_continuous_attack)
    {
        const player* pp = player_get_local_player();
        if (pp != nullptr && activity_cont_attack_start(pp, current_info->reinforcements[reinforcement_level].resolve_continuous_attack_name()))
        {
            start_continuous_attack = false;
        }
        return;
    }
    const area_defense_reinforcement_info& current = current_info->reinforcements[reinforcement_level];
    int next = -1;
    for (int i = 0; i < current_info->num_reinforcements; i++)
    {
        const area_defense_reinforcement_info& candidate = current_info->reinforcements[i];
        if (i != reinforcement_level && current.kill_limit <= candidate.kill_limit && current.time_limit <= candidate.time_limit && (next == -1 || (candidate.kill_limit <= current_info->reinforcements[next].kill_limit && candidate.time_limit <= current_info->reinforcements[next].time_limit)))
        {
            next = i;
        }
    }
    if (next == -1)
    {
        return;
    }
    int kills = 0;
    if (current_info->kill_type == AD_KILL_VEHICLES)
    {
        kills = vehicle_kills;
    }
    else if (current_info->kill_type == AD_KILL_HUMANS)
    {
        kills = human_kills;
    }
    else
    {
        VLIB_CRITICAL_FAILURE();
    }
    const int seconds = area_defense_timestamp.time_since() / 1000;
    if (current_info->reinforcements[next].kill_limit > kills && current_info->reinforcements[next].time_limit > seconds)
    {
        return;
    }
    reinforcement_level = next;
    activity_cont_attack_end(); // rfg.exe inlines it, and its reset
    start_continuous_attack = true;
    if (Area_defense_current_voice_line_handle == INVALID_VOICE_LINE_HANDLE)
    {
        Area_defense_current_voice_line_handle = Ad_driver_line_1_handle;
    }
    else if (Area_defense_current_voice_line_handle == Ad_driver_line_1_handle)
    {
        Area_defense_current_voice_line_handle = Ad_driver_line_2_handle;
    }
    else
    {
        return;
    }
    Area_defense_voice_timestamp.set(10000); // Magic number: ten seconds later
}

// 0x0076A0E0 (:1284)
void activity_area_defense_data::test_complete_conditions()
{
    const player* pp = player_get_local_player();
    if (pp == nullptr || pp->is_killed())
    {
        return;
    }
    int kills;
    if (current_info->kill_type == AD_KILL_VEHICLES)
    {
        kills = vehicle_kills;
    }
    else if (current_info->kill_type == AD_KILL_HUMANS)
    {
        kills = human_kills;
    }
    else
    {
        VLIB_CRITICAL_FAILURE();
        return;
    }
    if (kills >= current_info->required_kills)
    {
        process_completion_condition(AD_COMPLETE_SUCCESS);
    }
}

// 0x00756EE0 (:1322)
bool activity_area_defense_data::is_inside_region(const vector& point) const
{
    return vec_xz_dist_squared(point, area_defense_center) <= current_info->radius_squared;
}

// 0x00756F30 (:1328). Original: the GPS target asked for while the player is outside is dropped.
void activity_area_defense_data::do_frame_active_wait_player()
{
    player* pp = player_get_local_player();
    if (pp == nullptr)
    {
        return;
    }
    const bool inside = is_inside_region(pp->pos);
    pp->hflags.activity_engage = inside;
    if (!inside)
    {
        Cartographer.get_gps_target_activity_handle();
        return;
    }
    area_defense_timestamp.set(0);
    start_continuous_attack = true;
    active_state = AD_ACTIVE_STATE_RUNNING;
}

// 0x007B1610 (:1366). Original: the second half of the flee test, read from the asm, fails the activity for a player who
// has come back inside the cylinder, where it can only answer false, since player_in_area_defense_region was false.
void activity_area_defense_data::do_frame_active_running()
{
    const player* pp = player_get_local_player();
    if (pp == nullptr)
    {
        return;
    }
    if (player_in_area_defense_region() != nullptr)
    {
        if (Area_defense_flee_timer_handle != 0xFFFFFFFF)
        {
            script_timer_delete(Area_defense_flee_timer_handle);
            Area_defense_flee_timer_handle = 0xFFFFFFFF;
        }
    }
    else if (Area_defense_flee_timer_handle == 0xFFFFFFFF)
    {
        Area_defense_flee_timer_handle = script_timer_create(FLEE_TIME_MS);
        if (Area_defense_flee_timer_handle != 0xFFFFFFFF)
        {
            script_timer_display(Area_defense_flee_timer_handle, TP_DEFAULT);
        }
    }
    else if (script_timer_get_time(Area_defense_flee_timer_handle) == 0 || is_in_cylindrical_region(pp->pos, area_defense_center, current_info->radius_squared))
    {
        gamelog_set_activity_failed_reason(GLCR_OUT_OF_RANGE);
        process_completion_condition(AD_COMPLETE_LEFT_AREA); // rfg.exe inlines it
    }
    test_reinforcement_conditions();
    test_complete_conditions();
    play_activity_music();
    if (Area_defense_voice_timestamp.valid() && Area_defense_voice_timestamp.elapsed())
    {
        Area_defense_voice_timestamp.invalidate();
        const human_voice_persona_handle persona = human_voice_persona_handle_from_name(Current_ad_persona_name);
        if (persona != INVALID_HUMAN_VOICE_PERSONA_HANDLE)
        {
            const human_voice_persona* hvpp = human_voice_persona_from_handle(persona);
            if (hvpp != nullptr)
            {
                dialogue_manager_start_dialogue(Area_defense_current_voice_line_handle, hvpp, DIALOGUE_PRIORITY_RADIO, INVALID_PA_HANDLE, nullptr);
            }
        }
    }
}

// 0x007A8CD0 (:1466)
void activity_area_defense_data::do_frame_cleanup()
{
    if (!ui_fade_to_black_is_faded())
    {
        return;
    }
    activity_cont_attack_end(); // rfg.exe inlines it, and its reset
    human_weapons_cancel_reload(player_get_local_player());
    player_equipment_cleanup();
    projectile_invalidate_pending_charges();
    explosion_clear_all_pending();
    active_state = AD_ACTIVE_STATE_END;
    control_enable_player_input(true);
}

// 0x007A09F0 (:1490): fades out, starts up under the black, and fades back in to wait for the player.
void activity_area_defense_data::do_frame_load_layers()
{
    static bool input_enabled; // 0x0165C916
    human_weapons_cancel_reload(player_get_local_player());
    if (ui_fade_to_black_is_fade_in_complete() && gameseq_get_state() == GS_GAMEPLAY && gameseq_get_pending_state() == GS_NONE)
    {
        ui_fade_to_black_fade_out(false);
        input_enabled = control_player_input_enabled();
        control_enable_player_input(false);
    }
    if (!ui_fade_to_black_is_faded())
    {
        return;
    }
    if (began_startup)
    {
        control_enable_player_input(input_enabled);
        ui_fade_to_black_fade_in(true);
        active_state = AD_ACTIVE_STATE_WAIT_PLAYER;
        return;
    }
    if (!can_start())
    {
        return;
    }
    began_startup = activity_begin_start_up();
    if (!began_startup)
    {
        ui_fade_to_black_instant_fade_in();
        control_enable_player_input(true);
        player_get_local_player()->pflags.disable_controls = 0;
        player_equipment_cleanup();
        activity_kill();
    }
}

// 0x00756FD0 (:1533). A win waits on its own timer in activity_do_frame_active, with no complete screen asked for here.
void activity_area_defense_data::process_completion_condition(area_defense_complete_conditions condition)
{
    if (processed_completion)
    {
        return;
    }
    processed_completion = true;
    bool wait = false;
    if (condition == AD_COMPLETE_SUCCESS)
    {
        record_victory();
        wait = true;
        player_wins_activity = true;
    }
    else if (condition == AD_COMPLETE_PLAYER_KILLED || condition == AD_COMPLETE_LEFT_AREA)
    {
        record_loss();
        wait = hud_ui_activity_complete_start(ACTIVITY_MODE_AREA_DEFENSE, false);
        player_wins_activity = false;
    }
    else
    {
        VLIB_CRITICAL_FAILURE();
    }
    if (Area_defense_flee_timer_handle != 0xFFFFFFFF)
    {
        script_timer_delete(Area_defense_flee_timer_handle);
        Area_defense_flee_timer_handle = 0xFFFFFFFF;
    }
    if (wait)
    {
        active_state = AD_ACTIVE_STATE_WAIT_SPLASH_SCREEN;
    }
    else
    {
        activity_end();
    }
}

// 0x00757090 (:1579)
bool activity_area_defense_data::object_setup()
{
    return true;
}

// 0x00753E70 (.h:203)
bool activity_area_defense_data::is_running() const
{
    return is_active() && active_state == AD_ACTIVE_STATE_RUNNING;
}

// 0x00753E90 (.h:236)
bool activity_area_defense_data::area_defense_ready() const
{
    return current_info != nullptr;
}

// 0x007570A0 (:1591)
void activity_area_defense_exit()
{
    const player* pp = player_get_local_player();
    if (pp != nullptr && !pp->is_killed())
    {
        Area_defense_activity_data.start_cleanup(); // rfg.exe inlines it
    }
}

// 0x007570E0 (:1604)
void activity_area_defense_replay()
{
    Activity_node_start_on_load = Area_defense_activity_data.get_spawn_object_handle();
    World.save_and_reload();
}

// 0x00757100 (:1613)
void activity_area_defense_process_player_death()
{
    gamelog_set_activity_failed_reason(GLCR_PLAYER_DIED);
    Area_defense_activity_data.process_completion_condition(AD_COMPLETE_PLAYER_KILLED); // rfg.exe inlines it
}

// 0x007A0AC0 (:1620)
bool activity_area_defense_start(const object_area_defense_node* node)
{
    const district* dp = World.object_from_handle<district>(activity_int_get_district_handle_from_node(node));
    activity_kill_all_except(ACTIVITY_MODE_AREA_DEFENSE);
    return Area_defense_activity_data.activity_base::start_activity_spawn(node, dp);
}

// 0x007A0B10 (:1627). Magic number: 1 is the walker DLC's bundle, whose levels spawn walkers regardless.
bool activity_area_defense_can_be_started()
{
    return !Multiplayer && activity_none() && Mission_active_index == INVALID_MISSION_HANDLE && !mission_prevent_activity_spawn() && (Allow_walker_spawn || World.dlc_bundle_id == 1) && Area_defense_activity_data.get_state() == BASE_ACTIVITY_STATE_WAIT && Area_defense_activity_data.activity_base_modal::can_initialize() && (int)gsm_get_alert_level() < 2; // Magic number: below orange
}

// 0x007A0B70 (:1677)
bool activity_area_defense_play_start_voiceline(object_area_defense_node* ad_node)
{
    if (!activity_area_defense_can_be_started() || ad_node == nullptr)
    {
        return false;
    }
    get_voice_line_handles(ad_node);
    const human_voice_persona* hvpp = human_voice_radio_persona();
    if (hvpp != nullptr)
    {
        dialogue_manager_start_dialogue(Ad_offcam_start_line_handle, hvpp, DIALOGUE_PRIORITY_RADIO, INVALID_PA_HANDLE, nullptr);
    }
    return true;
}

// 0x00757180 (:1696)
bool activity_area_defense_is_running()
{
    return Area_defense_activity_data.is_running();
}

// 0x007571B0 (:1701). Original: no check for null info; the HUD only asks while the activity runs.
void activity_area_defense_get_kill_totals(int& num_killed, int& kill_goal, bool& target_vehicles)
{
    target_vehicles = false;
    num_killed = 0;
    if (Area_defense_activity_data.current_info->kill_type == AD_KILL_VEHICLES)
    {
        num_killed = Area_defense_activity_data.vehicle_kills;
        target_vehicles = true;
    }
    else if (Area_defense_activity_data.current_info->kill_type == AD_KILL_HUMANS)
    {
        num_killed = Area_defense_activity_data.human_kills;
    }
    else
    {
        VLIB_CRITICAL_FAILURE();
    }
    kill_goal = Area_defense_activity_data.current_info->required_kills;
}

// 0x0077FB90 (:1730)
object_area_defense_node* area_defense_find_node_generic(object* node)
{
    if (node == nullptr)
    {
        return nullptr;
    }
    if (node->get_obj_type() == OT_AREA_DEFENSE_NODE)
    {
        return static_cast<object_area_defense_node*>(node);
    }
    if (node->get_obj_type() != OT_ACTIVITY_SPAWN)
    {
        return nullptr;
    }
    for (int i = 0; i < World.get_num_objects<object_area_defense_node>(); i++)
    {
        object_area_defense_node* candidate = World.get_object<object_area_defense_node>(i);
        if (candidate->get_start_node() == node)
        {
            return candidate;
        }
    }
    return nullptr;
}

// 0x00757230 (:1752)
const activity_area_defense_info* get_area_defense_info_by_name(const char* name)
{
    for (int i = 0; i < Num_area_defense_infos; i++)
    {
        if (v_stricmp(Area_defense_infos[i].name, name) == 0)
        {
            return &Area_defense_infos[i];
        }
    }
    return nullptr;
}
