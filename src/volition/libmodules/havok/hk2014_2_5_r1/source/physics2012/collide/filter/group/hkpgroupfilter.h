// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/filter/group/hkpgroupfilter.h
//
// Seam only (Havok 2014.2.5).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4f.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/filter/hkpcollisionfilter.h"

class hkBool;
class hkClass;
class hkFinishLoadedObjectFlag;
class hkpCdBody;
class hkpCollidable;
struct hkpCollisionInput;
class hkpShapeContainer;
struct hkpShapeRayCastInput;
struct hkpWorldRayCastInput;

// size 0x100
class hkpGroupFilter : public hkpCollisionFilter
{
public:
    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int, void* p) 0x00D7F0C0, delete(void* p, unsigned int nbytes) 0x00D7F220
    enum
    {
        USE_COLLIDABLE_FILTER_INFO = -1,
    };

    static const hkClass& staticClass(); // 0x00D7F0D0
    hkpGroupFilter(hkFinishLoadedObjectFlag flag); // 0x00D7F170
    hkpGroupFilter(); // 0x00DAB1C0
    virtual ~hkpGroupFilter(); // vtbl+0x00 0x00DAB290
    void enableCollisionsBetween(int layerA, int layerB); // 0x00DAB120
    void disableCollisionsBetween(int layerA, int layerB); // 0x00DAB150
    void enableCollisionsUsingBitfield(unsigned int layerBitsA, unsigned int layerBitsB); // 0x00DAB0F0
    void disableCollisionsUsingBitfield(unsigned int layerBitsA, unsigned int layerBitsB); // 0x00DAB180
    static unsigned int calcFilterInfo(int, int, int, int);
    static int getLayerFromFilterInfo(unsigned int);
    static int setLayer(unsigned int, int);
    static int getSystemGroupFromFilterInfo(unsigned int);
    static int getSubSystemIdFromFilterInfo(unsigned int);
    static int getSubSystemDontCollideWithFromFilterInfo(unsigned int);
    int getNewSystemGroup();
    hkBool isCollisionEnabled(unsigned int infoA, unsigned int infoB) const; // 0x00DAB040
    virtual hkBool isCollisionEnabled(const hkpShapeRayCastInput& aInput, const hkpShapeContainer& bContainer, unsigned int bKey) const; // vtbl+0x00 0x00DAB0B0
    virtual hkBool isCollisionEnabled(const hkpCollisionInput& input, const hkpCdBody& collectionBodyA, const hkpCdBody& collectionBodyB, const hkpShapeContainer& containerShapeA, const hkpShapeContainer& containerShapeB, unsigned int keyA, unsigned int keyB) const; // vtbl+0x00 0x00DAB2F0
    virtual hkBool isCollisionEnabled(const hkpWorldRayCastInput& aInput, const hkpCollidable& collidableB) const; // vtbl+0x04 0x00DAB370
    virtual hkBool isCollisionEnabled(const hkpCollisionInput& input, const hkpCdBody& a, const hkpCdBody& b, const hkpShapeContainer& bContainer, unsigned int bKey) const; // vtbl+0x04 0x00DAB3A0
    virtual hkBool isCollisionEnabled(const hkpCollidable& a, const hkpCollidable& b) const; // vtbl+0x04 0x00DAB2C0
    void afterReflectNew(); // 0x00D7F160
    virtual int dummyUnused(); // vtbl+0x14 0x00D7F1D0

    int m_nextFreeSystemGroup; // 0x30
    unsigned int m_collisionLookupTable[32]; // 0x34
    hkVector4f m_pad256[4]; // 0xC0
};
