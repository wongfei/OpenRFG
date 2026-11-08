// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/version/hkversionpatchmanager.cpp
//
// Havok 2014.2.5 (stub_impl.md S4, serializer SG), from rfg.exe (the decompile and, for the LTCG-garbled functions,
// the asm): the class version patch manager. Patches are registered at start-up (hkProductFeatures::initialize ->
// registerVersionPatches, compat/patches) and ordered by recomputePatchDependencies; every tagfile load runs
// applyPatches on its data world: preparePatches walks the world's classes (and the classes their patches depend
// on) to the patches that bring them to the current versions, rebuilds the source versions of the patched classes in
// a scratch dictionary world (undoing the patches in reverse), copies the classes the world lacks into it, and
// applyPatchesFinally then applies the patches in order. A class at its current version needs no patch; its class
// is wrapped from the builtin registry to check the version.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/version/hkversionpatchmanager.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/string/hkstring.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/router/hkmemoryrouter.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/error/hkerror.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/data/dict/hkdataobjectdict.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/data/hkdataobject.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/hkbuiltintyperegistry.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/util/hkserializemultimap.h"

// 0x03A38154
template<> hkVersionPatchManager* hkSingleton<hkVersionPatchManager>::s_instance = nullptr;

// 0x00F40530 (:29)
static hkReferencedObject* hkVersionPatchManagercreate()
{
    return new hkVersionPatchManager();
}

// 0x016FA26C (dynamic initializer 0x010980F0)
static hkSingletonInitNode hkSingletonRegisterhkVersionPatchManager("hkVersionPatchManager",
    hkVersionPatchManagercreate, (void**)&hkSingleton<hkVersionPatchManager>::s_instance);

// ---- hkDefaultClassWrapper

// 0x00F3DD40 (:42): the builtin class name registry unless one is given
hkDefaultClassWrapper::hkDefaultClassWrapper(const hkClassNameRegistry* nameReg) : m_nameReg(nullptr)
{
    if (nameReg == nullptr)
    {
        nameReg = hkBuiltinTypeRegistry::getInstance().getClassNameRegistry();
    }
    m_nameReg = nameReg;
}

// 0x00F3DC70 (:53): the world's class, else the native class wrapped into a dictionary world
hkDataClassImpl* hkDefaultClassWrapper::wrapClass(hkDataWorld* world, const char* typeName)
{
    hkDataClassImpl* k = world->findClass(typeName);
    if (k != nullptr)
    {
        return k;
    }
    const hkClass* klass = m_nameReg->getClassByName(typeName);
    if (klass != nullptr && world->getType() == hkDataWorld::TYPE_DICTIONARY)
    {
        return static_cast<hkDataWorldDict*>(world)->wrapClass(*klass);
    }
    return nullptr;
}

typedef hkSerializeMultiMap<int, int, hkPointerMap<int, int>> IntMultiMap;

// 0x00F3E810 (:92): depth-first order of the patches, each after the patches it depends on (incoming: patch index ->
// the indices that must run before it). order holds -1 unvisited, -2 in progress, else the position.
static int walkDependencies(int curIndex, hkArray<int>& order, const IntMultiMap& incoming, int counter,
    hkArray<int>& pending, const hkArray<const hkVersionPatchManager::PatchInfo*>& patchInfos)
{
    if (order[curIndex] != -1)
    {
        return counter;
    }
    order[curIndex] = -2;
    pending.pushBack(curIndex);
    for (int i = incoming.getFirstIndex(curIndex); i != -1; i = incoming.getNextIndex(i))
    {
        const int index = incoming.getValue(i);
        if (order[index] == -2)
        {
            // A cycle: rfg.exe appends the index to the pending list without counting it (the report that read the
            // list is compiled out).
            pending.pushBack(index);
            pending.popBack();
        }
        else if (order[index] == -1)
        {
            counter = walkDependencies(index, order, incoming, counter, pending, patchInfos);
        }
    }
    pending.popBack();
    order[curIndex] = counter;
    return counter + 1;
}

// ---- hkVersionPatchManager::UidFromClassVersion

