// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/hkstructurelayout.cpp
//
// Havok 2014.2.5 (stub_impl.md S4, serializer SE; the host rules from SD), from rfg.exe: the platform layout rules
// and the member offset computation that retargets a class to them.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/hkstructurelayout.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/hkclass.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/hkclassmember.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/error/hkerror.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/io/ostream/hkostream.h"

// 0x016FA9E4: 4-byte pointers, little endian, no padding reuse, empty base class optimization. rfg.exe stores
// {4, 0, 0, 1} and its dynamic initializer (0x01098170) sets m_littleEndian from the first byte of an int 1
// (0x01373B78).
const hkStructureLayout::LayoutRules hkStructureLayout::HostLayoutRules = { sizeof(void*), 1, 0, 1 };

// The platform rules, 0x01373B7C-0x01373BE0: {bytes in pointer, little endian, reuse padding, empty base class}
const hkStructureLayout::LayoutRules hkStructureLayout::MsvcWin32LayoutRules = { 4, 1, 0, 1 }; // 0x01373B7C
const hkStructureLayout::LayoutRules hkStructureLayout::MsvcXboxLayoutRules = { 4, 1, 0, 1 }; // 0x01373B80
const hkStructureLayout::LayoutRules hkStructureLayout::MsvcAmd64LayoutRules = { 8, 1, 0, 1 }; // 0x01373B84
const hkStructureLayout::LayoutRules hkStructureLayout::CwPs2LayoutRules = { 4, 1, 0, 1 }; // 0x01373B88
const hkStructureLayout::LayoutRules hkStructureLayout::CwNgcLayoutRules = { 4, 0, 0, 1 }; // 0x01373B8C
const hkStructureLayout::LayoutRules hkStructureLayout::CwPspLayoutRules = { 4, 1, 0, 1 }; // 0x01373B90
const hkStructureLayout::LayoutRules hkStructureLayout::CwWiiLayoutRules = { 4, 0, 1, 1 }; // 0x01373B94
const hkStructureLayout::LayoutRules hkStructureLayout::Gcc32Ps2LayoutRules = { 4, 1, 1, 1 }; // 0x01373B98
const hkStructureLayout::LayoutRules hkStructureLayout::Gcc151PspLayoutRules = { 4, 1, 1, 1 }; // 0x01373B9C
const hkStructureLayout::LayoutRules hkStructureLayout::Gcc295LinuxLayoutRules = { 4, 1, 0, 0 }; // 0x01373BA0
const hkStructureLayout::LayoutRules hkStructureLayout::Gcc33LinuxLayoutRules = { 4, 1, 1, 1 }; // 0x01373BA4
const hkStructureLayout::LayoutRules hkStructureLayout::GccAmd64LinuxLayoutRules = { 8, 1, 1, 1 }; // 0x01373BA8
const hkStructureLayout::LayoutRules hkStructureLayout::GccAndroidLayoutRules = { 4, 1, 1, 1 }; // 0x01373BAC
const hkStructureLayout::LayoutRules hkStructureLayout::Sn31Ps2LayoutRules = { 4, 1, 0, 0 }; // 0x01373BB0
const hkStructureLayout::LayoutRules hkStructureLayout::Sn10PspLayoutRules = { 4, 1, 0, 1 }; // 0x01373BB4
const hkStructureLayout::LayoutRules hkStructureLayout::Sn393NgcLayoutRules = { 4, 0, 0, 0 }; // 0x01373BB8
const hkStructureLayout::LayoutRules hkStructureLayout::SnPsVitaLayoutRules = { 4, 1, 1, 1 }; // 0x01373BBC
const hkStructureLayout::LayoutRules hkStructureLayout::Xbox360LayoutRules = { 4, 0, 0, 1 }; // 0x01373BC0
const hkStructureLayout::LayoutRules hkStructureLayout::GccPs3LayoutRules = { 4, 0, 1, 1 }; // 0x01373BC4
const hkStructureLayout::LayoutRules hkStructureLayout::Gcc40MacPpcLayoutRules = { 4, 0, 1, 1 }; // 0x01373BC8
const hkStructureLayout::LayoutRules hkStructureLayout::Gcc40MacIntelLayoutRules = { 4, 1, 1, 1 }; // 0x01373BCC
const hkStructureLayout::LayoutRules hkStructureLayout::Gcc40iOSLayoutRules = { 4, 1, 1, 1 }; // 0x01373BD0
const hkStructureLayout::LayoutRules hkStructureLayout::Rvct3dsLayoutRules = { 4, 1, 1, 1 }; // 0x01373BD4
const hkStructureLayout::LayoutRules hkStructureLayout::GhsWiiULayoutRules = { 4, 0, 0, 1 }; // 0x01373BD8
const hkStructureLayout::LayoutRules hkStructureLayout::Ps4LayoutRules = { 8, 1, 1, 1 }; // 0x01373BDC
const hkStructureLayout::LayoutRules hkStructureLayout::XboxOneLayoutRules = { 8, 1, 0, 1 }; // 0x01373BE0

// 0x00F65AA0 (:67)
hkStructureLayout::hkStructureLayout() : m_rules(HostLayoutRules)
{
}

