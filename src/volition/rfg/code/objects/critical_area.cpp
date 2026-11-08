// Original: volition/rfg/code/objects/critical_area.cpp
//
// The critical area of the active mission: see critical_area.h. Flags: 1 exists, 2 the player has been inside, 4 the
// player is inside, 8 failed.

#include "volition/rfg/code/objects/critical_area.h"

#include "volition/rfg/code/gameplay/mission_info.h"
#include "volition/rfg/code/gameplay/missions.h"
#include "volition/rfg/code/hud/hud_message.h"
#include "volition/rfg/code/interface/localization/localization_shared.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/scripting/action_script/global_event_data.h"
#include "volition/rfg/code/scripting/action_script/global_event_manager.h"
#include "volition/rfg/code/scripting/action_script/script_timer.h"
#include "volition/vlib/code/cfile/cfile.h"
#include "volition/vlib/code/math/ix.h"

static critical_area Crit_area;

// Magic number: 8 s outside the area before the mission fails.
static const int CRITICAL_AREA_TIME_MS = 8000;

static void critical_area_reset()
{
    Crit_area.handle = 0xFFFFFFFF;
    Crit_area.radius = 0.0f;
    Crit_area.flags = 0;
    critical_area_timer_delete();
    Crit_area.voice_line_delay.invalidate();
}

// 0x00A05AC0
critical_area::critical_area()
{
}

// 0x00A05AD0 (:67)
void critical_area_level_init()
{
    Crit_area.handle = 0xFFFFFFFF;
    Crit_area.radius = 0.0f;
    Crit_area.flags = 0;
    Crit_area.tmr_handle = 0xFFFFFFFF;
    Crit_area.voice_line_delay.invalidate();
}

// 0x00A05B00 (:77)
void critical_area_timer_create()
{
    if (Crit_area.tmr_handle == 0xFFFFFFFF)
    {
        Crit_area.tmr_handle = script_timer_create(CRITICAL_AREA_TIME_MS);
    }
}

// 0x00A05B20 (:88)
void critical_area_timer_delete()
{
    if (Crit_area.tmr_handle != 0xFFFFFFFF)
    {
        script_timer_delete(Crit_area.tmr_handle);
        Crit_area.tmr_handle = 0xFFFFFFFF;
    }
}

// 0x00A05B40 (:99)
void critical_area_delete_internal()
{
    critical_area_reset();
}

// 0x00A05B80 (:110)
void critical_area_delete()
{
    if ((Crit_area.flags & 8) == 0)
    {
        critical_area_reset();
    }
}

// 0x00A05BD0 (:125)
bool critical_area_create(unsigned int handle, float radius)
{
    if (World.object_from_handle(handle) == nullptr)
    {
        return false;
    }
    critical_area_reset();
    Crit_area.handle = handle;
    Crit_area.radius = radius;
    Crit_area.flags = 1;
    critical_area_timer_create();
    Crit_area.voice_line_delay.invalidate();
    return true;
}

// 0x00A05C70 (:148)
void critical_area_activate()
{
    Crit_area.flags |= 2;
    player_enters_critical_area_event data;
    gem_post_event(data);
    critical_area_timer_delete();
}

// 0x00A7A950 (:162)
void critical_area_do_frame()
{
    if ((Crit_area.flags & 1) == 0 || (Crit_area.flags & 8) != 0 || !mission_is_active() || Player == nullptr)
    {
        return;
    }
    object* center = World.object_from_handle(Crit_area.handle);
    if (center == nullptr)
    {
        return;
    }
    const vector2 center_xz(center->pos.x, center->pos.z);
    const vector2 player_xz(Player->pos.x, Player->pos.z);
    if (ix2d_point_circle(player_xz, center_xz, Crit_area.radius))
    {
        if ((Crit_area.flags & 4) == 0)
        {
            critical_area_activate();
        }
        Crit_area.flags |= 4;
        return;
    }
    Crit_area.flags &= 0xFFFFFFFB;
    if ((Crit_area.flags & 2) == 0)
    {
        return;
    }
    if (Crit_area.tmr_handle != 0xFFFFFFFF)
    {
        if (script_timer_get_time(Crit_area.tmr_handle) < 1)
        {
            // Magic number: 4 s on screen.
            hud_message_add("MISS_CRIT_AREA_FAIL", 4.0f, nullptr, nullptr, nullptr);
            Crit_area.flags |= 8;
            mission_failure_event data;
            data.desc = "MISS_CRIT_AREA_FAIL";
            gem_post_event(data);
        }
        return;
    }
    mission_info* info = mission_info_get(mission_get_current_mission());
    human_voice_persona* persona = human_voice_radio_persona();
    if (!Crit_area.voice_line_delay.valid() || Crit_area.voice_line_delay.elapsed())
    {
        // Magic number: a warning every 4 s.
        Crit_area.voice_line_delay.set(4000);
        const char* warning = mission_crit_area_warning();
        if (warning != nullptr)
        {
            hud_message_add(localization_get_display_string_req(warning, nullptr), 4.0f, nullptr, nullptr, nullptr);
        }
        else if (persona != nullptr)
        {
            int cue = -1;
            unsigned short line = 0;
            human_voice_persona_line* linep = persona->find_voice_line(info->crit_area_line);
            if (linep == nullptr)
            {
                persona->last_operation_status = PLAY_VOICE_FAILURE_MISSING;
            }
            else
            {
                persona->persona_play_voice_line_internal(linep, ZERO_VECTOR, AUDIOLIB_CUE_PRIORITY_DIALOGUE_HIGH, cue, line, nullptr, -1, false, 0, true, true, nullptr);
            }
        }
    }
    Crit_area.tmr_handle = script_timer_create(CRITICAL_AREA_TIME_MS);
    script_timer_display(Crit_area.tmr_handle, TP_HIGH);
}

// 0x00A05CB0 (:258)
void critical_area_save(cfile* f)
{
    cf_write_uint32(Crit_area.handle, f);
    cf_write_float(Crit_area.radius, f);
    cf_write_uint32(Crit_area.flags, f);
    cf_write_uint32(Crit_area.tmr_handle, f);
}

// 0x00A05D00 (:267)
void critical_area_load(cfile* f)
{
    Crit_area.handle = cf_read_uint32(f);
    Crit_area.radius = cf_read_float(f);
    Crit_area.flags = cf_read_uint32(f);
    Crit_area.tmr_handle = cf_read_uint32(f);
}

// 0x00A05D40 (:275)
void critical_area_level_shutdown()
{
    critical_area_reset();
}
