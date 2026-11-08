// Original: volition/rfg/code/objects/object_bftp_node.cpp
//
// Blast from the past: tag nodes the player walks over (within 1.5 m, on foot) and car bomb nodes paired with them by
// number. Every third tag (or every tag with Bftp_always_bomb) enables the paired bomb node, which spawns a "bftp_bomb"
// item with a blinking light and a map icon; driving a car to the bomb (within 5 m) touches the item, which arms the car
// (activity_car_bomb takes over). Registers the class, so zones now create these objects.

#include "volition/rfg/code/objects/object_bftp_node.h"

#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/rfg/code/effects/effects.h"
#include "volition/rfg/code/effects/effects_shared.h"
#include "volition/rfg/code/event_logging/definitions_events/events.h"
#include "volition/rfg/code/event_logging/gamelog.h"
#include "volition/rfg/code/gameplay/activities/activity.h"
#include "volition/rfg/code/gameplay/activities/activity_car_bomb.h"
#include "volition/rfg/code/gamewide/gameaudio.h"
#include "volition/rfg/code/gamewide/stats.h"
#include "volition/rfg/code/hud/hud_message.h"
#include "volition/rfg/code/hud/hud_ui_handbook.h"
#include "volition/rfg/code/hud/minimap.h"
#include "volition/rfg/code/interface/ui_document_hud_tool_tips.h"
#include "volition/rfg/code/level/rfg_prop_block_shared.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/interface/localization/localization_shared.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/object_item.h"
#include "volition/rfg/code/objects/object_prop_names_shared.h"
#include "volition/rfg/code/vehicle/automobile.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/vlib/code/memory/object_pool.h"
#include "volition/vlib/code/util/unicode_tools.h"

static object_pool<object_bftp_node, 128, 1> object_bftp_nodepool; // 0x030544E0

static bool Bftp_always_bomb; // 0x0302F11E: every tag enables its bomb (debug, never set)
static bool Bftp_all_bombs_on; // 0x0302F11F: every bomb node is enabled (debug, never set)
static int Bftp_current_car_bomb_id = -1; // 0x0169457C: the pair number of the bomb armed on the player's car
static bool show_handbook_on_tag = true; // 0x0169452D

static const float BFTP_TAG_TOUCH_DIST_SQUARED = 2.25f; // Magic number: 1.5 m, the tag pickup distance
static const float BFTP_BOMB_TOUCH_DIST_SQUARED = 25.0f; // Magic number: 5 m, from the car to the bomb item

// 0x00A15B70 (:226)
void types_object_bftp_node(signed char& type, signed char& subtype)
{
    type = OT_BFTP_NODE;
    subtype = OT_SUB_UNDEFINED;
}

// 0x00A15B80 (:226)
int bpo_index_object_bftp_node()
{
    return 1;
}

// 0x00A15B90 (:226)
const char* classname_object_bftp_node()
{
    return "object_bftp_node";
}

// 0x00A611D0 (:226)
object* alloc_object_bftp_node()
{
    return object_bftp_nodepool.alloc(0);
}

// Magic numbers: rfg.exe's arguments (0x010766C0), the pool's bytes and capacity.
static obj_class_instantiator object_bftp_node_instantiator("object_bftp_node", alloc_object_bftp_node, types_object_bftp_node, bpo_index_object_bftp_node, classname_object_bftp_node, object_bftp_node::pool_remaining, 0x5AC0, 0x80); // 0x03169678

// ---- internal functions

// 0x00A15B20 (:60). LTCG: bftp_node in ESI.
// A bomb node shows a car bomb icon on the map.
static void create_bftp_bomb_icon(object_bftp_node* bftp_node)
{
    if (bftp_node != nullptr && !bftp_node->is_tag_node() && !Cartographer.does_icon_exist(bftp_node->get_handle()))
    {
        Cartographer.add_icon(bftp_node->get_handle(), MAP_ELEMENT_CARBOMB, true, false, false);
    }
}

