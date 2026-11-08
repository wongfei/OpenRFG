// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/data/hkdataobject.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (stub_impl.md S4, serializer); inline code as the 2014.1 SDK's
// hkDataObject.h/.inl (checked against rfg.exe, SC). The public wrappers (hkDataObject, hkDataArray, hkDataClass and
// the value accessors) forward to the private implementations of a world; the object and array wrappers hold an
// external reference.
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include <stdint.h>
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/pointermap/hkmap.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hkreferencedobject.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/data/hkdataobjectdeclarations.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/data/hkdataobjectimpl.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/typemanager/hktypemanager.h"

class hkMatrix3f;
class hkMatrix4f;
class hkQsTransformf;
class hkQuaternionf;
class hkRotationf;
class hkTransformf;
class hkVector4f;

// The float counts of the math types the value accessors move as vecs (hkVector4f/hkQuaternionf 4, hkMatrix3f/
// hkRotationf/hkQsTransformf 12, hkMatrix4f/hkTransformf 16: their sizes over sizeof(float) in the SDK's macro).
#define HK_DATA_VALUE_ACCESS(HKTYPE, NAME, NREAL) \
    void operator=(const HKTYPE& v) { this->setVec(reinterpret_cast<const float*>(&v), NREAL); } \
    const HKTYPE& as##NAME() const { return *reinterpret_cast<const HKTYPE*>(asVec(NREAL)); }

// size 0x8
class hkDataObject_Value
{
public:
    typedef const _hkDataObject_MemberHandle* MemberHandle;
    typedef hkTypeManager::Type* Type;

    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkDataObject_Value);
    void operator=(const hkDataObject_Value& v) { m_impl->assign(m_handle, v); } // 0x00F90F70
    void operator=(int i) { m_impl->assign(m_handle, i); } // 0x00F6AC40
    void operator=(int64_t i) { m_impl->assign(m_handle, i); } // 0x00F53750
    void operator=(float r) { m_impl->assign(m_handle, r); } // 0x00F53770
    void operator=(bool b) { m_impl->assign(m_handle, int64_t(b)); } // 0x00F94960
    void operator=(hkHalf r) { m_impl->assign(m_handle, r); } // 0x00F9EB40
    void operator=(const char* s) { m_impl->assign(m_handle, s); } // 0x00F537B0
    void operator=(const hkDataArray& a); // 0x00F46250
    void operator=(const hkDataObject& o); // 0x00F53710
    void operator=(const hkDataObjectImpl* o) { m_impl->assign(m_handle, const_cast<hkDataObjectImpl*>(o)); } // 0x00F536F0
    // Original: the SDK's assign(m_handle, HK_NULL) picks the int overload (HK_NULL is 0).
    void setNull() { m_impl->assign(m_handle, 0); } // 0x00F53730

    int asInt() const { return (int)asInt64(); } // 0x00F5FF80
    int64_t asInt64() const { return m_impl->asInt(m_handle); } // 0x00F5C4C0
    float asReal() const { return m_impl->asReal(m_handle); } // 0x00F5C530
    const char* asString() const { return m_impl->asString(m_handle); } // 0x00F5C4F0
    hkDataArray asArray() const; // 0x00F56050
    hkDataObject asObject() const; // 0x00F5C500
    const float* asVec(int n) const { return m_impl->asVec(m_handle, n); } // 0x00F5C4D0
    unsigned int isSet() const { return m_impl && m_impl->isSet(m_handle); } // 0x00F75500
    unsigned int isValid() const { return m_impl != nullptr; } // 0x00F754F0
    hkTypeManager::Type* getType() const; // 0x00F51110
    void getMemberInfo(hkDataClass_MemberInfo& infoOut) const { m_impl->getMemberInfo(m_handle, infoOut); } // 0x00F75520
    void setVec(const float* r, int n) { m_impl->assign(m_handle, r, n); } // 0x00F53790

    HK_DATA_VALUE_ACCESS(hkVector4f, Vector4, 4) // 0x00F90770, as 0x00F77D50
    HK_DATA_VALUE_ACCESS(hkQuaternionf, Quaternion, 4) // as 0x00F77D70
    HK_DATA_VALUE_ACCESS(hkMatrix3f, Matrix3, 12) // as 0x00F77D90
    HK_DATA_VALUE_ACCESS(hkRotationf, Rotation, 12) // as 0x00F77DB0
    HK_DATA_VALUE_ACCESS(hkQsTransformf, QsTransform, 12) // 0x00F95E50, as 0x00F77DD0
    HK_DATA_VALUE_ACCESS(hkMatrix4f, Matrix4, 16) // as 0x00F77DF0
    HK_DATA_VALUE_ACCESS(hkTransformf, Transform, 16) // as 0x00F77E10

    hkDataObject_Value(hkDataObjectImpl* impl, const _hkDataObject_MemberHandle* handle) : m_impl(impl), m_handle(handle) {} // 0x00F461D0

    hkDataObjectImpl* m_impl; // 0x00
    const _hkDataObject_MemberHandle* m_handle; // 0x04
};

