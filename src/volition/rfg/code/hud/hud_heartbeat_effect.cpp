// Original: volition/rfg/code/hud/hud_heartbeat_effect.cpp
//
// The low health heartbeat (stub_impl.md S7): effect_heartbeat.xtbl maps health to the flash, the sound, the shake, the
// blur, the saturation and the bloom.

#include "volition/rfg/code/hud/hud_heartbeat_effect.h"

#include "volition/audio_lib/code/audiolib_api.h"
#include "volition/rfg/code/camera/camera_shake.h"
#include "volition/rfg/code/game_render/fullscreen_effects/fb_color_correct.h"
#include "volition/rfg/code/gamewide/game_clock.h"
#include "volition/rfg/code/gamewide/game_render.h"
#include "volition/rfg/code/gamewide/gameaudio.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/render_lib/code/rl_color.h"
#include "volition/vlib/code/parse/xml_table_tools.h"
#include "volition/vlib/code/time/timestamp.h"
#include "volition/vlib/code/util/fastxml.h"

// ---- file-private types (rfg.pdb)
// size 0x28
struct heartbeat_table_entry
{
    float health; // 0x00
    float red_ring; // 0x04
    float flash_max; // 0x08
    float flash_min; // 0x0C
    float flash_frequency; // 0x10
    float volume; // 0x14
    float shake; // 0x18
    float blur; // 0x1C
    float saturation; // 0x20
    float bloom; // 0x24
};
static_assert(sizeof(heartbeat_table_entry) == 0x28, "heartbeat_table_entry");

// ---- file-static globals
static const char HEARTBEAT_DATA_FILE_NAME[22] = "effect_heartbeat.xtbl"; // 0x012FB170
static const char HEARTBEAT_ROOT_TAG[17] = "effect_heartbeat"; // 0x012FB188
static const char HEARTBEAT_NAME_TAG[5] = "Name"; // 0x012FB19C
static const char HEARTBEAT_TIME_SHORT_TAG[21] = "heartbeat_time_short"; // 0x012FB1A4
static const char HEARTBEAT_TIME_LONG_TAG[20] = "heartbeat_time_long"; // 0x012FB1BC
static const char HEARTBEAT_GRID_TAG[15] = "heartbeat_info"; // 0x012FB1D0
static const char HEARTBEAT_ITEM_TAG[15] = "heartbeat_item"; // 0x012FB1E0
static const char HEARTBEAT_HEALTH_TAG[7] = "health"; // 0x012FB1F0
static const char HEARTBEAT_RED_RING_TAG[9] = "red_ring"; // 0x012FB1F8
static const char HEARTBEAT_FLASH_MAX_TAG[10] = "flash_max"; // 0x012FB204
static const char HEARTBEAT_FLASH_MIN_TAG[10] = "flash_min"; // 0x012FB210
static const char HEARTBEAT_FLASH_FREQ_TAG[16] = "flash_frequency"; // 0x012FB21C
static const char HEARTBEAT_VOLUME_TAG[7] = "volume"; // 0x012FB22C
static const char HEARTBEAT_SHAKE_TAG[6] = "shake"; // 0x012FB234
static const char HEARTBEAT_BLUR_TAG[5] = "blur"; // 0x012FB23C
static const char HEARTBEAT_SATURATION_TAG[11] = "saturation"; // 0x012FB244
static const char HEARTBEAT_BLOOM_TAG[6] = "bloom"; // 0x012FB250
static float Heartbeat_time_short = 400.0f; // 0x01660F04
static float Heartbeat_time_long = 800.0f; // 0x01660F08
static float Heartbeat_override = 1.0f; // 0x01660F0C
static timestamp_percent Heartbeat_timestamp; // 0x01661780, initial data ffffffff
static heartbeat_table_entry Heartbeat_table[10]; // 0x02B953F8
static bool Heartbeat_table_loaded; // 0x02B9DC7C
static unsigned char Heartbeat_num_items; // 0x02B9DC7D
static float Heartbeat_alpha; // 0x02BE584C
static bool Up_beat; // 0x02BE5850

// 0x008559D0 (a copy of heartbeat_load_data in rfg.exe, inlined)
void unused_dcf_reload_heartbeat()
{
    heartbeat_load_data();
}

// 0x00843230
float heartbeat_get_health()
{
    float health = 1.0f;
    player* pp = player_get_local_player();
    if (pp != nullptr && (float)pp->max_hit_points > 0.0f)
    {
        health = pp->hit_points / (float)pp->max_hit_points;
    }
    return health;
}

