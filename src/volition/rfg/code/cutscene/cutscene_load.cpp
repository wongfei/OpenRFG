// Original: volition/rfg/code/cutscene/cutscene_load.cpp
//
// Loading a real-time cutscene: the list in cutscene.xtbl, then for the one playing its own table (navpoints, times of
// day, characters, vehicles, shots), its camera script, its animations and its effects. Everything goes in
// Cutscene_mempool, which cutscene_stop clears as a whole, so nothing here is freed one by one.

#include "volition/rfg/code/cutscene/cutscene_load.h"

#include "volition/anim_lib/code/anim_lib.h"
#include "volition/anim_lib/code/anim_lib_internal.h"
#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/rfg/code/camera/camera_cutscene.h"
#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/rfg/code/cutscene/cutscene_internal.h"
#include "volition/rfg/code/effects/effects.h"
#include "volition/rfg/code/effects/effects_shared.h"
#include "volition/rfg/code/geom/vanim.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human_info.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/rfg.h"
#include "volition/rfg/code/vehicle/vehicle_class.h"
#include "volition/rfg/code/vehicle/vehicle_system.h"
#include "volition/stream2/code/stream2.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/memory/rfg_split_mempool.h"
#include "volition/vlib/code/memory/virtual_mempool.h"
#include "volition/vlib/code/util/string_util.h"

#include "keen/base/trace.hpp"

#include <stdarg.h>

// ---- globals
cutscene_navpoint Cutscene_navpoints[2]; // 0x01E2B618
int Num_cutscene_navpoints; // 0x01E2B3F4
cutscene_npc Cutscene_npcs[12]; // 0x01E2B700
int Num_cutscene_npcs; // 0x01E2B3F8
cutscene_vehicle Cutscene_vehicles[6]; // 0x01E2B4C8
int Num_cutscene_vehicles; // 0x01E2B440

// 0x00705FC0 (:256)
// Empty in rfg.exe: a retail build drops the message. Global there; file-local here, so that it cannot meet another
// Errorf at link time.
static void cs_errorf(const char*, ...)
{
}

// 0x00705FD0 (:261)
float deg2rad(const float degrees)
{
    // Magic number: pi rounded to single precision, as a double, as rfg.exe's constant is.
    return (float)((double)degrees * 3.1415927410125732 / 180.0);
}

// 0x00705FF0 (:293)
void cs_base_sequence::parse_duration(xml_element* sequence_xml, unsigned int& total_shot_duration)
{
    unsigned int duration;
    xml_table_get_req_uint(duration, sequence_xml, "Duration");
    shot_start_frame = total_shot_duration;
    shot_end_frame = duration + total_shot_duration;
    total_shot_duration += duration;
}

// 0x00706030 (:309)
void cs_base_sequence::parse_duration_overlap(xml_element* sequence_xml, unsigned int& total_shot_duration, cs_base_sequence* prev_seq_element)
{
    parse_duration(sequence_xml, total_shot_duration);
    unsigned int overlap;
    xml_table_get_req_uint(overlap, sequence_xml, "OverlapPrevious");
    ramp_in = overlap;
    if (prev_seq_element != nullptr)
    {
        prev_seq_element->ramp_out = overlap;
    }
    shot_start_frame -= overlap;
    shot_end_frame -= overlap;
    total_shot_duration -= overlap;
}

// 0x007060A0 (:338)
void cs_base_sequence::set_duration_overlap(const unsigned int duration, const unsigned int overlap, unsigned int& total_shot_duration, cs_base_sequence* prev_seq_element)
{
    ramp_in = overlap;
    if (prev_seq_element != nullptr)
    {
        prev_seq_element->ramp_out = overlap;
    }
    shot_start_frame = total_shot_duration - overlap;
    shot_end_frame = total_shot_duration - overlap + duration;
    total_shot_duration += duration - overlap;
}

// 0x007097B0 (:367)
const cs_camera_index_info* cs_find_camera_info_by_name(const base_array<cs_camera_index_info>& cs_camera_index, const char* const camera_name)
{
    for (int i = 0; i < cs_camera_index.size(); i++)
    {
        if (v_stricmp(camera_name, cs_camera_index[i].name) == 0)
        {
            return &cs_camera_index[i];
        }
    }
    cs_errorf("Cutscene Load: camera '%s' was not found.", camera_name);
    return nullptr;
}

// A copy of a file name without its path, in the cutscene pool. rfg.exe writes this out each time.
static const char* cs_copy_file_name(const char* const path)
{
    char file_name[256];
    cf_drop_path(file_name, 0xFF, path);
    return str_new_copy(file_name, Cutscene_mempool);
}

// 0x007060E0 (:385)
// LTCG: table_xml in EDI.
static void cs_parse_camera_and_lightset_names(cutscene_current_info* const info, xml_element* table_xml)
{
    const char* const camera_script = xml_table_get_opt_string_ref(table_xml, "CameraScript");
    info->camera_script_filename = camera_script != nullptr ? cs_copy_file_name(camera_script) : nullptr;
    info->m_light_set_name = nullptr;
    const char* const light_set = xml_table_get_opt_string_ref(table_xml, "SceneLightset");
    if (light_set != nullptr)
    {
        info->m_light_set_name = cs_copy_file_name(light_set);
    }
}

