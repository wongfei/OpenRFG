// Original: volition/rfg/code/gameplay/activities/base_activity_data.cpp

#include "volition/rfg/code/gameplay/activities/base_activity_data.h"

#include "volition/rfg/code/gameplay/gameflow/alert_level_manager.h"
#include "volition/rfg/code/gameplay/managed_layer_loading.h"
#include "volition/rfg/code/gameplay/managed_layer_loading_shared.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice_shared.h"
#include "volition/rfg/code/scripting/managed_script_group.h"
#include "volition/rfg/code/rfg.h"
#include "volition/rfg/code/spawn/spawn_object_group.h"
#include "volition/vlib/code/util/hash_table.h"
#include "volition/vlib/code/util/string_pool.h"
#include "volition/vlib/code/memory/rfg_split_mempool.h"
#include "volition/vlib/code/memory/static_mempool.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/vlib/code/util/fastxml.h"

#include <stdio.h>
#include <string.h>

// Magic number: the table's size in rfg.pdb.
static farray<base_activity_data, 128> Base_activity_data_pool; // 0x0224D028
// rfg.exe constructs it as str_data_hash_table_cached (0x007869A0, vtable 0x012DE94C): keys compare without case, so the
// other tables' lookups by their own xml text find the pooled names. A plain data_hash_table compares pointers and finds
// nothing.
static str_data_hash_table_cached<int, 128, unsigned char> Base_activity_data_hash; // 0x02265D30
static string_pool Activity_data_name_string_pool("Activity Data Names"); // 0x022475E8
static char* Activity_data_name_string_pool_data; // 0x022295E8
static int DLC_num_base_activity_mark; // 0x022295EC

// 0x0077E5D0 (.h:34)
base_activity_data::base_activity_data()
{
    reset();
}

// 0x00764290
base_activity_data::~base_activity_data()
{
}

// 0x007AC8F0
base_activity_data& base_activity_data::operator=(const base_activity_data& that)
{
    name = that.name;
    control_limit = that.control_limit;
    managed_script_list.copy(that.managed_script_list);
    managed_layers = that.managed_layers;
    min_alert_cap = that.min_alert_cap;
    max_alert_cap = that.max_alert_cap;
    v_spawn_group = that.v_spawn_group;
    v_opt_spawn_group = that.v_opt_spawn_group;
    h_spawn_group = that.h_spawn_group;
    desired_voice = that.desired_voice;
    desired_music_type = that.desired_music_type;
    special_score_name = that.special_score_name;
    activity_flags = that.activity_flags;
    return *this;
}

// 0x0075ADD0 (:52). rfg.exe: LTCG, xml_reservation_string in ESI. Anything but the two names counts as "At Init".
static bool reserve_at_start(xml_element* xml_reservation_string)
{
    if (v_stricmp("At Start", xml_reservation_string->text) == 0)
    {
        return true;
    }
    if (v_stricmp("At Init", xml_reservation_string->text) != 0)
    {
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
    }
    return false;
}

// 0x0075AE20 (:70). rfg.exe: LTCG, music_intensity_name in ESI.
static GAMEMUSIC_THRESHOLDS find_music_intensity(const char* music_intensity_name)
{
    if (v_stricmp("Light", music_intensity_name) == 0)
    {
        return GAMEMUSIC_THRESHOLD_COMBAT_LIGHT;
    }
    return v_stricmp("Heavy", music_intensity_name) == 0 ? GAMEMUSIC_THRESHOLD_COMBAT_HEAVY : GAMEMUSIC_THRESHOLD_INVALID;
}

// 0x00786A60 (:88). rfg.exe: LTCG, data_name in ESI.
static base_activity_data* find_activity_data_internal(const char* data_name)
{
    int index;
    if (Base_activity_data_hash.get(data_name, index) && index >= 0 && index < Base_activity_data_pool.size())
    {
        return &Base_activity_data_pool[index];
    }
    return nullptr;
}

