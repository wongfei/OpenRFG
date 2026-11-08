// Original: volition/rfg/code/vehicle/vehicle_interaction_info.cpp
//
// vehicle_interactions.xtbl: per interaction its waypoints and the animated transitions between them, in one block of
// 0x14000 bytes. The waypoint positions are local to the vehicle or to a parent component.

#include "volition/rfg/code/vehicle/vehicle_interaction_info.h"

#include "volition/anim_lib/code/anim_lib.h"
#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/rfg/code/gamewide/game_trace.h"
#include "volition/rfg/code/geom/anim.h"
#include "volition/rfg/code/geom/character.h"
#include "volition/rfg/code/geom/character_instance.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/geom/vanim.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/rfg.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/vehicle/walker.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/vlib/code/util/string_comp.h"
#include "volition/vlib/code/util/string_pool.h"
#include "volition/vlib/code/util/string_util.h"

#include <stdio.h>
#include <string.h>

// ---- file-static globals
static const char* const Vehicle_action_tags[8] = {"Transition", "Seat Ejection", "Open Door", "Open and Extract", "Skip to Next Node", "Transition Type2", "Close Door", "Transition Closed Door"}; // 0x01336D10
static float vi_auto_connect_range_sqrd = 1.2f; // 0x016BA554
int Num_vehicle_interaction_infos; // 0x0377004C
vehicle_interaction_info* Vehicle_interaction_infos; // 0x03770050
static unsigned char* Vehicle_interaction_pool_data; // 0x03770058
static int Vehicle_interaction_data_pointer; // 0x0377005C
static char* Vehicle_interaction_string_pool_data;
static int Vehicle_interaction_data_pointer_dlc_mark;
static int Num_vehicle_interaction_infos_dlc_mark;
static string_pool Vehicle_interaction_string_pool("Vehicle interaction info strings"); // 0x0386BF58

static const int VEHICLE_INTERACTION_POOL_SIZE = 0x14000;

// Waypoint node types: 0 plain, 1 seat, then Node_Type's flag names from index 2.
static const int VI_NODE_TYPE_ENTER = 2;
static const int VI_NODE_TYPE_EXIT_ONSIDE_RIGHT = 7;
static const int VI_NODE_TYPE_EXIT_ONSIDE_LEFT = 8;
static const int VI_NODE_TYPE_EXIT = 3;
static const int VI_NODE_TYPE_EXIT_MOVING = 4;
static const int VI_NODE_TYPE_EXIT_MOVING_REVERSE = 5;
static const int VI_NODE_TYPE_EXIT_UPSIDE_DOWN = 6;

static const int VI_PARENT_CHASSIS = -4; // a waypoint on a walker's chassis

