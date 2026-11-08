// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/hkclass.cpp
//
// Havok 2014.2.5 (stub_impl.md S4, serializer SB): the reflection class descriptor (lookups through the parent
// chain, defaults, signatures), from rfg.exe.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/hkclass.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/string/hkstring.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/hkclassenum.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/hkclassmember.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/hkcustomattributes.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/io/oarchive/hkoarchive.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/io/writer/crc/hkcrcstreamwriter.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hktypedunion.h"

// 0x0134CAB0: the value of a member whose default is HK_CLASS_ZERO_DEFAULT (all zeros, as large as any basic type)
static const char s_defaultClassBuffer[64] = {};

// 0x00CF3080 (:47)
hkClass::hkClass(const char* className, const hkClass* parentClass, int objectSizeInBytes,
                 const hkClass** implementedInterfaces, int numImplementedInterfaces, const hkClassEnum* declaredEnums,
                 int numDeclaredEnums, const hkClassMember* members, int numMembers, const void* defaults,
                 const hkCustomAttributes* attrs, unsigned int flags, unsigned int version)
    : m_name(className), m_parent(parentClass), m_objectSize(objectSizeInBytes),
      m_numImplementedInterfaces(numImplementedInterfaces), m_declaredEnums(declaredEnums),
      m_numDeclaredEnums(numDeclaredEnums), m_declaredMembers(members), m_numDeclaredMembers(numMembers),
      m_defaults(defaults), m_attributes(attrs), m_flags(flags), m_describedVersion(version)
{
}

// 0x00CF29A0 (:63)
const char* hkClass::getName() const
{
    return m_name;
}

// 0x00CF29B0 (:71): the same descriptor or the same name
bool hkClass::equals(const hkClass* other) const
{
    if (other == nullptr)
    {
        return false;
    }
    if (this == other)
    {
        return true;
    }
    return hkString::strCmp(getName(), other->getName()) == 0;
}

// 0x00CF29F0 (:82)
const hkClass* hkClass::getParent() const
{
    return m_parent;
}

// 0x00CF2A00 (:87)
hkClass* hkClass::getParent()
{
    return const_cast<hkClass*>(m_parent);
}

// 0x00CF2A10 (:92): this class counts
int hkClass::getInheritanceDepth() const
{
    int depth = 0;
    for (const hkClass* c = this; c != nullptr; c = c->m_parent)
    {
        ++depth;
    }
    return depth;
}

// 0x00CF2A20 (:104): k or one of its parents has this class's name
hkBool hkClass::isSuperClass(const hkClass& k) const
{
    const hkClass* c = &k;
    while (c != nullptr)
    {
        if (hkString::strCmp(c->getName(), getName()) == 0)
        {
            return true;
        }
        c = c->getParent();
    }
    return false;
}

// 0x00CF2A80 (:129)
int hkClass::getNumInterfaces() const
{
    int num = m_numImplementedInterfaces;
    for (const hkClass* c = m_parent; c != nullptr; c = c->m_parent)
    {
        num += c->m_numImplementedInterfaces;
    }
    return num;
}

// 0x00CF2AA0 (:134): interfaces are not reflected
const hkClass* hkClass::getInterface(int i) const
{
    return nullptr;
}

// 0x00CF2AB0 (:139)
const hkClass* hkClass::getDeclaredInterface(int i) const
{
    return nullptr;
}

// 0x00CF2AC0 (:144)
int hkClass::getNumDeclaredInterfaces() const
{
    return m_numImplementedInterfaces;
}

// 0x00CF2AD0 (:149)
int hkClass::getNumEnums() const
{
    int num = m_numDeclaredEnums;
    for (const hkClass* c = m_parent; c != nullptr; c = c->m_parent)
    {
        num += c->m_numDeclaredEnums;
    }
    return num;
}

// 0x00CF2AF0 (:154): the enums of the root class come first; an index out of range gives this class's first enum
const hkClassEnum& hkClass::getEnum(int enumIndex) const
{
    int localIndex = enumIndex - getNumEnums();
    for (const hkClass* c = this; c != nullptr; c = c->m_parent)
    {
        localIndex += c->m_numDeclaredEnums;
        if (localIndex >= 0)
        {
            return c->m_declaredEnums[localIndex];
        }
    }
    return m_declaredEnums[0];
}

