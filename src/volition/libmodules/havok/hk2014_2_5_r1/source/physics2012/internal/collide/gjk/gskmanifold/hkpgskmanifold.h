// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/collide/gjk/gskmanifold/hkpgskmanifold.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0); inline members from the 2014.1 SDK header (HK6).
// Hand-maintained.
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"

// A feature based manifold: up to four contact points, each a feature pair (dimA vertices of A, dimB of B) whose
// vertices index a shared vertex id array after the used contact points. Only getTotalSizeInBytes() bytes are used.
// size 0x44
struct hkpGskManifold
{
    enum
    {
        MAX_POINTS = 4,
    };

    enum
    {
        MAX_VERTICES = 16,
    };

    typedef unsigned short VertexId;

    // size 0x8
    struct ContactPoint
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(ContactPoint);
        void zeroVerts() { m_allVerts = 0; } // 0x00E81560

        unsigned char m_dimA; // 0x00
        unsigned char m_dimB; // 0x01
        unsigned short m_id; // 0x02 (contact point id, 0xFFFF for none)
        union
        {
            unsigned char m_vert[4]; // 0x04 (vertex index * sizeof(hkVector4f), A's first)
            unsigned int m_allVerts; // 0x04
        };
    };

    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpGskManifold);

    // 0x00E83EB0
    unsigned short getVertexId(const ContactPoint& cp, int vertNr) const
    {
        // the byte offset: vert / sizeof(hkVector4f) * sizeof(VertexId)
        return *reinterpret_cast<const unsigned short*>(reinterpret_cast<const char*>(getVertexIds()) + (unsigned char)(cp.m_vert[vertNr] >> 3));
    }
    void init() { *reinterpret_cast<unsigned int*>(this) = 0; } // 0x00DB4EA0
    hkpGskManifold() { init(); } // 0x00DD0DD0
    // 0x00DC3F30
    int getTotalSizeInBytes() const
    {
        return 4 + m_numContactPoints * int(sizeof(ContactPoint)) + (m_numVertsA + m_numVertsB) * int(sizeof(VertexId));
    }
    // 0x00DC3F50
    const unsigned short* getVertexIds() const
    {
        return reinterpret_cast<const unsigned short*>(reinterpret_cast<const char*>(&m_contactPoints) + m_numContactPoints * sizeof(ContactPoint));
    }
    // 0x00E81570
    unsigned short* getVertexIds()
    {
        return reinterpret_cast<unsigned short*>(reinterpret_cast<char*>(&m_contactPoints) + m_numContactPoints * sizeof(ContactPoint));
    }

    unsigned char m_numVertsA; // 0x00
    unsigned char m_numVertsB; // 0x01
    unsigned char m_numContactPoints; // 0x02
    ContactPoint m_contactPoints[4]; // 0x04
    unsigned char m_padding[32]; // 0x24 (room for the vertex ids)
};
