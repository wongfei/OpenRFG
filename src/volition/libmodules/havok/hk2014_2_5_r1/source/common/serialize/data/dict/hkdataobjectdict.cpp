// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/data/dict/hkdataobjectdict.cpp
//
// Havok 2014.2.5 (stub_impl.md S4, serializer SC): the dictionary data world, the world the tagfile readers load into.
// Classes are hkDataClassDict (members by interned name), objects hkDataObjectDict (only the members set are stored,
// the rest come from the class defaults), arrays one implementation per element type. Member names are interned in
// the world's tracker, so names compare by pointer; a member handle is the class's MemberInfo. The private types
// and their layouts are rfg.pdb's (as in the 2014.1 SDK, none of them is in a header). From the decompile and asm.
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/data/dict/hkdataobjectdict.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/localarray/hklocalarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/string/hkstring.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/hkclass.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/hkclassmemberaccessor.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/util/hkvariantdatautil.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/router/hkmemoryrouter.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/error/hkerror.h"
#include <new>
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkrefptr.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/data/util/hkdataobjectutil.h"

// ---- interned strings and values

namespace
{
    // size 0x4
    // A string of the world's intern table (equal strings are the same pointer).
    struct InternedString
    {
        // 0x00F462B0 (:44)
        InternedString() : m_cachedString(nullptr) {}
        // 0x00F462C0 (:46)
        unsigned int operator==(const InternedString& o) const { return m_cachedString == o.m_cachedString; }
        unsigned int operator!=(const InternedString& o) const { return m_cachedString != o.m_cachedString; }
        // 0x00F462E0 (:49)
        const char* getString() const { return m_cachedString; }
        // 0x00F462F0 (:54)
        explicit InternedString(const char* s) : m_cachedString(s) {}

        const char* m_cachedString; // 0x00
    };

    // size 0xC
    // A reference-counted holder of an interned class name: the classes of one name share it, so a rename changes
    // them all.
    class InternedStringRefCounted : public hkReferencedObject
    {
    public:
        HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int nbytes) 0x00F49220, delete(void* p, unsigned int nbytes) 0x00F49240 (:64)

        // 0x00F49280 (:67): the static null instance; its memory size is set (12) so references are counted
        InternedStringRefCounted() : m_cachedString(nullptr)
        {
            setMemorySizeAndFlags(sizeof(InternedStringRefCounted));
        }

        // 0x00F46300 (:72)
        explicit InternedStringRefCounted(const InternedString& s) : m_cachedString(s.m_cachedString) {}
        // 0x00F46320 (:78)
        const char* getString() const { return m_cachedString; }

        const char* m_cachedString; // 0x08
    };

    InternedStringRefCounted InternedNullRefCounted;

    // size 0x4
    struct InternedStringHandle
    {
        // 0x00F4EB00 (:90)
        const char* getString() const { return m_cached->m_cachedString; }
        // 0x00F4B560 (:91)
        InternedStringHandle() : m_cached(nullptr) { m_cached = &InternedNullRefCounted; }
        ~InternedStringHandle() { m_cached = nullptr; }
        // 0x00F4B590 (:95)
        void clear() { m_cached = &InternedNullRefCounted; }

        hkRefPtr<InternedStringRefCounted> m_cached; // 0x00
    };

    // size 0x8
    // A member value; which field holds it follows the member's type.
    union Union
    {
        static void clearArray(hkTypeManager::Type* typeIn, Union* u, int n, unsigned int destroyStructs);
        void clear(hkTypeManager::Type* typeIn, unsigned int destroyStructs);

        int64_t i;
        void* p;
        float r;
        char* s;
        hkDataObjectImpl* o;
        hkDataArrayImpl* a;
    };

    // 0x00F46340 (:115)
    // Releases n values of the type. Original: for an array or tuple type only the first value is released (an object
    // or array slot holds a single array).
    void Union::clearArray(hkTypeManager::Type* typeIn, Union* u, int n, unsigned int destroyStructs)
    {
        const hkTypeManager::SubType subType = typeIn->getSubType();
        if (subType == hkTypeManager::SUB_TYPE_ARRAY || subType == hkTypeManager::SUB_TYPE_TUPLE)
        {
            if (u->o)
            {
                u->o->removeReference();
            }
            u->p = nullptr;
        }
        if (subType == hkTypeManager::SUB_TYPE_CSTRING)
        {
            for (int i = 0; i < n; i++)
            {
                hkString::strFree(u[i].s);
                u[i].p = nullptr;
            }
        }
        else if (subType == hkTypeManager::SUB_TYPE_CLASS)
        {
            // a struct: owned, destroyed with its members on request
            for (int i = 0; i < n; i++)
            {
                hkDataObjectImpl* o = u[i].o;
                u[i].p = nullptr;
                if (o)
                {
                    if (destroyStructs)
                    {
                        o->destroy();
                    }
                    o->removeReference();
                }
            }
        }
        else if (subType == hkTypeManager::SUB_TYPE_POINTER)
        {
            for (int i = 0; i < n; i++)
            {
                hkDataObjectImpl* o = u[i].o;
                u[i].p = nullptr;
                if (o)
                {
                    o->removeReference();
                }
            }
        }
    }

    // 0x00F46440 (:189)
    void Union::clear(hkTypeManager::Type* typeIn, unsigned int destroyStructs)
    {
        clearArray(typeIn, this, 1, destroyStructs);
    }
}

// ---- declarations

// size 0x7C
// The world's registry: classes by name, derived classes by parent, objects by class, the struct arrays of each
// class, and the intern tables (member names, class name holders).
class hkDataWorldDict::ObjectTracker
{
public:
    typedef hkSerializeMultiMap<hkDataClassDict*, hkDataObjectDict*, hkPointerMap<hkDataClassDict*, int> > ObjectsFromClassMap;
    typedef hkSerializeMultiMap<hkDataClassDict*, hkDataClassDict*, hkPointerMap<hkDataClassDict*, int> > DerivedClassFromParentMap;
    typedef hkPointerMap<hkDataArrayImpl*, int> ArrayMap;
    typedef hkPointerMap<hkDataClassDict*, ArrayMap*> ClassToArraysMap;

    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(ObjectTracker); // new(unsigned int nbytes) 0x00F49310, delete(void* p, unsigned int nbytes) 0x00F49330 (:240)
    ObjectTracker(hkMemoryAllocator* mall); // 0x00F482D0
    ~ObjectTracker(); // 0x00F47730
    void trackClass(hkDataClassDict* klass); // 0x00F509A0
    void untrackClass(hkDataClassDict* klass); // 0x00F50CF0
    hkDataClassDict* findTrackedClass(const char* name) const; // 0x00F49360
    void findTrackedClasses(hkArray<hkDataClassImpl*, hkContainerTempAllocator>& classesOut) const; // 0x00F4E480
    void retrackRenamedClass(const char* oldName, const char* newName); // 0x00F4BE70
    void retrackDerivedClass(hkDataClassDict* oldParent, hkDataClassDict* klass); // 0x00F50F50
    void trackStructArray(hkDataArrayImpl* a); // 0x00F4E400
    void untrackStructArray(hkDataArrayImpl* a); // 0x00F4FC30
    void getTrackedStructArrays(hkDataClassDict* c, unsigned int baseClass, hkArray<hkDataArrayImpl*, hkContainerTempAllocator>& arraysOut); // 0x00F4FDC0
    void trackObject(hkDataObjectDict* obj); // 0x00F50970
    hkDataObjectDict* getTopLevelObject(); // 0x00F46CF0
    void getTrackedObjects(const char* className, unsigned int baseClass, unsigned int addStructs, hkArray<hkDataObjectImpl*, hkContainerTempAllocator>& objectsOut) const; // 0x00F500B0
    void retractCastedObject(hkDataObjectDict* obj, hkDataClassDict* newClass); // 0x00F50FB0
    InternedString intern(const char* sin); // 0x00F4A340
    InternedStringHandle internClassNameHandle(const char* sin); // 0x00F4E510

private:
    void replaceClassNameInInternHandle(const char* oldName, const char* newName); // 0x00F4A390
    void removeClassContent(hkDataClassDict* klass); // 0x00F50A10
    void findTrackedObjectsByBase(const char* className, unsigned int baseClass, unsigned int addStructs, hkArray<hkDataObjectImpl*, hkContainerTempAllocator>& objectsOut) const; // 0x00F4FED0

public:
    hkMemoryAllocator* m_allocator; // 0x00
    hkStringMap<hkDataClassDict*> m_classes; // 0x04
    DerivedClassFromParentMap m_derivedFromParent; // 0x14
    hkDataObjectDict* m_topLevelObject; // 0x30: the first object made
    ObjectsFromClassMap m_objectsFromClass; // 0x34
    hkStringMap<char*> m_interns; // 0x50: key and value are the same copy
    hkStringMap<InternedStringRefCounted*> m_internedClassNames; // 0x60
    ClassToArraysMap m_arraysFromClass; // 0x70
};

// size 0x40
class hkDataClassDict : public hkDataClassImpl
{
public:
    // size 0x18
    struct MemberInfo
    {
        // 0x00F46460 (:304)
        MemberInfo() : m_type(nullptr) { m_default.i = 0; }
        // 0x00F46470 (:309)
        MemberInfo(const MemberInfo& other) : m_name(other.m_name), m_default(other.m_default), m_type(other.m_type), m_valuePtr(other.m_valuePtr) {}

        // 0x00F49550 (:320)
        void getMemberInfo(const hkDataClassImpl* cls, hkDataClass_MemberInfo& info) const
        {
            info.m_name = m_name.getString();
            info.m_owner = cls;
            info.m_type = m_type;
            info.m_valuePtr = m_valuePtr;
        }

        InternedString m_name; // 0x00
        Union m_default; // 0x08
        hkTypeManager::Type* m_type; // 0x10
        const void* m_valuePtr; // 0x14
    };

    // size 0x10
    struct Enum
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(Enum); // delete(void* p, unsigned int nbytes) 0x00F49380 (:335)

        // size 0x8
        struct Item
        {
            InternedString name; // 0x00
            int value; // 0x04
        };

        InternedString m_name; // 0x00
        hkArrayBase<Item> m_items; // 0x04
    };

    // 0x00F464A0 (:354)
    hkMemoryAllocator& getAllocator() const { return *m_world->m_allocator; }
    InternedString intern(const char* in) const; // 0x00F4B5B0
    InternedStringHandle internClassNameHandle(const char* in) const; // 0x00F4EA40
    hkDataClassDict(hkDataWorldDict* root, const char* name, int version); // 0x00F4EA60
    virtual ~hkDataClassDict(); // vtbl+0x00 0x00F4EDD0
    virtual hkDataWorldDict* getWorld() const; // vtbl+0x04 0x00F4EAC0
    virtual const char* getName() const; // vtbl+0x08 0x00F4EB10
    virtual int getVersion() const; // vtbl+0x0C 0x00F4EAD0
    virtual hkDataClassImpl* getParent() const; // vtbl+0x10 0x00F4EB60
    virtual hkBool isSuperClass(const hkDataClassImpl* k) const; // vtbl+0x14 0x00F4EB70
    virtual int getNumDeclaredMembers() const; // vtbl+0x18 0x00F4EB20
    virtual int getDeclaredMemberIndexByName(const char* nameIn) const; // vtbl+0x20 0x00F4EC00
    virtual void getDeclaredMemberInfo(int i, hkDataClass_MemberInfo& info) const; // vtbl+0x1C 0x00F4EB30
    virtual int getNumMembers() const; // vtbl+0x24 0x00F4EC70
    virtual int getMemberIndexByName(const char* nameIn) const; // vtbl+0x2C 0x00F4EC90
    virtual void getMemberInfo(int memberIndex, hkDataClass_MemberInfo& info) const; // vtbl+0x28 0x00F4ED20
    virtual void getAllMemberInfo(hkArrayBase<hkDataClass_MemberInfo>& infos) const; // vtbl+0x30 0x00F4ED70
    void _getAllMemberHandles(hkArrayBase<_hkDataObject_MemberHandle const*>& handles) const; // 0x00F4B610
    // 0x00F464B0 (:606)
    void setVersion(int v) { m_version = v; }
    static int _findDeclaredMemberByName(InternedString name, hkArrayBase<MemberInfo>& info); // 0x00F493B0
    void addMember(InternedString name, hkTypeManager::Type* type, const void* valuePtr); // 0x00F4CE00
    hkResult removeMember(InternedString name); // 0x00F4CE70
    void renameMember(InternedString oldName, InternedString newName); // 0x00F493E0
    void setMemberDefault(InternedString name, const void* valuePtr); // 0x00F49420
    const MemberInfo& getMemberInfoByIndex(int memberIndex) const; // 0x00F4B650
    const MemberInfo* getMemberInfoByName(InternedString name) const; // 0x00F4B690
    void setParent(hkDataClassDict* newParent); // 0x00F49460
    void selfDestruct(); // 0x00F4CEF0

    hkDataWorldDict* m_world; // 0x0C
    InternedStringHandle m_name; // 0x10
    int m_version; // 0x14
    hkRefPtr<hkDataClassDict> m_parent; // 0x18
    hkArrayBase<Enum*> m_enums; // 0x1C
    hkArrayBase<MemberInfo> m_memberInfo; // 0x28
    hkArrayBase<MemberInfo> m_hiddenMemberInfo; // 0x34
};

// size 0x20
class hkDataObjectDict : public hkDataObjectImpl
{
public:
    // size 0x10
    struct MemberData
    {
        // 0x00F49630
        MemberData() {}
        InternedString m_memberName; // 0x00
        Union m_value; // 0x08
    };

    // size 0x8
    struct MemberAccess
    {
        // 0x00F49570 (:762)
        MemberAccess(const hkDataClassDict::MemberInfo* m, int i) : info(m), index(i) {}
        const hkDataClassDict::MemberInfo* info; // 0x00
        int index; // 0x04: into m_memberData
    };

