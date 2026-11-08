// Original: volition/rfg/code/objects/humans/player_meta_data.cpp
//
// Partial (player_minimal.md P4): resetting the meta data and its upgrades, purchasing and unlocking upgrades, the
// sledgehammer, backpack and jetpack unlock state, loading it from a save and the current district. The rest of the
// recovered stub list stays under #if 0 at the end of the file.
// The external interface is declared in player_meta_data.h.

#include "volition/rfg/code/objects/humans/player_meta_data.h"

#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/rfg/code/control/control.h"
#include "volition/rfg/code/control/control_map.h"
#include "volition/rfg/code/destruction/rfg_mover.h"
#include "volition/rfg/code/event_logging/definitions_events/events.h"
#include "volition/rfg/code/event_logging/gamelog.h"
#include "volition/rfg/code/gameplay/difficulty.h"
#include "volition/rfg/code/gameplay/mission_info.h"
#include "volition/rfg/code/gameplay/object_district.h"
#include "volition/rfg/code/gameplay/upgrades.h"
#include "volition/rfg/code/gamewide/player_profile.h"
#include "volition/rfg/code/gamewide/downloadable_content.h"
#include "volition/rfg/code/gamewide/stats.h"
#include "volition/rfg/code/gameplay/wrecking_crew/wrecking_crew.h"
#include "volition/rfg/code/hud/hud_message.h"
#include "volition/rfg/code/interface/localization/localization_shared.h"
#include "volition/vlib/code/util/unicode_tools.h"
#include "volition/rfg/code/interface/ui_document_hud_tool_tips.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_game/multi_game_object_backpack.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice_shared.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/inventory_item.h"
#include "volition/rfg/code/weapons/weapons.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/os/frametime.h"
#include "volition/vlib/code/util/string_util.h"

#include <string.h>

// ---- globals and file statics
static const float STRESS_SOUND_MAX_DIST = 50.0f; // 0x01311000
// const hammer_upgrade_data Hammer_upgrade_data[16] (0x01311088) is used here too; placed in player.cpp.
static bool Play_sam_upgrade_line = true; // 0x016944BF
static voice_lines Last_upgrade_voice_line = VOICE_LINE_NONE; // 0x016944F0
static unsigned short s_backpacks_unlocked; // 0x03017C50
static unsigned char s_jetpack_upgrade_level; // 0x03017C52
static unsigned char s_jetpack_upgrade_level_backup; // 0x03017C53
static unsigned short s_backpacks_unlocked_backup; // 0x0301C7D0
static uint64_t s_hammers_unlocked_backup; // 0x03023660
// inventory_item* Inv_item_free_list (0x0302366C) is used here too; placed in human.cpp.
// player* Player (0x03023874) is used here too; placed in player.cpp.
static uint64_t s_hammers_unlocked; // 0x030266B0
static bool Purchased_nano_rifle; // 0x0302F0DA
static int Upgrade_play_id; // 0x0302F0DC
static unsigned short Upgrade_radio_id; // 0x0302F0E0
// int Num_inv_items (0x0302F0E8) is used here too; placed in human.cpp.

// ---- internal functions
static void restore_edf_movers(); // 0x00A5E360 (:146)

// 0x00A5E360 (:146)
// After a load, the streamed EDF buildings may restore themselves.
static void restore_edf_movers()
{
    const int num = World.get_num_objects<rfg_mover>();
    for (int i = 0; i < num; i++)
    {
        rfg_mover* mp = World.get_object<rfg_mover>(i);
        if (mp->obj_flags.streamed)
        {
            mp->maybe_flag_restore();
        }
    }
}

// ---- player_meta_data

// 0x00AD8FD0 (.h:19)
player_meta_data::player_meta_data()
{
    reset();
}

// 0x00A05770 (.h:20)
player_meta_data::~player_meta_data()
{
}

// 0x00AD6580 (:162)
// Statistics and upgrades reset; the unlock state is backed up (a save game restores it) and cleared.
void player_meta_data::reset()
{
    salvage = 0;
    mining_count = 0;
    supply_crate_count = 0;
    play_time = 0;
    last_death_time = 0;
    district_hash = 0;
    district_time = 0;
    reset_upgrades();
    s_jetpack_upgrade_level_backup = s_jetpack_upgrade_level;
    s_backpacks_unlocked_backup = s_backpacks_unlocked;
    s_hammers_unlocked_backup = s_hammers_unlocked;
    Purchased_nano_rifle = false;
    s_jetpack_upgrade_level = 0;
    Last_upgrade_voice_line = VOICE_LINE_NONE;
    s_backpacks_unlocked = 0;
    s_hammers_unlocked = 0;
}

