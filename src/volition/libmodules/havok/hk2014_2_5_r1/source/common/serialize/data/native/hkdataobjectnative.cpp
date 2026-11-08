// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/data/native/hkdataobjectnative.cpp
//
// Havok 2014.2.5 (stub_impl.md S4, serializer SC): the native data world, a read and in-place-write view of live
// objects through their hkClass reflection (the source world when objects are saved, and the destination the
// dictionary world is copied into). A member handle is the hkClassMember itself. From the decompile and asm.
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/data/native/hkdataobjectnative.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/string/hkstring.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/hkclass.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/hktypeinfo.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/util/hkvariantdatautil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/error/hkerror.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/data/util/hkdataobjectutil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/hkbuiltintyperegistry.h"

// The attribute that overrides a member's class with the named one (typeName).
static const char* s_hkDataObjectTupeAttributeID = "hk.DataObjectType";

// 0x00F57CC0 (:39)
hkDataClassNative::hkDataClassNative(const hkClass* k, hkDataWorldNative* w)
    : m_class(k), m_world(w)
{
}

// 0x00F57DF0
hkDataClassNative::~hkDataClassNative() {}
// 0x00F57CF0 (:43)
const hkDataWorld* hkDataClassNative::getWorld() const { return m_world; }
// 0x00F57D00 (:48)
const char* hkDataClassNative::getName() const { return m_class->getName(); }
// 0x00F57D10 (:52)
int hkDataClassNative::getVersion() const { return m_class->getDescribedVersion(); }

// 0x00F57D20 (:56)
hkDataClassImpl* hkDataClassNative::getParent() const
{
    const hkClass* parent = m_class->getParent();
    return parent ? m_world->findClass(parent->getName()) : nullptr;
}

// 0x00F57D50 (:61)
// By name along k's parent chain.
hkBool hkDataClassNative::isSuperClass(const hkDataClassImpl* k) const
{
    while (k)
    {
        if (hkString::strCmp(k->getName(), getName()) == 0)
        {
            return true;
        }
        k = k->getParent();
    }
    return false;
}

// 0x00F57DB0 (:76)
int hkDataClassNative::getNumDeclaredMembers() const { return m_class->getNumDeclaredMembers(); }
// 0x00F57DC0 (:80)
int hkDataClassNative::getDeclaredMemberIndexByName(const char* name) const { return m_class->getDeclaredMemberIndexByName(name); }
// 0x00F57DD0 (:86)
int hkDataClassNative::getNumMembers() const { return m_class->getNumMembers(); }
// 0x00F57DE0 (:90)
int hkDataClassNative::getMemberIndexByName(const char* name) const { return m_class->getMemberIndexByName(name); }

// 0x00F58B70 (:94)
// A member the world does not serialize is void. A struct or pointer member's class can be renamed by the
// hk.DataObjectType attribute, and hkpMaxSizeMotion is presented as hkpMotion. The default value is not filled in.
void hkDataClassNative::_fillMemberInfo(const hkDataClassImpl* clsIn, hkDataWorldNative* world, const hkClassMember& mem, hkDataClass_MemberInfo& info)
{
    hkTypeManager& typeManager = world->getTypeManager();
    info.m_name = mem.getName();
    info.m_owner = clsIn;
    if (mem.getFlags().allAreSet(hkClassMember::SERIALIZE_IGNORED) && !world->m_accessSerializeIgnored)
    {
        info.m_type = typeManager.getVoid();
        return;
    }
    const hkClass* klass = mem.getClass();
    if (klass)
    {
        const hkVariant* attr = mem.getAttribute(s_hkDataObjectTupeAttributeID);
        if (attr)
        {
            hkClassMemberAccessor typeName(*attr, "typeName");
            klass = world->m_reg->getClassByName(typeName.asCstring(0));
        }
        else if (hkString::strCmp(klass->getName(), "hkpMaxSizeMotion") == 0)
        {
            klass = world->m_reg->getClassByName("hkpMotion");
        }
    }
    info.m_type = world->getTypeFromMemberType(mem.getType(), mem.getSubType(), klass, mem.getCstyleArraySize());
}

// 0x00F58C70 (:130)
void hkDataClassNative::fillMemberInfo(const hkClassMember& mem, hkDataClass_MemberInfo& info) const
{
    _fillMemberInfo(this, m_world, mem, info);
}

// 0x00F58C90 (:136)
void hkDataClassNative::getDeclaredMemberInfo(int i, hkDataClass_MemberInfo& info) const
{
    _fillMemberInfo(this, m_world, m_class->getDeclaredMember(i), info);
    info.m_valuePtr = m_class->getDeclaredDefault(i);
}

// 0x00F58CD0 (:141)
void hkDataClassNative::getMemberInfo(int i, hkDataClass_MemberInfo& info) const
{
    _fillMemberInfo(this, m_world, m_class->getMember(i), info);
    info.m_valuePtr = m_class->getDefault(i);
}

// 0x00F58D10 (:146)
// Fills infos from its end backwards, one class of the parent chain at a time (infos holds getNumMembers() entries).
void hkDataClassNative::getAllMemberInfo(hkArrayBase<hkDataClass_MemberInfo>& infos) const
{
    hkDataClass_MemberInfo* cur = infos.begin() + infos.getSize();
    for (const hkClass* c = m_class; c; c = c->getParent())
    {
        const int n = c->getNumDeclaredMembers();
        cur -= n;
        for (int i = 0; i < n; i++)
        {
            _fillMemberInfo(this, m_world, c->getDeclaredMember(i), cur[i]);
            cur[i].m_valuePtr = c->getDeclaredDefault(i);
        }
    }
}