    // 0x00F494F0 (:733)
    hkMemoryAllocator& getAllocator() { return *m_class->m_world->m_allocator; }
    // 0x00F49640 (:738)
    hkDataWorldDict* getInnerWorld() const { return m_class->m_world; }
    hkDataObjectDict(const hkDataClassDict* k, bool createdDuringPatchingFlag); // 0x00F49500
    virtual ~hkDataObjectDict(); // vtbl+0x00 0x00F4EEA0
    virtual unsigned int createdDuringPatching() const; // vtbl+0x6C 0x00F49590
    virtual hkDataObject_Handle getHandle() const; // vtbl+0x04 0x00F495A0
    virtual const hkDataClassImpl* getClass() const; // vtbl+0x08 0x00F4B730
    virtual void getAllMemberHandles(hkArrayBase<_hkDataObject_MemberHandle const*>& handles) const; // vtbl+0x78 0x00F4B740
    void selfDestruct(); // 0x00F4D040
    virtual void destroy(); // vtbl+0x70 0x00F4D110
    void removeMember(InternedString name, bool destroyStructs); // 0x00F4D1E0
    void renameMember(InternedString oldname, InternedString newname); // 0x00F49650
    virtual int getMemberIterator() const; // vtbl+0x14 0x00F495B0
    virtual unsigned int isValid(int it) const; // vtbl+0x18 0x00F49690
    virtual int getNextMember(int it) const; // vtbl+0x1C 0x00F495C0
    virtual const char* getMemberName(int it) const; // vtbl+0x20 0x00F496A0
    virtual const hkDataObject_Value getMemberValue(int it) const; // vtbl+0x24 0x00F496B0
    void clear(int oindex, unsigned int destroyStructs); // 0x00F4B780
    InternedString intern(const char* name) const; // 0x00F4B7C0
    MemberAccess _readAccess(const _hkDataObject_MemberHandle* handle, hkTypeManager::SubType subType); // 0x00F50410
    MemberAccess _writeAccess(const _hkDataObject_MemberHandle* handle, hkTypeManager::SubType subType); // 0x00F4D2D0
    unsigned int _hasMember(InternedString name) const; // 0x00F4B820
    virtual unsigned int hasMember(const char* name) const; // vtbl+0x10 0x00F4B8A0
    virtual unsigned int isSet(const _hkDataObject_MemberHandle* handle); // vtbl+0x68 0x00F496D0
    virtual hkDataObject_Value accessByName(const char* name); // vtbl+0x0C 0x00F4B900
    virtual hkDataArrayImpl* asArray(const _hkDataObject_MemberHandle* handle); // vtbl+0x28 0x00F50630
    virtual const char* asString(const _hkDataObject_MemberHandle* handle); // vtbl+0x2C 0x00F50660
    virtual int64_t asInt(const _hkDataObject_MemberHandle* handle); // vtbl+0x30 0x00F50690
    virtual hkDataObjectImpl* asObject(const _hkDataObject_MemberHandle* handle); // vtbl+0x34 0x00F506E0
    virtual const float* asVec(const _hkDataObject_MemberHandle* handle, int nreal); // vtbl+0x38 0x00F50710
    virtual float asReal(const _hkDataObject_MemberHandle* handle); // vtbl+0x3C 0x00F50750
    virtual void assign(const _hkDataObject_MemberHandle* handle, const hkDataObject_Value& value); // vtbl+0x60 0x00F50780
    virtual void assign(const _hkDataObject_MemberHandle* handle, hkDataObjectImpl* value); // vtbl+0x5C 0x00F4D380
    virtual void assign(const _hkDataObject_MemberHandle* handle, hkDataArrayImpl* value); // vtbl+0x58 0x00F4D3C0
    virtual void assign(const _hkDataObject_MemberHandle* handle, const char* value); // vtbl+0x54 0x00F4D450
    virtual void assign(const _hkDataObject_MemberHandle* handle, float r); // vtbl+0x50 0x00F4D4B0
    virtual void assign(const _hkDataObject_MemberHandle* handle, hkHalf r); // vtbl+0x4C 0x00F4D4F0
    virtual void assign(const _hkDataObject_MemberHandle* handle, const float* r, int nreal); // vtbl+0x48 0x00F50830
    virtual void assign(const _hkDataObject_MemberHandle* handle, int valueIn); // vtbl+0x44 0x00F4D5A0
    virtual void assign(const _hkDataObject_MemberHandle* handle, int64_t valueIn); // vtbl+0x40 0x00F4D540
    virtual void getMemberInfo(const _hkDataObject_MemberHandle* handle, hkDataClass_MemberInfo& infoOut); // vtbl+0x64 0x00F495D0
    virtual void destroyMember(const char* name); // vtbl+0x74 0x00F4D600

    hkRefPtr<const hkDataClassDict> m_class; // 0x0C
    hkArrayBase<MemberData> m_memberData; // 0x10: the members set on this object
    unsigned int m_createdDuringPatching; // 0x1C
};

hkDataArrayImpl* hkDataArrayDict_create(hkDataWorldDict* world, hkTypeManager::Type* t, int size); // 0x00F46F90
float* hkRealArrayImplementation_getReals(hkDataArrayImpl* arrayIn, int numReals); // 0x00F46D70

static const hkDataClassDict::MemberInfo* memberInfoOf(const _hkDataObject_MemberHandle* handle)
{
    return reinterpret_cast<const hkDataClassDict::MemberInfo*>(handle);
}

static const _hkDataObject_MemberHandle* handleOf(const hkDataClassDict::MemberInfo* info)
{
    return reinterpret_cast<const _hkDataObject_MemberHandle*>(info);
}

// 0x00F49360 (:251)
hkDataClassDict* hkDataWorldDict::ObjectTracker::findTrackedClass(const char* name) const
{
    return name ? m_classes.getWithDefault(name, nullptr) : nullptr;
}

// ---- hkDataClassDict

// 0x00F4B5B0 (:360)
InternedString hkDataClassDict::intern(const char* in) const
{
    return m_world->m_tracker->intern(in);
}

// 0x00F4EA40 (:365)
InternedStringHandle hkDataClassDict::internClassNameHandle(const char* in) const
{
    return m_world->m_tracker->internClassNameHandle(in);
}

// 0x00F4EA60 (:375)
hkDataClassDict::hkDataClassDict(hkDataWorldDict* root, const char* name, int version)
    : m_world(root), m_name(root->m_tracker->internClassNameHandle(name)), m_version(version), m_parent(nullptr)
{
}

// 0x00F4EDD0 (:437)
// The hkRefPtr members are released here (no hkRefPtr destructor in this port).
hkDataClassDict::~hkDataClassDict()
{
    selfDestruct();
    m_parent = nullptr;
}

// 0x00F4EAC0 (:442)
hkDataWorldDict* hkDataClassDict::getWorld() const { return m_world; }
// 0x00F4EB10 (:447)
const char* hkDataClassDict::getName() const { return m_name.getString(); }
// 0x00F4EAD0 (:452)
int hkDataClassDict::getVersion() const { return m_version; }
// 0x00F4EB60 (:457)
hkDataClassImpl* hkDataClassDict::getParent() const { return m_parent; }

// 0x00F4EB70 (:462)
// By interned name along k's parent chain.
hkBool hkDataClassDict::isSuperClass(const hkDataClassImpl* k) const
{
    for (; k; k = k->getParent())
    {
        if (intern(k->getName()).getString() == m_name.getString())
        {
            return true;
        }
    }
    return false;
}

// 0x00F4EB20 (:478)
int hkDataClassDict::getNumDeclaredMembers() const { return m_memberInfo.getSize(); }

// 0x00F4EC00 (:482)
int hkDataClassDict::getDeclaredMemberIndexByName(const char* nameIn) const
{
    const InternedString name = intern(nameIn);
    for (int i = 0; i < m_memberInfo.getSize(); i++)
    {
        if (m_memberInfo[i].m_name == name)
        {
            return i;
        }
    }
    return -1;
}

// 0x00F4EB30 (:494)
void hkDataClassDict::getDeclaredMemberInfo(int i, hkDataClass_MemberInfo& info) const
{
    m_memberInfo[i].getMemberInfo(this, info);
}

// 0x00F4EC70 (:504)
int hkDataClassDict::getNumMembers() const
{
    int num = m_memberInfo.getSize();
    for (const hkDataClassDict* c = m_parent; c; c = c->m_parent)
    {
        num += c->m_memberInfo.getSize();
    }
    return num;
}

// 0x00F4EC90 (:516)
// The parent's members come first in the member index.
int hkDataClassDict::getMemberIndexByName(const char* nameIn) const
{
    const InternedString name = intern(nameIn);
    for (const hkDataClassDict* c = this; c; c = c->m_parent)
    {
        for (int i = 0; i < c->m_memberInfo.getSize(); i++)
        {
            if (c->m_memberInfo[i].m_name == name)
            {
                return c->getNumMembers() - c->getNumDeclaredMembers() + i;
            }
        }
    }
    return -1;
}

// 0x00F4ED20 (:534)
// Original: the owner reported is this class, also for an inherited member.
void hkDataClassDict::getMemberInfo(int memberIndex, hkDataClass_MemberInfo& info) const
{
    int index = memberIndex - getNumMembers();
    for (const hkDataClassDict* c = this; c; c = c->m_parent)
    {
        index += c->m_memberInfo.getSize();
        if (index >= 0)
        {
            c->m_memberInfo[index].getMemberInfo(this, info);
            return;
        }
    }
}

// 0x00F4ED70 (:554)
// Fills infos from its end backwards (it holds getNumMembers() entries); the owner is this class throughout.
void hkDataClassDict::getAllMemberInfo(hkArrayBase<hkDataClass_MemberInfo>& infos) const
{
    hkDataClass_MemberInfo* cur = infos.begin() + infos.getSize();
    for (const hkDataClassDict* c = this; c; c = c->m_parent)
    {
        const int n = c->m_memberInfo.getSize();
        cur -= n;
        for (int i = 0; i < n; i++)
        {
            c->m_memberInfo[i].getMemberInfo(this, cur[i]);
        }
    }
}

// 0x00F4B610 (:580)
void hkDataClassDict::_getAllMemberHandles(hkArrayBase<_hkDataObject_MemberHandle const*>& handles) const
{
    _hkDataObject_MemberHandle const** cur = handles.begin() + handles.getSize();
    for (const hkDataClassDict* c = this; c; c = c->m_parent)
    {
        const int n = c->m_memberInfo.getSize();
        cur -= n;
        for (int i = 0; i < n; i++)
        {
            cur[i] = handleOf(&c->m_memberInfo[i]);
        }
    }
}

// 0x00F493B0 (:611)
// The last declared member of the name.
int hkDataClassDict::_findDeclaredMemberByName(InternedString name, hkArrayBase<MemberInfo>& info)
{
    for (int i = info.getSize() - 1; i >= 0; i--)
    {
        if (info[i].m_name == name)
        {
            return i;
        }
    }
    return -1;
}

// 0x00F4CE00 (:624)
void hkDataClassDict::addMember(InternedString name, hkTypeManager::Type* type, const void* valuePtr)
{
    MemberInfo& mi = m_memberInfo._expandOne(getAllocator());
    mi.m_default.i = 0;
    mi.m_name = name;
    mi.m_type = type;
    mi.m_valuePtr = valuePtr;
}

// 0x00F4CE70 (:635)
// Original: a name the class does not declare removes the entry at index -1 (the shift starts before the array).
// The result is HK_FAILURE when a member of the name is still found (in the class or a parent).
hkResult hkDataClassDict::removeMember(InternedString name)
{
    const int index = _findDeclaredMemberByName(name, m_memberInfo);
    m_memberInfo.m_size--;
    hkString::memMove(m_memberInfo.begin() + index, m_memberInfo.begin() + index + 1, (m_memberInfo.getSize() - index) * (int)sizeof(MemberInfo));
    return getMemberInfoByName(name) ? HK_FAILURE : HK_SUCCESS;
}

// 0x00F493E0 (:659)
// Original: an unknown name writes before the array (index -1), as in setMemberDefault.
void hkDataClassDict::renameMember(InternedString oldName, InternedString newName)
{
    m_memberInfo[_findDeclaredMemberByName(oldName, m_memberInfo)].m_name = newName;
}

// 0x00F49420 (:666)
// Original bug kept: an unknown name writes before the array (index -1).
void hkDataClassDict::setMemberDefault(InternedString name, const void* valuePtr)
{
    m_memberInfo[_findDeclaredMemberByName(name, m_memberInfo)].m_valuePtr = valuePtr;
}

// 0x00F4B650 (:673)
// Original: an index out of range gives the entry before this class's members.
const hkDataClassDict::MemberInfo& hkDataClassDict::getMemberInfoByIndex(int memberIndex) const
{
    int index = memberIndex - getNumMembers();
    for (const hkDataClassDict* c = this; c; c = c->m_parent)
    {
        index += c->m_memberInfo.getSize();
        if (index >= 0)
        {
            return c->m_memberInfo[index];
        }
    }
    return m_memberInfo.begin()[-1];
}

// 0x00F4B690 (:692)
const hkDataClassDict::MemberInfo* hkDataClassDict::getMemberInfoByName(InternedString name) const
{
    for (const hkDataClassDict* c = this; c; c = c->m_parent)
    {
        for (int i = 0; i < c->m_memberInfo.getSize(); i++)
        {
            if (c->m_memberInfo[i].m_name == name)
            {
                return &c->m_memberInfo[i];
            }
        }
    }
    return nullptr;
}

// 0x00F49460 (:709)
void hkDataClassDict::setParent(hkDataClassDict* newParent)
{
    m_parent = newParent;
}

// 0x00F4CEF0 (:714)
// Frees the members and enums and drops the name (the null name takes its place). The hidden members are not freed.
void hkDataClassDict::selfDestruct()
{
    hkMemoryAllocator& alloc = getAllocator();
    m_memberInfo._clearAndDeallocate(alloc);
    for (int i = 0; i < m_enums.getSize(); i++)
    {
        Enum* e = m_enums[i];
        e->m_items._clearAndDeallocate(alloc);
        delete e;
    }
    m_enums._clearAndDeallocate(alloc);
    m_name.clear();
}

// ---- hkDataObjectDict

// 0x00F49500 (:770)
hkDataObjectDict::hkDataObjectDict(const hkDataClassDict* k, bool createdDuringPatchingFlag)
    : m_class(k), m_createdDuringPatching(createdDuringPatchingFlag)
{
}

// 0x00F4EEA0 (:774)
hkDataObjectDict::~hkDataObjectDict()
{
    selfDestruct();
    m_class = nullptr;
}

// 0x00F49590 (:779)
unsigned int hkDataObjectDict::createdDuringPatching() const { return m_createdDuringPatching; }

// 0x00F495A0 (:784)
hkDataObject_Handle hkDataObjectDict::getHandle() const
{
    hkDataObject_Handle h;
    h.p0 = const_cast<hkDataObjectDict*>(this);
    h.p1 = nullptr;
    return h;
}

// 0x00F4B730 (:792)
const hkDataClassImpl* hkDataObjectDict::getClass() const { return m_class; }

// 0x00F4B740 (:797)
void hkDataObjectDict::getAllMemberHandles(hkArrayBase<_hkDataObject_MemberHandle const*>& handles) const
{
    m_class->_getAllMemberHandles(handles);
}

// 0x00F4D040 (:802)
// Releases the member values (structs are not destroyed) and frees the storage.
void hkDataObjectDict::selfDestruct()
{
    for (int i = 0; i < m_memberData.getSize(); i++)
    {
        if (const hkDataClassDict::MemberInfo* mi = m_class->getMemberInfoByName(m_memberData[i].m_memberName))
        {
            Union::clearArray(mi->m_type, &m_memberData[i].m_value, 1, 0);
        }
    }
    m_memberData._clearAndDeallocate(*m_class->m_world->m_allocator);
}

// 0x00F4D110 (:811)
// As selfDestruct, but structs are destroyed too.
void hkDataObjectDict::destroy()
{
    for (int i = 0; i < m_memberData.getSize(); i++)
    {
        if (const hkDataClassDict::MemberInfo* mi = m_class->getMemberInfoByName(m_memberData[i].m_memberName))
        {
            Union::clearArray(mi->m_type, &m_memberData[i].m_value, 1, 1);
        }
    }
    m_memberData._clearAndDeallocate(*m_class->m_world->m_allocator);
}

// 0x00F4D1E0 (:820)
// The last entry moves into the hole. A name that is not set stops at the end of the array and returns (0x00F4D20A).
void hkDataObjectDict::removeMember(InternedString name, bool destroyStructs)
{
    if (m_memberData.getSize() <= 0)
    {
        return;
    }
    int index = 0;
    while (m_memberData[index].m_memberName != name)
    {
        index++;
        if (index >= m_memberData.getSize())
        {
            return;
        }
    }
    if (const hkDataClassDict::MemberInfo* mi = m_class->getMemberInfoByName(m_memberData[index].m_memberName))
    {
        Union::clearArray(mi->m_type, &m_memberData[index].m_value, 1, destroyStructs);
    }
    m_memberData.removeAt(index);
}

// 0x00F49650 (:833)
// A name that is not set stops at the end of the array and returns.
void hkDataObjectDict::renameMember(InternedString oldname, InternedString newname)
{
    if (m_memberData.getSize() <= 0)
    {
        return;
    }
    int index = 0;
    while (m_memberData[index].m_memberName != oldname)
    {
        index++;
        if (index >= m_memberData.getSize())
        {
            return;
        }
    }
    m_memberData[index].m_memberName = newname;
}

// 0x00F495B0 (:845)
int hkDataObjectDict::getMemberIterator() const { return 0; }
// 0x00F49690 (:849)
unsigned int hkDataObjectDict::isValid(int it) const { return it < m_memberData.getSize(); }
// 0x00F495C0 (:853)
int hkDataObjectDict::getNextMember(int it) const { return it + 1; }
// 0x00F496A0 (:858)
const char* hkDataObjectDict::getMemberName(int it) const { return m_memberData[it].m_memberName.getString(); }

// 0x00F496B0 (:862)
// Original: the handle is the member's name, not its MemberInfo.
const hkDataObject_Value hkDataObjectDict::getMemberValue(int it) const
{
    return hkDataObject_Value(const_cast<hkDataObjectDict*>(this), reinterpret_cast<const _hkDataObject_MemberHandle*>(m_memberData[it].m_memberName.getString()));
}

