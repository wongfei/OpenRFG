// Original: volition/rfg/code/objects/humans/human_info.cpp
//
// The human types of character.xtbl (player_minimal.md P3): footsteps.xtbl's effect lists, the human container
// descriptions (human_info_shared.cpp), and per Character a human_info (stats, model, rig, animation set, heads,
// voice personas, flags, default inventory, props, container). The "Human"/"Player" container callbacks
// (human_info_stream_load/_unload) create the characters of the human type and its props and resolve the bones the
// game looks up by name. Compiled in rfg_objects_unity.cpp.
// The external interface is declared in human_info.h.

#include "volition/rfg/code/objects/humans/human_info.h"

#include "volition/anim_lib/code/anim_lib.h"
#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/rfg/code/destruction/rfg_materials.h"
#include "volition/rfg/code/effects/effects_shared.h"
#include "volition/rfg/code/geom/character.h"
#include "volition/rfg/code/geom/character_mesh.h"
#include "volition/rfg/code/geom/game_rig.h"
#include "volition/rfg/code/geom/vanim.h"
#include "volition/rfg/code/interface/localization/localization_shared.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_game/multi_game_classes.h"
#include "volition/rfg/code/objects/humans/human_heads_and_props.h"
#include "volition/rfg/code/objects/humans/human_info_shared.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice_shared.h"
#include "volition/rfg/code/objects/inventory_item.h"
#include "volition/rfg/code/rfg.h"
#include "volition/rfg/code/spawn/spawn_resource_manager.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/stream2/code/stream2.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/math/crc.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/memory/rfg_split_mempool.h"
#include "volition/vlib/code/memory/virtual_mempool.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/vlib/code/util/fastxml.h"
#include "volition/vlib/code/util/string_comp.h"

#include "keen/base/trace.hpp"

#include <stdio.h>
#include <string.h>

// ---- globals
farray<human_info,165> Master_human_info_list; // 0x03188080
string_pool Human_info_string_pool("Human info strings"); // 0x033D4D20, initialized in parse_human_table
foot_ground_effects* Footstep_effects_list; // 0x0302F068
int Num_footstep_effect_lists; // 0x0302F06C
int INVALID_HUMAN_INFO_ID = -1; // 0x01694394
int INVALID_HUMAN_RESOURCE_SLOT_ID = -1; // 0x01694390
const char VIP_GUERRILLA_TECH_NAME[8] = "Samanya"; // 0x01310E98
const char VIP_GUERRILLA_RFC_NAME[12] = "RFCommander"; // 0x01310EA0
const char VIP_OLD_COOT_NAME[8] = "OldCoot"; // 0x01310EAC
const char VIP_DAN_NAME[4] = "D"; // 0x01310EB4

// ---- file-static globals
static const char VIP_JENKINS_NAME[6] = "Jorge"; // 0x01310EB8
static const char SPECIAL_DISTRICT_NAME[8] = "Special"; // 0x01310EE4
static const char COMMON_DISTRICT_NAME[7] = "Common"; // 0x01310EEC
static const char HUMAN_INFO_STRING_POOL_NAME[19] = "Human info strings"; // 0x01310EF4
static const char XML_HUMAN_INFO_MAX_HITS_TAG[15] = "max_hit_points"; // 0x01310F08
static const char XML_HUMAN_INFO_KNOCK_DOWN_HITS_TAG[16] = "knock_down_hits"; // 0x01310F18
// const float MAX_SIMULATE_RAMP_SPEED_FACTOR (0x01311B90) is used here too; placed in human.cpp (earlier in the unity TU).
static const char* Footstep_effect_names[2][2] = {"LeftFootstepWalking", "LeftFootstepRunning", "RightFootstepWalking", "RightFootstepRunning"}; // 0x01694398
static const char* Jump_effect_names[2] = {"JumpDefault", "LandDefault"}; // 0x016943A8
static farray<char const*,16> Home_district_names; // 0x016947B0
static bool Human_info_init; // 0x0302F067
static char* Human_info_string_pool_data; // 0x0302F070

// A percentage of the table as a fraction (the binary divides in double).
static float table_percent(int value)
{
    return (float)((double)(float)value / 100.0);
}

// ---- internal functions
// 0x00A2FFB0 (:188, LTCG: district_name in EDI)
static int find_home_district_id(const char* district_name)
{
    if (district_name == nullptr)
    {
        return -1;
    }
    for (int i = 0; i < Home_district_names.size(); i++)
    {
        if (v_stricmp(district_name, Home_district_names[i]) == 0)
        {
            return i;
        }
    }
    return -1;
}

// 0x00A55BB0 (:207, LTCG: district_name in EAX): the index of the district name, added when new; "Special" and no
// name are -1.
static int get_home_district(const char* district_name)
{
    if (district_name == nullptr || v_stricmp(SPECIAL_DISTRICT_NAME, district_name) == 0)
    {
        return -1;
    }
    int id = find_home_district_id(district_name);
    if (id == -1)
    {
        const char* name = Human_info_string_pool.add_unique(district_name);
        if (name == nullptr || Home_district_names.full())
        {
            return -1;
        }
        id = Home_district_names.size();
        Home_district_names.add(name);
    }
    return id;
}

// 0x00A0ABA0 (:290)
static bool human_info_loaded()
{
    return Human_info_string_pool_data != nullptr;
}

// 0x00A0ABB0 (:333, LTCG: fep on the stack, root in EAX, elem_name in EDI)
static unsigned int get_footsteps_effect_handle(foot_ground_effects* fep, xml_element* root, const char* elem_name)
{
    xml_element* element = xml_table_find_req(root, elem_name);
    if (element == nullptr)
    {
        // The original formats into a thread-local buffer; the second %s gets the effect list (its name comes first).
        char message[256];
        sprintf(message, "Unrecognized effect version \"%s\" found \nparsing entry \"%s\" in footsteps.xtbl", elem_name, fep->name);
        keen::debug::nativePrintArgs(message);
        return 0xFFFFFFFF;
    }
    const char* effect = xml_table_get_opt_string_ref(element, nullptr);
    if (effect == nullptr)
    {
        return 0xFFFFFFFF;
    }
    return effects_get_handle(effect);
}

