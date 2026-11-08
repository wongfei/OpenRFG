// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshapecontainer.cpp
//
// Havok 2014.2.5 (havok_impl.md HK3): hkpShapeContainer, the child interface of collections and wrappers, and
// hkpSingleShapeContainer (one child, key 0), with the out-of-line copies of their inline members
// (hkpShapeContainer.h in Havok 2014.1).

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshapecontainer.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshape.h"

// 0x00710910
hkpShapeContainer::hkpShapeContainer()
{
}

// 0x007108F0 (.h:41)
hkpShapeContainer::~hkpShapeContainer()
{
}

// 0x00D74310: counts the keys
int hkpShapeContainer::getNumChildShapes() const
{
    int n = 0;
    for (unsigned int key = getFirstKey(); key != 0xFFFFFFFF; key = getNextKey(key)) // HK_INVALID_SHAPE_KEY
    {
        n++;
    }
    return n;
}

// 0x00D74300
unsigned int hkpShapeContainer::getCollisionFilterInfo(unsigned int key) const
{
    return 0;
}

// 0x00710900 (.h:79)
bool hkpShapeContainer::isWeldingEnabled() const
{
    return true;
}

// 0x00D75200
hkpSingleShapeContainer::hkpSingleShapeContainer(hkFinishLoadedObjectFlag)
{
}

// 0x00D74C10
hkpSingleShapeContainer::hkpSingleShapeContainer(const hkpShape* s, hkpShapeContainer::ReferencePolicy ref)
    : m_childShape(s)
{
    if (ref == REFERENCE_POLICY_INCREMENT)
    {
        m_childShape->addReference();
    }
}

// inline in Havok (hkpShapeContainer.h)
hkpSingleShapeContainer::hkpSingleShapeContainer()
{
}

// 0x0071AA90 (.h:110)
hkpSingleShapeContainer::~hkpSingleShapeContainer()
{
    if (m_childShape != nullptr)
    {
        m_childShape->removeReference();
    }
}

// 0x0071AAB0 (.h:118)
int hkpSingleShapeContainer::getNumChildShapes() const
{
    return 1;
}

// 0x0071AAC0 (.h:121)
unsigned int hkpSingleShapeContainer::getFirstKey() const
{
    return 0;
}

// 0x0071AAD0 (.h:124)
unsigned int hkpSingleShapeContainer::getNextKey(unsigned int oldKey) const
{
    return 0xFFFFFFFF; // HK_INVALID_SHAPE_KEY
}

// 0x00D74340
const hkpShape* hkpSingleShapeContainer::getChildShape(unsigned int key, hkpShapeBufferStorage& buffer) const
{
    return m_childShape;
}

// 0x0071B6B0 (.h:134)
const hkpShape* hkpSingleShapeContainer::getChild() const
{
    return m_childShape;
}

// 0x00D74CE0
const hkpShape* hkpSingleShapeContainer::operator->() const
{
    return m_childShape;
}