// 0x00ACDCF0 (:190)
// Every upgrade starts at level 0 with its default availability (level 0 notified); the free levels are taken at once
// (each notified). The player starts with the sledgehammer (upgrade 19, level 1), or the golden hammer (40) when
// starting a game with it.
void player_meta_data::reset_upgrades()
{
    for (unsigned int id = 0; id < NUM_OF_UPGRADE_DEFINITIONS; id++)
    {
        const upgrade_definition_data* data = upgrade_get_data_by_index(id);
        upgrade_item& up = upgrades[id];
        up.current_level = 0;
        up.availability_bitfield = data->get_default_availability(); // inlined
        up.unlocked_notified_bitfield = 1;
        up.new_notified_bitfield = 1;
        // Magic number: 5, purchase_upgrade's "purchased" result.
        while (purchase_upgrade(id, true, false) == 5)
        {
            const unsigned short bit = (unsigned short)(1 << (up.current_level & 0x1F));
            up.unlocked_notified_bitfield |= bit;
            up.new_notified_bitfield |= bit;
        }
    }
    upgrades[UPGRADE_DEFINITION_SLEDGEHAMMER_LEVEL].new_notified_bitfield |= 2;
    upgrades[UPGRADE_DEFINITION_SLEDGEHAMMER_LEVEL].current_level = 1;
    if (Wants_to_start_game_with_golden_hammer)
    {
        upgrades[UPGRADE_DEFINITION_SLEDGEHAMMER_LEVEL_GOLD].new_notified_bitfield |= 1;
        upgrades[UPGRADE_DEFINITION_SLEDGEHAMMER_LEVEL].current_level = 0;
        upgrades[UPGRADE_DEFINITION_SLEDGEHAMMER_LEVEL_GOLD].current_level = 1;
    }
    clear_unlock_upgrade_notify();
}

// 0x00A36400 (:232)
// The next level of every upgrade that is already available counts as notified (the first 62 upgrades).
void player_meta_data::clear_unlock_upgrade_notify()
{
    for (int i = 0; i < NUM_OF_UPGRADE_DEFINITIONS; i++)
    {
        upgrade_item& up = upgrades[i];
        const unsigned char next = (unsigned char)(up.current_level + 1);
        if (next < 16 && ((up.availability_bitfield >> (next & 0x1F)) & 1) != 0)
        {
            up.unlocked_notified_bitfield |= (unsigned short)(1 << (next & 0x1F));
        }
    }
}

// 0x00A36460 (:252)
// A negative id clears every upgrade's. Magic numbers here and below: 62 upgrades; a level past 15 has no bit.
void player_meta_data::clear_new_upgrade_notify(int upgrade_id)
{
    for (unsigned int i = 0; i < 62; i++)
    {
        const unsigned char next = (unsigned char)(upgrades[i].current_level + 1);
        if (next < 16 && ((upgrades[i].availability_bitfield >> next) & 1) != 0 && (upgrade_id < 0 || i == (unsigned int)upgrade_id))
        {
            upgrades[i].new_notified_bitfield |= 1 << next;
        }
    }
    clear_unlock_upgrade_notify();
}

// 0x00A119D0 (:266)
// The weapons of the owned weapon upgrades go into the weapon cabinet.
void player_meta_data::add_unlocked_weapons_to_weapons_cabinet()
{
    for (unsigned int id = 0; id < NUM_OF_UPGRADE_DEFINITIONS; id++)
    {
        const upgrade_definition_data* data = upgrade_get_data_by_index(id);
        // Magic number: upgrade type 2, the weapon upgrades.
        if (data->get_upgrade_type() == 2 && upgrades[id].current_level != 0 && data->get_weapon_info() != nullptr)
        {
            if (!hud_ui_weapon_cabinet_unlock_weapon(data->get_weapon_info(), true))
            {
                hud_ui_weapon_cabinet_add_weapon(data->get_weapon_info(), true);
            }
        }
    }
}

