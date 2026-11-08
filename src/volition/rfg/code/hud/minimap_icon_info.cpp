// Original: volition/rfg/code/hud/minimap_icon_info.cpp
//
// The map elements' table (minimap_icon_info.xtbl): each element's bitmap, caption, description and flags.

#include "volition/rfg/code/hud/minimap_icon_info.h"

#include "volition/interface_lib/code/bitmap_sheets.h"
#include "volition/vlib/code/bmpman/bmpman.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/math/crc.h"
#include "volition/rfg/code/rfg.h"
#include "volition/vlib/code/memory/virtual_mempool.h"
#include "volition/vlib/code/util/fastxml.h"

#include <stdio.h>

// ---- file-static globals
minimap_icon_info Minimap_icon_infos[59]; // 0x02BD03D8
// object_pool<hud_icon,64,1> Hud_icons (0x02BD31F0) is used here too; placed in hud_icon.cpp.
// In minimap_elements order; the table's Name elements are matched to these.
static checksum_stri Minimap_icon_name_crcs[59] = // 0x02BE5138
{
    checksum_stri("MAP_ELEMENT_STAR_RED_ICON", 0),
    checksum_stri("MAP_ELEMENT_STAR_BLUE_ICON", 0),
    checksum_stri("MAP_ELEMENT_STAR_GREEN_ICON", 0),
    checksum_stri("MAP_ELEMENT_STAR_WHITE_ICON", 0),
    checksum_stri("MAP_ELEMENT_CIRCLE_BLUE_ICON", 0),
    checksum_stri("MAP_ELEMENT_CIRCLE_BLUE_1_ICON", 0),
    checksum_stri("MAP_ELEMENT_CIRCLE_BLUE_2_ICON", 0),
    checksum_stri("MAP_ELEMENT_CIRCLE_BLUE_3_ICON", 0),
    checksum_stri("MAP_ELEMENT_CIRCLE_BLUE_4_ICON", 0),
    checksum_stri("MAP_ELEMENT_CIRCLE_VIOLET_1_ICON", 0),
    checksum_stri("MAP_ELEMENT_CIRCLE_VIOLET_2_ICON", 0),
    checksum_stri("MAP_ELEMENT_CIRCLE_VIOLET_3_ICON", 0),
    checksum_stri("MAP_ELEMENT_CIRCLE_VIOLET_4_ICON", 0),
    checksum_stri("MAP_ELEMENT_AMMO_BOX", 0),
    checksum_stri("MAP_ELEMENT_CONVOY_ICON", 0),
    checksum_stri("MAP_ELEMENT_CONVOY_CAPTURE_ICON", 0),
    checksum_stri("MAP_ELEMENT_RIDING_SHOTGUN", 0),
    checksum_stri("MAP_ELEMENT_AREA_DEFENSE", 0),
    checksum_stri("MAP_ELEMENT_COURIER_ICON", 0),
    checksum_stri("MAP_ELEMENT_RAID_ICON", 0),
    checksum_stri("MAP_ELEMENT_RAID_ICON_ALWAYS_SHOW", 0),
    checksum_stri("MAP_ELEMENT_DEF_RAID_ICON", 0),
    checksum_stri("MAP_ELEMENT_BASE_ICON", 0),
    checksum_stri("MAP_ELEMENT_HOUSE_ICON", 0),
    checksum_stri("MAP_ELEMENT_HOUSE_ICON_ALWAYS_SHOW", 0),
    checksum_stri("MAP_ELEMENT_DEMOLITIONS_MASTER", 0),
    checksum_stri("MAP_ELEMENT_GUERRILLA_EXPRESS", 0),
    checksum_stri("MAP_ELEMENT_GENERIC_ACTIVITY", 0),
    checksum_stri("MAP_ELEMENT_ARROW_ICON", 0),
    checksum_stri("MAP_ELEMENT_CIRCLE_FILLED_OUTLINE", 0),
    checksum_stri("MAP_ELEMENT_CONVOY_END_POINT", 0),
    checksum_stri("MAP_ELEMENT_GUERILLA_EXP_END_POINT", 0),
    checksum_stri("MAP_ELEMENT_MISSION_START", 0),
    checksum_stri("MAP_ELEMENT_TARGET", 0),
    checksum_stri("MAP_ELEMENT_TARGET_PRIORITY_1", 0),
    checksum_stri("MAP_ELEMENT_TARGET_PRIORITY_2", 0),
    checksum_stri("MAP_ELEMENT_TARGET_PRIORITY_3", 0),
    checksum_stri("MAP_ELEMENT_TARGET_PRIORITY_4", 0),
    checksum_stri("MAP_ELEMENT_SAFE_HOUSE", 0),
    checksum_stri("MAP_ELEMENT_DIRECTIONAL_ARROW", 0),
    checksum_stri("MAP_ELEMENT_MISSION_OBJECTIVE", 0),
    checksum_stri("MAP_ELEMENT_MISSION_OBJECTIVE_UP", 0),
    checksum_stri("MAP_ELEMENT_MISSION_OBJECTIVE_DOWN", 0),
    checksum_stri("MAP_ELEMENT_TANK_ICON", 0),
    checksum_stri("MAP_ELEMENT_FLAG_ICON", 0),
    checksum_stri("MAP_ELEMENT_KOTH_GUERRILLA", 0),
    checksum_stri("MAP_ELEMENT_KOTH_EDF", 0),
    checksum_stri("MAP_ELEMENT_KOTH_NEUTRAL", 0),
    checksum_stri("MAP_ELEMENT_CARBOMB", 0),
    checksum_stri("MAP_ELEMENT_BLIP", 0),
    checksum_stri("MAP_ELEMENT_BLIP_HUMAN", 0),
    checksum_stri("MAP_ELEMENT_PLAYER", 0),
    checksum_stri("MAP_ELEMENT_STRUCTURE_EDF", 0),
    checksum_stri("MAP_ELEMENT_VEHICLE", 0),
    checksum_stri("MAP_ELEMENT_IMPORTANCE_MEDIUM", 0),
    checksum_stri("MAP_ELEMENT_IMPORTANCE_HIGH", 0),
    checksum_stri("MAP_ELEMENT_NORTH", 0),
    checksum_stri("MAP_ELEMENT_RING_ICON", 0),
    checksum_stri("MAP_ELEMENT_CAMERA_CONE", 0),
};

