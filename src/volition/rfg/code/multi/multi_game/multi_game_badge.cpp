// Original: volition/rfg/code/multi/multi_game/multi_game_badge.cpp
//
// Badges: a 31-bit value packing one field per mp_badges.xtbl entry (num_bits each, in table order): icon fields (a
// bitmap per value) and color scheme fields (three colors per value, applied to the icon fields named in their lists).
// Each value has the level that unlocks it; automatic fields take the highest value the player's level reaches. The
// player's badge is kept in the profile, sent with its game data and drawn by render order. mp_badge_unlocks.xtbl
// lists the unlock groups the experience screens name.
// Part of the multi unity TU (rfg_multi_unity.obj, every multi/ .cpp in path order): the file statics are shared
// by the whole tree, and a shared one is placed in its first user (tools/hdrgen/specs/mp0_stubs.py).

#include "volition/rfg/code/multi/multi_game/multi_game_badge.h"
#include "volition/rfg/code/multi/multi_game/multi_game_xp.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/control/control_map.h"
#include "volition/rfg/code/gamewide/player_profile.h"
#include "volition/vlib/code/memory/rfg_split_mempool.h"
#include "volition/rfg/code/hud/minimap.h"
#include "volition/rfg/code/interface/localization/localization_shared.h"
#include "volition/rfg/code/rfg.h"
#include "volition/render_lib/code/gr/gr.h"
#include "volition/render_lib/code/rl_render_lib.h"
#include "volition/render_lib/code/rl_screen_info.h"
#include "volition/interface_lib/code/bitmap_sheets.h"
#include "volition/interface_lib/code/vint_external.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/os/vconsole.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/vlib/code/util/fastxml.h"
#include "volition/vlib/code/util/string_comp.h"
#include "volition/vlib/code/util/string_util.h"
#include "volition/vlib/code/util/unicode_tools.h"
#include "keen/base/trace.hpp"

#include <stdlib.h>
#include <string.h>
#include <wchar.h>

// 0x0096BC00: the colors are not initialized
multi_badge_color_scheme::multi_badge_color_scheme()
{
}

// 0x009A6FD0: no name (checksum_stri::INVALID), empty pools and field name lists (their constructors)
multi_badge_field::multi_badge_field()
{
    name_crc = checksum_stri::INVALID;
}

// 0x0098C4A0: the pools' memory belongs to the multiplayer allocators
multi_badge_field::~multi_badge_field()
{
}

// ---- file-private types (rfg.pdb)
// size 0xC
struct multi_badge_field_type_info
{
    checksum_stri name_crc; // 0x00
    bool (*parse_field)(xml_element*, multi_badge_field*, rfg_split_mempool&); // 0x04
    void (*render)(const unsigned int&, multi_badge_field*, int, int, float, float); // 0x08
};

// size 0xC
struct multi_badge_unlock_info
{
    char* group_name; // 0x00
    int unlock_level; // 0x04
    multi_badge_unlock_type type; // 0x08
};

// size 0x8
struct unlock_type_info
{
    multi_badge_unlock_type type; // 0x00
    const char* unlock_tag; // 0x04
};

// ---- file-static globals
// The unlock groups' Type tags.
static unlock_type_info Unlock_type_infos[3] = {{MBUT_ICON, "icon"}, {MBUT_EMBLEM, "emblem"}, {MBUT_COLOR_SCHEME, "color"}}; // 0x0167E524
// The local player's experience level (multi_game_xp.cpp keeps it). Moved here from rfg_multi_unity_pending_early.inl.
static unsigned char My_level = 1; // 0x0167F4F8
// dynamic_object_pool<multi_badge_field,1> Multi_badge_fields (0x016925E8) is used here too; placed in multi_game.cpp.
static base_array<multi_badge_unlock_info> Badge_unlock_info; // 0x01692628
// The fields by render order.
static multi_badge_field* Multi_badge_ordered_fields[32]; // 0x02FEB4B8
// bool Multi_badge_render_my_badge (0x02FEB65B) is used here too; placed in multi.cpp.
// unsigned int My_badge (0x02FEB66C) is used here too; placed in multi.cpp.
// The parser and renderer of each field type (multi_badge_field_types order; set by multi_game_badge_system_init).
static multi_badge_field_type_info Multi_badge_field_type_info[2]; // 0x02FEB7B4
// tweak_table_item Tweak_table_item_score_penalty_for_mismatched_dlc (0x02FEB7CC) is registered in
// multi_game_preferences.cpp with its variable.

