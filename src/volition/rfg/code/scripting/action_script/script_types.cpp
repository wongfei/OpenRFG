// Original: volition/rfg/code/scripting/action_script/script_types.cpp
//
// The script node tree: atoms (constants), variables and the parser that turns a .scriptx element into a node, resolving
// names (weapons, voice lines, ...) to handles as it goes.

#include "volition/rfg/code/scripting/action_script/script_types.h"

#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/rfg/code/code_effects/explosion_shared.h"
#include "volition/rfg/code/effects/effects_shared.h"
#include "volition/rfg/code/gameplay/activities/activity.h"
#include "volition/rfg/code/gameplay/activities/activity_continuous_attack.h"
#include "volition/rfg/code/gameplay/activities/raid_common.h"
#include "volition/rfg/code/gameplay/activities/vrail_data.h"
#include "volition/rfg/code/gameplay/air_bombing.h"
#include "volition/rfg/code/gameplay/gameflow/alert_level_manager.h"
#include "volition/rfg/code/gameplay/mission_info.h"
#include "volition/rfg/code/gameplay/upgrades.h"
#include "volition/rfg/code/gamewide/gamemusic.h"
#include "volition/rfg/code/geom/vanim.h"
#include "volition/rfg/code/hud/objectives.h"
#include "volition/rfg/code/interface/localization/localization_shared.h"
#include "volition/rfg/code/objects/humans/human_info.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice_lines.h"
#include "volition/rfg/code/objects/humans/human_voice/voice_line_unlocking.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/object_convoy.h"
#include "volition/rfg/code/objects/object_squad.h"
#include "keen/base/trace.hpp"
#include "volition/rfg/code/rfg.h"
#include "volition/vlib/code/memory/virtual_mempool.h"
#include "volition/rfg/code/scripting/action_script/action_script_internal.h"
#include "volition/rfg/code/scripting/public_address.h"
#include "volition/rfg/code/spawn/ambient_spawn_info.h"
#include "volition/rfg/code/vehicle/vehicle_system.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/util/fastxml.h"
#include "volition/vlib/code/util/string_comp.h"

#include <new>
#include <stdio.h>
#include <string.h>

script_variable_value Action_return_value; // 0x016B2658
script_variable_value Script_variable_values[128]; // 0x036F6DC0
int Variable_idx; // 0x03611288
action_script* Cur_script_loading; // 0x03611410
object_pool<script_atom,15360,1> Script_atoms(&script_atom::init); // 0x03611500
object_pool<script_variable,512,1> Script_variables(&script_variable::init); // 0x03695BE0

// 0x016B03E8: the element name of each script_node_type.
const char* Script_node_tags[NUM_SN_TYPES] = {
    "", "number", "string", "flag", "object", "object_list", "script_point", "variable", "variable_ref", "script", "condition", "action", "event",
    "function", "delay", "min", "max", "ref", "group", "repeatable", "disabled", "managed", "timer", "air_bomb_handle", "weapon",
};

// 0x00B230D0
script_node::script_node()
    : class_type(SNC_INVALID)
    , type(SN_VOID)
{
}

// 0x00B1F430 (.h:239)
script_node::~script_node()
{
}

static void script_node_bad_read()
{
    keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", "script_types.h", 0);
}

// 0x00B1F440 (.h:271)
script_node::operator const char*()
{
    script_node_bad_read();
    return nullptr;
}

// 0x00B1F460 (.h:272)
script_node::operator bool()
{
    script_node_bad_read();
    return false;
}

// 0x00B1F480 (.h:273)
script_node::operator float()
{
    script_node_bad_read();
    return 0.0f;
}

// 0x00B1F4A0 (.h:274)
script_node::operator int()
{
    script_node_bad_read();
    return 0;
}

// 0x00B1F4C0 (.h:275)
script_node::operator unsigned int()
{
    script_node_bad_read();
    return 0xFFFFFFFF;
}

// 0x00B1F4E0 (.h:276)
script_node::operator unsigned int*()
{
    script_node_bad_read();
    return nullptr;
}

