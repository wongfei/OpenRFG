// Original: volition/rfg/code/objects/humans/human_voice/human_voice_lines.cpp
//
// The voice line table (rfg_voice_lines.xtbl and the DLC table): per line and persona group its cooldowns, delays,
// play chance, priority and flags, and the lookups between handles, names, CRCs and the situational voice_lines. A
// handle is the descriptor index, with VOICE_LINE_HANDLE_IS_DLC for the DLC table. The external interface is declared
// in human_voice_lines.h. SFX.md SX6: ported whole. Ported in SX4: human_voice_persona_group_from_team (the anim voice
// triggers of vanim_check_prop_handles).
//
// rfg.exe compiles human.cpp and the human_voice files in one unity TU; the statics they share are placed in their
// first user in path order and have external linkage here (declared in human_voice_lines.h).

#include "volition/rfg/code/objects/humans/human_voice/human_voice_lines.h"

#include "keen/base/trace.hpp"
#include "volition/rfg/code/objects/humans/human_voice/human_voice_shared.h"
#include "volition/vlib/code/memory/vmemory.h"
#include "volition/rfg/code/rfg.h"
#include "volition/vlib/code/memory/rfg_split_mempool.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/vlib/code/util/fastxml.h"
#include "volition/vlib/code/util/string_util.h"

#include <new>
#include <stdio.h>
#include <string.h>

// ---- file-static globals
// voice_line_lookup_entry Voice_line_lookup_table[129] (0x01695018) is used here too; placed in human.cpp.
static int Max_voice_line_descriptors; // 0x0302F084
// int Num_voice_line_descriptors (0x0302F088) is used here too; placed in human_voice.cpp.
// human_voice_line_info* Voice_line_descriptors (0x0302F08C) is used here too; placed in human.cpp.
// int Num_dlc_voice_line_descriptors (0x0302F090) is used here too; placed in human_voice.cpp.
// human_voice_line_info* Dlc_voice_line_descriptors (0x0302F094) is used here too; placed in human_voice.cpp.

char last_voice_line_name[32]; // 0x0301A1F8

// Magic number: the size of a voice line descriptor (the handle is a byte offset divided by it in rfg.exe).
static_assert(sizeof(human_voice_line_info) == 0x38, "human_voice_line_info");

// ---- internal functions

// 0x00A24770 (:35)
template<typename T>
static void swap_voice_lines(T& a, T& b)
{
    const T t = a;
    a = b;
    b = t;
}

// <unsigned char> 0x00A24790 (:421)
// 0x00A24720 <unsigned short>, 0x00A24790 <unsigned char> (:421): a required value clamped to [min, max].
template<typename T>
static void get_xml_uint(T& val, xml_element* root, const char* tag_name, unsigned int min, unsigned int max)
{
    unsigned int value;
    xml_table_get_req_uint(value, root, tag_name);
    if (value < min)
    {
        val = (T)min;
        return;
    }
    if (value > max)
    {
        val = (T)max;
        return;
    }
    val = (T)value;
}

// 0x00A0DA90 (:441). LTCG: info in EDX.
static bool voice_line_info_is_dlc(const human_voice_line_info* info)
{
    return Dlc_voice_line_descriptors != nullptr && info >= Dlc_voice_line_descriptors && info < Dlc_voice_line_descriptors + Num_dlc_voice_line_descriptors;
}

// 0x00A0DAC0 (:460): the base table first, then the DLC one.
static human_voice_line_info* voice_line_info_from_crc(checksum_stri crc_val)
{
    for (int i = 0; i < Num_voice_line_descriptors; i++)
    {
        if (Voice_line_descriptors[i].name_crc.get_crc() == crc_val.get_crc())
        {
            return &Voice_line_descriptors[i];
        }
    }
    for (int i = 0; i < Num_dlc_voice_line_descriptors; i++)
    {
        if (Dlc_voice_line_descriptors[i].name_crc.get_crc() == crc_val.get_crc())
        {
            return &Dlc_voice_line_descriptors[i];
        }
    }
    return nullptr;
}