// The props of a human type's general prop (Item or Sub_Item entries).
static void parse_human_prop(human_info::human_prop_info* prop, xml_element* entry)
{
    int chance = 0;
    xml_table_get_req_int(chance, entry, "Chance");
    prop->appearance_percent = table_percent(chance);
    prop->item_prop_tag = nullptr;
    const char* character_prop = xml_table_get_opt_string_ref(entry, "Character_Prop");
    prop->character_prop_tag = nullptr;
    if (character_prop != nullptr)
    {
        prop->character_prop_tag = Human_info_string_pool.add_unique(character_prop);
    }
    const char* item_prop = xml_table_get_opt_string_ref(entry, "Item_Prop");
    prop->item_prop_tag = nullptr;
    if (item_prop != nullptr)
    {
        prop->item_prop_tag = Human_info_string_pool.add_unique(item_prop);
    }
    prop->item = object_item_get_info(xml_table_get_req_string_ref(entry, "Name"));
}

// 0x00A7FB40 (:368)
// One Character entry. On a reload only the bones are looked up again (the rig is paged in) and the rest is parsed
// over; refresh keeps the inventory and the container.
static bool load_human_info_block(mempool* pool, xml_element* xml_char_root, xml_element* xml_type_name, human_info* hip, bool reload, bool refresh)
{
    if (xml_char_root == nullptr || xml_type_name == nullptr || hip == nullptr)
    {
        return false;
    }
    hip->flags = human_info::human_info_flags();
    hip->head_bone_index = -1;
    hip->camera_bone_index = -1;
    hip->name = Human_info_string_pool.add_unique(xml_type_name->text);
    hip->slot_id = INVALID_HUMAN_RESOURCE_SLOT_ID;
    hip->name_crc = crc_stri(hip->name, 0);
    if (reload)
    {
        page_in_animlib_rig(hip->rig_name);
        char rig_name[64];
        cf_add_ext(rig_name, sizeof(rig_name), hip->rig_name, ".rig_pc", false);
        anim_rig* rig = anim_rig_find(rig_name);
        if (rig != nullptr)
        {
            hip->lower_spine_bone_index = animlib_find_bone_by_name(rig, "spine");
            hip->upper_spine_bone_index = animlib_find_bone_by_name(rig, "spine1");
            hip->pelvis_bone_index = animlib_find_bone_by_name(rig, "pelvis");
            hip->head_bone_index = animlib_find_bone_by_name(rig, "head");
            hip->neck_bone_index = animlib_find_bone_by_name(rig, "neck");
            hip->eye_l_bone_index = animlib_find_bone_by_name(rig, "l-eye");
            hip->eye_r_bone_index = animlib_find_bone_by_name(rig, "r-eye");
            hip->calf_l_bone_index = animlib_find_bone_by_name(rig, "l-calf");
            hip->calf_r_bone_index = animlib_find_bone_by_name(rig, "r-calf");
            hip->camera_bone_index = animlib_find_bone_by_name(rig, "camera");
        }
    }
    else
    {
        hip->model_name = Human_info_string_pool.add_unique(xml_table_get_req_filename_ref(xml_char_root, "Model"));
        if (v_stricmp(hip->model_name, "walker") == 0)
        {
            // The walker has no model of its own (the pooled string is emptied).
            hip->model_name[0] = '\0';
        }
        xml_element* district = xml_find(xml_char_root, "origin_district");
        hip->home_district = district != nullptr ? get_home_district(district->text) : -1;
        xml_table_get_req_float(hip->default_height_scale, xml_char_root, "Height");
        xml_table_get_req_float(hip->height_scale_variation, xml_char_root, "Variation");
        // The animation set without its .xtbl extension.
        char anim_set_name[64];
        strcpy(anim_set_name, xml_table_get_req_string_ref(xml_char_root, "Anim_Set"));
        char* ext = strstr(anim_set_name, ".xtbl");
        if (ext != nullptr)
        {
            *ext = '\0';
        }
        hip->anim_set_name = Human_info_string_pool.add_unique(anim_set_name);
        hip->anim_set_name_crc = crc_stri(hip->anim_set_name, 0);
        hip->rig_name = Human_info_string_pool.add_unique(xml_table_get_req_filename_ref(xml_char_root, "Rig"));
    }
    hip->default_team = human_team_from_name(xml_table_find_req(xml_char_root, "Team")->text);
    xml_element* max_hits = xml_find(xml_char_root, XML_HUMAN_INFO_MAX_HITS_TAG);
    if (max_hits == nullptr)
    {
        return false;
    }
    hip->set_max_hit_points(xml_get_int(max_hits, 0));
    if (hip->get_max_hit_points() < 1)
    {
        hip->set_max_hit_points(300); // magic number: the default hit points
    }
    xml_element* knock_down_hits = xml_find(xml_char_root, XML_HUMAN_INFO_KNOCK_DOWN_HITS_TAG);
    if (knock_down_hits == nullptr)
    {
        return false;
    }
    hip->set_max_knockdown_hits(xml_get_int(knock_down_hits, 0));
    if (hip->get_max_knockdown_hits() < 0)
    {
        hip->set_max_knockdown_hits(0);
    }
    float max_speed = 0.0f;
    xml_table_get_req_float(max_speed, xml_char_root, "max_speed");
    hip->set_max_speed(max_speed);
    int aim_error = 0;
    xml_table_get_req_int(aim_error, xml_char_root, "AimErrorPct");
    float aim_error_pct = table_percent(aim_error);
    hip->aim_error_pct = aim_error_pct;
    // Clamped to [0.5, 1.5] (the binary subtracts in double).
    if ((float)((double)aim_error_pct - 0.5) < 0.0f)
    {
        aim_error_pct = 0.5f;
    }
    if ((float)((double)aim_error_pct - 1.5) >= 0.0f)
    {
        aim_error_pct = MAX_SIMULATE_RAMP_SPEED_FACTOR;
    }
    hip->aim_error_pct = aim_error_pct;
    hip->hurt_threshold = 80; // magic number: the default hurt threshold (percent of health)
    int hurt_threshold = 0;
    if (xml_table_get_opt_int(hurt_threshold, xml_char_root, "Hurt_Threshold"))
    {
        hurt_threshold = hurt_threshold < 1 ? 1 : (hurt_threshold > 100 ? 100 : hurt_threshold);
        hip->hurt_threshold = (short)hurt_threshold;
    }
    hip->num_heads = 0;
    xml_element* heads = xml_table_find_opt(xml_char_root, "Heads");
    if (heads != nullptr)
    {
        // Original: the four head slots are not checked.
        for (xml_element* head = xml_find(heads, "Head"); head != nullptr; head = xml_find_next(heads, head, "Head"))
        {
            hip->heads[hip->num_heads++] = human_hap_parse_head(head);
        }
    }
    const char* footsteps = xml_table_get_opt_string_ref(xml_char_root, "FootstepsControl");
    for (int i = 0; i < Num_footstep_effect_lists; i++)
    {
        if (v_stricmp(footsteps, Footstep_effects_list[i].name) == 0)
        {
            hip->foot_effects = &Footstep_effects_list[i];
            break;
        }
    }
    xml_element* personas = xml_table_find_opt(xml_char_root, "Personas");
    if (personas != nullptr)
    {
        hip->voice_personas.load(pool, personas);
    }
    xml_element* flags = xml_table_find_req(xml_char_root, "Flags");
    for (xml_element* flag = xml_find(flags, "Flag"); flag != nullptr; flag = xml_find_next(flags, flag, "Flag"))
    {
        const char* text = flag->text;
        if (v_stricmp(text, "preload") == 0)
        {
            hip->flags.preload = true;
        }
        else if (v_stricmp(text, "use tech level") == 0)
        {
            hip->flags.use_tech_level = true;
        }
        else if (v_stricmp(text, "officer") == 0)
        {
            hip->flags.officer = true;
        }
        else if (v_stricmp(text, "unarmed") == 0)
        {
            hip->flags.unarmed = true;
        }
        else if (v_stricmp(text, "VIP") == 0)
        {
            hip->flags.vip = true;
        }
        else if (v_stricmp(text, "riot shield") == 0)
        {
            hip->flags.riot_shield = true;
        }
        else if (v_stricmp(text, "elite dodge") == 0)
        {
            hip->flags.elite_dodge = true;
        }
        else if (v_stricmp(text, "body armor") == 0)
        {
            hip->flags.body_armor = true;
        }
        else if (v_stricmp(text, "nonflammable") == 0)
        {
            hip->flags.nonflammable = true;
        }
        else if (v_stricmp(text, "no attaching projectiles") == 0)
        {
            hip->flags.no_attaching_projectiles = true;
        }
        else if (v_stricmp(text, "driverless exit only") == 0)
        {
            hip->flags.driverless_exit_only = true;
        }
    }
    if (!localization_allow_attaching_projectiles())
    {
        hip->flags.no_attaching_projectiles = true;
    }
    if (!localization_allow_humans_on_fire())
    {
        hip->flags.nonflammable = true;
    }
    if (hip->flags.vip)
    {
        if (v_stricmp(VIP_GUERRILLA_TECH_NAME, hip->name) == 0)
        {
            hip->flags.guerrilla_tech = true;
        }
        else if (v_stricmp(VIP_GUERRILLA_RFC_NAME, hip->name) == 0)
        {
            hip->flags.guerrilla_rfc = true;
        }
    }
    if (v_stricmp(VIP_OLD_COOT_NAME, hip->name) == 0)
    {
        hip->flags.old_coot = true;
    }
    if (v_stricmp(VIP_DAN_NAME, hip->name) == 0)
    {
        hip->flags.dan = true;
    }
    if (v_stricmp(VIP_JENKINS_NAME, hip->name) == 0)
    {
        hip->flags.jenkins = true;
    }
    if (v_stricmp(xml_table_get_req_string_ref(xml_table_find_req(xml_char_root, "Sex"), nullptr), "Female") == 0)
    {
        hip->flags.female = true;
    }
    xml_element* cash = xml_table_find_opt(xml_char_root, "Cash");
    hip->drop_cash_max = 0;
    hip->drop_cash_min = 0;
    if (cash != nullptr)
    {
        xml_element* coin = xml_table_find_opt(cash, "Coin");
        if (coin != nullptr)
        {
            hip->flags.coin = true;
            xml_table_get_req_int(hip->drop_cash_min, cash, "Coin");
        }
        else
        {
            xml_element* dollar = xml_table_find_opt(cash, "Dollar");
            if (dollar != nullptr)
            {
                xml_table_get_req_int(hip->drop_cash_min, dollar, "Min");
                xml_table_get_req_int(hip->drop_cash_max, dollar, "Max");
            }
        }
    }
    hip->light_effect_handle = 0xFFFFFFFF;
    const char* light_effect = xml_table_get_opt_string_ref(xml_char_root, "Light_Effect");
    if (light_effect != nullptr)
    {
        hip->light_effect_handle = effects_get_handle(light_effect);
    }
    hip->light_tag_names[0] = nullptr;
    hip->light_tag_names[1] = nullptr;
    xml_element* light_tags = xml_table_find_opt(xml_char_root, "Light_Tag_Names");
    if (light_tags != nullptr)
    {
        int count = 0;
        for (xml_element* tag = xml_find(light_tags, "Light_Tag_Name"); tag != nullptr && count < 2; tag = xml_find_next(light_tags, tag, "Light_Tag_Name"))
        {
            hip->light_tag_names[count++] = Human_info_string_pool.add_unique(tag->text);
        }
    }
    if (!refresh)
    {
        hip->default_inventory.remove_all();
        xml_element* inventory = xml_table_find_req(xml_char_root, "Inventory");
        for (xml_element* item = xml_find(inventory, "Item"); item != nullptr; item = xml_find_next(inventory, item, "Item"))
        {
            human_info_inventory_info entry;
            entry.i_info = inv_item_get_info(xml_table_get_req_string_ref(item, "Item_Name"));
            if (entry.i_info == nullptr)
            {
                continue;
            }
            entry.count = 0;
            xml_table_get_req_int(entry.count, item, "Item_Count");
            entry.slot = -1;
            xml_table_get_req_int(entry.slot, item, "Item_Slot");
            hip->default_inventory.add(entry);
            if (entry.i_info->w_info != nullptr && entry.i_info->w_info->flags.can_scope)
            {
                hip->flags.sniper = true;
            }
        }
    }
    const char* equip_item = xml_table_get_opt_string_ref(xml_char_root, "Equip_Item_Name");
    hip->set_default_equip_item(equip_item != nullptr ? inv_item_get_info(equip_item) : nullptr);
    const char* last_equip_item = xml_table_get_opt_string_ref(xml_char_root, "Last_Equip_Item_Name");
    hip->set_default_last_equip_item(last_equip_item != nullptr ? inv_item_get_info(last_equip_item) : nullptr);
    hip->num_general_props = 0;
    xml_element* props = xml_table_find_opt(xml_char_root, "Props");
    if (props != nullptr)
    {
        int num_props = 0;
        for (xml_element* prop = xml_find(props, "Prop"); prop != nullptr; prop = xml_find_next(props, prop, "Prop"))
        {
            num_props++;
        }
        hip->num_general_props = num_props;
        hip->general_props = (human_info::human_prop_general_info*)pool->alloc(num_props * sizeof(human_info::human_prop_general_info), 4);
        if (hip->general_props == nullptr)
        {
            return false;
        }
        int prop_index = 0;
        for (xml_element* prop = xml_find(props, "Prop"); prop != nullptr; prop = xml_find_next(props, prop, "Prop"), prop_index++)
        {
            human_info::human_prop_general_info& general = hip->general_props[prop_index];
            general.prop_name = Human_info_string_pool.add_unique(xml_table_get_req_string_ref(prop, "Prop_Name"));
            int chance = 0;
            xml_table_get_req_int(chance, prop, "Chance");
            general.appearance_percent = table_percent(chance);
            general.m_prop_type = vanim_get_prop_type_from_name(xml_table_get_req_string_ref(prop, "Type"));
            xml_element* items = xml_table_find_req(prop, "Items");
            int num_items = 0;
            for (xml_element* item = xml_find(items, "Item"); item != nullptr; item = xml_find_next(items, item, "Item"))
            {
                num_items++;
            }
            general.num_props = num_items;
            general.props = (human_info::human_prop_info*)pool->alloc(num_items * sizeof(human_info::human_prop_info), 4);
            int item_index = 0;
            for (xml_element* item = xml_find(items, "Item"); item != nullptr; item = xml_find_next(items, item, "Item"), item_index++)
            {
                human_info::human_prop_info* prop_info = &general.props[item_index];
                parse_human_prop(prop_info, item);
                xml_element* sub_items = xml_table_find_opt(item, "Sub_Items");
                prop_info->num_sub_props = 0;
                if (sub_items != nullptr)
                {
                    int num_sub_items = 0;
                    for (xml_element* sub_item = xml_find(sub_items, "Sub_Item"); sub_item != nullptr; sub_item = xml_find_next(sub_items, sub_item, "Sub_Item"))
                    {
                        num_sub_items++;
                    }
                    prop_info->num_sub_props = num_sub_items;
                    prop_info->sub_props = (human_info::human_prop_info*)pool->alloc(num_sub_items * sizeof(human_info::human_prop_info), 4);
                    int sub_index = 0;
                    for (xml_element* sub_item = xml_find(sub_items, "Sub_Item"); sub_item != nullptr; sub_item = xml_find_next(sub_items, sub_item, "Sub_Item"))
                    {
                        parse_human_prop(&prop_info->sub_props[sub_index++], sub_item);
                    }
                }
            }
        }
    }
    if (!refresh)
    {
        // The container: the shared one when named, else the human type's own.
        const char* container = xml_table_get_opt_string_ref(xml_char_root, "shared_container");
        if (container == nullptr || container[0] == '\0')
        {
            container = hip->name;
        }
        hip->srid = stream2_container_find(container);
        for (int i = 0; i < hip->num_heads; i++)
        {
            hip->heads[i]->morph_mat_srid = hip->srid;
        }
    }
    hip->slot_id = hip->srid != 0 ? srm_create_human_resource_slot_id(hip->srid) : INVALID_HUMAN_RESOURCE_SLOT_ID;
    return true;
}

