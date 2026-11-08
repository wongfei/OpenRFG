// Port-only (no rfg.exe counterpart): the file statics of the multi/ files not ported yet that the ported multi_game
// files use (multiplayer.md MP4). rfg.exe defines them in their own files, in path order between multi_session.cpp and
// their first users; this file stands in for those files at that point of the unity TU (rfg_multi_unity.cpp). Each
// goes away when its file is ported. The file-static functions of unported files called by ported ones are stand-ins
// here (or in rfg_multi_unity_pending.inl when only later files call them).
#pragma once

#include "volition/rfg/code/multi/multi_game/multi_game_announcer.h"
#include "volition/rfg/code/multi/multi_game/multi_game_lobby.h"
#include "volition/rfg/code/multi/multi_game/multi_game_object_backpack.h"
#include "volition/rfg/code/multi/multi_game/multi_game_stats.h"
#include "volition/rfg/code/multi/multi_game/multi_game_xp.h"
#include "volition/rfg/code/multi/multi_core/include/multi_connection.h"
#include "volition/rfg/code/multi/multi_core/include/multi_core_objects.h"
#include "volition/rfg/code/objects/humans/human_voice/human_voice.h"
#include "volition/rfg/code/scripting/dialogue_manager.h"
#include "volition/vlib/code/memory/object_pool.h"
#include "volition/vlib/code/util/hash_table.h"
#include "volition/vlib/code/memory/mempool_base.h"
#include "volition/vlib/code/parse/xml_table_tools.h"

class multi_game_sim_event;
struct multi_game_playlist_entry;
struct remote_object;

// ---- multi_game_announcer.cpp (ported, SFX.md SX6; included after multi_game.cpp, which inlines its process step)
static void mga_play_line(multi_announcer_lines line); // 0x0094EED0 (:190)

// ---- multi_game_player.cpp
// The sim event queues by type (multi_game_sim_event.cpp's only users so far; MP5).
static multi_game_sim_event* Event_queue[6]; // 0x02FE5BFC