// 0x00706210 (:409)
cutscene_navpoint* const cutscene_navpoint_find(const char* const name)
{
    for (int i = 0; i < Num_cutscene_navpoints; i++)
    {
        if (v_stricmp(Cutscene_navpoints[i].name, name) == 0)
        {
            return &Cutscene_navpoints[i];
        }
    }
    return nullptr;
}

// 0x00706260 (:428)
// LTCG: cutscene_xml in EDI. cur_cutscene is unused: rfg.exe goes through Current_cutscene, with the calls to
// cutscene_set_no_ai_bbox inlined.
static void cs_parse_cutscene(xml_element* const cutscene_xml, cutscene_current_info* const cur_cutscene)
{
    const cutscene_navpoint* const start_nav = cutscene_navpoint_find(xml_table_get_req_string_ref(cutscene_xml, "StartNav"));
    if (start_nav == nullptr)
    {
        cutscene_set_no_ai_bbox(ZERO_VECTOR, ZERO_VECTOR);
        keen::debug::nativePrintArgs("Invalid navpoint specified.\n");
        return;
    }
    if (xml_table_find_opt(cutscene_xml, "BBoxMin") == nullptr)
    {
        cutscene_set_no_ai_bbox(ZERO_VECTOR, ZERO_VECTOR);
        return;
    }
    vector bbox_min;
    vector bbox_max;
    xml_table_get_req_vector(bbox_min, cutscene_xml, "BBoxMin");
    xml_table_get_req_vector(bbox_max, cutscene_xml, "BBoxMax");
    const vector world_min(start_nav->pos.x + bbox_min.x, start_nav->pos.y + bbox_min.y, start_nav->pos.z + bbox_min.z);
    const vector world_max(start_nav->pos.x + bbox_max.x, start_nav->pos.y + bbox_max.y, start_nav->pos.z + bbox_max.z);
    cutscene_set_no_ai_bbox(world_min, world_max);
}

// 0x00706420 (:454)
// LTCG: orient in EDI.
static cutscene_navpoint* cutscene_navpoint_create(const char* name, const vector& pos, const matrix& orient)
{
    // Magic number: the size of Cutscene_navpoints.
    if (Num_cutscene_navpoints == 2)
    {
        return nullptr;
    }
    cutscene_navpoint& navpoint = Cutscene_navpoints[Num_cutscene_navpoints];
    navpoint.name = str_new_copy(name, Cutscene_mempool);
    navpoint.pos = pos;
    navpoint.orient = orient;
    Num_cutscene_navpoints++;
    return &navpoint;
}

// 0x0070E3E0 (:471)
// LTCG: orient in EDI.
static cutscene_npc* cutscene_npc_create(const char* name, const human_info* hinfo, const vector& pos, const matrix& orient)
{
    // Magic number: the size of Cutscene_npcs.
    if (Num_cutscene_npcs == 12)
    {
        return nullptr;
    }
    cutscene_npc& created = Cutscene_npcs[Num_cutscene_npcs];
    created.name = str_new_copy(name, Cutscene_mempool);
    created.pos = pos;
    created.orient = orient;
    created.handle = 0xFFFFFFFF;
    const npc* const spawned = cutscene_spawn_npc(hinfo, pos, orient);
    if (spawned != nullptr)
    {
        created.handle = spawned->get_handle();
    }
    Num_cutscene_npcs++;
    return &created;
}

// 0x007064A0 (:495)
// LTCG: orient in EDI.
static cutscene_vehicle* cutscene_vehicle_create(const char* name, const vehicle_info* vinfo, vector& pos, const matrix& orient)
{
    // Magic number: the size of Cutscene_vehicles.
    if (Num_cutscene_vehicles == 6)
    {
        return nullptr;
    }
    cutscene_vehicle& created = Cutscene_vehicles[Num_cutscene_vehicles];
    created.name = str_new_copy(name, Cutscene_mempool);
    created.pos = pos;
    created.orient = orient;
    created.handle = 0xFFFFFFFF;
    // Magic number: spawned 50 m up, out of the way until a shot places it. This writes to the caller's vector, as in
    // rfg.exe.
    pos.y = (float)((double)pos.y + 50.0);
    vehicle* const spawned = cutscene_spawn_vehicle(vinfo, pos, orient);
    if (spawned != nullptr)
    {
        spawned->obj_flags.no_save = 1;
        spawned->obj_flags.destroy_on_stream = 1;
        created.handle = spawned->get_handle();
    }
    Num_cutscene_vehicles++;
    return &created;
}

// 0x00706560 (:529)
static cutscene_navpoint* cs_parse_navpoint(xml_element* const navpoint_xml)
{
    const char* const name = xml_table_get_req_string_ref(navpoint_xml, "Name");
    vector pos;
    xml_table_get_req_vector(pos, navpoint_xml, "Pos");
    matrix orient;
    orient.make_identity();
    if (xml_find(navpoint_xml, "FiddlePosition") != nullptr)
    {
        // Original quirk kept: the fiddle position is read and dropped, and the orient is not converted from degrees
        // as every other OrientY and FiddleOrient here is.
        vector fiddle_pos;
        float fiddle_orient;
        xml_table_get_req_vector(fiddle_pos, navpoint_xml, "FiddlePosition");
        xml_table_get_req_float(fiddle_orient, navpoint_xml, "FiddleOrient");
        orient.rotate_around_local_y(fiddle_orient);
    }
    return cutscene_navpoint_create(name, pos, orient);
}

