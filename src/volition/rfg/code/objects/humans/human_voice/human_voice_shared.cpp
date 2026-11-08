// Original: volition/rfg/code/objects/humans/human_voice/human_voice_shared.cpp
//
// The voice persona data shared with the tools: the persona file header, lines and variations (offsets fixed up to
// pointers and cue CRCs resolved to cue ids on load), the persona groups and human_voice_persona's table side (the
// persona table builds one stream2 container per persona). The external interface is declared in
// human_voice_shared.h. SFX.md SX6: ported whole.

#include "volition/rfg/code/objects/humans/human_voice/human_voice_shared.h"

#include "keen/base/filepath.hpp"
#include "keen/base/string.hpp"
#include "keen/base/trace.hpp"
#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/rfg/code/gamewide/asset_assembler.h"
#include "volition/rfg/code/gamewide/downloadable_content.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice_lines.h"
#include "volition/stream2/code/stream2.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/vlib/code/time/timestamp.h"
#include "volition/vlib/code/util/fastxml.h"
#include "volition/vlib/code/util/string_pool.h"
#include "volition/vlib/code/util/string_util.h"
#include "volition/vlib/code/util/varray.h"

#include <stdio.h>
#include <string.h>

// ---- file-static globals
static const char* Voice_line_group_names[3] = {"Guerilla", "EDF", "Marauder"}; // 0x01694424
// 0x016AE148 and 0x016AE13C, read from rfg.exe: the personas is_always_loaded_persona keeps loaded (the DLC list only
// for DLC personas).
static const char* ALWAYS_LOADED_PERSONA_NAMES[6] = {"PCM01", "OFFCAM", "SAFE", "RFC", "HANDBOOK", "TUTORIAL"};
static const char* DLC_ALWAYS_LOADED_PERSONA_NAMES[3] = {"DSAM", "DVASHA", "DOFFCAM"};
// human_voice_line_info* Voice_line_descriptors (0x0302F08C) is used here too; placed in human.cpp.
// int Num_dlc_voice_line_descriptors (0x0302F090) is used here too; placed in human_voice.cpp.
// human_voice_line_info* Dlc_voice_line_descriptors (0x0302F094) is used here too; placed in human_voice.cpp.

// Magic numbers: the persona data file signature ('0JXo' little endian) and the newest version (versions below 3 are
// refused by game_init; is_supported_version accepts 3 only, below 4).
static const unsigned int PERSONA_DATA_SIGNATURE = 0x6F584A30;
static const unsigned int PERSONA_DATA_VERSION = 3;

// ---- internal functions

// 0x00A0DF80 (:80). LTCG: name in EDI, dlc on the stack.
static bool is_always_loaded_persona(const char* name, bool dlc)
{
    for (int i = 0; i < 6; i++)
    {
        if (v_stricmp(name, ALWAYS_LOADED_PERSONA_NAMES[i]) == 0)
        {
            return true;
        }
    }
    if (dlc)
    {
        for (int i = 0; i < 3; i++)
        {
            if (v_stricmp(name, DLC_ALWAYS_LOADED_PERSONA_NAMES[i]) == 0)
            {
                return true;
            }
        }
    }
    return false;
}

// ---- human_voice_persona_line (human_voice_internal_shared.h)

// 0x00A0D5E0 (.h:59): the cooldown timer shares the word that holds the line name CRC in the file (game_init sets it).
timestamp& human_voice_persona_line::cooldown_timer()
{
    return reinterpret_cast<timestamp&>(name_crc_or_timer);
}

// 0x00A0D5F0 (.h:60)
const timestamp& human_voice_persona_line::cooldown_timer() const
{
    return reinterpret_cast<const timestamp&>(name_crc_or_timer);
}

// 0x00A0D600 (.h:64)
voice_line_handle human_voice_persona_line::get_voice_line_handle() const
{
    return descriptor_idx == 0xFFFF ? INVALID_VOICE_LINE_HANDLE : (voice_line_handle)descriptor_idx;
}

// 0x00A0E000 (:141)
human_voice_persona_line::human_voice_persona_line()
{
    variation_start_offset = 0xFFFF;
    descriptor_idx = 0xFFFF;
    num_variations = 0;
    next_variation = 0;
    name_crc_or_timer = 0xFFFFFFFF;
}

// 0x00A0E0C0 (:270): the line's descriptor from its name CRC, a random first variation, and the cooldown timer
// (replacing the CRC) set to now.
bool human_voice_persona_line::game_init()
{
    descriptor_idx = (unsigned short)voice_line_handle_from_crc(checksum_stri(name_crc_or_timer));
    if (num_variations != 0)
    {
        next_variation = (unsigned char)int_rand_range(0, num_variations - 1);
    }
    else
    {
        next_variation = 0;
    }
    cooldown_timer().set(0);
    return true;
}

