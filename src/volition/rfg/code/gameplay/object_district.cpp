// Original: volition/rfg/code/gameplay/object_district.cpp
//
// The district objects of a territory (one per t_district of the territory table, plus the "Default" district), the grid
// map from a world grid cell to its district (District_map), which the time of day uses to pick its parameter sets,
// saving and loading the district state, control and morale changes, liberation and radio lines, and the marauder grid
// flags. Functions are not in PDB line order: gameflow.md GF3's are at the end.

#include "volition/rfg/code/gameplay/object_district.h"

#include "volition/render_lib/code/rl_color.h"
#include "volition/rfg/code/gameplay/activities/activity_vrail.h"
#include "volition/rfg/code/gameplay/gameflow/gfm_control_morale.h"
#include "volition/rfg/code/gameplay/gameflow/gfm_tech_level.h"
#include "volition/rfg/code/gameplay/mission_info.h"
#include "volition/rfg/code/gameplay/missions.h"
#include "volition/rfg/code/gameplay/upgrades.h"
#include "volition/rfg/code/objects/movers/object_mover.h"
#include "volition/rfg/code/objects/object_convoy_end_point.h"
#include "volition/rfg/code/objects/object_zone.h"
#include "volition/rfg/code/scripting/dialogue_manager.h"
#include "volition/rfg/code/scripting/public_address.h"
#include "volition/rfg/code/gameplay/mission_info.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/rfg/code/gamewide/downloadable_content.h"
#include "volition/rfg/code/gamewide/stats.h"
#include "volition/rfg/code/level/rfg_zonex_shared.h"
#include "volition/rfg/code/level/territory_shared.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice_lines.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/object_prop_names_shared.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/memory/object_pool.h"
#include "volition/vlib/code/util/string_comp.h"
#include "volition/vlib/code/util/string_util.h"

#include "keen/base/trace.hpp"

#include <stdio.h>
#include <stdlib.h>

static const int DISTRICT_POOL_SIZE = 12; // districtpool
static const int DISTRICT_MAP_SIZE = 257; // District_map: the 16x16 world grid (index y * 16 + x) and one spare entry

// ---- file-static globals
static unsigned int Default_district_handle = 0xFFFFFFFF; // 0x0165153C
// District_map belongs to mission_info.cpp (a file static of the same unity build, not compiled yet); it lives here.
unsigned int District_map[DISTRICT_MAP_SIZE]; // 0x022267D8
static bool Loading_data; // 0x02229678
static object_pool<district, DISTRICT_POOL_SIZE, 1> districtpool; // 0x02229C40
static obj_class_instantiator district_instantiator("district", alloc_district, types_district, bpo_index_district, classname_district, district::pool_remaining, 0x1130, DISTRICT_POOL_SIZE); // 0x0224CA6C

// ---- internal functions
// 0x0078EE70 (:52): the first district object (the default district, created first).
static district* get_default_district()
{
    return World.get_object<district>(0);
}

// 0x00774D40 (:60)
unsigned int get_default_district_handle()
{
    if (Default_district_handle != 0xFFFFFFFF)
    {
        object* objp = World.object_from_handle(Default_district_handle);
        if (objp != nullptr && objp->get_obj_type() == OT_DISTRICT)
        {
            return Default_district_handle;
        }
    }
    keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
    return 0xFFFFFFFF;
}

// 0x0075EC20 (:78)
static int grid_index_compare(const void* arg1, const void* arg2)
{
    const unsigned short gid1 = *(const unsigned short*)arg1;
    const unsigned short gid2 = *(const unsigned short*)arg2;
    if (gid1 < gid2)
    {
        return -1;
    }
    return gid2 < gid1 ? 1 : 0;
}

// 0x0075EC40 (:91)
static void clear_all_districts()
{
    for (int i = 0; i < DISTRICT_MAP_SIZE; i++)
    {
        District_map[i] = 0xFFFFFFFF;
    }
}

