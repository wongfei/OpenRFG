// Original: volition/rfg/code/multi/multi_game/multi_game_character.cpp
//
// The multiplayer characters (mp_character.xtbl): one list per selection type (free for all, EDF, Guerilla), each
// entry a human info with its display name, bitmap, voice persona group (mp_personas.xtbl), unlock level and backpack
// offsets. A player's selection bundle holds one list index per selection type, a byte each (the free for all index
// in the low byte); the profile stores the chosen characters by name crc. The unlockable sledgehammers come from
// mp_weapon_unlockables.xtbl.
// Part of the multi unity TU (rfg_multi_unity.obj, every multi/ .cpp in path order): the file statics are shared
// by the whole tree, and a shared one is placed in its first user (tools/hdrgen/specs/mp0_stubs.py).

#include "volition/rfg/code/multi/multi_game/multi_game_character.h"
#include "volition/rfg/code/multi/multi_game/multi_game_object_backpack.h"
#include "volition/rfg/code/control/control_map.h"
#include "volition/rfg/code/gamewide/player_profile.h"
#include "volition/rfg/code/objects/humans/human_info.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice_shared.h"
#include "volition/rfg/code/objects/inventory_item.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/memory/mempool_base.h"
#include "volition/vlib/code/memory/object_pool.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/vlib/code/util/fastxml.h"
#include "volition/vlib/code/util/string_util.h"
#include "volition/vlib/code/util/string_comp.h"
#include "volition/vlib/code/util/string_pool.h"
#include "keen/base/trace.hpp"

#include <string.h>

// ---- file-private types (rfg.pdb)
// size 0x18
struct mp_persona_info
{
    mp_persona_info(); // 0x009972E0
    ~mp_persona_info(); // 0x00997300
    mp_persona_info& operator=(const mp_persona_info& __that); // 0x009A4630

    const char* name; // 0x00
    human_teams music_team; // 0x04
    voice_persona_list voice_personas; // 0x08
};

// 0x009972E0
mp_persona_info::mp_persona_info()
{
}

// 0x00997300
mp_persona_info::~mp_persona_info()
{
}

// 0x009A4630
mp_persona_info& mp_persona_info::operator=(const mp_persona_info& __that)
{
    name = __that.name;
    music_team = __that.music_team;
    voice_personas = __that.voice_personas;
    return *this;
}

// ---- globals
dynamic_object_pool<mp_character_unlockable_weapon, 1> Mp_unlockable_weapon_pool; // 0x01692B08

// ---- file-static globals
static const char MULTI_GAME_CHARACTER_STRING_POOL_NAME[29] = "Multi game character strings"; // 0x01309DC0
// unsigned char My_level (0x0167F4F8) is used here too; placed in multi_game_badge.cpp.
// rfg.exe has these constant-initialized (no dynamic initializer): the free for all, EDF and Guerilla lists.
static mp_character_selection_list Team_lists[NUM_MP_CHARACTER_SELECTION_TYPES] = { // 0x01692758
    mp_character_selection_list("FFA", HUMAN_TEAM_CIVILIAN),
    mp_character_selection_list("EDF", HUMAN_TEAM_EDF),
    mp_character_selection_list("Guerilla", HUMAN_TEAM_GUERILLA),
};
// dynamic_object_pool<mp_game_character_info,1> Mp_character_info_pool (0x016929F8) is used here too; placed in multi_game.cpp.
static farray<mp_persona_info,8> MP_persona_infos; // 0x01692A38
static string_pool Multi_game_character_string_pool("Multi game character strings"); // 0x02FEB8F8

// Magic numbers: 50 characters per list, 8 persona groups of at most 64 personas, 150 characters in all, a 1,400
// byte string pool.
static const int Max_characters_per_list = 50;
static const int Max_persona_groups = 8;
static const int Max_personas_per_group = 64;
static const int Max_character_infos = 150;
static const unsigned int Character_string_pool_size = 0x578;

// 0x009972A0 (:54)
mp_character_selection_list::mp_character_selection_list(const char* _team_name, human_teams _team)
{
    team_name = _team_name;
    team = _team;
}

// 0x009972D0
mp_character_selection_list::~mp_character_selection_list()
{
}

