// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/copier/hkobjectcopier.cpp
//
// Havok 2014.2.5 (stub_impl.md S4, serializer SE), from rfg.exe (the decompile is LTCG-garbled; read from the asm):
// copies an object from one class layout to another (another version of the class and/or another platform's
// layout rules), writing the body, then its extras (strings, arrays, embedded objects), and recording the
// relocations of every pointer it writes. Offsets in the relocations are relative to the output stream's position
// at the copy start (classStart/extrasStart).

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/copier/hkobjectcopier.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/resource/hkobjectresource.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/string/hkstring.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/hkclass.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/hkclassenum.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/hkclassmember.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/error/hkerror.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/io/oarchive/hkoarchive.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/io/ostream/hkostream.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/io/writer/array/hkarraystreamwriter.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/io/writer/hkstreamwriter.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/serialize/hkrelocationinfo.h"

#include <cstring>

extern const hkClass hkClassClass;

namespace
{
    // 0x00F7B830 (:30)
    bool inRange(int num, int lo, int hi)
    {
        return lo <= num && num < hi;
    }

    // 0x00F7D800 (:35)
    template<typename T>
    T min2(T a, T b)
    {
        return b <= a ? b : a;
    }
}

// 0x00F7B850 (:43): bytes are swapped when the endianness differs
hkObjectCopier::hkObjectCopier(const hkStructureLayout& layoutIn, const hkStructureLayout& layoutOut, hkFlags<enum ObjectCopierFlagBits,unsigned int> flags)
    : m_layoutIn(layoutIn), m_layoutOut(layoutOut), m_flags(flags)
{
    m_byteSwap = m_layoutIn.getRules().m_littleEndian != m_layoutOut.getRules().m_littleEndian;
}

// 0x00F7B890 (:49)
hkObjectCopier::~hkObjectCopier()
{
}

// 0x00F7BCF0 (:74): zeros up to the next multiple of pad (a power of two)
static void objectCopierPadUp(hkStreamWriter* w, int pad)
{
    const int offset = w->tell();
    hkLocalArray<char> buf(pad);
    buf.setSize(pad, 0);
    if ((offset & (pad - 1)) != 0)
    {
        w->write(buf.begin(), pad - (offset & (pad - 1)));
    }
}

// 0x00F7B8A0 (:86)
static int objectCopier_calcCArraySize(const hkClassMember& member)
{
    return member.getCstyleArraySize() != 0 ? member.getCstyleArraySize() : 1;
}

// 0x00F7BB10 (:91): the zero array grows as needed (it only ever holds zeros)
static int writeZeros(hkOArchive& oa, int numZeros, hkLocalArray<char>& zeroArray)
{
    zeroArray.setSize(numZeros, 0);
    return oa.writeRaw(zeroArray.begin(), numZeros);
}

// 0x00F7BB70 (:100): a member's size in zeros; an hkArray is written as empty and not owned. TYPE_ZERO,
// TYPE_CSTRING and TYPE_RELARRAY are reported as unknown and write nothing.
void hkObjectCopier::writeZero(hkOArchive& oa, const hkClassMember& memberOut, hkLocalArray<char>& zeroArray)
{
    const hkClassMember::Type type = memberOut.getType();
    const int bytesInPointer = m_layoutOut.getRules().m_bytesInPointer;
    int size = 0;
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
        size = memberOut.getSizeInBytes();
        break;
    case hkClassMember::TYPE_POINTER:
    case hkClassMember::TYPE_FUNCTIONPOINTER:
    case hkClassMember::TYPE_ULONG:
    case hkClassMember::TYPE_STRINGPTR:
        size = objectCopier_calcCArraySize(memberOut) * bytesInPointer;
        break;
    case hkClassMember::TYPE_ARRAY:
    case hkClassMember::TYPE_INPLACEARRAY:
    case hkClassMember::TYPE_SIMPLEARRAY:
    case hkClassMember::TYPE_HOMOGENEOUSARRAY:
        size = (type == hkClassMember::TYPE_HOMOGENEOUSARRAY ? bytesInPointer : 0) + bytesInPointer + 4;
        if (type == hkClassMember::TYPE_ARRAY)
        {
            // data pointer and size, then the capacity with DONT_DEALLOCATE_FLAG
            const char zeros[16] = {};
            oa.writeRaw(zeros, size);
            oa.write32u(0x80000000);
            return;
        }
        break;
    case hkClassMember::TYPE_STRUCT:
        size = memberOut.getStructClass().getObjectSize() * objectCopier_calcCArraySize(memberOut);
        break;
    case hkClassMember::TYPE_VARIANT:
        size = objectCopier_calcCArraySize(memberOut) * bytesInPointer * 2;
        break;
    default:
    {
        char buf[512];
        hkErrStream os(buf, sizeof(buf));
        os << "Unknown class member type found!";
        if (hkErrorFwd::messageError(0x5EF4E5A4, buf, "Copier\\hkObjectCopier.cpp", 178) != 0)
        {
            __debugbreak(); // HK_BREAKPOINT
        }
        break;
    }
    }
    writeZeros(oa, size, zeroArray);
}