// 0x00F4B780 (:867)
void hkDataObjectDict::clear(int oindex, unsigned int destroyStructs)
{
    MemberData& md = m_memberData[oindex];
    if (const hkDataClassDict::MemberInfo* mi = m_class->getMemberInfoByName(md.m_memberName))
    {
        Union::clearArray(mi->m_type, &md.m_value, 1, destroyStructs);
    }
}

// 0x00F4B7C0 (:876)
InternedString hkDataObjectDict::intern(const char* name) const
{
    return m_class->m_world->m_tracker->intern(name);
}

// 0x00F50410 (:881)
// The stored value of the member, made from the class default when the object does not hold it yet: an empty array
// (a tuple sized, real tuples filled from the default), a new struct object (createdDuringPatching), or the default
// value (strings copied). The last entry of the name wins. The subType is not used.
hkDataObjectDict::MemberAccess hkDataObjectDict::_readAccess(const _hkDataObject_MemberHandle* handle, hkTypeManager::SubType subType)
{
    const hkDataClassDict::MemberInfo* mi = memberInfoOf(handle);
    int found = -1;
    for (int i = 0; i < m_memberData.getSize(); i++)
    {
        if (m_memberData[i].m_memberName == mi->m_name)
        {
            found = i;
        }
    }
    if (found != -1)
    {
        return MemberAccess(mi, found);
    }

    hkDataWorldDict* world = m_class->m_world;
    const int index = m_memberData.getSize();
    MemberData& md = m_memberData._expandOne(*world->m_allocator);
    md.m_memberName = InternedString();
    md.m_memberName = mi->m_name;
    hkTypeManager::Type* type = mi->m_type;
    switch (type->getSubType())
    {
        case hkTypeManager::SUB_TYPE_ARRAY:
            md.m_value.a = hkDataArrayDict_create(world, type->getParent(), 0);
            md.m_value.a->addReference();
            break;
        case hkTypeManager::SUB_TYPE_TUPLE:
        {
            hkDataArrayImpl* arr = hkDataArrayDict_create(world, type->getParent(), 0);
            md.m_value.a = arr;
            arr->setSize(type->getTupleSize());
            md.m_value.a->addReference();
            if (mi->m_valuePtr && type->getParent()->isReal())
            {
                const float* defaults = static_cast<const float*>(mi->m_valuePtr);
                const int n = type->getTupleSize();
                for (int i = 0; i < n; i++)
                {
                    md.m_value.a->setReal(i, defaults[i]);
                }
            }
            break;
        }
        case hkTypeManager::SUB_TYPE_POINTER:
            // Original: only the low half of the value is cleared
            md.m_value.p = nullptr;
            break;
        case hkTypeManager::SUB_TYPE_CLASS:
        {
            hkDataClassImpl* cls = world->findClass(type->getTypeName());
            hkDataObject obj(world->newObject(hkDataClass(cls), true));
            md.m_value.o = obj.getImplementation();
            md.m_value.o->addReference();
            break;
        }
        default:
            if (mi->m_valuePtr == nullptr)
            {
                md.m_value.i = 0;
            }
            else if (type->isByte() || type->isInt())
            {
                md.m_value.i = *static_cast<const int64_t*>(mi->m_valuePtr);
            }
            else if (type->isReal())
            {
                md.m_value.r = *static_cast<const float*>(mi->m_valuePtr);
            }
            else if (type->isCstring())
            {
                // the default of a string member is the string itself
                md.m_value.s = hkString::strDup(static_cast<const char*>(mi->m_valuePtr));
            }
            break;
    }
    return MemberAccess(mi, index);
}

// 0x00F4D2D0 (:976)
// A held value is released first; a new entry starts zeroed. The subType is not used.
hkDataObjectDict::MemberAccess hkDataObjectDict::_writeAccess(const _hkDataObject_MemberHandle* handle, hkTypeManager::SubType subType)
{
    const hkDataClassDict::MemberInfo* mi = memberInfoOf(handle);
    for (int i = 0; i < m_memberData.getSize(); i++)
    {
        if (m_memberData[i].m_memberName == mi->m_name)
        {
            Union::clearArray(mi->m_type, &m_memberData[i].m_value, 1, 0);
            return MemberAccess(mi, i);
        }
    }
    const int index = m_memberData.getSize();
    MemberData& md = m_memberData._expandOne(*m_class->m_world->m_allocator);
    md.m_memberName = mi->m_name;
    md.m_value.i = 0;
    return MemberAccess(mi, index);
}

// 0x00F4B820 (:998)
// Set on the object, or declared with a default.
unsigned int hkDataObjectDict::_hasMember(InternedString name) const
{
    for (int i = 0; i < m_memberData.getSize(); i++)
    {
        if (m_memberData[i].m_memberName == name)
        {
            return 1;
        }
    }
    const hkDataClassDict::MemberInfo* mi = m_class->getMemberInfoByName(name);
    return mi && mi->m_valuePtr != nullptr;
}

// 0x00F4B8A0 (:1015)
unsigned int hkDataObjectDict::hasMember(const char* name) const { return _hasMember(intern(name)); }

// 0x00F496D0 (:1020)
unsigned int hkDataObjectDict::isSet(const _hkDataObject_MemberHandle* handle)
{
    const hkDataClassDict::MemberInfo* mi = memberInfoOf(handle);
    if (mi->m_valuePtr)
    {
        return 1;
    }
    for (int i = 0; i < m_memberData.getSize(); i++)
    {
        if (m_memberData[i].m_memberName == mi->m_name)
        {
            return 1;
        }
    }
    return 0;
}

// 0x00F4B900 (:1041)
hkDataObject_Value hkDataObjectDict::accessByName(const char* name)
{
    if (const hkDataClassDict::MemberInfo* mi = m_class->getMemberInfoByName(intern(name)))
    {
        return hkDataObject_Value(this, handleOf(mi));
    }
    return hkDataObject_Value(nullptr, nullptr);
}

// 0x00F50630 (:1058)
hkDataArrayImpl* hkDataObjectDict::asArray(const _hkDataObject_MemberHandle* handle) { return m_memberData[_readAccess(handle, hkTypeManager::SUB_TYPE_ARRAY).index].m_value.a; }
// 0x00F50660 (:1065)
const char* hkDataObjectDict::asString(const _hkDataObject_MemberHandle* handle) { return m_memberData[_readAccess(handle, hkTypeManager::SUB_TYPE_CSTRING).index].m_value.s; }

// 0x00F50690 (:1072)
// 0 for a member that is not an integer.
int64_t hkDataObjectDict::asInt(const _hkDataObject_MemberHandle* handle)
{
    const MemberAccess access = _readAccess(handle, hkTypeManager::SUB_TYPE_VOID);
    const hkTypeManager::Type* type = access.info->m_type;
    if (!type->isByte() && !type->isInt())
    {
        return 0;
    }
    return m_memberData[access.index].m_value.i;
}

// 0x00F506E0 (:1084)
hkDataObjectImpl* hkDataObjectDict::asObject(const _hkDataObject_MemberHandle* handle) { return m_memberData[_readAccess(handle, hkTypeManager::SUB_TYPE_CLASS).index].m_value.o; }

// 0x00F50710 (:1090)
// A tuple member holds a real array.
const float* hkDataObjectDict::asVec(const _hkDataObject_MemberHandle* handle, int nreal)
{
    const MemberAccess access = _readAccess(handle, hkTypeManager::SUB_TYPE_TUPLE);
    return hkRealArrayImplementation_getReals(m_memberData[access.index].m_value.a, nreal);
}

// 0x00F50750 (:1099)
float hkDataObjectDict::asReal(const _hkDataObject_MemberHandle* handle) { return m_memberData[_readAccess(handle, hkTypeManager::SUB_TYPE_REAL).index].m_value.r; }

// 0x00F50780 (:1106)
// Copies the raw value from another dictionary object; arrays, pointers and structs of a matching type gain a
// reference. Original: the source must be an hkDataObjectDict, and a string is shared, not copied (both objects free
// it).
void hkDataObjectDict::assign(const _hkDataObject_MemberHandle* handle, const hkDataObject_Value& value)
{
    hkDataObjectDict* src = static_cast<hkDataObjectDict*>(value.m_impl);
    const MemberAccess srcAccess = src->_readAccess(value.m_handle, hkTypeManager::SUB_TYPE_VOID);
    const MemberAccess dstAccess = _writeAccess(handle, hkTypeManager::SUB_TYPE_VOID);
    hkDataObjectImpl* srcObject = src->m_memberData[srcAccess.index].m_value.o;
    hkTypeManager::Type* srcType = srcAccess.info->m_type;
    hkTypeManager::Type* dstType = dstAccess.info->m_type;
    bool reference;
    if (srcType->isArray() || srcType->isTuple())
    {
        reference = dstType->isArray() || dstType->isTuple();
    }
    else if (srcType->isPointer() && dstType->isPointer())
    {
        reference = true;
    }
    else
    {
        reference = srcType->isClass() && srcType == dstType;
    }
    if (reference && srcObject)
    {
        srcObject->addReference();
    }
    m_memberData[dstAccess.index].m_value = src->m_memberData[srcAccess.index].m_value;
}

// 0x00F4D380 (:1150)
void hkDataObjectDict::assign(const _hkDataObject_MemberHandle* handle, hkDataObjectImpl* value)
{
    if (value)
    {
        value->addReference();
    }
    m_memberData[_writeAccess(handle, hkTypeManager::SUB_TYPE_CLASS).index].m_value.o = value;
}

// 0x00F4D3C0 (:1160)
// Kept when the element types match (a homogeneous class member takes any class). Original: the reference taken is
// kept (leaked) when the array does not match, and a null array is dereferenced.
void hkDataObjectDict::assign(const _hkDataObject_MemberHandle* handle, hkDataArrayImpl* value)
{
    value->addReference();
    const MemberAccess access = _writeAccess(handle, hkTypeManager::SUB_TYPE_VOID);
    hkTypeManager::Type* type = value->getType();
    hkTypeManager::Type* elementType = access.info->m_type->getParent();
    if (elementType->isEqual(type))
    {
        m_memberData[access.index].m_value.a = value;
    }
    if (elementType->isClass() && elementType->getTypeName() == nullptr && type->isClass())
    {
        m_memberData[access.index].m_value.a = value;
    }
}

// 0x00F4D450 (:1186)
void hkDataObjectDict::assign(const _hkDataObject_MemberHandle* handle, const char* value)
{
    const MemberAccess access = _writeAccess(handle, hkTypeManager::SUB_TYPE_CSTRING);
    m_memberData[access.index].m_value.s = value ? hkString::strDup(value) : nullptr;
}

// 0x00F4D4B0 (:1192)
void hkDataObjectDict::assign(const _hkDataObject_MemberHandle* handle, float r) { m_memberData[_writeAccess(handle, hkTypeManager::SUB_TYPE_REAL).index].m_value.r = r; }
// 0x00F4D4F0 (:1198)
void hkDataObjectDict::assign(const _hkDataObject_MemberHandle* handle, hkHalf r) { m_memberData[_writeAccess(handle, hkTypeManager::SUB_TYPE_REAL).index].m_value.r = r.getReal(); }

// 0x00F50830 (:1204)
// Only a tuple of reals takes a vec: its real array is made on first use.
void hkDataObjectDict::assign(const _hkDataObject_MemberHandle* handle, const float* r, int nreal)
{
    const MemberAccess access = _writeAccess(handle, hkTypeManager::SUB_TYPE_VOID);
    hkTypeManager::Type* type = access.info->m_type;
    if (type->isTuple() && type->getParent()->isReal())
    {
        Union& value = m_memberData[access.index].m_value;
        if (value.p == nullptr)
        {
            value.a = hkDataArrayDict_create(m_class->m_world, type->getParent(), 0);
            value.a->setSize(type->getTupleSize());
            value.o->addReference();
        }
        hkString::memCpy(hkRealArrayImplementation_getReals(value.a, nreal), r, nreal * (int)sizeof(float));
    }
}

// 0x00F4D540 (:1230)
// Integers stay integers, reals are converted; other types ignore the value.
void hkDataObjectDict::assign(const _hkDataObject_MemberHandle* handle, int64_t valueIn)
{
    const MemberAccess access = _writeAccess(handle, hkTypeManager::SUB_TYPE_VOID);
    const hkTypeManager::Type* type = access.info->m_type;
    if (type->isByte() || type->isInt())
    {
        m_memberData[access.index].m_value.i = valueIn;
    }
    else if (type->isReal())
    {
        m_memberData[access.index].m_value.r = (float)valueIn;
    }
}

// 0x00F4D5A0 (:1249)
void hkDataObjectDict::assign(const _hkDataObject_MemberHandle* handle, int valueIn)
{
    const MemberAccess access = _writeAccess(handle, hkTypeManager::SUB_TYPE_VOID);
    const hkTypeManager::Type* type = access.info->m_type;
    if (type->isByte() || type->isInt())
    {
        m_memberData[access.index].m_value.i = valueIn;
    }
    else if (type->isReal())
    {
        m_memberData[access.index].m_value.r = (float)valueIn;
    }
}

// 0x00F495D0 (:1267)
// No owner class is reported.
void hkDataObjectDict::getMemberInfo(const _hkDataObject_MemberHandle* handle, hkDataClass_MemberInfo& infoOut)
{
    memberInfoOf(handle)->getMemberInfo(nullptr, infoOut);
}

// 0x00F4D600 (:1273)
void hkDataObjectDict::destroyMember(const char* name)
{
    removeMember(intern(name), true);
}

// ---- arrays

// size 0x20
// A plain array of T in the world's allocator; new elements are zero.
template<typename T>
struct BasicArrayImplementation : public hkDataArrayImpl
{
    // <uchar> 0x00F4A500 (:1286)
    // <float> 0x00F4A580 (:1286)
    // <hkDataObjectImpl*> 0x00F4A600 (:1286)
    // <char*> 0x00F4A680 (:1286)
    // <hkDataArrayImpl*> 0x00F4A740 (:1286)
    BasicArrayImplementation(hkDataWorldDict* world, hkTypeManager::Type* type) : m_world(world), m_type(type) {}

    // <uchar> 0x00F4C040 (:1287)
    // <float> 0x00F4C140 (:1287)
    // <hkDataObjectImpl*> 0x00F4C240 (:1287)
    // <char*> 0x00F4C340 (:1287)
    // <hkDataArrayImpl*> 0x00F4C440 (:1287)
    virtual ~BasicArrayImplementation() { m_data._clearAndDeallocate(getAllocator()); }

