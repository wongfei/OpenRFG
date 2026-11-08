// Original: volition/rfg/code/multi/multi_game/multi_game_camera.cpp
//
// The multiplayer camera (header comment in multi_game_camera.h). Part of the multi unity TU (rfg_multi_unity.obj,
// every multi/ .cpp in path order): the file statics are shared by the whole tree, and a shared one is placed in its
// first user (tools/hdrgen/specs/mp0_stubs.py).
//
// A local player's death hands the view to its killer for Mp_camera_stay_dead_time_ms (the death camera), then to the
// spectator camera (multi_game_camera_start). A spectator picks one of four modes: manual (spectator camera markers
// and players by button), action (the most interesting player by score, with one of six automatic views), player
// (one player's view: by button, cycling, or following each killer) and aquarium (the marker that sees most players).
// A change of target cuts (Mp_camera_quick_transition) with Mp_camera_blur_time_ms of blur and signal noise. The points
// of interest (players, explosions) steer the tracking views' look-at and field of view.

#include "volition/rfg/code/multi/multi_game/multi_game_camera.h"

#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/interface_lib/code/vint_external.h"
#include "volition/render_lib/code/rl_color.h"
#include "volition/render_lib/code/rl_frustum.h"
#include "volition/rfg/code/camera/camera_free.h"
#include "volition/rfg/code/camera/camera_shake.h"
#include "volition/rfg/code/camera/camera_third_person.h"
#include "volition/rfg/code/camera/camera_transition.h"
#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/rfg/code/control/control_map.h"
#include "volition/rfg/code/game_render/fullscreen_effects/fb_color_correct.h"
#include "volition/rfg/code/gameplay/wrecking_crew/wrecking_crew.h"
#include "volition/rfg/code/gamewide/game_clock.h"
#include "volition/rfg/code/gamewide/game_render.h"
#include "volition/rfg/code/gamewide/gameaudio.h"
#include "volition/rfg/code/gamewide/gameseq.h"
#include "volition/rfg/code/havok/havok.h"
#include "volition/rfg/code/havok/havok_raycast.h"
#include "volition/rfg/code/interface/ingame/interface.h"
#include "volition/rfg/code/interface/localization/localization_shared.h"
#include "volition/rfg/code/interface/ui_document_hud.h"
#include "volition/rfg/code/interface/ui_document_hud_multiplayer.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/level/world.inl"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_compression.h"
#include "volition/rfg/code/multi/multi_game/multi_game.h"
#include "volition/rfg/code/multi/multi_game/multi_game_lobby.h"
#include "volition/rfg/code/multi/multi_game/multi_game_object_marker.h"
#include "volition/rfg/code/multi/multi_game/multi_game_settings.h"
#include "volition/rfg/code/objects/humans/human.h"
#include "volition/rfg/code/objects/humans/human_move.h"
#include "volition/rfg/code/objects/humans/human_teams.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/vlib/code/util/varray.h"
#include "volition/vlib/code/math/floating.h"
#include "volition/vlib/code/math/matrix.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/math/vector3.h"
#include "volition/vlib/code/os/vconsole.h"
#include "volition/vlib/code/time/timestamp.h"
#include "volition/vlib/code/util/unicode_tools.h"

#include <math.h>
#include <wchar.h>

// ---- file-private types (rfg.pdb)
enum action_camera_submodes
{
    ACSM_INVALID = -1,
    ACSM_AUTOMATIC = 0,
    ACSM_MANUAL = 1,
    NUM_ACTION_CAMERA_SUBMODES = 2,
};

// size 0x30
struct camera_offset_set
{
    vector normal; // 0x00
    vector crouch; // 0x0C
    vector normal_close; // 0x18
    vector crouch_close; // 0x24
};

enum multi_camera_offsets
{
    MCO_DEATH = 0,
    MCO_HIGH_RIGHT = 1,
    MCO_HIGH_LEFT = 2,
    MCO_LOW_RIGHT = 3,
    MCO_LOW_LEFT = 4,
    NUM_MULTI_CAMERA_OFFSETS = 5,
};

enum player_camera_modes
{
    PCM_NONE = -1,
    PCM_MANUAL = 0,
    PCM_CYCLE = 1,
    PCM_KILLER = 2,
    NUM_PLAYER_CAMERA_MODES = 3,
};

// Magic numbers: the spectator views' field of view (degrees), a human's height (1.9 m: its center at half of it) and
// eye height (1.9 / 1.5 m), the most points of interest, the most players and markers the scores keep.
static const float MP_CAMERA_DEFAULT_FOV = 45.0f;
static const float HUMAN_HEIGHT = 1.9f;
static const float HUMAN_EYE_HEIGHT = 1.2666667f;
static const int MAX_POINTS_OF_INTEREST = 2;
static const int MAX_SCORED_PLAYERS = 16;
static const int MAX_MARKERS = 128;

// ---- file-static globals
static const float KILLER_BLUR = 0.6f; // 0x01309DB8
static float Mp_camera_noise_scale = 30.0f; // 0x0167E53C
static float Mp_camera_noise_frequency = 800.0f; // 0x0167E540
// multi_camera_mode Current_mode_camera (0x0167E544) is used here too; placed in multi_game_mode_common.cpp.
// unsigned int Pending_target (0x0167E548) is used here too; placed in multi_game.cpp.
static bool Show_spectator_hud = true; // 0x0167E54C
static bool Mp_camera_quick_transition = true; // 0x0167E54D
static int Mp_camera_stay_dead_time_ms = 1000; // 0x0167E550
static int Mp_camera_blur_time_ms = 400; // 0x0167E554
static int Mp_camera_no_switching_time_ms = 1500; // 0x0167E558
static int Mp_camera_aquarium_min_time_ms = 6000; // 0x0167E55C
static int Mp_camera_aquarium_max_time_ms = 10000; // 0x0167E560
static int Mp_camera_action_min_time_ms = 6000; // 0x0167E564
static int Mp_camera_action_max_time_ms = 8000; // 0x0167E568
static int Mp_camera_action_nearby_enemy_bonus = 3000; // 0x0167E56C
static int Mp_camera_action_enemy_near_reticule_bonus = 2000; // 0x0167E570
static int Mp_camera_action_near_enemy_reticule_bonus = 2000; // 0x0167E574
static float Mp_aquarium_close_score = 10000.0f; // 0x0167E578
static float Mp_aquarium_far_score = 2000.0f; // 0x0167E57C
static float Mp_min_near_tracking_angle = 0.8f; // 0x0167E580
static float Mp_camera_action_nearby_enemy_max_distance_m = 25.0f; // 0x0167E584
static float Mp_camera_action_nearby_explosion_max_distance_m = 25.0f; // 0x0167E588
static float Mp_camera_action_enemy_near_reticule_dot = 0.8660254f; // 0x0167E58C
static float Mp_camera_action_near_enemy_reticule_dot = 0.8660254f; // 0x0167E590
static float Mp_camera_action_players_on_same_team_factor = 0.1f; // 0x0167E594
static int Mp_camera_player_min_time_ms = 15000; // 0x0167E598
static int Mp_camera_player_max_time_ms = 25000; // 0x0167E59C
static int Mp_camera_interest_blend_time_ms = 550; // 0x0167E5A0
static float Mp_camera_interest_weight_player = 1.0f; // 0x0167E5A4
static float Mp_camera_interest_weight_explosion = 3.0f; // 0x0167E5A8
static float Mp_camera_interest_lifetime_explosion = 2.0f; // 0x0167E5AC
static float Mp_camera_orient_adjust_factor = 0.98f; // 0x0167E5B0
static float Mp_camera_fov_adjust_factor = 0.98f; // 0x0167E5B4
static float Mp_camera_radius = 0.08f; // 0x0167E5B8
static float Mp_camera_human_fov_size = 5.0f; // 0x0167E5BC
static float Mp_camera_explosion_fov_size = 10.0f; // 0x0167E5C0
static float Mp_action_cam_fov_mod_min = 0.65f; // 0x0167E5C4
static float Mp_action_cam_fov_mod_max = 2.25f; // 0x0167E5C8
static float Aquarium_view_fov_multipliers[3] = {1.0f, 1.1f, 1.2f}; // 0x0167E5CC
static float Action_camera_mode_chances[6] = {0.4f, 0.3f, 0.18f, 0.02f, 0.1f, 0.0f}; // 0x0167E5D8
static float MP_action_cam_player_pan_camera_chance = 0.4f; // 0x0167E5F0
static vector KILLER_TINT_FACTOR(1.0f, 0.5f, 0.5f); // 0x01692638
// The third person offsets of each offset set (rfg.exe's .data).
static camera_offset_set Camera_offsets[5] = {
    {vector(-0.55f, 1.0f, 0.3f), vector(-0.55f, 1.0f, 0.3f), vector(-0.55f, 1.0f, 0.3f), vector(-0.55f, 1.0f, 0.3f)},
    {vector(0.58f, 1.6f, 0.3f), vector(0.55f, 1.2f, 0.3f), vector(0.5f, 1.57f, 0.3f), vector(0.5f, 1.2f, 0.3f)},
    {vector(-0.55f, 1.6f, 0.3f), vector(-0.55f, 1.2f, 0.3f), vector(-0.5f, 1.57f, 0.3f), vector(-0.5f, 1.2f, 0.3f)},
    {vector(0.55f, 1.0f, 0.3f), vector(0.55f, 1.0f, 0.3f), vector(0.55f, 1.0f, 0.3f), vector(0.55f, 1.0f, 0.3f)},
    {vector(-0.55f, 1.0f, 0.3f), vector(-0.55f, 1.0f, 0.3f), vector(-0.55f, 1.0f, 0.3f), vector(-0.55f, 1.0f, 0.3f)},
}; // 0x01692648
static vector Camera_position_landmark_offset(0.95f, 4.0f, 5.0f); // 0x01692738
// timestamp_percent Camera_transition_timestamp (0x01692744) is used here too; placed in multi_game.cpp.
// timestamp Camera_stay_dead_timestamp (0x0169274C) is used here too; placed in multi_game.cpp.
static timestamp Camera_no_switching_timestamp; // 0x01692750, initial data ffffffff
// timestamp Camera_change_timestamp (0x01692754) is used here too; placed in multi_game_mode_common.cpp.
static int Num_camera_points_of_interest; // 0x02FE5AF8
static bool Action_force_new_target; // 0x02FE5B1F
static float Aquarium_fov; // 0x02FE5BF8
static unsigned int Manual_camera_marker_handle; // 0x02FE5C14
static action_camera_modes Action_camera_mode; // 0x02FE73B4
static int Player_not_visible_time_ms; // 0x02FE8100
// Placed here: its first user, multi_game_mode_bagman.cpp, is not compiled (multiplayer.md MP10).
static point_of_interest Camera_points_of_interest[2]; // 0x02FE8300
// multi_round_winner Winner_last_check (0x02FE8340) is used here too; placed in multi_game_mode_common.cpp.
static team_info Team_disparity; // 0x02FE8348
static point_of_interest Old_camera_points_of_interest[2]; // 0x02FE8360
static action_camera_modes Last_ac_mode; // 0x02FE87D8
static unsigned int Manual_player_marker_handle; // 0x02FE88A8
static action_camera_submodes Action_camera_submode; // 0x02FE88FC
static int Num_old_camera_points_of_interest; // 0x02FE8D10
static bool Aquarium_force_new_target; // 0x02FE8D59
static float Action_fov_modifier; // 0x02FE8DB4
static int Player_dead_time_ms; // 0x02FE8DD8
static action_camera_switch_reason AC_switch_reason; // 0x02FE8E08
// bool Spectating (0x02FEB659) is used here too; placed in multi.cpp.
// multi_camera_view_modes Current_view_mode (0x02FEB680) is used here too; placed in multi_game_mode_common.cpp.
static player_camera_modes Current_player_cam_mode; // 0x02FEB684
static bool Just_switched_modes; // 0x02FEB688
// bool Players_killer (0x02FEB689) is used here too; placed in multi_game.cpp.
static multi_camera_offsets Current_offset; // 0x02FEB68C
static matrix Pending_orient; // 0x02FEBB48
static vector Pending_pos; // 0x02FEDA20
static wchar_t mode_name[128]; // 0x03017A08, multi_game_camera_get_camera_mode_name's text
// Not recovered (debug tweak table only): the Tweak_table_item_* entries of the tunables above.

