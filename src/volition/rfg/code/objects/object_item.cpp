// Original: volition/rfg/code/objects/object_item.cpp
//
// Partial (player_minimal.md P3): the item table (object_item_system_init, item_parse_table_element), the item info
// lookups and the "Items" container callbacks (item_stream_load/_unload: the character of an item with a character
// mesh). Compiled in rfg_objects_unity.cpp. The item object is P4, its render update P7. The high-res texture callbacks
// (item_high_res_stream_load/_unload) override the item mesh's render materials with the high-res container's standalone
// material. Every touch script of Item_touch_callbacks is ported (final_push.md step 2).
// The external interface is declared in object_item.h.

#include "volition/rfg/code/objects/object_item.h"

#include "volition/render_lib/code/rl_material_library.h"
#include "volition/render_lib/code/rl_material_map.h"
#include "volition/rfg/code/code_effects/material_effects_shared.h"
#include "volition/rfg/code/geom/cloth_simulation.h"
#include "volition/rfg/code/geom/material.h"
#include "volition/rfg/code/geom/mesh_shared.h"
#include "volition/rfg/code/objects/object_bftp_node.h"
#include "volition/rfg/code/objects/turret.h"

#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/render_lib/code/rl_mesh_instance.h"
#include "volition/render_lib/code/rl_skin_instance.h"
#include "volition/rfg/code/code_effects/explosion_shared.h"
#include "volition/rfg/code/code_effects/material_effects.h"
#include "volition/rfg/code/gameplay/wrecking_crew/wrecking_crew.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/objects/object_prop_names_shared.h"
#include "volition/rfg/code/destruction/rfg_materials.h"
#include "volition/rfg/code/gamewide/game_render.h"
#include "volition/rfg/code/gamewide/tweak_table.h"
#include "volition/rfg/code/geom/character.h"
#include "volition/rfg/code/geom/character_instance.h"
#include "volition/rfg/code/geom/character_mesh.h"
#include "volition/rfg/code/geom/game_rig.h"
#include "volition/rfg/code/geom/static_mesh.h"
#include "volition/rfg/code/geom/vanim.h"
#include "volition/rfg/code/havok/havok_cm_manager.h"
#include "volition/rfg/code/hud/minimap.h"
#include "volition/rfg/code/hud/hud.h"
#include "volition/rfg/code/hud/hud_message.h"
#include "volition/rfg/code/gameplay/gameflow/gfm_tech_level.h"
#include "volition/rfg/code/multi/multi_game/modes/multi_game_mode_bagman.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/rfg/code/event_logging/gamelog.h"
#include "volition/rfg/code/gameplay/upgrades.h"
#include "volition/rfg/code/interface/ui_document_hud.h"
#include "volition/rfg/code/multi/multi_game/multi_gameplay_mode.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_combat_turret.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/rfg/code/objects/inventory_item.h"
#include "volition/rfg/code/objects/object_item_shared.h"
#include "volition/rfg/code/rfg.h"
#include "volition/stream2/code/stream2.h"
#include "volition/vlib/code/bmpman/bmpman.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/memory/rfg_split_mempool.h"
#include "volition/vlib/code/memory/virtual_mempool.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/vlib/code/util/string_comp.h"

#include "keen/base/trace.hpp"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

// ---- globals and file statics compiled since P3
item_info* Item_infos; // 0x0302F148
int Max_item_infos; // 0x0302F144
int Num_item_infos; // 0x03017C5C
int Num_large_high_res_weapons_loaded; // 0x0302F14C
int Num_high_res_weapons_loaded; // 0x0302F150
item_info* High_res_weapons_loaded[4]; // 0x03024E74
static unsigned char Item_physical_material = 0xFF; // 0x0169452E
static unsigned char Item_effect_material = 0xFF; // 0x0169452F
static const char* Damage_info_flags[6] = {"can_explode", "damaged_by_bullets", "damaged_by_collisions", "damaged_by_explosions", "damaged_by_melee", "damaged_by_vehicles"}; // 0x01694588
static bool item_touch_ask_pickup(human* hp, item* obj_itemp, bool& play_secondary_sound, bool& ask_pickup); // 0x00A3B340 (:669)
static bool item_touch_give_item(human* hp, item* obj_itemp, bool& play_secondary_sound, bool& ask_pickup); // 0x00ABA110 (:722)
static bool item_touch_give_item_safehouse(human* hp, item* obj_itemp, bool& play_secondary_sound, bool& ask_pickup); // 0x00A64220 (:931)
static bool item_touch_give_money(human* hp, item* obj_itemp, bool& play_secondary_sound, bool& ask_pickup); // 0x00A18410 (:995)
static bool item_touch_give_money2(human* hp, item* obj_itemp, bool& play_secondary_sound, bool& ask_pickup); // 0x00A18670 (:1039)
static bool item_touch_give_full_health(human* hp, item* obj_itemp, bool& play_secondary_sound, bool& ask_pickup); // 0x00A18860 (:1082)
static bool item_touch_give_health_percent(human* hp, item* obj_itemp, bool& play_secondary_sound, bool& ask_pickup); // 0x00A18950 (:1111)
static bool item_touch_give_salvage(human* hp, item* obj_itemp, bool& play_secondary_sound, bool& ask_pickup); // 0x00A3B510 (:1134)
static bool item_touch_give_backpack(human* hp, item* obj_itemp, bool& play_secondary_sound, bool& ask_pickup); // 0x00A3B6C0 (:1191)
static bool item_touch_increase_tech_level(human* hp, item* obj_itemp, bool& play_secondary_sound, bool& ask_pickup); // 0x00A18A40 (:1222)
static bool item_touch_blast_from_the_past(human* hp, item* obj_itemp, bool& play_secondary_sound, bool& ask_pickup); // 0x00A64310 (:1260)
static bool item_touch_fuel_cell(human* hp, item* obj_itemp, bool& play_secondary_sound, bool& ask_pickup); // 0x00A18AF0 (:1282)
// 0x016945A0, the touch scripts by Item_touch_callback_names. "give_rand_money" is item_touch_give_money and
// "give_money" item_touch_give_money2 (rfg.exe's names); bagman_flag is multi_game_mode_bagman.cpp's.
static bool (*Item_touch_callbacks[13])(human*, item*, bool&, bool&) = {item_touch_ask_pickup, item_touch_give_item, item_touch_give_item_safehouse, item_touch_give_salvage,
                                                                         item_touch_give_money, item_touch_give_money2, item_touch_give_full_health, item_touch_give_health_percent,
                                                                         item_touch_give_backpack, item_touch_increase_tech_level, item_touch_blast_from_the_past, item_touch_bagman_flag,
                                                                         item_touch_fuel_cell};
// The thread's message buffer (TLS +0x38 in rfg.exe): the money pickup messages are formatted into it.
// Deviation from rfg.exe: a file-local array, where the buffer is part of its per-thread data.
static __declspec(thread) char Item_touch_message[1024];
static const char* Item_touch_callback_names[13] = {"ask_pickup", "give_item", "give_item_safehouse", "give_salvage", "give_rand_money", "give_money", "give_full_health", "give_health_percent", "give_backpack", "increase_tech_level", "blast_from_the_past", "bagman_flag", "fuel_cell"}; // 0x016945D4
static bool Object_item_init; // 0x0302F136

// 0x00A19920: nothing to construct.
item_info::item_info()
{
}

// ---- internal functions
// 0x00A908E0 (:378)
// The inventory items, then items_3d.xtbl. The high-res texture container of a weapon item is shared with the items it
// props (the remote charge excluded).
static void object_item_parse_table_file()
{
    inv_item_parse(-1);
    item_parse_table_shared(&Common_mempool, &Permanent_mempool, &Item_preload_pool, false, PLATFORM_PC, false, -1);
    for (int i = 0; i < Num_item_infos; i++)
    {
        if (Item_infos[i].high_res_srid == 0)
        {
            continue;
        }
        for (int prop = 0; prop < Item_infos[i].num_props; prop++)
        {
            item_info* prop_info = object_item_get_info(Item_infos[i].props[prop].prop_name);
            if (prop_info != nullptr && v_stricmp(prop_info->name, "remote_charge") != 0)
            {
                prop_info->high_res_srid = Item_infos[i].high_res_srid;
            }
        }
    }
}

// 0x00A18330 (:489): releases the preloaded item containers.
static void item_info_release_preloaded()
{
    for (int i = 0; i < Num_item_infos; i++)
    {
        if ((Item_infos[i].flags & 0x80000000) != 0)
        {
            const unsigned int srid = stream2_container_find(Item_infos[i].name);
            if (srid != 0)
            {
                stream2_release(srid);
            }
        }
    }
}

// ---- functions
// 0x00A3B710 (:1511)
// The gameplay part of an item entry: damage, props (attached meshes and their tags), touch script, physics and sounds.
void item_parse_table_element(xml_element* item, item_info* infop, mempool* pool)
{
    // Magic numbers: the default physics of an item.
    infop->restitution = 0.5f;
    infop->linear_damping = 0.1f;
    infop->angular_damping = 0.1f;
    infop->hitpoints = 0;
    infop->damage_flags = 0;
    infop->flags = 0;
    infop->touch = nullptr;
    infop->anim_set_name = nullptr;
    infop->friction = 1.0f;
    infop->high_res_material = nullptr;
    infop->special_flags.high_res_texture_requested = false;
    infop->special_flags.high_res_texture_loaded = false;
    infop->special_flags.high_res_texture_request_unload = false;
    xml_element* damage_info = xml_table_find_opt(item, "Damage_Info");
    if (damage_info != nullptr)
    {
        xml_table_get_req_int(infop->hitpoints, damage_info, "Hitpoints");
        if (infop->hitpoints < 1)
        {
            infop->hitpoints = 0;
        }
        xml_element* flags = xml_table_find_req(damage_info, "Flags");
        if (flags != nullptr)
        {
            infop->damage_flags |= xml_table_get_flags(Damage_info_flags, 6, flags, false);
        }
    }
    char buffer[512];
    xml_element* props = xml_table_find_opt(item, "Props");
    infop->num_props = 0;
    if (props != nullptr)
    {
        // Original: the six prop slots are not checked.
        for (xml_element* prop = xml_table_find_req(props, "Prop"); prop != nullptr; prop = xml_table_find_next(props, prop, "Prop"))
        {
            item_info::item_prop& item_prop = infop->props[infop->num_props];
            xml_table_get_req_string(buffer, sizeof(buffer), prop, "Name");
            const char* ext = cf_get_ext(buffer);
            item_prop.flags.offhand = 0;
            item_prop.prop_tag = nullptr;
            if (v_stricmp(ext, ".smeshx") == 0)
            {
                // A static mesh prop is named by its character mesh file.
                char mesh_name[64];
                cf_add_ext(mesh_name, sizeof(mesh_name), buffer, ".ccmesh_pc", false);
                item_prop.prop_name = (char*)pool->alloc((unsigned int)strlen(mesh_name) + 1, 1);
                strcpy(item_prop.prop_name, mesh_name);
            }
            else
            {
                item_prop.prop_name = (char*)pool->alloc((unsigned int)strlen(buffer) + 1, 1);
                strcpy(item_prop.prop_name, buffer);
            }
            if (xml_table_get_opt_string(buffer, sizeof(buffer), prop, "Attach_tag"))
            {
                if (v_stricmp(buffer, "(off-hand)") == 0)
                {
                    item_prop.flags.offhand = 1;
                }
                else
                {
                    item_prop.prop_tag = (char*)pool->alloc((unsigned int)strlen(buffer) + 1, 1);
                    strcpy(item_prop.prop_tag, buffer);
                }
            }
            item_prop.type = vanim_get_prop_type_from_name(xml_table_get_req_string_ref(prop, "Type"));
            infop->num_props++;
        }
    }
    infop->touch = nullptr;
    if (xml_table_get_opt_string(buffer, sizeof(buffer), item, "Touch_Script"))
    {
        int script = 0;
        for (; script < 13; script++)
        {
            if (v_stricmp(buffer, Item_touch_callback_names[script]) == 0)
            {
                break;
            }
        }
        if (script < 13)
        {
            infop->touch = Item_touch_callbacks[script];
        }
        else
        {
            // The original formats into a thread-local buffer.
            char message[600];
            sprintf(message, "Item touch script \"%s\" not found.", buffer);
            keen::debug::nativePrintArgs(message);
        }
    }
    if (!xml_table_get_opt_int(infop->spin_time, item, "Spin_Time"))
    {
        infop->spin_time = 5000; // magic number: milliseconds
    }
    if (!xml_table_get_opt_float(infop->touch_arg1, item, "Touch_Arg1"))
    {
        infop->touch_arg1 = 0.0f;
    }
    if (!xml_table_get_opt_float(infop->touch_arg2, item, "Touch_Arg2"))
    {
        infop->touch_arg2 = 0.0f;
    }
    if (xml_table_get_opt_string(buffer, sizeof(buffer), item, "Touch_Sound"))
    {
        audiolib_cue_get_id(buffer, &infop->pickup_snd);
    }
    else
    {
        infop->pickup_snd = -1;
    }
    if (xml_table_get_opt_string(buffer, sizeof(buffer), item, "Touch_Sound_2"))
    {
        audiolib_cue_get_id(buffer, &infop->pickup_snd_2);
    }
    else
    {
        infop->pickup_snd_2 = -1;
    }
    // The original reads the explosion name into a thread-local 0x400 byte buffer.
    char explosion[0x400];
    if (xml_table_get_opt_string(explosion, sizeof(explosion), item, "Explosion"))
    {
        infop->m_explosion_info = explosion_lookup(explosion);
    }
    if (!xml_table_get_opt_float(infop->mass, item, "Mass"))
    {
        infop->mass = 1.0f;
    }
    if (!xml_table_get_opt_int(infop->respawn_delay, item, "Respawn_Delay"))
    {
        infop->respawn_delay = -1;
    }
    // The original formats the foley errors into the thread-local buffer and does not print them.
    infop->m_foley_collision = -1;
    const char* collision_sound = xml_table_get_opt_string_ref(item, "Collision_Sound");
    if (collision_sound != nullptr)
    {
        audiolib_cue_get_id(collision_sound, &infop->m_foley_collision);
    }
    infop->cloth_foley = -1;
    const char* cloth_sound = xml_table_get_opt_string_ref(item, "Cloth_Sound");
    if (cloth_sound != nullptr)
    {
        audiolib_cue_get_id(cloth_sound, &infop->cloth_foley);
    }
    const char* safehouse_weapon = xml_table_get_opt_string_ref(item, "SafehouseWeapon");
    infop->safehouse_weapon = safehouse_weapon != nullptr ? inv_item_get_info(safehouse_weapon) : nullptr;
}

// 0x00A3BF60 (:1719): the character of an item with a character mesh (its mesh and rig, in this container).
void item_stream_load(unsigned int srid)
{
    item_info* info = object_item_get_info(stream2_get_container_name(srid));
    if (info == nullptr || info->cmesh_name == nullptr || character_find(info->name, info->srid) != nullptr)
    {
        return;
    }
    character_mesh* cmesh = character_mesh_find(info->cmesh_name, srid);
    char rig_name[64];
    cf_add_ext(rig_name, sizeof(rig_name), info->rig_name, ".rig_pc", false);
    character_load(info->name, srid, cmesh, anim_rig_find(rig_name), info->cmesh_name);
}

// 0x00A3C000 (:1753)
void item_stream_unload(unsigned int srid)
{
    item_info* info = object_item_get_info(stream2_get_container_name(srid));
    if (info == nullptr || info->cmesh_name == nullptr)
    {
        return;
    }
    character* cp = character_find(info->name, info->srid);
    if (cp != nullptr)
    {
        character_unload(cp);
    }
}

// The mesh header of an item's character or static mesh, or null when it is not loaded.
static const mesh_header_shared* item_high_res_get_mesh_header(const item_info* infop)
{
    if (infop->cmesh_name != nullptr)
    {
        const character* cp = character_find(infop->name, infop->srid);
        if (cp == nullptr || cp->m_data == nullptr)
        {
            return nullptr;
        }
        return &cp->m_data->m_shared_header;
    }
    if (infop->smesh_name == nullptr)
    {
        return nullptr;
    }
    const static_mesh* smesh = static_mesh_find(infop->smesh_name, infop->srid);
    return smesh != nullptr ? &smesh->m_shared_header : nullptr;
}

// 0x00A648B0 (:1779)
// The high-res container of an item loaded: its standalone material (named as the container) replaces the item mesh's
// render materials, slot by slot. Original: an item whose mesh is not loaded ends the walk, so later items sharing the
// container keep their low-res materials.
void item_high_res_stream_load(unsigned int srid)
{
    material* mat = material_find(stream2_get_container_name(srid), srid);
    if (mat == nullptr)
    {
        return;
    }
    for (int i = 0; i < Num_item_infos; i++)
    {
        item_info* infop = &Item_infos[i];
        if (infop->high_res_srid != srid)
        {
            continue;
        }
        infop->special_flags.high_res_texture_loaded = true;
        infop->high_res_material = mat;
        if ((infop->flags & 0x4000) != 0) // Magic number: item_info flag 14, a large weapon
        {
            Num_large_high_res_weapons_loaded++;
        }
        const mesh_header_shared* header = item_high_res_get_mesh_header(infop);
        if (header == nullptr)
        {
            return;
        }
        rl_material_map* map = header->m_material_map.to_pointer();
        if (map == nullptr)
        {
            return;
        }
        int count = map->get_num_materials();
        const int high_res_count = mat->m_material_map.to_pointer()->get_num_materials();
        if (high_res_count < count)
        {
            count = high_res_count;
        }
        for (int m = 0; m < count; m++)
        {
            rl_material_library_override_handle(map->get_material(m), mat->m_materials.to_pointer()[m].to_pointer());
        }
    }
}

