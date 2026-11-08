// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/internal/dynamics/world/simulation/continuous/toiresourcemgr/hkptoiresourcemgr.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0); hkpToiResources and hkpToiResourceMgrResponse
// added by hand in HK7 (layouts from rfg.pdb). Hand-maintained (nobuild in the hdrgen specs).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hkreferencedobject.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/hkpconstraintinstance.h"

class hkContactPoint;
struct hkContainerHeapAllocator;
struct hkResult;
class hkpConstraintInstance;
class hkpContactPointProperties;
class hkpEntity;
struct hkpToiEvent;

enum hkpToiResourceMgrResponse
{
    HK_TOI_RESOURCE_MGR_RESPONSE_CONTINUE = 0,
    HK_TOI_RESOURCE_MGR_RESPONSE_DO_NOT_EXPAND_AND_CONTINUE = 1,
    HK_TOI_RESOURCE_MGR_RESPONSE_BACKSTEP = 2,
};

// size 0x28
struct hkpToiResources
{
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpToiResources);

    hkpConstraintInstance::ConstraintPriority m_minPriorityToProcess; // 0x00
    int m_maxNumEntities; // 0x04
    int m_maxNumActiveEntities; // 0x08
    int m_maxNumConstraints; // 0x0C
    int m_numToiSolverIterations; // 0x10
    int m_numForcedToiFinalSolverIterations; // 0x14
    char* m_scratchpad; // 0x18
    int m_scratchpadSize; // 0x1C
    const unsigned char* m_priorityClassMap; // 0x20
    const float* m_priorityClassRatios; // 0x24
};

// size 0x8
class hkpToiResourceMgr : public hkReferencedObject
{
public:
    // size 0xC
    struct ConstraintViolationInfo
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(ConstraintViolationInfo);
        hkpEntity* getEntityA();
        hkpEntity* getEntityB();

        hkpConstraintInstance* m_constraint; // 0x00
        const hkContactPoint* m_contactPoint; // 0x04
        const hkpContactPointProperties* m_contactPointProperties; // 0x08
    };

    HK_DECLARE_CLASS_ALLOCATOR(); // delete(void* p, unsigned int nbytes) 0x00E9C300
    virtual ~hkpToiResourceMgr(); // vtbl+0x00 0x00E9C170
    virtual hkResult beginToiAndSetupResources(const hkpToiEvent&, const hkArray<hkpToiEvent,hkContainerHeapAllocator>&, hkpToiResources&) = 0; // vtbl+0x10
    virtual hkpToiResourceMgrResponse cannotSolve(hkArray<ConstraintViolationInfo,hkContainerHeapAllocator>& violatedConstraints); // vtbl+0x14 0x00E9C190
    virtual hkpToiResourceMgrResponse resourcesDepleted(); // vtbl+0x18 0x00E9C1A0
    virtual void endToiAndFreeResources(const hkpToiEvent&, const hkArray<hkpToiEvent,hkContainerHeapAllocator>&, const hkpToiResources&) = 0; // vtbl+0x1C
    virtual int getScratchpadCapacity() = 0; // vtbl+0x20
    hkpToiResourceMgr(); // 0x00E9C180

};