// 0x00CF2B30 (:173)
const hkClassEnum* hkClass::getEnumByName(const char* name) const
{
    for (int i = 0; i < getNumEnums(); ++i)
    {
        const hkClassEnum& e = getEnum(i);
        if (hkString::strCmp(e.getName(), name) == 0)
        {
            return &e;
        }
    }
    return nullptr;
}

// 0x00CF30E0 (:186)
const hkClassEnum* hkClass::getDeclaredEnumByName(const char* name) const
{
    for (int i = 0; i < getNumDeclaredEnums(); ++i)
    {
        const hkClassEnum& e = getDeclaredEnum(i);
        if (hkString::strCmp(e.getName(), name) == 0)
        {
            return &e;
        }
    }
    return nullptr;
}

// 0x00CF2B80 (:199)
const hkClassEnum& hkClass::getDeclaredEnum(int enumIndex) const
{
    return m_declaredEnums[enumIndex];
}

// 0x00CF2B90 (:205)
int hkClass::getNumDeclaredEnums() const
{
    return m_numDeclaredEnums;
}

// 0x00CF2BA0 (:210)
int hkClass::getNumMembers() const
{
    int num = m_numDeclaredMembers;
    for (const hkClass* c = m_parent; c != nullptr; c = c->m_parent)
    {
        num += c->m_numDeclaredMembers;
    }
    return num;
}

// 0x00CF2BC0 (:215): the members of the root class come first; an index out of range gives this class's first member
const hkClassMember& hkClass::getMember(int memberIndex) const
{
    int localIndex = memberIndex - getNumMembers();
    for (const hkClass* c = this; c != nullptr; c = c->m_parent)
    {
        localIndex += c->m_numDeclaredMembers;
        if (localIndex >= 0)
        {
            return c->m_declaredMembers[localIndex];
        }
    }
    return m_declaredMembers[0];
}

// 0x00CF2C00 (:234)
hkClassMember& hkClass::getMember(int memberIndex)
{
    return const_cast<hkClassMember&>(static_cast<const hkClass*>(this)->getMember(memberIndex));
}

// 0x00CF2C10 (:240)
int hkClass::getNumDeclaredMembers() const
{
    return m_numDeclaredMembers;
}

// 0x00CF2C20 (:245)
const hkClassMember& hkClass::getDeclaredMember(int i) const
{
    return m_declaredMembers[i];
}

// 0x00CF2C30 (:251)
const hkClassMember* hkClass::getDeclaredMemberByName(const char* name) const
{
    for (int i = 0; i < getNumDeclaredMembers(); ++i)
    {
        const hkClassMember& m = getDeclaredMember(i);
        if (hkString::strCmp(m.getName(), name) == 0)
        {
            return &m;
        }
    }
    return nullptr;
}

// 0x00CF2C80 (:264)
const hkClassMember* hkClass::getMemberByName(const char* name) const
{
    for (int i = 0; i < getNumMembers(); ++i)
    {
        const hkClassMember& m = getMember(i);
        if (hkString::strCmp(m.getName(), name) == 0)
        {
            return &m;
        }
    }
    return nullptr;
}

// 0x00CF2CD0 (:277)
int hkClass::getMemberIndexByName(const char* name) const
{
    for (int i = 0; i < getNumMembers(); ++i)
    {
        if (hkString::strCmp(getMember(i).getName(), name) == 0)
        {
            return i;
        }
    }
    return -1;
}

// 0x00CF2D20 (:291)
int hkClass::getMemberIndexByNameCaseInsensitive(const char* name) const
{
    for (int i = 0; i < getNumMembers(); ++i)
    {
        if (hkString::strCasecmp(getMember(i).getName(), name) == 0)
        {
            return i;
        }
    }
    return -1;
}

// 0x00CF2D70 (:304)
int hkClass::getDeclaredMemberIndexByName(const char* name) const
{
    for (int i = 0; i < getNumDeclaredMembers(); ++i)
    {
        if (hkString::strCmp(getDeclaredMember(i).getName(), name) == 0)
        {
            return i;
        }
    }
    return -1;
}

// 0x00CF2DC0 (:317)
int hkClass::getObjectSize() const
{
    return m_objectSize;
}

// 0x00CF2DD0 (:322)
void hkClass::setObjectSize(int size)
{
    m_objectSize = size;
}

// 0x00CF2DE0 (:327): a virtual class's root implements an interface (the vtable)
hkBool hkClass::hasVtable() const
{
    const hkClass* c = this;
    while (c->getParent() != nullptr)
    {
        c = c->getParent();
    }
    return c->m_numImplementedInterfaces != 0;
}

