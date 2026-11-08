// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/geometry/aabb/hkaabb.inl
//
// Havok 2014.2.5 (havok_impl.md HK3): ported from Havok 2014.1 (Common/Base/Types/Geometry/Aabb/hkAabb.inl), SSE path.

inline hkAabb::hkAabb(const hkVector4& min, const hkVector4& max)
    : m_min(min), m_max(max)
{
}

hkBool32 hkAabb::overlaps(const hkAabb& other) const
{
    hkVector4Comparison mincomp = m_min.lessEqual(other.m_max);
    hkVector4Comparison maxcomp = other.m_min.lessEqual(m_max);
    hkVector4Comparison both; both.setAnd(mincomp, maxcomp);
    return both.allAreSet<hkVector4ComparisonMask::MASK_XYZ>();
}

hkBool32 hkAabb::overlapsUpdateSmallestNonExtraHitAabb(const hkAabb& other, hkAabb& smallestNonHitAabbInOut) const
{
    hkVector4Comparison mincomp = m_min.lessEqual(other.m_max);
    hkVector4Comparison maxcomp = other.m_min.lessEqual(m_max);
    hkVector4 newMax; newMax.setMin(smallestNonHitAabbInOut.m_max, other.m_min);
    hkVector4 newMin; newMin.setMax(smallestNonHitAabbInOut.m_min, other.m_max);
    hkVector4Comparison both; both.setAnd(mincomp, maxcomp);
    smallestNonHitAabbInOut.m_min.setSelect(mincomp, smallestNonHitAabbInOut.m_min, newMin);
    smallestNonHitAabbInOut.m_max.setSelect(maxcomp, smallestNonHitAabbInOut.m_max, newMax);
    return both.allAreSet<hkVector4ComparisonMask::MASK_XYZ>();
}

hkBool32 hkAabb::contains(const hkAabb& other) const
{
    hkVector4Comparison mincomp = m_min.lessEqual(other.m_min);
    hkVector4Comparison maxcomp = other.m_max.lessEqual(m_max);
    hkVector4Comparison both; both.setAnd(mincomp, maxcomp);
    return both.allAreSet<hkVector4ComparisonMask::MASK_XYZ>();
}

hkBool32 hkAabb::notContains(const hkAabb& other) const
{
    hkVector4Comparison mincomp = other.m_min.less(m_min);
    hkVector4Comparison maxcomp = other.m_max.greater(m_max);
    hkVector4Comparison both; both.setOr(mincomp, maxcomp);
    return both.anyIsSet<hkVector4ComparisonMask::MASK_XYZ>();
}

hkBool32 hkAabb::containsPoint(const hkVector4& other) const
{
    hkVector4Comparison mincomp = m_min.lessEqual(other);
    hkVector4Comparison maxcomp = other.lessEqual(m_max);
    hkVector4Comparison both; both.setAnd(mincomp, maxcomp);
    return both.allAreSet<hkVector4ComparisonMask::MASK_XYZ>();
}

bool hkAabb::isEmpty() const
{
    hkVector4Comparison comp = m_min.greater(m_max);
    return comp.anyIsSet<hkVector4ComparisonMask::MASK_XYZ>();
}

void hkAabb::includePoint(const hkVector4& point)
{
    m_min.setMin(m_min, point);
    m_max.setMax(m_max, point);
}

void hkAabb::includeAabb(const hkAabb& aabb)
{
    m_min.setMin(m_min, aabb.m_min);
    m_max.setMax(m_max, aabb.m_max);
}

void hkAabb::setFull()
{
    m_max.setConstant<HK_QUADREAL_MAX>();
    m_min.setNeg<4>(m_max);
}

void hkAabb::setEmpty()
{
    m_min.setConstant<HK_QUADREAL_MAX>();
    m_max.setNeg<4>(m_min);
}

void hkAabb::setEmptyIfInvalid()
{
    const hkVector4Comparison cmpValid = m_min.greaterEqual(m_max);
    const hkVector4Comparison cmp = cmpValid.horizontalOr<3>();
    hkVector4 vMin; vMin.setConstant<HK_QUADREAL_MAX>();
    hkVector4 vMax; vMax.setNeg<4>(vMin);
    m_min.setSelect(cmp, vMin, m_min);
    m_max.setSelect(cmp, vMax, m_max);
}