// 0x00A364E0 (:310)
// A district change logs the time spent in the previous district.
void player_meta_data::set_current_district(district* current_district)
{
    const unsigned int hash = str_hash_string(World.get_object_name(current_district));
    if (district_hash == hash)
    {
        return;
    }
    if (district_hash != 0)
    {
        const unsigned int handle = get_district_by_hash(district_hash);
        district* old_district = handle != 0xFFFFFFFF ? (district*)World.object_from_handle(handle) : nullptr;
        if (old_district != nullptr && old_district->get_obj_type() != OT_DISTRICT)
        {
            old_district = nullptr;
        }
        const vector log_pos = gamelog_get_human_pos(Player);
        const int duration = play_time - district_time;
        const char difficulty = gamelog_get_difficulty();
        const int locale_id = gamelog_get_locale();
        const char pct_game_complete = gamelog_get_pct_complete();
        const int time_played = gamelog_get_time_played();
        const int district_crc = gamelog_get_district_crc(old_district);
        const int time = gamelog_get_logging_time();
        const int checksum = gamelog_get_player_checksum();
        log_event_district(checksum, time, district_crc, time_played, pct_game_complete, locale_id, difficulty, duration, log_pos);
    }
    district_hash = hash;
    district_time = play_time;
}

// 0x00A11A20 (:387)
// The marker 0xFFFFFFFF says the whole upgrade table follows (load_meta_data accepts shorter old saves too).
void player_meta_data::save_meta_data(cfile* save_file)
{
    cf_write_int32(salvage, save_file);
    cf_write_int32(mining_count, save_file);
    cf_write_int32(supply_crate_count, save_file);
    cf_write_uint32(0xFFFFFFFF, save_file);
    cf_write(upgrades, sizeof(upgrades), save_file);
    difficulty_save(save_file);
    cf_write_uint32(district_hash, save_file);
    cf_write_int32(district_time, save_file);
    cf_write_int32(play_time, save_file);
    cf_write_int32(last_death_time, save_file);
}

// 0x00A5E3B0 (:327)
// The saved meta data: the unlock state comes back from its backup; the upgrades are either the whole table (marked
// 0xFFFFFFFF) or the base game's 32 (39 with DLC); then the weapon cabinet, difficulty, district and times, the
// ammo refill and the EDF buildings.
void player_meta_data::load_meta_data(cfile* save_file)
{
    s_backpacks_unlocked = s_backpacks_unlocked_backup;
    s_hammers_unlocked = s_hammers_unlocked_backup;
    s_jetpack_upgrade_level = s_jetpack_upgrade_level_backup;
    salvage = cf_read_int32(save_file);
    mining_count = cf_read_int32(save_file);
    supply_crate_count = cf_read_int32(save_file);
    const unsigned int mark = cf_tell(save_file);
    if (cf_read_uint32(save_file) == 0xFFFFFFFF)
    {
        cf_read(upgrades, sizeof(upgrades), save_file, false);
    }
    else
    {
        cf_seek(save_file, mark, CF_SEEK_SET);
        // Magic numbers: 32 upgrades in old saves, 39 with DLC.
        const int count = World.dlc_bundle_id != -1 ? 0x27 : 0x20;
        cf_read(upgrades, count * (int)sizeof(upgrade_item), save_file, false);
        memset(upgrades + count, 0, (128 - count) * sizeof(upgrade_item));
    }
    add_unlocked_weapons_to_weapons_cabinet();
    difficulty_load(save_file);
    district_hash = cf_read_uint32(save_file);
    district_time = cf_read_int32(save_file);
    play_time = cf_read_int32(save_file);
    last_death_time = cf_read_int32(save_file);
    local_player_fill_ammo();
    restore_edf_movers();
}

// 0x00A11B00 (:455)
int player_meta_data::get_upgrade_value(unsigned int upgrade_id)
{
    const upgrade_definition_data* data = upgrade_get_data_by_index(upgrade_id);
    if (data == nullptr)
    {
        return 0;
    }
    const upgrade_level* ul = data->get_upgrade_level(upgrades[upgrade_id].current_level);
    return ul == nullptr ? 0 : ul->value;
}

// 0x00A11B40 (:473)
bool player_meta_data::is_next_upgrade_available(unsigned int upgrade_id)
{
    const unsigned char next = (unsigned char)(upgrades[upgrade_id].current_level + 1);
    return next < 16 && ((upgrades[upgrade_id].availability_bitfield >> next) & 1) != 0;
}

// 0x00A11B70 (:495)
bool player_meta_data::is_next_upgrade_newly_unlocked(unsigned int upgrade_id)
{
    const unsigned char next = (unsigned char)(upgrades[upgrade_id].current_level + 1);
    return next < 16 && ((upgrades[upgrade_id].availability_bitfield >> next) & 1) != 0 && ((upgrades[upgrade_id].unlocked_notified_bitfield >> next) & 1) == 0;
}

// 0x00A11BB0 (:509)
bool player_meta_data::is_next_upgrade_new_to_the_store(unsigned int upgrade_id)
{
    const unsigned char next = (unsigned char)(upgrades[upgrade_id].current_level + 1);
    return next < 16 && ((upgrades[upgrade_id].availability_bitfield >> next) & 1) != 0 && ((upgrades[upgrade_id].new_notified_bitfield >> next) & 1) == 0;
}

