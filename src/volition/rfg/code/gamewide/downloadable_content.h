// Original: volition/rfg/code/gamewide/downloadable_content.h
// Implementation: volition/rfg/code/gamewide/downloadable_content.cpp
//
// Downloadable content bundles: the current DLC id and the multiplayer mask of the installed bundles, the per-bundle
// init and shutdown of everything a bundle can add (items, weapons, vehicles, humans, animations, spawn info,
// missions, activities, personas, effects, interface, localization), the single player DLC levels (dlc_level_info,
// read from the bundle tables) and the temporary container list used while loading a DLC level.
#pragma once


class mempool_base;
enum npc_despawn_type;

// size 0x184
struct dlc_level_info
{
    int dlc_bundle_id; // 0x00
    char name[64]; // 0x04
    char display_name[64]; // 0x44
    char description[64]; // 0x84
    char image_file[64]; // 0xC4
    char file_name[64]; // 0x104
    char intro_video[64]; // 0x144
};

void dlc_cat_file_name(const char* src_name, char* dest_name); // 0x007B9EE0 (:354)
void dlc_set_current_id(int id); // 0x007B9F00 (:359)
int dlc_get_current_id(); // 0x007B9F10 (:364)
void dlc_items_init(int dlc_id); // 0x007B9FB0 (:428)
void dlc_interface_mark(); // 0x007BA170 (:501)
void dlc_interface_init(int bundle_id); // 0x007BA1F0 (:521)
void dlc_interface_shutdown(); // 0x007BA290 (:536)
void dlc_anim_init(int bundle_id); // 0x007CB050 (:554)
void dlc_localization_init(int bundle_id); // 0x007BA380 (:667)
void dlc_localization_shutdown(); // 0x007BA390 (:673)
void dlc_upgrade_shutdown(); // 0x007BA880 (:1024)
void dlc_territories_init(unsigned char* table_data); // 0x007BA890 (:1029)
void dlc_system_shutdown(); // 0x007BA8B0 (:1036)
void dlc_open_marketplace(); // 0x007BA8C0 (:1042)
void dlc_system_init(); // 0x007BA8D0 (:1047)
void dlc_check_media_changed(); // 0x007BA8E0 (:1052)
bool dlc_media_unavailable(); // 0x007BA8F0 (:1057)
unsigned int dlc_count_callback(); // 0x007BA900 (:1070)
const char* dlc_path_callback(unsigned int index); // 0x007BA910 (:1081)
void dlc_level_init_pre(); // 0x007BA940 (:1087)
void dlc_level_shutdown_post(); // 0x007BA990 (:1106)
void dlc_mp_level_init(); // 0x007BAA00 (:1127)
void dlc_level_init(); // 0x007EF1B0 (:1135)
void dlc_level_shutdown(); // 0x007F0B00 (:1377)
bool dlc_allocate_temp_container_list(int num_containers, mempool_base* pool); // 0x007BAA30 (:1486)
bool dlc_store_temp_srid(unsigned int srid); // 0x007BAA90 (:1497)
void dlc_clear_temp_srid(); // 0x007BAAD0 (:1507)
int dlc_bundle_index_to_id(unsigned int index); // 0x007BAB40 (:1529)
int dlc_bundle_id_to_index(int id); // 0x007BAB60 (:1534)
bool dlc_bundle_contains_mp(unsigned int index); // 0x007BAB90 (:1555)
bool dlc_bundle_contains_sp(unsigned int index); // 0x007BABB0 (:1560)
bool dlc_bundle_contains_wc(unsigned int index); // 0x007BABD0 (:1565)
void unused_dcf_load_dlc_level(); // 0x007CB650 (:1570)
void dlc_get_single_player_levels(int& level_count, dlc_level_info*& level_list); // 0x007BABF0 (:1585)
void dlc_remove_dlc_packfiles(); // 0x007BAF30 (:1698)
void dlc_free_single_player_levels(); // 0x007BAF80 (:1722)
void dlc_load_level(dlc_level_info* level); // 0x007BAFB0 (:1731)
unsigned int dlc_get_mp_mask(); // 0x007BB010 (:1740)
int dlc_get_override_id(); // 0x007BB050 (:1760)
bool dlc_should_restart(); // 0x007BB060 (:1767)
void dlc_should_restart(bool restart); // 0x007BB070 (:1772)
void dlc_restart(); // 0x007CB6B0 (:1779)
void dlc_init_console_override(); // 0x007ECE50 (:1789)
void dlc_managed_layer_store_srid(unsigned int srid); // 0x007D7FD0 (:1852)
void dlc_managed_layer_srid_removal(); // 0x007CB6E0 (:1859)
bool find_content_on_controller(unsigned int controller_index); // 0x007BB080 (:1875)
bool dlc_find_new_content(bool first_run_only, bool check_all_controllers); // 0x007BB090 (:1884)
bool dlc_found_corrupt_content(); // 0x007BB0A0 (:1890)
void dlc_get_bundles_sync(); // 0x007ED030 (:1896)
bool dlc_get_morale_disabled(); // 0x007BB0B0 (:1976)
bool dlc_get_morale_disabled_load_game(); // 0x007BB0C0 (:1981)
bool dlc_get_salvage_disabled(); // 0x007BB110 (:1987)
bool dlc_get_tooltips_disabled(); // 0x007BB120 (:1992)
bool dlc_replace_crates_with_totems(); // 0x007BB130 (:1997)
npc_despawn_type dlc_custom_despawn_priority(); // 0x007BB140 (:2002)
void dlc_subtitles(); // 0x007BB150 (:2007)
void dlc_load_all_ui_data(); // 0x007BB160 (:2012)
void dlc_exit_mp(); // 0x007BB170 (:2017)
void dlc_exit_wc(); // 0x007BB1B0 (:2029)
void dlc_add_bundle(int bundle_id, int bundle_index); // 0x007BB1F0 (:2041)
void dlc_check_for_content_removal(); // 0x007BB300 (:2062)

// multiplayer.md MP4: the bagman DLC flag the multiplayer data and settings read (written by the DLC bundle checks).
// Defined in multi_game_mode_bagman.cpp, by its address.
extern bool Bagman_dlc_unlocked; // 0x02FEB638
