// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/hkpconstraintinstance.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/string/hkstringptr.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hkreferencedobject.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"

class hkClass;
struct hkConstraintInternal;
struct hkInternalClassMember;
class hkTransformf;
class hkVector4f;
struct hkpConstraintAtom;
class hkpConstraintData;
struct hkpConstraintInfo;
class hkpConstraintListener;
class hkpConstraintOwner;
struct hkpConstraintRuntime;
class hkpEntity;
struct hkpModifierConstraintAtom;
class hkpRigidBody;
class hkpSimulationIsland;
class hkpWorld;

// size 0x38
class hkpConstraintInstance : public hkReferencedObject
{
public:
    enum ConstraintPriority
    {
        PRIORITY_INVALID = 0,
        PRIORITY_PSI = 1,
        PRIORITY_SIMPLIFIED_TOI_UNUSED = 2,
        PRIORITY_TOI = 3,
        PRIORITY_TOI_HIGHER = 4,
        PRIORITY_TOI_FORCED = 5,
        NUM_PRIORITIES = 6,
    };

    enum InstanceType
    {
        TYPE_NORMAL = 0,
        TYPE_CHAIN = 1,
        TYPE_DISABLE_SPU = 2,
    };

    enum AddReferences
    {
        DO_NOT_ADD_REFERENCES = 0,
        DO_ADD_REFERENCES = 1,
    };

    enum CloningMode
    {
        CLONE_SHALLOW_IF_NOT_CONSTRAINED_TO_WORLD = 0,
        CLONE_DATAS_WITH_MOTORS = 1,
        CLONE_FORCE_SHALLOW = 2,
    };

    enum OnDestructionRemapInfo
    {
        ON_DESTRUCTION_REMAP = 0,
        ON_DESTRUCTION_REMOVE = 1,
        ON_DESTRUCTION_RESET_REMOVE = 2,
    };

    // size 0x8
    class SmallArraySerializeOverrideType
    {
    public:
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(SmallArraySerializeOverrideType);
        static const hkClass& staticClass(); // 0x00DEED80

        static const hkInternalClassMember Members[];
        void* m_data; // 0x00
        unsigned short m_size; // 0x04
        unsigned short m_capacityAndFlags; // 0x06
    };

    static const hkClass& staticClass(); // 0x00DEEDA0
    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int, void* p) 0x00DEEE30, new(unsigned int nbytes) 0x00814210, delete(void* p, unsigned int nbytes) 0x00DEEF80
public:
    hkpConstraintInstance();
public:
    hkpConstraintInstance(ConstraintPriority priority); // 0x00E05F10
public:
    hkpConstraintInstance(hkFinishLoadedObjectFlag f); // 0x00DEEF30
    hkpConstraintInstance(hkpEntity* entityA, hkpEntity* entityB, hkpConstraintData* data, ConstraintPriority priority); // 0x00E05E40
    virtual ~hkpConstraintInstance(); // vtbl+0x00 0x00E060B0
    hkpConstraintInstance* clone(hkpEntity* newEntityA, hkpEntity* newEntityB, CloningMode mode) const; // 0x00E05F60
    hkpEntity* getEntityA() const; // 0x00DE9690
    hkpEntity* getEntity(int) const;
    void replaceEntity(hkpEntity* oldEntity, hkpEntity* newEntity); // 0x00E05BD0
    hkpEntity* getEntityB() const; // 0x00DE96A0
    hkpRigidBody* getRigidBodyA() const; // 0x0080CBF0 (hkpconstraintinstance.inl:27)
    hkpRigidBody* getRigidBodyB() const; // 0x0080CC00 (hkpconstraintinstance.inl:32)
    hkBool isConstrainedToWorld() const; // 0x00E05AF0
    hkpEntity* getOtherEntity(const hkpEntity* entity) const; // 0x00DDEAF0
    hkpSimulationIsland* getSimulationIsland(); // 0x00E05CB0
    const hkpConstraintData* getData() const; // 0x0080CC10 (hkpconstraintinstance.inl:90)
    hkpConstraintData* getDataRw() const; // 0x0080CC20 (hkpconstraintinstance.inl:95)
    void getPivotsInWorld(hkVector4f& pivotAinW, hkVector4f& pivotBinW) const; // 0x00E05B20
    void addConstraintListener(hkpConstraintListener* listener); // 0x00E05CD0
    void removeConstraintListener(hkpConstraintListener* listener); // 0x00E05AB0
    void transform(const hkTransformf& transformation); // 0x00E05D40
    hkpModifierConstraintAtom* getConstraintModifiers() const; // 0x00E1B250
    hkpConstraintRuntime* getRuntime() const; // 0x0080CC30 (hkpconstraintinstance.inl:106)
    void setWantRuntime(hkBool);
    hkBool getWantRuntime() const;
    unsigned long getUserData() const;
    void setUserData(unsigned long data); // 0x00DE9770
    ConstraintPriority getPriority() const; // 0x00E00600
    void setPriority(ConstraintPriority priority); // 0x00E05A70
    const char* getName() const; // 0x00E06230
    void setName(const char* name); // 0x00E061E0
    void setVirtualMassInverse(const hkVector4f& invMassA, const hkVector4f& invMassB); // 0x00E05DB0
    void disable(); // 0x00E05BA0
    void enable(); // 0x00E05B90
    void setEnabled(hkBool state); // 0x00E05BB0
    hkBool isEnabled(); // 0x00E05A50
    hkpConstraintOwner* getOwner() const; // 0x00A05430 (hkpconstraintinstance.inl:80)
    hkpEntity* getMasterEntity() const; // 0x00DFFC20
    hkpEntity* getSlaveEntity() const;
    virtual void entityAddedCallback(hkpEntity* entity); // vtbl+0x10 0x00E05A30
    virtual void entityRemovedCallback(hkpEntity* entity); // vtbl+0x14 0x00E05A90
    virtual void entityDeletedCallback(hkpEntity* entity); // vtbl+0x18 0x00E05A40
    void setOwner(hkpConstraintOwner* owner); // 0x00E0ABA0
    hkConstraintInternal* getInternal(); // 0x00A05440 (hkpconstraintinstance.inl:100)
    void pointNullsToFixedRigidBody(); // 0x00E05D70
    void setFixedRigidBodyPointersToZero(hkpWorld* world); // 0x00E05C30
    virtual InstanceType getType() const; // vtbl+0x1C 0x00DEEF70

    static const hkInternalClassMember Members[]; // 0x016F7790
