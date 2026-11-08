// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/reflection/hktypeinfo.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (stub_impl.md S4, serializer).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"


// size 0x18
class hkTypeInfo
{
public:
    typedef void (*FinishLoadedObjectFunction)(void*, int);
    typedef void (*CleanupLoadedObjectFunction)(void*);

    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkTypeInfo); // new(unsigned int nbytes) 0x01054880, delete(void* p, unsigned int nbytes) 0x010548A0
    // 0x0080F280 (.h:34); rfg.exe's type infos inline it into their dynamic initializers
    hkTypeInfo(const char* name, const char* scopedName, void (*finish)(void*, int), void (*cleanup)(void*), const void* vtable, unsigned int size)
        : m_typeName(name), m_scopedName(scopedName), m_finishLoadedObjectFunction(finish),
          m_cleanupLoadedObjectFunction(cleanup), m_vtable(vtable), m_size(size)
    {
    }
    const char* getTypeName() const { return m_typeName; } // 0x00F3C380 (.h:37)
    const char* getScopedName() const { return m_scopedName; }
    const void* getVtable() const { return m_vtable; } // 0x00F3BFB0 (.h:45)
    void cleanupLoadedObject(void* ptr) const; // 0x00D1AA10
    void finishLoadedObject(void* ptr, int finishFlag) const; // 0x00D1A9D0
    void finishLoadedObjectWithoutTracker(void* ptr, int finishFlag) const; // 0x00D1A9F0
    hkBool hasFinishFunction() const { return m_finishLoadedObjectFunction != nullptr; } // 0x00D184C0 (.h:58)
    hkBool hasCleanupFunction() const { return m_cleanupLoadedObjectFunction != nullptr; } // 0x00D184D0
    unsigned int getSize() const { return m_size; }
    hkBool isVirtual() const { return m_vtable != nullptr; }

public:
    const char* m_typeName; // 0x00
    const char* m_scopedName; // 0x04
    void (*m_finishLoadedObjectFunction)(void*, int); // 0x08
    void (*m_cleanupLoadedObjectFunction)(void*); // 0x0C
    const void* m_vtable; // 0x10
    unsigned int m_size; // 0x14
};
