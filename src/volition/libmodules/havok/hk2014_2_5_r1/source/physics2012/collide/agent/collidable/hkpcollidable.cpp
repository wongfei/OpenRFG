// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcollidable.cpp
//
// Havok 2014.2.5 (havok_impl.md HK6): the collidable's constructor and bounding volume data, from rfg.exe (the
// collection agent3 builds a temporary collidable around its convex body).

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcollidable.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/router/hkmemoryrouter.h"

#include <string.h>

// 0x00DB9B20
hkpCollidable::hkpCollidable(const hkpShape* shape, const hkMotionState* ms, int type)
    : m_broadPhaseHandle(type)
{
    m_motion = ms;
    m_shape = shape;
    m_parent = nullptr;
    m_shapeKey = 0xFFFFFFFF; // HK_INVALID_SHAPE_KEY
    m_ownerOffset = 0;
    m_allowedPenetrationDepth = -1.0f; // not set: the world uses its default
    m_broadPhaseHandle.setOwner(this);
    m_forceCollideOntoPpu = FORCE_PPU_SHAPE_UNCHECKED;
    m_shapeSizeOnSpu = 0;
}

// 0x00D7D900
hkpCollidable::~hkpCollidable()
{
}

// 0x00DCADD0: no child boxes and an invalid box (minimum x above maximum x)
hkpCollidable::BoundingVolumeData::BoundingVolumeData()
{
    memset(this, 0, sizeof(BoundingVolumeData)); // hkString::memSet in the original
    m_min[0] = 1;
    m_max[0] = 0;
}

// 0x00D7D8C0
void hkpCollidable::BoundingVolumeData::invalidate()
{
    m_min[0] = 1;
    m_max[0] = 0;
}

// 0x00DB9330
bool hkpCollidable::BoundingVolumeData::isValid() const
{
    return m_min[0] <= m_max[0];
}

// 0x00D7DA00: the bounding volume data and broadphase handle are not serialized
hkpCollidable::hkpCollidable(hkFinishLoadedObjectFlag flag)
    : m_broadPhaseHandle(flag)
    , m_boundingVolumeData(flag)
{
    if (flag.m_finishing)
    {
        m_broadPhaseHandle.m_ownerOffset = -0x14; // the collidable is 0x14 bytes before its handle
    }
}

// 0x00D7D970
hkpCollidable::BoundingVolumeData::BoundingVolumeData(hkFinishLoadedObjectFlag flag)
{
    if (flag.m_finishing)
    {
        m_numChildShapeAabbs = 0;
        m_capacityChildShapeAabbs = 0;
        m_childShapeAabbs = nullptr;
        m_childShapeKeys = nullptr;
        m_min[0] = 1;
        m_max[0] = 0;
    }
}

// 0x00DCAEF0: one 32-byte box and one key per child (keys rounded up to 4) on the thread's heap
void hkpCollidable::BoundingVolumeData::allocate(int numChildShapes)
{
    m_numChildShapeAabbs = (unsigned short)numChildShapes;
    m_capacityChildShapeAabbs = (unsigned short)numChildShapes;
    if (numChildShapes != 0)
    {
        m_childShapeAabbs = (hkAabbUint32*)hkMemoryRouter::getInstance().heap().blockAlloc((numChildShapes & 0xFFFF) * 32);
        m_childShapeKeys = (unsigned int*)hkMemoryRouter::getInstance().heap().blockAlloc(((m_capacityChildShapeAabbs + 3) & ~3) * 4);
    }
}

// 0x00DCAE80
void hkpCollidable::BoundingVolumeData::deallocate()
{
    hkMemoryRouter::getInstance().heap().blockFree(m_childShapeAabbs, m_capacityChildShapeAabbs * 32);
    hkMemoryRouter::getInstance().heap().blockFree(m_childShapeKeys, ((m_capacityChildShapeAabbs + 3) & ~3) * 4);
    m_childShapeAabbs = nullptr;
    m_childShapeKeys = nullptr;
    m_numChildShapeAabbs = 0;
    m_capacityChildShapeAabbs = 0;
}

// 0x0080CCB0 (hkpcollidable.inl:121)
hkpTypedBroadPhaseHandle* hkpCollidable::getBroadPhaseHandle()
{
    return &m_broadPhaseHandle;
}

// 0x0080CCA0 (hkpcollidable.inl:116)
const hkpTypedBroadPhaseHandle* hkpCollidable::getBroadPhaseHandle() const
{
    return &m_broadPhaseHandle;
}

// 0x0080CCC0 (hkpcollidable.inl:141)
unsigned int hkpCollidable::getCollisionFilterInfo() const
{
    return m_broadPhaseHandle.getCollisionFilterInfo();
}

// 0x0080CCD0 (hkpcollidable.inl:146)
void hkpCollidable::setCollisionFilterInfo(unsigned int info)
{
    m_broadPhaseHandle.m_collisionFilterInfo = info;
}

// 0x00710FA0 (hkpcollidable.inl:126)
int hkpCollidable::getType() const
{
    return m_broadPhaseHandle.getType();
}

// 0x00710F70 (hkptypedbroadphasehandle.inl:34)
int hkpTypedBroadPhaseHandle::getType() const
{
    return m_type;
}