// 0x00A11BF0 (:523)
const upgrade_level* player_meta_data::get_current_upgrade_level(unsigned int upgrade_id)
{
    const upgrade_definition_data* data = upgrade_get_data_by_index(upgrade_id);
    return data != nullptr ? data->get_upgrade_level(upgrades[upgrade_id].current_level) : nullptr;
}

// 0x00A365C0 (:534)
const upgrade_level* player_meta_data::get_next_upgrade_level(unsigned int upgrade_id)
{
    const upgrade_definition_data* data = upgrade_get_data_by_index(upgrade_id);
    if (data == nullptr)
    {
        return nullptr;
    }
    // Upgrade 0 is the jetpack: with one owned from an earlier save, its next level is the second.
    if (upgrade_id == 0 && upgrades[0].current_level == 0 && (s_jetpack_upgrade_level == 1 || s_jetpack_upgrade_level == 2))
    {
        return data->get_upgrade_level(2);
    }
    return data->get_upgrade_level(upgrades[upgrade_id].current_level + 1);
}

// 0x00A11C20 (:559)
// Makes a level available, unlocks the upgrade it unlocks immediately, and takes it when free (or forced).
void player_meta_data::unlock_upgrade(unsigned int upgrade_id, unsigned int level, bool force_purchase)
{
    const upgrade_definition_data* data = upgrade_get_data_by_index(upgrade_id);
    if (data == nullptr)
    {
        return;
    }
    if (level <= data->get_max_level())
    {
        upgrades[upgrade_id].availability_bitfield |= (unsigned short)(1 << (level & 0x1F));
    }
    const upgrade_level* ul = data->get_upgrade_level(level);
    if (ul == nullptr)
    {
        return;
    }
    if (ul->unlock_upgrade_id != NUM_OF_UPGRADE_DEFINITIONS && ul->unlock_action == 0)
    {
        unlock_upgrade(ul->unlock_upgrade_id, ul->unlock_upgrade_level, false);
    }
    purchase_upgrade(upgrade_id, true, force_purchase);
}

// The inventory item of a replaced sledgehammer leaves the inventory (it is freed). Inlined in purchase_upgrade (as
// in player_equip_item).
static void meta_data_remove_inventory_item(player* pp, inventory_item* iip)
{
    if (iip == pp->get_equipped_inv_item())
    {
        inventory_item* desired = pp->desired_equipped_inv_item;
        if (iip == desired)
        {
            pp->set_equipped_inv_item(pp->last_equipped_weapon);
            pp->desired_equipped_inv_item = pp->last_equipped_weapon;
            pp->last_equipped_weapon = pp->second_last_equipped_weapon;
            pp->second_last_equipped_weapon = nullptr;
        }
        else
        {
            pp->set_equipped_inv_item(desired);
        }
    }
    if (iip == pp->desired_equipped_inv_item)
    {
        pp->desired_equipped_inv_item = pp->get_equipped_inv_item();
    }
    if (iip == pp->last_equipped_weapon)
    {
        pp->last_equipped_weapon = pp->second_last_equipped_weapon;
        pp->second_last_equipped_weapon = nullptr;
    }
    if (iip == pp->second_last_equipped_weapon)
    {
        pp->second_last_equipped_weapon = nullptr;
    }
    if (pp->inventory == iip && iip->next == iip)
    {
        iip->next = nullptr;
        iip->prev = nullptr;
        pp->inventory = nullptr;
    }
    else
    {
        if (pp->inventory == iip)
        {
            pp->inventory = iip->next;
        }
        if (iip->prev != nullptr)
        {
            iip->prev->next = iip->next;
        }
        if (iip->next != nullptr)
        {
            iip->next->prev = iip->prev;
        }
        iip->next = nullptr;
        iip->prev = nullptr;
    }
    weapon* wp = nullptr;
    if (iip->weapon_handle != 0xFFFFFFFF)
    {
        wp = (weapon*)World.object_from_handle(iip->weapon_handle);
        if (wp == nullptr || wp->get_obj_type() != OT_ITEM || wp->get_sub_type() != OT_SUB_ITEM_WEAPON)
        {
            wp = nullptr;
        }
        else if (wp->w_info->flags.non_inventory)
        {
            // Magic number: gravity, 9.8 m/s^2.
            const vector gravity(0.0f, -9.8f, 0.0f);
            havok_body_add_custom_force(wp->havok_handle, gravity);
            wp->set_opacity_plus_child_items(1.0f);
        }
    }
    if (Multiplayer == true && multi_is_server() && pp->get_obj_type() == OT_HUMAN && pp->get_sub_type() == OT_SUB_HUMAN_PLAYER)
    {
        multi_game_player_remove_weapon(pp, iip);
    }
    // Original: wp is read without a null check.
    if (pp == Player && wp->info->special_flags.high_res_texture_requested)
    {
        wp->info->special_flags.high_res_texture_request_unload = true;
    }
    iip->weapon_handle = 0xFFFFFFFF;
    inv_item_free(iip); // inlined
}