// ---- human_voice_line_variation

// 0x00A0DFE0 (:132)
human_voice_line_variation::human_voice_line_variation()
{
    filename_or_cue_id = -1;
    subtitle_crc = 0;
    lipsync_data_offset = 0xFFFF;
}

// 0x00A0E060 (:218): the cue CRC (version 2 and later; older files have none) becomes the cue id, and a lipsync offset
// outside the persona's lipsync data is dropped.
bool human_voice_line_variation::game_init(const human_voice_persona_data_header* header)
{
    unsigned int cue_name_crc = 0;
    int cue_id = -1;
    if (header->version > 1)
    {
        cue_name_crc = (unsigned int)filename_or_cue_id;
    }
    audiolib_cue_get_id(cue_name_crc, &cue_id);
    filename_or_cue_id = cue_id;
    if (lipsync_data_offset != 0xFFFF && lipsync_data_offset >= header->lipsync_size)
    {
        lipsync_data_offset = 0xFFFF;
    }
    return cue_id != -1;
}

// ---- human_voice_persona_data_header

// 0x00A0E020 (:147)
void human_voice_persona_data_header::set_name(const char* new_name)
{
    strncpy(name, new_name, 64);
    name[63] = '\0';
}

// 0x00A32900 (:153)
human_voice_persona_data_header::human_voice_persona_data_header(const char* new_name)
{
    signature = PERSONA_DATA_SIGNATURE;
    version = PERSONA_DATA_VERSION;
    strncpy(name, new_name, 64);
    name[63] = '\0';
    num_lines = 0;
    num_variations = 0;
    lipsync_size = 0;
    lines = nullptr;
    variations = nullptr;
    lipsync_data = nullptr;
}

// 0x00A0E040 (:168)
bool human_voice_persona_data_header::is_supported_version() const
{
    if (signature == PERSONA_DATA_SIGNATURE && version < 4)
    {
        return version >= 3;
    }
    return false;
}

// 0x00A32950 (:304): every line and variation is initialized (inlined human_voice_persona_line::game_init and
// human_voice_line_variation::game_init).
bool human_voice_persona_data_header::game_init()
{
    if (signature == PERSONA_DATA_SIGNATURE && version < 4 && version > 2)
    {
        for (unsigned int i = 0; i < num_lines; i++)
        {
            human_voice_persona_line& line = lines[i];
            // Inlined voice_line_handle_from_crc (the handle is kept in 16 bits: none is 0xFFFF).
            line.descriptor_idx = (unsigned short)voice_line_handle_from_crc(checksum_stri(line.name_crc_or_timer));
            line.next_variation = line.num_variations == 0 ? 0 : (unsigned char)int_rand_range(0, line.num_variations - 1);
            line.cooldown_timer().set(0);
            for (unsigned int v = 0; v < lines[i].num_variations; v++)
            {
                human_voice_line_variation& variation = variations[lines[i].variation_start_offset + v];
                unsigned int cue_name_crc = 0;
                int cue_id = -1;
                if (version > 1)
                {
                    cue_name_crc = (unsigned int)variation.filename_or_cue_id;
                }
                audiolib_cue_get_id(cue_name_crc, &cue_id);
                variation.filename_or_cue_id = cue_id;
                if (variation.lipsync_data_offset != 0xFFFF && variation.lipsync_data_offset >= lipsync_size)
                {
                    variation.lipsync_data_offset = 0xFFFF;
                }
            }
        }
        return true;
    }
    // The message goes to a per-thread error buffer (the TLS block's 0x38 slot) that nothing reads here.
    char message[128];
    sprintf(message, "The data for persona %s is corrupt or an unsupported version.", name);
    return false;
}

// 0x00A32B10 (:343): the three offsets (from the header start; 0 or -1 for none) become pointers.
void human_voice_persona_offsets_to_pointers(human_voice_persona_data_header& target)
{
    unsigned char* base = (unsigned char*)&target;
    const uintptr_t lipsync = (uintptr_t)target.lipsync_data.to_pointer();
    target.lipsync_data = (lipsync == 0xFFFFFFFF || lipsync == 0) ? nullptr : (unsigned char*)(base + lipsync);
    const uintptr_t lines = (uintptr_t)target.lines.to_pointer();
    target.lines = (lines == 0xFFFFFFFF || lines == 0) ? nullptr : (human_voice_persona_line*)(base + lines);
    const uintptr_t variations = (uintptr_t)target.variations.to_pointer();
    target.variations = (variations == 0xFFFFFFFF || variations == 0) ? nullptr : (human_voice_line_variation*)(base + variations);
}

