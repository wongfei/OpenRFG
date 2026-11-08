// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/data/hkdataobjectimpl.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (stub_impl.md S4, serializer); inline code as the 2014.1 SDK
// (checked against rfg.exe, SC). Overloaded virtuals are declared in the SDK's order: MSVC lays an overload group out
// in reverse declaration order, which gives the slots of the comments.
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include <stdint.h>
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/typemanager/hktypemanager.h"

struct _hkDataObject_MemberHandle;
class hkDataArrayImpl;
class hkDataClassImpl;
struct hkDataClass_MemberInfo;
struct hkDataObject_Handle;
class hkDataObject_Value;
class hkDataWorld;
struct hkStridedBasicArray;

// size 0xC
// Private reference counting of the data world's objects, arrays and classes: m_count counts all references (the
// object deletes itself at 0), the low 16 bits of m_memSizeAndExternalCount the hkDataObject/hkDataArray wrappers
// among them (the high 16 bits are the hkReferencedObject-style memory size, 0xFFFF).
class hkDataRefCounted
{
public:
    // HK_DECLARE_CLASS_ALLOCATOR_UNCHECKED in the SDK: the size comes from the virtual destructor (the high 16 bits of
    // m_memSizeAndExternalCount stay 0xFFFF).
    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int nbytes) 0x00F49130, delete(void* p, unsigned int nbytes) 0x00F49150
    virtual ~hkDataRefCounted() {} // vtbl+0x00 0x00F46190
    hkDataRefCounted() : m_memSizeAndExternalCount(0xFFFF0000u), m_count(0) {} // 0x00F461A0
    void addReference() const { ++m_count; } // 0x00F3DAA0

    // 0x00F3DAB0
    void removeReference() const
    {
        if (--m_count == 0)
        {
            delete this;
        }
    }

    int getReferenceCount() const { return m_count; }

    // 0x00F3DAC0
    void addExternalReference() const
    {
        m_memSizeAndExternalCount = (m_memSizeAndExternalCount & 0xFFFF0000u) | ((m_memSizeAndExternalCount + 1) & 0xFFFFu);
        addReference();
    }

    // 0x00F3DAD0
    void removeExternalReference() const
    {
        m_memSizeAndExternalCount = (m_memSizeAndExternalCount & 0xFFFF0000u) | ((m_memSizeAndExternalCount - 1) & 0xFFFFu);
        removeReference();
    }

    int getExternalReferenceCount() const { return (int)(m_memSizeAndExternalCount & 0xFFFFu); } // 0x00F461C0

    // 0x00 vfptr
    mutable unsigned int m_memSizeAndExternalCount; // 0x04
    mutable int m_count; // 0x08
};

// size 0xC
class hkDataObjectImpl : public hkDataRefCounted
{
public:
    typedef int Iterator;
    typedef const _hkDataObject_MemberHandle* MemberHandle;
    typedef hkDataObject_Handle Handle;
    typedef hkDataObject_Value Value;