// 0x007065F0 (:560)
// LTCG: table_xml in EAX. rfg.exe inlines cs_parse_navpoint here.
static void cs_parse_navpoints(xml_element* const table_xml)
{
    xml_element* const navpoints_xml = xml_table_find_opt(table_xml, "Navpoints");
    for (xml_element* navpoint_xml = xml_table_find_opt(navpoints_xml, "Navpoint"); navpoint_xml != nullptr;
         navpoint_xml = xml_table_find_next(navpoints_xml, navpoint_xml, "Navpoint"))
    {
        cs_parse_navpoint(navpoint_xml);
    }
}

// 0x0070E480 (:582)
// LTCG: cur_cutscene in ECX.
static void cs_parse_time_of_days(xml_element* table_xml, cutscene_current_info* const cur_cutscene)
{
    xml_element* const tods_xml = xml_table_find_opt(table_xml, "TODs");
    cutscene_initialize_array_from_xml(cur_cutscene->time_of_days, tods_xml, "TOD", Cutscene_mempool);
    for (xml_element* tod_xml = xml_table_find_opt(tods_xml, "TOD"); tod_xml != nullptr;
         tod_xml = xml_table_find_next(tods_xml, tod_xml, "TOD"))
    {
        const cs_time_of_day new_tod;
        cs_time_of_day& tod = cur_cutscene->time_of_days[cur_cutscene->time_of_days.add(new_tod)];
        xml_table_get_req_uint(tod.start_time, tod_xml, "StartTime");
        xml_table_get_req_uint(tod.end_time, tod_xml, "EndTime");

        xml_element* const characters_xml = xml_table_find_opt(tod_xml, "Characters");
        cutscene_initialize_array_from_xml(tod.characters, characters_xml, "Character", Cutscene_mempool);
        for (xml_element* character_xml = xml_table_find_opt(characters_xml, "Character"); character_xml != nullptr;
             character_xml = xml_table_find_next(characters_xml, character_xml, "Character"))
        {
            const cs_tod_character new_character = {nullptr, nullptr};
            cs_tod_character& character = tod.characters[tod.characters.add(new_character)];
            const char* const lightset_file = xml_table_get_opt_string_ref(character_xml, "LightsetFile");
            character.lightset_filename = lightset_file != nullptr ? cs_copy_file_name(lightset_file) : nullptr;
        }

        xml_element* const lights_xml = xml_table_find_opt(tod_xml, "Lights");
        cutscene_initialize_array_from_xml(tod.light_names, lights_xml, "Light", Cutscene_mempool);
        for (xml_element* light_xml = xml_table_find_opt(lights_xml, "Light"); light_xml != nullptr;
             light_xml = xml_table_find_next(lights_xml, light_xml, "Light"))
        {
            // Original quirk kept: these point into the table, which cs_read_cutscene_table_for_current closes when it
            // is done. Nothing in rfg.exe reads them.
            if (light_xml->text != nullptr)
            {
                tod.light_names.add(light_xml->text);
            }
        }
    }
}

// 0x0070E790 (:654)
// LTCG: character_xml in EAX. cur_cutscene is unused.
static cutscene_npc* cs_parse_npc_and_create(cutscene_current_info* cur_cutscene, xml_element* const character_xml)
{
    const char* const name = xml_table_get_req_string_ref(character_xml, "Name");
    if (name == nullptr)
    {
        return nullptr;
    }
    const vector slate_pos = cutscene_get_slate_position();
    if (v_stricmp(name, "Player") == 0 || v_stricmp(name, "#Player#") == 0)
    {
        return nullptr;
    }
    const human_info* const hinfo = human_info_find(xml_table_get_req_string_ref(character_xml, "CharMesh"));
    if (hinfo == nullptr)
    {
        keen::debug::nativePrintArgs("Invalid parameters (mesh) for cutscene NPC.\n");
        return nullptr;
    }
    matrix orient;
    orient.make_identity();
    return cutscene_npc_create(name, hinfo, slate_pos, orient);
}

// 0x0070E830 (:686)
// LTCG: table_xml in EAX.
static void cs_parse_characters(xml_element* const table_xml, cutscene_current_info* const cur_cutscene)
{
    xml_element* const characters_xml = xml_table_find_opt(table_xml, "Characters");
    cutscene_initialize_array_from_xml(cur_cutscene->cutscene_npcs, characters_xml, "Character", Cutscene_mempool);
    for (xml_element* character_xml = xml_table_find_opt(characters_xml, "Character"); character_xml != nullptr;
         character_xml = xml_table_find_next(characters_xml, character_xml, "Character"))
    {
        cutscene_npc* const created = cs_parse_npc_and_create(cur_cutscene, character_xml);
        if (created == nullptr)
        {
            continue;
        }
        if (created->handle == 0xFFFFFFFF)
        {
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
            continue;
        }
        cur_cutscene->cutscene_npcs.add(created);
    }
}