// size 0x8
class hkDataArray_Value
{
public:
    typedef hkTypeManager::Type* Type;

    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkDataArray_Value);
    void operator=(const hkDataObject_Value&);
    void operator=(const hkDataArray_Value&);
    void operator=(int i) { m_impl->setInt(m_index, i); } // 0x00F8F940
    void operator=(int64_t i) { m_impl->setInt64(m_index, i); } // 0x00F538D0
    void operator=(float r) { m_impl->setReal(m_index, r); } // 0x00F53890
    void operator=(hkHalf r) { m_impl->setHalf(m_index, r); } // 0x00F98C60
    void operator=(const hkDataArray&);
    void operator=(const hkDataObject& obj); // 0x00F53850
    void operator=(const char* s) { m_impl->setString(m_index, s); } // 0x00F538F0
    void setNull() { m_impl->setObject(m_index, nullptr); } // 0x00F53870

    int asInt() const { return (int)m_impl->asInt(m_index); } // 0x00F5FFB0
    int64_t asInt64() const { return m_impl->asInt64(m_index); } // 0x00F5C5B0
    float asReal() const { return m_impl->asReal(m_index); } // 0x00F5C5A0
    hkDataObject asObject() const; // 0x00F53820
    hkDataArray asArray() const; // 0x00F5C5C0
    const char* asString() const { return m_impl->asString(m_index); } // 0x00F5C590
    // The count is not passed on (the array knows its tuple size).
    const float* asVec(int nreal) const { return m_impl->asVec(m_index); } // 0x00F5C570
    void clear();
    hkTypeManager::Type* getType() const { return m_impl->getType(); } // 0x00F5C560
    void setVec(const float* ra, int nreal) { m_impl->setVec(m_index, ra); } // 0x00F538B0

    HK_DATA_VALUE_ACCESS(hkVector4f, Vector4, 4) // 0x00F94230, as 0x00F77E30
    HK_DATA_VALUE_ACCESS(hkQuaternionf, Quaternion, 4) // as 0x00F77E40
    HK_DATA_VALUE_ACCESS(hkMatrix3f, Matrix3, 12) // 0x00F92A00, as 0x00F77E50
    HK_DATA_VALUE_ACCESS(hkRotationf, Rotation, 12) // as 0x00F77E60
    HK_DATA_VALUE_ACCESS(hkQsTransformf, QsTransform, 12) // 0x00F9A650, as 0x00F77E70
    HK_DATA_VALUE_ACCESS(hkMatrix4f, Matrix4, 16) // as 0x00F77E80
    HK_DATA_VALUE_ACCESS(hkTransformf, Transform, 16) // as 0x00F77E90

public:
    hkDataArray_Value(hkDataArrayImpl* impl, int idx) : m_impl(impl), m_index(idx) {} // 0x00F53630

    hkDataArrayImpl* m_impl; // 0x00
    int m_index; // 0x04
};

