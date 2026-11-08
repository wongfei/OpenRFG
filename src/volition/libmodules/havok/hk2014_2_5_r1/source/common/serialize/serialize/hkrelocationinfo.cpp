// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/serialize/hkrelocationinfo.cpp
//
// Havok 2014.2.5 (stub_impl.md S4, serializer SE), from rfg.exe.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/serialize/serialize/hkrelocationinfo.h"

// 0x00F72560 (:13): the arrays are freed by their destructors
hkRelocationInfo::~hkRelocationInfo()
{
    delete m_pool;
}

// 0x00F72430 (:18): local fixups become absolute pointers into the buffer, globals are stored as they are
void hkRelocationInfo::applyLocalAndGlobal(void* buffer)
{
    for (int i = 0; i < m_local.getSize(); i++)
    {
        const Local& l = m_local[i];
        *reinterpret_cast<void**>(static_cast<char*>(buffer) + l.m_fromOffset) = static_cast<char*>(buffer) + l.m_toOffset;
    }
    for (int i = 0; i < m_global.getSize(); i++)
    {
        const Global& g = m_global[i];
        *reinterpret_cast<void**>(static_cast<char*>(buffer) + g.m_fromOffset) = g.m_toAddress;
    }
}

// 0x00F72480 (:37): the name is the pool's copy
void hkRelocationInfo::addImport(int off, const char* name)
{
    if (m_pool == nullptr)
    {
        m_pool = new hkStorageStringMap<int,hkContainerHeapAllocator>();
    }
    name = m_pool->insert(name, 0);
    m_imports.pushBack(Import(off, name));
}