// 0x00A60FD0 (:76). LTCG: bftp_node in ESI.
// Spawns the bomb item on the node, with the remote charge light blinking on it; the item handle, or 0xFFFFFFFF.
static unsigned int create_bftp_bomb(object_bftp_node* bftp_node)
{
    if (bftp_node == nullptr)
    {
        return 0xFFFFFFFF;
    }
    create_bftp_bomb_icon(bftp_node);
    const vector pos = bftp_node->pos;
    const matrix orient(bftp_node->orient);
    obj_item_flags flags;
    flags.cast_drop_shadows = 1;
    flags.cast_shadows = 1;
    flags.preplaced = 1;
    flags.no_save = 1;
    flags.simulate = 1;
    obj_item_props props("bftp_bomb", pos, orient, flags, COLLIDE_ITEM, -1.0f, INVALID_SALVAGE_MATERIAL);
    item* bomb = World.create_object_global<item>(props, 0xFFFFFFFF);
    if (bomb == nullptr)
    {
        return 0xFFFFFFFF;
    }
    const unsigned int bomb_handle = bomb->get_handle();
    const int tag = bomb->get_tag_index_from_name("blinking_light");
    bftp_node->bftp_effect = effects_play(effects_get_handle("wep_remote_charge_light_a"), bomb_handle, tag, true, 0xFFFFFFFF, 1.0f, false);
    return bomb_handle;
}

// 0x00A61110 (:122)
static object_bftp_node* get_bomb_node(int pair_number)
{
    for (int i = 0; i < World.get_num_objects<object_bftp_node>(); i++)
    {
        object_bftp_node* node = World.get_object<object_bftp_node>(i);
        if (!node->is_tag_node() && node->get_pair_number() == pair_number)
        {
            return node;
        }
    }
    return nullptr;
}

// 0x00A61170 (:139)
static object_bftp_node* find_bftp_node_from_item(unsigned int item_handle)
{
    for (int i = 0; i < World.get_num_objects<object_bftp_node>(); i++)
    {
        object_bftp_node* node = World.get_object<object_bftp_node>(i);
        if (node->get_item_handle() == item_handle)
        {
            return node;
        }
    }
    return nullptr;
}

// 0x00A8DB50 (:157). LTCG: bftp_node in ESI.
// The local player collects a tag: every third enables its bomb ("MOAB_Located"), the others count down to the next.
static void bftp_touch_tag(human* hp, object_bftp_node* bftp_node)
{
    if (hp == nullptr || hp != Player || bftp_node == nullptr || !bftp_node->is_tag_node() || bftp_node->is_collected())
    {
        return;
    }
    bftp_node->collect_item();
    const int tags_left = 3 - blast_from_the_past_tags_collected() % 3; // Magic number: a bomb every three tags
    if (tags_left == 3 || Bftp_always_bomb)
    {
        ui_tool_tips_add_tip("MOAB_Located", 0, nullptr);
        bftp_node->spawn_bomb_from_tag();
        hud_message_add(localization_get_display_string_req("HM_BFTP_SPAWN_BOMB", nullptr), 4.0f, nullptr, nullptr, nullptr);
    }
    else
    {
        ui_tool_tips_add_tip("Blast_from_the_Past", 0, nullptr);
        wchar_t msg[128];
        if (tags_left == 1)
        {
            sprintf(msg, localization_get_display_string_req("HM_BFTP_TAG_FOUND", nullptr));
        }
        else
        {
            wchar_t count[10];
            sprintf(count, L"%d", tags_left);
            sprintf(msg, localization_get_display_string_req("HM_BFTP_TAGS_FOUND", nullptr), count);
        }
        hud_message_add(msg, 4.0f, nullptr, nullptr, nullptr);
    }
    audiolib_cue_play(Ga_cue_ids.bftp_pickup_tag, nullptr, nullptr);
    if (bftp_node->child_ptr != nullptr)
    {
        World.destroy_object(bftp_node->child_ptr->get_handle(), false);
    }
    stats_increment_by_one(STATS_TAGS_FOUND);
    const vector item_pos = bftp_node->pos;
    const int duration = gamelog_get_time_played();
    const char difficulty = gamelog_get_difficulty();
    const int locale_id = gamelog_get_locale();
    const char pct_game_complete = gamelog_get_pct_complete();
    const int time_played = gamelog_get_time_played();
    const int item_type = 0; // tag
    const int time = gamelog_get_logging_time();
    const int player_checksum = gamelog_get_player_checksum();
    log_event_player_find_collectable(player_checksum, time, item_type, time_played, pct_game_complete, locale_id, difficulty, duration, item_pos);
    if (show_handbook_on_tag)
    {
        hud_ui_handbook_show();
    }
}