// ---- internal functions
static bool ignore_spectator_controls(); // 0x0094F670 (:492)
static bool looking_from_intro_spot(); // 0x00979AE0 (:506)
static void look_at_my_dead_body(); // 0x00979B10 (:513)
static void process_death_cam(); // 0x009AC2A0 (:535)
static player* get_next_player(const player* pp); // 0x009966D0 (:577)
static player* get_prev_player(const player* pp); // 0x00996730 (:593)
static int score_player(const player* pp); // 0x0094F6A0 (:610)
static int score_action_player_pair(const player* pp, const player* enemy); // 0x00979C10 (:640)
static unsigned int action_find_best_player(unsigned int exclude_player_handle); // 0x00996790 (:667)
static int score_aquarium_marker(aquarium_score_info& score_info); // 0x009AC470 (:746)
static void get_interest_location(vector& interest_location, const point_of_interest* interest_point); // 0x0094F740 (:864)
static void adjust_interest(vector& lookat_point, float& lookat_weight, const point_of_interest* interest_point); // 0x0094F820 (:887)
static void get_interest_lookat(vector& lookat_point); // 0x0094F960 (:898)
static void remove_interesting_object(int i); // 0x0094FE50 (:915)
static void delete_interesting_object(point_of_interest* interest_list, int& sizeof_interest_list, int i); // 0x0094FEF0 (:932)
static void process_points_of_interest(); // 0x0094FF30 (:940)
static int get_interest_object_index(unsigned int handle); // 0x00950120 (:988)
static bool object_is_interest(unsigned int handle); // 0x00950150 (:998)
static void add_interesting_player(unsigned int interest_pp_handle); // 0x00950180 (:1003)
static void add_interesting_explosion(const vector& explosion_location); // 0x00950210 (:1023)
static const object* get_point_of_interest_object(int index); // 0x00950290 (:1038)
static float process_camera_fov(const vector& camera_pos, const vector& camera_fvec); // 0x009502C0 (:1047)
static void set_multi_camera_mode(multi_camera_mode new_mode); // 0x00950600 (:1095)
static void process_mode_selection(); // 0x00950660 (:1124)
static void set_action_view_mode(action_camera_modes view_mode, unsigned int best_player, unsigned int exclude_marker); // 0x009ACD80 (:1222)
static bool is_player_visible_by_camera(const player* pp); // 0x009AD6C0 (:1499)
static bool any_player_visible_by_camera(); // 0x009AD8F0 (:1528)
static unsigned int get_subsequent_manual_marker(unsigned int cur_handle, bool next); // 0x00996960 (:1570)
static unsigned int get_subsequent_player(unsigned int cur_handle, bool next); // 0x00996AB0 (:1593)
static void process_mode_manual(); // 0x00996B90 (:1611)
static bool aquarium_mode_should_switch_camera(); // 0x009AD950 (:1676)
static void process_mode_aquarium(); // 0x009AD9F0 (:1696)
static action_camera_modes choose_next_action_camera_mode(); // 0x00950820 (:1768)
static void update_action_mode_switch(action_camera_switch_reason switch_reason); // 0x00950880 (:1784)
static bool action_mode_should_switch_camera(); // 0x009ADCA0 (:1790)
static void process_mode_action(); // 0x009ADF00 (:1875)
static void process_mode_player(); // 0x00996E40 (:1927)
static void kill_camera_transition(); // 0x009508A0 (:2028)
static bool process_camera_transition(); // 0x0097A290 (:2042)
static void quick_transition_fov(); // 0x00950970 (:2176)
static void get_localized_text(wchar_t* unicode_buf, int unicode_buf_size, const char* raw_text); // 0x00950DB0 (:2502)

// ---- shared helpers (inlined in the original)

// The player of a handle (nullptr unless a player), as the original's inlined checks read it.
static player* camera_player_from_handle(unsigned int handle)
{
    if (handle == 0xFFFFFFFF)
    {
        return nullptr;
    }
    object* objp = World.object_from_handle(handle);
    return objp != nullptr && objp->get_obj_type() == OT_HUMAN && objp->get_sub_type() == OT_SUB_HUMAN_PLAYER ? (player*)objp : nullptr;
}

// The multiplayer marker of a handle (nullptr unless a marker).
static multi_object_marker* camera_marker_from_handle(unsigned int handle)
{
    if (handle == 0xFFFFFFFF)
    {
        return nullptr;
    }
    object* objp = World.object_from_handle(handle);
    return objp != nullptr && objp->get_obj_type() == OT_MULTI_MARKER ? (multi_object_marker*)objp : nullptr;
}

// The blur and signal noise off (kill_camera_transition's body, inlined at its other users).
static void camera_render_effects_off()
{
    if (!game_is_paused())
    {
        game_render_set_camera_frame_override_blur(0.0f);
        game_render_set_camera_signal_noise_frequency(0.0f);
        game_render_set_camera_signal_noise_scale(0.0f);
    }
}

// The spectator buttons count unless the options or the splash screen are up (inlined).
static bool spectator_controls_blocked()
{
    return gameseq_state_is_active(GS_INGAME_OPTIONS) || (Multiplayer == true && hud_ui_multiplayer_splash_screen_is_active());
}

// The level's spawn node markers, then (with spectator cameras) its spectator camera markers, by priority from the
// last (inlined in each user).
template<int N>
static void collect_camera_markers(farray<const multi_object_marker*, N>& markers, bool spectator_cameras)
{
    for (int i = World.get_num_objects<multi_object_marker>() - 1; i >= 0; i--)
    {
        if (Multi_markers_prioritized[i]->get_marker_type() == MMT_SPAWN_NODE)
        {
            markers.add(Multi_markers_prioritized[i]);
        }
    }
    if (!spectator_cameras)
    {
        return;
    }
    for (int i = World.get_num_objects<multi_object_marker>() - 1; i >= 0; i--)
    {
        if (Multi_markers_prioritized[i]->get_marker_type() == MMT_SPECTATOR_CAMERA)
        {
            markers.add(Multi_markers_prioritized[i]);
        }
    }
}

// The best player of the scores (score_player plus every pair score; minus 4000 for the excluded one), the excluded
// handle without players (action_find_best_player's body, inlined at its other users).
static unsigned int find_best_player_by_score(unsigned int exclude_player_handle)
{
    int scores[MAX_SCORED_PLAYERS];
    unsigned int handles[MAX_SCORED_PLAYERS];
    int count = 0;
    const int num = World.get_num_objects<player>();
    for (int i = 0; i < num; i++)
    {
        player* enemy = World.get_object<player>(i);
        handles[count] = enemy->get_handle();
        scores[count] = score_player(enemy); // inlined
        for (int j = 0; j < num; j++)
        {
            player* pp = World.get_object<player>(j);
            if (pp != enemy)
            {
                scores[count] += score_action_player_pair(pp, enemy);
            }
        }
        // Magic number: the excluded player loses 4000.
        if (enemy->get_handle() == exclude_player_handle)
        {
            scores[count] -= 4000;
        }
        count++;
    }
    if (count <= 0)
    {
        return exclude_player_handle;
    }
    int best = 0;
    for (int i = 1; i < count; i++)
    {
        if (scores[best] < scores[i])
        {
            best = i;
        }
    }
    return handles[best];
}

// 0x0094F640 (:186)
aquarium_score_info::aquarium_score_info(const multi_object_marker* _marker, bool _frustrum_check, bool _orient_check, const player* pp)
{
    marker = _marker;
    frustrum_check = _frustrum_check;
    orient_check = _orient_check;
    must_see_player = pp;
    fov = 0.0f;
}

// 0x0094F630
not_a_vector::operator vector&()
{
    return *(vector*)this;
}

not_a_vector::operator const vector&()
{
    return *(const vector*)this;
}

const vector* not_a_vector::operator&() const
{
    return (const vector*)this;
}

vector* not_a_vector::operator&()
{
    return (vector*)this;
}

// 0x009AC290 (:450): a debug console function (the local player kills itself on the camera).
void unused_dcf_i_killed_me()
{
    player* victim = player_get_local_player();
    multi_game_camera_player_killed(victim, victim);
}

// 0x0094F670 (:492)
static bool ignore_spectator_controls()
{
    return spectator_controls_blocked(); // inlined
}

// 0x00979AE0 (:506): the camera sits on a marker (the level's intro spot).
static bool looking_from_intro_spot()
{
    return camera_marker_from_handle(Pending_target) != nullptr;
}

// 0x00979B10 (:513): the third person camera looks from the target (the killer) at the local player.
static void look_at_my_dead_body()
{
    player* local = player_get_local_player();
    player* target = camera_player_from_handle(Rfg_camera.target_handle);
    if (local == nullptr || target == nullptr || local == target)
    {
        return;
    }
    vector dir = local->pos - target->pos;
    dir.normalize();
    if (camera_get_mode() == CAMERA_THIRD_PERSON_MODE)
    {
        Rfg_camera.lookaround_params.pitch = -asinf(dir.y);
        Rfg_camera.lookaround_params.heading = fl_atan2_safe(dir.x, dir.z);
    }
}

// 0x009AC2A0 (:535)
// The death camera: when the stay-dead time is up, the death camera mode goes and a remote killer is followed in third
// person (tinted red, desaturated, blurred with noise while paused), else the spectator camera starts. Until then a dead
// local player under the camera gets the dumb camera, looking at its body.
static void process_death_cam()
{
    if (Camera_stay_dead_timestamp.valid() && Camera_stay_dead_timestamp.elapsed())
    {
        Camera_stay_dead_timestamp.invalidate();
        if (camera_mode_is_on_stack(CAMERA_DEATH_MODE))
        {
            camera_pop_mode(CAMERA_DEATH_MODE);
        }
        player* killer = camera_player_from_handle(Pending_target);
        if (killer != nullptr && !object_is_local_player(killer))
        {
            camera_free_set_target(killer);
            camera_start_third_person(false);
            rl_color_float tint;
            tint.red = KILLER_TINT_FACTOR.x;
            tint.green = KILLER_TINT_FACTOR.y;
            tint.blue = KILLER_TINT_FACTOR.z;
            tint.alpha = 1.0f;
            fb_color_correct_modulate_tint(tint);
            // Magic number: half the saturation.
            fb_color_correct_modulate_saturation(0.5f);
            if (game_is_paused())
            {
                game_render_set_camera_frame_override_blur(KILLER_BLUR);
                game_render_set_camera_signal_noise_frequency(Mp_camera_noise_frequency * KILLER_BLUR);
                game_render_set_camera_signal_noise_scale(Mp_camera_noise_scale * KILLER_BLUR);
            }
            return;
        }
        multi_game_camera_start();
        return;
    }
    player* target = camera_player_from_handle(Rfg_camera.target_handle);
    if (target != nullptr && object_is_local_player(target) && target->is_killed() && camera_get_mode() != CAMERA_DUMB_MODE)
    {
        camera_start_dumb_mode();
    }
    look_at_my_dead_body();
}