// 0x00A30070 (:817)
// footsteps.xtbl: per Footsteps entry its foley cues and the walk, run, jump and land effects per effect material and
// shape type, in Effect_material_pool.
static void parse_human_table_footsteps()
{
    xml_element* table = xml_table_parse("footsteps.xtbl");
    if (table == nullptr)
    {
        return;
    }
    Num_footstep_effect_lists = xml_table_count(table, "Footsteps");
    Footstep_effects_list = (foot_ground_effects*)Effect_material_pool.static_mempool_base::alloc(Num_footstep_effect_lists * sizeof(foot_ground_effects), 0);
    if (Footstep_effects_list == nullptr)
    {
        xml_table_close(table);
        return;
    }
    foot_ground_effects* fep = Footstep_effects_list;
    for (xml_element* entry = xml_find(table, "Footsteps"); entry != nullptr; entry = xml_find_next(table, entry, "Footsteps"), fep++)
    {
        // Original: an unbounded copy into the 20 byte name (kept).
        strcpy(fep->name, xml_table_get_req_string_ref(entry, "Name"));
        const char* cue = xml_table_get_opt_string_ref(entry, "Walk");
        fep->walk_foley = -1;
        if (cue != nullptr)
        {
            audiolib_cue_get_id(cue, &fep->walk_foley);
        }
        cue = xml_table_get_opt_string_ref(entry, "Run");
        fep->run_foley = -1;
        if (cue != nullptr)
        {
            audiolib_cue_get_id(cue, &fep->run_foley);
        }
        cue = xml_table_get_opt_string_ref(entry, "Jump");
        fep->jump_foley = -1;
        if (cue != nullptr)
        {
            audiolib_cue_get_id(cue, &fep->jump_foley);
        }
        cue = xml_table_get_opt_string_ref(entry, "Land");
        fep->land_foley = -1;
        if (cue != nullptr)
        {
            audiolib_cue_get_id(cue, &fep->land_foley);
        }
        const int num_materials = rfg_get_effect_material_count();
        fep->footstep = (unsigned int (**)[5][2][2])Effect_material_pool.static_mempool_base::alloc(num_materials * sizeof(*fep->footstep), 0);
        if (fep->footstep == nullptr)
        {
            // The original returns without closing the table here and below.
            return;
        }
        memset(fep->footstep, 0, num_materials * sizeof(*fep->footstep));
        fep->jump = (unsigned int (**)[5][2])Effect_material_pool.static_mempool_base::alloc(rfg_get_effect_material_count() * sizeof(*fep->jump), 0);
        if (fep->jump == nullptr)
        {
            return;
        }
        memset(fep->jump, 0, rfg_get_effect_material_count() * sizeof(*fep->jump));
        xml_element* footsteps_control = xml_table_find_req(entry, "FootstepsControl");
        for (xml_element* definition = xml_find(footsteps_control, "FootstepDefinition"); definition != nullptr;
             definition = xml_find_next(footsteps_control, definition, "FootstepDefinition"))
        {
            const char* material_name = xml_table_get_req_string_ref(definition, "FootstepMaterial");
            const unsigned char material = rfg_get_effect_material_index(material_name);
            if (material == 0xFF || !rfg_is_same_effect_material(material, material_name))
            {
                continue;
            }
            material_shape_type shape_type = rfg_material_get_shape_type(xml_table_get_req_string_ref(definition, "FootstepShapeType"));
            if (shape_type == INVALID_MATERIAL_EVENT_SHAPE_TYPE)
            {
                shape_type = MATERIAL_EVENT_SHAPE_SOLID;
            }
            for (int version = FOOTSTEP_LEFT; version < NUM_FOOTSTEP_VERSIONS; version++)
            {
                for (int variation = FOOTSTEP_WALKING; variation < NUM_FOOTSTEP_VARIATIONS; variation++)
                {
                    const unsigned int handle = get_footsteps_effect_handle(fep, definition, Footstep_effect_names[version][variation]);
                    if (handle != 0xFFFFFFFF)
                    {
                        fep->set_footstep_effect_handle(handle, material, shape_type, (footstep_version)version, (footstep_variation)variation);
                    }
                }
            }
        }
        xml_element* jump_control = xml_table_find_req(entry, "JumpControl");
        for (xml_element* definition = xml_find(jump_control, "JumpDefinition"); definition != nullptr; definition = xml_find_next(jump_control, definition, "JumpDefinition"))
        {
            const unsigned char material = rfg_get_effect_material_index(xml_table_get_req_string_ref(definition, "JumpMaterial"));
            if (material == 0xFF)
            {
                continue;
            }
            material_shape_type shape_type = rfg_material_get_shape_type(xml_table_get_req_string_ref(definition, "JumpShapeType"));
            if (shape_type == INVALID_MATERIAL_EVENT_SHAPE_TYPE)
            {
                shape_type = MATERIAL_EVENT_SHAPE_SOLID;
            }
            for (int version = JUMP_TAKEOFF; version < NUM_JUMP_VERSIONS; version++)
            {
                const unsigned int handle = get_footsteps_effect_handle(fep, definition, Jump_effect_names[version]);
                if (handle != 0xFFFFFFFF)
                {
                    fep->set_jump_effect_handle(handle, material, shape_type, (jump_version)version);
                }
            }
        }
    }
    xml_table_close(table);
}