// 0x00B81ED0 (:85)
static void vehicle_interaction_parse_table(int dlc_id)
{
    xml_element* element;
    if (dlc_id != -1) {
        char filename[256];
        sprintf(filename, "dlc%02d_%s", dlc_id, "vehicle_interactions.xtbl");
        element = xml_table_parse(filename);
    }
    else {
        element = xml_table_parse("vehicle_interactions.xtbl");
    }

    // indexed by node type; 0 and 1 are not named in the table
    unsigned int node_type_crcs[11];
    node_type_crcs[0] = 0;
    node_type_crcs[1] = 0;
    node_type_crcs[2] = crc_stri("Enter Node", 0);
    node_type_crcs[3] = crc_stri("Exit Node", 0);
    node_type_crcs[4] = crc_stri("Exit Moving", 0);
    node_type_crcs[5] = crc_stri("Exit Moving Reverse", 0);
    node_type_crcs[6] = crc_stri("Exit Upside Down", 0);
    node_type_crcs[7] = crc_stri("Exit Onside Right", 0);
    node_type_crcs[8] = crc_stri("Exit Onside Left", 0);
    node_type_crcs[9] = crc_stri("Exit Dive", 0);
    node_type_crcs[10] = crc_stri("Exit Corpse", 0);

    int info_index = Num_vehicle_interaction_infos;
    for (xml_element* cur = xml_find(element, "Vehicle"); cur != NULL; cur = xml_find_next(element, cur, "Vehicle")) {
        Num_vehicle_interaction_infos++;
    }
    if (dlc_id == -1) {
        // the infos come first, with room for 3 more from the DLC
        Vehicle_interaction_data_pointer = (Num_vehicle_interaction_infos + 3) * sizeof(vehicle_interaction_info);
        info_index = 0;
        Vehicle_interaction_infos = (vehicle_interaction_info*)Vehicle_interaction_pool_data;
        // Original quirks kept here and below: out of room returns without closing the table.
        if (Vehicle_interaction_data_pointer >= VEHICLE_INTERACTION_POOL_SIZE) {
            return;
        }
    }

    for (xml_element* vehicle_element = xml_find(element, "Vehicle"); vehicle_element != NULL; vehicle_element = xml_find_next(element, vehicle_element, "Vehicle")) {
        vehicle_interaction_info* info = &Vehicle_interaction_infos[info_index++];
        info->name = Vehicle_interaction_string_pool.add_unique(xml_table_get_req_string_ref(vehicle_element, "Name"));
        info->is_initialized = false;
        info->animation_set = anim_set_find(xml_table_get_req_string_ref(vehicle_element, "Anim_Set"));
        xml_element* flags = xml_table_find_req(vehicle_element, "Vehicle_Interaction_Flags");
        info->player_prompt_only_at_entry = false;
        for (xml_element* flag = xml_find(flags, "Flag"); flag != NULL; flag = xml_find_next(flags, flag, "Flag")) {
            if (strcmp(flag->text, "Player_prompt_only_at_entry") == 0) {
                info->player_prompt_only_at_entry = true;
            }
        }

        xml_element* waypoints = xml_table_find_req(vehicle_element, "Waypoints");
        info->num_waypoints = 0;
        for (xml_element* cur = xml_find(waypoints, "Waypoint"); cur != NULL; cur = xml_find_next(waypoints, cur, "Waypoint")) {
            info->num_waypoints++;
        }
        info->waypoints = (vehicle_interaction_waypoint*)(Vehicle_interaction_pool_data + Vehicle_interaction_data_pointer);
        Vehicle_interaction_data_pointer += info->num_waypoints * sizeof(vehicle_interaction_waypoint);
        if (Vehicle_interaction_data_pointer >= VEHICLE_INTERACTION_POOL_SIZE) {
            return;
        }
        info->num_seats = 0;
        memset(info->waypoints, 0, info->num_waypoints * sizeof(vehicle_interaction_waypoint));
        vehicle_interaction_waypoint* waypoint = info->waypoints;
        for (xml_element* cur = xml_find(waypoints, "Waypoint"); cur != NULL; cur = xml_find_next(waypoints, cur, "Waypoint")) {
            waypoint->name = Vehicle_interaction_string_pool.add_unique(xml_table_get_req_string_ref(cur, "Name"));
            xml_table_get_vector(waypoint->local_pos, xml_table_find_req(cur, "Pos"));
            xml_table_get_req_float(waypoint->local_heading, cur, "Heading");
            waypoint->parent_component_name = NULL;
            waypoint->parent_component_index = -1;
            const char* parent = xml_table_get_opt_string_ref(cur, "Parent_Component");
            if (parent != NULL) {
                waypoint->parent_component_name = Vehicle_interaction_string_pool.add_unique(parent);
            }
            waypoint->node_type = 0;
            waypoint->seat_index = VSI_INVALID;
            const char* seat = xml_table_get_opt_string_ref(cur, "Seat");
            if (seat != NULL) {
                waypoint->seat_index = vehicle_seat_name_to_index(seat);
                waypoint->node_type = waypoint->seat_index != VSI_INVALID;
                if ((int)waypoint->seat_index + 1 >= info->num_seats) {
                    info->num_seats = (int)waypoint->seat_index + 1;
                }
            }
            xml_element* node_type = xml_table_find_req(cur, "Node_Type");
            for (xml_element* flag = xml_find(node_type, "Flag"); flag != NULL; flag = xml_find_next(node_type, flag, "Flag")) {
                const unsigned int crc = crc_stri(flag->text, 0);
                for (int type = VI_NODE_TYPE_ENTER; type < 11; type++) {
                    if (node_type_crcs[type] == crc) {
                        waypoint->node_type = type;
                        break;
                    }
                }
            }
            xml_element* attributes = xml_table_find_req(cur, "Attributes");
            for (xml_element* flag = xml_find(attributes, "Flag"); flag != NULL; flag = xml_find_next(attributes, flag, "Flag")) {
                if (strcmp(flag->text, "Short Range") == 0) {
                    waypoint->short_range = 1;
                }
                if (strcmp(flag->text, "Restrict Vertical Access") == 0) {
                    waypoint->restrict_vertical_access = 1;
                }
                if (strcmp(flag->text, "Last Choice") == 0) {
                    waypoint->last_choice = 1;
                }
                if (strcmp(flag->text, "Eject on Explosion") == 0) {
                    waypoint->eject_on_explosion = 1;
                }
            }
            waypoint++;
        }

        xml_element* transitions = xml_table_find_req(vehicle_element, "Waypoint_transitions");
        info->num_waypoint_transitions = 0;
        for (xml_element* cur = xml_find(transitions, "Waypoint_transition"); cur != NULL; cur = xml_find_next(transitions, cur, "Waypoint_transition")) {
            info->num_waypoint_transitions++;
        }
        info->transitions = (vehicle_interaction_waypoint_transition*)(Vehicle_interaction_pool_data + Vehicle_interaction_data_pointer);
        Vehicle_interaction_data_pointer += info->num_waypoint_transitions * sizeof(vehicle_interaction_waypoint_transition);
        if (Vehicle_interaction_data_pointer >= VEHICLE_INTERACTION_POOL_SIZE) {
            return;
        }
        memset(info->transitions, 0, info->num_waypoint_transitions * sizeof(vehicle_interaction_waypoint_transition));
        vehicle_interaction_waypoint_transition* transition = info->transitions;
        for (xml_element* cur = xml_find(transitions, "Waypoint_transition"); cur != NULL; cur = xml_find_next(transitions, cur, "Waypoint_transition")) {
            transition->index = -1;
            transition->destination_index = -1;
            const char* name = xml_table_get_req_string_ref(cur, "Name");
            for (int i = 0; i < info->num_waypoints; i++) {
                if (v_stricmp(name, info->waypoints[i].name) == 0) {
                    transition->index = (signed char)i;
                    break;
                }
            }
            name = xml_table_get_req_string_ref(cur, "Destination");
            for (int i = 0; i < info->num_waypoints; i++) {
                if (v_stricmp(name, info->waypoints[i].name) == 0) {
                    transition->destination_index = (signed char)i;
                    break;
                }
            }
            name = xml_table_get_opt_string_ref(cur, "Door");
            transition->component_obstacle_name = NULL;
            transition->component_obstacle_index = -1;
            if (name != NULL) {
                transition->component_obstacle_name = Vehicle_interaction_string_pool.add_unique(name);
            }
            name = xml_table_get_opt_string_ref(cur, "Door_open");
            if (name != NULL) {
                audiolib_cue_get_id(name, &transition->component_open_foley);
            }
            else {
                transition->component_open_foley = -1;
            }
            name = xml_table_get_opt_string_ref(cur, "Door_close");
            if (name != NULL) {
                audiolib_cue_get_id(name, &transition->component_close_foley);
            }
            else {
                transition->component_close_foley = -1;
            }
            xml_element* interactions = xml_table_find_req(cur, "Interactions");
            for (xml_element* action = xml_find(interactions, "Interaction"); action != NULL; action = xml_find_next(interactions, action, "Interaction")) {
                transition->num_interactions++;
            }
            transition->actions = (vehicle_transition_action*)(Vehicle_interaction_pool_data + Vehicle_interaction_data_pointer);
            Vehicle_interaction_data_pointer += transition->num_interactions * sizeof(vehicle_transition_action);
            if (Vehicle_interaction_data_pointer >= VEHICLE_INTERACTION_POOL_SIZE) {
                return;
            }
            memset(transition->actions, 0, transition->num_interactions * sizeof(vehicle_transition_action));
            vehicle_transition_action* out = transition->actions;
            for (xml_element* action = xml_find(interactions, "Interaction"); action != NULL; action = xml_find_next(interactions, action, "Interaction")) {
                const char* tag = xml_table_get_req_string_ref(action, "Action");
                for (int i = 0; i < 8; i++) {
                    if (v_stricmp(tag, Vehicle_action_tags[i]) == 0) {
                        out->action = (vehicle_interaction_actions)i;
                        break;
                    }
                }
                out->anim_action = vanim_lookup_action(xml_table_get_req_string_ref(action, "Anim_Action"));
                out->anim_state = vanim_lookup_state(xml_table_get_req_string_ref(action, "Anim_State"));
                out++;
            }
            transition++;
        }
    }
    xml_table_close(element);
}