// 0x009966D0 (:577): the player after pp in the world's list (the first one after the last, or when pp is not in the
// list). Original: no check for an empty list (a division by zero).
static player* get_next_player(const player* pp)
{
    const int num = World.get_num_objects<player>();
    int index = -1;
    for (int i = 0; i < num; i++)
    {
        index = i;
        if (World.get_object<player>(i) == pp)
        {
            break;
        }
    }
    return World.get_object<player>((index + 1) % num);
}

// 0x00996730 (:593): the player before pp (the last one before the first, or when pp is not in the list).
static player* get_prev_player(const player* pp)
{
    const int num = World.get_num_objects<player>();
    player* prev = nullptr;
    for (int i = 0; i < num; i++)
    {
        player* cur = World.get_object<player>(i);
        if (cur == pp)
        {
            break;
        }
        prev = cur;
    }
    if (prev == nullptr && num > 0)
    {
        prev = World.get_object<player>(num - 1);
    }
    return prev;
}

// 0x0094F6A0 (:610)
// Magic numbers: 6000 for a shot in the last second, 4000 for moving, 2000 for hurt (below 95%), -50000 when dead.
static int score_player(const player* pp)
{
    if (pp == nullptr)
    {
        return 0;
    }
    int score = 0;
    if (pp->last_fire_time.valid() && pp->last_fire_time.time_since() < 1000)
    {
        score = 6000;
    }
    if (pp->actual_velocity_mag > 1.0f)
    {
        score += 4000;
    }
    if (pp->hit_points < (float)pp->max_hit_points * 0.95f)
    {
        score += 2000;
    }
    if (pp->is_killed())
    {
        score -= 50000;
    }
    return score;
}

// 0x00979C10 (:640)
// What pp adds to enemy's interest: nearness, enemy aiming at pp, pp aiming at enemy (each up to its bonus); a tenth
// between allies.
static int score_action_player_pair(const player* pp, const player* enemy)
{
    vector dir = pp->pos - enemy->pos;
    const float dist = dir.mag_and_normalize_safe();
    const float enemy_aim = enemy->mp_camera_orient.fvec.y * dir.y + enemy->mp_camera_orient.fvec.x * dir.x + enemy->mp_camera_orient.fvec.z * dir.z;
    const float pp_aim = pp->mp_camera_orient.fvec.y * -dir.y + pp->mp_camera_orient.fvec.x * -dir.x + pp->mp_camera_orient.fvec.z * -dir.z;
    const int near_score = (int)fl_parameter(dist, Mp_camera_action_nearby_enemy_max_distance_m * Mp_server_options.get_level()->distance_multiplier, 0.0f, 0.0f,
                                             (float)Mp_camera_action_nearby_enemy_bonus, true);
    const int aimed_score = (int)fl_parameter(enemy_aim, Mp_camera_action_enemy_near_reticule_dot, 1.0f, 0.0f, (float)Mp_camera_action_enemy_near_reticule_bonus, true);
    const int aiming_score = (int)fl_parameter(pp_aim, Mp_camera_action_near_enemy_reticule_dot, 1.0f, 0.0f, (float)Mp_camera_action_near_enemy_reticule_bonus, true);
    const int score = aiming_score + aimed_score + near_score;
    if (human_teams_get_disposition(enemy, pp) == TEAM_DISPOSITION_ALLIED)
    {
        return (int)((float)score * Mp_camera_action_players_on_same_team_factor);
    }
    return score;
}

// 0x00996790 (:667)
static unsigned int action_find_best_player(unsigned int exclude_player_handle)
{
    return find_best_player_by_score(exclude_player_handle);
}

// 0x009AC470 (:746)
// A marker's view of the players: the players it sees (in its frustum when asked; in front within 30 degrees when
// asked), nearest first, up to six; each one unblocked from the marker scores by distance (5 m up to 50 m by the level's
// distance multiplier) once the player that must be seen is (or without one), plus its pair score with that player. 0
// when that player is not seen.
static int score_aquarium_marker(aquarium_score_info& score_info)
{
    const multi_object_marker* marker = score_info.marker;
    int score = 0;
    bool saw_must_see = false;
    if (World.get_num_objects<player>() > 0)
    {
        const float distance_multiplier = Mp_server_options.get_level()->distance_multiplier;
        // Magic number: the six nearest players.
        const player* nearest[6];
        float nearest_dist[6];
        int num_nearest = 0;
        rl_view_frustum frustum;
        if (score_info.frustrum_check)
        {
            Rfg_camera.compute_culling_frustrum(marker->pos, marker->orient, score_info.fov, frustum);
        }
        for (int i = 0; i < World.get_num_objects<player>(); i++)
        {
            const player* pp = World.get_object<player>(i);
            if (score_info.frustrum_check)
            {
                const vector center = pp->pos + UP_VECTOR * 0.5f * HUMAN_HEIGHT;
                // Magic number: a 0.95 m sphere.
                if (!game_render_is_object_visible(frustum, center, 0.95f))
                {
                    continue;
                }
            }
            vector to_player = pp->pos + UP_VECTOR * HUMAN_EYE_HEIGHT - marker->pos;
            const float dist = to_player.mag_and_normalize_safe();
            // Magic number: cos 30 degrees.
            if (score_info.orient_check && marker->orient.fvec.y * to_player.y + to_player.x * marker->orient.fvec.x + marker->orient.fvec.z * to_player.z < 0.8660254f)
            {
                continue;
            }
            if (num_nearest == 0)
            {
                nearest[0] = pp;
                nearest_dist[0] = dist;
                num_nearest = 1;
                continue;
            }
            // Original: an insertion that keeps six (the farthest falls off); the count stays at one.
            for (int j = 0; j < num_nearest; j++)
            {
                if (dist < nearest_dist[j])
                {
                    for (int k = 5; k > j; k--)
                    {
                        nearest[k] = nearest[k - 1];
                        nearest_dist[k] = nearest_dist[k - 1];
                    }
                    nearest[j] = pp;
                    nearest_dist[j] = dist;
                    break;
                }
            }
        }
        for (int i = 0; i < num_nearest; i++)
        {
            const player* pp = nearest[i];
            raycast_input in;
            in.start = marker->pos;
            in.end = pp->pos + UP_VECTOR * HUMAN_EYE_HEIGHT;
            in.radius = 0.0f;
            in.filter_params = nullptr;
            in.filter_callback = nullptr;
            in.m_filter_info = COLLIDE_RAY_DEFAULT;
            in.m_hit_collector_type = raycast_input::COLLECT_CLOSEST_HIT;
            in.m_flags = 0;
            raycast_output out;
            if (havok_raycast(in, out) && out.m_hits[0].hit_object_handle != pp->get_handle())
            {
                continue;
            }
            if (score_info.must_see_player == pp)
            {
                saw_must_see = true;
            }
            if (score_info.must_see_player == pp || saw_must_see || score_info.must_see_player == nullptr)
            {
                // Magic numbers: from 5 m to 50 m (by the level's distance multiplier).
                const float dist = nearest_dist[i];
                if (dist > 5.0f && dist < distance_multiplier * 50.0f)
                {
                    score += (int)fl_parameter(dist, 5.0f, distance_multiplier * 50.0f, Mp_aquarium_close_score, Mp_aquarium_far_score, true);
                }
            }
            if (score_info.must_see_player != nullptr && score_info.must_see_player != pp)
            {
                score += score_action_player_pair(pp, score_info.must_see_player);
            }
        }
    }
    if (score_info.must_see_player != nullptr && !saw_must_see)
    {
        return 0;
    }
    return score;
}

// 0x0094F740 (:864): an object's place (a human's eyes) or the location.
static void get_interest_location(vector& interest_location, const point_of_interest* interest_point)
{
    if (interest_point->type == POIT_OBJECT_HANDLE)
    {
        object* objp = World.object_from_handle(interest_point->point.object_handle);
        if (objp != nullptr)
        {
            interest_location = objp->pos;
            if (objp->get_obj_type() == OT_HUMAN)
            {
                interest_location += UP_VECTOR * HUMAN_EYE_HEIGHT;
            }
        }
    }
    else if (interest_point->type == POIT_LOCATION)
    {
        const vector& location = ((point_of_interest*)interest_point)->point.location;
        interest_location = location;
    }
}

// 0x0094F820 (:887): the point's weighted place (by its blend and weight) and weight added.
static void adjust_interest(vector& lookat_point, float& lookat_weight, const point_of_interest* interest_point)
{
    if (interest_point == nullptr)
    {
        return;
    }
    vector location = ZERO_VECTOR;
    get_interest_location(location, interest_point);
    const float blend = interest_point->interest_blend_in_pct;
    const float weight = interest_point->interest_weight;
    lookat_point.x = lookat_point.x + location.x * blend * weight;
    lookat_point.y = lookat_point.y + location.y * blend * weight;
    lookat_point.z = location.z * blend * weight + lookat_point.z;
    lookat_weight = interest_point->interest_weight * interest_point->interest_blend_in_pct + lookat_weight;
}

// 0x0094F960 (:898): the weighted mean of the points of interest, current and fading out.
static void get_interest_lookat(vector& lookat_point)
{
    float weight = 0.0f;
    for (int i = 0; i < Num_camera_points_of_interest; i++)
    {
        adjust_interest(lookat_point, weight, &Camera_points_of_interest[i]); // inlined
    }
    for (int i = 0; i < Num_old_camera_points_of_interest; i++)
    {
        adjust_interest(lookat_point, weight, &Old_camera_points_of_interest[i]); // inlined
    }
    if (weight > 0.0f)
    {
        const float inverse = 1.0f / weight;
        lookat_point.x = inverse * lookat_point.x;
        lookat_point.y = inverse * lookat_point.y;
        lookat_point.z = lookat_point.z * inverse;
    }
}

// 0x0094FE50 (:915): a point of interest fades out (the old list keeps two, the first replaced when full); the last
// one takes its slot.
static void remove_interesting_object(int i)
{
    int old_index;
    if (Num_old_camera_points_of_interest < MAX_POINTS_OF_INTEREST)
    {
        old_index = Num_old_camera_points_of_interest++;
    }
    else
    {
        old_index = 0;
    }
    Old_camera_points_of_interest[old_index] = Camera_points_of_interest[i];
    delete_interesting_object(Camera_points_of_interest, Num_camera_points_of_interest, i); // inlined
}

// 0x0094FEF0 (:932)
static void delete_interesting_object(point_of_interest* interest_list, int& sizeof_interest_list, int i)
{
    sizeof_interest_list--;
    interest_list[i] = interest_list[sizeof_interest_list];
}

// 0x0094FF30 (:940)
// The points of interest age (those with a lifetime fade out when it runs out; an object that is gone is dropped) and
// blend in over Mp_camera_interest_blend_time_ms; the fading ones blend out and go.
static void process_points_of_interest()
{
    for (int i = 0; i < Num_camera_points_of_interest;)
    {
        point_of_interest& poi = Camera_points_of_interest[i];
        if (poi.lifetime > 0.0f)
        {
            poi.lifetime = poi.lifetime - Frametime;
            if (poi.lifetime < 0.0f)
            {
                remove_interesting_object(i);
                continue;
            }
        }
        if (poi.type == POIT_OBJECT_HANDLE && World.object_from_handle(poi.point.object_handle) == nullptr)
        {
            delete_interesting_object(Camera_points_of_interest, Num_camera_points_of_interest, i); // inlined
            continue;
        }
        float blend = (Frametime * 1000.0f) / (float)Mp_camera_interest_blend_time_ms + poi.interest_blend_in_pct;
        if (1.0f - blend < 0.0f)
        {
            blend = 1.0f;
        }
        poi.interest_blend_in_pct = blend;
        i++;
    }
    const float blend_step = (Frametime * 1000.0f) / (float)Mp_camera_interest_blend_time_ms;
    for (int i = 0; i < Num_old_camera_points_of_interest;)
    {
        point_of_interest& poi = Old_camera_points_of_interest[i];
        float blend = poi.interest_blend_in_pct - blend_step;
        if (0.0f <= 0.0f - blend)
        {
            blend = 0.0f;
        }
        poi.interest_blend_in_pct = blend;
        if (blend > 0.0f)
        {
            i++;
            continue;
        }
        delete_interesting_object(Old_camera_points_of_interest, Num_old_camera_points_of_interest, i); // inlined
    }
}

