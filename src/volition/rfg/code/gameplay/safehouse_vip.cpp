// Original: volition/rfg/code/gameplay/safehouse_vip.cpp
//
// The safehouse VIP: when a safehouse streams in, one of its VIP squad spawn nodes may be picked to bring a guerrilla leader,
// who talks when the player is near, next to the radio operator's own lines. Scripts restrict who can be picked.

#include "volition/rfg/code/gameplay/safehouse_vip.h"

#include "volition/rfg/code/gameplay/activities/activity_vrail.h"
#include "volition/rfg/code/gameplay/gameflow/gfm_orders_manager.h"
#include "volition/rfg/code/gameplay/mission_info_shared.h"
#include "volition/rfg/code/gameplay/squad_spawn.h"
#include "volition/rfg/code/gamewide/tweak_table.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_info.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice_lines.h"
#include "volition/rfg/code/objects/humans/human_voice/voice_line_unlocking.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/object_action_node.h"
#include "volition/rfg/code/objects/object_safehouse.h"
#include "volition/rfg/code/objects/object_spawn_node.h"
#include "volition/rfg/code/objects/object_squad.h"
#include "volition/rfg/code/spawn/spawn_resource_manager.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/util/varray.h"

#include <float.h>

extern mission_handle Mission_active_index; // 0x0165150C (gameplay.cpp)

// ---- file statics
static farray<human_info const*, 4> Restricted_human_list; // 0x01659938
// Tweak table entry, written by the table's load (0x0222DE1C).
static float VIP_spawn_probability = 0.8f; // 0x01651540
static unsigned int VIP_spawn_node_handle = 0xFFFFFFFF; // 0x01651544
static unsigned int VIP_top_parent_handle = 0xFFFFFFFF; // 0x01651548
static unsigned int Radio_operator_handle = 0xFFFFFFFF; // 0x0165154C
static voice_line_handle Radio_operator_line_handle = INVALID_VOICE_LINE_HANDLE; // 0x01651550
static timestamp VIP_voice_line_timer; // 0x01659958
static timestamp Radio_operator_voice_line_timer; // 0x0165995C
static bool Spawn_ready; // 0x02229679

static tweak_table_item Tweak_table_item_VIP_spawn_probability("VIP_spawn_probability", &VIP_spawn_probability);

// 0x00790240 (:74)
// The radio operator, found again when the remembered one is gone.
static npc* get_radio_operator()
{
    if (Radio_operator_handle != 0xFFFFFFFF)
    {
        npc* np = World.object_from_handle<npc>(Radio_operator_handle);
        if (np != nullptr && np->hflags.radio_operator && !np->is_killed())
        {
            return np;
        }
    }
    const int count = World.get_num_objects<npc>();
    for (int i = 0; i < count; i++)
    {
        npc* np = World.get_object<npc>(i);
        if (np->hflags.radio_operator && !np->is_killed())
        {
            Radio_operator_handle = np->get_handle();
            return np;
        }
    }
    Radio_operator_handle = 0xFFFFFFFF;
    return nullptr;
}

// 0x0075F320 (:101)
static bool active_vip_spawn()
{
    return VIP_top_parent_handle != 0xFFFFFFFF;
}

// 0x00775420 (:107): the first member of the VIP node's squad.
static human* get_vip_human()
{
    if (VIP_spawn_node_handle == 0xFFFFFFFF)
    {
        return nullptr;
    }
    const object_squad_spawn_node* node = World.object_from_handle<object_squad_spawn_node>(VIP_spawn_node_handle);
    if (node == nullptr || node->get_child_squad_handle() == 0xFFFFFFFF)
    {
        return nullptr;
    }
    object_squad* squad = World.object_from_handle<object_squad>(node->get_child_squad_handle());
    if (squad == nullptr)
    {
        return nullptr;
    }
    // Original: reads the first member without looking at the count.
    const unsigned int handle = squad->member_array[0];
    return handle != 0xFFFFFFFF ? World.object_from_handle<human>(handle) : nullptr;
}

// 0x0075F330 (:127)
static const human_info* get_human_info(const object_squad_spawn_node* node)
{
    return node->get_info()->special_npc_info;
}

