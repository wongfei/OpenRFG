// OpenRFG's part of the game trace (--game-trace FILE, new code, not recovered; the trace itself is game_trace.h): the
// status line every 5 s and the crash line. Its status line reads the mission and object systems.

#include "volition/anim_lib/code/anim_lib.h"
#include "volition/rfg/code/camera/rfg_camera.h"
#include "volition/rfg/code/cutscene/video_cutscene.h"
#include "volition/rfg/code/geom/character_instance.h"
#include "volition/rfg/code/gameplay/gameflow/gameflow_enums.h"
#include "volition/rfg/code/gameplay/missions.h"
#include "volition/rfg/code/gamewide/game_trace.h"
#include "volition/rfg/code/gamewide/gameseq.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/objects/humans/npc.h"
#include "volition/rfg/code/objects/humans/player.h"
#include "volition/rfg/code/objects/object_squad.h"
#include "volition/rfg/code/scripting/action_script/action_script_internal.h"

#include <windows.h>

#include <math.h>
#include <string.h>

extern gfm_alert_levels Current_alert_level; // 0x022278B4 (gameplay.cpp)

namespace
{
    // Magic number: the status line's period in milliseconds.
    const long TICK_PERIOD = 5000;

    LPTOP_LEVEL_EXCEPTION_FILTER Previous_filter;