// 0x00A649E0 (:1827)
// The high-res container unloaded: the item meshes' materials lose their overrides. Original: as in the load, an item
// whose mesh is not loaded ends the walk.
void item_high_res_stream_unload(unsigned int srid)
{
    for (int i = 0; i < Num_item_infos; i++)
    {
        item_info* infop = &Item_infos[i];
        if (infop->high_res_srid != srid)
        {
            continue;
        }
        infop->special_flags.high_res_texture_loaded = false;
        infop->high_res_material = nullptr;
        if ((infop->flags & 0x4000) != 0) // Magic number: item_info flag 14, a large weapon
        {
            Num_large_high_res_weapons_loaded--;
        }
        const mesh_header_shared* header = item_high_res_get_mesh_header(infop);
        if (header == nullptr)
        {
            return;
        }
        rl_material_map* map = header->m_material_map.to_pointer();
        if (map == nullptr)
        {
            return;
        }
        for (int m = 0; m < map->get_num_materials(); m++)
        {
            rl_material_library_override_handle(map->get_material(m), nullptr);
        }
    }
}

// 0x00A18D30 (:2421)
item_info* object_item_get_info(const char* name)
{
    if (name == nullptr)
    {
        return nullptr;
    }
    for (int i = 0; i < Num_item_infos; i++)
    {
        if (Item_infos[i].name != nullptr && v_stricmp(Item_infos[i].name, name) == 0)
        {
            return &Item_infos[i];
        }
    }
    return nullptr;
}

// 0x00A18D80 (:2439)
item_info* object_item_get_info(checksum_stri name_crc)
{
    for (int i = 0; i < Num_item_infos; i++)
    {
        if (crc_stri(Item_infos[i].name, 0) == name_crc.get_crc())
        {
            return &Item_infos[i];
        }
    }
    return nullptr;
}

// 0x00A18DE0 (:2450)
item_info* object_item_get_info(unsigned int srid)
{
    if (srid == 0)
    {
        return nullptr;
    }
    for (int i = 0; i < Num_item_infos; i++)
    {
        if (Item_infos[i].srid == srid)
        {
            return &Item_infos[i];
        }
    }
    return nullptr;
}

// 0x00A18E20 (:2468)
int object_item_get_info_index(const char* name)
{
    for (int i = 0; i < Num_item_infos; i++)
    {
        if (Item_infos[i].name != nullptr && v_stricmp(Item_infos[i].name, name) == 0)
        {
            return i;
        }
    }
    return -1;
}

// 0x00A91780 (:4192)
void object_item_system_init()
{
    if (!Object_item_init)
    {
        Object_item_init = true;
        Num_item_infos = 0;
        object_item_parse_table_file();
        inv_item_system_init();
        Item_physical_material = rfg_get_physical_material_index("Steel-Normal");
        Item_effect_material = rfg_get_effect_material_index(Item_physical_material);
    }
}

// ---- the item object (player_minimal.md P4)

// ---- file-static globals
// const float COVER_NODE_Y_TOLERANCE (0x0131185C) is used here too; placed in human_move.cpp.
// const float COVER_NODE_MAX_NAVMESH_ATTACH_DIST (0x01311864) is used here too; placed in human_spinebend.cpp.
// const float MIN_UPDATE_TAG_POS_DIST_SQ (0x01311C74) is used here too; placed in player.cpp.
static int Salvage_touch_delay_ms = 1000; // 0x01694584
static tweak_table_item Tweak_table_item_Salvage_touch_delay_ms("Salvage_touch_delay_ms", &Salvage_touch_delay_ms); // 0x0303CAAC
static float Item_linear_impulse_scalar = 0.002f; // 0x01694608
static float Item_angular_impulse_scalar = 0.0002f; // 0x0169460C
static bool All_items_cast_shadows = true; // 0x01694610
// Salvage_material_data (0x0301AE4C) is in salvage.cpp, which fills it from salvage.xtbl.
// farray<object*,16384> Object_update_render_list (0x0169D028) is used here too; placed in object.cpp.
// player* Player (0x03023874) is used here too; placed in player.cpp.
static bool Shiny_ammo_crates; // 0x0302F137 (.bss, no writer)
static object_pool<item,512,1> itempool; // 0x033FC980
// read from rfg.exe: pool usage 0x28A40 bytes, 512 items. rfg.pdb places it in human.cpp; defined here with the item
// class (its registration functions are in this file).
obj_class_instantiator item_instantiator("item", alloc_item, types_item, bpo_index_item, classname_item, item::pool_remaining, 0x28A40, 512); // 0x0306654C

// ---- internal functions (forward)
static void create_physics_object(item* ip); // 0x00A64340 (:1347)

// 0x00A05950 (global_event_data.h:269)
player_gets_salvage_event::player_gets_salvage_event()
    : event_data(EVENT_PLAYER_GETS_SALVAGE)
{
}

// ---- class registration (:2805)
// 0x006E17C0 (.h:165)
const object_type item::get_type_static()
{
    return OT_ITEM;
}

// 0x006E17D0 (.h:165)
const object_sub_type item::get_subtype_static()
{
    return OT_SUB_UNDEFINED;
}

// 0x00A05E10 (.h:165)
const char* item::get_classname_static()
{
    return "item";
}

// 0x00756BB0 (.h:165)
int item::get_bpo_index_static()
{
    return 0;
}

// 0x00A18FF0 (.h:165)
const char* item::get_classname() const
{
    return "item";
}

// 0x00A18F50 (:2805)
void types_item(signed char& type, signed char& subtype)
{
    type = OT_ITEM;
    subtype = OT_SUB_UNDEFINED;
}

// 0x00A18F60 (:2805)
int bpo_index_item()
{
    return 0;
}

// 0x00A18F70 (:2805)
const char* classname_item()
{
    return "item";
}

// 0x00A3C100 (:2805)
int item::pool_remaining()
{
    return itempool.get_free();
}

// 0x00A65180 (:2805)
object* alloc_item()
{
    return itempool.alloc(0);
}

// ---- touch scripts

// 0x00A3B340 (:669)
// The local player is asked to pick the item up (unless it carries a non-inventory item); a non-inventory item only
// when the player stands with it about half a metre ahead (0.7 squared, from 0.3 m up). Never used up by the touch.
static bool item_touch_ask_pickup(human* hp, item* obj_itemp, bool& play_secondary_sound, bool& ask_pickup)
{
    ask_pickup = false;
    if (Player == nullptr || hp->get_handle() != Player->get_handle())
    {
        return false;
    }
    if (hp->get_obj_type() == OT_HUMAN && hp->get_sub_type() == OT_SUB_HUMAN_PLAYER && hp->equipped_inv_item != nullptr && hp->equipped_inv_item->info != nullptr &&
        hp->equipped_inv_item->info->w_info != nullptr && hp->equipped_inv_item->info->w_info->flags.non_inventory)
    {
        return false;
    }
    const inv_item_info* info = inv_item_get_info(obj_itemp->info->name);
    if (info != nullptr && info->w_info != nullptr && info->w_info->flags.non_inventory)
    {
        if (hp->cur_stance != HST_STAND)
        {
            return false;
        }
        const vector ahead = hp->orient.fvec.copy_flatten_normalize();
        const vector feet(hp->pos.x, hp->pos.y + 0.3f, hp->pos.z); // Magic number: 0.3 m up
        const vector to_item(obj_itemp->pos.x - (ahead.x * 0.5f + feet.x), obj_itemp->pos.y - (ahead.y * 0.5f + feet.y), obj_itemp->pos.z - (ahead.z * 0.5f + feet.z)); // Magic number: 0.5 m ahead
        if (0.7f <= to_item.mag_squared()) // Magic number: 0.7 squared metres
        {
            return false;
        }
        if (hp->cur_stance != HST_STAND)
        {
            return false;
        }
    }
    ask_pickup = true;
    return false;
}

// 0x00ABA110 (:722)
// A living player (not spectating, manning a turret, holding a non-inventory item or on a ladder) takes the item's
// inventory item (a weapon's own), unless it is full of a non-weapon item or the multiplayer mode refuses. A weapon it
// does not have (not the sledgehammer; not while scoped or fine aiming) is taken when there is room (else the player
// is asked); a weapon it has gives its ammo (the local player is told when the reserve is full). In multiplayer only
// the server gives. The local player sees the pickup. Used up unless a non-respawning item gave nothing.
static bool item_touch_give_item(human* hp, item* obj_itemp, bool& play_secondary_sound, bool& ask_pickup)
{
    static timestamp reserve_ammo_full_message_timestamp; // (initialized once, never used after)
    ask_pickup = false;
    object* objp = World.object_from_handle(hp->get_handle());
    if (objp == nullptr || objp->get_sub_type() != OT_SUB_HUMAN_PLAYER)
    {
        return false;
    }
    player* pp = hp->get_obj_type() == OT_HUMAN && hp->get_sub_type() == OT_SUB_HUMAN_PLAYER ? (player*)hp : nullptr;
    // Original: the player is read through without a null check.
    if (pp->cur_state == HS_DEAD || hp->cur_state == HS_SPECTATOR || human_combat_get_manned_turret(pp) != nullptr || player_has_non_inventory(pp) || pp->hflags.is_on_ladder)
    {
        return false;
    }
    weapon* wp = World.object_cast<weapon>(obj_itemp);
    const inv_item_info* item_info = wp != nullptr ? wp->w_info->weapon_inv_item_info : inv_item_get_info(obj_itemp->info->name);
    if (item_info == nullptr)
    {
        return false;
    }
    const inventory_item* owned = human_has_item(hp, item_info);
    if (owned != nullptr && owned->weapon_handle == 0xFFFFFFFF && item_info->max_item <= owned->count)
    {
        return false;
    }
    if (Multiplayer && !multi_gameplay_mode_item_touched(pp, obj_itemp))
    {
        return false;
    }
    bool new_item = false;
    int previous_reserve = 0;
    int previous_rounds = 0;
    if ((player*)hp == pp && wp != nullptr)
    {
        const weapon_class_type weapon_class = wp->w_info->weapon_class;
        if (weapon_class == WEAPON_CLASS_SLEDGEHAMMER)
        {
            return false;
        }
        if (weapon_class == WEAPON_CLASS_SINGULARITY_BOMB && wp->item_flags.preplaced && upgrade_has_singularity_increase() &&
            wp->get_rounds_total() == (int)((unsigned int)wp->w_info->magazine_start_num * (unsigned int)wp->w_info->magazine_size))
        {
            wp->add_ammo(2); // Magic number: the upgrade's 2 more bombs
        }
        const inventory_item* has = human_has_item(hp, item_info);
        if (has == nullptr)
        {
            new_item = true;
            if (player_is_scoped(pp) || pp->zoom_state == PZS_FINE_AIM)
            {
                return false;
            }
            if (!player_has_room_in_inventory(pp))
            {
                ask_pickup = true;
            }
        }
        else
        {
            weapon* own = World.object_from_handle<weapon>(has->weapon_handle);
            if (own->has_unlimited_ammo() && own->is_fully_loaded())
            {
                return false;
            }
            if (own->get_max_rounds() - own->get_rounds_in_magazine() <= own->get_rounds_in_reserve())
            {
                // The reserve is full: the local player is told (not twice in a row, not without ammo, scoped or
                // fine aiming).
                if (!object_is_local_player(hp) || obj_itemp->item_flags.touched_last_frame || !own->uses_ammo() || player_is_scoped(pp) || pp->zoom_state == PZS_FINE_AIM)
                {
                    return false;
                }
                item_pickup_info message;
                message.item_name = own->w_info->icon_name;
                message.num_picked_up = -1;
                message.total_val = 0;
                hud_ui_add_item_pickup_message(&message, 1);
                return false;
            }
            previous_reserve = own->get_rounds_in_reserve();
            previous_rounds = own->get_rounds_in_magazine() + previous_reserve;
            play_secondary_sound = true;
        }
    }
    if (Multiplayer && !multi_is_server())
    {
        return false;
    }
    int max_rounds = 1;
    int rounds = 1;
    if (wp != nullptr)
    {
        max_rounds = wp->get_max_rounds();
        rounds = wp->get_rounds_total();
    }
    gamelog_store_current_loadout(hp);
    inventory_item* iip = human_add_item_to_inventory(hp, obj_itemp, true, false, true);
    if (iip == nullptr)
    {
        return false;
    }
    ask_pickup = false;
    gamelog_maybe_record_loadout_change(hp, GLLCT_WEAPON_PICKUP);
    if (object_is_local_player(hp->get_handle()))
    {
        if (!new_item)
        {
            const weapon* own = World.object_from_handle<weapon>(iip->weapon_handle);
            if (own != nullptr)
            {
                if (own->w_info->weapon_class == WEAPON_CLASS_SLEDGEHAMMER)
                {
                    return false;
                }
                int gained = (own->get_rounds_in_magazine() - previous_rounds) + own->get_rounds_in_reserve();
                if (0 < gained && !player_is_scoped(pp))
                {
                    if (own->has_unlimited_ammo())
                    {
                        gained = gained + (previous_reserve - own->get_rounds_in_reserve());
                    }
                    item_pickup_info message;
                    message.item_name = own->w_info->icon_name;
                    message.total_val = 0;
                    message.num_picked_up = gained;
                    hud_ui_add_item_pickup_message(&message, 1);
                    return true;
                }
            }
            return (wp != nullptr && wp->item_flags.respawn) || 0 < rounds;
        }
        item_pickup_info message;
        message.num_picked_up = rounds == max_rounds ? -1 : rounds;
        message.item_name = item_info->w_info->icon_name;
        message.total_val = 0;
        hud_ui_add_item_pickup_message(&message, 1);
    }
    if (new_item)
    {
        return true;
    }
    return (wp != nullptr && wp->item_flags.respawn) || 0 < rounds;
}

// 0x00A64220 (:931)
// A safehouse ammo box: in single player, the local player (alive, not spectating) who has the box's weapon gets its
// magazine and reserve filled (the hud shows it when that changed anything) and the box is used; without the weapon the
// player is asked to pick it up.
static bool item_touch_give_item_safehouse(human* hp, item* obj_itemp, bool& play_secondary_sound, bool& ask_pickup)
{
    ask_pickup = false;
    if (Multiplayer || hp == nullptr || hp != Player)
    {
        return false;
    }
    // Original: the player cast is read without a null check (hp is the player here).
    const player* pp = hp->get_obj_type() == OT_HUMAN && hp->get_sub_type() == OT_SUB_HUMAN_PLAYER ? (const player*)hp : nullptr;
    if (pp->cur_state == HS_DEAD || hp->cur_state == HS_SPECTATOR)
    {
        return false;
    }
    const inventory_item* iip = human_has_item(hp, obj_itemp->info->safehouse_weapon);
    if (iip == nullptr)
    {
        ask_pickup = true;
        return false;
    }
    weapon* wp = World.object_from_handle<weapon>(iip->weapon_handle);
    const int previous = wp->get_rounds_in_reserve() + wp->get_rounds_in_magazine();
    wp->fill_magazine();
    wp->set_rounds_in_reserve(wp->get_max_rounds());
    if (previous != wp->get_rounds_in_reserve() + wp->get_rounds_in_magazine())
    {
        item_pickup_info message;
        message.item_name = wp->w_info->icon_name;
        message.num_picked_up = -1;
        message.total_val = 0;
        hud_ui_add_item_pickup_message(&message, 1);
    }
    return true;
}

// The money pickup of item_touch_give_money and item_touch_give_money2 (new helper: rfg.exe has the same code in both,
// probably inlined; the instructions are identical after the amount): the local player's
// running pickup message keeps the dollars and cents picked up so far in its two data words and shows the new total;
// the cash display shows for 25 s. In multiplayer only the server removes the item. Anyone else just gets the cash.
static bool item_touch_give_cash(human* hp, item* obj_itemp, float amount)
{
    if (Player == nullptr || hp->get_handle() != Player->get_handle())
    {
        hp->cash = (float)((double)hp->cash + (double)amount);
        return true;
    }
    player* pp = (player*)hp;
    const unsigned int dollars_before = (unsigned int)(uintptr_t)hud_message_get_data1(pp->hud_message);
    const unsigned int cents_before = (unsigned int)(uintptr_t)hud_message_get_data2(pp->hud_message);
    hp->cash = (float)((double)hp->cash + (double)amount);
    hud_display_cash(true, 25000); // Magic number: 25 s
    // Magic number: 0.01 dollars per cent (a double in rfg.exe).
    const float total = (float)(((double)cents_before * 0.01 + (double)dollars_before) + (double)amount);
    const unsigned int dollars = (unsigned int)(long long)total;
    sprintf(Item_touch_message, "You picked up $%.2f", (double)total);
    const unsigned int cents = (unsigned int)(long long)(((double)total - (double)dollars) / 0.01);
    pp->hud_message = hud_message_change_message(pp->hud_message, Item_touch_message, 4.0f, (void*)(uintptr_t)dollars, (void*)(uintptr_t)cents); // Magic number: 4 s
    if (!Multiplayer || multi_is_server())
    {
        World.destroy_object(obj_itemp, false);
    }
    return true;
}