// 0x007B3520 (:109)
void base_activity_load_table(int dlc_id)
{
    char filename[256];
    const char* system_name;
    mempool* pool;
    const bool is_dlc = dlc_id != -1;
    if (is_dlc)
    {
        sprintf(filename, "dlc%02d_%s", dlc_id, "activity.xtbl");
        system_name = "dlc_activity_containers";
        DLC_num_base_activity_mark = Base_activity_data_pool.size();
        Activity_data_name_string_pool.set_thread_ownership();
        Activity_data_name_string_pool.mark();
        pool = reinterpret_cast<mempool*>(&Level_mempool);
    }
    else
    {
        pool = reinterpret_cast<mempool*>(&Permanent_mempool);
        sprintf(filename, "%s", "activity.xtbl");
        system_name = "activity_containers";
        Activity_data_name_string_pool.clear();
        Base_activity_data_pool.clear();
        Base_activity_data_hash.clear();
    }
    managed_layer_loading_parse_shared(system_name, filename, "base_activity", 6, reinterpret_cast<mempool_base*>(&Common_mempool), pool, Use_packfiles, PLATFORM_PC, is_dlc, -1); // Magic number: container type 6
    xml_element* root = xml_table_parse(filename);
    if (root == nullptr)
    {
        return;
    }
    for (xml_element* entry = xml_find(root, "base_activity"); entry != nullptr; entry = xml_find_next(root, entry, "base_activity"))
    {
        if (Base_activity_data_pool.size() >= Base_activity_data_pool.capacity())
        {
            continue;
        }
        int index;
        base_activity_data* data = Base_activity_data_pool.add_in_place(index);
        if (data == nullptr)
        {
            break;
        }
        if (data->load(entry))
        {
            Base_activity_data_hash.insert_key_data(data->name, index);
        }
        else
        {
            data->reset();
            Base_activity_data_pool.remove_unsorted(index);
        }
    }
    xml_table_close(root);
}

// 0x007B36E0 (:167)
void base_activity_unload_dlc()
{
    for (int i = Base_activity_data_pool.size() - 1; i >= DLC_num_base_activity_mark; i--)
    {
        Base_activity_data_hash.remove(Base_activity_data_pool[i].name);
        Base_activity_data_pool.remove_unsorted(i);
    }
    Activity_data_name_string_pool.set_thread_ownership();
    Activity_data_name_string_pool.restore_to_mark(0);
}

// 0x00770FE0 (:184)
void base_activity_data::reset()
{
    name = nullptr;
    control_limit = 1.0f;
    managed_layers = nullptr;
    managed_script_list.clear();
    min_alert_cap = ALERT_LEVEL_GREEN;
    max_alert_cap = ALERT_LEVEL_RED;
    v_spawn_group = nullptr;
    v_opt_spawn_group = nullptr;
    h_spawn_group = nullptr;
    desired_voice = nullptr;
    desired_music_type = GAMEMUSIC_THRESHOLD_INVALID;
    special_score_name = nullptr;
    memset(&activity_flags, 0, sizeof(activity_flags));
}

// 0x0075AE60 (:202)
bool base_activity_data::reserve_voice(unsigned int activity_spawn_node) const
{
    return desired_voice == nullptr || human_voice_activity_reserve_voice(desired_voice, activity_spawn_node);
}

// 0x0075AE80 (:213)
bool base_activity_data::voice_loaded(unsigned int activity_spawn_node) const
{
    if (desired_voice == nullptr)
    {
        return true;
    }
    return human_voice_activity_is_reserved(desired_voice, activity_spawn_node) && desired_voice->is_loaded();
}

// 0x0075AEC0 (:225)
bool base_activity_data::release_voice(unsigned int activity_spawn_node) const
{
    return desired_voice == nullptr || human_voice_activity_release_voice(activity_spawn_node);
}

// 0x0075AEE0 (:235)
bool base_activity_data::can_reserve_voice(unsigned int activity_spawn_node) const
{
    return desired_voice == nullptr || human_voice_activity_can_reserve(activity_spawn_node);
}

// Clamps an alert level read from the table.
static gfm_alert_levels clamp_alert_level(gfm_alert_levels level)
{
    if ((int)level < 0)
    {
        return ALERT_LEVEL_GREEN;
    }
    return (int)level > ALERT_LEVEL_RED ? ALERT_LEVEL_RED : level;
}

