// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/util/welding/hkpweldingutility.cpp
//
// Havok 2014.2.5 (havok_impl.md HK3): the welding angle table, filled by initWeldingTable when a world is created
// (HK7); zero before, as in rfg.exe.

#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/util/welding/hkpweldingutility.h"

#include <xmmintrin.h>

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/hkmath.h"

// 0x03A29A30 (bss)
alignas(16) hkpWeldingUtility::SinCosTableEntry hkpWeldingUtility::m_sinCosTable[32];

// Single-lane SSE arithmetic, in the binary's order under /fp:fast.
static inline float hkWelding_add(float a, float b) { return _mm_cvtss_f32(_mm_add_ss(_mm_set_ss(a), _mm_set_ss(b))); }
static inline float hkWelding_sub(float a, float b) { return _mm_cvtss_f32(_mm_sub_ss(_mm_set_ss(a), _mm_set_ss(b))); }
static inline float hkWelding_mul(float a, float b) { return _mm_cvtss_f32(_mm_mul_ss(_mm_set_ss(a), _mm_set_ss(b))); }

// 0x0104CB50: the accept and snap limits of the NUM_ANGLES (31) edge angles from -pi to pi in steps of 2pi/30
// (0.20943952); the last of the 32 entries stays as it was. A convex edge (angle >= 0) accepts down to the edge's own
// angle and snaps up to it plus the edge snap angle (and at least to pi minus the triangle snap angle); a concave edge
// accepts everything on the triangle side.
void hkpWeldingUtility::initWeldingTable(float edgeNormalSnapDeltaAngle, float triangleNormalSnapDeltaAngle)
{
    const float pi = 3.1415927f; // 0x40490FDB
    for (int i = 0; i < NUM_ANGLES; i++)
    {
        SinCosTableEntry& e = m_sinCosTable[i];
        const float angle = hkWelding_sub(hkWelding_mul((float)i, 0.20943952f), pi);
        if (0.0f <= angle)
        {
            e.m_cosAccept0 = hkMath::cos(angle);
            e.m_sinAccept0 = hkMath::sin(angle);
            float snap0 = hkWelding_add(edgeNormalSnapDeltaAngle, angle);
            if (pi <= snap0)
            {
                snap0 = pi;
            }
            e.m_cosSnap0 = hkMath::cos(snap0);
            float snap1 = hkWelding_add(angle, edgeNormalSnapDeltaAngle);
            const float triangleLimit = hkWelding_sub(pi, triangleNormalSnapDeltaAngle);
            if (triangleLimit <= snap1)
            {
                if (pi <= snap1)
                {
                    snap1 = pi;
                }
                e.m_cosSnap1 = hkMath::cos(snap1);
            }
            else
            {
                e.m_cosSnap1 = hkMath::cos(triangleLimit);
            }
            e.m_cosAccept1 = -1.0f;
            e.m_sinAccept1 = 0.0f;
        }
        else
        {
            const float concave = hkWelding_sub(pi, fabsf(angle));
            e.m_cosAccept0 = 1.0f;
            e.m_sinAccept0 = 0.0f;
            float snap = hkWelding_sub(concave, edgeNormalSnapDeltaAngle);
            if (snap <= triangleNormalSnapDeltaAngle)
            {
                e.m_cosSnap0 = hkMath::cos(snap <= 0.0f ? 0.0f : snap);
            }
            else
            {
                e.m_cosSnap0 = hkMath::cos(triangleNormalSnapDeltaAngle);
            }
            if (snap <= 0.0f)
            {
                snap = 0.0f;
            }
            e.m_cosSnap1 = hkMath::cos(snap);
            e.m_cosAccept1 = hkMath::cos(concave);
            e.m_sinAccept1 = hkMath::sin(concave);
        }
    }
}
