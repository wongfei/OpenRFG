// Original: volition/rfg/code/gameplay/activities/activity_vrail.cpp
//
// gameflow.md GF4: Riding Shotgun. Jorge drives along a patrol from a riding shotgun node while the player shoots from
// the back, scoring damage against the EDF. The story is told in chapters of voiced sections, one chapter a run.

#include "volition/rfg/code/gameplay/activities/activity_vrail.h"
#include "volition/rfg/code/gameplay/activities/activity_vrail_internal.h"

#include "keen/base/trace.hpp"
#include "volition/rfg/code/ai/ai_system.h"
#include "volition/rfg/code/destruction/rfg_mover.h"
#include "volition/rfg/code/event_logging/gamelog.h"
#include "volition/rfg/code/gameplay/activities/activity.h"
#include "volition/rfg/code/gameplay/activities/activity_register_damage.h"
#include "volition/rfg/code/gameplay/activities/base_activity_data.h"
#include "volition/rfg/code/gameplay/activities/gfm_reinforcement_coordinator.h"
#include "volition/rfg/code/gameplay/activities/vrail_data.h"
#include "volition/rfg/code/gameplay/gameflow/alert_level_manager.h"
#include "volition/rfg/code/gameplay/gameflow/gfm_control_morale.h"
#include "volition/rfg/code/gameplay/gameflow/gfm_orders_manager.h"
#include "volition/rfg/code/gameplay/mission_info.h"
#include "volition/rfg/code/gameplay/missions.h"
#include "volition/rfg/code/gameplay/object_district.h"
#include "volition/rfg/code/gameplay/squad_spawn.h"
#include "volition/rfg/code/gamewide/stats.h"
#include "volition/rfg/code/hud/hud_ui_activity_complete.h"
#include "volition/rfg/code/hud/minimap.h"
#include "volition/rfg/code/interface/ui_document_hud_tool_tips.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/rfg.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice_lines.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/movers/object_mover.h"
#include "volition/rfg/code/objects/object_dummy.h"
#include "volition/rfg/code/objects/object_navpoint.h"
#include "volition/rfg/code/objects/object_patrol.h"
#include "volition/rfg/code/objects/object_riding_shotgun_node.h"
#include "volition/rfg/code/objects/object_roadblock_node.h"
#include "volition/rfg/code/objects/object_squad.h"
#include "volition/rfg/code/scripting/action_script/global_event.h"
#include "volition/rfg/code/scripting/action_script/global_event_data.h"
#include "volition/rfg/code/scripting/action_script/global_event_manager.h"
#include "volition/rfg/code/scripting/dialogue_manager.h"
#include "volition/rfg/code/scripting/public_address.h"
#include "volition/rfg/code/spawn/spawn_object_group.h"
#include "volition/rfg/code/spawn/spawn_resource_manager.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/vehicle/vehicle_interaction.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/memory/rfg_split_mempool.h"
#include "volition/vlib/code/memory/static_mempool.h"
#include "volition/vlib/code/os/vconsole.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/vlib/code/util/fastxml.h"
#include "volition/vlib/code/util/string_pool.h"
#include "volition/vlib/code/util/string_util.h"

#include <new>

extern float Frametime;
extern mission_handle Mission_active_index; // 0x0165150C (gameplay.cpp)
extern unsigned int Activity_node_start_on_load; // 0x01651244 (activity.cpp)
extern gfm_orders_flags ACTIVITY_ORDERS_FLAGS; // 0x02231B21 (gfm_smash_building_coordinator.cpp)

activity_vehicle_rail_data Vehicle_rail_activity_data; // 0x0226C5E8

static const float MAX_ACTIVITY_SPAWN_DIST_SQRD = 160000.0f; // 0x012DAC88: 400 m
static const float RS_NODE_START_DIST_SQRD = 25.0f; // Magic number: five metres
static const float RS_JORGE_ARRIVED_DIST_SQRD = 225.0f; // Magic number: 15 m from the root of the patrol
static const float RS_VEHICLE_STOPPED_SPEED = 2.0f; // Magic number: the player is asked out below this
static const float RS_VEHICLE_DAMAGE_LINE_HIGH = 0.6f; // 0x012DF210
static const float RS_VEHICLE_DAMAGE_LINE_LOW = 0.3f; // 0x012BF12C
static const int RS_MAX_START_PERIOD_MS = 30000; // Magic number: how long Jorge waits for the player
static const int RS_SPAWN_TIMEOUT_MS = 30000; // Magic number: how long the driver has to load
static const int RS_BORED_LINE_MS = 15000; // Magic number: 0x3A98 left when Jorge says he is bored
static const int RS_VEHICLE_HIT_THRESHOLD = 750; // Magic number: 0x2EE, the hit points below which a hit does not score
static const int MAX_RS_STORY_CHAPTERS = 15; // Magic number: the capacity of chapter_sections, checked against chapters
static const unsigned int RS_STRING_POOL_BYTES = 0x200; // Magic number: 512 bytes of names

static const float Score_multiplier = 1.0f; // 0x01651384
static const int RS_ACTIVITY_COMPLETE_TOOLTIP_DELAY_MS = 2000; // 0x01651388
static const int RS_START_TOOLTIP_DELAY_MS = 3000; // 0x0165138C
static bool Riding_shotgun_do_cleanup = true; // 0x016510DA, nothing clears it
static unsigned int Riding_shotgun_chapter_number = 1; // 0x01651390
static bool Too_many_civilians_killed; // 0x022295C9, nothing reads it
static riding_shotgun_info* Riding_shotgun_infos; // 0x022295D0
static int Num_riding_shotgun_infos; // 0x022295D4
static riding_shotgun_story_chapter_data* Riding_shotgun_story_chapters; // 0x022295D8
static unsigned int Num_riding_shotgun_story_chapters; // 0x022295DC
static char* Riding_shotgun_string_pool_data; // 0x022295E0
static notify_info* Riding_shotgun_damaged_gem_handle; // 0x022295E4
static string_pool Riding_shotgun_string_pool("Shotgun_String_Pool"); // 0x02246BB0

// Entry 0, the activity start line, comes from the table instead. 0x012DB27C
static const char* Radio_line_names[NUM_RIDING_SHOTGUN_RADIO_LINES] = {
    "", "act_rs_01A_jorge_intro", "act_rs_00A_jorge_blow", "act_rs_00A_jorge_bored", "act_rs_00A_jorge_bye",
    "act_rs_00A_jorge_call_res", "act_rs_00A_jorge_dam30", "act_rs_00A_jorge_dam60", "act_rs_00A_jorge_destroy_target",
    "act_rs_00A_jorge_go", "act_rs_00A_jorge_leave", "act_rs_00A_pc_bail", "act_rs_00A_pc_call",
};
// 0x012DB2B0
static const char* Radio_line_persona_names[NUM_RIDING_SHOTGUN_RADIO_LINES] = {
    "OFFCAM", "OFFCAM", "JORGE", "JORGE", "JORGE", "OFFCAM", "JORGE", "JORGE", "JORGE", "JORGE", "JORGE", "PCM01", "PCM01",
};

// 0x007A9780 (:177). A console command nothing registers. rfg.exe inlines the node's position through a lookup that also
// knows convoy end points, which a riding shotgun node never is.
void unused_dcf_riding_shotgun_start()
{
    const player* pp = player_get_local_player();
    object_riding_shotgun_node* best = nullptr;
    float best_dist = MAX_ACTIVITY_SPAWN_DIST_SQRD;
    for (int i = 0; i < World.get_num_objects<object_riding_shotgun_node>(); i++)
    {
        object_riding_shotgun_node* node = World.get_object<object_riding_shotgun_node>(i);
        const float dist = vec_dist_squared(node->pos, pp->pos);
        if (dist < best_dist)
        {
            best = node;
            best_dist = dist;
        }
    }
    if (best == nullptr)
    {
        console_printf("Unable to find any Ridin' Shotgun nodes.");
        return;
    }
    if (!Vehicle_rail_activity_data.activity_base::start_activity_spawn(best, district_get_from_pos(best->pos)))
    {
        console_printf("Unable to start closest Ridin' Shotgun activity.");
    }
}

// 0x00799DA0 (:199). A console command nothing registers.
void unused_dcf_riding_shotgun_play_story_section()
{
    if (Console_command == 0 || !console_get_arg(8, 1)) // Magic number: 8 asks for an integer
    {
        if (Console_help != 0 || Console_command != 0)
        {
            console_output("Usage: riding_shotgun_play_story_section <section_number> .", nullptr, false);
        }
        return;
    }
    const int section_number = Console_arg_int;
    if (Multiplayer || !Vehicle_rail_activity_data.is_active())
    {
        console_output("Riding shotgun must be active for this console command to work.", nullptr, false);
        return;
    }
    if (section_number < 0)
    {
        console_output("Invalid section number.", nullptr, false);
        return;
    }
    if (!Vehicle_rail_activity_data.play_story_section(section_number))
    {
        console_output("Error playing the story section.", nullptr, false);
    }
}

