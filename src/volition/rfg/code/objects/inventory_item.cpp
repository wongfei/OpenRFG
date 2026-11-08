// Original: volition/rfg/code/objects/inventory_item.cpp
//
// The inventory item table (items_inventory.xtbl) and the pool and queries of the humans' inventory lists
// (player_minimal.md P3). Compiled in rfg_objects_unity.cpp (Inv_item_free_list and Num_inv_items are file statics of
// human.cpp there).
// The external interface is declared in inventory_item.h.

#include "volition/rfg/code/objects/inventory_item.h"

#include "volition/rfg/code/interface/ingame/interface.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/level/world.inl"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/rfg.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/vlib/code/memory/rfg_split_mempool.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/vlib/code/util/string_comp.h"

#include "keen/base/trace.hpp"

#include <stdio.h>
#include <string.h>

// ---- globals and file statics
// 0x01311148, the use scripts by Inv_use_script_names (zero in rfg.exe: no item has a use script). Placed here; the P0
// stub list placed it in player_meta_data.cpp.
static bool (* const Inv_use_scripts[1])() = {nullptr};
static const char* Inv_use_script_names[1] = {"none"}; // 0x016944F4
// inventory_item* Inv_item_free_list (0x0302366C) is used here too; placed in human.cpp.
static inventory_item Inv_items[200]; // 0x03023878
// int Num_inv_items (0x0302F0E8) is used here too; placed in human.cpp.
inv_item_info Inv_item_info[64]; // 0x0306E550
int Num_inv_item_info; // 0x0302F0E4

// The weapon an inventory entry holds, when it is a weapon object.
static const weapon* inventory_weapon(const inventory_item* iip)
{
    if (iip->weapon_handle == 0xFFFFFFFF)
    {
        return nullptr;
    }
    object* objp = World.object_from_handle(iip->weapon_handle);
    if (objp == nullptr || objp->get_obj_type() != OT_ITEM || objp->get_sub_type() != OT_SUB_ITEM_WEAPON)
    {
        return nullptr;
    }
    return (const weapon*)objp;
}

// Firearms: bullets, projectiles, electricity, repair, blanks and flames (not thrown or melee weapons).
static bool is_firearm_ammo(weapon_ammo_type ammo_type)
{
    return ammo_type == WEAPON_AMMO_TYPE_BULLET || ammo_type == WEAPON_AMMO_TYPE_PROJECTILE || ammo_type == WEAPON_AMMO_TYPE_ELECTRICITY ||
           ammo_type == WEAPON_AMMO_TYPE_REPAIR || ammo_type == WEAPON_AMMO_TYPE_BLANKS || ammo_type == WEAPON_AMMO_TYPE_FLAME;
}

// ---- functions
// 0x00A121A0 (:62): all 200 entries on the circular free list.
void inv_item_system_init()
{
    Inv_item_free_list = nullptr;
    for (int i = 0; i < 200; i++)
    {
        inventory_item* iip = &Inv_items[i];
        if (Inv_item_free_list == nullptr)
        {
            Inv_item_free_list = iip;
            iip->next = iip;
            iip->prev = iip;
        }
        else
        {
            iip->prev = Inv_item_free_list->prev;
            iip->next = Inv_item_free_list;
            Inv_item_free_list->prev->next = iip;
            Inv_item_free_list->prev = iip;
        }
    }
    // The original counts the unrolled loop down to 0.
    Num_inv_items = 0;
}

// 0x00A12270 (:80): the head of the free list.
inventory_item* inv_item_allocate()
{
    inventory_item* iip = Inv_item_free_list;
    if (iip == nullptr)
    {
        return nullptr;
    }
    Num_inv_items++;
    if (iip->next == iip)
    {
        Inv_item_free_list = nullptr;
    }
    else
    {
        if (iip->prev != nullptr)
        {
            iip->prev->next = iip->next;
        }
        Inv_item_free_list = iip->next;
        if (iip->next != nullptr)
        {
            iip->next->prev = iip->prev;
        }
    }
    iip->prev = nullptr;
    iip->next = nullptr;
    return iip;
}

