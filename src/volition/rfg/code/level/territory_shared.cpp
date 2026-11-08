// Original: volition/rfg/code/level/territory_shared.cpp
//
// The territory table (territories.xtbl, plus dlcNN_territories.xtbl for a DLC) parsed into Territories /
// Dlc_territories: per territory its tech levels, streaming mask zones and the list of its districts (t_district).

#include "volition/rfg/code/level/territory_shared.h"

#include "volition/rfg/code/interface/localization/localization_shared.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/memory/mempool_base.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/vlib/code/util/fastxml.h"
#include "volition/vlib/code/util/hash_table.h"
#include "volition/vlib/code/util/string_comp.h"
#include "volition/vlib/code/util/string_pool.h"
#include "volition/vlib/code/util/string_util.h"
#include "volition/rfg/code/objects/object_prop_names_shared.h"

#include "keen/base/trace.hpp"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ---- file-static globals
static bool Territory_shared_inited; // 0x02CEC1E0
static bool Dlc_territory_shared_inited; // 0x02CEC1E1
static str_data_hash_table<territory,20,64,unsigned char> Dlc_territories; // 0x02F97410
static char District_name_pool_data[0x400]; // District_name_pool's buffer
static string_pool District_name_pool(District_name_pool_data, sizeof(District_name_pool_data), "District Name Pool"); // 0x02FD9800
static char District_thumbnail_name_pool_data[0x400];
static string_pool District_thumbnail_name_pool(District_thumbnail_name_pool_data, sizeof(District_thumbnail_name_pool_data), "District Thumbnail Name Pool"); // 0x02FDE6A0
static str_data_hash_table<territory,255,64,unsigned char> Territories; // 0x02FDE710
static char District_radio_line_name_pool_data[0x200];
static string_pool District_radio_line_name_pool(District_radio_line_name_pool_data, sizeof(District_radio_line_name_pool_data), "District Radio Lines"); // 0x02FE5A58

// ---- t_district / territory (header inlines)
// 0x0092B4A0
t_district::t_district()
{
}

// 0x00919CE0 (.h:116)
territory::territory()
{
    name = nullptr;
    tech_level_default = 0.0f;
    tech_level_max = 0.0f;
    index = 0;
    districts = nullptr;
    vertical_mask_zones = nullptr;
    num_v_mask_zones = 0;
    horiz_mask_zones = nullptr;
    num_h_mask_zones = 0;
}

// ---- internal functions
// 0x0091BD90 (:97): a zone number "XXYY" (column * 100 + row) as a grid id, column in bits 12-15, row in bits 8-11.
static unsigned short get_grid_id(xml_element* el)
{
    const unsigned int n = (unsigned int)atoi(el->text) & 0xFFFF;
    return (unsigned short)((((n / 100) & 0xF) << 4 | (n % 100) & 0xF) << 8);
}