// 0x00F7BA50 (:188): host ulongs (4 bytes) widened to the target's pointer size
static void writeUlongArray(hkOArchive& oa, int elemsize, int nelem, const void* startAddress)
{
    if (elemsize == 4)
    {
        oa.writeArrayGeneric(startAddress, 4, nelem);
        return;
    }
    hkLocalArray<hkUint64> buf(nelem);
    const unsigned long* in = static_cast<const unsigned long*>(startAddress);
    for (int i = 0; i < nelem; i++)
    {
        buf.begin()[i] = in[i];
    }
    oa.writeArrayGeneric(buf.begin(), elemsize, nelem);
}

// 0x00F7B8C0 (:214): the vector and matrix types as arrays of floats
static void writePodArray(hkOArchive& oa, const hkClassMember::Type mtype, int elemsize, int nelem, const void* startAddress)
{
    switch (mtype)
    {
    case hkClassMember::TYPE_VECTOR4:
    case hkClassMember::TYPE_QUATERNION:
    case hkClassMember::TYPE_MATRIX3:
    case hkClassMember::TYPE_ROTATION:
    case hkClassMember::TYPE_QSTRANSFORM:
    case hkClassMember::TYPE_MATRIX4:
    case hkClassMember::TYPE_TRANSFORM:
        nelem *= elemsize / 4;
        elemsize = 4;
        // fall through
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
    case hkClassMember::TYPE_HALF:
        oa.writeArrayGeneric(startAddress, elemsize, nelem);
        return;
    default:
    {
        char buf[512];
        hkErrStream os(buf, sizeof(buf));
        os << "Unknown class member found during write of plain data array.";
        if (hkErrorFwd::messageError(0x747E1E03, buf, "Copier\\hkObjectCopier.cpp", 259) != 0)
        {
            __debugbreak(); // HK_BREAKPOINT
        }
        return;
    }
    }
}

// 0x00F7B980 (:265): the member's default from the class; a struct member then gets its own members' defaults
// written after it, once per C array element (as rfg.exe does)
static hkResult applyDefaultsFor(const hkClass& klass, const hkClassMember& mem, int memberIndex, hkStreamWriter* writer)
{
    hkResult result = klass.getDefault(memberIndex, writer);
    if (mem.getType() == hkClassMember::TYPE_STRUCT)
    {
        const hkClass* sclass = mem.getClass();
        if (sclass != nullptr)
        {
            for (int n = objectCopier_calcCArraySize(mem); n > 0; n--)
            {
                for (int i = 0; i < sclass->getNumMembers(); i++)
                {
                    if (applyDefaultsFor(*sclass, sclass->getMember(i), i, writer) == HK_SUCCESS)
                    {
                        result = HK_SUCCESS;
                    }
                }
            }
        }
    }
    return result;
}

// 0x00F7BA20 (:290): same type and subtype (enums: any storage)
hkBool32 hkObjectCopier::areMembersCompatible(const hkClassMember& src, const hkClassMember& dst)
{
    return src.getType() == dst.getType() && (src.getSubType() == dst.getSubType() || src.getType() == hkClassMember::TYPE_ENUM);
}

namespace
{
    // A relative array's data, written after the body (saveBody)
    struct RelArrayItem
    {
        const hkClassMember* memberIn; // 0x00
        const hkClassMember* memberOut; // 0x04
        int offset; // 0x08: of the member from the class start
        const void* data; // 0x0C
        hkUint16 size; // 0x10
    };
}

