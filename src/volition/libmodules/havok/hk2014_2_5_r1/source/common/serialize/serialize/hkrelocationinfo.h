// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/serialize/hkrelocationinfo.h
// Implementation: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/serialize/hkrelocationinfo.cpp
//
// Havok 2014.2.5 (stub_impl.md S4, serializer SE): the fixups an object writer records for the pointers of the
// objects it writes (local: into the same buffer, global: to another object, finish: the class to finish, import:
// a named external). Layout from rfg.pdb; the inline members as the 2014.1 SDK's hkRelocationInfo.h, checked
// against rfg.exe.
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/stringmap/hkstoragestringmap.h"

class hkClass;

// size 0x34
class hkRelocationInfo
{
public:
    // size 0x8
    struct Local
    {
        HK_DECLARE_POD_TYPE();
        Local(int f, int t) : m_fromOffset(f), m_toOffset(t) {} // 0x00F59790 (.h:42)
    public:
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(Local);

    public:
        int m_fromOffset; // 0x00
        int m_toOffset; // 0x04
    };

    // size 0x10
    struct Global
    {
        HK_DECLARE_POD_TYPE();
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(Global);
        // 0x00F7B7F0 (.h:62)
        Global(int f, void* ta, const hkClass* k, hkBool32 related)
            : m_fromOffset(f), m_toAddress(ta), m_toClass(k), m_related(related)
        {
        }

        int m_fromOffset; // 0x00
        void* m_toAddress; // 0x04
        const hkClass* m_toClass; // 0x08
        hkBool32 m_related; // 0x0C
    };

    // size 0x8
    struct Finish
    {
        HK_DECLARE_POD_TYPE();
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(Finish);
        Finish(int f, const char* k) : m_fromOffset(f), m_className(k) {} // 0x00F7B810 (.h:82)

        int m_fromOffset; // 0x00
        const char* m_className; // 0x04
    };

    // size 0x8
    struct Import
    {
        HK_DECLARE_POD_TYPE();
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(Import);
        Import(int fromOffset, const char* identifier) : m_fromOffset(fromOffset), m_identifier(identifier) {} // 0x00F723E0 (.h:98)

        int m_fromOffset; // 0x00
        const char* m_identifier; // 0x04
    };

    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkRelocationInfo); // delete(void* p, unsigned int nbytes) 0x00F5AFD0
    hkRelocationInfo() : m_pool(nullptr) {} // 0x00F5BDA0 (.h:24)
    hkRelocationInfo(const hkRelocationInfo&); // not copyable (asserts in the SDK; never defined)
    ~hkRelocationInfo(); // 0x00F72560
    void addLocal(int fromOffset, int toOffset) { m_local.pushBack(Local(fromOffset, toOffset)); } // 0x00F5BA60 (.h:108)
    // 0x00F7DD20 (.h:114)
    void addGlobal(int fromOffset, void* toAddr, const hkClass* klass, hkBool32 related = false)
    {
        m_global.pushBack(Global(fromOffset, toAddr, klass, related));
    }
    void addFinish(int fromOffset, const char* klassName) { m_finish.pushBack(Finish(fromOffset, klassName)); } // 0x00F7DD90 (.h:120)
    void addImport(int off, const char* name); // 0x00F72480
    void applyLocalAndGlobal(void* buffer); // 0x00F72430
    // 0x00FAAB90 (.h:132): the pool keeps its names
    void clear()
    {
        m_local.clear();
        m_global.clear();
        m_finish.clear();
        m_imports.clear();
    }

    hkArray<Local,hkContainerHeapAllocator> m_local; // 0x00
    hkArray<Global,hkContainerHeapAllocator> m_global; // 0x0C
    hkArray<Finish,hkContainerHeapAllocator> m_finish; // 0x18
    hkArray<Import,hkContainerHeapAllocator> m_imports; // 0x24
    hkStorageStringMap<int,hkContainerHeapAllocator>* m_pool; // 0x30: the import names, created by the first import
};
