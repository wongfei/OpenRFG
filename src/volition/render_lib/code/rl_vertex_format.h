// Original: volition/render_lib/code/rl_vertex_format.h
// Implementation: volition/render_lib/code/rl_vertex_format.cpp
//
// rl_vertex_format, render_lib vertex format descriptions and their keen VertexFormat.
#pragma once

#include "volition/render_lib/code/rl_device.h"

// size 0x10
class rl_vertex_stream_info
{
public:
    rl_vertex_stream_info();

    rl_dev_vertex_buffer m_buffer; // 0x00
    unsigned int m_byte_offset; // 0x04
    unsigned int m_stride; // 0x08
    unsigned int m_instancing_frequency; // 0x0C
};

// size 0x4
struct rl_vertex_format
{
    rl_vertex_format(void* const);
    rl_vertex_format();

    void* m_internal_format_p; // 0x00
};
