// Original: volition/render_lib/code/rl_partition_sector_entry.h
// Implementation: volition/render_lib/code/rl_partition_sector_entry.cpp
//
// rl_partition_sector_entry, the link of a scene entity into a sector.
#pragma once


class rl_partition_sector;
class rl_scene_entity;

// size 0x10
struct rl_partition_sector_entry
{
    rl_partition_sector* m_sector_p; // 0x00
    rl_scene_entity* m_entity_p; // 0x04
    rl_partition_sector_entry* next; // 0x08
    rl_partition_sector_entry* prev; // 0x0C
};