// 0x00B827D0 (:367)
void vehicle_interaction_dlc_init(int dlc_id)
{
    Vehicle_interaction_data_pointer_dlc_mark = Vehicle_interaction_data_pointer;
    Num_vehicle_interaction_infos_dlc_mark = Num_vehicle_interaction_infos;
    Vehicle_interaction_string_pool.set_thread_ownership();
    Vehicle_interaction_string_pool.mark();
    vehicle_interaction_parse_table(dlc_id);
}

// 0x00B688A0 (:380)
void vehicle_interaction_dlc_shutdown()
{
    Vehicle_interaction_string_pool.set_thread_ownership();
    Vehicle_interaction_string_pool.restore_to_mark(0);
    Num_vehicle_interaction_infos = Num_vehicle_interaction_infos_dlc_mark;
    Vehicle_interaction_data_pointer = Vehicle_interaction_data_pointer_dlc_mark;
}

// 0x00B82800 (:388)
void vehicle_interaction_system_init()
{
    Vehicle_interaction_pool_data = (unsigned char*)Permanent_mempool.alloc(VEHICLE_INTERACTION_POOL_SIZE, 0);
    Vehicle_interaction_string_pool_data = (char*)Permanent_mempool.alloc(0x2000, 0);
    if (Vehicle_interaction_string_pool_data != NULL) {
        Vehicle_interaction_string_pool.init(Vehicle_interaction_string_pool_data, 0x2000, "Vehicle interaction info strings", 0, '\0');
        vehicle_interaction_parse_table(-1);
    }
}