// 0x0075A430 (:235)
static void player_exit_vrail_vehicle()
{
    vehicle_interaction_request_exit(player_get_local_player(), nullptr);
}

// 0x00785C90 (:242)
static const object* find_driver_spawn_object()
{
    for (int i = 0; i < World.get_num_objects<object_dummy>(); i++)
    {
        const object_dummy* dummy = World.get_object<object_dummy>(i);
        if (dummy->get_dummy_type() == DUMMY_OBJECT_TYPE_VRAIL_SPAWN)
        {
            return dummy;
        }
    }
    return nullptr;
}

// 0x007A9910 (:256). rfg.exe: LTCG, squad in ESI.
static void release_squad(object_squad* squad)
{
    const unsigned int orders = squad->get_orders_handle();
    if (orders != 0xFFFFFFFF)
    {
        Orders_manager.destroy_orders(orders); // rfg.exe inlines it
    }
    squad->request_despawn_all_members(NPC_DESPAWN_TYPE_MEDUIM, nullptr, false);
}

// 0x0075A450 (:262)
const riding_shotgun_info* get_riding_shotgun_info_by_name(const char* name)
{
    for (int i = 0; i < Num_riding_shotgun_infos; i++)
    {
        if (v_stricmp(Riding_shotgun_infos[i].name, name) == 0)
        {
            return &Riding_shotgun_infos[i];
        }
    }
    return nullptr;
}

// 0x00799E60 (:284)
static void load_riding_shotgun_table(bool reload)
{
    xml_element* root = xml_table_parse("activity_riding_shotgun.xtbl");
    const int count = (int)xml_count(root, "Activity_riding_shotgun");
    if (!reload)
    {
        Num_riding_shotgun_infos = count;
        Riding_shotgun_infos = static_cast<riding_shotgun_info*>(Permanent_mempool.alloc(count * sizeof(riding_shotgun_info), 0));
        if (Riding_shotgun_string_pool_data == nullptr)
        {
            Riding_shotgun_string_pool_data = static_cast<char*>(Permanent_mempool.alloc(RS_STRING_POOL_BYTES, 0));
            if (Riding_shotgun_string_pool_data == nullptr)
            {
                keen::debug::nativePrintArgs("Insufficient memory to allocate the riding shotgun string table");
            }
        }
    }
    else if (Num_riding_shotgun_infos < count)
    {
        return; // Original: leaves the table open
    }
    Riding_shotgun_string_pool.init(Riding_shotgun_string_pool_data, RS_STRING_POOL_BYTES, "Shotgun_String_Pool", 0, 0);
    Riding_shotgun_string_pool.clear();

    riding_shotgun_info* info = Riding_shotgun_infos;
    for (xml_element* entry = xml_table_find_opt(root, "Activity_riding_shotgun"); entry != nullptr; entry = xml_table_find_next(root, entry, "Activity_riding_shotgun"))
    {
        info->name = Riding_shotgun_string_pool.add_unique(xml_table_get_req_string_ref(entry, "Name"));
        const char* caption = xml_table_get_opt_string_ref(entry, "caption");
        info->caption = caption != nullptr ? Riding_shotgun_string_pool.add_unique(caption) : nullptr;
        info->secondary_caption = Riding_shotgun_string_pool.add_unique("MISS_H2H_PART_4"); // Magic number: 0x012DB198, POST_OBJECTIVE_CAPTION
        const char* base_data = xml_table_get_opt_string_ref(entry, "Base_Data");
        if (base_data != nullptr)
        {
            info->base_data = activity_data_find_handle(base_data); // Original: base_data stays unset without the tag
        }
        info->vrail_data = vrail_data_get_def(xml_table_get_req_string_ref(entry, "Vrail_data"));
        xml_table_get_req_float(info->target_score, entry, "Target_score");
        info->target_score *= Score_multiplier;
        xml_table_get_req_float(info->par_score, entry, "Par_Score");
        info->par_score *= Score_multiplier;
        xml_table_get_req_float(info->max_civilian_penalty, entry, "Max_civ_penalty");
        info->max_civilian_penalty /= 100.0f; // a percentage in the table
        xml_table_get_req_int(info->max_civilians, entry, "Max_civilians");
        if (!xml_table_get_opt_int(info->vehicle_hit_points, entry, "Vehicle_hit_points"))
        {
            info->vehicle_hit_points = 0;
        }
        int alert_level = -1;
        info->alert_level_start = xml_table_get_opt_int(alert_level, entry, "Alert_level_start") ? (gfm_alert_levels)alert_level : (gfm_alert_levels)-1;
        for (int i = 0; i < NUM_RIDING_SHOTGUN_RADIO_LINES; i++)
        {
            info->radio_lines[i] = INVALID_VOICE_LINE_HANDLE;
        }
        const char* start_line = xml_table_get_opt_string_ref(entry, "Activity_Start_Radio_Message");
        if (start_line != nullptr)
        {
            info->radio_lines[RIDING_SHOTGUN_RADIO_LINE_ACTIVITY_START] = voice_line_handle_from_name(start_line);
        }
        for (int i = RIDING_SHOTGUN_RADIO_LINE_ONE_TIME_INTRO; i < NUM_RIDING_SHOTGUN_RADIO_LINES; i++)
        {
            info->radio_lines[i] = voice_line_handle_from_name(Radio_line_names[i]);
        }
        load_standard_rewards(info->standard_rewards, entry);
        if (!xml_table_get_opt_float(info->extra_reward, entry, "Extra_reward"))
        {
            info->extra_reward = 0.0f;
        }
        info++;
    }
    xml_table_close(root);
}

// 0x007B8E00 (:442)
static void load_riding_shotgun_story_table(bool reload)
{
    xml_element* root = xml_table_parse("riding_shotgun_story.xtbl");
    const unsigned int count = xml_count(root, "RS_Story_Chapters");
    if (reload)
    {
        if (Num_riding_shotgun_story_chapters < count)
        {
            return; // Original: leaves the table open
        }
    }
    else
    {
        Num_riding_shotgun_story_chapters = count;
        Riding_shotgun_story_chapters = static_cast<riding_shotgun_story_chapter_data*>(Permanent_mempool.alloc(count * sizeof(riding_shotgun_story_chapter_data), 0));
        // Deviation: rfg.exe's operator= sets up each raw array itself; farray::operator= here wants a constructed one.
        for (unsigned int i = 0; i < count; i++)
        {
            new (&Riding_shotgun_story_chapters[i]) riding_shotgun_story_chapter_data();
        }
    }

    riding_shotgun_story_chapter_data chapter;
    riding_shotgun_story_section_data section;
    int chapter_index = 0;
    for (xml_element* chapter_entry = xml_table_find_opt(root, "RS_Story_Chapters"); chapter_entry != nullptr; chapter_entry = xml_table_find_next(root, chapter_entry, "RS_Story_Chapters"))
    {
        chapter.chapter_sections.clear();
        if (chapter_index >= MAX_RS_STORY_CHAPTERS)
        {
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
            break;
        }
        xml_table_get_req_uint(chapter.chapter_number, chapter_entry, "Chapter_Num");
        xml_element* sections = xml_table_find_req(chapter_entry, "Story_Sec");
        unsigned int num_sections = 0;
        for (xml_element* section_entry = xml_table_find_opt(sections, "Section"); section_entry != nullptr; section_entry = xml_table_find_next(sections, section_entry, "Section"))
        {
            section.dialogue_lines.clear();
            unsigned int num_lines = 0;
            xml_element* lines = xml_table_find_req(section_entry, "Dialogue_Lines");
            for (xml_element* line_entry = xml_table_find_opt(lines, "Dialogue_Line"); line_entry != nullptr; line_entry = xml_table_find_next(lines, line_entry, "Dialogue_Line"))
            {
                riding_shotgun_dialogue_line_data line;
                line.handle = voice_line_handle_from_name(xml_table_get_req_string_ref(line_entry, "Voice_Line"));
                xml_table_get_req_bool(line.player_line, line_entry, "Player_Line");
                xml_table_get_req_int(line.delay_ms, line_entry, "Delay");
                if (line.handle != INVALID_VOICE_LINE_HANDLE)
                {
                    section.dialogue_lines.add(line);
                    num_lines++;
                }
            }
            if (num_lines == 0)
            {
                keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
                continue;
            }
            section.num_lines = num_lines;
            chapter.chapter_sections.add(section);
            num_sections++;
        }
        chapter.num_sections = num_sections;
        Riding_shotgun_story_chapters[chapter_index] = chapter;
        chapter_index++;
    }
    xml_table_close(root);
}