    // Writes the crash line, then WinMain's handler writes the minidump (OpenRFG.dmp next to the exe).
    LONG WINAPI crash_filter(EXCEPTION_POINTERS* pointers)
    {
        const EXCEPTION_RECORD* record = pointers->ExceptionRecord;
        HMODULE module = nullptr;
        char path[MAX_PATH] = "?";
        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT, (LPCSTR)record->ExceptionAddress, &module))
        {
            GetModuleFileNameA(module, path, sizeof(path));
        }
        const char* slash = strrchr(path, '\\');
        char access[64] = "";
        // Magic number: an access violation's parameters are the kind (0 read, 1 write, 8 execute) and the address.
        if (record->ExceptionCode == EXCEPTION_ACCESS_VIOLATION && record->NumberParameters >= 2)
        {
            snprintf(access, sizeof(access), " kind=%u address=0x%08X", (unsigned int)record->ExceptionInformation[0], (unsigned int)record->ExceptionInformation[1]);
        }
        GAME_TRACE("crash", "exception 0x%08X at 0x%08X (%s + 0x%X) thread=%u%s", (unsigned int)record->ExceptionCode, (unsigned int)(uintptr_t)record->ExceptionAddress,
                   slash != nullptr ? slash + 1 : path, (unsigned int)((uintptr_t)record->ExceptionAddress - (uintptr_t)module), (unsigned int)GetCurrentThreadId(), access);
        return Previous_filter != nullptr ? Previous_filter(pointers) : EXCEPTION_CONTINUE_SEARCH;
    }

    // The largest free block of the address space, in MB.
    unsigned int largest_free_block()
    {
        SIZE_T largest = 0;
        MEMORY_BASIC_INFORMATION info;
        for (const char* address = nullptr; VirtualQuery(address, &info, sizeof(info)) == sizeof(info); address = (const char*)info.BaseAddress + info.RegionSize)
        {
            if (info.State == MEM_FREE && info.RegionSize > largest)
            {
                largest = info.RegionSize;
            }
            if ((uintptr_t)info.BaseAddress + info.RegionSize < (uintptr_t)info.BaseAddress + 1)
            {
                break; // the top of the address space
            }
        }
        return (unsigned int)(largest >> 20);
    }

    void write_status(int frames, long elapsed)
    {
        game_state stack[16];
        int top = -1;
        game_handshake_copy_gameseq(stack, top);
        char states[256] = "";
        for (int i = 0; i <= top; i++)
        {
            strncat_s(states, gameseq_trace_state_name(stack[i]), _TRUNCATE);
            if (i < top)
            {
                strncat_s(states, ">", _TRUNCATE);
            }
        }
        char player_text[160] = "player=none";
        player* pp = player_get_local_player();
        if (pp != nullptr)
        {
            snprintf(player_text, sizeof(player_text), "pos=(%.1f %.1f %.1f) hp=%.0f/%d state=%d killed=%d never_die=%d", pp->pos.x, pp->pos.y, pp->pos.z, pp->hit_points, pp->max_hit_points,
                     (int)pp->cur_state, pp->is_killed() ? 1 : 0, pp->pflags.never_die ? 1 : 0);
        }
        MEMORYSTATUSEX memory;
        memory.dwLength = sizeof(memory);
        GlobalMemoryStatusEx(&memory);
        const char* mission = mission_is_active() ? mission_get_name() : nullptr;
        // va_free: the texture OOM of the single player runs is the 32-bit address space running out (S9).
        GAME_TRACE("status", "states=%s mission=%s ready=%d world_state=%d %s scripts=%d/%d npcs=%d squads=%d cutscene=%d alert=%d fps=%.1f va_free=%uMB va_largest=%uMB", states, mission != nullptr ? mission : "none",
                   mission_is_ready() ? 1 : 0, (int)World.get_world_state(), player_text, (int)Action_scripts.get_count(0), (int)Action_scripts.get_count(1), World.get_num_objects<npc>(),
                   World.get_num_objects<object_squad>(), video_cutscene_pending_or_playing() ? 1 : 0, (int)Current_alert_level, elapsed > 0 ? frames * 1000.0f / elapsed : 0.0f,
                   (unsigned int)(memory.ullAvailVirtual >> 20), largest_free_block());
    }

    bool finite3(const vector& v)
    {
        return isfinite(v.x) && isfinite(v.y) && isfinite(v.z);
    }

    bool finite_matrix(const matrix& m)
    {
        return finite3(m.rvec) && finite3(m.uvec) && finite3(m.fvec);
    }

    // The player's render inputs and the camera, one line per change: the "player invisible after the turret Demolitions
    // Master" hunt (2026-11-06). bones: -1 no instanced matrices, else the first bone with a non-finite matrix or the bone
    // count when all are finite.
    void watch_player_render()
    {
        static char last[512];
        player* pp = player_get_local_player();
        if (pp == nullptr)
        {
            return;
        }
        if (pp->char_inst != nullptr)
        {
            Game_trace_anim_watch_handle = pp->char_inst->aim_handle;
        }
        int bones = -1;
        matrix43* matrices = nullptr;
        if (pp->char_inst != nullptr && pp->char_inst->aim_handle != -1 && animlib_get_instanced_model_matrices(pp->char_inst->aim_handle, &matrices) && matrices != nullptr)
        {
            const int num = animlib_get_num_bones(pp->char_inst->aim_handle);
            bones = num;
            for (int i = 0; i < num; i++)
            {
                if (!finite_matrix(matrices[i].m_rotation) || !finite3(matrices[i].m_translation))
                {
                    bones = i;
                    break;
                }
            }
        }
        char now[512];
        // Buckets of alpha, so that a fade logs its ends and not every frame.
        snprintf(now, sizeof(now), "alpha=%.1f cam_alpha=%.1f shown=%d zoom=%d turret=0x%08X state=%d orient_ok=%d bones=%d spinebend_off=%d cam_mode=%d stack=%d cam_pos_ok=%d cam_orient_ok=%d render_pos_ok=%d target=0x%08X",
                 pp->render_alpha, pp->camera_alpha_override, pp->obj_flags._dont_use_me ? 1 : 0, (int)pp->zoom_state, pp->combat.reserved_turret, (int)pp->cur_state,
                 finite_matrix(pp->orient) ? 1 : 0, bones, pp->spinebend_data.flags.disabled ? 1 : 0, (int)Rfg_camera.mode, Rfg_camera.mode_stack_size, finite3(Rfg_camera.real_pos) ? 1 : 0,
                 finite_matrix(Rfg_camera.real_orient) ? 1 : 0, finite3(Rfg_camera.render_pos) ? 1 : 0, Rfg_camera.target_handle);
        if (strcmp(now, last) != 0)
        {
            strncpy_s(last, now, _TRUNCATE);
            GAME_TRACE("render", "player %s pos=(%.1f %.1f %.1f) cam=(%.1f %.1f %.1f)", now, pp->pos.x, pp->pos.y, pp->pos.z, Rfg_camera.real_pos.x, Rfg_camera.real_pos.y, Rfg_camera.real_pos.z);
        }
    }

    void tick()
    {
        watch_player_render();
        static bool started;
        static long last_time;
        static int last_frame;
        if (!started)
        {
            // Here and not at the option: the options are read before WinMain, which then sets its own handler.
            started = true;
            //Previous_filter = SetUnhandledExceptionFilter(crash_filter); // temporary disabled
            last_time = (long)clock();
            last_frame = Framecount;
            return;
        }
        const long now = (long)clock();
        if (now - last_time >= TICK_PERIOD)
        {
            write_status(Framecount - last_frame, now - last_time);
            last_time = now;
            last_frame = Framecount;
        }
    }

    struct main_game_trace_setup
    {
        main_game_trace_setup()
        {
            Game_trace_tick_hook = tick;
        }
    };
    main_game_trace_setup Main_game_trace_setup;
}
