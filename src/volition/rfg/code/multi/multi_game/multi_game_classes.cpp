// Original: volition/rfg/code/multi/multi_game/multi_game_classes.cpp
//
// The multiplayer player classes (mp_classes.xtbl): hit points, knockdowns, speed, the default inventory and the
// class weapon. "Engineer" is the default class; a mode lists the classes it offers (Multi_game_modes). A client asks
// the server for a class, the server stores it in the player's game data and tells the other clients.
// Part of the multi unity TU (rfg_multi_unity.obj, every multi/ .cpp in path order): the file statics are shared
// by the whole tree, and a shared one is placed in its first user (tools/hdrgen/specs/mp0_stubs.py).

#include "volition/rfg/code/multi/multi_game/multi_game_classes.h"
#include "volition/rfg/code/multi/multi_game/modes/multi_game_mode_common.h"
#include "volition/rfg/code/multi/multi_game/multi_game_data.h"
#include "volition/rfg/code/multi/multi_game/multi_game_settings.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_io.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_packet_types.h"
#include "volition/rfg/code/multi/multi_core/include/multi_session.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/inventory_item.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/rfg/code/rfg.h"
#include "volition/vlib/code/util/string_util.h"
#include "volition/vlib/code/util/string_comp.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "keen/base/trace.hpp"

#include <string.h>

// ---- file-static globals
// multi_pool<mp_player_class_info,4> Class_pool (0x01692B48) is used here too; placed in multi_game.cpp.
// multi_game_mode_info Multi_game_modes[8] (0x02FE73C8) is used here too; placed in multi_game_mode_common.cpp.
// bool Server (0x02FEB590) is used here too; placed in multi_core_session_generic.cpp.
static int Num_classes; // 0x02FEB690
// mp_player_class_info* Default_class_info (0x02FEB694) is used here too; placed in multi_game.cpp.
// mp_game_data_wrapper* Mp_game_data (0x02FEB698) is used here too; placed in multi_connection.cpp.

// Magic numbers: at most 4 classes; class names hold 32 characters; packets are built in a 1,200 byte buffer.
static const int Max_classes = 4;
static const int Max_class_name = 32;
// Packet_buffer_size (1200) is defined in multi_game.cpp, earlier in the unity TU.

// 0x0098C420
mp_player_class_info::mp_player_class_info()
{
}

// ---- internal functions

// 0x00997520 (:38). LTCG: name in EDI.
static mp_player_class_info* multi_game_classes_lookup_class(const char* name)
{
    Class_pool.thread_access.enter();
    for (mp_player_class_info* info = Class_pool.get_first(0); info != nullptr; info = Class_pool.get_next(info))
    {
        if (v_stricmp(info->name, name) == 0)
        {
            Class_pool.thread_access.leave();
            return info;
        }
    }
    Class_pool.thread_access.leave();
    return nullptr;
}