// 0x00B688D0 (:405): empty in rfg.exe.
bool vehicle_interaction_info::find_waypoint_path(int waypoint_id, int seat_id, int* path) const
{
    return false;
}

// 0x00B688E0 (:476)
bool vehicle_interaction_info::is_auto_connected_transition(int start_waypoint_index, int destination_waypoint_index) const
{
    return waypoints[destination_waypoint_index].node_type == VI_NODE_TYPE_ENTER && waypoints[start_waypoint_index].node_type == 0 && waypoints[start_waypoint_index].short_range != 0;
}

// 0x00B91410 (:483)
bool vehicle_interaction_info::is_auto_connected_transition_active(const vehicle* vp, int start_waypoint_index, int destination_waypoint_index, vector* precalc_dest_pos) const
{
    if (!is_auto_connected_transition(start_waypoint_index, destination_waypoint_index)) {
        return false;
    }
    vector start_pos;
    get_waypoint_world_pos_orient(vp, start_waypoint_index, start_pos, NULL, UP_VECTOR);
    vector dest_pos;
    if (precalc_dest_pos == NULL) {
        get_waypoint_world_pos_orient(vp, destination_waypoint_index, dest_pos, NULL, UP_VECTOR);
    }
    else {
        dest_pos = *precalc_dest_pos;
    }
    return vec_dist_squared(start_pos, dest_pos) < vi_auto_connect_range_sqrd;
}

