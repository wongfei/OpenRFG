// Original: volition/rfg/code/geom/cloth_simulation_shared.cpp
//
// The cloth files' preload (stub_impl.md S3): one stream2 container per .sim_pc file (at most 64), built by the asset
// assembler or read from the preload file ("cloth_containers", "dlc_cloth_containers" for a DLC), each preloaded and
// listed in Cloth_simulation_srids.

#include "volition/rfg/code/geom/cloth_simulation_shared.h"

#include "volition/rfg/code/gamewide/asset_assembler.h"
#include "volition/rfg/code/gamewide/downloadable_content.h"
#include "volition/rfg/code/geom/cloth_simulation.h"
#include "volition/rfg/code/rfg.h"
#include "volition/stream2/code/stream2.h"
#include "volition/stream2/code/stream2_container.h"
#include "volition/vlib/code/cfile/cfile.h"

#include "keen/base/trace.hpp"

#include <stdio.h>

// The thread's message buffer (TLS +0x38 in rfg.exe, shared by the unity TU's messages).
static __declspec(thread) char Cloth_sim_message[1024];

// 0x007FBC20 (:21)
void cloth_sim_preload_all_shared(mempool_base* tmp_pool, mempool* pool, bool load_from_preload, vlib_platform plat, int dlc_id)
{
    // Magic numbers: the cloth container type and primitive type (GSP_CLOTH_SIM), and at most 64 cloth files.
    const unsigned char cloth_type = 9;
    const int max_files = 0x40;
    const bool is_temp = dlc_id != -1;
    assasm_start(is_temp ? "dlc_cloth_containers" : "cloth_containers", tmp_pool, (mempool_base*)pool, load_from_preload, nullptr, false, is_temp, true, false);
    const char* sim_ext[8] = { ".sim_pc", ".sim_pc", ".sim_pc", ".sim_pc", ".sim_pc", ".sim_pc", ".sim_pc", ".sim_pc" };
    const char* files[max_files + 1];
    int num_files = (int)cf_find_ext(files, max_files + 1, sim_ext[plat]);
    if (num_files > max_files)
    {
        sprintf(Cloth_sim_message, "Found too many cloth simulation files to preload (%d), only %d will be preloaded\n", num_files, max_files);
        keen::debug::nativePrintArgs(Cloth_sim_message);
    }
    if (num_files < 0)
    {
        num_files = 0;
    }
    else if (num_files > max_files)
    {
        num_files = max_files;
    }
    if (is_temp && !load_from_preload)
    {
        dlc_allocate_temp_container_list(max_files, &Permanent_mempool);
    }
    for (int i = 0; i < num_files; i++)
    {
        assasm_container_start(files[i], cloth_type, true, false);
        assasm_container_add(files[i], cloth_type, "Cloth simulation", false, false, false, nullptr, 0);
        assasm_container_finish(true, is_temp, false);
    }
    assasm_finish(nullptr, false, true, false);
    stream2_container* sc = Stream_containers;
    while (sc != nullptr)
    {
        if (sc->cdef->container_type == cloth_type)
        {
            const unsigned int srid = stream2_get_srid(sc);
            Cloth_simulation_srids[Num_cloth_simulation_srids] = srid;
            stream2_preload(srid);
            Num_cloth_simulation_srids++;
        }
        sc = sc->next;
        if (sc == Stream_containers)
        {
            break;
        }
    }
    if (!is_temp)
    {
        Num_cloth_simulation_srids_dlc_mark = Num_cloth_simulation_srids;
    }
}
