// Original: volition/rfg/code/camera/camera_shake.cpp
//
// The camera shake table (camera_shake.xtbl, player_minimal.md P3), the lookup by name, starting shakes and their
// per-frame processing into the camera (P6). Compiled in rfg_camera_unity.cpp. The float expressions follow the
// decompile; their double rounding was not checked against the asm (the shakes only turn the camera; the number of
// fl_rand calls, which the global generator's sequence depends on, does not depend on the rounding).

#include "volition/rfg/code/camera/camera_shake.h"

#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/rfg/code/control/control_map.h"
#include "volition/rfg/code/game_render/fullscreen_effects/fb_accum_blur.h"
#include "volition/rfg/code/gameplay/wrecking_crew/wrecking_crew.h"
#include "volition/rfg/code/gamewide/game_clock.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/multi/multi_core/include/multi.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_platform.h"
#include "volition/rfg/code/multi/multi_game/multi_game.h"
#include "volition/rfg/code/multi/multi_game/multi_game_camera.h"
#include "volition/rfg/code/rfg.h"
#include "volition/vlib/code/input/input.h"
#include "volition/vlib/code/math/rand.h"
#include "volition/vlib/code/os/frametime.h"
#include "volition/vlib/code/memory/object_pool.h"
#include "volition/vlib/code/memory/rfg_split_mempool.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/vlib/code/time/timestamp.h"
#include "volition/vlib/code/util/hash_table.h"

#include "keen/base/trace.hpp"

#include <math.h>
#include <string.h>

// ---- TU-private types (rfg.pdb; no header names them)
// The random walk state of one shake channel (jitter, wander, destable, wobble, oscillate).
// size 0x1C
struct camera_shake_type_info
{
    camera_shake_type_info(); // 0x006C6DA0

    float random_pitch; // 0x00
    float random_bank; // 0x04
    float random_heading; // 0x08
    float last_random_pitch; // 0x0C
    float last_random_bank; // 0x10
    float last_random_heading; // 0x14
    timestamp timer; // 0x18
};

// 0x006C6DA0: the timer starts invalid (its constructor); the walk values are set by camera_shake_process (P6).
camera_shake_type_info::camera_shake_type_info()
{
}

// 0x006C7020
camera_shake::camera_shake()
{
}

