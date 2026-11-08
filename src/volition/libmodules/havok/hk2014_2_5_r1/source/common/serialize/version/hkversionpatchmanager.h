// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/version/hkversionpatchmanager.h
// Implementation: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/version/hkversionpatchmanager.cpp
//
// Havok 2014.2.5: full declaration from rfg.pdb (stub_impl.md S4, serializer).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/typemanager/hktypemanager.h"
#include <stdint.h>
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/pointermap/hkpointermap.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/stringmap/hkstringmap.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hkreferencedobject.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hksingleton.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/registry/hkclassnameregistry.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/thread/criticalsection/hkcriticalsection.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkrefptr.h"

class hkDataClassImpl;
class hkDataObject;
class hkDataWorld;
struct hkResult;

// size 0x3C
class hkVersionPatchManager : public hkReferencedObject, public hkSingleton<hkVersionPatchManager>
{
public:
    enum PatchType
    {
        PATCH_INVALID = 0,
        PATCH_MEMBER_ADDED = 1,
        PATCH_MEMBER_REMOVED = 2,
        PATCH_MEMBER_RENAMED = 3,
        PATCH_MEMBER_DEFAULT_SET = 4,
        PATCH_FUNCTION = 5,
        PATCH_CAST = 6,
        PATCH_DEPENDS = 7,
        PATCH_PARENT_SET = 8,
        PATCH_END = 9,
    };

    typedef void (*Function)(hkDataObject&);
    typedef hkStringMap<hkDataClassImpl*,hkContainerHeapAllocator> ClassImplFromName;

    // size 0x18
    struct PatchInfo
    {
        // size 0x8
        struct Component
        {
            HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(Component);

            PatchType type; // 0x00
            const void* patch; // 0x04
        };

        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(PatchInfo);

        const char* oldName; // 0x00
        const char* newName; // 0x04
        int oldVersion; // 0x08
        int newVersion; // 0x0C
        const Component* component; // 0x10
        int numComponent; // 0x14
    };

    // size 0x8
    struct DependsPatch
    {
        enum
        {
            PATCH_TYPE = 7,
        };

        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(DependsPatch);

        const char* name; // 0x00
        int version; // 0x04
    };

    // size 0x8
    struct MemberRenamedPatch
    {
        enum
        {
            PATCH_TYPE = 3,
        };

        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(MemberRenamedPatch);

        const char* oldName; // 0x00
        const char* newName; // 0x04
    };

    // size 0x14
    struct MemberAddedPatch
    {
        enum
        {
            PATCH_TYPE = 1,
        };

        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(MemberAddedPatch);

        const char* name; // 0x00
        hkLegacyType::Type type; // 0x04
        const char* typeName; // 0x08
        int tuples; // 0x0C
        const void* defaultPtr; // 0x10
    };

    // size 0x10
    struct MemberRemovedPatch
    {
        enum
        {
            PATCH_TYPE = 2,
        };

        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(MemberRemovedPatch);

        const char* name; // 0x00
        hkLegacyType::Type type; // 0x04
        const char* typeName; // 0x08
        int tuples; // 0x0C
    };

    // size 0x8
    struct SetParentPatch
    {
        enum
        {
            PATCH_TYPE = 8,
        };

        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(SetParentPatch);

        const char* oldParent; // 0x00
        const char* newParent; // 0x04
    };

    // size 0x8
    struct FunctionPatch
    {
        enum
        {
            PATCH_TYPE = 5,
        };

        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(FunctionPatch);

        const char* name; // 0x00
        void (*function)(hkDataObject&); // 0x04
    };

    // size 0x4
    struct CastPatch
    {
        enum
        {
            PATCH_TYPE = 6,
        };

        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(CastPatch);

        const char* name; // 0x00
    };

    // size 0x8
    struct DefaultChangedPatch
    {
        enum
        {
            PATCH_TYPE = 4,
        };

        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(DefaultChangedPatch);

        const char* name; // 0x00
        const void* defaultPtr; // 0x04
    };

    // size 0x8
    class ClassWrapper : public hkReferencedObject
    {
    public:
        HK_DECLARE_CLASS_ALLOCATOR(); // delete(void* p, unsigned int nbytes) 0x00F40E60
        virtual ~ClassWrapper() {} // vtbl+0x00 0x00F3DBC0 (implicit)
        ClassWrapper() {} // 0x00F3DBB0 (implicit)
        virtual hkDataClassImpl* wrapClass(hkDataWorld*, const char*) = 0; // vtbl+0x10

    };