unsigned int Multi_badges_bitmap_sheet_srid; // 0x02FEB65C

// The fields and their values.
static const int Max_badge_fields = 32;
// The badge's sign bit is never set by a valid badge.
static const unsigned int Badge_invalid_bit = 0x80000000u;

// ---- internal functions
// Port helper: the bits of a field in the badge
static unsigned int multi_badge_field_mask(const multi_badge_field* field_info)
{
    return ((1u << field_info->num_bits) - 1) << field_info->num_previous_bits;
}

// Port helper: the field at an index of the pool (null past its capacity), as rfg.exe inlines it
static multi_badge_field* multi_badge_field_at(int index)
{
    return Multi_badge_fields.get_entry(index);
}

// Port helper: the fields in use (the pool's capacity less its free objects), as rfg.exe inlines it
static int multi_badge_num_fields()
{
    return (int)Multi_badge_fields.get_count(-1);
}

// 0x0094F1D0 (:144): the field's value in the badge (0 without a field). LTCG: field_info in ECX.
static int multi_badge_get_field_value(const unsigned int& badge, const multi_badge_field* field_info)
{
    if (field_info == nullptr)
    {
        return 0;
    }
    return (int)((multi_badge_field_mask(field_info) & badge) >> field_info->num_previous_bits);
}

// 0x0094F200 (:156): a value that fits the field's bits replaces it. LTCG: value in EAX, badge in ESI.
static void multi_badge_set_field_value(unsigned int& badge, const multi_badge_field* field_info, int value)
{
    if (field_info == nullptr || value >= (1 << field_info->num_bits))
    {
        return;
    }
    const unsigned int mask = multi_badge_field_mask(field_info);
    badge &= ~mask;
    badge |= ((unsigned int)value << field_info->num_previous_bits) & mask;
}

// 0x00995A50 (:170)
static const multi_badge_field* multi_badge_get_field_info(checksum_stri field_name_crc)
{
    const int count = multi_badge_num_fields();
    for (int i = 0; i < count; i++)
    {
        const multi_badge_field* field_info = multi_badge_field_at(i);
        if (field_info->name_crc == field_name_crc)
        {
            return field_info;
        }
    }
    return nullptr;
}

// 0x00995AA0 (:182). LTCG: field_name in EAX.
static const multi_badge_field* multi_badge_get_field_info(const char* field_name)
{
    return multi_badge_get_field_info(checksum_stri(field_name, 0));
}

// 0x00979980 (:188): the icon or color scheme of a value (null past the field's values). LTCG: field_value in ECX,
// field_info in EDX.
static const multi_badge_field_entry* multi_badge_get_field_entry_info(int field_value, const multi_badge_field* field_info)
{
    if (field_info->field_type == MBFT_ICON)
    {
        return const_cast<multi_badge_field*>(field_info)->icon_pool.get_entry(field_value);
    }
    if (field_info->field_type == MBFT_COLOR_SCHEME)
    {
        return const_cast<multi_badge_field*>(field_info)->color_scheme_pool.get_entry(field_value);
    }
    return nullptr;
}

// 0x00995AC0 (:213): every automatic field takes the last value (in table order) the level unlocks
static void multi_badge_generate_automatic_fields(unsigned int& badge, int level)
{
    for (int i = 0; i < multi_badge_num_fields(); i++)
    {
        const multi_badge_field* field_info = multi_badge_field_at(i);
        if (!field_info->automatic)
        {
            continue;
        }
        for (int value = 0;; value++)
        {
            const multi_badge_field_entry* entry = multi_badge_get_field_entry_info(value, field_info); // inlined
            if (entry == nullptr || level < entry->level)
            {
                break;
            }
            multi_badge_set_field_value(badge, field_info, value); // inlined
        }
    }
}