// 0x00843280
heartbeat_table_position get_table_position(float player_health)
{
    heartbeat_table_position position;
    position.lower_index = 0;
    position.higher_index = 0;
    position.interpolation_percent = 0.0f;
    if (Heartbeat_num_items == 0)
    {
        Heartbeat_table_loaded = false;
        return position;
    }
    if (Heartbeat_table_loaded)
    {
        // The health is clamped to the table's range.
        if (Heartbeat_table[Heartbeat_num_items - 1].health <= player_health)
        {
            player_health = Heartbeat_table[Heartbeat_num_items - 1].health;
        }
        if (player_health <= Heartbeat_table[0].health)
        {
            player_health = Heartbeat_table[0].health;
        }
        bool found = false;
        for (unsigned char i = 1; i < Heartbeat_num_items && !found; i++)
        {
            if (Heartbeat_table[i].health < player_health)
            {
                position.lower_index = i;
            }
            else
            {
                position.higher_index = i;
                found = true;
            }
        }
        const float lower = Heartbeat_table[position.lower_index].health;
        if (Heartbeat_table[position.higher_index].health <= lower)
        {
            // The table is not sorted.
            position.lower_index = 0;
            position.higher_index = 0;
            Heartbeat_table_loaded = false;
            return position;
        }
        position.interpolation_percent = (player_health - lower) / (Heartbeat_table[position.higher_index].health - lower);
    }
    return position;
}

// 0x008433A0
void heartbeat_load_data()
{
    Heartbeat_table_loaded = false;
    Heartbeat_num_items = 0;
    xml_element* root = xml_table_parse(HEARTBEAT_DATA_FILE_NAME);
    if (root == nullptr)
    {
        xml_table_close(nullptr);
        return;
    }
    xml_element* effect = xml_find(root, HEARTBEAT_ROOT_TAG);
    if (effect == nullptr || xml_find(effect, HEARTBEAT_NAME_TAG) == nullptr)
    {
        xml_table_close(root);
        return;
    }
    xml_table_get_req_float(Heartbeat_time_short, effect, HEARTBEAT_TIME_SHORT_TAG);
    xml_table_get_req_float(Heartbeat_time_long, effect, HEARTBEAT_TIME_LONG_TAG);
    xml_element* grid = xml_find(effect, HEARTBEAT_GRID_TAG);
    if (grid == nullptr)
    {
        xml_table_close(root);
        return;
    }
    // Original bug kept: no check against the 10 entries of Heartbeat_table.
    for (xml_element* item = xml_find(grid, HEARTBEAT_ITEM_TAG); item != nullptr; item = xml_find_next(grid, item, HEARTBEAT_ITEM_TAG))
    {
        heartbeat_table_entry& entry = Heartbeat_table[Heartbeat_num_items];
        xml_table_get_req_float(entry.health, item, HEARTBEAT_HEALTH_TAG);
        xml_table_get_req_float(entry.red_ring, item, HEARTBEAT_RED_RING_TAG);
        xml_table_get_req_float(entry.flash_min, item, HEARTBEAT_FLASH_MIN_TAG);
        xml_table_get_req_float(entry.flash_max, item, HEARTBEAT_FLASH_MAX_TAG);
        xml_table_get_req_float(entry.flash_frequency, item, HEARTBEAT_FLASH_FREQ_TAG);
        xml_table_get_req_float(entry.volume, item, HEARTBEAT_VOLUME_TAG);
        xml_table_get_req_float(entry.shake, item, HEARTBEAT_SHAKE_TAG);
        xml_table_get_req_float(entry.blur, item, HEARTBEAT_BLUR_TAG);
        xml_table_get_req_float(entry.saturation, item, HEARTBEAT_SATURATION_TAG);
        xml_table_get_req_float(entry.bloom, item, HEARTBEAT_BLOOM_TAG);
        Heartbeat_num_items++;
    }
    xml_table_close(root);
    if (Heartbeat_num_items == 0)
    {
        return;
    }
    Heartbeat_table_loaded = true;
}

// 0x00843620
void heartbeat_reset_effects()
{
    rl_color_float tint;
    tint.red = 1.0f;
    tint.green = 1.0f;
    tint.blue = 1.0f;
    tint.alpha = 1.0f;
    fb_color_correct_modulate_tint(tint);
    fb_color_correct_modulate_saturation(1.0f);
    fb_color_correct_bloom_exposure_factor(1.0f);
    if (!game_is_paused())
    {
        game_render_set_camera_frame_override_blur(0.0f);
    }
}

