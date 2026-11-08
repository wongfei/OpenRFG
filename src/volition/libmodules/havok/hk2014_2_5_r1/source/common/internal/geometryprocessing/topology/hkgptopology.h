// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/internal/geometryprocessing/topology/hkgptopology.h
//
// Hand-written (havok_impl.md HK3): the half-edge walking helpers of hkgpTopology (hkgpTopology.h/.inl of Havok
// 2014.1, packed UIDs: a triangle pointer with the edge index in its low two bits). Only what the convex hull uses.
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"

// size 0x1
class hkgpTopology
{
public:
    // A triangle / edge index pair packed in one word.
    typedef unsigned long Uid;

    static Uid nullUid() { return 0; }
    static Uid packUid(void* t, unsigned int i) { return (Uid)t + (Uid)i; }
    template<typename T>
    static void unpackUid(Uid uid, T*& t, unsigned int& i)
    {
        i = (unsigned int)(uid & 3);
        t = (T*)(uid & ~(Uid)3);
    }

    // An edge of a triangle (EDGE is the derived edge type, TRIANGLE has link(i) and vertex(i)).
    template<typename EDGE, typename VERTEX, typename TRIANGLE, typename VERTEX_MEMBER = VERTEX*>
    struct Edge
    {
        typedef VERTEX tVertex;
        typedef VERTEX_MEMBER tMVertex;
        typedef TRIANGLE tTriangle;

        // i + 1 mod 3, i + 2 mod 3
        static unsigned int ip1mod3(unsigned int i) { return (0x09 >> (i << 1)) & 3; }
        static unsigned int ip2mod3(unsigned int i) { return (0x12 >> (i << 1)) & 3; }

        Edge() {}
        Edge(tTriangle* t, unsigned int i) : m_triangle(t), m_index(i) {}
        Edge(Uid id) { unpackUid(id, m_triangle, m_index); }

        EDGE next() const { return EDGE(m_triangle, ip1mod3(m_index)); }
        EDGE prev() const { return EDGE(m_triangle, ip2mod3(m_index)); }
        EDGE link() const { return EDGE(m_triangle->link(m_index)); }
        TRIANGLE* triangle() const { return m_triangle; }
        int index() const { return (int)m_index; }
        Uid uid() const { return packUid(m_triangle, m_index); }
        tMVertex& start() const { return m_triangle->vertex(m_index); }
        tMVertex& end() const { return m_triangle->vertex(ip1mod3(m_index)); }
        tMVertex& apex() const { return m_triangle->vertex(ip2mod3(m_index)); }
        bool isValid() const { return m_triangle != nullptr; }
        bool isNaked() const { return !link().isValid(); }

        tTriangle* m_triangle; // 0x00
        unsigned int m_index; // 0x04
    };
};