// 0x00F580F0 (:169)
// Original: getMember(i) where getDeclaredMember(i) is meant, so a class with a parent gets the first members of the
// whole hierarchy in each block (rfg.exe's hkClass::getMember).
void hkDataClassNative::_getAllMemberHandles(hkArrayBase<_hkDataObject_MemberHandle const*>& handles) const
{
    _hkDataObject_MemberHandle const** cur = handles.begin() + handles.getSize();
    for (const hkClass* c = m_class; c; c = c->getParent())
    {
        const int n = c->getNumDeclaredMembers();
        cur -= n;
        for (int i = 0; i < n; i++)
        {
            cur[i] = reinterpret_cast<const _hkDataObject_MemberHandle*>(&c->getMember(i));
        }
    }
}

// size 0x34
// An array view: a c-style array (m_arrayObject null) or a native hkArray/hkSimpleArray/hkHomogeneousArray
// (m_arrayObject points at it, so the size and storage are its own).
class hkDataArrayNative : public hkDataArrayImpl
{
public:
    hkDataArrayNative(hkDataWorldNative* world, void* address, int arraySize, int elementStride, hkClassMember::Type type, hkClassMember::Type subType, const hkClass* klass, int tupleCount, void* arrayObject);
    virtual hkTypeManager::Type* getType() const;
    virtual hkDataWorld* getWorld() const;
    virtual void clear();
    virtual void reserve(int n);
    virtual void setSize(int n);
    virtual int getSize() const;
    virtual int getUnderlyingIntegerSize() const;
    virtual const hkDataClassImpl* getClass() const;
    virtual const float* asVec(int index) const;
    virtual const char* asString(int index) const;
    virtual float asReal(int index) const;
    virtual int asInt(int index) const;
    virtual int64_t asInt64(int index) const;
    virtual hkDataObjectImpl* asObject(int index) const;
    virtual hkDataArrayImpl* asArray(int index) const;
    virtual hkDataArrayImpl* swizzleObjectMember(const char* name) const;
    virtual hkResult asStridedBasicArray(hkStridedBasicArray& arrOut);
    void _setAll(hkClassMember::Type type, const void* v, int n);
    // 0x00F58390 (:397)
    virtual void setAll(const hkBool* v, int n) { if (m_nativeType == hkClassMember::TYPE_BOOL) _setAll(hkClassMember::TYPE_BOOL, v, n); }
    // 0x00F583D0 (:398)
    virtual void setAll(const char* v, int n) { if (m_nativeType == hkClassMember::TYPE_CHAR) _setAll(hkClassMember::TYPE_CHAR, v, n); }
    // 0x00F58410 (:399)
    virtual void setAll(const signed char* v, int n) { if (m_nativeType == hkClassMember::TYPE_INT8) _setAll(hkClassMember::TYPE_INT8, v, n); }
    // 0x00F58450 (:400)
    virtual void setAll(const unsigned char* v, int n) { _setAll(hkClassMember::TYPE_UINT8, v, n); }
    // 0x00F584B0 (:401)
    virtual void setAll(const short* v, int n) { if (m_nativeType == hkClassMember::TYPE_INT16) _setAll(hkClassMember::TYPE_INT16, v, n); }
    // 0x00F584F0 (:402)
    virtual void setAll(const unsigned short* v, int n) { if (m_nativeType == hkClassMember::TYPE_UINT16) _setAll(hkClassMember::TYPE_UINT16, v, n); }
    // 0x00F58530 (:403)
    virtual void setAll(const int* v, int n) { if (m_nativeType == hkClassMember::TYPE_INT32) _setAll(hkClassMember::TYPE_INT32, v, n); }
    // 0x00F58570 (:404)
    virtual void setAll(const unsigned int* v, int n) { if (m_nativeType == hkClassMember::TYPE_UINT32) _setAll(hkClassMember::TYPE_UINT32, v, n); }
    // 0x00F585B0 (:405)
    virtual void setAll(const int64_t* v, int n) { if (m_nativeType == hkClassMember::TYPE_INT64) _setAll(hkClassMember::TYPE_INT64, v, n); }
    // 0x00F585F0 (:406)
    virtual void setAll(const uint64_t* v, int n) { if (m_nativeType == hkClassMember::TYPE_UINT64) _setAll(hkClassMember::TYPE_UINT64, v, n); }
    // 0x00F58630 (:407)
    virtual void setAll(const float* v, int n) { if (m_nativeType == hkClassMember::TYPE_REAL) _setAll(hkClassMember::TYPE_REAL, v, n); }
    // 0x00F58670 (:408)
    virtual void setAll(const hkHalf* v, int n) { if (m_nativeType == hkClassMember::TYPE_HALF) _setAll(hkClassMember::TYPE_HALF, v, n); }
    virtual void setVec(int index, const float* val);
    virtual void setString(int index, const char* val);
    virtual void setReal(int index, float val);
    virtual void setHalf(int index, hkHalf val);
    virtual void setInt(int index, int val);
    virtual void setInt64(int index, int64_t val);
    virtual void setArray(int index, hkDataArrayImpl* val);
    virtual void setObject(int index, hkDataObjectImpl* val);
    void* _getElement(int index) const;