// 0x00F3DF80 (:128): frees the interned names
hkVersionPatchManager::UidFromClassVersion::~UidFromClassVersion()
{
    for (hkStringMap<char*>::Iterator it = m_cachedNames.getIterator(); m_cachedNames.isValid(it);
         it = m_cachedNames.getNext(it))
    {
        hkMemoryRouter::easyFree(hkMemoryRouter::getInstance().heap(), m_cachedNames.getValue(it));
    }
}

// 0x00F3DD90 (:136)
uint64_t hkVersionPatchManager::UidFromClassVersion::get(const char* name, int ver) const
{
    const char* key = cache(name);
    const int index = m_indexFromName.getOrInsert(key, m_names.getSize());
    if (index == m_names.getSize())
    {
        m_names.pushBack(key);
    }
    return (uint64_t(unsigned(ver)) << 32) | unsigned(index);
}

// 0x00F3DCD0 (:148)
const char* hkVersionPatchManager::UidFromClassVersion::getName(uint64_t uid) const
{
    return m_names[int(uid)];
}

// 0x00F3DBF0 (:154)
int hkVersionPatchManager::UidFromClassVersion::getVersion(uint64_t uid) const
{
    return int(uid >> 32);
}

// 0x00F3DCE0 (:159): an owned copy of each name, so registered names compare by pointer
const char* hkVersionPatchManager::UidFromClassVersion::cache(const char* name) const
{
    if (name == nullptr)
    {
        return nullptr;
    }
    char* key = m_cachedNames.getWithDefault(name, nullptr);
    if (key == nullptr)
    {
        key = hkString::strDup(name);
        m_cachedNames.insert(key, key);
    }
    return key;
}

// ---- hkVersionPatchManager

// 0x00F3E910 (:174)
hkVersionPatchManager::hkVersionPatchManager() : m_section(0)
{
    m_uidFromClassVersion = new UidFromClassVersion();
}

// 0x00F3E9B0 (:179)
void hkVersionPatchManager::clearPatches()
{
    m_patchInfos.clear();
    m_patchIndexFromUid.clear();
    delete m_uidFromClassVersion;
    m_uidFromClassVersion = new UidFromClassVersion();
}

// 0x00F3E040 (:191): removes the patches whose new class name starts with the prefix (swapping with the last)
void hkVersionPatchManager::clearProductPatches(const char* productPrefix)
{
    const int len = hkString::strLen(productPrefix);
    for (int i = m_patchInfos.getSize() - 1; i >= 0; --i)
    {
        const char* name = m_patchInfos[i]->newName;
        if (name != nullptr && hkString::strNcmp(productPrefix, name, len) == 0)
        {
            m_patchInfos.removeAt(i);
        }
    }
    m_patchIndexFromUid.clear();
    delete m_uidFromClassVersion;
    m_uidFromClassVersion = new UidFromClassVersion();
}

// 0x00F3E130 (:209)
hkVersionPatchManager::~hkVersionPatchManager()
{
    delete m_uidFromClassVersion;
}

// 0x00F3DC00 (:214)
const hkArray<const hkVersionPatchManager::PatchInfo*>& hkVersionPatchManager::getPatches() const
{
    return m_patchInfos;
}

// 0x00F3DE00 (:219)
uint64_t hkVersionPatchManager::getUid(const char* name, int ver) const
{
    return m_uidFromClassVersion->get(name, ver);
}

// 0x00F3DD20 (:224)
const char* hkVersionPatchManager::getClassName(uint64_t uid) const
{
    return m_uidFromClassVersion->getName(uid);
}

// 0x00F3DC10 (:229)
int hkVersionPatchManager::getClassVersion(uint64_t uid) const
{
    return m_uidFromClassVersion->getVersion(uid);
}

// 0x00F3DC30 (:234): a class added (no old name, a new name and version), removed (an old name, no new name) or
// updated (an old name)
unsigned int isValidPatch(const hkVersionPatchManager::PatchInfo* patch)
{
    if (patch->oldVersion == -1)
    {
        if (patch->oldName != nullptr || patch->newVersion == -1 || patch->newName == nullptr)
        {
            return 0;
        }
    }
    else if (patch->newVersion == -1)
    {
        if (patch->oldName == nullptr || patch->newName != nullptr)
        {
            return 0;
        }
    }
    else if (patch->oldName == nullptr)
    {
        return 0;
    }
    return 1;
}

