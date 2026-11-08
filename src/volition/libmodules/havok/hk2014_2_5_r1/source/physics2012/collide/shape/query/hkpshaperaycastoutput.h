// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/query/hkpshaperaycastoutput.h
//
// Havok 2014.2.5 (havok_impl.md HK3): the PDB declaration with the inline code of Havok 2014.1
// (hkpShapeRayCastOutput.inl), and hkpShapeRayBundleCastOutput. Hand-maintained (nobuild in the hdrgen specs).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/query/hkpshaperaycastcollectoroutput.h"

// size 0x50
struct alignas(16) hkpShapeRayCastOutput : public hkpShapeRayCastCollectorOutput
{
    enum
    {
        MAX_HIERARCHY_DEPTH = 8,
    };

    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpShapeRayCastOutput);

    // 0x007174F0 (hkpshaperaycastoutput.inl:23)
    hkpShapeRayCastOutput()
    {
        _reset();
    }

    // 0x00D751B0
    void changeLevel(int delta)
    {
        m_shapeKeyIndex += delta;
    }

    // 0x00717510 (hkpshaperaycastoutput.inl:34)
    void setKey(unsigned int key)
    {
        m_shapeKeys[m_shapeKeyIndex] = key;
    }

    // 0x00E72AC0
    int getLevel() const
    {
        return m_shapeKeyIndex;
    }

    // 0x0080D5F0 (hkpshaperaycastoutput.inl:16)
    void reset()
    {
        hkpShapeRayCastCollectorOutput::reset();
        _reset();
    }

public:
    // 0x007174E0 (hkpshaperaycastoutput.inl:10)
    void _reset()
    {
        m_shapeKeyIndex = 0;
        m_shapeKeys[0] = 0xFFFFFFFF; // HK_INVALID_SHAPE_KEY
    }

public:
    unsigned int m_shapeKeys[8]; // 0x20
public:
    int m_shapeKeyIndex; // 0x40
};

// size 0x140
struct hkpShapeRayBundleCastOutput
{
    hkpShapeRayCastOutput m_outputs[4]; // 0x00

    void reset()
    {
        m_outputs[0].reset();
        m_outputs[1].reset();
        m_outputs[2].reset();
        m_outputs[3].reset();
    }
};