// 0x00CF34B0 (:340): the CRC-32 of the class's signature, with its parents' unless SIGNATURE_LOCAL
unsigned int hkClass::getSignature(int signatureFlags) const
{
    hkCrc32StreamWriter crc;
    const hkClass* c = this;
    while (c != nullptr)
    {
        c->writeSignature(&crc);
        if (signatureFlags & SIGNATURE_LOCAL)
        {
            break;
        }
        c = c->getParent();
    }
    return crc.getCrc();
}

// 0x00CF2E10 (:354)
int hkClass::getDescribedVersion() const
{
    return m_describedVersion;
}

// 0x00CF2E20 (:365): the declaring class's default of a member (m_defaults holds one byte offset per declared member
// from the defaults block, -1 for none, HK_CLASS_ZERO_DEFAULT for zeros)
hkResult hkClass::retrieveMember(int memberIndex, const void*& defaultOut, const hkClassMember*& memberOut) const
{
    int localIndex = memberIndex - getNumMembers();
    for (const hkClass* c = this; c != nullptr; c = c->m_parent)
    {
        localIndex += c->m_numDeclaredMembers;
        if (localIndex >= 0)
        {
            const int* defaults = static_cast<const int*>(c->m_defaults);
            if (defaults != nullptr)
            {
                int offset = defaults[localIndex];
                if (offset >= 0 || offset == hkClassMember::HK_CLASS_ZERO_DEFAULT)
                {
                    defaultOut = offset == hkClassMember::HK_CLASS_ZERO_DEFAULT
                        ? static_cast<const void*>(s_defaultClassBuffer)
                        : reinterpret_cast<const char*>(defaults) + offset;
                    memberOut = &c->m_declaredMembers[localIndex];
                    return HK_SUCCESS;
                }
            }
            return HK_FAILURE;
        }
    }
    return HK_FAILURE;
}

// 0x00CF2EA0 (:393)
hkBool32 hkClass::hasDefault(int memberIndex) const
{
    const void* dummyDefault;
    const hkClassMember* dummyMember;
    return retrieveMember(memberIndex, dummyDefault, dummyMember) == HK_SUCCESS;
}

// 0x00CF2ED0 (:400)
hkBool32 hkClass::hasDeclaredDefault(int declaredIndex) const
{
    if (m_defaults != nullptr)
    {
        int offset = static_cast<const int*>(m_defaults)[declaredIndex];
        return offset >= 0 || offset == hkClassMember::HK_CLASS_ZERO_DEFAULT;
    }
    return false;
}

// 0x00CF2F00 (:407)
const void* hkClass::getDefault(int memberIndex) const
{
    const void* def = nullptr;
    const hkClassMember* mem;
    return retrieveMember(memberIndex, def, mem) == HK_SUCCESS ? def : nullptr;
}

// 0x00CF2F40 (:419): the default's bytes, as many as the member holds
hkResult hkClass::getDefault(int memberIndex, hkStreamWriter* writer) const
{
    const void* def = nullptr;
    const hkClassMember* mem = nullptr;
    hkResult res = retrieveMember(memberIndex, def, mem);
    if (res == HK_SUCCESS)
    {
        writer->write(def, mem->getSizeInBytes());
    }
    return res;
}

// 0x00CF2FA0 (:432)
hkResult hkClass::getDeclaredDefault(int declaredIndex, hkStreamWriter* writer) const
{
    if (hasDeclaredDefault(declaredIndex))
    {
        writer->write(getDeclaredDefault(declaredIndex), m_declaredMembers[declaredIndex].getSizeInBytes());
        return HK_SUCCESS;
    }
    return HK_FAILURE;
}

// 0x00CF3130 (:451): a pointer default is the address of its storage here, unlike getDeclaredDefault (original)
hkResult hkClass::getDefault(int memberIndex, hkTypedUnion& value) const
{
    const void* def = nullptr;
    const hkClassMember* mem = nullptr;
    hkResult res = retrieveMember(memberIndex, def, mem);
    if (res == HK_SUCCESS)
    {
        switch (mem->getType())
        {
            case hkClassMember::TYPE_POINTER:
                value.setObject(def, mem->getClass());
                break;
            case hkClassMember::TYPE_ENUM:
                value.setEnum(mem->getEnumValue(def), &mem->getEnumClass());
                break;
            default:
                return value.setSimple(def, mem->getType());
        }
    }
    return res;
}

