// Original: volition/rfg/code/objects/inventory_item.h
// Implementation: volition/rfg/code/objects/inventory_item.cpp
//
// Human inventories: inventory_item, a linked list entry of a human's inventory (item info, count, selection slot,
// weapon), allocated from a pool; inv_item_info entries come from the inventory items table. Lookups by name, weapon,
// weapon class and slot, and picking the best weapon.
#pragma once

#include "volition/rfg/code/weapons/weapons_enums.h"
#include "volition/vlib/code/math/crc.h"

class human;
struct weapon_info;
class weapon;

// An entry of items_inventory.xtbl (Inv_item_info). Placement inferred.
// size 0x28
struct inv_item_info
{
    char* name; // 0x00
    checksum_stri name_checksum; // 0x04
    char* display_name; // 0x08
    int cost; // 0x0C
    int default_count; // 0x10
    int max_item; // 0x14
    char* description; // 0x18
    weapon_info* w_info; // 0x1C, set by weapons_read_table
    unsigned char item_order; // 0x20
    bool (*use)(); // 0x24
};

extern inv_item_info Inv_item_info[64]; // 0x0306E550
extern int Num_inv_item_info; // 0x0302F0E4

// size 0x1C
struct inventory_item
{
    inventory_item* next; // 0x00
    inventory_item* prev; // 0x04
    const inv_item_info* info; // 0x08
    int count; // 0x0C
    int selection_slot; // 0x10
    int attachment_prop; // 0x14
    unsigned int weapon_handle; // 0x18
};

void inv_item_system_init(); // 0x00A121A0 (:62)
inventory_item* inv_item_allocate(); // 0x00A12270 (:80)
void inv_item_free(inventory_item* iip); // 0x00A122C0 (:97)
void inv_item_parse(int dlc_id); // 0x00A12300 (:106)
inv_item_info* inv_item_get_info(const char* name); // 0x00A125D0 (:206)
unsigned int inv_item_get_info_handle(const char* name); // 0x00A36740 (:230)
int inv_item_get_info_index(const char* name); // 0x00A12620 (:246)
inventory_item* inv_item_get_weapon_from_class(const human* hp, weapon_class_type weapon_class); // 0x00A36760 (:261)
inventory_item* inv_item_get_item_from_weapon(const human* hp, const weapon* wp); // 0x00A12660 (:273)
inventory_item* inv_item_get_item_from_weapon_info(const human* hp, unsigned int winfo_handle); // 0x00A367C0 (:291)
inventory_item* inv_item_get_best_firearm(const human* hp); // 0x00A36830 (:311)
inventory_item* inv_item_get_best_melee(const human* hp); // 0x00A368B0 (:325)
inventory_item* inv_item_get_best_weapon(const human* hp, bool return_anything); // 0x00A36910 (:339)
void inv_item_level_init_post(); // 0x00A126A0 (:367)
void inv_item_level_shutdown(); // 0x00A126B0 (:388)
inventory_item* inv_item_get_item_in_slot(const human* hp, int selection_slot); // 0x00A126C0 (:410)
unsigned int inv_item_info_get_handle(const inv_item_info* info); // 0x00A126F0 (:423)
inv_item_info* inv_item_info_from_handle(unsigned int handle); // 0x00A12700 (:428)