#undef HK_DATA_VALUE_ACCESS

// size 0x4
class hkDataObject
{
public:
    typedef hkTypeManager::Type* Type;
    typedef const _hkDataObject_MemberHandle* MemberHandle;
    typedef int Iterator;
    typedef hkDataObject_Value Value;
    typedef hkDataObject_Handle Handle;

    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkDataObject); // new(unsigned int nbytes) 0x00F60AD0, delete(void* p, unsigned int nbytes) 0x00F56020

    // 0x00F44CF0
    hkDataObject(const hkDataObject& o) : m_impl(o.m_impl)
    {
        if (m_impl)
        {
            m_impl->addExternalReference();
        }
    }

    // 0x00F3DB60
    hkDataObject(hkDataObjectImpl* i) : m_impl(i)
    {
        if (m_impl)
        {
            m_impl->addExternalReference();
        }
    }

    hkDataObject() : m_impl(nullptr) {} // 0x00F53670

    // 0x00F53680
    void operator=(const hkDataObject& o)
    {
        if (o.m_impl)
        {
            o.m_impl->addExternalReference();
        }
        if (m_impl)
        {
            m_impl->removeExternalReference();
        }
        m_impl = o.m_impl;
    }

    // 0x00F5D440
    bool operator==(const hkDataObject& o)
    {
        Handle h = getHandle();
        Handle oh = o.getHandle();
        return h.p0 == oh.p0 && h.p1 == oh.p1;
    }

    bool operator!=(const hkDataObject& o) { return !(*this == o); } // 0x00F5D4C0

    // 0x00F3DB80
    ~hkDataObject()
    {
        if (m_impl)
        {
            m_impl->removeExternalReference();
        }
    }

    const hkDataObject_Value operator[](const char* name) const { return m_impl->accessByName(name); } // 0x00F5FF60
    hkDataObject_Value operator[](const char* name) { return m_impl->accessByName(name); } // 0x00F536D0

    // 0x00F44D10
    hkDataObject_Handle getHandle() const
    {
        hkDataObject_Handle h = {nullptr, nullptr};
        return m_impl ? m_impl->getHandle() : h;
    }

    const hkDataObjectImpl* getImplementation() const { return m_impl; } // 0x00F53620
    hkDataObjectImpl* getImplementation() { return m_impl; } // 0x00F461F0
    const hkDataClass getClass() const; // 0x00F44D50
    unsigned int isNull() const { return m_impl == nullptr; } // 0x00F42A40
    int getMemberIterator() const { return m_impl->getMemberIterator(); } // 0x00F5C460
    unsigned int isValid(int it) const { return m_impl->isValid(it); } // 0x00F5C470
    int getNextMember(int it) const { return m_impl->getNextMember(it); } // 0x00F5C480
    const hkDataObject_Value getMemberValue(int it) const { return m_impl->getMemberValue(it); } // 0x00F5C4A0
    const char* getMemberName(int it) const { return m_impl->getMemberName(it); } // 0x00F5C490
    unsigned int hasMember(const char* name) const { return m_impl->hasMember(name); } // 0x00F92AD0
    void getAllMemberHandles(hkArrayBase<_hkDataObject_MemberHandle const*>& handles) const { m_impl->getAllMemberHandles(handles); } // 0x00F6AC30
    void destroy() { m_impl->destroy(); } // 0x00F66450
    void destroyMember(const char* name); // 0x00F51210

public:
    hkDataObjectImpl* m_impl; // 0x00
};

