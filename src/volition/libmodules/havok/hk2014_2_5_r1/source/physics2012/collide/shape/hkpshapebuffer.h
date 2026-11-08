// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshapebuffer.h
//
// Havok 2014.2.5 (havok_impl.md HK3): the stack buffer getChildShape may build a child in (no PDB layout record; the
// collections reserve 0x200 bytes for it). As in Havok 2014.1 (hkpShapeBuffer.h). Hand-written (not in the hdrgen
// specs).
#pragma once

enum hkpBufferSize
{
    HK_SHAPE_BUFFER_SIZE = 512,
};

// size 0x200
struct hkpShapeBufferStorage
{
    char m_storage[HK_SHAPE_BUFFER_SIZE];

    template<typename T>
    operator T*() { return reinterpret_cast<T*>(m_storage); }
};

// HK_ALIGN_REAL(hkpShapeBufferStorage) in Havok
struct alignas(16) hkpShapeBuffer : public hkpShapeBufferStorage
{
};
