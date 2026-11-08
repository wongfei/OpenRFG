// Original: volition/rfg/code/objects/object_mission_start_node.cpp
//
// Mission start nodes. A node with the autostart property starts its mission as soon as the player stands on it
// (player_is_near_mission_start): that is how the prologue, "Tutorial", begins at the new game's player start.

#include "volition/rfg/code/objects/object_mission_start_node.h"

#include "volition/rfg/code/effects/effects.h"
#include "volition/rfg/code/effects/object_effect.h"
#include "volition/rfg/code/gameplay/activities/activity.h"
#include "volition/rfg/code/gameplay/mission_info.h"
#include "volition/rfg/code/gameplay/missions.h"
#include "volition/rfg/code/hud/minimap.h"
#include "volition/rfg/code/hud/hud_message.h"
#include "volition/rfg/code/hud/hud_ui_mission_briefing.h"
#include "volition/rfg/code/interface/localization/localization_shared.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/scripting/dialogue_manager.h"
#include "volition/rfg/code/level/rfg_prop_name_shared.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/object_action_node.h"
#include "volition/rfg/code/objects/object_spawn_node.h"
#include "volition/vlib/code/memory/object_pool.h"
#include "volition/vlib/code/os/vconsole.h"
#include "volition/vlib/code/util/varray.h"

extern unsigned int Mission_effect_info_handle; // mission_info.cpp

// Magic numbers: the pool holds 32 nodes (object_pool<object_mission_start_node,32,1>). 0x200 is the action node's
// disabled_by bit for "the mission start node is hidden".
static object_pool<object_mission_start_node,32,1> object_mission_start_nodepool; // 0x03067FC0
static const int ACTION_NODE_DISABLED_BY_MISSION_START = 0x200;
static bool Markers_enabled; // 0x0302F157
static prop_name PROP_MISSION_START_INFO("mission_info"); // 0x016ADE74
static prop_name PROP_MISSION_START_ENABLED("enabled"); // 0x016ADE7C
static prop_name PROP_MISSION_START_AUTO("autostart"); // 0x016ADE84

// 0x00AB40A0 (:42)
void unused_dcf_mission_set_enabled()
{
    if (!Console_command)
    {
        if (Console_help)
        {
            console_output("mission_set_enabled <mission_name> (true|false)", nullptr, false);
        }
        return;
    }
    console_get_arg(2, 0);
    mission_info* info = mission_info_get(Console_arg);
    if (info == nullptr)
    {
        console_output("Invalid mission name.\r\n", nullptr, false);
        return;
    }
    object_mission_start_node* node = info->start_node_handle != 0xFFFFFFFF ? World.object_from_handle<object_mission_start_node>(info->start_node_handle) : nullptr;
    if (node == nullptr)
    {
        console_output("Couldn't find a start node for that mission.\r\n", nullptr, false);
        return;
    }
    console_get_arg(0xC0, 0);
    node->set_enabled((Console_arg_type & 0x40) != 0);
}

// 0x00A19F30 (:74)
static bool should_enable_markers()
{
    return !mission_is_active() && activity_none();
}

// 0x00AB4160 (:90)
static void mission_start_node_enable_markers()
{
    if (!Markers_enabled)
    {
        for (int i = 0; i < World.get_num_objects<object_mission_start_node>(); i++)
        {
            World.get_object<object_mission_start_node>(i)->enable_markers();
        }
        Markers_enabled = true;
    }
}

// 0x00AA9E20 (:107)
static void mission_start_node_disable_markers()
{
    if (Markers_enabled)
    {
        for (int i = 0; i < World.get_num_objects<object_mission_start_node>(); i++)
        {
            World.get_object<object_mission_start_node>(i)->disable_markers();
        }
        Markers_enabled = false;
    }
}

// 0x00A67260 (:131)
object* alloc_object_mission_start_node()
{
    return object_mission_start_nodepool.alloc(0);
}