// 0x00709880 (:721)
static cutscene_vehicle* cs_parse_vehicle_and_create(xml_element* const vehicle_xml)
{
    const char* const name = xml_table_get_req_string_ref(vehicle_xml, "Name");
    if (name == nullptr)
    {
        return nullptr;
    }
    vector slate_pos = cutscene_get_slate_position();
    const vehicle_info* const vinfo = vehicle_get_info_from_name(xml_table_get_req_string_ref(vehicle_xml, "VehicleMesh"));
    if (vinfo == nullptr)
    {
        keen::debug::nativePrintArgs("Invalid parameters (mesh) for cutscene vehicle.\n");
        return nullptr;
    }
    matrix orient;
    orient.make_identity();
    return cutscene_vehicle_create(name, vinfo, slate_pos, orient);
}

// 0x0070CFD0 (:748)
// LTCG: table_xml in EAX. rfg.exe inlines cs_parse_vehicle_and_create here.
static void cs_parse_vehicles(xml_element* const table_xml, cutscene_current_info* const cur_cutscene)
{
    xml_element* const vehicles_xml = xml_table_find_opt(table_xml, "Vehicles");
    cutscene_initialize_array_from_xml(cur_cutscene->cutscene_vehicles, vehicles_xml, "Vehicle", Cutscene_mempool);
    for (xml_element* vehicle_xml = xml_table_find_opt(vehicles_xml, "Vehicle"); vehicle_xml != nullptr;
         vehicle_xml = xml_table_find_next(vehicles_xml, vehicle_xml, "Vehicle"))
    {
        cutscene_vehicle* const created = cs_parse_vehicle_and_create(vehicle_xml);
        if (created == nullptr || created->handle == 0xFFFFFFFF)
        {
            keen::debug::nativePrintArgs("Int3 critical function at %s(%d)\n", __FILE__, __LINE__);
            continue;
        }
        cur_cutscene->cutscene_vehicles.add(created);
    }
}

// 0x0070E8E0 (:776)
// LTCG: shot_xml in EAX.
static void cs_parse_shot_lightset_info(xml_element* const shot_xml, base_array<cs_shot_tod>& tods)
{
    xml_element* const tods_xml = xml_table_find_opt(shot_xml, "TODs");
    cutscene_initialize_array_from_xml(tods, tods_xml, "TOD", Cutscene_mempool);
    for (xml_element* tod_xml = xml_table_find_opt(tods_xml, "TOD"); tod_xml != nullptr;
         tod_xml = xml_table_find_next(tods_xml, tod_xml, "TOD"))
    {
        const cs_shot_tod new_tod;
        tods.add(new_tod);
        cs_shot_tod& tod = tods[tods.size() - 1];
        xml_element* const lights_xml = xml_table_find_opt(tod_xml, "Lights");
        cutscene_initialize_array_from_xml(tod.lights_enabled, lights_xml, "Light", Cutscene_mempool);
        for (xml_element* light_xml = xml_table_find_opt(lights_xml, "Light"); light_xml != nullptr;
             light_xml = xml_table_find_next(lights_xml, light_xml, "Light"))
        {
            int enabled;
            // A null name reads the element's own text; the characters' list has a LightEnabled element instead.
            xml_table_get_req_int(enabled, light_xml, nullptr);
            tod.lights_enabled.add(enabled == 1);
        }
    }
}

// 0x0070D1A0 (:804)
void cs_parse_shot_character_animations(cutscene_shot& shot, cs_shot_actor& character, xml_element* character_xml)
{
    unsigned int total_duration = 0;
    xml_element* const animations_xml = xml_table_find_opt(character_xml, "Animations");
    cutscene_initialize_array_from_xml(character.m_animations, animations_xml, "Animation", Cutscene_mempool);
    cs_shot_char_animation* previous = nullptr;
    for (xml_element* animation_xml = xml_table_find_opt(animations_xml, "Animation"); animation_xml != nullptr;
         animation_xml = xml_table_find_next(animations_xml, animation_xml, "Animation"))
    {
        const cs_shot_char_animation new_animation;
        cs_shot_char_animation& animation = character.m_animations[character.m_animations.add(new_animation)];
        char no_path[68];
        char no_ext[68];
        cf_drop_path(no_path, 0x40, xml_table_get_req_string_ref(animation_xml, "Filename"));
        cf_drop_ext(no_ext, 0x40, no_path);
        animation.anim_name = str_new_copy(no_ext, Cutscene_mempool);
        xml_table_get_req_float(animation.max_weight, animation_xml, "MaxWeight");
        unsigned int start_frame;
        unsigned int end_frame;
        unsigned int overlap;
        xml_table_get_req_uint(start_frame, animation_xml, "StartFrame");
        xml_table_get_req_uint(end_frame, animation_xml, "EndFrame");
        xml_table_get_req_uint(overlap, animation_xml, "OverlapPrevious");
        animation.start_frame = start_frame;
        animation.end_frame = end_frame;
        // rfg.exe inlines this call.
        animation.set_duration_overlap(end_frame - start_frame, overlap, total_duration, previous);
        previous = &animation;
    }
    if (shot.duration_fm <= total_duration)
    {
        shot.duration_fm = total_duration;
    }
}

