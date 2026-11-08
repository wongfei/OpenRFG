// Original: volition/rfg/code/objects/humans/human_voice/voice_line_unlocking.cpp
//
// Voice lines unlocked by single player progress (voice_line_unlocking.xtbl): each group's lines open and close after
// district liberations, missions, upgrade purchases and team friendship, and are chosen round robin. The external
// interface is declared in voice_line_unlocking.h. SFX.md SX6: ported whole (single player only: nothing in a
// multiplayer path chooses an unlocking line).

#include "volition/rfg/code/objects/humans/human_voice/voice_line_unlocking.h"

#include "keen/base/trace.hpp"
#include "volition/rfg/code/gameplay/mission_info.h"
#include "volition/rfg/code/gameplay/object_district.h"
#include "volition/rfg/code/gameplay/upgrades.h"
#include "volition/rfg/code/interface/localization/localization_shared.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice_lines.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice_shared.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/humans/player_meta_data.h"
#include "volition/rfg/code/rfg.h"
#include "volition/vlib/code/memory/rfg_split_mempool.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/vlib/code/util/fastxml.h"
#include "volition/vlib/code/util/string_util.h"

#include <new>

// ---- file-static globals
// player* Player (0x03023874) is used here too; placed in player.h.
static voice_line_unlocking_group* Voice_line_unlocking_groups; // 0x0302F098
static int Num_voice_line_unlocking_groups; // 0x0302F09C

// ---- constructors

// 0x00A0E1B0 (:41). Original: the district hashes, the purchase unlock and the friendly team are left uninitialized
// (voice_line_unlocking_system_init sets every field of a named line).
voice_line_unlocking_line::voice_line_unlocking_line()
{
    line_handle = INVALID_VOICE_LINE_HANDLE;
    unlock_after = INVALID_MISSION_HANDLE;
    lock_after = INVALID_MISSION_HANDLE;
    lock_after_purchase = 0xFFFFFFFF;
}

// 0x00A0E1D0 (:54)
voice_line_unlocking_group::voice_line_unlocking_group()
{
    lines = nullptr;
    num_lines = 0;
    cur_line_idx = 0;
    name[0] = '\0';
}

// ---- internal functions

// 0x00A0E1E0 (:73)
static const voice_line_unlocking_group* vlu_find(const char* name)
{
    for (int i = 0; i < Num_voice_line_unlocking_groups; i++)
    {
        if (v_strnicmp(Voice_line_unlocking_groups[i].name, name, 63) == 0)
        {
            return &Voice_line_unlocking_groups[i];
        }
    }
    return nullptr;
}

// The district of a liberation hash is liberated (inlined four times in voice_line_unlocking_choose).
static district* vlu_district_from_hash(unsigned int district_hash)
{
    const unsigned int handle = get_district_by_hash(district_hash);
    if (handle == 0xFFFFFFFF)
    {
        return nullptr;
    }
    return World.object_cast<district>(World.object_from_handle(handle));
}

// The handle of an upgrade named by its localization key, or -1 for none (inlined twice in
// voice_line_unlocking_system_init).
static unsigned int vlu_upgrade_from_key(const char* key)
{
    if (key == nullptr)
    {
        return 0xFFFFFFFF;
    }
    return upgrades_get_handle(localization_generate_hash_from_key(key));
}

// ---- external functions