// 0x00A122C0 (:97): to the end of the free list.
void inv_item_free(inventory_item* iip)
{
    Num_inv_items--;
    if (Inv_item_free_list == nullptr)
    {
        iip->next = iip;
        iip->prev = iip;
        Inv_item_free_list = iip;
    }
    else
    {
        iip->prev = Inv_item_free_list->prev;
        iip->next = Inv_item_free_list;
        Inv_item_free_list->prev->next = iip;
        Inv_item_free_list->prev = iip;
    }
    iip->info = nullptr;
}

// 0x00A12300 (:106)
// items_inventory.xtbl into Permanent_mempool (a DLC's dlcNN_items_inventory.xtbl into the level pool, appended, its
// item_order continuing after the base items).
void inv_item_parse(int dlc_id)
{
    // Magic number: MAX_INV_ITEM_TYPES (Inv_item_info).
    const int max_inv_item_types = 64;
    unsigned char item_order = dlc_id != -1 ? (unsigned char)Num_inv_item_info : 0;
    mempool* pool;
    char filename[256];
    if (dlc_id != -1)
    {
        pool = &Level_mempool;
        sprintf(filename, "dlc%02d_%s", dlc_id, "items_inventory.xtbl");
    }
    else
    {
        pool = &Permanent_mempool;
        strcpy(filename, "items_inventory.xtbl");
    }
    xml_element* table = xml_table_parse(filename);
    // The original reads the strings into a thread-local 0x400 byte buffer.
    char buffer[0x400];
    for (xml_element* entry = xml_table_find_opt(table, "Inventory_Item"); entry != nullptr; entry = xml_table_find_next(table, entry, "Inventory_Item"))
    {
        if (Num_inv_item_info >= max_inv_item_types)
        {
            keen::debug::nativePrintArgs("Too many inventory item types.  Get a programmer to increase MAX_INV_ITEM_TYPES.");
            break;
        }
        inv_item_info& info = Inv_item_info[Num_inv_item_info++];
        info.item_order = item_order++;
        info.w_info = nullptr;
        xml_table_get_req_string(buffer, sizeof(buffer), entry, "Name");
        info.name = (char*)pool->alloc((unsigned int)strlen(buffer) + 1, 1);
        strcpy(info.name, buffer);
        info.name_checksum = checksum_stri(info.name, 0);
        xml_table_get_req_string(buffer, sizeof(buffer), entry, "Display_Name");
        info.display_name = (char*)pool->alloc((unsigned int)strlen(buffer) + 1, 1);
        strcpy(info.display_name, buffer);
        xml_element* use_script = xml_table_find_opt(entry, "Use_Script");
        info.use = use_script != nullptr ? Inv_use_scripts[xml_table_get_type_from_list(Inv_use_script_names, 1, use_script)] : nullptr;
        if (!xml_table_get_opt_int(info.cost, entry, "Cost"))
        {
            info.cost = -1;
        }
        if (!xml_table_get_opt_int(info.default_count, entry, "Default_Count"))
        {
            info.default_count = 1;
        }
        if (!xml_table_get_opt_int(info.max_item, entry, "Max_Inventory"))
        {
            info.max_item = info.default_count;
        }
        if (xml_table_get_opt_string(buffer, sizeof(buffer), entry, "Description"))
        {
            interface_replace_special_font(buffer);
            info.description = (char*)pool->alloc((unsigned int)strlen(buffer) + 1, 1);
            strcpy(info.description, buffer);
        }
        else
        {
            info.description = nullptr;
        }
    }
    xml_table_close(table);
}

// 0x00A125D0 (:206)
inv_item_info* inv_item_get_info(const char* name)
{
    if (name == nullptr)
    {
        return nullptr;
    }
    for (int i = 0; i < Num_inv_item_info; i++)
    {
        if (v_stricmp(Inv_item_info[i].name, name) == 0)
        {
            return &Inv_item_info[i];
        }
    }
    return nullptr;
}

// 0x00A36740 (:230)
unsigned int inv_item_get_info_handle(const char* name)
{
    const inv_item_info* info = inv_item_get_info(name);
    return info != nullptr ? info->name_checksum.get_crc() : 0xFFFFFFFF;
}

// 0x00A12620 (:246)
int inv_item_get_info_index(const char* name)
{
    for (int i = 0; i < Num_inv_item_info; i++)
    {
        if (v_stricmp(Inv_item_info[i].name, name) == 0)
        {
            return i;
        }
    }
    return -1;
}