// 0x00931D80 (:107, LTCG)
static bool parse_territory(xml_element* xml_territory_root, mempool_base* pool, bool dlc)
{
    xml_element* districts_el = xml_find(xml_territory_root, "Districts");
    if (districts_el == nullptr)
    {
        return false;
    }
    xml_element* name_el = xml_find(xml_territory_root, "Name");
    territory new_territory;
    territory* tp;
    if (dlc)
    {
        Dlc_territories.insert_key_data(name_el->text, new_territory);
        tp = Dlc_territories.get_ptr(name_el->text);
        if (tp == nullptr)
        {
            return false;
        }
        Dlc_territories.get_key(tp, tp->name);
    }
    else
    {
        Territories.insert_key_data(name_el->text, new_territory);
        tp = Territories.get_ptr(name_el->text);
        if (tp == nullptr)
        {
            return false;
        }
        Territories.get_key(tp, tp->name);
    }

    tp->tech_level_default = xml_get_float(xml_find(xml_territory_root, "Tech_level"), 0.0f);
    tp->tech_level_max = xml_get_float(xml_find(xml_territory_root, "Tech_level_max"), 5.0f);
    xml_element* streaming_el = xml_find(xml_territory_root, "StreamingParams");
    if (streaming_el != nullptr)
    {
        xml_element* mask_el = xml_find(streaming_el, "VerticalMask");
        if (mask_el != nullptr)
        {
            const unsigned int count = xml_count(mask_el, "Zone");
            tp->num_v_mask_zones = (int)count;
            if ((int)count > 0)
            {
                tp->vertical_mask_zones = (unsigned short*)pool->alloc(count * 2, 0);
                tp->num_v_mask_zones = 0;
                for (xml_element* zone_el = xml_find(mask_el, "Zone"); zone_el != nullptr; zone_el = xml_find_next(mask_el, zone_el, "Zone"))
                {
                    tp->vertical_mask_zones[tp->num_v_mask_zones++] = get_grid_id(zone_el);
                }
            }
        }
        mask_el = xml_find(streaming_el, "HorizMask");
        if (mask_el != nullptr)
        {
            const unsigned int count = xml_count(mask_el, "Zone");
            tp->num_h_mask_zones = (int)count;
            if ((int)count > 0)
            {
                tp->horiz_mask_zones = (unsigned short*)pool->alloc(count * 2, 0);
                tp->num_h_mask_zones = 0;
                for (xml_element* zone_el = xml_find(mask_el, "Zone"); zone_el != nullptr; zone_el = xml_find_next(mask_el, zone_el, "Zone"))
                {
                    tp->horiz_mask_zones[tp->num_h_mask_zones++] = get_grid_id(zone_el);
                }
            }
        }
    }
    tp->index = (unsigned char)xml_get_uint(xml_find(xml_territory_root, "territory_index"), 0);

    for (xml_element* district_el = xml_find(districts_el, "District"); district_el != nullptr; district_el = xml_find_next(districts_el, district_el, "District"))
    {
        xml_element* el = xml_find(district_el, "Name");
        if (el == nullptr || el->text == nullptr)
        {
            continue;
        }
        t_district* dp = new (pool->alloc(sizeof(t_district), 0)) t_district();
        if (tp->districts == nullptr)
        {
            dp->next = dp;
            dp->prev = dp;
            tp->districts = dp;
        }
        else
        {
            dp->prev = tp->districts->prev;
            dp->next = tp->districts;
            tp->districts->prev->next = dp;
            tp->districts->prev = dp;
        }
        dp->name = District_name_pool.add_unique(el->text);
        const char* display_name = "TERR_PARKER"; // the display name of a district without one (or with an empty one)
        el = xml_find(district_el, "Display_name");
        if (el != nullptr && el->text != nullptr && el->text[0] != '\0')
        {
            display_name = el->text;
        }

        dp->control = 0.0f;
        dp->control_max = 0.0f;
        dp->morale = 0.0f;
        dp->morale_max = 1000.0f;
        dp->liberated = false;
        dp->flags = district_flags();
        dp->control = xml_get_float(xml_find(district_el, "Control"), 0.0f);
        dp->control_max = xml_get_float(xml_find(district_el, "Control_max"), 0.0f);
        dp->morale = xml_get_float(xml_find(district_el, "Morale"), 0.0f);
        dp->morale_max = xml_get_float(xml_find(district_el, "Morale_max"), 1000.0f);
        dp->tech_level = 0.0f;
        dp->marauder_level = 0;
        dp->edf_level = 0;
        dp->edf_progression_level = 0;
        dp->tech_level = xml_get_float(xml_find(district_el, "liberated_tech"), 0.0f);
        dp->marauder_level = xml_get_int(xml_find(district_el, "liberated_marauder"), 0);
        dp->edf_level = xml_get_int(xml_find(district_el, "liberated_edf"), 0);
        dp->edf_progression_level = xml_get_int(xml_find(district_el, "min_progression_edf"), 0);
        const char* radio_line = xml_table_get_opt_string_ref(district_el, "liberated_radio");
        dp->liberated_radio_line = radio_line == nullptr ? nullptr : District_radio_line_name_pool.add_unique(radio_line);

        xml_element* flags_el = xml_table_find_opt(district_el, "District_Flags");
        if (flags_el != nullptr)
        {
            for (xml_element* flag_el = xml_find(flags_el, "Flag"); flag_el != nullptr; flag_el = xml_find_next(flags_el, flag_el, "Flag"))
            {
                if (v_stricmp(flag_el->text, "allow_ambient_cough") == 0)
                {
                    dp->flags.allow_cough = true;
                }
                if (v_stricmp(flag_el->text, "allow_amb_edf_civilian_dump") == 0)
                {
                    dp->flags.allow_amb_edf_civilian_dump = true;
                }
                if (v_stricmp(flag_el->text, "play_capstone_unlocked_lines") == 0)
                {
                    dp->flags.play_capstone_unlocked_lines = true;
                }
                if (v_stricmp(flag_el->text, "disable_morale_change") == 0)
                {
                    dp->flags.disable_morale_change = true;
                }
                if (v_stricmp(flag_el->text, "disable_control_change") == 0)
                {
                    dp->flags.disable_control_change = true;
                }
            }
        }

        xml_element* grid_el = xml_find(district_el, DISTRICT_GRID_INDEX_TAG.str);
        if (grid_el != nullptr)
        {
            for (xml_element* item_el = xml_find(grid_el, "Grid_item"); item_el != nullptr && dp->list.size() < 21; item_el = xml_find_next(grid_el, item_el, "Grid_item"))
            {
                grid_info info;
                info.gid = get_grid_id(xml_find(item_el, DISTRICT_INDEX_TAG.str));
                info.flags = grid_flags();
                xml_element* grid_flags_el = xml_table_find_opt(item_el, "Grid_flags");
                if (grid_flags_el != nullptr)
                {
                    for (xml_element* flag_el = xml_find(grid_flags_el, "Flag"); flag_el != nullptr; flag_el = xml_find_next(grid_flags_el, flag_el, "Flag"))
                    {
                        if (v_stricmp(flag_el->text, "play_marauder_howl") == 0)
                        {
                            info.flags.play_marauder_howl = true;
                        }
                        if (v_stricmp(flag_el->text, "marauder_zone") == 0)
                        {
                            info.flags.marauder_zone = true;
                        }
                    }
                }
                dp->list.add(info);
            }
        }

        // Magic numbers: the default grid color is grey 0x77 at alpha 0x3F, the default VFX tint white.
        dp->col.red = 0x77;
        dp->col.green = 0x77;
        dp->col.blue = 0x77;
        dp->col.alpha = 0x3F;
        el = xml_find(district_el, DISTRICT_GRID_COLOR_TAG.str);
        if (el != nullptr)
        {
            xml_element* r_el = xml_find(el, "R");
            xml_element* g_el = xml_find(el, "G");
            xml_element* b_el = xml_find(el, "B");
            dp->col.red = r_el != nullptr ? (unsigned char)atoi(r_el->text) : 0x77;
            dp->col.green = g_el != nullptr ? (unsigned char)atoi(g_el->text) : 0x77;
            dp->col.blue = b_el != nullptr ? (unsigned char)atoi(b_el->text) : 0x77;
        }
        dp->vfx_tint.red = 0xFF;
        dp->vfx_tint.green = 0xFF;
        dp->vfx_tint.blue = 0xFF;
        dp->vfx_tint.alpha = 0xFF;
        el = xml_find(district_el, VFX_TINT_TAG.str);
        if (el != nullptr)
        {
            xml_element* r_el = xml_find(el, "R");
            xml_element* g_el = xml_find(el, "G");
            xml_element* b_el = xml_find(el, "B");
            dp->vfx_tint.red = r_el != nullptr ? (unsigned char)atoi(r_el->text) : 0xFF;
            dp->vfx_tint.green = g_el != nullptr ? (unsigned char)atoi(g_el->text) : 0xFF;
            dp->vfx_tint.blue = b_el != nullptr ? (unsigned char)atoi(b_el->text) : 0xFF;
        }

        dp->localized_name_hash = localization_generate_hash_from_key(display_name);
        char no_ar_key[128];
        _snprintf_safe(no_ar_key, sizeof(no_ar_key), "%s_NO_AR", display_name);
        dp->localized_name_hash_no_ar = localization_generate_hash_from_key(no_ar_key);

        dp->disc_layer = 0;
        el = xml_find(district_el, "DiscLayer");
        if (el != nullptr)
        {
            if (v_stricmp(el->text, "Layer 0") == 0)
            {
                dp->disc_layer = 0;
            }
            else if (v_stricmp(el->text, "Layer 1") == 0)
            {
                dp->disc_layer = 1;
            }
            else
            {
                keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
            }
        }
        dp->index = (unsigned char)xml_get_uint(xml_find(district_el, "district_index"), 0);
        dp->thumbnail = "ui_thumb_parker";
        const char* thumbnail = xml_table_get_opt_filename_ref(district_el, "thumbnail");
        if (thumbnail != nullptr)
        {
            // The original drops the extension into the (no longer used) local territory, a 0x24-byte struct, with a
            // 0x40 limit; the overflow lands in the unused key buffer behind it. A 64-byte buffer here.
            char thumbnail_name[64];
            cf_drop_ext(thumbnail_name, sizeof(thumbnail_name), thumbnail);
            dp->thumbnail = District_thumbnail_name_pool.add_unique(thumbnail_name);
        }
        dp->num_load_screen_images = 0;
        xml_element* images_el = xml_find(district_el, "load_images");
        for (xml_element* image_el = xml_find(images_el, "load_image"); image_el != nullptr; image_el = xml_find_next(images_el, image_el, "load_image"))
        {
            if (image_el->text != nullptr)
            {
                dp->load_screen_images[dp->num_load_screen_images] = District_thumbnail_name_pool.add_unique(image_el->text);
                if (dp->load_screen_images[dp->num_load_screen_images] != nullptr)
                {
                    dp->num_load_screen_images++;
                }
            }
        }
    }
    return true;
}