// 0x00A18410 (:995), the "give_rand_money" script
// A random amount between the item's touch arguments (else the item type's), rounded up.
static bool item_touch_give_money(human* hp, item* obj_itemp, bool& play_secondary_sound, bool& ask_pickup)
{
    ask_pickup = false;
    float amount = fl_rand_range(obj_itemp->touch_arg1_override, obj_itemp->touch_arg2_override);
    if (amount == 0.0f)
    {
        amount = fl_rand_range(obj_itemp->info->touch_arg1, obj_itemp->info->touch_arg2);
    }
    amount = (float)ceil((double)amount);
    return item_touch_give_cash(hp, obj_itemp, amount);
}

// 0x00A18670 (:1039), the "give_money" script
// The item's first touch argument (else the item type's).
static bool item_touch_give_money2(human* hp, item* obj_itemp, bool& play_secondary_sound, bool& ask_pickup)
{
    ask_pickup = false;
    float amount = obj_itemp->touch_arg1_override;
    if (amount == 0.0f)
    {
        amount = obj_itemp->info->touch_arg1;
    }
    return item_touch_give_cash(hp, obj_itemp, amount);
}

// 0x00A18860 (:1082)
// A hurt human is healed to its maximum (the local player is told); the item goes (in multiplayer on the server only).
static bool item_touch_give_full_health(human* hp, item* obj_itemp, bool& play_secondary_sound, bool& ask_pickup)
{
    ask_pickup = false;
    const float max_hit_points = (float)hp->max_hit_points;
    if (!(max_hit_points > hp->hit_points)) // as rfg.exe's COMISD/JBE: a NaN hit point count gives nothing
    {
        return false;
    }
    hp->health_restore_hit_points = (float)((double)hp->health_restore_hit_points + (double)max_hit_points);
    if (hp->health_restore_hit_points > max_hit_points)
    {
        hp->health_restore_hit_points = max_hit_points;
    }
    hp->hit_points = hp->health_restore_hit_points;
    hp->health_restore_min_hit_points = 0.0f;
    if (Player != nullptr && hp->get_handle() == Player->get_handle())
    {
        hud_message_add("You now have full health", 4.0f, nullptr, nullptr, nullptr); // Magic number: 4 s
    }
    if (obj_itemp != nullptr && (!Multiplayer || multi_is_server()))
    {
        World.destroy_object(obj_itemp, false);
    }
    return true;
}

// 0x00A18950 (:1111)
// A hurt human gets touch_arg1 percent of its maximum (the local player is told). Original: the item is not removed
// here, unlike the full health pickup.
static bool item_touch_give_health_percent(human* hp, item* obj_itemp, bool& play_secondary_sound, bool& ask_pickup)
{
    ask_pickup = false;
    const float max_hit_points = (float)hp->max_hit_points;
    if (!(max_hit_points > hp->hit_points)) // as rfg.exe's COMISD/JBE: a NaN hit point count gives nothing
    {
        return false;
    }
    // Magic number: 0.01 per percent (a double in rfg.exe; the fraction is rounded to a float before the multiply).
    const float fraction = (float)((double)obj_itemp->info->touch_arg1 * 0.01);
    hp->health_restore_hit_points = (float)((double)fraction * (double)max_hit_points + (double)hp->health_restore_hit_points);
    if (hp->health_restore_hit_points > max_hit_points)
    {
        hp->health_restore_hit_points = max_hit_points;
    }
    hp->hit_points = hp->health_restore_hit_points;
    hp->health_restore_min_hit_points = 0.0f;
    if (Player != nullptr && hp->get_handle() == Player->get_handle())
    {
        hud_message_add("Your health has increased", 4.0f, nullptr, nullptr, nullptr); // Magic number: 4 s
    }
    return true;
}

// 0x00A3B510 (:1134)
// The local player gets the salvage's value (by material: ore +1 with the ore bonus upgrade; metal +1 one time in
// four; the item's touch_arg1 when the material pays nothing), 30000 at most; the hud shows the pickup, the item goes.
static bool item_touch_give_salvage(human* hp, item* obj_itemp, bool& play_secondary_sound, bool& ask_pickup)
{
    ask_pickup = false;
    if ((obj_itemp->info->touch_arg1 <= 0.0f && obj_itemp->salvage_material == INVALID_SALVAGE_MATERIAL) || Player == nullptr || hp->get_handle() != Player->get_handle())
    {
        return false;
    }
    int value;
    const salvage_material_type material = obj_itemp->salvage_material;
    if (material != INVALID_SALVAGE_MATERIAL && (int)material < 3)
    {
        if (material == SALVAGE_MATERIAL_ORE && upgrade_has_ore_bonus())
        {
            value = Salvage_material_data[SALVAGE_MATERIAL_ORE].value + 1;
        }
        else
        {
            value = Salvage_material_data[material].value;
        }
        if (value == 0)
        {
            value = (int)obj_itemp->info->touch_arg1;
        }
    }
    else
    {
        value = (int)obj_itemp->info->touch_arg1;
    }
    // Magic number: metal salvage pays one more one time in four.
    if (obj_itemp->salvage_material == SALVAGE_MATERIAL_METAL && int_rand_range(0, 3) == 0)
    {
        value++;
    }
    const vector player_pos = gamelog_get_human_pos(Player);
    const int duration = gamelog_get_game_duration();
    const char difficulty = gamelog_get_difficulty();
    const int locale_id = gamelog_get_locale();
    const char pct_game_complete = gamelog_get_pct_complete();
    const int time_played = gamelog_get_time_played();
    const int time = gamelog_get_logging_time();
    log_event_salvage(gamelog_get_player_checksum(), time, (char)value, time_played, pct_game_complete, locale_id, difficulty, duration, player_pos);
    (Player != nullptr ? &Player->meta_data : nullptr)->add_salvage(value, true); // inlined
    item_pickup_info pickup;
    pickup.item_name = "ui_hud_icon_salvage";
    pickup.num_picked_up = value;
    pickup.total_val = (Player != nullptr ? &Player->meta_data : nullptr)->get_salvage(); // inlined
    hud_ui_add_item_pickup_message(&pickup, 1);
    World.destroy_object(obj_itemp, false);
    player_gets_salvage_event ev;
    ev.salvage_val = pickup.total_val;
    gem_post_event(ev);
    return true;
}

// 0x00A3B6C0 (:1191)
// A player touched a multiplayer backpack nobody wears (multiplayer.md MP10: multi_object_backpack::event_player_touched
// decides). Never used up by the touch.
static bool item_touch_give_backpack(human* hp, item* obj_itemp, bool& play_secondary_sound, bool& ask_pickup)
{
    ask_pickup = false;
    if (hp == nullptr || hp->get_sub_type() != OT_SUB_HUMAN_PLAYER)
    {
        return false;
    }
    // Original: the cast is not checked for null.
    multi_object_backpack* backpack = obj_itemp != nullptr && obj_itemp->get_obj_type() == OT_ITEM && obj_itemp->get_sub_type() == OT_SUB_ITEM_MULTI_BACKPACK ? (multi_object_backpack*)obj_itemp : nullptr;
    if (backpack->get_player_attached_to() != nullptr)
    {
        return false;
    }
    return backpack->event_player_touched(hp->get_obj_type() == OT_HUMAN ? (player*)hp : nullptr, ask_pickup);
}

// 0x00A18A40 (:1222)
// The local player's touch raises the tech level by touch_arg1 (a random amount up to touch_arg2 when that is set),
// unless it is at its maximum. Never removes the item itself.
static bool item_touch_increase_tech_level(human* hp, item* obj_itemp, bool& play_secondary_sound, bool& ask_pickup)
{
    ask_pickup = false;
    if (hp == nullptr || hp != Player)
    {
        return false;
    }
    // rfg.exe compares the two x87 results, the first one stored as a double.
    if ((double)gfm_get_tech_level() == (double)gfm_get_tech_level_max())
    {
        return false;
    }
    const item_info* info = obj_itemp->info;
    float delta = info->touch_arg1;
    if (info->touch_arg2 > 0.0f)
    {
        delta = fl_rand_range(info->touch_arg1, info->touch_arg2);
    }
    gfm_change_tech_level(delta);
    return true;
}

// 0x00A64310 (:1260)
// The blast from the past bomb item: the local player's touch arms its car (blast_from_the_past_touch_item).
static bool item_touch_blast_from_the_past(human* hp, item* obj_itemp, bool& play_secondary_sound, bool& ask_pickup)
{
    ask_pickup = false;
    if (hp == nullptr || hp != Player)
    {
        return false;
    }
    return blast_from_the_past_touch_item(hp, obj_itemp->get_handle());
}

// 0x00A18AF0 (:1282)
// The local player finds a fuel cell: the stat counts it, the hud shows the count (up to 75), the item goes.
static bool item_touch_fuel_cell(human* hp, item* obj_itemp, bool& play_secondary_sound, bool& ask_pickup)
{
    ask_pickup = false;
    const tracked_stat* stat = nullptr;
    if (Player == nullptr || hp->get_handle() != Player->get_handle())
    {
        return false;
    }
    stats_increment_by_one(STATS_FUEL_CELLS_FOUND);
    stats_get_stat(stat, STATS_FUEL_CELLS_FOUND);
    const int found = stats_get_stat_value(stat);
    // Magic number: 75 fuel cells in the game.
    if (found < 0x4C)
    {
        wchar_t count[10];
        wchar_t msg[64];
        sprintf(count, L"%d", found);
        sprintf(msg, localization_get_display_string_req("HM_FUEL_CELLS_FOUND", nullptr), count);
        hud_message_add_minor(msg, -1.0f, false);
    }
    World.destroy_object(obj_itemp, false);
    return true;
}

// ---- internal functions

// 0x00A18380 (:514)
// The floor raycast of a spinning item ignores humans, items, debris and, in multiplayer, local movers.
static bool stick_to_floor_raycast_callback(object* obj, void* param)
{
    switch (obj->get_obj_type())
    {
    case OT_HUMAN:
    case OT_ITEM:
    case OT_DEBRIS:
        return false;
    case OT_MOVER:
        if (obj->get_sub_type() != 0 || !Multiplayer || obj->remote_obj_flags.remote_object)
        {
            return true;
        }
        return false;
    default:
        return true;
    }
}

// 0x00A90A00 (:540). LTCG: itemp in EDI, new_pos in ESI.
// Drops an item onto the floor below it (up to 75 m), 10 cm above.
static void object_item_stick_to_floor(const item* itemp, vector* new_pos)
{
    if (itemp == nullptr || new_pos == nullptr)
    {
        return;
    }
    vector bmin;
    vector bmax;
    itemp->get_bbox(bmin, bmax);
    const float pos_y = itemp->pos.y;
    raycast_input in;
    in.radius = 0.0f;
    // Magic numbers: from 5 cm above the box center down to 75 m below it.
    const float half_height = (float)(((double)bmax.y - (double)bmin.y) * 0.5);
    const float up = (float)((double)half_height + 0.05);
    in.start.x = (float)((double)itemp->pos.x + (double)UP_VECTOR.x * up);
    in.start.y = (float)((double)itemp->pos.y + (double)UP_VECTOR.y * up);
    in.start.z = (float)((double)itemp->pos.z + (double)UP_VECTOR.z * up);
    const float down = (float)((double)half_height + 75.0);
    in.filter_params = nullptr;
    in.m_filter_info = COLLIDE_RAY_DEFAULT;
    in.m_hit_collector_type = raycast_input::COLLECT_CLOSEST_HIT;
    in.m_flags = 0;
    raycast_output out;
    in.end.x = (float)((double)in.start.x - (double)UP_VECTOR.x * down);
    in.end.y = (float)((double)in.start.y - (double)UP_VECTOR.y * down);
    in.end.z = (float)((double)in.start.z - (double)UP_VECTOR.z * down);
    in.filter_callback = stick_to_floor_raycast_callback;
    if (havok_raycast(in, out))
    {
        *new_pos = out.m_hits[0].hit_pos;
        new_pos->y = (pos_y - bmin.y) + new_pos->y;
        // Magic number: 10 cm above the floor.
        new_pos->x = (float)((double)UP_VECTOR.x * 0.1 + (double)new_pos->x);
        new_pos->y = (float)((double)new_pos->y + (double)UP_VECTOR.y * 0.1);
        new_pos->z = (float)((double)new_pos->z + (double)UP_VECTOR.z * 0.1);
    }
}

// 0x00A90CF0 (:573). LTCG: itemp in EAX.
// Pickups spin (one turn per spin time) standing on the floor; salvage turns in place.
static void object_item_spin(item* itemp)
{
    // the phase of the turn, offset per item (200 ms per handle)
    const int spin_time = itemp->info->spin_time;
    const int phase = (int)(itemp->get_handle() * 200u + Frametime_total_milliseconds) % spin_time;
    const float angle = (float)((double)phase / (double)spin_time * 6.2831855);
    const vector fvec((float)sin((double)angle), 0.0f, (float)cos((double)angle));
    matrix orient;
    orient.make_uvec(fvec, UP_VECTOR);
    vector pos = itemp->pos;
    if (!Multiplayer || multi_is_server())
    {
        if (itemp->salvage_material == INVALID_SALVAGE_MATERIAL)
        {
            object_item_stick_to_floor(itemp, &pos);
            itemp->update_pos_and_orient(pos, orient, true);
            queue_render_update(itemp, itemp->obj_flags, 4); // inlined (render flag bit 3 of the byte)
            return;
        }
    }
    else if (itemp->salvage_material == INVALID_SALVAGE_MATERIAL)
    {
        itemp->update_pos_and_orient(pos, orient, true);
        queue_render_update(itemp, itemp->obj_flags, 4); // inlined
        return;
    }
    itemp->orient.rotate_around_local_y((float)((double)(float)((double)Frametime_real * 1000.0 * 6.2831855) / (double)(float)itemp->info->spin_time));
    itemp->update_pos_and_orient(itemp->pos, itemp->orient, true);
    queue_render_update(itemp, itemp->obj_flags, 4); // inlined
}

// 0x00A183D0 (:608). LTCG: itemp in EDI.
// The body becomes keyframed (motion type 4) and stops.
static void object_item_prepare_to_spin(item* itemp)
{
    hkpRigidBody* rb = havok_body_get_pointer(itemp->havok_handle);
    if (rb != nullptr)
    {
        // Magic number: motion type 4, keyframed.
        havok_body_set_motion_type(itemp->havok_handle, 4);
        havok_body_set_angular_velocity(rb, ZERO_VECTOR);
        havok_body_set_linear_velocity(rb, ZERO_VECTOR);
    }
}