    hkDataWorldNative* m_world; // 0x0C
    void* m_address; // 0x10
    int m_arraySize; // 0x14
    int m_elementStride; // 0x18
    hkClassMember::Type m_nativeType; // 0x1C
    hkClassMember::Type m_nativeSubType; // 0x20
    const hkClass* m_class; // 0x24
    hkTypeManager::Type* m_dataType; // 0x28
    int m_tupleCount; // 0x2C: elements are tuples of this many (0: single values)
    void* m_arrayObject; // 0x30
};

// 0x00F58150 (:206)
hkDataArrayNative::hkDataArrayNative(hkDataWorldNative* world, void* address, int arraySize, int elementStride, hkClassMember::Type type, hkClassMember::Type subType, const hkClass* klass, int tupleCount, void* arrayObject)
    : m_world(world), m_address(address), m_arraySize(arraySize), m_elementStride(elementStride), m_nativeType(type),
      m_nativeSubType(subType), m_class(klass), m_tupleCount(tupleCount), m_arrayObject(arrayObject)
{
    m_dataType = world->getTypeFromMemberType(type, subType, klass, tupleCount);
}

// 0x00F581C0 (:214)
hkTypeManager::Type* hkDataArrayNative::getType() const { return m_dataType; }
// 0x00F581D0 (:218)
hkDataWorld* hkDataArrayNative::getWorld() const { return m_world; }

// 0x00F58DA0 (:222)
void hkDataArrayNative::clear()
{
    hkVariantDataUtil::clearArray(m_world->m_infoReg, m_arrayObject, m_nativeType, m_class);
}

// 0x00F581E0 (:227)
void hkDataArrayNative::reserve(int n)
{
    m_address = hkVariantDataUtil::reserveArray(m_arrayObject, m_nativeType, m_class, n);
}

// 0x00F58DC0 (:232)
void hkDataArrayNative::setSize(int n)
{
    if (n != getSize())
    {
        m_address = hkVariantDataUtil::setArraySize(m_world->m_infoReg, m_arrayObject, m_nativeType, m_class, n);
    }
}

// 0x00F58210 (:240)
int hkDataArrayNative::getSize() const
{
    if (m_arrayObject)
    {
        return hkVariantDataUtil::getArraySize(m_arrayObject);
    }
    return m_arraySize;
}

// 0x00F58230 (:251)
int hkDataArrayNative::getUnderlyingIntegerSize() const
{
    return (m_nativeType == hkClassMember::TYPE_INT64 || m_nativeType == hkClassMember::TYPE_UINT64) ? 8 : 4;
}

// 0x00F58250 (:263)
const hkDataClassImpl* hkDataArrayNative::getClass() const
{
    return m_class ? m_world->findClass(m_class->getName()) : nullptr;
}

// 0x00F586F0 (:267)
const float* hkDataArrayNative::asVec(int index) const
{
    return hkVariantDataUtil::getReals(m_nativeType, m_tupleCount, _getElement(index), m_world->m_buffer);
}

// 0x00F58720 (:271)
const char* hkDataArrayNative::asString(int index) const { return hkVariantDataUtil::getString(m_nativeType, _getElement(index)); }
// 0x00F58740 (:275)
float hkDataArrayNative::asReal(int index) const { return hkVariantDataUtil::getReal(m_nativeType, _getElement(index)); }
// 0x00F58760 (:279)
int hkDataArrayNative::asInt(int index) const { return (int)hkVariantDataUtil::getInt(m_nativeType, m_nativeSubType, _getElement(index)); }
// 0x00F58780 (:283)
int64_t hkDataArrayNative::asInt64(int index) const { return hkVariantDataUtil::getInt(m_nativeType, m_nativeSubType, _getElement(index)); }

// 0x00F58E00 (:287)
hkDataObjectImpl* hkDataArrayNative::asObject(int index) const
{
    hkVariant v = hkVariantDataUtil::getObject(m_nativeType, m_class, m_world->m_vtable, m_world->m_reg, _getElement(index));
    if (v.m_object && v.m_class)
    {
        return hkNativeDataObjectImplCreate(v, m_world, false);
    }
    return nullptr;
}

// 0x00F587A0 (:293)
// An element of an array of tuples is a c-style array; an element of an array of arrays (hkArray) its own array.
hkDataArrayImpl* hkDataArrayNative::asArray(int index) const
{
    void* element = _getElement(index);
    if (m_tupleCount)
    {
        const int stride = m_nativeType == hkClassMember::TYPE_STRUCT
            ? m_class->getObjectSize()
            : hkClassMember::getClassMemberTypeProperties(m_nativeType).m_size;
        return new hkDataArrayNative(m_world, element, m_tupleCount, stride, m_nativeType, m_nativeSubType, m_class, 0, nullptr);
    }
    if (m_nativeType == hkClassMember::TYPE_ARRAY)
    {
        const int stride = (int)hkVariantDataUtil::calcElementSize(m_nativeSubType, hkClassMember::TYPE_VOID, m_class, 0);
        void** arr = static_cast<void**>(element);
        return new hkDataArrayNative(m_world, arr[0], (int)(hkUlong)arr[1], stride, m_nativeSubType, hkClassMember::TYPE_VOID, m_class, 0, nullptr);
    }
    return nullptr;
}