// 0x00B1F500 (.h:277)
script_node::operator game_handle()
{
    script_node_bad_read();
    return GAME_HANDLE_UNDEFINED;
}

// 0x00B1F520 (.h:278)
script_node::operator script_point()
{
    script_node_bad_read();
    script_point point;
    point.ref_handle = 0;
    point.index = 0;
    return point;
}

// 0x00B231E0 (:280)
bool script_node::save(cfile* file)
{
    return cf_write_uint32(class_type, file) && cf_write_uint32(type, file);
}

// 0x00B23220 (:294)
void script_node::load(cfile* file)
{
    class_type = (script_node_classes)cf_read_uint32(file);
    type = (script_node_type)cf_read_uint32(file);
}

// 0x00B23250 (:300)
void script_node::init()
{
    class_type = SNC_INVALID;
    type = SN_VOID;
}

// 0x00B2D1C0 (:75)
script_atom::operator const char*()
{
    return value.string;
}

// 0x00B2D1D0 (:76)
script_atom::operator bool()
{
    return value.number != 0.0f;
}

// 0x00B2D1F0 (:77)
script_atom::operator float()
{
    return value.number;
}

// 0x00B2D200 (:78)
script_atom::operator int()
{
    return (int)value.number;
}

// 0x00B2D210 (:79)
script_atom::operator unsigned int()
{
    return value.obj;
}

// 0x00B2D220 (:80)
script_atom::operator unsigned int*()
{
    return value.obj_ptr;
}

// 0x00B2D230 (:81)
script_atom::operator game_handle()
{
    return value.handle;
}

// 0x00B2D240 (:82)
script_atom::operator script_point()
{
    return value.point;
}

// 0x00B2D180 (:87)
script_atom::script_atom(bool _flag)
{
    class_type = SNC_ATOM;
    type = SN_BOOL;
    value.number = _flag ? 1.0f : 0.0f;
}

// 0x00B2D270 (:88)
script_atom::script_atom(char* _string)
{
    class_type = SNC_ATOM;
    type = SN_STRING;
    value.string = _string;
}

// 0x00B2D290 (:89)
script_atom::script_atom(float _n)
{
    class_type = SNC_ATOM;
    type = SN_NUMBER;
    value.number = _n;
}

// 0x00B2D2C0 (:91)
script_atom::script_atom(unsigned int _obj)
{
    class_type = SNC_ATOM;
    type = SN_OBJECT;
    value.obj = _obj;
}

// 0x00B2D2E0 (:92)
script_atom::script_atom(unsigned int* _obj_ptr)
{
    class_type = SNC_ATOM;
    type = SN_OBJECT_LIST;
    value.obj_ptr = _obj_ptr;
}

// 0x00B2D300 (:93)
script_atom::script_atom(game_handle _handle)
{
    class_type = SNC_ATOM;
    type = SN_GAME_HANDLE;
    value.handle = _handle;
}

// 0x00B2D320 (:94)
script_atom::script_atom(script_point& _point)
{
    class_type = SNC_ATOM;
    type = SN_POINT;
    value.point = _point;
}

// 0x00B2D350 (:98)
script_atom::script_atom()
{
    class_type = SNC_ATOM;
    type = SN_VOID;
    value.number = 0.0f;
}

// 0x00B2D260
script_atom::~script_atom()
{
}

// 0x00B2D390 (:127)
script_variable::operator unsigned int()
{
    return script_variable_get(idx);
}

// 0x00B230E0 (:132)
script_variable::script_variable(int _idx)
    : idx(_idx)
{
    class_type = SNC_VARIABLE;
    type = SN_VARIABLE_REF;
}

// 0x00B2D3B0 (:136)
script_variable::script_variable()
    : idx(-1)
{
    class_type = SNC_VARIABLE;
    type = SN_VOID;
}

// 0x00B23100
script_variable::~script_variable()
{
}

// 0x00B23110 (:156)
void script_variable_value::clear()
{
    handle = 0xFFFFFFFF;
    valid = false;
}

// 0x00B23120 (:160)
script_variable_value::operator unsigned int()
{
    return handle;
}

// 0x00B23130 (:164)
void script_variable_value::set(unsigned int obj)
{
    handle = obj;
    valid = true;
}