    // <uchar> 0x00F488B0 (:1290)
    // <float> 0x00F488C0 (:1290)
    // <hkDataObjectImpl*> 0x00F488E0 (:1290)
    // <char*> 0x00F488F0 (:1290)
    // <hkDataArrayImpl*> 0x00F48900 (:1290)
    hkMemoryAllocator& getAllocator() const { return *m_world->m_allocator; }
    // <uchar> 0x00F4A540 (:1294)
    // <float> 0x00F4A5C0 (:1294)
    // <hkDataObjectImpl*> 0x00F4A640 (:1294)
    // <char*> 0x00F4A6C0 (:1294)
    // <hkDataArrayImpl*> 0x00F4A780 (:1294)
    virtual hkTypeManager::Type* getType() const { return m_type; }
    // <uchar> 0x00F4A550 (:1298)
    // <float> 0x00F4A5D0 (:1298)
    // <hkDataObjectImpl*> 0x00F4A650 (:1298)
    // <char*> 0x00F4A6D0 (:1298)
    // <hkDataArrayImpl*> 0x00F4A790 (:1298)
    virtual hkDataWorld* getWorld() const { return m_world; }
    // <uchar> 0x00F4A560 (:1302)
    // <float> 0x00F4A5E0 (:1302)
    // <hkDataObjectImpl*> 0x00F4A660 (:1302)
    // <char*> 0x00F4A6E0 (:1302)
    // <hkDataArrayImpl*> 0x00F4A7A0 (:1302)
    virtual const hkDataClassImpl* getClass() const { return nullptr; }
    // <uchar> 0x00F4C090 (:1306)
    // <float> 0x00F4C190 (:1306)
    // <hkDataObjectImpl*> 0x00F4C290 (:1306)
    // <char*> 0x00F4C390 (:1306)
    // <hkDataArrayImpl*> 0x00F4C490 (:1306)
    virtual void clear() { m_data.clear(); }
    // <uchar> 0x00F4C0A0 (:1310)
    // <float> 0x00F4C1A0 (:1310)
    // <hkDataObjectImpl*> 0x00F4C2A0 (:1310)
    // <char*> 0x00F4C3A0 (:1310)
    // <hkDataArrayImpl*> 0x00F4C4A0 (:1310)
    virtual void reserve(int n) { m_data._reserve(getAllocator(), n); }
    // <uchar> 0x00F4C0E0 (:1314)
    // <float> 0x00F4C1E0 (:1314)
    // <hkDataObjectImpl*> 0x00F4C2E0 (:1314)
    // <char*> 0x00F4C3E0 (:1314)
    // <hkDataArrayImpl*> 0x00F4C4E0 (:1314)
    virtual void setSize(int n) { m_data._setSize(getAllocator(), n, T(0)); }
    // <uchar> 0x00F4A570 (:1318)
    // <float> 0x00F4A5F0 (:1318)
    // <hkDataObjectImpl*> 0x00F4A670 (:1318)
    // <char*> 0x00F4A730 (:1318)
    // <hkDataArrayImpl*> 0x00F4A7E0 (:1318)
    virtual int getSize() const { return m_data.getSize(); }

    hkArrayBase<T> m_data; // 0x0C
    hkDataWorldDict* m_world; // 0x18
    hkTypeManager::Type* m_type; // 0x1C
};

// size 0x30
// An int array stored as 32-bit until a 64-bit value is set (then it moves to m_int64Array for good, until a setAll
// of 32-bit or smaller values).
struct VariableIntArrayImplementation : public hkDataArrayImpl
{
    enum
    {
        ARRAY_TYPE_INT32 = 0,
        ARRAY_TYPE_INT64 = 1,
    };

    // 0x00F49760 (:1334)
    VariableIntArrayImplementation(hkDataWorldDict* world) : m_world(world), m_arrayType(ARRAY_TYPE_INT32), m_assigned(false) {}

    // 0x00F4D670 (:1336)
    virtual ~VariableIntArrayImplementation()
    {
        m_int32Array._clearAndDeallocate(_getAllocator());
        m_int64Array._clearAndDeallocate(_getAllocator());
    }

    // 0x00F497A0 (:1343)
    virtual hkTypeManager::Type* getType() const { return m_world->m_typeManager.getIntType(); }
    // 0x00F497B0 (:1348)
    virtual int getUnderlyingIntegerSize() const { return m_arrayType != ARRAY_TYPE_INT32 ? 8 : 4; }

    // 0x00F4D6F0 (:1360)
    virtual void clear()
    {
        m_int32Array.clear();
        m_int64Array._clearAndDeallocate(_getAllocator());
        m_arrayType = ARRAY_TYPE_INT32;
        m_assigned = false;
    }

    // 0x00F497C0 (:1369)
    virtual hkDataWorld* getWorld() const { return m_world; }

    // 0x00F4B980 (:1374)
    virtual void reserve(int n)
    {
        if (m_arrayType == ARRAY_TYPE_INT32)
        {
            m_int32Array._reserve(_getAllocator(), n);
        }
        else if (m_arrayType == ARRAY_TYPE_INT64)
        {
            m_int64Array._reserve(_getAllocator(), n);
        }
    }

    // 0x00F49900 (:1389)
    virtual hkResult asStridedBasicArray(hkStridedBasicArray& arrOut)
    {
        if (m_arrayType == ARRAY_TYPE_INT32)
        {
            arrOut.m_type = hkClassMember::TYPE_INT32;
            arrOut.m_size = m_int32Array.getSize();
            arrOut.m_data = m_int32Array.begin();
            arrOut.m_stride = 4;
        }
        else
        {
            arrOut.m_type = hkClassMember::TYPE_INT64;
            arrOut.m_size = m_int64Array.getSize();
            arrOut.m_data = m_int64Array.begin();
            arrOut.m_stride = 8;
        }
        arrOut.m_tupleSize = 1;
        return HK_SUCCESS;
    }

    // 0x00F4BA00 (:1409)
    // A size of 0 also forgets that values were assigned.
    virtual void setSize(int size)
    {
        if (m_arrayType == ARRAY_TYPE_INT32)
        {
            m_int32Array._setSize(_getAllocator(), size, 0);
        }
        else if (m_arrayType == ARRAY_TYPE_INT64)
        {
            m_int64Array._setSize(_getAllocator(), size, 0);
        }
        if (size == 0)
        {
            m_assigned = false;
        }
    }

    // 0x00F49950 (:1429)
    virtual int getSize() const
    {
        if (m_arrayType == ARRAY_TYPE_INT32)
        {
            return m_int32Array.getSize();
        }
        if (m_arrayType != ARRAY_TYPE_INT64)
        {
            return 0;
        }
        return m_int64Array.getSize();
    }

    // 0x00F49970 (:1443)
    virtual int asInt(int index) const
    {
        if (m_arrayType == ARRAY_TYPE_INT32)
        {
            return m_int32Array[index];
        }
        if (m_arrayType != ARRAY_TYPE_INT64)
        {
            return 0;
        }
        return (int)m_int64Array[index];
    }

    // 0x00F499A0 (:1459)
    virtual int64_t asInt64(int index) const
    {
        if (m_arrayType == ARRAY_TYPE_INT32)
        {
            return m_int32Array[index];
        }
        if (m_arrayType != ARRAY_TYPE_INT64)
        {
            return 0;
        }
        return m_int64Array[index];
    }

    // 0x00F499E0 (:1474)
    // The half's storage, in the 32-bit array whatever the array type.
    virtual void setHalf(int index, hkHalf val)
    {
        short storage;
        hkString::memCpy(&storage, &val, sizeof(storage));
        m_int32Array[index] = (unsigned short)storage;
    }

    // 0x00F49A00 (:1484)
    virtual hkHalf asHalf(int index) const
    {
        hkHalf h;
        const short storage = (short)m_int32Array[index];
        hkString::memCpy(&h, &storage, sizeof(storage));
        return h;
    }

    // 0x00F49A20 (:1494)
    virtual void setInt(int index, int val)
    {
        if (m_arrayType == ARRAY_TYPE_INT32)
        {
            m_int32Array[index] = val;
            m_assigned = true;
            return;
        }
        setInt64(index, val);
        m_assigned = true;
    }

    // 0x00F4BAE0 (:1507)
    // Moves the values to the 64-bit array.
    void _reallocateArray()
    {
        const int size = m_int32Array.getSize();
        m_int64Array._reserve(_getAllocator(), size);
        m_int64Array.setSizeUnchecked(size);
        for (int i = 0; i < m_int32Array.getSize(); i++)
        {
            m_int64Array[i] = m_int32Array[i];
        }
        m_int32Array._clearAndDeallocate(_getAllocator());
        m_arrayType = ARRAY_TYPE_INT64;
    }

    // 0x00F4BB80 (:1524)
    virtual void setInt64(int index, int64_t val)
    {
        if (m_arrayType != ARRAY_TYPE_INT64)
        {
            _reallocateArray();
        }
        m_assigned = true;
        m_int64Array[index] = val;
    }

    // 0x00F497D0 (:1536)
    virtual void setAll(const int64_t* in, int size)
    {
        setSize(size);
        for (int i = 0; i < size; i++)
        {
            setInt64(i, in[i]);
        }
    }

    // 0x00F49810 (:1545)
    virtual void setAll(const uint64_t* in, int size) { setAll(reinterpret_cast<const int64_t*>(in), size); }

    // 0x00F4D740 (:1550)
    virtual void setAll(const int* in, int size)
    {
        _resetToInt32();
        m_int32Array._setSize(_getAllocator(), size, 0);
        hkString::memCpy(m_int32Array.begin(), in, size * (int)sizeof(int));
    }

    // 0x00F49820 (:1561)
    virtual void setAll(const unsigned int* in, int size) { setAll(reinterpret_cast<const int*>(in), size); }

    // 0x00F4D800 (:1566)
    virtual void setAll(const short* in, int size)
    {
        _resetToInt32();
        m_int32Array._setSize(_getAllocator(), size, 0);
        for (int i = 0; i < size; i++)
        {
            setInt(i, in[i]);
        }
    }

    // 0x00F4D8C0 (:1580)
    virtual void setAll(const unsigned short* in, int size)
    {
        _resetToInt32();
        m_int32Array._setSize(_getAllocator(), size, 0);
        for (int i = 0; i < size; i++)
        {
            setInt(i, in[i]);
        }
    }

    // 0x00F49830 (:1598)
    virtual const hkDataClassImpl* getClass() const { return nullptr; }
    // 0x00F49840 (:1603)
    virtual int getIntType() { return m_arrayType; }

protected:
    // 0x00F49850 (:1610)
    hkMemoryAllocator& _getAllocator() const { return *m_world->m_allocator; }

    // inline in the setAll functions: back to 32-bit storage, the 64-bit values dropped
    void _resetToInt32()
    {
        if (m_arrayType != ARRAY_TYPE_INT32)
        {
            m_arrayType = ARRAY_TYPE_INT32;
            m_int64Array._clearAndDeallocate(_getAllocator());
        }
    }

public:
    hkDataWorldDict* m_world; // 0x0C
    int m_arrayType; // 0x10
    hkArrayBase<int> m_int32Array; // 0x14
    hkArrayBase<int64_t> m_int64Array; // 0x20
    hkBool m_assigned; // 0x2C
};

// size 0x20
struct ByteArrayImplementation : public BasicArrayImplementation<unsigned char>
{
    // 0x00F4D9D0 (:1621)
    ByteArrayImplementation(hkDataWorldDict* world, hkTypeManager::Type* type) : BasicArrayImplementation<unsigned char>(world, type) {}
    // 0x00F4DA10 (:1622)
    virtual int asInt(int i) const { return m_data[i]; }
    // 0x00F4DA20 (:1623)
    virtual int64_t asInt64(int i) const { return m_data[i]; }
    // 0x00F4DA30 (:1624)
    virtual void setInt(int i, int val) { m_data[i] = (unsigned char)val; }
    // 0x00F4DA50 (:1625)
    virtual void setInt64(int i, int64_t val) { m_data[i] = (unsigned char)val; }

    // 0x00F4DA70 (:1628)
    virtual hkResult asStridedBasicArray(hkStridedBasicArray& arrOut)
    {
        arrOut.m_type = hkClassMember::TYPE_UINT8;
        arrOut.m_size = m_data.getSize();
        arrOut.m_data = m_data.begin();
        arrOut.m_tupleSize = 1;
        arrOut.m_stride = 1;
        return HK_SUCCESS;
    }

    // 0x00F4DAA0 (:1639)
    virtual void setAll(const unsigned char* vals, int n)
    {
        m_data._reserve(getAllocator(), n);
        m_data.setSizeUnchecked(n);
        hkString::memCpy(m_data.begin(), vals, n);
    }
};

// size 0x1C
// An array of fixed-size tuples, stored flat in m_impl (m_tupleCount values per element).
struct ArrayOfTuplesImplementation : public hkDataArrayImpl
{
    // size 0x14
    // One tuple of the owner (elements m_baseIndex .. + tuple size of the flat array).
    struct View : public hkDataArrayImpl
    {
        // 0x00F46770 (:1652)
        View(ArrayOfTuplesImplementation* owner, int i) : m_owner(owner), m_baseIndex(i) {}

        // 0x00F49AB0 (:1657)
        virtual hkDataArrayImpl* swizzleObjectMember(const char* name) const
        {
            ArrayOfTuplesImplementation* swizzled = static_cast<ArrayOfTuplesImplementation*>(m_owner->swizzleObjectMember(name));
            return new View(swizzled, m_baseIndex);
        }

        // 0x00F467A0 (:1663)
        virtual hkTypeManager::Type* getType() const { return m_owner->getType()->getParent(); }
        // 0x00F467B0 (:1667)
        virtual hkDataWorld* getWorld() const { return m_owner->getWorld(); }

        // 0x00F467C0 (:1671)
        virtual hkDataClassImpl* getClass() const
        {
            hkTypeManager::Type* type = getType();
            return type->isClass() ? m_owner->m_world->findClass(type->getTypeName()) : nullptr;
        }

        // 0x00F46800 (:1687): Original: clears the whole flat array, not the tuple
        virtual void clear() { m_owner->m_impl->clear(); }
        // 0x00F46810 (:1691)
        virtual void reserve(int n) {}
        // 0x00F46820 (:1695)
        virtual void setSize(int n) {}
        // 0x00F46830 (:1699)
        virtual int getSize() const { return m_owner->getType()->getTupleSize(); }

        // 0x00F46850 (:1706)
        virtual void setAll(const unsigned char* v, int n)
        {
            for (int i = 0; i < n; i++)
            {
                m_owner->m_impl->setInt(m_baseIndex + i, v[i]);
            }
        }

        // 0x00F46890 (:1714)
        virtual void setAll(const float* v, int n)
        {
            for (int i = 0; i < n; i++)
            {
                m_owner->m_impl->setReal(m_baseIndex + i, v[i]);
            }
        }

        // 0x00F468D0 (:1722)
        virtual void setAll(const int* v, int n)
        {
            for (int i = 0; i < n; i++)
            {
                m_owner->m_impl->setInt(m_baseIndex + i, v[i]);
            }
        }

        // 0x00F46910 (:1730)
        virtual hkResult asStridedBasicArray(hkStridedBasicArray& arrOut) { return HK_FAILURE; }
        // 0x00F46920 (:1735)
        virtual const float* asVec(int index) const { return m_owner->m_impl->asVec(m_baseIndex + index); }
        // 0x00F46940 (:1735)
        virtual void setVec(int index, const float* val) { m_owner->m_impl->setVec(m_baseIndex + index, val); }
        // 0x00F46970 (:1736)
        virtual const char* asString(int index) const { return m_owner->m_impl->asString(m_baseIndex + index); }
        // 0x00F46990 (:1736)
        virtual void setString(int index, const char* val) { m_owner->m_impl->setString(m_baseIndex + index, val); }
        // 0x00F469C0 (:1737)
        virtual float asReal(int index) const { return m_owner->m_impl->asReal(m_baseIndex + index); }
        // 0x00F469E0 (:1737)
        virtual void setReal(int index, float val) { m_owner->m_impl->setReal(m_baseIndex + index, val); }
        // 0x00F46A10 (:1738)
        virtual int asInt(int index) const { return m_owner->m_impl->asInt(m_baseIndex + index); }
        // 0x00F46A30 (:1738)
        virtual void setInt(int index, int val) { m_owner->m_impl->setInt(m_baseIndex + index, val); }
        // 0x00F46A60 (:1739)
        virtual int64_t asInt64(int index) const { return m_owner->m_impl->asInt64(m_baseIndex + index); }
        // 0x00F46A80 (:1739)
        virtual void setInt64(int index, int64_t val) { m_owner->m_impl->setInt64(m_baseIndex + index, val); }
        // 0x00F46AB0 (:1740)
        virtual hkDataObjectImpl* asObject(int index) const { return m_owner->m_impl->asObject(m_baseIndex + index); }
        // 0x00F46AD0 (:1740)
        virtual void setObject(int index, hkDataObjectImpl* val) { m_owner->m_impl->setObject(m_baseIndex + index, val); }
        // 0x00F46B00 (:1741)
        virtual hkDataArrayImpl* asArray(int index) const { return m_owner->m_impl->asArray(m_baseIndex + index); }
        // 0x00F46B20 (:1741)
        virtual void setArray(int index, hkDataArrayImpl* val) { m_owner->m_impl->setArray(m_baseIndex + index, val); }
        // 0x00F46CB0
        virtual ~View() {}