// 0x00A0DB30 (:484). LTCG: line_name in EAX.
static human_voice_line_info* voice_line_info_from_name(const char* line_name)
{
    return voice_line_info_from_crc(checksum_stri(line_name, 0));
}

// 0x00A323E0 (:497). LTCG: pool in EAX (read for the DLC table only). The base table gets room for 200 dummy lines
// (add_dummy_voice_line) and comes from the permanent mempool; a line already in it is skipped. Each Types Entry sets
// the descriptor of its persona group once.
static bool parse_voice_line_descriptors(xml_element* root, bool dlc, mempool* pool)
{
    int count = (int)xml_count(root, "Voice_Line");
    if (!dlc)
    {
        // Magic number: room for 200 dummy lines.
        count += 200;
        Max_voice_line_descriptors = count;
    }
    // Magic number: the handles are 16 bits (0xFFFF is none).
    if (count >= 0xFFFF)
    {
        return false;
    }
    if (dlc)
    {
        if (pool == nullptr)
        {
            return false;
        }
        human_voice_line_info* table = (human_voice_line_info*)pool->alloc(count * sizeof(human_voice_line_info), 4);
        if (table == nullptr)
        {
            keen::debug::nativePrintArgs("HUMAN VOICE SYSTEM: Unable to allocate voice line descriptors!");
            return false;
        }
        for (int i = 0; i < count; i++)
        {
            new (&table[i]) human_voice_line_info();
        }
        Num_dlc_voice_line_descriptors = 0;
        Dlc_voice_line_descriptors = table;
    }
    else
    {
        human_voice_line_info* table = (human_voice_line_info*)Permanent_mempool.static_mempool_base::alloc(count * sizeof(human_voice_line_info), 4);
        if (table == nullptr)
        {
            keen::debug::nativePrintArgs("HUMAN VOICE SYSTEM: Unable to allocate voice line descriptors!");
            return false;
        }
        for (int i = 0; i < count; i++)
        {
            new (&table[i]) human_voice_line_info();
        }
        Num_voice_line_descriptors = 0;
        Voice_line_descriptors = table;
    }
    for (xml_element* line = xml_find(root, "Voice_Line"); line != nullptr; line = xml_find_next(root, line, "Voice_Line"))
    {
        const checksum_stri crc(xml_table_get_req_string_ref(line, "Name"), 0);
        human_voice_line_info* info;
        if (dlc)
        {
            info = &Dlc_voice_line_descriptors[Num_dlc_voice_line_descriptors];
        }
        else
        {
            bool exists = false;
            for (int i = 0; i < Num_voice_line_descriptors; i++)
            {
                if (Voice_line_descriptors[i].name_crc.get_crc() == crc.get_crc())
                {
                    exists = true;
                    break;
                }
            }
            if (exists)
            {
                continue;
            }
            info = &Voice_line_descriptors[Num_voice_line_descriptors];
        }
        info->name_crc = crc;
        int max_simultaneous = 0;
        xml_table_get_opt_int(max_simultaneous, line, "Max_Simultaneous");
        // Magic number: at most 100 simultaneous plays.
        if (max_simultaneous < 0 || max_simultaneous > 100)
        {
            max_simultaneous = 0;
        }
        info->max_simultaneous = (unsigned short)max_simultaneous;
        xml_element* types = xml_table_find_req(line, "Types");
        for (xml_element* entry = xml_find(types, "Entry"); entry != nullptr; entry = xml_find_next(types, entry, "Entry"))
        {
            const human_voice_persona_groups group = human_voice_persona_group_from_name(xml_table_get_req_string_ref(entry, "Persona_Type"));
            human_voice_line_group_info& desc = info->group_descriptors[group];
            if (desc.flags.valid)
            {
                keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n");
                continue;
            }
            // Magic number: the times are milliseconds, up to a minute.
            get_xml_uint(desc.global_cooldown, entry, "Global_Cooldown", 0, 60000);
            get_xml_uint(desc.local_cooldown, entry, "Local_Cooldown", 0, 60000);
            if (desc.local_cooldown < desc.global_cooldown)
            {
                swap_voice_lines(desc.global_cooldown, desc.local_cooldown);
            }
            get_xml_uint(desc.min_delay, entry, "Default_Min_Delay", 0, 60000);
            get_xml_uint(desc.max_delay, entry, "Default_Max_Delay", 0, 60000);
            if (desc.max_delay < desc.min_delay)
            {
                swap_voice_lines(desc.min_delay, desc.max_delay);
            }
            get_xml_uint(desc.play_chance_pct, entry, "Play_Chance", 0, 100);
            // The priority is its first digit; '0' means 10, anything outside 1-10 is clamped to 1-9.
            int priority = xml_table_get_req_string_ref(entry, "Default_Priority")[0] - '0';
            if (priority == 0)
            {
                priority = 10;
            }
            else if (priority < 1 || priority > 10)
            {
                keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n");
                if (priority < 1)
                {
                    priority = 1;
                }
                else if (priority > 9)
                {
                    priority = 9;
                }
            }
            desc.default_priority = (unsigned char)priority;
            const char* flags = xml_table_get_req_string_ref(entry, "Flags");
            if (strstr(flags, "EDF_Projected") != nullptr)
            {
                desc.flags.edf_projected = 1;
            }
            if (strstr(flags, "PA_Projected") != nullptr)
            {
                desc.flags.pa_projected = 1;
            }
            if (strstr(flags, "SPK_Projected") != nullptr)
            {
                desc.flags.spk_projected = 1;
            }
            if (strstr(flags, "Guerilla_Radio_Far") != nullptr)
            {
                desc.flags.gur_radio_far = 1;
            }
            if (strstr(flags, "Guerilla_Radio_Local") != nullptr)
            {
                desc.flags.gur_radio_local = 1;
            }
            if (strstr(flags, "Guerilla_PC_Radio") != nullptr)
            {
                desc.flags.gur_pc_transmit = 1;
            }
            if (strstr(flags, "MP_Announcer") != nullptr)
            {
                desc.flags.mp_announcer = 1;
            }
            if (strstr(flags, "Subtitles") != nullptr)
            {
                desc.flags.subtitled = 1;
            }
            if (strstr(flags, "Force_Dialogue") != nullptr)
            {
                desc.flags.force_dialogue = 1;
            }
            if (strstr(flags, "Clear_or_Radio") != nullptr)
            {
                desc.flags.clear_or_radio = 1;
            }
            desc.flags.valid = 1;
        }
        if (dlc)
        {
            Num_dlc_voice_line_descriptors++;
        }
        else
        {
            Num_voice_line_descriptors++;
        }
    }
    return true;
}

