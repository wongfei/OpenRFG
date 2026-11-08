// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/action/hkpaction.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/string/hkstringptr.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hkreferencedobject.h"

class hkClass;
struct hkContainerHeapAllocator;
class hkFinishLoadedObjectFlag;
struct hkInternalClassMember;
class hkStepInfo;
class hkpEntity;
class hkpPhantom;
class hkpSimulationIsland;
class hkpWorld;

// size 0x18
class hkpAction : public hkReferencedObject
{
public:
    static const hkClass& staticClass(); // 0x00DEDB80
    HK_DECLARE_CLASS_ALLOCATOR(); // delete(void* p, unsigned int nbytes) 0x00DF00F0
    hkpAction(hkFinishLoadedObjectFlag flag); // 0x00DEFFE0
    hkpAction(unsigned long userData); // 0x00DF5B30
    virtual ~hkpAction(); // vtbl+0x00 0x00DEFFC0
    virtual void applyAction(const hkStepInfo&) = 0; // vtbl+0x10
    virtual void getEntities(hkArray<hkpEntity*,hkContainerHeapAllocator>&) = 0; // vtbl+0x14
    virtual void getPhantoms(hkArray<hkpPhantom*,hkContainerHeapAllocator>& phantomsOut); // vtbl+0x18 0x00818120 (.h:51)
    hkpWorld* getWorld(); // 0x00DE8790
    virtual void entityRemovedCallback(hkpEntity*) = 0; // vtbl+0x1C
    unsigned long getUserData() const;
    void setUserData(unsigned long);
    const char* getName() const;
    void setName(const char*);
    virtual hkpAction* clone(const hkArray<hkpEntity*,hkContainerHeapAllocator>&, const hkArray<hkpPhantom*,hkContainerHeapAllocator>&) const = 0; // vtbl+0x20
    hkpSimulationIsland* getSimulationIsland(); // 0x00DE8770
    void setSimulationIsland(hkpSimulationIsland* isl); // 0x00E0A880
    void setWorld(hkpWorld* world); // 0x00DE8780

    static const hkInternalClassMember Members[]; // 0x0135CBC0
public:
    hkpWorld* m_world; // 0x08
    hkpSimulationIsland* m_island; // 0x0C
public:
    unsigned long m_userData; // 0x10
    hkStringPtr m_name; // 0x14
};
