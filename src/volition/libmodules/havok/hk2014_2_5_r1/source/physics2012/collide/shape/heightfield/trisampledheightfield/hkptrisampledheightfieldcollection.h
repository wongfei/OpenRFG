// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/heightfield/trisampledheightfield/hkptrisampledheightfieldcollection.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4f.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/compound/collection/hkpshapecollection.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/util/welding/hkpweldingutility.h"

class hkAabb;
class hkClass;
class hkFinishLoadedObjectFlag;
struct hkInternalClassMember;
class hkTransformf;
class hkpSampledHeightFieldShape;
struct hkpShapeBufferStorage;

// size 0x40
class hkpTriSampledHeightFieldCollection : public hkpShapeCollection
{
public:
    enum
    {
        THIS_SHAPE_TYPE = 6,
    };

    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int, void* p) 0x00D7DF30, delete(void* p, unsigned int nbytes) 0x00D9CA30
    static const hkClass& staticClass(); // 0x00D7DF40
    hkpTriSampledHeightFieldCollection(hkFinishLoadedObjectFlag flag); // 0x00D9C840
    hkpTriSampledHeightFieldCollection(const hkpSampledHeightFieldShape* shape, float radius); // 0x00D9C7C0
    hkpTriSampledHeightFieldCollection();
    void afterReflectNew(); // 0x00D9C5C0
    virtual ~hkpTriSampledHeightFieldCollection(); // vtbl+0x00 0x00D9C870
    virtual unsigned int getFirstKey() const; // vtbl+0x08 0x00D9C440
    virtual unsigned int getNextKey(unsigned int oldKey) const; // vtbl+0x0C 0x00D9C450
    virtual unsigned int getCollisionFilterInfo(unsigned int key) const; // vtbl+0x10 0x00D9C4F0
    virtual const hkpShape* getChildShape(unsigned int key, hkpShapeBufferStorage& buffer) const; // vtbl+0x14 0x00D9C5D0
    virtual void getAabb(const hkTransformf& localToWorld, float tolerance, hkAabb& out) const; // vtbl+0x14 0x00D9C510
    virtual int calcSizeForSpu(const hkpShape::CalcSizeForSpuInput& input, int spuBufferSizeLeft) const; // vtbl+0x44 0x00D9C4A0
    virtual void initWeldingInfo(hkpWeldingUtility::WeldingType weldingType); // vtbl+0x48 0x00D9C8D0
    virtual void setWeldingInfo(unsigned int key, short weldingInfo); // vtbl+0x4C 0x00D9C580
    const hkpSampledHeightFieldShape* getHeightFieldShape() const; // 0x00D9C500
    float getRadius() const; // 0x00DA62A0
    void setRadius(float);
    const hkVector4f& getTriangleExtrusion() const; // 0x00D9C410
    void setTriangleExtrusion(const hkVector4f&);
public:
    int getXFromShapeKey(unsigned int key) const; // 0x00D9C420
    int getZFromShapeKey(unsigned int key) const; // 0x00D9C430
    int getIndexFromShapeKey(unsigned int key) const; // 0x00D9C9B0
    unsigned short getWeldingInfo(unsigned int key) const; // 0x00D9C9E0

public:
    static const hkInternalClassMember Members[]; // 0x01355F30
public:
    const hkpSampledHeightFieldShape* m_heightfield; // 0x18
    int m_childSize; // 0x1C
    float m_radius; // 0x20
    hkArray<unsigned short,hkContainerHeapAllocator> m_weldingInfo; // 0x24
    hkVector4f m_triangleExtrusion; // 0x30
};