// 0x00B23140 (:168)
script_variable_value::script_variable_value()
    : valid(false)
    , handle(0xFFFFFFFF)
{
}

// 0x00B23170 (:233)
script_atom* verify_new_script_atom(script_atom* atom)
{
    return atom;
}

// 0x00B23180 (:243)
script_variable* verify_new_script_variable(script_variable* variable)
{
    return variable;
}

// 0x00B23190 (:248) LTCG: dest_type in ESI, arg_type in EAX
static bool script_types_match(script_node_type dest_type, script_node_type arg_type)
{
    const bool bool_as_number = arg_type == SN_BOOL && dest_type == SN_NUMBER;
    const bool variable_ref = arg_type == SN_VARIABLE_REF && (dest_type == SN_OBJECT || dest_type == SN_TIMER || dest_type == SN_AIR_BOMB_HANDLE);
    return variable_ref || dest_type == arg_type || bool_as_number;
}

// 0x00B231D0 (:266)
void script_mempools_set_thread_ownership()
{
    Script_mempool.set_thread_ownership();
}

// 0x00B39470 (:315)
void script_atom::release()
{
    Script_atoms.free(this);
}

// 0x00B23260 (:322)
void script_atom::reset()
{
}

// 0x00B2D410 (:331)
bool script_atom::save(cfile* file)
{
    return cf_write_uint32(class_type, file) && cf_write_uint32(type, file) && script_variant_save(file, value, type);
}

// 0x00B32E40 (:350)
void script_atom::load(cfile* file)
{
    class_type = (script_node_classes)cf_read_uint32(file);
    type = (script_node_type)cf_read_uint32(file);
    script_variant_load(file, value, type);
}

// 0x00B23270 (:361)
void script_atom::init()
{
    type = SN_VOID;
    class_type = SNC_ATOM;
    memset(&value, 0, sizeof(value));
}

// 0x00B39480 (:380)
void script_variable::release()
{
    Script_variables.free(this);
}

// 0x00B23290 (:387)
void script_variable::reset()
{
}

// 0x00B232A0 (:396)
bool script_variable::save(cfile* file)
{
    return cf_write_uint32(class_type, file) && cf_write_uint32(type, file) && cf_write_int32(idx, file);
}

// 0x00B232F0 (:415)
void script_variable::load(cfile* file)
{
    class_type = (script_node_classes)cf_read_uint32(file);
    type = (script_node_type)cf_read_uint32(file);
    idx = cf_read_int32(file);
}

// 0x00B23320 (:426)
void script_variable::init()
{
    type = SN_VOID;
    class_type = SNC_VARIABLE;
    idx = -1;
}

// 0x00B23340 (:447)
bool script_variable_value::save(cfile* file)
{
    return cf_write_uint8(valid, file) && cf_write_uint32(handle, file);
}

// 0x00B23380 (:466)
void script_variable_value::load(cfile* file)
{
    valid = cf_read_uint8(file) != 0;
    handle = cf_read_uint32(file);
}

// 0x00B233B0 (:484)
bool script_variant_save(cfile* file, script_variant& value, script_node_type type)
{
    switch (type)
    {
    case SN_NUMBER:
    case SN_BOOL:
        return cf_write_float(value.number, file);
    case SN_STRING:
        return cf_write_string(value.string, file);
    case SN_OBJECT:
    case SN_GAME_HANDLE:
        return cf_write_uint32(value.obj, file);
    case SN_OBJECT_LIST:
    {
        // The count is written back after the terminated list.
        unsigned int count = 0;
        const unsigned int count_pos = cf_tell(file);
        if (!cf_write_uint32(0, file))
        {
            return false;
        }
        for (; value.obj_ptr[count] != 0xFFFFFFFF; count++)
        {
            if (!cf_write_uint32(value.obj_ptr[count], file))
            {
                return false;
            }
        }
        const unsigned int end_pos = cf_tell(file);
        return cf_seek(file, count_pos, CF_SEEK_SET) && cf_write_uint32(count, file) && cf_seek(file, end_pos, CF_SEEK_SET);
    }
    case SN_POINT:
        return cf_write_uint32(value.obj, file) && cf_write_int32(value.point.index, file);
    default:
        break;
    }
    keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", "script_types.cpp", 484);
    return false;
}

