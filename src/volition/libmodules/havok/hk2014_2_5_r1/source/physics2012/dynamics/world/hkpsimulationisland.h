// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/hkpsimulationisland.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/debugutil/multithreadcheck/hkmultithreadcheck.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/agent3/machine/nn/hkpagentnntrack.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/hkpconstraintowner.h"

class hkBool;
class hkClass;
class hkUnionFind;
class hkpAction;
class hkpConstraintInstance;
class hkpEntity;
class hkpWorld;

// size 0x84
class hkpSimulationIsland : public hkpConstraintOwner
{
public:
    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int nbytes) 0x00DEA750, delete(void* p, unsigned int nbytes) 0x00E1DCD0
    const hkArray<hkpEntity*,hkContainerHeapAllocator>& getEntities() const; // 0x0080D350 (hkpsimulationisland.inl:27)
    const hkArray<hkpAction*,hkContainerHeapAllocator>& getActions() const; // 0x00DE8830
    hkBool wasActiveLastFrame() const; // 0x0080D340 (hkpsimulationisland.inl:17)
    hkBool isFixed() const; // 0x00DE8810
    hkpWorld* getWorld(); // 0x00DE8800
    int getMemUsageForIntegration(); // 0x00DEA770
    virtual const hkClass* getClassType() const; // vtbl+0x08 0x00E1DCC0
    static const hkClass& staticClass();
public:
    bool isFullyConnected(hkUnionFind& checkConnectivityOut); // 0x00E1D6A0
public:
    bool isFullyConnected(); // 0x00E1D9D0
public:
    hkpSimulationIsland(hkpWorld* world); // 0x00E1DA90
    virtual ~hkpSimulationIsland(); // vtbl+0x00 0x00E1D8F0
    void internalAddEntity(hkpEntity* entity); // 0x00E1D610
    void internalRemoveEntity(hkpEntity* entity); // 0x00E1D580
    virtual void addConstraintToCriticalLockedIsland(hkpConstraintInstance* constraint); // vtbl+0x10 0x00E1D540
    virtual void removeConstraintFromCriticalLockedIsland(hkpConstraintInstance* constraint); // vtbl+0x14 0x00E1D560
    virtual void addCallbackRequest(hkpConstraintInstance* constraint, int request); // vtbl+0x18 0x00E1D4B0
    void addAction(hkpAction* act); // 0x00E1D660
    void removeAction(hkpAction* act); // 0x00E1D4F0
public:
    int getStorageIndex(); // 0x00E0A850
    void markForWrite(); // 0x00E1D4D0
    void unmarkForWrite(); // 0x00DEA7B0
    void isValid(); // 0x00E1D4A0
    void mergeConstraintInfo(hkpSimulationIsland& other); // 0x00E1D5E0
    void markAllEntitiesReadOnly() const; // 0x00DF5C20
    void unmarkAllEntitiesReadOnly() const; // 0x00DF5C30
    hkMultiThreadCheck& getMultiThreadCheck() const; // 0x00E28AE0
    hkpAgentNnTrack* getAgentNnTrack(hkpAgentNnTrackType nnTrackType); // 0x00E1DD80

    hkpWorld* m_world; // 0x18
    int m_numConstraints; // 0x1C
    unsigned short m_storageIndex; // 0x20
    unsigned short m_dirtyListIndex; // 0x22
    unsigned char m_splitCheckFrameCounter; // 0x24
    bool m_splitCheckRequested : 2; // 0x25:0
    bool m_isSparse : 2; // 0x25:2
    bool m_actionListCleanupNeeded : 2; // 0x25:4
    bool m_allowIslandLocking : 2; // 0x25:6
    bool m_isInActiveIslandsArray : 2; // 0x26:0
    bool m_activeMark : 2; // 0x26:2
    bool m_tryToIncreaseIslandSizeMark : 2; // 0x26:4
    bool m_inIntegrateJob : 2; // 0x26:6
public:
    hkMultiThreadCheck m_multiThreadCheck; // 0x28
public:
    float m_timeSinceLastHighFrequencyCheck; // 0x34
    float m_timeSinceLastLowFrequencyCheck; // 0x38
    hkArray<hkpAction*,hkContainerHeapAllocator> m_actions; // 0x3C
    float m_timeOfDeactivation; // 0x48
    hkInplaceArray<hkpEntity*,1,hkContainerHeapAllocator> m_entities; // 0x4C
    hkpAgentNnTrack m_midphaseAgentTrack; // 0x5C
    hkpAgentNnTrack m_narrowphaseAgentTrack; // 0x70
};