// 0x00A19F60 (:131)
void types_object_mission_start_node(signed char& type, signed char& subtype)
{
    type = OT_MISSION_START_NODE;
    subtype = -1;
}

// 0x00A19F70 (:131)
int bpo_index_object_mission_start_node()
{
    return 1;
}

// 0x00A19F80 (:131)
const char* classname_object_mission_start_node()
{
    return "object_mission_start_node";
}

static obj_class_instantiator object_mission_start_node_instantiator("object_mission_start_node", alloc_object_mission_start_node, types_object_mission_start_node, bpo_index_object_mission_start_node,
                                                                      classname_object_mission_start_node, object_mission_start_node::pool_remaining, 0x15E0, 32); // (0x01077B70)

// 0x00A19F90 (:131)
unsigned int object_mission_start_node::get_classname_hash()
{
    return object_mission_start_node_instantiator.this_hash;
}

// 0x00A3CDB0 (:131)
int object_mission_start_node::pool_remaining()
{
    return object_mission_start_nodepool.get_free();
}

// 0x00A92080 (:131)
void object_mission_start_node::pool_free()
{
    object_mission_start_nodepool.free(this);
}

// 0x00A19FA0 (:132)
object_mission_start_node::object_mission_start_node()
{
}

// 0x00A19FD0
object_mission_start_node::~object_mission_start_node()
{
}

// 0x00754EC0 (.h:22)
const object_type object_mission_start_node::get_type_static()
{
    return OT_MISSION_START_NODE;
}

// 0x00754ED0 (.h:22)
const object_sub_type object_mission_start_node::get_subtype_static()
{
    return OT_SUB_UNDEFINED;
}

// 0x00A100C0 (.h:22)
const char* object_mission_start_node::get_classname_static()
{
    return "object_mission_start_node";
}

// 0x0091D000 (.h:22)
int object_mission_start_node::get_bpo_index_static()
{
    return 1;
}

// 0x00A19FC0 (.h:22)
const char* object_mission_start_node::get_classname() const
{
    return "object_mission_start_node";
}

// 0x00754EE0 (.h:31)
mission_info* object_mission_start_node::get_mission_info() const
{
    return m_info;
}

// 0x00A100D0 (.h:41)
bool object_mission_start_node::should_autostart()
{
    return autostart;
}

// 0x00AB41C0 (:144)
bool object_mission_start_node::serialize(obj_prop_block& pblock, bool load)
{
    if (!object::serialize(pblock, load))
    {
        return false;
    }
    if (!load)
    {
        if (m_info != nullptr)
        {
            pblock.set(PROP_MISSION_START_INFO, m_info->name);
        }
        return true;
    }
    if (Multiplayer)
    {
        return false;
    }
    char name[256];
    name[0] = '\0';
    if (!pblock.get_optional(PROP_MISSION_START_INFO, name))
    {
        return false;
    }
    m_info = mission_info_get(name);
    if (m_info == nullptr)
    {
        return false;
    }
    enabled = true;
    pblock.get_optional(PROP_MISSION_START_ENABLED, enabled);
    autostart = false;
    pblock.get_optional(PROP_MISSION_START_AUTO, autostart);
    m_info->start_node_handle = get_handle();
    if (!mission_is_active())
    {
        enable_markers();
    }
    effect_handle = 0xFFFFFFFF;
    return true;
}

// 0x00A67270 (:198)
bool object_mission_start_node::stream(bool load)
{
    if (load)
    {
        if (can_start() && !autostart)
        {
            start_effect();
        }
    }
    else
    {
        if (obj_flags.destroy_on_stream)
        {
            World.destroy_object(this, false);
        }
        stop_effect();
    }
    return true;
}

// 0x00A92090 (:226)
void object_mission_start_node::free_this()
{
    Cartographer.remove_icon(get_handle());
    object::free_this();
}