// 0x00B2D460 (:564)
void script_variant_load(cfile* file, script_variant& value, script_node_type type)
{
    switch (type)
    {
    case SN_NUMBER:
    case SN_BOOL:
        value.number = cf_read_float(file);
        return;
    case SN_STRING:
    {
        char text[1024];
        cf_read_string(text, sizeof(text), file);
        value.string = script_alloc_string(text);
        return;
    }
    case SN_OBJECT:
    case SN_GAME_HANDLE:
        value.obj = cf_read_uint32(file);
        return;
    case SN_OBJECT_LIST:
    {
        const unsigned int count = cf_read_uint32(file);
        value.obj_ptr = (unsigned int*)Script_mempool.alloc(count * 4 + 4, 0);
        if (value.obj_ptr != nullptr)
        {
            for (unsigned int i = 0; i < count; i++)
            {
                value.obj_ptr[i] = cf_read_uint32(file);
            }
            value.obj_ptr[count] = 0xFFFFFFFF;
        }
        return;
    }
    case SN_POINT:
        value.obj = cf_read_uint32(file);
        value.point.index = cf_read_int32(file);
        return;
    default:
        break;
    }
    keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", "script_types.cpp", 564);
}

// 0x00B235A0 (:630)
char* script_alloc_string(const char* text)
{
    const size_t length = text != nullptr ? strlen(text) : 0;
    char* copy = (char*)Script_mempool.alloc((unsigned int)length + 1, 0);
    if (copy == nullptr)
    {
        return nullptr;
    }
    if (text == nullptr)
    {
        *copy = '\0';
        return copy;
    }
    strcpy(copy, text);
    return copy;
}

static script_node* script_handle_atom(unsigned int handle)
{
    script_atom* atom = Script_atoms.alloc(0);
    if (atom == nullptr)
    {
        return nullptr;
    }
    new (atom) script_atom((game_handle)handle);
    return atom;
}