// 0x00A64340 (:1347). LTCG: ip in ESI.
// The rigid body of a streamed item: a box from its local box (cloth items: a 20 cm cube), centered, with its
// bounding radius, mass (the override or the item's), collision model (the pickup mesh when it lies alone, else the
// static mesh), physics parameters, quality (bullet for projectiles and thrown weapons) and auto removal level
// (salvage 2, dropped weapons 3); it starts at the velocity of its last move and sleeps unless simulated.
static void create_physics_object(item* ip)
{
    if (ip->havok_handle != 0xFFFFFFFF || !ip->obj_flags.streamed)
    {
        return;
    }
    vector bmin;
    vector bmax;
    ip->get_local_bbox(bmin, bmax);
    if (ip->cloth_sim != nullptr)
    {
        // Magic numbers: a 20 cm cube.
        bmin.set(-0.1f, -0.1f, -0.1f);
        bmax.set(0.1f, 0.1f, 0.1f);
    }
    physics_create_info pci;
    pci.size.set((float)((double)bmax.x - (double)bmin.x), (float)((double)bmax.y - (double)bmin.y), (float)((double)bmax.z - (double)bmin.z));
    pci.pos = ip->pos;
    pci.orient = ip->orient;
    // the box center, each sum rounded before the half
    pci.local_pos.set((float)((double)(float)((double)bmax.x + (double)bmin.x) * 0.5), (float)((double)(float)((double)bmax.y + (double)bmin.y) * 0.5),
                      (float)((double)(float)((double)bmax.z + (double)bmin.z) * 0.5));
    pci.local_orient.make_identity();
    pci.radius = ip->get_bounding_radius();
    pci.physical_material = Item_physical_material;
    pci.effect_material = Item_effect_material;
    pci.mass = 0.0f < ip->mass_override ? ip->mass_override : ip->info->mass;
    item_calc_collision_params(pci, ip->info, bmin, bmax);
    const bool loaded = ip->rdd.is_loaded();
    const bool pickup = loaded && ip->get_parent() == 0xFFFFFFFF && ip->rdd.get_data()->pickup_smesh_instance_p != nullptr;
    if (ip->char_inst == nullptr || pickup)
    {
        item::resource_dependent_data* data = ip->rdd.get_data();
        static_mesh* smesh = pickup ? data->pickup_smesh : data->smesh;
        pci.cm_index = item_smesh_get_collision_model(smesh, pci);
        if (ip->cloth_sim != nullptr)
        {
            if (pci.cm_index == 0xFFFFFFFF)
            {
                goto params;
            }
            // a cloth item with a collision model uses its real box after all
            ip->get_local_bbox(bmin, bmax);
            pci.size.set((float)((double)bmax.x - (double)bmin.x), (float)((double)bmax.y - (double)bmin.y), (float)((double)bmax.z - (double)bmin.z));
            item_calc_collision_params(pci, ip->info, bmin, bmax);
        }
    }
    if (pci.cm_index != 0xFFFFFFFF)
    {
        // Magic number: flag 2 cleared for collision models.
        pci.flags &= ~2;
        pci.shape_type = HAVOK_SHAPE_TYPE_COLLISION_MODEL;
    }
params:
    pci.friction = ip->info->friction;
    pci.collision_type = ip->collision_type;
    pci.restitution = ip->info->restitution;
    pci.linear_damping = ip->info->linear_damping;
    pci.angular_damping = ip->info->angular_damping;
    pci.obj_ptr = ip;
    const object_sub_type sub_type = ip->get_sub_type();
    if (sub_type == OT_SUB_ITEM_PROJECTILE)
    {
        const projectile* pp = (const projectile*)ip;
        if (ip->get_obj_type() == OT_ITEM && pp->w_info != nullptr)
        {
            // Magic number: 100 m/s above the projectile's speed.
            pci.max_linear_vel = (float)((double)pp->w_info->projectile_info.max_speed + 100.0);
        }
        pci.simulation_quality_type = HK_COLLIDABLE_QUALITY_BULLET;
    }
    else if (sub_type == OT_SUB_ITEM_WEAPON && ((weapon*)ip)->w_info->weapon_class == WEAPON_CLASS_THROWN)
    {
        pci.simulation_quality_type = HK_COLLIDABLE_QUALITY_BULLET;
    }
    else
    {
        pci.simulation_quality_type = HK_COLLIDABLE_QUALITY_MOVING;
    }
    unsigned char auto_remove = 0;
    if (!ip->item_flags.preplaced && !ip->special_flags.is_ammo_box)
    {
        if (ip->salvage_material != INVALID_SALVAGE_MATERIAL)
        {
            auto_remove = 2;
        }
        else if (sub_type == OT_SUB_ITEM_WEAPON)
        {
            auto_remove = 3;
        }
    }
    pci.auto_remove_level = auto_remove;
    ip->havok_handle = havok_body_create(pci);
    const vector vel((float)(((double)ip->pos.x - (double)ip->last_pos.x) / (double)Frametime), (float)(((double)ip->pos.y - (double)ip->last_pos.y) / (double)Frametime),
                     (float)(((double)ip->pos.z - (double)ip->last_pos.z) / (double)Frametime));
    havok_body_set_linear_velocity(ip->havok_handle, vel);
    if (!ip->item_flags.simulate)
    {
        havok_body_force_deactivate(ip->havok_handle);
    }
}

// 0x00A18C30 (:1465). LTCG: itemp in ESI, num_free_items in EDX, num_free_weapons in ECX.
// Single player: while free pool slots are scarce (20 weapons, 15 items) the item may go; otherwise after 10 s
// out of view range, or 30 s for items flagged 0x20000 nearby.
static bool object_can_be_cleaned_up(item* itemp, int& num_free_items, int& num_free_weapons, bool far_from_player)
{
    if (Multiplayer)
    {
        return false;
    }
    if (itemp->get_sub_type() == OT_SUB_ITEM_WEAPON)
    {
        if (num_free_weapons < 20)
        {
            num_free_weapons++;
            return true;
        }
    }
    else if (num_free_items < 15)
    {
        num_free_items++;
        return true;
    }
    if (far_from_player && itemp->deletion_timer.time_since() > 10000)
    {
        return true;
    }
    if ((itemp->info->flags & 0x20000) != 0 && !far_from_player && itemp->deletion_timer.time_since() > 30000)
    {
        return true;
    }
    return false;
}

// 0x00A3C040 (:1866)
// A weapon whose children are only its own props (character props, flag 1).
static bool object_item_is_weapon_and_children_are_allowed_props(item* itemp)
{
    if (itemp == nullptr || itemp->get_obj_type() != OT_ITEM || itemp->get_sub_type() != OT_SUB_ITEM_WEAPON)
    {
        return false;
    }
    object* child = itemp->child_ptr;
    while (child != nullptr)
    {
        if (child->get_obj_type() == OT_ITEM && ((item*)child)->info != (const item_info*)1)
        {
            // Original: compares the child's item info pointer with 1 (a prop flag read through the wrong field).
            return false;
        }
        child = child->child_next;
        if (child == itemp->child_ptr)
        {
            break;
        }
    }
    return true;
}

// 0x00A18CA0 (:1885)
// Multiplayer: items expire after 10 s (clips) or 30 s; a server keeps count of its synced items and remembers the
// oldest.
void mp_process_item_for_delete(item* itemp, int& mp_sync_item_count, int& mp_oldest_sync_item_age, item*& mp_oldest_sync_item)
{
    const int age = itemp->deletion_timer.time_since();
    // Magic numbers: clips last 10 s, other items 30 s.
    if ((itemp->m_prop_type != PROP_TYPE_CLIP ? 30000 : 10000) < age)
    {
        if (!itemp->remote_obj_flags.remote_object)
        {
            World.destroy_object(itemp, false);
            return;
        }
        if (Multiplayer == true && multi_is_server())
        {
            mp_sync_item_count -= itemp->remote_obj_flags.remote_object == 1 ? 1 : 0;
            World.destroy_object(itemp, false);
        }
    }
    else if (itemp->remote_obj_flags.remote_object)
    {
        const int since = itemp->deletion_timer.time_since();
        if (mp_oldest_sync_item_age < since)
        {
            mp_oldest_sync_item_age = since;
            mp_oldest_sync_item = itemp;
        }
    }
}

// ---- obj_item_props

// 0x00A3BD80 (:1686)
obj_item_props::obj_item_props(const item_info* info, const vector& pos, const matrix& orient, const obj_item_flags& flags, unsigned int collision_type, float mass_override,
                               salvage_material_type salvage_material)
    : obj_prop_block(pos, orient, nullptr)
{
    set<const item_info*>(PROP_ITEM_INFO, info);
    set<obj_item_flags>(PROP_ITEM_FLAGS, flags);
    set<unsigned int>(PROP_ITEM_COLLISION_TYPE, collision_type);
    set<float>(PROP_ITEM_MASS_OVERRIDE, mass_override);
    if (salvage_material != INVALID_SALVAGE_MATERIAL)
    {
        set<salvage_material_type>(PROP_ITEM_SALVAGE_MATERIAL, salvage_material);
    }
}

// 0x00A3BE70 (:1704)
obj_item_props::obj_item_props(const item_info* info, unsigned int srid, const vector& pos, const matrix& orient, const obj_item_flags& flags, unsigned int collision_type,
                               float mass_override)
    : obj_prop_block(pos, orient, nullptr)
{
    set<const item_info*>(PROP_ITEM_INFO, info);
    set<obj_item_flags>(PROP_ITEM_FLAGS, flags);
    set<unsigned int>(PROP_ITEM_COLLISION_TYPE, collision_type);
    set<float>(PROP_ITEM_MASS_OVERRIDE, mass_override);
    set<unsigned int>(PROP_ITEM_SRID, srid);
}

// 0x005DC830
obj_item_props::~obj_item_props()
{
}

// ---- processing

// 0x00AA75E0 (:1919)
// Every live item: dropped salvage (and multiplayer pickups) settles, then spins without collision; hidden
// respawning items come back; visible ones: fuel cell salvage glows, nano fades run out (then the item goes), cloth
// props torn from dead or destroyed hosts fade and go, and loose items stop being x-ray visible and get cleaned up
// (single player: by view distance, 150 m, and free pool slots; multiplayer by age, and the oldest synced item goes
// once a server syncs more than 118).
void object_item_process()
{
    int num_free_items = itempool.get_free();
    int num_free_weapons = weapon::pool_remaining();
    item* oldest_synced = nullptr;
    int oldest_synced_age = -1;
    int synced_count = 0;
    const int num = World.get_num_objects<item>();
    for (int i = 0; i < num; i++)
    {
        item* ip = World.get_object<item>(i);
        if (ip->obj_flags.destroyed)
        {
            continue;
        }
        synced_count += ip->remote_obj_flags.remote_object == 1 ? 1 : 0;
        // Magic number: item info flag 1, spinning pickups.
        if ((Multiplayer || ip->salvage_material != INVALID_SALVAGE_MATERIAL) && (ip->info->flags & 1) != 0 && !ip->is_owned() && ip->havok_handle != 0xFFFFFFFF)
        {
            if (ip->dropped_timer.elapsed() && (Multiplayer == true || !havok_body_is_active(ip->havok_handle)))
            {
                object_item_prepare_to_spin(ip);
                ip->dropped_timer.invalidate();
                if (!Multiplayer)
                {
                    havok_body_set_collision_type(ip->havok_handle, COLLIDE_NO_COLLISION, true, false);
                }
            }
            if (!ip->dropped_timer.valid())
            {
                object_item_spin(ip);
            }
        }
        if (!ip->obj_flags._dont_use_me)
        {
            if (ip->should_respawn() && ip->item_flags.respawn)
            {
                ip->respawn_timer.invalidate();
                if (ip->havok_handle != 0xFFFFFFFF)
                {
                    havok_body_get_pos_orient(ip->havok_handle, ip->pos, ip->orient);
                }
                object_set_visibility_plus_children(ip, true);
            }
            continue;
        }
        const item_info* info = ip->info;
        if (info->touch == item_touch_fuel_cell && ip->salvage_material != INVALID_SALVAGE_MATERIAL && ip->salvage_effect == 0xFFFFFFFF && info->material_effect_id != 0xFFFFFFFF &&
            ip->rdd.is_loaded() && ip->rdd.get_data()->smesh_instance_p != nullptr)
        {
            material_effect_parameters params;
            ip->salvage_effect = material_effects_apply(ip->rdd.get_data()->smesh_instance_p, (int)info->material_effect_id, &params);
            ip->touch_delay_timer.set(1000);
        }
        if (ip->special_flags.fading_from_nano)
        {
            queue_render_update(ip, ip->obj_flags, 2); // inlined (render flag bit 2 of the byte)
            if (Multiplayer || wrecking_crew_is_active() || !ip->deletion_timer.elapsed())
            {
                float alpha = (float)((double)(float)ip->deletion_timer.time_until() / 1000.0);
                if ((double)(float)ip->deletion_timer.time_until() / 1000.0 - 0.0 < 0.0)
                {
                    alpha = 0.0f;
                }
                if (0.0f <= alpha - 1.0f)
                {
                    alpha = 1.0f;
                }
                ip->render_alpha = alpha;
            }
            else
            {
                ip->render_alpha = 0.0f;
                World.destroy_object(ip, false);
            }
        }
        // Magic number: item info flag 2, never cleaned up.
        if ((ip->info->flags & 2) != 0)
        {
            continue;
        }
        if (ip->cloth_sim != nullptr)
        {
            constraint* cp = ip->child_constraint_ptr;
            bool torn = false;
            if (cp != nullptr)
            {
                object* host = World.object_from_handle(cp->host_handle);
                ip->item_flags.attached_cloth_sim = 1;
                // Magic number: mover flag 0x40000, destroyed.
                if (host != nullptr &&
                    (host->get_sub_type() == 0 || (host->get_sub_type() == 1 && host->get_obj_type() == OT_MOVER && (((object_mover*)host)->mover_flags & 0x40000) != 0)))
                {
                    if (ip->child_constraint_ptr != nullptr)
                    {
                        World.destroy_object(ip->child_constraint_ptr, false);
                    }
                    object* parent = World.object_from_handle(ip->get_parent());
                    if (parent != nullptr)
                    {
                        parent->remove_child(ip);
                    }
                    torn = true;
                }
                else if (host == nullptr && ip->item_flags.attached_cloth_sim)
                {
                    torn = true;
                }
            }
            else if (ip->item_flags.attached_cloth_sim)
            {
                torn = true;
            }
            if (torn)
            {
                ip->cloth_deletion_timer.set(0);
                ip->item_flags.attached_cloth_sim = 0;
            }
            if (ip->cloth_deletion_timer.valid() && ip->cloth_deletion_timer.elapsed())
            {
                // Magic numbers: a torn cloth fades over 1 s (2.1 s while eroding).
                const double total = ip->item_flags.doing_errode ? 2.1 : 1.0;
                const float left = (float)(total - (double)(float)ip->cloth_deletion_timer.time_since() / 1000.0);
                if (left < 0.0f)
                {
                    World.destroy_object(ip, false);
                    num_free_items++;
                    continue;
                }
                if (!ip->item_flags.doing_errode)
                {
                    ip->set_opacity_plus_child_items(left);
                }
            }
        }
        const object_sub_type sub_type = ip->get_sub_type();
        if (sub_type != OT_SUB_ITEM_MULTI_FLAG && sub_type != OT_SUB_ITEM_MULTI_BACKPACK && !ip->is_owned() && ip->get_parent() == 0xFFFFFFFF && !ip->item_flags.preplaced)
        {
            ip->item_flags.xray_visible = 0;
            ip->set_render_flags(8);
            xray_my_children(ip, false, 0xFF, 0, true);
            weapon* wp = World.object_cast<weapon>(ip);
            if (wp == nullptr || wp->w_info->ammo_type != WEAPON_AMMO_TYPE_MELEE || (Multiplayer && object_item_is_weapon_and_children_are_allowed_props(ip)))
            {
                if (ip->deletion_timer.valid())
                {
                    if (Multiplayer == true)
                    {
                        mp_process_item_for_delete(ip, synced_count, oldest_synced_age, oldest_synced);
                    }
                    else
                    {
                        // Magic number: farther than 150 m (apparent distance).
                        const bool far_from_player = !(ip->render_distance.get_apparent_distance(ip->pos) < 150.0f);
                        if (object_can_be_cleaned_up(ip, num_free_items, num_free_weapons, far_from_player))
                        {
                            World.destroy_object(ip, false);
                            num_free_items++;
                        }
                    }
                }
                else
                {
                    ip->deletion_timer.set(0);
                }
            }
        }
    }
    // Magic number: a server keeps at most 118 synced items.
    if (Multiplayer == true && multi_is_server() && synced_count > 118 && oldest_synced != nullptr)
    {
        World.destroy_object(oldest_synced, false);
    }
}

// 0x00A64EE0 (:2243)
// The animation batch finishes; multiplayer backpacks worn by players move their render bones by their player offset.
void object_item_process_post_transform()
{
    animlib_batch_process_all_end();
    if (Multiplayer != true && !wrecking_crew_is_active())
    {
        return;
    }
    const int num = World.get_num_objects<multi_object_backpack>();
    for (int i = 0; i < num; i++)
    {
        multi_object_backpack* bp = World.get_object<multi_object_backpack>(i);
        if (bp->get_player_attached_to() == nullptr || bp->char_inst == nullptr || !bp->char_inst->rdd.is_loaded())
        {
            continue;
        }
        const vector* offset = bp->get_player_offset();
        if (offset == nullptr)
        {
            continue;
        }
        matrix43* matrices = animlib_get_render_matrices(bp->char_inst->aim_handle);
        if (matrices == nullptr)
        {
            continue;
        }
        const character* cp = bp->char_inst->rdd.get_data()->base_character;
        for (int b = 0; b < cp->rig->num_bones; b++)
        {
            matrices[b].m_translation.x = offset->x + matrices[b].m_translation.x;
            matrices[b].m_translation.y = matrices[b].m_translation.y + offset->y;
            matrices[b].m_translation.z = matrices[b].m_translation.z + offset->z;
        }
    }
}

// 0x00A65030 (:2274)
// Every live item animates (transforms allowed again), then the animation batch starts.
void object_item_process_postsim()
{
    const int num = World.get_num_objects<item>();
    for (int i = 0; i < num; i++)
    {
        item* ip = World.get_object<item>(i);
        if (!ip->obj_flags.destroyed)
        {
            ip->obj_flags.dont_transform = 0;
            ip->update_animlib(Frametime, false);
        }
    }
    animlib_batch_process_all_start();
}