// 0x0078F430 (:815)
static bool parse_all_districts(territory* tp)
{
    t_district* dp = tp->districts;
    while (dp != nullptr)
    {
        object_district_props props(dp->control, dp->control_max, dp->morale, dp->morale_max, dp->liberated, false, dp->flags, ZERO_VECTOR, IDENTITY_MATRIX);
        props.set(OBJ_PROP_DISPLAY_NAME, tp->name);
        district* distp = World.create_object_global<district>(props, 0xFFFFFFFF);
        if (distp == nullptr)
        {
            return false;
        }
        distp->set_persistent(true);
        distp->set_name(dp->name);
        distp->set_gid_list(dp->list);
        distp->set_grid_color(dp->col);
        rl_color_float tint;
        tint.red = (float)dp->vfx_tint.red / 255.0f;
        tint.green = (float)dp->vfx_tint.green / 255.0f;
        tint.blue = (float)dp->vfx_tint.blue / 255.0f;
        tint.alpha = 1.0f;
        distp->set_vfx_tint(tint.from_sRGB_to_linear());
        distp->set_district_index(dp->index);
        if (dp->localized_name_hash != 0)
        {
            distp->set_localized_name_hash(dp->localized_name_hash);
        }
        distp->set_liberated_tech_level(dp->tech_level);
        distp->set_liberated_marauder_level(dp->marauder_level);
        distp->set_liberated_edf_level(dp->edf_level);
        distp->set_minimum_edf_progression(dp->edf_progression_level);
        distp->set_liberated_radio_line(dp->liberated_radio_line == nullptr ? INVALID_VOICE_LINE_HANDLE : voice_line_handle_from_name(dp->liberated_radio_line));
        distp->set_capstone_radio_lines();
        distp->set_loading_screen_images(dp->load_screen_images, dp->num_load_screen_images);
        dp = dp->next;
        if (dp == tp->districts)
        {
            break;
        }
    }
    return true;
}

// ---- object_district_props
// 0x00768F80
object_district_props::object_district_props(float control, float control_max, float morale, float morale_max, bool liberated, bool needs_to_play_radio_line, district_flags flags, const vector& _position, const matrix& _orient)
    : obj_prop_block(_position, _orient, nullptr)
{
    set(DISTRICT_PROP_CONTROL, control);
    set(DISTRICT_PROP_CONTROL_MAX, control_max);
    set(DISTRICT_PROP_MORALE, morale);
    set(DISTRICT_PROP_MORALE_MAX, morale_max);
    set(DISTRICT_LIBERATED, liberated);
    set(DISTRICT_LIBERATED_PLAY_LINE, needs_to_play_radio_line);
    set(DISTRICT_FLAGS, flags);
}

// 0x0075F230
object_district_props::~object_district_props()
{
}

// ---- district (header inlines)
// 0x00743B90 (.h:38)
const object_type district::get_type_static()
{
    return OT_DISTRICT;
}

// 0x00743BA0 (.h:38)
const object_sub_type district::get_subtype_static()
{
    return OT_SUB_UNDEFINED;
}

// 0x00754380 (.h:38)
const char* district::get_classname_static()
{
    return "district";
}

// 0x00754390 (.h:38)
int district::get_bpo_index_static()
{
    return 1;
}

// 0x0078EF40 (.h:38)
const char* district::get_classname() const
{
    return "district";
}

// 0x005DDB70 (.h:88)
bool district::is_liberated() const
{
    return liberated;
}

// 0x0078F860 (:946): the districts the player has liberated (a save's summary).
unsigned int object_district_num_liberated()
{
    unsigned int count = 0;
    const int num_districts = World.get_num_objects<district>();
    for (int i = 0; i < num_districts; i++)
    {
        if (World.get_object<district>(i)->is_liberated())
        {
            count++;
        }
    }
    return count;
}

// 0x00743BB0 (.h:109)
const rl_color_float& district::get_vfx_tint() const
{
    return m_vfx_tint;
}

// 0x0091B560 (.h:118)
unsigned char district::get_district_index() const
{
    return district_index;
}