// 0x00B39490 (:653)
script_node* script_node_parse(xml_element* elt, action_script* script)
{
    const char* name = elt->name;
    if (v_stricmp(name, Script_node_tags[SN_NUMBER]) == 0)
    {
        script_atom* atom = Script_atoms.alloc(0);
        if (atom == nullptr)
        {
            return nullptr;
        }
        new (atom) script_atom(xml_get_float(elt, 0.0f));
        return atom;
    }
    if (v_stricmp(name, Script_node_tags[SN_STRING]) == 0 || v_stricmp(name, "message") == 0 || v_stricmp(name, "tool_tip") == 0 || v_stricmp(name, "destroyed_message") == 0 ||
        v_stricmp(name, "district") == 0)
    {
        char* text = script_alloc_string(xml_get_string(elt, nullptr));
        if (text == nullptr)
        {
            return nullptr;
        }
        script_atom* atom = Script_atoms.alloc(0);
        if (atom == nullptr)
        {
            return nullptr;
        }
        new (atom) script_atom(text);
        return atom;
    }
    if (v_stricmp(name, Script_node_tags[SN_BOOL]) == 0)
    {
        script_atom* atom = Script_atoms.alloc(0);
        if (atom == nullptr)
        {
            return nullptr;
        }
        new (atom) script_atom(xml_get_bool(elt, false));
        return atom;
    }
    if (v_stricmp(name, Script_node_tags[SN_OBJECT]) == 0)
    {
        unsigned int object_handle = 0xFFFFFFFF;
        sscanf(elt->text, "%x", &object_handle);
        script_atom* atom = Script_atoms.alloc(0);
        if (atom == nullptr)
        {
            return nullptr;
        }
        new (atom) script_atom(object_handle);
        return atom;
    }
    if (v_stricmp(name, Script_node_tags[SN_OBJECT_LIST]) == 0)
    {
        // Grown by one entry per object; the entry being read is one past the realloc'ed size, as in rfg.exe, and the
        // list ends with 0xFFFFFFFF.
        unsigned int* list = (unsigned int*)Script_mempool.alloc(4, 0);
        if (list == nullptr)
        {
            return nullptr;
        }
        int count = 0;
        for (xml_element* child = elt->elements; child != nullptr; child = child->next)
        {
            if (v_stricmp(child->name, Script_node_tags[SN_OBJECT]) == 0)
            {
                sscanf(child->text, "%x", &list[count]);
                count++;
                list = (unsigned int*)Script_mempool.realloc(list, count * 4);
                if (list == nullptr)
                {
                    return nullptr;
                }
            }
        }
        list = (unsigned int*)Script_mempool.realloc(list, count * 4 + 4);
        if (list == nullptr)
        {
            return nullptr;
        }
        list[count] = 0xFFFFFFFF;
        script_atom* atom = Script_atoms.alloc(0);
        if (atom == nullptr)
        {
            return nullptr;
        }
        new (atom) script_atom(list);
        return atom;
    }
    if (v_stricmp(name, Script_node_tags[SN_EVENT]) == 0)
    {
        return script_event_create(elt, script);
    }
    if (v_stricmp(name, Script_node_tags[SN_FUNCTION]) == 0)
    {
        return script_function_create(elt, script);
    }
    if (v_stricmp(name, Script_node_tags[SN_POINT]) == 0)
    {
        script_point point;
        xml_element* object = elt->elements;
        sscanf(object->text, "%x", &point.ref_handle);
        point.index = xml_get_int(object->next, -1);
        script_atom* atom = Script_atoms.alloc(0);
        if (atom == nullptr)
        {
            return nullptr;
        }
        new (atom) script_atom(point);
        return atom;
    }
    if (v_stricmp(name, Script_node_tags[SN_VARIABLE_REF]) == 0)
    {
        const int index = get_variable_index(xml_get_string(elt, nullptr));
        script_variable* variable = Script_variables.alloc(0);
        if (variable == nullptr)
        {
            return nullptr;
        }
        new (variable) script_variable(index);
        return variable;
    }
    char* text = xml_get_string(elt, nullptr);
    if (v_stricmp(name, "weapon") == 0)
    {
        return script_handle_atom(weapons_get_weapon_info_handle(text));
    }
    if (v_stricmp(name, "vrail_type") == 0)
    {
        return script_handle_atom((unsigned int)vrail_data_get_type(text));
    }
    // Sic: the misspelled element name is rfg.exe's.
    if (v_stricmp(name, "cont_atttack_type") == 0)
    {
        return script_handle_atom((unsigned int)activity_cont_attack_get_index(text));
    }
    if (v_stricmp(name, "radio_message_type") == 0 || v_stricmp(name, "voice_line") == 0)
    {
        return script_handle_atom((unsigned int)voice_line_handle_from_name(text));
    }
    if (v_stricmp(name, "voice_persona") == 0)
    {
        return script_handle_atom((unsigned int)human_voice_persona_handle_from_name(text));
    }
    if (v_stricmp(name, "unlocking_voice_line_group") == 0)
    {
        return script_handle_atom((unsigned int)voice_line_unlocking_get_handle_slow(text));
    }
    if (v_stricmp(name, "team") == 0)
    {
        return script_handle_atom((unsigned int)human_team_from_name(text));
    }
    if (v_stricmp(name, "alert_level") == 0)
    {
        return script_handle_atom((unsigned int)alm_resolve_alert_level(text));
    }
    if (v_stricmp(name, "density_level") == 0)
    {
        return script_handle_atom((unsigned int)amb_spawn_density_from_name(text));
    }
    if (v_stricmp(name, "despawn_type") == 0)
    {
        return script_handle_atom((unsigned int)npc_get_despawn_type(text));
    }
    if (v_stricmp(name, "anim_action") == 0)
    {
        return script_handle_atom((unsigned int)vanim_lookup_action(text));
    }
    if (v_stricmp(name, "vehicle_type") == 0)
    {
        return script_handle_atom((unsigned int)vehicle_get_type(text));
    }
    if (v_stricmp(name, "human_type") == 0)
    {
        return script_handle_atom((unsigned int)human_info_id_find(text));
    }
    if (v_stricmp(name, "effect_type") == 0)
    {
        if (text == nullptr)
        {
            return nullptr;
        }
        const unsigned int handle = effects_get_handle(text);
        // Magic number: the invalid handle.
        if (handle == 0xFFFFFFFF)
        {
            return nullptr;
        }
        return script_handle_atom(handle);
    }
    if (v_stricmp(name, "explosion") == 0)
    {
        return script_handle_atom(explosion_get_handle(text));
    }
    if (v_stricmp(name, "squad") == 0)
    {
        return script_handle_atom((unsigned int)squad_definition_index_from_name(text));
    }
    if (v_stricmp(name, "convoy") == 0)
    {
        return script_handle_atom((unsigned int)convoy_definition_get_index_from_name(text));
    }
    if (v_stricmp(name, "raid_state") == 0)
    {
        return script_handle_atom((unsigned int)raid_common_resolve_state(text));
    }
    if (v_stricmp(name, "activity_state") == 0)
    {
        return script_handle_atom((unsigned int)activity_state_from_name(text));
    }
    if (v_stricmp(name, "air_bomb") == 0)
    {
        return script_handle_atom((unsigned int)air_bomb_definition_get_index_from_name(text));
    }
    if (v_stricmp(name, "objective") == 0 || v_stricmp(name, "mission_objective") == 0)
    {
        return script_handle_atom((unsigned int)objective_get_index_from_name(text));
    }
    if (v_stricmp(name, "foley_type") == 0)
    {
        int cue_id = 0;
        audiolib_cue_get_id(text, &cue_id);
        script_atom* atom = Script_atoms.alloc(0);
        if (atom == nullptr)
        {
            return nullptr;
        }
        new (atom) script_atom((game_handle)0);
        // Sic: rfg.exe stores the id converted to float in this game handle.
        atom->value.number = (float)cue_id;
        return atom;
    }
    if (v_stricmp(name, "music_threshold") == 0)
    {
        return script_handle_atom((unsigned int)game_music_get_index_from_name(text));
    }
    if (v_stricmp(name, "upgrades") == 0)
    {
        return script_handle_atom(upgrades_get_handle(localization_generate_hash_from_key(text)));
    }
    if (v_stricmp(name, "pa_group") == 0)
    {
        return script_handle_atom((unsigned int)public_address_find_group(text));
    }
    if (v_stricmp(name, "mission_handle") == 0)
    {
        return script_handle_atom((unsigned int)mission_handle_find(text));
    }
    keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", "script_types.cpp", 653);
    return nullptr;
}

