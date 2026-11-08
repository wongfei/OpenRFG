// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/geometry/aabb/hkaabb.h
//
// Havok 2014.2.5 (havok_impl.md HK3): ported from Havok 2014.1 (Common/Base/Types/Geometry/Aabb/hkAabb.h), checked
// against rfg.exe. Hand-maintained (nobuild in the hdrgen specs).
#ifndef HK_MATH_AABB_H
#define HK_MATH_AABB_H

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/hkmath.h"

class hkClass;
struct hkInternalClassMember;

// size 0x20
class alignas(16) hkAabb
{
public:
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkAabb); // new(unsigned int nbytes) 0x00E17DD0, delete(void* p, unsigned int nbytes) 0x00E17DF0
    static const hkClass& staticClass(); // 0x00CEB7E0

    hkAabb() {}
    HK_FORCE_INLINE hkAabb(const hkVector4& min, const hkVector4& max);

    HK_FORCE_INLINE void setFromCenterRadius(const hkVector4& center, const hkVector4& halfExtent) { m_min.setSub(center, halfExtent); m_max.setAdd(center, halfExtent); } // 0x00F36280
    HK_FORCE_INLINE void setFromCenterRadius(const hkVector4& center, const hkSimdReal& radius) { hkVector4 r; r.setAll(radius); m_min.setSub(center, r); m_max.setAdd(center, r); } // 0x00D518D0
    HK_FORCE_INLINE void setFromPoint(const hkVector4& x) { m_min = x; m_max = x; } // 0x00E72AA0
    HK_FORCE_INLINE void setFromLine(const hkVector4& x, const hkVector4& y) { m_min.setMin(x, y); m_max.setMax(x, y); } // 0x00EBB040
    HK_FORCE_INLINE void setFromTriangle(const hkVector4& x, const hkVector4& y, const hkVector4& z) { setFromLine(x, y); includePoint(z); } // 0x00EBD2D0
    HK_FORCE_INLINE void setFromTetrahedron(const hkVector4& x, const hkVector4& y, const hkVector4& z, const hkVector4& w) { setFromTriangle(x, y, z); includePoint(w); }
    HK_FORCE_INLINE hkBool32 overlaps(const hkAabb& other) const; // 0x00E3BE90
    HK_FORCE_INLINE hkBool32 overlapsUpdateSmallestNonExtraHitAabb(const hkAabb& other, hkAabb& smallestNonHitAabbInOut) const;
    bool isValid() const; // 0x00FBF6C0
    HK_FORCE_INLINE hkBool32 contains(const hkAabb& other) const; // 0x00D787F0
    HK_FORCE_INLINE hkBool32 notContains(const hkAabb& other) const; // 0x00E3BEC0
    HK_FORCE_INLINE hkBool32 containsPoint(const hkVector4& other) const; // 0x00FE0A20
    HK_FORCE_INLINE void includePoint(const hkVector4& point); // 0x00B62DE0 (hkaabb.inl:87)
    HK_FORCE_INLINE void includeAabb(const hkAabb& aabb); // 0x00D8B8C0
    HK_FORCE_INLINE void setFull();
    HK_FORCE_INLINE void setEmpty(); // 0x00D453B0
    HK_FORCE_INLINE bool isEmpty() const; // 0x00DA6CD0
    HK_FORCE_INLINE void getCenter(hkVector4& c) const; // 0x00D74780
    HK_FORCE_INLINE void getCenter2(hkVector4& c) const; // 0x00E3A9F0
    HK_FORCE_INLINE void getHalfExtents(hkVector4& he) const; // 0x00D747A0
    HK_FORCE_INLINE void getExtents(hkVector4& e) const; // 0x00D35A30
    HK_FORCE_INLINE void getVertex(int index, hkVector4& vertex) const; // 0x00F362A0
    HK_FORCE_INLINE void expandBy(hkSimdRealParameter exp); // 0x00D267F0
    HK_FORCE_INLINE void expandInDirection(hkVector4Parameter exp);
    HK_FORCE_INLINE void scaleBy(hkSimdRealParameter scale);
    HK_FORCE_INLINE void setExpandBy(const hkAabb& aabb, hkSimdRealParameter exp);
    HK_FORCE_INLINE hkBool32 equals(const hkAabb& aabb) const;
    HK_FORCE_INLINE void setIntersection(const hkAabb& aabb0, const hkAabb& aabb1); // 0x00EBB060
    HK_FORCE_INLINE void setEmptyIfInvalid();
    HK_FORCE_INLINE void setUnion(const hkAabb& aabb0, const hkAabb& aabb1); // 0x00E3AA10
    HK_FORCE_INLINE void setMinkowskiSum(const hkAabb& aabb0, const hkAabb& aabb1);
    HK_FORCE_INLINE void projectPoint(const hkVector4& point, hkVector4& projection) const;

    static const hkInternalClassMember Members[];
    hkVector4f m_min; // 0x00
    hkVector4f m_max; // 0x10
};

// size 0x20 (HK_CLASSALIGN16 in the SDK: rfg.exe loads arrays of them with MOVDQA)
struct alignas(16) hkAabbUint32
{
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkAabbUint32);
    static const hkClass& staticClass(); // 0x00CEB800
    inline void operator=(const hkAabbUint32& other); // 0x00DB9580
    inline void setInvalid();
    inline void setInvalidY();

    static const hkInternalClassMember Members[];
    unsigned int m_min[3]; // 0x00
    unsigned char m_expansionMin[3]; // 0x0C
    unsigned char m_expansionShift; // 0x0F
    unsigned int m_max[3]; // 0x10
    unsigned char m_expansionMax[3]; // 0x1C
    unsigned char m_shapeKeyByte; // 0x1F (the lowest byte of the hkpShapeKey)
};

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/geometry/aabb/hkaabb.inl"

#endif // HK_MATH_AABB_H