// 0x007543D0 (.h:110)
void district::set_vfx_tint(const rl_color_float& vfx_tint)
{
    m_vfx_tint = vfx_tint;
}

// 0x007543F0 (.h:113)
void district::set_localized_name_hash(unsigned int hash)
{
    localized_name_hash = hash;
}

// 0x00754410 (.h:117)
void district::set_district_index(unsigned char index)
{
    district_index = index;
}

// ---- class registration (:117)
// 0x0075ECA0
unsigned int district::get_classname_hash()
{
    return district_instantiator.this_hash;
}

// 0x00774D90
int district::pool_remaining()
{
    return districtpool.get_free();
}

// 0x0078EEF0
object* alloc_district()
{
    return districtpool.alloc(0);
}

// 0x0075EC80
int bpo_index_district()
{
    return 1;
}

// 0x0075EC70
void types_district(signed char& type, signed char& subtype)
{
    type = OT_DISTRICT;
    subtype = OT_SUB_UNDEFINED;
}

// 0x0079E440
void district::pool_free()
{
    districtpool.free(this);
}

// 0x0075EC90
const char* classname_district()
{
    return "district";
}

// ---- district
// 0x0078EF00 (:118)
district::district()
{
}

// 0x0078EF50
district::~district()
{
}

// 0x0078EF70 (:130)
bool district::serialize(obj_prop_block& pblock, bool load)
{
    if (!object::serialize(pblock, load))
    {
        return false;
    }
    if (load)
    {
        min_edf_progression_level = 0;
        obj_prop* prop;
        // Each property is taken only with the expected size (4 for the floats, 1 for the flags).
        if (pblock.get_internal(prop, DISTRICT_PROP_CONTROL, 5) && prop->size == 4)
        {
            control = *(const float*)(prop + 1);
        }
        if (pblock.get_internal(prop, DISTRICT_PROP_MORALE, 5) && prop->size == 4)
        {
            morale = *(const float*)(prop + 1);
        }
        if (pblock.get_internal(prop, DISTRICT_PROP_CONTROL_MAX, 5) && prop->size == 4)
        {
            control_max = *(const float*)(prop + 1);
        }
        if (pblock.get_internal(prop, DISTRICT_PROP_MORALE_MAX, 5) && prop->size == 4)
        {
            morale_max = *(const float*)(prop + 1);
        }
        if (pblock.get_internal(prop, DISTRICT_LIBERATED, 5) && prop->size == 1)
        {
            liberated = *(const bool*)(prop + 1);
        }
        if (pblock.get_internal(prop, DISTRICT_LIBERATED_PLAY_LINE, 5) && prop->size == 1)
        {
            needs_to_play_radio_line = *(const bool*)(prop + 1);
        }
        if (pblock.get_internal(prop, DISTRICT_FLAGS, 5) && prop->size == 1)
        {
            flags = *(const district_flags*)(prop + 1);
        }
        m_vfx_tint = rl_color_float(1.0f, 1.0f, 1.0f, 1.0f);
    }
    return true;
}

// 0x0075ECB0 (:180): districts are never saved with the world (their state has its own save block).
obj_serialize_mode district::serialize_mode()
{
    return OSM_NONE;
}

// 0x0075EFB0 (:365)
void district::set_liberated_tech_level(float level)
{
    liberated_tech_level = level;
}

// 0x0075EFD0 (:370)
void district::set_liberated_marauder_level(int level)
{
    liberated_marauder_level = level;
}

// 0x0075EFE0 (:375)
void district::set_liberated_edf_level(int level)
{
    liberated_edf_level = level;
}

// 0x0075EFF0 (:380)
void district::set_liberated_radio_line(voice_line_handle radio_line)
{
    liberated_radio_line_handle = radio_line;
}

// 0x0075F0A0 (:444)
void district::set_minimum_edf_progression(int level)
{
    min_edf_progression_level = level;
}

// 0x0075F0E0 (:529)
void district::set_grid_color(const color& col_to_set)
{
    col = col_to_set;
}