// 0x009799C0 (:238): the field's value exists and is unlocked at the level (any with -1). Original: a null field is
// read anyway. LTCG: field_info in EDX, player_level in EDI.
static bool multi_game_badge_field_is_valid(const unsigned int& badge, const multi_badge_field* field_info, int player_level)
{
    int value = 0;
    if (field_info != nullptr)
    {
        value = multi_badge_get_field_value(badge, field_info); // inlined
        if (value < 0)
        {
            return false;
        }
    }
    if (value >= field_info->num_values)
    {
        return false;
    }
    const multi_badge_field_entry* entry = multi_badge_get_field_entry_info(value, field_info); // inlined
    return entry != nullptr && (player_level == -1 || entry->level <= player_level);
}

// 0x0094F240 (:257): the unlock level and the name (a localization tag). LTCG: field_entry_info in EAX, root in EDI.
static void multi_badge_parse_base_field_entry(xml_element* root, multi_badge_field_entry* field_entry_info)
{
    xml_table_get_req_int(field_entry_info->level, root, "level");
    xml_table_get_req_string(field_entry_info->name, 32, root, "name");
}

// 0x009ABE00 (:264): an icon field: its offset, scale and optional clipping region, and its icons (each a bitmap; the
// icons of one level are numbered for their names)
static bool multi_badge_parse_icon(xml_element* root, multi_badge_field* field_info, rfg_split_mempool& pool)
{
    xml_element* icon = xml_table_find_opt(root, "icon");
    if (icon == nullptr)
    {
        return false;
    }
    xml_table_get_req_int(field_info->icon_offset_x, icon, "offset_x");
    xml_table_get_req_int(field_info->icon_offset_y, icon, "offset_y");
    xml_table_get_req_float(field_info->icon_scale, icon, "scale");
    field_info->icon_defines_clipping_region = false;
    xml_element* clipping = xml_table_find_opt(icon, "clipping_region");
    if (clipping != nullptr)
    {
        field_info->icon_defines_clipping_region = true;
        xml_table_get_req_int(field_info->icon_clipping_offset_x, clipping, "clipping_offset_x");
        xml_table_get_req_int(field_info->icon_clipping_offset_y, clipping, "clipping_offset_y");
        xml_table_get_req_int(field_info->icon_clipping_width, clipping, "clipping_width");
        xml_table_get_req_int(field_info->icon_clipping_height, clipping, "clipping_height");
    }
    xml_element* icons = xml_table_find_req(icon, "icons");
    if (icons == nullptr)
    {
        return false;
    }
    field_info->icon_pool.init(pool, (unsigned int)xml_table_count(icons, "icon_props"));
    field_info->icon_previous_level = -1;
    field_info->icon_count_at_this_level = 0;
    for (xml_element* props = xml_find(icons, "icon_props"); props != nullptr; props = xml_find_next(icons, props, "icon_props"))
    {
        multi_badge_icon_info* info = field_info->icon_pool.alloc(0);
        field_info->num_values++;
        info->bitmap_id = vint_bms_load(xml_table_get_req_string_ref(props, "bitmap_name"));
        multi_badge_parse_base_field_entry(props, info);
        if (info->level == field_info->icon_previous_level)
        {
            field_info->icon_count_at_this_level++;
        }
        else
        {
            field_info->icon_count_at_this_level = 1;
        }
        field_info->icon_previous_level = info->level;
        info->name_number = field_info->icon_count_at_this_level;
    }
    return true;
}