// ---- file-static globals
bool Camera_shake_enabled = true; // 0x01648859 (global)
static float Camera_shake_decay_rate = 1.25f; // 0x016488C8
static float Camera_shake_energy_scaler = 5.0f; // 0x016488CC
static float Camera_shake_minimum_distance = 350.0f; // 0x016488D0
static float Camera_shake_minimum_intensity = 0.2f; // 0x016488D4
static dynamic_object_pool<camera_shake,1> Camera_shake_pool; // 0x01648998
static camera_shake_type_info Camera_shake_jitter; // 0x016489D8
static camera_shake_type_info Camera_shake_wander; // 0x016489F4
static camera_shake_type_info Camera_shake_destable; // 0x01648A10
static camera_shake_type_info Camera_shake_wobble; // 0x01648A2C
static camera_shake_type_info Camera_shake_oscillate1; // 0x01648A48
static camera_shake_type_info Camera_shake_oscillate2; // 0x01648A64
static float Camera_shake_time[5]; // 0x01DE4520
static const camera_shake* Cur_camera_shake[5]; // 0x01DE4554
// camera_view_table_entry Camera_view_table[10] (0x01DE4568) is used here too; placed in camera_view.cpp.
static float Camera_shake_intensity[5]; // 0x01DE4888
static float Shake_intensity; // 0x01DE48A4
static str_data_hash_table<camera_shake*,128,32,unsigned char> Camera_shake_name_hash; // 0x01DE5580
// ---- functions
// 0x006E0640 (:218)
// camera_shake.xtbl: one pooled camera_shake per entry (at most 128), found by name through Camera_shake_name_hash.
// A refresh frees the shakes and parses them again, and may not add entries.
static void camera_shake_parse_table(bool refresh)
{
    xml_element* table = xml_table_parse("camera_shake.xtbl");
    const unsigned int count = (unsigned int)xml_table_count(table, "Camera_Shake");
    bool ok = true;
    if (refresh)
    {
        if (Camera_shake_pool.get_capacity() - (unsigned int)Camera_shake_pool.get_free() < count)
        {
            keen::debug::nativePrintArgs("You cannot add new camera shake entries with a refresh.");
            ok = false;
        }
    }
    else
    {
        Camera_shake_pool.init(Permanent_mempool, count);
    }
    // Magic number: 128, the size of Camera_shake_name_hash.
    if (count <= 128 && ok)
    {
        Camera_shake_pool.thread_access.enter();
        for (camera_shake* shake = Camera_shake_pool.get_first(0); shake != nullptr; shake = Camera_shake_pool.free(shake))
        {
        }
        Camera_shake_pool.thread_access.leave();
        for (xml_element* entry = xml_table_find_req(table, "Camera_Shake"); entry != nullptr; entry = xml_table_find_next(table, entry, "Camera_Shake"))
        {
            camera_shake* shake = Camera_shake_pool.alloc(0);
            const char* name = xml_table_get_req_string_ref(entry, "Name");
            strncpy(shake->name, name, sizeof(shake->name));
            shake->name[sizeof(shake->name) - 1] = '\0';
            const char* foley = xml_table_get_opt_string_ref(entry, "Foley");
            if (foley == nullptr)
            {
                shake->foley_ref = -1;
            }
            else
            {
                audiolib_cue_get_id(foley, &shake->foley_ref);
            }
            shake->foley_play_id = -1;
            xml_element* channel = xml_table_find_req(entry, "Wobble");
            xml_table_get_req_float(shake->wobble.x, channel, "Pitch");
            xml_table_get_req_float(shake->wobble.y, channel, "Roll");
            xml_table_get_req_float(shake->wobble.z, channel, "Yaw");
            xml_table_get_req_float(shake->wobble_variation, channel, "Variation");
            channel = xml_table_find_req(entry, "Destabilization");
            xml_table_get_req_float(shake->destable.x, channel, "Pitch");
            xml_table_get_req_float(shake->destable.y, channel, "Roll");
            xml_table_get_req_float(shake->destable.z, channel, "Yaw");
            xml_table_get_req_float(shake->destable_frequency, channel, "Frequency");
            channel = xml_table_find_req(entry, "Wander");
            xml_table_get_req_float(shake->wander.x, channel, "Pitch");
            xml_table_get_req_float(shake->wander.z, channel, "Yaw");
            xml_table_get_req_float(shake->wander_frequency, channel, "Frequency");
            channel = xml_table_find_req(entry, "Jitter");
            xml_table_get_req_float(shake->jitter.x, channel, "Pitch");
            xml_table_get_req_float(shake->jitter.y, channel, "Roll");
            xml_table_get_req_float(shake->jitter.z, channel, "Yaw");
            channel = xml_table_find_req(entry, "Oscillation1");
            xml_table_get_req_float(shake->oscillate1.x, channel, "Pitch");
            xml_table_get_req_float(shake->oscillate1.y, channel, "Roll");
            xml_table_get_req_float(shake->oscillate1.z, channel, "Yaw");
            xml_table_get_req_float(shake->oscillate1_frequency, channel, "Frequency");
            channel = xml_table_find_req(entry, "Oscillation2");
            xml_table_get_req_float(shake->oscillate2.x, channel, "Pitch");
            xml_table_get_req_float(shake->oscillate2.y, channel, "Roll");
            xml_table_get_req_float(shake->oscillate2.z, channel, "Yaw");
            xml_table_get_req_float(shake->oscillate2_frequency, channel, "Frequency");
            channel = xml_table_find_req(entry, "Direct");
            xml_table_get_req_float(shake->direct.x, channel, "Pitch");
            xml_table_get_req_float(shake->direct.y, channel, "Roll");
            xml_table_get_req_float(shake->direct.z, channel, "Yaw");
            channel = xml_table_find_req(entry, "Wander_Direct");
            xml_table_get_req_float(shake->wander_direct.x, channel, "Pitch");
            xml_table_get_req_float(shake->wander_direct.z, channel, "Yaw");
            xml_element* graph = xml_table_find_req(entry, "Strength_Graph");
            shake->num_weights = 0;
            // The original does not check the 10 weight slots.
            for (xml_element* element = xml_table_find_opt(graph, "Strength_Element"); element != nullptr; element = xml_table_find_next(graph, element, "Strength_Element"))
            {
                weight_times& weight = shake->weights[shake->num_weights];
                xml_table_get_req_float(weight.time, element, "Time");
                xml_table_get_req_float(weight.wobble, element, "Wobble");
                xml_table_get_req_float(weight.destable, element, "Destable");
                xml_table_get_req_float(weight.wander, element, "Wander");
                xml_table_get_req_float(weight.jitter, element, "Jitter");
                xml_table_get_req_float(weight.oscillate1, element, "Oscillation1");
                xml_table_get_req_float(weight.oscillate2, element, "Oscillation2");
                xml_table_get_req_float(weight.direct, element, "Direct");
                xml_table_get_req_float(weight.wander_direct, element, "Wander_Direct");
                xml_table_get_req_float(weight.blur, element, "Blur");
                xml_table_get_req_float(weight.controller_vibration_strong, element, "Strong_Vibration");
                xml_table_get_req_float(weight.controller_vibration_weak, element, "Weak_Vibration");
                shake->num_weights++;
            }
            // Added unless the name is already there (insert_key_data inlined; a full table drops it).
            camera_shake* existing = nullptr;
            if (!Camera_shake_name_hash.get(name, existing))
            {
                Camera_shake_name_hash.insert_key_data(name, shake);
            }
        }
    }
    xml_table_close(table);
}