// 0x0075F1D0 (:582)
void district::set_loading_screen_images(const char** image_names, int num_images)
{
    load_screen_images[0] = nullptr;
    for (int i = 0; i < num_images; i++)
    {
        load_screen_images[i] = image_names[i];
    }
    num_load_screen_images = (unsigned char)num_images;
}

// 0x0075F000 (:385)
void district::set_capstone_radio_lines()
{
    capstone_line_1 = INVALID_VOICE_LINE_HANDLE;
    capstone_line_2 = INVALID_VOICE_LINE_HANDLE;
    if (flags.play_capstone_unlocked_lines)
    {
        char line_name[64];
        sprintf(line_name, "ext_capstone_unlock_%s_rfc_01", get_name());
        capstone_line_1 = voice_line_handle_from_name(line_name);
        sprintf(line_name, "ext_capstone_unlock_%s_rfc_02", get_name());
        capstone_line_2 = voice_line_handle_from_name(line_name);
    }
}

// 0x0078F140 (:460): also enters the district into District_map for each of its cells.
bool district::set_gid_list(const farray<grid_info,21>& list_to_set)
{
    grid_ids.clear();
    const int count = list_to_set.size();
    for (int i = 0; i < count; i++)
    {
        grid_ids.add(list_to_set[i]);
        const unsigned short gid = list_to_set[i].gid;
        District_map[((gid >> 8) & 0xF) * 16 + (gid >> 12)] = get_handle();
    }
    // The grid infos are sorted by their first two bytes (the gid).
    qsort(grid_ids.elements(), count, sizeof(grid_info), grid_index_compare);
    return true;
}

// 0x00774DD0 (:502)
bool district::encompasses_gid(unsigned char grid_x, unsigned char grid_y) const
{
    const unsigned int key = (unsigned short)(((unsigned short)grid_x << 4 | (unsigned short)(grid_y & 0xF)) << 8);
    // The original searches with an element size of 2, not sizeof(grid_info) (4), so only the first half of the list
    // is seen, interleaved with the flag halves. Kept as in the binary.
    return bsearch(&key, const_cast<farray<grid_info,21>&>(grid_ids).elements(), grid_ids.size(), 2, grid_index_compare) != nullptr;
}

// ---- functions
// 0x0078F200 (:604)
unsigned int get_district_by_hash(unsigned int district_hash)
{
    for (int i = 0; i < World.get_num_objects<district>(); i++)
    {
        district* objp = World.get_object<district>(i);
        if (str_hash_string(objp->get_name()) == district_hash)
        {
            return objp->get_handle();
        }
    }
    return get_default_district_handle();
}

// 0x0078F2C0 (:618)
unsigned int get_district_by_name(const char* district_name)
{
    if (district_name != nullptr)
    {
        for (int i = 0; i < World.get_num_objects<district>(); i++)
        {
            district* objp = World.get_object<district>(i);
            if (v_stricmp(objp->get_name(), district_name) == 0)
            {
                return objp->get_handle();
            }
        }
    }
    return get_default_district_handle();
}

// 0x00774E30 (:658)
unsigned int district_handle_from_grid_location(int x, int y)
{
    const int index = y * 16 + x;
    if (index >= DISTRICT_MAP_SIZE)
    {
        return 0xFFFFFFFF;
    }
    const unsigned int handle = District_map[index];
    return handle == 0xFFFFFFFF ? get_default_district_handle() : handle;
}

// 0x0079E510 (:637): the district the local player is in (the first district without a player).
district* district_local_player()
{
    const player* pp = player_get_local_player();
    if (pp == nullptr)
    {
        return get_default_district();
    }
    return district_get_from_pos(pp->pos);
}

// 0x00774E60 (:674)
unsigned int district_handle_from_pos(const vector& pos)
{
    int grid_x;
    int grid_y;
    if (!rfg_zonex_get_grid_coords(pos, grid_x, grid_y))
    {
        return 0xFFFFFFFF;
    }
    const int index = grid_y * 16 + grid_x;
    if (index >= DISTRICT_MAP_SIZE)
    {
        return 0xFFFFFFFF;
    }
    const unsigned int handle = District_map[index];
    return handle == 0xFFFFFFFF ? get_default_district_handle() : handle;
}