// 0x00995BB0 (:321): the list's field names. LTCG: list_name in EAX, root in ECX.
static void multi_badge_parse_color_scheme_field_names(xml_element* root, const char* list_name, farray<checksum_stri,32>& crc_array)
{
    crc_array.clear(); // inlined (num = 0)
    xml_element* list = xml_table_find_req(root, list_name);
    if (list == nullptr)
    {
        return;
    }
    for (xml_element* name = xml_find(list, "field_name"); name != nullptr; name = xml_find_next(list, name, "field_name"))
    {
        checksum_stri crc(name->text, 0);
        crc_array.add(crc);
    }
}

// 0x009ABF90 (:335): a color scheme field: the icon fields each of its colors applies to, and its schemes
static bool multi_badge_parse_color_scheme(xml_element* root, multi_badge_field* field_info, rfg_split_mempool& pool)
{
    xml_element* scheme = xml_table_find_opt(root, "color_scheme");
    if (scheme == nullptr)
    {
        return false;
    }
    multi_badge_parse_color_scheme_field_names(scheme, "primary_color_fields", field_info->color_scheme_primary_field_names);
    multi_badge_parse_color_scheme_field_names(scheme, "secondary_color_fields", field_info->color_scheme_secondary_field_names);
    multi_badge_parse_color_scheme_field_names(scheme, "tertiary_color_fields", field_info->color_scheme_tertiary_field_names);
    xml_element* schemes = xml_table_find_req(scheme, "color_schemes");
    if (schemes == nullptr)
    {
        return false;
    }
    field_info->color_scheme_pool.init(pool, (unsigned int)xml_table_count(schemes, "color_scheme_props"));
    for (xml_element* props = xml_find(schemes, "color_scheme_props"); props != nullptr; props = xml_find_next(schemes, props, "color_scheme_props"))
    {
        multi_badge_color_scheme* info = field_info->color_scheme_pool.alloc(0);
        field_info->num_values++;
        xml_table_get_req_color(info->primary_color, props, "primary_color");
        xml_table_get_req_color(info->secondary_color, props, "secondary_color");
        xml_table_get_req_color(info->tertiary_color, props, "tertiary_color");
        multi_badge_parse_base_field_entry(props, info);
    }
    return true;
}

// Port helper: the list holds the name
static bool multi_badge_list_has(const farray<checksum_stri,32>& list, checksum_stri name)
{
    for (int i = 0; i < list.size(); i++)
    {
        if (list[i] == name)
        {
            return true;
        }
    }
    return false;
}

// 0x00995C20 (:371): the color the badge's color schemes give the named icon field (the first scheme field in table
// order naming it; white without one). Original: a scheme value out of range ends the search.
static void multi_badge_get_color_from_scheme(color& output_color, const unsigned int& badge, checksum_stri field_name_crc)
{
    output_color = color::WHITE;
    for (int i = 0; i < multi_badge_num_fields(); i++)
    {
        multi_badge_field* field_info = multi_badge_field_at(i);
        if (field_info->field_type != MBFT_COLOR_SCHEME)
        {
            continue;
        }
        const multi_badge_color_scheme* scheme = field_info->color_scheme_pool.get_entry(multi_badge_get_field_value(badge, field_info)); // inlined
        if (scheme == nullptr)
        {
            return;
        }
        if (multi_badge_list_has(field_info->color_scheme_primary_field_names, field_name_crc))
        {
            output_color = scheme->primary_color;
            return;
        }
        if (multi_badge_list_has(field_info->color_scheme_secondary_field_names, field_name_crc))
        {
            output_color = scheme->secondary_color;
            return;
        }
        if (multi_badge_list_has(field_info->color_scheme_tertiary_field_names, field_name_crc))
        {
            output_color = scheme->tertiary_color;
            return;
        }
    }
}

