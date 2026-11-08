// Original: volition/rfg/code/objects/humans/human_voice/human_voice.cpp
//
// Human voice playback and voice slotting (decompiled):
// - Playback: a persona line picks its next valid variation, honours its local and per-group global cooldowns, and
//   plays the cue 3D at the speaker, 2D (radio, PC transmissions, the announcer) or through the HUD radio, the subtitle
//   line, public address or the EDF projected start cue, by the line's flags.
// - Streaming: each persona is a stream2 container (human_voice_shared.cpp); its GSP_VOICE_PERSONA primitive holds the
//   line data, registered under its file name by the load callback and attached to the persona by the container
//   callback with a lookup of its situational lines.
// - Voice slotting (single player): at most 64 personas are streamed; the slots go to the players, the radio, public
//   address, the mission's required personas, activities and cutscenes first, then to the personas the npcs use, and
//   personas are combined, deleted or reallocated per team when that is too many. Multiplayer only streams the
//   multiplayer personas and the requested ones (human_voice_persona_level_init_pre).
// The external interface is declared in human_voice.h. SFX.md SX6: ported whole.

#include "volition/rfg/code/objects/humans/human_voice/human_voice.h"

#include "keen/base/string.hpp"
#include "keen/base/trace.hpp"
#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/rfg/code/gamewide/downloadable_content.h"
#include "volition/rfg/code/gamewide/gameaudio.h"
#include "volition/rfg/code/gamewide/gameseq.h"
#include "volition/rfg/code/hud/hud_ui_mission_briefing.h"
#include "volition/rfg/code/interface/localization/localization_shared.h"
#include "volition/rfg/code/interface/ui_document_hud.h"
#include "volition/rfg/code/interface/ui_message_box.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_game/multi_game_character.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_info.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice_internal_shared.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice_lines.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice_shared.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/rfg.h"
#include "volition/rfg/code/scripting/public_address.h"
#include "volition/stream2/code/stream2.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/math/crc.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/memory/object_pool.h"
#include "volition/vlib/code/memory/rfg_split_mempool.h"
#include "volition/vlib/code/memory/virtual_mempool.h"
#include "volition/vlib/code/memory/vmemory.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/vlib/code/time/timestamp.h"
#include "volition/vlib/code/util/fastxml.h"
#include "volition/vlib/code/util/hash_table.h"
#include "volition/vlib/code/util/string_pool.h"
#include "volition/vlib/code/util/string_util.h"
#include "volition/vlib/code/util/varray.h"

#include <stdio.h>
#include <string.h>

// ---- file-local types (rfg.pdb)
// size 0x4
struct line_variation_to_disable
{
    unsigned short line; // 0x00
    unsigned short variation; // 0x02
};
static_assert(sizeof(line_variation_to_disable) == 0x4, "line_variation_to_disable");

// The per-persona .vpd_pc file: a count, then that many (line, variation) pairs (the PDB records the first one).
// size 0x8
struct human_voice_line_disable_data
{
    unsigned int num_entries; // 0x00
    line_variation_to_disable disable_data; // 0x04
};
static_assert(sizeof(human_voice_line_disable_data) == 0x8, "human_voice_line_disable_data");

// size 0x8
struct activity_voice_info
{
    unsigned int spawn_node_handle; // 0x00
    human_voice_persona_handle desired_persona_handle; // 0x04
};
static_assert(sizeof(activity_voice_info) == 0x8, "activity_voice_info");

// The index of each situational voice_lines value in a loaded persona's lines (0xFFFF: none).
// size 0x102
struct voice_lines_lookup_table
{
    unsigned short entries[129]; // 0x00
};
static_assert(sizeof(voice_lines_lookup_table) == 0x102, "voice_lines_lookup_table");

// ---- globals
int INVALID_VOICE_SET_HANDLE = -1; // 0x01694400
const char* XML_MISSION_PERSONA_TAG_LIST[5] = {"voice_persona_0", "voice_persona_1", "voice_persona_2", "voice_persona_3", "voice_persona_4"}; // 0x0169440C, read from rfg.exe
farray<mission_persona_list,70> All_misison_persona_lists; // 0x01694A50
farray<human_voice_persona,150> Voice_personas; // 0x030F7E78
int Num_voice_line_descriptors; // 0x0302F088
int Num_dlc_voice_line_descriptors; // 0x0302F090
human_voice_line_info* Dlc_voice_line_descriptors; // 0x0302F094

// ---- file-static globals
static const char XML_PERSONA_FLAG_MP_TAG[12] = "multi_lines"; // 0x01310F2C
static const char RADIO_MESSAGE_PERSONA_NAME[7] = "OFFCAM"; // 0x01310F78
static const char XML_PERSONA_FLAG_ROOT_TAG[14] = "persona_flags"; // 0x01310F80
static const char XML_PERSONA_FLAG_NAME_TAG[5] = "Name"; // 0x01310F90
static const char XML_PERSONA_FLAG_PRELOAD_TAG[8] = "preload"; // 0x01310F98
static const char XML_PERSONA_FLAG_PLAYER_TAG[15] = "player_persona"; // 0x01310FA0
static const char XML_PERSONA_FLAG_SPECIAL_TAG[22] = "special_persona_flags"; // 0x01310FB0
static const char XML_ROOT_MISSION_LIST_TAG[19] = "voice_persona_list"; // 0x01310FC8
static human_voice_persona_handle Radio_message_persona_handle = INVALID_HUMAN_VOICE_PERSONA_HANDLE; // 0x01694404
static human_voice_persona_handle DLC_radio_message_persona_handle = INVALID_HUMAN_VOICE_PERSONA_HANDLE; // 0x01694408
static bool Check_dlc_offcam = true; // 0x01694420
static data_hash_table<unsigned int,human_voice_line_disable_data*,&hash_uint,50,unsigned char> Voice_line_disable_hash(false); // 0x01694850
static timestamp Voice_cleanup_period; // 0x01694A4C, invalid (-1) from the constructor
static farray<activity_voice_info,2> Activity_voice_list; // 0x01694FD8
static farray<enum human_voice_persona_handle,3> Cutscene_voice_list; // 0x01694FF8
static char Persona_string_pool_memory[9600]; // 0x03017C78
// The persona names (dynamic initializer at 0x010754A0: the storage above, "persona name string pool").
string_pool Persona_string_pool(Persona_string_pool_memory, sizeof(Persona_string_pool_memory), "persona name string pool"); // 0x031A2AB8
static bool Reallocate_voice_slots; // 0x0302F075
static bool Persona_stream_do_frame; // 0x0302F076
static bool Human_voice_init; // 0x0302F077
static int All_misison_persona_lists_mark; // 0x0302F080
// human_voice_line_info* Voice_line_descriptors (0x0302F08C) is used here too; placed in human.cpp.
static stream2_data_hash<human_voice_persona_data_header*,70,unsigned char> Voice_persona_data_hash; // 0x03067AE0
static object_pool<voice_lines_lookup_table,70,1> Situational_lines_allocator; // 0x030C0720
static int Required_set_list_handle; // 0x033B1FCC
static mission_persona_list Required_persona_list; // 0x033B58F8
// checksum_stri THERMO_ROCKET_CHECKSUM (0x033B590C) is used here too; placed in human.cpp.

// Magic number: at most 64 personas are streamed at once.
static const int MAX_STREAMED_PERSONAS = 64;

// ---- persona_flags_struct / human_persona_information / mission_persona_list

// 0x00A31720 (:86)
mission_persona_list::mission_persona_list()
{
    for (int i = 0; i < 5; i++)
    {
        mission_personas[i] = nullptr;
    }
}

// 0x00A0D730 (:2033)
void mission_persona_list::reset()
{
    for (int i = 0; i < 5; i++)
    {
        mission_personas[i] = nullptr;
    }
}

// 0x00A0D750 (:2042): the list is kept sorted by address, largest first, without duplicates.
void mission_persona_list::add(human_voice_persona* new_persona)
{
    for (int i = 0;; i++)
    {
        human_voice_persona* cur = mission_personas[i];
        if (new_persona == cur)
        {
            return;
        }
        if (cur == nullptr)
        {
            mission_personas[i] = new_persona;
            return;
        }
        if (cur < new_persona)
        {
            mission_personas[i] = new_persona;
            new_persona = cur;
        }
        if (i + 1 > 4)
        {
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n");
            return;
        }
    }
}

// 0x00A31740 (:104)
persona_flags_struct::persona_flags_struct()
{
    *(unsigned int*)this = 0;
}

// 0x00A0D7A0 (:2072)
void persona_flags_struct::reset()
{
    *(unsigned int*)this = 0;
}

// 0x00A0D7B0 (:2078)
bool persona_flags_struct::slot_is_required() const
{
    return player_slot || activity_raid || raid_intro || activity_ha_hostage || miner_persona || activity_slot || mission_slot || radio_slot || pa_slot || debug_slot || cutscene_slot;
}

// 0x00A31750 (:143)
human_persona_information::human_persona_information()
{
}

// 0x00A0D7D0 (:2091)
void human_persona_information::reset()
{
    persona_handle = INVALID_HUMAN_VOICE_PERSONA_HANDLE;
    persona = nullptr;
    human_count = 0;
    persona_flags.reset();
    *(unsigned char*)&loading_flags = 0;
}

// 0x00A319E0 (:2101)
bool human_persona_information::slot_is_required() const
{
    return persona_flags.slot_is_required() || persona->is_always_loaded();
}

// ---- internal functions

// The persona's handle when p points into Voice_personas (inlined human_voice_persona_get_handle).
static int persona_index(const human_voice_persona* p)
{
    return human_voice_persona_get_handle(p);
}

// 0x00A31790 (:730): the streamed personas that are not always loaded.
static int count_loaded_personas()
{
    int count = 0;
    for (int i = 0; i < Voice_personas.size(); i++)
    {
        if (!Voice_personas[i].is_always_loaded() && Voice_personas[i].is_loading())
        {
            count++;
        }
    }
    return count;
}

// 0x00A31810 (:751)
static int count_free_personas()
{
    const int free = MAX_STREAMED_PERSONAS - count_loaded_personas();
    return free < 1 ? 0 : free;
}

// 0x00A56B30 (:760). LTCG: persona_info_list in EDI. One entry per persona, then the required slot flags: the
// players' voices, the radio persona, the active public address persona, the mission's required personas, the
// activities' reserved voices and the cutscenes'.
static void init_persona_info_list(farray<human_persona_information,150>& persona_info_list)
{
    persona_info_list.remove_all();
    for (int i = 0; i < Voice_personas.size(); i++)
    {
        human_persona_information info;
        info.persona_handle = (human_voice_persona_handle)i;
        info.persona = &Voice_personas[i];
        info.human_count = 0;
        info.persona_flags.reset();
        *(unsigned char*)&info.loading_flags = 0;
        if (persona_info_list.full())
        {
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n");
            continue;
        }
        persona_info_list.add(info);
    }
    const int num_players = World.get_num_objects<player>();
    for (int i = 0; i < num_players; i++)
    {
        const human* hp = World.get_object<player>(i);
        const int index = persona_index(hp->voice);
        if (hp->voice != nullptr && index != -1)
        {
            persona_info_list[index].persona_flags.player_slot = 1;
        }
    }
    const human_voice_persona_handle radio = human_voice_radio_persona_handle();
    if (radio != INVALID_HUMAN_VOICE_PERSONA_HANDLE)
    {
        persona_info_list[radio].persona_flags.radio_slot = 1;
    }
    const human_voice_persona_handle pa = public_address_kiosk_get_active_persona();
    if (pa != INVALID_HUMAN_VOICE_PERSONA_HANDLE)
    {
        persona_info_list[pa].persona_flags.pa_slot = 1;
    }
    for (int i = 0; i < 5; i++)
    {
        const int index = persona_index(Required_persona_list.mission_personas[i]);
        if (Required_persona_list.mission_personas[i] != nullptr && index != -1)
        {
            persona_info_list[index].persona_flags.mission_slot = 1;
        }
    }
    for (int i = 0; i < Activity_voice_list.size(); i++)
    {
        if (Activity_voice_list[i].desired_persona_handle != INVALID_HUMAN_VOICE_PERSONA_HANDLE && Activity_voice_list[i].spawn_node_handle != 0xFFFFFFFF)
        {
            persona_info_list[Activity_voice_list[i].desired_persona_handle].persona_flags.activity_slot = 1;
        }
    }
    for (int i = 0; i < Cutscene_voice_list.size(); i++)
    {
        if (Cutscene_voice_list[i] != INVALID_HUMAN_VOICE_PERSONA_HANDLE)
        {
            persona_info_list[Cutscene_voice_list[i]].persona_flags.cutscene_slot = 1;
        }
    }
}

