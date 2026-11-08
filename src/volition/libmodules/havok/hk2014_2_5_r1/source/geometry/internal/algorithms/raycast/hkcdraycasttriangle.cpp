// Original: volition/libmodules/havok/hk2014_2_5_r1/source/geometry/internal/algorithms/raycast/hkcdraycasttriangle.cpp
//
// Havok 2014.2.5 (havok_impl.md HK3): ray versus triangle, from rfg.exe (no 2014.1 source).

#include "volition/libmodules/havok/hk2014_2_5_r1/source/geometry/internal/algorithms/raycast/hkcdraycasttriangle.h"

#include "volition/libmodules/havok/hk2014_2_5_r1/source/geometry/internal/types/hkcdraybundle.h"

namespace
{
// 0x00EF4DA0 (LTCG register arguments): per ray, (n x (I - a)) . (I - p) for the transposed points I; non-negative
// when I is on the inner side of the edge from a when p is the next vertex.
void hkcdHelperEdgeDots(const hkVector4f& a, const hkVector4f& n, const hkFourTransposedPointsf& vI, const hkVector4f& p, hkVector4f& dotsOut)
{
    const hkVector4f* I = vI.m_vertices;
    hkVector4 ax; ax.setAll(a.getComponent<0>());
    hkVector4 ay; ay.setAll(a.getComponent<1>());
    hkVector4 az; az.setAll(a.getComponent<2>());
    hkVector4 dX; dX.setSub(I[0], ax);
    hkVector4 dY; dY.setSub(I[1], ay);
    hkVector4 dZ; dZ.setSub(I[2], az);

    hkVector4 nx; nx.setAll(n.getComponent<0>());
    hkVector4 ny; ny.setAll(n.getComponent<1>());
    hkVector4 nz; nz.setAll(n.getComponent<2>());
    hkVector4 t;

    // c = n x d
    hkVector4 cy; cy.setMul(nz, dX); t.setMul(nx, dZ); cy.sub(t);
    hkVector4 cz; cz.setMul(nx, dY); t.setMul(ny, dX); cz.sub(t);
    hkVector4 cx; cx.setMul(ny, dZ); t.setMul(nz, dY); cx.sub(t);

    hkVector4 px; px.setAll(p.getComponent<0>());
    hkVector4 py; py.setAll(p.getComponent<1>());
    hkVector4 pz; pz.setAll(p.getComponent<2>());
    hkVector4 eX; eX.setSub(I[0], px);
    hkVector4 eY; eY.setSub(I[1], py);
    hkVector4 eZ; eZ.setSub(I[2], pz);

    dotsOut.setMul(cy, eY);
    t.setMul(eX, cx);
    dotsOut.add(t);
    t.setMul(eZ, cz);
    dotsOut.add(t);
}
}

