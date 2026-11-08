// Original: volition/rfg/code/game_render/game_render_task.cpp
//
// The game_render_task constructor and game_render_task_process_all.

#include "volition/rfg/code/game_render/game_render_task.h"

// 0x006E1800 (.h:16)
// The list links are left to linked_list_p.
game_render_task::game_render_task()
{
    m_enabled_p = nullptr;
    m_callback = nullptr;
    m_user_data_p = nullptr;
}

// 0x007F14F0 (:6)
// The enabled tasks with a callback, in registration order.
void game_render_task_process_all(const linked_list_p<game_render_task>& task_list, rl_renderer* renderer)
{
    game_render_task* first_p = task_list.first();
    game_render_task* task_p = first_p;
    while (task_p != nullptr)
    {
        if ((task_p->m_enabled_p == nullptr || *task_p->m_enabled_p) && task_p->m_callback != nullptr)
        {
            task_p->m_callback(task_p->m_user_data_p, renderer);
        }
        task_p = task_p->next;
        if (task_p == first_p)
        {
            break;
        }
    }
}