    hkDataObjectImpl() {} // 0x00F464F0
    virtual ~hkDataObjectImpl() {} // vtbl+0x00 0x00F46510
    virtual hkDataObject_Handle getHandle() const = 0; // vtbl+0x04
    virtual const hkDataClassImpl* getClass() const = 0; // vtbl+0x08
    virtual hkDataObject_Value accessByName(const char* name) = 0; // vtbl+0x0C
    virtual unsigned int hasMember(const char* name) const = 0; // vtbl+0x10
    virtual int getMemberIterator() const = 0; // vtbl+0x14
    virtual unsigned int isValid(int it) const = 0; // vtbl+0x18
    virtual int getNextMember(int it) const = 0; // vtbl+0x1C
    virtual const char* getMemberName(int it) const = 0; // vtbl+0x20
    virtual const hkDataObject_Value getMemberValue(int it) const = 0; // vtbl+0x24
    virtual hkDataArrayImpl* asArray(MemberHandle handle) = 0; // vtbl+0x28
    virtual const char* asString(MemberHandle handle) = 0; // vtbl+0x2C
    virtual int64_t asInt(MemberHandle handle) = 0; // vtbl+0x30
    virtual hkDataObjectImpl* asObject(MemberHandle handle) = 0; // vtbl+0x34
    virtual const float* asVec(MemberHandle handle, int nreal) = 0; // vtbl+0x38
    virtual float asReal(MemberHandle handle) = 0; // vtbl+0x3C
    virtual void assign(MemberHandle handle, const hkDataObject_Value& valueIn) = 0; // vtbl+0x60
    virtual void assign(MemberHandle handle, hkDataObjectImpl* value) = 0; // vtbl+0x5C
    virtual void assign(MemberHandle handle, hkDataArrayImpl* value) = 0; // vtbl+0x58
    virtual void assign(MemberHandle handle, const char* value) = 0; // vtbl+0x54
    virtual void assign(MemberHandle handle, float r) = 0; // vtbl+0x50
    virtual void assign(MemberHandle handle, hkHalf r) = 0; // vtbl+0x4C
    virtual void assign(MemberHandle handle, const float* r, int nreal) = 0; // vtbl+0x48
    virtual void assign(MemberHandle handle, int valueIn) = 0; // vtbl+0x44
    virtual void assign(MemberHandle handle, int64_t valueIn) = 0; // vtbl+0x40
    virtual void getMemberInfo(MemberHandle handle, hkDataClass_MemberInfo& infoOut) = 0; // vtbl+0x64
    virtual unsigned int isSet(MemberHandle handle) = 0; // vtbl+0x68
    virtual unsigned int createdDuringPatching() const = 0; // vtbl+0x6C
    virtual void destroy() = 0; // vtbl+0x70
    virtual void destroyMember(const char* name) = 0; // vtbl+0x74
    virtual void getAllMemberHandles(hkArrayBase<MemberHandle>& handles) const = 0; // vtbl+0x78
    static void assignValueImpl(hkDataObjectImpl* dstObj, MemberHandle handle, const hkDataObject_Value& valueIn); // 0x00F6EC20
};

// size 0xC
// The defaults of the item accessors are the SDK's asserting stubs (asserts compiled out): they answer 0.
class hkDataArrayImpl : public hkDataRefCounted
{
public:
    typedef hkTypeManager::Type* Type;