// 0x00F65AB0 (:110)
hkStructureLayout::hkStructureLayout(const LayoutRules& rules) : m_rules(rules)
{
}

// 0x00F65AE0 (:117): a member's size under the rules (the pointer-sized types and structs times the C array size)
static int computeMemberSize(const hkClassMember& member, const hkStructureLayout::LayoutRules& rules, const hkClassMember::Type type)
{
    int elemSize;
    switch (type)
    {
    case hkClassMember::TYPE_BOOL:
    case hkClassMember::TYPE_CHAR:
    case hkClassMember::TYPE_INT8:
    case hkClassMember::TYPE_UINT8:
    case hkClassMember::TYPE_INT16:
    case hkClassMember::TYPE_UINT16:
    case hkClassMember::TYPE_INT32:
    case hkClassMember::TYPE_UINT32:
    case hkClassMember::TYPE_INT64:
    case hkClassMember::TYPE_UINT64:
    case hkClassMember::TYPE_REAL:
    case hkClassMember::TYPE_VECTOR4:
    case hkClassMember::TYPE_QUATERNION:
    case hkClassMember::TYPE_MATRIX3:
    case hkClassMember::TYPE_ROTATION:
    case hkClassMember::TYPE_QSTRANSFORM:
    case hkClassMember::TYPE_MATRIX4:
    case hkClassMember::TYPE_TRANSFORM:
    case hkClassMember::TYPE_ENUM:
    case hkClassMember::TYPE_FLAGS:
    case hkClassMember::TYPE_HALF:
        return member.getSizeInBytes();
    case hkClassMember::TYPE_ZERO:
        return computeMemberSize(member, rules, member.getSubType());
    case hkClassMember::TYPE_POINTER:
    case hkClassMember::TYPE_FUNCTIONPOINTER:
    case hkClassMember::TYPE_CSTRING:
    case hkClassMember::TYPE_ULONG:
    case hkClassMember::TYPE_STRINGPTR:
        elemSize = rules.m_bytesInPointer;
        break;
    case hkClassMember::TYPE_ARRAY:
        return 4 + 4 + rules.m_bytesInPointer; // data pointer, size, capacity and flags
    case hkClassMember::TYPE_SIMPLEARRAY:
        return 4 + rules.m_bytesInPointer;
    case hkClassMember::TYPE_INPLACEARRAY:
        return 0;
    case hkClassMember::TYPE_STRUCT:
        elemSize = member.getStructClass().getObjectSize();
        break;
    case hkClassMember::TYPE_HOMOGENEOUSARRAY:
        return 2 * rules.m_bytesInPointer + 4; // class pointer, data pointer, size
    case hkClassMember::TYPE_VARIANT:
        return 2 * rules.m_bytesInPointer;
    case hkClassMember::TYPE_RELARRAY:
        return 4; // 16-bit size and offset
    default:
    {
        char buf[512];
        hkErrStream os(buf, sizeof(buf));
        os << "Unknown class member type in structureLayout::getMemberSize().";
        if (hkErrorFwd::messageError(0x50A18B58, buf, "Util\\hkStructureLayout.cpp", 201) != 0)
        {
            __debugbreak(); // HK_BREAKPOINT
        }
        return 0;
    }
    }
    return elemSize * (member.getCstyleArraySize() != 0 ? member.getCstyleArraySize() : 1);
}

// 0x00F65C70 (:209): the natural alignment of a member under the rules (structs: their members' largest), raised
// by the member's alignment flags; -1 for the in-place array and unknown types
static int getLayoutAlignment(const hkClassMember& member, hkClassMember::Type memType, int bytesInPointer)
{
    int align = -1;
    switch (memType)
    {
    case hkClassMember::TYPE_VOID:
    case hkClassMember::TYPE_BOOL:
    case hkClassMember::TYPE_CHAR:
    case hkClassMember::TYPE_INT8:
    case hkClassMember::TYPE_UINT8:
    case hkClassMember::TYPE_INT16:
    case hkClassMember::TYPE_UINT16:
    case hkClassMember::TYPE_INT32:
    case hkClassMember::TYPE_UINT32:
    case hkClassMember::TYPE_INT64:
    case hkClassMember::TYPE_UINT64:
    case hkClassMember::TYPE_REAL:
    case hkClassMember::TYPE_VECTOR4:
    case hkClassMember::TYPE_QUATERNION:
    case hkClassMember::TYPE_MATRIX3:
    case hkClassMember::TYPE_ROTATION:
    case hkClassMember::TYPE_QSTRANSFORM:
    case hkClassMember::TYPE_MATRIX4:
    case hkClassMember::TYPE_TRANSFORM:
    case hkClassMember::TYPE_ENUM:
    case hkClassMember::TYPE_FLAGS:
    case hkClassMember::TYPE_HALF:
    case hkClassMember::TYPE_RELARRAY:
        align = member.getAlignment();
        break;
    case hkClassMember::TYPE_ZERO:
        align = getLayoutAlignment(member, member.getSubType(), bytesInPointer);
        break;
    case hkClassMember::TYPE_POINTER:
    case hkClassMember::TYPE_FUNCTIONPOINTER:
    case hkClassMember::TYPE_ARRAY:
    case hkClassMember::TYPE_SIMPLEARRAY:
    case hkClassMember::TYPE_HOMOGENEOUSARRAY:
    case hkClassMember::TYPE_VARIANT:
    case hkClassMember::TYPE_CSTRING:
    case hkClassMember::TYPE_ULONG:
    case hkClassMember::TYPE_STRINGPTR:
        align = bytesInPointer;
        break;
    case hkClassMember::TYPE_STRUCT:
    {
        align = 1;
        const hkClass& sclass = member.getStructClass();
        for (int i = 0; i < sclass.getNumMembers(); i++)
        {
            const hkClassMember& m = sclass.getMember(i);
            int a = getLayoutAlignment(m, m.getType(), bytesInPointer);
            if (a > align)
            {
                align = a;
            }
        }
        break;
    }
    default:
        break;
    }
    const int flags = member.getFlags().get(hkClassMember::ALIGN_8 | hkClassMember::ALIGN_16 | hkClassMember::ALIGN_32);
    if (flags != 0)
    {
        int flagAlign;
        if (flags == hkClassMember::ALIGN_32)
        {
            flagAlign = 32;
        }
        else
        {
            flagAlign = flags == hkClassMember::ALIGN_16 ? 16 : 8; // combinations count as ALIGN_8
        }
        if (flagAlign > align)
        {
            align = flagAlign;
        }
    }
    return align;
}

