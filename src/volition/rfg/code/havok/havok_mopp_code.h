// Original: volition/rfg/code/havok/havok_mopp_code.h
// Implementation: volition/rfg/code/havok/havok_mopp_code.cpp
//
// MOPP code blobs (the precomputed bounding volume tree of a mesh shape): a header followed by the byte code, loaded
// in place.
#pragma once


class hkpMoppCode;

// size 0x20
struct havok_mopp_code_header
{
    unsigned int m_signature; // 0x00
    unsigned int m_version; // 0x04
    float m_offset_x; // 0x08
    float m_offset_y; // 0x0C
    float m_offset_z; // 0x10
    float m_scale; // 0x14
    unsigned int m_byte_code_size; // 0x18
    unsigned int m_build_type; // 0x1C
};

const hkpMoppCode* havok_mopp_code_load_in_place(const havok_mopp_code_header* p_header); // 0x008330A0 (:88)