// 0x00AC3BA0 (:586)
// Buys the next level of an upgrade. Results: 1 no such level, 2 not available, 3 not enough salvage, 4 not free (when
// only free ones are taken, the sledgehammers never are, or there is no player yet), 5 purchased. Purchasing unlocks
// what the level unlocks after purchase, puts weapons into the cabinet, logs paid upgrades (Sam comments them), swaps
// the sledgehammer in slot 3, keeps one backpack (the jetpack level carries over) and wears the armor.
int player_meta_data::purchase_upgrade(unsigned int upgrade_id, bool only_if_free, bool force_purchase)
{
    if (only_if_free && meta_data_get_hammer_upgrade_index((upgrade_definition_names)upgrade_id) > -1)
    {
        return 4;
    }
    const upgrade_definition_data* data = upgrade_get_data_by_index(upgrade_id);
    if (data == nullptr)
    {
        return 1;
    }
    unsigned int next_level;
    // Original: the jetpack bought as a backpack before counts as level 1 (or 2).
    if (upgrade_id == UPGRADE_DEFINITION_JETPACK && upgrades[0].current_level == 0 && (s_jetpack_upgrade_level == 1 || s_jetpack_upgrade_level == 2))
    {
        next_level = 2;
    }
    else
    {
        next_level = upgrades[upgrade_id].current_level + 1;
    }
    const upgrade_level* ul = data->get_upgrade_level(next_level);
    if (ul == nullptr)
    {
        return 1;
    }
    const unsigned char bit = (unsigned char)(upgrades[upgrade_id].current_level + 1);
    if (bit > 15 || ((upgrades[upgrade_id].availability_bitfield >> (bit & 0x1F)) & 1) == 0)
    {
        return 2;
    }
    if (!force_purchase && salvage < (int)ul->get_cost(upgrade_id, false))
    {
        return 3;
    }
    if ((only_if_free && ul->get_cost(upgrade_id, true) != 0) || Player == nullptr)
    {
        return 4;
    }
    if (!force_purchase)
    {
        const int left = salvage - (int)ul->get_cost(upgrade_id, false);
        // Magic number: 30000, the salvage limit.
        salvage = left < 30000 ? left : 30000;
    }
    if (upgrade_id == UPGRADE_DEFINITION_JETPACK)
    {
        const unsigned char level = (unsigned char)(s_jetpack_upgrade_level + 1);
        upgrades[0].current_level = level;
        if (level > 2)
        {
            upgrades[0].current_level = 2;
        }
    }
    else
    {
        upgrades[upgrade_id].current_level++;
    }
    if (ul->unlock_upgrade_id != NUM_OF_UPGRADE_DEFINITIONS && ul->unlock_action == 1)
    {
        unlock_upgrade(ul->unlock_upgrade_id, ul->unlock_upgrade_level, false);
    }
    data = upgrade_get_data_by_index(upgrade_id);
    if (data != nullptr && data->get_upgrade_type() == 2 && upgrades[upgrade_id].current_level == 1 && data->get_weapon_info() != nullptr)
    {
        if (!hud_ui_weapon_cabinet_unlock_weapon(data->get_weapon_info(), true))
        {
            hud_ui_weapon_cabinet_add_weapon(data->get_weapon_info(), true);
        }
        if (upgrade_id == UPGRADE_DEFINITION_NANO_RIFLE)
        {
            Purchased_nano_rifle = true;
        }
    }
    if (ul->get_cost(upgrade_id, false) != 0)
    {
        const vector log_pos = gamelog_get_human_pos(Player);
        const int duration = gamelog_get_game_duration();
        const char difficulty = gamelog_get_difficulty();
        const int locale_id = gamelog_get_locale();
        const char pct_game_complete = gamelog_get_pct_complete();
        const int time_played = gamelog_get_time_played();
        const unsigned char level = upgrades[upgrade_id].current_level;
        const int time = gamelog_get_logging_time();
        const int checksum = gamelog_get_player_checksum();
        log_event_weapon_upgrade(checksum, time, (char)upgrade_id, level, time_played, pct_game_complete, locale_id, difficulty, duration, log_pos);
        if (upgrades[upgrade_id].current_level != 1 || (Last_upgrade_voice_line = (voice_lines)data->get_voice_line(), Last_upgrade_voice_line == VOICE_LINE_NONE))
        {
            Last_upgrade_voice_line = VOICE_LINE_UPGRADE_GENERIC;
        }
        if (Play_sam_upgrade_line)
        {
            // Sam (the guerrilla tech character) comments the purchase.
            const int num = World.get_num_objects<npc>();
            for (int i = 0; i < num; i++)
            {
                npc* np = World.get_object<npc>(i);
                if (np->h_info->flags.guerrilla_tech)
                {
                    if (!audiolib_is_playing(Upgrade_play_id, true) && np->voice != nullptr)
                    {
                        np->voice->persona_play_voice_line(Last_upgrade_voice_line, Player->pos, AUDIOLIB_CUE_PRIORITY_DIALOGUE_NORMAL, Upgrade_play_id, Upgrade_radio_id, nullptr, false, 500);
                    }
                    break;
                }
            }
        }
    }
    if (meta_data_get_hammer_upgrade_index((upgrade_definition_names)upgrade_id) > -1)
    {
        meta_data_unlock_hammer(upgrade_id);
        player* pp = Player;
        weapon* wp = pp->get_equipped_weapon();
        const weapon_class_type equipped_class = wp != nullptr ? wp->w_info->weapon_class : WEAPON_CLASS_NONE;
        const inv_item_info* hammer = nullptr;
        for (const hammer_upgrade_data& hd : Hammer_upgrade_data)
        {
            if ((unsigned int)hd.upgrade_id == upgrade_id)
            {
                hammer = inv_item_get_info(hd.weapon_name);
            }
            else
            {
                upgrades[hd.upgrade_id].current_level = 0;
            }
        }
        if (hammer != nullptr)
        {
            // Magic number: slot 3, the sledgehammer slot.
            inventory_item* old = human_get_inventory_item(pp, 3);
            const int count = old->count;
            if (!(Multiplayer == true && !multi_is_server()))
            {
                old->count = old->count - count;
                if (old->count < 1)
                {
                    meta_data_remove_inventory_item(pp, old); // inlined
                }
            }
            inventory_item* iip = human_add_item_to_inventory(pp, hammer, -1, -1, 3, true, false, true);
            if (equipped_class == WEAPON_CLASS_SLEDGEHAMMER)
            {
                human_equip_inventory_item(pp, iip);
            }
        }
    }
    bool clear_backpacks = false;
    unsigned int keep_backpack = upgrade_id;
    if (upgrade_id == UPGRADE_DEFINITION_JETPACK)
    {
        const unsigned char level = upgrades[0].current_level;
        if (s_jetpack_upgrade_level < level)
        {
            s_jetpack_upgrade_level = level;
        }
        upgrades[0].current_level = s_jetpack_upgrade_level;
        clear_backpacks = true;
    }
    else if (upgrade_id - UPGRADE_DEFINITION_BACKPACK_THRUST < 9)
    {
        upgrades[0].current_level = 0;
        s_backpacks_unlocked |= (unsigned short)(1 << ((upgrade_id - UPGRADE_DEFINITION_BACKPACK_THRUST) & 0x1F));
        if (controls_get_button_for_action(CBA_GAC_TOGGLE_BACKPACK_ACTIVATION, Player_controller_index) == controls_get_button_for_action(CBA_OFC_SPRINT, Player_controller_index) &&
            !ui_tool_tips_tip_has_been_played("MP_Backpack_Activation", false))
        {
            ui_tool_tips_add_tip("MP_Backpack_Activation", 1000, nullptr);
        }
        clear_backpacks = true;
    }
    if (clear_backpacks)
    {
        // only the purchased backpack stays
        for (unsigned int id = UPGRADE_DEFINITION_BACKPACK_THRUST; id < NUM_OF_UPGRADE_DEFINITIONS; id++)
        {
            if (id != keep_backpack)
            {
                upgrades[id].current_level = 0;
            }
        }
        if (upgrade_id == UPGRADE_DEFINITION_JETPACK && upgrades[0].current_level == 1)
        {
            player* pp = Player;
            multi_object_backpack* bp = multi_game_backpack_create(multi_game_backpack_get_backpack_id("Jetpack"), pp->pos, pp->orient);
            bp->event_player_picked_up(pp, true, false);
            if (upgrade_get_armor_level() > 0)
            {
                player_hide_jetpack_straps(pp);
            }
        }
    }
    else if (upgrade_id == UPGRADE_DEFINITION_PLAYER_ARMOR && upgrades[UPGRADE_DEFINITION_PLAYER_ARMOR].current_level != 0)
    {
        player_create_armor_prop(upgrades[UPGRADE_DEFINITION_PLAYER_ARMOR].current_level, Player);
    }
    if (only_if_free)
    {
        upgrades[upgrade_id].unlocked_notified_bitfield |= (unsigned short)(1 << (upgrades[upgrade_id].current_level & 0x1F));
        return 5;
    }
    clear_unlock_upgrade_notify();
    return 5;
}

