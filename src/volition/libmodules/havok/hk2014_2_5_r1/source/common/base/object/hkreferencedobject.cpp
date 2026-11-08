// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hkreferencedobject.cpp
//
// Havok 2014.2.5 (havok_impl.md HK1): hkBaseObject and hkReferencedObject (the out-of-line copies of their inline
// constructors and destructors too), and the global reference lock: hkReferencedObjectLock, a singleton with a
// critical section that lockAll/unlockAll take recursively per thread (the thread's router remembers it holds the
// lock by the magic value 0x23DF4554 in its ref object store).
//
// OpenRFG addition: hkReferencedObjectTracker (see hkreferencedobject.h), called by the constructors and the
// destructor.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hkreferencedobject.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/debugutil/multithreadcheck/hkmultithreadcheck.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/router/hkmemoryrouter.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hksingleton.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/thread/criticalsection/hkcriticalsection.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/hktypeinfo.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"

#include <new>

namespace
{
const unsigned int LOCK_OWNER_MAGIC = 0x23DF4554; // the router's ref object store while the thread holds the lock
}

// size 0x34
// The global reference lock (file-local in the SDK).
class hkReferencedObjectLock : public hkReferencedObject, public hkSingleton<hkReferencedObjectLock>
{
public:
    typedef unsigned int ThreadId;

    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int nbytes) 0x00CE74D0, delete(void* p, unsigned int nbytes) 0x00CE7560
    hkReferencedObjectLock(); // 0x00CE74F0
    void lock(); // 0x00CE75A0
    void unlock(); // 0x00CE75E0
    virtual ~hkReferencedObjectLock(); // vtbl+0x00 0x00CE7540

    hkReferencedObject::LockMode m_lockMode; // 0x08
    int m_lockCount; // 0x0C
    hkMultiThreadCheck m_multiThreadCheck; // 0x10
    hkCriticalSection m_criticalSection; // 0x1C
};
static_assert(sizeof(hkReferencedObjectLock) == 0x34, "hkReferencedObjectLock size");

// ---- globals
template<> hkReferencedObjectLock* hkSingleton<hkReferencedObjectLock>::s_instance = nullptr;
void (*hkReferencedObjectTracker)(const hkReferencedObject* object, bool alive);

// ---- hkBaseObject
// 0x00710540 (hkbaseobject.h:29)
hkBaseObject::~hkBaseObject()
{
}

// 0x00CE7460 (hkbaseobject.h:33)
hkBaseObject::hkBaseObject(hkFinishLoadedObjectFlag flag)
{
}

// 0x00710560 (hkbaseobject.h:38)
void hkBaseObject::__first_virtual_table_function__()
{
}

// ---- hkBaseObject's type info (by hand in Havok, not generated: loading an object never constructs an hkBaseObject
// over it, so the finish and cleanup functions do nothing)

// 0x00CE7180 (:12)
static void finishLoadedObjecthkBaseObject(void*, int)
{
}

// 0x00CE7190 (:15)
static void cleanupLoadedObjecthkBaseObject(void*)
{
}

// 0x00CE71C0 (:18)
static const void* getVtablehkBaseObject()
{
    union
    {
        alignas(16) void* ptr;
        char buf[sizeof(hkBaseObject)];
    } u;
    hkFinishLoadedObjectFlag f;
    ::new (u.buf) hkBaseObject(f);
    return u.ptr;
}

// 0x016F2A00 (the vtable written by its dynamic initializer). The size is rfg.exe's 8, not sizeof(hkBaseObject).
extern const hkTypeInfo hkBaseObjectTypeInfo;
const hkTypeInfo hkBaseObjectTypeInfo("hkBaseObject", "!hkBaseObject", finishLoadedObjecthkBaseObject,
                                      cleanupLoadedObjecthkBaseObject, getVtablehkBaseObject(), 8);

// ---- hkReferencedObject
// 0x007105C0 (hkreferencedobject.inl:11)
hkReferencedObject::hkReferencedObject()
    : m_memSizeAndRefCount(0xFFFF0001) // memory size AUTO_MEMSIZE, one reference
{
    if (hkReferencedObjectTracker != nullptr)
    {
        hkReferencedObjectTracker(this, true);
    }
}

// 0x00B62A30
// A copy is a new object: its own count and size.
hkReferencedObject::hkReferencedObject(const hkReferencedObject& originalObject)
    : m_memSizeAndRefCount(0xFFFF0001)
{
    if (hkReferencedObjectTracker != nullptr)
    {
        hkReferencedObjectTracker(this, true);
    }
}

