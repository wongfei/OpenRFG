// Original: volition/rfg/code/gameplay/gameflow/squad_loadout.h
// Implementation: volition/rfg/code/gameplay/gameflow/squad_loadout.cpp
//
// Squad loadouts: the weapons of squad members, read from the loadout table.
#pragma once


class human;
struct inv_item_info;
struct xml_element;

// size 0x10
class squad_member_load_out
{
public:
    squad_member_load_out(); // 0x00773FC0 (:43)
    void reset(); // 0x0075E330 (:150)
    bool load(const char* name, xml_element* xml_root); // 0x0075E340 (:160)

    unsigned int name_crc; // 0x00
    inv_item_info* w_info; // 0x04
    inv_item_info* g_info; // 0x08
    int g_count; // 0x0C
};

void squad_loadout_system_init(int dlc_id); // 0x0078C360 (:201)
void squad_loadout_create(human* hp, int member_load); // 0x00774030 (:243)
int squad_loadout_find(const char* loadout_name); // 0x007740A0 (:269)
