// Original: volition/rfg/code/havok/havok_mesh_shape.h
// Implementation: volition/rfg/code/havok/havok_mesh_shape.cpp
//
// havok_mesh_shape, an extended mesh shape loaded in place (hkFinishLoadedObjectFlag constructor) that owns its
// vertex and triangle arrays.
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/compound/collection/extendedmeshshape/hkpextendedmeshshape.h"

class hkClass;
class hkFinishLoadedObjectFlag;
struct hkInternalClassMember;

// size 0x110
class havok_mesh_shape : public hkpExtendedMeshShape
{
public:
    // size 0xC
    struct vertex
    {
        struct MustEndWithSemiColon; // no definition in PDB

        struct DefaultStruct; // no definition in PDB

        static void* operator new(unsigned int, void*);
        static void* operator new(unsigned int);
        static void operator delete(void*, void*);
        static void operator delete(void*, unsigned int);
        static void* operator new[](unsigned int, void*);
        static void operator delete[](void*, void*);
        static const hkClass& staticClass(); // 0x00812720 (havok_mesh_shapereflection.cpp:47)

        static const hkInternalClassMember Members[];
        float m_x; // 0x00
        float m_y; // 0x04
        float m_z; // 0x08
    };

    // size 0x6
    struct triangle
    {
        struct MustEndWithSemiColon; // no definition in PDB

        struct DefaultStruct; // no definition in PDB

        static void* operator new(unsigned int, void*);
        static void* operator new(unsigned int);
        static void operator delete(void*, void*);
        static void operator delete(void*, unsigned int);
        static void* operator new[](unsigned int, void*);
        static void operator delete[](void*, void*);
        static const hkClass& staticClass(); // 0x00812740 (havok_mesh_shapereflection.cpp:94)

        static const hkInternalClassMember Members[];
        unsigned short m_a; // 0x00
        unsigned short m_b; // 0x02
        unsigned short m_c; // 0x04
    };

    struct DefaultStruct; // no definition in PDB

    static const hkClass& staticClass(); // 0x00812760 (havok_mesh_shapereflection.cpp:143)
    virtual int getNumChildShapes() const; // vtbl+0x04 0x0081C6B0 (:661)
    virtual unsigned int getFirstKey() const; // vtbl+0x08 0x00811AD0 (:670)
    virtual unsigned int getNextKey(unsigned int old_key) const; // vtbl+0x0C 0x00811AF0 (:698)
    havok_mesh_shape(hkFinishLoadedObjectFlag flag); // 0x00831710 (.h:82)
    virtual ~havok_mesh_shape(); // vtbl+0x00 0x008317A0

    static const hkInternalClassMember Members[]; // 0x012F91B8
protected:
    hkArray<vertex,hkContainerHeapAllocator> m_vertices; // 0xF0
    hkArray<triangle,hkContainerHeapAllocator> m_triangles; // 0xFC
};
