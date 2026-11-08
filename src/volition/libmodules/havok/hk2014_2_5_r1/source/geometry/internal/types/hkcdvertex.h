// Original: volition/libmodules/havok/hk2014_2_5_r1/source/geometry/internal/types/hkcdvertex.h
//
// Havok 2014.2.5 (havok_impl.md HK3): a vertex with its id in the low 16 bits of w. The PDB declaration with the
// inline code of Havok 2014.1 (hkcdVertex.h). Hand-maintained (nobuild in the hdrgen specs).
#pragma once

#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/memory/memoryclasses/hkmemoryclasses.h"
#include "volition/libmodules/havok/hk2014_2_5_r1/source/common/base/math/vector/hkvector4f.h"

// size 0x10
class alignas(16) hkcdVertex : public hkVector4f
{
public:
    HK_DECLARE_NONVIRTUAL_CLASS_ALLOCATOR(hkcdVertex);
    HK_DECLARE_POD_TYPE();
    const hkcdVertex& operator=(const hkcdVertex& p) { assign(p); return *this; } // 0x00DCB590
    void assign(const hkVector4f& p) { m_quad = p.m_quad; } // 0x00DCB580
    int getId() const { return getInt16W(); } // 0x00DAC180
};