// 0x00950120 (:988)
static int get_interest_object_index(unsigned int handle)
{
    for (int i = 0; i < Num_camera_points_of_interest; i++)
    {
        if (Camera_points_of_interest[i].type == POIT_OBJECT_HANDLE && Camera_points_of_interest[i].point.object_handle == handle)
        {
            return i;
        }
    }
    return -1;
}

// 0x00950150 (:998)
static bool object_is_interest(unsigned int handle)
{
    return get_interest_object_index(handle) != -1; // inlined
}

// 0x00950180 (:1003): a player not yet of interest becomes one (no lifetime), while there is room.
static void add_interesting_player(unsigned int interest_pp_handle)
{
    if (object_is_interest(interest_pp_handle)) // inlined
    {
        return;
    }
    if (Num_camera_points_of_interest < MAX_POINTS_OF_INTEREST)
    {
        point_of_interest& poi = Camera_points_of_interest[Num_camera_points_of_interest++];
        poi.interest_blend_in_pct = 0.0f;
        poi.interest_weight = Mp_camera_interest_weight_player;
        poi.fov_size = Mp_camera_human_fov_size;
        poi.type = POIT_OBJECT_HANDLE;
        poi.point.object_handle = interest_pp_handle;
        poi.lifetime = -1.0f;
    }
}

// 0x00950210 (:1023): an explosion becomes a point of interest for its lifetime, while there is room.
static void add_interesting_explosion(const vector& explosion_location)
{
    if (Num_camera_points_of_interest < MAX_POINTS_OF_INTEREST)
    {
        point_of_interest& poi = Camera_points_of_interest[Num_camera_points_of_interest++];
        poi.type = POIT_LOCATION;
        poi.point.location.x = explosion_location.x;
        poi.point.location.y = explosion_location.y;
        poi.point.location.z = explosion_location.z;
        poi.interest_blend_in_pct = 0.0f;
        poi.interest_weight = Mp_camera_interest_weight_explosion;
        poi.fov_size = Mp_camera_explosion_fov_size;
        poi.lifetime = Mp_camera_interest_lifetime_explosion;
    }
}

// 0x00950290 (:1038)
static const object* get_point_of_interest_object(int index)
{
    if (index < Num_camera_points_of_interest && Camera_points_of_interest[index].type == POIT_OBJECT_HANDLE)
    {
        return World.object_from_handle(Camera_points_of_interest[index].point.object_handle);
    }
    return nullptr;
}

// 0x009502C0 (:1047)
// The field of view that holds every point of interest: the widest ratio of (its distance off the view axis plus its
// size, by the action modifier) to its distance, as an angle in degrees; 45 degrees when that is below 0.01 rad.
// Original: LTCG returns the angle in XMM0.
static float process_camera_fov(const vector& camera_pos, const vector& camera_fvec)
{
    float widest = 0.0f;
    for (int i = 0; i < Num_camera_points_of_interest; i++)
    {
        vector location = ZERO_VECTOR;
        get_interest_location(location, &Camera_points_of_interest[i]); // inlined
        const vector d = camera_pos - location;
        vector along;
        vec_projection_parallel(along, d, camera_fvec);
        const float off_axis = sqrtf((d.x - along.x) * (d.x - along.x) + (d.y - along.y) * (d.y - along.y) + (d.z - along.z) * (d.z - along.z));
        const float ratio = ((off_axis + Camera_points_of_interest[i].fov_size) * Action_fov_modifier) / sqrtf(d.x * d.x + d.y * d.y + d.z * d.z);
        if (widest - ratio < 0.0f)
        {
            widest = ratio;
        }
    }
    const float angle = atanf(widest);
    // Magic number: 0.01 rad.
    if (angle < 0.01f)
    {
        return MP_CAMERA_DEFAULT_FOV;
    }
    // Magic number: 57.29583 degrees a radian.
    return (float)((double)angle * 57.295829772949219);
}

// 0x00950600 (:1095)
static void set_multi_camera_mode(multi_camera_mode new_mode)
{
    if (new_mode == Current_mode_camera)
    {
        return;
    }
    Current_mode_camera = new_mode;
    switch (new_mode)
    {
    case MCM_MANUAL:
        Current_view_mode = MCVM_MANUAL;
        return;
    case MCM_ACTION:
    case MCM_AQUARIUM:
        Camera_change_timestamp.invalidate();
        Current_view_mode = MCVM_DUMB;
        return;
    case MCM_PLAYER:
        Current_player_cam_mode = PCM_MANUAL;
        return;
    default:
        return;
    }
}

// 0x00950660 (:1124): during a round the mode buttons pick a mode; the scroll bar steps back through them (aquarium
// after manual) and forward from aquarium to manual.
static void process_mode_selection()
{
    if (Multi_match_info.match_state != MGMS_GAMEPLAY || spectator_controls_blocked())
    {
        return;
    }
    if (controls_process_button_just_pressed(CBA_SPECTATOR_MODE_CUSTOM, Player_controller_index))
    {
        set_multi_camera_mode(MCM_MANUAL); // inlined
    }
    else if (controls_process_button_just_pressed(CBA_SPECTATOR_MODE_AQUARIUM, Player_controller_index))
    {
        set_multi_camera_mode(MCM_AQUARIUM); // inlined
    }
    else if (controls_process_button_just_pressed(CBA_SPECTATOR_MODE_ACTION, Player_controller_index))
    {
        set_multi_camera_mode(MCM_ACTION); // inlined
    }
    else if (controls_process_button_just_pressed(CBA_SPECTATOR_MODE_PLAYER, Player_controller_index))
    {
        set_multi_camera_mode(MCM_PLAYER); // inlined
    }
    // Magic number: the scroll bar's 0.1 dead zone.
    if (controls_get_analog(CAA_SCROLL_BAR, Player_controller_index) <= 0.1f)
    {
        if (controls_get_analog(CAA_SCROLL_BAR, Player_controller_index) < -0.1f)
        {
            // Original: "back" adds one to the mode; only MCM_NONE + 1 wraps (to aquarium), so it never does.
            multi_camera_mode new_mode = (multi_camera_mode)(Current_mode_camera + 1);
            if (new_mode == MCM_NONE)
            {
                new_mode = MCM_AQUARIUM;
            }
            set_multi_camera_mode(new_mode);
        }
    }
    else if (Current_mode_camera == MCM_AQUARIUM)
    {
        set_multi_camera_mode(MCM_MANUAL);
    }
}

// 0x009507F0 (:1153)
void multi_game_camera_set_spectator_camera_to_action()
{
    if (Spectating && Current_mode_camera != MCM_ACTION)
    {
        set_multi_camera_mode(MCM_ACTION); // inlined
    }
}

// 0x009ACB30 (:1160)
// The marker (spawn nodes, then spectator cameras; not the excluded one) that sees best_player best: the first one's
// score, then each one's score plus a random bonus. True when one scores above 0 (Pending_target).
bool action_find_best_marker(unsigned int best_player, unsigned int exclude_marker_handle)
{
    farray<const multi_object_marker*, MAX_MARKERS> markers;
    collect_camera_markers(markers, true);
    const multi_object_marker* exclude = camera_marker_from_handle(exclude_marker_handle);
    if (exclude != nullptr)
    {
        for (int i = 0; i < markers.size();)
        {
            if (markers[i] == exclude)
            {
                markers.remove(i);
            }
            else
            {
                i++;
            }
        }
    }
    const player* must_see = camera_player_from_handle(best_player);
    int best_score = 0;
    const multi_object_marker* best = nullptr;
    if (markers.size() > 0)
    {
        best = markers[0];
        aquarium_score_info info(best, false, false, must_see);
        best_score = score_aquarium_marker(info);
    }
    for (int i = 0; i < markers.size(); i++)
    {
        aquarium_score_info info(markers[i], false, false, must_see);
        int score = score_aquarium_marker(info);
        if (score > 0)
        {
            // Magic number: a random bonus up to 6000 (x87: fl_rand times a double, truncated).
            score += (int)((double)fl_rand() * 6000.0);
        }
        if (best_score < score)
        {
            best_score = score;
            best = markers[i];
        }
    }
    if (best != nullptr)
    {
        Pending_target = best->get_handle();
    }
    return best_score > 0;
}

