// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/dynamics/world/simulation/continuous/toiresourcemgr/default/hkpdefaulttoiresourcemgr.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/dynamics/world/simulation/continuous/toiresourcemgr/hkptoiresourcemgr.h"

class hkBool;
class hkClass;
struct hkContainerHeapAllocator;
struct hkResult;
struct hkpToiEvent;
enum hkpToiResourceMgrResponse;
struct hkpToiResources;

// size 0x10
class hkpDefaultToiResourceMgr : public hkpToiResourceMgr
{
public:
    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int nbytes) 0x00DFFDC0, delete(void* p, unsigned int nbytes) 0x00E9C3A0
    hkpDefaultToiResourceMgr(); // 0x00E9C200
    virtual ~hkpDefaultToiResourceMgr(); // vtbl+0x00 0x00E9C1C0
    virtual const hkClass* getClassType() const; // vtbl+0x08 0x00E9C390
    virtual hkResult beginToiAndSetupResources(const hkpToiEvent& event, const hkArray<hkpToiEvent,hkContainerHeapAllocator>& otherEvents, hkpToiResources& resourcesOut); // vtbl+0x10 0x00E9C220
    virtual hkpToiResourceMgrResponse cannotSolve(hkArray<hkpToiResourceMgr::ConstraintViolationInfo,hkContainerHeapAllocator>& violatedConstraints); // vtbl+0x14 0x00E9C1F0
    virtual hkpToiResourceMgrResponse resourcesDepleted(); // vtbl+0x18 0x00E9C1E0
    virtual void endToiAndFreeResources(const hkpToiEvent& event, const hkArray<hkpToiEvent,hkContainerHeapAllocator>& otherEvents, const hkpToiResources& resources); // vtbl+0x1C 0x00E9C2D0
    static const hkClass& staticClass();
    virtual int getScratchpadCapacity(); // vtbl+0x20 0x00E9C1B0
public:
    hkBool shouldHandleGivenToi(const hkpToiEvent& event); // 0x00E9C1D0

    int m_scratchPadCapacity; // 0x08
public:
    int m_defaultScratchpadSize; // 0x0C
    static const unsigned char s_priorityClassMap[]; // 0x01365724
    static const float s_priorityClassRatios[]; // 0x0136572C
};