public:
    hkpConstraintOwner* m_owner; // 0x08
    hkpConstraintData* m_data; // 0x0C
    hkpModifierConstraintAtom* m_constraintModifiers; // 0x10
    hkpEntity* m_entities[2]; // 0x14
public:
    hkEnum<enum ConstraintPriority,unsigned char> m_priority; // 0x1C
public:
    hkBool m_wantRuntime; // 0x1D
public:
    hkEnum<enum OnDestructionRemapInfo,unsigned char> m_destructionRemapInfo; // 0x1E
    hkSmallArray<hkpConstraintListener*> m_listeners; // 0x20
    hkStringPtr m_name; // 0x28
    unsigned long m_userData; // 0x2C
    hkConstraintInternal* m_internal; // 0x30
    unsigned int m_uid; // 0x34
};

// size 0x30
struct alignas(16) hkConstraintInternal
{
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkConstraintInternal);
    hkpEntity* getOtherEntity(const hkpEntity* entity) const; // 0x00DFFC30
    hkpEntity* getMasterEntity() const; // 0x00DFFC10
    hkpEntity* getSlaveEntity() const; // 0x00E0F890
    hkpConstraintAtom* getAtoms() const; // 0x00E0D340
    unsigned short getAtomsSize() const; // 0x00E0D350
    void clearConstraintInfo(); // 0x00E0F8A0
    void getConstraintInfo(hkpConstraintInfo& info) const; // 0x00E0ABB0
    void addConstraintInfo(const hkpConstraintInfo& delta); // 0x00E0ABD0
    void subConstraintInfo(const hkpConstraintInfo& delta); // 0x00E0AC00
    hkConstraintInternal(); // 0x00E0F8B0

    hkpConstraintInstance* m_constraint; // 0x00
    hkpEntity* m_entities[2]; // 0x04
public:
    hkpConstraintAtom* m_atoms; // 0x0C
    unsigned short m_atomsSize; // 0x10
public:
    unsigned char m_callbackRequest; // 0x12
    hkEnum<enum hkpConstraintInstance::ConstraintPriority,unsigned char> m_priority; // 0x13
    unsigned short m_numSolverResults; // 0x14
    unsigned int m_sizeOfSchemas; // 0x18
    unsigned int m_numSolverElemTemps; // 0x1C
    unsigned char m_whoIsMaster; // 0x20
    hkEnum<enum hkpConstraintInstance::InstanceType,unsigned char> m_constraintType; // 0x21
    hkpConstraintRuntime* m_runtime; // 0x24
    unsigned short m_runtimeSize; // 0x28
    unsigned short m_slaveIndex; // 0x2A
};