// size 0x4
class hkDataArray
{
public:
    typedef hkTypeManager::Type* Type;
    typedef hkDataArray_Value Value;

    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkDataArray);
    hkDataArray(const hkDataArray&);

    // 0x00F46270
    explicit hkDataArray(hkDataArrayImpl* i) : m_impl(i)
    {
        if (m_impl)
        {
            m_impl->addExternalReference();
        }
    }

    // Not in rfg.exe. Original (2014.1 SDK): it adds an external reference to both arrays and removes none.
    void operator=(const hkDataArray& arr)
    {
        if (m_impl != arr.m_impl)
        {
            arr.m_impl->addExternalReference();
            m_impl->addExternalReference();
            m_impl = arr.m_impl;
        }
    }

    // 0x00F46290
    ~hkDataArray()
    {
        if (m_impl)
        {
            m_impl->removeExternalReference();
        }
    }

    hkResult asStridedBasicArray(hkStridedBasicArray& arrOut) const { return m_impl->asStridedBasicArray(arrOut); } // 0x00F6AC90
    void clear() { m_impl->clear(); }
    int getSize() const { return m_impl->getSize(); } // 0x00F53800
    void reserve(int n) { m_impl->reserve(n); }
    void setSize(int n) { m_impl->setSize(n); } // 0x00F537F0
    int getUnderlyingIntegerSize() const { return m_impl->getUnderlyingIntegerSize(); } // 0x00F5FFC0
    const hkDataArray_Value operator[](int i) const { return hkDataArray_Value(m_impl, i); } // 0x00F5C540
    hkDataArray_Value operator[](int i) { return hkDataArray_Value(m_impl, i); } // 0x00F537D0
    hkTypeManager::Type* getType() const { return m_impl->getType(); } // 0x00F53810
    hkDataClass getClass() const; // 0x00F5FF90
    hkDataArray swizzleObjectMember(const char* name) const { return hkDataArray(m_impl->swizzleObjectMember(name)); } // 0x00F5FFD0
    void setAll(const hkBool* t, int n) { m_impl->setAll(t, n); }
    void setAll(const char* t, int n) { m_impl->setAll(t, n); }
    void setAll(const signed char* t, int n) { m_impl->setAll(t, n); }
    void setAll(const unsigned char* t, int n) { m_impl->setAll(t, n); } // 0x00F6AC60
    void setAll(const short* t, int n) { m_impl->setAll(t, n); }
    void setAll(const unsigned short* t, int n) { m_impl->setAll(t, n); }
    void setAll(const int* t, int n) { m_impl->setAll(t, n); } // 0x00F6AC70
    void setAll(const unsigned int* t, int n) { m_impl->setAll(t, n); }
    void setAll(const int64_t* t, int n) { m_impl->setAll(t, n); }
    void setAll(const uint64_t* t, int n) { m_impl->setAll(t, n); }
    void setAll(const float* t, int n) { m_impl->setAll(t, n); } // 0x00F6AC80
    void setAll(const hkHalf* t, int n) { m_impl->setAll(t, n); }
    hkDataWorld* getWorld() const { return m_impl ? m_impl->getWorld() : nullptr; }
    const hkDataArrayImpl* getImplementation() const { return m_impl; }
    hkDataArrayImpl* getImplementation() { return m_impl; } // 0x00F46200

public:
    hkDataArrayImpl* m_impl; // 0x00
};

// size 0x4
class hkDataClass
{
public:
    typedef hkDataClass_MemberInfo MemberInfo;

    // size 0x18
    struct Cinfo
    {
        // size 0xC
        struct Member
        {
            HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(Member);

            // 0x00F51100
            void set(const char* n, hkTypeManager::Type* t)
            {
                name = n;
                type = t;
            }

            const char* name; // 0x00
            hkTypeManager::Type* type; // 0x04
            const void* valuePtr; // 0x08
        };

        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(Cinfo); // delete(void* p, unsigned int nbytes) 0x00F74BB0
        Cinfo() {} // 0x00F42810
        ~Cinfo() {} // 0x00F42830