// 0x0096BC10
mp_game_character_info::mp_game_character_info()
{
}

// ---- internal functions
static void parse_mp_characters(); // 0x009DD020 (:284), defined below (called before its definition)

// Port helper: rfg.exe inlines this lookup into every query by selection type: the index for the type (a byte of the
// bundle), the first character of the first list when it is out of range.
static mp_game_character_info* character_from_bundle(int selection_index, unsigned int player_selection_bundle)
{
    const int index = (int)(player_selection_bundle >> (selection_index * 8) & 0xFF);
    if (index < Team_lists[selection_index].list.size())
    {
        return Team_lists[selection_index].list[index];
    }
    return Team_lists[0].list[0];
}

// Port helper: the index of the character of a human info in a list (0 when not there), as rfg.exe inlines it.
static unsigned int list_index_of(int selection_index, const human_info* hi)
{
    for (int i = 0; i < Team_lists[selection_index].list.size(); i++)
    {
        if (Team_lists[selection_index].list[i]->hip == hi)
        {
            return (unsigned int)i;
        }
    }
    return 0;
}

// 0x00951050 (:114)
static mp_character_selection_types get_team_index(const char* team_name)
{
    for (int i = 0; i < NUM_MP_CHARACTER_SELECTION_TYPES; i++)
    {
        if (v_stricmp(team_name, Team_lists[i].team_name) == 0)
        {
            return (mp_character_selection_types)i;
        }
    }
    keen::debug::nativePrintArgs("Team index not found!!");
    return MP_SEL_FFA;
}

// 0x009510A0 (:126). LTCG: team in EDX.
static mp_character_selection_types get_team_index(human_teams team)
{
    for (int i = 0; i < NUM_MP_CHARACTER_SELECTION_TYPES; i++)
    {
        if (Team_lists[i].team == team)
        {
            return (mp_character_selection_types)i;
        }
    }
    keen::debug::nativePrintArgs("Team index not found!!");
    return MP_SEL_FFA;
}

// 0x0097A6C0 (:138). LTCG: selection_type in ECX, hi in ESI.
static unsigned char get_index_from_human_info(mp_character_selection_types selection_type, const human_info* hi)
{
    return (unsigned char)list_index_of(selection_type, hi);
}

// 0x0097A700 (:150). LTCG: selection_type in EAX, player_selection_bundle in EDX.
static mp_game_character_info* multi_game_player_get_info(mp_character_selection_types selection_type, unsigned int player_selection_bundle)
{
    if ((unsigned int)selection_type < NUM_MP_CHARACTER_SELECTION_TYPES)
    {
        return character_from_bundle(selection_type, player_selection_bundle);
    }
    return Team_lists[0].list[0];
}

// 0x0097A740 (:163). LTCG: selection_type in EAX, player_selection_bundle in EDX.
static const human_info* multi_game_player_get_human_info(mp_character_selection_types selection_type, unsigned int player_selection_bundle)
{
    return multi_game_player_get_info(selection_type, player_selection_bundle)->hip;
}

// 0x0097A780 (:169)
static const voice_persona_list* multi_game_player_get_voice_persona_list(const char* group_name)
{
    if (group_name == nullptr)
    {
        return nullptr;
    }
    for (int i = 0; i < MP_persona_infos.size(); i++)
    {
        if (v_stricmp(MP_persona_infos[i].name, group_name) == 0)
        {
            return &MP_persona_infos[i].voice_personas;
        }
    }
    return nullptr;
}

// 0x009F0830 (:102): console command (unregistered in retail)
void unused_dcf_reload_mp_character_table()
{
    cf_refresh_file_size("mp_character.xtbl");
    parse_mp_characters();
}

// 0x009510D0 (:187): EDF personas get the EDF music, the others team 0 (Guerilla)
human_teams get_music_team_from_persona_name(const char* persona_name)
{
    return (human_teams)(strstr(persona_name, "EDF") != nullptr);
}