    private:
        ArrayOfTuplesImplementation* m_owner; // 0x0C: not referenced
        int m_baseIndex; // 0x10
    };

    // 0x00F46730 (:1751)
    ArrayOfTuplesImplementation(hkDataWorldDict* world, hkDataArrayImpl* arr, int tc) : m_world(world), m_impl(arr), m_tupleCount(tc), m_swizzle(nullptr)
    {
        if (arr)
        {
            arr->addReference();
        }
    }

    // 0x00F46B50 (:1759)
    virtual ~ArrayOfTuplesImplementation()
    {
        if (m_impl)
        {
            m_impl->removeReference();
        }
        if (m_swizzle)
        {
            m_swizzle->removeReference();
        }
    }

    // 0x00F46B90 (:1772)
    void setImpl(hkDataArrayImpl* impl)
    {
        if (impl)
        {
            impl->addReference();
        }
        if (m_impl)
        {
            m_impl->removeReference();
        }
        m_impl = impl;
    }

    // 0x00F49B10 (:1788)
    // One swizzle wrapper is kept and refilled per call.
    virtual hkDataArrayImpl* swizzleObjectMember(const char* name) const
    {
        ArrayOfTuplesImplementation* self = const_cast<ArrayOfTuplesImplementation*>(this);
        if (m_swizzle == nullptr)
        {
            self->m_swizzle = new ArrayOfTuplesImplementation(m_world, nullptr, m_tupleCount);
            m_swizzle->addReference();
        }
        m_swizzle->setImpl(m_impl->swizzleObjectMember(name));
        return m_swizzle;
    }

    // 0x00F46BC0 (:1799)
    virtual hkTypeManager::Type* getType() const { return m_world->getTypeManager().makeTuple(m_impl->getType(), m_tupleCount); }
    // 0x00F46BF0 (:1804)
    virtual hkDataWorld* getWorld() const { return m_world; }

    // 0x00F46C00 (:1809)
    virtual hkDataClassImpl* getClass() const
    {
        hkTypeManager::Type* type = getType();
        return type->isClass() ? m_world->findClass(type->getTypeName()) : nullptr;
    }

    // 0x00F46C30 (:1820)
    virtual void clear() { m_impl->clear(); }
    // 0x00F46C40 (:1824)
    virtual void reserve(int n) { m_impl->reserve(n * m_tupleCount); }
    // 0x00F46C60 (:1828)
    virtual void setSize(int n) { m_impl->setSize(n * m_tupleCount); }
    // 0x00F46C80 (:1832)
    virtual int getSize() const { return m_impl->getSize() / m_tupleCount; }
    // 0x00F49BB0 (:1837)
    virtual hkDataArrayImpl* asArray(int idx) const { return new View(const_cast<ArrayOfTuplesImplementation*>(this), idx * m_tupleCount); }

    // 0x00F49C00 (:1844)
    // Copies a tuple element by element; an int source is read at its own width.
    virtual void setArray(int idx, hkDataArrayImpl* src)
    {
        const int base = idx * m_tupleCount;
        switch (src->getType()->getSubType())
        {
            case hkTypeManager::SUB_TYPE_BYTE:
                for (int i = 0; i < m_tupleCount; i++)
                {
                    m_impl->setInt(base + i, src->asInt(i));
                }
                break;
            case hkTypeManager::SUB_TYPE_REAL:
                for (int i = 0; i < m_tupleCount; i++)
                {
                    m_impl->setReal(base + i, src->asReal(i));
                }
                break;
            case hkTypeManager::SUB_TYPE_INT:
                // Original: the source is taken to be a VariableIntArrayImplementation (getIntType)
                for (int i = 0; i < m_tupleCount; i++)
                {
                    if (static_cast<VariableIntArrayImplementation*>(src)->getIntType() == VariableIntArrayImplementation::ARRAY_TYPE_INT32)
                    {
                        m_impl->setInt(base + i, src->asInt(i));
                    }
                    else
                    {
                        m_impl->setInt64(base + i, src->asInt64(i));
                    }
                }
                break;
            case hkTypeManager::SUB_TYPE_CSTRING:
                for (int i = 0; i < m_tupleCount; i++)
                {
                    m_impl->setString(base + i, src->asString(i));
                }
                break;
            case hkTypeManager::SUB_TYPE_CLASS:
            case hkTypeManager::SUB_TYPE_POINTER:
                for (int i = 0; i < m_tupleCount; i++)
                {
                    m_impl->setObject(base + i, src->asObject(i));
                }
                break;
            default:
                break;
        }
    }

    // 0x00F46CA0 (:1891)
    virtual hkResult asStridedBasicArray(hkStridedBasicArray& arrOut) { return HK_FAILURE; }

    hkDataWorldDict* m_world; // 0x0C
    hkDataArrayImpl* m_impl; // 0x10
    int m_tupleCount; // 0x14
    ArrayOfTuplesImplementation* m_swizzle; // 0x18
};

// size 0x20
struct RealArrayImplementation : public BasicArrayImplementation<float>
{
    // 0x00F4DB40 (:1904)
    RealArrayImplementation(hkDataWorldDict* world, hkTypeManager::Type* type) : BasicArrayImplementation<float>(world, type) {}
    // 0x00F4DB80 (:1905)
    virtual float asReal(int i) const { return m_data[i]; }
    // 0x00F4DB90 (:1906)
    virtual void setReal(int i, float val) { m_data[i] = val; }

    // 0x00F4DBB0 (:1909)
    virtual hkResult asStridedBasicArray(hkStridedBasicArray& arrOut)
    {
        arrOut.m_data = m_data.begin();
        arrOut.m_size = m_data.getSize();
        arrOut.m_stride = 4;
        arrOut.m_type = hkClassMember::TYPE_REAL;
        arrOut.m_tupleSize = 1;
        return HK_SUCCESS;
    }

    // 0x00F4DBF0 (:1921)
    virtual void setAll(const float* vals, int n)
    {
        m_data._reserve(getAllocator(), n);
        m_data.setSizeUnchecked(n);
        hkString::memCpy(m_data.begin(), vals, n * (int)sizeof(float));
    }
};

// size 0x18
// Reals in someone else's storage (a vec of a VecArrayImplementation); the size cannot change.
struct RealArrayView : public hkDataArrayImpl
{
    // 0x00F49ED0 (:1937)
    RealArrayView(hkDataWorldDict* world, float* data, int size) : m_world(world), m_data(data), m_size(size) {}
    // 0x00F49F00 (:1938)
    virtual float asReal(int i) const { return m_data[i]; }
    // 0x00F49F10 (:1939)
    virtual void setReal(int i, float val) { m_data[i] = val; }
    // 0x00F49F30 (:1942)
    virtual hkTypeManager::Type* getType() const { return m_world->getTypeManager().getReal(); }
    // 0x00F49F40 (:1945)
    virtual hkDataWorld* getWorld() const { return m_world; }
    // 0x00F49F50 (:1946)
    virtual int getSize() const { return m_size; }
    // 0x00F49F60 (:1947)
    virtual const hkDataClassImpl* getClass() const { return nullptr; }
    // 0x00F49F70 (:1948)
    virtual void reserve(int) {}
    // 0x00F49F80 (:1949)
    virtual void setSize(int) {}
    // 0x00F49F90 (:1950)
    virtual void clear() {}

    // 0x00F49FA0 (:1953)
    virtual hkResult asStridedBasicArray(hkStridedBasicArray& arrOut)
    {
        arrOut.m_data = m_data;
        arrOut.m_size = m_size;
        arrOut.m_stride = 4;
        arrOut.m_tupleSize = 1;
        arrOut.m_type = hkClassMember::TYPE_REAL;
        return HK_SUCCESS;
    }

    // 0x00F49FE0 (:1965)
    virtual void setAll(const float* vals, int n) { hkString::memCpy(m_data, vals, n * (int)sizeof(float)); }
    // 0x00F4A000
    virtual ~RealArrayView() {}

    hkDataWorldDict* m_world; // 0x0C
    float* m_data; // 0x10
    int m_size; // 0x14
};

// size 0x20
// An array of vecs (tuples of reals), stored flat.
struct VecArrayImplementation : public hkDataArrayImpl
{
    // 0x00F49E90 (:1979)
    VecArrayImplementation(hkDataWorldDict* world, hkTypeManager::Type* type) : m_world(world), m_type(type) {}
    // 0x00F4BBB0 (:1982)
    virtual ~VecArrayImplementation() { m_data._clearAndDeallocate(getAllocator()); }
    // 0x00F4A010 (:1985)
    hkMemoryAllocator& getAllocator() const { return *m_world->m_allocator; }
    // 0x00F4A160 (:1988)
    virtual const float* asVec(int index) const { return m_data.begin() + m_type->getTupleSize() * index; }

    // 0x00F4A180 (:1990)
    virtual void setVec(int index, const float* val)
    {
        const int n = m_type->getTupleSize();
        float* dst = m_data.begin() + n * index;
        for (int i = 0; i < n; i++)
        {
            dst[i] = val[i];
        }
    }

    // 0x00F4A020 (:1998)
    virtual hkTypeManager::Type* getType() const { return m_type; }
    // 0x00F4A030 (:2002)
    virtual hkDataWorld* getWorld() const { return m_world; }
    // 0x00F4A040 (:2006)
    virtual const hkDataClassImpl* getClass() const { return nullptr; }
    // 0x00F4BC00 (:2009)
    virtual void clear() { m_data.clear(); }
    // 0x00F4BC10 (:2010)
    virtual void reserve(int n) { m_data._reserve(getAllocator(), m_type->getTupleSize() * n); }
    // 0x00F4BC60 (:2012)
    virtual void setSize(int n) { m_data._setSize(getAllocator(), m_type->getTupleSize() * n, 0.0f); }
    // 0x00F4A220 (:2015)
    virtual int getSize() const { return m_data.getSize() / m_type->getTupleSize(); }

    // 0x00F4A240 (:2018)
    // A view of one vec (not referenced by the vec array).
    virtual hkDataArrayImpl* asArray(int index) const
    {
        const int n = m_type->getTupleSize();
        return new RealArrayView(m_world, const_cast<float*>(m_data.begin()) + n * index, n);
    }

    // 0x00F4BCC0 (:2024)
    // size vecs from data.
    virtual void setAll(const float* data, int size)
    {
        const int n = m_type->getTupleSize() * size;
        m_data._setSize(getAllocator(), n, 0.0f);
        hkString::memCpy(m_data.begin(), data, n * (int)sizeof(float));
    }

    // 0x00F4A050 (:2033)
    virtual void setAll(const hkBool*, int n) {}
    // 0x00F4A060 (:2034)
    virtual void setAll(const char*, int n) {}
    // 0x00F4A070 (:2035)
    virtual void setAll(const signed char*, int n) {}
    // 0x00F4A080 (:2036)
    virtual void setAll(const unsigned char*, int n) {}
    // 0x00F4A090 (:2037)
    virtual void setAll(const short*, int n) {}
    // 0x00F4A0A0 (:2038)
    virtual void setAll(const unsigned short*, int n) {}
    // 0x00F4A0B0 (:2039)
    virtual void setAll(const int*, int n) {}
    // 0x00F4A0C0 (:2040)
    virtual void setAll(const unsigned int*, int n) {}
    // 0x00F4A0D0 (:2041)
    virtual void setAll(const int64_t*, int n) {}
    // 0x00F4A0E0 (:2042)
    virtual void setAll(const uint64_t*, int n) {}
    // 0x00F4A0F0 (:2043)
    virtual void setAll(const hkHalf*, int n) {}

    // 0x00F4A2B0 (:2047)
    virtual hkResult asStridedBasicArray(hkStridedBasicArray& arrOut)
    {
        arrOut.m_data = m_data.begin();
        arrOut.m_size = m_data.getSize() / m_type->getTupleSize();
        arrOut.m_stride = m_type->getTupleSize() * (int)sizeof(float);
        arrOut.m_type = hkClassMember::TYPE_REAL;
        arrOut.m_tupleSize = m_type->getTupleSize();
        return HK_SUCCESS;
    }

    // 0x00F4A100 (:2056)
    int _getNumReal() const { return m_type->getTupleSize(); }

    hkArrayBase<float> m_data; // 0x0C
    hkDataWorldDict* m_world; // 0x18
    hkTypeManager::Type* m_type; // 0x1C
};

// 0x00F46D70 (:2060)
// The reals of a real array (a RealArrayImplementation, as a tuple member stores), or null when it holds fewer than
// numReals.
float* hkRealArrayImplementation_getReals(hkDataArrayImpl* arrayIn, int numReals)
{
    RealArrayImplementation* reals = static_cast<RealArrayImplementation*>(arrayIn);
    if (numReals <= reals->m_data.getSize())
    {
        return reals->m_data.begin();
    }
    return nullptr;
}

// size 0x20
// Pointers to objects, each holding a reference.
struct PointerArrayImplementation : public BasicArrayImplementation<hkDataObjectImpl*>
{
    // 0x00F4DCA0 (:2075)
    PointerArrayImplementation(hkDataWorldDict* world, hkTypeManager::Type* type) : BasicArrayImplementation<hkDataObjectImpl*>(world, type) {}

    // 0x00F4DD70 (:2079)
    virtual ~PointerArrayImplementation()
    {
        for (int i = 0; i < m_data.getSize(); i++)
        {
            if (m_data[i])
            {
                m_data[i]->removeReference();
            }
        }
    }

    // 0x00F4DD00 (:2089)
    virtual hkDataObjectImpl* asObject(int i) const { return m_data[i]; }

    // 0x00F4DD10 (:2093)
    virtual void setObject(int i, hkDataObjectImpl* val)
    {
        if (val)
        {
            val->addReference();
        }
        if (m_data[i])
        {
            m_data[i]->removeReference();
        }
        m_data[i] = val;
    }

    // 0x00F4DCE0 (:2098)
    virtual hkResult asStridedBasicArray(hkStridedBasicArray& arrOut) { return HK_FAILURE; }
};

// size 0x20
// Owned string copies.
struct CstringArrayImplementation : public BasicArrayImplementation<char*>
{
    // 0x00F4DE40 (:2103)
    CstringArrayImplementation(hkDataWorldDict* world, hkTypeManager::Type* type) : BasicArrayImplementation<char*>(world, type) {}

    // 0x00F4DF00 (:2105)
    virtual ~CstringArrayImplementation()
    {
        for (int i = 0; i < m_data.getSize(); i++)
        {
            hkString::strFree(m_data[i]);
        }
    }

    // 0x00F4DEB0 (:2111)
    virtual const char* asString(int i) const { return m_data[i]; }

    // 0x00F4DEC0 (:2113)
    virtual void setString(int i, const char* val)
    {
        hkString::strFree(m_data[i]);
        m_data[i] = val ? hkString::strDup(val) : nullptr;
    }

    // 0x00F4DE80 (:2117)
    virtual hkResult asStridedBasicArray(hkStridedBasicArray& arrOut) { return HK_FAILURE; }
};

// size 0x20
// Arrays of arrays, each holding a reference; an element is made on first access.
struct ArrayArrayImplementation : public BasicArrayImplementation<hkDataArrayImpl*>
{
    typedef BasicArrayImplementation<hkDataArrayImpl*> Parent;

    // 0x00F4DFC0 (:2125)
    ArrayArrayImplementation(hkDataWorldDict* world, hkTypeManager::Type* type) : Parent(world, type) {}

    // 0x00F4E0B0 (:2129)
    virtual ~ArrayArrayImplementation()
    {
        for (int i = 0; i < m_data.getSize(); i++)
        {
            if (m_data[i])
            {
                m_data[i]->removeReference();
            }
        }
    }

    // 0x00F4E020 (:2134)
    // Original: the elements are released but the size is kept (they still point at the released arrays).
    virtual void clear()
    {
        for (int i = 0; i < m_data.getSize(); i++)
        {
            if (m_data[i])
            {
                m_data[i]->removeReference();
            }
        }
    }

    // 0x00F4E050 (:2145)
    virtual void setArray(int index, hkDataArrayImpl* impl)
    {
        if (impl)
        {
            impl->addReference();
        }
        if (m_data[index])
        {
            m_data[index]->removeReference();
        }
        m_data[index] = impl;
    }