        const char* name; // 0x00
        int version; // 0x04
        const char* parent; // 0x08
        hkArray<Member,hkContainerHeapAllocator> members; // 0x0C
    };

    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkDataClass); // delete(void* p, unsigned int nbytes) 0x00F5D410
    hkDataClass(hkDataClassImpl* impl) : m_impl(impl) {} // 0x00F3DB00
    ~hkDataClass() {} // 0x00F3DB10
    unsigned int isNull() const { return m_impl == nullptr; } // 0x00F3DB20
    const char* getName() const { return m_impl->getName(); } // 0x00F44CE0
    int getVersion() const { return m_impl->getVersion(); } // 0x00F3DB30
    hkDataClass getParent() const { return m_impl->getParent(); } // 0x00F46220
    const hkDataWorld* getWorld() const { return m_impl->getWorld(); } // 0x00F5C450
    hkBool isSuperClass(const hkDataClass& k) const { return m_impl->isSuperClass(k.getImplementation()); }
    int getNumDeclaredMembers() const { return m_impl->getNumDeclaredMembers(); } // 0x00F46240
    void getDeclaredMemberInfo(int i, hkDataClass_MemberInfo& info) const { m_impl->getDeclaredMemberInfo(i, info); }
    int getDeclaredMemberIndexByName(const char* name) const { return m_impl->getDeclaredMemberIndexByName(name); }
    void getAllDeclaredMemberInfo(hkArrayBase<hkDataClass_MemberInfo>& minfos) const; // 0x00F51220
    int getNumMembers() const { return m_impl->getNumMembers(); } // 0x00F3DB40
    void getMemberInfo(int i, hkDataClass_MemberInfo& info) const { m_impl->getMemberInfo(i, info); } // 0x00F53650
    int getMemberIndexByName(const char* name) const { return m_impl->getMemberIndexByName(name); } // 0x00F53660
    void getAllMemberInfo(hkArrayBase<hkDataClass_MemberInfo>& members) const { m_impl->getAllMemberInfo(members); } // 0x00F3DB50
    hkDataClassImpl* getImplementation() { return m_impl; } // 0x00F3DAF0
    const hkDataClassImpl* getImplementation() const { return m_impl; } // 0x00F46210
    void getCinfo(Cinfo& cinfo) const; // 0x00F51270

public:
    hkDataClassImpl* m_impl; // 0x00
};

// size 0x8
class hkDataWorld : public hkReferencedObject
{
public:
    enum DataWorldType
    {
        TYPE_NATIVE = 0,
        TYPE_DICTIONARY = 1,
    };

    HK_DECLARE_CLASS_ALLOCATOR(); // delete(void* p, unsigned int nbytes) 0x00F491E0
    hkDataWorld() {} // 0x00F46D00
    virtual ~hkDataWorld() {} // vtbl+0x00 0x00F46D10
    virtual hkDataClassImpl* newClass(const hkDataClass::Cinfo& cinfo) = 0; // vtbl+0x10
    virtual hkDataObjectImpl* newObject(const hkDataClass& klass, bool createdDuringPatching = false) const = 0; // vtbl+0x14
    virtual hkDataArrayImpl* newArray(hkDataObject& obj, const _hkDataObject_MemberHandle* handle, const hkDataClass_MemberInfo& minfo) const = 0; // vtbl+0x18
    virtual hkEnum<enum DataWorldType,int> getType() const = 0; // vtbl+0x1C
    virtual hkDataObject getContents() const = 0; // vtbl+0x20
    virtual void findAllClasses(hkArray<hkDataClassImpl*,hkContainerTempAllocator>& classesOut) const = 0; // vtbl+0x24
    virtual hkDataClassImpl* findClass(const char* name) const = 0; // vtbl+0x28
    virtual hkDataObject findObject(const hkDataObject_Handle& handle) const = 0; // vtbl+0x2C
    virtual hkTypeManager& getTypeManager() = 0; // vtbl+0x30
    virtual void renameClass(hkDataClass& klass, const char* newName); // vtbl+0x34 0x00F51150
    virtual void removeClass(hkDataClass& klass); // vtbl+0x38 0x00F51160
    virtual void setClassVersion(hkDataClass& klass, int newVersion); // vtbl+0x3C 0x00F51170
    virtual void setClassParent(hkDataClass& klass, hkDataClass& parent); // vtbl+0x40 0x00F51180
    virtual void addClassMember(hkDataClass& klass, const char* name, hkTypeManager::Type* type, const void* valuePtr); // vtbl+0x44 0x00F51200
    virtual void setClassMemberDefault(hkDataClass& klass, const char* name, const void* valuePtr); // vtbl+0x48 0x00F511E0
    virtual void renameClassMember(hkDataClass& klass, const char* oldName, const char* newName); // vtbl+0x4C 0x00F51190
    virtual void removeClassMember(hkDataClass& klass, const char* name); // vtbl+0x50 0x00F511A0
    virtual void findObjectsByBaseClass(const char* baseClassName, hkArray<hkDataObjectImpl*,hkContainerTempAllocator>& objectsOut) const; // vtbl+0x54 0x00F511B0
    virtual void findObjectsByExactClass(const char* className, hkArray<hkDataObjectImpl*,hkContainerTempAllocator>& objectsOut) const; // vtbl+0x58 0x00F511C0
    virtual void castObject(hkDataObject& obj, const hkDataClass& objClass); // vtbl+0x5C 0x00F511D0
    virtual hkDataClassImpl* copyClassFromWorld(const char* name, const hkDataWorld& worldFrom); // vtbl+0x60 0x00F511F0
    void addClassMemberTypeExpression(hkDataClass& klass, const char* name, const char* expr, const char* className, const void* valuePtr); // 0x00F51360
};