// 0x00F58E70 (:319)
// The member of every element, as a strided array over the same elements.
hkDataArrayImpl* hkDataArrayNative::swizzleObjectMember(const char* name) const
{
    const hkClassMember* mem = m_class->getMemberByName(name);
    return new hkDataArrayNative(m_world, static_cast<char*>(m_address) + mem->getOffset(), m_arraySize, m_elementStride,
                                 mem->getType(), mem->getSubType(), mem->getClass(), mem->getCstyleArraySize(), nullptr);
}

// 0x00F58280 (:329)
// Vector and matrix elements are tuples of reals; enums and flags their storage type.
hkResult hkDataArrayNative::asStridedBasicArray(hkStridedBasicArray& arrOut)
{
    arrOut.m_tupleSize = 1;
    arrOut.m_type = m_nativeType;
    switch (m_nativeType)
    {
        case hkClassMember::TYPE_VECTOR4:
        case hkClassMember::TYPE_QUATERNION:
            arrOut.m_tupleSize = 4;
            arrOut.m_type = hkClassMember::TYPE_REAL;
            break;
        case hkClassMember::TYPE_MATRIX3:
        case hkClassMember::TYPE_ROTATION:
        case hkClassMember::TYPE_QSTRANSFORM:
            arrOut.m_tupleSize = 12;
            arrOut.m_type = hkClassMember::TYPE_REAL;
            break;
        case hkClassMember::TYPE_MATRIX4:
        case hkClassMember::TYPE_TRANSFORM:
            arrOut.m_tupleSize = 16;
            arrOut.m_type = hkClassMember::TYPE_REAL;
            break;
        case hkClassMember::TYPE_ENUM:
        case hkClassMember::TYPE_FLAGS:
            arrOut.m_type = m_nativeSubType;
            arrOut.m_tupleSize = 1;
            break;
        default:
            break;
    }
    arrOut.m_size = getSize();
    arrOut.m_stride = m_elementStride;
    arrOut.m_data = m_address;
    return HK_SUCCESS;
}

// 0x00F58330 (:377)
// Bytes also fill bool and char arrays; any other mismatch is ignored. The data is copied raw.
void hkDataArrayNative::_setAll(hkClassMember::Type type, const void* v, int n)
{
    if (type == hkClassMember::TYPE_UINT8 && (m_nativeType == hkClassMember::TYPE_BOOL || m_nativeType == hkClassMember::TYPE_CHAR))
    {
        type = m_nativeType;
    }
    if (type == m_nativeType)
    {
        setSize(n);
        hkString::memCpy(m_address, v, hkClassMember::getClassMemberTypeProperties(type).m_size * n);
    }
}

// 0x00F58F40 (:410)
// -1: as many reals as the element has.
void hkDataArrayNative::setVec(int index, const float* val)
{
    hkVariantDataUtil::setReals(m_nativeType, m_tupleCount, val, _getElement(index), -1);
}

// 0x00F58F70 (:411)
void hkDataArrayNative::setString(int index, const char* val) { hkVariantDataUtil::setString(m_nativeType, _getElement(index), val); }
// 0x00F58F90 (:412)
void hkDataArrayNative::setReal(int index, float val) { hkVariantDataUtil::setReal(m_nativeType, _getElement(index), val); }
// 0x00F58FC0 (:413)
void hkDataArrayNative::setHalf(int index, hkHalf val) { hkVariantDataUtil::setHalf(m_nativeType, _getElement(index), val); }
// 0x00F58FE0 (:414)
void hkDataArrayNative::setInt(int index, int val) { hkVariantDataUtil::setInt(m_nativeType, _getElement(index), (int64_t)val); }
// 0x00F59010 (:415)
void hkDataArrayNative::setInt64(int index, int64_t val) { hkVariantDataUtil::setInt(m_nativeType, _getElement(index), val); }
// 0x00F586B0 (:416)
void hkDataArrayNative::setArray(int index, hkDataArrayImpl* val) {}
// 0x00F58890 (:420)
void* hkDataArrayNative::_getElement(int index) const { return static_cast<char*>(m_address) + m_elementStride * index; }

// 0x00F57E50 (:445)
hkDataObjectNative::hkDataObjectNative(const hkVariant& v, hkDataWorldNative* world, hkBool hasOwnedRef)
    : m_object(const_cast<hkVariant&>(v)), m_world(world), m_hasOwnedRef(hasOwnedRef)
{
}

// 0x00F588F0 (:461)
// A handle without a class names a virtual object (its class comes from its vtable).
hkDataObjectNative::hkDataObjectNative(const hkDataObject_Handle& h, hkDataWorldNative* world)
    : m_object(h.p0, h.p1 ? static_cast<const hkClass*>(h.p1) : world->m_vtable->getClassFromVirtualInstance(h.p0)),
      m_world(world), m_hasOwnedRef(false)
{
}

// 0x00F58940 (:465)
hkDataObjectNative::~hkDataObjectNative()
{
    if (m_hasOwnedRef)
    {
        static_cast<hkReferencedObject*>(m_object.m_variant.m_object)->removeReference();
    }
}