// 0x00CF3010 (:477)
const void* hkClass::getDeclaredDefault(int declaredIndex) const
{
    if (m_defaults != nullptr)
    {
        int offset = static_cast<const int*>(m_defaults)[declaredIndex];
        if (offset == hkClassMember::HK_CLASS_ZERO_DEFAULT)
        {
            return s_defaultClassBuffer;
        }
        if (offset >= 0)
        {
            return static_cast<const char*>(m_defaults) + offset;
        }
    }
    return nullptr;
}

// 0x00CF31F0 (:491): a pointer default is the pointer stored in the defaults block; flags count as enums
hkResult hkClass::getDeclaredDefault(int declaredIndex, hkTypedUnion& value) const
{
    if (hasDeclaredDefault(declaredIndex))
    {
        const void* memberAddress = getDeclaredDefault(declaredIndex);
        const hkClassMember& mem = m_declaredMembers[declaredIndex];
        switch (mem.getType())
        {
            case hkClassMember::TYPE_POINTER:
                value.setObject(*static_cast<void* const*>(memberAddress), mem.getClass());
                return HK_SUCCESS;
            case hkClassMember::TYPE_ENUM:
            case hkClassMember::TYPE_FLAGS:
                value.setEnum(mem.getEnumValue(memberAddress), &mem.getEnumClass());
                return HK_SUCCESS;
            default:
                return value.setSimple(memberAddress, mem.getType());
        }
    }
    return HK_FAILURE;
}

// 0x00CF32C0 (:524): interfaces, enums and members (each with the classes and enum it refers to); the enum size bits
// and a serialize-ignored member are written in their pre-2010 form (an ignored member as TYPE_ZERO of its type) so
// the signatures of old classes do not change
void hkClass::writeSignature(hkStreamWriter* w) const
{
    hkOArchive oa(w, false);
    oa.write32(m_numImplementedInterfaces);
    for (int i = 0; i < m_numDeclaredEnums; ++i)
    {
        m_declaredEnums[i].writeSignature(w);
    }
    oa.write32(m_numDeclaredEnums);
    for (int i = 0; i < m_numDeclaredMembers; ++i)
    {
        const hkClassMember& member = m_declaredMembers[i];
        hkClassMember::Type type = member.getType();
        hkClassMember::Flags flags = member.getFlags();
        hkUint8 subtype;
        if (type == hkClassMember::TYPE_ENUM || type == hkClassMember::TYPE_FLAGS)
        {
            // DEPRECATED_SIZE_8/16/32: the storage size in bytes times 8
            flags.xorWith(static_cast<unsigned short>(
                hkClassMember::getClassMemberTypeProperties(member.getSubType()).m_size * 8));
            subtype = 0;
        }
        else
        {
            subtype = static_cast<hkUint8>(member.getSubType());
        }
        hkUint16 extraSubtype = 0;
        if (flags.get(hkClassMember::SERIALIZE_IGNORED))
        {
            flags.xorWith(hkClassMember::SERIALIZE_IGNORED);
            if (subtype != 0)
            {
                extraSubtype = subtype;
            }
            subtype = static_cast<hkUint8>(type);
            type = hkClassMember::TYPE_ZERO;
        }
        if (member.m_class != nullptr && member.getType() != hkClassMember::TYPE_POINTER &&
            member.getSubType() != hkClassMember::TYPE_POINTER)
        {
            for (const hkClass* c = member.getClass(); c != nullptr; c = c->getParent())
            {
                c->writeSignature(w);
            }
        }
        if (member.m_enum != nullptr)
        {
            member.getEnumClass().writeSignature(w);
        }
        const char* name = member.getName();
        oa.writeRaw(name, hkString::strLen(name));
        oa.write16(static_cast<short>(type));
        oa.write16(subtype);
        if (extraSubtype != 0)
        {
            oa.write16(static_cast<short>(extraSubtype));
        }
        oa.write16(member.m_cArraySize);
        oa.write16(static_cast<short>(flags.get()));
    }
    oa.write32(m_numDeclaredMembers);
}

// 0x00CF3040 (:605)
const hkVariant* hkClass::getAttribute(const char* id) const
{
    return m_attributes != nullptr ? m_attributes->getAttribute(id) : nullptr;
}

// 0x00CF3060 (:610)
const hkClass::Flags& hkClass::getFlags() const
{
    return m_flags;
}

// 0x00CF3070 (:615)
hkClass::Flags& hkClass::getFlags()
{
    return m_flags;
}
