// Original: volition/rfg/code/cutscene/cutscene.h
// Implementation: volition/rfg/code/cutscene/cutscene.cpp
//
// Cutscene playback: the cutscene list, playing and stopping one (the load state, the play job, the per-frame shot
// update of camera, actors, animations, effects and options, skipping), the transform of the cutscene into the world
// (offset and rotation, slates), NPC and vehicle actors, the Bink movie on a mover, and the depth of field values the
// shots tween. Placement of cs_depth_of_field inferred (its constructors are in cutscene.cpp).
#pragma once


class checksum_stri;
struct cpu_job;
union cpu_job_arg;
struct cutscene_current_info;
struct cutscene_info;
struct cutscene_navpoint;
class human_info;
class matrix;
class mempool_base;
class npc;
class rfg_mover;
class vector;
class vehicle;
class vehicle_info;

// size 0x18
struct cs_depth_of_field
{
    cs_depth_of_field operator-(const cs_depth_of_field& other); // 0x00705510 (:261)
    cs_depth_of_field operator+(const cs_depth_of_field& other); // 0x007055D0 (:276)

    float max_coc_radius; // 0x00
    float preblur_radius; // 0x04
    float m_near; // 0x08
    float ideal_near; // 0x0C
    float ideal_far; // 0x10
    float m_far; // 0x14
};

float frames_to_seconds(unsigned int frames); // 0x00705410 (:70)
void cutscene_callback_bink_ended(int handle); // 0x00705490 (:186)
void unused_dcf_cutscene_play(); // 0x0070DF00 (:194)
void cutscene_render_loading_screen(); // 0x007054B0 (:233)
cs_depth_of_field operator*(const float scale, const cs_depth_of_field& dof); // 0x00705690 (:291)
unsigned int cutscene_npc_lookup(const char* name); // 0x00708750 (:311)
unsigned int cutscene_vehicle_lookup(const char* name); // 0x007087C0 (:328)
void cs_play_job(cpu_job* de_job, cpu_job_arg arg1, cpu_job_arg arg2); // 0x00710200 (:1129)
void cutscene_system_init(); // 0x007102E0 (:1218)
void cutscene_level_init_pre(); // 0x007059B0 (:1236)
void cutscene_level_shutdown(); // 0x0070E130 (:1240)
const cutscene_current_info* cutscene_get_current_cutscene_info(); // 0x007059C0 (:1253)
cutscene_current_info* cutscene_get_current(); // 0x007059D0 (:1260)
cutscene_info* cutscene_get_cutscene_info(const char* name); // 0x00708B90 (:1271)
const cutscene_info* cutscene_get_cutscene_info(const checksum_stri& name); // 0x00708BD0 (:1290)
const cutscene_info* cutscene_get_cutscene_info(int index); // 0x00708C20 (:1303)
int cutscene_num_cutscenes(); // 0x00708C50 (:1312)
void cutscene_play(const char* name, unsigned int slate_handle, const vector& restore_pos, const matrix& restore_orient); // 0x007059E0 (:1327)
void cutscene_finalize_do(); // 0x00705A90 (:1344)
void cutscene_player_request_skip(); // 0x00710340 (:1371)
void cutscene_do_stop(); // 0x0070E150 (:1379)
void cutscene_stop(); // 0x0070C440 (:1389)
void cutscene_do_frame(); // 0x00710380 (:1615)
void cutscene_interface_render(); // 0x00705AB0 (:1672)
void cutscene_handshake(); // 0x00705AC0 (:1678)
bool cutscene_is_playing(); // 0x00705AD0 (:1686)
bool cutscene_is_finishing(); // 0x00708C60 (:1695)
int cutscene_get_cur_shot(); // 0x00705AE0 (:1710)
bool cutscene_get_no_ai_bbox(vector& bbox_min, vector& bbox_max); // 0x00705B00 (:1724)
void cutscene_set_no_ai_bbox(const vector& bbox_min, const vector& bbox_max); // 0x00705BE0 (:1741)
void cutscene_remove_objects(); // 0x00705C10 (:1751)
void cutscene_set_offset_and_rotation(const cutscene_navpoint& offset_nav, unsigned int slate_handle); // 0x0070C880 (:1866)
void cutscene_transform_point(vector& new_pos, const vector& old_pos); // 0x00708C90 (:1928)
void cutscene_transform_point_for_camera(vector& new_pos, const vector& old_pos); // 0x007090A0 (:1961)
void cutscene_rotate_orient(matrix& new_orient, const matrix& old_orient); // 0x00709580 (:1994)
bool cutscene_select_slate(unsigned int slate_handle, bool for_transform); // 0x00709670 (:2016)
void cutscene_add_effect_object(int handle); // 0x0070CB50 (:2044)
void cutscene_start_next_shot(int shot_number); // 0x0070E190 (:2058)
void cutscene_update_shot(int shot_number); // 0x0070CB80 (:2218)
bool cutscene_disallow_pause(); // 0x00705C30 (:2262)
vector cutscene_get_slate_position(); // 0x007096A0 (:2275)
bool cutscene_current_is_first_shot_processed(); // 0x00705C60 (:2288)
char* str_new_copy(const char* const source, mempool_base& pool); // 0x00705C70 (:2296)
void cutscene_reset_mover_for_bink(); // 0x00709730 (:2311)
void cutscene_setup_mover_for_bink(rfg_mover* rm); // 0x0070CC70 (:2338)
void cutscene_do_handshake(); // 0x0070CDD0 (:2394)
bool cutscene_start_bink_movie(const char* filename); // 0x00705CC0 (:2460)
void cutscene_set_bink_material(const char* material_name); // 0x00705CF0 (:2471)
unsigned int cutscene_get_closest_slate_handle(); // 0x0070CF70 (:2479)
bool cutscene_rendered_this_frame(); // 0x00705D60 (:2491)
npc* cutscene_spawn_npc(const human_info* h_info, const vector& pos, const matrix& orient); // 0x0070E2D0 (:2497)
vehicle* cutscene_spawn_vehicle(const vehicle_info* v_info, const vector& pos, const matrix& orient); // 0x00705D70 (:2518)
