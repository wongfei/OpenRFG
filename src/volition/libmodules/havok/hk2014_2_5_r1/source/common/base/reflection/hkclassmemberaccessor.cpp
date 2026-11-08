// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/hkclassmemberaccessor.cpp
//
// Havok 2014.2.5 (stub_impl.md S4, serializer SB): typed access to a reflected member of an object, from rfg.exe.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/hkclassmemberaccessor.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/string/hkstringptr.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/hkclass.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/hkclassmember.h"

// 0x00FC0660 (:14)
void hkClassMemberAccessor::connect(void* obj, const hkClassMember* mem)
{
    m_member = mem;
    m_address = static_cast<char*>(obj) + mem->getOffset();
}

// 0x00FC0680 (:23): not connected (null address) when klass has no such member
void hkClassMemberAccessor::connect(void* obj, const hkClass& klass, const char* memberName)
{
    m_address = nullptr;
    m_member = klass.getMemberByName(memberName);
    if (m_member != nullptr)
    {
        m_address = static_cast<char*>(obj) + m_member->getOffset();
    }
}

// 0x00FC06B0 (:40)
hkClassMemberAccessor::hkClassMemberAccessor(void* obj, const hkClassMember* mem)
{
    connect(obj, mem);
}

// 0x00FC06D0 (:45)
hkClassMemberAccessor::hkClassMemberAccessor(const hkVariant& var, const char* memberName)
{
    connect(var.m_object, *var.m_class, memberName);
}

// 0x00FC06F0 (:50)
hkClassMemberAccessor::hkClassMemberAccessor(void* obj, const hkClass& klass, const char* memberName)
{
    connect(obj, klass, memberName);
}

// 0x00FC0710 (:55)
hkBool hkClassMemberAccessor::isOk() const
{
    return m_address != nullptr;
}

// 0x00FC0720 (:60)
void* hkClassMemberAccessor::asRaw() const
{
    return m_address;
}

// 0x00FC0730 (:66)
void* hkClassMemberAccessor::getAddress() const
{
    return m_address;
}

// 0x00FC0740 (:72)
void*& hkClassMemberAccessor::asPointer(int index) const
{
    return static_cast<void**>(m_address)[index];
}

// 0x00FC0750 (:78)
char*& hkClassMemberAccessor::asCstring(int index) const
{
    return static_cast<char**>(m_address)[index];
}

// 0x00FC0760 (:84)
hkStringPtr& hkClassMemberAccessor::asStringPtr(int index) const
{
    return static_cast<hkStringPtr*>(m_address)[index];
}

// 0x00FC0770 (:90)
hkBool& hkClassMemberAccessor::asBool(int index) const
{
    return static_cast<hkBool*>(m_address)[index];
}

// 0x00FC0780 (:96)
float& hkClassMemberAccessor::asReal(int index) const
{
    return static_cast<float*>(m_address)[index];
}

// 0x00FC0790 (:102)
hkHalf& hkClassMemberAccessor::asHalf(int index) const
{
    return static_cast<hkHalf*>(m_address)[index];
}

// 0x00FC07A0 (:108)
int& hkClassMemberAccessor::asInt32(int index) const
{
    return static_cast<int*>(m_address)[index];
}

// 0x00FC07B0 (:114)
unsigned int& hkClassMemberAccessor::asUint32(int index) const
{
    return static_cast<unsigned int*>(m_address)[index];
}

// 0x00FC07C0 (:120)
int64_t& hkClassMemberAccessor::asInt64(int index) const
{
    return static_cast<int64_t*>(m_address)[index];
}

// 0x00FC07D0 (:126)
uint64_t& hkClassMemberAccessor::asUint64(int index) const
{
    return static_cast<uint64_t*>(m_address)[index];
}

// 0x00FC07E0 (:132)
unsigned long& hkClassMemberAccessor::asUlong(int index) const
{
    return static_cast<unsigned long*>(m_address)[index];
}

// 0x00FC07F0 (:138)
short& hkClassMemberAccessor::asInt16(int index) const
{
    return static_cast<short*>(m_address)[index];
}

// 0x00FC0800 (:144)
unsigned short& hkClassMemberAccessor::asUint16(int index) const
{
    return static_cast<unsigned short*>(m_address)[index];
}

// 0x00FC0810 (:150)
signed char& hkClassMemberAccessor::asInt8(int index) const
{
    return static_cast<signed char*>(m_address)[index];
}

// 0x00FC0820 (:156)
unsigned char& hkClassMemberAccessor::asUint8(int index) const
{
    return static_cast<unsigned char*>(m_address)[index];
}

// 0x00FC0830 (:162)
hkClassMemberAccessor::Vector4& hkClassMemberAccessor::asVector4(int index) const
{
    return static_cast<Vector4*>(m_address)[index];
}

// 0x00FC0840 (:168)
hkClassMemberAccessor::Matrix3& hkClassMemberAccessor::asMatrix3(int index) const
{
    return static_cast<Matrix3*>(m_address)[index];
}

// 0x00FC0850 (:174)
hkClassMemberAccessor::Transform& hkClassMemberAccessor::asTransform(int index) const
{
    return static_cast<Transform*>(m_address)[index];
}

// 0x00FC0860 (:180)
hkClassMemberAccessor::Matrix3& hkClassMemberAccessor::asRotation(int index) const
{
    return static_cast<Matrix3*>(m_address)[index];
}

// 0x00FC0870 (:186)
hkClassMemberAccessor::SimpleArray& hkClassMemberAccessor::asSimpleArray(int index) const
{
    return static_cast<SimpleArray*>(m_address)[index];
}

// 0x00FC0880 (:192)
hkClassMemberAccessor::HomogeneousArray& hkClassMemberAccessor::asHomogeneousArray(int index) const
{
    return static_cast<HomogeneousArray*>(m_address)[index];
}

// 0x00FC0890 (:198)
hkVariant& hkClassMemberAccessor::asVariant(int index) const
{
    return static_cast<hkVariant*>(m_address)[index];
}

// 0x00FC08A0 (:204)
const hkClassMember& hkClassMemberAccessor::getClassMember() const
{
    return *m_member;
}

// 0x00FC08B0 (:210): a member of this struct member
hkClassMemberAccessor hkClassMemberAccessor::member(const char* name) const
{
    return hkClassMemberAccessor(m_address, m_member->getStructClass(), name);
}

// 0x00FC0920 (:219): this struct member as an object
hkClassAccessor hkClassMemberAccessor::object() const
{
    return hkClassAccessor(m_address, &m_member->getStructClass());
}

// 0x00FC08E0 (:226)
hkClassAccessor::hkClassAccessor(void* object, const hkClass* klass)
{
    m_variant.m_object = object;
    m_variant.m_class = klass;
}

// 0x00FC0900 (:235)
hkClassAccessor::hkClassAccessor(hkVariant& v)
{
    m_variant = v;
}
