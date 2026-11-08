// Original: volition/rfg/code/cutscene/cutscene_load.h
// Implementation: volition/rfg/code/cutscene/cutscene_load.cpp
//
// Loading cutscenes: the cutscene list, the current cutscene's XML (navpoints, times of day, characters, vehicles,
// shots with their cameras, light sets, character animations and states, options, tweens), custom animations and
// effects. cutscene_initialize_array_from_xml<T> (this header) reads an XML list into an array of T; xml_get_req_tween<T>
// reads a tween (its instantiations are listed below the declarations).
#pragma once

#include "volition/rfg/code/cutscene/cutscene_tween.h"
#include "volition/vlib/code/memory/mempool_base.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/vlib/code/util/varray.h"

struct cs_camera_index_info;
struct cs_depth_of_field;
struct cs_shot_actor;
struct cutscene_current_info;
struct cutscene_info;
struct cutscene_navpoint;
struct cutscene_shot;
class mempool_base;
class vector;
struct xml_element;

// (.h:87) The instantiations are listed below. An array of as many elements as the list has, none added yet.
template<typename T>
void cutscene_initialize_array_from_xml(base_array<T>& _array, xml_element* const list_root_xml, const char* const list_item_name, mempool_base& pool)
{
    const int count = xml_table_count(list_root_xml, list_item_name);
    if (count > 0)
    {
        _array.init(new (pool) T[count], count, 0);
    }
    else
    {
        _array.init(nullptr, 0, 0);
    }
}

template<typename T>
void xml_get_req_tween(tweener<T>& that, xml_element* parent_xml, const char* name, bool read_when_untweened); // (:1046)

// template instance: void cutscene_initialize_array_from_xml<cs_time_of_day>(base_array<cs_time_of_day>& _array, xml_element* const list_root_xml, const char* const list_item_name, mempool_base& pool); // 0x0070B340 (.h:87)
// template instance: void cutscene_initialize_array_from_xml<cutscene_npc*>(base_array<cutscene_npc*>& _array, xml_element* const list_root_xml, const char* const list_item_name, mempool_base& pool); // 0x00707FD0 (.h:87)
// template instance: void cutscene_initialize_array_from_xml<cs_shot_tod>(base_array<cs_shot_tod>& _array, xml_element* const list_root_xml, const char* const list_item_name, mempool_base& pool); // 0x0070B550 (.h:87)
// template instance: void cutscene_initialize_array_from_xml<cs_shot_char_animation>(base_array<cs_shot_char_animation>& _array, xml_element* const list_root_xml, const char* const list_item_name, mempool_base& pool); // 0x0070B6B0 (.h:87)
// template instance: void cutscene_initialize_array_from_xml<char const*>(base_array<char const*>& _array, xml_element* const list_root_xml, const char* const list_item_name, mempool_base& pool); // 0x0070B4B0 (.h:87)
// template instance: void cutscene_initialize_array_from_xml<cs_shot_option>(base_array<cs_shot_option>& _array, xml_element* const list_root_xml, const char* const list_item_name, mempool_base& pool); // 0x0070BB40 (.h:87)
// template instance: void cutscene_initialize_array_from_xml<cutscene_shot>(base_array<cutscene_shot>& _array, xml_element* const list_root_xml, const char* const list_item_name, mempool_base& pool); // 0x0070DDF0 (.h:87)
// template instance: void cutscene_initialize_array_from_xml<cs_shot_actor>(base_array<cs_shot_actor>& _array, xml_element* const list_root_xml, const char* const list_item_name, mempool_base& pool); // 0x0070B790 (.h:87)
// template instance: void cutscene_initialize_array_from_xml<cs_shot_char_tod>(base_array<cs_shot_char_tod>& _array, xml_element* const list_root_xml, const char* const list_item_name, mempool_base& pool); // 0x0070B870 (.h:87)
// template instance: void cutscene_initialize_array_from_xml<cs_tod_character>(base_array<cs_tod_character>& _array, xml_element* const list_root_xml, const char* const list_item_name, mempool_base& pool); // 0x0070B410 (.h:87)
// template instance: void cutscene_initialize_array_from_xml<cutscene_info>(base_array<cutscene_info>& _array, xml_element* const list_root_xml, const char* const list_item_name, mempool_base& pool); // 0x0070BAA0 (.h:87)
// template instance: void cutscene_initialize_array_from_xml<cutscene_vehicle*>(base_array<cutscene_vehicle*>& _array, xml_element* const list_root_xml, const char* const list_item_name, mempool_base& pool); // 0x00708070 (.h:87)
// template instance: void cutscene_initialize_array_from_xml<bool>(base_array<bool>& _array, xml_element* const list_root_xml, const char* const list_item_name, mempool_base& pool); // 0x0070B620 (.h:87)
float deg2rad(const float degrees); // 0x00705FD0 (:261)
const cs_camera_index_info* cs_find_camera_info_by_name(const base_array<cs_camera_index_info>& cs_camera_index, const char* const camera_name); // 0x007097B0 (:367)
cutscene_navpoint* const cutscene_navpoint_find(const char* const name); // 0x00706210 (:409)
void cs_parse_shot_character_animations(cutscene_shot& shot, cs_shot_actor& character, xml_element* character_xml); // 0x0070D1A0 (:804)
void cs_parse_shot_character_anim_states(cs_shot_actor& character, xml_element* character_xml); // 0x0070D440 (:852)
void xml_get_req_value(vector& value, xml_element* root_xml, const char* const name); // 0x007067B0 (:1018)
void xml_get_req_value(float& value, xml_element* root_xml, const char* const name); // 0x007067C0 (:1024)
void xml_get_req_value(cs_depth_of_field& value, xml_element* root_xml, const char* const name); // 0x007067D0 (:1030)
// template instance: void xml_get_req_tween<cs_depth_of_field>(tweener<cs_depth_of_field>& that, xml_element* parent_xml, const char* name, bool read_when_untweened); // 0x0070B940 (:1046)
int cs_load_custom_animation(const char* filename, const char* morphset, bool& was_just_loaded); // 0x00706890 (:1340)
void cs_unload_animation(const char* const anim_name); // 0x00706A50 (:1429)
void cutscene_read_cutscene_list(base_array<cutscene_info>& cutscene_infos, const bool refresh); // 0x0070D690 (:1459)
void cutscene_load_current_data(cutscene_current_info* const cur_cutscene); // 0x0070F7C0 (:1515)
void cutscene_unload_data(const cutscene_current_info* const cur_cutscene); // 0x00709AC0 (:1574)
void cutscene_load_effects(cutscene_current_info* const cur_cutscene); // 0x00709DF0 (:1614)