// 0x00A90E80 (:2526)
// A visible touchable item (once its touch delay passed) is within reach: 1 m (1.2 m passively) plus its radius
// (0.7 m at most) for pickups, 2 m for salvage, 0.5 m for fuel cells; the 1.9 m of the player's height do not count.
// Weapons farther than 0.5 m must be in sight from 10 cm above the player's feet.
bool object_item_player_close_enough_to_touch(player* pp, const item* itemp, bool active)
{
    if (!itemp->obj_flags._dont_use_me || itemp->info->touch == nullptr || !itemp->touch_delay_timer.elapsed())
    {
        return false;
    }
    float reach;
    if (itemp->salvage_material == INVALID_SALVAGE_MATERIAL)
    {
        reach = active ? 1.0f : 1.2f;
    }
    else if (itemp->info->touch == item_touch_fuel_cell)
    {
        reach = 0.5f;
    }
    else
    {
        reach = 2.0f;
    }
    float radius = itemp->get_bounding_radius();
    if (0.0f <= radius - 0.7f)
    {
        radius = 0.7f;
    }
    vector from = pp->pos;
    const float dx = itemp->pos.x - from.x;
    float dy = itemp->pos.y - from.y;
    const float dz = itemp->pos.z - from.z;
    if (0.0f < dy)
    {
        dy = (float)((double)dy - 1.9);
        if (dy - 0.0f < 0.0f)
        {
            dy = 0.0f;
        }
    }
    const float dist_sq = (float)((double)dx * dx + (double)dy * dy + (double)dz * dz);
    if (!(dist_sq <= (float)(((double)radius + (double)reach) * ((double)radius + (double)reach))))
    {
        return false;
    }
    if (itemp->get_sub_type() == OT_SUB_ITEM_WEAPON && 0.25f <= dist_sq)
    {
        raycast_input in;
        in.end = itemp->pos;
        in.radius = 0.0f;
        in.m_filter_info = COLLIDE_RAY_DEFAULT;
        in.start.x = (float)((double)from.x + (double)UP_VECTOR.x * 0.1);
        in.start.y = (float)((double)from.y + (double)UP_VECTOR.y * 0.1);
        in.start.z = (float)((double)from.z + (double)UP_VECTOR.z * 0.1);
        in.filter_params = nullptr;
        in.filter_callback = nullptr;
        in.m_hit_collector_type = raycast_input::COLLECT_CLOSEST_HIT;
        in.m_flags = 0;
        raycast_output out;
        if (havok_raycast(in, out) && out.m_hits[0].hit_object_handle != 0xFFFFFFFF && out.m_hits[0].hit_object_handle != itemp->get_handle())
        {
            return false;
        }
    }
    return true;
}

// 0x00A91180 (:2608)
// The touch script runs; a used item respawns later (hidden) or goes (dropped items and those without owners;
// owned or flagged 0x400 items only leave their parent); the local player hears the (secondary) pickup sound.
// Returns whether the script asks for a pickup prompt.
bool object_item_player_touch(player* pp, item* itemp)
{
    if (Multiplayer && Multi_match_info.match_state != MGMS_GAMEPLAY)
    {
        return false;
    }
    if (pp->obj_flags.destroyed)
    {
        return false;
    }
    bool play_secondary_sound = false;
    bool ask_pickup = false;
    const bool used = itemp->info->touch(pp, itemp, play_secondary_sound, ask_pickup);
    itemp->item_flags.touched_last_frame = 1;
    if (!used)
    {
        return ask_pickup;
    }
    int respawn_delay = itemp->info->respawn_delay;
    if (Multiplayer == true)
    {
        respawn_delay = multi_game_get_item_respawn_delay(itemp);
    }
    bool hide = false;
    if (!itemp->item_flags.respawn || respawn_delay == -1)
    {
        if (Multiplayer && !multi_is_server())
        {
            hide = true;
        }
        else if (itemp->owner_handle == 0xFFFFFFFF && itemp->collision_type == COLLIDE_ITEM && (itemp->info->flags & 0x400) == 0)
        {
            World.destroy_object(itemp, false);
        }
        else if (itemp->get_parent() != 0xFFFFFFFF)
        {
            itemp->remove_from_parent();
        }
    }
    else
    {
        hide = true;
    }
    if (hide)
    {
        object_set_visibility_plus_children(itemp, false);
        itemp->respawn_timer.set(respawn_delay);
    }
    if (Player != nullptr && pp->get_handle() == Player->get_handle())
    {
        if (!play_secondary_sound)
        {
            audiolib_cue_play(itemp->info->pickup_snd, nullptr, nullptr);
            return ask_pickup;
        }
        audiolib_cue_play(itemp->info->pickup_snd_2, nullptr, nullptr);
    }
    return ask_pickup;
}

// ---- item

// 0x00A18E90 (:2696)
// A hidden item with a respawn time respawns when it has passed (multiplayer servers: when the mode allows, which may
// force it).
bool item::should_respawn() const
{
    if (this == nullptr)
    {
        return false;
    }
    if (obj_flags._dont_use_me || !respawn_timer.valid())
    {
        return false;
    }
    if (Multiplayer && !multi_is_server())
    {
        return false;
    }
    bool respawn = respawn_timer.elapsed();
    if (Multiplayer == true)
    {
        bool force = false;
        if (!multi_gameplay_mode_can_item_respawn(const_cast<item*>(this), force))
        {
            return false;
        }
        if (force)
        {
            respawn = true;
        }
    }
    return respawn;
}

// 0x00A3C080 (:2725)
// Requests the high resolution textures (4 weapons at most, 3 of them large ones flagged 0x4000).
bool item::stream_in_high_res()
{
    item_info* ii = (item_info*)info;
    if (ii->high_res_srid == 0 || !rdd.is_loaded() || ((ii->flags & 0x4000) != 0 && Num_large_high_res_weapons_loaded == 3) || Num_high_res_weapons_loaded == 4 ||
        ii->special_flags.high_res_texture_requested)
    {
        return false;
    }
    High_res_weapons_loaded[Num_high_res_weapons_loaded] = ii;
    Num_high_res_weapons_loaded++;
    ii->special_flags.high_res_texture_requested = true;
    stream2_request(info->high_res_srid);
    return true;
}

// 0x00A18F20 (:2759)
bool item::stream_out_high_res()
{
    if (info->special_flags.high_res_texture_requested)
    {
        ((item_info*)info)->special_flags.high_res_texture_request_unload = true;
    }
    return true;
}

// 0x00A18F80 (:2805)
unsigned int item::get_classname_hash()
{
    return item_instantiator.this_hash;
}

// 0x00A91480 (:2805)
void item::pool_free()
{
    itempool.free(this);
}

// 0x00A18F90 (:2806)
item::item()
{
    // the timestamps, render_distance, item_flags and the name index are set by their constructors
}

// 0x009587C0
item::~item()
{
}

// 0x00AA7B90 (:2818)
// Load: the flags (respawn from any of the three props, preplaced), collision type (a player's projectile collides
// like its owner's character proxy, the 0x10 bit set), the item info (by pointer, name or weapon type), the streaming
// resource (its own srid or the item's container), the damage flags of the info, physics overrides of the info flags
// (0x80 mover collision with the info mass, 0x200 no physics), the ammo box flags (0x800), timers, prop type,
// shadows, salvage material (salvage and fuel cell scripts default to metal) and the character of a character mesh.
// Save: the overrides, flags, info name, deletion time, prop type, salvage material and the character.
bool item::serialize(obj_prop_block& pblock, bool load)
{
    if (!object::serialize(pblock, load))
    {
        return false;
    }
    if (!load)
    {
        if (0.0f < mass_override)
        {
            pblock.set<float>(PROP_ITEM_MASS_OVERRIDE, mass_override);
        }
        pblock.set<unsigned int>(PROP_ITEM_COLLISION_TYPE, collision_type);
        pblock.set<obj_item_flags>(PROP_ITEM_FLAGS, item_flags);
        pblock.set(PROP_ITEM_INFO_NAME, info->name);
        if (deletion_timer.valid())
        {
            pblock.set<int>(PROP_ITEM_DELETION_TIMER, deletion_timer.time_until());
        }
        pblock.set<prop_type>(PROP_ITEM_PROP_TYPE, m_prop_type);
        if (salvage_material != INVALID_SALVAGE_MATERIAL)
        {
            pblock.set<salvage_material_type>(PROP_ITEM_SALVAGE_MATERIAL, salvage_material);
        }
        if (char_inst != nullptr)
        {
            character_instance_serialize(char_inst, pblock, false);
        }
        return true;
    }
    *(unsigned int*)&item_flags = 0;
    *(unsigned int*)&special_flags = 0;
    last_pos = pos;
    mass_override = 1.0f;
    info = nullptr;
    collision_type = COLLIDE_ITEM;
    render_alpha = 1.0f;
    owner_handle = 0xFFFFFFFF;
    cloth_sim = nullptr;
    xray_render_alpha = 0xFF;
    m_stealth_percent = 0.0f;
    m_nav_cell_detour_request_handle = 0xFFFFFFFF;
    pblock.get<float>(PROP_ITEM_MASS_OVERRIDE, mass_override);
    pblock.get<obj_item_flags>(PROP_ITEM_FLAGS, item_flags);
    bool respawn_prop = false;
    bool respawns_prop = false;
    pblock.get<bool>(PROP_ITEM_RESPAWN, respawn_prop);
    pblock.get<bool>(PROP_ITEM_RESPAWNS, respawns_prop);
    item_flags.respawn = (item_flags.respawn || respawns_prop || respawn_prop) ? 1 : 0;
    bool preplaced = false;
    pblock.get<bool>(PROP_ITEM_PREPLACED, preplaced);
    item_flags.preplaced = preplaced;
    pblock.get<unsigned int>(PROP_ITEM_COLLISION_TYPE, collision_type);
    if (get_sub_type() == OT_SUB_ITEM_PROJECTILE)
    {
        // Original: without the owner prop the handle read is the prop lookup's leftover (uninitialized here).
        unsigned int owner = 0xFFFFFFFF;
        pblock.get<unsigned int>(PROJ_PROP_OWNER_HANDLE, owner);
        object* owner_objp = World.object_from_handle(owner);
        if (owner_objp != nullptr && object_is_player(owner_objp->get_handle()))
        {
            human* hp = (human*)owner_objp;
            if (hp->rdd.is_loaded())
            {
                // Magic numbers: the character proxy's collision filter info (layer bits kept), bit 0x10 set.
                const unsigned int filter = hp->rdd.get_data()->controller->m_phantom->getCollidable()->getCollisionFilterInfo();
                collision_type = (filter & 0xFFFF0010) | 0x10;
            }
        }
    }
    default_batchable_setting = true;
    info = nullptr;
    const item_info* info_ptr;
    if (pblock.get_optional<const item_info*>(PROP_ITEM_INFO, info_ptr))
    {
        info = (item_info*)info_ptr;
    }
    else
    {
        char name[256];
        if (!pblock.get_optional(PROP_ITEM_INFO_NAME, name))
        {
            pblock.get(PROP_WEAPON_TYPE, name);
        }
        info = object_item_get_info(name);
    }
    if (info == nullptr)
    {
        keen::debug::nativePrintArgs("item::serialize info failed");
        if (info == nullptr)
        {
            return false;
        }
    }
    unsigned int prop_srid;
    if (pblock.get_optional<unsigned int>(PROP_ITEM_SRID, prop_srid))
    {
        srid = prop_srid;
        if (!request_streaming_resource(prop_srid, STREAM_PRIORITY_HIGH))
        {
            keen::debug::nativePrintArgs("item::serialize request_streaming_resource failed");
            return false;
        }
    }
    else
    {
        srid = stream2_container_find(info->name);
        if (!request_streaming_resource(srid, STREAM_PRIORITY_HIGH))
        {
            return false;
        }
    }
    damage_info.hitpoints = info->hitpoints;
    // the damage flags of the info, bit by bit (Damage_info_flags order)
    const int df = info->damage_flags;
    damage_info.flags.can_explode = df & 1;
    damage_info.flags.damaged_by_bullets = (df >> 1) & 1;
    damage_info.flags.damaged_by_collisions = (df >> 2) & 1;
    damage_info.flags.damaged_by_explosions = (df >> 3) & 1;
    damage_info.flags.damaged_by_melee = (df >> 4) & 1;
    damage_info.flags.damaged_by_vehicles = (df >> 5) & 1;
    // Magic numbers: item info flags 0x80 (collides as a mover, info mass), 0x200 (no physics), 0x800 (ammo box).
    if ((info->flags & 0x80) != 0)
    {
        collision_type = COLLIDE_MOVER;
        mass_override = -1.0f;
    }
    if ((info->flags & 0x200) != 0)
    {
        item_flags.no_physics = 1;
        item_flags.simulate = 0;
    }
    special_flags.is_ammo_box = (info->flags >> 11) & 1;
    special_flags.use_enabled = special_flags.is_ammo_box;
    special_flags.arc_welder_is_arcing = 0;
    special_flags.fading_from_nano = 0;
    deletion_timer.invalidate();
    cloth_deletion_timer.invalidate();
    int deletion_ms;
    if (pblock.get_optional<int>(PROP_ITEM_DELETION_TIMER, deletion_ms))
    {
        deletion_timer.set(deletion_ms);
    }
    respawn_timer.invalidate();
    dropped_timer.invalidate();
    m_foley_collision_instance = -1;
    touch_arg1_override = 0.0f;
    touch_arg2_override = 0.0f;
    touch_delay_timer.invalidate();
    m_prop_type = PROP_TYPE_NONE;
    pblock.get_optional<prop_type>(PROP_ITEM_PROP_TYPE, m_prop_type);
    if (All_items_cast_shadows && m_prop_type != PROP_TYPE_CLIP)
    {
        item_flags.cast_shadows = 1;
        item_flags.cast_drop_shadows = 1;
    }
    if (Multiplayer == true)
    {
        // Original: the result is not used.
        multi_is_server();
    }
    mp_sim_state = nullptr;
    salvage_effect = 0xFFFFFFFF;
    salvage_material = INVALID_SALVAGE_MATERIAL;
    pblock.get_optional<salvage_material_type>(PROP_ITEM_SALVAGE_MATERIAL, salvage_material);
    if (salvage_material == INVALID_SALVAGE_MATERIAL && (info->touch == item_touch_give_salvage || info->touch == item_touch_fuel_cell))
    {
        salvage_material = SALVAGE_MATERIAL_METAL;
    }
    if (info->cmesh_name == nullptr)
    {
        char_inst = nullptr;
        return true;
    }
    char_inst = character_instance_create(1.0f, info->anim_set_name);
    if (char_inst == nullptr)
    {
        keen::debug::nativePrintArgs("item::serialize char create");
    }
    if (char_inst == nullptr)
    {
        return false;
    }
    character_instance_serialize(char_inst, pblock, true);
    return true;
}

// 0x00A19000 (:3029)
obj_serialize_mode item::serialize_mode()
{
    // the original flag as OSM_FULL (bit 1), else OSM_NONE
    return (obj_serialize_mode)(obj_flags.original << 1);
}

// Places a mesh's "$ground" tag on the object's origin (inlined twice in item::stream).
// Original bug kept: the offset is computed into pos itself, so pos becomes twice the rotated offset (the item's
// position is lost); the orientation is the tag's inverse rotation times the object's.
static void item_apply_ground_tag(item* ip, const static_mesh* smesh)
{
    const int tag = smesh->m_tags.get_tag("$ground");
    if (tag < 0)
    {
        return;
    }
    matrix43 tag_transform;
    smesh->m_tags.get_tag_transform(tag_transform, tag);
    ip->pos.x = -tag_transform.m_translation.x;
    ip->pos.y = -tag_transform.m_translation.y;
    ip->pos.z = -tag_transform.m_translation.z;
    ip->pos.rotate(tag_transform.m_rotation);
    ip->pos.unrotate(ip->orient);
    ip->pos.x = ip->pos.x + ip->pos.x;
    ip->pos.y = ip->pos.y + ip->pos.y;
    ip->pos.z = ip->pos.z + ip->pos.z;
    ip->orient = tag_transform.m_rotation.copy_transpose() * ip->orient;
}

// A mesh instance of a static mesh (inlined in item::stream).
static rl_mesh_instance* item_create_mesh_instance(static_mesh* smesh)
{
    rl_mesh_instance* mi = rl_mesh_instance::create();
    if (mi != nullptr)
    {
        mi->init(smesh->m_shared_header.m_mesh.to_pointer(), smesh->m_shared_header.m_material_map.to_pointer(), *smesh->m_lod_submesh_id.to_pointer());
        mi->calculate_local_aabb();
    }
    return mi;
}