// 0x009AE9C0 (:54)
static void multi_game_classes_parse_mp_classes_table()
{
    xml_element* root = xml_table_parse("mp_classes.xtbl");
    xml_element* entry = xml_table_find_opt(root, "mp_classes_list");
    for (Num_classes = 0; entry != nullptr && Num_classes < Max_classes; Num_classes++)
    {
        mp_player_class_info* info = Class_pool.alloc(0);
        xml_table_get_req_string(info->name, sizeof(info->name), entry, "Name");
        xml_table_get_req_string(info->display_name, sizeof(info->display_name), entry, "display_name");
        xml_table_get_req_int(info->max_hit_points, entry, "max_hit_points");
        xml_table_get_req_int(info->max_knockdown_hits, entry, "knockdown_hits");
        xml_table_get_req_float(info->max_speed, entry, "max_speed");
        xml_table_get_req_float(info->health_regen_factor, entry, "health_regen_factor");
        xml_element* inventory = xml_table_find_req(entry, "Inventory");
        for (xml_element* item = xml_find(inventory, "Item"); item != nullptr; item = xml_find_next(inventory, item, "Item"))
        {
            inv_item_info* i_info = inv_item_get_info(xml_table_get_req_string_ref(item, "Item_Name"));
            if (i_info == nullptr)
            {
                keen::debug::nativePrintArgs("Invalid item name.");
                continue;
            }
            int count = 1;
            xml_table_get_req_int(count, item, "Item_Count");
            int slot = -1;
            xml_table_get_req_int(slot, item, "Item_Slot");
            human_info_inventory_info inv;
            inv.i_info = i_info;
            inv.count = count;
            inv.slot = slot;
            info->default_inventory.add(inv);
        }
        info->default_equip_item = inv_item_get_info(xml_table_get_req_string_ref(entry, "Equip_Item_Name"));
        const char* last_equip = xml_table_get_opt_string_ref(entry, "Last_Equip_Item_Name");
        info->default_last_equip_item = last_equip != nullptr ? inv_item_get_info(last_equip) : nullptr;
        info->class_unique_weapon = inv_item_get_info(xml_table_get_req_string_ref(entry, "class_unique_weapon"));
        xml_element* wounded = xml_table_find_opt(entry, "see_wounded_teammates_on_minimap");
        info->see_wounded_teammates_on_minimap = wounded != nullptr;
        if (wounded != nullptr)
        {
            xml_table_get_req_float(info->light_damage_threshold, wounded, "light_damage_threshold");
            xml_table_get_req_float(info->medium_damage_threshold, wounded, "medium_damage_threshold");
            xml_table_get_req_float(info->heavy_damage_threshold, wounded, "heavy_damage_threshold");
        }
        xml_table_get_req_bool(info->see_enemies_on_minimap, entry, "see_enemies_on_minimap");
        xml_table_get_req_float(info->headshot_damage_multiplier, entry, "headshot_damage_multiplier");
        entry = xml_table_find_next(root, entry, "mp_classes_list");
    }
    xml_table_close(root);
}

// 0x009BCE10 (:143): the player's game data handle and the class name to every game session client. LTCG:
// changed_player in ECX, new_class_name in EDX.
static void multi_game_classes_send_class_change(const multi_connection* changed_player, const char* new_class_name)
{
    unsigned char packet[Packet_buffer_size];
    packet[0] = PT_CLASS_CHANGE;
    unsigned short size = packet_header_size(PT_CLASS_CHANGE);
    write_u32_be(packet, size, changed_player->game_data);
    const unsigned short name_length = (unsigned short)strlen(new_class_name);
    memcpy(packet + size, new_class_name, name_length);
    size = (unsigned short)(size + name_length);
    packet[size++] = '\0';
    packet_set_size(packet, size);
    multi_session* session = multi_session_get_by_type(ST_GAME);
    multi_connection* first = session->get_connection_list();
    multi_connection* cp = first;
    while (cp != nullptr)
    {
        multi_session* game_session = multi_session_get_by_type(ST_GAME);
        multi_connection* host = game_session != nullptr ? game_session->get_host_connection() : nullptr;
        if (cp != host)
        {
            multi_io_send_reliable(cp, packet, size, nullptr, nullptr, nullptr);
        }
        cp = cp->next;
        if (cp == first)
        {
            break;
        }
    }
}

// 0x00951160 (:169): any sledgehammer counts as the class's unique sledgehammer
bool mp_player_class_info::is_unique_weapon_info(const inv_item_info* inv_info) const
{
    if (inv_info == nullptr)
    {
        return false;
    }
    if (inv_info->w_info == nullptr || inv_info->w_info->weapon_class != WEAPON_CLASS_SLEDGEHAMMER)
    {
        return inv_info == class_unique_weapon;
    }
    return class_unique_weapon != nullptr && class_unique_weapon->w_info != nullptr && class_unique_weapon->w_info->weapon_class == WEAPON_CLASS_SLEDGEHAMMER;
}

// 0x009DD370 (:193)
void multi_game_classes_system_init()
{
    Class_pool.init(MP_allocators_mempool);
    multi_game_classes_parse_mp_classes_table();
    Default_class_info = multi_game_classes_lookup_class("Engineer");
    if (Default_class_info == nullptr)
    {
        keen::debug::nativePrintArgs("Unable to get Default MP class");
    }
}

