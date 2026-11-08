// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/matrix/hkmatrix3f.cpp
//
// Havok 2014.2.5 (havok_impl.md HK2): the out-of-line hkMatrix3f functions of the implementation closure, from rfg.exe
// (hkdiff compares them bit for bit). The inline ones are in hkmatrix3f.inl.
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/matrix/hkmatrix3f.h"

// 0x00CF9440
void hkMatrix3f::add(const hkMatrix3f& a)
{
    _add(a);
}

// 0x00CF9470
void hkMatrix3f::sub(const hkMatrix3f& a)
{
    _sub(a);
}

// 0x00CF9380
void hkMatrix3f::mul(hkSimdFloat32Parameter scale)
{
    m_col0.mul(scale);
    m_col1.mul(scale);
    m_col2.mul(scale);
}

// 0x00CF9AE0
void hkMatrix3f::addMul(hkSimdFloat32Parameter scale, const hkMatrix3f& a)
{
    _addMul(scale, a);
}

// 0x00CF9240
void hkMatrix3f::transpose()
{
    HK_TRANSPOSE3f(m_col0, m_col1, m_col2);
}

// 0x00CF9700
void hkMatrix3f::setTranspose(const hkMatrix3f& s)
{
    _setTranspose(s);
}

// 0x00CF93B0
// this * v = r x v: the columns are (0, r.z, -r.y), (-r.z, 0, r.x), (r.y, -r.x, 0), w zero.
void hkMatrix3f::setCrossSkewSymmetric(hkVector4fParameter r)
{
    const hkQuadFloat32 zero = _mm_setzero_ps();
    const hkQuadFloat32 x = _mm_shuffle_ps(r.m_quad, r.m_quad, _MM_SHUFFLE(0, 0, 0, 0));
    const hkQuadFloat32 y = _mm_shuffle_ps(r.m_quad, r.m_quad, _MM_SHUFFLE(1, 1, 1, 1));
    const hkQuadFloat32 z = _mm_shuffle_ps(r.m_quad, r.m_quad, _MM_SHUFFLE(2, 2, 2, 2));
    m_col0.m_quad = _mm_movelh_ps(_mm_unpacklo_ps(zero, z), _mm_unpacklo_ps(_mm_sub_ps(zero, y), zero));
    m_col1.m_quad = _mm_movelh_ps(_mm_unpacklo_ps(_mm_sub_ps(zero, z), zero), _mm_unpacklo_ps(x, zero));
    m_col2.m_quad = _mm_movelh_ps(_mm_unpacklo_ps(y, _mm_sub_ps(zero, x)), _mm_unpacklo_ps(zero, zero));
}

// 0x00CFA3A0
// this = a * b. a is copied first (this may be a); the columns are written from the last one.
void hkMatrix3f::setMul(const hkMatrix3f& a, const hkMatrix3f& b)
{
    const hkMatrix3f ca = a;
    for (int i = 2; i >= 0; i--)
    {
        getColumn(i)._setRotatedDir(ca, b.getColumn(i));
    }
}

// 0x00CFA150
extern "C" void hkMatrix3f_setMulMat3Mat3(hkMatrix3f* thisMatrix, const hkMatrix3f& aTb, const hkMatrix3f& bTc)
{
    thisMatrix->setMul(aTb, bTc);
}

// 0x00CFA5A0
void hkMatrix3f::mul(const hkMatrix3f& a)
{
    hkMatrix3f temp;
    temp.setMul(*this, a);
    *this = temp;
}

// 0x00CFA450
// this = a * transpose(b)
void hkMatrix3f::setMulInverse(const hkMatrix3f& a, const hkRotationf& b)
{
    hkMatrix3f bT;
    bT._setTranspose(b);
    hkMatrix3f r = a;
    const hkMatrix3f ca = a;
    for (int i = 2; i >= 0; i--)
    {
        r.getColumn(i)._setRotatedDir(ca, bT.getColumn(i));
    }
    *this = r;
}