// 0x00AA9E80 (:235)
void object_mission_start_node::enable_markers()
{
    if (!can_start() || autostart)
    {
        return;
    }
    Cartographer.add_icon(get_handle(), MAP_ELEMENT_MISSION_START, true, false, false);
    if (obj_flags.streamed)
    {
        start_effect();
    }
    farray<object*,128> descendants;
    world_collect_descendants(this, descendants, nullptr, nullptr);
    for (int i = 0; i < descendants.size(); i++)
    {
        object* objp = descendants[i];
        if (objp->get_obj_type() == OT_ACTION_NODE)
        {
            object_action_node* node = (object_action_node*)objp;
            if ((node->disabled_by & ACTION_NODE_DISABLED_BY_MISSION_START) != 0)
            {
                node->disabled_by &= ~ACTION_NODE_DISABLED_BY_MISSION_START;
                if (node->disabled_by == 0)
                {
                    node->start_hotspot_effect();
                }
            }
        }
        else if (objp->get_obj_type() == OT_SQUAD_SPAWN_NODE)
        {
            ((object_squad_spawn_node*)objp)->set_mission_start_disabled(false);
        }
    }
}

// 0x00A920B0 (:283)
void object_mission_start_node::disable_markers()
{
    Cartographer.remove_icon(get_handle());
    stop_effect();
    farray<object*,128> descendants;
    world_collect_descendants(this, descendants, nullptr, nullptr);
    for (int i = 0; i < descendants.size(); i++)
    {
        object* objp = descendants[i];
        if (objp->get_obj_type() == OT_ACTION_NODE)
        {
            object_action_node* node = (object_action_node*)objp;
            if (node->disabled_by == 0)
            {
                human* occupant = node->get_occupant();
                // Magic number: subtype 3, the player keeps the node.
                if (occupant != nullptr && occupant->get_sub_type() != 3)
                {
                    occupant->release_action_node(node->get_handle());
                }
                node->stop_hotspot_effect();
            }
            node->disabled_by |= ACTION_NODE_DISABLED_BY_MISSION_START;
        }
        else if (objp->get_obj_type() == OT_SQUAD_SPAWN_NODE)
        {
            ((object_squad_spawn_node*)objp)->set_mission_start_disabled(true);
        }
    }
}

// 0x00A19FE0 (:325)
bool object_mission_start_node::can_start()
{
    return !mission_is_active() && mission_get_checkpoint_handle() == INVALID_MISSION_HANDLE && activity_none() && m_info != nullptr && !m_info->mission_flags.complete && m_info->is_unlocked() && enabled;
}

// 0x00AA9FB0 (:336)
void object_mission_start_node::set_enabled(bool set_val)
{
    enabled = set_val;
    if (can_start())
    {
        enable_markers();
        return;
    }
    disable_markers();
}

// 0x00A3CDE0 (:356)
void object_mission_start_node::start_effect()
{
    if (effect_handle != 0xFFFFFFFF && World.object_from_handle<object_effect>(effect_handle) != nullptr)
    {
        return;
    }
    effect_handle = effects_play(Mission_effect_info_handle, pos, orient, true, 0xFFFFFFFF, -1, 0xFFFFFFFF, 1.0f, false, EFFECTS_CATEGORY_NONE);
}

// 0x00A3CE40 (:365)
void object_mission_start_node::stop_effect()
{
    if (effect_handle != 0xFFFFFFFF)
    {
        object_effect* effect = World.object_from_handle<object_effect>(effect_handle);
        if (effect != nullptr)
        {
            effect->stop(object_effect::SS_STOP_EMITTING);
        }
    }
    effect_handle = 0xFFFFFFFF;
}

// 0x00AB42F0 (:384)
void object_mission_start_node_do_frame()
{
    if (should_enable_markers())
    {
        mission_start_node_enable_markers();
        return;
    }
    mission_start_node_disable_markers();
}

// 0x00A1A030 (:400)
void object_mission_start_node_level_init()
{
    Markers_enabled = false;
}

// 0x00AB4310 (:407)
void object_mission_start_node_update_markers()
{
    Markers_enabled = false;
    mission_start_node_enable_markers();
}

