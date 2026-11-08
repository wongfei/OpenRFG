// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/broadphase/hkptypedbroadphasehandle.cpp
//
// Havok 2014.2.5 (havok_impl.md HK6): the typed broadphase handle's constructor, from rfg.exe.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/broadphase/hkptypedbroadphasehandle.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"

// 0x00D9EC70: no owner yet (OFFSET_INVALID 0x7F) and HK_COLLIDABLE_QUALITY_INVALID (-1)
hkpTypedBroadPhaseHandle::hkpTypedBroadPhaseHandle(int type)
{
    m_id = 0;
    m_type = (signed char)type;
    m_collisionFilterInfo = 0;
    m_ownerOffset = OFFSET_INVALID;
    m_objectQualityType = -1;
}

// 0x00D7D8B0
void hkpTypedBroadPhaseHandle::setOwner(void* o)
{
    m_ownerOffset = (signed char)((char*)o - (char*)this);
}

// 0x00D7D5C0: nothing to fix up (the collidable sets the owner offset)
hkpTypedBroadPhaseHandle::hkpTypedBroadPhaseHandle(hkFinishLoadedObjectFlag flag)
    : hkpBroadPhaseHandle(flag)
{
}

// 0x0080CC70 (hkptypedbroadphasehandle.inl:50)
unsigned int hkpTypedBroadPhaseHandle::getCollisionFilterInfo() const
{
    return m_collisionFilterInfo;
}