// 0x00995D60 (:412): the icon of the field's value in its scheme color, offset and scaled (clipped to its region)
static void multi_badge_render_icon(const unsigned int& badge, multi_badge_field* field_info, int x, int y, float alpha, float scale)
{
    const multi_badge_icon_info* icon = field_info->icon_pool.get_entry(multi_badge_get_field_value(badge, field_info)); // inlined
    if (icon == nullptr)
    {
        return;
    }
    color icon_color = color::WHITE;
    multi_badge_get_color_from_scheme(icon_color, badge, field_info->name_crc);
    gr_set_color(icon_color.red, icon_color.green, icon_color.blue, (int)(alpha * 255.0f));
    scale = field_info->icon_scale * scale;
    if (field_info->icon_defines_clipping_region)
    {
        const int top = (int)((float)field_info->icon_clipping_offset_y * scale) + y;
        const int left = (int)((float)field_info->icon_clipping_offset_x * scale) + x;
        gr_set_scissor_region(left, top, (int)((float)field_info->icon_clipping_width * scale) + left, (int)((float)field_info->icon_clipping_height * scale) + top, 0);
    }
    const float offset_x = (float)field_info->icon_offset_x * scale;
    const float offset_y = (float)field_info->icon_offset_y * scale;
    const gr_state& state = vint_render_get_stencil_mode() == VINT_STENCIL_TEST ? GM_FILTER_STENCIL_TEST : GM_FILTER;
    minimap_render_bitmap(icon->bitmap_id, (float)(x + (int)offset_x), (float)(y + (int)offset_y), 0.0f, scale, scale, state);
}

// 0x0094F270 (:455): qsort: by render order
static int multi_badge_field_sort(const void* p1, const void* p2)
{
    const multi_badge_field* field1 = *(const multi_badge_field* const*)p1;
    const multi_badge_field* field2 = *(const multi_badge_field* const*)p2;
    if (field1->render_order < field2->render_order)
    {
        return -1;
    }
    return field2->render_order < field1->render_order ? 1 : 0;
}

// 0x009AC0C0 (:469): with its automatic fields, a valid badge becomes ours. LTCG: new_badge in EAX.
static void multi_badge_validate_and_set_my_badge(const unsigned int& new_badge)
{
    unsigned int badge = new_badge;
    multi_badge_generate_automatic_fields(badge, My_level);
    if (multi_game_badge_is_valid(badge, -1))
    {
        My_badge = badge;
    }
}

// 0x0094F2A0 (:480): the unlock group's type by its tag (icon without a match). LTCG: unlock_group_type in EDI.
static multi_badge_unlock_type get_badge_unlock_type(const char* unlock_group_type)
{
    for (int i = 0; i < 3; i++)
    {
        if (v_stricmp(unlock_group_type, Unlock_type_infos[i].unlock_tag) == 0)
        {
            return Unlock_type_infos[i].type;
        }
    }
    return Unlock_type_infos[0].type;
}

// 0x00995F60 (:492): mp_badge_unlocks.xtbl: each group's name (copied into the pool), unlock level and type
static void parse_badge_unlocks(rfg_split_mempool& pool)
{
    xml_element* root = xml_table_parse("mp_badge_unlocks.xtbl");
    if (root == nullptr)
    {
        return;
    }
    const int count = xml_table_count(root, "entry");
    Badge_unlock_info.init((multi_badge_unlock_info*)pool.alloc(count * sizeof(multi_badge_unlock_info), 0), count, 0); // inlined
    for (xml_element* entry = xml_find(root, "entry"); entry != nullptr; entry = xml_find_next(root, entry, "entry"))
    {
        const char* name = xml_table_get_req_string_ref(entry, "Name");
        int unlock_level;
        xml_table_get_req_int(unlock_level, entry, "Unlock_Level");
        const multi_badge_unlock_type type = get_badge_unlock_type(xml_table_get_req_string_ref(entry, "Type")); // inlined
        char* group_name = (char*)pool.alloc((unsigned int)strlen(name) + 1, 0);
        strcpy(group_name, name);
        multi_badge_unlock_info info;
        info.group_name = group_name;
        info.unlock_level = unlock_level;
        info.type = type;
        Badge_unlock_info.add(info); // inlined
    }
    xml_table_close(root);
}

