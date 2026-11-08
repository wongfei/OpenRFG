// Original: volition/libmodules/havok/hk2014_2_5_r1/source/physics2012/collide/shape/heightfield/sampledheightfield/hkpsampledheightfieldbasecinfo.h
//
// Havok 2014.2.5 (havok_impl.md HK3): the construction info of sampled height fields. Layout from rfg.pdb, the inline
// constructor from Havok 2014.1 (hkpSampledHeightFieldBaseCinfo.h; rfg.exe has no out-of-line copy). Hand-written.
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4f.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/types/hkbasetypes.h"

// size 0x30
class hkpSampledHeightFieldBaseCinfo
{
public:
    hkpSampledHeightFieldBaseCinfo()
    {
        m_scale.set(1.0f, 1.0f, 1.0f, 0.0f);
        m_xRes = 2;
        m_zRes = 2;
        m_minHeight = 0.0f;
        m_maxHeight = -1.0f; // max < min: the height range is scanned on the first getAabb
        m_useProjectionBasedHeight = false;
    }

    hkVector4f m_scale; // 0x00
    int m_xRes; // 0x10
    int m_zRes; // 0x14
    float m_minHeight; // 0x18
    float m_maxHeight; // 0x1C
    hkBool m_useProjectionBasedHeight; // 0x20
};