// 0x00A0E230 (:93): once; the groups and their lines come from the permanent mempool.
void voice_line_unlocking_system_init()
{
    if (Voice_line_unlocking_groups != nullptr)
    {
        return;
    }
    xml_element* root = xml_table_parse("voice_line_unlocking.xtbl");
    if (root == nullptr)
    {
        return;
    }
    Num_voice_line_unlocking_groups = (int)xml_count(root, "Line_Group");
    voice_line_unlocking_group* groups = (voice_line_unlocking_group*)Permanent_mempool.static_mempool_base::alloc(Num_voice_line_unlocking_groups * sizeof(voice_line_unlocking_group), 4);
    if (groups == nullptr)
    {
        // Original: the table is left open.
        return;
    }
    for (int i = 0; i < Num_voice_line_unlocking_groups; i++)
    {
        new (&groups[i]) voice_line_unlocking_group();
    }
    Voice_line_unlocking_groups = groups;
    int num_groups = 0;
    for (xml_element* group_elem = xml_find(root, "Line_Group"); group_elem != nullptr; group_elem = xml_find_next(root, group_elem, "Line_Group"))
    {
        const char* group_name = xml_table_get_opt_string_ref(group_elem, "Name");
        if (group_name == nullptr || vlu_find(group_name) != nullptr)
        {
            continue;
        }
        if (num_groups >= Num_voice_line_unlocking_groups)
        {
            break;
        }
        voice_line_unlocking_group& group = Voice_line_unlocking_groups[num_groups++];
        strncpy_safe(group.name, group_name, 64);
        xml_element* lines_elem = xml_find(group_elem, "Lines");
        if (lines_elem == nullptr)
        {
            continue;
        }
        group.num_lines = (int)xml_count(lines_elem, "Line");
        group.cur_line_idx = int_rand_range(0, group.num_lines - 1);
        voice_line_unlocking_line* lines = (voice_line_unlocking_line*)Permanent_mempool.static_mempool_base::alloc(group.num_lines * sizeof(voice_line_unlocking_line), 4);
        if (lines != nullptr)
        {
            for (int i = 0; i < group.num_lines; i++)
            {
                new (&lines[i]) voice_line_unlocking_line();
            }
        }
        group.lines = lines;
        int num_lines = 0;
        for (xml_element* line_elem = xml_find(lines_elem, "Line"); line_elem != nullptr; line_elem = xml_find_next(lines_elem, line_elem, "Line"))
        {
            if (num_lines >= group.num_lines)
            {
                break;
            }
            voice_line_unlocking_line& line = group.lines[num_lines++];
            const char* line_name = xml_table_get_opt_string_ref(line_elem, "Line_Name");
            if (line_name == nullptr)
            {
                continue;
            }
            line.line_handle = voice_line_handle_from_name(line_name);
            const char* text = xml_table_get_opt_string_ref(line_elem, "Begins_after_liberation_1");
            line.unlock_after_district_hash_1 = text != nullptr ? str_hash_string(text) : 0;
            text = xml_table_get_opt_string_ref(line_elem, "Begins_after_liberation_2");
            line.unlock_after_district_hash_2 = text != nullptr ? str_hash_string(text) : 0;
            text = xml_table_get_opt_string_ref(line_elem, "Ends_after_liberation_1");
            line.lock_after_district_hash_1 = text != nullptr ? str_hash_string(text) : 0;
            text = xml_table_get_opt_string_ref(line_elem, "Ends_after_liberation_2");
            line.lock_after_district_hash_2 = text != nullptr ? str_hash_string(text) : 0;
            text = xml_table_get_opt_string_ref(line_elem, "Begins_After");
            line.unlock_after = text != nullptr ? mission_handle_find(text) : INVALID_MISSION_HANDLE;
            text = xml_table_get_opt_string_ref(line_elem, "Ends_After");
            line.lock_after = text != nullptr ? mission_handle_find(text) : INVALID_MISSION_HANDLE;
            line.unlock_after_purchase = vlu_upgrade_from_key(xml_table_get_opt_string_ref(line_elem, "Begins_after_purchase"));
            line.lock_after_purchase = vlu_upgrade_from_key(xml_table_get_opt_string_ref(line_elem, "Ends_After_Purchase"));
            text = xml_table_get_opt_string_ref(line_elem, "Begins_after_friendly");
            line.unlock_after_friendly = HUMAN_TEAM_NONE;
            if (text != nullptr)
            {
                if (v_stricmp(text, "Guerilla") == 0)
                {
                    line.unlock_after_friendly = HUMAN_TEAM_GUERILLA;
                }
                else if (v_stricmp(text, "Marauder") == 0)
                {
                    line.unlock_after_friendly = HUMAN_TEAM_MARAUDER;
                }
                else if (v_stricmp(text, "EDF") == 0)
                {
                    line.unlock_after_friendly = HUMAN_TEAM_EDF;
                }
                else if (v_stricmp(text, "Civilian") == 0)
                {
                    line.unlock_after_friendly = HUMAN_TEAM_CIVILIAN;
                }
                else
                {
                    keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n");
                }
            }
        }
    }
    xml_table_close(root);
}

// 0x00A0E620 (:254)
voice_line_unlocking_handle voice_line_unlocking_get_handle_slow(const char* name)
{
    const voice_line_unlocking_group* group = vlu_find(name);
    if (group != nullptr)
    {
        return (voice_line_unlocking_handle)(group - Voice_line_unlocking_groups);
    }
    return INVALID_VOICE_LINE_UNLOCKING_HANDLE;
}

// 0x00A0E650 (:266): the four built-in groups are the first four of the table.
voice_line_unlocking_handle voice_line_unlocking_get_handle(vlu_groups group)
{
    if ((unsigned int)group > VLUG_GUERRILLA_VIP)
    {
        return INVALID_VOICE_LINE_UNLOCKING_HANDLE;
    }
    return (voice_line_unlocking_handle)group;
}