// 0x006C6DB0 (:358, LTCG: shake_index in EAX)
static void camera_shake_stop(int shake_index)
{
    if ((unsigned int)shake_index < 5 && Cur_camera_shake[shake_index] != nullptr)
    {
        Cur_camera_shake[shake_index] = nullptr;
    }
}

// 0x006E0BE0 (:373)
void camera_shake_system_init()
{
    Shake_intensity = 0.0f;
    camera_shake_parse_table(false);
    for (int i = 0; i < 5; i++)
    {
        Cur_camera_shake[i] = nullptr;
    }
    Camera_shake_wobble.last_random_pitch = 1.0f;
    Camera_shake_wobble.last_random_bank = 1.0f;
    Camera_shake_wobble.last_random_heading = 1.0f;
}

// 0x006C6DD0 (:386)
void camera_shake_level_init_post()
{
    Shake_intensity = 0.0f;
    for (int i = 0; i < 5; i++)
    {
        Cur_camera_shake[i] = nullptr;
    }
}

// 0x006D4290 (:463)
camera_shake* camera_shake_get(const char* name)
{
    camera_shake* shake = nullptr;
    Camera_shake_name_hash.get(name, shake);
    return shake;
}

// 0x006D42E0 (:471)
bool camera_shake_is_active(const char* name)
{
    camera_shake* shake = nullptr;
    if (!Camera_shake_name_hash.get(name, shake) || shake == nullptr)
    {
        return false;
    }
    for (int i = 0; i < 5; i++)
    {
        if (Cur_camera_shake[i] == shake)
        {
            return true;
        }
    }
    return false;
}

// 0x006C6F60 (:477)
bool camera_shake_is_active(const camera_shake* shake)
{
    if (shake != nullptr)
    {
        for (int i = 0; i < 5; i++)
        {
            if (Cur_camera_shake[i] == shake)
            {
                return true;
            }
        }
    }
    return false;
}

// 0x006C6F90 (:577)
void camera_shake_stop(const camera_shake* shake)
{
    for (int i = 0; i < 5; i++)
    {
        if (Cur_camera_shake[i] == shake && Cur_camera_shake[i] != nullptr)
        {
            Cur_camera_shake[i] = nullptr;
        }
    }
}

// 0x006C7000 (:592)
void camera_shake_stop()
{
    for (int i = 0; i < 5; i++)
    {
        camera_shake_stop(i);
    }
}

// 0x006C7030 (:978)
float camera_shake_get_intensity()
{
    return Shake_intensity;
}

// Whether a shake may start: always while spectating, in wrecking crew and in the free camera, else only in the
// lookaround, first person, turret and third person modes (inlined in each starter in the binary).
static bool camera_shake_mode_allows()
{
    if (multi_game_spectating() || wrecking_crew_is_active() || Rfg_camera.mode == CAMERA_FREE_MODE)
    {
        return true;
    }
    return CAMERA_DEATH_MODE < Rfg_camera.mode && Rfg_camera.mode < CAMERA_TEST_SUITE_MODE;
}

// The shake takes a free slot (or restarts its own) at the intensity (0-100); its foley cue plays unless it still
// plays (inlined in each starter in the binary).
static void camera_shake_add(camera_shake* shake, float intensity)
{
    if (intensity - 0.0f < 0.0f)
    {
        intensity = 0.0f;
    }
    if (intensity - 100.0f >= 0.0f)
    {
        intensity = 100.0f;
    }
    int slot = 0;
    for (; slot < 5; slot++)
    {
        if (Cur_camera_shake[slot] == nullptr || Cur_camera_shake[slot] == shake)
        {
            break;
        }
    }
    if (slot == 5)
    {
        return;
    }
    Cur_camera_shake[slot] = shake;
    Camera_shake_time[slot] = 0.0f;
    Camera_shake_intensity[slot] = intensity;
    if (shake->foley_ref != -1 && (shake->foley_play_id == -1 || !audiolib_is_playing(shake->foley_play_id, true)))
    {
        audiolib_params params;
        params.volume = intensity;
        shake->foley_play_id = audiolib_cue_play(shake->foley_ref, &params, nullptr);
    }
}