    // 0x00F4E130 (:2157)
    // The elements cut off are released (and left in the storage).
    virtual void setSize(int n)
    {
        for (int i = n; i < m_data.getSize(); i++)
        {
            if (m_data[i])
            {
                m_data[i]->removeReference();
            }
        }
        m_data._setSize(getAllocator(), n, nullptr);
    }

    // 0x00F508D0 (:2173)
    virtual hkDataArrayImpl* asArray(int i) const
    {
        hkDataArrayImpl* arr = m_data[i];
        if (arr == nullptr)
        {
            arr = hkDataArrayDict_create(m_world, m_type->getParent(), 0);
            arr->addReference();
            const_cast<ArrayArrayImplementation*>(this)->m_data[i] = arr;
        }
        return arr;
    }

    // 0x00F4E000 (:2185)
    virtual hkResult asStridedBasicArray(hkStridedBasicArray& arrOut) { return HK_FAILURE; }
};

// size 0x28
// An array of structs stored per member: each member of the class keeps an array of m_size values.
struct StructArrayImplementation : public hkDataArrayImpl
{
    typedef const _hkDataObject_MemberHandle* MemberHandle;

    // size 0x8
    struct MemberData
    {
        // 0x00F490A0
        MemberData() {}
        InternedString m_memberName; // 0x00
        hkDataArrayImpl* m_values; // 0x04
    };

    // size 0x14
    // A struct element: member access goes to the values arrays at m_index.
    struct Object : public hkDataObjectImpl
    {
        // 0x00F4F190 (:2393)
        Object(StructArrayImplementation* impl, int i) : m_impl(impl), m_index(i) {}
        // 0x00F4F1C0 (:2396)
        virtual const hkDataClassImpl* getClass() const { return m_impl->getClass(); }

        // 0x00F4F680 (:2400)
        virtual hkDataObject_Value accessByName(const char* nameIn)
        {
            if (const hkDataClassDict::MemberInfo* mi = m_impl->m_class->getMemberInfoByName(intern(nameIn)))
            {
                return hkDataObject_Value(this, handleOf(mi));
            }
            return hkDataObject_Value(nullptr, nullptr);
        }

        // 0x00F4F320 (:2415)
        virtual unsigned int hasMember(const char* name) const
        {
            const InternedString n = intern(name);
            for (int i = 0; i < m_impl->m_memberData.getSize(); i++)
            {
                if (n == m_impl->m_memberData[i].m_memberName)
                {
                    return 1;
                }
            }
            return 0;
        }

        // 0x00F4F370 (:2427)
        virtual unsigned int isSet(const _hkDataObject_MemberHandle* handle)
        {
            const InternedString name = memberInfoOf(handle)->m_name;
            for (int i = 0; i < m_impl->m_memberData.getSize(); i++)
            {
                if (name == m_impl->m_memberData[i].m_memberName)
                {
                    return 1;
                }
            }
            return 0;
        }

        // 0x00F4F1D0 (:2441)
        virtual hkDataObject_Handle getHandle() const
        {
            hkDataObject_Handle h;
            h.p0 = const_cast<Object*>(this);
            h.p1 = nullptr;
            return h;
        }

        // 0x00F4F1E0 (:2449)
        virtual int getMemberIterator() const { return 0; }
        // 0x00F4F3B0 (:2453)
        virtual unsigned int isValid(int it) const { return it < m_impl->m_memberData.getSize(); }
        // 0x00F4F1F0 (:2457)
        virtual int getNextMember(int it) const { return it + 1; }
        // 0x00F4F3D0 (:2461)
        virtual const char* getMemberName(int it) const { return m_impl->m_memberData[it].m_memberName.getString(); }

        // 0x00F4F3E0 (:2465)
        // Original: the handle is the member's name, as in hkDataObjectDict.
        virtual const hkDataObject_Value getMemberValue(int it) const
        {
            return hkDataObject_Value(const_cast<Object*>(this), reinterpret_cast<const _hkDataObject_MemberHandle*>(m_impl->m_memberData[it].m_memberName.getString()));
        }

        // 0x00F4F700 (:2470)
        virtual void getAllMemberHandles(hkArrayBase<_hkDataObject_MemberHandle const*>& handles) const { m_impl->m_class->_getAllMemberHandles(handles); }
        // 0x00F4F750 (:2474)
        virtual InternedString intern(const char* name) const { return m_impl->m_class->m_world->m_tracker->intern(name); }
        // 0x00F4F200 (:2475)
        virtual void selfDestruct() {}
        // 0x00F4F210 (:2478)
        virtual void destroy() {}
        // 0x00F4F8B0 (:2482)
        virtual const float* asVec(const _hkDataObject_MemberHandle* handle, int n) { return values(handle)->asVec(m_index); }
        // 0x00F4F8E0 (:2493)
        virtual const char* asString(const _hkDataObject_MemberHandle* handle) { return values(handle)->asString(m_index); }
        // 0x00F4F910 (:2494)
        virtual hkDataArrayImpl* asArray(const _hkDataObject_MemberHandle* handle) { return values(handle)->asArray(m_index); }
        // 0x00F4F940 (:2495)
        virtual int64_t asInt(const _hkDataObject_MemberHandle* handle) { return values(handle)->asInt64(m_index); }
        // 0x00F4F970 (:2496)
        virtual float asReal(const _hkDataObject_MemberHandle* handle) { return values(handle)->asReal(m_index); }
        // 0x00F4F9A0 (:2497)
        virtual hkHalf asHalf(const _hkDataObject_MemberHandle* handle) { return values(handle)->asHalf(m_index); }
        // 0x00F4F9D0 (:2501)
        virtual hkDataObjectImpl* asObject(const _hkDataObject_MemberHandle* handle) { return values(handle)->asObject(m_index); }
        // 0x00F4FA00 (:2507)
        virtual void assign(const _hkDataObject_MemberHandle* handle, const hkDataObject_Value& valueIn) { values(handle)->set(m_index, valueIn); }
        // 0x00F4FA30 (:2513)
        virtual void assign(const _hkDataObject_MemberHandle* handle, hkDataObjectImpl* valueIn) { values(handle)->setObject(m_index, valueIn); }
        // 0x00F4FA60 (:2519)
        virtual void assign(const _hkDataObject_MemberHandle* handle, hkDataArrayImpl* valueIn) { values(handle)->setArray(m_index, valueIn); }
        // 0x00F4FA90 (:2525)
        virtual void assign(const _hkDataObject_MemberHandle* handle, const char* valueIn) { values(handle)->setString(m_index, valueIn); }
        // 0x00F4FAC0 (:2531)
        virtual void assign(const _hkDataObject_MemberHandle* handle, float valueIn) { values(handle)->setReal(m_index, valueIn); }
        // 0x00F4FB00 (:2537)
        virtual void assign(const _hkDataObject_MemberHandle* handle, hkHalf valueIn) { values(handle)->setHalf(m_index, valueIn); }
        // 0x00F4FB30 (:2543)
        virtual void assign(const _hkDataObject_MemberHandle* handle, const float* valueIn, int nreal) { values(handle)->setVec(m_index, valueIn); }
        // 0x00F4FB60 (:2549)
        virtual void assign(const _hkDataObject_MemberHandle* handle, signed char valueIn) { values(handle)->setInt(m_index, valueIn); }
        // 0x00F4FB90 (:2555)
        virtual void assign(const _hkDataObject_MemberHandle* handle, short valueIn) { values(handle)->setInt(m_index, valueIn); }
        // 0x00F4FBC0 (:2561)
        virtual void assign(const _hkDataObject_MemberHandle* handle, int valueIn) { values(handle)->setInt(m_index, valueIn); }
        // 0x00F4FBF0 (:2567)
        virtual void assign(const _hkDataObject_MemberHandle* handle, int64_t valueIn) { values(handle)->setInt64(m_index, valueIn); }
        // 0x00F4F220 (:2573)
        virtual void getMemberInfo(const _hkDataObject_MemberHandle* handle, hkDataClass_MemberInfo& infoOut) { memberInfoOf(handle)->getMemberInfo(nullptr, infoOut); }
        // 0x00F4F250 (:2579)
        virtual unsigned int createdDuringPatching() const { return 0; }
        // 0x00F4F260 (:2585)
        virtual void destroyMember(const char* member) {}
        // 0x00F4F2A0
        virtual ~Object() {}

        // inline in the accessors: the member's values (added on first use)
        hkDataArrayImpl* values(const _hkDataObject_MemberHandle* handle) { return m_impl->m_memberData[m_impl->_findOrAdd(handle)].m_values; }

        StructArrayImplementation* m_impl; // 0x0C: not referenced
        int m_index; // 0x10
    };

    // 0x00F46CC0 (:2204)
    hkMemoryAllocator& getAllocator() { return *m_world->m_allocator; }
    // 0x00F4F5B0 (:2209)
    InternedString intern(const char* s) const { return m_class->m_world->m_tracker->intern(s); }

    // 0x00F4F2C0 (:2214)
    int _find(InternedString s) const
    {
        for (int i = 0; i < m_memberData.getSize(); i++)
        {
            if (m_memberData[i].m_memberName == s)
            {
                return i;
            }
        }
        return -1;
    }

    // 0x00F4F2F0 (:2227)
    int _find(const _hkDataObject_MemberHandle* handle) const { return _find(memberInfoOf(handle)->m_name); }
    // 0x00F4F610 (:2240)
    int _find(const char* nameIn) const { return _find(intern(nameIn)); }

    // 0x00F4E200 (:2246)
    // The member's values array of size elements, filled with the default (or new struct objects for a struct
    // member). A void member has no values (-1).
    int _addMember(const hkDataClassDict::MemberInfo& mi, int size)
    {
        if (mi.m_type->isVoid())
        {
            return -1;
        }
        MemberData& md = m_memberData._expandOne(getAllocator());
        md.m_memberName = InternedString();
        md.m_memberName = mi.m_name;
        hkTypeManager::Type* type = mi.m_type;
        md.m_values = hkDataArrayDict_create(m_world, type, 0);
        md.m_values->addReference();
        md.m_values->setSize(size);
        if (mi.m_valuePtr || type->isClass())
        {
            for (int i = 0; i < size; i++)
            {
                switch (type->getSubType())
                {
                    case hkTypeManager::SUB_TYPE_BYTE:
                    case hkTypeManager::SUB_TYPE_INT:
                        md.m_values->setInt64(i, *static_cast<const int64_t*>(mi.m_valuePtr));
                        break;
                    case hkTypeManager::SUB_TYPE_REAL:
                        md.m_values->setReal(i, *static_cast<const float*>(mi.m_valuePtr));
                        break;
                    case hkTypeManager::SUB_TYPE_TUPLE:
                        if (type->getParent()->isReal())
                        {
                            md.m_values->setVec(i, static_cast<const float*>(mi.m_valuePtr));
                        }
                        break;
                    case hkTypeManager::SUB_TYPE_CLASS:
                    {
                        const char* name = type->getTypeName();
                        hkDataClassDict* cls = name ? m_class->getWorld()->m_tracker->m_classes.getWithDefault(name, nullptr) : nullptr;
                        hkDataObject obj(m_class->m_world->newObject(hkDataClass(cls), false));
                        md.m_values->setObject(i, obj.getImplementation());
                        break;
                    }
                    case hkTypeManager::SUB_TYPE_CSTRING:
                        md.m_values->setString(i, static_cast<const char*>(mi.m_valuePtr));
                        break;
                    default:
                        break;
                }
            }
        }
        return m_memberData.getSize() - 1;
    }

    // 0x00F4F180 (:2319)
    virtual hkResult asStridedBasicArray(hkStridedBasicArray& arrOut) { return HK_FAILURE; }
    // 0x00F4F840 (:2344)
    int addMember(const _hkDataObject_MemberHandle* handle) { return _addMember(*memberInfoOf(handle), getSize()); }

    // 0x00F4F860 (:2350)
    int _findOrAdd(const _hkDataObject_MemberHandle* handle)
    {
        const int index = _find(handle);
        if (index != -1)
        {
            return index;
        }
        return _addMember(*memberInfoOf(handle), getSize());
    }

    // 0x00F4BDC0 (:2362)
    // An unknown name stops at the end of the array and returns (0x00F4BDDE).
    void removeMember(InternedString s)
    {
        if (m_memberData.getSize() <= 0)
        {
            return;
        }
        int index = 0;
        while (m_memberData[index].m_memberName != s)
        {
            index++;
            if (index >= m_memberData.getSize())
            {
                return;
            }
        }
        m_memberData[index].m_values->removeReference();
        m_memberData.removeAtAndCopy(index);
    }

    // 0x00F4A310 (:2376)
    // An unknown name stops at the end of the array and returns.
    void renameMember(InternedString oldName, InternedString newName)
    {
        if (m_memberData.getSize() <= 0)
        {
            return;
        }
        int index = 0;
        while (m_memberData[index].m_memberName != oldName)
        {
            index++;
            if (index >= m_memberData.getSize())
            {
                return;
            }
        }
        m_memberData[index].m_memberName = newName;
    }

    // 0x00F4F050 (:2595)
    // One values array per member of the class (inherited ones included); tracked as a struct array of the class.
    StructArrayImplementation(hkDataWorldDict* world, hkTypeManager::Type* type, const hkDataClassDict* klass, int size)
        : m_class(klass), m_type(type), m_size(size), m_world(world)
    {
        for (int i = 0; i < m_class->getNumMembers(); i++)
        {
            _addMember(m_class->getMemberInfoByIndex(i), m_size);
        }
        m_world->m_tracker->trackStructArray(this);
    }

    // 0x00F4FC60 (:2603)
    virtual ~StructArrayImplementation()
    {
        for (int i = 0; i < m_memberData.getSize(); i++)
        {
            m_memberData[i].m_values->removeReference();
        }
        m_world->m_tracker->untrackStructArray(this);
        m_memberData._clearAndDeallocate(getAllocator());
        m_class = nullptr;
    }

    // 0x00F4F7B0 (:2613)
    virtual const hkDataClassImpl* getClass() const { return m_class; }
    // 0x00F4F400 (:2617)
    virtual hkDataObjectImpl* asObject(int i) const { return new Object(const_cast<StructArrayImplementation*>(this), i); }

    // 0x00F4F450 (:2621)
    // Copies every member of val into element index.
    virtual void setObject(int index, hkDataObjectImpl* val)
    {
        val->addReference();
        for (int i = 0; i < m_memberData.getSize(); i++)
        {
            m_memberData[i].m_values->set(index, val->accessByName(m_memberData[i].m_memberName.getString()));
        }
        val->removeReference();
    }

    // 0x00F4F270 (:2634)
    virtual hkTypeManager::Type* getType() const { return m_type; }
    // 0x00F4F280 (:2638)
    virtual hkDataWorld* getWorld() const { return m_world; }

    // 0x00F4F4B0 (:2642)
    virtual void clear()
    {
        for (int i = 0; i < m_memberData.getSize(); i++)
        {
            m_memberData[i].m_values->clear();
        }
        m_size = 0;
    }

    // 0x00F4F4F0 (:2650)
    virtual void reserve(int n)
    {
        for (int i = 0; i < m_memberData.getSize(); i++)
        {
            m_memberData[i].m_values->reserve(n);
        }
    }

    // 0x00F4F520 (:2657)
    virtual void setSize(int n)
    {
        m_size = n;
        for (int i = 0; i < m_memberData.getSize(); i++)
        {
            m_memberData[i].m_values->setSize(n);
        }
    }

    // 0x00F4F290 (:2665)
    virtual int getSize() const { return m_size; }

    // 0x00F4F7C0 (:2682)
    // The member's values array itself. Original: an unknown name indexes -1.
    virtual hkDataArrayImpl* swizzleObjectMember(const char* name) const
    {
        return m_memberData[_find(name)].m_values;
    }

    hkArrayBase<MemberData> m_memberData; // 0x0C
    hkRefPtr<const hkDataClassDict> m_class; // 0x18
    hkTypeManager::Type* m_type; // 0x1C
    int m_size; // 0x20
    hkDataWorldDict* m_world; // 0x24
};

