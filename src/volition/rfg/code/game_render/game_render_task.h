// Original: volition/rfg/code/game_render/game_render_task.h
// Implementation: volition/rfg/code/game_render/game_render_task.cpp
//
// game_render_task, a unit of render work queued in a linked_list_p and run by game_render_task_process_all with a
// renderer (pre-stream render tasks, per-frame callbacks).
#pragma once

#include "volition/render_lib/code/linked_list_p.h"

class rl_renderer;

// size 0x14
struct game_render_task
{
    game_render_task(); // 0x006E1800 (.h:16)

    bool* m_enabled_p; // 0x00
    void (*m_callback)(void*, rl_renderer*); // 0x04
    void* m_user_data_p; // 0x08
    game_render_task* next; // 0x0C
    game_render_task* prev; // 0x10
};

void game_render_task_process_all(const linked_list_p<game_render_task>& task_list, rl_renderer* renderer); // 0x007F14F0 (:6)