// 0x006C6E00 (:395)
bool camera_shake_play(camera_shake* shake, float intensity, bool ignore_disabled)
{
    if ((!Camera_shake_enabled && !ignore_disabled) || shake == nullptr || !camera_shake_mode_allows())
    {
        return false;
    }
    int slot = 0;
    for (; slot < 5; slot++)
    {
        if (Cur_camera_shake[slot] == nullptr || Cur_camera_shake[slot] == shake)
        {
            break;
        }
    }
    if (slot == 5)
    {
        return false;
    }
    camera_shake_add(shake, intensity);
    return true;
}

// 0x006D4230 (:456)
bool camera_shake_play(const char* name, float intensity, bool ignore_disabled)
{
    return camera_shake_play(camera_shake_get(name), intensity, ignore_disabled); // the lookup is inlined in the binary
}

// 0x006D4340 (:497)
// A nearby impact (intensity over the squared distance, at least 1): above 0.5 the nearby shard shake at half of it;
// above the current shake intensity the strong one (and the shake intensity rises to it).
void camera_shake_start(float intensity, float dist_sqr)
{
    if (dist_sqr - 1.0f < 0.0f)
    {
        dist_sqr = 1.0f;
    }
    const float strength = intensity / dist_sqr;
    if (0.5f < strength)
    {
        float nearby = strength * 0.5f;
        if (nearby - 1.0f >= 0.0f)
        {
            nearby = 1.0f;
        }
        camera_shake* shake = camera_shake_get("shard_impact_nearby");
        if (Camera_shake_enabled && shake != nullptr && camera_shake_mode_allows())
        {
            camera_shake_add(shake, nearby);
        }
    }
    if (Shake_intensity < strength)
    {
        float strong = strength;
        if (strength - 1.0f >= 0.0f)
        {
            strong = 1.0f;
        }
        Shake_intensity = strength;
        camera_shake* shake = camera_shake_get("shard_impact_strong");
        if (Camera_shake_enabled && shake != nullptr && camera_shake_mode_allows())
        {
            camera_shake_add(shake, strong);
        }
    }
}

// 0x006D46E0 (:518)
void camera_shake_start_from_explosion(const vector& pos, float magnitude)
{
    if (magnitude - 0.0f < 0.0f)
    {
        magnitude = 0.0f;
    }
    if (magnitude - 1.0f >= 0.0f)
    {
        magnitude = 1.0f;
    }
    camera_shake* shake = camera_shake_get("explosion_small");
    if (Camera_shake_enabled && shake != nullptr && camera_shake_mode_allows())
    {
        camera_shake_add(shake, magnitude);
    }
}

// 0x006D48A0 (:534)
// Impact energy within reach (the energy covers the squared distance) shakes by sqrt(energy) over the distance.
void camera_shake_start_from_impact(float energy, float dist_sqr)
{
    if (dist_sqr <= energy)
    {
        float intensity = sqrtf(energy) / (Camera_shake_minimum_distance + dist_sqr) / Camera_shake_energy_scaler;
        if (intensity - 0.0f < 0.0f)
        {
            intensity = 0.0f;
        }
        if (intensity - 1.0f >= 0.0f)
        {
            intensity = 1.0f;
        }
        if (Camera_shake_minimum_intensity <= intensity)
        {
            camera_shake_start(intensity, 1.0f);
        }
    }
}

// 0x006D4980 (:555)
// A stressed building within reach: the building stress shake, by the energy's power curve and fading out to 1000
// squared metres.
void camera_shake_start_from_stress(float energy, float dist_sqr)
{
    if (dist_sqr <= energy)
    {
        // Magic numbers: pow(energy, 0.25) over the shake curve's 15 steps; 1000 - 900 squared metres of falloff.
        // The pow argument is the asm's x87 __CIpow (not checked).
        const float curve = (float)pow((double)energy, 0.25);
        float intensity = (1000.0f - dist_sqr) * ((curve - 1.0f) / 15.0f) / 900.0f;
        if (0.0f - intensity >= 0.0f)
        {
            intensity = 0.0f;
        }
        camera_shake* shake = camera_shake_get("building_stress");
        if (Camera_shake_enabled && shake != nullptr && camera_shake_mode_allows())
        {
            camera_shake_add(shake, intensity);
        }
    }
}