// 0x00F46F90 (:2690)
// The implementation for elements of type t: byte, real, int (variable width), string, struct (of a known class),
// pointer (to a known class or the homogeneous one), array (of size empty arrays), vec or tuple. Only struct and array
// arrays take the size.
hkDataArrayImpl* hkDataArrayDict_create(hkDataWorldDict* world, hkTypeManager::Type* t, int size)
{
    switch (t->getSubType())
    {
        case hkTypeManager::SUB_TYPE_BYTE:
            return new ByteArrayImplementation(world, t);
        case hkTypeManager::SUB_TYPE_REAL:
            return new RealArrayImplementation(world, t);
        case hkTypeManager::SUB_TYPE_INT:
            return new VariableIntArrayImplementation(world);
        case hkTypeManager::SUB_TYPE_CSTRING:
            return new CstringArrayImplementation(world, t);
        case hkTypeManager::SUB_TYPE_CLASS:
        {
            hkDataClassDict* klass = static_cast<hkDataClassDict*>(world->findClass(t->getTypeName()));
            if (klass)
            {
                return new StructArrayImplementation(world, t, klass, size);
            }
            return nullptr;
        }
        case hkTypeManager::SUB_TYPE_POINTER:
        {
            hkTypeManager::Type* target = t->getParent();
            if (target->getTypeName() && world->findClass(target->getTypeName()) == nullptr)
            {
                return nullptr;
            }
            return new PointerArrayImplementation(world, t);
        }
        case hkTypeManager::SUB_TYPE_ARRAY:
        {
            hkDataArrayImpl* arr = new ArrayArrayImplementation(world, t);
            arr->setSize(size);
            for (int i = 0; i < size; i++)
            {
                arr->setArray(i, hkDataArrayDict_create(world, t->getParent(), 0));
            }
            return arr;
        }
        case hkTypeManager::SUB_TYPE_TUPLE:
            if (t->getParent()->isReal())
            {
                return new VecArrayImplementation(world, t);
            }
            return new ArrayOfTuplesImplementation(world, hkDataArrayDict_create(world, t->getParent(), 0), t->getTupleSize());
        default:
            return nullptr;
    }
}

// ---- hkDataWorldDict::ObjectTracker

// 0x00F482D0 (:2779)
hkDataWorldDict::ObjectTracker::ObjectTracker(hkMemoryAllocator* mall) : m_allocator(mall), m_topLevelObject(nullptr)
{
}

// Warns about a world object that is still referenced from outside (an hkDataObject wrapper) when it is destroyed.
static void warnExternallyReferenced(hkDataObjectDict* obj, int id, int line)
{
    char buf[512];
    hkErrStream os(buf, sizeof(buf));
    os << "The object of class " << obj->getClass()->getName()
       << " is about to be removed and all references to it will be invalid.\nHowever, the object is referenced ("
       << (unsigned int)obj->getExternalReferenceCount()
       << ") from outside the world and it will lead to unexpected behavior or crash.";
    hkErrorFwd::messageWarning(id, buf, "Data\\Dict\\hkDataObjectDict.cpp", line);
}

// 0x00F47730 (:2784)
// Destroys every object (warning when one is still referenced from outside), then the classes, the class name
// holders, the interned strings and the struct array maps.
hkDataWorldDict::ObjectTracker::~ObjectTracker()
{
    hkLocalArray<hkDataObjectDict*> objects(m_objectsFromClass.m_valueChain.getSize());
    hkLocalArray<hkDataClassDict*> classes(m_classes.getSize());
    m_derivedFromParent.clear();
    m_topLevelObject = nullptr;
    for (hkStringMap<hkDataClassDict*>::Iterator it = m_classes.getIterator(); m_classes.isValid(it); it = m_classes.getNext(it))
    {
        hkDataClassDict* k = m_classes.getValue(it);
        classes.pushBackUnchecked(k);
        int index = m_objectsFromClass.getFirstIndex(k);
        if (index != -1)
        {
            for (; index != -1; index = m_objectsFromClass.getNextIndex(index))
            {
                objects.pushBack(m_objectsFromClass.getValue(index));
            }
            m_objectsFromClass.removeKey(k);
        }
    }
    for (int i = 0; i < objects.getSize(); i++)
    {
        hkDataObjectDict* obj = objects[i];
        if (obj->getExternalReferenceCount() != 0)
        {
            warnExternallyReferenced(obj, 0x760D3795, 0xAFD);
        }
        obj->selfDestruct();
    }
    for (int i = 0; i < objects.getSize(); i++)
    {
        objects[i]->removeReference();
    }
    for (int i = 0; i < classes.getSize(); i++)
    {
        classes[i]->removeReference();
    }
    for (hkStringMap<InternedStringRefCounted*>::Iterator it = m_internedClassNames.getIterator(); m_internedClassNames.isValid(it); it = m_internedClassNames.getNext(it))
    {
        m_internedClassNames.getValue(it)->removeReference();
    }
    for (hkStringMap<char*>::Iterator it = m_interns.getIterator(); m_interns.isValid(it); it = m_interns.getNext(it))
    {
        hkString::strFree(m_interns.getValue(it));
    }
    for (ClassToArraysMap::Iterator it = m_arraysFromClass.getIterator(); m_arraysFromClass.isValid(it); it = m_arraysFromClass.getNext(it))
    {
        if (ArrayMap* arrays = m_arraysFromClass.getValue(it))
        {
            arrays->clearAndDeallocate();
            arrays->~ArrayMap();
            hkMemory::heapBlockFree(arrays, sizeof(ArrayMap));
        }
    }
}

// 0x00F50970 (:2849)
// The first object made is the world's contents.
void hkDataWorldDict::ObjectTracker::trackObject(hkDataObjectDict* obj)
{
    m_objectsFromClass.insert(const_cast<hkDataClassDict*>(static_cast<const hkDataClassDict*>(obj->getClass())), obj);
    if (m_topLevelObject == nullptr)
    {
        m_topLevelObject = obj;
    }
}

// 0x00F4E400 (:2860)
// The per-class map is made on first use (a heap block, not referenced).
void hkDataWorldDict::ObjectTracker::trackStructArray(hkDataArrayImpl* a)
{
    hkDataClassDict* klass = const_cast<hkDataClassDict*>(static_cast<const hkDataClassDict*>(a->getClass()));
    ArrayMap* arrays = m_arraysFromClass.getWithDefault(klass, nullptr);
    if (arrays == nullptr)
    {
        void* mem = hkMemory::heapBlockAlloc(sizeof(ArrayMap));
        if (mem)
        {
            arrays = ::new (mem) ArrayMap();
        }
        m_arraysFromClass.insert(klass, arrays);
    }
    arrays->insert(a, 0);
}

// 0x00F4FC30 (:2875)
void hkDataWorldDict::ObjectTracker::untrackStructArray(hkDataArrayImpl* a)
{
    ArrayMap* arrays = m_arraysFromClass.getWithDefault(const_cast<hkDataClassDict*>(static_cast<const hkDataClassDict*>(a->getClass())), nullptr);
    arrays->remove(a);
}

// 0x00F4FDC0 (:2884)
// The struct arrays of class c (and of its derived classes for baseClass).
void hkDataWorldDict::ObjectTracker::getTrackedStructArrays(hkDataClassDict* c, unsigned int baseClass, hkArray<hkDataArrayImpl*, hkContainerTempAllocator>& arraysOut)
{
    ArrayMap* arrays = m_arraysFromClass.getWithDefault(c, nullptr);
    if (arrays && arrays->getSize() != 0)
    {
        const int start = arraysOut.getSize();
        arraysOut.reserve(start + arrays->getSize());
        arraysOut.setSizeUnchecked(start + arrays->getSize());
        hkDataArrayImpl** out = arraysOut.begin() + start;
        for (ArrayMap::Iterator it = arrays->getIterator(); arrays->isValid(it); it = arrays->getNext(it))
        {
            *out++ = arrays->getKey(it);
        }
    }
    if (baseClass)
    {
        for (int index = m_derivedFromParent.getFirstIndex(c); index != -1; index = m_derivedFromParent.getNextIndex(index))
        {
            getTrackedStructArrays(m_derivedFromParent.getValue(index), baseClass, arraysOut);
        }
    }
}

// 0x00F509A0 (:2908)
// A class of a name already tracked is ignored.
void hkDataWorldDict::ObjectTracker::trackClass(hkDataClassDict* klass)
{
    hkDataClassDict* parent = static_cast<hkDataClassDict*>(klass->getParent());
    if (!m_classes.isValid(m_classes.findKey(klass->getName())))
    {
        m_classes.insert(klass->getName(), klass);
        if (parent)
        {
            m_derivedFromParent.insert(parent, klass);
        }
    }
}

// 0x00F4E480 (:2932)
void hkDataWorldDict::ObjectTracker::findTrackedClasses(hkArray<hkDataClassImpl*, hkContainerTempAllocator>& classesOut) const
{
    classesOut.reserve(m_classes.getSize());
    for (hkStringMap<hkDataClassDict*>::Iterator it = m_classes.getIterator(); m_classes.isValid(it); it = m_classes.getNext(it))
    {
        classesOut.pushBackUnchecked(m_classes.getValue(it));
    }
}

// 0x00F4FED0 (:2942)
// The objects of the class, then (addStructs) the elements of its struct arrays as new struct objects, then
// (baseClass) the same for the derived classes.
void hkDataWorldDict::ObjectTracker::findTrackedObjectsByBase(const char* className, unsigned int baseClass, unsigned int addStructs, hkArray<hkDataObjectImpl*, hkContainerTempAllocator>& objectsOut) const
{
    hkDataClassDict* k = className ? m_classes.getWithDefault(className, nullptr) : nullptr;
    if (k == nullptr)
    {
        return;
    }
    for (int index = m_objectsFromClass.getFirstIndex(k); index != -1; index = m_objectsFromClass.getNextIndex(index))
    {
        objectsOut.pushBack(m_objectsFromClass.getValue(index));
    }
    if (addStructs)
    {
        ArrayMap* arrays = m_arraysFromClass.getWithDefault(k, nullptr);
        if (arrays && arrays->getSize() != 0)
        {
            for (ArrayMap::Iterator it = arrays->getIterator(); arrays->isValid(it); it = arrays->getNext(it))
            {
                hkDataArrayImpl* a = arrays->getKey(it);
                for (int i = 0; i < a->getSize(); i++)
                {
                    objectsOut.pushBack(a->asObject(i));
                }
            }
        }
    }
    if (baseClass)
    {
        for (int index = m_derivedFromParent.getFirstIndex(k); index != -1; index = m_derivedFromParent.getNextIndex(index))
        {
            findTrackedObjectsByBase(m_derivedFromParent.getValue(index)->getName(), baseClass, addStructs, objectsOut);
        }
    }
}

// 0x00F500B0 (:2979)
void hkDataWorldDict::ObjectTracker::getTrackedObjects(const char* className, unsigned int baseClass, unsigned int addStructs, hkArray<hkDataObjectImpl*, hkContainerTempAllocator>& objectsOut) const
{
    objectsOut.clear();
    findTrackedObjectsByBase(className, baseClass, addStructs, objectsOut);
}

// 0x00F50A10 (:2995)
// Destroys the objects of the class and its derived classes (the top-level object too; warning when one is still
// referenced from outside).
void hkDataWorldDict::ObjectTracker::removeClassContent(hkDataClassDict* klass)
{
    const char* name = klass->getName();
    hkDataClassDict* tracked = name ? m_classes.getWithDefault(name, nullptr) : nullptr;
    for (int index = m_derivedFromParent.getFirstIndex(klass); index != -1; index = m_derivedFromParent.getNextIndex(index))
    {
        removeClassContent(m_derivedFromParent.getValue(index));
    }
    int index = m_objectsFromClass.getFirstIndex(tracked);
    if (index == -1)
    {
        return;
    }
    hkLocalArray<hkDataObjectDict*> objects(512); // Magic number: rfg.exe's capacity (0x200)
    for (; index != -1; index = m_objectsFromClass.getNextIndex(index))
    {
        objects.pushBack(m_objectsFromClass.getValue(index));
    }
    for (int i = 0; i < objects.getSize(); i++)
    {
        if (m_topLevelObject == objects[i])
        {
            m_topLevelObject = nullptr;
        }
        if (objects[i]->getExternalReferenceCount() != 0)
        {
            warnExternallyReferenced(objects[i], 0x760D3794, 0xBD4);
        }
        objects[i]->selfDestruct();
    }
    for (int i = 0; i < objects.getSize(); i++)
    {
        objects[i]->removeReference();
    }
    m_objectsFromClass.removeKey(tracked);
}

// 0x00F50CF0 (:3041)
// Removes the class with its objects and, recursively, its derived classes.
void hkDataWorldDict::ObjectTracker::untrackClass(hkDataClassDict* klass)
{
    const char* name = klass->getName();
    hkDataClassDict* tracked = name ? m_classes.getWithDefault(name, nullptr) : nullptr;
    removeClassContent(klass);
    if (hkDataClassDict* parent = static_cast<hkDataClassDict*>(tracked->getParent()))
    {
        m_derivedFromParent.removeByValue(parent, klass);
    }
    klass->selfDestruct();

    hkLocalArray<hkDataClassDict*> derived(64); // Magic number: rfg.exe's capacity (0x40)
    int index = m_derivedFromParent.getFirstIndex(klass);
    const bool hasDerived = index != -1;
    for (; index != -1; index = m_derivedFromParent.getNextIndex(index))
    {
        derived.pushBack(m_derivedFromParent.getValue(index));
    }
    for (int i = 0; i < derived.getSize(); i++)
    {
        untrackClass(derived[i]);
        derived[i]->removeReference();
    }
    if (hasDerived)
    {
        m_derivedFromParent.removeKey(klass);
    }
    InternedStringRefCounted* holder = m_internedClassNames.getWithDefault(name, nullptr);
    m_internedClassNames.remove(name);
    holder->removeReference();
    m_classes.remove(name);
}

// 0x00F50F50 (:3105)
void hkDataWorldDict::ObjectTracker::retrackDerivedClass(hkDataClassDict* oldParent, hkDataClassDict* klass)
{
    if (oldParent)
    {
        for (int index = m_derivedFromParent.getFirstIndex(oldParent); index != -1; index = m_derivedFromParent.getNextIndex(index))
        {
            if (m_derivedFromParent.getValue(index) == klass)
            {
                m_derivedFromParent.removeByIndex(oldParent, index);
                break;
            }
        }
    }
    if (hkDataClassDict* parent = static_cast<hkDataClassDict*>(klass->getParent()))
    {
        m_derivedFromParent.insert(parent, klass);
    }
}

// 0x00F4BE70 (:3129)
// The class map and the name holder move to the interned new name (the class's name handle follows the holder).
void hkDataWorldDict::ObjectTracker::retrackRenamedClass(const char* oldName, const char* newName)
{
    hkDataClassDict* klass = oldName ? m_classes.getWithDefault(oldName, nullptr) : nullptr;
    const char* interned = intern(newName).getString();
    replaceClassNameInInternHandle(oldName, interned);
    m_classes.remove(oldName);
    m_classes.insert(interned, klass);
}

// 0x00F46CF0 (:3141)
hkDataObjectDict* hkDataWorldDict::ObjectTracker::getTopLevelObject() { return m_topLevelObject; }

// 0x00F50FB0 (:3146)
void hkDataWorldDict::ObjectTracker::retractCastedObject(hkDataObjectDict* obj, hkDataClassDict* newClass)
{
    hkDataClassDict* k = const_cast<hkDataClassDict*>(static_cast<const hkDataClassDict*>(obj->getClass()));
    for (int index = m_objectsFromClass.getFirstIndex(k); k && index != -1; index = m_objectsFromClass.getNextIndex(index))
    {
        if (m_objectsFromClass.getValue(index) == obj)
        {
            m_objectsFromClass.removeByIndex(k, index);
            m_objectsFromClass.insert(newClass, obj);
            k = nullptr;
        }
    }
}

