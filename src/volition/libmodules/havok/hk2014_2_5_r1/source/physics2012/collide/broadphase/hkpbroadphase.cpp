// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/broadphase/hkpbroadphase.cpp
//
// Havok 2014.2.5 (havok_impl.md HK4): the broadphase base (type, size, capabilities, the optional critical section) and
// the creation functions the world calls, from rfg.exe. hkProductFeatures::initialize sets
// s_createSweepAndPruneBroadPhaseFunction to hk3AxisSweep16CreateBroadPhase through the hkp3AxisSweep optional
// component; the optional components are not implemented yet (HK5), so the pointer stays null here.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/broadphase/hkpbroadphase.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/thread/criticalsection/hkcriticalsection.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/broadphase/hkpbroadphasecastcollector.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/broadphase/hkpbroadphasehandle.h"

// ---- globals (BSS)
hkpBroadPhase* (*hkpBroadPhase::s_createSweepAndPruneBroadPhaseFunction)(const hkVector4f&, const hkVector4f&, int); // 0x03A29D60
hkpBroadPhase* (*hkpBroadPhase::s_createTreeBroadPhaseFunction)(hkpBroadPhase*); // 0x03A29D64
void (*hkpBroadPhase::s_updateTreeBroadPhaseFunction)(hkpBroadPhase*, float); // 0x03A29D68
void (*hkpBroadPhase::s_updateTreeBroadPhaseFunction32)(hkpBroadPhase*, float); // 0x03A29D6C

// 0x00DB0FA0
hkpBroadPhase::hkpBroadPhase(BroadPhaseType type, int size, int caps)
    : m_type((unsigned short)type)
    , m_size((unsigned short)size)
    , m_caps((unsigned int)caps)
{
    m_criticalSection = nullptr;
    m_multiThreadCheck.disableChecks();
}

// 0x00DB1070
hkpBroadPhase::~hkpBroadPhase()
{
    if (m_criticalSection != nullptr)
    {
        delete m_criticalSection;
        m_criticalSection = nullptr;
    }
}

// 0x00DB0FF0
void hkpBroadPhase::lockImplementation()
{
    m_criticalSection->enter();
}

// 0x00DB1000
void hkpBroadPhase::unlockImplementation()
{
    m_criticalSection->leave();
}

// 0x00DB10C0 (hkpbroadphase.h)
void hkpBroadPhase::checkDeterminism()
{
}

// 0x00D8F8E0 (hkpbroadphase.h): this broadphase when it has the capability.
const hkpBroadPhase* hkpBroadPhase::getCapabilityDelegate(Capabilities cap) const
{
    return (m_caps & cap) != 0 ? this : nullptr;
}

// 0x00DB1010: a critical section for lock()/unlock() (once).
void hkpBroadPhase::enableMultiThreading(int spinCountForCriticalSection)
{
    if (m_criticalSection == nullptr)
    {
        m_criticalSection = new hkCriticalSection(spinCountForCriticalSection);
        m_multiThreadCheck.enableChecks();
    }
}

// 0x00DFFDA0 (hkpbroadphase.inl:42)
void hkpBroadPhase::lock()
{
    if (m_criticalSection != nullptr)
    {
        lockImplementation();
    }
}

// 0x00DFFDB0 (hkpbroadphase.inl:54)
void hkpBroadPhase::unlock()
{
    if (m_criticalSection != nullptr)
    {
        unlockImplementation();
    }
}

// 0x00DB0F80 (hkpbroadphase.inl:27): multithread checks are compiled out.
void hkpBroadPhase::markForWrite()
{
}

// 0x00DB0F90 (hkpbroadphase.inl:37)
void hkpBroadPhase::unmarkForWrite()
{
}

// 0x00D93040 (hkpbroadphase.h:205): one cast, packed targets, no cache (m_from and m_toBase are left).
hkpBroadPhase::hkpCastRayInput::hkpCastRayInput()
{
    m_numCasts = 1;
    m_toStriding = sizeof(hkVector4f);
    m_aabbCacheInfo = nullptr;
}

// 0x0104BCB0 (hkpbroadphase.h:268)
hkpBroadPhase::hkpCastAabbInput::hkpCastAabbInput()
{
    m_aabbCacheInfo = nullptr;
}

// 0x00812630 (hkpbroadphasecastcollector.h)
hkpBroadPhaseCastCollector::hkpBroadPhaseCastCollector()
{
}

// 0x008125F0 (hkpbroadphasecastcollector.h:21)
hkpBroadPhaseCastCollector::~hkpBroadPhaseCastCollector()
{
}

// 0x00D8B8E0 (hkpbroadphasehandle.h:30)
hkpBroadPhaseHandle::hkpBroadPhaseHandle()
{
    m_id = 0;
}

// 0x00D7D5A0
hkpBroadPhaseHandle::hkpBroadPhaseHandle(hkFinishLoadedObjectFlag flag)
{
}
