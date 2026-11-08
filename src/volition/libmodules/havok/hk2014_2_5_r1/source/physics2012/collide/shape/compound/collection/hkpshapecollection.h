// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/compound/collection/hkpshapecollection.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/geometry/collide/shapes/hkcdshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshapecontainer.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/util/welding/hkpweldingutility.h"

class hkAabb;
class hkClass;
struct hkInternalClassMember;
class hkTransformf;
class hkVector4f;
class hkpCdBody;
class hkpRayHitCollector;
struct hkpShapeRayCastInput;
struct hkpShapeRayCastOutput;

// size 0x18
class hkpShapeCollection : public hkpShape, public hkpShapeContainer
{
public:
    enum
    {
        THIS_SHAPE_TYPE = 18,
    };

    enum CollectionType
    {
        COLLECTION_LIST = 0,
        COLLECTION_EXTENDED_MESH = 1,
        COLLECTION_TRISAMPLED_HEIGHTFIELD = 2,
        COLLECTION_USER = 3,
        COLLECTION_SIMPLE_MESH = 4,
        COLLECTION_MESH_SHAPE = 5,
        COLLECTION_COMPRESSED_MESH = 6,
        COLLECTION_MAX = 7,
    };

    HK_DECLARE_CLASS_ALLOCATOR(); // delete(void* p, unsigned int nbytes) 0x00D81090
    static const hkClass& staticClass(); // 0x00D7C6F0
    hkpShapeCollection(hkFinishLoadedObjectFlag flag); // 0x00D81030
    hkpShapeCollection(hkcdShapeType::ShapeTypeEnum type, CollectionType subType); // 0x00D80B20
    hkpShapeCollection();
    void afterReflectNew(); // 0x00D80B60
    virtual ~hkpShapeCollection(); // vtbl+0x00 0x0080CAF0
    virtual void getAabb(const hkTransformf& localToWorld, float tolerance, hkAabb& out) const; // vtbl+0x14 0x00D80CE0
    virtual hkBool castRay(const hkpShapeRayCastInput& input, hkpShapeRayCastOutput& results) const; // vtbl+0x18 0x00D80B70
    virtual bool isWeldingEnabled() const; // vtbl+0x18 0x008118F0 (.h:104)
    virtual void castRayWithCollector(const hkpShapeRayCastInput& input, const hkpCdBody& cdBody, hkpRayHitCollector& collector) const; // vtbl+0x1C 0x00D80EE0
    virtual const hkpShapeContainer* getContainer() const; // vtbl+0x3C 0x00D80AF0
    virtual float getMaximumProjection(const hkVector4f& direction) const; // vtbl+0x40 0x00D80DF0
    virtual void initWeldingInfo(hkpWeldingUtility::WeldingType weldingType); // vtbl+0x48 0x00D80B10
    virtual void setWeldingInfo(unsigned int key, short weldingInfo); // vtbl+0x4C 0x00D80B00

    static const hkInternalClassMember Members[];
    hkBool m_disableWelding; // 0x14
    hkEnum<enum CollectionType,unsigned char> m_collectionType; // 0x15
};
