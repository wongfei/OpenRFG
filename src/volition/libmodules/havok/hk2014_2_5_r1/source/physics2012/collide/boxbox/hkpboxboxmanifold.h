// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/boxbox/hkpboxboxmanifold.h
//
// Havok 2014.2.5: full declaration from rfg.pdb (havok_impl.md HK0); the inline members from the 2014.1 SDK
// (hkpBoxBoxManifold.inl), checked against the binary (HK6). Hand-maintained.
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/hkmath.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/boxbox/hkpboxboxcontactpoint.h"

class hkpCdBody;

// size 0x40
class hkpBoxBoxManifold
{
public:
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkpBoxBoxManifold); // new(unsigned int n, void* p) 0x00DC31A0
    hkpBoxBoxManifold(); // 0x00DD4230
    int addPoint(const hkpCdBody& bodyA, const hkpCdBody& bodyB, hkpFeatureContactPoint& fcp); // 0x00DD41C0
    void removePoint(int i); // 0x00DD4210
    int getNumPoints() const { return m_numPoints; } // 0x00DC31B0
    hkpFeatureContactPoint& operator[](int index) { return m_contactPoints[index]; } // 0x00DC35D0
    // 0x00DD4190
    bool findInManifold(const hkpFeatureContactPoint& fcp) const
    {
        for (int i = m_numPoints - 1; i >= 0; i--)
        {
            if (m_contactPoints[i].m_featureIdA == fcp.m_featureIdA && m_contactPoints[i].m_featureIdB == fcp.m_featureIdB)
            {
                return true;
            }
        }
        return false;
    }
    bool isComplete() const { return m_isComplete; } // 0x00DD4260
    void setComplete(bool complete) { m_isComplete = complete; } // 0x00DD4270
    bool hasNoPointsLeft() const { return m_numPoints >= 8; } // 0x00DD4280
    // 0x00DD7DF0: w < 0 (ucomiss/jc: NaN counts too)
    unsigned int isNormalInitialized() const { return !(m_manifoldNormalA.m_quad.m128_f32[3] >= 0.0f); }
    // 0x00DD7E10: w = -1
    void setNormalInitialized()
    {
        m_manifoldNormalA.m_quad = _mm_shuffle_ps(m_manifoldNormalA.m_quad, _mm_unpackhi_ps(m_manifoldNormalA.m_quad, g_vectorfConstants[HK_QUADREAL_MINUS1]), _MM_SHUFFLE(3, 0, 1, 0));
    }
    // 0x00DD7B60: w = 0
    void clearNormalInitialized()
    {
        m_manifoldNormalA.m_quad = _mm_castsi128_ps(_mm_srli_si128(_mm_slli_si128(_mm_castps_si128(m_manifoldNormalA.m_quad), 4), 4));
    }

    hkpFeatureContactPoint m_contactPoints[8]; // 0x00
    unsigned char m_faceVertexFeatureCount; // 0x20
    unsigned char m_numPoints; // 0x21
    bool m_isComplete; // 0x22
    unsigned int m_manifoldNormalB; // 0x24
    hkVector4f m_manifoldNormalA; // 0x30
};