// 0x00A0DB60 (:694): the situational lines' descriptors in the base table.
static void create_voice_line_lookups()
{
    for (int line = 0; line < NUM_VOICE_LINES; line++)
    {
        const checksum_stri crc(Voice_line_lookup_table[line].name, 0);
        for (int i = 0; i < Num_voice_line_descriptors; i++)
        {
            if (Voice_line_descriptors[i].name_crc.get_crc() == crc.get_crc())
            {
                Voice_line_lookup_table[line].descriptor = &Voice_line_descriptors[i];
                break;
            }
        }
    }
}

// 0x00A0DBD0 (:723): a line the scripts name that the table lacks gets a valid dummy descriptor (in every group: 1 s
// cooldowns, no delay, 100% chance, priority 2) so its plays fail quietly.
static voice_line_handle add_dummy_voice_line(const char* line_name)
{
    const int handle = Num_voice_line_descriptors;
    if (Num_voice_line_descriptors >= Max_voice_line_descriptors)
    {
        keen::debug::nativePrintArgs("The human voice system has run out of space due to creating too many dummy\r\nreferences to invalid voice lines.  Please pay attention to the red box\r\nerrors on the screen and work with KateM and the designers to fix any invalid\r\nlines in the mission or activity scripts.\r\n");
        return INVALID_VOICE_LINE_HANDLE;
    }
    human_voice_line_info* info = &Voice_line_descriptors[Num_voice_line_descriptors++];
    info->name_crc = checksum_stri(line_name, 0);
    for (int group = 0; group < NUM_HUMAN_VOICE_PERSONA_GROUPS; group++)
    {
        human_voice_line_group_info& desc = info->group_descriptors[group];
        *(unsigned short*)&desc.flags = 0;
        desc.flags.valid = 1;
        desc.flags.dummy = 1;
        desc.global_cooldown_timer.set(0);
        // Magic numbers: 1000 ms local and global cooldowns, play chance 100, priority 2.
        desc.local_cooldown = 1000;
        desc.global_cooldown = 1000;
        desc.min_delay = 0;
        desc.max_delay = 0;
        desc.play_chance_pct = 100;
        desc.default_priority = 2;
    }
    return (voice_line_handle)handle;
}

