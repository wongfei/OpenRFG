// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/internal/geometryprocessing/abstractmesh/hkgpabstractmesh.h
//
// Hand-written (havok_impl.md HK3): the abstract mesh templates (hkgpAbstractMesh.h/.inl of Havok 2014.1, release
// build): pool allocated doubly linked lists of vertices and triangles. New items go to the front of their list.
// Checked against the hkgpConvexHullImpl instances (List::allocate 0x00FFC010/0x00FFC080, List::reset
// 0x00FE1E70/0x00FE1EE0, hkgpAbstractMesh::reset 0x00FE3230).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hkreferencedobject.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/internal/geometryprocessing/hkgeometryprocessing.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/internal/geometryprocessing/topology/hkgptopology.h"

// size 0x1
struct hkgpAbstractMeshDefinitions
{
    typedef hkgpTopology::Uid Uid;

    // A pool allocated doubly linked list.
    template<typename T, typename BASE, typename ALLOCATOR = hkGeometryProcessing::PoolAllocator<T, 32>>
    struct List
    {
        // The list links; the base is empty (offset 0 for m_next).
        struct Item : public BASE
        {
            T* next() const { return m_next; }
            T*& next() { return m_next; }
            T* prev() const { return m_prev; }
            T*& prev() { return m_prev; }

            Item() : BASE() {}
            // the links are not copied
            Item& operator=(const Item&) { return *this; }

        protected:
            Item(const Item& other) : BASE(other) {}

        private:
            T* m_next; // 0x00
            T* m_prev; // 0x04
        };

        List() : m_used(nullptr), m_numUsed(0) {}
        ~List() { reset(); }

        void reset()
        {
            if (m_allocator.needRelease())
            {
                while (m_used)
                {
                    release(m_used);
                }
            }
            else
            {
                m_allocator.clear();
                m_used = nullptr;
                m_numUsed = 0;
            }
        }

        T* allocate() { return link(m_allocator.allocate()); }
        T* allocate(const T& value) { return link(m_allocator.allocate(value)); }

        void release(T* item)
        {
            T* n = item->next();
            T* p = item->prev();
            if (n)
            {
                n->prev() = p;
            }
            if (p)
            {
                p->next() = n;
            }
            else
            {
                m_used = n;
            }
            m_numUsed--;
            m_allocator.release(item);
        }

        T* getFirst() const { return m_used; }
        int getSize() const { return m_numUsed; }

    private:
        T* link(T* item)
        {
            if (item)
            {
                item->prev() = nullptr;
                item->next() = m_used;
                if (m_used)
                {
                    m_used->prev() = item;
                }
                m_used = item;
                m_numUsed++;
            }
            return item;
        }

        List(const List&);
        void operator=(const List&);

    public:
        ALLOCATOR m_allocator; // 0x00
        T* m_used; // 0x04
        int m_numUsed; // 0x08
        unsigned int m_uid; // 0x0C (not initialized)
    };

    template<typename VERTEX, typename BASE>
    struct Vertex : public List<VERTEX, BASE>::Item
    {
        typedef typename List<VERTEX, BASE>::Item tBase;
    };

    template<typename TRIANGLE, typename BASE, typename VERTEX>
    struct Triangle : public List<TRIANGLE, BASE>::Item
    {
        typedef typename List<TRIANGLE, BASE>::Item tBase;

        void setVertices(const VERTEX& a, const VERTEX& b, const VERTEX& c)
        {
            m_vertices[0] = a;
            m_vertices[1] = b;
            m_vertices[2] = c;
        }
        VERTEX vertex(int i) const { return m_vertices[i]; }
        VERTEX& vertex(int i) { return m_vertices[i]; }
        Uid link(int i) const { return m_links[i]; }
        Uid& link(int i) { return m_links[i]; }

        VERTEX m_vertices[3]; // 0x08
        Uid m_links[3]; // 0x14
    };
};

// size 0x28
template<typename EDGE, typename VERTEX, typename TRIANGLE, typename ALLOCATOR = hkContainerHeapAllocator>
class hkgpAbstractMesh : public hkReferencedObject
{
public:
    typedef hkGeometryProcessing::PoolAllocator<VERTEX, 32, ALLOCATOR> VertexPoolAllocator;
    typedef hkGeometryProcessing::PoolAllocator<TRIANGLE, 32, ALLOCATOR> TrianglePoolAllocator;
    typedef hkgpAbstractMeshDefinitions::List<VERTEX, typename VERTEX::tBase, VertexPoolAllocator> VertexList;
    typedef hkgpAbstractMeshDefinitions::List<TRIANGLE, typename VERTEX::tBase, TrianglePoolAllocator> TriangleList;

    // the triangles then the vertices are freed (member destructors)
    virtual ~hkgpAbstractMesh() {}

    // vtbl+0x08
    virtual void reset()
    {
        m_vertices.reset();
        m_triangles.reset();
    }

    VertexList m_vertices; // 0x08
    TriangleList m_triangles; // 0x18
};
