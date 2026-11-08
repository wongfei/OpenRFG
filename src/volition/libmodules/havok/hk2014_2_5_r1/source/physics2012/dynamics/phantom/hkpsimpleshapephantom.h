// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/phantom/hkpsimpleshapephantom.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/phantom/hkpshapephantom.h"

class hkClass;
struct hkInternalClassMember;
class hkTransformf;
class hkVector4f;
class hkpCdBodyPairCollector;
class hkpCdPointCollector;
class hkpCollidable;
struct hkpCollisionInput;
struct hkpLinearCastInput;
class hkpPhantom;
enum hkpPhantomType;
class hkpShape;

// size 0x160
class hkpSimpleShapePhantom : public hkpShapePhantom
{
public:
    // size 0x4
    struct CollisionDetail
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(CollisionDetail);
        static const hkClass& staticClass(); // 0x00DEDF10

        static const hkInternalClassMember Members[];
        hkpCollidable* m_collidable; // 0x00
    };

    // size 0x1
    class OrderByUid
    {
    public:
        hkBool operator()(const CollisionDetail& a, const CollisionDetail& b); // 0x00DE0610

    };

    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int, void* p) 0x00DEDF00, new(unsigned int nbytes) 0x00814BA0, delete(void* p, unsigned int nbytes) 0x00DE0D10
    static const hkClass& staticClass(); // 0x00DEDF30
    hkpSimpleShapePhantom(hkFinishLoadedObjectFlag flag); // 0x00DEE280
    hkpSimpleShapePhantom(const hkpShape* shape, const hkTransformf& transform, unsigned int collisionFilterInfo); // 0x00DDFE30
    virtual ~hkpSimpleShapePhantom(); // vtbl+0x00 0x00DDFE80
    virtual hkpPhantomType getType() const; // vtbl+0x1C 0x00DDFBB0
    virtual void addOverlappingCollidable(hkpCollidable* collidable); // vtbl+0x24 0x00DDFD90
    virtual hkBool isOverlappingCollidableAdded(const hkpCollidable* collidable); // vtbl+0x28 0x00DDFBC0
    virtual void removeOverlappingCollidable(hkpCollidable* collidable); // vtbl+0x2C 0x00DDFD10
    virtual void ensureDeterministicOrder(); // vtbl+0x30 0x00DDFDF0
    virtual hkpPhantom* clone() const; // vtbl+0x34 0x00DDFEE0
    virtual void deallocateInternalArrays(); // vtbl+0x3C 0x00DE02A0
    virtual void setPositionAndLinearCast(const hkVector4f& position, const hkpLinearCastInput& input, hkpCdPointCollector& castCollector, hkpCdPointCollector* startCollector); // vtbl+0x40 0x00DE0070
    virtual void setTransformAndLinearCast(const hkTransformf& transform, const hkpLinearCastInput& input, hkpCdPointCollector& castCollector, hkpCdPointCollector* startCollector); // vtbl+0x44 0x00DE0270
    virtual void getClosestPoints(hkpCdPointCollector& collector, const hkpCollisionInput* input); // vtbl+0x48 0x00DDFC00
    hkArray<CollisionDetail,hkContainerHeapAllocator>& getCollisionDetails(); // 0x00F2A050
    virtual void getPenetrations(hkpCdBodyPairCollector& collector, const hkpCollisionInput* input); // vtbl+0x4C 0x00DDFC80

    static const hkInternalClassMember Members[]; // 0x0135D430
public:
    hkArray<CollisionDetail,hkContainerHeapAllocator> m_collisionDetails; // 0x150
public:
    hkBool m_orderDirty; // 0x15C
};
