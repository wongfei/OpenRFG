// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/entity/hkpentity.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/geometry/localframe/hklocalframe.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkrefptr.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/common/hkpmaterial.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/constraint/hkpconstraintinstance.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/motion/rigid/hkpkeyframedrigidmotion.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/world/hkpworldobject.h"

class hkBool;
class hkClass;
class hkFinishLoadedObjectFlag;
struct hkInternalClassMember;
class hkMotionState;
class hkSpuCollisionCallbackUtil;
class hkdBreakableBody;
class hkpAction;
class hkpBreakableBody;
class hkpContactListener;
class hkpDynamicsContactMgr;
class hkpEntityActivationListener;
class hkpEntityListener;
class hkpMotion;
class hkpShape;
class hkpSimulationIsland;
class hkpWorld;

// size 0x220
class alignas(16) hkpEntity : public hkpWorldObject
{
public:
    enum SpuCollisionCallbackEventFilter
    {
        SPU_SEND_NONE = 0,
        SPU_SEND_CONTACT_POINT_ADDED = 1,
        SPU_SEND_CONTACT_POINT_PROCESS = 2,
        SPU_SEND_CONTACT_POINT_REMOVED = 4,
        SPU_SEND_CONTACT_POINT_ADDED_OR_PROCESS = 3,
    };

    // size 0x8
    class SmallArraySerializeOverrideType
    {
    public:
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(SmallArraySerializeOverrideType);
        static const hkClass& staticClass(); // 0x00DEEC90

        static const hkInternalClassMember Members[];
        void* m_data; // 0x00
        unsigned short m_size; // 0x04
        unsigned short m_capacityAndFlags; // 0x06
    };

    // size 0x8
    struct SpuCollisionCallback
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(SpuCollisionCallback); // new(unsigned int n, void* p) 0x00DEED70
        static const hkClass& staticClass(); // 0x00DEECB0
        SpuCollisionCallback(hkFinishLoadedObjectFlag flag); // 0x00DDE770
        SpuCollisionCallback(); // 0x00DDE760

        static const hkInternalClassMember Members[];
        hkSpuCollisionCallbackUtil* m_util; // 0x00
        unsigned short m_capacity; // 0x04
        unsigned char m_eventFilter; // 0x06
        unsigned char m_userFilter; // 0x07
    };

    // size 0x10
    struct ExtendedListeners
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(ExtendedListeners); // new(unsigned int nbytes) 0x00DDECA0, delete(void* p, unsigned int nbytes) 0x00DDECC0
        static const hkClass& staticClass(); // 0x00DEECD0
        ExtendedListeners(); // 0x00DDF930
        ~ExtendedListeners(); // 0x00DDF8C0

        static const hkInternalClassMember Members[];
        hkSmallArray<hkpEntityActivationListener*> m_activationListeners; // 0x00
        hkSmallArray<hkpEntityListener*> m_entityListeners; // 0x08
    };

    static const hkClass& staticClass(); // 0x00DEECE0
    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int, void* p) 0x00DEED60, delete(void* p, unsigned int nbytes) 0x00DDFAD0
    virtual ~hkpEntity(); // vtbl+0x00 0x00DDE1B0
    void addEntityListener(hkpEntityListener* el); // 0x00DDD950
    void removeEntityListener(hkpEntityListener* el); // 0x00DDDA20
    void addEntityActivationListener(hkpEntityActivationListener* el); // 0x00DDDAC0
    void removeEntityActivationListener(hkpEntityActivationListener* el); // 0x00DDD240
    void addContactListener(hkpContactListener* cl); // 0x00DDD6F0
    void removeContactListener(hkpContactListener* cl); // 0x00DDD290
    const hkSmallArray<hkpEntityListener*>& getEntityListeners() const; // 0x00F29500
    const hkSmallArray<hkpEntityActivationListener*>& getEntityActivationListeners() const;
    const hkSmallArray<hkpContactListener*>& getContactListeners() const; // 0x00E1F0D0
    bool areContactListenersAdded() const; // 0x00E1B660
    unsigned short getContactPointCallbackDelay() const; // 0x00E19EB0
    void setContactPointCallbackDelay(unsigned short);
    const hkpMaterial& getMaterial() const; // 0x00DF2980
    hkpMaterial& getMaterial(); // 0x0080CD80 (hkpentity.inl:81)
    hkBool isFixed() const; // 0x00B71280 (hkpentity.inl:97)
    hkBool isFixedOrKeyframed() const; // 0x00DDECF0
    unsigned int getUid() const; // 0x00DDE790
    hkpDynamicsContactMgr* findContactMgrTo(const hkpEntity* entity) const; // 0x00DDD2F0
    hkdBreakableBody* getBreakableBody() const;
    void activate(); // 0x00DDD760
    void requestDeactivation(); // 0x00DDD370
    void deactivate(); // 0x00DDD3B0
    void activateAsCriticalOperation(); // 0x00DDD7D0
    void requestDeactivationAsCriticalOperation(); // 0x00DDD4B0
    void deactivateAsCriticalOperation(); // 0x00DDD4F0
    hkBool isActive() const; // 0x00DDD340
    int getNumActions() const; // 0x00A2C680 (hkpentity.inl:25)
    hkpAction* getAction(int i); // 0x00A2C670 (hkpentity.inl:20)
    int getNumConstraints() const; // 0x00DDD530
    const hkpConstraintInstance* getConstraint(int i) const; // 0x00DDD570
    hkpConstraintInstance* getConstraint(int i); // 0x00DDD540
    void getAllConstraints(hkArray<hkpConstraintInstance*,hkContainerHeapAllocator>& constraints); // 0x00DDD820
    const hkSmallArray<hkConstraintInternal>& getConstraintMasters() const; // 0x00DDE7A0
    hkSmallArray<hkConstraintInternal>& getConstraintMastersRw(); // 0x00DDE7B0
    const hkArray<hkViewPtr<hkpConstraintInstance>,hkContainerHeapAllocator>& getConstraintSlaves() const; // 0x00DDE7C0
    void sortConstraintsSlavesDeterministically(); // 0x00DDD8C0
    void setCachedShapeData(const hkpWorld* world, const hkpShape* shape); // 0x00DDD620
    void updateCachedAabb(); // 0x00DDD1F0