// 0x009ACD80 (:1222)
// The automatic view for a mode around best_player:
// - player tracking: a marker that sees the player (panning or tracking), else its point of view;
// - near tracking: a marker that tracks the player with its nearest enemy in front (within 25 m), else its point of
//   view;
// - forward / reverse: the player's point of view (or looking back at it);
// - enemy tracking: from the player, tracking the best other player (a marker when that is the player itself);
// - landmark: a fixed place behind the player's heading (pulled in to the first wall).
// A new target gets a random fov modifier and offset.
static void set_action_view_mode(action_camera_modes view_mode, unsigned int best_player, unsigned int exclude_marker)
{
    const unsigned int tracked_player = best_player;
    Just_switched_modes = true;
    bool track_one_player = false;
    switch (view_mode)
    {
    case ACM_PLAYER_TRACKING:
        Current_view_mode = fl_rand() < MP_action_cam_player_pan_camera_chance ? MCVM_STATIC_PAN : MCVM_STATIC_TRACKING;
        if (!action_find_best_marker(best_player, exclude_marker))
        {
            Action_camera_mode = ACM_FORWARD;
            Current_view_mode = MCVM_TARGET_POV;
            Pending_target = best_player;
            break;
        }
        Num_old_camera_points_of_interest = 0;
        track_one_player = true;
        break;
    case ACM_NEAR_TRACKING:
        Current_view_mode = MCVM_STATIC_TRACKING;
        if (action_find_best_marker(best_player, exclude_marker))
        {
            Camera_points_of_interest[0].interest_blend_in_pct = 1.0f;
            Camera_points_of_interest[0].interest_weight = Mp_camera_interest_weight_player;
            Camera_points_of_interest[0].fov_size = Mp_camera_human_fov_size;
            Camera_points_of_interest[0].type = POIT_OBJECT_HANDLE;
            Camera_points_of_interest[0].point.object_handle = best_player;
            Camera_points_of_interest[0].lifetime = -1.0f;
            Num_camera_points_of_interest = 1;
            Num_old_camera_points_of_interest = 0;
            player* tracked = World.object_from_handle<player>(best_player);
            if (tracked != nullptr)
            {
                for (int i = 0; i < World.get_num_objects<player>(); i++)
                {
                    player* other = World.get_object<player>(i);
                    if (other == tracked)
                    {
                        continue;
                    }
                    // Original: the marker is not checked (Pending_target was just set to one).
                    const multi_object_marker* marker = camera_marker_from_handle(Pending_target);
                    vector to_other = other->pos - marker->pos;
                    vector to_tracked = tracked->pos - marker->pos;
                    to_other.normalize();
                    to_tracked.normalize();
                    if (to_tracked.x * to_other.x + to_tracked.y * to_other.y + to_tracked.z * to_other.z < Mp_min_near_tracking_angle)
                    {
                        break;
                    }
                    const vector between = other->pos - tracked->pos;
                    const double max_distance = (double)Mp_camera_action_nearby_enemy_max_distance_m * (double)Mp_server_options.get_level()->distance_multiplier;
                    if ((double)between.mag_squared() < max_distance * max_distance)
                    {
                        point_of_interest& poi = Camera_points_of_interest[Num_camera_points_of_interest];
                        poi.type = POIT_OBJECT_HANDLE;
                        poi.point.object_handle = other->get_handle();
                        poi.interest_blend_in_pct = 1.0f;
                        poi.interest_weight = Mp_camera_interest_weight_player;
                        poi.fov_size = Mp_camera_human_fov_size;
                        poi.lifetime = -1.0f;
                        Num_camera_points_of_interest++;
                        if (Num_camera_points_of_interest > 1)
                        {
                            break;
                        }
                    }
                }
            }
            break;
        }
        Action_camera_mode = ACM_FORWARD;
        Current_view_mode = MCVM_TARGET_POV;
        Pending_target = best_player;
        break;
    case ACM_FORWARD:
        Current_view_mode = MCVM_TARGET_POV;
        Pending_target = best_player;
        break;
    case ACM_REVERSE:
        Current_view_mode = MCVM_TARGET_REVERSE_POV;
        Pending_target = best_player;
        break;
    case ACM_ENEMY_TRACK:
    {
        Current_view_mode = MCVM_TARGET_TRACKING;
        Pending_target = best_player;
        Camera_points_of_interest[0].type = POIT_OBJECT_HANDLE;
        const unsigned int enemy = find_best_player_by_score(tracked_player); // inlined
        Camera_points_of_interest[0].interest_blend_in_pct = 1.0f;
        Camera_points_of_interest[0].interest_weight = Mp_camera_interest_weight_player;
        Camera_points_of_interest[0].fov_size = Mp_camera_human_fov_size;
        Camera_points_of_interest[0].lifetime = -1.0f;
        Num_camera_points_of_interest = 1;
        Num_old_camera_points_of_interest = 0;
        Camera_points_of_interest[0].point.object_handle = enemy;
        if (tracked_player == enemy)
        {
            action_find_best_marker(tracked_player, exclude_marker);
            Current_view_mode = MCVM_STATIC_TRACKING;
            Action_camera_mode = ACM_PLAYER_TRACKING;
        }
        break;
    }
    case ACM_LANDMARK:
    {
        Current_view_mode = MCVM_STATIC_TRACKING;
        Pending_target = 0xFFFFFFFF;
        player* pp = camera_player_from_handle(best_player);
        if (pp != nullptr)
        {
            Pending_pos = Camera_position_landmark_offset;
            float heading;
            pp->mp_camera_orient.extract_heading(heading);
            Pending_pos.rotate_around_y_axis(heading);
            Pending_pos = pp->pos + Pending_pos;
            raycast_input in;
            in.start = pp->pos + UP_VECTOR * HUMAN_HEIGHT;
            in.end = Pending_pos;
            in.filter_params = nullptr;
            in.filter_callback = nullptr;
            in.m_filter_info = COLLIDE_RAY_DEFAULT;
            in.m_hit_collector_type = raycast_input::COLLECT_CLOSEST_HIT;
            in.m_flags = 0;
            in.radius = Mp_camera_radius;
            raycast_output out;
            if (havok_spherecast(in, out))
            {
                Pending_pos = out.m_hits[0].hit_center_pos;
            }
        }
        track_one_player = true;
        break;
    }
    default:
        break;
    }
    if (track_one_player)
    {
        Camera_points_of_interest[0].type = POIT_OBJECT_HANDLE;
        Camera_points_of_interest[0].interest_blend_in_pct = 1.0f;
        Camera_points_of_interest[0].interest_weight = Mp_camera_interest_weight_player;
        Camera_points_of_interest[0].fov_size = Mp_camera_human_fov_size;
        Num_camera_points_of_interest = 1;
        Camera_points_of_interest[0].lifetime = -1.0f;
        Camera_points_of_interest[0].point.object_handle = best_player;
    }
    if (Pending_target != Rfg_camera.target_handle)
    {
        Action_fov_modifier = fl_rand_range(Mp_action_cam_fov_mod_min, Mp_action_cam_fov_mod_max);
        Current_offset = (multi_camera_offsets)int_rand_range(0, 4);
    }
}

// 0x00979E90 (:1384)
// The view of the current view mode, once the camera is on its target: the static views turn to the points of
// interest (a pan keeps 45 degrees, tracking fits them in), the point of view ones follow the target's camera (or look
// back at it) by Mp_camera_orient_adjust_factor, the target tracking one looks from the target at the points of
// interest. The fov eases by Mp_camera_fov_adjust_factor (at once after a mode switch).
void process_view_mode()
{
    vector lookat = ZERO_VECTOR;
    process_points_of_interest();
    get_interest_lookat(lookat);
    float fov;
    matrix view;
    switch (Current_view_mode)
    {
    case MCVM_STATIC_TRACKING:
    case MCVM_STATIC_PAN:
    {
        if (Pending_target != Rfg_camera.target_handle)
        {
            return;
        }
        vector dir = lookat - Rfg_camera.ideal_pos;
        dir.normalize_safe();
        view.make_quick(dir);
        view.orthogonalize();
        camera_set_orient(view);
        if (Current_view_mode == MCVM_STATIC_PAN)
        {
            fov = MP_CAMERA_DEFAULT_FOV;
        }
        else
        {
            fov = process_camera_fov(Rfg_camera.real_pos, view.fvec);
        }
        break;
    }
    case MCVM_TARGET_POV:
    case MCVM_TARGET_REVERSE_POV:
    {
        if (Pending_target != Rfg_camera.target_handle)
        {
            return;
        }
        player* pp = World.object_from_handle<player>(Rfg_camera.target_handle);
        if (pp == nullptr)
        {
            return;
        }
        const matrix* orient = &pp->mp_camera_orient;
        if (Current_view_mode == MCVM_TARGET_REVERSE_POV)
        {
            const vector back(-pp->mp_camera_orient.fvec.x, -pp->mp_camera_orient.fvec.y, -pp->mp_camera_orient.fvec.z);
            view.make_quick(back);
            view.orthogonalize();
            orient = &view;
        }
        float p;
        float b;
        float h;
        orient->extract_angles(p, b, h);
        Rfg_camera.lookaround_params.heading = fl_angle_lerp_radians(Rfg_camera.lookaround_params.heading, h, exp_lerp_factor(Mp_camera_orient_adjust_factor, Frametime));
        const float factor = exp_lerp_factor(Mp_camera_orient_adjust_factor, Frametime);
        Rfg_camera.ideal_fov = MP_CAMERA_DEFAULT_FOV;
        Rfg_camera.lookaround_params.pitch = factor * (p - Rfg_camera.lookaround_params.pitch) + Rfg_camera.lookaround_params.pitch;
        return;
    }
    case MCVM_TARGET_TRACKING:
    {
        if (Pending_target != Rfg_camera.target_handle)
        {
            return;
        }
        player* pp = World.object_from_handle<player>(Rfg_camera.target_handle);
        if (pp == nullptr)
        {
            return;
        }
        vector dir = lookat - pp->pos;
        // Magic number: at least 25 cm away.
        if (dir.mag_and_normalize_safe() <= 0.25f)
        {
            return;
        }
        view.make_quick(dir);
        view.orthogonalize();
        float p;
        float b;
        float h;
        view.extract_angles(p, b, h);
        Rfg_camera.lookaround_params.heading = h;
        Rfg_camera.lookaround_params.pitch = p;
        fov = process_camera_fov(Rfg_camera.real_pos, view.fvec);
        break;
    }
    default:
        return;
    }
    if (!Just_switched_modes)
    {
        Rfg_camera.ideal_fov = exp_lerp_factor(Mp_camera_fov_adjust_factor, Frametime) * (fov - Rfg_camera.ideal_fov) + Rfg_camera.ideal_fov;
        return;
    }
    Just_switched_modes = false;
    Rfg_camera.ideal_fov = fov;
}

// 0x009AD6C0 (:1499): in the view and not hidden from the camera (the ray to its eyes hits it or nothing).
static bool is_player_visible_by_camera(const player* pp)
{
    if (pp == nullptr)
    {
        return false;
    }
    const vector center = pp->pos + UP_VECTOR * 0.5f * HUMAN_HEIGHT;
    // Magic number: a 0.95 m sphere.
    if (!game_render_is_object_visible(center, 0.95f))
    {
        return false;
    }
    raycast_input in;
    in.radius = 0.0f;
    in.start = Rfg_camera.real_pos;
    in.end = pp->pos + UP_VECTOR * HUMAN_EYE_HEIGHT;
    in.m_filter_info = COLLIDE_RAY_DEFAULT;
    in.filter_params = nullptr;
    in.filter_callback = nullptr;
    in.m_hit_collector_type = raycast_input::COLLECT_CLOSEST_HIT;
    in.m_flags = 0;
    raycast_output out;
    if (havok_raycast(in, out))
    {
        return out.m_hits[0].hit_object_handle == pp->get_handle();
    }
    return true;
}

// 0x009AD8F0 (:1528)
static bool any_player_visible_by_camera()
{
    for (int i = 0; i < World.get_num_objects<player>(); i++)
    {
        if (is_player_visible_by_camera(World.get_object<player>(i)))
        {
            return true;
        }
    }
    return false;
}

// <16> 0x00989630 (:1541)
// 0x009895B0 / 0x00989630 (:1541): the handle after (or before) cur_handle, a random one when it is not in the list.
template<int N>
static unsigned int get_subsequent_common(farray<unsigned int, N>& handles, unsigned int cur_handle, bool next)
{
    const unsigned int num = (unsigned int)handles.size();
    if (num == 0)
    {
        return 0xFFFFFFFF;
    }
    for (int i = 0; i < (int)num; i++)
    {
        if (handles[i] == cur_handle)
        {
            if (next)
            {
                return handles[(i + 1) % (int)num];
            }
            return handles[(int)(i - 1 + num) % (int)num];
        }
    }
    return handles[vrand() % num];
}

// 0x00996960 (:1570): among the spawn nodes, plus the spectator cameras during a round while the local player lives.
static unsigned int get_subsequent_manual_marker(unsigned int cur_handle, bool next)
{
    player* local = player_get_local_player();
    farray<unsigned int, MAX_MARKERS> handles;
    for (int i = World.get_num_objects<multi_object_marker>() - 1; i >= 0; i--)
    {
        if (Multi_markers_prioritized[i]->get_marker_type() == MMT_SPAWN_NODE)
        {
            handles.add(Multi_markers_prioritized[i]->get_handle());
        }
    }
    if (Multi_match_info.match_state == MGMS_GAMEPLAY && (local == nullptr || !local->is_killed()))
    {
        for (int i = World.get_num_objects<multi_object_marker>() - 1; i >= 0; i--)
        {
            if (Multi_markers_prioritized[i]->get_marker_type() == MMT_SPECTATOR_CAMERA)
            {
                handles.add(Multi_markers_prioritized[i]->get_handle());
            }
        }
    }
    return get_subsequent_common(handles, cur_handle, next);
}

// 0x00996AB0 (:1593)
static unsigned int get_subsequent_player(unsigned int cur_handle, bool next)
{
    farray<unsigned int, MAX_SCORED_PLAYERS> handles;
    for (int i = 0; i < World.get_num_objects<player>(); i++)
    {
        handles.add(World.get_object<player>(i)->get_handle());
    }
    return get_subsequent_common(handles, cur_handle, next);
}