// 0x00F57ED0 (:475)
// A virtual object is found again from its address alone.
hkDataObject_Handle hkDataObjectNative::getHandle() const
{
    const hkClass* klass = m_object.m_variant.m_class;
    hkDataObject_Handle h;
    h.p0 = m_object.m_variant.m_object;
    h.p1 = klass->hasVtable() ? nullptr : const_cast<hkClass*>(klass);
    return h;
}

// 0x00F57F00 (:488)
const hkDataClassImpl* hkDataObjectNative::getClass() const
{
    const hkClass* klass = m_object.m_variant.m_class;
    return klass ? m_world->findClass(klass->getName()) : nullptr;
}

// 0x00F57F30 (:493)
hkDataObject_Value hkDataObjectNative::accessByName(const char* name)
{
    hkClassMemberAccessor acc(m_object.m_variant, name);
    if (acc.isOk())
    {
        return hkDataObject_Value(this, reinterpret_cast<const _hkDataObject_MemberHandle*>(&acc.getClassMember()));
    }
    return hkDataObject_Value(nullptr, nullptr);
}

// 0x00F57F90 (:505)
unsigned int hkDataObjectNative::hasMember(const char* name) const
{
    hkClassMemberAccessor acc(m_object.m_variant, name);
    return acc.isOk() != false;
}

// 0x00F57FD0 (:511)
unsigned int hkDataObjectNative::isSet(const _hkDataObject_MemberHandle* handle)
{
    hkClassMemberAccessor acc(m_object.m_variant.m_object, reinterpret_cast<const hkClassMember*>(handle));
    return acc.isOk() != false;
}

// 0x00F58010 (:517)
int hkDataObjectNative::getMemberIterator() const { return 0; }
// 0x00F58020 (:521)
unsigned int hkDataObjectNative::isValid(int it) const { return it < m_object.m_variant.m_class->getNumMembers(); }
// 0x00F58040 (:525)
int hkDataObjectNative::getNextMember(int it) const { return it + 1; }
// 0x00F58960 (:529)
const char* hkDataObjectNative::getMemberName(int it) const { return m_object.m_variant.m_class->getMember(it).getName(); }

// 0x00F58050 (:533)
const hkDataObject_Value hkDataObjectNative::getMemberValue(int it) const
{
    const hkClassMember& mem = m_object.m_variant.m_class->getMember(it);
    return hkDataObject_Value(const_cast<hkDataObjectNative*>(this), reinterpret_cast<const _hkDataObject_MemberHandle*>(&mem));
}

// 0x00F58980 (:539)
void hkDataObjectNative::getAllMemberHandles(hkArrayBase<_hkDataObject_MemberHandle const*>& handles) const
{
    static_cast<const hkDataClassNative*>(getClass())->_getAllMemberHandles(handles);
}

// 0x00F58070 (:545)
void hkDataObjectNative::destroy() {}

// 0x00F59040 (:548)
// The array view of an array member: hkArray and hkSimpleArray (the view resizes the array itself),
// hkHomogeneousArray (its own class; none: a void array), hkRelArray (an offset from the member), or any scalar member
// as a c-style array of its count. Other members give an empty void array.
hkDataArrayImpl* hkDataObjectNative::asArray(const _hkDataObject_MemberHandle* handle)
{
    const hkClassMember& mem = *reinterpret_cast<const hkClassMember*>(handle);
    hkClassMemberAccessor acc(m_object.m_variant.m_object, &mem);
    void* address = nullptr;
    int size = 0;
    int stride = 0;
    hkClassMember::Type type = hkClassMember::TYPE_VOID;
    const hkClass* klass = nullptr;
    void* arrayObject = nullptr;
    switch (mem.getType())
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
        case hkClassMember::TYPE_POINTER:
        case hkClassMember::TYPE_STRUCT:
        case hkClassMember::TYPE_ULONG:
        case hkClassMember::TYPE_HALF:
            address = acc.getAddress();
            size = acc.getClassMember().getCstyleArraySize();
            type = mem.getType();
            klass = mem.getClass();
            stride = type == hkClassMember::TYPE_STRUCT ? klass->getObjectSize() : hkClassMember::getClassMemberTypeProperties(type).m_size;
            break;
        case hkClassMember::TYPE_ARRAY:
        case hkClassMember::TYPE_SIMPLEARRAY:
        {
            void** arr = static_cast<void**>(acc.getAddress());
            arrayObject = arr;
            address = arr[0];
            size = (int)(hkUlong)arr[1];
            klass = mem.getClass();
            type = mem.getSubType();
            stride = hkClassMember::getClassMemberTypeProperties(type).m_size;
            if (type == hkClassMember::TYPE_VOID)
            {
                return new hkDataArrayNative(m_world, nullptr, 0, 0, hkClassMember::TYPE_VOID, hkClassMember::TYPE_VOID, nullptr, 0, nullptr);
            }
            if (stride == -1)
            {
                stride = klass->getObjectSize();
            }
            break;
        }
        case hkClassMember::TYPE_HOMOGENEOUSARRAY:
        {
            // { const hkClass* klass; void* data; int size; }
            void** arr = static_cast<void**>(acc.getAddress());
            arrayObject = &arr[1];
            address = arr[1];
            size = (int)(hkUlong)arr[2];
            klass = static_cast<const hkClass*>(*static_cast<void**>(acc.getAddress()));
            if (klass)
            {
                stride = klass->getObjectSize();
                type = hkClassMember::TYPE_STRUCT;
            }
            break;
        }
        case hkClassMember::TYPE_RELARRAY:
        {
            // { hkUint16 size; hkUint16 offset; } relative to the member
            unsigned short* rel = static_cast<unsigned short*>(acc.getAddress());
            address = reinterpret_cast<char*>(rel) + rel[1];
            size = rel[0];
            klass = mem.getClass();
            type = mem.getSubType();
            stride = hkClassMember::getClassMemberTypeProperties(type).m_size;
            if (type == hkClassMember::TYPE_VOID)
            {
                return new hkDataArrayNative(m_world, nullptr, 0, 0, hkClassMember::TYPE_VOID, hkClassMember::TYPE_VOID, nullptr, 0, nullptr);
            }
            if (stride == -1)
            {
                stride = klass->getObjectSize();
            }
            break;
        }
        default:
            break;
    }
    return new hkDataArrayNative(m_world, address, size, stride, type, hkClassMember::TYPE_VOID, klass, 0, arrayObject);
}