// ---- territory
// 0x00922E30 (:437)
t_district* territory::find_district(unsigned short gid)
{
    t_district* dp = districts;
    while (dp != nullptr)
    {
        for (int i = 0; i < dp->list.size(); i++)
        {
            if (dp->list[i].gid == gid)
            {
                return dp;
            }
        }
        dp = dp->next;
        if (dp == districts)
        {
            break;
        }
    }
    return nullptr;
}

// 0x0091BDC0 (:453)
bool territory::zone_v_mask(unsigned short gid)
{
    for (int i = 0; i < num_v_mask_zones; i++)
    {
        if (gid == vertical_mask_zones[i])
        {
            return true;
        }
    }
    return false;
}

// 0x0091BDF0 (:467)
bool territory::zone_h_mask(unsigned short gid)
{
    for (int i = 0; i < num_h_mask_zones; i++)
    {
        if (gid == horiz_mask_zones[i])
        {
            return true;
        }
    }
    return false;
}

// ---- system
// 0x00922E80 (:487)
void territory_shared_system_dlc_shutdown()
{
    District_name_pool.set_thread_ownership();
    District_thumbnail_name_pool.set_thread_ownership();
    District_radio_line_name_pool.set_thread_ownership();
    District_name_pool.restore_to_mark(0);
    District_thumbnail_name_pool.restore_to_mark(0);
    District_radio_line_name_pool.restore_to_mark(0);
    Dlc_territories.clear();
    Dlc_territory_shared_inited = false;
}