// 0x00F3DE10 (:278): follows the chain of patches from the uid; the last one, if the chain ends in a removal (or a
// rename, unless allowed), else -1
int hkVersionPatchManager::findLastPatchIndexForUid(uint64_t uid, unsigned int allowRenames) const
{
    int index = m_patchIndexFromUid.getWithDefault(uid, -1);
    while (index != -1)
    {
        const PatchInfo* p = m_patchInfos[index];
        if (p->newVersion == -1 || (!allowRenames && p->newName != nullptr && hkString::strCmp(p->oldName, p->newName) != 0))
        {
            break;
        }
        index = m_patchIndexFromUid.getWithDefault(m_uidFromClassVersion->get(p->newName, p->newVersion), -1);
    }
    return index;
}

// 0x00F3DEA0 (:296): whether indexToFind is reachable from startIndex in incoming
static unsigned int findPatchIndexInDependencies(int indexToFind, int startIndex, const IntMultiMap& incoming,
    hkPointerMap<int, int>& done)
{
    if (!done.hasKey(startIndex))
    {
        done.insert(startIndex, 0);
        for (int i = incoming.getFirstIndex(startIndex); i != -1; i = incoming.getNextIndex(i))
        {
            const int index = incoming.getValue(i);
            if (indexToFind == index || findPatchIndexInDependencies(indexToFind, index, incoming, done))
            {
                return 1;
            }
        }
    }
    return 0;
}