// 0x0075F220 (:684)
unsigned int district_get_default()
{
    return Default_district_handle;
}

// 0x0078F3C0 (:692): the district at pos, else the first district.
district* district_get_from_pos(const vector& pos)
{
    int grid_x;
    int grid_y;
    if (rfg_zonex_get_grid_coords(pos, grid_x, grid_y))
    {
        const int index = grid_y * 16 + grid_x;
        if (index < DISTRICT_MAP_SIZE)
        {
            unsigned int handle = District_map[index];
            if (handle == 0xFFFFFFFF)
            {
                handle = get_default_district_handle();
            }
            if (handle != 0xFFFFFFFF)
            {
                object* objp = World.object_from_handle(handle);
                if (objp != nullptr && objp->get_obj_type() == OT_DISTRICT)
                {
                    return (district*)objp;
                }
            }
        }
    }
    return get_default_district();
}

// 0x00775030 (:780)
bool object_district_preload_default_district()
{
    clear_all_districts();
    object_district_props props(0.0f, 0.0f, 0.0f, 1000.0f, false, false, district_flags(), ZERO_VECTOR, IDENTITY_MATRIX);
    district* objp = World.create_object_global<district>(props, 0xFFFFFFFF);
    if (objp == nullptr)
    {
        return false;
    }
    objp->set_persistent(true);
    objp->set_name("Default");
    Default_district_handle = objp->get_handle();
    return true;
}

// 0x0075F240 (:805)
bool object_district_release_default_district()
{
    Default_district_handle = 0xFFFFFFFF;
    return true;
}

// 0x007ABE40 (:195)
// 0x0075F090 (:439)
int district::get_minimum_edf_progression() const
{
    return min_edf_progression_level;
}

// Original: outside a load a district whose control is already zero keeps it, and any change liberates the district.
void district::set_control(float control_value)
{
    const bool loading = Loading_data;
    if (!loading && control == 0.0f)
    {
        return;
    }
    if (control_value < 0.0f)
    {
        control_value = 0.0f;
    }
    if (control_value >= control_max)
    {
        control_value = control_max;
    }
    control = control_value;
    if (!loading)
    {
        liberate_district();
        mission_unlock_capstone_in_district(get_handle());
        mission_notify_new();
    }
}

// 0x0075ECD0 (:234)
void district::set_morale(float morale_value)
{
    if (dlc_get_morale_disabled())
    {
        return;
    }
    if (morale_value < 0.0f)
    {
        morale_value = 0.0f;
    }
    if (morale_value >= morale_max)
    {
        morale_value = morale_max;
    }
    morale = morale_value;
    stats_update_district_morale(district_index, morale_value / morale_max);
}

// 0x007ABF70 (:257)
void district::set_control_max(float control_max_value)
{
    const bool loading = Loading_data;
    float value = control;
    control_max = control_max_value;
    if (!loading && value == 0.0f)
    {
        return;
    }
    if (value < 0.0f)
    {
        value = 0.0f;
    }
    if (value >= control_max_value)
    {
        value = control_max_value;
    }
    control = value;
    if (!loading)
    {
        liberate_district();
        mission_unlock_capstone_in_district(get_handle());
        mission_notify_new();
    }
}

// 0x0075EE10 (:276)
void district::set_morale_max(float morale_max_value)
{
    morale_max = morale_max_value;
    float value = morale;
    if (dlc_get_morale_disabled())
    {
        return;
    }
    if (value < 0.0f)
    {
        value = 0.0f;
    }
    if (value >= morale_max)
    {
        value = morale_max;
    }
    morale = value;
    stats_update_district_morale(district_index, value / morale_max);
}

// 0x007543A0 (.h:89)
void district::load_liberated_value(bool load_value)
{
    liberated = load_value;
}

// 0x007543B0 (.h:90)
bool district::needs_to_play_line() const
{
    return needs_to_play_radio_line;
}

