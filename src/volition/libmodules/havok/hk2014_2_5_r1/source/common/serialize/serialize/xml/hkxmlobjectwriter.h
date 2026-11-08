// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/serialize/xml/hkxmlobjectwriter.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (stub_impl.md S4, serializer); inline members added by hand (nobuild).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/pointermap/hkpointermap.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/serialize/hkobjectwriter.h"

class hkClass;
class hkRelocationInfo;
class hkStreamWriter;

// size 0x2C: writes objects as "<hkparam>" lists of their reflected members
class hkXmlObjectWriter : public hkObjectWriter
{
public:
    // size 0x4: the text a pointer is written as
    struct NameFromAddress
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(NameFromAddress); // delete(void* p, unsigned int nbytes) 0x00FB0CB0
        virtual ~NameFromAddress() {} // vtbl+0x00 0x00FB0620 (hkxmlobjectwriter.h:26)
        virtual int nameFromAddress(const void* addr, char* buf, int bufSize) = 0; // vtbl+0x04
        NameFromAddress() {} // 0x00FB0C50

        // 0x00 vfptr
    };

    // size 0x10: "#NNNN" in the order the addresses are first seen
    struct SequentialNameFromAddress : public NameFromAddress
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(SequentialNameFromAddress);
        virtual int nameFromAddress(const void* addr, char* buf, int bufSize); // vtbl+0x04 0x01058600

        hkPointerMap<void const*,int,hkContainerHeapAllocator> m_map; // 0x04
    };

    HK_DECLARE_CLASS_ALLOCATOR(); // delete(void* p, unsigned int nbytes) 0x01059750
    hkXmlObjectWriter(NameFromAddress& n, hkBool writeSerializeIgnored); // 0x01058790
    void beginElement(hkStreamWriter* writer, const char* name, const char* const * attributes, hkBool prefixNewline); // 0x01058800
    void endElement(hkStreamWriter* writer, const char* name, hkBool prefixNewline); // 0x010588C0
    // the destructor (vtbl+0x00 0x00FB1160) is compiler-generated
    virtual hkResult writeObject(hkStreamWriter* writer, const void* objectData, const hkClass& klass, hkRelocationInfo& reloc); // vtbl+0x10 0x010594B0
    virtual hkResult writeRaw(hkStreamWriter* writer, const void* buf, int len); // vtbl+0x14 0x01057DD0
    void adjustIndent(int delta); // 0x01058770
    // 0x010578E0 (hkxmlobjectwriter.h:67)
    int nameFromAddress(const void* addr, char* buf, int bufSize) { return m_nameFromAddress.nameFromAddress(addr, buf, bufSize); }
    // 0x010578F0 (hkxmlobjectwriter.h:69)
    hkBool getWriteSerializeIgnored() { return m_writeSerializeIgnored; }
    static hkResult base64write(hkStreamWriter* sw, const void* buf, int len); // 0x01057910
    virtual hkResult writeObjectWithElement(hkStreamWriter* writer, const void* objectData, const hkClass& klass, const char* objectName, const char* const * attributes); // vtbl+0x18 0x010592D0

public:
    hkInplaceArray<char,16,hkContainerHeapAllocator> m_indent; // 0x08: tabs, zero terminated (the zero is not counted)
    NameFromAddress& m_nameFromAddress; // 0x24
    hkBool m_writeSerializeIgnored; // 0x28
};