// 0x00B39D40 (:1001)
bool script_arg_list_parse(script_node** dest, script_arg_list* args, xml_element* elt, action_script* script)
{
    int index = 0;
    for (xml_element* child = elt->elements; child != nullptr; child = child->next)
    {
        script_node* node = script_node_parse(child, script);
        dest[index] = node;
        if (node == nullptr)
        {
            return false;
        }
        // A negative count marks a variable argument list: the last type repeats.
        const int count = args->count < 0 ? -args->count : args->count;
        const script_node_type wanted = args->type[index < count ? index : count - 1];
        if (!script_types_match(wanted, node->type))
        {
            return false;
        }
        index++;
    }
    dest[index] = nullptr;
    return true;
}

// 0x00B235F0 (:1042)
script_node** script_param_alloc(int num_params)
{
    script_node** params = (script_node**)Script_mempool.alloc(num_params * 4, 4);
    memset(params, 0, num_params * 4);
    return params;
}

// 0x00B23620 (:1053)
bool script_params_save(cfile* file, script_node**& params)
{
    if (file == nullptr || params == nullptr)
    {
        return false;
    }
    unsigned int count = 0;
    const unsigned int count_pos = cf_tell(file);
    if (!cf_write_uint32(0, file))
    {
        return false;
    }
    for (; params[count] != nullptr; count++)
    {
        if (!params[count]->save(file))
        {
            return false;
        }
    }
    const unsigned int end_pos = cf_tell(file);
    return cf_seek(file, count_pos, CF_SEEK_SET) && cf_write_uint32(count, file) && cf_seek(file, end_pos, CF_SEEK_SET);
}