// 0x00A56D70 (:847). LTCG: persona_info_list in ESI. The humans each persona speaks for (not the dead ones whose voice
// is done), and the activity slots their special lines need.
static void persona_info_count_used_human(farray<human_persona_information,150>& persona_info_list)
{
    for (int i = 0; i < persona_info_list.size(); i++)
    {
        persona_info_list[i].human_count = 0;
        persona_info_list[i].persona_flags.activity_ha_hostage = 0;
        persona_info_list[i].persona_flags.activity_raid = 0;
        persona_info_list[i].persona_flags.raid_intro = 0;
        persona_info_list[i].persona_flags.miner_persona = 0;
    }
    const int num = World.get_num_objects<human>();
    for (int i = 0; i < num; i++)
    {
        human* hp = World.get_object<human>(i);
        if (hp->voice == nullptr || hp->voice_persona_finished())
        {
            continue;
        }
        const int index = persona_index(hp->voice);
        if (index == -1)
        {
            continue;
        }
        human_persona_information& info = persona_info_list[index];
        info.human_count++;
        if (hp->hflags.activity_ha_hostage && info.persona->flags.ha_hostage_lines)
        {
            info.persona_flags.activity_ha_hostage = 1;
        }
        if ((hp->hflags.activity_raid_required || hp->hflags.activity_raid_optional) && info.persona->flags.raid_lines)
        {
            info.persona_flags.activity_raid = 1;
        }
        if (hp->hflags.raid_introduction_lines && info.persona->flags.raid_into_lines)
        {
            info.persona_flags.raid_intro = 1;
        }
        if (hp->hflags.miner_persona_lines && info.persona->flags.miner_persona)
        {
            info.persona_flags.miner_persona = 1;
        }
    }
}

// 0x00A56F00 (:899): the other voices of the list that stay loaded.
static bool build_alternate_voice_list(farray<human_voice_persona*,64>& new_voice_list, const human_voice_persona* old_voice, const voice_persona_list& voice_personas, const farray<human_persona_information,150>& persona_info_list)
{
    if (old_voice == nullptr)
    {
        return false;
    }
    new_voice_list.remove_all();
    for (int i = 0; i < voice_personas.voice_list.size(); i++)
    {
        human_voice_persona* voice = voice_personas.voice_list[i];
        if (voice == old_voice)
        {
            continue;
        }
        const human_voice_persona_handle handle = human_voice_persona_get_handle(voice);
        if (handle != INVALID_HUMAN_VOICE_PERSONA_HANDLE && persona_info_list[handle].loading_flags.load_persona)
        {
            new_voice_list.add(voice_personas.voice_list[i]);
        }
    }
    return new_voice_list.size() > 0;
}

// 0x00A56F80 (:929). LTCG: voice_personas in EAX, old_voice and persona_info_list on the stack.
static bool find_alternate_voice(const human_voice_persona* old_voice, const voice_persona_list& voice_personas, const farray<human_persona_information,150>& persona_info_list)
{
    if (old_voice == nullptr)
    {
        return false;
    }
    for (int i = 0; i < voice_personas.voice_list.size(); i++)
    {
        if (voice_personas.voice_list[i] == old_voice)
        {
            continue;
        }
        const human_voice_persona_handle handle = human_voice_persona_get_handle(voice_personas.voice_list[i]);
        if (handle != INVALID_HUMAN_VOICE_PERSONA_HANDLE && persona_info_list[handle].loading_flags.load_persona)
        {
            return true;
        }
    }
    return false;
}

// 0x00A56FE0 (:958): the slot can be freed when every human using the persona has another loaded voice of its list;
// they switch to a random one.
static bool combine_human_persona_slot(const farray<human_persona_information,150>& persona_info_list, human_persona_information& persona_info, bool can_use_required)
{
    farray<human_voice_persona*,64> alternates;
    farray<human*,116> humans;
    if (!can_use_required && (persona_info.persona_flags.slot_is_required() || persona_info.persona->is_always_loaded()))
    {
        return false;
    }
    const int num = World.get_num_objects<human>();
    for (int i = 0; i < num; i++)
    {
        human* hp = World.get_object<human>(i);
        if (hp->voice != persona_info.persona || hp->h_info == nullptr)
        {
            continue;
        }
        if (!find_alternate_voice(hp->voice, hp->h_info->voice_personas, persona_info_list))
        {
            return false;
        }
        if (humans.full())
        {
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n");
            continue;
        }
        humans.add(hp);
    }
    for (int i = 0; i < humans.size(); i++)
    {
        human* hp = humans[i];
        if (hp->h_info != nullptr && build_alternate_voice_list(alternates, hp->voice, hp->h_info->voice_personas, persona_info_list) && alternates.size() > 0)
        {
            const int index = alternates.size() == 1 ? 0 : int_rand_range(0, alternates.size() - 1);
            hp->voice = alternates[index];
        }
    }
    return true;
}

// 0x00A57220 (:1031). LTCG: persona_info_list in ESI. The first loading persona whose slot can be combined is
// unloaded.
static bool combine_human_personas(farray<human_persona_information,150>& persona_info_list)
{
    for (int i = 0; i < persona_info_list.size(); i++)
    {
        human_persona_information& info = persona_info_list[i];
        if (info.persona_flags.slot_is_required() || info.persona->is_always_loaded())
        {
            continue;
        }
        if (info.loading_flags.load_persona && combine_human_persona_slot(persona_info_list, info, false))
        {
            info.loading_flags.load_persona = false;
            info.loading_flags.unload_persona = true;
            return true;
        }
    }
    return false;
}

// 0x00A572D0 (:1051). LTCG: hp in ECX. Original: the loop looks at the human's own voice each time, not at the list's
// voices, so a human whose voice is being unloaded never gets a new one here.
static human_voice_persona* pick_new_human_voice(farray<human_persona_information,150>& persona_info_list, const human* hp)
{
    if (hp->h_info == nullptr)
    {
        return nullptr;
    }
    const int count = hp->h_info->voice_personas.voice_list.size();
    const human_voice_persona_handle handle = human_voice_persona_get_handle(hp->voice);
    for (int i = 0; i < count; i++)
    {
        if (handle != INVALID_HUMAN_VOICE_PERSONA_HANDLE && persona_info_list[handle].loading_flags.load_persona)
        {
            return persona_info_list[handle].persona;
        }
    }
    return nullptr;
}

// 0x00A57330 (:1075). LTCG: persona_info_list in ESI. The humans of unloaded personas get a new voice, and the
// personas are marked to be loaded or unloaded by do_persona_streaming.
static void commit_persona_load_changes(farray<human_persona_information,150>& persona_info_list)
{
    const int num = World.get_num_objects<human>();
    for (int i = 0; i < num; i++)
    {
        human* hp = World.get_object<human>(i);
        const int index = persona_index(hp->voice);
        if (index == -1 || !persona_info_list[index].loading_flags.unload_persona)
        {
            continue;
        }
        human_voice_persona* voice = pick_new_human_voice(persona_info_list, hp);
        if (voice != nullptr)
        {
            hp->voice = voice;
        }
    }
    for (int i = 0; i < persona_info_list.size(); i++)
    {
        human_persona_information& info = persona_info_list[i];
        if (info.loading_flags.load_persona)
        {
            info.persona->set_to_be_loaded_flag();
            info.loading_flags.load_persona = false;
        }
        else if (info.loading_flags.unload_persona)
        {
            info.persona->set_to_be_unloaded_flag();
            info.loading_flags.unload_persona = false;
        }
    }
    Persona_stream_do_frame = true;
}

// 0x00A31830 (:1121). LTCG: persona_info_list in ECX.
static int count_loading_personas(farray<human_persona_information,150>& persona_info_list)
{
    int count = 0;
    for (int i = 0; i < persona_info_list.size(); i++)
    {
        if (persona_info_list[i].loading_flags.load_persona)
        {
            count++;
        }
    }
    return count;
}

// A loading persona that is neither required nor always loaded (the candidate test of delete_human_persona).
static bool is_deletable_persona(const human_persona_information& info)
{
    return info.loading_flags.load_persona && !info.persona_flags.slot_is_required() && !info.persona->is_always_loaded();
}

// 0x00A57440 (:1137). LTCG: persona_info_list in EDI. The deletable persona used by the fewest humans (the first of
// them) is unloaded.
static bool delete_human_persona(farray<human_persona_information,150>& persona_info_list)
{
    int best = 0;
    while (best < persona_info_list.size() && !is_deletable_persona(persona_info_list[best]))
    {
        best++;
    }
    if (best >= persona_info_list.size())
    {
        return false;
    }
    for (int i = best + 1; i < persona_info_list.size(); i++)
    {
        if (is_deletable_persona(persona_info_list[i]) && persona_info_list[i].human_count < persona_info_list[best].human_count)
        {
            best = i;
        }
    }
    persona_info_list[best].loading_flags.load_persona = false;
    persona_info_list[best].loading_flags.unload_persona = true;
    return true;
}

// 0x00A57590 (:1172): every 30 s, the personas no human uses that are not required are unloaded.
static void do_voice_clean_up()
{
    if (!Voice_cleanup_period.elapsed())
    {
        return;
    }
    // Magic number: the clean-up runs every 30 s.
    Voice_cleanup_period.set_rand(30000, 30000);
    farray<human_persona_information,150> persona_info_list;
    init_persona_info_list(persona_info_list);
    persona_info_count_used_human(persona_info_list);
    for (int i = 0; i < persona_info_list.size(); i++)
    {
        human_persona_information& info = persona_info_list[i];
        if (info.persona_flags.slot_is_required() || info.persona->is_always_loaded() || info.human_count > 0)
        {
            info.loading_flags.load_persona = true;
        }
        else if (info.persona->is_loading())
        {
            info.loading_flags.unload_persona = true;
        }
    }
    commit_persona_load_changes(persona_info_list);
}

// 0x00A576D0 (:1209)
static int count_required_persona_slots(const farray<human_persona_information,150>& persona_info_list)
{
    int count = 0;
    for (int i = 0; i < persona_info_list.size(); i++)
    {
        if (persona_info_list[i].persona_flags.slot_is_required() && !persona_info_list[i].persona->is_always_loaded())
        {
            count++;
        }
    }
    return count;
}

// A slot required only by an activity's raid or miner lines (the candidates of remove_required_slot and
// reduce_required_slots); flags gets the persona flags without those two.
static bool is_activity_only_slot(const human_persona_information& info, persona_flags_struct& flags)
{
    flags = info.persona_flags;
    if (!flags.activity_raid && !flags.miner_persona)
    {
        return false;
    }
    flags.activity_raid = 0;
    flags.miner_persona = 0;
    return !flags.slot_is_required();
}

// 0x00A577A0 (:1227). LTCG: persona_info_list in EDI.
static bool remove_required_slot(farray<human_persona_information,150>& persona_info_list)
{
    for (int i = 0; i < persona_info_list.size(); i++)
    {
        human_persona_information& info = persona_info_list[i];
        persona_flags_struct flags;
        if (is_activity_only_slot(info, flags) && combine_human_persona_slot(persona_info_list, info, true))
        {
            info.persona_flags = flags;
            info.loading_flags.load_persona = false;
            info.loading_flags.unload_persona = true;
            return true;
        }
    }
    return false;
}

