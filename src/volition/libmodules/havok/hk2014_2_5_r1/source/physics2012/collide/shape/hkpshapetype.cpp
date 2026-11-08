// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshapetype.cpp
//
// Havok 2014.2.5 (havok_impl.md HK3): the alternate type table of the collision dispatcher (which shape types also
// count as convex, collection, BV tree, sphere-representation or height field shapes) and the type names.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshapetype.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/dispatch/hkpcollisiondispatcher.h"

// 0x00DCA900
void hkpRegisterAlternateShapeTypes(hkpCollisionDispatcher* dis)
{
    // convex shapes
    dis->registerAlternateShapeType(hkcdShapeType::SPHERE, hkcdShapeType::CONVEX);
    dis->registerAlternateShapeType(hkcdShapeType::TRIANGLE, hkcdShapeType::CONVEX);
    dis->registerAlternateShapeType(hkcdShapeType::BOX, hkcdShapeType::CONVEX);
    dis->registerAlternateShapeType(hkcdShapeType::CAPSULE, hkcdShapeType::CONVEX);
    dis->registerAlternateShapeType(hkcdShapeType::CYLINDER, hkcdShapeType::CONVEX);
    dis->registerAlternateShapeType(hkcdShapeType::CONVEX_VERTICES, hkcdShapeType::CONVEX);
    dis->registerAlternateShapeType(hkcdShapeType::CONVEX_TRANSLATE, hkcdShapeType::CONVEX);
    dis->registerAlternateShapeType(hkcdShapeType::CONVEX_TRANSFORM, hkcdShapeType::CONVEX);
    dis->registerAlternateShapeType(hkcdShapeType::CONVEX_PIECE, hkcdShapeType::CONVEX);

    // collections
    dis->registerAlternateShapeType(hkcdShapeType::TRIANGLE_COLLECTION, hkcdShapeType::COLLECTION);
    dis->registerAlternateShapeType(hkcdShapeType::LIST, hkcdShapeType::COLLECTION);
    dis->registerAlternateShapeType(hkcdShapeType::EXTENDED_MESH, hkcdShapeType::COLLECTION);
    dis->registerAlternateShapeType(hkcdShapeType::COMPRESSED_MESH, hkcdShapeType::COLLECTION);
    dis->registerAlternateShapeType(hkcdShapeType::TRI_SAMPLED_HEIGHT_FIELD_COLLECTION, hkcdShapeType::COLLECTION);

    // BV trees
    dis->registerAlternateShapeType(hkcdShapeType::MOPP, hkcdShapeType::BV_TREE);
    dis->registerAlternateShapeType(hkcdShapeType::STATIC_COMPOUND, hkcdShapeType::BV_TREE);
    dis->registerAlternateShapeType(hkcdShapeType::BV_COMPRESSED_MESH, hkcdShapeType::BV_TREE);
    dis->registerAlternateShapeType(hkcdShapeType::TRI_SAMPLED_HEIGHT_FIELD_BV_TREE, hkcdShapeType::BV_TREE);

    // sphere representations
    dis->registerAlternateShapeType(hkcdShapeType::CONVEX, hkcdShapeType::SPHERE_REP);

    // height fields
    dis->registerAlternateShapeType(hkcdShapeType::PLANE, hkcdShapeType::HEIGHT_FIELD);
    dis->registerAlternateShapeType(hkcdShapeType::SAMPLED_HEIGHT_FIELD, hkcdShapeType::HEIGHT_FIELD);
}

#define HK_SHAPE_TYPE_NAME(t) \
    case hkcdShapeType::t:    \
        return "hkcdShapeType::" #t

// 0x00DCA9F0
const char* hkGetShapeTypeName(hkcdShapeType::ShapeTypeEnum type)
{
    switch (type)
    {
        HK_SHAPE_TYPE_NAME(SPHERE);
        HK_SHAPE_TYPE_NAME(CYLINDER);
        HK_SHAPE_TYPE_NAME(TRIANGLE);
        HK_SHAPE_TYPE_NAME(BOX);
        HK_SHAPE_TYPE_NAME(CAPSULE);
        HK_SHAPE_TYPE_NAME(CONVEX_VERTICES);
        HK_SHAPE_TYPE_NAME(TRI_SAMPLED_HEIGHT_FIELD_COLLECTION);
        HK_SHAPE_TYPE_NAME(TRI_SAMPLED_HEIGHT_FIELD_BV_TREE);
        HK_SHAPE_TYPE_NAME(LIST);
        HK_SHAPE_TYPE_NAME(MOPP);
        HK_SHAPE_TYPE_NAME(CONVEX_TRANSLATE);
        HK_SHAPE_TYPE_NAME(CONVEX_TRANSFORM);
        HK_SHAPE_TYPE_NAME(SAMPLED_HEIGHT_FIELD);
        HK_SHAPE_TYPE_NAME(EXTENDED_MESH);
        HK_SHAPE_TYPE_NAME(TRANSFORM);
        HK_SHAPE_TYPE_NAME(COMPRESSED_MESH);
        HK_SHAPE_TYPE_NAME(STATIC_COMPOUND);
        HK_SHAPE_TYPE_NAME(BV_COMPRESSED_MESH);
        HK_SHAPE_TYPE_NAME(COLLECTION);
        HK_SHAPE_TYPE_NAME(USER0);
        HK_SHAPE_TYPE_NAME(USER1);
        HK_SHAPE_TYPE_NAME(USER2);
        HK_SHAPE_TYPE_NAME(BV_TREE);
        HK_SHAPE_TYPE_NAME(CONVEX);
        HK_SHAPE_TYPE_NAME(CONVEX_PIECE);
        HK_SHAPE_TYPE_NAME(MULTI_SPHERE);
        HK_SHAPE_TYPE_NAME(CONVEX_LIST);
        HK_SHAPE_TYPE_NAME(TRIANGLE_COLLECTION);
        HK_SHAPE_TYPE_NAME(HEIGHT_FIELD);
        HK_SHAPE_TYPE_NAME(SPHERE_REP);
        HK_SHAPE_TYPE_NAME(BV);
        HK_SHAPE_TYPE_NAME(PLANE);
        HK_SHAPE_TYPE_NAME(PHANTOM_CALLBACK);
        HK_SHAPE_TYPE_NAME(MULTI_RAY);
        HK_SHAPE_TYPE_NAME(INVALID);
        HK_SHAPE_TYPE_NAME(MAX_PPU_SHAPE_TYPE);
        HK_SHAPE_TYPE_NAME(ALL_SHAPE_TYPES);
    default:
        return nullptr;
    }
}
