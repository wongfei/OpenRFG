// Original: volition/rfg/code/objects/salvage.cpp
//
// Salvage: the material name table, salvage.xtbl (value and item list per material), and the salvage items.

#include "volition/rfg/code/objects/salvage.h"

#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/rfg/code/gameplay/activities/activity.h"
#include "volition/rfg/code/gameplay/activities/activity_vrail.h"
#include "volition/rfg/code/gameplay/upgrades.h"
#include "volition/rfg/code/gameplay/wrecking_crew/wrecking_crew.h"
#include "volition/rfg/code/gamewide/downloadable_content.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/render_lib/code/gr/gr_font.h"
#include "volition/render_lib/code/gr/gr.h"
#include "volition/rfg/code/destruction/rfg_mover.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/objects/object_item.h"
#include "volition/rfg/code/scripting/action_script/global_event.h"
#include "volition/rfg/code/scripting/action_script/script_types.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/vlib/code/util/fastxml.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/util/string_comp.h"

#include <stdio.h>

// size 0x8
struct salvage_info
{
    salvage_material_type material; // 0x00
    const item_info* item_inf; // 0x04
};

const char* Salvage_material_names[3] = { "metal", "ore", "chemical" }; // 0x0169476C

// Built by the variadic constructor from the names above (0x0107CC60).
str_data_hash_table<unsigned int, 32, 32, unsigned char> Salvage_material_name_table( // 0x033B1A30
    Salvage_material_names[SALVAGE_MATERIAL_METAL], (unsigned int)SALVAGE_MATERIAL_METAL,
    Salvage_material_names[SALVAGE_MATERIAL_ORE], (unsigned int)SALVAGE_MATERIAL_ORE,
    Salvage_material_names[SALVAGE_MATERIAL_CHEMICAL], (unsigned int)SALVAGE_MATERIAL_CHEMICAL,
    nullptr, 0u);

salvage_material_data_type Salvage_material_data[3]; // 0x0301AE4C
static salvage_info Salvage_items[32]; // 0x0301AEC8
static bool Salvage_debug; // 0x0302F225, a debug switch nothing in rfg.exe writes

// 0x00A05860 (global_event_data.h:49)
salvage_created_event::salvage_created_event()
    : event_data(EVENT_SALVAGE_CREATED)
{
}

// 0x00A95DE0 (:124)
// Original bugs kept: an unknown material name indexes Salvage_material_data with -1, a material with no Item_3d is read
// again for ever because the next material is only found inside that branch, and Salvage_items has no bounds check.
static void salvage_parse_table()
{
    xml_element* root = xml_table_parse("salvage.xtbl");
    xml_element* material_elem = xml_table_find_req(root, "Material");
    Salvage_material_data[0].first_index = -1;
    Salvage_material_data[1].first_index = -1;
    Salvage_material_data[2].first_index = -1;
    int next_item = 0;
    while (material_elem != nullptr)
    {
        const salvage_material_type material = salvage_resolve_material_name(xml_table_get_req_string_ref(material_elem, "Name"));
        xml_table_get_req_int(Salvage_material_data[material].value, material_elem, "Value");
        xml_element* item_elem = xml_table_find_req(xml_table_find_req(material_elem, "Item_list"), "Item_3d");
        if (item_elem == nullptr)
        {
            continue;
        }
        Salvage_material_data[material].first_index = next_item;
        int current = next_item;
        do
        {
            current = next_item;
            const item_info* info = nullptr;
            if (item_elem->text != nullptr)
            {
                for (int i = 0; i < Num_item_infos; i++)
                {
                    if (Item_infos[i].name != nullptr && v_stricmp(Item_infos[i].name, item_elem->text) == 0)
                    {
                        info = &Item_infos[i];
                        break;
                    }
                }
            }
            Salvage_items[current].item_inf = info;
            Salvage_items[current].material = material;
            item_elem = xml_table_find_next(material_elem, item_elem, "Item_3d");
            next_item = current + 1;
        } while (item_elem != nullptr);
        Salvage_material_data[material].last_index = current;
        material_elem = xml_table_find_next(root, material_elem, "Material");
    }
    xml_table_close(root);
}