// 0x00F59300 (:672)
const char* hkDataObjectNative::asString(const _hkDataObject_MemberHandle* handle)
{
    const hkClassMember* mem = reinterpret_cast<const hkClassMember*>(handle);
    hkClassMemberAccessor acc(m_object.m_variant.m_object, mem);
    return hkVariantDataUtil::getString(mem->getType(), acc.getAddress());
}

// 0x00F59340 (:678)
int64_t hkDataObjectNative::asInt(const _hkDataObject_MemberHandle* handle)
{
    const hkClassMember* mem = reinterpret_cast<const hkClassMember*>(handle);
    hkClassMemberAccessor acc(m_object.m_variant.m_object, mem);
    return hkVariantDataUtil::getInt(mem->getType(), mem->getSubType(), acc.getAddress());
}

// 0x00F59380 (:684)
hkDataObjectImpl* hkDataObjectNative::asObject(const _hkDataObject_MemberHandle* handle)
{
    const hkClassMember* mem = reinterpret_cast<const hkClassMember*>(handle);
    hkClassMemberAccessor acc(m_object.m_variant.m_object, mem);
    hkVariant v = hkVariantDataUtil::getObject(mem->getType(), mem->getClass(), m_world->m_vtable, m_world->m_reg, acc.asRaw());
    if (v.m_object && v.m_class)
    {
        return hkNativeDataObjectImplCreate(v, m_world, false);
    }
    return nullptr;
}

// 0x00F59410 (:691)
const float* hkDataObjectNative::asVec(const _hkDataObject_MemberHandle* handle, int nreal)
{
    const hkClassMember* mem = reinterpret_cast<const hkClassMember*>(handle);
    hkClassMemberAccessor acc(m_object.m_variant.m_object, mem);
    return hkVariantDataUtil::getReals(mem->getType(), mem->getCstyleArraySize(), acc.getAddress(), m_world->m_buffer);
}

// 0x00F59460 (:697)
float hkDataObjectNative::asReal(const _hkDataObject_MemberHandle* handle)
{
    const hkClassMember* mem = reinterpret_cast<const hkClassMember*>(handle);
    hkClassMemberAccessor acc(m_object.m_variant.m_object, mem);
    return hkVariantDataUtil::getReal(mem->getType(), acc.getAddress());
}

// 0x00F58080 (:703)
void hkDataObjectNative::assign(const _hkDataObject_MemberHandle* handle, const hkDataObject_Value& valueIn)
{
    assignValueImpl(this, handle, valueIn);
}

// 0x00F58990 (:707)
// The object must be a native one of this world; null clears the pointer. Unless the member is NOT_OWNED the pointer
// holds a reference.
void hkDataObjectNative::assign(const _hkDataObject_MemberHandle* handle, hkDataObjectImpl* val)
{
    const hkClassMember* mem = reinterpret_cast<const hkClassMember*>(handle);
    const hkBool isReferenced = !mem->getFlags().allAreSet(hkClassMember::NOT_OWNED);
    hkClassMemberAccessor acc(m_object.m_variant.m_object, mem);
    if (val)
    {
        const hkVariant& v = static_cast<hkDataObjectNative*>(val)->m_object.m_variant;
        hkVariantDataUtil::setPointer(*v.m_class, v.m_object, static_cast<void**>(acc.getAddress()), isReferenced);
    }
    else if (mem->getClass())
    {
        hkVariantDataUtil::setPointer(*mem->getClass(), nullptr, static_cast<void**>(acc.getAddress()), isReferenced);
    }
}

// 0x00F580A0 (:735)
void hkDataObjectNative::assign(const _hkDataObject_MemberHandle* handle, hkDataArrayImpl* value) {}

// 0x00F594A0 (:739)
void hkDataObjectNative::assign(const _hkDataObject_MemberHandle* handle, const char* value)
{
    const hkClassMember* mem = reinterpret_cast<const hkClassMember*>(handle);
    hkClassMemberAccessor acc(m_object.m_variant.m_object, mem);
    hkVariantDataUtil::setString(mem->getType(), acc.getAddress(), value);
}