// 0x007543C0 (.h:92)
void district::load_liberated_play_line_value(bool load_value)
{
    needs_to_play_radio_line = load_value;
}

// 0x0078F680 (:873)
bool object_district_save_districts(cfile* save_file)
{
    const int count = World.get_num_objects<district>();
    cf_write_uint32((unsigned int)count, save_file);
    for (int i = 0; i < count; i++)
    {
        district* dp = World.get_object<district>(i);
        if (dp == nullptr)
        {
            return false;
        }
        cf_write_float(dp->get_control_max(), save_file);
        cf_write_float(dp->get_control(), save_file);
        cf_write_float(dp->get_morale_max(), save_file);
        float morale;
        if (stats_cheat_enabled(CHEATS_MAX_MORALE))
        {
            morale = dp->get_morale_max();
        }
        else if (dlc_get_morale_disabled())
        {
            morale = 0.0f;
        }
        else
        {
            morale = dp->get_morale();
        }
        cf_write_float(morale, save_file);
        cf_write_bool(dp->is_liberated(), save_file);
        cf_write_bool(dp->needs_to_play_line(), save_file);
    }
    return true;
}

// 0x007AC0B0 (:898)
bool object_district_load_districts(cfile* save_file)
{
    const unsigned int count = cf_read_uint32(save_file);
    if (count != (unsigned int)World.get_num_objects<district>())
    {
        return false;
    }
    Loading_data = true;
    for (int i = 0; i < (int)count; i++)
    {
        district* dp = World.get_object<district>(i);
        if (dp == nullptr)
        {
            // Original: leaves Loading_data set.
            return false;
        }
        dp->set_control_max(cf_read_float(save_file));
        dp->set_control(cf_read_float(save_file));
        dp->set_morale_max(cf_read_float(save_file));
        dp->set_morale(cf_read_float(save_file));
        dp->load_liberated_value(cf_read_bool(save_file));
        dp->load_liberated_play_line_value(cf_read_bool(save_file));
    }
    Loading_data = false;
    return true;
}

// 0x0078F660 (:861)
bool object_district_preload_districts(const char* territory_name)
{
    territory* tp = territory_shared_find(territory_name);
    if (tp == nullptr)
    {
        return true;
    }
    return parse_all_districts(tp);
}

// 0x0075ECC0 (:190)
float district::get_control() const
{
    return control;
}

// 0x00774DA0 (:220)
float district::get_morale() const
{
    if (stats_cheat_enabled(CHEATS_MAX_MORALE))
    {
        return morale_max;
    }
    if (dlc_get_morale_disabled())
    {
        return 0.0f;
    }
    return morale;
}

// 0x0075EDF0 (:252)
float district::get_control_max() const
{
    return control_max;
}

// 0x0075EE00 (:270)
float district::get_morale_max() const
{
    return morale_max;
}

// ---- gameflow.md GF3: control and morale changes, liberation and radio lines, the grid queries

// 0x0078EE90 (:100): the first district with a radio line waiting.
static district* find_radio_line_district()
{
    const int count = World.get_num_objects<district>();
    for (int i = 0; i < count; i++)
    {
        district* dp = World.get_object<district>(i);
        if (dp->needs_to_play_line())
        {
            return dp;
        }
    }
    return nullptr;
}

// 0x007ABED0 (:215)
// Original: outside a load a district whose control is already zero keeps it, and any change tries to liberate it.
void district::change_control(float control_delta)
{
    const bool loading = Loading_data;
    float value = control + control_delta;
    if (!loading && control == 0.0f)
    {
        return;
    }
    if (value < 0.0f)
    {
        value = 0.0f;
    }
    if (value >= control_max)
    {
        value = control_max;
    }
    control = value;
    if (!loading)
    {
        liberate_district();
        mission_unlock_capstone_in_district(get_handle());
        mission_notify_new();
    }
}