// 0x00EF5510
// Four segments against the triangle (vA, vB, vC). An active segment hits when its end points are on different sides
// of the plane (or the end is on it), the plane crossing is closer than its fraction, and the crossing is inside each
// edge by at least -tolerance * |n|^2. The hits get the fraction and the unit normal facing the segment's start.
hkVector4fComparison hkcdRayCastTriangle::hkcdSegmentBundleTriangleIntersect(const hkcdRayBundle& rayBundle, const hkVector4f& vA, const hkVector4f& vB, const hkVector4f& vC, const hkSimdFloat32& tolerance, hkFourTransposedPointsf& normalsOut, hkVector4f& fractionsInOut)
{
    const hkVector4f* start = rayBundle.m_start.m_vertices;
    const hkVector4f* end = rayBundle.m_end.m_vertices;
    hkVector4 dirX; dirX.setSub(end[0], start[0]);
    hkVector4 dirY; dirY.setSub(end[1], start[1]);
    hkVector4 dirZ; dirZ.setSub(end[2], start[2]);

    hkVector4 ab; ab.setSub(vB, vA);
    hkVector4 ac; ac.setSub(vC, vA);
    hkVector4 n; n.setCross(ab, ac);

    hkVector4 ax; ax.setAll(vA.getComponent<0>());
    hkVector4 relX; relX.setSub(start[0], ax);
    hkVector4 ay; ay.setAll(vA.getComponent<1>());
    hkVector4 relY; relY.setSub(start[1], ay);
    hkVector4 az; az.setAll(vA.getComponent<2>());
    hkVector4 relZ; relZ.setSub(start[2], az);

    hkVector4 nx; nx.setAll(n.getComponent<0>());
    hkVector4 ny; ny.setAll(n.getComponent<1>());
    hkVector4 nz; nz.setAll(n.getComponent<2>());
    hkVector4 t;

    // the distances of the start and of the direction along the normal
    hkVector4 dirDist; dirDist.setMul(ny, dirY);
    t.setMul(nx, dirX); dirDist.add(t);
    t.setMul(nz, dirZ); dirDist.add(t);
    hkVector4 startDist; startDist.setMul(ny, relY);
    t.setMul(nx, relX); startDist.add(t);
    t.setMul(nz, relZ); startDist.add(t);

    hkVector4 hitFraction; hitFraction.setReciprocal<HK_ACC_23_BIT, HK_DIV_IGNORE>(dirDist);
    hitFraction.mul(startDist);
    hitFraction.setNeg<4>(hitFraction);

    hkVector4 endDist; endDist.setAdd(dirDist, startDist);
    const hkVector4 zero = hkVector4f::getConstant<HK_QUADREAL_0>();
    hkVector4Comparison sameSide; sameSide.setOr(zero.equal(endDist), zero.notEqual(startDist));
    t.setMul(startDist, endDist);
    sameSide.setAnd(sameSide, zero.lessEqual(t));
    hkVector4Comparison mask; mask.setAndNot(hitFraction.less(fractionsInOut), sameSide);
    mask.setAnd(mask, rayBundle.m_activeRays);
    if (!mask.anyIsSet())
    {
        return mask;
    }

    hkVector4 n2; n2.setMul(n, n);
    const hkSimdReal lenSq = n2.horizontalAdd<3>();
    hkVector4 tol; tol.setAll(hkSimdReal_0 - lenSq);
    hkVector4 toleranceV; toleranceV.setAll(tolerance);
    tol.mul(toleranceV);

    // the plane crossings
    hkFourTransposedPointsf hits;
    hits.m_vertices[0].setMul(hitFraction, dirX); hits.m_vertices[0].add(start[0]);
    hits.m_vertices[1].setMul(dirY, hitFraction); hits.m_vertices[1].add(start[1]);
    hits.m_vertices[2].setMul(dirZ, hitFraction); hits.m_vertices[2].add(start[2]);

    hkVector4 dots;
    hkcdHelperEdgeDots(vA, n, hits, vB, dots);
    mask.setAnd(tol.lessEqual(dots), mask);
    hkcdHelperEdgeDots(vB, n, hits, vC, dots);
    mask.setAnd(tol.lessEqual(dots), mask);
    hkcdHelperEdgeDots(vC, n, hits, vA, dots);
    mask.setAnd(tol.lessEqual(dots), mask);

    fractionsInOut.setSelect(mask, hitFraction, fractionsInOut);

    // the unit normal, flipped to the start's side
    hkVector4 lenSqV; lenSqV.setAll(lenSq);
    hkVector4 invLen; invLen.setSqrtInverse<HK_ACC_23_BIT, HK_SQRT_SET_ZERO>(lenSqV);
    hkVector4 unitN; unitN.setMul(invLen, n);
    hkVector4 ux; ux.setAll(unitN.getComponent<0>()); ux.setFlipSign(ux, startDist);
    hkVector4 uy; uy.setAll(unitN.getComponent<1>()); uy.setFlipSign(uy, startDist);
    hkVector4 uz; uz.setAll(unitN.getComponent<2>()); uz.setFlipSign(uz, startDist);
    normalsOut.m_vertices[0].setSelect(mask, ux, normalsOut.m_vertices[0]);
    normalsOut.m_vertices[1].setSelect(mask, uy, normalsOut.m_vertices[1]);
    normalsOut.m_vertices[2].setSelect(mask, uz, normalsOut.m_vertices[2]);
    return mask;
}