// 0x00A30420 (:1025): releases the containers of the preloaded human types.
static void human_info_release_preloaded()
{
    for (int i = 0; i < Master_human_info_list.size(); i++)
    {
        if (Master_human_info_list[i].flags.preload)
        {
            const unsigned int srid = stream2_container_find(Master_human_info_list[i].name);
            if (srid != 0)
            {
                stream2_release(srid);
            }
        }
    }
}

// The character of a prop item of a human container (the item itself has no container of its own) and its tag or
// bone index on the human rig.
static void human_info_load_prop(human_info::human_prop_info* prop, unsigned int srid, anim_rig* rig)
{
    item_info* item = prop->item;
    if (item->srid == 0 && !item->multiplayer_only && item->cmesh_name != nullptr)
    {
        character_load(item->name, srid, character_mesh_find(item->cmesh_name, srid), anim_rig_find(item->rig_name), item->cmesh_name);
    }
}

static void human_info_find_prop_index(human_info::human_prop_info* prop, anim_rig* rig)
{
    if (prop->character_prop_tag == nullptr)
    {
        prop->character_prop_index = -1;
        return;
    }
    prop->character_prop_index = animlib_find_tag_by_name(rig, prop->character_prop_tag);
    if (prop->character_prop_index == -1)
    {
        prop->character_prop_index = animlib_find_bone_by_name(rig, prop->character_prop_tag);
    }
}

