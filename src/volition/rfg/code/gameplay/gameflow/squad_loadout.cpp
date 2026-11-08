// Original: volition/rfg/code/gameplay/gameflow/squad_loadout.cpp
//
// gameflow.md GF2. squad_loadout.xtbl: the weapon and grenades of each kind of squad member.

#include "volition/rfg/code/gameplay/gameflow/squad_loadout.h"

#include "volition/rfg/code/objects/inventory_item.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/vlib/code/math/crc.h"
#include "volition/vlib/code/util/fastxml.h"
#include "volition/vlib/code/util/string_comp.h"
#include "volition/vlib/code/util/varray.h"

#include <stdio.h>

// Magic number: the loadout table holds at most 20 kinds.
static farray<squad_member_load_out, 20> All_squad_load_out_types; // 0x016576B0
static const char* const SQUAD_LOADOUT_FILE = "squad_loadout.xtbl"; // 0x012DBC80

// 0x00773FC0 (:43)
squad_member_load_out::squad_member_load_out()
{
    reset();
}

// 0x0075E2F0 (:77), dlc_bundle_id in EAX, name_buffer in ESI. rfg.exe inlines it in the system init too.
static const char* get_squad_loadout_file_name(int dlc_bundle_id, char* name_buffer)
{
    if (dlc_bundle_id == -1)
    {
        return SQUAD_LOADOUT_FILE;
    }
    sprintf(name_buffer, "dlc%02d_%s", dlc_bundle_id, SQUAD_LOADOUT_FILE);
    return cf_exist(name_buffer) ? name_buffer : SQUAD_LOADOUT_FILE;
}

// 0x00773FD0 (:94)
static const squad_member_load_out* find_member_load_out(int load_type)
{
    if (load_type > -1 && load_type < All_squad_load_out_types.size())
    {
        return &All_squad_load_out_types[load_type];
    }
    return nullptr;
}

// 0x00773FF0 (:103)
static const squad_member_load_out* find_member_load_out(const char* name)
{
    const unsigned int crc = crc_stri(name, 0);
    for (int i = 0; i < All_squad_load_out_types.size(); i++)
    {
        if (crc == All_squad_load_out_types[i].name_crc)
        {
            return &All_squad_load_out_types[i];
        }
    }
    return nullptr;
}

// 0x0075E330 (:150)
void squad_member_load_out::reset()
{
    name_crc = 0;
    w_info = nullptr;
    g_info = nullptr;
    g_count = 0;
}

// 0x0075E340 (:160)
bool squad_member_load_out::load(const char* name, xml_element* xml_root)
{
    reset();
    if (name == nullptr)
    {
        return false;
    }
    name_crc = crc_stri(name, 0);
    const xml_element* weapon = xml_find(xml_root, "weapon_type");
    if (weapon == nullptr)
    {
        return false;
    }
    w_info = inv_item_get_info(weapon->text);
    if (w_info == nullptr)
    {
        return false;
    }
    xml_element* grenades = xml_find(xml_root, "grenades");
    if (grenades == nullptr)
    {
        return true;
    }
    const char* type = xml_table_get_req_string_ref(grenades, "grenade_type");
    const char* item = nullptr;
    if (v_stricmp(type, "normal") == 0)
    {
        item = "grenade";
    }
    else if (v_stricmp(type, "singularity") == 0)
    {
        item = "singularity_bomb";
    }
    else if (v_stricmp(type, "marauder") == 0)
    {
        item = "marauder_shockade";
    }
    if (item != nullptr)
    {
        g_info = inv_item_get_info(item);
    }
    xml_table_get_req_int(g_count, grenades, "grenade_count");
    return true;
}

// 0x0078C360 (:201)
void squad_loadout_system_init(int dlc_id)
{
    All_squad_load_out_types.clear();
    char name_buffer[256];
    const char* filename = get_squad_loadout_file_name(dlc_id, name_buffer);
    if (filename == nullptr)
    {
        return;
    }
    xml_element* root = xml_table_parse(filename);
    if (root == nullptr)
    {
        return;
    }
    for (xml_element* cur = xml_find(root, "squad_loadout"); cur != nullptr; cur = xml_find_next(root, cur, "squad_loadout"))
    {
        const xml_element* name = xml_find(cur, "Name");
        if (name != nullptr && find_member_load_out(name->text) == nullptr)
        {
            squad_member_load_out load_out;
            if (load_out.load(name->text, cur))
            {
                All_squad_load_out_types.add(load_out);
            }
        }
    }
    xml_table_close(root);
}

// 0x00774030 (:243)
void squad_loadout_create(human* hp, int member_load)
{
    if (hp == nullptr || member_load == -1)
    {
        return;
    }
    const squad_member_load_out* load_out = find_member_load_out(member_load);
    if (load_out == nullptr)
    {
        return;
    }
    human_add_item_to_inventory(hp, load_out->w_info, 0, -1, -1, true, false, true);
    if (load_out->g_info != nullptr)
    {
        human_add_item_to_inventory(hp, load_out->g_info, load_out->g_count, -1, -1, true, false, true);
    }
}

// 0x007740A0 (:269)
int squad_loadout_find(const char* loadout_name)
{
    const unsigned int crc = crc_stri(loadout_name, 0);
    for (int i = 0; i < All_squad_load_out_types.size(); i++)
    {
        if (crc == All_squad_load_out_types[i].name_crc)
        {
            return i;
        }
    }
    return -1;
}
