// Original: volition/libmodules/havok/hk2014_2_5_r1/source/geometry/collide/shapes/hkcdshape.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/object/hkreferencedobject.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"

class hkClass;
struct hkInternalClassMember;

// size 0x1
struct hkcdShapeType
{
    enum ShapeTypeEnum
    {
        SPHERE = 0,
        CYLINDER = 1,
        TRIANGLE = 2,
        BOX = 3,
        CAPSULE = 4,
        CONVEX_VERTICES = 5,
        TRI_SAMPLED_HEIGHT_FIELD_COLLECTION = 6,
        TRI_SAMPLED_HEIGHT_FIELD_BV_TREE = 7,
        LIST = 8,
        MOPP = 9,
        CONVEX_TRANSLATE = 10,
        CONVEX_TRANSFORM = 11,
        SAMPLED_HEIGHT_FIELD = 12,
        EXTENDED_MESH = 13,
        TRANSFORM = 14,
        COMPRESSED_MESH = 15,
        STATIC_COMPOUND = 16,
        BV_COMPRESSED_MESH = 17,
        COLLECTION = 18,
        USER0 = 19,
        USER1 = 20,
        USER2 = 21,
        BV_TREE = 22,
        CONVEX = 23,
        CONVEX_PIECE = 24,
        MULTI_SPHERE = 25,
        CONVEX_LIST = 26,
        TRIANGLE_COLLECTION = 27,
        HEIGHT_FIELD = 28,
        SPHERE_REP = 29,
        BV = 30,
        PLANE = 31,
        PHANTOM_CALLBACK = 32,
        MULTI_RAY = 33,
        INVALID = 34,
        FIRST_SHAPE_TYPE = 0,
        MAX_SPU_SHAPE_TYPE = 22,
        MAX_PPU_SHAPE_TYPE = 35,
        ALL_SHAPE_TYPES = -1,
    };

    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkcdShapeType);
    static const hkClass& staticClass(); // 0x00D1AAF0

    static const hkInternalClassMember Members[];
};

// size 0x1
struct hkcdShapeDispatchType
{
    enum ShapeDispatchTypeEnum
    {
        CONVEX_IMPLICIT = 0,
        CONVEX = 1,
        HEIGHT_FIELD = 2,
        COMPOSITE = 3,
        USER = 4,
        NUM_DISPATCH_TYPES = 5,
    };

    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkcdShapeDispatchType);
    static const hkClass& staticClass(); // 0x00D1AAB0

    static const hkInternalClassMember Members[];
};

// size 0x1
struct hkcdShapeInfoCodecType
{
    enum ShapeInfoCodecTypeEnum
    {
        NULL_CODEC = 0,
        UFM358 = 1,
        MAX_NUM_CODECS = 16,
    };

    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkcdShapeInfoCodecType);
    static const hkClass& staticClass(); // 0x00D1AAD0

    static const hkInternalClassMember Members[];
};

// size 0xC
class hkcdShape : public hkReferencedObject
{
public:
    typedef hkcdShapeType::ShapeTypeEnum ShapeType;
    typedef hkcdShapeDispatchType::ShapeDispatchTypeEnum DispatchType;
    typedef hkcdShapeInfoCodecType::ShapeInfoCodecTypeEnum ShapeInfoCodecType;

    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int, void* p) 0x00D1AA40, delete(void* p, unsigned int nbytes) 0x0071A7F0
    static const hkClass& staticClass(); // 0x00D1AA50
    hkcdShape(hkFinishLoadedObjectFlag flag); // 0x00D1B400
    hkcdShape();
    hkcdShape(hkcdShapeType::ShapeTypeEnum);
    hkcdShapeType::ShapeTypeEnum getType() const;
    void setType(hkcdShapeType::ShapeTypeEnum newType); // 0x00D74450
    virtual ~hkcdShape(); // vtbl+0x00 0x007108B0

    static const hkInternalClassMember Members[];
    hkEnum<enum hkcdShapeType::ShapeTypeEnum,unsigned char> m_type; // 0x08
    hkEnum<enum hkcdShapeDispatchType::ShapeDispatchTypeEnum,unsigned char> m_dispatchType; // 0x09
    unsigned char m_bitsPerKey; // 0x0A
    hkEnum<enum hkcdShapeInfoCodecType::ShapeInfoCodecTypeEnum,unsigned char> m_shapeInfoCodecType; // 0x0B
};