// 0x00B82860 (:499)
void vehicle_interaction_info::get_waypoint_world_pos_orient(const vehicle* vp, int waypoint_id, vector& pos, matrix* orient, vector sub_uvec) const
{
    // Original bug kept: "&&" makes this unreachable, and a bad index reads outside the waypoints.
    if (waypoint_id < 0 && num_waypoints <= waypoint_id) {
        pos = vp->pos;
        if (orient != NULL) {
            *orient = vp->orient;
        }
        return;
    }
    const vehicle_interaction_waypoint& waypoint = waypoints[waypoint_id];
    pos = waypoint.local_pos;
    vector parent_pos = vp->pos;
    matrix parent_orient(vp->orient);
    if (vp->get_sub_type() == OT_SUB_VEHICLE_WALKER) {
        const walker* wp = (const walker*)vp;
        if (waypoint.parent_component_index != VI_PARENT_CHASSIS) {
            parent_pos = wp->chassis_pos;
            parent_orient = wp->chassis_orient;
        }
    }
    const bool loaded = vp->rdd.is_loaded();
    if (loaded && waypoint.parent_component_index >= 0) {
        const vehicle_component& component = vp->rdd.get_data()->m_components[waypoint.parent_component_index];
        parent_pos = component.get_world_space_position();
        parent_orient = component.get_world_space_rotation();
    }
    vec_change_to_global_coords(pos, parent_pos, parent_orient);
    if (orient != NULL) {
        *orient = parent_orient;
        if (waypoint.local_heading != 0.0f) {
            orient->rotate_around_local_y((waypoint.local_heading * 3.1415927f) / 180.0f);
        }
        if (!loaded) {
            return;
        }
        if (waypoint.parent_component_index == VI_PARENT_CHASSIS) {
            orient->set(0.0f, 0.0f, orient->extract_heading());
        }
    }
    if (!loaded) {
        return;
    }

    // The vehicle is tipped: move the waypoint so a human reaches it upright (1.9 m, about the height of a human).
    bool change_uvec = false;
    if ((waypoint.node_type == VI_NODE_TYPE_EXIT && waypoint.parent_component_index == VI_PARENT_CHASSIS) || sub_uvec.y < 0.999f) {
        vector offset((vp->orient.uvec.x - sub_uvec.x) * 1.9f, (vp->orient.uvec.y - sub_uvec.y) * 1.9f, (vp->orient.uvec.z - sub_uvec.z) * 1.9f);
        if (offset.x * offset.x + offset.y * offset.y + offset.z * offset.z > 0.01f) {
            vector to_waypoint(pos.x - vp->pos.x, pos.y - vp->pos.y, pos.z - vp->pos.z);
            const vector to_waypoint_dir = to_waypoint.copy_normalize_safe();
            const vector offset_dir = offset.copy_normalize();
            const float dot = offset_dir.y * to_waypoint_dir.y + offset_dir.x * to_waypoint_dir.x + offset_dir.z * to_waypoint_dir.z;
            if (dot < 0.0f) {
                offset.x -= to_waypoint_dir.x * dot;
                offset.y -= to_waypoint_dir.y * dot;
                offset.z -= to_waypoint_dir.z * dot;
            }
            pos.x += offset.x;
            pos.y += offset.y;
            pos.z += offset.z;
            change_uvec = true;
        }
    }
    else if (waypoint.node_type == VI_NODE_TYPE_EXIT_UPSIDE_DOWN) {
        const vector& rvec = vp->orient.rvec;
        const float side = fl_sign(rvec.y * (pos.y - vp->pos.y) + rvec.x * (pos.x - vp->pos.x) + rvec.z * (pos.z - vp->pos.z));
        if (side * (rvec.z * sub_uvec.z + rvec.y * sub_uvec.y + rvec.x * sub_uvec.x) < 0.0f) {
            pos.x += (-vp->orient.uvec.x - sub_uvec.x) * 1.9f;
            pos.y += (-vp->orient.uvec.y - sub_uvec.y) * 1.9f;
            pos.z += (-vp->orient.uvec.z - sub_uvec.z) * 1.9f;
            change_uvec = true;
        }
    }
    if (orient != NULL && change_uvec) {
        MAT_CHANGE_UVEC(*orient, sub_uvec);
    }
}