// 0x0075F340 (:136)
static bool in_vip_spawn_set(const object* objp)
{
    while (objp != nullptr)
    {
        if (objp->get_handle() == VIP_top_parent_handle || objp->get_parent() == VIP_top_parent_handle)
        {
            return true;
        }
        objp = World.object_from_handle(objp->get_parent());
    }
    return false;
}

// 0x0075F370 (:174)
static object* find_top_parent_object(object* objp)
{
    object* parent = World.object_from_handle(objp->get_parent());
    while (parent != nullptr)
    {
        objp = parent;
        parent = World.object_from_handle(parent->get_parent());
    }
    return objp;
}

// 0x00790300 (:187)
static bool build_spawn_node_set_parent(farray<object_squad_spawn_node*,256>& node_list, object* parent_objp)
{
    if (parent_objp != nullptr && parent_objp->get_obj_type() == OT_SQUAD_SPAWN_NODE && ((object_squad_spawn_node*)parent_objp)->is_safehouse_vip())
    {
        node_list.add((object_squad_spawn_node*)parent_objp);
    }
    // Original: dereferences parent_objp whether or not the test above found it null; its callers never pass null.
    object* child = parent_objp->child_ptr;
    while (child != nullptr)
    {
        build_spawn_node_set_parent(node_list, child);
        child = child->child_next;
        if (child == parent_objp->child_ptr)
        {
            break;
        }
    }
    return node_list.size() > 0;
}

// 0x00790380 (:209)
// LTCG: the list in EDI and the node in ESI.
static bool build_spawn_node_set(farray<object_squad_spawn_node*,256>& node_list, object_squad_spawn_node* node)
{
    node_list.clear();
    object* top = node;
    object* parent = World.object_from_handle(node->get_parent());
    if (parent != nullptr)
    {
        top = find_top_parent_object(parent);
    }
    return top != nullptr && build_spawn_node_set_parent(node_list, top);
}

// 0x0075F3A0 (:224)
static void stream_notify_spawn_squad(object_squad_spawn_node* node)
{
    if (node->get_handle() == VIP_spawn_node_handle)
    {
        node->spawn_child_object(MAP_ELEMENT_INVALID, MAP_ICON_TYPE_NORMAL);
    }
}

// 0x007903C0 (:232)
// A node with no special npc counts as restricted, and so does Jenkins while the rail activity runs.
static bool is_on_restricted_list(const object_squad_spawn_node* node)
{
    const human_info* info = get_human_info(node);
    // Deviation from rfg.exe: asks activity_vrail_is_riding_shotgun_running, where it reads the activity
    // objects directly.
    if (info == nullptr || (info->flags.jenkins && activity_vrail_is_riding_shotgun_running()))
    {
        return true;
    }
    for (int i = 0; i < Restricted_human_list.size(); i++)
    {
        if (Restricted_human_list[i] == info)
        {
            return true;
        }
    }
    return false;
}

// 0x00790430 (:251)
// LTCG: the node also in EBX. A failed roll still marks the set as decided, with no node, until it unloads.
static void stream_notify_select_spawn(object_squad_spawn_node* node)
{
    if (VIP_top_parent_handle != 0xFFFFFFFF)
    {
        return;
    }
    object* top = node;
    object* parent = World.object_from_handle(node->get_parent());
    if (parent != nullptr)
    {
        top = find_top_parent_object(parent);
    }
    if (top == nullptr)
    {
        return;
    }
    if (VIP_spawn_probability < fl_rand())
    {
        VIP_top_parent_handle = top->get_handle();
        VIP_spawn_node_handle = 0xFFFFFFFF;
        return;
    }
    farray<object_squad_spawn_node*,256> nodes;
    if (!build_spawn_node_set(nodes, node))
    {
        return;
    }
    for (int i = 0; i < nodes.size();)
    {
        if (is_on_restricted_list(nodes[i]))
        {
            nodes.remove_unsorted(i);
        }
        else
        {
            i++;
        }
    }
    if (nodes.size() <= 0)
    {
        return;
    }
    object_squad_spawn_node* chosen = nodes[nodes.size() == 1 ? 0 : int_rand_range(0, nodes.size() - 1)];
    const human_info* info = get_human_info(chosen);
    if (info != nullptr)
    {
        srm_reserve_slot(SPAWN_SLOT_HUMAN_SAFEHOUSE_VIP, RESOURCE_SLOT_USER_ID_SAFEHOUSE_VIP, info);
        VIP_top_parent_handle = top->get_handle();
        VIP_spawn_node_handle = chosen->get_handle();
        if (chosen->obj_flags.streamed)
        {
            chosen->spawn_child_object(MAP_ELEMENT_INVALID, MAP_ICON_TYPE_NORMAL);
        }
    }
}