// 0x009AC100 (:536): console: each field in table order takes the next integer argument (a value that does not fit
// keeps the field); a valid result becomes our badge
void unused_dcf_mp_badge_set()
{
    for (int i = 0; i < multi_badge_num_fields(); i++)
    {
        // 8: an integer, optional
        if (!console_get_arg(8, 1))
        {
            return;
        }
        const multi_badge_field* field_info = multi_badge_field_at(i);
        unsigned int badge = My_badge;
        multi_badge_set_field_value(badge, field_info, Console_arg_int); // inlined
        if (multi_game_badge_is_valid(badge, -1))
        {
            My_badge = badge;
        }
        else
        {
            console_printf("Value '%d' is invalid for field '%d'", Console_arg_int, i + 1);
        }
    }
}

// 0x009F0800 (:555): console: mp_badges.xtbl again
void unused_dcf_mp_badge_reload_table()
{
    if (Multiplayer)
    {
        cf_refresh_file_size("mp_badges.xtbl");
        multi_game_badge_system_init(MP_allocators_mempool);
    }
}

// 0x009DCD50 (:569): the unlock groups; mp_badges.xtbl's fields (32 at most; each takes the bits after the previous
// ones, a field no type parses is dropped) sorted by render order; our badge from the profile when valid
void multi_game_badge_system_init(rfg_split_mempool& pool)
{
    parse_badge_unlocks(pool);
    Multi_badge_fields.init(pool, Max_badge_fields);
    Multi_badge_field_type_info[0].parse_field = multi_badge_parse_icon;
    Multi_badge_field_type_info[0].render = multi_badge_render_icon;
    Multi_badge_field_type_info[1].parse_field = multi_badge_parse_color_scheme;
    Multi_badge_field_type_info[1].render = nullptr;
    xml_element* root = xml_table_parse("mp_badges.xtbl");
    if (root != nullptr)
    {
        int num_previous_bits = 0;
        for (xml_element* entry = xml_find(root, "entry"); entry != nullptr; entry = xml_find_next(root, entry, "entry"))
        {
            multi_badge_field* field_info = Multi_badge_fields.alloc(0);
            if (field_info == nullptr)
            {
                break;
            }
            field_info->name_crc = checksum_stri(xml_table_get_req_string_ref(entry, "Name"), 0);
            xml_table_get_req_int(field_info->num_bits, entry, "num_bits");
            field_info->num_previous_bits = num_previous_bits;
            xml_table_get_req_int(field_info->render_order, entry, "render_order");
            xml_table_get_req_bool(field_info->automatic, entry, "automatic");
            xml_element* types = xml_table_find_req(entry, "field_types");
            if (types == nullptr)
            {
                continue;
            }
            bool parsed = false;
            for (int type = MBFT_ICON; type < NUM_MULTI_BADGE_FIELD_TYPES; type++)
            {
                if (Multi_badge_field_type_info[type].parse_field != nullptr && Multi_badge_field_type_info[type].parse_field(types, field_info, pool))
                {
                    num_previous_bits += field_info->num_bits;
                    field_info->field_type = (multi_badge_field_types)type;
                    parsed = true;
                    break;
                }
            }
            if (!parsed)
            {
                Multi_badge_fields.free(field_info);
            }
        }
        xml_table_close(root);
    }
    const int count = multi_badge_num_fields();
    for (int i = 0; i < count; i++)
    {
        Multi_badge_ordered_fields[i] = multi_badge_field_at(i);
    }
    qsort(Multi_badge_ordered_fields, count, sizeof(multi_badge_field*), multi_badge_field_sort);
    My_badge = 0;
    player_profile_data* profile = player_profile_get("multi_badge_init", Player_controller_index != 0xFFFFFFFFu ? (int)Player_controller_index : 0);
    if (profile != nullptr)
    {
        multi_badge_validate_and_set_my_badge(profile->mp_badge); // inlined
    }
}

// 0x009DCF90 (:662)
void multi_game_badge_system_shutdown()
{
    Multi_badge_fields.object_pool_server<multi_badge_field>::destroy();
}