// Picks the louder value of a channel (absolute) for one of the 3 angles (inlined).
static void camera_shake_max_abs(float& acc, float value)
{
    if (fabsf(acc) < fabsf(value))
    {
        acc = value;
    }
}

// A random walk channel restarts its timer after the period, minus how late it is (inlined per channel).
static void camera_shake_restart_timer(timestamp& timer, int period)
{
    int next = period;
    if (timer.valid())
    {
        next = period - timer.time_since();
        if (next < 0)
        {
            next = period;
        }
    }
    timer.set(next);
}

// The cosine blend of a channel over its period: 1 at the start, 0 at the end (x87 __CIcos).
static float camera_shake_cos_blend(const timestamp& timer, int period)
{
    const float angle = (1.0f - (float)timer.time_until() / (float)period) * 3.14159274f;
    const float c = (float)cos((double)angle);
    return (c + 1.0f) * 0.5f;
}

// 0x006CCDF0 (:601)
// The active shakes' channels at their strength graph's time (the loudest of each), then: jitter (random targets
// every 33 ms), destabilisation and wander (random targets at their frequencies, cosine blended), wobble (random
// alternating every frame), the two oscillations (alternating at their frequencies) and the direct offsets add pitch,
// bank and heading that rotate the real orientation; wander and the direct wander turn the lookaround angles (the free
// camera's orientation). The blur and the controller vibration follow the loudest values. The random targets use
// fl_rand (the global generator).
void camera_shake_process()
{
    if (World.sl_flags.save_game_warp)
    {
        return;
    }
    vector jitter = ZERO_VECTOR;
    vector destable = ZERO_VECTOR;
    float destable_frequency = 0.0f;
    vector wander = ZERO_VECTOR;
    float wander_frequency = 0.0f;
    vector oscillate1 = ZERO_VECTOR;
    float oscillate1_frequency = 0.0f;
    vector oscillate2 = ZERO_VECTOR;
    float oscillate2_frequency = 0.0f;
    vector wobble = ZERO_VECTOR;
    float wobble_variation = 0.0f;
    float blur = 0.0f;
    float vibration_strong = 0.0f;
    float vibration_weak = 0.0f;
    float pitch = 0.0f;
    float bank = 0.0f;
    float heading = 0.0f;
    bool shaking = false;
    for (unsigned int i = 0; i < 5; i++)
    {
        const camera_shake* shake = Cur_camera_shake[i];
        if (shake == nullptr)
        {
            continue;
        }
        // The last graph point at or before the shake's time; past the last one the shake ends.
        // Original: before the first point the index stays -1 (the graphs start at time 0).
        int idx = -1;
        for (int k = 0; k < shake->num_weights; k++)
        {
            if (!(shake->weights[k].time <= Camera_shake_time[i]))
            {
                break;
            }
            if (k == shake->num_weights - 1)
            {
                Cur_camera_shake[i] = nullptr;
                break;
            }
            idx = k;
        }
        shake = Cur_camera_shake[i];
        if (shake == nullptr)
        {
            continue;
        }
        const weight_times& w0 = shake->weights[idx];
        const weight_times& w1 = shake->weights[idx + 1];
        const float t = (Camera_shake_time[i] - w0.time) / (w1.time - w0.time);
        const float s = 1.0f - t;
        const float intensity = Camera_shake_intensity[i];
        // Magic numbers: the graph weights are tenths (and 0.003 for the wander channels).
        float amount = (w1.jitter * t + w0.jitter * s) * 0.1f * intensity;
        camera_shake_max_abs(jitter.x, shake->jitter.x * amount);
        camera_shake_max_abs(jitter.y, shake->jitter.y * amount);
        camera_shake_max_abs(jitter.z, shake->jitter.z * amount);
        amount = (w1.destable * t + w0.destable * s) * 0.1f * intensity;
        camera_shake_max_abs(destable.x, shake->destable.x * amount);
        camera_shake_max_abs(destable.y, shake->destable.y * amount);
        camera_shake_max_abs(destable.z, shake->destable.z * amount);
        if (destable_frequency < shake->destable_frequency)
        {
            destable_frequency = shake->destable_frequency;
        }
        amount = (w1.oscillate1 * t + w0.oscillate1 * s) * 0.1f * intensity;
        camera_shake_max_abs(oscillate1.x, shake->oscillate1.x * amount);
        camera_shake_max_abs(oscillate1.y, shake->oscillate1.y * amount);
        camera_shake_max_abs(oscillate1.z, shake->oscillate1.z * amount);
        if (oscillate1_frequency < shake->oscillate1_frequency)
        {
            oscillate1_frequency = shake->oscillate1_frequency;
        }
        amount = (w1.oscillate2 * t + w0.oscillate2 * s) * 0.1f * intensity;
        camera_shake_max_abs(oscillate2.x, shake->oscillate2.x * amount);
        camera_shake_max_abs(oscillate2.y, shake->oscillate2.y * amount);
        camera_shake_max_abs(oscillate2.z, shake->oscillate2.z * amount);
        if (oscillate2_frequency < shake->oscillate2_frequency)
        {
            oscillate2_frequency = shake->oscillate2_frequency;
        }
        amount = (w1.wander * t + w0.wander * s) * 0.003f * intensity;
        camera_shake_max_abs(wander.x, shake->wander.x * amount);
        camera_shake_max_abs(wander.z, shake->wander.z * amount);
        if (wander_frequency < shake->wander_frequency)
        {
            wander_frequency = shake->wander_frequency;
        }
        amount = (w1.wobble * t + w0.wobble * s) * 0.1f * intensity;
        camera_shake_max_abs(wobble.x, shake->wobble.x * amount);
        camera_shake_max_abs(wobble.y, shake->wobble.y * amount);
        camera_shake_max_abs(wobble.z, shake->wobble.z * amount);
        shaking = true;
        if (wobble_variation < shake->wobble_variation)
        {
            wobble_variation = shake->wobble_variation;
        }
        // Magic number: 0.0001, a negligible weight.
        amount = (w1.direct * t + w0.direct * s) * 0.1f * intensity;
        if (0.0001f <= amount - 0.0f || 0.0001f <= -(amount - 0.0f))
        {
            pitch = shake->direct.x * amount + pitch;
            bank = shake->direct.y * amount + bank;
            heading = shake->direct.z * amount + heading;
        }
        amount = (w1.wander_direct * t + w0.wander_direct * s) * 0.003f * intensity;
        if (0.0001f <= amount - 0.0f || 0.0001f <= -(amount - 0.0f))
        {
            const float dp = shake->wander_direct.x * amount;
            const float dh = shake->wander_direct.z * amount;
            if (Rfg_camera.mode == CAMERA_FREE_MODE)
            {
                Rfg_camera.real_orient.unrotate(matrix(dp, 0.0f, dh));
            }
            else
            {
                Rfg_camera.lookaround_params.pitch = dp + Rfg_camera.lookaround_params.pitch;
                Rfg_camera.lookaround_params.heading = dh + Rfg_camera.lookaround_params.heading;
            }
        }
        const float value_blur = (w0.blur * s + w1.blur * t) * intensity;
        camera_shake_max_abs(blur, value_blur);
        const float strong = (w1.controller_vibration_strong * t + w0.controller_vibration_strong * s) * intensity;
        if (fabsf(vibration_strong) < fabsf(strong))
        {
            vibration_strong = strong;
            if (1.0f - strong < 0.0f)
            {
                vibration_strong = 1.0f;
            }
        }
        const float weak = (w1.controller_vibration_weak * t + w0.controller_vibration_weak * s) * intensity;
        if (fabsf(vibration_weak) < fabsf(weak))
        {
            vibration_weak = weak;
            if (1.0f - weak < 0.0f)
            {
                vibration_weak = 1.0f;
            }
        }
        Camera_shake_time[i] = Frametime + Camera_shake_time[i];
    }

    // jitter: a new random target every 33 ms, blended linearly from the last one
    if (jitter.is_null())
    {
        Camera_shake_jitter.timer.invalidate();
        Camera_shake_jitter.random_pitch = 0.0f;
        Camera_shake_jitter.random_bank = 0.0f;
        Camera_shake_jitter.random_heading = 0.0f;
    }
    else
    {
        if (Camera_shake_jitter.timer.elapsed() || !Camera_shake_jitter.timer.valid())
        {
            Camera_shake_jitter.last_random_pitch = Camera_shake_jitter.random_pitch;
            Camera_shake_jitter.last_random_bank = Camera_shake_jitter.random_bank;
            Camera_shake_jitter.last_random_heading = Camera_shake_jitter.random_heading;
            float r = fl_rand();
            Camera_shake_jitter.random_pitch = (r + r) - 1.0f;
            r = fl_rand();
            Camera_shake_jitter.random_bank = (r + r) - 1.0f;
            r = fl_rand();
            Camera_shake_jitter.random_heading = (r + r) - 1.0f;
            camera_shake_restart_timer(Camera_shake_jitter.timer, 33); // Magic number: 33 ms between targets.
        }
        const float last = (float)Camera_shake_jitter.timer.time_until() / 33.0f;
        const float next = 1.0f - last;
        pitch = Camera_shake_jitter.random_pitch * jitter.x * next + Camera_shake_jitter.last_random_pitch * jitter.x * last + pitch;
        bank = Camera_shake_jitter.random_bank * jitter.y * next + Camera_shake_jitter.last_random_bank * jitter.y * last + bank;
        heading = Camera_shake_jitter.random_heading * jitter.z * next + Camera_shake_jitter.last_random_heading * jitter.z * last + heading;
    }

    // destabilisation: random targets at its frequency, cosine blended
    if (destable.is_null())
    {
        Camera_shake_destable.timer.invalidate();
        Camera_shake_destable.random_pitch = 0.0f;
        Camera_shake_destable.random_bank = 0.0f;
        Camera_shake_destable.random_heading = 0.0f;
    }
    else
    {
        const int period = (int)(1000.0f / destable_frequency);
        if (Camera_shake_destable.timer.elapsed() || !Camera_shake_destable.timer.valid())
        {
            Camera_shake_destable.last_random_pitch = Camera_shake_destable.random_pitch;
            Camera_shake_destable.last_random_bank = Camera_shake_destable.random_bank;
            Camera_shake_destable.last_random_heading = Camera_shake_destable.random_heading;
            float r = fl_rand();
            Camera_shake_destable.random_pitch = (r + r) - 1.0f;
            r = fl_rand();
            Camera_shake_destable.random_bank = (r + r) - 1.0f;
            r = fl_rand();
            Camera_shake_destable.random_heading = (r + r) - 1.0f;
            camera_shake_restart_timer(Camera_shake_destable.timer, period);
        }
        const float last = camera_shake_cos_blend(Camera_shake_destable.timer, period);
        const float next = 1.0f - last;
        pitch = Camera_shake_destable.last_random_pitch * destable.x * last + Camera_shake_destable.random_pitch * destable.x * next + pitch;
        bank = Camera_shake_destable.random_bank * destable.y * next + Camera_shake_destable.last_random_bank * destable.y * last + bank;
        heading = Camera_shake_destable.random_heading * destable.z * next + Camera_shake_destable.last_random_heading * destable.z * last + heading;
    }

    // wobble: alternating sides every frame, by a random amount (the variation)
    if (wobble.is_null())
    {
        Camera_shake_wobble.last_random_pitch = 1.0f;
        Camera_shake_wobble.last_random_bank = 1.0f;
        Camera_shake_wobble.last_random_heading = 1.0f;
    }
    else
    {
        Camera_shake_wobble.last_random_pitch = -Camera_shake_wobble.last_random_pitch;
        Camera_shake_wobble.last_random_bank = -Camera_shake_wobble.last_random_bank;
        Camera_shake_wobble.last_random_heading = -Camera_shake_wobble.last_random_heading;
        Camera_shake_wobble.random_pitch = fl_rand() * Camera_shake_wobble.last_random_pitch;
        Camera_shake_wobble.random_bank = fl_rand() * Camera_shake_wobble.last_random_bank;
        Camera_shake_wobble.random_heading = fl_rand() * Camera_shake_wobble.last_random_heading;
        const float fixed = 1.0f - wobble_variation;
        pitch = Camera_shake_wobble.random_pitch * wobble.x * wobble_variation + Camera_shake_wobble.last_random_pitch * wobble.x * fixed + pitch;
        bank = Camera_shake_wobble.random_bank * wobble.y * wobble_variation + Camera_shake_wobble.last_random_bank * wobble.y * fixed + bank;
        heading = Camera_shake_wobble.random_heading * wobble.z * wobble_variation + Camera_shake_wobble.last_random_heading * wobble.z * fixed + heading;
    }

    // wander: random pitch and heading targets at its frequency, cosine blended, turning the camera
    if (wander.is_null())
    {
        Camera_shake_wander.timer.invalidate();
        Camera_shake_wander.random_pitch = 0.0f;
        Camera_shake_wander.random_heading = 0.0f;
    }
    else
    {
        const int period = (int)(1000.0f / wander_frequency);
        if (Camera_shake_wander.timer.elapsed() || !Camera_shake_wander.timer.valid())
        {
            Camera_shake_wander.last_random_pitch = Camera_shake_wander.random_pitch;
            Camera_shake_wander.last_random_heading = Camera_shake_wander.random_heading;
            float r = fl_rand();
            Camera_shake_wander.random_pitch = (r + r) - 1.0f;
            r = fl_rand();
            Camera_shake_wander.random_heading = (r + r) - 1.0f;
            camera_shake_restart_timer(Camera_shake_wander.timer, period);
        }
        const float last = camera_shake_cos_blend(Camera_shake_wander.timer, period);
        const float dp = Camera_shake_wander.random_pitch * wander.x * (1.0f - last) + Camera_shake_wander.last_random_pitch * wander.x * last;
        const float dh = Camera_shake_wander.random_heading * wander.z * (1.0f - last) + Camera_shake_wander.last_random_heading * wander.z * last;
        if (Rfg_camera.mode == CAMERA_FREE_MODE)
        {
            Rfg_camera.real_orient.unrotate(matrix(dp, 0.0f, dh));
        }
        else
        {
            Rfg_camera.lookaround_params.pitch = dp + Rfg_camera.lookaround_params.pitch;
            Rfg_camera.lookaround_params.heading = dh + Rfg_camera.lookaround_params.heading;
        }
    }

    // the oscillations: alternating sides at their frequencies, cosine blended
    camera_shake_type_info* oscillators[2] = {&Camera_shake_oscillate1, &Camera_shake_oscillate2};
    const vector* amplitudes[2] = {&oscillate1, &oscillate2};
    const float frequencies[2] = {oscillate1_frequency, oscillate2_frequency};
    for (int o = 0; o < 2; o++)
    {
        camera_shake_type_info& osc = *oscillators[o];
        const vector& amp = *amplitudes[o];
        if (amp.is_null())
        {
            osc.timer.invalidate();
            osc.random_pitch = 1.0f;
            osc.random_bank = 1.0f;
            osc.random_heading = 1.0f;
            continue;
        }
        const int period = (int)(1000.0f / frequencies[o]);
        if (osc.timer.elapsed() || !osc.timer.valid())
        {
            osc.last_random_bank = osc.random_bank;
            osc.last_random_pitch = osc.random_pitch;
            osc.random_bank = osc.random_bank * -1.0f;
            osc.random_pitch = osc.random_pitch * -1.0f;
            osc.last_random_heading = osc.random_heading;
            osc.random_heading = osc.random_heading * -1.0f;
            camera_shake_restart_timer(osc.timer, period);
        }
        const float last = camera_shake_cos_blend(osc.timer, period);
        const float next = 1.0f - last;
        pitch = osc.random_pitch * amp.x * next + osc.last_random_pitch * amp.x * last + pitch;
        bank = osc.random_bank * amp.y * next + osc.last_random_bank * amp.y * last + bank;
        heading = osc.random_heading * amp.z * next + osc.last_random_heading * amp.z * last + heading;
    }

    if (0.0001f <= blur - 0.0f || 0.0001f <= -(blur - 0.0f))
    {
        fb_accum_blur_set_enable(true);
        fb_accum_blur_change_percentage(blur);
    }
    if (vibration_strong < 0.0f)
    {
        vibration_strong = 0.0f;
    }
    if (vibration_weak < 0.0f)
    {
        vibration_weak = 0.0f;
    }
    if (game_is_paused() || !control_is_controller_in_use() ||
        (vibration_strong - 0.0f < 0.0001f && -(vibration_strong - 0.0f) < 0.0001f && vibration_weak - 0.0f < 0.0001f && -(vibration_weak - 0.0f) < 0.0001f) ||
        (Multiplayer && (multi_game_spectating() || multi_game_camera_following_killer())))
    {
        vibration_strong = 0.0f;
        vibration_weak = 0.0f;
    }
    input_set_feedback(multi_core_platform_get_active_controller(), vibration_strong, vibration_weak);
    if (shaking)
    {
        Rfg_camera.real_orient = matrix(pitch, bank, heading) * Rfg_camera.real_orient;
    }
    Shake_intensity = Shake_intensity - Camera_shake_decay_rate * Frametime;
    if (0.0f - Shake_intensity >= 0.0f)
    {
        Shake_intensity = 0.0f;
    }
}