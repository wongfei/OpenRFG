// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/data/hkdataobject.cpp
//
// Havok 2014.2.5 (stub_impl.md S4, serializer SC): the out-of-line wrapper members and hkDataWorld's default
// mutators (no-ops: only the dictionary world can change its classes). From the decompile and asm.
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/data/hkdataobject.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/string/hkstringbuf.h"

// 0x00F51220 (:18)
// minfos must already hold getNumDeclaredMembers() entries.
void hkDataClass::getAllDeclaredMemberInfo(hkArrayBase<hkDataClass_MemberInfo>& minfos) const
{
    const int n = m_impl->getNumDeclaredMembers();
    for (int i = 0; i < n; i++)
    {
        m_impl->getDeclaredMemberInfo(i, minfos[i]);
    }
}

// 0x00F51270 (:29)
// Original: the members' valuePtr is left as it was (not the member's default).
void hkDataClass::getCinfo(Cinfo& cinfo) const
{
    cinfo.name = m_impl->getName();
    cinfo.version = m_impl->getVersion();
    cinfo.parent = m_impl->getParent() ? m_impl->getParent()->getName() : nullptr;
    const int n = m_impl->getNumDeclaredMembers();
    cinfo.members.setSize(n);
    for (int i = 0; i < n; i++)
    {
        hkDataClass_MemberInfo minfo;
        m_impl->getDeclaredMemberInfo(i, minfo);
        cinfo.members[i].set(minfo.m_name, minfo.m_type);
    }
}

// 0x00F51110 (:57)
hkTypeManager::Type* hkDataObject_Value::getType() const
{
    hkDataClass_MemberInfo minfo;
    m_impl->getMemberInfo(m_handle, minfo);
    return minfo.m_type;
}

// 0x00F51150 (:71)
void hkDataWorld::renameClass(hkDataClass& klass, const char* newName) {}
// 0x00F51160 (:72)
void hkDataWorld::removeClass(hkDataClass& klass) {}
// 0x00F51170 (:74)
void hkDataWorld::setClassVersion(hkDataClass& klass, int newVersion) {}
// 0x00F51180 (:75)
void hkDataWorld::setClassParent(hkDataClass& klass, hkDataClass& parent) {}
// 0x00F51190 (:77)
void hkDataWorld::renameClassMember(hkDataClass& klass, const char* oldName, const char* newName) {}
// 0x00F511A0 (:78)
void hkDataWorld::removeClassMember(hkDataClass& klass, const char* name) {}
// 0x00F511B0 (:79)
void hkDataWorld::findObjectsByBaseClass(const char* baseClassName, hkArray<hkDataObjectImpl*, hkContainerTempAllocator>& objectsOut) const {}
// 0x00F511C0 (:80)
void hkDataWorld::findObjectsByExactClass(const char* className, hkArray<hkDataObjectImpl*, hkContainerTempAllocator>& objectsOut) const {}
// 0x00F511D0 (:81)
void hkDataWorld::castObject(hkDataObject& obj, const hkDataClass& objClass) {}
// 0x00F511E0 (:82)
void hkDataWorld::setClassMemberDefault(hkDataClass& klass, const char* name, const void* valuePtr) {}
// 0x00F511F0 (:85)
hkDataClassImpl* hkDataWorld::copyClassFromWorld(const char* name, const hkDataWorld& worldFrom) { return nullptr; }
// 0x00F51200 (:90)
void hkDataWorld::addClassMember(hkDataClass& klass, const char* name, hkTypeManager::Type* type, const void* valuePtr) {}

// 0x00F51360 (:95)
// With a class name the expression is completed with the class terminal "C<className>;". An expression that does
// not parse adds nothing.
void hkDataWorld::addClassMemberTypeExpression(hkDataClass& klass, const char* name, const char* expr, const char* className, const void* valuePtr)
{
    hkTypeManager& typeManager = getTypeManager();
    hkTypeManager::Type* type;
    if (className == nullptr)
    {
        type = typeManager.parseTypeExpression(expr);
    }
    else
    {
        hkStringBuf buf;
        buf.append(expr, -1);
        buf.append("C", -1);
        buf.append(className, -1);
        buf.append(";", -1);
        type = typeManager.parseTypeExpression(buf.cString());
    }
    if (type)
    {
        addClassMember(klass, name, type, valuePtr);
    }
}

// 0x00F51210 (:124)
void hkDataObject::destroyMember(const char* name)
{
    m_impl->destroyMember(name);
}