// 0x009AC1E0 (:669): the debug display of our badge
void multi_game_badge_system_render()
{
    if (Multi_badge_render_my_badge)
    {
        // Magic numbers: at (150, 150), unscaled.
        multi_game_badge_render(My_badge, 150, 150, 1.0f, 1.0f);
    }
}

// 0x009AC220 (:676): a valid badge becomes ours; ours goes to the profile
void multi_game_save_badge_to_profile(const unsigned int& badge)
{
    multi_badge_validate_and_set_my_badge(badge); // inlined
    player_profile_data* profile = player_profile_get("multi_game_save_badge_to_profile", Player_controller_index != 0xFFFFFFFFu ? (int)Player_controller_index : 0);
    if (profile != nullptr)
    {
        profile->mp_badge = My_badge;
    }
}

// 0x00996100 (:686): the next value of the field (wrapping)
void multi_game_badge_increment_field(unsigned int& badge, const char* field_name)
{
    const multi_badge_field* field_info = multi_badge_get_field_info(checksum_stri(field_name, 0)); // inlined
    if (field_info != nullptr)
    {
        multi_badge_set_field_value(badge, field_info, (multi_badge_get_field_value(badge, field_info) + 1) % field_info->num_values); // inlined
    }
}

// 0x00996170 (:701): the previous value of the field (wrapping)
void multi_game_badge_decrement_field(unsigned int& badge, const char* field_name)
{
    const multi_badge_field* field_info = multi_badge_get_field_info(checksum_stri(field_name, 0)); // inlined
    if (field_info == nullptr)
    {
        return;
    }
    int value = multi_badge_get_field_value(badge, field_info) - 1; // inlined
    while (value < 0)
    {
        value += field_info->num_values;
    }
    multi_badge_set_field_value(badge, field_info, value);
}

// 0x009961E0 (:718): the name of the field's value (localized when the tag exists); an icon adds its number
void multi_game_badge_get_field_value_name(wchar_t* buffer, int buffer_size, const unsigned int& badge, const char* field_name)
{
    buffer[0] = L'\0';
    multi_badge_field* field_info = const_cast<multi_badge_field*>(multi_badge_get_field_info(checksum_stri(field_name, 0))); // inlined
    if (field_info == nullptr)
    {
        return;
    }
    const int value = multi_badge_get_field_value(badge, field_info); // inlined
    const multi_badge_field_entry* entry = nullptr;
    if (field_info->field_type == MBFT_ICON)
    {
        entry = field_info->icon_pool.get_entry(value);
    }
    else if (field_info->field_type == MBFT_COLOR_SCHEME)
    {
        entry = field_info->color_scheme_pool.get_entry(value);
    }
    if (entry == nullptr)
    {
        return;
    }
    if (localization_does_tag_exist(entry->name))
    {
        wcsncpy(buffer, localization_get_display_string_req(entry->name, nullptr), buffer_size);
        buffer[buffer_size - 1] = L'\0';
    }
    else
    {
        copy_to_wide(buffer, entry->name, buffer_size);
    }
    if (field_info->field_type == MBFT_ICON)
    {
        wchar_t number[8];
        _snprintf_safe(number, 8, " %d", static_cast<const multi_badge_icon_info*>(entry)->name_number);
        wcsncat(buffer, number, 8);
    }
}

// 0x00996300 (:755): ours, with its automatic fields brought up to our level
void multi_game_badge_get_my_badge(unsigned int& output)
{
    multi_badge_generate_automatic_fields(My_badge, My_level);
    output = My_badge;
}

// 0x00996330 (:766): no sign bit, and every field (pool order) valid at the level
bool multi_game_badge_is_valid(const unsigned int& badge, int player_level)
{
    if ((badge & Badge_invalid_bit) != 0)
    {
        return false;
    }
    const int count = multi_badge_num_fields();
    for (int i = 0; i < count; i++)
    {
        if (!multi_game_badge_field_is_valid(badge, multi_badge_field_at(i), player_level))
        {
            return false;
        }
    }
    return true;
}