// 0x00770E00 (:546): a damaged zone building is remembered so the run's end can restore it. Mines are left alone.
static event_result riding_shotgun_damaged_callback(const event_data& in_event, notify_info* info)
{
    if (in_event.get_type() != EVENT_BUILDING_DAMAGED)
    {
        return EVENT_IGNORED;
    }
    const unsigned int building = static_cast<const building_damaged_event&>(in_event).building;
    // Magic number: byte 2 of a handle is the zone, 0xFF for an object in no zone.
    if (building == 0xFFFFFFFF || ((building >> 16) & 0xFF) == 0xFF)
    {
        return EVENT_IGNORED;
    }
    object* objp = World.object_from_handle(building);
    if (objp == nullptr || objp->get_obj_type() != OT_MOVER)
    {
        return EVENT_IGNORED;
    }
    if (objp->get_sub_type() != OT_SUB_MOVER_RFG || !static_cast<rfg_mover*>(objp)->flags.mining)
    {
        static_cast<object_mover*>(objp)->mover_flags |= 0x10000000; // Magic number: damaged during this run
    }
    return EVENT_OK;
}

// 0x00785D10 (:582)
static void activity_riding_shotgun_damage_cleanup()
{
    if (!Riding_shotgun_do_cleanup)
    {
        return;
    }
    for (int i = 0; i < World.get_num_objects<object_mover>(); i++)
    {
        object_mover* mover = World.get_object<object_mover>(i);
        const unsigned int flags = mover->mover_flags;
        if ((flags & 0x10000000) != 0) // Magic number: see riding_shotgun_damaged_callback
        {
            mover->mover_flags = flags & ~0x10000000u;
            // Magic number: flag 4 is left alone by world restores (rfg_dynamics.cpp tests it too)
            if (((mover->get_handle() >> 16) & 0xFF) != 0xFF && (flags & 4) == 0)
            {
                World.restore_object(mover->get_handle());
            }
        }
    }
    gem_unregister_for_event(Riding_shotgun_damaged_gem_handle);
    Riding_shotgun_damaged_gem_handle = nullptr;
}

// 0x0075A4A0 (:612)
riding_shotgun_story_chapter_data* riding_shotgun_get_current_chapter()
{
    for (unsigned int i = 0; i < Num_riding_shotgun_story_chapters; i++)
    {
        if (Riding_shotgun_story_chapters[i].chapter_number == Riding_shotgun_chapter_number)
        {
            return &Riding_shotgun_story_chapters[i];
        }
    }
    return nullptr;
}

// ---- the story types

// 0x00785CE0
riding_shotgun_story_section_data::riding_shotgun_story_section_data()
{
}

// 0x00785D00
riding_shotgun_story_section_data::~riding_shotgun_story_section_data()
{
}

// 0x007AC7F0
riding_shotgun_story_section_data& riding_shotgun_story_section_data::operator=(const riding_shotgun_story_section_data& __that)
{
    num_lines = __that.num_lines;
    dialogue_lines = __that.dialogue_lines;
    return *this;
}

// 0x0079A160
riding_shotgun_story_chapter_data::riding_shotgun_story_chapter_data()
{
}

// 0x0079A170
riding_shotgun_story_chapter_data::~riding_shotgun_story_chapter_data()
{
}

// 0x007B8B20
riding_shotgun_story_chapter_data& riding_shotgun_story_chapter_data::operator=(const riding_shotgun_story_chapter_data& __that)
{
    chapter_number = __that.chapter_number;
    num_sections = __that.num_sections;
    chapter_sections = __that.chapter_sections;
    return *this;
}

// 0x00768EF0 (.h:128)
riding_shotgun_data::riding_shotgun_data()
{
    clear();
}

// 0x007541C0 (.h:134)
void riding_shotgun_data::clear()
{
    soldiers = 0;
    soldier_damage = 0.0f;
    building_damage = 0.0f;
    vehicle_damage = 0.0f;
    civilians = 0;
    civilian_damage = 0.0f;
    nuetral_damage = 0.0f;
    penalty = 0.0f;
}

// ---- activity_vehicle_rail_data

// 0x007A00C0 (.h:263). Original: the rest stays unset until level_init calls reset.
activity_vehicle_rail_data::activity_vehicle_rail_data()
    : activity_base_modal(ACTIVITY_MODE_VEHICLE_RAIL, false)
{
}

// 0x0077E3E0
activity_vehicle_rail_data::~activity_vehicle_rail_data()
{
}

// 0x007B90A0 (:629)
void activity_vehicle_rail_data::system_init()
{
    load_riding_shotgun_table(false);
    load_riding_shotgun_story_table(false);
}

// 0x007B3170 (:638)
void activity_vehicle_rail_data::level_init()
{
    activity_base_modal::level_init(); // rfg.exe inlines it
    reset();
    Riding_shotgun_chapter_number = 1;
}

// 0x0075A4E0 (:649)
const char* activity_vehicle_rail_data::get_title() const
{
    return "ACT_SHOTGUN_NAME";
}

// 0x00770E60 (:663)
const char* activity_vehicle_rail_data::get_subtitle() const
{
    return target_score_reached() ? current_info->secondary_caption : current_info->caption;
}

// 0x00785DC0 (:676)
bool activity_vehicle_rail_data::activity_begin_start_up()
{
    return activity_base_modal::activity_begin_start_up();
}

// 0x00785DD0 (:690). Original: does not ask can_spawn, so a finished node can be collected.
bool activity_vehicle_rail_data::collect_spawn_objects(farray<object const*,32>& spawn_objects, const vector& player_pos, unsigned int district_handlep) const
{
    for (int i = 0; i < World.get_num_objects<object_riding_shotgun_node>(); i++)
    {
        const object_riding_shotgun_node* node = World.get_object<object_riding_shotgun_node>(i);
        if (vec_dist_squared(player_pos, node->pos) <= MAX_ACTIVITY_SPAWN_DIST_SQRD)
        {
            spawn_objects.add(node); // base_array::add reports a full list
        }
    }
    return spawn_objects.size() > 0;
}

// 0x00770E90 (:706)
bool activity_vehicle_rail_data::set_up_vrail_patrol()
{
    const object_riding_shotgun_node* node = riding_shotgun_node_handle != 0xFFFFFFFF ? World.object_from_handle<object_riding_shotgun_node>(riding_shotgun_node_handle) : nullptr;
    if (node == nullptr)
    {
        return false;
    }
    const object_patrol* patrol = node->find_patrol();
    if (patrol == nullptr)
    {
        return false;
    }
    vrail_patrol_handle = patrol->get_handle();
    return patrol->get_last_patrol_node_pos(drop_off_pos);
}

// 0x0079A1A0 (:723)
bool activity_vehicle_rail_data::activity_initialize(const object* spawn_objp, unsigned int new_district_handle)
{
    if (was_initialized() || is_active() || !activity_base_modal::activity_initialize(spawn_objp, new_district_handle))
    {
        return false;
    }
    stop_all_flares();
    if (spawn_objp == nullptr || spawn_objp->get_obj_type() != OT_RIDING_SHOTGUN_NODE)
    {
        return false;
    }
    riding_shotgun_node_handle = spawn_objp->get_handle();
    const object_riding_shotgun_node* node = static_cast<const object_riding_shotgun_node*>(spawn_objp);
    if (!node->can_spawn())
    {
        return false;
    }
    current_info = node->get_riding_shotgun_info();
    if (current_info == nullptr)
    {
        return false;
    }
    current_vrail_def = current_info->vrail_data;
    if (current_vrail_def == nullptr)
    {
        return false;
    }
    current_story_chapter = riding_shotgun_get_current_chapter();
    Too_many_civilians_killed = false;
    return true;
}

// 0x007AEB70 (:762)
bool activity_vehicle_rail_data::activity_start()
{
    if (!activity_base_modal::activity_start() || !set_up_vrail_patrol())
    {
        return false;
    }
    for (int i = 0; i < World.get_num_objects<object_mover>(); i++)
    {
        World.get_object<object_mover>(i)->set_ignore_importance(true);
    }
    active_state = RS_ACTIVE_PRE_SPAWN;
    play_radio_line(RIDING_SHOTGUN_RADIO_LINE_MASON_ARRIVAL, DIALOGUE_PRIORITY_UNDEFINED);
    return true;
}