    // size 0x2C: class names interned to indices; a uid is (version << 32) | name index
    class UidFromClassVersion
    {
    public:
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(UidFromClassVersion); // new(unsigned int nbytes) 0x00F40EA0, delete(void* p, unsigned int nbytes) 0x00F40EC0
        ~UidFromClassVersion(); // 0x00F3DF80
        uint64_t get(const char* name, int ver) const; // 0x00F3DD90
        const char* getName(uint64_t uid) const; // 0x00F3DCD0
        int getVersion(uint64_t uid) const; // 0x00F3DBF0
    public:
        const char* cache(const char* name) const; // 0x00F3DCE0
    public:
        UidFromClassVersion() {} // 0x00F42630

    public:
        mutable hkStringMap<int,hkContainerHeapAllocator> m_indexFromName; // 0x00
        mutable hkArray<char const*,hkContainerHeapAllocator> m_names; // 0x10
        mutable hkStringMap<char*,hkContainerHeapAllocator> m_cachedNames; // 0x1C
    };

    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int nbytes) 0x00F40E40, delete(void* p, unsigned int nbytes) 0x00F426B0
    hkVersionPatchManager(); // 0x00F3E910
    virtual ~hkVersionPatchManager(); // vtbl+0x00 0x00F3E130
    hkResult applyPatches(hkDataWorld& worldToUpdate, ClassWrapper* classWrapper) const; // 0x00F40440
    void addPatch(const PatchInfo* p); // 0x00F3DF20
    hkResult recomputePatchDependencies() const; // 0x00F3EA50
    hkResult applyPatchesDebug(hkDataWorld& worldToUpdate) const; // 0x00F40510
    const hkArray<PatchInfo const*,hkContainerHeapAllocator>& getPatches() const; // 0x00F3DC00
    uint64_t getUid(const char* name, int ver) const; // 0x00F3DE00
    const char* getClassName(uint64_t uid) const; // 0x00F3DD20
    int getClassVersion(uint64_t uid) const; // 0x00F3DC10
    int getPatchIndex(uint64_t uid) const { return m_patchIndexFromUid.getWithDefault(uid, -1); } // 0x00F41DB0 (.inl)
    const PatchInfo* getPatch(int patchIndex) const { return m_patchInfos[patchIndex]; } // 0x00F40EF0 (.inl)
    int getNumPatches() const { return m_patchInfos.getSize(); } // 0x008197A0 (hkversionpatchmanager.inl:20)
    int findLastPatchIndexForUid(uint64_t uid, unsigned int allowRenames = false) const; // 0x00F3DE10
    void clearPatches(); // 0x00F3E9B0
    void clearProductPatches(const char* productPrefix); // 0x00F3E040
private:
    hkResult preparePatches(hkDataWorld& worldToUpdate, ClassWrapper* wrapper, hkArray<PatchInfo const*,hkContainerHeapAllocator>& patchInfosOut) const; // 0x00F3F9D0

public:
    UidFromClassVersion* m_uidFromClassVersion; // 0x08
    mutable hkArray<PatchInfo const*,hkContainerHeapAllocator> m_patchInfos; // 0x0C
    mutable hkPointerMap<uint64_t,int,hkContainerHeapAllocator> m_patchIndexFromUid; // 0x18
    mutable hkCriticalSection m_section; // 0x24
};

// size 0xC
class hkDefaultClassWrapper : public hkVersionPatchManager::ClassWrapper
{
public:
    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int nbytes) 0x00FA1580, delete(void* p, unsigned int nbytes) 0x00F416D0
    // vtbl+0x00 0x00F41710 (implicit; hkRefPtr has no destructor in this port)
    virtual ~hkDefaultClassWrapper()
    {
        m_nameReg = nullptr;
    }
    hkDefaultClassWrapper(const hkClassNameRegistry* nameReg); // 0x00F3DD40
    virtual hkDataClassImpl* wrapClass(hkDataWorld* world, const char* typeName); // vtbl+0x10 0x00F3DC70

public:
    hkRefPtr<hkClassNameRegistry const > m_nameReg; // 0x08
};

unsigned int isValidPatch(const hkVersionPatchManager::PatchInfo* patch); // 0x00F3DC30