// 0x009963A0 (:786)
bool multi_game_badge_field_is_valid(const unsigned int& badge, const char* field_name, int player_level)
{
    const multi_badge_field* field_info = multi_badge_get_field_info(checksum_stri(field_name, 0)); // inlined
    if (field_info == nullptr)
    {
        return false;
    }
    return multi_game_badge_field_is_valid(badge, field_info, player_level);
}

// 0x009963F0 (:794): a valid badge's fields by render order, unclipped (the scissor region and the minimap's
// scissor flag come back afterwards)
void multi_game_badge_render(const unsigned int& badge, int x, int y, float alpha, float scale)
{
    if (!multi_game_badge_is_valid(badge, -1))
    {
        return;
    }
    int left = 0;
    int top = 0;
    int right = 0;
    int bottom = 0;
    gr_get_scissor_region(left, top, right, bottom);
    const bool minimap_scissor = Minimap_scissor_enabled;
    Minimap_scissor_enabled = true;
    const rl_screen_info* screen = rl_render_lib::get_instance()->get_screen_info();
    gr_set_scissor_region(0, 0, screen->get_screen_width(), screen->get_screen_height(), 0); // m_current_width/height, inlined
    for (int i = 0; i < multi_badge_num_fields(); i++)
    {
        multi_badge_field* field_info = Multi_badge_ordered_fields[i];
        if (Multi_badge_field_type_info[field_info->field_type].render != nullptr)
        {
            Multi_badge_field_type_info[field_info->field_type].render(badge, field_info, x, y, alpha, scale);
        }
    }
    gr_set_scissor_region(left, top, right, bottom, 0);
    Minimap_scissor_enabled = minimap_scissor;
}

// 0x0094F2D0 (:834): BE
void multi_game_badge_write_to_packet(const unsigned int& badge, unsigned char* packet_data, unsigned short& packet_size)
{
    write_u32_be(packet_data, packet_size, badge);
}

// 0x0094F340 (:842)
void multi_game_badge_read_from_packet(unsigned int& badge, const unsigned char* packet_data, int& packet_offset)
{
    badge = read_u32_be(packet_data, packet_offset);
}

// 0x0094F3B0 (:852): the experience unlock types of the badge's icons, emblems and color schemes (4, 5, 6). LTCG:
// xp_unlock_type in ECX, badge_unlock_type in EAX.
static bool is_same_unlock_type(multi_xp_unlock_types xp_unlock_type, multi_badge_unlock_type badge_unlock_type)
{
    switch (badge_unlock_type)
    {
    case MBUT_ICON:
        return xp_unlock_type == 4;
    case MBUT_EMBLEM:
        return xp_unlock_type == 5;
    case MBUT_COLOR_SCHEME:
        return xp_unlock_type == 6;
    default:
        return false;
    }
}

// 0x00979A30 (:867): the unlock groups of an experience unlock type (none when they would not all fit)
void multi_game_badge_get_xp_unlocks(int& num_xp_badge_unlocks, xp_badge_unlock_info* unlock_infos, int xp_unlock_type, int max_unlock_infos)
{
    num_xp_badge_unlocks = 0;
    if (Badge_unlock_info.size() > max_unlock_infos)
    {
        return;
    }
    for (int i = 0; i < Badge_unlock_info.size(); i++)
    {
        if (is_same_unlock_type((multi_xp_unlock_types)xp_unlock_type, Badge_unlock_info[i].type)) // inlined
        {
            unlock_infos[num_xp_badge_unlocks].unlock_info_index = (unsigned int)i;
            unlock_infos[num_xp_badge_unlocks].unlock_level = Badge_unlock_info[i].unlock_level;
            num_xp_badge_unlocks++;
        }
    }
}

// 0x00979AC0 (:881)
const char* multi_game_badge_get_xp_unlock_group_name(unsigned int index)
{
    if (index >= (unsigned int)Badge_unlock_info.size())
    {
        return "unknown";
    }
    return Badge_unlock_info[(int)index].group_name;
}
