// Original: volition/rfg/code/game_render_terrain.cpp
//
// Partial (CPU half): game_render_terrain_load_lowlod (terrain_load_terrain parses the low LOD patches through it) and
// the level init/shutdown (both empty in the retail build). The renderables of the subzones, stitch pieces, roads and
// low LOD patches and the landmark LOD streaming are in game_render_terrain_render.cpp (rfg_render);
// game_render_terrain_debug (2D text) is not ported.

#include "volition/rfg/code/game_render_terrain.h"

#include "volition/render_lib/code/rl_aabb.h"

#include "keen/base/string.hpp"

#include <math.h>

// 0x005DA720 (:379)
terrain_lowlod_patch* game_render_terrain_load_lowlod(unsigned char* cpu_data_p, int& cpu_offset, unsigned char* gpu_data_p, int& gpu_offset, const char* pFilename)
{
    // A territory cell has 3x3 low LOD patches, each one mesh.
    cpu_offset += (cpu_offset & 3) == 0 ? 0 : 4 - (cpu_offset & 3);
    terrain_lowlod_patch* patches = (terrain_lowlod_patch*)(cpu_data_p + cpu_offset);
    cpu_offset += 9 * sizeof(terrain_lowlod_patch);
    for (int i = 0; i < 9; i++)
    {
        terrain_lowlod_patch& patch = patches[i];
        LoadedMeshInfo mesh_info(nullptr, nullptr);
        keen::copyString(mesh_info.name, pFilename, sizeof(mesh_info.name));
        patch.m_mesh_data_p.from_pointer(rl_mesh_data_load_validated(cpu_data_p, gpu_data_p, cpu_offset, gpu_offset, &mesh_info));
        if (patch.m_mesh_data_p.to_pointer() == nullptr)
        {
            return nullptr;
        }
        patch.m_mesh_p.from_pointer(rl_mesh::create());
        rl_mesh* mesh = patch.m_mesh_p.to_pointer();
        if (mesh != nullptr)
        {
            mesh->init(patch.m_mesh_data_p.to_pointer(), mesh_info, false);
            mesh->set_local_aabb(rl_aabb(patch.m_aabb_min, patch.m_aabb_max));
        }
    }
    return patches;
}

// 0x005D0C60 (:340)
void game_render_terrain_level_init()
{
}

// 0x005D0C70 (:349)
void game_render_terrain_level_shutdown()
{
}