// 0x0070D440 (:852)
void cs_parse_shot_character_anim_states(cs_shot_actor& character, xml_element* character_xml)
{
    unsigned int total_duration = 0;
    xml_element* const states_xml = xml_table_find_opt(character_xml, "AnimStates");
    cutscene_initialize_array_from_xml(character.m_animation_states, states_xml, "AnimState", Cutscene_mempool);
    for (xml_element* state_xml = xml_table_find_opt(states_xml, "AnimState"); state_xml != nullptr;
         state_xml = xml_table_find_next(states_xml, state_xml, "AnimState"))
    {
        const cs_shot_char_animation new_state;
        cs_shot_char_animation& state = character.m_animation_states[character.m_animation_states.add(new_state)];
        char no_path[68];
        char no_ext[68];
        cf_drop_path(no_path, 0x40, xml_table_get_req_string_ref(state_xml, "Filename"));
        cf_drop_ext(no_ext, 0x40, no_path);
        state.anim_name = str_new_copy(no_ext, Cutscene_mempool);
        // rfg.exe inlines this call.
        state.parse_duration(state_xml, total_duration);
    }
}

// 0x0070EAA0 (:886)
static void cs_parse_shot_characters(xml_element* const shot_xml, cutscene_shot& shot)
{
    xml_element* const characters_xml = xml_table_find_opt(shot_xml, "Characters");
    cutscene_initialize_array_from_xml(shot.m_characters, characters_xml, "Character", Cutscene_mempool);
    shot.duration_fm = 0;
    for (xml_element* character_xml = xml_table_find_opt(characters_xml, "Character"); character_xml != nullptr;
         character_xml = xml_table_find_next(characters_xml, character_xml, "Character"))
    {
        const cs_shot_actor new_actor;
        cs_shot_actor& actor = shot.m_characters[shot.m_characters.add(new_actor)];
        actor.handle = 0xFFFFFFFF;
        actor.actor_name = str_new_copy(xml_table_get_req_string_ref(character_xml, "Name"), Cutscene_mempool);
        xml_table_get_req_vector(actor.position, character_xml, "Position");
        xml_table_get_req_float(actor.orient_y, character_xml, "OrientY");
        actor.orient_y = deg2rad(actor.orient_y);
        xml_element* const options_xml = xml_table_find_req(character_xml, "Options");
        if (options_xml != nullptr)
        {
            const char* const weapon = xml_table_get_opt_string_ref(options_xml, "Weapon");
            actor.options.weapon_name = weapon != nullptr ? str_new_copy(weapon, Cutscene_mempool) : nullptr;
            const char* const item = xml_table_get_opt_string_ref(options_xml, "Item");
            actor.options.item_name = item != nullptr ? str_new_copy(item, Cutscene_mempool) : nullptr;
            xml_table_get_req_bool(actor.options.fat_bones, options_xml, "FatBones");
            xml_table_get_req_bool(actor.options.hide_char, options_xml, "HideChar");
            xml_table_get_req_bool(actor.options.draw_weapon, options_xml, "DrawWeapon");
            xml_table_get_req_bool(actor.options.holster_weapon, options_xml, "HolsterWeapon");
            xml_table_get_req_bool(actor.options.dont_reposition, options_xml, "DontReposition");
        }
        cs_parse_shot_character_animations(shot, actor, character_xml);
        cs_parse_shot_character_anim_states(actor, character_xml);

        xml_element* const tods_xml = xml_table_find_opt(character_xml, "TODs");
        cutscene_initialize_array_from_xml(actor.time_of_days, tods_xml, "TOD", Cutscene_mempool);
        for (xml_element* tod_xml = xml_table_find_opt(tods_xml, "TOD"); tod_xml != nullptr;
             tod_xml = xml_table_find_next(tods_xml, tod_xml, "TOD"))
        {
            const cs_shot_char_tod new_tod;
            cs_shot_char_tod& tod = actor.time_of_days[actor.time_of_days.add(new_tod)];
            xml_element* const lights_xml = xml_table_find_req(tod_xml, "Lights");
            cutscene_initialize_array_from_xml(tod.lights_enabled, lights_xml, "Light", Cutscene_mempool);
            for (xml_element* light_xml = xml_table_find_opt(lights_xml, "Light"); light_xml != nullptr;
                 light_xml = xml_table_find_next(lights_xml, light_xml, "Light"))
            {
                int enabled;
                xml_table_get_req_int(enabled, light_xml, "LightEnabled");
                tod.lights_enabled.add(enabled == 1);
            }
        }
    }
    unsigned int defined_length = 0;
    xml_table_get_req_uint(defined_length, shot_xml, "DefinedShotLength");
    if (defined_length != 0)
    {
        shot.duration_fm = defined_length;
    }
}

// 0x0070EFF0 (:987)
// LTCG: shot_xml in EAX.
static void cs_parse_shot_vehicles(xml_element* const shot_xml, cutscene_shot& shot)
{
    xml_element* const vehicles_xml = xml_table_find_opt(shot_xml, "Vehicles");
    cutscene_initialize_array_from_xml(shot.m_vehicles, vehicles_xml, "Vehicle", Cutscene_mempool);
    for (xml_element* vehicle_xml = xml_table_find_opt(vehicles_xml, "Vehicle"); vehicle_xml != nullptr;
         vehicle_xml = xml_table_find_next(vehicles_xml, vehicle_xml, "Vehicle"))
    {
        const cs_shot_actor new_actor;
        cs_shot_actor& actor = shot.m_vehicles[shot.m_vehicles.add(new_actor)];
        actor.actor_name = str_new_copy(xml_table_get_req_string_ref(vehicle_xml, "Name"), Cutscene_mempool);
        actor.handle = 0xFFFFFFFF;
        xml_table_get_req_vector(actor.position, vehicle_xml, "Position");
        xml_table_get_req_float(actor.orient_y, vehicle_xml, "OrientY");
        actor.orient_y = deg2rad(actor.orient_y);
        cs_parse_shot_character_animations(shot, actor, vehicle_xml);
        cs_parse_shot_character_anim_states(actor, vehicle_xml);
    }
}

