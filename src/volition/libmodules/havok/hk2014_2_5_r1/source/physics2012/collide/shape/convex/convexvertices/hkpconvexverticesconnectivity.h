// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/convex/convexvertices/hkpconvexverticesconnectivity.h
//
// Seam only (Havok 2014.2.5).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hkreferencedobject.h"

class hkBool;
class hkClass;
class hkFinishLoadedObjectFlag;

// size 0x20
class hkpConvexVerticesConnectivity : public hkReferencedObject
{
public:
    static const hkClass& staticClass(); // 0x00D7F550
    hkpConvexVerticesConnectivity(hkFinishLoadedObjectFlag flag); // 0x00D7F5B0
    hkpConvexVerticesConnectivity();
    hkBool isClosed() const; // 0x00DAB5C0
    virtual ~hkpConvexVerticesConnectivity(); // vtbl+0x00 0x00D7F600
    void addFace(int* indices, int numIndices); // 0x00DAB540
    int getNumFaces() const; // 0x00D99330
    virtual void clear(); // vtbl+0x10 0x00DAB530

    hkArray<unsigned short,hkContainerHeapAllocator> m_vertexIndices; // 0x08
    hkArray<unsigned char,hkContainerHeapAllocator> m_numVerticesPerFace; // 0x14
};