// ---- functions
// 0x00A0AB00 (:245): the multiplayer class's in multiplayer.
int human_info::get_max_hit_points() const
{
    return Multiplayer ? multi_game_classes_get_default_class()->max_hit_points : max_hit_points;
}

// 0x00A0AB20 (:254)
int human_info::get_max_knockdown_hits() const
{
    return Multiplayer ? multi_game_classes_get_default_class()->max_knockdown_hits : max_knockdown_hits;
}

// 0x00A0AB40 (:263)
float human_info::get_max_speed() const
{
    return Multiplayer ? multi_game_classes_get_default_class()->max_speed : max_speed;
}

// 0x00A0AB60 (:272)
const inv_item_info* human_info::get_default_equip_item() const
{
    return Multiplayer ? multi_game_classes_get_default_class()->default_equip_item : default_equip_item;
}

// 0x00A0AB80 (:281)
const inv_item_info* human_info::get_default_last_equip_item() const
{
    return Multiplayer ? multi_game_classes_get_default_class()->default_last_equip_item : default_last_equip_item;
}

// 0x00A05510 (.h:214)
void human_info::set_max_hit_points(int _max_hit_points)
{
    max_hit_points = _max_hit_points;
}

// 0x00A05520 (.h:215)
void human_info::set_max_knockdown_hits(int _max_knockdown_hits)
{
    max_knockdown_hits = _max_knockdown_hits;
}