// 0x00F3EA50 (:317): validates the patches and sorts them so that every patch runs after the patches producing its
// source class version and its dependencies, and after the patches of a class it renames or removes the name of;
// rebuilds the uid -> patch index map (by old name and version).
hkResult hkVersionPatchManager::recomputePatchDependencies() const
{
    UidFromClassVersion& uids = *m_uidFromClassVersion;
    hkPointerMap<uint64_t, int> addedIndexFromUid;
    hkSerializeMultiMap<uint64_t, int, hkPointerMap<uint64_t, int>> producersFromUid;
    hkSerializeMultiMap<const char*, int, hkPointerMap<const char*, int>> renamesFromOldName;
    m_patchIndexFromUid.clear();
    for (int i = 0; i < m_patchInfos.getSize(); ++i)
    {
        const PatchInfo* p = m_patchInfos[i];
        if (!isValidPatch(p))
        {
            return HK_FAILURE;
        }
        if (p->oldVersion == -1)
        {
            const uint64_t uid = uids.get(p->newName, p->newVersion);
            if (addedIndexFromUid.hasKey(uid))
            {
                return HK_FAILURE;
            }
            addedIndexFromUid.insert(uid, i);
        }
        else
        {
            const uint64_t uid = uids.get(p->oldName, p->oldVersion);
            if (m_patchIndexFromUid.hasKey(uid))
            {
                return HK_FAILURE;
            }
            m_patchIndexFromUid.insert(uid, i);
            if (p->newVersion == -1 || (p->newName != nullptr && hkString::strCmp(p->oldName, p->newName) != 0))
            {
                renamesFromOldName.insert(p->oldName, i);
            }
        }
        if (p->newVersion != -1)
        {
            producersFromUid.insert(uids.get(p->newName != nullptr ? p->newName : p->oldName, p->newVersion), i);
        }
    }

    hkStringMap<int> removedIndexFromName;
    IntMultiMap incoming;
    IntMultiMap renameEdges;
    for (int i = 0; i < m_patchInfos.getSize(); ++i)
    {
        const PatchInfo* p = m_patchInfos[i];
        const char* name = p->oldName;
        int version = p->oldVersion;
        if (version == -1)
        {
            name = p->newName;
            version = p->newVersion;
            if (name == nullptr || version == -1)
            {
                return HK_FAILURE;
            }
        }
        const uint64_t uid = uids.get(name, version);
        int patchIndex;
        if (p->oldVersion == -1)
        {
            patchIndex = addedIndexFromUid.getWithDefault(uid, -1);
        }
        else
        {
            patchIndex = m_patchIndexFromUid.getWithDefault(uid, -1);
            for (int k = producersFromUid.getFirstIndex(uid); k != -1; k = producersFromUid.getNextIndex(k))
            {
                incoming.insert(patchIndex, producersFromUid.getValue(k));
            }
        }
        for (int c = 0; c < p->numComponent; ++c)
        {
            if (p->component[c].type == PATCH_DEPENDS)
            {
                const DependsPatch* d = static_cast<const DependsPatch*>(p->component[c].patch);
                const uint64_t dependUid = uids.get(d->name, d->version);
                const int dependIndex = m_patchIndexFromUid.getWithDefault(dependUid, -1);
                if (dependIndex != -1)
                {
                    incoming.insert(dependIndex, patchIndex);
                }
                for (int k = producersFromUid.getFirstIndex(dependUid); k != -1; k = producersFromUid.getNextIndex(k))
                {
                    incoming.insert(patchIndex, producersFromUid.getValue(k));
                }
            }
        }
        // A class added under a name that other patches rename or remove: it goes after them (or, when the old
        // class's chain does not end in a rename or removal, keeps them apart unless already ordered).
        if (p->oldVersion == -1 && renamesFromOldName.getFirstIndex(name) != -1 && renamesFromOldName.getFirstIndex(name) != -1)
        {
            const int last = findLastPatchIndexForUid(uid, false);
            IntMultiMap* edges;
            if (last == -1)
            {
                if (removedIndexFromName.getWithDefault(name, -1) != -1)
                {
                    return HK_FAILURE;
                }
                removedIndexFromName.insert(name, i);
                edges = &incoming;
            }
            else
            {
                edges = &renameEdges;
            }
            for (int k = renamesFromOldName.getFirstIndex(name); k != -1; k = renamesFromOldName.getNextIndex(k))
            {
                const int renameIndex = renamesFromOldName.getValue(k);
                if (last != renameIndex)
                {
                    edges->insert(patchIndex, renameIndex);
                }
            }
        }
    }
    for (hkPointerMap<int, int>::Iterator it = renameEdges.m_indexMap.getIterator(); renameEdges.m_indexMap.isValid(it);
         it = renameEdges.m_indexMap.getNext(it))
    {
        const int key = renameEdges.m_indexMap.getKey(it);
        for (int k = renameEdges.getFirstIndex(key); k != -1; k = renameEdges.getNextIndex(k))
        {
            hkPointerMap<int, int> done;
            const int value = renameEdges.getValue(k);
            if (!findPatchIndexInDependencies(key, value, incoming, done))
            {
                incoming.insert(key, value);
            }
        }
    }

    hkArray<const PatchInfo*> oldPatchInfos;
    oldPatchInfos.swap(m_patchInfos);
    const int numPatches = oldPatchInfos.getSize();
    hkArray<int> order;
    order.setSize(numPatches, -1);
    hkArray<int> pending;
    int counter = 0;
    for (int i = 0; i < numPatches; ++i)
    {
        counter = walkDependencies(i, order, incoming, counter, pending, oldPatchInfos);
    }
    m_patchInfos.setSize(numPatches);
    for (int i = 0; i < numPatches; ++i)
    {
        if (order[i] < 0)
        {
            m_patchInfos.swap(oldPatchInfos);
            return HK_FAILURE;
        }
        m_patchInfos[order[i]] = oldPatchInfos[i];
    }
    m_patchIndexFromUid.clear();
    for (int i = 0; i < m_patchInfos.getSize(); ++i)
    {
        const PatchInfo* p = m_patchInfos[i];
        if (p->oldName != nullptr)
        {
            m_patchIndexFromUid.insert(uids.get(p->oldName, p->oldVersion), i);
        }
    }
    return HK_SUCCESS;
}

// size 0x10: a class version to visit, with the patch that updates it (-1: none)
struct UidItem
{
    bool operator<(UidItem p) const { return patchIndex < p.patchIndex; } // 0x00F40560 (:599)
    bool operator==(UidItem p) const { return uid == p.uid; } // 0x00F40570 (:600)

    uint64_t uid; // 0x00
    int patchIndex; // 0x08
};

