// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/hkpgskcache.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0); inline members of the 2014.1 SDK added by hand
// (HK6). Hand-maintained.
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"

class hkTransformf;
class hkpConvexShape;
class hkpTriangleShape;

// size 0xC
class hkpGskCache
{
public:
    enum MaxDim
    {
        MAX_DIM_MAX = 15,
    };

    enum GskFlagValues
    {
        GSK_FLAGS_SHAPE_A_IS_CYLINDER = 1,
        GSK_FLAGS_SHAPE_B_IS_CYLINDER = 2,
        GSK_FLAGS_REPLACE_SHAPE_A_WITH_CAPSULE = 4,
        GSK_FLAGS_REPLACE_SHAPE_B_WITH_CAPSULE = 8,
        GSK_FLAGS_CYLINDER_AGENT_FLAGS = 15,
        GSK_FLAGS_DISABLE_CONTACT_TIMS = 16,
        GSK_FLAGS_CONVEX_LIST_IN_GSK_MODE = 32,
        GSK_FLAGS_PROCESS_FUNCTION_CALLED = 64,
        GSK_FLAGS_ALLOW_QUICKER_CONTACT_POINT_RECREATION = 128,
    };

    typedef unsigned char GskFlags;

    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpGskCache);
    void init(const hkpConvexShape* shapeA, const hkpConvexShape* shapeB, const hkTransformf& aTb); // 0x00E83FD0
    void initTriangle(const hkpConvexShape* shapeA, const hkpTriangleShape* shapeB, const hkTransformf& btoa); // 0x00E841A0
    // 0x00D79630
    void flip()
    {
        unsigned char h;
        h = m_maxDimA;
        m_maxDimA = m_maxDimB;
        m_maxDimB = h;
        h = m_dimA;
        m_dimA = m_dimB;
        m_dimB = h;
        int last = m_dimA + m_dimB - 1;
        // exchange the first and the last vertex: enough except for the edge-edge case
        unsigned short k = m_vertices[0];
        m_vertices[0] = m_vertices[last];
        m_vertices[last] = k;
        if ((m_dimA & m_dimB) == 2) // edge-edge: 0 and 3 are swapped already, now swap 1 and 2
        {
            k = m_vertices[1];
            m_vertices[1] = m_vertices[2];
            m_vertices[2] = k;
        }
    }

    unsigned short m_vertices[4]; // 0x00
    unsigned char m_dimA; // 0x08
    unsigned char m_dimB; // 0x09
    unsigned char m_maxDimA : 4; // 0x0A:0
    unsigned char m_maxDimB : 4; // 0x0A:4
    unsigned char m_gskFlags; // 0x0B
};

// size 0x10
class alignas(16) hkGskCache16 : public hkpGskCache
{
public:
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkGskCache16);
    // 0x00E848E0
    void set(const hkGskCache16& other)
    {
        *this = other;
    }

};
