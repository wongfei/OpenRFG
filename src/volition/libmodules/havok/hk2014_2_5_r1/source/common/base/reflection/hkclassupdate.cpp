// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/hkclassupdate.cpp
//
// Havok 2014.2.5 (stub_impl.md S4, serializer SB): brings class descriptors loaded from old packfiles up to the
// current metadata layout in place, from rfg.exe. Each class (and every class its members refer to) is updated once
// per step; the steps done are recorded per class in the map.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/hkclass.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/string/hkstring.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/hkclassmember.h"

// hkclassreflection.cpp / the version 1 metadata layout (reflection data)
extern const hkClass hkClassMemberClass;
extern const hkClass hkClassVersion1Class;

namespace
{
    // The update steps a class went through (values from rfg.exe).
    enum
    {
        UPDATED_VERSION_1 = 1,
        UPDATED_POINTER_CHAR = 2,
        UPDATED_CONSTRAINT_INSTANCE_300 = 4,
        UPDATED_ENUM_SIZE_AND_TYPE_ZERO = 8,
    };

    typedef void (*MemberCallback)(hkClassMember&);

    // 0x00FC2A50 (:32): the callback on every declared member of klass, its parents and the classes they refer to
    void walkMembers(hkClass* klass, hkClass::UpdateFlagFromClassMap& updateFlagFromClass, int walkingFlag,
                     MemberCallback memberCallback)
    {
        int flags = updateFlagFromClass.getWithDefault(klass, 0);
        if (flags & walkingFlag)
        {
            return;
        }
        updateFlagFromClass.insert(klass, flags | walkingFlag);
        for (int i = 0; i < klass->getNumDeclaredMembers(); ++i)
        {
            hkClassMember& member = const_cast<hkClassMember&>(klass->getDeclaredMember(i));
            if (member.getClass() != nullptr)
            {
                walkMembers(const_cast<hkClass*>(member.getClass()), updateFlagFromClass, walkingFlag, memberCallback);
            }
            memberCallback(member);
        }
        if (klass->getParent() != nullptr)
        {
            walkMembers(klass->getParent(), updateFlagFromClass, walkingFlag, memberCallback);
        }
    }

    // 0x00FC29C0 (:56): char* used to be a pointer to char
    void updatePointerCharToCString(hkClassMember& klassMem)
    {
        if (klassMem.getType() == hkClassMember::TYPE_POINTER && klassMem.getSubType() == hkClassMember::TYPE_CHAR)
        {
            klassMem.setType(hkClassMember::TYPE_CSTRING);
            klassMem.setSubType(hkClassMember::TYPE_VOID);
        }
    }

    // 0x00FC2B00 (:66): version 1 descriptors had a hasVtable flag; a virtual root class now counts the vtable as an
    // interface. The flag is cleared as a 32-bit word on the classes above the root but tested as a byte on the root
    // (original).
    void updateClassVersion1Inplace(hkClass* classInOut, hkClass::UpdateFlagFromClassMap& updateFlagFromClass)
    {
        int flags = updateFlagFromClass.getWithDefault(classInOut, 0);
        if (flags & UPDATED_VERSION_1)
        {
            return;
        }
        updateFlagFromClass.insert(classInOut, flags | UPDATED_VERSION_1);
        int hasVtableOffset = hkClassVersion1Class.getMemberByName("hasVtable")->getOffset();
        hkClass* c = classInOut;
        while (c->getParent() != nullptr)
        {
            *reinterpret_cast<hkInt32*>(reinterpret_cast<char*>(c) + hasVtableOffset) = 0;
            c = c->getParent();
        }
        if (*(reinterpret_cast<char*>(c) + hasVtableOffset) != 0)
        {
            int interfacesOffset = hkClassVersion1Class.getMemberByName("numImplementedInterfaces")->getOffset();
            *reinterpret_cast<int*>(reinterpret_cast<char*>(c) + interfacesOffset) += 1;
        }
        *reinterpret_cast<hkInt32*>(reinterpret_cast<char*>(c) + hasVtableOffset) = 0;
        for (int i = 0; i < classInOut->getNumDeclaredMembers(); ++i)
        {
            const hkClassMember& member = classInOut->getDeclaredMember(i);
            if (member.m_class != nullptr)
            {
                updateClassVersion1Inplace(const_cast<hkClass*>(&member.getStructClass()), updateFlagFromClass);
            }
        }
        if (classInOut->getParent() != nullptr)
        {
            updateClassVersion1Inplace(classInOut->getParent(), updateFlagFromClass);
        }
    }