// 0x00790570 (:318)
static void stream_notify_load(object_squad_spawn_node* node)
{
    if (!active_vip_spawn())
    {
        stream_notify_select_spawn(node);
        return;
    }
    stream_notify_spawn_squad(node);
}

// 0x00775490 (:327)
static void release_vip_reservation()
{
    if (VIP_top_parent_handle == 0xFFFFFFFF)
    {
        return;
    }
    if (VIP_spawn_node_handle != 0xFFFFFFFF)
    {
        const object_squad_spawn_node* node = World.object_from_handle<object_squad_spawn_node>(VIP_spawn_node_handle);
        if (node != nullptr && node->get_child_squad_handle() != 0xFFFFFFFF)
        {
            object_squad* squad = World.object_from_handle<object_squad>(node->get_child_squad_handle());
            if (squad != nullptr)
            {
                squad->request_despawn_all_members(NPC_DESPAWN_TYPE_LOW, nullptr, true);
            }
        }
    }
    srm_release_all_slots(SPAWN_SLOT_HUMAN_SAFEHOUSE_VIP, RESOURCE_SLOT_USER_ID_SAFEHOUSE_VIP);
    VIP_spawn_node_handle = 0xFFFFFFFF;
    VIP_top_parent_handle = 0xFFFFFFFF;
}

// 0x007905A0 (:354)
// LTCG: the node in ESI. Releases the reservation when this was the last streamed node of the set.
static void stream_notify_unload(object_squad_spawn_node* node)
{
    if (VIP_top_parent_handle == 0xFFFFFFFF || node == nullptr)
    {
        return;
    }
    if (node->get_handle() != VIP_top_parent_handle && node->get_parent() != VIP_top_parent_handle)
    {
        const object* parent = World.object_from_handle(node->get_parent());
        if (parent == nullptr || !in_vip_spawn_set(parent))
        {
            return;
        }
    }
    farray<object_squad_spawn_node*,256> nodes;
    if (!build_spawn_node_set(nodes, node))
    {
        return;
    }
    for (int i = 0; i < nodes.size(); i++)
    {
        if (nodes[i] != node && nodes[i]->obj_flags.streamed)
        {
            return;
        }
    }
    release_vip_reservation();
}

// 0x00790670 (:391)
static object_squad_spawn_node* find_nearest_vip_node()
{
    const player* pp = player_get_local_player();
    object_squad_spawn_node* best = nullptr;
    float best_dist = FLT_MAX;
    const int count = World.get_num_objects<object_squad_spawn_node>();
    for (int i = 0; i < count; i++)
    {
        object_squad_spawn_node* node = World.get_object<object_squad_spawn_node>(i);
        if (node->is_safehouse_vip())
        {
            const float dist = vec_dist_squared(pp->pos, node->pos);
            if (dist < best_dist)
            {
                best = node;
                best_dist = dist;
            }
        }
    }
    return best;
}

// 0x00775510 (:424)
void safehouse_vip_level_init_pre()
{
    Restricted_human_list.clear();
    VIP_spawn_node_handle = 0xFFFFFFFF;
    VIP_top_parent_handle = 0xFFFFFFFF;
    Spawn_ready = false;
}

// 0x00790730 (:433)
// Original: the operator's line and both timers are only set when a VIP node is streamed in at the start, so without
// one the timers stay unset until a later load.
void safehouse_vip_level_init_post()
{
    Spawn_ready = true;
    object_squad_spawn_node* node = find_nearest_vip_node();
    if (node != nullptr && node->obj_flags.streamed)
    {
        stream_notify_load(node);
        Radio_operator_line_handle = voice_line_handle_from_name("opr_amb_batch01");
        // Magic numbers: 10-20 s.
        VIP_voice_line_timer.set_rand(10000, 20000);
        Radio_operator_voice_line_timer.set_rand(10000, 20000);
    }
}