// ---- human_voice_line_group_info / human_voice_line_info

// 0x00A06920 (.h:59): radio, PC transmissions and the announcer play without position.
bool human_voice_line_group_info::play_2d() const
{
    return flags.gur_radio_far || flags.gur_radio_local || flags.mp_announcer || flags.gur_pc_transmit;
}

// 0x00A0DC60 (:785)
human_voice_line_group_info::human_voice_line_group_info()
{
    local_cooldown = 0;
    global_cooldown = 0;
    min_delay = 0;
    max_delay = 0;
    play_chance_pct = 100;
    default_priority = 3;
    global_cooldown_timer.set(0);
    *(unsigned short*)&flags = 0;
}

// 0x00A0DC90 (:794)
human_voice_line_info::human_voice_line_info()
    : name_crc(checksum_stri::INVALID)
{
    max_simultaneous = 0;
}

// 0x00A06940 (.h:92)
int human_voice_line_info::global_cooldown_time_until(human_voice_persona_groups group) const
{
    return group_descriptors[group].global_cooldown_timer.time_until();
}

// 0x00A06960 (.h:101): the names are not kept, only their CRC.
const char* human_voice_line_info::get_name() const
{
    sprintf(last_voice_line_name, " CRC: 0x%08x ", name_crc.get_crc());
    return last_voice_line_name;
}

// ---- external functions

// 0x00A0DB50 (:686)
void human_voice_lines_dlc_shutdown()
{
    Num_dlc_voice_line_descriptors = 0;
    Dlc_voice_line_descriptors = nullptr;
}

// 0x00A32830 (:809). LTCG: pool goes on to parse_voice_line_descriptors in EAX (the caller's EBX).
bool human_voice_lines_parse_table(const char* table_name, bool dlc, mempool* pool)
{
    if (table_name == nullptr)
    {
        table_name = "rfg_voice_lines.xtbl";
    }
    xml_element* root = xml_table_parse(table_name);
    if (root == nullptr)
    {
        return false;
    }
    const bool result = parse_voice_line_descriptors(root, dlc, pool);
    if (!dlc)
    {
        create_voice_line_lookups();
    }
    xml_table_close(root);
    return result;
}

// 0x00A32890 (:840). The persona group whose voice line settings apply to a team's humans: any for no team, the
// marauders' for the teams allied with them, the EDF's for the teams friendly to it, else the guerrillas'.
// (human_teams_get_disposition inlined: an unknown team, or a multiplayer mode without teams, is guerrilla.)
human_voice_persona_groups human_voice_persona_group_from_team(human_teams team)
{
    if (team == HUMAN_TEAM_NONE)
    {
        return HVPG_ANY;
    }
    if (human_teams_are_allies(team, HUMAN_TEAM_MARAUDER))
    {
        return HVPG_MARAUDER;
    }
    if (human_teams_are_friendly(team, HUMAN_TEAM_EDF))
    {
        return HVPG_EDF;
    }
    return HVPG_GUERILLA;
}

// 0x00A0DCE0 (:856)
human_voice_line_info* human_voice_line_get_info(voice_lines line)
{
    if ((unsigned int)line < NUM_VOICE_LINES)
    {
        return Voice_line_lookup_table[line].descriptor;
    }
    return nullptr;
}

