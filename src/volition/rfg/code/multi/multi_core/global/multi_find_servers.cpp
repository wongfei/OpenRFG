// Original: volition/rfg/code/multi/multi_core/global/multi_find_servers.cpp
//
// Found servers: the LAN search results (kept by the multiplayer menus) and the server pool.
// Part of the multi unity TU (rfg_multi_unity.obj, every multi/ .cpp in path order): the file statics are shared
// by the whole tree, and a shared one is placed in its first user (tools/hdrgen/specs/mp0_stubs.py).

#include "volition/rfg/code/multi/multi_core/include/multi_find_servers.h"
#include "volition/rfg/code/interface/multi_player_menus/multi_player_menu_system.h"
#include "volition/rfg/code/multi/multi_core/include/multi_pool.inl"

// ---- file-static globals
// rfg.exe defines it here (:89) and multi.cpp, earlier in the unity TU, reads it through an extern declaration; with
// the MSVC extension that allows that (a static after an extern) unavailable, it has external linkage here.
multi_pool<multi_server_info, 64> Server_pool; // 0x01691CD0 (:89)

// 0x0088A0A0
multi_server_info::multi_server_info()
{
}

// 0x0088A0C0
multi_server_info::~multi_server_info()
{
}

// ---- internal functions

// 0x009AA860 (:133). LTCG: sp in EAX.
static void server_delete(multi_server_info* sp)
{
    Server_pool.free(sp);
}

// 0x0094B6D0 (:173): a LAN search result (none while searching)
multi_server_info* multi_fs_server_info(unsigned int index)
{
    if (index < s_lan_num_search_results && !s_lan_search_running)
    {
        return &s_lan_server_infos[index];
    }
    return nullptr;
}

// 0x009FC3C0 (:194)
void multi_fs_init(mempool_base& pool)
{
    Server_pool.init(pool);
}

// 0x009AA870 (:200)
void multi_fs_reset_servers()
{
    Server_pool.thread_access.enter();
    multi_server_info* sp = Server_pool.get_first(0);
    while (sp != nullptr)
    {
        multi_server_info* const next = Server_pool.get_next(sp);
        server_delete(sp);
        sp = next;
    }
    Server_pool.thread_access.leave();
}