// 0x0075ED50 (:247)
void district::change_morale(float morale_delta)
{
    float value = morale_delta + morale;
    if (dlc_get_morale_disabled())
    {
        return;
    }
    if (value < 0.0f)
    {
        value = 0.0f;
    }
    if (value >= morale_max)
    {
        value = morale_max;
    }
    morale = value;
    stats_update_district_morale(district_index, value / morale_max);
}

// 0x007AC000 (:265)
void district::change_control_max(float control_max_delta)
{
    const bool loading = Loading_data;
    const float new_max = control_max_delta + control_max;
    float value = control;
    control_max = new_max;
    if (!loading && value == 0.0f)
    {
        return;
    }
    if (value < 0.0f)
    {
        value = 0.0f;
    }
    control = value < new_max ? value : new_max;
    if (!loading)
    {
        liberate_district();
        mission_unlock_capstone_in_district(get_handle());
        mission_notify_new();
    }
}

// 0x0075EEB0 (:284)
void district::change_morale_max(float morale_max_delta)
{
    morale_max = morale_max_delta + morale_max;
    float value = morale;
    if (dlc_get_morale_disabled())
    {
        return;
    }
    if (value < 0.0f)
    {
        value = 0.0f;
    }
    if (value >= morale_max)
    {
        value = morale_max;
    }
    morale = value;
    stats_update_district_morale(district_index, value / morale_max);
}

// 0x0075EF60 (:289)
// True when no line is waiting any more. Original: without a line or a radio persona the flag stays as it was.
bool district::play_radio_line()
{
    if (liberated_radio_line_handle != INVALID_VOICE_LINE_HANDLE)
    {
        const human_voice_persona* hvpp = human_voice_radio_persona();
        if (hvpp != nullptr)
        {
            needs_to_play_radio_line = !dialogue_manager_start_dialogue(liberated_radio_line_handle, hvpp, DIALOGUE_PRIORITY_RADIO, INVALID_PA_HANDLE, nullptr);
        }
    }
    return !needs_to_play_radio_line;
}

// 0x007A5CE0 (:304)
// Liberated once the EDF control is zero and no mission is left here, hidden ones included.
bool district::liberate_district()
{
    if (liberated || Loading_data)
    {
        return false;
    }
    if (gfm_get_edf_control(this) == 0.0f && missions_left_in_district(get_handle(), true) == 0)
    {
        liberated = true;
    }
    if (!liberated)
    {
        return false;
    }
    if (liberated_edf_level != 0)
    {
        edf_progression_set_level(liberated_edf_level);
    }
    if (liberated_marauder_level != 0)
    {
        marauder_progression_set_level(liberated_marauder_level);
    }
    if (liberated_tech_level != 0.0f)
    {
        World.set_tech_level(liberated_tech_level);
    }
    // Every convoy end point of the map starts again, not only this district's.
    const int num_convoy_ends = World.get_num_objects<object_convoy_end_point>();
    for (int i = 0; i < num_convoy_ends; i++)
    {
        World.get_object<object_convoy_end_point>(i)->enable_convoy();
    }
    const int num_movers = World.get_num_objects<object_mover>();
    for (int i = 0; i < num_movers; i++)
    {
        World.get_object<object_mover>(i)->check_liberation();
    }
    upgrade_unlock_from_liberation(get_handle());
    mission_unlock_from_liberation();
    stats_district_liberated(get_name());
    public_address_register_district_liberated(get_handle());
    if (liberated_radio_line_handle != INVALID_VOICE_LINE_HANDLE)
    {
        needs_to_play_radio_line = true;
    }
    activity_riding_shotgun_liberation();
    return liberated;
}