// 0x00996B90 (:1611)
// Manual: not while a transition runs (outside the stay-dead time) or switching is held off; a spectator in a round
// steps through the players, anyone through the markers (also by the Wrecking Crew hint clicks), with the switch cue.
static void process_mode_manual()
{
    const bool transitioning = Camera_transition_timestamp.valid() && !Camera_stay_dead_timestamp.valid();
    const bool spectating = Spectating && Multi_match_info.match_state == MGMS_GAMEPLAY;
    const bool no_switching = Camera_no_switching_timestamp.valid();
    if (Camera_no_switching_timestamp.valid() && Camera_no_switching_timestamp.elapsed())
    {
        Camera_no_switching_timestamp.invalidate();
    }
    if (transitioning || no_switching || spectator_controls_blocked())
    {
        return;
    }
    multi_object_marker* marker = camera_marker_from_handle(Rfg_camera.target_handle);
    player* target = camera_player_from_handle(Rfg_camera.target_handle);
    if (spectating)
    {
        const bool next = controls_process_button_just_pressed(CBA_SPECTATOR_NEXT_PLAYER, Player_controller_index);
        if (next || controls_process_button_just_pressed(CBA_SPECTATOR_PREV_PLAYER, Player_controller_index))
        {
            const unsigned int handle = get_subsequent_player(target != nullptr ? target->get_handle() : Manual_player_marker_handle, next);
            if (handle == 0xFFFFFFFF)
            {
                return;
            }
            Pending_target = handle;
            Manual_player_marker_handle = handle;
            audiolib_cue_play(Ga_cue_ids.multiplayer_camera_switch, nullptr, nullptr);
            Current_offset = MCO_HIGH_RIGHT;
            return;
        }
    }
    bool next = controls_process_button_just_pressed(CBA_SPECTATOR_NEXT_TARGET, Player_controller_index) || wrecking_crew_check_mouse_clicks() == WC_MCR_HINT_NEXT_CAM;
    if (!next && !controls_process_button_just_pressed(CBA_SPECTATOR_PREV_TARGET, Player_controller_index) && wrecking_crew_check_mouse_clicks() != WC_MCR_HINT_PREV_CAM)
    {
        return;
    }
    const unsigned int handle = get_subsequent_manual_marker(marker != nullptr ? marker->get_handle() : Manual_camera_marker_handle, next);
    if (handle != 0xFFFFFFFF)
    {
        Pending_target = handle;
        Manual_camera_marker_handle = handle;
        audiolib_cue_play(Ga_cue_ids.multiplayer_camera_switch, nullptr, nullptr);
    }
}

// 0x009AD950 (:1676): the next target button, the change time, or 1.5 s without a visible player.
static bool aquarium_mode_should_switch_camera()
{
    if (controls_process_button_just_pressed(CBA_SPECTATOR_NEXT_TARGET, Player_controller_index) && !spectator_controls_blocked())
    {
        Aquarium_force_new_target = true;
        return true;
    }
    if (Camera_change_timestamp.valid() && !Camera_change_timestamp.elapsed())
    {
        int step = (int)(Frametime * 1000.0f);
        if (any_player_visible_by_camera())
        {
            step = -step;
        }
        Player_not_visible_time_ms += step;
        if (Player_not_visible_time_ms < 1)
        {
            Player_not_visible_time_ms = 0;
        }
        // Magic number: 1.5 s.
        return Player_not_visible_time_ms > 1500;
    }
    return true;
}

// 0x009AD9F0 (:1696)
// Aquarium: on a switch, up to 16 markers (the current one first unless forced, the rest at random) are scored with a
// random fov of the three, and the best one becomes the target with its fov.
static void process_mode_aquarium()
{
    if (!aquarium_mode_should_switch_camera())
    {
        return;
    }
    Camera_change_timestamp.set(int_rand_range(Mp_camera_aquarium_min_time_ms, Mp_camera_aquarium_max_time_ms));
    const bool forced = Aquarium_force_new_target;
    Aquarium_force_new_target = false;
    Player_not_visible_time_ms = 0;
    farray<const multi_object_marker*, MAX_MARKERS> markers;
    collect_camera_markers(markers, true);
    const object* candidates[16];
    int num_candidates = 0;
    multi_object_marker* current = camera_marker_from_handle(Rfg_camera.target_handle);
    if (current != nullptr)
    {
        for (int i = 0; i < markers.size();)
        {
            if (markers[i] == current)
            {
                markers.remove(i);
            }
            else
            {
                i++;
            }
        }
        if (!forced)
        {
            candidates[num_candidates++] = current;
        }
    }
    // Magic number: 16 candidates at most.
    while (markers.size() >= 1 && num_candidates < 16)
    {
        const unsigned int pick = vrand() % (unsigned int)markers.size();
        candidates[num_candidates++] = markers[pick];
        markers.remove(pick);
    }
    if (num_candidates <= 0)
    {
        return;
    }
    int best = 0;
    int best_score = -1;
    unsigned int best_fov = 0;
    for (int i = 0; i < num_candidates; i++)
    {
        const unsigned int fov_index = vrand() % 3;
        aquarium_score_info info((const multi_object_marker*)candidates[i], true, false, nullptr);
        // Magic number: the culling fov is 90% of the view's.
        info.fov = Aquarium_view_fov_multipliers[fov_index] * MP_CAMERA_DEFAULT_FOV * 0.9f;
        const int score = score_aquarium_marker(info);
        if (best_score < score)
        {
            best = i;
            best_score = score;
            best_fov = fov_index;
        }
    }
    Pending_target = candidates[best]->get_handle();
    Aquarium_fov = Aquarium_view_fov_multipliers[best_fov] * MP_CAMERA_DEFAULT_FOV;
}

// 0x00950820 (:1768): by Action_camera_mode_chances.
static action_camera_modes choose_next_action_camera_mode()
{
    const float r = fl_rand();
    float total = 0.0f;
    for (int mode = ACM_PLAYER_TRACKING; mode < NUM_ACTION_CAMERA_MODES; mode++)
    {
        total = Action_camera_mode_chances[mode] + total;
        if (r < total)
        {
            return (action_camera_modes)mode;
        }
    }
    return ACM_PLAYER_TRACKING;
}

// 0x00950880 (:1784)
static void update_action_mode_switch(action_camera_switch_reason switch_reason)
{
    AC_switch_reason = switch_reason;
    Last_ac_mode = Action_camera_mode;
}

// 0x009ADCA0 (:1790)
// Action: switch at the start, on the next target button, at the change time (automatic only), after 0.8 s without the
// tracked players in view, or 1.2 s of them dead.
static bool action_mode_should_switch_camera()
{
    const int step = (int)(Frametime * 1000.0f);
    if (Current_view_mode == MCVM_DUMB)
    {
        update_action_mode_switch(ACSR_INIT); // inlined
        return true;
    }
    if (controls_process_button_just_pressed(CBA_SPECTATOR_NEXT_TARGET, Player_controller_index) && !spectator_controls_blocked())
    {
        update_action_mode_switch(ACSR_BUTTON_PRESS); // inlined
        Action_force_new_target = true;
        return true;
    }
    if (Action_camera_submode == ACSM_MANUAL)
    {
        return false;
    }
    if (!Camera_change_timestamp.valid() || Camera_change_timestamp.elapsed())
    {
        update_action_mode_switch(ACSR_TIME); // inlined
        return true;
    }
    const object* objp = get_point_of_interest_object(0);
    const player* tracked = objp != nullptr && objp->get_obj_type() == OT_HUMAN && objp->get_sub_type() == OT_SUB_HUMAN_PLAYER ? (const player*)objp : nullptr;
    if (Current_view_mode == MCVM_TARGET_POV || Current_view_mode == MCVM_TARGET_REVERSE_POV || Current_view_mode == MCVM_TARGET_TRACKING)
    {
        player* target = World.object_from_handle<player>(Rfg_camera.target_handle);
        if (target != nullptr && human_is_dead(target))
        {
            Player_dead_time_ms += step;
        }
        // Original: the tracked player is not checked here.
        if (Current_view_mode == MCVM_TARGET_TRACKING && human_is_dead(tracked))
        {
            Player_dead_time_ms += step;
        }
    }
    if (Current_view_mode == MCVM_STATIC_TRACKING || Current_view_mode == MCVM_STATIC_PAN)
    {
        const bool visible = is_player_visible_by_camera(tracked);
        bool second_visible = false;
        if (Action_camera_mode == ACM_NEAR_TRACKING && Num_camera_points_of_interest == 2)
        {
            player* second = World.object_cast<player>((object*)get_point_of_interest_object(1));
            if (second != nullptr)
            {
                second_visible = is_player_visible_by_camera(second);
                if (human_is_dead(second))
                {
                    Player_dead_time_ms += step;
                }
            }
        }
        Player_not_visible_time_ms += visible || second_visible ? -step : step;
        if (Player_not_visible_time_ms < 1)
        {
            Player_not_visible_time_ms = 0;
        }
        // Original: the tracked player is not checked here either.
        if (human_is_dead(tracked))
        {
            Player_dead_time_ms += step;
        }
    }
    // Magic numbers: 0.8 s not visible, 1.2 s dead.
    if (Player_not_visible_time_ms > 800)
    {
        update_action_mode_switch(ACSR_DEAD_OR_NOT_VISIBLE); // inlined
        return true;
    }
    if (Player_dead_time_ms > 1200)
    {
        update_action_mode_switch(ACSR_TRACKED_PLAYER_DIED); // inlined
        return true;
    }
    return false;
}

// 0x009ADF00 (:1875)
// Action: the menu button toggles the manual submode; on a switch the best player (a forced switch excludes the current
// one and its marker) gets a view of a random mode.
static void process_mode_action()
{
    if (controls_process_button_just_pressed(CBA_MENU_X, Player_controller_index) && !spectator_controls_blocked())
    {
        Action_camera_submode = Action_camera_submode == ACSM_MANUAL ? ACSM_AUTOMATIC : ACSM_MANUAL;
    }
    if (!action_mode_should_switch_camera())
    {
        process_view_mode();
        return;
    }
    Camera_change_timestamp.set(int_rand_range(Mp_camera_action_min_time_ms, Mp_camera_action_max_time_ms));
    const bool forced = Action_force_new_target;
    Player_not_visible_time_ms = 0;
    Player_dead_time_ms = 0;
    Action_force_new_target = false;
    unsigned int exclude_player = 0xFFFFFFFF;
    unsigned int exclude_marker = 0xFFFFFFFF;
    if (forced)
    {
        switch (Current_view_mode)
        {
        case MCVM_STATIC_TRACKING:
        case MCVM_STATIC_PAN:
        case MCVM_TARGET_TRACKING:
            exclude_player = Camera_points_of_interest[0].point.object_handle;
            exclude_marker = Pending_target;
            break;
        case MCVM_TARGET_POV:
        case MCVM_TARGET_REVERSE_POV:
            exclude_player = Pending_target;
            break;
        default:
            break;
        }
    }
    const unsigned int best_player = action_find_best_player(exclude_player); // inlined
    Action_camera_mode = choose_next_action_camera_mode(); // inlined
    set_action_view_mode(Action_camera_mode, best_player, exclude_marker);
}