// 0x00F594E0 (:745)
void hkDataObjectNative::assign(const _hkDataObject_MemberHandle* handle, float r)
{
    const hkClassMember* mem = reinterpret_cast<const hkClassMember*>(handle);
    hkClassMemberAccessor acc(m_object.m_variant.m_object, mem);
    hkVariantDataUtil::setReal(mem->getType(), acc.getAddress(), r);
}

// 0x00F59520 (:751)
void hkDataObjectNative::assign(const _hkDataObject_MemberHandle* handle, hkHalf r)
{
    const hkClassMember* mem = reinterpret_cast<const hkClassMember*>(handle);
    hkClassMemberAccessor acc(m_object.m_variant.m_object, mem);
    hkVariantDataUtil::setReal(mem->getType(), acc.getAddress(), r.getReal());
}

// 0x00F59570 (:757)
void hkDataObjectNative::assign(const _hkDataObject_MemberHandle* handle, const float* r, int nreal)
{
    const hkClassMember* mem = reinterpret_cast<const hkClassMember*>(handle);
    hkClassMemberAccessor acc(m_object.m_variant.m_object, mem);
    hkVariantDataUtil::setReals(mem->getType(), mem->getCstyleArraySize(), r, acc.getAddress(), nreal);
}

// 0x00F595C0 (:763)
void hkDataObjectNative::assign(const _hkDataObject_MemberHandle* handle, int64_t valueIn)
{
    const hkClassMember* mem = reinterpret_cast<const hkClassMember*>(handle);
    hkClassMemberAccessor acc(m_object.m_variant.m_object, mem);
    hkVariantDataUtil::setInt(mem->getType(), mem->getSubType(), acc.getAddress(), valueIn);
}

// 0x00F59610 (:769)
void hkDataObjectNative::assign(const _hkDataObject_MemberHandle* handle, int valueIn)
{
    const hkClassMember* mem = reinterpret_cast<const hkClassMember*>(handle);
    hkClassMemberAccessor acc(m_object.m_variant.m_object, mem);
    hkVariantDataUtil::setInt(mem->getType(), mem->getSubType(), acc.getAddress(), (int64_t)valueIn);
}

// 0x00F59660 (:775)
// The owner is not known here (null); the default value is not filled in.
void hkDataObjectNative::getMemberInfo(const _hkDataObject_MemberHandle* handle, hkDataClass_MemberInfo& infoOut)
{
    hkDataClassNative::_fillMemberInfo(nullptr, m_world, *reinterpret_cast<const hkClassMember*>(handle), infoOut);
}

// 0x00F580B0 (:781)
unsigned int hkDataObjectNative::createdDuringPatching() const { return 0; }
// 0x00F580C0 (:787)
void hkDataObjectNative::destroyMember(const char*) {}

// 0x00F573B0 (:797)
// The variant's class is replaced by the world's registered class of that name; an object of an unregistered class
// is skipped with a warning.
hkDataObjectImpl* hkNativeDataObjectImplCreate(hkVariant& v, hkDataWorldNative* w, hkBool isRefOwned)
{
    if (v.m_class)
    {
        hkDataClassNative* cls = static_cast<hkDataClassNative*>(w->findClass(v.m_class->getName()));
        if (cls == nullptr)
        {
            char buf[512];
            hkErrStream os(buf, sizeof(buf));
            os << "Ignore object at 0x" << v.m_object << ". Class '" << v.m_class->getName()
               << "' is not registered in the provided hkDataWorldNative.";
            hkErrorFwd::messageWarning(0x1800473A, buf, "Data\\Native\\hkDataObjectNative.cpp", 0x326);
            return nullptr;
        }
        v.m_class = cls->m_class;
    }
    return new hkDataObjectNative(v, w, isRefOwned);
}

// 0x00F574D0 (:822)
// Struct elements are stored in place and cannot be set.
void hkDataArrayNative::setObject(int index, hkDataObjectImpl* val)
{
    if (m_nativeType != hkClassMember::TYPE_STRUCT)
    {
        const hkVariant& v = static_cast<hkDataObjectNative*>(val)->m_object.m_variant;
        hkVariantDataUtil::setPointer(*v.m_class, v.m_object, static_cast<void**>(_getElement(index)), true);
    }
}

// 0x00F57860 (:848)
// The registries default to the built-in ones.
hkDataWorldNative::hkDataWorldNative(hkBool accessSerializeIgnored)
    : m_reg(nullptr), m_vtable(nullptr), m_infoReg(nullptr), m_accessSerializeIgnored(accessSerializeIgnored)
{
    m_contents.m_class = nullptr;
    m_contents.m_object = nullptr;
    hkBuiltinTypeRegistry& builtin = hkBuiltinTypeRegistry::getInstance();
    m_vtable = builtin.getVtableClassRegistry();
    m_reg = builtin.getClassNameRegistry();
    m_infoReg = builtin.getTypeInfoRegistry();
}

// 0x00F57940 (:857)
// The registry references are released here (hkRefPtr has no destructor in this port; rfg.exe releases them after
// the other members).
hkDataWorldNative::~hkDataWorldNative()
{
    for (hkStringMap<hkDataClassNative*>::Iterator it = m_classes.getIterator(); m_classes.isValid(it); it = m_classes.getNext(it))
    {
        delete m_classes.getValue(it);
    }
    m_infoReg = nullptr;
    m_vtable = nullptr;
    m_reg = nullptr;
}

