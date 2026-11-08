// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/hkpworldobject.cpp
//
// Havok 2014.2.5 (havok_impl.md HK7): the base of entities and phantoms (collidable, name, properties), from
// rfg.exe, with the out-of-line copies of hkpworldobject.inl.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/hkpworldobject.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshape.h"


// 0x00DF48F0: the collidable at +0x10 points back here (owner offset -0x10); the object references its shape
hkpWorldObject::hkpWorldObject(const hkpShape* shape, BroadPhaseType type)
    : m_collidable(shape, nullptr, type)
{
    m_world = nullptr;
    m_userData = 0;
    m_multiThreadCheck.m_markCount = 0x8000; // hkMultiThreadCheck::disableChecks
    m_multiThreadCheck.m_threadId = 0xFFFFFFF1;
    m_collidable.m_ownerOffset = (signed char)((char*)this - (char*)&m_collidable);
    if (shape != nullptr)
    {
        shape->addReference();
    }
}

// 0x00DF4870
hkpWorldObject::hkpWorldObject(hkFinishLoadedObjectFlag flag)
    : hkReferencedObject(flag)
    , m_collidable(flag)
    , m_name(flag)
    , m_properties(flag)
{
    if (flag.m_finishing)
    {
        // the bounding volume data and broadphase handle are not serialized
        m_collidable.m_boundingVolumeData.m_numChildShapeAabbs = 0;
        m_collidable.m_boundingVolumeData.m_capacityChildShapeAabbs = 0;
        m_collidable.m_boundingVolumeData.m_childShapeAabbs = nullptr;
        m_collidable.m_boundingVolumeData.m_childShapeKeys = nullptr;
        m_collidable.m_boundingVolumeData.m_min[0] = 1;
        m_collidable.m_boundingVolumeData.m_max[0] = 0;
        m_collidable.m_broadPhaseHandle.m_ownerOffset = -0x14; // the collidable is 0x14 bytes before its handle
    }
    m_multiThreadCheck.m_threadId = 0xFFFFFFF1;
    m_multiThreadCheck.m_markCount = 0x8000;
    if (flag.m_finishing)
    {
        afterReflectNew();
    }
}

// 0x00DDF990 (hkpworldobject.inl): the reference on the shape goes; the members free their storage
hkpWorldObject::~hkpWorldObject()
{
    if (m_collidable.m_shape != nullptr)
    {
        m_collidable.m_shape->removeReference();
    }
}

// 0x00DF4660
void hkpWorldObject::afterReflectNew()
{
    m_collidable.m_ownerOffset = -0x10;
}

// 0x00DF45D0, 0x00DF45E0: only rigid bodies and shape phantoms change their shape
hkWorldOperation::Result hkpWorldObject::setShape(const hkpShape* shape)
{
    return hkWorldOperation::DONE;
}

hkWorldOperation::Result hkpWorldObject::updateShape(hkpShapeModifier* shapeModifier)
{
    return hkWorldOperation::DONE;
}

// 0x00DF4770: a key that exists keeps its value; the new entry's alignment padding is not written
void hkpWorldObject::addProperty(unsigned int key, hkSimplePropertyValue value)
{
    for (int i = 0; i < m_properties.getSize(); i++)
    {
        if (m_properties[i].m_key == key)
        {
            return;
        }
    }
    hkSimpleProperty& property = m_properties.expandOne();
    property.m_value = value;
    property.m_key = key;
}

// 0x00DF46B0: the value, or 0 when there is none
hkSimplePropertyValue hkpWorldObject::removeProperty(unsigned int key)
{
    for (int i = 0; i < m_properties.getSize(); i++)
    {
        if (m_properties[i].m_key == key)
        {
            const hkSimplePropertyValue value = m_properties[i].m_value;
            m_properties.removeAtAndCopy(i);
            return value;
        }
    }
    hkSimplePropertyValue none;
    none.m_data = 0;
    return none;
}

// 0x00814240 (hkpworldobject.inl)
hkSimplePropertyValue hkpWorldObject::getProperty(unsigned int key, MtChecks mtCheck) const
{
    for (int i = 0; i < m_properties.getSize(); i++)
    {
        if (m_properties[i].m_key == key)
        {
            return m_properties[i].m_value;
        }
    }
    hkSimplePropertyValue none;
    none.m_data = 0;
    return none;
}

// 0x008142A0 (hkpworldobject.inl)
bool hkpWorldObject::hasProperty(unsigned int key, MtChecks mtCheck) const
{
    for (int i = 0; i < m_properties.getSize(); i++)
    {
        if (m_properties[i].m_key == key)
        {
            return true;
        }
    }
    return false;
}

// 0x00DE0C50 (hkpworldobject.inl)
void hkpWorldObject::copyProperties(const hkpWorldObject* otherObj)
{
    m_properties = otherObj->m_properties;
}

// 0x00710FC0 (hkpworldobject.inl)
hkpWorld* hkpWorldObject::getWorld() const
{
    return m_world;
}

// 0x00742370 (hkpworldobject.inl)
const hkpCollidable* hkpWorldObject::getCollidable() const
{
    return &m_collidable;
}

// 0x0080CD50 (hkpworldobject.inl)
hkpCollidable* hkpWorldObject::getCollidableRw()
{
    return &m_collidable;
}

// 0x00DDE660, 0x00DDE650
const hkpLinkedCollidable* hkpWorldObject::getLinkedCollidable() const
{
    return &m_collidable;
}

hkpLinkedCollidable* hkpWorldObject::getLinkedCollidable()
{
    return &m_collidable;
}

// 0x0080CD60 (hkpworldobject.inl)
hkBool hkpWorldObject::isAddedToWorld() const
{
    hkBool result;
    result.m_bool = m_world != nullptr;
    return result;
}

// 0x00DE9200
void hkpWorldObject::setWorld(hkpWorld* world)
{
    m_world = world;
}

// 0x00DF2880, 0x00DF2890
unsigned long hkpWorldObject::getUserData() const
{
    return m_userData;
}

void hkpWorldObject::setUserData(unsigned long data)
{
    m_userData = data;
}

// 0x00DF8D40, 0x00DF8D50, 0x00DF4630, 0x00DF4640, 0x00DF4650: multithreading checks (no-ops in release)
void hkpWorldObject::markForWrite()
{
}

void hkpWorldObject::unmarkForWrite()
{
}

void hkpWorldObject::markForWriteImpl()
{
}

void hkpWorldObject::checkReadWrite()
{
}

void hkpWorldObject::checkReadOnly() const
{
}
