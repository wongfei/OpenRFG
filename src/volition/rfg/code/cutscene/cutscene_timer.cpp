// Original: volition/rfg/code/cutscene/cutscene_timer.cpp
//
// The cutscene timer: walks the shots as frame time passes, starting each one and updating the current one.

#include "volition/rfg/code/cutscene/cutscene_timer.h"

#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/rfg/code/cutscene/cutscene_internal.h"

// 0x00706D90 (:41)
bool cutscene_timer::has_cutscene_elapsed() const
{
    return m_cs_duration_sec <= m_cs_elapsed_sec;
}

// 0x00706DB0 (:49)
void cutscene_timer::start_playing(const float cs_duration_sec)
{
    m_cs_duration_sec = cs_duration_sec;
    m_cs_elapsed_sec = 0.0f;
    m_cur_shot_index = -1;
    m_cur_shot_duration_sec = 0.0f;
    m_cur_shot_elapsed_sec = 0.0f;
    m_frame_time = 0.0f;
    m_frame_time_intershot = 0.0f;
}

// 0x00706DE0 (:64)
void cutscene_timer::clear()
{
    m_cs_elapsed_sec = 0.0f;
    m_cs_duration_sec = 0.0f;
    m_cur_shot_index = -1;
    m_cur_shot_elapsed_sec = 0.0f;
    m_cur_shot_duration_sec = 0.0f;
    m_frame_time = 0.0f;
    m_frame_time_intershot = 0.0f;
}

// 0x0070F9B0 (:81)
void cutscene_timer::advance(const float _frame_time)
{
    const cutscene_current_info* const cur_cutscene = Current_cutscene;
    int shot_index = m_cur_shot_index;
    if (shot_index == -1)
    {
        m_cur_shot_index = 0;
        cutscene_start_next_shot(0);
        update_cur_shot_duration_sec();
        m_frame_time = 0.0f;
        m_frame_time_intershot = 0.0f;
        return;
    }

    m_frame_time = 0.0f;
    m_frame_time_intershot = _frame_time;
    float time_left = _frame_time;
    if (!(0.0f < time_left))
    {
        return;
    }
    for (;;)
    {
        const float shot_remaining = m_cur_shot_duration_sec - m_cur_shot_elapsed_sec;
        // Magic number: a shot ending within 1 ms of this frame's end counts as ended.
        if ((float)((double)time_left + 0.001) <= shot_remaining)
        {
            m_cs_elapsed_sec += time_left;
            m_cur_shot_elapsed_sec += time_left;
            m_frame_time = time_left;
            cutscene_update_shot(m_cur_shot_index);
            return;
        }
        if (cur_cutscene->m_shots.size() - 1 <= shot_index)
        {
            break;
        }
        m_cs_elapsed_sec += shot_remaining;
        m_cur_shot_elapsed_sec = 0.0f;
        m_cur_shot_index = shot_index + 1;
        cutscene_start_next_shot(shot_index + 1);
        shot_index = m_cur_shot_index;
        // rfg.exe inlines update_cur_shot_duration_sec here.
        update_cur_shot_duration_sec();
        time_left -= shot_remaining;
        if (time_left <= 0.0f)
        {
            return;
        }
    }
    // The last shot ran out. Original quirk kept: m_frame_time is what was left of the shot, not of the frame.
    m_frame_time = m_cur_shot_duration_sec - m_cur_shot_elapsed_sec;
    m_cs_elapsed_sec = m_cs_duration_sec;
    m_cur_shot_elapsed_sec = m_cur_shot_duration_sec;
}

// 0x0070DA20 (:149)
void cutscene_timer::update_cur_shot_duration_sec()
{
    const c_cutscene_camera* const script = Rfg_camera.cscript_params.camera_script;
    if (script != nullptr)
    {
        const c_cutscene_shot& shot = script->csc_shots[m_cur_shot_index];
        m_cur_shot_duration_sec = shot.end_time - shot.start_time;
        return;
    }
    m_cur_shot_duration_sec = frames_to_seconds(Current_cutscene->m_shots[m_cur_shot_index].duration_fm);
}

// 0x00706E10 (:163)
void cutscene_timer::skip_to_next_shot()
{
    const float shot_remaining = m_cur_shot_duration_sec - m_cur_shot_elapsed_sec;
    m_cur_shot_elapsed_sec += shot_remaining;
    m_cs_elapsed_sec += shot_remaining;
}