// 0x0083F540 (.h:105)
minimap_icon_info_flags::minimap_icon_info_flags()
{
    visible_on_fs_map = 0;
    visible_when_zoomed = 0;
    display_caption = 0;
    is_target_icon = 0;
    is_objective_icon = 0;
    is_activity_icon = 0;
    visible_over_fow = 0;
}

// 0x0083F550 (.h:114)
void minimap_icon_info_flags::set(const minimap_icon_info_flags& other)
{
    visible_on_fs_map = other.visible_on_fs_map;
    visible_over_fow = other.visible_over_fow;
    is_activity_icon = other.is_activity_icon;
    is_objective_icon = other.is_objective_icon;
    is_target_icon = other.is_target_icon;
    display_caption = other.display_caption;
    visible_when_zoomed = other.visible_when_zoomed;
}

// 0x0084E630
minimap_icon_info::minimap_icon_info()
{
    init();
}

// 0x0084E640 (:87)
void minimap_icon_info::init()
{
    bitmap_id = bm_get_bogus_bitmap_handle();
    icon_name[0] = '\0';
    caption[0] = '\0';
    description[0] = '\0';
}

// 0x0084E660 (:96)
unsigned int get_minimap_bitmap_id(int which)
{
    return (unsigned int)which < NUM_MAP_ELEMENTS ? Minimap_icon_infos[which].bitmap_id : bm_get_bogus_bitmap_handle();
}

// 0x0084E680 (:104)
const char* get_minimap_bitmap_icon_name(int which)
{
    return (unsigned int)which < NUM_MAP_ELEMENTS ? Minimap_icon_infos[which].icon_name : nullptr;
}

// 0x0084E6A0 (:112)
bool minimap_is_icon_mission_start(minimap_elements element)
{
    return element == MAP_ELEMENT_MISSION_START;
}

// 0x0084E6B0 (:118)
const char* get_minimap_bitmap_caption(int which)
{
    return (unsigned int)which < NUM_MAP_ELEMENTS ? Minimap_icon_infos[which].caption : nullptr;
}

// 0x0084E6D0 (:126)
const char* get_minimap_bitmap_description(int which)
{
    return (unsigned int)which < NUM_MAP_ELEMENTS ? Minimap_icon_infos[which].description : nullptr;
}

// 0x0084E6F0 (:136)
const char* get_unknown_mission_caption()
{
    return "HM_UNKOWN_MISSION";
}

// 0x0084E700 (:142)
const char* get_unknown_mission_description()
{
    return "HM_MAP_MISSION_DESC";
}

// 0x0084E710 (:148)
static int get_minimap_icon_info_index(checksum_stri crc_val)
{
    for (int i = 0; i < NUM_MAP_ELEMENTS; i++)
    {
        if (Minimap_icon_name_crcs[i] == crc_val)
        {
            return i;
        }
    }
    return -1;
}

// 0x0084E730 (:161) (LTCG: name in EAX, icon_caption in EDI)
static void add_minimap_icon_info(const char* name, const char* icon_name, const char* icon_caption, const char* icon_description, minimap_icon_info_flags flags)
{
    if (name == nullptr || icon_name == nullptr)
    {
        return;
    }
    const int index = get_minimap_icon_info_index(checksum_stri(name, 0));
    if (index == -1)
    {
        return;
    }
    _snprintf_safe(Minimap_icon_infos[index].icon_name, 0x20, icon_name);
    if (icon_caption != nullptr)
    {
        _snprintf_safe(Minimap_icon_infos[index].caption, 0x20, icon_caption);
    }
    if (icon_description != nullptr)
    {
        _snprintf_safe(Minimap_icon_infos[index].description, 0x80, icon_description);
    }
    Minimap_icon_infos[index].flags.set(flags);
}