// 0x00AA82E0 (:3046)
// Load: the static mesh instance (or the character: idle state, the parent's rig for flag 0x100 items, a cloth
// simulation whose render box grows sideways by its height), the pickup mesh, the info's props (attached at their
// tags), the ammo box map icon and lid state, the physics (pickups wait 2.5 s before they spin), x-ray from a
// non-local human parent, salvage glow, nav detour for mover-like items.
// Unload: destroy-on-stream, effects and detours go, the character's resources and cloth, the mesh instances, the body.
bool item::stream(bool load)
{
    if (!load)
    {
        if (obj_flags.destroy_on_stream)
        {
            World.destroy_object(this, false);
        }
        if (salvage_effect != 0xFFFFFFFF)
        {
            material_effects_remove(salvage_effect);
        }
        if (m_nav_cell_detour_request_handle != 0xFFFFFFFF)
        {
            nav_cell_detour_remove_request(m_nav_cell_detour_request_handle);
            m_nav_cell_detour_request_handle = 0xFFFFFFFF;
        }
        if (char_inst != nullptr && char_inst->rdd.is_loaded())
        {
            character_instance_resources_unloaded(char_inst);
            if (cloth_sim != nullptr)
            {
                cloth_sim_delete(cloth_sim);
                cloth_sim = nullptr;
            }
        }
        if (rdd.is_loaded())
        {
            resource_dependent_data* data = rdd.get_data();
            if (data->smesh_instance_p != nullptr)
            {
                data->smesh_instance_p->release();
            }
            if (data->pickup_smesh_instance_p != nullptr)
            {
                data->pickup_smesh_instance_p->release();
            }
        }
        special_flags.using_high_res_texture = 0;
        rdd.set_loaded(false);
        if (havok_handle != 0xFFFFFFFF)
        {
            havok_body_free(havok_handle, false, true, false);
            havok_handle = 0xFFFFFFFF;
        }
        if (special_flags.is_ammo_box)
        {
            Cartographer.remove_icon(get_handle());
        }
        return true;
    }
    rdd.set_loaded(true);
    resource_dependent_data* data = rdd.get_data();
    data->smesh = nullptr;
    data->smesh_instance_p = nullptr;
    data->pickup_smesh = nullptr;
    data->pickup_smesh_instance_p = nullptr;
    if (info->cmesh_name == nullptr)
    {
        if (info->smesh_name != nullptr)
        {
            data->smesh = static_mesh_find(info->smesh_name, srid);
            if (data->smesh != nullptr)
            {
                item_apply_ground_tag(this, data->smesh);
                data->smesh_instance_p = item_create_mesh_instance(data->smesh);
                if (data->smesh_instance_p == nullptr)
                {
                    rdd.set_loaded(false);
                    return false;
                }
                default_batchable_setting = data->smesh_instance_p->get_is_batchable();
            }
        }
    }
    else
    {
        character* base_character = character_find(info->name, srid);
        if (base_character == nullptr)
        {
            return false;
        }
        // Magic number: render category 0x11.
        if (!character_instance_resources_loaded(char_inst, base_character, 0x11))
        {
            rdd.set_loaded(false);
            return false;
        }
        if (char_inst == nullptr || !char_inst->rdd.is_loaded())
        {
            return false;
        }
        char_inst->set_state(index_alias(ANIMATION_STATE_STAND, ANIM_ALIAS_NONE), index_alias(-1, ANIM_ALIAS_NONE), 0.3f, false);
        // Magic number: item info flag 0x100, animated by the parent's rig.
        if ((info->flags & 0x100) != 0)
        {
            human* parent_hp = World.object_from_handle<human>(get_parent());
            if (parent_hp != nullptr && parent_hp->rdd.is_loaded())
            {
                animlib_set_parent_rig(char_inst->aim_handle, parent_hp->char_inst->aim_handle);
            }
        }
        cloth_sim = cloth_sim_create(info->cmesh_name, &pos, &orient, char_inst->aim_handle);
        if (cloth_sim != nullptr)
        {
            collision_type = COLLIDE_CLOTH_SIM;
            // The render box grows by its height on x and z (room for the cloth to swing).
            rl_skin_instance* ri = char_inst->rdd.get_data()->renderable_instance_p;
            rl_aabb box = ri->get_local_aabb(); // inlined
            const float height = box.max.y - box.min.y;
            box.min.x = box.min.x - height;
            box.max.x = box.max.x + height;
            box.min.z = box.min.z - height;
            box.max.z = box.max.z + height;
            ri->set_local_aabb(box); // inlined
            if (item_flags.character_prop)
            {
                cloth_sim->inherite_obj_motion = 1;
            }
        }
    }
    if (info->pickup_smesh_name != nullptr)
    {
        // Original: the pickup mesh is used without a null check.
        data->pickup_smesh = static_mesh_find(info->pickup_smesh_name, srid);
        item_apply_ground_tag(this, data->pickup_smesh);
        data->pickup_smesh_instance_p = item_create_mesh_instance(data->pickup_smesh);
    }
    if (!item_flags.props_created)
    {
        for (int i = 0; i < info->num_props; i++)
        {
            const item_info::item_prop& prop = info->props[i];
            if (prop.prop_tag == nullptr)
            {
                continue;
            }
            item_info* prop_info = object_item_get_info(prop.prop_name);
            obj_item_flags flags;
            // Magic numbers: no physics, simulate, cast shadows and drop shadows.
            *(unsigned int*)&flags = (*(unsigned int*)&flags & 0xFFFC6006) | 0x6006;
            obj_item_props props(prop_info, pos, orient, flags, COLLIDE_ITEM, -1.0f, INVALID_SALVAGE_MATERIAL);
            item* prop_item = World.create_object<item>(props, 0xFFFFFFFF);
            if (prop_item != nullptr)
            {
                prop_item->m_prop_type = prop.type;
                set_child(prop_item, get_tag_index_from_name(prop.prop_tag), -1, false, true);
            }
        }
        item_flags.props_created = 1;
    }
    if (special_flags.is_ammo_box)
    {
        special_flags.use_enabled = 1;
        Cartographer.add_icon(get_handle(), MAP_ELEMENT_AMMO_BOX, true, false, false);
        if (char_inst != nullptr)
        {
            // Magic number: state 106 (the crate's closed lid).
            char_inst->set_state(index_alias(ANIMATION_STATE_CROUCH, ANIM_ALIAS_NONE), index_alias(-1, ANIM_ALIAS_NONE), 0.3f, false);
            if (Shiny_ammo_crates && info->material_effect_id != 0xFFFFFFFF)
            {
                material_effect_parameters params;
                if (!char_inst->rdd.is_loaded())
                {
                    return false;
                }
                salvage_effect = material_effects_apply(char_inst->rdd.get_data()->renderable_instance_p, (int)info->material_effect_id, &params);
            }
        }
    }
    if (!item_flags.no_physics)
    {
        create_physics_object(this);
        // Magic numbers: item info flag 1 (spinning pickup) waits 2.5 s.
        if ((info->flags & 1) != 0)
        {
            dropped_timer.set(2500);
        }
    }
    object* parent_objp = World.object_from_handle(get_parent());
    if (parent_objp != nullptr && parent_objp->get_obj_type() == OT_HUMAN && !object_is_local_player(parent_objp))
    {
        item_flags.xray_visible = ((human*)parent_objp)->hflags.xray_visible;
    }
    set_render_flags(8);
    touch_delay_timer.set(0);
    if (salvage_material != INVALID_SALVAGE_MATERIAL && info->touch == item_touch_give_salvage)
    {
        if (info->material_effect_id != 0xFFFFFFFF)
        {
            material_effect_parameters params;
            rl_renderable_instance* ri;
            if (char_inst == nullptr)
            {
                ri = data->smesh_instance_p;
            }
            else
            {
                if (!char_inst->rdd.is_loaded())
                {
                    return false;
                }
                ri = char_inst->rdd.get_data()->renderable_instance_p;
            }
            salvage_effect = material_effects_apply(ri, (int)info->material_effect_id, &params);
        }
        touch_delay_timer.set(Salvage_touch_delay_ms);
    }
    if (collision_type == COLLIDE_MOVER && !item_flags.no_physics)
    {
        m_nav_cell_detour_request_handle = nav_cell_detour_add_request(get_handle(), -1);
    }
    return true;
}

// 0x00A19010 (:3319)
void item::hierarchy_notify(hierarchy_notify_code code)
{
    if (code == HN_PARENT_DESTROYED)
    {
        World.destroy_object(this, false);
    }
}

// 0x00A19030 (:3326)
bool item::is_owned() const
{
    return owner_handle != 0xFFFFFFFF;
}

// The placement of an item's mesh instance (inlined twice in item::update_render): shown or hidden, and when shown
// placed at the item (the inlined rl_scene_entity setters) with the LOD level, then the scene update; the opacity and
// the transparent shadows in any case.
static void item_place_mesh_instance(item* ip, rl_mesh_instance* instance, bool visible, int lod)
{
    instance->set_is_visible(visible); // inlined
    if (visible)
    {
        instance->set_position(ip->pos);
        instance->set_rotation(ip->orient);
        instance->set_lod_level(lod);
        game_render_scene_update(instance);
    }
    instance->set_opacity(ip->render_alpha);
    instance->set_casts_transparent_shadows(ip->casts_transparent_shadows);
}

// The stealth cut height of a held item (instance parameter w), from the root parent's box when it is a human; 100000
// (nothing cut) otherwise. The 0.3 literals are doubles.
static float item_stealth_cut_height(item* ip, object* parent)
{
    object* root = parent;
    while (World.object_from_handle(root->get_parent()) != nullptr)
    {
        root = World.object_from_handle(root->get_parent());
    }
    if (World.object_from_handle<human>(root->get_handle()) == nullptr)
    {
        // Magic number: no cut.
        return 100000.0f;
    }
    vector bmin;
    vector bmax;
    root->get_bbox(bmin, bmax);
    return (float)((((double)bmin.y - 0.3) - ((double)bmax.y + 0.3)) * (double)ip->m_stealth_percent + (double)bmax.y + 0.3);
}

// 0x00A65190 (:3331)
// The render update of an item: shown when _dont_use_me is set, with the character LOD of the apparent distance (0
// when hidden). A dropped item with a pickup mesh shows only that one. Otherwise the high-res texture flag follows the
// item info, and the mesh instance is placed with the shadow, x-ray and stealth settings (a held stealthed item
// distorts, is cut at its holder's stealth height and is not batched); an item with a character (animated items)
// updates its skin renderable likewise through character_instance::update_render.
// Notes (decompiled behaviour, kept as is): a held item that is not stealthed clears the mesh instance's distortion
// flag twice, and only the mesh instance (not the skin renderable) becomes batchable again.
void item::update_render(rl_renderer*)
{
    const bool visible = obj_flags._dont_use_me;
    int lod = 0;
    if (visible)
    {
        const float distance = render_distance.get_apparent_distance(pos);
        while (lod < character_instance::c_lod_distance_max && character_instance::ms_char_lod_distances[lod].dist < distance)
        {
            lod++;
        }
        // Magic number: the last LOD (3).
        if (lod > 2)
        {
            lod = 3;
        }
    }
    if (!rdd.is_loaded())
    {
        return;
    }
    resource_dependent_data* data = rdd.get_data();
    character_instance::resource_dependent_data* char_data = nullptr;
    if (char_inst != nullptr)
    {
        char_data = char_inst->rdd.get_data();
    }
    if (get_parent() == 0xFFFFFFFF && data->pickup_smesh_instance_p != nullptr)
    {
        if (data->smesh_instance_p != nullptr)
        {
            data->smesh_instance_p->set_is_visible(false); // inlined
        }
        if (char_data != nullptr && char_data->renderable_instance_p != nullptr)
        {
            char_data->renderable_instance_p->set_is_visible(false); // inlined
        }
        item_place_mesh_instance(this, data->pickup_smesh_instance_p, visible, lod);
        return;
    }
    if (data->pickup_smesh_instance_p != nullptr)
    {
        data->pickup_smesh_instance_p->set_is_visible(false); // inlined
    }
    if (special_flags.using_high_res_texture)
    {
        if (!info->special_flags.high_res_texture_loaded)
        {
            special_flags.using_high_res_texture = 0;
        }
    }
    else if (info->special_flags.high_res_texture_loaded)
    {
        special_flags.using_high_res_texture = 1;
    }
    rl_mesh_instance* mesh = data->smesh_instance_p;
    if (mesh != nullptr)
    {
        item_place_mesh_instance(this, mesh, visible, lod);
        mesh->set_casts_shadows(item_flags.cast_shadows != 0);
        mesh->set_casts_drop_shadows(item_flags.cast_drop_shadows != 0);
        mesh->set_xray_visible(item_flags.xray_visible != 0);
        mesh->set_xray_render_params(xray_material, xray_render_alpha);
        // Magic number: the robust comparison's epsilon.
        const bool stealthed = fl_greater_than_robust(m_stealth_percent, 0.0f, 0.0001f);
        object* parent_p = World.object_from_handle(get_parent());
        mesh->set_distortion_visible(false);
        if (parent_p != nullptr)
        {
            vector4 params = mesh->get_instance_params();
            if (!stealthed)
            {
                mesh->set_distortion_visible(false);
                // Magic number: no cut.
                params.w = 100000.0f;
                mesh->set_instance_params(params);
                mesh->set_is_batchable(true);
            }
            else
            {
                mesh->set_distortion_visible(true);
                params.w = item_stealth_cut_height(this, parent_p);
                mesh->set_instance_params(params);
                mesh->set_is_batchable(false);
            }
        }
    }
    if (char_inst == nullptr || char_data == nullptr)
    {
        return;
    }
    rl_skin_instance* skin = char_data->renderable_instance_p;
    skin->set_is_visible(visible); // inlined
    if (!visible)
    {
        return;
    }
    vector bmin;
    vector bmax;
    get_local_bbox(bmin, bmax);
    char_inst->update_render(pos, orient, bmin, bmax, lod, false);
    skin->set_opacity(render_alpha);
    skin->set_casts_transparent_shadows(casts_transparent_shadows);
    skin->set_casts_shadows(item_flags.cast_shadows != 0);
    skin->set_casts_drop_shadows(item_flags.cast_drop_shadows != 0);
    skin->set_xray_visible(item_flags.xray_visible != 0);
    skin->set_xray_render_params(xray_material, xray_render_alpha);
    // Magic number: the robust comparison's epsilon.
    const bool stealthed = fl_greater_than_robust(m_stealth_percent, 0.0f, 0.0001f);
    object* parent_p = World.object_from_handle(get_parent());
    skin->set_distortion_visible(false);
    if (parent_p == nullptr)
    {
        return;
    }
    skin->set_distortion_visible(stealthed);
    vector4 params = skin->get_instance_params();
    if (stealthed)
    {
        params.w = item_stealth_cut_height(this, parent_p);
        skin->set_instance_params(params);
        skin->set_is_batchable(false);
        return;
    }
    // Magic number: no cut.
    params.w = 100000.0f;
    skin->set_instance_params(params);
}

// Sets the outline pass of a layer (0..3) on a renderable instance (rl_renderable_instance::set_outline_enabled,
// inlined in enable_object_outline).
static void item_set_outline(rl_renderable_instance* ri, int layer)
{
    if (ri != nullptr && (unsigned int)layer < 4)
    {
        ri->set_outline_enabled(layer); // inlined
    }
}

// 0x00A3C150 (:3512): the character's instance, the mesh and the pickup mesh.
bool item::enable_object_outline(int layer)
{
    if (char_inst != nullptr && char_inst->rdd.is_loaded())
    {
        item_set_outline(char_inst->rdd.get_data()->renderable_instance_p, layer);
    }
    if (rdd.is_loaded())
    {
        item_set_outline(rdd.get_data()->smesh_instance_p, layer);
        item_set_outline(rdd.get_data()->pickup_smesh_instance_p, layer);
    }
    return true;
}

// 0x00A3C210 (:3537)
bool item::disable_object_outline()
{
    if (char_inst != nullptr && char_inst->rdd.is_loaded() && char_inst->rdd.get_data()->renderable_instance_p != nullptr)
    {
        char_inst->rdd.get_data()->renderable_instance_p->set_outline_disabled(); // inlined
    }
    if (rdd.is_loaded())
    {
        if (rdd.get_data()->smesh_instance_p != nullptr)
        {
            rdd.get_data()->smesh_instance_p->set_outline_disabled(); // inlined
        }
        if (rdd.get_data()->pickup_smesh_instance_p != nullptr)
        {
            rdd.get_data()->pickup_smesh_instance_p->set_outline_disabled(); // inlined
        }
    }
    return true;
}

// 0x00A3C290 (:3562): the mesh instance's bounding sphere, or the character's.
float item::get_bounding_radius() const
{
    if (rdd.is_loaded())
    {
        const rl_mesh_instance* mi = rdd.get_data()->smesh_instance_p;
        if (mi != nullptr)
        {
            return mi->get_local_bounding_sphere().m_radius;
        }
        if (char_inst != nullptr)
        {
            vector center;
            float radius;
            char_inst->get_bounding_sphere(center, radius);
            return radius;
        }
    }
    return 0.0f;
}

// 0x00A91490 (:3592)
void item::free_this()
{
    if (havok_handle != 0xFFFFFFFF)
    {
        havok_body_free(havok_handle, false, true, false);
        havok_handle = 0xFFFFFFFF;
    }
    if (char_inst != nullptr)
    {
        character_instance_delete(char_inst);
        char_inst = nullptr;
    }
    if (Multiplayer && get_obj_type() == OT_ITEM && get_sub_type() == OT_SUB_ITEM_WEAPON)
    {
        multi_core_object_process_object_free(this);
    }
    if (salvage_effect != 0xFFFFFFFF)
    {
        // Original: the material effect handle is destroyed as an object handle.
        World.destroy_object(salvage_effect, false);
    }
    object::free_this();
}

// 0x00A19040 (:3629)
void item::get_bbox(vector& bmin, vector& bmax) const
{
    vector local_min;
    vector local_max;
    get_local_bbox(local_min, local_max);
    bbox_instance(bmin, bmax, orient, pos, local_min, local_max);
}

// The box of a mesh instance's local bounding volume (inlined in get_local_bbox).
static void item_mesh_local_box(const rl_mesh_instance* mi, vector& bmin, vector& bmax)
{
    const rl_aabb box = mi->get_local_aabb(); // inlined
    bmin = box.min;
    bmax = box.max;
}