// 0x007AEC50 (:808)
void activity_vehicle_rail_data::activity_end_common()
{
    vehicle_set_invulnerable(patrol_data.get_patrol_vehicle_handle(), true);
    stop_all_flares();
    activity_reinstate_diversions(ADR_ACTIVITY);
    roadblock_destroy_all();
    roadblock_destroy_all_created_objects();
    gfm_end_temporary_morale(); // rfg.exe inlines it
    patrol_data.release_and_reset();
    spawn_data.release_and_reset();
    const object_squad* squad = squad_handle != 0xFFFFFFFF ? World.object_from_handle<object_squad>(squad_handle) : nullptr;
    if (squad != nullptr)
    {
        for (int i = 0; i < squad->member_array.size(); i++)
        {
            const unsigned int member = squad->member_array[i];
            npc* np = member != 0xFFFFFFFF ? World.object_from_handle<npc>(member) : nullptr;
            if (np == nullptr || np->vehicle_handle == 0xFFFFFFFF)
            {
                continue;
            }
            vehicle* vp = World.object_from_handle<vehicle>(np->vehicle_handle);
            if (vp != nullptr && vehicle_passenger_in_seat(vp, VSI_DRIVER) == np)
            {
                np->hflags.damaged_by_player = 0;
                np->npc_flags.passenger_wait = 0;
                np->npc_flags.player_exit_wait = 1;
                np->start_active_despawn(NPC_DESPAWN_TYPE_MEDUIM, nullptr);
                vp->m_flags.disable_for_player = 1;
                np->get_ai()->planner.request_replan();
            }
        }
    }
    for (int i = 0; i < World.get_num_objects<object_mover>(); i++)
    {
        World.get_object<object_mover>(i)->set_ignore_importance(false);
    }
    activity_riding_shotgun_damage_cleanup();
    activity_cooldown_period.set(0);
}

// 0x007AEE80 (:882)
void activity_vehicle_rail_data::activity_end()
{
    if (vrail_successfully_completed)
    {
        play_radio_line(RIDING_SHOTGUN_RADIO_LINE_JORGE_SAYS_BYE, DIALOGUE_PRIORITY_HIGH);
        vehicle_interaction_play_custom_anim(patrol_data.get_vrail_driver(), ANIMATION_ACTION_TAUNT, true, true);
    }
    else if (vrail_started_and_failed)
    {
        play_radio_line(RIDING_SHOTGUN_RADIO_LINE_MASON_SAYS_BYE, DIALOGUE_PRIORITY_HIGH);
    }
    activity_base_modal::activity_end();
    activity_end_common();
}

// 0x007AEEE0 (:899)
void activity_vehicle_rail_data::activity_kill()
{
    activity_base_modal::activity_kill();
    activity_end_common();
}

// 0x007B31D0 (:907)
void activity_vehicle_rail_data::activity_complete()
{
    activity_base_modal::activity_complete(); // rfg.exe inlines it
    reset();
}

// 0x007A26F0 (:916)
bool activity_vehicle_rail_data::start_activity_spawn(const object* spawn_objp, const district* dp)
{
    return activity_base::start_activity_spawn(spawn_objp, dp);
}

// 0x00770EE0 (:926)
void activity_vehicle_rail_data::activity_do_frame_ready()
{
    activity_base::activity_do_frame_ready();
    const player* pp = player_get_local_player();
    if (pp == nullptr || pp->vehicle_handle != 0xFFFFFFFF || riding_shotgun_node_handle == 0xFFFFFFFF)
    {
        return;
    }
    const object_riding_shotgun_node* node = World.object_from_handle<object_riding_shotgun_node>(riding_shotgun_node_handle);
    if (node != nullptr && vec_dist_squared(pp->pos, node->pos) <= RS_NODE_START_DIST_SQRD && !activity_begin_start_up())
    {
        activity_kill();
    }
}

// 0x00770F60 (:960). The driver squad must be gone too; rfg.exe inlines the base.
bool activity_vehicle_rail_data::activity_clean_up_completed() const
{
    if (!activity_base_modal::activity_clean_up_completed())
    {
        return false;
    }
    object_squad* squad = squad_handle != 0xFFFFFFFF ? World.object_from_handle<object_squad>(squad_handle) : nullptr;
    if (squad != nullptr)
    {
        squad->request_despawn_all_members(NPC_DESPAWN_TYPE_MEDUIM, nullptr, false);
        return false;
    }
    return true;
}

// 0x0075A4F0 (:983)
void activity_vehicle_rail_data::activity_do_frame_clean_up()
{
    activity_base::activity_do_frame_clean_up(); // rfg.exe inlines it
}

// 0x007B8BD0 (:993)
void activity_vehicle_rail_data::activity_do_frame_active()
{
    switch (active_state)
    {
    case RS_ACTIVE_PRE_SPAWN:
        do_frame_active_pre_spawn();
        break;
    case RS_ACTIVE_STATE_DO_SPAWN:
        do_frame_active_do_spawn();
        break;
    case RS_ACTIVE_STATE_WAIT_JORGE:
        do_frame_active_wait_jorge();
        break;
    case RS_ACTIVE_STATE_WAIT_PLAYER:
        do_frame_active_wait_player();
        break;
    case RS_ACTIVE_STATE_RUNNING:
        do_frame_active_running();
        break;
    case RS_ACTIVE_STATE_WAIT_SPLASH_SCREEN:
        break;
    default:
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        activity_kill();
        break;
    }
}

// 0x00785E80 (:1027)
void activity_vehicle_rail_data::stop_all_flares() const
{
    for (int i = 0; i < World.get_num_objects<object_riding_shotgun_node>(); i++)
    {
        object_riding_shotgun_node* node = World.get_object<object_riding_shotgun_node>(i);
        if (node->flare_started())
        {
            node->stop_flare();
        }
    }
}

// 0x007A9960 (:1041): the flare and map icon of each node that can start.
void activity_vehicle_rail_data::activity_do_frame_inactive()
{
    if (Multiplayer)
    {
        return;
    }
    if (!activity_riding_shotgun_can_be_started())
    {
        stop_all_flares();
        return;
    }
    for (int i = 0; i < World.get_num_objects<object_riding_shotgun_node>(); i++)
    {
        object_riding_shotgun_node* node = World.get_object<object_riding_shotgun_node>(i);
        if (!node->can_spawn())
        {
            continue;
        }
        if (!node->flare_started())
        {
            node->start_flare();
        }
        Cartographer.add_temporary_icon(node->pos, MAP_ELEMENT_RIDING_SHOTGUN, false, 0xFFFFFFFF, node->is_completed() ? Completed_icon_color : Default_map_item_color, false, node->get_handle());
    }
}

// 0x0075A510 (:1118)
bool activity_vehicle_rail_data::on_alert(const npc* np) const
{
    return is_active();
}

// Inlined at the head of the register_ functions.
static bool score_counts(bool active, riding_shotgun_active_states active_state, vrail_patrol_state patrol_state)
{
    return active && active_state == RS_ACTIVE_STATE_RUNNING && patrol_state != VRAIL_PATROL_STATE_COMPLETE && patrol_state != VRAIL_PATROL_STATE_COMPLETE_WAIT;
}