// 0x009BCC40 (:192): the persona groups, each with its voice personas (at most 64, without duplicates)
static void parse_mp_personas(mempool_base& pool)
{
    MP_persona_infos.clear();
    xml_element* root = xml_table_parse("mp_personas.xtbl");
    if (root == nullptr)
    {
        return;
    }
    if (xml_table_count(root, "MP_Persona") > Max_persona_groups)
    {
        keen::debug::nativePrintArgs("too many personna groups");
    }
    for (xml_element* group = xml_find(root, "MP_Persona"); group != nullptr; group = xml_find_next(root, group, "MP_Persona"))
    {
        const char* name = Multi_game_character_string_pool.add_unique(xml_table_get_req_string_ref(group, "Name"));
        mp_persona_info new_info;
        new_info.voice_personas.reset();
        mp_persona_info& info = MP_persona_infos[MP_persona_infos.add(new_info)];
        info.name = name;
        info.music_team = get_music_team_from_persona_name(name);
        xml_element* list = xml_table_find_req(group, "Persona_List");
        farray<human_voice_persona*, Max_personas_per_group> personas;
        for (xml_element* persona = xml_find(list, "Persona"); persona != nullptr; persona = xml_find_next(list, persona, "Persona"))
        {
            if (personas.size() < Max_personas_per_group)
            {
                human_voice_persona* voice = human_voice_persona_from_name(xml_table_get_req_string_ref(persona, nullptr));
                if (voice != nullptr)
                {
                    personas.add_unique(voice);
                }
            }
        }
        human_voice_persona** data = (human_voice_persona**)pool.alloc(personas.size() * sizeof(human_voice_persona*), 4);
        if (data == nullptr)
        {
            return;
        }
        info.voice_personas.voice_list.init(data, personas.size(), 0);
        for (int i = 0; i < personas.size(); i++)
        {
            info.voice_personas.voice_list.add(personas[i]);
        }
    }
    xml_table_close(root);
}

// 0x009AE8A0 (:252)
void parse_weapon_unlockables(mempool_base& pool)
{
    xml_element* root = xml_table_parse("mp_weapon_unlockables.xtbl");
    if (root == nullptr)
    {
        return;
    }
    const int num_objects = xml_table_count(root, "unlockable");
    if (num_objects <= 0)
    {
        return;
    }
    Mp_unlockable_weapon_pool.init(pool, num_objects);
    for (xml_element* entry = xml_table_find_opt(root, "unlockable"); entry != nullptr; entry = xml_table_find_next(root, entry, "unlockable"))
    {
        const weapon_info* w_info = weapons_get_weapon_info(xml_table_get_req_string_ref(entry, "Name"));
        if (w_info == nullptr)
        {
            continue;
        }
        mp_character_unlockable_weapon* unlockable = Mp_unlockable_weapon_pool.alloc(0);
        if (unlockable == nullptr)
        {
            return;
        }
        unlockable->unique_weapon_id = w_info->unique_id;
        xml_table_get_req_int(unlockable->level_needed, entry, "level_needed");
        xml_table_get_req_string(unlockable->image_name, sizeof(unlockable->image_name), entry, "image_name");
    }
    xml_table_close(root);
}

