// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/deprecated/multisphere/hkpmultisphereshape.h
//
// Havok 2014.2.5: declaration from rfg.pdb (pdbdump header, stub_impl.md S4), for the multi-sphere agents. The shape
// itself is not ported (no RFG data has one); only its inline accessors are defined. Hand-maintained.
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4f.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/heightfield/hkpsphererepshape.h"

class hkAabb;
class hkClass;
class hkSphere;
class hkTransformf;
class hkpCdBody;
class hkpRayHitCollector;
struct hkInternalClassMember;
struct hkpShapeRayCastInput;
struct hkpShapeRayCastOutput;

// size 0xA0
// Up to eight spheres (center xyz, radius w) in the shape's space.
class hkpMultiSphereShape : public hkpSphereRepShape
{
public:
    enum
    {
        THIS_SHAPE_TYPE = 25,
    };

    enum
    {
        MAX_SPHERES = 8,
    };

    HK_DECLARE_CLASS_ALLOCATOR(); // new(unsigned int, void* p) 0x00D7F050, delete(void* p, unsigned int nbytes) 0x00DAAF70
    static const hkClass& staticClass(); // 0x00D7F060
    hkpMultiSphereShape(hkFinishLoadedObjectFlag flag); // 0x00DAA8E0
    hkpMultiSphereShape(const hkVector4f* spheres, int numSpheres); // 0x00DAA890

    // 0x00DBA940
    const hkVector4f* getSpheres() const
    {
        return m_spheres;
    }

    // 0x00DAA820
    int getNumSpheres() const
    {
        return m_numSpheres;
    }

    virtual ~hkpMultiSphereShape(); // vtbl+0x00 0x00DAAF60
    virtual void getAabb(const hkTransformf& localToWorld, float tolerance, hkAabb& out) const; // vtbl+0x14 0x00DAAD90
    virtual hkBool castRay(const hkpShapeRayCastInput& input, hkpShapeRayCastOutput& results) const; // vtbl+0x18 0x00DAAC20
    virtual void castRayWithCollector(const hkpShapeRayCastInput& inputLocal, const hkpCdBody& cdBody, hkpRayHitCollector& collector) const; // vtbl+0x1C 0x00DAAC70
    virtual int getNumCollisionSpheres() const; // vtbl+0x30 0x00DAA830
    virtual const hkSphere* getCollisionSpheres(hkSphere* sphereBuffer) const; // vtbl+0x34 0x00DAA900

    static const hkInternalClassMember Members[]; // 0x0135683C

protected:
    int m_numSpheres; // 0x10
    hkVector4f m_spheres[8]; // 0x20
};