// 0x00A57810 (:1263). LTCG: persona_info_list in EDI, count on the stack.
static void reduce_required_slots(farray<human_persona_information,150>& persona_info_list, int count)
{
    for (int n = 0; n < count; n++)
    {
        if (!remove_required_slot(persona_info_list))
        {
            return;
        }
    }
}

// 0x00A578A0 (:1275). LTCG: np in EAX, persona_info_list on the stack.
static bool has_required_voice(const npc* np, const farray<human_persona_information,150>& persona_info_list)
{
    const int index = persona_index(np->voice);
    if (index == -1)
    {
        return false;
    }
    return persona_info_list[index].persona_flags.slot_is_required() || persona_info_list[index].persona->is_always_loaded();
}

// 0x00A57980 (:1286). LTCG: np in EAX, persona_info_list on the stack.
static bool can_use_loading_persona(const npc* np, const farray<human_persona_information,150>& persona_info_list)
{
    if (np == nullptr)
    {
        return false;
    }
    const voice_persona_list& voices = np->h_info->voice_personas;
    for (int i = 0; i < voices.voice_list.size(); i++)
    {
        const human_voice_persona_handle handle = human_voice_persona_get_handle(voices.voice_list[i]);
        if (handle != INVALID_HUMAN_VOICE_PERSONA_HANDLE && persona_info_list[handle].loading_flags.load_persona)
        {
            return true;
        }
    }
    return false;
}

// 0x00A579E0 (:1314). LTCG: npc_list in ECX, persona_info_list and desired_team on the stack. The persona most of the
// team's npcs can use (a random one of the most used) is loaded, and the npcs that now have a loading voice leave the
// list.
static bool pick_persona_for_team(farray<npc*,100>& npc_list, farray<human_persona_information,150>& persona_info_list, human_teams desired_team)
{
    farray<human_voice_persona_handle,150> best_personas;
    farray<npc*,100> team_npcs;
    for (int i = 0; i < npc_list.size(); i++)
    {
        if (npc_list[i]->current_team != desired_team)
        {
            continue;
        }
        if (team_npcs.full())
        {
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n");
            continue;
        }
        team_npcs.add(npc_list[i]);
    }
    if (team_npcs.size() < 1)
    {
        return false;
    }
    int counts[150] = {};
    for (int i = 0; i < team_npcs.size(); i++)
    {
        const voice_persona_list& voices = team_npcs[i]->h_info->voice_personas;
        for (int j = 0; j < voices.voice_list.size(); j++)
        {
            const int index = persona_index(voices.voice_list[j]);
            if (index != -1 && index < 150)
            {
                counts[index]++;
            }
        }
    }
    int best = 0;
    for (int i = 1; i < 150; i++)
    {
        if (counts[best] < counts[i])
        {
            best = i;
        }
    }
    const int max_count = counts[best];
    if (max_count < 1)
    {
        return false;
    }
    for (int i = 0; i < 150; i++)
    {
        if (counts[i] == max_count)
        {
            best_personas.add((human_voice_persona_handle)i);
        }
    }
    if (best_personas.size() < 1)
    {
        return false;
    }
    const human_voice_persona_handle pick = best_personas[int_rand_range(0, best_personas.size() - 1)];
    persona_info_list[pick].loading_flags.load_persona = true;
    persona_info_list[pick].loading_flags.unload_persona = false;
    for (int i = 0; i < npc_list.size();)
    {
        if (can_use_loading_persona(npc_list[i], persona_info_list))
        {
            npc_list.remove_unsorted(i);
        }
        else
        {
            i++;
        }
    }
    return true;
}

// 0x00A57C60 (:1407)
// 0x00A57EB0 (:1407). LTCG: persona_info_list in EDI. Only the required and always loaded personas stay; the free
// slots go one team at a time (EDF first, then civilians, marauders, guerrillas) to the persona most of the team's
// voiceless npcs can use.
static void full_voice_reallocation(farray<human_persona_information,150>& persona_info_list)
{
    int required = 0;
    for (int i = 0; i < persona_info_list.size(); i++)
    {
        human_persona_information& info = persona_info_list[i];
        if (info.persona->is_always_loaded())
        {
            info.loading_flags.load_persona = true;
            info.loading_flags.unload_persona = false;
        }
        else if (info.persona_flags.slot_is_required())
        {
            required++;
            info.loading_flags.load_persona = true;
            info.loading_flags.unload_persona = false;
        }
        else
        {
            info.loading_flags.load_persona = false;
            info.loading_flags.unload_persona = true;
        }
    }
    int free = MAX_STREAMED_PERSONAS - required;
    if (free > 0)
    {
        farray<npc*,100> npcs;
        const int num_npcs = World.get_num_objects<npc>();
        for (int i = 0; i < num_npcs; i++)
        {
            npc* np = World.get_object<npc>(i);
            if (has_required_voice(np, persona_info_list) || can_use_loading_persona(np, persona_info_list))
            {
                continue;
            }
            if (npcs.full())
            {
                keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n");
                continue;
            }
            npcs.add(np);
        }
        if (npcs.size() != 0)
        {
            // Magic number: four passes per npc over the teams.
            const int passes = npcs.size() * 4;
            int team = HUMAN_TEAM_EDF;
            for (int i = 0; i < passes; i++)
            {
                if (free < 1 || npcs.size() == 0)
                {
                    break;
                }
                if (pick_persona_for_team(npcs, persona_info_list, (human_teams)team))
                {
                    free--;
                }
                if (++team > HUMAN_TEAM_MARAUDER)
                {
                    team = HUMAN_TEAM_GUERILLA;
                }
            }
        }
    }
    commit_persona_load_changes(persona_info_list);
}

// 0x00A57ED0 (:1639): the slots are reassigned: required and used personas load; when that is more than 64 the
// activity-only slots are dropped, then personas are combined, then deleted, and as a last resort reallocated per team.
static void do_reallocate_voice_slots()
{
    farray<human_persona_information,150> persona_info_list;
    init_persona_info_list(persona_info_list);
    persona_info_count_used_human(persona_info_list);
    const int over = count_required_persona_slots(persona_info_list) - MAX_STREAMED_PERSONAS;
    if (over > 0)
    {
        reduce_required_slots(persona_info_list, over);
    }
    int required = 0;
    int loading = 0;
    if (persona_info_list.size() > 0)
    {
        for (int i = 0; i < persona_info_list.size(); i++)
        {
            human_persona_information& info = persona_info_list[i];
            if (info.persona->is_always_loaded())
            {
                info.loading_flags.load_persona = true;
            }
            else if (info.persona_flags.slot_is_required())
            {
                info.loading_flags.load_persona = true;
                required++;
                loading++;
            }
            else if (info.human_count > 0)
            {
                info.loading_flags.load_persona = true;
                loading++;
            }
            else if (info.persona->is_loading())
            {
                info.loading_flags.unload_persona = true;
            }
        }
        if (required > MAX_STREAMED_PERSONAS)
        {
            for (int i = 0; i < required - MAX_STREAMED_PERSONAS; i++)
            {
                if (!remove_required_slot(persona_info_list))
                {
                    keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n");
                    break;
                }
            }
        }
        if (loading > MAX_STREAMED_PERSONAS)
        {
            for (int i = 0; i < MAX_STREAMED_PERSONAS; i++)
            {
                if (!combine_human_personas(persona_info_list))
                {
                    break;
                }
                if (count_loading_personas(persona_info_list) <= MAX_STREAMED_PERSONAS)
                {
                    commit_persona_load_changes(persona_info_list);
                    return;
                }
            }
            for (int i = 0; i < MAX_STREAMED_PERSONAS; i++)
            {
                if (!delete_human_persona(persona_info_list))
                {
                    break;
                }
                if (count_loading_personas(persona_info_list) <= MAX_STREAMED_PERSONAS)
                {
                    commit_persona_load_changes(persona_info_list);
                    return;
                }
            }
            full_voice_reallocation(persona_info_list);
            return;
        }
    }
    commit_persona_load_changes(persona_info_list);
}

// 0x00A31850 (:1755): the personas marked to be unloaded are released, then the marked ones are requested while slots
// are free. Original: a released persona keeps its to_be_unloaded flag.
static void do_persona_streaming()
{
    for (int i = 0; i < Voice_personas.size(); i++)
    {
        human_voice_persona& p = Voice_personas[i];
        if (!p.flags.to_be_unloaded)
        {
            continue;
        }
        if (p.is_loading())
        {
            const stream2_status_val status = stream2_get_status(p.get_srid());
            p.flags.background_requested = 0;
            p.flags.to_be_loaded = 0;
            if (status != ST_ERROR && (status == ST_LOADING || status == ST_LOADED))
            {
                stream2_release(p.get_srid());
            }
        }
        else
        {
            p.flags.to_be_unloaded = 0;
        }
    }
    int free = MAX_STREAMED_PERSONAS - count_loaded_personas();
    if (free < 1)
    {
        Persona_stream_do_frame = true;
        return;
    }
    if (Voice_personas.size() < 1)
    {
        return;
    }
    for (int i = 0; i < Voice_personas.size(); i++)
    {
        human_voice_persona& p = Voice_personas[i];
        if (p.is_loading())
        {
            p.flags.to_be_loaded = 0;
        }
        else if (p.flags.to_be_loaded)
        {
            p.flags.background_requested = 0;
            p.flags.to_be_loaded = 0;
            if (stream2_get_status(p.get_srid()) != ST_ERROR && stream2_get_ref_count(p.get_srid()) < 1)
            {
                stream2_request(p.get_srid());
            }
            if (--free < 1)
            {
                break;
            }
        }
    }
    for (int i = 0; i < Voice_personas.size(); i++)
    {
        if (Voice_personas[i].flags.to_be_loaded || Voice_personas[i].flags.to_be_unloaded)
        {
            Persona_stream_do_frame = true;
        }
    }
}

// 0x00A0D630 (:1842). LTCG: xml_flag_root in EDI.
static bool flag_exists_in_xml_human_voice(xml_element* xml_flag_root, const char* flag_tag)
{
    if (xml_flag_root == nullptr)
    {
        return false;
    }
    for (xml_element* cur = xml_find(xml_flag_root, "Flag"); cur != nullptr; cur = xml_find_next(xml_flag_root, cur, "Flag"))
    {
        if (v_stricmp(flag_tag, cur->text) == 0)
        {
            return true;
        }
    }
    return false;
}

// 0x00A0D690 (:1859). LTCG: xml_special_flags in EDI, the persona's flags in ESI.
static void set_special_persona_flags(xml_element* xml_special_flags, human_voice_persona* persona)
{
    persona->flags.raid_lines = flag_exists_in_xml_human_voice(xml_special_flags, "Raid") ? 1 : 0;
    persona->flags.raid_into_lines = flag_exists_in_xml_human_voice(xml_special_flags, "Raid_Intro") ? 1 : 0;
    persona->flags.ha_hostage_lines = flag_exists_in_xml_human_voice(xml_special_flags, "House_Arrest") ? 1 : 0;
    persona->flags.miner_persona = flag_exists_in_xml_human_voice(xml_special_flags, "Miner_Persona") ? 1 : 0;
}

// 0x00A0D710 (:2011). LTCG: list_a in ECX, list_b in EDX.
static bool same_mission_persona_lists(const mission_persona_list& list_a, const mission_persona_list& list_b)
{
    for (int i = 0; i < 5; i++)
    {
        if (list_a.mission_personas[i] != list_b.mission_personas[i])
        {
            return false;
        }
    }
    return true;
}

