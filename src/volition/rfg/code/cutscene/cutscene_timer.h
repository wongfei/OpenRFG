// Original: volition/rfg/code/cutscene/cutscene_timer.h
// Implementation: volition/rfg/code/cutscene/cutscene_timer.cpp
//
// The cutscene timer: elapsed time of the cutscene and of the current shot, the shot index, skipping to the next shot.
#pragma once

// size 0x1C
class cutscene_timer
{
public:
    cutscene_timer(); // 0x00705340 (.h:42)
    bool has_cutscene_elapsed() const; // 0x00706D90 (:41)
    float get_cutscene_elapsed_sec() const; // 0x006C39D0 (.h:48)
    float get_cutscene_duration_sec() const;
    float get_shot_remaining_sec() const; // 0x00705350 (.h:58)
    float get_shot_elapsed_sec() const; // 0x00705360 (.h:64)
    int get_cur_shot_index() const; // 0x00705370 (.h:70)
    float get_frame_time() const; // 0x00A05FD0 (.h:81)
    float get_frame_time_intershot() const;
    void start_playing(const float cs_duration_sec); // 0x00706DB0 (:49)
    void clear(); // 0x00706DE0 (:64)
    void advance(const float _frame_time); // 0x0070F9B0 (:81)
    void update_cur_shot_duration_sec(); // 0x0070DA20 (:149)
    void skip_to_next_shot(); // 0x00706E10 (:163)

private:
    float m_cs_elapsed_sec; // 0x00
    float m_cs_duration_sec; // 0x04
    int m_cur_shot_index; // 0x08
    float m_cur_shot_elapsed_sec; // 0x0C
    float m_cur_shot_duration_sec; // 0x10
    float m_frame_time; // 0x14
    float m_frame_time_intershot; // 0x18
};

// 0x00705340 (.h:42)
inline cutscene_timer::cutscene_timer()
{
}

// 0x006C39D0 (.h:48)
inline float cutscene_timer::get_cutscene_elapsed_sec() const
{
    return m_cs_elapsed_sec;
}

inline float cutscene_timer::get_cutscene_duration_sec() const
{
    return m_cs_duration_sec;
}

// 0x00705350 (.h:58)
inline float cutscene_timer::get_shot_remaining_sec() const
{
    return m_cur_shot_duration_sec - m_cur_shot_elapsed_sec;
}

// 0x00705360 (.h:64)
inline float cutscene_timer::get_shot_elapsed_sec() const
{
    return m_cur_shot_elapsed_sec;
}

// 0x00705370 (.h:70)
inline int cutscene_timer::get_cur_shot_index() const
{
    return m_cur_shot_index;
}

// 0x00A05FD0 (.h:81)
inline float cutscene_timer::get_frame_time() const
{
    return m_frame_time;
}

inline float cutscene_timer::get_frame_time_intershot() const
{
    return m_frame_time_intershot;
}