// 0x007A2870 (:249)
bool base_activity_data::load(xml_element* xml_root)
{
    reset();
    xml_element* e = xml_find(xml_root, "Name");
    if (e == nullptr)
    {
        return false;
    }
    name = Activity_data_name_string_pool.add_unique(e->text);
    if (name == nullptr)
    {
        return false;
    }
    e = xml_find(xml_root, "disabled");
    if (e == nullptr)
    {
        return false;
    }
    activity_flags.disabled = xml_get_bool(e, false);
    e = xml_find(xml_root, "control_limit");
    if (e == nullptr)
    {
        return false;
    }
    control_limit = xml_get_float(e, 1.0f);
    e = xml_find(xml_root, "guerrilla_reinforcement");
    if (e == nullptr)
    {
        return false;
    }
    activity_flags.allow_guerrilla_reinforcement = xml_get_bool(e, true);
    e = xml_find(xml_root, "reservation_info");
    if (e != nullptr)
    {
        xml_element* group = xml_find(e, "vehicle_group");
        xml_element* time = xml_find(e, "reserve_at_start");
        if (group == nullptr || time == nullptr)
        {
            return false;
        }
        v_spawn_group = sog_get_vehicle_group(group->text);
        activity_flags.reserve_vehicle_at_start = reserve_at_start(time);
        xml_element* optional = xml_find(e, "optional_vehicle_group");
        if (optional != nullptr)
        {
            v_opt_spawn_group = sog_get_vehicle_group(optional->text);
        }
    }
    e = xml_find(xml_root, "restoration_time");
    if (e == nullptr)
    {
        return false;
    }
    activity_flags.restore_at_start = reserve_at_start(e);
    e = xml_find(xml_root, "human_reservation_info");
    if (e != nullptr)
    {
        xml_element* group = xml_find(e, "human_group");
        if (group == nullptr)
        {
            return false;
        }
        h_spawn_group = sog_get_human_group(group->text);
        if (h_spawn_group == nullptr)
        {
            return false;
        }
        xml_element* time = xml_find(e, "human_reserve_time");
        if (time == nullptr)
        {
            return false;
        }
        activity_flags.reserve_human_at_start = reserve_at_start(time);
    }
    e = xml_find(xml_root, "layer_load_time");
    if (e != nullptr)
    {
        activity_flags.load_layer_at_start = reserve_at_start(e);
    }
    managed_layers = managed_layer_load(name, xml_root, false);
    e = xml_find(xml_root, "script_groups");
    if (e != nullptr)
    {
        for (xml_element* group = xml_find(e, "script_group_name"); group != nullptr; group = xml_find_next(e, group, "script_group_name"))
        {
            int handle = managed_script_get_handle(group->text);
            if (handle == -1)
            {
                handle = managed_script_create_handle(group->text);
            }
            if (handle != -1)
            {
                managed_script_list.add(handle);
            }
        }
    }
    e = xml_find(xml_root, "min_alert_cap");
    if (e != nullptr)
    {
        min_alert_cap = clamp_alert_level(alm_resolve_alert_level(e->text));
    }
    e = xml_find(xml_root, "max_alert_cap");
    if (e != nullptr)
    {
        max_alert_cap = clamp_alert_level(alm_resolve_alert_level(e->text));
    }
    e = xml_find(xml_root, "ratchet_alert_level");
    if (e != nullptr)
    {
        activity_flags.ratchet_alert_level = strcmp(e->text, "True") == 0; // Original: case sensitive, unlike the rest
    }
    if ((int)max_alert_cap < (int)min_alert_cap)
    {
        min_alert_cap = ALERT_LEVEL_GREEN;
        max_alert_cap = ALERT_LEVEL_RED;
    }
    e = xml_find(xml_root, "voice_info");
    if (e != nullptr)
    {
        xml_element* type = xml_find(e, "voice_type");
        if (type == nullptr)
        {
            return false;
        }
        desired_voice = human_voice_persona_from_name(type->text);
        if (desired_voice == nullptr)
        {
            return false;
        }
        xml_element* time = xml_find(e, "reserve_at_start");
        if (time == nullptr)
        {
            return false;
        }
        activity_flags.persona_at_start = reserve_at_start(time);
    }
    e = xml_find(xml_root, "music_info");
    if (e != nullptr)
    {
        xml_element* intensity = xml_find(e, "music_intensity");
        if (intensity == nullptr)
        {
            return false;
        }
        desired_music_type = find_music_intensity(intensity->text);
        xml_element* score = xml_find(e, "special_score");
        if (score != nullptr)
        {
            special_score_name = Activity_data_name_string_pool.add_unique(score->text);
        }
    }
    return true;
}

// 0x007B37F0 (:424)
void activity_data_system_init()
{
    if (Activity_data_name_string_pool_data == nullptr)
    {
        Activity_data_name_string_pool_data = static_cast<char*>(Permanent_mempool.alloc(0x500, 0)); // Magic number: 1280 bytes of names
        if (Activity_data_name_string_pool_data == nullptr)
        {
            keen::debug::nativePrintArgs("Insufficient memory to allocate the activity data name string table");
        }
        Activity_data_name_string_pool.init(Activity_data_name_string_pool_data, 0x500, "Activity Data Names", 0, 0);
    }
    Activity_data_name_string_pool.clear();
    Base_activity_data_pool.clear();
    Base_activity_data_hash.clear();
    base_activity_load_table(-1);
}

// 0x00786AB0 (:451)
const base_activity_data* activity_data_find_handle(const char* data_name)
{
    return find_activity_data_internal(data_name); // rfg.exe inlines it
}

// 0x00771020 (:457). Original: a negative index is not refused.
const base_activity_data* activity_data_find_by_index(int index)
{
    if (index >= Base_activity_data_pool.size())
    {
        return nullptr;
    }
    return &Base_activity_data_pool[index];
}
