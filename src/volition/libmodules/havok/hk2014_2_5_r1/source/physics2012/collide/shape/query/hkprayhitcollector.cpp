// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/query/hkprayhitcollector.cpp
//
// Havok 2014.2.5 (havok_impl.md HK3).

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/query/hkprayhitcollector.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent/collidable/hkpcdbody.h"

// 0x00D83E70
// The shape keys of a hit body's path from the root: the root's child key first, HK_INVALID_SHAPE_KEY last. Writes at
// most maxKeys - 1 keys plus the terminator; returns the number of words written.
int hkpRayHitCollector::shapeKeysFromCdBody(unsigned int* buf, int maxKeys, const hkpCdBody& body)
{
    const hkpCdBody* path[8]; // HK_MAX_SHAPE_HIERARCHY_DEPTH; the root itself is not stored
    int numBodies = 0;
    const hkpCdBody* b = &body;
    while (b->m_parent != nullptr)
    {
        path[numBodies++] = b;
        b = b->m_parent;
    }

    int i = 0;
    for (; numBodies > 0 && i < maxKeys - 1; i++)
    {
        buf[i] = path[--numBodies]->m_shapeKey;
    }
    buf[i] = 0xFFFFFFFF; // HK_INVALID_SHAPE_KEY
    return i + 1;
}