// player.cpp's, here so that the programs on stand-ins for the mission code link player.cpp without it.
extern unsigned int Player_mission_radio_last_node; // 0x01694450
extern voice_line_handle Player_mission_radio_message_rfc; // 0x0169
extern voice_line_handle Player_mission_radio_message_tech;
extern timestamp Player_mission_radio_timer; // 0x01695614

// 0x00A5B9A0: the nearest mission start the player can use, within 3 m. An autostart node starts its mission at once.
bool player_is_near_mission_start(player* pp, t_usable_object* usable_object)
{
    if (Multiplayer || human_in_vehicle_interaction(pp, nullptr))
    {
        return false;
    }
    object_mission_start_node* nearest = nullptr;
    // Magic number: 3 m, squared.
    float nearest_dist_squared = 9.0f;
    for (int i = 0; i < World.get_num_objects<object_mission_start_node>(); i++)
    {
        object_mission_start_node* node = World.get_object<object_mission_start_node>(i);
        if (node->can_start())
        {
            const float dist_squared = vec_dist_squared(pp->pos, node->pos);
            if (dist_squared <= nearest_dist_squared)
            {
                nearest_dist_squared = dist_squared;
                nearest = node;
            }
        }
    }
    if (nearest == nullptr)
    {
        Player_mission_radio_last_node = 0xFFFFFFFF;
        Player_mission_radio_timer.invalidate();
        return false;
    }
    if (nearest->should_autostart())
    {
        if (World.get_world_state() == WSM_DEFAULT)
        {
            mission_activate(nearest->get_mission_info()->name, true);
        }
        return false;
    }
    if (mission_alert_allows_start())
    {
        if (Player_mission_radio_last_node != 0xFFFFFFFF)
        {
            Player_mission_radio_last_node = 0xFFFFFFFF;
            Player_mission_radio_timer.invalidate();
        }
        usable_object->action_callback = player_start_mission;
        usable_object->action_type = GENERAL_ACTION_START_MISSION;
        usable_object->action_icon = USE_OBJECT_ICON_MISSION;
        usable_object->dist_squared = nearest_dist_squared;
        usable_object->handle = nearest->get_handle();
        usable_object->dot_prod = 1.0f;
        return true;
    }
    if (nearest->get_handle() != Player_mission_radio_last_node && !Player_mission_radio_timer.elapsed())
    {
        const mission_info* camp = mission_info_get("Save the Guerrilla Camp");
        const voice_line_handle line = camp->mission_flags.complete ? Player_mission_radio_message_tech : Player_mission_radio_message_rfc;
        human_voice_persona* persona = human_voice_radio_persona();
        if (persona != nullptr)
        {
            dialogue_manager_start_dialogue(line, persona, DIALOGUE_PRIORITY_RADIO, INVALID_PA_HANDLE, nullptr);
        }
        hud_message_add_minor(localization_get_display_string_req(mission_allow_at_yellow_alert() ? "HM_MISSION_ALERT_YELLOW" : "HM_MISSION_ALERT", nullptr), -1.0f, false);
    }
    Player_mission_radio_last_node = nearest->get_handle();
    return false;
}

// 0x00A342D0 (:6174)
bool player_start_mission(player* pp, bool do_action)
{
    if (Multiplayer || mission_is_active() || human_in_vehicle_interaction(pp, nullptr) || Cartographer.in_fullscreen_map() || pp->action_object.handle == 0xFFFFFFFF)
    {
        return false;
    }
    object_mission_start_node* node = World.object_from_handle<object_mission_start_node>(pp->action_object.handle);
    if (node == nullptr || node->get_mission_info() == nullptr)
    {
        return false;
    }
    if (do_action)
    {
        mission_info* info = node->get_mission_info();
        info->mission_flags.viewed = true;
        if (info->mis_mode != MISSION_MODE_TUTORIAL && info->mis_mode != MISSION_MODE_CUTSCENE)
        {
            hud_ui_mission_briefing_start(info->name, false);
            return true;
        }
        mission_activate(info->name, false);
    }
    return true;
}
