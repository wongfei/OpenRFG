// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/internal/geometryprocessing/hkgeometryprocessing.h
//
// Hand-written (havok_impl.md HK3): the part of hkGeometryProcessing that the convex hull uses, the pool allocator
// (hkGeometryProcessing.h/.inl of Havok 2014.1; the PDB only has the instances for hkgpConvexHullImpl::Vertex and
// ::Triangle, checked against allocateNewPool 0x00FE0530/0x00FE05A0, releasePool 0x00FDE4E0 and the inlined
// allocate/release in hkgpConvexHullImpl::build and ::clearHorizon).
// The pools are blocks of N items threaded on a free list. A released item is not put back on the free list: its
// pool only counts down and is freed when its last item is released.
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.h"

#include <new>

// size 0x1
struct hkGeometryProcessing
{
    // Pool allocator policy. Objects still allocated when a pool is destroyed are not destructed.
    template<typename T, const int N, typename ALLOCATOR = hkContainerHeapAllocator>
    struct PoolAllocator
    {
        // size: sizeof(T) rounded up to 16, plus 16 (the pool pointer)
        struct Item
        {
            alignas(16) unsigned char m_data[sizeof(T)];
            alignas(16) void* m_pool;
        };

        // size N * sizeof(Item) + 0x10
        struct Pool
        {
            Item m_items[N];
            Item* m_free;
            Pool* m_links[2];
            int m_used;
        };

        PoolAllocator() : m_firstPool(nullptr) {}
        ~PoolAllocator() { clear(); }

        T* allocate()
        {
            Item* item = allocateItem();
            return item ? new (item) T() : nullptr;
        }

        T* allocate(const T& value)
        {
            Item* item = allocateItem();
            return item ? new (item) T(value) : nullptr;
        }

        void release(T* object)
        {
            if (object)
            {
                object->~T();
                Item* item = (Item*)object;
                Pool* pool = (Pool*)item->m_pool;
                if (--pool->m_used == 0)
                {
                    releasePool(pool);
                }
            }
        }

        bool needRelease() const { return false; }

        void clear()
        {
            while (m_firstPool)
            {
                releasePool(m_firstPool);
            }
        }

    private:
        Item* allocateItem()
        {
            Pool* pool = (m_firstPool && m_firstPool->m_free) ? m_firstPool : allocateNewPool();
            if (pool)
            {
                Item* item = pool->m_free;
                pool->m_free = *(Item**)item;
                item->m_pool = pool;
                ++pool->m_used;
                return item;
            }
            return nullptr;
        }

        Pool* allocateNewPool()
        {
            ALLOCATOR allocatorType;
            hkMemoryAllocator& allocator = allocatorType.get(nullptr);
            Item* prev = nullptr;
            Pool* pool = (Pool*)allocator.blockAlloc(sizeof(Pool));
            if (pool)
            {
                for (int i = N - 1; i >= 0; --i)
                {
                    Item** current = (Item**)&pool->m_items[i];
                    *current = prev;
                    prev = (Item*)current;
                }
                pool->m_used = 0;
                pool->m_free = prev;
                pool->m_links[0] = nullptr;
                pool->m_links[1] = m_firstPool;
                m_firstPool = pool;
                if (pool->m_links[1])
                {
                    pool->m_links[1]->m_links[0] = pool;
                }
            }
            return pool;
        }

        void releasePool(Pool* pool)
        {
            ALLOCATOR allocatorType;
            hkMemoryAllocator& allocator = allocatorType.get(nullptr);
            if (pool->m_links[0])
            {
                pool->m_links[0]->m_links[1] = pool->m_links[1];
            }
            else
            {
                m_firstPool = pool->m_links[1];
            }
            if (pool->m_links[1])
            {
                pool->m_links[1]->m_links[0] = pool->m_links[0];
            }
            allocator.blockFree(pool, sizeof(Pool));
        }

        PoolAllocator(const PoolAllocator&);
        void operator=(const PoolAllocator&);

    public:
        Pool* m_firstPool; // 0x00
    };
};
