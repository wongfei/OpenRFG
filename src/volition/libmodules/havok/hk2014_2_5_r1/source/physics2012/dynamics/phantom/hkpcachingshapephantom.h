// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/phantom/hkpcachingshapephantom.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (player_minimal.md P0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/array/hkarray.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/container/hkcontainerallocators.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/phantom/hkpphantomtype.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/dynamics/phantom/hkpshapephantom.h"

class hkClass;
struct hkInternalClassMember;
class hkTransformf;
class hkVector4f;
class hkpCdBodyPairCollector;
class hkpCdPointCollector;
class hkpCollidable;
class hkpCollisionAgent;
struct hkpCollisionInput;
struct hkpLinearCastInput;
class hkpPhantom;
class hkpShape;

// size 0x160
class hkpCachingShapePhantom : public hkpShapePhantom
{
public:
    // size 0x8
    struct CollisionDetail
    {
        HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(CollisionDetail);

        hkpCollisionAgent* m_agent; // 0x00
        hkpCollidable* m_collidable; // 0x04
    };

    struct hkpCollisionDetail; // no definition in PDB

    // size 0x1
    class OrderByUid
    {
    public:
        hkBool operator()(const CollisionDetail& a, const CollisionDetail& b); // 0x00E0E7F0

    };

    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int, void* p) 0x00DF03A0, new(unsigned int nbytes) 0x00814BC0, delete(void* p, unsigned int nbytes) 0x00DF05B0
    static const hkClass& staticClass(); // 0x00DF03B0
    hkpCachingShapePhantom(hkFinishLoadedObjectFlag flag); // 0x00DF0550
    hkpCachingShapePhantom(const hkpShape* shape, const hkTransformf& transform, unsigned int collisionFilterInfo); // 0x00E0E120
    virtual ~hkpCachingShapePhantom(); // vtbl+0x00 0x00E0E170
    virtual hkpPhantomType getType() const; // vtbl+0x1C 0x00E0DED0
    virtual void addOverlappingCollidable(hkpCollidable* collidable); // vtbl+0x24 0x00E0E210
    virtual hkBool isOverlappingCollidableAdded(const hkpCollidable* collidable); // vtbl+0x28 0x00E0DF90
    virtual void removeOverlappingCollidable(hkpCollidable* collidable); // vtbl+0x2C 0x00E0E060
    virtual void ensureDeterministicOrder(); // vtbl+0x30 0x00E0E3A0
    virtual hkpPhantom* clone() const; // vtbl+0x34 0x00E0E3E0
    virtual void updateShapeCollectionFilter(); // vtbl+0x38 0x00E0DFE0
    hkArray<CollisionDetail,hkContainerHeapAllocator>& getCollisionDetails(); // 0x0080D620 (hkpcachingshapephantom.inl:11)
    virtual void deallocateInternalArrays(); // vtbl+0x3C 0x00E0E340
    virtual void setPositionAndLinearCast(const hkVector4f& position, const hkpLinearCastInput& input, hkpCdPointCollector& castCollector, hkpCdPointCollector* startCollector); // vtbl+0x40 0x00E0E570
    virtual void setTransformAndLinearCast(const hkTransformf& transform, const hkpLinearCastInput& input, hkpCdPointCollector& castCollector, hkpCdPointCollector* startCollector); // vtbl+0x44 0x00E0E740
    virtual void getClosestPoints(hkpCdPointCollector& collector, const hkpCollisionInput* input); // vtbl+0x48 0x00E0DEE0
    virtual void getPenetrations(hkpCdBodyPairCollector& collector, const hkpCollisionInput* input); // vtbl+0x4C 0x00E0DF30

    static const hkInternalClassMember Members[]; // 0x01361374
public:
    hkArray<CollisionDetail,hkContainerHeapAllocator> m_collisionDetails; // 0x150
public:
    hkBool m_orderDirty; // 0x15C
};