    hkDataArrayImpl() {} // 0x00F46520
    virtual ~hkDataArrayImpl() {} // vtbl+0x00 0x00F46720
    virtual hkTypeManager::Type* getType() const = 0; // vtbl+0x04
    virtual void clear() = 0; // vtbl+0x08
    virtual void reserve(int n) = 0; // vtbl+0x0C
    virtual void setSize(int size) = 0; // vtbl+0x10
    virtual int getSize() const = 0; // vtbl+0x14
    virtual const hkDataClassImpl* getClass() const = 0; // vtbl+0x18
    virtual hkDataWorld* getWorld() const = 0; // vtbl+0x1C
    virtual hkResult asStridedBasicArray(hkStridedBasicArray& arrOut) = 0; // vtbl+0x20
    virtual int getUnderlyingIntegerSize() const { return 0; } // vtbl+0x24 0x00F46540
    virtual hkDataArrayImpl* swizzleObjectMember(const char* name) const { return nullptr; } // vtbl+0x28 0x00F46550
    virtual const float* asVec(int index) const { return nullptr; } // vtbl+0x2C 0x00F46560
    virtual void setVec(int index, const float* val) {} // vtbl+0x30 0x00F46570
    virtual const char* asString(int index) const { return nullptr; } // vtbl+0x34 0x00F46580
    virtual void setString(int index, const char* val) {} // vtbl+0x38 0x00F46590
    virtual float asReal(int index) const { return 0.0f; } // vtbl+0x3C 0x00F465A0
    virtual void setReal(int index, float val) {} // vtbl+0x40 0x00F465B0
    virtual int asInt(int index) const { return 0; } // vtbl+0x44 0x00F465C0
    virtual void setInt(int index, int val) {} // vtbl+0x48 0x00F465D0
    virtual int64_t asInt64(int index) const { return 0; } // vtbl+0x4C 0x00F465E0
    virtual void setInt64(int index, int64_t val) {} // vtbl+0x50 0x00F465F0
    virtual hkDataObjectImpl* asObject(int index) const { return nullptr; } // vtbl+0x54 0x00F46600
    virtual void setObject(int index, hkDataObjectImpl* val) {} // vtbl+0x58 0x00F46610
    virtual hkDataArrayImpl* asArray(int index) const { return nullptr; } // vtbl+0x5C 0x00F46620
    virtual void setArray(int index, hkDataArrayImpl* val) {} // vtbl+0x60 0x00F46630
    // Original: the default hkHalf is uninitialised.
    virtual hkHalf asHalf(int index) const { return hkHalf(); } // vtbl+0x64 0x00F46640
    virtual void setHalf(int index, hkHalf val) {} // vtbl+0x68 0x00F46650
    virtual void setAll(const hkBool*, int n) {} // vtbl+0x98 0x00F46660
    virtual void setAll(const char*, int n) {} // vtbl+0x94 0x00F46670
    virtual void setAll(const signed char*, int n) {} // vtbl+0x90 0x00F46680
    virtual void setAll(const unsigned char*, int n) {} // vtbl+0x8C 0x00F46690
    virtual void setAll(const short*, int n) {} // vtbl+0x88 0x00F466A0
    virtual void setAll(const unsigned short*, int n) {} // vtbl+0x84 0x00F466B0
    virtual void setAll(const int*, int n) {} // vtbl+0x80 0x00F466C0
    virtual void setAll(const unsigned int*, int n) {} // vtbl+0x7C 0x00F466D0
    virtual void setAll(const int64_t*, int n) {} // vtbl+0x78 0x00F466E0
    virtual void setAll(const uint64_t*, int n) {} // vtbl+0x74 0x00F466F0
    virtual void setAll(const float*, int n) {} // vtbl+0x70 0x00F46700
    virtual void setAll(const hkHalf*, int n) {} // vtbl+0x6C 0x00F46710
    void set(int index, const hkDataObject_Value& val); // 0x00F6ED70
};

// size 0xC
class hkDataClassImpl : public hkDataRefCounted
{
public:
    typedef hkDataClass_MemberInfo MemberInfo;

    hkDataClassImpl() {} // 0x00F464C0
    virtual ~hkDataClassImpl() {} // vtbl+0x00 0x00F464E0
    virtual const hkDataWorld* getWorld() const = 0; // vtbl+0x04
    virtual const char* getName() const = 0; // vtbl+0x08
    virtual int getVersion() const = 0; // vtbl+0x0C
    virtual hkDataClassImpl* getParent() const = 0; // vtbl+0x10
    virtual hkBool isSuperClass(const hkDataClassImpl* k) const = 0; // vtbl+0x14
    virtual int getNumDeclaredMembers() const = 0; // vtbl+0x18
    virtual void getDeclaredMemberInfo(int i, hkDataClass_MemberInfo& info) const = 0; // vtbl+0x1C
    virtual int getDeclaredMemberIndexByName(const char* name) const = 0; // vtbl+0x20
    virtual int getNumMembers() const = 0; // vtbl+0x24
    virtual void getMemberInfo(int i, hkDataClass_MemberInfo& info) const = 0; // vtbl+0x28
    virtual int getMemberIndexByName(const char* name) const = 0; // vtbl+0x2C
    virtual void getAllMemberInfo(hkArrayBase<hkDataClass_MemberInfo>& infos) const = 0; // vtbl+0x30
};
