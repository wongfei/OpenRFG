// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/compat/deprecated/util/hkbindingclassnameregistry.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (stub_impl.md S4, serializer).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/pointermap/hkpointermap.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/stringmap/hkstringmap.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/compat/deprecated/version/hkversionregistry.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/hkchainedclassnameregistry.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/hkserializemultimap.h"

class hkClass;
class hkClassNameRegistry;
struct hkInternalClassEnum;
struct hkInternalClassMember;

// size 0x4C
class hkBindingClassNameRegistry : public hkChainedClassNameRegistry
{
public:
    // size 0xC
    class ClassAllocationsTracker
    {
    public:
        // size 0x10
        struct ClassData
        {
            HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(ClassData); // delete(void* p, unsigned int nbytes) 0x00FB22D0
            ClassData(const ClassData&);
            ClassData(); // 0x00FB2910
            ~ClassData(); // 0x00FB2640

            hkClass* allocatedClass; // 0x00
            hkPointerMap<hkInternalClassEnum*,unsigned int,hkContainerHeapAllocator> enumsAllocatedForMembers; // 0x04
        };

        // size 0x1C
        struct UnresolvedClassPointerTracker
        {
            HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(UnresolvedClassPointerTracker);
            UnresolvedClassPointerTracker(); // 0x00FB1A90
            ~UnresolvedClassPointerTracker(); // 0x00FB1820
            void classPointedBy(const hkClass* newClass, const hkClass** fromWhere); // 0x00FB1880
            void replaceClassAndRemove(const hkClass* oldClass, const hkClass* newClass); // 0x00FB1770
            void resolveClasses(hkPointerMap<hkClass const*,hkClass const*,hkContainerHeapAllocator>& classFromUnresolved); // 0x00FB17B0
            int getSize() const; // 0x00FB15A0

            hkSerializeMultiMap<void*,hkClass const**,hkPointerMap<void*,int,hkContainerHeapAllocator> > m_pointers; // 0x00
        };

        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(ClassAllocationsTracker);
        ClassAllocationsTracker(); // 0x00FB1FB0
        ~ClassAllocationsTracker(); // 0x00FB1FD0
        hkClass* restoreClassHierarchy(const hkClass& classToCopy, const hkBindingClassNameRegistry* classRegistry, const hkStringMap<char const*,hkContainerHeapAllocator>& oldNameFromNewName, hkStringMap<hkClass*,hkContainerHeapAllocator>& classesInOut); // 0x00FB1D00
    public:
        hkInternalClassEnum* allocateEnums(const hkInternalClassEnum* enumsToCopy, int numEnums); // 0x00FB12D0
        hkInternalClassMember* allocateMembers(const hkInternalClassMember* membersToCopy, int numMembers, const hkInternalClassEnum* originalEnums, const hkInternalClassEnum* copiedEnums, int numAvailableEnums, hkPointerMap<hkInternalClassEnum*,unsigned int,hkContainerHeapAllocator>& enumsAllocatedForMembersOut, UnresolvedClassPointerTracker& unresolvedClassPointersInOut); // 0x00FB18E0
        void* allocateDefaults(const hkClass& klass, void* defaultsToCopy); // 0x00FB13C0
        hkClass* allocateClass(const hkClass& classToDuplicate, const char* classNameToSet, UnresolvedClassPointerTracker& unresolvedClassPointersOut); // 0x00FB1AB0
        void deallocateEnums(hkInternalClassEnum* enums, int numEnums); // 0x00FB1440
        void deallocateMembers(hkInternalClassMember* members, int numMembers, hkPointerMap<hkInternalClassEnum*,unsigned int,hkContainerHeapAllocator>& enumsAllocatedForMembersInOut); // 0x00FB15B0
        void deallocateClass(hkClass* classToDeallocate, hkPointerMap<hkInternalClassEnum*,unsigned int,hkContainerHeapAllocator>& enumsAllocatedForMembersInOut); // 0x00FB1660

        hkArray<ClassData,hkContainerHeapAllocator> m_trackedClassData; // 0x00
    };

    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int nbytes) 0x00FA3530, delete(void* p, unsigned int nbytes) 0x00FB2F30
    hkBindingClassNameRegistry(const hkVersionRegistry::ClassRename* renames, const hkClassNameRegistry* next); // 0x00FB2070
    virtual ~hkBindingClassNameRegistry(); // vtbl+0x00 0x00FB2120
    virtual const hkClass* getClassByName(const char* className) const; // vtbl+0x14 0x00FB1F10
    virtual const hkClass* getClassByNameNoRecurse(const char* className) const; // vtbl+0x30 0x00FB11C0
    virtual void registerRenames(const hkStringMap<char const*,hkContainerHeapAllocator>& newNameFromOldNameMap); // vtbl+0x34 0x00FB1250
    virtual void registerRenames(const hkVersionRegistry::ClassRename* renames); // vtbl+0x38 0x00FB1200
public:
    const char* getNewName(const char* oldName) const; // 0x00FB11E0

public:
    ClassAllocationsTracker m_tracker; // 0x20
    hkStringMap<char const*,hkContainerHeapAllocator> m_newNameFromOldName; // 0x2C
    hkStringMap<char const*,hkContainerHeapAllocator> m_oldNameFromNewName; // 0x3C
};