// 0x00A31F20 (:3445): the preloaded and always loaded personas.
static bool voice_persona_game_preload_default()
{
    Check_dlc_offcam = false;
    for (int i = 0; i < Voice_personas.size(); i++)
    {
        human_voice_persona& p = Voice_personas[i];
        if (p.flags.preload || p.flags.always_loaded)
        {
            p.flags.background_requested = 0;
            if (stream2_get_status(p.get_srid()) != ST_ERROR && stream2_get_ref_count(p.get_srid()) < 1)
            {
                stream2_preload(p.get_srid());
            }
        }
    }
    Check_dlc_offcam = true;
    DLC_radio_message_persona_handle = INVALID_HUMAN_VOICE_PERSONA_HANDLE;
    return true;
}

// 0x00A31FA0 (:3471): the multiplayer personas (multi_lines: the announcer).
static bool voice_persona_game_preload_multi()
{
    for (int i = 0; i < Voice_personas.size(); i++)
    {
        human_voice_persona& p = Voice_personas[i];
        if (p.flags.multiplayer)
        {
            p.flags.background_requested = 0;
            if (stream2_get_status(p.get_srid()) != ST_ERROR && stream2_get_ref_count(p.get_srid()) < 1)
            {
                stream2_preload(p.get_srid());
            }
        }
    }
    DLC_radio_message_persona_handle = INVALID_HUMAN_VOICE_PERSONA_HANDLE;
    return true;
}

// 0x00A32020 (:3494): the DLC's preloaded personas; its OFFCAM persona becomes the DLC radio persona.
static bool voice_persona_game_preload_dlc()
{
    for (int i = 0; i < Voice_personas.size(); i++)
    {
        human_voice_persona& p = Voice_personas[i];
        if ((p.flags.preload || p.flags.always_loaded) && p.get_dlc())
        {
            p.flags.background_requested = 0;
            if (stream2_get_status(p.get_srid()) != ST_ERROR && (stream2_get_ref_count(p.get_srid()) > 0 || stream2_preload(p.get_srid())) && strstr(p.get_name(), "OFFCAM") != nullptr)
            {
                DLC_radio_message_persona_handle = (human_voice_persona_handle)i;
            }
        }
    }
    return true;
}

// ---- human_voice_persona

// 0x00A58430 (:2133): the persona index in the top 12 bits, the lipsync offset in the low 20.
lipsync_data_handle human_voice_persona::get_variation_lipsync(const human_voice_line_variation* varp) const
{
    const unsigned int size = voice_line_data->lipsync_size;
    if (size == 0)
    {
        return INVALID_LIPSYNC_DATA_HANDLE;
    }
    const unsigned short offset = varp->lipsync_data_offset;
    if (offset == 0xFFFF || offset >= size)
    {
        return INVALID_LIPSYNC_DATA_HANDLE;
    }
    return (lipsync_data_handle)((persona_index(this) << 20) | offset);
}

// 0x00A31A30 (:2150)
human_voice_line_variation* human_voice_persona::get_line_variation(const human_voice_persona_line* linep, int which) const
{
    return &voice_line_data->variations[linep->variation_start_offset + which];
}

// 0x00A58490 (:2162)
human_voice_persona_line* human_voice_persona::find_voice_line(const char* line_name) const
{
    return find_voice_line(voice_line_handle_from_name(line_name));
}

// 0x00A31A50 (:2167)
human_voice_persona_line* human_voice_persona::find_voice_line(voice_lines line) const
{
    if ((unsigned int)line < NUM_VOICE_LINES && voice_line_data != nullptr && situational_lines != nullptr && situational_lines->entries[line] != 0xFFFF)
    {
        return &voice_line_data->lines[situational_lines->entries[line]];
    }
    return nullptr;
}

// 0x00A31A90 (:2183)
human_voice_persona_line* human_voice_persona::find_voice_line(voice_line_handle line_handle) const
{
    if (line_handle == INVALID_VOICE_LINE_HANDLE || voice_line_data == nullptr)
    {
        return nullptr;
    }
    for (unsigned int i = 0; i < voice_line_data->num_lines; i++)
    {
        if (voice_line_data->lines[i].get_voice_line_handle() == line_handle)
        {
            return &voice_line_data->lines[i];
        }
    }
    return nullptr;
}

// 0x00A31AF0 (:2215)
bool human_voice_persona::can_play_line_internal(const human_voice_persona_line* linep, int ms_in_future) const
{
    human_voice_persona* self = const_cast<human_voice_persona*>(this);
    if (linep->num_variations == 0)
    {
        self->last_operation_status = PLAY_VOICE_FAILURE_MISSING_VARIATIONS;
        return false;
    }
    if (linep->cooldown_timer().time_until() > ms_in_future)
    {
        self->last_operation_status = PLAY_VOICE_FAILURE_LOCAL_COOLDOWN;
        return false;
    }
    const human_voice_line_info* info = human_voice_line_get_info(linep->get_voice_line_handle());
    if (info->group_descriptors[group].global_cooldown_timer.time_until() > ms_in_future)
    {
        self->last_operation_status = PLAY_VOICE_FAILURE_GLOBAL_COOLDOWN;
        return false;
    }
    self->last_operation_status = PLAY_VOICE_SUCCESS;
    return true;
}

// 0x00A31B80 (:2257): from the line's next variation, the first with a cue (the next variation moves on).
const human_voice_line_variation* human_voice_persona::get_next_valid_variation(human_voice_persona_line* linep) const
{
    if (linep->num_variations == 0)
    {
        return nullptr;
    }
    const unsigned char start = linep->next_variation;
    unsigned char next;
    do
    {
        const human_voice_line_variation* varp = &voice_line_data->variations[linep->variation_start_offset + linep->next_variation];
        next = (unsigned char)((linep->next_variation + 1) % linep->num_variations);
        linep->next_variation = next;
        if (varp->filename_or_cue_id != -1)
        {
            return varp;
        }
    } while (next != start);
    const_cast<human_voice_persona*>(this)->last_operation_status = PLAY_VOICE_FAILURE_NO_VALID_CUES;
    return nullptr;
}

// 0x00A31C00 (:2298)
const human_voice_line_variation* human_voice_persona::get_next_variation(human_voice_persona_line* linep) const
{
    if (linep->num_variations == 0)
    {
        return nullptr;
    }
    const unsigned char current = linep->next_variation;
    linep->next_variation = (unsigned char)((current + 1) % linep->num_variations);
    return &voice_line_data->variations[linep->variation_start_offset + current];
}

// The persona lines whose subtitles name another speaker (CRCs read from rfg.exe, persona_play_voice_line_internal).
static const unsigned int SAFE_SUBTITLED_LINES[9] = {0xC9AD9E8C, 0xF464AFC1, 0x972C4210, 0xA5AB82B3, 0xF6FD93C9, 0x520FB0A8, 0x8DA45260, 0x91FFFF25, 0xC18D180F};
static const unsigned int OFFCAM_DRIVER_LINES[11] = {0x37732D83, 0xECA2395E, 0x622D3EBD, 0xAE873E23, 0xA442373A, 0x68E837A4, 0xE6673047, 0x2ACD30D9, 0xF3ED2275, 0x3F4722EB, 0xD89CFC53};
static const unsigned int OFFCAM_JORGE_LINES[3] = {0x3838DD9D, 0x657CF1B9, 0x7D0F8060};
static const unsigned int OFFCAM_RFC_LINES[16] = {0x8912B863, 0x101BE9D9, 0x8A166F0F, 0x131F3EB5, 0xC9AFA10F, 0x50A6F0B5, 0x43A0A758, 0xDAA9F6E2, 0xC12824D1, 0x5821756B, 0x65CE41AB, 0xFCC71011, 0x8BC02087, 0x15A4B524, 0xE093FB84, 0x55E7E407};
static const unsigned int OFFCAM_TECH_LINES[3] = {0xFA757AA8, 0x8E71DC2D, 0x09E8D5D1};
static const unsigned int OFFCAM_VASHA_LINE = 0x0BC66829;

static bool crc_in_list(unsigned int crc, const unsigned int* list, int count)
{
    for (int i = 0; i < count; i++)
    {
        if (crc == list[i])
        {
            return true;
        }
    }
    return false;
}

// How persona_play_voice_line_internal plays a line (the cases of its switch).
enum voice_play_mode
{
    VPM_CUE = 0, // audiolib_cue_play
    VPM_SUBTITLE = 1, // the subtitle line, then the cue
    VPM_PA = 2, // public address
    VPM_PA_SUBTITLED = 3, // public address with a subtitled radio message
    VPM_EDF_PROJECTED = 4, // the EDF projected start cue, then the line after it
    VPM_RADIO = 5, // a HUD radio message
    VPM_RADIO_SUBTITLED = 6, // a HUD radio message shown as a subtitle
};