    // 0x00FC2C00 (:105): hkpConstraintInstance's third member became an array of structs (its subtype, the fifth
    // member of hkClassMember's own descriptor, set to TYPE_STRUCT)
    void updateConstraintInstance300(const hkClass* classInOut, hkClass::UpdateFlagFromClassMap& updateFlagFromClass)
    {
        int flags = updateFlagFromClass.getWithDefault(classInOut, 0);
        if ((flags & UPDATED_CONSTRAINT_INSTANCE_300) == 0)
        {
            updateFlagFromClass.insert(classInOut, flags | UPDATED_CONSTRAINT_INSTANCE_300);
            if (hkString::strCmp(classInOut->getName(), "hkpConstraintInstance") == 0)
            {
                const hkClassMember& member = classInOut->getDeclaredMember(2);
                const hkClassMember& subtypeMember = hkClassMemberClass.getMember(4);
                *(const_cast<hkUint8*>(reinterpret_cast<const hkUint8*>(&member)) + subtypeMember.getOffset()) =
                    hkClassMember::TYPE_STRUCT;
            }
        }
    }

    // 0x00FC29E0 (:122): TYPE_ZERO members became serialize-ignored ones, the enum storage size moved from the
    // DEPRECATED_ENUM flags to the subtype
    void updateEnumSizeAndTypeZero(hkClassMember& member)
    {
        if (member.getType() == hkClassMember::TYPE_ZERO)
        {
            member.setType(member.getSubType());
            member.setSubType(hkClassMember::TYPE_VOID);
            member.getFlags().orWith(hkClassMember::SERIALIZE_IGNORED);
        }
        if (member.getType() == hkClassMember::TYPE_ENUM || member.getType() == hkClassMember::TYPE_FLAGS)
        {
            if (member.getFlags().get(hkClassMember::DEPRECATED_ENUM_8))
            {
                member.setSubType(hkClassMember::TYPE_UINT8);
                member.getFlags().clear(hkClassMember::DEPRECATED_ENUM_8);
            }
            if (member.getFlags().get(hkClassMember::DEPRECATED_ENUM_16))
            {
                member.setSubType(hkClassMember::TYPE_UINT16);
                member.getFlags().clear(hkClassMember::DEPRECATED_ENUM_16);
            }
            if (member.getFlags().get(hkClassMember::DEPRECATED_ENUM_32))
            {
                member.setSubType(hkClassMember::TYPE_UINT32);
                member.getFlags().clear(hkClassMember::DEPRECATED_ENUM_32);
            }
        }
    }
}

// 0x00FC2C60 (:149): the steps a descriptor of the given metadata version needs
void hkClass::updateMetadataInplace(hkClass* c, UpdateFlagFromClassMap& updated, int sourceVersion)
{
    if (sourceVersion == 1)
    {
        updateClassVersion1Inplace(c, updated);
        updateConstraintInstance300(c, updated);
    }
    if (sourceVersion <= 3)
    {
        walkMembers(c, updated, UPDATED_POINTER_CHAR, updatePointerCharToCString);
    }
    if (sourceVersion < 5)
    {
        walkMembers(c, updated, UPDATED_ENUM_SIZE_AND_TYPE_ZERO, updateEnumSizeAndTypeZero);
    }
}

// 0x00FC2CC0 (:166): a null-terminated list
void hkClass::updateMetadataInplace(hkClass** c, int sourceVersion)
{
    UpdateFlagFromClassMap updated;
    for (int i = 0; c[i] != nullptr; ++i)
    {
        updateMetadataInplace(c[i], updated, sourceVersion);
    }
}
