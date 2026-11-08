// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hkreferencedobject.h
// Implementation: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hkreferencedobject.cpp
//
// Hand-written (Havok 2014.2.5, havok_impl.md HK1): the reference-counted Havok object. The word at +4 holds the
// reference count (low 16 bits) and the memory size (high 16 bits): 0xFFFF (AUTO_MEMSIZE, set by the constructor)
// means "sizeof the class" for the class operator delete, 0 means the object lives in other storage (loaded in place
// or embedded) and is never counted or deleted. The count changes with an atomic compare-exchange on the whole word
// (hkreferencedobject.inl); when it drops to 0, deleteThisReferencedObject runs (the deleting destructor).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hkbaseobject.h"

#include <intrin.h>

class hkClass;
class hkCriticalSection;
class hkFinishLoadedObjectFlag;

// size 0x8
class hkReferencedObject : public hkBaseObject
{
public:
    enum LockMode
    {
        LOCK_MODE_NONE = 0,
        LOCK_MODE_AUTO = 1,
        LOCK_MODE_MANUAL = 2,
    };

    enum
    {
        AUTO_MEMSIZE = 65535,
    };

    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int, void* p) 0x00CEB8C0, new(unsigned int nbytes) 0x00CE6220, delete(void* p, unsigned int nbytes) 0x007105E0
    hkReferencedObject(hkFinishLoadedObjectFlag flag); // 0x00CE8FD0
    hkReferencedObject(const hkReferencedObject& originalObject); // 0x00B62A30
    hkReferencedObject(); // 0x007105C0 (hkreferencedobject.inl:11)
    void operator=(const hkReferencedObject& originalObject) {} // 0x00F7E590 (the count and size stay)
    virtual ~hkReferencedObject(); // vtbl+0x00 0x007105B0
    int getReferenceCount() const { return (int)(m_memSizeAndRefCount & 0xFFFF); } // 0x00710630
    void setReferenceCount(int newRefCount) { m_memSizeAndRefCount = (m_memSizeAndRefCount & 0xFFFF0000) | (unsigned int)(newRefCount & 0xFFFF); } // 0x00DF29D0
    int getMemorySizeAndFlags() const { return (int)(m_memSizeAndRefCount >> 16); } // 0x00710640
    void setMemorySizeAndFlags(int newMemSizeAndFlags) { m_memSizeAndRefCount = (m_memSizeAndRefCount & 0xFFFF) | ((unsigned int)newMemSizeAndFlags << 16); } // 0x00DF29A0
    void setMemorySizeFlagsAndReferenceCount(int newMemSizeAndFlags, int newRefCount) { m_memSizeAndRefCount = ((unsigned int)newMemSizeAndFlags << 16) | (unsigned int)(newRefCount & 0xFFFF); } // 0x00D1A2B0

    // 0x00813F20 (hkreferencedobject.inl:118)
    void addReference() const
    {
        if ((m_memSizeAndRefCount >> 16) != 0)
        {
            volatile long* word = (volatile long*)&m_memSizeAndRefCount;
            long oldValue;
            do
            {
                oldValue = *word;
            } while (_InterlockedCompareExchange(word, (long)(((oldValue + 1) ^ oldValue) & 0xFFFF ^ oldValue), oldValue) != oldValue);
        }
    }

    // 0x0071A6A0 (hkreferencedobject.inl:149)
    void removeReference() const
    {
        if ((m_memSizeAndRefCount >> 16) != 0)
        {
            volatile long* word = (volatile long*)&m_memSizeAndRefCount;
            long oldValue;
            long newValue;
            do
            {
                oldValue = *word;
                newValue = ((oldValue - 1) ^ oldValue) & 0xFFFF ^ oldValue;
            } while (_InterlockedCompareExchange(word, newValue, oldValue) != oldValue);
            if ((newValue & 0xFFFF) == 0)
            {
                deleteThisReferencedObject();
            }
        }
    }

    static void addReferences(const hkReferencedObject* const* objects, int numObjects, int pointerStriding); // 0x00CE7260
    static void removeReferences(const hkReferencedObject* const* objects, int numObjects, int pointerStriding); // 0x00CE72B0
    static void setLockMode(LockMode mode); // 0x00CE71D0
    static LockMode getLockMode(); // 0x00CE71E0
    static void lockAll(); // 0x00CE71F0
    static void unlockAll(); // 0x00CE7230
    static hkCriticalSection* getLockCriticalSection(); // 0x00CE7320
    static void lockInit(LockMode lockMode); // 0x00CE73D0
    static void initializeLock(); // 0x00CE7330
    static void deinitializeLock(); // 0x00CE73B0
    virtual const hkClass* getClassType() const; // vtbl+0x08 0x00CE71A0
protected:
    virtual void deleteThisReferencedObject() const; // vtbl+0x0C 0x00CE71B0

private:
    unsigned int m_memSizeAndRefCount; // 0x04, reference count in the low 16 bits, memory size in the high 16 (0 = loaded in place, never deleted)
};

// OpenRFG addition (not in Havok): told about every counted object (memory size not 0) when it is constructed, and
// about every object when it is destructed; null in the game. The headless tests count the live Havok objects with it.
extern void (*hkReferencedObjectTracker)(const hkReferencedObject* object, bool alive);