// 0x00A0E110 (:361)
human_voice_persona_groups human_voice_persona_group_from_name(const char* group_name)
{
    for (int i = HVPG_GUERILLA; i < NUM_HUMAN_VOICE_PERSONA_GROUPS; i++)
    {
        if (v_stricmp(group_name, Voice_line_group_names[i]) == 0)
        {
            return (human_voice_persona_groups)i;
        }
    }
    keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n");
    return HVPG_GUERILLA;
}

// 0x00A0E160 (:378)
const char* human_voice_persona_group_get_name(human_voice_persona_groups group)
{
    if ((unsigned int)group < NUM_HUMAN_VOICE_PERSONA_GROUPS)
    {
        return Voice_line_group_names[group];
    }
    return "(invalid)";
}

// ---- human_voice_persona

// 0x00A2C640 (.h:94)
human_voice_persona::human_voice_persona()
{
    name = nullptr;
    resource_filename = nullptr;
    srid = 0;
    group = HVPG_ANY;
    voice_line_data = nullptr;
    voice_line_dlc_disable_data = nullptr;
    situational_lines = nullptr;
    last_operation_status = PLAY_VOICE_SUCCESS;
    *(unsigned int*)&flags = 0;
}

// 0x00A057B0 (.h:96)
const char* human_voice_persona::get_name() const
{
    return name;
}

// 0x00A057C0 (.h:97)
human_voice_persona_groups human_voice_persona::get_voice_persona_group() const
{
    return group;
}

// 0x00A057D0 (.h:132)
play_voice_status human_voice_persona::get_last_operation_status() const
{
    return last_operation_status;
}

// 0x00A057E0 (.h:153)
bool human_voice_persona::multiplayer() const
{
    return flags.multiplayer != 0;
}

// 0x00A057F0 (.h:154)
unsigned int human_voice_persona::get_srid() const
{
    return srid;
}

// 0x00A05800 (.h:196)
bool human_voice_persona::get_dlc() const
{
    return dlc;
}

// 0x00A0E180 (:384). Original: dlc and reference_count are left as they are.
void human_voice_persona::reset()
{
    name = nullptr;
    resource_filename = nullptr;
    srid = 0;
    group = HVPG_ANY;
    voice_line_data = nullptr;
    voice_line_dlc_disable_data = nullptr;
    situational_lines = nullptr;
    last_operation_status = PLAY_VOICE_SUCCESS;
    *(unsigned int*)&flags = 0;
}

// 0x00A32B60 (:402)
human_voice_persona* human_voice_persona_from_name(const char* persona_name)
{
    for (int i = 0; i < Voice_personas.size(); i++)
    {
        if (v_stricmp(persona_name, Voice_personas[i].name) == 0)
        {
            return &Voice_personas[i];
        }
    }
    return nullptr;
}

