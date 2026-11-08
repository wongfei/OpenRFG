// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/action/hkpunaryaction.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/action/hkpaction.h"

class hkClass;
struct hkContainerHeapAllocator;
class hkFinishLoadedObjectFlag;
struct hkInternalClassMember;
class hkStepInfo;
class hkpEntity;
class hkpRigidBody;

// size 0x1C
class hkpUnaryAction : public hkpAction
{
public:
    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int nbytes) 0x00814780, delete(void* p, unsigned int nbytes) 0x00818150
    static const hkClass& staticClass(); // 0x00DED930
    hkpUnaryAction(hkFinishLoadedObjectFlag flag); // 0x00F15070
    hkpUnaryAction(hkpEntity* entity, unsigned long userData); // 0x00DF59E0
    virtual ~hkpUnaryAction(); // vtbl+0x00 0x00DF5A30
    virtual const hkClass* getClassType() const; // vtbl+0x08 0x00818130 (.h:53)
    virtual void applyAction(const hkStepInfo&) = 0; // vtbl+0x10
    void setEntity(hkpEntity* entity); // 0x00DF5A70
    hkpEntity* getEntity(); // 0x00B63020 (hkpunaryaction.inl:10)
    hkpRigidBody* getRigidBody(); // 0x00F24A60
    virtual void getEntities(hkArray<hkpEntity*,hkContainerHeapAllocator>& entitiesOut); // vtbl+0x14 0x00DF5AF0
    virtual void entityRemovedCallback(hkpEntity* entity); // vtbl+0x1C 0x00DF59C0

    static const hkInternalClassMember Members[]; // 0x0135C23C
public:
    hkpEntity* m_entity; // 0x18
};