// 0x008436A0
void heartbeat_process(float hit_damage_amount)
{
    float health = heartbeat_get_health();
    if (Heartbeat_override < 1.0f)
    {
        health = Heartbeat_override;
    }
    const heartbeat_table_position position = get_table_position(health);
    if (!Heartbeat_table_loaded)
    {
        return;
    }
    const heartbeat_table_entry& lower = Heartbeat_table[position.lower_index];
    const heartbeat_table_entry& higher = Heartbeat_table[position.higher_index];
    const float t = position.interpolation_percent;
    const float flash_min = (higher.flash_min - lower.flash_min) * t + lower.flash_min;
    const float flash_max = (higher.flash_max - lower.flash_max) * t + lower.flash_max;
    const float frequency = (higher.flash_frequency - lower.flash_frequency) * t + lower.flash_frequency;

    if (health - 0.0f < 0.0f)
    {
        health = 0.0f;
    }
    if (health - 1.0f >= 0.0f)
    {
        health = 1.0f;
    }

    if (!Heartbeat_timestamp.valid() || Heartbeat_timestamp.elapsed())
    {
        if (health < 0.0f || health >= 1.0f)
        {
            Heartbeat_alpha = 0.0f;
        }
        else
        {
            Up_beat = !Up_beat;
            const float short_time = Heartbeat_time_short * frequency;
            const float time = (Heartbeat_time_long * frequency - short_time) * t + short_time;
            if (Up_beat)
            {
                // Magic number: the up beat takes 30 % of the beat's time.
                Heartbeat_timestamp.set((int)(time * 0.3f));
                player* pp = player_get_local_player();
                // Magic number: the sound and the shake start below 35 % health.
                if ((pp == nullptr || !pp->is_killed()) && health < 0.35f)
                {
                    const int play_id = audiolib_cue_play(Ga_cue_ids.heartbeat, nullptr, nullptr);
                    if (play_id != -1)
                    {
                        audiolib_play_set_volume(play_id, (higher.volume - lower.volume) * t + lower.volume, 100);
                        camera_shake_play("player_heartbeat", (higher.shake - lower.shake) * t + lower.shake, false);
                    }
                }
            }
            else
            {
                Heartbeat_timestamp.set((int)time);
            }
        }
    }
    else
    {
        float percent = Heartbeat_timestamp.percent();
        if (Up_beat)
        {
            percent = 1.0f - percent;
        }
        Heartbeat_alpha = (flash_max - flash_min) * percent + flash_min;
    }

    float amount = Heartbeat_alpha + hit_damage_amount;
    if (amount - 1.0f >= 0.0f)
    {
        amount = 1.0f;
    }
    // Magic numbers, 0x02BE583C: the static local damage_tint, the color at full damage.
    static const float damage_tint_x = 1.0f;
    static const float damage_tint_y = 0.65f;
    static const float damage_tint_z = 0.55f;
    rl_color_float tint;
    tint.red = (damage_tint_x - 1.0f) * amount + 1.0f;
    tint.green = (damage_tint_y - 1.0f) * amount + 1.0f;
    tint.blue = (damage_tint_z - 1.0f) * amount + 1.0f;
    tint.alpha = 1.0f;
    fb_color_correct_modulate_tint(tint);
    fb_color_correct_modulate_saturation((higher.saturation - lower.saturation) * t + lower.saturation);
    fb_color_correct_bloom_exposure_factor((higher.bloom - lower.bloom) * t + lower.bloom);
    if (!game_is_paused())
    {
        float blur = amount * ((higher.blur - lower.blur) * t + lower.blur);
        if (blur - 1.0f >= 0.0f)
        {
            blur = 1.0f;
        }
        game_render_set_camera_frame_override_blur(blur);
    }
}

// 0x00843CD0
float heartbeat_get_ring_alpha()
{
    float health = heartbeat_get_health();
    if (Heartbeat_override < 1.0f)
    {
        health = Heartbeat_override;
    }
    const heartbeat_table_position position = get_table_position(health);
    if (!Heartbeat_table_loaded)
    {
        return 0.0f;
    }
    return Heartbeat_table[position.lower_index].red_ring + position.interpolation_percent * (Heartbeat_table[position.higher_index].red_ring - Heartbeat_table[position.lower_index].red_ring);
}

// 0x00843DA0
void heartbeat_reset_timestamp()
{
    Heartbeat_timestamp.invalidate();
}

// 0x00843DB0
float heartbeat_get_current_blur()
{
    float health = heartbeat_get_health();
    if (Heartbeat_override < 1.0f)
    {
        health = Heartbeat_override;
    }
    const heartbeat_table_position position = get_table_position(health);
    if (!Heartbeat_table_loaded)
    {
        return 0.0f;
    }
    return Heartbeat_table[position.lower_index].blur + position.interpolation_percent * (Heartbeat_table[position.higher_index].blur - Heartbeat_table[position.lower_index].blur);
}