// 0x007067B0 (:1018)
// Original quirk kept: despite the name, a missing element leaves the value as it was. rfg.exe inlines xml_find.
void xml_get_req_value(vector& value, xml_element* root_xml, const char* const name)
{
    xml_element* const found = xml_find(root_xml, name);
    if (found != nullptr)
    {
        xml_table_get_vector(value, found);
    }
}

// 0x007067C0 (:1024)
void xml_get_req_value(float& value, xml_element* root_xml, const char* const name)
{
    char text[1024];
    xml_table_get_req_string(text, sizeof(text), root_xml, name);
    value = parse_float(text, sizeof(text));
}

// 0x007067D0 (:1030)
void xml_get_req_value(cs_depth_of_field& value, xml_element* root_xml, const char* const name)
{
    xml_element* const dof_xml = xml_table_find_req(root_xml, name);
    xml_table_get_req_float(value.max_coc_radius, dof_xml, "MaxCOCRadius");
    xml_table_get_req_float(value.preblur_radius, dof_xml, "PreblurRadius");
    xml_table_get_req_float(value.m_near, dof_xml, "Near");
    xml_table_get_req_float(value.ideal_near, dof_xml, "IdealNear");
    xml_table_get_req_float(value.ideal_far, dof_xml, "IdealFar");
    xml_table_get_req_float(value.m_far, dof_xml, "Far");
}

// <cs_depth_of_field> 0x0070B940 (:1046)
template<typename T>
void xml_get_req_tween(tweener<T>& that, xml_element* parent_xml, const char* name, bool read_when_untweened)
{
    if (name == nullptr)
    {
        return;
    }
    xml_element* const tween_xml = xml_find(parent_xml, name);
    if (tween_xml == nullptr)
    {
        return;
    }
    bool tweened = false;
    xml_table_get_req_bool(tweened, tween_xml, "AmTweened");
    if (!tweened)
    {
        if (read_when_untweened)
        {
            T value;
            xml_get_req_value(value, tween_xml, "Value");
            that.set_values(value);
        }
        return;
    }
    T start_value;
    T end_value;
    unsigned int start_frame;
    unsigned int end_frame;
    xml_get_req_value(start_value, tween_xml, "StartValue");
    xml_get_req_value(end_value, tween_xml, "EndValue");
    xml_table_get_req_uint(start_frame, tween_xml, "StartFrame");
    xml_table_get_req_uint(end_frame, tween_xml, "EndFrame");
    that.set_values(start_value, frames_to_seconds(start_frame), end_value, frames_to_seconds(end_frame));
    that.set_blend_mode(xml_table_get_req_string_ref(tween_xml, "BlendMode"));
}

// 0x0070F1F0 (:1085)
// LTCG: table_xml in EAX. The camera index parameter of the original is unused and LTCG dropped it.
static void cs_parse_shots(xml_element* const table_xml, cutscene_current_info* const cur_cutscene)
{
    xml_element* const shots_xml = xml_table_find_opt(table_xml, "Shots");
    cutscene_initialize_array_from_xml(cur_cutscene->m_shots, shots_xml, "Shot", Cutscene_mempool);
    for (xml_element* shot_xml = xml_table_find_opt(shots_xml, "Shot"); shot_xml != nullptr;
         shot_xml = xml_table_find_next(shots_xml, shot_xml, "Shot"))
    {
        const cutscene_shot new_shot;
        cutscene_shot& shot = cur_cutscene->m_shots[cur_cutscene->m_shots.add(new_shot)];
        shot.fade_out_duration_sec = 0.0f;
        shot.fade_out_letterbox = false;
        shot.slate_name = str_new_copy(xml_table_get_req_string_ref(shot_xml, "SlateName"), Cutscene_mempool);
        if (xml_find(shot_xml, "FiddlePosition") == nullptr)
        {
            shot.slate_fiddle_pos.x = 0.0f;
            shot.slate_fiddle_pos.y = 0.0f;
            shot.slate_fiddle_pos.z = 0.0f;
            shot.slate_fiddle_orient = 0.0f;
        }
        else
        {
            xml_table_get_req_vector(shot.slate_fiddle_pos, shot_xml, "FiddlePosition");
            xml_table_get_req_float(shot.slate_fiddle_orient, shot_xml, "FiddleOrient");
            shot.slate_fiddle_orient = deg2rad(shot.slate_fiddle_orient);
        }
        xml_table_get_req_bool(shot.depth_of_field_enabled, shot_xml, "DOFEnabled");
        xml_get_req_tween(shot.depth_of_field, shot_xml, "DOF", true);
        cs_parse_shot_characters(shot_xml, shot);
        cs_parse_shot_vehicles(shot_xml, shot);
        cutscene_parse_shot_options(shot_xml, shot);
        cs_parse_shot_lightset_info(shot_xml, shot.TODs);

        // Original quirk kept: the action effects count towards the array, but only the CSEffects are read into it.
        xml_element* const indices_xml = xml_table_find_opt(shot_xml, "Indices");
        const int num_action_effects = xml_table_count(xml_table_find_opt(indices_xml, "ActionEffects"), "ActionEffect");
        const int num_cs_effects = xml_table_count(xml_table_find_opt(shot_xml, "CSEffects"), "CSEffect");
        const int num_effects = num_action_effects + num_cs_effects;
        if (num_effects < 1)
        {
            shot.m_effect_infos.init(nullptr, 0, 0);
            continue;
        }
        shot.m_effect_infos.init(new (Cutscene_mempool) cutscene_effect[num_effects], num_effects, 0);
        xml_element* const effects_xml = xml_table_find_opt(shot_xml, "CSEffects");
        for (xml_element* effect_xml = xml_table_find_opt(effects_xml, "CSEffect"); effect_xml != nullptr;
             effect_xml = xml_table_find_next(effects_xml, effect_xml, "CSEffect"))
        {
            cutscene_effect* const effect = shot.m_effect_infos.add_in_place();
            effect->handle = effects_get_handle(xml_table_get_req_string_ref(effect_xml, "Name"));
        }
    }
}