// 0x00F7BE60 (:313): each member of klassOut at its offset, from klassIn's member of the same name (defaults for
// missing or incompatible ones); then the relative arrays' data after the object, 16-aligned
void hkObjectCopier::saveBody(const void* dataIn, const hkClass& klassIn, hkOArchive& dataOut, const hkClass& klassOut, hkLocalArray<char>& zeroArray)
{
    const int classStart = dataOut.getStreamWriter()->tell();
    hkArray<RelArrayItem,hkContainerTempAllocator> relArrays;
    const int bytesInPointer = m_layoutOut.getRules().m_bytesInPointer;
    const int numMembers = klassOut.getNumMembers();
    for (int memberIndex = 0; memberIndex < numMembers; memberIndex++)
    {
        const hkClassMember& memberOut = klassOut.getMember(memberIndex);
        writeZeros(dataOut, memberOut.getOffset() + classStart - dataOut.getStreamWriter()->tell(), zeroArray);
        if (memberOut.getFlags().get(hkClassMember::SERIALIZE_IGNORED))
        {
            if (m_flags.get(FLAG_RESPECT_SERIALIZE_IGNORED))
            {
                writeZero(dataOut, memberOut, zeroArray);
                continue;
            }
            if (m_flags.get(FLAG_APPLY_DEFAULT_IF_SERIALIZE_IGNORED))
            {
                applyDefaultsFor(klassOut, memberOut, memberIndex, dataOut.getStreamWriter());
                continue;
            }
        }
        const hkClassMember* memberInPtr = &klassIn == &klassOut ? &memberOut : klassIn.getMemberByName(memberOut.getName());
        if (memberInPtr == nullptr || !areMembersCompatible(*memberInPtr, memberOut))
        {
            applyDefaultsFor(klassOut, memberOut, memberIndex, dataOut.getStreamWriter());
            continue;
        }
        const hkClassMember& memberIn = *memberInPtr;
        const char* data = static_cast<const char*>(dataIn) + memberIn.getOffset();
        switch (memberOut.getType())
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
        case hkClassMember::TYPE_HALF:
        {
            const int nIn = objectCopier_calcCArraySize(memberIn);
            const int nOut = objectCopier_calcCArraySize(memberOut);
            const int sizeOut = memberOut.getSizeInBytes();
            writePodArray(dataOut, memberOut.getType(), sizeOut / nOut, min2(nIn, nOut), data);
            break;
        }
        case hkClassMember::TYPE_ZERO:
            break;
        case hkClassMember::TYPE_POINTER:
        case hkClassMember::TYPE_FUNCTIONPOINTER:
        case hkClassMember::TYPE_CSTRING:
        case hkClassMember::TYPE_STRINGPTR:
        {
            // null here, the targets are written and relocated by saveExtras
            const hkUint64 zero = 0;
            for (int n = objectCopier_calcCArraySize(memberOut); n > 0; n--)
            {
                dataOut.writeRaw(&zero, bytesInPointer);
            }
            break;
        }
        case hkClassMember::TYPE_ARRAY:
        case hkClassMember::TYPE_SIMPLEARRAY:
        case hkClassMember::TYPE_HOMOGENEOUSARRAY:
        {
            // {[class pointer], data pointer, size[, capacity and flags]}: the pointers are relocated by saveExtras
            int size = 0;
            const hkUint64 zero = 0;
            bool sized = false;
            if (memberOut.getType() == hkClassMember::TYPE_HOMOGENEOUSARRAY)
            {
                dataOut.writeArrayGeneric(&zero, bytesInPointer, 1);
                if (*reinterpret_cast<const void* const*>(data) != nullptr)
                {
                    size = *reinterpret_cast<const int*>(data + 2 * sizeof(void*));
                    sized = true;
                }
                else
                {
                    char buf[512];
                    hkErrStream os(buf, sizeof(buf));
                    os << "Can't copy homogeneous array. No hkClass for " << klassIn.getName() << "::" << memberIn.getName() << ".";
                    hkErrorFwd::messageWarning(0xABBA5A55, buf, "Copier\\hkObjectCopier.cpp", 462);
                }
            }
            if (!sized)
            {
                size = memberOut.getSubType() == hkClassMember::TYPE_VOID ? 0 : *reinterpret_cast<const int*>(data + sizeof(void*));
            }
            dataOut.writeArrayGeneric(&zero, bytesInPointer, 1);
            dataOut.write32(size);
            if (memberOut.getType() == hkClassMember::TYPE_ARRAY)
            {
                dataOut.write32(size | 0x80000000); // capacity == size, not owned (DONT_DEALLOCATE_FLAG)
            }
            break;
        }
        case hkClassMember::TYPE_ENUM:
            // each value mapped by name from the input enum to the output's (0 when unknown)
            if (memberIn.hasEnumClass() && memberOut.hasEnumClass())
            {
                const int nIn = objectCopier_calcCArraySize(memberIn);
                const int n = min2(nIn, objectCopier_calcCArraySize(memberOut));
                const hkClassEnum& enumIn = memberIn.getEnumClass();
                const hkClassEnum& enumOut = memberOut.getEnumClass();
                const int elemSize = memberIn.getSizeInBytes() / nIn; // the input's storage size, also written
                const char* p = data;
                for (int i = 0; i < n; i++)
                {
                    const int valueIn = memberIn.getEnumValue(p);
                    int valueOut = 0;
                    const char* name;
                    if (enumIn.getNameOfValue(valueIn, &name) == HK_SUCCESS)
                    {
                        enumOut.getValueOfName(name, &valueOut);
                    }
                    switch (elemSize)
                    {
                    case 1: dataOut.write8(static_cast<signed char>(valueOut)); break;
                    case 2: dataOut.write16(static_cast<short>(valueOut)); break;
                    case 4: dataOut.write32(valueOut); break;
                    }
                    p += elemSize;
                }
            }
            break;
        case hkClassMember::TYPE_STRUCT:
        {
            const hkClass* classIn = memberIn.getClass();
            const hkClass* classOut = memberOut.getClass();
            if (classIn != nullptr && classOut != nullptr)
            {
                const int n = min2(objectCopier_calcCArraySize(memberIn), objectCopier_calcCArraySize(memberOut));
                for (int i = 0; i < n; i++)
                {
                    saveBody(data + classIn->getObjectSize() * i, *classIn, dataOut, *classOut, zeroArray);
                }
            }
            break;
        }
        case hkClassMember::TYPE_VARIANT:
        {
            const hkUint64 zero[2] = {};
            for (int n = min2(objectCopier_calcCArraySize(memberIn), objectCopier_calcCArraySize(memberOut)); n > 0; n--)
            {
                dataOut.writeArrayGeneric(zero, bytesInPointer, 2);
            }
            break;
        }
        case hkClassMember::TYPE_ULONG:
            writeUlongArray(dataOut, bytesInPointer, min2(objectCopier_calcCArraySize(memberIn), objectCopier_calcCArraySize(memberOut)), data);
            break;
        case hkClassMember::TYPE_FLAGS:
            // the named bits mapped from the input enum to the output's, unnamed bits kept when the output names them
            if (memberIn.hasEnumClass() && memberOut.hasEnumClass())
            {
                const int nIn = objectCopier_calcCArraySize(memberIn);
                const int n = min2(nIn, objectCopier_calcCArraySize(memberOut));
                const hkClassEnum& enumIn = memberIn.getEnumClass();
                const hkClassEnum& enumOut = memberOut.getEnumClass();
                const int elemSize = memberIn.getSizeInBytes() / nIn;
                const char* p = data;
                int bitValue = 0; // original bug kept: a failed lookup ors in the last value found (rfg.exe's stack slot is never set)
                for (int i = 0; i < n; i++)
                {
                    const int valueIn = memberIn.getEnumValue(p);
                    hkArray<const char*,hkContainerHeapAllocator> bits;
                    int leftover = 0;
                    enumIn.decomposeFlags(valueIn, bits, leftover);
                    int valueOut = 0;
                    for (int b = 0; b < bits.getSize(); b++)
                    {
                        if (enumOut.getValueOfName(bits[b], &bitValue) == HK_SUCCESS)
                        {
                            valueOut |= bitValue;
                        }
                        else
                        {
                            leftover |= bitValue;
                        }
                    }
                    if (leftover != 0)
                    {
                        int leftoverOut = 0;
                        enumOut.decomposeFlags(leftover, bits, leftoverOut);
                        valueOut |= leftoverOut;
                    }
                    switch (elemSize)
                    {
                    case 1: dataOut.write8(static_cast<signed char>(valueOut)); break;
                    case 2: dataOut.write16(static_cast<short>(valueOut)); break;
                    case 4: dataOut.write32(valueOut); break;
                    }
                    p += elemSize;
                }
            }
            break;
        case hkClassMember::TYPE_RELARRAY:
        {
            // {hkUint16 size, hkUint16 offset}: written after the body
            RelArrayItem& item = relArrays.expandOne();
            item.memberIn = &memberIn;
            item.memberOut = &memberOut;
            item.offset = dataOut.getStreamWriter()->tell() - classStart;
            item.data = data + reinterpret_cast<const hkUint16*>(data)[1];
            item.size = reinterpret_cast<const hkUint16*>(data)[0];
            dataOut.write16u(0);
            dataOut.write16u(0);
            break;
        }
        default:
        {
            char buf[512];
            hkErrStream os(buf, sizeof(buf));
            os << "Unknown class member found during write of data.";
            if (hkErrorFwd::messageError(0x641E3E03, buf, "Copier\\hkObjectCopier.cpp", 585) != 0)
            {
                __debugbreak(); // HK_BREAKPOINT
            }
            applyDefaultsFor(klassOut, memberOut, memberIndex, dataOut.getStreamWriter());
            break;
        }
        }
    }
    writeZeros(dataOut, classStart + klassOut.getObjectSize() - dataOut.getStreamWriter()->tell(), zeroArray);

    for (int r = 0; r < relArrays.getSize(); r++)
    {
        const RelArrayItem& item = relArrays[r];
        objectCopierPadUp(dataOut.getStreamWriter(), 16);
        const int dataOffset = dataOut.getStreamWriter()->tell() - classStart;
        // the seek takes the offset from the class start as absolute: the copier's buffer starts at the class
        dataOut.getStreamWriter()->seek(item.offset, hkStreamWriter::STREAM_SET);
        dataOut.write16u(item.size);
        dataOut.write16u(static_cast<hkUint16>(dataOffset - item.offset));
        dataOut.getStreamWriter()->seek(0, hkStreamWriter::STREAM_END);
        if (item.size == 0)
        {
            continue;
        }
        const hkClassMember::Type subType = item.memberOut->getSubType();
        if (subType == hkClassMember::TYPE_VOID || subType == hkClassMember::TYPE_POINTER || subType == hkClassMember::TYPE_CSTRING ||
            subType == hkClassMember::TYPE_STRINGPTR)
        {
            continue; // not copied
        }
        if (subType == hkClassMember::TYPE_STRUCT)
        {
            const hkClass* classIn = item.memberIn->getClass();
            const hkClass* classOut = item.memberOut->getClass();
            if (classIn != nullptr && classOut != nullptr)
            {
                for (int i = 0; i < item.size; i++)
                {
                    saveBody(static_cast<const char*>(item.data) + classIn->getObjectSize() * i, *classIn, dataOut, *classOut, zeroArray);
                }
            }
        }
        else if (subType == hkClassMember::TYPE_VARIANT)
        {
            // not copied
        }
        else if (subType == hkClassMember::TYPE_ULONG)
        {
            writeUlongArray(dataOut, bytesInPointer, item.size, item.data);
        }
        else
        {
            writePodArray(dataOut, subType, item.memberOut->getArrayMemberSize(), item.size, item.data);
        }
    }
}