// 0x00CE8FD0 (hkreferencedobject.h:38)
// Loaded in place: the word was loaded with the object (memory size 0 from a packfile, the allocation size from a
// tagfile heap load).
hkReferencedObject::hkReferencedObject(hkFinishLoadedObjectFlag flag)
{
    if (hkReferencedObjectTracker != nullptr && getMemorySizeAndFlags() != 0)
    {
        hkReferencedObjectTracker(this, true);
    }
}

// 0x007105B0 (hkreferencedobject.h:44)
hkReferencedObject::~hkReferencedObject()
{
    // every object is reported, whatever its memory size: a member object counted when constructed gets memory size 0
    // afterwards (hkpSimpleConstraintContactMgr's constraint and data), and the tracker ignores what it never counted
    if (hkReferencedObjectTracker != nullptr)
    {
        hkReferencedObjectTracker(this, false);
    }
}

// 0x00CE71A0 (:34)
const hkClass* hkReferencedObject::getClassType() const
{
    return nullptr;
}

// 0x00CE71B0 (:162)
void hkReferencedObject::deleteThisReferencedObject() const
{
    delete this;
}

// 0x00CE7260 (:167)
void hkReferencedObject::addReferences(const hkReferencedObject* const* objects, int numObjects, int pointerStriding)
{
    for (int i = 0; i < numObjects; i++)
    {
        (*objects)->addReference();
        objects = (const hkReferencedObject* const*)((const char*)objects + pointerStriding);
    }
}

// 0x00CE72B0 (:176)
void hkReferencedObject::removeReferences(const hkReferencedObject* const* objects, int numObjects, int pointerStriding)
{
    for (int i = 0; i < numObjects; i++)
    {
        (*objects)->removeReference();
        objects = (const hkReferencedObject* const*)((const char*)objects + pointerStriding);
    }
}

// 0x00CE71D0 (:110)
void hkReferencedObject::setLockMode(LockMode mode)
{
    hkReferencedObjectLock::s_instance->m_lockMode = mode;
}

// 0x00CE71E0 (:131)
hkReferencedObject::LockMode hkReferencedObject::getLockMode()
{
    return hkReferencedObjectLock::s_instance->m_lockMode;
}

// 0x00CE71F0 (:137)
void hkReferencedObject::lockAll()
{
    hkReferencedObjectLock::s_instance->lock();
}

// 0x00CE7230 (:143)
void hkReferencedObject::unlockAll()
{
    hkReferencedObjectLock::s_instance->unlock();
}

// 0x00CE7320 (:185)
hkCriticalSection* hkReferencedObject::getLockCriticalSection()
{
    return &hkReferencedObjectLock::s_instance->m_criticalSection;
}

// 0x00CE7330 (:100)
void hkReferencedObject::initializeLock()
{
    hkReferencedObjectLock* lock = new hkReferencedObjectLock();
    hkReferencedObjectLock::replaceInstance(lock);
}

// 0x00CE73B0 (:105)
void hkReferencedObject::deinitializeLock()
{
    hkReferencedObjectLock::replaceInstance(nullptr);
}

// 0x00CE73D0 (:116)
void hkReferencedObject::lockInit(LockMode lockMode)
{
    if (hkReferencedObjectLock::s_instance == nullptr)
    {
        hkReferencedObjectLock* lock = new hkReferencedObjectLock();
        lock->m_lockMode = lockMode;
        hkReferencedObjectLock::replaceInstance(lock);
        return;
    }
    hkReferencedObjectLock::s_instance->m_lockMode = lockMode;
}

// ---- hkReferencedObjectLock
// 0x00CE74F0 (:50)
hkReferencedObjectLock::hkReferencedObjectLock()
    : m_criticalSection(4000) // 4000: spin count
{
    m_lockMode = LOCK_MODE_AUTO;
    m_multiThreadCheck.enableChecks();
    m_lockCount = 0;
}

// 0x00CE7540
hkReferencedObjectLock::~hkReferencedObjectLock()
{
}

// 0x00CE75A0 (:59)
// Recursive per thread: the outermost lock enters the critical section.
void hkReferencedObjectLock::lock()
{
    unsigned int* store = hkMemoryRouter::getInstance().getRefObjectLocalStore();
    if (*store == LOCK_OWNER_MAGIC)
    {
        m_lockCount++;
        return;
    }
    m_criticalSection.enter();
    m_lockCount = 1;
    *store = LOCK_OWNER_MAGIC;
}

// 0x00CE75E0 (:75)
void hkReferencedObjectLock::unlock()
{
    unsigned int* store = hkMemoryRouter::getInstance().getRefObjectLocalStore();
    m_lockCount--;
    if (m_lockCount < 1)
    {
        *store = 0;
        m_criticalSection.leave();
    }
}
