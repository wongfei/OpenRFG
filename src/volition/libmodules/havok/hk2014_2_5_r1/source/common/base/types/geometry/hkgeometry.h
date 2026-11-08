// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/geometry/hkgeometry.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4f.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hkreferencedobject.h"

class hkBool;
class hkClass;
class hkFinishLoadedObjectFlag;
struct hkInternalClassMember;
class hkMatrix4f;
struct hkResult;
class hkTransformf;

// size 0x20
struct hkGeometry : public hkReferencedObject
{
    enum GeometryType
    {
        GEOMETRY_STATIC = 0,
        GEOMETRY_DYNAMIC_VERTICES = 1,
    };

    // size 0x10
    struct Triangle
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(Triangle);
        static const hkClass& staticClass(); // 0x00CEB620
        HK_DECLARE_POD_TYPE();
        void set(int a, int b, int c, int material); // 0x00D36830
        int getVertex(int) const;

        static const hkInternalClassMember Members[];
        int m_a; // 0x00
        int m_b; // 0x04
        int m_c; // 0x08
        int m_material; // 0x0C
    };

    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int, void* p) 0x00CEB610, delete(void* p, unsigned int nbytes) 0x00D0C5F0
    static const hkClass& staticClass(); // 0x00CEB640
    hkGeometry(hkFinishLoadedObjectFlag f); // 0x00D0BDA0
    hkGeometry(const hkGeometry& other); // 0x00D0BC40
    hkGeometry(); // 0x00EDD9F0
    void clear(); // 0x00D0BC30
    hkBool isValid() const; // 0x00D0BBC0
    hkVector4f& getVertex(int, int);
    const hkVector4f& getVertex(int, int) const;
    void getTriangle(int triangleIndex, hkVector4f* vertices) const; // 0x00F36240
    hkResult appendGeometry(const hkGeometry& geometry, const hkMatrix4f* transform); // 0x00D0BDB0
    hkResult appendGeometry(const hkGeometry&, const hkTransformf&);
    virtual ~hkGeometry(); // vtbl+0x00 0x00D0C630

    static const hkInternalClassMember Members[];
    hkArray<hkVector4f,hkContainerHeapAllocator> m_vertices; // 0x08
    hkArray<Triangle,hkContainerHeapAllocator> m_triangles; // 0x14
};