// 0x0079A240 (:1127), read from the asm: Ghidra fails on this function. Jorge reacts to damage to his own vehicle,
// and damage to any other vehicle scores by its fraction of the hit points.
void activity_vehicle_rail_data::register_damage(const object* obj_a, const object* obj_b, float damage, int original_hitpoints)
{
    if (!score_counts(is_active(), active_state, patrol_data.last_patrol_state))
    {
        return;
    }
    if (!activity_register_damage_is_player_controlled(obj_a) && !activity_register_damage_is_player_controlled(obj_b))
    {
        return;
    }
    const vehicle* vp;
    if (obj_a != nullptr && obj_a->get_obj_type() == OT_VEHICLE)
    {
        vp = static_cast<const vehicle*>(obj_a);
    }
    else if (obj_b != nullptr && obj_b->get_obj_type() == OT_VEHICLE)
    {
        vp = static_cast<const vehicle*>(obj_b);
    }
    else
    {
        return;
    }
    if (vehicle_player_in_vehicle(vp))
    {
        const float vehicle_damage = vp->get_vehicle_damage();
        if (vp->is_on_fire())
        {
            if (!played_fire_message)
            {
                play_radio_line(RIDING_SHOTGUN_RADIO_LINE_VEHICLE_PRE_EXPLODE, DIALOGUE_PRIORITY_HIGH);
                played_fire_message = true;
                played_veh_dam60_message = true;
                played_veh_dam30_message = true;
            }
        }
        else if (vehicle_damage >= RS_VEHICLE_DAMAGE_LINE_HIGH)
        {
            if (!played_veh_dam60_message)
            {
                play_radio_line(RIDING_SHOTGUN_RADIO_LINE_VEH_HEALTH_60, DIALOGUE_PRIORITY_HIGH);
                played_veh_dam60_message = true;
                played_veh_dam30_message = true;
            }
        }
        else if (vehicle_damage >= RS_VEHICLE_DAMAGE_LINE_LOW && !played_veh_dam30_message)
        {
            play_radio_line(RIDING_SHOTGUN_RADIO_LINE_VEH_HEALTH_30, DIALOGUE_PRIORITY_HIGH);
            played_veh_dam30_message = true;
        }
        return;
    }
    if (original_hitpoints < RS_VEHICLE_HIT_THRESHOLD)
    {
        return;
    }
    // Original: a vehicle left at or under the threshold scores all it had before the hit, not what the hit took.
    const unsigned int hit_points = vehicle_get_hit_points(vp);
    const int hit = (int)hit_points > RS_VEHICLE_HIT_THRESHOLD ? original_hitpoints - (int)hit_points : original_hitpoints;
    if ((float)hit <= 0.0f)
    {
        return;
    }
    const double fraction = (double)(float)hit / (double)vehicle_get_max_hit_points(vp);
    const float value = (float)(activity_register_damage_get_cost(vp) * fraction);
    const double score = (double)Score_multiplier * (double)value;
    if (human_teams_are_unfriendly(human_teams_get_team(vp), HUMAN_TEAM_GUERILLA))
    {
        current_score.vehicle_damage = (float)(score + (double)current_score.vehicle_damage);
    }
    else
    {
        current_score.civilian_damage = (float)(score + (double)current_score.civilian_damage);
    }
}

// 0x00785EF0 (:1218)
void activity_vehicle_rail_data::register_death(const object* target_obj, const object* attacker_obj)
{
    if (!score_counts(is_active(), active_state, patrol_data.last_patrol_state) || !activity_register_damage_is_player_controlled(attacker_obj))
    {
        return;
    }
    // Deviation: rfg.exe reads through null for a null target.
    if (target_obj == nullptr)
    {
        return;
    }
    const float cost = activity_register_damage_get_cost(target_obj);
    if (target_obj->get_obj_type() == OT_HUMAN)
    {
        const human_teams team = static_cast<const human*>(target_obj)->current_team;
        if (team == HUMAN_TEAM_EDF)
        {
            current_score.soldiers++;
            current_score.soldier_damage += cost;
        }
        else if (team == HUMAN_TEAM_GUERILLA || team == HUMAN_TEAM_CIVILIAN)
        {
            current_score.civilians++;
            float penalty = (float)((double)current_info->max_civilian_penalty / (double)current_info->max_civilians + (double)current_score.penalty);
            if (penalty < 0.0f)
            {
                penalty = 0.0f;
            }
            if (penalty >= current_info->max_civilian_penalty)
            {
                penalty = current_info->max_civilian_penalty;
            }
            current_score.penalty = penalty;
            if (current_score.civilians > current_info->max_civilians)
            {
                Too_many_civilians_killed = true;
            }
        }
        return;
    }
    if (target_obj->get_sub_type() != 0) // Magic number: sub type 0 of a non-human, as tested at 0x00786047
    {
        return;
    }
    const human_teams team = human_teams_get_team(target_obj);
    if (human_teams_are_friendly(team, HUMAN_TEAM_GUERILLA))
    {
        current_score.civilian_damage += cost;
    }
    else if (human_teams_are_unfriendly(team, HUMAN_TEAM_GUERILLA))
    {
        current_score.building_damage += cost; // Original: not vehicle_damage, whatever the target
    }
    else
    {
        current_score.nuetral_damage += cost;
    }
}

// 0x00786100 (:1279)
void activity_vehicle_rail_data::register_building_damage(const rfg_mover* objp, float damage_percent_delta, float original_damage_percent)
{
    if (!score_counts(is_active(), active_state, patrol_data.last_patrol_state) || original_damage_percent < 0.0f || !activity_register_damage_is_player_controlled(objp) || original_damage_percent >= objp->game_destroyed_pct)
    {
        return;
    }
    const float fraction = damage_percent_delta + original_damage_percent < objp->game_destroyed_pct ? damage_percent_delta : 1.0f - original_damage_percent;
    const float score = Score_multiplier * activity_register_damage_get_cost(objp) * fraction;
    const human_teams team = human_teams_get_team(objp);
    if (human_teams_are_friendly(team, HUMAN_TEAM_GUERILLA))
    {
        current_score.civilian_damage += score;
    }
    else if (human_teams_are_unfriendly(team, HUMAN_TEAM_GUERILLA))
    {
        current_score.building_damage += score;
    }
    else
    {
        current_score.nuetral_damage += score;
    }
}

// 0x0079A490 (:1321)
void activity_vehicle_rail_data::register_building_destroyed(const rfg_mover* objp)
{
    if (objp != nullptr && score_counts(is_active(), active_state, patrol_data.last_patrol_state) && activity_register_damage_is_player_controlled(objp) && human_teams_get_team(objp) == HUMAN_TEAM_EDF && dialogue_manager_get_num_dialogues(DIALOGUE_PRIORITY_UNDEFINED) == 0)
    {
        play_radio_line(RIDING_SHOTGUN_RADIO_LINE_TARGET_DESTROYED, DIALOGUE_PRIORITY_UNDEFINED);
    }
}

// 0x0075A520 (:1337)
void activity_vehicle_rail_data::save_activity_state(cfile* save_file)
{
    activity_base::save_activity_state(save_file); // rfg.exe inlines it
    cf_write_uint32(Riding_shotgun_chapter_number, save_file);
}

// 0x0075A560 (:1345)
void activity_vehicle_rail_data::load_activity_state(cfile* save_file)
{
    activity_base::load_activity_state(save_file); // rfg.exe inlines it
    Riding_shotgun_chapter_number = cf_read_uint32(save_file);
}

// 0x00786290 (:1360)
void activity_vehicle_rail_data::play_radio_line(riding_shotgun_radio_lines line, dialogue_priorities override_priority)
{
    if (line >= NUM_RIDING_SHOTGUN_RADIO_LINES)
    {
        return;
    }
    const voice_line_handle line_handle = current_info->radio_lines[line];
    if (line_handle == INVALID_VOICE_LINE_HANDLE)
    {
        return;
    }
    const human_voice_persona* persona = human_voice_persona_from_name(Radio_line_persona_names[line]);
    if (persona == nullptr)
    {
        return;
    }
    if (v_stricmp("OFFCAM", Radio_line_persona_names[line]) != 0 && v_stricmp("PCM01", Radio_line_persona_names[line]) != 0)
    {
        // Jorge's lines come from the driver himself, whatever the priority asked for.
        dialogue_manager_start_dialogue(line_handle, patrol_data.get_vrail_driver(), DIALOGUE_PRIORITY_HIGH);
        most_recent_line_handle = line_handle;
        return;
    }
    const dialogue_priorities priority = override_priority != DIALOGUE_PRIORITY_UNDEFINED ? override_priority : DIALOGUE_PRIORITY_RADIO;
    most_recent_line_handle = dialogue_manager_start_dialogue(line_handle, persona, priority, INVALID_PA_HANDLE, nullptr) ? line_handle : INVALID_VOICE_LINE_HANDLE;
}

// 0x0075A5A0 (:1402)
void activity_vehicle_rail_data::append_radio_line(riding_shotgun_radio_lines line, int time_delay)
{
    if (line >= NUM_RIDING_SHOTGUN_RADIO_LINES || most_recent_line_handle == INVALID_VOICE_LINE_HANDLE)
    {
        return;
    }
    const voice_line_handle line_handle = current_info->radio_lines[line];
    if (line_handle == INVALID_VOICE_LINE_HANDLE)
    {
        return;
    }
    const human_voice_persona* persona = human_voice_persona_from_name(Radio_line_persona_names[line]);
    if (persona != nullptr)
    {
        most_recent_line_handle = dialogue_manager_add_line(line_handle, persona, most_recent_line_handle, time_delay, INVALID_PA_HANDLE, nullptr) ? line_handle : INVALID_VOICE_LINE_HANDLE;
    }
}

// 0x0079A500 (:1434)
void activity_vehicle_rail_data::render_icons() const
{
    if (is_active())
    {
        render_icon_player();
        render_icon_attackers();
    }
}

// 0x0075A620 (:1447)
bool activity_vehicle_rail_data::get_drop_point(vector& drop_pos) const
{
    if (current_vrail_def == nullptr)
    {
        return false;
    }
    drop_pos = drop_off_pos;
    return true;
}