// 0x00F65AD0 (:295)
static void s_setOffset(hkClassMember& member, int currentOffset)
{
    member.setOffset(currentOffset);
}

// 0x00F65D80 (:304): the member classes first (when recursing), then the class's hierarchy from the root down:
// interface (vtable) pointers, then each declared member at its alignment. The alignment kept for padding the
// object sizes is the largest seen so far in the hierarchy, not the class's own.
static void retargetClassInplace(hkClass& topLevelClass, const hkStructureLayout::LayoutRules& rules, hkPointerMap<hkClass const*,int,hkContainerHeapAllocator>& classesDone, hkBool32 recurse)
{
    classesDone.insert(&topLevelClass, 0);
    if (recurse)
    {
        const int numMembers = topLevelClass.getNumMembers();
        for (int i = 0; i < numMembers; i++)
        {
            const hkClassMember& m = topLevelClass.getMember(i);
            if (m.hasClass() && !classesDone.hasKey(&m.getStructClass()))
            {
                retargetClassInplace(const_cast<hkClass&>(m.getStructClass()), rules, classesDone, recurse);
            }
        }
    }

    hkArray<hkClass*,hkContainerTempAllocator> hierarchy;
    hierarchy.reserve(16);
    for (hkClass* c = &topLevelClass; c != nullptr; c = c->getParent())
    {
        hierarchy.insertAt(0, c);
    }

    const int bytesInPointer = rules.m_bytesInPointer;
    int currentOffset = 0;
    int classAlign = 1;
    for (int ci = 0; ci < hierarchy.getSize(); ci++)
    {
        hkClass& klass = *hierarchy[ci];
        for (int i = 0; i < klass.getNumDeclaredInterfaces(); i++)
        {
            if (currentOffset % bytesInPointer != 0)
            {
                currentOffset += bytesInPointer - currentOffset % bytesInPointer;
            }
            currentOffset += bytesInPointer;
            if (bytesInPointer > classAlign)
            {
                classAlign = bytesInPointer;
            }
        }
        for (int i = 0; i < klass.getNumDeclaredMembers(); i++)
        {
            hkClassMember& member = const_cast<hkClassMember&>(klass.getDeclaredMember(i));
            if (currentOffset == 0 && ci != 0 && !rules.m_emptyBaseClassOptimization)
            {
                currentOffset = 1; // an empty base takes a byte
            }
            const int align = getLayoutAlignment(member, member.getType(), rules.m_bytesInPointer);
            if (align > classAlign)
            {
                classAlign = align;
            }
            if (currentOffset % align != 0)
            {
                currentOffset += align - currentOffset % align;
            }
            if (member.getOffset() != currentOffset)
            {
                s_setOffset(member, currentOffset);
            }
            currentOffset += computeMemberSize(member, rules, member.getType());
        }
        int paddedSize = currentOffset;
        if (paddedSize % classAlign != 0)
        {
            paddedSize += classAlign - paddedSize % classAlign;
        }
        const int objectSize = paddedSize != 0 ? paddedSize : 1;
        if (klass.getObjectSize() != objectSize)
        {
            klass.setObjectSize(objectSize);
        }
        if (!rules.m_reusePaddingOptimization)
        {
            currentOffset = paddedSize; // derived members start after the padding
        }
    }
}

// 0x00F65FF0 (:426)
void hkStructureLayout::computeMemberOffsetsInplace(hkClass& klass, hkPointerMap<hkClass const*,int,hkContainerHeapAllocator>& classesDone, hkBool32 recurse) const
{
    if (!classesDone.hasKey(&klass))
    {
        retargetClassInplace(klass, m_rules, classesDone, recurse);
    }
}