// 0x0097ADC0 (:206): every player has the default class (the local player is looked up and not used)
const char* multi_game_classes_get_current_class_name()
{
    player_get_local_player();
    return Default_class_info->name;
}

// 0x00997590 (:217)
const char* multi_game_classes_get_desired_class_name()
{
    player* pp = player_get_local_player();
    if (pp == nullptr)
    {
        return Default_class_info->name;
    }
    const multi_game_data* data = multi_game_data_get(pp->mp_data, false, false);
    if (data == nullptr)
    {
        return Default_class_info->name;
    }
    const mp_player_class_info* info = multi_game_classes_lookup_class(data->desired_class);
    if (info == nullptr)
    {
        info = Default_class_info;
    }
    return info->name;
}

// 0x009DD3C0 (:239): the class after the desired one in the mode's list (wrapping); the server takes it at once
void multi_game_classes_cycle_to_next_available_class()
{
    const char* desired = multi_game_classes_get_desired_class_name();
    if (multi_game_classes_lookup_class(desired) == nullptr)
    {
        return;
    }
    const multi_game_mode_info* mode_info = &Multi_game_modes[Mp_server_options.get_mode()];
    if (mode_info == nullptr)
    {
        return;
    }
    int index = 0;
    while (index < mode_info->num_classes && strcmp(desired, mode_info->classes[index]) != 0)
    {
        index++;
    }
    index++;
    if (index >= mode_info->num_classes)
    {
        index = 0;
    }
    const mp_player_class_info* next = multi_game_classes_lookup_class(mode_info->classes[index]);
    if (next == nullptr)
    {
        next = Default_class_info;
        if (next == nullptr)
        {
            return;
        }
    }
    if (Multiplayer && Server)
    {
        multi_connection* host = multi_session_game_host_connection();
        if (host != nullptr)
        {
            multi_game_data_set_desired_class(host->game_data, next->name);
            multi_game_classes_send_class_change(host, next->name);
        }
    }
    else
    {
        multi_game_classes_send_class_request(next->name);
    }
}

// 0x009511B0 (:278)
const mp_player_class_info* multi_game_classes_get_default_class()
{
    return Default_class_info;
}

// 0x009BCF40 (:286)
void multi_game_classes_send_class_request(const char* class_name)
{
    unsigned char packet[Packet_buffer_size];
    packet[0] = PT_REQUEST_CLASS_CHANGE;
    unsigned short size = packet_header_size(PT_REQUEST_CLASS_CHANGE);
    const unsigned short name_length = (unsigned short)strlen(class_name);
    memcpy(packet + size, class_name, name_length);
    size = (unsigned short)(size + name_length);
    packet[size++] = '\0';
    packet_set_size(packet, size);
    multi_session* session = multi_session_get_by_type(ST_GAME);
    multi_io_send_reliable(session != nullptr ? session->get_host_connection() : nullptr, packet, size, nullptr, nullptr, nullptr);
}

// 0x009BD000 (:301): the server takes any class name it is asked for
void multi_game_classes_process_class_request(unsigned char* packet_data, const sender& from)
{
    char class_name[Max_class_name];
    if (strlen((const char*)packet_data) < Max_class_name)
    {
        strcpy(class_name, (const char*)packet_data);
    }
    else
    {
        strncpy_safe(class_name, (const char*)packet_data, Max_class_name);
    }
    if (from.connection != nullptr)
    {
        multi_game_data_set_desired_class(from.connection->game_data, class_name);
        multi_game_classes_send_class_change(from.connection, class_name);
    }
}

// 0x0097ADD0 (:322)
void multi_game_classes_process_class_change(unsigned char* packet_data, const sender& from)
{
    int offset = 0;
    const unsigned int game_data_handle = read_u32_be(packet_data, offset);
    const char* received = (const char*)packet_data + offset;
    char class_name[Max_class_name];
    if (strlen(received) < Max_class_name)
    {
        strcpy(class_name, received);
    }
    else
    {
        strncpy_safe(class_name, received, Max_class_name);
    }
    if (game_data_handle != 0xFFFFFFFF)
    {
        strcpy(game_data_of(game_data_handle)->desired_class, class_name);
    }
}