// 0x0070F5B0 (:1169)
// LTCG: cur_cutscene in ESI. The camera index parameter of the original is unused and LTCG dropped it.
static void cs_read_cutscene_table_for_current(cutscene_current_info* const cur_cutscene)
{
    if (cur_cutscene == nullptr || cur_cutscene->cs_info == nullptr)
    {
        return;
    }
    char table_name[256];
    _snprintf_safe(table_name, sizeof(table_name), "%s%s", cur_cutscene->cs_info->name, ".xtbl");
    xml_element* const root = xml_table_parse(table_name);
    if (root == nullptr)
    {
        return;
    }
    xml_element* const cutscene_xml = xml_table_find_req(root, "Cutscene");
    cur_cutscene->bink_movie_handle = 0;
    const char* const soundtrack = xml_table_get_opt_string_ref(cutscene_xml, "Soundtrack");
    cur_cutscene->soundtrack.play_id = -1;
    if (soundtrack != nullptr && soundtrack[0] != '\0')
    {
        audiolib_cue_get_id(soundtrack, &cur_cutscene->soundtrack.cue_id);
    }
    else
    {
        cur_cutscene->soundtrack.cue_id = -1;
    }
    bool restore_player_pos = false;
    xml_table_get_req_bool(restore_player_pos, cutscene_xml, "RestorePlayerPos");
    if (restore_player_pos)
    {
        cur_cutscene->post_cutscene_restore_pos = player_get_local_player()->pos;
        cur_cutscene->post_cutscene_restore_orient = player_get_local_player()->orient;
    }
    cs_parse_camera_and_lightset_names(cur_cutscene, cutscene_xml);
    cs_parse_navpoints(cutscene_xml);
    // Original quirk kept: a table with no navpoints passes an entry that was never filled.
    cutscene_set_offset_and_rotation(Cutscene_navpoints[0], cur_cutscene->m_slate_handle);
    cs_parse_cutscene(cutscene_xml, cur_cutscene);
    cutscene_select_slate(cur_cutscene->m_slate_handle, false);
    camera_set_pos(cutscene_get_slate_position());
    cs_parse_time_of_days(cutscene_xml, cur_cutscene);
    cs_parse_characters(cutscene_xml, cur_cutscene);
    cs_parse_vehicles(cutscene_xml, cur_cutscene);
    cs_parse_shots(cutscene_xml, cur_cutscene);
    xml_table_close(root);
    cur_cutscene->current_tod = nullptr;
    cur_cutscene->current_tod_i = 0;

    int num_effects = 0;
    for (int i = 0; i < cur_cutscene->m_shots.size(); i++)
    {
        num_effects += cur_cutscene->m_shots[i].m_effect_infos.size();
    }
    if (num_effects > 0)
    {
        cur_cutscene->m_effect_object_handles.init(new (Cutscene_mempool) int[num_effects], num_effects, 0);
    }
    else
    {
        cur_cutscene->m_effect_object_handles.init(nullptr, 0, 0);
    }
}

// 0x00706890 (:1340)
int cs_load_custom_animation(const char* filename, const char* morphset, bool& was_just_loaded)
{
    was_just_loaded = false;
    char base_name[68];
    char anim_name[68];
    char anim_pc_name[68];
    cf_drop_path(base_name, 0x40, filename);
    cf_drop_ext(base_name, 0x40, base_name);
    cf_add_ext(anim_name, 0x40, base_name, ".anim", false);
    cf_add_ext(anim_pc_name, 0x40, base_name, ".anim_pc", false);
    int anim_handle = animlib_find_anim(anim_name);
    if (anim_handle == -1)
    {
        anim_handle = animlib_get_anim_slot(str_new_copy(anim_name, Cutscene_mempool));
    }
    else if (animlib_is_paged_in(anim_handle))
    {
        return anim_handle;
    }
    cfile* const file = cf_open(anim_pc_name, "rb", PLATFORM_PC, true);
    if (file == nullptr)
    {
        // Deviation: a stack buffer where rfg.exe prints to its thread-local one.
        char message[256];
        _snprintf_safe(message, sizeof(message), "Failed opening file %s", anim_pc_name);
        keen::debug::nativePrintArgs(message);
        return -1;
    }
    const unsigned int size = cf_size(file);
    void* const data = Cutscene_mempool.alloc(size, 0x10);
    cf_read(data, size, file, false);
    cf_close(file);
    animlib_overlay_anim(anim_handle, data, size, false);
    if (morphset != nullptr)
    {
        animlib_set_morph_set_for_animation(anim_handle, morphset);
    }
    was_just_loaded = true;
    return anim_handle;
}