// 0x009DD020 (:284): the character lists (the character infos of an earlier parse are freed first)
static void parse_mp_characters()
{
    for (int i = 0; i < NUM_MP_CHARACTER_SELECTION_TYPES; i++)
    {
        Team_lists[i].list.clear();
    }
    Mp_character_info_pool.thread_access.enter();
    for (mp_game_character_info* info = Mp_character_info_pool.get_first(0); info != nullptr; info = Mp_character_info_pool.free(info))
    {
    }
    Mp_character_info_pool.thread_access.leave();
    xml_element* root = xml_table_parse("mp_character.xtbl");
    if (root == nullptr)
    {
        return;
    }
    for (xml_element* team = xml_find(root, "MP_character"); team != nullptr; team = xml_find_next(root, team, "MP_character"))
    {
        const mp_character_selection_types selection_type = get_team_index(xml_table_get_req_string_ref(team, "Name"));
        xml_element* grid = xml_table_find_opt(team, "Character_grid");
        for (xml_element* element = xml_find(grid, "Character_Element");
             element != nullptr && Team_lists[selection_type].list.size() < Max_characters_per_list;
             element = xml_find_next(grid, element, "Character_Element"))
        {
            const char* character_name = xml_table_get_req_string_ref(element, "Character_Name");
            const char* display_name = xml_table_get_req_string_ref(element, "Display_Name");
            const char* bitmap_name = xml_table_get_req_string_ref(element, "Bitmap_Name");
            const char* group_name = xml_table_get_req_string_ref(element, "Persona_Group");
            int unlock_level;
            xml_table_get_req_int(unlock_level, element, "Unlock_Level");
            display_name = Multi_game_character_string_pool.add_unique(display_name);
            bitmap_name = Multi_game_character_string_pool.add_unique(bitmap_name);
            const human_info* hip = human_info_find(character_name);
            mp_game_character_info* info = Mp_character_info_pool.alloc(0);
            info->hip = hip;
            info->display_name = display_name;
            info->bitmap_name = bitmap_name;
            info->voice_persona = multi_game_player_get_voice_persona_list(group_name);
            info->unlock_level = unlock_level;
            for (int i = 0; i < 16; i++)
            {
                info->backpack_offsets[i].zero();
            }
            xml_element* offsets = xml_table_find_req(element, "backpack_offsets");
            for (xml_element* backpack = xml_find(offsets, "backpack"); backpack != nullptr; backpack = xml_find_next(offsets, backpack, "backpack"))
            {
                const int backpack_id = multi_game_backpack_get_backpack_id(xml_table_get_req_string_ref(backpack, "backpack_name"));
                if (backpack_id != -1)
                {
                    xml_table_get_req_vector(info->backpack_offsets[backpack_id], backpack, "backpack_offset");
                }
            }
            Team_lists[selection_type].list.add(info);
        }
    }
    xml_table_close(root);
}

// 0x009DD2B0 (:360)
void multi_game_character_init(mempool_base& pool)
{
    void* address = pool.alloc(Character_string_pool_size, 0);
    Multi_game_character_string_pool.init(address, Character_string_pool_size, nullptr, 0, 0);
    Mp_character_info_pool.init(pool, Max_character_infos);
    parse_weapon_unlockables(pool);
    parse_mp_personas(pool);
    parse_mp_characters();
}

// 0x009DD320 (:378)
void multi_game_character_shutdown()
{
    Mp_character_info_pool.thread_access.enter();
    for (mp_game_character_info* info = Mp_character_info_pool.get_first(0); info != nullptr; info = Mp_character_info_pool.free(info))
    {
    }
    Mp_character_info_pool.thread_access.leave();
}

// 0x0097A7F0 (:383)
const human_info* multi_game_charcter_get_human_info(human_teams team, unsigned int player_selection_bundle)
{
    return character_from_bundle(get_team_index(team), player_selection_bundle)->hip;
}

// 0x0097A860 (:399)
const voice_persona_list* multi_game_charcter_get_voice_persona_list(human_teams team, unsigned int player_selection_bundle)
{
    return character_from_bundle(get_team_index(team), player_selection_bundle)->voice_persona;
}

// 0x0097A8D0 (:415)
void multi_game_character_get_base_voice_persona_lists(int& num_voice_persona_lists, const voice_persona_list** vp_lists, int max_vp_lists)
{
    if (MP_persona_infos.size() < max_vp_lists)
    {
        max_vp_lists = MP_persona_infos.size();
    }
    num_voice_persona_lists = max_vp_lists;
    for (int i = 0; i < num_voice_persona_lists; i++)
    {
        vp_lists[i] = &MP_persona_infos[i].voice_personas;
    }
}

// 0x0097A910 (:425)
bool multi_game_player_is_locked(mp_character_selection_types selection_type, unsigned int player_selection_bundle)
{
    return (int)My_level < multi_game_player_get_info(selection_type, player_selection_bundle)->unlock_level;
}

// 0x0097A970 (:431)
const char* multi_game_player_get_display_name(mp_character_selection_types selection_type, unsigned int player_selection_bundle)
{
    return multi_game_player_get_info(selection_type, player_selection_bundle)->display_name;
}

// 0x0097A9C0 (:437)
const char* multi_game_player_get_bitmap_name(mp_character_selection_types selection_type, unsigned int player_selection_bundle)
{
    return multi_game_player_get_info(selection_type, player_selection_bundle)->bitmap_name;
}