// 0x00CF98E0
// this = transpose(a) * b
void hkMatrix3f::setMulInverseMul(const hkRotationf& a, const hkMatrix3f& b)
{
    hkMatrix3f aT;
    aT._setTranspose(a);
    for (int i = 0; i < 3; i++)
    {
        const hkVector4f bi = b.getColumn(i);
        getColumn(i)._setRotatedDir(aT, bi);
    }
}

// 0x00CFAD50
// this = r * this * transpose(r)
void hkMatrix3f::changeBasis(const hkRotationf& r)
{
    hkMatrix3f temp;
    temp.setMulInverse(*this, r);
    setMul(r, temp);
}

// 0x00CFA200
// Inverse of a symmetric matrix by its cofactors; a matrix whose determinant is small against its row sums (scaled
// by HK_QUADREAL_EPS) becomes zero. 12-bit reciprocal with one Newton step (hkMath::quadReciprocal).
extern "C" void hkMatrix3f_invertSymmetric(hkMatrix3f& thisMatrix)
{
    const hkVector4f c0 = thisMatrix.m_col0;
    const hkVector4f c1 = thisMatrix.m_col1;
    const hkVector4f c2 = thisMatrix.m_col2;

    hkVector4f r0, r1, r2;
    r0.setCross(c1, c2);
    r1.setCross(c2, c0);
    r2.setCross(c0, c1);

    const hkSimdFloat32 det = c0.dot<3>(r0);
    hkVector4f a0, a1, a2;
    a0.setAbs(c0);
    a1.setAbs(c1);
    a2.setAbs(c2);
    const hkSimdFloat32 bound = (a2.horizontalAdd<3>() * a1.horizontalAdd<3>()) * (a0.horizontalAdd<3>() * hkSimdFloat32_Eps);
    hkSimdFloat32 absDet;
    absDet.setAbs(det);
    const __m128 ok = _mm_cmplt_ps(bound.m_real, absDet.m_real);

    const __m128 dinv = hkMath::quadReciprocal(det.m_real);
    thisMatrix.m_col1.m_quad = _mm_and_ps(ok, _mm_mul_ps(dinv, r1.m_quad));
    thisMatrix.m_col0.m_quad = _mm_and_ps(_mm_mul_ps(dinv, r0.m_quad), ok);
    thisMatrix.m_col2.m_quad = _mm_and_ps(_mm_mul_ps(dinv, r2.m_quad), ok);
}

// 0x00CF9410
void hkMatrix3f::setMul(hkSimdFloat32Parameter scale, const hkMatrix3f& a)
{
    _setMul(scale, a);
}

// 0x00CF9B20
// x, y and z of every column; w is not compared.
bool hkMatrix3f::isApproximatelyEqual(const hkMatrix3f& m, hkFloat32 epsilon) const
{
    const hkSimdFloat32 eps = hkSimdFloat32::fromFloat(epsilon);
    return m_col0.allEqual<3>(m.m_col0, eps) && m_col1.allEqual<3>(m.m_col1, eps) && m_col2.allEqual<3>(m.m_col2, eps);
}

// 0x00CF9CB0
// Inverse by the cofactors. src is read before this is written, so they may be the same matrix. A determinant not
// above HK_QUADREAL_EPS times the product of the absolute column sums (or a NaN one) makes this zero and fails.
hkResult hkMatrix3f::setInverse(const hkMatrix3f& src)
{
    const hkVector4f c0 = src.m_col0;
    const hkVector4f c1 = src.m_col1;
    const hkVector4f c2 = src.m_col2;

    hkVector4f r0, r1, r2;
    r0.setCross(c1, c2);
    r1.setCross(c2, c0);
    r2.setCross(c0, c1);

    const hkSimdFloat32 det = c0.dot<3>(r0);
    hkVector4f a0, a1, a2;
    a0.setAbs(c0);
    a1.setAbs(c1);
    a2.setAbs(c2);
    const hkSimdFloat32 bound = (a2.horizontalAdd<3>() * a1.horizontalAdd<3>()) * (a0.horizontalAdd<3>() * hkSimdFloat32_Eps);
    hkSimdFloat32 absDet;
    absDet.setAbs(det);

    if (!(absDet.getReal() > bound.getReal()))
    {
        setZero();
        return HK_FAILURE;
    }

    const __m128 dinv = hkMath::quadReciprocal(det.m_real);
    HK_TRANSPOSE3f(r0, r1, r2);
    m_col0.m_quad = _mm_mul_ps(dinv, r0.m_quad);
    m_col1.m_quad = _mm_mul_ps(dinv, r1.m_quad);
    m_col2.m_quad = _mm_mul_ps(dinv, r2.m_quad);
    return HK_SUCCESS;
}