// 0x00F4A340 (:3172)
// The world's copy of the string (made on first use; null stays null).
InternedString hkDataWorldDict::ObjectTracker::intern(const char* sin)
{
    if (sin == nullptr)
    {
        return InternedString();
    }
    char* key = m_interns.getWithDefault(sin, nullptr);
    if (key == nullptr)
    {
        key = hkString::strDup(sin);
        m_interns.insert(key, key);
    }
    return InternedString(key);
}

// 0x00F4E510 (:3190)
// The shared name holder of a class name (made on first use, held by the map).
InternedStringHandle hkDataWorldDict::ObjectTracker::internClassNameHandle(const char* sin)
{
    InternedStringHandle handle;
    if (sin)
    {
        InternedStringRefCounted* holder = m_internedClassNames.getWithDefault(sin, nullptr);
        if (holder == nullptr)
        {
            holder = new InternedStringRefCounted(intern(sin));
            m_internedClassNames.insert(holder->m_cachedString, holder);
        }
        handle.m_cached = holder;
    }
    return handle;
}

// 0x00F4A390 (:3206)
void hkDataWorldDict::ObjectTracker::replaceClassNameInInternHandle(const char* oldName, const char* newName)
{
    if (oldName != newName)
    {
        InternedStringRefCounted* holder = m_internedClassNames.getWithDefault(oldName, nullptr);
        m_internedClassNames.remove(oldName);
        holder->m_cachedString = newName;
        m_internedClassNames.insert(newName, holder);
    }
}

// ---- hkDataWorldDict

// The member type override attribute (as hkDataObjectNative.cpp's).
static const char* s_hkDataObjectTypeAttributeID = "hk.DataObjectType";

// 0x00F48360 (:3222)
// The thread's heap allocator holds the world's storage.
hkDataWorldDict::hkDataWorldDict()
{
    m_allocator = &hkMemoryRouter::getInstance().heap();
    m_tracker = new ObjectTracker(m_allocator);
}

// 0x00F483D0 (:3228)
hkDataWorldDict::hkDataWorldDict(hkMemoryAllocator* mall)
{
    m_allocator = mall;
    m_tracker = new ObjectTracker(m_allocator);
}

// 0x00F48430 (:3234)
hkDataWorldDict::~hkDataWorldDict()
{
    delete m_tracker;
}

// 0x00F46D20 (:3239)
hkDataObject hkDataWorldDict::getContents() const
{
    return hkDataObject(m_tracker->m_topLevelObject);
}

// 0x00F47D30 (:3244)
// The object holds one reference for the world.
hkDataObjectImpl* hkDataWorldDict::newObject(const hkDataClass& klass, bool createdDuringPatching) const
{
    hkDataObjectDict* obj = new hkDataObjectDict(static_cast<const hkDataClassDict*>(klass.getImplementation()), createdDuringPatching);
    obj->addReference();
    m_tracker->trackObject(obj);
    return obj;
}

// 0x00F47DD0 (:3252)
// The parent must already be in the world. Original: the members' default values (valuePtr) are not taken.
hkDataClassImpl* hkDataWorldDict::newClass(const hkDataClass::Cinfo& cinfo)
{
    hkDataClassDict* klass = new hkDataClassDict(this, cinfo.name, cinfo.version);
    if (cinfo.parent)
    {
        klass->m_parent = m_tracker->m_classes.getWithDefault(cinfo.parent, nullptr);
    }
    klass->addReference();
    m_tracker->trackClass(klass);
    for (int i = 0; i < cinfo.members.getSize(); i++)
    {
        const hkDataClass::Cinfo::Member& m = cinfo.members[i];
        hkDataClassDict::MemberInfo& mi = klass->m_memberInfo._expandOne(*klass->m_world->m_allocator);
        mi.m_name = m_tracker->intern(m.name);
        mi.m_default.i = 0;
        mi.m_type = m.type;
        mi.m_valuePtr = nullptr;
    }
    m_typeManager.addClass(cinfo.name);
    return klass;
}

// 0x00F47230 (:3280)
// A new array for the member, assigned to the object. Original: the array is returned after the temporary wrapper
// released it (the object's reference keeps it alive only when the assignment kept it).
hkDataArrayImpl* hkDataWorldDict::newArray(hkDataObject& obj, const _hkDataObject_MemberHandle* handle, const hkDataClass_MemberInfo& minfo) const
{
    hkTypeManager::Type* type = minfo.m_type;
    const int size = type->isTuple() ? type->getTupleSize() : 0;
    hkDataArrayImpl* arr = hkDataArrayDict_create(const_cast<hkDataWorldDict*>(this), type->getParent(), size);
    {
        hkDataArray wrapper(arr);
        obj.getImplementation()->assign(handle, arr);
    }
    return arr;
}

// 0x00F46D80 (:3303)
hkEnum<enum hkDataWorld::DataWorldType, int> hkDataWorldDict::getType() const
{
    hkEnum<enum hkDataWorld::DataWorldType, int> type;
    type = TYPE_DICTIONARY;
    return type;
}

// 0x00F46EB0 (:3309)
void hkDataWorldDict::findAllClasses(hkArray<hkDataClassImpl*, hkContainerTempAllocator>& classesOut) const
{
    m_tracker->findTrackedClasses(classesOut);
}

// 0x00F47F60 (:3314)
// A dictionary class from a native hkClass (its parents first, when not in the world yet). Serialize-ignored members
// are void; the member type's class can be renamed by the hk.DataObjectType attribute of the member or of its class.
hkDataClassImpl* hkDataWorldDict::wrapClass(const hkClass& klass)
{
    hkDataClassDict* dict = new hkDataClassDict(this, klass.getName(), klass.getDescribedVersion());
    if (klass.getParent())
    {
        const char* parentName = klass.getParent()->getName();
        hkDataClassDict* parent = parentName ? m_tracker->m_classes.getWithDefault(parentName, nullptr) : nullptr;
        if (parent == nullptr)
        {
            parent = static_cast<hkDataClassDict*>(wrapClass(*klass.getParent()));
        }
        dict->m_parent = parent;
    }
    dict->addReference();
    m_tracker->trackClass(dict);
    for (int i = 0; i < klass.getNumDeclaredMembers(); i++)
    {
        const hkClassMember& mem = klass.getDeclaredMember(i);
        hkTypeManager::Type* type = m_typeManager.getVoid();
        if (!mem.getFlags().allAreSet(hkClassMember::SERIALIZE_IGNORED))
        {
            const char* typeName = nullptr;
            if (const hkClass* memberClass = mem.getClass())
            {
                typeName = memberClass->getName();
                const hkVariant* attr = mem.getAttribute(s_hkDataObjectTypeAttributeID);
                if (attr == nullptr)
                {
                    attr = memberClass->getAttribute(s_hkDataObjectTypeAttributeID);
                }
                if (attr)
                {
                    hkClassMemberAccessor accessor(*attr, "typeName");
                    typeName = accessor.asCstring(0);
                }
            }
            type = hkDataObjectUtil::getTypeFromMemberTypeClassName(m_typeManager, mem.getType(), mem.getSubType(), typeName, mem.getCstyleArraySize());
        }
        hkDataClassDict::MemberInfo& mi = dict->m_memberInfo._expandOne(*dict->m_world->m_allocator);
        mi.m_name = m_tracker->intern(mem.getName());
        mi.m_default.i = 0;
        mi.m_type = type;
        mi.m_valuePtr = nullptr;
    }
    return dict;
}

// 0x00F48480 (:3367)
// Copies a class (and the parent and member classes it needs) from another world.
hkDataClassImpl* hkDataWorldDict::copyClassFromWorld(const char* name, const hkDataWorld& worldFrom)
{
    if (hkDataClassImpl* existing = findClass(name))
    {
        return existing;
    }
    hkDataClass srcClass(worldFrom.findClass(name));
    hkDataClass::Cinfo cinfo;
    cinfo.name = srcClass.getName();
    cinfo.version = srcClass.getVersion();
    hkDataClassImpl* srcParent = srcClass.getImplementation()->getParent();
    if (srcParent)
    {
        cinfo.parent = srcParent->getName();
        copyClassFromWorld(srcParent->getName(), worldFrom);
        // the parent's copy may have copied this class through a member
        if (hkDataClassImpl* existing = findClass(name))
        {
            return existing;
        }
    }
    else
    {
        cinfo.parent = nullptr;
    }
    hkDataClass dstClass(newClass(cinfo));
    const int n = srcClass.getNumDeclaredMembers();
    hkArray<hkDataClass_MemberInfo, hkContainerTempAllocator> minfos;
    minfos.setSize(n);
    for (int i = 0; i < n; i++)
    {
        minfos[i] = hkDataClass_MemberInfo();
    }
    srcClass.getAllDeclaredMemberInfo(minfos);
    cinfo.members.reserve(n);
    for (int i = 0; i < n; i++)
    {
        hkTypeManager::Type* type = m_typeManager.copyType(minfos[i].m_type);
        hkTypeManager::Type* terminal = type->findTerminal();
        if (terminal->isClass() && worldFrom.findClass(type->getTypeName()))
        {
            copyClassFromWorld(terminal->getTypeName(), worldFrom);
        }
        addClassMember(dstClass, minfos[i].m_name, type, minfos[i].m_valuePtr);
    }
    return dstClass.getImplementation();
}

// 0x00F46D90 (:3419)
hkDataClassImpl* hkDataWorldDict::findClass(const char* name) const
{
    return name ? m_tracker->m_classes.getWithDefault(name, nullptr) : nullptr;
}

// 0x00F46D40 (:3424)
// A dictionary handle is the object itself.
hkDataObject hkDataWorldDict::findObject(const hkDataObject_Handle& handle) const
{
    return hkDataObject(static_cast<hkDataObjectImpl*>(handle.p0));
}

// 0x00F46E30 (:3429)
void hkDataWorldDict::renameClass(hkDataClass& klass, const char* newName)
{
    const char* clsName = m_tracker->intern(klass.getName()).getString();
    if (m_typeManager.getClass(clsName))
    {
        m_typeManager.renameClass(clsName, newName);
    }
    m_tracker->retrackRenamedClass(clsName, newName);
}

// 0x00F48200 (:3445)
// The wrapper loses the class; the world's reference is released.
void hkDataWorldDict::removeClass(hkDataClass& klass)
{
    if (hkTypeManager::Type* clsType = m_typeManager.getClass(klass.getName()))
    {
        m_typeManager.removeClass(clsType);
    }
    hkDataClassDict* dict = static_cast<hkDataClassDict*>(klass.getImplementation());
    klass.m_impl = nullptr;
    m_tracker->untrackClass(dict);
    dict->removeReference();
}

// 0x00F46D60 (:3461)
void hkDataWorldDict::setClassVersion(hkDataClass& klass, int newVersion)
{
    static_cast<hkDataClassDict*>(klass.getImplementation())->m_version = newVersion;
}

// 0x00F48250 (:3467)
void hkDataWorldDict::setClassParent(hkDataClass& klass, hkDataClass& parent)
{
    hkDataClassDict* dict = static_cast<hkDataClassDict*>(klass.getImplementation());
    hkDataClassDict* oldParent = static_cast<hkDataClassDict*>(dict->getParent());
    dict->setParent(static_cast<hkDataClassDict*>(parent.getImplementation()));
    m_tracker->retrackDerivedClass(oldParent, dict);
}

// 0x00F46EC0 (:3477)
// A member of the name declared already is replaced.
void hkDataWorldDict::addClassMember(hkDataClass& klass, const char* name, hkTypeManager::Type* type, const void* valuePtr)
{
    hkDataClassDict* dict = static_cast<hkDataClassDict*>(klass.getImplementation());
    const InternedString interned = m_tracker->intern(name);
    if (dict->getDeclaredMemberIndexByName(name) != -1)
    {
        dict->removeMember(interned);
    }
    dict->addMember(interned, type, valuePtr);
}

// 0x00F46DB0 (:3492)
void hkDataWorldDict::setClassMemberDefault(hkDataClass& klass, const char* name, const void* valuePtr)
{
    static_cast<hkDataClassDict*>(klass.getImplementation())->setMemberDefault(m_tracker->intern(name), valuePtr);
}

// 0x00F472A0 (:3502)
// Renames the member in the objects of the class and its derived classes, their struct arrays, and the class.
void hkDataWorldDict::renameClassMember(hkDataClass& klass, const char* oldName, const char* newName)
{
    hkDataClassDict* dict = static_cast<hkDataClassDict*>(klass.getImplementation());
    const InternedString oldInterned = m_tracker->intern(oldName);
    const InternedString newInterned = m_tracker->intern(newName);
    hkArray<hkDataObjectImpl*, hkContainerTempAllocator> objects;
    m_tracker->getTrackedObjects(klass.getName(), 1, 0, objects);
    for (int i = 0; i < objects.getSize(); i++)
    {
        hkDataObjectDict* obj = static_cast<hkDataObjectDict*>(objects[i]);
        for (int j = 0; j < obj->m_memberData.getSize(); j++)
        {
            if (obj->m_memberData[j].m_memberName == oldInterned)
            {
                obj->m_memberData[j].m_memberName = newInterned;
                break;
            }
        }
    }
    hkArray<hkDataArrayImpl*, hkContainerTempAllocator> arrays;
    m_tracker->getTrackedStructArrays(dict, 1, arrays);
    for (int i = 0; i < arrays.getSize(); i++)
    {
        StructArrayImplementation* sa = static_cast<StructArrayImplementation*>(arrays[i]);
        for (int j = 0; j < sa->m_memberData.getSize(); j++)
        {
            if (sa->m_memberData[j].m_memberName == oldInterned)
            {
                sa->m_memberData[j].m_memberName = newInterned;
                break;
            }
        }
    }
    dict->renameMember(oldInterned, newInterned);
}

// 0x00F474B0 (:3525)
// Removes the member from the objects of the class and its derived classes (structs are not destroyed), their struct
// arrays, and the class.
void hkDataWorldDict::removeClassMember(hkDataClass& klass, const char* name)
{
    hkDataClassDict* dict = static_cast<hkDataClassDict*>(klass.getImplementation());
    const InternedString interned = m_tracker->intern(name);
    hkArray<hkDataObjectImpl*, hkContainerTempAllocator> objects;
    m_tracker->getTrackedObjects(klass.getName(), 1, 0, objects);
    for (int i = 0; i < objects.getSize(); i++)
    {
        static_cast<hkDataObjectDict*>(objects[i])->removeMember(interned, false);
    }
    hkArray<hkDataArrayImpl*, hkContainerTempAllocator> arrays;
    m_tracker->getTrackedStructArrays(dict, 1, arrays);
    for (int i = 0; i < arrays.getSize(); i++)
    {
        StructArrayImplementation* sa = static_cast<StructArrayImplementation*>(arrays[i]);
        for (int j = 0; j < sa->m_memberData.getSize(); j++)
        {
            if (sa->m_memberData[j].m_memberName == interned)
            {
                sa->m_memberData[j].m_values->removeReference();
                sa->m_memberData.removeAtAndCopy(j);
                break;
            }
        }
    }
    dict->removeMember(interned);
}

// 0x00F482B0 (:3548)
void hkDataWorldDict::castObject(hkDataObject& obj, const hkDataClass& castClass)
{
    m_tracker->retractCastedObject(static_cast<hkDataObjectDict*>(obj.getImplementation()), const_cast<hkDataClassDict*>(static_cast<const hkDataClassDict*>(castClass.getImplementation())));
}

// 0x00F476F0 (:3557)
void hkDataWorldDict::findObjectsByExactClass(const char* className, hkArray<hkDataObjectImpl*, hkContainerTempAllocator>& objectsOut) const
{
    m_tracker->getTrackedObjects(className, 0, 1, objectsOut);
}

// 0x00F47710 (:3562)
void hkDataWorldDict::findObjectsByBaseClass(const char* className, hkArray<hkDataObjectImpl*, hkContainerTempAllocator>& objectsOut) const
{
    m_tracker->getTrackedObjects(className, 1, 1, objectsOut);
}

// 0x00F3DBA0 (.h)
hkTypeManager& hkDataWorldDict::getTypeManager() { return m_typeManager; }