// 0x00A584B0 (:2353): plays a variation of the line (the next valid one, or override_variation) and starts the line's
// cooldowns. False only when the line cannot play (cooldown, no variation, disabled, no cue); an audio_lib refusal by
// the rules (limits, distance, chance) still counts as played.
bool human_voice_persona::persona_play_voice_line_internal(human_voice_persona_line* linep, const vector& pos, audiolib_cue_priority cue_priority, int& play_id, unsigned short& radio_id, lipsync_data_handle* lipsync_handle, int override_variation, bool delayed, int delay_ms, bool play_intro, bool play_outro, human* hp) const
{
    human_voice_persona* self = const_cast<human_voice_persona*>(this);
    self->last_operation_status = PLAY_VOICE_SUCCESS;
    if (override_variation < 0 && !can_play_line_internal(linep, 0))
    {
        return false;
    }
    human_voice_line_info* info = human_voice_line_get_info(linep->get_voice_line_handle());
    human_voice_line_group_info& desc = info->group_descriptors[group];
    const human_voice_line_variation* varp;
    if (override_variation < 0)
    {
        varp = get_next_valid_variation(linep);
        if (varp == nullptr)
        {
            varp = get_next_variation(linep);
            if (varp == nullptr)
            {
                self->last_operation_status = PLAY_VOICE_FAILURE_NO_AVAILABLE_VARIATIONS;
                return false;
            }
        }
    }
    else
    {
        if (override_variation >= linep->num_variations)
        {
            return false;
        }
        varp = &voice_line_data->variations[linep->variation_start_offset + override_variation];
    }
    // The DLC disables single variations of the base game's lines.
    if (voice_line_dlc_disable_data != nullptr)
    {
        const line_variation_to_disable* entries = &voice_line_dlc_disable_data->disable_data;
        for (unsigned int i = 0; i < voice_line_dlc_disable_data->num_entries; i++)
        {
            if (&voice_line_data->lines[entries[i].line] == linep && &voice_line_data->variations[linep->variation_start_offset + entries[i].variation] == varp)
            {
                self->last_operation_status = PLAY_VOICE_FAILURE_MISSING_VARIATIONS;
                return false;
            }
        }
    }
    if (lipsync_handle != nullptr)
    {
        *lipsync_handle = get_variation_lipsync(varp);
    }
    // The play mode from the line's flags.
    bool play_2d = desc.play_2d();
    bool player_transmit = false;
    int mode = VPM_CUE;
    if (desc.flags.pa_projected)
    {
        mode = VPM_PA;
        play_2d = false;
        if (desc.flags.subtitled)
        {
            mode = VPM_PA_SUBTITLED;
        }
    }
    else if (desc.flags.edf_projected)
    {
        mode = VPM_EDF_PROJECTED;
    }
    else
    {
        bool check_subtitles = true;
        if (desc.flags.gur_pc_transmit)
        {
            player_transmit = true;
            play_2d = true;
        }
        if (desc.flags.gur_radio_far || desc.flags.gur_radio_local || desc.flags.gur_pc_transmit)
        {
            mode = VPM_RADIO;
            play_2d = true;
        }
        if (desc.flags.clear_or_radio)
        {
            play_2d = true;
            if (player_transmit)
            {
                mode = VPM_RADIO_SUBTITLED;
            }
            else if (human_voice_clear_or_radio(pos))
            {
                mode = VPM_RADIO;
                check_subtitles = false;
            }
            else
            {
                mode = VPM_RADIO_SUBTITLED;
                play_2d = false;
            }
        }
        else if (mode == VPM_RADIO)
        {
            check_subtitles = false;
        }
        if (check_subtitles && desc.flags.subtitled)
        {
            mode = VPM_RADIO_SUBTITLED;
        }
    }
    if (strstr(name, "BRIEF") != nullptr)
    {
        mode = VPM_CUE;
    }
    if (strstr(name, "SAFE") != nullptr && crc_in_list(info->name_crc.get_crc(), SAFE_SUBTITLED_LINES, 9))
    {
        mode = Subtitles_enabled ? VPM_SUBTITLE : VPM_CUE;
    }
    if ((unsigned char)cue_priority == AUDIOLIB_CUE_PRIORITY_DIALOGUE_BRIEFING)
    {
        mode = VPM_SUBTITLE;
    }
    if (hud_ui_get_loaded_module() == HUD_MODULE_SATELLITE)
    {
        mode = VPM_SUBTITLE;
    }
    unsigned short radio = 0;
    audiolib_params params;
    int cue_play_id = -1;
    audiolib_cue_info cue_info;
    audiolib_result result = audiolib_cue_get_info(varp->filename_or_cue_id, &cue_info);
    if ((int)result < 0)
    {
        self->last_operation_status = varp->filename_or_cue_id == -1 ? PLAY_VOICE_FAILURE_AUDIOLIB : PLAY_VOICE_FAILURE_NO_VALID_CUES;
        return false;
    }
    // Magic number: the subtitle and radio message stay up 500 ms past the line.
    const int show_ms = cue_info.play_time[0] + 500;
    const float show_time = (float)((double)(float)show_ms / 1000.0);
    params.priority = (unsigned char)cue_priority;
    if (!cue_info.always_2d && !play_2d && pos.x != 0.0f)
    {
        params.pos = pos;
        params.flags = 1;
    }
    else
    {
        params.flags = 0x30;
    }
    if (delayed)
    {
        params.flags |= 2;
    }
    if (delay_ms != 0)
    {
        params.delay_ms = delay_ms;
    }
    if (mode == VPM_RADIO_SUBTITLED && !gameseq_state_is_active(GS_GAMEPLAY))
    {
        mode = VPM_CUE;
    }
    // The radio name of OFFCAM's lines: the speaker it stands in for.
    const char* speaker = name;
    if (strstr(name, "OFFCAM") != nullptr)
    {
        const unsigned int crc = info->name_crc.get_crc();
        if (crc_in_list(crc, OFFCAM_DRIVER_LINES, 11))
        {
            speaker = "DRIVER";
        }
        else if (crc_in_list(crc, OFFCAM_JORGE_LINES, 3))
        {
            speaker = "JORGE";
        }
        else if (crc_in_list(crc, OFFCAM_RFC_LINES, 16))
        {
            speaker = "RFC";
        }
        else if (crc_in_list(crc, OFFCAM_TECH_LINES, 3))
        {
            speaker = "TECH";
        }
        else if (crc == OFFCAM_VASHA_LINE)
        {
            speaker = "VASHA";
        }
    }
    // Original: the first DLC subtitles its marauders' lines by the guerrilla group's flag.
    if (mode == VPM_CUE && dlc_get_current_id() == 1 && group == HVPG_MARAUDER && info->group_descriptors[HVPG_GUERILLA].flags.subtitled)
    {
        mode = VPM_SUBTITLE;
    }
    switch (mode)
    {
    case VPM_SUBTITLE:
        ui_add_subtitle(varp->subtitle_crc, speaker, show_ms);
        cue_play_id = audiolib_cue_play(varp->filename_or_cue_id, &params, &result);
        break;
    case VPM_PA_SUBTITLED:
        radio = hud_ui_add_radio_message(varp->subtitle_crc, show_time, -1, params.priority, false, true, speaker, result, cue_play_id, nullptr, play_intro, play_outro, nullptr);
        // fall through
    case VPM_PA:
        cue_play_id = public_address_play(varp->filename_or_cue_id, INVALID_PA_HANDLE);
        result = AUDIOLIB_ERR_NONE;
        break;
    case VPM_EDF_PROJECTED:
    {
        audiolib_cue_info start_info;
        if ((int)audiolib_cue_get_info(Ga_cue_ids.edf_projected_start, &start_info) >= 0)
        {
            const int start_id = audiolib_cue_play_3d(Ga_cue_ids.edf_projected_start, &pos, nullptr, nullptr, nullptr);
            if (audiolib_play_get_state(start_id, nullptr) == AUDIOLIB_ERR_NONE)
            {
                params.delay_ms = start_info.play_time[0];
                cue_play_id = audiolib_cue_play(varp->filename_or_cue_id, &params, &result);
            }
        }
        break;
    }
    case VPM_RADIO:
        radio = hud_ui_add_radio_message(varp->subtitle_crc, 0.0f, varp->filename_or_cue_id, params.priority, player_transmit, false, speaker, result, cue_play_id, nullptr, play_intro, play_outro, nullptr);
        break;
    case VPM_RADIO_SUBTITLED:
        if (!cue_info.always_2d && !play_2d && pos.x != 0.0f)
        {
            radio = hud_ui_add_radio_message(varp->subtitle_crc, 0.0f, varp->filename_or_cue_id, params.priority, player_transmit, true, speaker, result, cue_play_id, &pos, play_intro, play_outro, hp);
        }
        else
        {
            radio = hud_ui_add_radio_message(varp->subtitle_crc, 0.0f, varp->filename_or_cue_id, params.priority, player_transmit, true, speaker, result, cue_play_id, nullptr, play_intro, play_outro, nullptr);
        }
        break;
    default:
        cue_play_id = audiolib_cue_play(varp->filename_or_cue_id, &params, &result);
        break;
    }
    linep->cooldown_timer().set(desc.local_cooldown);
    desc.global_cooldown_timer.set(desc.global_cooldown);
    switch (result)
    {
    case AUDIOLIB_ERR_CATEGORY_VOLUME_TOO_LOW:
    case AUDIOLIB_ERR_EXCEED_CATEGORY_LIMIT:
    case AUDIOLIB_ERR_EXCEED_FOLEY_LIMIT:
    case AUDIOLIB_ERR_DONT_PLAY_PERCENTAGE:
    case AUDIOLIB_ERR_DIST_TOO_FAR:
    case AUDIOLIB_ERR_NONE:
        break;
    default:
        self->last_operation_status = PLAY_VOICE_FAILURE_AUDIOLIB;
        break;
    }
    play_id = cue_play_id;
    radio_id = radio;
    return true;
}

// 0x00A0D870 (:2827)
void human_voice_persona::set_to_be_loaded_flag()
{
    flags.to_be_loaded = 1;
    flags.to_be_unloaded = 0;
}

// 0x00A0D880 (:2839)
void human_voice_persona::set_to_be_unloaded_flag()
{
    flags.to_be_loaded = 0;
    flags.to_be_unloaded = 1;
}

// 0x00A0D890 (:2857)
bool human_voice_persona::request_stream(bool immediate)
{
    flags.background_requested = 0;
    if (stream2_get_status(srid) == ST_ERROR)
    {
        return false;
    }
    if (stream2_get_ref_count(srid) > 0)
    {
        return true;
    }
    if (immediate)
    {
        return stream2_preload(srid);
    }
    return stream2_request(srid);
}

// 0x00A0D8F0 (:2914)
bool human_voice_persona::request_destream(bool immediate)
{
    const stream2_status_val status = stream2_get_status(srid);
    flags.background_requested = 0;
    flags.to_be_loaded = 0;
    if (status == ST_ERROR)
    {
        return false;
    }
    if (status == ST_LOADING || status == ST_LOADED)
    {
        stream2_release(srid);
    }
    return true;
}

// The persona's line data loaded: its DLC disable data and the lookup of its situational lines (shared by stream and
// human_voice_persona_stream_load, inlined in both).
static bool persona_attach_line_data(human_voice_persona_data_header* data, const char* resource_filename, human_voice_line_disable_data*& disable_data, voice_lines_lookup_table*& situational_lines)
{
    // Original: the disable data is keyed by the persona's name (human_voice_line_disable_init) but looked up by its
    // resource file name, which differs once localized.
    Voice_line_disable_hash.get(crc_stri(resource_filename, 0), disable_data);
    if (situational_lines == nullptr)
    {
        situational_lines = Situational_lines_allocator.alloc(0);
        if (situational_lines == nullptr)
        {
            return false;
        }
    }
    memset(situational_lines, 0xFF, sizeof(voice_lines_lookup_table));
    for (int line = 0; line < NUM_VOICE_LINES; line++)
    {
        const voice_line_handle handle = voice_line_handle_from_line((voice_lines)line);
        if (handle == INVALID_VOICE_LINE_HANDLE)
        {
            continue;
        }
        for (unsigned int i = 0; i < data->num_lines; i++)
        {
            if (data->lines[i].get_voice_line_handle() == handle)
            {
                situational_lines->entries[line] = (unsigned short)i;
                break;
            }
        }
    }
    return true;
}

// 0x00A83180 (:2946)
bool human_voice_persona::stream(bool load)
{
    if (!load)
    {
        if (situational_lines != nullptr)
        {
            Situational_lines_allocator.free(situational_lines);
        }
        voice_line_data = nullptr;
        voice_line_dlc_disable_data = nullptr;
        situational_lines = nullptr;
        return true;
    }
    Voice_persona_data_hash.get(resource_filename, srid, voice_line_data);
    if (voice_line_data == nullptr)
    {
        return false;
    }
    return persona_attach_line_data(voice_line_data, resource_filename, voice_line_dlc_disable_data, situational_lines);
}

// 0x00A0D930 (:3011)
bool human_voice_persona::has_been_requested() const
{
    const stream2_status_val status = stream2_get_status(srid);
    return status == ST_LOADING || status == ST_LOADED;
}

// 0x00A0D950 (:3016)
bool human_voice_persona::stream_failed() const
{
    return stream2_get_status(srid) == ST_ERROR;
}

// 0x00A0D970 (:3030)
bool human_voice_persona::has_line(voice_lines line) const
{
    if ((unsigned int)line < NUM_VOICE_LINES && situational_lines != nullptr)
    {
        return situational_lines->entries[line] != 0xFFFF;
    }
    return false;
}

// 0x00A58EB0 (:3044)
bool human_voice_persona::has_line(const char* line_name) const
{
    return find_voice_line(voice_line_handle_from_name(line_name)) != nullptr;
}

// 0x00A31C40 (:3049)
bool human_voice_persona::has_line(voice_line_handle line_handle) const
{
    return find_voice_line(line_handle) != nullptr;
}

// 0x00A31C60 (:3055): loaded, requested, or a slot is free.
bool human_voice_persona::can_load() const
{
    if (is_loading() || flags.background_requested)
    {
        return true;
    }
    return count_loaded_personas() < MAX_STREAMED_PERSONAS;
}

// 0x00A31CA0 (:3081): requested now when a slot is free, else at the next slot reallocation.
void human_voice_persona::background_load_request()
{
    if (is_loading())
    {
        return;
    }
    if (count_loaded_personas() >= MAX_STREAMED_PERSONAS)
    {
        flags.background_requested = 1;
        Reallocate_voice_slots = true;
        return;
    }
    flags.background_requested = 0;
    if (stream2_get_status(srid) != ST_ERROR && stream2_get_ref_count(srid) < 1)
    {
        stream2_request(srid);
    }
}