void hkAabb::getCenter(hkVector4& c) const
{
    hkVector4 g; g.setAdd(m_min, m_max);
    c.setMul(g, hkSimdReal::getConstant<HK_QUADREAL_INV_2>());
}

void hkAabb::getCenter2(hkVector4& c) const
{
    c.setAdd(m_min, m_max);
}

void hkAabb::getHalfExtents(hkVector4& he) const
{
    hkVector4 sub; sub.setSub(m_max, m_min);
    he.setMul(sub, hkSimdReal::getConstant<HK_QUADREAL_INV_2>());
}

void hkAabb::getExtents(hkVector4& e) const
{
    e.setSub(m_max, m_min);
}

void hkAabb::getVertex(int index, hkVector4& vertex) const
{
    hkVector4Comparison comp; comp.set((hkVector4ComparisonMask::Mask)index);
    vertex.setSelect(comp, m_max, m_min);
}

void hkAabb::expandBy(hkSimdRealParameter exp)
{
    m_max.setAdd(m_max, exp);
    m_min.setSub(m_min, exp);
}

void hkAabb::expandInDirection(hkVector4Parameter exp)
{
    hkVector4 newMax; newMax.setAdd(m_max, exp);
    hkVector4 newMin; newMin.setAdd(m_min, exp);
    m_max.setMax(m_max, newMax);
    m_min.setMin(m_min, newMin);
}

void hkAabb::scaleBy(hkSimdRealParameter scale)
{
    hkVector4 e; e.setSub(m_min, m_max);
    e.mul(hkSimdReal_Half - (hkSimdReal_Half * scale));
    m_min.sub(e);
    m_max.add(e);
}

void hkAabb::setExpandBy(const hkAabb& aabb, hkSimdRealParameter exp)
{
    m_max.setAdd(aabb.m_max, exp);
    m_min.setSub(aabb.m_min, exp);
}

hkBool32 hkAabb::equals(const hkAabb& aabb) const
{
    hkVector4Comparison mi = aabb.m_min.equal(m_min);
    hkVector4Comparison ma = aabb.m_max.equal(m_max);
    hkVector4Comparison co; co.setAnd(mi, ma);
    return co.allAreSet<hkVector4ComparisonMask::MASK_XYZ>();
}

void hkAabb::setIntersection(const hkAabb& aabb0, const hkAabb& aabb1)
{
    m_min.setMax(aabb0.m_min, aabb1.m_min);
    m_max.setMin(aabb0.m_max, aabb1.m_max);
}

void hkAabb::setUnion(const hkAabb& aabb0, const hkAabb& aabb1)
{
    m_min.setMin(aabb0.m_min, aabb1.m_min);
    m_max.setMax(aabb0.m_max, aabb1.m_max);
}

void hkAabb::setMinkowskiSum(const hkAabb& aabb0, const hkAabb& aabb1)
{
    m_min.setSub(aabb1.m_min, aabb0.m_max);
    m_max.setSub(aabb1.m_max, aabb0.m_min);
}

void hkAabb::projectPoint(const hkVector4& point, hkVector4& projection) const
{
    projection.setMax(point, m_min);
    projection.setMin(projection, m_max);
}

inline void hkAabbUint32::setInvalid()
{
    const int ma = 0x7fffffff;
    hkUint32* minP = m_min;
    hkUint32* maxP = m_max;
    minP[0] = ma;
    minP[1] = ma;
    minP[2] = ma;
    minP[3] = 0; // writes m_expansionMin/m_expansionShift, as in Havok
    maxP[0] = 0;
    maxP[1] = 0;
    maxP[2] = 0;
    maxP[3] = 0;
}

inline void hkAabbUint32::setInvalidY()
{
    const int ma = 0x7fff0000;
    hkUint32* minP = m_min;
    hkUint32* maxP = m_max;
    minP[1] = ma;
    minP[2] = ma;
    minP[3] = 0;
    maxP[1] = 0;
    maxP[2] = 0;
}

// 0x00DB9580 (hkaabb.inl:247): eight 32-bit words
inline void hkAabbUint32::operator=(const hkAabbUint32& other)
{
    const unsigned int* src = (const unsigned int*)&other;
    unsigned int* dst = (unsigned int*)this;
    for (int i = 0; i < 8; i++)
    {
        dst[i] = src[i];
    }
}
