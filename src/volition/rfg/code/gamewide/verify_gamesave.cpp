// Original: volition/rfg/code/gamewide/verify_gamesave.cpp
//
// See verify_gamesave.h. The state runs over the state it was pushed on: it borrows the gameplay GPU pool for the copy of
// the save, checks the MD5 at its end and starts loading the territory from the world's load buffer.
//
// Original quirk kept: the save is copied into stored_world_state[cur_world_state] and only then does platform_load make
// WSM_DEFAULT the current state. After a mission checkpoint (cur_world_state WSM_CHECKPOINT) the file goes to the checkpoint
// buffer and the load runs from the default one, the last state saved in memory. rfg.exe's menu refuses a load during a
// mission only with a warning box, so this can be reached from the pause menu.

#include "volition/rfg/code/gamewide/verify_gamesave.h"

#include "keen/base/trace.hpp"
#include "volition/rfg/code/gameplay/difficulty.h"
#include "volition/rfg/code/gamewide/game_save.h"
#include "volition/rfg/code/gamewide/game_trace.h"
#include "volition/rfg/code/gamewide/gameseq.h"
#include "volition/rfg/code/interface/ui_document_manager.h"
#include "volition/rfg/code/interface/ui_message_box.h"
#include "volition/rfg/code/level/world.h"
#include "volition/rfg/code/util/special_allocators.h"
#include "volition/vlib/code/math/md5.h"
#include "volition/vlib/code/memory/mempool_base.h"

#include <string.h>

// Magic number: the room of a save (RfgSaveFile::data).
static const unsigned int Verify_buffer_size = 0x280010;
// Magic number: the MD5 at the end of a save file.
static const int Verify_md5_size = 0x10;

// ---- file-private types (rfg.pdb)
enum verify_states
{
    VERIFY_STATE_INVALID = -1,
    VERIFY_STATE_GETTING_MEMPOOL = 0,
    VERIFY_STATE_WAIT_FOR_ANOTHER_OPERATION_TO_FINISH = 1,
    VERIFY_STATE_READING_SAVE = 2,
    VERIFY_STATE_COMPUTING_CHECKSUM = 3,
    VERIFY_STATE_LOADING_WORLD_BUF = 4,
    VERIFY_STATE_CLEARING_MEMPOOL = 5,
    VERIFY_STATE_DONE = 6,
};

// ---- file-static globals
static int Verify_input_id = -1; // 0x0165E0B8
static verify_states Verify_state = VERIFY_STATE_INVALID; // 0x0165E0BC
static bool Reset_destruction_on_load; // 0x02533945
static bool Exiting; // 0x02533946
static game_save_info* Pending_save; // 0x0253394C
static mempool_base* Verify_mempool; // 0x02533950
static unsigned char* Verify_mem; // 0x02533954
static int Verify_size; // 0x02533958
static unsigned int Verify_mempool_mark; // 0x0253395C

// ---- internal functions

// 0x007C93B0 (:42)
static void verify_gamesave_enter(game_state)
{
    Verify_input_id = ui_register_for_processed_input(IRT_UNLIMITED_INPUT, false);
    ui_show_loading_screen(false, "MENU_LOADING");
    salloc_gameplay_gpu_lock_async();
    Verify_state = VERIFY_STATE_GETTING_MEMPOOL;
    Exiting = false;
}

// The result of a game_save_load, in the state's reading states.
static bool verify_gamesave_start_load(int result, const char* state_name)
{
    // Magic numbers: rfg.exe's codes for a read in progress and for another operation running. PC's game_save_load
    // finishes at once and only answers 0, -10 and -11.
    if (result == -2)
    {
        Verify_state = VERIFY_STATE_READING_SAVE;
        return true;
    }
    if (result == -12 && Verify_state == VERIFY_STATE_GETTING_MEMPOOL)
    {
        Verify_state = VERIFY_STATE_WAIT_FOR_ANOTHER_OPERATION_TO_FINISH;
        return true;
    }
    if (result != 0)
    {
        keen::debug::nativePrintArgs("verify_gamesave_process: %s: game_save_load failed! result: %d\n", state_name, result);
    }
    return result == 0;
}

