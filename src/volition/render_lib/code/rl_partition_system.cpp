// Original: volition/render_lib/code/rl_partition_system.cpp
//
// rl_partition_system: the base of the spatial partitions (rl_quadtree), owning the root sector. The header-inline
// members of rl_partition_system.h are defined here as well.

#include "volition/render_lib/code/rl_partition_system.h"

#include "volition/render_lib/code/rl_partition_sector.h"

// Never set in the binary: moving an entity does not re-insert it into its partition system (rl_scene_entity), the
// owners call update_partition_system or the partition system themselves.
static bool RL_auto_partition_update_enabled = false; // 0x01B7BCED

// 0x004C4470 (:13)
rl_partition_system::rl_partition_system()
{
    m_root_sector_p = nullptr;
}

// 0x004C4480 (:18)
rl_partition_system::~rl_partition_system()
{
    if (m_root_sector_p != nullptr)
    {
        m_root_sector_p->release();
    }
}

// 0x004C44A0 (:25)
bool RL_auto_partition_update()
{
    return RL_auto_partition_update_enabled;
}

// 0x004B86E0 (.h:52)
rl_partition_sector* rl_partition_system::get_root_sector() const
{
    return m_root_sector_p;
}

// 0x004B86F0 (.h:57)
void rl_partition_system::set_root_sector(rl_partition_sector* root_sector_p)
{
    m_root_sector_p = root_sector_p;
}