// 0x00B68920 (:587)
const vehicle_interaction_waypoint_transition* const vehicle_interaction_info::get_transition(signed char& idx, int start_waypoint_index, int destination_waypoint_index) const
{
    if (idx != -1) {
        return &transitions[idx];
    }
    for (int i = 0; i < num_waypoint_transitions; i++) {
        if (transitions[i].index == start_waypoint_index && transitions[i].destination_index == destination_waypoint_index) {
            idx = (signed char)i;
            return &transitions[i];
        }
    }
    idx = -1;
    return NULL;
}

// 0x00B82FE0 (:607)
void vi_info_get_animation_space(vanim_play_parameters& vpp, vehicle* vp, vi_human_manager* hm, anim* animp)
{
    if (animp == NULL) {
        // Original bug kept: without an object of type human this reads through null.
        human* hp = hm->handle != 0xFFFFFFFF ? World.object_from_handle<human>(hm->handle) : NULL;
        v_anim* action = vanim_get_action(hp->char_inst->vp, 0x400);
        if (action == NULL || anim_get_anim_from_handle(action->anim_handle) == NULL) {
            return;
        }
    }
    const vehicle_interaction_info* info = vp->info->vi_info;
    info->get_waypoint_world_pos_orient(vp, hm->animate_from_wpi, vpp.ideal_start_pos, &vpp.ideal_start_orient, UP_VECTOR);
    info->get_waypoint_world_pos_orient(vp, hm->animate_dest_wpi, vpp.real_end_pos, &vpp.real_end_orient, UP_VECTOR);
    if (hm->animate_from_wpi != hm->animate_dest_wpi && hm->cur_path_idx + 2 < 6) {
        const int next = hm->cur_path_idx + 2;
        if (info->is_auto_connected_transition(hm->path_nodes[hm->cur_path_idx + 1], hm->path_nodes[next])) {
            vector unused_pos;
            info->get_waypoint_world_pos_orient(vp, hm->path_nodes[next], unused_pos, &vpp.real_end_orient, UP_VECTOR);
        }
    }
    const int dest_type = info->waypoints[hm->animate_dest_wpi].node_type;
    if (dest_type == VI_NODE_TYPE_EXIT_ONSIDE_LEFT) {
        MAT_CHANGE_UVEC(vpp.ideal_start_orient, vp->orient.rvec);
    }
    else if (dest_type == VI_NODE_TYPE_EXIT_ONSIDE_RIGHT) {
        MAT_CHANGE_UVEC(vpp.ideal_start_orient, vector(-vp->orient.rvec.x, -vp->orient.rvec.y, -vp->orient.rvec.z));
    }
    if (info->waypoints[hm->animate_from_wpi].eject_on_explosion != 0) {
        vpp.ideal_start_orient = vp->orient;
    }
}