// 0x00A05530 (.h:216)
void human_info::set_max_speed(float _max_speed)
{
    max_speed = _max_speed;
}

// 0x00A05540 (.h:217)
void human_info::set_default_equip_item(inv_item_info* _default_equip_item)
{
    default_equip_item = _default_equip_item;
}

// 0x00A05550 (.h:218)
void human_info::set_default_last_equip_item(inv_item_info* _default_last_equip_item)
{
    default_last_equip_item = _default_last_equip_item;
}

// 0x00A74370
human_info::human_info()
{
}

// 0x00A47480
human_info::~human_info()
{
}

// 0x007F03E0: memberwise (the arrays copy their elements; everything else is plain data).
human_info& human_info::operator=(const human_info& __that)
{
    memcpy((void*)this, (const void*)&__that, offsetof(human_info, voice_personas));
    voice_personas = __that.voice_personas;
    default_inventory.remove_all();
    for (int i = 0; i < __that.default_inventory.size(); i++)
    {
        default_inventory.add(__that.default_inventory[i]);
    }
    memcpy((void*)&num_general_props, (const void*)&__that.num_general_props, sizeof(human_info) - offsetof(human_info, num_general_props));
    return *this;
}

// 0x00A2FFF0 (:296)
human_info* human_info_find_internal(const char* human_name)
{
    const unsigned int name_crc = crc_stri(human_name, 0);
    if (Human_info_string_pool_data == nullptr)
    {
        return nullptr;
    }
    for (int i = 0; i < Master_human_info_list.size(); i++)
    {
        human_info* hip = &Master_human_info_list[i];
        if (name_crc == hip->name_crc && v_stricmp(human_name, hip->name) == 0)
        {
            return hip;
        }
    }
    return nullptr;
}

// 0x00ABD070 (:950)
// character.xtbl (or a DLC's dlcNN_character.xtbl): the footsteps and the human containers first, then a human_info
// per Character (an existing one is reloaded). An entry that fails to load is removed again.
void parse_human_table(bool refresh, mempool* pool, int dlc_id)
{
    if (dlc_id == -1)
    {
        if (Human_info_string_pool_data == nullptr)
        {
            // Magic number: 0x1838 bytes for the human info strings.
            Human_info_string_pool_data = (char*)pool->alloc(0x1838, 0);
            if (Human_info_string_pool_data == nullptr)
            {
                keen::debug::nativePrintArgs("Unable to allocate memory for the human info string table");
            }
            Human_info_string_pool.init(Human_info_string_pool_data, 0x1838, HUMAN_INFO_STRING_POOL_NAME, 0, 0);
        }
        Human_info_string_pool.clear();
        // The first home district is "Common".
        Home_district_names.remove_all();
        Home_district_names.add(COMMON_DISTRICT_NAME);
        parse_human_table_footsteps();
    }
    build_human_containers_shared(&Common_mempool, pool, PLATFORM_PC, false, dlc_id);
    xml_element* table;
    if (dlc_id != -1)
    {
        char filename[256];
        sprintf(filename, "dlc%02d_%s", dlc_id, "character.xtbl");
        table = xml_table_parse(filename);
    }
    else
    {
        table = xml_table_parse("character.xtbl");
    }
    if (table == nullptr)
    {
        return;
    }
    for (xml_element* entry = xml_find(table, "Character"); entry != nullptr; entry = xml_find_next(table, entry, "Character"))
    {
        xml_element* type_name = xml_find(entry, "Name");
        human_info* hip = human_info_find_internal(type_name->text);
        int index = -1;
        bool reload = hip != nullptr;
        if (hip == nullptr)
        {
            hip = Master_human_info_list.add_in_place(index);
        }
        if (hip != nullptr && !load_human_info_block(pool, entry, type_name, hip, reload, refresh))
        {
            // Original: on a failed reload the index is -1, and remove_unsorted copies the last entry before the array.
            Master_human_info_list.remove_unsorted(index);
        }
    }
    xml_table_close(table);
}

// 0x00A30470 (:1049)
// A "Human"/"Player" container loaded: for each human type of it, the bone indices, its character (mesh and rig of the
// container) and the characters of its prop items, then the heads.
void human_info_stream_load(unsigned int srid)
{
    if (Human_info_string_pool_data == nullptr)
    {
        return;
    }
    for (int i = 0; i < Master_human_info_list.size(); i++)
    {
        human_info* hip = &Master_human_info_list[i];
        if (hip->srid != srid)
        {
            continue;
        }
        if (character_find(hip->name, hip->srid) != nullptr)
        {
            // Original: an already loaded character ends the whole callback (the heads are not loaded either).
            return;
        }
        character_mesh* cmesh = character_mesh_find(hip->model_name, srid);
        char rig_name[64];
        cf_add_ext(rig_name, sizeof(rig_name), hip->rig_name, ".rig_pc", false);
        anim_rig* rig = anim_rig_find(rig_name);
        if (rig != nullptr)
        {
            hip->lower_spine_bone_index = animlib_find_bone_by_name(rig, "spine");
            hip->upper_spine_bone_index = animlib_find_bone_by_name(rig, "spine1");
            hip->pelvis_bone_index = animlib_find_bone_by_name(rig, "pelvis");
            hip->head_bone_index = animlib_find_bone_by_name(rig, "head");
            hip->neck_bone_index = animlib_find_bone_by_name(rig, "neck");
            hip->eye_l_bone_index = animlib_find_bone_by_name(rig, "l-eye");
            hip->eye_r_bone_index = animlib_find_bone_by_name(rig, "r-eye");
            hip->calf_l_bone_index = animlib_find_bone_by_name(rig, "l-calf");
            hip->calf_r_bone_index = animlib_find_bone_by_name(rig, "r-calf");
            hip->camera_bone_index = animlib_find_bone_by_name(rig, "camera");
        }
        character_load(hip->name, srid, cmesh, rig, hip->model_name);
        for (int general = 0; general < hip->num_general_props; general++)
        {
            human_info::human_prop_general_info& general_prop = hip->general_props[general];
            for (int p = 0; p < general_prop.num_props; p++)
            {
                human_info::human_prop_info* prop = &general_prop.props[p];
                if (prop->item == nullptr)
                {
                    continue;
                }
                human_info_load_prop(prop, srid, rig);
                for (int s = 0; s < prop->num_sub_props; s++)
                {
                    // Original: the sub prop items are not checked for null.
                    human_info_load_prop(&prop->sub_props[s], srid, rig);
                    human_info_find_prop_index(&prop->sub_props[s], rig);
                }
                human_info_find_prop_index(prop, rig);
            }
        }
    }
    human_hap_stream_load(srid);
}