// 0x00A65B60 (:3641)
// The pickup mesh (lying alone) or the mesh; a character: its box (ammo boxes raise the top to the bone box's),
// the bone box (flag 0x10000), or the bone box grown by the largest dimension (flag 0x100). 10 cm otherwise.
void item::get_local_bbox(vector& bmin, vector& bmax) const
{
    if (rdd.is_loaded())
    {
        const resource_dependent_data* data = rdd.get_data();
        if (get_parent() == 0xFFFFFFFF && data->pickup_smesh_instance_p != nullptr)
        {
            item_mesh_local_box(data->pickup_smesh_instance_p, bmin, bmax);
            return;
        }
        if (data->smesh_instance_p != nullptr)
        {
            item_mesh_local_box(data->smesh_instance_p, bmin, bmax);
            return;
        }
        if (char_inst != nullptr)
        {
            const int flags = info->flags;
            // Magic numbers: item info flags 0x800 (ammo box), 0x10000 (bone box), 0x100 (parent rig).
            if ((flags & 0x800) != 0)
            {
                char_inst->get_bbox(bmin, bmax);
                matrix43* matrices;
                if (animlib_get_instanced_model_matrices(char_inst->aim_handle, &matrices))
                {
                    vector bone_min;
                    vector bone_max;
                    animlib_get_bone_bbox(char_inst->aim_handle, bone_min, bone_max);
                    if (bmax.y - bone_max.y < 0.0f)
                    {
                        bmax.y = bone_max.y;
                    }
                }
                return;
            }
            if ((flags & 0x10000) != 0)
            {
                animlib_get_bone_bbox(char_inst->aim_handle, bmin, bmax);
                return;
            }
            if ((flags & 0x100) == 0)
            {
                char_inst->get_bbox(bmin, bmax);
                return;
            }
            animlib_get_bone_bbox(char_inst->aim_handle, bmin, bmax);
            bmin.x = bmin.x - char_inst->bbox_max_dimension;
            bmin.y = bmin.y - char_inst->bbox_max_dimension;
            bmin.z = bmin.z - char_inst->bbox_max_dimension;
            bmax.x = char_inst->bbox_max_dimension + bmax.x;
            bmax.y = char_inst->bbox_max_dimension + bmax.y;
            bmax.z = char_inst->bbox_max_dimension + bmax.z;
            return;
        }
    }
    bmin.zero();
    // Magic number: a 10 cm box.
    bmax.x = 0.1f;
    bmax.y = 0.1f;
    bmax.z = 0.1f;
}

// 0x00A3C2F0 (:3686)
// A tag of the static mesh (the item's own mesh, looked up when not loaded), else a character tag or bone.
int item::get_tag_index_from_name(const char* tag_name) const
{
    int index = -1;
    const resource_dependent_data* data = rdd.is_loaded() ? rdd.get_data() : nullptr;
    if (tag_name == nullptr)
    {
        return -1;
    }
    const static_mesh* smesh = data == nullptr ? static_mesh_find(info->name, info->srid) : data->smesh;
    if (smesh != nullptr)
    {
        index = smesh->m_tags.get_tag(tag_name);
        if (index != -1)
        {
            return index;
        }
    }
    if (char_inst != nullptr)
    {
        index = animlib_find_character_tag(char_inst->aim_handle, tag_name);
        if (index == -1)
        {
            index = animlib_find_character_bone(char_inst->aim_handle, tag_name);
        }
    }
    return index;
}

// 0x00A3C390 (:3749)
// A tag in world space from the object's (or the given) placement; the placement itself while not loaded.
bool item::get_tag_world_coords(int tag_index, vector& tag_pos, matrix* tag_orient, vector* obj_pos, matrix* obj_orient) const
{
    const resource_dependent_data* data = rdd.is_loaded() ? rdd.get_data() : nullptr;
    const vector& base_pos = obj_pos != nullptr ? *obj_pos : pos;
    const matrix& base_orient = obj_orient != nullptr ? *obj_orient : orient;
    if (data == nullptr)
    {
        tag_pos = base_pos;
        if (tag_orient != nullptr)
        {
            *tag_orient = base_orient;
        }
        return false;
    }
    if (data->smesh == nullptr)
    {
        if (char_inst != nullptr)
        {
            animlib_get_tag_world_coords(char_inst->aim_handle, tag_index, base_pos, base_orient, tag_pos, tag_orient);
        }
        return true;
    }
    matrix43 base;
    base.set_orientation(base_orient);
    base.set_translation(base_pos);
    matrix43 tag_transform;
    data->smesh->m_tags.get_tag_world_transform(tag_transform, tag_index, base);
    tag_pos = tag_transform.m_translation;
    if (tag_orient != nullptr)
    {
        *tag_orient = tag_transform.m_rotation;
    }
    return true;
}

// 0x00A3C510 (:3784)
bool item::get_tag_local_coords(int tag_index, vector& tag_pos, matrix* tag_orient) const
{
    if (!rdd.is_loaded())
    {
        tag_pos.zero();
        if (tag_orient != nullptr)
        {
            tag_orient->make_identity();
        }
        return false;
    }
    const static_mesh* smesh = rdd.get_data()->smesh;
    if (smesh == nullptr)
    {
        if (char_inst != nullptr)
        {
            animlib_get_tag_local_coords(char_inst->aim_handle, tag_index, tag_pos, tag_orient);
        }
        return true;
    }
    matrix43 tag_transform;
    smesh->m_tags.get_tag_transform(tag_transform, tag_index);
    tag_pos = tag_transform.m_translation;
    if (tag_orient != nullptr)
    {
        *tag_orient = tag_transform.m_rotation;
    }
    return true;
}

// 0x00AA8E20 (:3810)
// Held items show with their holder; their offhand props (prop flag 1) are created on the holder's other hand. A
// remote charge in the hand (or the turret's weapon) tells a multiplayer player's backpack.
void item::hold_item(human* hp, bool held)
{
    item_flags.item_held = held;
    object_set_visibility_plus_children(this, held ? hp->obj_flags._dont_use_me != 0 : false);
    if (held)
    {
        obj_item_flags flags;
        for (int i = 0; i < info->num_props; i++)
        {
            const item_info::item_prop& prop = info->props[i];
            if (!prop.flags.offhand)
            {
                continue;
            }
            item_info* prop_info = object_item_get_info(prop.prop_name);
            // Magic numbers: no physics, simulate, cast shadows and drop shadows.
            *(unsigned int*)&flags = (*(unsigned int*)&flags & 0xFFFC6006) | 0x6006;
            obj_item_props props(prop_info, pos, orient, flags, COLLIDE_ITEM, -1.0f, INVALID_SALVAGE_MATERIAL);
            object* prop_item = World.create_object<item>(props, 0xFFFFFFFF);
            int tag = hp->equip_offhand_tag_index;
            weapon* wp = World.object_cast<weapon>(this);
            if (wp != nullptr && wp->w_info->flags.held_in_left_hand)
            {
                tag = hp->equip_tag_index;
            }
            hp->set_child(prop_item, tag, -1, false, true);
        }
    }
    unsigned int weapon_handle;
    // human_combat_get_reserved_turret, inlined: the turret the human operates
    turret* tp = World.object_from_handle<turret>(hp->combat.reserved_turret);
    if (tp != nullptr && World.object_from_handle<human>(tp->current_operator) == hp)
    {
        weapon_handle = tp->primary_weapon_handle;
    }
    else
    {
        if (hp->equipped_inv_item == nullptr)
        {
            return;
        }
        weapon_handle = hp->equipped_inv_item->weapon_handle;
    }
    weapon* wp = World.object_from_handle<weapon>(weapon_handle);
    if (wp != nullptr && wp->w_info->weapon_class == WEAPON_CLASS_THROWN_CHARGE && hp->get_obj_type() == OT_HUMAN && hp->get_sub_type() == OT_SUB_HUMAN_PLAYER &&
        ((player*)hp)->backpack != nullptr)
    {
        ((player*)hp)->backpack->remote_charge_change(held);
    }
}

// 0x00A91510 (:3848)
// Non-inventory weapons explode as weapons; others hide and go (a multiplayer client keeps them) and explode with
// their info's explosion, upright with a random turn.
void item::explode()
{
    if (special_flags.is_ammo_box)
    {
        Cartographer.remove_icon(get_handle());
    }
    if (get_sub_type() == OT_SUB_ITEM_WEAPON)
    {
        weapon* wp = get_obj_type() == OT_ITEM ? (weapon*)this : nullptr;
        if (wp->w_info->flags.non_inventory)
        {
            wp->explode();
            return;
        }
    }
    if (!Multiplayer || multi_is_server())
    {
        set_is_visible(false); // inlined
        World.destroy_object(this, false);
    }
    matrix explosion_orient(IDENTITY_MATRIX);
    const vector dir = orient.uvec;
    explosion_orient.make(dir);
    // Magic numbers: a quarter turn about x, a random turn about y.
    explosion_orient.rotate_around_local_x(1.5707964f);
    explosion_orient.rotate_around_local_y(fl_rand_range(0.0f, 6.2831855f));
    explosion_create(info->m_explosion_info, this, World.object_from_handle(owner_handle), pos, explosion_orient, &dir, nullptr, false);
}

// 0x00A91660 (:3897)
// Hit points go down by the damage (whole points); an item that can explode does at 0. A multiplayer client takes
// damage only from the server. Returns whether the item had hit points.
bool item::apply_damage(float damage, damaged_by_types from_type, const vector& hit_pos, const vector& hit_normal, const object* source, unsigned int hit_shape_key, bool from_server)
{
    if (Multiplayer && !multi_is_server() && !from_server)
    {
        return true;
    }
    int hp = damage_info.hitpoints;
    if (hp < 1)
    {
        return false;
    }
    if ((int)damage <= hp)
    {
        hp = (int)damage;
    }
    damage_info.hitpoints = damage_info.hitpoints - hp;
    if (damage_info.hitpoints < 1 && damage_info.flags.can_explode && info->m_explosion_info != nullptr)
    {
        explode();
    }
    return true;
}

// 0x00A19090 (:3918)
// Bullet damage (when damaged by bullets) and, for movable bodies, an impulse against the hit normal (0.3 per point).
void item::process_bullet_hit(human* attacker, float damage, const weapon_fire_info& fire_info)
{
    if (!damage_info.flags.damaged_by_bullets)
    {
        return;
    }
    const int original_hitpoints = damage_info.hitpoints;
    apply_damage(damage, DBT_BULLET, fire_info.hit_pos, fire_info.hit_normal, attacker, fire_info.hit_shape_key, false);
    activity_register_damage(attacker, this, damage, original_hitpoints);
    if (havok_handle != 0xFFFFFFFF && havok_body_is_movable(havok_handle))
    {
        // Magic number: 0.3 impulse per damage point.
        const vector impulse(-fire_info.hit_normal.x * damage * 0.3f, -fire_info.hit_normal.y * damage * 0.3f, -fire_info.hit_normal.z * damage * 0.3f);
        havok_body_apply_point_impulse(havok_handle, impulse, fire_info.hit_pos);
    }
}

// 0x00A3C5E0 (:3938)
// Shields (item info flags 0x3000) pass a quarter of the hit to the human holding them, along their up (0x2000) or
// forward vector, and take none. Otherwise movable bodies get the impulse and items damaged by melee take the damage.
bool item::process_melee_hit(object* attacker, float damage, const vector& impulse, const vector& hit_pos, const vector& hit_normal, unsigned int shape_key, const weapon_info& w_info)
{
    if ((info->flags & 0x3000) != 0)
    {
        if (get_parent() != 0xFFFFFFFF)
        {
            object* holder = World.object_from_handle(get_parent());
            if (holder != nullptr && holder->get_obj_type() == OT_HUMAN)
            {
                // Magic number: a quarter of the impulse and damage.
                const vector holder_impulse(impulse.x * 0.25f, impulse.y * 0.25f, impulse.z * 0.25f);
                const vector& axis = ((info->flags >> 13) & 1) != 0 ? orient.uvec : orient.fvec;
                const vector normal = axis.copy_normalize_safe();
                holder->process_melee_hit(attacker, damage * 0.25f, holder_impulse, pos, normal, 0, w_info);
            }
        }
        return false;
    }
    if (havok_handle != 0xFFFFFFFF && havok_body_is_movable(havok_handle))
    {
        havok_body_apply_point_impulse(havok_handle, impulse, hit_pos);
    }
    if (!damage_info.flags.damaged_by_melee)
    {
        return false;
    }
    const int original_hitpoints = damage_info.hitpoints;
    const bool result = apply_damage(damage, DBT_MELEE, hit_pos, hit_normal, attacker, shape_key, false);
    activity_register_damage(attacker, this, damage, original_hitpoints);
    return result;
}

// 0x00A191F0 (:3970)
float item::get_energy_scale()
{
    // Magic number: items take three times the collision energy.
    return 3.0f;
}

// 0x00A19230 (:4010)
unsigned char item::get_physical_material(unsigned int shape_key, const vector* contact_point, const hkpWorldObject* hk_objp) const
{
    return Item_physical_material;
}

// 0x00A19240 (:4018)
unsigned char item::get_effect_material(unsigned int shape_key, const vector* contact_point, const hkpWorldObject* hk_objp) const
{
    return Item_effect_material;
}

// 0x00A916D0 (:4039)
// An owned item loses its body and hides (with update_other_attributes); weapons stop their pickup effect.
void item::set_owner(unsigned int owner, bool update_other_attributes)
{
    owner_handle = owner;
    if (update_other_attributes)
    {
        if (havok_handle != 0xFFFFFFFF)
        {
            havok_body_free(havok_handle, false, true, false);
            havok_handle = 0xFFFFFFFF;
        }
        object_set_visibility_plus_children(this, false);
    }
    if (get_obj_type() == OT_ITEM && get_sub_type() == OT_SUB_ITEM_WEAPON)
    {
        weapon_stop_pickup_effect((weapon*)this);
    }
}

// 0x00A91730 (:4054)
// A released item shows again with a body (streamed items) or physics enabled for its next stream-in.
void item::unset_owner(bool update_other_attributes)
{
    owner_handle = 0xFFFFFFFF;
    if (!update_other_attributes)
    {
        return;
    }
    if (!obj_flags.streamed)
    {
        item_flags.no_physics = 0;
    }
    else if (havok_handle == 0xFFFFFFFF)
    {
        create_physics_object(this);
    }
    object_set_visibility_plus_children(this, true);
}

// 0x00A3C830 (:4071)
void item::set_casts_transparent_shadows_plus_child_items(bool do_cast)
{
    casts_transparent_shadows = do_cast;
    object* child = child_ptr;
    do
    {
        if (child == nullptr)
        {
            return;
        }
        if (child->get_obj_type() == OT_ITEM)
        {
            ((item*)child)->set_casts_transparent_shadows_plus_child_items(do_cast);
        }
        child = child->child_next;
    } while (child != child_ptr);
}

// 0x00A65F80 (:4087)
void item::set_opacity_plus_child_items(float alpha)
{
    render_alpha = alpha;
    queue_render_update(this, obj_flags, 1); // inlined
    object* child = child_ptr;
    do
    {
        if (child == nullptr)
        {
            return;
        }
        if (child->get_obj_type() == OT_ITEM)
        {
            ((item*)child)->set_opacity_plus_child_items(alpha);
        }
        child = child->child_next;
    } while (child != child_ptr);
}

// 0x00A19380 (:4270)
// Item info shape flags: 8 sphere, 0x10 capsule (half the radius, from the center up), 0x20 cylinder (half the
// radius, along y through the center); otherwise a box with its center of mass at the box center.
void item_calc_collision_params(physics_create_info& pci, const item_info* i_info, vector& local_bmin, vector& local_bmax)
{
    pci.flags = 1;
    if (i_info != nullptr)
    {
        const int flags = i_info->flags;
        if ((flags & 8) != 0)
        {
            pci.shape_type = HAVOK_SHAPE_TYPE_SPHERE;
            havok_body_calc_mass_props(pci.radius, pci.mass, pci.precalc_center_of_mass, pci.precalc_tensor);
            return;
        }
        if ((flags & 0x10) != 0)
        {
            const float radius = pci.radius * 0.5f;
            pci.radius = radius;
            pci.shape_type = HAVOK_SHAPE_TYPE_CAPSULE;
            pci.bottom.x = UP_VECTOR.x * radius;
            pci.bottom.y = UP_VECTOR.y * radius;
            pci.bottom.z = UP_VECTOR.z * radius;
            pci.top.x = pci.local_pos.x * 2.0f - pci.bottom.x;
            pci.top.y = pci.local_pos.y * 2.0f - pci.bottom.y;
            pci.top.z = pci.local_pos.z * 2.0f - pci.bottom.z;
            havok_body_calc_mass_props(local_bmin, local_bmax, pci.mass, pci.precalc_center_of_mass, pci.precalc_tensor);
            return;
        }
        if ((flags & 0x20) != 0)
        {
            pci.shape_type = HAVOK_SHAPE_TYPE_CYLINDER;
            pci.radius = pci.radius * 0.5f;
            pci.bottom.set(pci.local_pos.x, local_bmin.y, pci.local_pos.z);
            pci.top.set(pci.local_pos.x, local_bmax.y, pci.local_pos.z);
            havok_body_calc_mass_props(local_bmin, local_bmax, pci.mass, pci.precalc_center_of_mass, pci.precalc_tensor);
            return;
        }
    }
    pci.flags = 3;
    pci.shape_type = HAVOK_SHAPE_TYPE_BOX;
    havok_body_calc_mass_props(local_bmin, local_bmax, pci.mass, pci.precalc_center_of_mass, pci.precalc_tensor);
    pci.precalc_center_of_mass.x = (local_bmin.x + local_bmax.x) * 0.5f;
    pci.precalc_center_of_mass.y = (local_bmin.y + local_bmax.y) * 0.5f;
    pci.precalc_center_of_mass.z = (local_bmin.z + local_bmax.z) * 0.5f;
}