// 0x00F7C8E0 (:674): the string (2-aligned) with a local fixup from the member to it
static hkResult saveCstring(const char* dataIn, int klassMemOffset, int extrasStart, hkOArchive& dataOut, hkRelocationInfo& fixups)
{
    objectCopierPadUp(dataOut.getStreamWriter(), 2);
    fixups.addLocal(klassMemOffset, dataOut.getStreamWriter()->tell() + extrasStart);
    dataOut.writeRaw(dataIn, hkString::strLen(dataIn) + 1);
    return HK_SUCCESS;
}

// 0x00F7C970 (:692): what the members point to, after the body: strings, array data (with their elements' extras),
// embedded structs' extras; pointers to other objects become global fixups, each member's extras padded to 16
void hkObjectCopier::saveExtras(const void* dataIn, const hkClass& klassIn, hkOArchive& dataOut, const hkClass& klassOut, int classStart, int extrasStart, hkRelocationInfo& fixups, hkLocalArray<char>& zeroArray, int level)
{
    if (level == 0)
    {
        fixups.addFinish(classStart, klassOut.getName());
    }
    level++;
    const int bytesInPointer = m_layoutOut.getRules().m_bytesInPointer;
    const int numMembers = klassOut.getNumMembers();
    for (int memberIndex = 0; memberIndex < numMembers; memberIndex++)
    {
        const hkClassMember& memberOut = klassOut.getMember(memberIndex);
        const hkClassMember* memberInPtr = &klassIn == &klassOut ? &memberOut : klassIn.getMemberByName(memberOut.getName());
        if (memberInPtr == nullptr)
        {
            continue;
        }
        const hkClassMember& memberIn = *memberInPtr;
        const char* data = static_cast<const char*>(dataIn) + memberIn.getOffset();
        if (!areMembersCompatible(memberIn, memberOut) ||
            (m_flags.get(FLAG_RESPECT_SERIALIZE_IGNORED) && memberOut.getFlags().get(hkClassMember::SERIALIZE_IGNORED)))
        {
            continue;
        }
        const int memberOffset = memberOut.getOffset() + classStart;
        switch (memberOut.getType())
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
        case hkClassMember::TYPE_FUNCTIONPOINTER:
        case hkClassMember::TYPE_ENUM:
        case hkClassMember::TYPE_ULONG:
        case hkClassMember::TYPE_FLAGS:
        case hkClassMember::TYPE_HALF:
        case hkClassMember::TYPE_RELARRAY:
            break;
        case hkClassMember::TYPE_ZERO:
        {
            char buf[512];
            hkErrStream os(buf, sizeof(buf));
            os << "TYPE_ZERO should not occur.";
            if (hkErrorFwd::messageError(0x641E3E05, buf, "Copier\\hkObjectCopier.cpp", 1047) != 0)
            {
                __debugbreak(); // HK_BREAKPOINT
            }
            break;
        }
        case hkClassMember::TYPE_POINTER:
        {
            const int n = min2(objectCopier_calcCArraySize(memberIn), objectCopier_calcCArraySize(memberOut));
            const int stride = memberIn.getSizeInBytes() / objectCopier_calcCArraySize(memberIn);
            const char* p = data;
            for (int i = 0; i < n; i++)
            {
                const void* target = *reinterpret_cast<void* const*>(p);
                if (target != nullptr)
                {
                    const int offset = bytesInPointer * i + memberOut.getOffset();
                    if (memberOut.getSubType() == hkClassMember::TYPE_CHAR)
                    {
                        saveCstring(static_cast<const char*>(target), offset + classStart, extrasStart, dataOut, fixups);
                    }
                    else if (memberOut.getSubType() != hkClassMember::TYPE_VOID)
                    {
                        fixups.addGlobal(offset + classStart, const_cast<void*>(target), memberIn.getClass(), false);
                    }
                }
                p += stride;
            }
            break;
        }
        case hkClassMember::TYPE_ARRAY:
        case hkClassMember::TYPE_INPLACEARRAY:
        case hkClassMember::TYPE_SIMPLEARRAY:
        {
            const void* arrayData = *reinterpret_cast<void* const*>(data);
            const int arraySize = *reinterpret_cast<const int*>(data + sizeof(void*));
            const hkClassMember::Type subType = memberOut.getSubType();
            if (arraySize == 0 || subType == hkClassMember::TYPE_VOID)
            {
                break;
            }
            if (subType == hkClassMember::TYPE_POINTER)
            {
                fixups.addLocal(memberOffset, dataOut.getStreamWriter()->tell() + extrasStart);
                const hkUint64 zero = 0;
                const int stride = memberIn.getArrayMemberSize();
                const char* p = static_cast<const char*>(arrayData);
                for (int i = 0; i < arraySize; i++, p += stride)
                {
                    void* target = *reinterpret_cast<void* const*>(p);
                    if (target != nullptr && memberOut.getSubType() != hkClassMember::TYPE_VOID)
                    {
                        fixups.addGlobal(dataOut.getStreamWriter()->tell() + extrasStart, target, memberIn.getClass(), false);
                    }
                    dataOut.writeRaw(&zero, bytesInPointer);
                }
            }
            else if (subType == hkClassMember::TYPE_CSTRING || subType == hkClassMember::TYPE_STRINGPTR)
            {
                // the pointer array, then the strings; the input strings are read as 4-byte pointers
                fixups.addLocal(memberOffset, dataOut.getStreamWriter()->tell() + extrasStart);
                const hkUint64 zero = 0;
                const int arrayStart = dataOut.getStreamWriter()->tell() + extrasStart;
                for (int i = 0; i < arraySize; i++)
                {
                    dataOut.writeRaw(&zero, bytesInPointer);
                }
                const hkUlong* strings = static_cast<const hkUlong*>(arrayData);
                for (int i = 0; i < arraySize; i++)
                {
                    // an hkStringPtr's low bit is its ownership flag
                    const hkUlong s = subType == hkClassMember::TYPE_STRINGPTR ? (strings[i] & ~hkUlong(1)) : strings[i];
                    if (s != 0)
                    {
                        saveCstring(reinterpret_cast<const char*>(s), bytesInPointer * i + arrayStart, extrasStart, dataOut, fixups);
                    }
                }
            }
            else if (subType == hkClassMember::TYPE_STRUCT)
            {
                const int arrayStart = dataOut.getStreamWriter()->tell() + extrasStart;
                fixups.addLocal(memberOffset, arrayStart);
                const hkClass* classIn = memberIn.getClass();
                const hkClass* classOut = memberOut.getClass();
                if (classIn != nullptr && classOut != nullptr)
                {
                    const char* elems = static_cast<const char*>(arrayData);
                    for (int i = 0; i < arraySize; i++)
                    {
                        saveBody(elems + classIn->getObjectSize() * i, *classIn, dataOut, *classOut, zeroArray);
                    }
                    for (int i = 0; i < arraySize; i++)
                    {
                        saveExtras(elems + classIn->getObjectSize() * i, *classIn, dataOut, *classOut, classOut->getObjectSize() * i + arrayStart,
                                   extrasStart, fixups, zeroArray, level);
                    }
                }
            }
            else if (subType == hkClassMember::TYPE_VARIANT)
            {
                // {object, class} pairs; both relocated
                const int arrayStart = dataOut.getStreamWriter()->tell() + extrasStart;
                fixups.addLocal(memberOffset, arrayStart);
                for (int i = 0; i < arraySize; i++)
                {
                    const hkUint64 zero[2] = {};
                    dataOut.writeArrayGeneric(zero, bytesInPointer, 2);
                }
                const hkVariant* variants = static_cast<const hkVariant*>(arrayData);
                for (int i = 0; i < arraySize; i++)
                {
                    const int offset = bytesInPointer * i * 2;
                    if (variants[i].m_object != nullptr)
                    {
                        fixups.addGlobal(offset + arrayStart, variants[i].m_object, variants[i].m_class, false);
                    }
                    if (variants[i].m_class != nullptr)
                    {
                        fixups.addGlobal(bytesInPointer + offset + arrayStart, const_cast<hkClass*>(variants[i].m_class), &hkClassClass, false);
                    }
                }
            }
            else
            {
                fixups.addLocal(memberOffset, dataOut.getStreamWriter()->tell() + extrasStart);
                if (subType == hkClassMember::TYPE_ULONG)
                {
                    writeUlongArray(dataOut, bytesInPointer, arraySize, arrayData);
                }
                else
                {
                    writePodArray(dataOut, subType, memberOut.getArrayMemberSize(), arraySize, arrayData);
                }
            }
            break;
        }
        case hkClassMember::TYPE_STRUCT:
        {
            const hkClass* classIn = memberIn.getClass();
            const hkClass* classOut = memberOut.getClass();
            if (classIn != nullptr && classOut != nullptr)
            {
                const int n = min2(objectCopier_calcCArraySize(memberIn), objectCopier_calcCArraySize(memberOut));
                for (int i = 0; i < n; i++)
                {
                    saveExtras(data + classIn->getObjectSize() * i, *classIn, dataOut, *classOut, classOut->getObjectSize() * i + memberOffset,
                               extrasStart, fixups, zeroArray, level);
                }
            }
            break;
        }
        case hkClassMember::TYPE_HOMOGENEOUSARRAY:
        {
            // {const hkClass* klass, void* data, int size}: the class is a related global, the objects are copied
            // in the input class's own layout
            const hkClass* arrayClass = *reinterpret_cast<const hkClass* const*>(data);
            if (arrayClass == nullptr)
            {
                break;
            }
            fixups.addGlobal(memberOffset, const_cast<hkClass*>(arrayClass), &hkClassClass, true);
            const int arrayStart = dataOut.getStreamWriter()->tell() + extrasStart;
            fixups.addLocal(memberOut.getOffset() + bytesInPointer + classStart, arrayStart);
            const char* elems = *reinterpret_cast<const char* const*>(data + sizeof(void*));
            const int arraySize = *reinterpret_cast<const int*>(data + 2 * sizeof(void*));
            for (int i = 0; i < arraySize; i++)
            {
                saveBody(elems + arrayClass->getObjectSize() * i, *arrayClass, dataOut, *arrayClass, zeroArray);
            }
            for (int i = 0; i < arraySize; i++)
            {
                saveExtras(elems + arrayClass->getObjectSize() * i, *arrayClass, dataOut, *arrayClass, arrayClass->getObjectSize() * i + arrayStart,
                           extrasStart, fixups, zeroArray, level);
            }
            break;
        }
        case hkClassMember::TYPE_VARIANT:
        {
            // the input variants are read with the input layout's pointer size
            const int n = min2(objectCopier_calcCArraySize(memberIn), objectCopier_calcCArraySize(memberOut));
            const int bytesInPointerIn = m_layoutIn.getRules().m_bytesInPointer;
            for (int i = 0, k = 1; i < n; i++, k += 2)
            {
                void* object = *reinterpret_cast<void* const*>(data + i * bytesInPointerIn * 2);
                hkClass* klass = *reinterpret_cast<hkClass* const*>(data + k * bytesInPointerIn);
                if (object != nullptr)
                {
                    fixups.addGlobal(memberOffset + bytesInPointer * i * 2, object, klass, false);
                }
                if (klass != nullptr)
                {
                    fixups.addGlobal(bytesInPointer * k + memberOffset, klass, &hkClassClass, false);
                }
            }
            break;
        }
        case hkClassMember::TYPE_CSTRING:
        {
            const char* s = *reinterpret_cast<const char* const*>(data);
            if (s != nullptr)
            {
                saveCstring(s, memberOffset, extrasStart, dataOut, fixups);
            }
            break;
        }
        case hkClassMember::TYPE_STRINGPTR:
        {
            const hkUlong s = *reinterpret_cast<const hkUlong*>(data) & ~hkUlong(1); // the ownership flag
            if (s != 0)
            {
                saveCstring(reinterpret_cast<const char*>(s), memberOffset, extrasStart, dataOut, fixups);
            }
            break;
        }
        default:
        {
            char buf[512];
            hkErrStream os(buf, sizeof(buf));
            os << "Unknown class member found during write of data.";
            if (hkErrorFwd::messageError(0x641E3E05, buf, "Copier\\hkObjectCopier.cpp", 1052) != 0)
            {
                __debugbreak(); // HK_BREAKPOINT
            }
            break;
        }
        }
        objectCopierPadUp(dataOut.getStreamWriter(), 16);
    }
}

// 0x00F7D470 (:1062): the body and the extras go through a local buffer each (written to dataOut, 16-aligned), so
// the offsets the two passes record are relative to dataOut's positions at their starts
hkResult hkObjectCopier::copyObject(const void* dataIn, const hkClass& klassIn, hkStreamWriter* dataOut, const hkClass& klassOut, hkRelocationInfo& reloc)
{
    hkLocalArray<char> buffer(1024);
    hkLocalArray<char> zeroArray(16);
    hkArrayStreamWriter writer(&buffer, hkArrayStreamWriter::ARRAY_BORROW);
    hkOArchive oa(&writer, m_byteSwap);

    const int classStart = dataOut->tell();
    saveBody(dataIn, klassIn, oa, klassOut, zeroArray);
    dataOut->write(buffer.begin(), buffer.getSize());
    objectCopierPadUp(dataOut, 16);
    writer.clear();

    const int extrasStart = dataOut->tell();
    saveExtras(dataIn, klassIn, oa, klassOut, classStart, extrasStart, reloc, zeroArray, 0);
    dataOut->write(buffer.begin(), buffer.getSize());
    objectCopierPadUp(dataOut, 16);
    writer.clear();

    return oa.isOk() ? HK_SUCCESS : HK_FAILURE;
}