public:
    const hkSmallArray<hkConstraintInternal>& getConstraintMastersImpl() const; // 0x00DDD220
    hkSmallArray<hkConstraintInternal>& getConstraintMastersRwImpl(); // 0x00DDD230
    const hkArray<hkViewPtr<hkpConstraintInstance>,hkContainerHeapAllocator>& getConstraintSlavesImpl() const; // 0x00DDD810
public:
    hkpEntity(hkFinishLoadedObjectFlag flag); // 0x00DDE0F0
public:
    hkpEntity(const hkpShape* shape); // 0x00DDDFC0
public:
    void afterReflectNew(); // 0x00DDD5A0
    hkpMotion* getMotion(); // 0x0080CD70 (hkpentity.inl:10)
    hkpSimulationIsland* getSimulationIsland() const; // 0x00DDE780
    virtual hkMotionState* getMotionState(); // vtbl+0x18 0x00DDFAC0
    virtual void deallocateInternalArrays(); // vtbl+0x1C 0x00DDDB80

    static const hkInternalClassMember Members[]; // 0x0135DF60
public:
    hkpMaterial m_material; // 0x88
public:
    void* m_limitContactImpulseUtilAndFlag; // 0x94
    float m_damageMultiplier; // 0x98
    hkpBreakableBody* m_breakableBody; // 0x9C
    unsigned int m_solverData; // 0xA0
    unsigned short m_storageIndex; // 0xA4
public:
    unsigned short m_contactPointCallbackDelay; // 0xA6
    hkSmallArray<hkConstraintInternal> m_constraintsMaster; // 0xA8
    hkArray<hkViewPtr<hkpConstraintInstance>,hkContainerHeapAllocator> m_constraintsSlave; // 0xB0
    hkArray<unsigned char,hkContainerHeapAllocator> m_constraintRuntime; // 0xBC
    hkpSimulationIsland* m_simulationIsland; // 0xC8
public:
    signed char m_autoRemoveLevel; // 0xCC
    unsigned char m_numShapeKeysInContactPointProperties; // 0xCD
    unsigned char m_responseModifierFlags; // 0xCE
    unsigned int m_uid; // 0xD0
    SpuCollisionCallback m_spuCollisionCallback; // 0xD4
    hkpMaxSizeMotion m_motion; // 0xE0
public:
    hkSmallArray<hkpContactListener*> m_contactListeners; // 0x200
    hkSmallArray<hkpAction*> m_actions; // 0x208
public:
    hkRefPtr<hkLocalFrame> m_localFrame; // 0x210
public:
    ExtendedListeners* m_extendedListeners; // 0x214
public:
    unsigned int m_npData; // 0x218
};