// 0x0075A650 (:1457)
float activity_vehicle_rail_data::get_target_score()
{
    return is_active() ? current_info->target_score : 0.0f;
}

// 0x0075A670 (:1466)
float activity_vehicle_rail_data::get_display_score()
{
    return is_active() ? display_score : 0.0f;
}

// 0x0075A690 (:1475)
float activity_vehicle_rail_data::get_building_score()
{
    return is_active() ? current_score.building_damage : 0.0f;
}

// 0x0075A6B0 (:1484)
float activity_vehicle_rail_data::get_vehicle_score()
{
    return is_active() ? current_score.vehicle_damage : 0.0f;
}

// 0x0075A6D0 (:1493)
float activity_vehicle_rail_data::get_soldier_score()
{
    return is_active() ? current_score.soldier_damage : 0.0f;
}

// 0x0075A6F0 (:1502)
float activity_vehicle_rail_data::get_base_score() const
{
    if (!is_active())
    {
        return 0.0f;
    }
    return fl_round(current_score.vehicle_damage + current_score.building_damage + current_score.soldier_damage, 0.01f);
}

// 0x0075A750 (:1511)
float activity_vehicle_rail_data::get_current_penalty() const
{
    return is_active() ? fl_round(current_score.penalty * get_base_score(), 0.01f) : 0.0f;
}

// 0x0075A810 (:1520)
float activity_vehicle_rail_data::get_current_score() const
{
    return get_base_score() - get_current_penalty();
}

// 0x0075A8A0 (:1525)
bool activity_vehicle_rail_data::target_score_reached() const
{
    const float score = fl_round(get_base_score() - get_current_penalty(), 0.01f);
    return is_active() && current_info->target_score <= score;
}

// 0x007541F0 (.h:378)
const char* activity_vehicle_rail_data::get_chapter_name() const
{
    return current_info != nullptr ? current_info->name : nullptr;
}

// 0x00786380 (:1591)
bool activity_vehicle_rail_data::play_story_section(unsigned int section_number)
{
    if (!is_active() || patrol_data.last_patrol_state == VRAIL_PATROL_STATE_COMPLETE || patrol_data.last_patrol_state == VRAIL_PATROL_STATE_COMPLETE_WAIT || current_story_chapter == nullptr || section_number >= current_story_chapter->num_sections)
    {
        return false;
    }
    const unsigned int vehicle_handle = get_patrol_vehicle();
    const vehicle* vp = vehicle_handle != 0xFFFFFFFF ? World.object_from_handle<vehicle>(vehicle_handle) : nullptr;
    if (vp == nullptr)
    {
        return false;
    }
    human* driver = vehicle_get_first_passenger(vp, VSI_DRIVER);
    human* pp = player_get_local_player();
    if (driver == nullptr || pp == nullptr)
    {
        return false;
    }
    const riding_shotgun_story_section_data& section = current_story_chapter->chapter_sections[(int)section_number];
    voice_line_handle previous = section.dialogue_lines[0].handle;
    dialogue_manager_start_dialogue(previous, section.dialogue_lines[0].player_line ? pp : driver, DIALOGUE_PRIORITY_HIGH);
    for (unsigned int i = 1; i < section.num_lines; i++)
    {
        const riding_shotgun_dialogue_line_data& line = section.dialogue_lines[(int)i];
        dialogue_manager_add_line(line.handle, line.player_line ? pp : driver, previous, line.delay_ms);
        previous = line.handle;
    }
    return true;
}

// 0x007864D0 (:1645)
void activity_vehicle_rail_data::play_next_story_section()
{
    if (play_story_section(story_section_number))
    {
        story_section_number++;
    }
}

// 0x0075A990 (:1654)
vrail_patrol_state activity_vehicle_rail_data::get_patrol_state() const
{
    return patrol_data.last_patrol_state;
}

// 0x00754200 (.h:388)
bool activity_vehicle_rail_data::is_ready() const
{
    return current_vrail_def != nullptr;
}

// 0x00754210 (.h:391)
bool activity_vehicle_rail_data::is_running() const
{
    return is_active() && active_state == RS_ACTIVE_STATE_RUNNING;
}

// 0x00754240 (.h:394)
unsigned int activity_vehicle_rail_data::get_patrol() const
{
    return vrail_patrol_handle;
}

// 0x00754250 (.h:397)
unsigned int activity_vehicle_rail_data::get_patrol_squad() const
{
    return patrol_data.vrail_patrol_squad;
}

// 0x0077E340 (.h:398)
unsigned int activity_vehicle_rail_data::get_patrol_vehicle() const
{
    return patrol_data.get_patrol_vehicle_handle();
}

// 0x0077E370 (.h:401)
void activity_vehicle_rail_data::set_chase_vehicle_count(int new_count)
{
    spawn_data.set_chase_vehicle_count(new_count);
}

// 0x00754260 (.h:405)
bool activity_vehicle_rail_data::is_riding_shotgun_activity()
{
    return current_info != nullptr;
}

// 0x007A0250 (.h:410)
void activity_vehicle_rail_data::stop_squad_spawning()
{
    spawn_data.release_and_reset();
}

// 0x007AEF00 (:1665)
void activity_vehicle_rail_data::reset()
{
    patrol_data.release_and_reset();
    spawn_data.release_and_reset();
    current_vrail_def = nullptr;
    vrail_patrol_handle = 0xFFFFFFFF;
    riding_shotgun_node_handle = 0xFFFFFFFF;
    drop_off_pos.zero();
    max_start_period.invalidate();
    spawn_timeout.invalidate();
    squad_handle = 0xFFFFFFFF;
    active_state = INVALID_RS_ACTIVE_STATE;
    current_score.clear();
    display_score = 0.0f;
    current_info = nullptr;
    played_timeout_warning = false;
    played_vrail_end = false;
    played_fire_message = false;
    played_veh_dam30_message = false;
    played_veh_dam60_message = false;
    vrail_successfully_completed = false;
    vrail_started_and_failed = false;
    vrail_has_ended = false;
    most_recent_line_handle = INVALID_VOICE_LINE_HANDLE;
    current_story_chapter = riding_shotgun_get_current_chapter(); // rfg.exe inlines it
    story_section_number = 0;
    activity_cooldown_period.set(0);
}

// 0x007864F0 (:1710)
void activity_vehicle_rail_data::do_frame_active_pre_spawn()
{
    if (dialogue_manager_get_num_dialogues(DIALOGUE_PRIORITY_UNDEFINED) == 0)
    {
        play_radio_line(RIDING_SHOTGUN_RADIO_LINE_MASON_ARRIVAL_JORGE_RES, DIALOGUE_PRIORITY_UNDEFINED);
        active_state = RS_ACTIVE_STATE_DO_SPAWN;
    }
}

// 0x007B8390 (:1725)
void activity_vehicle_rail_data::do_frame_active_do_spawn()
{
    if (!srm_vehicle_info_loaded(current_vrail_def->player_veh_spawn_group->v_info_list[0]))
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        activity_kill();
        return;
    }
    if (!spawn_timeout.valid())
    {
        spawn_timeout.set(RS_SPAWN_TIMEOUT_MS);
    }
    if (object_setup())
    {
        active_state = RS_ACTIVE_STATE_WAIT_JORGE;
    }
    else if (spawn_timeout.elapsed())
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        activity_kill();
    }
}

// 0x00786520 (:1755)
void activity_vehicle_rail_data::do_frame_active_wait_jorge()
{
    const unsigned int vehicle_handle = patrol_data.get_patrol_vehicle_handle();
    const vehicle* vp = vehicle_handle != 0xFFFFFFFF ? World.object_from_handle<vehicle>(vehicle_handle) : nullptr;
    const object_patrol* patrol = vrail_patrol_handle != 0xFFFFFFFF ? World.object_from_handle<object_patrol>(vrail_patrol_handle) : nullptr;
    // Deviation: rfg.exe reads through a null patrol here.
    const navpoint* root = patrol != nullptr ? patrol->get_root_nav() : nullptr;
    if (vp == nullptr || root == nullptr)
    {
        activity_kill();
        return;
    }
    if (vec_xz_dist_squared(vp->pos, root->pos) < RS_JORGE_ARRIVED_DIST_SQRD)
    {
        active_state = RS_ACTIVE_STATE_WAIT_PLAYER;
        play_radio_line(RIDING_SHOTGUN_RADIO_LINE_JORGE_ARRIVAL, DIALOGUE_PRIORITY_UNDEFINED);
        vehicle_interaction_play_custom_anim(patrol_data.get_vrail_driver(), ANIMATION_ACTION_TALK_YELL, true, true);
        max_start_period.set(RS_MAX_START_PERIOD_MS);
    }
}