// 0x00A58EE0 (:3113)
const unsigned char* human_voice_persona::get_lipsync_data(lipsync_data_handle lipsync_handle) const
{
    if (lipsync_handle == INVALID_LIPSYNC_DATA_HANDLE)
    {
        return nullptr;
    }
    const stream2_status_val status = stream2_get_status(srid);
    if (status != ST_ERROR && status != ST_LOADED)
    {
        return nullptr;
    }
    const unsigned int size = voice_line_data->lipsync_size;
    if (size == 0)
    {
        return nullptr;
    }
    const unsigned int index = (unsigned int)persona_index(this);
    if (((unsigned int)lipsync_handle >> 20) == index && ((unsigned int)lipsync_handle & 0xFFFFF) < size)
    {
        return &voice_line_data->lipsync_data[(unsigned int)lipsync_handle & 0xFFFFF];
    }
    return nullptr;
}

// 0x00A31D10 (:3143): the subtitle CRC of the line's first variation.
bool human_voice_persona::get_line_crc(voice_line_handle line_handle, unsigned int& crc)
{
    const human_voice_persona_line* linep = find_voice_line(line_handle);
    if (linep != nullptr && linep->num_variations != 0)
    {
        crc = voice_line_data->variations[linep->variation_start_offset].subtitle_crc;
        return true;
    }
    return false;
}

// 0x00A31D50 (:3167)
bool human_voice_persona::can_play_line(voice_lines line, int ms_in_future, play_voice_status* last_status) const
{
    bool result = false;
    if ((unsigned int)line < NUM_VOICE_LINES)
    {
        const human_voice_persona_line* linep = find_voice_line(line);
        if (linep == nullptr)
        {
            const_cast<human_voice_persona*>(this)->last_operation_status = PLAY_VOICE_FAILURE_MISSING;
        }
        else
        {
            result = can_play_line_internal(linep, ms_in_future);
        }
    }
    if (last_status != nullptr)
    {
        *last_status = last_operation_status;
    }
    return result;
}

// 0x00A58F70 (:3183)
bool human_voice_persona::can_play_line(const char* line_name, int ms_in_future, play_voice_status* last_status) const
{
    bool result = false;
    if (line_name != nullptr)
    {
        const human_voice_persona_line* linep = find_voice_line(voice_line_handle_from_name(line_name));
        if (linep == nullptr)
        {
            const_cast<human_voice_persona*>(this)->last_operation_status = PLAY_VOICE_FAILURE_MISSING;
        }
        else
        {
            result = can_play_line_internal(linep, ms_in_future);
        }
    }
    if (last_status != nullptr)
    {
        *last_status = last_operation_status;
    }
    return result;
}

// 0x00A31DC0 (:3199)
bool human_voice_persona::can_play_line(voice_line_handle line_handle, int ms_in_future, play_voice_status* last_status) const
{
    bool result = false;
    const human_voice_persona_line* linep = find_voice_line(line_handle);
    if (linep == nullptr)
    {
        const_cast<human_voice_persona*>(this)->last_operation_status = PLAY_VOICE_FAILURE_MISSING;
    }
    else
    {
        result = can_play_line_internal(linep, ms_in_future);
    }
    if (last_status != nullptr)
    {
        *last_status = last_operation_status;
    }
    return result;
}

// 0x00A58FC0 (:3218)
void human_voice_persona::reset_line_cooldown(voice_lines line) const
{
    set_line_cooldown_internal(find_voice_line(line), 0);
}

// 0x00A59040 (:3224)
void human_voice_persona::reset_line_cooldown(voice_line_handle line_handle) const
{
    set_line_cooldown_internal(find_voice_line(line_handle), 0);
}

// 0x00A31E00 (:3235): the line's local cooldown and its group's global one.
void human_voice_persona::set_line_cooldown_internal(human_voice_persona_line* line_ptr, int cooldown_ms) const
{
    if (line_ptr == nullptr)
    {
        return;
    }
    line_ptr->cooldown_timer().set(cooldown_ms);
    human_voice_line_get_info(line_ptr->get_voice_line_handle())->group_descriptors[group].global_cooldown_timer.set(cooldown_ms);
}

// 0x00A31E50 (:3249)
void human_voice_persona::set_line_cooldown(voice_lines line, int cooldown_ms) const
{
    set_line_cooldown_internal(find_voice_line(line), cooldown_ms);
}

// 0x00A31ED0 (:3254)
void human_voice_persona::set_line_cooldown(voice_line_handle line_handle, int cooldown_ms) const
{
    set_line_cooldown_internal(find_voice_line(line_handle), cooldown_ms);
}

// 0x00A59090 (:3270)
bool human_voice_persona::persona_play_voice_line(voice_lines line, const vector& pos, audiolib_cue_priority cue_priority, int& play_id, unsigned short& radio_id, lipsync_data_handle* lipsync_handle, bool delayed, int delay_ms) const
{
    play_id = -1;
    radio_id = 0;
    if (lipsync_handle != nullptr)
    {
        *lipsync_handle = INVALID_LIPSYNC_DATA_HANDLE;
    }
    if ((unsigned int)line >= NUM_VOICE_LINES)
    {
        return false;
    }
    human_voice_persona_line* linep = find_voice_line(line);
    if (linep == nullptr)
    {
        const_cast<human_voice_persona*>(this)->last_operation_status = PLAY_VOICE_FAILURE_MISSING;
        return false;
    }
    return persona_play_voice_line_internal(linep, pos, cue_priority, play_id, radio_id, lipsync_handle, -1, delayed, delay_ms, true, true, nullptr);
}

// 0x00A59130 (:3301)
bool human_voice_persona::persona_play_voice_line(const char* line_name, const vector& pos, audiolib_cue_priority cue_priority, int& play_id, unsigned short& radio_id, lipsync_data_handle* lipsync_handle, bool delayed) const
{
    play_id = -1;
    radio_id = 0;
    if (lipsync_handle != nullptr)
    {
        *lipsync_handle = INVALID_LIPSYNC_DATA_HANDLE;
    }
    human_voice_persona_line* linep = find_voice_line(voice_line_handle_from_name(line_name));
    if (linep == nullptr)
    {
        const_cast<human_voice_persona*>(this)->last_operation_status = PLAY_VOICE_FAILURE_MISSING;
        return false;
    }
    return persona_play_voice_line_internal(linep, pos, cue_priority, play_id, radio_id, lipsync_handle, -1, delayed, 0, true, true, nullptr);
}

// 0x00A591B0 (:3332)
bool human_voice_persona::persona_play_voice_line(voice_line_handle line_handle, const vector& pos, audiolib_cue_priority cue_priority, int& play_id, unsigned short& radio_id, lipsync_data_handle* lipsync_handle, bool delayed, bool unused, bool play_intro, bool play_outro, human* hp) const
{
    play_id = -1;
    radio_id = 0;
    if (lipsync_handle != nullptr)
    {
        *lipsync_handle = INVALID_LIPSYNC_DATA_HANDLE;
    }
    human_voice_persona_line* linep = find_voice_line(line_handle);
    if (linep == nullptr)
    {
        const_cast<human_voice_persona*>(this)->last_operation_status = PLAY_VOICE_FAILURE_MISSING;
        return false;
    }
    return persona_play_voice_line_internal(linep, pos, cue_priority, play_id, radio_id, lipsync_handle, -1, delayed, 0, play_intro, play_outro, hp);
}

// 0x00A0D9A0 (:3381): in a single player DLC bundle only the DLC's always loaded personas count.
bool human_voice_persona::is_always_loaded() const
{
    if (World.dlc_bundle_id != -1 && dlc_bundle_contains_sp(dlc_bundle_id_to_index(World.dlc_bundle_id)))
    {
        return flags.always_loaded && dlc;
    }
    return flags.always_loaded != 0;
}

// 0x00A0D9E0 (:3395)
bool human_voice_persona::should_preload() const
{
    return flags.preload || flags.always_loaded;
}

// 0x00A0D9F0 (:3403): a failed stream counts as loaded.
bool human_voice_persona::is_loaded() const
{
    const stream2_status_val status = stream2_get_status(srid);
    return status == ST_ERROR || status == ST_LOADED;
}

// 0x00A0DA10 (:3422): loaded, loading or failed.
bool human_voice_persona::is_loading() const
{
    const stream2_status_val status = stream2_get_status(srid);
    return status == ST_ERROR || status == ST_LOADED || status == ST_LOADING;
}

// ---- external functions

// 0x00A58130 (:1818)
void voice_slotting_do_frame()
{
    if (!Multiplayer)
    {
        if (Reallocate_voice_slots)
        {
            do_reallocate_voice_slots();
            Reallocate_voice_slots = false;
        }
        else
        {
            do_voice_clean_up();
        }
    }
    if (Persona_stream_do_frame)
    {
        Persona_stream_do_frame = false;
        do_persona_streaming();
    }
}

// 0x00A58170 (:1875). Original: the later flags are read only when the earlier ones are present (every entry of the
// retail table has them all).
void parse_personas_flags_table(const char* filename, bool dlc_only)
{
    if (!cf_exist(filename))
    {
        return;
    }
    xml_element* root = xml_table_parse(filename);
    if (root == nullptr)
    {
        return;
    }
    for (xml_element* entry = xml_find(root, XML_PERSONA_FLAG_ROOT_TAG); entry != nullptr; entry = xml_find_next(root, entry, XML_PERSONA_FLAG_ROOT_TAG))
    {
        xml_element* name = xml_find(entry, XML_PERSONA_FLAG_NAME_TAG);
        if (name == nullptr)
        {
            continue;
        }
        human_voice_persona* persona = human_voice_persona_from_name(name->text);
        if (persona == nullptr || (dlc_only && !persona->get_dlc()))
        {
            continue;
        }
        xml_element* value = xml_find(entry, XML_PERSONA_FLAG_PRELOAD_TAG);
        if (value == nullptr)
        {
            continue;
        }
        persona->flags.preload = xml_get_bool(value, false) ? 1 : 0;
        value = xml_find(entry, XML_PERSONA_FLAG_PLAYER_TAG);
        if (value == nullptr)
        {
            continue;
        }
        persona->flags.player_persona = xml_get_bool(value, false) ? 1 : 0;
        value = xml_find(entry, XML_PERSONA_FLAG_SPECIAL_TAG);
        if (value == nullptr)
        {
            continue;
        }
        set_special_persona_flags(value, persona);
        value = xml_find(entry, XML_PERSONA_FLAG_MP_TAG);
        if (value != nullptr)
        {
            persona->flags.multiplayer = xml_get_bool(value, false) ? 1 : 0;
        }
    }
    xml_table_close(root);
}

// 0x00A83020 (:1931): each persona's .vpd_pc (the DLC's disabled variations), read whole into pool.
void human_voice_line_disable_init(mempool* pool)
{
    Voice_line_disable_hash.clear();
    for (int i = 0; i < Voice_personas.size(); i++)
    {
        char filename[64];
        cf_add_ext(filename, 64, Voice_personas[i].get_name(), ".vpd_pc", false);
        if (!cf_exist(filename))
        {
            continue;
        }
        cfile* fp = cf_open(filename, "rb", PLATFORM_PC, false);
        const unsigned int size = cf_size(fp);
        human_voice_line_disable_data* data = (human_voice_line_disable_data*)pool->alloc(size, 0);
        if (data == nullptr)
        {
            cf_close(fp);
            return;
        }
        cf_read(data, size, fp, false);
        cf_close(fp);
        Voice_line_disable_hash.insert_key_data(crc_stri(Voice_personas[i].get_name(), 0), data);
    }
}

// 0x00A319A0 (:1967)
void human_voice_line_disable_shutdown()
{
    Voice_line_disable_hash.clear();
}

