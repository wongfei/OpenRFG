// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/hkbasesystem.cpp
//
// Havok 2014.2.5 (havok_impl.md HK1): Havok start-up and shut-down. init installs the calling thread's memory
// router, creates the reference lock, the error handler (an hkDefaultError over the report function) and the
// registered singletons, then requests the product features. quit releases them in reverse.
//
// Not reimplemented (S phase, see havok_impl.md section 3):
// - the file system singleton (hkWin32FileSystem) init creates and quit releases;
// - the thread's monitor stream (hkMonitorStream::init/quit): the shipped build keeps the timers, they are not
//   reproduced;
// - the platform network shut-down of quit (hkSocket).

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/hkbasesystem.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/config/hkproductfeatures.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/router/hkmemoryrouter.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hkreferencedobject.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hksingleton.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/system/error/hkdefaulterror.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"

// size 0x8
// A singleton whose only use is a virtual call that forces this file to be linked (file-local in the SDK).
class hkDummySingleton : public hkReferencedObject, public hkSingleton<hkDummySingleton>
{
public:
    hkDummySingleton() {} // 0x00CE6810 (:81)
    virtual void forceLinkage() {} // vtbl+0x10 0x00CE6820 (:83)
    virtual ~hkDummySingleton() {} // vtbl+0x00 0x00CE6830
};

// ---- globals
static hkBool hkBaseSystemIsInitialized; // 0x03A26FD8

// 0x00CE6300 (:413)
static hkReferencedObject* hkDummySingletoncreate()
{
    return new hkDummySingleton();
}

template<> hkDummySingleton* hkSingleton<hkDummySingleton>::s_instance = nullptr; // HK_SINGLETON_IMPLEMENTATION(hkDummySingleton)
static hkSingletonInitNode hkDummySingletonRegister("hkDummySingleton", hkDummySingletoncreate, (void**)&hkSingleton<hkDummySingleton>::s_instance);

// 0x00CE6270 (:136)
static void showHavokBuild()
{
}

// 0x00CE6280 (:404)
hkBool hkBaseSystem::isInitialized()
{
    return hkBaseSystemIsInitialized;
}

// 0x00CE6290 (:334)
hkResult hkBaseSystem::initThread(hkMemoryRouter* memoryRouter)
{
    hkMemoryRouter::replaceInstance(memoryRouter);
    // hkMonitorStream::init(): not reproduced (see the file comment)
    return hkResult(HK_SUCCESS);
}

// 0x00CE62C0 (:356)
hkResult hkBaseSystem::quitThread()
{
    // hkMonitorStream::quit() when the thread has a stream: not reproduced (see the file comment)
    if (hkMemoryRouter::getInstancePtr() != nullptr)
    {
        hkMemoryRouter::replaceInstance(nullptr);
    }
    return hkResult(HK_SUCCESS);
}

// 0x00CE6320 (:184)
// Creates every registered singleton not set yet. A singleton whose creation fails (it needs another one) is taken
// off the list and retried, last first, until all are created; created ones go back onto the list in that order.
void hkBaseSystem::initSingletons()
{
    hkArray<hkSingletonInitNode*, hkContainerTempAllocator> failed;
    hkSingletonInitNode** prevNext = &hkSingletonInitNode::m_head;
    hkSingletonInitNode* node = hkSingletonInitNode::m_head;
    while (node != nullptr)
    {
        if (*node->m_value == nullptr && node->m_createFunc != nullptr)
        {
            hkReferencedObject* obj = node->m_createFunc();
            if (obj == nullptr)
            {
                failed.pushBack(node);
                hkSingletonInitNode* next = node->m_next;
                node->m_next = nullptr;
                *prevNext = next;
                node = next;
                continue;
            }
            *node->m_value = obj;
        }
        prevNext = &node->m_next;
        node = *prevNext;
    }
    while (failed.getSize() != 0)
    {
        for (int i = failed.getSize() - 1; i >= 0; i--)
        {
            node = failed[i];
            hkReferencedObject* obj = node->m_createFunc();
            if (obj != nullptr)
            {
                *node->m_value = obj;
                *prevNext = node;
                prevNext = &node->m_next;
                const int last = failed.getSize() - 1;
                failed.setSizeUnchecked(last);
                if (last != i)
                {
                    failed[i] = failed[last];
                }
            }
        }
    }
}

// 0x00CE6520 (:270)
// Releases the created singletons, last created first.
void hkBaseSystem::quitSingletons()
{
    hkInplaceArray<hkSingletonInitNode*, 128, hkContainerHeapAllocator> nodes;
    for (hkSingletonInitNode* node = hkSingletonInitNode::m_head; node != nullptr; node = node->m_next)
    {
        if (*node->m_value != nullptr && node->m_createFunc != nullptr)
        {
            nodes.pushBack(node);
        }
    }
    for (int i = nodes.getSize() - 1; i >= 0; i--)
    {
        ((hkReferencedObject*)*nodes[i]->m_value)->removeReference();
        *nodes[i]->m_value = nullptr;
    }
}

// 0x00CE6440 (:297)
hkResult hkBaseSystem::init(hkMemoryRouter* memoryRouter, void (*errorReportFunction)(const char*, void*), void* errorReportObject)
{
    if (!hkBaseSystemIsInitialized)
    {
        initThread(memoryRouter);
        hkReferencedObject::initializeLock();
        // hkFileSystem::replaceInstance(new hkWin32FileSystem()): not reimplemented (see the file comment)
        hkError::replaceInstance(new hkDefaultError(errorReportFunction, errorReportObject));
        initSingletons();
        hkDummySingleton::getInstance().forceLinkage();
        hkProductFeatures::initialize();
        hkBaseSystemIsInitialized.m_bool = 1;
    }
    return hkResult(HK_SUCCESS);
}

// 0x00CE6640 (:371)
hkResult hkBaseSystem::quit()
{
    if (hkBaseSystemIsInitialized)
    {
        hkReferencedObject::setLockMode(hkReferencedObject::LOCK_MODE_NONE);
        quitSingletons();
        // the platform network shut-down (hkSocket): not reimplemented (see the file comment)
        hkError::replaceInstance(nullptr);
        // hkFileSystem::replaceInstance(nullptr): not reimplemented (see the file comment)
        hkReferencedObject::deinitializeLock();
        quitThread();
        hkBaseSystemIsInitialized.m_bool = 0;
    }
    return hkResult(HK_SUCCESS);
}