// 0x00A95FE0 (:196)
void salvage_system_init()
{
    salvage_parse_table();
}

// 0x00A1F3F0 (:202)
void salvage_level_init()
{
}

// 0x00A1F400 (:206)
void salvage_level_shutdown()
{
}

// 0x00A1F410 (:210)
void salvage_do_frame()
{
}

// 0x00A41920 (:214)
item* salvage_create_item(salvage_material_type material, const vector& pos, const matrix& orient)
{
    if (material == INVALID_SALVAGE_MATERIAL || Salvage_material_data[material].first_index == -1)
    {
        return nullptr;
    }
    const int index = int_rand_range(Salvage_material_data[material].first_index, Salvage_material_data[material].last_index);
    // Magic number: rfg.exe writes these three bits as 0x6004 over the low 18 bits of the flags.
    obj_item_flags flags;
    flags.simulate = 1;
    flags.cast_shadows = 1;
    flags.cast_drop_shadows = 1;
    obj_item_props props(Salvage_items[index].item_inf, pos, orient, flags, COLLIDE_ITEM, -1.0f, material);
    item* ip = World.create_object_global<item>(props, 0xFFFFFFFF);
    if (ip == nullptr)
    {
        return nullptr;
    }
    // Magic number: 180 s until a salvage item disappears.
    ip->deletion_timer.set(180000);
    salvage_created_event event;
    gem_post_event(event);
    return ip;
}

// 0x00A72490 (:251)
salvage_material_type salvage_resolve_material_name(const char* material_name)
{
    unsigned int material;
    if (material_name != nullptr && Salvage_material_name_table.get(material_name, material))
    {
        return (salvage_material_type)material;
    }
    return INVALID_SALVAGE_MATERIAL;
}

// 0x00A1F420 (:267)
int salvage_get_value(salvage_material_type material)
{
    if (material == INVALID_SALVAGE_MATERIAL || (int)material >= 3)
    {
        return 0;
    }
    if (material == SALVAGE_MATERIAL_ORE && upgrade_has_ore_bonus())
    {
        return Salvage_material_data[SALVAGE_MATERIAL_ORE].value + 1;
    }
    return Salvage_material_data[material].value;
}

// 0x00A1F460 (:282)
bool salvage_should_spawn()
{
    if (Multiplayer || wrecking_crew_is_active())
    {
        return false;
    }
    if (activity_is_active(ACTIVITY_MODE_DEMOLITIONS_MASTER) || activity_is_active(ACTIVITY_MODE_AREA_DEFENSE))
    {
        return false;
    }
    if (camera_get_mode() == CAMERA_SATELLITE_MODE || activity_vrail_is_riding_shotgun_running())
    {
        return false;
    }
    return !dlc_get_salvage_disabled();
}

// 0x00A724D0 (:318)
// Deviation: prints to a stack buffer where rfg.exe uses its thread-local one, like the other debug renders.
void salvage_render_debug()
{
    if (!Salvage_debug)
    {
        return;
    }
    const int count = World.get_num_objects<rfg_mover>();
    for (int i = 0; i < count; i++)
    {
        rfg_mover* mover = World.get_object<rfg_mover>(i);
        const int salvage = mover->salvage_num_pieces;
        if (salvage != 0)
        {
            char text[64];
            sprintf(text, "Salvage: %d", salvage);
            gr_set_color(0xFF, 0xFF, 0xFF, 0xFF);
            // Magic numbers: 5 m above the mover, a scale of 0.03.
            vector pos(mover->pos.x + 0.0f, mover->pos.y + 5.0f, mover->pos.z + 0.0f);
            gr_3d_string(pos, Rfg_camera.real_orient, 0.03f, text, -1, GM_FILTER);
        }
    }
}