// 0x00706A50 (:1429)
void cs_unload_animation(const char* const anim_name)
{
    char base_name[68];
    char file_name[68];
    cf_drop_path(base_name, 0x40, anim_name);
    cf_drop_ext(base_name, 0x40, base_name);
    cf_add_ext(file_name, 0x40, base_name, ".anim", false);
    animlib_unload_anim(file_name);
}

// 0x0070D690 (:1459)
void cutscene_read_cutscene_list(base_array<cutscene_info>& cutscene_infos, const bool refresh)
{
    if (refresh)
    {
        return;
    }
    xml_element* const list_xml = xml_table_parse("cutscene.xtbl");
    cutscene_initialize_array_from_xml(cutscene_infos, list_xml, "Cutscene", Permanent_mempool);
    for (xml_element* cutscene_xml = xml_table_find_opt(list_xml, "Cutscene"); cutscene_xml != nullptr;
         cutscene_xml = xml_table_find_next(list_xml, cutscene_xml, "Cutscene"))
    {
        cutscene_info info;
        info.name = str_new_copy(xml_table_get_req_string_ref(cutscene_xml, "Name"), Permanent_mempool);
        cutscene_infos.add(info);
    }
    xml_table_close(list_xml);
}

// The animations and animation states of the actors of one shot's list.
static void cs_load_actor_animations(base_array<cs_shot_actor>& actors)
{
    for (int i = 0; i < actors.size(); i++)
    {
        cs_shot_actor& actor = actors[i];
        for (int j = 0; j < actor.m_animations.size(); j++)
        {
            cs_shot_char_animation& animation = actor.m_animations[j];
            animation.anim_handle = cs_load_custom_animation(animation.anim_name, nullptr, animation.cs_loaded);
        }
        for (int j = 0; j < actor.m_animation_states.size(); j++)
        {
            cs_shot_char_animation& state = actor.m_animation_states[j];
            state.anim_handle = cs_load_custom_animation(state.anim_name, nullptr, state.cs_loaded);
        }
    }
}

// 0x0070F7C0 (:1515)
// rfg.exe writes the loop of cs_load_actor_animations out twice.
void cutscene_load_current_data(cutscene_current_info* const cur_cutscene)
{
    cs_read_cutscene_table_for_current(cur_cutscene);
    if (cur_cutscene->camera_script_filename != nullptr)
    {
        camera_cutscene_load_scene(cur_cutscene->camera_script_filename, &Cutscene_mempool);
    }
    vanim_mark_anim_string_pool();
    for (int i = 0; i < cur_cutscene->m_shots.size(); i++)
    {
        cutscene_shot& shot = cur_cutscene->m_shots[i];
        cs_load_actor_animations(shot.m_characters);
        cs_load_actor_animations(shot.m_vehicles);
    }
    cutscene_load_effects(cur_cutscene);
}

static void cs_unload_animation_list(const base_array<cs_shot_char_animation>& animations)
{
    for (int i = 0; i < animations.size(); i++)
    {
        if (animations[i].cs_loaded)
        {
            // rfg.exe inlines this call.
            cs_unload_animation(animations[i].anim_name);
        }
    }
}

static void cs_unload_actor_animations(const base_array<cs_shot_actor>& actors)
{
    for (int i = 0; i < actors.size(); i++)
    {
        cs_unload_animation_list(actors[i].m_animations);
        cs_unload_animation_list(actors[i].m_animation_states);
    }
}

// 0x00709AC0 (:1574)
void cutscene_unload_data(const cutscene_current_info* const cur_cutscene)
{
    for (int i = 0; i < cur_cutscene->m_shots.size(); i++)
    {
        const cutscene_shot& shot = cur_cutscene->m_shots[i];
        cs_unload_actor_animations(shot.m_characters);
        cs_unload_actor_animations(shot.m_vehicles);
    }
    vanim_restore_anim_string_pool_to_mark();
    camera_stop_cutscene_mode();
}

// 0x00709DF0 (:1614)
void cutscene_load_effects(cutscene_current_info* const cur_cutscene)
{
    for (int i = 0; i < cur_cutscene->m_shots.size(); i++)
    {
        base_array<cutscene_effect>& effects = cur_cutscene->m_shots[i].m_effect_infos;
        for (int j = 0; j < effects.size(); j++)
        {
            cutscene_effect& effect = effects[j];
            if (effect.handle == 0xFFFFFFFF)
            {
                continue;
            }
            const visual_effect_info* const info = effects_get_visual_effect_info(Effect_info[effect.handle].visual_handle);
            // Magic number: flag 2 is an effect kept loaded, which needs no preload.
            if ((info->flags & 2) == 0)
            {
                stream2_preload(info->srid);
                effect.srid = info->srid;
                effect.preloaded = false;
            }
            else
            {
                effect.preloaded = true;
            }
        }
    }
}