// 0x00A32D60 (:282): from the group's current line on, the first unlocked line (its liberations, missions and
// purchases done, its locks not reached, the player friendly to its team); the group moves on past it.
// Original: the handle check lets vlu_handle == Num_voice_line_unlocking_groups through (one past the end).
voice_line_handle voice_line_unlocking_choose(voice_line_unlocking_handle vlu_handle)
{
    if ((int)vlu_handle < 0 || (int)vlu_handle > Num_voice_line_unlocking_groups)
    {
        return INVALID_VOICE_LINE_HANDLE;
    }
    voice_line_unlocking_group& group = Voice_line_unlocking_groups[vlu_handle];
    for (int i = 0; i < group.num_lines; i++)
    {
        const int index = (group.cur_line_idx + i) % group.num_lines;
        const voice_line_unlocking_line& line = group.lines[index];
        if (line.line_handle == INVALID_VOICE_LINE_HANDLE)
        {
            continue;
        }
        // The two unlocking liberations: a named first district that is not found locks the line; one found but not
        // liberated needs the second to be named and liberated.
        bool need_second = false;
        if (line.unlock_after_district_hash_1 != 0)
        {
            need_second = true;
            district* dp = vlu_district_from_hash(line.unlock_after_district_hash_1);
            if (dp == nullptr)
            {
                continue;
            }
            if (dp->is_liberated())
            {
                goto unlocked;
            }
        }
        if (line.unlock_after_district_hash_2 != 0)
        {
            district* dp = vlu_district_from_hash(line.unlock_after_district_hash_2);
            if (dp == nullptr || !dp->is_liberated())
            {
                continue;
            }
        }
        else if (need_second)
        {
            continue;
        }
    unlocked:
        if (line.lock_after_district_hash_1 != 0)
        {
            district* dp = vlu_district_from_hash(line.lock_after_district_hash_1);
            if (dp == nullptr || dp->is_liberated())
            {
                continue;
            }
        }
        if (line.lock_after_district_hash_2 != 0)
        {
            district* dp = vlu_district_from_hash(line.lock_after_district_hash_2);
            if (dp == nullptr || dp->is_liberated())
            {
                continue;
            }
        }
        if (line.lock_after != INVALID_MISSION_HANDLE && mission_is_complete(line.lock_after))
        {
            continue;
        }
        if (line.unlock_after != INVALID_MISSION_HANDLE && !mission_is_complete(line.unlock_after))
        {
            continue;
        }
        player_meta_data* meta = player_get_meta_data();
        if (meta != nullptr)
        {
            if (line.unlock_after_purchase != 0xFFFFFFFF && meta->get_upgrade_value(line.unlock_after_purchase) == 0)
            {
                continue;
            }
            if (line.lock_after_purchase != 0xFFFFFFFF && meta->get_upgrade_value(line.lock_after_purchase) >= 1)
            {
                continue;
            }
        }
        if (Player != nullptr && line.unlock_after_friendly != HUMAN_TEAM_NONE && !human_teams_are_friendly(Player->current_team, line.unlock_after_friendly))
        {
            continue;
        }
        group.cur_line_idx = (index + 1) % group.num_lines;
        return line.line_handle;
    }
    return INVALID_VOICE_LINE_HANDLE;
}

// 0x00A32F90 (:394)
voice_line_handle voice_line_unlocking_choose(vlu_groups group)
{
    return voice_line_unlocking_choose(voice_line_unlocking_get_handle(group));
}

// 0x00A32FB0 (:400): every line of the group gets the cooldown on the human's voice.
void voice_line_unlocking_set_global_cooldown(human* hp, voice_line_unlocking_handle group_handle, int cooldown_ms)
{
    const human_voice_persona* voice = hp->voice;
    if (voice == nullptr || (int)group_handle < 0 || (int)group_handle >= Num_voice_line_unlocking_groups)
    {
        return;
    }
    const voice_line_unlocking_group& group = Voice_line_unlocking_groups[group_handle];
    for (int i = 0; i < group.num_lines; i++)
    {
        voice->set_line_cooldown(group.lines[i].line_handle, cooldown_ms);
    }
}

// 0x00A33010 (:420)
void voice_line_unlocking_set_global_cooldown(human* hp, vlu_groups group, int cooldown_ms)
{
    voice_line_unlocking_set_global_cooldown(hp, voice_line_unlocking_get_handle(group), cooldown_ms);
}

// 0x00A33030 (:425)
void voice_line_unlocking_set_global_cooldown_slow(human* hp, const char* group_name, int cooldown_ms)
{
    voice_line_unlocking_set_global_cooldown(hp, voice_line_unlocking_get_handle_slow(group_name), cooldown_ms);
}