// ---- object_bftp_node

// 0x00A15BB0 (:227)
object_bftp_node::object_bftp_node()
{
}

// 0x00A15BE0
object_bftp_node::~object_bftp_node()
{
}

// 0x0084C2E0 (.h:23)
const object_type object_bftp_node::get_type_static()
{
    return OT_BFTP_NODE;
}

// 0x0084C2F0 (.h:23)
const object_sub_type object_bftp_node::get_subtype_static()
{
    return OT_SUB_UNDEFINED;
}

// 0x00A10050 (.h:23)
const char* object_bftp_node::get_classname_static()
{
    return "object_bftp_node";
}

// 0x00A10060 (.h:23)
int object_bftp_node::get_bpo_index_static()
{
    return 1;
}

// 0x00A15BD0 (.h:23)
const char* object_bftp_node::get_classname() const
{
    return "object_bftp_node";
}

// 0x00A15BA0 (:226)
unsigned int object_bftp_node::get_classname_hash()
{
    return object_bftp_node_instantiator.this_hash;
}

// 0x00A39590 (:226)
int object_bftp_node::pool_remaining()
{
    return object_bftp_nodepool.get_free();
}

// 0x00A8DD50 (:226)
void object_bftp_node::pool_free()
{
    object_bftp_nodepool.free(this);
}

// 0x00AA4A40 (:231)
// A bomb node that is enabled and not collected gets its map icon at load. Loading clears the armed car bomb.
// Original: a save made while the player's car carries a bomb stores that bomb's node as not collected, so the bomb
// comes back at load.
bool object_bftp_node::serialize(obj_prop_block& pblock, bool load)
{
    if (!object::serialize(pblock, load))
    {
        return false;
    }
    if (load)
    {
        *reinterpret_cast<unsigned char*>(&bftp_flags) = 0;
        bftp_item_handle = 0xFFFFFFFF;
        bftp_effect = 0xFFFFFFFF;
        bool value;
        // Original: tag_node and pair_number stay as the pool left them when the property is missing.
        if (pblock.get_optional(BFTP_PROP_COLLECTED, value))
        {
            bftp_flags.collected = value;
        }
        if (pblock.get_optional(BFTP_PROP_ENABLED_BOMB, value))
        {
            bftp_flags.enabled_bomb = value;
        }
        pblock.get_optional(BFTP_PROP_TAG_NODE, tag_node);
        pblock.get_optional(BFTP_PROP_PAIR_NUMBER, pair_number);
        if (!tag_node && (bftp_flags.enabled_bomb || Bftp_all_bombs_on) && !bftp_flags.collected)
        {
            create_bftp_bomb_icon(this);
        }
        Bftp_current_car_bomb_id = -1;
        return true;
    }
    if (pair_number == Bftp_current_car_bomb_id && !tag_node)
    {
        pblock.set(BFTP_PROP_COLLECTED, false);
    }
    else
    {
        const bool collected = bftp_flags.collected;
        pblock.set(BFTP_PROP_COLLECTED, collected);
    }
    const bool enabled_bomb = bftp_flags.enabled_bomb;
    pblock.set(BFTP_PROP_ENABLED_BOMB, enabled_bomb);
    pblock.set(BFTP_PROP_TAG_NODE, tag_node);
    pblock.set(BFTP_PROP_PAIR_NUMBER, pair_number);
    return true;
}

// 0x00A15BF0 (:287)
obj_serialize_mode object_bftp_node::serialize_mode()
{
    return OSM_FULL;
}