// 0x00A196D0 (:4310)
// The static mesh's collision model, created once from the create info's shape (boxes: half the size, at the local
// placement) and kept in the mesh.
unsigned int item_smesh_get_collision_model(static_mesh* smesh, const physics_create_info& pci)
{
    if (smesh->m_cm_index != 0xFFFFFFFF)
    {
        return smesh->m_cm_index;
    }
    hkpShape* cm;
    switch (pci.shape_type)
    {
    case HAVOK_SHAPE_TYPE_SPHERE:
        cm = havok_cm_manager_create_sphere_shape(&ZERO_VECTOR, pci.radius);
        break;
    case HAVOK_SHAPE_TYPE_CAPSULE:
        cm = havok_cm_manager_create_capsule_shape(&pci.bottom, &pci.top, pci.radius);
        break;
    case HAVOK_SHAPE_TYPE_BOX:
    {
        const vector half(pci.size.x * 0.5f, pci.size.y * 0.5f, pci.size.z * 0.5f);
        const quaternion q(pci.local_orient);
        cm = havok_cm_manager_create_box_shape(&q, &pci.local_pos, &half);
        break;
    }
    case HAVOK_SHAPE_TYPE_CYLINDER:
        cm = havok_cm_manager_create_cylinder_shape(&pci.bottom, &pci.top, pci.radius);
        break;
    default:
        return smesh->m_cm_index;
    }
    if (cm != nullptr)
    {
        smesh->m_cm_index = havok_cm_manager_add(cm);
    }
    return smesh->m_cm_index;
}

// 0x00A64AC0 (:2106)
// Culled by its bounding sphere (the pickup mesh lying alone, the mesh, or the character); streamed items only.
bool item::is_offscreen() const
{
    float radius = 0.0f;
    vector center;
    if (!obj_flags.streamed)
    {
        return false;
    }
    if (char_inst == nullptr)
    {
        const resource_dependent_data* data = rdd.is_loaded() ? rdd.get_data() : nullptr;
        const rl_mesh_instance* mi;
        if (rdd.is_loaded() && get_parent() == 0xFFFFFFFF && data->pickup_smesh_instance_p != nullptr)
        {
            mi = data->pickup_smesh_instance_p;
        }
        else
        {
            // Original: without a loaded resource this reads the mesh instance through a null data pointer.
            mi = data->smesh_instance_p;
        }
        if (mi != nullptr)
        {
            const rl_sphere sphere = mi->get_local_bounding_sphere();
            center = sphere.m_position;
            radius = sphere.m_radius;
        }
        // Original: without a mesh instance the center is uninitialized.
    }
    else
    {
        char_inst->get_bounding_sphere(center, radius);
    }
    center.unrotate(orient);
    center.y = pos.y + center.y;
    center.x = pos.x + center.x;
    center.z = pos.z + center.z;
    return game_render_cull_sphere(center, radius);
}

// 0x00A64C30 (:2141)
// Items of a human whose bones were transformed this frame, or offscreen loose items, skip their transform (unless
// forced or a cutscene rendered). Visible (or cloth) characters process their states; with bones allocated they
// invalidate the bone caches and either transform now (forced or cloth), queue a batch command, or pre-simulate the
// cloth against the holder's (or the local player's) bones. Each loaded bone of the character is invalidated.
void item::update_animlib(float delta_time, bool force_transform)
{
    player* holder = nullptr;
    object* parent_objp = get_parent() != 0xFFFFFFFF ? World.object_from_handle(get_parent()) : nullptr;
    if (parent_objp != nullptr && parent_objp->get_obj_type() == OT_HUMAN)
    {
        // Original: the parent is any human, typed as a player.
        holder = (player*)parent_objp;
        if (!holder->hflags.bones_transformed_this_frame)
        {
            obj_flags.dont_transform = 1;
        }
    }
    else if (get_parent() == 0xFFFFFFFF && is_offscreen())
    {
        obj_flags.dont_transform = 1;
    }
    if (force_transform || cutscene_rendered_this_frame())
    {
        obj_flags.dont_transform = 0;
    }
    last_pos = pos;
    if (char_inst == nullptr || !char_inst->rdd.is_loaded() || (!obj_flags._dont_use_me && cloth_sim == nullptr))
    {
        return;
    }
    vanim_process_states(char_inst->vp);
    if (obj_flags.dont_transform || !animlib_allocate_bones(char_inst->aim_handle))
    {
        return;
    }
    set_render_flags(8);
    const character* base = char_inst->rdd.get_data()->base_character;
    for (int b = 0; b < base->bones_used_info.num_bones; b++)
    {
        animlib_invalidate_cache_bone(char_inst->aim_handle, b, true, true);
    }
    if (force_transform || cloth_sim != nullptr)
    {
        // Magic number: rfg.exe allocas 0xC00 bytes, 64 matrices (Mason_Coat uses all 64).
        matrix43 tmp_matrices[64];
        animlib_process_instance(char_inst->aim_handle, delta_time);
        animlib_transform_all_bones_pre(char_inst->aim_handle, tmp_matrices, nullptr, 0);
        animlib_transform_all_bones_post(char_inst->aim_handle, tmp_matrices, nullptr, 0);
        if (cloth_sim == nullptr)
        {
            return;
        }
    }
    if (attach_info != nullptr)
    {
        attach_info->updated = 0;
    }
    if (cloth_sim == nullptr)
    {
        animlib_transform_command* cmd = animlib_request_batch_start();
        cmd->aim_handle = char_inst->aim_handle;
        cmd->process_instance = 1;
        cmd->transform_all_bones_post = 1;
        cmd->transform_all_bones_pre = 1;
        cmd->frametime = Frametime;
        cmd->human_ptr = this;
        cmd->bone_lod_level = 0;
        cmd->bone_lods = Bone_lods;
        cmd->num_bone_lods = 100;
        animlib_request_batch_end();
        return;
    }
    matrix43* cloth_matrices;
    if (!animlib_get_instanced_model_matrices(char_inst->aim_handle, &cloth_matrices))
    {
        return;
    }
    matrix43* collider_matrices = nullptr;
    const vector* collider_pos = nullptr;
    const matrix* collider_orient = nullptr;
    player* collider = holder != nullptr ? holder : Player;
    if (collider != nullptr && collider->char_inst->rdd.is_loaded())
    {
        if (!animlib_get_instanced_model_matrices(collider->char_inst->aim_handle, &collider_matrices))
        {
            collider_matrices = nullptr;
        }
        collider_orient = &collider->orient;
        collider_pos = &collider->pos;
    }
    cloth_sim->pre_simulate(this, cloth_matrices, collider_matrices, collider_pos, collider_orient);
}

// 0x00A641C0 (:505): every item goes (a multiplayer client may destroy them too).
static void object_item_free_all()
{
    const int num = World.get_num_objects<item>();
    for (int i = 0; i < num; i++)
    {
        item* ip = World.get_object<item>(i);
        ip->remote_obj_flags.client_destroy_permission = 1;
        World.destroy_object(ip, false);
    }
}

// 0x00A192B0 (:4226)
void object_item_level_init_post()
{
}

// 0x00A192C0 (:4232)
// High-res textures marked for unloading are released and leave the list.
void object_item_process_high_res_textures()
{
    for (int i = 0; i < Num_high_res_weapons_loaded; i++)
    {
        item_info* ii = High_res_weapons_loaded[i];
        if (!ii->special_flags.high_res_texture_request_unload)
        {
            continue;
        }
        stream2_release(ii->high_res_srid);
        ii->special_flags.high_res_texture_requested = false;
        ii->special_flags.high_res_texture_request_unload = false;
        for (int k = i; k < Num_high_res_weapons_loaded - 1; k++)
        {
            High_res_weapons_loaded[k] = High_res_weapons_loaded[k + 1];
        }
        Num_high_res_weapons_loaded--;
        i--;
    }
}

// 0x00A66000 (:4355): the items go and every high-res texture is released.
void object_item_level_shutdown()
{
    object_item_free_all();
    for (int i = 0; i < Num_high_res_weapons_loaded; i++)
    {
        High_res_weapons_loaded[i]->special_flags.high_res_texture_request_unload = true;
    }
    object_item_process_high_res_textures();
}

// ---- weapons and pickups (multiplayer.md MP8)

// 0x00A19200 (:3975): a nano faded mesh goes back to its batching setting when the fade ends (scene entity flag 0x80).
static void material_effect_item_completion_callback(void* cb_data, rl_renderable_instance* ri)
{
    if (cb_data == nullptr || ri == nullptr)
    {
        return;
    }
    if (((item*)cb_data)->default_batchable_setting)
    {
        ri->set_flag(0x80, true);
        return;
    }
    ri->set_flag(0x80, false);
}

// 0x00A65F70 (:3337)
void item::enable_physics()
{
    if (havok_handle == 0xFFFFFFFF)
    {
        create_physics_object(this);
    }
}

// 0x00A3C780 (:3984): the nano rifle's fade on the item's mesh (unbatched while it fades) and character (single
// player only).
void item::start_nano_rifle_fade_out(material_effect_parameters& effect_params, const int fading_material_effect)
{
    if (Multiplayer || wrecking_crew_is_active())
    {
        return;
    }
    if (rdd.is_loaded() && rdd.get_data()->smesh_instance_p != nullptr)
    {
        effect_params.update_render_done_cb = material_effect_item_completion_callback;
        effect_params.cb_data = this;
        rdd.get_data()->smesh_instance_p->set_flag(0x80, false);
        material_effects_apply((rl_renderable_instance*)rdd.get_data()->smesh_instance_p, fading_material_effect, &effect_params);
    }
    if (char_inst != nullptr)
    {
        effect_params.update_render_done_cb = nullptr;
        if (char_inst->rdd.is_loaded())
        {
            material_effects_apply((rl_renderable_instance*)char_inst->rdd.get_data()->renderable_instance_p, fading_material_effect, &effect_params);
            return;
        }
        material_effects_apply(nullptr, fading_material_effect, &effect_params);
    }
}

// 0x00A3C870 (:4105)
void item::set_stealth_plus_child_items(float percent)
{
    m_stealth_percent = percent;
    for (object* child = child_ptr; child != nullptr;)
    {
        if (child->get_obj_type() == OT_ITEM)
        {
            ((item*)child)->set_stealth_plus_child_items(m_stealth_percent);
        }
        child = child->child_next;
        if (child == child_ptr)
        {
            return;
        }
    }
}

// 0x00A912E0 (:4366): a respawning item comes back where its body is, shown.
void item::respawn()
{
    if (this == nullptr || !item_flags.respawn)
    {
        return;
    }
    respawn_timer.invalidate();
    if (havok_handle != 0xFFFFFFFF)
    {
        havok_body_get_pos_orient(havok_handle, pos, orient);
    }
    object_set_visibility_plus_children(this, true);
}

// 0x00A909A0 (:423): the multiplayer items of the table (and their preload containers) into the multiplayer pool.
void mp_preload_object_items()
{
    mempool_mark_scope scope(&Common_mempool);
    item_parse_table_shared(&Common_mempool, &MP_item_preload_mempool, &MP_item_preload_mempool, true, PLATFORM_PC, false, -1);
}

// 0x00A182C0 (:456): the multiplayer items' preload containers released and deleted, then their peg unloaded (the
// next mp_preload_object_items loads it again).
void mp_unload_object_items()
{
    for (int i = 0; i < Num_item_infos; i++)
    {
        item_info& info = Item_infos[i];
        if (info.multiplayer_only)
        {
            stream2_release(info.srid);
            stream2_container_delete(info.srid);
            if (info.high_res_srid != 0)
            {
                stream2_container_delete(info.high_res_srid);
            }
            info.srid = 0;
            info.high_res_srid = 0;
        }
    }
    bm_unload_peg("mp_items_containers");
}

// 0x00A3BCA0 (:1668): the item named in the table (the remote items of multiplayer).
obj_item_props::obj_item_props(const char* item_info_name, const vector& pos, const matrix& orient, const obj_item_flags& flags, unsigned int collision_type, float mass_override,
                               salvage_material_type salvage_material)
    : obj_prop_block(pos, orient, nullptr)
{
    set(PROP_ITEM_INFO_NAME, item_info_name);
    set<obj_item_flags>(PROP_ITEM_FLAGS, flags);
    set<unsigned int>(PROP_ITEM_COLLISION_TYPE, collision_type);
    set<float>(PROP_ITEM_MASS_OVERRIDE, mass_override);
    if (salvage_material != INVALID_SALVAGE_MATERIAL)
    {
        set<salvage_material_type>(PROP_ITEM_SALVAGE_MATERIAL, salvage_material);
    }
}

// ---- respawn and explosions

// 0x00A18E70 (:2678)
// A multiplayer item that was picked up and waits for its respawn timer.
bool item::waiting_to_respawn() const
{
    if (this == nullptr || obj_flags._dont_use_me)
    {
        return false;
    }
    return respawn_timer.valid();
}

// 0x00A3C8C0 (:4123)
// An explosion hits an item (not a turret's, not one without a body; no weapon in multiplayer). A cloth item only
// recalculates its wind for 2.5 s and, for an impulse under 2 m/s, takes a gust and starts eroding (reconstructor
// shards) unless its info forbids it. Otherwise an explosive weapon explodes, the impulses are applied (scaled down
// unless the item does not dampen them) and an item damaged by explosions takes the damage.
void item_process_explosion_hit(item* ip, unsigned int damage, const vector& linear_impulse, const vector& angular_impulse)
{
    if (turret_from_item(ip) != nullptr || ip->havok_handle == 0xFFFFFFFF)
    {
        return;
    }
    weapon* wp = nullptr;
    if (ip->get_obj_type() == OT_ITEM && ip->get_sub_type() == OT_SUB_ITEM_WEAPON)
    {
        if (Multiplayer)
        {
            return;
        }
        wp = (weapon*)ip;
    }
    if (ip->cloth_sim != nullptr)
    {
        ip->cloth_sim->wind_recalc_ts.set(2500); // Magic number: 2.5 s of wind recalculation
        const float impulse_squared = (float)((double)linear_impulse.x * linear_impulse.x + (double)linear_impulse.y * linear_impulse.y + (double)linear_impulse.z * linear_impulse.z);
        if (impulse_squared < 4.0f) // Magic number: 2 m/s
        {
            ip->cloth_sim->explosion_gust = linear_impulse.copy_normalize_safe();
            ip->cloth_sim->do_explosion_gust = 1;
            if ((ip->info->flags & 2) == 0) // Magic number: item_info flag 1, no erode
            {
                ip->cloth_deletion_timer.set(0);
                material_effect_parameters params = material_effects_get_parameters(1, 2200); // Magic numbers: flags, 2.2 s
                if (ip->char_inst->rdd.is_loaded())
                {
                    material_effects_apply(ip->char_inst->rdd.get_data()->renderable_instance_p, material_effects_get_reconstructor_shards_id(), &params);
                    ip->item_flags.doing_errode = 1;
                }
            }
        }
        return;
    }
    if (wp != nullptr && wp->w_info->is_explosive())
    {
        wp->explode();
    }
    if (ip->item_flags.dont_dampen_impulse)
    {
        havok_body_apply_linear_impulse(ip->havok_handle, linear_impulse);
        havok_body_apply_angular_impulse(ip->havok_handle, angular_impulse);
    }
    else
    {
        const vector linear(linear_impulse.x * Item_linear_impulse_scalar, linear_impulse.y * Item_linear_impulse_scalar, linear_impulse.z * Item_linear_impulse_scalar);
        havok_body_apply_linear_impulse(ip->havok_handle, linear);
        const vector angular(angular_impulse.x * Item_angular_impulse_scalar, angular_impulse.y * Item_angular_impulse_scalar, angular_impulse.z * Item_angular_impulse_scalar);
        havok_body_apply_angular_impulse(ip->havok_handle, angular);
    }
    if (ip->damage_info.flags.damaged_by_explosions)
    {
        ip->apply_damage((float)damage, DBT_EXPLOSION, ip->pos, ZERO_VECTOR, nullptr, 0xFFFFFFFF, false);
    }
}

// ---- not recovered yet: the other touch scripts and the rest of the stub list.
#if 0
// ---- file-static globals
static unsigned int $S16; // 0x035B5A34

// ---- internal functions
static int object_item_lookup_touch_callback(const char* name); // 0x00A18290 (:366)
static bool item_touch_ask_pickup(human* hp, item* obj_itemp, bool& play_secondary_sound, bool& ask_pickup); // 0x00A3B340 (:669)
static bool item_touch_give_item(human* hp, item* obj_itemp, bool& play_secondary_sound, bool& ask_pickup); // 0x00ABA110 (:722)
static bool item_touch_give_backpack(human* hp, item* obj_itemp, bool& play_secondary_sound, bool& ask_pickup); // 0x00A3B6C0 (:1191)
static unsigned char item_get_auto_remove_level(item* ip); // 0x00A18C00 (:1320)
static void material_effect_item_completion_callback(void* cb_data, rl_renderable_instance* ri); // 0x00A19200 (:3975)
#endif
