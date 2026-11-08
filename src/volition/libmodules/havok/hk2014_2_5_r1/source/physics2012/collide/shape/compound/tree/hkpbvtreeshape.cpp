// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/compound/tree/hkpbvtreeshape.cpp
//
// Havok 2014.2.5 (havok_impl.md HK3): the base of the shapes that find their children through a bounding volume tree
// (MOPP, height fields, rfg_destroyable_shape as BVTREE_USER), from rfg.exe.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/compound/tree/hkpbvtreeshape.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/allocator/lifo/hklifoallocator.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/router/hkmemoryrouter.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/geometry/aabb/hkaabb.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/query/hkpaabbcastcollector.h"

// 0x0071AB60 (hkpbvtreeshape.inl)
hkpBvTreeShape::hkpBvTreeShape(hkcdShapeType::ShapeTypeEnum type, BvTreeType bvType)
    : hkpShape(type)
{
    m_bvTreeType.m_storage = (unsigned char)bvType;
}

// 0x00D74980
hkpBvTreeShape::hkpBvTreeShape(hkFinishLoadedObjectFlag flag)
    : hkpShape(flag)
{
    if (flag.m_finishing)
    {
        afterReflectNew();
    }
}

// 0x00710920
hkpBvTreeShape::~hkpBvTreeShape()
{
}

// 0x00D74820: a bv tree loaded on its own is a user tree
void hkpBvTreeShape::afterReflectNew()
{
    m_type.m_storage = hkcdShapeType::BV_TREE;
    m_bvTreeType.m_storage = BVTREE_USER;
}

// 0x00D747C0
unsigned int hkpBvTreeShape::queryAabb(const hkAabb& aabb, unsigned int* hits, int maxNumKeys) const
{
    return queryAabbImpl(aabb, hits, maxNumKeys);
}

// 0x00DB1F20
void hkpBvTreeShape::castAabb(const hkAabb& from, const hkVector4f& to, hkpAabbCastCollector& collector) const
{
    castAabbImpl(from, to, collector);
}

// 0x00D74830
// The default AABB cast: every key whose volume overlaps the box swept from its centre to `to` (at most 2048, in a
// LIFO block) goes to the collector.
void hkpBvTreeShape::castAabbImpl(const hkAabb& from, const hkVector4f& to, hkpAabbCastCollector& collector) const
{
    hkVector4 center; center.setAdd(from.m_max, from.m_min);
    center.mul(hkVector4::getConstant<HK_QUADREAL_INV_2>());
    hkVector4 path; path.setSub(to, center);
    const hkVector4 zero = hkVector4::getConstant<HK_QUADREAL_0>();
    hkAabb swept;
    swept.m_min.setMin(path, zero);
    swept.m_min.add(from.m_min);
    swept.m_max.setMax(path, zero);
    swept.m_max.add(from.m_max);

    const int maxKeys = 0x800; // 2048 keys: one 8 KB LIFO block
    hkLifoAllocator& stack = hkMemoryRouter::getInstance().stack();
    unsigned int* hits = static_cast<unsigned int*>(stack.fastBlockAlloc(maxKeys * (int)sizeof(unsigned int)));
    int numHits = (int)queryAabbImpl(swept, hits, maxKeys);
    if (numHits >= maxKeys)
    {
        numHits = maxKeys;
    }
    for (int i = 0; i < numHits; i++)
    {
        collector.addHit(hits[i]);
    }
    stack.fastBlockFree(hits, maxKeys * (int)sizeof(unsigned int));
}