// 0x00A59FD0 (:415): one stream2 container per Persona of the table (named after its file, localized with
// lang_suffix: "<file>_<lang><ext>"), holding the persona's foley bank (.vfdvp_pc, GSP_FOLEY_BANK) and its voice line
// data (.rfgvp_pc, GSP_VOICE_PERSONA) when they exist. Always loaded personas go in the always loaded container type.
bool human_voice_persona::parse_personas_table_shared(const char* filename, mempool_base* tmp_pool, mempool* pool, bool load_from_preload, vlib_platform plat, bool verify, bool dlc, const char* lang_suffix)
{
    human_voice_persona new_persona;
    Persona_string_pool.set_thread_ownership();
    if (dlc)
    {
        assasm_start("dlc_persona_containers", tmp_pool, (mempool_base*)pool, load_from_preload, nullptr, false, true, true, false);
        Persona_string_pool.mark();
    }
    else
    {
        assasm_start("persona_containers", tmp_pool, (mempool_base*)pool, load_from_preload, nullptr, false, false, true, false);
    }
    xml_element* root = xml_table_parse(filename);
    if (root == nullptr)
    {
        return false;
    }
    // Original: a DLC table whose temporary container list cannot be allocated is parsed as a non-DLC one.
    if (dlc && !load_from_preload && !dlc_allocate_temp_container_list(25, (mempool_base*)pool))
    {
        dlc = false;
    }
    for (xml_element* entry = xml_find(root, "Persona"); entry != nullptr; entry = xml_find_next(root, entry, "Persona"))
    {
        if (Voice_personas.full())
        {
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n");
            break;
        }
        const char* persona_name = xml_table_get_opt_string_ref(entry, "Name");
        if (persona_name == nullptr || human_voice_persona_from_name(persona_name) != nullptr)
        {
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n");
            continue;
        }
        const char* pooled_name = Persona_string_pool.add(persona_name);
        if (pooled_name == nullptr)
        {
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n");
            break;
        }
        const char* persona_file = xml_table_get_opt_filename_ref(entry, "PersonaFile");
        if (persona_file == nullptr)
        {
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n");
            continue;
        }
        const char* container_name = persona_file;
        char localized_name[64];
        if (lang_suffix != nullptr)
        {
            keen::FilePath path(persona_file, false);
            keen::formatString(localized_name, 64, "%s_%s%s", path.getFileName(), lang_suffix, path.getExtension());
            keen::changeStringToLowercase(localized_name);
            container_name = localized_name;
        }
        char base_name[64];
        cf_drop_ext(base_name, 64, container_name);
        const char* resource_name = Persona_string_pool.add(base_name);
        if (resource_name == nullptr)
        {
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n");
            break;
        }
        // Original: the unlocalized base name is pooled too and never used.
        cf_drop_ext(base_name, 64, persona_file);
        if (Persona_string_pool.add(base_name) == nullptr)
        {
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n");
            break;
        }
        const char* persona_type = xml_table_get_opt_string_ref(entry, "Persona_Type");
        if (persona_type == nullptr)
        {
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n");
            continue;
        }
        // One extension per vlib_platform (all the same on PC).
        const char* voice_data_ext[8] = {".rfgvp_pc", ".rfgvp_pc", ".rfgvp_pc", ".rfgvp_pc", ".rfgvp_pc", ".rfgvp_pc", ".rfgvp_pc", ".rfgvp_pc"};
        const char* foley_bank_ext[8] = {".vfdvp_pc", ".vfdvp_pc", ".vfdvp_pc", ".vfdvp_pc", ".vfdvp_pc", ".vfdvp_pc", ".vfdvp_pc", ".vfdvp_pc"};
        bool always_loaded;
        if (!xml_table_get_opt_bool(always_loaded, entry, "always_loaded"))
        {
            always_loaded = false;
        }
        // Magic numbers: GSC_VOICE_PERSONA (30) and GSC_ALWAYS_LOADED_VOICE_PERSONA (31).
        unsigned char container_type = 30;
        if (always_loaded || is_always_loaded_persona(pooled_name, dlc))
        {
            container_type = 31;
        }
        if (!assasm_container_start(container_name, container_type, false, false))
        {
            // Original: returns with the table open and the assembler started.
            return false;
        }
        char prim_name[64];
        cf_add_ext(prim_name, 64, resource_name, foley_bank_ext[plat], false);
        if (cf_exist(prim_name))
        {
            // Magic number: 14, GSP_FOLEY_BANK.
            assasm_container_add(prim_name, 14, "human voice", false, false, false, nullptr, 0);
        }
        cf_add_ext(prim_name, 64, resource_name, voice_data_ext[plat], false);
        if (cf_exist(prim_name))
        {
            // Magic number: 16, GSP_VOICE_PERSONA.
            assasm_container_add(prim_name, 16, "human voice", false, false, false, nullptr, 0);
        }
        const unsigned int container_srid = assasm_container_finish(false, dlc, false);
        if (container_srid == 0)
        {
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n");
            continue;
        }
        new_persona.reset();
        new_persona.name = pooled_name;
        new_persona.resource_filename = resource_name;
        new_persona.group = human_voice_persona_group_from_name(persona_type);
        new_persona.flags.always_loaded = container_type == 31 ? 1 : 0;
        new_persona.srid = container_srid;
        new_persona.dlc = dlc;
        Voice_personas.add(new_persona);
    }
    xml_table_close(root);
    assasm_finish(nullptr, verify, true, false);
    return true;
}

// 0x00A32BB0 (:651): every persona's container is released and deleted (last first), then the names.
void human_voice_persona::persona_shutdown()
{
    for (int i = Voice_personas.size() - 1; i >= 0; i--)
    {
        if (Voice_personas[i].srid != 0)
        {
            if (stream2_get_ref_count(Voice_personas[i].srid) > 0)
            {
                stream2_release(Voice_personas[i].srid);
            }
            stream2_container_delete(Voice_personas[i].srid);
            Voice_personas[i].srid = 0;
        }
        Voice_personas.remove(i);
    }
    Persona_string_pool.set_thread_ownership();
    Persona_string_pool.reset_mark();
    Persona_string_pool.clear();
}

// 0x00A32CB0 (:674): the DLC personas leave the table and the names go back to the mark. Original: their containers
// are neither released nor deleted here (dlc_persona_shutdown runs after the DLC containers are gone).
void human_voice_persona::dlc_persona_shutdown()
{
    for (int i = Voice_personas.size() - 1; i >= 0; i--)
    {
        if (Voice_personas[i].dlc)
        {
            Voice_personas.remove(i);
        }
    }
    Persona_string_pool.set_thread_ownership();
    Persona_string_pool.restore_to_mark(0);
}