// 0x00786610 (:1787)
void activity_vehicle_rail_data::do_frame_active_wait_player()
{
    npc* driver = patrol_data.get_vrail_driver();
    if (driver == nullptr)
    {
        activity_end();
        return;
    }
    if (max_start_period.elapsed() && vec_dist_squared(player_get_local_player()->pos, driver->pos) > RS_NODE_START_DIST_SQRD)
    {
        driver->stop_voice(VOICE_LINE_PRIORITY_CRITICAL);
        play_radio_line(RIDING_SHOTGUN_RADIO_LINE_JORGE_LEAVES_BEFORE_START, DIALOGUE_PRIORITY_HIGH);
        activity_end(); // Original: no return, so the rest runs on the released rail
    }
    if (!played_timeout_warning && max_start_period.time_until() <= RS_BORED_LINE_MS)
    {
        play_radio_line(RIDING_SHOTGUN_RADIO_LINE_JORGE_BORED, DIALOGUE_PRIORITY_UNDEFINED);
        played_timeout_warning = true;
    }
    const object_squad* squad = squad_handle != 0xFFFFFFFF ? World.object_from_handle<object_squad>(squad_handle) : nullptr;
    if (squad != nullptr)
    {
        const object_riding_shotgun_node* node = riding_shotgun_node_handle != 0xFFFFFFFF ? World.object_from_handle<object_riding_shotgun_node>(riding_shotgun_node_handle) : nullptr;
        const bool completed = node != nullptr && node->is_completed();
        Cartographer.add_temporary_icon(squad->pos, MAP_ELEMENT_RIDING_SHOTGUN, true, riding_shotgun_node_handle, completed ? Completed_icon_color : Default_map_item_color, false, get_spawn_object_handle());
    }
    if (human_in_vehicle(player_get_local_player(), patrol_data.get_patrol_vehicle_handle()))
    {
        vehicle_set_invulnerable(patrol_data.get_patrol_vehicle_handle(), false);
        activity_suspend_diversions(ADR_ACTIVITY);
        gfm_start_temporary_morale(); // rfg.exe inlines these two
        gfm_set_temporary_morale(0.0f);
        active_state = RS_ACTIVE_STATE_RUNNING;
        ui_tool_tips_add_tip("Riding_Shotgun", RS_START_TOOLTIP_DELAY_MS, nullptr);
        notify_info info;
        info.type = EVENT_BUILDING_DAMAGED;
        info.callback = riding_shotgun_damaged_callback;
        info.data = nullptr;
        Riding_shotgun_damaged_gem_handle = gem_register_for_event(info);
        return;
    }
    if (driver->hflags.damaged_by_player && !patrol_data.player_in_vrail_vehicle())
    {
        driver->stop_voice(VOICE_LINE_PRIORITY_CRITICAL);
        play_radio_line(RIDING_SHOTGUN_RADIO_LINE_JORGE_LEAVES_BEFORE_START, DIALOGUE_PRIORITY_HIGH);
        record_loss();
        activity_end();
    }
}

// 0x007B8440 (:1892)
void activity_vehicle_rail_data::do_frame_active_running()
{
    update_display_score();
    play_activity_music(); // rfg.exe inlines it
    const unsigned int vehicle_handle = patrol_data.get_patrol_vehicle_handle();
    const vehicle* vp = vehicle_handle != 0xFFFFFFFF ? World.object_from_handle<vehicle>(vehicle_handle) : nullptr;
    if (vp != nullptr && vehicle_get_hit_points(vp) != 0 && !patrol_data.player_in_vrail_vehicle())
    {
        gamelog_set_activity_failed_reason(GLCR_OUT_OF_RANGE);
        determine_reward_and_end();
        return;
    }
    const vrail_patrol_state patrol_state = patrol_data.do_frame();
    if (patrol_state > VRAIL_PATROL_STATE_FAILED)
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        activity_end();
        return;
    }
    spawn_data.do_frame();
    switch (patrol_state)
    {
    case VRAIL_PATROL_STATE_PATROL:
        spawn_data.do_frame_spawn();
        break;
    case VRAIL_PATROL_STATE_GOING_TO_DROP_OFF:
    case VRAIL_PATROL_STATE_COMPLETE_WAIT:
        if (spawn_data.is_in_use())
        {
            spawn_data.release_and_reset();
        }
        if (!played_vrail_end && patrol_state == VRAIL_PATROL_STATE_COMPLETE_WAIT && (vp == nullptr || vp->forward_velocity <= RS_VEHICLE_STOPPED_SPEED))
        {
            played_vrail_end = true;
            player_exit_vrail_vehicle();
        }
        break;
    case VRAIL_PATROL_STATE_COMPLETE:
    case VRAIL_PATROL_STATE_FAILED:
        // Original: this state is running here, so a failed rail never ends from this test.
        if (!activity_vrail_is_riding_shotgun_running() || patrol_state != VRAIL_PATROL_STATE_FAILED)
        {
            gamelog_set_activity_failed_reason(GLCR_OUT_OF_RANGE);
            determine_reward_and_end();
        }
        break;
    default:
        break;
    }
}

// 0x00786890 (:1961). Original: looks the squad up and draws nothing.
void activity_vehicle_rail_data::render_icon_player() const
{
}

// 0x007868B0 (:1982). Original: nothing sets Render_reinforcement_icon, so this draws nothing.
void activity_vehicle_rail_data::render_icon_attackers() const
{
    if (!reinforcement_should_render_icon())
    {
        return;
    }
    for (int i = 0; i < spawn_data.chase_vehicle_array.capacity(); i++)
    {
        vector pos;
        if (spawn_data.chase_vehicle_array[i].get_squad_position(pos))
        {
            Cartographer.add_temporary_icon(pos, MAP_ELEMENT_STAR_RED_ICON, true, 0xFFFFFFFF, Default_map_item_color, false, 0xFFFFFFFF);
        }
    }
}

// 0x007B6D20 (:1999): true once the driver squad is spawned, streamed and voiced, and the rail is set up on it.
bool activity_vehicle_rail_data::object_setup()
{
    if (current_vrail_def == nullptr)
    {
        return false;
    }
    const object_patrol* patrol = vrail_patrol_handle != 0xFFFFFFFF ? World.object_from_handle<object_patrol>(vrail_patrol_handle) : nullptr;
    if (patrol == nullptr)
    {
        return false;
    }
    object_squad* squad;
    if (squad_handle == 0xFFFFFFFF)
    {
        squad = spawn_squad(patrol);
        // Deviation: rfg.exe reads the handle through null when the spawn fails.
        if (squad == nullptr)
        {
            return false;
        }
        squad_handle = squad->get_handle();
        squad->squad_flags.ignore_road_spawn = 1;
    }
    else
    {
        squad = World.object_from_handle<object_squad>(squad_handle);
        if (squad == nullptr)
        {
            return false;
        }
    }
    if (squad->simulated())
    {
        return false;
    }
    for (int i = 0; i < squad->member_array.size(); i++)
    {
        const unsigned int member = squad->member_array[i];
        const npc* np = member != 0xFFFFFFFF ? World.object_from_handle<npc>(member) : nullptr;
        if (np == nullptr || !np->streamed() || np->get_voice() == nullptr || !np->get_voice()->is_loaded())
        {
            return false;
        }
    }
    spawn_timeout.set(0);
    if (!patrol_data.vrail_init(squad, drop_off_pos) || !spawn_data.vrail_init(squad, current_vrail_def, patrol, current_vrail_def->chase_vehicle_count))
    {
        release_squad(squad);
        return false;
    }
    if (current_info != nullptr && current_info->vehicle_hit_points > 0)
    {
        vehicle_set_max_hp(current_info->vehicle_hit_points, patrol_data.get_patrol_vehicle_handle());
    }
    return true;
}

// 0x007B31F0 (:2076)
object_squad* activity_vehicle_rail_data::spawn_squad(const object_patrol* patrol)
{
    if (patrol == nullptr || current_vrail_def == nullptr)
    {
        return nullptr;
    }
    squad_spawn_parameters params;
    params.orders.orders_type = GFM_ORDERS_TYPE_PATROL;
    params.orders.ofp = ACTIVITY_ORDERS_FLAGS;
    params.orders.object_target = patrol->get_handle();
    params.orders.location_target = patrol->pos;
    const object* start = find_driver_spawn_object();
    if (start == nullptr)
    {
        start = patrol;
    }
    params.squad.squad_pos = start->pos;
    params.squad.squad_orient = start->orient;
    params.squad.squad_def = current_vrail_def->driver_squad_def;
    params.squad.veh_spawn_group = current_vrail_def->player_veh_spawn_group;
    params.squad.hflags.invulnerable = 1;
    params.squad.hflags.disallow_vehicle_exit = 1;
    params.squad.npc_flags.activity_driver_vr = 1;
    params.squad.vflags.m_invulnerable = 1;
    params.squad.vflags.no_ragdoll = 1;
    params.squad.squad_flags.vehicle_infinite_mass = 1;
    params.squad.squad_flags.disallow_reassignment = 1;
    return params.spawn_squad();
}