// 0x00A11C90 (:821)
void player_meta_data::mining_destroyed(unsigned int mine_handle)
{
    // Magic number: 300, the count of the collectable.
    if (Multiplayer || wrecking_crew_is_active() || mining_count >= 300)
    {
        return;
    }
    mining_count++;
    wchar_t count[10];
    sprintf(count, L"%d", mining_count);
    wchar_t message[64];
    sprintf(message, localization_get_display_string_req("HM_MINING_FOUND", nullptr), count);
    hud_message_add_minor(message, -1.0f, false);
    stats_increment_by_one(STATS_ORE_MINED);
    const object* objp = World.object_from_handle(mine_handle);
    const vector item_pos = objp != nullptr ? objp->pos : ZERO_VECTOR;
    // Original: the game time played is passed as the duration too.
    log_event_player_find_collectable(gamelog_get_player_checksum(), gamelog_get_logging_time(), 2, gamelog_get_time_played(), gamelog_get_pct_complete(), gamelog_get_locale(), gamelog_get_difficulty(), gamelog_get_time_played(), item_pos);
    ui_tool_tips_add_tip("Mining", 1000, nullptr);
}

// 0x00A11E00 (:854)
void player_meta_data::supply_crate_destroyed(unsigned int supply_crate_handle)
{
    if (Multiplayer || wrecking_crew_is_active())
    {
        return;
    }
    // Magic numbers: 250 supply crates, 4 of the DLC's war totems.
    if (supply_crate_count >= (dlc_replace_crates_with_totems() ? 4 : 250))
    {
        return;
    }
    supply_crate_count++;
    wchar_t count[10];
    sprintf(count, L"%d", supply_crate_count);
    const wchar_t* format = dlc_replace_crates_with_totems() ? localization_get_display_string_req("HM_WAR_TOTEMS_DESTROYED", nullptr)
                                                             : localization_get_display_string_req("HM_SUPPLY_CRATE_FOUND", nullptr);
    wchar_t message[64];
    sprintf(message, format, count);
    hud_message_add_minor(message, -1.0f, false);
    const object* objp = World.object_from_handle(supply_crate_handle);
    const vector item_pos = objp != nullptr ? objp->pos : ZERO_VECTOR;
    // Original: the game time played is passed as the duration too.
    log_event_player_find_collectable(gamelog_get_player_checksum(), gamelog_get_logging_time(), 3, gamelog_get_time_played(), gamelog_get_pct_complete(), gamelog_get_locale(), gamelog_get_difficulty(), gamelog_get_time_played(), item_pos);
    stats_increment_by_one(STATS_SUPPLY_CRATES_DESTROYED);
}