// The visit list, kept sorted by patch index (the patches in their dependency order, the unpatched classes first).
template<typename T>
struct HeapArray
{
    // 0x00F428E0 (:554): before the first item with a greater patch index
    void insert(const T& e)
    {
        for (int i = 0; i < m_elems.getSize(); ++i)
        {
            if (e < m_elems[i])
            {
                m_elems.insertAt(i, e);
                return;
            }
        }
        m_elems.pushBack(e);
    }
    const T& top() const { return m_elems[0]; } // 0x00F41070 (:566)
    int getSize() const { return m_elems.getSize(); } // 0x00F41080 (:570)
    // 0x00F418A0 (:579)
    void popTop()
    {
        m_elems.popBack();
        hkString::memMove(m_elems.begin(), m_elems.begin() + 1, m_elems.getSize() * int(sizeof(T)));
    }
    // 0x00F41090 (:583)
    bool hasElement(const T& e)
    {
        for (int i = 0; i < m_elems.getSize(); ++i)
        {
            if (m_elems[i] == e)
            {
                return true;
            }
        }
        return false;
    }

    hkArray<T> m_elems;
};

// 0x00F3F8C0 (:606): queues the class version with the patch that updates it, then (recursively) the classes the
// patch depends on. A class the world holds at that version is updated by the patch from the world's version; one it
// lacks (or holds at another version) by a class-added patch unless that one is done, else by the regular patch.
// patchesDone: 1 a class-added patch applied, 2 done.
static void setAllDependeciesToDo(HeapArray<UidItem>& todo, const char* classname, int version,
    const hkDataWorld& worldToUpdate, const hkVersionPatchManager& man,
    const hkPointerMap<uint64_t, int>& idxNewSrcFromUid, const hkPointerMap<uint64_t, int>& patchesDone)
{
    const uint64_t uid = man.getUid(classname, version);
    int patchIndex = -1;
    hkDataClassImpl* klass = worldToUpdate.findClass(classname);
    if (klass == nullptr || klass->getVersion() != version)
    {
        patchIndex = idxNewSrcFromUid.getWithDefault(uid, -1);
    }
    const int done = patchesDone.getWithDefault(uid, 0);
    if (patchIndex == -1 || done == 1)
    {
        patchIndex = man.m_patchIndexFromUid.getWithDefault(uid, -1);
    }
    if (done != 2)
    {
        UidItem item;
        item.uid = uid;
        item.patchIndex = patchIndex;
        if (!todo.hasElement(item))
        {
            todo.insert(item);
        }
    }
    if (patchIndex >= 0)
    {
        const hkVersionPatchManager::PatchInfo* p = man.m_patchInfos[patchIndex];
        for (int c = p->numComponent - 1; c >= 0; --c)
        {
            if (p->component[c].type == hkVersionPatchManager::PATCH_DEPENDS)
            {
                const hkVersionPatchManager::DependsPatch* d =
                    static_cast<const hkVersionPatchManager::DependsPatch*>(p->component[c].patch);
                setAllDependeciesToDo(todo, d->name, d->version, worldToUpdate, man, idxNewSrcFromUid, patchesDone);
            }
        }
    }
}

namespace
{
    struct ClassAndVersion
    {
        const char* name;
        int version;
    };
}