// 0x00A582C0 (:1973): the GSP_VOICE_PERSONA primitive: offsets fixed up, lines and variations initialized, and the
// data registered under the file name (without extension) for the container callback.
bool human_voice_load_voice_persona_cb(const char* name, unsigned int srid, unsigned char* cpu_data, int cpu_data_size, unsigned char* gpu_data, int gpu_data_size)
{
    if (cpu_data == nullptr)
    {
        return false;
    }
    human_voice_persona_data_header* header = (human_voice_persona_data_header*)cpu_data;
    human_voice_persona_offsets_to_pointers(*header);
    if (!header->game_init())
    {
        return false;
    }
    char base_name[64];
    cf_drop_ext(base_name, 64, name);
    Voice_persona_data_hash.insert(base_name, srid, header);
    return true;
}

// 0x00A583C0 (:1997)
void human_voice_unload_voice_persona_cb(const char* name, unsigned int srid)
{
    char base_name[64];
    cf_drop_ext(base_name, 64, name);
    Voice_persona_data_hash.remove(base_name, srid);
}

// 0x00A0D7F0 (:2315): the speaker is farther from the listener than the crossover distance.
bool human_voice_clear_or_radio(const vector& pos)
{
    audiolib_stats stats;
    audiolib_stats_get(&stats);
    const vector listener(stats.pos_x, stats.pos_y, stats.pos_z);
    return vec_dist(listener, pos) > gameaudio_clr_radio_crossover_distance;
}

// 0x00A320B0 (:3534): every persona is released, the always loaded pool cleared, then the set of the game type is
// preloaded.
bool human_voice_persona_do_game_preload(bool multi, bool dlc)
{
    for (int i = 0; i < Voice_personas.size(); i++)
    {
        human_voice_persona& p = Voice_personas[i];
        const stream2_status_val status = stream2_get_status(p.get_srid());
        p.flags.background_requested = 0;
        p.flags.to_be_loaded = 0;
        if (status != ST_ERROR && (status == ST_LOADING || status == ST_LOADED))
        {
            stream2_release(p.get_srid());
        }
    }
    Human_voice_always_loaded_pool.clear();
    if (dlc)
    {
        return voice_persona_game_preload_dlc();
    }
    if (multi)
    {
        return voice_persona_game_preload_multi();
    }
    return voice_persona_game_preload_default();
}

// 0x00A59230 (:3573): in multiplayer the non-multiplayer personas are released and each character class's voice list
// requests one of its voices (pick_voice_generic: one that can load, now). Original: the list is copied again when the
// copy is empty, which it cannot be.
void human_voice_persona_level_init_pre(bool multiplayer)
{
    if (!multiplayer)
    {
        return;
    }
    for (int i = 0; i < Voice_personas.size(); i++)
    {
        human_voice_persona& p = Voice_personas[i];
        if (p.flags.multiplayer)
        {
            continue;
        }
        const stream2_status_val status = stream2_get_status(p.get_srid());
        p.flags.background_requested = 0;
        p.flags.to_be_loaded = 0;
        if (status != ST_ERROR && (status == ST_LOADING || status == ST_LOADED))
        {
            stream2_release(p.get_srid());
        }
    }
    int num_lists = 0;
    const voice_persona_list* lists[8];
    multi_game_character_get_base_voice_persona_lists(num_lists, lists, 8);
    for (int i = 0; i < num_lists; i++)
    {
        const voice_persona_list* list = lists[i];
        if (list->voice_list.size() < 1)
        {
            continue;
        }
        farray<human_voice_persona*,64> voices;
        for (int j = 0; j < list->voice_list.size(); j++)
        {
            voices.add(list->voice_list[j]);
        }
        if (voices.size() < 1)
        {
            voices.remove_all();
            for (int j = 0; j < list->voice_list.size(); j++)
            {
                voices.add(list->voice_list[j]);
            }
        }
        list->pick_voice_generic(voices);
    }
}

// 0x00A832A0 (:3598): once: the voice lines, the persona table (localized to the audio language, as "en_us"), the
// persona flags, the default preload and the radio persona.
void human_voice_system_init()
{
    if (Human_voice_init)
    {
        return;
    }
    Human_voice_init = true;
    char lang_suffix[256];
    keen::formatString(lang_suffix, 256, "%s", keen::getLanguageIdString(localization_get_audio_language()));
    keen::changeStringToLowercase(lang_suffix);
    keen::replaceStringCharacters(lang_suffix, '-', '_');
    // rfg.exe inlines the same steps here (parse rfg_voice_lines.xtbl, the descriptors, the situational lookups,
    // close); the pool is only read for the DLC table.
    human_voice_lines_parse_table("rfg_voice_lines.xtbl", false, nullptr);
    human_voice_persona::parse_personas_table_shared("rfg_voice_personas.xtbl", &Common_mempool, &Permanent_mempool, Use_packfiles, PLATFORM_PC, false, false, lang_suffix);
    parse_personas_flags_table("rfg_voice_persona_flags.xtbl", false);
    human_voice_persona_do_game_preload(false, false);
    const human_voice_persona* radio = human_voice_persona_from_name(RADIO_MESSAGE_PERSONA_NAME);
    Radio_message_persona_handle = radio != nullptr ? human_voice_persona_get_handle(radio) : INVALID_HUMAN_VOICE_PERSONA_HANDLE;
}

// 0x00A0DA40 (:3625)
human_voice_persona_handle human_voice_radio_persona_handle()
{
    return World.dlc_bundle_id == -1 ? Radio_message_persona_handle : DLC_radio_message_persona_handle;
}

// 0x00A59350 (:3637)
human_voice_persona* human_voice_radio_persona()
{
    return human_voice_persona_from_handle(human_voice_radio_persona_handle());
}

// 0x00A59390 (:3653)
void human_voice_system_shutdown()
{
    if (!Human_voice_init)
    {
        return;
    }
    Human_voice_init = false;
    for (int i = 0; i < Voice_personas.size(); i++)
    {
        human_voice_persona& p = Voice_personas[i];
        const stream2_status_val status = stream2_get_status(p.get_srid());
        p.flags.background_requested = 0;
        p.flags.to_be_loaded = 0;
        if (status != ST_ERROR && (status == ST_LOADING || status == ST_LOADED))
        {
            stream2_release(p.get_srid());
        }
    }
    human_voice_persona::persona_shutdown();
    Voice_personas.remove_all();
}

// 0x00A0DA60 (:3672)
bool human_voice_initialized()
{
    return Human_voice_init;
}

// 0x00A59410 (:3678): no required, activity or cutscene personas, and every human stops talking.
void human_voice_level_init_post()
{
    // Magic number: the first clean-up after 30 s.
    Voice_cleanup_period.set(30000);
    Required_set_list_handle = INVALID_VOICE_SET_HANDLE;
    Required_persona_list.reset();
    Activity_voice_list.remove_all();
    Cutscene_voice_list.remove_all();
    const int num = World.get_num_objects<human>();
    for (int i = 0; i < num; i++)
    {
        World.get_object<human>(i)->stop_voice(VOICE_LINE_PRIORITY_FORCE_ALWAYS);
    }
}

// 0x00A59490 (:3695). LTCG: pool goes on to parse_voice_line_descriptors in EAX.
void dlc_voice_persona_level_init(int dlc_id, mempool* pool)
{
    char filename[256];
    sprintf(filename, "dlc%02d_%s", dlc_id, cMission_briefing_table);
    if (cf_exist(filename))
    {
        hud_ui_mission_briefing_mark();
        hud_ui_mission_briefing_init_lines(filename);
    }
    sprintf(filename, "dlc%02d_rfg_voice_lines.xtbl", dlc_id);
    if (cf_exist(filename))
    {
        human_voice_lines_parse_table(filename, true, pool);
    }
}

// 0x00A32130 (:3713)
void dlc_persona_mission_init()
{
    All_misison_persona_lists_mark = All_misison_persona_lists.size();
}

// 0x00A32140 (:3719): the DLC's mission persona lists are dropped.
void dlc_voice_persona_shutdown_on_exit()
{
    if (All_misison_persona_lists_mark == 0)
    {
        return;
    }
    while (All_misison_persona_lists_mark < All_misison_persona_lists.size())
    {
        All_misison_persona_lists.remove(All_misison_persona_lists.size() - 1);
    }
    All_misison_persona_lists_mark = 0;
}

// 0x00A321D0 (:3731)
void dlc_voice_persona_level_shutdown()
{
    hud_ui_mission_briefing_restore_to_mark();
    human_voice_lines_dlc_shutdown();
}

// 0x00A9D680 (:3739): every human's voice, then the slotting and streaming (inlined voice_slotting_do_frame).
void human_voice_do_frame()
{
    const int num = World.get_num_objects<human>();
    for (int i = 0; i < num; i++)
    {
        World.get_object<human>(i)->voice_do_frame();
    }
    voice_slotting_do_frame();
}

// The set of the list (as a sorted mission_persona_list): an existing equal set's handle, else a new one (shared by
// the two list overloads of human_voice_load_set).
static int human_voice_add_set(const mission_persona_list& set)
{
    const int count = All_misison_persona_lists.size();
    for (int i = 0; i < count; i++)
    {
        if (same_mission_persona_lists(All_misison_persona_lists[i], set))
        {
            return i;
        }
    }
    if (All_misison_persona_lists.full())
    {
        return INVALID_VOICE_SET_HANDLE;
    }
    All_misison_persona_lists.add(set);
    return count;
}

// 0x00A59550 (:3754): one to five personas (the player and always loaded ones are left out).
int human_voice_load_set(const farray<human_voice_persona*,150>& persona_list)
{
    mission_persona_list set;
    if (persona_list.size() > 5 || persona_list.size() < 1)
    {
        return INVALID_VOICE_SET_HANDLE;
    }
    for (int i = 0; i < persona_list.size(); i++)
    {
        human_voice_persona* persona = persona_list[i];
        if (!persona->flags.player_persona && !persona->is_always_loaded())
        {
            set.add(persona_list[i]);
        }
    }
    return human_voice_add_set(set);
}

// 0x00A59620 (:3798): the voice_persona_0 to _4 of a mission's voice_persona_list.
int human_voice_load_set(xml_element* xml_root)
{
    xml_element* list = xml_find(xml_root, XML_ROOT_MISSION_LIST_TAG);
    if (list == nullptr)
    {
        return INVALID_VOICE_SET_HANDLE;
    }
    mission_persona_list set;
    for (int i = 0; i < 5; i++)
    {
        xml_element* entry = xml_find(list, XML_MISSION_PERSONA_TAG_LIST[i]);
        if (entry == nullptr)
        {
            continue;
        }
        human_voice_persona* persona = human_voice_persona_from_name(entry->text);
        if (persona != nullptr)
        {
            set.add(persona);
        }
    }
    if (set.mission_personas[0] == nullptr)
    {
        return INVALID_VOICE_SET_HANDLE;
    }
    return human_voice_add_set(set);
}

// 0x00A321F0 (:3850)
bool human_voice_set_streamed(int set_handle)
{
    if (Required_set_list_handle != set_handle || set_handle < INVALID_VOICE_SET_HANDLE || set_handle >= All_misison_persona_lists.size())
    {
        return false;
    }
    for (int i = 0; i < 5; i++)
    {
        if (Required_persona_list.mission_personas[i] != nullptr && !Required_persona_list.mission_personas[i]->is_loaded())
        {
            return false;
        }
    }
    return true;
}

// 0x00A59710 (:3877): the set becomes the required personas (when none is), which are requested at once.
bool human_voice_load_set(int set_handle)
{
    if (set_handle < INVALID_VOICE_SET_HANDLE || set_handle >= All_misison_persona_lists.size() || Required_set_list_handle != INVALID_VOICE_SET_HANDLE)
    {
        return false;
    }
    Required_set_list_handle = set_handle;
    if (set_handle == INVALID_VOICE_SET_HANDLE)
    {
        Required_persona_list.reset();
        return true;
    }
    Required_persona_list = All_misison_persona_lists[set_handle];
    for (int i = 0; i < 5; i++)
    {
        if (Required_persona_list.mission_personas[i] != nullptr)
        {
            Required_persona_list.mission_personas[i]->background_load_request();
        }
    }
    do_reallocate_voice_slots();
    do_persona_streaming();
    return true;
}

