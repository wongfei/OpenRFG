// Original: volition/render_lib/code/rl_stream.h
// Implementation: volition/render_lib/code/rl_stream.cpp
//
// rl_stream, a read cursor over serialized render_lib data.
#pragma once

// size 0x8
class rl_stream
{
public:
    rl_stream();
    ~rl_stream();
    void set_data_pointer(unsigned char*);
    unsigned char* get_data_pointer();
    void read_data_pointed(unsigned char*, int);
    void write_data_pointed(const unsigned char*, int);
    unsigned char* read_data_referenced();

private:
    unsigned char* m_data_p; // 0x00
    int m_data_offset; // 0x04
};
