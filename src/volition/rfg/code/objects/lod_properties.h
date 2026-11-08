// Original: volition/rfg/code/objects/lod_properties.h
// Implementation: volition/rfg/code/objects/lod_properties.cpp
//
// LOD property sets (LOD table): a named LOD off distance, referenced by gameplay properties.
#pragma once


class mempool_base;

// size 0x8
struct LOD_properties
{
    LOD_properties(); // 0x00A2C690 (.h:19)
    void clear(); // 0x00A05DA0 (.h:25)

    char* m_name; // 0x00
    float m_lod_off_dist; // 0x04
};

void lpm_system_init(mempool_base* pool); // 0x00A8A880 (:144)
const LOD_properties* lpm_get_properties(const char* property_entry_name); // 0x00A5E6B0 (:172)
const LOD_properties* lpm_get_default_properties(); // 0x00A12FC0 (:192)