// 0x00A11AA0 (:416)
// Magic numbers: picked up salvage shows the tool tip after 500 ms; 30000 salvage at most.
void player_meta_data::add_salvage(int amount, bool picked_up)
{
    if (picked_up)
    {
        ui_tool_tips_add_tip("Salvage", 500, nullptr);
    }
    const int total = salvage + amount;
    salvage = total < 30000 ? total : 30000;
}

// 0x00A11AE0 (:438)
int player_meta_data::get_salvage() const
{
    return salvage;
}

// 0x00A11F90 (:892)
// Magic number: the play time stops at 500 hours (in ms).
void player_meta_data::update_play_time()
{
    if (!Multiplayer)
    {
        play_time += fl_round((float)((double)Frametime * 1000.0));
        if (play_time > 1800000000)
        {
            play_time = 1800000000;
        }
    }
}

// 0x00A11FE0 (:902)
void player_meta_data::set_last_death_time()
{
    last_death_time = play_time;
}

// 0x00A11FF0 (:907)
int meta_data_get_last_upgrade_voice_line()
{
    return Last_upgrade_voice_line;
}

// 0x00A12000 (:912)
void meta_data_clear_last_upgrade_voice_line()
{
    Last_upgrade_voice_line = VOICE_LINE_NONE;
}