// 0x00A32240 (:3914): a free slot, or the activity already holds one.
bool human_voice_activity_can_reserve(unsigned int activity_spawn_handle)
{
    if (activity_spawn_handle == 0xFFFFFFFF)
    {
        return false;
    }
    if (!Activity_voice_list.full())
    {
        return true;
    }
    for (int i = 0; i < Activity_voice_list.size(); i++)
    {
        if (Activity_voice_list[i].spawn_node_handle != 0xFFFFFFFF && Activity_voice_list[i].spawn_node_handle == activity_spawn_handle)
        {
            return true;
        }
    }
    return false;
}

// 0x00A597C0 (:3940)
bool human_voice_activity_is_reserved(const human_voice_persona* desired_voice, unsigned int activity_spawn_handle)
{
    if (activity_spawn_handle == 0xFFFFFFFF || desired_voice == nullptr || Activity_voice_list.size() < 1)
    {
        return false;
    }
    for (int i = 0; i < Activity_voice_list.size(); i++)
    {
        const unsigned int spawn = Activity_voice_list[i].spawn_node_handle;
        if (spawn == 0xFFFFFFFF || spawn != activity_spawn_handle)
        {
            continue;
        }
        const human_voice_persona_handle handle = human_voice_persona_get_handle(desired_voice);
        if (handle == INVALID_HUMAN_VOICE_PERSONA_HANDLE)
        {
            return false;
        }
        if (handle == Activity_voice_list[i].desired_persona_handle)
        {
            return true;
        }
    }
    return false;
}

// 0x00A59830 (:3974)
bool human_voice_activity_reserve_voice(const human_voice_persona* desired_voice, unsigned int activity_spawn_handle)
{
    if (activity_spawn_handle == 0xFFFFFFFF || desired_voice == nullptr || !human_voice_activity_can_reserve(activity_spawn_handle))
    {
        return false;
    }
    activity_voice_info info;
    info.desired_persona_handle = human_voice_persona_get_handle(desired_voice);
    if (info.desired_persona_handle == INVALID_HUMAN_VOICE_PERSONA_HANDLE)
    {
        return false;
    }
    info.spawn_node_handle = activity_spawn_handle;
    Activity_voice_list.add(info);
    const_cast<human_voice_persona*>(desired_voice)->background_load_request();
    return true;
}

// 0x00A32290 (:4003)
bool human_voice_activity_release_voice(unsigned int activity_spawn_handle)
{
    if (activity_spawn_handle == 0xFFFFFFFF || !human_voice_activity_can_reserve(activity_spawn_handle))
    {
        return false;
    }
    for (int i = 0; i < Activity_voice_list.size();)
    {
        if (Activity_voice_list[i].spawn_node_handle == activity_spawn_handle)
        {
            Activity_voice_list.remove_unsorted(i);
        }
        else
        {
            i++;
        }
    }
    return true;
}

// 0x00A598B0 (:4027): always loaded voices need no slot.
bool human_voice_cutscene_reserve_slot(const human_voice_persona* desired_voice)
{
    if (desired_voice == nullptr || Cutscene_voice_list.full())
    {
        return false;
    }
    if (desired_voice->is_always_loaded())
    {
        return true;
    }
    const human_voice_persona_handle handle = human_voice_persona_get_handle(desired_voice);
    if (handle == INVALID_HUMAN_VOICE_PERSONA_HANDLE)
    {
        return false;
    }
    Cutscene_voice_list.add(handle);
    const_cast<human_voice_persona*>(desired_voice)->background_load_request();
    return true;
}

// 0x00A59950 (:4054)
bool human_voice_cutscene_release_voice(const human_voice_persona* reserved_voice)
{
    if (reserved_voice == nullptr)
    {
        return false;
    }
    if (reserved_voice->is_always_loaded())
    {
        return true;
    }
    const human_voice_persona_handle handle = human_voice_persona_get_handle(reserved_voice);
    if (handle == INVALID_HUMAN_VOICE_PERSONA_HANDLE)
    {
        return false;
    }
    for (int i = 0; i < Cutscene_voice_list.size(); i++)
    {
        if (Cutscene_voice_list[i] == handle)
        {
            Cutscene_voice_list.remove_unsorted(i);
            break;
        }
    }
    return true;
}

// 0x00A322E0 (:4081)
bool human_voice_set_loaded(int set_handle)
{
    if (set_handle < INVALID_VOICE_SET_HANDLE || set_handle >= All_misison_persona_lists.size())
    {
        return false;
    }
    if (set_handle == INVALID_VOICE_SET_HANDLE)
    {
        return true;
    }
    if (Required_set_list_handle != set_handle)
    {
        return false;
    }
    for (int i = 0; i < 5; i++)
    {
        if (Required_persona_list.mission_personas[i] != nullptr && !Required_persona_list.mission_personas[i]->is_loaded())
        {
            return false;
        }
    }
    return true;
}

// 0x00A32340 (:4111)
void human_voice_unload_set(int set_handle)
{
    if (set_handle > INVALID_VOICE_SET_HANDLE && set_handle < All_misison_persona_lists.size() && set_handle == Required_set_list_handle)
    {
        Required_set_list_handle = INVALID_VOICE_SET_HANDLE;
        Required_persona_list.reset();
    }
}

// 0x00A32390 (:4129)
human_voice_persona_handle human_voice_persona_get_handle(const human_voice_persona* persona)
{
    // Magic number: 0x2C, the size of a persona (the handle is its index, computed from the address in rfg.exe).
    const int index = (int)(((const char*)persona - (const char*)&Voice_personas[0]) / 0x2C);
    if (index < 0 || index >= Voice_personas.size())
    {
        return INVALID_HUMAN_VOICE_PERSONA_HANDLE;
    }
    return (human_voice_persona_handle)index;
}

// 0x00A599F0 (:4137)
human_voice_persona_handle human_voice_persona_handle_from_name(const char* persona_name)
{
    const human_voice_persona* persona = human_voice_persona_from_name(persona_name);
    if (persona == nullptr)
    {
        return INVALID_HUMAN_VOICE_PERSONA_HANDLE;
    }
    return human_voice_persona_get_handle(persona);
}

// 0x00A323C0 (:4149)
human_voice_persona* human_voice_persona_from_handle(human_voice_persona_handle persona_handle)
{
    if ((int)persona_handle >= 0 && (int)persona_handle < Voice_personas.size())
    {
        return &Voice_personas[persona_handle];
    }
    return nullptr;
}

// 0x00A59A30 (:4170): a random persona of the team's group (no TEST persona) with the line that can play it now
// (inlined can_play_line). The search stops at a persona whose line is on cooldown or busy.
human_voice_persona* human_voice_find_persona_with_line(voice_line_handle line_handle, human_teams team, bool ignore_error)
{
    const human_voice_persona_groups group = human_voice_persona_group_from_team(team);
    human_voice_persona* candidates[50];
    int count = 0;
    for (int i = 0; i < Voice_personas.size(); i++)
    {
        // Magic number: room for 50 candidates.
        if (count > 49)
        {
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n");
            break;
        }
        human_voice_persona* p = &Voice_personas[i];
        if ((group == HVPG_ANY || p->group == group) && strstr(p->name, "TEST") == nullptr && p->find_voice_line(line_handle) != nullptr)
        {
            candidates[count++] = p;
        }
    }
    if (count < 1)
    {
        return nullptr;
    }
    const int start = int_rand_range(0, count - 1);
    for (int i = 0; i < count; i++)
    {
        human_voice_persona* p = candidates[(i + start) % count];
        const human_voice_persona_line* linep = p->find_voice_line(line_handle);
        bool can_play = false;
        if (linep == nullptr)
        {
            p->last_operation_status = PLAY_VOICE_FAILURE_MISSING;
        }
        else
        {
            can_play = p->can_play_line_internal(linep, 0);
        }
        if (can_play)
        {
            return p;
        }
        const play_voice_status status = p->last_operation_status;
        if (status >= PLAY_VOICE_FAILURE_LOCAL_COOLDOWN && (status <= PLAY_VOICE_FAILURE_PRIORITY || status == PLAY_VOICE_FAILURE_IN_VEHICLE))
        {
            break;
        }
    }
    return nullptr;
}

// 0x00A59CE0 (:4271): as above for a situational line. Original: the error report (the personas not loaded) is only
// status reads.
human_voice_persona* human_voice_find_persona_with_line(voice_lines voice_line, human_teams team, bool ignore_error)
{
    const human_voice_persona_groups group = human_voice_persona_group_from_team(team);
    human_voice_persona* candidates[50];
    int count = 0;
    for (int i = 0; i < Voice_personas.size(); i++)
    {
        if (count > 49)
        {
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n");
            break;
        }
        human_voice_persona* p = &Voice_personas[i];
        if ((group == HVPG_ANY || p->group == group) && strstr(p->name, "TEST") == nullptr && p->has_line(voice_line))
        {
            candidates[count++] = p;
        }
    }
    if (count > 0)
    {
        const int start = int_rand_range(0, count - 1);
        for (int i = 0; i < count; i++)
        {
            human_voice_persona* p = candidates[(i + start) % count];
            bool can_play = false;
            if ((unsigned int)voice_line < NUM_VOICE_LINES)
            {
                const human_voice_persona_line* linep = p->find_voice_line(voice_line);
                if (linep == nullptr)
                {
                    p->last_operation_status = PLAY_VOICE_FAILURE_MISSING;
                }
                else
                {
                    can_play = p->can_play_line_internal(linep, 0);
                }
            }
            if (can_play)
            {
                return p;
            }
            const play_voice_status status = p->last_operation_status;
            if (status >= PLAY_VOICE_FAILURE_LOCAL_COOLDOWN && (status <= PLAY_VOICE_FAILURE_PRIORITY || status == PLAY_VOICE_FAILURE_IN_VEHICLE))
            {
                return nullptr;
            }
        }
    }
    if (!ignore_error)
    {
        for (int i = 0; i < Voice_personas.size(); i++)
        {
            stream2_get_status(Voice_personas[i].srid);
        }
    }
    return nullptr;
}

// 0x00A59F30 (:4384): the loading and loaded personas; false when the list is too small.
bool human_voice_collect_active_personas(base_array<human_voice_persona*>& persona_list)
{
    persona_list.remove_all();
    for (int i = 0; i < Voice_personas.size(); i++)
    {
        const stream2_status_val status = stream2_get_status(Voice_personas[i].get_srid());
        if (status != ST_LOADING && status != ST_LOADED)
        {
            continue;
        }
        if (persona_list.full())
        {
            return false;
        }
        persona_list.add(&Voice_personas[i]);
    }
    return true;
}

// 0x00A833C0 (:4410): the GSC_VOICE_PERSONA container is loaded: its persona takes the line data (inlined stream).
void human_voice_persona_stream_load(unsigned int srid)
{
    for (int i = 0; i < Voice_personas.size(); i++)
    {
        human_voice_persona& p = Voice_personas[i];
        if (p.srid != srid)
        {
            continue;
        }
        Voice_persona_data_hash.get(p.resource_filename, p.srid, p.voice_line_data);
        if (p.voice_line_data != nullptr)
        {
            persona_attach_line_data(p.voice_line_data, p.resource_filename, p.voice_line_dlc_disable_data, p.situational_lines);
        }
        return;
    }
}

// 0x00A834D0 (:4424)
void human_voice_persona_stream_unload(unsigned int srid)
{
    for (int i = 0; i < Voice_personas.size(); i++)
    {
        human_voice_persona& p = Voice_personas[i];
        if (p.srid != srid)
        {
            continue;
        }
        if (p.situational_lines != nullptr)
        {
            Situational_lines_allocator.free(p.situational_lines);
        }
        p.voice_line_data = nullptr;
        p.voice_line_dlc_disable_data = nullptr;
        p.situational_lines = nullptr;
        return;
    }
}