// 0x00A611E0 (:293)
// Streaming in spawns an enabled bomb, or removes a collected tag's child (the tag prop); streaming out removes the
// bomb item and its light.
bool object_bftp_node::stream(bool load)
{
    if (load)
    {
        if (!tag_node)
        {
            if ((bftp_flags.enabled_bomb || Bftp_all_bombs_on) && !bftp_flags.collected)
            {
                bftp_item_handle = create_bftp_bomb(this);
            }
        }
        else if (child_ptr != nullptr && bftp_flags.collected)
        {
            World.destroy_object(child_ptr->get_handle(), false);
        }
        return true;
    }
    if (obj_flags.destroy_on_stream)
    {
        World.destroy_object(this, false);
    }
    if (bftp_item_handle != 0xFFFFFFFF)
    {
        World.destroy_object(bftp_item_handle, false);
        bftp_item_handle = 0xFFFFFFFF;
    }
    if (bftp_effect != 0xFFFFFFFF)
    {
        World.destroy_object(bftp_effect, false);
        bftp_effect = 0xFFFFFFFF;
    }
    return true;
}

// 0x00A15C00 (:329)
bool object_bftp_node::is_collected()
{
    return bftp_flags.collected;
}

// 0x00A15C10 (:335)
void object_bftp_node::collect_item()
{
    bftp_flags.collected = true;
    hud_ui_handbook_unlock_bftp_entry(pair_number);
}

// 0x00A612A0 (:344)
// Enables an uncollected bomb node: its map icon, and its item when streamed.
void object_bftp_node::spawn_bomb_item()
{
    if (tag_node || bftp_flags.collected)
    {
        return;
    }
    bftp_flags.enabled_bomb = true;
    if (!Cartographer.does_icon_exist(get_handle()))
    {
        Cartographer.add_icon(get_handle(), MAP_ELEMENT_CARBOMB, true, false, false);
    }
    if (obj_flags.streamed)
    {
        bftp_item_handle = create_bftp_bomb(this);
    }
}

// 0x00A61300 (:364)
// A tag enables its paired bomb node, unless it already is.
void object_bftp_node::spawn_bomb_from_tag()
{
    if (!tag_node)
    {
        return;
    }
    object_bftp_node* bomb_node = get_bomb_node(pair_number);
    if (bomb_node != nullptr && !bomb_node->tag_node && !bomb_node->bftp_flags.enabled_bomb && !Bftp_all_bombs_on)
    {
        bomb_node->spawn_bomb_item();
    }
}

// 0x00A15C30 (:379)
const bool object_bftp_node::is_bomb_enabled() const
{
    return bftp_flags.enabled_bomb || Bftp_all_bombs_on;
}

// 0x00A61340 (:388)
void object_bftp_node::restore_bomb()
{
    bftp_flags.collected = false;
    spawn_bomb_item();
}

// 0x0084C300 (.h:48)
const bool object_bftp_node::is_tag_node() const
{
    return tag_node;
}

// 0x00A10070 (.h:49)
const int object_bftp_node::get_pair_number() const
{
    return pair_number;
}

// 0x00A10080 (.h:50)
const unsigned int object_bftp_node::get_item_handle() const
{
    return bftp_item_handle;
}

// ---- external functions

// 0x00A61350 (:406)
// The bomb item's touch script (item_touch_blast_from_the_past): arms the player's car, unless another bomb activity
// runs, the player is on foot or the car is already armed.
bool blast_from_the_past_touch_item(human* hp, unsigned int item_handle)
{
    if (hp == nullptr || hp != Player || activity_car_bomb_is_active() || activity_is_active(ACTIVITY_MODE_DELIVERY))
    {
        return false;
    }
    object_bftp_node* node = find_bftp_node_from_item(item_handle);
    if (node == nullptr || node->is_tag_node())
    {
        return false;
    }
    const char* message;
    automobile* vp = World.object_from_handle<automobile>(hp->vehicle_handle);
    if (vp == nullptr)
    {
        message = "HM_BFTP_GET_A_CAR";
    }
    else if (vp->m_bomb_status == VEHICLE_BOMB_ARMED)
    {
        message = "HM_BFTP_CAR_ALREADY_ARMED";
    }
    else
    {
        // Original: a collected node here only reached a debug break ("Int3 critical function").
        node->collect_item();
        vehicle_set_bomb_status(vp, VEHICLE_BOMB_ARMED);
        Cartographer.remove_icon(node->get_handle());
        audiolib_cue_play(Ga_cue_ids.bftp_arm_bomb, nullptr, nullptr);
        Bftp_current_car_bomb_id = node->get_pair_number();
        const vector item_pos = node->pos;
        const int duration = gamelog_get_time_played();
        const char difficulty = gamelog_get_difficulty();
        const int locale_id = gamelog_get_locale();
        const char pct_game_complete = gamelog_get_pct_complete();
        const int time_played = gamelog_get_time_played();
        const int item_type = 1; // car bomb
        const int time = gamelog_get_logging_time();
        const int player_checksum = gamelog_get_player_checksum();
        log_event_player_find_collectable(player_checksum, time, item_type, time_played, pct_game_complete, locale_id, difficulty, duration, item_pos);
        ui_tool_tips_add_tip("Blast", 0, nullptr);
        if (node->bftp_effect != 0xFFFFFFFF)
        {
            World.destroy_object(node->bftp_effect, false);
            node->bftp_effect = 0xFFFFFFFF;
        }
        return true;
    }
    hud_message_add(localization_get_display_string_req(message, nullptr), 4.0f, nullptr, nullptr, nullptr);
    return false;
}