// 0x00B32E70 (:1094)
void script_node_load(script_node*& node, cfile* file)
{
    // The class is read ahead of the node's own load.
    const unsigned int pos = cf_tell(file);
    const unsigned int node_class = cf_read_uint32(file);
    cf_seek(file, pos, CF_SEEK_SET);
    switch (node_class)
    {
    case SNC_ATOM:
        node = Script_atoms.alloc(0);
        new (node) script_atom();
        break;
    case SNC_EVENT:
        node = Script_events.alloc(0);
        break;
    case SNC_FUNCTION:
        node = Script_functions.alloc(0);
        break;
    case SNC_VARIABLE:
        node = Script_variables.alloc(0);
        new (node) script_variable();
        break;
    default:
        keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", "script_types.cpp", 1094);
        return;
    }
    node->load(file);
}

// 0x00B32FB0 (:1130)
void script_params_load(script_node**& params, cfile* file)
{
    const unsigned int count = cf_read_uint32(file);
    const unsigned int size = count * 4 + 4;
    params = (script_node**)Script_mempool.alloc(size, 4);
    memset(params, 0, size);
    for (unsigned int i = 0; i < count; i++)
    {
        script_node_load(params[i], file);
    }
}

// 0x00B236D0 (:1149)
void scripting_clear_action_return_value()
{
    Action_return_value.clear();
}

// 0x00B236F0 (:1157)
void scripting_set_action_return_value(unsigned int return_value)
{
    Action_return_value.set(return_value);
}

// 0x00B23710 (:1165)
void scripting_get_action_return_value(unsigned int& return_value)
{
    return_value = Action_return_value.handle;
}

// 0x00B23720 (:1173)
bool scripting_action_return_value_valid()
{
    return Action_return_value.valid;
}

// 0x00B23730 (:1181)
void script_mempool_clear()
{
    Script_mempool.clear();
}

// 0x00B3D6A0 (:1189)
void script_atoms_clear_pool()
{
    Script_atoms.clear();
}

// 0x00B3D6F0 (:1197)
void script_variables_clear()
{
    Script_variables.clear();
    for (int i = 0; i < 128; i++)
    {
        Script_variable_values[i].clear();
    }
    Variable_idx = 0;
}

// 0x00B23740 (:1211)
bool script_variable_is_set(int idx)
{
    return Script_variable_values[idx].valid;
}

// 0x00B23750 (:1219)
unsigned int script_variable_get(int idx)
{
    // Magic number: the size of Script_variable_values.
    if ((unsigned int)idx < 128 && Script_variable_values[idx].valid)
    {
        return Script_variable_values[idx].handle;
    }
    return 0xFFFFFFFF;
}

// 0x00B23770 (:1232)
void script_variable_set(int idx, unsigned int variable_value)
{
    Script_variable_values[idx].set(variable_value);
}

// 0x00B23790 (:1240)
void script_variable_clear(script_node* pnode)
{
    Script_variable_values[((script_variable*)pnode)->idx].clear();
}

// 0x00B237B0 (:1248)
bool script_variables_save(cfile* file)
{
    if (!cf_write_int32(Variable_idx, file))
    {
        return false;
    }
    for (int i = 0; i < Variable_idx; i++)
    {
        if (!Script_variable_values[i].save(file))
        {
            return false;
        }
    }
    return true;
}

// 0x00B23810 (:1268)
void script_variables_load(cfile* file)
{
    Variable_idx = cf_read_int32(file);
    for (int i = 0; i < Variable_idx; i++)
    {
        Script_variable_values[i].load(file);
    }
}

// 0x00B1F580 (.h:302)
script_arg_list::script_arg_list(int _count, ...)
{
    const int total = _count < 0 ? -_count : _count;
    count = 0;
    va_list args;
    va_start(args, _count);
    for (; count < total; count++)
    {
        type[count] = va_arg(args, script_node_type);
    }
    va_end(args);
    count = _count;
}
