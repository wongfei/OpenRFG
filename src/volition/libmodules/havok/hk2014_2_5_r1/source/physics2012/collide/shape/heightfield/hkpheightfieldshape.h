// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/heightfield/hkpheightfieldshape.h
//
// Seam only (Havok 2014.2.5).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/geometry/collide/shapes/hkcdshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/hkpshape.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/query/hkpshaperaycastinput.h"

class hkClass;
class hkFinishLoadedObjectFlag;
class hkSphere;
class hkVector4f;
class hkpCdBody;
class hkpRayHitCollector;

// size 0x10
class hkpHeightFieldShape : public hkpShape
{
public:
    enum
    {
        THIS_SHAPE_TYPE = 28,
    };

    // size 0xC
    struct CollideSpheresInput
    {
        hkSphere* m_spheres; // 0x00
        int m_numSpheres; // 0x04
        float m_tolerance; // 0x08
    };

    // size 0x40
    struct hkpSphereCastInput : public hkpShapeRayCastInput
    {
        hkpSphereCastInput(); // 0x00D8FA60

        float m_radius; // 0x30
        float m_maxExtraPenetration; // 0x34
    };

    static const hkClass& staticClass(); // 0x00D7C670
    virtual ~hkpHeightFieldShape(); // vtbl+0x00 0x00D9BA90
    virtual void collideSpheres(const CollideSpheresInput&, hkVector4f*) const = 0; // vtbl+0x48
    hkpHeightFieldShape(hkFinishLoadedObjectFlag flag); // 0x00D9BEA0
    hkpHeightFieldShape(hkcdShapeType::ShapeTypeEnum type); // 0x00D9BE30
    virtual void castSphere(const hkpSphereCastInput&, const hkpCdBody&, hkpRayHitCollector&) const = 0; // vtbl+0x4C

};