// 0x007907B0 (:459)
void safehouse_vip_do_frame()
{
    const player* pp = player_get_local_player();
    if (pp == nullptr || !safehouse_contains_player())
    {
        return;
    }
    if (Radio_operator_voice_line_timer.elapsed())
    {
        const human_voice_persona* persona = human_voice_radio_persona();
        if (persona != nullptr && Radio_operator_line_handle != INVALID_VOICE_LINE_HANDLE)
        {
            npc* op = get_radio_operator();
            if (op != nullptr && op->get_current_action_node(false) != nullptr)
            {
                // RADIO_OPERATOR_SAY_VOICE_LINE_DIST_SQR: 8 m squared.
                if (vec_xz_dist_squared(op->pos, pp->pos) < 64.0f)
                {
                    // Magic number: 1.9 m up, the operator's head.
                    const vector mouth(UP_VECTOR.x * 1.9f + op->pos.x, op->pos.y + UP_VECTOR.y * 1.9f, op->pos.z + UP_VECTOR.z * 1.9f);
                    int play_id;
                    unsigned short lipsync;
                    persona->persona_play_voice_line(Radio_operator_line_handle, mouth, AUDIOLIB_CUE_PRIORITY_DIALOGUE_NORMAL, play_id, lipsync, nullptr, false, false, true, true, nullptr);
                }
                // Magic numbers: the next try in 15-25 s.
                Radio_operator_voice_line_timer.set_rand(15000, 25000);
            }
        }
    }
    if (VIP_voice_line_timer.elapsed())
    {
        human* vip = get_vip_human();
        // Magic number: 4 m squared.
        if (vip != nullptr && vec_xz_dist_squared(vip->pos, pp->pos) < 16.0f)
        {
            const voice_line_handle line = voice_line_unlocking_choose(VLUG_GUERRILLA_VIP);
            if (line != INVALID_VOICE_LINE_HANDLE)
            {
                vip->play_voice_line(line, nullptr, nullptr, AUDIOLIB_CUE_PRIORITY_DIALOGUE_NORMAL, false, false, true, true);
            }
            VIP_voice_line_timer.set_rand(10000, 20000);
            return;
        }
        VIP_voice_line_timer.set(1000);
    }
}

// 0x007909A0 (:513)
// Called when a VIP node streams, only outside missions and once the level is ready.
void safehouse_vip_notify(object_squad_spawn_node* node, bool load)
{
    if (Mission_active_index != INVALID_MISSION_HANDLE || !Spawn_ready)
    {
        return;
    }
    if (load)
    {
        stream_notify_load(node);
    }
    else
    {
        stream_notify_unload(node);
    }
}

// 0x00775530 (:532)
void safehouse_vip_mission_start()
{
    if (VIP_top_parent_handle != 0xFFFFFFFF)
    {
        release_vip_reservation();
    }
}

// 0x007909F0 (:544)
void safehouse_vip_disable(const human_info* hinfo, bool disable_vip)
{
    if (hinfo == nullptr)
    {
        return;
    }
    if (disable_vip)
    {
        Restricted_human_list.add_unique(hinfo);
    }
    else
    {
        Restricted_human_list.remove_matching(hinfo);
    }
}

// 0x00775540 (:560)
void safehouse_vip_save(cfile* save_file)
{
    cf_write_uint32((unsigned int)Restricted_human_list.size(), save_file);
    for (int i = 0; i < Restricted_human_list.size(); i++)
    {
        cf_write_string(Restricted_human_list[i]->name, save_file);
    }
}

// 0x00790A20 (:574)
void safehouse_vip_load(cfile* load_file)
{
    Restricted_human_list.clear();
    for (int remaining = (int)cf_read_uint32(load_file); remaining > 0; remaining--)
    {
        char name[256];
        name[0] = '\0';
        cf_read_string(name, sizeof(name), load_file);
        const human_info* info = human_info_find(name);
        if (info != nullptr)
        {
            Restricted_human_list.add(info);
        }
    }
}