// 0x00A30870 (:1156)
void human_info_stream_unload(unsigned int srid)
{
    if (Human_info_string_pool_data == nullptr)
    {
        return;
    }
    for (int i = 0; i < Master_human_info_list.size(); i++)
    {
        human_info* hip = &Master_human_info_list[i];
        if (hip->srid != srid)
        {
            continue;
        }
        character* cp = character_find(hip->name, hip->srid);
        if (cp == nullptr)
        {
            // Original: as in the load, this ends the whole callback.
            return;
        }
        character_unload(cp);
        hip->lower_spine_bone_index = -1;
        hip->upper_spine_bone_index = -1;
        hip->pelvis_bone_index = -1;
        hip->head_bone_index = -1;
        hip->neck_bone_index = -1;
        hip->eye_l_bone_index = -1;
        hip->eye_r_bone_index = -1;
        hip->calf_l_bone_index = -1;
        hip->calf_r_bone_index = -1;
        hip->camera_bone_index = -1;
        for (int general = 0; general < hip->num_general_props; general++)
        {
            human_info::human_prop_general_info& general_prop = hip->general_props[general];
            for (int p = 0; p < general_prop.num_props; p++)
            {
                human_info::human_prop_info* prop = &general_prop.props[p];
                item_info* item = prop->item;
                if (item != nullptr && item->srid == 0 && !item->multiplayer_only && item->cmesh_name != nullptr)
                {
                    character* prop_cp = character_find(item->name, srid);
                    if (prop_cp != nullptr)
                    {
                        character_unload(prop_cp);
                    }
                }
                for (int s = 0; s < prop->num_sub_props; s++)
                {
                    item_info* sub_item = prop->sub_props[s].item;
                    if (sub_item != nullptr && sub_item->srid == 0 && !sub_item->multiplayer_only && sub_item->cmesh_name != nullptr)
                    {
                        // Original: the sub prop's character is unloaded without a null check (kept).
                        character_unload(character_find(sub_item->name, srid));
                    }
                }
            }
        }
    }
    human_hap_stream_unload(srid);
}

// 0x00ABD250 (:1226)
void human_info_system_init()
{
    if (!Human_info_init)
    {
        Human_info_init = true;
        Master_human_info_list.remove_all();
        parse_human_table(false, &Permanent_mempool, -1);
    }
}

// 0x00A30A60 (:1239)
void human_info_system_shutdown()
{
    if (Human_info_init)
    {
        Human_info_init = false;
        human_info_release_preloaded();
    }
}

// 0x00A0AC20 (:1251)
const farray<human_info,165>& human_info_get_master_list()
{
    return Master_human_info_list;
}

// 0x00A30A80 (:1290)
const human_info* human_info_find(const char* human_name)
{
    return human_info_find_internal(human_name);
}

// 0x00A30A90 (:1296)
int human_info_id_find(const char* human_name)
{
    const unsigned int name_crc = crc_stri(human_name, 0);
    if (Human_info_string_pool_data == nullptr)
    {
        return INVALID_HUMAN_INFO_ID;
    }
    for (int i = 0; i < Master_human_info_list.size(); i++)
    {
        if (name_crc == Master_human_info_list[i].name_crc && v_stricmp(human_name, Master_human_info_list[i].name) == 0)
        {
            return i;
        }
    }
    return INVALID_HUMAN_INFO_ID;
}

// 0x00A30B10 (:1317)
const human_info* human_info_find(int id)
{
    if (id > -1 && id < Master_human_info_list.size())
    {
        return &Master_human_info_list[id];
    }
    return nullptr;
}

// 0x00A30B30 (:1338)
const human_info* human_info_find(unsigned int checksum)
{
    if (Human_info_string_pool_data != nullptr)
    {
        for (int i = 0; i < Master_human_info_list.size(); i++)
        {
            if (checksum == Master_human_info_list[i].name_crc)
            {
                return &Master_human_info_list[i];
            }
        }
    }
    return nullptr;
}

// 0x00970690 (.h:160)
voice_persona_list::voice_persona_list()
{
    reset();
}

// 0x0097A7E0
voice_persona_list::~voice_persona_list()
{
}

// 0x007D7150
voice_persona_list& voice_persona_list::operator=(const voice_persona_list& __that) = default;

// 0x00A2C590 (.h:169)
int voice_persona_list::get_voice_count()
{
    return voice_list.size();
}

// 0x00A30B80 (:1357)
void voice_persona_list::reset()
{
    voice_list.remove_all();
}

// 0x00A55C20 (:1366)
bool voice_persona_list::add_voice_name(const char* new_name)
{
    if (voice_list.full())
    {
        return false;
    }
    human_voice_persona* persona = human_voice_persona_from_name(new_name);
    if (persona != nullptr)
    {
        voice_list.add(persona);
    }
    return true;
}

// 0x00A55C80 (:1391)
// A random persona of the list that can be loaded (each tried once), else any of them; its load is requested.
human_voice_persona* voice_persona_list::pick_voice_generic(const farray<human_voice_persona*,64>& filtered_voice_list) const
{
    if (filtered_voice_list.size() < 1)
    {
        return nullptr;
    }
    farray<human_voice_persona*,64> candidates;
    for (int i = 0; i < filtered_voice_list.size(); i++)
    {
        candidates.add(filtered_voice_list[i]);
    }
    human_voice_persona* persona = nullptr;
    for (;;)
    {
        if (candidates.size() < 1)
        {
            const int count = filtered_voice_list.size();
            persona = filtered_voice_list[count == 1 ? 0 : int_rand_range(0, count - 1)];
            break;
        }
        const int index = candidates.size() == 1 ? 0 : int_rand_range(0, candidates.size() - 1);
        persona = candidates[index];
        candidates.remove_unsorted(index);
        if (persona->can_load())
        {
            break;
        }
    }
    persona->background_load_request();
    return persona;
}

