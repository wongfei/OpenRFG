// Original: volition/render_lib/code/rl_vertex_formats.hpp
// Implementation: volition/render_lib/code/rl_vertex_formats.cpp
//
// The table of render_lib vertex formats and their vertex layouts (rl_mesh_pixlit_nmap_ca_vertex; placement inferred).
#pragma once

#include "volition/render_lib/code/rl_vertex_format_defines.h"
#include "volition/vlib/code/math/vector3.h"

namespace keen
{
    struct GraphicsSystem;
    struct VertexAttributeDescription;
    struct VertexFormat;
}

// size 0x20
struct rl_mesh_pixlit_nmap_ca_vertex
{
    rl_mesh_pixlit_nmap_ca_vertex(); // 0x006E5400

    vector pos; // 0x00
    rl_compressed_vector normal; // 0x0C
    rl_compressed_vector tangent; // 0x10
    unsigned char vweights[4]; // 0x14
    unsigned char vindices[4]; // 0x18
    unsigned int uvs; // 0x1C
};

const keen::VertexFormat* rl_create_keen_vertex_format(keen::GraphicsSystem* pGraphicsSystem, keen::VertexAttributeDescription* pAttributesDesc, unsigned int numAttributes); // 0x004D4700 (:9)
const keen::VertexFormat* rl_create_combined_keen_vertex_format(keen::GraphicsSystem* pGraphicsSystem, keen::VertexAttributeDescription* pAttributesDescA, unsigned int numAttributesA, keen::VertexAttributeDescription* pAttributesDescB, unsigned int numAttributesB); // 0x004D4710 (:14)
bool rl_create_vertex_formats(keen::GraphicsSystem* graphics_system_p, const keen::VertexFormat** vertex_formats_p, unsigned int buffer_size); // 0x00500500 (:29)