// 0x00A12010 (:917)
// Answers once per purchase. Buying the nano rifle unlocks Eos's capstone mission.
bool meta_data_purchased_nano_rifle()
{
    const bool purchased = Purchased_nano_rifle;
    Purchased_nano_rifle = false;
    if (purchased && mission_unlock_capstone_in_district(get_district_by_name("Eos")))
    {
        mission_notify_new();
    }
    return purchased;
}

// 0x00A12140 (:1040)
// The jetpack replaces the backpacks.
void player_meta_data::select_jetpack()
{
    upgrades[UPGRADE_DEFINITION_JETPACK].current_level = s_jetpack_upgrade_level;
    for (unsigned int id = UPGRADE_DEFINITION_BACKPACK_THRUST; id < NUM_OF_UPGRADE_DEFINITIONS; id++)
    {
        upgrades[id].current_level = 0;
    }
}

// ---- hammers, backpacks and the jetpack

// 0x00A36620 (:929)
// The plain sledgehammer is always unlocked, the golden one with the profile's golden hammer.
bool meta_data_has_hammer_unlocked(unsigned int hammer_upgrade_id)
{
    if (hammer_upgrade_id == UPGRADE_DEFINITION_SLEDGEHAMMER_LEVEL)
    {
        return true;
    }
    if (hammer_upgrade_id == UPGRADE_DEFINITION_SLEDGEHAMMER_LEVEL_GOLD)
    {
        const player_profile_data* profile = player_profile_get("player_meta_data", Player_controller_index != 0xFFFFFFFF ? Player_controller_index : 0);
        if (profile != nullptr && profile->golden_hammer_unlocked)
        {
            return true;
        }
    }
    const int index = meta_data_get_hammer_upgrade_index((upgrade_definition_names)hammer_upgrade_id);
    return index > -1 && index < 16 && ((s_hammers_unlocked >> index) & 1) != 0;
}

// 0x00A366A0 (:951)
// Original: an unknown upgrade sets the bit past the table (16).
void meta_data_unlock_hammer(unsigned int hammer_upgrade_id)
{
    int index = 0;
    for (; index < 16; index++)
    {
        if ((unsigned int)Hammer_upgrade_data[index].upgrade_id == hammer_upgrade_id)
        {
            break;
        }
    }
    s_hammers_unlocked |= (uint64_t)1 << index;
}

// 0x00A12080 (:968)
int meta_data_get_hammer_upgrade_index(upgrade_definition_names upgrade_id)
{
    for (int i = 0; i < 16; i++)
    {
        if (Hammer_upgrade_data[i].upgrade_id == upgrade_id)
        {
            return i;
        }
    }
    return -1;
}

// 0x00A366E0 (:994)
bool meta_data_has_backpack_unlocked(unsigned int backpack_upgrade_id)
{
    const int index = meta_data_get_backpack_upgrade_index(backpack_upgrade_id);
    return index > -1 && ((s_backpacks_unlocked >> index) & 1) != 0;
}

// 0x00A12100 (:1020)
int meta_data_get_backpack_upgrade_index(unsigned int upgrade_id)
{
    // Magic number: the 9 backpack upgrades follow the sledgehammer levels.
    if (upgrade_id - UPGRADE_DEFINITION_BACKPACK_THRUST < 9)
    {
        return (int)(upgrade_id - UPGRADE_DEFINITION_BACKPACK_THRUST);
    }
    return -1;
}

// 0x00A12120 (:1030)
unsigned char meta_data_get_jetpack_upgrade_level()
{
    return s_jetpack_upgrade_level;
}

// 0x00A12130 (:1035)
void meta_data_set_jetpack_upgrade_level(unsigned char jetpack_upgrade_level)
{
    s_jetpack_upgrade_level = jetpack_upgrade_level;
}

// 0x00A12050 (:958)
uint64_t meta_data_get_hammers_unlocked()
{
    return s_hammers_unlocked;
}

// 0x00A12060 (:963)
void meta_data_set_hammers_unlocked(uint64_t hammers_unlocked)
{
    s_hammers_unlocked = hammers_unlocked;
}

// 0x00A120E0 (:1010)
unsigned short meta_data_get_backpacks_unlocked()
{
    return s_backpacks_unlocked;
}

// 0x00A120F0 (:1015)
void meta_data_set_backpacks_unlocked(unsigned short backpacks_unlocked)
{
    s_backpacks_unlocked = backpacks_unlocked;
}

#if 0
// ---- not ported yet (the rest of the recovered stub list)
static bool (* const Inv_use_scripts[1])(); // 0x01311148
#endif
