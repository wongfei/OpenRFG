// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/hkclass.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (stub_impl.md S4, serializer).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/pointermap/hkpointermap.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"

class hkClassEnum;
class hkClassMember;
struct hkContainerHeapAllocator;
class hkCustomAttributes;
struct hkInternalClassMember;
class hkStreamWriter;
class hkTypedUnion;
struct hkVariant;

// size 0x30
class hkClass
{
public:
    enum SignatureFlags
    {
        SIGNATURE_LOCAL = 1,
    };

    enum FlagValues
    {
        FLAGS_NONE = 0,
        FLAGS_NOT_SERIALIZABLE = 1,
    };

    typedef hkFlags<enum FlagValues,unsigned int> Flags;
    typedef hkPointerMap<hkClass const*,int,hkContainerHeapAllocator> UpdateFlagFromClassMap;

    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkClass); // new(unsigned int n, void* p) 0x00FB11B0
    static const hkClass& staticClass(); // 0x00CEB8A0
    hkClass(const char* className, const hkClass* parentClass, int objectSizeInBytes, const hkClass** implementedInterfaces, int numImplementedInterfaces, const hkClassEnum* declaredEnums, int numDeclaredEnums, const hkClassMember* members, int numMembers, const void* defaults, const hkCustomAttributes* attrs, unsigned int flags, unsigned int version); // 0x00CF3080
    const char* getName() const; // 0x00CF29A0
    bool equals(const hkClass* other) const; // 0x00CF29B0
    hkClass* getParent(); // 0x00CF2A00
    const hkClass* getParent() const; // 0x00CF29F0
    int getInheritanceDepth() const; // 0x00CF2A10
    hkBool isSuperClass(const hkClass& k) const; // 0x00CF2A20
    const hkClass* getInterface(int i) const; // 0x00CF2AA0
    int getNumInterfaces() const; // 0x00CF2A80
    const hkClass* getDeclaredInterface(int i) const; // 0x00CF2AB0
    int getNumDeclaredInterfaces() const; // 0x00CF2AC0
    const hkClassEnum& getEnum(int enumIndex) const; // 0x00CF2AF0
    const hkClassEnum* getEnumByName(const char* name) const; // 0x00CF2B30
    int getNumEnums() const; // 0x00CF2AD0
    const hkClassEnum& getDeclaredEnum(int enumIndex) const; // 0x00CF2B80
    const hkClassEnum* getDeclaredEnumByName(const char* name) const; // 0x00CF30E0
    int getNumDeclaredEnums() const; // 0x00CF2B90
    hkClassMember& getMember(int memberIndex); // 0x00CF2C00
    const hkClassMember& getMember(int memberIndex) const; // 0x00CF2BC0
    const hkClassMember* getMemberByName(const char* name) const; // 0x00CF2C80
    int getMemberIndexByName(const char* name) const; // 0x00CF2CD0
    int getMemberIndexByNameCaseInsensitive(const char* name) const; // 0x00CF2D20
    int getNumMembers() const; // 0x00CF2BA0
    const hkClassMember& getDeclaredMember(int i) const; // 0x00CF2C20
    const hkClassMember* getDeclaredMemberByName(const char* name) const; // 0x00CF2C30
    int getDeclaredMemberIndexByName(const char* name) const; // 0x00CF2D70
    int getNumDeclaredMembers() const; // 0x00CF2C10
    int getObjectSize() const; // 0x00CF2DC0
    void setObjectSize(int size); // 0x00CF2DD0
    hkBool hasVtable() const; // 0x00CF2DE0
    unsigned int hasDefault(int memberIndex) const; // 0x00CF2EA0
    unsigned int hasDeclaredDefault(int declaredIndex) const; // 0x00CF2ED0
    hkResult getDefault(int memberIndex, hkTypedUnion& value) const; // 0x00CF3130
    const void* getDefault(int memberIndex) const; // 0x00CF2F00
    hkResult getDefault(int memberIndex, hkStreamWriter* writer) const; // 0x00CF2F40
    hkResult getDeclaredDefault(int declaredIndex, hkTypedUnion& value) const; // 0x00CF31F0
    const void* getDeclaredDefault(int declaredIndex) const; // 0x00CF3010
    hkResult getDeclaredDefault(int declaredIndex, hkStreamWriter* writer) const; // 0x00CF2FA0
    unsigned int getSignature(int signatureFlags) const; // 0x00CF34B0
    int getDescribedVersion() const; // 0x00CF2E10
    void writeSignature(hkStreamWriter* w) const; // 0x00CF32C0
    const hkVariant* getAttribute(const char* id) const; // 0x00CF3040
    hkFlags<enum FlagValues,unsigned int>& getFlags(); // 0x00CF3070
    const hkFlags<enum FlagValues,unsigned int>& getFlags() const; // 0x00CF3060
    static void updateMetadataInplace(hkClass* c, hkPointerMap<hkClass const*,int,hkContainerHeapAllocator>& updated, int sourceVersion); // 0x00FC2C60
    static void updateMetadataInplace(hkClass** c, int sourceVersion); // 0x00FC2CC0
public:
    hkResult retrieveMember(int memberIndex, const void*& defaultOut, const hkClassMember*& memberOut) const; // 0x00CF2E20

public:
    static const hkInternalClassMember Members[]; // 0x016F4078
public:
    const char* m_name; // 0x00
    const hkClass* m_parent; // 0x04
    int m_objectSize; // 0x08
    int m_numImplementedInterfaces; // 0x0C
    const hkClassEnum* m_declaredEnums; // 0x10
    int m_numDeclaredEnums; // 0x14
    const hkClassMember* m_declaredMembers; // 0x18
    int m_numDeclaredMembers; // 0x1C
    const void* m_defaults; // 0x20
    const hkCustomAttributes* m_attributes; // 0x24
    hkFlags<enum FlagValues,unsigned int> m_flags; // 0x28
    int m_describedVersion; // 0x2C
};
