// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/hktypeinfo.cpp
//
// Havok 2014.2.5 (stub_impl.md S4, serializer SB): runs a type's finish (in-place construction) and cleanup functions,
// from rfg.exe. The release build has no memory tracker, so the tracked and untracked finish are the same.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/hktypeinfo.h"

// 0x00D1A9D0 (:20)
void hkTypeInfo::finishLoadedObject(void* ptr, int finishFlag) const
{
    if (m_finishLoadedObjectFunction != nullptr)
    {
        m_finishLoadedObjectFunction(ptr, finishFlag);
    }
}

// 0x00D1A9F0 (:42)
void hkTypeInfo::finishLoadedObjectWithoutTracker(void* ptr, int finishFlag) const
{
    if (m_finishLoadedObjectFunction != nullptr)
    {
        m_finishLoadedObjectFunction(ptr, finishFlag);
    }
}

// 0x00D1AA10 (:50)
void hkTypeInfo::cleanupLoadedObject(void* ptr) const
{
    if (m_cleanupLoadedObjectFunction != nullptr)
    {
        m_cleanupLoadedObjectFunction(ptr);
    }
}