// 0x00A61510 (:464)
int blast_from_the_past_tags_collected()
{
    int count = 0;
    for (int i = 0; i < World.get_num_objects<object_bftp_node>(); i++)
    {
        object_bftp_node* node = World.get_object<object_bftp_node>(i);
        if (node->is_tag_node() && node->is_collected())
        {
            count++;
        }
    }
    return count;
}

// 0x00AA4C60 (:480)
// Every frame (player_process_object_interaction), single player only: the player on foot collects tags within 1.5 m;
// in a car, it touches enabled bomb items within 5 m.
void blast_from_the_past_process_car_bombs()
{
    player* pp = Player;
    if (Multiplayer || pp == nullptr)
    {
        return;
    }
    object* car = nullptr;
    if (pp->vehicle_handle != 0xFFFFFFFF)
    {
        car = World.object_from_handle(pp->vehicle_handle);
        if (car != nullptr && (car->get_obj_type() != OT_VEHICLE || car->get_sub_type() != OT_SUB_VEHICLE_AUTO))
        {
            car = nullptr;
        }
    }
    const int num_nodes = World.get_num_objects<object_bftp_node>();
    for (int i = 0; i < num_nodes; i++)
    {
        object_bftp_node* node = World.get_object<object_bftp_node>(i);
        if (node->is_collected())
        {
            continue;
        }
        if (node->is_tag_node())
        {
            if (car == nullptr)
            {
                const float dx = node->pos.x - pp->pos.x;
                const float dy = node->pos.y - pp->pos.y;
                const float dz = node->pos.z - pp->pos.z;
                const float dist_squared = (float)((double)dx * dx + (double)dy * dy + (double)dz * dz);
                if (dist_squared <= BFTP_TAG_TOUCH_DIST_SQUARED)
                {
                    bftp_touch_tag(pp, node);
                }
            }
        }
        else if (node->is_bomb_enabled() && car != nullptr && node->get_item_handle() != 0xFFFFFFFF)
        {
            item* itemp = World.object_from_handle<item>(node->get_item_handle());
            if (itemp != nullptr)
            {
                vector diff(itemp->pos.x - car->pos.x, itemp->pos.y - car->pos.y, itemp->pos.z - car->pos.z);
                if (diff.mag_squared() <= BFTP_BOMB_TOUCH_DIST_SQUARED)
                {
                    object_item_player_touch(pp, itemp);
                }
            }
        }
    }
}

// 0x00A61570 (:544)
// The armed car bomb was lost (activity_car_bomb): its node spawns the bomb again.
void blast_from_the_past_reset_last_bomb()
{
    if (Bftp_current_car_bomb_id < 0)
    {
        return;
    }
    object_bftp_node* node = get_bomb_node(Bftp_current_car_bomb_id);
    if (node != nullptr)
    {
        node->restore_bomb();
        Bftp_current_car_bomb_id = -1;
    }
}

// 0x00A15C50 (:567)
void blast_from_the_past_bomb_destroyed()
{
    Bftp_current_car_bomb_id = -1;
}