// 0x0079E480 (:400)
// Nothing once the district's capstone mission is complete.
void district::play_capstone_radio_lines()
{
    const mission_handle capstone = mission_capstone_in_district(get_handle());
    if (capstone != INVALID_MISSION_HANDLE && (int)capstone >= 0 && (int)capstone < All_mission_infos.size() && All_mission_infos[(int)capstone].mission_flags.complete)
    {
        return;
    }
    const human_voice_persona* hvpp = human_voice_radio_persona();
    if (hvpp == nullptr || capstone_line_1 == INVALID_VOICE_LINE_HANDLE)
    {
        return;
    }
    if (dialogue_manager_start_dialogue(capstone_line_1, hvpp, DIALOGUE_PRIORITY_RADIO, INVALID_PA_HANDLE, nullptr) && capstone_line_2 != INVALID_VOICE_LINE_HANDLE)
    {
        // Magic number: the second line starts 500 ms after the first ends.
        dialogue_manager_add_line(capstone_line_2, hvpp, capstone_line_1, 500, INVALID_PA_HANDLE, nullptr);
    }
}

// 0x0075F0B0 (:490)
const farray<grid_info,21>& district::get_gid_list()
{
    return grid_ids;
}

// 0x0075F0C0 (:517)
void district::gid_to_grid_index(unsigned char& grid_x, unsigned char& grid_y, unsigned short gid)
{
    const unsigned char high = (unsigned char)(gid >> 8);
    grid_x = high >> 4;
    grid_y = high & 0xF;
}

// 0x0075F0F0 (:540)
void district::get_grid_color(unsigned char& r, unsigned char& g, unsigned char& b) const
{
    r = col.red;
    g = col.green;
    b = col.blue;
}

// 0x0075F120 (:553)
void district::get_grid_color_f(float& r, float& g, float& b) const
{
    // Magic number: one_over_255 (0x0165A26C).
    const float scale = 1.0f / 255.0f;
    r = (float)col.red * scale;
    g = (float)col.green * scale;
    b = (float)col.blue * scale;
}

// 0x0075F1B0 (:571)
bool district::get_disable_morale_change() const
{
    return flags.disable_morale_change;
}

// 0x0075F1C0 (:576)
bool district::get_disable_control_change() const
{
    return flags.disable_control_change;
}

// 0x0075F210 (:595)
// Original: only answers the count; image_names is never filled.
int district::get_loading_screen_images(const char** image_names)
{
    return num_load_screen_images;
}

// 0x0079E540 (:646)
unsigned int district_handle_local_player()
{
    const player* pp = player_get_local_player();
    const district* dp = pp == nullptr ? get_default_district() : district_get_from_pos(pp->pos);
    return dp != nullptr ? dp->get_handle() : 0xFFFFFFFF;
}

// The grid flags of pos's cell, in the district of pos's zone; false when the zone's district does not hold that cell.
static const grid_info* grid_info_at_pos(const vector& pos)
{
    obj_zone* zone = World.get_zone_by_pos(pos);
    if (zone == nullptr || zone->district_handle == 0xFFFFFFFF)
    {
        return nullptr;
    }
    district* dp = World.object_from_handle<district>(zone->district_handle);
    int grid_x;
    int grid_y;
    if (dp == nullptr || !rfg_zonex_get_grid_coords(pos, grid_x, grid_y))
    {
        return nullptr;
    }
    const farray<grid_info,21>& list = dp->get_gid_list();
    for (int i = 0; i < list.size(); i++)
    {
        const unsigned short gid = list[i].gid;
        if ((gid >> 12) == grid_x && ((gid >> 8) & 0xF) == grid_y)
        {
            return &list[i];
        }
    }
    return nullptr;
}

// 0x00774EB0 (:699)
bool district_play_howl_at_pos(const vector& pos)
{
    const grid_info* info = grid_info_at_pos(pos);
    return info != nullptr && info->flags.play_marauder_howl;
}

// 0x00774F70 (:740)
bool district_marauder_zone_at_pos(const vector& pos)
{
    const grid_info* info = grid_info_at_pos(pos);
    return info != nullptr && info->flags.marauder_zone;
}

// 0x0078F780 (:929)
// True when no district has a line waiting, or the first one that does has now played it.
bool object_district_play_liberated_line()
{
    district* dp = find_radio_line_district();
    return dp == nullptr || dp->play_radio_line();
}

// 0x0078F810 (:939)
bool object_district_radio_line_ready()
{
    return find_radio_line_district() != nullptr;
}