// 0x00F3F9D0 (:737)
hkResult hkVersionPatchManager::preparePatches(hkDataWorld& worldToUpdate, ClassWrapper* wrapper,
    hkArray<const PatchInfo*>& patchInfosOut) const
{
    hkArray<hkDataClassImpl*, hkContainerTempAllocator> worldClasses;
    worldToUpdate.findAllClasses(worldClasses);
    hkArray<ClassAndVersion> classVersions;
    classVersions.reserve(worldClasses.getSize());
    for (int i = 0; i < worldClasses.getSize(); ++i)
    {
        hkDataClassImpl* c = worldClasses[i];
        if (c->getVersion() >= 0)
        {
            ClassAndVersion cv;
            cv.name = c->getName();
            cv.version = c->getVersion();
            classVersions.pushBackUnchecked(cv);
        }
    }

    hkDataWorldDict patchWorld;
    hkPointerMap<uint64_t, int> idxNewSrcFromUid;
    for (int i = 0; i < m_patchInfos.getSize(); ++i)
    {
        const PatchInfo* p = m_patchInfos[i];
        if (!isValidPatch(p))
        {
            return HK_FAILURE;
        }
        if (p->oldVersion == -1)
        {
            const uint64_t uid = m_uidFromClassVersion->get(p->newName, p->newVersion);
            if (idxNewSrcFromUid.hasKey(uid))
            {
                return HK_FAILURE;
            }
            idxNewSrcFromUid.insert(uid, i);
        }
    }

    hkPointerMap<uint64_t, int> patchesDone;
    patchInfosOut.clear();
    {
        HeapArray<UidItem> todo;
        for (int i = 0; i < classVersions.getSize(); ++i)
        {
            setAllDependeciesToDo(todo, classVersions[i].name, classVersions[i].version, worldToUpdate, *this,
                idxNewSrcFromUid, patchesDone);
        }
        while (todo.getSize() != 0)
        {
            const UidItem item = todo.top();
            todo.popTop();
            const int patchIndex = item.patchIndex;
            patchesDone.insert(item.uid, (patchIndex != -1 && m_patchInfos[patchIndex]->oldVersion == -1) ? 1 : 2);
            if (patchIndex >= 0)
            {
                const PatchInfo* p = m_patchInfos[patchIndex];
                patchInfosOut.pushBack(p);
                setAllDependeciesToDo(todo, p->newName != nullptr ? p->newName : p->oldName, p->newVersion,
                    worldToUpdate, *this, idxNewSrcFromUid, patchesDone);
            }
            else
            {
                // No patch: the class must be registered at that version
                const char* name = getClassName(item.uid);
                const int version = getClassVersion(item.uid);
                if (version != -1)
                {
                    hkDataClassImpl* klass = wrapper->wrapClass(&patchWorld, name);
                    if (klass == nullptr)
                    {
                        char buf[512];
                        hkErrStream os(buf, sizeof(buf));
                        os << "Class " << name << " is not registered. If this is a Havok class, make sure the class's "
                           "product reflection is enabled near where hkProductFeatures.cxx is included. Otherwise, check "
                           "your own class registration.";
                        hkErrorFwd::messageWarning(0x3f79ddb0, buf, "Version\\hkVersionPatchManager.cpp", 0x346);
                        return HK_FAILURE;
                    }
                    if (klass->getVersion() != version)
                    {
                        char buf[512];
                        hkErrStream os(buf, sizeof(buf));
                        os << "Source contains " << name << " version " << version << ", but  " << klass->getVersion()
                           << " is the current version.\n" << "Make sure required patches are registered to update this class.";
                        hkErrorFwd::messageWarning(0x3f79ddb1, buf, "Version\\hkVersionPatchManager.cpp", 0x34d);
                        return HK_FAILURE;
                    }
                }
            }
        }
    }

    // The source versions of the patched classes: undo the patches, last first, on the wrapped current classes
    for (int i = patchInfosOut.getSize() - 1; i >= 0; --i)
    {
        const PatchInfo* p = patchInfosOut[i];
        const char* oldName = p->oldName;
        const char* name = p->newName != nullptr ? p->newName : oldName;
        hkDataClass klass(patchWorld.findClass(name));
        if (klass.isNull())
        {
            hkDataClass::Cinfo cinfo;
            cinfo.name = name;
            cinfo.version = p->oldVersion;
            cinfo.parent = nullptr;
            klass = hkDataClass(patchWorld.newClass(cinfo));
        }
        for (int c = p->numComponent - 1; c >= 0; --c)
        {
            const void* patch = p->component[c].patch;
            switch (p->component[c].type)
            {
                case PATCH_MEMBER_ADDED:
                    patchWorld.removeClassMember(klass, static_cast<const MemberAddedPatch*>(patch)->name);
                    break;
                case PATCH_MEMBER_REMOVED:
                {
                    const MemberRemovedPatch* m = static_cast<const MemberRemovedPatch*>(patch);
                    hkTypeManager::Type* type = patchWorld.m_typeManager.getType(m->type, m->typeName, m->tuples);
                    patchWorld.addClassMember(klass, m->name, type, nullptr);
                    break;
                }
                case PATCH_MEMBER_RENAMED:
                {
                    const MemberRenamedPatch* m = static_cast<const MemberRenamedPatch*>(patch);
                    patchWorld.renameClassMember(klass, m->newName, m->oldName);
                    break;
                }
                case PATCH_DEPENDS:
                    patchWorld.findClass(static_cast<const DependsPatch*>(patch)->name);
                    break;
                case PATCH_PARENT_SET:
                {
                    const SetParentPatch* s = static_cast<const SetParentPatch*>(patch);
                    hkDataClass parent(s->oldParent != nullptr ? patchWorld.findClass(s->oldParent) : nullptr);
                    patchWorld.setClassParent(klass, parent);
                    break;
                }
                default:
                    break;
            }
        }
        if (p->oldVersion == -1)
        {
            patchWorld.removeClass(klass);
        }
        else
        {
            if (name != oldName && hkString::strCmp(name, oldName) != 0)
            {
                patchWorld.renameClass(klass, oldName);
            }
            patchWorld.setClassVersion(klass, p->oldVersion);
        }
    }

    // Copy the classes the world lacks
    hkArray<hkDataClassImpl*, hkContainerTempAllocator> patchClasses;
    patchWorld.findAllClasses(patchClasses);
    hkStringMap<hkDataClassImpl*> classFromName;
    hkStringMap<hkDataClassImpl*> unused; // constructed and destroyed unused in rfg.exe
    for (int i = 0; i < patchClasses.getSize(); ++i)
    {
        classFromName.insert(patchClasses[i]->getName(), patchClasses[i]);
    }
    for (int i = 0; i < worldClasses.getSize(); ++i)
    {
        hkDataClassImpl* c = worldClasses[i];
        if (classFromName.getWithDefault(c->getName(), nullptr) == nullptr)
        {
            c->getVersion(); // the result is unused (an assert compiled out)
        }
        else
        {
            classFromName.remove(c->getName());
        }
    }
    for (hkStringMap<hkDataClassImpl*>::Iterator it = classFromName.getIterator(); classFromName.isValid(it);
         it = classFromName.getNext(it))
    {
        worldToUpdate.copyClassFromWorld(classFromName.getValue(it)->getName(), patchWorld);
    }
    return HK_SUCCESS;
}