// ---- hkDataObject.inl: the members that need the complete wrappers

inline void hkDataObject_Value::operator=(const hkDataArray& a) { m_impl->assign(m_handle, const_cast<hkDataArray&>(a).getImplementation()); } // 0x00F46250
inline void hkDataObject_Value::operator=(const hkDataObject& o) { m_impl->assign(m_handle, const_cast<hkDataObject&>(o).getImplementation()); } // 0x00F53710
inline hkDataArray hkDataObject_Value::asArray() const { return hkDataArray(m_impl->asArray(m_handle)); } // 0x00F56050
inline hkDataObject hkDataObject_Value::asObject() const { return hkDataObject(m_impl->asObject(m_handle)); } // 0x00F5C500
inline void hkDataArray_Value::operator=(const hkDataObject& obj) { m_impl->setObject(m_index, const_cast<hkDataObjectImpl*>(obj.getImplementation())); } // 0x00F53850
inline hkDataObject hkDataArray_Value::asObject() const { return hkDataObject(const_cast<hkDataObjectImpl*>(m_impl->asObject(m_index))); } // 0x00F53820
inline hkDataArray hkDataArray_Value::asArray() const { return hkDataArray(const_cast<hkDataArrayImpl*>(m_impl->asArray(m_index))); } // 0x00F5C5C0
inline const hkDataClass hkDataObject::getClass() const { return hkDataClass(const_cast<hkDataClassImpl*>(m_impl->getClass())); } // 0x00F44D50
inline hkDataClass hkDataArray::getClass() const { return hkDataClass(const_cast<hkDataClassImpl*>(m_impl->getClass())); } // 0x00F5FF90

// size 0x1
// The handle map key: p0 without its low 4 bits times 2654435761 (0x9E3779B1); p0 -1 marks a free slot.
template<>
struct hkMapOperations<hkDataObject_Handle>
{
    static unsigned int hash(const hkDataObject_Handle& key, unsigned int mod) { return (unsigned int)((hkUlong(key.p0) >> 4) * 2654435761u) & mod; }
    static void invalidate(hkDataObject_Handle& key) { key.p0 = (void*)hkUlong(-1); }
    static hkBool32 isValid(const hkDataObject_Handle& key) { return key.p0 != (void*)hkUlong(-1); }
    static hkBool32 equal(const hkDataObject_Handle& key0, const hkDataObject_Handle& key1) { return key0.p0 == key1.p0 && key0.p1 == key1.p1; }
};