// 0x0084E7E0 (:192)
void refresh_minimap_icon_bitmap_ids()
{
    for (int i = 0; i < NUM_MAP_ELEMENTS; i++)
    {
        Minimap_icon_infos[i].bitmap_id = vint_bms_load(Minimap_icon_infos[i].icon_name);
    }
}

// The text of a boolean element. Inlined per flag in rfg.exe.
static bool xml_is_true(const xml_element* element)
{
    return element != nullptr && v_stricmp(element->text, "true") == 0;
}

// 0x0084E810 (:201)
void minimap_load_icon_table(int dlc_id)
{
    char filename[64];
    bool found = false;
    if (dlc_id != -1)
    {
        sprintf(filename, "dlc%02d_%s", dlc_id, "minimap_icon_info.xtbl");
        found = cf_exist(filename);
    }
    if (!found)
    {
        strcpy(filename, "minimap_icon_info.xtbl");
    }
    for (int i = 0; i < NUM_MAP_ELEMENTS; i++)
    {
        Minimap_icon_infos[i].init();
    }
    const int mark = Common_mempool.mark();
    xml_element* root = xml_parse(filename, (mempool_base*)&Common_mempool);
    xml_element* table = root != nullptr ? xml_find(root, "Table") : nullptr;
    if (table != nullptr)
    {
        for (xml_element* desc = xml_find(table, "Description"); desc != nullptr; desc = xml_find_next(table, desc, "Description"))
        {
            minimap_icon_info_flags flags;
            const xml_element* name = xml_find(desc, "Name");
            const xml_element* icon_name = xml_find(desc, "IconName");
            const xml_element* caption = xml_find(desc, "FSCaption");
            const xml_element* description = xml_find(desc, "FSDescription");
            flags.visible_over_fow = xml_is_true(xml_find(desc, "VisibleOverFOW"));
            flags.is_activity_icon = xml_is_true(xml_find(desc, "IsActivityIcon"));
            flags.is_objective_icon = xml_is_true(xml_find(desc, "IsObjectiveIcon"));
            flags.is_target_icon = xml_is_true(xml_find(desc, "IsTargetIcon"));
            flags.visible_on_fs_map = xml_is_true(xml_find(desc, "VisibleOnFSMap"));
            flags.visible_when_zoomed = xml_is_true(xml_find(desc, "VisibleWhenZoomed"));
            flags.display_caption = caption != nullptr;
            add_minimap_icon_info(name != nullptr ? name->text : nullptr, icon_name != nullptr ? icon_name->text : nullptr, caption != nullptr ? caption->text : nullptr, description != nullptr ? description->text : nullptr, flags);
        }
    }
    Common_mempool.restore_to_mark(mark);
}

// 0x0084EB10 (:305)
bool minimap_is_directional_icon(minimap_elements element)
{
    return element == MAP_ELEMENT_GUERILLA_EXP_END_POINT;
}

// 0x0084EB20 (:321)
bool minimap_element_visible_on_fs_map(minimap_elements element)
{
    return element < NUM_MAP_ELEMENTS && Minimap_icon_infos[element].flags.visible_on_fs_map;
}

// 0x0084EB40 (:331)
bool minimap_element_is_visible_when_zoomed(minimap_elements element)
{
    return element < NUM_MAP_ELEMENTS && Minimap_icon_infos[element].flags.visible_when_zoomed;
}

// 0x0084EB60 (:342)
bool minimap_element_visible_over_fow(minimap_elements element)
{
    return element < NUM_MAP_ELEMENTS && Minimap_icon_infos[element].flags.visible_over_fow;
}

// 0x0084EB80 (:352)
bool minimap_element_display_caption(minimap_elements element)
{
    return element < NUM_MAP_ELEMENTS && Minimap_icon_infos[element].flags.display_caption;
}

// 0x0084EBA0 (:364)
bool minimap_element_is_activity_icon(minimap_elements element)
{
    return element < NUM_MAP_ELEMENTS && Minimap_icon_infos[element].flags.is_activity_icon;
}

// 0x0084EBC0 (:375)
bool minimap_element_is_objective_icon(minimap_elements element)
{
    return element < NUM_MAP_ELEMENTS && Minimap_icon_infos[element].flags.is_objective_icon;
}

// 0x0084EBE0 (:386)
bool minimap_element_is_target_icon(minimap_elements element)
{
    return element < NUM_MAP_ELEMENTS && Minimap_icon_infos[element].flags.is_target_icon;
}