// 0x00CFA5E0
// Jacobi rotations until the off-diagonal part is below epsilon^2 * frobeniusNormSqr or maxIter rotations are done.
// The eigenvalues are the remaining diagonal (w repeats z). HK_SUCCESS if it converged.
hkResult hkMatrix3f::diagonalizeSymmetric(hkRotationf& eigenVec, hkVector4f& eigenVal, int maxIter, hkFloat32 epsilon) const
{
    hkMatrix3f m = *this;
    eigenVec.setIdentity();

    const hkSimdFloat32 eps = hkSimdFloat32::fromFloat(epsilon);
    const hkSimdFloat32 frob = (m_col1.lengthSquared<3>() + m_col0.lengthSquared<3>()) + m_col2.lengthSquared<3>();
    const hkSimdFloat32 threshold = frob * (eps * eps);

    // twice the squares of the upper triangle
    hkFloat32 offDiag = (m.m_col2(0) * m.m_col2(0) + m.m_col1(0) * m.m_col1(0) + m.m_col2(1) * m.m_col2(1)) * 2.0f;
    for (int iter = 0; offDiag > threshold.getReal() && iter < maxIter; iter++)
    {
        // the largest off-diagonal element (p, q)
        const hkFloat32 a01 = hkMath::fabs(m.m_col1(0));
        const hkFloat32 a02 = hkMath::fabs(m.m_col2(0));
        const hkFloat32 a12 = hkMath::fabs(m.m_col2(1));
        int p = 0;
        int q = 1;
        hkFloat32 largest = a01;
        if (a02 > a01)
        {
            q = 2;
            largest = a02;
        }
        if (a12 > largest)
        {
            p = 1;
            q = 2;
        }

        const hkFloat32 apq = m.getColumn(q)(p);
        if (apq != 0.0f)
        {
            // the rotation that zeroes (p, q); the +1 is written as -(-1) in the binary
            const hkFloat32 theta = (m.getColumn(q)(q) - m.getColumn(p)(p)) / (apq * 2.0f);
            const hkFloat32 root = hkMath::sqrt(theta * theta - -1.0f);
            const hkFloat32 t = (theta < 0.0f) ? 1.0f / (theta - root) : 1.0f / (root + theta);
            const hkFloat32 c = 1.0f / hkMath::sqrt(t * t - -1.0f);
            const hkFloat32 s = c * t;

            hkMatrix3f jacobi;
            jacobi.setIdentity();
            hkMatrix3f jacobiT;
            jacobiT.setIdentity();
            jacobi.getColumn(p)(p) = c;
            jacobiT.getColumn(p)(p) = c;
            jacobi.getColumn(q)(p) = s;
            jacobi.getColumn(p)(q) = -s;
            jacobiT.getColumn(q)(p) = -s;
            jacobiT.getColumn(p)(q) = s;
            jacobi.getColumn(q)(q) = c;
            jacobiT.getColumn(q)(q) = c;

            m.mul(jacobi);
            m.setMul(jacobiT, m);
            eigenVec.mul(jacobi);
        }

        offDiag = (m.m_col2(0) * m.m_col2(0) + m.m_col1(0) * m.m_col1(0) + m.m_col2(1) * m.m_col2(1)) * 2.0f;
    }

    eigenVal.m_quad = _mm_movelh_ps(_mm_unpacklo_ps(_mm_shuffle_ps(m.m_col0.m_quad, m.m_col0.m_quad, 0x00),
                                                    _mm_shuffle_ps(m.m_col1.m_quad, m.m_col1.m_quad, 0x55)),
                                    _mm_shuffle_ps(m.m_col2.m_quad, m.m_col2.m_quad, 0xAA));
    return (offDiag > threshold.getReal()) ? HK_FAILURE : HK_SUCCESS;
}