// The game trace (game_trace.h, not in rfg.exe): an anim's file name, marked when its data is not paged in (anim_lib then
// refuses it). A ring of buffers, so one trace line can name several.
const char* vi_trace_anim(int anim_handle)
{
    static char buffers[4][96];
    static int next;
    char* text = buffers[next];
    next = (next + 1) % 4;
    const anim* animp = anim_get_anim_from_handle(anim_handle);
    if (animp == NULL) {
        snprintf(text, sizeof(buffers[0]), "none");
    }
    else {
        snprintf(text, sizeof(buffers[0]), "%s%s", animp->filename, animlib_is_paged_in(animp->animlib_handle) ? "" : " NOT PAGED IN");
    }
    return text;
}

// 0x00B83220 (:642)
bool vehicle_interaction_waypoint_transition::play_animation(vehicle* vp, vi_human_manager* hm, vehicle_interaction_actions action) const
{
    const vehicle_interaction_info* info = vp->info->vi_info;
    human* hp = hm->handle != 0xFFFFFFFF ? World.object_from_handle<human>(hm->handle) : NULL;
    int action_index = 0;
    for (;; action_index++) {
        if (action_index >= num_interactions) {
            return false;
        }
        if (actions[action_index].action == action) {
            break;
        }
    }
    if (action == VIA_SKIP_TO_NEXT_NODE) {
        return true;
    }

    v_anim* current = vanim_get_action(hp->char_inst->vp, 0x400);
    if (current != NULL) {
        vanim_ramp_out_action(hp->char_inst->vp, 0x400, 3);
        current->alias = ANIM_ALIAS_NONE;
    }
    const int anim_set_index = (int)(info->animation_set - Anim_sets);
    int anim_handle = vanim_find_anim_handle(anim_set_index, 0, actions[action_index].anim_action);
    hp->char_inst->vp->hp = hp;
    index_alias action_handle(anim_handle, ANIM_ALIAS_VEHICLE_TRANSITION);
    vanim_play_custom_action(hp->char_inst->vp, action_handle, 0x40, 1.0f, 1.0f);
    anim* animp = anim_get_anim_from_handle(anim_handle);
    if (animp == NULL) {
        GAME_TRACE("vehicle", "anim missing: human=0x%X vehicle=%s %s", hm->handle, vp->info->name, Vehicle_action_tags[action]);
        return false;
    }

    // The animation runs between the nearest nodes that do not restrict vertical access.
    int from = hm->cur_path_idx;
    int dest = from + 1;
    while (from > 0) {
        const vehicle_interaction_waypoint& waypoint = info->waypoints[hm->path_nodes[from]];
        if (waypoint.restrict_vertical_access == 0 || waypoint.node_type != 0) {
            break;
        }
        from--;
    }
    while (dest < 5) {
        const vehicle_interaction_waypoint& waypoint = info->waypoints[hm->path_nodes[dest]];
        if (waypoint.restrict_vertical_access == 0 || waypoint.node_type != 0) {
            break;
        }
        dest++;
    }
    hm->animate_from_wpi = hm->path_nodes[from];
    hm->animate_dest_wpi = hm->path_nodes[dest];
    if (action == VIA_OPEN_DOOR || action == VIA_CLOSE_DOOR) {
        hm->animate_dest_wpi = hm->animate_from_wpi;
        hp->char_inst->vp->flags.adjusting_orient = 1;
    }

    vanim_play_parameters params;
    memset(&params, 0, sizeof(params));
    params.should_initialize = true;
    vi_info_get_animation_space(params, vp, hm, animp);
    const int trigger = (action == VIA_OPEN_DOOR || action == VIA_CLOSE_DOOR) ? Trigger_door_open_start : Trigger_relative_space;
    const int trigger_time = animlib_get_trigger_time(trigger, animp->animlib_handle, 0);
    // animation time is in 1/4800 s
    float err_corr_time = (float)trigger_time / 4800.0f;
    if (err_corr_time - 0.1f < 0.0f) {
        err_corr_time = 0.1f;
    }
    if (info->waypoints[hm->animate_from_wpi].eject_on_explosion != 0) {
        err_corr_time = 0.7f;
    }
    params.start_err_corr_length = err_corr_time;
    params.end_err_corr_begin_time = err_corr_time;
    if (2 < info->waypoints[hm->animate_dest_wpi].node_type) {
        params.end_err_corr_begin_time = err_corr_time * 0.3f;
    }
    const int dest_type = info->waypoints[hm->animate_dest_wpi].node_type;
    if (dest_type == VI_NODE_TYPE_EXIT_ONSIDE_LEFT || dest_type == VI_NODE_TYPE_EXIT_ONSIDE_RIGHT) {
        params.start_err_corr_length = 0.0f;
        v_anim* played = vanim_get_action(hp->char_inst->vp, 0x400);
        if (played != NULL) {
            played->blend = 1.0f;
        }
    }
    vanim_set_action_relative_space(hp->char_inst->vp, 0x400, &params);
    if (dest_type == VI_NODE_TYPE_EXIT) {
        vector desired(params.ideal_start_orient.uvec.x * 0.7f + UP_VECTOR.x * 0.3f, params.ideal_start_orient.uvec.y * 0.7f + UP_VECTOR.y * 0.3f, params.ideal_start_orient.uvec.z * 0.7f + UP_VECTOR.z * 0.3f);
        vanim_interpolate_uvec(hp->char_inst->vp, hp, desired, params.end_err_corr_begin_time - 0.06666667f);
    }
    else if (dest_type == VI_NODE_TYPE_EXIT_MOVING || dest_type == VI_NODE_TYPE_EXIT_MOVING_REVERSE) {
        const float uvec_time = vanim_get_anim_length(hp->char_inst->vp, 0x400) + fl_acos(hp->orient.uvec.y) * 0.3f;
        vanim_interpolate_uvec(hp->char_inst->vp, hp, UP_VECTOR, uvec_time);
    }

    hm->ride_state_handle = vanim_find_anim_handle(anim_set_index, 0, actions[action_index].anim_state);
    GAME_TRACE("vehicle", "anim start: human=0x%X vehicle=%s %s %s -> %s: %s, then state %s", hm->handle, vp->info->name, Vehicle_action_tags[action], info->waypoints[hm->animate_from_wpi].name, info->waypoints[hm->animate_dest_wpi].name, vi_trace_anim(anim_handle), vi_trace_anim(hm->ride_state_handle));
    index_alias state_handle(hm->ride_state_handle, ANIM_ALIAS_NONE);
    index_alias no_state(-1, (vanim_aliases)-1);
    vanim_change_custom_state(hp->char_inst->vp, state_handle, no_state, 0.6f, false);
    if (hp->cur_state != HS_VEHICLE_DRIVING && hp->cur_state != HS_VEHICLE_SITTING) {
        human_override_default_animation_state(hp, true);
    }
    return true;
}

// 0x00B83770 (:808)
void vehicle_interaction_info::get_closest_seat_for_ragdoll(vehicle_seat_index& seat_idx, vehicle* vp, vector& pos, float range_squared) const
{
    seat_idx = VSI_INVALID;
    int closest = -1;
    for (int i = 0; i < num_waypoints; i++) {
        if (waypoints[i].seat_index != VSI_INVALID && vp->vii.vvm.used_waypoints[i].block_type == 2) {
            vector waypoint_pos;
            get_waypoint_world_pos_orient(vp, i, waypoint_pos, NULL, UP_VECTOR);
            const float dist = vec_dist_squared(waypoint_pos, pos);
            if (dist < range_squared) {
                seat_idx = waypoints[i].seat_index;
                range_squared = dist;
                closest = i;
            }
        }
    }
    if (closest >= 0 && waypoints[closest].last_choice != 0) {
        seat_idx = VSI_INVALID;
    }
}