// 0x00996E40 (:1927)
// Player: the menu button cycles manual / cycle / killer. Cycle moves to the next player at its change time (or when
// the target dies); killer follows the kills (multi_game_camera_player_killed); manual steps by button (outside a
// transition or switching hold-off), starting from the first player.
static void process_mode_player()
{
    if (controls_process_button_just_pressed(CBA_MENU_X, Player_controller_index) && !spectator_controls_blocked())
    {
        Current_player_cam_mode = (player_camera_modes)((Current_player_cam_mode + 1) % NUM_PLAYER_CAMERA_MODES);
        Current_view_mode = MCVM_MANUAL;
        Camera_change_timestamp.invalidate();
    }
    player* target = camera_player_from_handle(Rfg_camera.target_handle);
    if (Current_player_cam_mode != PCM_MANUAL)
    {
        if (Current_player_cam_mode == PCM_CYCLE)
        {
            if (!Camera_change_timestamp.valid() || Camera_change_timestamp.elapsed() || target == nullptr || human_is_dead(target))
            {
                Camera_change_timestamp.set(int_rand_range(Mp_camera_player_min_time_ms, Mp_camera_player_max_time_ms));
                target = get_next_player(target);
                Pending_target = target->get_handle();
                Num_camera_points_of_interest = 0;
            }
        }
        else if (Current_player_cam_mode != PCM_KILLER)
        {
            return;
        }
        Current_view_mode = MCVM_TARGET_POV;
        process_view_mode();
        return;
    }
    const bool transitioning = Camera_transition_timestamp.valid() && !Camera_stay_dead_timestamp.valid();
    const bool no_switching = Camera_no_switching_timestamp.valid();
    if (Camera_no_switching_timestamp.valid() && Camera_no_switching_timestamp.elapsed())
    {
        Camera_no_switching_timestamp.invalidate();
    }
    Current_view_mode = MCVM_TARGET_POV;
    if (!transitioning && !no_switching)
    {
        // Original: the first player is read without a check for an empty list.
        if (target == nullptr && World.get_object<player>(0) != nullptr)
        {
            Pending_target = World.get_object<player>(0)->get_handle();
            audiolib_cue_play(Ga_cue_ids.multiplayer_camera_switch, nullptr, nullptr);
            Current_offset = MCO_HIGH_RIGHT;
        }
        if (!ignore_spectator_controls())
        {
            player* next = nullptr;
            if (controls_process_button_just_pressed(CBA_SPECTATOR_NEXT_PLAYER, Player_controller_index))
            {
                next = get_next_player(target);
            }
            else if (controls_process_button_just_pressed(CBA_SPECTATOR_PREV_PLAYER, Player_controller_index))
            {
                next = get_prev_player(target);
            }
            if (next != nullptr)
            {
                Pending_target = next->get_handle();
                audiolib_cue_play(Ga_cue_ids.multiplayer_camera_switch, nullptr, nullptr);
                Current_offset = MCO_HIGH_RIGHT;
            }
        }
    }
    process_view_mode();
}

// 0x009508A0 (:2028)
static void kill_camera_transition()
{
    Camera_transition_timestamp.invalidate();
    Camera_stay_dead_timestamp.invalidate();
    Pending_target = Rfg_camera.target_handle;
    camera_render_effects_off(); // inlined
}

// 0x0097A290 (:2042)
// A running transition fades the blur and noise in towards its end and, when its time is up with a new target pending,
// moves the camera there (a marker: its place and orient; a player: the free camera's target; nothing: no target) and
// fades them out again over Mp_camera_blur_time_ms; with no change pending it ends. False when no transition runs.
static bool process_camera_transition()
{
    const bool changing = !(Pending_target == Rfg_camera.target_handle &&
                            (Pending_target != 0xFFFFFFFF || (Pending_pos.x == Rfg_camera.ideal_pos.x && Pending_pos.y == Rfg_camera.ideal_pos.y && Pending_pos.z == Rfg_camera.ideal_pos.z)));
    if (!Camera_transition_timestamp.valid())
    {
        return false;
    }
    camera_shake_stop();
    float pct = 0.0f;
    if (!Camera_transition_timestamp.elapsed())
    {
        pct = Camera_transition_timestamp.percent();
        if (!changing)
        {
            pct = 1.0f - pct;
        }
        if (1.0f <= pct)
        {
            return true;
        }
    }
    else
    {
        if (!changing)
        {
            kill_camera_transition();
            return true;
        }
        float fov = MP_CAMERA_DEFAULT_FOV;
        multi_object_marker* marker = camera_marker_from_handle(Pending_target);
        player* pp = camera_player_from_handle(Pending_target);
        if (marker != nullptr)
        {
            Rfg_camera.target_handle = Pending_target;
            camera_trans_abort();
            camera_set_pos(marker->pos);
            camera_set_orient(marker->orient);
            fov = process_camera_fov(Rfg_camera.ideal_pos, marker->orient.fvec);
        }
        else if (pp != nullptr)
        {
            camera_free_set_target(pp);
            fov = process_camera_fov(Rfg_camera.ideal_pos, pp->orient.fvec);
        }
        else
        {
            Rfg_camera.target_handle = Pending_target;
        }
        Rfg_camera.ideal_fov = fov;
        Camera_transition_timestamp.set(Mp_camera_blur_time_ms);
        pct = 0.0f;
    }
    float amount = pct * -1.0f + 1.0f;
    if (amount - 0.0f < 0.0f)
    {
        amount = 0.0f;
    }
    if (0.0f <= amount - 1.0f)
    {
        amount = 1.0f;
    }
    if (!game_is_paused())
    {
        game_render_set_camera_frame_override_blur(amount);
        game_render_set_camera_signal_noise_frequency(Spectating ? 0.0f : Mp_camera_noise_frequency * amount);
        game_render_set_camera_signal_noise_scale(Spectating ? 0.0f : Mp_camera_noise_scale * amount);
    }
    return true;
}

// 0x009508F0 (:2104)
void multi_game_camera_shutdown()
{
    kill_camera_transition(); // inlined
    Current_mode_camera = MCM_NONE;
    Players_killer = false;
}

// 0x00950950 (:2113)
void multi_game_camera_init_post()
{
    Pending_target = 0xFFFFFFFF;
    Show_spectator_hud = true;
}

// 0x009970A0 (:2123)
// The spectator camera starts in manual mode: unless it is on a marker already, the dumb camera goes to the level's
// starting camera marker and the controls go (INT_MODE_NO_CONTROL); the transition starts.
void multi_game_camera_start()
{
    multi_object_marker* start;
    if (camera_marker_from_handle(Pending_target) == nullptr && (start = multi_game_get_camera_starting_position()) != nullptr)
    {
        camera_start_dumb_mode();
        camera_trans_abort();
        camera_set_pos(start->pos);
        camera_set_orient(start->orient);
        Rfg_camera.target_handle = start->get_handle();
        interface_push_mode(INT_MODE_NO_CONTROL);
    }
    kill_camera_transition(); // inlined
    camera_shake_stop();
    Camera_transition_timestamp.set(Mp_camera_blur_time_ms);
    Current_mode_camera = MCM_MANUAL;
    Current_view_mode = MCVM_MANUAL;
    Pending_target = Rfg_camera.target_handle;
    Players_killer = false;
}

// 0x0097A580 (:2154): back to the local player (out of the dumb camera when on a marker), with the controls.
void multi_game_camera_stop()
{
    camera_free_set_target(player_get_local_player());
    if (camera_marker_from_handle(Pending_target) != nullptr)
    {
        camera_stop_dumb_mode();
        kill_camera_transition(); // inlined
    }
    if (interface_get_mode() == INT_MODE_NO_CONTROL)
    {
        interface_pop_mode();
    }
    Pending_target = Rfg_camera.target_handle;
    Players_killer = false;
    Current_mode_camera = MCM_NONE;
    Current_view_mode = MCVM_DUMB;
}

// 0x00950970 (:2176)
static void quick_transition_fov()
{
    if (Current_mode_camera == MCM_AQUARIUM)
    {
        Rfg_camera.real_fov = Aquarium_fov;
        Rfg_camera.ideal_fov = Aquarium_fov;
    }
}

// 0x009AE230 (:2184)
// Every frame: the transition; then, while the camera runs, the death camera (not spectating) or the spectator's mode
// selection (taking the controls away outside manual during a round), the mode's processing and the HUD button. A new
// target cuts to it (a marker: the dumb camera there; a player: third person; nothing: Pending_pos) with full blur and
// noise, and the transition starts.
void multi_game_camera_process()
{
    process_camera_transition();
    if (Current_mode_camera == MCM_NONE)
    {
        return;
    }
    if (!Spectating)
    {
        process_death_cam();
    }
    else
    {
        if (Multi_match_info.match_state == MGMS_GAMEPLAY)
        {
            if (Current_view_mode == MCVM_MANUAL)
            {
                if (interface_get_mode() == INT_MODE_NO_CONTROL)
                {
                    interface_pop_mode();
                }
            }
            else if (interface_get_mode() != INT_MODE_NO_CONTROL)
            {
                interface_push_mode(INT_MODE_NO_CONTROL);
            }
        }
        process_mode_selection();
    }
    switch (Current_mode_camera)
    {
    case MCM_MANUAL:
        process_mode_manual();
        break;
    case MCM_ACTION:
        process_mode_action();
        break;
    case MCM_PLAYER:
        process_mode_player();
        break;
    case MCM_AQUARIUM:
        process_mode_aquarium();
        break;
    default:
        break;
    }
    if (Spectating && controls_process_button_just_pressed(CBA_SPECTATOR_TOGGLE_HUD, Player_controller_index) && !spectator_controls_blocked())
    {
        multi_game_camera_show_spectator_hud(!Show_spectator_hud);
    }
    if (Pending_target == Rfg_camera.target_handle &&
        (Pending_target != 0xFFFFFFFF || (Pending_pos.x == Rfg_camera.ideal_pos.x && Pending_pos.y == Rfg_camera.ideal_pos.y && Pending_pos.z == Rfg_camera.ideal_pos.z)))
    {
        return;
    }
    if (camera_player_from_handle(Rfg_camera.target_handle) != nullptr)
    {
        Players_killer = false;
        fb_color_correct_modulate_saturation(1.0f);
    }
    if (Mp_camera_quick_transition)
    {
        object* objp = World.object_from_handle(Pending_target);
        if (objp != nullptr && objp->get_obj_type() == OT_MULTI_MARKER)
        {
            if (camera_get_mode() != CAMERA_DUMB_MODE)
            {
                camera_start_dumb_mode();
            }
            Rfg_camera.target_handle = Pending_target;
            camera_trans_abort();
            camera_set_pos(objp->pos);
            camera_set_orient(objp->orient);
            quick_transition_fov(); // inlined
        }
        else if (objp != nullptr && objp->get_obj_type() == OT_HUMAN && objp->get_sub_type() == OT_SUB_HUMAN_PLAYER)
        {
            camera_free_set_target(objp);
            if (camera_get_mode() != CAMERA_THIRD_PERSON_MODE)
            {
                camera_start_third_person(false);
            }
        }
        else
        {
            Rfg_camera.target_handle = Pending_target;
            camera_trans_abort();
            camera_set_pos(Pending_pos);
            camera_set_orient(Pending_orient);
        }
        if (!game_is_paused())
        {
            game_render_set_camera_frame_override_blur(1.0f);
            game_render_set_camera_signal_noise_frequency(Mp_camera_noise_frequency);
            game_render_set_camera_signal_noise_scale(Mp_camera_noise_scale);
        }
    }
    Camera_transition_timestamp.set(Mp_camera_blur_time_ms);
}

// 0x009509A0 (:2270)
multi_camera_mode multi_game_camera_get_mode()
{
    return Current_mode_camera;
}

// 0x009509B0 (:2275)
bool multi_game_camera_following_killer()
{
    return Players_killer;
}