// 0x00F57360 (:866)
hkTypeManager::Type* hkDataWorldNative::getTypeFromMemberType(hkClassMember::Type mtype, hkClassMember::Type stype, const hkClass* klass, int count)
{
    return hkDataObjectUtil::getTypeFromMemberType(m_typeManager, mtype, stype, klass, count);
}

// 0x00F576D0 (:871)
void hkDataWorldNative::setClassRegistry(const hkClassNameRegistry* r) { m_reg = r; }
// 0x00F57700 (:876)
void hkDataWorldNative::setVtableRegistry(const hkVtableClassRegistry* vt) { m_vtable = vt; }

// 0x00F57500 (:881)
hkDataObjectImpl* hkDataWorldNative::wrapObject(void* object, const hkClass& klass) const
{
    hkVariant v;
    v.m_object = object;
    v.m_class = &klass;
    return hkNativeDataObjectImplCreate(v, const_cast<hkDataWorldNative*>(this), false);
}

// 0x00F57730 (:888)
// A virtual object gets its most derived class; an unknown class leaves no contents.
void hkDataWorldNative::setContents(void* object, const hkClass& klass)
{
    m_contents.m_object = object;
    const hkClass* registered = m_reg->getClassByName(klass.getName());
    if (klass.hasVtable())
    {
        m_contents.m_class = hkVariantDataUtil::findMostDerivedClass(object, m_vtable, m_reg);
    }
    else
    {
        m_contents.m_class = registered;
    }
    if (m_contents.m_class == nullptr)
    {
        m_contents.m_object = nullptr;
    }
}

// 0x00F57530 (:916)
// Original: a wrapper is made and cached for any name, also an unregistered one (its m_class is null) and a null
// name; the map keeps the caller's name pointer.
hkDataClassImpl* hkDataWorldNative::findClass(const char* name) const
{
    const hkClass* klass = nullptr;
    if (name)
    {
        if (hkDataClassNative* cls = m_classes.getWithDefault(name, nullptr))
        {
            return cls;
        }
        klass = m_reg->getClassByName(name);
    }
    hkDataClassNative* cls = new hkDataClassNative(klass, const_cast<hkDataWorldNative*>(this));
    const_cast<hkStringMap<hkDataClassNative*>&>(m_classes).insert(name, cls);
    return cls;
}

// 0x00F57AE0 (:929)
// The serializable classes of the class registry.
void hkDataWorldNative::findAllClasses(hkArray<hkDataClassImpl*, hkContainerTempAllocator>& classesOut) const
{
    if (m_reg == nullptr)
    {
        return;
    }
    hkArray<const hkClass*> classes;
    m_reg->getClasses(classes);
    for (int i = 0; i < classes.getSize(); i++)
    {
        if (classes[i]->getFlags().noneIsSet(hkClass::FLAGS_NOT_SERIALIZABLE))
        {
            classesOut.pushBack(findClass(classes[i]->getName()));
        }
    }
}

// 0x00F575C0 (:949)
hkDataObject hkDataWorldNative::findObject(const hkDataObject_Handle& handle) const
{
    return hkDataObject(new hkDataObjectNative(handle, const_cast<hkDataWorldNative*>(this)));
}

// 0x00F57650 (:954)
hkEnum<enum hkDataWorld::DataWorldType, int> hkDataWorldNative::getType() const
{
    hkEnum<enum hkDataWorld::DataWorldType, int> type;
    type = TYPE_NATIVE;
    return type;
}

// 0x00F57660 (:959)
hkDataObject hkDataWorldNative::getContents() const
{
    return hkDataObject(new hkDataObjectNative(m_contents, const_cast<hkDataWorldNative*>(this), false));
}

// 0x00F577A0 (:964)
// A zeroed object of the registered class, finished by its type info. Original: the new object is not owned by the
// wrapper (it leaks unless something else takes it).
hkDataObjectImpl* hkDataWorldNative::newObject(const hkDataClass& klass, bool) const
{
    const hkClass* nativeClass = m_reg->getClassByName(klass.getName());
    void* object = hkMemory::heapBlockAlloc(nativeClass->getObjectSize());
    if (object)
    {
        hkString::memSet(object, 0, nativeClass->getObjectSize());
        const hkTypeInfo* info = hkBuiltinTypeRegistry::getInstance().getTypeInfoRegistry()->getTypeInfo(klass.getName());
        if (info)
        {
            info->finishLoadedObject(object, 0);
        }
    }
    hkVariant v;
    v.m_object = object;
    v.m_class = nativeClass;
    return hkNativeDataObjectImplCreate(v, const_cast<hkDataWorldNative*>(this), false);
}

// 0x00F57390 (:983)
hkDataClassImpl* hkDataWorldNative::newClass(const hkDataClass::Cinfo& cinfo) { return nullptr; }
// 0x00F573A0 (:988)
hkDataArrayImpl* hkDataWorldNative::newArray(hkDataObject& _obj, const _hkDataObject_MemberHandle* handle, const hkDataClass_MemberInfo& minfo) const { return nullptr; }
// 0x00F596C0 (.h)
hkTypeManager& hkDataWorldNative::getTypeManager() { return m_typeManager; }