// 0x0097AA10 (:443)
const vector* multi_game_player_get_backpack_offset(human_teams team, unsigned int player_selection_bundle, int backpack_id)
{
    return &multi_game_player_get_info(get_team_index(team), player_selection_bundle)->backpack_offsets[backpack_id];
}

// 0x0097AAA0 (:450): the type's byte of the bundle steps to the next or previous character (wrapping)
void multi_game_character_modify_player_selection(mp_character_selection_types selection_type, unsigned int& player_selection_bundle, bool inc)
{
    if ((unsigned int)selection_type >= NUM_MP_CHARACTER_SELECTION_TYPES)
    {
        return;
    }
    const int count = Team_lists[selection_type].list.size();
    const int shift = selection_type * 8;
    const int step = inc ? 1 : count - 1;
    const int index = ((int)(player_selection_bundle >> shift & 0xFF) + step) % count;
    player_selection_bundle = (unsigned int)index << shift | (~(0xFFu << shift) & player_selection_bundle);
}

// 0x0097AB00 (:468): the bundle of the profile's characters (0 without a profile)
unsigned int multi_game_character_get_profile_player_selection()
{
    player_profile_data* profile = player_profile_get("multi_game_character_get_profile_player_selection",
        Player_controller_index != 0xFFFFFFFF ? (int)Player_controller_index : 0);
    if (profile == nullptr)
    {
        return 0;
    }
    const human_info* ffa = human_info_find(profile->mp_ffa_player_crc);
    const unsigned int ffa_index = ffa != nullptr ? list_index_of(MP_SEL_FFA, ffa) : 0;
    const human_info* edf = human_info_find(profile->mp_edf_player_crc);
    const unsigned int edf_index = edf != nullptr ? list_index_of(MP_SEL_EDF, edf) & 0xFF : 0;
    const human_info* gur = human_info_find(profile->mp_gur_player_crc);
    const unsigned int gur_index = gur != nullptr ? list_index_of(MP_SEL_GUR, gur) & 0xFF : 0;
    return (gur_index << 8 | edf_index) << 8 | (ffa_index & 0xFF);
}

// 0x0097AC00 (:492): the bundle's characters into the profile, by name crc
void multi_game_data_store_profile_player_selection(unsigned int player_selection_bundle)
{
    const mp_game_character_info* ffa = character_from_bundle(MP_SEL_FFA, player_selection_bundle);
    const unsigned int ffa_crc = ffa->hip != nullptr ? ffa->hip->name_crc : 0;
    const mp_game_character_info* edf = character_from_bundle(MP_SEL_EDF, player_selection_bundle);
    const unsigned int edf_crc = edf->hip != nullptr ? edf->hip->name_crc : 0;
    const mp_game_character_info* gur = character_from_bundle(MP_SEL_GUR, player_selection_bundle);
    const unsigned int gur_crc = gur->hip != nullptr ? gur->hip->name_crc : 0;
    player_profile_data* profile = player_profile_get("multi_game_data_store_profile_player_selection",
        Player_controller_index != 0xFFFFFFFF ? (int)Player_controller_index : 0);
    if (profile != nullptr)
    {
        profile->mp_ffa_player_crc = ffa_crc;
        profile->mp_edf_player_crc = edf_crc;
        profile->mp_gur_player_crc = gur_crc;
    }
}

// 0x009510F0 (:511)
void multi_game_data_store_profile_sledge_selection(int weapon_unique_id)
{
    player_profile_data* profile = player_profile_get("multi_game_data_store_profile_sledge_selection",
        Player_controller_index != 0xFFFFFFFF ? (int)Player_controller_index : 0);
    if (profile != nullptr)
    {
        profile->sledge_unique_id = weapon_unique_id;
    }
}