// 0x00997190 (:2280)
// A local victim's camera stays on the death for Mp_camera_stay_dead_time_ms (no switching for
// Mp_camera_no_switching_time_ms): on a remote killer (followed) or, without one, on itself (the dumb camera); manual
// mode. A camera following a killer that is killed moves to the victim's view. The killer player mode follows the kill
// (to the next player for a suicide).
void multi_game_camera_player_killed(player* victim, player* killer)
{
    if (victim != nullptr)
    {
        if (object_is_local_player(victim))
        {
            Camera_stay_dead_timestamp.set(Mp_camera_stay_dead_time_ms);
            Camera_no_switching_timestamp.set(Mp_camera_no_switching_time_ms);
            if (killer == nullptr)
            {
                camera_start_dumb_mode();
                Pending_target = victim->get_handle();
            }
            else if (!object_is_local_player(killer))
            {
                camera_free_set_target(killer);
                Pending_target = killer->get_handle();
                Players_killer = true;
            }
            Current_offset = MCO_DEATH;
            Current_mode_camera = MCM_MANUAL;
            return;
        }
        if (Players_killer && victim->get_handle() == Rfg_camera.target_handle && killer != nullptr)
        {
            camera_free_set_target(victim);
            Pending_target = victim->get_handle();
        }
    }
    // Original: victim is read here without a check.
    if (Current_mode_camera == MCM_PLAYER && Current_player_cam_mode == PCM_KILLER && Pending_target == victim->get_handle())
    {
        if (victim == killer || killer == nullptr)
        {
            killer = get_next_player(victim);
        }
        if (killer != nullptr)
        {
            camera_free_set_target(killer);
            Pending_target = killer->get_handle();
        }
    }
}

// 0x009AE520 (:2318)
// A big explosion (a human damage radius of 2.5 m or more) becomes a point of interest for the tracking views when it
// is near the look-at point (the static views) or the camera (target tracking), or in plain view of the camera.
void multi_game_camera_explosion_occured(const vector& position, float human_damage_radius)
{
    if (human_damage_radius < 2.5f)
    {
        return;
    }
    const float max_distance = Mp_camera_action_nearby_explosion_max_distance_m * Mp_server_options.get_level()->distance_multiplier;
    raycast_input in;
    in.radius = 0.0f;
    in.start = Rfg_camera.real_pos;
    in.end = position;
    in.filter_params = nullptr;
    in.filter_callback = nullptr;
    in.m_hit_collector_type = raycast_input::COLLECT_CLOSEST_HIT;
    in.m_flags = 0;
    in.m_filter_info = COLLIDE_RAY_DEFAULT;
    if (Current_view_mode == MCVM_STATIC_TRACKING || Current_view_mode == MCVM_LAGGING_TRACKING)
    {
        vector lookat = ZERO_VECTOR;
        get_interest_lookat(lookat);
        raycast_output out;
        if (vec_dist_squared(lookat, position) < max_distance * max_distance || !havok_raycast(in, out))
        {
            add_interesting_explosion(position);
        }
    }
    else if (Current_view_mode == MCVM_TARGET_TRACKING)
    {
        raycast_output out;
        if (vec_dist_squared(Rfg_camera.real_pos, position) < max_distance * max_distance || !havok_raycast(in, out))
        {
            add_interesting_explosion(position);
        }
    }
}

// 0x009AE730 (:2370)
// A respawn: the local player's camera stops (with a transition); a spectated player goes back to third person. The
// tracking views take a respawn near the camera (within 25 m) and in plain view as a point of interest.
void multi_game_camera_player_respawn(player* pp)
{
    player* target = camera_player_from_handle(Rfg_camera.target_handle);
    if (object_is_local_player(pp))
    {
        multi_game_camera_stop();
        Camera_transition_timestamp.set(Mp_camera_blur_time_ms);
    }
    else if (target != nullptr && pp == target)
    {
        camera_start_third_person(false);
    }
    const float max_distance = Mp_camera_action_nearby_enemy_max_distance_m;
    if (Current_view_mode == MCVM_STATIC_TRACKING || Current_view_mode == MCVM_LAGGING_TRACKING || Current_view_mode == MCVM_TARGET_TRACKING)
    {
        raycast_input in;
        in.radius = 0.0f;
        in.m_filter_info = COLLIDE_RAY_DEFAULT;
        in.start = Rfg_camera.real_pos;
        in.end = pp->pos;
        in.filter_params = nullptr;
        in.filter_callback = nullptr;
        in.m_hit_collector_type = raycast_input::COLLECT_CLOSEST_HIT;
        in.m_flags = 0;
        raycast_output out;
        if (vec_dist_squared(Rfg_camera.real_pos, pp->pos) < max_distance * max_distance && !havok_raycast(in, out))
        {
            add_interesting_player(pp->get_handle());
        }
    }
}

// 0x009509C0 (:2412)
bool multi_game_camera_is_running()
{
    return Current_mode_camera != MCM_NONE;
}

// 0x009509D0 (:2417)
bool multi_game_camera_is_spectator_hud_visible()
{
    return Show_spectator_hud && Multi_match_info.match_state == MGMS_GAMEPLAY;
}

// 0x009509F0 (:2422): the chat moves out of the HUD's way while it shows.
void multi_game_camera_show_spectator_hud(bool show)
{
    console_change_chat_window_pos(show ? VCP_ENDGAME : VCP_INGAME);
    Show_spectator_hud = show;
    hud_hide(!show);
}

// 0x00950A20 (:2433)
void multi_game_camera_level_init_pre()
{
    Manual_camera_marker_handle = 0xFFFFFFFF;
    Manual_player_marker_handle = 0xFFFFFFFF;
}

// 0x0097A640 (:2439)
void multi_game_camera_get_control_hints(char* dest, unsigned int dest_size)
{
    wchar_t hints[512];
    multi_game_camera_get_control_hints(hints, 512);
    vint_encode_wide_text(dest, dest_size, hints);
}

// The display text of a tag, the tag itself when it has none (inlined).
static void camera_display_text(wchar_t* buf, const char* tag)
{
    const wchar_t* text = localization_get_display_string_opt(tag);
    if (text == nullptr)
    {
        // Magic number: 256 characters.
        swprintf(buf, 256, L"%hs", tag);
    }
    else
    {
        swprintf(buf, 256, L"%ls", text);
    }
}

// 0x00950A30 (:2446)
// The button hints of the mode: outside spectating the camera switch only; a spectator's mode's buttons (players,
// markers, the menu button's cycle) and the HUD button.
void multi_game_camera_get_control_hints(wchar_t* dest, unsigned int dest_size)
{
    wchar_t hide_hud[256];
    wchar_t switch_player[256];
    wchar_t cycle_mode[256];
    wchar_t switch_camera[256];
    if (!Spectating || Multi_match_info.match_state != MGMS_GAMEPLAY)
    {
        camera_display_text(switch_player, "MENU_SWITCH_CAMERA");
        swprintf(dest, dest_size, L"%hs %hs  %ls", "[button:spec_prev]", "[button:spec_next]", switch_player);
        return;
    }
    camera_display_text(hide_hud, "MENU_HIDE_HUD");
    camera_display_text(switch_player, "MENU_SWITCH_PLAYER");
    camera_display_text(switch_camera, "MENU_SWITCH_CAMERA");
    camera_display_text(cycle_mode, "MENU_CYCLE_PLAYER_CAMERA_MODE");
    switch (Current_mode_camera)
    {
    case MCM_MANUAL:
        swprintf(dest, dest_size, L"%hs  %hs %ls   %hs  %hs %ls   %hs %ls", "[button:spec_prev_play]", "[button:spec_next_play]", switch_player, "[button:spec_prev]",
                 "[button:spec_next]", switch_camera, "[button:spec_hud]", hide_hud);
        return;
    case MCM_ACTION:
        swprintf(dest, dest_size, L"%hs %ls   %hs %ls   %hs %ls", "[button:spec_next]", switch_camera, "[button:menu_x]", cycle_mode, "[button:spec_hud]", hide_hud);
        return;
    case MCM_PLAYER:
        if (Current_player_cam_mode == PCM_MANUAL)
        {
            swprintf(dest, dest_size, L"%hs  %hs %ls   %hs %ls   %hs %ls", "[button:spec_prev_play]", "[button:spec_next_play]", switch_player, "[button:menu_x]", cycle_mode,
                     "[button:spec_hud]", hide_hud);
            return;
        }
        if (Current_player_cam_mode > PCM_MANUAL && Current_player_cam_mode < NUM_PLAYER_CAMERA_MODES)
        {
            swprintf(dest, dest_size, L"%hs %ls   %hs %ls", "[button:menu_x]", cycle_mode, "[button:spec_hud]", hide_hud);
            return;
        }
        break;
    case MCM_AQUARIUM:
        swprintf(dest, dest_size, L"%hs %ls   %hs %ls", "[button:spec_next]", switch_camera, "[button:spec_hud]", hide_hud);
        return;
    default:
        break;
    }
    swprintf(dest, dest_size, L"%hs %ls", "[button:spec_hud]", hide_hud);
}

// 0x00950DB0 (:2502): a tag's text, "!!" and the tag when it has none.
// LTCG: unicode_buf in EDI, raw_text in ESI, unicode_buf_size on the stack (the port has no register convention).
static void get_localized_text(wchar_t* unicode_buf, int unicode_buf_size, const char* raw_text)
{
    const wchar_t* text;
    wchar_t wide_tag[256];
    wchar_t missing[256];
    if (localization_does_tag_exist(raw_text))
    {
        text = localization_get_display_string_req(raw_text, nullptr);
    }
    else
    {
        copy_to_wide(wide_tag, raw_text, 256);
        _snprintf_safe(missing, 256, L"!!%ls", wide_tag);
        text = missing;
    }
    _snprintf_safe(unicode_buf, (unsigned int)unicode_buf_size, L"%ls", text);
}

// 0x00950E40 (:2518): the mode's name (action and player with their submode).
const wchar_t* multi_game_camera_get_camera_mode_name()
{
    // Magic number: 128 characters.
    const int size = 128;
    wchar_t first[128];
    wchar_t second[128];
    switch (Current_mode_camera)
    {
    case MCM_NONE:
        get_localized_text(mode_name, size, "MENU_SPECTATE_NONE");
        return mode_name;
    case MCM_MANUAL:
        get_localized_text(mode_name, size, "MENU_SPECTATE_MANUAL");
        return mode_name;
    case MCM_ACTION:
        get_localized_text(first, size, "MENU_SPECTATE_ACTION");
        get_localized_text(second, size, Action_camera_submode != ACSM_AUTOMATIC ? "MENU_SPECTATE_MANUAL" : "MENU_SPECTATE_AUTO");
        _snprintf_safe(mode_name, size, L"%ls - %ls ", first, second);
        return mode_name;
    case MCM_PLAYER:
    {
        get_localized_text(first, size, "MENU_PLAYER");
        const char* submode;
        switch (Current_player_cam_mode)
        {
        case PCM_MANUAL:
            submode = "MENU_SPECTATE_MANUAL";
            break;
        case PCM_CYCLE:
            submode = "MENU_SPECTATE_AUTO";
            break;
        case PCM_KILLER:
            submode = "MENU_SPECTATE_PLAYER_KILLER";
            break;
        default:
            submode = "";
            break;
        }
        get_localized_text(second, size, submode);
        _snprintf_safe(mode_name, size, L"%ls - %ls ", first, second);
        return mode_name;
    }
    case MCM_AQUARIUM:
        get_localized_text(mode_name, size, "MENU_SPECTATE_AQUARIUM");
        return mode_name;
    default:
        return L"";
    }
}

// 0x00950FC0 (:2576): the third person offset of the current set by stance (closer while fine aiming).
const vector* multi_game_camera_get_third_person_offset(const player* target_pp)
{
    if (target_pp != nullptr)
    {
        if (player_is_fine_aimed(target_pp))
        {
            return human_move_is_crouching(target_pp) ? &Camera_offsets[Current_offset].crouch_close : &Camera_offsets[Current_offset].normal_close;
        }
        if (human_move_is_crouching(target_pp))
        {
            return &Camera_offsets[Current_offset].crouch;
        }
    }
    return &Camera_offsets[Current_offset].normal;
}