// 0x00A55D60 (:1437): the Persona names (up to 64 unique ones) into an array allocated from pool.
void voice_persona_list::load(mempool* pool, xml_element* xml_root)
{
    farray<human_voice_persona*,64> personas;
    for (xml_element* entry = xml_find(xml_root, "Persona"); entry != nullptr; entry = xml_find_next(xml_root, entry, "Persona"))
    {
        if (voice_list.size() < 64)
        {
            human_voice_persona* persona = human_voice_persona_from_name(xml_table_get_req_string_ref(entry, nullptr));
            if (persona != nullptr)
            {
                personas.add_unique(persona);
            }
        }
    }
    human_voice_persona** storage = (human_voice_persona**)pool->alloc(personas.size() * sizeof(human_voice_persona*), 4);
    if (storage != nullptr)
    {
        voice_list.init(storage, personas.size(), 0);
        for (int i = 0; i < personas.size(); i++)
        {
            voice_list.add(personas[i]);
        }
    }
}

// 0x00A55E70 (:1481): a voice with the needed line sets, else any voice of the list.
human_voice_persona* voice_persona_list::pick_voice(bool needs_hostage_lines, bool needs_raid_lines, bool needs_raid_intro, bool needs_miner_persona) const
{
    if (voice_list.size() < 1)
    {
        return nullptr;
    }
    farray<human_voice_persona*,64> filtered;
    for (int i = 0; i < voice_list.size(); i++)
    {
        filtered.add(voice_list[i]);
    }
    for (int i = 0; needs_hostage_lines && i < filtered.size();)
    {
        if (filtered[i]->flags.ha_hostage_lines == 0)
        {
            filtered.remove_unsorted(i);
        }
        else
        {
            i++;
        }
    }
    for (int i = 0; needs_raid_lines && i < filtered.size();)
    {
        if (filtered[i]->flags.raid_lines == 0)
        {
            filtered.remove_unsorted(i);
        }
        else
        {
            i++;
        }
    }
    for (int i = 0; needs_raid_intro && i < filtered.size();)
    {
        if (filtered[i]->flags.raid_into_lines == 0)
        {
            filtered.remove_unsorted(i);
        }
        else
        {
            i++;
        }
    }
    for (int i = 0; needs_miner_persona && i < filtered.size();)
    {
        if (filtered[i]->flags.miner_persona == 0)
        {
            filtered.remove_unsorted(i);
        }
        else
        {
            i++;
        }
    }
    if (filtered.size() < 1)
    {
        for (int i = 0; i < voice_list.size(); i++)
        {
            filtered.add(voice_list[i]);
        }
    }
    return pick_voice_generic(filtered);
}

// 0x00A0AC30 (:1571): the effect of the material, else of the default effect material.
unsigned int foot_ground_effects::get_footstep_effect_handle(unsigned char mat_index, material_shape_type shape_type, footstep_version version, footstep_variation variation)
{
    if (footstep == nullptr)
    {
        return 0xFFFFFFFF;
    }
    unsigned int handle = 0xFFFFFFFF;
    if ((footstep[mat_index] == nullptr || (handle = (*footstep[mat_index])[shape_type][version][variation]) == 0xFFFFFFFF) &&
        mat_index != Rfg_effect_material_default_index && footstep[Rfg_effect_material_default_index] != nullptr)
    {
        return (*footstep[Rfg_effect_material_default_index])[shape_type][version][variation];
    }
    return handle;
}

// 0x00A0ACB0 (:1590)
// The first effect of a material and version/variation is used for every shape type; later ones set their shape type.
void foot_ground_effects::set_footstep_effect_handle(unsigned int handle, unsigned char mat_index, material_shape_type shape_type, footstep_version version, footstep_variation variation)
{
    if (footstep == nullptr)
    {
        return;
    }
    if (footstep[mat_index] == nullptr)
    {
        footstep[mat_index] = (unsigned int (*)[5][2][2])Effect_material_pool.static_mempool_base::alloc(sizeof(**footstep), 0);
        if (footstep[mat_index] == nullptr)
        {
            return;
        }
        memset(footstep[mat_index], 0xFF, sizeof(**footstep));
    }
    unsigned int (&handles)[5][2][2] = *footstep[mat_index];
    if (handles[shape_type][version][variation] == 0xFFFFFFFF)
    {
        for (int shape = 0; shape < 5; shape++)
        {
            handles[shape][version][variation] = handle;
        }
        return;
    }
    handles[shape_type][version][variation] = handle;
}

// 0x00A0AD90 (:1622)
unsigned int foot_ground_effects::get_jump_effect_handle(unsigned char mat_index, material_shape_type shape_type, jump_version version)
{
    if (jump == nullptr)
    {
        return 0xFFFFFFFF;
    }
    unsigned int handle = 0xFFFFFFFF;
    if ((jump[mat_index] == nullptr || (handle = (*jump[mat_index])[shape_type][version]) == 0xFFFFFFFF) && mat_index != Rfg_effect_material_default_index &&
        jump[Rfg_effect_material_default_index] != nullptr)
    {
        return (*jump[Rfg_effect_material_default_index])[shape_type][version];
    }
    return handle;
}

// 0x00A0AE00 (:1641)
void foot_ground_effects::set_jump_effect_handle(unsigned int handle, unsigned char mat_index, material_shape_type shape_type, jump_version version)
{
    if (jump == nullptr)
    {
        return;
    }
    if (jump[mat_index] == nullptr)
    {
        jump[mat_index] = (unsigned int (*)[5][2])Effect_material_pool.static_mempool_base::alloc(sizeof(**jump), 0);
        if (jump[mat_index] == nullptr)
        {
            return;
        }
        memset(jump[mat_index], 0xFF, sizeof(**jump));
    }
    unsigned int (&handles)[5][2] = *jump[mat_index];
    if (handles[shape_type][version] == 0xFFFFFFFF)
    {
        for (int shape = 0; shape < 5; shape++)
        {
            handles[shape][version] = handle;
        }
        return;
    }
    handles[shape_type][version] = handle;
}