// 0x007EFFF0 (:55)
static void verify_gamesave_process()
{
    switch (Verify_state)
    {
    case VERIFY_STATE_GETTING_MEMPOOL:
        Verify_mempool = salloc_gameplay_gpu_lock_async_poll();
        if (Verify_mempool != nullptr)
        {
            Verify_mempool_mark = Verify_mempool->mark();
            Verify_mem = (unsigned char*)Verify_mempool->alloc(Verify_buffer_size, 0);
            if (Verify_mem == nullptr)
            {
                gameseq_pop_state();
                return;
            }
            // Original: a failure stays in this state, which takes the pool again.
            verify_gamesave_start_load(game_save_load(Pending_save, Verify_mem, verify_gamesave_loaded), "VERIFY_STATE_GETTING_MEMPOOL");
        }
        break;
    case VERIFY_STATE_WAIT_FOR_ANOTHER_OPERATION_TO_FINISH:
        if (game_save_op_completed())
        {
            verify_gamesave_start_load(game_save_load(Pending_save, Verify_mem, verify_gamesave_loaded), "VERIFY_STATE_WAIT_FOR_ANOTHER_OPERATION_TO_FINISH");
        }
        break;
    case VERIFY_STATE_READING_SAVE:
        if (game_save_op_completed())
        {
            Verify_state = VERIFY_STATE_DONE;
        }
        break;
    case VERIFY_STATE_COMPUTING_CHECKSUM:
    {
        md5_value stored;
        memcpy(&stored, Verify_mem + Verify_size - Verify_md5_size, sizeof(stored));
        Verify_size -= Verify_md5_size;
        const md5_value computed = console_md5_mem(Verify_mem, Verify_size);
        GAME_TRACE("save", "verify: %d bytes, md5 %s", Verify_size, computed != stored ? "MISMATCH" : "ok");
        if (computed != stored)
        {
            Verify_state = VERIFY_STATE_INVALID;
            ui_add_msgbox(MBT_OK, "MENU_LOAD_FAILED_TITLE", "PLT_LOAD_CORRUPT", nullptr, false, true, nullptr, nullptr, false);
            gameseq_pop_state();
            return;
        }
        Verify_state = VERIFY_STATE_LOADING_WORLD_BUF;
        break;
    }
    case VERIFY_STATE_LOADING_WORLD_BUF:
    {
        void* load_buf = World.get_load_buf();
        World.set_cur_size(Verify_size);
        memcpy(load_buf, Verify_mem, Verify_size);
        Verify_state = VERIFY_STATE_DONE;
        break;
    }
    case VERIFY_STATE_CLEARING_MEMPOOL:
        // Original: nothing enters this state.
        if (!Exiting)
        {
            Verify_mempool->restore_to_mark(Verify_mempool_mark);
            salloc_gameplay_gpu_unlock_async();
            Exiting = true;
        }
        if (Verify_mempool != nullptr && salloc_gameplay_gpu_unlock_async_poll())
        {
            Verify_mempool = nullptr;
            Verify_state = VERIFY_STATE_DONE;
        }
        break;
    case VERIFY_STATE_DONE:
        difficulty_set_level_reload_overwrite(DIFFICULTY_LEVEL_INVALID);
        World.platform_load(Pending_save, true);
        World.set_pending_territory_load("", false, false, false, Pending_save->dlc_id, Reset_destruction_on_load);
        Verify_state = VERIFY_STATE_INVALID;
        break;
    default:
        break;
    }
}

// 0x007C93F0 (:215)
static void verify_gamesave_exit(game_state)
{
    // Original: tests against 0 here, where its "no id" is -1.
    if (Verify_input_id != 0)
    {
        ui_relinquish_input_id(Verify_input_id);
        Verify_input_id = -1;
        ui_wait_for_button_ups();
    }
    if (ui_loading_screen_is_open())
    {
        ui_remove_loading_screen();
    }
    Verify_state = VERIFY_STATE_INVALID;
}

// 0x007C9430 (:235): the pool goes back before the state may be left. Original: when the buffer allocation failed the
// pool is still there; a pop before the pool arrived reads a null pool.
static bool verify_gamesave_exit_notify(game_state)
{
    if (!Exiting)
    {
        Verify_mempool->restore_to_mark(Verify_mempool_mark);
        salloc_gameplay_gpu_unlock_async();
        Exiting = true;
    }
    if (Verify_mempool != nullptr && salloc_gameplay_gpu_unlock_async_poll())
    {
        Verify_mempool = nullptr;
        return true;
    }
    return false;
}

// ---- external interface

// 0x007F02B0 (:267)
void verify_gamesave_system_init()
{
    state_info info;
    info.user_flags = 0;
    info.hide_cursor = false;
    info.handshake = nullptr;
    info.render = nullptr;
    info.enter = verify_gamesave_enter;
    info.exit = verify_gamesave_exit;
    info.exit_notify = verify_gamesave_exit_notify;
    info.process = verify_gamesave_process;
    info.transparent = true;
    info.pause_beneath = false;
    gameseq_register_state(GS_VERIFY_SAVEGAME, info);
}

// 0x007E7650 (:281)
void verify_gamesave_start(game_save_info* save, bool reset_destruction)
{
    Pending_save = save;
    Reset_destruction_on_load = reset_destruction;
    gameseq_push_state(GS_VERIFY_SAVEGAME, true, false);
}

// 0x007E7680 (:292): the completion callback of game_save_load
void verify_gamesave_loaded(void* buf, int size)
{
    if (buf == nullptr)
    {
        if (ui_loading_screen_is_open())
        {
            ui_remove_loading_screen();
        }
        gameseq_pop_state();
        return;
    }
    Verify_size = size;
    Verify_state = VERIFY_STATE_COMPUTING_CHECKSUM;
}