// 0x009327E0 (:500)
bool territory_shared_system_init(mempool_base* pool, int dlc_id, unsigned char* table_data)
{
    const bool dlc = dlc_id != -1;
    char filename[256];
    District_name_pool.set_thread_ownership();
    District_thumbnail_name_pool.set_thread_ownership();
    District_radio_line_name_pool.set_thread_ownership();
    if (dlc)
    {
        if (!Territory_shared_inited)
        {
            return false;
        }
        if (Dlc_territory_shared_inited)
        {
            return true;
        }
        District_name_pool.mark();
        District_thumbnail_name_pool.mark();
        District_radio_line_name_pool.mark();
        sprintf(filename, "dlc%02d_%s", dlc_id, "territories.xtbl");
        if (!cf_exist(filename))
        {
            return true;
        }
    }
    else
    {
        if (Territory_shared_inited)
        {
            return true;
        }
        District_name_pool.clear();
        District_thumbnail_name_pool.clear();
        District_radio_line_name_pool.clear();
        strncpy(filename, "territories.xtbl", sizeof(filename));
    }
    xml_element* root = table_data == nullptr ? xml_table_parse(filename) : xml_table_parse_from_memory(table_data);
    if (root == nullptr)
    {
        return false;
    }
    for (xml_element* territory_el = xml_find(root, "Territory"); territory_el != nullptr; territory_el = xml_find_next(root, territory_el, "Territory"))
    {
        xml_element* name_el = xml_find(territory_el, "Name");
        if (name_el != nullptr && name_el->text != nullptr)
        {
            parse_territory(territory_el, pool, dlc);
        }
    }
    xml_table_close(root);
    if (dlc)
    {
        Dlc_territory_shared_inited = true;
    }
    else
    {
        Territory_shared_inited = true;
    }
    return true;
}

// 0x009329E0 (:580)
t_district* territory_shared_find_district(int territory_index, int district_index)
{
    if (territory_index == 0 || district_index == 0)
    {
        return nullptr;
    }
    for (territory* tp = Territories.get_first(); tp != nullptr; tp = Territories.get_next(tp))
    {
        if ((int)tp->index == territory_index)
        {
            t_district* dp = tp->districts;
            while (dp != nullptr)
            {
                if ((int)dp->index == district_index)
                {
                    return dp;
                }
                dp = dp->next;
                if (dp == tp->districts)
                {
                    break;
                }
            }
        }
    }
    for (territory* tp = Dlc_territories.get_first(); tp != nullptr; tp = Dlc_territories.get_next(tp))
    {
        if ((int)tp->index == territory_index)
        {
            t_district* dp = tp->districts;
            while (dp != nullptr)
            {
                if ((int)dp->index == district_index)
                {
                    return dp;
                }
                dp = dp->next;
                if (dp == tp->districts)
                {
                    break;
                }
            }
        }
    }
    return nullptr;
}

// 0x0092B4C0 (:615)
territory* territory_shared_find(const char* name)
{
    territory* tp = Territories.get_ptr(name);
    if (tp == nullptr)
    {
        tp = Dlc_territories.get_ptr(name);
    }
    return tp;
}