// 0x00A36760 (:261)
inventory_item* inv_item_get_weapon_from_class(const human* hp, weapon_class_type weapon_class)
{
    inventory_item* iip = hp->inventory;
    while (iip != nullptr)
    {
        const weapon* wp = inventory_weapon(iip);
        if (wp != nullptr && wp->w_info->weapon_class == weapon_class)
        {
            return iip;
        }
        iip = iip->next;
        if (iip == hp->inventory)
        {
            break;
        }
    }
    return nullptr;
}

// 0x00A12660 (:273)
inventory_item* inv_item_get_item_from_weapon(const human* hp, const weapon* wp)
{
    if (hp == nullptr || wp == nullptr)
    {
        return nullptr;
    }
    inventory_item* iip = hp->inventory;
    while (iip != nullptr)
    {
        if (iip->weapon_handle == wp->get_handle())
        {
            return iip;
        }
        iip = iip->next;
        if (iip == hp->inventory)
        {
            break;
        }
    }
    return nullptr;
}

// 0x00A367C0 (:291)
inventory_item* inv_item_get_item_from_weapon_info(const human* hp, unsigned int winfo_handle)
{
    if (hp == nullptr || winfo_handle == 0xFFFFFFFF)
    {
        return nullptr;
    }
    inventory_item* iip = hp->inventory;
    while (iip != nullptr)
    {
        weapon* wp = World.object_from_handle<weapon>(iip->weapon_handle);
        if (wp != nullptr && weapons_get_weapon_info_handle(wp->w_info) == winfo_handle)
        {
            return iip;
        }
        iip = iip->next;
        if (iip == hp->inventory)
        {
            break;
        }
    }
    return nullptr;
}

// 0x00A36830 (:311)
inventory_item* inv_item_get_best_firearm(const human* hp)
{
    inventory_item* iip = hp->inventory;
    while (iip != nullptr)
    {
        const weapon* wp = inventory_weapon(iip);
        if (wp != nullptr && is_firearm_ammo(wp->w_info->ammo_type))
        {
            return iip;
        }
        iip = iip->next;
        if (iip == hp->inventory)
        {
            break;
        }
    }
    return nullptr;
}

// 0x00A368B0 (:325)
inventory_item* inv_item_get_best_melee(const human* hp)
{
    inventory_item* iip = hp->inventory;
    while (iip != nullptr)
    {
        const weapon* wp = inventory_weapon(iip);
        if (wp != nullptr && wp->w_info->ammo_type == WEAPON_AMMO_TYPE_MELEE)
        {
            return iip;
        }
        iip = iip->next;
        if (iip == hp->inventory)
        {
            break;
        }
    }
    return nullptr;
}

// 0x00A36910 (:339): the first firearm, else the first melee weapon, else the first entry (or none).
inventory_item* inv_item_get_best_weapon(const human* hp, bool return_anything)
{
    inventory_item* firearm = inv_item_get_best_firearm(hp);
    if (firearm != nullptr)
    {
        return firearm;
    }
    inventory_item* melee = inv_item_get_best_melee(hp);
    if (melee != nullptr)
    {
        return melee;
    }
    return return_anything ? hp->inventory : nullptr;
}

// 0x00A126A0 (:367)
void inv_item_level_init_post()
{
}

// 0x00A126B0 (:388)
void inv_item_level_shutdown()
{
}

// 0x00A126C0 (:410)
inventory_item* inv_item_get_item_in_slot(const human* hp, int selection_slot)
{
    inventory_item* iip = hp->inventory;
    while (iip != nullptr)
    {
        if (iip->selection_slot == selection_slot)
        {
            return iip;
        }
        iip = iip->next;
        if (iip == hp->inventory)
        {
            break;
        }
    }
    return nullptr;
}

// 0x00A126F0 (:423)
unsigned int inv_item_info_get_handle(const inv_item_info* info)
{
    return info->name_checksum.get_crc();
}

// 0x00A12700 (:428)
inv_item_info* inv_item_info_from_handle(unsigned int handle)
{
    for (int i = 0; i < Num_inv_item_info; i++)
    {
        if (Inv_item_info[i].name_checksum.get_crc() == handle)
        {
            return &Inv_item_info[i];
        }
    }
    return nullptr;
}