// 0x00A0DD00 (:861)
human_voice_line_info* human_voice_line_get_info(const char* line_name)
{
    return voice_line_info_from_crc(checksum_stri(line_name, 0));
}

// 0x00A0DD30 (:865)
human_voice_line_info* human_voice_line_get_info(voice_line_handle line_handle)
{
    if (line_handle == INVALID_VOICE_LINE_HANDLE)
    {
        return nullptr;
    }
    if ((line_handle & VOICE_LINE_HANDLE_IS_DLC) == 0)
    {
        if ((int)line_handle < Num_voice_line_descriptors)
        {
            return &Voice_line_descriptors[line_handle];
        }
    }
    else if ((int)(line_handle & ~VOICE_LINE_HANDLE_IS_DLC) < Num_dlc_voice_line_descriptors)
    {
        return &Dlc_voice_line_descriptors[line_handle & ~VOICE_LINE_HANDLE_IS_DLC];
    }
    return nullptr;
}

// 0x00A0DD80 (:881)
voice_line_handle voice_line_handle_from_crc(checksum_stri name_crc)
{
    human_voice_line_info* info = voice_line_info_from_crc(name_crc);
    if (info == nullptr)
    {
        return INVALID_VOICE_LINE_HANDLE;
    }
    if (voice_line_info_is_dlc(info))
    {
        return (voice_line_handle)((info - Dlc_voice_line_descriptors) | VOICE_LINE_HANDLE_IS_DLC);
    }
    return (voice_line_handle)(info - Voice_line_descriptors);
}

// 0x00A0DE00 (:903): a name the table lacks becomes a dummy line.
voice_line_handle voice_line_handle_from_name(const char* line_name)
{
    if (line_name == nullptr)
    {
        return INVALID_VOICE_LINE_HANDLE;
    }
    human_voice_line_info* info = voice_line_info_from_name(line_name);
    if (info == nullptr)
    {
        return add_dummy_voice_line(line_name);
    }
    if (voice_line_info_is_dlc(info))
    {
        return (voice_line_handle)((info - Dlc_voice_line_descriptors) | VOICE_LINE_HANDLE_IS_DLC);
    }
    return (voice_line_handle)(info - Voice_line_descriptors);
}

// 0x00A0DEA0 (:930)
voice_line_handle voice_line_handle_from_line(voice_lines line)
{
    if ((unsigned int)line < NUM_VOICE_LINES && Voice_line_lookup_table[line].descriptor != nullptr)
    {
        return (voice_line_handle)(Voice_line_lookup_table[line].descriptor - Voice_line_descriptors);
    }
    return INVALID_VOICE_LINE_HANDLE;
}

// 0x00A0DEE0 (:943)
voice_lines voice_line_from_name(const char* line_name)
{
    for (int line = 0; line < NUM_VOICE_LINES; line++)
    {
        if (v_stricmp(Voice_line_lookup_table[line].name, line_name) == 0)
        {
            return (voice_lines)line;
        }
    }
    return VOICE_LINE_NONE;
}

// 0x00A0DF20 (:958): the names are not kept, only their CRC.
const char* voice_line_handle_to_name(voice_line_handle line_handle)
{
    if (line_handle != INVALID_VOICE_LINE_HANDLE)
    {
        if ((line_handle & VOICE_LINE_HANDLE_IS_DLC) == 0)
        {
            if ((int)line_handle < Num_voice_line_descriptors)
            {
                sprintf(last_voice_line_name, " CRC: 0x%08x ", Voice_line_descriptors[line_handle].name_crc.get_crc());
                return last_voice_line_name;
            }
        }
        else
        {
            const int index = line_handle & ~VOICE_LINE_HANDLE_IS_DLC;
            if (index < Num_dlc_voice_line_descriptors)
            {
                sprintf(last_voice_line_name, " CRC: 0x%08x ", Dlc_voice_line_descriptors[index].name_crc.get_crc());
                return last_voice_line_name;
            }
        }
    }
    return "(error)";
}