hkResult applyPatchesFinally(hkDataWorld& worldToUpdate, const hkArray<const hkVersionPatchManager::PatchInfo*>& patchInfos);

// 0x00F40440 (:1021)
hkResult hkVersionPatchManager::applyPatches(hkDataWorld& worldToUpdate, ClassWrapper* classWrapper) const
{
    hkDefaultClassWrapper defaultWrapper(nullptr);
    ClassWrapper* wrapper = classWrapper != nullptr ? classWrapper : &defaultWrapper;
    hkArray<const PatchInfo*> patchInfos;
    m_section.enter();
    hkResult result = preparePatches(worldToUpdate, wrapper, patchInfos);
    if (result == HK_SUCCESS)
    {
        result = applyPatchesFinally(worldToUpdate, patchInfos);
    }
    m_section.leave();
    return result;
}

// 0x00F40510 (:1047): every registered patch, in order
hkResult hkVersionPatchManager::applyPatchesDebug(hkDataWorld& worldToUpdate) const
{
    return applyPatchesFinally(worldToUpdate, m_patchInfos);
}

// 0x00F3E1D0 (:1054): applies the patches in order. Function patches run on every object of the class or a subclass;
// a parent change first destroys the old parent's members on them; a cast changes the exact class's objects.
hkResult applyPatchesFinally(hkDataWorld& worldToUpdate, const hkArray<const hkVersionPatchManager::PatchInfo*>& patchInfos)
{
    typedef hkVersionPatchManager Man;
    for (int pi = 0; pi < patchInfos.getSize(); ++pi)
    {
        const Man::PatchInfo* p = patchInfos[pi];
        const char* className = p->oldName != nullptr ? p->oldName : p->newName;
        if (p->oldVersion == -1)
        {
            hkDataClass::Cinfo cinfo;
            cinfo.name = className;
            cinfo.version = p->newVersion;
            cinfo.parent = nullptr;
            if (worldToUpdate.findClass(className) != nullptr)
            {
                return HK_FAILURE;
            }
            worldToUpdate.newClass(cinfo);
        }
        hkDataClass klass(worldToUpdate.findClass(className));
        if (p->oldName != nullptr && p->newName != nullptr && hkString::strCmp(p->oldName, p->newName) != 0)
        {
            worldToUpdate.renameClass(klass, p->newName);
            className = p->newName;
        }
        for (int c = 0; c < p->numComponent; ++c)
        {
            const void* patch = p->component[c].patch;
            switch (p->component[c].type)
            {
                case Man::PATCH_MEMBER_ADDED:
                {
                    const Man::MemberAddedPatch* m = static_cast<const Man::MemberAddedPatch*>(patch);
                    hkTypeManager::Type* type = worldToUpdate.getTypeManager().getType(m->type, m->typeName, m->tuples);
                    worldToUpdate.addClassMember(klass, m->name, type, m->defaultPtr);
                    break;
                }
                case Man::PATCH_MEMBER_REMOVED:
                    worldToUpdate.removeClassMember(klass, static_cast<const Man::MemberRemovedPatch*>(patch)->name);
                    break;
                case Man::PATCH_MEMBER_RENAMED:
                {
                    const Man::MemberRenamedPatch* m = static_cast<const Man::MemberRenamedPatch*>(patch);
                    worldToUpdate.renameClassMember(klass, m->oldName, m->newName);
                    break;
                }
                case Man::PATCH_MEMBER_DEFAULT_SET:
                {
                    const Man::DefaultChangedPatch* m = static_cast<const Man::DefaultChangedPatch*>(patch);
                    worldToUpdate.setClassMemberDefault(klass, m->name, m->defaultPtr);
                    break;
                }
                case Man::PATCH_FUNCTION:
                {
                    Man::Function function = static_cast<const Man::FunctionPatch*>(patch)->function;
                    hkArray<hkDataObjectImpl*, hkContainerTempAllocator> objects;
                    worldToUpdate.findObjectsByBaseClass(className, objects);
                    for (int i = 0; i < objects.getSize(); ++i)
                    {
                        hkDataObject obj(objects[i]);
                        function(obj);
                    }
                    break;
                }
                case Man::PATCH_CAST:
                {
                    hkDataClass castClass(worldToUpdate.findClass(static_cast<const Man::CastPatch*>(patch)->name));
                    hkArray<hkDataObjectImpl*, hkContainerTempAllocator> objects;
                    worldToUpdate.findObjectsByExactClass(className, objects);
                    for (int i = 0; i < objects.getSize(); ++i)
                    {
                        hkDataObject obj(objects[i]);
                        worldToUpdate.castObject(obj, castClass);
                    }
                    break;
                }
                case Man::PATCH_PARENT_SET:
                {
                    const Man::SetParentPatch* s = static_cast<const Man::SetParentPatch*>(patch);
                    if (s->oldParent != nullptr)
                    {
                        hkDataClassImpl* oldParent = worldToUpdate.findClass(s->oldParent);
                        hkArray<hkDataClass_MemberInfo> members;
                        members.setSize(oldParent->getNumMembers(), hkDataClass_MemberInfo());
                        oldParent->getAllMemberInfo(members);
                        hkArray<hkDataObjectImpl*, hkContainerTempAllocator> objects;
                        worldToUpdate.findObjectsByBaseClass(className, objects);
                        for (int i = 0; i < objects.getSize(); ++i)
                        {
                            hkDataObject obj(objects[i]);
                            for (int m = 0; m < members.getSize(); ++m)
                            {
                                obj.destroyMember(members[m].m_name);
                            }
                        }
                    }
                    hkDataClass newParent(s->newParent != nullptr ? worldToUpdate.findClass(s->newParent) : nullptr);
                    worldToUpdate.setClassParent(klass, newParent);
                    break;
                }
                default: // PATCH_DEPENDS
                    break;
            }
        }
        if (p->newVersion == -1)
        {
            worldToUpdate.removeClass(klass);
        }
        else if (p->oldVersion != -1 && p->oldVersion != p->newVersion)
        {
            worldToUpdate.setClassVersion(klass, p->newVersion);
        }
    }
    return HK_SUCCESS;
}

// 0x00F3DF20 (:1236): a valid patch is appended (the order is recomputed by recomputePatchDependencies)
void hkVersionPatchManager::addPatch(const PatchInfo* p)
{
    if (isValidPatch(p))
    {
        m_patchInfos.pushBack(p);
        m_patchIndexFromUid.clear();
    }
}
