// Original: volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/matrix/hkmatrix6f.cpp
//
// Havok 2014.2.5 (havok_impl.md HK2): the out-of-line hkMatrix6f functions of the implementation closure, from
// rfg.exe. The 6x6 matrix is 2x2 blocks of hkMatrix3f ([row][column]); full column c is the column c % 3 of the
// blocks [0][c / 3] (rows 0-2) and [1][c / 3] (rows 3-5).
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/matrix/hkmatrix6f.h"

#include <string.h>

// 0x01059C80
// Gauss-Jordan elimination on the columns, without pivoting: the pivot column is scaled by 1 / pivot (a scalar
// division) and subtracted from every other column, in the input and in the result (which starts as identity).
void hkMatrix6f::setInvert(const hkMatrix6f& in)
{
    hkMatrix6f tmp;
    memcpy(&tmp, &in, sizeof(hkMatrix6f));
    setIdentity();

    for (int pb = 0; pb < 2; pb++)
    {
        for (int k = 0; k < 3; k++)
        {
            // pivot (row p, column p), p = 3 * pb + k
            hkSimdFloat32 inv;
            inv.setFromFloat(1.0f / tmp.m_m[pb][pb].getColumn(k)(k));
            tmp.m_m[0][pb].getColumn(k).mul(inv);
            tmp.m_m[1][pb].getColumn(k).mul(inv);
            m_m[0][pb].getColumn(k).mul(inv);
            m_m[1][pb].getColumn(k).mul(inv);

            for (int b = 0; b < 2; b++)
            {
                for (int j = 0; j < 3; j++)
                {
                    if (b == pb && j == k)
                    {
                        continue;
                    }
                    hkSimdFloat32 factor;
                    factor.setFromFloat(-tmp.m_m[pb][b].getColumn(j)(k));
                    tmp.m_m[0][b].getColumn(j).addMul(tmp.m_m[0][pb].getColumn(k), factor);
                    tmp.m_m[1][b].getColumn(j).addMul(tmp.m_m[1][pb].getColumn(k), factor);
                    m_m[0][b].getColumn(j).addMul(m_m[0][pb].getColumn(k), factor);
                    m_m[1][b].getColumn(j).addMul(m_m[1][pb].getColumn(k), factor);
                }
            }
        }
    }
}

// 0x01059980
extern "C" void HK_CALL hkMatrix6fSub(hkMatrix6f& aOut, const hkMatrix6f& b)
{
    aOut.sub(b);
}

// 0x01059A40
extern "C" void HK_CALL hkMatrix6fSetMul(hkMatrix6f& out, const hkMatrix6f& a, const hkMatrix6f& b)
{
    out.setMul(a, b);
}

// 0x01059BB0
extern "C" void HK_CALL hkMatrix6fSetMulV(hkVector8f& out, const hkMatrix6f& a, const hkVector8f& b)
{
    out._setMul6(a, b);
}

// 0x01059880
// The blocks' transposes go through the out-of-line hkMatrix3f::setTranspose, the off-diagonal blocks swapped.
extern "C" void HK_CALL hkMatrix6fSetTranspose(hkMatrix6f& out, const hkMatrix6f& in)
{
    out.m_m[0][0].setTranspose(in.m_m[0][0]);
    out.m_m[0][1].setTranspose(in.m_m[1][0]);
    out.m_m[1][0].setTranspose(in.m_m[0][1]);
    out.m_m[1][1].setTranspose(in.m_m[1][1]);
}

// 0x01059EA0
extern "C" void HK_CALL hkMatrix6fSetInvert(hkMatrix6f& out, const hkMatrix6f& in)
{
    out.setInvert(in);
}