// Port helper: the unlockable of a weapon (rfg.exe inlines the pool walk into both image getters).
static const mp_character_unlockable_weapon* find_unlockable_weapon(int weapon_unique_id)
{
    Mp_unlockable_weapon_pool.thread_access.enter();
    for (mp_character_unlockable_weapon* unlockable = Mp_unlockable_weapon_pool.get_first(0); unlockable != nullptr;
         unlockable = Mp_unlockable_weapon_pool.get_next(unlockable))
    {
        if (unlockable->unique_weapon_id == weapon_unique_id)
        {
            Mp_unlockable_weapon_pool.thread_access.leave();
            return unlockable;
        }
    }
    Mp_unlockable_weapon_pool.thread_access.leave();
    return nullptr;
}

// 0x00997310 (:519)
const char* multi_game_character_get_weapon_icon(int weapon_unique_id)
{
    const mp_character_unlockable_weapon* unlockable = find_unlockable_weapon(weapon_unique_id);
    return unlockable != nullptr ? unlockable->image_name : nullptr;
}

// 0x009973C0 (:531): the same image as the icon
const char* multi_game_character_get_weapon_image(int weapon_unique_id)
{
    const mp_character_unlockable_weapon* unlockable = find_unlockable_weapon(weapon_unique_id);
    return unlockable != nullptr ? unlockable->image_name : nullptr;
}

// 0x00951120 (:544)
const char* multi_game_character_get_weapon_display_name(int weapon_unique_id)
{
    const weapon_info* w_info = weapons_get_weapon_info_from_weapon_unique_id(weapon_unique_id);
    if (w_info != nullptr && w_info->weapon_inv_item_info != nullptr)
    {
        return w_info->weapon_inv_item_info->display_name;
    }
    return "unknown";
}

// 0x0097ACD0 (:552): the free for all characters, numbered from 1
void multi_game_character_get_xp_events(int& xp_event_count, multi_xp_character_unlock_info* event_infos, int max_multi_xp_character_unlock_infos)
{
    xp_event_count = 0;
    for (int i = 0; i < Team_lists[0].list.size(); i++)
    {
        multi_xp_character_unlock_info& info = event_infos[xp_event_count++];
        info.character_id = (unsigned int)i + 1;
        info.unlock_level = Team_lists[0].list[i]->unlock_level;
    }
}

// 0x0097AD20 (:565)
const char* multi_game_character_get_unlock_character_name(unsigned int character_id)
{
    if ((int)(character_id - 1) >= Team_lists[0].list.size())
    {
        return "unknown character";
    }
    return Team_lists[0].list[character_id - 1]->display_name;
}

// 0x00997470 (:573): the team of the profile's free for all character (EDF without a profile)
human_teams multi_game_character_get_my_base_team()
{
    player_profile_data* profile = player_profile_get("multi_game_character_get_my_base_team",
        Player_controller_index != 0xFFFFFFFF ? (int)Player_controller_index : 0);
    if (profile != nullptr)
    {
        const human_info* h_info;
        if (profile->mp_ffa_player_crc == 0)
        {
            h_info = multi_game_charcter_get_human_info(HUMAN_TEAM_CIVILIAN, 0);
        }
        else
        {
            h_info = human_info_find(profile->mp_ffa_player_crc);
        }
        const human_teams team = multi_game_character_get_base_team(h_info);
        if (team != HUMAN_TEAM_NONE)
        {
            return team;
        }
    }
    return HUMAN_TEAM_EDF;
}

// 0x009974D0 (:590)
human_teams multi_game_character_get_base_team(unsigned int player_selection_bundle)
{
    const mp_game_character_info* info = character_from_bundle(MP_SEL_FFA, player_selection_bundle);
    if (info->hip == nullptr)
    {
        return HUMAN_TEAM_NONE;
    }
    return multi_game_character_get_base_team(info->hip);
}

// 0x0097AD40 (:600): the music team of the character's persona group
human_teams multi_game_character_get_base_team(const human_info* h_info)
{
    for (int i = 0; i < Team_lists[0].list.size(); i++)
    {
        if (Team_lists[0].list[i]->hip != h_info)
        {
            continue;
        }
        for (int j = 0; j < MP_persona_infos.size(); j++)
        {
            if (&MP_persona_infos[j].voice_personas == Team_lists[0].list[i]->voice_persona)
            {
                return MP_persona_infos[j].music_team;
            }
        }
    }
    return HUMAN_TEAM_NONE;
}