// 0x007B3340 (:2117)
void activity_vehicle_rail_data::determine_reward_and_end()
{
    if (current_info == nullptr || vrail_has_ended)
    {
        return;
    }
    vrail_has_ended = true;
    dialogue_manager_cancel_all();
    const bool rail_done = vrail_completed();
    const bool score_reached = target_score_reached();
    if (score_reached && rail_done)
    {
        ui_tool_tips_add_tip("Guerrilla_Action_Complete", RS_ACTIVITY_COMPLETE_TOOLTIP_DELAY_MS, nullptr);
        activity_standard_rewards rewards = current_info->standard_rewards;
        rewards.morale += get_current_score() < current_info->par_score ? 0.0f : current_info->extra_reward;
        apply_standard_rewards(rewards, nullptr);
        if (Riding_shotgun_chapter_number < Num_riding_shotgun_story_chapters)
        {
            Riding_shotgun_chapter_number++;
        }
        object_riding_shotgun_node* node = riding_shotgun_node_handle != 0xFFFFFFFF ? World.object_from_handle<object_riding_shotgun_node>(riding_shotgun_node_handle) : nullptr;
        if (node != nullptr)
        {
            node->set_completed();
        }
        else
        {
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
        }
        stats_update_riding_shotgun_score(get_chapter_name(), get_current_score());
        vrail_successfully_completed = true;
        record_victory();
    }
    else
    {
        if (!score_reached && rail_done)
        {
            gamelog_set_activity_failed_reason(GLCR_TARGET_GOAL_NOT_REACHED);
        }
        vrail_started_and_failed = true;
        record_loss();
    }
    if (hud_ui_activity_complete_start(ACTIVITY_MODE_VEHICLE_RAIL, score_reached && rail_done))
    {
        active_state = RS_ACTIVE_STATE_WAIT_SPLASH_SCREEN;
    }
    else
    {
        activity_end();
    }
}

// 0x0075A9A0 (:2190)
bool activity_vehicle_rail_data::vrail_completed()
{
    return patrol_data.last_patrol_state == VRAIL_PATROL_STATE_COMPLETE || patrol_data.last_patrol_state == VRAIL_PATROL_STATE_COMPLETE_WAIT;
}

// 0x0075A9C0 (:2195)
void activity_vehicle_rail_data::update_display_score()
{
    const float score = get_current_score();
    float next = (score - display_score) * Frametime * 2.0f + display_score;
    if (next < 0.0f)
    {
        next = 0.0f;
    }
    display_score = next >= score ? score : next;
}

// 0x0079A650 (:2442)
int activity_vehicle_rail_data::get_squads(base_array<object_squad*>& squad_list)
{
    return spawn_data.get_squads(squad_list);
}

// ---- free functions

// 0x007AF010 (:2260)
void activity_riding_shotgun_exit()
{
    const player* pp = player_get_local_player();
    if (pp != nullptr && !pp->is_killed())
    {
        Vehicle_rail_activity_data.activity_end();
    }
}

// 0x0075AAE0 (:2274)
void activity_riding_shotgun_replay()
{
    Activity_node_start_on_load = Vehicle_rail_activity_data.get_spawn_object_handle();
    World.save_and_reload();
}

// 0x007B3500 (:2283)
void activity_riding_shotgun_process_player_death()
{
    gamelog_set_activity_failed_reason(GLCR_PLAYER_DIED);
    Vehicle_rail_activity_data.determine_reward_and_end();
}

// 0x007A2710 (:2290)
bool activity_riding_shotgun_start(const object_riding_shotgun_node* node)
{
    const unsigned int handle = activity_int_get_district_handle_from_node(node);
    const district* dp = handle != 0xFFFFFFFF ? World.object_from_handle<district>(handle) : nullptr;
    return Vehicle_rail_activity_data.activity_base::start_activity_spawn(node, dp);
}

// 0x0075AB00 (:2299)
bool activity_vrail_is_riding_shotgun_running()
{
    return Vehicle_rail_activity_data.current_info != nullptr && Vehicle_rail_activity_data.is_running();
}

// 0x007A2750 (:2309)
bool activity_riding_shotgun_can_be_started()
{
    return !Multiplayer && activity_none() && Mission_active_index == INVALID_MISSION_HANDLE && !mission_prevent_activity_spawn() && Vehicle_rail_activity_data.get_state() == BASE_ACTIVITY_STATE_WAIT && Vehicle_rail_activity_data.activity_base_modal::can_initialize() && (int)gsm_get_alert_level() < 2; // Magic number: below a red alert
}

// 0x007A27A0 (:2343): the intro until a first node is finished, the table's start line after.
bool activity_riding_shotgun_play_start_voiceline(object_riding_shotgun_node* rs_node)
{
    if (!activity_riding_shotgun_can_be_started())
    {
        return false;
    }
    bool any_completed = false;
    for (int i = 0; i < World.get_num_objects<object_riding_shotgun_node>(); i++)
    {
        if (World.get_object<object_riding_shotgun_node>(i)->is_completed())
        {
            any_completed = true;
            break;
        }
    }
    Vehicle_rail_activity_data.current_info = rs_node->get_riding_shotgun_info();
    if (Vehicle_rail_activity_data.current_info == nullptr)
    {
        return false;
    }
    Vehicle_rail_activity_data.play_radio_line(any_completed ? RIDING_SHOTGUN_RADIO_LINE_ACTIVITY_START : RIDING_SHOTGUN_RADIO_LINE_ONE_TIME_INTRO, DIALOGUE_PRIORITY_UNDEFINED);
    return true;
}

// 0x0079A530 (:2374). rfg.exe inlines district_get_from_pos.
void activity_riding_shotgun_liberation()
{
    for (int i = 0; i < World.get_num_objects<object_riding_shotgun_node>(); i++)
    {
        object_riding_shotgun_node* node = World.get_object<object_riding_shotgun_node>(i);
        const district* dp = district_get_from_pos(node->pos);
        if (dp == nullptr || dp->is_liberated())
        {
            node->stop_flare();
        }
    }
}

// 0x00786910 (:2386)
void activity_riding_shotgun_play_next_story_section()
{
    Vehicle_rail_activity_data.play_next_story_section(); // rfg.exe inlines it
}

// 0x0075AB40 (:2392)
void activity_riding_shotgun_get_display_score_string(char* buf, int size)
{
    _snprintf_safe(buf, size, "%.2f", (double)Vehicle_rail_activity_data.get_display_score());
}

// 0x0075AB90 (:2399)
void activity_riding_shotgun_get_target_score_string(char* buf, int size)
{
    _snprintf_safe(buf, size, "%.2f", (double)Vehicle_rail_activity_data.get_target_score());
}

// 0x0075ABE0 (:2407)
void acitivty_riding_shotgun_get_penalty_string(char* buf, int size)
{
    _snprintf_safe(buf, size, "- %.1f %%", (double)Vehicle_rail_activity_data.get_current_penalty());
}

// 0x00786930 (:2415)
float activity_riding_shotgun_get_vehicle_health_percentage()
{
    const unsigned int vehicle_handle = Vehicle_rail_activity_data.get_patrol_vehicle();
    const vehicle* vp = vehicle_handle != 0xFFFFFFFF ? World.object_from_handle<vehicle>(vehicle_handle) : nullptr;
    return vp != nullptr ? 1.0f - vp->get_vehicle_damage() : 1.0f;
}

// 0x0075AC10 (:2429)
void activity_riding_shotgun_get_score_results(float& target_goal, float& soldier_score, float& building_score, float& vehicle_score, float& base_score, float& penalty, float& total)
{
    target_goal = Vehicle_rail_activity_data.get_target_score();
    soldier_score = Vehicle_rail_activity_data.get_soldier_score();
    building_score = Vehicle_rail_activity_data.get_building_score();
    vehicle_score = Vehicle_rail_activity_data.get_vehicle_score();
    base_score = Vehicle_rail_activity_data.get_base_score();
    penalty = Vehicle_rail_activity_data.get_current_penalty();
    total = Vehicle_rail_activity_data.get_current_score();
}
