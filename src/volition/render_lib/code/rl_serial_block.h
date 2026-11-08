// Original: volition/render_lib/code/rl_serial_block.h
// Implementation: volition/render_lib/code/rl_serial_block.cpp
//
// rl_serial_block, a serialized block split into cloned and referenced data streams.
#pragma once

#include "volition/render_lib/code/rl_stream.h"

// size 0x10
class rl_serial_block
{
public:
    rl_serial_block();
    ~rl_serial_block();
    void init(unsigned char*, unsigned char*);
    rl_stream& get_cloned_stream();
    rl_stream& get_referenced_stream();

private:
    rl_stream m_cloned_data; // 0x00
    rl_stream m_referenced_data; // 0x08
};
